#include "Scale.h"
#include "Pins.h"
#include <cassert>
#include <cmath>

namespace {
uint32_t now = 100;
uint32_t rawBits = 0;
unsigned readBit = 0;
bool ready = false, clockHigh = false;
void sample(Scale &scale, int32_t raw, uint32_t elapsed = 100) {
  now += elapsed; rawBits = static_cast<uint32_t>(raw) & 0xffffff;
  readBit = 0; ready = true; scale.tick();
}
void window(Scale &scale, int32_t raw) {
  for (int i = 0; i < 20; ++i) sample(scale, raw);
}
}
uint32_t millis() { return now; }
int digitalRead(int pin) {
  assert(pin == Pins::hxData);
  if (!clockHigh) return ready ? LOW : HIGH;
  assert(readBit < 24);
  const int value = (rawBits >> (23 - readBit++)) & 1;
  if (readBit == 24) ready = false;
  return value;
}
void digitalWrite(int pin, int value) {
  assert(pin == Pins::hxClock); clockHigh = value == HIGH;
}
void scaleDriverTests() {
  Scale scale; scale.begin();
  assert(!scale.fresh() && !scale.calibrated());
  scale.tick(); assert(!scale.fresh());
  window(scale, -100000);
  assert(scale.fresh() && scale.stable() && scale.mean() == -100000);
  assert(!scale.calibrate(1000));
  assert(scale.tare() && scale.zero == -100000 && !scale.fresh());
  window(scale, 400000);
  assert(!scale.calibrate(99));
  assert(scale.calibrate(1000) && scale.countsPerGram == 500);
  assert(!scale.fresh());
  window(scale, 25000);
  assert(scale.stable() && scale.gross() == 250);
  sample(scale, 25000, 1000);
  assert(!scale.fresh() && std::isnan(scale.gross()));
  for (int i = 0; i < 18; ++i) sample(scale, 25000);
  assert(!scale.fresh());
  sample(scale, 25000);
  assert(scale.fresh() && scale.stable());
  sample(scale, 40000);
  assert(!scale.stable() && !scale.tare());
  window(scale, 25000);
  sample(scale, 0x7fffff);
  assert(!scale.fresh());
  window(scale, 25000);
  sample(scale, -0x800000);
  assert(!scale.fresh());
  now += 600;
  assert(!scale.fresh() && std::isnan(scale.gross()));

  Scale recalibrated; recalibrated.begin();
  window(recalibrated, 100000);
  assert(recalibrated.tare());
  window(recalibrated, 600000);
  assert(recalibrated.calibrate(1000) && recalibrated.countsPerGram == 500);
  window(recalibrated, 101000);
  assert(!recalibrated.calibrate(2000));
  assert(recalibrated.calibrated() && recalibrated.countsPerGram == 500);
  assert(recalibrated.fresh() && recalibrated.stable());
  assert(recalibrated.mean() == 101000 && recalibrated.gross() == 2);
  window(recalibrated, -400000);
  assert(recalibrated.calibrate(1000) && recalibrated.countsPerGram == -500);
  assert(!recalibrated.fresh());
  window(recalibrated, -25000);
  assert(recalibrated.stable() && recalibrated.gross() == 250);
}
