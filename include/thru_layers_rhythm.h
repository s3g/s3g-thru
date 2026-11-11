#pragma once
#include "thru_spectral.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <new>

namespace s3g_thru_layers {
struct RhythmClock {
  float bpm = 120;
  double beat = 0, position = -1;
  std::uint64_t launch = 0;
  bool playing = false, shared = false;
};
inline float rhythmBeats(float size) {
  constexpr float lengths[]{.03125f, .0625f, .125f, .25f, .5f, 1, 2, 4};
  return lengths[std::clamp(int(std::lround(size * 7)), 0, 7)];
}
inline float rhythmMilliseconds(int algorithm, float size, bool sync,
                                float bpm) {
  if (sync)
    return rhythmBeats(size) * 60000 / std::clamp(bpm, 40.f, 240.f);
  if (algorithm == 14)
    return 2000 / std::pow(160.f, size);
  if (algorithm == 15)
    return 80 * std::pow(25.f, size);
  if (algorithm == 16)
    return 50 * std::pow(80.f, size);
  return 20 * std::pow(100.f, size);
}
// Shared, precomputed polyphase readers for upward-transposed repeats. Table
// construction happens during prepare, never at a live capture or dial edit.
// Wider kernels at two octaves keep energy near the folding edge suppressed;
// centered reads use captured history, so filtering adds no branch latency.
struct RhythmSinc {
  static constexpr int maxTaps = 64, readMargin = maxTaps / 2 + 2;
  static int taps(int pitch) { return pitch <= 12 ? 32 : 64; }
  std::array<std::array<std::array<float, maxTaps>, 65>, 25> kernels{};
  RhythmSinc() {
    constexpr double pi = 3.14159265358979323846;
    for (int pitch = 1; pitch <= 24; ++pitch)
      for (int phase = 0; phase <= 64; ++phase) {
        const int count = taps(pitch);
        const double cutoff = .34 / std::exp2(pitch / 12.0),
                     fraction = phase / 64.0;
        double sum = 0;
        for (int tap = 0; tap < count; ++tap) {
          const double x = tap - (count / 2 - 1) - fraction;
          const double window = .42 -
                                .5 * std::cos(2 * pi * tap / (count - 1)) +
                                .08 * std::cos(4 * pi * tap / (count - 1));
          const double value =
              (std::abs(x) < 1e-12 ? 2 * cutoff
                                   : std::sin(2 * pi * cutoff * x) / (pi * x)) *
              window;
          kernels[pitch][phase][tap] = float(value);
          sum += value;
        }
        for (auto &value : kernels[pitch][phase])
          value = float(value / sum);
      }
  }
  static const RhythmSinc &get() {
    static const RhythmSinc table;
    return table;
  }
};

// One continuously written bank plus one held bank. A capture freezes a view
// of valid history in O(1); ownership changes only after the old reader fades.
// Bank zero borrows Slot's existing delay storage. No full buffer copy/clear
// occurs on the audio thread, including hold, recapture and live resizing.
class Rhythm {
  struct Snapshot {
    int bank = 0;
    std::size_t end = 0, span = 0;
    bool valid = false;
  };
  struct Reader {
    Snapshot source;
    double start = 0, period = 32, seam = 1;
    float speed = 1, level = 1;
    double position = 0;
    bool reverse = false, oneShot = false;
    int pitch = 0;
  };
  std::unique_ptr<float[]> owned_, extra_;
  std::array<float *, 2> bank_{};
  std::size_t capacity_ = 0, write_ = 0, valid_ = 0;
  int writer_ = 0, retireTo_ = -1, algorithm_ = 13;
  float rate_ = 44100, bpm_ = 120, amplitude_ = 1, amplitudeSlew_ = 0,
        brakeMix_ = 0, pan_ = 0;
  float fade_ = 1, fadeStep_ = 1, brakeElapsed_ = 0, periodFrames_ = 1,
        beatPeriod_ = 1;
  float seamFrames_ = 1, recoveryFrames_ = 1, brakeSlew_ = 0;
  float velocitySlew_ = 0, stopSlew_ = 0, panSlew_ = 0;
  std::array<float, 4> p_{}, a_{}, pendingP_{}, pendingA_{};
  Snapshot captured_;
  Reader current_, old_;
  RhythmClock context_;
  double beat_ = 0, origin_ = 0, seconds_ = 0, lastPosition_ = -1;
  std::uint64_t launch_ = 0, captures_ = 0;
  std::uint64_t written_ = 0, firstSignal_ = 0, lastSignal_ = 0;
  bool signalSeen_ = false;
  std::int64_t cycle_ = -1;
  int previousSlice_ = 0;
  bool engaged_ = false, wasEngaged_ = false, wasPlaying_ = false,
       needCapture_ = true;
  bool ready_ = false, dirty_ = false, brakeDone_ = false;

