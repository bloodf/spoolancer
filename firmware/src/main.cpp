#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#ifndef SPOOLANCER_HEADLESS
#include <SPI.h>
#include <Adafruit_ST7789.h>
#endif
#include <esp_system.h>
#include <vector>
#include "Pins.h"
#include "Domain.h"
#include "Scale.h"
#include "Nfc.h"
#include "StatusUi.h"
#ifdef SPOOLANCER_HEADLESS
#include "Setup.h"
#endif
#ifdef SPOOLANCER_TOUCH
#include "TouchUi.h"
#endif

namespace {
constexpr size_t maxSpools = 100, maxRequest = 4096;
constexpr uint32_t sessionMs = 2UL * 60 * 60 * 1000;
#if defined(SPOOLANCER_TOUCH)
constexpr const char *deviceVariant = "touch-8048";
#elif defined(SPOOLANCER_OLED)
constexpr const char *deviceVariant = "oled-led";
#elif defined(SPOOLANCER_STATUS_LEDS)
constexpr const char *deviceVariant = "headless-led";
#elif defined(SPOOLANCER_HEADLESS)
constexpr const char *deviceVariant = "headless";
#else
constexpr const char *deviceVariant = "st7789";
#endif
struct Spool {
  String id, name, brand, material, color, tagUid;
  double empty = NAN, initial = 1000, density = 1.24, diameter = 1.75;
  double lastGross = NAN, lastRemaining = NAN;
  int remoteId = 0, vendorId = 0, filamentId = 0;
  String pending;
  uint32_t measurement = 0;
};
struct Config { String name = "Spoolancer", ssid, password, spoolman; } config;
struct Session { String token; uint32_t started = 0; } sessions[4];
std::vector<Spool> spools;
String activeId, pin, apName, nfcSpoolId, appMessage, syncMessage, syncError;
JsonDocument nfcMetadata;
Preferences preferences;
fs::LittleFSFS inventory;
WebServer server(80);
Scale scale;
HardwareSerial nfcSerial(1);
Nfc nfc(nfcSerial);
#ifndef SPOOLANCER_HEADLESS
SPIClass displaySpi(FSPI);
Adafruit_ST7789 display(&displaySpi, Pins::displayCs, Pins::displayDc, Pins::displayReset);
#endif
bool assetsReady = false, storeReady = false, activeHadLoad = false, apActive = false;
uint32_t nextNfc = 0, nextDisplay = 0, connectStarted = 0, reconnectAt = 0;
uint32_t nextLogin = 0, pinVisibleUntil = 0;
unsigned loginFailures = 0, sessionSlot = 0;
#ifdef SPOOLANCER_HEADLESS
Setup::Window setupWindow;
bool viaSetupAp() {
  if (!apActive || server.client().localIP() != WiFi.softAPIP()) return false;
  if (WiFi.status() == WL_CONNECTED && server.client().localIP() == WiFi.localIP()) return false;
  const IPAddress peer = server.client().remoteIP(), local = WiFi.softAPIP();
  return peer[0] == local[0] && peer[1] == local[1] && peer[2] == local[2] && peer[3] > 1 && peer[3] < 255;
}
#endif

bool saveConfig(const Config &value) {
  JsonDocument doc; doc["name"] = value.name; doc["ssid"] = value.ssid;
  doc["password"] = value.password; doc["spoolman"] = value.spoolman;
  String json; serializeJson(doc, json);
  return preferences.putString("config", json) == json.length();
}
bool saveCalibration() {
  JsonDocument doc; doc["zero"] = scale.zero; doc["factor"] = scale.countsPerGram;
  doc["zero_set"] = scale.zeroSet;
  String json; serializeJson(doc, json);
  return preferences.putString("calibration", json) == json.length();
}
bool zeroCradle() {
  const double previousZero = scale.zero; const bool previousSet = scale.zeroSet;
  if (!scale.tare()) return false;
  if (saveCalibration()) return true;
  scale.zero = previousZero; scale.zeroSet = previousSet;
  return false;
}
String randomHex(size_t bytes) {
  String result; result.reserve(bytes * 2);
  for (size_t i = 0; i < bytes; ++i) { char b[3]; snprintf(b, sizeof(b), "%02x", uint8_t(esp_random())); result += b; }
  return result;
}
void generatePin() { char out[9]; snprintf(out, sizeof(out), "%08lu", (unsigned long)(esp_random() % 100000000UL)); pin = out; }
Spool *findSpool(const String &id) {
  for (auto &spool : spools) if (spool.id == id) return &spool;
  return nullptr;
}
void renderStatusUi(bool force = false) {
#ifdef SPOOLANCER_STATUS_LEDS
  StatusUi::Snapshot state;
  state.gross = scale.gross(); state.calibrated = scale.calibrated();
  state.fresh = scale.fresh(); state.stable = scale.stable(); state.setup = apActive;
  const Spool *spool = findSpool(activeId);
  if (spool) {
    state.selected = true; state.empty = spool->empty; state.initial = spool->initial;
    state.material = spool->material.c_str();
  }
  const String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() :
                    (apActive ? WiFi.softAPIP().toString() : "");
  state.ip = ip.c_str();
  StatusUi::render(state, millis(), force);
#endif
}
void startStatusOperation(StatusModel::Operation operation) {
  StatusUi::start(operation, millis()); renderStatusUi(true);
}
void finishStatusOperation(bool success) {
  StatusUi::finish(success, millis()); renderStatusUi(true);
}
void numberOrNull(JsonVariant field, double value) { if (std::isfinite(value)) field.set(value); else field.set(nullptr); }
void spoolJson(JsonObject obj, const Spool &s, bool persisted = false) {
  obj["id"] = s.id; obj["name"] = s.name; obj["brand"] = s.brand;
  obj["material"] = s.material; obj["color"] = s.color;
  numberOrNull(obj["empty_weight_g"], s.empty); obj["initial_weight_g"] = s.initial;
  obj["density_g_cm3"] = s.density; obj["diameter_mm"] = s.diameter;
  if (s.remoteId) obj["spoolman_id"] = s.remoteId; else obj["spoolman_id"] = nullptr;
  numberOrNull(obj["remaining_weight_g"], s.lastRemaining);
  numberOrNull(obj["last_gross_g"], s.lastGross);
  obj["measurement_sequence"] = s.measurement; obj["tag_uid"] = s.tagUid;
  obj["sync_pending"] = !s.pending.isEmpty();
  if (persisted) { obj["vendor_id"] = s.vendorId; obj["filament_id"] = s.filamentId; obj["pending"] = s.pending; }
}
bool saveInventory() {
  if (!storeReady) return false;
  JsonDocument doc; doc["schema"] = 1;
  JsonArray array = doc["spools"].to<JsonArray>();
  for (const auto &s : spools) spoolJson(array.add<JsonObject>(), s, true);
  File file = inventory.open("/state.tmp", FILE_WRITE);
  if (!file) return false;
  const size_t expected = measureJson(doc);
  const size_t written = serializeJson(doc, file);
  file.flush(); file.close();
  if (written != expected) { inventory.remove("/state.tmp"); return false; }
  return inventory.rename("/state.tmp", "/state.json");
}
bool textField(JsonObjectConst obj, const char *key, String &out, size_t max, bool required = true) {
  if (!obj[key].is<const char *>()) return false;
  out = obj[key].as<String>(); out.trim();
  if ((required && out.isEmpty()) || out.length() > max) return false;
  for (size_t i = 0; i < out.length(); ++i) if (uint8_t(out[i]) < 32) return false;
  return true;
}
bool numericField(JsonObjectConst obj, const char *key, double &out, double low, double high) {
  if (!obj[key].is<double>()) return false;
  out = obj[key].as<double>(); return Domain::inRange(out, low, high);
}
bool validateSpool(JsonObjectConst obj, Spool &s) {
  if (!textField(obj, "name", s.name, 64) || !textField(obj, "brand", s.brand, 60) ||
      !textField(obj, "material", s.material, 30) || !textField(obj, "color", s.color, 7) ||
      !Domain::validColor(s.color.c_str()) ||
      !numericField(obj, "initial_weight_g", s.initial, 1, 5000) ||
      !numericField(obj, "density_g_cm3", s.density, .1, 5) ||
      !numericField(obj, "diameter_mm", s.diameter, .5, 5)) return false;
  if (obj["empty_weight_g"].isNull()) s.empty = NAN;
  else if (!numericField(obj, "empty_weight_g", s.empty, 0, 2000)) return false;
  if (obj["spoolman_id"].isNull()) s.remoteId = 0;
  else {
    if (!obj["spoolman_id"].is<int>() || obj["spoolman_id"].as<int>() < 1) return false;
    s.remoteId = obj["spoolman_id"].as<int>();
  }
  return true;
}
bool loadInventory() {
  if (!inventory.exists("/state.json")) return true;
  File file = inventory.open("/state.json", FILE_READ);
  if (!file || file.size() > 150000) return false;
  JsonDocument doc;
  if (deserializeJson(doc, file) || doc["schema"] != 1 || !doc["spools"].is<JsonArray>()) return false;
  const JsonArrayConst array = doc["spools"].as<JsonArrayConst>();
  if (array.size() > maxSpools) return false;
  std::vector<Spool> loaded;
  for (JsonObjectConst obj : array) {
    Spool s;
    if (!validateSpool(obj, s) || !obj["id"].is<const char *>() || !Domain::validId(obj["id"])) return false;
    s.id = obj["id"].as<String>();
    for (const auto &prior : loaded) if (prior.id == s.id) return false;
    s.tagUid = obj["tag_uid"] | "";
    s.vendorId = obj["vendor_id"] | 0; s.filamentId = obj["filament_id"] | 0;
    s.pending = obj["pending"] | "";
    s.lastGross = obj["last_gross_g"].is<double>() ? obj["last_gross_g"].as<double>() : NAN;
    s.lastRemaining = obj["remaining_weight_g"].is<double>() ? obj["remaining_weight_g"].as<double>() : NAN;
    s.measurement = obj["measurement_sequence"] | 0;
    loaded.push_back(s);
  }
  spools = std::move(loaded); return true;
}
void error(int code, const String &message) {
  JsonDocument doc; doc["error"] = message;
  String body; serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store"); server.send(code, "application/json", body);
}
void reply(JsonDocument &doc, int code = 200) {
  String body; serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store"); server.send(code, "application/json", body);
}
void ok(const String &message) { JsonDocument doc; doc["ok"] = true; doc["message"] = message; reply(doc); }
bool authorized() {
  const String header = server.header("Authorization");
  if (header.startsWith("Bearer ")) {
    const String token = header.substring(7);
    for (const auto &session : sessions)
      if (session.token.length() == 64 && token == session.token && uint32_t(millis() - session.started) < sessionMs) return true;
  }
  error(401, "Pair with your device PIN to make changes."); return false;
}
bool body(JsonDocument &doc) {
  if (!server.header("Content-Type").startsWith("application/json")) { error(415, "Send application/json."); return false; }
  const String raw = server.arg("plain");
  if (raw.length() > maxRequest || raw.isEmpty()) { error(413, "JSON body must be 1–4096 bytes."); return false; }
  if (deserializeJson(doc, raw, DeserializationOption::NestingLimit(6)) || !doc.is<JsonObject>()) {
    error(400, "Invalid JSON object."); return false;
  }
  return true;
}
bool confirmation(JsonDocument &doc) {
  if (!doc["confirm"].is<bool>() || !doc["confirm"].as<bool>()) { error(400, "Explicit confirmation is required."); return false; }
  return true;
}
Spool *requestedSpool(JsonDocument &doc) {
  if (!doc["spool_id"].is<const char *>()) { error(400, "spool_id is required."); return nullptr; }
  Spool *s = findSpool(doc["spool_id"].as<String>());
  if (!s) error(404, "Spool not found.");
  return s;
}
String measurementProblem(const Spool &s) {
  if (activeId != s.id) return "Select or scan this spool on the scale before saving.";
  if (!Domain::measurementAllowed(scale.gross(), s.empty, s.initial, scale.calibrated(), scale.stable(), scale.fresh())) {
    return "Need a fresh stable calibrated weight (20–2000 g), known empty-spool weight, and remaining filament within the initial weight.";
  }
  return "";
}
bool validMeasurement(const Spool &s) {
  const String problem = measurementProblem(s);
  if (problem.isEmpty()) return true;
  error(409, problem); return false;
}
bool recordMeasurement(Spool &s) {
  const double gross = scale.gross();
  startStatusOperation(StatusModel::Operation::Save);
  Spool previous = s;
  s.lastGross = gross; s.lastRemaining = s.lastGross - s.empty; ++s.measurement;
  const bool saved = saveInventory();
  if (!saved) s = previous;
  finishStatusOperation(saved); return saved;
}
void chooseSpool(const String &id) {
  activeId = id; activeHadLoad = scale.calibrated() && scale.fresh() && scale.gross() >= 20;
}
void nfcJson(JsonObject obj) {
  obj["available"] = nfc.available; obj["present"] = nfc.present;
  obj["uid"] = nfc.uid; obj["type"] = nfc.type; obj["writable"] = nfc.writable;
  obj["last_message"] = nfc.lastMessage; obj["last_error"] = nfc.lastError;
  if (nfcSpoolId.isEmpty()) obj["spool_id"] = nullptr; else obj["spool_id"] = nfcSpoolId;
  if (!nfcMetadata.isNull()) obj["metadata"].set(nfcMetadata);
}
void readNfcImpl(bool explicitRead) {
  const String previousUid = nfc.uid;
  nfcSpoolId = ""; nfcMetadata.clear();
  const bool scanned = nfc.scan(true);
  if (explicitRead || nfc.targets > 1 || (nfc.present && nfc.uid != previousUid)) chooseSpool("");
  if (!scanned || nfc.targets != 1) return;
  if (!nfc.payload.isEmpty()) {
    JsonDocument tag;
    if (deserializeJson(tag, nfc.payload, DeserializationOption::NestingLimit(3)) || tag["schema"] != 1 ||
        !tag["spool_id"].is<const char *>() || !Domain::validId(tag["spool_id"])) {
      nfc.writable = false; nfc.lastError = "Invalid Spoolancer tag record; writing refused."; return;
    }
    Spool parsed;
    if (!validateSpool(tag.as<JsonObjectConst>(), parsed)) {
      nfc.writable = false; nfc.lastError = "Invalid tag metadata; writing refused."; return;
    }
    parsed.id = tag["spool_id"].as<String>();
    spoolJson(nfcMetadata.to<JsonObject>(), parsed);
    if (findSpool(parsed.id)) nfcSpoolId = parsed.id;
  }
  // Originals remain read-only. A UID can only identify a record explicitly bound locally.
  if (nfcSpoolId.isEmpty()) for (const auto &s : spools) if (!s.tagUid.isEmpty() && s.tagUid == nfc.uid) nfcSpoolId = s.id;
  if (!nfcSpoolId.isEmpty() && (!scale.calibrated() || scale.gross() >= 20)) chooseSpool(nfcSpoolId);
}
void readNfc(bool explicitRead = false) {
  if (explicitRead) startStatusOperation(StatusModel::Operation::Nfc);
  readNfcImpl(explicitRead);
  if (explicitRead) finishStatusOperation(StatusModel::nfcReadSucceeded(nfc.present, nfc.targets, !nfc.lastError.isEmpty()));
}
String writeSpoolTagImpl(Spool &s, const String &expectedUid) {
  if (expectedUid.isEmpty()) return "Scan and confirm the target tag UID first.";
  for (const auto &other : spools) if (other.id != s.id && other.tagUid == expectedUid)
    return "This UID is already linked to another local spool. Unlink it before rewriting this inventory tag.";
  JsonDocument tag; spoolJson(tag.to<JsonObject>(), s);
  tag.remove("id"); tag.remove("spoolman_id"); tag.remove("remaining_weight_g"); tag.remove("last_gross_g");
  tag.remove("measurement_sequence"); tag.remove("tag_uid"); tag.remove("sync_pending");
  tag["schema"] = 1; tag["spool_id"] = s.id;
  String json; serializeJson(tag, json);
  if (!nfc.writeInventory(expectedUid, json)) return nfc.lastError;
  const String oldUid = s.tagUid; s.tagUid = nfc.uid;
  if (!saveInventory()) {
    s.tagUid = oldUid;
    return "Tag was verified, but local UID binding could not be saved. The tag still contains the spool ID.";
  }
  nfcSpoolId = s.id; chooseSpool(s.id); nextNfc = millis() + 2000;
  return "";
}
String writeSpoolTag(Spool &s, const String &expectedUid) {
  startStatusOperation(StatusModel::Operation::Nfc);
  const String problem = writeSpoolTagImpl(s, expectedUid);
  finishStatusOperation(problem.isEmpty()); return problem;
}
void statusRoute() {
  JsonDocument doc; doc["mode"] = "device"; doc["device"]["name"] = config.name;
  doc["device"]["firmware"] = "0.5.0-prototype"; doc["device"]["storage_ready"] = storeReady;
  doc["device"]["variant"] = deviceVariant;
#ifdef SPOOLANCER_HEADLESS
#ifdef SPOOLANCER_TOUCH
  doc["device"]["headless"] = false;
#elif defined(SPOOLANCER_OLED)
  doc["device"]["headless"] = false;
  doc["device"]["oled_available"] = StatusUi::oledAvailable();
#elif defined(SPOOLANCER_STATUS_LEDS)
  doc["device"]["headless"] = true;
#else
  doc["device"]["headless"] = true;
#endif
  JsonObject setup = doc["setup"].to<JsonObject>();
  setup["ap_active"] = apActive; setup["open_network"] = apActive;
  setup["via_ap"] = viaSetupAp(); setup["enrollment_required"] = pin.isEmpty();
  setup["enrollment_armed"] = apActive && pin.isEmpty() && setupWindow.armed(millis());
  setup["enrollment_seconds"] = (Setup::remaining(setupWindow.enrollmentDeadline, millis()) + 999) / 1000;
  setup["ap_seconds"] = (setupWindow.apRemaining(millis()) + 999) / 1000;
  setup["warning"] = "Temporary setup network has no password; use it in a trusted location. Wi-Fi credentials are sent over local HTTP.";
#else
  doc["device"]["headless"] = false;
#endif
  JsonObject weight = doc["scale"].to<JsonObject>();
  numberOrNull(weight["gross_g"], scale.gross()); weight["calibrated"] = scale.calibrated();
  weight["stable"] = scale.stable(); weight["fresh"] = scale.fresh(); weight["zero_set"] = scale.zeroSet;
  Spool *s = findSpool(activeId);
  const double net = s && std::isfinite(s->empty) ? scale.gross() - s->empty : NAN;
  numberOrNull(weight["net_g"], net); numberOrNull(weight["remaining_g"], net);
  if (activeId.isEmpty()) doc["active_spool_id"] = nullptr; else doc["active_spool_id"] = activeId;
  // Public status reports hardware, not inventory metadata or configuration secrets.
  JsonObject tag = doc["nfc"].to<JsonObject>();
  tag["available"] = nfc.available; tag["present"] = nfc.present; tag["uid"] = nfc.uid;
  tag["type"] = nfc.type; tag["writable"] = nfc.writable;
  tag["last_message"] = nfc.lastMessage; tag["last_error"] = nfc.lastError;
  doc["network"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["network"]["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["network"]["ap_active"] = apActive;
  doc["spoolman"]["configured"] = !config.spoolman.isEmpty();
  doc["spoolman"]["last_message"] = syncMessage; doc["spoolman"]["last_error"] = syncError;
  doc["spoolman"]["pending"] = s && !s->pending.isEmpty();
  doc["message"] = appMessage; reply(doc);
}
bool validServerUrl(const String &url) {
  if (url.isEmpty()) return true;
  if (!url.startsWith("http://") || url.length() > 200 || url.length() < 8) return false;
  for (char c : url) if (uint8_t(c) <= 32 || c == '@' || c == '?' || c == '#' || c == '\\') return false;
  String authority = url.substring(7);
  const int slash = authority.indexOf('/');
  if (slash >= 0) authority = authority.substring(0, slash);
  const int colon = authority.indexOf(':');
  String host = colon < 0 ? authority : authority.substring(0, colon);
  IPAddress address;
  if (!address.fromString(host) || address[0] == 0 || address[0] >= 224) return false;
  if (colon >= 0) {
    const String port = authority.substring(colon + 1);
    if (port.isEmpty() || port.length() > 5) return false;
    for (char c : port) if (c < '0' || c > '9') return false;
    if (port.toInt() < 1 || port.toInt() > 65535) return false;
  }
  return true;
}
bool remoteRequest(const char *method, const String &path, JsonDocument *input, JsonDocument &output, int &code) {
  code = 0;
  if (WiFi.status() != WL_CONNECTED || config.spoolman.isEmpty()) { syncError = "Connect Wi-Fi and configure a local Spoolman URL first."; return false; }
  WiFiClient client;
  HTTPClient http;
  http.useHTTP10(true);
  http.setConnectTimeout(1000); http.setTimeout(1500); http.setReuse(false);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(client, config.spoolman + "/api/v1" + path)) { syncError = "Invalid Spoolman address."; return false; }
  http.addHeader("Content-Type", "application/json");
  String data;
  if (input) serializeJson(*input, data);
  code = http.sendRequest(method, data);
  if (code < 200 || code >= 300) {
    syncError = code <= 0 ? "Spoolman connection timed out or failed." : "Spoolman returned HTTP " + String(code) + ".";
    http.end(); return false;
  }
  if (http.getSize() > 20000) { syncError = "Spoolman response exceeds device limit."; http.end(); return false; }
  // Bound the body regardless of Content-Length. No remote payload is reflected in errors.
  String response; response.reserve(1024);
  WiFiClient *stream = http.getStreamPtr();
  const uint32_t end = millis() + 1500;
  int remaining = http.getSize();
  while (int32_t(end - millis()) > 0 && (http.connected() || stream->available()) && remaining != 0) {
    if (stream->available()) {
      response += char(stream->read()); if (remaining > 0) --remaining;
      if (response.length() > 20000) break;
    } else { scale.tick(); delay(1); }
  }
  http.end();
  if (response.length() > 20000 || remaining > 0 || deserializeJson(output, response, DeserializationOption::NestingLimit(8))) {
    syncError = "Spoolman returned an incomplete or invalid JSON response."; return false;
  }
  return true;
}
bool persistPending(Spool &s, const String &stage) {
  s.pending = stage;
  if (saveInventory()) return true;
  syncError = "Cannot persist the remote-operation journal; nothing further was sent."; return false;
}
bool createRemote(Spool &s, const char *kind, JsonDocument &request, int &id) {
  if (WiFi.status() != WL_CONNECTED || config.spoolman.isEmpty()) {
    syncError = "Connect Wi-Fi and configure a local Spoolman URL first."; return false;
  }
  if (!persistPending(s, kind)) return false;
  JsonDocument response; int code;
  const bool good = remoteRequest("POST", String("/") + kind, &request, response, code);
  if (good && response["id"].is<int>() && response["id"].as<int>() > 0) {
    id = response["id"].as<int>();
    s.pending = "";
    if (saveInventory()) {
      if (String(kind) == "spool" && (!response["remaining_weight"].is<double>() ||
          std::abs(response["remaining_weight"].as<double>() - s.lastRemaining) > .1)) {
        syncError = "Spoolman spool was created and linked, but its returned remaining weight differs. Check it and sync again."; return false;
      }
      return true;
    }
    s.pending = kind;
    syncError = "Remote creation succeeded but local saving failed. Check Spoolman before any retry."; return false;
  }
  if (code >= 400 && code < 500) {
    s.pending = "";
    if (!saveInventory()) { s.pending = kind; syncError += " Could not save the rejection; check the server before retrying."; }
  } else {
    syncError = "Creation outcome is unknown. Check Spoolman and enter the existing Spoolman spool ID; automatic retries are blocked.";
  }
  return false;
}
bool syncSpoolImpl(Spool &s) {
  syncError = ""; syncMessage = "";
  if (!s.pending.isEmpty()) { syncError = "A prior creation is unresolved. Check Spoolman and link its existing spool ID before retrying."; return false; }
  if (!s.remoteId) {
    if (!s.vendorId) {
      // Exact equality is checked locally; never select a partial-name match.
      JsonDocument vendors; int code;
      String encoded;
      for (uint8_t c : s.brand) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.') encoded += char(c);
        else { char h[4]; snprintf(h, sizeof(h), "%%%02X", c); encoded += h; }
      }
      if (!remoteRequest("GET", "/vendor?limit=100&name=" + encoded, nullptr, vendors, code) || !vendors.is<JsonArray>()) return false;
      for (JsonObjectConst item : vendors.as<JsonArrayConst>()) {
        if (item["name"].is<const char *>() && s.brand.equalsIgnoreCase(item["name"].as<String>()) && item["id"].is<int>()) {
          s.vendorId = item["id"].as<int>(); break;
        }
      }
      if (!s.vendorId) {
        JsonDocument request; request["name"] = s.brand;
        if (!createRemote(s, "vendor", request, s.vendorId)) return false;
      } else if (!saveInventory()) { syncError = "Cannot save the vendor link."; return false; }
    }
    if (!s.filamentId) {
      JsonDocument request; request["name"] = s.name; request["vendor_id"] = s.vendorId;
      request["material"] = s.material; request["color_hex"] = s.color.substring(1);
      request["density"] = s.density; request["diameter"] = s.diameter;
      request["weight"] = s.initial; request["spool_weight"] = s.empty;
      request["comment"] = "Spoolancer record " + s.id;
      if (!createRemote(s, "filament", request, s.filamentId)) return false;
    }
    JsonDocument request; request["filament_id"] = s.filamentId;
    request["initial_weight"] = s.initial; request["spool_weight"] = s.empty;
    request["remaining_weight"] = s.lastRemaining; request["comment"] = "Spoolancer record " + s.id;
    if (!createRemote(s, "spool", request, s.remoteId)) return false;
    syncMessage = "Created Spoolman spool " + String(s.remoteId) + " with the measured remaining weight.";
    return true;
  }
  JsonDocument request, response; int code;
  request["remaining_weight"] = s.lastRemaining;
  if (!remoteRequest("PATCH", "/spool/" + String(s.remoteId), &request, response, code)) {
    syncError += " The absolute update may have reached the server; retrying the same saved weight does not subtract consumption twice.";
    return false;
  }
  if (response["id"] != s.remoteId || !response["remaining_weight"].is<double>() ||
      std::abs(response["remaining_weight"].as<double>() - s.lastRemaining) > .1) {
    syncError = "Spoolman did not confirm the requested remaining weight."; return false;
  }
  syncMessage = "Spoolman remaining weight updated."; return true;
}
bool syncSpool(Spool &s) {
  startStatusOperation(StatusModel::Operation::Sync);
  const bool synced = syncSpoolImpl(s);
  finishStatusOperation(synced); return synced;
}
#ifdef SPOOLANCER_TOUCH
void renderTouch() {
  TouchUi::State state;
  state.gross = scale.gross(); state.stable = scale.stable(); state.calibrated = scale.calibrated();
  state.tagUid = nfc.uid; state.tagType = nfc.type;
  state.ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : (apActive ? WiFi.softAPIP().toString() : "");
  state.pin = pin; state.message = appMessage;
  if (pin.isEmpty() && apActive) {
    state.message = "Open setup Wi-Fi " + apName + ". Hold/release BOOT 2s, then choose a PIN in the browser.";
    if (setupWindow.armed(millis())) state.message = "Setup enrollment armed. Submit your chosen PIN in the browser within 60s.";
  }
  Spool *spool = findSpool(activeId);
  if (spool) {
    state.spoolId = spool->id; state.name = spool->name; state.brand = spool->brand;
    state.material = spool->material; state.color = spool->color;
    const String problem = measurementProblem(*spool);
    if (problem.isEmpty()) state.remaining = state.gross - spool->empty;
    else if (state.message.isEmpty() || state.message == "Spool selected.") state.message = problem;
    state.canWrite = storeReady && nfc.present && nfc.writable;
    state.canSave = storeReady && problem.isEmpty();
    state.canSync = state.canSave && spool->pending.isEmpty() && !config.spoolman.isEmpty() && WiFi.status() == WL_CONNECTED;
  }
  TouchUi::render(state);
}
void pollTouch() {
  const TouchUi::Command command = TouchUi::poll();
  using Action = TouchUi::Action;
  if (command.action == Action::None) return;
  if (command.action == Action::Previous || command.action == Action::Next) {
    if (spools.empty()) { appMessage = "Add a spool in the browser first."; return; }
    size_t at = command.action == Action::Next ? spools.size() - 1 : 0;
    for (size_t i = 0; i < spools.size(); ++i) if (spools[i].id == activeId) at = i;
    at = command.action == Action::Next ? (at + 1) % spools.size() : (at + spools.size() - 1) % spools.size();
    chooseSpool(spools[at].id); appMessage = "Spool selected."; return;
  }
  if (command.action == Action::Read) {
    readNfc(true); nextNfc = millis() + 2000;
    appMessage = nfc.lastError.isEmpty() ? nfc.lastMessage : nfc.lastError; return;
  }
  if (command.action == Action::Zero) {
    if (zeroCradle()) { chooseSpool(""); appMessage = "Empty cradle zero saved."; }
    else appMessage = "Cannot zero/save: keep the EMPTY cradle still and check storage.";
    return;
  }
  Spool *spool = findSpool(command.spoolId);
  if (!spool || activeId != command.spoolId) { appMessage = "Spool selection changed. Review and confirm again."; return; }
  if (command.action == Action::Write) {
    appMessage = "Writing inventory tag. Keep the spool and tag still."; renderTouch();
    const String problem = writeSpoolTag(*spool, command.expectedUid);
    appMessage = problem.isEmpty() ? "Inventory tag written and read-back verified." : problem; return;
  }
  if (command.action == Action::Save || command.action == Action::Sync) {
    const String problem = measurementProblem(*spool);
    if (!problem.isEmpty()) { appMessage = problem; return; }
    if (command.action == Action::Sync && !spool->pending.isEmpty()) {
      appMessage = "Prior Spoolman creation is unresolved. Check the server and link its existing spool ID in the browser."; return;
    }
    if (!recordMeasurement(*spool)) { appMessage = "Cannot persist measurement; no remote update was sent."; return; }
    if (command.action == Action::Save) { appMessage = "Measurement saved on the scale."; return; }
    appMessage = "Syncing saved weight to Spoolman. Please wait."; renderTouch();
    appMessage = syncSpool(*spool) ? syncMessage : syncError;
  }
}
#endif
void startAp() {
  if (apActive) return;
#ifdef SPOOLANCER_HEADLESS
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  apActive = WiFi.softAP(apName.c_str());
  if (apActive) setupWindow.begin(millis());
  Serial.printf("Temporary OPEN setup Wi-Fi: %s\nOpen http://192.168.4.1 in a trusted location.\n", apName.c_str());
#else
  WiFi.mode(WIFI_AP_STA);
  apActive = WiFi.softAP(apName.c_str(), pin.c_str());
  pinVisibleUntil = millis() + 10UL * 60 * 1000;
  Serial.printf("Setup Wi-Fi: %s\nAP password / device PIN: %s\nOpen http://192.168.4.1\n", apName.c_str(), pin.c_str());
#endif
}
void connectWifi() {
#ifdef SPOOLANCER_HEADLESS
  if (pin.isEmpty() || config.ssid.isEmpty()) return;
#else
  if (config.ssid.isEmpty()) { startAp(); return; }
#endif
  WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(config.ssid.c_str(), config.password.c_str());
  connectStarted = millis();
}
void setupRoutes() {
  const char *headers[] = {"Authorization", "Content-Type", "Content-Length", "Origin"};
  server.collectHeaders(headers, 4);
  server.on("/api/status", HTTP_GET, statusRoute);
  server.on("/api/session", HTTP_POST, [] {
    if (int32_t(nextLogin - millis()) > 0) { error(429, "Wait before trying the PIN again."); return; }
    nextLogin = millis() + 1500;
    JsonDocument doc; if (!body(doc)) return;
#ifdef SPOOLANCER_HEADLESS
    if (doc["setup"] == true) {
      if (!setupWindow.consume(millis(), pin.isEmpty(), viaSetupAp(), WiFi.status() != WL_CONNECTED,
                               Setup::trustedHeaders(server.hostHeader().c_str(), server.header("Origin").c_str()))) {
        error(403, "Join the temporary setup Wi-Fi, open http://192.168.4.1, then hold BOOT for 2 seconds and release it. Enrollment lasts 60 seconds."); return;
      }
      if (!doc["pin"].is<const char *>() || !Setup::validPin(doc["pin"].as<const char *>())) {
        error(400, "Choose exactly 8 digits. Hold BOOT for 2 seconds and release it again before another enrollment attempt."); return;
      }
      const String chosen = doc["pin"].as<String>();
      if (preferences.putString("pin", chosen) != chosen.length()) {
        error(507, "Could not save the device PIN. Hold BOOT for 2 seconds and release it again to retry."); return;
      }
      pin = chosen;
      appMessage = "Device PIN saved. Configure Wi-Fi, then reconnect your phone to the same LAN.";
    } else if (pin.isEmpty()) {
      error(403, "Complete physical setup and choose your device PIN first."); return;
    }
#endif
    if (!doc["pin"].is<const char *>() || doc["pin"].as<String>() != pin) {
      if (++loginFailures >= 5) { nextLogin = millis() + 60000; loginFailures = 0; }
      error(401, "Incorrect device PIN."); return;
    }
    loginFailures = 0;
    Session &session = sessions[sessionSlot++ % 4]; session.token = randomHex(32); session.started = millis();
    JsonDocument result; result["token"] = session.token; result["expires_in"] = sessionMs / 1000; reply(result);
  });
  server.on("/api/spools", HTTP_GET, [] {
    if (!authorized()) return;
    JsonDocument doc; JsonArray list = doc["spools"].to<JsonArray>();
    for (const auto &s : spools) spoolJson(list.add<JsonObject>(), s);
    reply(doc);
  });
  server.on("/api/spools", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    if (!storeReady) { error(503, "Inventory storage is unavailable."); return; }
    Spool *existing = nullptr;
    if (!doc["id"].isNull()) {
      if (!doc["id"].is<const char *>() || !(existing = findSpool(doc["id"].as<String>()))) { error(404, "Spool not found."); return; }
    }
    Spool value = existing ? *existing : Spool();
    if (!validateSpool(doc.as<JsonObjectConst>(), value)) { error(400, "Invalid spool fields. Check names, color, weights, density and diameter."); return; }
    if (!existing && spools.size() >= maxSpools) { error(409, "Device inventory limit is 100 spools."); return; }
    if (existing && existing->remoteId > 0 && value.remoteId != existing->remoteId) {
      value.vendorId = value.filamentId = 0; value.pending = "";
    }
    if (value.remoteId > 0) value.pending = "";
    if (doc["tag_uid"].is<const char *>()) {
      // Explicit binding of a scanned original identifies it without modifying that tag.
      String requested = doc["tag_uid"].as<String>();
      if (!requested.isEmpty() && (!nfc.scan(false) || requested != nfc.uid || nfc.targets != 1)) { error(409, "Tag changed or multiple tags detected. Rescan before linking its UID."); return; }
      for (const auto &s : spools) if (s.id != value.id && !requested.isEmpty() && s.tagUid == requested) { error(409, "This tag already identifies another local spool."); return; }
      value.tagUid = requested;
    }
    Spool before;
    if (existing) { before = *existing; *existing = value; }
    else { value.id = randomHex(8); spools.push_back(value); }
    if (!saveInventory()) {
      if (existing) *existing = before; else spools.pop_back();
      error(507, "Could not save the inventory."); return;
    }
    JsonDocument result; result["ok"] = true; spoolJson(result["spool"].to<JsonObject>(), value); reply(result);
  });
  server.on("/api/active-spool", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    if (doc["id"].isNull()) { chooseSpool(""); ok("Spool selection cleared."); return; }
    if (!doc["id"].is<const char *>() || !findSpool(doc["id"].as<String>())) { error(404, "Spool not found."); return; }
    chooseSpool(doc["id"].as<String>()); ok("Spool selected.");
  });
  server.on("/api/measurements", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    Spool *s = requestedSpool(doc); if (!s || !validMeasurement(*s)) return;
    if (!recordMeasurement(*s)) { error(507, "Cannot persist measurement; no remote update was sent."); return; }
    JsonDocument result; result["ok"] = true; result["gross_g"] = s->lastGross;
    result["remaining_weight_g"] = s->lastRemaining; result["message"] = "Measurement saved on the scale."; reply(result);
  });
  server.on("/api/scale/tare", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    if (!zeroCradle()) { error(409, "Could not zero and save calibration. Keep the empty cradle still and check device storage."); return; }
    chooseSpool("");
    ok("Empty cradle zero saved. This is not the empty-spool weight.");
  });
  server.on("/api/scale/calibrate", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    double grams;
    const double previousFactor = scale.countsPerGram;
    if (!numericField(doc.as<JsonObjectConst>(), "reference_g", grams, 100, 2000) || !scale.calibrate(grams)) {
      error(409, "Zero the empty cradle first, then place a known 100–2000 g reference and keep it still."); return;
    }
    if (!saveCalibration()) { scale.countsPerGram = previousFactor; error(507, "Could not save calibration."); return; }
    ok("Calibration saved. Verify with independent known weights.");
  });
  server.on("/api/nfc/read", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    readNfc(true); nextNfc = millis() + 2000;
    if (!nfc.present || !nfc.lastError.isEmpty()) { error(409, nfc.lastError.isEmpty() ? "No tag detected. Align one tag with the reader." : nfc.lastError); return; }
    JsonDocument result; result["ok"] = true; nfcJson(result["nfc"].to<JsonObject>());
    if (nfcSpoolId.isEmpty()) result["spool_id"] = nullptr; else result["spool_id"] = nfcSpoolId;
    result["message"] = nfc.lastMessage; reply(result);
  });
  server.on("/api/nfc/write", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    Spool *s = requestedSpool(doc); if (!s) return;
    if (!doc["expected_uid"].is<const char *>() || doc["expected_uid"].as<String>().isEmpty()) { error(400, "Scan and confirm the target tag UID first."); return; }
    const String problem = writeSpoolTag(*s, doc["expected_uid"].as<String>());
    if (!problem.isEmpty()) { error(409, problem); return; }
    ok("Tag written and read-back verified. Printer tags were not modified.");
  });
  server.on("/api/config", HTTP_GET, [] {
    if (!authorized()) return;
    JsonDocument doc; doc["device_name"] = config.name; doc["wifi_ssid"] = config.ssid;
    doc["spoolman_url"] = config.spoolman; doc["has_wifi_password"] = !config.password.isEmpty(); reply(doc);
  });
  server.on("/api/config", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    Config changed = config;
    if (!doc["device_name"].isUnbound() && !textField(doc.as<JsonObjectConst>(), "device_name", changed.name, 48)) { error(400, "Device name must be 1–48 characters."); return; }
    if (!doc["wifi_ssid"].isUnbound() && !textField(doc.as<JsonObjectConst>(), "wifi_ssid", changed.ssid, 32, false)) { error(400, "SSID must be at most 32 bytes."); return; }
    if (!doc["wifi_password"].isUnbound()) {
      if (!doc["wifi_password"].is<const char *>()) { error(400, "Invalid Wi-Fi password."); return; }
      changed.password = doc["wifi_password"].as<String>();
      if (!changed.password.isEmpty() && (changed.password.length() < 8 || changed.password.length() > 63)) { error(400, "Wi-Fi password must be 8–63 bytes, or empty for an open network."); return; }
    }
    if (!doc["clear_wifi_password"].isUnbound() && !doc["clear_wifi_password"].is<bool>()) { error(400, "clear_wifi_password must be true or false."); return; }
    if (doc["clear_wifi_password"] == true) changed.password = "";
    if (!doc["spoolman_url"].isUnbound()) {
      if (!doc["spoolman_url"].is<const char *>()) { error(400, "Invalid Spoolman URL."); return; }
      changed.spoolman = doc["spoolman_url"].as<String>(); changed.spoolman.trim();
      while (changed.spoolman.endsWith("/")) changed.spoolman.remove(changed.spoolman.length() - 1);
      if (!validServerUrl(changed.spoolman)) { error(400, "Use http://numeric-IPv4:port for the local Spoolman server; DNS, HTTPS and authenticated proxies are not supported yet."); return; }
      if (changed.spoolman != config.spoolman) {
        for (const auto &s : spools) if (s.remoteId || s.vendorId || s.filamentId || !s.pending.isEmpty()) {
          error(409, "Spoolman links already exist. Changing servers requires migrating or clearing those links first."); return;
        }
      }
    }
    const bool reconnect = changed.ssid != config.ssid || changed.password != config.password;
    if (changed.ssid.isEmpty()) changed.password = "";
    if (!saveConfig(changed)) { error(507, "Could not save configuration."); return; }
    config = changed;
    JsonDocument result; result["ok"] = true; result["reconnect_required"] = reconnect;
    result["ap_ssid"] = apName; result["ip"] = "192.168.4.1";
#ifdef SPOOLANCER_HEADLESS
    result["message"] = reconnect ? "Configuration saved; Wi-Fi will reconnect. The temporary setup network closes after connection or its 10-minute limit. Hold BOOT 8 seconds while running to recover." : "Configuration saved.";
#else
    result["message"] = reconnect ? "Configuration saved; Wi-Fi will reconnect. Setup AP remains available if connection fails." : "Configuration saved.";
#endif
    reply(result);
    if (reconnect) reconnectAt = millis() + 1000;
  });
  server.on("/api/spoolman/test", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument input; if (!body(input)) return;
    JsonDocument output; int code; syncError = "";
    startStatusOperation(StatusModel::Operation::Sync);
    const bool reached = remoteRequest("GET", "/spool?limit=1", nullptr, output, code) && output.is<JsonArray>();
    finishStatusOperation(reached);
    if (!reached) { error(502, syncError.isEmpty() ? "Unexpected Spoolman API response." : syncError); return; }
    syncMessage = "Spoolman API is reachable."; ok(syncMessage);
  });
  server.on("/api/spoolman/sync", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    Spool *s = requestedSpool(doc); if (!s || !validMeasurement(*s)) return;
    if (!s->pending.isEmpty()) { error(409, "Unresolved Spoolman creation. Check the server and enter its existing spool ID."); return; }
    if (!recordMeasurement(*s)) { error(507, "Cannot persist measurement; no remote update was sent."); return; }
    if (!syncSpool(*s)) { error(502, syncError.isEmpty() ? "Spoolman operation failed." : syncError); return; }
    JsonDocument result; result["ok"] = true; result["spoolman_id"] = s->remoteId; result["message"] = syncMessage; reply(result);
  });
  auto asset = [](const char *path, const char *mime) {
    if (!assetsReady || !LittleFS.exists(path)) { error(503, "Device web assets are missing. Build and upload the LittleFS asset image."); return; }
    File file = LittleFS.open(path, FILE_READ);
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Content-Security-Policy", "default-src 'self'; connect-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; frame-ancestors 'none'");
    server.streamFile(file, mime); file.close();
  };
  server.on("/", HTTP_GET, [asset] { asset("/index.html", "text/html"); });
  server.on("/index.html", HTTP_GET, [asset] { asset("/index.html", "text/html"); });
  server.on("/app.js", HTTP_GET, [asset] { asset("/app.js", "text/javascript"); });
  server.on("/logic.js", HTTP_GET, [asset] { asset("/logic.js", "text/javascript"); });
  server.on("/styles.css", HTTP_GET, [asset] { asset("/styles.css", "text/css"); });
  server.on("/favicon.svg", HTTP_GET, [asset] { asset("/favicon.svg", "image/svg+xml"); });
  server.onNotFound([] { error(404, "Not found."); });
  server.begin();
}
#ifndef SPOOLANCER_HEADLESS
void displayStatus() {
  display.fillScreen(ST77XX_BLACK); display.setCursor(10, 8); display.setTextSize(2);
  display.setTextColor(ST77XX_CYAN); display.println(config.name.substring(0, 23));
  display.setTextColor(ST77XX_WHITE);
  const double gross = scale.gross();
  display.setTextSize(3); display.setCursor(10, 40);
  if (std::isfinite(gross)) { display.print(gross, 1); display.println(" g"); }
  else { display.setTextSize(2); display.println(scale.calibrated() ? "Waiting for sensor" : "Calibration needed"); }
  display.setTextSize(2); display.setCursor(10, 78);
  Spool *s = findSpool(activeId);
  if (s) {
    display.println(s->name.substring(0, 24));
    if (std::isfinite(s->empty) && std::isfinite(gross)) { display.print("Filament: "); display.print(gross - s->empty, 1); display.println(" g"); }
    else display.println("Set empty-spool weight");
  } else display.println("Scan / select a spool");
  display.setTextSize(1); display.setCursor(10, 134);
  display.println(scale.stable() ? "Weight stable" : "Waiting for stable weight");
  display.println(nfc.available ? nfc.lastMessage.substring(0, 48) : nfc.lastError.substring(0, 48));
  display.setCursor(10, 166);
  display.print("http://"); display.println(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "192.168.4.1");
  if (int32_t(pinVisibleUntil - millis()) > 0 || config.ssid.isEmpty()) {
    display.println(apActive ? apName : "Pair device in browser");
    display.print("Device PIN / AP password: "); display.println(pin);
  } else display.println("Hold PREVIOUS 3s to show device PIN");
  display.println("Hold SELECT 2s: zero EMPTY cradle");
  display.println("Hold all buttons 8s: reset Wi-Fi/PIN");
}
void buttons() {
  static uint32_t previousDown = 0, selectDown = 0, allDown = 0, lastCycle = 0;
  static bool selectDone = false, previousDone = false, nextWasDown = false;
  const bool previous = digitalRead(Pins::previous) == LOW;
  const bool next = digitalRead(Pins::next) == LOW;
  const bool select = digitalRead(Pins::select) == LOW;
  const uint32_t now = millis();
  if (previous && next && select) {
    if (!allDown) allDown = now;
    if (now - allDown > 8000) {
      Config reset = config; reset.ssid = reset.password = "";
      if (!saveConfig(reset)) { appMessage = "Could not save Wi-Fi reset."; allDown = now; return; }
      generatePin(); preferences.putString("pin", pin);
      config = reset; for (auto &s : sessions) s.token = "";
      WiFi.disconnect(true, true); WiFi.softAPdisconnect(true); apActive = false; startAp();
      appMessage = "Wi-Fi and device PIN reset. Inventory and calibration kept.";
      allDown = now; selectDone = previousDone = true;
    }
    return;
  }
  allDown = 0;
  if (previous) {
    if (!previousDown) previousDown = now;
    if (!previousDone && now - previousDown > 3000) { pinVisibleUntil = now + 60000; previousDone = true; }
  } else {
    if (previousDown && !previousDone && now - previousDown > 40 && !spools.empty()) {
      size_t at = 0; for (size_t i = 0; i < spools.size(); ++i) if (spools[i].id == activeId) at = i;
      chooseSpool(spools[(at + spools.size() - 1) % spools.size()].id);
    }
    previousDown = 0; previousDone = false;
  }
  if (next && !nextWasDown && now - lastCycle > 200 && !spools.empty()) {
    size_t at = spools.size() - 1; for (size_t i = 0; i < spools.size(); ++i) if (spools[i].id == activeId) at = i;
    chooseSpool(spools[(at + 1) % spools.size()].id); lastCycle = now;
  }
  nextWasDown = next;
  if (select) {
    if (!selectDown) selectDown = now;
    if (!selectDone && now - selectDown > 2000) {
      if (zeroCradle()) { chooseSpool(""); appMessage = "Cradle zero saved."; }
      else appMessage = "Cannot zero: keep empty cradle still and check sensor.";
      selectDone = true;
    }
  } else { selectDown = 0; selectDone = false; }
}
#else
void buttons() {
  static Setup::Button button;
  const Setup::Gesture gesture = button.update(digitalRead(Pins::wifiReset) == LOW, millis());
  if (gesture == Setup::Gesture::ArmEnrollment && apActive && pin.isEmpty()) {
    if (setupWindow.arm(millis())) appMessage = "Choose your 8-digit device PIN in the setup browser within 60 seconds.";
  }
  if (gesture == Setup::Gesture::ResetWifi) {
    Config reset = config; reset.ssid = reset.password = "";
    if (!saveConfig(reset)) { appMessage = "Could not save Wi-Fi reset."; return; }
    if (preferences.isKey("pin") && !preferences.remove("pin")) { appMessage = "Wi-Fi cleared, but PIN reset failed. Hold BOOT 8 seconds again."; config = reset; return; }
    pin = ""; loginFailures = 0; nextLogin = 0;
    config = reset; for (auto &session : sessions) session.token = "";
    WiFi.disconnect(true, true); WiFi.softAPdisconnect(true); apActive = false; startAp();
    appMessage = "Wi-Fi and pairing reset. Inventory and calibration kept. Join the temporary open setup network.";
  }
}
#endif
}  // namespace

