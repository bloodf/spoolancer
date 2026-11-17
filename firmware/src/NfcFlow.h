#pragma once
// NFC workflows: tag reads and selection, inventory/brand tag writes and erasing.
#include "AppState.h"
#include "BrandTags.h"

namespace spoolancer {
const char *nfcChip();
void nfcJson(JsonObject obj);
void readNfc(bool explicitRead = false);
String writeSpoolTag(Spool &s, const String &expectedUid, BrandTags::Format format);
String eraseTag(const String &expectedUid);
}  // namespace spoolancer
