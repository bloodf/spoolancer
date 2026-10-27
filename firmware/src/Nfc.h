#pragma once
#include <Arduino.h>
#include <functional>

class Nfc {
 public:
  explicit Nfc(HardwareSerial &port) : serial(port) {}
  void begin(std::function<void()> idle);
  bool scan(bool contents = true);
  bool writeInventory(const String &expectedUid, const String &json);
  bool available = false, present = false, writable = false;
  String uid, type = "none", payload, lastError, lastMessage;
  uint8_t targets = 0;
 private:
  HardwareSerial &serial;
  std::function<void()> idleHook;
  uint32_t deadline = 0;
  bool ntag216 = false;
  bool stepByte(uint8_t &byte, uint32_t end);
  bool command(const uint8_t *data, size_t size, uint8_t *response, size_t &length);
  bool select();
  bool exchange(const uint8_t *data, size_t size, uint8_t *response, size_t &length);
  bool readPages(uint8_t page, uint8_t *out);
  bool writePage(uint8_t page, const uint8_t *data);
  bool inspect();
  bool fail(const char *message);
};
