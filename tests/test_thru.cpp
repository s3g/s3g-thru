#include "thru_schwung_abi.h"
#include "sample_motion_app.h"
#include "thru_waveset_worker.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr int kFrames = MOVE_FRAMES_PER_BLOCK;
constexpr int kInputOffset = 4096;
constexpr float kOutputCeiling = 0.8912509381337456f;
std::array<std::uint8_t, 8192> mappedMemory {};
std::array<std::int16_t, kFrames * 2> output {};

using ClickEngine = s3g_daisy::SampleMotionApp;

struct ClickFixture {
    static constexpr std::size_t capacity = 8192u;
    ClickEngine engine;
    std::array<s3g_daisy::ThruWavesetBank, 4u> wavesetBanks;
    std::array<std::array<float, capacity>, ClickEngine::kChannels> storage {};
    std::array<std::array<float, kFrames>, ClickEngine::kChannels> input {};
    std::array<std::array<float, kFrames>, ClickEngine::kChannels> output {};
    std::array<const float*, ClickEngine::kChannels> inputPointers {};
    std::array<float*, ClickEngine::kChannels> outputPointers {};

    struct RenderMetrics {
        float maximumStep = 0.0f;
        float dc = 0.0f;
        float highFrequencyRatio = 0.0f;
    };

    ClickFixture()
    {
        for (std::size_t channel = 0u;
             channel < ClickEngine::kChannels; ++channel) {
            inputPointers[channel] = input[channel].data();
            outputPointers[channel] = output[channel].data();
        }
        assert(engine.prepare(static_cast<float>(MOVE_SAMPLE_RATE),
            storage[0u].data(), storage[1u].data(),
            storage[2u].data(), storage[3u].data(), capacity));
    }

    void prepareWavesets()
    {
        for (std::size_t lane = 0u; lane < 4u; ++lane) {
            if (engine.stackMode() && !engine.laneLoaded(lane)) continue;
            if (!engine.stackMode() && lane != 0u) continue;
            const auto frames = engine.stackMode() ? engine.laneFrames(lane) : capacity;
            const float* left = engine.stackMode() ? engine.laneData(lane,0u) : storage[0u].data();
            const float* right = engine.stackMode() ? engine.laneData(lane,1u) : storage[1u].data();
            wavesetBanks[lane].analyze(left,right,frames,MOVE_SAMPLE_RATE);
            engine.setPreparedWavesetBank(lane, &wavesetBanks[lane]);
        }
    }

    void loadRamp(bool stack = false)
    {
        if (stack) {
            constexpr std::size_t laneFrames = capacity / 2u;
            for (std::size_t channel = 0u; channel < 2u; ++channel) {
                float* lane = engine.writableLane(0u, channel);
                for (std::size_t frame = 0u; frame < laneFrames; ++frame) {
                    lane[frame] = -0.7f + 1.4f
                        * static_cast<float>(frame)
                        / static_cast<float>(laneFrames - 1u);
                }
            }
            assert(engine.adoptLane(
                0u, laneFrames, static_cast<float>(MOVE_SAMPLE_RATE)));
            prepareWavesets();
            return;
        }
        for (std::size_t channel = 0u; channel < 2u; ++channel) {
            for (std::size_t frame = 0u; frame < capacity; ++frame) {
                storage[channel][frame] = -0.7f + 1.4f
                    * static_cast<float>(frame)
                    / static_cast<float>(capacity - 1u);
            }
        }
        assert(engine.adoptSource(
            capacity, 2u, static_cast<float>(MOVE_SAMPLE_RATE)));
        prepareWavesets();
    }

    void loadContrastingStack()
    {
        constexpr std::size_t laneFrames = capacity / 2u;
        for (std::size_t laneIndex = 0u;
             laneIndex < ClickEngine::kStackLanes; ++laneIndex) {
            for (std::size_t channel = 0u; channel < 2u; ++channel) {
                float* lane = engine.writableLane(laneIndex, channel);
                const float offset = static_cast<float>(laneIndex) * 0.25f;
                for (std::size_t frame = 0u; frame < laneFrames; ++frame) {
                    const float ramp = -0.65f + 1.05f
                        * static_cast<float>(frame)
                        / static_cast<float>(laneFrames - 1u);
                    lane[frame] = std::min(0.75f, ramp + offset);
                }
            }
            assert(engine.adoptLane(laneIndex, laneFrames,
                static_cast<float>(MOVE_SAMPLE_RATE)));
        }
        prepareWavesets();
    }

    void loadWavesetStack()
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr std::size_t laneFrames = capacity / 2u;
        for (std::size_t laneIndex = 0u;
             laneIndex < ClickEngine::kStackLanes; ++laneIndex) {
            const double cycles = 13.0 + static_cast<double>(laneIndex) * 3.0;
            for (std::size_t channel = 0u; channel < 2u; ++channel) {
                float* lane = engine.writableLane(laneIndex, channel);
                for (std::size_t frame = 0u; frame < laneFrames; ++frame) {
                    const double phase = static_cast<double>(frame)
                        / static_cast<double>(laneFrames) * cycles * 2.0 * pi;
                    lane[frame] = 0.62f * static_cast<float>(std::sin(
                        phase + static_cast<double>(laneIndex) * 0.11
                            + static_cast<double>(channel) * 0.27));
                }
            }
            assert(engine.adoptLane(laneIndex, laneFrames,
                static_cast<float>(MOVE_SAMPLE_RATE)));
        }
        prepareWavesets();
    }

    void loadSine(float cycles = 24.0f)
    {
        constexpr double pi = 3.14159265358979323846;
        for (std::size_t channel = 0u; channel < 2u; ++channel) {
            for (std::size_t frame = 0u; frame < capacity; ++frame) {
                const double phase = static_cast<double>(frame)
                    / static_cast<double>(capacity) * cycles * 2.0 * pi;
                storage[channel][frame] = 0.65f * static_cast<float>(
                    std::sin(phase + static_cast<double>(channel) * 0.15));
            }
        }
        assert(engine.adoptSource(
            capacity, 2u, static_cast<float>(MOVE_SAMPLE_RATE)));
        prepareWavesets();
    }

    void loadAntiPhaseSine(float cycles = 31.0f)
    {
        constexpr double pi = 3.14159265358979323846;
        for (std::size_t frame = 0u; frame < capacity; ++frame) {
            const double phase = static_cast<double>(frame)
                / static_cast<double>(capacity) * cycles * 2.0 * pi;
            const float sample = 0.7f * static_cast<float>(std::sin(phase));
            storage[0u][frame] = sample;
            storage[1u][frame] = -sample;
        }
        assert(engine.adoptSource(
            capacity, 2u, static_cast<float>(MOVE_SAMPLE_RATE)));
        prepareWavesets();
    }

    void loadDcOffsetSine()
    {
        constexpr double pi = 3.14159265358979323846;
        for (std::size_t channel = 0u; channel < 2u; ++channel) {
            for (std::size_t frame = 0u; frame < capacity; ++frame) {
                const double phase = static_cast<double>(frame)
                    / static_cast<double>(capacity) * 9.0 * 2.0 * pi;
                storage[channel][frame] = 0.45f + 0.12f
                    * static_cast<float>(std::sin(phase
                        + static_cast<double>(channel) * 0.2));
            }
        }
        assert(engine.adoptSource(
            capacity, 2u, static_cast<float>(MOVE_SAMPLE_RATE)));
        prepareWavesets();
    }

    void loadDelayedSine()
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr std::size_t leadIn = 5000u;
        static_assert(leadIn < capacity);
        for (std::size_t channel = 0u; channel < 2u; ++channel) {
            for (std::size_t frame = 0u; frame < capacity; ++frame) {
                if (frame < leadIn) {
                    storage[channel][frame] = 0.0f;
                    continue;
                }
                const double phase = static_cast<double>(frame - leadIn)
                    / static_cast<double>(capacity - leadIn)
                    * 20.0 * 2.0 * pi;
                storage[channel][frame] = 0.65f * static_cast<float>(
                    std::sin(phase + static_cast<double>(channel) * 0.15));
            }
        }
        assert(engine.adoptSource(
            capacity, 2u, static_cast<float>(MOVE_SAMPLE_RATE)));
        prepareWavesets();
    }

    float renderPeak(const ClickEngine::Controls& controls, int blocks)
    {
        float peak = 0.0f;
        for (int block = 0; block < blocks; ++block) {
            engine.process(inputPointers.data(), outputPointers.data(),
                           kFrames, controls);
            for (int frame = 0; frame < kFrames; ++frame) {
                peak = std::max(peak, std::abs(
                    output[0u][static_cast<std::size_t>(frame)]));
            }
        }
        return peak;
    }

    float renderMaximumStep(const ClickEngine::Controls& controls, int blocks)
    {
        float maximum = 0.0f;
        float previous = 0.0f;
        bool havePrevious = false;
        for (int block = 0; block < blocks; ++block) {
            engine.process(inputPointers.data(), outputPointers.data(),
                           kFrames, controls);
            for (int frame = 0; frame < kFrames; ++frame) {
                const float sample = output[0u][static_cast<std::size_t>(frame)];
                assert(std::isfinite(sample));
                if (havePrevious) {
                    const float step = std::abs(sample - previous);
                    maximum = std::max(maximum, step);
                }
                previous = sample;
                havePrevious = true;
            }
        }
        return maximum;
    }

    template <typename Update>
    float renderChangingMaximumStep(
        ClickEngine::Controls controls, int blocks, Update update)
    {
        for (int block = 0; block < 8; ++block) {
            engine.process(inputPointers.data(), outputPointers.data(),
                           kFrames, controls);
        }
        float previous = output[0u][kFrames - 1u];
        float maximum = 0.0f;
        int maximumBlock = -1;
        int maximumFrame = -1;
        float maximumBefore = 0.0f;
        float maximumAfter = 0.0f;
        for (int block = 0; block < blocks; ++block) {
            update(controls, block);
            engine.process(inputPointers.data(), outputPointers.data(),
                           kFrames, controls);
            for (int frame = 0; frame < kFrames; ++frame) {
                const float sample = output[0u][static_cast<std::size_t>(frame)];
                assert(std::isfinite(sample));
                const float step = std::abs(sample - previous);
                if (step > maximum) {
                    maximum = step;
                    maximumBlock = block;
                    maximumFrame = frame;
                    maximumBefore = previous;
                    maximumAfter = sample;
                }
                previous = sample;
            }
        }
        if (maximum >= 0.5f) {
            std::fprintf(stderr,
                "Live-control maximum %.6f at %d:%d (%.6f -> %.6f)\n",
                maximum, maximumBlock, maximumFrame,
                maximumBefore, maximumAfter);
        }
        return maximum;
    }

    RenderMetrics renderChangingMetrics(
        ClickEngine::Controls controls, int blocks)
    {
        constexpr ClickEngine::WavesetProcess processes[] {
            ClickEngine::WavesetProcess::repeat,
            ClickEngine::WavesetProcess::replace,
            ClickEngine::WavesetProcess::interpolate,
            ClickEngine::WavesetProcess::groupReverse,
            ClickEngine::WavesetProcess::cycleReverse,
            ClickEngine::WavesetProcess::envelope,
        };
        RenderMetrics metrics;
        float previous = 0.0f;
        bool havePrevious = false;
        float lowOne = 0.0f;
        float lowTwo = 0.0f;
        constexpr float analysisCoefficient = 0.7921204f;
        double sum = 0.0;
        double energy = 0.0;
        double highEnergy = 0.0;
        std::size_t sampleCount = 0u;
        for (int block = 0; block < blocks; ++block) {
            if (block != 0 && block % 16 == 0) {
                controls.wavesetProcess = processes[
                    (block / 16) % (sizeof(processes) / sizeof(processes[0]))];
                controls.wavesetDirection = static_cast<ClickEngine::WavesetDirection>(
                    (block / 16) % ClickEngine::kWavesetDirectionCount);
                controls.wavesetProcessAmount
                    = ((block / 16) & 1) ? 1.0f : 0.0f;
                controls.join = (block & 16) ? 1.0f : 0.0f;
                controls.wavesetGroup = (block & 32) ? 2.0f : 16.0f;
                controls.wavesetDetail = (block & 64)
                    ? ClickEngine::WavesetDetail::hz4000
                    : ClickEngine::WavesetDetail::raw;
            }
            engine.process(inputPointers.data(), outputPointers.data(),
                           kFrames, controls);
            for (int frame = 0; frame < kFrames; ++frame) {
                const float sample = output[0u][static_cast<std::size_t>(frame)];
                assert(std::isfinite(sample));
                if (havePrevious) {
                    metrics.maximumStep = std::max(
                        metrics.maximumStep, std::abs(sample - previous));
                }
                previous = sample;
                havePrevious = true;
                lowOne += analysisCoefficient * (sample - lowOne);
                lowTwo += analysisCoefficient * (lowOne - lowTwo);
                if (block >= 4) {
                    const float high = sample - lowTwo;
                    sum += sample;
                    energy += static_cast<double>(sample) * sample;
                    highEnergy += static_cast<double>(high) * high;
                    ++sampleCount;
                }
            }
        }
        metrics.dc = sampleCount != 0u
            ? static_cast<float>(sum / static_cast<double>(sampleCount))
            : 0.0f;
        metrics.highFrequencyRatio = energy > 1.0e-12
            ? static_cast<float>(std::sqrt(highEnergy / energy)) : 0.0f;
        return metrics;
    }
};

#include "test_waveset_prepared.inc"

