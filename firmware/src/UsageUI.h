#pragma once
#include <lvgl.h>
#include "UsageClient.h"

// Costruisce l'interfaccia LVGL sulla schermata attiva. Va chiamata dopo Lvgl_Init().
void UsageUI_Init();

// Aggiorna i widget con uno snapshot. Va chiamata frequentemente (ad es. ogni 500 ms).
// Sicura da chiamare dal main loop dove gira lv_timer_handler().
void UsageUI_Update(const UsageData& d);

// Aggiorna il testo IP nella status bar.
void UsageUI_SetIp(const char* ip);

// ===== Splash di boot (overlay sopra l'UI principale) =====
// UsageUI_Splash crea l'overlay. UsageUI_SplashSetState aggiorna la riga di stato
// in basso ("Connessione WiFi...", "Lettura bridge..."). UsageUI_DismissSplash
// fa fade-out e cancella l'overlay; idempotente.
void UsageUI_Splash();
void UsageUI_SplashSetState(const char* line);
void UsageUI_DismissSplash();
bool UsageUI_SplashVisible();

// ===== Captive portal panel =====
// Mostra un pannello full-screen con "Modalità Setup", AP name, password WPA2
// e URL. L'UI principale resta in background (i 4 panel sono nascosti).
// ap_pass == nullptr/"" => la riga password non viene mostrata.
void UsageUI_ShowPortal(const char* ap_name, const char* ap_ip, const char* ap_pass = nullptr);
void UsageUI_HidePortal();

// ===== Navigazione manuale (pulsante BOOT, M4) =====
// Passa al tab successivo (0→1→2→3→0).
void UsageUI_NextTab();

// Mette in pausa la rotazione automatica per `ms` millisecondi.
void UsageUI_PauseRotate(uint32_t ms);

// Abilita/disabilita persistentemente la rotazione automatica.
void UsageUI_SetAutoRotate(bool on);

// Toast overlay in basso (1.5s).
void UsageUI_Toast(const char* msg);
