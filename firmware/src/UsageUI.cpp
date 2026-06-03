#include "UsageUI.h"
#include "Display_ST7789.h"
#include <stdio.h>
#include <string.h>

// ===== v0.2 palette =====
// Una sola fonte di verità per i colori; il codice sotto usa solo questi macro.
#define COL_BG           lv_color_hex(0x0b0d10)
#define COL_BG_PANEL     lv_color_hex(0x14181d)
#define COL_BG_STATUS    lv_color_hex(0x101820)
#define COL_BG_CARD      lv_color_hex(0x1a1a1a)
#define COL_FG           lv_color_hex(0xe6e6e6)
#define COL_FG_DIM       lv_color_hex(0x8a8f96)
#define COL_FG_MUTED     lv_color_hex(0x666666)
#define COL_DIVIDER      lv_color_hex(0x444444)
#define COL_BAR_BG       lv_color_hex(0x4c5159)
#define COL_ACCENT_COST  lv_color_hex(0x35d399)   // verde — costo/money
#define COL_ACCENT_TOK   lv_color_hex(0x38bdf8)   // ciano — token
#define COL_WARN         lv_color_hex(0xfbbf24)   // ambra — 70%+ / ETA medio
#define COL_DANGER       lv_color_hex(0xef4444)   // rosso — 90%+ / ETA <30
#define COL_TIME         lv_color_hex(0xa78bfa)   // viola — barra tempo

// ----- Font: usa quelli opzionali se abilitati in lv_conf.h, altrimenti fallback -----
#if LV_FONT_MONTSERRAT_32
  #define FONT_HUGE   &lv_font_montserrat_32
#elif LV_FONT_MONTSERRAT_28
  #define FONT_HUGE   &lv_font_montserrat_28
#elif LV_FONT_MONTSERRAT_24
  #define FONT_HUGE   &lv_font_montserrat_24
#else
  #define FONT_HUGE   &lv_font_montserrat_16
#endif

#if LV_FONT_MONTSERRAT_22
  #define FONT_BIG    &lv_font_montserrat_22
#elif LV_FONT_MONTSERRAT_20
  #define FONT_BIG    &lv_font_montserrat_20
#elif LV_FONT_MONTSERRAT_18
  #define FONT_BIG    &lv_font_montserrat_18
#else
  #define FONT_BIG    &lv_font_montserrat_16
#endif

#define FONT_MED    &lv_font_montserrat_16
#define FONT_SMALL  &lv_font_montserrat_14
#define FONT_TINY   &lv_font_montserrat_12

#define STATUS_H 18
#define SCREEN_W LCD_WIDTH    // 172
#define SCREEN_H LCD_HEIGHT   // 320
#define PANEL_W  SCREEN_W
#define PANEL_H  (SCREEN_H - STATUS_H)

#define ROTATE_INTERVAL_MS 6000

// ----- Stato widget -----
static lv_obj_t* status_dot;
static lv_obj_t* status_label;
static lv_obj_t* status_ip;

static lv_obj_t* panels[4];

// Splash (boot overlay)
static lv_obj_t* splash_root = nullptr;
static lv_obj_t* splash_state_lbl = nullptr;

// Portal (setup mode overlay)
static lv_obj_t* portal_root = nullptr;

// Tab Costo
static lv_obj_t* today_cost_lbl;
static lv_obj_t* month_cost_lbl;
static lv_obj_t* yesterday_lbl;
static lv_obj_t* spark_chart;
static lv_chart_series_t* spark_series;
static lv_obj_t* updated_lbl;

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

// Tab Grafico
static lv_obj_t* chart;
static lv_chart_series_t* chart_series;
static lv_obj_t* chart_day_lbls[7];
static lv_obj_t* chart_max_lbl;

// Tab Modelli
#define MODEL_ROWS 5
static lv_obj_t* model_row[MODEL_ROWS];
static lv_obj_t* model_name_lbl[MODEL_ROWS];
static lv_obj_t* model_cost_lbl[MODEL_ROWS];
static lv_obj_t* model_bar[MODEL_ROWS];
static lv_obj_t* model_empty_lbl;

static uint8_t  active_panel = 0;
static bool     auto_rotate = true;
static uint32_t rotate_paused_until = 0;

