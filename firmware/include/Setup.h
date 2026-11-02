#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace Setup {
constexpr uint32_t apDuration = 10UL * 60 * 1000;
constexpr uint32_t setupRetryDelay = 1500;
constexpr uint32_t connectedGrace = 60UL * 1000;
inline uint32_t remaining(uint32_t deadline, uint32_t now) {
  return deadline && int32_t(deadline - now) > 0 ? deadline - now : 0;
}
inline bool validPin(const char *pin) {
  if (!pin || std::strlen(pin) != 8) return false;
  for (unsigned i = 0; i < 8; ++i) if (pin[i] < '0' || pin[i] > '9') return false;
  return true;
}
enum class AuthMode { Unconfigured, Protected, Open, Invalid };
struct StoredAuth {
  bool hasFields = false, fieldsTyped = false, complete = false, pinRequired = true;
  const char *pin = "";
  bool legacyPresent = false;
  const char *legacyPin = "";
};
inline AuthMode authMode(const StoredAuth &stored) {
  if (!stored.hasFields) {
    if (!stored.legacyPresent) return AuthMode::Unconfigured;
    return validPin(stored.legacyPin) ? AuthMode::Protected : AuthMode::Invalid;
  }
  if (!stored.fieldsTyped || !stored.pin) return AuthMode::Invalid;
  if (!stored.complete)
    return stored.pinRequired && !*stored.pin ? AuthMode::Unconfigured : AuthMode::Invalid;
  if (stored.pinRequired) return validPin(stored.pin) ? AuthMode::Protected : AuthMode::Invalid;
  return !*stored.pin ? AuthMode::Open : AuthMode::Invalid;
}
inline bool complete(AuthMode mode) { return mode == AuthMode::Protected || mode == AuthMode::Open; }
inline bool required(AuthMode mode) { return mode == AuthMode::Unconfigured; }
inline bool pinRequired(AuthMode mode) { return mode != AuthMode::Open; }
inline bool validText(const char *value) {
  if (!value) return false;
  const auto *p = reinterpret_cast<const unsigned char *>(value);
  while (*p) {
    if (*p < 0x20 || *p == 0x7f) return false;
    if (*p < 0x80) { ++p; continue; }
    unsigned trailing; uint32_t codepoint, minimum;
    if (*p >= 0xc2 && *p <= 0xdf) { trailing = 1; codepoint = *p & 0x1f; minimum = 0x80; }
    else if (*p >= 0xe0 && *p <= 0xef) { trailing = 2; codepoint = *p & 0x0f; minimum = 0x800; }
    else if (*p >= 0xf0 && *p <= 0xf4) { trailing = 3; codepoint = *p & 0x07; minimum = 0x10000; }
    else return false;
    ++p;
    while (trailing--) {
      if (*p < 0x80 || *p > 0xbf) return false;
      codepoint = (codepoint << 6) | (*p++ & 0x3f);
    }
    if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
  }
  return true;
}
inline bool validSsid(const char *ssid) {
  if (!validText(ssid)) return false;
  const size_t length = std::strlen(ssid);
  if (!length || length > 32) return false;
  for (size_t i = 0; i < length; ++i) if (ssid[i] != ' ') return true;
  return false;
}
inline bool validWifiPassword(const char *password) {
  if (!validText(password)) return false;
  const size_t length = std::strlen(password);
  if (length >= 8 && length <= 63) return true;
  if (length != 64) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = password[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}
inline bool validWifi(const char *ssid, const char *password, bool open) {
  return validSsid(ssid) && password && (open ? !*password : validWifiPassword(password));
}
inline bool resolveWifiPassword(const char *previousSsid, const char *previousPassword,
                                const char *ssid, const char *password, bool openPresent,
                                bool open, const char *&resolved) {
  if (!previousSsid || !previousPassword || !validSsid(ssid) || !password) return false;
  if (openPresent && open) { resolved = ""; return !*password; }
  if (*password) { resolved = password; return validWifiPassword(password); }
  if (std::strcmp(previousSsid, ssid)) return false;
  resolved = previousPassword;
  return *resolved ? validWifiPassword(resolved) : !openPresent;
}
inline bool trustedLocalHeaders(const char *host, const char *origin, const char *localIp,
                                bool allowMdns = true) {
  if (!host || !origin || !localIp || !*localIp || !std::strcmp(localIp, "0.0.0.0")) return false;
  char ipPort[48]; std::snprintf(ipPort, sizeof(ipPort), "%s:80", localIp);
  const bool local = !std::strcmp(host, localIp) || !std::strcmp(host, ipPort);
  const bool mdns = allowMdns && (!std::strcmp(host, "spoolancer.local") || !std::strcmp(host, "spoolancer.local:80"));
  if (!local && !mdns) return false;
  if (!*origin) return true;
  char expected[80]; std::snprintf(expected, sizeof(expected), "http://%s", host);
  if (!std::strcmp(origin, expected)) return true;
  // HTTP's default port is the same origin whether the browser serializes it or omits it.
  const char *name = mdns ? "spoolancer.local" : localIp;
  std::snprintf(expected, sizeof(expected), "http://%s", name);
  if (!std::strcmp(origin, expected)) return true;
  std::snprintf(expected, sizeof(expected), "http://%s:80", name);
  return !std::strcmp(origin, expected);
}
inline bool trustedHeaders(const char *host, const char *origin) {
  return trustedLocalHeaders(host, origin, "192.168.4.1", false);
}
struct Window {
  uint32_t apDeadline = 0, nextAttempt = 0, connectedAt = 0;
  bool attempted = false;
  bool connected = false;
  void begin(uint32_t now) { apDeadline = now + apDuration; nextAttempt = 0; attempted = false; connected = false; }
  void close() { apDeadline = nextAttempt = 0; attempted = false; connected = false; }
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
  bool available(uint32_t now) const {
    return !expired(now) && (!attempted || !remaining(nextAttempt, now));
  }
  bool consume(uint32_t now, bool required, bool viaAp, bool stationDisconnected, bool headersTrusted) {
    if (!required || !viaAp || !stationDisconnected || !headersTrusted || !available(now)) return false;
    nextAttempt = now + setupRetryDelay; attempted = true; return true;
  }
};
enum class Gesture { None, ResetWifi };
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
    down = fired = false;
    return Gesture::None;
  }
};
}
