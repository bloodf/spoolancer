#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Domain {
inline bool validColor(const char *s) {
  if (!s || std::strlen(s) != 7 || s[0] != '#') return false;
  for (size_t i = 1; i < 7; ++i)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f') ||
          (s[i] >= 'A' && s[i] <= 'F'))) return false;
  return true;
}
inline bool validId(const char *s) {
  if (!s || std::strlen(s) != 16) return false;
  for (size_t i = 0; i < 16; ++i)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
  return true;
}
inline bool inRange(double n, double low, double high) {
  return std::isfinite(n) && n >= low && n <= high;
}
inline bool measurementAllowed(double gross, double empty, double initial,
                               bool calibrated, bool stable, bool fresh) {
  return calibrated && stable && fresh && inRange(gross, 20, 2000) &&
         inRange(empty, 0, 2000) && inRange(initial, 1, 5000) &&
         gross >= empty && gross - empty <= initial;
}
inline bool ntag216Version(const uint8_t *v, size_t n) {
  const uint8_t expected[] = {0, 4, 4, 2, 1, 0, 0x13, 3};
  return n == sizeof(expected) && std::memcmp(v, expected, n) == 0;
}
inline bool ntag216Unlocked(const uint8_t *page2, const uint8_t *page226) {
  // No static/dynamic locks, read/write CC, no password-protected pages or mirroring.
  return page2[2] == 0 && page2[3] == 0 && page2[4] == 0xe1 &&
         page2[5] == 0x10 && page2[6] == 0x6d && page2[7] == 0 &&
         page226[0] == 0 && page226[1] == 0 && page226[2] == 0 &&
         (page226[4] & 0xc0) == 0 && page226[7] == 0xff;
}
constexpr const char *ndefMime = "application/vnd.spoolancer+json";
constexpr size_t maxPayload = 650;
// NDEF MIME record with a 32-bit payload length, inside a Type 2 extended TLV.
inline size_t encodeNdef(const uint8_t *payload, size_t len, uint8_t *out, size_t cap) {
  const size_t typeLen = std::strlen(ndefMime), recordLen = 6 + typeLen + len;
  const size_t total = 4 + recordLen + 1;
  if (!len || len > maxPayload || cap < total) return 0;
  out[0] = 3; out[1] = 0xff; out[2] = recordLen >> 8; out[3] = recordLen;
  out[4] = 0xc2; out[5] = typeLen;
  out[6] = 0; out[7] = 0; out[8] = len >> 8; out[9] = len;
  std::memcpy(out + 10, ndefMime, typeLen);
  std::memcpy(out + 10 + typeLen, payload, len);
  out[total - 1] = 0xfe;
  return total;
}
inline bool decodeNdef(const uint8_t *data, size_t count, size_t &offset, size_t &length) {
  if (count < 10 || data[0] != 3 || data[1] != 0xff || data[4] != 0xc2) return false;
  const size_t typeLen = std::strlen(ndefMime);
  const size_t recordLen = (size_t(data[2]) << 8) | data[3];
  length = (size_t(data[6]) << 24) | (size_t(data[7]) << 16) |
           (size_t(data[8]) << 8) | data[9];
  offset = 10 + typeLen;
  return data[5] == typeLen && length > 0 && length <= maxPayload &&
         recordLen == 6 + typeLen + length && count >= 4 + recordLen + 1 &&
         std::memcmp(data + 10, ndefMime, typeLen) == 0 &&
         data[4 + recordLen] == 0xfe;
}
}
