#include "UsageUI.h"
#include "UsageUI_internal.h"

// Captive portal overlay (setup mode), above the main UI. Hides the 4
// panels via the core helpers (ui_hide_main_panels /
// ui_restore_active_panel), which own panels[]/active_panel.
static lv_obj_t* portal_root = nullptr;

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
  lv_label_set_text(title, LV_SYMBOL_WIFI " WiFi Setup");
  lv_obj_set_style_text_color(title, COL_FG, LV_PART_MAIN);
  lv_obj_set_style_text_font(title, FONT_BIG, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

  // ----- Left column: join the AP -----
  lv_obj_t* sub1 = lv_label_create(portal_root);
  lv_label_set_text(sub1, "Connect to:");
  lv_obj_set_style_text_color(sub1, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(sub1, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(sub1, LV_ALIGN_TOP_LEFT, 4, 30);

  lv_obj_t* ap = lv_label_create(portal_root);
  lv_label_set_text(ap, ap_name ? ap_name : "ClaudeMonitor");
  lv_obj_set_style_text_color(ap, COL_ACCENT_TOK, LV_PART_MAIN);
  lv_obj_set_style_text_font(ap, FONT_SMALL, LV_PART_MAIN);
  lv_obj_set_style_text_align(ap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_long_mode(ap, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(ap, 150);
  lv_obj_align(ap, LV_ALIGN_TOP_LEFT, 4, 48);

  // WPA2 password of the AP (shown only if provided): the user reads it here
  // to join the setup network.
  if (ap_pass && ap_pass[0]) {
    lv_obj_t* sub_pw = lv_label_create(portal_root);
    lv_label_set_text(sub_pw, "Password:");
    lv_obj_set_style_text_color(sub_pw, COL_FG_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(sub_pw, FONT_SMALL, LV_PART_MAIN);
    lv_obj_align(sub_pw, LV_ALIGN_TOP_LEFT, 4, 92);

    lv_obj_t* pw = lv_label_create(portal_root);
    lv_label_set_text(pw, ap_pass);
    lv_obj_set_style_text_color(pw, COL_ACCENT_COST, LV_PART_MAIN);
    lv_obj_set_style_text_font(pw, FONT_BIG, LV_PART_MAIN);
    lv_obj_align(pw, LV_ALIGN_TOP_LEFT, 4, 108);
  }

  lv_obj_t* sub2 = lv_label_create(portal_root);
  lv_label_set_text(sub2, "then open:");
  lv_obj_set_style_text_color(sub2, COL_FG_DIM, LV_PART_MAIN);
  lv_obj_set_style_text_font(sub2, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(sub2, LV_ALIGN_TOP_LEFT, 168, 30);

  lv_obj_t* url = lv_label_create(portal_root);
  lv_label_set_text_fmt(url, "http://%s", ap_ip ? ap_ip : "192.168.4.1");
  lv_obj_set_style_text_color(url, COL_ACCENT_COST, LV_PART_MAIN);
  lv_obj_set_style_text_font(url, FONT_SMALL, LV_PART_MAIN);
  lv_obj_align(url, LV_ALIGN_TOP_LEFT, 168, 48);

  lv_obj_t* hint = lv_label_create(portal_root);
  lv_label_set_text(hint, "BOOT >5s = reset");
  lv_obj_set_style_text_color(hint, COL_FG_MUTED, LV_PART_MAIN);
  lv_obj_set_style_text_font(hint, FONT_TINY, LV_PART_MAIN);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);

  // Hide the 4 main panels (the status bar stays underneath, covered by
  // portal_root).
  ui_hide_main_panels();
}

void UsageUI_HidePortal() {
  if (!portal_root) return;
  lv_obj_del(portal_root);
  portal_root = nullptr;
  ui_restore_active_panel();
}

bool ui_portal_blocking() {
  return portal_root != nullptr;
}