void testEngineClickMatrix()
{
    const auto requireSmooth = [](const char* name, float step, float limit) {
        if (step >= limit) {
            std::fprintf(stderr, "%s discontinuity: %.6f (limit %.6f)\n",
                         name, step, limit);
        }
        assert(step < limit);
    };

    {
        ClickFixture fixture;
        fixture.loadRamp();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::sample;
        controls.sampleMode = ClickEngine::SampleMode::forward;
        controls.speed = 4.0f;
        requireSmooth("Sample one-shot",
            fixture.renderMaximumStep(controls, 16), 0.12f);
    }
    {
        ClickFixture fixture;
        fixture.loadRamp();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::slices;
        controls.sampleMode = ClickEngine::SampleMode::reverse;
        controls.sampleRegionMode = ClickEngine::RegionMode::equal;
        controls.sampleRegionCount = 8.0f;
        controls.sampleSlice = 3.0f;
        controls.speed = 4.0f;
        requireSmooth("Slices reverse one-shot",
            fixture.renderMaximumStep(controls, 16), 0.12f);
    }
    {
        ClickFixture fixture;
        fixture.loadRamp();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::sample;
        controls.sampleMode = ClickEngine::SampleMode::loop;
        fixture.renderMaximumStep(controls, 8);
        controls.sampleMode = ClickEngine::SampleMode::reverse;
        requireSmooth("Sample mode change",
            fixture.renderMaximumStep(controls, 8), 0.12f);
    }
    {
        ClickFixture fixture;
        fixture.loadRamp();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::motion;
        controls.motion = ClickEngine::MotionMode::zigzag;
        controls.motionPathRate = 24.0f;
        controls.speed = 4.0f;
        controls.join = 1.0f;
        requireSmooth("Motion Zigzag",
            fixture.renderMaximumStep(controls, 48), 0.18f);
    }
    {
        ClickFixture fixture;
        fixture.loadRamp();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::motion;
        controls.motionSound = ClickEngine::MotionSound::pulser;
        controls.segmentOverlap = ClickEngine::SegmentOverlap::cut;
        controls.segmentTrigger = ClickEngine::SegmentTrigger::clock;
        controls.segmentRateHz = 64.0f;
        controls.motorShape = ClickEngine::MotorShape::pulse;
        controls.join = 0.0f;
        requireSmooth("Motion segment replacement",
            fixture.renderMaximumStep(controls, 48), 0.20f);
    }
    {
        ClickFixture fixture;
        fixture.loadContrastingStack();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::lanes;
        controls.lanes = ClickEngine::LanesMode::forward;
        controls.lanePath = ClickEngine::LanePath::stepsDown;
        controls.laneBlend = ClickEngine::LaneBlendMode::jump;
        controls.laneCycles = 8.0f;
        controls.laneSlewMs = 0.0f;
        controls.speed = 2.0f;
        requireSmooth("Lanes zero-slew jump",
            fixture.renderMaximumStep(controls, 48), 0.20f);
    }
    {
        ClickFixture fixture;
        fixture.loadRamp();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::grains;
        controls.grainReverseChance = 1.0f;
        controls.grainSizeMs = 80.0f;
        controls.grainDensityHz = 20.0f;
        controls.join = 1.0f;
        requireSmooth("Grains reverse seam",
            fixture.renderMaximumStep(controls, 48), 0.18f);
    }
    {
        ClickFixture fixture;
        fixture.loadRamp();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::longGrains;
        controls.longGrainReverseChance = 1.0f;
        controls.longGrainLengthMs = 250.0f;
        controls.longGrainLayers = 2.0f;
        controls.join = 1.0f;
        requireSmooth("Weave reverse seam",
            fixture.renderMaximumStep(controls, 96), 0.18f);
    }
    {
        ClickFixture fixture;
        fixture.loadRamp(true);
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::cutups;
        controls.cutClock = ClickEngine::CutClock::free;
        controls.cutRateHz = 64.0f;
        controls.cutJoinMs = 0.0f;
        controls.cutReverseChance = 1.0f;
        controls.cutGate = 1.0f;
        controls.join = 0.0f;
        requireSmooth("Cutups zero-join reverse",
            fixture.renderMaximumStep(controls, 48), 0.20f);
    }
    {
        ClickFixture fixture;
        fixture.loadRamp();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::glitch;
        controls.glitchClock = ClickEngine::CutClock::free;
        controls.glitchRateHz = 64.0f;
        controls.glitchSliceMs = 16.0f;
        controls.glitchSteps = 1.0f;
        controls.glitchPulses = 1.0f;
        controls.glitchRepeats = 16.0f;
        controls.glitchMix = 1.0f;
        requireSmooth("Glitch replacement",
            fixture.renderMaximumStep(controls, 64), 0.22f);
    }
    {
        ClickFixture fixture;
        fixture.loadSine();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetGroup = 8.0f;
        controls.wavesetRepeats = 3.0f;
        controls.wavesetStride = 2.0f;
        controls.wavesetDirection = ClickEngine::WavesetDirection::pendulum;
        controls.wavesetProcess = ClickEngine::WavesetProcess::groupReverse;
        controls.wavesetProcessAmount = 0.7f;
        controls.wavesetDetail = ClickEngine::WavesetDetail::hz4000;
        controls.speed = 2.0f;
        controls.join = 1.0f;
        requireSmooth("Wavesets processed group",
            fixture.renderMaximumStep(controls, 64), 0.22f);
    }
    for (std::size_t process = 0u;
         process < ClickEngine::kWavesetProcessCount; ++process) {
        for (std::size_t direction = 0u;
             direction < ClickEngine::kWavesetDirectionCount; ++direction) {
            ClickFixture fixture;
            fixture.loadSine(29.0f);
            ClickEngine::Controls controls;
            controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
            controls.wavesetGroup = 8.0f;
            controls.wavesetRepeats = 2.0f;
            controls.wavesetDirection
                = static_cast<ClickEngine::WavesetDirection>(direction);
            controls.wavesetProcess = ClickEngine::wavesetProcessAt(process);
            controls.wavesetProcessAmount = 1.0f;
            controls.wavesetDetail = ClickEngine::WavesetDetail::hz1000;
            controls.speed = 2.0f;
            controls.join = 0.0f;
            char label[96] {};
            std::snprintf(label, sizeof(label),
                "Wavesets process/order matrix %zu/%zu", process, direction);
            const bool intentionallyHarmonic
                = controls.wavesetProcess == ClickEngine::WavesetProcess::multiply
                || controls.wavesetProcess == ClickEngine::WavesetProcess::fractal
                || controls.wavesetProcess == ClickEngine::WavesetProcess::harmonic;
            requireSmooth(label,
                fixture.renderMaximumStep(controls, 32),
                intentionallyHarmonic ? 0.55f : 0.24f);
        }
    }
    {
        ClickFixture fixture;
        fixture.loadAntiPhaseSine();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetGroup = 16.0f;
        controls.wavesetDirection = ClickEngine::WavesetDirection::shuffle;
        controls.wavesetProcess = ClickEngine::WavesetProcess::cycleReverse;
        controls.wavesetProcessAmount = 1.0f;
        controls.wavesetDetail = ClickEngine::WavesetDetail::hz4000;
        controls.speed = 3.0f;
        controls.join = 0.0f;
        requireSmooth("Wavesets anti-phase stereo reference",
            fixture.renderMaximumStep(controls, 64), 0.24f);
        assert(!fixture.engine.wavesetFallbackActive());
    }
    {
        ClickFixture fixture;
        fixture.loadDcOffsetSine();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetGroup = 32.0f;
        controls.wavesetProcess = ClickEngine::WavesetProcess::harmonic;
        controls.wavesetProcessAmount = 1.0f;
        controls.speed = 4.0f;
        controls.join = 0.0f;
        requireSmooth("Wavesets guarded fallback",
            fixture.renderMaximumStep(controls, 48), 0.24f);
        assert(fixture.engine.wavesetFallbackActive());
    }
    {
        ClickFixture fixture;
        fixture.loadDelayedSine();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetGroup = 8.0f;
        controls.wavesetRepeats = 2.0f;
        controls.wavesetProcess = ClickEngine::WavesetProcess::repeat;
        controls.join = 1.0f;
        const float peak = fixture.renderPeak(controls, 192);
        assert(peak > 0.02f);
        assert(fixture.engine.normalizedPosition() > 0.25f);
    }
    {
        ClickFixture fixture;
        fixture.loadContrastingStack();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetGroup = 4.0f;
        controls.wavesetRepeats = 1.0f;
        controls.wavesetAdvance = ClickEngine::WavesetAdvance::hold;
        controls.wavesetProcess = ClickEngine::WavesetProcess::interpolate;
        controls.wavesetProcessAmount = 0.65f;
        controls.lanePath = ClickEngine::LanePath::triangle;
        controls.laneCycles = 16.0f;
        controls.laneSlewMs = 0.0f;
        controls.speed = 2.0f;
        controls.join = 0.0f;
        requireSmooth("Wavesets moving stack groups",
            fixture.renderMaximumStep(controls, 96), 0.19f);
    }
    {
        // Hardware regression: STRETCH with REPT 1 adopts a fresh measured
        // group at every group boundary. Contiguous cycles retain source time
        // without starting a second reader or treating natural slope as a seam.
        ClickFixture fixture;
        fixture.loadWavesetStack();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetGroup = 8.0f;
        controls.wavesetRepeats = 1.0f;
        controls.wavesetAdvance = ClickEngine::WavesetAdvance::stretch;
        controls.wavesetProcess = ClickEngine::WavesetProcess::repeat;
        controls.wavesetProcessAmount = 0.0f;
        controls.wavesetDetail = ClickEngine::WavesetDetail::raw;
        controls.lanePath = ClickEngine::LanePath::manual;
        controls.manualLane = 0.5f;
        controls.laneScanPaused = true;
        controls.lanePhaseOverride = true;
        controls.lanePhaseOverrideValue = 0.0f;
        controls.speed = 2.0f;
        controls.join = 0.0f;
        bool overlapObserved = false;
        float maximumStep = 0.0f;
        float previous = 0.0f;
        bool havePrevious = false;
        for (std::size_t frame = 0u; frame < 32768u; ++frame) {
            fixture.engine.process(fixture.inputPointers.data(),
                fixture.outputPointers.data(), 1u, controls);
            overlapObserved = overlapObserved
                || fixture.engine.wavesetOverlapActive();
            const float sample = fixture.output[0u][0u];
            assert(std::isfinite(sample));
            if (havePrevious) {
                maximumStep = std::max(
                    maximumStep, std::abs(sample - previous));
            }
            previous = sample;
            havePrevious = true;
        }
        assert(!overlapObserved);
        assert(fixture.engine.wavesetOverlapStartCount() == 0u);
        requireSmooth("Wavesets held-stack stretch handoff",
            maximumStep, 0.06f);
    }
    {
        ClickFixture fixture;
        fixture.loadSine(13.0f);
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetGroup = 4.0f;
        controls.wavesetRepeats = 2.0f;
        controls.wavesetProcess = ClickEngine::WavesetProcess::repeat;
        controls.wavesetProcessAmount = 0.0f;
        controls.wavesetDetail = ClickEngine::WavesetDetail::raw;
        controls.join = 1.0f;
        fixture.renderPeak(controls, 8);

        controls.wavesetGroup = 16.0f;
        controls.wavesetDirection = ClickEngine::WavesetDirection::reverse;
        controls.wavesetProcess = ClickEngine::WavesetProcess::harmonic;
        controls.wavesetProcessAmount = 1.0f;
        controls.wavesetDetail = ClickEngine::WavesetDetail::hz4000;
        controls.join = 0.0f;
        fixture.engine.process(fixture.inputPointers.data(),
            fixture.outputPointers.data(), 1u, controls);
        assert(fixture.engine.wavesetControlChangePending());
        assert(fixture.engine.wavesetActiveProcessAmount() == 0.0f);
        const auto queuedDisplay = fixture.engine.wavesetDisplayState();
        assert(queuedDisplay.active);
        assert(queuedDisplay.orderPending);
        assert(queuedDisplay.direction
            == ClickEngine::WavesetDirection::forward);
        assert(queuedDisplay.requestedDirection
            == ClickEngine::WavesetDirection::reverse);

        float previous = fixture.output[0u][0u];
        float maximumStep = 0.0f;
        bool committed = false;
        bool overlapObserved = false;
        for (int frame = 0; frame < 4096; ++frame) {
            fixture.engine.process(fixture.inputPointers.data(),
                fixture.outputPointers.data(), 1u, controls);
            const float sample = fixture.output[0u][0u];
            assert(std::isfinite(sample));
            maximumStep = std::max(maximumStep,
                std::abs(sample - previous));
            previous = sample;
            if (fixture.engine.wavesetActiveProcessAmount() > 0.99f) {
                committed = true;
                overlapObserved = overlapObserved
                    || fixture.engine.wavesetOverlapActive();
            }
        }
        assert(committed);
        assert(overlapObserved);
        assert(!fixture.engine.wavesetControlChangePending());
        const auto committedDisplay = fixture.engine.wavesetDisplayState();
        assert(committedDisplay.active);
        assert(!committedDisplay.orderPending);
        assert(committedDisplay.direction
            == ClickEngine::WavesetDirection::reverse);
        assert(committedDisplay.groupPosition >= 0.0f
            && committedDisplay.groupPosition <= 1.0f);
        assert(committedDisplay.sourcePosition >= 0.0f
            && committedDisplay.sourcePosition <= 1.0f);
        requireSmooth("Wavesets safe-point control commit",
            maximumStep, 0.30f);
    }
    {
        ClickFixture fixture;
        fixture.loadSine(29.0f);
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetGroup = 8.0f;
        controls.wavesetProcess
            = ClickEngine::WavesetProcess::cycleReverse;
        controls.wavesetProcessAmount = 1.0f;
        controls.join = 1.0f;
        fixture.renderPeak(controls, 8);
        const auto display = fixture.engine.wavesetDisplayState();
        assert(display.active);
        assert(display.primaryReversed);
        assert(display.secondaryGroupPosition < 0.0f
            || display.secondaryGroupPosition <= 1.0f);
        assert(display.secondarySourcePosition < 0.0f
            || display.secondarySourcePosition <= 1.0f);
    }
    {
        ClickFixture fixture;
        fixture.loadSine(37.0f);
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetGroup = 4.0f;
        controls.wavesetRepeats = 1.0f;
        controls.wavesetProcessAmount = 0.0f;
        controls.speed = 4.0f;
        controls.join = 0.0f;
        const ClickFixture::RenderMetrics metrics
            = fixture.renderChangingMetrics(controls, 192);
        requireSmooth("Wavesets live parameter changes",
            metrics.maximumStep, 0.20f);
        if (std::abs(metrics.dc) >= 0.05f
            || metrics.highFrequencyRatio >= 0.35f) {
            std::fprintf(stderr,
                "Wavesets live sweep metrics: dc %.6f, high ratio %.6f\n",
                metrics.dc, metrics.highFrequencyRatio);
        }
        assert(std::abs(metrics.dc) < 0.05f);
        assert(metrics.highFrequencyRatio < 0.35f);
    }
}

void testWavesetRetiredProcesses()
{
    // The public menu is contiguous, while saved process/gesture IDs remain
    // stable. Removed or invalid stored values must never reach a renderer.
    for (const unsigned retired : {5u, 11u, 255u}) {
        ClickFixture reference;
        ClickFixture retiredFixture;
        reference.loadSine(19.0f);
        retiredFixture.loadSine(19.0f);
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetAdvance = ClickEngine::WavesetAdvance::hold;
        controls.wavesetProcessAmount = 1.0f;
        ClickEngine::Controls retiredControls = controls;
        retiredControls.wavesetProcess
            = static_cast<ClickEngine::WavesetProcess>(retired);
        for (int block = 0; block < 16; ++block) {
            reference.engine.process(reference.inputPointers.data(),
                reference.outputPointers.data(), kFrames, controls);
            retiredFixture.engine.process(retiredFixture.inputPointers.data(),
                retiredFixture.outputPointers.data(), kFrames, retiredControls);
            for (std::size_t channel = 0u; channel < 2u; ++channel) {
                for (int frame = 0; frame < kFrames; ++frame) {
                    assert(reference.output[channel][frame]
                        == retiredFixture.output[channel][frame]);
                }
            }
        }
        assert(retiredFixture.engine.wavesetDisplayState().process
            == ClickEngine::WavesetProcess::repeat);
    }
}

void testLiveParameterTransitionMatrix()
{
    const auto requireSmooth = [](const char* name, float step, float limit) {
        if (step >= limit) {
            std::fprintf(stderr, "%s live-control discontinuity: %.6f "
                         "(limit %.6f)\n", name, step, limit);
        }
        assert(step < limit);
    };

    {
        ClickFixture fixture;
        fixture.loadSine(19.0f);
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::sample;
        controls.sampleMode = ClickEngine::SampleMode::loop;
        controls.locus = 0.5f;
        controls.field = 0.8f;
        controls.speed = 1.0f;
        controls.join = 1.0f;
        const float step = fixture.renderChangingMaximumStep(
            controls, 160, [](ClickEngine::Controls& live, int block) {
                if (block % 4 != 0) return;
                const bool alternate = (block / 4) & 1;
                live.locus = alternate ? 0.18f : 0.82f;
                live.field = alternate ? 0.24f : 0.92f;
                live.speed = alternate ? 0.08f : 4.0f;
                live.join = alternate ? 0.0f : 1.0f;
            });
        requireSmooth("Sample shared controls", step, 0.24f);
    }

    {
        ClickFixture fixture;
        fixture.loadSine(23.0f);
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::motion;
        controls.motion = ClickEngine::MotionMode::zigzag;
        controls.motionPathRate = 4.0f;
        controls.speed = 1.0f;
        controls.join = 1.0f;
        const float step = fixture.renderChangingMaximumStep(
            controls, 192, [](ClickEngine::Controls& live, int block) {
                if (block % 4 != 0) return;
                const bool alternate = (block / 4) & 1;
                live.motionTravel = alternate ? 0.08f : 1.0f;
                live.motionJitter = alternate ? 0.0f : 1.0f;
                live.motionSoundRateHz = alternate ? 0.25f : 20.0f;
                live.motorRateHz = alternate ? 0.05f : 20.0f;
                live.packetDuty = alternate ? 0.05f : 0.95f;
                live.motorSymmetry = alternate ? 0.05f : 0.95f;
                live.motionSeed = alternate ? 1.0f : 9999.0f;
                live.join = alternate ? 0.0f : 1.0f;
            });
        requireSmooth("Motion continuous controls", step, 0.28f);
    }

    {
        ClickFixture fixture;
        fixture.loadContrastingStack();
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::lanes;
        controls.lanes = ClickEngine::LanesMode::forward;
        controls.lanePath = ClickEngine::LanePath::triangle;
        controls.laneBlend = ClickEngine::LaneBlendMode::crossfade;
        controls.lanesStretch = true;
        controls.lanesStretchRatio = 1.0f;
        const float step = fixture.renderChangingMaximumStep(
            controls, 192, [](ClickEngine::Controls& live, int block) {
                if (block % 4 != 0) return;
                const bool alternate = (block / 4) & 1;
                live.lanePath = alternate
                    ? ClickEngine::LanePath::stepsDown
                    : ClickEngine::LanePath::random;
                live.laneBlend = alternate
                    ? ClickEngine::LaneBlendMode::jump
                    : ClickEngine::LaneBlendMode::crossfade;
                live.laneCycles = alternate ? 0.125f : 16.0f;
                live.lanePathOffset = alternate ? 0.0f : 1.0f;
                live.lanePathSkew = alternate ? 0.02f : 0.98f;
                live.lanePathCurve = alternate ? -1.0f : 1.0f;
                live.lanesStretchRatio = alternate ? 0.25f : 4.0f;
                live.join = alternate ? 0.0f : 1.0f;
            });
        requireSmooth("Lanes path and stretch controls", step, 0.30f);
    }

    {
        ClickFixture fixture;
        fixture.loadSine(17.0f);
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::motion;
        controls.sampleMode = ClickEngine::SampleMode::loop;
        controls.cutClock = ClickEngine::CutClock::free;
        controls.cutRateHz = 12.0f;
        controls.glitchClock = ClickEngine::CutClock::free;
        controls.glitchRateHz = 12.0f;
        controls.longGrainLengthMs = 250.0f;
        controls.wavesetGroup = 4.0f;
        const float step = fixture.renderChangingMaximumStep(
            controls, 320, [](ClickEngine::Controls& live, int block) {
                if (block % 24 != 0) return;
                live.algorithm = static_cast<ClickEngine::PlaybackAlgorithm>(
                    (block / 24) % 9);
            });
        requireSmooth("Playback algorithm changes", step, 0.30f);
    }
}

void fillInput(std::int16_t level)
{
    auto* input = reinterpret_cast<std::int16_t*>(mappedMemory.data() + kInputOffset);
    for (int frame = 0; frame < kFrames; ++frame) {
        input[frame * 2] = level;
        input[frame * 2 + 1] = static_cast<std::int16_t>(-level);
    }
}

void render(plugin_api_v2_t* api, void* instance, int blocks)
{
    for (int block = 0; block < blocks; ++block) {
        output.fill(0);
        api->render_block(instance, output.data(), kFrames);
    }
}

void midi(plugin_api_v2_t* api, void* instance,
          std::uint8_t status, std::uint8_t data1, std::uint8_t data2,
          int source = MOVE_MIDI_SOURCE_HOST)
{
    const std::uint8_t message[3] { status, data1, data2 };
    api->on_midi(instance, message, 3, source);
}

std::string get(plugin_api_v2_t* api, void* instance, const char* key)
{
    std::array<char, 16384> buffer {};
    const int length = api->get_param(instance, key, buffer.data(),
                                      static_cast<int>(buffer.size()));
    assert(length >= 0);
    assert(length < static_cast<int>(buffer.size()));
    return std::string(buffer.data(), static_cast<std::size_t>(length));
}

int peak()
{
    int result = 0;
    for (const std::int16_t sample : output) {
        const int magnitude = sample == -32768 ? 32768 : std::abs(static_cast<int>(sample));
        result = std::max(result, magnitude);
    }
    return result;
}

int stereoDifferencePeak()
{
    int result = 0;
    for (int frame = 0; frame < kFrames; ++frame) {
        result = std::max(result, std::abs(
            static_cast<int>(output[frame * 2])
            - static_cast<int>(output[frame * 2 + 1])));
    }
    return result;
}

int renderStereoDifference(
    plugin_api_v2_t* api, void* instance, int blocks)
{
    int result = 0;
    for (int block = 0; block < blocks; ++block) {
        render(api, instance, 1);
        result = std::max(result, stereoDifferencePeak());
    }
    return result;
}

void accumulateMaximumOutputStep(
    int& result, std::array<int, 2u>& previous, bool& havePrevious)
{
    for (int frame = 0; frame < kFrames; ++frame) {
        for (int channel = 0; channel < 2; ++channel) {
            const int sample = output[frame * 2 + channel];
            if (havePrevious) {
                result = std::max(result,
                    std::abs(sample - previous[channel]));
            }
            previous[channel] = sample;
        }
        havePrevious = true;
    }
}

int renderMaximumOutputStep(
    plugin_api_v2_t* api, void* instance, int blocks)
{
    int result = 0;
    std::array<int, 2u> previous {};
    bool havePrevious = false;
    for (int block = 0; block < blocks; ++block) {
        render(api, instance, 1);
        accumulateMaximumOutputStep(result, previous, havePrevious);
    }
    return result;
}

int stateField(const std::string& state, int index)
{
    int current = 0;
    std::size_t start = 0u;
    while (current < index) {
        start = state.find(',', start);
        assert(start != std::string::npos);
        ++start;
        ++current;
    }
    return std::atoi(state.c_str() + start);
}

std::uint32_t unsignedStateField(const std::string& state, int index)
{
    int current = 0;
    std::size_t start = 0u;
    while (current < index) {
        start = state.find(',', start);
        assert(start != std::string::npos);
        ++start;
        ++current;
    }
    return static_cast<std::uint32_t>(std::strtoul(state.c_str() + start, nullptr, 10));
}

void recordLane(plugin_api_v2_t* api, void* instance,
                int lane, std::int16_t level, int blocks)
{
    api->set_param(instance, "record_lane", std::to_string(lane).c_str());
    fillInput(level);
    midi(api, instance, 0xB0, 52, 127, MOVE_MIDI_SOURCE_EXTERNAL);
    assert(get(api, instance, "recording") == "1");
    render(api, instance, blocks);
    midi(api, instance, 0xB0, 52, 0, MOVE_MIDI_SOURCE_EXTERNAL);
    assert(get(api, instance, "recording") == "0");
    render(api, instance, 6);
}

