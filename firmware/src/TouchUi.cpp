#include "TouchUi.h"
#if defined(SPOOLANCER_TOUCH)
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <Wire.h>

namespace TouchUi {
namespace {
// Arduino_GFX takes R/G/B pins; the ESP-IDF RGB565 bus orders B/G/R bits.
Arduino_ESP32RGBPanel panel(40, 41, 39, 42,
    45, 48, 47, 21, 14, 5, 6, 7, 15, 16, 4, 8, 3, 46, 9, 1,
    0, 8, 4, 8, 0, 8, 4, 8, 1, 12500000, false, 0, 0);
Arduino_RGB_Display display(800, 480, &panel, 0, true);
Preferences settings;
State current;
Command pending;
bool screenReady = false, touchReady = false, dark = false, pressed = false;
bool confirmOpen = false, infoOpen = false;
uint8_t touchAddress = 0;
uint32_t nextTouch = 0;
String lastFrame;

uint16_t rgb(uint32_t c) { return ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f); }
uint16_t background() { return rgb(dark ? 0x101b19 : 0xf5f7f2); }
uint16_t surface() { return rgb(dark ? 0x1c2c28 : 0xffffff); }
uint16_t ink() { return rgb(dark ? 0xe9f3ec : 0x173f37); }
uint16_t muted() { return rgb(dark ? 0xb2c8bf : 0x506a61); }
uint16_t accent() { return rgb(dark ? 0x90d2ae : 0x1b5948); }
uint16_t disabled() { return rgb(dark ? 0x253d34 : 0xd9e3dc); }

String ascii(const String &value, size_t limit) {
  String result;
  for (size_t i = 0; i < value.length() && result.length() < limit; ++i) {
    uint8_t c = value[i];
    if (c < 128) result += (c >= 32 ? char(c) : ' ');
    else if ((c & 0xc0) != 0x80) result += '?';
  }
  return result;
}
void text(int x, int y, const String &value, int size = 2, uint16_t color = 0xffff, size_t limit = 60) {
  display.setTextSize(size); display.setTextColor(color); display.setCursor(x, y);
  display.print(ascii(value, limit));
}
void button(int x, int y, int w, const char *label, bool enabled = true, int h = 50) {
  display.fillRoundRect(x, y, w, h, 8, enabled ? accent() : disabled());
  text(x + 14, y + (h - 16) / 2, label, 2,
       enabled ? (dark ? rgb(0x13271d) : rgb(0xffffff)) : muted(), w / 12 - 1);
}
bool inside(int x, int y, int left, int top, int width, int height = 50) {
  return x >= left && x < left + width && y >= top && y < top + height;
}
String grams(double value) { return std::isfinite(value) ? String(value, 0) + " g" : "-- g"; }

void draw() {
  if (!screenReady) return;
  display.fillScreen(background());
  text(24, 22, "Spoolancer", 3, ink());
  button(510, 12, 120, "Setup", true, 44);
  button(646, 12, 130, dark ? "Light" : "Dark", true, 44);
  if (infoOpen) {
    text(24, 88, "Open the web interface", 3, ink());
    text(24, 140, current.ip.isEmpty() ? "No network. Hold BOOT 8s to reopen setup." : "http://" + current.ip, 2, ink());
    text(24, 184, "Manage brand, material, color and Wi-Fi there.", 2, muted());
    text(24, 228, "Hold BOOT 8 sec while running to reset Wi-Fi.", 2, muted());
    text(24, 264, "Release before restarting or reconnecting power.", 2, muted());
    text(24, 308, current.pin.isEmpty() ? "Follow the setup page to choose a device PIN." : "Pairing PIN: " + current.pin, 2, ink());
    button(24, 390, 180, "Back");
  } else {
    display.fillRoundRect(24, 76, 400, 190, 12, rgb(0x173f37));
    const bool net = std::isfinite(current.remaining);
    text(44, 96, net ? "REMAINING FILAMENT" : "GROSS SPOOL WEIGHT", 2, rgb(0xc5ddce));
    text(44, 136, grams(net ? current.remaining : current.gross), 6, rgb(0xf4f6ed), 10);
    text(44, 214, !current.calibrated ? "Calibration needed" : current.stable ? "Stable" : "Settling", 2, rgb(0xc5ddce));
    display.fillRoundRect(440, 76, 336, 190, 12, surface());
    uint32_t swatch = strtoul(current.color.c_str() + (current.color.startsWith("#") ? 1 : 0), nullptr, 16);
    display.fillCircle(466, 103, 10, rgb(swatch));
    text(488, 96, current.material.isEmpty() ? "NO SPOOL" : current.material, 2, muted(), 22);
    text(458, 132, current.name.isEmpty() ? "Select a spool" : current.name, 2, ink(), 25);
    text(458, 164, current.brand, 2, muted(), 25);
    button(458, 206, 140, "Previous", true, 44);
    button(614, 206, 144, "Next", true, 44);
    button(24, 282, 176, "Read tag");
    button(216, 282, 176, "Write tag", current.canWrite);
    button(408, 282, 176, "Save weight", current.canSave);
    button(600, 282, 176, "Spoolman", current.canSync);
    button(24, 348, 176, "Zero scale");
    text(222, 352, current.tagUid.isEmpty() ? "No NFC tag aligned" : current.tagType + "  " + current.tagUid, 2, muted(), 45);
    text(222, 380, "Gross: " + grams(current.gross), 2, muted());
    text(24, 416, current.message, 2, ink(), 62);
    if (current.message.length() > 62) text(24, 442, current.message.substring(62), 2, ink(), 62);
    else if (!touchReady) text(24, 442, "Touch not detected - use the web interface.", 2, muted());
  }
  if (confirmOpen) {
    display.fillRoundRect(88, 84, 624, 320, 14, surface());
    display.drawRoundRect(88, 84, 624, 320, 14, ink());
    const char *title = pending.action == Action::Write ? "Write this inventory tag?" : pending.action == Action::Sync ? "Send weight to Spoolman?" : "Zero the empty cradle?";
    text(112, 108, title, 2, ink(), 48);
    text(112, 158, pending.action == Action::Zero ? "Remove the spool first." : current.name, 2, ink(), 47);
    text(112, 196, pending.action == Action::Write ? "UID: " + pending.expectedUid : pending.action == Action::Sync ? "Remaining: " + grams(current.remaining) : "Keep reader and cables installed.", 2, muted(), 47);
    text(112, 234, pending.action == Action::Write ? "Keep the tag aligned until verified." : "Wait until the scale is still.", 2, muted(), 47);
    button(112, 316, 276, "Cancel"); button(412, 316, 276, "Confirm");
  }
  display.flush();
}

bool readRegister(uint16_t reg, uint8_t *out, size_t count) {
  Wire.beginTransmission(touchAddress); Wire.write(reg >> 8); Wire.write(reg & 255);
  if (Wire.endTransmission(false)) return false;
  if (Wire.requestFrom(touchAddress, uint8_t(count)) != count) return false;
  for (size_t i = 0; i < count; ++i) out[i] = Wire.read();
  return true;
}
bool clearTouch() {
  Wire.beginTransmission(touchAddress); Wire.write(0x81); Wire.write(0x4e); Wire.write(0);
  return Wire.endTransmission() == 0;
}
Command command(Action action) { return {action, current.spoolId, current.tagUid}; }
void confirm(Action action) { pending = command(action); confirmOpen = true; draw(); }
}

