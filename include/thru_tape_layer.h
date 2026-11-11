#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

namespace s3g_thru_layers {
// Tape coloration/transport equations from Thru's existing tapeBlock.
// The corner exposes format, record level, age and mechanics. Hiss,
// crinkle and transport-stop are disabled; the dry reference remains 2 frames.
class TapeLayer {
  static void settle(float &state, float target, float coefficient) {
    state += (target - state) * coefficient;
    if (std::abs(target - state) < 1e-8f)
      state = target;
  }
  std::unique_ptr<float[]> history_;
  std::size_t size_ = 0, write_ = 0, valid_ = 0, countdown_ = 0;
  float rate_ = 44100, slew_ = 0, formatSlew_ = 0, format_ = 0, drive_ = .1f,
        age_ = 0, mech_ = 0;
  float bandwidth_ = 1, width_ = 1, headAmount_ = .015f, saturation_ = .015f;
  float bandwidthTarget_ = 1, widthTarget_ = 1, headTarget_ = .015f,
        saturationTarget_ = .015f;
  float wowSin_ = 0, wowCos_ = 1, flutterSin_ = 0, flutterCos_ = 1, drift_ = 0,
        driftTarget_ = 0;
  float wowS_ = 0, wowC_ = 1, flutterS_ = 0, flutterC_ = 1, headAlpha_ = 0,
        driftAlpha_ = 0;
  std::uint32_t noise_ = 0x574f5746u;
  std::array<float, 2> low_{}, head_{};
  std::array<float, 4> target_{0, .1f, 0, 0}, detail_{.5f, .5f, .5f, .5f},
      detailTarget_{.5f, .5f, .5f, .5f};
  static float random(std::uint32_t &state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return float(state & 0xffffffu) * (2.f / 16777215.f) - 1;
  }
  float tap(int channel, std::size_t age) const {
    if (age > valid_)
      return 0;
    return history_[channel * size_ + (write_ + size_ - age) % size_];
  }
  float read(int channel, float lag) const {
    lag = std::clamp(lag, 2.f, float(size_ - 3));
    const auto n = std::size_t(std::ceil(lag));
    const float f = float(n) - lag;
    const float z = tap(channel, n + 1), a = tap(channel, n),
                b = tap(channel, n - 1), c = tap(channel, n - 2);
    return ((.5f * (c - z) + 1.5f * (a - b)) * f +
            (z - 2.5f * a + 2 * b - .5f * c)) *
               f * f +
           .5f * (b - z) * f + a;
  }
  void coefficients() {
    constexpr float cut[]{21000, 19000, 16500, 13000, 9000},
        aged[]{8500, 7600, 6500, 5200, 3800};
    constexpr float widths[]{1, .985f, .955f, .90f, .80f},
        heads[]{.015f, .020f, .030f, .045f, .065f};
    constexpr float saturation[]{.015f, .025f, .040f, .065f, .10f};
    const float fmt = std::clamp(format_, 0.f, 4.f);
    const int lo = int(fmt), hi = std::min(4, lo + 1);
    const auto value = [&](const float *a) {
      return a[lo] + (a[hi] - a[lo]) * (fmt - lo);
    };
    const float ageCurve = target_[2] * std::sqrt(target_[2]),
                cutoff = value(cut) *
                         std::pow(value(aged) / value(cut), ageCurve) *
                         std::pow(2.f, (detailTarget_[2] - .5f) * 2);
    bandwidthTarget_ =
        1 - std::exp(-6.28318530718f * std::min(cutoff, rate_ * .45f) / rate_);
    widthTarget_ = value(widths) * (1 - .18f * ageCurve);
    headTarget_ = value(heads) + .08f * ageCurve;
    saturationTarget_ = value(saturation) + .22f * ageCurve;
    const float wow =
        6.28318530718f * (.14f + .26f * target_[3] + .025f * fmt) / rate_;
    const float flutter =
        6.28318530718f * (5.5f + 3 * target_[3] + .75f * fmt) / rate_;
    wowS_ = std::sin(wow);
    wowC_ = std::cos(wow);
    flutterS_ = std::sin(flutter);
    flutterC_ = std::cos(flutter);
  }

public:
  bool prepare(float rate) {
    rate_ = rate;
    size_ = std::size_t(std::ceil(rate * .05f)) + 8;
    history_.reset(new (std::nothrow) float[size_ * 2]{});
    slew_ = 1 - std::exp(-1 / (rate * .005f));
    formatSlew_ = 1 - std::exp(-1 / (rate * .020f));
    headAlpha_ = 1 - std::exp(-6.28318530718f * 85 / rate);
    driftAlpha_ = 1 - std::exp(-1 / (rate * .22f));
    reset();
    return bool(history_);
  }
  void reset() {
    write_ = valid_ = countdown_ = 0;
    low_ = {};
    head_ = {};
    wowSin_ = flutterSin_ = drift_ = driftTarget_ = 0;
    wowCos_ = flutterCos_ = 1;
  }
  void beginBlock(const std::array<float, 4> &p,
                  const std::array<float, 4> &a) {
    detailTarget_ = a;
    target_ = p;
    coefficients();
    const float wn = std::sqrt(wowSin_ * wowSin_ + wowCos_ * wowCos_),
                fn = std::sqrt(flutterSin_ * flutterSin_ +
                               flutterCos_ * flutterCos_);
    if (wn > 0) {
      wowSin_ /= wn;
      wowCos_ /= wn;
    }
    if (fn > 0) {
      flutterSin_ /= fn;
      flutterCos_ /= fn;
    }
  }
  std::array<float, 2> process(std::array<float, 2> in) {
    for (int i = 0; i < 4; ++i)
      settle(detail_[i], detailTarget_[i], slew_);
    settle(format_, target_[0] * 4, formatSlew_);
    settle(drive_, target_[1], slew_);
    settle(age_, target_[2], slew_);
    settle(mech_, target_[3], slew_);
    settle(bandwidth_, bandwidthTarget_, slew_);
    settle(width_, widthTarget_, slew_);
    settle(headAmount_, headTarget_, slew_);
    settle(saturation_, saturationTarget_, slew_);
    for (int c = 0; c < 2; ++c)
      history_[c * size_ + write_] = in[c];
    if (!countdown_) {
      driftTarget_ = random(noise_);
      countdown_ = std::max<std::size_t>(
          1, std::size_t(rate_ * (.20f + .25f * (random(noise_) + 1))));
    }
    --countdown_;
    drift_ += (driftTarget_ - drift_) * driftAlpha_;
    const float mechanics = mech_ * mech_ * (.22f + .78f * mech_) *
                            (1 + .12f * format_) * (1 + .35f * age_);
    const float lag =
        2 + rate_ * .007f * mechanics +
        rate_ * .0028f * mechanics * (.68f * wowSin_ + .32f * drift_) *
            detail_[0] * 2 +
        rate_ * .00022f * mechanics *
            (.76f * flutterSin_ + .48f * flutterSin_ * flutterCos_) *
            detail_[1] * 2;
    const float driveGain = 1 + .9952623f * drive_ * drive_,
                sat = saturation_ + .34f * drive_ * drive_;
    const float stability =
        1 - .018f * age_ * (.35f + .65f * age_) * (.5f + .5f * drift_);
    std::array<float, 2> out{};
    for (int c = 0; c < 2; ++c) {
      const float x = read(c, lag);
      low_[c] += bandwidth_ * (x - low_[c]);
      head_[c] += headAlpha_ * (low_[c] - head_[c]);
      if (std::abs(low_[c]) < 1e-20f)
        low_[c] = 0;
      if (std::abs(head_[c]) < 1e-20f)
        head_[c] = 0;
      const float value = (low_[c] + headAmount_ * head_[c]) * driveGain;
      out[c] =
          value / (1 + sat * std::abs(value)) * (1 + .18f * sat) * stability;
    }
    const float mid = (out[0] + out[1]) * .5f,
                side = (out[0] - out[1]) * .5f * width_ * detail_[3] * 2;
    const float wc = wowCos_ * wowC_ - wowSin_ * wowS_,
                fc = flutterCos_ * flutterC_ - flutterSin_ * flutterS_;
    wowSin_ = wowSin_ * wowC_ + wowCos_ * wowS_;
    wowCos_ = wc;
    flutterSin_ = flutterSin_ * flutterC_ + flutterCos_ * flutterS_;
    flutterCos_ = fc;
    write_ = (write_ + 1) % size_;
    valid_ = std::min(size_ - 1, valid_ + 1);
    return {mid + side, mid - side};
  }
};
} // namespace s3g_thru_layers
