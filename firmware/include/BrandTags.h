#pragma once
// Printer-ecosystem tag encoders. Layouts mirror the open-source writers that stock
// printer firmware is known to accept:
//   Creality CFS  - DnG-Crafts/K2-RFID (MIFARE Classic 1K, sector 1, AES-128-ECB)
//   Qidi Box      - DnG-Crafts/QIDI-RFID, QIDI_Q2 officiall_filas_list.cfg (Classic 1K block 4)
//   Anycubic ACE  - DnG-Crafts/ACE-RFID (NTAG21x raw pages 4-31, little-endian)
//   Elegoo Canvas - DnG-Crafts/ELG-RFID, Savion/elegoo-rfid-editor (NTAG21x pages 16-24)
//   OpenSpool     - spuder/OpenSpool (NTAG215/216 NDEF application/json)
// Bambu Lab and stock Snapmaker U1 tags carry RSA signatures and cannot be created.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace BrandTags {
enum class Format : uint8_t { Spoolancer, Creality, Qidi, Anycubic, Elegoo, OpenSpool, Count };
enum class Chip : uint8_t { Ntag, Classic };

struct FormatInfo { const char *id, *label; Chip chip; };
constexpr FormatInfo formats[] = {
  {"spoolancer", "Spoolancer inventory", Chip::Ntag},
  {"creality", "Creality CFS", Chip::Classic},
  {"qidi", "Qidi Box", Chip::Classic},
  {"anycubic", "Anycubic ACE", Chip::Ntag},
  {"elegoo", "Elegoo Canvas", Chip::Ntag},
  {"openspool", "OpenSpool", Chip::Ntag},
};
inline const FormatInfo &info(Format f) { return formats[size_t(f)]; }
inline bool parseFormat(const char *id, Format &out) {
  if (!id) return false;
  for (size_t i = 0; i < size_t(Format::Count); ++i)
    if (std::strcmp(id, formats[i].id) == 0) { out = Format(i); return true; }
  return false;
}

// One row per material family. Empty strings / zero codes mean the ecosystem has no profile.
struct Profile {
  const char *name;
  const char *creality;        // 5-digit material_database.json id
  uint8_t qidi;                // officiall_filas_list.cfg code
  const char *anycubicType, *anycubicSku;
  const char *elegooType;      // material name encoded on page 18
  int8_t elegooFamily, elegooSub;
  uint16_t nozzleMin, nozzleMax, bedMin, bedMax;
  const char *openspoolType, *openspoolSubtype;
};
// Temperatures follow the ACE-RFID filament database where Anycubic has a profile, except ABS,
// which keeps the generic 240-280 C range used by Creality and Elegoo.
constexpr Profile profiles[] = {
  {"PLA", "00001", 1, "PLA", "AHPLBK-101", "PLA", 0, 0, 190, 230, 50, 60, "PLA", "Basic"},
  {"PLA+", "00001", 1, "PLA+", "AHPLPBK-102", "PLA", 0, 1, 205, 215, 50, 60, "PLA", "Plus"},
  {"PLA Silk", "00002", 4, "PLA Silk", "AHSCWH-102", "PLA", 0, 3, 200, 230, 55, 65, "PLA", "Silk"},
  {"PLA Matte", "00001", 2, "PLA Matte", "HYGBK-102", "PLA", 0, 6, 190, 230, 55, 65, "PLA", "Matte"},
  {"PLA-CF", "00006", 5, "PLA-CF", "", "PLA", 0, 4, 210, 240, 45, 65, "PLA-CF", "Basic"},
  {"PETG", "00003", 39, "PETG", "", "PETG", 1, 0, 230, 250, 70, 90, "PETG", "Basic"},
  {"PETG-CF", "00014", 42, "PETG-CF", "", "PETG", 1, 1, 240, 270, 65, 75, "PETG-CF", "Basic"},
  {"ABS", "00004", 11, "ABS", "SHABBK-102", "ABS", 2, 0, 240, 280, 90, 100, "ABS", "Basic"},
  {"ASA", "00007", 18, "ASA", "", "ASA", 8, 0, 240, 280, 90, 100, "ASA", "Basic"},
  {"TPU", "00005", 50, "TPU", "STPBK-101", "TPU", 3, 1, 210, 230, 25, 60, "TPU", "95A"},
  {"PC", "00021", 23, "PC", "", "PC", 6, 0, 260, 300, 100, 110, "PC", "Basic"},
  {"PA", "00008", 24, "", "", "", -1, -1, 250, 280, 50, 60, "PA", "Basic"},
  {"PA-CF", "00009", 25, "", "", "", -1, -1, 260, 300, 50, 60, "PA-CF", "Basic"},
  {"PVA", "00011", 47, "", "", "PVA", 7, 0, 215, 225, 50, 60, "PVA", "Basic"},
};

