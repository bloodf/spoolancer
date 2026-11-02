#include "Nfc.h"
#include "Pins.h"
#include "Domain.h"
#include <algorithm>
#include <cstring>

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
    uint8_t byte = 0, a = 1, b = 1; // Require two real zero bytes before 0xFF.
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
  available = present = writable = rawWritable = contentsKnown = typeKnown = couldBeNtag = false;
  uid = payload = lastError = lastMessage = ""; type = "none";
  targets = supportedProtocols = 0; layout = {};
  // Retries after a missing reader only repeat the handshake, not the UART setup.
  if (!serialStarted) { serial.begin(115200, SERIAL_8N1, Pins::nfcRx, Pins::nfcTx); serialStarted = true; }
  const uint8_t wake[] = {0x55, 0x55, 0, 0, 0};
  serial.write(wake, sizeof(wake)); delay(5);
  deadline = millis() + 1500;
  uint8_t out[32]; size_t len = sizeof(out);
  const uint8_t version[] = {0x02};
  if (!command(version, sizeof(version), out, len) || len != 4 || out[0] != 0x32) {
    fail("PN532 not detected. Check HSU switches, power and RX/TX wiring."); return;
  }
  supportedProtocols = out[3];
  const uint8_t sam[] = {0x14, 1, 0x14, 1}; len = sizeof(out);
  if (!command(sam, sizeof(sam), out, len)) { fail("PN532 SAM configuration failed."); return; }
  const uint8_t retry[] = {0x32, 5, 0xff, 1, 0}; len = sizeof(out);
  available = command(retry, sizeof(retry), out, len);
  if (!available) fail("PN532 bounded polling configuration failed.");
}
bool Nfc::select(bool identify, uint8_t technology) {
  uid = ""; type = "none"; targets = 0; present = false; layout = {};
  couldBeNtag = false; typeKnown = true; sak = 0; uidLength = 0;
  uint8_t out[80]; size_t len = sizeof(out);
  // UM0701-02 7.3.5: Type B needs AFI; FeliCa needs its five-byte polling payload.
  uint8_t selectCommand[] = {0x4a, uint8_t(technology == 4 ? 1 : 2), technology, 0, 0xff, 0xff, 1, 0};
  const size_t commandLength = technology == 3 ? 4 : (technology == 1 || technology == 2) ? 8 : 3;
  if (!command(selectCommand, commandLength, out, len) || len < 1) return false;
  targets = out[0];
  if (!targets) return false;
  if (targets > selectCommand[1]) return fail("Malformed PN532 target count.");
  if (targets > 1) return true; // No identity is accepted when anti-collision finds several tags.
  if (len < 2 || out[1] != 1) return fail("Malformed PN532 target response.");
  size_t uidOffset = 0, idLength = 0;
  const char *prefix = "";
  if (technology == 0) {
    if (len < 6) return fail("Truncated Type A identity.");
    idLength = out[5]; uidOffset = 6;
    if ((idLength != 4 && idLength != 7 && idLength != 10) || len < uidOffset + idLength)
      return fail("Invalid Type A UID length.");
    const size_t base = uidOffset + idLength;
    if ((len > base && (!out[base] || len != base + out[base])) || ((out[4] & 0x20) && len == base))
      return fail("Truncated Type A activation data.");
    sak = out[4]; uidLength = uint8_t(idLength);
    std::memcpy(uidBytes, out + uidOffset, idLength);
    type = (out[4] & 0x18) == 0x08 && idLength == 4 ? "MIFARE Classic 1K" :
           (out[4] & 8) ? "MIFARE Classic (read-only UID)" : "ISO14443A (read-only UID)";
    couldBeNtag = out[4] == 0 && idLength == 7;
  } else if (technology == 3) {
    if (len < 16 || out[2] != 0x50 || out[14] == 0 || len != size_t(15 + out[14]))
      return fail("Malformed Type B identity.");
    prefix = "B:"; uidOffset = 3; idLength = 4; type = "ISO14443B (read-only PUPI)";
  } else if (technology == 1 || technology == 2) {
    if (len < 20 || (out[2] != 18 && out[2] != 20) || len != size_t(2 + out[2]) || out[3] != 1)
      return fail("Malformed FeliCa identity.");
    prefix = "F:"; uidOffset = 4; idLength = 8; type = "FeliCa (read-only IDm)";
  } else if (technology == 4) {
    if (len != 8) return fail("Malformed Jewel identity.");
    prefix = "J:"; uidOffset = 4; idLength = 4; type = "Jewel/Topaz (read-only RID)";
  } else return false;
  uid = prefix;
  for (size_t i = 0; i < idLength; ++i) { char hex[3]; snprintf(hex, sizeof(hex), "%02X", out[uidOffset + i]); uid += hex; }
  present = true;
  typeKnown = !couldBeNtag;
  if (identify) identifyTag();
  return true;
}
bool Nfc::find(bool identify) {
  // Type A stays the fast path. Each missing family costs at most 350 ms and yields to the scale.
  for (uint8_t technology : {0, 3, 1, 2, 4}) {
    const uint8_t required = technology == 3 ? 2 : (technology == 1 || technology == 2) ? 4 : 1;
    if (!(supportedProtocols & required)) continue;
    if (select(identify, technology)) return true;
    if (targets || !lastError.isEmpty()) return false;
  }
  return false;
}
void Nfc::identifyTag() {
  if (!couldBeNtag) return;
  uint8_t version[16]; size_t vlen = sizeof(version);
  const uint8_t getVersion[] = {0x60};
  typeKnown = exchange(getVersion, sizeof(getVersion), version, vlen);
  if (typeKnown) layout = Domain::ntagLayout(version, vlen);
  if (layout.name) type = layout.name;
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
  if (page < 4 || !layout.capacity() || size_t(page - 4) * 4 + 4 > layout.capacity()) return false;
  uint8_t write[6] = {0xa2, page}; memcpy(write + 2, data, 4);
  uint8_t out[8]; size_t len = sizeof(out);
  return exchange(write, sizeof(write), out, len) && (len == 0 || (len == 1 && out[0] == 0x0a));
}
bool Nfc::inspect() {
  payload = ""; writable = rawWritable = false;
  if (!layout.capacity()) {
    rawWritable = classic1k();
    lastMessage = rawWritable ? "MIFARE Classic 1K; can be written for Creality CFS or Qidi Box, or read if it is a Bambu Lab or Snapmaker tag." :
                                "UID only; proprietary filament data is not decoded.";
    return true;
  }
  uint8_t start[16], config[16], content[720] = {};
  if (!readPages(2, start)) return fail("Cannot read tag capability container.");
  if (!Domain::ntagContainer(layout, start)) { lastMessage = "Unrecognized capability container; UID only."; return true; }
  if (!readPages(layout.lockPage, config) || !readPages(4, content)) return fail("Cannot read tag protection or NDEF pages; the UID can still be linked.");
  const bool unlocked = Domain::ntagUnlocked(layout, start, config) && targets == 1;
  rawWritable = unlocked;
  const bool blank = content[0] == 3 && content[1] == 0 && content[2] == 0xfe;
  if (blank) { writable = unlocked; lastMessage = "Blank inventory tag; record must fit its available memory."; return true; }
  size_t header = 0, value = 0;
  if (!Domain::ndefTlv(content, sizeof(content), header, value)) {
    lastMessage = unlocked ? "Existing foreign tag content; printer formats may replace it." : "Existing foreign tag content is read-only.";
    return true;
  }
  const size_t size = header + value + 1;
  if (size > sizeof(content) || size > layout.capacity()) { rawWritable = false; return fail("Unsupported NDEF length; tag is read-only."); }
  for (size_t at = 16; at < size; at += 16)
    if (!readPages(4 + at / 4, content + at)) return fail("Tag moved or read timed out.");
  size_t offset = 0, length = 0;
  if (!Domain::decodeNdef(content, size, offset, length)) {
    lastMessage = unlocked ? "Foreign NDEF; printer formats may replace it." : "Foreign or damaged NDEF is read-only.";
    return true;
  }
  payload.reserve(length);
  for (size_t i = 0; i < length; ++i) payload += char(content[offset + i]);
  writable = unlocked; lastMessage = "Spoolancer inventory tag read.";
  return true;
}
bool Nfc::scanIdentity() {
  contentsKnown = false; layout = {};
  lastError = ""; payload = ""; writable = rawWritable = false; present = false; uid = ""; type = "none"; targets = 0;
  if (!available) return fail("PN532 unavailable. Restart after checking the wiring.");
  deadline = millis() + 2500;
  if (!find(false)) { lastMessage = "No tag detected."; return false; }
  if (targets != 1) return fail("Multiple tags detected; present only one tag to link its UID.");
  lastMessage = "Tag UID read. Linking does not change tag contents.";
  return true;
}
bool Nfc::scan(bool refreshContents) {
  const bool reuse = !refreshContents && contentsKnown;
  const String previousUid = uid, previousType = type, previousPayload = payload, previousMessage = lastMessage;
  const bool previousWritable = writable, previousRaw = rawWritable;
  const auto previousLayout = layout;
  contentsKnown = false;
  lastError = ""; payload = ""; writable = rawWritable = false; present = false; uid = ""; type = "none"; targets = 0; layout = {};
  if (!available) return fail("PN532 unavailable. Restart after checking the wiring.");
  deadline = millis() + 2500;
  if (!find(refreshContents)) { lastMessage = "No tag detected."; return false; }
  if (targets > 1) { lastMessage = "Multiple tags detected; present only one inventory tag."; return true; }
  if (reuse && uid == previousUid) {
    type = previousType; payload = previousPayload; lastMessage = previousMessage;
    writable = previousWritable; rawWritable = previousRaw; layout = previousLayout;
    typeKnown = contentsKnown = true;
    return true;
  }
  if (!refreshContents) identifyTag();
  const bool inspected = inspect();
  contentsKnown = inspected && typeKnown;
  return inspected;
}
bool Nfc::sameTag(const String &expectedUid) {
  const auto expectedLayout = layout;
  return select() && uid == expectedUid && targets == 1 && layout.ccSize == expectedLayout.ccSize;
}
bool Nfc::writeRange(const String &expectedUid, uint8_t firstPage, const uint8_t *data, size_t bytes, const uint8_t *placeholder) {
  // A failed partial write must never make the next poll reuse the old record.
  contentsKnown = false;
  deadline = millis() + 10000;
  // The first page is published last, so an interrupted write never looks complete.
  if (!writePage(firstPage, placeholder)) return fail("Cannot start tag write; contents may be incomplete.");
  for (size_t at = 4; at < bytes; at += 4) {
    // Re-select and compare the UID for each 16-byte chunk.
    if ((at % 16) == 4 && !sameTag(expectedUid)) return fail("Tag changed during write; contents may be incomplete. Rescan.");
    if (!writePage(uint8_t(firstPage + at / 4), data + at)) return fail("Tag write failed; contents may be incomplete. Rescan.");
  }
  if (!sameTag(expectedUid) || !writePage(firstPage, data)) return fail("Tag commit failed; contents may be incomplete. Rescan.");
  for (size_t at = 0; at < bytes; at += 16) {
    uint8_t check[16];
    if (!readPages(uint8_t(firstPage + at / 4), check) || std::memcmp(check, data + at, std::min<size_t>(16, bytes - at)) != 0)
      return fail("Tag read-back did not match; do not rely on this write. Rescan.");
  }
  return true;
}
bool Nfc::writeInventory(const String &expectedUid, const String &json) {
  if (!scan(true) || uid != expectedUid || targets != 1 || !writable)
    return fail("Write refused: rescan one unlocked blank/Spoolancer NTAG213, NTAG215 or NTAG216 and confirm its UID.");
  uint8_t bytes[720] = {};
  const size_t capacity = std::min(sizeof(bytes), layout.capacity());
  const size_t size = Domain::encodeNdef(reinterpret_cast<const uint8_t *>(json.c_str()), json.length(), bytes, capacity);
  if (!size) {
    char message[144];
    snprintf(message, sizeof(message), "Record needs %u bytes; %s has %u NDEF bytes (650-byte JSON limit). Link its UID instead.",
             unsigned(Domain::ndefSize(std::strlen(Domain::ndefMime), json.length())), layout.name, unsigned(layout.capacity()));
    return fail(message);
  }
  const uint8_t empty[] = {3, 0, 0xfe, 0};
  if (!writeRange(expectedUid, 4, bytes, size, empty)) return false;
  if (!scan(true) || uid != expectedUid || payload != json)
    return fail("Tag read-back did not match; do not rely on this write. Rescan.");
  lastMessage = "Inventory tag written and read-back verified."; return true;
}
bool Nfc::writeNdef(const String &expectedUid, const String &record, const char *mime) {
  if (!scan(true) || uid != expectedUid || targets != 1 || !rawWritable || !layout.capacity())
    return fail("Write refused: rescan one unlocked NTAG213, NTAG215 or NTAG216 and confirm its UID.");
  uint8_t bytes[720] = {};
  const size_t capacity = std::min(sizeof(bytes), layout.capacity());
  const size_t size = Domain::encodeNdef(reinterpret_cast<const uint8_t *>(record.c_str()), record.length(), bytes, capacity, mime);
  if (!size) {
    char message[144];
    snprintf(message, sizeof(message), "Record needs %u bytes; %s has %u. Use an NTAG215 or NTAG216.",
             unsigned(Domain::ndefSize(std::strlen(mime), record.length())), layout.name, unsigned(layout.capacity()));
    return fail(message);
  }
  const uint8_t empty[] = {3, 0, 0xfe, 0};
  if (!writeRange(expectedUid, 4, bytes, size, empty)) return false;
  lastMessage = "Tag written and read-back verified."; return true;
}
bool Nfc::writeNtagPages(const String &expectedUid, uint8_t firstPage, const uint8_t *data, size_t pages) {
  if (!scan(true) || uid != expectedUid || targets != 1 || !rawWritable || !layout.capacity())
    return fail("Write refused: rescan one unlocked NTAG213, NTAG215 or NTAG216 and confirm its UID.");
  if (firstPage < 4 || !pages || size_t(firstPage - 4 + pages) * 4 > layout.capacity())
    return fail("This format does not fit the tag's user memory.");
  const uint8_t zero[4] = {};
  if (!writeRange(expectedUid, firstPage, data, pages * 4, zero)) return false;
  lastMessage = "Tag written and read-back verified."; return true;
}
bool Nfc::authClassic(uint8_t block, const uint8_t key[6]) {
  uint8_t request[12] = {0x60, block};
  std::memcpy(request + 2, key, 6); std::memcpy(request + 8, uidBytes, 4);
  uint8_t out[4]; size_t len = sizeof(out);
  return exchange(request, sizeof(request), out, len) && len == 0;
}
bool Nfc::writeBlock(uint8_t block, const uint8_t *data) {
  // Sector 1 only: never block 0 (manufacturer) or another sector's trailer.
  if (block < 4 || block > 7) return false;
  uint8_t request[18] = {0xa0, block};
  std::memcpy(request + 2, data, 16);
  uint8_t out[4]; size_t len = sizeof(out);
  return exchange(request, sizeof(request), out, len) && len == 0;
}
bool Nfc::writeClassicSector1(const String &expectedUid, const uint8_t (*keys)[6], size_t keyCount,
                              const uint8_t *blocks, size_t blockCount, const uint8_t *newKey) {
  contentsKnown = false; layout = {};
  if (!scanIdentity() || uid != expectedUid || !classic1k())
    return fail("Write refused: present one MIFARE Classic 1K tag with a 4-byte ID and confirm its UID.");
  if (!blockCount || blockCount > 3) return fail("Invalid sector data.");
  deadline = millis() + 10000;
  const uint8_t *used = nullptr;
  for (size_t k = 0; k < keyCount && !used; ++k) {
    // A failed authentication returns the card to IDLE; select it again before the next key.
    if (!sameTag(expectedUid)) return fail("Tag changed during write. Rescan.");
    if (authClassic(7, keys[k])) used = keys[k];
  }
  if (!used) return fail("Sector 1 rejected the known keys; this tag is protected by another key.");
  uint8_t trailer[16] = {};
  const bool rekey = newKey && std::memcmp(used, newKey, 6) != 0;
  if (rekey) {
    // Change keys only under transport access bits (FF 07 80); other values could lock the sector.
    if (!readPages(7, trailer) || trailer[6] != 0xff || trailer[7] != 0x07 || trailer[8] != 0x80)
      return fail("Unexpected sector access bits; tag was not modified.");
  }
  for (size_t b = 0; b < blockCount; ++b)
    if (!writeBlock(uint8_t(4 + b), blocks + b * 16)) return fail("Tag write failed; contents may be incomplete. Rescan.");
  if (rekey) {
    std::memcpy(trailer, newKey, 6); std::memcpy(trailer + 10, newKey, 6);
    if (!writeBlock(7, trailer)) return fail("Sector key update failed; the tag may need its old key. Rescan.");
  }
  const uint8_t *finalKey = rekey ? newKey : used;
  if (!sameTag(expectedUid) || !authClassic(7, finalKey)) return fail("Tag read-back authentication failed. Rescan.");
  for (size_t b = 0; b < blockCount; ++b) {
    uint8_t check[16];
    if (!readPages(uint8_t(4 + b), check) || std::memcmp(check, blocks + b * 16, 16) != 0)
      return fail("Tag read-back did not match; do not rely on this write. Rescan.");
  }
  lastMessage = "Tag written and read-back verified."; return true;
}
bool Nfc::readBlock(uint8_t block, uint8_t out[16]) { return readPages(block, out); }
bool Nfc::readClassicSector(uint8_t sector, const uint8_t key[6], uint8_t out[48]) {
  if (!authClassic(uint8_t(sector * 4), key)) return false;
  for (uint8_t b = 0; b < 3; ++b)
    if (!readBlock(uint8_t(sector * 4 + b), out + b * 16)) return false;
  return true;
}
bool Nfc::rawWriteBlock(uint8_t block, const uint8_t *data) {
  uint8_t request[18] = {0xa0, block};
  std::memcpy(request + 2, data, 16);
  uint8_t out[4]; size_t len = sizeof(out);
  return exchange(request, sizeof(request), out, len) && len == 0;
}
bool Nfc::readClassic(const String &expectedUid, ReadTags::Decoded &out) {
  out = {};
  if (!classic1k() || uid != expectedUid) return fail("Present one MIFARE Classic 1K tag with a 4-byte ID.");
  deadline = millis() + 6000;
  uint8_t data[1024];
  uint8_t bambu[16][6]; ReadTags::bambuKeys(uidBytes, bambu);
  // Reads `sectors` sectors into a block-aligned buffer; a failed auth idles the card, so reselect.
  auto readBrand = [&](std::function<void(int, uint8_t[6])> keyFor, uint8_t sectors) -> bool {
    std::memset(data, 0, sizeof(data));
    for (uint8_t s = 0; s < sectors; ++s) {
      if (!sameTag(expectedUid)) return false;
      uint8_t key[6]; keyFor(s, key);
      if (!readClassicSector(s, key, data + s * 64)) return false;
    }
    return true;
  };
  if (readBrand([&](int s, uint8_t k[6]) { std::memcpy(k, bambu[s], 6); }, 4)) {
    out = ReadTags::decodeBambu(data);
    if (out.valid) { type = "Bambu Lab (read-only)"; lastMessage = "Bambu Lab tag read. It is RSA-signed, so it cannot be rewritten, only cloned onto a magic card."; return true; }
  }
  if (readBrand([&](int s, uint8_t k[6]) { ReadTags::snapmakerKey(uidBytes, s, k); }, 3)) {
    out = ReadTags::decodeSnapmaker(data);
    if (out.valid) { type = "Snapmaker U1 (read-only)"; lastMessage = "Snapmaker tag read. It is RSA-signed and cannot be rewritten or cloned."; return true; }
  }
  return fail("This MIFARE Classic tag is not a recognized Bambu Lab or Snapmaker filament tag.");
}
bool Nfc::readClassicDump(const String &expectedUid, uint8_t dump[1024]) {
  // Builds a full Bambu clone image: data blocks read with derived keys, trailers reconstructed
  // (Key A = derived, access = 87 87 87 69, Key B = 0) exactly as genuine Bambu tags store them.
  if (!classic1k() || uid != expectedUid) return false;
  deadline = millis() + 10000;
  std::memset(dump, 0, 1024);
  uint8_t keys[16][6]; ReadTags::bambuKeys(uidBytes, keys);
  for (uint8_t s = 0; s < 16; ++s) {
    if (!sameTag(expectedUid)) return false;
    if (!readClassicSector(s, keys[s], dump + s * 64)) return false;
    uint8_t *trailer = dump + s * 64 + 48;
    std::memcpy(trailer, keys[s], 6);
    trailer[6] = 0x87; trailer[7] = 0x87; trailer[8] = 0x87; trailer[9] = 0x69;
    std::memset(trailer + 10, 0, 6);
  }
  return true;
}
bool Nfc::cloneClassic(const uint8_t source[1024], const uint8_t sourceUid[4]) {
  contentsKnown = false; layout = {};
  if (!scanIdentity() || !classic1k()) return fail("Present the blank magic card (MIFARE Classic 1K, 4-byte ID).");
  deadline = millis() + 15000;
  const uint8_t defaults[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  const uint8_t transport[6] = {0xd3, 0xf7, 0xd3, 0xf7, 0xd3, 0xf7};
  const String destUid = uid;
  // Block 0 first: write the source UID and verify, so a non-magic card is never rekeyed.
  const uint8_t *openers0[] = {defaults, transport};
  bool authed = false;
  for (const uint8_t *key : openers0) { if (sameTag(destUid) && authClassic(0, key)) { authed = true; break; } }
  if (!authed) return fail("Magic card sector 0 rejected the default key.");
  if (!rawWriteBlock(0, source)) return fail("This card did not accept a block-0 write; use a Gen2/CUID or FUID magic card. Nothing was changed.");
  if (!scanIdentity()) return fail("Card lost after the block-0 write. Rescan.");
  char wrote[11]; snprintf(wrote, sizeof(wrote), "%02X%02X%02X%02X", sourceUid[0], sourceUid[1], sourceUid[2], sourceUid[3]);
  if (uid != wrote) return fail("The card did not take the source ID; it is not a UID-writable magic card. Nothing else was changed.");
  uint8_t bambu[16][6]; ReadTags::bambuKeys(sourceUid, bambu);
  for (uint8_t s = 0; s < 16; ++s) {
    const uint8_t *openers[] = {defaults, transport, bambu[s]};
    bool opened = false;
    for (const uint8_t *key : openers) { if (sameTag(wrote) && authClassic(uint8_t(s * 4), key)) { opened = true; break; } }
    if (!opened) return fail("A card sector rejected every known key; the clone is incomplete. Use a fresh magic card.");
    for (uint8_t b = (s == 0 ? 1 : 0); b < 4; ++b) // Block 0 already written; include the trailer.
      if (!rawWriteBlock(uint8_t(s * 4 + b), source + (s * 4 + b) * 16)) return fail("Clone write failed; the card is incomplete.");
  }
  ReadTags::Decoded check;
  if (!readClassic(wrote, check) || !check.valid) return fail("Clone read-back failed; do not rely on this card.");
  lastMessage = "Magic card cloned and read-back verified."; return true;
}
