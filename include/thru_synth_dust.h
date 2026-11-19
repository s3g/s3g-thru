#pragma once
namespace s3g_thru_synth {
// Offline artifact synthesis: bounded event voices and deliberate discontinuity
// controls. File boundaries still use the normal one-shot/loop cleanup.
struct DustNoise {
  float pink[3]{}, brown = 0, previous = 0;
  float tick(float white, int color, float rate) {
    const float hz[3]{12, 120, 1200};
    for (int j = 0; j < 3; ++j)
      pink[j] += (white - pink[j]) * (1 - std::exp(-Plan::tau * hz[j] / rate));
    brown += (white - brown) * (1 - std::exp(-Plan::tau * 35 / rate));
    const float blue = (white - previous) * .5f;
    previous = white;
    return color == 0
               ? white
               : color == 1
                     ? std::clamp((pink[0] + pink[1] + pink[2]) * .8f, -1.f,
                                  1.f)
                     : color == 2 ? std::clamp(brown * 3, -1.f, 1.f) : blue;
  }
};
struct DustVoice {
  std::uint32_t seed = 0, age = 0, size = 1, stride = 1;
  int repeats = 0;
  float gain = 0, pan = 0, polarity = 1, last[2]{};
  bool active() const {
    return repeats > 0 && age < stride * std::uint32_t(repeats);
  }
};
template <class Cancel, class Progress>
bool renderDust(const Recipe &r, float rate, Audio &out, Cancel cancel,
                Progress progress) {
  if (cancel())
    return false;
  WorkshopSpan span(r, rate, out);
  const auto &p = r.dustParams;
  const int mode = int(p[uMode]), color = int(p[uColor]),
            repeats = int(p[uRepeat]);
  std::array<DustVoice, 32> voices{};
  DustNoise noise[2];
  float tail[2]{};
  float rateHz = p[uRate], sizeMs = p[uSize];
  float targetRate = rateHz, targetSize = sizeMs;
  const float slew = 1 - std::exp(-1 / (rate * .008f));
  const float levels = std::exp2(p[uBits] - 1);
  const auto first =
      std::min<std::size_t>(span.frames / 4, std::size_t(rate * .005f));
  std::uint32_t serial = 0;
  for (std::size_t i = 0; i < span.total; ++i) {
    if ((i & 1023u) == 0) {
      if (cancel())
        return false;
      progress(unsigned(i * 88 / span.total));
    }
    const float seconds = float(i) / rate;
    if ((i & 31u) == 0) {
      const float u =
          float(i % span.frames) / std::max(1.f, float(span.frames - 1));
      targetRate = std::clamp(
          p[uRate] * std::exp2(animatedCurve(r, 0, u, seconds)), .1f, 2000.f);
      targetSize = std::clamp(
          p[uSize] * std::exp2(animatedCurve(r, 1, u, seconds)), .02f, 500.f);
    }
    rateHz += (targetRate - rateHz) * slew;
    sizeMs += (targetSize - sizeMs) * slew;

    // Slowly changing hazard groups the Poisson arrivals into short busy and
    // quiet regions. No event list, allocation or unbounded catch-up loop.
    const float cluster =
        std::pow(.5f +
                     .5f * workshopRandom(r.seed ^ 0x434c554du, seconds * 3.5f),
                 3.f) *
        4;
    const float probability = std::min(
        .8f, rateHz / rate * (1 - p[uClump] + p[uClump] * (.12f + cluster)));
    if (i == first ||
        random(r.seed ^ 0x44555354u, std::uint32_t(i)) < probability) {
      auto *voice = &voices[0];
      for (auto &v : voices)
        if (!v.active()) {
          voice = &v;
          break;
        } else if (float(v.age) / v.stride > float(voice->age) / voice->stride)
          voice = &v;
      if (voice->active())
        for (int ch = 0; ch < 2; ++ch)
          tail[ch] = std::clamp(tail[ch] + voice->last[ch], -4.f, 4.f);
      *voice = {};
      voice->seed = hash(r.seed ^ hash(++serial + 0x504f5053u));
      const float variation = p[uVariation];
      voice->size = std::max(
          1u, std::uint32_t(sizeMs * .001f * rate *
                            (1 - variation * .45f +
                             variation * .9f * random(voice->seed, 1))));
      voice->stride = voice->size + std::max(1u, voice->size / 2);
      voice->repeats = repeats;
      voice->gain = 1 - variation * .95f * random(voice->seed, 2);
      voice->pan = (random(voice->seed, 3) * 2 - 1) * r.v[width];
      voice->polarity = random(voice->seed, 4) < .5f ? -1.f : 1.f;
    }
    float bed[2]{};
    for (int ch = 0; ch < 2; ++ch)
      bed[ch] = noise[ch].tick(
          random(r.seed ^ (ch ? 0x52474854u : 0x4c454654u), std::uint32_t(i)) *
                  2 -
              1,
          color, rate);
    const float mid = .5f * (bed[0] + bed[1]);
    for (int ch = 0; ch < 2; ++ch)
      bed[ch] = mid + (bed[ch] - mid) * r.v[width];
    float frame[2]{}, activity = 0, weight = 0;
    for (auto &v : voices) {
      if (!v.active())
        continue;
      const auto phase = v.age % v.stride, repeat = v.age / v.stride;
      v.last[0] = v.last[1] = 0;
      if (phase < v.size) {
        const float u = float(phase) / std::max(1.f, float(v.size - 1));
        const float fade =
            std::min(float(v.size) * .5f, float(v.size) * p[uEdge] * .5f);
        const float edge =
            fade > 0 ? std::clamp(std::min(float(phase) + .5f,
                                           float(v.size - phase) - .5f) /
                                      std::max(1.f, fade),
                                  0.f, 1.f)
                     : 1.f;
        const float envelope = edge * edge * (3 - 2 * edge);
        const float g = v.gain * envelope;
        activity += g;
        weight += v.gain * v.gain;
        for (int ch = 0; ch < 2; ++ch) {
          float value = 0;
          if (mode == 0)
            value = v.polarity * std::exp(-9 * u) * (.65f + .35f * bed[ch]);
          else if (mode == 1)
            value = v.polarity * std::exp(-6 * u) * (1 - 12 * u) +
                    bed[ch] * .18f * std::exp(-12 * u);
          else if (mode == 3) {
            const float pulse =
                p[uEdge] > 0
                    ? std::tanh((.5f - u) / std::max(.001f, p[uEdge] * .25f))
                    : (u < .5f ? 1.f : -1.f);
            value = v.polarity * pulse * (.8f + .2f * bed[ch]);
          } else if (mode == 4)
            value = bed[ch];
          else if (mode == 5) {
            const bool corrupt = random(v.seed, repeat + 91) < p[uFault];
            const auto pattern = corrupt ? hash(v.seed ^ repeat) : v.seed;
            const auto address = (phase / std::max(1u, v.size / 32)) % 8;
            const auto word = hash(pattern ^ address);
            const auto flipped =
                corrupt ? word ^ (1u << ((repeat + address) % 24)) : word;
            value = (float(flipped & 65535u) / 32767.5f - 1) * .8f;
            if ((word & 31u) == 0)
              value = 0;
          }
          value *= g * std::sqrt(ch ? 1 + v.pan : 1 - v.pan);
          v.last[ch] = value;
          frame[ch] += value;
        }
      }
      ++v.age;
    }
    const float gain = 1 / std::sqrt(std::max(1.f, weight));

    for (int ch = 0; ch < 2; ++ch) {
      float value = mode == 2 ? bed[ch] * std::max(.05f, 1 - .8f * activity)
                              : frame[ch] * gain + bed[ch] * p[uFloor] * .5f;
      value += tail[ch];
      tail[ch] *= .6f;

      (ch == 0 ? out.left[i] : out.right[i]) = value;
    }
  }
  // Normalize the quantizer input, then restore its scale. Quiet pink/brown
  // material must not disappear simply because a low bit depth is selected.
  float peak = 0;
  if (p[uBits] < 24)
    for (std::size_t i = 0; i < span.total; ++i) {
      if ((i & 1023u) == 0 && cancel())
        return false;
      peak = std::max({peak, std::abs(out.left[i]), std::abs(out.right[i])});
    }
  const float norm = peak > 1e-9f ? .8f / peak : 1.f;
  float low[2]{}, smooth[2]{}, toneHz = p[uTone], targetTone = toneHz;
  for (std::size_t i = 0; i < span.total; ++i) {
    if ((i & 1023u) == 0 && cancel())
      return false;
    if ((i & 31u) == 0) {
      const float u =
          float(i % span.frames) / std::max(1.f, float(span.frames - 1));
      targetTone = std::clamp(
          p[uTone] * std::exp2(animatedCurve(r, 2, u, float(i) / rate)), 20.f,
          20000.f);
    }
    toneHz += (targetTone - toneHz) * slew;
    const float coefficient =
        1 - std::exp(-Plan::tau * std::min(toneHz, rate * .44f) / rate);
    const float filtered = std::clamp((20000 - toneHz) / 1500, 0.f, 1.f);
    for (int ch = 0; ch < 2; ++ch) {
      float &sample = ch == 0 ? out.left[i] : out.right[i];
      float value = p[uBits] < 24
                        ? std::round(sample * norm * levels) / (norm * levels)
                        : sample;
      low[ch] += (value - low[ch]) * coefficient;
      smooth[ch] += (low[ch] - smooth[ch]) * coefficient;
      sample = value + (smooth[ch] - value) * filtered;
    }
  }
  if (!finishWorkshop(r, rate, out, span, cancel))
    return false;
  progress(100);
  return true;
}
} // namespace s3g_thru_synth
