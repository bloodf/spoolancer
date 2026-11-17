#pragma once
// Device configuration, calibration persistence and Wi-Fi (station, setup AP and scanning).
#include "AppState.h"

namespace spoolancer {
#ifdef SPOOLANCER_HEADLESS
bool viaSetupAp();
#endif
bool saveConfig(const Config &value);
bool saveCalibration();
bool zeroCradle();
String randomHex(size_t bytes);
String generatedPin();
void cancelWifiScan();
void pollWifiScan();
bool scanAllowed();
void scanReply();
void startAp();
void connectWifi();
}  // namespace spoolancer