inline bool contains(const char *upper, const char *needle) { return std::strstr(upper, needle) != nullptr; }
// Maps free-text material names ("Silk PLA", "PETG CF", "Nylon") to a family, or nullptr.
// Unknown materials are refused: a wrong family would make the printer use wrong temperatures.
inline const Profile *profileFor(const char *material) {
  if (!material) return nullptr;
  char m[40] = {};
  for (size_t i = 0; i + 1 < sizeof(m) && material[i]; ++i) {
    const char c = material[i];
    m[i] = (c >= 'a' && c <= 'z') ? char(c - 32) : c;
  }
  const bool cf = contains(m, "CF") || contains(m, "CARBON");
  auto named = [](const char *name) -> const Profile * {
    for (const auto &p : profiles) if (std::strcmp(p.name, name) == 0) return &p;
    return nullptr;
  };
  // Families that share letters with supported ones (PCL vs PC, PPA vs PA) are refused, not guessed.
  for (const char *other : {"PCTG", "GF", "PCL", "PEBA", "HIPS", "PPA", "PPS", "PEEK", "PEI", "PEKK", "PP-", "PET-"})
    if (contains(m, other)) return nullptr;
  if (contains(m, "PETG")) return named(cf ? "PETG-CF" : "PETG");
  if (contains(m, "PLA")) {
    if (cf) return named("PLA-CF");
    if (contains(m, "SILK")) return named("PLA Silk");
    if (contains(m, "MATTE")) return named("PLA Matte");
    if (contains(m, "+") || contains(m, "PLUS")) return named("PLA+");
    return named("PLA");
  }
  if (contains(m, "ABS")) return cf ? nullptr : named("ABS");
  if (contains(m, "ASA")) return cf ? nullptr : named("ASA");
  if (contains(m, "TPU")) return named("TPU");
  if (contains(m, "PVA")) return named("PVA");
  if (contains(m, "PC")) return cf ? nullptr : named("PC");
  if (contains(m, "PA") || contains(m, "NYLON")) return named(cf ? "PA-CF" : "PA");
  return nullptr;
}
inline bool supports(Format f, const Profile &p) {
  switch (f) {
    case Format::Creality: return p.creality[0] != 0;
    case Format::Qidi: return p.qidi != 0;
    case Format::Anycubic: return p.anycubicType[0] != 0;
    case Format::Elegoo: return p.elegooFamily >= 0;
    default: return true;
  }
}

inline bool parseColor(const char *hex, uint8_t rgb[3]) {
  if (!hex || hex[0] != '#' || std::strlen(hex) != 7) return false;
  for (int i = 0; i < 3; ++i) {
    unsigned v = 0;
    if (std::sscanf(hex + 1 + i * 2, "%2x", &v) != 1) return false;
    rgb[i] = uint8_t(v);
  }
  return true;
}
// K2-RFID / ACE-RFID tables use 330 m per kg (1.75 mm, density 1.24); other spools scale from that.
inline uint16_t lengthMeters(double grams, double density = 1.24, double diameter = 1.75) {
  if (!std::isfinite(grams) || grams <= 0 || !std::isfinite(density) || density <= 0 ||
      !std::isfinite(diameter) || diameter <= 0) return 0;
  const double m = std::floor(grams * 33 / 100 * (1.24 / density) * (1.75 / diameter) * (1.75 / diameter) + 1e-9);
  return m > 9999 ? 9999 : uint16_t(m);
}

