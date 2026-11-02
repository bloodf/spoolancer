#include "Nfc.h"
#include "Domain.h"
#include "BrandTags.h"
#include "ReadTags.h"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>

namespace {
uint32_t now = 0;
struct ReaderFixture {
  HardwareSerial serial;
  std::array<uint8_t, 924> memory{};
  std::array<uint8_t, 7> uid{4, 1, 2, 3, 4, 5, 6};
  std::array<uint8_t, 8> version{0, 4, 4, 2, 1, 0, 0x13, 3};
  uint8_t sak = 0;
  bool present = true, multiple = false, dropSelect = false, dropVersion = false, denyReads = false;
  int reads = 0, versions = 0, writes = 0, failAfterWrites = -1;
  ReaderFixture(uint8_t storageCode = 0x13) {
    version[6] = storageCode;
    const auto tag = layout();
    memory[12] = 0xe1; memory[13] = 0x10; memory[14] = tag.ccSize; memory[size_t(tag.lockPage) * 4 + 7] = 0xff;
    record("old record");
    serial.onWrite = [this](const uint8_t *frame, size_t size) {
      if (size < 8 || frame[2] != 0xff) return; // Wake/abort sequences.
      assert(frame[3] + 7 == size);
      const uint8_t *request = frame + 6;
      std::vector<uint8_t> response;
      if (request[0] == 0x02) response = {0x32, 1, 6, 7};
      else if (request[0] == 0x4a) {
        if (dropSelect) return;
        if (!present) response = {0};
        else {
          response = {uint8_t(multiple ? 2 : 1), 1, 0, 0, sak, 7};
          response.insert(response.end(), uid.begin(), uid.end());
        }
      } else if (request[0] == 0x40) {
        response = {0};
        if (request[2] == 0x60) {
          ++versions;
          if (dropVersion) return;
          response.insert(response.end(), version.begin(), version.end());
        } else if (request[2] == 0x30) {
          ++reads;
          const size_t address = size_t(request[3]) * 4;
          assert(address + 16 <= size_t(layout().lockPage + 5) * 4);
          if (denyReads) response = {1};
          else response.insert(response.end(), memory.begin() + address, memory.begin() + address + 16);
        } else if (request[2] == 0xa2) {
          if (failAfterWrites >= 0 && writes >= failAfterWrites) response = {1};
          else {
            ++writes;
            const size_t address = size_t(request[3]) * 4;
            assert(request[3] >= 4 && address + 4 <= 16 + layout().capacity());
            std::memcpy(memory.data() + address, request + 4, 4);
            response.push_back(0x0a);
          }
        } else assert(false);
      } else assert(request[0] == 0x14 || request[0] == 0x32);
      reply(request[0], response);
    };
  }
  Domain::NtagLayout layout() const { return Domain::ntagLayout(version.data(), version.size()); }
  void reply(uint8_t command, const std::vector<uint8_t> &response) {
    const uint8_t ack[] = {0, 0, 0xff, 0, 0xff, 0};
    for (auto byte : ack) serial.incoming.push_back(byte);
    const uint8_t size = response.size() + 2;
    const uint8_t prefix[] = {0, 0, 0xff, size, uint8_t(-size), 0xd5, uint8_t(command + 1)};
    for (auto byte : prefix) serial.incoming.push_back(byte);
    uint8_t checksum = 0xd5 + command + 1;
    for (auto byte : response) { serial.incoming.push_back(byte); checksum += byte; }
    serial.incoming.push_back(uint8_t(-checksum)); serial.incoming.push_back(0);
  }
  void record(const char *value) {
    std::fill(memory.begin() + 16, memory.begin() + 16 + layout().capacity(), 0);
    assert(Domain::encodeNdef(reinterpret_cast<const uint8_t *>(value), std::strlen(value), memory.data() + 16, layout().capacity()));
  }
};
}
uint32_t millis() { return now; }
void delay(unsigned milliseconds) { now += milliseconds; }

