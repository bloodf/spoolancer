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
  assert(view.leds[2].blue == 128 && view.leds[3].blue == 0 && view.leds[4].blue == 0);
  auto invalid = [&](const Snapshot &snapshot, Problem problem) {
    const auto result = evaluate(snapshot, event, 0);
    assert(result.mode == Mode::Waiting && result.problem == problem);
    assert(std::isnan(result.remaining));
    assert(result.leds[0].red == 255 && result.leds[0].green == 100);
  };
  Snapshot changed = ready; changed.empty = NAN; invalid(changed, Problem::EmptyWeight);
  changed = ready; changed.empty = -1; invalid(changed, Problem::EmptyWeight);
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
  event.begin(Operation::Nfc, 20);
  assert(evaluate(changed, event, 20).mode == Mode::BusyNfc);
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
