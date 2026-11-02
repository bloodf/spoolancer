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
  const uint32_t now = millis(), elapsed = now - lastSample;
  updateFilter(now, elapsed);
  lastSample = now;
}
void Scale::resetWindow() {
  used = cursor = 0;
  filtered = NAN;
  trackingSteady = settled = false;
}
void Scale::updateFilter(uint32_t now, uint32_t elapsed) {
  if (used < 3) return;
  int32_t recent[3];
  for (size_t i = 0; i < 3; ++i) recent[i] = samples[(cursor + count - 1 - i) % count];
  std::sort(recent, recent + 3);
  const double value = recent[1];
  const double unit = calibrated() ? std::abs(countsPerGram) : 800.0;
  // Reject isolated spikes, but follow a real load change after two samples.
  if (!std::isfinite(filtered) || std::abs(value - filtered) > 3 * unit) filtered = value;
  else filtered += (value - filtered) * (double(elapsed) / (300.0 + elapsed));

  // A fixed anchor catches slow drift; separate entry/exit bands avoid chatter.
  // The displayed value keeps updating even while the stable flag is held.
  const double band = (settled ? 3 : 2) * unit;
  if (!trackingSteady || std::abs(value - steadyAnchor) > band) {
    steadyAnchor = value; steadySince = now;
    trackingSteady = true; settled = false; steadySamples = 1;
  } else {
    if (steadySamples < 4) ++steadySamples;
    if (!settled && steadySamples >= 4 && uint32_t(now - steadySince) >= 500) settled = true;
  }
}
bool Scale::fresh() const { return used >= 3 && uint32_t(millis() - lastSample) < 600; }
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
bool Scale::rawStable() const { return used == count && fresh() && range() <= 4000; }
bool Scale::stable() const { return calibrated() ? fresh() && settled : rawStable(); }
bool Scale::tareReady() const {
  return used == count && fresh() && (calibrated() ? range() / std::abs(countsPerGram) <= 5 : rawStable());
}
bool Scale::calibrationReady() const {
  if (!zeroSet || used != count || !fresh()) return false;
  const double delta = mean() - zero;
  return std::abs(delta) >= 1000 && range() <= std::max(500.0, std::abs(delta) * .005);
}
double Scale::gross() const { return calibrated() && fresh() ? (filtered - zero) / countsPerGram : NAN; }
bool Scale::tare() {
  if (!tareReady()) return false;
  zero = mean(); zeroSet = true; resetWindow(); return true;
}
bool Scale::calibrate(double reference) {
  if (!calibrationReady() || !Domain::inRange(reference, 100, 2000)) return false;
  const double delta = mean() - zero;
  const double candidate = delta / reference;
  if (!std::isfinite(candidate) || std::abs(candidate) < 1) return false;
  countsPerGram = candidate; resetWindow(); return true;
}
