#pragma once
#include <lvgl.h>
#include "UsageClient.h"     // UsageData
#include "UsageUI_theme.h"

// Contratto interno tra il core (UsageUI.cpp) e i moduli UsageUI_*.cpp.
// La API pubblica resta esclusivamente in UsageUI.h; i nomi qui usano il
// prefisso minuscolo `ui_` per distinguerli.

// Pannelli: build() popola il container fornito dal core; update() aggiorna i
// widget dai dati. Ogni modulo possiede i propri widget come static di TU.
void ui_cost_build(lv_obj_t* parent);    void ui_cost_update(const UsageData& d);
void ui_window_build(lv_obj_t* parent);  void ui_window_update(const UsageData& d);
void ui_chart_build(lv_obj_t* parent);   void ui_chart_update(const UsageData& d);
void ui_models_build(lv_obj_t* parent);  void ui_models_update(const UsageData& d);

// Overlay: il core li interroga per sospendere l'auto-rotazione quando uno
// dei due è visibile.
bool ui_splash_blocking();   // splash visibile
bool ui_portal_blocking();   // portal visibile

// Helper del core usati dal modulo portal per nascondere/ripristinare i 4
// pannelli principali (panels[]/active_panel restano stato del core).
void ui_hide_main_panels();
void ui_restore_active_panel();
