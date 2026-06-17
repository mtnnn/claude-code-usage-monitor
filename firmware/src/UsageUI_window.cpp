#include "UsageUI.h"
#include "UsageUI_internal.h"
#include "UsageUI_format.h"

// Tab 1: 5h WINDOW (Claude Code rate-limit window) — % of limit, cost + cap,
// messages/tokens, Time (purple) and Limit (green/amber/red) bars, ETA-to-limit
// and reset countdown.
static lv_obj_t* win_msg_lbl;
static lv_obj_t* win_cost_lbl;
static lv_obj_t* win_tokens_lbl;
static lv_obj_t* win_time_bar;       // Time bar (purple, 0..300 min)
static lv_obj_t* win_time_val_lbl;
static lv_obj_t* win_limit_bar;      // Limit bar (green/amber/red, 0..100%)
static lv_obj_t* win_limit_val_lbl;
static lv_obj_t* win_eta_lbl;        // populated in M4 (ETA-to-limit)
static lv_obj_t* win_reset_lbl;

void ui_window_build(lv_obj_t* p) {
  lv_obj_t* hdr = lv_label_create(p);
  lv_label_set_text(hdr, LV_SYMBOL_REFRESH " 5h WINDOW");
  lv_obj_set_style_text_color(hdr, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

  // ----- Left column: % of limit + cost + msgs -----
  win_msg_lbl = lv_label_create(p);
  lv_label_set_text(win_msg_lbl, "0%");
  lv_obj_set_style_text_color(win_msg_lbl, COL_ACCENT_TOK, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_msg_lbl, FONT_HUGE, LV_PART_MAIN);
  lv_obj_align(win_msg_lbl, LV_ALIGN_TOP_LEFT, 0, 18);

  lv_obj_t* mh = lv_label_create(p);
  lv_label_set_text(mh, "of 5h limit");
  lv_obj_set_style_text_color(mh, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(mh, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(mh, LV_ALIGN_TOP_LEFT, 0, 56);

  win_cost_lbl = lv_label_create(p);
  lv_label_set_text(win_cost_lbl, "$0.00");
  lv_obj_set_style_text_color(win_cost_lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_cost_lbl, FONT_BIG, LV_PART_MAIN);
  lv_obj_align(win_cost_lbl, LV_ALIGN_TOP_LEFT, 0, 74);

  win_tokens_lbl = lv_label_create(p);
  lv_label_set_text(win_tokens_lbl, "");
  lv_obj_set_style_text_color(win_tokens_lbl, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_tokens_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(win_tokens_lbl, LV_ALIGN_TOP_LEFT, 0, 104);

  // ----- Right column: Time bar (purple) + Limit bar -----
  const int RX = 160;          // right column x
  const int RW = PANEL_W - RX - 6;

  lv_obj_t* tempo_lbl = lv_label_create(p);
  lv_label_set_text(tempo_lbl, "Time");
  lv_obj_set_style_text_color(tempo_lbl, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(tempo_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(tempo_lbl, LV_ALIGN_TOP_LEFT, RX, 14);

  win_time_val_lbl = lv_label_create(p);
  lv_label_set_text(win_time_val_lbl, "0h 00m");
  lv_obj_set_style_text_color(win_time_val_lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_time_val_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(win_time_val_lbl, LV_ALIGN_TOP_RIGHT, -2, 14);

  win_time_bar = lv_bar_create(p);
  lv_obj_set_size(win_time_bar, RW, 10);
  lv_obj_set_pos(win_time_bar, RX, 32);
  lv_bar_set_range(win_time_bar, 0, 300);
  lv_bar_set_value(win_time_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(win_time_bar, COL_BAR_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_color(win_time_bar, COL_TIME, LV_PART_INDICATOR);

  lv_obj_t* lim_lbl = lv_label_create(p);
  lv_label_set_text(lim_lbl, "Limit");
  lv_obj_set_style_text_color(lim_lbl, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(lim_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(lim_lbl, LV_ALIGN_TOP_LEFT, RX, 52);

  win_limit_val_lbl = lv_label_create(p);
  lv_label_set_text(win_limit_val_lbl, "0%");
  lv_obj_set_style_text_color(win_limit_val_lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_limit_val_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(win_limit_val_lbl, LV_ALIGN_TOP_RIGHT, -2, 52);

  win_limit_bar = lv_bar_create(p);
  lv_obj_set_size(win_limit_bar, RW, 10);
  lv_obj_set_pos(win_limit_bar, RX, 70);
  lv_bar_set_range(win_limit_bar, 0, 100);
  lv_bar_set_value(win_limit_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(win_limit_bar, COL_BAR_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_color(win_limit_bar, COL_ACCENT_COST, LV_PART_INDICATOR);

  // ETA-to-limit (right column)
  win_eta_lbl = lv_label_create(p);
  lv_label_set_text(win_eta_lbl, "");
  lv_obj_set_style_text_color(win_eta_lbl, COL_WARN, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_eta_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(win_eta_lbl, LV_ALIGN_TOP_LEFT, RX, 90);

  // Countdown reset (right column, bottom)
  win_reset_lbl = lv_label_create(p);
  lv_label_set_text(win_reset_lbl, "no active window");
  lv_obj_set_style_text_color(win_reset_lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(win_reset_lbl, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(win_reset_lbl, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
}

void ui_window_update(const UsageData& d) {
  char buf[40];
  if (d.win_active) {
    // % of limit, large
    uint16_t lp = d.win_limit_pct;
    if (lp > 999) lp = 999;
    lv_label_set_text_fmt(win_msg_lbl, "%u%%", (unsigned)lp);

    // large-number color + limit bar: green / amber / red
    lv_color_t lpcol = COL_ACCENT_COST;
    if (lp >= 90)      lpcol = COL_DANGER;
    else if (lp >= 70) lpcol = COL_WARN;
    lv_obj_set_style_text_color(win_msg_lbl, lpcol, LV_PART_MAIN);
    lv_obj_set_style_bg_color(win_limit_bar, lpcol, LV_PART_INDICATOR);

    // cost + plan cap
    if (d.win_limit_usd > 0.001f) {
      lv_label_set_text_fmt(win_cost_lbl, "$%.2f / $%.0f",
                            d.win_cost_usd, d.win_limit_usd);
    } else {
      ui_fmt_money(buf, sizeof(buf), d.win_cost_usd);
      lv_label_set_text(win_cost_lbl, buf);
    }

    // assistant messages + compact tokens
    char wo[16];
    ui_fmt_tokens(wo, sizeof(wo), d.win_tokens_out);
    lv_label_set_text_fmt(win_tokens_lbl, "%u msg  |  out %s",
                          (unsigned)d.win_messages, wo);

    // Time bar (0..300 min)
    uint16_t em = d.win_elapsed_min > 300 ? 300 : d.win_elapsed_min;
    lv_bar_set_value(win_time_bar, em, LV_ANIM_OFF);
    lv_label_set_text_fmt(win_time_val_lbl, "%uh %02um",
                          (unsigned)(em / 60), (unsigned)(em % 60));

    // Limit bar (0..100%, capped)
    int lim_pct_cap = (lp > 100) ? 100 : (int)lp;
    lv_bar_set_value(win_limit_bar, lim_pct_cap, LV_ANIM_OFF);
    lv_label_set_text_fmt(win_limit_val_lbl, "%u%%", (unsigned)lp);

    // Reset countdown
    uint16_t r = d.win_remaining_min;
    if (r == 0) {
      lv_label_set_text(win_reset_lbl, "resetting...");
    } else if (r < 60) {
      lv_label_set_text_fmt(win_reset_lbl, "reset in %um", (unsigned)r);
    } else {
      lv_label_set_text_fmt(win_reset_lbl, "reset in %uh %02um",
                            (unsigned)(r / 60), (unsigned)(r % 60));
    }

    // ETA-to-limit: minutes left until the limit is reached at the current burn rate.
    // Shown only if the rate is non-zero and the ETA falls within the remaining window —
    // otherwise the limit will never be reached in this window.
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
        lv_label_set_text_fmt(win_eta_lbl, "limit ETA: %dm", em);
      } else {
        lv_label_set_text_fmt(win_eta_lbl, "limit ETA: %dh %02dm",
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
    lv_label_set_text(win_reset_lbl, "no active window");
  }
}