static void tagProfiles() {
  for (const uint8_t code : {0x0f, 0x11, 0x13}) {
    ReaderFixture fixture(code);
    Nfc reader(fixture.serial); reader.begin([] {});
    assert(reader.scan(true) && reader.type == fixture.layout().name && reader.writable);
    assert(reader.ndefCapacity() == fixture.layout().capacity());
    const auto original = fixture.memory;
    const String expectedUid = reader.uid;
    const size_t typeLen = std::strlen(Domain::ndefMime);
    size_t payloadLimit = 1;
    while (payloadLimit < Domain::maxPayload && Domain::ndefSize(typeLen, payloadLimit + 1) <= fixture.layout().capacity()) ++payloadLimit;
    const String largest(payloadLimit, 'x');
    assert(reader.writeInventory(expectedUid, largest) && reader.payload == largest);
    const int writes = fixture.writes;
    const String oversized(payloadLimit + 1, 'y');
    assert(!reader.writeInventory(expectedUid, oversized));
    assert(reader.lastError.find("Record needs") != String::npos && fixture.writes == writes);
    assert(reader.scan(true) && reader.payload == largest); // No length invalidation on rejection.
    assert(std::equal(original.begin(), original.begin() + 16, fixture.memory.begin()));
    const auto untouched = 16 + fixture.layout().capacity();
    assert(std::equal(original.begin() + untouched, original.end(), fixture.memory.begin() + untouched));

    // Same-UID background polling keeps metadata; explicit access rereads CC and locks.
    const int reads = fixture.reads, versions = fixture.versions;
    assert(reader.scan(false) && fixture.reads == reads && fixture.versions == versions);
    fixture.memory[14] ^= 1;
    assert(reader.scan(true) && !reader.writable && reader.payload.isEmpty());
    assert(!reader.writeInventory(expectedUid, "replacement") && fixture.writes == writes);
    fixture.memory[14] ^= 1;
    for (size_t address : {size_t(10), size_t(11), size_t(15), size_t(fixture.layout().lockPage) * 4,
                           size_t(fixture.layout().lockPage) * 4 + 1, size_t(fixture.layout().lockPage) * 4 + 2}) {
      fixture.memory[address] = 1;
      assert(!reader.writeInventory(expectedUid, "replacement") && fixture.writes == writes);
      fixture.memory[address] = 0;
    }
    const size_t config = size_t(fixture.layout().lockPage) * 4;
    for (const auto field : {std::pair<size_t, uint8_t>{4, 0x40}, {7, 4}, {8, 0x80}, {8, 0x40}}) {
      const auto old = fixture.memory[config + field.first];
      fixture.memory[config + field.first] = field.second;
      assert(!reader.writeInventory(expectedUid, "replacement") && fixture.writes == writes);
      fixture.memory[config + field.first] = old;
    }
    fixture.memory[15] = 0x0f;
    assert(reader.scan(true) && reader.payload == largest && !reader.writable);
    fixture.memory[15] = 0;

    // Never read an advertised NDEF beyond this chip's capacity, nor accept missing terminators.
    fixture.memory[17] = 0xff; fixture.memory[18] = 0xff; fixture.memory[19] = 0xff;
    assert(!reader.scan(true) && !reader.writable);
    fixture.record("small record");
    const size_t terminator = 16 + Domain::ndefSize(typeLen, std::strlen("small record")) - 1;
    fixture.memory[terminator] = 0;
    assert(reader.scan(true) && !reader.writable && reader.payload.isEmpty());
    assert(!reader.writeInventory(expectedUid, "replacement") && fixture.writes == writes);

    // A valid but foreign NDEF and the NTAG213 factory lock-control TLV stay untouched.
    const uint8_t foreign[] = {3, 4, 0xd1, 1, 0, 'T', 0xfe};
    std::memcpy(fixture.memory.data() + 16, foreign, sizeof(foreign));
    assert(reader.scan(true) && !reader.writable);
    assert(!reader.writeInventory(expectedUid, "replacement") && fixture.writes == writes);
    const uint8_t factory[] = {1, 3, 0xa0, 0x0c, 0x34, 3, 0, 0xfe};
    std::memcpy(fixture.memory.data() + 16, factory, sizeof(factory));
    assert(reader.scan(true) && !reader.writable);
    assert(!reader.writeInventory(expectedUid, "replacement") && fixture.writes == writes);

    fixture.memory[16] = 3; fixture.memory[17] = 0; fixture.memory[18] = 0xfe;
    assert(reader.scan(true) && reader.writable && reader.payload.isEmpty());
    assert(reader.writeInventory(expectedUid, "small full-format record"));
  }
}

