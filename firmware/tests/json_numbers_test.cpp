#include <ArduinoJson.h>
#include "JsonNumbers.h"
#include "InventoryTag.h"
#include "RemoteProfile.h"
#include <vector>
#include <cassert>
#include <iostream>
#include <string>

void inventoryTagTests() {
  const char *id = "0123456789abcdef";
  for (const size_t capacity : {size_t(144), size_t(496), size_t(872)}) {
    JsonDocument profile;
    profile["name"] = "Green PLA for the workshop";
    profile["brand"] = "Example brand";
    profile["material"] = "PLA";
    profile["color"] = "#00aa00";
    profile["empty_weight_g"] = 250;
    profile["initial_weight_g"] = 1000;
    profile["density_g_cm3"] = 1.24;
    profile["diameter_mm"] = 1.75;
    profile["id"] = id; profile["tag_uid"] = "04010203040506";
    InventoryTag::prepare(profile, id, capacity);
    assert(InventoryTag::fits(profile, capacity));
    assert(InventoryTag::bytes(profile) <= capacity);
    assert(InventoryTag::linkOnly(profile.as<JsonObjectConst>()) == (capacity == 144));
    assert(profile["spool_id"] == id && profile["schema"] == 1);
    if (capacity > 144) assert(profile["material"] == "PLA" && profile["color"] == "#00aa00");
    std::string json; serializeJson(profile, json);
    uint8_t encoded[880] = {};
    const size_t size = Domain::encodeNdef(reinterpret_cast<const uint8_t *>(json.data()), json.size(), encoded, capacity);
    assert(size == InventoryTag::bytes(profile));
    size_t offset = 0, length = 0;
    assert(Domain::decodeNdef(encoded, size, offset, length));
    JsonDocument read;
    assert(!deserializeJson(read, encoded + offset, length));
    assert(read["spool_id"] == id && InventoryTag::linkOnly(read.as<JsonObjectConst>()) == (capacity == 144));
  }
  // Full details survive on an NTAG216; an NTAG215 sheds least-important details first.
  JsonDocument rich;
  rich["name"] = "Green PLA for the workshop"; rich["brand"] = "Example brand"; rich["material"] = "PLA";
  rich["color"] = "#00aa00"; rich["empty_weight_g"] = 250; rich["initial_weight_g"] = 1000;
  rich["density_g_cm3"] = 1.24; rich["diameter_mm"] = 1.75;
  JsonObject details = rich["details"].to<JsonObject>();
  details["extruder_temp_c"] = 215; details["bed_temp_c"] = 60; details["article_number"] = "PLA-G-1";
  details["comment"] = std::string(150, 'c'); details["filament_comment"] = std::string(150, 'f');
  details["location"] = "Shelf A"; details["lot_nr"] = "L-77";
  JsonDocument big = rich; InventoryTag::prepare(big, id, 872);
  assert(InventoryTag::fits(big, 872) && big["details"]["comment"].is<const char *>() && big["details"]["extruder_temp_c"] == 215);
  JsonDocument mid = rich; InventoryTag::prepare(mid, id, 496);
  assert(InventoryTag::fits(mid, 496) && !InventoryTag::linkOnly(mid.as<JsonObjectConst>()));
  assert(mid["details"]["comment"].isNull() && mid["details"]["extruder_temp_c"] == 215 && mid["details"]["lot_nr"] == "L-77");
  JsonDocument tiny = rich; InventoryTag::prepare(tiny, id, 144);
  assert(InventoryTag::linkOnly(tiny.as<JsonObjectConst>()));
  JsonDocument large; large["name"] = std::string(900, 'x');
  InventoryTag::prepare(large, id, 872);
  assert(InventoryTag::linkOnly(large.as<JsonObjectConst>()) && InventoryTag::fits(large, 144));
  assert(!InventoryTag::fits(large, 40));
  large["color"] = "#ffffff";
  assert(!InventoryTag::linkOnly(large.as<JsonObjectConst>()));
  std::cout << "PASS: capacity-aware inventory records, NTAG213 compact link and NTAG215/216 full metadata round-trip\n";
}
void remoteProfileTests() {
  struct Spool {
    std::string name, material, brand, color, tagUid, details;
    std::vector<std::string> serverTags;
    int remoteId=0, filamentId=0, vendorId=0;
    bool remote=false, remoteListed=false;
    double initial=0, empty=0, lastRemaining=0, remoteRemaining=0, density=0, diameter=0;
  };
  JsonDocument doc;
  assert(!deserializeJson(doc, R"({"id":42,"archived":false,"initial_weight":1000,"spool_weight":250,"remaining_weight":618,
    "lot_nr":"L-77","location":"Shelf A","price":21.5,"comment":"bad\u0001text",
    "filament":{"id":7,"name":"Green PLA","material":"PLA","color_hex":"A0B0C0FF","density":1.24,"diameter":1.75,
    "settings_extruder_temp":215,"settings_bed_temp":60,"article_number":"PLA-G-1","price":19,"comment":"dry first",
    "vendor":{"id":3,"name":"Test","empty_spool_weight":200},"tags":[{"uid":"11223344"}]},
    "tags":[{"uid":"04:a1:b2:c3:d4:e5:f6"},{"uid":"04A1B2C3D4E5F6"},{"uid":"bad"}],
    "extra":{"nfc_id":"\"AABBCCDD\""}})"));
  Spool first;
  assert(RemoteProfile::decode(doc.as<JsonObjectConst>(), first));
  assert(first.remoteId==42 && first.empty==250 && first.initial==1000 && first.remoteRemaining==618);
  assert(first.color=="#A0B0C0" && first.serverTags.size()==2);
  assert(first.serverTags[0]=="04A1B2C3D4E5F6" && first.serverTags[1]=="AABBCCDD");
  {
    JsonDocument details; assert(!deserializeJson(details, first.details));
    assert(details["extruder_temp_c"]==215 && details["bed_temp_c"]==60 && details["article_number"]=="PLA-G-1");
    assert(details["lot_nr"]=="L-77" && details["location"]=="Shelf A" && details["price"]==21.5);
    assert(details["filament_comment"]=="dry first" && details["comment"].isNull()); // Control characters dropped.
  }
  std::vector<Spool> spools{first};
  auto visible=[](const Spool &s){return s.remoteListed;};
  assert(SpoolActions::matchTag(spools,"04A1B2C3D4E5F6",visible)==0);
  assert(SpoolActions::matchTag(spools,"11223344",visible)==-1); // Filament tag cannot identify one roll.
  spools.push_back(first);
  assert(SpoolActions::matchTag(spools,"AABBCCDD",visible)==-2); // Ambiguous link is not guessed.
  spools[1].remoteListed=false;
  assert(SpoolActions::matchTag(spools,"aabbccdd",visible)==0);
  doc.remove("spool_weight"); doc.remove("initial_weight");
  doc["filament"]["weight"]=750; doc["filament"]["spool_weight"]=185;
  doc["filament"].remove("vendor"); doc["filament"].remove("name"); doc.remove("tags"); doc.remove("extra");
  Spool old;
  assert(RemoteProfile::decode(doc.as<JsonObjectConst>(), old));
  assert(old.empty==185 && old.initial==750 && old.brand.empty() && old.name.empty() && old.serverTags.empty());
  doc["remaining_weight"]="618";
  assert(!RemoteProfile::decode(doc.as<JsonObjectConst>(), old));
  std::cout << "PASS: real Spoolman schema, optional fields, weight precedence, native/custom RFID aliases and unique auto-selection\n";
}
int main() {
  inventoryTagTests(); remoteProfileTests();
  using JsonNumbers::numberOrNull;
  JsonDocument document;
  auto scale = document["scale"].to<JsonObject>();
  numberOrNull(scale["gross_g"], 842);
  numberOrNull(scale["zero_g"], 0);
  numberOrNull(scale["negative_g"], -2.5);
  numberOrNull(scale["unknown"], NAN);
  numberOrNull(scale["infinite"], INFINITY);
  assert(scale["gross_g"].is<double>() && scale["gross_g"].as<double>() == 842);
  assert(scale["zero_g"].is<double>() && scale["zero_g"].as<double>() == 0);
  assert(scale["negative_g"].as<double>() == -2.5);
  assert(scale["unknown"].is<JsonVariant>() && scale["unknown"].isNull());
  assert(scale["infinite"].is<JsonVariant>() && scale["infinite"].isNull());
  std::string json;
  serializeJson(document, json);
  assert(json == "{\"scale\":{\"gross_g\":842,\"zero_g\":0,\"negative_g\":-2.5,\"unknown\":null,\"infinite\":null}}");
  JsonDocument reopened;
  assert(!deserializeJson(reopened, json));
  assert(reopened["scale"]["gross_g"].as<double>() == 842);
  assert(reopened["scale"]["zero_g"].is<double>());
  auto spool = reopened["spool"].to<JsonObject>();
  numberOrNull(spool["empty_weight_g"], 250);
  numberOrNull(spool["initial_weight_g"], 1000);
  numberOrNull(spool["remaining_weight_g"], 592);
  json.clear(); serializeJson(reopened, json);
  JsonDocument persisted;
  assert(!deserializeJson(persisted, json));
  assert(persisted["spool"]["empty_weight_g"].as<double>() == 250);
  assert(persisted["spool"]["initial_weight_g"].as<double>() == 1000);
  assert(persisted["spool"]["remaining_weight_g"].as<double>() == 592);
  std::cout << "PASS: real ArduinoJson absent-field creation, finite/unknown weight values and inventory round-trip\n";
}
