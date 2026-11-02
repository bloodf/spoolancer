#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <string>

class String : public std::string {
 public:
  using std::string::string;
  using std::string::operator=;
  bool isEmpty() const { return empty(); }
};
constexpr int SERIAL_8N1 = 0;
uint32_t millis();
void delay(unsigned milliseconds);

class HardwareSerial {
 public:
  std::deque<uint8_t> incoming;
  std::function<void(const uint8_t *, size_t)> onWrite;
  void begin(unsigned, int, int, int) {}
  size_t available() const { return incoming.size(); }
  uint8_t read() { const auto byte = incoming.front(); incoming.pop_front(); return byte; }
  size_t write(const uint8_t *bytes, size_t size) {
    if (onWrite) onWrite(bytes, size);
    return size;
  }
};
