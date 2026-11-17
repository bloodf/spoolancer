#include "Routes.h"
#include <WiFi.h>
#include "Device.h"
#include "DeviceUi.h"
#include "Http.h"
#include "Inventory.h"
#include <Update.h>
#include <cstring>
#include "Spoolman.h"
#include "WebAssets.h"

namespace spoolancer {
void registerConfigRoutes() {
  server.on("/api/config", HTTP_GET, [] {
    if (!authorized()) return;
    JsonDocument doc; doc["device_name"] = config.name; doc["wifi_ssid"] = config.ssid;
    doc["spoolman_url"] = config.spoolman; doc["has_wifi_password"] = !config.password.isEmpty();
    doc["wifi_open"] = config.password.isEmpty(); doc["pin_required"] = Setup::pinRequired(config.auth); reply(doc);
  });
  server.on("/api/config", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    Config changed = config;
    if (!doc["device_name"].isUnbound() && !textField(doc.as<JsonObjectConst>(), "device_name", changed.name, 48)) { error(400, "Device name must be 1–48 characters."); return; }
    const bool wifiChanged = !doc["wifi_ssid"].isUnbound() || !doc["wifi_password"].isUnbound() ||
                             !doc["wifi_open"].isUnbound() || !doc["clear_wifi_password"].isUnbound();
    if (wifiChanged) {
      if ((!doc["wifi_ssid"].isUnbound() && !textWithoutNul(doc["wifi_ssid"])) ||
          (!doc["wifi_password"].isUnbound() && !textWithoutNul(doc["wifi_password"])) ||
          (!doc["wifi_open"].isUnbound() && !doc["wifi_open"].is<bool>()) ||
          (!doc["clear_wifi_password"].isUnbound() && !doc["clear_wifi_password"].is<bool>())) {
        error(400, "Invalid Wi-Fi fields. SSID/password must be text; wifi_open must be true or false."); return;
      }
      if (!doc["wifi_ssid"].isUnbound()) changed.ssid = doc["wifi_ssid"].as<String>();
      const String password = doc["wifi_password"] | "";
      bool openPresent = !doc["wifi_open"].isUnbound(), open = doc["wifi_open"] | false;
      if (doc["clear_wifi_password"] == true) {
        if ((openPresent && !open) || (changed.ssid != config.ssid && !open)) {
          error(400, "Explicitly set wifi_open to true for an open network with a new SSID."); return;
        }
        openPresent = open = true;
      }
      const char *resolved = nullptr;
      if (changed.ssid.length() != std::strlen(changed.ssid.c_str()) || password.length() != std::strlen(password.c_str()) ||
          !Setup::resolveWifiPassword(config.ssid.c_str(), config.password.c_str(), changed.ssid.c_str(),
                                     password.c_str(), openPresent, open, resolved)) {
        error(400, "Use a 1–32 byte SSID. A new secured network needs an 8–63 byte password or 64 hex digits; choose wifi_open for an open network. Blank passwords only keep credentials for the same SSID."); return;
      }
      changed.password = resolved;
    }
    if (!doc["spoolman_url"].isUnbound()) {
      if (!doc["spoolman_url"].is<const char *>()) { error(400, "Invalid Spoolman URL."); return; }
      changed.spoolman = doc["spoolman_url"].as<String>(); changed.spoolman.trim();
      while (changed.spoolman.endsWith("/")) changed.spoolman.remove(changed.spoolman.length() - 1);
      if (!validServerUrl(changed.spoolman)) { error(400, "Use http://numeric-IPv4:port for the local Spoolman server; DNS, HTTPS and authenticated proxies are not supported yet."); return; }
      if (changed.spoolman != config.spoolman) {
        for (const auto &s : spools) if (s.remoteId || s.vendorId || s.filamentId || !s.pending.isEmpty()) {
          error(409, "Spoolman links already exist. Changing servers requires migrating or clearing those links first."); return;
        }
      }
    }
    const bool serverChanged = changed.spoolman != config.spoolman;
    const bool reconnect = changed.ssid != config.ssid || changed.password != config.password;
    if (!saveConfig(changed)) { error(507, "Could not save configuration."); return; }
    config = changed;
    if (serverChanged) { remoteLoad = {}; remoteInventory = {}; inventoryError = ""; syncError = ""; syncMessage = ""; }
    JsonDocument result; result["ok"] = true; result["reconnect_required"] = reconnect;
    result["ap_ssid"] = apName; result["ip"] = "192.168.4.1";
#ifdef SPOOLANCER_HEADLESS
    result["message"] = reconnect ? "Configuration saved; Wi-Fi will reconnect. The temporary setup network closes after connection or its 10-minute limit. Hold BOOT 8 seconds while running to recover." : "Configuration saved.";
#else
    result["message"] = reconnect ? "Configuration saved; Wi-Fi will reconnect. Setup AP remains available if connection fails." : "Configuration saved.";
#endif
    reply(result);
    if (reconnect) {
#ifdef SPOOLANCER_HEADLESS
      if (apActive) setupWindow.begin(millis());
#endif
      reconnectAt = millis() + 1000;
    }
  });
  server.on("/api/update", HTTP_GET, [] {
    if (!authorized()) return;
    if (WiFi.status() != WL_CONNECTED) { error(409, "Connect the scale to your local Wi-Fi before checking for updates."); return; }
    const Ota::Release rel = Ota::fetchLatest(deviceVariant);
    JsonDocument doc; doc["current"] = SPOOLANCER_VERSION; doc["variant"] = deviceVariant;
    if (!rel.ok) { doc["error"] = rel.error; reply(doc); return; }
    doc["latest"] = rel.tag;
    doc["update_available"] = Ota::updateAvailable(SPOOLANCER_VERSION, rel.tag.c_str());
    doc["notes_url"] = rel.notesUrl; doc["web_embedded"] = true; doc["has_filesystem"] = false;
    reply(doc);
  });
  server.on("/api/update", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument in; if (!body(in) || !confirmation(in)) return;
    if (WiFi.status() != WL_CONNECTED) { error(409, "Connect the scale to your local Wi-Fi before updating."); return; }
    if (otaRequested) { error(409, "An update is already in progress."); return; }
    const Ota::Release rel = Ota::fetchLatest(deviceVariant);
    if (!rel.ok) { error(502, rel.error); return; }
    if (!Ota::updateAvailable(SPOOLANCER_VERSION, rel.tag.c_str())) { error(409, "This device already runs the latest release."); return; }
    otaPending = rel; otaRequested = true;
    JsonDocument result; result["ok"] = true; result["latest"] = rel.tag;
    result["message"] = "Update started. The device is downloading " + rel.tag + " and reboots in about a minute. Do not power it off.";
    reply(result);
  });
  server.on("/api/update/upload", HTTP_POST,
    [] {  // Completion handler: runs after the whole multipart body is received.
      if (!uploadDidStart) { error(400, "No firmware file was received."); return; }
      uploadDidStart = false;
      if (!uploadError.isEmpty()) { const String e = uploadError; uploadError = ""; error(507, e); return; }
      JsonDocument r; r["ok"] = true;
      r["message"] = "Firmware and embedded webpage uploaded and verified. The device reboots in a moment to run them.";
      reply(r);
      rebootAt = millis() + 1500;  // Reboot after the response is flushed.
    },
    [] {  // Upload handler: streams the firmware image straight into the inactive OTA slot.
      HTTPUpload &up = server.upload();
      if (up.status == UPLOAD_FILE_START) {
        uploadDidStart = true; uploadError = "";
        if (!hasSession()) { uploadError = "Open a device session before uploading firmware."; return; }
        if (server.arg("confirm") != "true") { uploadError = "Explicit confirmation is required to flash uploaded firmware."; return; }
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) { uploadError = Update.errorString(); }
      } else if (up.status == UPLOAD_FILE_WRITE) {
        if (!uploadError.isEmpty()) return;  // Already rejected; drain the body.
        if (Update.write(up.buf, up.currentSize) != up.currentSize) uploadError = Update.errorString();
      } else if (up.status == UPLOAD_FILE_END) {
        if (!uploadError.isEmpty()) { Update.abort(); return; }
        if (!Update.end(true)) uploadError = Update.errorString();
      } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort(); uploadError = "The firmware upload was interrupted.";
      }
    });
  server.on("/api/spoolman/discover", HTTP_GET, [] {
    if (!authorized()) return;
    discoveryReply();
  });
  server.on("/api/spoolman/discover", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument input; if (!body(input)) return;
    if (discovery.state == SpoolmanDiscovery::State::Searching) { discoveryReply(); return; }
    if (WiFi.status() != WL_CONNECTED) { error(409, "Connect the scale to your local Wi-Fi before looking for Spoolman."); return; }
    if (discovery.attempted && uint32_t(millis() - discovery.started) < 60000) { error(429, "Wait one minute before another server search."); return; }
    startDiscovery(); discoveryReply();
  });
  server.on("/api/spoolman/test", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument input; if (!body(input)) return;
    if (!input["url"].isUnbound() && !textWithoutNul(input["url"])) { error(400, "Invalid server URL."); return; }
    const String target = input["url"].isUnbound() ? config.spoolman : input["url"].as<String>();
    if (target.isEmpty() || !validServerUrl(target)) { error(400, "Enter an HTTP server address before testing."); return; }
    const String previousError = syncError;
    syncError = "";
    startStatusOperation(StatusModel::Operation::Sync);
    JsonDocument response; int code;
    bool reached = remoteRequestAt(target, "GET", "/spool?limit=1", nullptr, response, code) &&
                   response.is<JsonArray>() && response.size() <= 1;
    if (reached && response.size()) { Spool probe; reached = parseRemoteSpool(response[0].as<JsonObjectConst>(), probe); }
    const String testedError = syncError;
    if (target != config.spoolman) syncError = previousError;
    finishStatusOperation(reached);
    if (!reached) { error(502, testedError.isEmpty() ? "The address did not return a compatible Spoolman inventory." : testedError); return; }
    JsonDocument result; result["ok"] = true; result["url"] = target;
    result["message"] = "Spoolman inventory is readable at " + target + ".";
    reply(result);
  });
  server.on("/api/spoolman/sync", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    Spool *s = requestedSpool(doc); if (!s || !validMeasurement(*s)) return;
    if (!s->pending.isEmpty()) { error(409, "Unresolved Spoolman creation. Check the server and enter its existing spool ID."); return; }
    if (!recordMeasurement(*s)) { error(507, "Cannot persist measurement; no remote update was sent."); return; }
    if (!syncSpool(*s)) { error(502, syncError.isEmpty() ? "Spoolman operation failed." : syncError); return; }
    JsonDocument result; result["ok"] = true; result["spoolman_id"] = s->remoteId; result["message"] = syncMessage; reply(result);
  });
  auto asset = [](const char *path, const char *mime) {
    const auto *file = WebAssets::find(path);
    if (!file) { error(404, "Web asset not found."); return; }
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Content-Security-Policy", "default-src 'self'; connect-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; frame-ancestors 'none'");
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("X-Spoolancer-Web-Build", WebAssets::buildId);
    server.send_P(200, mime, reinterpret_cast<const char *>(file->data), file->size);
  };
  server.on("/", HTTP_GET, [asset] { asset("/index.html", "text/html"); });
  server.on("/index.html", HTTP_GET, [asset] { asset("/index.html", "text/html"); });
  server.on("/app.js", HTTP_GET, [asset] { asset("/app.js", "text/javascript"); });
  server.on("/logic.js", HTTP_GET, [asset] { asset("/logic.js", "text/javascript"); });
  server.on("/styles.css", HTTP_GET, [asset] { asset("/styles.css", "text/css"); });
  server.on("/favicon.svg", HTTP_GET, [asset] { asset("/favicon.svg", "image/svg+xml"); });
}
}  // namespace spoolancer
