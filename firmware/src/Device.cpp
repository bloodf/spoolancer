#include "Device.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <algorithm>
#include "Http.h"

namespace spoolancer {
#ifdef SPOOLANCER_HEADLESS
bool viaSetupAp() {
  if (!apActive || server.client().localIP() != WiFi.softAPIP()) return false;
  if (WiFi.status() == WL_CONNECTED && server.client().localIP() == WiFi.localIP()) return false;
  const IPAddress peer = server.client().remoteIP(), local = WiFi.softAPIP();
  return peer[0] == local[0] && peer[1] == local[1] && peer[2] == local[2] && peer[3] > 1 && peer[3] < 255;
}
#endif
bool saveConfig(const Config &value) {
  if (value.auth == Setup::AuthMode::Invalid) return false;
  JsonDocument doc; doc["name"] = value.name; doc["ssid"] = value.ssid;
  doc["password"] = value.password; doc["spoolman"] = value.spoolman;
  doc["setup_complete"] = Setup::complete(value.auth);
  doc["pin_required"] = Setup::pinRequired(value.auth); doc["pin"] = value.pin;
  String json; serializeJson(doc, json);
  return preferences.putString("config", json) == json.length();
}
bool saveCalibration() {
  JsonDocument doc; doc["zero"] = scale.zero; doc["factor"] = scale.countsPerGram;
  doc["zero_set"] = scale.zeroSet;
  String json; serializeJson(doc, json);
  return preferences.putString("calibration", json) == json.length();
}
bool zeroCradle() {
  const double previousZero = scale.zero; const bool previousSet = scale.zeroSet;
  if (!scale.tare()) return false;
  if (saveCalibration()) return true;
  scale.zero = previousZero; scale.zeroSet = previousSet;
  return false;
}
String randomHex(size_t bytes) {
  String result; result.reserve(bytes * 2);
  for (size_t i = 0; i < bytes; ++i) { char b[3]; snprintf(b, sizeof(b), "%02x", uint8_t(esp_random())); result += b; }
  return result;
}
String generatedPin() { char out[9]; snprintf(out, sizeof(out), "%08lu", (unsigned long)(esp_random() % 100000000UL)); return String(out); }
void cancelWifiScan() {
  if (!wifiScan.radioPending) return;
  const int16_t count = WiFi.scanComplete();
  const esp_err_t stopped = count >= 0 ? ESP_OK : esp_wifi_scan_stop();
  wifiScan.state = WifiScan::State::Failed;
  if (stopped != ESP_OK && stopped != ESP_ERR_WIFI_NOT_STARTED && stopped != ESP_ERR_WIFI_NOT_INIT) return;
  wifiScan.radioPending = false;
  if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) WiFi.scanDelete();
}
void pollWifiScan() {
  if (!wifiScan.radioPending) return;
  const int16_t count = WiFi.scanComplete();
  if (wifiScan.state == WifiScan::State::Failed && count < 0) return;
  if (count == WIFI_SCAN_RUNNING && uint32_t(millis() - wifiScan.startedAt) < 8000) return;
  if (count < 0) { cancelWifiScan(); return; }
  wifiScan.radioPending = false;
  if (wifiScan.state != WifiScan::State::Scanning) { WiFi.scanDelete(); return; }
  wifiScan.networks.clear();
  for (int i = 0; i < count && i < 255; ++i) {
    const String ssid = WiFi.SSID(uint8_t(i));
    if (!Setup::validSsid(ssid.c_str())) continue;
    const wifi_auth_mode_t auth = WiFi.encryptionType(uint8_t(i));
    WifiNetwork network{ssid, WiFi.RSSI(uint8_t(i)), WiFi.channel(uint8_t(i)), auth != WIFI_AUTH_OPEN,
      auth == WIFI_AUTH_OPEN || auth == WIFI_AUTH_WPA_PSK || auth == WIFI_AUTH_WPA2_PSK ||
      auth == WIFI_AUTH_WPA_WPA2_PSK || auth == WIFI_AUTH_WPA2_WPA3_PSK};
    auto existing = std::find_if(wifiScan.networks.begin(), wifiScan.networks.end(),
      [&ssid](const WifiNetwork &value) { return value.ssid == ssid; });
    if (existing != wifiScan.networks.end()) {
      if (network.rssi > existing->rssi) *existing = network;
    } else if (wifiScan.networks.size() < 24) wifiScan.networks.push_back(network);
    else {
      auto weakest = std::min_element(wifiScan.networks.begin(), wifiScan.networks.end(),
        [](const WifiNetwork &a, const WifiNetwork &b) { return a.rssi < b.rssi; });
      if (network.rssi > weakest->rssi) *weakest = network;
    }
  }
  std::sort(wifiScan.networks.begin(), wifiScan.networks.end(),
    [](const WifiNetwork &a, const WifiNetwork &b) { return a.rssi > b.rssi; });
  WiFi.scanDelete(); wifiScan.state = WifiScan::State::Ready;
}
bool scanAllowed() {
  if (hasSession()) return true;
#ifdef SPOOLANCER_HEADLESS
  if (Setup::required(config.auth) && viaSetupAp() && !setupWindow.expired(millis()) &&
      Setup::trustedHeaders(server.hostHeader().c_str(), server.header("Origin").c_str())) return true;
#endif
  error(403, "Open a device session or join the temporary first-run setup Wi-Fi to scan."); return false;
}
void scanReply() {
  JsonDocument doc;
  doc["state"] = wifiScan.state == WifiScan::State::Scanning ? "scanning" :
                 wifiScan.state == WifiScan::State::Failed ? "failed" : "ready";
  JsonArray networks = doc["networks"].to<JsonArray>();
  if (wifiScan.state == WifiScan::State::Ready) for (const auto &network : wifiScan.networks) {
    JsonObject item = networks.add<JsonObject>();
    item["ssid"] = network.ssid; item["rssi"] = network.rssi; item["channel"] = network.channel;
    item["secure"] = network.secure; item["supported"] = network.supported;
  }
  reply(doc);
}
void startAp() {
  if (apActive) return;
#ifdef SPOOLANCER_HEADLESS
  cancelWifiScan(); WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  apActive = WiFi.softAP(apName.c_str());
  if (apActive) setupWindow.begin(millis());
  Serial.printf("Temporary OPEN setup Wi-Fi: %s\nOpen http://192.168.4.1 in a trusted location.\n", apName.c_str());
#else
  WiFi.mode(WIFI_AP_STA);
  apActive = WiFi.softAP(apName.c_str(), pin.c_str());
  pinVisibleUntil = millis() + 10UL * 60 * 1000;
  Serial.printf("Setup Wi-Fi: %s\nAP password / device PIN: %s\nOpen http://192.168.4.1\n", apName.c_str(), pin.c_str());
#endif
}
void connectWifi() {
  if (!Setup::complete(config.auth)) return;
  if (config.ssid.isEmpty()) { startAp(); return; }
  cancelWifiScan(); WiFi.setAutoReconnect(true);
  WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(config.ssid.c_str(), config.password.c_str());
  connectStarted = millis(); connectionAttempt = true;
}
}  // namespace spoolancer
