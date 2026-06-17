#include "Control.h"
#include "Config.h"
#include "UsageClient.h"
#include <WiFi.h>
#include <WebServer.h>

static WebServer s_ctrl(80);
static bool      s_running = false;

// WebServer only collects the headers explicitly requested: we need
// Authorization for the automatic path (the bridge sends the bearer token in a
// header, not in the form).
static const char* COLLECT_HEADERS[] = { "Authorization" };

// ===== Helpers =====

static String escape_html(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '<':  out += "&lt;";   break;
      case '>':  out += "&gt;";   break;
      case '&':  out += "&amp;";  break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;";  break;
      default:   out += c;
    }
  }
  return out;
}

// (Near) constant-time comparison: avoids revealing how many characters of the
// token match. The length can leak, but that's acceptable in the LAN threat
// model documented in SECURITY.md.
static bool ct_equal(const String& a, const String& b) {
  if (a.length() != b.length()) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < a.length(); i++) diff |= (uint8_t)a[i] ^ (uint8_t)b[i];
  return diff == 0;
}

// Extracts the token from "Authorization: Bearer <token>" (empty string if absent).
static String bearer_token() {
  String hdr = s_ctrl.header("Authorization");
  if (!hdr.startsWith("Bearer ")) return String();
  String t = hdr.substring(7);
  t.trim();
  return t;
}

// ===== HTML =====

static const char* CTRL_HTML_HEAD = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Claude Code Usage Monitor — Bridge</title>
<style>
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
         max-width: 480px; margin: 1rem auto; padding: 0 1rem;
         background: #0b0d10; color: #e6e6e6; }
  h1 { font-size: 1.25rem; color: #d97757; margin: 0.5rem 0; }
  .ver { color: #8a8f96; font-size: 0.8rem; margin-bottom: 1rem; }
  label { display: block; margin: 0.75rem 0 0.25rem; font-size: 0.9rem; color: #8a8f96; }
  input { width: 100%; padding: 0.5rem; font-size: 1rem; box-sizing: border-box;
          background: #14181d; color: #e6e6e6; border: 1px solid #4c5159;
          border-radius: 4px; }
  input:focus { outline: none; border-color: #38bdf8; }
  small { color: #666; font-size: 0.75rem; }
  button { width: 100%; padding: 0.75rem; font-size: 1rem; margin-top: 1rem;
           background: #d97757; color: #0b0d10; border: none; border-radius: 4px;
           font-weight: 600; cursor: pointer; }
  button:hover { background: #e8916f; }
  .row { display: flex; gap: 0.5rem; }
  .row > * { flex: 1; }
  .hint { color: #8a8f96; font-size: 0.85rem; margin: 0.5rem 0 1rem; }
</style></head><body>
<h1>Claude Code Usage Monitor</h1>
<div class="ver">v0.2.0 — Bridge target</div>
)HTML";

static void handle_root() {
  const AppConfig& c = Config::get();
  String body = CTRL_HTML_HEAD;
  body += "<div class='hint'>This device is <b>";
  body += escape_html(WiFi.localIP().toString());
  body += "</b> (<code>claudemonitor.local</code>). Point it at the laptop "
          "running <code>bridge.py</code> — no reboot, no AP needed.</div>";
  body += "<form action='/config' method='POST'>";

  body += "<div class='row'>";
  body += "<div><label>Bridge host</label>";
  body += "<input name='host' required value='" + escape_html(c.bridge_host) + "'></div>";
  body += "<div><label>Port</label>";
  body += "<input type='number' name='port' min='1' max='65535' value='" + String(c.bridge_port) + "'></div>";
  body += "</div>";

  body += "<label>Bridge token <small>(from bridge.py — required to apply)</small></label>";
  // Same principle as the portal: the token is never repopulated.
  body += "<input type='password' name='token' placeholder='"
          + String(c.bridge_token.length() ? "current token (unchanged if left blank)" : "paste the token")
          + "'>";

  body += "<button type='submit'>Update bridge</button></form>";
  body += "<div class='hint' style='margin-top:1rem'>The token authorises the "
          "change and stays the same unless you enter a new one.</div>";
  body += "</body></html>";

  s_ctrl.send(200, "text/html; charset=utf-8", body);
}

static void send_plain(int code, const char* msg) {
  s_ctrl.send(code, "text/plain; charset=utf-8", msg);
}

static void handle_config() {
  String host = s_ctrl.arg("host");   host.trim();
  String ftok = s_ctrl.arg("token");  ftok.trim();
  long   port_l = s_ctrl.arg("port").toInt();

  if (host.length() == 0) {
    send_plain(400, "host is required");
    return;
  }

  // Authorization: whoever changes the target must know the current token.
  // The token can come from the header (automatic bridge) or the form (browser).
  const String& stored = Config::get().bridge_token;
  if (stored.length() > 0) {
    String provided = bearer_token();
    if (provided.length() == 0) provided = ftok;
    if (!ct_equal(provided, stored)) {
      Serial.println("[Control] POST /config rejected — invalid token");
      send_plain(401, "invalid token");
      return;
    }
  }

  uint16_t port = (port_l > 0 && port_l < 65536) ? (uint16_t)port_l : Config::get().bridge_port;
  // Empty token = unchanged (as in the portal): preserve the saved one.
  String new_token = ftok.length() ? ftok : stored;

  Config::setBridge(host, port, new_token);
  Config::save();
  UsageClient_SetTarget(host.c_str(), port, new_token.c_str());

  Serial.printf("[Control] bridge re-pointed to %s:%u\n", host.c_str(), (unsigned)port);
  String ok = "ok: polling http://" + host + ":" + String(port) + "/usage";
  send_plain(200, ok.c_str());
}

// ===== API =====

void Control::start() {
  if (s_running) return;
  s_ctrl.collectHeaders(COLLECT_HEADERS, sizeof(COLLECT_HEADERS) / sizeof(COLLECT_HEADERS[0]));
  s_ctrl.on("/",       HTTP_GET,  handle_root);
  s_ctrl.on("/config", HTTP_POST, handle_config);
  s_ctrl.begin();
  s_running = true;
  Serial.printf("[Control] server active on http://%s/ (claudemonitor.local)\n",
                WiFi.localIP().toString().c_str());
}

void Control::loop() {
  if (!s_running) return;
  s_ctrl.handleClient();
}

void Control::stop() {
  if (!s_running) return;
  s_ctrl.stop();
  s_running = false;
  Serial.println("[Control] server stopped");
}

bool Control::isRunning() { return s_running; }
