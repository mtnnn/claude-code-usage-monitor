# Firmware — Claude Code Usage Monitor (PlatformIO)

Firmware for the **LilyGo T-Display-S3** board that displays, in real time,
the Claude Code usage read from the Python bridge `../bridge/`. (Ported from the
upstream Waveshare ESP32-S3-LCD-1.47, which drives the same ST7789 over SPI.)

## What it displays

170×320 display with 4 views that rotate automatically every 6 seconds
(you can pause the rotation with the BOOT button, see below):

1. **Cost** — `$ today` (large, green) + "yesterday $X" + 7-day sparkline + `$ month`.
2. **5h window** — `% of limit` (green/amber/red), `$cost / $limit`,
   two labelled bars **Time** (purple, 0-300 min) and **Limit** (0-100 %),
   ETA-to-limit if relevant, reset countdown.
3. **Last 7 days** — daily cost bar chart.
4. **Models (month)** — top 5 models with cost and proportional bar.

At the top there is always a status bar with a WiFi icon + ONLINE/OFFLINE + IP. The onboard RGB LED
(GPIO 38) blinks purple on every successful fetch, stays green when
online, red when offline, blue in setup mode.

At startup a **splash** (`Claude Code Usage v0.2.0`) stays visible for ~2 s.

## Setup

You have **two paths**: binary release (zero compile-time config) or source build
(put everything in `secrets.h`).

### Path A: binary release (captive portal)

1. `pio run -t upload` with an **empty** `secrets.h` (a copy of the template).
2. On first boot, the board opens an AP `ClaudeMonitor-XXYY` (XXYY = last 4
   hex digits of the MAC) and shows the URL and network name on screen.
3. Connect your phone to the AP → the captive popup opens, or navigate
   manually to `http://192.168.4.1`.
4. Fill in the form (WiFi, bridge IP/port/token, polling) and press
   **Save and connect**. The board reboots and enters normal mode.
5. Config persisted in NVS; you no longer need to recompile to change WiFi/bridge.

### Path B: source build

```bash
cp src/secrets.h.template src/secrets.h
$EDITOR src/secrets.h   # fill in WIFI_SSID, WIFI_PASS, BRIDGE_HOST, BRIDGE_TOKEN
pio run -t upload
pio device monitor      # 115200 baud
```

The values in `secrets.h` act as a **fallback** when NVS is empty — so the
source build path stays backward-compatible with v0.1.

## BOOT button (GPIO 0)

| Gesture | Duration | Effect |
|---|---|---|
| Tap | <500 ms | Next tab; automatic rotation paused for 30 s |
| Long press | 500 ms – 5 s | Toggle automatic rotation (persisted in NVS) |
| Very long | >5 s | Reset NVS + reboot into captive portal |

Polling-only (no interrupt): GPIO 0 is also the download-mode strap pin, so
interrupts here are dangerous.

## Captive portal triggers

| Trigger | Effect |
|---|---|
| NVS empty **and** `secrets.h` empty at boot | Automatic portal mode |
| Hold BOOT >5 s | Wipe NVS + reboot into portal |
| WiFi STA fails 3× in 30 s | Drop into portal mode |

> Holding BOOT *during reset* does not work: the chip enters the ROM bootloader,
> not the firmware. Use the very-long-press at runtime.

## Build & flash

```bash
pio run                  # build only
pio run -t upload        # build + upload (auto-detect port)
pio device monitor       # 115200 baud
```

If the upload fails: hold down **BOOT** + press **RESET** + release **RESET**
+ release **BOOT** to force the bootloader, then try again.

## PlatformIO structure

