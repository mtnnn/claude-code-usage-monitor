#include "UsageUI_format.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void ui_fmt_money(char* dst, size_t cap, float v) {
  if (v >= 1000.0f) snprintf(dst, cap, "$%.0f", v);
  else              snprintf(dst, cap, "$%.2f", v);
}

void ui_fmt_tokens(char* dst, size_t cap, uint64_t v) {
  if (v < 1000ULL)              snprintf(dst, cap, "%llu", (unsigned long long)v);
  else if (v < 1000000ULL)      snprintf(dst, cap, "%.1fK", v / 1000.0);
  else if (v < 1000000000ULL)   snprintf(dst, cap, "%.2fM", v / 1000000.0);
  else                          snprintf(dst, cap, "%.2fB", v / 1000000000.0);
}

// estrae sigla modello: "claude-opus-4-7" -> "Opus 4.7"
void ui_short_model_name(const char* full, char* dst, size_t cap) {
  if (!full || !*full) { snprintf(dst, cap, "?"); return; }
  // cerca opus/sonnet/haiku
  const char* fam = nullptr;
  if (strstr(full, "opus"))   fam = "Opus";
  else if (strstr(full, "sonnet")) fam = "Sonnet";
  else if (strstr(full, "haiku"))  fam = "Haiku";
  else { snprintf(dst, cap, "%s", full); return; }

  // estrai prima coppia di numeri "4-7" -> "4.7"
  int maj = 0, min = -1;
  const char* p = full;
  while (*p) {
    if (*p >= '0' && *p <= '9') {
      maj = atoi(p);
      while (*p >= '0' && *p <= '9') p++;
      if (*p == '-' && p[1] >= '0' && p[1] <= '9') {
        min = atoi(p + 1);
      }
      break;
    }
    p++;
  }
  if (min >= 0) snprintf(dst, cap, "%s %d.%d", fam, maj, min);
  else if (maj > 0) snprintf(dst, cap, "%s %d", fam, maj);
  else snprintf(dst, cap, "%s", fam);
}
