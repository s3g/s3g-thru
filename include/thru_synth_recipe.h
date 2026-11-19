#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace s3g_thru_synth {
enum Field {
  length,
  root,
  width,
  attack,
  release,
  level,
  points,
  evolution,
  drift,
  driftRate,
  curvature,
  dynamics,
  loop,
  fields
};
inline constexpr std::array<float, fields> minimum{.04f, 24, 0, 0, 0, 0, 1,
                                                   1,    0,  1, 0, 0, 0};
inline constexpr std::array<float, fields> maximum{
    12, 96, 1, 2000, 4000, 1, 32, 4096, 12, 1000, 1, 1, 1};
inline constexpr std::array<float, fields> defaults{
    2, 60, 0, 5, 20, .75f, 4, 16, .15f, 100, 1, .2f, 0};
inline constexpr std::array<float, 4> curveLimits{24, 16, 4, 24};
struct Point {
  float time = 0, value = 0;
  int shape = 2;
}; // Hold, Linear, Smooth.
struct Curve {
  std::array<Point, 8> p{};
  std::uint32_t enabled = 0x81;
  Curve() {
    for (int i = 0; i < 8; ++i)
      p[i].time = float(i) / 7;
  }
};
enum Generator {
  spline,
  wave,
  bloom,
  resynth,
  partialsType,
  noiseBank,
  fm,
  convolution,
  drum,
  pluck,
  subharmonic,
  hybrid,
  dust,
  generatorCount
};
inline const char *generatorName(int type) {
  static constexpr const char *names[] = {
      "Spline",      "Wave",   "Bloom",    "Resynth", "Partials",
      "Noise Bank",  "FM",     "Convolve", "Drum",    "Pluck",
      "Subharmonic", "Hybrid", "Dust"};
  return type >= 0 && type < generatorCount ? names[type] : names[0];
}
enum WaveField {
  wShape,
  wPulse,
  wDetune,
  wSub,
  wCutoff,
  wResonance,
  wNoise,
  waveFields
};
inline constexpr std::array<float, waveFields> waveMin{0, .05f, 0, 0, 20, 0, 0};
inline constexpr std::array<float, waveFields> waveMax{3,     .95f, 50, 1,
                                                       20000, .95f, 1};
inline constexpr std::array<float, waveFields> waveDefaults{0,     .5f, 0, 0,
                                                            20000, 0,   0};
enum BloomField {
  bMode,
  bSpectrum,
  bCount,
  bGap,
  bScatter,
  bRise,
  bSustain,
  bDecay,
  bRiseVar,
  bSustainVar,
  bDecayVar,
  bPitchJitter,
  bAmpJitter,
  bBend,
  bOctave,
  bShape,
  bloomFields
};
inline constexpr std::array<float, bloomFields> bloomMin{
    0, 0, 1, 50, 0, 2, 0, 20, 0, 0, 0, 0, 0, 0, 0, .25f};
inline constexpr std::array<float, bloomFields> bloomMax{
    2, 3, 8, 1000, 1, 200, 200, 2000, 1, 1, 1, 1, 1, 1, 1, 4};
inline constexpr std::array<float, bloomFields> bloomDefaults{
    0, 0, 8, 150, .2f, 10, 20, 120, .2f, .2f, .2f, 0, .1f, 0, 0, 1};
enum ModField {
  mShape,
  mRate,
  mAmount,
  mDestination,
  mFade,
  mPhase,
  mFit,
  modFields
};
inline constexpr std::array<float, modFields> modMin{0, .01f, 0, 0, 0, 0, 0};
inline constexpr std::array<float, modFields> modMax{4, 64, 1, 4, 12000, 1, 1};
inline constexpr std::array<float, modFields> modDefaults{0, .5f, 0, 1,
                                                          0, 0,   0};
enum ResynthField {
  rsMode,
  rsPitch,
  rsStart,
  rsEnd,
  rsBlur,
  rsSmooth,
  rsPartials,
  rsFloor,
  resynthFields
};
inline constexpr std::array<float, resynthFields> resynthMin{0, -24, 0, .01f,
                                                             0, 0,   4, -96};
inline constexpr std::array<float, resynthFields> resynthMax{1, 24, .99f, 1,
                                                             1, 1,  64,   -6};
inline constexpr std::array<float, resynthFields> resynthDefaults{
    0, 0, 0, 1, 0, 0, 24, -96};
enum AdditiveField {
  aCount,
  aTilt,
  aStretch,
  aDetune,
  aPhase,
  aDrift,
  aDriftRate,
  aDecay,
  additiveFields
};
inline constexpr std::array<float, additiveFields> additiveMin{1, -24, .25f, 0,
                                                               0, 0,   .01f, 0};
inline constexpr std::array<float, additiveFields> additiveMax{32, 12, 4,  30,
                                                               1,  1,  20, 1};
inline constexpr std::array<float, additiveFields> additiveDefaults{
    16, 0, 1, 0, 0, 0, .5f, 0};
enum NoiseField {
  nCount,
  nBandwidth,
  nTilt,
  nStretch,
  nColor,
  nMotion,
  nRate,
  nLeak,
  noiseFields
};
inline constexpr std::array<float, noiseFields> noiseMin{1, .1f, -24,  .25f,
                                                         0, 0,   .01f, 0};
inline constexpr std::array<float, noiseFields> noiseMax{32, 48, 12, 4,
                                                         2,  24, 20, 1};
inline constexpr std::array<float, noiseFields> noiseDefaults{8, 3, 0,   1,
                                                              0, 0, .5f, 0};
enum FmField {
  fAlgorithm,
  fRatio1,
  fIndex1,
  fRatio2,
  fIndex2,
  fFeedback,
  fDetune,
  fMix,
  fmFields
};
inline constexpr std::array<float, fmFields> fmMin{0, .125f, 0, .125f,
                                                   0, 0,     0, 0};
inline constexpr std::array<float, fmFields> fmMax{3,  32,  12, 32,
                                                   12, .9f, 30, 1};
inline constexpr std::array<float, fmFields> fmDefaults{0, 1, 1, 2, 0, 0, 0, 1};
enum ConvolutionField {
  cStart,
  cLength,
  cIrStart,
  cIrLength,
  cWindow,
  cTone,
  cMix,
  cTail,
  convolutionFields
};
inline constexpr std::array<float, convolutionFields> convolutionMin{
    0, 1, 0, .05f, 0, -1, 0, 0};
inline constexpr std::array<float, convolutionFields> convolutionMax{
    .9999f, 12000, .9999f, 12000, 3, 1, 1, 1};
inline constexpr std::array<float, convolutionFields> convolutionDefaults{
    0, 12000, 0, 1000, 0, 0, 1, 1};
enum DrumField {
  dModel,
  dTune,
  dDecay,
  dBody,
  dNoise,
  dHit,
  dTone,
  dCharacter,
  dBend,
  dSweep,
  dRing,
  dDamping,
  dForm,
  dContactX,
  dContactY,
  dExtent,
  drumFields
};
inline constexpr std::array<float, drumFields> drumMin{
    0, 18, 30, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0};
inline constexpr std::array<float, drumFields> drumMax{
    10, 3200, 12000, 1, 1, 1, 1, 1, 48, 500, 16, 1, 15, 1, 1, 1};
inline constexpr std::array<float, drumFields> drumDefaults{
    0, 48, 850, .25f, .04f, .2f, .55f, 0, 24, 45, .1f, .78f, 0, .6f, .4f, .7f};
inline constexpr int drumModelCount = 11;
enum DrumAttackField {
  xPunch,
  xHitDecay,
  xNoiseDecay,
  xExciter,
  xBias,
  xCompression,
  xSnap,
  xEdge
};
enum DrumColorField { cBits, cRate, cLowpass, cBlend };
inline constexpr std::array<float, 8> colorMin{4, 375, 20, 0, 0, 0, 0, 0},
    colorMax{24, 48000, 20000, 1, 0, 0, 0, 0},
    colorDefaults{24, 48000, 20000, 1, 0, 0, 0, 0};