void writePcm16Wav(const std::string& path,
                   std::uint32_t sampleRate = MOVE_SAMPLE_RATE,
                   bool cueCrossing = false,
                   bool continuousTone = false)
{
    constexpr std::uint32_t frames = 4096u;
    constexpr std::uint32_t dataBytes = frames * 2u * sizeof(std::int16_t);
    std::FILE* file = std::fopen(path.c_str(), "wb");
    assert(file != nullptr);
    const auto u16 = [&](std::uint16_t value) {
        const std::uint8_t bytes[2] {
            static_cast<std::uint8_t>(value),
            static_cast<std::uint8_t>(value >> 8u)
        };
        assert(std::fwrite(bytes, 1u, sizeof(bytes), file) == sizeof(bytes));
    };
    const auto u32 = [&](std::uint32_t value) {
        const std::uint8_t bytes[4] {
            static_cast<std::uint8_t>(value),
            static_cast<std::uint8_t>(value >> 8u),
            static_cast<std::uint8_t>(value >> 16u),
            static_cast<std::uint8_t>(value >> 24u)
        };
        assert(std::fwrite(bytes, 1u, sizeof(bytes), file) == sizeof(bytes));
    };
    assert(std::fwrite("RIFF", 1u, 4u, file) == 4u);
    u32(36u + dataBytes);
    assert(std::fwrite("WAVEfmt ", 1u, 8u, file) == 8u);
    u32(16u); u16(1u); u16(2u); u32(sampleRate);
    u32(sampleRate * 4u); u16(4u); u16(16u);
    assert(std::fwrite("data", 1u, 4u, file) == 4u);
    u32(dataBytes);
    for (std::uint32_t frame = 0u; frame < frames; ++frame) {
        if (continuousTone) {
            constexpr float twoPi = 6.28318530717958647692f;
            const float phase = twoPi * 32.0f
                * static_cast<float>(frame) / static_cast<float>(frames);
            const auto left = static_cast<std::int16_t>(
                std::lround(std::sin(phase) * 12000.0f));
            const auto right = static_cast<std::int16_t>(
                std::lround(std::sin(phase) * -6000.0f));
            u16(static_cast<std::uint16_t>(left));
            u16(static_cast<std::uint16_t>(right));
            continue;
        }
        if (cueCrossing) {
            const std::int16_t sample = frame < 1200u ? 12000
                : (frame == 1200u ? 0 : -12000);
            u16(static_cast<std::uint16_t>(sample));
            u16(static_cast<std::uint16_t>(sample));
            continue;
        }
        const bool burst = frame < 192u
            || (frame >= 1100u && frame < 1292u)
            || (frame >= 2200u && frame < 2392u)
            || (frame >= 3300u && frame < 3492u);
        u16(static_cast<std::uint16_t>(burst ? 12000 : 0));
        u16(static_cast<std::uint16_t>(burst ? -6000 : 0));
    }
    assert(std::fclose(file) == 0);
}

std::uint32_t littleU32(
    const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    assert(offset + 4u <= bytes.size());
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u
        | static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u
        | static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u;
}

void appendLittleU32(
    std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16u));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24u));
}

void downgradeClipBankToV8(const std::string& path)
{
    struct stat status {};
    assert(stat(path.c_str(), &status) == 0);
    assert(status.st_size > 4);
    std::vector<std::uint8_t> input(
        static_cast<std::size_t>(status.st_size) - 4u);
    std::FILE* file = std::fopen(path.c_str(), "rb");
    assert(file != nullptr);
    assert(std::fread(input.data(), 1u, input.size(), file) == input.size());
    assert(std::fclose(file) == 0);
    assert(input.size() >= 240u);
    assert(std::memcmp(input.data(), "S3GTHR3", 7u) == 0);
    assert(littleU32(input, 8u) == 11u);

    std::vector<std::uint8_t> output;
    output.reserve(input.size());
    output.insert(output.end(), input.begin(), input.begin() + 8u);
    appendLittleU32(output, 8u);
    std::size_t offset = 12u;
    const auto copyBytes = [&](std::size_t count) {
        assert(offset + count <= input.size());
        output.insert(output.end(), input.begin() + offset,
                      input.begin() + offset + count);
        offset += count;
    };
    copyBytes(228u); // generation/globals plus the 16-entry chain
    for (std::size_t clip = 0u; clip < 64u; ++clip) {
        copyBytes(4u + 132u * 4u); // occupied and format-8 parameters
        offset += 20u * 4u; // Tape controls and Wavesets parameters were appended.
        copyBytes(4u * (4u + 768u + 64u) + 8u * 4u); // sources and cues
        copyBytes(36u); // recorded state through take count
        const std::uint32_t noteCount = littleU32(input, offset);
        const std::uint32_t pointCount = littleU32(input, offset + 4u);
        copyBytes(8u);
        copyBytes(static_cast<std::size_t>(noteCount) * 24u);
        copyBytes(static_cast<std::size_t>(pointCount) * 24u);
    }
    assert(offset == input.size());
    std::uint32_t checksum = 2166136261u;
    for (const std::uint8_t byte : output) {
        checksum ^= byte;
        checksum *= 16777619u;
    }
    appendLittleU32(output, checksum);
    file = std::fopen(path.c_str(), "wb");
    assert(file != nullptr);
    assert(std::fwrite(output.data(), 1u, output.size(), file) == output.size());
    assert(std::fclose(file) == 0);
}

void downgradeClipBankToV9(const std::string& path)
{
    struct stat status {};
    assert(stat(path.c_str(), &status) == 0);
    assert(status.st_size > 4);
    std::vector<std::uint8_t> input(
        static_cast<std::size_t>(status.st_size) - 4u);
    std::FILE* file = std::fopen(path.c_str(), "rb");
    assert(file != nullptr);
    assert(std::fread(input.data(), 1u, input.size(), file) == input.size());
    assert(std::fclose(file) == 0);
    assert(input.size() >= 240u);
    assert(std::memcmp(input.data(), "S3GTHR3", 7u) == 0);
    assert(littleU32(input, 8u) == 11u);

    std::vector<std::uint8_t> output;
    output.reserve(input.size());
    output.insert(output.end(), input.begin(), input.begin() + 8u);
    appendLittleU32(output, 9u);
    std::size_t offset = 12u;
    const auto copyBytes = [&](std::size_t count) {
        assert(offset + count <= input.size());
        output.insert(output.end(), input.begin() + offset,
                      input.begin() + offset + count);
        offset += count;
    };
    copyBytes(228u); // generation/globals plus the 16-entry chain
    for (std::size_t clip = 0u; clip < 64u; ++clip) {
        copyBytes(4u + 140u * 4u); // occupied and format-9 parameters
        offset += 12u * 4u; // format-10 tape macros plus format-11 Wavesets
        copyBytes(4u * (4u + 768u + 64u) + 8u * 4u); // sources and cues
        copyBytes(36u); // recorded state through take count
        const std::uint32_t noteCount = littleU32(input, offset);
        const std::uint32_t pointCount = littleU32(input, offset + 4u);
        copyBytes(8u);
        copyBytes(static_cast<std::size_t>(noteCount) * 24u);
        copyBytes(static_cast<std::size_t>(pointCount) * 24u);
    }
    assert(offset == input.size());
    std::uint32_t checksum = 2166136261u;
    for (const std::uint8_t byte : output) {
        checksum ^= byte;
        checksum *= 16777619u;
    }
    appendLittleU32(output, checksum);
    file = std::fopen(path.c_str(), "wb");
    assert(file != nullptr);
    assert(std::fwrite(output.data(), 1u, output.size(), file) == output.size());
    assert(std::fclose(file) == 0);
}

void downgradeClipBankToV1(const std::string& path)
{
    struct stat status {};
    assert(stat(path.c_str(), &status) == 0);
    assert(status.st_size > 4);
    std::vector<std::uint8_t> input(
        static_cast<std::size_t>(status.st_size) - 4u);
    std::FILE* file = std::fopen(path.c_str(), "rb");
    assert(file != nullptr);
    assert(std::fread(input.data(), 1u, input.size(), file) == input.size());
    assert(std::fclose(file) == 0);
    assert(input.size() >= 240u);
    assert(std::memcmp(input.data(), "S3GTHR3", 7u) == 0);
    assert(littleU32(input, 8u) == 11u);

    std::vector<std::uint8_t> output;
    output.reserve(input.size());
    output.insert(output.end(), input.begin(), input.begin() + 8u);
    appendLittleU32(output, 1u);
    std::size_t offset = 12u;
    const auto copyBytes = [&](std::size_t count) {
        assert(offset + count <= input.size());
        output.insert(output.end(), input.begin() + offset,
                      input.begin() + offset + count);
        offset += count;
    };
    copyBytes(228u); // generation/globals plus the 16-entry chain
    for (std::size_t clip = 0u; clip < 64u; ++clip) {
        copyBytes(4u + 68u * 4u); // occupied and version-1 base parameters
        offset += 84u * 4u; // fidelity, effects, Weave, ADSR, tape, Wavesets
        copyBytes(4u * (4u + 768u + 64u) + 8u * 4u); // sources and cues
        copyBytes(32u); // recorded through capacity flags
        offset += 4u; // take count does not exist in format 1
        const std::uint32_t noteCount = littleU32(input, offset);
        const std::uint32_t pointCount = littleU32(input, offset + 4u);
        copyBytes(8u);
        for (std::uint32_t note = 0u; note < noteCount; ++note) {
            copyBytes(20u);
            offset += 4u; // note take
        }
        for (std::uint32_t point = 0u; point < pointCount; ++point) {
            copyBytes(20u);
            offset += 4u; // gesture take
        }
    }
    assert(offset == input.size());
    std::uint32_t checksum = 2166136261u;
    for (const std::uint8_t byte : output) {
        checksum ^= byte;
        checksum *= 16777619u;
    }
    appendLittleU32(output, checksum);
    file = std::fopen(path.c_str(), "wb");
    assert(file != nullptr);
    assert(std::fwrite(output.data(), 1u, output.size(), file) == output.size());
    assert(std::fclose(file) == 0);
}

void downgradeClipBankToV2(const std::string& path)
{
    struct stat status {};
    assert(stat(path.c_str(), &status) == 0);
    assert(status.st_size > 4);
    std::vector<std::uint8_t> input(
        static_cast<std::size_t>(status.st_size) - 4u);
    std::FILE* file = std::fopen(path.c_str(), "rb");
    assert(file != nullptr);
    assert(std::fread(input.data(), 1u, input.size(), file) == input.size());
    assert(std::fclose(file) == 0);
    assert(input.size() >= 240u);
    assert(std::memcmp(input.data(), "S3GTHR3", 7u) == 0);
    assert(littleU32(input, 8u) == 11u);

    std::vector<std::uint8_t> output;
    output.reserve(input.size());
    output.insert(output.end(), input.begin(), input.begin() + 8u);
    appendLittleU32(output, 2u);
    std::size_t offset = 12u;
    const auto copyBytes = [&](std::size_t count) {
        assert(offset + count <= input.size());
        output.insert(output.end(), input.begin() + offset,
                      input.begin() + offset + count);
        offset += count;
    };
    copyBytes(228u); // generation/globals plus the 16-entry chain
    for (std::size_t clip = 0u; clip < 64u; ++clip) {
        copyBytes(4u + 68u * 4u); // occupied and format-2 base parameters
        offset += 84u * 4u; // fidelity, effects, Weave, ADSR, tape, Wavesets
        copyBytes(4u * (4u + 768u + 64u) + 8u * 4u); // sources and cues
        copyBytes(32u + 4u); // capacities/flags and take count
        const std::uint32_t noteCount = littleU32(input, offset);
        const std::uint32_t pointCount = littleU32(input, offset + 4u);
        copyBytes(8u);
        copyBytes(static_cast<std::size_t>(noteCount) * 24u);
        copyBytes(static_cast<std::size_t>(pointCount) * 24u);
    }
    assert(offset == input.size());
    std::uint32_t checksum = 2166136261u;
    for (const std::uint8_t byte : output) {
        checksum ^= byte;
        checksum *= 16777619u;
    }
    appendLittleU32(output, checksum);
    file = std::fopen(path.c_str(), "wb");
    assert(file != nullptr);
    assert(std::fwrite(output.data(), 1u, output.size(), file) == output.size());
    assert(std::fclose(file) == 0);
}

void waitForClip(plugin_api_v2_t* api, void* instance, int clip)
{
    for (int attempt = 0; attempt < 1200
         && stateField(get(api, instance, "clip_bank_state"), 0) != clip;
         ++attempt) {
        render(api, instance, 1);
        usleep(1000);
    }
    if (stateField(get(api, instance, "clip_bank_state"), 0) != clip) {
        std::fprintf(stderr, "Clip %d failed to activate: state=%s prepare=%s error=%s\n",
                     clip, get(api, instance, "clip_bank_state").c_str(),
                     get(api, instance, "clip_prepare_state").c_str(),
                     get(api, instance, "sample_load_state").c_str());
    }
    assert(stateField(get(api, instance, "clip_bank_state"), 0) == clip);
}

std::vector<std::string> directoryFiles(const std::string& directory)
{
    std::vector<std::string> files;
    DIR* handle = opendir(directory.c_str());
    if (!handle) return files;
    while (dirent* entry = readdir(handle)) {
        if (entry->d_name[0] == '.') continue;
        files.emplace_back(directory + "/" + entry->d_name);
    }
    closedir(handle);
    std::sort(files.begin(), files.end());
    return files;
}

} // namespace

