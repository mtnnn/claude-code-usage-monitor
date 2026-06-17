#pragma once
#include <Arduino.h>

// Runtime provisioning captive portal.
//
// When we start without WiFi/bridge configured (or the user forces a reset),
// the board becomes an AP "ClaudeMonitor-XXYY" with a web form at 192.168.4.1
// to enter the WiFi SSID/password + the bridge host/port/token.
namespace Portal {

// Start softAP (WPA2, random per-session password shown on the display)
// + DNSServer (catch-all → 192.168.4.1) + WebServer on port 80. After
// Portal::start call Portal::loop() on every iteration of the main loop to
// serve DNS and HTTP.
void start();

// To be called in the main loop when isRunning() == true.
void loop();

bool isRunning();

// Name of the AP (ClaudeMonitor-AABB) — useful for the UI.
String apName();

// IP of the AP — always "192.168.4.1" while the portal is active.
String apIp();

// WPA2 password of the AP (random, generated in start()) — to be shown on the display.
String apPassword();

} // namespace Portal
