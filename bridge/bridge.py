#!/usr/bin/env python3
"""
Claude Code Usage Bridge — reads Claude Code's JSONL transcripts
(~/.claude/projects/**/*.jsonl) and exposes them as JSON over HTTP for
the ESP32-S3-LCD-1.47 firmware.

Python 3.10+ stdlib only. Start with:
    python3 bridge.py                    # port 8787, budget 500 USD
    python3 bridge.py --port 9000 --budget 200
"""

import argparse
import datetime as dt
import glob
import hmac
import json
import os
import secrets as _secrets
import socket
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

PROJECTS_DIR = Path.home() / ".claude" / "projects"
DEFAULT_PRICING_FILE = Path(__file__).parent / "pricing.json"
TOKEN_DIR = Path.home() / ".claude-code-usage"
TOKEN_PATH = TOKEN_DIR / "token"

# Duration of Claude Code's rolling rate-limit window (hours).
WINDOW_HOURS = 5

# USD per 1M tokens. Editable via pricing.json.
DEFAULT_PRICING = {
    "claude-opus-4-7":    {"input": 15.00, "output": 75.00, "cache_write": 18.75, "cache_read": 1.50},
    "claude-opus-4-6":    {"input": 15.00, "output": 75.00, "cache_write": 18.75, "cache_read": 1.50},
    "claude-opus-4":      {"input": 15.00, "output": 75.00, "cache_write": 18.75, "cache_read": 1.50},
    "claude-sonnet-4-6":  {"input":  3.00, "output": 15.00, "cache_write":  3.75, "cache_read": 0.30},
    "claude-sonnet-4-5":  {"input":  3.00, "output": 15.00, "cache_write":  3.75, "cache_read": 0.30},
    "claude-sonnet-4":    {"input":  3.00, "output": 15.00, "cache_write":  3.75, "cache_read": 0.30},
    "claude-haiku-4-5":   {"input":  0.80, "output":  4.00, "cache_write":  1.00, "cache_read": 0.08},
    "claude-haiku-4":     {"input":  0.80, "output":  4.00, "cache_write":  1.00, "cache_read": 0.08},
    # default fallback for unknown models
    "_default":           {"input":  3.00, "output": 15.00, "cache_write":  3.75, "cache_read": 0.30},
}


def load_pricing():
    if DEFAULT_PRICING_FILE.exists():
        try:
            with DEFAULT_PRICING_FILE.open() as f:
                return json.load(f)
        except Exception as e:
            print(f"[warn] pricing.json not readable ({e}); using defaults")
    return DEFAULT_PRICING


def price_for(model: str, pricing: dict) -> dict:
    if model in pricing:
        return pricing[model]
    # match by prefix (e.g. "claude-opus-4-7-20251201" -> "claude-opus-4-7")
    for key in sorted(pricing.keys(), key=len, reverse=True):
        if key != "_default" and model.startswith(key):
            return pricing[key]
    return pricing.get("_default", DEFAULT_PRICING["_default"])


def compute_cost(usage: dict, model: str, pricing: dict) -> float:
    p = price_for(model, pricing)
    inp = usage.get("input_tokens", 0) or 0
    out = usage.get("output_tokens", 0) or 0
    cw = usage.get("cache_creation_input_tokens", 0) or 0
    cr = usage.get("cache_read_input_tokens", 0) or 0
    return (
        inp * p["input"] / 1_000_000
        + out * p["output"] / 1_000_000
        + cw * p["cache_write"] / 1_000_000
        + cr * p["cache_read"] / 1_000_000
    )


def parse_ts(s: str) -> dt.datetime | None:
    if not s:
        return None
    try:
        # e.g. "2026-05-16T14:45:01.824Z"
        return dt.datetime.fromisoformat(s.replace("Z", "+00:00"))
    except Exception:
        return None


