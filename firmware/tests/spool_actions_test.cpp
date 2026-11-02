#include "SpoolActions.h"
#include <cassert>
#include <initializer_list>

int main() {
  using namespace SpoolActions;
  assert(removal(0, false, "") == Removal::Local);
  assert(removal(0, true, "") == Removal::Local); // Discard an unlinked draft, not a server record.
  for (const char *journal : {"vendor", "filament", "spool", "archive"})
    assert(removal(0, true, journal) == Removal::Unresolved);
  for (const char *journal : {"vendor", "filament", "spool"})
    assert(removal(42, true, journal) == Removal::Unresolved);
  assert(removal(42, false, "") == Removal::MissingServer);
  assert(removal(42, false, "archive") == Removal::MissingServer);
  assert(removal(42, true, "") == Removal::Archive);
  assert(removal(42, true, "archive") == Removal::Archive); // Only an archive journal can resume archive.
  assert(archiveState(42, 42, true, true) == ArchiveState::Archived);
  assert(archiveState(42, 42, true, false) == ArchiveState::Active);
  assert(archiveState(42, 43, true, true) == ArchiveState::Unknown); // Wrong record never permits removal.
  assert(archiveState(42, 42, false, false) == ArchiveState::Unknown);
  assert(archiveState(0, 0, true, true) == ArchiveState::Unknown);
  for (const char *uid : {"A1B2C3D4", "04A1B2C3D4E5F6", "0102030405060708090A", "B:12345678", "F:0123456789ABCDEF", "J:A1B2C3D4"}) assert(validUid(uid));
  for (const char *uid : {"", "04", "04a1b2c3d4e5f6", "04A1B2C3D4E5FZ", "04:A1:B2:C3:D4:E5:F6", "0102030405060708090A00", "B:A1", "F:A1B2C3D4", "J:A1B2C3D4E5F6", "A:12345678", "B:1234567Z", "b:12345678"}) assert(!validUid(uid));
  assert(!validUid(nullptr));
  assert(inactiveTagLink(true, true, false, "", "04:aa:bb:cc:dd:ee:ff"));
  assert(!inactiveTagLink(false, true, false, "", "11223344"));
  assert(!inactiveTagLink(true, false, false, "", "11223344"));
  assert(!inactiveTagLink(true, true, true, "", "11223344"));
  assert(!inactiveTagLink(true, true, false, "archive", "11223344"));
  assert(!inactiveTagLink(true, true, false, "", ""));
  assert(unlink("04:aa:bb:cc:dd:ee:ff", "04:aa:bb:cc:dd:ee:ff", "") == Unlink::Allowed); // Legacy links can be explicitly cleared.
  assert(unlink("B:11223344", "B:11223344", "") == Unlink::Allowed);
  assert(unlink("B:11223344", "11223344", "") == Unlink::Changed); // Never strip the radio-family namespace.
  assert(unlink("11223344", "AABBCCDD", "") == Unlink::Changed); // Stale confirmation cannot clear a replacement link.
  assert(unlink("", "11223344", "") == Unlink::Changed);
  assert(unlink("11223344", "", "") == Unlink::InvalidUid);
  assert(unlink("11223344", nullptr, "") == Unlink::InvalidUid);
  assert(unlink("0123456789abcdef0123456789abcdef", "0123456789abcdef0123456789abcdef", "") == Unlink::Allowed);
  assert(unlink("11223344", "0123456789abcdef0123456789abcdef", "") == Unlink::Changed);
  assert(unlink("11223344", "0123456789abcdef0123456789abcdef0", "") == Unlink::InvalidUid);
  for (const char *journal : {"vendor", "filament", "spool", "archive"})
    assert(unlink("11223344", "11223344", journal) == Unlink::Unresolved);
  assert(identifiesSpool(true, "11223344", "11223344"));
  assert(!identifiesSpool(true, "", "11223344")); // Unlink remains effective even if the tag still holds a profile.
  assert(!identifiesSpool(true, "AABBCCDD", "11223344"));
  assert(!identifiesSpool(false, "11223344", "11223344"));
  assert(!identifiesSpool(true, "B:11223344", "11223344"));
  assert(confirmedMissing("archive", true, true, true));
  for (const char *journal : {"", "vendor", "filament", "spool"}) assert(!confirmedMissing(journal, true, true, true));
  assert(!confirmedMissing("archive", false, true, true)); // A generic web server's 404 is not Spoolman evidence.
  assert(!confirmedMissing("archive", true, false, true)); // An empty active inventory does not prove deletion.
  assert(!confirmedMissing("archive", true, true, false)); // Keep the journal on timeout, bad paging or a conflicting list.
  assert(missingReply(404, 42, "No spool with ID 42 found."));
  assert(!missingReply(404, 42, "Not Found"));
  assert(!missingReply(404, 42, "No spool with ID 43 found."));
  assert(!missingReply(500, 42, "No spool with ID 42 found."));
  assert(!missingReply(404, 0, "No spool with ID 0 found."));
  assert(!missingReply(404, 42, nullptr));
}
