#include "UsageUI.h"
#include "UsageUI_internal.h"
#include "UsageUI_format.h"
#include <Arduino.h>   // millis()

// Tab 0: COST — today, yesterday, month, 7-day sparkline, "updated X ago".
static lv_obj_t* today_cost_lbl;
static lv_obj_t* month_cost_lbl;
static lv_obj_t* yesterday_lbl;
static lv_obj_t* spark_chart;
static lv_chart_series_t* spark_series;
static lv_obj_t* updated_lbl;

void ui_cost_build(lv_obj_t* p) {
  // ----- Left column: TODAY -----
  lv_obj_t* hdr = lv_label_create(p);
  lv_label_set_text(hdr, "$ TODAY");
  lv_obj_set_style_text_color(hdr, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

  today_cost_lbl = lv_label_create(p);
  lv_label_set_text(today_cost_lbl, "$0.00");
  lv_obj_set_style_text_color(today_cost_lbl, COL_ACCENT_COST, LV_PART_MAIN);
  lv_obj_set_style_text_font(today_cost_lbl, FONT_HUGE, LV_PART_MAIN);
  lv_obj_align(today_cost_lbl, LV_ALIGN_TOP_LEFT, 0, 24);

  yesterday_lbl = lv_label_create(p);
  lv_label_set_text(yesterday_lbl, "");
  lv_obj_set_style_text_color(yesterday_lbl, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(yesterday_lbl, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(yesterday_lbl, LV_ALIGN_TOP_LEFT, 0, 66);

  // ----- Vertical divider between columns -----
  lv_obj_t* sep = lv_obj_create(p);
  lv_obj_remove_style_all(sep);
  lv_obj_set_size(sep, 1, 110);
  lv_obj_set_style_bg_color(sep, COL_DIVIDER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(sep, LV_ALIGN_TOP_LEFT, 150, 6);

  // ----- Right column: MONTH + 7-day sparkline -----
  lv_obj_t* mh = lv_label_create(p);
  lv_label_set_text(mh, "MONTH");
  lv_obj_set_style_text_color(mh, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(mh, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(mh, LV_ALIGN_TOP_LEFT, 168, 0);

  month_cost_lbl = lv_label_create(p);
  lv_label_set_text(month_cost_lbl, "$0.00");
  lv_obj_set_style_text_color(month_cost_lbl, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(month_cost_lbl, FONT_BIG, LV_PART_MAIN);
  lv_obj_align(month_cost_lbl, LV_ALIGN_TOP_LEFT, 168, 22);

  spark_chart = lv_chart_create(p);
  lv_obj_set_size(spark_chart, 132, 50);
  lv_obj_align(spark_chart, LV_ALIGN_TOP_LEFT, 166, 58);
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
  lv_obj_align(updated_lbl, LV_ALIGN_BOTTOM_LEFT, 0, -2);
}

void ui_cost_update(const UsageData& d) {
  char buf[40];
  ui_fmt_money(buf, sizeof(buf), d.today_cost_usd);
  lv_label_set_text(today_cost_lbl, buf);
  ui_fmt_money(buf, sizeof(buf), d.month_cost_usd);
  lv_label_set_text(month_cost_lbl, buf);

  // "yesterday $X.YZ" from last7[5] (index 6 = today)
  if (d.last7_cost[5] > 0.001f) {
    char yb[16];
    ui_fmt_money(yb, sizeof(yb), d.last7_cost[5]);
    lv_label_set_text_fmt(yesterday_lbl, "yesterday %s", yb);
  } else {
    lv_label_set_text(yesterday_lbl, "");
  }

  // 7-day sparkline
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
    if (age_s < 999) lv_label_set_text_fmt(updated_lbl, "upd %us ago  (#%u)", (unsigned)age_s, (unsigned)d.fetch_count);
    else             lv_label_set_text(updated_lbl, "waiting...");
  } else {
    lv_label_set_text(updated_lbl, "waiting for data...");
  }
}
