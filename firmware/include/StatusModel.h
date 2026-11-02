#pragma once
#include "Domain.h"
#include <array>
#include <algorithm>

namespace StatusModel {
constexpr size_t ledCount = 5;
constexpr uint32_t resultMs = 2500, ledTestMs = 12000;
enum class Operation { None, Nfc, Save, Sync, Scale };
enum class Phase { Idle, Busy, Success, Failure };
enum class Problem { None, Calibrate, Stale, Unstable, SelectSpool, EmptyWeight, GrossRange, RemainingRange, NoSpool };
enum class Mode { Remaining, Setup, Waiting, BusyNfc, BusySave, BusySync, Success, Failure, Connecting, Offline, LedTest };
inline bool nfcReadSucceeded(bool present, unsigned targets, bool error) {
  return present && targets == 1 && !error;
}
struct Snapshot {
  double gross = NAN, empty = NAN, initial = NAN;
  bool calibrated = false, stable = false, fresh = false, selected = false, setup = false;
  bool connected = true, connecting = false;
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
  if (std::isfinite(s.gross) && s.gross < 20 && s.gross >= -5) return Problem::NoSpool;
  if (!s.stable) return Problem::Unstable;
  if (!s.selected) return Problem::SelectSpool;
  if (!Domain::inRange(s.empty, 0, 2000)) return Problem::EmptyWeight;
  if (!Domain::inRange(s.gross, 20, 2000)) return Problem::GrossRange;
  if (!Domain::measurementAllowed(s.gross, s.empty, s.initial, s.calibrated, s.stable, s.fresh)) return Problem::RemainingRange;
  return Problem::None;
}
inline uint8_t pulse(uint32_t now, uint32_t period, uint8_t low = 20, uint8_t high = 255) {
  const uint32_t half = period / 2, step = now % period;
  return low + (step < half ? step : period - step) * (high - low) / half;
}
inline Color dim(Color color, double fraction) {
  return {uint8_t(std::lround(color.red * fraction)), uint8_t(std::lround(color.green * fraction)), uint8_t(std::lround(color.blue * fraction))};
}
inline Color segmentColor(double fraction) {
  if (fraction <= 0) return {};
  if (fraction >= 1 - 1e-9) return {0, 200, 255};
  if (fraction <= .25 + 1e-9) return {255, 45, 45};
  if (fraction <= .5 + 1e-9) return {255, 140, 0};
  if (fraction <= .75 + 1e-9) return {255, 220, 0};
  return {120, 220, 50};
}
inline std::array<Color, ledCount> remainingBar(double fraction) {
  std::array<Color, ledCount> leds{};
  if (!std::isfinite(fraction)) return leds;
  const double segments = std::fmin(1.0, std::fmax(0.0, fraction)) * ledCount;
  for (size_t i = 0; i < ledCount; ++i)
    leds[i] = segmentColor(std::fmin(1.0, std::fmax(0.0, segments - i)));
  return leds;
}
inline std::array<Color, ledCount> ledTest(uint32_t elapsed) {
  std::array<Color, ledCount> leds{};
  if (elapsed >= ledTestMs) return leds;
  if (elapsed < 3000) {
    const Color colors[] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    leds.fill(colors[elapsed / 1000]);
  } else if (elapsed < 8000) leds[elapsed / 1000 - 3] = {255, 255, 255};
  else leds = remainingBar(double(elapsed - 8000) / 3999);
  return leds;
}
inline View evaluate(const Snapshot &s, const Event &event, uint32_t now) {
  View view;
  view.problem = measurementProblem(s);
  if (view.problem == Problem::None) view.remaining = s.gross - s.empty;
  const Phase phase = event.at(now);
  if (phase == Phase::Busy) {
    view.remaining = NAN; view.operation = event.operation;
    view.mode = event.operation == Operation::Nfc ? Mode::BusyNfc :
                event.operation == Operation::Sync ? Mode::BusySync : Mode::BusySave;
    const Color color = event.operation == Operation::Nfc ? Color{180, 0, 255} : Color{0, 150, 255};
    const size_t head = ((now - event.started) / 130) % ledCount;
    for (size_t i = 0; i < ledCount; ++i) view.leds[i] = dim(color, i == head ? 1.0 : .08);
  } else if (phase == Phase::Success || phase == Phase::Failure) {
    view.operation = event.operation;
    view.mode = phase == Phase::Success ? Mode::Success : Mode::Failure;
    const uint32_t elapsed = now - event.started;
    if (phase == Phase::Success) {
      const size_t filled = std::min(ledCount, size_t(elapsed / 90 + 1));
      for (size_t i = 0; i < filled; ++i) view.leds[i] = {0, pulse(elapsed, 1000, 90, 255), 0};
    } else if (elapsed % 900 < 150 || (elapsed % 900 >= 300 && elapsed % 900 < 450))
      view.leds.fill({255, 0, 0});
  } else if (s.setup) {
    view.mode = Mode::Setup; view.leds.fill({0, 0, pulse(now, 1600)});
  } else if (!s.selected && !s.connected && s.connecting) {
    view.mode = Mode::Connecting; view.leds[(now / 180) % ledCount] = {0, 70, 255};
  } else if (!s.selected && !s.connected) {
    view.mode = Mode::Offline;
    if (now % 1800 < 150 || (now % 1800 >= 300 && now % 1800 < 450)) view.leds.fill({0, 70, 255});
  } else if (view.problem != Problem::None) {
    view.mode = Mode::Waiting;
    switch (view.problem) {
      case Problem::NoSpool: view.leds[2] = {pulse(now, 2400, 4, 40), pulse(now, 2400, 4, 40), pulse(now, 2400, 4, 40)}; break;
      case Problem::Stale: view.leds.fill({pulse(now, 900), 0, 0}); break;
      case Problem::Unstable: {
        const size_t step = (now / 150) % 8;
        view.leds[step < ledCount ? step : 8 - step] = {255, 100, 0}; break;
      }
      case Problem::SelectSpool: view.leds.fill({0, pulse(now, 1800, 10, 100), pulse(now, 1800, 10, 100)}); break;
      case Problem::EmptyWeight:
        view.leds[0] = view.leds[4] = {pulse(now, 1400), uint8_t(pulse(now, 1400) / 3), 0}; break;
      case Problem::Calibrate:
        if (now % 1400 < 180 || (now % 1400 >= 360 && now % 1400 < 540)) view.leds.fill({255, 100, 0});
        break;
      default:
        for (size_t i = 0; i < ledCount; ++i) if ((i + now / 220) % 2 == 0) view.leds[i] = {255, 0, 0};
        break;
    }
  } else {
    view.mode = Mode::Remaining;
    const double fraction = view.remaining / s.initial;
    view.leds = remainingBar(fraction);
    if (fraction == 0) view.leds[0] = {pulse(now, 1800), 0, 0};
    else if (fraction <= .1) view.leds[0] = dim(view.leds[0], pulse(now, 1800, 80, 255) / 255.0);
    // A brief network hint leaves the weight bar visible for most of each cycle.
    if (!s.connected && now % 5000 < 180) view.leds[4] = {0, 70, 255};
  }
  return view;
}
inline const char *problemText(Problem problem) {
  switch (problem) {
    case Problem::Calibrate: return "Calibrate scale";
    case Problem::NoSpool: return "Place a spool";
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
    case Operation::Scale: return "Scale test";
    case Operation::Sync: return "Spoolman";
    default: return "Operation";
  }
}
}