// ---------- AES-128 encryption (FIPS-197), ECB single block ----------
namespace detail {
constexpr uint8_t sbox[256] = {
  0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
  0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
  0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
  0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
  0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
  0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
  0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
  0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
  0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
  0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
  0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
  0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
  0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
  0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
  0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
  0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};
inline uint8_t xtime(uint8_t x) { return uint8_t((x << 1) ^ ((x & 0x80) ? 0x1b : 0)); }
}  // namespace detail
inline void aesEncrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
  using namespace detail;
  uint8_t rk[176];
  std::memcpy(rk, key, 16);
  uint8_t rcon = 1;
  for (int i = 16; i < 176; i += 4) {
    uint8_t t[4] = {rk[i - 4], rk[i - 3], rk[i - 2], rk[i - 1]};
    if (i % 16 == 0) {
      const uint8_t first = t[0];
      t[0] = uint8_t(sbox[t[1]] ^ rcon); t[1] = sbox[t[2]]; t[2] = sbox[t[3]]; t[3] = sbox[first];
      rcon = xtime(rcon);
    }
    for (int j = 0; j < 4; ++j) rk[i + j] = uint8_t(rk[i - 16 + j] ^ t[j]);
  }
  uint8_t s[16];
  for (int i = 0; i < 16; ++i) s[i] = uint8_t(in[i] ^ rk[i]);
  for (int round = 1; round <= 10; ++round) {
    uint8_t t[16];
    // SubBytes + ShiftRows (column-major state).
    for (int c = 0; c < 4; ++c)
      for (int r = 0; r < 4; ++r) t[c * 4 + r] = sbox[s[((c + r) % 4) * 4 + r]];
    if (round < 10) {
      for (int c = 0; c < 4; ++c) {
        uint8_t *col = t + c * 4;
        const uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3], all = uint8_t(a0 ^ a1 ^ a2 ^ a3);
        col[0] = uint8_t(a0 ^ all ^ xtime(uint8_t(a0 ^ a1)));
        col[1] = uint8_t(a1 ^ all ^ xtime(uint8_t(a1 ^ a2)));
        col[2] = uint8_t(a2 ^ all ^ xtime(uint8_t(a2 ^ a3)));
        col[3] = uint8_t(a3 ^ all ^ xtime(uint8_t(a3 ^ a0)));
      }
    }
    for (int i = 0; i < 16; ++i) s[i] = uint8_t(t[i] ^ rk[round * 16 + i]);
  }
  std::memcpy(out, s, 16);
}

// ---------- Creality CFS ----------
constexpr uint8_t crealityUidKey[16] = {'q','3','b','u','^','t','1','n','q','f','Z','(','p','f','$','1'};
constexpr uint8_t crealityDataKey[16] = {'H','@','C','F','k','R','n','z','@','K','A','t','B','J','p','2'};
// Sector-1 Key A/B: AES(uid repeated to 16 bytes), first six bytes.
inline void crealityKey(const uint8_t uid[4], uint8_t key[6]) {
  uint8_t block[16], out[16];
  for (int i = 0; i < 16; ++i) block[i] = uid[i % 4];
  aesEncrypt(crealityUidKey, block, out);
  std::memcpy(key, out, 6);
}
// 48-byte plaintext: date "AB124", vendor 0276, batch A2, "1"+id, "0"+RRGGBB, length m, serial, reserve.
inline bool crealityPlaintext(const Profile &p, const char *color, uint16_t meters, unsigned serial, char out[49]) {
  uint8_t rgb[3];
  if (!p.creality[0] || !parseColor(color, rgb)) return false;
  const int n = std::snprintf(out, 49, "AB1240276A21%s0%02X%02X%02X%04u%06u00000000000000",
                              p.creality, rgb[0], rgb[1], rgb[2], unsigned(meters > 9999 ? 9999 : meters), serial % 1000000);
  return n == 48;
}
inline bool crealityBlocks(const Profile &p, const char *color, uint16_t meters, unsigned serial, uint8_t out[48]) {
  char plain[49];
  if (!crealityPlaintext(p, color, meters, serial, plain)) return false;
  for (int b = 0; b < 3; ++b) aesEncrypt(crealityDataKey, reinterpret_cast<const uint8_t *>(plain) + b * 16, out + b * 16);
  return true;
}

