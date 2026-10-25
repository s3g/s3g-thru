#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace s3g_daisy {

// Immutable after publication. One bank is shared by every note reading a
// source. Construction, allocation and destruction belong to a worker.
struct ThruWavesetCrossing {
    double position = 0.0;
    float level[2u] {};
    float slope[2u] {};
    float peak = 0.0f;
};

struct ThruWavesetBank {
    std::array<std::vector<ThruWavesetCrossing>, 5u> crossings;
    std::size_t frames = 0u;
    float sampleRate = 44100.0f;
    std::uint64_t revision = 0u;
    std::uint64_t analyzedFrames = 0u;
    std::size_t referenceChannel = 0u;

    static float read(const float* data, std::size_t size, double position)
    {
        if (!data || size == 0u) return 0.0f;
        position = std::clamp(position, 0.0, static_cast<double>(size - 1u));
        const auto first = static_cast<std::size_t>(position);
        const auto next = std::min(first + 1u, size - 1u);
        return data[first] + (data[next] - data[first])
            * static_cast<float>(position - first);
    }

    static float slope(const float* data, std::size_t size, double position)
    {
        if (!data || size < 2u) return 0.0f;
        const double low = std::clamp(position - 0.5, 0.0, static_cast<double>(size - 1u));
        const double high = std::clamp(position + 0.5, 0.0, static_cast<double>(size - 1u));
        return high > low ? static_cast<float>((read(data, size, high)
            - read(data, size, low)) / (high - low)) : 0.0f;
    }

    // Call only outside audio rendering. All five analysis filters retain
    // continuous state, so a crossing never depends on the search start.
    void analyze(const float* left, const float* right, std::size_t size,
                 float rate, std::uint64_t sourceRevision = 1u)
    {
        frames = size;
        sampleRate = rate;
        revision = sourceRevision;
        analyzedFrames = 0u;
        for (auto& map : crossings) map.clear();
        if (!left || !right || size < 8u || !(rate > 0.0f)) return;
        double energy[2u] {};
        for (std::size_t frame = 0u; frame < size; ++frame) {
            energy[0u] += static_cast<double>(left[frame]) * left[frame];
            energy[1u] += static_cast<double>(right[frame]) * right[frame];
        }
        referenceChannel = energy[0u] < energy[1u] * 1.0e-8 ? 1u : 0u;
        const float* reference = referenceChannel == 0u ? left : right;
        constexpr double cutoffs[5u] {0.0, 8000.0, 4000.0, 1000.0, 250.0};
        std::array<double, 5u> coefficient {}, filtered {};
        std::array<std::size_t, 5u> lastFrame {};
        std::array<float, 5u> peak {};
        for (std::size_t detail = 0u; detail < 5u; ++detail) {
            coefficient[detail] = detail == 0u ? 1.0
                : 1.0 - std::exp(-6.283185307179586 * cutoffs[detail] / rate);
            filtered[detail] = reference[0u];
            crossings[detail].reserve(size / 32u + 2u);
        }
        for (std::size_t frame = 1u; frame < size; ++frame) {
            ++analyzedFrames;
            const float raw = reference[frame];
            for (std::size_t detail = 0u; detail < 5u; ++detail) {
                auto& map = crossings[detail];
                const double previous = filtered[detail];
                filtered[detail] += coefficient[detail] * (raw - previous);
                peak[detail] = std::max(peak[detail], std::abs(raw));
                if (previous > 0.0 || filtered[detail] <= 0.0
                    || (!map.empty() && frame - lastFrame[detail] < 4u)) continue;
                const double fraction = std::clamp(-previous
                    / (filtered[detail] - previous), 0.0, 1.0);
                ThruWavesetCrossing crossing;
                crossing.position = static_cast<double>(frame - 1u) + fraction;
                crossing.level[0u] = read(left, size, crossing.position);
                crossing.level[1u] = read(right, size, crossing.position);
                crossing.slope[0u] = slope(left, size, crossing.position);
                crossing.slope[1u] = slope(right, size, crossing.position);
                if (!map.empty()) map.back().peak = peak[detail];
                map.push_back(crossing);
                lastFrame[detail] = frame;
                peak[detail] = std::abs(raw);
            }
        }
    }
};

} // namespace s3g_daisy
