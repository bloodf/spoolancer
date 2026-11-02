#pragma once
#include <cstring>
#include <cstdio>

namespace SpoolActions {
enum class Removal { Local, Archive, Unresolved, MissingServer };
inline bool archiving(const char *pending) { return pending && std::strcmp(pending, "archive") == 0; }
inline bool identifiesSpool(bool visible, const char *boundUid, const char *observedUid) {
  return visible && boundUid && *boundUid && observedUid && std::strcmp(boundUid, observedUid) == 0;
}
inline bool confirmedMissing(const char *pending, bool spoolmanVerified, bool exactMissingReply, bool absentFromList) {
  return archiving(pending) && spoolmanVerified && exactMissingReply && absentFromList;
}
inline bool missingReply(int code, int remoteId, const char *message) {
  if (code != 404 || remoteId <= 0 || !message) return false;
  char expected[64]; std::snprintf(expected, sizeof(expected), "No spool with ID %d found.", remoteId);
  return std::strcmp(expected, message) == 0;
}
inline bool inactiveTagLink(bool configured, bool remote, bool listed, const char *pending, const char *uid) {
  return configured && remote && !listed && (!pending || !*pending) && uid && *uid;
}
enum class Unlink { Allowed, InvalidUid, Changed, Unresolved };
inline Unlink unlink(const char *storedUid, const char *expectedUid, const char *pending) {
  if (pending && *pending) return Unlink::Unresolved;
  if (!expectedUid || !*expectedUid || std::strlen(expectedUid) > 32) return Unlink::InvalidUid;
  if (!storedUid || std::strcmp(storedUid, expectedUid) != 0) return Unlink::Changed;
  return Unlink::Allowed;
}
inline Removal removal(int remoteId, bool configured, const char *pending) {
  if (pending && *pending && (!archiving(pending) || remoteId <= 0)) return Removal::Unresolved;
  if (remoteId <= 0) return Removal::Local;
  return configured ? Removal::Archive : Removal::MissingServer;
}
enum class ArchiveState { Unknown, Active, Archived, Missing };
inline ArchiveState archiveState(int expectedId, int responseId, bool hasArchived, bool archived) {
  if (expectedId <= 0 || responseId != expectedId || !hasArchived) return ArchiveState::Unknown;
  return archived ? ArchiveState::Archived : ArchiveState::Active;
}
inline bool validUid(const char *uid) {
  if (!uid) return false;
  const auto length = std::strlen(uid);
  const bool typeB = length == 10 && uid[0] == 'B' && uid[1] == ':';
  const bool felica = length == 18 && uid[0] == 'F' && uid[1] == ':';
  const bool jewel = length == 10 && uid[0] == 'J' && uid[1] == ':';
  const bool prefixed = typeB || felica || jewel;
  if (!prefixed && length != 8 && length != 14 && length != 20) return false;
  for (const char *c = uid + (prefixed ? 2 : 0); *c; ++c) if (!((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'F'))) return false;
  return true;
}
}
