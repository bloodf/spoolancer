#pragma once
// Shared firmware state: data types, limits and the globals every module reads and writes.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <Wire.h>
#ifndef SPOOLANCER_HEADLESS
#include <SPI.h>
#include <Adafruit_ST7789.h>
#endif
#include <mdns.h>
#include <vector>
#include "Discovery.h"
#include "Nfc.h"
#include "Ota.h"
#include "RemoteInventory.h"
#include "Scale.h"
#include "Setup.h"

namespace spoolancer {
constexpr size_t maxLocalSpools = 100, maxSpools = maxLocalSpools + RemoteInventory::limit, maxRequest = 4096;
constexpr uint32_t sessionMs = 2UL * 60 * 60 * 1000;
extern const char *const deviceVariant;  // Defined in main.cpp for the active build variant.

struct Spool {
  String id, name, brand, material, color, tagUid;
  String details; // Serialized InventoryTag details object (extra Spoolman facts), or empty.
  std::vector<String> serverTags;
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
};
struct Session { String token; uint32_t started = 0; };
struct RemoteLoad {
  bool running = false;
  size_t offset = 0;
  uint32_t started = 0, epoch = 0;
  String target;
  RemoteInventory::Pages pages;
  std::vector<Spool> records;
};
struct WifiNetwork { String ssid; int32_t rssi, channel; bool secure, supported; };
struct WifiScan {
  enum class State { Ready, Scanning, Failed } state = State::Ready;
  bool started = false, radioPending = false;
  uint32_t startedAt = 0;
  std::vector<WifiNetwork> networks;
};
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
};

extern Config config;
extern Session sessions[4];
extern std::vector<Spool> spools;
extern RemoteInventory::Cache remoteInventory;
extern RemoteLoad remoteLoad;
extern uint32_t remoteMutationEpoch;
extern String inventoryError;
extern String selectionSource;
extern String activeId, pin, apName, nfcSpoolId, appMessage, syncMessage, syncError;
extern JsonDocument nfcMetadata;
// Holds a genuine Bambu tag image between the clone's read step and its write-to-magic-card step.
extern uint8_t cloneImage[1024]; extern uint8_t cloneUid[4]; extern bool cloneReady; extern String cloneLabel;
extern Preferences preferences;
extern fs::LittleFSFS inventory;
extern WebServer server;
extern Scale scale;
extern Nfc nfc;
#ifndef SPOOLANCER_HEADLESS
extern Adafruit_ST7789 display;
extern SPIClass displaySpi;
#endif
extern bool assetsReady, storeReady, activeHadLoad, apActive;
extern bool connectionAttempt;
extern uint32_t nextNfc, nextDisplay, connectStarted, reconnectAt;
extern uint32_t nextLogin, pinVisibleUntil;
extern Ota::Release otaPending; extern bool otaRequested;
extern bool uploadDidStart; extern String uploadError; extern uint32_t rebootAt;
extern unsigned loginFailures, sessionSlot;
extern WifiScan wifiScan;
#ifdef SPOOLANCER_HEADLESS
extern Setup::Window setupWindow;
#endif
extern uint32_t lastPrivateRequest; extern bool privateSeen;
extern SpoolmanDiscovery discovery;
}  // namespace spoolancer
