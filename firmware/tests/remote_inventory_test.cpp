#include "RemoteInventory.h"
#include "Discovery.h"
#include "RemoteHttp.h"
#include <cassert>
#include <cstring>

void remoteInventoryTests() {
  using namespace RemoteInventory;
  auto value = weights(NAN, 1000, NAN, 180, 200, 620);
  assert(value.initial == 1000 && value.empty == 180 && value.remaining == 620);
  value = weights(750, 1000, 0, 180, 200, NAN);
  assert(value.initial == 750 && value.empty == 0 && std::isnan(value.remaining));
  value = weights(NAN, NAN, NAN, NAN, NAN, NAN);
  assert(std::isnan(value.initial) && std::isnan(value.empty) && std::isnan(value.remaining));
  assert(weights(0, 1000, NAN, NAN, 190, 0).initial == 0); // Do not replace explicit zero.
  assert(weights(1000, NAN, NAN, NAN, 190, 0).empty == 190);
  assert(sameNumber(NAN, NAN) && !sameNumber(NAN, 0) && !sameNumber(180, 181));
  char id[17]; stableId(42, 0, id); assert(!std::strcmp(id, "000000000000002a"));
  stableId(42, 1, id); assert(!std::strcmp(id, "000000010000002a"));
  assert(visible(false, false, false, 0));
  assert(!visible(true, false, false, 0)); // Local drafts never masquerade as server inventory.
  assert(!visible(true, false, false, 42)); // Legacy link alone is not remote evidence.
  assert(!visible(true, true, false, 42)); // Removed/archived records disappear.
  assert(visible(true, true, true, 42));
  Pages pages;
  for (int i = 1; i <= 200; ++i) assert(pages.accept(i));
  assert(pages.complete(0)); // Exactly the limit still needs a terminating page.
  assert(!pages.accept(201)); assert(!pages.complete(0));
  Pages duplicate; assert(duplicate.accept(20)); assert(!duplicate.accept(20));
  Pages reordered; assert(reordered.accept(20)); assert(!reordered.accept(19));
  Pages invalid; assert(!invalid.accept(0));
  int code = 0;
  assert(RemoteHttp::status("HTTP/1.0 200 OK", code) && code == 200);
  assert(RemoteHttp::status("HTTP/1.1 404 Not Found", code) && code == 404);
  assert(!RemoteHttp::status("HTTP/1.1 20", code));
  assert(!RemoteHttp::status("HTTP/1.1 2000", code));
  assert(!RemoteHttp::status("HTTP/1.1 2xx Nope", code));
  int remaining = -1; bool lengthSeen = false;
  assert(RemoteHttp::header("Content-Length: 42", remaining, lengthSeen) && remaining == 42);
  assert(!RemoteHttp::header("content-length: 42", remaining, lengthSeen));
  for (const char *bad : {"Content-Length: -1", "Content-Length:", "Content-Length: 20 10", "Content-Length: 20001", "Transfer-Encoding: chunked", "Content-Encoding: gzip", "Bad", " Content-Length: 1"}) {
    remaining = -1; lengthSeen = false;
    assert(!RemoteHttp::header(bad, remaining, lengthSeen));
  }
  remaining = -1; lengthSeen = false;
  assert(RemoteHttp::header("X: y", remaining, lengthSeen));
  assert(RemoteHttp::header("content-length: 20000", remaining, lengthSeen));
  assert(RemoteHttp::complete(0, true, 20000));
  assert(!RemoteHttp::complete(4, false, 38)); // Truncated declared body.
  assert(!RemoteHttp::complete(-1, true, 42)); // Close-delimited timeout is not EOF.
  assert(RemoteHttp::complete(-1, false, 42));
  assert(!RemoteHttp::complete(0, false, 20001));
  const auto subnet = Discovery::range(0xc0a80132, 0xffffff00);
  assert(subnet.first == 0xc0a80101 && subnet.last == 0xc0a801fe);
  assert(Discovery::eligible(0xc0a80145, subnet));
  assert(!Discovery::eligible(0xc0a80132, subnet));
  assert(!Discovery::eligible(0xc0a80100, subnet) && !Discovery::eligible(0xc0a801ff, subnet));
  assert(!Discovery::eligible(0xc0a80201, subnet) && !Discovery::eligible(0x7f000001, subnet));
  const auto smaller = Discovery::range(0xc0a80185, 0xffffffc0);
  assert(smaller.first == 0xc0a80181 && smaller.last == 0xc0a801be);
  const auto larger = Discovery::range(0x0a001932, 0xffff0000);
  assert(larger.first == 0x0a001901 && larger.last == 0x0a0019fe);
  Cache cache;
  assert(cache.stale(0, true) && cache.shouldRefresh(0, true, false));
  cache.begin(100); cache.finish(200, true);
  assert(cache.loaded && !cache.stale(300, true));
  assert(cache.stale(300, false));
  assert(!cache.shouldRefresh(300, true, true));
  assert(cache.shouldRefresh(10100, true, true));
  assert(!cache.shouldRefresh(10100, true, false));
  assert(cache.shouldRefresh(30200, true, false));
  cache.begin(30200); cache.finish(30300, false);
  assert(cache.loaded && cache.stale(30301, true)); // Preserve known cache, mark failure.
  assert(!cache.shouldRefresh(30301, true, false));
  assert(cache.shouldRefresh(40200, true, false));
  Cache reboot; reboot.loaded = true;
  assert(reboot.stale(0, true)); // Persisted records are not a fresh server response.
  Cache wrap; wrap.begin(0xfffffff0); wrap.finish(0xfffffff0, true);
  assert(!wrap.stale(30, true));
}
