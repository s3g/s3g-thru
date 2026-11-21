#pragma once
#include "thru_control_lfo_destinations.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace s3g_thru_lfo {
constexpr int slots = 8, fields = 9, poolFields = 12, routeSlots = 8;
enum Field { destination, wave, clock, rate, depth, mode, phase, smooth, flow };
enum Destination {
  off,
  level,
  pan,
  tone,
  pitch,
  grainSize,
  grainSpray,
  delaySend,
  reverbSend,
  layersX,
  layersY,
  trigPosition,
  trigNote,
  trigVelocity,
  trigDuration,
  destinationCount
};
enum Offset { baseOffset, autoOffset };
struct Modulation {
  float base = 0, automatic = 0, span = 0;
  bool routed = false;
};
enum Run { freeRun, transport, noteRestart, oneShot };
inline constexpr std::array<float, fields> defaults{0, 0, 0, 1, 0, 0, 0, 10, 0};
inline constexpr std::array<float, fields> low{0, 0, 0, 1.f / 256, -1,
                                               0, 0, 3, 0},
    high{float(destinations.size() - 1), 6, 1, 16, 1, 3, 1, 1000, 1};
inline constexpr std::array<float, 25> beats{
    .0625f, 1.f / 12, .125f, 1.f / 6, .25f, 1.f / 3, .5f, 2.f / 3, 1,
    1.5f,   2,        3,     4,       6,    8,       12,  16,      24,
    32,     48,       64,    96,      128,  192,     256};
inline constexpr const char *beatNames[]{
    "1/64",  "1/32T", "1/32",  "1/16T", "1/16", "1/8T",  "1/8",
    "1/4T",  "1/4",   "1/4D",  "1/2",   "1/2D", "1BAR",  "1.5BAR",
    "2BAR",  "3BAR",  "4BAR",  "6BAR",  "8BAR", "12BAR", "16BAR",
    "24BAR", "32BAR", "48BAR", "64BAR"};
