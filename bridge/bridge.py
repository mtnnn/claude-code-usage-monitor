#!/usr/bin/env python3
"""
Claude Code Usage Bridge — legge i transcript JSONL di Claude Code
(~/.claude/projects/**/*.jsonl) e li espone come JSON via HTTP per
il firmware ESP32-S3-LCD-1.47.

Solo stdlib Python 3.10+. Avvio:
    python3 bridge.py                    # porta 8787, budget 500 USD
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
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

PROJECTS_DIR = Path.home() / ".claude" / "projects"
DEFAULT_PRICING_FILE = Path(__file__).parent / "pricing.json"
TOKEN_DIR = Path.home() / ".claude-code-usage"
TOKEN_PATH = TOKEN_DIR / "token"

# Durata della finestra rate-limit rolling di Claude Code (ore).
WINDOW_HOURS = 5

# USD per 1M tokens. Editabile via pricing.json.
DEFAULT_PRICING = {
    "claude-opus-4-7":    {"input": 15.00, "output": 75.00, "cache_write": 18.75, "cache_read": 1.50},
    "claude-opus-4-6":    {"input": 15.00, "output": 75.00, "cache_write": 18.75, "cache_read": 1.50},
    "claude-opus-4":      {"input": 15.00, "output": 75.00, "cache_write": 18.75, "cache_read": 1.50},
    "claude-sonnet-4-6":  {"input":  3.00, "output": 15.00, "cache_write":  3.75, "cache_read": 0.30},
    "claude-sonnet-4-5":  {"input":  3.00, "output": 15.00, "cache_write":  3.75, "cache_read": 0.30},
    "claude-sonnet-4":    {"input":  3.00, "output": 15.00, "cache_write":  3.75, "cache_read": 0.30},
    "claude-haiku-4-5":   {"input":  0.80, "output":  4.00, "cache_write":  1.00, "cache_read": 0.08},
    "claude-haiku-4":     {"input":  0.80, "output":  4.00, "cache_write":  1.00, "cache_read": 0.08},
    # default fallback per modelli sconosciuti
    "_default":           {"input":  3.00, "output": 15.00, "cache_write":  3.75, "cache_read": 0.30},
}


def load_pricing():
    if DEFAULT_PRICING_FILE.exists():
        try:
            with DEFAULT_PRICING_FILE.open() as f:
                return json.load(f)
        except Exception as e:
            print(f"[warn] pricing.json non leggibile ({e}); uso default")
    return DEFAULT_PRICING


def price_for(model: str, pricing: dict) -> dict:
    if model in pricing:
        return pricing[model]
    # match by prefix (es. "claude-opus-4-7-20251201" -> "claude-opus-4-7")
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
        # Es. "2026-05-16T14:45:01.824Z"
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
    """Calcola lo stato della finestra rate-limit rolling di `window_hours` ore.

    Pura: nessun I/O, nessuna chiamata a now() interna — `now` è iniettato, così
    la logica resta deterministica e testabile.

    - anchor_ts: timestamp (user + assistant) entro la finestra recente. Una
      nuova finestra inizia quando un timestamp supera di >window_hours lo start
      della finestra precedente (Anthropic misura dal primo messaggio della
      richiesta, non dal completamento della generazione).
    - msgs: tuple (ts, cost_usd, tokens_in, tokens_out) dei soli assistant
      message, per sommare costo/token della finestra attiva.
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
        # Finestra già scaduta: nessuna finestra attiva.
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
    """True se un file con questo mtime PUÒ contribuire a qualche vista.

    Un transcript è append-only: tutti i timestamp di riga sono <= mtime del
    file. Quindi se l'mtime è anteriore alla data più vecchia richiesta da una
    vista (meno un buffer per clock skew / timezone), il file è ininfluente e
    può essere saltato senza riaprirlo — è questa l'ottimizzazione che evita di
    rileggere tutta la storia ad ogni refresh.

    NB: non copre i file ripristinati da backup con mtime datato ma contenuto
    recente — per quel caso esiste il flag --rescan-all.
    """
    cutoff = (
        dt.datetime.combine(oldest_needed_date, dt.time.min).astimezone()
        - dt.timedelta(days=buffer_days)
    )
    return file_mtime_ts >= cutoff.timestamp()


