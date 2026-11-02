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
struct NtagLayout {
  uint8_t ccSize = 0, lockPage = 0;
  const char *name = nullptr;
  size_t capacity() const { return size_t(ccSize) * 8; }
};
inline NtagLayout ntagLayout(const uint8_t *v, size_t n) {
  // NXP GET_VERSION, including vendor, product, silicon revision and protocol.
  const uint8_t prefix[] = {0, 4, 4, 2, 1, 0};
  if (n != 8 || std::memcmp(v, prefix, sizeof(prefix)) != 0 || v[7] != 3) return {};
  switch (v[6]) {
    case 0x0f: return {0x12, 40, "NTAG213"};
    case 0x11: return {0x3e, 130, "NTAG215"};
    case 0x13: return {0x6d, 226, "NTAG216"};
    default: return {};
  }
}
inline bool ntagContainer(const NtagLayout &tag, const uint8_t *page2) {
  return tag.capacity() && page2[4] == 0xe1 && page2[5] == 0x10 &&
         page2[6] == tag.ccSize && (page2[7] == 0 || page2[7] == 0x0f);
}
inline bool ntagUnlocked(const NtagLayout &tag, const uint8_t *page2, const uint8_t *locks) {
  // READ at lockPage includes dynamic locks, CFG0 and CFG1. Never modify them.
  return ntagContainer(tag, page2) && page2[2] == 0 && page2[3] == 0 && page2[7] == 0 &&
         locks[0] == 0 && locks[1] == 0 && locks[2] == 0 &&
         (locks[4] & 0xc0) == 0 && locks[7] == 0xff && (locks[8] & 0xc0) == 0;
}
constexpr const char *ndefMime = "application/vnd.spoolancer+json";
constexpr size_t maxPayload = 650;
// Bytes a single MIME record needs inside a Type 2 NDEF TLV, including the terminator.
// Short forms are used when they fit: SR record for payloads up to 255 bytes and the
// one-byte TLV length below 0xFF, as NFC Forum Type 2 readers (phones) require.
inline size_t ndefSize(size_t typeLen, size_t len) {
  const size_t recordLen = (len <= 255 ? 3 : 6) + typeLen + len;
  return (recordLen < 0xff ? 2 : 4) + recordLen + 1;
}
inline size_t encodeNdef(const uint8_t *payload, size_t len, uint8_t *out, size_t cap, const char *type = ndefMime) {
  const size_t typeLen = std::strlen(type), total = ndefSize(typeLen, len);
  if (!len || len > maxPayload || typeLen > 255 || cap < total) return 0;
  const bool shortRecord = len <= 255;
  const size_t recordLen = (shortRecord ? 3 : 6) + typeLen + len;
  size_t at = 0;
  out[at++] = 3;
  if (recordLen < 0xff) out[at++] = uint8_t(recordLen);
  else { out[at++] = 0xff; out[at++] = uint8_t(recordLen >> 8); out[at++] = uint8_t(recordLen); }
  out[at++] = shortRecord ? 0xd2 : 0xc2; // MB|ME(|SR), TNF = MIME media.
  out[at++] = uint8_t(typeLen);
  if (shortRecord) out[at++] = uint8_t(len);
  else { out[at++] = 0; out[at++] = 0; out[at++] = uint8_t(len >> 8); out[at++] = uint8_t(len); }
  std::memcpy(out + at, type, typeLen); at += typeLen;
  std::memcpy(out + at, payload, len); at += len;
  out[at++] = 0xfe;
  return at;
}
// Reads an NDEF TLV header: value length and header size. Accepts the three-byte length
// form for any value so records written by earlier firmware remain readable.
inline bool ndefTlv(const uint8_t *data, size_t count, size_t &header, size_t &length) {
  if (count < 2 || data[0] != 3) return false;
  if (data[1] != 0xff) { header = 2; length = data[1]; return true; }
  if (count < 4) return false;
  header = 4; length = (size_t(data[2]) << 8) | data[3];
  return true;
}
inline bool decodeNdef(const uint8_t *data, size_t count, size_t &offset, size_t &length, const char *type = ndefMime) {
  size_t header = 0, recordLen = 0;
  if (!ndefTlv(data, count, header, recordLen) || recordLen < 3 || count < header + recordLen + 1 ||
      data[header + recordLen] != 0xfe) return false;
  const uint8_t *record = data + header, flags = record[0];
  if ((flags & ~0x10) != 0xc2) return false; // Exactly one MIME record, no ID, not chunked.
  const bool shortRecord = flags & 0x10;
  const size_t typeLen = std::strlen(type), fixed = shortRecord ? 3 : 6;
  if (recordLen < fixed || record[1] != typeLen) return false;
  length = shortRecord ? record[2] : (size_t(record[2]) << 24) | (size_t(record[3]) << 16) |
                                     (size_t(record[4]) << 8) | record[5];
  offset = header + fixed + typeLen;
  return length > 0 && length <= maxPayload && recordLen == fixed + typeLen + length &&
         std::memcmp(record + fixed, type, typeLen) == 0;
}
}
