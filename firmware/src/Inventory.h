#pragma once
// Inventory model: spool lookup, JSON encoding/validation, persistence, selection and measurement.
#include "AppState.h"

namespace spoolancer {
bool visibleSpool(const Spool &spool);
bool inactiveTagLink(const Spool &spool);
bool inventoryStale();
Spool *findStoredSpool(const String &id);
Spool *findSpool(const String &id);
void spoolJson(JsonObject obj, const Spool &s, bool persisted = false);
bool saveInventory();
bool textField(JsonObjectConst obj, const char *key, String &out, size_t max, bool required = true);
bool textWithoutNul(JsonVariantConst value);
bool numericField(JsonObjectConst obj, const char *key, double &out, double low, double high);
void readDetails(JsonObjectConst obj, Spool &s);
bool validateSpool(JsonObjectConst obj, Spool &s);
bool loadInventory();
String measurementProblem(const Spool &s);
bool recordMeasurement(Spool &s);
void chooseSpool(const String &id, const char *source = "manual");
bool cycleVisibleSpool(bool forward);
}  // namespace spoolancer
