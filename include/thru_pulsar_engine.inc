// Included in SampleMotionApp. Four fixed pulse readers per note; every reader
// captures its window, PCM span and lane blend. Active readers are never stolen.
struct PulsarHead {
    std::array<double, kStackLanes> start{}, span{};
    std::array<std::size_t, kStackLanes> frames{};
    std::array<float, kStackLanes> weight{}, increment{}, lod{};
    double age = 0, duration = 1;
    float skew = .5f;
    int shape = 0;
    bool active = false, reverse = false;
};
std::array<PulsarHead, ThruPulsar::readers> pulsarHeads_{};
std::array<float, kStackLanes> pulsarPreparedMix_{};
std::array<const ThruWavesetBank *, kStackLanes> pulsarSeenBanks_{};
std::array<float, 2> pulsarDcIn_{}, pulsarDcOut_{};
double pulsarCountdown_ = 0, pulsarEmitModPhase_ = 0, pulsarFormModPhase_ = 0;
float pulsarEmitModRate_ = .13f, pulsarFormModRate_ = 1.7f;
float pulsarEmitModDepth_ = 0, pulsarFormModDepth_ = 0, pulsarSpray_ = 0,
      pulsarFormVariation_ = 0, pulsarReverse_ = 0, pulsarKeytrack_ = 0;
