#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <new>

namespace s3g_daisy {

// Linked stereo lookahead limiter. All storage and attack curves are prepared
// off the audio thread. A newly detected peak schedules its own complete ramp;
// it never inherits an older peak's nearly expired attack deadline.
class ThruOutputLimiter {
public:
    bool prepare(float sampleRate, float ceiling)
    {
        if (!std::isfinite(sampleRate) || sampleRate <= 0.0f
            || !std::isfinite(ceiling) || ceiling <= 0.0f) return false;
        lookahead_ = std::max<std::size_t>(1u,
            static_cast<std::size_t>(std::lround(sampleRate * 0.0015f)));
        capacity_ = lookahead_ + 1u;
        data_.reset(new (std::nothrow) float[capacity_ * 4u]);
        if (!data_) { capacity_ = 0u; return false; }
        ceiling_ = ceiling;
        release_ = 1.0f - std::exp(-1.0f / (sampleRate * 0.080f));
        float* curve = data_.get() + capacity_ * 3u;
        for (std::size_t frame = 0u; frame < capacity_; ++frame) {
            const float phase = static_cast<float>(frame) / lookahead_;
            curve[frame] = 0.5f + 0.5f * std::cos(3.14159265358979323846f * phase);
        }
        // Exact endpoints matter for both unity transparency and the ceiling.
        curve[0u] = 1.0f;
        curve[lookahead_] = 0.0f;
        reset();
        return true;
    }

    void reset()
    {
        write_ = hold_ = 0u;
        gain_ = 1.0f;
        if (!data_) return;
        std::fill_n(data_.get(), capacity_ * 2u, 0.0f);
        std::fill_n(data_.get() + capacity_ * 2u, capacity_, 1.0f);
    }

    float gain() const { return gain_; }
    std::size_t lookaheadFrames() const { return lookahead_; }

    void process(float inputLeft, float inputRight, float& outputLeft, float& outputRight)
    {
        inputLeft = std::isfinite(inputLeft) ? inputLeft : 0.0f;
        inputRight = std::isfinite(inputRight) ? inputRight : 0.0f;
        if (!data_ || capacity_ == 0u) {
            outputLeft = std::clamp(inputLeft, -ceiling_, ceiling_);
            outputRight = std::clamp(inputRight, -ceiling_, ceiling_);
            return;
        }
        float* left = data_.get();
        float* right = left + capacity_;
        float* schedule = right + capacity_;
        const float* curve = schedule + capacity_;
        left[write_] = inputLeft;
        right[write_] = inputRight;
        const float peak = std::max(std::abs(inputLeft), std::abs(inputRight));
        const float required = peak > ceiling_ ? ceiling_ / peak : 1.0f;
        if (required < 1.0f) {
            hold_ = lookahead_;
            // A gain already below this requirement stays held until this
            // peak is emitted. Only stricter peaks need additional work.
            if (required < gain_) {
                std::size_t slot = write_;
                for (std::size_t ahead = 0u; ahead < capacity_; ++ahead) {
                    const float limit = required + (1.0f - required) * curve[ahead];
                    schedule[slot] = std::min(schedule[slot], limit);
                    if (++slot == capacity_) slot = 0u;
                }
            }
        }
        const float limit = schedule[write_];
        schedule[write_] = 1.0f;
        if (hold_ != 0u) --hold_;
        else gain_ += (1.0f - gain_) * release_;
        gain_ = std::min(gain_, limit);

        const std::size_t read = write_ + 1u == capacity_ ? 0u : write_ + 1u;
        outputLeft = std::clamp(left[read] * gain_, -ceiling_, ceiling_);
        outputRight = std::clamp(right[read] * gain_, -ceiling_, ceiling_);
        if (++write_ == capacity_) write_ = 0u;
    }

private:
    std::unique_ptr<float[]> data_;
    std::size_t capacity_ = 0u, lookahead_ = 0u, write_ = 0u, hold_ = 0u;
    float ceiling_ = 0.8912509381337456f, release_ = 1.0f, gain_ = 1.0f;
};

} // namespace s3g_daisy
