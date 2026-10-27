#include "Nfc.h"
#include "Pins.h"
#include "Domain.h"
#include <algorithm>

bool Nfc::fail(const char *message) { lastError = message; return false; }
bool Nfc::stepByte(uint8_t &byte, uint32_t end) {
  while (int32_t(end - millis()) > 0) {
    if (serial.available()) { byte = serial.read(); return true; }
    if (idleHook) idleHook();
    delay(1);
  }
  return false;
}
bool Nfc::command(const uint8_t *data, size_t size, uint8_t *response, size_t &length) {
  const uint32_t started = millis();
  const int32_t remaining = int32_t(deadline - started);
  if (!size || size > 80 || remaining <= 0) return false;
  while (serial.available()) serial.read();
  uint8_t frame[90] = {0, 0, 0xff, uint8_t(size + 1), uint8_t(0 - (size + 1)), 0xd4};
  uint8_t checksum = 0xd4;
  for (size_t i = 0; i < size; ++i) { frame[6 + i] = data[i]; checksum += data[i]; }
  frame[6 + size] = -checksum; frame[7 + size] = 0;
  serial.write(frame, size + 8);
  const uint32_t end = started + std::min<uint32_t>(350, remaining);
  bool ack = false;
  while (int32_t(end - millis()) > 0) {
    uint8_t byte = 0, a = 0, b = 0;
    do {
      if (!stepByte(byte, end)) goto timeout;
      if (a == 0 && b == 0 && byte == 0xff) break;
      a = b; b = byte;
    } while (true);
    uint8_t len = 0, lcs = 0;
    if (!stepByte(len, end) || !stepByte(lcs, end)) goto timeout;
    if (len == 0 && lcs == 0xff) {
      if (!stepByte(byte, end) || byte != 0) return false;
      ack = true; continue;
    }
    if (!ack || uint8_t(len + lcs) != 0 || len < 2 || len > 190) return false;
    uint8_t body[192];
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
      if (!stepByte(body[i], end)) goto timeout;
      sum += body[i];
    }
    if (!stepByte(byte, end) || uint8_t(sum + byte) != 0) return false;
    if (!stepByte(byte, end) || byte != 0 || body[0] != 0xd5 || body[1] != uint8_t(data[0] + 1)) return false;
    if (len - 2 > length) return false;
    length = len - 2; memcpy(response, body + 2, length); return true;
  }
