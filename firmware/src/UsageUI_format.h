#pragma once
#include <stddef.h>
#include <stdint.h>

// Helper di formattazione condivisi dai pannelli UI. Funzioni pure: scrivono in
// `dst` (cap byte) a partire dagli argomenti, nessuno stato.
void ui_fmt_money(char* dst, size_t cap, float v);
void ui_fmt_tokens(char* dst, size_t cap, uint64_t v);
void ui_short_model_name(const char* full, char* dst, size_t cap);
