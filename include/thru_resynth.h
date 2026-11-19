#pragma once
#include <complex>
#include <limits>
namespace s3g_thru_synth {
// Worker-only, streaming STFT: keep three analysis windows rather than an
// entire spectral file.
struct ResynthPlan {
  static constexpr int size = 2048, hop = 512, bins = size / 2 + 1;
  using Complex = std::complex<float>;
  using Block = std::array<Complex, size>;
  std::array<int, size> reverse{};
  std::array<Complex, size / 2> roots{};
  std::array<float, size> window{};
  ResynthPlan() {
    for (int i = 0; i < size; ++i) {
      int v = i, r = 0;
      for (int j = 0; j < 11; ++j) {
        r = (r << 1) | (v & 1);
        v >>= 1;
      }
      reverse[i] = r;
      window[i] = .5f - .5f * std::cos(Plan::tau * i / size);
    }
    for (int i = 0; i < size / 2; ++i)
      roots[i] = std::polar(1.f, -Plan::tau * i / size);
  }
  static const ResynthPlan &get() {
    static const ResynthPlan p;
    return p;
  }
  void fft(Block &a, bool inverse = false) const {
    for (int i = 0; i < size; ++i)
      if (reverse[i] > i)
        std::swap(a[i], a[reverse[i]]);
    for (int width = 2; width <= size; width *= 2)
      for (int base = 0; base < size; base += width)
        for (int i = 0; i < width / 2; ++i) {
          const auto w = inverse ? std::conj(roots[i * size / width])
                                 : roots[i * size / width];
          const auto x = a[base + i], y = a[base + i + width / 2] * w;
          a[base + i] = x + y;
          a[base + i + width / 2] = x - y;
        }
    if (inverse)
      for (auto &v : a)
        v /= size;
  }
};
inline float wrapPhase(float v) { return std::remainder(v, Plan::tau); }
struct AnalysisWindow {
  int index = std::numeric_limits<int>::min();
  std::array<float, ResynthPlan::bins> left{}, right{}, phaseLeft{},
      phaseRight{};
};
struct ResynthAnalysis {
  const SourceAudio &source;
  std::size_t start, end;
  std::array<AnalysisWindow, 3> cache{};
  AnalysisWindow &frame(int index) {
    auto &out = cache[std::size_t((index % 3 + 3) % 3)];
    if (out.index == index)
      return out;
    out.index = index;
    const auto &p = ResynthPlan::get();
    ResynthPlan::Block left{}, right{};
    for (int i = 0; i < ResynthPlan::size; ++i) {
      const auto at = std::int64_t(start) +
                      std::int64_t(index) * ResynthPlan::hop + i -
                      ResynthPlan::size / 2;
      if (at >= std::int64_t(start) && at < std::int64_t(end)) {
        left[i] = source.left[std::size_t(at)] * p.window[i];
        right[i] = source.right[std::size_t(at)] * p.window[i];
      }
    }
    p.fft(left);
    p.fft(right);
    for (int k = 0; k < ResynthPlan::bins; ++k) {
      out.left[k] = std::abs(left[k]);
      out.right[k] = std::abs(right[k]);
      out.phaseLeft[k] = std::arg(left[k]);
      out.phaseRight[k] = std::arg(right[k]);
    }
    return out;
  }
};
struct SineTrack {
  float sourceHz = 0, hz = 0, nextHz = 0, left = 0, right = 0, nextLeft = 0,
        nextRight = 0, difference = 0, nextDifference = 0;
  double phase = 0;
  bool assigned = false;
  float admission = 1;
};
} // namespace s3g_thru_synth
#include "thru_synth_spectral_workshop.h"
namespace s3g_thru_synth {
template <class Cancel, class Progress>
bool renderResynth(const Recipe &r, float rate, Audio &out,
                   const SourceAudio &source, Cancel cancel,
                   Progress progress) {
  if (r.spectral[rsMode] == 1 && r.trace[tMode] > 0)
    return renderTraceAtoms(r, rate, out, source, cancel, progress);
  using P = ResynthPlan;
  constexpr int n = P::size, hop = P::hop, bins = P::bins;
  if (source.left.empty() || source.left.size() != source.right.size() ||
      std::abs(source.rate - rate) > .5f)
    return false;
  const auto frames = std::size_t(std::llround(r.v[length] * rate)),
             splice = r.v[loop] > .5f
                          ? std::min<std::size_t>(frames / 4,
                                                  std::size_t(rate * .015f))
                          : 0,
             total = frames + splice;
  out.left.assign(total, 0);
  out.right.assign(total, 0);
  out.recipe = r;
  const auto start =
      std::min(source.left.size() - 1,
               std::size_t(source.left.size() * r.spectral[rsStart]));
  const auto end =
      std::clamp(std::size_t(source.left.size() * r.spectral[rsEnd]), start + 1,
                 source.left.size());
  ResynthAnalysis analysis{source, start, end, {}};
  const auto &plan = P::get();
  std::array<float, bins> smoothedL{}, smoothedR{}, phase{},
      previousDifference{};
  std::array<bool, bins> phaseValid{}, rightReference{};
  std::vector<float> normalization(total, 0);
  std::array<SineTrack, 64> tracks{};
  const auto outputWindows = (total + n / 2 + hop - 1) / hop;
  for (std::size_t window = 0; window < outputWindows; ++window) {
    if (cancel())
      return false;
    progress(unsigned(window * 85 / outputWindows));
    const auto center = window * hop;
    const float t = std::min(1.f,
                             float(center) / std::max(1.f, float(frames - 1))),
                seconds = float(center) / rate;
    const double position = double(center) * (end - start) / frames / hop;
    const int index = int(position);
    const float fraction = float(position - index);
    const auto &before = analysis.frame(index - 1);
    const auto &a = analysis.frame(index);
    const auto &b = analysis.frame(index + 1);
    std::array<float, bins> magL{}, magR{}, inputL{}, inputR{}, freqL{},
        freqR{}, blurL{}, blurR{};
    for (int k = 1; k < bins - 1; ++k) {
      magL[k] = a.left[k] + (b.left[k] - a.left[k]) * fraction;
      magR[k] = a.right[k] + (b.right[k] - a.right[k]) * fraction;
      inputL[k] = a.phaseLeft[k] +
                  wrapPhase(b.phaseLeft[k] - a.phaseLeft[k]) * fraction;
      inputR[k] = a.phaseRight[k] +
                  wrapPhase(b.phaseRight[k] - a.phaseRight[k]) * fraction;
      const float expected = Plan::tau * k * hop / n, base = Plan::tau * k / n;
      const float fl0 = base + wrapPhase(a.phaseLeft[k] - before.phaseLeft[k] -
                                         expected) /
                                   hop,
                  fl1 = base +
                        wrapPhase(b.phaseLeft[k] - a.phaseLeft[k] - expected) /
                            hop;
      const float fr0 = base + wrapPhase(a.phaseRight[k] -
                                         before.phaseRight[k] - expected) /
                                   hop,
                  fr1 = base + wrapPhase(b.phaseRight[k] - a.phaseRight[k] -
                                         expected) /
                                   hop;
      freqL[k] = fl0 + (fl1 - fl0) * fraction;
      freqR[k] = fr0 + (fr1 - fr0) * fraction;
    }
    const float blur = std::clamp(r.spectral[rsBlur] +
                                      animatedCurve(r, 1, t, seconds),
                                  0.f, 1.f),
                smooth = std::clamp(r.spectral[rsSmooth] +
                                        animatedCurve(r, 2, t, seconds),
                                    0.f, 1.f);
    const int radius = int(std::round(blur * 16));
    const float alpha =
        smooth > 0 ? 1 - std::exp(-hop / (rate * smooth * .25f)) : 1;
    float maximum = 0;
    for (int k = 1; k < bins - 1; ++k) {
      float left = 0, right = 0, weights = 0;
      for (int j = -radius; j <= radius; ++j) {
        const int at = std::clamp(k + j, 1, bins - 2);
        const float w = float(radius + 1 - std::abs(j));
        left += magL[at] * w;
        right += magR[at] * w;
        weights += w;
      }
      smoothedL[k] += (left / weights - smoothedL[k]) * alpha;
      smoothedR[k] += (right / weights - smoothedR[k]) * alpha;
      blurL[k] = smoothedL[k];
      blurR[k] = smoothedR[k];
      maximum = std::max({maximum, blurL[k], blurR[k]});
    }
    const float floor = maximum * std::pow(10.f, r.spectral[rsFloor] / 20),
                pitch = std::exp2(
                    (r.spectral[rsPitch] + animatedCurve(r, 0, t, seconds)) /
                    12);
    if (r.spectral[rsMode] == 0) {
      std::array<float, bins> left{}, right{}, fLeft{}, fRight{}, weights{};
      std::array<P::Complex, bins> initialL{}, initialR{}, differences{};
      for (int k = 1; k < bins - 1; ++k) {
        if (std::max(blurL[k], blurR[k]) < floor)
          continue;
        const int at = int(std::round(k * pitch));
        if (at < 1 || at >= bins - 1)
          continue;
        const float weight = std::max(blurL[k], blurR[k]);
        left[at] += blurL[k];
        right[at] += blurR[k];
        weights[at] += weight;
        fLeft[at] += std::max(0.f, freqL[k]) * pitch * weight;
        fRight[at] += std::max(0.f, freqR[k]) * pitch * weight;
        initialL[at] += std::polar(blurL[k], inputL[k]);
        initialR[at] += std::polar(blurR[k], inputR[k]);
        differences[at] += std::polar(weight, wrapPhase(inputR[k] - inputL[k]));
      }
      P::Block specL{}, specR{};
      for (int k = 1; k < bins - 1; ++k) {
        if (weights[k] <= 1e-12f) {
          phaseValid[k] = false;
          continue;
        }
        const float difference = std::arg(differences[k]);
        const bool ref = right[k] > left[k] * 2;
        if (!phaseValid[k]) {
          phase[k] = std::arg(ref ? initialR[k] : initialL[k]);
          phaseValid[k] = true;
        } else {
          if (ref != rightReference[k])
            phase[k] += ref ? difference : -difference;
          phase[k] += std::clamp((ref ? fRight[k] : fLeft[k]) / weights[k], 0.f,
                                 3.14159265359f) *
                      hop;
          phase[k] = wrapPhase(phase[k]);
        }
        rightReference[k] = ref;
        previousDifference[k] = difference;
        const float pl = phase[k] - (ref ? difference : 0),
                    pr = phase[k] + (ref ? 0 : difference);
        specL[k] = std::polar(left[k], pl);
        specR[k] = std::polar(right[k], pr);
        specL[n - k] = std::conj(specL[k]);
        specR[n - k] = std::conj(specR[k]);
      }
      plan.fft(specL, true);
      plan.fft(specR, true);
      for (int i = 0; i < n; ++i) {
        const auto at = std::int64_t(center) + i - n / 2;
        if (at < 0 || at >= std::int64_t(total))
          continue;
        const float w = plan.window[i];
        out.left[std::size_t(at)] += specL[i].real() * w;
        out.right[std::size_t(at)] += specR[i].real() * w;
        normalization[std::size_t(at)] += w * w;
      }
    } else if (center < total) {
      struct Peak {
        int bin;
        float magnitude;
      };
      std::array<Peak, bins> peaks{};
      int count = 0;
      for (int k = 1; k < bins - 1; ++k) {
        const float v = std::max(blurL[k], blurR[k]);
        if (v > floor && v > std::max(blurL[k - 1], blurR[k - 1]) &&
            v >= std::max(blurL[k + 1], blurR[k + 1]))
          peaks[count++] = {k, v};
      }
      const int chosen = std::min(count, int(r.spectral[rsPartials]));
      std::partial_sort(
          peaks.begin(), peaks.begin() + chosen, peaks.begin() + count,
          [](Peak a, Peak b) { return a.magnitude > b.magnitude; });
      for (auto &track : tracks) {
        track.assigned = false;
        track.nextLeft = track.nextRight = 0;
        track.nextHz = track.hz;
      }
      for (int i = 0; i < chosen; ++i) {
        const int k = peaks[i].bin;
        const float freq =
                        std::clamp((blurR[k] > blurL[k] ? freqR[k] : freqL[k]) *
                                       rate / Plan::tau,
                                   (k - .6f) * rate / n, (k + .6f) * rate / n),
                    hz = freq * pitch;
        if (hz >= rate * .44f)
          continue;
        int selected = -1;
        float distance =
            r.trace[tTolerance] == 144 ? .12f : r.trace[tTolerance] / 1200;
        for (int j = 0; j < 64; ++j)
          if (!tracks[j].assigned && tracks[j].sourceHz > 0) {
            const float d = std::abs(std::log2(freq / tracks[j].sourceHz));
            if (d < distance) {
              distance = d;
              selected = j;
            }
          }
        bool fresh = false;
        if (selected < 0) {
          for (int j = 0; j < 64; ++j)
            if (!tracks[j].assigned &&
                tracks[j].left + tracks[j].right < 1e-8f) {
              selected = j;
              fresh = true;
              break;
            }
        }
        if (selected < 0)
          continue;
        auto &track = tracks[selected];
        track.assigned = true;
        track.sourceHz = freq;
        track.nextHz = hz;
        if (r.trace[tDrift] > 0)
          track.nextHz *=
              std::exp2(r.trace[tDrift] *
                        workshopRandom(hash(r.seed ^ std::uint32_t(selected)),
                                       seconds * .7f) *
                        .12f);
        if (fresh)
          track.admission =
              random(hash(r.seed ^ std::uint32_t(selected * 97 + k)), 1) <
                      r.trace[tDensity]
                  ? 1.f
                  : 0.f;
        track.nextLeft = blurL[k] * 4 / n;
        track.nextRight = blurR[k] * 4 / n;
        track.nextLeft *= track.admission;
        track.nextRight *= track.admission;
        if (r.trace[tSpread] > 0) {
          const float pan =
              (random(r.seed, std::uint32_t(selected + 503)) * 2 - 1) *
              r.trace[tSpread] * r.v[width];
          track.nextLeft *= std::sqrt(1 - pan);
          track.nextRight *= std::sqrt(1 + pan);
        }
        const float difference = wrapPhase(inputR[k] - inputL[k]);
        if (fresh) {
          track.hz = hz;
          track.phase = inputL[k];
          track.difference = difference;
        }
        track.nextDifference =
            track.difference + wrapPhase(difference - track.difference);
      }
      const auto block = std::min<std::size_t>(hop, total - center);
      for (std::size_t sample = 0; sample < block; ++sample) {
        const float u = float(sample + 1) / hop, s = u * u * (3 - 2 * u);
        float left = 0, right = 0;
        for (auto &track : tracks) {
          if (track.left + track.right + track.nextLeft + track.nextRight <
              1e-10f)
            continue;
          const float hz = track.hz + (track.nextHz - track.hz) * u,
                      difference =
                          track.difference +
                          (track.nextDifference - track.difference) * s;
          left += std::sin(track.phase) *
                  (track.left + (track.nextLeft - track.left) * s);
          right += std::sin(track.phase + difference) *
                   (track.right + (track.nextRight - track.right) * s);
          track.phase += Plan::tau * hz / rate;
          if (track.phase >= Plan::tau)
            track.phase -= Plan::tau;
        }
        out.left[center + sample] = left;
        out.right[center + sample] = right;
      }
      for (auto &track : tracks) {
        track.left = track.nextLeft;
        track.right = track.nextRight;
        track.hz = track.nextHz;
        track.difference = wrapPhase(track.nextDifference);
        if (track.left + track.right < 1e-10f)
          track.sourceHz = 0;
      }
    }
  }
  float level = std::pow(10.f, animatedCurve(r, 3, 0, 0) / 20),
        slew = 1 - std::exp(-1 / (rate * .003f));
  for (std::size_t i = 0; i < total; ++i) {
    if ((i & 1023u) == 0 && cancel())
      return false;
    if (r.spectral[rsMode] == 0 && normalization[i] > 1e-8f) {
      out.left[i] /= normalization[i];
      out.right[i] /= normalization[i];
    }
    const float t =
        std::min(1.f, float(i % frames) / std::max(1.f, float(frames - 1)));
    level +=
        (std::pow(10.f, animatedCurve(r, 3, t, float(i) / rate) / 20) - level) *
        slew;
    const float mid = (out.left[i] + out.right[i]) * .5f,
                g = level * fileEnvelope(r, i, frames, splice, rate);
    out.left[i] = (mid + (out.left[i] - mid) * r.v[width]) * g;
    out.right[i] = (mid + (out.right[i] - mid) * r.v[width]) * g;
  }
  finishSound(r, rate, out, frames, splice);
  progress(100);
  return !cancel();
}
} // namespace s3g_thru_synth
