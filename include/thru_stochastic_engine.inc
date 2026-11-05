// Included in SampleMotionApp. Fixed storage and at most two PCM readers per
// lane. Prepared crossing maps are only binary-searched on a head's birth.
struct StochasticHead {
    ThruStochasticWalk::Shape shape{};
    std::array<double, kStackLanes> start{}, span{};
    std::array<ThruStochasticFolder, 2> folder{};
};
ThruStochasticWalk stochasticWalk_{};
ThruStochasticListener stochasticListener_{};
double stochasticCycleEvolution_ = 0;
std::array<StochasticHead, 2> stochasticHeads_{};
double stochasticPhase_ = 0, stochasticEvolution_ = 0;
float stochasticIncrement_ = 0, stochasticTargetIncrement_ = 0, stochasticSlew_ = .001f;
float stochasticAmut_ = 0, stochasticTmut_ = 0, stochasticDrift_ = 0;
bool stochasticInitialized_ = false;
std::uint32_t stochasticSerial_ = 0;
std::array<float, 2> stochasticDcInput_{}, stochasticDcOutput_{};
std::array<float, kStackLanes> stochasticPreparedMix_{};
std::array<const ThruWavesetBank *, kStackLanes> stochasticSeenBanks_{};
float stochasticDcPole_ = .999f, stochasticRate_ = 0;
void resetStochastic() {
    stochasticHeads_ = {};
    stochasticListener_ = {};
    stochasticCycleEvolution_ = 0;
    stochasticWalk_.reset(1);
    stochasticPhase_ = stochasticEvolution_ = 0;
    stochasticIncrement_ = stochasticTargetIncrement_ = 0;
    stochasticInitialized_ = false;
    stochasticAmut_ = stochasticTmut_ = stochasticDrift_ = 0;
    stochasticPreparedMix_ = {};
    stochasticSeenBanks_ = {};
    stochasticDcInput_ = {};
    stochasticDcOutput_ = {};
    ++stochasticSerial_;
}
void prepareStochasticControls(const Controls &c, std::size_t frames) {
    if (currentAlgorithm_ == PlaybackAlgorithm::stochastic ||
        c.algorithm == PlaybackAlgorithm::stochastic)
        stochasticListener_.update(c.stochasticLaw >= 2.5f, c.stochasticCapture,
                                   c.stochasticListenTarget, frames, outputSampleRate_);
    if ((currentAlgorithm_ == PlaybackAlgorithm::stochastic ||
         c.algorithm == PlaybackAlgorithm::stochastic) &&
        stochasticRate_ != outputSampleRate_) {
        stochasticRate_ = outputSampleRate_;
        stochasticSlew_ = 1 - std::exp(-1 / (outputSampleRate_ * .025f));
        stochasticDcPole_ = std::exp(-6.2831853f * 8 / outputSampleRate_);
    }
}
void evolveStochastic(const Controls &c) {
    const bool listening = c.stochasticLaw >= 2.5f;
    stochasticWalk_.evolve(
        c.stochasticMemory, listening ? 0.f : c.stochasticPull,
        listening ? 2 : static_cast<int>(clamp(std::round(c.stochasticLaw), 0.f, 2.f)),
        stochasticListener_.mutation, stochasticListener_.retention);
}
void launchStochasticHead(std::size_t index, const Controls &c, const Window &window) {
    if (stochasticInitialized_) {
        const bool cycle = c.stochasticEvolution >= 1024.f;
        int steps = 0;
        if (cycle && index == 0) {
            stochasticCycleEvolution_ += stochasticListener_.evolution;
            steps = static_cast<int>(stochasticCycleEvolution_);
            stochasticCycleEvolution_ -= steps;
        } else if (!cycle) {
            stochasticCycleEvolution_ = 0;
            steps = std::min(4, static_cast<int>(stochasticEvolution_));
            stochasticEvolution_ -= steps;
        }
        for (int step = 0; step < steps; ++step)
            evolveStochastic(c);
    }
    auto &head = stochasticHeads_[index];
    head = {};
    head.shape = stochasticWalk_.shape(stochasticAmut_, stochasticTmut_, stochasticDrift_);
    double duration = 0, weights = 0;
    const auto groups =
        static_cast<std::size_t>(clamp(std::round(c.stochasticSize), 1.f, 64.f));
    for (std::size_t lane = 0; lane < (stackMode_ ? kStackLanes : 1u); ++lane) {
        const auto count = stackMode_ ? laneFrames_[lane] : sourceFrames_;
        if (count < 2)
            continue;
        const double scale = stackMode_ ? lanePositionScales_[lane] : 1;
        const double low = clamp(window.low * scale, 0.0, static_cast<double>(count - 1));
        const double high = clamp(window.high * scale, low, static_cast<double>(count - 1));
        if (high - low < 1)
            continue;
        const double cursor = low + phase_ * (high - low);
        const auto *bank = wavesetBanks_[lane];
        double start = cursor,
               span = std::min(high - low,
                               static_cast<double>(laneSampleRate(lane)) * .005 * groups);
        if (bank && bank->frames == count) {
            const auto &map = bank->crossings[2];
            const auto first =
                std::lower_bound(map.begin(), map.end(), low,
                                 [](const auto &x, double p) { return x.position < p; });
            const auto end = std::upper_bound(
                first, map.end(), high, [](double p, const auto &x) { return p < x.position; });
            if (end - first > static_cast<std::ptrdiff_t>(groups)) {
                auto at = std::lower_bound(
                    first, end, cursor, [](const auto &x, double p) { return x.position < p; });
                at = std::min(at, end - static_cast<std::ptrdiff_t>(groups) - 1);
                start = at->position;
                span = (at + groups)->position - start;
            }
        }
        // A microscopic/high-frequency group is expanded, never repeatedly
        // searched. Long unpitched gaps are also bounded for responsive edits.
        const double rate = stackMode_ ? laneSampleRate(lane) : sourceSampleRate_;
        span =
            clamp(span, std::min(high - low, rate * .00075), std::min(high - low, rate * .35));
        const double previousSpan = stochasticHeads_[1 - index].span[lane];
        if (stochasticInitialized_ && previousSpan > 0)
            span = std::min(high - low, clamp(span, previousSpan * .8, previousSpan * 1.25));
        start = clamp(start, low, std::max(low, high - span));
        head.start[lane] = start;
        head.span[lane] = span;
        const float weight = stackMode_ ? laneWeights_[lane] : 1.f;
        duration += span / rate * weight;
        weights += weight;
    }
    if (weights > 1e-6)
        duration /= weights;
    else
        duration = .02;
    const double pitch = clamp(c.notePitch, .001f, 512.f);
    // DRFT=0 leaves the captured period fixed at its source/keyboard duration.
    stochasticTargetIncrement_ = static_cast<float>(
        clamp(pitch / (std::max(.00025, duration) * outputSampleRate_ * head.shape.drift),
              1.0 / (outputSampleRate_ * 2), 1.0 / 32));
    if (stochasticInitialized_)
        stochasticTargetIncrement_ =
            clamp(stochasticTargetIncrement_, stochasticIncrement_ * .8f,
                  stochasticIncrement_ * 1.25f);
}
void renderStochasticFrame(const Controls &c, float *wet) {
    const auto window = smoothEventWindow(c);
    fieldLow_ = window.low;
    fieldHigh_ = window.high;
    const double width = std::max(1.0, window.high - window.low);
    if (c.stochasticSourceHold >= .5f)
        phase_ = clamp((clamp01(c.stochasticHoldPosition) * sourceFrames_ - window.low) / width,
                       0.0, 1.0);
    if (!stochasticInitialized_) {
        stochasticWalk_.reset(
            static_cast<std::uint32_t>(clamp(c.cutVoiceIndex, 0.f, 127.f) + 1) * 2654435761u +
            stochasticSerial_ * 2246822519u + 1);
        // Give short notes a source-derived deformation immediately. EVOL=0
        // holds this per-note shape; zero mutation depths remain exact identity.
        for (int step = 0; step < 4; ++step)
            evolveStochastic(c);
        stochasticAmut_ = clamp01(c.stochasticAmplitude);
        stochasticTmut_ = clamp01(c.stochasticTiming);
        stochasticDrift_ = clamp01(c.stochasticDrift);
        launchStochasticHead(0, c, window);
        launchStochasticHead(1, c, window);
        stochasticIncrement_ = stochasticTargetIncrement_;
        stochasticInitialized_ = true;
    }
    stochasticAmut_ += stochasticSlew_ * (clamp01(c.stochasticAmplitude) - stochasticAmut_);
    stochasticTmut_ += stochasticSlew_ * (clamp01(c.stochasticTiming) - stochasticTmut_);
    stochasticDrift_ += stochasticSlew_ * (clamp01(c.stochasticDrift) - stochasticDrift_);
    if (c.stochasticEvolution <= 0 || c.stochasticEvolution >= 1024.f)
        stochasticEvolution_ = 0;
    else
        stochasticEvolution_ =
            std::min(4.999, stochasticEvolution_ + clamp(c.stochasticEvolution, 0.f, 1024.f) *
                                                       stochasticListener_.evolution /
                                                       outputSampleRate_);
    for (std::size_t lane = 0; lane < kStackLanes; ++lane) {
        const auto *bank = wavesetBanks_[lane];
        if (bank != stochasticSeenBanks_[lane]) {
            stochasticSeenBanks_[lane] = bank;
            stochasticPreparedMix_[lane] = 0;
        }
        const float target =
            bank && bank->pyramid.frames == (stackMode_ ? laneFrames_[lane] : sourceFrames_) &&
                    !bank->pyramid.data[0].pcm[0].empty()
                ? 1.f
                : 0.f;
        stochasticPreparedMix_[lane] +=
            stochasticSlew_ * (target - stochasticPreparedMix_[lane]);
        if (std::abs(target - stochasticPreparedMix_[lane]) < 1e-5f)
            stochasticPreparedMix_[lane] = target;
    }
    stochasticIncrement_ +=
        stochasticSlew_ * (stochasticTargetIncrement_ - stochasticIncrement_);
    for (std::size_t h = 0; h < 2; ++h) {
        auto &head = stochasticHeads_[h];
        const float phase = static_cast<float>(wrapPhase(stochasticPhase_ + (h ? .5 : 0)));
        // Complementary C1 windows sum to one, with exact zero at replacement.
        const float edge = phase < .5f ? phase * 2 : 2 - phase * 2;
        const float envelope = smoothStep(edge);
        const auto point = head.shape.at(phase);
        float headAudio[2]{};
        for (std::size_t lane = 0; lane < (stackMode_ ? kStackLanes : 1u); ++lane) {
            const float weight =
                (stackMode_ ? laneWeights_[lane] * activeControlsLaneGains_[lane] : 1.f);
            if (weight <= 1e-7f || head.span[lane] < 1)
                continue;
            const auto count = stackMode_ ? laneFrames_[lane] : sourceFrames_;
            const double position = head.start[lane] + point.position * head.span[lane];
            const double increment = head.span[lane] * stochasticIncrement_ * point.slope;
            const auto *bank = wavesetBanks_[lane];
            const auto *pyramid =
                bank && bank->pyramid.frames == count && !bank->pyramid.data[0].pcm[0].empty()
                    ? &bank->pyramid
                    : nullptr;
            // A continuous octave blend avoids a filter switch at powers of
            // two. Extra guard at deep mutation suppresses fast warp sidebands.
            const float rate = std::max(1.f, static_cast<float>(increment));
            std::uint32_t bits = 0;
            std::memcpy(&bits, &rate, sizeof(bits));
            const float lod =
                static_cast<float>(static_cast<int>((bits >> 23) & 255u) - 127) +
                (bits & 0x7fffffu) / 8388608.f +
                head.shape.filterGuard * smoothStep(static_cast<float>((increment - 1) * 2));
            for (std::size_t ch = 0; ch < 2; ++ch) {
                const auto *data =
                    stackMode_ ? laneData(lane, ch) : storage_[mapSourceChannel(ch)];
                float sample = 0;
                if (pyramid)
                    sample = ThruSamplePyramid::read(
                        pyramid->prepare(data, count, ch, position, lod));
                if (!pyramid || stochasticPreparedMix_[lane] < 1) {
                    const float direct = ThruSamplePyramid::linear(data, count, position);
                    float fallback = direct;
                    const float aa = clamp(static_cast<float>(increment - 1), 0.f, 1.f);
                    if (aa > 0) {
                        float average = 0;
                        const double spread = std::min(256.0, increment) * .5;
                        for (int tap = 0; tap < 8; ++tap)
                            average += ThruSamplePyramid::linear(
                                data, count,
                                clamp(position + spread * ((tap + .5) / 4 - 1),
                                      head.start[lane], head.start[lane] + head.span[lane]));
                        fallback += (average * .125f - direct) * aa;
                    }
                    sample = pyramid
                                 ? fallback + (sample - fallback) * stochasticPreparedMix_[lane]
                                 : fallback;
                }
                headAudio[ch] += sample * weight;
            }
        }
        // Deform the mixed lane source once per head. This also lets lanes
        // interact inside the fold while keeping overlap changes outside it.
        for (std::size_t ch = 0; ch < 2; ++ch)
            wet[ch] += head.folder[ch].process(headAudio[ch] * point.gain, point.drive,
                                               head.shape.foldMix) *
                       envelope * .8f;
    }
    for (std::size_t ch = 0; ch < 2; ++ch) {
        const float raw = wet[ch];
        wet[ch] = raw - stochasticDcInput_[ch] + stochasticDcPole_ * stochasticDcOutput_[ch];
        stochasticDcInput_[ch] = raw;
        stochasticDcOutput_[ch] = wet[ch];
        wet[ch + 2] = wet[ch];
    }
    displayPosition_ = window.low + phase_ * width;
    if (c.stochasticSourceHold < .5f)
        phase_ = wrapPhase(phase_ + static_cast<double>(speed_) * sourceSampleRate_ /
                                        outputSampleRate_ / width);
    const double before = stochasticPhase_;
    stochasticPhase_ += stochasticIncrement_;
    if (before < .5 && stochasticPhase_ >= .5)
        launchStochasticHead(1, c, window);
    if (stochasticPhase_ >= 1) {
        stochasticPhase_ -= 1;
        launchStochasticHead(0, c, window);
    }
}
