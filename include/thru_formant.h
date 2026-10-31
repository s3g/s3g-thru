#pragma once
#include "thru_spectral.h"

namespace s3g_daisy {
// Bounded time-domain PSOLA. Old grains retain their increments and finish
// their Hann windows; edits affect only new grains. Stereo uses common marks.
class ThruFormantVoice {
  public:
    struct Stereo {
        float left = 0, right = 0;
    };
    struct Articulation {
        float tune = 0, frequency = 261.625565f, rough = 0, sub = 0;
        std::uint32_t seed = 1;
    };
    void reset() {
        for (auto &g : grains_)
            g = {};
        countdown_ = 0;
        initialized_ = false;
    }
    template <class Read>
    Stereo render(const ThruPitchBank *bank, double position, double low, double high,
                  float sourceRate, float outputRate, float pitch, float formant, float alpha,
                  float smoothingMs, float overlap, const Articulation &articulation, bool enabled,
                  Read read) {
        Stereo out;
        if (!enabled) {
            if (initialized_)
                reset();
            return out;
        }
        if (!initialized_) {
            pitch_ = pitch;
            formant_ = formant;
            period_ = bank ? bank->sample(position).period : sourceRate / 150;
            tune_ = articulation.tune;
            frequency_ = articulation.frequency;
            rough_ = articulation.rough;
            sub_ = articulation.sub;
            random_ = articulation.seed ? articulation.seed : 1;
            jitter_ = 0;
            subPhase_ = 0;
            clockInterval_ = std::max(1.0, period_ * outputRate / sourceRate / pitch_);
            initialized_ = true;
        }
        slew(pitch_, pitch, alpha);
        slew(formant_, formant, alpha);
        slew(tune_, articulation.tune, alpha);
        slew(frequency_, articulation.frequency, alpha);
        slew(rough_, articulation.rough, alpha);
        slew(sub_, articulation.sub, alpha);
        if (countdown_ <= 0) {
            const auto p =
                bank ? bank->sample(position) : ThruPitchBank::Point{sourceRate / 150, 0};
            const float confidence = std::clamp((p.confidence - .6f) / .25f, 0.f, 1.f);
            const float voiced = confidence * confidence * (3 - 2 * confidence);
            const double desiredPeriod = p.period > 0 ? p.period : sourceRate / 150;
            const double oldInterval = std::clamp(period_ * outputRate / sourceRate / pitch_,
                                                  outputRate / 6000.0, outputRate / 20.0);
            const float step =
                1 - std::exp(-static_cast<float>(oldInterval) / (outputRate * smoothingMs * .001f));
            period_ += (desiredPeriod - period_) * step;
            const double freeHz = sourceRate / period_ * pitch_;
            const float lock = std::clamp(tune_ * voiced, 0.f, 1.f);
            const double hz = lock < 1e-6f
                                  ? freeHz
                                  : lock > .999999f
                                        ? frequency_
                                        : std::exp(std::log(std::max(1e-6, freeHz)) * (1 - lock) +
                                                   std::log(std::max(1.f, frequency_)) * lock);
            const double interval = outputRate / std::clamp(hz, 20.0, 6000.0);
            double clock = interval;
            if (rough_ > 1e-6f) {
                random_ ^= random_ << 13;
                random_ ^= random_ >> 17;
                random_ ^= random_ << 5;
                const float value = (random_ & 0xffffffu) / 8388608.f - 1;
                jitter_ += (value - jitter_) * .55f;
                clock = std::clamp(interval * (1 + .3f * rough_ * jitter_), outputRate / 6000.0,
                                   outputRate / 20.0);
            }
            clockInterval_ = clock;
            for (auto &g : grains_)
                if (!g.active) {
                    const double ratio = pitch_ + (formant_ - pitch_) * voiced;
                    g.increment = sourceRate / outputRate * ratio;
                    const double duration = std::min(overlap * period_ / g.increment, 6 * interval);
                    g.duration = std::max(4.0, duration);
                    const double mark =
                        bank ? bank->nearestMark(position, desiredPeriod) : position;
                    const double center = position + (mark - position) * voiced;
                    g.position = std::clamp(center, low, high) - g.duration * .5 * g.increment;
                    g.low = low;
                    g.high = high;
                    g.edge = std::max(1.0, std::min((high - low) * .25, sourceRate * .001));
                    g.age = 0;
                    g.active = true;
                    break;
                }
            countdown_ += clock;
        }
        --countdown_;
        float sum = 0;
        const auto &plan = ThruSpectralPlan::get();
        for (auto &g : grains_)
            if (g.active) {
                const float u = static_cast<float>(g.age / g.duration);
                const float envelope = .5f - .5f * plan.sinCycle(u + .25f);
                const double at = g.position + g.age * g.increment;
                float edge = static_cast<float>(
                    std::clamp(std::min(at - g.low, g.high - at) / g.edge, 0.0, 1.0));
                edge = edge * edge * (3 - 2 * edge);
                const auto sample = read(at, g.increment);
                out.left += sample.left * envelope * edge;
                out.right += sample.right * envelope * edge;
                sum += envelope;
                if (++g.age >= g.duration)
                    g.active = false;
            }
        // Continuous alternating-cycle emphasis remains audible even when
        // long windows average several grains. It adds no grain readers and
        // never increases peak gain or resets phase on a parameter change.
        const float emphasis =
            sub_ > 1e-6f
                ? 1 - .5f * sub_ * (1 - plan.sinCycle(static_cast<float>(subPhase_) + .25f))
                : 1;
        subPhase_ += 1 / (2 * clockInterval_);
        if (subPhase_ >= 1)
            subPhase_ -= 1;
        const float gain = emphasis / std::max(1.f, sum);
        out.left *= gain;
        out.right *= gain;
        return out;
    }

  private:
    static void slew(float &value, float target, float alpha) {
        const float next = value + (target - value) * alpha;
        value = (std::abs(target - value) < 1e-6f || next == value) ? target : next;
    }
    struct Grain {
        double position = 0, increment = 0, age = 0, duration = 0, low = 0, high = 0, edge = 1;
        bool active = false;
    };
    std::array<Grain, 12> grains_{};
    double countdown_ = 0, period_ = 294;
    float pitch_ = 1, formant_ = 1;
    float tune_ = 0, frequency_ = 261.625565f, rough_ = 0, sub_ = 0, jitter_ = 0;
    double subPhase_ = 0, clockInterval_ = 294;
    std::uint32_t random_ = 1;
    bool initialized_ = false;
};
} // namespace s3g_daisy
