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
    void reset() { channels_ = {}; write_ = residualFrames_ = 0u; }
    // Optional residual fade is used by Layers when waking an idle nonlinear path.
    std::array<float, 2u> process(const std::array<float, 2u>& input, float drive,
                                float residualGain = 1.0f)
    {
        std::array<float, 2u> result {};
        // At zero drive the shaper's residual is exactly zero. Keep the
        // upsampler input history hot for an immediate future edit, and drain
        // the complete downsampler tail before skipping its zero arithmetic.
        // 64 base-rate frames exceed both cascaded FIR tails (under 32).
        const bool nonlinear = drive != 0.0f || residualFrames_ != 0u;
        if (drive != 0.0f) residualFrames_ = 64u;
        else if (residualFrames_ != 0u) --residualFrames_;
        for (std::size_t ch = 0u; ch < 2u; ++ch) {
            auto& c = channels_[ch];
            const float dry = c.dry[write_];
            c.dry[write_] = input[ch];
            float residual = 0.0f;
            if (!nonlinear) {
                // Only upOne's output feeds future upTwo history; upTwo's
                // convolution cannot affect sound until a nonzero shaper.
                for (std::size_t p = 0u; p < 2u; ++p) {
                    const float up = c.upOne.process(p == 0u ? input[ch] * 2.0f : 0.0f);
                    c.upTwo.push(up * 2.0f);c.upTwo.push(0.0f);
                }
                result[ch] = dry;
                continue;
            }
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
            result[ch] = dry + residual * residualGain;
        }
        if (++write_ == latencyFrames) write_ = 0u;
        return result;
    }
    std::array<float, 2u> processClean(const std::array<float, 2u>& input)
    {
        std::array<float, 2u> result{};
        for(std::size_t ch=0;ch<2;++ch) {
            result[ch]=channels_[ch].dry[write_];
            channels_[ch].dry[write_]=input[ch];
        }
        if(++write_==latencyFrames)write_=0;
        return result;
    }
    void resetNonlinearHistory()
    {
        for(auto& channel:channels_){channel.upOne={};channel.upTwo={};channel.downOne={};channel.downTwo={};}
        residualFrames_=0u;
    }
private:
    struct HalfBand {
        // Mirror the 33-sample FIR history so every tap has a direct address.
        // Coefficients, accumulation order and delay are unchanged. Avoiding
        // modulo for every tap matters for the five drives in a Quad/Sequence
        // mix, including clean drives whose nonlinear residual is zero.
        std::array<float, 66u> history {};
        std::size_t write = 0u;
        void push(float sample) {
            history[write] = history[write + 33u] = sample;
            if (++write == 33u) write = 0u;
        }
        float process(float sample)
        {
            static constexpr float coefficients[33u] {
                0.00000000000f, -0.0000746390270651f, 0.00000000000f, 0.000853939593532f, 0.00000000000f, -0.00322899956814f, 0.00000000000f, 0.00878936036566f, 0.00000000000f, -0.0201708049171f, 0.00000000000f, 0.0424680634392f, 0.00000000000f, -0.0919110080345f, 0.00000000000f, 0.313274088148f, 0.500000000000f, 0.313274088148f, 0.00000000000f, -0.0919110080345f, 0.00000000000f, 0.0424680634392f, 0.00000000000f, -0.0201708049171f, 0.00000000000f, 0.00878936036566f, 0.00000000000f, -0.00322899956814f, 0.00000000000f, 0.000853939593532f, 0.00000000000f, -0.0000746390270651f, 0.00000000000f
            };
            history[write] = history[write + 33u] = sample;
            float sum = history[write + 33u - 16u] * 0.5f;
            for (std::size_t tap = 1u; tap < 33u; tap += 2u)
                sum += history[write + 33u - tap] * coefficients[tap];
            if (++write == 33u) write = 0u;
            return sum;
        }
    };
    struct Channel {
        HalfBand upOne, upTwo, downTwo, downOne;
        std::array<float, latencyFrames> dry {};
    };
    std::array<Channel, 2u> channels_ {};
    std::size_t write_ = 0u, residualFrames_ = 0u;
};
} // namespace s3g_daisy
