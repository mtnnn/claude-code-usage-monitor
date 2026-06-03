#include "Portal.h"
#include "Config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_random.h>

static WebServer s_web(80);
static DNSServer s_dns;
static bool      s_running = false;
static String    s_ap_name;
static String    s_ap_ip;
static String    s_ap_pass;

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

static String escape_json(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else out += c;
    }
  }
  return out;
}

// ===== HTML form =====

static const char* PORTAL_HTML_HEAD = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Claude Code Usage Monitor — Setup</title>
<style>
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
         max-width: 480px; margin: 1rem auto; padding: 0 1rem;
         background: #0b0d10; color: #e6e6e6; }
  h1 { font-size: 1.25rem; color: #35d399; margin: 0.5rem 0; }
  .ver { color: #8a8f96; font-size: 0.8rem; margin-bottom: 1rem; }
  label { display: block; margin: 0.75rem 0 0.25rem; font-size: 0.9rem; color: #8a8f96; }
  input, select { width: 100%; padding: 0.5rem; font-size: 1rem; box-sizing: border-box;
                  background: #14181d; color: #e6e6e6; border: 1px solid #4c5159;
                  border-radius: 4px; }
  input:focus, select:focus { outline: none; border-color: #38bdf8; }
  small { color: #666; font-size: 0.75rem; }
  button { width: 100%; padding: 0.75rem; font-size: 1rem; margin-top: 1rem;
           background: #35d399; color: #0b0d10; border: none; border-radius: 4px;
           font-weight: 600; cursor: pointer; }
  button:hover { background: #4ade80; }
  .row { display: flex; gap: 0.5rem; }
  .row > * { flex: 1; }
  .hint { color: #8a8f96; font-size: 0.85rem; margin: 0.5rem 0 1rem; }
</style></head><body>
<h1>Claude Code Usage Monitor</h1>
<div class="ver">v0.2.0 — Setup</div>
<div class="hint">Configura WiFi e bridge. Il device farà reboot in modalità normale.</div>
<form action="/save" method="POST">
)HTML";

static const char* PORTAL_HTML_TAIL = R"HTML(
  <button type="submit">Salva e collegati</button>
</form>
<div class="hint" style="margin-top:1rem">
  Il token lo trovi nel terminale dove gira <code>bridge.py</code>
  (riga <code>token: ...</code>). Avvia il bridge prima di salvare.
</div>
<script>
  fetch('/scan').then(r => r.json()).then(nets => {
    const sel = document.getElementById('ssidSel');
    if (!sel) return;
    sel.innerHTML = '<option value="">— scegli rete —</option>';
    nets.sort((a,b) => b.rssi - a.rssi).forEach(n => {
      const o = document.createElement('option');
      o.value = n.ssid;
      o.textContent = n.ssid + ' (' + n.rssi + ' dBm' + (n.lock ? ' 🔒' : '') + ')';
      sel.appendChild(o);
    });
  }).catch(() => {});
</script>
</body></html>)HTML";

static void handle_root() {
  const AppConfig& c = Config::get();
  String body = PORTAL_HTML_HEAD;
  body += "<label>Rete WiFi (auto-scan)</label>";
  body += "<select id='ssidSel' name='ssid'><option value=''>— scansiono... —</option></select>";
  body += "<label>SSID manuale (se non in lista)</label>";
  body += "<input name='ssid_manual' placeholder='nome rete' value='" + escape_html(c.wifi_ssid) + "'>";
  body += "<label>Password WiFi</label>";
  body += "<input type='password' name='pass'>";

  body += "<div class='row'>";
  body += "<div><label>Bridge host</label>";
  body += "<input name='host' required value='" + escape_html(c.bridge_host) + "'></div>";
  body += "<div><label>Porta</label>";
  body += "<input type='number' name='port' min='1' max='65535' value='" + String(c.bridge_port) + "'></div>";
  body += "</div>";

  body += "<label>Bridge token <small>(da bridge.py)</small></label>";
  // Il token NON viene ripopolato: e' un segreto e non va riflesso nel form.
  // Se l'utente lascia vuoto al salvataggio, handle_save preserva quello esistente.
  body += "<input type='password' name='token' placeholder='"
          + String(c.bridge_token.length() ? "(invariato se lasci vuoto)" : "incolla il token")
          + "'>";

  body += "<label>Polling (ms) — min 1000, max 60000</label>";
  body += "<input type='number' name='poll' min='1000' max='60000' value='" + String(c.poll_ms) + "'>";

  body += PORTAL_HTML_TAIL;

  s_web.send(200, "text/html; charset=utf-8", body);
}

static void handle_scan() {
  int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false);
  String body = "[";
  bool first = true;
  for (int i = 0; i < n; i++) {
    if (!first) body += ",";
    first = false;
    body += "{\"ssid\":\"";
    body += escape_json(WiFi.SSID(i));
    body += "\",\"rssi\":";
    body += String(WiFi.RSSI(i));
    body += ",\"lock\":";
    body += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    body += "}";
  }
  body += "]";
  WiFi.scanDelete();
  s_web.send(200, "application/json", body);
}

static void handle_save() {
  String ssid = s_web.arg("ssid_manual");
  ssid.trim();
  if (ssid.length() == 0) ssid = s_web.arg("ssid");
  ssid.trim();
  String pass  = s_web.arg("pass");
  String host  = s_web.arg("host");   host.trim();
  String token = s_web.arg("token");  token.trim();
  // Token vuoto = "invariato": preserva quello gia' salvato (non lo
  // ripopoliamo nel form, quindi un re-save per cambiare solo il WiFi non
  // deve cancellarlo).
  if (token.length() == 0) token = Config::get().bridge_token;
  long   port_l = s_web.arg("port").toInt();
  long   poll_l = s_web.arg("poll").toInt();

  if (ssid.length() == 0 || host.length() == 0) {
    s_web.send(400, "text/html",
               "<h2>SSID e Bridge host sono obbligatori.</h2>"
               "<a href='/'>Torna al form</a>");
    return;
  }
  uint16_t port = (port_l > 0 && port_l < 65536) ? (uint16_t)port_l : 8787;
  uint32_t poll = (poll_l > 0) ? (uint32_t)poll_l : 5000;

  Config::setWifi(ssid, pass);
  Config::setBridge(host, port, token);
  Config::setPoll(poll);
  Config::save();

  String body =
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<title>Salvato</title>"
    "<style>body{font-family:sans-serif;background:#0b0d10;color:#e6e6e6;"
    "max-width:480px;margin:2rem auto;padding:0 1rem;}h2{color:#35d399;}</style>"
    "</head><body><h2>Salvato &mdash; reboot in corso</h2>"
    "<p>La board uscirà dalla modalità AP e si collegherà alla tua rete.</p>"
    "<p>Se il display non raggiunge il bridge entro 60 secondi, tieni premuto "
    "BOOT per &gt;5 secondi per rientrare in setup.</p>"
    "</body></html>";
  s_web.send(200, "text/html; charset=utf-8", body);
  s_web.client().flush();
  delay(700);
  ESP.restart();
}

// Catch-all: redirect a "/" per attivare la captive notification su OS moderni
// (iOS captive.apple.com, Android generate_204, Windows connecttest.txt).
static void handle_captive() {
  String loc = "http://" + s_ap_ip + "/";
  s_web.sendHeader("Location", loc, true);
  s_web.send(302, "text/plain", "");
}

// ===== Public API =====

void Portal::start() {
  // Disconnetti eventuale STA in corso (se entriamo in portal mid-run dopo
  // un fallimento WiFi persistente) e libera la radio per lo scan.
  WiFi.disconnect(true, true);
  // AP_STA, non WIFI_AP: in pure AP mode `WiFi.scanNetworks()` ritorna 0
  // (la radio STA è disattivata). Con AP_STA softAP funziona e lo scan
  // resta possibile dall'interfaccia STA.
  WiFi.mode(WIFI_AP_STA);
  uint8_t mac[6]; WiFi.macAddress(mac);
  char ap[24];
  snprintf(ap, sizeof(ap), "ClaudeMonitor-%02X%02X", mac[4], mac[5]);
  s_ap_name = ap;

  // Password WPA2 casuale (RNG hardware), mostrata sul display durante il
  // setup. NON derivata dal MAC: il BSSID viaggia in chiaro nei beacon e il
  // firmware e' open-source, quindi una derivazione dal MAC sarebbe
  // ricostruibile da un attaccante. Una pwd casuale per-sessione no.
  static const char PW_ALPHABET[] = "abcdefghjkmnpqrstuvwxyz23456789";
  const size_t alpha_n = sizeof(PW_ALPHABET) - 1;
  char pw[9];
  for (uint8_t i = 0; i < 8; i++) pw[i] = PW_ALPHABET[esp_random() % alpha_n];
  pw[8] = 0;
  s_ap_pass = pw;

  WiFi.softAP(ap, pw);
  s_ap_ip = WiFi.softAPIP().toString();

  // Catch-all DNS → l'IP dell'AP
  s_dns.setErrorReplyCode(DNSReplyCode::NoError);
  s_dns.start(53, "*", WiFi.softAPIP());

  s_web.on("/",     HTTP_GET,  handle_root);
  s_web.on("/save", HTTP_POST, handle_save);
  s_web.on("/scan", HTTP_GET,  handle_scan);
  s_web.onNotFound(handle_captive);
  s_web.begin();

  s_running = true;
  Serial.printf("[Portal] AP=%s pass=%s IP=%s — apri http://%s\n",
                ap, pw, s_ap_ip.c_str(), s_ap_ip.c_str());
}

void Portal::loop() {
  if (!s_running) return;
  s_dns.processNextRequest();
  s_web.handleClient();
}

bool   Portal::isRunning()  { return s_running; }
String Portal::apName()     { return s_ap_name; }
String Portal::apIp()       { return s_ap_ip;   }
String Portal::apPassword() { return s_ap_pass; }
