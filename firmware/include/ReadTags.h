#pragma once
// Decode genuine Bambu Lab and Snapmaker U1 filament tags (MIFARE Classic 1K) to identify a
// spool on the scale. These tags are RSA-signed, so they can only be read, never recreated.
// Key derivation and byte layouts mirror the open-source research:
//   Bambu    - Bambu-Research-Group/RFID-Tag-Guide (deriveKeys.py, BambuLabRfid.md)
//   Snapmaker - DnG-Crafts/U1-RFID (fm175xx_reader.py, U1 Tag Protocol.md)
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "Sha256.h"

namespace ReadTags {
struct Decoded {
  bool valid = false;
  char brand[24] = {};     // "Bambu Lab" or the Snapmaker vendor string.
  char material[40] = {};  // Detailed filament type, e.g. "PLA Basic".
  char color[8] = {};      // "#RRGGBB".
  uint16_t weightG = 0;    // Nominal spool weight (0 if absent).
  uint16_t lengthM = 0;    // Filament length in metres (0 if absent).
};

// ---------- Bambu Lab: HKDF-SHA256 over the 4-byte UID ----------
constexpr uint8_t bambuMaster[16] = {0x9a, 0x75, 0x9c, 0xf2, 0xc4, 0xf7, 0xca, 0xff,
                                     0x22, 0x2c, 0xb9, 0x76, 0x9b, 0x41, 0xbc, 0x96};
// Fills keys[16][6] with Key A for every sector.
inline void bambuKeys(const uint8_t uid[4], uint8_t keys[16][6]) {
  uint8_t okm[96];
  const uint8_t info[] = {'R', 'F', 'I', 'D', '-', 'A', 0}; // Context includes the trailing NUL.
  Crypto::hkdfSha256(uid, 4, bambuMaster, sizeof(bambuMaster), info, sizeof(info), okm, sizeof(okm));
  for (int s = 0; s < 16; ++s) std::memcpy(keys[s], okm + s * 6, 6);
}

// ---------- Snapmaker U1: per-sector HMAC-SHA256 ----------
constexpr char snapmakerSaltA[] = "Snapmaker_qwertyuiop[,.;]";
// Key A for one sector. prk = HMAC(salt, uid); key = HMAC(prk, "key_a_<sector>" + 0x01)[:6].
inline void snapmakerKey(const uint8_t uid[4], int sector, uint8_t key[6]) {
  uint8_t prk[32];
  Crypto::hmacSha256(reinterpret_cast<const uint8_t *>(snapmakerSaltA), sizeof(snapmakerSaltA) - 1, uid, 4, prk);
  char info[16];
  const int n = std::snprintf(info, sizeof(info), "key_a_%d", sector);
  uint8_t message[20];
  std::memcpy(message, info, n); message[n] = 0x01;
  uint8_t out[32];
  Crypto::hmacSha256(prk, 32, message, n + 1, out);
  std::memcpy(key, out, 6);
}

// ---------- Decoders (data is the 1024-byte linear dump, 64 blocks of 16) ----------
inline void copyTrimmed(char *dst, size_t cap, const uint8_t *src, size_t len) {
  size_t n = 0;
  for (size_t i = 0; i < len && n + 1 < cap; ++i) {
    if (src[i] == 0) break;
    if (src[i] >= 0x20 && src[i] < 0x7f) dst[n++] = char(src[i]);
  }
  dst[n] = 0;
}
inline void hexColor(char out[8], const uint8_t rgb[3]) {
  std::snprintf(out, 8, "#%02X%02X%02X", rgb[0], rgb[1], rgb[2]);
}
inline uint16_t le16(const uint8_t *p) { return uint16_t(p[0] | (p[1] << 8)); }

// Bambu: block 2 filament type, block 4 detailed type, block 5 color+weight, block 14 length.
inline Decoded decodeBambu(const uint8_t *data) {
  Decoded d;
  const uint8_t *b4 = data + 4 * 16, *b5 = data + 5 * 16, *b14 = data + 14 * 16, *b2 = data + 2 * 16;
  if (b4[0] < 0x20 || b4[0] >= 0x7f) copyTrimmed(d.material, sizeof(d.material), b2, 16);
  else copyTrimmed(d.material, sizeof(d.material), b4, 16);
  if (!d.material[0]) return d; // A genuine tag always names its filament.
  std::strncpy(d.brand, "Bambu Lab", sizeof(d.brand) - 1);
  hexColor(d.color, b5);        // RGBA, A ignored.
  d.weightG = le16(b5 + 4);
  d.lengthM = le16(b14 + 4);
  d.valid = true;
  return d;
}

constexpr const char *snapmakerMain[] = {"", "PLA", "PETG", "ABS", "TPU", "PVA"};
constexpr const char *snapmakerSub[] = {"", "Basic", "Matte", "SnapSpeed", "Silk", "Support", "HF", "95A", "95A HF"};
// Snapmaker: sector 1 = version/type/color, sector 2 = diameter/weight/length.
inline Decoded decodeSnapmaker(const uint8_t *data) {
  Decoded d;
  const uint8_t *s1b0 = data + 64 + 0, *s1b1 = data + 64 + 16, *s2b0 = data + 128 + 0;
  const uint16_t main = le16(s1b0 + 2), sub = le16(s1b0 + 4);
  if (main == 0 || main >= sizeof(snapmakerMain) / sizeof(*snapmakerMain)) return d;
  const char *mainName = snapmakerMain[main];
  const char *subName = sub < sizeof(snapmakerSub) / sizeof(*snapmakerSub) ? snapmakerSub[sub] : "";
  if (subName[0]) std::snprintf(d.material, sizeof(d.material), "%s %s", mainName, subName);
  else std::strncpy(d.material, mainName, sizeof(d.material) - 1);
  copyTrimmed(d.brand, sizeof(d.brand), data + 16, 16); // Sector 0 vendor name.
  if (!d.brand[0]) std::strncpy(d.brand, "Snapmaker", sizeof(d.brand) - 1);
  hexColor(d.color, s1b1); // RGB_1.
  d.weightG = le16(s2b0 + 2);
  d.lengthM = le16(s2b0 + 4);
  d.valid = true;
  return d;
}
}  // namespace ReadTags
