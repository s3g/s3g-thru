#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace s3g_daisy {
// Immutable windows initialize when the module loads, never on a note trigger.
struct ThruPulsar {
    static constexpr std::size_t readers = 4, tableSize = 256;
    inline static const std::array<std::array<float, tableSize + 1>, 5> windows = [] {
        std::array<std::array<float, tableSize + 1>, 5> t{};
        for (std::size_t i = 0; i <= tableSize; ++i) {
            const float p = static_cast<float>(i) / tableSize;
            const auto smooth = [](float x) {
                x = std::clamp(x, 0.f, 1.f);
                return x * x * (3 - 2 * x);
            };
            t[0][i] = .5f - .5f * std::cos(6.28318530718f * p);
            t[1][i] = 16 * p * p * (1 - p) * (1 - p);
            t[2][i] = smooth(std::min(p, 1 - p) * 4);
            t[3][i] = smooth(p / .12f) * smooth((1 - p) / .12f) * std::exp(-6 * p);
        }
        float peak = 0;
        for (float v : t[3])
            peak = std::max(peak, v);
        for (std::size_t i = 0; i <= tableSize; ++i)
            t[3][i] /= peak;
        for (std::size_t i = 0; i <= tableSize; ++i)
            t[4][i] = t[3][tableSize - i];
        for (auto &w : t)
            w[0] = w[tableSize] = 0;
        return t;
    }();
    inline static const std::array<float, tableSize + 1> modulationSine = [] {
        std::array<float, tableSize + 1> t{};
        for (std::size_t i = 0; i < tableSize; ++i)
            t[i] = std::sin(6.283185307179586 * static_cast<double>(i) / tableSize);
        t[tableSize] = t[0];
        return t;
    }();
    inline static const std::array<float, tableSize + 1> emissionRatios = [] {
        std::array<float, tableSize + 1> t{};
        for (std::size_t i = 0; i <= tableSize; ++i)
            t[i] = std::exp2((-24.f + 48.f * i / tableSize) / 12);
        return t;
    }();
    static float modulation(double phase) {
        const double x = std::clamp(phase, 0., 1.) * tableSize;
        const auto at = std::min(tableSize - 1, static_cast<std::size_t>(x));
        return modulationSine[at] +
               (modulationSine[at + 1] - modulationSine[at]) * static_cast<float>(x - at);
    }
    static float emissionRatio(float semitones) {
        const float x = (std::clamp(semitones, -24.f, 24.f) + 24) * tableSize / 48;
        const auto at = std::min(tableSize - 1, static_cast<std::size_t>(x));
        return emissionRatios[at] + (emissionRatios[at + 1] - emissionRatios[at]) * (x - at);
    }
    static float window(float phase, int shape, float skew) {
        if (phase <= 0 || phase >= 1)
            return 0;
        const float peak = .1f + .8f * std::clamp(skew, 0.f, 1.f);
        const float p =
            phase < peak ? .5f * phase / peak : .5f + .5f * (phase - peak) / (1 - peak);
        const float x = std::clamp(p, 0.f, 1.f) * tableSize;
        const auto at = std::min(tableSize - 1, static_cast<std::size_t>(x));
        const auto &t = windows[std::clamp(shape, 0, 4)];
        return t[at] + (t[at + 1] - t[at]) * (x - at);
    }
    // Continuous, then explicit pulse/rest ratios. The clock never restarts on
    // a mask edit. Counts express attempted emissions, including omitted ones.
    inline static constexpr int bursts[9][2] = {{1, 0}, {2, 1}, {3, 1}, {4, 1}, {1, 1},
                                                {2, 2}, {3, 3}, {4, 4}, {8, 8}};
    static bool burstAllows(std::uint64_t event, int pattern) {
        const auto &b = bursts[std::clamp(pattern, 0, 8)];
        return event % static_cast<unsigned>(b[0] + b[1]) < static_cast<unsigned>(b[0]);
    }
    static float random(std::uint32_t &state) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (state & 0xffffffu) / 16777216.f;
    }
};
} // namespace s3g_daisy
