#pragma once
#include "thru_fm_voice.h"
#include "thru_reson_parameters.h"
namespace s3g_thru_reson {
constexpr float tau = 6.2831853071795864769f;
// Well below the 16-bit output floor, including the overload return.
constexpr float tailSilence = 1e-8f;
// Compact physical-body spectra: no nested voice pools, meshes or solvers.
struct Profile {
  std::array<float, 8> ratios, weights;
  float noiseGain;
};
inline constexpr std::array<Profile, 8> profiles{
    {{{{1, 1.593f, 2.136f, 2.653f, 3.155f, 3.598f, 4.12f, 4.72f}},
      {{1, .8f, .65f, .5f, .38f, .3f, .22f, .16f}},
      .35f},
     {{{1, 2, 3.01f, 4.05f, 5.1f, 6.2f, 7.35f, 8.5f}},
      {{1, .18f, .1f, .06f, .04f, .02f, .01f, .005f}},
      .15f},
     {{{1, 1.58f, 2.12f, 2.62f, 3.25f, 3.97f, 4.7f, 5.6f}},
      {{1, .8f, .6f, .45f, .3f, .22f, .15f, .1f}},
      .95f},
     {{{1, 1.55f, 2.26f, 3.07f, 4.1f, 5.17f, 6.4f, 7.7f}},
      {{1, .6f, .4f, .28f, .17f, .12f, .08f, .04f}},
      .35f},
     {{{1, 1.4142f, 2.01f, 2.75f, 3.76f, 5.04f, 6.4f, 8.27f}},
      {{.8f, 1, .7f, .5f, .4f, .28f, .19f, .12f}},
      .25f},
     {{{1, 1.46f, 1.91f, 2.43f, 3.02f, 3.72f, 4.54f, 5.48f}},
      {{.18f, .35f, .6f, .8f, 1, .9f, .7f, .5f}},
      .7f},
     {{{1, 1.34f, 1.71f, 2.1f, 3.03f, 4.16f, 6.31f, 9.08f}},
      {{.25f, .4f, .6f, .85f, 1, .8f, .6f, .35f}},
      .7f},
     {{{1, 1.593f, 2.136f, 2.653f, 3.155f, 3.598f, 6.2f, 8.7f}},
      {{1, .8f, .6f, .42f, .3f, .22f, .1f, .05f}},
      .65f}}};
struct Overload {
  struct Stage {
    float previous = 0, root = 1;
    void reset(float x) {
      previous = x;
      root = std::sqrt(1 + x * x);
    }
    float tick(float x, float edge) {
      const float next = std::sqrt(1 + x * x),
                  soft = (x + previous) / (next + root);
      const float lo = std::min(x, previous), hi = std::max(x, previous);
      float hard = 0;
      if (lo >= 1)
        hard = 1;
      else if (hi <= -1)
        hard = -1;
      else if (lo >= -1 && hi <= 1)
        hard = (lo + hi) * .5f;
      else {
        const float span = std::max(1e-12f, hi - lo);
        if (lo >= -1) {
          const float d = 1 - lo;
          hard = 1 - .5f * d * d / span;
        } else if (hi <= 1) {
          const float d = hi + 1;
          hard = -1 + .5f * d * d / span;
        } else
          hard = (hi + lo) / span;
      }
      previous = x;
      root = next;
      return soft + (hard - soft) * edge;
    }
  };
  std::array<Stage, 2> signal{};
  Stage reference{};
  std::array<float, 2> previousInput{}, previousSub{}, low{}, dc{}, last{};
  std::array<float, 8> prepared{};
  std::array<float, 11> targets{};
  float controlPole = .01f;
  float sampleRate = 0, envelope = 0, attack = 0, release = 0, driveGain = 1,
        normal = 1, biasValue = 0, edge = 0, amount = 0, mixValue = 1,
        levelGain = 1, tonePole = 1, toneMix = 0, sagAmount = 0,
        dcPole = .9999f;
  void prepare(const Config &c, float sr) {
    std::array<float, 8> p{};
    std::copy(c.v.begin() + drive, c.v.end(), p.begin());
    if (sampleRate == sr && p == prepared)
      return;
    const bool fresh = sampleRate != sr;
    sampleRate = sr;
    prepared = p;
    attack = 1 - std::exp(-1 / (sr * .001f));
    dcPole = std::exp(-tau * 3 / (sr * 2));
    controlPole = 1 - std::exp(-1 / (sr * .002f));
    targets[0] = 1 + c.v[drive] * .16f;
    targets[1] = 1 / std::sqrt(std::sqrt(targets[0]));
    targets[2] = c.v[bias] * .003f;
    targets[3] = c.v[clip] * .01f;
    targets[4] = std::min(1.f, c.v[drive] * .02f);
    targets[5] = c.v[mix] * .01f;
    targets[6] = std::pow(10.f, c.v[output] * .05f);
    targets[7] = 1 - std::exp(-tau * std::min(c.v[tone], sr * .44f) / (sr * 2));
    targets[8] = std::clamp((19500 - c.v[tone]) / 2000, 0.f, 1.f);
    targets[9] = c.v[sag] * .06f;
    targets[10] = 1 - std::exp(-1000 / (sr * c.v[recovery]));
    if (fresh) snapToTargets();
  }