// ---------- Qidi Box ----------
constexpr uint8_t qidiColors[24][3] = {
  {0xFA,0xFA,0xFA},{0x06,0x06,0x06},{0xD9,0xE3,0xED},{0x5C,0xF3,0x0F},{0x63,0xE4,0x92},{0x28,0x50,0xFF},
  {0xFE,0x98,0xFE},{0xDF,0xD6,0x28},{0x22,0x83,0x32},{0x99,0xDE,0xFF},{0x17,0x14,0xB0},{0xCE,0xC0,0xFE},
  {0xCA,0xDE,0x4B},{0x13,0x53,0xAB},{0x5E,0xA9,0xFD},{0xA8,0x78,0xFF},{0xFE,0x71,0x7A},{0xFF,0x36,0x2D},
  {0xE2,0xDF,0xCD},{0x89,0x8F,0x9B},{0x6E,0x38,0x12},{0xCA,0xC5,0x9F},{0xF2,0x86,0x36},{0xB8,0x7F,0x2B}};
// Qidi stores a palette index (1-24), so arbitrary colors snap to the nearest entry.
inline uint8_t qidiColor(const uint8_t rgb[3]) {
  uint8_t best = 1; long bestDistance = -1;
  for (int i = 0; i < 24; ++i) {
    long d = 0;
    for (int c = 0; c < 3; ++c) { const long delta = long(rgb[c]) - qidiColors[i][c]; d += delta * delta; }
    if (bestDistance < 0 || d < bestDistance) { bestDistance = d; best = uint8_t(i + 1); }
  }
  return best;
}
inline bool qidiBlock(const Profile &p, const char *color, uint8_t out[16]) {
  uint8_t rgb[3];
  if (!p.qidi || !parseColor(color, rgb)) return false;
  std::memset(out, 0, 16);
  out[0] = p.qidi; out[1] = qidiColor(rgb); out[2] = 0; // Vendor 0 = Generic.
  std::memcpy(out + 12, "QIDI", 4); // QIDI-RFID read-back marker; the printer ignores it.
  return true;
}

// ---------- Anycubic ACE: pages 4..31 (112 bytes) ----------
constexpr uint8_t anycubicFirstPage = 4, anycubicPages = 28;
inline void putLe16(uint8_t *at, unsigned v) { at[0] = uint8_t(v); at[1] = uint8_t(v >> 8); }
inline bool anycubicData(const Profile &p, const char *color, uint16_t meters, double grams, double diameter, uint8_t out[anycubicPages * 4]) {
  uint8_t rgb[3];
  if (!p.anycubicType[0] || !parseColor(color, rgb) || !std::isfinite(diameter)) return false;
  std::memset(out, 0, anycubicPages * 4);
  auto page = [&](int n) { return out + (n - anycubicFirstPage) * 4; };
  const uint8_t magic[] = {0x7b, 0x00, 0x65, 0x00};
  std::memcpy(page(4), magic, 4);
  std::strncpy(reinterpret_cast<char *>(page(5)), p.anycubicSku, 20);
  std::memcpy(page(10), "AC", 2);
  std::strncpy(reinterpret_cast<char *>(page(15)), p.anycubicType, 20);
  // A,B,G,R. ACE-RFID writes 010101 for black because 000000 means transparent.
  if (!rgb[0] && !rgb[1] && !rgb[2]) rgb[0] = rgb[1] = rgb[2] = 1;
  uint8_t *c = page(20); c[0] = 0xff; c[1] = rgb[2]; c[2] = rgb[1]; c[3] = rgb[0];
  putLe16(page(24), p.nozzleMin); putLe16(page(24) + 2, p.nozzleMax);
  putLe16(page(29), p.bedMin); putLe16(page(29) + 2, p.bedMax);
  putLe16(page(30), unsigned(std::lround(diameter * 100))); putLe16(page(30) + 2, meters);
  const unsigned weight = grams > 0 && grams < 65535 ? unsigned(std::lround(grams)) : 1000;
  putLe16(page(31), weight);
  return true;
}

