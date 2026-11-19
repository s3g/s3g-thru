#pragma once
namespace s3g_thru_synth {
struct PluckString {
  s3g::WaveguideFractionalDelay delay;
  float previous = 0, allpassInput = 0, allpassOutput = 0, hz = 110;
  void prepare(float rate, float tune, const Recipe &r, std::uint32_t seed) {
    hz = tune;
    delay.prepare(rate, .06f);
    const float dispersion = r.plucked[pDispersion] > 0
                                 ? (1 - .65f * r.plucked[pDispersion]) /
                                       (1 + .65f * r.plucked[pDispersion])
                                 : 0;
    delay.setDelaySamples(
        std::max(2.f, rate / hz - .5f * r.plucked[pDamping] - dispersion));
    const auto length =
        std::max<std::size_t>(3, std::size_t(std::ceil(rate / hz)));
    std::vector<float> noise(length);
    float mean = 0, low = 0;
    for (std::size_t i = 0; i < length; ++i) {
      const float white = random(seed, std::uint32_t(i)) * 2 - 1;
      low += (white - low) * (.04f + .9f * r.plucked[pBrightness]);
      noise[i] = low;
      mean += low;
    }
    mean /= float(length);
    const auto pick = std::clamp<std::size_t>(
        std::size_t(length * (.05f + .9f * r.plucked[pPick])), 1, length - 1);
    for (std::size_t i = 0; i < length; ++i)
      delay.writeAndAdvance(
          (noise[i] - noise[(i + length - pick) % length]) * .5f - mean * .01f);
  }
  float tick(float rate, float tune, float damping, const Recipe &r) {
    hz += (tune - hz) * (1 - std::exp(-1 / (rate * .003f)));
    const float sample = delay.read();
    float value = sample * (1 - .5f * damping) + previous * .5f * damping;
    previous = sample;
    if (r.plucked[pDispersion] > 0) {
      const float a = .65f * r.plucked[pDispersion],
                  y = a * value + allpassInput - a * allpassOutput;
      allpassInput = value;
      allpassOutput = y;
      value = y;
    }
    const float drive = 1 + r.plucked[pHardness] * 4;
    if (r.plucked[pHardness] > 0)
      value = std::tanh(value * drive) / drive;
    const float gain =
        std::exp(-6.90775528f / std::max(1.f, hz * r.plucked[pDecay] * .001f));
    delay.writeAndAdvance(value * gain);
    return sample;
  }
};
template <class Cancel, class Progress>
bool renderPluck(const Recipe &r, float rate, Audio &out, Cancel cancel,
                 Progress progress) {
  WorkshopSpan span(r, rate, out);
  std::array<PluckString, 2> strings;
  for (int ch = 0; ch < 2; ++ch)
    strings[ch].prepare(rate,
                        r.plucked[pTune] * std::exp2((ch ? 1.f : -1.f) *
                                                     r.plucked[pDetune] / 2400),
                        r, hash(r.seed ^ 0x504c5543u));
  float damping = r.plucked[pDamping];
  std::array<float, 2> tone{};
  for (std::size_t i = 0; i < span.total; ++i) {
    if ((i & 1023u) == 0) {
      if (cancel())
        return false;
      progress(unsigned(i * 88 / span.total));
    }
    const float u = float(i % span.frames) /
                    std::max(1.f, float(span.frames - 1)),
                seconds = float(i) / rate;
    const float pitch = animatedCurve(r, 0, u, seconds),
                target = std::clamp(r.plucked[pDamping] +
                                        animatedCurve(r, 1, u, seconds),
                                    0.f, .98f);
    damping += (target - damping) * .01f;
    float frame[2]{};
    for (int ch = 0; ch < 2; ++ch) {
      const float hz = std::clamp(
          r.plucked[pTune] *
              std::exp2((pitch + (ch ? 1.f : -1.f) * r.plucked[pDetune] / 200) /
                        12),
          20.f, 2000.f);
      if ((i & 31u) == 0) {
        const float dispersion = r.plucked[pDispersion] > 0
                                     ? (1 - .65f * r.plucked[pDispersion]) /
                                           (1 + .65f * r.plucked[pDispersion])
                                     : 0;
        strings[ch].delay.setDelaySamples(
            std::max(2.f, rate / hz - .5f * damping - dispersion), true);
      }
      frame[ch] = strings[ch].tick(rate, hz, damping, r);
      const float brightness = std::clamp(r.plucked[pBrightness] +
                                              animatedCurve(r, 2, u, seconds),
                                          0.f, 1.f),
                  cutoff =
                      std::min(rate * .44f, 80 * std::pow(200.f, brightness));
      tone[ch] +=
          (frame[ch] - tone[ch]) * (1 - std::exp(-Plan::tau * cutoff / rate));
      frame[ch] = tone[ch];
    }
    const float mid = .5f * (frame[0] + frame[1]),
                side = .5f * (frame[0] - frame[1]) * r.v[width];
    out.left[i] = mid + side;
    out.right[i] = mid - side;
  }
  if (!finishWorkshop(r, rate, out, span, cancel))
    return false;
  progress(100);
  return true;
}
// Thirty-two independently editable sine components: sixteen multipliers
// followed by sixteen dividers. Random motion is optional, never voice gating.
template <class Cancel, class Progress>
bool renderSubharmonic(const Recipe &r, float rate, Audio &out, Cancel cancel,
                       Progress progress) {
  WorkshopSpan span(r, rate, out);
  std::array<double, 32> phase{};
  std::array<float, 32> hz{}, targetHz{}, gain{}, targetGain{}, pan{};
  const float slew = 1 - std::exp(-1 / (rate * .008f));
  for (int j = 0; j < 32; ++j)
    pan[j] = bankPan(r.seed, j) * r.v[width];
  for (std::size_t i = 0; i < span.total; ++i) {
    if ((i & 1023u) == 0) {
      if (cancel())
        return false;
      progress(unsigned(i * 88 / span.total));
    }
    if ((i & 15u) == 0) {
      const float u = float(i % span.frames) /
                      std::max(1.f, float(span.frames - 1)),
                  seconds = float(i) / rate;
      const float base =
          440 * std::exp2((r.v[root] + 12 * r.sub[sRegister] +
                           animatedCurve(r, 0, u, seconds) - 69) /
                          12);
      const float balance =
          r.sub[sSubs] > 0
              ? std::clamp(r.sub[sBalance] + animatedCurve(r, 1, u, seconds),
                           0.f, 1.f)
              : 0;
      const float tilt = std::clamp(
          r.sub[sTilt] + animatedCurve(r, 2, u, seconds), -30.f, 12.f);
      for (int j = 0; j < 32; ++j) {
        const bool sub = j >= 16;
        const float ratio = r.ratios[j / 8][j % 8];
        const auto seed = hash(r.seed ^ std::uint32_t(j + 0x53554248u));
        const float cents =
            r.sub[sDetune] * (random(seed, 1) * 2 - 1) +
            r.sub[sDrift] * workshopRandom(seed, seconds * r.sub[sRate]);
        const float frequency =
            base * (sub ? 1 / ratio : ratio) * std::exp2(cents / 1200);
        targetHz[j] = std::clamp(frequency, 1.f, rate * .48f);
        const float enabled = std::clamp(
            (sub ? r.sub[sSubs] : r.sub[sHarmonics]) - (sub ? j - 16 : j), 0.f,
            1.f);
        targetGain[j] =
            r.partials[j / 8][j % 8] * enabled * (sub ? balance : 1 - balance) *
            std::pow(std::max(1.f, sub ? ratio * .5f : ratio), tilt / 6.0206f) *
            bankFade(frequency, rate) *
            std::clamp((frequency - 5) / 7, 0.f, 1.f);
        if (i == 0) {
          hz[j] = targetHz[j];
          gain[j] = targetGain[j];
        }
      }
    }
    float left = 0, right = 0;
    for (int j = 0; j < 32; ++j) {
      hz[j] += (targetHz[j] - hz[j]) * slew;
      gain[j] += (targetGain[j] - gain[j]) * slew;
      const float value = std::sin(phase[j] * Plan::tau) * gain[j];
      left += value * std::sqrt(1 - pan[j]);
      right += value * std::sqrt(1 + pan[j]);
      phase[j] += hz[j] / rate;
      phase[j] -= std::floor(phase[j]);
    }
    out.left[i] = left;
    out.right[i] = right;
  }
  if (!finishWorkshop(r, rate, out, span, cancel))
    return false;
  progress(100);
  return true;
}
struct EventPlacement {
  std::size_t frame = 0;
  int ordinal = 0;
};
inline std::size_t eventFrame(const Recipe &r, float rate, int event) {
  const auto seed = hash(r.seed ^ std::uint32_t(event + 0x45564e54u));
  const double offset =
      event == 0
          ? 0
          : ((r.timing[eClock] > .5f && (event & 1)) ? r.timing[eSwing] * .5
                                                     : 0) +
                (random(seed, 2) * 2 - 1) * r.events[eJitter] * .1;
  return std::size_t(
      std::llround(std::max(0.0, (event + offset) * eventInterval(r) * rate)));
}
inline int eventFitCount(const Recipe &r, float rate) {
  const auto frames = std::size_t(std::llround(r.v[length] * rate));
  int count = 0;
  while (count < int(r.events[eCount]) && eventFrame(r, rate, count) < frames)
    ++count;
  return count;
}
inline std::vector<EventPlacement> eventPlacements(const Recipe &r,
                                                   float rate) {
  std::vector<EventPlacement> out;
  const auto count = eventFitCount(r, rate);
  out.reserve(count);
  for (int event = 0; event < count; ++event)
    out.push_back({eventFrame(r, rate, event), event});
  return out;
}
inline int eventNote(const Recipe &r, float offset) {
  const int wanted = std::clamp(int(std::lround(r.v[root] + offset)), 24, 96);
  int selected = wanted, distance = 100;
  for (int n = 24; n <= 96; ++n)
    if (r.scale & (1u << ((n - int(r.v[root]) + 120) % 12))) {
      const int d = std::abs(n - wanted);
      if (d < distance) {
        distance = d;
        selected = n;
      }
    }
  return selected;
}
template <class Cancel, class Progress, class Render>
bool renderEventField(const Recipe &r, float rate, Audio &out, Cancel cancel,
                      Progress progress, Render renderChild) {
  WorkshopSpan span(r, rate, out);
  const int count = int(r.events[eCount]), mode = int(r.events[eMode]);
  const auto placements = eventPlacements(r, rate);
  const auto gateFrames = eventWindowFrames(r, rate);
  for (const auto &placement : placements) {
    if (cancel())
      return false;
    const int event = placement.ordinal;
    if (mode == 4 && (event % 4) == 3)
      continue; // Three-on/one-rest grouping.
    const auto seed = hash(r.seed ^ std::uint32_t(event + 0x45564e54u));
    const float seconds = float(placement.frame) / rate,
                u = seconds / r.v[length];
    float density = r.events[eDensity];
    if (r.type == drum)
      density += animatedCurve(r, 1, u, seconds);
    if (random(seed, 3) >=
        std::clamp(density * (mode == 2 ? .75f : 1.f), 0.f, 1.f))
      continue;
    Recipe child = r;
    child.events[eMode] = 0;
    child.v[loop] = 0;
    child.v[level] = 1;
    child.seed = hash(seed) % 65536;
    const auto eventFrames = std::max<std::size_t>(
        2, std::size_t(std::llround(
               gateFrames * (r.type == drum
                                 ? std::exp2(animatedCurve(r, 2, u, seconds))
                                 : 1.f))));
    child.v[length] = std::clamp(float(eventFrames) / rate, .04f, 12.f);
    const float offset = (random(seed, 4) * 2 - 1) * r.events[ePitch] * .5f +
                         animatedCurve(r, 0, u, seconds);
    const int note = eventNote(r, offset);
    child.v[root] = float(note);
    if (r.type == drum)
      setDrum(child, dTune,
              r.drums[dTune] * std::exp2((note - r.v[root]) / 12));
    if (r.type == pluck)
      setWorkshop(child, 0, pTune,
                  r.plucked[pTune] * std::exp2((note - r.v[root]) / 12));
    if (r.type == partialsType) {
      child.additive[aCount] = std::clamp(
          r.additive[aCount] + animatedCurve(r, 1, u, seconds), 1.f, 32.f);
      child.additive[aStretch] = std::clamp(
          r.additive[aStretch] * std::exp2(animatedCurve(r, 2, u, seconds)),
          .25f, 4.f);
    }
    if (r.type == bloom) {
      child.packets[bCount] = std::clamp(
          r.packets[bCount] + animatedCurve(r, 1, u, seconds), 1.f, 8.f);
      child.packets[bGap] = std::clamp(
          r.packets[bGap] * std::exp2(animatedCurve(r, 2, u, seconds)), 50.f,
          1000.f);
    }
    if (r.type == pluck) {
      child.plucked[pDamping] = std::clamp(
          r.plucked[pDamping] + animatedCurve(r, 1, u, seconds), 0.f, .98f);
      child.plucked[pBrightness] = std::clamp(
          r.plucked[pBrightness] + animatedCurve(r, 2, u, seconds), 0.f, 1.f);
    }
    for (auto &curve : child.curves)
      curve = Curve{};
    child.mod[mAmount] = 0;
    if (mode == 3) {
      child.v[attack] = std::min(child.v[length] * 400.f, 1000.f);
      child.v[release] = std::min(child.v[length] * 400.f, 2000.f);
    } else {
      child.v[attack] =
          std::max(child.v[attack], r.type == drum ? r.drumExtra[xEdge] : 2.f);
      child.v[release] = std::max(child.v[release], 8.f);
    }
    Audio sound;
    const auto childProgress = [&](unsigned p) {
      progress(unsigned((event * 100 + p) * 88 / (count * 100)));
    };
    if (!renderChild(child, sound, childProgress))
      return false;
    const auto first = placement.frame;
    const float pan = (random(seed, 5) * 2 - 1) * r.events[ePan] * r.v[width],
                gain = 1 - r.events[eVariation] * random(seed, 6);
    const float gl = std::sqrt(1 - pan) * gain, gr = std::sqrt(1 + pan) * gain;
    const auto available =
        std::min({sound.left.size(), eventFrames, span.total - first});
    const float fade =
        std::max(1.f, std::min(rate * .002f, float(available - 1) * .2f));
    const float attackFade =
        r.type == drum
            ? std::max(1.f, std::min(rate * r.drumExtra[xEdge] * .001f,
                                     float(available - 1) * .2f))
            : fade;
    for (std::size_t i = 0; i < available; ++i) {
      if ((i & 1023u) == 0 && cancel())
        return false;
      const float edge = std::clamp(
          std::min(float(i) / attackFade, float(available - 1 - i) / fade), 0.f,
          1.f);
      const float envelope = edge * edge * (3 - 2 * edge);
      out.left[first + i] += sound.left[i] * gl * envelope;
      out.right[first + i] += sound.right[i] * gr * envelope;
    }
  }
  if (!finishWorkshop(r, rate, out, span, cancel))
    return false;
  progress(100);
  return true;
}
} // namespace s3g_thru_synth
