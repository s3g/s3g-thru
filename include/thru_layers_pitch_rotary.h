#pragma once
#include "thru_part_effects.h"
#include "thru_spectral.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <new>

namespace s3g_thru_layers {
namespace modulation_detail {
inline float quiet(float x) { return std::abs(x) < 1e-20f ? 0.f : x; }
inline float unit(float x) {
  return std::isfinite(x) ? std::clamp(x, 0.f, 1.f) : 0.f;
}
inline void slew(float &x, float target, float coefficient) {
  x = quiet(x + (target - x) * coefficient);
  if (std::abs(x - target) < 1e-8f)
    x = target;
}
inline void advance(float &phase, float increment) {
  phase += increment;
  if (phase < 0)
    phase += 1;
  else if (phase >= 1)
    phase -= 1;
}
} // namespace modulation_detail

// Stereo adaptation of s3g-dsp MacroPitch's two overlapping sample readers.
// Signed phase movement preserves reader positions through pitch direction
// changes. A smooth unison blend replaces the original hard bypass threshold.
class StereoPitch {
  struct Channel {
    float phase = .25f, ratio = 1, targetRatio = 1, mix = 0, targetMix = 0;
    std::array<float, 4> filter{};
  };
  std::array<Channel, 2> channels_{};
  std::unique_ptr<float[]> history_;
  std::size_t capacity_ = 0, mask_ = 0, write_ = 0, valid_ = 0;
  float rate_ = 44100, slew_ = 0, windowSlew_ = 0, mixSlew_ = 0;
  float window_ = 1, targetWindow_ = 1, g_ = 1, targetG_ = 1;
  bool ready_ = false;

