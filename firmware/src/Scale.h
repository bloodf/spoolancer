#pragma once
#include <Arduino.h>
#include <cmath>

class Scale {
 public:
  void begin();
  void tick();
  void resetWindow();
  bool fresh() const;
  bool stable() const;
  bool rawStable() const;
  bool tareReady() const;
  bool calibrationReady() const;
  bool calibrated() const;
  double mean() const;
  size_t sampleCount() const { return used; }
  double gross() const;
  bool tare();
  bool calibrate(double reference);
  double zero = 0, countsPerGram = 0;
  bool zeroSet = false;
 private:
  static constexpr size_t count = 20;
  int32_t samples[count] = {};
  size_t used = 0, cursor = 0;
  uint32_t lastSample = 0;
  double filtered = NAN, steadyAnchor = 0;
  uint32_t steadySince = 0;
  uint8_t steadySamples = 0;
  bool trackingSteady = false, settled = false;
  void updateFilter(uint32_t now, uint32_t elapsed);
  double range() const;
};
