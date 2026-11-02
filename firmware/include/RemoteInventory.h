#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace RemoteInventory {
constexpr size_t limit = 200, pageSize = 10;
constexpr uint32_t freshnessMs = 30000, retryMs = 10000;
inline double prefer(double first, double fallback) { return std::isfinite(first) ? first : fallback; }
struct Weights { double initial, empty, remaining; };
inline Weights weights(double initial, double filamentWeight, double empty,
                       double filamentEmpty, double vendorEmpty, double remaining) {
  return {prefer(initial, filamentWeight), prefer(empty, prefer(filamentEmpty, vendorEmpty)), remaining};
}
inline void stableId(uint32_t remoteId, uint32_t prefix, char (&result)[17]) {
  std::snprintf(result, sizeof(result), "%08lx%08lx", static_cast<unsigned long>(prefix),
                static_cast<unsigned long>(remoteId));
}
inline bool visible(bool configured, bool remote, bool listed, int remoteId) {
  return configured ? remote && listed && remoteId > 0 : true;
}
inline bool sameNumber(double a, double b) {
  return (std::isnan(a) && std::isnan(b)) || (std::isfinite(a) && std::isfinite(b) && std::abs(a - b) < .0001);
}
struct Pages {
  size_t count = 0;
  int previousId = 0;
  bool valid = true;
  bool accept(int id) {
    if (!valid || id <= previousId || count >= limit) { valid = false; return false; }
    previousId = id; ++count; return true;
  }
  bool complete(size_t received) const { return valid && received < pageSize; }
};
struct Cache {
  bool loaded = false, successful = false, attempted = false, failed = false;
  uint32_t refreshedAt = 0, attemptedAt = 0;
  bool stale(uint32_t now, bool connected) const {
    return !connected || !successful || failed || uint32_t(now - refreshedAt) >= freshnessMs;
  }
  bool shouldRefresh(uint32_t now, bool connected, bool force) const {
    if (!connected || (attempted && uint32_t(now - attemptedAt) < retryMs)) return false;
    return force || stale(now, connected);
  }
  void begin(uint32_t now) { attempted = true; attemptedAt = now; }
  void finish(uint32_t now, bool ok) {
    failed = !ok;
    if (ok) { loaded = successful = true; refreshedAt = now; }
  }
};
}
