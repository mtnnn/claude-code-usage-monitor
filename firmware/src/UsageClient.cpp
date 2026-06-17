#include "UsageClient.h"
#include "Wireless.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static UsageData s_data {};
static SemaphoreHandle_t s_mutex = nullptr;

static String s_host;
static String s_token;
static uint16_t s_port = 8787;
static uint32_t s_interval_ms = 5000;

static void copy_str(char* dst, size_t cap, const char* src) {
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
}

// Extracts "16" from "2026-05-16" (the last 2 characters)
static void date_to_day(const char* iso, char out[4]) {
  size_t n = iso ? strlen(iso) : 0;
  if (n >= 2) {
    out[0] = iso[n - 2];
    out[1] = iso[n - 1];
    out[2] = 0;
  } else {
    copy_str(out, 4, "??");
  }
}

static bool fetch_once(UsageData& tmp) {
  if (!wifi_connected) return false;

  // Snapshot the target under the mutex: SetTarget() can rewrite it from another
  // core (Control's HTTP handler) while this task runs. We copy into locals and
  // release before the blocking GET so we don't hold the lock during I/O.
  String host, token;
  uint16_t port;
  if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
    host  = s_host;
    port  = s_port;
    token = s_token;
    xSemaphoreGive(s_mutex);
  } else {
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(2000);
  http.setTimeout(3000);

  String url = "http://" + host + ":" + String(port) + "/usage";
  if (!http.begin(url)) {
    Serial.println("[UsageClient] http.begin failed");
    return false;
  }
  if (token.length() > 0) {
    http.addHeader("Authorization", String("Bearer ") + token);
  }
  int code = http.GET();
  if (code == 401) {
    Serial.println("[UsageClient] HTTP 401 — bridge rejected the token, check BRIDGE_TOKEN");
    http.end();
    return false;
  }
  if (code != 200) {
    Serial.printf("[UsageClient] HTTP %d\n", code);
    http.end();
    return false;
  }

  // Defense-in-depth: if the server declares a response much larger
  // than expected (~2-4KB typical), we don't even try to deserialize it — so
  // an unexpected endpoint cannot force a huge allocation on the device.
  // Content-Length -1 (unknown, e.g. behind a chunked proxy) stays allowed.
  static const int MAX_RESPONSE_BYTES = 32 * 1024;
  int declared = http.getSize();
  if (declared > MAX_RESPONSE_BYTES) {
    Serial.printf("[UsageClient] declared response too large (%d B), ignoring\n",
                  declared);
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    Serial.printf("[UsageClient] JSON err: %s\n", err.c_str());
    return false;
  }

  JsonObject today = doc["today"].as<JsonObject>();
  tmp.today_cost_usd   = today["cost_usd"]   | 0.0f;
  tmp.today_tokens_in  = today["tokens_in"]  | 0ULL;
  tmp.today_tokens_out = today["tokens_out"] | 0ULL;
  tmp.today_cache_read = today["cache_read"] | 0ULL;

  JsonObject month = doc["month"].as<JsonObject>();
  tmp.month_cost_usd   = month["cost_usd"]   | 0.0f;
  tmp.month_tokens_in  = month["tokens_in"]  | 0ULL;
  tmp.month_tokens_out = month["tokens_out"] | 0ULL;
  tmp.month_cache_read = month["cache_read"] | 0ULL;

  tmp.budget_monthly_usd = doc["budget_monthly_usd"] | 0.0f;

  JsonObject w = doc["window5h"].as<JsonObject>();
  tmp.win_active        = w["active"]        | false;
  tmp.win_messages      = w["messages"]      | 0u;
  tmp.win_cost_usd      = w["cost_usd"]      | 0.0f;
  tmp.win_tokens_in     = w["tokens_in"]     | 0ULL;
  tmp.win_tokens_out    = w["tokens_out"]    | 0ULL;
  tmp.win_elapsed_min   = w["elapsed_min"]   | 0u;
  tmp.win_remaining_min = w["remaining_min"] | 0u;
  tmp.win_limit_usd     = w["limit_usd"]     | 0.0f;
  tmp.win_limit_pct     = w["limit_pct"]     | 0u;

  JsonArray last7 = doc["last7"].as<JsonArray>();
  for (uint8_t i = 0; i < 7; i++) {
    if (i < last7.size()) {
      JsonObject d = last7[i].as<JsonObject>();
      tmp.last7_cost[i] = d["cost_usd"] | 0.0f;
      date_to_day(d["date"] | "", tmp.last7_label[i]);
    } else {
      tmp.last7_cost[i] = 0.0f;
      copy_str(tmp.last7_label[i], 4, "");
    }
  }

  JsonArray models = doc["by_model"].as<JsonArray>();
  uint8_t n = 0;
  for (JsonObject m : models) {
    if (n >= MAX_MODELS) break;
    copy_str(tmp.models[n].name, sizeof(tmp.models[n].name), m["name"] | "?");
    tmp.models[n].cost_usd   = m["cost_usd"]   | 0.0f;
    tmp.models[n].tokens_in  = m["tokens_in"]  | 0ULL;
    tmp.models[n].tokens_out = m["tokens_out"] | 0ULL;
    n++;
  }
  tmp.n_models = n;

  return true;
}

static void poll_task(void*) {
  for (;;) {
    UsageData tmp = {};
    bool ok = fetch_once(tmp);
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
      if (ok) {
        // preserve fetch_count, update the data
        tmp.fetch_count    = s_data.fetch_count + 1;
        tmp.online         = true;
        tmp.last_update_ms = millis();
        s_data = tmp;
      } else {
        s_data.online = false;
      }
      xSemaphoreGive(s_mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(s_interval_ms));
  }
}

void UsageClient_Begin(const char* host, uint16_t port, uint32_t interval_ms,
                       const char* token) {
  s_host = host;
  s_port = port;
  s_interval_ms = interval_ms;
  s_token = token ? token : "";
  if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(poll_task, "usage_poll", 8192, nullptr, 1, nullptr, 0);
  // We NEVER log the token to Serial.
  Serial.printf("[UsageClient] polling http://%s:%u/usage every %u ms (auth %s)\n",
                host, (unsigned)port, (unsigned)interval_ms,
                s_token.length() > 0 ? "ON" : "OFF");
}

void UsageClient_SetTarget(const char* host, uint16_t port, const char* token) {
  if (!s_mutex) return;  // Begin not yet called: nothing to re-point.
  if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
    s_host  = host  ? host  : "";
    s_port  = port;
    s_token = token ? token : "";
    xSemaphoreGive(s_mutex);
  }
  // We NEVER log the token to Serial.
  Serial.printf("[UsageClient] target updated: http://%s:%u/usage (auth %s)\n",
                host ? host : "", (unsigned)port,
                (token && token[0]) ? "ON" : "OFF");
}

void UsageClient_Snapshot(UsageData& out) {
  if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    out = s_data;
    xSemaphoreGive(s_mutex);
  } else {
    out = {};
  }
}