def compute_window5h(
    anchor_ts: list[dt.datetime],
    msgs: list[tuple[dt.datetime, float, int, int]],
    now: dt.datetime,
    plan_limit_5h: float,
    window_hours: int = WINDOW_HOURS,
) -> dict:
    """Compute the state of the rolling `window_hours`-hour rate-limit window.

    Pure: no I/O, no internal now() call — `now` is injected, so the logic
    stays deterministic and testable.

    - anchor_ts: timestamps (user + assistant) within the recent window. A new
      window starts when a timestamp exceeds the previous window's start by
      >window_hours (Anthropic measures from the request's first message, not
      from the completion of generation).
    - msgs: (ts, cost_usd, tokens_in, tokens_out) tuples for assistant messages
      only, to sum the active window's cost/tokens.
    """
    window5h = {
        "active": False,
        "messages": 0,
        "cost_usd": 0.0,
        "tokens_in": 0,
        "tokens_out": 0,
        "elapsed_min": 0,
        "remaining_min": window_hours * 60,
        "limit_usd": round(plan_limit_5h, 2),
        "limit_pct": 0,
        "start": None,
        "reset_at": None,
    }

    window_start: dt.datetime | None = None
    for ts in sorted(anchor_ts):
        if window_start is None or ts > window_start + dt.timedelta(hours=window_hours):
            window_start = ts

    if window_start is None:
        return window5h

    reset_at = window_start + dt.timedelta(hours=window_hours)
    if reset_at <= now:
        # Window already expired: no active window.
        return window5h

    wm = wi = wo = 0
    wc = 0.0
    for ts, c, i, o in msgs:
        if ts >= window_start:
            wm += 1
            wc += c
            wi += i
            wo += o

    elapsed_s = (now - window_start).total_seconds()
    remaining_s = (reset_at - now).total_seconds()
    limit_pct = 0
    if plan_limit_5h > 0.001:
        limit_pct = int(round(wc / plan_limit_5h * 100))

    window5h.update({
        "active": True,
        "messages": wm,
        "cost_usd": round(wc, 4),
        "tokens_in": wi,
        "tokens_out": wo,
        "elapsed_min": max(0, int(elapsed_s / 60)),
        "remaining_min": max(0, int(remaining_s / 60)),
        "limit_pct": max(0, min(999, limit_pct)),
        "start": window_start.astimezone().replace(microsecond=0).isoformat(),
        "reset_at": reset_at.astimezone().replace(microsecond=0).isoformat(),
    })
    return window5h


def file_is_relevant(
    file_mtime_ts: float, oldest_needed_date: dt.date, buffer_days: int = 2
) -> bool:
    """True if a file with this mtime CAN contribute to any view.

    A transcript is append-only: every line timestamp is <= the file's mtime.
    So if the mtime is older than the oldest date any view needs (minus a
    buffer for clock skew / timezone), the file is irrelevant and can be
    skipped without reopening it — this is the optimization that avoids
    re-reading the entire history on every refresh.

    Note: this does not cover files restored from backup with an old mtime but
    recent contents — the --rescan-all flag exists for that case.
    """
    cutoff = (
        dt.datetime.combine(oldest_needed_date, dt.time.min).astimezone()
        - dt.timedelta(days=buffer_days)
    )
    return file_mtime_ts >= cutoff.timestamp()


