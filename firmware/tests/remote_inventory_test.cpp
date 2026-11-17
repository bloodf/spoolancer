#include "RemoteInventory.h"
#include "Discovery.h"
#include "RemoteHttp.h"
#include <cassert>
#include <string>
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
  int remaining = -1; bool lengthSeen = false, chunked = false;
  assert(RemoteHttp::header("Content-Length: 42", remaining, lengthSeen, chunked) && remaining == 42);
  assert(!RemoteHttp::header("content-length: 42", remaining, lengthSeen, chunked));
  for (const char *bad : {"Content-Length: -1", "Content-Length:", "Content-Length: 20 10", "Content-Length: 20001", "Content-Encoding: gzip", "Bad", " Content-Length: 1"}) {
    remaining = -1; lengthSeen = false;
    assert(!RemoteHttp::header(bad, remaining, lengthSeen, chunked));
  }
  remaining = -1; lengthSeen = false;
  assert(RemoteHttp::header("X: y", remaining, lengthSeen, chunked));
  assert(RemoteHttp::header("content-length: 20000", remaining, lengthSeen, chunked));
  {
    remaining=-1; lengthSeen=chunked=false;
    assert(RemoteHttp::header("Transfer-Encoding: chunked",remaining,lengthSeen,chunked) && chunked);
    assert(!RemoteHttp::header("Content-Length: 4",remaining,lengthSeen,chunked));
    assert(RemoteHttp::header("Content-Encoding: identity",remaining,lengthSeen,chunked));
    for (const std::string wire : {"3\r\n[1,\r\n2\r\n2]\r\n0\r\n\r\n",
                                   "3;x=y\r\n[1,\r\n2\r\n2]\r\n0\r\nX-Trace: value\r\n\r\n"}) {
      RemoteHttp::Body body(-1,true); std::string text;
      for (char byte : wire) assert(body.feed(byte,text));
      assert(body.done(true) && text=="[1,2]");
    }
    for (const std::string wire : {"Z\r\n", "FFFFF\r\n", "1\r\naX", "0\r\n\n"}) {
      RemoteHttp::Body body(-1,true); std::string text; bool valid=true;
      for(char byte:wire) if(!body.feed(byte,text)){valid=false;break;}
      assert(!valid);
    }
    RemoteHttp::Body truncated(-1,true); std::string text;
    for(char byte:std::string("1\r\nx\r\n")) assert(truncated.feed(byte,text));
    assert(!truncated.done(false));
    RemoteHttp::Body fixed(2,false); text.clear(); assert(fixed.feed('a',text));
    assert(!fixed.done(false)); assert(fixed.feed('b',text) && fixed.done(true));
    assert(!fixed.feed('c',text));
    RemoteHttp::Body closed(-1,false); assert(!closed.done(true) && closed.done(false));
  }
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
  assert(cache.shouldRefresh(300, true, true)); // Explicit retry is not silently throttled.
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
