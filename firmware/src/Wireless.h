#pragma once
#include <Arduino.h>
#include <WiFi.h>

extern volatile bool wifi_connected;

// Avvia la connessione STA. Non bloccante: la riconnessione avviene
// automaticamente via WiFiEvent. wifi_connected riflette lo stato corrente.
void WiFi_Connect_STA(const char* ssid, const char* pass);

// IP locale come stringa (es. "192.168.1.42") oppure "0.0.0.0" se non connesso
String WiFi_LocalIP();

// Reset contatori interni (chiamare quando si abbandona STA per portal).
void WiFi_ResetFailCount();

// True se abbiamo accumulato troppi disconnect senza un successo recente —
// il caller può decidere di rientrare in captive portal.
bool WiFi_HasFailedPersistently();