class Aggregator:
    """Aggrega tutti i transcript JSONL in 4 viste."""

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
        seven_days_ago = today - dt.timedelta(days=6)  # ultimi 7 giorni inclusi

        today_agg = {"cost_usd": 0.0, "tokens_in": 0, "tokens_out": 0, "cache_read": 0}
        month_agg = {"cost_usd": 0.0, "tokens_in": 0, "tokens_out": 0, "cache_read": 0}
        by_model: dict[str, dict] = {}
        per_day: dict[dt.date, float] = {}

        # Per calcolare la finestra 5h teniamo:
        # - recent_msgs: assistant message (con cost/token) per i totali
        # - recent_anchor_ts: TUTTI i timestamp (user + assistant) per trovare
        #   il vero start della finestra (Anthropic misura dalla richiesta user,
        #   non dalla fine generazione assistant — differenza tipica 10-30s/min).
        recent_msgs: list[tuple[dt.datetime, float, int, int]] = []
        recent_anchor_ts: list[dt.datetime] = []
        recent_cutoff = now - dt.timedelta(hours=WINDOW_HOURS * 2)

        # Claude Code talvolta scrive lo stesso messaggio assistant più volte
        # (resume/compact, tool call intermedi). Dedupplichiamo per message.id
        # per non contare lo stesso usage 2-5 volte.
        seen_msg_ids: set[str] = set()

        # Data più vecchia che una vista può richiedere: il min tra inizio mese
        # (vista "month") e 7 giorni fa (vista "last7"); la finestra 5h è sempre
        # più recente. I file con mtime anteriore vengono saltati (vedi
        # file_is_relevant), a meno di --rescan-all.
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

                        # Anchor timestamp anche per user, per il calcolo finestra
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

        # ----- Calcolo finestra 5h (logica pura, testabile) -----
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


# ----- Cache leggera in RAM per evitare ri-scan a ogni richiesta -----
class CachedAggregator:
    """Cache in RAM con TTL.

    Lo scan (`agg.collect()`) NON viene eseguito tenendo il lock dei dati: i
    lettori che hanno già un valore in cache (anche scaduto) lo ricevono subito
    mentre un singolo thread fa il refresh, invece di serializzarsi per tutta la
    durata dello scan. Un solo refresh per volta (`_refresh_lock`) evita il
    thundering herd. `clock` è iniettabile per rendere il TTL testabile.
    """

    def __init__(self, agg: Aggregator, ttl_seconds: float = 2.0,
                 clock=time.monotonic):
        self.agg = agg
        self.ttl = ttl_seconds
        self._clock = clock
        self._lock = threading.Lock()          # protegge solo _cached/_cached_at
        self._refresh_lock = threading.Lock()  # garantisce un refresh per volta
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
            stale = self._cached  # può essere None solo al primo avvio

        # Cache miss. Un solo thread fa lo scan (refresh_lock). Se esiste già un
        # valore (anche scaduto) e un altro thread sta già rinfrescando, lo
        # restituiamo subito invece di bloccarci per la durata dello scan.
        got_lock = self._refresh_lock.acquire(blocking=(stale is None))
        if not got_lock:
            return stale  # stale è garantito non-None quando acquire è non-bloccante
        try:
            cached = self._fresh()  # un altro thread potrebbe aver appena rinfrescato
            if cached is not None:
                return cached
            data = self.agg.collect()  # fuori dal lock dei dati
            with self._lock:
                self._cached = data
                self._cached_at = self._clock()
            return data
        finally:
            self._refresh_lock.release()