enum DustField {
  uMode,
  uRate,
  uSize,
  uClump,
  uTone,
  uEdge,
  uBits,
  uVariation,
  uFloor,
  uColor,
  uRepeat,
  uFault,
  dustFields
};
inline constexpr std::array<float, dustFields> dustMin{0, .1f, .02f, 0, 20, 0,
                                                       2, 0,   0,    0, 1,  0},
    dustMax{5, 2000, 500, 1, 20000, 1, 24, 1, 1, 3, 16, 1},
    dustDefaults{0, 20, 1.2f, .2f, 10000, 0, 16, .75f, 0, 0, 1, .25f};
enum PluckField {
  pTune,
  pDecay,
  pDamping,
  pBrightness,
  pDispersion,
  pPick,
  pHardness,
  pDetune
};
enum SubField {
  sHarmonics,
  sSubs,
  sBalance,
  sTilt,
  sDetune,
  sDrift,
  sRate,
  sRegister
};
enum EventTimingField { eClock, eBpm, eDivision, eGate, eSwing };
inline constexpr std::array<float, 14> eventBeats{
    4,    2,       4.f / 3, 1,        2.f / 3, .5f,  1.f / 3,
    .25f, 1.f / 6, .125f,   1.f / 12, .0625f,  .75f, .375f};
enum HybridField {
  hMode,
  hSources,
  hPitch,
  hDensity,
  hMutation,
  hTexture,
  hLife,
  hDrift
};
enum TraceField {
  tMode,
  tDensity,
  tLife,
  tDrift,
  tJitter,
  tTolerance,
  tFreeze,
  tSpread
};
enum EventField {
  eMode,
  eCount,
  eDensity,
  eJitter,
  ePitch,
  eLife,
  eVariation,
  ePan
};
inline constexpr std::array<float, 8> pluckMin{20, 30, 0, 0, 0, 0, 0, 0},
    pluckMax{2000, 8000, .98f, 1, 1, 1, 1, 50},
    pluckDefaults{110, 1200, .35f, .65f, .08f, .35f, .1f, 8},
    subMin{1, 0, 0, -18, 0, 0, .01f, -2}, subMax{16, 16, 1, 6, 30, 30, 4, 2},
    subDefaults{8, 4, .25f, -6, 0, 0, .25f, 0},
    hybridMin{0, 2, -24, 0, 0, 0, 20, 0}, hybridMax{4, 4, 24, 1, 1, 1, 2000, 1},
    hybridDefaults{0, 2, 0, .8f, .65f, .35f, 140, .1f},
    traceMin{0, 0, 10, 0, 0, 10, 0, 0}, traceMax{3, 1, 2000, 1, 1, 600, 1, 1},
    traceDefaults{0, 1, 120, 0, 0, 144, .5f, 0},
    eventMin{0, 1, 0, 0, 0, 40, 0, 0}, eventMax{4, 64, 1, 1, 48, 4000, 1, 1},
    eventDefaults{0, 8, 1, 0, 0, 500, 0, .35f},
    timingMin{0, 1, 0, .05f, 0, 0, 0, 0},
    timingMax{1, 999, 13, 4, .75f, 0, 0, 0},
    timingDefaults{1, 120, 5, .7f, 0, 0, 0, 0},
    attackMin{0, .25f, 1, 0, -1, 0, 0, .05f},
    attackMax{1, 4000, 12000, 1, 1, 1, 1, 2},
    attackDefaults{.94f, 4, 80, .12f, 0, 0, .35f, .15f};
inline bool supportsEvents(int type) {
  return type == drum || type == bloom || type == partialsType || type == pluck;
}

inline constexpr int detailLockCount = 62;
using Spectrum = std::array<float, 8>;
inline constexpr std::array<Spectrum, 4> ratioDefaults{
    {{1, 2, 3, 4, 5, 6, 7, 8},
     {1, 2, 3, 4, 5, 6, 7, 8},
     {1, 1.414f, 2.76f, 3.414f, 4.8f, 6.1f, 8.3f, 11},
     {1, 2.01f, 3.99f, 7.02f, 9.17f, 12.3f, 17.2f, 24.1f}}};
inline constexpr std::array<Spectrum, 4> partialDefaults{
    {{1, .5f, .333f, .25f, .2f, .167f, .143f, .125f},
     {1, 0, -.5f, 0, .3f, 0, -.2f, 0},
     {1, .7f, .4f, .3f, .2f, .17f, .14f, .1f},
     {1, .6f, .4f, .3f, .2f, .15f, .1f, .07f}}};

