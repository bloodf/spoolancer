#pragma once
// Minimal SHA-256, HMAC-SHA256 and HKDF-SHA256 for deriving MIFARE Classic sector keys
// from a tag UID. Used to read Bambu Lab (HKDF) and Snapmaker U1 (HMAC) filament tags.
// ponytail: plain reference implementation; the ESP32 could use mbedtls, but a self-contained
// header keeps the host tests dependency-free and the per-tag cost (16 sectors) is negligible.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace Crypto {
struct Sha256 {
  uint32_t state[8]; uint64_t bits = 0; uint8_t buffer[64]; size_t fill = 0;
  static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
  Sha256() {
    const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                              0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::memcpy(state, init, sizeof(state));
  }
  void block(const uint8_t *p) {
    static const uint32_t k[64] = {
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) | (uint32_t(p[i * 4 + 2]) << 8) | p[i * 4 + 3];
    for (int i = 16; i < 64; ++i) {
      const uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t v[8]; std::memcpy(v, state, sizeof(v));
    for (int i = 0; i < 64; ++i) {
      const uint32_t S1 = ror(v[4], 6) ^ ror(v[4], 11) ^ ror(v[4], 25);
      const uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
      const uint32_t t1 = v[7] + S1 + ch + k[i] + w[i];
      const uint32_t S0 = ror(v[0], 2) ^ ror(v[0], 13) ^ ror(v[0], 22);
      const uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
      const uint32_t t2 = S0 + maj;
      v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1;
      v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + t2;
    }
    for (int i = 0; i < 8; ++i) state[i] += v[i];
  }
  void update(const uint8_t *data, size_t len) {
    bits += uint64_t(len) * 8;
    while (len) {
      const size_t take = std::min(sizeof(buffer) - fill, len);
      std::memcpy(buffer + fill, data, take); fill += take; data += take; len -= take;
      if (fill == 64) { block(buffer); fill = 0; }
    }
  }
  void finish(uint8_t out[32]) {
    const uint8_t one = 0x80; const uint64_t total = bits;
    update(&one, 1);
    const uint8_t zero = 0;
    while (fill != 56) update(&zero, 1);
    uint8_t len[8];
    for (int i = 0; i < 8; ++i) len[i] = uint8_t(total >> (56 - i * 8));
    // update() would keep adding to bits; write the length block directly.
    std::memcpy(buffer + 56, len, 8); block(buffer);
    for (int i = 0; i < 8; ++i) { out[i * 4] = uint8_t(state[i] >> 24); out[i * 4 + 1] = uint8_t(state[i] >> 16); out[i * 4 + 2] = uint8_t(state[i] >> 8); out[i * 4 + 3] = uint8_t(state[i]); }
  }
};
inline void sha256(const uint8_t *data, size_t len, uint8_t out[32]) { Sha256 h; h.update(data, len); h.finish(out); }

inline void hmacSha256(const uint8_t *key, size_t keyLen, const uint8_t *msg, size_t msgLen, uint8_t out[32]) {
  uint8_t block[64] = {};
  uint8_t shortened[32];
  if (keyLen > 64) { sha256(key, keyLen, shortened); key = shortened; keyLen = 32; }
  std::memcpy(block, key, keyLen);
  uint8_t ipad[64], opad[64];
  for (int i = 0; i < 64; ++i) { ipad[i] = block[i] ^ 0x36; opad[i] = block[i] ^ 0x5c; }
  uint8_t inner[32];
  Sha256 h1; h1.update(ipad, 64); h1.update(msg, msgLen); h1.finish(inner);
  Sha256 h2; h2.update(opad, 64); h2.update(inner, 32); h2.finish(out);
}

// RFC 5869. length must be a multiple of 32 here (<= 255*32).
inline void hkdfSha256(const uint8_t *ikm, size_t ikmLen, const uint8_t *salt, size_t saltLen,
                       const uint8_t *info, size_t infoLen, uint8_t *out, size_t length) {
  uint8_t prk[32];
  hmacSha256(salt, saltLen, ikm, ikmLen, prk);
  uint8_t previous[32]; size_t prevLen = 0, done = 0;
  for (uint8_t counter = 1; done < length; ++counter) {
    // T(n) = HMAC(prk, T(n-1) | info | counter)
    uint8_t ipad[64], opad[64], block[64] = {};
    std::memcpy(block, prk, 32);
    for (int i = 0; i < 64; ++i) { ipad[i] = block[i] ^ 0x36; opad[i] = block[i] ^ 0x5c; }
    uint8_t t[32];
    Sha256 h1; h1.update(ipad, 64); h1.update(previous, prevLen); h1.update(info, infoLen); h1.update(&counter, 1); h1.finish(t);
    Sha256 h2; h2.update(opad, 64); h2.update(t, 32); h2.finish(t);
    const size_t take = std::min<size_t>(32, length - done);
    std::memcpy(out + done, t, take); done += take;
    std::memcpy(previous, t, 32); prevLen = 32;
  }
}
}  // namespace Crypto
