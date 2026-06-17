/******************************************************************************
 *  Claude Code Usage Monitor — firmware LilyGo T-Display-S3
 *
 *  Connects to the local WiFi, polls a Python bridge on the PC
 *  (see ../bridge/) every N seconds and shows in real time cost + token
 *  + 5h window + 7-day chart + per-model breakdown on an
 *  ST7789 170x320 display.
 *
 *  v0.2: runtime provisioning captive portal (no recompile to change
 *  WiFi/bridge), bearer-token auth, chart polish, BOOT button (M4).
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
#include "Control.h"
#include "Button.h"
#include <ESPmDNS.h>

static uint32_t last_ui_update_ms = 0;
static uint32_t last_ip_refresh_ms = 0;
static uint32_t last_rgb_count = 0;
static uint32_t fetch_pulse_until = 0;
static uint32_t splash_until_ms = 0;
static bool     splash_dismissed = false;
static bool     in_portal_mode = false;
static uint32_t portal_entered_ms = 0;
static bool     portal_can_timeout = false;
static const uint32_t PORTAL_TIMEOUT_MS = 5 * 60 * 1000;  // 5 min
static bool     net_services_up = false;  // mDNS + Control server (STA mode)

// Start/stop the network services that require the WiFi to be connected: mDNS
// (claudemonitor.local) and the control server to re-point the bridge.
static void start_net_services() {
  if (net_services_up) return;
  if (MDNS.begin("claudemonitor")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] claudemonitor.local active");
  } else {
    Serial.println("[mDNS] begin failed");
  }
  Control::start();
  net_services_up = true;
}

static void stop_net_services() {
  if (!net_services_up) return;
  Control::stop();
  MDNS.end();
  net_services_up = false;
}

static void rgb_set_status(bool online, bool pulse) {
  if (pulse) {
    Set_Color(60, 0, 120);   // purple: fetch just succeeded
  } else if (online) {
    Set_Color(0, 20, 0);     // dim green
  } else {
    Set_Color(30, 0, 0);     // dim red
  }
}

static void enter_portal_mode() {
  in_portal_mode = true;
  // Free port 80 (Control uses it in STA mode) before Portal re-binds it.
  stop_net_services();
  // If we already had a valid config we are here because of a WiFi failure:
  // we can reboot after a timeout to retry. On first boot (config
  // absent) there is nothing to retry, so we stay in the portal.
  portal_can_timeout = Config::isProvisioned();
  portal_entered_ms = millis();
  Portal::start();
  UsageUI_DismissSplash();        // if splash still active
  splash_dismissed = true;
  UsageUI_ShowPortal(Portal::apName().c_str(), Portal::apIp().c_str(),
                     Portal::apPassword().c_str());
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
  UsageUI_SplashSetState("Reading config...");
  splash_until_ms = millis() + 2500;

  Config::load();
  Button::begin();
  UsageUI_SetAutoRotate(Config::get().auto_rotate);

  // No config persisted nor from secrets.h → portal mode.
  if (!Config::isProvisioned()) {
    Serial.println("[boot] config invalid — entering portal mode");
    enter_portal_mode();
    return;
  }

  const AppConfig& c = Config::get();
  UsageUI_SplashSetState("Connecting WiFi...");
  WiFi_Connect_STA(c.wifi_ssid.c_str(), c.wifi_pass.c_str());

  UsageUI_SplashSetState("Reading bridge...");
  UsageClient_Begin(c.bridge_host.c_str(), c.bridge_port,
                    c.poll_ms, c.bridge_token.c_str());

  Set_Color(0, 0, 30);  // dim blue during boot
}

void loop() {
  Timer_Loop();

  // BOOT button: handling always active, even in portal mode
  // (VERY_LONG from the portal is less useful but does no harm).
  Button::Event ev = Button::poll();
  if (ev == Button::VERY_LONG) {
    UsageUI_Toast("Resetting NVS...");
    delay(400);
    Config::reset();  // erase + ESP.restart()
    // unreachable
  }

  if (in_portal_mode) {
    Portal::loop();
    // If we are in the portal because of a WiFi failure (config already valid),
    // after a timeout we reboot to retry: avoids staying an AP indefinitely
    // if the router becomes available again in the meantime.
    if (portal_can_timeout && (millis() - portal_entered_ms > PORTAL_TIMEOUT_MS)) {
      Serial.println("[portal] timeout — reboot to retry the WiFi");
      delay(50);
      ESP.restart();
    }
    return;
  }

  // Tab/rotate gestures valid only outside the portal
  if (ev == Button::TAP) {
    UsageUI_NextTab();
    UsageUI_PauseRotate(30000);
  } else if (ev == Button::LONG) {
    bool now_on = !Config::get().auto_rotate;
    Config::setAutoRotate(now_on);
    Config::save();
    UsageUI_SetAutoRotate(now_on);
    UsageUI_Toast(now_on ? "Rotate: ON" : "Rotate: OFF");
  }

  uint32_t now = millis();

  // Fail-safe: if WiFi does not latch on for too long, enter the portal
  // (typically happens with wrong credentials or a powered-off router).
  if (WiFi_HasFailedPersistently()) {
    Serial.println("[loop] WiFi failed persistently — entering portal mode");
    enter_portal_mode();
    return;
  }

  // Network services (mDNS + Control): up when the WiFi latches on, down when
  // it drops — so claudemonitor.local is re-announced on every reconnection.
  if (wifi_connected) {
    start_net_services();
  } else if (net_services_up) {
    stop_net_services();
  }
  Control::loop();

  // Update IP in the status bar every 2 seconds
  if (now - last_ip_refresh_ms > 2000) {
    last_ip_refresh_ms = now;
    UsageUI_SetIp(WiFi_LocalIP().c_str());
  }

  // Update UI with snapshot every 400 ms
  if (now - last_ui_update_ms > 400) {
    last_ui_update_ms = now;
    UsageData d;
    UsageClient_Snapshot(d);
    UsageUI_Update(d);

    // Dismiss splash on the first successful fetch or after timeout
    if (!splash_dismissed) {
      bool first_data = (d.fetch_count > 0);
      if (first_data || now >= splash_until_ms) {
        UsageUI_DismissSplash();
        splash_dismissed = true;
      }
    }

    // RGB feedback: 300ms pulse on every new fetch
    if (d.fetch_count != last_rgb_count) {
      last_rgb_count = d.fetch_count;
      fetch_pulse_until = now + 300;
    }
    rgb_set_status(d.online, now < fetch_pulse_until);
  }

  delay(5);
}
