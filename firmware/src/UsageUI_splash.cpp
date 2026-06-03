#include "UsageUI.h"
#include "UsageUI_internal.h"

// Splash overlay di boot, sopra l'UI principale.
static lv_obj_t* splash_root = nullptr;
static lv_obj_t* splash_state_lbl = nullptr;

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

bool ui_splash_blocking() {
  return splash_root != nullptr;
}
