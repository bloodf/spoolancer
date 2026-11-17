#include "Spoolman.h"
#include <WiFi.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "DeviceUi.h"
#include "Inventory.h"
#include "RemoteHttp.h"
#include "RemoteProfile.h"

namespace spoolancer {
bool validServerUrl(const String &url) {
  if (url.isEmpty()) return true;
  if (!url.startsWith("http://") || url.length() > 200 || url.length() < 8) return false;
  for (char c : url) if (uint8_t(c) <= 32 || c == '@' || c == '?' || c == '#' || c == '\\') return false;
  String authority = url.substring(7);
  const int slash = authority.indexOf('/');
  if (slash >= 0) authority = authority.substring(0, slash);
  const int colon = authority.indexOf(':');
  String host = colon < 0 ? authority : authority.substring(0, colon);
  IPAddress address;
  if (!address.fromString(host) || address[0] == 0 || address[0] >= 224) return false;
  if (colon >= 0) {
    const String port = authority.substring(colon + 1);
    if (port.isEmpty() || port.length() > 5) return false;
    for (char c : port) if (c < '0' || c > '9') return false;
    if (port.toInt() < 1 || port.toInt() > 65535) return false;
  }
  return true;
}
bool readHttpLine(WiFiClient &client, String &line, uint32_t deadline, size_t &headerBytes) {
  line = "";
  while (int32_t(deadline - millis()) > 0) {
    scale.tick(); renderStatusUi();
    if (!client.available()) {
      if (!client.connected()) return false;
      delay(1); continue;
    }
    const char c = client.read();
    if (++headerBytes > 4096 || line.length() > 512) return false;
    if (c == '\n') return true;
    if (c != '\r') line += c;
  }
  return false;
}
// Allow ARP and routed Wi-Fi connections time to complete without pausing the scale.
bool connectSpoolman(WiFiClient &client, const IPAddress &address, uint16_t port) {
  const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) return false;
  if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { close(fd); return false; }
  sockaddr_in target{}; target.sin_family = AF_INET; target.sin_port = htons(port);
  target.sin_addr.s_addr = uint32_t(address);
  const int started = ::connect(fd, reinterpret_cast<sockaddr *>(&target), sizeof(target));
  if (started < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK) { close(fd); return false; }
  const uint32_t end = millis() + 1500;
  while (int32_t(end - millis()) > 0) {
    scale.tick(); renderStatusUi();
    fd_set ready; FD_ZERO(&ready); FD_SET(fd, &ready);
    timeval wait{}; const int result = select(fd + 1, nullptr, &ready, nullptr, &wait);
    if (result < 0) { if (errno == EINTR) continue; close(fd); return false; }
    if (result > 0) {
      int error = 0; socklen_t size = sizeof(error);
      if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) || error) { close(fd); return false; }
      client = WiFiClient(fd); return true;
    }
    delay(1);
  }
  close(fd); return false;
}
bool remoteRequestAt(const String &url, const char *method, const String &path, JsonDocument *input, JsonDocument &output, int &code, int allowedError) {
  code = 0; syncError = "";
  if (url == config.spoolman && std::strcmp(method, "GET") != 0) ++remoteMutationEpoch;
  if (WiFi.status() != WL_CONNECTED || url.isEmpty() || !validServerUrl(url)) { syncError = "Connect Wi-Fi and configure a local Spoolman URL first."; return false; }
  String authority = url.substring(7), basePath;
  const int slash = authority.indexOf('/');
  if (slash >= 0) { basePath = authority.substring(slash); authority = authority.substring(0, slash); }
  const int colon = authority.indexOf(':');
  const String host = colon < 0 ? authority : authority.substring(0, colon);
  const uint16_t port = colon < 0 ? 80 : authority.substring(colon + 1).toInt();
  IPAddress address; address.fromString(host);
  WiFiClient client;
  scale.tick(); renderStatusUi();
  // code -1 marks that no request bytes left the device, so a create can be retried safely.
  if (!connectSpoolman(client, address, port)) { code = -1; syncError = "Cannot connect to " + url + ". Check the server address, TCP port and access from the scale Wi-Fi."; return false; }
  scale.tick(); renderStatusUi();
  String data; if (input) serializeJson(*input, data);
  const String headers = String(method) + " " + basePath + "/api/v1" + path + " HTTP/1.0\r\nHost: " + authority +
      "\r\nConnection: close\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: " + String(data.length()) + "\r\n\r\n";
  const String request = headers + data;
  const uint32_t deadline = millis() + 5000;
  for (size_t offset = 0; offset < request.length();) {
    scale.tick(); renderStatusUi();
    const size_t length = std::min(size_t(128), request.length() - offset);
    const int written = ::send(client.fd(), request.c_str() + offset, length, MSG_DONTWAIT);
    if (int32_t(deadline - millis()) <= 0 || written == 0 ||
        (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
      client.stop(); syncError = "Spoolman request could not be sent."; return false;
    }
    if (written > 0) offset += static_cast<size_t>(written);
    else delay(1);
  }
  String line; size_t headerBytes = 0;
  if (!readHttpLine(client, line, deadline, headerBytes) || !RemoteHttp::status(line.c_str(), code)) {
    client.stop(); syncError = "Spoolman returned an invalid or incomplete HTTP response."; return false;
  }
  if ((code < 200 || code >= 300) && code != allowedError) { client.stop(); syncError = "Spoolman returned HTTP " + String(code) + "."; return false; }
  int remaining = -1; bool lengthSeen = false, chunked = false;
  for (;;) {
    if (!readHttpLine(client, line, deadline, headerBytes)) { client.stop(); syncError = "Spoolman response headers timed out or exceeded the limit."; return false; }
    if (line.isEmpty()) break;
    if (!RemoteHttp::header(line.c_str(), remaining, lengthSeen, chunked)) {
      client.stop(); syncError = "Spoolman response has invalid headers, exceeds 20 KB, or uses unsupported transfer encoding. Use its direct HTTP address."; return false;
    }
  }
  String response; response.reserve(1024);
  RemoteHttp::Body body(remaining, chunked);
  bool validBody = true;
  while (int32_t(deadline - millis()) > 0 && !body.done(client.connected() || client.available())) {
    scale.tick(); renderStatusUi();
    if (client.available()) {
      if (!body.feed(char(client.read()), response)) { validBody = false; break; }
    } else {
      if (!client.connected()) break;
      delay(1);
    }
  }
  const bool incomplete = !validBody || !body.done(client.connected() || client.available());
  client.stop();
  if (response.length() > 20000 || incomplete || deserializeJson(output, response, DeserializationOption::NestingLimit(8))) {
    syncError = "Spoolman returned an incomplete or invalid JSON response."; return false;
  }
  return true;
}
bool remoteRequest(const char *method, const String &path, JsonDocument *input, JsonDocument &output, int &code, int allowedError) {
  return remoteRequestAt(config.spoolman, method, path, input, output, code, allowedError);
}
bool parseRemoteSpool(JsonObjectConst item, Spool &value) {
  return RemoteProfile::decode(item, value);
}
void remoteIdentity(Spool &record) {
  for (const auto &old : spools) if (old.remoteId == record.remoteId) {
    record.id = old.id; record.tagUid = old.tagUid;
    record.lastGross = old.lastGross; record.lastRemaining = old.lastRemaining; record.measurement = old.measurement;
    record.pending = old.pending; return;
  }
  for (uint32_t prefix = 0; ; ++prefix) {
    char generated[17]; RemoteInventory::stableId(record.remoteId, prefix, generated);
    bool collision = false;
    for (const auto &old : spools) if (old.id == generated) { collision = true; break; }
    if (!collision) { record.id = generated; return; }
  }
}
bool refreshRemoteInventory(bool force) {
  if (config.spoolman.isEmpty()) return true;
  if (remoteLoad.running) return !inventoryStale();
  if (WiFi.status() != WL_CONNECTED) {
    inventoryError = "Spoolman is offline: Wi-Fi is disconnected. Showing only its last cached inventory, if available.";
    return false;
  }
  if (!remoteInventory.shouldRefresh(millis(), WiFi.status() == WL_CONNECTED, force)) return !inventoryStale();
  remoteLoad = {};
  remoteLoad.running = true; remoteLoad.started = millis();
  remoteLoad.epoch = remoteMutationEpoch; remoteLoad.target = config.spoolman;
  remoteInventory.begin(millis());
  return !inventoryStale();
}
void remoteLoadFailed(const String &message) {
  remoteLoad.running = false; remoteLoad.records.clear();
  remoteInventory.finish(millis(), false); inventoryError = message;
}
void pollRemoteInventory() {
  if (!remoteLoad.running) {
    // Background refresh only while someone uses the private UI (or before the first snapshot):
    // each attempt can block the loop for a connect timeout when Spoolman is down.
    const bool active = privateSeen && uint32_t(millis() - lastPrivateRequest) < 120000;
    if (active || !remoteInventory.successful) refreshRemoteInventory();
    return;
  }
  if (remoteLoad.target != config.spoolman || remoteLoad.epoch != remoteMutationEpoch) {
    remoteLoadFailed("Inventory changed during loading. Refreshing the server snapshot.");
    remoteInventory.attempted = false; return;
  }
  if (WiFi.status() != WL_CONNECTED) { remoteLoadFailed("Wi-Fi disconnected while loading Spoolman."); return; }
  if (uint32_t(millis() - remoteLoad.started) > 20000) {
    remoteLoadFailed("Spoolman inventory loading timed out. Refresh to retry."); return;
  }
  JsonDocument response; int code;
  // Background loading reports through inventoryError and never clears a measurement-sync error.
  const String previousSyncError = syncError;
  const String path = "/spool?allow_archived=false&sort=id:asc&limit=" + String(RemoteInventory::pageSize) +
                      "&offset=" + String(remoteLoad.offset);
  const bool fetched = remoteRequest("GET", path, nullptr, response, code);
  const String requestError = syncError; syncError = previousSyncError;
  if (!fetched || !response.is<JsonArray>() || response.size() > RemoteInventory::pageSize) {
    remoteLoadFailed(requestError.isEmpty() ? "Unexpected Spoolman inventory response." : requestError); return;
  }
  for (JsonVariantConst item : response.as<JsonArrayConst>()) {
    Spool record;
    if (!item.is<JsonObjectConst>() || !parseRemoteSpool(item.as<JsonObjectConst>(), record) ||
        !remoteLoad.pages.accept(record.remoteId)) {
      remoteLoadFailed("Spoolman inventory is invalid, changed during paging, or exceeds 200 spools."); return;
    }
    if (record.remoteListed) remoteLoad.records.push_back(record);
  }
  remoteLoad.offset += RemoteInventory::pageSize;
  if (!remoteLoad.pages.complete(response.size())) return;
  // Merge local measurements and RFID links at commit, never from an old partial page.
  for (auto &record : remoteLoad.records) remoteIdentity(record);
  // Replace a complete server snapshot atomically. Unresolved local journals stay hidden, never deleted.
  std::vector<Spool> updated;
  for (const auto &old : spools) {
    bool hasReplacement = false;
    for (const auto &current : remoteLoad.records) if (current.id == old.id) { hasReplacement = true; break; }
    if (hasReplacement) continue;
    if (old.remote && old.pending.isEmpty() && old.tagUid.isEmpty() && old.measurement == 0) continue;
    Spool retained = old; retained.remoteListed = false;
    updated.push_back(retained);
  }
  if (updated.size() + remoteLoad.records.size() > maxSpools) {
    remoteInventory.finish(millis(), false); inventoryError = "The device cache is full. Existing records were preserved; inventory was not replaced."; remoteLoad.running = false; remoteLoad.records.clear(); return;
  }
  updated.insert(updated.end(), remoteLoad.records.begin(), remoteLoad.records.end());
  spools = std::move(updated); remoteInventory.finish(millis(), true); inventoryError = "";
  if (syncError.isEmpty()) syncMessage = "Loaded " + String(remoteLoad.records.size()) + " spools from Spoolman.";
  if (!activeId.isEmpty() && !findSpool(activeId)) chooseSpool("");
  remoteLoad.running = false; remoteLoad.records.clear();
}
void inventoryJson(JsonObject obj) {
  const bool configured = !config.spoolman.isEmpty();
  obj["refreshing"] = configured && remoteLoad.running;
  obj["source"] = configured ? "spoolman" : "local"; obj["configured"] = configured;
  obj["stale"] = configured && inventoryStale(); obj["cached"] = configured && remoteInventory.loaded;
  obj["error"] = configured ? inventoryError : ""; obj["limit"] = configured ? RemoteInventory::limit : maxLocalSpools;
  if (configured && remoteInventory.successful) obj["age_seconds"] = uint32_t(millis() - remoteInventory.refreshedAt) / 1000;
  else obj["age_seconds"] = nullptr;
}
bool remoteWritePreflight(Spool &s) {
  if (!s.remoteId) return true;
  JsonDocument response; int code;
  if (!remoteRequest("GET", "/spool/" + String(s.remoteId), nullptr, response, code)) {
    inventoryError = syncError; remoteInventory.failed = true; return false;
  }
  Spool current;
  if (!parseRemoteSpool(response.as<JsonObjectConst>(), current) || current.remoteId != s.remoteId || !current.remoteListed) {
    syncError = "Spoolman spool is missing, archived, or invalid. Refresh the inventory."; remoteInventory.failed = true; inventoryError = syncError; return false;
  }
  if (!RemoteInventory::sameNumber(current.empty, s.empty) || !RemoteInventory::sameNumber(current.initial, s.initial)) {
    syncError = "Spoolman weights changed. Refresh the inventory and measure again before sending."; remoteInventory.failed = true; inventoryError = syncError; return false;
  }
  return true;
}
SpoolActions::ArchiveState readArchiveState(const Spool &s) {
  JsonDocument response; int code;
  if (!remoteRequest("GET", "/spool/" + String(s.remoteId), nullptr, response, code, 404)) return SpoolActions::ArchiveState::Unknown;
  if (code == 404) {
    return textWithoutNul(response["message"]) && SpoolActions::missingReply(code, s.remoteId, response["message"].as<const char *>())
      ? SpoolActions::ArchiveState::Missing : SpoolActions::ArchiveState::Unknown;
  }
  return SpoolActions::archiveState(s.remoteId, response["id"].is<int>() ? response["id"].as<int>() : 0,
                                    response["archived"].is<bool>(), response["archived"] == true);
}
bool confirmMissingArchive(const Spool &s) {
  if (!SpoolActions::archiving(s.pending.c_str())) return false;
  JsonDocument info; int code;
  const bool verified = remoteRequest("GET", "/info", nullptr, info, code) && textWithoutNul(info["version"]) &&
    !info["version"].as<String>().isEmpty() && textWithoutNul(info["db_type"]) && !info["db_type"].as<String>().isEmpty() && info["debug_mode"].is<bool>();
  if (!verified) return false;
  // A complete, ordered prefix including archived records must independently prove absence.
  RemoteInventory::Pages pages; const uint32_t started = millis();
  for (size_t offset = 0; offset <= RemoteInventory::limit; offset += RemoteInventory::pageSize) {
    if (uint32_t(millis() - started) >= 6000) return false;
    JsonDocument response;
    const String path = "/spool?allow_archived=true&sort=id:asc&limit=" + String(RemoteInventory::pageSize) + "&offset=" + String(offset);
    if (!remoteRequest("GET", path, nullptr, response, code) || !response.is<JsonArray>() || response.size() > RemoteInventory::pageSize) return false;
    for (JsonVariantConst item : response.as<JsonArrayConst>()) {
      Spool record;
      if (!item.is<JsonObjectConst>() || !parseRemoteSpool(item.as<JsonObjectConst>(), record) || !pages.accept(record.remoteId) || record.remoteId == s.remoteId) return false;
    }
    if (pages.previousId > s.remoteId || pages.complete(response.size()))
      return SpoolActions::confirmedMissing(s.pending.c_str(), verified, true, true);
  }
  return false;
}
bool archiveSpool(Spool &s, bool &removedMissing) {
  using SpoolActions::ArchiveState;
  syncError = ""; removedMissing = false;
  const auto current = readArchiveState(s);
  if (current == ArchiveState::Missing) {
    removedMissing = confirmMissingArchive(s);
    if (!removedMissing) syncError = "The server returned a missing spool, but its identity and complete list absence could not be confirmed. The recovery record was kept.";
    return removedMissing;
  }
  if (current == ArchiveState::Unknown) {
    syncError = "Could not confirm this spool's current archive status. No local record was removed. " + syncError;
    return false;
  }
  if (!SpoolActions::archiving(s.pending.c_str())) {
    const String previous = s.pending; s.pending = "archive";
    if (!saveInventory()) {
      s.pending = previous;
      syncError = "Cannot save the archive journal; nothing was changed in Spoolman.";
      return false;
    }
  }
  if (current == ArchiveState::Archived) return true;
  JsonDocument request, response; request["archived"] = true; int code;
  remoteRequest("PATCH", "/spool/" + String(s.remoteId), &request, response, code);
  // PATCH is idempotent, but its response alone is never treated as proof of an archive.
  const auto checked = readArchiveState(s);
  if (checked == ArchiveState::Archived) return true;
  if (checked == ArchiveState::Missing && confirmMissingArchive(s)) { removedMissing = true; return true; }
  syncError = "Archive was not confirmed. The record and its recovery journal were kept. Check archive status to retry safely.";
  return false;
}
bool persistPending(Spool &s, const String &stage) {
  s.pending = stage;
  if (saveInventory()) return true;
  syncError = "Cannot persist the remote-operation journal; nothing further was sent."; return false;
}
bool createRemote(Spool &s, const char *kind, JsonDocument &request, int &id) {
  if (WiFi.status() != WL_CONNECTED || config.spoolman.isEmpty()) {
    syncError = "Connect Wi-Fi and configure a local Spoolman URL first."; return false;
  }
  if (!persistPending(s, kind)) return false;
  JsonDocument response; int code;
  const bool good = remoteRequest("POST", String("/") + kind, &request, response, code);
  if (good && response["id"].is<int>() && response["id"].as<int>() > 0) {
    id = response["id"].as<int>();
    s.pending = "";
    // Mark the server record before saving so a later mismatch or reboot cannot hide it.
    if (String(kind) == "spool") s.remote = s.remoteListed = true;
    if (saveInventory()) {
      if (String(kind) == "spool" && (!response["remaining_weight"].is<double>() ||
          std::abs(response["remaining_weight"].as<double>() - s.lastRemaining) > .1)) {
        syncError = "Spoolman spool was created and linked, but its returned remaining weight differs. Check it and sync again."; return false;
      }
      return true;
    }
    s.pending = kind;
    syncError = "Remote creation succeeded but local saving failed. Check Spoolman before any retry."; return false;
  }
  if ((code >= 400 && code < 500) || code == -1) {
    s.pending = "";
    if (!saveInventory()) { s.pending = kind; syncError += " Could not save the rejection; check the server before retrying."; }
  } else {
    syncError = "Creation outcome is unknown. Check Spoolman and enter the existing Spoolman spool ID; automatic retries are blocked.";
  }
  return false;
}
bool syncSpoolImpl(Spool &s) {
  syncError = ""; syncMessage = "";
  if (!s.pending.isEmpty()) { syncError = "A prior creation is unresolved. Check Spoolman and link its existing spool ID before retrying."; return false; }
  if (!s.remoteId) {
    if (!s.vendorId) {
      // Exact equality is checked locally; never select a partial-name match.
      JsonDocument vendors; int code;
      String encoded;
      for (uint8_t c : s.brand) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.') encoded += char(c);
        else { char h[4]; snprintf(h, sizeof(h), "%%%02X", c); encoded += h; }
      }
      if (!remoteRequest("GET", "/vendor?limit=100&name=" + encoded, nullptr, vendors, code) || !vendors.is<JsonArray>()) return false;
      for (JsonObjectConst item : vendors.as<JsonArrayConst>()) {
        if (item["name"].is<const char *>() && s.brand.equalsIgnoreCase(item["name"].as<String>()) && item["id"].is<int>()) {
          s.vendorId = item["id"].as<int>(); break;
        }
      }
      if (!s.vendorId) {
        JsonDocument request; request["name"] = s.brand;
        if (!createRemote(s, "vendor", request, s.vendorId)) return false;
      } else if (!saveInventory()) { syncError = "Cannot save the vendor link."; return false; }
    }
    if (!s.filamentId) {
      JsonDocument request; request["name"] = s.name; request["vendor_id"] = s.vendorId;
      request["material"] = s.material; request["color_hex"] = s.color.substring(1);
      request["density"] = s.density; request["diameter"] = s.diameter;
      request["weight"] = s.initial; request["spool_weight"] = s.empty;
      request["comment"] = "Spoolancer record " + s.id;
      if (!createRemote(s, "filament", request, s.filamentId)) return false;
    }
    JsonDocument request; request["filament_id"] = s.filamentId;
    request["initial_weight"] = s.initial; request["spool_weight"] = s.empty;
    request["remaining_weight"] = s.lastRemaining; request["comment"] = "Spoolancer record " + s.id;
    if (!createRemote(s, "spool", request, s.remoteId)) return false;
    s.remote = s.remoteListed = true; s.remoteRemaining = s.lastRemaining;
    if (!saveInventory()) { syncError = "Spoolman spool " + String(s.remoteId) + " was created, but its local link could not be saved."; return false; }
    syncMessage = "Created Spoolman spool " + String(s.remoteId) + " with the saved remaining weight.";
    return true;
  }
  if (!remoteWritePreflight(s)) return false;
  JsonDocument request, response; int code;
  request["remaining_weight"] = s.lastRemaining;
  if (!remoteRequest("PATCH", "/spool/" + String(s.remoteId), &request, response, code)) {
    syncError += " The absolute update may have reached the server; retrying the same saved weight does not subtract consumption twice.";
    remoteInventory.failed = true; inventoryError = syncError; return false;
  }
  if (response["id"] != s.remoteId || !response["remaining_weight"].is<double>() ||
      std::abs(response["remaining_weight"].as<double>() - s.lastRemaining) > .1) {
    syncError = "Spoolman did not confirm the requested remaining weight.";
    remoteInventory.failed = true; inventoryError = syncError; return false;
  }
  s.remoteRemaining = s.lastRemaining;
  syncMessage = "Spoolman remaining weight updated."; return true;
}
bool syncSpool(Spool &s) {
  startStatusOperation(StatusModel::Operation::Sync);
  const bool synced = syncSpoolImpl(s);
  finishStatusOperation(synced); return synced;
}
}  // namespace spoolancer