struct Recipe {
  std::array<float, fields> v = defaults;
  std::uint32_t
      seed = 1,
      locks = 0x113f; // Protect setup, envelope, level, pitch drift and LOOP.
  std::uint16_t scale = 0xfff;
  std::array<Curve, 4> curves{};
  int type = spline;
  std::array<float, waveFields> osc = waveDefaults;
  std::array<float, bloomFields> packets = bloomDefaults;
  std::array<float, modFields> mod = modDefaults;
  std::array<float, resynthFields> spectral = resynthDefaults;
  std::array<float, additiveFields> additive = additiveDefaults;
  std::array<float, noiseFields> noise = noiseDefaults;
  std::array<float, fmFields> fmOsc = fmDefaults;
  std::array<float, convolutionFields> conv = convolutionDefaults;
  std::array<float, drumFields> drums = drumDefaults;
  std::uint32_t extendedLocks = 0x10f;
  std::uint32_t drumFlags = 0; // Bit 0 preserves the older dual TONE routing.
  std::array<float, 8> plucked = pluckDefaults, sub = subDefaults,
                       hybridParams = hybridDefaults, trace = traceDefaults,
                       events = eventDefaults, timing = timingDefaults,
                       drumExtra = attackDefaults, drumColor = colorDefaults;
  std::array<float, dustFields> dustParams = dustDefaults;
  std::uint32_t dustLocks =
      1; // Keep the selected artifact mode during mutation.
  std::uint64_t workshopLocks = (1ULL << 0) | (1ULL << 8) | (1ULL << 16) |
                                (1ULL << 17) | (1ULL << 24) | (1ULL << 32) |
                                (1ULL << 9) | (1ULL << 33) | (31ULL << 40) |
                                (3ULL << 54) | (15ULL << 56);
  std::array<Spectrum, 4> ratios = ratioDefaults, partials = partialDefaults;
  std::uint64_t detailLocks = (1u << 0) | (3u << 7) | (1u << 23) | (1u << 25) |
                              (1u << 26) | (1u << 29) | (1ULL << 38) |
                              (1ULL << 46) | (1ULL << 54);
};
inline void initializeDrumModel(Recipe &r, int model) {
  static constexpr std::array<std::array<float, 16>, 11> starts{
      {{0, 48, 850, .25f, .04f, .2f, .55f, 0, 24, 45, .1f, .78f, 0, .6f, .4f,
        .7f},
       {1, 180, 420, .58f, .7f, .1f, .56f, 0, 7, 22, .32f, .48f, 0, .6f, .4f,
        .7f},
       {2, 72, 700, .7f, 0, .35f, .58f, 0, 8, 45, .42f, .35f, 0, .6f, .4f, .7f},
       {3, 560, 240, .62f, .16f, .62f, .58f, 0, 0, 30, .32f, .46f, 0, .6f, .4f,
        .7f},
       {4, 1180, 180, .72f, .58f, .52f, .62f, 0, 0, 40, .28f, .45f, 0, .6f, .4f,
        .7f},
       {5, 720, 1300, .4f, .74f, .72f, .68f, 0, 0, 40, .2f, .44f, 0, .6f, .4f,
        .7f},
       {6, 43, 1800, .85f, 0, .25f, .5f, 0, 24, 42, 0, .4f, 0, .6f, .4f, .7f},
       {7, 180, 1800, .5f, .3f, .6f, .5f, 0, 0, 40, .35f, .4f, 0, .6f, .4f,
        .5f},
       {8, 343, 1800, .25f, .15f, .7f, .5f, 0, 0, 40, .35f, .4f, 0, .6f, .4f,
        .7f},
       {9, 44, 3200, .88f, .62f, .72f, .38f, 0, .56f, .45f, .5f, .22f, 0, .2f,
        .28f, .55f},
       {10, 80, 650, .8f, .025f, .3f, .7f, 0, 24, 30, 1.5f, .12f, 0, .35f, .35f,
        0}}};
  static constexpr std::array<std::array<float, 8>, 11> extra{
      {{.94f, 4, 80, .12f, 0, 0, .35f, .15f},
       {.94f, 6.5f, 340, .18f, 0, 0, .35f, .15f},
       {.94f, 4.5f, 120, .25f, 0, 0, .35f, .15f},
       {.68f, 180, 35, .08f, 0, 0, .35f, .15f},
       {.85f, 6.5f, 120, .62f, 0, 0, .35f, .15f},
       {.58f, 6.5f, 1450, .58f, 0, 0, .35f, .15f},
       {.72f, 6.5f, 120, .25f, 0, 0, .35f, .15f},
       {.72f, 6.5f, 120, .25f, 0, 0, .35f, .15f},
       {.72f, 6.5f, 120, .25f, 0, 0, .35f, .15f},
       {.65f, 320, 140, .35f, 0, 0, .35f, .15f},
       {.8f, 3, 120, 0, 0, 0, .35f, .15f}}};
  if (model < 0 || model >= drumModelCount)
    return;
  r.drumFlags = 0;
  r.drums = starts[model];
  r.drumExtra = extra[model];
}
inline std::array<float, 8> legacyDrumAttack(const Recipe &r) {
  std::array<float, 8> a{.72f, 6.5f, 120, .25f, 0, r.drums[dCharacter] * .25f,
                         0,    2};
  switch (int(r.drums[dModel])) {
  case 1:
    a[0] = .78f;
    a[2] = std::clamp(r.drums[dDecay] * .8f, 10.f, 4000.f);
    a[3] = .18f;
    break;
  case 2:
    a[1] = 4.5f;
    break;
  case 3:
    a[0] = .68f;
    a[1] = std::clamp(r.drums[dDecay] * .7f, 20.f, 1500.f);
    a[2] = 35;
    a[3] = .08f;
    break;
  case 4:
    a[0] = .66f;
    a[3] = .62f;
    break;
  case 5:
    a[0] = .58f;
    a[2] = r.drums[dDecay];
    a[3] = .58f;
    break;
  }
  return a;
}
inline bool drumAttackAvailable(const Recipe &r, int f) {
  const int model = int(r.drums[dModel]);
  if (f < 0 || f > 7)
    return false;
  if (f == xPunch)
    return model < 6 || model >= 9;
  if (f == xHitDecay)
    return model == 0 || model == 2 || model == 3 || model == 10 ||
           (model == 9 && r.drums[dForm] == 1);
  if (f == xNoiseDecay)
    return model == 0 || model == 1 || model == 3 || model == 5 ||
           model == 10 || (model == 9 && r.drums[dForm] == 2);
  if (f == xExciter)
    return (model < 6 && model != 2) || (model == 9 && r.drums[dForm] == 2);
  if (f == xBias)
    return model != 6 && model != 8 && r.drums[dCharacter] > 0;
  return true;
}
inline std::array<float, 2> drumAttackRange(int model, int field) {
  if (field == xHitDecay) {
    if (model == 2)
      return {.5f, 40};
    if (model == 3)
      return {20, 1500};
    if (model == 9)
      return {40, 1500};
    return {.25f, 40};
  }
  if (field == xNoiseDecay) {
    if (model == 3)
      return {4, 400};
    if (model == 5)
      return {80, 8000};
    if (model == 9)
      return {15, 1000};
    return {10, 4000};
  }
  return {attackMin[field], attackMax[field]};
}
inline void initializeSubBank(Recipe &r) {
  for (int i = 0; i < 32; ++i) {
    r.ratios[i / 8][i % 8] = float(i < 16 ? i + 1 : i - 14);
    r.partials[i / 8][i % 8] = 1;
  }
}
inline double eventInterval(const Recipe &r) {
  return r.timing[eClock] > .5f
             ? 60.0 / r.timing[eBpm] * eventBeats[int(r.timing[eDivision])]
             : double(r.v[length]) / r.events[eCount];
}
inline std::size_t eventWindowFrames(const Recipe &r, float rate) {
  const double seconds = r.timing[eClock] > .5f
                             ? eventInterval(r) * r.timing[eGate]
                             : r.events[eLife] * .001;
  return std::max<std::size_t>(
      2, std::size_t(std::llround(std::min(12.0, seconds) * rate)));
}
inline void initializeGenerator(Recipe &r, int type) {
  r.type = type;
  if (type == drum)
    initializeDrumModel(r, 0);
  if (type == pluck) {
    r.events[eMode] = 1;
    r.v[attack] = 0;
    r.v[release] = 10;
  }
  if (type == subharmonic)
    initializeSubBank(r);
  if (type == partialsType || type == noiseBank)
    for (int i = 0; i < 32; ++i) {
      r.ratios[i / 8][i % 8] = float(i + 1);
      r.partials[i / 8][i % 8] =
          type == partialsType ? 1.f / float(i + 1) : 1.f;
    }
}
inline std::array<float, 4> limits(const Recipe &r) {
  if (r.type == dust)
    return {4, 4, 4, 24};
  if (r.type == subharmonic)
    return {24, 1, 12, 24};
  if (r.type == convolution || r.type == drum || r.type == pluck ||
      r.type == subharmonic || r.type == hybrid)
    return {24, 1, 1, 24};
  if (r.type == partialsType)
    return {24, 31, 2, 24};
  if (r.type == noiseBank)
    return {24, 4, 24, 24};
  if (r.type == fm)
    return {24, 12, 12, 24};
  return r.type == wave
             ? std::array<float, 4>{24, .45f, 4, 24}
             : r.type == bloom
                   ? std::array<float, 4>{24, 7, 4, 24}
                   : r.type == resynth ? std::array<float, 4>{24, 1, 1, 24}
                                       : curveLimits;
}
inline bool curveAvailable(const Recipe &r, int row) {
  if ((r.type == convolution || (r.type == drum && r.events[eMode] == 0)) &&
      row != 3)
    return false;
  return row >= 0 && row < 4 &&
         !(r.type == wave && row == 1 && r.osc[wShape] != 3) &&
         !(r.type == fm && row == 2 && r.fmOsc[fAlgorithm] == 0);
}
inline bool modActive(const Recipe &r) {
  return r.mod[mAmount] > 0 && r.mod[mDestination] > 0 &&
         curveAvailable(r, int(r.mod[mDestination]) - 1);
}
inline bool toneActive(const Recipe &r) {
  if (r.osc[wCutoff] < 19999)
    return true;
  if (modActive(r) && r.mod[mDestination] == 3)
    return true;
  for (int i = 0; i < 8; ++i)
    if ((r.curves[2].enabled & (1u << i)) && r.curves[2].p[i].value < 0)
      return true;
  return false;
}
inline bool waveAvailable(const Recipe &r, int f) {
  return f >= 0 && f < waveFields && (f != wPulse || r.osc[wShape] == 3) &&
         (f != wResonance || toneActive(r));
}
inline bool bloomAvailable(const Recipe &r, int f) {
  return f >= 0 && f < bloomFields && (f != bSpectrum || r.packets[bMode] == 0);
}
inline bool modAvailable(const Recipe &r, int f) {
  if (f == mDestination)
    return true;
  if (f == mAmount)
    return r.mod[mDestination] > 0 &&
           curveAvailable(r, int(r.mod[mDestination]) - 1);
  return f >= 0 && f < modFields && modActive(r);
}
inline bool setWave(Recipe &r, int f, float v) {
  if (f < 0 || f >= waveFields || !std::isfinite(v))
    return false;
  r.osc[f] =
      std::clamp(f == wShape ? std::round(v) : v, waveMin[f], waveMax[f]);
  return true;
}
inline bool setBloom(Recipe &r, int f, float v) {
  if (f < 0 || f >= bloomFields || !std::isfinite(v))
    return false;
  r.packets[f] = std::clamp(f == bMode || f == bSpectrum ? std::round(v) : v,
                            bloomMin[f], bloomMax[f]);
  return true;
}
inline bool setMod(Recipe &r, int f, float v) {
  if (f < 0 || f >= modFields || !std::isfinite(v))
    return false;
  r.mod[f] = std::clamp(
      f == mShape || f == mDestination || f == mFit ? std::round(v) : v,
      modMin[f], modMax[f]);
  if (r.mod[mFit] > .5f)
    r.mod[mRate] = std::clamp(std::round(r.mod[mRate]), 1.f, 64.f);
  else
    r.mod[mRate] = std::clamp(r.mod[mRate], .01f, 40.f);
  return true;
}
inline bool resynthAvailable(const Recipe &r, int f) {
  return f >= 0 && f < resynthFields &&
         (f != rsPartials || r.spectral[rsMode] == 1);
}
inline bool setResynth(Recipe &r, int f, float v) {
  if (f < 0 || f >= resynthFields || !std::isfinite(v))
    return false;
  r.spectral[f] = std::clamp(f == rsMode || f == rsPartials ? std::round(v) : v,
                             resynthMin[f], resynthMax[f]);
  if (f == rsStart)
    r.spectral[rsEnd] = std::max(r.spectral[rsEnd], r.spectral[rsStart] + .01f);
  if (f == rsEnd)
    r.spectral[rsStart] =
        std::min(r.spectral[rsStart], r.spectral[rsEnd] - .01f);
  return true;
}
inline bool additiveAvailable(const Recipe &r, int f) {
  return f >= 0 && f < additiveFields &&
         (f != aDriftRate || r.additive[aDrift] > 0);
}
inline bool noiseAvailable(const Recipe &r, int f) {
  if (f != nRate)
    return f >= 0 && f < noiseFields;
  if (r.noise[nMotion] > 0 || (modActive(r) && r.mod[mDestination] == 3))
    return true;
  for (int i = 0; i < 8; ++i)
    if ((r.curves[2].enabled & (1u << i)) && r.curves[2].p[i].value > 0)
      return true;
  return false;
}
inline bool fmAvailable(const Recipe &r, int f) {
  return f >= 0 && f < fmFields &&
         (r.fmOsc[fAlgorithm] != 0 || (f != fRatio2 && f != fIndex2));
}
template <std::size_t N>
inline bool setGeneratorField(std::array<float, N> &values, int f, float v,
                              const std::array<float, N> &low,
                              const std::array<float, N> &high,
                              int discrete = -1) {
  if (f < 0 || f >= int(N) || !std::isfinite(v))
    return false;
  values[f] = std::clamp(f == discrete ? std::round(v) : v, low[f], high[f]);
  return true;
}
inline bool dustAvailable(const Recipe &r, int f) {
  if (f < 0 || f >= dustFields)
    return false;
  return !(f == uFloor && r.dustParams[uMode] == 2) &&
         !(f == uColor && r.dustParams[uMode] == 5 &&
           r.dustParams[uFloor] == 0) &&
         !(f == uFault && r.dustParams[uMode] != 5);
}
inline bool setDust(Recipe &r, int f, float v) {
  if (f < 0 || f >= dustFields || !std::isfinite(v))
    return false;
  if (f == uMode || f == uBits || f == uColor || f == uRepeat)
    v = std::round(v);
  r.dustParams[std::size_t(f)] =
      std::clamp(v, dustMin[std::size_t(f)], dustMax[std::size_t(f)]);
  return true;
}
inline bool setAdditive(Recipe &r, int f, float v) {
  return setGeneratorField(r.additive, f, v, additiveMin, additiveMax);
}
inline bool setNoise(Recipe &r, int f, float v) {
  return setGeneratorField(r.noise, f, v, noiseMin, noiseMax, nColor);
}
inline bool setFm(Recipe &r, int f, float v) {
  return setGeneratorField(r.fmOsc, f, v, fmMin, fmMax, fAlgorithm);
}
inline bool setConvolution(Recipe &r, int f, float v) {
  if (!setGeneratorField(r.conv, f, v, convolutionMin, convolutionMax,
                         f == cWindow ? cWindow : cTail))
    return false;
  return true;
}
inline bool drumAvailable(const Recipe &r, int f) {
  const int model = int(r.drums[dModel]);
  if (model == 10)
    return f >= 0 && f < drumFields;
  if (model == 9)
    return f >= 0 && f < drumFields && !(f == dExtent && r.drums[dForm] != 2) &&
           !(f == dDecay && r.drums[dForm] != 0);
  return f >= 0 && f < drumFields && !(f >= dForm && model < 6) &&
         !(f == dNoise && (model == 2 || model == 6)) &&
         !(f == dRing && model == 6) &&
         !((f == dBend || f == dSweep) &&
           (model == 4 || model == 5 || model >= 7));
}
inline constexpr std::array<float, drumModelCount> drumTuneLow{
    20, 70, 35, 180, 320, 180, 25, 30, 20, 24, 20},
    drumTuneHigh{180, 420, 180, 2400, 3200, 2400, 90, 1200, 2000, 96, 3200},
    drumTuneDefault{48, 180, 72, 560, 1180, 720, 43, 180, 343, 44, 80},
    drumDecayLow{30, 30, 40, 30, 30, 80, 80, 50, 50, 150, 30},
    drumDecayHigh{8000, 3000,  4000,  2000, 1000, 8000,
                  6000, 12000, 12000, 8000, 8000},
    drumBendHigh{48, 36, 30, 24, 48, 48, 48, 48, 48, 1, 48},
    drumSweepHigh{500, 250, 300, 500, 500, 500, 250, 500, 500, 1, 500};
