#include "StatusModel.h"
#include <cassert>

void statusTests() {
  using namespace StatusModel;
  assert(nfcReadSucceeded(true, 1, false));
  assert(!nfcReadSucceeded(true, 2, false));
  assert(!nfcReadSucceeded(true, 0, false));
  assert(!nfcReadSucceeded(false, 1, false));
  assert(!nfcReadSucceeded(true, 1, true));
  Snapshot ready;
  ready.gross = 700; ready.empty = 200; ready.initial = 1000;
  ready.calibrated = ready.stable = ready.fresh = ready.selected = true;
  Event event;
  auto view = evaluate(ready, event, 0);
  assert(view.mode == Mode::Remaining && view.remaining == 500);
  assert(view.leds[0].blue == 255 && view.leds[1].blue == 255);
  assert(view.leds[2].red == 255 && view.leds[2].green == 140 && view.leds[2].blue == 0 && view.leds[3].blue == 0 && view.leds[4].blue == 0);
  // Five equal segments, with a proportional partial segment at exact boundaries.
  for (int percent = 0; percent <= 100; ++percent) {
    const auto bar = remainingBar(percent / 100.0);
    int full = 0;
    for (const auto &led : bar) if (led.blue == 255) ++full;
    assert(full == percent / 20);
  }
  assert(remainingBar(.6)[2].blue == 255 && remainingBar(.6)[3].blue == 0);
  assert(remainingBar(.5)[2].red == 255 && remainingBar(.5)[2].green == 140);
  assert(remainingBar(.85)[4].red == 255 && remainingBar(.85)[4].green == 45);
  assert(remainingBar(.9)[4].red == 255 && remainingBar(.9)[4].green == 140);
  assert(remainingBar(.95)[4].red == 255 && remainingBar(.95)[4].green == 220);
  assert(remainingBar(.99)[4].red == 120 && remainingBar(.99)[4].green == 220);
  assert(remainingBar(1)[4].red == 0 && remainingBar(1)[4].blue == 255);
  for (size_t i = 0; i < 4; ++i) assert(remainingBar(.85)[i].blue == 255);
  assert(remainingBar(NAN)[0].blue == 0);
  assert(remainingBar(-1)[0].blue == 0 && remainingBar(2)[4].blue == 255);
  // The visual test exposes color order and every physical pixel; it ends automatically.
  assert(ledTest(0)[4].red == 255 && ledTest(0)[0].green == 0);
  assert(ledTest(1000)[0].green == 255 && ledTest(1000)[0].blue == 0);
  assert(ledTest(2000)[4].blue == 255 && ledTest(2000)[4].red == 0);
  for (size_t i = 0; i < ledCount; ++i) {
    const auto frame = ledTest(3000 + i * 1000);
    for (size_t j = 0; j < ledCount; ++j)
      assert(frame[j].red == (i == j ? 255 : 0) && frame[j].green == frame[j].red && frame[j].blue == frame[j].red);
  }
  assert(ledTest(8000)[0].blue == 0 && ledTest(11999)[4].blue == 255);
  for (const auto &led : ledTest(ledTestMs)) assert(led.red == 0 && led.green == 0 && led.blue == 0);
  auto invalid = [&](const Snapshot &snapshot, Problem problem) {
    const auto result = evaluate(snapshot, event, 0);
    assert(result.mode == Mode::Waiting && result.problem == problem);
    assert(std::isnan(result.remaining));
    const auto later = evaluate(snapshot, event, 230);
    bool animated = false;
    for (size_t i = 0; i < ledCount; ++i)
      animated |= result.leds[i].red != later.leds[i].red || result.leds[i].green != later.leds[i].green || result.leds[i].blue != later.leds[i].blue;
    assert(animated);
  };
  Snapshot changed = ready; changed.empty = NAN; invalid(changed, Problem::EmptyWeight);
  changed = ready; changed.empty = -1; invalid(changed, Problem::EmptyWeight);
  changed = ready; changed.gross = 0; invalid(changed, Problem::NoSpool);
  changed = ready; changed.fresh = false; invalid(changed, Problem::Stale);
  changed = ready; changed.stable = false; invalid(changed, Problem::Unstable);
  changed = ready; changed.calibrated = false; invalid(changed, Problem::Calibrate);
  changed = ready; changed.selected = false; invalid(changed, Problem::SelectSpool);
  changed = ready; changed.gross = 2001; invalid(changed, Problem::GrossRange);
  changed = ready; changed.gross = 199; invalid(changed, Problem::RemainingRange);
  changed = ready; changed.initial = 499; invalid(changed, Problem::RemainingRange);
  changed = ready; changed.initial = NAN; invalid(changed, Problem::RemainingRange);
  changed = ready; changed.gross = 200;
  view = evaluate(changed, event, 0);
  assert(view.remaining == 0 && view.leds[0].blue == 0);
  changed.gross = 1200;
  view = evaluate(changed, event, 0);
  assert(view.remaining == 1000 && view.leds[4].blue == 255);
  changed = ready; changed.setup = true;
  assert(evaluate(changed, event, 0).mode == Mode::Setup);
  assert(evaluate(changed, event, 800).leds[0].blue > evaluate(changed, event, 0).leds[0].blue);
  changed = ready; changed.connected = false; changed.selected = false; changed.connecting = true;
  assert(evaluate(changed, event, 0).mode == Mode::Connecting);
  changed.connecting = false;
  assert(evaluate(changed, event, 0).mode == Mode::Offline);
  changed.selected = true;
  assert(evaluate(changed, event, 500).mode == Mode::Remaining); // Offline weighing keeps the bar.
  assert(evaluate(changed, event, 500).leds[4].blue == 0);
  assert(evaluate(changed, event, 0).leds[4].blue == 255); // Brief network hint.
  changed = ready; changed.setup = true;
  event.begin(Operation::Nfc, 20);
  assert(evaluate(changed, event, 20).mode == Mode::BusyNfc);
  assert(evaluate(changed, event, 20).leds[0].red == 180);
  assert(evaluate(changed, event, 150).leds[1].red == 180);
  event.finish(true, 30);
  assert(evaluate(changed, event, 30 + resultMs - 1).mode == Mode::Success);
  assert(evaluate(changed, event, 30 + resultMs).mode == Mode::Setup);
  event.begin(Operation::Sync, 50);
  assert(evaluate(ready, event, 51).mode == Mode::BusySync);
  assert(std::isnan(evaluate(ready, event, 51).remaining));
  event.finish(false, UINT32_MAX - 100);
  assert(evaluate(ready, event, 0).mode == Mode::Failure);
  assert(evaluate(ready, event, resultMs).mode == Mode::Remaining);
  event.begin(Operation::Save, 100);
  assert(evaluate(ready, event, 100).mode == Mode::BusySave);
  event.finish(true, 101);
  changed = ready; changed.fresh = false;
  assert(std::isnan(evaluate(changed, event, 102).remaining));
  assert(evaluate(changed, event, 101 + resultMs).mode == Mode::Waiting);
}
