#pragma once
#include "thru_sequence.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Four-lane channel strip adapted from the s3g Opps final mix design
// (BSD-3-Clause, Shawn Greenlee). The filter and balance stages are bounded,
// stereo preserving, and prepared without audio-thread allocations.
namespace s3g_thru_sequence {
class StripProcessor {
  struct Channel {
    float low = 0, highLow = 0, midLow = 0, midHigh = 0;
  };
  struct State {
    std::array<Channel, 2> channels{};
    float pan = 0, lowGain = 1, midGain = 1, highGain = 1, audible = 1;
  };
  std::array<State, lanes> states_{};
  float rate_ = 0, lowCoefficient_ = 0, highCoefficient_ = 0, smooth_ = 0;
  bool active_ = false;

 public:
  template <std::size_t Block>
  void process(std::size_t frames,
               std::array<std::array<std::array<float, Block>, 2>, lanes> &bus,
               const Mix &mix, float sampleRate) {
    if (!active_) {
      for (const auto &strip : mix.strips)
        active_ |= strip.pan != 0 || strip.lowDb != 0 || strip.midDb != 0 ||
                   strip.highDb != 0 || strip.solo;
      if (!active_)
        return;
    }
    if (rate_ != sampleRate) {
      rate_ = sampleRate;
      lowCoefficient_ = 1 - std::exp(-6.28318530718f * 170 / rate_);
      highCoefficient_ = 1 - std::exp(-6.28318530718f * 4200 / rate_);
      smooth_ = 1 - std::exp(-1 / (rate_ * .015f));
      states_ = {};
      for (auto &state : states_)
        state.lowGain = state.midGain = state.highGain = state.audible = 1;
    }
    bool anySolo = false;
    for (const auto &strip : mix.strips)
      anySolo |= strip.solo;
    for (std::size_t lane = 0; lane < lanes; ++lane) {
      const auto &strip = mix.strips[lane];
      auto &state = states_[lane];
      const float targetLow = std::pow(10.f, strip.lowDb / 20.f);
      const float targetMid = std::pow(10.f, strip.midDb / 20.f);
      const float targetHigh = std::pow(10.f, strip.highDb / 20.f);
      const float targetAudible = !anySolo || strip.solo ? 1.f : 0.f;
      const float frequency = std::clamp(strip.midHz, 120.f, 8000.f);
      const float midLowCoefficient =
          1 - std::exp(-6.28318530718f * frequency * .70710678f / rate_);
      const float midHighCoefficient =
          1 - std::exp(-6.28318530718f * std::min(frequency * 1.41421356f,
                                                  rate_ * .45f) / rate_);
      for (std::size_t frame = 0; frame < frames; ++frame) {
        state.pan += (strip.pan - state.pan) * smooth_;
        state.lowGain += (targetLow - state.lowGain) * smooth_;
        state.midGain += (targetMid - state.midGain) * smooth_;
        state.highGain += (targetHigh - state.highGain) * smooth_;
        state.audible += (targetAudible - state.audible) * smooth_;
        for (std::size_t side = 0; side < 2; ++side) {
          auto &filter = state.channels[side];
          const float clean = bus[lane][side][frame];
          filter.low += (clean - filter.low) * lowCoefficient_;
          filter.highLow += (clean - filter.highLow) * highCoefficient_;
          filter.midLow += (clean - filter.midLow) * midLowCoefficient;
          filter.midHigh += (clean - filter.midHigh) * midHighCoefficient;
          const float low = filter.low, mid = filter.midHigh - filter.midLow;
          const float high = clean - filter.highLow;
          const float panGain = side == 0
              ? std::max(0.f, 1.f - std::max(0.f, state.pan))
              : std::max(0.f, 1.f + std::min(0.f, state.pan));
          const float result = (clean + (state.lowGain - 1) * low
              + (state.midGain - 1) * mid + (state.highGain - 1) * high)
              * panGain * state.audible;
          bus[lane][side][frame] = std::isfinite(result) ? result : 0;
        }
      }
    }
  }
};
} // namespace s3g_thru_sequence
