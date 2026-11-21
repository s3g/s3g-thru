#pragma once
#include "thru_spectral.h"
#include "thru_synth_recipe.h"
#include "thru_synth_source.h"
namespace s3g_thru_synth {
using Plan = s3g_daisy::ThruSpectralPlan;
using Table = std::array<float, Plan::size>;
inline void makeTable(Table &output, std::uint32_t seed, std::uint32_t serial,
                      int count, float smooth, int harmonics) {
  count = std::clamp(count, 1, 32);
  const int half = count + 1, total = 2 * half;
  std::array<float, 66> nodes{};
  for (int i = 1; i < total; ++i)
    if (i != half)
      nodes[i] = (i < half ? 1.f : -1.f) *
                 (.05f + .95f * random(seed ^ hash(serial + 1), i));
  std::array<Plan::Complex, Plan::size> spectrum{};
  for (std::size_t i = 0; i < Plan::size; ++i) {
    const float x = float(i) * total / Plan::size;
    const int n = int(x);
    const float t = x - n;
    const float a = nodes[(n + total - 1) % total], b = nodes[n % total],
                c = nodes[(n + 1) % total], d = nodes[(n + 2) % total];
    const float linear = b + (c - b) * t;
    const float cubic =
        b + .5f * t *
                (c - a +
                 t * (2 * a - 5 * b + 4 * c - d + t * (3 * (b - c) + d - a)));
    spectrum[i].real = linear + (cubic - linear) * smooth;
  }
  const auto &plan = Plan::get();
  plan.fft(spectrum);
  spectrum[0] = {};
  spectrum[Plan::size / 2] = {};
  for (int i = harmonics + 1; i < int(Plan::size / 2); ++i) {
    spectrum[i] = {};
    spectrum[Plan::size - i] = {};
  }
  plan.fft(spectrum, true);
  float peak = 1e-6f;
  for (auto v : spectrum)
    peak = std::max(peak, std::abs(v.real));
  for (std::size_t i = 0; i < output.size(); ++i)
    output[i] = spectrum[i].real / peak;
}
inline float lookup(const Table &table, double phase) {
  const double x = phase * table.size();
  const auto i = std::size_t(x) % table.size();
  const float t = float(x - std::floor(x));
  const float a = table[(i + table.size() - 1) % table.size()], b = table[i],
              c = table[(i + 1) % table.size()],
              d = table[(i + 2) % table.size()];
  return b + .5f * t *
                 (c - a +
                  t * (2 * a - 5 * b + 4 * c - d + t * (3 * (b - c) + d - a)));
}
struct Audio {
  std::vector<float> left, right;
  Recipe recipe;
  std::array<float, 112> low{}, high{};
  std::array<char, 225> waveText{};
  std::shared_ptr<const SourceAudio> source, impulse;
  std::array<std::shared_ptr<const SourceAudio>, 4> inputs{};
  std::uint64_t revision = 0;
};
inline void finishSound(const Recipe &r, float rate, Audio &out,
                        std::size_t frames, std::size_t splice,
                        float attackEdgeMs = 2) {
  if (splice) {
    for (std::size_t i = 0; i < splice; ++i) {
      const float t = float(i) / std::max(1.f, float(splice - 1));
      const float w = t * t * (3 - 2 * t);
      out.left[frames + i] = out.left[frames + i] * (1 - w) + out.left[i] * w;
      out.right[frames + i] =
          out.right[frames + i] * (1 - w) + out.right[i] * w;
    }
    std::memmove(out.left.data(), out.left.data() + splice,
                 frames * sizeof(float));
    std::memmove(out.right.data(), out.right.data() + splice,
                 frames * sizeof(float));
  }
  out.left.resize(frames);
  out.right.resize(frames);
  double dcL = 0, dcR = 0;
  for (std::size_t i = 0; i < frames; ++i) {
    dcL += out.left[i];
    dcR += out.right[i];
  }
  dcL /= frames;
  dcR /= frames;
  float peak = 1e-9f;
  for (std::size_t i = 0; i < frames; ++i) {
    out.left[i] -= float(dcL);
    out.right[i] -= float(dcR);
    peak = std::max({peak, std::abs(out.left[i]), std::abs(out.right[i])});
  }
  const float norm = .89125094f * r.v[level] / peak;
  out.low.fill(0);
  out.high.fill(0);
  for (std::size_t i = 0; i < frames; ++i) {
    out.left[i] *= norm;
    out.right[i] *= norm;
  }
  // Removing DC must not undo the one-shot's zero-amplitude boundaries.
  if (!splice) {
    const auto edge = std::min<std::size_t>(
        frames / 4, std::max<std::size_t>(2, std::size_t(rate * .002)));
    for (std::size_t i = 0; i < edge; ++i) {
      const float t = float(i) / float(edge - 1), g = t * t * (3 - 2 * t);
      out.left[frames - 1 - i] *= g;
      out.right[frames - 1 - i] *= g;
    }
    const auto attackEdge = std::min<std::size_t>(
        frames / 4,
        std::max<std::size_t>(
            2,
            std::size_t(rate * std::clamp(attackEdgeMs, .05f, 5.f) * .001f)));
    for (std::size_t i = 0; i < attackEdge; ++i) {
      const float t = float(i) / float(attackEdge - 1), g = t * t * (3 - 2 * t);
      out.left[i] *= g;
      out.right[i] *= g;
    }
  }
  out.low.fill(0);
  out.high.fill(0);
  for (std::size_t i = 0; i < frames; ++i) {
    const auto bin = std::min<std::size_t>(111, i * 112 / frames);
    out.low[bin] = std::min({out.low[bin], out.left[i], out.right[i]});
    out.high[bin] = std::max({out.high[bin], out.left[i], out.right[i]});
  }
  constexpr char hex[] = "0123456789abcde";
  for (std::size_t i = 0; i < 112; ++i) {
    out.waveText[i * 2] =
        hex[std::clamp(int(std::lround(out.low[i] * 7)) + 7, 0, 14)];
    out.waveText[i * 2 + 1] =
        hex[std::clamp(int(std::lround(out.high[i] * 7)) + 7, 0, 14)];
  }
  out.waveText[224] = 0;
}
template <class Cancel, class Progress>
bool renderSpline(const Recipe &r, float rate, Audio &out, Cancel cancel,
                  Progress progress) {
  if (!valid(r) || !std::isfinite(rate) || rate < 8000 || rate > 192000)
    return false;
  const auto frames = std::size_t(std::llround(r.v[length] * rate));
  const auto splice =
      r.v[loop] > .5f
          ? std::min<std::size_t>(frames / 4, std::size_t(rate * .015f))
          : 0;
  const auto total = frames + splice;
  out.left.resize(total);
  out.right.resize(total);
  out.recipe = r;
  float highPitch = 0;
  for (auto p : r.curves[0].p)
    highPitch = std::max(highPitch, p.value);
  const float maxHz = std::min(
      rate * .44f, 440.f * std::exp2((r.v[root] + highPitch + r.v[drift] +
                                      (modActive(r) && r.mod[mDestination] == 1
                                           ? r.mod[mAmount] * 24
                                           : 0) -
                                      69) /
                                     12));
  const int harmonics =
      std::clamp(int(rate * .44f / std::max(1.f, maxHz)), 1, 128);
  std::array<Table, 2> first{}, next{};
  std::uint32_t serial = 0;
  for (int ch = 0; ch < 2; ++ch) {
    const auto seed = ch ? r.seed ^ 0x52474854u : r.seed;
    makeTable(
        first[ch], seed, 0,
        std::clamp(int(std::lround(r.v[points] + animatedCurve(r, 1, 0, 0))), 1,
                   32),
        r.v[curvature], harmonics);
    makeTable(
        next[ch], seed, 1,
        std::clamp(int(std::lround(r.v[points] + animatedCurve(r, 1, 0, 0))), 1,
                   32),
        r.v[curvature], harmonics);
  }
  float levelGain = std::pow(10.f, animatedCurve(r, 3, 0, 0) / 20);
  const float levelSlew = 1 - std::exp(-1.f / (rate * .003f));
  double phase = 0, evolve = 0;
  const double attackFrames = r.v[attack] * rate * .001,
               releaseFrames = r.v[release] * rate * .001;
  const double envelopeScale = std::min(
      1.0, double(frames - 1) / std::max(1.0, attackFrames + releaseFrames));
  for (std::size_t i = 0; i < total; ++i) {
    if ((i & 1023u) == 0) {
      if (cancel())
        return false;
      progress(unsigned(i * 88 / std::max<std::size_t>(1, total)));
    }
    const float t =
        std::min(1.f, float(i % frames) / std::max(1.f, float(frames - 1)));
    const float pitch = animatedCurve(r, 0, t, float(i) / rate);
    const double driftPhase = double(i) / (rate * r.v[driftRate] * .001);
    const auto driftIndex = std::uint32_t(driftPhase);
    float dt = float(driftPhase - driftIndex);
    dt = dt * dt * (3 - 2 * dt);
    const float bend = (random(r.seed ^ 0x44524946u, driftIndex) * (1 - dt) +
                        random(r.seed ^ 0x44524946u, driftIndex + 1) * dt) *
                           2 -
                       1;
    const double hz = std::min(
        double(rate) * .44,
        440.0 * std::exp2((r.v[root] + pitch + bend * r.v[drift] - 69) / 12));
    const float morph = float(evolve * evolve * (3 - 2 * evolve));
    const float amplitude =
        1 -
        r.v[dynamics] * (.25f + .75f * random(r.seed ^ 0x44594e41u, serial));
    // Interpolate random amplitude with the wavetable transition as well.
    const float nextAmplitude =
        1 - r.v[dynamics] *
                (.25f + .75f * random(r.seed ^ 0x44594e41u, serial + 1));
    levelGain += (std::pow(10.f, animatedCurve(r, 3, t, float(i) / rate) / 20) -
                  levelGain) *
                 levelSlew;
    float gain = (amplitude + (nextAmplitude - amplitude) * morph) * levelGain;
    if (!splice) {
      const double a = attackFrames * envelopeScale,
                   b = releaseFrames * envelopeScale;
      const float edge =
          float(std::min({1.0, double(i) / std::max(1.0, a),
                          double(frames - 1 - i) / std::max(1.0, b)}));
      gain *= edge * edge * (3 - 2 * edge);
    }
    const float l = (lookup(first[0], phase) * (1 - morph) +
                     lookup(next[0], phase) * morph) *
                    gain;
    const float rr = (lookup(first[1], phase) * (1 - morph) +
                      lookup(next[1], phase) * morph) *
                     gain;
    out.left[i] = l;
    out.right[i] = l + (rr - l) * r.v[width];
    phase += hz / rate;
    if (phase >= 1)
      phase -= std::floor(phase);
    evolve += hz / (rate * std::clamp(double(r.v[evolution]) *
                                          std::exp2(double(animatedCurve(
                                              r, 2, t, float(i) / rate))),
                                      1.0, 65536.0));
    if (evolve >= 1) {
      evolve -= 1;
      ++serial;
      first = next;
      const int pts =
          std::clamp(int(std::lround(r.v[points] +
                                     animatedCurve(r, 1, t, float(i) / rate))),
                     1, 32);
      for (int ch = 0; ch < 2; ++ch)
        makeTable(next[ch], ch ? r.seed ^ 0x52474854u : r.seed, serial + 1, pts,
                  r.v[curvature], harmonics);
    }
  }
  finishSound(r, rate, out, frames, splice);
  progress(100);
  return !cancel();
}
} // namespace s3g_thru_synth
#include "thru_synth_generators.h"
