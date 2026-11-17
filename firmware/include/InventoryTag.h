#pragma once
#include <ArduinoJson.h>
#include <cstring>
#include "Domain.h"
#include "Setup.h"

namespace InventoryTag {
// Extra Spoolman filament/spool facts carried in a nested "details" object.
// Ordered least important first: when a tag is too small, fields are dropped in this order.
struct DetailField { const char *key; size_t maxText; double low, high; };
constexpr DetailField detailFields[] = {
  {"comment", 160, 0, 0}, {"filament_comment", 160, 0, 0}, {"location", 64, 0, 0},
  {"price", 0, 0, 1000000}, {"lot_nr", 64, 0, 0}, {"external_id", 64, 0, 0},
  {"multi_color_direction", 16, 0, 0}, {"multi_color_hexes", 64, 0, 0},
  {"article_number", 64, 0, 0}, {"bed_temp_c", 0, 0, 200}, {"extruder_temp_c", 0, 0, 500},
};
// Copies only known, well-formed fields; anything else (untrusted tag or server data) is dropped.
inline void cleanDetails(JsonObjectConst in, JsonObject out) {
  for (const auto &field : detailFields) {
    JsonVariantConst value = in[field.key];
    if (field.maxText) {
      if (!value.is<const char *>()) continue;
      const JsonString text = value.as<JsonString>();
      if (!text.size() || text.size() > field.maxText || text.size() != std::strlen(text.c_str()) ||
          !Setup::validText(text.c_str())) continue;
      out[field.key] = text.c_str();
    } else if (value.is<double>() && Domain::inRange(value.as<double>(), field.low, field.high)) {
      out[field.key] = value.as<double>();
    }
  }
}
inline size_t bytes(const JsonDocument &tag) {
  return Domain::ndefSize(std::strlen(Domain::ndefMime), measureJson(tag));
}
inline bool fits(const JsonDocument &tag, size_t capacity) {
  return measureJson(tag) <= Domain::maxPayload && bytes(tag) <= capacity;
}
inline bool linkOnly(JsonObjectConst tag) {
  return tag.size() == 3 && tag["schema"] == 1 && tag["link_only"] == true &&
         tag["spool_id"].is<const char *>() && Domain::validId(tag["spool_id"]);
}
// Keep the complete inventory profile on the device/server. Small tags drop optional
// details (least important first), then carry only identity; never truncate a value.
inline void prepare(JsonDocument &tag, const char *id, size_t capacity) {
  const char *keys[] = {"id", "spoolman_id", "remaining_weight_g", "last_gross_g",
       "measurement_sequence", "tag_uid", "sync_pending", "source", "metadata_readonly",
       "last_measurement_remaining_g", "removal_pending", "inactive_tag_link", "server_tag_uids"};
  for (const char *key : keys) tag.remove(key);
  tag["schema"] = 1; tag["spool_id"] = id;
  JsonObject details = tag["details"];
  if (!details.isNull() && details.size() == 0) tag.remove("details");
  for (const auto &field : detailFields) {
    if (fits(tag, capacity)) return;
    if (details.isNull()) break;
    details.remove(field.key);
    if (details.size() == 0) { tag.remove("details"); details = JsonObject(); }
  }
  if (fits(tag, capacity)) return;
  tag.clear();
  tag["schema"] = 1; tag["spool_id"] = id; tag["link_only"] = true;
}
}
