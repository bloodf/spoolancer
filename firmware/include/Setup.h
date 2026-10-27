#pragma once
#include <cstdint>
#include <cstring>

namespace Setup {
constexpr uint32_t apDuration = 10UL * 60 * 1000;
constexpr uint32_t enrollmentDuration = 60UL * 1000;
constexpr uint32_t connectedGrace = 60UL * 1000;
inline uint32_t remaining(uint32_t deadline, uint32_t now) {
  return deadline && int32_t(deadline - now) > 0 ? deadline - now : 0;
}
inline bool validPin(const char *pin) {
  if (!pin || std::strlen(pin) != 8) return false;
  for (unsigned i = 0; i < 8; ++i) if (pin[i] < '0' || pin[i] > '9') return false;
  return true;
}
inline bool trustedHeaders(const char *host, const char *origin) {
  return host && (!std::strcmp(host, "192.168.4.1") || !std::strcmp(host, "192.168.4.1:80")) &&
         origin && (!*origin || !std::strcmp(origin, "http://192.168.4.1") || !std::strcmp(origin, "http://192.168.4.1:80"));
}
struct Window {
  uint32_t apDeadline = 0, enrollmentDeadline = 0, connectedAt = 0;
  bool connected = false;
  void begin(uint32_t now) { apDeadline = now + apDuration; enrollmentDeadline = 0; connected = false; }
  void close() { apDeadline = enrollmentDeadline = 0; connected = false; }
  void observeConnection(bool value, uint32_t now) {
    if (value && !connected) connectedAt = now;
    connected = value;
  }
  bool expired(uint32_t now) const {
    return !remaining(apDeadline, now) || (connected && uint32_t(now - connectedAt) >= connectedGrace);
  }
  uint32_t apRemaining(uint32_t now) const {
    const uint32_t limit = remaining(apDeadline, now);
    if (!connected) return limit;
    const uint32_t grace = remaining(connectedAt + connectedGrace, now);
    return limit < grace ? limit : grace;
  }
  bool armed(uint32_t now) const { return !expired(now) && remaining(enrollmentDeadline, now); }
  bool arm(uint32_t now) {
    if (expired(now)) return false;
    enrollmentDeadline = now + enrollmentDuration; return true;
  }
  bool consume(uint32_t now, bool required, bool viaAp, bool stationDisconnected, bool headersTrusted) {
    if (!required || !viaAp || !stationDisconnected || !headersTrusted || !armed(now)) return false;
    enrollmentDeadline = 0; return true;
  }
};
enum class Gesture { None, ArmEnrollment, ResetWifi };
struct Button {
  uint32_t downAt = 0;
  bool ready = false, down = false, fired = false;
  Gesture update(bool pressed, uint32_t now) {
    if (!ready) {
      ready = !pressed;
      return Gesture::None;
    }
    if (pressed) {
      if (!down) { downAt = now; down = true; fired = false; }
      if (!fired && uint32_t(now - downAt) >= 8000) { fired = true; return Gesture::ResetWifi; }
      return Gesture::None;
    }
    const bool arm = down && !fired && uint32_t(now - downAt) >= 2000;
    down = fired = false;
    return arm ? Gesture::ArmEnrollment : Gesture::None;
  }
};
}