class Aggregator:
    """Aggregates all JSONL transcripts into 4 views."""

    def __init__(self, pricing: dict, budget_monthly_usd: float,
                 plan_limit_5h_usd: float, rescan_all: bool = False):
        self.pricing = pricing
        self.budget = budget_monthly_usd
        self.plan_limit_5h = plan_limit_5h_usd
        self.rescan_all = rescan_all

    def collect(self) -> dict:
        now = dt.datetime.now(dt.timezone.utc).astimezone()
        today = now.date()
        month_start = today.replace(day=1)
        seven_days_ago = today - dt.timedelta(days=6)  # last 7 days, inclusive

        today_agg = {"cost_usd": 0.0, "tokens_in": 0, "tokens_out": 0, "cache_read": 0}
        month_agg = {"cost_usd": 0.0, "tokens_in": 0, "tokens_out": 0, "cache_read": 0}
        by_model: dict[str, dict] = {}
        per_day: dict[dt.date, float] = {}

        # To compute the 5h window we keep:
        # - recent_msgs: assistant messages (with cost/tokens) for the totals
        # - recent_anchor_ts: ALL timestamps (user + assistant) to find the
        #   true window start (Anthropic measures from the user request, not
        #   from the end of assistant generation — typical difference 10-30s/min).
        recent_msgs: list[tuple[dt.datetime, float, int, int]] = []
        recent_anchor_ts: list[dt.datetime] = []
        recent_cutoff = now - dt.timedelta(hours=WINDOW_HOURS * 2)

        # Claude Code sometimes writes the same assistant message more than once
        # (resume/compact, intermediate tool calls). We dedupe by message.id so
        # the same usage is not counted 2-5 times.
        seen_msg_ids: set[str] = set()

        # Oldest date any view can need: the min of month start ("month" view)
        # and 7 days ago ("last7" view); the 5h window is always more recent.
        # Files with an older mtime are skipped (see file_is_relevant), unless
        # --rescan-all is set.
        oldest_needed = min(month_start, seven_days_ago)

        files = glob.glob(str(PROJECTS_DIR / "**" / "*.jsonl"), recursive=True)

        for path in files:
            try:
                if not self.rescan_all and not file_is_relevant(
                    os.path.getmtime(path), oldest_needed
                ):
                    continue
                with open(path, "r", encoding="utf-8", errors="ignore") as f:
                    for line in f:
                        line = line.strip()
                        if not line:
                            continue
                        try:
                            obj = json.loads(line)
                        except json.JSONDecodeError:
                            continue
                        ev_type = obj.get("type")
                        ts = parse_ts(obj.get("timestamp", ""))
                        if not ts:
                            continue

                        # Anchor timestamp for user too, for the window calculation
                        if ev_type in ("user", "assistant") and ts >= recent_cutoff:
                            recent_anchor_ts.append(ts)

                        if ev_type != "assistant":
                            continue
                        msg = obj.get("message") or {}
                        if not isinstance(msg, dict):
                            continue
                        usage = msg.get("usage")
                        if not usage:
                            continue
                        mid = msg.get("id")
                        if mid:
                            if mid in seen_msg_ids:
                                continue
                            seen_msg_ids.add(mid)
                        model = msg.get("model") or "_default"
                        date = ts.astimezone().date()
                        cost = compute_cost(usage, model, self.pricing)
                        inp = usage.get("input_tokens", 0) or 0
                        out = usage.get("output_tokens", 0) or 0
                        cr = usage.get("cache_read_input_tokens", 0) or 0

                        if date == today:
                            today_agg["cost_usd"] += cost
                            today_agg["tokens_in"] += inp
                            today_agg["tokens_out"] += out
                            today_agg["cache_read"] += cr
                        if date >= month_start:
                            month_agg["cost_usd"] += cost
                            month_agg["tokens_in"] += inp
                            month_agg["tokens_out"] += out
                            month_agg["cache_read"] += cr
                            slot = by_model.setdefault(
                                model,
                                {"name": model, "cost_usd": 0.0, "tokens_in": 0, "tokens_out": 0},
                            )
                            slot["cost_usd"] += cost
                            slot["tokens_in"] += inp
                            slot["tokens_out"] += out
                        if date >= seven_days_ago:
                            per_day[date] = per_day.get(date, 0.0) + cost
                        if ts >= recent_cutoff:
                            recent_msgs.append((ts, cost, inp, out))
            except OSError:
                continue

        # ----- 5h window calculation (pure, testable logic) -----
        window5h = compute_window5h(
            recent_anchor_ts, recent_msgs, now, self.plan_limit_5h, WINDOW_HOURS
        )

        last7 = []
        for i in range(6, -1, -1):
            d = today - dt.timedelta(days=i)
            last7.append({"date": d.isoformat(), "cost_usd": round(per_day.get(d, 0.0), 4)})

        for k in ("cost_usd",):
            today_agg[k] = round(today_agg[k], 4)
            month_agg[k] = round(month_agg[k], 4)

        models_list = sorted(
            (
                {
                    "name": m["name"],
                    "cost_usd": round(m["cost_usd"], 4),
                    "tokens_in": m["tokens_in"],
                    "tokens_out": m["tokens_out"],
                }
                for m in by_model.values()
            ),
            key=lambda x: x["cost_usd"],
            reverse=True,
        )

        return {
            "ts": now.replace(microsecond=0).isoformat(),
            "today": today_agg,
            "month": month_agg,
            "last7": last7,
            "by_model": models_list,
            "budget_monthly_usd": self.budget,
            "window5h": window5h,
        }


