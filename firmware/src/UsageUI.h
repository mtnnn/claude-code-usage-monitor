#pragma once
#include <lvgl.h>
#include "UsageClient.h"

// Builds the LVGL interface on the active screen. Must be called after Lvgl_Init().
void UsageUI_Init();

// Updates the widgets with a snapshot. Should be called frequently (e.g. every 500 ms).
// Safe to call from the main loop where lv_timer_handler() runs.
void UsageUI_Update(const UsageData& d);

// Updates the IP text in the status bar.
void UsageUI_SetIp(const char* ip);

// ===== Boot splash (overlay above the main UI) =====
// UsageUI_Splash creates the overlay. UsageUI_SplashSetState updates the status line
// at the bottom ("Connecting WiFi...", "Reading bridge..."). UsageUI_DismissSplash
// fades out and deletes the overlay; idempotent.
void UsageUI_Splash();
void UsageUI_SplashSetState(const char* line);
void UsageUI_DismissSplash();
bool UsageUI_SplashVisible();

// ===== Captive portal panel =====
// Shows a full-screen panel with "Setup Mode", AP name, WPA2 password
// and URL. The main UI stays in the background (the 4 panels are hidden).
// ap_pass == nullptr/"" => the password line is not shown.
void UsageUI_ShowPortal(const char* ap_name, const char* ap_ip, const char* ap_pass = nullptr);
void UsageUI_HidePortal();

// ===== Manual navigation (BOOT button, M4) =====
// Switches to the next tab (0→1→2→3→0).
void UsageUI_NextTab();

// Pauses auto-rotation for `ms` milliseconds.
void UsageUI_PauseRotate(uint32_t ms);

// Persistently enables/disables auto-rotation.
void UsageUI_SetAutoRotate(bool on);

// Toast overlay at the bottom (1.5s).
void UsageUI_Toast(const char* msg);
