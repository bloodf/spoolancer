#pragma once
#include "StatusModel.h"

namespace StatusUi {
struct Snapshot : StatusModel::Snapshot {
  const char *material = "", *ip = "";
};
#ifdef SPOOLANCER_STATUS_LEDS
void begin();
void render(const Snapshot &snapshot, uint32_t now, bool force = false);
void start(StatusModel::Operation operation, uint32_t now);
void finish(bool success, uint32_t now);
bool oledAvailable();
#else
inline void begin() {}
inline void render(const Snapshot &, uint32_t, bool = false) {}
inline void start(StatusModel::Operation, uint32_t) {}
inline void finish(bool, uint32_t) {}
inline bool oledAvailable() { return false; }
#endif
}