void setup() {
  Serial.begin(115200);
  WiFi.persistent(false);
#ifdef SPOOLANCER_HEADLESS
  pinMode(Pins::wifiReset, INPUT_PULLUP);
#else
  pinMode(Pins::previous, INPUT_PULLUP); pinMode(Pins::next, INPUT_PULLUP); pinMode(Pins::select, INPUT_PULLUP);
#endif
#ifdef SPOOLANCER_TOUCH
  pinMode(Pins::sdSelect, OUTPUT); digitalWrite(Pins::sdSelect, HIGH);
#endif
  preferences.begin("spoolancer", false);
  pin = preferences.getString("pin", "");
#ifdef SPOOLANCER_HEADLESS
  if (!Setup::validPin(pin.c_str())) pin = "";
#else
  if (pin.length() != 8) { generatePin(); preferences.putString("pin", pin); }
#endif
  JsonDocument saved;
  if (!deserializeJson(saved, preferences.getString("config", "{}"))) {
    config.name = saved["name"] | "Spoolancer"; config.ssid = saved["ssid"] | "";
    config.password = saved["password"] | ""; config.spoolman = saved["spoolman"] | "";
  }
  apName = "Spoolancer-" + String(uint32_t(ESP.getEfuseMac()) & 0xffff, HEX);
  saved.clear();
  if (!deserializeJson(saved, preferences.getString("calibration", "{}"))) {
    scale.zero = saved["zero"] | 0.0; scale.zeroSet = saved["zero_set"] | false;
    scale.countsPerGram = saved["factor"] | 0.0;
  }
  scale.begin();
  assetsReady = LittleFS.begin(false, "/assets", 5, "spiffs");
  const bool initialized = preferences.getBool("storeReady", false);
  storeReady = inventory.begin(!initialized, "/inventory", 5, "inventory");
  if (storeReady && !initialized) preferences.putBool("storeReady", true);
  if (storeReady && !loadInventory()) { storeReady = false; appMessage = "Inventory file invalid. Preserved for recovery; writes disabled."; }
#ifndef SPOOLANCER_HEADLESS
  displaySpi.begin(Pins::displayClock, -1, Pins::displayData, Pins::displayCs);
  display.init(240, 320); display.setRotation(1); display.setTextWrap(false);
  pinVisibleUntil = millis() + 60000;
#endif
  Serial.printf("Spoolancer boot. Device PIN: %s\nFlash: %u bytes; PSRAM: %u bytes\n", pin.c_str(), ESP.getFlashChipSize(), ESP.getPsramSize());
#ifdef SPOOLANCER_HEADLESS
  if (pin.isEmpty()) startAp();
#endif
  connectWifi();
  Serial.println("Startup: configuring web routes");
  setupRoutes();
  Serial.println("Startup: initializing status UI");
  StatusUi::begin(); renderStatusUi(true);
  Serial.println("Startup: status UI ready");
  if (MDNS.begin("spoolancer")) MDNS.addService("http", "tcp", 80);
  nfc.begin([] { scale.tick(); renderStatusUi(); });
#ifdef SPOOLANCER_TOUCH
  if (!TouchUi::begin()) appMessage = "Touchscreen initialization failed. Use the browser and check the board revision.";
  else renderTouch();
#endif
#ifndef SPOOLANCER_HEADLESS
  displayStatus(); nextDisplay = millis() + 1000;
#endif
  nextNfc = millis() + 1500;
  const String address = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() :
                         (apActive ? WiFi.softAPIP().toString() : "not connected");
  Serial.printf("Spoolancer ready. Variant: %s; assets: %s; inventory: %s; setup AP: %s; address: %s\n",
                deviceVariant, assetsReady ? "ready" : "missing", storeReady ? "ready" : "unavailable",
                apActive ? apName.c_str() : "off", address.c_str());
}

