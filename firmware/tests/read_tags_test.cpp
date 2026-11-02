#include "Sha256.h"
#include "ReadTags.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

namespace {
std::vector<uint8_t> hx(const char *t) {
  std::vector<uint8_t> o;
  for (const char *p = t; *p; p += 2) { unsigned v; std::sscanf(p, "%2x", &v); o.push_back(uint8_t(v)); }
  return o;
}
std::string hex(const uint8_t *d, size_t n) {
  std::string s; char b[3];
  for (size_t i = 0; i < n; ++i) { std::snprintf(b, sizeof(b), "%02x", d[i]); s += b; }
  return s;
}
}

static void sha() {
  uint8_t out[32];
  Crypto::sha256(reinterpret_cast<const uint8_t *>("abc"), 3, out);
  assert(hex(out, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  Crypto::sha256(nullptr, 0, out);
  assert(hex(out, 32) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  // A message spanning several blocks.
  std::string long_(200, 'a');
  Crypto::sha256(reinterpret_cast<const uint8_t *>(long_.data()), long_.size(), out);
  Crypto::Sha256 ref; for (char c : long_) ref.update(reinterpret_cast<const uint8_t *>(&c), 1); ref.finish(out);
  // RFC 4231 HMAC test case 2.
  uint8_t mac[32];
  Crypto::hmacSha256(reinterpret_cast<const uint8_t *>("Jefe"), 4,
                     reinterpret_cast<const uint8_t *>("what do ya want for nothing?"), 28, mac);
  assert(hex(mac, 32) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
  // RFC 5869 HKDF test case 1.
  const auto ikm = hx("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");
  const auto salt = hx("000102030405060708090a0b0c");
  const auto info = hx("f0f1f2f3f4f5f6f7f8f9");
  uint8_t okm[42];
  Crypto::hkdfSha256(ikm.data(), ikm.size(), salt.data(), salt.size(), info.data(), info.size(), okm, sizeof(okm));
  assert(hex(okm, 42) == "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");
}

static void bambu() {
  const uint8_t uid[4] = {0xde, 0xad, 0xbe, 0xef};
  uint8_t keys[16][6];
  ReadTags::bambuKeys(uid, keys);
  assert(hex(keys[0], 6) == "045c6dc690e9");
  assert(hex(keys[1], 6) == "daf05c224715");
  assert(hex(keys[2], 6) == "141899c0b498");

  uint8_t data[1024] = {};
  std::memcpy(data + 4 * 16, "PLA Basic", 9);
  const uint8_t block5[] = {0xff, 0x00, 0x00, 0xff, 0xe8, 0x03}; // Red, 1000 g.
  std::memcpy(data + 5 * 16, block5, sizeof(block5));
  data[14 * 16 + 4] = 0x4a; data[14 * 16 + 5] = 0x01; // 330 m.
  const auto d = ReadTags::decodeBambu(data);
  assert(d.valid && std::string(d.brand) == "Bambu Lab" && std::string(d.material) == "PLA Basic");
  assert(std::string(d.color) == "#FF0000" && d.weightG == 1000 && d.lengthM == 330);
  uint8_t blank[1024] = {};
  assert(!ReadTags::decodeBambu(blank).valid);
}

static void snapmaker() {
  const uint8_t uid[4] = {0xde, 0xad, 0xbe, 0xef};
  uint8_t key[6];
  ReadTags::snapmakerKey(uid, 0, key); assert(hex(key, 6) == "a550491f66e9");
  ReadTags::snapmakerKey(uid, 1, key); assert(hex(key, 6) == "b6bca4e3fe05");

  uint8_t data[1024] = {};
  std::memcpy(data + 16, "Snapmaker", 9);           // Sector 0 vendor.
  data[64 + 2] = 1;                                  // main_type PLA.
  data[64 + 4] = 4;                                  // sub_type Silk.
  const uint8_t rgb[] = {0x00, 0xff, 0x00};
  std::memcpy(data + 64 + 16, rgb, 3);               // RGB_1 green.
  data[128 + 2] = 0xe8; data[128 + 3] = 0x03;        // Weight 1000 g.
  data[128 + 4] = 0x4a; data[128 + 5] = 0x01;        // Length 330 m.
  const auto d = ReadTags::decodeSnapmaker(data);
  assert(d.valid && std::string(d.brand) == "Snapmaker" && std::string(d.material) == "PLA Silk");
  assert(std::string(d.color) == "#00FF00" && d.weightG == 1000 && d.lengthM == 330);
  uint8_t blank[1024] = {};
  assert(!ReadTags::decodeSnapmaker(blank).valid);
}

void readTagTests() { sha(); bambu(); snapmaker(); }