static void identityOnly() {
  ReaderFixture fixture;
  Nfc reader(fixture.serial); reader.begin([] {});
  fixture.denyReads = true;
  assert(!reader.scan(true) && reader.present && !reader.uid.isEmpty() && !reader.writable);
  const auto reads = fixture.reads, versions = fixture.versions;
  assert(reader.scanIdentity() && reader.targets == 1 && !reader.uid.isEmpty());
  assert(fixture.reads == reads && fixture.versions == versions && !reader.writable && reader.payload.isEmpty());
  fixture.multiple = true;
  assert(!reader.scanIdentity() && !reader.writable);
  fixture.multiple = false; fixture.present = false;
  assert(!reader.scanIdentity() && !reader.present && reader.uid.isEmpty());
  fixture.present = true; fixture.sak = 8;
  assert(reader.scanIdentity() && reader.type == "MIFARE Classic (read-only UID)");
  fixture.sak = 0; fixture.denyReads = false;
  fixture.version[5] = 1; // Unsupported revision is UID-only, never treated as a known chip.
  assert(reader.scan(true) && !reader.writable && reader.ndefCapacity() == 0);
  assert(!reader.writeInventory(reader.uid, "replacement") && fixture.writes == 0);
}

static void otherProtocols() {
  struct Example { uint8_t technology; std::vector<uint8_t> response; const char *uid; };
  // PN532 UM0701-02 7.3.5 published response vectors (plus FeliCa at 424 kbps).
  const std::vector<Example> examples = {
    {0, {1, 1, 4, 0, 8, 4, 0x92, 0x2e, 0x58, 0x32}, "922E5832"},
    {3, {1, 1, 0x50, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 1, 1}, "B:01020304"},
    {1, {1, 1, 0x14, 1, 1, 1, 6, 1, 0x67, 2, 0xa5, 0x15, 3, 0, 0x4b, 2, 0x4f, 0x49, 0x8a, 0x8a, 0xff, 0xff}, "F:010106016702A515"},
    {2, {1, 1, 0x12, 1, 1, 1, 6, 1, 0x67, 2, 0xa5, 0x15, 3, 0, 0x4b, 2, 0x4f, 0x49, 0x8a, 0x8a}, "F:010106016702A515"},
    {4, {1, 1, 4, 0, 0x92, 0x2e, 0x58, 0x32}, "J:922E5832"},
    // Type 4A activation includes ATS; 10-byte Type A identifiers remain accepted.
    {0, {1, 1, 4, 0, 0x20, 7, 4, 1, 2, 3, 4, 5, 6, 2, 0x80}, "04010203040506"},
    {0, {1, 1, 4, 0, 0, 10, 4, 1, 2, 3, 4, 5, 6, 7, 8, 9}, "04010203040506070809"}
  };
  for (const auto &example : examples) {
    ReaderFixture fixture;
    auto originalWrite = fixture.serial.onWrite;
    std::vector<uint8_t> response = example.response, polled;
    uint8_t features = 7;
    int memoryCommands = 0;
    fixture.serial.onWrite = [&](const uint8_t *frame, size_t size) {
      if (size < 8 || frame[2] != 0xff) return;
      const uint8_t *request = frame + 6;
      if (request[0] == 2) { fixture.reply(2, {0x32, 1, 6, features}); return; }
      if (request[0] == 0x4a) {
        const uint8_t technology = request[2];
        polled.push_back(technology);
        const size_t length = frame[3] - 1;
        assert(request[1] == (technology == 4 ? 1 : 2));
        if (technology == 3) assert(length == 4 && request[3] == 0);
        else if (technology == 1 || technology == 2) {
          const uint8_t poll[] = {0, 0xff, 0xff, 1, 0};
          assert(length == 8 && std::memcmp(request + 3, poll, sizeof(poll)) == 0);
        } else assert(length == 3);
        fixture.reply(0x4a, technology == example.technology ? response : std::vector<uint8_t>{0});
        return;
      }
      if (request[0] == 0x40) ++memoryCommands;
      originalWrite(frame, size);
    };
    Nfc reader(fixture.serial); reader.begin([] {});
    assert(reader.scanIdentity() && reader.uid == example.uid && !reader.writable);
    assert(memoryCommands == 0 && polled.front() == 0 && polled.back() == example.technology);
    assert(reader.scan(false) && reader.uid == example.uid && !reader.writable && reader.ndefCapacity() == 0);
    assert(!reader.writeInventory(reader.uid, "replacement") && fixture.writes == 0 && memoryCommands == 0);

    for (size_t count = 0; count < example.response.size(); ++count) {
      response.assign(example.response.begin(), example.response.begin() + count);
      assert(!reader.scanIdentity() && !reader.present && reader.uid.isEmpty() && !reader.writable);
    }
    response = example.response; response[0] = 2;
    polled.clear();
    assert(!reader.scanIdentity() && !reader.present && reader.uid.isEmpty());
    assert(polled.back() == example.technology); // No fallback after ambiguous targets.
    response = example.response; response[0] = 3;
    assert(!reader.scanIdentity() && !reader.present);
    response = example.response; response[1] = 2;
    assert(!reader.scanIdentity() && !reader.present);
    response = example.response;
    if (example.technology == 3) { response[2] = 0; assert(!reader.scanIdentity()); }
    if (example.technology == 1 || example.technology == 2) {
      response[3] = 0; assert(!reader.scanIdentity());
      response = example.response; response[2] = 19; assert(!reader.scanIdentity());
    }

    // Do not poll a family the PN532 firmware reports as unsupported.
    response = example.response;
    features = example.technology == 3 ? 5 : (example.technology == 1 || example.technology == 2) ? 3 : 6;
    reader.begin([] {}); polled.clear();
    assert(!reader.scanIdentity());
    assert(std::find(polled.begin(), polled.end(), example.technology) == polled.end());
  }
}