int main()
{
    testPreparedWavesetProgression();
    testPreparedWavesetControlLatency();
    testPreparedWavesetLaneReaders();
    testWavesetSnapshotWorker();
    testEngineClickMatrix();
    testWavesetRetiredProcesses();
    testLiveParameterTransitionMatrix();
    host_api_v1_t host {};
    host.api_version = 1;
    host.sample_rate = MOVE_SAMPLE_RATE;
    host.frames_per_block = MOVE_FRAMES_PER_BLOCK;
    host.mapped_memory = mappedMemory.data();
    host.audio_in_offset = kInputOffset;

    plugin_api_v2_t* api = move_plugin_init_v2(&host);
    assert(api != nullptr);
    assert(api->api_version == MOVE_PLUGIN_API_VERSION_2);
    assert(api->get_error != nullptr);
    const std::string persistenceDir = "/tmp/s3g-thru-persistence-"
        + std::to_string(static_cast<long>(getpid()));
    assert(mkdir(persistenceDir.c_str(), 0700) == 0);
    const std::string libraryDir = persistenceDir + "/library";
    assert(mkdir(libraryDir.c_str(), 0700) == 0);
    void* instance = api->create_instance(persistenceDir.c_str(), nullptr);
    assert(instance != nullptr);
    assert(get(api, instance, "name") == "s3g Thru");
    assert(get(api, instance, "captures_directory")
        == persistenceDir + "/captures");

    assert(get(api, instance, "polyphony") == "8");
    constexpr const char* noPlayheads = "-1,-1,-1,-1,-1,-1,-1,-1";
    assert(get(api, instance, "playheads") == noPlayheads);
    assert(get(api, instance, "waveset_state").empty());

    /* Every engine/global control has a stable absolute external MIDI CC.
       Internal Move CCs remain reserved for the takeover surface. */
    const std::string defaultState = get(api, instance, "state");
    constexpr const char* waveProcessNames[] {
        "Repeat", "Omit", "Replace", "Envelope", "Multiply", "Interpolate",
        "Fractal", "Harmonic", "Group Reverse", "Cycle Reverse",
    };
    constexpr int savedWaveProcessIds[] {0, 1, 2, 3, 4, 6, 7, 8, 9, 10};
    for (std::size_t selection = 0u; selection < 10u; ++selection) {
        api->set_param(instance, "waveset_process", waveProcessNames[selection]);
        assert(get(api, instance, "waveset_process") == std::to_string(selection));
        const std::string saved = get(api, instance, "state");
        assert(saved.find("\"waveset_process\":"
            + std::to_string(savedWaveProcessIds[selection]) + ",")
            != std::string::npos);
        api->set_param(instance, "waveset_process", "Repeat");
        api->set_param(instance, "state", saved.c_str());
        assert(get(api, instance, "waveset_process") == std::to_string(selection));
    }
    for (const int retired : {5, 11}) {
        std::string saved = defaultState;
        const std::string key = "\"waveset_process\":0";
        const std::size_t at = saved.find(key);
        assert(at != std::string::npos);
        saved.replace(at, key.size(), "\"waveset_process\":" + std::to_string(retired));
        api->set_param(instance, "state", saved.c_str());
        assert(get(api, instance, "waveset_process") == "0");
    }
    api->set_param(instance, "state", defaultState.c_str());
    assert(get(api, instance, "polyphony_comp") == "1");
    api->set_param(instance, "polyphony_comp", "0");
    const std::string uncompensatedState = get(api, instance, "state");
    assert(uncompensatedState.find("\"polyphony_comp\":0")
        != std::string::npos);
    api->set_param(instance, "polyphony_comp", "1");
    api->set_param(instance, "state", uncompensatedState.c_str());
    assert(get(api, instance, "polyphony_comp") == "0");
    api->set_param(instance, "state", defaultState.c_str());
    const std::string initialLocus = get(api, instance, "locus");
    api->set_param(instance, "hold_release_ms", "1250");
    const std::string holdState = get(api, instance, "state");
    api->set_param(instance, "hold_release_ms", "0");
    api->set_param(instance, "state", holdState.c_str());
    assert(get(api, instance, "hold_release_ms") == "1250");
    api->set_param(instance, "amp_attack", "150");
    api->set_param(instance, "amp_decay", "400");
    api->set_param(instance, "amp_sustain", "37");
    api->set_param(instance, "amp_release", "750");
    const std::string envelopeState = get(api, instance, "state");
    api->set_param(instance, "amp_attack", "0");
    api->set_param(instance, "amp_decay", "0");
    api->set_param(instance, "amp_sustain", "100");
    api->set_param(instance, "amp_release", "0");
    api->set_param(instance, "state", envelopeState.c_str());
    assert(get(api, instance, "amp_attack") == "150.00");
    assert(get(api, instance, "amp_decay") == "400.00");
    assert(get(api, instance, "amp_sustain") == "37.00");
    assert(get(api, instance, "amp_release") == "750.00");
    api->set_param(instance, "state", defaultState.c_str());
    api->set_param(instance, "state", "{\"version\":4,\"locus\":0}");
    assert(get(api, instance, "locus") == initialLocus);
    std::array<char, 16> removedParam {};
    assert(api->get_param(instance, "seq_bpm", removedParam.data(),
                          static_cast<int>(removedParam.size())) < 0);
    std::vector<char> chainParamBuffer(65536u, '\0');
    const int chainParamLength = api->get_param(instance, "chain_params",
        chainParamBuffer.data(), static_cast<int>(chainParamBuffer.size()));
    assert(chainParamLength > 0);
    assert(chainParamLength < static_cast<int>(chainParamBuffer.size()));
    const std::string chainParams(chainParamBuffer.data(),
        static_cast<std::size_t>(chainParamLength));
    assert(chainParams.find("\"Multiply\",\"Interpolate\",\"Fractal\","
        "\"Harmonic\",\"Group Reverse\",\"Cycle Reverse\"]")
        != std::string::npos);
    assert(chainParams.find("\"Average\"") == std::string::npos);
    assert(chainParams.find("\"Telescope\"") == std::string::npos);
    assert(chainParams.find("\"effect_tape_format\"") != std::string::npos);
    assert(chainParams.find("\"options\":[\"2\\\"\",\"1\\\"\","
        "\"1/2\\\"\",\"1/4\\\"\",\"1/8\\\"\"]")
        != std::string::npos);
    struct CcCheck { int cc; const char* key; const char* high; };
    const CcCheck ccChecks[] {
        {20, "locus", "1.0000"}, {21, "field", "1.0000"},
        {22, "speed", "4.0000"}, {23, "manual_lane", "4.0000"},
        {24, "lane_cycles", "16.0000"}, {25, "algorithm", "8"},
        {26, "join", "1.0000"}, {27, "output_gain", "2.0000"},
        {28, "motion_mode", "7"}, {29, "motion_sound", "7"},
        {30, "motion_rate", "20.0000"}, {31, "lanes_mode", "2"},
        {32, "lane_path", "7"}, {33, "lanes_stretch", "1"},
        {34, "stretch_ratio", "4.0000"}, {35, "lane_1_gain", "2.0000"},
        {36, "lane_2_gain", "2.0000"}, {37, "lane_3_gain", "2.0000"},
        {38, "lane_4_gain", "2.0000"}, {39, "grain_mode", "3"},
        {40, "grain_size", "1000.0000"}, {41, "grain_density", "96.0000"},
        {42, "grain_pitch", "4.0000"}, {43, "grain_window", "5"},
        {44, "grain_spread", "1.0000"}, {45, "cutups_mode", "5"},
        {46, "pressure_mode", "1"}, {47, "root_note", "96"},
        {48, "record_lane", "4"}, {49, "record_source", "1"},
        {50, "record_gain", "4.0000"}, {51, "monitor_level", "1.0000"},
        {54, "tempo_bpm", "240.00"}, {55, "tempo_swing", "75.00"},
        {56, "clip_playing", "1"},
        {69, "motion_travel", "1.0000"}, {70, "motion_jitter", "1.0000"},
        {71, "packet_duty", "98.00"}, {72, "motor_rate", "20.0000"},
        {73, "motor_symmetry", "95.00"}, {74, "motor_shape", "4"},
        {75, "lane_blend", "1"}, {76, "lane_path_shape", "3"},
        {77, "lane_path_offset", "1.0000"}, {78, "lane_path_skew", "0.9800"},
        {79, "lane_path_curve", "1.0000"}, {80, "lane_slew", "500.00"},
        {81, "grain_spray", "1.0000"}, {82, "grain_bias", "2"},
        {83, "grain_pitch_spray", "24.0000"}, {84, "grain_reverse", "100.00"},
        {85, "grain_size_variation", "1.0000"},
        {86, "grain_level_variation", "1.0000"}, {87, "grain_timing", "1"},
        {88, "grain_timing_scatter", "1.0000"},
        {89, "grain_envelope_skew", "1.0000"}, {90, "grain_process", "4"},
        {91, "grain_process_amount", "1.0000"}, {92, "grain_regions", "64"},
        {93, "grain_advance", "1"}, {94, "cut_clock", "1"},
        {95, "cut_division", "5"}, {96, "cut_rate", "64.0000"},
        {97, "cut_time_variation", "1.0000"}, {98, "cut_gate", "100.00"},
        {99, "cut_join", "100.00"}, {100, "cut_regions", "64"},
        {101, "cut_repeats", "16"}, {102, "cut_reverse", "100.00"},
        {103, "cut_pitch_variation", "24.0000"},
        {104, "cut_level_variation", "1.0000"}, {105, "cut_lane_order", "6"},
        {106, "cut_poly_path", "3"}, {107, "cut_region_mode", "1"},
        {108, "sample_mode", "3"}, {109, "slice_region_mode", "1"},
        {110, "slice_regions", "64"}, {111, "pad_mode", "3"},
        {112, "lane_variation", "4"}, {113, "preview_level", "1.0000"},
        {59, "motion_rate_basis", "1"},
        {60, "motion_path_rate", "80.0000"},
        {61, "motion_step", "1.0000"}, {62, "motion_seed", "9999"},
        {63, "segment_trigger", "2"}, {64, "segment_overlap", "1"},
        {118, "segment_layers", "4"},
        {119, "segment_rate", "80.0000"},
        {121, "segment_repeats", "16"},
        {122, "segment_step", "1.0000"},
        {124, "segment_pitch", "12.0000"},
        {125, "segment_level", "1.0000"},
        {126, "segment_curve", "1.0000"},
    };
    for (const auto& check : ccChecks) {
        midi(api, instance, 0xB0, static_cast<std::uint8_t>(check.cc), 127,
             MOVE_MIDI_SOURCE_EXTERNAL);
        const std::string actual = get(api, instance, check.key);
        if (actual != check.high) {
            std::fprintf(stderr, "CC %d %s: expected %s, got %s\n",
                check.cc, check.key, check.high, actual.c_str());
        }
        assert(actual == check.high);
    }
    midi(api, instance, 0xB0, 20, 0, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "locus") == "1.0000");
    assert(get(api, instance, "midi_map").find("53 clear_lane") != std::string::npos);
    assert(get(api, instance, "midi_map").find("54 tempo_bpm") != std::string::npos);
    assert(get(api, instance, "midi_map").find("56 clip_playing") != std::string::npos);
    assert(get(api, instance, "midi_map").find("58 chain_clear") != std::string::npos);
    api->set_param(instance, "clip_playing", "0");
    api->set_param(instance, "state", defaultState.c_str());
    api->set_param(instance, "monitor_level", "0");
    assert(get(api, instance, "preview_level") == "0.7000");

    /* Input monitoring is genuinely off by default. */
    fillInput(12000);
    render(api, instance, 1);
    assert(peak() == 0);

    /* Record lane 1 from Move's selected input and build its waveform cache. */
    recordLane(api, instance, 1, 10000, 12);
    std::string ui = get(api, instance, "ui_state");
    assert((stateField(ui, 1) & 0x1) != 0);
    const std::string waves = get(api, instance, "waveforms");
    assert(waves.size() == 4u * 112u * 2u + 3u);
    assert(waves.substr(0u, 224u).find_first_not_of('7') != std::string::npos);
    api->set_param(instance, "maintenance", "1");
    const std::string firstCapture = get(api, instance, "lane_source_path");
    assert(firstCapture == persistenceDir
        + "/captures/t0000000001-c01-l1-input.wav");
    struct stat captureStatus {};
    assert(stat(firstCapture.c_str(), &captureStatus) == 0);
    assert(get(api, instance, "lane_source_state").rfind(
        "CAPTURE:TAKE 0000000001", 0u)
        == 0u);

    /* Rerecording archives a new WAV rather than replacing the earlier take.
       Shift+Back's generic lane undo can therefore restore actual old audio. */
    recordLane(api, instance, 1, 2000, 12);
    api->set_param(instance, "maintenance", "1");
    const std::string secondCapture = get(api, instance, "lane_source_path");
    assert(secondCapture == persistenceDir
        + "/captures/t0000000002-c01-l1-input.wav");
    assert(secondCapture != firstCapture);
    assert(stat(firstCapture.c_str(), &captureStatus) == 0);
    assert(stat(secondCapture.c_str(), &captureStatus) == 0);
    api->set_param(instance, "undo", "1");
    for (int attempt = 0; attempt < 500
         && get(api, instance, "lane_source_path") != firstCapture; ++attempt) {
        render(api, instance, 1);
        usleep(1000);
    }
    assert(get(api, instance, "lane_source_path") == firstCapture);

    api->set_param(instance, "sample_trim_start", "0.10");
    api->set_param(instance, "sample_trim_end", "0.90");
    api->set_param(instance, "sample_trim_apply", "1");
    assert(get(api, instance, "lane_source") == "TRIMMED");
    const std::string trimCapture = get(api, instance, "lane_source_path");
    api->set_param(instance, "sample_reverse", "1");
    const std::string reverseCapture = get(api, instance, "lane_source_path");
    midi(api, instance, 0x90, 60, 127);
    render(api, instance, 4);
    const int peakBeforeNormalize = peak();
    midi(api, instance, 0x80, 60, 0);
    api->set_param(instance, "sample_normalize", "1");
    assert(get(api, instance, "last_error").empty());
    const std::string normalizeCapture = get(api, instance, "lane_source_path");
    midi(api, instance, 0x90, 60, 127);
    render(api, instance, 4);
    const int peakAfterNormalize = peak();
    midi(api, instance, 0x80, 60, 0);
    assert(peakBeforeNormalize > 0);
    if (peakAfterNormalize <= peakBeforeNormalize) {
        std::fprintf(stderr,
            "Normalize did not raise mastered output: before=%d after=%d\n",
            peakBeforeNormalize, peakAfterNormalize);
    }
    assert(peakAfterNormalize > peakBeforeNormalize);
    assert(peakAfterNormalize
        <= static_cast<int>(kOutputCeiling * 32767.0f) + 1);
    api->set_param(instance, "performance_reset", "1");
    // The mastering stage can legitimately engage the safety limiter on a
    // freshly normalized transient. Let its release finish before checking
    // the reset idle reading.
    render(api, instance, 600);
    assert(std::strtof(get(api, instance, "limiter_state").c_str(), nullptr)
        < 0.01f);
    api->set_param(instance, "sample_fade", "1");
    const std::string fadeCapture = get(api, instance, "lane_source_path");
    api->set_param(instance, "maintenance", "1");
    assert(get(api, instance, "lane_source_state").rfind("CAPTURE:FADED", 0u)
        == 0u);
    assert(trimCapture.find("-trim.wav") != std::string::npos);
    assert(reverseCapture.find("-reverse.wav") != std::string::npos);
    assert(normalizeCapture.find("-normalize.wav") != std::string::npos);
    assert(fadeCapture.find("-fade.wav") != std::string::npos);
    assert(trimCapture != reverseCapture && reverseCapture != normalizeCapture
        && normalizeCapture != fadeCapture);
    for (const std::string& path : {
             firstCapture, secondCapture, trimCapture, reverseCapture,
             normalizeCapture, fadeCapture }) {
        assert(stat(path.c_str(), &captureStatus) == 0);
    }
    api->set_param(instance, "undo", "1");
    for (int attempt = 0; attempt < 500
         && get(api, instance, "lane_source") != "NORMALIZED"; ++attempt) {
        render(api, instance, 1);
        usleep(1000);
    }
    assert(get(api, instance, "lane_source") == "NORMALIZED");

    /* A lane can alternatively come from the on-device sample library. WAV
       parsing/resampling runs on a worker; render only performs bounded copies. */
    const std::string librarySample = libraryDir + "/Library Tone.wav";
    const std::string convertedLibrarySample =
        libraryDir + "/Library Tone 48k.wav";
    writePcm16Wav(librarySample);
    writePcm16Wav(convertedLibrarySample, 48000u);
    api->set_param(instance, "record_lane", "2");
    api->set_param(instance, "preview_file", convertedLibrarySample.c_str());
    int convertedPreviewState = 0;
    for (int attempt = 0; attempt < 500 && convertedPreviewState != 8
         && convertedPreviewState != 7; ++attempt) {
        render(api, instance, 1);
        usleep(1000);
        convertedPreviewState = stateField(
            get(api, instance, "sample_load_state"), 0);
    }
    assert(convertedPreviewState == 8);
    api->set_param(instance, "preview_stop", "1");
    api->set_param(instance, "preview_file", librarySample.c_str());
    int previewState = 0;
    for (int attempt = 0; attempt < 500 && previewState != 8
         && previewState != 7; ++attempt) {
        render(api, instance, 1);
        usleep(1000);
        previewState = stateField(get(api, instance, "sample_load_state"), 0);
    }
    assert(previewState == 8);
    int previewPeak = 0;
    for (int block = 0; block < 4; ++block) {
        render(api, instance, 1);
        previewPeak = std::max(previewPeak, peak());
    }
    assert(previewPeak > 0);
    const std::string loadCommand = "2," + librarySample;
    api->set_param(instance, "load_lane_file", loadCommand.c_str());
    int loadState = 0;
    for (int attempt = 0; attempt < 500 && loadState != 6 && loadState != 7;
         ++attempt) {
        render(api, instance, 1);
        usleep(1000);
        loadState = stateField(get(api, instance, "sample_load_state"), 0);
    }
    if (loadState != 6) {
        std::fprintf(stderr, "Sample load failed: %s\n",
                     get(api, instance, "sample_load_state").c_str());
    }
    assert(loadState == 6);
    assert(get(api, instance, "lane_source") == "Library Tone.wav");
    assert(stateField(get(api, instance, "region_state"), 0) > 1);
    assert((stateField(get(api, instance, "ui_state"), 1) & 0x2) != 0);
    assert(stateField(get(api, instance, "ui_state"), 13) == 0);
    const std::string libraryWaves = get(api, instance, "waveforms");
    const std::string libraryLaneWave = libraryWaves.substr(225u, 224u);
    assert(libraryLaneWave.find('e') != std::string::npos);
    api->set_param(instance, "clip_copy", "2");
    api->set_param(instance, "clear_lane", "Clear");
    render(api, instance, 8);

    /* Record free-time performance data rather than programming Steps. The
       clip stores independent notes, pressure, and knob gestures, then loops
       immediately with eight-voice playback. */
    assert(stateField(get(api, instance, "clip_state"), 4) == 0);
    api->set_param(instance, "clip_record_mode", "Replace");
    /* Notes already held when recording begins must be captured at t=0. */
    midi(api, instance, 0x90, 73, 108, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 16);
    api->set_param(instance, "clip_record", "1");
    assert(stateField(get(api, instance, "clip_state"), 0) == 1);
    render(api, instance, 80);
    midi(api, instance, 0xA0, 73, 32, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "field", "0.25");
    api->set_param(instance, "clip_automation", "field");
    api->set_param(instance, "amp_attack", "8");
    api->set_param(instance, "clip_automation", "amp_attack");
    render(api, instance, 40);
    midi(api, instance, 0x90, 76, 96, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 80);
    midi(api, instance, 0x80, 73, 0, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 40);
    midi(api, instance, 0x80, 76, 0, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "clip_record", "1");
    std::string clipState = get(api, instance, "clip_state");
    assert(stateField(clipState, 0) == 0);
    assert(stateField(clipState, 1) == 1);
    assert(stateField(clipState, 4) >= 150);
    assert(stateField(clipState, 5) == 2);
    assert(stateField(clipState, 6) >= 3);
    assert(stateField(clipState, 10) == 12000);
    assert(stateField(clipState, 11) > 0);
    assert(stateField(get(api, instance, "clip_take_state"), 0) == 1);
    assert(stateField(get(api, instance, "clip_take_state"), 1) == 0);
    render(api, instance, 4);
    assert(peak() > 0);
    const std::string loopVoicePads = get(api, instance, "voice_pads");
    bool loopHasPad5 = false;
    for (int voice = 0; voice < 8; ++voice) {
        loopHasPad5 = loopHasPad5 || stateField(loopVoicePads, voice) == 5;
    }
    assert(loopHasPad5);

    /* Free clips retain wall-clock microtiming. Beat Sync scales that exact
       timing from the BPM captured with the performance, while Play can wait
       for the next beat or bar without reporting a false running state. */
    api->set_param(instance, "launch_quantize", "Off");
    api->set_param(instance, "clip_timing", "Free");
    api->set_param(instance, "tempo_bpm", "120");
    api->set_param(instance, "clip_playing", "0");
    api->set_param(instance, "clip_playing", "1");
    render(api, instance, 20);
    const int freePosition = stateField(get(api, instance, "clip_state"), 3);
    api->set_param(instance, "clip_playing", "0");
    api->set_param(instance, "clip_timing", "Beat Sync");
    api->set_param(instance, "tempo_bpm", "240");
    api->set_param(instance, "clip_playing", "1");
    render(api, instance, 20);
    const int syncPosition = stateField(get(api, instance, "clip_state"), 3);
    assert(syncPosition > freePosition * 3 / 2);
    api->set_param(instance, "clip_playing", "0");
    api->set_param(instance, "launch_quantize", "Beat");
    api->set_param(instance, "clip_playing", "1");
    assert(stateField(get(api, instance, "clip_state"), 1) == 0);
    assert(stateField(get(api, instance, "clip_state"), 8) == 1);
    for (int block = 0; block < 400
         && stateField(get(api, instance, "clip_state"), 8) != 0; ++block) {
        render(api, instance, 1);
    }
    assert(stateField(get(api, instance, "clip_state"), 8) == 0);
    assert(stateField(get(api, instance, "clip_state"), 1) == 1);
    api->set_param(instance, "launch_quantize", "Off");
    api->set_param(instance, "clip_timing", "Free");
    api->set_param(instance, "tempo_bpm", "120");

    /* Loop time is continuously variable. Overdub retains the original
       duration and adds new events against the running phase. */
    const int originalDuration = stateField(clipState, 4);
    api->set_param(instance, "clip_rate", "4");
    assert(get(api, instance, "clip_rate") == "4.0000");
    const int positionBefore = stateField(get(api, instance, "clip_state"), 3);
    render(api, instance, 20);
    const int positionAfter = stateField(get(api, instance, "clip_state"), 3);
    assert(positionAfter != positionBefore);
    api->set_param(instance, "clip_record_mode", "Overdub");
    api->set_param(instance, "clip_record", "1");
    midi(api, instance, 0x90, 70, 88, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 24);
    midi(api, instance, 0x80, 70, 0, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "clip_record", "1");
    clipState = get(api, instance, "clip_state");
    assert(stateField(clipState, 4) == originalDuration);
    assert(stateField(clipState, 5) == 3);
    assert(stateField(get(api, instance, "clip_take_state"), 0) == 2);
    assert(stateField(get(api, instance, "clip_take_state"), 1) == 1);

    /* Clearing only the performance layer is undoable and does not remove
       the clip's sample ownership. */
    api->set_param(instance, "clip_clear", "1");
    assert(stateField(get(api, instance, "clip_state"), 4) == 0);
    assert(get(api, instance, "undo_available") == "1");
    api->set_param(instance, "undo", "1");
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);
    assert(stateField(get(api, instance, "clip_take_state"), 0) == 2);

    /* Every completed overdub is a separate note/gesture take. A second
       pass survives copy and persistence independently from the first. */
    const int pointsBeforeSecondTake = stateField(
        get(api, instance, "clip_state"), 6);
    api->set_param(instance, "clip_record", "1");
    midi(api, instance, 0x90, 71, 92, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "field", "0.75");
    api->set_param(instance, "clip_automation", "field");
    render(api, instance, 20);
    midi(api, instance, 0x80, 71, 0, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "clip_record", "1");
    assert(stateField(get(api, instance, "clip_state"), 5) == 4);
    assert(stateField(get(api, instance, "clip_state"), 6)
        == pointsBeforeSecondTake + 1);
    assert(stateField(get(api, instance, "clip_take_state"), 0) == 3);

    /* Chains are a persistent ordered list with independent repeat counts. */
    api->set_param(instance, "chain_slot", "1");
    api->set_param(instance, "chain_clip", "2");
    api->set_param(instance, "chain_repeats", "2");
    api->set_param(instance, "chain_slot", "2");
    api->set_param(instance, "chain_clip", "3");
    api->set_param(instance, "chain_repeats", "1");
    const std::string chainEntries = get(api, instance, "chain_entries");
    assert(chainEntries.rfind("1:1:2,1:2:1", 0u) == 0u);
    api->set_param(instance, "chain_insert", "1,4");
    assert(get(api, instance, "chain_entries").rfind(
        "1:1:2,1:4:1,1:2:1", 0u) == 0u);
    api->set_param(instance, "chain_move", "1,2");
    assert(get(api, instance, "chain_entries").rfind(
        "1:1:2,1:2:1,1:4:1", 0u) == 0u);
    api->set_param(instance, "chain_delete", "2");
    assert(get(api, instance, "chain_entries").rfind(
        "1:1:2,1:2:1,0:0:1", 0u) == 0u);
    api->set_param(instance, "chain_slot", "2");
    midi(api, instance, 0xB0, 58, 127, MOVE_MIDI_SOURCE_EXTERNAL);
    assert(get(api, instance, "chain_entries").rfind("1:1:2,0:0:1", 0u)
        == 0u);
    api->set_param(instance, "chain_clip", "3");
    api->set_param(instance, "chain_repeats", "1");
    api->set_param(instance, "chain_enabled", "1");
    assert(stateField(get(api, instance, "chain_state"), 0) == 1);
    api->set_param(instance, "chain_enabled", "0");

    /* Four banks of sixteen performance clips snapshot event data, source
       assignments, and engine settings. Running changes queue at a loop seam. */
    api->set_param(instance, "clip_copy", "1");
    api->set_param(instance, "clip_take_undo", "1");
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);
    assert(stateField(get(api, instance, "clip_state"), 6)
        == pointsBeforeSecondTake);
    assert(stateField(get(api, instance, "clip_take_state"), 0) == 2);
    assert((unsignedStateField(get(api, instance, "clip_bank_state"), 2) & 0x7u)
        == 0x7u);
    api->set_param(instance, "clip_select", "1");
    waitForClip(api, instance, 1);
    api->set_param(instance, "algorithm", "Lanes");
    api->set_param(instance, "clip_select", "0");
    waitForClip(api, instance, 0);
    assert(get(api, instance, "algorithm") == "0");
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);
    api->set_param(instance, "clip_playing", "1");
    api->set_param(instance, "clip_select", "1");
    assert(stateField(get(api, instance, "clip_bank_state"), 1) == 1);
    for (int block = 0; block < 240
         && stateField(get(api, instance, "clip_bank_state"), 0) != 1; ++block) {
        render(api, instance, 1);
    }
    assert(stateField(get(api, instance, "clip_bank_state"), 0) == 1);
    assert(get(api, instance, "algorithm") == "1");
    api->set_param(instance, "clip_playing", "0");
    api->set_param(instance, "clip_select", "0");
    waitForClip(api, instance, 0);

    /* Mirror and Zigzag must expose their actual bidirectional read-head
       motion to the UI. Ignore only the large discontinuity of a wrap when
       detecting direction changes. */
    const auto cursorMovesBothWays = [&](const char* mode, const char* sound) {
        api->set_param(instance, "all_notes_off", "1");
        render(api, instance, 160);
        api->set_param(instance, "algorithm", "Motion");
        api->set_param(instance, "motion_sound", sound);
        api->set_param(instance, "motion_mode", mode);
        api->set_param(instance, "speed", "1");
        midi(api, instance, 0x90, 60, 100);
        render(api, instance, 16);
        int previous = stateField(get(api, instance, "playheads"), 0);
        bool forward = false;
        bool backward = false;
        for (int block = 0; block < 40; ++block) {
            render(api, instance, 1);
            const int current = stateField(get(api, instance, "playheads"), 0);
            const int delta = current - previous;
            if (delta > 5 && delta < 300) forward = true;
            if (delta < -5 && delta > -300) backward = true;
            previous = current;
        }
        midi(api, instance, 0x80, 60, 0);
        return forward && backward;
    };
    constexpr const char* motionSounds[] {
        "Continuous", "Packets", "Motor"
    };
    for (const char* sound : motionSounds) {
        const bool mirrorMoves = cursorMovesBothWays("Mirror", sound);
        const bool zigzagMoves = cursorMovesBothWays("Zigzag", sound);
        if (!mirrorMoves || !zigzagMoves) {
            std::fprintf(stderr, "Motion cursor failure: %s mirror=%d zigzag=%d\n",
                         sound, mirrorMoves ? 1 : 0, zigzagMoves ? 1 : 0);
        }
        assert(mirrorMoves);
        assert(zigzagMoves);
    }
    api->set_param(instance, "segment_trigger", "Clock");
    api->set_param(instance, "segment_overlap", "Layer");
    api->set_param(instance, "segment_layers", "4");
    api->set_param(instance, "segment_rate", "20");
    api->set_param(instance, "segment_repeats", "4");
    constexpr const char* segmentSounds[] {
        "Bounce", "Freeze", "Iterate", "Pulser", "Doublets"
    };
    for (const char* sound : segmentSounds) {
        api->set_param(instance, "all_notes_off", "1");
        api->set_param(instance, "motion_sound", sound);
        midi(api, instance, 0x90, 60, 100);
        render(api, instance, 40);
        assert(peak() > 0);
        midi(api, instance, 0x80, 60, 0);
    }
    api->set_param(instance, "motion_sound", "Continuous");
    api->set_param(instance, "motion_mode", "Forward");
    render(api, instance, 160);

    /* Held notes have independent engines, moving stack offsets, and visible
       playheads. Releasing every voice removes every playhead. */
    api->set_param(instance, "lane_cycles", "8");
    midi(api, instance, 0x90, 60, 32);
    render(api, instance, 2);
    midi(api, instance, 0x90, 67, 110);
    for (int note = 61; note <= 66; ++note) {
        render(api, instance, 1);
        midi(api, instance, 0x90, static_cast<std::uint8_t>(note),
             static_cast<std::uint8_t>(40 + note));
    }
    render(api, instance, 8);
    const std::string eightPlayheads = get(api, instance, "playheads");
    for (int voice = 0; voice < 8; ++voice) {
        assert(stateField(eightPlayheads, voice) >= 0);
    }
    assert(stateField(eightPlayheads, 0) != stateField(eightPlayheads, 7));
    const std::string lanesBefore = get(api, instance, "voice_lanes");
    render(api, instance, 40);
    const std::string lanesAfter = get(api, instance, "voice_lanes");
    assert(stateField(lanesBefore, 0) != stateField(lanesAfter, 0));
    assert(stateField(lanesBefore, 1) != stateField(lanesAfter, 1));
    api->set_param(instance, "stack_scan_paused", "1");
    assert(get(api, instance, "stack_scan_paused") == "1");
    render(api, instance, 2);
    const std::string heldLanes = get(api, instance, "voice_lanes");
    midi(api, instance, 0xA0, 60, 120);
    render(api, instance, 40);
    const std::string heldLanesAfter = get(api, instance, "voice_lanes");
    assert(stateField(heldLanes, 0) == stateField(heldLanesAfter, 0));
    assert(stateField(heldLanes, 1) == stateField(heldLanesAfter, 1));
    api->set_param(instance, "stack_scan_paused", "0");
    assert(get(api, instance, "stack_scan_paused") == "0");
    render(api, instance, 40);
    const std::string resumedLanes = get(api, instance, "voice_lanes");
    assert(stateField(heldLanesAfter, 0) != stateField(resumedLanes, 0));
    assert(stateField(heldLanesAfter, 1) != stateField(resumedLanes, 1));
    render(api, instance, 4);
    midi(api, instance, 0x80, 60, 0);
    midi(api, instance, 0x80, 67, 0);
    for (int note = 61; note <= 66; ++note) {
        midi(api, instance, 0x80, static_cast<std::uint8_t>(note), 0);
    }
    assert(get(api, instance, "playheads") == noPlayheads);
    render(api, instance, 160);
    assert(get(api, instance, "playheads") == noPlayheads);

    const auto laneForPad = [&](int pad) {
        const std::string pads = get(api, instance, "voice_pads");
        const std::string lanes = get(api, instance, "voice_lanes");
        for (int voice = 0; voice < 8; ++voice) {
            if (stateField(pads, voice) == pad) {
                return stateField(lanes, voice);
            }
        }
        return -1;
    };
    /* A live Stack Hold belongs to the key, not the release-tail voice.
       Releasing and retriggering the same pad while held restores its exact
       frozen crossfade; Run clears that retained position. */
    api->set_param(instance, "algorithm", "Lanes");
    api->set_param(instance, "lane_path", "Down");
    api->set_param(instance, "lane_cycles", "4");
    midi(api, instance, 0x90, 68, 100, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 24);
    api->set_param(instance, "stack_scan_paused", "1");
    render(api, instance, 4);
    const int liveHeldLane = laneForPad(0);
    assert(liveHeldLane >= 0);
    midi(api, instance, 0x80, 68, 0, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 160);
    assert(laneForPad(0) == -1);
    midi(api, instance, 0x90, 68, 100, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 4);
    const int retriggeredHeldLane = laneForPad(0);
    assert(retriggeredHeldLane >= 0);
    assert(std::abs(retriggeredHeldLane - liveHeldLane) <= 2);
    render(api, instance, 40);
    assert(std::abs(laneForPad(0) - liveHeldLane) <= 2);
    api->set_param(instance, "stack_scan_paused", "0");
    render(api, instance, 40);
    assert(std::abs(laneForPad(0) - liveHeldLane) > 2);
    midi(api, instance, 0x80, 68, 0, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 160);

    /* Performance recording owns Stack Hold as data. If Record begins while
       a pad is frozen between lanes, loop playback restores that voice's
       exact stack phase after the physical hold is released. */
    api->set_param(instance, "clip_select", "62");
    waitForClip(api, instance, 62);
    api->set_param(instance, "algorithm", "Lanes");
    api->set_param(instance, "lane_path", "Down");
    api->set_param(instance, "lane_cycles", "4");
    midi(api, instance, 0x90, 68, 100, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 24);
    api->set_param(instance, "stack_scan_paused", "1");
    render(api, instance, 8);
    const int recordedHeldLane = laneForPad(0);
    assert(recordedHeldLane >= 0);
    api->set_param(instance, "clip_record_mode", "Replace");
    api->set_param(instance, "clip_record", "1");
    render(api, instance, 40);
    midi(api, instance, 0x80, 68, 0, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 4);
    api->set_param(instance, "clip_record", "1");
    assert(stateField(get(api, instance, "clip_state"), 6) >= 3);
    api->set_param(instance, "stack_scan_paused", "0");
    render(api, instance, 16);
    assert(get(api, instance, "clip_stack_hold") == "1");
    const int recalledHeldLane = laneForPad(0);
    assert(recalledHeldLane >= 0);
    assert(std::abs(recalledHeldLane - recordedHeldLane) <= 2);
    render(api, instance, 12);
    assert(std::abs(laneForPad(0) - recalledHeldLane) <= 1);
    api->set_param(instance, "clip_playing", "0");
    api->set_param(instance, "clip_select", "0");
    waitForClip(api, instance, 0);
    render(api, instance, 160);
    api->set_param(instance, "lane_cycles", "1");

    /* Overtake receives raw hardware pad IDs. Map the 8x4 surface to the
       isomorphic note grid, and remember the mapped note until release even
       if the octave changes in between. */
    api->set_param(instance, "all_notes_off", "1");
    midi(api, instance, 0x90, 16, 127, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "-1");
    midi(api, instance, 0x90, 68, 100, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "48");
    api->set_param(instance, "pad_octave", "1");
    midi(api, instance, 0x80, 68, 0, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "-1");
    midi(api, instance, 0x90, 68, 100, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "60");
    midi(api, instance, 0xA0, 68, 64, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "performance_lane") == "2.5118");
    midi(api, instance, 0x80, 68, 0, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "pad_octave", "0");
    midi(api, instance, 0x90, 73, 100, MOVE_MIDI_SOURCE_INTERNAL);
    midi(api, instance, 0x90, 76, 100, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "53");
    const std::string duplicatePitchPads = get(api, instance, "voice_pads");
    bool hasPad5 = false;
    bool hasPad8 = false;
    for (int voice = 0; voice < 8; ++voice) {
        hasPad5 = hasPad5 || stateField(duplicatePitchPads, voice) == 5;
        hasPad8 = hasPad8 || stateField(duplicatePitchPads, voice) == 8;
    }
    assert(hasPad5 && hasPad8);
    midi(api, instance, 0x80, 76, 0, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "53");
    midi(api, instance, 0x80, 73, 0, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "-1");

    /* Named scales fold Keys mode to scale degrees while preserving the
       legacy chromatic grid as the default. Root is both the scale tonic and
       the sample's unison note; external MIDI remains unquantized. */
    assert(get(api, instance, "scale") == "0");
    api->set_param(instance, "scale", "Major");
    assert(get(api, instance, "scale") == "1");
    midi(api, instance, 0x90, 68, 100, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "48");
    midi(api, instance, 0x80, 68, 0, MOVE_MIDI_SOURCE_INTERNAL);
    midi(api, instance, 0x90, 69, 100, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "50");
    midi(api, instance, 0x80, 69, 0, MOVE_MIDI_SOURCE_INTERNAL);
    midi(api, instance, 0x90, 75, 100, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "60");
    midi(api, instance, 0x80, 75, 0, MOVE_MIDI_SOURCE_INTERNAL);
    midi(api, instance, 0x90, 76, 100, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "53");
    midi(api, instance, 0x80, 76, 0, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "scale", "Hungarian Minor");
    assert(get(api, instance, "scale") == "24");
    midi(api, instance, 0x90, 70, 100, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "51");
    midi(api, instance, 0x80, 70, 0, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "scale", "Major Pentatonic");
    midi(api, instance, 0x90, 99, 100, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "86");
    midi(api, instance, 0x80, 99, 0, MOVE_MIDI_SOURCE_INTERNAL);
    midi(api, instance, 0x90, 61, 100, MOVE_MIDI_SOURCE_EXTERNAL);
    assert(get(api, instance, "active_note") == "61");
    midi(api, instance, 0x80, 61, 0, MOVE_MIDI_SOURCE_EXTERNAL);
    api->set_param(instance, "scale", "Chromatic");

    /* Rolls capture the combined stereo history of every active playhead
       into one division-sized fragment. The grouped Roll does not steal any
       source voice, remains audible after those source notes end, and uses
       the normal Shift+pad latch path. */
    api->set_param(instance, "algorithm", "Sample");
    api->set_param(instance, "sample_mode", "Loop");
    api->set_param(instance, "locus", "0.4");
    api->set_param(instance, "field", "0.2");
    api->set_param(instance, "speed", "0.05");
    api->set_param(instance, "tempo_bpm", "240");
    api->set_param(instance, "cut_time_variation", "1");
    api->set_param(instance, "cut_reverse", "100");
    api->set_param(instance, "cut_pitch_variation", "24");
    api->set_param(instance, "pad_mode", "Rolls");
    midi(api, instance, 0x90, 60, 100, MOVE_MIDI_SOURCE_EXTERNAL);
    midi(api, instance, 0x90, 67, 100, MOVE_MIDI_SOURCE_EXTERNAL);
    render(api, instance, 20);
    const std::string sourcePlayheads = get(api, instance, "playheads");
    int sourcePlayheadCount = 0;
    for (int voice = 0; voice < 8; ++voice) {
        if (stateField(sourcePlayheads, voice) >= 0) ++sourcePlayheadCount;
    }
    assert(sourcePlayheadCount == 2);
    api->set_param(instance, "shift_held", "1");
    midi(api, instance, 0x90, 73, 100, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "shift_held", "0");
    midi(api, instance, 0x80, 73, 0, MOVE_MIDI_SOURCE_INTERNAL);
    const std::string groupedRoll = get(api, instance, "roll_state");
    assert(stateField(groupedRoll, 0) == 1);
    assert(stateField(groupedRoll, 1) == 2);
    assert(stateField(groupedRoll, 2) > 0);
    assert(stateField(get(api, instance, "roll_pads"), 0) == 5);
    const std::string playheadsDuringRoll = get(api, instance, "playheads");
    int playheadsDuringRollCount = 0;
    for (int voice = 0; voice < 8; ++voice) {
        if (stateField(playheadsDuringRoll, voice) >= 0) {
            ++playheadsDuringRollCount;
        }
    }
    assert(playheadsDuringRollCount == 2);
    midi(api, instance, 0x80, 60, 0, MOVE_MIDI_SOURCE_EXTERNAL);
    midi(api, instance, 0x80, 67, 0, MOVE_MIDI_SOURCE_EXTERNAL);
    render(api, instance, 200);
    assert(peak() > 0);
    assert(stateField(get(api, instance, "roll_state"), 0) == 1);
    assert(get(api, instance, "active_note") == "60");
    api->set_param(instance, "shift_held", "1");
    midi(api, instance, 0x90, 73, 100, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "shift_held", "0");
    midi(api, instance, 0x80, 73, 0, MOVE_MIDI_SOURCE_INTERNAL);
    assert(stateField(get(api, instance, "roll_state"), 0) == 0);
    assert(get(api, instance, "active_note") == "-1");

    /* Performance capture stores the Roll anchor and lane blend alongside
       the note, so loop playback and persistence can reconstruct the same
       stutter rather than a plain Root note. */
    api->set_param(instance, "clip_select", "60");
    waitForClip(api, instance, 60);
    api->set_param(instance, "clip_record_mode", "Replace");
    api->set_param(instance, "clip_record", "1");
    midi(api, instance, 0x90, 74, 100, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 20);
    midi(api, instance, 0x80, 74, 0, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 40);
    api->set_param(instance, "clip_record", "1");
    const std::string rollClipState = get(api, instance, "clip_state");
    assert(stateField(rollClipState, 5) == 1);
    assert(stateField(rollClipState, 6) >= 3);
    api->set_param(instance, "clip_playing", "0");
    api->set_param(instance, "clip_select", "0");
    waitForClip(api, instance, 0);
    api->set_param(instance, "pad_mode", "Keys");
    api->set_param(instance, "algorithm", "Lanes");
    api->set_param(instance, "locus", "0.5");
    api->set_param(instance, "field", "1");
    api->set_param(instance, "speed", "1");
    api->set_param(instance, "tempo_bpm", "120");

    /* Shift+pad latches each hardware pad independently. Releasing the pad
       does not end its voice; a second Shift+pad gesture does. HOLD OFF at
       zero is manual, while a nonzero global delay schedules every existing
       latch and applies to new ones. */
    api->set_param(instance, "shift_held", "1");
    midi(api, instance, 0x90, 68, 100, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "shift_held", "0");
    midi(api, instance, 0x80, 68, 0, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "48");
    const std::string latchPads = get(api, instance, "voice_pads");
    const std::string latchNotes = get(api, instance, "voice_notes");
    bool foundLatchOwner = false;
    for (int voice = 0; voice < 8; ++voice) {
        if (stateField(latchPads, voice) != 0) continue;
        assert(stateField(latchNotes, voice) == 48);
        foundLatchOwner = true;
    }
    assert(foundLatchOwner);
    api->set_param(instance, "pad_octave", "1");
    assert(get(api, instance, "active_note") == "48");
    api->set_param(instance, "shift_held", "1");
    midi(api, instance, 0x90, 68, 100, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "shift_held", "0");
    midi(api, instance, 0x80, 68, 0, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "-1");
    api->set_param(instance, "pad_octave", "0");
    api->set_param(instance, "shift_held", "1");
    midi(api, instance, 0x90, 68, 100, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "shift_held", "0");
    midi(api, instance, 0x80, 68, 0, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "48");
    api->set_param(instance, "pad_mode", "Rolls");
    assert(get(api, instance, "active_note") == "48");
    api->set_param(instance, "pad_mode", "Keys");
    assert(get(api, instance, "active_note") == "48");
    api->set_param(instance, "shift_held", "1");
    midi(api, instance, 0x90, 69, 100, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "shift_held", "0");
    midi(api, instance, 0x80, 69, 0, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "49");
    api->set_param(instance, "shift_held", "1");
    midi(api, instance, 0x90, 69, 100, MOVE_MIDI_SOURCE_INTERNAL);
    api->set_param(instance, "shift_held", "0");
    midi(api, instance, 0x80, 69, 0, MOVE_MIDI_SOURCE_INTERNAL);
    assert(get(api, instance, "active_note") == "48");
    assert(get(api, instance, "hold_release_ms") == "0");
    render(api, instance, 8);
    assert(get(api, instance, "active_note") == "48");
    api->set_param(instance, "hold_release_ms", "10");
    render(api, instance, 5);
    assert(get(api, instance, "active_note") == "-1");
    api->set_param(instance, "all_notes_off", "1");
    render(api, instance, 160);
    api->set_param(instance, "hold_release_ms", "0");

    /* The ninth latch steals the oldest of the eight voices and must also
       clear that pad's latch/LED ownership. */
    api->set_param(instance, "shift_held", "1");
    for (int pad = 0; pad < 9; ++pad) {
        midi(api, instance, 0x90, static_cast<std::uint8_t>(68 + pad), 100,
             MOVE_MIDI_SOURCE_INTERNAL);
        midi(api, instance, 0x80, static_cast<std::uint8_t>(68 + pad), 0,
             MOVE_MIDI_SOURCE_INTERNAL);
    }
    api->set_param(instance, "shift_held", "0");
    const std::string stolenLatchPads = get(api, instance, "voice_pads");
    bool stillHasOldestPad = false;
    bool hasNewestPad = false;
    for (int voice = 0; voice < 8; ++voice) {
        stillHasOldestPad = stillHasOldestPad
            || stateField(stolenLatchPads, voice) == 0;
        hasNewestPad = hasNewestPad || stateField(stolenLatchPads, voice) == 8;
    }
    assert(!stillHasOldestPad && hasNewestPad);
    api->set_param(instance, "all_notes_off", "1");
    render(api, instance, 160);

    api->set_param(instance, "performance_lane", "2.5");
    assert(get(api, instance, "performance_lane") == "2.5000");

    /* Velocity chooses performance depth, never sample amplitude. With one
       populated lane, low and high strikes must converge on the same level.
       Start both measurements from the program compressor's released state. */
    render(api, instance, 600);
    midi(api, instance, 0x90, 60, 1);
    render(api, instance, 8);
    const int lowVelocityPeak = peak();
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 600);
    midi(api, instance, 0x90, 60, 127);
    render(api, instance, 8);
    const int highVelocityPeak = peak();
    assert(lowVelocityPeak > 8000);
    assert(std::abs(lowVelocityPeak - highVelocityPeak) <= 2);

    /* Aftertouch can move stack depth without altering the gain model. */
    midi(api, instance, 0xA0, 60, 32);
    render(api, instance, 2);
    assert(peak() > 8000);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 160);

    /* Every source voice has an independent ADSR after its playback engine.
       Time controls remain precise at the short end, sustain is a true held
       level, and release keeps a voice audible after note-off. Velocity is
       deliberately absent from this gain stage. */
    api->set_param(instance, "algorithm", "Sample");
    api->set_param(instance, "sample_mode", "Loop");
    api->set_param(instance, "locus", "0.4");
    api->set_param(instance, "field", "0.2");
    api->set_param(instance, "speed", "0.05");
    api->set_param(instance, "amp_attack", "0");
    api->set_param(instance, "amp_decay", "0");
    api->set_param(instance, "amp_sustain", "100");
    api->set_param(instance, "amp_release", "0");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 8);
    const int fullEnvelopePeak = peak();
    assert(fullEnvelopePeak > 8000);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 2);

    api->set_param(instance, "amp_attack", "1000");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 1);
    const int attackStartPeak = peak();
    assert(attackStartPeak > 0 && attackStartPeak < fullEnvelopePeak / 4);
    render(api, instance, 400);
    assert(peak() > fullEnvelopePeak * 3 / 4);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 2);

    api->set_param(instance, "amp_attack", "0");
    api->set_param(instance, "amp_decay", "200");
    api->set_param(instance, "amp_sustain", "25");
    midi(api, instance, 0x90, 60, 100);
    // Allow the 1.5 ms output lookahead and the engine's retrigger fade to
    // expose the full start level before measuring the decay.
    render(api, instance, 2);
    const int decayStartPeak = peak();
    render(api, instance, 100);
    const int sustainPeak = peak();
    assert(decayStartPeak > sustainPeak * 2);
    assert(sustainPeak > fullEnvelopePeak / 6
        && sustainPeak < fullEnvelopePeak / 3);
    api->set_param(instance, "amp_release", "250");
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 1);
    assert(peak() > 0);
    render(api, instance, 100);
    assert(peak() == 0);
    api->set_param(instance, "amp_attack", "3");
    api->set_param(instance, "amp_decay", "0");
    api->set_param(instance, "amp_sustain", "100");
    api->set_param(instance, "amp_release", "25");

    /* Library-lane assignments belong to clip slots. Clip 3 recalls its
       source, while A01's explicit EMPTY ownership remains silent. */
    api->set_param(instance, "clip_select", "2");
    waitForClip(api, instance, 2);
    assert(get(api, instance, "lane_source") == "Library Tone.wav");
    assert(stateField(get(api, instance, "ui_state"), 13) == 0);
    assert(get(api, instance, "waveforms").substr(225u, 224u)
        .find('e') != std::string::npos);
    api->set_param(instance, "clip_select", "0");
    waitForClip(api, instance, 0);
    assert(get(api, instance, "lane_source") == "EMPTY");

    /* Event storage is bounded and observable. Dense duplicate gestures are
       collapsed to a latest endpoint, while note overflow stops cleanly and
       leaves the recorded clip playable instead of allocating at runtime. */
    api->set_param(instance, "clip_playing", "0");
    api->set_param(instance, "clip_select", "63");
    waitForClip(api, instance, 63);
    api->set_param(instance, "clip_record_mode", "Replace");
    api->set_param(instance, "clip_record", "1");
    for (int point = 0; point < 1000; ++point) {
        api->set_param(instance, "clip_automation", "locus");
    }
    for (int note = 0; note < 1025; ++note) {
        midi(api, instance, 0x90, 60, 100, MOVE_MIDI_SOURCE_EXTERNAL);
    }
    api->set_param(instance, "clip_record", "1");
    const std::string capacity = get(api, instance, "clip_capacity_state");
    assert(stateField(capacity, 0) == 1024);
    assert(stateField(capacity, 1) == 1024);
    assert(stateField(capacity, 2) == 2);
    assert(stateField(capacity, 3) == 8192);
    assert(stateField(capacity, 4) == 1);
    api->set_param(instance, "clip_reset", "63");
    api->set_param(instance, "clip_select", "0");
    waitForClip(api, instance, 0);
    api->set_param(instance, "clip_record_mode", "Overdub");

    /* Monitor is explicit and independent from recording or note velocity. */
    api->set_param(instance, "monitor_level", "0.5");
    fillInput(12000);
    render(api, instance, 30);
    assert(peak() >= 5990 && peak() <= 6001);
    api->set_param(instance, "monitor_level", "0");
    render(api, instance, 40);
    assert(peak() <= 8);

    /* The final ceiling is stereo linked and catches full-scale monitoring.
       Dry monitoring bypasses both effects and program compression. */
    api->set_param(instance, "monitor_level", "1");
    fillInput(20000);
    render(api, instance, 100);
    int limiterPrevious = output[(kFrames - 1) * 2];
    fillInput(32767);
    render(api, instance, 1);
    int limiterPreludeStep = 0;
    for (int frame = 0; frame < 60; ++frame) {
        const int sample = output[frame * 2];
        limiterPreludeStep = std::max(
            limiterPreludeStep, std::abs(sample - limiterPrevious));
        limiterPrevious = sample;
    }
    assert(limiterPreludeStep < 1000);
    api->set_param(instance, "performance_reset", "1");
    render(api, instance, 3);
    assert(peak() <= static_cast<int>(kOutputCeiling * 32767.0f) + 1);
    assert(std::strtof(get(api, instance, "limiter_state").c_str(), nullptr)
        > 0.5f);
    api->set_param(instance, "monitor_level", "0");
    render(api, instance, 100);

    /* Record gain uses a linked recovery envelope at the same -1 dBFS
       ceiling and reports intervention instead of flat-topping each sample. */
    api->set_param(instance, "record_gain", "4");
    recordLane(api, instance, 3, 12000, 8);
    assert(stateField(get(api, instance, "ui_state"), 11) == 1);
    api->set_param(instance, "record_gain", "1");

    /* Replace another lane, then exercise every Sample Motion algorithm. */
    recordLane(api, instance, 4, 18000, 12);
    ui = get(api, instance, "ui_state");
    assert((stateField(ui, 1) & 0x9) == 0x9);
    api->set_param(instance, "field", "1");
    for (int algorithm = 0; algorithm < 9; ++algorithm) {
        api->set_param(instance, "algorithm", std::to_string(algorithm).c_str());
        midi(api, instance, 0x90, 60, 64);
        render(api, instance, 8);
        if (peak() == 0) {
            std::fprintf(stderr, "Algorithm %d silent: ui=%s clip=%s voices=%s error=%s\n",
                algorithm, get(api, instance, "ui_state").c_str(),
                get(api, instance, "clip_state").c_str(),
                get(api, instance, "voice_pads").c_str(),
                get(api, instance, "last_error").c_str());
        }
        assert(peak() > 0);
        midi(api, instance, 0x80, 60, 0);
        render(api, instance, 40);
    }

    /* Weave has its own long-grain controls, state keys, and bounded
       layer scheduler rather than stretching the short Grains engine. */
    api->set_param(instance, "algorithm", "Weave");
    api->set_param(instance, "long_grain_mode", "Cloud");
    api->set_param(instance, "long_grain_length", "12000");
    api->set_param(instance, "long_grain_layers", "4");
    api->set_param(instance, "long_grain_pitch", "1.5");
    api->set_param(instance, "long_grain_shape", "Plateau");
    api->set_param(instance, "long_grain_edge", "2000");
    api->set_param(instance, "long_grain_spread", "0.75");
    api->set_param(instance, "long_grain_reverse", "25");
    api->set_param(instance, "long_grain_spray", "0.5");
    api->set_param(instance, "long_grain_bias", "Ahead");
    api->set_param(instance, "long_grain_duration_variation", "0.4");
    api->set_param(instance, "long_grain_pitch_spray", "7");
    api->set_param(instance, "long_grain_level_variation", "0.3");
    api->set_param(instance, "long_grain_phase", "0.5");
    api->set_param(instance, "long_grain_advance", "Per Layer");
    assert(get(api, instance, "algorithm") == "7");
    assert(get(api, instance, "long_grain_mode") == "2");
    assert(get(api, instance, "long_grain_length") == "12000.00");
    assert(get(api, instance, "long_grain_layers") == "4");
    assert(get(api, instance, "long_grain_shape") == "3");
    assert(get(api, instance, "long_grain_advance") == "1");
    midi(api, instance, 0x90, 64, 100);
    render(api, instance, 24);
    assert(peak() > 0);
    midi(api, instance, 0x80, 64, 0);
    render(api, instance, 40);
    const std::string longGrainState = get(api, instance, "state");
    assert(longGrainState.find("\"version\":12") != std::string::npos);
    assert(longGrainState.find("\"long_grain_length\":12000.00000")
        != std::string::npos);

    /* The background worker publishes every loaded lane once. The voices
       consume prepared maps rather than performing on-demand searches. */
    for (int attempt = 0; attempt < 1000
         && stateField(get(api, instance, "waveset_analysis_state"), 1) != 0;
         ++attempt) {
        render(api, instance, 1);
        usleep(1000);
    }
    assert(stateField(get(api, instance, "waveset_analysis_state"), 1) == 0);
    assert(stateField(get(api, instance, "waveset_analysis_state"), 0) != 0);
    api->set_param(instance, "algorithm", "Wavesets");
    api->set_param(instance, "waveset_group", "16 Cycles");
    api->set_param(instance, "waveset_repeats", "5");
    api->set_param(instance, "waveset_stride", "3");
    api->set_param(instance, "waveset_advance", "Preserve");
    api->set_param(instance, "waveset_direction", "Shuffle");
    api->set_param(instance, "waveset_process", "Interpolate");
    api->set_param(instance, "waveset_amount", "65");
    api->set_param(instance, "waveset_detail", "4 kHz");
    assert(get(api, instance, "algorithm") == "8");
    assert(get(api, instance, "waveset_group") == "4");
    assert(get(api, instance, "waveset_repeats") == "5");
    assert(get(api, instance, "waveset_stride") == "3");
    assert(get(api, instance, "waveset_advance") == "1");
    assert(get(api, instance, "waveset_direction") == "3");
    assert(get(api, instance, "waveset_process") == "5");
    assert(get(api, instance, "waveset_amount") == "65.00");
    assert(get(api, instance, "waveset_detail") == "2");
    midi(api, instance, 0x90, 67, 100);
    render(api, instance, 48);
    assert(peak() > 0);
    const std::string wavesetDisplay = get(api, instance, "waveset_state");
    assert(!wavesetDisplay.empty());
    assert(stateField(wavesetDisplay, 1) >= 1);
    assert(stateField(wavesetDisplay, 1) <= 16);
    assert(stateField(wavesetDisplay, 4) >= 1);
    assert(stateField(wavesetDisplay, 5) >= 0);
    assert(stateField(wavesetDisplay, 5) < 4);
    assert(stateField(wavesetDisplay, 7) >= 0);
    assert(stateField(wavesetDisplay, 7) < 12);
    assert(stateField(wavesetDisplay, 9) >= 0);
    assert(stateField(wavesetDisplay, 9) <= 1000);
    assert(stateField(wavesetDisplay, 11) >= 0);
    assert(stateField(wavesetDisplay, 11) <= 1000);
    assert((stateField(wavesetDisplay, 13) & 16) != 0);
    midi(api, instance, 0x80, 67, 0);
    render(api, instance, 40);
    api->set_param(instance, "performance_reset", "1");
    for (int note = 60; note < 68; ++note) {
        midi(api, instance, 0x90, note, 100);
    }
    render(api, instance, 64);
    assert(peak() > 0);
    for (int note = 60; note < 68; ++note) {
        midi(api, instance, 0x80, note, 0);
    }
    render(api, instance, 40);
    const std::string wavesetState = get(api, instance, "state");
    assert(wavesetState.find("\"waveset_group\":16") != std::string::npos);
    assert(wavesetState.find("\"waveset_process\":6") != std::string::npos);

    /* Glitch uses a live per-voice history and Effects colors the summed
       instrument path without changing the dry input monitor. */
    api->set_param(instance, "algorithm", "Glitch");
    api->set_param(instance, "glitch_mode", "Override");
    api->set_param(instance, "glitch_clock", "Free");
    api->set_param(instance, "glitch_rate", "64");
    api->set_param(instance, "glitch_slice", "8");
    api->set_param(instance, "glitch_steps", "1");
    api->set_param(instance, "glitch_pulses", "1");
    api->set_param(instance, "glitch_mix", "100");
    assert(get(api, instance, "glitch_buffer_lock") == "0");
    api->set_param(instance, "glitch_buffer_lock", "1");
    assert(get(api, instance, "glitch_buffer_lock") == "1");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 192);
    assert(peak() > 0);
    api->set_param(instance, "glitch_buffer_lock", "0");
    assert(get(api, instance, "glitch_buffer_lock") == "0");
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 40);

    api->set_param(instance, "algorithm", "Sample");
    api->set_param(instance, "effect_mix", "0");
    api->set_param(instance, "effect_drive", "0");
    render(api, instance, 600);
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 8);
    const int cleanPeak = peak();
    const auto cleanOutput = output;
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 600);
    api->set_param(instance, "effect_drive", "1");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 8);
    int driveDifference = 0;
    for (std::size_t index = 0u; index < output.size(); ++index) {
        driveDifference = std::max(driveDifference, std::abs(
            static_cast<int>(output[index])
            - static_cast<int>(cleanOutput[index])));
    }
    assert(cleanPeak > 0);
    assert(driveDifference > 64);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 40);
    api->set_param(instance, "effect_drive", "0");

    /* Cross-pan preserves the original stereo image at -100, reaches an
       exact mono sum at zero, and feeds the rotary speaker after that field
       transform. Rotary adds independent horn/drum Doppler and level motion. */
    api->set_param(instance, "effect_cross_pan", "-100");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 8);
    assert(stereoDifferencePeak() > 0);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 40);
    api->set_param(instance, "effect_cross_pan", "0");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 40);
    render(api, instance, 8);
    assert(stereoDifferencePeak() <= 2);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 40);
    api->set_param(instance, "effect_cross_pan", "-100");
    api->set_param(instance, "effect_rotary_rate", "0");
    assert(get(api, instance, "effect_rotary_rate") == "0.00");
    api->set_param(instance, "effect_rotary_rate", "6.0");
    api->set_param(instance, "effect_rotary_depth", "100");
    api->set_param(instance, "effect_rotary_mix", "100");
    midi(api, instance, 0x90, 60, 100);
    const int rotaryDifference = renderStereoDifference(api, instance, 20);
    if (rotaryDifference <= 4) {
        std::fprintf(stderr, "Rotary stereo difference: %d, peak: %d\n",
                     rotaryDifference, peak());
    }
    assert(rotaryDifference > 4);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 40);
    api->set_param(instance, "effect_cross_pan", "0");
    api->set_param(instance, "effect_rotary_spread", "0");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 40);
    render(api, instance, 20);
    assert(stereoDifferencePeak() <= 2);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 40);
    api->set_param(instance, "effect_cross_pan", "-100");
    api->set_param(instance, "effect_rotary_rate", "0.8");
    api->set_param(instance, "effect_rotary_depth", "70");
    api->set_param(instance, "effect_rotary_mix", "0");
    api->set_param(instance, "effect_rotary_spread", "100");

    /* Tape is a single stereo post-engine stage. Its historical width ladder
       feeds condition and mechanics macros; hiss remains independently quiet. */
    const std::string tapeTone = libraryDir + "/Tape Continuous Tone.wav";
    writePcm16Wav(tapeTone, MOVE_SAMPLE_RATE, false, true);
    api->set_param(instance, "record_lane", "2");
    const std::string tapeLoadCommand = "2," + tapeTone;
    api->set_param(instance, "load_lane_file", tapeLoadCommand.c_str());
    int tapeLoadState = 0;
    for (int attempt = 0; attempt < 500
         && tapeLoadState != 6 && tapeLoadState != 7; ++attempt) {
        render(api, instance, 1);
        usleep(1000);
        tapeLoadState = stateField(
            get(api, instance, "sample_load_state"), 0);
    }
    assert(tapeLoadState == 6);

    /* Continuous effect motion must not turn block-rate clip automation into
       tap jumps. Delay queues the newest request until its active crossfade
       finishes; rotary controls, Tape Format, and all wet/dry gains remain
       continuous under deliberately harsher motion than the UI produces. */
    api->set_param(instance, "all_notes_off", "1");
    api->set_param(instance, "algorithm", "Sample");
    api->set_param(instance, "sample_mode", "Loop");
    api->set_param(instance, "effect_tone", "0");
    api->set_param(instance, "effect_drive", "0");
    api->set_param(instance, "effect_bits", "0");
    api->set_param(instance, "effect_rate", "1");
    api->set_param(instance, "effect_feedback", "65");
    api->set_param(instance, "effect_width", "100");
    api->set_param(instance, "effect_mix", "100");
    api->set_param(instance, "effect_cross_pan", "-100");
    api->set_param(instance, "effect_rotary_mix", "0");
    api->set_param(instance, "effect_tape_mix", "0");
    api->set_param(instance, "effect_delay", "120");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 400);
    int delayMotionMaximumStep = 0;
    std::array<int, 2u> delayPrevious {};
    bool delayPreviousValid = false;
    for (int block = 0; block < 120; ++block) {
        api->set_param(instance, "effect_delay",
            std::to_string(120 + block).c_str());
        render(api, instance, 1);
        accumulateMaximumOutputStep(delayMotionMaximumStep,
            delayPrevious, delayPreviousValid);
    }
    if (delayMotionMaximumStep >= 3000) {
        std::fprintf(stderr, "Delay motion maximum step: %d\n",
                     delayMotionMaximumStep);
    }
    assert(delayMotionMaximumStep < 3000);

    api->set_param(instance, "effect_feedback", "0");
    api->set_param(instance, "effect_mix", "0");
    render(api, instance, 40);
    api->set_param(instance, "effect_drive", "1");
    const int driveMotionMaximumStep = renderMaximumOutputStep(
        api, instance, 40);
    assert(driveMotionMaximumStep < 3000);
    api->set_param(instance, "effect_drive", "0");

    api->set_param(instance, "effect_rotary_rate", "8");
    api->set_param(instance, "effect_rotary_depth", "100");
    api->set_param(instance, "effect_rotary_mix", "100");
    api->set_param(instance, "effect_rotary_ramp", "50");
    render(api, instance, 200);
    int rotaryMotionMaximumStep = 0;
    std::array<int, 2u> rotaryPrevious {};
    bool rotaryPreviousValid = false;
    for (int block = 0; block < 100; ++block) {
        api->set_param(instance, "effect_rotary_balance",
            block & 1 ? "-100" : "100");
        api->set_param(instance, "effect_rotary_crossover",
            block & 1 ? "250" : "2000");
        api->set_param(instance, "effect_rotary_spread",
            block & 1 ? "0" : "100");
        render(api, instance, 1);
        accumulateMaximumOutputStep(rotaryMotionMaximumStep,
            rotaryPrevious, rotaryPreviousValid);
    }
    assert(rotaryMotionMaximumStep < 3000);

    api->set_param(instance, "effect_rotary_mix", "0");
    api->set_param(instance, "effect_tape_format", "2\"");
    api->set_param(instance, "effect_tape_drive", "0");
    api->set_param(instance, "effect_tape_age", "0");
    api->set_param(instance, "effect_tape_crinkle", "0");
    api->set_param(instance, "effect_tape_mechanics", "100");
    api->set_param(instance, "effect_tape_hiss", "-84");
    api->set_param(instance, "effect_tape_mix", "100");
    render(api, instance, 300);
    int tapeMotionMaximumStep = 0;
    std::array<int, 2u> tapePrevious {};
    bool tapePreviousValid = false;
    for (int block = 0; block < 120; ++block) {
        api->set_param(instance, "effect_tape_format",
            block & 1 ? "2\"" : "1/8\"");
        api->set_param(instance, "effect_tape_mix",
            block & 1 ? "0" : "100");
        render(api, instance, 1);
        accumulateMaximumOutputStep(tapeMotionMaximumStep,
            tapePrevious, tapePreviousValid);
    }
    if (tapeMotionMaximumStep >= 3000) {
        std::fprintf(stderr, "Tape motion maximum step: %d\n",
                     tapeMotionMaximumStep);
    }
    assert(tapeMotionMaximumStep < 3000);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 80);

    api->set_param(instance, "effect_delay", "250");
    api->set_param(instance, "effect_rotary_rate", "0.8");
    api->set_param(instance, "effect_rotary_depth", "70");
    api->set_param(instance, "effect_rotary_ramp", "350");
    api->set_param(instance, "effect_rotary_balance", "0");
    api->set_param(instance, "effect_rotary_crossover", "800");
    api->set_param(instance, "effect_rotary_spread", "100");
    api->set_param(instance, "effect_tape_mix", "0");

    api->set_param(instance, "all_notes_off", "1");
    api->set_param(instance, "effect_tape_format", "1/8\"");
    api->set_param(instance, "effect_tape_drive", "35");
    api->set_param(instance, "effect_tape_age", "64");
    api->set_param(instance, "effect_tape_crinkle", "100");
    api->set_param(instance, "effect_tape_mechanics", "72");
    api->set_param(instance, "effect_tape_inertia", "40");
    api->set_param(instance, "effect_tape_hiss", "-54");
    api->set_param(instance, "effect_tape_mix", "100");
    assert(get(api, instance, "effect_tape_format") == "4");
    assert(get(api, instance, "effect_tape_age") == "64.00");
    assert(get(api, instance, "effect_tape_crinkle") == "100.00");
    assert(get(api, instance, "effect_tape_mechanics") == "72.00");
    assert(get(api, instance, "effect_tape_inertia") == "40.00");
    render(api, instance, 600);
    assert(peak() > 2);
    assert(peak() < 1000);
    api->set_param(instance, "effect_tape_mix", "0");
    render(api, instance, 100);
    assert(peak() <= 2);

    /* CRNK is tape drag and bandwidth sag, never an impulse-noise layer.
       A seamless tone remains continuous even at the most extreme setting. */
    api->set_param(instance, "effect_tape_format", "2\"");
    api->set_param(instance, "effect_tape_drive", "0");
    api->set_param(instance, "effect_tape_age", "0");
    api->set_param(instance, "effect_tape_crinkle", "100");
    api->set_param(instance, "effect_tape_mechanics", "0");
    api->set_param(instance, "effect_tape_inertia", "0");
    api->set_param(instance, "effect_tape_hiss", "-84");
    api->set_param(instance, "effect_tape_mix", "100");
    api->set_param(instance, "sample_mode", "Loop");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 120);
    const int crinkleMaximumStep = renderMaximumOutputStep(
        api, instance, 1400);
    if (crinkleMaximumStep >= 1000) {
        std::fprintf(stderr, "Tape crinkle maximum step: %d\n",
                     crinkleMaximumStep);
    }
    assert(crinkleMaximumStep < 1000);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 80);

    /* The lowest HISS detent is a true off state. INRT uses the existing
       Stack Hold gesture to stop and restart only the fully wet tape path. */
    api->set_param(instance, "effect_tape_format", "2\"");
    api->set_param(instance, "effect_tape_drive", "0");
    api->set_param(instance, "effect_tape_age", "0");
    api->set_param(instance, "effect_tape_crinkle", "0");
    api->set_param(instance, "effect_tape_mechanics", "0");
    api->set_param(instance, "effect_tape_hiss", "-84");
    api->set_param(instance, "effect_tape_mix", "100");
    render(api, instance, 300);
    render(api, instance, 2);
    assert(peak() <= 2);
    api->set_param(instance, "sample_mode", "Loop");
    api->set_param(instance, "effect_tape_inertia", "60");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 24);
    assert(peak() > 100);
    api->set_param(instance, "stack_scan_paused", "1");
    render(api, instance, 400);
    render(api, instance, 2);
    assert(peak() <= 4);
    api->set_param(instance, "stack_scan_paused", "0");
    render(api, instance, 300);
    assert(peak() > 100);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 80);
    api->set_param(instance, "effect_tape_drive", "10");
    api->set_param(instance, "effect_tape_mechanics", "5");
    api->set_param(instance, "effect_tape_inertia", "0");
    api->set_param(instance, "effect_tape_hiss", "-72");
    api->set_param(instance, "effect_tape_mix", "0");

    api->set_param(instance, "algorithm", "Slices");
    api->set_param(instance, "slice_region_mode", "Transients");
    api->set_param(instance, "slice_regions", "32");
    api->set_param(instance, "sample_mode", "Forward");
    api->set_param(instance, "lane_variation", "Cycle");
    api->set_param(instance, "pad_mode", "Slices");
    assert(get(api, instance, "algorithm") == "5");
    assert(get(api, instance, "region_state").find(',') != std::string::npos);
    midi(api, instance, 0x90, 68, 100, MOVE_MIDI_SOURCE_INTERNAL);
    render(api, instance, 8);
    assert(peak() > 0);
    midi(api, instance, 0x80, 68, 0, MOVE_MIDI_SOURCE_INTERNAL);

    /* New cue points snap to the safest stereo zero crossing within 10 ms.
       Moving an already sounding voice uses a 5 ms down/up transition, so
       even a DC-like section cannot click when its reader is repositioned. */
    const std::string cueSample = libraryDir + "/Cue Crossing.wav";
    writePcm16Wav(cueSample, MOVE_SAMPLE_RATE, true);
    api->set_param(instance, "clip_playing", "0");
    api->set_param(instance, "all_notes_off", "1");
    render(api, instance, 40);
    api->set_param(instance, "record_lane", "3");
    const std::string cueLoadCommand = "3," + cueSample;
    api->set_param(instance, "load_lane_file", cueLoadCommand.c_str());
    int cueLoadState = 0;
    for (int attempt = 0; attempt < 500
         && cueLoadState != 6 && cueLoadState != 7; ++attempt) {
        render(api, instance, 1);
        usleep(1000);
        cueLoadState = stateField(
            get(api, instance, "sample_load_state"), 0);
    }
    assert(cueLoadState == 6);
    api->set_param(instance, "algorithm", "Sample");
    api->set_param(instance, "sample_mode", "Loop");
    api->set_param(instance, "manual_lane", "3");
    api->set_param(instance, "lane_variation", "Manual");
    api->set_param(instance, "locus", "0.5");
    api->set_param(instance, "field", "1");
    api->set_param(instance, "speed", "1");
    api->set_param(instance, "pad_mode", "Cues");
    api->set_param(instance, "cue_store", "0,0.25");
    assert(stateField(get(api, instance, "cue_state"), 0) == 293);
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 40);
    const int cuePreviousSample = output[(kFrames - 1) * 2];
    assert(std::abs(cuePreviousSample) > 1000);
    api->set_param(instance, "cue_trigger", "0");
    render(api, instance, 1);
    assert(std::abs(static_cast<int>(output[0]) - cuePreviousSample) < 1000);
    render(api, instance, 12);
    assert(peak() > 0);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 40);

    /* Replaying the same note during a long release must not reclaim and
       hard-reset its audible reader. The release tail remains on its own
       voice while the new attack starts, so a DC-like source stays free of
       the large single-sample discontinuity produced by the old reset. */
    api->set_param(instance, "pad_mode", "Keys");
    api->set_param(instance, "locus", "0.1");
    api->set_param(instance, "field", "0.1");
    api->set_param(instance, "speed", "0.05");
    api->set_param(instance, "amp_attack", "0");
    api->set_param(instance, "amp_decay", "0");
    api->set_param(instance, "amp_sustain", "100");
    api->set_param(instance, "amp_release", "1000");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 16);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 4);
    int previousLeft = output[(kFrames - 1) * 2];
    int previousRight = output[(kFrames - 1) * 2 + 1];
    midi(api, instance, 0x90, 60, 100);
    int sameNoteRetriggerStep = 0;
    int sameNoteRetriggerBlock = -1;
    int sameNoteRetriggerFrame = -1;
    int sameNoteRetriggerBefore = 0;
    int sameNoteRetriggerAfter = 0;
    for (int block = 0; block < 8; ++block) {
        render(api, instance, 1);
        for (int frame = 0; frame < kFrames; ++frame) {
            const int left = output[frame * 2];
            const int right = output[frame * 2 + 1];
            const int leftStep = std::abs(left - previousLeft);
            const int rightStep = std::abs(right - previousRight);
            if (leftStep > sameNoteRetriggerStep) {
                sameNoteRetriggerStep = leftStep;
                sameNoteRetriggerBlock = block;
                sameNoteRetriggerFrame = frame;
                sameNoteRetriggerBefore = previousLeft;
                sameNoteRetriggerAfter = left;
            }
            if (rightStep > sameNoteRetriggerStep) {
                sameNoteRetriggerStep = rightStep;
                sameNoteRetriggerBlock = block;
                sameNoteRetriggerFrame = frame;
                sameNoteRetriggerBefore = previousRight;
                sameNoteRetriggerAfter = right;
            }
            previousLeft = left;
            previousRight = right;
        }
    }
    if (sameNoteRetriggerStep >= 2000) {
        std::fprintf(stderr,
            "Same-note retrigger step: %d at %d:%d (%d -> %d)\n",
            sameNoteRetriggerStep, sameNoteRetriggerBlock,
            sameNoteRetriggerFrame, sameNoteRetriggerBefore,
            sameNoteRetriggerAfter);
    }
    assert(sameNoteRetriggerStep < 2000);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 400);

    /* A long release can keep all eight playheads occupied. The ninth note
       must fade the oldest tail with its original controls, reach silence,
       and only then reset that reader for the replacement attack. This is
       the voice-steal boundary that used to click when attack was zero. */
    api->set_param(instance, "amp_release", "3000");
    for (int voice = 0; voice < 8; ++voice) {
        midi(api, instance, 0x90, 60, 100);
        render(api, instance, 8);
        midi(api, instance, 0x80, 60, 0);
    }
    render(api, instance, 1000);
    previousLeft = output[(kFrames - 1) * 2];
    previousRight = output[(kFrames - 1) * 2 + 1];
    midi(api, instance, 0x90, 60, 100);
    int voiceStealStep = 0;
    for (int block = 0; block < 12; ++block) {
        render(api, instance, 1);
        for (int frame = 0; frame < kFrames; ++frame) {
            const int left = output[frame * 2];
            const int right = output[frame * 2 + 1];
            voiceStealStep = std::max(voiceStealStep,
                std::abs(left - previousLeft));
            voiceStealStep = std::max(voiceStealStep,
                std::abs(right - previousRight));
            previousLeft = left;
            previousRight = right;
        }
    }
    if (voiceStealStep >= 2000) {
        std::fprintf(stderr, "Long-release voice-steal step: %d\n",
                     voiceStealStep);
    }
    assert(voiceStealStep < 2000);
    api->set_param(instance, "amp_release", "25");
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 200);

    /* JOIN also owns the direct Sample Loop seam. The synthetic cue file has
       opposite-polarity endpoints, so an unsmoothed wrap would create a step
       close to twice the steady sample level. */
    api->set_param(instance, "root_note", "60");
    api->set_param(instance, "locus", "0.5");
    api->set_param(instance, "field", "1");
    api->set_param(instance, "speed", "1");
    api->set_param(instance, "join", "1");
    api->set_param(instance, "amp_release", "0");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 4);
    previousLeft = output[(kFrames - 1) * 2];
    previousRight = output[(kFrames - 1) * 2 + 1];
    int sampleLoopStep = 0;
    for (int block = 0; block < 40; ++block) {
        render(api, instance, 1);
        for (int frame = 0; frame < kFrames; ++frame) {
            const int left = output[frame * 2];
            const int right = output[frame * 2 + 1];
            sampleLoopStep = std::max(sampleLoopStep,
                std::abs(left - previousLeft));
            sampleLoopStep = std::max(sampleLoopStep,
                std::abs(right - previousRight));
            previousLeft = left;
            previousRight = right;
        }
    }
    assert(sampleLoopStep < 20000);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 4);
    api->set_param(instance, "amp_attack", "3");
    api->set_param(instance, "amp_release", "25");

    api->set_param(instance, "cue_clear", "0");
    assert(stateField(get(api, instance, "cue_state"), 0) == -1);
    api->set_param(instance, "clear_lane", "Clear");
    render(api, instance, 8);
    api->set_param(instance, "record_lane", "1");

    /* Fidelity controls exercise the hardware-bounded implementations of the
       source-family paths, grain event processes, and lane/region Cutups. */
    const std::string fidelityState = get(api, instance, "state");
    api->set_param(instance, "algorithm", "Motion");
    api->set_param(instance, "motion_mode", "Drunk");
    api->set_param(instance, "motion_travel", "0.8");
    api->set_param(instance, "motion_jitter", "0.6");
    api->set_param(instance, "motion_sound", "Motor");
    api->set_param(instance, "packet_duty", "37");
    api->set_param(instance, "motor_rate", "3.5");
    api->set_param(instance, "motor_symmetry", "72");
    api->set_param(instance, "motor_shape", "Pulse");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 20);
    assert(peak() > 0);
    midi(api, instance, 0x80, 60, 0);

    api->set_param(instance, "algorithm", "Lanes");
    api->set_param(instance, "lane_path", "Random");
    api->set_param(instance, "lane_blend", "Jump");
    api->set_param(instance, "lane_path_shape", "Plateau");
    api->set_param(instance, "lane_path_offset", "0.31");
    api->set_param(instance, "lane_path_skew", "0.73");
    api->set_param(instance, "lane_path_curve", "-0.4");
    api->set_param(instance, "lane_slew", "0");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 20);
    assert(peak() > 0);
    midi(api, instance, 0x80, 60, 0);

    api->set_param(instance, "algorithm", "Grains");
    api->set_param(instance, "grain_quality", "4 Voices");
    assert(get(api, instance, "grain_quality") == "0");
    api->set_param(instance, "grain_quality", "8 Voices");
    assert(get(api, instance, "grain_quality") == "1");
    api->set_param(instance, "grain_quality", "16 Voices");
    assert(get(api, instance, "grain_quality") == "2");
    api->set_param(instance, "grain_spray", "0.7");
    api->set_param(instance, "grain_bias", "Ahead");
    api->set_param(instance, "grain_pitch_spray", "12");
    api->set_param(instance, "grain_reverse", "50");
    api->set_param(instance, "grain_size_variation", "0.8");
    api->set_param(instance, "grain_level_variation", "0.4");
    api->set_param(instance, "grain_timing", "Scatter");
    api->set_param(instance, "grain_timing_scatter", "0.6");
    api->set_param(instance, "grain_envelope_skew", "-0.5");
    api->set_param(instance, "grain_regions", "31");
    api->set_param(instance, "grain_advance", "Per Grain");
    constexpr const char* grainProcesses[] {
        "Ordinary", "Stutter", "Shrink", "Doublets", "Sorter"
    };
    midi(api, instance, 0x90, 72, 100);
    for (const char* process : grainProcesses) {
        api->set_param(instance, "grain_process", process);
        api->set_param(instance, "grain_process_amount", "0.75");
        render(api, instance, 24);
        assert(peak() > 0);
    }
    midi(api, instance, 0x80, 72, 0);

    api->set_param(instance, "algorithm", "Cutups");
    api->set_param(instance, "cut_clock", "Tempo");
    api->set_param(instance, "cut_division", "1/32");
    api->set_param(instance, "cut_time_variation", "0.3");
    api->set_param(instance, "cut_gate", "75");
    api->set_param(instance, "cut_join", "4");
    api->set_param(instance, "cut_regions", "37");
    api->set_param(instance, "cut_repeats", "3");
    api->set_param(instance, "cut_reverse", "35");
    api->set_param(instance, "cut_pitch_variation", "7");
    api->set_param(instance, "cut_level_variation", "0.2");
    api->set_param(instance, "cut_lane_order", "Outside In");
    api->set_param(instance, "cut_poly_path", "Rotate");
    api->set_param(instance, "cutups_mode", "Random Cycle");
    midi(api, instance, 0x90, 48, 100);
    midi(api, instance, 0x90, 72, 100);
    render(api, instance, 40);
    assert(peak() > 0);
    midi(api, instance, 0x80, 48, 0);
    midi(api, instance, 0x80, 72, 0);
    render(api, instance, 80);

    api->set_param(instance, "state", fidelityState.c_str());

    /* Clearing is a bounded engine transition and does not disturb lane 1. */
    api->set_param(instance, "record_lane", "4");
    midi(api, instance, 0xB0, 53, 127, MOVE_MIDI_SOURCE_EXTERNAL);
    render(api, instance, 8);
    ui = get(api, instance, "ui_state");
    assert((stateField(ui, 1) & 0x8) == 0);
    assert((stateField(ui, 1) & 0x1) != 0);

    api->set_param(instance, "motion_mode", "Mirror");
    api->set_param(instance, "motion_sound", "Iterate");
    api->set_param(instance, "motion_rate_basis", "Hertz");
    api->set_param(instance, "motion_path_rate", "2.5");
    api->set_param(instance, "motion_step", "0.17");
    api->set_param(instance, "motion_seed", "42");
    api->set_param(instance, "segment_overlap", "Layer");
    api->set_param(instance, "segment_layers", "3");
    api->set_param(instance, "segment_curve", "-0.25");
    api->set_param(instance, "grain_window", "Hann");
    api->set_param(instance, "lane_2_gain", "0.375");
    api->set_param(instance, "record_gain", "1.75");
    api->set_param(instance, "pad_octave", "2");
    api->set_param(instance, "scale", "Lydian Dominant");
    api->set_param(instance, "glitch_mode", "Scatter");
    api->set_param(instance, "glitch_steps", "13");
    api->set_param(instance, "glitch_pulses", "7");
    api->set_param(instance, "glitch_mix", "82");
    api->set_param(instance, "effect_drive", "0.23");
    api->set_param(instance, "effect_delay", "375");
    api->set_param(instance, "effect_mix", "28");
    api->set_param(instance, "effect_cross_pan", "25");
    api->set_param(instance, "effect_rotary_rate", "3.2");
    api->set_param(instance, "effect_rotary_depth", "76");
    api->set_param(instance, "effect_rotary_mix", "41");
    api->set_param(instance, "effect_rotary_ramp", "2250");
    api->set_param(instance, "effect_rotary_balance", "-35");
    api->set_param(instance, "effect_rotary_crossover", "1250");
    api->set_param(instance, "effect_rotary_spread", "64");
    api->set_param(instance, "effect_tape_format", "1/4\"");
    api->set_param(instance, "effect_tape_drive", "37");
    api->set_param(instance, "effect_tape_wow_rate", "0.41");
    api->set_param(instance, "effect_tape_wow_depth", "29");
    api->set_param(instance, "effect_tape_flutter_rate", "11.3");
    api->set_param(instance, "effect_tape_flutter_depth", "17");
    api->set_param(instance, "effect_tape_hiss", "-68");
    api->set_param(instance, "effect_tape_mix", "33");
    api->set_param(instance, "effect_tape_age", "42");
    api->set_param(instance, "effect_tape_crinkle", "19");
    api->set_param(instance, "effect_tape_mechanics", "58");
    api->set_param(instance, "effect_tape_inertia", "36");
    api->set_param(instance, "algorithm", "Motion");
    api->set_param(instance, "cue_store", "2,0.625");
    const std::string state = get(api, instance, "state");
    assert(state.find("\"version\":12") != std::string::npos);
    assert(state.find("\"scale\":20") != std::string::npos);
    assert(state.find("\"clip_record_mode\":1") != std::string::npos);
    assert(state.find("\"monitor_level\"") == std::string::npos);
    api->set_param(instance, "motion_mode", "Forward");
    api->set_param(instance, "motion_sound", "Continuous");
    api->set_param(instance, "motion_rate_basis", "Normal");
    api->set_param(instance, "motion_path_rate", "1");
    api->set_param(instance, "segment_layers", "1");
    api->set_param(instance, "grain_window", "Bell");
    api->set_param(instance, "lane_2_gain", "1");
    api->set_param(instance, "record_gain", "1");
    api->set_param(instance, "glitch_mode", "Repeat");
    api->set_param(instance, "glitch_steps", "8");
    api->set_param(instance, "effect_drive", "0");
    api->set_param(instance, "effect_mix", "0");
    api->set_param(instance, "effect_cross_pan", "-100");
    api->set_param(instance, "effect_rotary_rate", "0.8");
    api->set_param(instance, "effect_rotary_depth", "70");
    api->set_param(instance, "effect_rotary_mix", "0");
    api->set_param(instance, "effect_rotary_ramp", "350");
    api->set_param(instance, "effect_rotary_balance", "0");
    api->set_param(instance, "effect_rotary_crossover", "800");
    api->set_param(instance, "effect_rotary_spread", "100");
    api->set_param(instance, "effect_tape_format", "2\"");
    api->set_param(instance, "effect_tape_drive", "10");
    api->set_param(instance, "effect_tape_wow_rate", "0.18");
    api->set_param(instance, "effect_tape_wow_depth", "4");
    api->set_param(instance, "effect_tape_flutter_rate", "7");
    api->set_param(instance, "effect_tape_flutter_depth", "3");
    api->set_param(instance, "effect_tape_hiss", "-72");
    api->set_param(instance, "effect_tape_mix", "0");
    api->set_param(instance, "effect_tape_age", "0");
    api->set_param(instance, "effect_tape_crinkle", "0");
    api->set_param(instance, "effect_tape_mechanics", "5");
    api->set_param(instance, "effect_tape_inertia", "0");
    api->set_param(instance, "state", state.c_str());
    assert(get(api, instance, "motion_mode") == "5");
    assert(get(api, instance, "motion_sound") == "5");
    assert(get(api, instance, "motion_rate_basis") == "1");
    assert(get(api, instance, "motion_path_rate") == "2.5000");
    assert(get(api, instance, "motion_step") == "0.1700");
    assert(get(api, instance, "motion_seed") == "42");
    assert(get(api, instance, "segment_overlap") == "1");
    assert(get(api, instance, "segment_layers") == "3");
    assert(get(api, instance, "segment_curve") == "-0.2500");
    assert(get(api, instance, "grain_window") == "2");
    assert(get(api, instance, "lane_2_gain") == "0.3750");
    assert(get(api, instance, "record_gain") == "1.7500");
    assert(get(api, instance, "glitch_mode") == "2");
    assert(get(api, instance, "glitch_steps") == "13");
    assert(get(api, instance, "glitch_pulses") == "7");
    assert(get(api, instance, "glitch_mix") == "82.00");
    assert(get(api, instance, "effect_drive") == "0.2300");
    assert(get(api, instance, "effect_delay") == "375.00");
    assert(get(api, instance, "effect_mix") == "28.00");
    assert(get(api, instance, "effect_cross_pan") == "25.00");
    assert(get(api, instance, "effect_rotary_rate") == "3.20");
    assert(get(api, instance, "effect_rotary_depth") == "76.00");
    assert(get(api, instance, "effect_rotary_mix") == "41.00");
    assert(get(api, instance, "effect_rotary_ramp") == "2250.00");
    assert(get(api, instance, "effect_rotary_balance") == "-35.00");
    assert(get(api, instance, "effect_rotary_crossover") == "1250.00");
    assert(get(api, instance, "effect_rotary_spread") == "64.00");
    assert(get(api, instance, "effect_tape_format") == "3");
    assert(get(api, instance, "effect_tape_drive") == "37.00");
    assert(get(api, instance, "effect_tape_wow_rate") == "0.41");
    assert(get(api, instance, "effect_tape_wow_depth") == "29.00");
    assert(get(api, instance, "effect_tape_flutter_rate") == "11.30");
    assert(get(api, instance, "effect_tape_flutter_depth") == "17.00");
    assert(get(api, instance, "effect_tape_hiss") == "-68.00");
    assert(get(api, instance, "effect_tape_mix") == "33.00");
    assert(get(api, instance, "effect_tape_age") == "42.00");
    assert(get(api, instance, "effect_tape_crinkle") == "19.00");
    assert(get(api, instance, "effect_tape_mechanics") == "58.00");
    assert(get(api, instance, "effect_tape_inertia") == "36.00");
    assert(get(api, instance, "long_grain_length") == "12000.00");
    assert(get(api, instance, "long_grain_layers") == "4");
    assert(get(api, instance, "long_grain_advance") == "1");
    assert(get(api, instance, "waveset_group") == "4");
    assert(get(api, instance, "waveset_repeats") == "5");
    assert(get(api, instance, "waveset_stride") == "3");
    assert(get(api, instance, "waveset_process") == "5");
    assert(get(api, instance, "waveset_amount") == "65.00");
    assert(get(api, instance, "amp_attack") == "3.00");
    assert(get(api, instance, "amp_decay") == "0.00");
    assert(get(api, instance, "amp_sustain") == "100.00");
    assert(get(api, instance, "amp_release") == "25.00");
    assert(get(api, instance, "pad_octave") == "2");
    assert(get(api, instance, "scale") == "20");
    assert(get(api, instance, "grain_quality") == "1");
    assert(stateField(get(api, instance, "cue_state"), 2) == 625);
    assert(get(api, instance, "chain_entries").rfind("1:1:2,1:2:1", 0u) == 0u);

    std::array<char, 256> error {};
    assert(api->get_error(instance, error.data(), static_cast<int>(error.size())) >= 0);
    api->destroy_instance(instance);

    /* Normal overtake teardown atomically saves settings and stereo float WAV
       lanes. A new instance in the same module directory recalls both. */
    const std::string statePath = persistenceDir + "/thru-state.json";
    const std::string lanePath = persistenceDir + "/thru-lane-1.wav";
    const std::string clipPath = persistenceDir + "/thru-clips.bin";
    std::FILE* stateFile = std::fopen(statePath.c_str(), "rb");
    std::FILE* laneFile = std::fopen(lanePath.c_str(), "rb");
    std::FILE* clipFile = std::fopen(clipPath.c_str(), "rb");
    assert(stateFile != nullptr);
    assert(laneFile != nullptr);
    assert(clipFile != nullptr);
    char clipMagic[8] {};
    std::uint32_t clipVersion = 0u;
    assert(std::fread(clipMagic, 1u, sizeof(clipMagic), clipFile)
        == sizeof(clipMagic));
    assert(std::memcmp(clipMagic, "S3GTHR3", 7u) == 0);
    assert(std::fread(&clipVersion, sizeof(clipVersion), 1u, clipFile) == 1u);
    assert(clipVersion == 11u);
    std::fclose(stateFile);
    std::fclose(laneFile);
    std::fclose(clipFile);

    /* A format-9 bank retains the detailed tape lanes. The four macro lanes
       inherit their matching JSON values and upgrade on the next save. */
    downgradeClipBankToV9(clipPath);
    instance = api->create_instance(persistenceDir.c_str(), nullptr);
    assert(instance != nullptr);
    assert(get(api, instance, "persistence") == "1,1");
    assert(get(api, instance, "effect_tape_age") == "42.00");
    assert(get(api, instance, "effect_tape_crinkle") == "19.00");
    assert(get(api, instance, "effect_tape_mechanics") == "58.00");
    assert(get(api, instance, "effect_tape_inertia") == "36.00");
    api->destroy_instance(instance);

    /* A format-8 bank has 132 lock lanes. Tape detail and macro lanes inherit
       the matching JSON state and upgrade on the next save. */
    downgradeClipBankToV8(clipPath);
    instance = api->create_instance(persistenceDir.c_str(), nullptr);
    assert(instance != nullptr);
    assert(get(api, instance, "persistence") == "1,1");
    ui = get(api, instance, "ui_state");
    assert((stateField(ui, 1) & 0x1) != 0);
    assert((stateField(ui, 1) & 0x8) == 0);
    assert(get(api, instance, "motion_mode") == "5");
    assert(get(api, instance, "motion_sound") == "5");
    assert(get(api, instance, "motion_rate_basis") == "1");
    assert(get(api, instance, "segment_layers") == "3");
    assert(get(api, instance, "grain_window") == "2");
    assert(get(api, instance, "lane_2_gain") == "0.3750");
    assert(get(api, instance, "record_gain") == "1.7500");
    assert(get(api, instance, "pad_octave") == "2");
    assert(get(api, instance, "scale") == "20");
    assert(get(api, instance, "glitch_mode") == "2");
    assert(get(api, instance, "glitch_steps") == "13");
    assert(get(api, instance, "effect_drive") == "0.2300");
    assert(get(api, instance, "effect_mix") == "28.00");
    assert(get(api, instance, "effect_cross_pan") == "25.00");
    assert(get(api, instance, "effect_rotary_rate") == "3.20");
    assert(get(api, instance, "effect_rotary_depth") == "76.00");
    assert(get(api, instance, "effect_rotary_mix") == "41.00");
    assert(get(api, instance, "effect_rotary_ramp") == "2250.00");
    assert(get(api, instance, "effect_rotary_balance") == "-35.00");
    assert(get(api, instance, "effect_rotary_crossover") == "1250.00");
    assert(get(api, instance, "effect_rotary_spread") == "64.00");
    assert(get(api, instance, "effect_tape_format") == "3");
    assert(get(api, instance, "effect_tape_drive") == "37.00");
    assert(get(api, instance, "effect_tape_wow_rate") == "0.41");
    assert(get(api, instance, "effect_tape_wow_depth") == "29.00");
    assert(get(api, instance, "effect_tape_flutter_rate") == "11.30");
    assert(get(api, instance, "effect_tape_flutter_depth") == "17.00");
    assert(get(api, instance, "effect_tape_hiss") == "-68.00");
    assert(get(api, instance, "effect_tape_mix") == "33.00");
    assert(get(api, instance, "effect_tape_age") == "42.00");
    assert(get(api, instance, "effect_tape_crinkle") == "19.00");
    assert(get(api, instance, "effect_tape_mechanics") == "58.00");
    assert(get(api, instance, "effect_tape_inertia") == "36.00");
    assert(get(api, instance, "long_grain_length") == "12000.00");
    assert(get(api, instance, "long_grain_layers") == "4");
    assert(get(api, instance, "long_grain_advance") == "1");
    assert(stateField(get(api, instance, "cue_state"), 2) == 625);
    assert(get(api, instance, "chain_entries").rfind("1:1:2,1:2:1", 0u) == 0u);
    assert((unsignedStateField(get(api, instance, "clip_bank_state"), 2) & 0x3u)
        == 0x3u);
    api->set_param(instance, "clip_select", "1");
    waitForClip(api, instance, 1);
    assert(get(api, instance, "algorithm") == "1");
    assert(stateField(get(api, instance, "clip_state"), 5) == 4);
    assert(stateField(get(api, instance, "clip_take_state"), 0) == 3);
    api->set_param(instance, "clip_take_undo", "1");
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);
    assert(stateField(get(api, instance, "clip_take_state"), 0) == 2);
    api->set_param(instance, "clip_take_undo", "1");
    assert(stateField(get(api, instance, "clip_state"), 5) == 2);
    assert(stateField(get(api, instance, "clip_take_state"), 0) == 1);
    assert(stateField(get(api, instance, "clip_take_state"), 1) == 0);
    assert(stateField(get(api, instance, "clip_state"), 6) >= 2);
    assert(get(api, instance, "clip_rate") == "4.0000");
    api->set_param(instance, "clip_select", "0");
    waitForClip(api, instance, 0);
    api->set_param(instance, "record_lane", "2");
    api->set_param(instance, "clip_select", "2");
    waitForClip(api, instance, 2);
    assert(get(api, instance, "lane_source") == "Library Tone.wav");
    api->set_param(instance, "clip_select", "0");
    waitForClip(api, instance, 0);
    assert(stateField(get(api, instance, "clip_state"), 4) >= 150);
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);
    api->set_param(instance, "pressure_mode", "Off");
    api->set_param(instance, "performance_lane", "1");
    api->set_param(instance, "motion_sound", "Continuous");
    api->set_param(instance, "motion_rate_basis", "Normal");
    api->set_param(instance, "effect_cross_pan", "-100");
    api->set_param(instance, "effect_rotary_mix", "0");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 12);
    assert(peak() > 8000);
    assert(static_cast<int>(output[0]) * static_cast<int>(output[1]) < 0);
    midi(api, instance, 0x80, 60, 0);
    api->set_param(instance, "effect_cross_pan", "25");
    api->set_param(instance, "effect_rotary_mix", "41");
    api->set_param(instance, "motion_sound", "Iterate");
    api->set_param(instance, "motion_rate_basis", "Hertz");
    api->set_param(instance, "record_gain", "2.5");
    api->destroy_instance(instance);

    /* The second complete save created one recoverable generation. Corrupt
       only the current clip bank. The newer JSON remains valid, but startup
       must select the older matching JSON/binary pair rather than combining
       two individually valid generations. */
    const std::string stateBackupPath = statePath + ".bak";
    const std::string clipBackupPath = clipPath + ".bak";
    assert(stat(stateBackupPath.c_str(), &captureStatus) == 0);
    assert(stat(clipBackupPath.c_str(), &captureStatus) == 0);
    clipFile = std::fopen(clipPath.c_str(), "r+b");
    assert(clipFile != nullptr);
    assert(std::fwrite("X", 1u, 1u, clipFile) == 1u);
    assert(std::fclose(clipFile) == 0);

    instance = api->create_instance(persistenceDir.c_str(), nullptr);
    assert(instance != nullptr);
    assert(get(api, instance, "persistence") == "1,1");
    assert(get(api, instance, "motion_mode") == "5");
    assert(get(api, instance, "grain_quality") == "1");
    assert(get(api, instance, "record_gain") == "1.7500");
    assert(stateField(get(api, instance, "clip_state"), 4) >= 150);
    api->destroy_instance(instance);

    /* That recovery save leaves a valid generation shared by the current
       binary and JSON backup. A malformed current JSON must find that pair. */
    stateFile = std::fopen(statePath.c_str(), "wb");
    assert(stateFile != nullptr);
    assert(std::fwrite("{", 1u, 1u, stateFile) == 1u);
    assert(std::fclose(stateFile) == 0);
    instance = api->create_instance(persistenceDir.c_str(), nullptr);
    assert(instance != nullptr);
    assert(get(api, instance, "persistence") == "1,1");
    assert(get(api, instance, "record_gain") == "2.5000");
    assert(stateField(get(api, instance, "clip_state"), 4) >= 150);
    api->destroy_instance(instance);

    /* A format-2 bank keeps take membership and receives the restored global
       values for appended controls before upgrading on save. An actual 0.19
       JSON has no such keys, so those values are the module defaults. */
    downgradeClipBankToV2(clipPath);
    instance = api->create_instance(persistenceDir.c_str(), nullptr);
    assert(instance != nullptr);
    assert(get(api, instance, "persistence") == "1,1");
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);
    assert(stateField(get(api, instance, "clip_take_state"), 0) == 2);
    assert(get(api, instance, "motion_path_rate") == "2.5000");
    assert(get(api, instance, "segment_layers") == "3");
    api->destroy_instance(instance);

    /* A 0.18 format-1 bank imports all existing events as its protected base
       take, then upgrades to the current format on the next successful save. */
    downgradeClipBankToV1(clipPath);
    instance = api->create_instance(persistenceDir.c_str(), nullptr);
    assert(instance != nullptr);
    assert(get(api, instance, "persistence") == "1,1");
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);
    assert(stateField(get(api, instance, "clip_take_state"), 0) == 1);
    assert(stateField(get(api, instance, "clip_take_state"), 1) == 0);
    api->destroy_instance(instance);
    clipFile = std::fopen(clipPath.c_str(), "rb");
    assert(clipFile != nullptr);
    assert(std::fread(clipMagic, 1u, sizeof(clipMagic), clipFile)
        == sizeof(clipMagic));
    assert(std::fread(&clipVersion, sizeof(clipVersion), 1u, clipFile) == 1u);
    assert(clipVersion == 11u);
    assert(std::fclose(clipFile) == 0);

    std::remove(statePath.c_str());
    std::remove(stateBackupPath.c_str());
    std::remove(lanePath.c_str());
    std::remove(clipPath.c_str());
    std::remove(clipBackupPath.c_str());
    for (int lane = 2; lane <= 4; ++lane) {
        const std::string path = persistenceDir + "/thru-lane-"
            + std::to_string(lane) + ".wav";
        std::remove(path.c_str());
    }
    assert(std::remove(librarySample.c_str()) == 0);
    assert(std::remove(convertedLibrarySample.c_str()) == 0);
    assert(std::remove(tapeTone.c_str()) == 0);
    assert(std::remove(cueSample.c_str()) == 0);
    assert(std::remove(libraryDir.c_str()) == 0);
    const std::string capturesDir = persistenceDir + "/captures";
    for (const std::string& path : directoryFiles(capturesDir)) {
        assert(std::remove(path.c_str()) == 0);
    }
    assert(std::remove(capturesDir.c_str()) == 0);
    assert(std::remove(persistenceDir.c_str()) == 0);
    std::puts("s3g Thru native tests: PASS");
    return 0;
}