inline bool setDrum(Recipe &r, int f, float v) {
  if (!std::isfinite(r.drums[dModel]) || r.drums[dModel] < 0 ||
      r.drums[dModel] >= drumModelCount ||
      std::round(r.drums[dModel]) != r.drums[dModel])
    return false;
  const int before = int(r.drums[dModel]);
  if (f == dSweep && before == 9) {
    if (!std::isfinite(v))
      return false;
    r.drums[dSweep] = std::clamp(v, 0.f, 1.f);
    return true;
  }
  if (!setGeneratorField(r.drums, f, v, drumMin, drumMax, dModel))
    return false;
  const int model = int(r.drums[dModel]);
  if (f == dModel && model != before)
    initializeDrumModel(r, model);
  if (f == dForm)
    r.drums[dForm] = std::round(r.drums[dForm]);
  r.drums[dForm] =
      std::clamp(r.drums[dForm], 0.f,
                 model == 6 ? 4.f
                            : model == 7 ? 15.f
                                         : model == 8 || model == 10
                                               ? 3.f
                                               : model == 9 ? 2.f : 0.f);

  r.drums[dRing] = std::clamp(r.drums[dRing], model == 10 ? .125f : 0.f,
                              model == 10 ? 16.f : 1.f);
  r.drums[dTune] =
      std::clamp(r.drums[dTune], drumTuneLow[model], drumTuneHigh[model]);
  r.drums[dDecay] =
      std::clamp(r.drums[dDecay], drumDecayLow[model], drumDecayHigh[model]);
  r.drums[dBend] = std::clamp(r.drums[dBend], 0.f, drumBendHigh[model]);
  r.drums[dSweep] =
      std::clamp(r.drums[dSweep],
                 model == 9 ? 0.f : model == 3 ? 3.f : model == 6 ? 5.f : 2.f,
                 drumSweepHigh[model]);
  return true;
}

