#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace s3g_daisy {
// Internal note bus, before output/part gain and effects. A shared
// previous-block reference makes capture independent of voice processing order
// and master level.
struct ThruStochasticPartMeter {
    float rms = 0, peak = 0, decay = 0, rate = 0;
    std::size_t block = 0;
    void observe(float energy, std::size_t frames, float sampleRate) {
        if (!frames || sampleRate <= 0)
            return;
        if (rate != sampleRate || block != frames) {
            rate = sampleRate;
            block = frames;
            decay = std::exp(-static_cast<float>(frames) / (2.f * sampleRate));
        }
        rms = std::sqrt(std::max(0.f, energy) / frames);
        peak = std::max(rms, peak * decay);
    }
    float target(float voiceRms) const {
        const float reference = .006f + peak;
        const float activity = std::clamp(rms / reference, 0.f, 1.f);
        const float share = std::clamp(voiceRms / reference, 0.f, 1.f);
        return activity * share * std::sqrt(std::sqrt(share));
    }
};
// Slow negative feedback, never an audio gain or a pitch correction. Rates are
// evaluated once per block. Switching LAW also fades the feedback influence.
struct ThruStochasticListener {
    float capture = 0, enabledMix = 0, mutation = 1, evolution = 1, retention = 1;
    float attack = 0, release = 0, slew = 0, rate = 0;
    std::size_t block = 0;
    void update(bool enabled, float amount, float heard, std::size_t frames, float sampleRate) {
        if (!enabled && capture == 0 && enabledMix == 0)
            return;
        if (rate != sampleRate || block != frames) {
            rate = sampleRate;
            block = frames;
            const float seconds = static_cast<float>(frames) / sampleRate;
            attack = 1 - std::exp(-seconds / .14f);
            release = 1 - std::exp(-seconds / 2.2f);
            slew = 1 - std::exp(-seconds / .025f);
        }
        const float target =
            enabled ? std::clamp(amount, 0.f, 1.f) * std::clamp(heard, 0.f, 1.f) : 0;
        capture += (target - capture) * (target > capture ? attack : release);
        enabledMix += ((enabled ? 1.f : 0.f) - enabledMix) * slew;
        if (!enabled && enabledMix < 1e-6f) {
            capture = enabledMix = 0;
        }
        const float influence = effective();
        mutation = std::max(.04f, std::exp(-3.2f * influence));
        evolution = std::max(.03f, std::exp(-4.2f * influence));
        retention = std::exp(-2.f * influence);
    }
    float effective() const { return capture * enabledMix; }
};
// Persistent PCM deformation. Bounded second-order walks drive accumulated
// signed gain shapes and logarithmic time maps; no audio is synthesized here.
struct ThruStochasticWalk {
    static constexpr int segments = 16;
    struct Shape {
        std::array<float, segments + 1> time{}, slope{}, gain{}, drive{};
        float drift = 1, foldMix = 0, filterGuard = 0;
        Shape() {
            for (int i = 0; i <= segments; ++i) {
                time[i] = static_cast<float>(i) / segments;
                slope[i] = gain[i] = drive[i] = 1;
            }
        }
        struct Point {
            float position, gain, slope, drive;
        };
        Point at(float phase) const {
            const float x = std::clamp(phase, 0.f, 1.f) * segments;
            const int i = std::min(segments - 1, static_cast<int>(x));
            const float t = x - i, t2 = t * t, t3 = t2 * t, m0 = slope[i] / segments,
                        m1 = slope[i + 1] / segments;
            const float p = (2 * t3 - 3 * t2 + 1) * time[i] + (t3 - 2 * t2 + t) * m0 +
                            (-2 * t3 + 3 * t2) * time[i + 1] + (t3 - t2) * m1;
            const float d = ((6 * t2 - 6 * t) * time[i] + (3 * t2 - 4 * t + 1) * m0 +
                             (-6 * t2 + 6 * t) * time[i + 1] + (3 * t2 - 2 * t) * m1) *
                            segments;
            const float smooth = t2 * (3 - 2 * t);
            return {std::clamp(p, 0.f, 1.f), gain[i] + (gain[i + 1] - gain[i]) * smooth,
                    std::max(.0001f, d), drive[i] + (drive[i + 1] - drive[i]) * smooth};
        }
    };
    std::array<float, segments> amplitude{}, duration{}, ampVelocity{}, timeVelocity{},
        ampHistory{}, timeHistory{};
    float period = 0, periodVelocity = 0;
    std::uint32_t random = 1;
    void reset(std::uint32_t seed) {
        *this = {};
        random = seed ? seed : 1;
    }
    float unit() {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        return (random & 0xffffffu) / 16777216.f;
    }
    float deviate(int law) {
        const float a = 2 * unit() - 1;
        if (law == 0)
            return .5f * (a + 2 * unit() - 1);
        if (law == 2)
            return a * a * a * 2;
        return a;
    }
    static float reflect(float x) {
        if (x > 1)
            x = 2 - x;
        if (x < -1)
            x = -2 - x;
        return std::clamp(x, -1.f, 1.f);
    }
    void evolve(float memory, float pull, int law, float mutation = 1, float retention = 1) {
        const float inertia = .1f + .88f * std::clamp(memory, 0.f, 1.f),
                    attraction = .3f * std::clamp(pull, 0.f, 1.f);
        const auto step = [&](float &p, float &v) {
            v = std::clamp(inertia * retention * v +
                               (.22f * deviate(law) - attraction * p) * mutation,
                           -.5f, .5f);
            const float next = p + v * mutation;
            if (next > 1 || next < -1)
                v = -v * .85f;
            p = reflect(next);
        };
        for (int i = 0; i < segments; ++i) {
            step(amplitude[i], ampVelocity[i]);
            step(duration[i], timeVelocity[i]);
            // Multiplicative local stretches accumulate in logarithmic space;
            // Pull restores the identity map without resetting its momentum.
            ampHistory[i] =
                reflect(ampHistory[i] + mutation * (.24f * amplitude[i] + .1f * ampVelocity[i] -
                                                    attraction * ampHistory[i]));
            timeHistory[i] = reflect(timeHistory[i] +
                                     mutation * (.16f * duration[i] + .1f * timeVelocity[i] -
                                                 attraction * timeHistory[i]));
        }
        step(period, periodVelocity);
    }
    Shape shape(float amut, float tmut, float drift) const {
        amut = std::clamp(amut, 0.f, 1.f);
        tmut = std::clamp(tmut, 0.f, 1.f);
        Shape r;
        std::array<float, segments> length{};
        float total = 0;
        const float amplitudeDepth = 3.2f * std::pow(amut, 1.25f),
                    timeDepth = 6 * std::pow(tmut, 1.5f),
                    driveDepth = 47 * std::pow(amut, 2.5f);
        for (int i = 0; i < segments; ++i) {
            length[i] = std::exp2(timeDepth * timeHistory[i]);
            total += length[i];
        }
        float cumulative = 0;
        for (int i = 0; i < segments; ++i) {
            r.time[i] = cumulative / total;
            cumulative += length[i];
            r.gain[i] = 1 + amplitudeDepth * ampHistory[i];
            r.drive[i] = 1 + driveDepth * (.3f + .7f * std::abs(ampHistory[i]));
        }
        r.time[segments] = 1;
        r.gain[segments] = r.gain[0];
        r.drive[segments] = r.drive[0];
        for (int i = 0; i <= segments; ++i) {
            const float a = length[(i + segments - 1) % segments] / total * segments,
                        b = length[i % segments] / total * segments;
            r.slope[i] = 2 * a * b / (a + b);
        }
        const float mix = std::clamp((amut - .2f) / .8f, 0.f, 1.f);
        r.foldMix = mix * mix * (3 - 2 * mix);
        r.filterGuard = .9f * std::max(amut, tmut);
        r.drift = std::exp2(.5f * std::clamp(drift, 0.f, 1.f) * period);
        return r;
    }
};
// First-order antiderivative anti-aliasing of a periodic reflected fold. The
// dry term has the same half-sample reference; changing mix cannot comb it.
struct ThruStochasticFolder {
    double previousDriven = 0;
    float previousDry = 0;
    bool initialized = false;
    static double fold(double x) {
        const double u = x + 1 - 4 * std::floor((x + 1) * .25);
        return 1 - std::abs(u - 2);
    }
    static double integral(double x) {
        const double u = x + 1 - 4 * std::floor((x + 1) * .25);
        return u <= 2 ? .5 * u * u - u : 3 * u - .5 * u * u - 4;
    }
    float process(float input, float drive, float mix) {
        const double x = static_cast<double>(input) * drive;
        if (!initialized) {
            previousDriven = x;
            previousDry = input;
            initialized = true;
        }
        const double delta = x - previousDriven;
        const float folded = static_cast<float>(
            std::abs(delta) < 1e-3 ? fold(.5 * (x + previousDriven))
                                   : (integral(x) - integral(previousDriven)) / delta);
        const float dry = .5f * (input + previousDry);
        previousDriven = x;
        previousDry = input;
        return dry + (folded - dry) * std::clamp(mix, 0.f, 1.f);
    }
};
} // namespace s3g_daisy