// ----- Helper di formattazione -----
static void fmt_money(char* dst, size_t cap, float v) {
  if (v >= 1000.0f) snprintf(dst, cap, "$%.0f", v);
  else              snprintf(dst, cap, "$%.2f", v);
}

static void fmt_tokens(char* dst, size_t cap, uint64_t v) {
  if (v < 1000ULL)              snprintf(dst, cap, "%llu", (unsigned long long)v);
  else if (v < 1000000ULL)      snprintf(dst, cap, "%.1fK", v / 1000.0);
  else if (v < 1000000000ULL)   snprintf(dst, cap, "%.2fM", v / 1000000.0);
  else                          snprintf(dst, cap, "%.2fB", v / 1000000000.0);
}

// estrae sigla modello: "claude-opus-4-7" -> "Opus 4.7"
static void short_model_name(const char* full, char* dst, size_t cap) {
  if (!full || !*full) { snprintf(dst, cap, "?"); return; }
  // cerca opus/sonnet/haiku
  const char* fam = nullptr;
  if (strstr(full, "opus"))   fam = "Opus";
  else if (strstr(full, "sonnet")) fam = "Sonnet";
  else if (strstr(full, "haiku"))  fam = "Haiku";
  else { snprintf(dst, cap, "%s", full); return; }

  // estrai prima coppia di numeri "4-7" -> "4.7"
  int maj = 0, min = -1;
  const char* p = full;
  while (*p) {
    if (*p >= '0' && *p <= '9') {
      maj = atoi(p);
      while (*p >= '0' && *p <= '9') p++;
      if (*p == '-' && p[1] >= '0' && p[1] <= '9') {
        min = atoi(p + 1);
      }
      break;
    }
    p++;
  }
  if (min >= 0) snprintf(dst, cap, "%s %d.%d", fam, maj, min);
  else if (maj > 0) snprintf(dst, cap, "%s %d", fam, maj);
  else snprintf(dst, cap, "%s", fam);
}

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

