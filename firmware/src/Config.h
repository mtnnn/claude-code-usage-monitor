#pragma once
#include <Arduino.h>

// Configurazione persistita in NVS (Preferences). Tutti i campi hanno un
// fallback dai define di secrets.h quando NVS è vuota — così:
//   - source build con secrets.h riempita: continua a funzionare (v0.1 compat)
//   - binary release con secrets.h vuota: triggera il captive portal al primo
//     boot, salva in NVS, e non serve più ricompilare per cambiare config.
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

// Legge da NVS. Le chiavi assenti cadono sui define di secrets.h.
void load();

// Persiste lo stato corrente su NVS.
void save();

// Cancella la namespace NVS e riavvia il device.
void reset();

// True se SSID e bridge_host sono entrambi non vuoti.
bool isProvisioned();

// Snapshot read-only dello stato corrente.
const AppConfig& get();

// Setters granulari (memoria volatile; chiama save() per persistere).
void setWifi(const String& ssid, const String& pass);
void setBridge(const String& host, uint16_t port, const String& token);
void setPoll(uint32_t poll_ms);
void setAutoRotate(bool on);

} // namespace Config
