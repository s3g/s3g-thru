#pragma once
#include "thru_pitch_bank.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace s3g_daisy {
// Shared fixed-size FFT and oscillator tables are prepared before audio starts.
struct ThruSpectralPlan {
    static constexpr std::size_t size = 512, bins = size / 2 + 1, hop = 128, analysisHop = 256;
    static constexpr float tau = 6.2831853071795864769f;
    struct Complex {
        float real = 0, imag = 0;
    };
    std::array<Complex, size / 2> twiddle{};
    std::array<std::uint16_t, size> reverse{};
    std::array<float, size> window{};
    std::array<float, 4097> sine{};
    ThruSpectralPlan() {
        for (std::size_t i = 0; i < size; ++i) {
            std::size_t value = i, bits = 0;
            for (int b = 0; b < 9; ++b) {
                bits = (bits << 1) | (value & 1);
                value >>= 1;
            }
            reverse[i] = static_cast<std::uint16_t>(bits);
            window[i] = .5f - .5f * std::cos(tau * i / size);
        }
        for (std::size_t i = 0; i < twiddle.size(); ++i)
            twiddle[i] = {std::cos(tau * i / size), -std::sin(tau * i / size)};
        for (std::size_t i = 0; i < sine.size(); ++i)
            sine[i] = std::sin(tau * i / 4096.0f);
    }
    static const ThruSpectralPlan &get() {
        static const ThruSpectralPlan plan;
        return plan;
    }
    static float wrap(float cycle) { return cycle - std::floor(cycle + .5f); }
    float sinCycle(float cycle) const {
        cycle -= std::floor(cycle);
        const float p = cycle * 4096;
        const auto i = static_cast<std::size_t>(p);
        return sine[i] + (sine[i + 1] - sine[i]) * (p - i);
    }
    void fft(std::array<Complex, size> &data, bool inverse = false) const {
        for (std::size_t i = 0; i < size; ++i)
            if (i < reverse[i])
                std::swap(data[i], data[reverse[i]]);
        for (std::size_t length = 2; length <= size; length <<= 1)
            for (std::size_t base = 0; base < size; base += length)
                for (std::size_t j = 0; j < length / 2; ++j) {
                    auto w = twiddle[j * size / length];
                    if (inverse)
                        w.imag = -w.imag;
                    const auto a = data[base + j], b = data[base + j + length / 2];
                    const Complex c{b.real * w.real - b.imag * w.imag,
                                    b.real * w.imag + b.imag * w.real};
                    data[base + j] = {a.real + c.real, a.imag + c.imag};
                    data[base + j + length / 2] = {a.real - c.real, a.imag - c.imag};
                }
        if (inverse)
            for (auto &c : data) {
                c.real /= size;
                c.imag /= size;
            }
    }
};
struct ThruSpectralFeature {
    float left = 0, right = 0, hz = 0, phase = 0, delta = 0;
};
struct ThruSpectralBank {
    using Plan = ThruSpectralPlan;
    struct Bin {
        float left = 0, right = 0, hz = 0;
        std::int16_t phase = 0, delta = 0;
    };
    ThruPitchBank pitch;
    std::vector<std::array<Bin, Plan::bins>> frames;
    std::uint64_t revision = 0;
    std::size_t sourceFrames = 0;
    float rate = 44100;
    void analyze(const float *left, const float *right, std::size_t count, float sampleRate,
                 std::uint64_t serial) {
        revision = serial;
        sourceFrames = count;
        rate = sampleRate;
        frames.clear();
        pitch.analyze(left, right, count, sampleRate);
        if (!left || !right || count == 0 || !std::isfinite(sampleRate) || sampleRate <= 0)
            return;
        const auto &plan = Plan::get();
        frames.resize((count + Plan::analysisHop - 1) / Plan::analysisHop + 1);
        std::array<Plan::Complex, Plan::size> l{}, r{};
        std::array<float, Plan::bins> previousL{}, previousR{};
        constexpr std::size_t measurementHop = Plan::analysisHop / 2;
        for (std::size_t f = 0; f < frames.size() * 2 - 1; ++f) {
            for (std::size_t i = 0; i < Plan::size; ++i) {
                const auto at = static_cast<std::int64_t>(f * measurementHop + i) -
                                static_cast<std::int64_t>(Plan::size / 2);
                const bool valid = at >= 0 && static_cast<std::size_t>(at) < count;
                const float a = valid ? left[at] : 0, b = valid ? right[at] : 0;
                l[i] = {std::isfinite(a) ? std::clamp(a, -8.0f, 8.0f) * plan.window[i] : 0, 0};
                r[i] = {std::isfinite(b) ? std::clamp(b, -8.0f, 8.0f) * plan.window[i] : 0, 0};
            }
            plan.fft(l);
            plan.fft(r);
            for (std::size_t k = 0; k < Plan::bins; ++k) {
                const float pl = std::atan2(l[k].imag, l[k].real) / Plan::tau,
                            pr = std::atan2(r[k].imag, r[k].real) / Plan::tau;
                Bin out;
                out.left = std::hypot(l[k].real, l[k].imag);
                out.right = std::hypot(r[k].real, r[k].imag);
                const bool useLeft = out.left >= out.right * .01f;
                const float phase = useLeft ? pl : pr,
                            previous = useLeft ? previousL[k] : previousR[k];
                const float center = static_cast<float>(k) / Plan::size;
                const float frequency =
                    f == 0 ? center
                           : center + Plan::wrap(phase - previous - center * measurementHop) /
                                          measurementHop;
                out.hz = std::max(0.0f, frequency * sampleRate);
                out.phase = static_cast<std::int16_t>(std::lround(pl * 65534));
                out.delta = static_cast<std::int16_t>(std::lround(Plan::wrap(pr - pl) * 65534));
                if ((f & 1) == 0)
                    frames[f / 2][k] = out;
                previousL[k] = pl;
                previousR[k] = pr;
            }
        }
    }
    using Frame = std::array<Bin, Plan::bins>;
    struct ReadFrame {
        const Frame *a = nullptr, *b = nullptr;
        float u = 0;
        ThruSpectralFeature sample(float bin) const {
            if (!a || !b || !std::isfinite(bin) || bin < 0 || bin > Plan::size / 2)
                return {};
            const auto k = static_cast<std::size_t>(bin), next = std::min(Plan::bins - 1, k + 1);
            const float v = bin - k;
            const auto convert = [](const Bin &x) {
                return ThruSpectralFeature{x.left, x.right, x.hz, x.phase / 65534.0f,
                                           x.delta / 65534.0f};
            };
            const auto blend = [](ThruSpectralFeature x, const ThruSpectralFeature &y, float t) {
                x.left += (y.left - x.left) * t;
                x.right += (y.right - x.right) * t;
                x.hz += (y.hz - x.hz) * t;
                x.phase += Plan::wrap(y.phase - x.phase) * t;
                x.delta += Plan::wrap(y.delta - x.delta) * t;
                return x;
            };
            return blend(blend(convert((*a)[k]), convert((*a)[next]), v),
                         blend(convert((*b)[k]), convert((*b)[next]), v), u);
        }
    };
    ReadFrame prepareRead(float unit) const {
        if (frames.empty() || sourceFrames == 0 || !std::isfinite(unit))
            return {};
        const float at =
            std::clamp(unit, 0.f, 1.f) * static_cast<float>(sourceFrames - 1) / Plan::analysisHop;
        const auto a = std::min(frames.size() - 1, static_cast<std::size_t>(at)),
                   b = std::min(frames.size() - 1, a + 1);
        return {&frames[a], &frames[b], at - a};
    }
    ThruSpectralFeature sample(float unit, float bin) const {
        return prepareRead(unit).sample(bin);
    }
};
// One stereo phase-vocoder voice: cached spectra in, fixed-memory WOLA out.
// All engine/parameter transitions share the existing note safety envelopes.
class ThruSpectralVoice {
  public:
    using Plan = ThruSpectralPlan;
    using Spectrum = std::array<ThruSpectralFeature, Plan::bins>;
    void reset() {
        phase_.fill(0);
        left_.fill(0);
        right_.fill(0);
        frequency_.fill(0);
        delta_.fill(0);
        initialized_.fill(false);
        for (auto &ring : ring_)
            ring.fill(0);
        read_ = countdown_ = 0;
    }
    bool needsFrame() const { return countdown_ == 0; }
    void synthesize(const Spectrum &target, float sampleRate, float blurMs, float decayMs,
                    bool accumulate, float tilt) {
        const auto &plan = Plan::get();
        const float seconds = std::max(.008f, blurMs * .001f);
        const float alpha = 1 - std::exp(-static_cast<float>(Plan::hop) / (sampleRate * seconds));
        const float decay = std::exp(-static_cast<float>(Plan::hop) /
                                     (sampleRate * std::max(.05f, decayMs * .001f)));
        for (auto &c : work_)
            c = {};
        for (std::size_t k = 1; k + 1 < Plan::bins; ++k) {
            auto x = target[k];
            if (!std::isfinite(x.left) || !std::isfinite(x.right))
                x.left = x.right = 0;
            if (!std::isfinite(x.hz) || x.hz < 0 || x.hz >= sampleRate * .49f) {
                x.left = x.right = 0;
                x.hz = 0;
            }
            if (!std::isfinite(x.phase))
                x.phase = 0;
            if (!std::isfinite(x.delta))
                x.delta = 0;
            // A silent target fades the old energy without bending its pitch toward DC.
            if (x.left + x.right <= 1e-8f) {
                x.hz = frequency_[k];
                x.delta = delta_[k];
            }
            if (!initialized_[k] && x.left + x.right > 1e-8f) {
                phase_[k] = x.phase;
                frequency_[k] = x.hz;
                delta_[k] = x.delta;
                initialized_[k] = true;
            }
            if (accumulate) {
                const bool replace = x.left + x.right >= left_[k] + right_[k];
                left_[k] *= decay;
                right_[k] *= decay;
                x.left = std::max(x.left, left_[k]);
                x.right = std::max(x.right, right_[k]);
                if (!replace) {
                    x.hz = frequency_[k];
                    x.delta = delta_[k];
                }
            }
            left_[k] += (x.left - left_[k]) * alpha;
            right_[k] += (x.right - right_[k]) * alpha;
            frequency_[k] += (x.hz - frequency_[k]) * alpha;
            delta_[k] += Plan::wrap(x.delta - delta_[k]) * alpha;
            phase_[k] = Plan::wrap(phase_[k] + frequency_[k] * Plan::hop / sampleRate);
            const float shape =
                std::abs(tilt) < 1e-6f
                    ? 1.0f
                    : std::clamp(std::pow(std::max(80.0f, frequency_[k]) / 1000.0f, tilt * .5f),
                                 .25f, 4.0f);
            const float lp = phase_[k], rp = phase_[k] + delta_[k];
            const Plan::Complex l{left_[k] * shape * plan.sinCycle(lp + .25f),
                                  left_[k] * shape * plan.sinCycle(lp)};
            const Plan::Complex r{right_[k] * shape * plan.sinCycle(rp + .25f),
                                  right_[k] * shape * plan.sinCycle(rp)};
            // Pack two real spectra into one complex inverse transform.
            work_[k] = {l.real - r.imag, l.imag + r.real};
            work_[Plan::size - k] = {l.real + r.imag, -l.imag + r.real};
        }
        plan.fft(work_, true);
        for (std::size_t i = 0; i < Plan::size; ++i) {
            const auto at = (read_ + i) % Plan::size;
            const float w = plan.window[i] * (2.0f / 3.0f);
            ring_[0][at] += work_[i].real * w;
            ring_[1][at] += work_[i].imag * w;
        }
        countdown_ = Plan::hop;
    }
    std::array<float, 2> next() {
        std::array<float, 2> result{ring_[0][read_], ring_[1][read_]};
        ring_[0][read_] = ring_[1][read_] = 0;
        read_ = (read_ + 1) % Plan::size;
        if (countdown_)
            --countdown_;
        return result;
    }