inline bool setPartial(Recipe &r, int spectrum, int at, int field,
                       float value) {
  if (spectrum < 0 || spectrum > 3 || at < 0 || at > 7 || field < 0 ||
      field > 1 || !std::isfinite(value))
    return false;
  (field ? r.partials[spectrum][at] : r.ratios[spectrum][at]) =
      std::clamp(value, field ? -1.f : 1.f, field ? 1.f : 64.f);
  return true;
}

inline std::uint32_t hash(std::uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}
inline float random(std::uint32_t seed, std::uint32_t at) {
  return float(hash(seed ^ (at * 0x9e3779b9u)) >> 8) / 16777216.f;
}
inline bool set(Recipe &r, int field, float value) {
  if (field < 0 || field >= fields || !std::isfinite(value))
    return false;
  value = std::clamp(value, minimum[field], maximum[field]);
  if (field == root || field == points || field == loop)
    value = std::round(value);
  r.v[field] = value;
  return true;
}
inline std::array<float, 8> &workshopGroup(Recipe &r, int group) {
  return group == 0
             ? r.plucked
             : group == 1
                   ? r.sub
                   : group == 2
                         ? r.hybridParams
                         : group == 3
                               ? r.trace
                               : group == 4
                                     ? r.events
                                     : group == 5 ? r.timing
                                                  : group == 6 ? r.drumExtra
                                                               : r.drumColor;
}
inline const std::array<float, 8> &workshopMinimum(int group) {
  return group == 0
             ? pluckMin
             : group == 1
                   ? subMin
                   : group == 2
                         ? hybridMin
                         : group == 3
                               ? traceMin
                               : group == 4
                                     ? eventMin
                                     : group == 5
                                           ? timingMin
                                           : group == 6 ? attackMin : colorMin;
}
inline const std::array<float, 8> &workshopMaximum(int group) {
  return group == 0
             ? pluckMax
             : group == 1
                   ? subMax
                   : group == 2
                         ? hybridMax
                         : group == 3
                               ? traceMax
                               : group == 4
                                     ? eventMax
                                     : group == 5
                                           ? timingMax
                                           : group == 6 ? attackMax : colorMax;
}
inline bool workshopAvailable(const Recipe &r, int group, int field) {
  if (group < 0 || group > 7 || field < 0 || field > 7)
    return false;
  if (group == 3)
    return r.spectral[rsMode] == 1 &&
           !(field == tLife && r.trace[tMode] == 0) &&
           !(field == tJitter && r.trace[tMode] == 0) &&
           !(field == tTolerance && r.trace[tMode] != 0) &&
           !(field == tFreeze && r.trace[tMode] != 3) &&
           !(field == tSpread && r.v[width] == 0);
  if (group == 4)
    return supportsEvents(r.type) && (field == eMode || r.events[eMode] > 0) &&
           !(field == ePan && r.v[width] == 0) &&
           !(field == eLife && r.timing[eClock] > .5f);
  if (group == 6)
    return r.type == drum && drumAttackAvailable(r, field);
  if (group == 7)
    return r.type == drum && field <= cBlend;
  if (group == 5)
    return supportsEvents(r.type) && r.events[eMode] > 0 && field <= eSwing &&
           (field == eClock || r.timing[eClock] > .5f);
  if (group == 1 && field == sRate)
    return r.sub[sDrift] > 0;
  if (group == 1 && field == sBalance)
    return r.sub[sSubs] > 0;
  return true;
}
inline bool setWorkshop(Recipe &r, int group, int field, float value) {
  if (group < 0 || group > 7 || field < 0 || field > 7 || !std::isfinite(value))
    return false;
  const auto &lo = workshopMinimum(group);
  const auto &hi = workshopMaximum(group);
  value = std::clamp(value, lo[field], hi[field]);
  if ((group == 1 && (field == 0 || field == 1 || field == 7)) ||
      (group == 2 && field <= 1) || (group == 3 && field == 0) ||
      (group == 4 && field <= 1) || (group == 5 && field <= eDivision))
    value = std::round(value);
  if (group == 6) {
    if (!std::isfinite(r.drums[dModel]) || r.drums[dModel] < 0 ||
        r.drums[dModel] >= drumModelCount)
      return false;
    const auto range = drumAttackRange(int(r.drums[dModel]), field);
    value = std::clamp(value, range[0], range[1]);
  }
  if (group == 7 && field == cBits)
    value = std::round(value);
  workshopGroup(r, group)[field] = value;
  return true;
}

