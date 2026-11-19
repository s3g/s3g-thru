#pragma once
#include <complex>
namespace s3g_thru_synth {
using ConvolutionComplex = std::complex<float>;
template <class Cancel, class Progress>
bool convolutionFft(std::vector<ConvolutionComplex> &a, bool inverse,
                    Cancel cancel, Progress progress) {
  const auto n = a.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
    if ((i & 4095u) == 0 && cancel())
      return false;
  }
  for (std::size_t width = 2; width <= n; width *= 2) {
    const auto step =
        std::polar(1.0, (inverse ? 1 : -1) * 6.283185307179586 / double(width));
    for (std::size_t base = 0; base < n; base += width) {
      std::complex<double> w = 1;
      for (std::size_t j = 0; j < width / 2; ++j) {
        if ((j & 4095u) == 0 && cancel())
          return false;
        const auto x = a[base + j],
                   y = a[base + j + width / 2] *
                       ConvolutionComplex(float(w.real()), float(w.imag()));
        a[base + j] = x + y;
        a[base + j + width / 2] = x - y;
        w *= step;
      }
    }
    progress();
  }
  if (inverse)
    for (auto &v : a)
      v /= float(n);
  return !cancel();
}
template <class Cancel, class Progress>
bool convolveStereo(const SourceAudio &source, const SourceAudio &impulse,
                    std::size_t maximum, bool tail, std::vector<float> &left,
                    std::vector<float> &right, Cancel cancel,
                    Progress progress) {
  if (source.left.empty() || impulse.left.empty() ||
      source.left.size() != source.right.size() ||
      impulse.left.size() != impulse.right.size() || !maximum)
    return false;
  const auto natural = source.left.size() + impulse.left.size() - 1,
             count = std::min(maximum, tail ? natural : source.left.size()),
             srcCount = std::min(count, source.left.size()),
             irCount = std::min(count, impulse.left.size());
  std::size_t n = 1, stages = 0;
  while (n < srcCount + irCount - 1) {
    n *= 2;
    ++stages;
  }
  if (n > (1u << 24))
    return false;
  std::vector<ConvolutionComplex> a(n), b(n);
  left.assign(count, 0);
  right.assign(count, 0);
  unsigned done = 0;
  const auto advance = [&] {
    progress(unsigned(5 + 75 * ++done / std::max<std::size_t>(1, 6 * stages)));
  };
  for (int channel = 0; channel < 2; ++channel) {
    std::fill(a.begin(), a.end(), ConvolutionComplex{});
    std::fill(b.begin(), b.end(), ConvolutionComplex{});
    const auto &src = channel ? source.right : source.left;
    const auto &ir = channel ? impulse.right : impulse.left;
    for (std::size_t i = 0; i < srcCount; ++i)
      a[i] = src[i];
    for (std::size_t i = 0; i < irCount; ++i)
      b[i] = ir[i];
    if (!convolutionFft(a, false, cancel, advance) ||
        !convolutionFft(b, false, cancel, advance))
      return false;
    for (std::size_t i = 0; i < n; ++i) {
      a[i] *= b[i];
      if ((i & 4095u) == 0 && cancel())
        return false;
    }
    if (!convolutionFft(a, true, cancel, advance))
      return false;
    auto &out = channel ? right : left;
    for (std::size_t i = 0; i < count; ++i)
      out[i] = a[i].real();
  }
  return !cancel();
}
inline SourceAudio convolutionRegion(const SourceAudio &in, float start,
                                     float ms, float rate) {
  SourceAudio out;
  out.rate = rate;
  const auto first = std::min(in.left.size() - 1,
                              std::size_t(in.left.size() * start)),
             count =
                 std::min(in.left.size() - first,
                          std::max<std::size_t>(
                              1, std::size_t(std::llround(ms * rate * .001))));
  out.left.assign(in.left.begin() + first, in.left.begin() + first + count);
  out.right.assign(in.right.begin() + first, in.right.begin() + first + count);
  return out;
}
template <class Cancel, class Progress>
bool renderConvolution(const Recipe &r, float rate, Audio &out,
                       const SourceAudio &primary, const SourceAudio &impulse,
                       Cancel cancel, Progress progress) {
  if (primary.left.empty() || impulse.left.empty() ||
      primary.right.size() != primary.left.size() ||
      impulse.right.size() != impulse.left.size() ||
      std::abs(primary.rate - rate) > .5f ||
      std::abs(impulse.rate - rate) > .5f)
    return false;
  auto source =
           convolutionRegion(primary, r.conv[cStart], r.conv[cLength], rate),
       ir = convolutionRegion(impulse, r.conv[cIrStart], r.conv[cIrLength],
                              rate);
  for (auto *channel : {&ir.left, &ir.right}) {
    float previous = 0, low = 0;
    for (std::size_t i = 0; i < channel->size(); ++i) {
      if ((i & 1023u) == 0 && cancel())
        return false;
      const float t = float(i) / std::max(1.f, float(channel->size() - 1)),
                  x = (*channel)[i],
                  window = channel->size() < 3
                               ? 1
                               : r.conv[cWindow] == 1
                                     ? 1 - std::abs(2 * t - 1)
                                     : r.conv[cWindow] == 2
                                           ? .5f - .5f * std::cos(Plan::tau * t)
                                           : r.conv[cWindow] == 3
                                                 ? std::exp(-6.907755f * t)
                                                 : 1;
      low += (x - low) * (1 - std::exp(-Plan::tau * 1800 / rate));
      const float tone = r.conv[cTone],
                  v = tone >= 0 ? x * (1 - tone) + (x - previous) * 4 * tone
                                : x + (low - x) * (-tone);
      previous = x;
      (*channel)[i] = v * window;
    }
  }
  const auto cap = std::size_t(std::llround(r.v[length] * rate));
  if (!convolveStereo(source, ir, cap, r.conv[cTail] > .5f, out.left, out.right,
                      cancel, progress))
    return false;
  const auto frames = out.left.size();
  out.recipe = r;
  const auto seam =
      r.v[loop] > .5f
          ? std::min<std::size_t>(frames / 4, std::size_t(rate * .015f))
          : 0;
  float sourcePeak = 0, wetPeak = 0;
  for (std::size_t i = 0; i < source.left.size(); ++i)
    sourcePeak = std::max(
        {sourcePeak, std::abs(source.left[i]), std::abs(source.right[i])});
  for (std::size_t i = 0; i < frames; ++i)
    wetPeak =
        std::max({wetPeak, std::abs(out.left[i]), std::abs(out.right[i])});
  const float wetGain = wetPeak > 1e-12f ? sourcePeak / wetPeak : 0;
  float levelGain = 1;
  const float slew = 1 - std::exp(-1 / (rate * .003f));
  for (std::size_t i = 0; i < frames; ++i) {
    if ((i & 1023u) == 0 && cancel())
      return false;
    const float t = float(i) / std::max(1.f, float(frames - 1)),
                target = std::pow(10.f,
                                  animatedCurve(r, 3, t, float(i) / rate) / 20);
    if (i == 0)
      levelGain = target;
    levelGain += (target - levelGain) * slew;
    const float dl = i < source.left.size() ? source.left[i] : 0,
                dr = i < source.right.size() ? source.right[i] : 0,
                l = dl * (1 - r.conv[cMix]) +
                    out.left[i] * wetGain * r.conv[cMix],
                rr = dr * (1 - r.conv[cMix]) +
                     out.right[i] * wetGain * r.conv[cMix],
                mid = (l + rr) * .5f,
                gain = fileEnvelope(r, i, frames, seam, rate) * levelGain;
    out.left[i] = (mid + (l - mid) * r.v[width]) * gain;
    out.right[i] = (mid + (rr - mid) * r.v[width]) * gain;
  }
  const auto splice =
      r.v[loop] > .5f
          ? std::min<std::size_t>(frames / 4, std::size_t(rate * .015f))
          : 0;
  finishSound(r, rate, out, frames - splice, splice);
  progress(100);
  return !cancel();
}
} // namespace s3g_thru_synth
