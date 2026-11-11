#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace s3g_thru_layers {
// Small 2x half-band FIR. Blackman-windowed sinc; each pair has eight
// base-rate frames of group delay. No allocation or control-rate design.
class Oversample2 {
  struct Filter {
    std::array<float, 34> data{};
    std::size_t write = 0;
    float process(float x, bool calculate = true) {
      static const auto taps = []() {
        std::array<float, 17> a{};
        float sum = 0;
        constexpr float pi = 3.14159265359f;
        for (int k = 0; k < 17; ++k) {
          const int n = k - 8;
          const float h = n == 0 ? .5f : std::sin(pi * .5f * n) / (pi * n);
          const float w = .42f - .5f * std::cos(2 * pi * k / 16) +
                          .08f * std::cos(4 * pi * k / 16);
          a[k] = (n != 0 && n % 2 == 0) ? 0 : h * w;
          sum += a[k];
        }
        for (auto &v : a)
          v /= sum;
        return a;
      }();
      data[write] = data[write + 17] = x;
      float y = 0;
      if (calculate) {
        const auto *last = data.data() + write + 17;
        y = last[-8] * taps[8];
        for (int k = 1; k < 17; k += 2)
          y += taps[k] * last[-k];
      }
      if (++write == 17)
        write = 0;
      return y;
    }
  };
  std::array<Filter, 2> up_{}, down_{};

public:
  static constexpr unsigned latency = 8;
  void reset() {
    up_ = {};
    down_ = {};
  }
  template <class Function>
  std::array<float, 2> process(std::array<float, 2> in, Function &&fn) {
    std::array<float, 2> out{};
    for (int phase = 0; phase < 2; ++phase) {
      std::array<float, 2> hi{};
      for (int c = 0; c < 2; ++c)
        hi[c] = up_[c].process(phase ? 0 : in[c] * 2);
      hi = fn(hi);
      for (int c = 0; c < 2; ++c) {
        const float v = down_[c].process(hi[c], phase == 0);
        if (phase == 0)
          out[c] = v;
      }
    }
    return out;
  }
};
class BusConditioner {
  static void settle(float &state, float target, float coefficient) {
    state += (target - state) * coefficient;
    if (std::abs(target - state) < 1e-8f)
      state = target;
  }

  float rate_ = 44100, head_ = 1, headTarget_ = 1, high_ = 0, highTarget_ = 0,
        low_ = 1, lowTarget_ = 1, dynamics_ = 0, dynamicsTarget_ = 0;
  float attack_ = 0, release_ = 0, slew_ = 0, envelope_ = 0, gain_ = 1,
        outputGain_ = 1;
  bool ready_ = false;
  std::array<float, 2> dc_{}, postOne_{}, postTwo_{};

public:
  void prepare(float rate) {
    rate_ = rate;
    attack_ = 1 - std::exp(-1 / (rate * .0015f));
    release_ = 1 - std::exp(-1 / (rate * .16f));
    slew_ = 1 - std::exp(-1 / (rate * .012f));
    reset();
  }
  void reset() {
    dc_ = {};
    postOne_ = {};
    postTwo_ = {};
    envelope_ = 0;
    gain_ = 1;
    outputGain_ = 1;
    ready_ = false;
  }
  void beginBlock(const std::array<float, 4> &a) {
    headTarget_ = std::pow(10.f, -18 * a[0] / 20);
    highTarget_ =
        1 - std::exp(-6.28318530718f * 20 * std::pow(30.f, a[1]) / rate_);
    lowTarget_ =
        1 -
        std::exp(-6.28318530718f *
                 std::min(rate_ * .42f, 1200 * std::pow(15.f, a[2])) / rate_);
    dynamicsTarget_ = a[3];
    if (!ready_) {
      head_ = headTarget_;
      high_ = highTarget_;
      low_ = lowTarget_;
      dynamics_ = dynamicsTarget_;
      ready_ = true;
    }
  }
  std::array<float, 2> input(std::array<float, 2> in) {
    settle(head_, headTarget_, slew_);
    settle(high_, highTarget_, slew_);
    settle(low_, lowTarget_, slew_);
    settle(dynamics_, dynamicsTarget_, slew_);
    const float peak = std::max(std::abs(in[0]), std::abs(in[1])) * head_;
    envelope_ += (peak - envelope_) * (peak > envelope_ ? attack_ : release_);
    if (envelope_ < 1e-20f)
      envelope_ = 0;
    const float reduction = envelope_ > .18f ? std::sqrt(.18f / envelope_) : 1;
    const float target = 1 + (reduction - 1) * dynamics_;
    settle(gain_, target, target < gain_ ? attack_ : release_);
    for (int c = 0; c < 2; ++c) {
      in[c] *= head_ * gain_;
      dc_[c] += high_ * (in[c] - dc_[c]);
      if (std::abs(dc_[c]) < 1e-20f)
        dc_[c] = 0;
      in[c] -= dc_[c];
    }
    return in;
  }
  std::array<float, 2> output(std::array<float, 2> out) {
    const float peak = std::max(std::abs(out[0]), std::abs(out[1]));
    const float target = peak > .55f ? std::sqrt(.55f / peak) : 1;
    settle(outputGain_, target, target < outputGain_ ? attack_ : release_);
    for (int c = 0; c < 2; ++c) {
      postOne_[c] += low_ * (out[c] - postOne_[c]);
      postTwo_[c] += low_ * (postOne_[c] - postTwo_[c]);
      if (std::abs(postOne_[c]) < 1e-20f)
        postOne_[c] = 0;
      if (std::abs(postTwo_[c]) < 1e-20f)
        postTwo_[c] = 0;
      out[c] = postTwo_[c] * (1 + (outputGain_ - 1) * dynamics_);
    }
    return out;
  }
};
} // namespace s3g_thru_layers
