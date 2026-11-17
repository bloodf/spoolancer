#include "Routes.h"
#include <WiFi.h>
#include "Device.h"
#include "DeviceUi.h"
#include "Http.h"
#include "Inventory.h"
#include <algorithm>
#include <cmath>
#include "Domain.h"
#include "Spoolman.h"
#include "SpoolActions.h"

namespace spoolancer {
void registerSpoolRoutes() {
  server.on("/api/spools", HTTP_GET, [] {
    if (!authorized()) return;
    refreshRemoteInventory(server.arg("refresh") == "1");
    JsonDocument doc; JsonArray list = doc["spools"].to<JsonArray>();
    for (const auto &s : spools) if (visibleSpool(s)) spoolJson(list.add<JsonObject>(), s);
    inventoryJson(doc["inventory"].to<JsonObject>());
    JsonArray drafts = doc["drafts"].to<JsonArray>();
    if (!config.spoolman.isEmpty()) for (const auto &s : spools) if ((!s.remote && !s.remoteId) || !s.pending.isEmpty() || inactiveTagLink(s)) spoolJson(drafts.add<JsonObject>(), s);
    reply(doc);
  });
  server.on("/api/spools", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    if (!storeReady) { error(503, "Inventory storage is unavailable."); return; }
    Spool *existing = nullptr;
    if (!doc["id"].isNull()) {
      if (!doc["id"].is<const char *>() || !(existing = findStoredSpool(doc["id"].as<String>()))) { error(404, "Spool not found."); return; }
    }
    if (existing && SpoolActions::archiving(existing->pending.c_str())) { error(409, "Check this spool's pending archive status before editing it."); return; }
    String mergeCacheId;
    const bool bindOnly = existing && existing->remote && existing->pending.isEmpty();
    Spool value = existing ? *existing : Spool();
    if (bindOnly) {
      if (!doc["tag_uid"].is<const char *>() || doc.size() != 2) { error(409, "Edit this spool's profile in Spoolman. Only its tag binding can be changed here."); return; }
      if (inventoryStale()) { error(409, "Refresh the Spoolman inventory before linking this tag."); return; }
    } else if (!validateSpool(doc.as<JsonObjectConst>(), value)) { error(400, "Invalid spool fields. Check names, color, weights, density and diameter."); return; }
    const size_t localCount = std::count_if(spools.begin(), spools.end(), [](const Spool &s) { return !s.remote; });
    if (!existing && (localCount >= maxLocalSpools || spools.size() >= maxSpools)) { error(409, "Device local draft limit is 100 spools, with up to 200 remote records."); return; }
    const bool create = !config.spoolman.isEmpty() && !bindOnly && !value.remoteId;
    if (create && (!doc["confirm"].is<bool>() || !doc["confirm"].as<bool>())) { error(400, "Confirm creation in Spoolman before saving this new spool."); return; }
    if (create && existing && !existing->pending.isEmpty()) { error(409, "A prior creation has an unknown outcome. Check Spoolman and link the existing spool ID before retrying."); return; }
    const bool inferEmpty = doc["infer_empty_from_measurement"] == true;
    if (inferEmpty || create) {
      if (!doc["spool_condition"].is<const char *>() || (doc["spool_condition"] != "full" && doc["spool_condition"] != "used")) {
        error(400, "Specify whether this is a new full spool or a used spool."); return;
      }
      const bool full = doc["spool_condition"] == "full";
      if ((inferEmpty || (create && !full)) && value.initial > Domain::maxFilament) {
        error(409, "This spool exceeds the 3000 g initial-filament measuring limit. Keep its inventory profile without estimating or saving a measurement."); return;
      }
      if (inferEmpty && (!full || !scale.calibrated() || !scale.stable() || !scale.fresh() ||
          !Domain::inRange(scale.gross(), 20, Domain::maxGross) || !Domain::inRange(scale.gross() - value.initial, 0, 2000) ||
          !std::isfinite(value.empty) || std::abs(value.empty - (scale.gross() - value.initial)) > 1)) {
        error(409, "The full-spool measurement changed. Weigh it again before estimating the empty spool weight."); return;
      }
      if (inferEmpty) value.empty = scale.gross() - value.initial;
      if (create) {
        if (full) value.lastRemaining = value.initial;
        else {
          // The cradle reading must belong to this spool, not to another selected spool.
          if (!activeId.isEmpty() && (!existing || activeId != existing->id)) {
            error(409, "Another spool is selected on the scale. Place this spool alone in the cradle and clear the selection before creating it as used."); return;
          }
          if (!Domain::measurementAllowed(scale.gross(), value.empty, value.initial, scale.calibrated(), scale.stable(), scale.fresh())) {
            error(409, "A used spool needs a fresh stable calibrated measurement and a known empty spool weight before creating it in Spoolman."); return;
          }
          value.lastGross = scale.gross(); value.lastRemaining = value.lastGross - value.empty; ++value.measurement;
        }
      }
    }
    if (existing && existing->remoteId > 0 && value.remoteId != existing->remoteId) {
      value.vendorId = value.filamentId = 0; value.pending = "";
    }
    if (!config.spoolman.isEmpty() && !bindOnly && value.remoteId > 0) {
      // Linking a prior ambiguous creation uses the actual server record, never submitted local metadata.
      JsonDocument response; int code; Spool remote;
      if (!remoteRequest("GET", "/spool/" + String(value.remoteId), nullptr, response, code) ||
          !parseRemoteSpool(response.as<JsonObjectConst>(), remote) || remote.remoteId != value.remoteId || !remote.remoteListed) {
        error(409, "Could not confirm an active Spoolman spool with that ID. The existing journal was kept."); return;
      }
      for (const auto &other : spools) if ((!existing || other.id != existing->id) && other.remoteId == value.remoteId) {
        if (!existing || existing->pending.isEmpty() || !other.remote || !other.pending.isEmpty() ||
            (!other.tagUid.isEmpty() && !value.tagUid.isEmpty() && other.tagUid != value.tagUid) || !mergeCacheId.isEmpty()) {
          error(409, "This Spoolman spool already has a record or conflicting tag binding. Select it directly or resolve its tag links before reconciling."); return;
        }
        mergeCacheId = other.id;
        if (value.tagUid.isEmpty()) value.tagUid = other.tagUid;
      }
      remote.id = value.id; remote.tagUid = value.tagUid; remote.measurement = value.measurement;
      remote.lastGross = value.lastGross; remote.lastRemaining = value.lastRemaining; value = remote;
    }
    if (value.remoteId > 0) value.pending = "";
    if (doc["tag_uid"].is<const char *>()) {
      String requested = doc["tag_uid"].as<String>();
      if (!textWithoutNul(doc["tag_uid"]) || (!requested.isEmpty() && !SpoolActions::validUid(requested.c_str()))) { error(400, "Invalid tag UID. Scan the physical tag first."); return; }
      if (!requested.isEmpty() && (!nfc.scanIdentity() || requested != nfc.uid)) { error(409, "Tag changed or multiple tags detected. Rescan before linking its UID."); return; }
      for (const auto &s : spools) if (s.id != value.id && s.id != mergeCacheId && !requested.isEmpty() && SpoolActions::tagMatches(s, requested.c_str())) { error(409, "This tag already identifies another spool."); return; }
      value.tagUid = requested;
    }
    Spool before; std::vector<Spool> beforeMerge;
    if (!mergeCacheId.isEmpty()) beforeMerge = spools;
    if (existing) { before = *existing; *existing = value; }
    else { value.id = randomHex(8); spools.push_back(value); }
    if (!mergeCacheId.isEmpty()) {
      spools.erase(std::remove_if(spools.begin(), spools.end(), [&](const Spool &s) { return s.id == mergeCacheId; }), spools.end());
      existing = findStoredSpool(value.id);
    }
    if (!saveInventory()) {
      if (!mergeCacheId.isEmpty()) spools = std::move(beforeMerge);
      else if (existing) *existing = before; else spools.pop_back();
      error(507, "Could not save the inventory."); return;
    }
    if (!mergeCacheId.isEmpty() && activeId == mergeCacheId) chooseSpool(value.id);
    Spool &saved = existing ? *existing : spools.back();
    if (create && !syncSpool(saved)) {
      JsonDocument result; result["error"] = syncError; result["draft"] = true;
      result["recovery_required"] = !saved.pending.isEmpty(); spoolJson(result["spool"].to<JsonObject>(), saved);
      reply(result, 502); return;
    }
    if (create && !saveInventory()) {
      JsonDocument result; result["error"] = "Spoolman confirmed creation, but the final cache could not be saved. Keep the returned spool ID; do not create it again.";
      result["draft"] = true; result["recovery_required"] = true; spoolJson(result["spool"].to<JsonObject>(), saved);
      reply(result, 507); return;
    }
    JsonDocument result; result["ok"] = true; result["inventory_source"] = config.spoolman.isEmpty() ? "local" : "spoolman";
    spoolJson(result["spool"].to<JsonObject>(), saved); reply(result);
    if (create) { remoteInventory.attempted = false; remoteInventory.failed = true; } // Next inventory request reads the complete server list.
  });
  server.on("/api/spools/tag", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    if (!storeReady) { error(503, "Inventory storage is unavailable."); return; }
    if (!textWithoutNul(doc["id"]) || !Domain::validId(doc["id"])) { error(400, "A valid spool id is required."); return; }
    if (!doc["unlink"].isUnbound() && !doc["unlink"].is<bool>()) { error(400, "unlink must be true or false."); return; }
    const bool unlink = doc["unlink"] == true;
    Spool *s = unlink ? findStoredSpool(doc["id"].as<String>()) : findSpool(doc["id"].as<String>());
    if (!s) { error(404, "Spool not found."); return; }
    if (!s->pending.isEmpty()) { error(409, "Resolve the pending Spoolman operation before linking a tag."); return; }
    if (unlink) {
      if (!textWithoutNul(doc["expected_uid"])) { error(400, "Confirm the currently saved RFID tag ID before removing its link."); return; }
      const String expected = doc["expected_uid"].as<String>();
      const auto check = SpoolActions::unlink(s->tagUid.c_str(), expected.c_str(), s->pending.c_str());
      if (check == SpoolActions::Unlink::InvalidUid) { error(400, "The saved RFID tag ID must contain 1–32 characters."); return; }
      if (check != SpoolActions::Unlink::Allowed) { error(409, "The saved RFID link changed. Refresh and confirm the current tag ID before removing it."); return; }
      const String previous = s->tagUid; s->tagUid = "";
      if (!saveInventory()) { s->tagUid = previous; error(507, "Could not save the inventory. The RFID link was kept."); return; }
      if (nfcSpoolId == s->id) { nfcSpoolId = ""; nfcMetadata.clear(); }
      JsonDocument result; result["ok"] = true;
      result["message"] = "RFID link removed from this scale. The tag, spool profile and Spoolman were not changed.";
      spoolJson(result["spool"].to<JsonObject>(), *s); reply(result); return;
    }
    if (s->remote && inventoryStale()) { error(409, "Refresh the Spoolman inventory before linking this tag."); return; }
    if (!textWithoutNul(doc["expected_uid"]) || !SpoolActions::validUid(doc["expected_uid"])) { error(400, "Scan and confirm the target tag UID first."); return; }
    const String expected = doc["expected_uid"].as<String>();
    startStatusOperation(StatusModel::Operation::Nfc);
    if (!nfc.scanIdentity() || nfc.uid != expected) {
      finishStatusOperation(false); error(409, "Tag changed, disappeared, or multiple tags were detected. Scan one tag again before linking it."); return;
    }
    for (const auto &other : spools) if (other.id != s->id && SpoolActions::tagMatches(other, expected.c_str())) {
      finishStatusOperation(false); error(409, "This tag already identifies another spool. Change or remove that spool's link first."); return;
    }
    const String oldUid = s->tagUid; s->tagUid = expected;
    if (!saveInventory()) {
      s->tagUid = oldUid; finishStatusOperation(false); error(507, "Could not save the tag link. The existing binding was kept."); return;
    }
    nfcMetadata.clear(); nfcSpoolId = s->id; chooseSpool(s->id); nextNfc = millis() + 2000;
    finishStatusOperation(true);
    JsonDocument result; result["ok"] = true;
    result["message"] = "RFID linked to this spool on the scale. Tag contents were not changed.";
    spoolJson(result["spool"].to<JsonObject>(), *s); reply(result);
  });
  server.on("/api/spools/remove", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    if (!storeReady) { error(503, "Inventory storage is unavailable."); return; }
    if (!textWithoutNul(doc["id"]) || !Domain::validId(doc["id"])) { error(400, "A valid spool id is required."); return; }
    Spool *s = findStoredSpool(doc["id"].as<String>());
    if (!s) { error(404, "Spool not found."); return; }
    const auto action = SpoolActions::removal(s->remoteId, !config.spoolman.isEmpty(), s->pending.c_str());
    if (action == SpoolActions::Removal::Unresolved) { error(409, "Resolve the pending Spoolman creation before removing this spool. Its recovery record was kept."); return; }
    if (action == SpoolActions::Removal::MissingServer) { error(409, "Reconnect the original Spoolman server to archive this linked spool."); return; }
    const bool remote = action == SpoolActions::Removal::Archive;
    bool removedMissing = false;
    if (remote) {
      if (WiFi.status() != WL_CONNECTED) { error(409, "Connect Wi-Fi before archiving a Spoolman spool. No local record was removed."); return; }
      if (!SpoolActions::archiving(s->pending.c_str()) && inventoryStale()) { error(409, "Refresh the Spoolman inventory before archiving this spool."); return; }
      if (!archiveSpool(*s, removedMissing)) {
        remoteInventory.failed = true; inventoryError = syncError;
        JsonDocument result; result["error"] = syncError;
        result["recovery_required"] = SpoolActions::archiving(s->pending.c_str());
        spoolJson(result["spool"].to<JsonObject>(), *s); reply(result, 502); return;
      }
    }
    const size_t index = size_t(s - spools.data());
    const Spool previous = *s; const String id = s->id;
    spools.erase(spools.begin() + index);
    if (!saveInventory()) {
      spools.insert(spools.begin() + index, previous);
      JsonDocument result; result["error"] = removedMissing ? "Spoolman confirmed the spool is missing, but the device could not save the cleanup. Check archive status again; the recovery record was kept." : remote ? "Spoolman confirmed the archive, but the device could not save it. Check archive status to finish; the recovery record was kept." : "Could not save the inventory. The spool and selection were kept.";
      result["recovery_required"] = remote; spoolJson(result["spool"].to<JsonObject>(), previous); reply(result, 507); return;
    }
    if (activeId == id) chooseSpool("");
    if (nfcSpoolId == id) { nfcSpoolId = ""; nfcMetadata.clear(); }
    if (remote) { remoteInventory.attempted = false; remoteInventory.failed = true; inventoryError = "Refresh to load the remaining Spoolman inventory."; }
    JsonDocument result; result["ok"] = true; result["id"] = id; result["archived"] = remote && !removedMissing; result["removed_missing"] = removedMissing;
    result["message"] = removedMissing ? "Spoolman confirmed this spool is already missing. Its local recovery record and RFID link were removed; nothing was archived or changed on the server." : remote ? "Spool archived in Spoolman and removed from the active inventory. Its history remains in Spoolman." : "Spool removed from this scale. The physical tag was not changed.";
    reply(result);
  });
  server.on("/api/active-spool", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    if (doc["id"].isNull()) { chooseSpool(""); ok("Spool selection cleared."); return; }
    if (!doc["id"].is<const char *>() || !findSpool(doc["id"].as<String>())) { error(404, "Spool not found."); return; }
    chooseSpool(doc["id"].as<String>()); ok("Spool selected.");
  });
  server.on("/api/measurements", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    Spool *s = requestedSpool(doc); if (!s || !validMeasurement(*s)) return;
    if (!recordMeasurement(*s)) { error(507, "Cannot persist measurement; no remote update was sent."); return; }
    JsonDocument result; result["ok"] = true; result["gross_g"] = s->lastGross;
    result["remaining_weight_g"] = s->lastRemaining; result["message"] = "Measurement saved on the scale."; reply(result);
  });
}
}  // namespace spoolancer
