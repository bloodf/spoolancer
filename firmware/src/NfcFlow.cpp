#include "NfcFlow.h"
#include <esp_system.h>
#include <cmath>
#include "DeviceUi.h"
#include "Domain.h"
#include "Inventory.h"
#include "InventoryTag.h"
#include "ReadTags.h"
#include "SpoolActions.h"

namespace spoolancer {
// Which printer formats the presented tag can take: "ntag" (Anycubic, Elegoo, OpenSpool) or "classic" (Creality, Qidi).
const char *nfcChip() { return nfc.classic1k() ? "classic" : nfc.ndefCapacity() ? "ntag" : "other"; }
void nfcJson(JsonObject obj) {
  obj["available"] = nfc.available; obj["present"] = nfc.present;
  obj["uid"] = nfc.uid; obj["type"] = nfc.type; obj["writable"] = nfc.writable;
  obj["raw_writable"] = nfc.rawWritable; obj["chip"] = nfcChip();
  obj["capacity_bytes"] = nfc.ndefCapacity();
  if (const Spool *s = findSpool(activeId); s && nfc.ndefCapacity()) {
    JsonDocument tag; spoolJson(tag.to<JsonObject>(), *s);
    InventoryTag::prepare(tag, s->id.c_str(), nfc.ndefCapacity());
    JsonObject plan = obj["inventory_write"].to<JsonObject>();
    plan["spool_id"] = s->id; plan["kind"] = InventoryTag::linkOnly(tag.as<JsonObjectConst>()) ? "link" : "profile";
    plan["bytes"] = InventoryTag::bytes(tag); plan["fits"] = InventoryTag::fits(tag, nfc.ndefCapacity());
  }
  obj["last_message"] = nfc.lastMessage; obj["last_error"] = nfc.lastError;
  if (nfcSpoolId.isEmpty()) obj["spool_id"] = nullptr; else obj["spool_id"] = nfcSpoolId;
  if (!nfcMetadata.isNull()) obj["metadata"].set(nfcMetadata);
}
void readNfcImpl(bool explicitRead) {
  const String previousUid = nfc.uid;
  nfcSpoolId = ""; nfcMetadata.clear();
  nfc.scan(explicitRead);
  // A failed or unlinked explicit scan keeps the user's manual selection.
  if (!explicitRead && (nfc.targets > 1 || (nfc.present && nfc.uid != previousUid))) chooseSpool("");
  if (!nfc.present || nfc.targets != 1 || nfc.uid.isEmpty()) return;
  // Resolve exactly one visible spool. Conflicting local/server links never select by guess.
  const int match = SpoolActions::matchTag(spools, nfc.uid.c_str(), visibleSpool);
  if (match == -2) {
    // Clear once when the tag arrives; later polls must not undo a manual choice.
    if (explicitRead || nfc.uid != previousUid) chooseSpool("");
    nfc.lastError = "This tag matches several inventory spools. Fix the duplicate RFID links before weighing.";
    return;
  }
  if (match >= 0) nfcSpoolId = spools[match].id;
  if (!nfc.payload.isEmpty()) {
    JsonDocument tag;
    if (deserializeJson(tag, nfc.payload, DeserializationOption::NestingLimit(3)) || tag["schema"] != 1 ||
        !tag["spool_id"].is<const char *>() || !Domain::validId(tag["spool_id"])) {
      nfc.writable = false; nfc.lastError = "Invalid Spoolancer tag record; writing refused.";
    } else if (InventoryTag::linkOnly(tag.as<JsonObjectConst>())) {
      nfc.lastMessage = "Compact inventory link read. Full spool details stay in the scale or Spoolman inventory.";
    } else {
      Spool parsed;
      if (!validateSpool(tag.as<JsonObjectConst>(), parsed)) {
        nfc.writable = false; nfc.lastError = "Invalid tag metadata; writing refused.";
      } else {
        parsed.id = tag["spool_id"].as<String>();
        spoolJson(nfcMetadata.to<JsonObject>(), parsed);
      }
    }
  }
  // Explicit read of a genuine Bambu Lab / Snapmaker Classic tag: show a read-only material preview.
  if (explicitRead && nfc.payload.isEmpty() && nfcMetadata.isNull() && nfc.classic1k()) {
    ReadTags::Decoded tag;
    if (nfc.readClassic(nfc.uid, tag) && tag.valid) {
      JsonObject meta = nfcMetadata.to<JsonObject>();
      meta["brand"] = tag.brand; meta["material"] = tag.material; meta["color"] = tag.color;
      if (tag.weightG) meta["initial_weight_g"] = tag.weightG;
      meta["read_only"] = true; meta["source"] = "printer_tag";
    }
  }
  // Re-selecting on every poll would override a manual choice while the same tag stays seated.
  // Retried while nothing is selected (tag read before the load settled), never over a manual choice.
  if (!nfcSpoolId.isEmpty() && (explicitRead || activeId.isEmpty() || nfc.uid != previousUid)) chooseSpool(nfcSpoolId, "nfc");
}
void readNfc(bool explicitRead) {
  if (explicitRead) startStatusOperation(StatusModel::Operation::Nfc);
  readNfcImpl(explicitRead);
  if (explicitRead) finishStatusOperation(nfc.present && nfc.targets == 1 && !nfc.uid.isEmpty());
}
// Printer-ecosystem formats; the scale still identifies the spool by its linked UID.
String writeBrandTag(const Spool &s, const String &expectedUid, BrandTags::Format format) {
  using namespace BrandTags;
  const Profile *profile = profileFor(s.material.c_str());
  if (!profile) return "No " + String(info(format).label) + " material profile for \"" + s.material + "\". Use PLA, PETG, ABS, ASA, TPU, PC, PA, PVA or their CF/Silk/Matte variants.";
  if (!supports(format, *profile)) return String(info(format).label) + " has no " + profile->name + " profile.";
  // Remaining filament is what the printer should start counting from; fall back to the full spool.
  // Length starts from the remaining filament; weight fields stay the nominal spool size, as upstream tools write.
  const double grams = std::isfinite(s.lastRemaining) && s.lastRemaining > 0 ? s.lastRemaining : s.initial;
  const uint16_t meters = lengthMeters(grams, s.density, s.diameter);
  bool ok = false;
  switch (format) {
    case Format::Creality: {
      if (!nfc.scanIdentity() || nfc.uid != expectedUid || !nfc.classic1k()) return "Present one MIFARE Classic 1K tag with a 4-byte ID for Creality CFS.";
      uint8_t key[6], blocks[48];
      crealityKey(nfc.uidData(), key);
      if (!crealityBlocks(*profile, s.color.c_str(), meters, esp_random() % 1000000, blocks)) return "Could not encode the Creality record.";
      const uint8_t keys[][6] = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, {key[0], key[1], key[2], key[3], key[4], key[5]}};
      ok = nfc.writeClassicSector1(expectedUid, keys, 2, blocks, 3, key);
      break;
    }
    case Format::Qidi: {
      uint8_t block[16];
      if (!qidiBlock(*profile, s.color.c_str(), block)) return "Could not encode the Qidi record.";
      const uint8_t keys[][6] = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, {0xd3, 0xf7, 0xd3, 0xf7, 0xd3, 0xf7}};
      ok = nfc.writeClassicSector1(expectedUid, keys, 2, block, 1, nullptr);
      if (!ok && nfc.classic1k()) {
        // A tag previously written for Creality: restore the default keys the Qidi Box uses.
        uint8_t creality[6]; crealityKey(nfc.uidData(), creality);
        const uint8_t retry[][6] = {{creality[0], creality[1], creality[2], creality[3], creality[4], creality[5]}};
        ok = nfc.writeClassicSector1(expectedUid, retry, 1, block, 1, keys[0]);
      }
      break;
    }
    case Format::Anycubic: {
      uint8_t data[anycubicPages * 4];
      if (!anycubicData(*profile, s.color.c_str(), meters, s.initial, s.diameter, data)) return "Could not encode the Anycubic record.";
      ok = nfc.writeNtagPages(expectedUid, anycubicFirstPage, data, anycubicPages);
      break;
    }
    case Format::Elegoo: {
      uint8_t data[elegooPages * 4];
      if (!elegooData(*profile, s.color.c_str(), s.initial, s.diameter, data)) return "Could not encode the Elegoo record.";
      ok = nfc.writeNtagPages(expectedUid, elegooFirstPage, data, elegooPages);
      break;
    }
    case Format::OpenSpool: {
      char json[400];
      if (!openspoolJson(*profile, s.brand.c_str(), s.color.c_str(), s.initial, s.diameter, json, sizeof(json))) return "Could not encode the OpenSpool record.";
      ok = nfc.writeNdef(expectedUid, String(json), openspoolMime);
      break;
    }
    default: return "Unsupported tag format.";
  }
  return ok ? "" : nfc.lastError;
}
String writeSpoolTagImpl(Spool &s, const String &expectedUid, BrandTags::Format format) {
  if (!s.pending.isEmpty()) return "Resolve the pending Spoolman operation before writing this tag.";
  if (s.remote && inventoryStale()) return "Refresh Spoolman before writing this profile to a tag.";
  JsonDocument profile; spoolJson(profile.to<JsonObject>(), s); Spool checked;
  if (!validateSpool(profile.as<JsonObjectConst>(), checked)) return "Complete the spool metadata in Spoolman before writing an inventory tag.";
  if (expectedUid.isEmpty()) return "Scan and confirm the target tag UID first.";
  for (const auto &other : spools) if (other.id != s.id && SpoolActions::tagMatches(other, expectedUid.c_str()))
    return "This UID is already linked to another local spool. Unlink it before rewriting this inventory tag.";
  if (format != BrandTags::Format::Spoolancer) {
    const String problem = writeBrandTag(s, expectedUid, format);
    if (!problem.isEmpty()) return problem;
  } else {
    // Size the record for the tag actually present now, not a layout left by an earlier UID-only scan.
    if (!nfc.scan(true) || nfc.uid != expectedUid || !nfc.ndefCapacity()) return "Present the confirmed NTAG213/215/216 alone, then try again.";
    JsonDocument tag; spoolJson(tag.to<JsonObject>(), s);
    InventoryTag::prepare(tag, s.id.c_str(), nfc.ndefCapacity());
    String json; serializeJson(tag, json);
    if (!nfc.writeInventory(expectedUid, json)) return nfc.lastError;
  }
  const String oldUid = s.tagUid; s.tagUid = nfc.uid;
  if (!saveInventory()) {
    s.tagUid = oldUid;
    return "Tag was written and verified, but its local UID link could not be saved. Use Add RFID to link it before tag identification.";
  }
  nfcSpoolId = s.id; chooseSpool(s.id); nextNfc = millis() + 2000;
  return "";
}
String writeSpoolTag(Spool &s, const String &expectedUid, BrandTags::Format format) {
  startStatusOperation(StatusModel::Operation::Nfc);
  const String problem = writeSpoolTagImpl(s, expectedUid, format);
  finishStatusOperation(problem.isEmpty()); return problem;
}
// Clears tag memory only; UID links on the scale and in Spoolman are kept (unlink them separately).
String eraseTag(const String &expectedUid) {
  if (!nfc.scanIdentity() || nfc.uid != expectedUid) return "Present the confirmed tag alone, then try again.";
  bool ok;
  if (nfc.classic1k()) {
    // Sector 1 holds the Creality CFS / Qidi Box records; restore the factory key afterwards.
    uint8_t creality[6]; BrandTags::crealityKey(nfc.uidData(), creality);
    const uint8_t keys[][6] = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, {creality[0], creality[1], creality[2], creality[3], creality[4], creality[5]},
                               {0xd3, 0xf7, 0xd3, 0xf7, 0xd3, 0xf7}};
    const uint8_t zeros[48] = {};
    ok = nfc.writeClassicSector1(expectedUid, keys, 3, zeros, 3, keys[0]);
  } else {
    ok = nfc.eraseNtag(expectedUid);
  }
  if (!ok) return nfc.lastError.isEmpty() ? String("Tag could not be erased.") : nfc.lastError;
  nfcSpoolId = ""; nfcMetadata.clear(); nextNfc = millis() + 2000;
  return "";
}
}  // namespace spoolancer
