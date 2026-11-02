#pragma once
// Over-the-air firmware/filesystem update pulled from the project's GitHub releases.
// The release workflow (.github/workflows/release.yml) publishes, for every board variant,
// two fixed-name assets: spoolancer-<variant>-firmware.bin (app image) and
// spoolancer-<variant>-littlefs.bin (web/filesystem image). The device asks the GitHub API
// for the latest release, matches the assets for its own variant, and flashes them.
//
// ponytail: TLS transport only (setInsecure) and no binary signature — the OTA trusts GitHub
// over HTTPS and the dual OTA app partitions make a bad app image recoverable, a bad filesystem
// image re-flashable. Upgrade path: WiFiClientSecure::setCACertBundle + signed images.
#include <cstddef>
#include <cstdio>
#include <cstring>

#ifndef SPOOLANCER_OTA_REPO
#define SPOOLANCER_OTA_REPO "bloodf/spoolancer"
#endif

// Firmware version — injected by the release workflow as the git tag, else a dev marker.
#ifndef SPOOLANCER_VERSION
#define SPOOLANCER_VERSION "0.0.0-dev"
#endif

// The board-variant string (e.g. "headless-led", "st7789") is supplied by the caller — main.cpp
// already derives it as `deviceVariant` from the feature macros, and the release workflow names
// each asset with the same string, so the device downloads the image it can flash.

namespace Ota {
// Fixed per-variant asset name the release workflow publishes. kind is "firmware" or "littlefs".
inline void assetName(const char *variant, const char *kind, char *out, size_t cap) {
  std::snprintf(out, cap, "spoolancer-%s-%s.bin", variant, kind);
}
// releases/latest always returns the newest published release, so any tag that differs from the
// running one is an available update — no semantic-version ordering needed.
inline bool updateAvailable(const char *current, const char *latest) {
  return latest && latest[0] && (!current || std::strcmp(current, latest) != 0);
}
}  // namespace Ota

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

namespace Ota {
struct Release {
  bool ok = false;
  String error;        // Non-empty when the latest release could not be resolved.
  String tag;          // Latest release tag, e.g. "v1.2.0".
  String firmwareUrl;  // Download URL of this variant's app image (required).
  String fsUrl;        // Download URL of this variant's filesystem image (optional).
  String notesUrl;     // Human-readable release page.
};

// Ask the GitHub API for the latest release and resolve this variant's download URLs.
inline Release fetchLatest(const char *variant, const char *repo = SPOOLANCER_OTA_REPO) {
  Release r;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("spoolancer-ota");
  http.setTimeout(8000);
  const String url = String("https://api.github.com/repos/") + repo + "/releases/latest";
  if (!http.begin(client, url)) { r.error = "Could not reach GitHub."; return r; }
  http.addHeader("Accept", "application/vnd.github+json");
  const int code = http.GET();
  if (code != 200) {
    r.error = code == 404 ? "No published releases yet." : String("GitHub returned HTTP ") + code;
    http.end(); return r;
  }
  // Keep only the fields we read so a large release payload stays within RAM.
  JsonDocument filter;
  filter["tag_name"] = true; filter["html_url"] = true;
  filter["assets"][0]["name"] = true; filter["assets"][0]["browser_download_url"] = true;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { r.error = "Could not parse the GitHub response."; return r; }
  r.tag = doc["tag_name"].as<String>();
  r.notesUrl = doc["html_url"].as<String>();
  char fw[64], fs[64];
  assetName(variant, "firmware", fw, sizeof(fw));
  assetName(variant, "littlefs", fs, sizeof(fs));
  for (JsonObject a : doc["assets"].as<JsonArray>()) {
    const char *name = a["name"];
    if (!name) continue;
    if (!std::strcmp(name, fw)) r.firmwareUrl = a["browser_download_url"].as<String>();
    else if (!std::strcmp(name, fs)) r.fsUrl = a["browser_download_url"].as<String>();
  }
  if (r.tag.isEmpty()) { r.error = "The latest release has no tag."; return r; }
  if (r.firmwareUrl.isEmpty()) { r.error = String("The latest release has no ") + fw + " image."; return r; }
  r.ok = true;
  return r;
}

// Flash the filesystem image first (if present), then the app image. On success the device
// reboots into the new firmware and this never returns; on failure it returns the reason.
inline String apply(const Release &rel) {
  WiFiClientSecure client; client.setInsecure();
  httpUpdate.rebootOnUpdate(false);
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!rel.fsUrl.isEmpty()) {
    if (httpUpdate.updateSpiffs(client, rel.fsUrl) == HTTP_UPDATE_FAILED)
      return String("Filesystem update failed: ") + httpUpdate.getLastErrorString();
  }
  if (httpUpdate.update(client, rel.firmwareUrl) == HTTP_UPDATE_FAILED)
    return String("Firmware update failed: ") + httpUpdate.getLastErrorString();
  ESP.restart();
  return "";
}
}  // namespace Ota
#endif  // ARDUINO