bool begin() {
  pinMode(2, OUTPUT); digitalWrite(2, LOW);
  if (!psramFound()) return false;
  screenReady = display.begin();
  if (!screenReady) return false;
  display.setTextWrap(false);
  if (settings.begin("touch_ui", false)) dark = settings.getBool("dark", false);
  Wire.begin(19, 20, 400000); Wire.setTimeOut(25);
  pinMode(38, OUTPUT); digitalWrite(38, LOW); delay(10); digitalWrite(38, HIGH); delay(60);
  for (uint8_t address : {uint8_t(0x5d), uint8_t(0x14)}) {
    touchAddress = address; uint8_t product[4];
    if (readRegister(0x8140, product, sizeof(product)) && product[0] == '9' && product[1] == '1' && product[2] == '1') { touchReady = true; break; }
  }
  draw(); digitalWrite(2, HIGH); return true;
}
bool ready() { return screenReady; }
void render(const State &state) {
  if (confirmOpen && (state.spoolId != pending.spoolId ||
      (pending.action == Action::Write && state.tagUid != pending.expectedUid))) {
    confirmOpen = false; pending = {};
  }
  current = state;
  String fingerprint = state.spoolId + state.name + state.brand + state.material + state.color + state.tagUid + state.tagType + state.ip + state.message + state.pin + grams(state.gross) + grams(state.remaining) + String(state.stable) + String(state.calibrated) + String(state.canWrite) + String(state.canSave) + String(state.canSync);
  if (fingerprint != lastFrame) { lastFrame = fingerprint; draw(); }
}
Command poll() {
  if (!touchReady || int32_t(millis() - nextTouch) < 0) return {};
  nextTouch = millis() + 35;
  uint8_t status;
  if (!readRegister(0x814e, &status, 1) || !(status & 0x80)) return {};
  if ((status & 0x0f) != 1) { clearTouch(); pressed = false; return {}; }
  uint8_t point[5];
  const bool valid = readRegister(0x814f, point, sizeof(point));
  clearTouch();
  if (!valid || pressed) return {};
  pressed = true;
  int x = point[1] | (point[2] << 8), y = point[3] | (point[4] << 8);
  if (x >= 800 || y >= 480) return {};
  if (confirmOpen) {
    if (inside(x, y, 112, 316, 276)) { confirmOpen = false; pending = {}; draw(); }
    else if (inside(x, y, 412, 316, 276)) { Command result = pending; confirmOpen = false; pending = {}; draw(); return result; }
    return {};
  }
  if (inside(x, y, 646, 12, 130, 44)) { dark = !dark; settings.putBool("dark", dark); draw(); return {}; }
  if (inside(x, y, 510, 12, 120, 44)) { infoOpen = !infoOpen; draw(); return {}; }
  if (infoOpen) { if (inside(x, y, 24, 390, 180)) { infoOpen = false; draw(); } return {}; }
  if (inside(x, y, 458, 206, 140, 44)) return command(Action::Previous);
  if (inside(x, y, 614, 206, 144, 44)) return command(Action::Next);
  if (inside(x, y, 24, 282, 176)) return command(Action::Read);
  if (inside(x, y, 216, 282, 176) && current.canWrite) confirm(Action::Write);
  if (inside(x, y, 408, 282, 176) && current.canSave) return command(Action::Save);
  if (inside(x, y, 600, 282, 176) && current.canSync) confirm(Action::Sync);
  if (inside(x, y, 24, 348, 176)) confirm(Action::Zero);
  return {};
}
}
#endif
