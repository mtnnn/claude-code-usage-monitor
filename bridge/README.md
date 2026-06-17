# Claude Code Usage Bridge

Local HTTP server (Python 3.10+, zero dependencies) that reads the JSONL transcripts of
Claude Code in `~/.claude/projects/` and exposes them as JSON for the
ESP32-S3-LCD-1.47 firmware (`../firmware`).

## Startup

```bash
python3 bridge.py
# start with a custom port/budget:
python3 bridge.py --port 9000 --budget 200
```

Expected output (v0.2+):

```
Claude Code Usage Bridge started
  listening on: http://0.0.0.0:8787
  local IP:     http://192.168.x.x:8787/usage
  month budget: 500.00 USD
  5h limit:     200.00 USD (max5)
  ...
  auth:         bearer (token persisted in /home/you/.claude-code-usage/token)
  token:        aBcDeFgHiJ...xyz
  short:        aBcD...8XYz

On the ESP32, in secrets.h (or in the captive portal) set:
    #define BRIDGE_HOST   "192.168.x.x"
    #define BRIDGE_PORT   8787
    #define BRIDGE_TOKEN  "aBcDeFgHiJ...xyz"
```

## Authentication (v0.2+)

From v0.2 onward the bridge requires a **bearer token** on `/usage` and `/metrics`:

- On first startup it is generated and saved in `~/.claude-code-usage/token`
  (permissions `0600`, directory `0700`).
- Subsequent runs load the same token.
- You can force an explicit token with `--token <value>` (e.g. for environments where
  you want control yourself).
- You can disable auth with `--no-auth` (not recommended: it prints a warning, and
  anyone on the LAN can read your usage).
- `/health` always stays anonymous (useful as a liveness probe).

## Verification

```bash
# Current token
TOK=$(cat ~/.claude-code-usage/token)

# Test endpoints
curl -H "Authorization: Bearer $TOK" http://localhost:8787/usage | python3 -m json.tool
curl http://localhost:8787/health
curl -H "Authorization: Bearer $TOK" http://localhost:8787/metrics
```

## Automatic device re-pointing (announcement)

The device always pulls `/usage` — but when the laptop's IP changes,
instead of re-entering the captive portal to re-enter it, the **bridge** is the one to
re-point the device. By default (`--announce` active) the bridge:

1. resolves **`claudemonitor.local`** (Bonjour on macOS, avahi on Linux; fallback
   `--device-ip <ip>` if mDNS is not available);
2. `POST`s its own IP to the device's control endpoint (port 80),
   authenticating with the same bearer token;
3. repeats every `--announce-interval` seconds, recomputing the local IP — so a
   network change mid-session is propagated on its own.

```bash
python3 bridge.py                      # announcement active by default
python3 bridge.py --no-announce        # disable the announcement
python3 bridge.py --device-ip 192.168.1.78   # fallback if .local does not resolve
python3 bridge.py --announce-once      # announce once and exit (test)
```

Stdlib only (`urllib` + `socket.getaddrinfo`): no added dependencies. Alternatively,
you can re-point manually from `http://claudemonitor.local/` in the browser.

## `/metrics` endpoint (Prometheus)

Exposes in Prometheus 0.0.4 text format:

```
cc_cost_today_usd       <float>
cc_cost_month_usd       <float>
cc_window5h_pct         <int>
cc_window5h_cost_usd    <float>
cc_messages_total       <int>
```

For anonymous scraping from Prometheus/Grafana: start the bridge with `--metrics-anon`
(in this case only `/metrics` stays anonymous; `/usage` still requires a token).

## CLI flags

| Flag | Default | Description |
|---|---|---|
| `--host` | `0.0.0.0` | Bind address |
| `--port` | `8787` | TCP port |
| `--budget` | `500.0` | Monthly budget USD (informational) |
| `--plan` | `max5` | 5h limit preset: `pro`/`max5`/`max20` |
| `--plan-limit` | (preset) | Explicit override of the 5h limit USD |
| `--ttl` | `2.0` | Cache TTL seconds |
| `--token` | (auto) | Explicit token (skip persistence) |
| `--no-auth` | off | Disable auth (not recommended) |
| `--metrics-anon` | off | Expose `/metrics` without a token |
| `--rescan-all` | off | Re-reads all transcripts every time (see note below) |
| `--announce` / `--no-announce` | on | Announce the bridge's IP to the device via `claudemonitor.local` |
| `--device-name` | `claudemonitor.local` | mDNS hostname of the device to re-point |
| `--device-ip` | (auto) | Device IP as a fallback if mDNS does not resolve |
| `--device-control-port` | `80` | Port of the control endpoint on the device |
| `--announce-interval` | `30.0` | Seconds between one announcement and the next |
| `--announce-once` | off | Announce once and terminate (useful for testing) |

## `/usage` response schema

```json
{
  "ts": "2026-05-16T15:23:10+02:00",
  "today":  {"cost_usd": 12.34, "tokens_in": 1234567, "tokens_out": 56789, "cache_read": 9876543},
  "month":  {"cost_usd": 234.56, "tokens_in": ..., "tokens_out": ..., "cache_read": ...},
  "last7":  [{"date": "2026-05-10", "cost_usd": 3.21}, ...],
  "by_model": [
    {"name": "claude-opus-4-7", "cost_usd": 150.20, "tokens_in": ..., "tokens_out": ...},
    ...
  ],
  "budget_monthly_usd": 500,
  "window5h": {
    "active": true,
    "messages": 49,
    "cost_usd": 10.62,
    "tokens_in": 123456,
    "tokens_out": 5678,
    "elapsed_min": 87,
    "remaining_min": 213,
    "limit_usd": 200.0,
    "limit_pct": 5,
    "start": "2026-05-18T13:15:00+02:00",
    "reset_at": "2026-05-18T18:15:00+02:00"
  }
}
```

`last7` always has 7 entries (today is the last one); `by_model` is sorted by descending cost.

## Customizing the prices

Edit `pricing.json` (USD per 1 million tokens). Models are matched by
prefix, so `claude-opus-4-7-20251201` falls back on `claude-opus-4-7`. The
`_default` key is used for unknown models.

## Automatic startup (Linux, systemd user)

```ini
# ~/.config/systemd/user/claude-bridge.service
[Unit]
Description=Claude Code Usage Bridge

[Service]
ExecStart=/usr/bin/python3 %h/Desktop/claude\x20code\x20usage/bridge/bridge.py
Restart=on-failure

[Install]
WantedBy=default.target
```

```bash
systemctl --user enable --now claude-bridge.service
```

## Notes

- The JSONL files are read read-only on every request (with a 2s TTL cache in RAM so as not to
  re-read thousands of lines on every poll from the ESP32).
- To avoid rescanning the whole history on every refresh, files whose `mtime` is
  too old to contribute to a view (month / 7 days / 5h window)
  are skipped. If you restore transcripts from a backup with a dated `mtime` but
  recent content, start with `--rescan-all` to force a full scan.
- It works even if Claude Code is not running: the transcripts stay on disk.
- No sensitive prompt data is exposed — only cost/token aggregates.
