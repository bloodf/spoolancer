#include "BrandTags.h"
#include "Domain.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

using namespace BrandTags;

namespace {
std::vector<uint8_t> hex(const char *text) {
  std::vector<uint8_t> out;
  for (const char *p = text; *p;) {
    if (*p == ' ') { ++p; continue; }
    unsigned v = 0; std::sscanf(p, "%2x", &v); out.push_back(uint8_t(v)); p += 2;
  }
  return out;
}
bool equals(const uint8_t *data, const char *expected) {
  const auto bytes = hex(expected);
  return std::memcmp(data, bytes.data(), bytes.size()) == 0;
}
// Research vectors use PLA 200-220 C nozzle and 60 C bed.
Profile vectorPla() { Profile p = *profileFor("PLA"); p.nozzleMin = 200; p.nozzleMax = 220; p.bedMin = 60; p.bedMax = 60; return p; }
}

static void aes() {
  // FIPS-197 Appendix C.1.
  const auto key = hex("000102030405060708090a0b0c0d0e0f"), plain = hex("00112233445566778899aabbccddeeff");
  uint8_t out[16];
  aesEncrypt(key.data(), plain.data(), out);
  assert(equals(out, "69c4e0d86a7b0430d8cdb78070b4c55a"));
}

static void creality() {
  const uint8_t uid[4] = {0xde, 0xad, 0xbe, 0xef};
  uint8_t key[6];
  crealityKey(uid, key);
  assert(equals(key, "9B 79 96 EB 4A 68"));
  Profile hyper = *profileFor("PLA"); hyper.creality = "01001"; // DnG K2-RFID vector uses Hyper PLA.
  char plain[49];
  assert(crealityPlaintext(hyper, "#ff0000", lengthMeters(1000), 1, plain));
  assert(std::string(plain) == "AB1240276A21010010FF0000033000000100000000000000");
  uint8_t blocks[48];
  assert(crealityBlocks(hyper, "#FF0000", lengthMeters(1000), 1, blocks));
  assert(equals(blocks, "57 F2 5B 78 07 6D 4C 17 97 B1 BE 35 CA 26 95 40"));
  assert(equals(blocks + 16, "11 A0 4C 3E 0D 0B DF 51 73 4B 01 1C 49 C7 A9 AB"));
  assert(equals(blocks + 32, "FA C8 F0 75 09 29 2D F9 43 D4 CD F6 4C BA 06 A1"));
  assert(lengthMeters(750) == 247 && lengthMeters(600) == 198 && lengthMeters(500) == 165 && lengthMeters(250) == 82);
  assert(lengthMeters(1000, 1.24, 2.85) == 124 && lengthMeters(1000, 1.04) == 393 && lengthMeters(1000, 0) == 0);
  assert(!crealityPlaintext(hyper, "red", 330, 1, plain));
}

static void qidi() {
  uint8_t block[16];
  assert(qidiBlock(*profileFor("PLA"), "#FF0000", block));
  assert(equals(block, "01 12 00 00 00 00 00 00 00 00 00 00 51 49 44 49"));
  assert(qidiBlock(*profileFor("PETG-CF"), "#060606", block) && block[0] == 42 && block[1] == 2);
}

static void anycubic() {
  uint8_t data[anycubicPages * 4];
  assert(anycubicData(vectorPla(), "#FF0000", 330, 1000, 1.75, data));
  auto page = [&](int n) { return data + (n - 4) * 4; };
  assert(equals(page(4), "7B 00 65 00") && equals(page(5), "41 48 50 4C 42 4B 2D 31 30 31 00 00"));
  assert(equals(page(10), "41 43 00 00") && equals(page(15), "50 4C 41 00"));
  assert(equals(page(20), "FF 00 00 FF") && equals(page(24), "C8 00 DC 00"));
  assert(equals(page(29), "3C 00 3C 00") && equals(page(30), "AF 00 4A 01") && equals(page(31), "E8 03 00 00"));
  assert(anycubicData(vectorPla(), "#000000", 330, 1000, 1.75, data) && equals(page(20), "FF 01 01 01"));
  assert(anycubicData(vectorPla(), "#123456", 330, 1000, 1.75, data) && equals(page(20), "FF 56 34 12"));
  assert(!anycubicData(*profileFor("PA"), "#FF0000", 330, 1000, 1.75, data));
}

