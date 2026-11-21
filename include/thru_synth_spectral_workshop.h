#pragma once
namespace s3g_thru_synth {
struct WorkshopSpan {
  std::size_t frames, splice, total;
  WorkshopSpan(const Recipe &r, float rate, Audio &out)
      : frames(std::size_t(std::llround(r.v[length] * rate))),
        splice(r.v[loop] > .5f ? std::min(frames / 4, std::size_t(rate * .015f))
                               : 0),
        total(frames + splice) {
    out.left.assign(total, 0);
    out.right.assign(total, 0);
    out.recipe = r;
  }
};
inline float workshopRandom(std::uint32_t seed, float position) {
  const auto i = std::uint32_t(std::max(0.f, std::floor(position)));
  float t = position - float(i);
  t = t * t * (3 - 2 * t);
  return ((1 - t) * random(seed, i) + t * random(seed, i + 1)) * 2 - 1;
}
template <class Cancel>
bool finishWorkshop(const Recipe &r, float rate, Audio &out,
                    const WorkshopSpan &span, Cancel cancel,
                    bool stereoWidth = false) {
  float gain = 1;
  const float slew = 1 - std::exp(-1 / (rate * .003f));
  for (std::size_t i = 0; i < span.total; ++i) {
    if ((i & 1023u) == 0 && cancel())
      return false;
    const float u =
        float(i % span.frames) / std::max(1.f, float(span.frames - 1));
    const float target =
        std::pow(10.f, animatedCurve(r, 3, u, float(i) / rate) / 20);
    if (i == 0)
      gain = target;
    gain += (target - gain) * slew;
    if (stereoWidth) {
      const float mid = .5f * (out.left[i] + out.right[i]);
      out.left[i] = mid + (out.left[i] - mid) * r.v[width];
      out.right[i] = mid + (out.right[i] - mid) * r.v[width];
    }
    const float level =
        gain * fileEnvelope(r, i, span.frames, span.splice, rate);
    out.left[i] *= level;
    out.right[i] *= level;
  }
  finishSound(r, rate, out, span.frames, span.splice,
              isDrumType(r.type) ? r.drumExtra[xEdge] : 2.f);
  return !cancel();
}
struct WorkshopPeak {
  float hz = 0, left = 0, right = 0, difference = 0;
};
struct WorkshopPeaks {
  std::array<WorkshopPeak, 64> p{};
  int count = 0;
};
inline WorkshopPeaks workshopPeaks(ResynthAnalysis &analysis, float u,
                                   float rate, int wanted, float floorDb,
                                   float blur = 0, float smooth = 0) {
  using P = ResynthPlan;
  WorkshopPeaks out;
  const int frame = std::max(
      0, int(std::lround(u * float(analysis.end - analysis.start) / P::hop)));
  // The analysis owns three ring slots. Acquire the previous/current/next
  // frames together, then copy all peak information before another query.
  const auto &before = analysis.frame(frame - 1);
  const auto &a = analysis.frame(frame);
  const auto &after = analysis.frame(frame + 1);
  std::array<float, P::bins> left{}, right{}, magnitude{};
  float peak = 0;
  const int radius = int(blur * 8);
  for (int k = 1; k < P::bins - 1; ++k) {
    float l = 0, rr = 0;
    int count = 0;
    for (int j = std::max(1, k - radius);
         j <= std::min(P::bins - 2, k + radius); ++j) {
      l += a.left[j] * (1 - smooth * .5f) +
           (before.left[j] + after.left[j]) * smooth * .25f;
      rr += a.right[j] * (1 - smooth * .5f) +
            (before.right[j] + after.right[j]) * smooth * .25f;
      ++count;
    }
    left[k] = l / count;
    right[k] = rr / count;
    magnitude[k] = std::max(left[k], right[k]);
    peak = std::max(peak, magnitude[k]);
  }
  const float floor = std::max(1e-7f, peak * std::pow(10.f, floorDb / 20));
  struct Bin {
    int k;
    float value;
  };
  std::array<Bin, P::bins> bins{};
  int count = 0;
  for (int k = 1; k < P::bins - 1; ++k)
    if (magnitude[k] > floor && magnitude[k] > magnitude[k - 1] &&
        magnitude[k] >= magnitude[k + 1])
      bins[count++] = {k, magnitude[k]};
  out.count = std::min({count, wanted, 64});
  std::partial_sort(bins.begin(), bins.begin() + out.count,
                    bins.begin() + count,
                    [](Bin x, Bin y) { return x.value > y.value; });
  for (int i = 0; i < out.count; ++i) {
    const int k = bins[i].k;
    const bool rr = right[k] > left[k];
    const float expected = Plan::tau * k * P::hop / P::size;
    const float phase = rr ? a.phaseRight[k] : a.phaseLeft[k],
                old = rr ? before.phaseRight[k] : before.phaseLeft[k];
    const float hz =
        (Plan::tau * k / P::size + wrapPhase(phase - old - expected) / P::hop) *
        rate / Plan::tau;
    out.p[i] = {
        std::clamp(hz, (k - .6f) * rate / P::size, (k + .6f) * rate / P::size),
        left[k] * 4 / P::size, right[k] * 4 / P::size,
        wrapPhase(a.phaseRight[k] - a.phaseLeft[k])};
  }
  return out;
}
template <class Cancel>
bool addWorkshopAtom(Audio &out, float rate, std::int64_t start,
                     std::size_t length, WorkshopPeak peak, float pitch,
                     float drift, float texture, float pan, std::uint32_t seed,
                     Cancel cancel) {
  length = std::max<std::size_t>(8, length);
  double phase = random(seed, 1) * Plan::tau;
  float residual = 0;
  const float gl = std::sqrt(std::max(0.f, 1 - pan)),
              gr = std::sqrt(std::max(0.f, 1 + pan));
  const float frequency = peak.hz * std::exp2(pitch / 12);
  if (frequency < 12 || frequency >= rate * .44f)
    return true;
  for (std::size_t i = 0; i < length; ++i) {
    if ((i & 511u) == 0 && cancel())
      return false;
    const auto at = start + std::int64_t(i);
    if (at >= std::int64_t(out.left.size()))
      break;
    const float t = float(i) / std::max(1.f, float(length - 1)),
                window = std::sin(t * 3.14159265359f), env = window * window;
    const float detune =
        1 + drift * .025f * workshopRandom(seed ^ 0x44524654u, t * 3);
    residual +=
        (random(seed ^ 0x4e4f4953u, std::uint32_t(i)) * 2 - 1 - residual) *
        .12f;
    const float l =
        (std::sin(phase) * (1 - .72f * texture) + residual * texture * .58f) *
        env;
    const float rr = (std::sin(phase + peak.difference) * (1 - .72f * texture) +
                      residual * texture * .58f) *
                     env;
    if (at >= 0) {
      out.left[std::size_t(at)] += l * peak.left * gl;
      out.right[std::size_t(at)] += rr * peak.right * gr;
    }
    phase +=
        Plan::tau * std::clamp(frequency * detune, 12.f, rate * .44f) / rate;
    if (phase >= Plan::tau)
      phase -= Plan::tau;
  }
  return true;
}
template <class Cancel, class Progress>
bool renderTraceAtoms(const Recipe &r, float rate, Audio &out,
                      const SourceAudio &source, Cancel cancel,
                      Progress progress) {
  WorkshopSpan span(r, rate, out);
  if (source.left.empty() || source.left.size() != source.right.size() ||
      std::abs(source.rate - rate) > .5f)
    return false;
  const auto begin =
      std::min(source.left.size() - 1,
               std::size_t(source.left.size() * r.spectral[rsStart]));
  const auto end =
      std::clamp(std::size_t(source.left.size() * r.spectral[rsEnd]), begin + 1,
                 source.left.size());
  ResynthAnalysis analysis{source, begin, end, {}};
  const int mode = int(r.trace[tMode]);
  const auto windows = (span.total + ResynthPlan::hop - 1) / ResynthPlan::hop;
  const auto stride =
      mode == 3 ? std::max<std::size_t>(1, std::size_t(r.trace[tLife] * .001f *
                                                       rate / ResynthPlan::hop))
                : 1;
  const auto life = std::size_t(rate * r.trace[tLife] * .001f *
                                (mode == 2 ? 2.8f : mode == 3 ? 7.5f : 1.f));
  for (std::size_t w = 0; w < windows; w += stride) {
    if (cancel())
      return false;
    progress(unsigned(w * 88 / windows));
    const float u = float(w) / std::max(1.f, float(windows - 1)),
                seconds = float(w * ResynthPlan::hop) / rate;
    const float blur = std::clamp(r.spectral[rsBlur] +
                                      animatedCurve(r, 1, u, seconds),
                                  0.f, 1.f),
                smooth = std::clamp(r.spectral[rsSmooth] +
                                        animatedCurve(r, 2, u, seconds),
                                    0.f, 1.f);
    const auto peaks = workshopPeaks(analysis, mode == 3 ? r.trace[tFreeze] : u,
                                     rate, int(r.spectral[rsPartials]),
                                     r.spectral[rsFloor], blur, smooth);
    for (int i = 0; i < peaks.count; ++i) {
      const auto seed = hash(r.seed ^ std::uint32_t(w * 67 + i + 1));
      if (random(seed, 2) > r.trace[tDensity])
        continue;
      const auto jitter = std::int64_t((random(seed, 3) * 2 - 1) *
                                       r.trace[tJitter] * ResynthPlan::hop);
      const float pan = (random(seed, 4) * 2 - 1) * r.trace[tSpread];
      if (!addWorkshopAtom(
              out, rate, std::int64_t(w * ResynthPlan::hop) + jitter, life,
              peaks.p[i], r.spectral[rsPitch] + animatedCurve(r, 0, u, seconds),
              r.trace[tDrift], 0, pan, seed, cancel))
        return false;
    }
  }
  if (!finishWorkshop(r, rate, out, span, cancel, true))
    return false;
  progress(100);
  return true;
}
using HybridInputs = std::array<std::shared_ptr<const SourceAudio>, 4>;
template <class Cancel, class Progress>
bool renderLegacyHybrid(const Recipe &r, float rate, Audio &out,
                  const HybridInputs &inputs, Cancel cancel,
                  Progress progress) {
  const int count = int(r.hybridParams[hSources]);
  for (int i = 0; i < count; ++i)
    if (!inputs[i] || inputs[i]->left.empty() ||
        inputs[i]->left.size() != inputs[i]->right.size() ||
        std::abs(inputs[i]->rate - rate) > .5f)
      return false;
  WorkshopSpan span(r, rate, out);
  std::array<std::unique_ptr<ResynthAnalysis>, 4> analysis;
  for (int i = 0; i < count; ++i)
    analysis[i] = std::make_unique<ResynthAnalysis>(
        ResynthAnalysis{*inputs[i], 0, inputs[i]->left.size(), {}});
  const auto windows = (span.total + ResynthPlan::hop - 1) / ResynthPlan::hop;
  const int mode = int(r.hybridParams[hMode]);
  for (std::size_t w = 0; w < windows; ++w) {
    if (cancel())
      return false;
    progress(unsigned(w * 88 / windows));
    const float u = float(w) / std::max(1.f, float(windows - 1)),
                seconds = float(w * ResynthPlan::hop) / rate;
    std::array<WorkshopPeaks, 4> peaks;
    for (int i = 0; i < count; ++i)
      peaks[i] = workshopPeaks(*analysis[i], u, rate, 8, -66);
    const auto seed = hash(r.seed ^ std::uint32_t(w + 0x48594252u));
    const float mutation = std::clamp(
        r.hybridParams[hMutation] + animatedCurve(r, 2, u, seconds), 0.f, 1.f);
    const int timeSource = mode == 1 || mode == 2
                               ? 0
                               : mode == 3 ? (random(seed, 1) < mutation
                                                  ? int(random(seed, 2) * count)
                                                  : 0)
                                           : int(w % std::size_t(count));
    const auto &time = peaks[timeSource];
    for (int i = 0; i < time.count; ++i) {
      const auto atomSeed = hash(seed ^ std::uint32_t(i + 1));
      if (random(atomSeed, 1) >
          std::clamp(r.hybridParams[hDensity] + animatedCurve(r, 1, u, seconds),
                     0.f, 1.f))
        continue;
      const int ps = mode == 1 || mode == 2
                         ? 1
                         : mode == 4 ? (timeSource + 1) % count
                                     : random(atomSeed, 2) < mutation
                                           ? int(random(atomSeed, 3) * count)
                                           : timeSource;
      const int as = mode == 2 ? 0
                               : random(atomSeed, 4) < mutation
                                     ? int(random(atomSeed, 5) * count)
                                     : timeSource;
      WorkshopPeak p = time.p[i];
      if (mode == 2) {
        bool match = false;
        for (int k = 0; k < peaks[1].count; ++k)
          if (std::abs(1200 *
                       std::log2(peaks[1].p[k].hz / std::max(1.f, p.hz))) <
              30 + mutation * 270)
            match = true;
        if (!match)
          continue;
      }
      if (peaks[ps].count)
        p.hz = peaks[ps].p[std::min(i, peaks[ps].count - 1)].hz;
      if (peaks[as].count) {
        const auto &a = peaks[as].p[std::min(i, peaks[as].count - 1)];
        p.left = .35f * p.left + .65f * a.left;
        p.right = .35f * p.right + .65f * a.right;
      }
      const float texture = r.hybridParams[hTexture];
      const auto life = std::size_t(rate * r.hybridParams[hLife] * .001f *
                                    (1 - .58f * texture));
      const float pan = (random(atomSeed, 7) * 2 - 1);
      if (!addWorkshopAtom(
              out, rate, std::int64_t(w * ResynthPlan::hop), life, p,
              r.hybridParams[hPitch] + animatedCurve(r, 0, u, seconds),
              r.hybridParams[hDrift], texture, pan, atomSeed, cancel))
        return false;
    }
  }
  if (!finishWorkshop(r, rate, out, span, cancel, true))
    return false;
  progress(100);
  return true;
}
} // namespace s3g_thru_synth

#include "thru_synth_hybrid.h"
namespace s3g_thru_synth {
template<class Cancel,class Progress>
bool renderHybrid(const Recipe& r,float rate,Audio& out,const HybridInputs& inputs,Cancel cancel,Progress progress){
 return r.hybridStyle?renderHybridTransfer(r,rate,out,inputs,cancel,progress):renderLegacyHybrid(r,rate,out,inputs,cancel,progress);
}
}
