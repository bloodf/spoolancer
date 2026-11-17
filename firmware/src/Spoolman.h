#pragma once
// Spoolman client: HTTP transport, inventory snapshots, archive and sync journals, and discovery.
#include "AppState.h"
#include "SpoolActions.h"

namespace spoolancer {
bool validServerUrl(const String &url);
bool remoteRequestAt(const String &url, const char *method, const String &path, JsonDocument *input, JsonDocument &output, int &code, int allowedError = 0);
bool remoteRequest(const char *method, const String &path, JsonDocument *input, JsonDocument &output, int &code, int allowedError = 0);
bool parseRemoteSpool(JsonObjectConst item, Spool &value);
bool refreshRemoteInventory(bool force = false);
void pollRemoteInventory();
void inventoryJson(JsonObject obj);
bool archiveSpool(Spool &s, bool &removedMissing);
bool syncSpool(Spool &s);
// SpoolmanDiscovery.cpp
void discoveryReply();
void startDiscovery();
void pollDiscovery();
}  // namespace spoolancer
