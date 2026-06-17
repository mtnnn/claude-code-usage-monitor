# Contributing

Thanks for wanting to contribute! This is a hobby/community project with a small
codebase: everything must stay readable and pragmatic.

## Setup

### Bridge (Python)

Stdlib only, no venv or requirements. Python 3.10+:

```bash
python3 bridge/bridge.py --no-auth --port 8788   # local, for quick testing
```

### Firmware (PlatformIO)

```bash
pip install --user platformio
cd firmware
cp src/secrets.h.template src/secrets.h
# fill in WIFI_SSID/PASS/BRIDGE_HOST/BRIDGE_PORT/BRIDGE_TOKEN
pio run                      # build
pio run -t upload            # upload via USB-C
pio device monitor -b 115200 # serial
```

## Workflow

1. Fork, create a branch with an area prefix: `feat/portal-form`, `fix/null-deref`, `docs/quickstart`.
2. One commit per intent. Commit message in the format `area: short imperative`,
   e.g. `bridge: gate /metrics behind same token`.
3. Open a PR with a `Summary` + `Test plan` description (you can copy the template from
   previous examples).

## Style

- **Python**: PEP 8, no automatic formatters imposed (I prefer small diffs over
  mass re-indents). Type hints encouraged but not required.
- **C++ / Arduino**: existing style — `Lower_snake_case` functions for the
  module's public APIs (e.g. `WiFi_Connect_STA`), `lowerSnake` for locals. No
  heavy STL: Arduino's `String` and POSIX types are fine.
- No new libraries if we can avoid them. The ESP32 core builtins and the already
  loaded LVGL cover almost everything.

## What we gladly accept

- Bug fixes with steps to reproduce.
- Support for new boards (keeping the existing build intact).
- Leaner provisioning (e.g. BLE as an alternative to softAP).
- Localizations (the UI is in Italian today, English as soon as someone proposes it).
- Documentation: troubleshooting based on real experiences, screenshots.

## What we prefer to discuss first

Open a proposal issue first for:

- Adding external dependencies.
- Changes to the `/usage` protocol (they break existing firmware).
- Large rewrites (>500 LOC).

## Security guidelines

Never commit:

- `firmware/src/secrets.h` (it's already in `.gitignore`).
- Files containing real bearer tokens, API keys, or Wi-Fi passwords.
- Personal JSONL transcripts in tests.

To report vulnerabilities, follow [`SECURITY.md`](SECURITY.md) — do not open
public issues.
