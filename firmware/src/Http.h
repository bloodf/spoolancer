#pragma once
// HTTP reply helpers, session authentication and shared request validation.
#include "AppState.h"

namespace spoolancer {
void error(int code, const String &message);
void reply(JsonDocument &doc, int code = 200);
void ok(const String &message);
bool hasSession();
bool authorized();
bool trustedSessionRequest();
void sessionReply(JsonDocument &result);
bool body(JsonDocument &doc);
bool confirmation(JsonDocument &doc);
Spool *requestedSpool(JsonDocument &doc);
bool validMeasurement(const Spool &s);
}  // namespace spoolancer
