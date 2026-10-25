#pragma once

#include <array>
#include <cstddef>
#include <cmath>

namespace s3g_daisy {
// Four-times oversampled nonlinear residual. The clean signal takes an exact
// 24-frame delay; only the added distortion passes through the antialias filters.
// Thus DRIVE=0 retains every clean sample, with constant latency during edits.
class ThruDrive {
public:
    static constexpr std::size_t latencyFrames = 24u;
    void reset() { channels_ = {}; write_ = 0u; }
    std::array<float, 2u> process(const std::array<float, 2u>& input, float drive)
    {
        std::array<float, 2u> result {};
        for (std::size_t ch = 0u; ch < 2u; ++ch) {
            auto& c = channels_[ch];
            const float dry = c.dry[write_];
            c.dry[write_] = input[ch];
            float residual = 0.0f;
            for (std::size_t p = 0u; p < 2u; ++p) {
                const float up = c.upOne.process(p == 0u ? input[ch] * 2.0f : 0.0f);
                float down = 0.0f;
                for (std::size_t q = 0u; q < 2u; ++q) {
                    const float sample = c.upTwo.process(q == 0u ? up * 2.0f : 0.0f);
                    const float shaped = sample * (1.0f + drive)
                        / (1.0f + drive * std::abs(sample));
                    const float filtered = c.downTwo.process(shaped - sample);
                    if (q == 0u) down = filtered;
                }
                const float filtered = c.downOne.process(down);
                if (p == 0u) residual = filtered;
            }
            result[ch] = dry + residual;
        }
        if (++write_ == latencyFrames) write_ = 0u;
        return result;
    }
private:
    struct HalfBand {
        std::array<float, 33u> history {};
        std::size_t write = 0u;
        float process(float sample)
        {
            static constexpr float coefficients[33u] {
                0.00000000000f, -0.0000746390270651f, 0.00000000000f, 0.000853939593532f, 0.00000000000f, -0.00322899956814f, 0.00000000000f, 0.00878936036566f, 0.00000000000f, -0.0201708049171f, 0.00000000000f, 0.0424680634392f, 0.00000000000f, -0.0919110080345f, 0.00000000000f, 0.313274088148f, 0.500000000000f, 0.313274088148f, 0.00000000000f, -0.0919110080345f, 0.00000000000f, 0.0424680634392f, 0.00000000000f, -0.0201708049171f, 0.00000000000f, 0.00878936036566f, 0.00000000000f, -0.00322899956814f, 0.00000000000f, 0.000853939593532f, 0.00000000000f, -0.0000746390270651f, 0.00000000000f
            };
            history[write] = sample;
            float sum = history[(write + 33u - 16u) % 33u] * 0.5f;
            for (std::size_t tap = 1u; tap < 33u; tap += 2u)
                sum += history[(write + 33u - tap) % 33u] * coefficients[tap];
            if (++write == 33u) write = 0u;
            return sum;
        }
    };
    struct Channel {
        HalfBand upOne, upTwo, downTwo, downOne;
        std::array<float, latencyFrames> dry {};
    };
    std::array<Channel, 2u> channels_ {};
    std::size_t write_ = 0u;
};
} // namespace s3g_daisy
