#include "UsageUI.h"
#include "UsageUI_theme.h"
#include "UsageUI_internal.h"
#include <Arduino.h>   // millis()

// Core UI: status bar, container dei pannelli, rotazione/navigazione tab,
// orchestrazione (Init) e dispatch di UsageUI_Update verso i moduli pannello.
// I 4 pannelli e gli overlay (splash/portal/toast) vivono in UsageUI_*.cpp.

// ----- Stato widget -----
static lv_obj_t* status_dot;
static lv_obj_t* status_label;
static lv_obj_t* status_ip;

static lv_obj_t* panels[4];

static uint8_t  active_panel = 0;
static bool     auto_rotate = true;
static uint32_t rotate_paused_until = 0;

// ----- Costruzione UI -----

static void make_status_bar(lv_obj_t* parent) {
  lv_obj_t* bar = lv_obj_create(parent);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, SCREEN_W, STATUS_H);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, COL_BG_STATUS, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  status_dot = lv_obj_create(bar);
  lv_obj_remove_style_all(status_dot);
  lv_obj_set_size(status_dot, 8, 8);
  lv_obj_set_pos(status_dot, 5, 5);
  lv_obj_set_style_radius(status_dot, 4, LV_PART_MAIN);
  lv_obj_set_style_bg_color(status_dot, COL_DANGER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(status_dot, LV_OPA_COVER, LV_PART_MAIN);

  status_label = lv_label_create(bar);
  lv_label_set_text(status_label, LV_SYMBOL_WIFI " ...");
  lv_obj_set_style_text_color(status_label, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(status_label, FONT_TINY, LV_PART_MAIN);
  lv_obj_set_pos(status_label, 18, 2);

  status_ip = lv_label_create(bar);
  lv_label_set_text(status_ip, "");
  lv_obj_set_style_text_color(status_ip, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(status_ip, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(status_ip, LV_ALIGN_RIGHT_MID, -3, 0);
}

static lv_obj_t* make_panel(lv_obj_t* parent, lv_color_t bg) {
  lv_obj_t* p = lv_obj_create(parent);
  lv_obj_remove_style_all(p);
  lv_obj_set_size(p, PANEL_W, PANEL_H);
  lv_obj_set_pos(p, 0, STATUS_H);
  lv_obj_set_style_bg_color(p, bg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(p, 6, LV_PART_MAIN);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
  return p;
}

// Helper one-shot per fade-out: nasconde il panel a fine animazione.
// NB: NON chiamare lv_timer_del nel callback: lv_timer_set_repeat_count(tm, 1)
// fa già auto-delete dopo la singola esecuzione. Doppio delete = UAF.
static void hide_after_fade(lv_timer_t* t) {
  lv_obj_t* obj = (lv_obj_t*)t->user_data;
  if (obj) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// Mostra solo il panel idx, nasconde gli altri con un fade discreto
static void show_panel(uint8_t idx) {
  uint8_t target = idx % 4;
  if (target == active_panel) return;
  uint8_t prev = active_panel;
  active_panel = target;

  lv_obj_clear_flag(panels[target], LV_OBJ_FLAG_HIDDEN);
  lv_obj_fade_in(panels[target], 200, 0);

  lv_obj_fade_out(panels[prev], 180, 0);
  lv_timer_t* tm = lv_timer_create(hide_after_fade, 200, panels[prev]);
  lv_timer_set_repeat_count(tm, 1);
}

static void rotate_timer_cb(lv_timer_t*) {
  if (!auto_rotate) return;
  if (rotate_paused_until && millis() < rotate_paused_until) return;
  // Non ruotare mentre splash o portal sono visibili
  if (ui_splash_blocking() || ui_portal_blocking()) return;
  show_panel(active_panel + 1);
}

void UsageUI_NextTab() {
  show_panel(active_panel + 1);
}

void UsageUI_PauseRotate(uint32_t ms) {
  rotate_paused_until = millis() + ms;
}

void UsageUI_SetAutoRotate(bool on) {
  auto_rotate = on;
}

// Helper usati dal modulo portal: nascondono/ripristinano i 4 pannelli
// (panels[]/active_panel restano stato del core).
void ui_hide_main_panels() {
  for (uint8_t i = 0; i < 4; i++) {
    if (panels[i]) lv_obj_add_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void ui_restore_active_panel() {
  if (active_panel < 4 && panels[active_panel]) {
    lv_obj_clear_flag(panels[active_panel], LV_OBJ_FLAG_HIDDEN);
  }
}

void UsageUI_Init() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  make_status_bar(scr);

  panels[0] = make_panel(scr, COL_BG);
  panels[1] = make_panel(scr, COL_BG);
  panels[2] = make_panel(scr, COL_BG);
  panels[3] = make_panel(scr, COL_BG);

  ui_cost_build(panels[0]);
  ui_window_build(panels[1]);
  ui_chart_build(panels[2]);
  ui_models_build(panels[3]);

  // primo panel visibile senza fade
  active_panel = 0;
  lv_obj_clear_flag(panels[0], LV_OBJ_FLAG_HIDDEN);
  for (uint8_t i = 1; i < 4; i++) lv_obj_add_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
  lv_timer_create(rotate_timer_cb, ROTATE_INTERVAL_MS, nullptr);
}

void UsageUI_SetIp(const char* ip) {
  if (status_ip && ip) lv_label_set_text(status_ip, ip);
}

void UsageUI_Update(const UsageData& d) {
  // --- Status bar ---
  if (d.online) {
    lv_obj_set_style_bg_color(status_dot, COL_ACCENT_COST, LV_PART_MAIN);
    lv_label_set_text(status_label, LV_SYMBOL_WIFI " ONLINE");
  } else {
    lv_obj_set_style_bg_color(status_dot, COL_DANGER, LV_PART_MAIN);
    lv_label_set_text(status_label, LV_SYMBOL_WIFI " OFFLINE");
  }

  // --- Costo ---
  ui_cost_update(d);

  // --- Finestra 5h ---
  ui_window_update(d);

  // --- Grafico 7 giorni ---
  ui_chart_update(d);

  // --- Modelli ---
  ui_models_update(d);
}
