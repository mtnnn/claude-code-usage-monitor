#include "Config.h"
#include <Preferences.h>

#if __has_include("secrets.h")
  #include "secrets.h"
#endif

#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS ""
#endif
#ifndef BRIDGE_HOST
  #define BRIDGE_HOST ""
#endif
#ifndef BRIDGE_PORT
  #define BRIDGE_PORT 8787
#endif
#ifndef BRIDGE_TOKEN
  #define BRIDGE_TOKEN ""
#endif
#ifndef POLL_INTERVAL_MS
  #define POLL_INTERVAL_MS 5000
#endif

// Le chiavi NVS sono limitate a 15 char (limite Preferences/nvs_open).
static const char* NS         = "cc-monitor";
static const char* K_SSID     = "wifi_ssid";
static const char* K_PASS     = "wifi_pass";
static const char* K_HOST     = "br_host";
static const char* K_PORT     = "br_port";
static const char* K_TOKEN    = "br_token";
static const char* K_POLL     = "poll_ms";
static const char* K_ROTATE   = "rotate";

static AppConfig g;

void Config::load() {
  Preferences p;
  if (p.begin(NS, /*readonly=*/true)) {
    g.wifi_ssid    = p.getString(K_SSID,   WIFI_SSID);
    g.wifi_pass    = p.getString(K_PASS,   WIFI_PASS);
    g.bridge_host  = p.getString(K_HOST,   BRIDGE_HOST);
    g.bridge_port  = p.getUShort(K_PORT,   BRIDGE_PORT);
    g.bridge_token = p.getString(K_TOKEN,  BRIDGE_TOKEN);
    g.poll_ms      = p.getULong (K_POLL,   POLL_INTERVAL_MS);
    g.auto_rotate  = p.getBool  (K_ROTATE, true);
    p.end();
  } else {
    // Namespace NVS non ancora creato: usa i fallback da secrets.h senza
    // interrogare un handle Preferences chiuso (prima ci si affidava al fatto
    // che la libreria ritorni il default su handle non aperto).
    g.wifi_ssid    = WIFI_SSID;
    g.wifi_pass    = WIFI_PASS;
    g.bridge_host  = BRIDGE_HOST;
    g.bridge_port  = BRIDGE_PORT;
    g.bridge_token = BRIDGE_TOKEN;
    g.poll_ms      = POLL_INTERVAL_MS;
    g.auto_rotate  = true;
  }

  Serial.printf("[Config] SSID=\"%s\" host=\"%s\" port=%u poll=%u rotate=%d token=%s\n",
                g.wifi_ssid.c_str(), g.bridge_host.c_str(),
                (unsigned)g.bridge_port, (unsigned)g.poll_ms,
                (int)g.auto_rotate,
                g.bridge_token.length() ? "(set)" : "(empty)");
}

void Config::save() {
  Preferences p;
  if (!p.begin(NS, /*readonly=*/false)) {
    Serial.println("[Config] save: NVS open failed");
    return;
  }
  p.putString(K_SSID,   g.wifi_ssid);
  p.putString(K_PASS,   g.wifi_pass);
  p.putString(K_HOST,   g.bridge_host);
  p.putUShort(K_PORT,   g.bridge_port);
  p.putString(K_TOKEN,  g.bridge_token);
  p.putULong (K_POLL,   g.poll_ms);
  p.putBool  (K_ROTATE, g.auto_rotate);
  p.end();
  Serial.println("[Config] saved");
}

void Config::reset() {
  Preferences p;
  if (p.begin(NS, false)) {
    p.clear();
    p.end();
  }
  Serial.println("[Config] NVS cleared, rebooting");
  delay(200);
  ESP.restart();
}

bool Config::isProvisioned() {
  return g.wifi_ssid.length() > 0 && g.bridge_host.length() > 0;
}

const AppConfig& Config::get() { return g; }

void Config::setWifi(const String& ssid, const String& pass) {
  g.wifi_ssid = ssid;
  g.wifi_pass = pass;
}

void Config::setBridge(const String& host, uint16_t port, const String& token) {
  g.bridge_host  = host;
  g.bridge_port  = port;
  g.bridge_token = token;
}

void Config::setPoll(uint32_t poll_ms) {
  if (poll_ms < 1000)  poll_ms = 1000;
  if (poll_ms > 60000) poll_ms = 60000;
  g.poll_ms = poll_ms;
}

void Config::setAutoRotate(bool on) {
  g.auto_rotate = on;
}
