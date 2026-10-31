#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <new>

namespace s3g_daisy {
// Original implementation of a Dattorro-style cross-coupled plate and a
// compact four-line orthogonal FDN room. Delay times for the plate follow
// Jon Dattorro, Effect Design, Part 1 (JAES 45/9, 1997). All storage is
// allocated at prepare time. Reset invalidates history without clearing it.
class ThruReverb {
    struct Line {
        std::unique_ptr<float[]> data;
        std::size_t capacity = 0, write = 0, valid = 0;
        float length = 1;
        bool prepare(float frames, float margin = 2) {
            length = frames;
            capacity = static_cast<std::size_t>(std::ceil(frames + margin)) + 2;
            data.reset(new (std::nothrow) float[capacity] {});
            reset(); return bool(data);
        }
        void reset() { write = valid = 0; }
        float tap(std::size_t lag) const {
            if (lag == 0 || lag > valid) return 0;
            return data[(write + capacity - lag) % capacity];
        }
        float read(float lag) const {
            lag = std::clamp(lag, 1.0f, float(capacity - 2));
            const auto n = static_cast<std::size_t>(lag);
            const float a = tap(n), b = tap(n + 1);
            return a + (b - a) * (lag - float(n));
        }
        void push(float value) {
            // Avoid denormals in long quiet tails without a DC bias.
            data[write] = std::abs(value) < 1.0e-20f ? 0.0f : value;
            if (++write == capacity) write = 0;
            if (valid < capacity - 1) ++valid;
        }
        float delay(float value, float modulation = 0) {
            const float out = read(length + modulation); push(value); return out;
        }
        float allpass(float value, float coefficient, float modulation = 0) {
            const float out = read(length + modulation) - coefficient * value;
            push(value + coefficient * out); return out;
        }
    };
public:
    bool prepare(float rate) {
        if (!std::isfinite(rate) || rate < 8000 || rate > 192000) return false;
        rate_ = rate; scale_ = rate / 29761.0f;
        constexpr float times[] = {142,107,379,277,672,4453,1800,3720,908,4217,2656,3163};
        for (std::size_t i = 0; i < plate_.size(); ++i)
            if (!plate_[i].prepare(times[i] * scale_, 16 * rate / 44100.0f)) return false;
        constexpr float roomTimes[] = {.0199f,.0277f,.0353f,.0437f};
        for (std::size_t i = 0; i < room_.size(); ++i)
            if (!room_[i].prepare(roomTimes[i] * rate, 4)) return false;
        for (auto& line : predelay_) if (!line.prepare(rate * .2f + 1)) return false;
        slew_ = 1 - std::exp(-1 / (rate * .005f));
        mixSlew_ = 1 - std::exp(-1 / (rate * .012f));
        dcCoefficient_ = 1 - std::exp(-6.28318530718f * 20.0f / rate);
        transitionStep_ = 1 / (rate * .020f);
        preFadeLength_ = std::max<std::size_t>(1, std::size_t(rate * .020f));
        mix_ = targetMix_ = 0; active_ = requested_ = 0;
        algorithmPhase_ = 1; algorithmTransition_ = 0;
        preCurrent_ = preFrom_ = preTo_ = prePending_ = 0; preRemaining_ = 0;
        damping_ = .5f; diffusion_ = .7f; modulation_ = .1f; width_ = 1;
        feedback_ = .65f; roomGains_.fill(.85f); sin_ = 0; cos_ = 1;
        resetNetwork(); return true;
    }
    void reset() {
        resetNetwork();mix_=targetMix_=0;active_=requested_=0;
        algorithmPhase_=1;algorithmTransition_=0;preCurrent_=preFrom_=preTo_=prePending_=0;
        preRemaining_=0;sin_=0;cos_=1;sleeping_=true;
    }
    void beginBlock(int algorithm, float seconds, float preMs, float dampHz,
                    float diffusion, float modulation, float width, float mix) {
        requested_ = std::clamp(algorithm, 0, 1);
        seconds = std::clamp(seconds, .2f, 20.0f);
        targetMix_ = std::clamp(mix, 0.0f, 1.0f);
        targetDamping_ = 1 - std::exp(-6.28318530718f * std::clamp(dampHz, 500.0f,
            std::min(18000.0f, rate_ * .45f)) / rate_);
        targetDiffusion_ = std::clamp(diffusion, 0.0f, 1.0f);
        targetModulation_ = std::clamp(modulation, 0.0f, 1.0f);
        targetWidth_ = std::clamp(width, 0.0f, 1.0f);
        targetFeedback_ = std::pow(.001f, .1814f / seconds);
        for (std::size_t i = 0; i < room_.size(); ++i)
            targetRoomGains_[i] = std::pow(.001f, room_[i].length / rate_ / seconds);
        prePending_ = std::clamp(preMs, 0.0f, 200.0f) * rate_ * .001f;
        startPredelay();
        const float angle = 6.28318530718f * .31f / rate_;
        sinDelta_ = std::sin(angle); cosDelta_ = std::cos(angle);
        if (algorithmTransition_ == 0 && requested_ != active_) algorithmTransition_ = -1;
        // Oscillator normalization only once per block.
        const float norm = std::sqrt(sin_ * sin_ + cos_ * cos_);
        if (norm > 0) { sin_ /= norm; cos_ /= norm; }
    }
    std::array<float, 2> process(std::array<float, 2> dry, bool returnOnly = false) {
        if (targetMix_ == 0 && mix_ < 1.0e-6f) {
            if (!sleeping_) { resetNetwork(); sleeping_ = true; }
            mix_ = 0; active_ = requested_; algorithmTransition_ = 0; algorithmPhase_ = 1;
            return returnOnly ? std::array<float,2>{0,0} : dry;
        }
        sleeping_ = false;
        mix_ += (targetMix_ - mix_) * mixSlew_;
        damping_ += (targetDamping_ - damping_) * slew_;
        diffusion_ += (targetDiffusion_ - diffusion_) * slew_;
        modulation_ += (targetModulation_ - modulation_) * slew_;
        width_ += (targetWidth_ - width_) * slew_;
        feedback_ += (targetFeedback_ - feedback_) * slew_;
        for (std::size_t i = 0; i < room_.size(); ++i)
            roomGains_[i] += (targetRoomGains_[i] - roomGains_[i]) * slew_;
        if (algorithmTransition_ < 0) {
            algorithmPhase_ = std::max(0.0f, algorithmPhase_ - transitionStep_);
            if (algorithmPhase_ == 0) { active_ = requested_; resetNetwork(); algorithmTransition_ = 1; }
        } else if (algorithmTransition_ > 0) {
            algorithmPhase_ = std::min(1.0f, algorithmPhase_ + transitionStep_);
            if (algorithmPhase_ == 1) algorithmTransition_ = requested_ == active_ ? 0 : -1;
        }
        std::array<float, 2> input {};
        auto excitation = dry;
        for (std::size_t c = 0; c < 2; ++c) {
            dcLow_[c] += dcCoefficient_ * (dry[c] - dcLow_[c]);
            excitation[c] -= dcLow_[c];
        }
        float fade = preRemaining_ ? 1 - float(preRemaining_) / float(preFadeLength_) : 1;
        fade = fade * fade * (3 - 2 * fade);
        for (std::size_t c = 0; c < 2; ++c) {
            const auto read = [&](float delay) {
                // Continuous interpolation from the immediate input at <1 frame.
                return delay < 1 ? excitation[c] + (predelay_[c].tap(1) - excitation[c]) * delay
                                 : predelay_[c].read(delay);
            };
            const float a = read(preRemaining_ ? preFrom_ : preCurrent_);
            input[c] = preRemaining_ ? a + (read(preTo_) - a) * fade : a;
            predelay_[c].push(excitation[c]);
        }
        if (preRemaining_ && --preRemaining_ == 0) { preCurrent_ = preTo_; startPredelay(); }
        const auto wet = active_ == 0 ? plate(input) : room(input);
        const float mid = (wet[0] + wet[1]) * .5f;
        const float side = (wet[0] - wet[1]) * .5f * width_;
        const float amount = mix_ * algorithmPhase_ * algorithmPhase_ * (3 - 2 * algorithmPhase_);
        if (returnOnly) dry = {(mid+side)*amount,(mid-side)*amount};
        else {
            dry[0] += (mid + side - dry[0]) * amount;
            dry[1] += (mid - side - dry[1]) * amount;
        }
        const float next = cos_ * cosDelta_ - sin_ * sinDelta_;
        sin_ = sin_ * cosDelta_ + cos_ * sinDelta_; cos_ = next;
        return dry;
    }
private:
    void startPredelay() {
        if (preRemaining_ == 0 && std::abs(prePending_ - preCurrent_) >= .5f) {
            preFrom_ = preCurrent_; preTo_ = prePending_; preRemaining_ = preFadeLength_;
        }
    }
    void resetNetwork() {
        for (auto& line : plate_) line.reset();
        for (auto& line : room_) line.reset();
        for (auto& line : predelay_) line.reset();
        tank_.fill(0); low_.fill(0); bandwidth_.fill(0); dcLow_.fill(0);
    }
    std::array<float, 2> plate(const std::array<float, 2>& input) {
        // Side excitation prevents phase-opposed stereo input disappearing in
        // the plate's conventional mono input diffuser.
        const float mid = .5f * (input[0] + input[1]);
        const float side = .5f * (input[0] - input[1]);
        bandwidth_[0] += .75f * (mid - bandwidth_[0]);
        float diffused = bandwidth_[0];
        for (std::size_t i = 0; i < 4; ++i)
            diffused = plate_[i].allpass(diffused, (i < 2 ? .75f : .625f) * diffusion_);
        const float depth = modulation_ * 8 * rate_ / 44100.0f;
        float a = plate_[4].allpass(.5f * (diffused + side) + tank_[1] * feedback_,
            -.7f * diffusion_, depth * sin_);
        float b = plate_[8].allpass(.5f * (diffused - side) + tank_[0] * feedback_,
            -.7f * diffusion_, depth * cos_);
        a = plate_[5].delay(a); b = plate_[9].delay(b);
        low_[0] += damping_ * (a - low_[0]); low_[1] += damping_ * (b - low_[1]);
        a = plate_[6].allpass(low_[0] * feedback_, .5f * diffusion_);
        b = plate_[10].allpass(low_[1] * feedback_, .5f * diffusion_);
        tank_[0] = plate_[7].delay(a); tank_[1] = plate_[11].delay(b);
        const auto tap = [&](std::size_t line, float offset) {
            return plate_[line].tap(std::max<std::size_t>(1, std::size_t(offset * scale_)));
        };
        return { .6f * (tap(9,266)+tap(9,2974)-tap(10,1913)+tap(11,1996)
                          -tap(5,1990)-tap(6,187)-tap(7,1066)),
                 .6f * (tap(5,353)+tap(5,3627)-tap(6,1228)+tap(7,2673)
                          -tap(9,2111)-tap(10,335)-tap(11,121)) };
    }
    std::array<float, 2> room(const std::array<float, 2>& input) {
        std::array<float, 4> y {};
        for (std::size_t i = 0; i < 4; ++i) {
            const float delayed = room_[i].read(room_[i].length
                + modulation_ * .7f * (i & 1 ? sin_ : cos_));
            low_[i] += damping_ * (delayed - low_[i]); y[i] = low_[i] * roomGains_[i];
        }
        // Orthogonal normalized Hadamard feedback: stable with all gains <1.
        const std::array<float, 4> feedback {
            .5f*(y[0]+y[1]+y[2]+y[3]), .5f*(y[0]-y[1]+y[2]-y[3]),
            .5f*(y[0]+y[1]-y[2]-y[3]), .5f*(y[0]-y[1]-y[2]+y[3]) };
        float l = plate_[0].allpass(input[0] * .5f, .7f * diffusion_);
        float r = plate_[1].allpass(input[1] * .5f, .7f * diffusion_);
        l = plate_[2].allpass(l, .5f * diffusion_);
        r = plate_[3].allpass(r, .5f * diffusion_);
        room_[0].push(feedback[0]+l); room_[1].push(feedback[1]+r);
        room_[2].push(feedback[2]-l); room_[3].push(feedback[3]-r);
        return {.5f*(y[0]+y[1]-y[2]), .5f*(y[0]-y[1]+y[3])};
    }
    std::array<Line, 12> plate_;
    std::array<Line, 4> room_;
    std::array<Line, 2> predelay_;
    std::array<float, 2> tank_ {}, bandwidth_ {}, dcLow_ {};
    float dcCoefficient_ = 0;
    std::array<float, 4> low_ {}, roomGains_ {}, targetRoomGains_ {};
    float rate_ = 44100, scale_ = 1, slew_ = 1, mixSlew_ = 1, transitionStep_ = 1;
    float mix_ = 0, targetMix_ = 0, damping_ = .5f, targetDamping_ = .5f;
    float diffusion_ = .7f, targetDiffusion_ = .7f, modulation_ = .1f, targetModulation_ = .1f;
    float width_ = 1, targetWidth_ = 1, feedback_ = .7f, targetFeedback_ = .7f;
    float sin_ = 0, cos_ = 1, sinDelta_ = 0, cosDelta_ = 1, algorithmPhase_ = 1;
    float preCurrent_ = 0, preFrom_ = 0, preTo_ = 0, prePending_ = 0;
    std::size_t preRemaining_ = 0, preFadeLength_ = 1;
    int active_ = 0, requested_ = 0, algorithmTransition_ = 0;
    bool sleeping_ = false;
};
} // namespace s3g_daisy
