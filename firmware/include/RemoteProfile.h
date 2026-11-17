#pragma once
#include <ArduinoJson.h>
#include <cctype>
#include <cstring>
#include "Domain.h"
#include "Setup.h"
#include "RemoteInventory.h"
#include "SpoolActions.h"
#include "InventoryTag.h"

namespace RemoteProfile {
template<class Text> bool text(JsonVariantConst field, Text &value, size_t limit) {
  if (field.isNull()) { value = ""; return true; }
  if (!field.is<const char *>()) return false;
  const auto json = field.as<JsonString>();
  const char *raw = json.c_str();
  if (json.size() != std::strlen(raw) || json.size() > limit || !Setup::validText(raw)) return false;
  value = raw; return true;
}
inline bool number(JsonVariantConst field, double &value) {
  if (field.isNull()) { value = NAN; return true; }
  if (!field.is<double>() || !Domain::inRange(field.as<double>(), 0, 1000000)) return false;
  value = field.as<double>(); return true;
}
template<class Spool> bool decode(JsonObjectConst item, Spool &value) {
  const auto filament = item["filament"].as<JsonObjectConst>();
  if (!item["id"].is<int>() || item["id"].as<int>() < 1 || filament.isNull() ||
      !filament["id"].is<int>() || filament["id"].as<int>() < 1 || !item["archived"].is<bool>()) return false;
  value.remoteId = item["id"].as<int>(); value.filamentId = filament["id"].as<int>();
  value.remote = true; value.remoteListed = !item["archived"].as<bool>();
  const auto vendor = filament["vendor"].as<JsonObjectConst>();
  value.vendorId = vendor["id"] | 0;
  if (!text(filament["name"], value.name, 64) || !text(filament["material"], value.material, 64) ||
      !text(vendor["name"], value.brand, 64)) return false;
  if (!filament["color_hex"].isNull() && !filament["color_hex"].is<const char *>()) return false;
  const char *color = filament["color_hex"] | "";
  if (!filament["color_hex"].isNull() && filament["color_hex"].as<JsonString>().size() != std::strlen(color)) return false;
  if (*color == '#') ++color;
  const size_t length = std::strlen(color);
  if (length && length != 6 && length != 8) return false;
  for (size_t i = 0; i < length; ++i) if (!std::isxdigit(static_cast<unsigned char>(color[i]))) return false;
  char rgb[8] = {};
  if (length) { rgb[0] = '#'; std::memcpy(rgb + 1, color, 6); }
  value.color = rgb;
  double initial, filamentWeight, empty, filamentEmpty, vendorEmpty, remaining;
  if (!number(item["initial_weight"], initial) || !number(filament["weight"], filamentWeight) ||
      !number(item["spool_weight"], empty) || !number(filament["spool_weight"], filamentEmpty) ||
      !number(vendor["empty_spool_weight"], vendorEmpty) || !number(item["remaining_weight"], remaining) ||
      !number(filament["density"], value.density) || !number(filament["diameter"], value.diameter)) return false;
  const auto weights = RemoteInventory::weights(initial, filamentWeight, empty, filamentEmpty, vendorEmpty, remaining);
  value.initial = weights.initial; value.empty = weights.empty;
  value.lastRemaining = value.remoteRemaining = weights.remaining;
  // Everything else Spoolman knows about the filament and roll, for full inventory tags.
  JsonDocument raw, clean;
  raw["article_number"] = filament["article_number"]; raw["external_id"] = filament["external_id"];
  raw["extruder_temp_c"] = filament["settings_extruder_temp"]; raw["bed_temp_c"] = filament["settings_bed_temp"];
  raw["multi_color_hexes"] = filament["multi_color_hexes"]; raw["multi_color_direction"] = filament["multi_color_direction"];
  raw["filament_comment"] = filament["comment"]; raw["comment"] = item["comment"];
  raw["lot_nr"] = item["lot_nr"]; raw["location"] = item["location"];
  raw["price"] = item["price"].is<double>() ? item["price"] : filament["price"];
  InventoryTag::cleanDetails(raw.as<JsonObjectConst>(), clean.to<JsonObject>());
  value.details = "";
  if (clean.as<JsonObjectConst>().size()) serializeJson(clean, value.details);
  value.serverTags.clear();
  auto addTag = [&](const char *uid) {
    char canonical[33];
    if (!SpoolActions::normalizeUid(uid, canonical)) return;
    for (const auto &known : value.serverTags) if (std::strcmp(known.c_str(), canonical) == 0) return;
    if (value.serverTags.size() < 16) value.serverTags.emplace_back(canonical);
  };
  // Only spool tags identify this roll. Filament tags identify a product, not a roll.
  for (JsonObjectConst tag : item["tags"].as<JsonArrayConst>()) addTag(tag["uid"] | "");
  // Older Spoolman integrations use JSON-encoded text in a custom extra field.
  for (const char *key : {"nfc_id", "rfid", "tag_uid"}) {
    if (!item["extra"][key].is<const char *>()) continue;
    JsonDocument decoded;
    if (!deserializeJson(decoded, item["extra"][key].template as<const char *>()) && decoded.is<const char *>())
      addTag(decoded.as<const char *>());
  }
  return true;
}
}
