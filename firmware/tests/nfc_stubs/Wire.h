#pragma once
#include "Arduino.h"
#include <cassert>
#include <vector>

// Models separate PN532 I2C transactions: ready byte, ACK, then response.
class TwoWire {
 public:
  std::deque<uint8_t> incoming;
  std::function<void(const uint8_t *, size_t)> onWrite;
  unsigned busyReads = 0, beginCalls = 0, oledWrites = 0, busyFrameReads = 0;
  bool failBegin = false, nack = false, truncate = false;
  size_t setBufferSize(size_t size) { return size; }
  bool begin(int sda, int scl, unsigned speed) {
    assert(sda == 4 && scl == 5 && speed == 100000);
    ++beginCalls; return !failBegin;
  }
  void setTimeOut(unsigned) {}
  void beginTransmission(uint8_t address) {
    assert(address == 0x24 || address == 0x3c);
    outgoingAddress = address; outgoing.clear();
  }
  size_t write(const uint8_t *data, size_t size) {
    outgoing.insert(outgoing.end(), data, data + size); return size;
  }
  uint8_t endTransmission() {
    if (outgoingAddress == 0x3c) { ++oledWrites; return 0; }
    if (nack) return 2;
    if (outgoing.size() == 6) incoming.clear(); // Abort discards pending response.
    if (!outgoing.empty() && onWrite) onWrite(outgoing.data(), outgoing.size());
    return 0;
  }
  size_t requestFrom(uint8_t address, size_t size) {
    assert(address == 0x24);
    received.clear();
    if (busyReads || incoming.empty()) {
      if (size > 1) ++busyFrameReads;
      if (busyReads) --busyReads;
      received.push_back(0); return 1;
    }
    received.push_back(1);
    // The device returns only this frame; extra requested bytes are padding.
    size_t frameSize = incoming.size() >= 4 ? incoming[3] ? size_t(incoming[3]) + 7 : 6 : incoming.size();
    frameSize = std::min(frameSize, incoming.size());
    const size_t count = std::min(frameSize, size - 1);
    for (size_t i = 0; i < count; ++i) { received.push_back(incoming.front()); incoming.pop_front(); }
    if (truncate && received.size() > 1) received.pop_back();
    else while (received.size() < size) received.push_back(0);
    return received.size();
  }
  size_t available() const { return received.size(); }
  int read() {
    if (received.empty()) return -1;
    const int byte = received.front(); received.pop_front(); return byte;
  }
 private:
  uint8_t outgoingAddress = 0;
  std::vector<uint8_t> outgoing;
  std::deque<uint8_t> received;
};
