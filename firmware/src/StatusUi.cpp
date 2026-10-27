#include "StatusUi.h"
#ifdef SPOOLANCER_STATUS_LEDS
#if !defined(SPOOLANCER_HEADLESS) || defined(SPOOLANCER_TOUCH)
#error "Status LEDs require the standalone headless or OLED board profile."
#endif
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Pins.h"
#ifdef SPOOLANCER_OLED
#include <Wire.h>
#include <U8g2lib.h>
#endif

#ifndef SPOOLANCER_LED_ORDER
#define SPOOLANCER_LED_ORDER NEO_GRB
#endif
#ifndef SPOOLANCER_LED_BRIGHTNESS
#define SPOOLANCER_LED_BRIGHTNESS 51
#endif
#ifndef SPOOLANCER_OLED_ADDRESS
#define SPOOLANCER_OLED_ADDRESS 0x3c
#endif

namespace StatusUi {
namespace {
static_assert(SPOOLANCER_LED_BRIGHTNESS >= 0 && SPOOLANCER_LED_BRIGHTNESS <= 255, "LED brightness must be 0-255");
Adafruit_NeoPixel pixels(StatusModel::ledCount, Pins::statusLed, SPOOLANCER_LED_ORDER + NEO_KHZ800);
StatusModel::Event event;
uint32_t lastRender = 0, lastOled = 0;
bool screenAvailable = false;
#ifdef SPOOLANCER_OLED
#ifdef SPOOLANCER_OLED_SH1106
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, Pins::oledScl, Pins::oledSda);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, Pins::oledScl, Pins::oledSda);
#endif
void drawScreen(const Snapshot &snapshot, const StatusModel::View &view) {
  // Stop using a disconnected/stuck bus; reboot after repairing the connection.
  Wire.beginTransmission(SPOOLANCER_OLED_ADDRESS);
  if (Wire.endTransmission() != 0) { screenAvailable = false; return; }
  oled.clearBuffer(); oled.setFont(u8g2_font_6x10_tf);
  char line[24];
  snprintf(line, sizeof(line), "%.21s", snapshot.material[0] ? snapshot.material : "Spoolancer");
  oled.drawStr(0, 9, line);
  if (std::isfinite(view.remaining)) snprintf(line, sizeof(line), "%.0f g left", view.remaining);
  else snprintf(line, sizeof(line), "-- g left");
  oled.setFont(u8g2_font_9x18B_tf); oled.drawStr(0, 29, line);
  oled.setFont(u8g2_font_6x10_tf);
  if (snapshot.calibrated && snapshot.fresh && std::isfinite(snapshot.gross)) snprintf(line, sizeof(line), "Gross %.0fg", snapshot.gross);
  else snprintf(line, sizeof(line), "Gross --g");
  oled.drawStr(0, 41, line);
  using Mode = StatusModel::Mode;
  if (view.mode == Mode::Success || view.mode == Mode::Failure) {
    snprintf(line, sizeof(line), "%s %s", StatusModel::operationText(view.operation), view.mode == Mode::Success ? "OK" : "failed");
  } else if (view.mode == Mode::BusyNfc || view.mode == Mode::BusySave || view.mode == Mode::BusySync) {
    snprintf(line, sizeof(line), "%s working", StatusModel::operationText(view.operation));
  } else if (view.mode == Mode::Setup) {
    snprintf(line, sizeof(line), "Wi-Fi setup / browser");
  } else snprintf(line, sizeof(line), "%s", StatusModel::problemText(view.problem));
  oled.drawStr(0, 52, line);
  oled.drawStr(0, 63, snapshot.ip[0] ? snapshot.ip : "Wi-Fi disconnected");
  oled.sendBuffer();
}
#endif
}
void begin() {
  pixels.begin(); pixels.setBrightness(SPOOLANCER_LED_BRIGHTNESS); pixels.clear(); pixels.show();
#ifdef SPOOLANCER_OLED
  Wire.begin(Pins::oledSda, Pins::oledScl); Wire.setTimeOut(10);
  Wire.beginTransmission(SPOOLANCER_OLED_ADDRESS);
  if (Wire.endTransmission() == 0) {
    oled.setI2CAddress(SPOOLANCER_OLED_ADDRESS * 2); oled.setBusClock(100000);
    oled.begin(); screenAvailable = true;
  }
  Serial.printf("Status OLED: %s (address 0x%02x)\n", screenAvailable ? "I2C acknowledged; verify controller and image" : "not found; web remains available", SPOOLANCER_OLED_ADDRESS);
#endif
}
void start(StatusModel::Operation operation, uint32_t now) { event.begin(operation, now); }
void finish(bool success, uint32_t now) { event.finish(success, now); }
bool oledAvailable() { return screenAvailable; }
void render(const Snapshot &snapshot, uint32_t now, bool force) {
  if (!force && uint32_t(now - lastRender) < 50) return;
  lastRender = now;
  const StatusModel::View view = StatusModel::evaluate(snapshot, event, now);
  for (size_t i = 0; i < StatusModel::ledCount; ++i) {
    const auto &color = view.leds[i];
    pixels.setPixelColor(i, pixels.Color(color.red, color.green, color.blue, 0));
  }
  pixels.show();
#ifdef SPOOLANCER_OLED
  if (screenAvailable && (force || uint32_t(now - lastOled) >= 500)) {
    lastOled = now; drawScreen(snapshot, view);
  }
#endif
}
}
#endif