// ---------- Elegoo Canvas: pages 4..24 (84 bytes) ----------
constexpr uint8_t elegooFirstPage = 4, elegooPages = 21;
inline void putBe16(uint8_t *at, unsigned v) { at[0] = uint8_t(v >> 8); at[1] = uint8_t(v); }
inline bool elegooData(const Profile &p, const char *color, double grams, double diameter, uint8_t out[elegooPages * 4]) {
  uint8_t rgb[3];
  if (p.elegooFamily < 0 || !parseColor(color, rgb) || !std::isfinite(diameter)) return false;
  std::memset(out, 0, elegooPages * 4);
  auto page = [&](int n) { return out + (n - elegooFirstPage) * 4; };
  const uint8_t header[] = {0x36, 0xee, 0xee, 0xee, 0xee, 0, 0, 0};
  std::memcpy(page(16), header, 8);
  // Each ASCII code's decimal digits are written as one hex byte: "PLA" -> 00 80 76 65.
  const size_t len = std::strlen(p.elegooType);
  for (size_t i = 0; i < 4 && i < len; ++i) {
    const unsigned code = uint8_t(p.elegooType[len - 1 - i]);
    page(18)[3 - i] = uint8_t(((code / 10) << 4) | (code % 10));
  }
  page(19)[0] = uint8_t(p.elegooFamily); page(19)[1] = uint8_t(p.elegooSub);
  page(20)[0] = rgb[0]; page(20)[1] = rgb[1]; page(20)[2] = rgb[2]; page(20)[3] = 0xff;
  putBe16(page(21), p.nozzleMin); putBe16(page(21) + 2, p.nozzleMax);
  const unsigned weight = grams > 0 && grams < 65535 ? unsigned(std::lround(grams)) : 1000;
  putBe16(page(23), unsigned(std::lround(diameter * 100))); putBe16(page(23) + 2, weight);
  const uint8_t date[] = {0x00, 0x36, 0xc8, 0x00};
  std::memcpy(page(24), date, 4);
  return true;
}

// ---------- OpenSpool JSON (NDEF application/json) ----------
constexpr const char *openspoolMime = "application/json";
inline size_t jsonEscape(const char *in, char *out, size_t cap) {
  size_t n = 0;
  for (; in && *in; ++in) {
    const unsigned char c = static_cast<unsigned char>(*in);
    char buf[8]; size_t len = 0;
    if (c == '"' || c == '\\') { buf[0] = '\\'; buf[1] = char(c); len = 2; }
    else if (c < 0x20) len = size_t(std::snprintf(buf, sizeof(buf), "\\u%04x", c));
    else { buf[0] = char(c); len = 1; }
    if (n + len >= cap) return 0;
    std::memcpy(out + n, buf, len); n += len;
  }
  if (n >= cap) return 0;
  out[n] = 0; return n;
}
inline size_t openspoolJson(const Profile &p, const char *brand, const char *color, double grams, double diameter, char *out, size_t cap) {
  uint8_t rgb[3];
  char escaped[128];
  if (!parseColor(color, rgb) || !std::isfinite(diameter)) return 0;
  if (!brand || !*brand) brand = "Generic";
  if (!jsonEscape(brand, escaped, sizeof(escaped))) return 0;
  const unsigned weight = grams > 0 && grams < 65535 ? unsigned(std::lround(grams)) : 1000;
  const int n = std::snprintf(out, cap,
      "{\"protocol\":\"openspool\",\"version\":\"1.0\",\"type\":\"%s\",\"subtype\":\"%s\",\"color_hex\":\"%02X%02X%02X\","
      "\"brand\":\"%s\",\"min_temp\":%u,\"max_temp\":%u,\"bed_min_temp\":%u,\"bed_max_temp\":%u,\"weight\":%u,\"diameter\":%.2f}",
      p.openspoolType, p.openspoolSubtype, rgb[0], rgb[1], rgb[2], escaped, p.nozzleMin, p.nozzleMax, p.bedMin, p.bedMax,
      weight, diameter);
  return n > 0 && size_t(n) < cap ? size_t(n) : 0;
}
}  // namespace BrandTags