namespace {
// MIFARE Classic 1K behind a PN532: per-sector Key A authentication, 16-byte READ/WRITE.
struct ClassicFixture {
  HardwareSerial serial;
  std::array<uint8_t, 4> uid{0xde, 0xad, 0xbe, 0xef};
  std::array<uint8_t, 1024> memory{};
  int authenticated = -1, writes = 0;
  bool magic = false; // Gen2 card: accepts a block-0 write that changes the UID.
  ClassicFixture() {
    for (int sector = 0; sector < 16; ++sector) {
      uint8_t *trailer = memory.data() + sector * 64 + 48;
      const uint8_t defaults[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0x80, 0x69, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
      std::memcpy(trailer, defaults, 16);
    }
    serial.onWrite = [this](const uint8_t *frame, size_t size) {
      if (size < 8 || frame[2] != 0xff) return;
      const uint8_t *request = frame + 6;
      const size_t length = frame[3] - 1;
      std::vector<uint8_t> response;
      if (request[0] == 0x02) response = {0x32, 1, 6, 7};
      else if (request[0] == 0x4a) {
        authenticated = -1;
        response = {1, 1, 0, 4, 8, 4};
        response.insert(response.end(), uid.begin(), uid.end());
      } else if (request[0] == 0x40) {
        const uint8_t op = request[2], block = request[3];
        if (op == 0x60) {
          assert(length == 14 && std::memcmp(request + 10, uid.data(), 4) == 0);
          const bool ok = std::memcmp(request + 4, memory.data() + (block / 4) * 64 + 48, 6) == 0;
          authenticated = ok ? block / 4 : -1;
          response = {uint8_t(ok ? 0 : 0x14)};
        } else if (op == 0x30) {
          if (authenticated != block / 4) response = {0x14};
          else {
            response = {0};
            response.insert(response.end(), memory.begin() + block * 16, memory.begin() + block * 16 + 16);
            if (block % 4 == 3) std::fill(response.begin() + 1, response.begin() + 7, 0); // Key A never reads back.
          }
        } else if (op == 0xa0) {
          assert(length == 20);
          if (block == 0 && !magic) response = {0x14}; // Non-magic cards reject block-0 writes.
          else if (authenticated != block / 4) response = {0x14};
          else {
            std::memcpy(memory.data() + block * 16, request + 4, 16); ++writes; response = {0};
            if (block == 0) std::memcpy(uid.data(), memory.data(), 4);
          }
        } else assert(false);
      } else assert(request[0] == 0x14 || request[0] == 0x32);
      const uint8_t ack[] = {0, 0, 0xff, 0, 0xff, 0};
      for (auto byte : ack) serial.incoming.push_back(byte);
      const uint8_t total = response.size() + 2;
      const uint8_t prefix[] = {0, 0, 0xff, total, uint8_t(-total), 0xd5, uint8_t(request[0] + 1)};
      for (auto byte : prefix) serial.incoming.push_back(byte);
      uint8_t checksum = 0xd5 + request[0] + 1;
      for (auto byte : response) { serial.incoming.push_back(byte); checksum += byte; }
      serial.incoming.push_back(uint8_t(-checksum)); serial.incoming.push_back(0);
    };
  }
};
}

static void classicWrites() {
  using namespace BrandTags;
  ClassicFixture fixture;
  Nfc reader(fixture.serial); reader.begin([] {});
  assert(reader.scan(true) && reader.classic1k() && reader.rawWritable && !reader.writable);
  assert(reader.type == "MIFARE Classic 1K" && reader.uid == "DEADBEEF");
  uint8_t key[6], blocks[48];
  crealityKey(reader.uidData(), key);
  assert(crealityBlocks(*profileFor("PLA"), "#FF0000", 330, 1, blocks));
  const uint8_t defaults[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  const uint8_t keys[][6] = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, {key[0], key[1], key[2], key[3], key[4], key[5]}};
  assert(reader.writeClassicSector1("DEADBEEF", keys, 2, blocks, 3, key));
  assert(std::memcmp(fixture.memory.data() + 64, blocks, 48) == 0);
  const uint8_t *trailer = fixture.memory.data() + 112;
  assert(std::memcmp(trailer, key, 6) == 0 && std::memcmp(trailer + 10, key, 6) == 0);
  assert(trailer[6] == 0xff && trailer[7] == 0x07 && trailer[8] == 0x80 && trailer[9] == 0x69);
  for (int sector = 0; sector < 16; ++sector)
    if (sector != 1) assert(std::memcmp(fixture.memory.data() + sector * 64 + 48, defaults, 6) == 0);

  // Rewriting an already keyed Creality tag authenticates with the derived key.
  assert(crealityBlocks(*profileFor("PETG"), "#00FF00", 165, 2, blocks));
  const int writes = fixture.writes;
  assert(reader.writeClassicSector1("DEADBEEF", keys, 2, blocks, 3, key));
  assert(fixture.writes == writes + 3 && std::memcmp(fixture.memory.data() + 64, blocks, 48) == 0);

  // A Creality-keyed tag can be returned to default keys (Qidi Box reuse).
  {
    ClassicFixture reused;
    std::memcpy(reused.memory.data() + 112, key, 6); std::memcpy(reused.memory.data() + 122, key, 6);
    Nfc reusedReader(reused.serial); reusedReader.begin([] {});
    const uint8_t crealityOnly[][6] = {{key[0], key[1], key[2], key[3], key[4], key[5]}};
    uint8_t qidiData[16] = {1, 18};
    assert(reusedReader.writeClassicSector1("DEADBEEF", crealityOnly, 1, qidiData, 1, defaults));
    assert(std::memcmp(reused.memory.data() + 112, defaults, 6) == 0 && std::memcmp(reused.memory.data() + 122, defaults, 6) == 0);
    assert(std::memcmp(reused.memory.data() + 64, qidiData, 16) == 0);
  }

  // Unknown keys, wrong UID, or non-transport access bits leave the tag untouched.
  const auto before = fixture.memory;
  assert(!reader.writeClassicSector1("DEADBEEF", keys, 1, blocks, 3, key) && fixture.memory == before);
  assert(!reader.writeClassicSector1("01020304", keys, 2, blocks, 3, key) && fixture.memory == before);
  ClassicFixture locked;
  locked.memory[112 + 7] = 0x77; locked.memory[112 + 8] = 0x8f; // Readable but different access bits.
  Nfc lockedReader(locked.serial); lockedReader.begin([] {});
  const auto lockedBefore = locked.memory;
  assert(!lockedReader.writeClassicSector1("DEADBEEF", keys, 2, blocks, 3, key) && locked.memory == lockedBefore);

  // Qidi: block 4 only with the default key, trailer unchanged.
  ClassicFixture qidiTag;
  Nfc qidiReader(qidiTag.serial); qidiReader.begin([] {});
  uint8_t block[16];
  assert(qidiBlock(*profileFor("PLA"), "#FF0000", block));
  const uint8_t qidiKeys[][6] = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, {0xd3, 0xf7, 0xd3, 0xf7, 0xd3, 0xf7}};
  const auto qidiBefore = qidiTag.memory;
  assert(qidiReader.writeClassicSector1("DEADBEEF", qidiKeys, 2, block, 1, nullptr));
  assert(std::memcmp(qidiTag.memory.data() + 64, block, 16) == 0 && qidiTag.writes == 1);
  assert(std::equal(qidiBefore.begin() + 80, qidiBefore.end(), qidiTag.memory.begin() + 80));
}

