#include "Domain.h"
#include <array>
#include <vector>
#include <cassert>
#include <iostream>
#include <string>

static void measurements() {
  using Domain::measurementAllowed;
  assert(measurementAllowed(1250, 250, 1000, true, true, true));
  assert(measurementAllowed(250, 250, 1000, true, true, true));
  assert(!measurementAllowed(1250, 250, 1000, false, true, true));
  assert(!measurementAllowed(1250, 250, 1000, true, false, true));
  assert(!measurementAllowed(1250, 250, 1000, true, true, false));
  assert(!measurementAllowed(NAN, 250, 1000, true, true, true));
  assert(!measurementAllowed(INFINITY, 250, 1000, true, true, true));
  assert(!measurementAllowed(1250, NAN, 1000, true, true, true));
  assert(!measurementAllowed(2001, 250, 5000, true, true, true));
  assert(!measurementAllowed(19, 0, 1000, true, true, true));
  assert(!measurementAllowed(249, 250, 1000, true, true, true));
  assert(!measurementAllowed(1251, 250, 1000, true, true, true));
  assert(!measurementAllowed(1250, -1, 5000, true, true, true));
  assert(!measurementAllowed(1250, 250, 5001, true, true, true));
  assert(Domain::validColor("#Ab09eF"));
  assert(!Domain::validColor("#gg0000"));
  assert(!Domain::validColor("000000"));
  assert(!Domain::validColor(nullptr));
  assert(Domain::validId("0123456789abcdef"));
  assert(!Domain::validId("0123456789ABCDEF"));
  assert(!Domain::validId("0123456789abcdefx"));
}

static void tagProtection() {
  for (const auto code : {0x0f, 0x11, 0x13}) {
    uint8_t version[] = {0, 4, 4, 2, 1, 0, uint8_t(code), 3};
    const auto layout = Domain::ntagLayout(version, sizeof(version));
    assert(layout.capacity() == (code == 0x0f ? 144 : code == 0x11 ? 496 : 872));
    assert(layout.lockPage == (code == 0x0f ? 40 : code == 0x11 ? 130 : 226));
    for (size_t size : {size_t(0), size_t(7), size_t(9)})
      assert(!Domain::ntagLayout(version, size).capacity());
    for (size_t index = 0; index < sizeof(version); ++index) {
      auto bad = std::array<uint8_t, 8>{};
      std::memcpy(bad.data(), version, sizeof(version)); bad[index] ^= 0x80;
      assert(!Domain::ntagLayout(bad.data(), bad.size()).capacity());
    }
    std::array<uint8_t, 16> start{}, config{};
    start[4] = 0xe1; start[5] = 0x10; start[6] = layout.ccSize; config[7] = 0xff;
    assert(Domain::ntagUnlocked(layout, start.data(), config.data()));
    for (int index : {2, 3, 4, 5, 6, 7}) {
      const uint8_t original = start[index]; start[index] ^= 1;
      assert(!Domain::ntagUnlocked(layout, start.data(), config.data()));
      start[index] = original;
    }
    start[7] = 0x0f;
    assert(Domain::ntagContainer(layout, start.data()));
    assert(!Domain::ntagUnlocked(layout, start.data(), config.data()));
    start[7] = 0;
    for (int index : {0, 1, 2}) {
      config[index] = 1;
      assert(!Domain::ntagUnlocked(layout, start.data(), config.data()));
      config[index] = 0;
    }
    for (int bit : {0x40, 0x80}) {
      config[4] = bit;
      assert(!Domain::ntagUnlocked(layout, start.data(), config.data()));
      config[4] = 0; config[8] = bit;
      assert(!Domain::ntagUnlocked(layout, start.data(), config.data()));
      config[8] = 0;
    }
    config[7] = 4;
    assert(!Domain::ntagUnlocked(layout, start.data(), config.data()));
    assert(!Domain::ntagUnlocked({}, start.data(), config.data()));
  }
}