  void snapToTargets() {
    driveGain = targets[0];
    normal = targets[1];
    biasValue = targets[2];
    edge = targets[3];
    amount = targets[4];
    mixValue = targets[5];
    levelGain = targets[6];
    tonePole = targets[7];
    toneMix = targets[8];
    sagAmount = targets[9];
    release = targets[10];
  }

  void reset(const Config &c, float sr) {
    // Coefficients are cached; restarting audio state does not invalidate them.
    prepare(c, sr);
    snapToTargets();
    envelope = 0;
    previousInput = {};
    previousSub = {};
    low = {};
    dc = {};
    last = {};
    for (auto &s : signal)
      s.reset(biasValue);
    reference.reset(biasValue);
  }
  std::array<std::array<float, 2>, 2> tick(const std::array<float, 2> &input) {
    const auto approach = [&](float &value, float target) {
      const float delta = target - value;
      if (std::abs(delta) < 1e-6f * (1 + std::abs(target)))
        value = target;
      else
        value += delta * controlPole;
    };
    approach(driveGain, targets[0]);
    approach(normal, targets[1]);
    approach(biasValue, targets[2]);
    approach(edge, targets[3]);
    approach(amount, targets[4]);
    approach(mixValue, targets[5]);
    approach(levelGain, targets[6]);
    approach(tonePole, targets[7]);
    approach(toneMix, targets[8]);
    approach(sagAmount, targets[9]);
    approach(release, targets[10]);
    const float peak = std::max(std::abs(input[0]), std::abs(input[1]));
    envelope += (peak - envelope) * (peak > envelope ? attack : release);
    if (envelope < 1e-20f)
      envelope = 0;
    const float gain = driveGain / (1 + sagAmount * envelope * amount);
    std::array<std::array<float, 2>, 2> out{};
    for (int sub = 0; sub < 2; ++sub) {
      const float biasReference = reference.tick(biasValue, edge);
      for (int ch = 0; ch < 2; ++ch) {
        const float x = sub ? input[ch] : (previousInput[ch] + input[ch]) * .5f;
        const float dry = (previousSub[ch] + x) * .5f;
        previousSub[ch] = x;
        const float saturated =
            (signal[ch].tick(x * gain + biasValue, edge) - biasReference) *
            normal;
        const float nonlinear = dry + (saturated - dry) * amount;
        low[ch] += (nonlinear - low[ch]) * tonePole;
        const float wet = nonlinear + (low[ch] - nonlinear) * toneMix;
        // AC-couple the combined path: asymmetric saturation must not leave
        // a signal-dependent DC offset, and MIX must retain matched timing.
        const float sample = (dry + (wet - dry) * mixValue) * levelGain;
        dc[ch] = sample - last[ch] + dcPole * dc[ch];
        last[ch] = sample;
        out[sub][ch] = dc[ch];
      }
    }
    previousInput = input;
    return out;
  }
  bool quiet() const {
    for (const auto &values : {previousInput, previousSub, low, dc, last})
      for (float value : values)
        if (!(std::abs(value) <= tailSilence)) return false;
    for (const auto &stage : signal)
      if (!(std::abs(stage.previous - biasValue) <= tailSilence)) return false;
    return true;
  }
  void flush() {
    for (int c = 0; c < 2; ++c)
      for (float *x :
           {&previousInput[c], &previousSub[c], &low[c], &dc[c], &last[c]})
        if (std::abs(*x) < 1e-20f)
          *x = 0;
  }
};
struct Modal {
  std::array<float, 2> phase{};
  float ratio = 1, targetRatio = 1, weight = 0, targetWeight = 0, envelope = 1,
        pole = 1, pan = 0;
};
struct Model {
  Config current{}, target{}, prepared{};
  std::array<Modal, 8> modes{};
  Overload circuit;
  std::array<float, 2> noiseLow{}, hitLow{}, coupleState{}, dc{}, last{};
  float hz = 261.625565f, parameterPole = 0, noiseEnvelope = 1, hitEnvelope = 1,
        pitchEnvelope = 1, punchEnvelope = 1;
  float noisePole = 1, hitPole = 1, pitchPole = 1, punchPole = 1,
        colorPole = .5f, hardPole = .5f, detuneRatio = 1, noiseGain = .35f,
        targetNoiseGain = .35f, readGain = 0, srPrepared = 0;
  std::uint32_t random = 1;
  int countdown = 0, quietFrames = 0;
  bool latchedHit = false, smoothing = false, wasAudible = false, finished = false;
  float nextRandom() {
    random = s3g_thru_table::Library::hash(random + 0x9e3779b9u);
    return float(random >> 8) * (2.f / 16777215.f) - 1;
  }
  void prepare(float sr) {
    const auto &p = current.v;
    const bool fresh = srPrepared != sr;
    const auto &old = prepared.v;
    const auto pole = [&](float ms) {
      return std::exp(-6.907755f / (sr * std::max(.0002f, ms * .001f)));
    };
    if (fresh || old[noise_decay] != p[noise_decay])
      noisePole = pole(p[noise_decay]);
    if (fresh || old[hit_decay] != p[hit_decay])
      hitPole = pole(p[hit_decay]);
    if (fresh || old[sweep] != p[sweep])
      pitchPole = pole(p[sweep]);
    if (fresh)
      punchPole = std::exp(-1 / (sr * .015f));
    if (fresh || old[color] != p[color]) {
      const float c = p[color] * .01f;
      colorPole = 1 - std::exp(-tau * (120 + 11000 * c * c) / sr);
    }
    if (fresh || old[hardness] != p[hardness])
      hardPole = 1 - std::exp(-tau * (400 + 10000 * p[hardness] * .01f) / sr);
    if (fresh || old[detune] != p[detune])
      detuneRatio = s3g_thru_table::Library::get().pitchRatio(p[detune] / 200);
    const bool bodyChanged =
        fresh || old[model] != p[model] || old[stretch] != p[stretch] ||
        old[inharmonic] != p[inharmonic] || old[mass] != p[mass] ||
        old[brightness] != p[brightness] || old[position] != p[position] ||
        old[damping] != p[damping] || old[decay] != p[decay];
    if (bodyChanged) {
      const auto &profile = profiles[std::clamp(int(p[model]), 0, 7)];
      targetNoiseGain = profile.noiseGain;
      float total = 0;
      for (int j = 0; j < 8; ++j) {
        auto &m = modes[j];
        const float order = float(j) / 7, shape = p[inharmonic] * .01f;
        const float ratio =
            float(j + 1) + (profile.ratios[j] - float(j + 1)) * shape;
        m.targetRatio = std::pow(ratio, p[stretch]);
        const float spot =
            .3f +
            .7f * std::abs(std::cos((j + 1) * p[position] * .01f * 1.5707963f));
        const float bright = std::pow(.15f + .85f * p[brightness] * .01f,
                                      order * 2.5f),
                    massScale = 1 / (1 + order * p[mass] * .02f);
        m.targetWeight = profile.weights[j] * spot * bright * massScale;
        total += m.targetWeight;
        const float duration =
            p[decay] * std::max(.08f, 1 - order * p[damping] * .0085f);
        m.pole = pole(duration);
      }
      const float normalize = 1 / std::max(.8f, total * .72f);
      for (auto &m : modes)
        m.targetWeight *= normalize;
    }
    circuit.prepare(current, sr);
    circuit.flush();
    for (int ch = 0; ch < 2; ++ch)
      for (float *x :
           {&noiseLow[ch], &hitLow[ch], &coupleState[ch], &dc[ch], &last[ch]})
        if (std::abs(*x) < 1e-20f)
          *x = 0;
    prepared = current;
    srPrepared = sr;
  }
  void reset(const Config &c, std::uint32_t id, float sr, float noteHz) {
    current = target = c;
    prepared = {};
    modes = {};
    noiseLow = {};
    hitLow = {};
    coupleState = {};
    dc = {};
    last = {};
    noiseEnvelope = hitEnvelope = pitchEnvelope = punchEnvelope = 1;
    readGain = 0;
    countdown = 0;
    quietFrames = 0;
    finished = false;
    smoothing = false;
    wasAudible = false;
    srPrepared = 0;
    latchedHit = c.v[mode] > .5f;
    random = s3g_thru_table::Library::hash(id + std::uint32_t(c.v[seed]) + 1);
    parameterPole = 1 - std::exp(-1 / (sr * .006f));
    hz = noteHz * s3g_thru_table::Library::get().pitchRatio(c.v[tune]);
    prepare(sr);
    noiseGain = targetNoiseGain;
    for (int j = 0; j < 8; ++j) {
      auto &m = modes[j];
      m.ratio = m.targetRatio;
      m.weight = m.targetWeight;
      m.pan = j == 0 ? 0.f : ((j & 1) ? 1.f : -1.f) * (.2f + .1f * j);
    }
    // The circuit is reset on first audible use (and each later re-entry).
  }
  bool exciterQuiet() const {
    if (!latchedHit || !(noiseEnvelope <= tailSilence)
        || !(hitEnvelope <= tailSilence)) return false;
    for (const auto &m : modes)
      if (!(m.envelope <= tailSilence)) return false;
    return true;
  }
  std::array<std::array<float, 2>, 2> tick(float noteHz, float sr,
                                           bool audible) {
    if (finished) return {};
    if (smoothing) {
      smoothing = false;
      for (int f = 0; f < fields; ++f) {
        const float d = target.v[f] - current.v[f];
        if (discrete(f) || std::abs(d) < 1e-5f * (1 + std::abs(target.v[f])))
          current.v[f] = target.v[f];
        else {
          current.v[f] += d * parameterPole;
          smoothing = true;
        }
      }
    }
    if (countdown-- <= 0) {
      countdown = 31;
      prepare(sr);
    }
    const auto &p = current.v;
    const auto &lib = s3g_thru_table::Library::get();
    hz += (noteHz * lib.pitchRatio(p[tune]) - hz) * parameterPole;
    noiseGain += (targetNoiseGain - noiseGain) * parameterPole;
    const float frequency =
        hz * lib.pitchRatio(latchedHit ? p[bend] * pitchEnvelope : 0);
    std::array<float, 2> body{};
    const auto &sine = s3g_thru_fm::Sines::get();
    for (int j = 0; j < 8; ++j) {
      auto &m = modes[j];
      m.ratio += (m.targetRatio - m.ratio) * parameterPole;
      m.weight += (m.targetWeight - m.weight) * parameterPole;
      const float env =
          latchedHit
              ? m.envelope
              : ((j == 0 ? .65f : .3f) + (j == 0 ? .35f : .7f) * m.envelope);
      for (int ch = 0; ch < 2; ++ch) {
        const float f =
            frequency * m.ratio * (ch ? detuneRatio : 1 / detuneRatio);
        if (audible) {
          const float guard =
                          std::clamp((sr * .42f - f) / (sr * .08f), 0.f, 1.f),
                      phase = m.phase[ch] + coupleState[ch] * p[coupling] *
                                                .0003f * float(j) / 7;
          const float value = sine.read(phase) * m.weight * env * guard;
          body[ch] += value * (ch ? 1 + std::min(0.f, m.pan)
                                  : 1 - std::max(0.f, m.pan));
        }
        m.phase[ch] += std::min(f, sr * .45f) / sr;
        if (m.phase[ch] >= 1)
          m.phase[ch] -= 1;
      }
      m.envelope *= m.pole;
      if (m.envelope < 1e-20f)
        m.envelope = 0;
    }
    std::array<std::array<float, 2>, 2> result{};
    if (audible) {
      if (!wasAudible) {
        circuit.reset(current, sr);
        readGain = 0;
        wasAudible = true;
      }
      readGain += (1 - readGain) * .035f;
      const float noiseEnv =
                      latchedHit ? noiseEnvelope : .35f + .65f * noiseEnvelope,
                  boost = 1 + p[punch] * .025f * punchEnvelope;
      for (int ch = 0; ch < 2; ++ch) {
        const float white = nextRandom();
        noiseLow[ch] += (white - noiseLow[ch]) * colorPole;
        hitLow[ch] += (white - hitLow[ch]) * hardPole;
        const float knock =
            (white - hitLow[ch]) * (.25f + .75f * p[hardness] * .01f) +
            sine.read(modes[0].phase[ch] * 3) * .25f * (1 - p[hardness] * .01f);
        float sample =
            (body[ch] + noiseLow[ch] * p[noise] * .012f * noiseGain * noiseEnv +
             knock * p[hit] * .009f * hitEnvelope) *
            boost;
        coupleState[ch] += (body[ch] - coupleState[ch]) * .1f;
        body[ch] = sample;
      }
      result = circuit.tick(body);
      for (auto &sub : result)
        for (auto &x : sub)
          x *= readGain;
    } else {
      wasAudible = false;
      readGain = 0;
    }
    const auto age = [](float &v, float pole) {
      v *= pole;
      if (v < 1e-20f)
        v = 0;
    };
    age(noiseEnvelope, noisePole);
    age(hitEnvelope, hitPole);
    age(pitchEnvelope, pitchPole);
    age(punchEnvelope, punchPole);
    // Completion requires spent exciters AND quiet filter/overload history.
    // It is never inferred from a waveform zero crossing or a muted level.
    // An inaudible patch resets its circuit before any later audible entry.
    if (latchedHit && countdown == 31) {
      if (exciterQuiet() && (!wasAudible || circuit.quiet())) {
        quietFrames += 32;
        if (quietFrames >= sr * .005f) finished = true;
      } else quietFrames = 0;
    }
    return result;
  }
};
struct Instrument {
  std::array<Model, 4> models{};
  std::array<s3g_thru_table::Decimator, 2> decimator{};
  std::array<float, 4> laneOutput{};
  float position = 0, noteHz = 261.625565f, positionPole = 0, lastSlew = -1;
  bool started = false, released = false, finished = false;
  float releasePosition = -1;
  int finishCountdown = 0;
  int firstPatch = 0, lastPatch = 3;
  std::uint32_t identity = 1;
  void prepare() {
    (void)s3g_thru_fm::Sines::get();
    (void)s3g_thru_table::Library::get();
  }
  void reset() {
    started = released = finished = false;
    releasePosition = -1;
    finishCountdown = 0;
    firstPatch = 0;
    lastPatch = 3;
    lastSlew = -1;
    for (auto &d : decimator)
      for (auto &s : d.sections)
        s.z1 = s.z2 = 0;
  }
  // Lane ownership is fixed for the note; the host resets voices on mode/owner changes.
  void setPatchRange(int first, int last) {
    if (started) return;
    const bool valid = first >= 0 && last < 4 && first <= last;
    firstPatch = valid ? first : 0;
    lastPatch = valid ? last : 3;
  }
  void release() {
    if (released) return;
    released = true;
    releasePosition = started ? std::clamp(position, float(firstPatch), float(lastPatch)) : -1.f;
  }
  bool relevantPatchesFinished() const {
    if (!released) {
      // Held mixed stacks must retain a TONE patch they can scan back into.
      for (int lane = firstPatch; lane <= lastPatch; ++lane)
        if (!models[lane].finished) return false;
      return true;
    }
    if (!(releasePosition >= firstPatch && releasePosition <= lastPatch)) return false;
    const int first = std::clamp(int(releasePosition), 0, 3), second = std::min(3, std::min(lastPatch, first + 1));
    const float t = releasePosition - first, w = t * t * (3 - 2 * t);
    return (w == 1.f || models[first].finished)
        && (w == 0.f || models[second].finished);
  }
  bool decimatorQuiet() const {
    for (const auto &d : decimator) for (const auto &s : d.sections)
      if (!(std::abs(s.z1) <= tailSilence) || !(std::abs(s.z2) <= tailSilence)) return false;
    return true;
  }
  void beginBlock(const std::array<Config, 4> &p) {
    for (int i = firstPatch; i <= lastPatch; ++i)
      if (!models[i].finished && models[i].target.v != p[i].v) {
        models[i].target = p[i];
        models[i].smoothing = true;
      }
  }
  void endBlock() {}
  std::array<float, 2> tick(const std::array<Config, 4> &p,
                            float targetPosition, float hz, float sr,
                            float slewMs = 5, const float *gains = nullptr) {
    if (finished) { laneOutput = {}; return {}; }
    if (!started) {
      for (int i = firstPatch; i <= lastPatch; ++i)
        models[i].reset(p[i], identity + i * 7919, sr, hz);
      position = targetPosition;
      noteHz = hz;
      started = true;
    }
    if (released) {
      if (releasePosition < 0) releasePosition = std::clamp(targetPosition, float(firstPatch), float(lastPatch));
      targetPosition = releasePosition;
    }
    if (lastSlew != slewMs) {
      lastSlew = slewMs;
      positionPole = 1 - std::exp(-1 / (sr * std::max(.005f, slewMs * .001f)));
    }
    position += (targetPosition - position) * positionPole;
    position = std::clamp(position, 0.f, 3.f);
    noteHz += (hz - noteHz) * .01f;
    const int first = int(position), second = std::min(3, first + 1);
    const float x = position - first, mix = x * x * (3 - 2 * x);
    std::array<std::array<float, 2>, 2> sum{};
    laneOutput = {};
    for (int lane = firstPatch; lane <= lastPatch; ++lane) {
      // A released blend cannot reach other patches again during this note.
      if (released && !((lane == first && mix < 1.f) || (lane == second && mix > 0.f))) continue;
      const float gain =
          ((lane == first ? 1 - mix : 0) + (lane == second ? mix : 0)) *
          (gains ? gains[lane] : 1.f);
      auto audio = models[lane].tick(noteHz, sr, gain > 1e-7f);
      const float levelGain = models[lane].current.v[level] * .009f * gain,
                  stereo = models[lane].current.v[width] * .01f;
      for (int sub = 0; sub < 2; ++sub) {
        const float mid = (audio[sub][0] + audio[sub][1]) * .5f;
        for (int ch = 0; ch < 2; ++ch)
          sum[sub][ch] += (mid + (audio[sub][ch] - mid) * stereo) * levelGain;
        laneOutput[lane] += mid * levelGain * .5f;
      }
    }
    std::array<float, 2> out{};
    for (int sub = 0; sub < 2; ++sub)
      for (int ch = 0; ch < 2; ++ch)
        out[ch] = decimator[ch].tick(sum[sub][ch]);
    if (++finishCountdown >= 32) {
      finishCountdown = 0;
      if (relevantPatchesFinished() && decimatorQuiet()) finished = true;
    }
    return out;
  }
};
} // namespace s3g_thru_reson
