"""Test unitari per bridge.py — solo stdlib (unittest), eseguibili con:

    cd bridge && python3 -m unittest test_bridge

Coprono le funzioni pure: pricing, costo, parsing timestamp e — soprattutto —
l'algoritmo della finestra rate-limit 5h (compute_window5h), che è la feature
di punta ed è interamente euristico.
"""

import datetime as dt
import os
import stat
import tempfile
import unittest
import unittest.mock

import bridge

UTC = dt.timezone.utc


def _ts(year, month, day, hour, minute=0, second=0):
    return dt.datetime(year, month, day, hour, minute, second, tzinfo=UTC)


class TestPriceFor(unittest.TestCase):
    def setUp(self):
        self.pricing = bridge.DEFAULT_PRICING

    def test_price_for_exact_model_returns_that_entry(self):
        # Arrange / Act
        price = bridge.price_for("claude-opus-4-7", self.pricing)
        # Assert
        self.assertEqual(price["input"], 15.00)

    def test_price_for_versioned_suffix_matches_longest_prefix(self):
        # "claude-opus-4-7-20251201" deve matchare "claude-opus-4-7",
        # non il prefisso più corto "claude-opus-4".
        price = bridge.price_for("claude-opus-4-7-20251201", self.pricing)
        self.assertEqual(price["output"], 75.00)

    def test_price_for_unknown_model_falls_back_to_default(self):
        price = bridge.price_for("some-other-model", self.pricing)
        self.assertEqual(price, self.pricing["_default"])


class TestComputeCost(unittest.TestCase):
    def setUp(self):
        self.pricing = {
            "_default": {"input": 3.0, "output": 15.0, "cache_write": 3.75, "cache_read": 0.30}
        }

    def test_compute_cost_sums_all_four_token_buckets(self):
        usage = {
            "input_tokens": 1_000_000,
            "output_tokens": 1_000_000,
            "cache_creation_input_tokens": 1_000_000,
            "cache_read_input_tokens": 1_000_000,
        }
        cost = bridge.compute_cost(usage, "x", self.pricing)
        self.assertAlmostEqual(cost, 3.0 + 15.0 + 3.75 + 0.30)

    def test_compute_cost_missing_fields_treated_as_zero(self):
        self.assertEqual(bridge.compute_cost({}, "x", self.pricing), 0.0)


class TestParseTs(unittest.TestCase):
    def test_parse_ts_z_suffix_parses_to_utc(self):
        got = bridge.parse_ts("2026-05-16T14:45:01.824Z")
        self.assertEqual(got, dt.datetime(2026, 5, 16, 14, 45, 1, 824000, tzinfo=UTC))

    def test_parse_ts_empty_returns_none(self):
        self.assertIsNone(bridge.parse_ts(""))

    def test_parse_ts_garbage_returns_none(self):
        self.assertIsNone(bridge.parse_ts("not-a-date"))


class TestComputeWindow5h(unittest.TestCase):
    def test_window_no_timestamps_is_inactive_with_defaults(self):
        now = _ts(2026, 6, 3, 12, 0)
        w = bridge.compute_window5h([], [], now, plan_limit_5h=200.0)
        self.assertFalse(w["active"])
        self.assertEqual(w["remaining_min"], 300)
        self.assertEqual(w["limit_usd"], 200.0)
        self.assertIsNone(w["start"])

    def test_window_active_burst_sums_cost_messages_and_pct(self):
        now = _ts(2026, 6, 3, 12, 0)
        start = _ts(2026, 6, 3, 10, 0)  # 2h fa
        anchors = [start, _ts(2026, 6, 3, 10, 30), _ts(2026, 6, 3, 11, 0)]
        msgs = [(start, 10.0, 100, 200), (_ts(2026, 6, 3, 11, 0), 20.0, 300, 400)]
        w = bridge.compute_window5h(anchors, msgs, now, plan_limit_5h=200.0)
        self.assertTrue(w["active"])
        self.assertEqual(w["messages"], 2)
        self.assertAlmostEqual(w["cost_usd"], 30.0)
        self.assertEqual(w["tokens_in"], 400)
        self.assertEqual(w["tokens_out"], 600)
        self.assertEqual(w["elapsed_min"], 120)
        self.assertEqual(w["remaining_min"], 180)
        self.assertEqual(w["limit_pct"], 15)  # 30/200 = 15%

    def test_window_expired_is_inactive(self):
        now = _ts(2026, 6, 3, 12, 0)
        # Unico messaggio 6h fa: start+5h è già passato => finestra scaduta.
        old = _ts(2026, 6, 3, 6, 0)
        w = bridge.compute_window5h([old], [(old, 5.0, 1, 1)], now, plan_limit_5h=200.0)
        self.assertFalse(w["active"])

    def test_window_gap_over_5h_starts_new_window(self):
        now = _ts(2026, 6, 3, 12, 0)
        old = _ts(2026, 6, 3, 2, 0)       # cluster vecchio (10h fa)
        recent = _ts(2026, 6, 3, 11, 30)  # nuovo messaggio: gap > 5h
        anchors = [old, recent]
        msgs = [(old, 99.0, 1, 1), (recent, 4.0, 10, 20)]
        w = bridge.compute_window5h(anchors, msgs, now, plan_limit_5h=200.0)
        self.assertTrue(w["active"])
        # Solo il messaggio recente conta: il vecchio precede window_start.
        self.assertEqual(w["messages"], 1)
        self.assertAlmostEqual(w["cost_usd"], 4.0)

    def test_window_zero_limit_does_not_divide_by_zero(self):
        now = _ts(2026, 6, 3, 12, 0)
        start = _ts(2026, 6, 3, 11, 0)
        w = bridge.compute_window5h([start], [(start, 5.0, 1, 1)], now, plan_limit_5h=0.0)
        self.assertEqual(w["limit_pct"], 0)


