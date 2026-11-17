#include "DeviceUi.h"
#include <WiFi.h>
#include "Device.h"
#include "Inventory.h"
#include "Pins.h"
#ifdef SPOOLANCER_TOUCH
#include "TouchUi.h"
#include "NfcFlow.h"
#include "Spoolman.h"
#endif

namespace spoolancer {
void renderStatusUi(bool force) {
#ifdef SPOOLANCER_STATUS_LEDS
  static uint32_t lastRender = 0;
  if (!force && uint32_t(millis() - lastRender) < 20) return;
  lastRender = millis();
  StatusUi::Snapshot state;
  state.gross = scale.gross(); state.calibrated = scale.calibrated();
  state.fresh = scale.fresh(); state.stable = scale.stable(); state.setup = apActive;
  state.connected = WiFi.status() == WL_CONNECTED; state.connecting = connectionAttempt;
  const Spool *spool = findSpool(activeId);
  if (spool) {
    state.selected = true; state.empty = spool->empty; state.initial = spool->initial;
    state.material = spool->material.c_str();
  }
  const String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() :
                    (apActive ? WiFi.softAPIP().toString() : "");
  state.ip = ip.c_str();
  StatusUi::render(state, millis(), force);
#endif
}
void nfcIdle() { scale.tick(); renderStatusUi(); }
void startStatusOperation(StatusModel::Operation operation) {
  StatusUi::start(operation, millis()); renderStatusUi(true);
}
void finishStatusOperation(bool success) {
  StatusUi::finish(success, millis()); renderStatusUi(true);
}
#ifdef SPOOLANCER_TOUCH
void renderTouch() {
  TouchUi::State state;
  state.gross = scale.gross(); state.stable = scale.stable(); state.calibrated = scale.calibrated();
  state.tagUid = nfc.uid; state.tagType = nfc.type;
  state.ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : (apActive ? WiFi.softAPIP().toString() : "");
  state.pin = pin; state.message = appMessage;
  if (Setup::required(config.auth) && apActive) {
    state.message = "Open setup Wi-Fi " + apName + ". Choose Wi-Fi and optional PIN in the browser.";
  }
  Spool *spool = findSpool(activeId);
  if (spool) {
    state.spoolId = spool->id; state.name = spool->name; state.brand = spool->brand;
    state.material = spool->material; state.color = spool->color;
    const String problem = measurementProblem(*spool);
    if (problem.isEmpty()) state.remaining = state.gross - spool->empty;
    else if (state.message.isEmpty() || state.message == "Spool selected.") state.message = problem;
    state.canWrite = storeReady && nfc.present && nfc.writable;
    state.canSave = storeReady && problem.isEmpty();
    state.canSync = state.canSave && spool->pending.isEmpty() && !config.spoolman.isEmpty() && WiFi.status() == WL_CONNECTED;
  }
  TouchUi::render(state);
}
void pollTouch() {
  const TouchUi::Command command = TouchUi::poll();
  using Action = TouchUi::Action;
  if (command.action == Action::None) return;
  if (command.action == Action::Previous || command.action == Action::Next) {
    if (!cycleVisibleSpool(command.action == Action::Next)) { appMessage = "Load the inventory in the browser first."; return; }
    appMessage = "Spool selected."; return;
  }
  if (command.action == Action::Read) {
    readNfc(true); nextNfc = millis() + 2000;
    appMessage = nfc.lastError.isEmpty() ? nfc.lastMessage : nfc.lastError; return;
  }
  if (command.action == Action::Zero) {
    if (zeroCradle()) { chooseSpool(""); appMessage = "Empty cradle zero saved."; }
    else appMessage = "Cannot zero/save: keep the EMPTY cradle still and check storage.";
    return;
  }
  Spool *spool = findSpool(command.spoolId);
  if (!spool || activeId != command.spoolId) { appMessage = "Spool selection changed. Review and confirm again."; return; }
  if (command.action == Action::Write) {
    appMessage = "Writing inventory tag. Keep the spool and tag still."; renderTouch();
    const String problem = writeSpoolTag(*spool, command.expectedUid, BrandTags::Format::Spoolancer);
    appMessage = problem.isEmpty() ? "Inventory tag written and read-back verified." : problem; return;
  }
  if (command.action == Action::Save || command.action == Action::Sync) {
    const String problem = measurementProblem(*spool);
    if (!problem.isEmpty()) { appMessage = problem; return; }
    if (command.action == Action::Sync && !spool->pending.isEmpty()) {
      appMessage = "Prior Spoolman creation is unresolved. Check the server and link its existing spool ID in the browser."; return;
    }
    if (!recordMeasurement(*spool)) { appMessage = "Cannot persist measurement; no remote update was sent."; return; }
    if (command.action == Action::Save) { appMessage = "Measurement saved on the scale."; return; }
    appMessage = "Syncing saved weight to Spoolman. Please wait."; renderTouch();
    appMessage = syncSpool(*spool) ? syncMessage : syncError;
  }
}
#endif
#ifndef SPOOLANCER_HEADLESS
void displayStatus() {
  display.fillScreen(ST77XX_BLACK); display.setCursor(10, 8); display.setTextSize(2);
  display.setTextColor(ST77XX_CYAN); display.println(config.name.substring(0, 23));
  display.setTextColor(ST77XX_WHITE);
  const double gross = scale.gross();
  display.setTextSize(3); display.setCursor(10, 40);
  if (std::isfinite(gross)) { display.print(gross, 1); display.println(" g"); }
  else { display.setTextSize(2); display.println(scale.calibrated() ? "Waiting for sensor" : "Calibration needed"); }
  display.setTextSize(2); display.setCursor(10, 78);
  Spool *s = findSpool(activeId);
  if (s) {
    display.println(s->name.substring(0, 24));
    if (std::isfinite(s->empty) && std::isfinite(gross)) { display.print("Filament: "); display.print(gross - s->empty, 1); display.println(" g"); }
    else display.println("Set empty-spool weight");
  } else display.println("Scan / select a spool");
  display.setTextSize(1); display.setCursor(10, 134);
  display.println(scale.stable() ? "Weight stable" : "Waiting for stable weight");
  display.println(nfc.available ? nfc.lastMessage.substring(0, 48) : nfc.lastError.substring(0, 48));
  display.setCursor(10, 166);
  display.print("http://"); display.println(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "192.168.4.1");
  if (int32_t(pinVisibleUntil - millis()) > 0 || config.ssid.isEmpty()) {
    display.println(apActive ? apName : "Pair device in browser");
    display.print("Device PIN / AP password: "); display.println(pin);
  } else display.println("Hold PREVIOUS 3s to show device PIN");
  display.println("Hold SELECT 2s: zero EMPTY cradle");
  display.println("Hold all buttons 8s: reset Wi-Fi/PIN");
}
void buttons() {
  static uint32_t previousDown = 0, selectDown = 0, allDown = 0, lastCycle = 0;
  static bool selectDone = false, previousDone = false, nextWasDown = false;
  const bool previous = digitalRead(Pins::previous) == LOW;
  const bool next = digitalRead(Pins::next) == LOW;
  const bool select = digitalRead(Pins::select) == LOW;
  const uint32_t now = millis();
  if (previous && next && select) {
    if (!allDown) allDown = now;
    if (now - allDown > 8000) {
      Config reset = config; reset.ssid = reset.password = "";
      reset.pin = generatedPin(); reset.auth = Setup::AuthMode::Protected;
      if (!saveConfig(reset)) { appMessage = "Could not save Wi-Fi reset."; allDown = now; return; }
      config = reset; pin = config.pin; for (auto &s : sessions) s.token = "";
      loginFailures = 0; nextLogin = 0;
      reconnectAt = 0; connectionAttempt = false; cancelWifiScan();
      WiFi.disconnect(true, true); WiFi.softAPdisconnect(true); apActive = false; startAp();
      appMessage = "Wi-Fi and device PIN reset. Inventory and calibration kept.";
      allDown = now; selectDone = previousDone = true;
    }
    return;
  }
  allDown = 0;
  if (previous) {
    if (!previousDown) previousDown = now;
    if (!previousDone && now - previousDown > 3000) { pinVisibleUntil = now + 60000; previousDone = true; }
  } else {
    if (previousDown && !previousDone && now - previousDown > 40 && !spools.empty()) {
      cycleVisibleSpool(false);
    }
    previousDown = 0; previousDone = false;
  }
  if (next && !nextWasDown && now - lastCycle > 200 && !spools.empty()) {
    cycleVisibleSpool(true); lastCycle = now;
  }
  nextWasDown = next;
  if (select) {
    if (!selectDown) selectDown = now;
    if (!selectDone && now - selectDown > 2000) {
      if (zeroCradle()) { chooseSpool(""); appMessage = "Cradle zero saved."; }
      else appMessage = "Cannot zero: keep empty cradle still and check sensor.";
      selectDone = true;
    }
  } else { selectDown = 0; selectDone = false; }
}
#else
void buttons() {
  static Setup::Button button;
  const Setup::Gesture gesture = button.update(digitalRead(Pins::wifiReset) == LOW, millis());
  if (gesture == Setup::Gesture::ResetWifi) {
    Config reset = config; reset.ssid = reset.password = "";
    reset.pin = ""; reset.auth = Setup::AuthMode::Unconfigured;
    if (!saveConfig(reset)) { appMessage = "Could not save Wi-Fi reset."; return; }
    pin = ""; loginFailures = 0; nextLogin = 0;
    config = reset; for (auto &session : sessions) session.token = "";
    reconnectAt = 0; connectionAttempt = false; cancelWifiScan();
    WiFi.disconnect(true, true); WiFi.softAPdisconnect(true); apActive = false; startAp();
    appMessage = "Wi-Fi and pairing reset. Inventory and calibration kept. Join the temporary open setup network.";
  }
}
#endif
}  // namespace spoolancer
