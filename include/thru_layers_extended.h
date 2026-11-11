#pragma once
#include "s3g_fx/s3g_buffer_processor.h"
#include "s3g_fx/s3g_macro_fracture.h"
#include "s3g_fx/s3g_macro_shred.h"
#include "thru_layers_dynamics.h"
#include "thru_layers_pitch_rotary.h"
#include "thru_layers_rhythm.h"
#include "thru_reverb.h"
#include "thru_tape_layer.h"
#include <array>
#include <cmath>

namespace s3g_thru_layers {
class Extended {
  std::array<s3g::MacroFractureCore, 2> fracture_;
  std::array<s3g::MacroShredCore, 2> shred_;
  s3g::BufferProcessor buffer_;
  s3g_daisy::ThruReverb reverb_;
  TapeLayer tape_;
  StereoPitch pitch_;
  RotaryLayer rotary_;
  Rhythm rhythm_;
  Oversample2 oversample_;
  BusConditioner conditioner_;
  std::array<float, 2> reverbHigh_{};
  float reverbHpf_ = 0, reverbHpfTarget_ = 0;
  float rate_ = 44100, logicPhase_ = 0;
  std::array<float, 2> logicLevel_{};
  std::array<float, 4> previous_{}, previousDetail_{};
  int algorithm_ = -1;

public:
  bool prepare(float rate, float *history, std::size_t capacity) {
    rate_ = rate;
    for (auto &f : fracture_) {
      f.setExternalOversampling(true);
      f.prepare(rate * 2);
    }
    for (auto &s : shred_) {
      s.setExternalOversampling(true);
      s.prepare(rate * 2, .05);
    }
    buffer_.prepare(rate, 2, 2.1);
    conditioner_.prepare(rate);
    oversample_.process({0, 0}, [](auto x) { return x; });
    oversample_.reset();
    return reverb_.prepare(rate) && tape_.prepare(rate) &&
           pitch_.prepare(rate) && rotary_.prepare(rate) &&
           rhythm_.prepare(rate, history, capacity);
  }
  void context(const RhythmClock &clock, bool engaged) {
    rhythm_.context(clock, engaged);
  }
  void reset(int algorithm) {
    algorithm_ = -1;
    logicPhase_ = 0;
    logicLevel_ = {};
    oversample_.reset();
    conditioner_.reset();
    reverbHigh_ = {};
    if (algorithm == 6)
      for (auto &f : fracture_)
        f.reset();
    if (algorithm == 7)
      for (auto &s : shred_)
        s.reset();
    if (algorithm == 8)
      buffer_.reset();
    if (algorithm == 9)
      reverb_.reset();
    if (algorithm == 10)
      tape_.reset();
    if (algorithm == 11)
      pitch_.reset();
    if (algorithm == 12)
      rotary_.reset();
    if (algorithm >= 13)
      rhythm_.reset();
  }
  void beginBlock(int algorithm, const std::array<float, 4> &p,
                  const std::array<float, 4> &a) {
    const bool changed =
        algorithm_ != algorithm || p != previous_ || a != previousDetail_;
    if (changed && (algorithm == 6 || algorithm == 7))
      conditioner_.beginBlock(a);
    if (changed && algorithm == 6) {
      s3g::MacroFractureCoreParams c;
      c.processor = static_cast<s3g::FractureProcessor>(
          std::clamp(int(std::lround(p[0] * 9)), 0, 9));
      c.amount = p[1];
      c.color = p[2];
      c.bias = p[3] * 2 - 1;
      c.react = .1f;
      c.memory = 0;
      c.mix = 1;
      c.outputGainDb = 0;
      for (auto &f : fracture_)
        f.setParams(c);
    }
    if (changed && algorithm == 7) {
      s3g::MacroShredCoreParams c;
      c.circuit = static_cast<s3g::MacroShredCircuit>(
          std::clamp(int(std::lround(p[0] * 7)), 0, 7));
      c.pressure = p[1];
      c.shred = p[1];
      c.feedback = p[2] * .85f;
      c.color = p[3];
      c.react = .1f;
      c.mix = 1;
      c.outputGainDb = -6;
      for (auto &s : shred_)
        s.setParams(c);
    }
    if (changed && algorithm == 8) {
      s3g::BufferProcessorParams c;
      c.sizeMs = 40 * std::pow(25.f, p[0]);
      c.rate = .25f * std::pow(16.f, p[1]);
      c.repeat = p[2];
      c.reverse = p[3];
      c.skip = p[3] * .8f;
      c.deviation = p[3] * .18f;
      c.crossfade = .01f + .44f * a[0];
      c.spread = a[3];
      c.skipGrid = std::round(a[2] * 4);
      c.mix = 1;
      c.outputGainDb = 0;
      c.glideMs = 10 * std::pow(25.f, a[1]);
      buffer_.setParams(c);
    }
    if (algorithm == 9) {
      reverb_.beginBlock(p[0] >= .5f ? 1 : 0, .2f * std::pow(100.f, p[1]),
                         200 * p[3], 500 * std::pow(36.f, p[2]), a[0], a[1],
                         a[2], 1);
      reverbHpfTarget_ =
          1 - std::exp(-6.28318530718f * 20 * std::pow(50.f, a[3]) / rate_);
    }
    if (algorithm == 10)
      tape_.beginBlock(p, a);
    if (changed && algorithm == 11)
      pitch_.beginBlock(p);
    if (changed && algorithm == 12)
      rotary_.beginBlock(p, a);
    if (algorithm >= 13)
      rhythm_.beginBlock(algorithm, p, a);
    previousDetail_ = a;
    previous_ = p;
    algorithm_ = algorithm;
  }
  std::array<float, 2> process(std::array<float, 2> in, int algorithm) {
    std::array<float, 2> out{};
    if (algorithm == 6 || algorithm == 7) {
      in = conditioner_.input(in);
      out = oversample_.process(in, [&](std::array<float, 2> hi) {
        std::array<float, 2> result{};
        if (algorithm == 6) {
          logicPhase_ += 137 / (rate_ * 2);
          logicPhase_ -= std::floor(logicPhase_);
          for (int c = 0; c < 2; ++c) {
            const float env = std::max(std::abs(hi[c]), std::abs(hi[1 - c]));
            logicLevel_[c] +=
                (env - logicLevel_[c]) * (env > logicLevel_[c] ? .1f : .001f);
            if (std::abs(logicLevel_[c]) < 1e-20f)
              logicLevel_[c] = 0;
            const float assist =
                std::sin(6.28318530718f * (logicPhase_ + c * .27f)) *
                logicLevel_[c] * .18f;
            result[c] = fracture_[c].processSample(hi[c], hi[1 - c] + assist);
          }
        } else
          for (int c = 0; c < 2; ++c)
            result[c] = shred_[c].processSample(hi[c]);
        return result;
      });
      out = conditioner_.output(out);
    } else if (algorithm == 8)
      buffer_.processFrame(in.data(), out.data());
    else if (algorithm == 9) {
      reverbHpf_ += (reverbHpfTarget_ - reverbHpf_) * .005f;
      for (int c = 0; c < 2; ++c) {
        reverbHigh_[c] += reverbHpf_ * (in[c] - reverbHigh_[c]);
        if (std::abs(reverbHigh_[c]) < 1e-20f)
          reverbHigh_[c] = 0;
        in[c] -= reverbHigh_[c];
      }
      out = reverb_.process(in, true);
    } else if (algorithm == 10)
      out = tape_.process(in);
    else if (algorithm == 11)
      out = pitch_.process(in);
    else if (algorithm == 12)
      out = rotary_.process(in);
    else if (algorithm >= 13)
      out = rhythm_.process(in);
    return out;
  }
};
} // namespace s3g_thru_layers