class TestFileIsRelevant(unittest.TestCase):
    def _mtime(self, year, month, day):
        # mtime POSIX della mezzanotte locale di una data (coerente col cutoff,
        # anch'esso calcolato con .astimezone()).
        return dt.datetime.combine(
            dt.date(year, month, day), dt.time.min
        ).astimezone().timestamp()

    def test_file_older_than_cutoff_minus_buffer_is_skipped(self):
        oldest = dt.date(2026, 6, 1)  # cutoff = 2026-05-30 (buffer 2gg)
        self.assertFalse(
            bridge.file_is_relevant(self._mtime(2026, 5, 20), oldest, buffer_days=2)
        )

    def test_file_within_buffer_is_kept(self):
        oldest = dt.date(2026, 6, 1)  # cutoff = 2026-05-30
        self.assertTrue(
            bridge.file_is_relevant(self._mtime(2026, 5, 31), oldest, buffer_days=2)
        )

    def test_file_newer_than_oldest_needed_is_kept(self):
        oldest = dt.date(2026, 6, 1)
        self.assertTrue(
            bridge.file_is_relevant(self._mtime(2026, 6, 5), oldest, buffer_days=2)
        )


class TestLoadOrCreateToken(unittest.TestCase):
    def _patched_paths(self, base):
        token_dir = bridge.Path(base) / "store"
        token_path = token_dir / "token"
        ctx = unittest.mock.patch.multiple(
            bridge, TOKEN_DIR=token_dir, TOKEN_PATH=token_path
        )
        return token_dir, token_path, ctx

    def test_explicit_token_is_returned_and_not_persisted(self):
        with tempfile.TemporaryDirectory() as base:
            _, token_path, ctx = self._patched_paths(base)
            with ctx:
                got = bridge.load_or_create_token("explicit-value")
            self.assertEqual(got, "explicit-value")
            self.assertFalse(token_path.exists())

    def test_generated_token_is_persisted_and_stable_across_calls(self):
        with tempfile.TemporaryDirectory() as base:
            _, token_path, ctx = self._patched_paths(base)
            with ctx:
                first = bridge.load_or_create_token(None)
                self.assertTrue(token_path.exists())
                self.assertEqual(token_path.read_text(), first)
                # Seconda chiamata: deve rileggere il file, non rigenerare.
                second = bridge.load_or_create_token(None)
            self.assertEqual(second, first)

    @unittest.skipUnless(os.name == "posix", "permessi POSIX-only")
    def test_generated_token_file_has_0600_perms(self):
        with tempfile.TemporaryDirectory() as base:
            token_dir, token_path, ctx = self._patched_paths(base)
            with ctx:
                bridge.load_or_create_token(None)
            self.assertEqual(stat.S_IMODE(os.stat(token_path).st_mode), 0o600)
            self.assertEqual(stat.S_IMODE(os.stat(token_dir).st_mode), 0o700)


if __name__ == "__main__":
    unittest.main()
