#pragma once
#include <Arduino.h>

// Configuration persisted in NVS (Preferences). Every field has a
// fallback from the secrets.h defines when NVS is empty — so:
//   - source build with secrets.h filled in: keeps working (v0.1 compat)
//   - binary release with secrets.h empty: triggers the captive portal on first
//     boot, saves to NVS, and no recompile is needed anymore to change the config.
struct AppConfig {
  String   wifi_ssid;
  String   wifi_pass;
  String   bridge_host;
  uint16_t bridge_port;
  String   bridge_token;
  uint32_t poll_ms;
  bool     auto_rotate;
};

namespace Config {

// Reads from NVS. Missing keys fall back to the secrets.h defines.
void load();

// Persists the current state to NVS.
void save();

// Clears the NVS namespace and reboots the device.
void reset();

// True if SSID and bridge_host are both non-empty.
bool isProvisioned();

// Read-only snapshot of the current state.
const AppConfig& get();

// Granular setters (volatile memory; call save() to persist).
void setWifi(const String& ssid, const String& pass);
void setBridge(const String& host, uint16_t port, const String& token);
void setPoll(uint32_t poll_ms);
void setAutoRotate(bool on);

} // namespace Config
