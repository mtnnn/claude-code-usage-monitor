#include "UsageUI.h"
#include "UsageUI_internal.h"
#include "UsageUI_format.h"

// Tab 3: MODELS — up to 5 rows (name + cost + % bar), or "No data".
#define MODEL_ROWS 5
static lv_obj_t* model_row[MODEL_ROWS];
static lv_obj_t* model_name_lbl[MODEL_ROWS];
static lv_obj_t* model_cost_lbl[MODEL_ROWS];
static lv_obj_t* model_bar[MODEL_ROWS];
static lv_obj_t* model_empty_lbl;

void ui_models_build(lv_obj_t* p) {
  lv_obj_t* hdr = lv_label_create(p);
  lv_label_set_text(hdr, LV_SYMBOL_OK " MODELS (MONTH)");
  lv_obj_set_style_text_color(hdr, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 4);

  // Landscape: short rows so all 5 fit in the ~140px tall content area.
  int row_h = 22;
  int top_y = 18;
  for (int i = 0; i < MODEL_ROWS; i++) {
    model_row[i] = lv_obj_create(p);
    lv_obj_remove_style_all(model_row[i]);
    lv_obj_set_size(model_row[i], PANEL_W - 12, row_h);
    lv_obj_set_pos(model_row[i], 6, top_y + i * (row_h + 2));
    lv_obj_set_style_bg_color(model_row[i], COL_BG_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(model_row[i], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(model_row[i], 4, LV_PART_MAIN);
    lv_obj_clear_flag(model_row[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(model_row[i], LV_OBJ_FLAG_HIDDEN);

    model_name_lbl[i] = lv_label_create(model_row[i]);
    lv_label_set_text(model_name_lbl[i], "");
    lv_obj_set_style_text_color(model_name_lbl[i], COL_FG, LV_PART_MAIN);
    lv_obj_set_style_text_font(model_name_lbl[i], FONT_TINY, LV_PART_MAIN);
    lv_obj_set_pos(model_name_lbl[i], 6, 2);

    model_cost_lbl[i] = lv_label_create(model_row[i]);
    lv_label_set_text(model_cost_lbl[i], "");
    lv_obj_set_style_text_color(model_cost_lbl[i], COL_ACCENT_COST, LV_PART_MAIN);
    lv_obj_set_style_text_font(model_cost_lbl[i], FONT_TINY, LV_PART_MAIN);
    lv_obj_align(model_cost_lbl[i], LV_ALIGN_TOP_RIGHT, -6, 2);

    model_bar[i] = lv_bar_create(model_row[i]);
    lv_obj_set_size(model_bar[i], PANEL_W - 32, 4);
    lv_obj_set_pos(model_bar[i], 6, 16);
    lv_bar_set_range(model_bar[i], 0, 100);
    lv_bar_set_value(model_bar[i], 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(model_bar[i], COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(model_bar[i], COL_ACCENT_TOK, LV_PART_INDICATOR);
  }

  model_empty_lbl = lv_label_create(p);
  lv_label_set_text(model_empty_lbl, "No data");
  lv_obj_set_style_text_color(model_empty_lbl, COL_FG_MUTED, LV_PART_MAIN);
  lv_obj_set_style_text_font(model_empty_lbl, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(model_empty_lbl, LV_ALIGN_CENTER, 0, 0);
}

void ui_models_update(const UsageData& d) {
  uint8_t n = d.n_models;
  if (n > MODEL_ROWS) n = MODEL_ROWS;
  float total_cost = 0.0f;
  for (int i = 0; i < n; i++) total_cost += d.models[i].cost_usd;
  if (total_cost < 0.001f) total_cost = 1.0f;

  for (int i = 0; i < MODEL_ROWS; i++) {
    if (i < n) {
      lv_obj_clear_flag(model_row[i], LV_OBJ_FLAG_HIDDEN);
      char nm[24];
      ui_short_model_name(d.models[i].name, nm, sizeof(nm));
      lv_label_set_text(model_name_lbl[i], nm);
      char mc[20];
      ui_fmt_money(mc, sizeof(mc), d.models[i].cost_usd);
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
