#pragma once
#include <Arduino.h>

class Scale {
 public:
  void begin();
  void tick();
  void resetWindow();
  bool fresh() const;
  bool stable() const;
  bool rawStable() const;
  bool calibrated() const;
  double mean() const;
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
  double range() const;
};
