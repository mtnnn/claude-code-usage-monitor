#pragma once
#include <Arduino.h>

// Captive portal di provisioning runtime.
//
// Quando partiamo senza WiFi/bridge configurati (o l'utente forza il reset),
// la board diventa un AP "ClaudeMonitor-XXYY" con un form web a 192.168.4.1
// per inserire SSID/password WiFi + host/port/token del bridge.
namespace Portal {

// Avvia softAP + DNSServer (catch-all → 192.168.4.1) + WebServer su porta 80.
// L'AP è aperto (no password). Dopo Portal::start chiama Portal::loop() ad
// ogni iterazione del main loop per servire DNS e HTTP.
void start();

// Da chiamare nel main loop quando isRunning() == true.
void loop();

bool isRunning();

// Nome dell'AP (ClaudeMonitor-AABB) — utile per la UI.
String apName();

// IP dell'AP — sempre "192.168.4.1" finché il portal è attivo.
String apIp();

} // namespace Portal