inline bool valid(const Recipe &r) {
  if (r.drumFlags > 1 || r.dustLocks >= (1u << dustFields))
    return false;
  for (int f = 0; f < dustFields; ++f) {
    Recipe copy = r;
    if (!setDust(copy, f, r.dustParams[f]) || copy.dustParams != r.dustParams)
      return false;
  }
  if (!std::isfinite(r.drums[dModel]) || r.drums[dModel] < 0 ||
      r.drums[dModel] >= drumModelCount ||
      std::round(r.drums[dModel]) != r.drums[dModel])
    return false;
  if (r.workshopLocks & ~(((1ULL << 45) - 1) | (4095ULL << 48)))
    return false;
  for (int group = 0; group < 8; ++group)
    for (int f = 0; f < 8; ++f) {
      Recipe c = r;
      const auto values = workshopGroup(c, group);
      if (!setWorkshop(c, group, f, values[f]) ||
          workshopGroup(c, group) != values)
        return false;
    }
  if (r.extendedLocks >= (1u << 24))
    return false;
  if (r.type < 0 || r.type >= generatorCount ||
      r.detailLocks >= (1ULL << detailLockCount))
    return false;
  if (r.seed > 65535 || r.locks >= (1u << fields) || !r.scale ||
      (r.scale & 0xf000u))
    return false;
  for (int f = 0; f < fields; ++f) {
    Recipe copy = r;
    if (!set(copy, f, r.v[f]) || copy.v[f] != r.v[f])
      return false;
  }
  for (int row = 0; row < 4; ++row) {
    const auto &c = r.curves[row];
    if ((c.enabled & 0x81) != 0x81 || c.enabled > 255)
      return false;
    for (int i = 0; i < 8; ++i) {
      const auto &p = c.p[i];
      if (!std::isfinite(p.time) || !std::isfinite(p.value) || p.time < 0 ||
          p.time > 1 || std::abs(p.value) > limits(r)[row] || p.shape < 0 ||
          p.shape > 2 || (i && p.time <= c.p[i - 1].time))
        return false;
    }
    if (c.p.front().time != 0 || c.p.back().time != 1)
      return false;
  }
  for (int f = 0; f < waveFields; ++f) {
    Recipe c = r;
    if (!setWave(c, f, r.osc[f]) || c.osc != r.osc)
      return false;
  }
  for (int f = 0; f < bloomFields; ++f) {
    Recipe c = r;
    if (!setBloom(c, f, r.packets[f]) || c.packets != r.packets)
      return false;
  }
  for (int f = 0; f < modFields; ++f) {
    Recipe c = r;
    if (!setMod(c, f, r.mod[f]) || c.mod != r.mod)
      return false;
  }
  for (int f = 0; f < resynthFields; ++f) {
    Recipe c = r;
    if (!setResynth(c, f, r.spectral[f]) || c.spectral != r.spectral)
      return false;
  }
  for (int f = 0; f < 8; ++f) {
    Recipe c = r;
    if (!setAdditive(c, f, r.additive[f]) || c.additive != r.additive ||
        !setNoise(c, f, r.noise[f]) || c.noise != r.noise ||
        !setFm(c, f, r.fmOsc[f]) || c.fmOsc != r.fmOsc)
      return false;
  }
  for (int f = 0; f < convolutionFields; ++f) {
    Recipe c = r;
    if (!setConvolution(c, f, r.conv[f]) || c.conv != r.conv)
      return false;
  }
  for (int f = 0; f < drumFields; ++f) {
    Recipe c = r;
    if (!setDrum(c, f, r.drums[f]) || c.drums != r.drums)
      return false;
  }
  for (int s = 0; s < 4; ++s)
    for (int i = 0; i < 8; ++i)
      if (!std::isfinite(r.ratios[s][i]) || r.ratios[s][i] < 1 ||
          r.ratios[s][i] > 64 || !std::isfinite(r.partials[s][i]) ||
          std::abs(r.partials[s][i]) > 1)
        return false;
  return true;
}
inline float curve(const Curve &c, float time) {
  int left = 0, right = 7;
  for (int i = 1; i < 8; ++i)
    if (c.enabled & (1u << i)) {
      right = i;
      if (time <= c.p[i].time)
        break;
      left = i;
    }
  if (left == right)
    return c.p[left].value;
  float t = std::clamp(
      (time - c.p[left].time) / (c.p[right].time - c.p[left].time), 0.f, 1.f);
  if (c.p[left].shape == 0)
    t = t >= 1 ? 1 : 0;
  else if (c.p[left].shape == 2)
    t = t * t * (3 - 2 * t);
  return c.p[left].value + (c.p[right].value - c.p[left].value) * t;
}
inline bool editPoint(Recipe &r, int row, int at, int field, float value) {
  if (row < 0 || row > 3 || at < 0 || at > 7 || field < 0 || field > 2 ||
      !std::isfinite(value))
    return false;
  auto &c = r.curves[row];
  auto &p = c.p[at];
  if (field == 0) {
    if (at == 0 || at == 7)
      return false;
    const float before = c.p[at - 1].time, after = c.p[at + 1].time;
    const float margin = std::min(.0001f, (after - before) * .25f);
    float low = std::max(std::nextafter(before, after), before + margin),
          high = std::min(std::nextafter(after, before), after - margin);
    if (low > high) {
      low = std::nextafter(before, after);
      high = std::nextafter(after, before);
    }
    if (low <= high)
      p.time = std::clamp(value, low, high);
  }
  if (field == 1)
    p.value = std::clamp(value, -limits(r)[row], limits(r)[row]);
  if (field == 2)
    p.shape = int(std::round(std::clamp(value, 0.f, 2.f)));
  c.enabled |= 1u << at;
  return true;
}
inline float scalePitch(float note, std::uint16_t scale) {
  int best = 0;
  float distance = 1e6f;
  for (int n = -24; n <= 24; ++n)
    if (scale & (1u << ((n + 120) % 12))) {
      const float d = std::abs(note - n);
      if (d < distance) {
        distance = d;
        best = n;
      }
    }
  return float(best);
}
inline void mutateExtra(Recipe &, float, std::uint32_t);
inline void mutate(Recipe &r, float amount) {
  if (!std::isfinite(amount) || amount <= 0)
    return;
  amount = std::clamp(amount, 0.f, 1.f);
  const auto seed = hash(r.seed + 0x53504c4eu);
  for (int f = 0; f < fields; ++f)
    if (!(r.locks & (1u << f)) &&
        (r.type == spline || f <= level || f == loop)) {
      const float delta = (random(seed, f + 1) * 2 - 1) * amount;
      const float value =
          (f == length || f == evolution || f == driftRate)
              ? r.v[f] * std::exp2(delta * 2)
              : r.v[f] + delta * (maximum[f] - minimum[f]) * .35f;
      set(r, f, value);
    }
  mutateExtra(r, amount, seed);
  r.seed = hash(seed) % 65536;
}
inline void randomCurve(Recipe &r, int row, float low, float high, float amount,
                        bool keepEnds) {
  if (row < 0 || row > 3 || !std::isfinite(low) || !std::isfinite(high) ||
      !std::isfinite(amount) || amount <= 0)
    return;
  amount = std::min(amount, 1.f);
  low = std::clamp(low, -limits(r)[row], limits(r)[row]);
  high = std::clamp(high, low, limits(r)[row]);
  std::array<float, 49> pitches{};
  int pitchCount = 0;
  if (row == 0 && r.type != dust) {
    for (int note = -24; note <= 24; ++note)
      if (note >= low && note <= high &&
          (r.scale & (1u << ((note + 120) % 12))))
        pitches[pitchCount++] = float(note);
    if (!pitchCount)
      return;
  }
  const auto seed = hash(r.seed + 0x43555256u);
  auto &c = r.curves[row];
  for (int i = 0; i < 8; ++i) {
    if (keepEnds && (i == 0 || i == 7))
      continue;
    float value =
        low + (high - low) * random(seed, std::uint32_t(row * 8 + i + 1));
    if (row == 0 && r.type != dust)
      value = pitches[std::min(
          pitchCount - 1,
          int(random(seed, std::uint32_t(row * 8 + i + 1)) * pitchCount))];
    c.p[i].value = std::clamp(c.p[i].value + (value - c.p[i].value) * amount,
                              -limits(r)[row], limits(r)[row]);
    c.enabled |= 1u << i;
  }
  r.seed = hash(seed) % 65536;
}

inline float lfo(const Recipe &r, float seconds) {
  if (!modActive(r))
    return 0;
  const float rate =
      r.mod[mFit] > .5f ? r.mod[mRate] / r.v[length] : r.mod[mRate];
  const double position = double(seconds) * rate + r.mod[mPhase],
               cycle = std::floor(position);
  const float phase = float(position - cycle);
  float value = 0;
  switch (int(r.mod[mShape])) {
  case 0:
    value = std::sin(6.28318530718f * phase);
    break;
  case 1:
    value = 1 - 4 * std::abs(phase - .5f);
    break;
  case 2:
    value = phase * 2 - 1;
    break;
  case 3:
    value = phase < .5f ? 1.f : -1.f;
    break;
  default: {
    const float t = phase * phase * (3 - 2 * phase);
    value = 2 * (random(r.seed ^ 0x4c464f30u, std::uint32_t(cycle)) * (1 - t) +
                 random(r.seed ^ 0x4c464f30u, std::uint32_t(cycle) + 1) * t) -
            1;
    break;
  }
  }
  const float fade =
      r.mod[mFade] > 0 ? std::clamp(seconds / (r.mod[mFade] * .001f), 0.f, 1.f)
                       : 1.f;
  return value * r.mod[mAmount] * fade;
}
inline float animatedCurve(const Recipe &r, int row, float time,
                           float seconds) {
  const float value = curve(r.curves[row], time);
  return value + (int(r.mod[mDestination]) == row + 1
                      ? lfo(r, seconds) * limits(r)[row]
                      : 0);
}
inline void clean(Recipe &r) {
  r.v[width] = r.type == resynth ? 1.f : 0.f;
  r.v[drift] = 0;
  r.v[dynamics] = 0;
  r.mod = modDefaults;
  r.curves = {};
  const float selected = r.osc[wShape];
  r.osc = waveDefaults;
  r.osc[wShape] = selected;
  r.spectral = resynthDefaults;
  r.additive[aDetune] = r.additive[aPhase] = r.additive[aDrift] =
      r.additive[aDecay] = 0;
  r.noise[nMotion] = r.noise[nLeak] = 0;
  r.fmOsc[fFeedback] = r.fmOsc[fDetune] = 0;
  r.conv[cTone] = 0;
  r.conv[cMix] = 1;
  r.drums[dCharacter] = 0;
  r.drumExtra[xBias] = r.drumExtra[xCompression] = r.drumExtra[xSnap] = 0;
  r.drumColor = colorDefaults;
  r.dustParams[uClump] = r.dustParams[uFloor] = r.dustParams[uFault] = 0;
  r.dustParams[uBits] = 24;
  r.dustParams[uColor] = 0;
  r.dustParams[uRepeat] = 1;
  r.dustParams[uVariation] = 0;
  r.plucked[pHardness] = r.plucked[pDetune] = 0;
  r.sub[sDetune] = r.sub[sDrift] = 0;
  r.timing[eSwing] = 0;
  r.hybridParams[hMutation] = r.hybridParams[hDrift] = 0;
  r.trace[tDrift] = r.trace[tJitter] = r.trace[tSpread] = 0;
  r.events[eJitter] = r.events[ePitch] = r.events[eVariation] = r.events[ePan] =
      0;
  r.packets[bMode] = 0;
  for (int f : {bScatter, bRiseVar, bSustainVar, bDecayVar, bPitchJitter,
                bAmpJitter, bBend, bOctave})
    r.packets[f] = 0;
}