  float read(int channel, float lag) const {
    float at = float(write_) - lag;
    if (at < 0)
      at += float(capacity_);
    const auto raw = std::size_t(at);
    const auto i = raw & mask_;
    const float f = at - float(raw);
    const auto sample = [&](std::size_t index) {
      const auto age = (write_ + capacity_ - index) & mask_;
      return age < valid_ ? history_[channel * capacity_ + index] : 0.f;
    };
    const float a = sample((i + capacity_ - 1) & mask_);
    const float b = sample(i), c = sample((i + 1) & mask_);
    const float d = sample((i + 2) & mask_);
    return b +
           .5f * f *
               (c - a +
                f * (2 * a - 5 * b + 4 * c - d + f * (3 * (b - c) + d - a)));
  }
  float lowPass(Channel &channel, float input) {
    // Fourth-order Butterworth source filtering before upward transposition.
    // Both channels share the cutoff so detuning does not alter stereo tone.
    constexpr float inverseQ[2]{1.847759065f, .765366865f};
    for (int stage = 0; stage < 2; ++stage) {
      auto &s1 = channel.filter[stage * 2];
      auto &s2 = channel.filter[stage * 2 + 1];
      const float a = 1 / (1 + g_ * (g_ + inverseQ[stage]));
      const float v1 = a * (s1 + g_ * (input - s2));
      const float v2 = s2 + g_ * v1;
      s1 = modulation_detail::quiet(2 * v1 - s1);
      s2 = modulation_detail::quiet(2 * v2 - s2);
      input = v2;
    }
    return input;
  }

public:
  bool prepare(float rate) {
    rate_ = rate;
    capacity_ = 1;
    while (capacity_ < std::size_t(std::ceil(rate * .185f)) + 8)
      capacity_ *= 2;
    mask_ = capacity_ - 1;
    history_.reset(new (std::nothrow) float[capacity_ * 2]{});
    slew_ = 1 - std::exp(-1 / (rate * .03f));
    windowSlew_ = 1 - std::exp(-1 / (rate * .08f));
    mixSlew_ = 1 - std::exp(-1 / (rate * .012f));
    reset();
    return bool(history_);
  }
  void reset() {
    channels_ = {};
    write_ = valid_ = 0;
    ready_ = false;
  }
  void beginBlock(const std::array<float, 4> &p) {
    const float pitch = std::round(modulation_detail::unit(p[0]) * 48) - 24;
    const float fine = modulation_detail::unit(p[1]) * 2 - 1;
    const float spread = modulation_detail::unit(p[3]) * .5f;
    targetWindow_ = rate_ * .02f * std::pow(9.f, modulation_detail::unit(p[2]));
    float fastest = 1;
    for (int c = 0; c < 2; ++c) {
      const float semitones = pitch + fine + (c ? spread : -spread);
      auto &channel = channels_[c];
      channel.targetRatio = std::exp2(semitones / 12);
      channel.targetMix = std::clamp(std::abs(semitones) * 50, 0.f, 1.f);
      fastest = std::max(fastest, channel.targetRatio);
    }
    targetG_ = std::tan(3.14159265359f * .45f / fastest);
    if (!ready_) {
      window_ = targetWindow_;
      g_ = targetG_;
      for (auto &channel : channels_) {
        channel.ratio = channel.targetRatio;
        channel.mix = channel.targetMix;
      }
      ready_ = true;
    }
  }
  std::array<float, 2> process(std::array<float, 2> in) {
    const auto &plan = s3g_daisy::ThruSpectralPlan::get();
    modulation_detail::slew(window_, targetWindow_, windowSlew_);
    modulation_detail::slew(g_, targetG_, slew_);
    valid_ = std::min(capacity_, valid_ + 1);
    std::array<float, 2> out{};
    for (int c = 0; c < 2; ++c) {
      auto &channel = channels_[c];
      modulation_detail::slew(channel.ratio, channel.targetRatio, slew_);
      modulation_detail::slew(channel.mix, channel.targetMix, mixSlew_);
      history_[c * capacity_ + write_] = lowPass(channel, in[c]);
      modulation_detail::advance(channel.phase, (1 - channel.ratio) / window_);
      float second = channel.phase + .5f;
      if (second >= 1)
        second -= 1;
      const float weight = .5f - .5f * plan.sinCycle(channel.phase + .25f);
      const float shifted = read(c, 3 + window_ * channel.phase) * weight +
                            read(c, 3 + window_ * second) * (1 - weight);
      out[c] = in[c] + (shifted - in[c]) * channel.mix;
    }
    write_ = (write_ + 1) & mask_;
    return out;
  }
};

// The same horn/drum and Doppler model as Thru's existing rotary, isolated
// from its tone/drive/lo-fi inserts so every XY corner owns a clean history.
class RotaryLayer {
  std::unique_ptr<float[]> history_;
  std::size_t capacity_ = 0, write_ = 0, valid_ = 0;
  float rate_ = 44100, slew_ = 0, ramp_ = 0;
  float speed_ = 0, hornRate_ = 0, drumRate_ = 0;
  float horn_ = 0, drum_ = .17f;
  float depth_ = 0, spread_ = 1, cross_ = .1f, balance_ = 0;
  float doppler_ = 1, tone_ = 1, low_ = 0;
  float targetDepth_ = 0, targetSpread_ = 1, targetCross_ = .1f;
  float targetBalance_ = 0, targetDoppler_ = 1, targetTone_ = 1;
  std::array<float, 2> toneHistory_{};
  bool ready_ = false;

