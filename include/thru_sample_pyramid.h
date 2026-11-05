#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace s3g_daisy {
// Immutable stereo decimation levels, built only with the source analysis on
// a worker. Centered filtering keeps every level at the original source time.
// At most one additional stereo source's worth of storage, shared by notes.
struct ThruSamplePyramid {
    static constexpr std::size_t levels = 9;
    struct Level {
        std::array<std::vector<float>, 2> pcm;
    };
    std::array<Level, levels> data;
    std::size_t frames = 0;
    static const std::array<float, 31> &coefficients() {
        static const auto taps = [] {
            std::array<float, 31> result{};
            double total = 0;
            for (int i = 0; i < 31; ++i) {
                const int n = i - 15;
                const double w = .42 - .5 * std::cos(6.283185307179586 * i / 30) +
                                 .08 * std::cos(12.566370614359172 * i / 30);
                const double value =
                    (n ? std::sin(6.283185307179586 * .20 * n) / (3.141592653589793 * n) : .4) * w;
                result[i] = static_cast<float>(value);
                total += value;
            }
            for (auto &v : result)
                v = static_cast<float>(v / total);
            return result;
        }();
        return taps;
    }
    void analyze(const float *left, const float *right, std::size_t count) {
        frames = count;
        const float *source[2]{left, right};
        std::size_t size = count;
        const auto &taps = coefficients();
        for (auto &level : data) {
            const auto next = (size + 1) / 2;
            for (int ch = 0; ch < 2; ++ch) {
                auto &output = level.pcm[ch];
                output.resize(next);
                for (std::size_t i = 0; i < next; ++i) {
                    double sample = 0;
                    for (int k = 0; k < 31; ++k) {
                        const auto at =
                            std::clamp<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(i * 2) + k - 15,
                                                       0, static_cast<std::ptrdiff_t>(size - 1));
                        sample += source[ch][at] * taps[k];
                    }
                    output[i] = static_cast<float>(sample);
                }
                source[ch] = output.data();
            }
            size = next;
        }
    }
    static float linear(const float *pcm, std::size_t count, double position) {
        if (!pcm || !count)
            return 0;
        position = std::clamp(position, 0.0, static_cast<double>(count - 1));
        const auto i = static_cast<std::size_t>(position);
        const auto next = std::min(i + 1, count - 1);
        return pcm[i] + (pcm[next] - pcm[i]) * static_cast<float>(position - i);
    }
    struct Read {
        const float *first = nullptr;
        const float *second = nullptr;
        std::size_t firstCount = 0, secondCount = 0;
        double firstPosition = 0, secondPosition = 0;
        float blend = 0;
    };
    Read prepare(const float *original, std::size_t count, std::size_t channel, double position,
                 float lod) const {
        Read r;
        const float level = std::clamp(lod, 0.f, static_cast<float>(levels));
        const int a = static_cast<int>(level), b = std::min(a + 1, static_cast<int>(levels));
        r.blend = level - a;
        const auto set = [&](int n, const float *&pcm, std::size_t &size, double &p) {
            pcm = n ? data[n - 1].pcm[channel].data() : original;
            size = n ? data[n - 1].pcm[channel].size() : count;
            p = position / static_cast<double>(1u << n);
        };
        set(a, r.first, r.firstCount, r.firstPosition);
        set(b, r.second, r.secondCount, r.secondPosition);
        return r;
    }
    static float read(const Read &r) {
        const float first = linear(r.first, r.firstCount, r.firstPosition);
        return r.blend > 0
                   ? first + (linear(r.second, r.secondCount, r.secondPosition) - first) * r.blend
                   : first;
    }
};
} // namespace s3g_daisy