inline void mutateExtra(Recipe &r, float amount, std::uint32_t seed) {
  const auto delta = [&](int f) {
    return (random(seed ^ 0x45585452u, f + 1) * 2 - 1) * amount;
  };
  if (r.type == wave)
    for (int f = 0; f < waveFields; ++f)
      if (!(r.detailLocks & (1u << f)) && waveAvailable(r, f)) {
        const float v =
            f == wShape
                ? std::round(random(seed, f + 71) * 3)
                : f == wCutoff
                      ? r.osc[f] * std::exp2(delta(f) * 2)
                      : r.osc[f] + delta(f) * (waveMax[f] - waveMin[f]) * .35f;
        setWave(r, f, v);
      }
  if (r.type == bloom)
    for (int f = 0; f < bloomFields; ++f)
      if (!(r.detailLocks & (1u << (waveFields + f))) && bloomAvailable(r, f)) {
        const float v =
            f == bMode || f == bSpectrum
                ? std::round(random(seed, f + 71) * bloomMax[f])
                : f == bGap || f == bRise || f == bSustain || f == bDecay
                      ? r.packets[f] * std::exp2(delta(f) * 2)
                      : r.packets[f] +
                            delta(f) * (bloomMax[f] - bloomMin[f]) * .35f;
        setBloom(r, f, v);
      }
  if (r.type == resynth)
    for (int f = 0; f < resynthFields; ++f)
      if (!(r.detailLocks & (1ULL << (30 + f))) && resynthAvailable(r, f)) {
        if (f == rsMode || f == rsStart || f == rsEnd)
          continue;
        setResynth(r, f,
                   r.spectral[f] +
                       delta(f + 51) * (resynthMax[f] - resynthMin[f]) * .25f);
      }
  for (int f = 0; f < 8; ++f) {
    if (r.type == partialsType && !(r.detailLocks & (1ULL << (38 + f))) &&
        additiveAvailable(r, f))
      setAdditive(r, f,
                  r.additive[f] +
                      delta(f + 81) * (additiveMax[f] - additiveMin[f]) * .2f);
    if (r.type == noiseBank && !(r.detailLocks & (1ULL << (46 + f))) &&
        noiseAvailable(r, f))
      setNoise(r, f,
               f == nColor
                   ? std::round(random(seed, f + 91) * 2)
                   : r.noise[f] +
                         delta(f + 91) * (noiseMax[f] - noiseMin[f]) * .2f);
    if (r.type == fm && !(r.detailLocks & (1ULL << (54 + f))) &&
        fmAvailable(r, f))
      setFm(r, f,
            f == fAlgorithm
                ? std::round(random(seed, f + 101) * 3)
                : r.fmOsc[f] + delta(f + 101) * (fmMax[f] - fmMin[f]) * .2f);
  }
  if (r.type == dust)
    for (int f = 0; f < dustFields; ++f)
      if (!(r.dustLocks & (1u << f)) && dustAvailable(r, f)) {
        const bool log = f == uRate || f == uSize || f == uTone;
        setDust(r, f,
                log ? r.dustParams[f] * std::exp2(delta(f + 241) * 2)
                    : r.dustParams[f] +
                          delta(f + 241) * (dustMax[f] - dustMin[f]) * .2f);
      }
  if (r.type == convolution)
    for (int f = 0; f < 8; ++f)
      if (!(r.extendedLocks & (1u << f)))
        setConvolution(r, f,
                       r.conv[f] + delta(f + 111) *
                                       (convolutionMax[f] - convolutionMin[f]) *
                                       .15f);
  if (r.type == drum)
    for (int f = 0; f < drumFields; ++f)
      if (!(r.extendedLocks & (1u << (8 + f))) && drumAvailable(r, f))
        setDrum(r, f,
                f == dModel
                    ? std::round(random(seed, 121) * float(drumModelCount - 1))
                    : (f == dTune || f == dDecay || f == dSweep)
                          ? r.drums[f] * std::exp2(delta(f + 121) * 2)
                          : r.drums[f] +
                                delta(f + 121) *
                                    ((f == dRing && r.drums[dModel] != 10)
                                         ? 1.f
                                         : drumMax[f] - drumMin[f]) *
                                    .15f);
  for (int group = 0; group < 8; ++group) {
    const bool relevant =
        (group == 0 && r.type == pluck) ||
        (group == 1 && r.type == subharmonic) ||
        (group == 2 && r.type == hybrid) || (group == 3 && r.type == resynth) ||
        ((group == 4 || group == 5) && supportsEvents(r.type)) ||
        ((group == 6 || group == 7) && r.type == drum);
    if (relevant)
      for (int f = 0; f < 8; ++f)
        if (!(r.workshopLocks & (1ULL << (group * 8 + f))) &&
            workshopAvailable(r, group, f)) {
          auto &values = workshopGroup(r, group);
          const auto &lo = workshopMinimum(group);
          const auto &hi = workshopMaximum(group);
          const bool exponential =
              (group == 0 && f < 2) || (group == 2 && f == hLife) ||
              (group == 3 && (f == tLife || f == tTolerance)) ||
              (group == 4 && f == eLife) ||
              (group == 6 &&
               (f == xHitDecay || f == xNoiseDecay || f == xEdge));
          setWorkshop(r, group, f,
                      exponential
                          ? values[f] *
                                std::exp2(delta(151 + group * 8 + f) * 2)
                          : values[f] + delta(151 + group * 8 + f) *
                                            (hi[f] - lo[f]) * .2f);
        }
  }
  for (int f = 0; f < modFields; ++f)
    if (!(r.detailLocks & (1u << (waveFields + bloomFields + f))) &&
        modAvailable(r, f)) {
      const float v =
          f == mRate
              ? r.mod[f] * std::exp2(delta(f + 31) * 2)
              : r.mod[f] + delta(f + 31) * (modMax[f] - modMin[f]) * .35f;
      setMod(r, f, v);
    }
}
inline std::uint32_t checksum(const std::vector<std::uint8_t> &v) {
  std::uint32_t h = 2166136261u;
  for (auto b : v) {
    h ^= b;
    h *= 16777619u;
  }
  return h;
}
inline void put(std::vector<std::uint8_t> &v, std::uint32_t n) {
  for (int i = 0; i < 4; ++i)
    v.push_back(std::uint8_t(n >> (i * 8)));
}
inline void putFloat(std::vector<std::uint8_t> &v, float f) {
  std::uint32_t n;
  std::memcpy(&n, &f, 4);
  put(v, n);
}
template <class Emit> inline void serializeRecipe(const Recipe &r, Emit emit) {
  std::uint32_t crc = 2166136261u;
  const auto byte = [&](std::uint8_t b) {
    emit(b);
    crc ^= b;
    crc *= 16777619u;
  };
  const auto integer = [&](std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
      byte(std::uint8_t(v >> (i * 8)));
  };
  const auto scalar = [&](float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, 4);
    integer(bits);
  };
  integer(10);
  integer(std::uint32_t(r.type));
  integer(r.seed);
  integer(r.locks);
  integer(r.scale);
  for (float v : r.v)
    scalar(v);
  for (const auto &c : r.curves) {
    integer(c.enabled);
    for (auto point : c.p) {
      scalar(point.time);
      scalar(point.value);
      integer(std::uint32_t(point.shape));
    }
  }
  integer(std::uint32_t(r.detailLocks));
  integer(std::uint32_t(r.detailLocks >> 32));
  for (float v : r.osc)
    scalar(v);
  for (float v : r.packets)
    scalar(v);
  for (float v : r.mod)
    scalar(v);
  for (float v : r.spectral)
    scalar(v);
  for (const auto &a : r.ratios)
    for (float v : a)
      scalar(v);
  for (const auto &a : r.partials)
    for (float v : a)
      scalar(v);
  for (const auto *a : {&r.additive, &r.noise, &r.fmOsc})
    for (float v : *a)
      scalar(v);
  for (float v : r.conv)
    scalar(v);
  for (float v : r.drums)
    scalar(v);
  integer(r.extendedLocks);
  for (const auto *a :
       {&r.plucked, &r.sub, &r.hybridParams, &r.trace, &r.events})
    for (float v : *a)
      scalar(v);
  integer(std::uint32_t(r.workshopLocks));
  integer(std::uint32_t(r.workshopLocks >> 32));
  for (float v : r.timing)
    scalar(v);
  for (float v : r.drumExtra)
    scalar(v);
  integer(r.drumFlags);
  for (float v : r.drumColor)
    scalar(v);
  for (float v : r.dustParams)
    scalar(v);
  integer(r.dustLocks);
  for (int i = 0; i < 4; ++i)
    emit(std::uint8_t(crc >> (i * 8)));
}
inline std::vector<std::uint8_t> encode(const Recipe &r) {
  std::vector<std::uint8_t> out;
  out.reserve(1408);
  serializeRecipe(r, [&](std::uint8_t b) { out.push_back(b); });
  return out;
}
inline constexpr std::size_t legacyRecipeBytes = 476, previousRecipeBytes = 892,
                             thirdRecipeBytes = 988, fourthRecipeBytes = 1072,
                             fifthRecipeBytes = 1088, sixthRecipeBytes = 1256,
                             seventhRecipeBytes = 1288,
                             eighthRecipeBytes = 1324, ninthRecipeBytes = 1356,
                             recipeBytes = 1408;
