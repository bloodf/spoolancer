#include "Inventory.h"
#include <WiFi.h>
#include <cstring>
#include "Domain.h"
#include "DeviceUi.h"
#include "InventoryTag.h"
#include "JsonNumbers.h"
#include "SpoolActions.h"

namespace spoolancer {
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
using JsonNumbers::numberOrNull;
void spoolJson(JsonObject obj, const Spool &s, bool persisted) {
  obj["id"] = s.id; obj["name"] = s.name; obj["brand"] = s.brand;
  obj["material"] = s.material; obj["color"] = s.color;
  if (!s.details.isEmpty()) {
    JsonDocument details;
    if (!deserializeJson(details, s.details) && details.is<JsonObject>()) obj["details"].set(details);
  }
  numberOrNull(obj["empty_weight_g"], s.empty); numberOrNull(obj["initial_weight_g"], s.initial);
  numberOrNull(obj["density_g_cm3"], s.density); numberOrNull(obj["diameter_mm"], s.diameter);
  if (s.remoteId) obj["spoolman_id"] = s.remoteId; else obj["spoolman_id"] = nullptr;
  numberOrNull(obj["remaining_weight_g"], s.remote && !persisted ? s.remoteRemaining : s.lastRemaining);
  numberOrNull(obj["last_measurement_remaining_g"], s.measurement ? s.lastRemaining : NAN);
  numberOrNull(obj["last_gross_g"], s.lastGross);
  obj["measurement_sequence"] = s.measurement; obj["tag_uid"] = s.tagUid;
  JsonArray serverTags = obj["server_tag_uids"].to<JsonArray>();
  for (const auto &uid : s.serverTags) serverTags.add(uid);
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
bool textField(JsonObjectConst obj, const char *key, String &out, size_t max, bool required) {
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
void readDetails(JsonObjectConst obj, Spool &s) {
  JsonDocument clean; s.details = "";
  InventoryTag::cleanDetails(obj["details"].as<JsonObjectConst>(), clean.to<JsonObject>());
  if (clean.as<JsonObjectConst>().size()) serializeJson(clean, s.details);
}
bool validateSpool(JsonObjectConst obj, Spool &s) {
  // The spool form does not send details; keep the existing ones unless a record carries its own.
  if (!obj["details"].isNull()) readDetails(obj, s);
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
    for (JsonVariantConst item : obj["server_tag_uids"].as<JsonArrayConst>()) {
      char uid[33];
      if (item.is<const char *>() && SpoolActions::normalizeUid(item.as<const char *>(), uid) && s.serverTags.size() < 16)
        s.serverTags.emplace_back(uid);
    }
    s.vendorId = obj["vendor_id"] | 0; s.filamentId = obj["filament_id"] | 0;
    if (s.remote) readDetails(obj, s);
    s.pending = obj["pending"] | "";
    s.lastGross = obj["last_gross_g"].is<double>() ? obj["last_gross_g"].as<double>() : NAN;
    s.lastRemaining = obj["remaining_weight_g"].is<double>() ? obj["remaining_weight_g"].as<double>() : NAN;
    s.remoteRemaining = obj["remote_remaining_weight_g"].is<double>() ? obj["remote_remaining_weight_g"].as<double>() : NAN;
    s.measurement = obj["measurement_sequence"] | 0;
    loaded.push_back(s);
  }
  spools = std::move(loaded); return true;
}
String measurementProblem(const Spool &s) {
  if (SpoolActions::archiving(s.pending.c_str())) return "Finish checking this spool's archive status before measuring it.";
  if (s.remote && inventoryStale()) return "Refresh the Spoolman inventory before saving this measurement.";
  if (activeId != s.id) return "Select or scan this spool on the scale before saving.";
  if (s.initial > Domain::maxFilament) return "This spool exceeds the 3000 g initial-filament measuring limit. Its inventory profile can be kept, but measurements cannot be saved.";
  if (!Domain::measurementAllowed(scale.gross(), s.empty, s.initial, scale.calibrated(), scale.stable(), scale.fresh())) {
    return "Need a fresh stable calibrated weight (20–5000 g gross), known empty-spool weight, and remaining filament within the initial weight.";
  }
  return "";
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
void chooseSpool(const String &id, const char *source) {
  selectionSource = id.isEmpty() ? "none" : source;
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
}  // namespace spoolancer
