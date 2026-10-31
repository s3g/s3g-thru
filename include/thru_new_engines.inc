std::array<s3g_daisy::ThruFormantVoice, kStackLanes> formantVoices_{};
s3g_daisy::ThruFormantVoice::Articulation formantArticulation_;
float formantPitchRatio_ = 1, formantShiftRatio_ = 1, formantSlew_ = .001f;
void prepareFormantControls(const Controls &c) {
    if (currentAlgorithm_ != PlaybackAlgorithm::formant &&
        c.algorithm != PlaybackAlgorithm::formant)
        return;
    formantPitchRatio_ =
        clamp(c.notePitch, .001f, 512.f) * std::exp2(clamp(c.formantPitch, -24.f, 24.f) / 12);
    const float key =
        12 * std::log2(clamp(c.notePitch, .001f, 512.f)) + clamp(c.formantPitch, -24.f, 24.f);
    formantShiftRatio_ = std::exp2(
        clamp(c.formantShift + key * clamp(c.formantKeytrack, -1.f, 1.f), -24.f, 24.f) / 12);
    formantArticulation_.tune = clamp01(c.formantTune);
    formantArticulation_.frequency =
        clamp(c.formantNoteHz * std::exp2(clamp(c.formantPitch, -24.f, 24.f) / 12), 20.f, 6000.f);
    formantArticulation_.rough = clamp01(c.formantRough);
    formantArticulation_.sub = clamp01(c.formantSub);
    formantArticulation_.seed =
        static_cast<std::uint32_t>(clamp(c.cutVoiceIndex, 0.f, 127.f)) * 2654435761u + 1u;
    formantSlew_ =
        1 - std::exp(-1 / (outputSampleRate_ * clamp(c.formantSmoothingMs, 5.f, 200.f) * .001f));
}
void renderFormantFrame(const Controls &c, float *wet) {
    const auto window = smoothEventWindow(c);
    fieldLow_ = window.low;
    fieldHigh_ = window.high;
    const double width = std::max(1.0, window.high - window.low);
    if (c.formantFreeze >= .5f)
        phase_ = clamp((clamp01(c.formantFreezePosition) * sourceFrames_ - window.low) / width, 0.0,
                       1.0);
    displayPosition_ = window.low + phase_ * width;
    s3g_daisy::ThruFormantVoice::Stereo sum;
    for (std::size_t lane = 0; lane < kStackLanes; ++lane) {
        const auto count = laneFrames_[lane];
        const float weight = laneWeights_[lane] * activeControlsLaneGains_[lane];
        const double scale = lanePositionScales_[lane];
        const auto *bank = spectralBanks_[lane];
        const auto read = [&](double position, double increment) {
            s3g_daisy::ThruFormantVoice::Stereo sample;
            if (count < 2 || position < 0 || position > count - 1)
                return sample;
            const double spread = std::max(0.0, increment - 1) * .25;
            const auto channel = [&](std::size_t ch) {
                const auto *pcm = laneData(lane, ch);
                return spread < 1e-6 ? interpolatedSample(pcm, count, position)
                                     : .5f * (interpolatedSample(pcm, count, position - spread) +
                                              interpolatedSample(pcm, count, position + spread));
            };
            sample.left = channel(0);
            sample.right = channel(1);
            return sample;
        };
        auto articulation = formantArticulation_;
        articulation.seed ^= static_cast<std::uint32_t>(lane + 1) * 2246822519u;
        const auto sample = formantVoices_[lane].render(
            bank ? &bank->pitch : nullptr, displayPosition_ * scale, window.low * scale,
            window.high * scale, laneSampleRate(lane), outputSampleRate_, formantPitchRatio_,
            formantShiftRatio_, formantSlew_, clamp(c.formantSmoothingMs, 5.f, 200.f),
            1.5f + .4f * clamp01(c.join), articulation, weight > 1e-6f && count > 1, read);
        sum.left += sample.left * weight;
        sum.right += sample.right * weight;
    }
    for (std::size_t ch = 0; ch < kChannels; ++ch)
        wet[ch] = (ch & 1) ? sum.right : sum.left;
    if (c.formantFreeze < .5f)
        phase_ = wrapPhase(phase_ + static_cast<double>(speed_) * sourceSampleRate_ /
                                        outputSampleRate_ / width);
}
std::array<const s3g_daisy::ThruSpectralBank *, kStackLanes> spectralBanks_{};
s3g_daisy::ThruSpectralVoice spectralVoice_;
std::array<LongGrain, 4> textureEvents_{};
std::uint32_t textureRandom_ = 1;
int textureSeed_ = -1;
std::size_t textureStep_ = 0;
double textureCountdown_ = 0;
float morphSecondaryPosition_ = -1;
float imprintMix_ = 0;
bool imprintInitialized_ = false;
float textureRandom() {
    textureRandom_ ^= textureRandom_ << 13;
    textureRandom_ ^= textureRandom_ >> 17;
    textureRandom_ ^= textureRandom_ << 5;
    return (textureRandom_ & 0xffffffu) / 16777216.0f;
}
int textureInterval(int requested, std::uint16_t mask) const {
    int best = 0, distance = 999;
    for (int n = -48; n <= 48; ++n)
        if (mask & (1u << ((n % 12 + 12) % 12))) {
            const int d = std::abs(requested - n);
            if (d < distance) {
                best = n;
                distance = d;
            }
        }
    return best;
}
void renderTextureFrame(const Controls &controls, float *wet) {
    const Window window = smoothEventWindow(controls);
    fieldLow_ = window.low;
    fieldHigh_ = window.high;
    const double width = std::max(1.0, window.high - window.low);
    displayPosition_ = window.low + phase_ * width;
    const auto group =
        static_cast<std::size_t>(clamp(std::round(controls.textureGroup), 2.0f, 8.0f));
    const auto mode = static_cast<int>(clamp(std::round(controls.textureMode), 0.0f, 3.0f));
    if (textureCountdown_ <= 0) {
        if (textureStep_ % group == 0 && textureSeed_ != static_cast<int>(controls.textureSeed)) {
            textureSeed_ = static_cast<int>(controls.textureSeed);
            textureRandom_ = static_cast<std::uint32_t>(textureSeed_) * 2654435761u +
                             static_cast<std::uint32_t>(controls.cutVoiceIndex) * 2246822519u + 1u;
        }
        LongGrain *event = nullptr;
        for (auto &candidate : textureEvents_)
            if (!candidate.active) {
                event = &candidate;
                break;
            }
        if (event) {
            const auto step = textureStep_ % group;
            const float range = clamp(controls.textureRange, 0.0f, 24.0f);
            const int pattern[]{0, 2, 4, 1, 5, 3, 6, 2};
            const int raw =
                mode == 0 || mode == 1
                    ? static_cast<int>(std::lround((textureRandom() * 2 - 1) * range))
                    : mode == 2 ? static_cast<int>(std::lround((pattern[step] / 6.0f) * range))
                                : step == 0 ? 0
                                            : static_cast<int>(std::lround(
                                                  (step & 1 ? 1.0f : -1.0f) * range * .5f));
            const int interval = textureInterval(raw, controls.textureScaleMask);
            double origin = mode == 0 ? textureRandom() : phase_;
            if (mode == 2)
                origin = wrapPhase(phase_ + static_cast<double>(step) / group);
            else if (mode == 1)
                origin = wrapPhase(phase_ + (textureRandom() - .5) * controls.textureScatter * .2);
            event->lanes =
                makeNaturalEventLaneBlend(lanePhase_ + controls.lanePhaseOffset, controls);
            const auto lane = event->lanes.first;
            double low = window.low, high = window.high, position = low + origin * width;
            if (lane < kStackLanes && laneRegionCounts_[lane] > 1) {
                const auto count = laneRegionCounts_[lane];
                std::size_t region = 0;
                const float unit =
                    static_cast<float>(position / std::max<std::size_t>(1, sourceFrames_));
                while (region + 1 < count && laneRegionBoundaries_[lane][region + 1] <= unit)
                    ++region;
                const double a = laneRegionBoundaries_[lane][region] * sourceFrames_,
                             b = laneRegionBoundaries_[lane][region + 1] * sourceFrames_;
                low = std::max(low, a);
                high = std::min(high, b);
                if (high <= low + 1) {
                    low = window.low;
                    high = window.high;
                }
                position = low;
            }
            const float pitch =
                clamp(controls.notePitch, .001f, 512.0f) * std::pow(2.0f, interval / 12.0f);
            event->increment = sourceSampleRate_ / outputSampleRate_ * pitch;
            const float length =
                clamp(controls.textureLengthMs, 20.0f, 2000.0f) * (mode == 3 && step ? .4f : 1.0f);
            event->duration = std::max<std::size_t>(
                16, static_cast<std::size_t>(outputSampleRate_ * length * .001f));
            event->edgeFrames =
                std::min(event->duration / 2,
                         std::max<std::size_t>(
                             8, static_cast<std::size_t>(
                                    outputSampleRate_ * (.002f + .018f * clamp01(controls.join)))));
            event->position = position;
            event->low = low;
            event->high = high;
            event->seam =
                std::min((high - low) * .25,
                         std::max(4.0, sourceSampleRate_ * (.002 + .018 * clamp01(controls.join))));
            event->age = 0;
            event->active = true;
            event->shape = LongGrainShape::plateau;
            event->gain = mode == 3 && step ? .6f : .8f + .2f * textureRandom();
            const float pan = (textureRandom() * 2 - 1) * clamp01(controls.textureSpread);
            event->channelGains[0] = std::sqrt(1 - pan);
            event->channelGains[1] = std::sqrt(1 + pan);
        }
        const double spacing = outputSampleRate_ / clamp(controls.textureRate, .25f, 24.0f);
        textureCountdown_ +=
            spacing *
            std::max(.1f, 1 + (textureRandom() * 2 - 1) * clamp01(controls.textureScatter) * .9f);
        if (mode == 1 && (textureStep_ + 1) % group == 0)
            textureCountdown_ += spacing * group;
        if (mode == 3 && (textureStep_ + 1) % group != 0)
            textureCountdown_ = std::max(1.0, textureCountdown_ * .35);
        ++textureStep_;
    }
    --textureCountdown_;
    float stereo[2]{}, sum = 0;
    for (auto &event : textureEvents_)
        if (event.active) {
            const float gain = event.gain * longGrainEnvelope(event);
            sum += gain;
            const auto head = makeEventReadHead(event.position, event.increment, event.low,
                                                event.high, event.seam);
            for (std::size_t ch = 0; ch < 2; ++ch)
                stereo[ch] +=
                    readLaneBlendHead(ch, head, event.lanes) * gain * event.channelGains[ch];
            advanceEventPosition(event.position, event.increment, event.low, event.high,
                                 event.seam);
            if (++event.age >= event.duration)
                event.active = false;
        }
    const float normalize = 1 / std::max(1.0f, sum);
    for (std::size_t ch = 0; ch < kChannels; ++ch)
        wet[ch] = stereo[ch & 1u] * normalize;
    phase_ = wrapPhase(phase_ +
                       static_cast<double>(speed_) * sourceSampleRate_ / outputSampleRate_ / width);
}
void renderSpectralFrame(const Controls &controls, float *wet) {
    using Plan = s3g_daisy::ThruSpectralPlan;
    using Feature = s3g_daisy::ThruSpectralFeature;
    const auto window = smoothEventWindow(controls);
    fieldLow_ = window.low;
    fieldHigh_ = window.high;
    const double width = std::max(1.0, window.high - window.low),
                 total = std::max<std::size_t>(1, sourceFrames_);
    const bool morph = currentAlgorithm_ == PlaybackAlgorithm::morph;
    const int mode = static_cast<int>(std::round(controls.prismMode));
    const float moving = static_cast<float>((window.low + phase_ * width) / total);
    const float held = static_cast<float>((window.low + .5 * width) / total);
    const float position = !morph && mode == 1 ? held : moving;
    displayPosition_ = position * total;
    const float second =
        static_cast<float>((window.low + wrapPhase(phase_ + controls.morphOffset) * width) / total);
    morphSecondaryPosition_ = morph ? second : -1;
    if (spectralVoice_.needsFrame()) {
        s3g_daisy::ThruSpectralVoice::Spectrum spectrum{};
        const float imprintTarget = morph && controls.morphMode >= .5f ? 1.0f : 0.0f;
        if (!imprintInitialized_) {
            imprintMix_ = imprintTarget;
            imprintInitialized_ = true;
        } else
            imprintMix_ +=
                (imprintTarget - imprintMix_) *
                (1 - std::exp(-static_cast<float>(Plan::hop) / (outputSampleRate_ * .02f)));
        if (std::abs(imprintMix_ - imprintTarget) < 1e-5f)
            imprintMix_ = imprintTarget;
        s3g_daisy::ThruSpectralVoice::Spectrum carrier{}, shaper{};

        const float pitch =
            clamp(controls.notePitch, .001f, 512.0f) *
            std::pow(2.0f, (morph ? controls.morphPitch : controls.prismPitch) / 12.0f);
        const auto first = static_cast<std::size_t>(clamp(controls.stackFirstLane, 0.0f, 3.0f));
        const auto last = static_cast<std::size_t>(
            clamp(controls.stackLastLane, static_cast<float>(first), 3.0f));
        const float pitchGain=1/std::sqrt(std::max(.25f,pitch));
        std::array<s3g_daisy::ThruSpectralBank::ReadFrame,kStackLanes> primary{},secondary{},heldFrames{};
        std::array<float, kStackLanes> binScales{};
        for (std::size_t lane = 0; lane < kStackLanes; ++lane)
            if (spectralBanks_[lane]) {
                binScales[lane] = outputSampleRate_ / std::max(1.0f, spectralBanks_[lane]->rate);
                primary[lane]=spectralBanks_[lane]->prepareRead(position);
                if(morph)secondary[lane]=spectralBanks_[lane]->prepareRead(second);
                if(!morph&&(mode==2||mode==3))heldFrames[lane]=spectralBanks_[lane]->prepareRead(held);
            }
        const auto sample = [&](std::size_t lane, const auto& view, float bin) {
            Feature f;
            if (lane >= kStackLanes || !spectralBanks_[lane])
                return f;
            f = view.sample(bin * binScales[lane]);
            f.left *= activeControlsLaneGains_[lane];
            f.right *= activeControlsLaneGains_[lane];
            return f;
        };
        const auto combine = [](Feature &out, const Feature &in, float gain, float &totalGain) {
            if (gain <= 1e-5f)
                return;
            const float mix = gain / (totalGain + gain);
            out.left += in.left * gain;
            out.right += in.right * gain;
            out.hz += (in.hz - out.hz) * mix;
            out.phase += Plan::wrap(in.phase - out.phase) * mix;
            out.delta += Plan::wrap(in.delta - out.delta) * mix;
            totalGain += gain;
        };
        const auto stack = [&](const auto& views, float bin, bool next) {
            Feature out;
            float weight = 0;
            for (std::size_t lane = first; lane <= last; ++lane) {
                const std::size_t source =
                    next ? (first == last ? (lane + 1) % 4
                                          : first + (lane - first + 1) % (last - first + 1))
                         : lane;
                if (!spectralBanks_[source] || laneWeights_[lane]<=1e-5f)
                    continue;
                combine(out, sample(source, views[source], bin), laneWeights_[lane], weight);
            }
            if (weight > 0) {
                out.left /= weight;
                out.right /= weight;
            }
            return out;
        };
        bool partner = false;
        if (morph)
            for (std::size_t lane = first; lane <= last; ++lane) {
                const auto next = first == last ? (lane + 1) % 4
                                                : first + (lane - first + 1) % (last - first + 1);
                partner |= laneWeights_[lane] > 1e-5f && spectralBanks_[next] &&
                           !spectralBanks_[next]->frames.empty();
            }
        for (std::size_t k = 1; k + 1 < Plan::bins; ++k) {
            const float bin = k / std::max(.001f, pitch);
            Feature a = stack(primary, bin, false);
            if (imprintMix_ > 0) {
                carrier[k] = a;
                shaper[k] = partner ? stack(secondary, static_cast<float>(k), true) : a;
            }
            if (morph && imprintMix_ < 1) {
                const auto b = partner ? stack(secondary, bin, true) : a;
                const float amp = clamp01(controls.morphAmplitude),
                            freq = clamp01(controls.morphFrequency);
                a.left += (b.left - a.left) * amp;
                a.right += (b.right - a.right) * amp;
                a.hz += (b.hz - a.hz) * freq;
                a.delta += Plan::wrap(b.delta - a.delta) * amp;
            } else if (!morph) {
                if (mode == 2 || mode == 3) {
                    const auto frozen = stack(heldFrames, bin, false);
                    if (mode == 2) {
                        a.left = frozen.left;
                        a.right = frozen.right;
                    } else
                        a.hz = frozen.hz;
                }
            }
            a.hz *= pitch;
            a.left *= pitchGain;
            a.right *= pitchGain;
            spectrum[k] = a;
        }

        if (imprintMix_ > 0) {
            s3g_daisy::ThruSpectralImprint::apply(carrier, shaper,
                                                  partner ? clamp01(controls.morphAmplitude) : 0);
            for (std::size_t k = 1; k + 1 < Plan::bins; ++k) {
                carrier[k].hz *= pitch;
                carrier[k].left *= pitchGain;
                carrier[k].right *= pitchGain;
                spectrum[k].left += (carrier[k].left - spectrum[k].left) * imprintMix_;
                spectrum[k].right += (carrier[k].right - spectrum[k].right) * imprintMix_;
                spectrum[k].hz += (carrier[k].hz - spectrum[k].hz) * imprintMix_;
                spectrum[k].delta += Plan::wrap(carrier[k].delta - spectrum[k].delta) * imprintMix_;
            }
        }
        if (!morph && controls.prismSmear > .01f) {
            std::array<float, Plan::bins + 1> left{}, right{};
            for (std::size_t k = 0; k < Plan::bins; ++k) {
                left[k + 1] = left[k] + spectrum[k].left;
                right[k + 1] = right[k] + spectrum[k].right;
            }
            const int radius = std::max(1, static_cast<int>(clamp01(controls.prismSmear) * 4));
            const float blend = clamp01(controls.prismSmear);
            for (std::size_t k = 1; k + 1 < Plan::bins; ++k) {
                const auto lo = static_cast<std::size_t>(std::max(0, static_cast<int>(k) - radius)),
                           hi = std::min(Plan::bins, k + radius + 1);
                spectrum[k].left += ((left[hi] - left[lo]) / (hi - lo) - spectrum[k].left) * blend;
                spectrum[k].right +=
                    ((right[hi] - right[lo]) / (hi - lo) - spectrum[k].right) * blend;
            }
        }
        spectralVoice_.synthesize(spectrum, outputSampleRate_,
                                  std::max(8.0f + 24.0f * clamp01(controls.join),
                                           morph ? controls.morphBlurMs : controls.prismBlurMs),
                                  controls.prismDecayMs, !morph && mode == 4,
                                  morph ? 0 : controls.prismTilt);
    }
    const auto audio = spectralVoice_.next();
    for (std::size_t ch = 0; ch < kChannels; ++ch)
        wet[ch] = audio[ch & 1u];
    phase_ = wrapPhase(phase_ +
                       static_cast<double>(speed_) * sourceSampleRate_ / outputSampleRate_ / width);
}