inline bool decode(const std::uint8_t *data, std::size_t count, Recipe &out) {
  if (!data || (count != recipeBytes && count != previousRecipeBytes &&
                count != thirdRecipeBytes && count != fourthRecipeBytes &&
                count != fifthRecipeBytes && count != sixthRecipeBytes &&
                count != seventhRecipeBytes && count != eighthRecipeBytes &&
                count != ninthRecipeBytes && count != legacyRecipeBytes))
    return false;
  std::size_t at = 0;
  const auto get = [&]() {
    std::uint32_t n = 0;
    for (int i = 0; i < 4; ++i)
      n |= std::uint32_t(data[at++]) << (i * 8);
    return n;
  };
  const auto version = get(), kind = get();
  if (!((version == 1 && kind == 1 && count == legacyRecipeBytes) ||
        (version == 2 && kind < 4 && count == previousRecipeBytes) ||
        (version == 3 && kind < 7 && count == thirdRecipeBytes) ||
        (version == 4 && kind < 9 && count == fourthRecipeBytes) ||
        (version == 5 && kind < 9 && count == fifthRecipeBytes) ||
        (version == 6 && kind < 12 && count == sixthRecipeBytes) ||
        (version == 7 && kind < 12 && count == seventhRecipeBytes) ||
        (version == 8 && kind < 12 && count == eighthRecipeBytes) ||
        (version == 9 && kind < 12 && count == ninthRecipeBytes) ||
        (version == 10 && kind < generatorCount && count == recipeBytes)))
    return false;
  Recipe next;
  next.type = version == 1 ? spline : int(kind);
  next.seed = get();
  next.locks = get();
  const auto scale = get();
  if (scale > 0xfff)
    return false;
  next.scale = std::uint16_t(scale);
  const auto getFloat = [&]() {
    const auto n = get();
    float f;
    std::memcpy(&f, &n, 4);
    return f;
  };
  for (auto &v : next.v)
    v = getFloat();
  for (auto &c : next.curves) {
    c.enabled = get();
    for (auto &p : c.p) {
      p.time = getFloat();
      p.value = getFloat();
      p.shape = int(get());
    }
  }
  if (version >= 2) {
    next.detailLocks = get();
    next.detailLocks |= std::uint64_t(get()) << 32;
    for (auto &v : next.osc)
      v = getFloat();
    for (auto &v : next.packets)
      v = getFloat();
    for (auto &v : next.mod)
      v = getFloat();
    for (auto &v : next.spectral)
      v = getFloat();
    for (auto &a : next.ratios)
      for (auto &v : a)
        v = getFloat();
    for (auto &a : next.partials)
      for (auto &v : a)
        v = getFloat();
  }
  if (version >= 3)
    for (auto *a : {&next.additive, &next.noise, &next.fmOsc})
      for (float &v : *a)
        v = getFloat();
  else if (version == 2) {
    if (next.detailLocks >= (1ULL << 38))
      return false;
    next.detailLocks |= (1ULL << 38) | (1ULL << 46) | (1ULL << 54);
  }
  if (version >= 4) {
    for (float &v : next.conv)
      v = getFloat();
    for (int i = 0; i < (version == 4 ? 12 : drumFields); ++i)
      next.drums[std::size_t(i)] = getFloat();
    next.extendedLocks = get();
    if (version == 4 &&
        (next.extendedLocks >= (1u << 20) || next.drums[dModel] > 5))
      return false;
  }
  if (version >= 6) {
    for (auto *a : {&next.plucked, &next.sub, &next.hybridParams, &next.trace,
                    &next.events})
      for (float &v : *a)
        v = getFloat();
    next.workshopLocks = get();
    next.workshopLocks |= std::uint64_t(get()) << 32;
  }
  if (version >= 7)
    for (auto &v : next.timing)
      v = getFloat();
  if (version >= 8)
    for (auto &v : next.drumExtra)
      v = getFloat();
  if (version >= 8)
    next.drumFlags = get();
  if (version >= 9)
    for (auto &v : next.drumColor)
      v = getFloat();
  if (version >= 10) {
    for (auto &v : next.dustParams)
      v = getFloat();
    next.dustLocks = get();
  }
  const std::vector<std::uint8_t> checked(data, data + count - 4);
  if (get() != checksum(checked))
    return false;
  if (version < 7) {
    // The old SUB recipe described a nonlinear divider cloud. Translate its
    // count/register into a clean bank, keeping other generator recipes intact.
    if (version == 6) {
      if (next.workshopLocks >= (1ULL << 40))
        return false;
      constexpr std::array<float, 8> lo{0, 1, 0, 0, 0, .01f, .05f, 0},
          hi{3, 32, 1, 1, 1, 8, 1, 3};
      for (int i = 0; i < 8; ++i)
        if (!std::isfinite(next.sub[i]) || next.sub[i] < lo[i] ||
            next.sub[i] > hi[i] ||
            ((i == 0 || i == 1 || i == 7) &&
             std::round(next.sub[i]) != next.sub[i]))
          return false;
      const auto old = next.sub;
      next.sub = subDefaults;
      if (next.type == subharmonic) {
        next.sub[sHarmonics] = std::clamp(std::round(old[1] * .5f), 1.f, 16.f);
        next.sub[sSubs] = std::clamp(std::round(old[1] * .25f), 0.f, 16.f);
        initializeSubBank(next);
      }
      if (next.events[eMode] > 0) {
        const double interval =
            double(next.v[length]) / std::max(1.f, next.events[eCount]);
        next.timing[eBpm] =
            std::clamp(float(std::round(30.0 / interval)), 1.f, 999.f);
      }
    }
    next.workshopLocks |= (31ULL << 40);
  }
  if (version < 8) {
    if (!std::isfinite(next.drums[dModel]) || next.drums[dModel] < 0 ||
        next.drums[dModel] > 8 ||
        std::round(next.drums[dModel]) != next.drums[dModel] ||
        (version >= 6 && next.workshopLocks >= (1ULL << 45)))
      return false;
    next.drumFlags = 1;
    next.drumExtra = legacyDrumAttack(next);
    next.workshopLocks |= (3ULL << 54);
  }
  if (version < 9) {
    if (version == 8 &&
        (next.drums[dModel] > 9 ||
         (next.workshopLocks & ~(((1ULL << 45) - 1) | (255ULL << 48)))))
      return false;
    next.workshopLocks |= 15ULL << 56;
  }
  if (!valid(next))
    return false;
  out = next;
  return true;
}

} // namespace s3g_thru_synth
