// Pure OTA helpers (asset naming + update detection). The networking in Ota.h is behind
// #ifdef ARDUINO and not exercised here.
#include "Ota.h"
#include <cassert>
#include <cstring>

static void assetNames() {
  char out[64];
  Ota::assetName("headless-led", "firmware", out, sizeof(out));
  assert(std::strcmp(out, "spoolancer-headless-led-firmware.bin") == 0);
  Ota::assetName("st7789", "littlefs", out, sizeof(out));
  assert(std::strcmp(out, "spoolancer-st7789-littlefs.bin") == 0);
}

static void availability() {
  // Any differing, non-empty latest tag is an update; equal or empty is not.
  assert(Ota::updateAvailable("v1.0.0", "v1.1.0"));
  assert(Ota::updateAvailable("0.0.0-dev", "v1.0.0"));
  assert(!Ota::updateAvailable("v1.1.0", "v1.1.0"));
  assert(!Ota::updateAvailable("v1.1.0", ""));
  assert(!Ota::updateAvailable("v1.1.0", nullptr));
  assert(Ota::updateAvailable(nullptr, "v1.0.0"));
}

void otaTests() { assetNames(); availability(); }
