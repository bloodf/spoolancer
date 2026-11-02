#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#ifndef SPOOLANCER_HEADLESS
#include <SPI.h>
#include <Adafruit_ST7789.h>
#endif
#include <esp_system.h>
#include <esp_wifi.h>
#include <algorithm>
#include <vector>
#include "Pins.h"
#include "Domain.h"
#include "Scale.h"
#include "Nfc.h"
#include "BrandTags.h"
#include "ReadTags.h"
#include "StatusUi.h"
#include "Setup.h"
#include "RemoteInventory.h"
#include "RemoteHttp.h"
#include "Discovery.h"
#include "SpoolActions.h"
#include "JsonNumbers.h"
#include <mdns.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include "Ota.h"
#ifdef SPOOLANCER_TOUCH
#include "TouchUi.h"
#endif

namespace {
constexpr size_t maxLocalSpools = 100, maxSpools = maxLocalSpools + RemoteInventory::limit, maxRequest = 4096;
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
  double lastGross = NAN, lastRemaining = NAN, remoteRemaining = NAN;
  int remoteId = 0, vendorId = 0, filamentId = 0;
  String pending;
  uint32_t measurement = 0;
  bool remote = false, remoteListed = false;
};
struct Config {
  String name = "Spoolancer", ssid, password, spoolman, pin;
  Setup::AuthMode auth = Setup::AuthMode::Unconfigured;
} config;
struct Session { String token; uint32_t started = 0; } sessions[4];
std::vector<Spool> spools;
RemoteInventory::Cache remoteInventory;
String inventoryError;
String activeId, pin, apName, nfcSpoolId, appMessage, syncMessage, syncError;
JsonDocument nfcMetadata;
// Holds a genuine Bambu tag image between the clone's read step and its write-to-magic-card step.
uint8_t cloneImage[1024]; uint8_t cloneUid[4]; bool cloneReady = false; String cloneLabel;
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
bool connectionAttempt = false;
uint32_t nextNfc = 0, nextDisplay = 0, connectStarted = 0, reconnectAt = 0;
uint32_t nextLogin = 0, pinVisibleUntil = 0;
Ota::Release otaPending; bool otaRequested = false;
bool uploadDidStart = false; String uploadError; uint32_t rebootAt = 0;
unsigned loginFailures = 0, sessionSlot = 0;
struct WifiNetwork { String ssid; int32_t rssi, channel; bool secure, supported; };
struct WifiScan {
  enum class State { Ready, Scanning, Failed } state = State::Ready;
  bool started = false, radioPending = false;
  uint32_t startedAt = 0;
  std::vector<WifiNetwork> networks;
} wifiScan;
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
  if (value.auth == Setup::AuthMode::Invalid) return false;
  JsonDocument doc; doc["name"] = value.name; doc["ssid"] = value.ssid;
  doc["password"] = value.password; doc["spoolman"] = value.spoolman;
  doc["setup_complete"] = Setup::complete(value.auth);
  doc["pin_required"] = Setup::pinRequired(value.auth); doc["pin"] = value.pin;
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
String generatedPin() { char out[9]; snprintf(out, sizeof(out), "%08lu", (unsigned long)(esp_random() % 100000000UL)); return String(out); }
bool visibleSpool(const Spool &spool) {
  return RemoteInventory::visible(!config.spoolman.isEmpty(), spool.remote, spool.remoteListed, spool.remoteId);
}
bool inactiveTagLink(const Spool &spool) {
  return SpoolActions::inactiveTagLink(!config.spoolman.isEmpty(), spool.remote, spool.remoteListed, spool.pending.c_str(), spool.tagUid.c_str());
}
bool inventoryStale() { return remoteInventory.stale(millis(), WiFi.status() == WL_CONNECTED); }
Spool *findStoredSpool(const String &id) {
  for (auto &spool : spools) if (spool.id == id) return &spool;
  return nullptr;
}
Spool *findSpool(const String &id) {
  for (auto &spool : spools) if (spool.id == id && (visibleSpool(spool) || (!spool.remote && !spool.remoteId))) return &spool;
  return nullptr;
}
void renderStatusUi(bool force = false) {
#ifdef SPOOLANCER_STATUS_LEDS
  static uint32_t lastRender = 0;
  if (!force && uint32_t(millis() - lastRender) < 20) return;
  lastRender = millis();
  StatusUi::Snapshot state;
  state.gross = scale.gross(); state.calibrated = scale.calibrated();
  state.fresh = scale.fresh(); state.stable = scale.stable(); state.setup = apActive;
  state.connected = WiFi.status() == WL_CONNECTED; state.connecting = connectionAttempt;
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
void nfcIdle() { scale.tick(); renderStatusUi(); }
void startStatusOperation(StatusModel::Operation operation) {
  StatusUi::start(operation, millis()); renderStatusUi(true);
}
void finishStatusOperation(bool success) {
  StatusUi::finish(success, millis()); renderStatusUi(true);
}
using JsonNumbers::numberOrNull;
void spoolJson(JsonObject obj, const Spool &s, bool persisted = false) {
  obj["id"] = s.id; obj["name"] = s.name; obj["brand"] = s.brand;
  obj["material"] = s.material; obj["color"] = s.color;
  numberOrNull(obj["empty_weight_g"], s.empty); numberOrNull(obj["initial_weight_g"], s.initial);
  numberOrNull(obj["density_g_cm3"], s.density); numberOrNull(obj["diameter_mm"], s.diameter);
  if (s.remoteId) obj["spoolman_id"] = s.remoteId; else obj["spoolman_id"] = nullptr;
  numberOrNull(obj["remaining_weight_g"], s.remote && !persisted ? s.remoteRemaining : s.lastRemaining);
  numberOrNull(obj["last_measurement_remaining_g"], s.measurement ? s.lastRemaining : NAN);
  numberOrNull(obj["last_gross_g"], s.lastGross);
  obj["measurement_sequence"] = s.measurement; obj["tag_uid"] = s.tagUid;
  obj["sync_pending"] = !s.pending.isEmpty();
  obj["removal_pending"] = SpoolActions::archiving(s.pending.c_str());
  obj["inactive_tag_link"] = inactiveTagLink(s);
  obj["source"] = s.remote ? "spoolman" : "local"; obj["metadata_readonly"] = s.remote;
  if (persisted) { numberOrNull(obj["remote_remaining_weight_g"], s.remoteRemaining); obj["remote"] = s.remote; obj["remote_listed"] = s.remoteListed; obj["vendor_id"] = s.vendorId; obj["filament_id"] = s.filamentId; obj["pending"] = s.pending; }
}
bool saveInventory() {
  if (!storeReady) return false;
  JsonDocument doc; doc["schema"] = 1;
  JsonArray array = doc["spools"].to<JsonArray>();
  for (const auto &s : spools) spoolJson(array.add<JsonObject>(), s, true);
  File file = inventory.open("/state.tmp", FILE_WRITE);
  if (!file) return false;
  const size_t expected = measureJson(doc);
  if (expected > 400000) { file.close(); inventory.remove("/state.tmp"); return false; }
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
bool textWithoutNul(JsonVariantConst value) {
  if (!value.is<const char *>()) return false;
  const JsonString text = value.as<JsonString>();
  return text.size() == std::strlen(text.c_str());
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
  if (!file || file.size() > 400000) return false;
  JsonDocument doc;
  if (deserializeJson(doc, file) || doc["schema"] != 1 || !doc["spools"].is<JsonArray>()) return false;
  const JsonArrayConst array = doc["spools"].as<JsonArrayConst>();
  if (array.size() > maxSpools) return false;
  std::vector<Spool> loaded;
  for (JsonObjectConst obj : array) {
    Spool s;
    if (!obj["id"].is<const char *>() || !Domain::validId(obj["id"])) return false;
    s.remote = obj["remote"] == true; s.remoteListed = obj["remote_listed"] == true;
    if (s.remote) {
      if (!textField(obj, "name", s.name, 64, false) || !textField(obj, "brand", s.brand, 64, false) ||
          !textField(obj, "material", s.material, 64, false) || !textField(obj, "color", s.color, 7, false) ||
          (!s.color.isEmpty() && !Domain::validColor(s.color.c_str())) || !obj["spoolman_id"].is<int>() || obj["spoolman_id"].as<int>() < 1) return false;
      s.remoteId = obj["spoolman_id"].as<int>();
      for (const char *key : {"empty_weight_g", "initial_weight_g", "density_g_cm3", "diameter_mm"})
        if (!obj[key].isNull() && (!obj[key].is<double>() || !Domain::inRange(obj[key].as<double>(), 0, 1000000))) return false;
      s.empty = obj["empty_weight_g"].is<double>() ? obj["empty_weight_g"].as<double>() : NAN;
      s.initial = obj["initial_weight_g"].is<double>() ? obj["initial_weight_g"].as<double>() : NAN;
      s.density = obj["density_g_cm3"].is<double>() ? obj["density_g_cm3"].as<double>() : NAN;
      s.diameter = obj["diameter_mm"].is<double>() ? obj["diameter_mm"].as<double>() : NAN;
      if (s.remoteListed) remoteInventory.loaded = true;
    } else if (!validateSpool(obj, s)) return false;
    s.id = obj["id"].as<String>();
    for (const auto &prior : loaded) if (prior.id == s.id) return false;
    s.tagUid = obj["tag_uid"] | "";
    s.vendorId = obj["vendor_id"] | 0; s.filamentId = obj["filament_id"] | 0;
    s.pending = obj["pending"] | "";
    s.lastGross = obj["last_gross_g"].is<double>() ? obj["last_gross_g"].as<double>() : NAN;
    s.lastRemaining = obj["remaining_weight_g"].is<double>() ? obj["remaining_weight_g"].as<double>() : NAN;
    s.remoteRemaining = obj["remote_remaining_weight_g"].is<double>() ? obj["remote_remaining_weight_g"].as<double>() : NAN;
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
bool hasSession() {
  const String header = server.header("Authorization");
  if (header.startsWith("Bearer ")) {
    const String token = header.substring(7);
    for (const auto &session : sessions)
      if (session.token.length() == 64 && token == session.token && uint32_t(millis() - session.started) < sessionMs) return true;
  }
  return false;
}
bool authorized() {
  if (hasSession()) return true;
  error(401, "Open a device session to make changes."); return false;
}
bool trustedSessionRequest() {
  const String local = server.client().localIP().toString();
  return Setup::trustedLocalHeaders(server.hostHeader().c_str(), server.header("Origin").c_str(), local.c_str());
}
void sessionReply(JsonDocument &result) {
  Session &session = sessions[sessionSlot++ % 4];
  session.token = randomHex(32); session.started = millis();
  result["token"] = session.token; result["expires_in"] = sessionMs / 1000;
  loginFailures = 0; reply(result);
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
  if (SpoolActions::archiving(s.pending.c_str())) return "Finish checking this spool's archive status before measuring it.";
  if (s.remote && inventoryStale()) return "Refresh the Spoolman inventory before saving this measurement.";
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
bool cycleVisibleSpool(bool forward) {
  std::vector<size_t> visible;
  for (size_t i = 0; i < spools.size(); ++i) if (visibleSpool(spools[i])) visible.push_back(i);
  if (visible.empty()) return false;
  size_t at = forward ? visible.size() - 1 : 0;
  for (size_t i = 0; i < visible.size(); ++i) if (spools[visible[i]].id == activeId) at = i;
  at = forward ? (at + 1) % visible.size() : (at + visible.size() - 1) % visible.size();
  chooseSpool(spools[visible[at]].id); return true;
}

// Which printer formats the presented tag can take: "ntag" (Anycubic, Elegoo, OpenSpool) or "classic" (Creality, Qidi).
const char *nfcChip() { return nfc.classic1k() ? "classic" : nfc.ndefCapacity() ? "ntag" : "other"; }
void nfcJson(JsonObject obj) {
  obj["available"] = nfc.available; obj["present"] = nfc.present;
  obj["uid"] = nfc.uid; obj["type"] = nfc.type; obj["writable"] = nfc.writable;
  obj["raw_writable"] = nfc.rawWritable; obj["chip"] = nfcChip();
  obj["last_message"] = nfc.lastMessage; obj["last_error"] = nfc.lastError;
  if (nfcSpoolId.isEmpty()) obj["spool_id"] = nullptr; else obj["spool_id"] = nfcSpoolId;
  if (!nfcMetadata.isNull()) obj["metadata"].set(nfcMetadata);
}
void readNfcImpl(bool explicitRead) {
  const String previousUid = nfc.uid;
  nfcSpoolId = ""; nfcMetadata.clear();
  nfc.scan(explicitRead);
  // A failed or unlinked explicit scan keeps the user's manual selection.
  if (!explicitRead && (nfc.targets > 1 || (nfc.present && nfc.uid != previousUid))) chooseSpool("");
  if (!nfc.present || nfc.targets != 1 || nfc.uid.isEmpty()) return;
  // Payload metadata is a preview; only an explicit UID association selects an inventory record.
  for (const auto &s : spools) if (SpoolActions::identifiesSpool(visibleSpool(s), s.tagUid.c_str(), nfc.uid.c_str())) nfcSpoolId = s.id;
  if (!nfc.payload.isEmpty()) {
    JsonDocument tag;
    if (deserializeJson(tag, nfc.payload, DeserializationOption::NestingLimit(3)) || tag["schema"] != 1 ||
        !tag["spool_id"].is<const char *>() || !Domain::validId(tag["spool_id"])) {
      nfc.writable = false; nfc.lastError = "Invalid Spoolancer tag record; writing refused.";
    } else {
      Spool parsed;
      if (!validateSpool(tag.as<JsonObjectConst>(), parsed)) {
        nfc.writable = false; nfc.lastError = "Invalid tag metadata; writing refused.";
      } else {
        parsed.id = tag["spool_id"].as<String>();
        spoolJson(nfcMetadata.to<JsonObject>(), parsed);
      }
    }
  }
  // Explicit read of a genuine Bambu Lab / Snapmaker Classic tag: show a read-only material preview.
  if (explicitRead && nfc.payload.isEmpty() && nfcMetadata.isNull() && nfc.classic1k()) {
    ReadTags::Decoded tag;
    if (nfc.readClassic(nfc.uid, tag) && tag.valid) {
      JsonObject meta = nfcMetadata.to<JsonObject>();
      meta["brand"] = tag.brand; meta["material"] = tag.material; meta["color"] = tag.color;
      if (tag.weightG) meta["initial_weight_g"] = tag.weightG;
      meta["read_only"] = true; meta["source"] = "printer_tag";
    }
  }
  // Re-selecting on every poll would override a manual choice while the same tag stays seated.
  // Retried while nothing is selected (tag read before the load settled), never over a manual choice.
  if (!nfcSpoolId.isEmpty() && (explicitRead || activeId.isEmpty() || nfc.uid != previousUid) && (!scale.calibrated() || scale.gross() >= 20)) chooseSpool(nfcSpoolId);
}
void readNfc(bool explicitRead = false) {
  if (explicitRead) startStatusOperation(StatusModel::Operation::Nfc);
  readNfcImpl(explicitRead);
  if (explicitRead) finishStatusOperation(nfc.present && nfc.targets == 1 && !nfc.uid.isEmpty());
}
// Printer-ecosystem formats; the scale still identifies the spool by its linked UID.
String writeBrandTag(const Spool &s, const String &expectedUid, BrandTags::Format format) {
  using namespace BrandTags;
  const Profile *profile = profileFor(s.material.c_str());
  if (!profile) return "No " + String(info(format).label) + " material profile for \"" + s.material + "\". Use PLA, PETG, ABS, ASA, TPU, PC, PA, PVA or their CF/Silk/Matte variants.";
  if (!supports(format, *profile)) return String(info(format).label) + " has no " + profile->name + " profile.";
  // Remaining filament is what the printer should start counting from; fall back to the full spool.
  // Length starts from the remaining filament; weight fields stay the nominal spool size, as upstream tools write.
  const double grams = std::isfinite(s.lastRemaining) && s.lastRemaining > 0 ? s.lastRemaining : s.initial;
  const uint16_t meters = lengthMeters(grams, s.density, s.diameter);
  bool ok = false;
  switch (format) {
    case Format::Creality: {
      if (!nfc.scanIdentity() || nfc.uid != expectedUid || !nfc.classic1k()) return "Present one MIFARE Classic 1K tag with a 4-byte ID for Creality CFS.";
      uint8_t key[6], blocks[48];
      crealityKey(nfc.uidData(), key);
      if (!crealityBlocks(*profile, s.color.c_str(), meters, esp_random() % 1000000, blocks)) return "Could not encode the Creality record.";
      const uint8_t keys[][6] = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, {key[0], key[1], key[2], key[3], key[4], key[5]}};
      ok = nfc.writeClassicSector1(expectedUid, keys, 2, blocks, 3, key);
      break;
    }
    case Format::Qidi: {
      uint8_t block[16];
      if (!qidiBlock(*profile, s.color.c_str(), block)) return "Could not encode the Qidi record.";
      const uint8_t keys[][6] = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, {0xd3, 0xf7, 0xd3, 0xf7, 0xd3, 0xf7}};
      ok = nfc.writeClassicSector1(expectedUid, keys, 2, block, 1, nullptr);
      if (!ok && nfc.classic1k()) {
        // A tag previously written for Creality: restore the default keys the Qidi Box uses.
        uint8_t creality[6]; crealityKey(nfc.uidData(), creality);
        const uint8_t retry[][6] = {{creality[0], creality[1], creality[2], creality[3], creality[4], creality[5]}};
        ok = nfc.writeClassicSector1(expectedUid, retry, 1, block, 1, keys[0]);
      }
      break;
    }
    case Format::Anycubic: {
      uint8_t data[anycubicPages * 4];
      if (!anycubicData(*profile, s.color.c_str(), meters, s.initial, s.diameter, data)) return "Could not encode the Anycubic record.";
      ok = nfc.writeNtagPages(expectedUid, anycubicFirstPage, data, anycubicPages);
      break;
    }
    case Format::Elegoo: {
      uint8_t data[elegooPages * 4];
      if (!elegooData(*profile, s.color.c_str(), s.initial, s.diameter, data)) return "Could not encode the Elegoo record.";
      ok = nfc.writeNtagPages(expectedUid, elegooFirstPage, data, elegooPages);
      break;
    }
    case Format::OpenSpool: {
      char json[400];
      if (!openspoolJson(*profile, s.brand.c_str(), s.color.c_str(), s.initial, s.diameter, json, sizeof(json))) return "Could not encode the OpenSpool record.";
      ok = nfc.writeNdef(expectedUid, String(json), openspoolMime);
      break;
    }
    default: return "Unsupported tag format.";
  }
  return ok ? "" : nfc.lastError;
}
String writeSpoolTagImpl(Spool &s, const String &expectedUid, BrandTags::Format format) {
  if (!s.pending.isEmpty()) return "Resolve the pending Spoolman operation before writing this tag.";
  if (s.remote && inventoryStale()) return "Refresh Spoolman before writing this profile to a tag.";
  JsonDocument profile; spoolJson(profile.to<JsonObject>(), s); Spool checked;
  if (!validateSpool(profile.as<JsonObjectConst>(), checked)) return "Complete the spool metadata in Spoolman before writing an inventory tag.";
  if (expectedUid.isEmpty()) return "Scan and confirm the target tag UID first.";
  for (const auto &other : spools) if (other.id != s.id && other.tagUid == expectedUid)
    return "This UID is already linked to another local spool. Unlink it before rewriting this inventory tag.";
  if (format != BrandTags::Format::Spoolancer) {
    const String problem = writeBrandTag(s, expectedUid, format);
    if (!problem.isEmpty()) return problem;
  } else {
    JsonDocument tag; spoolJson(tag.to<JsonObject>(), s);
    tag.remove("id"); tag.remove("spoolman_id"); tag.remove("remaining_weight_g"); tag.remove("last_gross_g");
    tag.remove("measurement_sequence"); tag.remove("tag_uid"); tag.remove("sync_pending");
    tag.remove("source"); tag.remove("metadata_readonly"); tag.remove("last_measurement_remaining_g");
    tag.remove("removal_pending");
    tag.remove("inactive_tag_link");
    tag["schema"] = 1; tag["spool_id"] = s.id;
    String json; serializeJson(tag, json);
    if (!nfc.writeInventory(expectedUid, json)) return nfc.lastError;
  }
  const String oldUid = s.tagUid; s.tagUid = nfc.uid;
  if (!saveInventory()) {
    s.tagUid = oldUid;
    return "Tag was written and verified, but its local UID link could not be saved. Use Add RFID to link it before tag identification.";
  }
  nfcSpoolId = s.id; chooseSpool(s.id); nextNfc = millis() + 2000;
  return "";
}
String writeSpoolTag(Spool &s, const String &expectedUid, BrandTags::Format format) {
  startStatusOperation(StatusModel::Operation::Nfc);
  const String problem = writeSpoolTagImpl(s, expectedUid, format);
  finishStatusOperation(problem.isEmpty()); return problem;
}
void statusRoute() {
  JsonDocument doc; doc["mode"] = "device"; doc["device"]["name"] = config.name;
  doc["device"]["firmware"] = SPOOLANCER_VERSION; doc["device"]["storage_ready"] = storeReady;
  doc["device"]["variant"] = deviceVariant;
  doc["auth"]["pin_required"] = Setup::pinRequired(config.auth);
  doc["auth"]["invalid"] = config.auth == Setup::AuthMode::Invalid;
  JsonObject setup = doc["setup"].to<JsonObject>();
  setup["required"] = Setup::required(config.auth); setup["complete"] = Setup::complete(config.auth);
  setup["enrollment_required"] = Setup::required(config.auth);
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
  setup["ap_active"] = apActive; setup["open_network"] = apActive;
  setup["via_ap"] = viaSetupAp();
  setup["physical_confirmation_required"] = false;
  setup["can_submit"] = Setup::required(config.auth) && viaSetupAp() && WiFi.status() != WL_CONNECTED && setupWindow.available(millis());
  setup["ap_seconds"] = (setupWindow.apRemaining(millis()) + 999) / 1000;
  setup["warning"] = "Temporary setup network has no password; use it in a trusted location. Wi-Fi credentials are sent over local HTTP.";
#else
  doc["device"]["headless"] = false;
  setup["ap_active"] = apActive; setup["open_network"] = false;
  setup["via_ap"] = apActive && server.client().localIP() == WiFi.softAPIP();
  setup["physical_confirmation_required"] = false; setup["can_submit"] = false;
#endif
  JsonObject weight = doc["scale"].to<JsonObject>();
  numberOrNull(weight["gross_g"], scale.gross()); weight["calibrated"] = scale.calibrated();
  weight["tare_ready"] = scale.tareReady(); weight["calibration_ready"] = scale.calibrationReady();
  weight["sample_count"] = scale.sampleCount();
  weight["stable"] = scale.stable(); weight["fresh"] = scale.fresh(); weight["zero_set"] = scale.zeroSet;
  Spool *s = findSpool(activeId);
  const double net = s && std::isfinite(s->empty) ? scale.gross() - s->empty : NAN;
  numberOrNull(weight["net_g"], net); numberOrNull(weight["remaining_g"], net);
  if (activeId.isEmpty()) doc["active_spool_id"] = nullptr; else doc["active_spool_id"] = activeId;
  // Public status reports hardware, not inventory metadata or configuration secrets.
  JsonObject tag = doc["nfc"].to<JsonObject>();
  tag["available"] = nfc.available; tag["present"] = nfc.present; tag["uid"] = nfc.uid;
  tag["type"] = nfc.type; tag["writable"] = nfc.writable;
  tag["raw_writable"] = nfc.rawWritable; tag["chip"] = nfcChip();
  tag["last_message"] = nfc.lastMessage; tag["last_error"] = nfc.lastError;
#ifdef SPOOLANCER_STATUS_LEDS
  doc["diagnostics"]["leds_supported"] = true;
#else
  doc["diagnostics"]["leds_supported"] = false;
#endif
  doc["diagnostics"]["led_test_remaining_ms"] = StatusUi::ledTestRemaining(millis());
  doc["network"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["network"]["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["network"]["ap_active"] = apActive;
  doc["spoolman"]["configured"] = !config.spoolman.isEmpty();
  doc["spoolman"]["inventory_source"] = config.spoolman.isEmpty() ? "local" : "spoolman";
  doc["spoolman"]["inventory_stale"] = !config.spoolman.isEmpty() && inventoryStale();
  doc["spoolman"]["inventory_error"] = inventoryError;
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
bool readHttpLine(WiFiClient &client, String &line, uint32_t deadline, size_t &headerBytes) {
  line = "";
  while (int32_t(deadline - millis()) > 0) {
    scale.tick(); renderStatusUi();
    if (!client.available()) {
      if (!client.connected()) return false;
      delay(1); continue;
    }
    const char c = client.read();
    if (++headerBytes > 4096 || line.length() > 512) return false;
    if (c == '\n') return true;
    if (c != '\r') line += c;
  }
  return false;
}
bool remoteRequestAt(const String &url, const char *method, const String &path, JsonDocument *input, JsonDocument &output, int &code, int allowedError = 0) {
  code = 0;
  if (WiFi.status() != WL_CONNECTED || url.isEmpty() || !validServerUrl(url)) { syncError = "Connect Wi-Fi and configure a local Spoolman URL first."; return false; }
  String authority = url.substring(7), basePath;
  const int slash = authority.indexOf('/');
  if (slash >= 0) { basePath = authority.substring(slash); authority = authority.substring(0, slash); }
  const int colon = authority.indexOf(':');
  const String host = colon < 0 ? authority : authority.substring(0, colon);
  const uint16_t port = colon < 0 ? 80 : authority.substring(colon + 1).toInt();
  IPAddress address; address.fromString(host);
  WiFiClient client;
  scale.tick(); renderStatusUi();
  // code -1 marks that no request bytes left the device, so a create can be retried safely.
  if (!client.connect(address, port, 150)) { code = -1; syncError = "Spoolman connection failed."; return false; }
  scale.tick(); renderStatusUi();
  String data; if (input) serializeJson(*input, data);
  const String headers = String(method) + " " + basePath + "/api/v1" + path + " HTTP/1.0\r\nHost: " + authority +
      "\r\nConnection: close\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: " + String(data.length()) + "\r\n\r\n";
  const String request = headers + data;
  const uint32_t deadline = millis() + 2000;
  for (size_t offset = 0; offset < request.length();) {
    scale.tick(); renderStatusUi();
    const size_t length = std::min(size_t(128), request.length() - offset);
    const int written = ::send(client.fd(), request.c_str() + offset, length, MSG_DONTWAIT);
    if (int32_t(deadline - millis()) <= 0 || written == 0 ||
        (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
      client.stop(); syncError = "Spoolman request could not be sent."; return false;
    }
    if (written > 0) offset += static_cast<size_t>(written);
    else delay(1);
  }
  String line; size_t headerBytes = 0;
  if (!readHttpLine(client, line, deadline, headerBytes) || !RemoteHttp::status(line.c_str(), code)) {
    client.stop(); syncError = "Spoolman returned an invalid or incomplete HTTP response."; return false;
  }
  if ((code < 200 || code >= 300) && code != allowedError) { client.stop(); syncError = "Spoolman returned HTTP " + String(code) + "."; return false; }
  int remaining = -1; bool lengthSeen = false;
  for (;;) {
    if (!readHttpLine(client, line, deadline, headerBytes)) { client.stop(); syncError = "Spoolman response headers timed out or exceeded the limit."; return false; }
    if (line.isEmpty()) break;
    if (!RemoteHttp::header(line.c_str(), remaining, lengthSeen)) {
      client.stop(); syncError = "Spoolman response has invalid headers, exceeds 20 KB, or uses unsupported transfer encoding. Use its direct HTTP address."; return false;
    }
  }
  String response; response.reserve(1024);
  while (int32_t(deadline - millis()) > 0 && (client.connected() || client.available()) && remaining != 0) {
    scale.tick(); renderStatusUi();
    if (client.available()) {
      response += char(client.read()); if (remaining > 0) --remaining;
      if (response.length() > 20000) break;
    } else delay(1);
  }
  const bool incomplete = !RemoteHttp::complete(remaining, client.connected(), response.length());
  client.stop();
  if (response.length() > 20000 || incomplete || deserializeJson(output, response, DeserializationOption::NestingLimit(8))) {
    syncError = "Spoolman returned an incomplete or invalid JSON response."; return false;
  }
  return true;
}
bool remoteRequest(const char *method, const String &path, JsonDocument *input, JsonDocument &output, int &code, int allowedError = 0) {
  return remoteRequestAt(config.spoolman, method, path, input, output, code, allowedError);
}
bool remoteNumber(JsonVariantConst value, double &result) {
  if (value.isNull()) { result = NAN; return true; }
  if (!value.is<double>() || !Domain::inRange(value.as<double>(), 0, 1000000)) return false;
  result = value.as<double>(); return true;
}
bool remoteText(JsonVariantConst field, String &value, size_t limit) {
  if (field.isNull()) { value = ""; return true; }
  if (!textWithoutNul(field)) return false;
  value = field.as<String>();
  return value.length() <= limit && Setup::validText(value.c_str());
}
bool parseRemoteSpool(JsonObjectConst item, Spool &value) {
  const JsonObjectConst filament = item["filament"].as<JsonObjectConst>();
  if (!item["id"].is<int>() || item["id"].as<int>() < 1 || filament.isNull() ||
      !filament["id"].is<int>() || filament["id"].as<int>() < 1 || !item["archived"].is<bool>()) return false;
  value.remoteId = item["id"].as<int>(); value.filamentId = filament["id"].as<int>();
  value.remote = true; value.remoteListed = !item["archived"].as<bool>();
  const JsonObjectConst vendor = filament["vendor"].as<JsonObjectConst>();
  value.vendorId = vendor["id"] | 0;
  if (!remoteText(filament["name"], value.name, 64) || !remoteText(filament["material"], value.material, 64) ||
      !remoteText(vendor["name"], value.brand, 64)) return false;
  String color;
  if (!remoteText(filament["color_hex"], color, 9)) return false;
  if (color.startsWith("#")) color.remove(0, 1);
  if (!color.isEmpty()) {
    if (color.length() != 6 && color.length() != 8) return false;
    for (const char c : color) if (!isxdigit(static_cast<unsigned char>(c))) return false;
    value.color = "#" + color.substring(0, 6);
  } else value.color = "";
  double initial, filamentWeight, empty, filamentEmpty, vendorEmpty, remaining;
  if (!remoteNumber(item["initial_weight"], initial) || !remoteNumber(filament["weight"], filamentWeight) ||
      !remoteNumber(item["spool_weight"], empty) || !remoteNumber(filament["spool_weight"], filamentEmpty) ||
      !remoteNumber(vendor["empty_spool_weight"], vendorEmpty) || !remoteNumber(item["remaining_weight"], remaining) ||
      !remoteNumber(filament["density"], value.density) || !remoteNumber(filament["diameter"], value.diameter)) return false;
  const auto weights = RemoteInventory::weights(initial, filamentWeight, empty, filamentEmpty, vendorEmpty, remaining);
  value.initial = weights.initial; value.empty = weights.empty; value.lastRemaining = value.remoteRemaining = weights.remaining;
  return true;
}
void remoteIdentity(Spool &record) {
  for (const auto &old : spools) if (old.remoteId == record.remoteId) {
    record.id = old.id; record.tagUid = old.tagUid;
    record.lastGross = old.lastGross; record.lastRemaining = old.lastRemaining; record.measurement = old.measurement;
    record.pending = old.pending; return;
  }
  for (uint32_t prefix = 0; ; ++prefix) {
    char generated[17]; RemoteInventory::stableId(record.remoteId, prefix, generated);
    bool collision = false;
    for (const auto &old : spools) if (old.id == generated) { collision = true; break; }
    if (!collision) { record.id = generated; return; }
  }
}
bool refreshRemoteInventory(bool force = false) {
  if (config.spoolman.isEmpty()) return true;
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (!connected) { inventoryError = "Spoolman is offline. Showing only its last cached inventory, if available."; return false; }
  if (!remoteInventory.shouldRefresh(millis(), connected, force)) return !inventoryStale();
  remoteInventory.begin(millis());
  std::vector<Spool> fetched;
  RemoteInventory::Pages pages;
  const uint32_t started = millis();
  bool complete = false;
  for (size_t offset = 0; offset <= RemoteInventory::limit; offset += RemoteInventory::pageSize) {
    if (uint32_t(millis() - started) > 6000) { syncError = "Spoolman inventory refresh timed out; retry to load the complete list."; break; }
    JsonDocument response; int code;
    const String path = "/spool?allow_archived=false&sort=id:asc&limit=" + String(RemoteInventory::pageSize) + "&offset=" + String(offset);
    if (!remoteRequest("GET", path, nullptr, response, code) || !response.is<JsonArray>() || response.size() > RemoteInventory::pageSize) {
      if (syncError.isEmpty()) syncError = "Unexpected Spoolman inventory response.";
      break;
    }
    bool valid = true;
    for (JsonVariantConst item : response.as<JsonArrayConst>()) {
      Spool record;
      if (!item.is<JsonObjectConst>() || !parseRemoteSpool(item.as<JsonObjectConst>(), record) || !pages.accept(record.remoteId)) {
        syncError = "Spoolman inventory is invalid, changed during paging, or exceeds the 200-spool limit."; valid = false; break;
      }
      if (!record.remoteListed) continue;
      remoteIdentity(record); fetched.push_back(record);
    }
    if (!valid) break;
    if (pages.complete(response.size())) { complete = true; break; }
  }
  if (!complete) {
    remoteInventory.finish(millis(), false); inventoryError = syncError;
    return false;
  }
  // Replace a complete server snapshot atomically. Unresolved local journals stay hidden, never deleted.
  std::vector<Spool> updated;
  for (const auto &old : spools) {
    bool fetchedRecord = false;
    for (const auto &current : fetched) if (current.id == old.id) { fetchedRecord = true; break; }
    if (fetchedRecord) continue;
    if (old.remote && old.pending.isEmpty() && old.tagUid.isEmpty() && old.measurement == 0) continue;
    Spool retained = old; retained.remoteListed = false;
    updated.push_back(retained);
  }
  if (updated.size() + fetched.size() > maxSpools) {
    remoteInventory.finish(millis(), false); inventoryError = "The device cache is full. Existing records were preserved; inventory was not replaced."; return false;
  }
  updated.insert(updated.end(), fetched.begin(), fetched.end());
  spools = std::move(updated); remoteInventory.finish(millis(), true); inventoryError = "";
  if (!activeId.isEmpty() && !findSpool(activeId)) chooseSpool("");
  return true;
}
void inventoryJson(JsonObject obj) {
  const bool configured = !config.spoolman.isEmpty();
  obj["source"] = configured ? "spoolman" : "local"; obj["configured"] = configured;
  obj["stale"] = configured && inventoryStale(); obj["cached"] = configured && remoteInventory.loaded;
  obj["error"] = configured ? inventoryError : ""; obj["limit"] = configured ? RemoteInventory::limit : maxLocalSpools;
  if (configured && remoteInventory.successful) obj["age_seconds"] = uint32_t(millis() - remoteInventory.refreshedAt) / 1000;
  else obj["age_seconds"] = nullptr;
}
bool remoteWritePreflight(Spool &s) {
  if (!s.remoteId) return true;
  JsonDocument response; int code;
  if (!remoteRequest("GET", "/spool/" + String(s.remoteId), nullptr, response, code)) {
    inventoryError = syncError; remoteInventory.failed = true; return false;
  }
  Spool current;
  if (!parseRemoteSpool(response.as<JsonObjectConst>(), current) || current.remoteId != s.remoteId || !current.remoteListed) {
    syncError = "Spoolman spool is missing, archived, or invalid. Refresh the inventory."; remoteInventory.failed = true; inventoryError = syncError; return false;
  }
  if (!RemoteInventory::sameNumber(current.empty, s.empty) || !RemoteInventory::sameNumber(current.initial, s.initial)) {
    syncError = "Spoolman weights changed. Refresh the inventory and measure again before sending."; remoteInventory.failed = true; inventoryError = syncError; return false;
  }
  return true;
}
SpoolActions::ArchiveState readArchiveState(const Spool &s) {
  JsonDocument response; int code;
  if (!remoteRequest("GET", "/spool/" + String(s.remoteId), nullptr, response, code, 404)) return SpoolActions::ArchiveState::Unknown;
  if (code == 404) {
    return textWithoutNul(response["message"]) && SpoolActions::missingReply(code, s.remoteId, response["message"].as<const char *>())
      ? SpoolActions::ArchiveState::Missing : SpoolActions::ArchiveState::Unknown;
  }
  return SpoolActions::archiveState(s.remoteId, response["id"].is<int>() ? response["id"].as<int>() : 0,
                                    response["archived"].is<bool>(), response["archived"] == true);
}
bool confirmMissingArchive(const Spool &s) {
  if (!SpoolActions::archiving(s.pending.c_str())) return false;
  JsonDocument info; int code;
  const bool verified = remoteRequest("GET", "/info", nullptr, info, code) && textWithoutNul(info["version"]) &&
    !info["version"].as<String>().isEmpty() && textWithoutNul(info["db_type"]) && !info["db_type"].as<String>().isEmpty() && info["debug_mode"].is<bool>();
  if (!verified) return false;
  // A complete, ordered prefix including archived records must independently prove absence.
  RemoteInventory::Pages pages; const uint32_t started = millis();
  for (size_t offset = 0; offset <= RemoteInventory::limit; offset += RemoteInventory::pageSize) {
    if (uint32_t(millis() - started) >= 6000) return false;
    JsonDocument response;
    const String path = "/spool?allow_archived=true&sort=id:asc&limit=" + String(RemoteInventory::pageSize) + "&offset=" + String(offset);
    if (!remoteRequest("GET", path, nullptr, response, code) || !response.is<JsonArray>() || response.size() > RemoteInventory::pageSize) return false;
    for (JsonVariantConst item : response.as<JsonArrayConst>()) {
      Spool record;
      if (!item.is<JsonObjectConst>() || !parseRemoteSpool(item.as<JsonObjectConst>(), record) || !pages.accept(record.remoteId) || record.remoteId == s.remoteId) return false;
    }
    if (pages.previousId > s.remoteId || pages.complete(response.size()))
      return SpoolActions::confirmedMissing(s.pending.c_str(), verified, true, true);
  }
  return false;
}
bool archiveSpool(Spool &s, bool &removedMissing) {
  using SpoolActions::ArchiveState;
  syncError = ""; removedMissing = false;
  const auto current = readArchiveState(s);
  if (current == ArchiveState::Missing) {
    removedMissing = confirmMissingArchive(s);
    if (!removedMissing) syncError = "The server returned a missing spool, but its identity and complete list absence could not be confirmed. The recovery record was kept.";
    return removedMissing;
  }
  if (current == ArchiveState::Unknown) {
    syncError = "Could not confirm this spool's current archive status. No local record was removed. " + syncError;
    return false;
  }
  if (!SpoolActions::archiving(s.pending.c_str())) {
    const String previous = s.pending; s.pending = "archive";
    if (!saveInventory()) {
      s.pending = previous;
      syncError = "Cannot save the archive journal; nothing was changed in Spoolman.";
      return false;
    }
  }
  if (current == ArchiveState::Archived) return true;
  JsonDocument request, response; request["archived"] = true; int code;
  remoteRequest("PATCH", "/spool/" + String(s.remoteId), &request, response, code);
  // PATCH is idempotent, but its response alone is never treated as proof of an archive.
  const auto checked = readArchiveState(s);
  if (checked == ArchiveState::Archived) return true;
  if (checked == ArchiveState::Missing && confirmMissingArchive(s)) { removedMissing = true; return true; }
  syncError = "Archive was not confirmed. The record and its recovery journal were kept. Check archive status to retry safely.";
  return false;
}
struct SpoolmanDiscovery {
  enum class State { Ready, Searching, Failed } state = State::Ready;
  struct Socket { int fd = -1; uint32_t address = 0, deadline = 0; } sockets[4];
  struct Candidate { String url, label; };
  std::vector<Candidate> candidates;
  std::vector<String> pending;
  size_t checked = 0;
  uint32_t started = 0, nextAddress = 0;
  Discovery::Range scope{};
  bool attempted = false;
  mdns_search_once_t *serviceSearch = nullptr, *hostSearch = nullptr;
  String message;
} discovery;
uint32_t addressNumber(const IPAddress &ip) {
  return (uint32_t(ip[0]) << 24) | (uint32_t(ip[1]) << 16) | (uint32_t(ip[2]) << 8) | ip[3];
}
IPAddress addressFromNumber(uint32_t address) { return IPAddress(address >> 24, address >> 16, address >> 8, address); }
void queueDiscovery(const IPAddress &address, uint16_t port) {
  if (!port || !Discovery::eligible(addressNumber(address), discovery.scope) || discovery.pending.size() >= 16) return;
  const String url = "http://" + address.toString() + ":" + String(port);
  for (const auto &known : discovery.pending) if (known == url) return;
  for (const auto &known : discovery.candidates) if (known.url == url) return;
  discovery.pending.push_back(url);
}
void pollMdns(mdns_search_once_t *&query, bool service) {
  if (!query) return;
  mdns_result_t *results = nullptr;
  if (!mdns_query_async_get_results(query, 0, &results)) return;
  mdns_query_async_delete(query); query = nullptr;
  for (auto *result = results; result; result = result->next) {
    String name = result->instance_name ? result->instance_name : ""; name.toLowerCase();
    if (service && result->port != 7912 && name.indexOf("spoolman") < 0) continue;
    for (auto *address = result->addr; address; address = address->next)
      if (address->addr.type == IPADDR_TYPE_V4) queueDiscovery(IPAddress(address->addr.u_addr.ip4.addr), service ? result->port : 7912);
  }
  mdns_query_results_free(results);
}
void discoveryReply() {
  JsonDocument result;
  result["state"] = discovery.state == SpoolmanDiscovery::State::Searching ? "searching" :
                    discovery.state == SpoolmanDiscovery::State::Failed ? "failed" : "ready";
  result["message"] = discovery.message;
  JsonArray candidates = result["candidates"].to<JsonArray>();
  for (const auto &candidate : discovery.candidates) {
    JsonObject item = candidates.add<JsonObject>(); item["url"] = candidate.url; item["label"] = candidate.label;
  }
  reply(result);
}
void startDiscovery() {
  discovery.state = SpoolmanDiscovery::State::Searching; discovery.attempted = true;
  discovery.started = millis(); discovery.checked = 0; discovery.candidates.clear(); discovery.pending.clear();
  discovery.scope = Discovery::range(addressNumber(WiFi.localIP()), addressNumber(WiFi.subnetMask()));
  discovery.nextAddress = discovery.scope.first;
  discovery.message = "Searching this Wi-Fi subnet (up to 254 addresses) on port 7912 and advertised Spoolman services. Other ports or subnets need a manual URL.";
  if (!config.spoolman.isEmpty()) {
    String host = config.spoolman.substring(7);
    const int slash = host.indexOf('/'); if (slash >= 0) host = host.substring(0, slash);
    const int colon = host.indexOf(':'); if (colon >= 0) host = host.substring(0, colon);
    IPAddress address;
    if (address.fromString(host) && Discovery::eligible(addressNumber(address), discovery.scope)) discovery.pending.push_back(config.spoolman);
  }
  discovery.serviceSearch = mdns_query_async_new(nullptr, "_http", "_tcp", MDNS_TYPE_PTR, 1500, 8, nullptr);
  discovery.hostSearch = mdns_query_async_new("spoolman", nullptr, nullptr, MDNS_TYPE_A, 1500, 1, nullptr);
}
void pollDiscovery() {
  pollMdns(discovery.serviceSearch, true); pollMdns(discovery.hostSearch, false);
  if (discovery.state != SpoolmanDiscovery::State::Searching) return;
  const bool stop = WiFi.status() != WL_CONNECTED || uint32_t(millis() - discovery.started) >= 45000;
  bool socketsActive = false;
  for (auto &probe : discovery.sockets) {
    if (probe.fd >= 0) {
      fd_set ready, errors; FD_ZERO(&ready); FD_ZERO(&errors); FD_SET(probe.fd, &ready); FD_SET(probe.fd, &errors);
      timeval timeout{};
      const int selected = select(probe.fd + 1, nullptr, &ready, &errors, &timeout);
      if (selected > 0) {
        int socketError = 0; socklen_t length = sizeof(socketError);
        if (getsockopt(probe.fd, SOL_SOCKET, SO_ERROR, &socketError, &length) == 0 && !socketError)
          queueDiscovery(addressFromNumber(probe.address), 7912);
      }
      if (stop || selected != 0 || int32_t(probe.deadline - millis()) <= 0) { close(probe.fd); probe.fd = -1; }
    }
    if (probe.fd < 0 && !stop && discovery.nextAddress <= discovery.scope.last) {
      const uint32_t address = discovery.nextAddress++;
      if (Discovery::eligible(address, discovery.scope)) {
        probe.fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (probe.fd >= 0) {
          if (fcntl(probe.fd, F_SETFL, O_NONBLOCK) < 0) { close(probe.fd); probe.fd = -1; continue; }
          sockaddr_in target{}; target.sin_family = AF_INET; target.sin_port = htons(7912); target.sin_addr.s_addr = htonl(address);
          const int result = connect(probe.fd, reinterpret_cast<sockaddr *>(&target), sizeof(target));
          probe.address = address; probe.deadline = millis() + 180;
          if (result == 0) { queueDiscovery(addressFromNumber(address), 7912); close(probe.fd); probe.fd = -1; }
          else if (errno != EINPROGRESS) { close(probe.fd); probe.fd = -1; }
        }
      }
    }
    socketsActive |= probe.fd >= 0;
  }
  if (!stop && discovery.checked < discovery.pending.size() && discovery.candidates.size() < 8) {
    const String url = discovery.pending[discovery.checked++];
    JsonDocument info; int code; const String previousError = syncError;
    if (remoteRequestAt(url, "GET", "/info", nullptr, info, code) && info["version"].is<const char *>() &&
        info["db_type"].is<const char *>() && info["debug_mode"].is<bool>()) {
      JsonDocument inventoryProbe;
      if (remoteRequestAt(url, "GET", "/spool?limit=1", nullptr, inventoryProbe, code) && inventoryProbe.is<JsonArray>() && inventoryProbe.size() <= 1) {
        bool valid = inventoryProbe.size() == 0;
        if (inventoryProbe.size() == 1) { Spool record; valid = parseRemoteSpool(inventoryProbe[0].as<JsonObjectConst>(), record); }
        if (valid) discovery.candidates.push_back({url, "Spoolman " + info["version"].as<String>()});
      }
    }
    syncError = previousError;
  }
  if (stop || (!socketsActive && discovery.nextAddress > discovery.scope.last && !discovery.serviceSearch && !discovery.hostSearch &&
      (discovery.checked >= discovery.pending.size() || discovery.candidates.size() >= 8))) {
    discovery.state = WiFi.status() == WL_CONNECTED ? SpoolmanDiscovery::State::Ready : SpoolmanDiscovery::State::Failed;
    discovery.message = discovery.state == SpoolmanDiscovery::State::Failed ? "Wi-Fi disconnected during discovery. Reconnect or enter the server URL manually." :
      discovery.candidates.empty() ? "No Spoolman server answered in this subnet. Enter its URL manually if it uses another port, subnet, or blocked discovery." :
                                    "Select a server, then save its URL. Discovery does not change the configured server.";
  }
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
    // Mark the server record before saving so a later mismatch or reboot cannot hide it.
    if (String(kind) == "spool") s.remote = s.remoteListed = true;
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
  if ((code >= 400 && code < 500) || code == -1) {
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
    s.remote = s.remoteListed = true; s.remoteRemaining = s.lastRemaining;
    if (!saveInventory()) { syncError = "Spoolman spool " + String(s.remoteId) + " was created, but its local link could not be saved."; return false; }
    syncMessage = "Created Spoolman spool " + String(s.remoteId) + " with the saved remaining weight.";
    return true;
  }
  if (!remoteWritePreflight(s)) return false;
  JsonDocument request, response; int code;
  request["remaining_weight"] = s.lastRemaining;
  if (!remoteRequest("PATCH", "/spool/" + String(s.remoteId), &request, response, code)) {
    syncError += " The absolute update may have reached the server; retrying the same saved weight does not subtract consumption twice.";
    remoteInventory.failed = true; inventoryError = syncError; return false;
  }
  if (response["id"] != s.remoteId || !response["remaining_weight"].is<double>() ||
      std::abs(response["remaining_weight"].as<double>() - s.lastRemaining) > .1) {
    syncError = "Spoolman did not confirm the requested remaining weight.";
    remoteInventory.failed = true; inventoryError = syncError; return false;
  }
  s.remoteRemaining = s.lastRemaining;
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
  if (Setup::required(config.auth) && apActive) {
    state.message = "Open setup Wi-Fi " + apName + ". Choose Wi-Fi and optional PIN in the browser.";
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
    if (!cycleVisibleSpool(command.action == Action::Next)) { appMessage = "Load the inventory in the browser first."; return; }
    appMessage = "Spool selected."; return;
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
    const String problem = writeSpoolTag(*spool, command.expectedUid, BrandTags::Format::Spoolancer);
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
void cancelWifiScan() {
  if (!wifiScan.radioPending) return;
  const int16_t count = WiFi.scanComplete();
  const esp_err_t stopped = count >= 0 ? ESP_OK : esp_wifi_scan_stop();
  wifiScan.state = WifiScan::State::Failed;
  if (stopped != ESP_OK && stopped != ESP_ERR_WIFI_NOT_STARTED && stopped != ESP_ERR_WIFI_NOT_INIT) return;
  wifiScan.radioPending = false;
  if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) WiFi.scanDelete();
}
void pollWifiScan() {
  if (!wifiScan.radioPending) return;
  const int16_t count = WiFi.scanComplete();
  if (wifiScan.state == WifiScan::State::Failed && count < 0) return;
  if (count == WIFI_SCAN_RUNNING && uint32_t(millis() - wifiScan.startedAt) < 8000) return;
  if (count < 0) { cancelWifiScan(); return; }
  wifiScan.radioPending = false;
  if (wifiScan.state != WifiScan::State::Scanning) { WiFi.scanDelete(); return; }
  wifiScan.networks.clear();
  for (int i = 0; i < count && i < 255; ++i) {
    const String ssid = WiFi.SSID(uint8_t(i));
    if (!Setup::validSsid(ssid.c_str())) continue;
    const wifi_auth_mode_t auth = WiFi.encryptionType(uint8_t(i));
    WifiNetwork network{ssid, WiFi.RSSI(uint8_t(i)), WiFi.channel(uint8_t(i)), auth != WIFI_AUTH_OPEN,
      auth == WIFI_AUTH_OPEN || auth == WIFI_AUTH_WPA_PSK || auth == WIFI_AUTH_WPA2_PSK ||
      auth == WIFI_AUTH_WPA_WPA2_PSK || auth == WIFI_AUTH_WPA2_WPA3_PSK};
    auto existing = std::find_if(wifiScan.networks.begin(), wifiScan.networks.end(),
      [&ssid](const WifiNetwork &value) { return value.ssid == ssid; });
    if (existing != wifiScan.networks.end()) {
      if (network.rssi > existing->rssi) *existing = network;
    } else if (wifiScan.networks.size() < 24) wifiScan.networks.push_back(network);
    else {
      auto weakest = std::min_element(wifiScan.networks.begin(), wifiScan.networks.end(),
        [](const WifiNetwork &a, const WifiNetwork &b) { return a.rssi < b.rssi; });
      if (network.rssi > weakest->rssi) *weakest = network;
    }
  }
  std::sort(wifiScan.networks.begin(), wifiScan.networks.end(),
    [](const WifiNetwork &a, const WifiNetwork &b) { return a.rssi > b.rssi; });
  WiFi.scanDelete(); wifiScan.state = WifiScan::State::Ready;
}
bool scanAllowed() {
  if (hasSession()) return true;
#ifdef SPOOLANCER_HEADLESS
  if (Setup::required(config.auth) && viaSetupAp() && !setupWindow.expired(millis()) &&
      Setup::trustedHeaders(server.hostHeader().c_str(), server.header("Origin").c_str())) return true;
#endif
  error(403, "Open a device session or join the temporary first-run setup Wi-Fi to scan."); return false;
}
void scanReply() {
  JsonDocument doc;
  doc["state"] = wifiScan.state == WifiScan::State::Scanning ? "scanning" :
                 wifiScan.state == WifiScan::State::Failed ? "failed" : "ready";
  JsonArray networks = doc["networks"].to<JsonArray>();
  if (wifiScan.state == WifiScan::State::Ready) for (const auto &network : wifiScan.networks) {
    JsonObject item = networks.add<JsonObject>();
    item["ssid"] = network.ssid; item["rssi"] = network.rssi; item["channel"] = network.channel;
    item["secure"] = network.secure; item["supported"] = network.supported;
  }
  reply(doc);
}
void startAp() {
  if (apActive) return;
#ifdef SPOOLANCER_HEADLESS
  cancelWifiScan(); WiFi.setAutoReconnect(false);
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
  if (!Setup::complete(config.auth)) return;
  if (config.ssid.isEmpty()) { startAp(); return; }
  cancelWifiScan(); WiFi.setAutoReconnect(true);
  WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(config.ssid.c_str(), config.password.c_str());
  connectStarted = millis(); connectionAttempt = true;
}
void setupRoutes() {
  const char *headers[] = {"Authorization", "Content-Type", "Content-Length", "Origin"};
  server.collectHeaders(headers, 4);
  server.on("/api/status", HTTP_GET, statusRoute);
  server.on("/api/setup", HTTP_POST, [] {
#ifdef SPOOLANCER_HEADLESS
    JsonDocument doc; if (!body(doc)) return;
    if (!setupWindow.consume(millis(), Setup::required(config.auth), viaSetupAp(), WiFi.status() != WL_CONNECTED,
                             Setup::trustedHeaders(server.hostHeader().c_str(), server.header("Origin").c_str()))) {
      error(403, "First-run setup only: join the temporary Wi-Fi and open http://192.168.4.1 directly. Wait two seconds between attempts."); return;
    }
    if (!textWithoutNul(doc["wifi_ssid"]) || !doc["wifi_open"].is<bool>() ||
        !doc["pin_required"].is<bool>() ||
        (!doc["wifi_password"].isUnbound() && !textWithoutNul(doc["wifi_password"]))) {
      error(400, "Choose Wi-Fi, explicitly select open or secured, and choose whether a PIN is required."); return;
    }
    Config changed = config;
    changed.ssid = doc["wifi_ssid"].as<String>(); changed.password = doc["wifi_password"] | "";
    if (changed.ssid.length() != std::strlen(changed.ssid.c_str()) ||
        changed.password.length() != std::strlen(changed.password.c_str()) ||
        !Setup::validWifi(changed.ssid.c_str(), changed.password.c_str(), doc["wifi_open"].as<bool>())) {
      error(400, "SSID must be 1–32 UTF-8 bytes. Secured Wi-Fi needs an 8–63 byte password or 64 hexadecimal digits; open Wi-Fi needs an empty password."); return;
    }
    const bool requirePin = doc["pin_required"].as<bool>();
    if (requirePin && (!textWithoutNul(doc["pin"]) ||
        doc["pin"].as<String>().length() != 8 || !Setup::validPin(doc["pin"].as<const char *>()))) {
      error(400, "Choose exactly 8 digits for the device PIN."); return;
    }
    if (!requirePin && !doc["pin"].isUnbound() &&
        (!textWithoutNul(doc["pin"]) || !doc["pin"].as<String>().isEmpty())) {
      error(400, "Omit the PIN when PIN protection is disabled."); return;
    }
    changed.pin = requirePin ? doc["pin"].as<String>() : "";
    changed.auth = requirePin ? Setup::AuthMode::Protected : Setup::AuthMode::Open;
    if (!saveConfig(changed)) { error(507, "Could not save setup. No settings changed."); return; }
    config = changed; pin = config.pin;
    for (auto &session : sessions) session.token = "";
    nextLogin = 0; setupWindow.begin(millis());
    appMessage = "Setup saved. Connecting to your Wi-Fi. Reconnect this browser to the same LAN.";
    JsonDocument result; result["ok"] = true; result["reconnect_required"] = true;
    result["pin_required"] = Setup::pinRequired(config.auth); result["ap_ssid"] = apName;
    result["message"] = appMessage; sessionReply(result);
    reconnectAt = millis() + 1000;
#else
    error(403, "This display variant uses its generated device PIN. Open a session and configure Wi-Fi in Settings.");
#endif
  });
  server.on("/api/session", HTTP_POST, [] {
    if (!trustedSessionRequest()) { error(403, "Open the device's current local IP address or http://spoolancer.local directly."); return; }
    if (!Setup::complete(config.auth)) { error(403, "Complete first-run setup, or hold BOOT 8 seconds to recover invalid setup."); return; }
    if (int32_t(nextLogin - millis()) > 0) { error(429, "Wait before opening another device session."); return; }
    nextLogin = millis() + 1500;
    JsonDocument doc; if (!body(doc)) return;
    if (doc["setup"] == true) { error(403, "Use /api/setup for first-run Wi-Fi and optional PIN setup."); return; }
    if (Setup::pinRequired(config.auth) && (!textWithoutNul(doc["pin"]) || doc["pin"].as<String>() != pin)) {
      if (++loginFailures >= 5) { nextLogin = millis() + 60000; loginFailures = 0; }
      error(401, "Incorrect device PIN."); return;
    }
    JsonDocument result; result["ok"] = true; sessionReply(result);
  });
  server.on("/api/wifi/scan", HTTP_GET, [] {
    if (!scanAllowed()) return;
    pollWifiScan(); scanReply();
  });
  server.on("/api/wifi/scan", HTTP_POST, [] {
    if (!scanAllowed()) return;
    JsonDocument doc; if (!body(doc)) return;
    pollWifiScan();
    if (wifiScan.state == WifiScan::State::Scanning) { scanReply(); return; }
    if (wifiScan.radioPending || WiFi.scanComplete() == WIFI_SCAN_RUNNING || reconnectAt || connectionAttempt) {
      error(409, "Wait for the current Wi-Fi operation to finish before scanning."); return;
    }
    if (wifiScan.started && uint32_t(millis() - wifiScan.startedAt) < 10000) {
      error(429, "Wait 10 seconds between Wi-Fi scans."); return;
    }
    if (Setup::required(config.auth)) {
      WiFi.setAutoReconnect(false);
      WiFi.mode(WIFI_AP_STA);
      WiFi.disconnect(false, true);
    } else WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
    wifiScan.started = true; wifiScan.startedAt = millis(); wifiScan.networks.clear();
    const int16_t count = WiFi.scanNetworks(true, false, false, 300);
    wifiScan.radioPending = count == WIFI_SCAN_RUNNING || count >= 0;
    wifiScan.state = wifiScan.radioPending ? WifiScan::State::Scanning : WifiScan::State::Failed;
    pollWifiScan(); scanReply();
  });
  server.on("/api/spools", HTTP_GET, [] {
    if (!authorized()) return;
    refreshRemoteInventory(server.arg("refresh") == "1");
    JsonDocument doc; JsonArray list = doc["spools"].to<JsonArray>();
    for (const auto &s : spools) if (visibleSpool(s)) spoolJson(list.add<JsonObject>(), s);
    inventoryJson(doc["inventory"].to<JsonObject>());
    JsonArray drafts = doc["drafts"].to<JsonArray>();
    if (!config.spoolman.isEmpty()) for (const auto &s : spools) if ((!s.remote && !s.remoteId) || !s.pending.isEmpty() || inactiveTagLink(s)) spoolJson(drafts.add<JsonObject>(), s);
    reply(doc);
  });
  server.on("/api/spools", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    if (!storeReady) { error(503, "Inventory storage is unavailable."); return; }
    Spool *existing = nullptr;
    if (!doc["id"].isNull()) {
      if (!doc["id"].is<const char *>() || !(existing = findStoredSpool(doc["id"].as<String>()))) { error(404, "Spool not found."); return; }
    }
    if (existing && SpoolActions::archiving(existing->pending.c_str())) { error(409, "Check this spool's pending archive status before editing it."); return; }
    String mergeCacheId;
    const bool bindOnly = existing && existing->remote && existing->pending.isEmpty();
    Spool value = existing ? *existing : Spool();
    if (bindOnly) {
      if (!doc["tag_uid"].is<const char *>() || doc.size() != 2) { error(409, "Edit this spool's profile in Spoolman. Only its tag binding can be changed here."); return; }
      if (inventoryStale()) { error(409, "Refresh the Spoolman inventory before linking this tag."); return; }
    } else if (!validateSpool(doc.as<JsonObjectConst>(), value)) { error(400, "Invalid spool fields. Check names, color, weights, density and diameter."); return; }
    const size_t localCount = std::count_if(spools.begin(), spools.end(), [](const Spool &s) { return !s.remote; });
    if (!existing && (localCount >= maxLocalSpools || spools.size() >= maxSpools)) { error(409, "Device local draft limit is 100 spools, with up to 200 remote records."); return; }
    const bool create = !config.spoolman.isEmpty() && !bindOnly && !value.remoteId;
    if (create && (!doc["confirm"].is<bool>() || !doc["confirm"].as<bool>())) { error(400, "Confirm creation in Spoolman before saving this new spool."); return; }
    if (create && existing && !existing->pending.isEmpty()) { error(409, "A prior creation has an unknown outcome. Check Spoolman and link the existing spool ID before retrying."); return; }
    const bool inferEmpty = doc["infer_empty_from_measurement"] == true;
    if (inferEmpty || create) {
      if (!doc["spool_condition"].is<const char *>() || (doc["spool_condition"] != "full" && doc["spool_condition"] != "used")) {
        error(400, "Specify whether this is a new full spool or a used spool."); return;
      }
      const bool full = doc["spool_condition"] == "full";
      if (inferEmpty && (!full || !scale.calibrated() || !scale.stable() || !scale.fresh() ||
          !Domain::inRange(scale.gross(), 20, 2000) || !Domain::inRange(scale.gross() - value.initial, 0, 2000) ||
          !std::isfinite(value.empty) || std::abs(value.empty - (scale.gross() - value.initial)) > 1)) {
        error(409, "The full-spool measurement changed. Weigh it again before estimating the empty spool weight."); return;
      }
      if (inferEmpty) value.empty = scale.gross() - value.initial;
      if (create) {
        if (full) value.lastRemaining = value.initial;
        else {
          // The cradle reading must belong to this spool, not to another selected spool.
          if (!activeId.isEmpty() && (!existing || activeId != existing->id)) {
            error(409, "Another spool is selected on the scale. Place this spool alone in the cradle and clear the selection before creating it as used."); return;
          }
          if (!Domain::measurementAllowed(scale.gross(), value.empty, value.initial, scale.calibrated(), scale.stable(), scale.fresh())) {
            error(409, "A used spool needs a fresh stable calibrated measurement and a known empty spool weight before creating it in Spoolman."); return;
          }
          value.lastGross = scale.gross(); value.lastRemaining = value.lastGross - value.empty; ++value.measurement;
        }
      }
    }
    if (existing && existing->remoteId > 0 && value.remoteId != existing->remoteId) {
      value.vendorId = value.filamentId = 0; value.pending = "";
    }
    if (!config.spoolman.isEmpty() && !bindOnly && value.remoteId > 0) {
      // Linking a prior ambiguous creation uses the actual server record, never submitted local metadata.
      JsonDocument response; int code; Spool remote;
      if (!remoteRequest("GET", "/spool/" + String(value.remoteId), nullptr, response, code) ||
          !parseRemoteSpool(response.as<JsonObjectConst>(), remote) || remote.remoteId != value.remoteId || !remote.remoteListed) {
        error(409, "Could not confirm an active Spoolman spool with that ID. The existing journal was kept."); return;
      }
      for (const auto &other : spools) if ((!existing || other.id != existing->id) && other.remoteId == value.remoteId) {
        if (!existing || existing->pending.isEmpty() || !other.remote || !other.pending.isEmpty() ||
            (!other.tagUid.isEmpty() && !value.tagUid.isEmpty() && other.tagUid != value.tagUid) || !mergeCacheId.isEmpty()) {
          error(409, "This Spoolman spool already has a record or conflicting tag binding. Select it directly or resolve its tag links before reconciling."); return;
        }
        mergeCacheId = other.id;
        if (value.tagUid.isEmpty()) value.tagUid = other.tagUid;
      }
      remote.id = value.id; remote.tagUid = value.tagUid; remote.measurement = value.measurement;
      remote.lastGross = value.lastGross; remote.lastRemaining = value.lastRemaining; value = remote;
    }
    if (value.remoteId > 0) value.pending = "";
    if (doc["tag_uid"].is<const char *>()) {
      String requested = doc["tag_uid"].as<String>();
      if (!textWithoutNul(doc["tag_uid"]) || (!requested.isEmpty() && !SpoolActions::validUid(requested.c_str()))) { error(400, "Invalid tag UID. Scan the physical tag first."); return; }
      if (!requested.isEmpty() && (!nfc.scanIdentity() || requested != nfc.uid)) { error(409, "Tag changed or multiple tags detected. Rescan before linking its UID."); return; }
      for (const auto &s : spools) if (s.id != value.id && s.id != mergeCacheId && !requested.isEmpty() && s.tagUid == requested) { error(409, "This tag already identifies another spool."); return; }
      value.tagUid = requested;
    }
    Spool before; std::vector<Spool> beforeMerge;
    if (!mergeCacheId.isEmpty()) beforeMerge = spools;
    if (existing) { before = *existing; *existing = value; }
    else { value.id = randomHex(8); spools.push_back(value); }
    if (!mergeCacheId.isEmpty()) {
      spools.erase(std::remove_if(spools.begin(), spools.end(), [&](const Spool &s) { return s.id == mergeCacheId; }), spools.end());
      existing = findStoredSpool(value.id);
    }
    if (!saveInventory()) {
      if (!mergeCacheId.isEmpty()) spools = std::move(beforeMerge);
      else if (existing) *existing = before; else spools.pop_back();
      error(507, "Could not save the inventory."); return;
    }
    if (!mergeCacheId.isEmpty() && activeId == mergeCacheId) chooseSpool(value.id);
    Spool &saved = existing ? *existing : spools.back();
    if (create && !syncSpool(saved)) {
      JsonDocument result; result["error"] = syncError; result["draft"] = true;
      result["recovery_required"] = !saved.pending.isEmpty(); spoolJson(result["spool"].to<JsonObject>(), saved);
      reply(result, 502); return;
    }
    if (create && !saveInventory()) {
      JsonDocument result; result["error"] = "Spoolman confirmed creation, but the final cache could not be saved. Keep the returned spool ID; do not create it again.";
      result["draft"] = true; result["recovery_required"] = true; spoolJson(result["spool"].to<JsonObject>(), saved);
      reply(result, 507); return;
    }
    JsonDocument result; result["ok"] = true; result["inventory_source"] = config.spoolman.isEmpty() ? "local" : "spoolman";
    spoolJson(result["spool"].to<JsonObject>(), saved); reply(result);
    if (create) { remoteInventory.attempted = false; remoteInventory.failed = true; } // Next inventory request reads the complete server list.
  });
  server.on("/api/spools/tag", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    if (!storeReady) { error(503, "Inventory storage is unavailable."); return; }
    if (!textWithoutNul(doc["id"]) || !Domain::validId(doc["id"])) { error(400, "A valid spool id is required."); return; }
    if (!doc["unlink"].isUnbound() && !doc["unlink"].is<bool>()) { error(400, "unlink must be true or false."); return; }
    const bool unlink = doc["unlink"] == true;
    Spool *s = unlink ? findStoredSpool(doc["id"].as<String>()) : findSpool(doc["id"].as<String>());
    if (!s) { error(404, "Spool not found."); return; }
    if (!s->pending.isEmpty()) { error(409, "Resolve the pending Spoolman operation before linking a tag."); return; }
    if (unlink) {
      if (!textWithoutNul(doc["expected_uid"])) { error(400, "Confirm the currently saved RFID tag ID before removing its link."); return; }
      const String expected = doc["expected_uid"].as<String>();
      const auto check = SpoolActions::unlink(s->tagUid.c_str(), expected.c_str(), s->pending.c_str());
      if (check == SpoolActions::Unlink::InvalidUid) { error(400, "The saved RFID tag ID must contain 1–32 characters."); return; }
      if (check != SpoolActions::Unlink::Allowed) { error(409, "The saved RFID link changed. Refresh and confirm the current tag ID before removing it."); return; }
      const String previous = s->tagUid; s->tagUid = "";
      if (!saveInventory()) { s->tagUid = previous; error(507, "Could not save the inventory. The RFID link was kept."); return; }
      if (nfcSpoolId == s->id) { nfcSpoolId = ""; nfcMetadata.clear(); }
      JsonDocument result; result["ok"] = true;
      result["message"] = "RFID link removed from this scale. The tag, spool profile and Spoolman were not changed.";
      spoolJson(result["spool"].to<JsonObject>(), *s); reply(result); return;
    }
    if (s->remote && inventoryStale()) { error(409, "Refresh the Spoolman inventory before linking this tag."); return; }
    if (!textWithoutNul(doc["expected_uid"]) || !SpoolActions::validUid(doc["expected_uid"])) { error(400, "Scan and confirm the target tag UID first."); return; }
    const String expected = doc["expected_uid"].as<String>();
    startStatusOperation(StatusModel::Operation::Nfc);
    if (!nfc.scanIdentity() || nfc.uid != expected) {
      finishStatusOperation(false); error(409, "Tag changed, disappeared, or multiple tags were detected. Scan one tag again before linking it."); return;
    }
    for (const auto &other : spools) if (other.id != s->id && other.tagUid == expected) {
      finishStatusOperation(false); error(409, "This tag already identifies another spool. Change or remove that spool's link first."); return;
    }
    const String oldUid = s->tagUid; s->tagUid = expected;
    if (!saveInventory()) {
      s->tagUid = oldUid; finishStatusOperation(false); error(507, "Could not save the tag link. The existing binding was kept."); return;
    }
    nfcMetadata.clear(); nfcSpoolId = s->id; chooseSpool(s->id); nextNfc = millis() + 2000;
    finishStatusOperation(true);
    JsonDocument result; result["ok"] = true;
    result["message"] = "RFID linked to this spool on the scale. Tag contents were not changed.";
    spoolJson(result["spool"].to<JsonObject>(), *s); reply(result);
  });
  server.on("/api/spools/remove", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    if (!storeReady) { error(503, "Inventory storage is unavailable."); return; }
    if (!textWithoutNul(doc["id"]) || !Domain::validId(doc["id"])) { error(400, "A valid spool id is required."); return; }
    Spool *s = findStoredSpool(doc["id"].as<String>());
    if (!s) { error(404, "Spool not found."); return; }
    const auto action = SpoolActions::removal(s->remoteId, !config.spoolman.isEmpty(), s->pending.c_str());
    if (action == SpoolActions::Removal::Unresolved) { error(409, "Resolve the pending Spoolman creation before removing this spool. Its recovery record was kept."); return; }
    if (action == SpoolActions::Removal::MissingServer) { error(409, "Reconnect the original Spoolman server to archive this linked spool."); return; }
    const bool remote = action == SpoolActions::Removal::Archive;
    bool removedMissing = false;
    if (remote) {
      if (WiFi.status() != WL_CONNECTED) { error(409, "Connect Wi-Fi before archiving a Spoolman spool. No local record was removed."); return; }
      if (!SpoolActions::archiving(s->pending.c_str()) && inventoryStale()) { error(409, "Refresh the Spoolman inventory before archiving this spool."); return; }
      if (!archiveSpool(*s, removedMissing)) {
        remoteInventory.failed = true; inventoryError = syncError;
        JsonDocument result; result["error"] = syncError;
        result["recovery_required"] = SpoolActions::archiving(s->pending.c_str());
        spoolJson(result["spool"].to<JsonObject>(), *s); reply(result, 502); return;
      }
    }
    const size_t index = size_t(s - spools.data());
    const Spool previous = *s; const String id = s->id;
    spools.erase(spools.begin() + index);
    if (!saveInventory()) {
      spools.insert(spools.begin() + index, previous);
      JsonDocument result; result["error"] = removedMissing ? "Spoolman confirmed the spool is missing, but the device could not save the cleanup. Check archive status again; the recovery record was kept." : remote ? "Spoolman confirmed the archive, but the device could not save it. Check archive status to finish; the recovery record was kept." : "Could not save the inventory. The spool and selection were kept.";
      result["recovery_required"] = remote; spoolJson(result["spool"].to<JsonObject>(), previous); reply(result, 507); return;
    }
    if (activeId == id) chooseSpool("");
    if (nfcSpoolId == id) { nfcSpoolId = ""; nfcMetadata.clear(); }
    if (remote) { remoteInventory.attempted = false; remoteInventory.failed = true; inventoryError = "Refresh to load the remaining Spoolman inventory."; }
    JsonDocument result; result["ok"] = true; result["id"] = id; result["archived"] = remote && !removedMissing; result["removed_missing"] = removedMissing;
    result["message"] = removedMissing ? "Spoolman confirmed this spool is already missing. Its local recovery record and RFID link were removed; nothing was archived or changed on the server." : remote ? "Spool archived in Spoolman and removed from the active inventory. Its history remains in Spoolman." : "Spool removed from this scale. The physical tag was not changed.";
    reply(result);
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
  server.on("/api/diagnostics", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument input; if (!body(input)) return;
    if (!textWithoutNul(input["component"])) { error(400, "Choose nfc, leds or scale."); return; }
    const String component = input["component"].as<String>();
    if (component != "nfc" && component != "leds" && component != "scale") { error(400, "Choose nfc, leds or scale."); return; }
    JsonDocument result; result["ok"] = true; result["component"] = component;
    if (component == "leds") {
      if (!StatusUi::testLeds(millis())) { error(409, "This firmware profile has no addressable status LEDs."); return; }
      renderStatusUi(true);
      result["state"] = "observe"; result["duration_ms"] = StatusModel::ledTestMs;
      result["message"] = "Watch all five LEDs: red, green, blue, one white LED at a time, then a 0–100% bar. Normal status returns after 12 seconds. This checks the output signal; confirm the light and color order visually.";
    } else if (component == "nfc") {
      startStatusOperation(StatusModel::Operation::Nfc);
      nfc.begin(nfcIdle);
      const bool detected = nfc.scanIdentity();
      nfcSpoolId = ""; nfcMetadata.clear(); nextNfc = millis() + 2000;
      result["reader_available"] = nfc.available; result["tag_detected"] = detected;
      result["targets"] = nfc.targets; result["uid"] = nfc.uid; result["type"] = nfc.type;
      result["state"] = detected ? "pass" : nfc.available && nfc.targets < 2 ? "warning" : "fail";
      result["message"] = detected ? "One tag ID was read. Reader communication and tag detection passed. No tag data or spool profile was changed." :
        nfc.available && nfc.targets < 2 ? "PN532 is responding, but no single tag was read. Hold one tag beside the antenna and test again." :
        nfc.targets > 1 ? "Multiple tags detected. Keep only one tag beside the antenna and test again." :
        "PN532 did not respond. Check power, HSU/UART mode and crossed TX/RX connections, then retry.";
      finishStatusOperation(detected);
    } else {
      startStatusOperation(StatusModel::Operation::Scale);
      scale.tick();
      result["fresh"] = scale.fresh(); result["stable"] = scale.stable();
      result["calibrated"] = scale.calibrated(); result["sample_count"] = scale.sampleCount();
      numberOrNull(result["raw_mean"], scale.mean()); numberOrNull(result["gross_g"], scale.gross());
      result["state"] = !scale.fresh() ? "fail" : !scale.calibrated() || !scale.stable() ? "warning" : "pass";
      result["message"] = !scale.fresh() ? "No fresh HX711 samples. Check its power, DOUT and clock wiring, then retry." :
        !scale.calibrated() ? "HX711 samples are arriving. Complete Calibration to convert them to grams." :
        !scale.stable() ? "HX711 samples are arriving, but the weight is settling. Keep the cradle still and retry." :
        "Fresh, stable weight samples are arriving. Add and remove a known weight to check response. Use Calibration to verify accuracy.";
      finishStatusOperation(scale.fresh());
    }
    reply(result);
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
    if (!nfc.present || nfc.targets != 1 || nfc.uid.isEmpty()) { error(409, nfc.lastError.isEmpty() ? "Align exactly one tag with the reader." : nfc.lastError); return; }
    JsonDocument result; result["ok"] = true; nfcJson(result["nfc"].to<JsonObject>());
    if (nfcSpoolId.isEmpty()) result["spool_id"] = nullptr; else result["spool_id"] = nfcSpoolId;
    result["message"] = nfc.lastMessage; reply(result);
  });
  server.on("/api/nfc/write", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    Spool *s = requestedSpool(doc); if (!s) return;
    if (!doc["expected_uid"].is<const char *>() || doc["expected_uid"].as<String>().isEmpty()) { error(400, "Scan and confirm the target tag UID first."); return; }
    BrandTags::Format format = BrandTags::Format::Spoolancer;
    if (!doc["format"].isNull() && !BrandTags::parseFormat(doc["format"].as<const char *>(), format)) { error(400, "Unknown tag format."); return; }
    const String problem = writeSpoolTag(*s, doc["expected_uid"].as<String>(), format);
    if (!problem.isEmpty()) { error(409, problem); return; }
    ok(format == BrandTags::Format::Spoolancer ? "Tag written and read-back verified. Printer tags were not modified." :
       String(BrandTags::info(format).label) + " tag written and read-back verified. Its ID is linked to this spool.");
  });
  server.on("/api/nfc/clone", HTTP_POST, [] {
    // Two-step Bambu clone: read a genuine tag, then copy it byte-for-byte onto a magic card.
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    const String step = doc["step"] | "";
    startStatusOperation(StatusModel::Operation::Nfc);
    if (step == "read") {
      cloneReady = false;
      if (!nfc.scanIdentity() || !nfc.classic1k()) { finishStatusOperation(false); error(409, nfc.lastError.isEmpty() ? "Present one genuine Bambu Lab tag (MIFARE Classic 1K)." : nfc.lastError); return; }
      ReadTags::Decoded tag;
      if (!nfc.readClassic(nfc.uid, tag) || !tag.valid) { finishStatusOperation(false); error(409, nfc.lastError); return; }
      if (nfc.type != "Bambu Lab (read-only)") { finishStatusOperation(false); error(409, "Only Bambu Lab tags can be cloned. Snapmaker tags are signed over their keys and cannot be cloned."); return; }
      if (!nfc.readClassicDump(nfc.uid, cloneImage)) { finishStatusOperation(false); error(409, "Could not read the full tag. Hold it still and try again."); return; }
      std::memcpy(cloneUid, nfc.uidData(), 4); cloneReady = true;
      cloneLabel = String(tag.brand) + " " + tag.material + " " + tag.color;
      JsonDocument result; result["ok"] = true; result["step"] = "read"; result["source_uid"] = nfc.uid;
      result["label"] = cloneLabel; result["message"] = "Source tag read. Put a blank Gen2/FUID magic card on the reader and confirm the copy.";
      finishStatusOperation(true); reply(result); return;
    }
    if (step == "write") {
      if (!confirmation(doc)) { finishStatusOperation(false); return; }
      if (!cloneReady) { finishStatusOperation(false); error(409, "Read a source tag first."); return; }
      const bool done = nfc.cloneClassic(cloneImage, cloneUid);
      finishStatusOperation(done);
      if (!done) { error(409, nfc.lastError); return; }
      cloneReady = false;
      ok("Magic card cloned and read-back verified. The printer will treat it as the original spool.");
      return;
    }
    finishStatusOperation(false); error(400, "Clone step must be \"read\" or \"write\".");
  });
  server.on("/api/config", HTTP_GET, [] {
    if (!authorized()) return;
    JsonDocument doc; doc["device_name"] = config.name; doc["wifi_ssid"] = config.ssid;
    doc["spoolman_url"] = config.spoolman; doc["has_wifi_password"] = !config.password.isEmpty();
    doc["wifi_open"] = config.password.isEmpty(); doc["pin_required"] = Setup::pinRequired(config.auth); reply(doc);
  });
  server.on("/api/config", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    Config changed = config;
    if (!doc["device_name"].isUnbound() && !textField(doc.as<JsonObjectConst>(), "device_name", changed.name, 48)) { error(400, "Device name must be 1–48 characters."); return; }
    const bool wifiChanged = !doc["wifi_ssid"].isUnbound() || !doc["wifi_password"].isUnbound() ||
                             !doc["wifi_open"].isUnbound() || !doc["clear_wifi_password"].isUnbound();
    if (wifiChanged) {
      if ((!doc["wifi_ssid"].isUnbound() && !textWithoutNul(doc["wifi_ssid"])) ||
          (!doc["wifi_password"].isUnbound() && !textWithoutNul(doc["wifi_password"])) ||
          (!doc["wifi_open"].isUnbound() && !doc["wifi_open"].is<bool>()) ||
          (!doc["clear_wifi_password"].isUnbound() && !doc["clear_wifi_password"].is<bool>())) {
        error(400, "Invalid Wi-Fi fields. SSID/password must be text; wifi_open must be true or false."); return;
      }
      if (!doc["wifi_ssid"].isUnbound()) changed.ssid = doc["wifi_ssid"].as<String>();
      const String password = doc["wifi_password"] | "";
      bool openPresent = !doc["wifi_open"].isUnbound(), open = doc["wifi_open"] | false;
      if (doc["clear_wifi_password"] == true) {
        if ((openPresent && !open) || (changed.ssid != config.ssid && !open)) {
          error(400, "Explicitly set wifi_open to true for an open network with a new SSID."); return;
        }
        openPresent = open = true;
      }
      const char *resolved = nullptr;
      if (changed.ssid.length() != std::strlen(changed.ssid.c_str()) || password.length() != std::strlen(password.c_str()) ||
          !Setup::resolveWifiPassword(config.ssid.c_str(), config.password.c_str(), changed.ssid.c_str(),
                                     password.c_str(), openPresent, open, resolved)) {
        error(400, "Use a 1–32 byte SSID. A new secured network needs an 8–63 byte password or 64 hex digits; choose wifi_open for an open network. Blank passwords only keep credentials for the same SSID."); return;
      }
      changed.password = resolved;
    }
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
    const bool serverChanged = changed.spoolman != config.spoolman;
    const bool reconnect = changed.ssid != config.ssid || changed.password != config.password;
    if (!saveConfig(changed)) { error(507, "Could not save configuration."); return; }
    config = changed;
    if (serverChanged) { remoteInventory = {}; inventoryError = ""; syncError = ""; syncMessage = ""; }
    JsonDocument result; result["ok"] = true; result["reconnect_required"] = reconnect;
    result["ap_ssid"] = apName; result["ip"] = "192.168.4.1";
#ifdef SPOOLANCER_HEADLESS
    result["message"] = reconnect ? "Configuration saved; Wi-Fi will reconnect. The temporary setup network closes after connection or its 10-minute limit. Hold BOOT 8 seconds while running to recover." : "Configuration saved.";
#else
    result["message"] = reconnect ? "Configuration saved; Wi-Fi will reconnect. Setup AP remains available if connection fails." : "Configuration saved.";
#endif
    reply(result);
    if (reconnect) {
#ifdef SPOOLANCER_HEADLESS
      if (apActive) setupWindow.begin(millis());
#endif
      reconnectAt = millis() + 1000;
    }
  });
  server.on("/api/update", HTTP_GET, [] {
    if (!authorized()) return;
    if (WiFi.status() != WL_CONNECTED) { error(409, "Connect the scale to your local Wi-Fi before checking for updates."); return; }
    const Ota::Release rel = Ota::fetchLatest(deviceVariant);
    JsonDocument doc; doc["current"] = SPOOLANCER_VERSION; doc["variant"] = deviceVariant;
    if (!rel.ok) { doc["error"] = rel.error; reply(doc); return; }
    doc["latest"] = rel.tag;
    doc["update_available"] = Ota::updateAvailable(SPOOLANCER_VERSION, rel.tag.c_str());
    doc["notes_url"] = rel.notesUrl; doc["has_filesystem"] = !rel.fsUrl.isEmpty();
    reply(doc);
  });
  server.on("/api/update", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument in; if (!body(in) || !confirmation(in)) return;
    if (WiFi.status() != WL_CONNECTED) { error(409, "Connect the scale to your local Wi-Fi before updating."); return; }
    if (otaRequested) { error(409, "An update is already in progress."); return; }
    const Ota::Release rel = Ota::fetchLatest(deviceVariant);
    if (!rel.ok) { error(502, rel.error); return; }
    if (!Ota::updateAvailable(SPOOLANCER_VERSION, rel.tag.c_str())) { error(409, "This device already runs the latest release."); return; }
    otaPending = rel; otaRequested = true;
    JsonDocument result; result["ok"] = true; result["latest"] = rel.tag;
    result["message"] = "Update started. The device is downloading " + rel.tag + " and reboots in about a minute. Do not power it off.";
    reply(result);
  });
  server.on("/api/update/upload", HTTP_POST,
    [] {  // Completion handler: runs after the whole multipart body is received.
      if (!uploadDidStart) { error(400, "No firmware file was received."); return; }
      uploadDidStart = false;
      if (!uploadError.isEmpty()) { const String e = uploadError; uploadError = ""; error(507, e); return; }
      JsonDocument r; r["ok"] = true;
      r["message"] = "Firmware uploaded and verified. The device reboots in a moment to run it.";
      reply(r);
      rebootAt = millis() + 1500;  // Reboot after the response is flushed.
    },
    [] {  // Upload handler: streams the firmware image straight into the inactive OTA slot.
      HTTPUpload &up = server.upload();
      if (up.status == UPLOAD_FILE_START) {
        uploadDidStart = true; uploadError = "";
        if (!hasSession()) { uploadError = "Open a device session before uploading firmware."; return; }
        if (server.arg("confirm") != "true") { uploadError = "Explicit confirmation is required to flash uploaded firmware."; return; }
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) { uploadError = Update.errorString(); }
      } else if (up.status == UPLOAD_FILE_WRITE) {
        if (!uploadError.isEmpty()) return;  // Already rejected; drain the body.
        if (Update.write(up.buf, up.currentSize) != up.currentSize) uploadError = Update.errorString();
      } else if (up.status == UPLOAD_FILE_END) {
        if (!uploadError.isEmpty()) { Update.abort(); return; }
        if (!Update.end(true)) uploadError = Update.errorString();
      } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort(); uploadError = "The firmware upload was interrupted.";
      }
    });
  server.on("/api/spoolman/discover", HTTP_GET, [] {
    if (!authorized()) return;
    discoveryReply();
  });
  server.on("/api/spoolman/discover", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument input; if (!body(input)) return;
    if (discovery.state == SpoolmanDiscovery::State::Searching) { discoveryReply(); return; }
    if (WiFi.status() != WL_CONNECTED) { error(409, "Connect the scale to your local Wi-Fi before looking for Spoolman."); return; }
    if (discovery.attempted && uint32_t(millis() - discovery.started) < 60000) { error(429, "Wait one minute before another server search."); return; }
    startDiscovery(); discoveryReply();
  });
  server.on("/api/spoolman/test", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument input; if (!body(input)) return;
    syncError = "";
    startStatusOperation(StatusModel::Operation::Sync);
    const bool reached = !config.spoolman.isEmpty() && refreshRemoteInventory(true);
    finishStatusOperation(reached);
    if (!reached) { error(502, syncError.isEmpty() ? "Unexpected Spoolman API response." : syncError); return; }
    syncMessage = "Spoolman connected. Inventory is loaded from the server."; ok(syncMessage);
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
      reset.pin = generatedPin(); reset.auth = Setup::AuthMode::Protected;
      if (!saveConfig(reset)) { appMessage = "Could not save Wi-Fi reset."; allDown = now; return; }
      config = reset; pin = config.pin; for (auto &s : sessions) s.token = "";
      loginFailures = 0; nextLogin = 0;
      reconnectAt = 0; connectionAttempt = false; cancelWifiScan();
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
      cycleVisibleSpool(false);
    }
    previousDown = 0; previousDone = false;
  }
  if (next && !nextWasDown && now - lastCycle > 200 && !spools.empty()) {
    cycleVisibleSpool(true); lastCycle = now;
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
  if (gesture == Setup::Gesture::ResetWifi) {
    Config reset = config; reset.ssid = reset.password = "";
    reset.pin = ""; reset.auth = Setup::AuthMode::Unconfigured;
    if (!saveConfig(reset)) { appMessage = "Could not save Wi-Fi reset."; return; }
    pin = ""; loginFailures = 0; nextLogin = 0;
    config = reset; for (auto &session : sessions) session.token = "";
    reconnectAt = 0; connectionAttempt = false; cancelWifiScan();
    WiFi.disconnect(true, true); WiFi.softAPdisconnect(true); apActive = false; startAp();
    appMessage = "Wi-Fi and pairing reset. Inventory and calibration kept. Join the temporary open setup network.";
  }
}
#endif
}  // namespace

