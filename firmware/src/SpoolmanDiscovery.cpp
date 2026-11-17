#include "Spoolman.h"
#include <WiFi.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include "Http.h"

namespace spoolancer {
uint32_t addressNumber(const IPAddress &ip) {
  return (uint32_t(ip[0]) << 24) | (uint32_t(ip[1]) << 16) | (uint32_t(ip[2]) << 8) | ip[3];
}
IPAddress addressFromNumber(uint32_t address) { return IPAddress(address >> 24, address >> 16, address >> 8, address); }
void queueDiscovery(const IPAddress &address, uint16_t port) {
  if (!port || !Discovery::eligible(addressNumber(address), discovery.scope) || discovery.pending.size() >= 16) return;
  const String url = "http://" + address.toString() + ":" + String(port);
  for (const auto &known : discovery.pending) if (known == url) return;
  for (const auto &known : discovery.candidates) if (known.url == url) return;
  discovery.pending.push_back(url);
}
void pollMdns(mdns_search_once_t *&query, bool service) {
  if (!query) return;
  mdns_result_t *results = nullptr;
  if (!mdns_query_async_get_results(query, 0, &results)) return;
  mdns_query_async_delete(query); query = nullptr;
  for (auto *result = results; result; result = result->next) {
    String name = result->instance_name ? result->instance_name : ""; name.toLowerCase();
    if (service && result->port != 7912 && name.indexOf("spoolman") < 0) continue;
    for (auto *address = result->addr; address; address = address->next)
      if (address->addr.type == IPADDR_TYPE_V4) queueDiscovery(IPAddress(address->addr.u_addr.ip4.addr), service ? result->port : 7912);
  }
  mdns_query_results_free(results);
}
void discoveryReply() {
  JsonDocument result;
  result["state"] = discovery.state == SpoolmanDiscovery::State::Searching ? "searching" :
                    discovery.state == SpoolmanDiscovery::State::Failed ? "failed" : "ready";
  result["message"] = discovery.message;
  JsonArray candidates = result["candidates"].to<JsonArray>();
  for (const auto &candidate : discovery.candidates) {
    JsonObject item = candidates.add<JsonObject>(); item["url"] = candidate.url; item["label"] = candidate.label;
  }
  reply(result);
}
void startDiscovery() {
  discovery.state = SpoolmanDiscovery::State::Searching; discovery.attempted = true;
  discovery.started = millis(); discovery.checked = 0; discovery.candidates.clear(); discovery.pending.clear();
  discovery.scope = Discovery::range(addressNumber(WiFi.localIP()), addressNumber(WiFi.subnetMask()));
  discovery.nextAddress = discovery.scope.first;
  discovery.message = "Searching this Wi-Fi subnet (up to 254 addresses) on port 7912 and advertised Spoolman services. Other ports or subnets need a manual URL.";
  if (!config.spoolman.isEmpty()) {
    String host = config.spoolman.substring(7);
    const int slash = host.indexOf('/'); if (slash >= 0) host = host.substring(0, slash);
    const int colon = host.indexOf(':'); if (colon >= 0) host = host.substring(0, colon);
    IPAddress address;
    if (address.fromString(host) && Discovery::eligible(addressNumber(address), discovery.scope)) discovery.pending.push_back(config.spoolman);
  }
  discovery.serviceSearch = mdns_query_async_new(nullptr, "_http", "_tcp", MDNS_TYPE_PTR, 1500, 8, nullptr);
  discovery.hostSearch = mdns_query_async_new("spoolman", nullptr, nullptr, MDNS_TYPE_A, 1500, 1, nullptr);
}
void pollDiscovery() {
  pollMdns(discovery.serviceSearch, true); pollMdns(discovery.hostSearch, false);
  if (discovery.state != SpoolmanDiscovery::State::Searching) return;
  const bool stop = WiFi.status() != WL_CONNECTED || uint32_t(millis() - discovery.started) >= 45000;
  bool socketsActive = false;
  for (auto &probe : discovery.sockets) {
    if (probe.fd >= 0) {
      fd_set ready, errors; FD_ZERO(&ready); FD_ZERO(&errors); FD_SET(probe.fd, &ready); FD_SET(probe.fd, &errors);
      timeval timeout{};
      const int selected = select(probe.fd + 1, nullptr, &ready, &errors, &timeout);
      if (selected > 0) {
        int socketError = 0; socklen_t length = sizeof(socketError);
        if (getsockopt(probe.fd, SOL_SOCKET, SO_ERROR, &socketError, &length) == 0 && !socketError)
          queueDiscovery(addressFromNumber(probe.address), 7912);
      }
      if (stop || selected != 0 || int32_t(probe.deadline - millis()) <= 0) { close(probe.fd); probe.fd = -1; }
    }
    if (probe.fd < 0 && !stop && discovery.nextAddress <= discovery.scope.last) {
      const uint32_t address = discovery.nextAddress++;
      if (Discovery::eligible(address, discovery.scope)) {
        probe.fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (probe.fd >= 0) {
          if (fcntl(probe.fd, F_SETFL, O_NONBLOCK) < 0) { close(probe.fd); probe.fd = -1; continue; }
          sockaddr_in target{}; target.sin_family = AF_INET; target.sin_port = htons(7912); target.sin_addr.s_addr = htonl(address);
          const int result = connect(probe.fd, reinterpret_cast<sockaddr *>(&target), sizeof(target));
          probe.address = address; probe.deadline = millis() + 180;
          if (result == 0) { queueDiscovery(addressFromNumber(address), 7912); close(probe.fd); probe.fd = -1; }
          else if (errno != EINPROGRESS) { close(probe.fd); probe.fd = -1; }
        }
      }
    }
    socketsActive |= probe.fd >= 0;
  }
  if (!stop && discovery.checked < discovery.pending.size() && discovery.candidates.size() < 8) {
    const String url = discovery.pending[discovery.checked++];
    JsonDocument info; int code; const String previousError = syncError;
    if (remoteRequestAt(url, "GET", "/info", nullptr, info, code) && info["version"].is<const char *>() &&
        info["db_type"].is<const char *>() && info["debug_mode"].is<bool>()) {
      JsonDocument inventoryProbe;
      if (remoteRequestAt(url, "GET", "/spool?limit=1", nullptr, inventoryProbe, code) && inventoryProbe.is<JsonArray>() && inventoryProbe.size() <= 1) {
        bool valid = inventoryProbe.size() == 0;
        if (inventoryProbe.size() == 1) { Spool record; valid = parseRemoteSpool(inventoryProbe[0].as<JsonObjectConst>(), record); }
        if (valid) discovery.candidates.push_back({url, "Spoolman " + info["version"].as<String>()});
      }
    }
    syncError = previousError;
  }
  if (stop || (!socketsActive && discovery.nextAddress > discovery.scope.last && !discovery.serviceSearch && !discovery.hostSearch &&
      (discovery.checked >= discovery.pending.size() || discovery.candidates.size() >= 8))) {
    discovery.state = WiFi.status() == WL_CONNECTED ? SpoolmanDiscovery::State::Ready : SpoolmanDiscovery::State::Failed;
    discovery.message = discovery.state == SpoolmanDiscovery::State::Failed ? "Wi-Fi disconnected during discovery. Reconnect or enter the server URL manually." :
      discovery.candidates.empty() ? "No Spoolman server answered in this subnet. Enter its URL manually if it uses another port, subnet, or blocked discovery." :
                                    "Select a server, then save its URL. Discovery does not change the configured server.";
  }
}
}  // namespace spoolancer