static void elegoo() {
  uint8_t data[elegooPages * 4];
  assert(elegooData(vectorPla(), "#FF0000", 1000, 1.75, data));
  auto page = [&](int n) { return data + (n - 4) * 4; };
  for (int n = 4; n < 16; ++n) assert(equals(page(n), "00 00 00 00"));
  assert(equals(page(16), "36 EE EE EE EE 00 00 00 00 80 76 65 00 00 00 00 FF 00 00 FF 00 C8 00 DC 00 00 00 00 00 AF 03 E8 00 36 C8 00"));
  const struct { const char *material, *code; } codes[] = {
    {"PETG", "80 69 84 71"}, {"ABS", "00 65 66 83"}, {"TPU", "00 84 80 85"}, {"ASA", "00 65 83 65"}, {"PC", "00 00 80 67"}};
  for (const auto &c : codes) { assert(elegooData(*profileFor(c.material), "#FF0000", 1000, 1.75, data)); assert(equals(page(18), c.code)); }
  assert(!elegooData(*profileFor("PA"), "#FF0000", 1000, 1.75, data));
}

static void openspool() {
  char json[400];
  const size_t n = openspoolJson(vectorPla(), "Generic", "#FF0000", 1000, 1.75, json, sizeof(json));
  assert(n && std::string(json) == "{\"protocol\":\"openspool\",\"version\":\"1.0\",\"type\":\"PLA\",\"subtype\":\"Basic\","
         "\"color_hex\":\"FF0000\",\"brand\":\"Generic\",\"min_temp\":200,\"max_temp\":220,\"bed_min_temp\":60,"
         "\"bed_max_temp\":60,\"weight\":1000,\"diameter\":1.75}");
  assert(openspoolJson(vectorPla(), "Quote \"\\ Co", "#FF0000", 1000, 1.75, json, sizeof(json)));
  assert(std::string(json).find("\"brand\":\"Quote \\\"\\\\ Co\"") != std::string::npos);
  assert(!openspoolJson(vectorPla(), "Generic", "#FF0000", 1000, 1.75, json, 40));
  // The record fits NTAG215, not NTAG213.
  uint8_t ndef[720];
  assert(!Domain::encodeNdef(reinterpret_cast<const uint8_t *>(json), n, ndef, 144, openspoolMime));
  assert(Domain::encodeNdef(reinterpret_cast<const uint8_t *>(json), n, ndef, 496, openspoolMime));
}

static void materials() {
  const struct { const char *in, *out; } cases[] = {
    {"PLA", "PLA"}, {"pla", "PLA"}, {"PLA+", "PLA+"}, {"Silk PLA", "PLA Silk"}, {"PLA Matte", "PLA Matte"},
    {"PLA-CF", "PLA-CF"}, {"PETG", "PETG"}, {"PETG CF", "PETG-CF"}, {"ABS", "ABS"}, {"ASA", "ASA"},
    {"TPU 95A", "TPU"}, {"PC", "PC"}, {"Nylon", "PA"}, {"PA6-CF", "PA-CF"}, {"PVA", "PVA"}};
  for (const auto &c : cases) { const Profile *p = profileFor(c.in); assert(p && std::strcmp(p->name, c.out) == 0); }
  for (const char *unknown : {"PCTG", "PETG-GF", "HIPS", "Wood?", "", "ABS-CF", "PCL", "PPA-CF", "PEBA", "PET-CF", "PP-CF"}) assert(!profileFor(unknown));
  Format f;
  assert(parseFormat("creality", f) && f == Format::Creality && info(f).chip == Chip::Classic);
  assert(parseFormat("openspool", f) && f == Format::OpenSpool && !parseFormat("bambu", f) && !parseFormat(nullptr, f));
  assert(supports(Format::Creality, *profileFor("PA")) && !supports(Format::Anycubic, *profileFor("PA")));
}

void brandTagTests() { aes(); creality(); qidi(); anycubic(); elegoo(); openspool(); materials(); }
