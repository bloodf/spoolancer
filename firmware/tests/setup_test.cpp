#include "Setup.h"
#include <cassert>

void setupTests() {
  assert(Setup::validPin("57290381"));
  assert(!Setup::validPin("5729038"));
  assert(!Setup::validPin("572903810"));
  assert(!Setup::validPin("5729038a"));
  assert(!Setup::validPin(nullptr));
  assert(Setup::trustedHeaders("192.168.4.1", "http://192.168.4.1"));
  assert(Setup::trustedHeaders("192.168.4.1:80", ""));
  assert(!Setup::trustedHeaders("attacker.invalid", "http://192.168.4.1"));
  assert(!Setup::trustedHeaders("192.168.4.1", "https://attacker.invalid"));
  assert(!Setup::trustedHeaders("192.168.4.1.attacker.invalid", ""));
  Setup::Window window;
  assert(!window.arm(10));
  window.begin(100);
  assert(!window.consume(101, true, true, true, true));
  assert(window.arm(200));
  assert(!window.consume(201, false, true, true, true));
  assert(!window.consume(201, true, false, true, true));
  assert(!window.consume(201, true, true, false, true));
  assert(!window.consume(201, true, true, true, false));
  assert(window.armed(201));
  assert(window.consume(201, true, true, true, true));
  assert(!window.consume(202, true, true, true, true));
  assert(window.arm(300));
  assert(window.armed(60299));
  assert(!window.consume(60300, true, true, true, true));
  assert(!window.expired(600099));
  assert(window.expired(600100));
  window.begin(700000);
  window.observeConnection(true, 701000);
  assert(window.apRemaining(701000) == 60000);
  assert(!window.expired(760999));
  assert(window.expired(761000));
  window.close();
  assert(!window.arm(761001));
  window.begin(0xfffffff0);
  assert(!window.expired(20));
  assert(window.arm(20));
  assert(window.consume(40, true, true, true, true));

  Setup::Button button;
  using G = Setup::Gesture;
  assert(button.update(false, 10) == G::None);
  assert(button.update(true, 20) == G::None);
  assert(button.update(false, 40) == G::None);
  assert(button.update(true, 100) == G::None);
  assert(button.update(false, 2099) == G::None);
  assert(button.update(true, 3000) == G::None);
  assert(button.update(true, 5000) == G::None);
  assert(button.update(false, 5000) == G::ArmEnrollment);
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
  assert(heldAtStartup.update(false, 13000) == G::ArmEnrollment);
  assert(heldAtStartup.update(true, 14000) == G::None);
  assert(heldAtStartup.update(true, 22000) == G::ResetWifi);
  assert(heldAtStartup.update(true, 32000) == G::None);
  assert(heldAtStartup.update(false, 32001) == G::None);
  assert(heldAtStartup.update(true, 33000) == G::None);
  assert(heldAtStartup.update(true, 41000) == G::ResetWifi);
}
