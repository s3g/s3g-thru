#pragma once
#include "thru_fm_voice.h"
#include "thru_partials_parameters.h"
namespace s3g_thru_partials {
constexpr float tau = 6.2831853071795864769f;
inline float randomUnit(std::uint32_t x) {
  return float(s3g_thru_table::Library::hash(x) >> 8) * (1.f / 16777215.f);
}
inline float smoothRandom(std::uint32_t seed, float at) {
  const auto i = std::uint32_t(std::max(0.f, std::floor(at)));
  float x = at - float(i);
  x = x * x * (3 - 2 * x);
  return (randomUnit(seed + i) * (1 - x) + randomUnit(seed + i + 1) * x) * 2 -
         1;
}
struct Oscillator {
  float phase = 0, increment = 0, targetIncrement = 0, gain = 0, targetGain = 0,
        ratio = 1, weight = 1, pan = 0, detuning = 0, driftOffset = 0,
        envelope = 1, pole = 1;
};
struct Packet {
  std::array<float, 8> phases{}, ratios{}, gains{};
  int count = 0, age = 0, rise = 1, sustain = 0, decay = 1;
  float shape = 1, pan = 0, driftFactor = 1, targetDrift = 1;
  std::uint32_t seed = 1;
  bool active() const { return age < rise + sustain + decay && count > 0; }
  float envelope() const {
    float x = age < rise ? float(age) / rise
                         : age < rise + sustain
                               ? 1.f
                               : 1 - float(age - rise - sustain) / decay;
    x = std::clamp(x, 0.f, 1.f);
    x = x * x * (3 - 2 * x);
    return x / (shape + (1 - shape) * x);
  }
};
struct Model {
  Config target{}, prepared{};
  std::array<float, macros> current{};
  std::array<Oscillator, 32> oscillators{};
  std::array<Packet, 4> packets{};
  std::array<float, 2> dc{}, previous{};
  std::uint32_t identity = 1, serial = 0;
  std::uint64_t age = 0;
  float sampleRate = 44100, parameterPole = .01f, dcPole = .999f,
        hz = 261.625565f, lastBase = -1, normal = 1, targetNormal = 1,
        packetMix = 0, readGain = 0;
  int controlCountdown = 0, nextPacket = 0;
  bool smoothing = false, dirty = true, wasAudible = false;
  void reset(const Config &c, std::uint32_t seed, float sr, float noteHz) {
    target = c;
    prepared = {};
    std::copy_n(c.v.begin(), macros, current.begin());
    oscillators = {};
    packets = {};
    dc = {};
    previous = {};
    identity = seed;
    serial = 0;
    age = 0;
    sampleRate = sr;
    parameterPole = 1 - std::exp(-1 / (sr * .006f));
    dcPole = std::exp(-tau * 3 / sr);
    hz = noteHz;
    lastBase = -1;
    nextPacket = 0;
    packetMix = c.v[type] > 1.5f ? 1.f : 0.f;
    readGain = 0;
    controlCountdown = 0;
    dirty = true;
    smoothing = false;
    wasAudible = false;
    for (int j = 0; j < 32; ++j) {
      auto &o = oscillators[j];
      o.phase = randomUnit(seed + std::uint32_t(j) * 7919u) * c.v[phase] * .01f;
      o.pan = randomUnit(seed ^ std::uint32_t(j + 11) * 104729u) * 2 - 1;
      o.detuning = randomUnit(seed ^ std::uint32_t(j + 1) * 65537u) * 2 - 1;
      o.driftOffset = randomUnit(seed + std::uint32_t(j + 19)) * 10;
    }
    prepare();
    for (auto &o : oscillators) {
      o.increment = o.targetIncrement;
      o.gain = o.targetGain;
    }
    normal = targetNormal;
  }
  void beginBlock(const Config &c) {
    if (target.v != c.v) {
      target = c;
      smoothing = true;
      dirty = true;
    }
  }
  void prepare() {
    const auto &p = current;
    const auto &old = prepared.v;
    const int mode = int(p[type]);
    const bool shapeChanged =
        dirty || old[type] != p[type] || old[stretch] != p[stretch] ||
        old[tilt] != p[tilt] || old[count] != p[count] ||
        old[harmonics] != p[harmonics] || old[subs] != p[subs] ||
        old[balance] != p[balance];
    if (shapeChanged) {
      double squares = 0;
      for (int j = 0; j < 32; ++j) {
        auto &o = oscillators[j];
        const bool sub = mode == 1 && j >= 16;
        const float authored = target.v[sub ? divisors + j - 16 : ratios + j];
        const float ratio =
            p[stretch] == 1 ? authored : std::pow(authored, p[stretch]);
        o.ratio = sub ? 1 / ratio : ratio;
        const float enabled =
            std::clamp((mode == 1 ? (sub ? p[subs] : p[harmonics]) : p[count]) -
                           (mode == 1 && sub ? j - 16 : j),
                       0.f, 1.f);
        const float balanceGain =
            mode == 1 ? (sub ? p[balance] * .01f : 1 - p[balance] * .01f) : 1;
        const float tiltGain =
            p[tilt] == 0 ? 1.f
                         : std::pow(std::max(1.f, sub ? ratio * .5f : ratio),
                                    p[tilt] / 6.0206f);
        o.weight = target.v[bands + j] * enabled * balanceGain * tiltGain;
        squares += double(o.weight) * o.weight;
      }
      targetNormal = .7f / std::sqrt(float(std::max(1., squares)));
    }
    const float base = hz * s3g_thru_table::Library::get().pitchRatio(p[tune]);
    if (shapeChanged || base != lastBase || p[detune] != old[detune] ||
        p[drift] != old[drift] || p[drift] > 0) {
      for (int j = 0; j < 32; ++j) {
        auto &o = oscillators[j];
        const float driftCents =
            p[drift] > 0
                ? smoothRandom(identity + std::uint32_t(j) * 7907u,
                               float(age) / sampleRate * p[drift_rate] +
                                   o.driftOffset) *
                      p[drift]
                : 0;
        const float frequency =
            base * o.ratio *
            s3g_thru_table::Library::get().pitchRatio(
                (o.detuning * p[detune] + driftCents) / 100);
        const float guard =
            std::clamp((sampleRate * .48f - frequency) / (sampleRate * .04f),
                       0.f, 1.f) *
            std::clamp((frequency - 5) / 7, 0.f, 1.f);
        o.targetIncrement = std::min(.48f, frequency / sampleRate);
        o.targetGain = o.weight * guard * targetNormal;
      }
      lastBase = base;
    }
    if (dirty || p[damping] != old[damping] || p[time] != old[time])
      for (int j = 0; j < 32; ++j)
        oscillators[j].pole =
            std::exp(-8 * p[damping] * .01f * float(j) / 31 /
                     (sampleRate * std::max(.01f, p[time] * .001f)));
    for (auto &packet : packets)
      if (packet.active())
        packet.targetDrift = s3g_thru_table::Library::get().pitchRatio(
            (p[drift] > 0 ? smoothRandom(packet.seed, float(age) / sampleRate *
                                                          p[drift_rate]) *
                                p[drift]
                          : 0) /
            100);
    if (old[gap] != p[gap])
      nextPacket =
          std::min(nextPacket, int(std::ceil(p[gap] * sampleRate * .001f)));
    for (int j = 0; j < macros; ++j)
      prepared.v[j] = p[j];
    dirty = false;
  }
  void startPacket() {
    Packet *next = nullptr;
    for (auto &p : packets)
      if (!p.active()) {
        next = &p;
        break;
      }
    if (!next)
      return;
    auto &packet = *next;
    const auto seed =
        s3g_thru_table::Library::hash(identity + ++serial * 65537u);
    const auto r = [&](int salt) {
      return randomUnit(seed + std::uint32_t(salt) * 7919u);
    };
    const auto &p = current;
    const float varianceAmount = p[s3g_thru_partials::variation] * .01f;
    packet.age = 0;
    packet.seed = seed;
    packet.driftFactor = packet.targetDrift = 1;
    packet.rise = std::max(
        1, int(std::ceil(p[rise] * (1 + (r(1) * 2 - 1) * varianceAmount) *
                         sampleRate * .001f)));
    packet.sustain = std::max(
        0, int(std::ceil(p[sustain] * (1 + (r(2) * 2 - 1) * varianceAmount) *
                         sampleRate * .001f)));
    packet.decay = std::max(
        1, int(std::ceil(p[decay] * (1 + (r(3) * 2 - 1) * varianceAmount) *
                         sampleRate * .001f)));
    packet.shape = p[shape];
    packet.pan = r(4) * 2 - 1;
    packet.count = std::min(8, int(std::ceil(p[count])));
    const float pitch = s3g_thru_table::Library::get().pitchRatio(
        (r(5) * 2 - 1) * p[pitch_jitter]);
    float squares = 0;
    for (int j = 0; j < packet.count; ++j) {
      const float ratio = target.v[ratios + j];
      packet.ratios[j] =
          (p[stretch] == 1 ? ratio : std::pow(ratio, p[stretch])) * pitch *
          s3g_thru_table::Library::get().pitchRatio((r(20 + j) * 2 - 1) *
                                                    p[detune] / 100);
      packet.gains[j] =
          target.v[bands + j] * std::clamp(p[count] - j, 0.f, 1.f) *
          (p[tilt] == 0 ? 1.f : std::pow(ratio, p[tilt] / 6.0206f));
      packet.phases[j] = r(40 + j) * p[phase] * .01f;
      squares += packet.gains[j] * packet.gains[j];
    }
    const float gain = .55f * (1 - r(6) * varianceAmount * .5f) /
                       std::sqrt(std::max(1.f, squares));
    for (int j = 0; j < packet.count; ++j)
      packet.gains[j] *= gain;
  }
  std::array<float, 2> tick(float noteHz, bool audible) {
    const bool enteringPackets = current[type] < 1.5f && target.v[type] > 1.5f;
    if (enteringPackets)
      nextPacket = 0;
    if (smoothing) {
      smoothing = false;
      for (int f = 0; f < macros; ++f) {
        const float d = target.v[f] - current[f];
        if (f == type || std::abs(d) < 1e-5f * (1 + std::abs(target.v[f])))
          current[f] = target.v[f];
        else {
          current[f] += d * parameterPole;
          smoothing = true;
        }
      }
    }
    hz += (noteHz - hz) * parameterPole;
    if (controlCountdown-- <= 0) {
      controlCountdown = 31;
      prepare();
    }
    normal += (targetNormal - normal) * parameterPole;
    const bool stream = current[type] > 1.5f;
    packetMix += ((stream ? 1.f : 0.f) - packetMix) * parameterPole;
    std::array<float, 2> bank{}, cloud{};
    const auto &sine = s3g_thru_fm::Sines::get();
    for (auto &o : oscillators) {
      o.increment += (o.targetIncrement - o.increment) * parameterPole;
      o.gain += (o.targetGain - o.gain) * parameterPole;
      if (audible && packetMix < .99999f && std::abs(o.gain) > 1e-9f) {
        const float x = sine.read(o.phase) * o.gain * o.envelope;
        bank[0] += x * (1 - std::max(0.f, o.pan));
        bank[1] += x * (1 + std::min(0.f, o.pan));
      }
      o.phase += o.increment;
      if (o.phase >= 1)
        o.phase -= 1;
      o.envelope *= o.pole;
      if (o.envelope < 1e-20f)
        o.envelope = 0;
    }
    if (stream && nextPacket-- <= 0) {
      startPacket();
      const float maximumLife =
          (current[rise] + current[sustain] + current[decay]) *
          (1 + current[variation] * .01f);
      const float scatterFactor =
          1 + (randomUnit(identity + serial * 3671u) * 2 - 1) *
                  current[s3g_thru_partials::scatter] * .009f;
      nextPacket =
          std::max(1, int(std::ceil(std::max(maximumLife / 4 + .05f,
                                             current[gap] * scatterFactor) *
                                    sampleRate * .001f)));
    }
    const float base =
        hz * s3g_thru_table::Library::get().pitchRatio(current[tune]);
    for (auto &packet : packets)
      if (packet.active()) {
        const float envelope = packet.envelope();
        packet.driftFactor +=
            (packet.targetDrift - packet.driftFactor) * parameterPole;
        float sample = 0;
        for (int j = 0; j < packet.count; ++j) {
          const float frequency = base * packet.ratios[j] * packet.driftFactor;
          if (audible && packetMix > 1e-7f) {
            const float guard = std::clamp((sampleRate * .48f - frequency) /
                                               (sampleRate * .04f),
                                           0.f, 1.f);
            sample += sine.read(packet.phases[j]) * packet.gains[j] * guard;
          }
          packet.phases[j] += std::min(.48f, frequency / sampleRate);
          if (packet.phases[j] >= 1)
            packet.phases[j] -= 1;
        }
        cloud[0] += sample * envelope * (1 - std::max(0.f, packet.pan));
        cloud[1] += sample * envelope * (1 + std::min(0.f, packet.pan));
        ++packet.age;
      }
    std::array<float, 2> result{};
    if (audible) {
      if (!wasAudible) {
        readGain = 0;
        dc = {};
        previous = {};
        wasAudible = true;
      }
      readGain += (1 - readGain) * .02f;
      for (int ch = 0; ch < 2; ++ch) {
        const float value = bank[ch] * (1 - packetMix) + cloud[ch] * packetMix;
        dc[ch] = value - previous[ch] + dcPole * dc[ch];
        previous[ch] = value;
        result[ch] = dc[ch] * readGain;
      }
    } else {
      wasAudible = false;
      readGain = 0;
    }
    if ((age++ & 31u) == 0)
      for (int ch = 0; ch < 2; ++ch) {
        if (std::abs(dc[ch]) < 1e-20f)
          dc[ch] = 0;
        if (std::abs(previous[ch]) < 1e-20f)
          previous[ch] = 0;
      }
    return result;
  }
};
struct Instrument {
  std::array<Model, 4> models{};
  std::array<float, 4> laneOutput{};
  float position = 0, noteHz = 261.625565f, positionPole = .01f, lastSlew = -1;
  bool started = false;
  std::uint32_t identity = 1;
  void prepare() {
    (void)s3g_thru_fm::Sines::get();
    (void)s3g_thru_table::Library::get();
  }
  void reset() {
    started = false;
    lastSlew = -1;
  }
  void beginBlock(const std::array<Config, 4> &p) {
    for (int lane = 0; lane < 4; ++lane)
      models[lane].beginBlock(p[lane]);
  }
  void endBlock() {}
  std::array<float, 2> tick(const std::array<Config, 4> &p,
                            float targetPosition, float hz, float sr,
                            float slewMs = 5, const float *gains = nullptr) {
    if (!started) {
      for (int lane = 0; lane < 4; ++lane)
        models[lane].reset(p[lane], identity + std::uint32_t(lane) * 7919u, sr,
                           hz);
      position = targetPosition;
      noteHz = hz;
      started = true;
    }
    if (lastSlew != slewMs) {
      lastSlew = slewMs;
      positionPole = 1 - std::exp(-1 / (sr * std::max(.005f, slewMs * .001f)));
    }
    position = std::clamp(position + (targetPosition - position) * positionPole,
                          0.f, 3.f);
    noteHz += (hz - noteHz) * .01f;
    const int first = int(position), second = std::min(3, first + 1);
    const float at = position - first, blend = at * at * (3 - 2 * at);
    std::array<float, 2> sum{};
    laneOutput = {};
    for (int lane = 0; lane < 4; ++lane) {
      const float gain =
          ((lane == first ? 1 - blend : 0) + (lane == second ? blend : 0)) *
          (gains ? gains[lane] : 1.f);
      const auto audio = models[lane].tick(noteHz, gain > 1e-7f);
      const float level = models[lane].current[s3g_thru_partials::level] *
                          .01f * gain,
                  stereo = models[lane].current[width] * .01f,
                  mid = (audio[0] + audio[1]) * .5f;
      for (int ch = 0; ch < 2; ++ch)
        sum[ch] += (mid + (audio[ch] - mid) * stereo) * level;
      laneOutput[lane] = mid * level;
    }
    return sum;
  }
};
} // namespace s3g_thru_partials
