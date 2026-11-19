#pragma once
#include "s3g_drum/s3g_drum_concert_bass.h"
#include "s3g_drum/s3g_drum_cowbell.h"
#include "s3g_drum/s3g_drum_crash.h"
#include "s3g_drum/s3g_drum_floor_tom.h"
#include "s3g_drum/s3g_drum_hi_hat.h"
#include "s3g_drum/s3g_drum_kick.h"
#include "s3g_drum/s3g_drum_snare.h"
#include "thru_synth_drum_color.h"
#include "thru_synth_fm_drum.h"
#include "thru_synth_modal_drum.h"
namespace s3g_thru_synth {
template <class Core, class Cancel, class Progress>
bool renderDrumHit(Core &core, const Recipe &r, float rate, Audio &out,
                   Cancel cancel, Progress progress) {
  BankRender span(r, rate, out);
  float levelGain = 1;
  const float slew = 1 - std::exp(-1 / (rate * .003f));
  for (std::size_t i = 0; i < span.total; ++i) {
    if ((i & 1023u) == 0) {
      if (cancel())
        return false;
      progress(unsigned(i * 88 / span.total));
    }
    float l = 0, rr = 0;
    core.processFrame(l, rr);
    const float t = float(i % span.frames) /
                    std::max(1.f, float(span.frames - 1)),
                target = std::pow(10.f,
                                  animatedCurve(r, 3, t, float(i) / rate) / 20);
    if (i == 0)
      levelGain = target;
    levelGain += (target - levelGain) * slew;
    float g = levelGain * fileEnvelope(r, i, span.frames, span.splice, rate);
    if (r.drumExtra[xSnap] > 0)
      g *= 1 + 2 * r.drumExtra[xSnap] * std::exp(-float(i) / (rate * .025f));
    out.left[i] = l * g;
    out.right[i] = rr * g;
  }
  if (!colorDrum(r, rate, out, cancel))
    return false;
  finishSound(r, rate, out, span.frames, span.splice, r.drumExtra[xEdge]);
  progress(100);
  return !cancel();
}
template <class Cancel, class Progress>
bool renderDrum(const Recipe &r, float rate, Audio &out, Cancel cancel,
                Progress progress) {
  const auto &v = r.drums;
  const float decay = v[dDecay] * .001f;
  s3g::DrumCharacterParams character;
  character.drive = v[dCharacter];
  character.tone = r.drumFlags ? (v[dTone] - .5f) * 1.5f : 0.f;
  character.compression = r.drumExtra[xCompression];
  character.bias = r.drumExtra[xBias];
  const auto common = [&](auto &p) {
    p.noteTracking = 0;
    p.stereoWidth = r.v[width];
    p.velocitySensitivity = 0;
    p.outputGainDb = -6;
    p.character = character;
  };
  const auto run = [&](auto &core, auto &p, auto trigger) {
    common(p);
    core.prepare(rate);
    core.setParams(p);
    trigger();
    return renderDrumHit(core, r, rate, out, cancel, progress);
  };
  switch (int(v[dModel])) {
  case 0: {
    s3g::DrumKick core;
    s3g::DrumKickParams p;
    p.tuneHz = v[dTune];
    p.decaySeconds = decay;
    p.body = v[dBody];
    p.texture = v[dNoise];
    p.click = v[dHit];
    p.clickTone = p.textureTone = v[dTone];
    p.pitchDropSemitones = v[dBend];
    p.pitchSweepMs = v[dSweep];
    p.harmonics = v[dRing];
    p.tail = 1 - v[dDamping];
    p.punch = r.drumExtra[xPunch];
    p.clickDecayMs = r.drumExtra[xHitDecay];
    p.textureDecaySeconds = r.drumExtra[xNoiseDecay] * .001f;
    p.pitchSettle = r.drumExtra[xExciter];
    return run(core, p, [&] { core.trigger(); });
  }
  case 1: {
    s3g::DrumSnare core;
    s3g::DrumSnareParams p;
    p.tuneHz = v[dTune];
    p.bodyDecaySeconds = decay;
    p.wireDecaySeconds = r.drumExtra[xNoiseDecay] * .001f;
    p.punch = r.drumExtra[xPunch];
    p.rattle = r.drumExtra[xExciter];
    p.body = v[dBody];
    p.wires = v[dNoise];
    p.click = v[dHit];
    p.clickTone = p.wireTone = v[dTone];
    p.pitchDropSemitones = v[dBend];
    p.pitchSweepMs = v[dSweep];
    p.ring = v[dRing];
    p.wireTension = v[dDamping];
    return run(core, p, [&] { core.trigger(); });
  }
  case 2: {
    s3g::DrumFloorTom core;
    s3g::DrumFloorTomParams p;
    p.tuneHz = v[dTune];
    p.bodyDecaySeconds = decay;
    p.body = v[dBody];
    p.stickLevel = v[dHit];
    p.punch = r.drumExtra[xPunch];
    p.stickDecayMs = r.drumExtra[xHitDecay];
    p.stickTone = v[dTone];
    p.pitchDropSemitones = v[dBend];
    p.pitchSweepMs = v[dSweep];
    p.ring = v[dRing];
    p.damping = v[dDamping];
    return run(core, p,
               [&] { core.trigger(s3g::DrumTomArticulation::Head, 1); });
  }
  case 3: {
    s3g::DrumCowbell core;
    s3g::DrumCowbellParams p;
    p.tuneHz = v[dTune];
    p.decaySeconds = decay;
    p.bodyDecaySeconds = r.drumExtra[xHitDecay] * .001f;
    p.shape = r.drumExtra[xPunch];
    p.noiseDecaySeconds = r.drumExtra[xNoiseDecay] * .001f;
    p.detune = r.drumExtra[xExciter];
    p.body = v[dBody];
    p.noise = v[dNoise];
    p.attack = v[dHit];
    p.brightness = p.strikeTone = v[dTone];
    p.bendSemitones = v[dBend];
    p.bendDecaySeconds = v[dSweep] * .001f;
    p.intervalRatio = 1.2f + v[dRing];
    p.damping = v[dDamping];
    return run(core, p,
               [&] { core.trigger(s3g::DrumCowbellArticulation::Cowbell, 1); });
  }
  case 4: {
    s3g::DrumHiHat core;
    s3g::DrumHiHatParams p;
    p.tuneHz = v[dTune];
    p.closedDecaySeconds = decay;
    p.attack = r.drumExtra[xPunch];
    p.alloy = r.drumExtra[xExciter];
    p.openDecaySeconds = decay;
    p.density = v[dBody];
    p.wash = v[dNoise];
    p.chick = v[dHit];
    p.tone = p.chickTone = v[dTone];
    p.sizzle = v[dRing];
    p.air = 1 - v[dDamping];
    return run(core, p,
               [&] { core.trigger(s3g::DrumHiHatArticulation::Closed, 1); });
  }
  case 5: {
    s3g::DrumCrash core;
    s3g::DrumCrashParams p;
    p.tuneHz = v[dTune];
    p.decaySeconds = decay;
    p.washDecaySeconds = r.drumExtra[xNoiseDecay] * .001f;
    p.texture = r.drumExtra[xPunch];
    p.alloy = r.drumExtra[xExciter];
    p.density = 4 + 20 * v[dBody];
    p.wash = v[dNoise];
    p.attack = v[dHit];
    p.brightness = v[dTone];
    p.bell = v[dRing];
    p.damping = v[dDamping];
    return run(core, p,
               [&] { core.trigger(s3g::DrumCrashArticulation::Crash, 1); });
  }
  case 6: {
    if (cancel())
      return false;
    auto core = std::make_unique<MembraneDrumSample>(r, rate);
    return renderDrumHit(*core, r, rate, out, cancel, progress);
  }
  case 7: {
    if (cancel())
      return false;
    auto core = std::make_unique<ModalBodyDrumSample>(r, rate);
    return renderDrumHit(*core, r, rate, out, cancel, progress);
  }
  case 8: {
    if (cancel())
      return false;
    auto core = std::make_unique<MediumDrumSample>(r, rate);
    return renderDrumHit(*core, r, rate, out, cancel, progress);
  }
  case 9: {
    s3g::DrumConcertBass core;
    s3g::DrumConcertBassParams p;
    p.tuneHz = v[dTune];
    p.bodyDecaySeconds = decay;
    p.body = v[dBody];
    p.air = v[dNoise];
    p.impact = v[dHit];
    p.shellTone = v[dTone];
    p.size = v[dBend];
    p.headTension = v[dSweep];
    p.bloom = v[dRing];
    p.damping = v[dDamping];
    p.strikePosition = v[dContactX];
    p.shell = v[dContactY];
    p.rimLevel = v[dExtent];
    p.beaterHardness = r.drumExtra[xPunch];
    p.mutedDecaySeconds = r.drumExtra[xHitDecay] * .001f;
    p.rimDecaySeconds = r.drumExtra[xNoiseDecay] * .001f;
    p.rimTone = r.drumExtra[xExciter];
    return run(core, p, [&] {
      core.trigger(static_cast<s3g::DrumConcertBassArticulation>(int(v[dForm])),
                   1);
    });
  }
  case 10: {
    if (cancel())
      return false;
    FmDrumSample core(r, rate);
    return renderDrumHit(core, r, rate, out, cancel, progress);
  }
  default:
    return false;
  }
}
} // namespace s3g_thru_synth
