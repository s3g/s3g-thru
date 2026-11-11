#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Scheduling adapted from s3g Opps src/core/engine.cpp and model.h,
// d3f4ed8d6e3b1c3d5299bc23212308cf932813aa (BSD-3-Clause, Shawn Greenlee).
// Thru adapter: four sample lanes, 32 steps, inherited trig values, no
// independent value cycles. Events use musical time and rounded sample offsets.
namespace s3g_thru_sequence {
constexpr int lanes = 4, steps = 32, children = 8;
constexpr std::array<double, 9> rates{4,       2,    1,        .5,   .25,
                                      1.0 / 6, .125, 1.0 / 12, .0625};
constexpr std::array<const char *, 9> rateNames{
    "1/1", "1/2", "1/4", "1/8", "1/16", "1/16T", "1/32", "1/32T", "1/64"};
struct Values {
  std::uint8_t mask = 0;
  std::array<float, 4> values{
      0, 0, 100, .8f}; // sample position, root offset, velocity, gate in steps
  bool timeLocked = false;
  float timeMs = 1000;
};
struct Child {
  float when = 0;
  Values locks;
};
struct Step {
  bool enabled = false;
  float micro = 0, probability = 1;
  int count = 1;
  Values locks;
  std::array<Child, children> events{};
};
struct Lane {
  int length = 16, rate = 4, rotation = 0;
  bool enabled = true, scaleEntry = false, timeMode = false;
  float velocity = 100, gate = .8f, timeMs = 1000;
  std::array<Step, steps> trigs{};
};
struct Strip {
  float pan = 0, lowDb = 0, midDb = 0, highDb = 0, midHz = 900;
  bool solo = false;
};
struct Mix {
  std::array<Strip, lanes> strips{};
  int priority = 0; // Off, or the one-based protected lane.
  float depthDb = 0, thresholdDb = -24, attackMs = 12, holdMs = 40;
  float releaseMs = 220, hpfHz = 100, kneeDb = 6;
};
struct Pattern {
  bool active = false;
  std::array<Lane, lanes> tracks{};
  Mix mix{};
};
inline std::uint32_t hash(std::uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}
inline int padStep(int pad) { return (3 - pad / 8) * 8 + pad % 8; }
inline int stepPad(int step) { return (3 - step / 8) * 8 + step % 8; }
inline float resolve(const Lane &lane, const Step &step, int child, int field,
                     float inherited) {
  float value = field == 2 ? lane.velocity : field == 3 ? lane.gate : inherited;
  if (step.locks.mask & (1u << field))
    value = step.locks.values[field];
  if (child >= 0 && (step.events[child].locks.mask & (1u << field)))
    value = step.events[child].locks.values[field];
  return value;
}
inline float resolveTime(const Lane &lane, const Step &step, int child) {
  float ms = lane.timeMs;
  if (step.locks.timeLocked)
    ms = step.locks.timeMs;
  if (child >= 0 && step.events[child].locks.timeLocked)
    ms = step.events[child].locks.timeMs;
  return ms;
}
inline void setCount(Step &step, int count) {
  step.count = std::clamp(count, 1, children);
  for (int i = 0; i < step.count; ++i)
    step.events[i].when = float(i) / step.count;
}
struct Event {
  int offset = 0, lane = 0, step = 0, child = 0;
  double gateBeats = 0;
  std::int64_t tick = 0;
};
struct EventList {
  std::array<Event, 128> items{};
  int size = 0;
  bool overflow = false;
};
inline void schedule(const Pattern &pattern, double start, double increment,
                     int frames, EventList &out) {
  out.size = 0;
  out.overflow = false;
  for (int t = 0; t < lanes; ++t) {
    const auto &track = pattern.tracks[t];
    if (!track.enabled)
      continue;
    const double span = rates[track.rate];
    const auto first = std::int64_t(std::floor(start / span)) - 2;
    const auto last =
        std::int64_t(std::ceil((start + frames * increment) / span)) + 1;
    for (auto n = std::max<std::int64_t>(0, first); n <= last; ++n) {
      const int row = int((n + track.rotation) % track.length);
      const auto &step = track.trigs[row];
      if (!step.enabled || step.probability <= 0)
        continue;
      const auto random =
          hash(1979u ^ std::uint32_t(n) ^ std::uint32_t(n >> 32) ^
               (0x9e3779b9u * (t + 1)));
      if (double(random) / 4294967296.0 >= step.probability)
        continue;
      for (int c = 0; c < step.count; ++c) {
        const double authored = (n + step.micro + step.events[c].when) * span;
        const double onset = n == 0 ? std::max(0.0, authored) : authored;
        const double relative = (onset - start) / increment;
        if (relative < -1 || relative > frames + 1)
          continue;
        const int offset = int(std::floor(relative + .5 + 1e-7));
        if (offset < 0 || offset >= frames)
          continue;
        if (out.size == int(out.items.size())) {
          out.overflow = true;
          continue;
        }
        out.items[out.size++] = {
            offset, t, row, c, span * resolve(track, step, c, 3, track.gate),
            n};
      }
    }
  }
  std::sort(out.items.begin(), out.items.begin() + out.size,
            [](const auto &a, const auto &b) {
              if (a.offset != b.offset)
                return a.offset < b.offset;
              if (a.lane != b.lane)
                return a.lane < b.lane;
              if (a.step != b.step)
                return a.step < b.step;
              return a.child < b.child;
            });
}
// Fixed extension encoding, independent of C++ struct layout. Existing
// formats omit this whole extension and construct a disabled Pattern.
constexpr int valuesSize = 5, childSize = 6,
              stepSize = 9 + children * childSize;
constexpr int laneSize = 7 + steps * stepSize,
              patternSize = 1 + lanes * laneSize;
inline float &scalar(Pattern &p, int index, float &scratch) {
  if (index == 0) {
    scratch = p.active ? 1.f : 0.f;
    return scratch;
  }
  --index;
  auto &lane = p.tracks[index / laneSize];
  index %= laneSize;
  if (index < 7) {
    switch (index) {
    case 0:
      scratch = float(lane.length);
      break;
    case 1:
      scratch = float(lane.rate);
      break;
    case 2:
      scratch = float(lane.rotation);
      break;
    case 3:
      scratch = lane.enabled ? 1.f : 0.f;
      break;
    case 4:
      return lane.velocity;
    case 5:
      return lane.gate;
    default:
      scratch = lane.scaleEntry ? 1.f : 0.f;
    }
    return scratch;
  }
  index -= 7;
  auto &step = lane.trigs[index / stepSize];
  index %= stepSize;
  if (index == 0) {
    scratch = step.enabled ? 1.f : 0.f;
    return scratch;
  }
  if (index == 1)
    return step.micro;
  if (index == 2)
    return step.probability;
  if (index == 3) {
    scratch = float(step.count);
    return scratch;
  }
  index -= 4;
  auto *lock = &step.locks;
  if (index >= 5) {
    index -= 5;
    auto &child = step.events[index / childSize];
    index %= childSize;
    if (index == 0)
      return child.when;
    --index;
    lock = &child.locks;
  }
  if (index == 0) {
    scratch = float(lock->mask);
    return scratch;
  }
  return lock->values[index - 1];
}
inline float get(const Pattern &p, int index) {
  float scratch = 0;
  return scalar(const_cast<Pattern &>(p), index, scratch);
}
inline bool set(Pattern &p, int index, float value) {
  if (!std::isfinite(value) || index < 0 || index >= patternSize)
    return false;
  const auto integer = [&](int low, int high) {
    return value >= low && value <= high && std::floor(value) == value;
  };
  if (index == 0) {
    if (!integer(0, 1))
      return false;
    p.active = value != 0;
    return true;
  }
  --index;
  auto &lane = p.tracks[index / laneSize];
  index %= laneSize;
  if (index < 7) {
    switch (index) {
    case 0:
      if (!integer(1, 32))
        return false;
      lane.length = int(value);
      break;
    case 1:
      if (!integer(0, 8))
        return false;
      lane.rate = int(value);
      break;
    case 2:
      if (!integer(0, 31))
        return false;
      lane.rotation = int(value);
      break;
    case 3:
      if (!integer(0, 1))
        return false;
      lane.enabled = value != 0;
      break;
    case 4:
      if (!integer(1, 127))
        return false;
      lane.velocity = value;
      break;
    case 5:
      if (value < .01f || value > 32)
        return false;
      lane.gate = value;
      break;
    default:
      if (!integer(0, 1))
        return false;
      lane.scaleEntry = value != 0;
    }
    return true;
  }
  index -= 7;
  auto &step = lane.trigs[index / stepSize];
  index %= stepSize;
  if (index == 0) {
    if (!integer(0, 1))
      return false;
    step.enabled = value != 0;
    return true;
  }
  if (index == 1) {
    if (value < -.5f || value > .5f)
      return false;
    step.micro = value;
    return true;
  }
  if (index == 2) {
    if (value < 0 || value > 1)
      return false;
    step.probability = value;
    return true;
  }
  if (index == 3) {
    if (!integer(1, 8))
      return false;
    step.count = int(value);
    return true;
  }
  index -= 4;
  Values *lock = &step.locks;
  if (index >= 5) {
    index -= 5;
    auto &child = step.events[index / childSize];
    index %= childSize;
    if (index == 0) {
      if (value < 0 || value > .999f)
        return false;
      child.when = value;
      return true;
    }
    --index;
    lock = &child.locks;
  }
  if (index == 0) {
    if (!integer(0, 15))
      return false;
    lock->mask = std::uint8_t(value);
    return true;
  }
  const float lo[]{0, -48, 1, .01f}, hi[]{1, 48, 127, 32};
  const int field = index - 1;
  if (value < lo[field] || value > hi[field] ||
      ((field == 1 || field == 2) && std::floor(value) != value))
    return false;
  lock->values[field] = value;
  return true;
}
struct Voice {
  bool active = false, audition = false;
  int step = -1, child = -1;
  float position = 0, startPhase = 0, attack = 0, decay = 0, release = 3;
  std::uint64_t elapsed = 0, frames = 0, releaseAt = 0;
};
} // namespace s3g_thru_sequence