  static float window(double x) {
    x = std::clamp(x, 0.0, 1.0);
    return float(x * x * x * (10 + x * (-15 + 6 * x)));
  }
  static float unit(float x) {
    return std::isfinite(x) ? std::clamp(x, 0.f, 1.f) : 0.f;
  }
  static std::uint32_t hash(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
  }
  float random(std::int64_t step, int salt) const {
    const int seed = int(std::lround(a_[algorithm_ == 14 ? 3 : 2] * 127));
    return float(hash(std::uint32_t(step) ^ hash(std::uint32_t(seed + salt)))) /
           4294967296.f;
  }
  void geometry() {
    periodFrames_ = rate_ * .001f *
                    rhythmMilliseconds(algorithm_, p_[0], a_[0] >= .5f, bpm_);
    beatPeriod_ = rhythmBeats(p_[0]);
    seamFrames_ = rate_ * .001f * (1 + 29 * a_[3]);
    recoveryFrames_ = rate_ * .05f * std::pow(80.f, p_[3]);
  }
  float frames() const { return periodFrames_; }
  float seam() const { return seamFrames_; }
  int slices() const {
    return 4 << std::clamp(int(std::lround(a_[1] * 2)), 0, 2);
  }
  float sample(const Snapshot &source, int channel, double position,
               int pitch) const {
    const auto tap = [&](int offset) {
      const auto bounded =
          std::size_t(std::clamp(offset, 0, int(source.span) - 1));
      std::size_t start = source.end + capacity_ - source.span;
      if (start >= capacity_)
        start -= capacity_;
      std::size_t at = start + bounded;
      if (at >= capacity_)
        at -= capacity_;
      return bank_[source.bank][channel * capacity_ + at];
    };
    const int index = int(position);
    const float fraction = float(position - index);
    if (pitch > 0) {
      const float phase = fraction * 64;
      const int row = std::min(63, int(phase));
      const float blend = phase - row;
      const auto &kernel = RhythmSinc::get().kernels[pitch];
      const int count = RhythmSinc::taps(pitch),
                first = index - (count / 2 - 1);
      std::size_t at = source.end + capacity_ - source.span;
      if (at >= capacity_)
        at -= capacity_;
      at += std::size_t(std::max(0, first));
      if (at >= capacity_)
        at -= capacity_;
      // Guarded captures normally provide a contiguous convolution. Avoid
      // bounds and wrap checks on every tap; retain the safe edge fallback.
      const float *data = first >= 0 && first + count <= int(source.span) &&
                                  at + count <= capacity_
                              ? bank_[source.bank] + channel * capacity_ + at
                              : nullptr;
      const auto weight = [&](int k) {
        return kernel[row][k] + (kernel[row + 1][k] - kernel[row][k]) * blend;
      };
      if (data) {
        float a = 0, b = 0, c = 0, d = 0;
        for (int k = 0; k < count; k += 4) {
          a += data[k] * weight(k);
          b += data[k + 1] * weight(k + 1);
          c += data[k + 2] * weight(k + 2);
          d += data[k + 3] * weight(k + 3);
        }
        return (a + b) + (c + d);
      }
      float out = 0;
      for (int k = 0; k < count; ++k)
        out += tap(first + k) * weight(k);
      return out;
    }
    const float x0 = tap(index - 1), x1 = tap(index), x2 = tap(index + 1),
                x3 = tap(index + 2);
    return x1 + .5f * fraction *
                    (x2 - x0 +
                     fraction * (2 * x0 - 5 * x1 + 4 * x2 - x3 +
                                 fraction * (3 * (x1 - x2) + x3 - x0)));
  }
  std::array<float, 2> read(Reader &r) const {
    if (!r.source.valid)
      return {};
    double pos = r.position;
    if (r.reverse) {
      pos = r.period - (r.oneShot ? 1 : 0) - pos;
      if (!r.oneShot && pos >= r.period)
        pos -= r.period;
    }
    std::array<float, 2> out{};
    const float blend = window(pos / r.seam);
    for (int ch = 0; ch < 2; ++ch) {
      const float head = sample(r.source, ch, r.start + pos, r.pitch);
      out[ch] = !r.oneShot && pos < r.seam
                    ? sample(r.source, ch, r.start + r.period + pos, r.pitch) *
                              (1 - blend) +
                          head * blend
                    : head;
    }
    for (auto &sample : out)
      sample *= r.level;
    r.position += r.speed;
    if (!r.oneShot && r.position >= r.period)
      r.position -= r.period * std::floor(r.position / r.period);
    if (!r.oneShot && r.position < 0)
      r.position += r.period * std::ceil(-r.position / r.period);
    return out;
  }
  Reader reader(const Snapshot &source, int slice = 0,
                bool reverse = false) const {
    Reader r;
    r.source = source;
    if (!source.valid)
      return r;
    const double margin = RhythmSinc::readMargin;
    // Shuffled slices are one-shot reads at original pitch. Keep real audio
    // on both sides for the outgoing overlap, including reversed slices.
    // Reserve the maximum transition tail even for tiny slices: a live
    // edit can lengthen the next fade while the old reader is still audible.
    const double guard = margin + std::ceil(double(rate_) * .02);
    if (algorithm_ == 15 && double(source.span) >= frames() + 2 * guard) {
      r.oneShot = true;
      r.period = double(frames()) / slices();
      r.start = double(source.span) - guard - frames() + slice * r.period;
      r.reverse = reverse;
      return r;
    }
    // A resized span may temporarily exceed the old capture. Loop that valid
    // material until the next complete capture is ready, without reading past
    // it.
    double available = double(source.span) - 2 * margin, desired = frames();
    if (algorithm_ == 15) {
      available = std::min(available, desired) / slices();
      desired = available;
    }
    r.seam = std::clamp(double(seam()), 1.0,
                        std::max(1.0, std::min(available * .2, desired * .2)));
    r.period = std::max(8.0, std::min(desired, available - r.seam));
    r.start =
        double(source.span) - margin -
        (algorithm_ == 15 ? double(slices() - slice) * available : available) +
        available - r.period - r.seam;
    if (algorithm_ == 13) {
      r.pitch = std::clamp(int(std::lround(p_[3] * 48)) - 24, -24, 24);
      r.speed = std::exp2(r.pitch / 12.f) * r.period / frames();
    }
    r.reverse = reverse;
    return r;
  }
  void transition(Reader next) {
    if (!next.source.valid || fade_ < 1)
      return;
    old_ = current_;
    current_ = next;
    captured_ = next.source;
    fade_ = 0;
    const double interval = algorithm_ == 15 ? frames() / slices() : frames();
    fadeStep_ =
        float(1 / std::max(1.0, std::min(double(rate_) * .02,
                                         std::min(std::max(double(rate_) * .004,
                                                           double(seam()) * 2),
                                                  interval * .45))));
    retireTo_ = next.source.bank == writer_
                    ? (old_.source.valid ? old_.source.bank : 1 - writer_)
                    : -1;
  }
  Snapshot snapshot() const {
    const double guard = RhythmSinc::readMargin;
    const auto required = std::size_t(
        std::ceil(algorithm_ == 15
                      ? frames() + 2 * (guard + std::ceil(double(rate_) * .02))
                      : frames() + seam() + 2 * guard));
    if (valid_ < std::min(required, capacity_ - 16))
      return {};
    if (needCapture_ && (!signalSeen_ || written_ - firstSignal_ < required ||
                         written_ - lastSignal_ >= required))
      return {};
    return {writer_, write_, std::min(valid_, std::size_t(rate_ * 6.05f)),
            true};
  }
  bool capture() {
    if (fade_ < 1)
      return false;
    const auto next = snapshot();
    if (!next.valid)
      return false;
    captured_ = next;
    ++captures_;
    needCapture_ = false;
    return true;
  }
  float shape(float x) const {
    const int mode = std::clamp(int(std::lround(a_[1] * 2)), 0, 2);
    return mode == 0 ? x : mode == 1 ? x * x * (3 - 2 * x) : x * x;
  }
  struct Phase {
    std::int64_t cycle;
    float fraction;
  };
  Phase phase() const {
    const bool sync = a_[0] >= .5f;
    double pos = sync ? beat_ / beatPeriod_ : seconds_ / (frames() / rate_);
    if (algorithm_ == 15)
      pos *= slices();
    const float swing =
        algorithm_ == 13 ? a_[1] * .6f : algorithm_ == 14 ? p_[3] * .6f : 0;
    const double pair = std::floor(pos * .5), within = pos - pair * 2,
                 first = 1 + swing;
    const bool second = within >= first;
    return {std::int64_t(pair) * 2 + (second ? 1 : 0),
            float(second ? (within - first) / (2 - first) : within / first)};
  }
  void boundary(std::int64_t cycle) {
    if (fade_ < 1)
      return;
    const bool changed = dirty_;
    p_ = pendingP_;
    a_ = pendingA_;
    dirty_ = false;
    if (changed)
      geometry();
    if (algorithm_ == 14)
      return;
    if (!engaged_ && !captured_.valid)
      return;
    if (algorithm_ == 16) {
      if (needCapture_ || (brakeDone_ && (a_[2] >= .5f || changed))) {
        if (capture()) {
          brakeElapsed_ = 0;
          brakeDone_ = false;
          transition(reader(captured_));
        }
      }
      return;
    }
    bool fresh = false;
    if (needCapture_ || (algorithm_ == 13 && p_[1] < .5f) ||
        (algorithm_ == 15 && cycle % slices() == 0))
      fresh = capture();
    if (algorithm_ == 13) {
      if (captured_.valid && (fresh || changed))
        transition(reader(captured_, 0, a_[2] >= .5f));
    } else {
      const auto event = cycle / (1 + int(std::lround(p_[2] * 15)));
      const int n = slices(), step = int(event % n),
                pattern = int(std::lround(p_[1] * 7));
      int selected = step;
      if (pattern == 1)
        selected = n - 1 - step;
      if (pattern == 2) {
        const int ping = int(event % (2 * n - 2));
        selected = ping < n ? ping : 2 * n - 2 - ping;
      }
      if (pattern == 3)
        selected = (step ^ 1) % n;
      if (pattern == 4)
        selected = (step * 3 + 1) % n;
      if (pattern == 5)
        selected = (step * 2 + int(event / n)) % n;
      if (pattern == 6)
        selected = std::min(n - 1, int(random(event, 83) * n));
      if (pattern == 7)
        selected = (step % 2) * (n / 2) + (step / 2) % (n / 2);
      previousSlice_ = selected;
      if (captured_.valid)
        transition(reader(captured_, selected, random(event, 233) < p_[3]));
    }
  }

