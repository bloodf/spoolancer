#pragma once
#include <Arduino.h>
#include <functional>
#include "Domain.h"
#include "ReadTags.h"

class Nfc {
 public:
  explicit Nfc(HardwareSerial &port) : serial(port) {}
  void begin(std::function<void()> idle);
  // Background polls reuse known content only after a fresh same-UID check.
  bool scan(bool refreshContents = true);
  // Association needs only a fresh single UID, even if tag memory is protected.
  bool scanIdentity();
  bool writeInventory(const String &expectedUid, const String &json);
  // Any unlocked NTAG21x: NDEF record of another MIME type, or raw user pages from page 4.
  bool writeNdef(const String &expectedUid, const String &payload, const char *mime);
  bool writeNtagPages(const String &expectedUid, uint8_t firstPage, const uint8_t *data, size_t pages);
  // MIFARE Classic 1K sector 1 (blocks 4-6). Tries each Key A; optionally sets newKey as Key A/B.
  bool writeClassicSector1(const String &expectedUid, const uint8_t (*keys)[6], size_t keyCount,
                           const uint8_t *blocks, size_t blockCount, const uint8_t *newKey);
  bool classic1k() const { return present && targets == 1 && uidLength == 4 && (sak & 0x18) == 0x08 && sak != 0x09; } // 0x09 = MIFARE Mini.
  const uint8_t *uidData() const { return uidBytes; }
  // Reads a genuine Bambu Lab / Snapmaker Classic tag (derives sector keys from the UID).
  bool readClassic(const String &expectedUid, ReadTags::Decoded &out);
  // Byte-exact clone of a genuine tag onto a Gen2/FUID magic card (destination UID is set).
  bool cloneClassic(const uint8_t source[1024], const uint8_t sourceUid[4]);
  // Full 1024-byte Bambu image (data read with derived keys, trailers reconstructed) for cloning.
  bool readClassicDump(const String &expectedUid, uint8_t dump[1024]);
  size_t ndefCapacity() const { return layout.capacity(); }
  bool available = false, present = false, writable = false;
  // Unlocked NTAG21x or MIFARE Classic 1K that brand formats may overwrite after confirmation.
  bool rawWritable = false;
  String uid, type = "none", payload, lastError, lastMessage;
  uint8_t targets = 0;
 private:
  HardwareSerial &serial;
  std::function<void()> idleHook;
  uint32_t deadline = 0;
  Domain::NtagLayout layout;
  uint8_t supportedProtocols = 0, sak = 0, uidLength = 0, uidBytes[10] = {};
  bool serialStarted = false, couldBeNtag = false, typeKnown = false, contentsKnown = false;
  bool stepByte(uint8_t &byte, uint32_t end);
  bool command(const uint8_t *data, size_t size, uint8_t *response, size_t &length);
  bool select(bool identify = true, uint8_t technology = 0);
  bool find(bool identify);
  void identifyTag();
  bool exchange(const uint8_t *data, size_t size, uint8_t *response, size_t &length);
  bool readPages(uint8_t page, uint8_t *out);
  bool writePage(uint8_t page, const uint8_t *data);
  bool sameTag(const String &expectedUid);
  bool writeRange(const String &expectedUid, uint8_t firstPage, const uint8_t *data, size_t bytes, const uint8_t *placeholder);
  bool authClassic(uint8_t block, const uint8_t key[6]);
  bool readBlock(uint8_t block, uint8_t out[16]);
  bool readClassicSector(uint8_t sector, const uint8_t key[6], uint8_t out[48]);
  bool rawWriteBlock(uint8_t block, const uint8_t *data);
  bool writeBlock(uint8_t block, const uint8_t *data);
  bool inspect();
  bool fail(const char *message);
};