  private:
    std::array<float, Plan::bins> phase_{}, left_{}, right_{}, frequency_{}, delta_{};
    std::array<bool, Plan::bins> initialized_{};
    std::array<Plan::Complex, Plan::size> work_{};
    std::array<std::array<float, Plan::size>, 2> ring_{};
    std::size_t read_ = 0, countdown_ = 0;
};
// Linked stereo envelope transfer: the carrier retains its frequencies and
// stereo phase, while the shaper supplies a changing, smoothed band envelope.
struct ThruSpectralImprint {
    using Spectrum = ThruSpectralVoice::Spectrum;
    static void apply(Spectrum &carrier, const Spectrum &shaper, float amount) {
        constexpr std::size_t bins = ThruSpectralPlan::bins;
        std::array<float, bins + 1> a{}, b{};
        for (std::size_t k = 0; k < bins; ++k) {
            const float x = carrier[k].left * carrier[k].left + carrier[k].right * carrier[k].right;
            const float y = shaper[k].left * shaper[k].left + shaper[k].right * shaper[k].right;
            a[k + 1] = a[k] + x * .5f;
            b[k + 1] = b[k] + y * .5f;
        }
        const float floor = std::max(1e-6f, std::sqrt(a[bins] / bins) * .02f);
        for (std::size_t k = 1; k + 1 < bins; ++k) {
            const std::size_t radius = 4, lo = k > radius ? k - radius : 0,
                              hi = std::min(bins, k + radius + 1);
            const float carrierEnvelope = std::sqrt(std::max(0.0f, (a[hi] - a[lo]) / (hi - lo)));
            const float shaperEnvelope = std::sqrt(std::max(0.0f, (b[hi] - b[lo]) / (hi - lo)));
            const float ratio = std::min(8.0f, shaperEnvelope / std::max(floor, carrierEnvelope));
            const float gain = 1 + std::clamp(amount, 0.0f, 1.0f) * (ratio - 1);
            carrier[k].left *= gain;
            carrier[k].right *= gain;
        }
    }
};
} // namespace s3g_daisy
