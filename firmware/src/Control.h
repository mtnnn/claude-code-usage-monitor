#pragma once
#include <Arduino.h>

// Control endpoint in normal (STA) mode.
//
// Unlike the captive portal (Portal.*, active only in AP mode for the
// initial provisioning), Control runs on the local network after the board is
// connected to the WiFi and serves to re-point the bridge WITHOUT disconnecting from the WiFi and
// without rebooting: when the laptop's IP changes you just update host/port from
//   - a browser at http://claudemonitor.local/  (manual form), or
//   - the bridge itself announcing (automatic POST /config).
//
// Exposes, on port 80:
//   GET  /         HTML form (shows current host/port, device IP, hostname)
//   POST /config   updates host/port/token — requires the current bearer token
//
// The change is applied live via UsageClient_SetTarget() and persisted to
// NVS via Config::save(). The polling of /usage continues unchanged.
namespace Control {

// Register the routes and start the WebServer on port 80. To be called when the
// WiFi is connected (see main.cpp). Idempotent.
void start();

// To be called on every iteration of the main loop when the server is active.
void loop();

// Stop the server and free port 80 (necessary before entering portal
// mode, which re-binds the same port).
void stop();

bool isRunning();

} // namespace Control
