#include "AppState.h"
#include "Pins.h"

namespace spoolancer {
Config config;
Session sessions[4];
std::vector<Spool> spools;
RemoteInventory::Cache remoteInventory;
RemoteLoad remoteLoad;
uint32_t remoteMutationEpoch = 0;
String inventoryError;
String selectionSource = "none";
String activeId, pin, apName, nfcSpoolId, appMessage, syncMessage, syncError;
JsonDocument nfcMetadata;
uint8_t cloneImage[1024]; uint8_t cloneUid[4]; bool cloneReady = false; String cloneLabel;
Preferences preferences;
fs::LittleFSFS inventory;
WebServer server(80);
Scale scale;
#if defined(SPOOLANCER_HEADLESS) && !defined(SPOOLANCER_TOUCH)
Nfc nfc(Wire); // PN532 and optional OLED share controller 0 on GPIO4/5.
#else
TwoWire nfcBus(1); // Integrated touch and legacy ST7789 keep their separate pin maps.
Nfc nfc(nfcBus);
#endif
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
WifiScan wifiScan;
#ifdef SPOOLANCER_HEADLESS
Setup::Window setupWindow;
#endif
uint32_t lastPrivateRequest = 0; bool privateSeen = false;
SpoolmanDiscovery discovery;
}  // namespace spoolancer