static void records() {
  std::array<uint8_t, 720> encoded{};
  const size_t typeLen = std::strlen(Domain::ndefMime);
  for (size_t length : {size_t(1), size_t(200), size_t(219), size_t(220), size_t(254), size_t(255), size_t(256), Domain::maxPayload}) {
    std::string payload(length, 'x');
    payload[0] = '{';
    const size_t total = Domain::encodeNdef(reinterpret_cast<const uint8_t *>(payload.data()), payload.size(), encoded.data(), encoded.size());
    assert(total && total <= 720 && total == Domain::ndefSize(typeLen, length));
    // Short TLV below 0xFF record bytes; short record up to 255 payload bytes.
    const size_t header = encoded[1] == 0xff ? 4 : 2;
    const size_t recordLen = (length <= 255 ? 3 : 6) + typeLen + length;
    assert((recordLen < 0xff) == (header == 2));
    assert(encoded[header] == (length <= 255 ? 0xd2 : 0xc2));
    size_t offset = 0, count = 0;
    assert(Domain::decodeNdef(encoded.data(), total, offset, count));
    assert(count == length && std::memcmp(encoded.data() + offset, payload.data(), count) == 0);
    for (size_t truncated = 0; truncated < total; ++truncated)
      assert(!Domain::decodeNdef(encoded.data(), truncated, offset, count));
    auto bad = encoded; bad[offset - 1] ^= 1; // MIME type text.
    assert(!Domain::decodeNdef(bad.data(), total, offset, count));
    bad = encoded; bad[header] |= 0x08; // ID length flag.
    assert(!Domain::decodeNdef(bad.data(), total, offset, count));
    bad = encoded; bad[header - 1] ^= 1; // TLV length.
    assert(!Domain::decodeNdef(bad.data(), total, offset, count));
    bad = encoded; bad[header + 2] ^= 0x01; // Payload length.
    assert(!Domain::decodeNdef(bad.data(), total, offset, count));
    bad = encoded; bad[total - 1] = 0;
    assert(!Domain::decodeNdef(bad.data(), total, offset, count));
    assert(!Domain::decodeNdef(encoded.data(), total, offset, count, "application/json"));
    assert(!Domain::encodeNdef(reinterpret_cast<const uint8_t *>(payload.data()), payload.size(), encoded.data(), total - 1));
  }
  // Records written by earlier firmware (three-byte TLV, long record) stay readable.
  const char *legacyPayload = "{}";
  std::vector<uint8_t> legacy = {3, 0xff, 0, uint8_t(6 + typeLen + 2), 0xc2, uint8_t(typeLen), 0, 0, 0, 2};
  legacy.insert(legacy.end(), Domain::ndefMime, Domain::ndefMime + typeLen);
  legacy.insert(legacy.end(), legacyPayload, legacyPayload + 2);
  legacy.push_back(0xfe);
  size_t offset = 0, count = 0;
  assert(Domain::decodeNdef(legacy.data(), legacy.size(), offset, count) && count == 2 && legacy[offset] == '{');
  // Other MIME types (OpenSpool).
  const uint8_t json[] = {'{', '}'};
  const size_t total = Domain::encodeNdef(json, 2, encoded.data(), encoded.size(), "application/json");
  assert(total == 2 + 3 + 16 + 2 + 1 && encoded[1] == 21 && encoded[2] == 0xd2 && encoded[3] == 16 && encoded[4] == 2);
  assert(Domain::decodeNdef(encoded.data(), total, offset, count, "application/json") && count == 2);
  const uint8_t dummy[] = {0};
  assert(!Domain::encodeNdef(dummy, 0, encoded.data(), encoded.size()));
  assert(!Domain::encodeNdef(dummy, Domain::maxPayload + 1, encoded.data(), encoded.size()));
}

void scaleDriverTests();
void setupTests();
void remoteInventoryTests();
void statusTests();
void brandTagTests();
void otaTests();
int main() {
  measurements(); tagProtection(); records(); scaleDriverTests(); setupTests(); remoteInventoryTests(); statusTests(); brandTagTests(); otaTests();
  std::cout << "PASS: measurement/NDEF/tag safeguards, mocked HX711 calibration/freshness, AP-only setup guards, remote inventory cache/pagination, LED/OLED validity and event expiry, printer-brand tag vectors\n";
}