  float read(int band, float lag) const {
    if (valid_ < 2 || lag > float(valid_ - 1))
      return 0;
    return s3g_daisy::readRotaryDelaySample(history_.get() + band * capacity_,
                                            capacity_, write_, lag);
  }

public:
  bool prepare(float rate) {
    rate_ = rate;
    capacity_ = std::size_t(std::ceil(rate * .026f)) + 4;
    history_.reset(new (std::nothrow) float[capacity_ * 3]{});
    slew_ = 1 - std::exp(-1 / (rate * .012f));
    reset();
    return bool(history_);
  }
  void reset() {
    write_ = valid_ = 0;
    horn_ = 0;
    drum_ = .17f;
    low_ = 0;
    toneHistory_ = {};
    ready_ = false;
  }
  void beginBlock(const std::array<float, 4> &p,
                  const std::array<float, 4> &a) {
    speed_ = 8 * std::pow(modulation_detail::unit(p[0]), 2);
    const float rampSeconds =
        .05f * std::pow(160.f, modulation_detail::unit(p[2]));
    ramp_ = 1 - std::exp(-1 / (rate_ * rampSeconds));
    targetDepth_ = modulation_detail::unit(p[1]);
    targetSpread_ = modulation_detail::unit(p[3]) * 2;
    targetCross_ =
        1 -
        std::exp(-6.28318530718f * 120 *
                 std::pow(5000.f / 120, modulation_detail::unit(a[0])) / rate_);
    targetBalance_ = modulation_detail::unit(a[1]) * 2 - 1;
    targetDoppler_ = modulation_detail::unit(a[2]);
    targetTone_ =
        1 - std::exp(-6.28318530718f * 500 *
                     std::pow(36.f, modulation_detail::unit(a[3])) / rate_);
    if (!ready_) {
      hornRate_ = speed_;
      drumRate_ = speed_ * .82f;
      depth_ = targetDepth_;
      spread_ = targetSpread_;
      cross_ = targetCross_;
      balance_ = targetBalance_;
      doppler_ = targetDoppler_;
      tone_ = targetTone_;
      ready_ = true;
    }
  }
  std::array<float, 2> process(std::array<float, 2> in) {
    using modulation_detail::slew;
    const auto &plan = s3g_daisy::ThruSpectralPlan::get();
    slew(hornRate_, speed_, ramp_);
    slew(drumRate_, speed_ * .82f, ramp_ * .625f);
    slew(depth_, targetDepth_, slew_);
    slew(spread_, targetSpread_, slew_);
    slew(cross_, targetCross_, slew_);
    slew(balance_, targetBalance_, slew_);
    slew(doppler_, targetDoppler_, slew_);
    slew(tone_, targetTone_, slew_);
    const float middle = (in[0] + in[1]) * .5f;
    low_ = modulation_detail::quiet(low_ + cross_ * (middle - low_));
    history_[write_] = low_;
    history_[capacity_ + write_] = middle - low_;
    history_[capacity_ * 2 + write_] = (in[0] - in[1]) * .5f;
    valid_ = std::min(capacity_, valid_ + 1);
    const float hs = plan.sinCycle(horn_), ds = plan.sinCycle(drum_);
    const float hc = plan.sinCycle(horn_ + .25f),
                dc = plan.sinCycle(drum_ + .25f);
    const float hm = rate_ * .00085f * doppler_ * hc;
    const float dm = rate_ * .00038f * doppler_ * dc;
    const float hg = 1 + std::min(0.f, balance_);
    const float dg = 1 - std::max(0.f, balance_);
    const float side = read(2, rate_ * .0039f) * (.55f + .15f * hc);
    const float left = hg * read(1, rate_ * .0032f + hm) * (1 - .52f * hs) +
                       dg * read(0, rate_ * .0046f + dm) * (1 - .34f * ds) +
                       side;
    const float right = hg * read(1, rate_ * .0032f - hm) * (1 + .52f * hs) +
                        dg * read(0, rate_ * .0046f - dm) * (1 + .34f * ds) -
                        side;
    const float mid = (left + right) * .5f,
                stereo = (left - right) * .5f * spread_;
    std::array<float, 2> out{mid + stereo, mid - stereo};
    for (int c = 0; c < 2; ++c) {
      toneHistory_[c] = modulation_detail::quiet(
          toneHistory_[c] + tone_ * (out[c] - toneHistory_[c]));
      out[c] = in[c] + (toneHistory_[c] - in[c]) * depth_;
    }
    write_ = (write_ + 1) % capacity_;
    modulation_detail::advance(horn_, hornRate_ / rate_);
    modulation_detail::advance(drum_, drumRate_ / rate_);
    return out;
  }
};
} // namespace s3g_thru_layers
