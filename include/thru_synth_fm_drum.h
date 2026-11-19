#pragma once
// Original FM percussion model. Generic FM, pitch/transient envelopes and
// waveshaping; no code, lookup tables or presets imported from LXR firmware.
namespace s3g_thru_synth {
struct FmDrumSample {
  static constexpr int oversample = 4, taps = 63;
  const Recipe &recipe;
  float rate;
  std::uint64_t clock = 0;
  double carrierPhase = 0, modPhase = 0;
  float feedback = 0;
  std::array<float, 2> noiseLow{}, toneLow{};
  std::array<std::array<float, taps>, 2> history{};
  std::array<float, taps> kernel{};
  int write = 0;
  s3g::DrumCharacter character;
  FmDrumSample(const Recipe &r, float sr) : recipe(r), rate(sr) {
    double sum = 0;
    for (int i = 0; i < taps; ++i) {
      const double n = i - (taps - 1) * .5, x = n / (taps - 1);
      const double window = .42 + .5 * std::cos(Plan::tau * x) +
                            .08 * std::cos(2 * Plan::tau * x);
      kernel[i] = float(
          (n == 0 ? .21
                  : std::sin(Plan::tau * .105 * n) / (3.141592653589793 * n)) *
          window);
      sum += kernel[i];
    }
    for (auto &k : kernel)
      k /= float(sum);
    s3g::DrumCharacterParams p;
    p.drive = r.drums[dCharacter];
    p.bias = r.drumExtra[xBias];
    p.compression = r.drumExtra[xCompression];
    character.setParams(p);
    character.prepare(sr);
  }
  void processFrame(float &left, float &right) {
    const auto &v = recipe.drums;
    const double sr = rate * oversample;
    const float fmDecay = .002f * std::pow(2000.f, v[dContactY]);
    const float cutoff =
        std::min(rate * .42f, 300.f * std::pow(60.f, v[dTone]));
    const float tone = float(1 - std::exp(-Plan::tau * cutoff / sr));
    const float noiseTone = float(1 - std::exp(-Plan::tau * 2200 / sr));
    for (int sub = 0; sub < oversample; ++sub, ++clock) {
      const double seconds = double(clock) / sr;
      const double pitch =
          v[dBend] * std::exp(-6.90775527898 * seconds / (v[dSweep] * .001));
      const double hz = std::min(sr * .35, v[dTune] * std::exp2(pitch / 12));
      const double modHz = std::min(sr * .35, hz * v[dRing]);
      const float depth =
          std::min(24 * v[dContactX] *
                       float(std::exp(-6.90775527898 * seconds / fmDecay)),
                   float(std::max(0.0, sr * .35 - hz) / std::max(1.0, modHz)));
      const float modulator =
          std::sin(float(Plan::tau * modPhase) + 3 * v[dDamping] * feedback);
      feedback += (modulator - feedback) * .4f;
      const float angle = float(Plan::tau * carrierPhase) + depth * modulator;
      float carrier = 0;
      const int shape = int(v[dForm]);
      for (int h = 1; h <= (shape == 0 ? 1 : 8); ++h) {
        if ((shape == 1 || shape == 3) && !(h & 1))
          continue;
        const float edge = float(sr * .42 / h);
        const float guard = std::clamp(
            (edge - float(hz + depth * modHz)) / (edge * .12f), 0.f, 1.f);
        const float weight =
            shape == 0
                ? 1.f
                : shape == 1 ? ((h & 2) ? -1.f : 1.f) / float(h * h) : 1.f / h;
        carrier += std::sin(angle * h) * weight * guard;
      }
      const float body =
          float(std::exp(-6.90775527898 * seconds / (v[dDecay] * .001)));
      const float hit = float(std::exp(-6.90775527898 * seconds /
                                       (recipe.drumExtra[xHitDecay] * .001)));
      const float air = float(std::exp(-6.90775527898 * seconds /
                                       (recipe.drumExtra[xNoiseDecay] * .001)));
      const float punch =
          1 + recipe.drumExtra[xPunch] * float(std::exp(-seconds / .012));
      float frame[2]{};
      for (int ch = 0; ch < 2; ++ch) {
        const float noise =
            random(recipe.seed ^ (ch ? 0x52494748u : 0x4c454654u),
                   std::uint32_t(clock)) *
                2 -
            1;
        noiseLow[ch] += (noise - noiseLow[ch]) * noiseTone;
        const float raw = v[dBody] * carrier * body * punch +
                          v[dExtent] * modulator * body +
                          v[dNoise] * noiseLow[ch] * air +
                          v[dHit] * (noise - noiseLow[ch]) * hit;
        toneLow[ch] += (raw - toneLow[ch]) * tone;
        frame[ch] = toneLow[ch];
      }
      const float mid = .5f * (frame[0] + frame[1]);
      for (int ch = 0; ch < 2; ++ch)
        history[ch][write] = mid + (frame[ch] - mid) * recipe.v[width];
      write = (write + 1) % taps;
      carrierPhase += hz / sr;
      carrierPhase -= std::floor(carrierPhase);
      modPhase += modHz / sr;
      modPhase -= std::floor(modPhase);
    }
    float frame[2]{};
    for (int ch = 0; ch < 2; ++ch)
      for (int j = 0; j < taps; ++j)
        frame[ch] += history[ch][(write + taps - 1 - j) % taps] * kernel[j];
    left = frame[0] * .5f;
    right = frame[1] * .5f;
    character.processFrame(left, right);
  }
};
} // namespace s3g_thru_synth