  float gate(const Phase &point) const {
    const float duty = algorithm_ == 14 ? p_[1] : p_[2];
    if (duty < 1e-5f)
      return 0;
    if (algorithm_ == 14) {
      constexpr std::uint16_t masks[]{0xffff, 0x5555, 0x3333,
                                      0x6db6, 0x9249, 0x5b5b};
      const int pattern = std::clamp(int(std::lround(a_[1] * 7)), 0, 7);
      if (pattern < 6 && !(masks[pattern] & (1u << (point.cycle % 16))))
        return 0;
      if (pattern == 6 && random(point.cycle, 491) > .65f)
        return 0;
      if (pattern == 7 && point.cycle % 8 >= 3)
        return 0;
    }
    if (point.fraction >= duty)
      return 0;
    const float u = point.fraction / duty;
    if (duty >= .99999f &&
        (algorithm_ == 13 || (p_[2] <= 1e-7f && a_[1] <= 1e-7f)))
      return 1;
    const float swing = algorithm_ == 13 ? a_[1] * .6f : p_[3] * .6f;
    const double cycleFrames =
        frames() * (point.cycle % 2 ? 1 - swing : 1 + swing);
    const double edge =
        std::min(.4, double(rate_) * .008 / (cycleFrames * duty));
    const float taper = window(u / edge) * window((1 - u) / edge);
    if (algorithm_ == 13)
      return taper;
    const float shape = p_[2] * 3;
    const int index = std::min(2, int(shape));
    const float amount = shape - index;
    const float forms[]{
        1, 1 - u,
        std::max(0.f, s3g_daisy::ThruSpectralPlan::get().sinCycle(u * .5f)), u};
    return (forms[index] + (forms[index + 1] - forms[index]) * amount) * taper;
  }

public:
  bool prepare(float rate, float *borrowed, std::size_t capacity) {
    rate_ = rate;
    capacity_ = capacity;
    if (!borrowed) {
      capacity_ = std::size_t(std::ceil(rate * 8.1f)) + 4;
      owned_.reset(new (std::nothrow) float[capacity_ * 2]{});
      borrowed = owned_.get();
    }
    extra_.reset(new (std::nothrow) float[capacity_ * 2]{});
    bank_ = {borrowed, extra_.get()};
    (void)RhythmSinc::get();
    (void)s3g_daisy::ThruSpectralPlan::get();
    amplitudeSlew_ = 1 - std::exp(-1 / (rate * .003f));
    velocitySlew_ = 1 - std::exp(-1 / (rate * .006f));
    stopSlew_ = 1 - std::exp(-1 / (rate * .008f));
    panSlew_ = 1 - std::exp(-1 / (rate * .012f));
    brakeSlew_ = 1 - std::exp(-1 / (rate * .012f));
    reset();
    return bank_[0] && bank_[1];
  }
  void reset() {
    write_ = valid_ = 0;
    writer_ = 0;
    retireTo_ = -1;
    captured_ = {};
    current_ = {};
    old_ = {};
    fade_ = 1;
    cycle_ = -1;
    ready_ = false;
    dirty_ = false;
    needCapture_ = true;
    engaged_ = wasEngaged_ = wasPlaying_ = false;
    lastPosition_ = -1;
    beat_ = origin_ = seconds_ = 0;
    previousSlice_ = 0;
    amplitude_ = 1;
    brakeMix_ = pan_ = 0;
    brakeElapsed_ = 0;
    brakeDone_ = false;
    captures_ = 0;
    written_ = firstSignal_ = lastSignal_ = 0;
    signalSeen_ = false;
  }
  void context(const RhythmClock &clock, bool engaged) {
    context_ = clock;
    engaged_ = engaged;
  }
  void beginBlock(int algorithm, const std::array<float, 4> &p,
                  const std::array<float, 4> &a) {
    algorithm_ = algorithm;
    for (int i = 0; i < 4; ++i) {
      pendingP_[i] = unit(p[i]);
      pendingA_[i] = unit(a[i]);
    }
    dirty_ = dirty_ || pendingP_ != p_ || pendingA_ != a_;
    const float bpm = std::isfinite(context_.bpm)
                          ? std::clamp(context_.bpm, 40.f, 240.f)
                          : 120;
    const bool tempo = bpm != bpm_;
    bpm_ = bpm;
    if (!ready_) {
      p_ = pendingP_;
      a_ = pendingA_;
      ready_ = true;
      dirty_ = false;
      geometry();
    } else if (tempo) {
      geometry();
      dirty_ = true;
    }
    const bool restart =
        context_.playing && (!wasPlaying_ || launch_ != context_.launch ||
                             context_.position + 1e-5 < lastPosition_);
    if (restart) {
      origin_ = context_.beat;
      beat_ = seconds_ = 0;
      cycle_ = -1;
      needCapture_ = true;
      brakeDone_ = false;
      previousSlice_ = 0;
    }
    if (context_.playing)
      beat_ = std::max(0.0, context_.beat - origin_);
    else if (context_.shared)
      beat_ = std::max(0.0, context_.beat);
    launch_ = context_.launch;
    lastPosition_ = context_.position;
    wasPlaying_ = context_.playing;
    if (engaged_ && !wasEngaged_) {
      needCapture_ = true;
      brakeDone_ = false;
    }
    wasEngaged_ = engaged_;
  }
  std::uint64_t captures() const { return captures_; }
  bool captured() const { return captured_.valid; }
  int selectedSlice() const { return previousSlice_; }
  double repeatSamples() const {
    return current_.source.valid
               ? current_.period / std::max(.0001f, std::abs(current_.speed))
               : 0;
  }
  std::array<float, 2> process(std::array<float, 2> in) {
    ++written_;
    if (std::max(std::abs(in[0]), std::abs(in[1])) > 1e-6f) {
      if (!signalSeen_) {
        firstSignal_ = written_;
        signalSeen_ = true;
      }
      lastSignal_ = written_;
    }
    bank_[writer_][write_] = in[0];
    bank_[writer_][capacity_ + write_] = in[1];
    if (++write_ == capacity_)
      write_ = 0;
    valid_ = std::min(valid_ + 1, capacity_);
    auto point = phase();
    if (point.cycle != cycle_) {
      cycle_ = point.cycle;
      boundary(cycle_);
      point = phase();
      cycle_ = point.cycle;
    }
    if (needCapture_ && engaged_ && algorithm_ != 14 && a_[0] < .5f &&
        fade_ >= 1)
      boundary(cycle_);
    if (algorithm_ == 16 && brakeDone_ && dirty_ && fade_ >= 1)
      boundary(cycle_);
    std::array<float, 2> out = in;
    if (algorithm_ != 14 && current_.source.valid) {
      if (algorithm_ == 16) {
        const float stop = frames(), recovery = recoveryFrames_;
        const float target = 1 - p_[1] * (1 + p_[2]);
        const float velocity =
            brakeElapsed_ < stop
                ? 1 + (target - 1) *
                          shape(std::clamp(brakeElapsed_ / stop, 0.f, 1.f))
                : target + (1 - target) * shape(std::clamp(
                                              (brakeElapsed_ - stop) / recovery,
                                              0.f, 1.f));
        current_.speed += (velocity - current_.speed) * velocitySlew_;
        if (std::abs(velocity - current_.speed) < 1e-8f)
          current_.speed = velocity;
        const float gain = window(std::abs(current_.speed) / .1);
        current_.level += (gain - current_.level) * stopSlew_;
        if (std::abs(gain - current_.level) < 1e-8f)
          current_.level = gain;
        brakeDone_ = brakeElapsed_ >= stop + recovery;
        brakeElapsed_ += 1;
      }
      const auto fresh = read(current_);
      const auto prior = old_.source.valid && fade_ < 1 ? read(old_) : in;
      const float blend = window(fade_);
      for (int c = 0; c < 2; ++c)
        out[c] = prior[c] + (fresh[c] - prior[c]) * blend;
      if (fade_ < 1) {
        fade_ = std::min(1.f, fade_ + fadeStep_);
        if (fade_ == 1 && retireTo_ >= 0) {
          writer_ = retireTo_;
          write_ = valid_ = 0;
          written_ = firstSignal_ = lastSignal_ = 0;
          signalSeen_ = false;
          retireTo_ = -1;
        }
      }
      if (algorithm_ == 16) {
        const float wanted = brakeDone_ ? 0.f : p_[1];
        brakeMix_ += (wanted - brakeMix_) * brakeSlew_;
        if (std::abs(wanted - brakeMix_) < 1e-8f)
          brakeMix_ = wanted;
        for (int c = 0; c < 2; ++c)
          out[c] = in[c] + (out[c] - in[c]) * brakeMix_;
      }
    }
    if (algorithm_ == 13 || algorithm_ == 14) {
      const float wanted = gate(point);
      amplitude_ += (wanted - amplitude_) * amplitudeSlew_;
      if (std::abs(amplitude_ - wanted) < 1e-8f)
        amplitude_ = wanted;
      const float pan = algorithm_ == 14 ? a_[2] * 2 - 1 : 0;
      pan_ += (pan - pan_) * panSlew_;
      if (std::abs(pan - pan_) < 1e-8f)
        pan_ = pan;
      out[0] *= amplitude_ * (1 - std::max(0.f, pan_));
      out[1] *= amplitude_ * (1 + std::min(0.f, pan_));
    }
    beat_ += bpm_ / (60.0 * rate_);
    seconds_ += 1.0 / rate_;
    return out;
  }
};
} // namespace s3g_thru_layers
