#include "Http.h"
#include "Device.h"
#include "Inventory.h"

namespace spoolancer {
void error(int code, const String &message) {
  JsonDocument doc; doc["error"] = message;
  String body; serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store"); server.send(code, "application/json", body);
}
void reply(JsonDocument &doc, int code) {
  String body; serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store"); server.send(code, "application/json", body);
}
void ok(const String &message) { JsonDocument doc; doc["ok"] = true; doc["message"] = message; reply(doc); }
bool hasSession() {
  const String header = server.header("Authorization");
  if (header.startsWith("Bearer ")) {
    const String token = header.substring(7);
    for (const auto &session : sessions)
      if (session.token.length() == 64 && token == session.token && uint32_t(millis() - session.started) < sessionMs) {
        lastPrivateRequest = millis(); privateSeen = true; return true;
      }
  }
  return false;
}
bool authorized() {
  if (hasSession()) return true;
  error(401, "Open a device session to make changes."); return false;
}
bool trustedSessionRequest() {
  const String local = server.client().localIP().toString();
  return Setup::trustedLocalHeaders(server.hostHeader().c_str(), server.header("Origin").c_str(), local.c_str());
}
void sessionReply(JsonDocument &result) {
  Session &session = sessions[sessionSlot++ % 4];
  session.token = randomHex(32); session.started = millis();
  result["token"] = session.token; result["expires_in"] = sessionMs / 1000;
  loginFailures = 0; reply(result);
}
bool body(JsonDocument &doc) {
  if (!server.header("Content-Type").startsWith("application/json")) { error(415, "Send application/json."); return false; }
  const String raw = server.arg("plain");
  if (raw.length() > maxRequest || raw.isEmpty()) { error(413, "JSON body must be 1–4096 bytes."); return false; }
  if (deserializeJson(doc, raw, DeserializationOption::NestingLimit(6)) || !doc.is<JsonObject>()) {
    error(400, "Invalid JSON object."); return false;
  }
  return true;
}
bool confirmation(JsonDocument &doc) {
  if (!doc["confirm"].is<bool>() || !doc["confirm"].as<bool>()) { error(400, "Explicit confirmation is required."); return false; }
  return true;
}
Spool *requestedSpool(JsonDocument &doc) {
  if (!doc["spool_id"].is<const char *>()) { error(400, "spool_id is required."); return nullptr; }
  Spool *s = findSpool(doc["spool_id"].as<String>());
  if (!s) error(404, "Spool not found.");
  return s;
}
bool validMeasurement(const Spool &s) {
  const String problem = measurementProblem(s);
  if (problem.isEmpty()) return true;
  error(409, problem); return false;
}
}  // namespace spoolancer
