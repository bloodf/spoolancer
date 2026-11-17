#include "Routes.h"
#include <WiFi.h>
#include "Device.h"
#include "DeviceUi.h"
#include "Http.h"
#include "Inventory.h"
#include "BrandTags.h"
#include "JsonNumbers.h"
#include "NfcFlow.h"
#include "Pins.h"
#include "ReadTags.h"
#include <cstring>

namespace spoolancer {
using JsonNumbers::numberOrNull;
void registerHardwareRoutes() {
  server.on("/api/diagnostics", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument input; if (!body(input)) return;
    if (!textWithoutNul(input["component"])) { error(400, "Choose nfc, leds or scale."); return; }
    const String component = input["component"].as<String>();
    if (component != "nfc" && component != "leds" && component != "scale") { error(400, "Choose nfc, leds or scale."); return; }
    JsonDocument result; result["ok"] = true; result["component"] = component;
    if (component == "leds") {
      if (!StatusUi::testLeds(millis())) { error(409, "This firmware profile has no addressable status LEDs."); return; }
      renderStatusUi(true);
      result["state"] = "observe"; result["duration_ms"] = StatusModel::ledTestMs;
      result["message"] = "Watch all five LEDs: red, green, blue, one white LED at a time, then a 0–100% bar. Normal status returns after 12 seconds. This checks the output signal; confirm the light and color order visually.";
    } else if (component == "nfc") {
      startStatusOperation(StatusModel::Operation::Nfc);
      nfc.begin(nfcIdle);
      const bool detected = nfc.scanIdentity();
      nfcSpoolId = ""; nfcMetadata.clear(); nextNfc = millis() + 2000;
      result["reader_available"] = nfc.available; result["tag_detected"] = detected;
      result["transport"] = "i2c"; result["sda_gpio"] = Pins::nfcSda; result["scl_gpio"] = Pins::nfcScl;
      result["i2c_error"] = nfc.i2cError; result["transport_step"] = nfc.transportStep;
      result["sda_high"] = digitalRead(Pins::nfcSda) == HIGH; result["scl_high"] = digitalRead(Pins::nfcScl) == HIGH;
      result["targets"] = nfc.targets; result["uid"] = nfc.uid; result["type"] = nfc.type;
      result["state"] = detected ? "pass" : nfc.available && nfc.targets < 2 ? "warning" : "fail";
      result["message"] = detected ? "One tag ID was read. Reader communication and tag detection passed. No tag data or spool profile was changed." :
        nfc.available && nfc.targets < 2 ? "PN532 is responding, but no single tag was read. Hold one tag beside the antenna and test again." :
        nfc.targets > 1 ? "Multiple tags detected. Keep only one tag beside the antenna and test again." :
        "PN532 did not respond. Check 3.3 V power, I2C mode (switch 1 ON, 2 OFF), SDA and SCL connections, then retry.";
      if (!nfc.available) {
        result["message"] = nfc.i2cError == 2 ? String("No acknowledgement at PN532 I2C address 0x24 on GPIO") + Pins::nfcSda + " SDA / GPIO" + Pins::nfcScl + " SCL. Check power, common ground, shared header pins, and mode switches, then power-cycle."
          : nfc.i2cError == 5 ? "PN532 I2C transaction timed out. Check SDA/SCL connections, pull-ups and power."
          : "PN532 handshake failed after the I2C write. Check the transport details and retry after a power-cycle.";
      }
      finishStatusOperation(detected);
    } else {
      startStatusOperation(StatusModel::Operation::Scale);
      scale.tick();
      result["fresh"] = scale.fresh(); result["stable"] = scale.stable();
      result["calibrated"] = scale.calibrated(); result["sample_count"] = scale.sampleCount();
      numberOrNull(result["raw_mean"], scale.mean()); numberOrNull(result["gross_g"], scale.gross());
      result["state"] = !scale.fresh() ? "fail" : !scale.calibrated() || !scale.stable() ? "warning" : "pass";
      result["message"] = !scale.fresh() ? "No fresh HX711 samples. Check its power, DOUT and clock wiring, then retry." :
        !scale.calibrated() ? "HX711 samples are arriving. Complete Calibration to convert them to grams." :
        !scale.stable() ? "HX711 samples are arriving, but the weight is settling. Keep the cradle still and retry." :
        "Fresh, stable weight samples are arriving. Add and remove a known weight to check response. Use Calibration to verify accuracy.";
      finishStatusOperation(scale.fresh());
    }
    reply(result);
  });
  server.on("/api/scale/tare", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    if (!zeroCradle()) { error(409, "Could not zero and save calibration. Keep the empty cradle still and check device storage."); return; }
    chooseSpool("");
    ok("Empty cradle zero saved. This is not the empty-spool weight.");
  });
  server.on("/api/scale/calibrate", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    double grams;
    const double previousFactor = scale.countsPerGram;
    if (!numericField(doc.as<JsonObjectConst>(), "reference_g", grams, 100, 2000) || !scale.calibrate(grams)) {
      error(409, "Zero the empty cradle first, then place a known 100–2000 g reference and keep it still."); return;
    }
    if (!saveCalibration()) { scale.countsPerGram = previousFactor; error(507, "Could not save calibration."); return; }
    ok("Calibration saved. Verify with independent known weights.");
  });
  server.on("/api/nfc/read", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    readNfc(true); nextNfc = millis() + 2000;
    if (!nfc.present || nfc.targets != 1 || nfc.uid.isEmpty()) { error(409, nfc.lastError.isEmpty() ? "Align exactly one tag with the reader." : nfc.lastError); return; }
    JsonDocument result; result["ok"] = true; nfcJson(result["nfc"].to<JsonObject>());
    if (nfcSpoolId.isEmpty()) result["spool_id"] = nullptr; else result["spool_id"] = nfcSpoolId;
    result["message"] = nfc.lastMessage; reply(result);
  });
  server.on("/api/nfc/write", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    Spool *s = requestedSpool(doc); if (!s) return;
    if (!doc["expected_uid"].is<const char *>() || doc["expected_uid"].as<String>().isEmpty()) { error(400, "Scan and confirm the target tag UID first."); return; }
    BrandTags::Format format = BrandTags::Format::Spoolancer;
    if (!doc["format"].isNull() && !BrandTags::parseFormat(doc["format"].as<const char *>(), format)) { error(400, "Unknown tag format."); return; }
    const String problem = writeSpoolTag(*s, doc["expected_uid"].as<String>(), format);
    if (!problem.isEmpty()) { error(409, problem); return; }
    ok(format == BrandTags::Format::Spoolancer ? "Tag written and read-back verified. Printer tags were not modified." :
       String(BrandTags::info(format).label) + " tag written and read-back verified. Its ID is linked to this spool.");
  });
  server.on("/api/nfc/erase", HTTP_POST, [] {
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc) || !confirmation(doc)) return;
    if (!doc["expected_uid"].is<const char *>() || doc["expected_uid"].as<String>().isEmpty()) { error(400, "Scan and confirm the target tag UID first."); return; }
    startStatusOperation(StatusModel::Operation::Nfc);
    const String problem = eraseTag(doc["expected_uid"].as<String>());
    finishStatusOperation(problem.isEmpty());
    if (!problem.isEmpty()) { error(409, problem); return; }
    ok("Tag erased and read-back verified. Its ID stays linked to any spool until you unlink it.");
  });
  server.on("/api/nfc/clone", HTTP_POST, [] {
    // Two-step Bambu clone: read a genuine tag, then copy it byte-for-byte onto a magic card.
    if (!authorized()) return;
    JsonDocument doc; if (!body(doc)) return;
    const String step = doc["step"] | "";
    startStatusOperation(StatusModel::Operation::Nfc);
    if (step == "read") {
      cloneReady = false;
      if (!nfc.scanIdentity() || !nfc.classic1k()) { finishStatusOperation(false); error(409, nfc.lastError.isEmpty() ? "Present one genuine Bambu Lab tag (MIFARE Classic 1K)." : nfc.lastError); return; }
      ReadTags::Decoded tag;
      if (!nfc.readClassic(nfc.uid, tag) || !tag.valid) { finishStatusOperation(false); error(409, nfc.lastError); return; }
      if (nfc.type != "Bambu Lab (read-only)") { finishStatusOperation(false); error(409, "Only Bambu Lab tags can be cloned. Snapmaker tags are signed over their keys and cannot be cloned."); return; }
      if (!nfc.readClassicDump(nfc.uid, cloneImage)) { finishStatusOperation(false); error(409, "Could not read the full tag. Hold it still and try again."); return; }
      std::memcpy(cloneUid, nfc.uidData(), 4); cloneReady = true;
      cloneLabel = String(tag.brand) + " " + tag.material + " " + tag.color;
      JsonDocument result; result["ok"] = true; result["step"] = "read"; result["source_uid"] = nfc.uid;
      result["label"] = cloneLabel; result["message"] = "Source tag read. Put a blank Gen2/FUID magic card on the reader and confirm the copy.";
      finishStatusOperation(true); reply(result); return;
    }
    if (step == "write") {
      if (!confirmation(doc)) { finishStatusOperation(false); return; }
      if (!cloneReady) { finishStatusOperation(false); error(409, "Read a source tag first."); return; }
      const bool done = nfc.cloneClassic(cloneImage, cloneUid);
      finishStatusOperation(done);
      if (!done) { error(409, nfc.lastError); return; }
      cloneReady = false;
      ok("Magic card cloned and read-back verified. Test recognition on your printer; acceptance is not guaranteed.");
      return;
    }
    finishStatusOperation(false); error(400, "Clone step must be \"read\" or \"write\".");
  });
}
}  // namespace spoolancer
