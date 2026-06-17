#pragma once
#include <lvgl.h>
#include "Display_ST7789.h"

// Shared UI theme (colors, fonts, layout). A single source of truth,
// included by all UsageUI_*.cpp modules.

// ===== v0.2 palette =====
// A single source of truth for the colors; the code below uses only these macros.
#define COL_BG           lv_color_hex(0x0b0d10)
#define COL_BG_PANEL     lv_color_hex(0x14181d)
#define COL_BG_STATUS    lv_color_hex(0x101820)
#define COL_BG_CARD      lv_color_hex(0x1a1a1a)
#define COL_FG           lv_color_hex(0xe6e6e6)
#define COL_FG_DIM       lv_color_hex(0x8a8f96)
#define COL_FG_MUTED     lv_color_hex(0x666666)
#define COL_DIVIDER      lv_color_hex(0x444444)
#define COL_BAR_BG       lv_color_hex(0x4c5159)
#define COL_ACCENT_COST  lv_color_hex(0xd97757)   // Claude coral/orange — cost/money
#define COL_ACCENT_TOK   lv_color_hex(0x38bdf8)   // cyan — tokens
#define COL_WARN         lv_color_hex(0xfbbf24)   // amber — 70%+ / medium ETA
#define COL_DANGER       lv_color_hex(0xef4444)   // red — 90%+ / ETA <30
#define COL_TIME         lv_color_hex(0xa78bfa)   // purple — time bar

// ----- Font: use the optional ones if enabled in lv_conf.h, otherwise fall back -----
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
#define SCREEN_W LCD_WIDTH    // 320 (landscape)
#define SCREEN_H LCD_HEIGHT   // 170 (landscape)
#define PANEL_W  SCREEN_W
#define PANEL_H  (SCREEN_H - STATUS_H)

#define ROTATE_INTERVAL_MS 6000
