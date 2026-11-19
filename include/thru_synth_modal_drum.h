#pragma once
#include "s3g_drum/s3g_accelerometer_field_encoder.h"
#include "s3g_drum/s3g_ambi_membrane_kick.h"
#include "s3g_drum/s3g_fractional_waveguide_network.h"
#include <memory>

namespace s3g_thru_synth {
// These cores exist only in the offline worker. Their outputs are ordinary
// stereo, with one linked file normalization after the complete render.
struct ModalDrumOutput {
  s3g::DrumCharacter character;
  float stereoWidth;
  ModalDrumOutput(const Recipe &r, float rate, bool localDrive)
      : stereoWidth(r.v[width]) {
    s3g::DrumCharacterParams p;
    p.drive = localDrive ? 0.f : r.drums[dCharacter];
    p.compression = r.drumExtra[xCompression];
    p.bias = r.drumExtra[xBias];
    p.tone = (r.drumFlags || r.drums[dModel] != 8)
                 ? (r.drums[dTone] - .5f) * 1.5f
                 : 0.f;
    character.prepare(rate);
    character.setParams(p);
  }
  void finish(float &left, float &right) {
    const float mid = .5f * (left + right);
    const float side = .5f * (left - right) * stereoWidth;
    left = mid + side;
    right = mid - side;
    character.processFrame(left, right);
  }
};

struct MembraneDrumSample {
  s3g::AmbiMembraneKick core;
  ModalDrumOutput output;
  MembraneDrumSample(const Recipe &r, float rate) : output(r, rate, true) {
    const auto &v = r.drums;
    s3g::AmbiMembraneKickParams p;
    p.order = s3g::kAmbiMembraneKickStereoDownmix;
    p.tuneHz = v[dTune];
    p.decaySeconds = v[dDecay] * .001f;
    p.punch = v[dBody];
    p.click = v[dHit];
    p.drive = v[dCharacter];
    p.pitchSweepSemitones = v[dBend];
    p.pitchSweepMs = v[dSweep];
    p.damping = v[dDamping];
    p.shape = static_cast<s3g::AmbiMembraneShape>(int(v[dForm]));
    p.shapeAmount = v[dExtent];
    p.strikeX = (v[dContactX] * 2 - 1) * .98f;
    p.strikeY = (v[dContactY] * 2 - 1) * .98f;
    p.velocitySensitivity = 0;
    p.noteTracking = 0;
    p.outputGainDb = -6;
    core.prepare(rate);
    core.setParams(p);
    core.reset();
    core.trigger();
  }
  void processFrame(float &left, float &right) {
    std::array<float, 2> frame{};
    core.processFrame(frame.data(), 2);
    left = frame[0];
    right = frame[1];
    output.finish(left, right);
  }
};

struct ModalBodyDrumSample {
  static constexpr std::size_t block = 64;
  s3g::AccelerometerFieldEncoder core;
  ModalDrumOutput output;
  std::array<std::array<float, block>, 8> samples{};
  std::array<float *, 8> pointers{};
  std::array<std::array<float, 2>, 8> pans{};
  std::size_t cursor = block;
  int bodies;
  ModalBodyDrumSample(const Recipe &r, float rate)
      : output(r, rate, false),
        bodies(4 + int(std::lround(r.drums[dExtent] * 4))) {
    using S = s3g::AccelerometerSubstrate;
    static constexpr S materials[] = {
        S::DeepBronze,     S::TieredBronze,      S::BroadBronze,
        S::BrightBronze,   S::CarbonLaminate,    S::GlassPlate,
        S::AluminumPlate,  S::SprucePlate,       S::SteelShell,
        S::PorcelainShell, S::PorousEarthenware, S::TensionedSkin,
        S::LoadedMembrane, S::CoupledMembrane,   S::CavityMembrane,
        S::LooseMembrane};
    const auto &v = r.drums;
    s3g::AccelerometerFieldParams p;
    p.substrate = materials[int(v[dForm])];
    p.outputMode = s3g::AccelerometerFieldOutputMode::BodyStems;
    p.ambisonicOrder = 1;
    p.bodyCount = std::uint32_t(bodies);
    p.size = .5f;
    p.sensorMass = v[dBody];
    p.damping = v[dDamping];
    p.irregularity = v[dNoise];
    p.coupling = v[dRing];
    p.energy = v[dCharacter] * .5f;
    p.bodySkinX.fill(v[dContactX]);
    p.bodySkinY.fill(v[dContactY]);
    p.activity = 0;
    p.ambientDrive = 0;
    p.externalDrive = 0;
    p.sensorNoise = 0;
    p.modalLift = 0;
    p.fieldListenMode = s3g::AmbiFieldListenMode::Off;
    p.outputGainDb = -6;
    p.seed = hash(r.seed ^ 0x4d4f444cu);
    core.prepare(rate);
    core.setParams(p);
    core.reset();
    core.tuneOfflineSample(v[dTune], v[dDecay] * .001f);
    for (int i = 0; i < 8; ++i)
      pointers[i] = samples[i].data();
    for (int i = 0; i < bodies; ++i) {
      const float angle = float(i) / float(bodies - 1) * 1.57079632679f;
      pans[i] = {std::cos(angle) / std::sqrt(float(bodies)),
                 std::sin(angle) / std::sqrt(float(bodies))};
      core.strikeMidi(60, .15f + .85f * v[dHit]);
    }
  }
  void processFrame(float &left, float &right) {
    if (cursor == block) {
      core.process(nullptr, pointers.data(), 8, std::uint32_t(block));
      cursor = 0;
    }
    left = right = 0;
    for (int i = 0; i < bodies; ++i) {
      left += samples[i][cursor] * pans[i][0];
      right += samples[i][cursor] * pans[i][1];
    }
    ++cursor;
    output.finish(left, right);
  }
};

struct MediumDrumSample {
  static constexpr std::size_t block = 64;
  s3g::FractionalWaveguideNetwork core;
  ModalDrumOutput output;
  std::array<std::array<float, block>, 4> samples{};
  std::array<float *, 4> pointers{};
  std::array<std::array<float, block>, 8> actuation{};
  std::array<const float *, 8> actuators{};
  std::array<float, 8> weights{};
  std::size_t cursor = block, frame = 0;
  float sampleRate, noise;
  std::uint32_t seed;
  MediumDrumSample(const Recipe &r, float rate)
      : output(r, rate, true), sampleRate(rate), noise(r.drums[dNoise]),
        seed(r.seed) {
    const auto &v = r.drums;
    s3g::FractionalWaveguideParams p;
    p.order = 1;
    p.propagationSpeed = v[dTune];
    p.decaySeconds = v[dDecay] * .001f;
    p.absorption = v[dDamping];
    p.junctionNonlinearity = v[dCharacter];
    p.dispersion = v[dRing];
    p.radiation = v[dTone];
    p.exciter = static_cast<s3g::WaveguideExciter>(int(v[dForm]));
    p.sustainedExcitation = 0;
    p.exciterCharacter = v[dTone];
    p.outputGainDb = -6;
    core.prepare(rate, .025f);
    core.setParams(p);
    core.configureCube(.02f + 3.98f * v[dBody]);
    for (int i = 0; i < 4; ++i)
      pointers[i] = samples[i].data();
    for (int i = 0; i < 8; ++i) {
      weights[i] = (i & 1 ? v[dContactX] : 1 - v[dContactX]) *
                   (i & 2 ? v[dContactY] : 1 - v[dContactY]) *
                   (i & 4 ? v[dExtent] : 1 - v[dExtent]);
      actuators[i] = actuation[i].data();
      core.strike(i, weights[i] * (.15f + .85f * v[dHit]));
      core.triggerExciter(i, weights[i] * (.15f + .85f * v[dHit]));
    }
  }
  void processFrame(float &left, float &right) {
    if (cursor == block) {
      for (std::size_t i = 0; i < block; ++i, ++frame) {
        const float phase = std::min(1.f, float(frame) / (sampleRate * .012f));
        const float window = std::sin(phase * 3.14159265359f);
        const float force =
            phase < 1
                ? (random(seed ^ 0x4d454449u, std::uint32_t(frame)) * 2 - 1) *
                      noise * .12f * window * window
                : 0;
        for (int j = 0; j < 8; ++j)
          actuation[j][i] = force * weights[j];
      }
      core.process(nullptr, pointers.data(), 4, std::uint32_t(block), 0,
                   nullptr, actuators.data());
      cursor = 0;
    }
    // A fixed first-order stereo virtual pair: W is shared, X faces forward,
    // and opposite Y components distinguish left/right. Never treat raw ACN
    // channels 0 and 1 as a stereo pair.
    const float mid =
        .70710678f * samples[0][cursor] + .5f * samples[3][cursor];
    const float side = .5f * samples[1][cursor];
    left = mid + side;
    right = mid - side;
    ++cursor;
    output.finish(left, right);
  }
};
} // namespace s3g_thru_synth
