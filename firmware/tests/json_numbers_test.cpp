#include <ArduinoJson.h>
#include "JsonNumbers.h"
#include <cassert>
#include <iostream>
#include <string>

int main() {
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
