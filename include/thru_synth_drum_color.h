#pragma once
namespace s3g_thru_synth {
// Offline, stereo-linked bit/rate/tone processing. Neutral controls are an
// exact bypass, preserving older recipes and waveform references.
template <class Cancel>
bool colorDrum(const Recipe &r, float rate, Audio &out, Cancel cancel) {
  const auto &c = r.drumColor;
  if (c[cBlend] == 0 ||
      (c[cBits] == 24 && c[cRate] >= rate && c[cLowpass] >= 19999))
    return !cancel();
  float peak = 0;
  for (std::size_t i = 0; i < out.left.size(); ++i) {
    if ((i & 1023u) == 0 && cancel())
      return false;
    peak = std::max({peak, std::abs(out.left[i]), std::abs(out.right[i])});
  }
  if (peak < 1e-9f)
    return !cancel();
  const float inputGain = .8f / peak, outputGain = 1 / inputGain;
  const float levels = std::exp2(c[cBits] - 1);
  const double increment = std::min(1.0, double(c[cRate]) / rate);
  double phase = 1;
  float held[2]{}, low[2]{}, smooth[2]{};
  const bool filter = c[cLowpass] < 19999;
  const float coefficient =
      1 - std::exp(-Plan::tau * std::min(c[cLowpass], rate * .44f) / rate);
  for (std::size_t i = 0; i < out.left.size(); ++i) {
    if ((i & 1023u) == 0 && cancel())
      return false;
    float raw[2]{out.left[i] * inputGain, out.right[i] * inputGain};
    if (phase >= 1) {
      phase -= std::floor(phase);
      for (int ch = 0; ch < 2; ++ch)
        held[ch] =
            c[cBits] < 24 ? std::round(raw[ch] * levels) / levels : raw[ch];
    }
    phase += increment;
    for (int ch = 0; ch < 2; ++ch) {
      float wet = held[ch];
      if (filter) {
        low[ch] += (wet - low[ch]) * coefficient;
        smooth[ch] += (low[ch] - smooth[ch]) * coefficient;
        wet = smooth[ch];
      }
      const float value =
          (c[cBlend] == 1 ? wet : raw[ch] + (wet - raw[ch]) * c[cBlend]) *
          outputGain;
      (ch == 0 ? out.left[i] : out.right[i]) = value;
    }
  }
  return !cancel();
}
} // namespace s3g_thru_synth
