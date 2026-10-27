#pragma once
#if defined(SPOOLANCER_TOUCH)
#include <Arduino.h>
#include <cmath>

namespace TouchUi {
enum class Action { None, Previous, Next, Read, Write, Save, Sync, Zero };
struct State {
  String spoolId, name, brand, material, color, tagUid, tagType, ip, message, pin;
  double gross = NAN, remaining = NAN;
  bool stable = false, calibrated = false, canWrite = false, canSave = false, canSync = false;
};
struct Command {
  Action action = Action::None;
  String spoolId, expectedUid;
};
bool begin();
void render(const State &state);
Command poll();
bool ready();
}
#endif