// Tab 0: COSTO
static void build_cost_panel(lv_obj_t* p) {
  lv_obj_t* hdr = lv_label_create(p);
  lv_label_set_text(hdr, "$ OGGI");
  lv_obj_set_style_text_color(hdr, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 4);

  today_cost_lbl = lv_label_create(p);
  lv_label_set_text(today_cost_lbl, "$0.00");
  lv_obj_set_style_text_color(today_cost_lbl, COL_ACCENT_COST, LV_PART_MAIN);
  lv_obj_set_style_text_font(today_cost_lbl, FONT_HUGE, LV_PART_MAIN);
  lv_obj_align(today_cost_lbl, LV_ALIGN_TOP_MID, 0, 26);

  yesterday_lbl = lv_label_create(p);
  lv_label_set_text(yesterday_lbl, "");
  lv_obj_set_style_text_color(yesterday_lbl, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(yesterday_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(yesterday_lbl, LV_ALIGN_TOP_MID, 0, 72);

  lv_obj_t* sep = lv_obj_create(p);
  lv_obj_remove_style_all(sep);
  lv_obj_set_size(sep, 120, 1);
  lv_obj_set_style_bg_color(sep, COL_DIVIDER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(sep, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t* mh = lv_label_create(p);
  lv_label_set_text(mh, "MESE");
  lv_obj_set_style_text_color(mh, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(mh, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(mh, LV_ALIGN_CENTER, 0, 20);

  month_cost_lbl = lv_label_create(p);
  lv_label_set_text(month_cost_lbl, "$0.00");
  lv_obj_set_style_text_color(month_cost_lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(month_cost_lbl, FONT_BIG, LV_PART_MAIN);
  lv_obj_align(month_cost_lbl, LV_ALIGN_CENTER, 0, 50);

  // Sparkline 7 giorni in basso a destra (trend visivo)
  spark_chart = lv_chart_create(p);
  lv_obj_set_size(spark_chart, 64, 26);
  lv_obj_align(spark_chart, LV_ALIGN_BOTTOM_RIGHT, -4, -22);
  lv_chart_set_type(spark_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(spark_chart, 7);
  lv_chart_set_div_line_count(spark_chart, 0, 0);
  lv_obj_set_style_bg_opa(spark_chart, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(spark_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_size(spark_chart, 0, LV_PART_INDICATOR);     // no point markers
  lv_obj_set_style_line_width(spark_chart, 2, LV_PART_ITEMS);
  spark_series = lv_chart_add_series(spark_chart, COL_ACCENT_COST, LV_CHART_AXIS_PRIMARY_Y);

  updated_lbl = lv_label_create(p);
  lv_label_set_text(updated_lbl, "");
  lv_obj_set_style_text_color(updated_lbl, COL_FG_MUTED, LV_PART_MAIN);
  lv_obj_set_style_text_font(updated_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(updated_lbl, LV_ALIGN_BOTTOM_LEFT, 4, -4);
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

// Tab 2: GRAFICO 7 GIORNI
static void build_chart_panel(lv_obj_t* p) {
  lv_obj_t* hdr = lv_label_create(p);
  lv_label_set_text(hdr, LV_SYMBOL_LIST " ULTIMI 7 GIORNI");
  lv_obj_set_style_text_color(hdr, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 4);

  chart_max_lbl = lv_label_create(p);
  lv_label_set_text(chart_max_lbl, "max $0");
  lv_obj_set_style_text_color(chart_max_lbl, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(chart_max_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(chart_max_lbl, LV_ALIGN_TOP_RIGHT, -4, 22);

  chart = lv_chart_create(p);
  lv_obj_set_size(chart, PANEL_W - 16, PANEL_H - 70);
  lv_obj_set_pos(chart, 8, 40);
  lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
  lv_chart_set_point_count(chart, 7);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(chart, 4, 0);
  lv_obj_set_style_bg_color(chart, COL_BG_PANEL, LV_PART_MAIN);
  lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
  chart_series = lv_chart_add_series(chart, COL_ACCENT_COST, LV_CHART_AXIS_PRIMARY_Y);

  // Labels giorno sotto le barre
  int chart_left = 8;
  int chart_w    = PANEL_W - 16;
  int chart_bottom = 40 + (PANEL_H - 70);
  int slot_w   = chart_w / 7;
  for (int i = 0; i < 7; i++) {
    chart_day_lbls[i] = lv_label_create(p);
    lv_label_set_text(chart_day_lbls[i], "--");
    lv_obj_set_style_text_color(chart_day_lbls[i], COL_FG_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(chart_day_lbls[i], FONT_TINY, LV_PART_MAIN);
    lv_obj_set_pos(chart_day_lbls[i], chart_left + slot_w * i + slot_w/2 - 8, chart_bottom + 2);
  }
}

// Tab 3: MODELLI
static void build_models_panel(lv_obj_t* p) {
  lv_obj_t* hdr = lv_label_create(p);
  lv_label_set_text(hdr, LV_SYMBOL_OK " MODELLI (MESE)");
  lv_obj_set_style_text_color(hdr, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 4);

  int row_h = 44;
  int top_y = 26;
  for (int i = 0; i < MODEL_ROWS; i++) {
    model_row[i] = lv_obj_create(p);
    lv_obj_remove_style_all(model_row[i]);
    lv_obj_set_size(model_row[i], PANEL_W - 12, row_h);
    lv_obj_set_pos(model_row[i], 6, top_y + i * (row_h + 4));
    lv_obj_set_style_bg_color(model_row[i], COL_BG_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(model_row[i], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(model_row[i], 4, LV_PART_MAIN);
    lv_obj_clear_flag(model_row[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(model_row[i], LV_OBJ_FLAG_HIDDEN);

    model_name_lbl[i] = lv_label_create(model_row[i]);
    lv_label_set_text(model_name_lbl[i], "");
    lv_obj_set_style_text_color(model_name_lbl[i], COL_FG, LV_PART_MAIN);
    lv_obj_set_style_text_font(model_name_lbl[i], FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_pos(model_name_lbl[i], 6, 4);

    model_cost_lbl[i] = lv_label_create(model_row[i]);
    lv_label_set_text(model_cost_lbl[i], "");
    lv_obj_set_style_text_color(model_cost_lbl[i], COL_ACCENT_COST, LV_PART_MAIN);
    lv_obj_set_style_text_font(model_cost_lbl[i], FONT_SMALL, LV_PART_MAIN);
    lv_obj_align(model_cost_lbl[i], LV_ALIGN_TOP_RIGHT, -6, 4);

    model_bar[i] = lv_bar_create(model_row[i]);
    lv_obj_set_size(model_bar[i], PANEL_W - 32, 6);
    lv_obj_set_pos(model_bar[i], 6, 28);
    lv_bar_set_range(model_bar[i], 0, 100);
    lv_bar_set_value(model_bar[i], 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(model_bar[i], COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(model_bar[i], COL_ACCENT_TOK, LV_PART_INDICATOR);
  }

  model_empty_lbl = lv_label_create(p);
  lv_label_set_text(model_empty_lbl, "Nessun dato");
  lv_obj_set_style_text_color(model_empty_lbl, COL_FG_MUTED, LV_PART_MAIN);
  lv_obj_set_style_text_font(model_empty_lbl, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(model_empty_lbl, LV_ALIGN_CENTER, 0, 0);
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
  if (splash_root || portal_root) return;
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

// One-shot: auto-delete del timer è gestito da lv_timer_set_repeat_count(tm, 1).
static void toast_del_cb(lv_timer_t* t) {
  lv_obj_t* obj = (lv_obj_t*)t->user_data;
  if (obj) lv_obj_del(obj);
}

void UsageUI_Toast(const char* msg) {
  if (!msg) return;
  lv_obj_t* scr = lv_scr_act();
  lv_obj_t* t = lv_obj_create(scr);
  lv_obj_remove_style_all(t);
  lv_obj_set_style_bg_color(t, COL_BG_CARD, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(t, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_all(t, 8, LV_PART_MAIN);
  lv_obj_t* lbl = lv_label_create(t);
  lv_label_set_text(lbl, msg);
  lv_obj_set_style_text_color(lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl, FONT_SMALL, LV_PART_MAIN);
  // Auto-size: aspettiamo il layout prima di centrare
  lv_obj_update_layout(t);
  lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -30);

  lv_obj_fade_in(t, 150, 0);
  lv_timer_t* tm = lv_timer_create(toast_del_cb, 1500, t);
  lv_timer_set_repeat_count(tm, 1);
}

// --- API splash (overlay sopra l'UI principale) ---
void UsageUI_Splash() {
  if (splash_root) return;
  lv_obj_t* scr = lv_scr_act();
  splash_root = lv_obj_create(scr);
  lv_obj_remove_style_all(splash_root);
  lv_obj_set_size(splash_root, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(splash_root, 0, 0);
  lv_obj_set_style_bg_color(splash_root, COL_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(splash_root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(splash_root, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* logo = lv_label_create(splash_root);
  lv_label_set_text(logo, "$");
  lv_obj_set_style_text_color(logo, COL_ACCENT_COST, LV_PART_MAIN);
  lv_obj_set_style_text_font(logo, FONT_HUGE, LV_PART_MAIN);
  lv_obj_align(logo, LV_ALIGN_CENTER, 0, -60);

  lv_obj_t* title = lv_label_create(splash_root);
  lv_label_set_text(title, "Claude Code");
  lv_obj_set_style_text_color(title, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(title, FONT_BIG, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -12);

  lv_obj_t* sub = lv_label_create(splash_root);
  lv_label_set_text(sub, "Usage Monitor");
  lv_obj_set_style_text_color(sub, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(sub, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, 12);

  lv_obj_t* ver = lv_label_create(splash_root);
  lv_label_set_text(ver, "v0.2.0");
  lv_obj_set_style_text_color(ver, COL_FG_MUTED, LV_PART_MAIN);
  lv_obj_set_style_text_font(ver, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(ver, LV_ALIGN_CENTER, 0, 38);

  splash_state_lbl = lv_label_create(splash_root);
  lv_label_set_text(splash_state_lbl, "");
  lv_obj_set_style_text_color(splash_state_lbl, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(splash_state_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(splash_state_lbl, LV_ALIGN_BOTTOM_MID, 0, -20);
}

void UsageUI_SplashSetState(const char* line) {
  if (splash_state_lbl && line) lv_label_set_text(splash_state_lbl, line);
}

// One-shot: auto-delete del timer è gestito da lv_timer_set_repeat_count(tm, 1).
static void splash_del_cb(lv_timer_t* t) {
  lv_obj_t* obj = (lv_obj_t*)t->user_data;
  if (obj) lv_obj_del(obj);
}

void UsageUI_DismissSplash() {
  if (!splash_root) return;
  lv_obj_fade_out(splash_root, 400, 0);
  lv_timer_t* tm = lv_timer_create(splash_del_cb, 450, splash_root);
  lv_timer_set_repeat_count(tm, 1);
  splash_root = nullptr;
  splash_state_lbl = nullptr;
}

bool UsageUI_SplashVisible() {
  return splash_root != nullptr;
}

// --- Captive portal panel ---
void UsageUI_ShowPortal(const char* ap_name, const char* ap_ip, const char* ap_pass) {
  if (portal_root) {
    lv_obj_del(portal_root);
    portal_root = nullptr;
  }
  lv_obj_t* scr = lv_scr_act();
  portal_root = lv_obj_create(scr);
  lv_obj_remove_style_all(portal_root);
  lv_obj_set_size(portal_root, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(portal_root, 0, 0);
  lv_obj_set_style_bg_color(portal_root, COL_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(portal_root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(portal_root, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(portal_root);
  lv_label_set_text(title, LV_SYMBOL_WIFI " Modalita");
  lv_obj_set_style_text_color(title, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(title, FONT_BIG, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t* title2 = lv_label_create(portal_root);
  lv_label_set_text(title2, "Setup");
  lv_obj_set_style_text_color(title2, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(title2, FONT_HUGE, LV_PART_MAIN);
  lv_obj_align(title2, LV_ALIGN_TOP_MID, 0, 36);

  lv_obj_t* sub1 = lv_label_create(portal_root);
  lv_label_set_text(sub1, "Connettiti a:");
  lv_obj_set_style_text_color(sub1, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(sub1, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(sub1, LV_ALIGN_TOP_MID, 0, 92);

  lv_obj_t* ap = lv_label_create(portal_root);
  lv_label_set_text(ap, ap_name ? ap_name : "ClaudeMonitor");
  lv_obj_set_style_text_color(ap, COL_ACCENT_TOK, LV_PART_MAIN);
  lv_obj_set_style_text_font(ap, FONT_SMALL, LV_PART_MAIN);
  lv_obj_set_style_text_align(ap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_long_mode(ap, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(ap, SCREEN_W - 12);
  lv_obj_align(ap, LV_ALIGN_TOP_MID, 0, 110);

  // Password WPA2 dell'AP (mostrata solo se fornita): l'utente la legge qui
  // per collegarsi alla rete di setup.
  if (ap_pass && ap_pass[0]) {
    lv_obj_t* sub_pw = lv_label_create(portal_root);
    lv_label_set_text(sub_pw, "Password:");
    lv_obj_set_style_text_color(sub_pw, COL_FG_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(sub_pw, FONT_SMALL, LV_PART_MAIN);
    lv_obj_align(sub_pw, LV_ALIGN_TOP_MID, 0, 148);

    lv_obj_t* pw = lv_label_create(portal_root);
    lv_label_set_text(pw, ap_pass);
    lv_obj_set_style_text_color(pw, COL_ACCENT_COST, LV_PART_MAIN);
    lv_obj_set_style_text_font(pw, FONT_BIG, LV_PART_MAIN);
    lv_obj_align(pw, LV_ALIGN_TOP_MID, 0, 166);
  }

  lv_obj_t* sub2 = lv_label_create(portal_root);
  lv_label_set_text(sub2, "poi apri:");
  lv_obj_set_style_text_color(sub2, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(sub2, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(sub2, LV_ALIGN_TOP_MID, 0, 205);

  lv_obj_t* url = lv_label_create(portal_root);
  lv_label_set_text_fmt(url, "http://%s", ap_ip ? ap_ip : "192.168.4.1");
  lv_obj_set_style_text_color(url, COL_ACCENT_COST, LV_PART_MAIN);
  lv_obj_set_style_text_font(url, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(url, LV_ALIGN_TOP_MID, 0, 223);

  lv_obj_t* hint = lv_label_create(portal_root);
  lv_label_set_text(hint, "BOOT >5s = reset");
  lv_obj_set_style_text_color(hint, COL_FG_MUTED, LV_PART_MAIN);
  lv_obj_set_style_text_font(hint, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);

  // Nascondi i 4 panel principali e la status bar resta visibile (status_bar
  // sta a z-index inferiore comunque, ma il portal_root la copre)
  for (uint8_t i = 0; i < 4; i++) {
    if (panels[i]) lv_obj_add_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void UsageUI_HidePortal() {
  if (!portal_root) return;
  lv_obj_del(portal_root);
  portal_root = nullptr;
  // Ripristina pannello attivo
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

  build_cost_panel(panels[0]);
  build_window_panel(panels[1]);
  build_chart_panel(panels[2]);
  build_models_panel(panels[3]);

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
  char buf[40];
  fmt_money(buf, sizeof(buf), d.today_cost_usd);
  lv_label_set_text(today_cost_lbl, buf);
  fmt_money(buf, sizeof(buf), d.month_cost_usd);
  lv_label_set_text(month_cost_lbl, buf);

  // "ieri $X.YZ" da last7[5] (index 6 = oggi)
  if (d.last7_cost[5] > 0.001f) {
    char yb[16];
    fmt_money(yb, sizeof(yb), d.last7_cost[5]);
    lv_label_set_text_fmt(yesterday_lbl, "ieri %s", yb);
  } else {
    lv_label_set_text(yesterday_lbl, "");
  }

  // Sparkline 7 giorni
  {
    float spark_max = 0.01f;
    for (int i = 0; i < 7; i++) if (d.last7_cost[i] > spark_max) spark_max = d.last7_cost[i];
    int sm = (int)(spark_max + 1.0f);
    lv_chart_set_range(spark_chart, LV_CHART_AXIS_PRIMARY_Y, 0, sm);
    for (int i = 0; i < 7; i++) {
      lv_chart_set_value_by_id(spark_chart, spark_series, i,
                               (int)(d.last7_cost[i] + 0.5f));
    }
    lv_chart_refresh(spark_chart);
  }

  if (d.last_update_ms > 0) {
    uint32_t age_s = (millis() - d.last_update_ms) / 1000;
    if (age_s < 999) lv_label_set_text_fmt(updated_lbl, "agg. %us fa  (#%u)", (unsigned)age_s, (unsigned)d.fetch_count);
    else             lv_label_set_text(updated_lbl, "in attesa...");
  } else {
    lv_label_set_text(updated_lbl, "in attesa di dati...");
  }

  // --- Finestra 5h ---
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
      fmt_money(buf, sizeof(buf), d.win_cost_usd);
      lv_label_set_text(win_cost_lbl, buf);
    }

    // messaggi assistant + token compatti
    char wo[16];
    fmt_tokens(wo, sizeof(wo), d.win_tokens_out);
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
  float maxv = 0.0f;
  for (int i = 0; i < 7; i++) if (d.last7_cost[i] > maxv) maxv = d.last7_cost[i];
  int scale_max = (int)(maxv * 1.1f) + 1;
  if (scale_max < 1) scale_max = 1;
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, scale_max);
  for (int i = 0; i < 7; i++) {
    int v = (int)(d.last7_cost[i] + 0.5f);
    lv_chart_set_value_by_id(chart, chart_series, i, v);
    lv_label_set_text(chart_day_lbls[i], d.last7_label[i]);
  }
  lv_chart_refresh(chart);
  if (maxv >= 1.0f) lv_label_set_text_fmt(chart_max_lbl, "max $%.0f", maxv);
  else              lv_label_set_text_fmt(chart_max_lbl, "max $%.2f", maxv);

  // --- Modelli ---
  uint8_t n = d.n_models;
  if (n > MODEL_ROWS) n = MODEL_ROWS;
  float total_cost = 0.0f;
  for (int i = 0; i < n; i++) total_cost += d.models[i].cost_usd;
  if (total_cost < 0.001f) total_cost = 1.0f;

  for (int i = 0; i < MODEL_ROWS; i++) {
    if (i < n) {
      lv_obj_clear_flag(model_row[i], LV_OBJ_FLAG_HIDDEN);
      char nm[24];
      short_model_name(d.models[i].name, nm, sizeof(nm));
      lv_label_set_text(model_name_lbl[i], nm);
      char mc[20];
      fmt_money(mc, sizeof(mc), d.models[i].cost_usd);
      lv_label_set_text(model_cost_lbl[i], mc);
      int p = (int)((d.models[i].cost_usd / total_cost) * 100.0f);
      lv_bar_set_value(model_bar[i], p, LV_ANIM_OFF);
    } else {
      lv_obj_add_flag(model_row[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (n == 0) lv_obj_clear_flag(model_empty_lbl, LV_OBJ_FLAG_HIDDEN);
  else        lv_obj_add_flag(model_empty_lbl,   LV_OBJ_FLAG_HIDDEN);
}