struct RouteId {
  int part = -1, engine = -1, dest = 0;
  bool operator==(const RouteId &b) const {
    return part == b.part && engine == b.engine && dest == b.dest;
  }
};
struct Route {
  RouteId id{};
  float value = 0, from = 0;
  float automatic = 0, autoFrom = 0, span = 0, spanFrom = 0;
};
struct Config {
  std::array<float, fields> v = defaults;
  double anchor = 0;
  std::uint32_t seed = 0x53474c46u, cycle = 0;
  bool captured = false, runningAnchor = false;
  int target = 0, engine = -1, offset = baseOffset;
  float spanAnchor = 0;
  std::array<Route, routeSlots> poolRoutes{};
  float outputAnchor = 0;
  std::array<float, destinationCount> routeAnchor{};
};
using Part = std::array<Config, 2>;
using Pool = std::array<Config, slots>;
inline bool integerField(int f) {
  return f == destination || f == wave || f == clock || f == mode || f == flow;
}
inline int beatIndex(float speed) {
  int best = 0;
  float distance = 1e30f;
  for (int i = 0; i < int(beats.size()); ++i) {
    const float d = std::abs(std::log2(std::max(speed, 1e-9f) * beats[i]));
    if (d < distance) {
      distance = d;
      best = i;
    }
  }
  return best;
}
inline bool set(Config &c, int f, float x) {
  if (f < 0 || f >= fields || !std::isfinite(x))
    return false;
  x = std::clamp(x, low[f], high[f]);
  if (integerField(f))
    x = std::round(x);
  if (f == rate && c.v[clock] == 0)
    x = 1 / beats[beatIndex(x)];
  c.v[f] = x;
  if (f == clock && x == 0)
    c.v[rate] = 1 / beats[beatIndex(c.v[rate])];
  return true;
}
inline bool valid(const Config &c) {
  if (c.target < 0 || c.target > 4 || c.engine < -1 || c.engine > 14 ||
      c.offset < baseOffset || c.offset > autoOffset ||
      !std::isfinite(c.spanAnchor) || c.spanAnchor < 0 || c.spanAnchor > 1.001f)
    return false;
  for (const auto &r : c.poolRoutes)
    if (r.id.part < -1 || r.id.part > 4 || r.id.engine < -1 ||
        r.id.engine > 14 || r.id.dest < 0 ||
        r.id.dest >= int(destinations.size()) || !std::isfinite(r.value) ||
        std::abs(r.value) > 1.001f || !std::isfinite(r.automatic) ||
        !std::isfinite(r.span) || r.span < 0 || r.span > 1.001f ||
        std::abs(r.automatic) > r.span + .0001f)
      return false;
  if (!std::isfinite(c.anchor) || c.anchor < 0 || c.anchor >= 1)
    return false;
  if (!std::isfinite(c.outputAnchor) || std::abs(c.outputAnchor) > 1.001f)
    return false;
  float sum = 0;
  for (float x : c.routeAnchor) {
    if (!std::isfinite(x) || x < 0 || x > 1.001f)
      return false;
    sum += x;
  }
  if (sum > 1.001f)
    return false;
  for (int f = 0; f < fields; ++f) {
    const float x = c.v[f];
    if (!std::isfinite(x) || x < low[f] || x > high[f] ||
        (integerField(f) && x != std::round(x)))
      return false;
  }
  return true;
}
inline std::uint32_t hash(std::uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}
inline float random(std::uint32_t seed, std::uint32_t cycle) {
  return float(hash(seed ^ hash(cycle + 0x9e3779b9u)) >> 8) *
             (2.f / 16777215.f) -
         1;
}
inline float value(int shape, double p, std::uint32_t cycle,
                   std::uint32_t seed) {
  const float x = float(p);
  switch (shape) {
  case 1:
    return 1 - 4 * std::abs(x - .5f);
  case 2:
    return x * 2 - 1;
  case 3:
    return 1 - x * 2;
  case 4:
    return x < .5f ? 1.f : -1.f;
  case 5:
    return random(seed, cycle);
  case 6: {
    const float w = x * x * (3 - 2 * x);
    const float a = random(seed, cycle);
    return a + (random(seed, cycle + 1) - a) * w;
  }
  default:
    return std::sin(float(6.283185307179586 * p));
  }
}
struct Runtime {
  double position = 0;
  std::uint32_t cycle = 0;
  float output = 0, from = 0, to = 0, spanEnvelope = 0;
  bool running = false, initialized = false;
  int lastMode = 0;
  float lastPhase = 0;
  std::array<Route, routeSlots> routes{};
  float rateOverride = 0, depthOverride = 0;
  std::uint8_t overrides = 0;
  void reset(const Config &c, bool capture = true) {
    initialized = true;
    position =
        capture && c.captured ? c.anchor : std::fmod(double(c.v[phase]), 1.0);
    cycle = capture && c.captured ? c.cycle : 0;
    running = capture && c.captured ? c.runningAnchor : c.v[mode] != oneShot;
    lastMode = int(c.v[mode]);
    lastPhase = c.v[phase];
    overrides = 0;
    output = capture && c.captured ? c.outputAnchor : 0;
    from = to = output;
    spanEnvelope =
        capture && c.captured ? std::max(c.spanAnchor, std::abs(output)) : 0;
    routes =
        capture && c.captured ? c.poolRoutes : std::array<Route, routeSlots>{};
    for (auto &r : routes) {
      r.from = r.value;
      r.autoFrom = r.automatic;
      r.spanFrom = r.span;
    }
  }
  void synchronizePhase(const Config &c) {
    if (lastPhase != c.v[phase]) {
      position = std::fmod(double(c.v[phase]), 1.0);
      cycle = 0;
      lastPhase = c.v[phase];
    }
  }
  void trigger(const Config &c, bool permit = true) {
    if (!initialized)
      reset(c);
    synchronizePhase(c);
    if (permit && (c.v[mode] == noteRestart || c.v[mode] == oneShot)) {
      position = std::fmod(double(c.v[phase]), 1.0);
      cycle = 0;
      running = true;
      lastMode = int(c.v[mode]);
    }
  }
  float raw(const Config &c) const {
    return (c.v[mode] == oneShot && !running
                ? 0
                : value(int(c.v[wave]), position, cycle, c.seed)) *
           (overrides & 2 ? depthOverride : c.v[depth]);
  }
  void advance(const Config &c, int frames, double sr, double bpm,
               double clipSpeed, bool playing) {
    if (!initialized)
      reset(c);
    from = output;
    for (auto &r : routes) {
      r.from = r.value;
      r.autoFrom = r.automatic;
      r.spanFrom = r.span;
    }
    synchronizePhase(c);
    const int run = int(c.v[mode]);
    if (lastMode != run) {
      if (run == oneShot)
        running = false;
      lastMode = run;
    }
    const bool go = run == freeRun || run == noteRestart ||
                    (run == transport && playing) ||
                    (run == oneShot && running);
    if (go) {
      const double speed = overrides & 1 ? rateOverride : c.v[rate];
      const double scale = playing && c.v[flow] == 0 ? clipSpeed : 1;
      position +=
          speed * frames / sr * (c.v[clock] == 0 ? bpm / 60 : 1) * scale;
      if (position >= 1) {
        const auto n = std::uint32_t(std::floor(position));
        cycle += n;
        position -= n;
        if (run == oneShot) {
          position = 0;
          running = false;
        }
      }
    }
    if (c.v[destination] == 0 && !routing()) {
      from = to = output = spanEnvelope = 0;
      return;
    }
    const float target = raw(c);
    const float slew =
        1 - std::exp(-float(frames) / (float(sr) * c.v[smooth] * .001f));
    routeSlew = slew;
    const float peak =
        c.v[mode] == oneShot && !running
            ? 0
            : std::abs(overrides & 2 ? depthOverride : c.v[depth]);
    spanEnvelope += (peak - spanEnvelope) * slew;
    if (spanEnvelope < 1e-20f)
      spanEnvelope = 0;
    output += (target - output) * slew;
    if (std::abs(output) < 1e-20f)
      output = 0;
    to = output;
  }
  void route(const Config &c, bool available, int engineFilter) {
    const RouteId wanted{c.target, engineFilter, int(c.v[destination])};
    Route *selected = nullptr;
    if (available && wanted.dest)
      for (auto &r : routes)
        if (r.id == wanted) {
          selected = &r;
          break;
        }
    if (available && wanted.dest && !selected)
      for (auto &r : routes)
        if (r.id.part < 0) {
          r.id = wanted;
          r.value = r.from = 0;
          selected = &r;
          break;
        }
    const float slew = routeSlew;
    for (auto &r : routes) {
      if (r.id.part < 0)
        continue;
      r.value += ((selected == &r ? output : 0) - r.value) * slew;
      const bool automatic = selected == &r && c.offset == autoOffset;
      r.automatic += ((automatic ? output : 0) - r.automatic) * slew;
      r.span += ((automatic ? spanEnvelope : 0) - r.span) * slew;
      if (std::abs(r.value) < 1e-20f)
        r.value = 0;
      if (std::abs(r.automatic) < 1e-20f)
        r.automatic = 0;
      if (r.span < 1e-20f)
        r.span = 0;
      if (&r != selected && std::abs(r.value) < 1e-6f &&
          std::abs(r.automatic) < 1e-6f && r.span < 1e-6f) {
        r = {};
      }
    }
  }
  float routeSlew = 1;
  bool routing() const {
    for (const auto &r : routes)
      if (r.id.part >= 0)
        return true;
    return false;
  }
};
struct TrigLocks {
  std::uint16_t mask = 0;
  std::uint8_t reset = 255;
  std::array<float, slots> rates{1, 1, 1, 1, 1, 1, 1, 1}, depths{};
};
inline bool valid(const TrigLocks &t) {
  for (int i = 0; i < slots; ++i)
    if (!std::isfinite(t.rates[i]) || t.rates[i] < low[rate] ||
        t.rates[i] > high[rate] || !std::isfinite(t.depths[i]) ||
        std::abs(t.depths[i]) > 1)
      return false;
  return true;
}
template <class Append>
void appendJson(Append append, int group, const Part &part) {
  for (int slot = 0; slot < 2; ++slot) {
    const auto &c = part[slot];
    append(",\"lfo_%d_%d\":[", group, slot);
    for (int f = 0; f < fields; ++f)
      append("%s%.9g", f ? "," : "", double(c.v[f]));
    append(",%.17g,%u,%u,%d,%.9g", c.anchor, unsigned(c.seed),
           unsigned(c.cycle), c.captured ? 1 : 0, double(c.outputAnchor));
    for (float x : c.routeAnchor)
      append(",%.9g", double(x));
    append(",%d]", c.runningAnchor ? 1 : 0);
  }
}
template <class Append> void appendPoolJson(Append append, const Pool &pool) {
  append("%s", ",\"lfo_pool\":[");
  for (int slot = 0; slot < 8; ++slot) {
    const auto &c = pool[slot];
    append("%s[", slot ? "," : "");
    for (int f = 0; f < 9; ++f)
      append("%s%.9g", f ? "," : "", double(c.v[f]));
    append(",%d,%d,%.17g,%u,%u,%d,%.9g,%d", c.target, c.engine, c.anchor,
           unsigned(c.seed), unsigned(c.cycle), c.captured ? 1 : 0,
           double(c.outputAnchor), c.runningAnchor ? 1 : 0);
    for (const auto &r : c.poolRoutes)
      append(",%d,%d,%d,%.9g", r.id.part, r.id.engine, r.id.dest,
             double(r.value));
    append("%s", "]");
  }
  append("%s", "],\"lfo_auto\":[");
  for (int slot = 0; slot < 8; ++slot) {
    const auto &c = pool[slot];
    append("%s[%d,%.9g", slot ? "," : "", c.offset, double(c.spanAnchor));
    for (const auto &r : c.poolRoutes)
      append(",%.9g,%.9g", double(r.automatic), double(r.span));
    append("%s", "]");
  }
  append("%s", "]");
}
} // namespace s3g_thru_lfo
