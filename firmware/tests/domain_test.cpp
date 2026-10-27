#include "Domain.h"
#include <array>
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
  uint8_t version[] = {0, 4, 4, 2, 1, 0, 0x13, 3};
  assert(Domain::ntag216Version(version, sizeof(version)));
  assert(!Domain::ntag216Version(version, 7));
  version[6] = 0x11;
  assert(!Domain::ntag216Version(version, sizeof(version)));
  std::array<uint8_t, 16> start{}, config{};
  start[4] = 0xe1; start[5] = 0x10; start[6] = 0x6d; config[7] = 0xff;
  assert(Domain::ntag216Unlocked(start.data(), config.data()));
  for (int index : {2, 3, 7}) {
    start[index] = 1;
    assert(!Domain::ntag216Unlocked(start.data(), config.data()));
    start[index] = 0;
  }
  for (int index : {0, 1, 2}) {
    config[index] = 1;
    assert(!Domain::ntag216Unlocked(start.data(), config.data()));
    config[index] = 0;
  }
  config[4] = 0x40;
  assert(!Domain::ntag216Unlocked(start.data(), config.data()));
  config[4] = 0; config[7] = 4;
  assert(!Domain::ntag216Unlocked(start.data(), config.data()));
}

static void records() {
  std::array<uint8_t, 720> encoded{};
  for (size_t length : {size_t(1), size_t(254), size_t(255), Domain::maxPayload}) {
    std::string payload(length, 'x');
    payload[0] = '{';
    const size_t total = Domain::encodeNdef(reinterpret_cast<const uint8_t *>(payload.data()), payload.size(), encoded.data(), encoded.size());
    assert(total && total <= 720);
    size_t offset = 0, count = 0;
    assert(Domain::decodeNdef(encoded.data(), total, offset, count));
    assert(count == length && std::memcmp(encoded.data() + offset, payload.data(), count) == 0);
    for (size_t truncated = 0; truncated < total; ++truncated)
      assert(!Domain::decodeNdef(encoded.data(), truncated, offset, count));
    auto bad = encoded; bad[10] ^= 1;
    assert(!Domain::decodeNdef(bad.data(), total, offset, count));
    bad = encoded; bad[4] = 0xd2;
    assert(!Domain::decodeNdef(bad.data(), total, offset, count));
    bad = encoded; bad[3] ^= 1;
    assert(!Domain::decodeNdef(bad.data(), total, offset, count));
    bad = encoded; bad[6] = 0xff;
    assert(!Domain::decodeNdef(bad.data(), total, offset, count));
    bad = encoded; bad[total - 1] = 0;
    assert(!Domain::decodeNdef(bad.data(), total, offset, count));
    assert(!Domain::encodeNdef(reinterpret_cast<const uint8_t *>(payload.data()), payload.size(), encoded.data(), total - 1));
  }
  const uint8_t dummy[] = {0};
  assert(!Domain::encodeNdef(dummy, 0, encoded.data(), encoded.size()));
  assert(!Domain::encodeNdef(dummy, Domain::maxPayload + 1, encoded.data(), encoded.size()));
}

void scaleDriverTests();
void setupTests();
int main() {
  measurements(); tagProtection(); records(); scaleDriverTests(); setupTests();
  std::cout << "PASS: measurement/NDEF/tag safeguards, mocked HX711 calibration/freshness, physical setup timing and enrollment guards\n";
}
