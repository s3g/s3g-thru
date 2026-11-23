#pragma once
#include "thru_fm_voice.h"
#include "thru_lowform_parameters.h"
namespace s3g_thru_lowform {
// Fixed-memory Lowform voice adaptation. Coefficients are prepared at control
// rate; only the audible patch blend runs filters/nonlinear processing. All
// patches keep aging pitch/phase/envelopes, including inaudible patches.

constexpr float tau = 6.2831853071795864769f;
inline float saturate(float x) { return x / std::sqrt(1 + x * x); }
inline float blend(float x, float y, float t) { return x + (y - x) * t; }
inline void advance(float &phase, float increment) {
  phase += increment;
  phase -= std::floor(phase);
}
inline float blep(float phase, float increment) {
  if (increment <= 0)
    return 0;
  if (phase < increment) {
    const float x = phase / increment;
    return x + x - x * x - 1;
  }
  if (phase > 1 - increment) {
    const float x = (phase - 1) / increment;
    return x * x + x + x + 1;
  }
  return 0;
}
inline float oscillator(float phase, float increment, float wave,
                        float pw = .5f) {
  const float saw = 2 * phase - 1 - blep(phase, increment);
  float edge = phase - pw;
  edge -= std::floor(edge);
  const float pulse = (phase < pw ? 1.f : -1.f) + blep(phase, increment) -
                      blep(edge, increment);
  // Triangle is a band-limited sum; no discontinuity at the phase boundary.
  const auto &sine = s3g_thru_fm::Sines::get();
  float triangle = sine.read(phase);
  const float guard = std::clamp((.42f - increment * 3) / .08f, 0.f, 1.f);
  triangle -= sine.read(phase * 3) * (.111111f * guard);
  return wave < .5f ? blend(triangle, saw, wave * 2)
                    : blend(saw, pulse, (wave - .5f) * 2);
}
struct Svf {
  float one = 0, two = 0;
  struct Output {
    float low, band, high;
  };
  Output tick(float x, float g, float damping) {
    const float v1 = (one + g * (x - two)) / (1 + g * (g + damping)),
                v2 = two + g * v1;
    one = 2 * v1 - one;
    two = 2 * v2 - two;
    if (std::abs(one) < 1e-20f)
      one = 0;
    if (std::abs(two) < 1e-20f)
      two = 0;
    return {v2, v1, x - damping * v1 - v2};
  }
};
struct DriveStage {
  float previous = 0, previousRoot = 1;
  float tick(float x, float gain, float mix, float normal) {
    if (mix < 1e-8f) {
      const float dry = (previous + x) * .5f;
      previous = x;
      return dry;
    }
    const float p = previous * gain, q = x * gain, root = std::sqrt(1 + q * q);
    const float before = std::sqrt(1 + p * p);
    const float wet = (p + q) / (before + root) * normal;
    const float dry = (previous + x) * .5f;
    previous = x;
    previousRoot = root;
    return blend(dry, wet, mix);
  }
};
struct Model {
  Config current{}, target{};
  float hz = 261.625565f, targetHz = 261.625565f, rate = 0, pole = .01f,
        readGain = 0;
  float basePhase = 0, subPhase = 0, ringPhase = 0, smearPhase = 0,
        secondaryPhase = 0, syncJump = 0;
  std::array<float, 7> swarmPhases{}, swarmIncrements{}, swarmDrift{},
      swarmWeights{};
  std::array<float, 4> ladder{};
  std::array<Svf, 6> throat{};
  std::array<float, 6> throatG{}, throatLevels{};
  std::array<std::array<Svf, 2>, 2> filters{};
  std::array<float, 6> filterMix{};
  std::array<float, 3> foundationMix{};
  float octaveMix = 0;
  std::array<float, 5> textureMix{};
  std::array<DriveStage, 2> overload{};
  std::array<float, 2048> delay{};
  unsigned write = 0;
  std::array<float, 2> noiseLow{}, wetLow1{}, wetLow2{}, cleanLow1{},
      cleanLow2{}, dc{}, last{}, previousFoundation{}, lastBody{}, correction{};
  float noiseHeld = 0, noiseCounter = 0, raveNoise = 0, bodyEnv = 1,
        textureEnv = 1, pitchEnv = 1, filterEnv = 1, syncEnv = 1;
  float bodyPole = 1, texturePole = 1, textureRise = 1, pitchPole = 1,
        filterPole = 1, syncPole = 1;
  float filterG = .1f, filterTarget = .1f, ladderG = .1f, noiseG = .1f,
        lowG = .01f, dcG = .9999f;
  float bodyDrive = 1, bodyNorm = 1, filterDriveGain = 1, filterDriveNorm = 1,
        driveGain = 1;
  float correctionGain = 0, correctionPole = .01f, swarmNorm = 1, ringRatio = 1,
        throatDamping = .5f, driveNormal = 1;
  Config preparedConfig{};
  float preparedRate = 0, preparedNote = -1, keySemitones = 0,
        preparedCutoff = -1, preparedAcidCutoff = -1;
  int preparedBody = -1;
  int bodyType = 0, swarmCount = 1, countdown = 0;
  bool smoothing = false, wasAudible = false, textureAttacking = false,
       pendingBody = false;
  std::uint32_t random = 1;
  float white() {
    random = s3g_thru_table::Library::hash(random + 0x9e3779b9u);
    return float(random >> 8) * (2.f / 16777215.f) - 1;
  }
  void prepare(float noteHz, float sr) {
    const auto &p = current.v;
    const auto &old = preparedConfig.v;
    const auto &lib = s3g_thru_table::Library::get();
    const float os = sr * 2;
    const bool fresh = preparedRate != sr || preparedBody != bodyType;
    targetHz = std::clamp(
        noteHz * lib.pitchRatio(p[tune] + p[punch] * pitchEnv), 5.f, sr * .22f);
    const auto decay = [](float ms, float sampleRate) {
      return ms >= 11999 ? 1.f
                         : std::exp(-6.907755f /
                                    (sampleRate * std::max(.001f, ms * .001f)));
    };
    if (fresh || old[body_decay] != p[body_decay])
      bodyPole = decay(p[body_decay], sr);
    if (fresh || old[texture_decay] != p[texture_decay])
      texturePole = decay(p[texture_decay], sr);
    if (fresh || old[texture_attack] != p[texture_attack])
      textureRise = p[texture_attack] > 0
                        ? 1 - std::exp(-5 / (sr * p[texture_attack] * .001f))
                        : 1;
    if (fresh || old[punch_time] != p[punch_time])
      pitchPole = std::exp(-1 / (sr * p[punch_time] * .001f));
    if (fresh || old[filter_decay] != p[filter_decay])
      filterPole = std::exp(-1 / (sr * p[filter_decay] * .001f));
    if (bodyType == 5 && (fresh || old[d] != p[d]))
      syncPole = std::exp(-1 / (sr * (.015f * std::pow(80.f, p[d] * .01f))));
    if (fresh || preparedNote != noteHz)
      keySemitones = 12 * std::log2(std::max(5.f, noteHz) / 65.406391f);
    const float cutoffHz = std::clamp(
        p[cutoff] * lib.pitchRatio(keySemitones * p[keytrack] * .01f +
                                   12 * p[filter_env] * filterEnv),
        20.f, sr * .42f);
    if (fresh || cutoffHz != preparedCutoff)
      filterTarget = std::tan(3.14159265359f * cutoffHz / os);
    if (bodyType == 2) {
      const float acidCut = std::clamp(
          cutoffHz * lib.pitchRatio((p[e] * .01f - .5f) * 96), 25.f, sr * .41f);
      if (fresh || acidCut != preparedAcidCutoff)
        ladderG = 1 - std::exp(-tau * acidCut / os);
      preparedAcidCutoff = acidCut;
    }
    if (fresh || old[texture_color] != p[texture_color])
      noiseG = 1 - std::exp(-tau * (250 + 9000 * p[texture_color] * .01f) / os);
    if (fresh || old[d] != p[d] || old[e] != p[e]) {
      bodyDrive = 1 + (bodyType == 0
                           ? p[e] * .052f
                           : bodyType == 2
                                 ? p[d] * .048f
                                 : bodyType == 3 ? p[e] * .075f : p[e] * .05f);
      bodyNorm = std::max(.1f, saturate(bodyDrive));
    }
    if (fresh || old[filter_drive] != p[filter_drive]) {
      filterDriveGain = 1 + p[filter_drive] * .05f;
      filterDriveNorm = std::max(.1f, saturate(filterDriveGain));
    }
    if (fresh || old[drive] != p[drive]) {
      driveGain = 1 + p[drive] * .12f;
      driveNormal = 1 / std::sqrt(std::sqrt(driveGain));
    }
    if (bodyType == 1) {
      if (fresh || old[c] != p[c]) {
        const float count = 1 + p[c] * .06f;
        constexpr int order[]{3, 2, 4, 1, 5, 0, 6};
        float power = 0;
        for (int i = 0; i < 7; ++i) {
          const float weight = std::clamp(count - i, 0.f, 1.f);
          swarmWeights[order[i]] = weight;
          power += weight * weight;
        }
        swarmNorm = 1 / (std::sqrt(std::max(1.f, power)) * 1.35f);
      }
      constexpr float cents[]{-19, -11, -5, 0, 6, 12, 20};
      if (fresh || old[b] != p[b] || old[d] != p[d] || p[d] > 0)
        for (int i = 0; i < 7; ++i) {
          const float driftCents =
              s3g_thru_fm::Sines::get().read(swarmDrift[i]) * p[d] * .045f;
          swarmIncrements[i] = lib.pitchRatio(
              (cents[i] * (.05f + p[b] * .0195f) + driftCents) * .01f);
        }
    }
    ringRatio = .5f + 3.5f * p[texture_color] * .01f;
    if (bodyType == 4 &&
        (fresh || old[a] != p[a] || old[b] != p[b] || old[c] != p[c])) {
      static constexpr float formants[5][6] = {
          {650, 1080, 2650, 2900, 3250, 3700},
          {400, 1700, 2600, 3200, 3580, 4100},
          {290, 1870, 2800, 3250, 3540, 4200},
          {400, 800, 2600, 2800, 3000, 3500},
          {350, 600, 2400, 2675, 2950, 3300}};
      static constexpr float levels[5][6] = {{1, .5f, .3f, .2f, .12f, .08f},
                                             {.7f, 1, .35f, .2f, .12f, .08f},
                                             {.8f, 1, .45f, .2f, .12f, .08f},
                                             {1, .7f, .25f, .16f, .1f, .06f},
                                             {1, .55f, .18f, .12f, .08f, .05f}};
      const float pos = p[a] * .04f;
      const int first = std::min(3, int(pos));
      const float mix = pos - first,
                  shift = lib.pitchRatio((p[b] * .01f - .5f) * 30);
      float sum = 0;
      for (int i = 0; i < 6; ++i) {
        const float f = std::clamp(
            blend(formants[first][i], formants[first + 1][i], mix) * shift,
            55.f, sr * .39f);
        throatG[i] = std::tan(3.14159265359f * f / os);
        throatLevels[i] = blend(levels[first][i], levels[first + 1][i], mix);
        sum += throatLevels[i];
      }
      for (auto &v : throatLevels)
        v *= 2.4f / std::max(.8f, sum);
      throatDamping = 2 - (.30f + p[c] * .0067f) * 1.92f;
    }
    preparedConfig = current;
    preparedRate = sr;
    preparedNote = noteHz;
    preparedBody = bodyType;
    preparedCutoff = cutoffHz;
  }
  void clearBody() {
    ladder = {};
    throat = {};
    delay = {};
    write = 0;
    raveNoise = 0;
    secondaryPhase = basePhase;
    syncJump = 0;
  }
  void reset(const Config &cfg, std::uint32_t id, float sr, float noteHz) {
    *this = {};
    current = target = cfg;
    bodyType = int(cfg.v[body]);
    random = s3g_thru_table::Library::hash(id + 1);
    rate = sr;
    pole = 1 - std::exp(-1 / (sr * .008f));
    correctionPole = std::exp(-1 / (sr * .003f));
    textureAttacking = cfg.v[texture_attack] > 0;
    textureEnv = textureAttacking ? 0.f : 1.f;
    foundationMix[int(cfg.v[foundation_wave])] = 1;
    octaveMix = cfg.v[foundation_octave] < 0 ? 1.f : 0.f;
    filterMix[int(cfg.v[filter_type])] = 1;
    textureMix[int(cfg.v[texture_type])] = 1;
    lowG = 1 - std::exp(-tau * 118 / (sr * 2));
    dcG = std::exp(-tau * 3 / (sr * 2));
    prepare(noteHz, sr);
    hz = targetHz;
    filterG = filterTarget;
  }
  float readDelay(float samples) const {
    samples = std::clamp(samples, 1.f, 2046.f);
    float at = float(write) - samples;
    if (at < 0)
      at += 2048;
    const unsigned i = unsigned(at);
    return blend(delay[i & 2047], delay[(i + 1) & 2047], at - i);
  }
  std::array<std::array<float, 2>, 2> tick(float noteHz, float sr,
                                           bool audible) {
    const auto &lib = s3g_thru_table::Library::get();
    const auto &sine = s3g_thru_fm::Sines::get();
    if (smoothing) {
      smoothing = false;
      for (int f = 0; f < fields; ++f) {
        const float change = target.v[f] - current.v[f];
        if (discrete(f) ||
            std::abs(change) < 1e-5f * (1 + std::abs(target.v[f])))
          current.v[f] = target.v[f];
        else {
          current.v[f] += change * pole;
          smoothing = true;
        }
      }
    }
    const auto &p = current.v;
    const int nextBody = int(p[body]);
    if (nextBody != bodyType) {
      pendingBody = true;
      correctionGain = 1;
      bodyType = nextBody;
      clearBody();
      countdown = 0;
    }
    if (audible && !wasAudible) {
      clearBody();
      filters = {};
      overload = {};
      wetLow1 = {};
      wetLow2 = {};
      cleanLow1 = {};
      cleanLow2 = {};
      dc = {};
      last = {};
      previousFoundation = {};
      readGain = 0;
      correctionGain = 0;
      pendingBody = false;
      // Keep the same control clock as inaudible matching patches.
      filterG = filterTarget;
    }
    wasAudible = audible;
    if (countdown-- <= 0) {
      prepare(noteHz, sr);
      countdown = 15;
    }
    hz += (targetHz - hz) * .025f;
    const float increment = hz / (sr * 2);
    const float oldBodyEnv = bodyEnv, oldTexEnv = textureEnv;
    bodyEnv *= bodyPole;
    if (textureAttacking) {
      textureEnv += (1 - textureEnv) * textureRise;
      if (textureEnv >= .9999f) {
        textureEnv = 1;
        textureAttacking = false;
      }
    } else
      textureEnv *= texturePole;
    for (float *v : {&bodyEnv, &textureEnv, &pitchEnv, &filterEnv, &syncEnv})
      if (std::abs(*v) < 1e-20f)
        *v = 0;
    pitchEnv *= pitchPole;
    filterEnv *= filterPole;
    syncEnv *= syncPole;
    if (!audible) {
      for (int sub = 0; sub < 2; ++sub) {
        advance(basePhase, increment);
        advance(subPhase, increment * .5f);
        advance(ringPhase, increment * ringRatio);
        advance(smearPhase, (.075f + p[c] * .0012f) / (sr * 2));
        if (bodyType == 1)
          for (int i = 0; i < 7; ++i) {
            advance(swarmPhases[i], increment * swarmIncrements[i]);
            advance(swarmDrift[i], (.035f + i * .011f) / (sr * 2));
          }
      }
      readGain = 0;
      return {};
    }
    std::array<std::array<float, 2>, 2> out{};
    for (int sub = 0; sub < 2; ++sub) {
      const float A = p[a] * .01f, B = p[b] * .01f, C = p[c] * .01f,
                  D = p[d] * .01f, E = p[e] * .01f;
      for (int i = 0; i < 3; ++i)
        foundationMix[i] +=
            (float(i == int(p[foundation_wave])) - foundationMix[i]) * .015f;
      octaveMix += (float(p[foundation_octave] < 0) - octaveMix) * .015f;
      const auto foundationAt = [&](float phase, float inc) {
        const float fundamental = sine.read(phase),
                    third = sine.read(phase * 3) *
                            std::clamp((.42f - inc * 3) / .08f, 0.f, 1.f);
        return fundamental * foundationMix[0] +
               (fundamental - third * .111111f) * 1.1f * foundationMix[1] +
               (fundamental + third * .18f) * .9f * foundationMix[2];
      };
      const float foundation =
          blend(foundationAt(basePhase, increment),
                foundationAt(subPhase, increment * .5f), octaveMix) *
          p[foundation_level] * .01f;
      std::array<float, 2> bodyAudio{};
      if (bodyType == 0) {
        const float raw =
            oscillator(basePhase, increment, A * .5f, .12f + C * .76f) +
            oscillator(basePhase, increment, 1, .12f + C * .76f) * B * .42f;
        const float driven = saturate(raw * bodyDrive) / bodyNorm;
        const float cross =
            oscillator(basePhase, increment, .5f) *
            oscillator(basePhase, increment, 1, .12f + C * .76f);
        const float mono = blend(driven, cross, D * .48f);
        bodyAudio = {mono, mono};
      } else if (bodyType == 1) {

        for (int i = 0; i < 7; ++i) {
          const float inc = std::min(.42f, increment * swarmIncrements[i]);
          if (swarmWeights[i] > 0) {
            const float raw =
                blend(sine.read(swarmPhases[i]),
                      oscillator(swarmPhases[i], inc, .5f), .22f + E * .78f);
            const float shaped = blend(raw, saturate(raw * (1.5f + A * 3.5f)),
                                       A * .72f) *
                                 swarmWeights[i],
                        pan = (i - 3) / 3.f * .28f;
            bodyAudio[0] += shaped * (.5f - pan) * swarmNorm;
            bodyAudio[1] += shaped * (.5f + pan) * swarmNorm;
          }
          advance(swarmPhases[i], inc);
          advance(swarmDrift[i], (.035f + i * .011f) / (sr * 2));
        }
      } else if (bodyType == 2) {
        const float raw =
            oscillator(basePhase, increment, .5f + A * .5f, .1f + C * .8f);
        float input = saturate(
            raw * (1 + p[filter_drive] * .032f + D * 4.8f) -
            ladder[3] * (.34f + B * .48f + p[resonance] * .0016f) * 3.72f);
        for (float &stage : ladder) {
          stage += (input - stage) * ladderG;
          input = saturate(stage);
        }
        const float reson = .34f + B * .48f + p[resonance] * .0016f;
        const float bass = ladder[3] * (1 + reson * 1.35f) * .72f,
                    edge = (ladder[2] - ladder[3]) * (1.15f + reson * 2.2f);
        bodyAudio = {bass + edge * .28f, bass + edge * .04f};
      } else if (bodyType == 3) {
        const float core = oscillator(basePhase, increment, A),
                    divider = oscillator(subPhase, increment * .5f, 1);
        raveNoise += (white() - raveNoise) * .14f;
        const float clean = core * .66f + saturate(divider * 1.55f) * B * .64f +
                            raveNoise * D * .2f;
        const float mono =
            blend(clean, saturate(clean * bodyDrive) / bodyNorm, E * .88f);
        delay[write] = mono;
        const float base = sr * 2 * (.0014f + C * .0048f),
                    depth = sr * 2 * (.00008f + C * .00095f);
        bodyAudio = {
            blend(mono, readDelay(base + sine.read(smearPhase) * depth),
                  C * .74f),
            blend(mono, readDelay(base + sine.read(smearPhase + .37f) * depth),
                  C * .74f)};
        write = (write + 1) & 2047;
        advance(smearPhase, (.075f + C * .12f) / (sr * 2));
      } else if (bodyType == 4) {
        const float glottal =
            oscillator(basePhase, increment, .5f + D * .41f, .18f + D * .54f);
        for (int i = 0; i < 6; ++i) {
          const float band =
              throat[i].tick(glottal, throatG[i], throatDamping).band *
              throatDamping * throatLevels[i];
          const float pan =
              i == 0 ? 0.f : (i & 1 ? -1.f : 1.f) * (.04f + i * .018f);
          bodyAudio[0] += band * (.5f - pan);
          bodyAudio[1] += band * (.5f + pan);
        }
        for (float &v : bodyAudio)
          v = saturate(v * bodyDrive) / bodyNorm * .78f;
      } else {
        const float ratio = 1 + B * 7, sweep = C * C * 4.5f * syncEnv;
        const float secondaryIncrement =
            std::min(.22f, increment * ratio * lib.pitchRatio(sweep * 12));
        const float nextCross = (1 - basePhase) / std::max(1e-7f, increment);
        float crossing = secondaryPhase + secondaryIncrement * nextCross;
        crossing -= std::floor(crossing);
        const float jump =
            oscillator(0, secondaryIncrement, A) - oscillator(crossing, secondaryIncrement, A);
        const float raw = oscillator(secondaryPhase, secondaryIncrement, A) +
                          .5f * (basePhase > 1 - increment ? jump : syncJump) *
                              blep(basePhase, increment);
        const float primary = sine.read(basePhase),
                    ringed = raw * primary * 1.55f,
                    mono = blend(raw, ringed, E * .78f),
                    side = (ringed - raw) * .1f;
        bodyAudio = {(mono + side) * .72f, (mono - side) * .72f};
        advance(secondaryPhase, secondaryIncrement);
        if (basePhase + increment >= 1) {
          syncJump = jump;
          secondaryPhase = (basePhase + increment - 1) /
                       std::max(1e-7f, increment) * secondaryIncrement;
          secondaryPhase -= std::floor(secondaryPhase);
        }
      }
      for (int ch = 0; ch < 2; ++ch) {
        bodyAudio[ch] *=
            p[body_level] * .01f * blend(oldBodyEnv, bodyEnv, (sub + 1) * .5f);
        if (pendingBody)
          correction[ch] = lastBody[ch] - bodyAudio[ch];
        bodyAudio[ch] += correction[ch] * correctionGain;
        lastBody[ch] = bodyAudio[ch];
      }
      pendingBody = false;
      correctionGain *= correctionPole;
      for (int i = 0; i < 5; ++i)
        textureMix[i] +=
            (float(i == int(p[texture_type])) - textureMix[i]) * .015f;
      std::array<float, 2> texture{};
      if (textureMix[1] + textureMix[2] + textureMix[3] + textureMix[4] >
          1e-8f) {
        const float track = p[texture_track] * .01f;
        const float heldRate =
            std::clamp(blend(35.f + 1500 * p[texture_color] * .01f,
                             hz * (.25f + 5 * p[texture_color] * .01f), track),
                       10.f, sr * .4f);
        noiseCounter += heldRate / (sr * 2);
        if (noiseCounter >= 1) {
          noiseCounter -= 1;
          noiseHeld = white();
        }
        for (int ch = 0; ch < 2; ++ch) {
          const float w = white();
          noiseLow[ch] += (w - noiseLow[ch]) * noiseG;
          const float ring = bodyAudio[ch] * sine.read(ringPhase),
                      wire = sine.read(ringPhase + (ch ? 0.17f : 0)) *
                             noiseLow[ch];
          texture[ch] = noiseLow[ch] * textureMix[1] +
                        noiseHeld * textureMix[2] + ring * textureMix[3] +
                        wire * textureMix[4];
        }
        const float mid = .5f * (texture[0] + texture[1]),
                    widthValue = p[texture_width] * .01f;
        const float env = blend(oldTexEnv, textureEnv, (sub + 1) * .5f) *
                          p[texture_level] * .01f * .38f;
        for (float &v : texture)
          v = (mid + (v - mid) * widthValue) * env;
      }
      const float ringHz = int(p[texture_type]) == 4
                               ? blend(180 + 35 * p[texture_color],
                                       hz * ringRatio, p[texture_track] * .01f)
                               : hz * ringRatio;
      advance(ringPhase, std::min(.42f, ringHz / (sr * 2)));
      filterG += (filterTarget - filterG) * .02f;
      for (int i = 0; i < 6; ++i)
        filterMix[i] +=
            (float(i == int(p[filter_type])) - filterMix[i]) * .015f;
      const float damping = 2 - 1.92f * p[resonance] * .01f;
      for (int ch = 0; ch < 2; ++ch) {
        const float input = foundation + bodyAudio[ch] + texture[ch];
        const float driven =
            saturate(input * filterDriveGain) / filterDriveNorm;
        const auto first = filters[ch][0].tick(driven, filterG, damping),
                   second = filters[ch][1].tick(first.low, filterG, damping);
        const float filtered =
            input * filterMix[0] + first.low * filterMix[1] +
            second.low * filterMix[2] + first.band * damping * filterMix[3] +
            first.high * filterMix[4] + (first.low + first.high) * filterMix[5];
        float signal = overload[ch].tick(
            filtered, driveGain, std::min(1.f, p[drive] * .02f), driveNormal);
        const float clean = (foundation + previousFoundation[ch]) * .5f;
        previousFoundation[ch] = foundation;
        cleanLow1[ch] += (clean - cleanLow1[ch]) * lowG;
        cleanLow2[ch] += (cleanLow1[ch] - cleanLow2[ch]) * lowG;
        wetLow1[ch] += (signal - wetLow1[ch]) * lowG;
        wetLow2[ch] += (wetLow1[ch] - wetLow2[ch]) * lowG;
        signal += (cleanLow2[ch] - wetLow2[ch]) * p[low_return] * .01f *
                  std::min(1.f, p[foundation_level] * .04f);
        const float high = signal - last[ch] + dcG * dc[ch];
        last[ch] = signal;
        dc[ch] = std::abs(high) < 1e-20f ? 0 : high;
        out[sub][ch] = dc[ch];
      }
      advance(basePhase, increment);
      advance(subPhase, increment * .5f);
    }
    readGain += (1 - readGain) * .04f;
    return out;
  }
};
struct Instrument {
  std::array<Model, 4> models{};
  std::array<s3g_thru_table::Decimator, 2> decimator{};
  std::array<float, 4> laneOutput{};
  float position = 0, noteHz = 261.625565f, positionPole = 0, lastSlew = -1;
  bool started = false;
  int firstPatch = 0, lastPatch = 3;
  std::uint32_t identity = 1;
  void prepare() {
    (void)s3g_thru_fm::Sines::get();
    (void)s3g_thru_table::Library::get();
  }
  void reset() {
    started = false;
    firstPatch = 0;
    lastPatch = 3;
    decimator = {};
    lastSlew = -1;
  }
  // Fixed for this note; reset before changing lane ownership.
  void setPatchRange(int first, int last) {
    if (started) return;
    const bool valid = first >= 0 && last < 4 && first <= last;
    firstPatch = valid ? first : 0;
    lastPatch = valid ? last : 3;
  }
  void beginBlock(const std::array<Config, 4> &patches) {
    for (int i = firstPatch; i <= lastPatch; ++i)
      if (models[i].target.v != patches[i].v) {
        models[i].target = patches[i];
        models[i].smoothing = true;
      }
  }
  void endBlock() {}
  std::array<float, 2> tick(const std::array<Config, 4> &patches,
                            float targetPosition, float note, float sr,
                            float slewMs = 5, const float *gains = nullptr) {
    if (!started) {
      for (int i = firstPatch; i <= lastPatch; ++i)
        models[i].reset(patches[i], identity + unsigned(i) * 7919, sr, note);
      position = targetPosition;
      noteHz = note;
      started = true;
    }
    if (slewMs != lastSlew) {
      lastSlew = slewMs;
      positionPole = 1 - std::exp(-1 / (sr * std::max(.005f, slewMs * .001f)));
    }
    position += (targetPosition - position) * positionPole;
    position = std::clamp(position, 0.f, 3.f);
    noteHz += (note - noteHz) * .005f;
    const int first = int(position), second = std::min(3, first + 1);
    const float x = position - first, mix = x * x * (3 - 2 * x);
    std::array<std::array<float, 2>, 2> summed{};
    laneOutput = {};
    for (int i = firstPatch; i <= lastPatch; ++i) {
      const float gain =
          ((i == first ? 1 - mix : 0) + (i == second ? mix : 0)) *
          (gains ? gains[i] : 1.f);
      auto &model = models[i];
      const auto audio = model.tick(noteHz, sr, gain > 1e-7f);
      if (gain <= 1e-7f)
        continue;
      const float amount =
                      model.current.v[level] * .0065f * model.readGain * gain,
                  wide = model.current.v[width] * .01f;
      for (int sub = 0; sub < 2; ++sub) {
        const float mid = (audio[sub][0] + audio[sub][1]) * .5f;
        for (int ch = 0; ch < 2; ++ch)
          summed[sub][ch] += (mid + (audio[sub][ch] - mid) * wide) * amount;
        laneOutput[i] += mid * amount * .5f;
      }
    }
    std::array<float, 2> out{};
    for (int sub = 0; sub < 2; ++sub)
      for (int ch = 0; ch < 2; ++ch)
        out[ch] = decimator[ch].tick(summed[sub][ch]);
    return out;
  }
};
} // namespace s3g_thru_lowform
