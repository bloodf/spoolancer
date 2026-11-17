#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "AppState.h"
#include "Device.h"
#include "DeviceUi.h"
#include "Inventory.h"
#include "NfcFlow.h"
#include "Pins.h"
#include "Routes.h"
#include "Spoolman.h"
#ifdef SPOOLANCER_TOUCH
#include "TouchUi.h"
#endif

namespace spoolancer {
#if defined(SPOOLANCER_TOUCH)
const char *const deviceVariant = "touch-8048";
#elif defined(SPOOLANCER_OLED)
const char *const deviceVariant = "oled-led";
#elif defined(SPOOLANCER_STATUS_LEDS)
const char *const deviceVariant = "headless-led";
#elif defined(SPOOLANCER_HEADLESS)
const char *const deviceVariant = "headless";
#else
const char *const deviceVariant = "st7789";
#endif
}  // namespace spoolancer

using namespace spoolancer;

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
  assetsReady = true; // Webpage is compiled into the app; no asset filesystem mount is needed.
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
  if (!rebootAt && !otaRequested && !uploadDidStart) pollRemoteInventory();
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
