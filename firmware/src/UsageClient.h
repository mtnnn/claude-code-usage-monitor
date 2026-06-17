#pragma once
#include <Arduino.h>

#define MAX_MODELS 6

struct ModelStat {
  char     name[40];
  float    cost_usd;
  uint64_t tokens_in;
  uint64_t tokens_out;
};

struct UsageData {
  bool      online;            // last poll succeeded?
  uint32_t  last_update_ms;    // millis() at the last success
  uint32_t  fetch_count;       // number of successful polls (for "live" anim)

  float    today_cost_usd;
  uint64_t today_tokens_in;
  uint64_t today_tokens_out;
  uint64_t today_cache_read;

  float    month_cost_usd;
  uint64_t month_tokens_in;
  uint64_t month_tokens_out;
  uint64_t month_cache_read;

  float    budget_monthly_usd;

  // Current 5h window (Claude Code rate-limit window)
  bool     win_active;
  uint32_t win_messages;
  float    win_cost_usd;
  uint64_t win_tokens_in;
  uint64_t win_tokens_out;
  uint16_t win_elapsed_min;
  uint16_t win_remaining_min;
  float    win_limit_usd;       // plan limit (e.g. $200 for Max 5x)
  uint16_t win_limit_pct;       // 0-100+ (can exceed 100 if over limit)

  // Last 7 days: index 0 = 6 days ago, index 6 = today
  float    last7_cost[7];
  char     last7_label[7][4];   // "10", "11", ...

  ModelStat models[MAX_MODELS];
  uint8_t   n_models;
};

// Start a dedicated FreeRTOS task that polls the bridge.
// host: e.g. "192.168.1.42", port: 8787, interval_ms: poll cadence
// token: bridge bearer token (empty string => header omitted, v0.1 compatibility)
void UsageClient_Begin(const char* host, uint16_t port, uint32_t interval_ms,
                       const char* token = "");

// Update the bridge target (host/port/token) at runtime without rebooting the
// device: the poll_task will restart from the new endpoint on the next cycle. Used
// by the control endpoint (Control) to re-point the board when the laptop's IP
// changes. Thread-safe (protected by the same mutex as the snapshot).
void UsageClient_SetTarget(const char* host, uint16_t port, const char* token);

// Atomically copies the latest snapshot into out. Safe to call from the UI thread.
void UsageClient_Snapshot(UsageData& out);