void setup() {
  Serial.begin(115200);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
#ifdef SPOOLANCER_HEADLESS
  pinMode(Pins::wifiReset, INPUT_PULLUP);
#else
  pinMode(Pins::previous, INPUT_PULLUP); pinMode(Pins::next, INPUT_PULLUP); pinMode(Pins::select, INPUT_PULLUP);
#endif
#ifdef SPOOLANCER_TOUCH
  pinMode(Pins::sdSelect, OUTPUT); digitalWrite(Pins::sdSelect, HIGH);
#endif
  preferences.begin("spoolancer", false);
  JsonDocument saved;
  if (!deserializeJson(saved, preferences.getString("config", "{}")) && saved.is<JsonObject>()) {
    config.name = saved["name"] | "Spoolancer"; config.ssid = saved["ssid"] | "";
    config.password = saved["password"] | ""; config.spoolman = saved["spoolman"] | "";
    const String legacyPin = preferences.getString("pin", "");
    Setup::StoredAuth auth;
    auth.hasFields = !saved["setup_complete"].isUnbound() || !saved["pin_required"].isUnbound() || !saved["pin"].isUnbound();
    auth.fieldsTyped = saved["setup_complete"].is<bool>() && saved["pin_required"].is<bool>() && textWithoutNul(saved["pin"]);
    auth.complete = saved["setup_complete"] | false; auth.pinRequired = saved["pin_required"] | true;
    auth.pin = saved["pin"] | ""; auth.legacyPresent = preferences.isKey("pin"); auth.legacyPin = legacyPin.c_str();
    config.auth = Setup::authMode(auth);
    config.pin = auth.hasFields ? String(auth.pin) : legacyPin;
    if (!auth.hasFields && Setup::complete(config.auth) && !saveConfig(config)) config.auth = Setup::AuthMode::Invalid;
  } else config.auth = Setup::AuthMode::Invalid;
#ifndef SPOOLANCER_HEADLESS
  if (Setup::required(config.auth)) {
    Config generated = config; generated.auth = Setup::AuthMode::Protected; generated.pin = generatedPin();
    if (saveConfig(generated)) config = generated; else config.auth = Setup::AuthMode::Invalid;
  }
#endif
  if (config.auth == Setup::AuthMode::Invalid) {
    config.pin = ""; appMessage = "Saved authentication is invalid. Use the physical Wi-Fi/PIN reset to recover; inventory and calibration are kept.";
  }
  pin = config.pin;
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
  if (!Setup::complete(config.auth)) startAp();
#endif
  connectWifi();
  Serial.println("Startup: configuring web routes");
  setupRoutes();
  Serial.println("Startup: initializing status UI");
  StatusUi::begin(); renderStatusUi(true);
  Serial.println("Startup: status UI ready");
  if (MDNS.begin("spoolancer")) MDNS.addService("http", "tcp", 80);
  nfc.begin(nfcIdle);
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
  if (rebootAt && int32_t(millis() - rebootAt) >= 0) ESP.restart();
  if (otaRequested) {
    otaRequested = false;
    appMessage = "Installing firmware update; the device will reboot.";
    // apply() reboots on success and only returns on failure.
    const String failure = Ota::apply(otaPending);
    appMessage = failure.isEmpty() ? "Update complete." : failure;
    otaPending = Ota::Release{};
    return;
  }
  scale.tick(); server.handleClient(); buttons();
  pollWifiScan(); pollDiscovery();
#ifdef SPOOLANCER_TOUCH
  pollTouch();
#endif
  const uint32_t now = millis();
  if (reconnectAt && int32_t(now - reconnectAt) >= 0) {
    reconnectAt = 0; cancelWifiScan(); WiFi.disconnect();
#ifndef SPOOLANCER_HEADLESS
    startAp();
#endif
    connectWifi();
  }
  if (connectionAttempt && WiFi.status() == WL_CONNECTED) connectionAttempt = false;
  if (connectionAttempt && uint32_t(now - connectStarted) > 20000) {
    connectionAttempt = false;
#ifndef SPOOLANCER_HEADLESS
    if (!apActive) startAp();
#endif
    appMessage = "Wi-Fi did not connect. Check the network name and password in Settings on the setup network.";
  }
#ifdef SPOOLANCER_HEADLESS
  if (apActive) {
    setupWindow.observeConnection(!reconnectAt && WiFi.status() == WL_CONNECTED, now);
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
  if (int32_t(now - nextNfc) >= 0) {
    // A PN532 that powered up late is retried every 30 s instead of staying offline until reboot.
    if (nfc.available) { readNfc(); nextNfc = millis() + 2000; }
    else { nfc.begin(nfcIdle); nextNfc = millis() + 30000; }
  }
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
