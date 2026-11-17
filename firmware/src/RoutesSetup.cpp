#include "Routes.h"
#include <WiFi.h>
#include "Device.h"
#include "DeviceUi.h"
#include "Http.h"
#include "Inventory.h"
#include <cstring>
#include "JsonNumbers.h"
#include "NfcFlow.h"
#include "WebAssets.h"

namespace spoolancer {
using JsonNumbers::numberOrNull;
void statusRoute() {
  JsonDocument doc; doc["mode"] = "device"; doc["device"]["name"] = config.name;
  doc["device"]["web_embedded"] = true; doc["device"]["web_build"] = WebAssets::buildId;
  doc["device"]["firmware"] = SPOOLANCER_VERSION; doc["device"]["storage_ready"] = storeReady;
  doc["device"]["variant"] = deviceVariant;
  doc["auth"]["pin_required"] = Setup::pinRequired(config.auth);
  doc["auth"]["invalid"] = config.auth == Setup::AuthMode::Invalid;
  JsonObject setup = doc["setup"].to<JsonObject>();
  setup["required"] = Setup::required(config.auth); setup["complete"] = Setup::complete(config.auth);
  setup["enrollment_required"] = Setup::required(config.auth);
#ifdef SPOOLANCER_HEADLESS
#ifdef SPOOLANCER_TOUCH
  doc["device"]["headless"] = false;
#elif defined(SPOOLANCER_OLED)
  doc["device"]["headless"] = false;
  doc["device"]["oled_available"] = StatusUi::oledAvailable();
#elif defined(SPOOLANCER_STATUS_LEDS)
  doc["device"]["headless"] = true;
#else
  doc["device"]["headless"] = true;
#endif
  setup["ap_active"] = apActive; setup["open_network"] = apActive;
  setup["via_ap"] = viaSetupAp();
  setup["physical_confirmation_required"] = false;
  setup["can_submit"] = Setup::required(config.auth) && viaSetupAp() && WiFi.status() != WL_CONNECTED && setupWindow.available(millis());
  setup["ap_seconds"] = (setupWindow.apRemaining(millis()) + 999) / 1000;
  setup["warning"] = "Temporary setup network has no password; use it in a trusted location. Wi-Fi credentials are sent over local HTTP.";
#else
  doc["device"]["headless"] = false;
  setup["ap_active"] = apActive; setup["open_network"] = false;
  setup["via_ap"] = apActive && server.client().localIP() == WiFi.softAPIP();
  setup["physical_confirmation_required"] = false; setup["can_submit"] = false;
#endif
  JsonObject weight = doc["scale"].to<JsonObject>();
  numberOrNull(weight["gross_g"], scale.gross()); weight["calibrated"] = scale.calibrated();
  weight["tare_ready"] = scale.tareReady(); weight["calibration_ready"] = scale.calibrationReady();
  weight["sample_count"] = scale.sampleCount();
  weight["stable"] = scale.stable(); weight["fresh"] = scale.fresh(); weight["zero_set"] = scale.zeroSet;
  Spool *s = findSpool(activeId);
  const double net = s && std::isfinite(s->empty) ? scale.gross() - s->empty : NAN;
  numberOrNull(weight["net_g"], net); numberOrNull(weight["remaining_g"], net);
  if (activeId.isEmpty()) doc["active_spool_id"] = nullptr; else doc["active_spool_id"] = activeId;
  // Public status reports hardware, not inventory metadata or configuration secrets.
  JsonObject tag = doc["nfc"].to<JsonObject>();
  tag["available"] = nfc.available; tag["present"] = nfc.present; tag["uid"] = nfc.uid;
  tag["type"] = nfc.type; tag["writable"] = nfc.writable;
  tag["raw_writable"] = nfc.rawWritable; tag["chip"] = nfcChip();
  tag["last_message"] = nfc.lastMessage; tag["last_error"] = nfc.lastError;
  tag["capacity_bytes"] = nfc.ndefCapacity();
  if (hasSession()) nfcJson(tag); // Selection links and tag metadata stay private.
#ifdef SPOOLANCER_STATUS_LEDS
  doc["diagnostics"]["leds_supported"] = true;
#else
  doc["diagnostics"]["leds_supported"] = false;
#endif
  doc["diagnostics"]["led_test_remaining_ms"] = StatusUi::ledTestRemaining(millis());
  doc["network"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["network"]["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["network"]["ap_active"] = apActive;
  doc["spoolman"]["inventory_refreshing"] = remoteLoad.running;
  doc["spoolman"]["configured"] = !config.spoolman.isEmpty();
  doc["spoolman"]["inventory_source"] = config.spoolman.isEmpty() ? "local" : "spoolman";
  doc["spoolman"]["inventory_stale"] = !config.spoolman.isEmpty() && inventoryStale();
  doc["spoolman"]["inventory_error"] = inventoryError;
  doc["spoolman"]["last_message"] = syncMessage; doc["spoolman"]["last_error"] = syncError;
  doc["spoolman"]["pending"] = s && !s->pending.isEmpty();
  doc["selection_source"] = selectionSource;
  doc["message"] = appMessage; reply(doc);
}
void registerSetupRoutes() {
  server.on("/api/status", HTTP_GET, statusRoute);
  server.on("/api/setup", HTTP_POST, [] {
#ifdef SPOOLANCER_HEADLESS
    JsonDocument doc; if (!body(doc)) return;
    if (!setupWindow.consume(millis(), Setup::required(config.auth), viaSetupAp(), WiFi.status() != WL_CONNECTED,
                             Setup::trustedHeaders(server.hostHeader().c_str(), server.header("Origin").c_str()))) {
      error(403, "First-run setup only: join the temporary Wi-Fi and open http://192.168.4.1 directly. Wait two seconds between attempts."); return;
    }
    if (!textWithoutNul(doc["wifi_ssid"]) || !doc["wifi_open"].is<bool>() ||
        !doc["pin_required"].is<bool>() ||
        (!doc["wifi_password"].isUnbound() && !textWithoutNul(doc["wifi_password"]))) {
      error(400, "Choose Wi-Fi, explicitly select open or secured, and choose whether a PIN is required."); return;
    }
    Config changed = config;
    changed.ssid = doc["wifi_ssid"].as<String>(); changed.password = doc["wifi_password"] | "";
    if (changed.ssid.length() != std::strlen(changed.ssid.c_str()) ||
        changed.password.length() != std::strlen(changed.password.c_str()) ||
        !Setup::validWifi(changed.ssid.c_str(), changed.password.c_str(), doc["wifi_open"].as<bool>())) {
      error(400, "SSID must be 1–32 UTF-8 bytes. Secured Wi-Fi needs an 8–63 byte password or 64 hexadecimal digits; open Wi-Fi needs an empty password."); return;
    }
    const bool requirePin = doc["pin_required"].as<bool>();
    if (requirePin && (!textWithoutNul(doc["pin"]) ||
        doc["pin"].as<String>().length() != 8 || !Setup::validPin(doc["pin"].as<const char *>()))) {
      error(400, "Choose exactly 8 digits for the device PIN."); return;
    }
    if (!requirePin && !doc["pin"].isUnbound() &&
        (!textWithoutNul(doc["pin"]) || !doc["pin"].as<String>().isEmpty())) {
      error(400, "Omit the PIN when PIN protection is disabled."); return;
    }
    changed.pin = requirePin ? doc["pin"].as<String>() : "";
    changed.auth = requirePin ? Setup::AuthMode::Protected : Setup::AuthMode::Open;
    if (!saveConfig(changed)) { error(507, "Could not save setup. No settings changed."); return; }
    config = changed; pin = config.pin;
    for (auto &session : sessions) session.token = "";
    nextLogin = 0; setupWindow.begin(millis());
    appMessage = "Setup saved. Connecting to your Wi-Fi. Reconnect this browser to the same LAN.";
    JsonDocument result; result["ok"] = true; result["reconnect_required"] = true;
    result["pin_required"] = Setup::pinRequired(config.auth); result["ap_ssid"] = apName;
    result["message"] = appMessage; sessionReply(result);
    reconnectAt = millis() + 1000;
#else
    error(403, "This display variant uses its generated device PIN. Open a session and configure Wi-Fi in Settings.");
#endif
  });
  server.on("/api/session", HTTP_POST, [] {
    if (!trustedSessionRequest()) { error(403, "Open the device's current local IP address or http://spoolancer.local directly."); return; }
    if (!Setup::complete(config.auth)) { error(403, "Complete first-run setup, or hold BOOT 8 seconds to recover invalid setup."); return; }
    if (int32_t(nextLogin - millis()) > 0) { error(429, "Wait before opening another device session."); return; }
    nextLogin = millis() + 1500;
    JsonDocument doc; if (!body(doc)) return;
    if (doc["setup"] == true) { error(403, "Use /api/setup for first-run Wi-Fi and optional PIN setup."); return; }
    if (Setup::pinRequired(config.auth) && (!textWithoutNul(doc["pin"]) || doc["pin"].as<String>() != pin)) {
      if (++loginFailures >= 5) { nextLogin = millis() + 60000; loginFailures = 0; }
      error(401, "Incorrect device PIN."); return;
    }
    JsonDocument result; result["ok"] = true; sessionReply(result);
  });
  server.on("/api/wifi/scan", HTTP_GET, [] {
    if (!scanAllowed()) return;
    pollWifiScan(); scanReply();
  });
  server.on("/api/wifi/scan", HTTP_POST, [] {
    if (!scanAllowed()) return;
    JsonDocument doc; if (!body(doc)) return;
    pollWifiScan();
    if (wifiScan.state == WifiScan::State::Scanning) { scanReply(); return; }
    if (wifiScan.radioPending || WiFi.scanComplete() == WIFI_SCAN_RUNNING || reconnectAt || connectionAttempt) {
      error(409, "Wait for the current Wi-Fi operation to finish before scanning."); return;
    }
    if (wifiScan.started && uint32_t(millis() - wifiScan.startedAt) < 10000) {
      error(429, "Wait 10 seconds between Wi-Fi scans."); return;
    }
    if (Setup::required(config.auth)) {
      WiFi.setAutoReconnect(false);
      WiFi.mode(WIFI_AP_STA);
      WiFi.disconnect(false, true);
    } else WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
    wifiScan.started = true; wifiScan.startedAt = millis(); wifiScan.networks.clear();
    const int16_t count = WiFi.scanNetworks(true, false, false, 300);
    wifiScan.radioPending = count == WIFI_SCAN_RUNNING || count >= 0;
    wifiScan.state = wifiScan.radioPending ? WifiScan::State::Scanning : WifiScan::State::Failed;
    pollWifiScan(); scanReply();
  });
}
void setupRoutes() {
  const char *headers[] = {"Authorization", "Content-Type", "Content-Length", "Origin"};
  server.collectHeaders(headers, 4);
  registerSetupRoutes();
  registerSpoolRoutes();
  registerHardwareRoutes();
  registerConfigRoutes();
  server.onNotFound([] { error(404, "Not found."); });
  server.begin();
}
}  // namespace spoolancer
