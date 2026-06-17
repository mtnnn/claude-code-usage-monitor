#pragma once
#include <Arduino.h>
#include <WiFi.h>

extern volatile bool wifi_connected;

// Start the STA connection. Non-blocking: reconnection happens
// automatically via WiFiEvent. wifi_connected reflects the current state.
void WiFi_Connect_STA(const char* ssid, const char* pass);

// Local IP as a string (e.g. "192.168.1.42") or "0.0.0.0" if not connected
String WiFi_LocalIP();

// Reset internal counters (call when leaving STA for the portal).
void WiFi_ResetFailCount();

// True if we have accumulated too many disconnects without a recent success —
// the caller can decide to re-enter the captive portal.
bool WiFi_HasFailedPersistently();
