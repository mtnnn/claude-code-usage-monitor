# Changelog

All notable changes to this project are documented here. Format based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versioning [SemVer](https://semver.org/lang/it/).

## [Unreleased]

### Security
- Captive portal: the setup AP is now **WPA2**-protected with a random
  per-session password shown on the display (previously it was open). The
  bridge bearer token is no longer repopulated in the web form; if left empty
  on save, the existing one is preserved. A portal entered due to a WiFi
  failure restarts after a timeout instead of remaining an indefinitely
  running AP.
- Bridge: the token file is written atomically with `0600` permissions right
  from creation (no window in which it is readable by other local users).
- Bridge: removed `Access-Control-Allow-Origin: *` from authenticated endpoints.

### Added
- Re-pointing the bridge without the captive portal. When the laptop's IP
  changes you no longer need to re-enter the setup AP:
  - **Firmware**: the device advertises itself via mDNS as `claudemonitor.local`
    and exposes, in normal mode (STA), a control endpoint on port 80
    (`GET /` web form, `POST /config`) that updates the bridge host/port/token
    **on the fly** (no reboot, persisted in NVS). Polling of `/usage`
    stays unchanged. The endpoint requires the current bearer token.
  - **Bridge**: by default it announces itself to the device (`--announce`),
    resolving `claudemonitor.local` (Bonjour on macOS / avahi on Linux,
    fallback `--device-ip`) and `POST`-ing its own IP to the control endpoint.
    Network changed? Just restart (or keep running) the bridge. New flags:
    `--no-announce`, `--device-name`, `--device-ip`, `--device-control-port`,
    `--announce-interval`, `--announce-once`. Still stdlib only (urllib).
- Bridge: `--rescan-all` flag to force a rescan of all transcripts
  (useful if you restore files from a backup with an old `mtime`).

### Changed
- Bridge: by default, transcripts with an `mtime` too old to contribute to
  a view are skipped, avoiding re-reading the entire history on every
  refresh (`--rescan-all` to disable).
- README: the 5h window is now described as a **cost-based estimate** (the
  real Anthropic limit is opaque/usage-based); `SECURITY.md` now also covers
  the captive portal threat model.

### Fixed
- Bridge: the filesystem scan is no longer performed while holding the cache
  lock — concurrent `/usage` and `/metrics` requests no longer serialize
  for the entire duration of the scan (single-flight, no thundering herd).
- Bridge: `/usage` routing with exact match and tolerant of the query string
  (previously `startswith` also captured paths like `/usagexyz`).
- Firmware: `UsageClient` rejects `/usage` responses declared much larger
  than expected (Content-Length guard, defense-in-depth).
- Firmware: `Config::load` no longer uses the `Preferences` handle after `end()`
  when `begin()` fails.

### Internal
- Bridge: first unit test suite (stdlib `unittest`, 29 tests) run in
  CI; the 5h window logic extracted into a pure function; coverage for
  mDNS resolution (`resolve_device`) and device announcement (`announce_once`).
- Firmware: `UsageUI.cpp` (~850 lines) split into modules by responsibility
  (theme, formatting, 4 panels, splash/portal/toast); the core drops to ~170 lines.
  Behavior unchanged.
- CI: GitHub Actions updated to versions compatible with Node 24
  (checkout v5, setup-python v6, cache v5).

## [0.2.0] — 2026-05-18

### Added — Security
- Bridge: bearer token required on `/usage` and `/metrics` (`hmac.compare_digest`,
  constant-time). Generated in `~/.claude-code-usage/token` on first run
  with `0600` permissions. Flags `--token`, `--no-auth`, `--metrics-anon`.
- The `/health` endpoint stays anonymous for liveness probes.
- The `/metrics` endpoint in Prometheus 0.0.4 format (cost today/month, window5h %, messages).
- `SECURITY.md` with threat model and disclosure contact.
- Firmware: sends `Authorization: Bearer ...` automatically when `BRIDGE_TOKEN`
  is set; explicit log on HTTP 401; the token is never printed to Serial.

### Added — UX
- Runtime provisioning captive portal on the ESP32: on first boot (or by
  holding BOOT >5s) the device creates an AP `ClaudeMonitor-XXYY` with a web
  form for WiFi, bridge host/port, token, and poll cadence. Config persisted in NVS.
- Boot splash with logo, title, version.
- Unified color palette with semantic accents (green cost, cyan token,
  amber warning, red danger, purple time).
- LV_SYMBOL icons on every tab header.
- "5h window" tab: now with two labeled bars — `Time` (0-300 min, purple)
  and `Limit` (0-100%, green→amber→red). Resolved the visual ambiguity of v0.1.
- 7-day sparkline at the bottom right of the Cost tab.
- "yesterday $X.YZ" label below the TODAY figure.
- Fade transitions between tabs.
- ETA-to-limit on the 5h window tab: extrapolation of the moment you'll run out
  of budget at the current rate (amber >30 min, red <30 min).
- BOOT button (GPIO 0) for manual navigation:
  - **Tap** (<500 ms): next tab, auto-rotation paused 30s.
  - **Long press** (1–5 s): toggle auto-rotation (persistent).
  - **Very long** (>5 s): reset NVS + reboot into captive portal.

### Added — Repo
- `CONTRIBUTING.md` with guidelines.
- `CHANGELOG.md` (this file).
- Issue templates (`.github/ISSUE_TEMPLATE/`).
- GitHub Actions workflow (`.github/workflows/build.yml`): py_compile for the
  bridge, `pio run` for the firmware on every push/PR.

### Changed
- `secrets.h.template`: added `BRIDGE_TOKEN`. Empty values are treated
  as fallbacks for the captive portal — source-build users can still
  fill everything in at compile-time.
- README.md: new "Setup (binary release)" section as the main path,
  "Build from source" remains for those who want to compile.

### Migration v0.1 → v0.2
- Users who already have `secrets.h` filled in: add `#define BRIDGE_TOKEN ""`
  (or the value printed by the bridge), then `pio run -t upload`. Everything keeps working.
- Bridge users: the first `curl` after the upgrade will return `401`. Add
  `-H "Authorization: Bearer $(cat ~/.claude-code-usage/token)"`, or start with
  `--no-auth` for legacy behavior (warning displayed).

## [0.1.0] — 2026-05-16

### Added
- Python stdlib-only bridge that reads `~/.claude/projects/**/*.jsonl` and exposes
  `/usage` with `today`, `month`, `last7`, `by_model`, `window5h` aggregates.
- Deduplication by `message.id` (resolves ~3× cost inflation from resume/compact).
- 5h window anchored on user timestamps (aligns with the Anthropic dashboard).
- Plan presets `pro` / `max5` / `max20`, override with `--plan-limit`.
- Pricing override via `pricing.json`.
- Waveshare ESP32-S3-LCD-1.47 firmware with LVGL 8.3, 4 auto-rotating tabs,
  WiFi STA with auto-reconnect, 5s HTTP polling, RGB status LED.