static void brandNtagWrites() {
  using namespace BrandTags;
  for (const uint8_t code : {0x0f, 0x11, 0x13}) {
    ReaderFixture fixture(code);
    Nfc reader(fixture.serial); reader.begin([] {});
    assert(reader.scan(true) && reader.rawWritable);
    const String uid = reader.uid;
    uint8_t anycubic[anycubicPages * 4];
    assert(anycubicData(*profileFor("PLA"), "#FF0000", 330, 1000, 1.75, anycubic));
    assert(reader.writeNtagPages(uid, anycubicFirstPage, anycubic, anycubicPages));
    assert(std::memcmp(fixture.memory.data() + 16, anycubic, sizeof(anycubic)) == 0);
    // Foreign raw data stays replaceable, but never as the Spoolancer inventory record.
    assert(reader.scan(true) && reader.rawWritable && !reader.writable && reader.payload.isEmpty());
    uint8_t elegoo[elegooPages * 4];
    assert(elegooData(*profileFor("PETG"), "#00FF00", 1000, 1.75, elegoo));
    assert(reader.writeNtagPages(uid, elegooFirstPage, elegoo, elegooPages));
    assert(std::memcmp(fixture.memory.data() + 16, elegoo, sizeof(elegoo)) == 0);

    char json[400];
    const size_t n = openspoolJson(*profileFor("PLA"), "Generic", "#FF0000", 1000, 1.75, json, sizeof(json));
    const bool fits = fixture.layout().capacity() >= Domain::ndefSize(std::strlen(openspoolMime), n);
    const int writes = fixture.writes;
    assert(reader.writeNdef(uid, String(json), openspoolMime) == fits);
    if (fits) {
      size_t offset = 0, length = 0;
      assert(Domain::decodeNdef(fixture.memory.data() + 16, fixture.layout().capacity(), offset, length, openspoolMime));
      assert(length == n && std::memcmp(fixture.memory.data() + 16 + offset, json, n) == 0);
    } else assert(fixture.writes == writes && reader.lastError.find("NTAG215") != String::npos);

    // Locked or password-protected tags are refused before any write.
    fixture.memory[10] = 1;
    const int lockedWrites = fixture.writes;
    assert(reader.scan(true) && !reader.rawWritable);
    assert(!reader.writeNtagPages(uid, anycubicFirstPage, anycubic, anycubicPages) && fixture.writes == lockedWrites);
    assert(!reader.writeNdef(uid, String(json), openspoolMime) && fixture.writes == lockedWrites);
    fixture.memory[10] = 0;
    // A different UID is refused.
    assert(!reader.writeNtagPages("04000000000000", anycubicFirstPage, anycubic, anycubicPages) && fixture.writes == lockedWrites);
  }
}

