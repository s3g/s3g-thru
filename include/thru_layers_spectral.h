#pragma once
#include "thru_spectral.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace s3g_thru_layers {
// Fixed-cost stereo WOLA: fractional bin travel, continuous tail phase and
// energy containment. Four overlapping Hann windows retain 512-frame latency.
class Blur {
  using Plan = s3g_daisy::ThruSpectralPlan;
  std::array<std::array<float, 512>, 2> input_{};
  std::array<std::array<float, 1024>, 2> ola_{};
  std::array<std::array<float, 257>, 2> magnitude_{}, previous_{}, phase_{},
      frequency_{}, lastRaw_{}, smoothMagnitude_{};
  std::array<std::array<Plan::Complex, 512>, 2> work_{};
  std::array<float, 257> positions_{};
  std::array<float, 2> energy_{};
  std::size_t write_ = 0, read_ = 0, hop_ = 0;
  float rate_ = 44100, clock_ = 0, seed_ = 0;

public:
  void prepare(float rate, float seed = 0) {
    rate_ = rate;
    seed_ = seed * .371f;
    (void)Plan::get();
    reset();
  }
  void reset() {
    input_ = {};
    ola_ = {};
    magnitude_ = {};
    previous_ = {};
    phase_ = {};
    lastRaw_ = {};
    smoothMagnitude_ = {};
    energy_ = {};
    work_ = {};
    for (int k = 0; k < 257; ++k) {
      positions_[k] = float(k);
      for (int c = 0; c < 2; ++c)
        frequency_[c][k] = k * .25f;
    }
    write_ = read_ = hop_ = 0;
    clock_ = seed_;
  }
  std::array<float, 2>
  process(std::array<float, 2> in, const std::array<float, 4> &p,
          bool spray = false,
          const std::array<float, 4> &a = {.65f, .75f, .4f, 1}) {
    std::array<float, 2> out{};
    for (int c = 0; c < 2; ++c) {
      out[c] = ola_[c][read_];
      ola_[c][read_] = 0;
      input_[c][write_] = in[c];
    }
    write_ = (write_ + 1) % 512;
    if (++hop_ == 128) {
      hop_ = 0;
      frame(p, a, spray);
    }
    read_ = (read_ + 1) % 1024;
    const float mid = (out[0] + out[1]) * .5f,
                side = (out[0] - out[1]) * (spray ? .5f : p[3]);
    return {mid + side, mid - side};
  }

private:
  static float clean(float v) { return std::abs(v) < 1e-20f ? 0 : v; }
  void frame(const std::array<float, 4> &p, const std::array<float, 4> &a,
             bool spray) {
    const auto &plan = Plan::get();
    constexpr float tau = 6.28318530718f;
    const float persistence = spray ? p[2] : p[1];
    const float decay =
        std::exp(-128.f / (rate_ * (.03f + 8 * persistence * persistence)));
    const float attack =
        1 - std::exp(-128.f / (rate_ * (.003f + 2 * p[0] * p[0])));
    const float soften =
        1 - std::exp(-128.f / (rate_ * (.002f + .025f * a[0] * a[0])));
    const float cutoff = 1200 * std::pow(15.f, a[1]);
    const bool frozen = spray && p[2] >= .995f;
    std::array<std::array<float, 257>, 2> raw{}, incoming{}, mappedAdvance{};
    std::array<float, 2> flux{}, sum{};
    for (int c = 0; c < 2; ++c) {
      for (int j = 0; j < 512; ++j)
        work_[c][j] = {input_[c][(write_ + j) % 512] * plan.window[j], 0};
      plan.fft(work_[c]);
      float inputEnergy = 0;
      for (int k = 1; k < 256; ++k) {
        const auto z = work_[c][k];
        raw[c][k] = std::hypot(z.real, z.imag);
        incoming[c][k] = std::atan2(z.imag, z.real) / tau;
        const float expected = k * .25f;
        if (raw[c][k] > 1e-7f)
          frequency_[c][k] = expected + Plan::wrap(incoming[c][k] -
                                                   previous_[c][k] - expected);
        previous_[c][k] = incoming[c][k];
        const float rising = std::max(0.f, raw[c][k] - lastRaw_[c][k]);
        flux[c] += rising;
        sum[c] += raw[c][k];
        lastRaw_[c][k] = raw[c][k];
        if (!frozen) {
          auto &m = magnitude_[c][k];
          if (spray)
            m = std::max(raw[c][k], m * decay);
          else if (raw[c][k] > m)
            m += (raw[c][k] - m) * attack;
          else
            m = std::max(raw[c][k], m * decay);
          m = clean(m);
        }
        inputEnergy += raw[c][k] * raw[c][k];
      }
      energy_[c] = clean(
          std::max(inputEnergy, energy_[c] * (frozen ? 1.f : decay * decay)));
      flux[c] =
          std::clamp(flux[c] / std::max(sum[c], 1e-12f) * 2, 0.f, 1.f) * a[2];
    }
    const float radius = spray ? 32 * p[0] * p[0] : 0;
    for (int k = 1; k < 256; ++k) {
      const float pattern =
          .62f * plan.sinCycle(k * .0113f + clock_) +
          .38f * plan.sinCycle(k * .0027f + clock_ * 2.137f +
                               plan.sinCycle(k * .00049f) * .5f);
      const float target = std::clamp(k + radius * pattern, 1.f, 255.f);
      positions_[k] += (target - positions_[k]) * soften;
      const int lo = std::clamp(int(positions_[k]), 1, 255),
                hi = std::min(255, lo + 1);
      const float fraction = positions_[k] - lo;
      for (int c = 0; c < 2; ++c) {
        const float lowResidual = frequency_[c][lo] - lo * .25f,
                    highResidual = frequency_[c][hi] - hi * .25f;
        mappedAdvance[c][k] =
            lowResidual + (highResidual - lowResidual) * fraction;
      }
    }
    for (int c = 0; c < 2; ++c) {
      float outEnergy = 0;
      for (int k = 1; k < 256; ++k) {
        const int lo = std::clamp(int(positions_[k]), 1, 255),
                  hi = std::min(255, lo + 1);
        const float f = positions_[k] - lo;
        auto magAt = [&](int n) {
          return magnitude_[c][std::clamp(n, 1, 255)];
        };
        const float center = magAt(lo) + (magAt(hi) - magAt(lo)) * f;
        const float neighbors = .25f * (magAt(lo - 1) + magAt(lo + 1) +
                                        magAt(hi - 1) + magAt(hi + 1));
        float mag = center + (neighbors - center) * a[0] * .45f;
        if (spray) {
          const float hole =
              .5f + .5f * plan.sinCycle(k * .257f + clock_ * .37f);
          mag *= 1 - p[3] * p[3] * hole * .70f;
        } else
          mag *=
              std::clamp(1 + (p[2] - .5f) * 1.5f * (k / 128.f - 1), .15f, 2.f);
        const float ratio = k * rate_ / 512 / cutoff;
        mag /= 1 + ratio * ratio * ratio * ratio;
        auto &smooth = smoothMagnitude_[c][k];
        smooth = clean(smooth + (mag - smooth) * soften);
        mag = smooth;
        const float weight =
            raw[1 - c][k] / std::max(1e-12f, raw[c][k] + raw[1 - c][k]);
        const float linked = mappedAdvance[c][k] +
                             (mappedAdvance[1 - c][k] - mappedAdvance[c][k]) *
                                 a[3] * weight * .35f;
        phase_[c][k] = Plan::wrap(
            phase_[c][k] + k * .25f + linked +
            (spray ? p[3] * .0015f * plan.sinCycle(k * .017f + clock_) : 0));
        const float live = std::clamp(
            raw[c][k] / std::max(magnitude_[c][k], 1e-12f), 0.f, 1.f);
        const float coherence =
            (spray ? std::max(0.f, 1 - std::abs(positions_[k] - k))
                   : (1 - p[0] * .7f)) *
            live;
        const float phasePull = coherence * coherence * (1 - a[0] * .65f);
        phase_[c][k] =
            Plan::wrap(phase_[c][k] +
                       Plan::wrap(incoming[c][k] - phase_[c][k]) * phasePull);
        Plan::Complex z{mag * plan.sinCycle(phase_[c][k] + .25f),
                        mag * plan.sinCycle(phase_[c][k])};
        // Original complex input preserves attacks without arbitrary phase
        // resets.
        z.real += (work_[c][k].real - z.real) * flux[c];
        z.imag += (work_[c][k].imag - z.imag) * flux[c];
        work_[c][k] = z;
        outEnergy += z.real * z.real + z.imag * z.imag;
      }
      const float norm = outEnergy > energy_[c] * 1.35f && outEnergy > 1e-12f
                             ? std::sqrt(energy_[c] * 1.35f / outEnergy)
                             : 1;
      for (int k = 1; k < 256; ++k) {
        work_[c][k].real *= norm;
        work_[c][k].imag *= norm;
        work_[c][512 - k] = {work_[c][k].real, -work_[c][k].imag};
      }
      work_[c][0] = {};
      work_[c][256] = {};
      plan.fft(work_[c], true);
      for (int j = 0; j < 512; ++j)
        ola_[c][(read_ + 1 + j) % 1024] +=
            work_[c][j].real * plan.window[j] * (2.f / 3.f);
    }
    if (spray)
      clock_ = Plan::wrap(clock_ + p[1] * .013f);
  }
};
} // namespace s3g_thru_layers
