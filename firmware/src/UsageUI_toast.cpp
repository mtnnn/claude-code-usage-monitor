#include "UsageUI.h"
#include "UsageUI_theme.h"

// Toast overlay in basso (1.5s), auto-dismiss. Modulo autonomo.

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
