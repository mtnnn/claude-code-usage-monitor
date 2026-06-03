#include "UsageUI.h"
#include "UsageUI_theme.h"
#include "UsageUI_format.h"
#include "UsageUI_internal.h"
#include "Display_ST7789.h"
#include <stdio.h>
#include <string.h>

// ----- Stato widget -----
static lv_obj_t* status_dot;
static lv_obj_t* status_label;
static lv_obj_t* status_ip;

static lv_obj_t* panels[4];

// Tab Finestra 5h
static lv_obj_t* win_msg_lbl;
static lv_obj_t* win_cost_lbl;
static lv_obj_t* win_tokens_lbl;
static lv_obj_t* win_time_bar;       // barra Tempo (viola, 0..300 min)
static lv_obj_t* win_time_val_lbl;
static lv_obj_t* win_limit_bar;      // barra Limite (verde/ambra/rosso, 0..100%)
static lv_obj_t* win_limit_val_lbl;
static lv_obj_t* win_eta_lbl;        // popolata in M4 (ETA-to-limit)
static lv_obj_t* win_reset_lbl;

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

// Tab 1: FINESTRA 5h (Claude Code rate-limit window)
static void build_window_panel(lv_obj_t* p) {
  lv_obj_t* hdr = lv_label_create(p);
  lv_label_set_text(hdr, LV_SYMBOL_REFRESH " FINESTRA 5h");
  lv_obj_set_style_text_color(hdr, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 4);

  // % limite in grande
  win_msg_lbl = lv_label_create(p);
  lv_label_set_text(win_msg_lbl, "0%");
  lv_obj_set_style_text_color(win_msg_lbl, COL_ACCENT_TOK, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_msg_lbl, FONT_HUGE, LV_PART_MAIN);
  lv_obj_align(win_msg_lbl, LV_ALIGN_TOP_MID, 0, 22);

  lv_obj_t* mh = lv_label_create(p);
  lv_label_set_text(mh, "del limite 5h");
  lv_obj_set_style_text_color(mh, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(mh, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(mh, LV_ALIGN_TOP_MID, 0, 62);

  // Costo finestra
  win_cost_lbl = lv_label_create(p);
  lv_label_set_text(win_cost_lbl, "$0.00");
  lv_obj_set_style_text_color(win_cost_lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_cost_lbl, FONT_BIG, LV_PART_MAIN);
  lv_obj_align(win_cost_lbl, LV_ALIGN_TOP_MID, 0, 80);

  // Messaggi + token out compatti
  win_tokens_lbl = lv_label_create(p);
  lv_label_set_text(win_tokens_lbl, "");
  lv_obj_set_style_text_color(win_tokens_lbl, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_tokens_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(win_tokens_lbl, LV_ALIGN_TOP_MID, 0, 112);

  // --- Barra Tempo (viola, 0-300 min) ---
  lv_obj_t* tempo_lbl = lv_label_create(p);
  lv_label_set_text(tempo_lbl, "Tempo");
  lv_obj_set_style_text_color(tempo_lbl, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(tempo_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(tempo_lbl, LV_ALIGN_TOP_LEFT, 6, 132);

  win_time_val_lbl = lv_label_create(p);
  lv_label_set_text(win_time_val_lbl, "0h 00m");
  lv_obj_set_style_text_color(win_time_val_lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_time_val_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(win_time_val_lbl, LV_ALIGN_TOP_RIGHT, -6, 132);

  win_time_bar = lv_bar_create(p);
  lv_obj_set_size(win_time_bar, PANEL_W - 24, 10);
  lv_obj_set_pos(win_time_bar, 12, 148);
  lv_bar_set_range(win_time_bar, 0, 300);
  lv_bar_set_value(win_time_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(win_time_bar, COL_BAR_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_color(win_time_bar, COL_TIME, LV_PART_INDICATOR);

  // --- Barra Limite (verde→ambra→rosso, 0-100%) ---
  lv_obj_t* lim_lbl = lv_label_create(p);
  lv_label_set_text(lim_lbl, "Limite");
  lv_obj_set_style_text_color(lim_lbl, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(lim_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(lim_lbl, LV_ALIGN_TOP_LEFT, 6, 168);

  win_limit_val_lbl = lv_label_create(p);
  lv_label_set_text(win_limit_val_lbl, "0%");
  lv_obj_set_style_text_color(win_limit_val_lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_limit_val_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(win_limit_val_lbl, LV_ALIGN_TOP_RIGHT, -6, 168);

  win_limit_bar = lv_bar_create(p);
  lv_obj_set_size(win_limit_bar, PANEL_W - 24, 10);
  lv_obj_set_pos(win_limit_bar, 12, 184);
  lv_bar_set_range(win_limit_bar, 0, 100);
  lv_bar_set_value(win_limit_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(win_limit_bar, COL_BAR_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_color(win_limit_bar, COL_ACCENT_COST, LV_PART_INDICATOR);

  // ETA-to-limit (popolato in M4)
  win_eta_lbl = lv_label_create(p);
  lv_label_set_text(win_eta_lbl, "");
  lv_obj_set_style_text_color(win_eta_lbl, COL_WARN, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_eta_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(win_eta_lbl, LV_ALIGN_BOTTOM_MID, 0, -20);

  // Countdown reset
  win_reset_lbl = lv_label_create(p);
  lv_label_set_text(win_reset_lbl, "nessuna finestra attiva");
  lv_obj_set_style_text_color(win_reset_lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_reset_lbl, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(win_reset_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);
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
  build_window_panel(panels[1]);
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
  char buf[40];
  if (d.win_active) {
    // % limite in grande
    uint16_t lp = d.win_limit_pct;
    if (lp > 999) lp = 999;
    lv_label_set_text_fmt(win_msg_lbl, "%u%%", (unsigned)lp);

    // colore numero grande + barra limite: verde / ambra / rosso
    lv_color_t lpcol = COL_ACCENT_COST;
    if (lp >= 90)      lpcol = COL_DANGER;
    else if (lp >= 70) lpcol = COL_WARN;
    lv_obj_set_style_text_color(win_msg_lbl, lpcol, LV_PART_MAIN);
    lv_obj_set_style_bg_color(win_limit_bar, lpcol, LV_PART_INDICATOR);

    // costo + tetto piano
    if (d.win_limit_usd > 0.001f) {
      lv_label_set_text_fmt(win_cost_lbl, "$%.2f / $%.0f",
                            d.win_cost_usd, d.win_limit_usd);
    } else {
      ui_fmt_money(buf, sizeof(buf), d.win_cost_usd);
      lv_label_set_text(win_cost_lbl, buf);
    }

    // messaggi assistant + token compatti
    char wo[16];
    ui_fmt_tokens(wo, sizeof(wo), d.win_tokens_out);
    lv_label_set_text_fmt(win_tokens_lbl, "%u msg  |  out %s",
                          (unsigned)d.win_messages, wo);

    // Barra Tempo (0..300 min)
    uint16_t em = d.win_elapsed_min > 300 ? 300 : d.win_elapsed_min;
    lv_bar_set_value(win_time_bar, em, LV_ANIM_OFF);
    lv_label_set_text_fmt(win_time_val_lbl, "%uh %02um",
                          (unsigned)(em / 60), (unsigned)(em % 60));

    // Barra Limite (0..100%, capped)
    int lim_pct_cap = (lp > 100) ? 100 : (int)lp;
    lv_bar_set_value(win_limit_bar, lim_pct_cap, LV_ANIM_OFF);
    lv_label_set_text_fmt(win_limit_val_lbl, "%u%%", (unsigned)lp);

    // Countdown reset
    uint16_t r = d.win_remaining_min;
    if (r == 0) {
      lv_label_set_text(win_reset_lbl, "reset in corso...");
    } else if (r < 60) {
      lv_label_set_text_fmt(win_reset_lbl, "reset tra %um", (unsigned)r);
    } else {
      lv_label_set_text_fmt(win_reset_lbl, "reset tra %uh %02um",
                            (unsigned)(r / 60), (unsigned)(r % 60));
    }

    // ETA-to-limit: minuti residui al raggiungimento del limite al burn rate corrente.
    // Mostra solo se il rate è non-zero e l'ETA è dentro la finestra residua —
    // altrimenti il limite non sarà mai raggiunto in questa finestra.
    float eta_min = -1.0f;
    if (d.win_elapsed_min > 0 && d.win_limit_usd > 0.001f
        && d.win_cost_usd < d.win_limit_usd) {
      float per_min = d.win_cost_usd / (float)d.win_elapsed_min;
      if (per_min > 1e-6f) {
        eta_min = (d.win_limit_usd - d.win_cost_usd) / per_min;
      }
    }
    if (eta_min > 0.0f && eta_min < (float)d.win_remaining_min) {
      int em = (int)eta_min;
      lv_obj_set_style_text_color(win_eta_lbl,
                                  em < 30 ? COL_DANGER : COL_WARN,
                                  LV_PART_MAIN);
      if (em < 60) {
        lv_label_set_text_fmt(win_eta_lbl, "ETA limite: %dm", em);
      } else {
        lv_label_set_text_fmt(win_eta_lbl, "ETA limite: %dh %02dm",
                              em / 60, em % 60);
      }
    } else {
      lv_label_set_text(win_eta_lbl, "");
    }
  } else {
    lv_label_set_text(win_msg_lbl, "0%");
    lv_obj_set_style_text_color(win_msg_lbl, COL_FG_MUTED, LV_PART_MAIN);
    lv_label_set_text(win_cost_lbl, "$0.00");
    lv_label_set_text(win_tokens_lbl, "");
    lv_bar_set_value(win_time_bar, 0, LV_ANIM_OFF);
    lv_bar_set_value(win_limit_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(win_time_val_lbl, "0h 00m");
    lv_label_set_text(win_limit_val_lbl, "0%");
    lv_label_set_text(win_eta_lbl, "");
    lv_label_set_text(win_reset_lbl, "nessuna finestra attiva");
  }

  // --- Grafico 7 giorni ---
  ui_chart_update(d);

  // --- Modelli ---
  ui_models_update(d);
}
