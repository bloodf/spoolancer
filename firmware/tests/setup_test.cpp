#include "Setup.h"
#include <cassert>
#include <string>

void setupTests() {
  assert(Setup::validPin("57290381"));
  assert(!Setup::validPin("5729038"));
  assert(!Setup::validPin("572903810"));
  assert(!Setup::validPin("5729038a"));
  assert(!Setup::validPin(nullptr));
  using Auth = Setup::AuthMode;
  Setup::StoredAuth stored;
  assert(Setup::authMode(stored) == Auth::Unconfigured);
  stored.legacyPresent = true; stored.legacyPin = "57290381";
  assert(Setup::authMode(stored) == Auth::Protected);
  stored.legacyPin = "";
  assert(Setup::authMode(stored) == Auth::Invalid);
  stored.legacyPin = "not-a-pin";
  assert(Setup::authMode(stored) == Auth::Invalid);
  stored.hasFields = true; stored.fieldsTyped = true; stored.complete = true; stored.pinRequired = true;
  assert(Setup::authMode(stored) == Auth::Invalid);
  stored.pin = "57290381";
  assert(Setup::authMode(stored) == Auth::Protected);
  stored.pinRequired = false;
  assert(Setup::authMode(stored) == Auth::Invalid);
  stored.pin = "";
  assert(Setup::authMode(stored) == Auth::Open);
  stored.fieldsTyped = false;
  assert(Setup::authMode(stored) == Auth::Invalid);
  stored.fieldsTyped = true; stored.complete = false;
  assert(Setup::authMode(stored) == Auth::Invalid);
  stored.pinRequired = true; stored.legacyPin = "57290381";
  assert(Setup::authMode(stored) == Auth::Unconfigured);
  stored.pin = "57290381";
  assert(Setup::authMode(stored) == Auth::Invalid);
  for (Auth auth : {Auth::Unconfigured, Auth::Protected, Auth::Open, Auth::Invalid}) {
    assert(Setup::required(auth) == (auth == Auth::Unconfigured));
    assert(Setup::complete(auth) == (auth == Auth::Protected || auth == Auth::Open));
    assert(Setup::pinRequired(auth) == (auth != Auth::Open));
  }

  assert(Setup::validSsid(" My Wi-Fi "));
  assert(!Setup::validSsid(""));
  assert(!Setup::validSsid("   "));
  assert(!Setup::validSsid("bad\nSSID"));
  assert(!Setup::validSsid(nullptr));
  assert(Setup::validSsid(std::string(32, 'x').c_str()));
  assert(!Setup::validSsid(std::string(33, 'x').c_str()));
  std::string utf8;
  for (unsigned i = 0; i < 16; ++i) utf8 += "\xc3\xa9";
  assert(Setup::validSsid(utf8.c_str()));
  utf8 += "\xc3\xa9";
  assert(!Setup::validSsid(utf8.c_str()));
  for (const char *bad : {"\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "\xff"})
    assert(!Setup::validSsid(bad));
  assert(Setup::validWifi("Home", "password", false));
  assert(!Setup::validWifi("Home", "short", false));
  assert(!Setup::validWifi("Home", "", false));
  assert(Setup::validWifi("Guest", "", true));
  assert(!Setup::validWifi("Guest", "password", true));
  assert(!Setup::validWifi("Home", "pass\nword", false));
  assert(Setup::validWifiPassword(std::string(63, 'z').c_str()));
  assert(Setup::validWifiPassword(std::string(64, 'f').c_str()));
  assert(!Setup::validWifiPassword(std::string(64, 'z').c_str()));
  assert(!Setup::validWifiPassword(std::string(65, 'f').c_str()));
  const char *resolved = nullptr;
  assert(Setup::resolveWifiPassword("Home", "password", "Home", "", false, false, resolved));
  assert(!std::strcmp(resolved, "password"));
  assert(!Setup::resolveWifiPassword("Home", "password", "New", "", false, false, resolved));
  assert(Setup::resolveWifiPassword("Home", "password", "New", "new-pass", true, false, resolved));
  assert(!std::strcmp(resolved, "new-pass"));
  assert(Setup::resolveWifiPassword("Home", "password", "New", "", true, true, resolved));
  assert(!*resolved);
  assert(!Setup::resolveWifiPassword("Home", "password", "New", "password", true, true, resolved));
  assert(Setup::resolveWifiPassword("Guest", "", "Guest", "", false, false, resolved));
  assert(!Setup::resolveWifiPassword("Guest", "", "Guest", "", true, false, resolved));

  assert(Setup::trustedHeaders("192.168.4.1", "http://192.168.4.1"));
  assert(Setup::trustedHeaders("192.168.4.1:80", ""));
  assert(!Setup::trustedHeaders("attacker.invalid", "http://192.168.4.1"));
  assert(!Setup::trustedHeaders("192.168.4.1", "https://attacker.invalid"));
  assert(!Setup::trustedHeaders("192.168.4.1.attacker.invalid", ""));
  assert(Setup::trustedLocalHeaders("10.0.0.4", "http://10.0.0.4", "10.0.0.4"));
  assert(Setup::trustedLocalHeaders("10.0.0.4:80", "http://10.0.0.4", "10.0.0.4"));
  assert(Setup::trustedLocalHeaders("spoolancer.local", "http://spoolancer.local:80", "10.0.0.4"));
  assert(Setup::trustedLocalHeaders("spoolancer.local:80", "", "10.0.0.4"));
  assert(!Setup::trustedLocalHeaders("10.0.0.5", "", "10.0.0.4"));
  assert(!Setup::trustedLocalHeaders("10.0.0.4:8080", "", "10.0.0.4"));
  assert(!Setup::trustedLocalHeaders("attacker.invalid", "http://attacker.invalid", "10.0.0.4"));
  assert(!Setup::trustedLocalHeaders("spoolancer.local.attacker.invalid", "", "10.0.0.4"));
  assert(!Setup::trustedLocalHeaders("10.0.0.4", "http://spoolancer.local", "10.0.0.4"));
  assert(!Setup::trustedLocalHeaders("10.0.0.4", "https://10.0.0.4", "10.0.0.4"));
  assert(!Setup::trustedLocalHeaders("10.0.0.4", "null", "10.0.0.4"));
  assert(!Setup::trustedLocalHeaders("10.0.0.4", "http://10.0.0.4.attacker.invalid", "10.0.0.4"));
  assert(!Setup::trustedLocalHeaders("spoolancer.local", "", "0.0.0.0"));
  assert(!Setup::trustedLocalHeaders("spoolancer.local", "", "192.168.4.1", false));
  Setup::Window window;
  assert(!window.available(10));
  window.begin(100);
  assert(window.available(101));
  assert(!window.consume(101, false, true, true, true));
  assert(!window.consume(101, true, false, true, true));
  assert(!window.consume(101, true, true, false, true));
  assert(!window.consume(101, true, true, true, false));
  assert(window.consume(101, true, true, true, true)); // No physical gesture required.
  assert(!window.consume(102, true, true, true, true));
  assert(!window.consume(1600, true, true, true, true));
  assert(window.consume(1601, true, true, true, true)); // Invalid form can be retried.
  for (Auth auth : {Auth::Protected, Auth::Open, Auth::Invalid})
    assert(!window.consume(4000, Setup::required(auth), true, true, true));
  assert(!window.expired(600099));
  assert(window.expired(600100));
  assert(!window.consume(600100, true, true, true, true));
  window.begin(700000);
  window.observeConnection(true, 701000);
  assert(window.apRemaining(701000) == 60000);
  assert(!window.expired(760999));
  assert(window.expired(761000));
  window.begin(761001);
  window.observeConnection(false, 761001);
  assert(window.apRemaining(761001) == Setup::apDuration);
  window.observeConnection(true, 762000);
  assert(!window.expired(821999));
  assert(window.expired(822000));
  window.close();
  assert(!window.available(822001));
  window.begin(0xfffffff0);
  assert(!window.expired(20));
  assert(window.consume(40, true, true, true, true));
  assert(!window.consume(100, true, true, true, true));
  assert(window.consume(1540, true, true, true, true));

  Setup::Button button;
  using G = Setup::Gesture;
  assert(button.update(false, 10) == G::None);
  assert(button.update(true, 20) == G::None);
  assert(button.update(false, 40) == G::None);
  assert(button.update(true, 100) == G::None);
  assert(button.update(false, 2099) == G::None);
  assert(button.update(true, 3000) == G::None);
  assert(button.update(true, 5000) == G::None);
  assert(button.update(false, 5000) == G::None);
  assert(button.update(false, 5001) == G::None);
  assert(button.update(true, 6000) == G::None);
  assert(button.update(true, 13999) == G::None);
  assert(button.update(true, 14000) == G::ResetWifi);
  assert(button.update(true, 22000) == G::None);
  assert(button.update(false, 22001) == G::None);
  assert(button.update(true, 0xfffffff0) == G::None);
  assert(button.update(true, 7984) == G::ResetWifi);
  assert(button.update(false, 7985) == G::None);

  Setup::Button heldAtStartup;
  assert(heldAtStartup.update(true, 0) == G::None);
  assert(heldAtStartup.update(true, 9000) == G::None);
  assert(heldAtStartup.update(false, 10000) == G::None);
  assert(heldAtStartup.update(true, 11000) == G::None);
  assert(heldAtStartup.update(false, 13000) == G::None);
  assert(heldAtStartup.update(true, 14000) == G::None);
  assert(heldAtStartup.update(true, 22000) == G::ResetWifi);
  assert(heldAtStartup.update(true, 32000) == G::None);
  assert(heldAtStartup.update(false, 32001) == G::None);
  assert(heldAtStartup.update(true, 33000) == G::None);
  assert(heldAtStartup.update(true, 41000) == G::ResetWifi);
}