```
firmware/
├── platformio.ini             # board, PSRAM, lib_deps (lvgl, ArduinoJson)
├── .gitignore                 # ignores .pio/ and src/secrets.h
└── src/
    ├── main.cpp               # boot flow + button polling
    ├── secrets.h.template     # fallback config (leave empty for portal)
    ├── secrets.h              # created by the user, do NOT commit
    ├── lv_conf.h              # LVGL 8.3 config (Montserrat 22/28/32)
    ├── Display_ST7789.{cpp,h} # SPI + ST7789 driver
    ├── LVGL_Driver.{cpp,h}    # LVGL init + flush callback
    ├── RGB_lamp.{cpp,h}       # NeoPixel GPIO 38 driver
    ├── Wireless.{cpp,h}       # WiFi STA + auto-reconnect + fail counter
    ├── UsageClient.{cpp,h}    # GET /usage + Authorization header + ArduinoJson
    ├── UsageUI.{cpp,h}        # 4 panels, splash, portal, toast, fade tabs
    ├── Config.{cpp,h}         # NVS config (Preferences) + secrets.h fallback
    ├── Portal.{cpp,h}         # softAP + DNSServer + WebServer form (provisioning)
    ├── Control.{cpp,h}        # mDNS + WebServer STA: re-point the bridge on the fly
    └── Button.{cpp,h}         # BOOT button polled state machine
```

## Changing the bridge IP (without the captive portal)

When the laptop's IP changes you no longer need to re-enter the setup AP. As soon as it
connects to WiFi the board announces itself as **`claudemonitor.local`** and exposes a
small control endpoint on port 80. The pull of `/usage` continues identically;
only how you communicate the bridge's new address changes. Two ways:

- **Automatic (recommended)** — start `bridge.py` (announcement active by default):
  it resolves `claudemonitor.local` and `POST`s its own IP to the device. Changed network?
  Restart (or keep running) the bridge and the board re-points itself within
  `--announce-interval` seconds. See `../bridge/README.md`.
- **Manual** — open **`http://claudemonitor.local/`** (or the IP shown in the
  status bar) in the browser, enter the bridge host/port + the current token,
  press *Update bridge*. It applies on the fly, no reboot.

The endpoint requires the current bearer token (header `Authorization: Bearer` or
form field): a LAN host that does not know it receives `401` and cannot re-point
the board. The change is persisted in NVS, so it survives a reboot.

> The **first** provisioning (WiFi credentials) still goes through the captive portal —
> you cannot deliver the Wi-Fi credentials over a network you are not yet
> connected to. The re-pointing above is for every subsequent IP change.

## Serial debug

Typical output at boot in normal mode:

```
=== Claude Code Usage Monitor v0.2.0 ===
[Config] SSID="MioWifi" host="192.168.1.55" port=8787 poll=5000 rotate=1 token=(set)
[WiFi] begin SSID="MioWifi"
[WiFi] connected, IP=192.168.1.78 RSSI=-52 dBm
[UsageClient] polling http://192.168.1.55:8787/usage every 5000 ms (auth ON)
[mDNS] claudemonitor.local active
[Control] server active on http://192.168.1.78/ (claudemonitor.local)
```

In captive portal:

```
[boot] invalid config — entering portal mode
[Portal] AP=ClaudeMonitor-A1B2 IP=192.168.4.1 — open http://192.168.4.1
```

The bridge token is never printed (only `(set)` / `(empty)`).

## Technical notes

- **arduino-esp32 2.0.x compat**: `Backlight_Init` uses `ledcSetup`/`ledcAttachPin`
  with a `#if ESP_ARDUINO_VERSION_MAJOR >= 3` guard to also support 3.x.
- **`PIN_NEOPIXEL` collision**: the ESP32-S3 DevKitM-1 core defines
  `PIN_NEOPIXEL=48`; on the Waveshare the LED is on 38. The macro has been renamed
  to `RGB_LED_PIN`.
- **NVS keys**: `Preferences` 15-char limit → we use `br_host`, `br_port`,
  `br_token`, `rotate` instead of descriptive names.
- **Task layout**: HTTP polling on a FreeRTOS task on core 0; LVGL on the main loop
  on core 1; access to shared data via `xSemaphoreCreateMutex()`.
- **DNS captive trap**: the `DNSServer` on `*` routes every hostname to
  `192.168.4.1`, and `WebServer.onNotFound` does a 302 to `/`. It works with the probes
  of iOS (captive.apple.com), Android (generate_204), and Windows (connecttest.txt).
- **Memory** (v0.2): ~1.22 MB flash, ~109 KB RAM. Still ~80 % of flash free.

## Hardware pin summary

Display ST7789 SPI: MOSI=45 SCLK=40 CS=42 DC=41 RST=39 BL=48 (PWM)
RGB LED: GPIO 38
BOOT button: GPIO 0 (with internal pull-up)