void readTagTests();
static void bambuReadClone() {
  using namespace ReadTags;
  // A genuine Bambu tag: sectors keyed with the derived keys, filament data in blocks 2/4/5/14.
  ClassicFixture src;
  uint8_t keys[16][6]; bambuKeys(src.uid.data(), keys);
  for (int s = 0; s < 16; ++s) {
    std::memcpy(src.memory.data() + s * 64 + 48, keys[s], 6);
    src.memory[s * 64 + 48 + 6] = 0x87; src.memory[s * 64 + 48 + 7] = 0x87;
    src.memory[s * 64 + 48 + 8] = 0x87; src.memory[s * 64 + 48 + 9] = 0x69;
  }
  std::memcpy(src.memory.data(), src.uid.data(), 4); // Block 0 carries the UID, as on a real tag.
  std::memcpy(src.memory.data() + 4 * 16, "PLA Basic", 9);
  const uint8_t block5[] = {0xff, 0x00, 0x00, 0xff, 0xe8, 0x03};
  std::memcpy(src.memory.data() + 5 * 16, block5, sizeof(block5));
  src.memory[14 * 16 + 4] = 0x4a; src.memory[14 * 16 + 5] = 0x01;

  Nfc reader(src.serial); reader.begin([] {});
  assert(reader.scan(true) && reader.classic1k() && reader.rawWritable);
  Decoded decoded;
  assert(reader.readClassic("DEADBEEF", decoded) && decoded.valid);
  assert(std::string(decoded.brand) == "Bambu Lab" && std::string(decoded.material) == "PLA Basic");
  assert(std::string(decoded.color) == "#FF0000" && decoded.weightG == 1000 && decoded.lengthM == 330);
  // A non-Bambu Classic (FF keys) is reported as unrecognized, not decoded.
  ClassicFixture plain;
  Nfc plainReader(plain.serial); plainReader.begin([] {});
  Decoded none;
  assert(plainReader.scan(true) && !plainReader.readClassic("DEADBEEF", none) && !none.valid);

  // Build the full clone image and clone it onto a magic card.
  uint8_t dump[1024];
  assert(reader.scanIdentity() && reader.readClassicDump("DEADBEEF", dump));
  assert(std::memcmp(dump, src.memory.data(), 48) == 0);               // Block 0 + data read back.
  assert(std::memcmp(dump + 48, keys[0], 6) == 0 && dump[48 + 6] == 0x87); // Reconstructed trailer.

  ClassicFixture magic; magic.magic = true; magic.uid = {0x11, 0x22, 0x33, 0x44};
  Nfc writer(magic.serial); writer.begin([] {});
  const uint8_t sourceUid[4] = {0xde, 0xad, 0xbe, 0xef};
  assert(writer.cloneClassic(dump, sourceUid));
  assert(std::memcmp(magic.uid.data(), sourceUid, 4) == 0);            // UID changed to source.
  assert(std::memcmp(magic.memory.data(), dump, 1024) == 0);          // Byte-exact clone.

  // A non-magic card is refused at the block-0 write, leaving its sectors untouched.
  ClassicFixture normal; normal.uid = {0x55, 0x66, 0x77, 0x88};
  Nfc normalWriter(normal.serial); normalWriter.begin([] {});
  const auto before = normal.memory;
  assert(!normalWriter.cloneClassic(dump, sourceUid) && normal.memory == before);
  assert(std::memcmp(normal.uid.data(), (const uint8_t[]){0x55,0x66,0x77,0x88}, 4) == 0);
}

