#include "Scale.h"
#include "Pins.h"
#include "Domain.h"
#include <algorithm>

static portMUX_TYPE scaleMux = portMUX_INITIALIZER_UNLOCKED;
void Scale::begin() {
  pinMode(Pins::hxData, INPUT_PULLUP);
  pinMode(Pins::hxClock, OUTPUT);
  digitalWrite(Pins::hxClock, LOW);
}
void Scale::tick() {
  if (digitalRead(Pins::hxData) != LOW) return;
  if (used && uint32_t(millis() - lastSample) >= 600) resetWindow();
  uint32_t value = 0;
  // Keep every high pulse below the HX711 power-down threshold (60 us).
  portENTER_CRITICAL(&scaleMux);
  for (int i = 0; i < 24; ++i) {
    digitalWrite(Pins::hxClock, HIGH); delayMicroseconds(1);
    value = (value << 1) | digitalRead(Pins::hxData);
    digitalWrite(Pins::hxClock, LOW); delayMicroseconds(1);
  }
  digitalWrite(Pins::hxClock, HIGH); delayMicroseconds(1);
  digitalWrite(Pins::hxClock, LOW);
  portEXIT_CRITICAL(&scaleMux);
  if (value == 0x7fffff || value == 0x800000) { resetWindow(); return; }
  if (value & 0x800000) value |= 0xff000000;
  samples[cursor] = static_cast<int32_t>(value);
  cursor = (cursor + 1) % count;
  used = std::min(used + 1, count);
  lastSample = millis();
}
void Scale::resetWindow() { used = cursor = 0; }
bool Scale::fresh() const { return used == count && uint32_t(millis() - lastSample) < 600; }
double Scale::mean() const {
  if (!used) return NAN;
  double sum = 0;
  for (size_t i = 0; i < used; ++i) sum += samples[i];
  return sum / used;
}
double Scale::range() const {
  if (!used) return INFINITY;
  int32_t low = samples[0], high = samples[0];
  for (size_t i = 1; i < used; ++i) {
    low = std::min(low, samples[i]); high = std::max(high, samples[i]);
  }
  return double(high) - low;
}
bool Scale::calibrated() const { return zeroSet && std::isfinite(countsPerGram) && std::abs(countsPerGram) >= 1; }
bool Scale::rawStable() const { return fresh() && range() <= 4000; }
bool Scale::stable() const { return calibrated() ? fresh() && range() / std::abs(countsPerGram) <= 5 : rawStable(); }
double Scale::gross() const { return calibrated() && fresh() ? (mean() - zero) / countsPerGram : NAN; }
bool Scale::tare() {
  if (!stable()) return false;
  zero = mean(); zeroSet = true; resetWindow(); return true;
}
bool Scale::calibrate(double reference) {
  if (!zeroSet || !fresh() || !Domain::inRange(reference, 100, 2000)) return false;
  const double delta = mean() - zero;
  if (std::abs(delta) < 1000 || range() > std::max(500.0, std::abs(delta) * .005)) return false;
  const double candidate = delta / reference;
  if (!std::isfinite(candidate) || std::abs(candidate) < 1) return false;
  countsPerGram = candidate; resetWindow(); return true;
}
