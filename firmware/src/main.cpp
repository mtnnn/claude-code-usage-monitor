/******************************************************************************
 *  Claude Code Usage Monitor — firmware Waveshare ESP32-S3-LCD-1.47
 *
 *  Si connette al WiFi locale, fa polling di un bridge Python sul PC
 *  (vedi ../bridge/) ogni N secondi e mostra in tempo reale costo + token
 *  + finestra 5h + grafico 7 giorni + breakdown per modello su display
 *  ST7789 172x320.
 *
 *  v0.2: captive portal di provisioning runtime (no recompile per cambiare
 *  WiFi/bridge), bearer-token auth, polish grafico, pulsante BOOT (M4).
 ******************************************************************************/
#include <Arduino.h>
#include "Display_ST7789.h"
#include "LVGL_Driver.h"
#include "RGB_lamp.h"
#include "Wireless.h"
#include "UsageClient.h"
#include "UsageUI.h"
#include "Config.h"
#include "Portal.h"
#include "Button.h"

static uint32_t last_ui_update_ms = 0;
static uint32_t last_ip_refresh_ms = 0;
static uint32_t last_rgb_count = 0;
static uint32_t fetch_pulse_until = 0;
static uint32_t splash_until_ms = 0;
static bool     splash_dismissed = false;
static bool     in_portal_mode = false;

static void rgb_set_status(bool online, bool pulse) {
  if (pulse) {
    Set_Color(60, 0, 120);   // viola: fetch appena riuscito
  } else if (online) {
    Set_Color(0, 20, 0);     // verde tenue
  } else {
    Set_Color(30, 0, 0);     // rosso tenue
  }
}

static void enter_portal_mode() {
  in_portal_mode = true;
  Portal::start();
  UsageUI_DismissSplash();        // se splash ancora attiva
  splash_dismissed = true;
  UsageUI_ShowPortal(Portal::apName().c_str(), Portal::apIp().c_str());
  Set_Color(0, 0, 60);            // blu = setup mode
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== Claude Code Usage Monitor v0.2.0 ===");

  LCD_Init();
  Set_Backlight(80);

  Lvgl_Init();
  UsageUI_Init();
  UsageUI_Splash();
  UsageUI_SplashSetState("Lettura config...");
  splash_until_ms = millis() + 2500;

  Config::load();
  Button::begin();
  UsageUI_SetAutoRotate(Config::get().auto_rotate);

  // Nessuna config persistita né da secrets.h → portal mode.
  if (!Config::isProvisioned()) {
    Serial.println("[boot] config non valida — entro in portal mode");
    enter_portal_mode();
    return;
  }

  const AppConfig& c = Config::get();
  UsageUI_SplashSetState("Connessione WiFi...");
  WiFi_Connect_STA(c.wifi_ssid.c_str(), c.wifi_pass.c_str());

  UsageUI_SplashSetState("Lettura bridge...");
  UsageClient_Begin(c.bridge_host.c_str(), c.bridge_port,
                    c.poll_ms, c.bridge_token.c_str());

  Set_Color(0, 0, 30);  // blu tenue durante boot
}

void loop() {
  Timer_Loop();

  // Pulsante BOOT: gestione sempre attiva, anche in portal mode
  // (VERY_LONG da portal è meno utile ma non fa danno).
  Button::Event ev = Button::poll();
  if (ev == Button::VERY_LONG) {
    UsageUI_Toast("Reset NVS...");
    delay(400);
    Config::reset();  // erase + ESP.restart()
    // unreachable
  }

  if (in_portal_mode) {
    Portal::loop();
    return;
  }

  // Gesti tab/rotate validi solo fuori dal portal
  if (ev == Button::TAP) {
    UsageUI_NextTab();
    UsageUI_PauseRotate(30000);
  } else if (ev == Button::LONG) {
    bool now_on = !Config::get().auto_rotate;
    Config::setAutoRotate(now_on);
    Config::save();
    UsageUI_SetAutoRotate(now_on);
    UsageUI_Toast(now_on ? "Rotazione: ON" : "Rotazione: OFF");
  }

  uint32_t now = millis();

  // Fail-safe: se WiFi non si aggancia per troppo tempo, entra in portal
  // (succede tipicamente per credenziali sbagliate o router spento).
  if (WiFi_HasFailedPersistently()) {
    Serial.println("[loop] WiFi failed persistently — entro in portal mode");
    enter_portal_mode();
    return;
  }

  // Aggiorna IP nella status bar ogni 2 secondi
  if (now - last_ip_refresh_ms > 2000) {
    last_ip_refresh_ms = now;
    UsageUI_SetIp(WiFi_LocalIP().c_str());
  }

  // Aggiorna UI con snapshot ogni 400 ms
  if (now - last_ui_update_ms > 400) {
    last_ui_update_ms = now;
    UsageData d;
    UsageClient_Snapshot(d);
    UsageUI_Update(d);

    // Dismiss splash al primo fetch riuscito o dopo timeout
    if (!splash_dismissed) {
      bool first_data = (d.fetch_count > 0);
      if (first_data || now >= splash_until_ms) {
        UsageUI_DismissSplash();
        splash_dismissed = true;
      }
    }

    // RGB feedback: pulse 300ms ad ogni nuovo fetch
    if (d.fetch_count != last_rgb_count) {
      last_rgb_count = d.fetch_count;
      fetch_pulse_until = now + 300;
    }
    rgb_set_status(d.online, now < fetch_pulse_until);
  }

  delay(5);
}