std::uint64_t pulsarEvent_ = 0;
std::uint32_t pulsarRandom_ = 1, pulsarSerial_ = 0;
float pulsarRate_ = 0, pulsarSlew_ = .001f, pulsarDcPole_ = .999f;
float pulsarBaseFrequency_ = 261.625565f;
float pulsarFrequency_ = 261.625565f, pulsarForm_ = 500, pulsarSize_ = 4, pulsarGain_ = 1;
float pulsarFragmentStart_ = 0, pulsarFragmentEnd_ = 0;
bool pulsarInitialized_ = false, pulsarLimited_ = false;
void resetPulsar() {
    pulsarHeads_ = {};
    pulsarPreparedMix_ = {};
    pulsarSeenBanks_ = {};
    pulsarDcIn_ = {};
    pulsarDcOut_ = {};
    pulsarCountdown_ = 0;
    pulsarEmitModPhase_ = pulsarFormModPhase_ = 0;
    pulsarEmitModDepth_ = pulsarFormModDepth_ = pulsarSpray_ = pulsarFormVariation_ =
        pulsarReverse_ = pulsarKeytrack_ = 0;
    pulsarEvent_ = 0;
    pulsarInitialized_ = false;
    pulsarLimited_ = false;
    pulsarGain_ = 1;
    pulsarFragmentStart_ = pulsarFragmentEnd_ = 0;
    ++pulsarSerial_;
}
void preparePulsarControls(const Controls &c) {
    if ((currentAlgorithm_ == PlaybackAlgorithm::pulsar ||
         c.algorithm == PlaybackAlgorithm::pulsar) &&
        pulsarRate_ != outputSampleRate_) {
        pulsarRate_ = outputSampleRate_;
        pulsarSlew_ = 1 - std::exp(-1 / (outputSampleRate_ * .025f));
        pulsarDcPole_ = std::exp(-6.28318530718f * 8 / outputSampleRate_);
    }
}
void launchPulsar(const Controls &c, const Window &window, double age, float minimumInterval) {
    // Three minimum intervals leave a spare reader for edits and fractional
    // scheduling. A live acceleration can temporarily exhaust the pool; omit
    // that emission rather than interrupting an audible reader.
    float semitones = pulsarFormModDepth_ > 0
                          ? pulsarFormModDepth_ * ThruPulsar::modulation(pulsarFormModPhase_)
                          : 0;
    if (pulsarFormVariation_ > 0)
        semitones += pulsarFormVariation_ * (2 * ThruPulsar::random(pulsarRandom_) - 1);
    if (pulsarKeytrack_ > 0)
        semitones += 12 * std::log2(clamp(c.notePitch, .001f, 512.f)) * pulsarKeytrack_;
    const float form = semitones == 0
                           ? pulsarForm_
                           : clamp(pulsarForm_ * std::exp2(semitones / 12), 20.f, 2000.f);
    const double requested = outputSampleRate_ / form;
    const double duration = clamp(requested, 16., std::max(16., 2.9 * minimumInterval));
    pulsarLimited_ = std::abs(duration - requested) > .01;
    const auto event = pulsarEvent_++;
    const float probability = ThruPulsar::random(pulsarRandom_);
    if (!ThruPulsar::burstAllows(event, static_cast<int>(std::round(c.pulsarBurst))) ||
        probability < clamp01(c.pulsarOmit))
        return;
    PulsarHead *head = nullptr;
    for (auto &h : pulsarHeads_)
        if (!h.active) {
            head = &h;
            break;
        }
    if (!head) {
        pulsarLimited_ = true;
        return;
    }
    *head = {};
    head->active = true;
    head->duration = duration;
    head->age = age;
    head->shape = static_cast<int>(clamp(std::round(c.pulsarWindow), 0.f, 4.f));
    head->skew = clamp01(c.pulsarSkew);
    // A zero depth consumes no random numbers, preserving the original jitter
    // and omission sequence in programs saved before the modulation page.
    head->reverse = pulsarReverse_ > 0 && ThruPulsar::random(pulsarRandom_) < pulsarReverse_;
    const double sourcePhase =
        pulsarSpray_ > 0
            ? clamp(phase_ + .5 * pulsarSpray_ * (2 * ThruPulsar::random(pulsarRandom_) - 1),
                    0., 1.)
            : phase_;
    float total = 0;
    for (std::size_t lane = 0; lane < (stackMode_ ? kStackLanes : 1u); ++lane) {
        const auto count = stackMode_ ? laneFrames_[lane] : sourceFrames_;
        if (count < 2)
            continue;
        const double scale = stackMode_ ? lanePositionScales_[lane] : 1;
        const double low = clamp(window.low * scale, 0., static_cast<double>(count - 1));
        const double high = clamp(window.high * scale, low, static_cast<double>(count - 1));
        if (high - low < 1)
            continue;
        const double span =
            std::min(high - low, static_cast<double>(stackMode_ ? laneSampleRate(lane)
                                                                : sourceSampleRate_) *
                                     pulsarSize_ * .001);
        const double start =
            clamp(low + sourcePhase * (high - low), low, std::max(low, high - span));
        head->start[lane] = start;
        head->span[lane] = span;
        head->frames[lane] = count;
        const float increment = static_cast<float>(span / duration);
        head->increment[lane] = increment;
        head->lod[lane] = increment > 1 ? std::log2(increment) : 0;
        head->weight[lane] =
            stackMode_ ? laneWeights_[lane] * activeControlsLaneGains_[lane] : 1;
        total += head->weight[lane];
    }
    // Preserve deliberate lane gain settings while bounding an accidental sum
    // of stack weights. Existing output/part gain and dynamics stay downstream.
    if (total > 4)
        for (auto &w : head->weight)
            w *= 4 / total;
    const double span =
        std::min(window.high - window.low, sourceSampleRate_ * pulsarSize_ * .001);
    pulsarFragmentStart_ =
        static_cast<float>(clamp(window.low + sourcePhase * (window.high - window.low),
                                 window.low, std::max(window.low, window.high - span)) /
                           std::max<std::size_t>(1, sourceFrames_));
    pulsarFragmentEnd_ = static_cast<float>(
        std::min(window.high,
                 static_cast<double>(pulsarFragmentStart_) * sourceFrames_ + span) /
        std::max<std::size_t>(1, sourceFrames_));
}
void renderPulsarFrame(const Controls &c, float *wet) {
    const auto window = smoothEventWindow(c);
    fieldLow_ = window.low;
    fieldHigh_ = window.high;
    const double width = std::max(1., window.high - window.low);
    if (c.pulsarSourceHold >= .5f)
        phase_ =
            clamp((clamp01(c.pulsarHoldPosition) * sourceFrames_ - window.low) / width, 0., 1.);
    if (!pulsarInitialized_) {
        pulsarRandom_ =
            (static_cast<std::uint32_t>(clamp(c.cutVoiceIndex, 0.f, 127.f)) + 1) * 2654435761u +
            pulsarSerial_ * 2246822519u + 1;
        if (!pulsarRandom_)
            pulsarRandom_ = 1;
        pulsarFrequency_ = clamp(
            clamp(c.pulsarRate, .1f, 2000.f) * clamp(c.notePitch, .001f, 512.f), .05f, 2000.f);
        pulsarBaseFrequency_ = pulsarFrequency_;
        pulsarEmitModRate_ = clamp(c.pulsarEmitModRate, .01f, 20.f);
        pulsarFormModRate_ = clamp(c.pulsarFormModRate, .01f, 20.f);
        pulsarEmitModDepth_ = clamp(c.pulsarEmitModDepth, 0.f, 24.f);
        pulsarFormModDepth_ = clamp(c.pulsarFormModDepth, 0.f, 24.f);
        pulsarFormVariation_ = clamp(c.pulsarFormVariation, 0.f, 24.f);
        pulsarSpray_ = clamp01(c.pulsarSpray);
        pulsarReverse_ = clamp01(c.pulsarReverse);
        pulsarKeytrack_ = clamp01(c.pulsarKeytrack);
        pulsarForm_ = clamp(c.pulsarForm, 20.f, 2000.f);
        pulsarSize_ = clamp(c.pulsarSize, ThruPulsar::minimumSizeMs, ThruPulsar::maximumSizeMs);
        pulsarInitialized_ = true;
    }
    const auto smooth = [&](float &value, float target) {
        if (value == target)
            return;
        value += pulsarSlew_ * (target - value);
        if (std::abs(target - value) < .00005f * std::max(1.f, std::abs(target)))
            value = target;
    };
    smooth(pulsarEmitModRate_, clamp(c.pulsarEmitModRate, .01f, 20.f));
    smooth(pulsarFormModRate_, clamp(c.pulsarFormModRate, .01f, 20.f));
    smooth(pulsarEmitModDepth_, clamp(c.pulsarEmitModDepth, 0.f, 24.f));
    smooth(pulsarFormModDepth_, clamp(c.pulsarFormModDepth, 0.f, 24.f));
    smooth(pulsarFormVariation_, clamp(c.pulsarFormVariation, 0.f, 24.f));
    smooth(pulsarSpray_, clamp01(c.pulsarSpray));
    smooth(pulsarReverse_, clamp01(c.pulsarReverse));
    smooth(pulsarKeytrack_, clamp01(c.pulsarKeytrack));
    const float modulation =
        pulsarEmitModDepth_ > 0
            ? ThruPulsar::emissionRatio(pulsarEmitModDepth_ *
                                        ThruPulsar::modulation(pulsarEmitModPhase_))
            : 1;
    const float baseTarget = clamp(
        clamp(c.pulsarRate, .1f, 2000.f) * clamp(c.notePitch, .001f, 512.f), .05f, 2000.f);
    pulsarBaseFrequency_ += pulsarSlew_ * (baseTarget - pulsarBaseFrequency_);
    const float previousFrequency = pulsarFrequency_;
    // Glide knob edits without filtering the intentional vibrato a second time.
    pulsarFrequency_ = clamp(pulsarBaseFrequency_ * modulation, .05f, 2000.f);
    // Preserve clock phase through a rate edit, including long silent periods.
    pulsarCountdown_ *= previousFrequency / pulsarFrequency_;
    pulsarForm_ += pulsarSlew_ * (clamp(c.pulsarForm, 20.f, 2000.f) - pulsarForm_);
    pulsarSize_ += pulsarSlew_ * (clamp(c.pulsarSize, ThruPulsar::minimumSizeMs, ThruPulsar::maximumSizeMs) - pulsarSize_);
    const float interval = outputSampleRate_ / pulsarFrequency_,
                jitter = clamp01(c.pulsarJitter) * .45f;
    if (pulsarCountdown_ <= 0) {
        launchPulsar(c, window, -pulsarCountdown_, interval * (1 - jitter));
        pulsarCountdown_ +=
            interval * (1 + jitter * (2 * ThruPulsar::random(pulsarRandom_) - 1));
    }
    pulsarCountdown_ -= 1;
    for (std::size_t lane = 0; lane < kStackLanes; ++lane) {
        const auto *bank = wavesetBanks_[lane];
        if (bank != pulsarSeenBanks_[lane]) {
            pulsarSeenBanks_[lane] = bank;
            pulsarPreparedMix_[lane] = 0;
        }
        const bool ready =
            bank && bank->pyramid.frames == (stackMode_ ? laneFrames_[lane] : sourceFrames_) &&
            !bank->pyramid.data[0].pcm[0].empty();
        pulsarPreparedMix_[lane] +=
            pulsarSlew_ * ((ready ? 1.f : 0.f) - pulsarPreparedMix_[lane]);
        if (pulsarPreparedMix_[lane] < 1e-6f && !ready)
            pulsarPreparedMix_[lane] = 0;
        if (pulsarPreparedMix_[lane] > .99999f && ready)
            pulsarPreparedMix_[lane] = 1;
    }
    float envelopeSum = 0;
    for (auto &head : pulsarHeads_) {
        if (!head.active)
            continue;
        if (head.age >= head.duration) {
            head.active = false;
            continue;
        }
        const float phase = static_cast<float>(head.age / head.duration);
        const float envelope = ThruPulsar::window(phase, head.shape, head.skew);
        envelopeSum += envelope;
        for (std::size_t lane = 0; lane < (stackMode_ ? kStackLanes : 1u); ++lane) {
            const auto count = stackMode_ ? laneFrames_[lane] : sourceFrames_;
            if (head.weight[lane] <= 1e-7f || head.frames[lane] != count)
                continue;
            const double position =
                head.start[lane] + (head.reverse ? 1 - phase : phase) * head.span[lane];
            const float increment = head.increment[lane], lod = head.lod[lane];
            const auto *bank = wavesetBanks_[lane];
            const auto *pyramid =
                bank && bank->pyramid.frames == count && !bank->pyramid.data[0].pcm[0].empty()
                    ? &bank->pyramid
                    : nullptr;
            for (std::size_t ch = 0; ch < 2; ++ch) {
                const auto *pcm =
                    stackMode_ ? laneData(lane, ch) : storage_[mapSourceChannel(ch)];
                // The bounded fallback keeps output available before analysis.
                const float direct = (!pyramid || pulsarPreparedMix_[lane] < 1)
                                         ? ThruSamplePyramid::linear(pcm, count, position)
                                         : 0;
                float fallback = direct;
                if ((!pyramid || pulsarPreparedMix_[lane] < 1) && increment > 1.05f) {
                    float average = 0;
                    const double spread = std::min(64.f, increment);
                    for (int tap = 0; tap < 4; ++tap)
                        average += ThruSamplePyramid::linear(
                            pcm, count,
                            clamp(position + spread * ((tap + .5) / 2 - 1), head.start[lane],
                                  head.start[lane] + head.span[lane]));
                    fallback +=
                        (average * .25f - direct) * clamp((increment - 1) * 2, 0.f, 1.f);
                }
                const float sample =
                    pyramid ? fallback + (ThruSamplePyramid::read(
                                              pyramid->prepare(pcm, count, ch, position, lod)) -
                                          fallback) *
                                             pulsarPreparedMix_[lane]
                            : fallback;
                wet[ch] += sample * head.weight[lane] * envelope;
            }
        }
        head.age += 1;
    }
    const float gain = 1 / std::max(1.f, envelopeSum);
    pulsarGain_ += pulsarSlew_ * (gain - pulsarGain_);
    for (std::size_t ch = 0; ch < 2; ++ch) {
        const float raw = wet[ch] * pulsarGain_ * .8f;
        wet[ch] = raw - pulsarDcIn_[ch] + pulsarDcPole_ * pulsarDcOut_[ch];
        pulsarDcIn_[ch] = raw;
        pulsarDcOut_[ch] = wet[ch];
        wet[ch + 2] = wet[ch];
    }
    pulsarEmitModPhase_ =
        wrapPhase(pulsarEmitModPhase_ + pulsarEmitModRate_ / outputSampleRate_);
    pulsarFormModPhase_ =
        wrapPhase(pulsarFormModPhase_ + pulsarFormModRate_ / outputSampleRate_);
    displayPosition_ = window.low + phase_ * width;
    if (c.pulsarSourceHold < .5f)
        phase_ = wrapPhase(phase_ + static_cast<double>(speed_) * sourceSampleRate_ /
                                        outputSampleRate_ / width);
}