# ----- Lightweight in-RAM cache to avoid re-scanning on every request -----
class CachedAggregator:
    """In-RAM cache with TTL.

    The scan (`agg.collect()`) is NOT run while holding the data lock: readers
    that already have a cached value (even a stale one) get it immediately
    while a single thread does the refresh, instead of serializing for the
    whole duration of the scan. One refresh at a time (`_refresh_lock`) avoids
    the thundering herd. `clock` is injectable to make the TTL testable.
    """

    def __init__(self, agg: Aggregator, ttl_seconds: float = 2.0,
                 clock=time.monotonic):
        self.agg = agg
        self.ttl = ttl_seconds
        self._clock = clock
        self._lock = threading.Lock()          # protects only _cached/_cached_at
        self._refresh_lock = threading.Lock()  # ensures one refresh at a time
        self._cached: dict | None = None
        self._cached_at: float = 0.0

    def _fresh(self) -> dict | None:
        with self._lock:
            if self._cached is not None and (self._clock() - self._cached_at) < self.ttl:
                return self._cached
            return None

    def get(self) -> dict:
        cached = self._fresh()
        if cached is not None:
            return cached

        with self._lock:
            stale = self._cached  # can be None only on first startup

        # Cache miss. A single thread does the scan (refresh_lock). If a value
        # already exists (even a stale one) and another thread is already
        # refreshing, we return it immediately instead of blocking for the
        # duration of the scan.
        got_lock = self._refresh_lock.acquire(blocking=(stale is None))
        if not got_lock:
            return stale  # stale is guaranteed non-None when acquire is non-blocking
        try:
            cached = self._fresh()  # another thread may have just refreshed
            if cached is not None:
                return cached
            data = self.agg.collect()  # outside the data lock
            with self._lock:
                self._cached = data
                self._cached_at = self._clock()
            return data
        finally:
            self._refresh_lock.release()