int main() {
  readTagTests(); classicWrites(); brandNtagWrites(); bambuReadClone();
  ReaderFixture fixture;
  unsigned idleCalls = 0;
  Nfc reader(fixture.serial); reader.begin([&] { ++idleCalls; });
  assert(reader.available && reader.scan(false));
  assert(reader.present && reader.writable && reader.payload == "old record");
  const int initialReads = fixture.reads, initialVersions = fixture.versions;
  assert(initialReads >= 3);
  assert(reader.scan(false));
  assert(fixture.reads == initialReads && fixture.versions == initialVersions);
  assert(reader.writable && reader.payload == "old record" && reader.type == "NTAG216");

  // Same-UID content is refreshed on explicit Read; swapped UIDs refresh immediately.
  fixture.record("updated record");
  assert(reader.scan(true) && reader.payload == "updated record" && fixture.reads > initialReads);
  fixture.uid[6] = 7; fixture.record("other spool");
  assert(reader.scan(false) && reader.payload == "other spool" && reader.uid == "04010203040507");
  fixture.present = false;
  assert(!reader.scan(false) && !reader.present && !reader.writable && reader.payload.isEmpty());
  fixture.present = true; fixture.record("returned spool");
  assert(reader.scan(false) && reader.payload == "returned spool");
  fixture.multiple = true;
  assert(reader.scan(false) && reader.targets == 2 && !reader.writable && reader.payload.isEmpty());
  fixture.multiple = false;
  assert(reader.scan(false) && reader.writable);

  // A formerly writable cached tag must still pass fresh protection reads on Write.
  const String expectedUid = reader.uid;
  fixture.memory[10] = 1;
  assert(reader.scan(false) && reader.writable);
  assert(!reader.writeInventory(expectedUid, "replacement") && fixture.writes == 0);
  assert(!reader.writable);
  fixture.memory[10] = 0;
  assert(reader.writeInventory(expectedUid, "replacement") && reader.payload == "replacement");

  // Interrupted writes invalidate cached pre-write content, even for the same UID.
  fixture.failAfterWrites = fixture.writes + 1;
  assert(!reader.writeInventory(expectedUid, "partial replacement"));
  fixture.failAfterWrites = -1;
  const int beforeRetry = fixture.reads;
  assert(reader.scan(false) && fixture.reads > beforeRetry && reader.payload.isEmpty());

  fixture.dropSelect = true;
  const uint32_t started = now;
  assert(!reader.scan(false) && !reader.present && !reader.writable);
  assert(now - started == 1750 && idleCalls >= 1750);
  fixture.dropSelect = false; fixture.dropVersion = true;
  assert(reader.scan(false) && !reader.writable);
  fixture.dropVersion = false;
  assert(reader.scan(false) && reader.writable && reader.type == "NTAG216");
  // A fresh diagnostic handshake must not reuse a prior available/writable state.
  {
    ReaderFixture probe;
    Nfc device(probe.serial); device.begin([] {});
    assert(device.available && device.scan(true) && device.writable);
    const auto respond = probe.serial.onWrite;
    probe.serial.onWrite = [](const uint8_t *, size_t) {};
    device.begin([] {});
    assert(!device.available && !device.present && !device.writable && device.uid.isEmpty() && device.payload.isEmpty());
    probe.serial.onWrite = respond;
    device.begin([] {});
    assert(device.available && device.scanIdentity() && !device.uid.isEmpty() && probe.writes == 0);
  }
  tagProfiles(); identityOnly(); otherProtocols();
  std::cout << "PASS: NFC A/B/FeliCa/Jewel UID association, NTAG213/215/216 capacity/protection/read-back, cache, malformed/foreign tags, Creality/Qidi Classic sector writes, Anycubic/Elegoo/OpenSpool NTAG writes, Bambu/Snapmaker read and magic-card clone and bounded timeout yielding\n";
}
