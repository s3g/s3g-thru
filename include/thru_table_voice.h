#pragma once
#include "thru_table_parameters.h"
#include "thru_table_tables.h"
namespace s3g_thru_table {
// Sources already use output-rate band-limited mip tables. Two internal
// samples retain nonlinear filtering/drive protection within Move's voice budget.
inline constexpr int oversample = 2;
struct Filter {
  float a = 0, b = 0, g = 1, k = 2, inverse = .25f;
  void setup(float cutoff, float resonance, float sr) {
    g = std::tan(3.14159265359f * std::clamp(cutoff, 20.f, sr * .42f) / sr);
    k = 2 - 1.95f * resonance;
    inverse = 1 / (1 + g * (g + k));
  }
  float tick(float x) {
    const float v1 = (a + g * (x - b)) * inverse, v2 = b + g * v1;
    a = 2 * v1 - a;
    b = 2 * v2 - b;
    return v2;
  }
};
struct State {
  Config current{}, target{};
  std::array<Filter, 2> filters{};
  std::array<Decimator, 2> decimator{};
  std::array<float, 2> pink{}, low{}, dc{}, last{};
  double phaseA = 0, phaseB = 0, subPhase = 0, driftPhase = 0, evolve = 0;
  std::array<double, 2> drumPhaseA{}, drumPhaseB{};
  float body = 1, pitchEnv = 1, scanEnv = 1, noiseEnv = 1, hitEnv = 1,
        hz = 261.625565f;
  float bodyPole = 1, pitchPole = 1, scanPole = 1, noisePole = 1, hitPole = 1,
        lod = 0, drumLodA = 0, drumLodB = 0, detuneRatio = 1;
  std::uint32_t randomState = 1, serial = 0, driftSerial = 0, identity = 1;
  int countdown = 0;
  float controlSlew = 0, pitchSlew = 0, pinkSlew = 0, lastSemitones = 10000,
        frequencyRatio = 1;
  bool smoothing = true;
  std::uint32_t splineSerial = ~0u, splineSeed = ~0u;
  std::array<std::array<int, 2>, 2> splinePalette{};
  std::array<float, 2> splineDynamics{};
  int splineCount = 0;
  float splineCountMix = 0, splineCurve = 1, splinePointValue = -1;
  std::uint32_t cachedDriftSerial = ~0u;
  std::array<float,2> driftValues{};
  struct SplineReadPart { const Library::Cycle* cycle=nullptr;float gain=0; };
  struct SplineReadPlan {
    std::array<SplineReadPart,8> parts{};int count=0;
    float read(const Library::Index& at)const {
      float out=0;for(int i=0;i<count;++i)out+=Library::sample(*parts[i].cycle,at)*parts[i].gain;return out;
    }
  };
  std::array<std::array<SplineReadPlan,2>,2> splinePlans{};
  std::uint32_t planSerial=~0u,planSeed=~0u;
  float planPoints=-1,planCurve=-1,planLod=-1;
  const Library* planLibrary=nullptr;
  bool splinePlansReady=false;
  void prepareSplinePlans(const Library& lib){
    if(planLibrary==&lib&&planSerial==splineSerial&&planSeed==splineSeed&&planPoints==splinePointValue&&planCurve==splineCurve&&planLod==lod)return;
    planLibrary=&lib;planSerial=splineSerial;planSeed=splineSeed;planPoints=splinePointValue;planCurve=splineCurve;planLod=lod;
    const int mip=std::min(Library::mips-2,int(lod));const float mipMix=lod-mip;
    const float countWeights[]{1-splineCountMix,splineCountMix},curveWeights[]{1-splineCurve,splineCurve},mipWeights[]{1-mipMix,mipMix};
    for(int ch=0;ch<2;++ch)for(int event=0;event<2;++event){
      auto& plan=splinePlans[ch][event];plan.count=0;
      for(int count=0;count<2;++count)if(countWeights[count]>0)
       for(int curve=0;curve<2;++curve)if(curveWeights[curve]>0)
        for(int level=0;level<2;++level)if(mipWeights[level]>0)
         plan.parts[plan.count++]={&lib.spline[splineCount+count][splinePalette[ch][event]][curve][mip+level],countWeights[count]*curveWeights[curve]*mipWeights[level]};
    }
  }
  void prepareSpline(bool controls = false) {
    const auto &p = current.v;
    if (controls) {
      if (splinePointValue != p[points]) {
        splinePointValue = p[points];
        splineCount = 0;
        while (splineCount < 6 && p[points] >= Library::pointCounts[splineCount + 1]) ++splineCount;
        splineCountMix = std::clamp((p[points] - Library::pointCounts[splineCount]) /
          float(Library::pointCounts[splineCount + 1] - Library::pointCounts[splineCount]), 0.f, 1.f);
      }
      splineCurve = p[curve] * .01f;
    }
    if (splineSerial == serial && splineSeed == std::uint32_t(p[seed])) return;
    splineSerial = serial; splineSeed = std::uint32_t(p[seed]);
    for (int event=0; event<2; ++event) {
      splineDynamics[event] = Library::random(identity ^ (serial + event));
      for (int channel=0; channel<2; ++channel)
        splinePalette[channel][event] = int(Library::hash(splineSeed ^ identity ^
          Library::hash(serial + event + 1) ^ std::uint32_t(channel * 0x52474854u)) % Library::palettes);
    }
  }
  void reset(const Config &cfg, float noteHz, float sr, std::uint32_t id,
             double age = 0, double phase = 0) {
    *this = {};
    current = target = cfg;
    identity = id;
    randomState = Library::hash(id + 1);
    hz = noteHz;
    phaseA = phase;
    phaseB = phase * cfg.v[ratio];
    drumPhaseA = {phase, phase};
    drumPhaseB = {phaseB, phaseB};
    subPhase = phase * .5;
    const double evol = age * noteHz / std::max(1.f, cfg.v[evolution]);
    serial = std::uint32_t(evol);
    evolve = evol - std::floor(evol);
    body =
        std::exp(float(-age * 6.907755 / std::max(.01f, cfg.v[decay] * .001f)));
    pitchEnv = std::exp(
        float(-age * 4.60517 / std::max(.001f, cfg.v[pitch_decay] * .001f)));
    scanEnv = std::exp(
        float(-age * 4.60517 / std::max(.001f, cfg.v[scan_decay] * .001f)));
    noiseEnv = std::exp(
        float(-age * 6.907755 / std::max(.001f, cfg.v[noise_decay] * .001f)));
    hitEnv = std::exp(float(-age / .0035));
    controlSlew = 1 - std::exp(-1 / (sr * .005f));
    pitchSlew = 1 - std::exp(-1 / (sr * .003f));
    pinkSlew = 1 - std::exp(-6.2831853f * 600 / (sr * oversample));
    coefficients(sr);
  }
  float white() {
    randomState = Library::hash(randomState + 0x9e3779b9u);
    return float(randomState >> 8) * (2.f / 16777215.f) - 1;
  }
  void coefficients(float sr) {
    const auto &p = current.v;
    constexpr int os = oversample;
    const float rate = sr * os;
    bodyPole = std::exp(-6.907755f / (rate * std::max(.01f, p[decay] * .001f)));
    pitchPole =
        std::exp(-4.60517f / (rate * std::max(.001f, p[pitch_decay] * .001f)));
    scanPole =
        std::exp(-4.60517f / (rate * std::max(.001f, p[scan_decay] * .001f)));
    noisePole =
        std::exp(-6.907755f / (rate * std::max(.001f, p[noise_decay] * .001f)));
    hitPole = std::exp(-1 / (rate * .0035f));
    const bool drum = int(p[type]) == 2;
    const float cents = drum ? p[wt_detune] : p[detune];
    detuneRatio = std::exp2(cents * p[width] / 240000.f);
    float cutoffHz =
        drum ? 300.f * std::exp2(std::clamp(p[tone] * .01f, 0.f, 1.f) * 6)
             : int(p[type]) == 0 ? p[cutoff] : sr * .42f;
    for (auto &f : filters)
      f.setup(cutoffHz, int(p[type]) == 0 ? p[resonance] * .01f : 0, rate);
    // Conservative upper bound keeps every mip choice stable between control
    // ticks.
    const float highest = std::min(
        sr * .43f,
        hz * std::exp2((p[tune] + (drum ? std::max(0.f, p[bend]) : p[drift])) /
                       12));
    lod = std::clamp(std::log2(std::max(1.f, 256 * highest / (sr * .36f))), 0.f,
                     8.f);
    drumLodA = std::clamp(
        std::log2(std::max(1.f, 32 * highest * (1 + p[fm] * .08f * p[ratio]) /
                                    (sr * .36f))),
        0.f, 5.f);
    drumLodB = std::clamp(
        std::log2(std::max(1.f, 32 * highest * p[ratio] / (sr * .36f))), 0.f,
        5.f);
  }
  float waveRead(const Library &lib, int which, double phase, float mip,
                 const std::array<float, fields> &p) const {
    if (which != 3)
      return Library::read(lib.wave[which], phase, mip);
    const auto &saw = lib.wave[2];
    const float pw = p[pulse] * .01f;
    return (Library::read(saw, phase, mip) -
            Library::read(saw, phase + pw, mip)) /
           (2 * std::max(pw, 1 - pw));
  }
  float splineRead(const Library &lib, const Library::Index &index, int channel, float fade, float gain) const {
    const auto at = [&](int event) {
      const int palette = splinePalette[channel][event];
      const auto get = [&](int count) {
        const auto &tables = lib.spline[count][palette];
        if (splineCurve == 1) return Library::read(tables[1], index, lod);
        const float x = Library::read(tables[0], index, lod);
        return splineCurve == 0 ? x : x + (Library::read(tables[1], index, lod) - x) * splineCurve;
      };
      if (splineCountMix == 1) return get(splineCount + 1);
      const float x = get(splineCount);
      return splineCountMix == 0 ? x : x + (get(splineCount + 1) - x) * splineCountMix;
    };
    const float first = at(0), second = at(1);
    return (first + (second - first) * fade) * gain;
  }
  float splinePlannedRead(const Library::Index& index,int channel,float fade,float gain)const {
    const float first=splinePlans[channel][0].read(index),second=splinePlans[channel][1].read(index);
    return (first+(second-first)*fade)*gain;
  }
  void setTarget(const Config &config) {
    if (target.v != config.v) { target = config; smoothing = true; }
  }
  std::array<float, 2> tick(const Library &lib, const Config &config,
                            float noteHz, float sr, bool targetReady = false) {
    if (!targetReady) setTarget(config);
    const bool moving = smoothing || std::abs(noteHz - hz) > 1e-5f;
    if (smoothing) {
      smoothing = false;
      for (int f = 0; f < fields; ++f) {
        const float delta = target.v[f] - current.v[f];
        if (discrete(f) || std::abs(delta) < 1e-5f)
          current.v[f] = target.v[f];
        else {
          const float next = current.v[f] + delta * controlSlew;
          // Stop when float resolution prevents another slew step. Without
          // this, large values can leave coefficient refreshes running forever.
          if (next == current.v[f])
            current.v[f] = target.v[f];
          else {
            current.v[f] = next;
            smoothing = true;
          }
        }
      }
    }
    if (std::abs(noteHz - hz) < 1e-5f)
      hz = noteHz;
    else {
      const float next = hz + (noteHz - hz) * pitchSlew;
      hz = next == hz ? noteHz : next;
    }
    if (countdown-- <= 0) {
      if (moving)
        coefficients(sr);
      // Unused hit/pitch envelopes otherwise stick at subnormal values during
      // long held tones. Retire them far below the audible/noise floor.
      for(float* value:{&body,&pitchEnv,&scanEnv,&noiseEnv,&hitEnv})
        if(*value<1e-20f)*value=0;
      countdown = 15;
    }
    const auto &p = current.v;
    const int variant = int(p[type]);
    const bool hit = p[art] > .5f;
    std::array<float, 2> out{};
    const float internalRate = sr * oversample, inv = 1 / internalRate;
    const float driftPosition = float(driftPhase),
                d = driftPosition * driftPosition * (3 - 2 * driftPosition);
    if (variant == 1 && cachedDriftSerial != driftSerial) {
      cachedDriftSerial = driftSerial;
      driftValues = {Library::random(identity ^ driftSerial),
                     Library::random(identity ^ (driftSerial + 1))};
    }
    const float driftSemitones = variant == 1
      ? p[drift] * (2 * (driftValues[0] * (1-d) + driftValues[1] * d) - 1) : 0;
    const float semitones =
        p[tune] + driftSemitones + (variant == 2 ? p[bend] * pitchEnv : 0);
    if (semitones != lastSemitones) {
      frequencyRatio = lib.pitchRatio(semitones);
      lastSemitones = semitones;
    }
    const float tuneRatio = frequencyRatio;
    const float frequency = std::clamp(hz * tuneRatio, 5.f, sr * .4f),
                fa = frequency / detuneRatio,
                fb = std::min(sr * .43f, frequency * detuneRatio *
                                             (variant == 2 ? p[ratio] : 1));
    const float width = p[s3g_thru_table::width] * .01f,
                noiseGain = p[noise] * .01f,
                drive = p[s3g_thru_table::drive] * .01f,
                compression =
                    variant == 2 ? p[s3g_thru_table::compression] * .01f : 0;

    if (variant == 1) prepareSpline(true);
    // Smooth edits/LFO sweeps keep the original direct reader; building a
    // weighted plan on every smoothing sample would cost more than it saves.
    splinePlansReady=variant==1 && current.v[points]==target.v[points] && current.v[curve]==target.v[curve];
    const float evolutionStep = frequency * inv / std::max(1.f, p[evolution]);
    const float driftStep = inv / std::max(.001f, p[drift_rate] * .001f);
    for (int os = 0; os < oversample; ++os) {
      float a = 0, b = 0;
      const bool useNoise = noiseGain > 0 || (variant == 2 && p[impact] > 0 && hitEnv > 1e-8f);
      // Keep output noise density comparable to the former 4x renderer.
      constexpr float noiseScale = .70710678119f;
      const float w = useNoise ? white() * noiseScale : 0;
      if (variant == 0) {
        a = waveRead(lib, int(p[shape]), phaseA, lod, p);
        b = phaseA == phaseB ? a : waveRead(lib, int(p[shape]), phaseB, lod, p);
      } else if (variant == 1) {
        prepareSpline();if(splinePlansReady)prepareSplinePlans(lib);
        const float fade = float(evolve * evolve * (3 - 2 * evolve));
        const float gain = 1 - p[dynamics] * .01f *
          (.25f + .75f * (splineDynamics[0] * (1 - fade) + splineDynamics[1] * fade));
        const Library::Index index(phaseA);
        if(splinePlansReady){
          a=splinePlannedRead(index,0,fade,gain);
          b=width>0?splinePlannedRead(phaseA==phaseB?index:Library::Index(phaseB),1,fade,gain):a;
        }else{
          a = splineRead(lib, index, 0, fade, gain);
          b = width > 0 ? splineRead(lib, phaseA == phaseB ? index : Library::Index(phaseB), 1, fade, gain) : a;
        }
      }

      const float subLevel =
          (variant == 2 ? p[wt_sub] : variant == 0 ? p[sub] : 0) * .01f;
      const float subSound =
          subLevel > 0 ? Library::read(lib.wave[0], subPhase, 0) * subLevel * .6f : 0;
      const float envelope = hit ? body : 1,
                  noiseEnvelope = hit ? (variant == 2 ? noiseEnv : body) : 1;
      const bool sharedDrum = variant == 2 && detuneRatio == 1 &&
        drumPhaseA[0] == drumPhaseA[1] && drumPhaseB[0] == drumPhaseB[1];
      float firstDrumA = 0, firstDrumB = 0;
      for (int ch = 0; ch < 2; ++ch) {
        const float n = useNoise ? w + (white() * noiseScale - w) * width * .3f : 0;
        if (useNoise) {
          pink[ch] += (n - pink[ch]) * pinkSlew;
          low[ch] += (n - low[ch]) * .025f;
        }
        const int col = variant == 2 ? int(p[color]) : 0;
        const float no =
            col == 1
                ? pink[ch] * 2.5f
                : col == 2
                      ? n - low[ch]
                      : col == 3 ? n * (variant == 2
                                            ? Library::read(lib.wave[0],
                                                            drumPhaseB[ch], 0)
                                            : b)
                                 : n;
        float drumA = 0, drumB = 0;
        if (variant == 2) {
          const float scanPosition = p[scan] * .01f * scanEnv;
          if (ch && sharedDrum) { drumA = firstDrumA; drumB = firstDrumB; }
          else {
          drumB = Library::drumRead(
              lib.drum[int(p[bank_b])].values, drumPhaseB[ch],
              std::clamp(p[pos_b] * .01f + scanPosition, 0.f, 1.f), drumLodB);
          drumA = Library::drumRead(
              lib.drum[int(p[bank_a])].values,
              drumPhaseA[ch] +
                  p[fm] * .0025f * drumB * (.25f + .75f * pitchEnv),
              std::clamp(p[pos_a] * .01f + scanPosition, 0.f, 1.f), drumLodA);
          firstDrumA = drumA; firstDrumB = drumB;
          }
          const float pitch =
              ch ? frequency * detuneRatio : frequency / detuneRatio;
          drumPhaseA[ch] += pitch * inv;
          drumPhaseA[ch] -= std::floor(drumPhaseA[ch]);
          drumPhaseB[ch] += std::min(sr * .43f, pitch * p[ratio]) * inv;
          drumPhaseB[ch] -= std::floor(drumPhaseB[ch]);
        }
        const float center = variant == 2
                                 ? drumA + (drumB - drumA) * p[blend] * .01f
                                 : (a + b) * .5f;
        float x = (center + (variant == 2 ? 0 : (ch ? b : a) - center) * width +
                   subSound) *
                      envelope +
                  no * noiseGain * noiseEnvelope * .65f;
        if (variant == 2)
          x += w * hitEnv * p[impact] * .0042f;
        x = filters[ch].tick(x);
        if (drive > 1e-5f) {
          const float gain = drive * 7;
          x = x * (1 + gain) / (1 + gain * std::abs(x));
        }
        if (compression > 1e-5f) {
          const float above =
              std::max(0.f, std::abs(x) - (.8f - .6f * compression));
          x /= 1 + compression * 5 * above;
        }
        const float high = x - last[ch] + .99940009f * dc[ch];
        last[ch] = x;
        dc[ch] = high;
        out[ch] = decimator[ch].tick(high) * p[level] * .006f;
      }
      phaseA += fa * inv;
      phaseA -= std::floor(phaseA);
      phaseB += fb * inv;
      phaseB -= std::floor(phaseB);
      subPhase += frequency * .5f * inv;
      subPhase -= std::floor(subPhase);
      evolve += evolutionStep;
      if (evolve >= 1) {
        evolve -= std::floor(evolve);
        ++serial;
      }
      driftPhase += driftStep;
      if (driftPhase >= 1) {
        driftPhase -= std::floor(driftPhase);
        ++driftSerial;
      }
      body *= bodyPole;
      pitchEnv *= pitchPole;
      scanEnv *= scanPole;
      noiseEnv *= noisePole;
      hitEnv *= hitPole;
    }
    return out;
  }
};
struct Voice {
  State current{}, outgoing{};
  Config frozen{};
  bool initialized = false;
  int fade = 0, fadeFrames = 1;
  void reset() {
    initialized = false;
    fade = 0;
  }
  static bool changed(const Config &a, const Config &b) {
    for (int f : {type, art, shape, bank_a, bank_b, seed})
      if (a.v[f] != b.v[f])
        return true;
    return false;
  }
  void setTarget(const Config &p) {
    if (!initialized) return;
    if (!fade && changed(current.target, p)) {
      outgoing = current;
      frozen = outgoing.target;
      fade = fadeFrames;
    }
    current.setTarget(p);
  }
  std::array<float, 2> tick(const Library &lib, const Config &p, float noteHz,
                            float sr, std::uint32_t id, double age,
                            double phase, bool targetReady = false) {
    if (!initialized) {
      current.reset(p, noteHz, sr, id, age, phase);
      initialized = true;
      fadeFrames = std::max(1, int(sr * .008f));
    } else if (!targetReady) setTarget(p);
    auto out = current.tick(lib, p, noteHz, sr, true);
    if (fade) {
      const auto old = outgoing.tick(lib, frozen, noteHz, sr, true);
      const float t = 1 - float(fade) / fadeFrames, w = t * t * (3 - 2 * t);
      for (int c = 0; c < 2; ++c)
        out[c] = old[c] + (out[c] - old[c]) * w;
      --fade;
    }
    return out;
  }
};
struct Instrument {
  const Library *library = nullptr;
  std::array<Voice, 2> voices{};
  std::array<int, 2> lanes{-1, -1};
  std::array<float, 4> laneOutput{};
  double age = 0, phase = 0;
  float position = 0, lastSlew = -1, positionSlew = 0;
  bool started = false, blockPrepared = false;
  std::uint32_t identity = 1;
  void prepare() { library = &Library::get(); }
  void reset(std::uint32_t id = 1) {
    for (auto &v : voices)
      v.reset();
    lanes = {-1, -1};
    age = phase = 0;
    started = blockPrepared = false;
    identity = id;
  }
  void beginBlock(const std::array<Config, 4> &patches) {
    for (int slot = 0; slot < 2; ++slot)
      if (lanes[slot] >= 0) voices[slot].setTarget(patches[lanes[slot]]);
    blockPrepared = true;
  }
  void endBlock() { blockPrepared = false; }
  std::array<float, 2> tick(const std::array<Config, 4> &patches,
                            float targetPosition, float hz, float sr,
                            float slewMs = 5, const float *gains = nullptr) {
    laneOutput = {};
    if (!started) {
      position = targetPosition;
      started = true;
    }
    if (lastSlew != slewMs) {
      lastSlew = slewMs;
      positionSlew = 1 - std::exp(-1 / (sr * std::max(.005f, slewMs * .001f)));
    }
    position += (targetPosition - position) * positionSlew;
    position = std::clamp(position, 0.f, 3.f);
    const int first = int(position), second = std::min(3, first + 1);
    const float t = position - first, w = t * t * (3 - 2 * t);
    std::array<float, 2> result{};
    for (int i = 0; i < 2; ++i) {
      const int lane = i ? second : first;
      const float gain = (i ? w : 1 - w) * (gains ? gains[lane] : 1.f);
      if (gain < .000001f)
        continue;
      int slot = lanes[0] == lane ? 0 : lanes[1] == lane ? 1 : -1;
      if (slot < 0) {
        slot = lanes[0] != first && lanes[0] != second ? 0 : 1;
        lanes[slot] = lane;
        voices[slot].reset();
      }
      const auto out = voices[slot].tick(
          *library, patches[lane], hz, sr,
          Library::hash(identity ^ std::uint32_t(lane)), age, phase, blockPrepared);
      laneOutput[lane] += out[0] * gain;
      for (int ch = 0; ch < 2; ++ch)
        result[ch] += out[ch] * gain;
    }
    age += 1 / sr;
    phase += hz / sr;
    phase -= std::floor(phase);
    return result;
  }
};
} // namespace s3g_thru_table