def load_or_create_token(explicit: str | None) -> str:
    """Return a bearer token persisted in ~/.claude-code-usage/token.

    If `explicit` is provided, use it directly (and don't save). Otherwise load
    the file if it exists, or generate a new one (24 urlsafe bytes).
    Best-effort chmod 0600 on the file and 0700 on the directory.
    """
    if explicit:
        return explicit
    if TOKEN_PATH.exists():
        try:
            tok = TOKEN_PATH.read_text().strip()
            if tok:
                return tok
        except OSError:
            pass
    tok = _secrets.token_urlsafe(24)
    try:
        TOKEN_DIR.mkdir(parents=True, exist_ok=True)
        try:
            os.chmod(TOKEN_DIR, 0o700)
        except OSError:
            pass  # Windows / non-POSIX FS: best-effort
        # Atomic write with 0600 from creation: no window in which the token is
        # readable by other local users (the chmod-after approach allowed it),
        # and no half-truncated file if the process dies during the write
        # (write-tmp + replace).
        tmp = TOKEN_PATH.with_name(TOKEN_PATH.name + ".tmp")
        fd = os.open(str(tmp), os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        try:
            os.write(fd, tok.encode("utf-8"))
        finally:
            os.close(fd)
        os.replace(str(tmp), str(TOKEN_PATH))
    except OSError as e:
        print(f"[warn] could not save token to {TOKEN_PATH}: {e}")
    return tok


def constant_time_eq(a: str, b: str) -> bool:
    return hmac.compare_digest(a.encode("utf-8"), b.encode("utf-8"))


def render_metrics(data: dict) -> bytes:
    w = data.get("window5h") or {}
    today = data.get("today") or {}
    month = data.get("month") or {}
    lines = [
        "# HELP cc_cost_today_usd Total cost USD today",
        "# TYPE cc_cost_today_usd gauge",
        f'cc_cost_today_usd {float(today.get("cost_usd", 0.0))}',
        "# HELP cc_cost_month_usd Total cost USD month-to-date",
        "# TYPE cc_cost_month_usd gauge",
        f'cc_cost_month_usd {float(month.get("cost_usd", 0.0))}',
        "# HELP cc_window5h_pct 5h rolling window utilization percent of limit",
        "# TYPE cc_window5h_pct gauge",
        f'cc_window5h_pct {int(w.get("limit_pct", 0))}',
        "# HELP cc_window5h_cost_usd 5h rolling window cost USD",
        "# TYPE cc_window5h_cost_usd gauge",
        f'cc_window5h_cost_usd {float(w.get("cost_usd", 0.0))}',
        "# HELP cc_messages_total Assistant messages counted in current 5h window",
        "# TYPE cc_messages_total gauge",
        f'cc_messages_total {int(w.get("messages", 0))}',
        "",
    ]
    return "\n".join(lines).encode("utf-8")


def make_handler(cache: CachedAggregator, token: str, require_auth: bool, metrics_anon: bool):
    class Handler(BaseHTTPRequestHandler):
        def _send_json(self, code: int, payload: dict):
            body = json.dumps(payload).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            # No Access-Control-Allow-Origin: the endpoints are authenticated
            # (bearer token) and the consumers — ESP32 firmware and Prometheus
            # scraper — are not browsers, so CORS is not needed. A "*" on an
            # authenticated endpoint is an anti-pattern: we don't expose it.
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def _send_unauthorized(self):
            body = b'{"error":"unauthorized"}'
            self.send_response(401)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("WWW-Authenticate", 'Bearer realm="cc-monitor"')
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def _authorized(self) -> bool:
            if not require_auth:
                return True
            hdr = self.headers.get("Authorization", "")
            if not hdr.startswith("Bearer "):
                return False
            return constant_time_eq(hdr[7:].strip(), token)

        def do_GET(self):  # noqa: N802
            # Normalize: exact comparison on the path without query string, so
            # /usage?x=1 works but /usagexyz does not (startswith used to accept it).
            path = self.path.split("?", 1)[0]
            if path == "/usage":
                if not self._authorized():
                    self._send_unauthorized()
                    return
                try:
                    data = cache.get()
                    self._send_json(200, data)
                except Exception as e:
                    # Log details server-side, generic response to the client to
                    # avoid leaking stack traces or paths over the LAN.
                    sys.stderr.write(f"[error] /usage: {e!r}\n")
                    self._send_json(500, {"error": "internal error"})
            elif path == "/metrics":
                if not (metrics_anon or self._authorized()):
                    self._send_unauthorized()
                    return
                try:
                    body = render_metrics(cache.get())
                    self.send_response(200)
                    self.send_header("Content-Type", "text/plain; version=0.0.4")
                    self.send_header("Content-Length", str(len(body)))
                    self.send_header("Cache-Control", "no-store")
                    self.end_headers()
                    self.wfile.write(body)
                except Exception as e:
                    sys.stderr.write(f"[error] /metrics: {e!r}\n")
                    self._send_json(500, {"error": "internal error"})
            elif path == "/health":
                # Liveness probe — always anonymous
                self._send_json(200, {"ok": True})
            else:
                self._send_json(404, {"error": "not found"})

        def log_message(self, fmt, *args):  # silence default logging
            sys.stdout.write(f"[{self.log_date_time_string()}] {fmt % args}\n")

    return Handler


def local_ip() -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return "127.0.0.1"


# ===== Announce (reverse setup): the bridge tells the device where to find it =====
#
# The device always pulls /usage; only WHO communicates the address changes.
# Instead of opening the captive portal to re-enter the laptop's IP when it
# changes, the bridge resolves claudemonitor.local (mDNS) and POSTs its own IP
# to the device's control endpoint. stdlib only: getaddrinfo + urllib.

def resolve_device(name: str, fallback_ip: str | None = None) -> str:
    """Resolve an mDNS/DNS hostname to an IPv4.

    On macOS .local names resolve via Bonjour with no dependencies; on Linux
    you need avahi/nss-mdns (or pass --device-ip). If resolution fails and a
    fallback is given, return it, otherwise re-raise the exception.
    """
    try:
        infos = socket.getaddrinfo(name, None, family=socket.AF_INET)
        return infos[0][4][0]
    except (socket.gaierror, OSError):
        if fallback_ip:
            return fallback_ip
        raise


def announce_once(device_ip: str, control_port: int, token: str,
                  host_ip: str, bridge_port: int, timeout: float = 3.0) -> bool:
    """POST host/port/token to the device's /config endpoint. True on HTTP 2xx.

    Sends form-urlencoded (the ESP32 WebServer only exposes urlencoded bodies
    via arg()) and repeats the token in the Authorization header for the
    device-side check.
    """
    url = f"http://{device_ip}:{control_port}/config"
    form = {"host": host_ip, "port": str(bridge_port)}
    if token:
        form["token"] = token
    data = urllib.parse.urlencode(form).encode("utf-8")
    req = urllib.request.Request(url, data=data, method="POST")
    req.add_header("Content-Type", "application/x-www-form-urlencoded")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return 200 <= resp.status < 300
    except urllib.error.HTTPError as e:
        sys.stderr.write(f"[announce] {url} -> HTTP {e.code}\n")
        return False
    except (urllib.error.URLError, OSError) as e:
        sys.stderr.write(f"[announce] {url} -> {e}\n")
        return False


def announce_loop(device_name: str, fallback_ip: str | None, control_port: int,
                  bridge_port: int, token: str, interval: float,
                  stop_event: threading.Event) -> None:
    """Periodically announce the bridge's IP to the device until stop_event is set.

    local_ip() is recomputed each loop: if the laptop's IP changes during the
    session the device is re-pointed automatically, without restarting anything.
    """
    while not stop_event.is_set():
        try:
            device_ip = resolve_device(device_name, fallback_ip)
            host_ip = local_ip()
            if announce_once(device_ip, control_port, token, host_ip, bridge_port):
                sys.stdout.write(
                    f"[announce] {device_name} ({device_ip}) ← bridge {host_ip}:{bridge_port}\n")
        except (socket.gaierror, OSError) as e:
            sys.stderr.write(
                f"[announce] could not resolve {device_name}: {e} "
                f"(pass --device-ip <ip> as fallback)\n")
        stop_event.wait(interval)


PLAN_PRESETS = {
    "pro":     40.0,    # Claude Pro ~$20/month, 5h limit estimated at $40 equivalent API cost
    "max5":    200.0,   # Claude Max 5x ~$100/month, 5h limit estimated at $200
    "max20":   1000.0,  # Claude Max 20x ~$200/month, 5h limit estimated at $1000
}


def main():
    ap = argparse.ArgumentParser(description="Claude Code Usage Bridge")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=8787)
    ap.add_argument("--budget", type=float, default=500.0, help="Monthly budget USD (month info)")
    ap.add_argument("--plan", choices=list(PLAN_PRESETS.keys()), default="max5",
                    help="Plan used to estimate the 5h limit (pro/max5/max20)")
    ap.add_argument("--plan-limit", type=float, default=None,
                    help="Override the 5h window limit in USD (e.g. --plan-limit 200)")
    ap.add_argument("--ttl", type=float, default=2.0, help="Cache TTL seconds")
    ap.add_argument("--token", default=None,
                    help="Explicit bearer token (skips generation/persistence)")
    ap.add_argument("--no-auth", action="store_true",
                    help="Disable authentication (not recommended, prints a warning)")
    ap.add_argument("--metrics-anon", action="store_true",
                    help="Expose /metrics without authentication (for Prometheus scrapers)")
    ap.add_argument("--rescan-all", action="store_true",
                    help="Re-scan ALL transcripts on every refresh, ignoring the mtime "
                         "(default: skip files too old to contribute). Use it if you "
                         "restore transcripts from backup with an old mtime.")
    ap.add_argument("--announce", dest="announce", action="store_true", default=True,
                    help="Announce the bridge's IP to the device via claudemonitor.local "
                         "(default: on). The device keeps pulling /usage.")
    ap.add_argument("--no-announce", dest="announce", action="store_false",
                    help="Disable the automatic announcement to the device.")
    ap.add_argument("--device-name", default="claudemonitor.local",
                    help="mDNS hostname of the device to re-point (default claudemonitor.local)")
    ap.add_argument("--device-ip", default=None,
                    help="Device IP as a fallback if mDNS does not resolve (e.g. on Linux "
                         "without avahi)")
    ap.add_argument("--device-control-port", type=int, default=80,
                    help="Port of the control endpoint on the device (default 80)")
    ap.add_argument("--announce-interval", type=float, default=30.0,
                    help="Seconds between one announcement and the next (default 30)")
    ap.add_argument("--announce-once", action="store_true",
                    help="Announce once and exit (useful for tests / point-and-exit)")
    args = ap.parse_args()

    if not PROJECTS_DIR.exists():
        print(f"[warn] {PROJECTS_DIR} does not exist — no transcripts will be found")

    pricing = load_pricing()
    plan_limit = args.plan_limit if args.plan_limit is not None else PLAN_PRESETS[args.plan]
    agg = Aggregator(pricing, args.budget, plan_limit, rescan_all=args.rescan_all)
    cache = CachedAggregator(agg, ttl_seconds=args.ttl)

    require_auth = not args.no_auth
    token = load_or_create_token(args.token) if require_auth else ""
    handler_cls = make_handler(cache, token, require_auth, args.metrics_anon)
    server = ThreadingHTTPServer((args.host, args.port), handler_cls)
    ip = local_ip()
    print(f"Claude Code Usage Bridge started")
    print(f"  listening on: http://{args.host}:{args.port}")
    print(f"  local IP:     http://{ip}:{args.port}/usage")
    print(f"  month budget: {args.budget:.2f} USD")
    print(f"  5h limit:     {plan_limit:.2f} USD ({args.plan})")
    print(f"  cache TTL:    {args.ttl:.1f} s")
    print(f"  projects dir: {PROJECTS_DIR}")
    print()
    if require_auth:
        short = (token[:4] + "..." + token[-4:]) if len(token) > 10 else token
        print(f"  auth:         bearer (token persisted in {TOKEN_PATH})")
        print(f"  token:        {token}")
        print(f"  short:        {short}")
        print()
        print(f"On the ESP32, in secrets.h (or in the captive portal) set:")
        print(f"    #define BRIDGE_HOST   \"{ip}\"")
        print(f"    #define BRIDGE_PORT   {args.port}")
        print(f"    #define BRIDGE_TOKEN  \"{token}\"")
        print()
        print(f"Quick test:")
        print(f"    TOK={token}")
        print(f"    curl -H \"Authorization: Bearer $TOK\" http://{ip}:{args.port}/usage")
    else:
        print(f"  auth:         DISABLED (--no-auth)")
        print(f"  WARNING: anyone on the network can read your Claude Code usage.")
        print(f"  Use only for local debugging, never exposed to LAN/Internet.")
        print()
        print(f"On the ESP32, in secrets.h set:")
        print(f"    #define BRIDGE_HOST  \"{ip}\"")
        print(f"    #define BRIDGE_PORT  {args.port}")

    # Reverse announcement: the bridge re-points the device via mDNS, so the
    # captive portal is no longer needed when the laptop's IP changes.
    if args.announce or args.announce_once:
        print()
        print(f"  announce:     {args.device_name}:{args.device_control_port}"
              f"{' (fallback ' + args.device_ip + ')' if args.device_ip else ''}")

    if args.announce_once:
        device_ip = resolve_device(args.device_name, args.device_ip)
        ok = announce_once(device_ip, args.device_control_port, token, ip, args.port)
        print(f"[announce] {args.device_name} ({device_ip}) "
              f"{'ok' if ok else 'FAILED'} ← bridge {ip}:{args.port}")
        return

    stop_event = threading.Event()
    if args.announce:
        t = threading.Thread(
            target=announce_loop,
            args=(args.device_name, args.device_ip, args.device_control_port,
                  args.port, token, args.announce_interval, stop_event),
            daemon=True,
        )
        t.start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nGoodbye.")
    finally:
        stop_event.set()


if __name__ == "__main__":
    main()
