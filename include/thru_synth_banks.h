#pragma once
namespace s3g_thru_synth {
inline float smoothRandom(std::uint32_t seed, float position) {
  const auto at = std::uint32_t(std::floor(position));
  float t = position - float(at);
  t = t * t * (3 - 2 * t);
  return (random(seed, at) * (1 - t) + random(seed, at + 1) * t) * 2 - 1;
}
struct BankRender {
  std::size_t frames, splice, total;
  explicit BankRender(const Recipe &r, float rate, Audio &out)
      : frames(std::size_t(std::llround(r.v[length] * rate))),
        splice(r.v[loop] > .5f ? std::min<std::size_t>(
                                     frames / 4, std::size_t(rate * .015f))
                               : 0),
        total(frames + splice) {
    out.left.assign(total, 0);
    out.right.assign(total, 0);
    out.recipe = r;
  }
};
inline float bankPan(std::uint32_t seed, int band) {
  return random(seed ^ 0x50414e34u, band + 1) * 2 - 1;
}
inline float bankFade(float hz, float rate) {
  return std::clamp((rate * .48f - hz) / (rate * .04f), 0.f, 1.f);
}
template <class Cancel, class Progress>
bool renderPartials(const Recipe &r, float rate, Audio &out, Cancel cancel,
                    Progress progress) {
  BankRender span(r, rate, out);
  const float slew = 1 - std::exp(-1 / (rate * .003f));
  std::array<double, 32> phase{};
  std::array<float, 32> hz{}, targetHz{}, gain{}, targetGain{}, pan{};
  for (int j = 0; j < 32; ++j) {
    phase[j] = random(r.seed ^ 0x50484153u, j + 1) * r.additive[aPhase];
    pan[j] = bankPan(r.seed, j) * r.v[width];
  }
  float levelGain = 1, targetLevel = 1;
  for (std::size_t i = 0; i < span.total; ++i) {
    if ((i & 1023u) == 0) {
      if (cancel())
        return false;
      progress(unsigned(i * 88 / span.total));
    }
    if ((i & 15u) == 0) {
      const float t = std::min(1.f, float(i % span.frames) /
                                        std::max(1.f, float(span.frames - 1))),
                  seconds = float(i) / rate;
      const float base =
                      440 * std::exp2((r.v[root] +
                                       animatedCurve(r, 0, t, seconds) - 69) /
                                      12),
                  count = std::clamp(r.additive[aCount] +
                                         animatedCurve(r, 1, t, seconds),
                                     1.f, 32.f),
                  stretch = r.additive[aStretch] *
                            std::exp2(animatedCurve(r, 2, t, seconds));
      targetLevel = std::pow(10.f, animatedCurve(r, 3, t, seconds) / 20);
      for (int j = 0; j < 32; ++j) {
        const float ratio = std::min(1000000.f,
                                     std::pow(r.ratios[j / 8][j % 8], stretch)),
                    drift = smoothRandom(r.seed ^ hash(j + 0x44524946u),
                                         seconds * r.additive[aDriftRate] +
                                             float(j) * .17f),
                    cents = r.additive[aDetune] *
                                (random(r.seed ^ 0x4445544eu, j + 1) * 2 - 1) +
                            30 * r.additive[aDrift] * drift,
                    frequency =
                        std::min(rate, base * ratio * std::exp2(cents / 1200));
        targetHz[j] = std::min(rate * .48f, frequency);
        targetGain[j] = r.partials[j / 8][j % 8] *
                        std::clamp(count - j, 0.f, 1.f) *
                        bankFade(frequency, rate) *
                        std::pow(ratio, r.additive[aTilt] / 6.0206f) *
                        std::exp(-r.additive[aDecay] * t * float(j) / 31 * 8);
        if (i == 0) {
          hz[j] = targetHz[j];
          gain[j] = targetGain[j];
        }
      }
      if (i == 0)
        levelGain = targetLevel;
    }
    float left = 0, right = 0;
    for (int j = 0; j < 32; ++j) {
      hz[j] += (targetHz[j] - hz[j]) * slew;
      gain[j] += (targetGain[j] - gain[j]) * slew;
      if (std::abs(gain[j]) + std::abs(targetGain[j]) > 1e-10f) {
        const float v = std::sin(Plan::tau * phase[j]) * gain[j];
        left += v * (1 - std::max(0.f, pan[j]));
        right += v * (1 + std::min(0.f, pan[j]));
      }
      phase[j] += hz[j] / rate;
      phase[j] -= std::floor(phase[j]);
    }
    levelGain += (targetLevel - levelGain) * slew;
    const float g =
        levelGain * fileEnvelope(r, i, span.frames, span.splice, rate);
    out.left[i] = left * g;
    out.right[i] = right * g;
  }
  finishSound(r, rate, out, span.frames, span.splice);
  progress(100);
  return !cancel();
}
struct NoiseBandState {
  float a = 0, b = 0, color = 0, g = 0, k = 1, targetG = 0, targetK = 1,
        gain = 0, targetGain = 0;
  float tick(float white, int type, float slew) {
    color += (white - color) * (type == 2 ? .005f : .15f);
    const float in =
        type == 0 ? white : type == 1 ? .5f * white + 1.5f * color : 10 * color;
    g += (targetG - g) * slew;
    k += (targetK - k) * slew;
    gain += (targetGain - gain) * slew;
    const float v1 = (a + g * (in - b)) / (1 + g * (g + k)), v2 = b + g * v1;
    a = 2 * v1 - a;
    b = 2 * v2 - b;
    return v1 * k * gain;
  }
};
template <class Cancel, class Progress>
bool renderNoiseBank(const Recipe &r, float rate, Audio &out, Cancel cancel,
                     Progress progress) {
  BankRender span(r, rate, out);
  const float slew = 1 - std::exp(-1 / (rate * .003f));
  std::array<NoiseBandState, 32> bands{};
  std::array<float, 32> pan{};
  for (int j = 0; j < 32; ++j)
    pan[j] = bankPan(r.seed, j) * r.v[width];
  float levelGain = 1, targetLevel = 1;
  for (std::size_t i = 0; i < span.total; ++i) {
    if ((i & 1023u) == 0) {
      if (cancel())
        return false;
      progress(unsigned(i * 88 / span.total));
    }
    if ((i & 15u) == 0) {
      const float t = std::min(1.f, float(i % span.frames) /
                                        std::max(1.f, float(span.frames - 1))),
                  seconds = float(i) / rate,
                  base =
                      440 * std::exp2((r.v[root] +
                                       animatedCurve(r, 0, t, seconds) - 69) /
                                      12),
                  bandwidth =
                      std::clamp(r.noise[nBandwidth] *
                                     std::exp2(animatedCurve(r, 1, t, seconds)),
                                 .1f, 48.f),
                  movement = std::clamp(r.noise[nMotion] +
                                            animatedCurve(r, 2, t, seconds),
                                        0.f, 24.f),
                  k = std::clamp(2 * std::sinh(.69314718f * bandwidth / 24),
                                 .005f, 3.9f);
      targetLevel = std::pow(10.f, animatedCurve(r, 3, t, seconds) / 20);
      for (int j = 0; j < 32; ++j) {
        auto &b = bands[j];
        const float ratio = std::min(1000000.f, std::pow(r.ratios[j / 8][j % 8],
                                                         r.noise[nStretch])),
                    motion = smoothRandom(r.seed ^ hash(j + 0x4d4f5645u),
                                          seconds * r.noise[nRate] +
                                              float(j) * .17f),
                    hz = std::min(rate, base * ratio *
                                            std::exp2(movement * motion / 12));
        b.targetG =
            std::tan(3.14159265359f * std::clamp(hz, 20.f, rate * .44f) / rate);
        b.targetK = k;
        b.targetGain = r.partials[j / 8][j % 8] *
                       std::clamp(r.noise[nCount] - j, 0.f, 1.f) *
                       bankFade(hz, rate) *
                       std::pow(ratio, r.noise[nTilt] / 6.0206f);
        if (i == 0) {
          b.g = b.targetG;
          b.k = k;
          b.gain = b.targetGain;
        }
      }
      if (i == 0)
        levelGain = targetLevel;
    }
    float left = 0, right = 0;
    for (int j = 0; j < 32; ++j) {
      auto &b = bands[j];
      if (std::abs(b.gain) + std::abs(b.targetGain) < 1e-10f)
        continue;
      const float v = b.tick(
          random(r.seed ^ hash(j + 0x4e4f4953u), std::uint32_t(i)) * 2 - 1,
          int(r.noise[nColor]), slew);
      left += v * (1 - std::max(0.f, pan[j]));
      right += v * (1 + std::min(0.f, pan[j]));
    }
    const float nl = random(r.seed ^ 0x4c45414bu, std::uint32_t(i)) * 2 - 1,
                nr = random(r.seed ^ 0x524c4541u, std::uint32_t(i)) * 2 - 1;
    left += nl * r.noise[nLeak];
    right += (nl + (nr - nl) * r.v[width]) * r.noise[nLeak];
    levelGain += (targetLevel - levelGain) * slew;
    const float g =
        levelGain * fileEnvelope(r, i, span.frames, span.splice, rate);
    out.left[i] = left * g;
    out.right[i] = right * g;
  }
  finishSound(r, rate, out, span.frames, span.splice);
  progress(100);
  return !cancel();
}
struct FmVoice {
  double carrier = 0, mod1 = 0, mod2 = 0;
  float feedback = 0;
  float tick(float hz, float rate, const Recipe &r, float index1,
             float index2) {
    const int alg = int(r.fmOsc[fAlgorithm]);
    const float maxMod = std::max(r.fmOsc[fRatio1],
                                  alg == 0 ? 0.f : r.fmOsc[fRatio2]) *
                         hz,
                safety = std::min(
                    1.f,
                    rate * .4f /
                        std::max(1.f, hz + maxMod * (index1 + index2 + 1)));
    index1 *= safety * bankFade(hz * r.fmOsc[fRatio1], rate);
    index2 *= safety * bankFade(hz * r.fmOsc[fRatio2], rate);
    const float m2 = std::sin(Plan::tau * mod2),
                m1 = std::sin(Plan::tau * mod1 +
                              feedback * r.fmOsc[fFeedback] * 2 +
                              (alg == 1 ? index2 * m2 : 0)),
                c = std::sin(Plan::tau * carrier),
                wet = std::sin(Plan::tau * carrier + index1 * m1 +
                               (alg == 2 ? index2 * m2 : 0)),
                v = alg == 3 ? wet * (1 - index2 / 12 + index2 / 12 * m2) : wet;
    feedback += (m1 - feedback) * .5f;
    carrier += hz / rate;
    mod1 += hz * r.fmOsc[fRatio1] / rate;
    mod2 += hz * r.fmOsc[fRatio2] / rate;
    carrier -= std::floor(carrier);
    mod1 -= std::floor(mod1);
    mod2 -= std::floor(mod2);
    return c + (v - c) * r.fmOsc[fMix];
  }
};
template <class Cancel, class Progress>
bool renderFm(const Recipe &r, float rate, Audio &out, Cancel cancel,
              Progress progress) {
  BankRender span(r, rate, out);
  constexpr int oversampling = 4, taps = 129, latency = (taps - 1) / 2;
  std::array<float, taps> kernel{}, historyL{}, historyR{};
  double sum = 0;
  for (int i = 0; i < taps; ++i) {
    const double x = i - latency, cut = .45 / oversampling,
                 sinc = std::abs(x) < 1e-10
                            ? 2 * cut
                            : std::sin(2 * 3.141592653589793 * cut * x) /
                                  (3.141592653589793 * x),
                 win = .42 -
                       .5 * std::cos(2 * 3.141592653589793 * i / (taps - 1)) +
                       .08 * std::cos(4 * 3.141592653589793 * i / (taps - 1));
    kernel[i] = float(sinc * win);
    sum += kernel[i];
  }
  for (auto &v : kernel)
    v /= float(sum);
  FmVoice voices[2];
  float hz = 0, index1 = 0, index2 = 0, targetHz = 0, targetI1 = 0,
        targetI2 = 0;
  const float slew = 1 - std::exp(-1 / (rate * oversampling * .003f)),
              detune = std::exp2(r.fmOsc[fDetune] / 2400);
  std::size_t head = 0;
  const auto count = span.total * oversampling + latency;
  for (std::size_t i = 0; i < count; ++i) {
    if ((i & 2047u) == 0) {
      if (cancel())
        return false;
      progress(unsigned(i * 88 / count));
    }
    if ((i & 63u) == 0) {
      const auto frame = std::min(span.total - 1, i / oversampling);
      const float t = std::min(1.f, float(frame % span.frames) /
                                        std::max(1.f, float(span.frames - 1))),
                  seconds = float(frame) / rate;
      targetHz = std::min(
          rate * .44f,
          440 * std::exp2((r.v[root] + animatedCurve(r, 0, t, seconds) - 69) /
                          12));
      targetI1 = std::clamp(r.fmOsc[fIndex1] + animatedCurve(r, 1, t, seconds),
                            0.f, 12.f);
      targetI2 =
          r.fmOsc[fAlgorithm] == 0
              ? 0
              : std::clamp(r.fmOsc[fIndex2] + animatedCurve(r, 2, t, seconds),
                           0.f, 12.f);
      if (i == 0) {
        hz = targetHz;
        index1 = targetI1;
        index2 = targetI2;
      }
    }
    hz += (targetHz - hz) * slew;
    index1 += (targetI1 - index1) * slew;
    index2 += (targetI2 - index2) * slew;
    const float a = voices[0].tick(hz / detune, rate * oversampling, r, index1,
                                   index2),
                b = voices[1].tick(hz * detune, rate * oversampling, r, index1,
                                   index2),
                mid = (a + b) * .5f;
    historyL[head] = mid + (a - mid) * r.v[width];
    historyR[head] = mid + (b - mid) * r.v[width];
    if (i >= latency && (i - latency) % oversampling == 0) {
      const auto frame = (i - latency) / oversampling;
      float left = 0, right = 0;
      for (int k = 0; k < taps; ++k) {
        const auto at = (head + taps - k) % taps;
        left += historyL[at] * kernel[k];
        right += historyR[at] * kernel[k];
      }
      out.left[frame] = left;
      out.right[frame] = right;
    }
    head = (head + 1) % taps;
  }
  float gain = 1;
  const float gainSlew = 1 - std::exp(-1 / (rate * .003f));
  for (std::size_t i = 0; i < span.total; ++i) {
    if ((i & 1023u) == 0 && cancel())
      return false;
    const float t = std::min(1.f, float(i % span.frames) /
                                      std::max(1.f, float(span.frames - 1))),
                target = std::pow(10.f,
                                  animatedCurve(r, 3, t, float(i) / rate) / 20);
    if (i == 0)
      gain = target;
    gain += (target - gain) * gainSlew;
    const float g = gain * fileEnvelope(r, i, span.frames, span.splice, rate);
    out.left[i] *= g;
    out.right[i] *= g;
  }
  finishSound(r, rate, out, span.frames, span.splice);
  progress(100);
  return !cancel();
}
} // namespace s3g_thru_synth