timeout:
  // ACK aborts an unfinished PN532 command; discard a late frame on the next call.
  const uint8_t cancel[] = {0, 0, 0xff, 0, 0xff, 0};
  serial.write(cancel, sizeof(cancel));
  return false;
}
void Nfc::begin(std::function<void()> idle) {
  idleHook = idle;
  serial.begin(115200, SERIAL_8N1, Pins::nfcRx, Pins::nfcTx);
  const uint8_t wake[] = {0x55, 0x55, 0, 0, 0};
  serial.write(wake, sizeof(wake)); delay(5);
  deadline = millis() + 1500;
  uint8_t out[32]; size_t len = sizeof(out);
  const uint8_t version[] = {0x02};
  if (!command(version, sizeof(version), out, len) || len != 4 || out[0] != 0x32) {
    fail("PN532 not detected. Check HSU switches, power and RX/TX wiring."); return;
  }
  const uint8_t sam[] = {0x14, 1, 0x14, 1}; len = sizeof(out);
  if (!command(sam, sizeof(sam), out, len)) { fail("PN532 SAM configuration failed."); return; }
  const uint8_t retry[] = {0x32, 5, 0xff, 1, 0}; len = sizeof(out);
  available = command(retry, sizeof(retry), out, len);
  if (!available) fail("PN532 bounded polling configuration failed.");
}
bool Nfc::select() {
  uint8_t out[80]; size_t len = sizeof(out);
  const uint8_t selectCommand[] = {0x4a, 2, 0};
  if (!command(selectCommand, sizeof(selectCommand), out, len) || len < 1) return false;
  targets = out[0];
  if (!targets || len < 6 || out[1] != 1) return false;
  const uint8_t uidLength = out[5];
  if ((uidLength != 4 && uidLength != 7 && uidLength != 10) || len < size_t(6 + uidLength)) return false;
  uid = "";
  for (size_t i = 0; i < uidLength; ++i) { char hex[3]; snprintf(hex, sizeof(hex), "%02X", out[6 + i]); uid += hex; }
  type = (out[4] & 8) ? "MIFARE Classic (read-only UID)" : "ISO14443A (read-only UID)";
  present = true; ntag216 = false;
  if (out[4] != 0 || uidLength != 7) return true;
  uint8_t version[16]; size_t vlen = sizeof(version);
  const uint8_t getVersion[] = {0x60};
  if (exchange(getVersion, sizeof(getVersion), version, vlen) && Domain::ntag216Version(version, vlen)) {
    ntag216 = true; type = "NTAG216";
  }
  return true;
}
bool Nfc::exchange(const uint8_t *data, size_t size, uint8_t *response, size_t &length) {
  uint8_t request[82] = {0x40, 1};
  if (size > 78) return false;
  memcpy(request + 2, data, size);
  uint8_t out[192]; size_t outLen = sizeof(out);
  if (!command(request, size + 2, out, outLen) || outLen < 1 || out[0] != 0 || outLen - 1 > length) return false;
  length = outLen - 1; memcpy(response, out + 1, length); return true;
}
bool Nfc::readPages(uint8_t page, uint8_t *out) {
  const uint8_t read[] = {0x30, page}; size_t len = 16;
  return exchange(read, sizeof(read), out, len) && len == 16;
}
bool Nfc::writePage(uint8_t page, const uint8_t *data) {
  // Only NDEF user pages; never UID, CC, lock, password or configuration pages.
  if (page < 4 || page > 220) return false;
  uint8_t write[6] = {0xa2, page}; memcpy(write + 2, data, 4);
  uint8_t out[8]; size_t len = sizeof(out);
  return exchange(write, sizeof(write), out, len) && (len == 0 || (len == 1 && out[0] == 0x0a));
}
bool Nfc::inspect() {
  payload = ""; writable = false;
  if (!ntag216) { lastMessage = "UID only; proprietary filament data is not decoded."; return true; }
  uint8_t start[16], config[16], content[720] = {};
  if (!readPages(2, start) || !readPages(226, config) || !readPages(4, content)) return fail("Cannot read tag protection or NDEF pages.");
  const bool unlocked = Domain::ntag216Unlocked(start, config) && targets == 1;
  const bool blank = content[0] == 3 && content[1] == 0 && content[2] == 0xfe;
  if (blank) { writable = unlocked; lastMessage = "Blank NTAG216 inventory tag."; return true; }
  if (content[0] != 3 || content[1] != 0xff) { lastMessage = "Existing foreign tag content is read-only."; return true; }
  const size_t size = 4 + (size_t(content[2]) << 8) + content[3] + 1;
  if (size > sizeof(content) || size < 10) return fail("Unsupported NDEF length; tag is read-only.");
  for (size_t at = 16; at < size; at += 16)
    if (!readPages(4 + at / 4, content + at)) return fail("Tag moved or read timed out.");
  size_t offset = 0, length = 0;
  if (!Domain::decodeNdef(content, size, offset, length)) { lastMessage = "Foreign or damaged NDEF is read-only."; return true; }
  payload.reserve(length);
  for (size_t i = 0; i < length; ++i) payload += char(content[offset + i]);
  writable = unlocked; lastMessage = "Spoolancer inventory tag read.";
  return true;
}
bool Nfc::scan(bool contents) {
  lastError = ""; payload = ""; writable = false; present = false; uid = ""; type = "none"; targets = 0;
  if (!available) return fail("PN532 unavailable. Restart after checking the wiring.");
  deadline = millis() + 2500;
  if (!select()) { lastMessage = "No tag detected."; return false; }
  if (targets > 1) { lastMessage = "Multiple tags detected; present only one inventory tag."; return true; }
  return !contents || inspect();
}
bool Nfc::writeInventory(const String &expectedUid, const String &json) {
  if (!scan(true) || uid != expectedUid || targets != 1 || !writable)
    return fail("Write refused: rescan one unlocked blank/Spoolancer NTAG216 and confirm its UID.");
  uint8_t bytes[720] = {};
  const size_t size = Domain::encodeNdef(reinterpret_cast<const uint8_t *>(json.c_str()), json.length(), bytes, sizeof(bytes));
  if (!size) return fail("Inventory record is too large for the tag.");
  deadline = millis() + 10000;
  const uint8_t empty[] = {3, 0, 0xfe, 0};
  if (!writePage(4, empty)) return fail("Cannot start tag write; contents may be incomplete.");
  // Publish length last. Re-select/compare the UID for each 16-byte chunk.
  for (size_t at = 4; at < size; at += 4) {
    if ((at % 16) == 4) {
      if (!select() || uid != expectedUid || targets != 1 || !ntag216)
        return fail("Tag changed during write; contents may be incomplete. Rescan.");
    }
    if (!writePage(4 + at / 4, bytes + at)) return fail("Tag write failed; contents may be incomplete. Rescan.");
  }
  if (!select() || uid != expectedUid || targets != 1 || !ntag216 || !writePage(4, bytes))
    return fail("Tag commit failed; contents may be incomplete. Rescan.");
  if (!scan(true) || uid != expectedUid || payload != json)
    return fail("Tag read-back did not match; do not rely on this write. Rescan.");
  lastMessage = "Inventory tag written and read-back verified."; return true;
}
