#pragma once
#include "Domain.h"
#include <array>

namespace StatusModel {
constexpr size_t ledCount = 5;
constexpr uint32_t resultMs = 2500;
enum class Operation { None, Nfc, Save, Sync };
enum class Phase { Idle, Busy, Success, Failure };
enum class Problem { None, Calibrate, Stale, Unstable, SelectSpool, EmptyWeight, GrossRange, RemainingRange };
enum class Mode { Remaining, Setup, Waiting, BusyNfc, BusySave, BusySync, Success, Failure };
inline bool nfcReadSucceeded(bool present, unsigned targets, bool error) {
  return present && targets == 1 && !error;
}
struct Snapshot {
  double gross = NAN, empty = NAN, initial = NAN;
  bool calibrated = false, stable = false, fresh = false, selected = false, setup = false;
};
struct Event {
  Operation operation = Operation::None;
  Phase phase = Phase::Idle;
  uint32_t started = 0;
  void begin(Operation value, uint32_t now) { operation = value; phase = Phase::Busy; started = now; }
  void finish(bool success, uint32_t now) { phase = success ? Phase::Success : Phase::Failure; started = now; }
  Phase at(uint32_t now) const {
    if ((phase == Phase::Success || phase == Phase::Failure) && uint32_t(now - started) >= resultMs) return Phase::Idle;
    return phase;
  }
};
struct Color { uint8_t red = 0, green = 0, blue = 0; };
struct View {
  double remaining = NAN;
  Problem problem = Problem::None;
  Mode mode = Mode::Waiting;
  Operation operation = Operation::None;
  std::array<Color, ledCount> leds{};
};
inline Problem measurementProblem(const Snapshot &s) {
  if (!s.calibrated) return Problem::Calibrate;
  if (!s.fresh) return Problem::Stale;
  if (!s.stable) return Problem::Unstable;
  if (!s.selected) return Problem::SelectSpool;
  if (!Domain::inRange(s.empty, 0, 2000)) return Problem::EmptyWeight;
  if (!Domain::inRange(s.gross, 20, 2000)) return Problem::GrossRange;
  if (!Domain::measurementAllowed(s.gross, s.empty, s.initial, s.calibrated, s.stable, s.fresh)) return Problem::RemainingRange;
  return Problem::None;
}
inline View evaluate(const Snapshot &s, const Event &event, uint32_t now) {
  View view;
  view.problem = measurementProblem(s);
  if (view.problem == Problem::None) view.remaining = s.gross - s.empty;
  const Phase phase = event.at(now);
  if (phase == Phase::Busy) {
    // Blocking NFC/HTTP work cannot refresh the display; do not hold a net reading on it.
    view.remaining = NAN;
    view.operation = event.operation;
    view.mode = event.operation == Operation::Nfc ? Mode::BusyNfc :
                event.operation == Operation::Sync ? Mode::BusySync : Mode::BusySave;
    view.leds.fill(event.operation == Operation::Nfc ? Color{180, 0, 255} : Color{0, 150, 255});
  } else if (phase == Phase::Success || phase == Phase::Failure) {
    view.operation = event.operation;
    view.mode = phase == Phase::Success ? Mode::Success : Mode::Failure;
    view.leds.fill(phase == Phase::Success ? Color{0, 255, 0} : Color{255, 0, 0});
  } else if (s.setup) {
    view.mode = Mode::Setup;
    const uint32_t step = now % 1600;
    const uint8_t pulse = 30 + (step < 800 ? step : 1600 - step) * 225 / 800;
    view.leds.fill(Color{0, 0, pulse});
  } else if (view.problem != Problem::None) {
    view.mode = Mode::Waiting;
    view.leds.fill(Color{255, 100, 0});
  } else {
    view.mode = Mode::Remaining;
    // Each complete LED is 20%; the next LED dims proportionally for the fraction.
    const double segments = view.remaining / s.initial * ledCount;
    for (size_t i = 0; i < ledCount; ++i) {
      const double fraction = std::fmin(1.0, std::fmax(0.0, segments - i));
      view.leds[i] = Color{0, uint8_t(std::lround(200 * fraction)), uint8_t(std::lround(255 * fraction))};
    }
  }
  return view;
}
inline const char *problemText(Problem problem) {
  switch (problem) {
    case Problem::Calibrate: return "Calibrate scale";
    case Problem::Stale: return "Waiting for scale";
    case Problem::Unstable: return "Keep spool still";
    case Problem::SelectSpool: return "Select/scan spool";
    case Problem::EmptyWeight: return "Set empty weight";
    case Problem::GrossRange: return "Need 20-2000g gross";
    case Problem::RemainingRange: return "Check spool weights";
    default: return "Ready";
  }
}
inline const char *operationText(Operation operation) {
  switch (operation) {
    case Operation::Nfc: return "NFC";
    case Operation::Save: return "Save";
    case Operation::Sync: return "Spoolman";
    default: return "Operation";
  }
}
}
