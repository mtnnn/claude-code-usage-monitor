#include "UsageUI.h"
#include "UsageUI_internal.h"

// Tab 2: GRAFICO 7 GIORNI — bar chart con etichette giorno e "max $X".
static lv_obj_t* chart;
static lv_chart_series_t* chart_series;
static lv_obj_t* chart_day_lbls[7];
static lv_obj_t* chart_max_lbl;

void ui_chart_build(lv_obj_t* p) {
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

void ui_chart_update(const UsageData& d) {
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
}