def load_or_create_token(explicit: str | None) -> str:
    """Restituisce un bearer token persistente in ~/.claude-code-usage/token.

    Se `explicit` è fornito, lo usa direttamente (e non salva). Altrimenti
    carica il file se esiste, oppure ne genera uno nuovo (24 bytes urlsafe).
    Best-effort chmod 0600 sul file e 0700 sulla directory.
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
            pass  # Windows / FS non-POSIX: best-effort
        # Scrittura atomica con 0600 fin dalla creazione: nessuna finestra in
        # cui il token è leggibile da altri utenti locali (il chmod-dopo lo
        # consentiva), e nessun file troncato a metà se il processo muore
        # durante la scrittura (write-tmp + replace).
        tmp = TOKEN_PATH.with_name(TOKEN_PATH.name + ".tmp")
        fd = os.open(str(tmp), os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        try:
            os.write(fd, tok.encode("utf-8"))
        finally:
            os.close(fd)
        os.replace(str(tmp), str(TOKEN_PATH))
    except OSError as e:
        print(f"[warn] impossibile salvare token in {TOKEN_PATH}: {e}")
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
            # Niente Access-Control-Allow-Origin: gli endpoint sono autenticati
            # (bearer token) e i consumer — firmware ESP32 e scraper Prometheus —
            # non sono browser, quindi non serve CORS. Un "*" su un endpoint con
            # auth è un anti-pattern: non lo esponiamo.
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
            if self.path.startswith("/usage"):
                if not self._authorized():
                    self._send_unauthorized()
                    return
                try:
                    data = cache.get()
                    self._send_json(200, data)
                except Exception as e:
                    # Log dettagli server-side, risposta generica al client per
                    # non leakare stack trace o path su LAN.
                    sys.stderr.write(f"[error] /usage: {e!r}\n")
                    self._send_json(500, {"error": "internal error"})
            elif self.path == "/metrics":
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
            elif self.path == "/health":
                # Liveness probe — sempre anonimo
                self._send_json(200, {"ok": True})
            else:
                self._send_json(404, {"error": "not found"})

        def log_message(self, fmt, *args):  # silenzio log default
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


PLAN_PRESETS = {
    "pro":     40.0,    # Claude Pro ~$20/mese, limite 5h stimato $40 di costo API equivalente
    "max5":    200.0,   # Claude Max 5x ~$100/mese, limite 5h stimato $200
    "max20":   1000.0,  # Claude Max 20x ~$200/mese, limite 5h stimato $1000
}


def main():
    ap = argparse.ArgumentParser(description="Claude Code Usage Bridge")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=8787)
    ap.add_argument("--budget", type=float, default=500.0, help="Budget mensile USD (info mese)")
    ap.add_argument("--plan", choices=list(PLAN_PRESETS.keys()), default="max5",
                    help="Piano per stimare il limite 5h (pro/max5/max20)")
    ap.add_argument("--plan-limit", type=float, default=None,
                    help="Override limite finestra 5h in USD (es. --plan-limit 200)")
    ap.add_argument("--ttl", type=float, default=2.0, help="Cache TTL secondi")
    ap.add_argument("--token", default=None,
                    help="Bearer token esplicito (salta generazione/persistenza)")
    ap.add_argument("--no-auth", action="store_true",
                    help="Disabilita autenticazione (sconsigliato, stampa warning)")
    ap.add_argument("--metrics-anon", action="store_true",
                    help="Espone /metrics senza autenticazione (per scraper Prometheus)")
    ap.add_argument("--rescan-all", action="store_true",
                    help="Riscansiona TUTTI i transcript a ogni refresh, ignorando l'mtime "
                         "(default: salta i file troppo vecchi per contribuire). Usalo se "
                         "ripristini transcript da backup con mtime datato.")
    args = ap.parse_args()

    if not PROJECTS_DIR.exists():
        print(f"[warn] {PROJECTS_DIR} non esiste — non verranno trovati transcript")

    pricing = load_pricing()
    plan_limit = args.plan_limit if args.plan_limit is not None else PLAN_PRESETS[args.plan]
    agg = Aggregator(pricing, args.budget, plan_limit, rescan_all=args.rescan_all)
    cache = CachedAggregator(agg, ttl_seconds=args.ttl)

    require_auth = not args.no_auth
    token = load_or_create_token(args.token) if require_auth else ""
    handler_cls = make_handler(cache, token, require_auth, args.metrics_anon)
    server = ThreadingHTTPServer((args.host, args.port), handler_cls)
    ip = local_ip()
    print(f"Claude Code Usage Bridge avviato")
    print(f"  ascolta su:   http://{args.host}:{args.port}")
    print(f"  IP locale:    http://{ip}:{args.port}/usage")
    print(f"  budget mese:  {args.budget:.2f} USD")
    print(f"  limite 5h:    {plan_limit:.2f} USD ({args.plan})")
    print(f"  TTL cache:    {args.ttl:.1f} s")
    print(f"  projects dir: {PROJECTS_DIR}")
    print()
    if require_auth:
        short = (token[:4] + "..." + token[-4:]) if len(token) > 10 else token
        print(f"  auth:         bearer (token persistito in {TOKEN_PATH})")
        print(f"  token:        {token}")
        print(f"  short:        {short}")
        print()
        print(f"Su ESP32, in secrets.h (o nel captive portal) imposta:")
        print(f"    #define BRIDGE_HOST   \"{ip}\"")
        print(f"    #define BRIDGE_PORT   {args.port}")
        print(f"    #define BRIDGE_TOKEN  \"{token}\"")
        print()
        print(f"Test rapido:")
        print(f"    TOK={token}")
        print(f"    curl -H \"Authorization: Bearer $TOK\" http://{ip}:{args.port}/usage")
    else:
        print(f"  auth:         DISABILITATA (--no-auth)")
        print(f"  WARNING: chiunque sulla rete può leggere il tuo consumo Claude Code.")
        print(f"  Usa solo per debug locale, mai esposto a LAN/Internet.")
        print()
        print(f"Su ESP32, in secrets.h imposta:")
        print(f"    #define BRIDGE_HOST  \"{ip}\"")
        print(f"    #define BRIDGE_PORT  {args.port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nArrivederci.")


if __name__ == "__main__":
    main()
