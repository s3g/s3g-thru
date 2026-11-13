#pragma once
#include "thru_drive.h"
#include "thru_layers_extended.h"
#include "thru_layers_macros.h"
#include "thru_layers_spectral.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

namespace s3g_thru_layers {
inline constexpr int algorithmCount = 17;
inline std::array<float, 4> detailDefaults(int algorithm) {
  if (algorithm == 2 || algorithm == 5)
    return {.65f, .75f, .4f, 1};
  if (algorithm == 4)
    return {0, 0, .5f, 0};
  if (algorithm == 6 || algorithm == 7)
    return {1.f / 3, .1f, .75f, .7f};
  if (algorithm == 8)
    return {.44f, .4f, 0, 0};
  if (algorithm == 9)
    return {.72f, .12f, 1, 0};
  if (algorithm == 10)
    return {.5f, .5f, .5f, .5f};
  if (algorithm == 12)
    return {.50865197f, .5f, 1, 1};
  if (algorithm == 13)
    return {1, 0, 0, .25f};
  if (algorithm == 14)
    return {1, 0, .5f, 0};
  if (algorithm == 15)
    return {1, .5f, .17f, .25f};
  if (algorithm == 16)
    return {0, .5f, 0, .25f};
  return {};
}

// Values below -400 dB are inaudible; clear them before recursive state can
// enter the subnormal range on the audio CPU.
inline float quiet(float v) { return std::abs(v) < 1e-20f ? 0.f : v; }
inline void slew(float &state, float target, float coefficient) {
  state += (target - state) * coefficient;
  if (std::abs(target - state) < 1e-8f)
    state = target;
}
inline float unit(float v) {
  return std::isfinite(v) ? std::clamp(v, 0.f, 1.f) : 0.f;
}
struct Point {
  float x = 0, y = 0;
};
inline Point padPoint(int pad) {
  return {float(pad % 4) / 3.f, 1.f - float(pad / 8) / 3.f};
}
inline int cornerForPad(int pad) {
  const auto p = padPoint(pad);
  return (p.x == 0 || p.x == 1) && (p.y == 0 || p.y == 1)
             ? int(p.y) * 2 + int(p.x)
             : -1;
}
inline std::array<float, 4> weights(Point p) {
  const float x = unit(p.x), y = unit(p.y);
  return {(1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y};
}
template <std::size_t Fields> struct Patch {
  static constexpr std::size_t fx = Fields * 4, config = fx + 32;
  // Reuse tagged, previously reserved prototype fields without changing the
  // fixed clip-bank layout or any existing MIDI/automation index.
  static constexpr std::size_t advanced = fx - 32, advancedCount = 16,
                               advancedVersion = advanced + 16;
  static constexpr float advancedTag = 31005;
  static constexpr bool editable(std::size_t i) {
    return (i >= advanced && i < advanced + advancedCount) ||
           (i >= fx && i < size);
  }
  void upgrade() {
    if (values[advancedVersion] == advancedTag)
      return;
    for (int c = 0; c < 4; ++c) {
      const auto a = detailDefaults(int(values[fx + c * 8]));
      for (int k = 0; k < 4; ++k)
        values[advanced + c * 4 + k] = a[k];
    }
    values[advancedVersion] = advancedTag;
  }
  enum Setting : std::size_t {
    play,
    engineX,
    engineY,
    glide,
    weight0,
    weight1,
    weight2,
    weight3,
    depth,
    zMode,
    pressure,
    returnDry,
    macro0,
    macro1,
    macro2,
    macro3,
    engineZ,
    count
  };
  static constexpr std::size_t size = config + count;
  bool active = false, initialized = false;
  std::array<float, size> values{};
  Patch() {
    const float defaults[4][8]{{0, .5f, .2f, 0, 0, 0, 0, 1},
                               {1, .64f, .2f, 0, 0, 1, .35f, 1},
                               {2, .4f, .55f, .5f, .65f, 2, .4f, 1},
                               {3, .58f, .4f, .65f, .65f, 2, .25f, 1}};
    for (int c = 0; c < 4; ++c)
      for (int f = 0; f < 8; ++f)
        values[fx + c * 8 + f] = defaults[c][f];
    values[config + glide] = 120;
    values[config + weight0] = 1;
    values[config + depth] = 1;
    values[config + pressure] = 1;
    values[config + engineZ] = .35f;
    upgrade();
  }
  static float bound(std::size_t i, float v) {
    if (!std::isfinite(v))
      return 0;
    if (i >= advanced && i < advanced + advancedCount)
      return unit(v);
    if (i < fx)
      return std::clamp(v, -100000.f, 100000.f);
    if (i < config) {
      const auto f = (i - fx) % 8;
      return f == 0
                 ? std::round(std::clamp(v, 0.f, float(algorithmCount - 1)))
                 : f == 5 ? std::round(std::clamp(v, 0.f, 9.f))
                          : f == 6 ? std::clamp(v, -1.f, 1.f)
                                   : f == 7 ? std::clamp(v, 0.f, 2.f) : unit(v);
    }
    const auto f = i - config;
    if (f == glide)
      return std::clamp(v, 5.f, 1500.f);
    if (f >= macro0 && f <= macro3)
      return std::clamp(v, -1.f, 1.f);
    if (f == play || f == zMode || f == returnDry)
      return std::round(unit(v));
    return unit(v);
  }
  void set(std::size_t i, float v) {
    if (i < size)
      values[i] = bound(i, v);
  }
  float setting(Setting s) const { return values[config + s]; }
  bool discrete(std::size_t i) const {
    if (i >= advanced && i < advanced + advancedCount) {
      const int c = int((i - advanced) / 4), f = int((i - advanced) % 4),
                algorithm = int(values[fx + c * 8]);
      return (algorithm == 13 && f == 2) ||
             (algorithm >= 13 &&
              (f == 0 || (algorithm == 14 && (f == 1 || f == 3)) ||
               (algorithm == 15 && (f == 1 || f == 2)) ||
               (algorithm == 16 && (f == 1 || f == 2)))) ||
             (f == 0 && (algorithm == 3 || algorithm == 4)) ||
             (f == 2 && algorithm == 8);
    }
    if (i >= fx && i < config) {
      const auto c = (i - fx) / 8, f = (i - fx) % 8;
      const int algorithm = int(values[fx + c * 8]);
      if ((algorithm == 13 && (f == 2 || f == 4)) ||
          (algorithm == 15 && f == 2))
        return true;
    }
    return i >= fx && i < config ? ((i - fx) % 8 == 0 || (i - fx) % 8 == 5)
                                 : i == config + play || i == config + zMode ||
                                       i == config + returnDry;
  }
};

class Slot {
  Blur blur_;
  s3g_daisy::ThruDrive drive_;
  std::unique_ptr<float[]> delay_;
  std::size_t delaySize_ = 0, write_ = 0, alignWrite_ = 0;
  std::array<std::array<float, 512>, 2> alignment_{};
  std::array<float, 2> integrator1_{}, integrator2_{}, delayTone_{};
  std::unique_ptr<Extended> extended_;
  std::array<float, 4> fieldOld_{}, fieldNew_{}, fieldTarget_{};
  float fieldFade_ = 1, fieldClock_ = 0;
  bool fieldReady_ = false;

  std::array<float, 4> p_{.5f, .2f, 0, 0}, a_{}, detailTargets_{},
      coefficientDetail_{};
  std::array<float, 2> delayHigh_{};
  float high_ = 0, targetHigh_ = 0, fieldTone_ = 0, targetFieldTone_ = 0,
        duckEnvelope_ = 0, duckGain_ = 1, duckAttack_ = 0, duckRelease_ = 0;
  float rate_ = 44100, slew_ = 0, g_ = 0, k_ = 1, tone_ = 1;
  float oldTime_ = 1, newTime_ = 1, pendingTime_ = 1, timeFade_ = 1, swap_ = 0;
  float targetG_ = 0, targetK_ = 1, targetTone_ = 1;
  int algorithm_ = 0, pendingAlgorithm_ = 0, swapDirection_ = 0;
  std::array<float, 4> coefficientParams_{};
  bool coefficientsReady_ = false, driveRunning_ = false;
  int driveTail_ = 0;
  float driveFade_ = 0;

public:
  bool prepare(float r, int index) {
    rate_ = r;
    coefficientsReady_ = false;
    delaySize_ = std::size_t(std::ceil(r * 8.1f)) + 4;
    delay_.reset(new (std::nothrow) float[delaySize_ * 2]{});
    slew_ = 1 - std::exp(-1 / (r * .008f));
    duckAttack_ = 1 - std::exp(-1 / (r * .003f));
    duckRelease_ = 1 - std::exp(-1 / (r * .18f));
    blur_.prepare(r, float(index));
    try {
      extended_.reset(new Extended);
      if (!extended_->prepare(r, delay_.get(), delaySize_))
        return false;
    } catch (...) {
      return false;
    }
    swap_ = 1;
    algorithm_ = pendingAlgorithm_ = index;
    return bool(delay_);
  }
  int algorithm() const { return algorithm_; }
  void context(const RhythmClock &clock, bool engaged) {
    extended_->context(clock, engaged);
  }
  void request(int alg) {
    pendingAlgorithm_ = std::clamp(alg, 0, algorithmCount - 1);
    if (pendingAlgorithm_ != algorithm_ && swapDirection_ == 0)
      swapDirection_ = -1;
  }
  void beginBlock(const std::array<float, 4> &targets) {
    beginBlock(targets, detailDefaults(algorithm_));
  }
  void beginBlock(const std::array<float, 4> &targets,
                  const std::array<float, 4> &detail) {
    detailTargets_ = detail;
    if (!coefficientsReady_ || coefficientParams_ != targets ||
        coefficientDetail_ != detail) {
      coefficientParams_ = targets;
      coefficientDetail_ = detail;
      coefficientsReady_ = true;
      targetG_ = std::tan(
          3.14159265359f *
          std::min(25 * std::pow(720.f, targets[0]), rate_ * .42f) / rate_);
      targetK_ = 1 / (.707f + 12 * targets[1] * targets[1]);
      targetTone_ = 1 - std::exp(-6.28318530718f *
                                 (100 * std::pow(160.f, targets[2])) / rate_);
      pendingTime_ = rate_ * .02f *
                     std::pow(100.f * std::pow(2.f, std::round(detail[0] * 2)),
                              targets[0]);
      targetHigh_ = 1 - std::exp(-6.28318530718f * 20 *
                                 std::pow(50.f, detail[1]) / rate_);
      targetFieldTone_ = 1 - std::exp(-6.28318530718f * 1200 *
                                      std::pow(15.f, detail[1]) / rate_);
    }
    if (algorithm_ == 4) {
      const float base =
          rate_ * .02f *
          std::pow(25.f * std::pow(2.f, std::round(detailTargets_[0] * 2)),
                   targets[0]);
      for (int tap = 0; tap < 4; ++tap)
        fieldTarget_[tap] =
            std::min(float(delaySize_ - 2), base *(1 + targets[1] * tap));
    }
    if (algorithm_ >= 6)
      extended_->beginBlock(algorithm_, targets, detailTargets_);
    if (delayValid_ == 0) {
      oldTime_ = newTime_ = pendingTime_;
      timeFade_ = 1;
    }
  }
  void prime(int algorithm, const std::array<float, 4> &targets) {
    prime(algorithm, targets, detailDefaults(algorithm));
  }
  void prime(int algorithm, const std::array<float, 4> &targets,
             const std::array<float, 4> &detail) {
    algorithm_ = pendingAlgorithm_ =
        std::clamp(algorithm, 0, algorithmCount - 1);
    swap_ = 1;
    swapDirection_ = 0;
    p_ = targets;
    a_ = detail;
    beginBlock(targets, detail);
    g_ = targetG_;
    k_ = targetK_;
    tone_ = targetTone_;
    high_ = targetHigh_;
    fieldTone_ = targetFieldTone_;
    resetHistory();
    if (algorithm_ >= 13)
      extended_->context({}, true);
    if (algorithm_ >= 6)
      extended_->beginBlock(algorithm_, targets, detailTargets_);
    write_ = 0;
  }
  void reset() {
    resetHistory();
    write_ = alignWrite_ = 0;
  }
  std::array<float, 2> process(std::array<float, 2> in,
                               std::array<float, 4> targets) {
    return process(in, targets, detailDefaults(algorithm_));
  }
  std::array<float, 2> process(std::array<float, 2> in,
                               std::array<float, 4> targets,
                               const std::array<float, 4> &detail) {
    for (int f = 0; f < 4; ++f)
      slew(a_[f], detail[f], slew_);
    for (int f = 0; f < 4; ++f)
      slew(p_[f], targets[f], slew_);
    if (swapDirection_ < 0) {
      swap_ = std::max(0.f, swap_ - 1 / (rate_ * .012f));
      if (swap_ == 0) {
        algorithm_ = pendingAlgorithm_;
        resetHistory();
        if (algorithm_ == 4) {
          const float base =
              rate_ * .02f *
              std::pow(25.f * std::pow(2.f, std::round(detailTargets_[0] * 2)),
                       targets[0]);
          for (int tap = 0; tap < 4; ++tap)
            fieldTarget_[tap] =
                std::min(float(delaySize_ - 2), base *(1 + targets[1] * tap));
        }
        if (algorithm_ >= 6)
          extended_->beginBlock(algorithm_, targets, detailTargets_);
        swapDirection_ = 1;
      }
    } else if (swapDirection_ > 0) {
      swap_ = std::min(1.f, swap_ + 1 / (rate_ * .012f));
      if (swap_ == 1)
        swapDirection_ = 0;
    }
    std::array<float, 2> out = in;
    if (algorithm_ == 1) {
      // TPT state-variable filter. Coefficients slew with the controls.
      slew(g_, targetG_, slew_);
      slew(k_, targetK_, slew_);
      const float a = 1 / (1 + g_ * (g_ + k_));
      for (int c = 0; c < 2; ++c) {
        const float v1 = a * (integrator1_[c] + g_ * (in[c] - integrator2_[c]));
        const float v2 = integrator2_[c] + g_ * v1;
        integrator1_[c] = quiet(2 * v1 - integrator1_[c]);
        integrator2_[c] = quiet(2 * v2 - integrator2_[c]);
        const float high = in[c] - k_ * v1 - v2;
        out[c] = p_[3] < .5f ? v2 + (v1 - v2) * (p_[3] * 2)
                             : v1 + (high - v1) * ((p_[3] - .5f) * 2);
      }
      const float amount = p_[2] * 6;
      if (amount > 1e-7f) {
        if (!driveRunning_) {
          drive_.resetNonlinearHistory();
          driveRunning_ = true;
          driveFade_ = 0;
        }
        driveTail_ = 64;
      }
      if (driveRunning_) {
        driveFade_ = std::min(1.f, driveFade_ + 1 / (rate_ * .004f));
        out = drive_.process(out, amount, driveFade_);
        if (amount <= 1e-7f && --driveTail_ <= 0) {
          driveRunning_ = false;
          driveFade_ = 0;
        }
      } else
        out = drive_.processClean(out);
    } else if (algorithm_ == 2)
      out = blur_.process(in, p_, false, a_);
    else if (algorithm_ == 5)
      out = blur_.process(in, p_, true, a_);
    else if (algorithm_ == 4)
      out = field(in);
    else if (algorithm_ >= 6)
      out = extended_->process(in, algorithm_);
    else if (algorithm_ == 3) {
      if (timeFade_ >= 1 && std::abs(pendingTime_ - newTime_) > .5f) {
        oldTime_ = newTime_;
        newTime_ = pendingTime_;
        timeFade_ = 0;
      }
      timeFade_ = std::min(1.f, timeFade_ + 1 / (rate_ * .035f));
      const float a = timeFade_ * timeFade_ * (3 - 2 * timeFade_);
      for (int c = 0; c < 2; ++c)
        out[c] =
            read(c, oldTime_) + (read(c, newTime_) - read(c, oldTime_)) * a;
      slew(tone_, targetTone_, slew_);
      for (int c = 0; c < 2; ++c)
        delayTone_[c] = quiet(delayTone_[c] + tone_ * (out[c] - delayTone_[c]));
      slew(high_, targetHigh_, slew_);
      for (int c = 0; c < 2; ++c) {
        const float feedback =
            (delayTone_[c] * (1 - p_[3]) + delayTone_[1 - c] * p_[3]) *
            std::min(.9f, p_[1] * .9f);
        delayHigh_[c] = quiet(delayHigh_[c] + high_ * (in[c] - delayHigh_[c]));
        const float shaped = feedback / (1 + a_[2] * 3 * std::abs(feedback));
        delay_[c * delaySize_ + write_] = in[c] - delayHigh_[c] + shaped;
      }
      write_ = (write_ + 1) % delaySize_;
      delayValid_ = std::min(delaySize_, delayValid_ + 1);
    }
    if (algorithm_ == 3 || algorithm_ == 4) {
      const float peak = std::max(std::abs(in[0]), std::abs(in[1]));
      duckEnvelope_ += (peak - duckEnvelope_) *
                       (peak > duckEnvelope_ ? duckAttack_ : duckRelease_);
      if (duckEnvelope_ < 1e-20f)
        duckEnvelope_ = 0;
      const float target =
          1 / (1 + std::max(0.f, duckEnvelope_ - .04f) * 35 * a_[3]);
      duckGain_ += (target - duckGain_) *
                   (target < duckGain_ ? duckAttack_ : duckRelease_);
      for (auto &v : out)
        v *= duckGain_;
    }
    // Spectral latency is fixed, independent of selected corner/mix.
    if (algorithm_ != 2 && algorithm_ != 5)
      for (int c = 0; c < 2; ++c) {
        const float v = alignment_[c][alignWrite_];
        alignment_[c][alignWrite_] = out[c];
        out[c] = v;
      }
    const auto alignmentFrames =
        algorithm_ == 1
            ? 512u - s3g_daisy::ThruDrive::latencyFrames
            : algorithm_ == 10 ? 510u
                               : (algorithm_ == 6 || algorithm_ == 7)
                                     ? 512u - Oversample2::latency
                                     : 512u;
    alignWrite_ = (alignWrite_ + 1) % alignmentFrames;
    for (auto &v : out)
      v = std::isfinite(v) ? v * swap_ : 0;
    return out;
  }

private:
  std::array<float, 2> field(std::array<float, 2> in) {
    if (!fieldReady_) {
      fieldOld_ = fieldNew_ = fieldTarget_;
      fieldReady_ = true;
      fieldFade_ = 1;
    }
    if (fieldFade_ >= 1 && fieldNew_ != fieldTarget_) {
      fieldOld_ = fieldNew_;
      fieldNew_ = fieldTarget_;
      fieldFade_ = 0;
    }
    fieldFade_ = std::min(1.f, fieldFade_ + 1 / (rate_ * .035f));
    const float fade = fieldFade_ * fieldFade_ * (3 - 2 * fieldFade_);
    fieldClock_ =
        std::fmod(fieldClock_ + (.02f + .8f * p_[3] * p_[3]) / rate_, 1.f);
    constexpr float gain[]{.4f, .3f, .2f, .1f};
    std::array<float, 2> out{};
    const auto &plan = s3g_daisy::ThruSpectralPlan::get();
    for (int tap = 0; tap < 4; ++tap) {
      const float l =
          read(0, fieldOld_[tap]) +
          (read(0, fieldNew_[tap]) - read(0, fieldOld_[tap])) * fade;
      const float r =
          read(1, fieldOld_[tap]) +
          (read(1, fieldNew_[tap]) - read(1, fieldOld_[tap])) * fade;
      const float pan =
          std::clamp((float(tap) - 1.5f) * .4f * p_[1] +
                         plan.sinCycle(fieldClock_ + tap * .25f) * p_[3] * .5f,
                     -.9f, .9f);
      const float mid = (l + r) * .5f, side = (l - r) * .5f;
      out[0] += (mid * (1 + pan) + side) * gain[tap];
      out[1] += (mid * (1 - pan) - side) * gain[tap];
    }
    slew(fieldTone_, targetFieldTone_, slew_);
    for (int c = 0; c < 2; ++c) {
      delayTone_[c] =
          quiet(delayTone_[c] + fieldTone_ * (out[c] - delayTone_[c]));
      delay_[c * delaySize_ + write_] =
          in[c] + std::tanh(delayTone_[c] * p_[2] * .85f);
    }
    write_ = (write_ + 1) % delaySize_;
    delayValid_ = std::min(delaySize_, delayValid_ + 1);
    const float mid = (out[0] + out[1]) * .5f, side = (out[0] - out[1]) * a_[2];
    return {mid + side, mid - side};
  }
  float read(int c, float frames) const {
    const float n = std::clamp(frames, 1.f, float(delaySize_ - 2));
    if (n > float(delayValid_))
      return 0;
    float at = float(write_) - n;
    if (at < 0)
      at += float(delaySize_);
    const auto i = std::size_t(at) % delaySize_, j = (i + 1) % delaySize_;
    const float t = at - std::floor(at);
    return delay_[c * delaySize_ + i] +
           (delay_[c * delaySize_ + j] - delay_[c * delaySize_ + i]) * t;
  }
  void resetHistory() {
    blur_.reset();
    alignment_ = {};
    integrator1_ = {};
    integrator2_ = {};
    delayTone_ = {};
    delayHigh_ = {};
    duckEnvelope_ = 0;
    duckGain_ = 1;
    drive_.reset();
    driveRunning_ = false;
    driveTail_ = 0;
    driveFade_ = 0;
    // Delay uses a generation-free bounded clear: prepare allocates once,
    // and a swap mutes output while its existing ring is overwritten.
    delayValid_ = 0;
    fieldReady_ = false;
    fieldClock_ = 0;
    if (extended_)
      extended_->reset(algorithm_);
    alignWrite_ = 0;
    oldTime_ = newTime_ = pendingTime_;
    timeFade_ = 1;
  }
  std::size_t delayValid_ = 0;
};

class Rack {
  using Slots = std::array<Slot, 4>;
  // Keep spectral histories stationary when clip fades exchange racks.
  std::unique_ptr<Slots> slots_;
  std::array<float, 4> weights_{1, 0, 0, 0};
  std::array<float, 4> trim_{1, 1, 1, 1};
  std::array<std::array<float, 512>, 2> dry_{};
  std::size_t write_ = 0;
  float rate_ = 44100, depth_ = 1;

public:
  bool prepare(float r) {
    if (!slots_) slots_.reset(new (std::nothrow) Slots {});
    if (!slots_) return false;
    rate_ = r;
    for (int i = 0; i < 4; ++i)
      if (!(*slots_)[i].prepare(r, i))
        return false;
    return true;
  }
  const std::array<float, 4> &currentWeights() const { return weights_; }
  void swapWith(Rack &other) noexcept {
    slots_.swap(other.slots_);
    weights_.swap(other.weights_);
    trim_.swap(other.trim_);
    dry_.swap(other.dry_);
    std::swap(write_, other.write_);
    std::swap(depth_, other.depth_);
    std::swap(rate_, other.rate_);
  }
  template <class PatchType> void prime(const PatchType &patch) {
    dry_ = {};
    write_ = 0;
    float sum = 0;
    const float z = patch.setting(PatchType::pressure);
    depth_ = patch.setting(PatchType::depth) *
             (patch.setting(PatchType::zMode) < .5f ? z : 1.f);
    for (int c = 0; c < 4; ++c) {
      const auto i = PatchType::fx + c * 8;
      std::array<float, 4> params{}, detail{};
      for (int f = 0; f < 4; ++f) {
        params[f] = patch.values[i + 1 + f];
        if (patch.setting(PatchType::zMode) >= .5f &&
            int(patch.values[i + 5]) == f + 1)
          params[f] = unit(params[f] + z * patch.values[i + 6]);
      }
      for (int f = 0; f < 4; ++f) {
        detail[f] = patch.values[PatchType::advanced + c * 4 + f];
        if (patch.setting(PatchType::zMode) >= .5f &&
            int(patch.values[i + 5]) == f + 5)
          detail[f] = unit(detail[f] + z * patch.values[i + 6]);
      }
      if (patch.setting(PatchType::zMode) >= .5f &&
          int(patch.values[i + 5]) == 9)
        pressureMacro(int(patch.values[i]), z *patch.values[i + 6], params,
                      detail);
      (*slots_)[c].prime(int(patch.values[i]), params, detail);
      trim_[c] = patch.values[i + 7];
      weights_[c] =
          unit(patch.values[PatchType::config + PatchType::weight0 + c]);
      sum += weights_[c];
    }
    if (sum < 1e-6f)
      weights_ = {1, 0, 0, 0};
    else
      for (float &w : weights_)
        w /= sum;
  }
  void reset() {
    if (slots_) for (auto &slot : *slots_)
      slot.reset();
    dry_ = {};
    write_ = 0;
    weights_ = {1, 0, 0, 0};
    depth_ = 1;
  }
  template <class PatchType, class Buffer>
  void process(std::size_t frames, Buffer &bus, const PatchType &patch,
               const RhythmClock &clock = {}) {
    std::array<std::array<float, 4>, 4> params{}, details{};
    std::array<float, 4> targets{};
    float sum = 0;
    const float z = patch.setting(PatchType::pressure),
                baseDepth = patch.setting(PatchType::depth);
    for (int c = 0; c < 4; ++c) {
      const auto i = PatchType::fx + c * 8;
      (*slots_)[c].request(int(patch.values[i]));
      for (int f = 0; f < 4; ++f) {
        params[c][f] = patch.values[i + 1 + f];
        if (patch.setting(PatchType::zMode) >= .5f &&
            int(patch.values[i + 5]) == f + 1)
          params[c][f] = unit(params[c][f] + z * patch.values[i + 6]);
      }
      for (int f = 0; f < 4; ++f) {
        details[c][f] = patch.values[PatchType::advanced + c * 4 + f];
        if (patch.setting(PatchType::zMode) >= .5f &&
            int(patch.values[i + 5]) == f + 5)
          details[c][f] = unit(details[c][f] + z * patch.values[i + 6]);
      }
      if (patch.setting(PatchType::zMode) >= .5f &&
          int(patch.values[i + 5]) == 9)
        pressureMacro(int(patch.values[i]), z *patch.values[i + 6], params[c],
                      details[c]);
      (*slots_)[c].context(
          clock,
          patch.values[PatchType::config + PatchType::weight0 + c] > .005f &&
              baseDepth > 1e-4f &&
              (patch.setting(PatchType::zMode) >= .5f || z > 1e-4f) &&
              patch.values[i + 7] > 1e-4f);
      (*slots_)[c].beginBlock(params[c], details[c]);
      targets[c] =
          unit(patch.values[PatchType::config + PatchType::weight0 + c]);
      sum += targets[c];
    }
    if (sum < 1e-6f) {
      targets = {1, 0, 0, 0};
      sum = 1;
    }
    for (float &w : targets)
      w /= sum;
    const float coefficient =
        1 - std::exp(-1 / (rate_ * patch.setting(PatchType::glide) * .001f));
    const float fast = 1 - std::exp(-1 / (rate_ * .012f));
    const float wantedDepth =
        baseDepth * (patch.setting(PatchType::zMode) < .5f ? z : 1.f);
    for (std::size_t f = 0; f < frames; ++f) {
      const std::array<float, 2> in{std::isfinite(bus[0][f]) ? bus[0][f] : 0.f,
                                    std::isfinite(bus[1][f]) ? bus[1][f] : 0.f};
      std::array<float, 2> out{}, dry{};
      slew(depth_, wantedDepth, fast);
      for (int ch = 0; ch < 2; ++ch) {
        dry[ch] = dry_[ch][write_];
        dry_[ch][write_] = in[ch];
      }
      write_ = (write_ + 1) % 512;
      for (int c = 0; c < 4; ++c) {
        slew(weights_[c], targets[c], coefficient);
        slew(trim_[c], patch.values[PatchType::fx + c * 8 + 7], fast);
        const auto processed = (*slots_)[c].process(in, params[c], details[c]);
        for (int ch = 0; ch < 2; ++ch)
          out[ch] += processed[ch] * weights_[c] * trim_[c];
      }
      for (int ch = 0; ch < 2; ++ch)
        bus[ch][f] = dry[ch] + (out[ch] - dry[ch]) * depth_;
    }
  }
};
} // namespace s3g_thru_layers
