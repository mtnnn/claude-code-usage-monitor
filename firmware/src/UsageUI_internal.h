#pragma once
#include <lvgl.h>
#include "UsageClient.h"     // UsageData
#include "UsageUI_theme.h"

// Internal contract between the core (UsageUI.cpp) and the UsageUI_*.cpp modules.
// The public API stays exclusively in UsageUI.h; the names here use the
// lowercase `ui_` prefix to distinguish them.

// Panels: build() populates the container provided by the core; update() refreshes
// the widgets from the data. Each module owns its own widgets as TU statics.
void ui_cost_build(lv_obj_t* parent);    void ui_cost_update(const UsageData& d);
void ui_window_build(lv_obj_t* parent);  void ui_window_update(const UsageData& d);
void ui_chart_build(lv_obj_t* parent);   void ui_chart_update(const UsageData& d);
void ui_models_build(lv_obj_t* parent);  void ui_models_update(const UsageData& d);

// Overlays: the core queries these to suspend auto-rotation when either
// one is visible.
bool ui_splash_blocking();   // splash visible
bool ui_portal_blocking();   // portal visible

// Core helpers used by the portal module to hide/restore the 4 main
// panels (panels[]/active_panel remain core state).
void ui_hide_main_panels();
void ui_restore_active_panel();
