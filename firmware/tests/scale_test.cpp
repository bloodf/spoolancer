#include "Scale.h"
#include "Pins.h"
#include <cassert>
#include <cmath>
#include <cstdio>

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
Scale calibratedScale() {
  Scale scale; scale.begin(); scale.zeroSet = true; scale.countsPerGram = 500;
  window(scale, 0);
  return scale;
}
void responseTests() {
  Scale scale = calibratedScale();
  assert(scale.stable());
  unsigned settledMs = 0;
  // Legacy min/max window cannot be stable until all 20 samples are replaced.
  for (unsigned step = 1; step <= 20; ++step) {
    sample(scale, 500000);
    if (step == 2) {
      assert(!scale.stable());
      assert(std::abs(scale.gross() - 1000) < .01);
    }
    if (!settledMs && scale.stable() && scale.gross() > 999) settledMs = step * 100;
  }
  assert(settledMs <= 800);
  std::printf("Scale load step at 10 SPS: stable in %u ms (legacy window: 2000 ms)\n", settledMs);
  for (unsigned step = 1; step <= 8; ++step) {
    sample(scale, 0);
    if (step == 2) assert(!scale.stable() && std::abs(scale.gross()) < .01);
  }
  assert(scale.stable() && std::abs(scale.gross()) < .01);

  // Resting noise and a single electrical spike do not move the reading 40 g.
  for (int step = 0; step < 100; ++step) {
    const int32_t noise[] = {-400, 250, 100, -100, 350, -200};
    sample(scale, step == 30 ? 20000 : noise[step % 6]);
    assert(scale.stable() && std::abs(scale.gross()) < 1);
    if (step == 30) assert(!scale.tareReady() && !scale.tare());
  }
  assert(scale.tareReady());

  // Stable is not a frozen display: even a 2 g adjustment must be visible.
  for (int step = 0; step < 20; ++step) sample(scale, 1000);
  assert(scale.stable() && std::abs(scale.gross() - 2) < .1);

  unsigned unstableSamples = 0;
  for (int step = 1; step <= 40; ++step) {
    sample(scale, 1000 + step * 250);
    if (!scale.stable()) ++unstableSamples;
  }
  assert(unstableSamples > 20);
  assert(scale.gross() > 19 && scale.gross() < 22);

  // Neither a paused driver nor a restarted stream may reuse old stability.
  now += 600;
  assert(!scale.fresh() && !scale.stable() && std::isnan(scale.gross()));
  sample(scale, 500000);
  assert(!scale.fresh());
  sample(scale, 500000); sample(scale, 500000);
  assert(scale.fresh() && !scale.stable() && !scale.tareReady());
  sample(scale, 500000, 500);
  assert(!scale.stable());

  // Fast-rate converters must still provide 500 ms of steady evidence.
  Scale fast = calibratedScale(); fast.resetWindow();
  for (int i = 0; i < 30; ++i) sample(fast, 500000, 12);
  assert(fast.fresh() && !fast.stable());
  for (int i = 0; i < 20; ++i) sample(fast, 500000, 12);
  assert(fast.stable());

  // Calibration uses all raw samples, not the median or smoothed display.
  Scale reference; reference.begin(); reference.zeroSet = true; reference.zero = 100000;
  for (int i = 0; i < 20; ++i) sample(reference, 600000 + (i % 2 ? 1000 : -1000));
  assert(reference.mean() == 600000 && reference.calibrationReady());
  assert(reference.calibrate(1000) && reference.countsPerGram == 500);
  window(reference, 600000);
  sample(reference, 620000);
  assert(reference.stable() && !reference.calibrationReady());
  assert(!reference.calibrate(1000) && reference.countsPerGram == 500);

  Scale wrap = calibratedScale(); wrap.resetWindow();
  now = UINT32_MAX - 300;
  for (int i = 0; i < 8; ++i) sample(wrap, 0);
  assert(wrap.fresh() && wrap.stable());
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
  sample(scale, 25000);
  assert(!scale.fresh());
  sample(scale, 25000);
  assert(scale.fresh() && !scale.stable() && !scale.tareReady());
  assert(!scale.calibrate(1000));
  for (int i = 0; i < 5; ++i) sample(scale, 25000);
  assert(scale.stable() && !scale.tareReady());
  sample(scale, 40000);
  assert(scale.stable() && !scale.tare());
  sample(scale, 40000);
  assert(!scale.stable());
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
  assert(recalibrated.mean() == 101000 && std::abs(recalibrated.gross() - 2) < .01);
  window(recalibrated, -400000);
  assert(recalibrated.calibrate(1000) && recalibrated.countsPerGram == -500);
  assert(!recalibrated.fresh());
  window(recalibrated, -25000);
  assert(recalibrated.stable() && recalibrated.gross() == 250);
  responseTests();
}
