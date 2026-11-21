#pragma once
#include <memory>
#include "thru_table_tables.h"
// Legacy file renderer. WT bank preparation is shared with the live TABLE engine;
// this complete-file renderer remains available for existing saved recipes.
namespace s3g_thru_synth {
struct WtDrumTables {
  static constexpr int size = 512, frames = 16, levels = 6;
  using Cycle = std::array<float, size + 1>;
  using Frame = std::array<Cycle, levels>;
  std::array<std::array<Frame, frames>, 2> banks{};
  template <class Cancel> bool prepare(const Recipe &r, Cancel cancel) {
    for(int osc=0;osc<2;++osc)if(!s3g_thru_table::WtBankData::build(int(r.wt[osc?wtTableB:wtTableA]),banks[osc],cancel))return false;
    return true;
  }
  float read(int osc, double phase, float position, float hz,
             float rate) const {
    phase -= std::floor(phase);
    const float at = float(phase * size),
                pt = std::clamp(position, 0.f, 1.f) * (frames - 1);
    const int i = std::min(size - 1, int(at)),
              f = std::min(frames - 2, int(pt));
    const float t = at - i, u = pt - f;
    const float lod =
        std::clamp(std::log2(std::max(1.f, 32 * hz / (rate * .38f))), 0.f,
                   float(levels - 1));
    const int l = std::min(levels - 2, int(lod));
    const float v = lod - l;
    const auto sample = [&](int frame, int mip) {
      const auto &c = banks[osc][frame][mip];
      return c[i] + (c[i + 1] - c[i]) * t;
    };
    const float a = sample(f, l) + (sample(f + 1, l) - sample(f, l)) * u;
    const float b =
        sample(f, l + 1) + (sample(f + 1, l + 1) - sample(f, l + 1)) * u;
    return a + (b - a) * v;
  }
};
struct WtDrumDecimator {
  struct Section {
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setup(double normalized, double q) {
      const double w = 6.283185307179586 * normalized, c = std::cos(w),
                   s = std::sin(w), a = s / (2 * q), d = 1 + a;
      b0 = (1 - c) * .5 / d;
      b1 = (1 - c) / d;
      b2 = b0;
      a1 = -2 * c / d;
      a2 = (1 - a) / d;
    }
    float tick(float x) {
      const double y = b0 * x + z1;
      z1 = b1 * x - a1 * y + z2;
      z2 = b2 * x - a2 * y;
      return float(y);
    }
  };
  std::array<Section, 3> sections{};
  WtDrumDecimator() {
    sections[0].setup(.1125, .51763809);
    sections[1].setup(.1125, .70710678);
    sections[2].setup(.1125, 1.93185165);
  }
  float tick(float x) {
    for (auto &s : sections)
      x = s.tick(x);
    return x;
  }
};
template <class Cancel, class Progress>
bool renderWtDrum(const Recipe &r, float rate, Audio &out, Cancel cancel,
                  Progress progress, const WtDrumTables *prepared = nullptr) {
  std::unique_ptr<WtDrumTables> owned;
  if (!prepared) {
    owned = std::make_unique<WtDrumTables>();
    if (!owned->prepare(r, cancel))
      return false;
    prepared = owned.get();
  }
  BankRender span(r, rate, out);
  const auto &v = r.wt;
  constexpr int oversample = 4;
  const float sr = rate * oversample, inv = 1 / sr;
  const float bodyTau = std::max(.001f, v[wtDecay] * .001f / 6.907755f),
              noiseTau = std::max(.0001f, v[wtNoiseDecay] * .001f / 6.907755f);
  const float pitchTau = v[wtPitchDecay] * .001f / 4.60517f,
              scanTau = v[wtScanDecay] * .001f / 4.60517f;
  const float dcPole = std::exp(-6.2831853f * 8 / sr),
              pinkSlew = 1 - std::exp(-6.2831853f * 600 / sr);
  std::array<WtDrumDecimator, 2> decimator{};
  std::array<Lowpass, 2> filter{};
  std::array<float, 2> pink{}, whiteLow{}, lastIn{}, dc{};
  std::array<double, 2> phaseA{}, phaseB{};
  double subPhase = 0;
  float pitchCurve = 0, tableCurve = 0, toneCurve = 0, levelCurve = 0;
  std::uint32_t rng = r.seed ^ 0x57544452u;
  const auto white = [&]() {
    rng = hash(rng + 0x9e3779b9u);
    return float(rng >> 8) * (2.f / 16777215.f) - 1;
  };
  for (std::size_t i = 0; i < span.total; ++i) {
    if ((i & 1023u) == 0) {
      if (cancel())
        return false;
      progress(unsigned(5 + i * 88 / span.total));
    }
    if ((i & 15u) == 0) {
      const float u = std::min(1.f, float(i % span.frames) /
                                        std::max(1.f, float(span.frames - 1))),
                  t = float(i) / rate;
      pitchCurve = animatedCurve(r, 0, u, t);
      tableCurve = animatedCurve(r, 1, u, t);
      toneCurve = animatedCurve(r, 2, u, t);
      levelCurve = animatedCurve(r, 3, u, t);
    }
    float left = 0, right = 0;
    for (int os = 0; os < oversample; ++os) {
      const float t = (float(i) * oversample + os) * inv;
      const float pitch = v[wtBend] * std::exp(-t / pitchTau) + pitchCurve;
      const float hz =
          std::clamp(v[wtTune] * std::exp2(pitch / 12), 10.f, rate * .42f);
      const float sweep = v[wtScan] * std::exp(-t / scanTau) + tableCurve;
      const float posA = std::clamp(v[wtPosA] + sweep, 0.f, 1.f),
                  posB = std::clamp(v[wtPosB] + sweep, 0.f, 1.f);
      const float body = std::exp(-t / bodyTau),
                  bendEnv = std::exp(-t / std::max(.0001f, pitchTau));
      float noiseEnv = std::exp(-t / noiseTau);
      if (int(v[wtModel]) == 4) { // Three softened exciter taps form the clap.
        for (float offset : {.012f, .024f})
          if (t >= offset)
            noiseEnv += .65f * std::exp(-(t - offset) /
                                        std::max(.001f, noiseTau * .45f));
      }
      const float hitEnv = std::exp(-t / .0035f), common = white();
      const float sub =
          std::sin(float(6.283185307179586 * subPhase)) * v[wtSub] * body * .6f;
      subPhase += hz * inv;
      subPhase -= std::floor(subPhase);
      for (int ch = 0; ch < 2; ++ch) {
        const float detune =
            std::exp2((ch ? 1.f : -1.f) * v[wtDetune] * r.v[width] / 2400);
        const float fa = std::min(rate * .43f, hz * detune),
                    fb = std::min(rate * .43f, fa * v[wtRatio]);
        const float b = prepared->read(1, phaseB[ch], posB, fb, rate);
        const float fmAmount = v[wtFm] * (.25f + .75f * bendEnv);
        const float a =
            prepared->read(0, phaseA[ch] + fmAmount * b * .25f, posA,
                           std::min(rate, fa + fb * fmAmount * 4), rate);
        phaseA[ch] += fa * inv;
        phaseA[ch] -= std::floor(phaseA[ch]);
        phaseB[ch] += fb * inv;
        phaseB[ch] -= std::floor(phaseB[ch]);
        const float n = common * (1 - r.v[width] * .3f) +
                        (ch ? white() : common) * r.v[width] * .3f;
        pink[ch] += (n - pink[ch]) * pinkSlew;
        whiteLow[ch] += (n - whiteLow[ch]) * .025f;
        const int color = int(v[wtNoiseColor]);
        const float noise =
            color == 1
                ? pink[ch] * 2.5f
                : color == 2
                      ? n - whiteLow[ch]
                      : color == 3 ? n * std::sin(float(6.283185307179586 *
                                                        phaseB[ch]))
                                   : n;
        float x = ((a * (1 - v[wtBlend]) + b * v[wtBlend]) * body + sub) * .9f +
                  noise * noiseEnv * v[wtNoise] * .65f;
        const float tick =
            (common * .65f +
             std::sin(float(t * 6.283185307179586 * (1500 + v[wtTune] * 2))) *
                 .35f) *
            hitEnv * v[wtImpact] * .42f;
        x += tick;
        const float cutoff = std::clamp(
            300.f * std::exp2(std::clamp(v[wtTone] + toneCurve, 0.f, 1.f) * 6),
            40.f, rate * .43f);
        x = filter[ch].tick(x, cutoff, 0, sr);
        const float drive = 1 + v[wtDrive] * 7;
        x += v[wtDrive] * (std::tanh(x * drive) / std::tanh(drive) - x);
        const float comp = 1 + v[wtComp] * 5;
        x += v[wtComp] * (std::tanh(x * comp) / std::tanh(comp) - x);
        const float hp = x - lastIn[ch] + dcPole * dc[ch];
        lastIn[ch] = x;
        dc[ch] = hp;
        const float result = decimator[ch].tick(hp);
        if (ch)
          right = result;
        else
          left = result;
      }
    }
    const float env = fileEnvelope(r, i, span.frames, span.splice, rate) *
                      std::exp2(levelCurve / 6);
    out.left[i] = left * env;
    out.right[i] = right * env;
  }
  finishSound(r, rate, out, span.frames, span.splice, v[wtEdge]);
  progress(100);
  return true;
}
} // namespace s3g_thru_synth
