#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace s3g_daisy {

// Two bounded sample/hold banks preserve the intended quantizer and reducer
// character, but changing their settings does not hard-switch the output.
class ThruLoFi {
public:
    void prepare(float sampleRate)
    {
        duration_ = std::max<std::size_t>(1u,
            static_cast<std::size_t>(std::lround(sampleRate * 0.010f)));
        current_ = destination_ = Bank {};
        requestedBits_ = 0.0f;
        requestedRate_ = 1u;
        remaining_ = 0u;
    }

    void request(float bits, std::size_t rate)
    {
        requestedBits_ = std::clamp(bits, 0.0f, 1.0f);
        requestedRate_ = std::clamp<std::size_t>(rate, 1u, 16u);
        startPending();
    }

    std::array<float, 2u> process(const std::array<float, 2u>& input)
    {
        auto output = current_.process(input);
        if (remaining_ == 0u) return output;
        const auto to = destination_.process(input);
        float mix = 1.0f - static_cast<float>(remaining_) / duration_;
        mix = mix * mix * (3.0f - 2.0f * mix);
        for (std::size_t channel = 0u; channel < 2u; ++channel)
            output[channel] += (to[channel] - output[channel]) * mix;
        if (--remaining_ == 0u) {
            current_ = destination_;
            startPending();
        }
        return output;
    }

private:
    struct Bank {
        float bits = 0.0f, quantization = 32768.0f;
        std::size_t rate = 1u, countdown = 0u;
        std::array<float, 2u> held {};

        std::array<float, 2u> process(const std::array<float, 2u>& input)
        {
            if (countdown == 0u) {
                for (std::size_t channel = 0u; channel < 2u; ++channel)
                    held[channel] = bits > 1.0e-5f
                        ? std::round(input[channel] * quantization) / quantization
                        : input[channel];
                countdown = rate;
            }
            --countdown;
            return held;
        }
    };

    void startPending()
    {
        if (remaining_ != 0u || (requestedBits_ == current_.bits
            && requestedRate_ == current_.rate)) return;
        destination_ = Bank {};
        destination_.bits = requestedBits_;
        destination_.quantization = std::pow(2.0f, 15.0f - requestedBits_ * 12.0f);
        destination_.rate = requestedRate_;
        remaining_ = duration_;
    }

    Bank current_, destination_;
    float requestedBits_ = 0.0f;
    std::size_t requestedRate_ = 1u, remaining_ = 0u, duration_ = 1u;
};

} // namespace s3g_daisy