void loop() {
  scale.tick(); server.handleClient(); buttons();
#ifdef SPOOLANCER_TOUCH
  pollTouch();
#endif
  const uint32_t now = millis();
  if (reconnectAt && int32_t(now - reconnectAt) >= 0) {
    reconnectAt = 0; WiFi.disconnect();
#ifndef SPOOLANCER_HEADLESS
    startAp();
#endif
    connectWifi();
  }
#ifdef SPOOLANCER_HEADLESS
  if (apActive) {
    setupWindow.observeConnection(WiFi.status() == WL_CONNECTED, now);
    if (setupWindow.expired(now)) {
      WiFi.softAPdisconnect(true); apActive = false; setupWindow.close();
      appMessage = "Setup network closed. Use the LAN address, or hold BOOT 8 seconds while running to start again.";
    }
  }
#else
  if (!config.ssid.isEmpty() && WiFi.status() != WL_CONNECTED && uint32_t(now - connectStarted) > 20000) startAp();
#endif
  if (scale.calibrated() && scale.stable()) {
    if (!activeId.isEmpty() && scale.gross() >= 20) activeHadLoad = true;
    if (activeHadLoad && std::abs(scale.gross()) < 15) { chooseSpool(""); nfcSpoolId = ""; appMessage = "Spool removed; select or scan the next spool."; }
  }
  if (nfc.available && int32_t(now - nextNfc) >= 0) { readNfc(); nextNfc = millis() + 2000; }
#ifndef SPOOLANCER_HEADLESS
  if (int32_t(now - nextDisplay) >= 0) { displayStatus(); nextDisplay = millis() + 1000; }
#endif
#ifdef SPOOLANCER_TOUCH
  if (int32_t(now - nextDisplay) >= 0) { renderTouch(); nextDisplay = millis() + 500; }
#endif
#ifdef SPOOLANCER_STATUS_LEDS
  if (int32_t(now - nextDisplay) >= 0) { renderStatusUi(); nextDisplay = millis() + 50; }
#endif
  delay(1);
}
