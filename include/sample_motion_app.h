#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace s3g_daisy {

/**
 * Fixed-memory Daisy adaptation of the s3g Sample instrument family.
 *
 * The desktop engine owns decoded vectors, MIDI voices, envelopes, and event
 * schedulers. This embedded version deliberately keeps each engine
 * allocation-free. It accepts storage from the platform so audio can live in
 * external SDRAM, then provides one bounded image with Motion, optionally
 * stretched Lanes, Grains, Weave long-grain, Cutups, and Wavesets renderers.
 * No renderer allocates or performs file I/O in the audio callback.
 */
class SampleMotionApp {
public:
    static constexpr std::size_t kChannels = 4u;
    static constexpr std::size_t kStackLanes = 4u;
    static constexpr std::size_t kLaneChannels = 2u;
    static constexpr std::size_t kNoLane = kStackLanes;
    static constexpr std::size_t kResampleInputOffset = kChannels;
    static constexpr std::size_t kRegionCount = 64u;
    static constexpr std::size_t kRegionBoundaryCount = kRegionCount + 1u;

    enum class PlaybackAlgorithm : std::uint8_t {
        motion,
        lanes,
        grains,
        cutups,
        sample,
        slices,
        glitch,
        longGrains,
        wavesets,
    };

    static constexpr std::size_t kPlaybackAlgorithmCount = 9u;

    enum class SampleMode : std::uint8_t {
        forward,
        reverse,
        loop,
        pingPong,
    };

    static constexpr std::size_t kSampleModeCount = 4u;

    enum class RegionMode : std::uint8_t {
        equal,
        transients,
    };

    static constexpr std::size_t kRegionModeCount = 2u;

    enum class MotionMode : std::uint8_t {
        forward,
        reverse,
        hover,
        roundTrip,
        movingLoop,
        mirror,
        zigzag,
        drunk,
    };

    static constexpr std::size_t kMotionModeCount = 8u;

    enum class MotionSound : std::uint8_t {
        continuous,
        packets,
        motor,
        bounce,
        freeze,
        iterate,
        pulser,
        doublets,
    };

    static constexpr std::size_t kMotionSoundCount = 8u;

    enum class MotionRateBasis : std::uint8_t {
        normal,
        hertz,
    };

    static constexpr std::size_t kMotionRateBasisCount = 2u;

    enum class SegmentTrigger : std::uint8_t {
        clock,
        packet,
        turn,
    };

    static constexpr std::size_t kSegmentTriggerCount = 3u;

    enum class SegmentOverlap : std::uint8_t {
        cut,
        layer,
    };

    static constexpr std::size_t kSegmentOverlapCount = 2u;

    enum class LanesMode : std::uint8_t {
        forward,
        reverse,
        pingPong,
    };

    static constexpr std::size_t kLanesModeCount = 3u;

    enum class GrainMode : std::uint8_t {
        scan,
        freeze,
        cloud,
        slice,
    };

    static constexpr std::size_t kGrainModeCount = 4u;

    enum class GrainWindow : std::uint8_t {
        parzen,
        sine,
        hann,
        triangle,
        gaussian,
        bell,
    };

    static constexpr std::size_t kGrainWindowCount = 6u;

    enum class LongGrainShape : std::uint8_t {
        bell,
        hann,
        triangle,
        plateau,
    };

    static constexpr std::size_t kLongGrainShapeCount = 4u;

    enum class CutupsMode : std::uint8_t {
        forward,
        reverse,
        palindrome,
        random,
        walk,
        randomCycle,
    };

    static constexpr std::size_t kCutupsModeCount = 6u;

    enum class LanePath : std::uint8_t {
        down,
        up,
        triangle,
        sine,
        stepsDown,
        stepsUp,
        manual,
        random,
    };

    static constexpr std::size_t kLanePathCount = 8u;

    enum class MotorShape : std::uint8_t {
        triangle,
        rounded,
        pulse,
        exponential,
        plateau,
    };

    static constexpr std::size_t kMotorShapeCount = 5u;

    enum class LaneBlendMode : std::uint8_t {
        crossfade,
        jump,
    };

    static constexpr std::size_t kLaneBlendModeCount = 2u;

    enum class LanePathShape : std::uint8_t {
        linear,
        smooth,
        exponential,
        plateau,
    };

    static constexpr std::size_t kLanePathShapeCount = 4u;

    enum class GrainTiming : std::uint8_t {
        regular,
        scatter,
    };

    static constexpr std::size_t kGrainTimingCount = 2u;

    enum class GrainBias : std::uint8_t {
        behind,
        around,
        ahead,
    };

    static constexpr std::size_t kGrainBiasCount = 3u;

    enum class GrainProcess : std::uint8_t {
        ordinary,
        stutter,
        shrink,
        doublets,
        sorter,
    };

    static constexpr std::size_t kGrainProcessCount = 5u;

    enum class GrainAdvance : std::uint8_t {
        scan,
        grain,
    };

    static constexpr std::size_t kGrainAdvanceCount = 2u;

    enum class CutClock : std::uint8_t {
        free,
        tempo,
    };

    static constexpr std::size_t kCutClockCount = 2u;

    enum class CutDivision : std::uint8_t {
        quarter,
        eighth,
        eighthTriplet,
        sixteenth,
        sixteenthTriplet,
        thirtySecond,
    };

    static constexpr std::size_t kCutDivisionCount = 6u;

    enum class CutLaneOrder : std::uint8_t {
        down,
        up,
        palindrome,
        random,
        pairs,
        outsideIn,
        centerOut,
    };

    static constexpr std::size_t kCutLaneOrderCount = 7u;

    enum class CutPolyPath : std::uint8_t {
        together,
        stagger,
        rotate,
        random,
    };

    static constexpr std::size_t kCutPolyPathCount = 4u;

    enum class GlitchMode : std::uint8_t {
        repeat,
        stutter,
        scatter,
        divide,
        override,
    };

    static constexpr std::size_t kGlitchModeCount = 5u;

    enum class WavesetAdvance : std::uint8_t {
        stretch,
        preserve,
        hold,
    };

    static constexpr std::size_t kWavesetAdvanceCount = 3u;

    enum class WavesetDirection : std::uint8_t {
        forward,
        reverse,
        pendulum,
        shuffle,
    };

    static constexpr std::size_t kWavesetDirectionCount = 4u;

    enum class WavesetProcess : std::uint8_t {
        repeat,
        omit,
        replace,
        envelope,
        multiply,
        average,
        interpolate,
        fractal,
        harmonic,
        groupReverse,
        cycleReverse,
        telescope,
    };

    static constexpr std::size_t kWavesetProcessCount = 12u;

    enum class WavesetDetail : std::uint8_t {
        raw,
        hz8000,
        hz4000,
        hz1000,
        hz250,
    };

    static constexpr std::size_t kWavesetDetailCount = 5u;

    struct Controls {
        float locus = 0.5f;
        float field = 1.0f;
        float speed = 1.0f;
        float mix = 1.0f;
        float join = 1.0f;
        // Shared note-amplitude envelope. This lives with the controls so
        // performance clips can snapshot and automate it like other shared
        // playback parameters; the sample engines do not apply it directly.
        float ampAttackMs = 3.0f;
        float ampDecayMs = 0.0f;
        float ampSustain = 1.0f;
        float ampReleaseMs = 25.0f;
        PlaybackAlgorithm algorithm = PlaybackAlgorithm::motion;
        MotionMode motion = MotionMode::forward;
        MotionSound motionSound = MotionSound::continuous;
        float motionSoundRateHz = 4.0f;
        MotionRateBasis motionRateBasis = MotionRateBasis::normal;
        float motionPathRate = 1.0f;
        float motionStep = 0.05f;
        float motionSeed = 1.0f;
        float motionTravel = 1.0f;
        float motionJitter = 0.0f;
        float packetDuty = 0.65f;
        float motorRateHz = 1.0f;
        float motorSymmetry = 0.5f;
        MotorShape motorShape = MotorShape::rounded;
        SegmentTrigger segmentTrigger = SegmentTrigger::clock;
        SegmentOverlap segmentOverlap = SegmentOverlap::cut;
        float segmentLayerLimit = 2.0f;
        float segmentRateHz = 4.0f;
        float segmentRepeats = 2.0f;
        float segmentStep = 0.05f;
        float segmentPitchScatter = 0.0f;
        float segmentLevelVariation = 0.15f;
        float segmentIntervalCurve = 0.0f;
        LanesMode lanes = LanesMode::forward;
        GrainMode grains = GrainMode::scan;
        CutupsMode cutups = CutupsMode::forward;
        LanePath lanePath = LanePath::triangle;
        float laneCycles = 1.0f;
        float manualLane = 0.0f;
        float lanePhaseOffset = 0.0f;
        bool laneScanPaused = false;
        bool lanePhaseOverride = false;
        float lanePhaseOverrideValue = 0.0f;
        LaneBlendMode laneBlend = LaneBlendMode::crossfade;
        LanePathShape lanePathShape = LanePathShape::linear;
        float lanePathOffset = 0.0f;
        float lanePathSkew = 0.5f;
        float lanePathCurve = 0.0f;
        float laneSlewMs = 20.0f;
        float laneGains[kStackLanes] { 1.0f, 1.0f, 1.0f, 1.0f };
        bool lanesStretch = true;
        float lanesStretchRatio = 1.0f;
        float grainSizeMs = 90.0f;
        float grainDensityHz = 24.0f;
        float grainVoiceLimit = 8.0f;
        float grainPitch = 1.0f;
        // Per-voice keyboard ratio. Granular source advance remains on speed.
        float notePitch = 1.0f;
        GrainWindow grainWindow = GrainWindow::bell;
        float grainStereoSpread = 0.0f;
        float grainSpray = 0.0f;
        GrainBias grainBias = GrainBias::around;
        float grainPitchSpray = 0.0f;
        float grainReverseChance = 0.0f;
        float grainSizeVariation = 0.0f;
        float grainLevelVariation = 0.0f;
        GrainTiming grainTiming = GrainTiming::regular;
        float grainTimingScatter = 0.0f;
        float grainEnvelopeSkew = 0.0f;
        GrainProcess grainProcess = GrainProcess::ordinary;
        float grainProcessAmount = 0.5f;
        float grainRegionCount = 16.0f;
        GrainAdvance grainAdvance = GrainAdvance::scan;
        GrainMode longGrainMode = GrainMode::scan;
        float longGrainLengthMs = 2000.0f;
        float longGrainLayers = 2.0f;
        float longGrainPitch = 1.0f;
        LongGrainShape longGrainShape = LongGrainShape::plateau;
        float longGrainEdgeMs = 120.0f;
        float longGrainSpread = 0.5f;
        float longGrainReverseChance = 0.0f;
        float longGrainSpray = 0.0f;
        GrainBias longGrainBias = GrainBias::around;
        float longGrainDurationVariation = 0.0f;
        float longGrainPitchSpray = 0.0f;
        float longGrainLevelVariation = 0.0f;
        float longGrainPhaseSpread = 0.0f;
        GrainAdvance longGrainAdvance = GrainAdvance::scan;
        CutClock cutClock = CutClock::tempo;
        CutDivision cutDivision = CutDivision::sixteenth;
        float cutRateHz = 8.0f;
        float cutTempoBpm = 120.0f;
        float cutSwing = 0.5f;
        float cutTimeVariation = 0.0f;
        float cutGate = 1.0f;
        float cutJoinMs = 10.0f;
        float cutRegionCount = 16.0f;
        float cutRepeats = 1.0f;
        float cutReverseChance = 0.0f;
        float cutPitchVariation = 0.0f;
        float cutLevelVariation = 0.0f;
        CutLaneOrder cutLaneOrder = CutLaneOrder::down;
        CutPolyPath cutPolyPath = CutPolyPath::together;
        float cutPitch = 1.0f;
        float cutVoiceIndex = 0.0f;
        RegionMode cutRegionMode = RegionMode::equal;
        SampleMode sampleMode = SampleMode::forward;
        RegionMode sampleRegionMode = RegionMode::transients;
        float sampleRegionCount = 16.0f;
        float sampleLane = 0.0f;
        float sampleSlice = 0.0f;
        GlitchMode glitchMode = GlitchMode::override;
        CutClock glitchClock = CutClock::tempo;
        CutDivision glitchDivision = CutDivision::sixteenth;
        float glitchRateHz = 8.0f;
        float glitchSliceMs = 32.0f;
        float glitchRepeats = 4.0f;
        float glitchChance = 1.0f;
        float glitchMix = 1.0f;
        float glitchSteps = 11.0f;
        float glitchPulses = 5.0f;
        float glitchRotate = 0.0f;
        float glitchJumpMs = 120.0f;
        float glitchPitch = 7.0f;
        float glitchReverseChance = 0.25f;
        float glitchDecay = 0.9f;
        float glitchSeed = 1.0f;
        bool glitchBufferLocked = false;
        float wavesetGroup = 8.0f;
        float wavesetRepeats = 2.0f;
        float wavesetStride = 1.0f;
        WavesetAdvance wavesetAdvance = WavesetAdvance::stretch;
        WavesetDirection wavesetDirection = WavesetDirection::forward;
        WavesetProcess wavesetProcess = WavesetProcess::repeat;
        float wavesetProcessAmount = 0.0f;
        WavesetDetail wavesetDetail = WavesetDetail::raw;
        float effectTone = 0.0f;
        float effectDrive = 0.0f;
        float effectBits = 0.0f;
        float effectRate = 1.0f;
        float effectDelayMs = 250.0f;
        float effectFeedback = 0.35f;
        float effectWidth = 1.0f;
        float effectMix = 0.0f;
        // Post-delay stereo field and rotary-speaker controls. Cross-pan uses
        // -1 for the original image, 0 for a mono sum, and +1 for swapped L/R.
        float effectCrossPan = -1.0f;
        float effectRotaryRateHz = 0.8f;
        float effectRotaryDepth = 0.7f;
        float effectRotaryMix = 0.0f;
        float effectRotaryRampMs = 350.0f;
        float effectRotaryBalance = 0.0f;
        float effectRotaryCrossoverHz = 800.0f;
        float effectRotarySpread = 1.0f;
        // Shared post-engine tape stage. Format runs from two-inch mastering
        // tape (0) through compact-cassette-width tape (4). The legacy
        // detailed wow/flutter values remain serialized for compatibility;
        // the Move surface uses the four musical condition/mechanics macros.
        int effectTapeFormat = 0;
        float effectTapeDrive = 0.10f;
        float effectTapeWowRateHz = 0.18f;
        float effectTapeWowDepth = 0.04f;
        float effectTapeFlutterRateHz = 7.0f;
        float effectTapeFlutterDepth = 0.03f;
        float effectTapeHissDb = -72.0f;
        float effectTapeMix = 0.0f;
        float effectTapeAge = 0.0f;
        float effectTapeCrinkle = 0.0f;
        float effectTapeMechanics = 0.05f;
        float effectTapeInertia = 0.0f;
        float recordInputGain = 1.0f;
    };

    bool prepare(float outputSampleRate,
                 float* channel0,
                 float* channel1,
                 float* channel2,
                 float* channel3,
                 std::size_t capacityFrames)
    {
        if (!(outputSampleRate > 0.0f) || !channel0 || !channel1 || !channel2
            || !channel3 || capacityFrames == 0u) {
            prepared_ = false;
            return false;
        }

        storage_[0] = channel0;
        storage_[1] = channel1;
        storage_[2] = channel2;
        storage_[3] = channel3;
        capacityFrames_ = capacityFrames;
        laneCapacityFrames_ = capacityFrames_ / kLaneChannels;
        outputSampleRate_ = outputSampleRate;
        windowFadeFrames_ = std::max<std::size_t>(
            32u, static_cast<std::size_t>(outputSampleRate_ * 0.030f));
        retriggerFadeFrames_ = std::max<std::size_t>(
            16u, static_cast<std::size_t>(outputSampleRate_ * 0.003f));
        retriggerTransitionFrames_ = std::max<std::size_t>(
            32u, static_cast<std::size_t>(outputSampleRate_ * 0.005f));
        safetyFadeFrames_ = std::max<std::size_t>(
            16u, static_cast<std::size_t>(outputSampleRate_ * 0.003f));
        readerTransitionFrames_ = std::max<std::size_t>(
            32u, static_cast<std::size_t>(outputSampleRate_ * 0.005f));
        algorithmFadeFrames_ = std::max<std::size_t>(
            32u, static_cast<std::size_t>(outputSampleRate_ * 0.010f));
        recordTransitionFrames_ = std::max<std::size_t>(
            32u, static_cast<std::size_t>(outputSampleRate_ * 0.005f));
        speedSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.020f));
        joinSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.010f));
        motionControlSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.020f));
        stretchRatioSlew_ = motionControlSlew_;
        laneSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.020f));
        eventWindowSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.040f));
        motionEnvelopeSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.002f));
        controlGainSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.003f));
        // The desktop Wavesets engine reads Depth and Join continuously.
        // A compact five-millisecond slew removes encoder zipper noise while
        // remaining perceptually immediate and leaving the process itself
        // unchanged.
        wavesetControlSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.005f));
        wavesetControlTransitionFrames_ = std::max<std::size_t>(
            32u, static_cast<std::size_t>(outputSampleRate_ * 0.005f));
        recordMeterRelease_ = std::exp(
            -1.0f / std::max(1.0f, outputSampleRate_ * 0.250f));
        recordLimiterRelease_ = 1.0f - std::exp(
            -1.0f / std::max(1.0f, outputSampleRate_ * 0.080f));
        prepareGrainWindowTables();
        clear();
        prepareWavesetAnalysisFilters();
        prepared_ = true;
        return true;
    }

    /**
     * Rebind an already-prepared engine to another same-format sample bank.
     * This is intended for an audio-boundary double-buffer swap: window tables
     * and coefficients remain prepared, while reader/record state is reset.
     */
    bool rebindStorage(float* channel0,
                       float* channel1,
                       float* channel2,
                       float* channel3,
                       std::size_t capacityFrames)
    {
        if (!prepared_ || !channel0 || !channel1 || !channel2 || !channel3
            || capacityFrames == 0u) return false;
        storage_[0] = channel0;
        storage_[1] = channel1;
        storage_[2] = channel2;
        storage_[3] = channel3;
        capacityFrames_ = capacityFrames;
        laneCapacityFrames_ = capacityFrames_ / kLaneChannels;
        clear();
        prepared_ = true;
        return true;
    }

    void clear()
    {
        sourceFrames_ = 0u;
        sourceChannels_ = 0u;
        sourceSampleRate_ = outputSampleRate_;
        recordFrames_ = 0u;
        recordChannels_ = 0u;
        recordingLane_ = kNoLane;
        lastRecordedLane_ = kNoLane;
        recordPeak_ = 0.0f;
        recordMeterPeak_ = 0.0f;
        lastRecordedPeak_ = 0.0f;
        recordLimiterGain_ = 1.0f;
        recordLimited_ = false;
        lastRecordLimited_ = false;
        recordInputOffset_ = 0u;
        recordWriteActive_ = false;
        recordTransitionRemaining_ = 0u;
        laneTransition_ = LaneTransition::none;
        transitionLane_ = kNoLane;
        transitionFrames_ = 0u;
        transitionSampleRate_ = outputSampleRate_;
        stackMode_ = false;
        referenceLane_ = 0u;
        lanePosition_ = 0.0f;
        lanePhase_ = 0.0f;
        lanePhaseIncrement_ = 0.0f;
        laneFrozenPhase_ = 0.0f;
        laneScanPaused_ = false;
        laneWeightsInitialized_ = false;
        lastLaneSlewMs_ = -1.0f;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            laneFrames_[lane] = 0u;
            laneSampleRates_[lane] = outputSampleRate_;
            lanePositionScales_[lane] = 0.0;
            laneWeights_[lane] = 0.0f;
            laneWeightTargets_[lane] = 0.0f;
            activeControlsLaneGains_[lane] = 1.0f;
            targetControlsLaneGains_[lane] = 1.0f;
            laneRegionCounts_[lane] = 0u;
            laneRegionBoundaries_[lane][0u] = 0.0f;
            laneRegionBoundaries_[lane][1u] = 1.0f;
        }
        phase_ = 0.0;
        speed_ = 1.0f;
        join_ = 1.0f;
        joinInitialized_ = false;
        stretchRatio_ = 1.0f;
        stretchRatioTarget_ = 1.0f;
        stretchRatioInitialized_ = false;
        displayPosition_ = 0.0;
        fieldLow_ = 0.0;
        fieldHigh_ = 0.0;
        currentWindow_ = {};
        fromWindow_ = {};
        toWindow_ = {};
        currentMode_ = MotionMode::forward;
        fromMode_ = MotionMode::forward;
        toMode_ = MotionMode::forward;
        requestedMode_ = MotionMode::forward;
        currentMotionSound_ = MotionSound::continuous;
        pendingMotionSound_ = MotionSound::continuous;
        motionSoundTransitionRemaining_ = 0u;
        sampleConfigInitialized_ = false;
        sampleConfigPending_ = false;
        sampleTransitionRemaining_ = 0u;
        currentAlgorithm_ = PlaybackAlgorithm::motion;
        pendingAlgorithm_ = PlaybackAlgorithm::motion;
        algorithmFadeRemaining_ = 0u;
        windowFadeRemaining_ = 0u;
        retriggerFadeRemaining_ = 0u;
        retriggerTransitionRemaining_ = 0u;
        windowInitialized_ = false;
        eventWindowInitialized_ = false;
        movingCenterInitialized_ = false;
        lastControlLocus_ = -1.0f;
        motionInnerPhase_ = 0.0;
        motionOuterPhase_ = 0.5;
        motionEnvelope_ = 1.0f;
        resetEventReaders();
        playing_ = false;
        recording_ = false;
    }

    bool adoptSource(std::size_t frames,
                     std::size_t channels,
                     float sourceSampleRate)
    {
        if (!prepared_ || frames == 0u || frames > capacityFrames_
            || channels == 0u || channels > kChannels
            || !(sourceSampleRate > 0.0f)) {
            return false;
        }

        sourceFrames_ = frames;
        sourceChannels_ = channels;
        sourceSampleRate_ = sourceSampleRate;
        prepareWavesetAnalysisFilters();
        stackMode_ = false;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            laneFrames_[lane] = 0u;
        }
        recordFrames_ = 0u;
        recordChannels_ = 0u;
        resetReader();
        retriggerFadeRemaining_ = retriggerFadeFrames_;
        recording_ = false;
        recordWriteActive_ = false;
        recordTransitionRemaining_ = 0u;
        playing_ = true;
        return true;
    }

    /** Adopt one planar-stereo file into a four-file Sample Lanes stack. */
    bool adoptLane(std::size_t lane,
                   std::size_t frames,
                   float sourceSampleRate)
    {
        if (!prepared_ || lane >= kStackLanes || frames == 0u
            || frames > laneCapacityFrames_ || !(sourceSampleRate > 0.0f)) {
            return false;
        }

        if (!stackMode_) {
            for (std::size_t index = 0u; index < kStackLanes; ++index) {
                laneFrames_[index] = 0u;
                laneSampleRates_[index] = outputSampleRate_;
            }
            referenceLane_ = lane;
        }
        stackMode_ = true;
        laneFrames_[lane] = frames;
        laneSampleRates_[lane] = sourceSampleRate;
        if (referenceLane_ >= kStackLanes
            || laneFrames_[referenceLane_] == 0u) {
            referenceLane_ = lane;
        }
        sourceFrames_ = laneFrames_[referenceLane_];
        sourceChannels_ = kLaneChannels;
        sourceSampleRate_ = laneSampleRates_[referenceLane_];
        prepareWavesetAnalysisFilters();
        updateLanePositionScales();
        recordFrames_ = 0u;
        recordChannels_ = 0u;
        resetReader();
        retriggerFadeRemaining_ = retriggerFadeFrames_;
        recording_ = false;
        recordWriteActive_ = false;
        recordTransitionRemaining_ = 0u;
        playing_ = true;
        return true;
    }

    /** Record a Patch stereo input pair or the stereo output into a stack lane. */
    bool beginLaneRecording(std::size_t lane, std::size_t inputOffset)
    {
        if (!prepared_ || recording_ || lane >= kStackLanes
            || recordTransitionRemaining_ != 0u
            || laneTransition_ != LaneTransition::none
            || (inputOffset != 0u && inputOffset != 2u
                && inputOffset != kResampleInputOffset)) {
            return false;
        }

        const bool canContinuePlayback
            = stackMode_ && loadedLaneCount() != 0u;
        if (!stackMode_) {
            for (std::size_t index = 0u; index < kStackLanes; ++index) {
                laneFrames_[index] = 0u;
                laneSampleRates_[index] = outputSampleRate_;
            }
            referenceLane_ = lane;
            sourceFrames_ = 0u;
            playing_ = false;
        }
        stackMode_ = true;
        sourceChannels_ = kLaneChannels;
        recordFrames_ = 0u;
        recordChannels_ = kLaneChannels;
        recordingLane_ = lane;
        lastRecordedLane_ = kNoLane;
        recordPeak_ = 0.0f;
        recordMeterPeak_ = 0.0f;
        recordLimiterGain_ = 1.0f;
        recordLimited_ = false;
        recordInputOffset_ = inputOffset;
        recording_ = true;
        recordWriteActive_ = false;
        if (canContinuePlayback) {
            // Keep the transition delay even when this engine is acting only
            // as the record-bank coordinator. Shared playback voices use the
            // same midpoint before the destination lane is overwritten.
            recordTransitionRemaining_ = recordTransitionFrames_ * 2u;
        } else {
            recordTransitionRemaining_ = 0u;
            activateLaneRecording();
        }
        return true;
    }

    /** Fade a loaded lane out of the active stack without erasing its memory. */
    bool beginLaneRemoval(std::size_t lane)
    {
        if (!prepared_ || recording_ || lane >= kStackLanes
            || laneFrames_[lane] == 0u || recordTransitionRemaining_ != 0u
            || laneTransition_ != LaneTransition::none) {
            return false;
        }
        laneTransition_ = LaneTransition::remove;
        transitionLane_ = lane;
        recordTransitionRemaining_ = recordTransitionFrames_ * 2u;
        return true;
    }

    /**
     * Fade a main-thread-loaded lane into the active stack. The destination
     * must already be absent so its SDRAM can be written safely.
     */
    bool beginLaneAdoption(std::size_t lane,
                           std::size_t frames,
                           float sourceSampleRate)
    {
        if (!prepared_ || recording_ || lane >= kStackLanes
            || laneFrames_[lane] != 0u || frames == 0u
            || frames > laneCapacityFrames_ || !(sourceSampleRate > 0.0f)
            || recordTransitionRemaining_ != 0u
            || laneTransition_ != LaneTransition::none) {
            return false;
        }
        laneTransition_ = LaneTransition::adopt;
        transitionLane_ = lane;
        transitionFrames_ = frames;
        transitionSampleRate_ = sourceSampleRate;
        recordTransitionRemaining_ = recordTransitionFrames_ * 2u;
        return true;
    }

    bool beginRecording(std::size_t channels)
    {
        if (!prepared_ || channels == 0u || channels > kChannels) {
            return false;
        }

        sourceFrames_ = 0u;
        sourceChannels_ = 0u;
        sourceSampleRate_ = outputSampleRate_;
        stackMode_ = false;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            laneFrames_[lane] = 0u;
        }
        recordFrames_ = 0u;
        recordChannels_ = channels;
        recordingLane_ = kNoLane;
        lastRecordedLane_ = kNoLane;
        recordInputOffset_ = 0u;
        recordWriteActive_ = true;
        recordTransitionRemaining_ = 0u;
        recordPeak_ = 0.0f;
        recordMeterPeak_ = 0.0f;
        recordLimiterGain_ = 1.0f;
        recordLimited_ = false;
        resetReader();
        playing_ = false;
        recording_ = true;
        return true;
    }

    bool endRecording()
    {
        if (!recording_) {
            return false;
        }

        recording_ = false;
        if (recordingLane_ < kStackLanes) {
            const std::size_t lane = recordingLane_;
            if (!recordWriteActive_ && recordFrames_ == 0u) {
                recordingLane_ = kNoLane;
                recordChannels_ = 0u;
                return playing_;
            }
            const bool continuingPlayback = playing_ && sourceFrames_ != 0u;
            laneFrames_[lane] = recordFrames_;
            laneSampleRates_[lane] = outputSampleRate_;
            if (referenceLane_ >= kStackLanes
                || laneFrames_[referenceLane_] == 0u) {
                referenceLane_ = lane;
            }
            sourceFrames_ = laneFrames_[referenceLane_];
            sourceChannels_ = kLaneChannels;
            sourceSampleRate_ = laneSampleRates_[referenceLane_];
            prepareWavesetAnalysisFilters();
            stackMode_ = true;
            lastRecordedLane_ = lane;
            lastRecordedPeak_ = recordPeak_;
            lastRecordLimited_ = recordLimited_;
            recordingLane_ = kNoLane;
            recordChannels_ = 0u;
            recordWriteActive_ = false;
            updateLanePositionScales();
            if (!continuingPlayback) {
                resetReader();
                retriggerFadeRemaining_ = retriggerFadeFrames_;
            }
            playing_ = sourceFrames_ != 0u;
            return playing_;
        }
        sourceFrames_ = recordFrames_;
        sourceChannels_ = recordChannels_;
        sourceSampleRate_ = outputSampleRate_;
        prepareWavesetAnalysisFilters();
        stackMode_ = false;
        lastRecordedLane_ = kNoLane;
        recordChannels_ = 0u;
        recordWriteActive_ = false;
        recordTransitionRemaining_ = 0u;
        resetReader();
        playing_ = sourceFrames_ != 0u;
        return playing_;
    }

    void retrigger(bool currentOutputAudible)
    {
        if (sourceFrames_ == 0u) {
            return;
        }
        const bool audiblePlayback = currentOutputAudible && playing_ && !sampleDone_
            && retriggerFadeRemaining_ != retriggerFadeFrames_;
        if (audiblePlayback) {
            // A voice may have to be reused when all eight slots are busy.
            // Reach silence before resetting its reader so a nonzero sample
            // cannot jump directly to the start fade.
            seekPending_ = false;
            if (retriggerTransitionRemaining_ == 0u) {
                retriggerTransitionRemaining_ = retriggerTransitionFrames_ * 2u;
            } else if (retriggerTransitionRemaining_
                       < retriggerTransitionFrames_) {
                retriggerTransitionRemaining_ = retriggerTransitionFrames_ * 2u
                    - retriggerTransitionRemaining_;
            }
        } else {
            resetReader();
            retriggerFadeRemaining_ = retriggerFadeFrames_;
        }
        playing_ = true;
    }

    void seekNormalized(float position)
    {
        if (sourceFrames_ == 0u) return;
        const double normalized = static_cast<double>(clamp01(position));
        double target = normalized;
        if (currentAlgorithm_ == PlaybackAlgorithm::sample
            || currentAlgorithm_ == PlaybackAlgorithm::slices) {
            if (currentSampleMode_ == SampleMode::reverse) {
                target = 1.0 - normalized;
            } else if (currentSampleMode_ == SampleMode::pingPong) {
                // Choose the forward half of the triangle so a cue always
                // lands on the requested source position.
                target = normalized * 0.5;
            }
        }
        const bool silentRetrigger = playing_
            && retriggerFadeRemaining_ == retriggerFadeFrames_;
        if (!playing_ || silentRetrigger) {
            phase_ = target;
            resetEventReaders();
            retriggerTransitionRemaining_ = 0u;
            seekPending_ = false;
            retriggerFadeRemaining_ = retriggerFadeFrames_;
            sampleDone_ = false;
        } else {
            pendingSeekPhase_ = target;
            seekPending_ = true;
            if (retriggerTransitionRemaining_ == 0u) {
                retriggerTransitionRemaining_ = retriggerTransitionFrames_ * 2u;
            } else if (retriggerTransitionRemaining_
                       < retriggerTransitionFrames_) {
                // A second cue during fade-in reverses the envelope from its
                // current gain instead of jumping back to full level.
                retriggerTransitionRemaining_ = retriggerTransitionFrames_ * 2u
                    - retriggerTransitionRemaining_;
            }
        }
        playing_ = true;
    }

    void stopPlayback() { playing_ = false; }

    void process(const float* const* input,
                 float** output,
                 std::size_t frames,
                 const Controls& controls)
    {
        if (!prepared_ || !input || !output || frames == 0u) {
            return;
        }
        for (std::size_t channel = 0u; channel < kChannels; ++channel) {
            if (!input[channel] || !output[channel]) {
                return;
            }
        }

        const float dryWet = clamp01(controls.mix);
        const float targetSpeed = clamp(controls.speed, 0.05f, 4.0f);
        const float targetJoin = clamp01(controls.join);
        if (!joinInitialized_) {
            join_ = targetJoin;
            joinInitialized_ = true;
        }
        stretchRatioTarget_ = clamp(
            controls.lanesStretchRatio, 0.25f, 4.0f);
        if (!stretchRatioInitialized_) {
            stretchRatio_ = stretchRatioTarget_;
            stretchRatioInitialized_ = true;
        }
        recordInputGain_ = clamp(controls.recordInputGain, 0.0f, 1.0f);
        const float requestedLaneSlewMs = clamp(
            controls.laneSlewMs, 0.0f, 500.0f);
        if (std::abs(requestedLaneSlewMs - lastLaneSlewMs_) > 1.0e-3f) {
            // "0 ms" remains immediate to the player, but a 5 ms internal
            // floor prevents contrasting lanes from creating a short digital
            // edge when a path or live parameter changes abruptly.
            const float laneSlewSeconds = std::max(
                0.005f, requestedLaneSlewMs * 0.001f);
            laneSlew_ = 1.0f - std::exp(-1.0f / std::max(
                1.0f, outputSampleRate_ * laneSlewSeconds));
            lastLaneSlewMs_ = requestedLaneSlewMs;
        }
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            targetControlsLaneGains_[lane]
                = clamp(controls.laneGains[lane], 0.0f, 4.0f);
        }
        requestAlgorithm(controls.algorithm);
        Controls preparedControls = controls;
        if (controls.algorithm == PlaybackAlgorithm::glitch) {
            preparedControls.motionSound = MotionSound::continuous;
        }
        prepareMotionSoundBlock(preparedControls);
        prepareGrainControls(controls);
        prepareLongGrainControls(controls);
        prepareWavesetControlsBlock(controls, frames, preparedControls);

        std::size_t laneControlCountdown = 0u;
        for (std::size_t frame = 0u; frame < frames; ++frame) {
            for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
                activeControlsLaneGains_[lane] += controlGainSlew_
                    * (targetControlsLaneGains_[lane]
                        - activeControlsLaneGains_[lane]);
            }
            const float recordGain = recordTransitionGain();
            if (recording_ && recordWriteActive_
                && recordInputOffset_ != kResampleInputOffset) {
                if (recordingLane_ < kStackLanes) {
                    recordFrame(input[recordInputOffset_][frame],
                                input[recordInputOffset_ + 1u][frame]);
                } else {
                    recordInputFrame(input, frame);
                }
            }

            if (!playing_ || sourceFrames_ == 0u) {
                for (std::size_t channel = 0u; channel < kChannels; ++channel) {
                    output[channel][frame] = input[channel][frame] * recordGain;
                }
                if (recording_ && recordWriteActive_
                    && recordInputOffset_ == kResampleInputOffset) {
                    recordFrame(output[0u][frame], output[1u][frame]);
                }
                advanceRecordTransition();
                continue;
            }

            constexpr std::size_t laneControlFrames = 48u;
            if (stackMode_ && laneControlCountdown == 0u) {
                laneControlCountdown
                    = std::min(laneControlFrames, frames - frame);
                prepareLaneScanBlock(controls, laneControlCountdown);
            }
            if (stackMode_) {
                advanceLaneWeights();
            }
            speed_ += (targetSpeed - speed_) * speedSlew_;
            join_ += (targetJoin - join_) * joinSlew_;
            stretchRatio_ += (stretchRatioTarget_ - stretchRatio_)
                * stretchRatioSlew_;
            advanceMotionControlSmoothing();
            preparedControls.join = join_;
            const float algorithmGain = algorithmTransitionGain();
            const float retriggerGain = retriggerTransitionGain();
            const float motionSoundGain = motionSoundTransitionGain(controls);
            const float motionSeedGain = motionSeedTransitionGain();
            const float sampleTransition = sampleTransitionGain();
            float wetOutput[kChannels] {};
            switch (currentAlgorithm_) {
                case PlaybackAlgorithm::motion:
                    renderContinuousFrame(
                        controls, join_, false, false, wetOutput);
                    break;
                case PlaybackAlgorithm::lanes:
                    renderContinuousFrame(
                        controls, join_, true, controls.lanesStretch, wetOutput);
                    break;
                case PlaybackAlgorithm::grains:
                    renderGrainsFrame(preparedControls, wetOutput);
                    break;
                case PlaybackAlgorithm::cutups:
                    renderCutupsFrame(preparedControls, wetOutput);
                    break;
                case PlaybackAlgorithm::sample:
                    renderSampleFrame(preparedControls, false, wetOutput);
                    break;
                case PlaybackAlgorithm::slices:
                    renderSampleFrame(preparedControls, true, wetOutput);
                    break;
                case PlaybackAlgorithm::glitch:
                    renderGlitchFrame(controls, join_, wetOutput);
                    break;
                case PlaybackAlgorithm::longGrains:
                    renderLongGrainsFrame(preparedControls, wetOutput);
                    break;
                case PlaybackAlgorithm::wavesets:
                    // Wavesets has its own five-millisecond Depth/Join glide
                    // and consumes discrete changes at cycle boundaries.
                    preparedControls.join = targetJoin;
                    renderWavesetsFrame(preparedControls, wetOutput);
                    break;
            }
            advanceLaneClock();
            if (laneControlCountdown != 0u) {
                --laneControlCountdown;
            }

            for (std::size_t channel = 0u; channel < kChannels; ++channel) {
                float wet = wetOutput[channel] * algorithmGain * retriggerGain
                    * motionSoundGain * motionSeedGain * sampleTransition;
                if (retriggerFadeRemaining_ != 0u) {
                    const float retriggerMix = smoothStep(1.0f
                        - static_cast<float>(retriggerFadeRemaining_)
                            / static_cast<float>(retriggerFadeFrames_));
                    wet *= retriggerMix;
                }
                output[channel][frame]
                    = (input[channel][frame] * (1.0f - dryWet) + wet * dryWet)
                        * recordGain;
            }
            if (recording_ && recordWriteActive_
                && recordInputOffset_ == kResampleInputOffset) {
                recordFrame(output[0u][frame], output[1u][frame]);
            }
            advanceAlgorithmTransition();
            advanceRetriggerTransition();
            advanceMotionSoundTransition();
            advanceMotionSeedTransition();
            advanceSampleTransition();
            advanceRecordTransition();
            if (retriggerFadeRemaining_ != 0u) {
                --retriggerFadeRemaining_;
            }
        }
    }

    float* writableChannel(std::size_t channel)
    {
        return channel < kChannels ? storage_[channel] : nullptr;
    }

    float* writableLane(std::size_t lane, std::size_t channel = 0u)
    {
        return lane < kStackLanes && channel < kLaneChannels
            ? storage_[lane] + channel * laneCapacityFrames_ : nullptr;
    }

    const float* sourceChannel(std::size_t channel) const
    {
        if (stackMode_) {
            return nullptr;
        }
        return channel < sourceChannels_ ? storage_[channel] : nullptr;
    }

    const float* laneData(std::size_t lane, std::size_t channel = 0u) const
    {
        return lane < kStackLanes && channel < kLaneChannels
                && laneFrames_[lane] != 0u
            ? storage_[lane] + channel * laneCapacityFrames_ : nullptr;
    }

    bool setLaneRegions(std::size_t lane,
                        const float* boundaries,
                        std::size_t regionCount)
    {
        if (lane >= kStackLanes || !boundaries || regionCount == 0u
            || regionCount > kRegionCount) return false;
        float previous = 0.0f;
        for (std::size_t index = 0u; index <= regionCount; ++index) {
            const float boundary = clamp01(boundaries[index]);
            laneRegionBoundaries_[lane][index] = std::max(previous, boundary);
            previous = laneRegionBoundaries_[lane][index];
        }
        laneRegionBoundaries_[lane][0u] = 0.0f;
        laneRegionBoundaries_[lane][regionCount] = 1.0f;
        laneRegionCounts_[lane] = regionCount;
        return true;
    }

    void clearLaneRegions(std::size_t lane)
    {
        if (lane >= kStackLanes) return;
        laneRegionCounts_[lane] = 0u;
        laneRegionBoundaries_[lane][0u] = 0.0f;
        laneRegionBoundaries_[lane][1u] = 1.0f;
    }

    std::size_t laneRegionCount(std::size_t lane) const
    {
        return lane < kStackLanes ? laneRegionCounts_[lane] : 0u;
    }

    float laneRegionBoundary(std::size_t lane, std::size_t boundary) const
    {
        if (lane >= kStackLanes || boundary > laneRegionCounts_[lane]) {
            return boundary == 0u ? 0.0f : 1.0f;
        }
        return laneRegionBoundaries_[lane][boundary];
    }

    bool prepared() const { return prepared_; }
    bool playing() const { return playing_; }
    bool recording() const { return recording_; }
    bool laneTransitionActive() const
    {
        return recordTransitionRemaining_ != 0u;
    }
    std::size_t sourceFrames() const { return sourceFrames_; }
    std::size_t sourceChannels() const { return sourceChannels_; }
    float sourceSampleRate() const { return sourceSampleRate_; }
    std::size_t recordedFrames() const { return recordFrames_; }
    std::size_t capacityFrames() const { return capacityFrames_; }
    std::size_t laneCapacityFrames() const { return laneCapacityFrames_; }
    std::size_t lastRecordedLane() const { return lastRecordedLane_; }
    float recordPeak() const { return recordPeak_; }
    float recordMeterPeak() const { return recordMeterPeak_; }
    float lastRecordedPeak() const { return lastRecordedPeak_; }
    bool recordLimited() const { return recordLimited_; }
    bool lastRecordLimited() const { return lastRecordLimited_; }
    bool stackMode() const { return stackMode_; }
    bool laneLoaded(std::size_t lane) const
    {
        return lane < kStackLanes && laneFrames_[lane] != 0u;
    }
    std::size_t laneFrames(std::size_t lane) const
    {
        return lane < kStackLanes ? laneFrames_[lane] : 0u;
    }
    float laneSampleRate(std::size_t lane) const
    {
        return lane < kStackLanes ? laneSampleRates_[lane] : 0.0f;
    }
    std::size_t loadedLaneCount() const
    {
        std::size_t count = 0u;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            if (laneFrames_[lane] != 0u) {
                ++count;
            }
        }
        return count;
    }
    std::uint32_t loadedLaneMask() const
    {
        std::uint32_t mask = 0u;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            if (laneFrames_[lane] != 0u) {
                mask |= 1u << lane;
            }
        }
        return mask;
    }
    float normalizedLanePosition() const { return clamp01(lanePosition_ / 3.0f); }
    float normalizedLaneScanPhase(float phaseOffset = 0.0f) const
    {
        float phase = laneScanPaused_ ? laneFrozenPhase_
            : lanePhase_ + clamp01(phaseOffset);
        while (phase >= 1.0f) phase -= 1.0f;
        while (phase < 0.0f) phase += 1.0f;
        return phase;
    }
    float laneWeight(std::size_t lane) const
    {
        return lane < kStackLanes ? laneWeights_[lane] : 0.0f;
    }

    std::size_t activeGrainVoices() const
    {
        std::size_t active = 0u;
        for (const auto& grain : grains_) {
            if (grain.active) {
                ++active;
            }
        }
        return active;
    }

    float effectiveGrainDensityHz() const { return grainDensityHz_; }

    bool wavesetFallbackActive() const
    {
        return waveset_.initialized && waveset_.fallback;
    }

    float normalizedPosition() const
    {
        return sourceFrames_ == 0u
            ? 0.0f
            : clamp01(static_cast<float>(displayPosition_
                / static_cast<double>(sourceFrames_)));
    }

    float normalizedFieldStart() const
    {
        return sourceFrames_ == 0u
            ? 0.0f
            : clamp01(static_cast<float>(fieldLow_
                / static_cast<double>(sourceFrames_)));
    }

    float normalizedFieldEnd() const
    {
        return sourceFrames_ == 0u
            ? 1.0f
            : clamp01(static_cast<float>(fieldHigh_
                / static_cast<double>(sourceFrames_)));
    }

private:
    enum class LaneTransition : std::uint8_t {
        none,
        remove,
        adopt,
    };

    struct Window {
        double low = 0.0;
        double high = 1.0;
    };

    struct ReadHead {
        double a = 0.0;
        double b = 0.0;
        double position = 0.0;
        double low = 0.0;
        double high = 1.0;
        float mix = 0.0f;
        float gain = 1.0f;
    };

    struct LaneBlend {
        std::size_t first = 0u;
        std::size_t second = 0u;
        float firstGain = 1.0f;
        float secondGain = 0.0f;
        float position = 0.0f;
    };

    struct Grain {
        bool active = false;
        std::size_t age = 0u;
        std::size_t duration = 1u;
        double position = 0.0;
        double increment = 0.0;
        double low = 0.0;
        double high = 1.0;
        double seam = 0.0;
        LaneBlend lanes {};
        float gain = 1.0f;
        float envelopeSkew = 0.0f;
        float channelGains[kLaneChannels] { 1.0f, 1.0f };
        GrainWindow window = GrainWindow::bell;
    };

    struct LongGrain {
        bool active = false;
        std::size_t age = 0u;
        std::size_t duration = 1u;
        std::size_t edgeFrames = 1u;
        double position = 0.0;
        double increment = 0.0;
        double low = 0.0;
        double high = 1.0;
        double seam = 0.0;
        LaneBlend lanes {};
        float gain = 1.0f;
        float channelGains[kLaneChannels] { 1.0f, 1.0f };
        LongGrainShape shape = LongGrainShape::plateau;
    };

    struct CutReader {
        bool active = false;
        bool retiring = false;
        std::size_t age = 0u;
        std::size_t retirementAge = 0u;
        std::size_t joinFrames = 1u;
        double position = 0.0;
        double increment = 0.0;
        double low = 0.0;
        double high = 1.0;
        double seam = 0.0;
        std::size_t lane = kNoLane;
        std::size_t gateFrames = 1u;
        float gain = 1.0f;
    };

    struct MotionSegment {
        bool active = false;
        bool retiring = false;
        std::uint32_t age = 0u;
        std::size_t retirementAge = 0u;
        double phase = 0.0;
        double low = 0.0;
        double high = 1.0;
        double increment = 0.0;
        LaneBlend lanes {};
        float gain = 1.0f;
        bool forward = true;
    };

    struct SampleReaderConfig {
        SampleMode mode = SampleMode::forward;
        RegionMode regionMode = RegionMode::transients;
        std::size_t regionCount = 16u;
        std::size_t slice = 0u;
        std::size_t lane = kNoLane;
        bool sliced = false;
    };

    struct WavesetUnit {
        double start = 0.0;
        double end = 1.0;
        float peak = 0.0f;
    };

    struct WavesetDiscreteControls {
        std::size_t groupSize = 8u;
        std::size_t repeats = 2u;
        std::size_t stride = 1u;
        WavesetAdvance advance = WavesetAdvance::stretch;
        WavesetDirection direction = WavesetDirection::forward;
        WavesetProcess process = WavesetProcess::repeat;
        WavesetDetail detail = WavesetDetail::raw;
    };

    struct WavesetReader {
        bool initialized = false;
        bool fallback = false;
        bool controlsInitialized = false;
        bool pendulumForward = true;
        std::size_t groupSize = 1u;
        std::size_t requestedGroupSize = 1u;
        std::size_t cycleOffset = 0u;
        std::size_t repeatIndex = 0u;
        std::size_t repeats = 2u;
        std::size_t stride = 1u;
        std::size_t processMultiple = 2u;
        std::size_t referenceChannel = 0u;
        std::size_t strongestUnit = 0u;
        std::size_t analysisRetryFrames = 0u;
        double oscillatorPhase = 0.0;
        double groupCursor = 0.0;
        std::uint32_t randomState = 0x6d2b79f5u;
        float processAmount = 0.0f;
        float join = 1.0f;
        WavesetAdvance advance = WavesetAdvance::stretch;
        WavesetDirection direction = WavesetDirection::forward;
        WavesetProcess process = WavesetProcess::repeat;
        WavesetDetail detail = WavesetDetail::raw;
        LaneBlend lanes {};
        WavesetUnit units[32u] {};
        WavesetUnit adjacentUnit {};
        bool adjacentUnitValid = false;
    };

    static constexpr std::size_t kGrainVoiceCount = 16u;
    static constexpr std::size_t kLongGrainVoiceCount = 4u;
    static constexpr std::size_t kCutRegionCount = 64u;
    static constexpr std::size_t kMotionSegmentLayerCount = 4u;
    static constexpr std::size_t kMotionSegmentSlotCount
        = kMotionSegmentLayerCount * 2u;

    static float clamp(float value, float minimum, float maximum)
    {
        return std::max(minimum, std::min(maximum, value));
    }

    static double clamp(double value, double minimum, double maximum)
    {
        return std::max(minimum, std::min(maximum, value));
    }

    static float clamp01(float value) { return clamp(value, 0.0f, 1.0f); }

    static float smoothStep(float value)
    {
        value = clamp01(value);
        return value * value * (3.0f - 2.0f * value);
    }

    static float smoothBell(double phase)
    {
        phase = clamp(phase, 0.0, 1.0);
        const float edge = static_cast<float>(phase < 0.5
            ? phase * 2.0 : (1.0 - phase) * 2.0);
        return smoothStep(edge);
    }

    // The STM32H7 has native single-precision floating point but software
    // double precision. Keep the audio-rate Motion articulation envelopes on
    // the native float path; source addressing remains double precision.
    static float smoothBell(float phase)
    {
        phase = clamp01(phase);
        const float edge = phase < 0.5f
            ? phase * 2.0f : (1.0f - phase) * 2.0f;
        return smoothStep(edge);
    }

    static double wrapPhase(double phase)
    {
        while (phase >= 1.0) {
            phase -= 1.0;
        }
        while (phase < 0.0) {
            phase += 1.0;
        }
        return phase;
    }

    float randomUnit()
    {
        randomState_ ^= randomState_ << 13u;
        randomState_ ^= randomState_ >> 17u;
        randomState_ ^= randomState_ << 5u;
        if (randomState_ == 0u) {
            randomState_ = 0x6d2b79f5u;
        }
        return static_cast<float>(randomState_ & 0x00ffffffu)
            / static_cast<float>(0x01000000u);
    }

    static float hashUnit(std::uint32_t value)
    {
        value ^= value >> 16u;
        value *= 0x7feb352du;
        value ^= value >> 15u;
        value *= 0x846ca68bu;
        value ^= value >> 16u;
        return static_cast<float>(value & 0x00ffffffu)
            / static_cast<float>(0x01000000u);
    }

    void resetEventReaders()
    {
        for (auto& grain : grains_) {
            grain = {};
        }
        for (auto& grain : longGrains_) {
            grain = {};
        }
        grainCountdown_ = 0.0;
        grainSerial_ = 0u;
        grainProcessIndex_ = 0u;
        grainSourcePhase_ = 0.0;
        grainStutterAnchor_ = -1.0;
        grainDoubletPending_ = false;
        grainDoubletPosition_ = 0.0;
        longGrainCountdown_ = 0.0;
        longGrainSerial_ = 0u;
        longGrainSourcePhase_ = 0.0;
        sorterCacheValid_ = false;
        stretchPhase_ = 0.0;
        stretchMix_ = 0.0f;
        cutCurrent_ = {};
        cutTail_ = {};
        cutCountdown_ = 0.0;
        cutStep_ = 0u;
        cutWalkRegion_ = 0u;
        cutRepeatIndex_ = 0u;
        cutRepeatedRegion_ = 0u;
        cutSwingPhase_ = false;
        glitchWriteIndex_ = 0u;
        glitchHistoryFrames_ = 0u;
        glitchReadStart_ = 0u;
        glitchSliceFrames_ = 0u;
        glitchRepeatIndex_ = 0u;
        glitchRepeatLimit_ = 0u;
        glitchStep_ = 0u;
        glitchCycle_ = 0u;
        glitchCountdown_ = 0.0;
        glitchReadPhase_ = 0.0;
        glitchReadIncrement_ = 1.0;
        glitchActive_ = false;
        glitchTailActive_ = false;
        glitchTailAge_ = 0u;
        glitchControlInitialized_ = false;
        waveset_ = {};
        wavesetControlTransitionFrom_ = {};
        wavesetControlTransitionRemaining_ = 0u;
        for (auto& segment : motionSegments_) {
            segment = {};
        }
        motionSegmentAge_ = 0u;
        motionEventPhase_ = 0.0;
        motionEventRepeatIndex_ = 0u;
        motionEventBasePosition_ = 0.5;
        motionEventRateScale_ = 1.0f;
        motionEventStarted_ = false;
        motionPacketEvent_ = false;
        motionTurnEvent_ = false;
        motionPathForward_ = true;
        motionRandomCycle_ = 0u;
        sampleDone_ = false;
        sampleConfigInitialized_ = false;
        sampleConfigPending_ = false;
        sampleTransitionRemaining_ = 0u;
        randomState_ = 0x51f15e5du;
        currentMotionSound_ = MotionSound::continuous;
        pendingMotionSound_ = MotionSound::continuous;
        motionSoundChangePending_ = false;
        motionSoundTransitionRemaining_ = 0u;
        motionInnerPhase_ = 0.0;
        motionOuterPhase_ = 0.5;
        motionSoundPhaseIncrement_ = 0.0f;
        motionSoundPhaseIncrementTarget_ = 0.0f;
        motionMotorPhaseIncrement_ = 0.0f;
        motionMotorPhaseIncrementTarget_ = 0.0f;
        motionPacketDuty_ = 0.65f;
        motionPacketDutyTarget_ = 0.65f;
        motionMotorSymmetry_ = 0.5f;
        motionMotorSymmetryTarget_ = 0.5f;
        motionTravel_ = 1.0f;
        motionTravelTarget_ = 1.0f;
        motionJitter_ = 0.0f;
        motionJitterTarget_ = 0.0f;
        motionMotorShape_ = MotorShape::rounded;
        motionInnerRateScale_ = 1.0f;
        motionSeed_ = 1u;
        pendingMotionSeed_ = 1u;
        motionSeedPending_ = false;
        motionSeedTransitionRemaining_ = 0u;
        motionControlsInitialized_ = false;
        motionEnvelope_ = 1.0f;
    }

    void activateLaneRecording()
    {
        if (!recording_ || recordingLane_ >= kStackLanes
            || recordWriteActive_) {
            return;
        }

        const std::size_t lane = recordingLane_;
        laneFrames_[lane] = 0u;
        if (referenceLane_ >= kStackLanes || referenceLane_ == lane
            || laneFrames_[referenceLane_] == 0u) {
            referenceLane_ = kNoLane;
            for (std::size_t candidate = 0u;
                 candidate < kStackLanes;
                 ++candidate) {
                if (laneFrames_[candidate] != 0u) {
                    referenceLane_ = candidate;
                    break;
                }
            }
        }

        if (referenceLane_ < kStackLanes) {
            sourceFrames_ = laneFrames_[referenceLane_];
            sourceSampleRate_ = laneSampleRates_[referenceLane_];
            playing_ = sourceFrames_ != 0u;
        } else {
            sourceFrames_ = 0u;
            sourceSampleRate_ = outputSampleRate_;
            playing_ = false;
        }
        sourceChannels_ = kLaneChannels;
        prepareWavesetAnalysisFilters();
        updateLanePositionScales();
        laneWeightsInitialized_ = false;
        windowInitialized_ = false;
        eventWindowInitialized_ = false;
        movingCenterInitialized_ = false;
        lastControlLocus_ = -1.0f;
        resetEventReaders();
        recordWriteActive_ = true;
    }

    void refreshStackAfterLaneChange()
    {
        if (referenceLane_ >= kStackLanes
            || laneFrames_[referenceLane_] == 0u) {
            referenceLane_ = kNoLane;
            for (std::size_t candidate = 0u;
                 candidate < kStackLanes;
                 ++candidate) {
                if (laneFrames_[candidate] != 0u) {
                    referenceLane_ = candidate;
                    break;
                }
            }
        }

        if (referenceLane_ < kStackLanes) {
            sourceFrames_ = laneFrames_[referenceLane_];
            sourceSampleRate_ = laneSampleRates_[referenceLane_];
            playing_ = sourceFrames_ != 0u;
        } else {
            sourceFrames_ = 0u;
            sourceSampleRate_ = outputSampleRate_;
            playing_ = false;
        }
        sourceChannels_ = kLaneChannels;
        stackMode_ = true;
        prepareWavesetAnalysisFilters();
        updateLanePositionScales();
        laneWeightsInitialized_ = false;
        windowInitialized_ = false;
        eventWindowInitialized_ = false;
        movingCenterInitialized_ = false;
        lastControlLocus_ = -1.0f;
        resetEventReaders();
    }

    void completeLaneTransition()
    {
        if (transitionLane_ >= kStackLanes) {
            laneTransition_ = LaneTransition::none;
            return;
        }

        if (laneTransition_ == LaneTransition::remove) {
            laneFrames_[transitionLane_] = 0u;
            laneSampleRates_[transitionLane_] = outputSampleRate_;
            clearLaneRegions(transitionLane_);
            refreshStackAfterLaneChange();
        } else if (laneTransition_ == LaneTransition::adopt) {
            const bool hadPlayback = playing_ && sourceFrames_ != 0u;
            laneFrames_[transitionLane_] = transitionFrames_;
            laneSampleRates_[transitionLane_] = transitionSampleRate_;
            if (referenceLane_ >= kStackLanes
                || laneFrames_[referenceLane_] == 0u) {
                referenceLane_ = transitionLane_;
            }
            refreshStackAfterLaneChange();
            if (!hadPlayback) {
                phase_ = 0.0;
                displayPosition_ = 0.0;
            }
        }
        laneTransition_ = LaneTransition::none;
        transitionLane_ = kNoLane;
        transitionFrames_ = 0u;
    }

    float recordTransitionGain()
    {
        if (recordTransitionRemaining_ == 0u) {
            return 1.0f;
        }
        if (recordTransitionRemaining_ == recordTransitionFrames_) {
            if (laneTransition_ == LaneTransition::none) {
                activateLaneRecording();
            } else {
                completeLaneTransition();
            }
        }
        if (recordTransitionRemaining_ > recordTransitionFrames_) {
            return smoothStep(static_cast<float>(
                recordTransitionRemaining_ - recordTransitionFrames_)
                / static_cast<float>(recordTransitionFrames_));
        }
        return smoothStep(1.0f
            - static_cast<float>(recordTransitionRemaining_)
                / static_cast<float>(recordTransitionFrames_));
    }

    void advanceRecordTransition()
    {
        if (recordTransitionRemaining_ != 0u) {
            --recordTransitionRemaining_;
        }
    }

    void recordFrame(float left, float right)
    {
        const std::size_t recordCapacity = recordingLane_ < kStackLanes
            ? laneCapacityFrames_ : capacityFrames_;
        if (recordFrames_ < recordCapacity) {
            if (recordingLane_ < kStackLanes) {
                left = std::isfinite(left) ? left : 0.0f;
                right = std::isfinite(right) ? right : 0.0f;
                const float inputPeak = std::max(
                    std::abs(left), std::abs(right));
                recordPeak_ = std::max(recordPeak_, inputPeak);
                recordMeterPeak_ = std::max(
                    inputPeak, recordMeterPeak_ * recordMeterRelease_);
                left *= recordInputGain_;
                right *= recordInputGain_;
                const float storedPeak = std::max(
                    std::abs(left), std::abs(right));
                constexpr float captureCeiling = 0.8912509381337456f;
                const float targetGain = storedPeak > captureCeiling
                    ? captureCeiling / storedPeak : 1.0f;
                if (targetGain < 1.0f) {
                    recordLimiterGain_ = std::min(
                        recordLimiterGain_, targetGain);
                    recordLimited_ = true;
                } else {
                    recordLimiterGain_ += (1.0f - recordLimiterGain_)
                        * recordLimiterRelease_;
                }
                left *= recordLimiterGain_;
                right *= recordLimiterGain_;
                writableLane(recordingLane_, 0u)[recordFrames_] = left;
                writableLane(recordingLane_, 1u)[recordFrames_] = right;
            } else {
                storage_[0u][recordFrames_] = left;
                if (recordChannels_ > 1u) {
                    storage_[1u][recordFrames_] = right;
                }
            }
            ++recordFrames_;
        }
        if (recordFrames_ >= recordCapacity) {
            endRecording();
        }
    }

    void recordInputFrame(const float* const* input, std::size_t frame)
    {
        if (recordFrames_ < capacityFrames_) {
            for (std::size_t channel = 0u;
                 channel < recordChannels_;
                 ++channel) {
                storage_[channel][recordFrames_] = input[channel][frame];
            }
            ++recordFrames_;
        }
        if (recordFrames_ >= capacityFrames_) {
            endRecording();
        }
    }

    void resetPlaybackForAlgorithm()
    {
        phase_ = 0.0;
        windowFadeRemaining_ = 0u;
        windowInitialized_ = false;
        eventWindowInitialized_ = false;
        movingCenterInitialized_ = false;
        lastControlLocus_ = -1.0f;
        laneWeightsInitialized_ = false;
        resetEventReaders();
    }

    void requestAlgorithm(PlaybackAlgorithm algorithm)
    {
        if (algorithm == currentAlgorithm_ && algorithmFadeRemaining_ == 0u) {
            pendingAlgorithm_ = algorithm;
            return;
        }
        if (algorithm != pendingAlgorithm_ || algorithmFadeRemaining_ == 0u) {
            pendingAlgorithm_ = algorithm;
            if (pendingAlgorithm_ != currentAlgorithm_) {
                algorithmFadeRemaining_ = algorithmFadeFrames_ * 2u;
            }
        }
    }

    float algorithmTransitionGain()
    {
        if (algorithmFadeRemaining_ == 0u) {
            return 1.0f;
        }
        if (algorithmFadeRemaining_ == algorithmFadeFrames_) {
            currentAlgorithm_ = pendingAlgorithm_;
            resetPlaybackForAlgorithm();
        }
        if (algorithmFadeRemaining_ > algorithmFadeFrames_) {
            return smoothStep(static_cast<float>(
                algorithmFadeRemaining_ - algorithmFadeFrames_)
                / static_cast<float>(algorithmFadeFrames_));
        }
        return smoothStep(1.0f - static_cast<float>(algorithmFadeRemaining_)
            / static_cast<float>(algorithmFadeFrames_));
    }

    void advanceAlgorithmTransition()
    {
        if (algorithmFadeRemaining_ != 0u) {
            --algorithmFadeRemaining_;
        }
    }

    float retriggerTransitionGain()
    {
        if (retriggerTransitionRemaining_ == 0u) {
            return 1.0f;
        }
        if (retriggerTransitionRemaining_ == retriggerTransitionFrames_) {
            // Move every continuous or event reader only when the outgoing
            // signal has reached silence. The second half then reveals the
            // new loop start without interpolating from a held DC sample.
            if (seekPending_) {
                phase_ = pendingSeekPhase_;
                resetEventReaders();
                sampleDone_ = false;
                seekPending_ = false;
            } else {
                // A true note retrigger restarts every reader and stack phase,
                // but resetReader() also clears this transition. Preserve its
                // midpoint so the new start is faded back in.
                const std::size_t transition = retriggerTransitionRemaining_;
                resetReader();
                retriggerTransitionRemaining_ = transition;
            }
        }
        if (retriggerTransitionRemaining_ > retriggerTransitionFrames_) {
            return smoothStep(static_cast<float>(
                retriggerTransitionRemaining_ - retriggerTransitionFrames_)
                / static_cast<float>(retriggerTransitionFrames_));
        }
        return smoothStep(1.0f
            - static_cast<float>(retriggerTransitionRemaining_)
                / static_cast<float>(retriggerTransitionFrames_));
    }

    void advanceRetriggerTransition()
    {
        if (retriggerTransitionRemaining_ != 0u) {
            --retriggerTransitionRemaining_;
        }
    }

    void resetReader()
    {
        phase_ = 0.0;
        lanePhase_ = 0.0f;
        lanePhaseIncrement_ = 0.0f;
        laneFrozenPhase_ = 0.0f;
        laneScanPaused_ = false;
        speed_ = 1.0f;
        displayPosition_ = 0.0;
        fieldLow_ = 0.0;
        fieldHigh_ = static_cast<double>(sourceFrames_);
        windowFadeRemaining_ = 0u;
        retriggerFadeRemaining_ = 0u;
        retriggerTransitionRemaining_ = 0u;
        pendingSeekPhase_ = 0.0;
        seekPending_ = false;
        windowInitialized_ = false;
        eventWindowInitialized_ = false;
        movingCenterInitialized_ = false;
        lastControlLocus_ = -1.0f;
        laneWeightsInitialized_ = false;
        resetEventReaders();
        for (std::size_t channel = 0u; channel < kChannels; ++channel) {
            laneWeights_[channel] = 0.0f;
            laneWeightTargets_[channel] = 0.0f;
        }
    }

    Window controlWindow(const Controls& controls) const
    {
        const double frames = static_cast<double>(sourceFrames_);
        if (!(frames > 1.0)) {
            return { 0.0, std::max(1.0, frames) };
        }
        const double minimumWidth = std::min(frames, 4.0);
        const double width = clamp(
            static_cast<double>(clamp01(controls.field)) * frames,
            minimumWidth, frames);
        const double half = width * 0.5;
        const double center = clamp(
            static_cast<double>(clamp01(controls.locus)) * frames,
            half, frames - half);
        return { center - half, center + half };
    }

    Window smoothEventWindow(const Controls& controls)
    {
        const Window target = controlWindow(controls);
        if (!eventWindowInitialized_) {
            eventWindow_ = target;
            eventWindowInitialized_ = true;
        } else {
            eventWindow_.low += static_cast<double>(eventWindowSlew_)
                * (target.low - eventWindow_.low);
            eventWindow_.high += static_cast<double>(eventWindowSlew_)
                * (target.high - eventWindow_.high);
        }
        return eventWindow_;
    }

    Window requestedWindow(const Controls& controls) const
    {
        Window window = controlWindow(controls);
        if (controls.motion != MotionMode::movingLoop) {
            return window;
        }
        const double frames = static_cast<double>(sourceFrames_);
        const double width = window.high - window.low;
        const double half = width * 0.5;
        const double center = clamp(movingCenterTarget_, half, frames - half);
        return { center - half, center + half };
    }

    void updateMovingControl(const Controls& controls)
    {
        requestedMode_ = controls.motion;
        if (controls.motion != MotionMode::movingLoop || sourceFrames_ == 0u) {
            movingCenterInitialized_ = false;
            lastControlLocus_ = clamp01(controls.locus);
            return;
        }

        const float locus = clamp01(controls.locus);
        if (!movingCenterInitialized_
            || std::abs(locus - lastControlLocus_) >= 0.005f) {
            movingCenterTarget_ = static_cast<double>(locus)
                * static_cast<double>(sourceFrames_);
            movingCenterInitialized_ = true;
            lastControlLocus_ = locus;
        }
    }

    void stepMovingLoop(double width, float normalizedStep)
    {
        const double frames = static_cast<double>(sourceFrames_);
        width = clamp(width, std::min(4.0, frames), frames);
        const double half = width * 0.5;
        const double minimum = half;
        const double maximum = frames - half;
        const double range = maximum - minimum;
        if (!(range > 1.0)) {
            movingCenterTarget_ = frames * 0.5;
            return;
        }

        const double step = std::max(1.0,
            static_cast<double>(sourceFrames_)
                * static_cast<double>(clamp01(normalizedStep)));
        double offset = std::fmod(
            movingCenterTarget_ + step - minimum, range);
        if (offset < 0.0) {
            offset += range;
        }
        movingCenterTarget_ = minimum + offset;
    }

    void beginWindowFadeIfNeeded(const Window& requested, MotionMode mode)
    {
        if (!windowInitialized_) {
            currentWindow_ = requested;
            currentMode_ = mode;
            fromWindow_ = requested;
            toWindow_ = requested;
            fromMode_ = mode;
            toMode_ = mode;
            windowInitialized_ = true;
            fieldLow_ = requested.low;
            fieldHigh_ = requested.high;
            return;
        }
        if (windowFadeRemaining_ != 0u) {
            return;
        }
        const double deadband = std::max(1.0,
            static_cast<double>(sourceFrames_) * 0.0005);
        if (mode == currentMode_
            && std::abs(requested.low - currentWindow_.low) < deadband
            && std::abs(requested.high - currentWindow_.high) < deadband) {
            return;
        }

        fromWindow_ = currentWindow_;
        fromMode_ = currentMode_;
        toWindow_ = requested;
        toMode_ = mode;
        windowFadeRemaining_ = windowFadeFrames_;
    }

    void followStretchWindow(const Window& requested, MotionMode mode)
    {
        if (!windowInitialized_) {
            currentWindow_ = requested;
            currentMode_ = mode;
            fromWindow_ = requested;
            toWindow_ = requested;
            fromMode_ = mode;
            toMode_ = mode;
            windowInitialized_ = true;
            fieldLow_ = requested.low;
            fieldHigh_ = requested.high;
            return;
        }
        if (windowFadeRemaining_ != 0u) {
            // Mode changes retain the old/new render crossfade, but the target
            // window follows the smoothed knobs so it cannot jump at the end.
            toWindow_ = requested;
            toMode_ = mode;
            return;
        }
        if (mode != currentMode_) {
            fromWindow_ = currentWindow_;
            fromMode_ = currentMode_;
            toWindow_ = requested;
            toMode_ = mode;
            windowFadeRemaining_ = windowFadeFrames_;
            return;
        }
        // Live Start/End motion is already one-pole smoothed. Following it
        // directly avoids doubling the expensive stretch renderer for every
        // small analog-knob update.
        currentWindow_ = requested;
        fromWindow_ = requested;
        toWindow_ = requested;
    }

    double joinFrames(const Window& window, float join) const
    {
        const double width = std::max(1.0, window.high - window.low);
        if (width < 8.0) {
            return 0.0;
        }
        const double maximum = std::max(0.0, std::min(width * 0.08,
            static_cast<double>(sourceSampleRate_) * 0.020));
        const double safety = std::min(maximum,
            static_cast<double>(safetyFadeFrames_)
                * static_cast<double>(sourceSampleRate_)
                / static_cast<double>(outputSampleRate_));
        return std::max(safety,
            maximum * static_cast<double>(clamp01(join)));
    }

    double pathFrames(const Window& window, MotionMode mode, float join) const
    {
        const double width = std::max(1.0, window.high - window.low);
        if (mode == MotionMode::forward || mode == MotionMode::reverse
            || mode == MotionMode::movingLoop) {
            return std::max(1.0, width - joinFrames(window, join));
        }
        return std::max(1.0, width * 2.0);
    }

    bool motionTrajectoryForward(MotionMode mode, double phase) const
    {
        phase = wrapPhase(phase);
        if (mode == MotionMode::reverse) return false;
        if (mode == MotionMode::hover || mode == MotionMode::roundTrip
            || mode == MotionMode::mirror) {
            return phase < 0.5;
        }
        if (mode == MotionMode::zigzag) {
            if (motionTravel_ <= 1.0e-6f) return true;
            // Fixed temporal thirds keep direction stable while Travel and
            // Jitter move the targets. Distance-derived boundaries crossed
            // the current phase during a live edit and could jump between
            // path segments even though the controls themselves were slewed.
            return phase < (1.0 / 3.0) || phase >= (2.0 / 3.0);
        }
        if (mode == MotionMode::drunk) {
            if (motionTravel_ <= 1.0e-6f) return true;
            constexpr std::size_t segmentCount = 8u;
            const std::size_t segment = std::min<std::size_t>(
                static_cast<std::size_t>(phase
                    * static_cast<double>(segmentCount)),
                segmentCount - 1u);
            const std::uint32_t globalSegment = motionRandomCycle_
                * static_cast<std::uint32_t>(segmentCount)
                + static_cast<std::uint32_t>(segment);
            const float from = hashUnit(
                motionSeed_ + globalSegment + 0x45d9f3bu);
            const float to = hashUnit(
                motionSeed_ + globalSegment + 1u + 0x45d9f3bu);
            return to >= from;
        }
        return true;
    }

    double zigzagPosition(const Window& window,
                          double phase,
                          std::uint32_t cycle) const
    {
        const std::uint32_t cycleSeed = motionSeed_
            + cycle * 0x9e3779b9u;
        const double reach = 0.5 * static_cast<double>(motionTravel_);
        const double highTarget = 0.5 + reach * (1.0
            - static_cast<double>(motionJitter_) * 0.8
                * hashUnit(cycleSeed + 0x7f4a7c15u));
        const double lowTarget = 0.5 - reach * (1.0
            - static_cast<double>(motionJitter_) * 0.8
                * hashUnit(cycleSeed + 0x94d049bbu));
        constexpr double firstBoundary = 1.0 / 3.0;
        constexpr double secondBoundary = 2.0 / 3.0;
        double from = 0.0;
        double to = highTarget;
        double local = phase / firstBoundary;
        if (phase >= secondBoundary) {
            from = lowTarget;
            to = 1.0;
            local = (phase - secondBoundary) / (1.0 - secondBoundary);
        } else if (phase >= firstBoundary) {
            from = highTarget;
            to = lowTarget;
            local = (phase - firstBoundary)
                / (secondBoundary - firstBoundary);
        }
        const double shaped = from + (to - from)
            * static_cast<double>(smoothStep(static_cast<float>(local)));
        return window.low + shaped * std::max(1.0, window.high - window.low);
    }

    ReadHead makeReadHead(const Window& window,
                          MotionMode mode,
                          double phase,
                          float join) const
    {
        ReadHead result;
        result.low = window.low;
        result.high = window.high;
        const double width = std::max(1.0, window.high - window.low);

        if (mode == MotionMode::hover) {
            // A globally eased out-and-back path retains Hover's long pauses
            // at the field edges without an audio-rate trigonometric call.
            const float palindrome = static_cast<float>(phase < 0.5
                ? phase * 2.0 : 2.0 - phase * 2.0);
            result.a = window.low
                + static_cast<double>(smoothStep(palindrome)) * width;
            result.b = result.a;
            result.position = result.a;
            return result;
        }
        if (mode == MotionMode::roundTrip || mode == MotionMode::mirror) {
            // Retain the constant-speed palindrome over almost the entire
            // field, easing only the final 5 ms at each reversal. This avoids
            // the triangular path's one-sample velocity flip without turning
            // Round Trip into the globally slowed Hover trajectory.
            double palindrome = phase < 0.5
                ? phase * 2.0 : 2.0 - phase * 2.0;
            const double turnFrames = std::min(
                width * 0.05,
                static_cast<double>(sourceSampleRate_) * 0.005);
            const double margin = clamp(turnFrames / width, 0.0, 0.05);
            if (margin > 0.0 && palindrome < margin) {
                const double x = palindrome / margin;
                palindrome = margin * x * x * (2.0 - x);
            } else if (margin > 0.0 && palindrome > 1.0 - margin) {
                const double x = (1.0 - palindrome) / margin;
                palindrome = 1.0 - margin * x * x * (2.0 - x);
            }
            result.a = window.low + palindrome * width;
            result.b = result.a;
            result.position = result.a;
            if (mode == MotionMode::mirror) {
                const float polarity = phase < 0.5 ? 1.0f : -1.0f;
                // Reuse half the already-computed turn margin. The previous
                // double-precision sine here caused short CPU overruns at
                // every polarity reversal on the Cortex-M7. A smoothstep
                // reaches the same zero-gain boundary without libm work.
                const float transition = std::max(
                    static_cast<float>(margin) * 0.5f,
                    clamp01(join) * 0.04f);
                const float distance = static_cast<float>(std::min({
                    phase,
                    std::abs(phase - 0.5),
                    1.0 - phase,
                }));
                if (distance < transition) {
                    result.gain = polarity
                        * smoothStep(distance / transition);
                } else {
                    result.gain = polarity;
                }
            }
            return result;
        }
        if (mode == MotionMode::zigzag) {
            result.a = zigzagPosition(window, phase, motionRandomCycle_);
            result.b = result.a;
            result.position = result.a;
            const double transition = clamp(
                std::max(joinFrames(window, join) / width,
                    std::abs(lastMotionPhaseIncrement_)
                        * static_cast<double>(safetyFadeFrames_)),
                1.0 / width, 0.45);
            if (phase >= 1.0 - transition) {
                const double local = clamp(
                    (phase - (1.0 - transition)) / transition,
                    0.0, 1.0);
                result.b = zigzagPosition(
                    window, local * transition, motionRandomCycle_ + 1u);
                result.mix = smoothStep(static_cast<float>(local));
                result.position = result.a
                    + (result.b - result.a) * result.mix;
            }
            return result;
        }
        if (mode == MotionMode::drunk) {
            constexpr std::size_t segmentCount = 8u;
            const double scaled = phase * static_cast<double>(segmentCount);
            const std::size_t segment = std::min<std::size_t>(
                static_cast<std::size_t>(scaled), segmentCount - 1u);
            const float local = static_cast<float>(scaled
                - static_cast<double>(segment));
            const std::uint32_t globalSegment = motionRandomCycle_
                * static_cast<std::uint32_t>(segmentCount)
                + static_cast<std::uint32_t>(segment);
            const float fromRandom = hashUnit(
                motionSeed_ + globalSegment + 0x45d9f3bu);
            const float toRandom = hashUnit(
                motionSeed_ + globalSegment + 1u + 0x45d9f3bu);
            const float center = 0.5f;
            const float from = center
                + (fromRandom - center) * motionTravel_;
            const float to = center + (toRandom - center) * motionTravel_;
            const float jitter = (hashUnit(motionSeed_ + globalSegment
                + 0x27d4eb2du) - 0.5f) * motionJitter_ * 0.24f;
            const float shaped = smoothStep(clamp01(local + jitter));
            result.a = window.low + static_cast<double>(
                clamp01(from + (to - from) * shaped)) * width;
            result.b = result.a;
            result.position = result.a;
            return result;
        }

        const double crossfade = joinFrames(window, join);
        const double cycle = std::max(1.0, width - crossfade);
        const double travel = phase * cycle;
        const bool reverse = mode == MotionMode::reverse;
        result.a = reverse
            ? window.high - crossfade - travel
            : window.low + crossfade + travel;
        result.b = result.a;
        result.position = result.a;

        if (crossfade > 0.0 && travel >= width - 2.0 * crossfade) {
            const double local = clamp(
                (travel - (width - 2.0 * crossfade)) / crossfade,
                0.0, 1.0);
            result.b = reverse
                ? window.high - local * crossfade
                : window.low + local * crossfade;
            result.mix = smoothStep(static_cast<float>(local));
            result.position = result.a + (result.b - result.a) * result.mix;
        }
        return result;
    }

    static float interpolatedSample(const float* data,
                                    std::size_t frames,
                                    double position)
    {
        if (!data || frames == 0u) return 0.0f;
        position = clamp(position, 0.0,
            static_cast<double>(frames - 1u));
        const std::size_t first = static_cast<std::size_t>(position);
        const std::size_t previous = first == 0u ? 0u : first - 1u;
        const std::size_t second = std::min(first + 1u, frames - 1u);
        const std::size_t third = std::min(first + 2u, frames - 1u);
        const float phase = static_cast<float>(
            position - static_cast<double>(first));
        const float y0 = data[previous];
        const float y1 = data[first];
        const float y2 = data[second];
        const float y3 = data[third];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        const float value = ((c3 * phase + c2) * phase + c1) * phase + y1;
        // Bound the Hermite curve to its adjacent samples. This preserves the
        // smoother fractional read without creating new inter-sample peaks.
        return clamp(value, std::min(y1, y2), std::max(y1, y2));
    }

    float sampleAt(std::size_t sourceChannel, double position) const
    {
        return interpolatedSample(
            storage_[sourceChannel], sourceFrames_, position);
    }

    float readHead(std::size_t sourceChannel, const ReadHead& head) const
    {
        const float a = sampleAt(sourceChannel, head.a);
        if (head.mix <= 0.0f) {
            return a * head.gain;
        }
        return (a + (sampleAt(sourceChannel, head.b) - a) * head.mix)
            * head.gain;
    }

    float sampleLaneAt(std::size_t lane,
                       std::size_t channel,
                       double referencePosition) const
    {
        if (lane >= kStackLanes || channel >= kLaneChannels
            || laneFrames_[lane] == 0u
            || sourceFrames_ == 0u) {
            return 0.0f;
        }
        const double position = clamp(referencePosition
                * lanePositionScales_[lane],
            0.0, static_cast<double>(laneFrames_[lane] - 1u));
        return interpolatedSample(
            laneData(lane, channel), laneFrames_[lane], position);
    }

    float readLaneHead(std::size_t lane,
                       std::size_t channel,
                       const ReadHead& head) const
    {
        const float a = sampleLaneAt(lane, channel, head.a);
        if (head.mix <= 0.0f) {
            return a * head.gain;
        }
        return (a + (sampleLaneAt(lane, channel, head.b) - a) * head.mix)
            * head.gain;
    }

    void updateLanePositionScales()
    {
        const double referenceRange = sourceFrames_ > 1u
            ? static_cast<double>(sourceFrames_ - 1u) : 1.0;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            lanePositionScales_[lane] = laneFrames_[lane] > 1u
                ? static_cast<double>(laneFrames_[lane] - 1u) / referenceRange
                : 0.0;
        }
    }

    LaneBlend makeLaneBlend(float phase, const Controls& controls) const
    {
        LaneBlend result;
        phase += clamp(controls.lanePathOffset, 0.0f, 1.0f);
        while (phase >= 1.0f) {
            phase -= 1.0f;
        }
        while (phase < 0.0f) {
            phase += 1.0f;
        }
        const float skew = clamp(controls.lanePathSkew, 0.02f, 0.98f);
        phase = phase < skew
            ? 0.5f * phase / skew
            : 0.5f + 0.5f * (phase - skew) / (1.0f - skew);
        const float curve = clamp(controls.lanePathCurve, -1.0f, 1.0f);
        if (curve > 0.0f) {
            phase = phase + (phase * phase - phase) * curve;
        } else if (curve < 0.0f) {
            const float inverse = 1.0f - (1.0f - phase) * (1.0f - phase);
            phase = phase + (inverse - phase) * -curve;
        }
        float unit = phase;
        switch (controls.lanePath) {
            case LanePath::up:
                unit = 1.0f - phase;
                break;
            case LanePath::triangle:
                unit = 1.0f - std::abs(2.0f * phase - 1.0f);
                break;
            case LanePath::sine:
                // Smooth triangle: cosine-like zero-slope turns without a
                // transcendental call at control-block rate.
                unit = smoothStep(
                    1.0f - std::abs(2.0f * phase - 1.0f));
                break;
            case LanePath::stepsDown:
                unit = static_cast<float>(std::min(3,
                    static_cast<int>(std::floor(phase * 4.0f)))) / 3.0f;
                break;
            case LanePath::stepsUp:
                unit = 1.0f - static_cast<float>(std::min(3,
                    static_cast<int>(std::floor(phase * 4.0f)))) / 3.0f;
                break;
            case LanePath::manual:
                unit = clamp(controls.manualLane, 0.0f, 3.0f) / 3.0f;
                break;
            case LanePath::random: {
                const std::uint32_t step = static_cast<std::uint32_t>(
                    std::floor(phase * 16.0f));
                unit = hashUnit(step + 0x9e3779b9u);
                break;
            }
            case LanePath::down:
                break;
        }
        switch (controls.lanePathShape) {
            case LanePathShape::smooth:
                unit = smoothStep(unit);
                break;
            case LanePathShape::exponential:
                unit *= unit;
                break;
            case LanePathShape::plateau: {
                const float stepped = std::floor(unit * 4.0f) / 3.0f;
                unit = clamp01(stepped);
                break;
            }
            case LanePathShape::linear:
                break;
        }
        result.position = unit * 3.0f;

        int lower = -1;
        int upper = -1;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            if (laneFrames_[lane] == 0u) {
                continue;
            }
            if (static_cast<float>(lane) <= result.position) {
                lower = static_cast<int>(lane);
            }
            if (upper < 0 && static_cast<float>(lane) >= result.position) {
                upper = static_cast<int>(lane);
            }
        }
        if (lower < 0) {
            lower = upper;
        }
        if (upper < 0) {
            upper = lower;
        }
        if (lower < 0 || upper < 0) {
            return result;
        }

        result.first = static_cast<std::size_t>(lower);
        result.second = static_cast<std::size_t>(upper);
        if (lower == upper || controls.laneBlend == LaneBlendMode::jump) {
            if (controls.laneBlend == LaneBlendMode::jump && lower != upper) {
                const bool chooseUpper = result.position
                    >= 0.5f * static_cast<float>(lower + upper);
                result.first = chooseUpper
                    ? static_cast<std::size_t>(upper)
                    : static_cast<std::size_t>(lower);
                result.second = result.first;
                result.position = static_cast<float>(result.first);
            }
            return result;
        }
        const float mix = smoothStep(clamp01(
            (result.position - static_cast<float>(lower))
                / static_cast<float>(upper - lower)));
        result.firstGain = 1.0f - mix;
        result.secondGain = mix;
        return result;
    }

    void prepareLaneScanBlock(const Controls& controls, std::size_t frames)
    {
        if (!stackMode_) {
            lanePhaseIncrement_ = 0.0f;
            return;
        }
        const float phaseOffset = clamp(controls.lanePhaseOffset, 0.0f, 1.0f);
        if (controls.laneScanPaused && controls.lanePhaseOverride) {
            laneFrozenPhase_ = clamp01(controls.lanePhaseOverrideValue);
            lanePhase_ = laneFrozenPhase_ - phaseOffset;
            while (lanePhase_ >= 1.0f) lanePhase_ -= 1.0f;
            while (lanePhase_ < 0.0f) lanePhase_ += 1.0f;
        } else if (controls.laneScanPaused && !laneScanPaused_) {
            laneFrozenPhase_ = lanePhase_ + phaseOffset;
            while (laneFrozenPhase_ >= 1.0f) laneFrozenPhase_ -= 1.0f;
            while (laneFrozenPhase_ < 0.0f) laneFrozenPhase_ += 1.0f;
        } else if (!controls.laneScanPaused && laneScanPaused_) {
            // Resume from the audible held blend even if pressure changed
            // while the scan was paused.
            lanePhase_ = laneFrozenPhase_ - phaseOffset;
            while (lanePhase_ >= 1.0f) lanePhase_ -= 1.0f;
            while (lanePhase_ < 0.0f) lanePhase_ += 1.0f;
        }
        laneScanPaused_ = controls.laneScanPaused;
        lanePhaseIncrement_ = laneScanPaused_ ? 0.0f : clamp(
            controls.laneCycles, 0.125f, 16.0f) / outputSampleRate_;
        float targetPhase = laneScanPaused_ ? laneFrozenPhase_ : lanePhase_
            + lanePhaseIncrement_ * static_cast<float>(frames) + phaseOffset;
        while (targetPhase >= 1.0f) {
            targetPhase -= 1.0f;
        }
        const LaneBlend lanes = makeLaneBlend(targetPhase, controls);
        const std::size_t firstLane = std::min(lanes.first, kStackLanes - 1u);
        const std::size_t secondLane = std::min(lanes.second, kStackLanes - 1u);
        lanePosition_ = lanes.position;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            laneWeightTargets_[lane] = 0.0f;
        }
        laneWeightTargets_[firstLane] += lanes.firstGain;
        laneWeightTargets_[secondLane] += lanes.secondGain;
        if (!laneWeightsInitialized_) {
            for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
                laneWeights_[lane] = laneWeightTargets_[lane];
            }
            laneWeightsInitialized_ = true;
            return;
        }

        // Correct only once per block for accumulated floating-point drift.
        float sum = 0.0f;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            sum += laneWeights_[lane];
        }
        if (sum > 1.0e-12f) {
            const float normalizer = 1.0f / sum;
            for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
                laneWeights_[lane] *= normalizer;
            }
        }
    }

    void advanceLaneWeights()
    {
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            laneWeights_[lane] += laneSlew_
                * (laneWeightTargets_[lane] - laneWeights_[lane]);
        }
    }

    float readLaneStack(std::size_t channel, const ReadHead& head) const
    {
        float sample = 0.0f;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            if (laneWeights_[lane] > 1.0e-3f) {
                sample += readLaneHead(lane, channel, head)
                    * laneWeights_[lane] * activeControlsLaneGains_[lane];
            }
        }
        return sample;
    }

    MotionMode lanesMotionMode(LanesMode mode) const
    {
        switch (mode) {
            case LanesMode::reverse: return MotionMode::reverse;
            case LanesMode::pingPong: return MotionMode::roundTrip;
            case LanesMode::forward: return MotionMode::forward;
        }
        return MotionMode::forward;
    }

    float readLaneBlendAt(std::size_t channel,
                          double position,
                          const LaneBlend& lanes) const
    {
        const std::size_t firstLane = std::min(lanes.first, kStackLanes - 1u);
        const std::size_t secondLane = std::min(lanes.second, kStackLanes - 1u);
        const float first = sampleLaneAt(firstLane, channel, position)
            * activeControlsLaneGains_[firstLane];
        if (firstLane == secondLane || lanes.secondGain <= 0.0f) {
            return first;
        }
        return first * lanes.firstGain
            + sampleLaneAt(secondLane, channel, position)
                * activeControlsLaneGains_[secondLane] * lanes.secondGain;
    }

    float readLaneBlendHead(std::size_t channel,
                            const ReadHead& head,
                            const LaneBlend& lanes) const
    {
        const float first = readLaneBlendAt(channel, head.a, lanes);
        if (head.mix <= 0.0f) {
            return first;
        }
        return first
            + (readLaneBlendAt(channel, head.b, lanes) - first) * head.mix;
    }

    ReadHead makeEventReadHead(double position,
                               double increment,
                               double low,
                               double high,
                               double seam) const
    {
        ReadHead head;
        head.a = position;
        head.b = position;
        head.position = position;
        head.low = low;
        head.high = high;
        seam = clamp(seam, 0.0, std::max(0.0, (high - low) * 0.5));
        if (!(seam > 0.0)) {
            return head;
        }
        if (increment < 0.0) {
            if (position > low + seam) return head;
            const double local = clamp(
                (low + seam - position) / seam, 0.0, 1.0);
            head.b = high - local * seam;
            head.mix = smoothStep(static_cast<float>(local));
            head.position = head.a + (head.b - head.a) * head.mix;
            return head;
        }
        if (position < high - seam) return head;
        const double local = clamp(
            (position - (high - seam)) / seam, 0.0, 1.0);
        head.b = low + local * seam;
        head.mix = smoothStep(static_cast<float>(local));
        head.position = head.a + (head.b - head.a) * head.mix;
        return head;
    }

    static void advanceEventPosition(double& position,
                                     double increment,
                                     double low,
                                     double high,
                                     double seam)
    {
        const double width = std::max(1.0, high - low);
        seam = clamp(seam, 0.0, width * 0.5);
        const double cycle = std::max(1.0, width - seam);
        position += increment;
        while (position >= high) {
            position -= cycle;
        }
        while (position < low) {
            position += cycle;
        }
    }

    float readAtPhase(std::size_t outputChannel,
                      const Window& window,
                      MotionMode mode,
                      double phase,
                      float join) const
    {
        const ReadHead head = makeReadHead(window, mode, wrapPhase(phase), join);
        if (stackMode_) {
            return readLaneStack(outputChannel & 1u, head);
        }
        return readHead(mapSourceChannel(outputChannel), head);
    }

    void readAtPhaseStereo(const Window& window,
                           MotionMode mode,
                           double phase,
                           float join,
                           float* stereo) const
    {
        const ReadHead head = makeReadHead(
            window, mode, wrapPhase(phase), join);
        stereo[0u] = 0.0f;
        stereo[1u] = 0.0f;
        if (!stackMode_) {
            stereo[0u] = readHead(mapSourceChannel(0u), head);
            stereo[1u] = readHead(mapSourceChannel(1u), head);
            return;
        }
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            const float weight = laneWeights_[lane];
            if (weight <= 1.0e-3f) {
                continue;
            }
            const float gain = weight * activeControlsLaneGains_[lane];
            stereo[0u] += readLaneHead(lane, 0u, head) * gain;
            stereo[1u] += readLaneHead(lane, 1u, head) * gain;
        }
    }

    float readStretchedAtPhase(std::size_t outputChannel,
                               const Window& window,
                               MotionMode mode,
                               double phase,
                               float join,
                               float stretchRatio,
                               float firstGain,
                               float secondGain) const
    {
        const double path = pathFrames(window, mode, join);
        const double baseIncrement = static_cast<double>(sourceSampleRate_)
            / static_cast<double>(outputSampleRate_) / path;
        const double pitchIncrement = baseIncrement
            * static_cast<double>(speed_);
        const double stretchedIncrement = pitchIncrement
            / static_cast<double>(clamp(stretchRatio, 0.25f, 4.0f));
        const double transportIncrement = pitchIncrement
            + (stretchedIncrement - pitchIncrement)
                * static_cast<double>(stretchMix_);
        const double difference
            = std::abs(pitchIncrement - transportIncrement);
        const double windowPhase = clamp(
            static_cast<double>(sourceSampleRate_) * 0.040 / path,
            1.0 / path, 0.5);
        if (!(difference > 1.0e-12) || !(windowPhase > 0.0)) {
            return readAtPhase(outputChannel, window, mode, phase, join);
        }
        if (stretchMix_ <= 0.001f) {
            return readAtPhase(outputChannel, window, mode, phase, join);
        }

        const auto readerPhase = [&](double windowClock) {
            const double delay = pitchIncrement >= transportIncrement
                ? windowPhase * (1.0 - windowClock)
                : windowPhase * windowClock;
            // phase_ always advances, including Reverse; makeReadHead() maps
            // that clock to source direction. Keeping the OLA correction in
            // phase-clock space preserves pitch in all three Lanes modes.
            return wrapPhase(phase - delay);
        };
        const double firstClock = stretchPhase_;
        const double secondClock = wrapPhase(stretchPhase_ + 0.5);
        const float normalizer = 1.0f
            / std::max(1.0e-6f, firstGain + secondGain);
        const float granular = (readAtPhase(outputChannel, window, mode,
                    readerPhase(firstClock), join) * firstGain
                + readAtPhase(outputChannel, window, mode,
                    readerPhase(secondClock), join) * secondGain)
            * normalizer;
        if (stretchMix_ >= 0.999f) {
            return granular;
        }
        const float direct = readAtPhase(
            outputChannel, window, mode, phase, join);
        return direct + (granular - direct) * stretchMix_;
    }

    void readStretchedAtPhaseStereo(const Window& window,
                                    MotionMode mode,
                                    double phase,
                                    float join,
                                    float stretchRatio,
                                    float firstGain,
                                    float secondGain,
                                    float* stereo) const
    {
        const double path = pathFrames(window, mode, join);
        const double baseIncrement = static_cast<double>(sourceSampleRate_)
            / static_cast<double>(outputSampleRate_) / path;
        const double pitchIncrement = baseIncrement
            * static_cast<double>(speed_);
        const double stretchedIncrement = pitchIncrement
            / static_cast<double>(clamp(stretchRatio, 0.25f, 4.0f));
        const double transportIncrement = pitchIncrement
            + (stretchedIncrement - pitchIncrement)
                * static_cast<double>(stretchMix_);
        const double difference
            = std::abs(pitchIncrement - transportIncrement);
        const double windowPhase = clamp(
            static_cast<double>(sourceSampleRate_) * 0.040 / path,
            1.0 / path, 0.5);
        if (!(difference > 1.0e-12) || !(windowPhase > 0.0)
            || stretchMix_ <= 0.001f) {
            readAtPhaseStereo(window, mode, phase, join, stereo);
            return;
        }

        const auto readerPhase = [&](double windowClock) {
            const double delay = pitchIncrement >= transportIncrement
                ? windowPhase * (1.0 - windowClock)
                : windowPhase * windowClock;
            return wrapPhase(phase - delay);
        };
        float first[kLaneChannels] {};
        float second[kLaneChannels] {};
        readAtPhaseStereo(window, mode, readerPhase(stretchPhase_),
                          join, first);
        readAtPhaseStereo(window, mode,
                          readerPhase(wrapPhase(stretchPhase_ + 0.5)),
                          join, second);
        const float normalizer = 1.0f
            / std::max(1.0e-6f, firstGain + secondGain);
        float granular[kLaneChannels] {
            (first[0u] * firstGain + second[0u] * secondGain) * normalizer,
            (first[1u] * firstGain + second[1u] * secondGain) * normalizer,
        };
        if (stretchMix_ >= 0.999f) {
            stereo[0u] = granular[0u];
            stereo[1u] = granular[1u];
            return;
        }
        float direct[kLaneChannels] {};
        readAtPhaseStereo(window, mode, phase, join, direct);
        stereo[0u] = direct[0u]
            + (granular[0u] - direct[0u]) * stretchMix_;
        stereo[1u] = direct[1u]
            + (granular[1u] - direct[1u]) * stretchMix_;
    }

    void activateMotionSound(MotionSound sound, const Controls& controls)
    {
        currentMotionSound_ = sound;
        motionInnerPhase_ = sound == MotionSound::continuous
            ? 0.0 : clamp(controls.packetDuty, 0.02f, 0.98f) * 0.5f;
        motionOuterPhase_ = clamp(controls.motorSymmetry, 0.05f, 0.95f);
        motionEnvelope_ = 1.0f;
        motionInnerRateScale_ = 1.0f;
        for (auto& segment : motionSegments_) segment = {};
        motionEventPhase_ = 0.0;
        motionEventRepeatIndex_ = 0u;
        motionEventBasePosition_ = clamp01(controls.locus);
        motionEventRateScale_ = 1.0f;
        motionEventStarted_ = false;
        motionPacketEvent_ = false;
        motionTurnEvent_ = false;
    }

    void prepareMotionSound(const Controls& controls)
    {
        if (currentMotionSound_ == controls.motionSound
            && !motionSoundChangePending_) return;
        pendingMotionSound_ = controls.motionSound;
        motionSoundChangePending_ = true;
        if (currentAlgorithm_ != PlaybackAlgorithm::motion || !playing_) {
            activateMotionSound(pendingMotionSound_, controls);
            motionSoundChangePending_ = false;
            motionSoundTransitionRemaining_ = 0u;
        } else if (motionSoundTransitionRemaining_ == 0u) {
            motionSoundTransitionRemaining_ = readerTransitionFrames_ * 2u;
        }
    }

    float motionSoundTransitionGain(const Controls& controls)
    {
        if (motionSoundTransitionRemaining_ == 0u) return 1.0f;
        if (motionSoundTransitionRemaining_ == readerTransitionFrames_) {
            activateMotionSound(pendingMotionSound_, controls);
            motionSoundChangePending_ = false;
        }
        if (motionSoundTransitionRemaining_ > readerTransitionFrames_) {
            return smoothStep(static_cast<float>(
                motionSoundTransitionRemaining_ - readerTransitionFrames_)
                / static_cast<float>(readerTransitionFrames_));
        }
        return smoothStep(1.0f
            - static_cast<float>(motionSoundTransitionRemaining_)
                / static_cast<float>(readerTransitionFrames_));
    }

    void advanceMotionSoundTransition()
    {
        if (motionSoundTransitionRemaining_ != 0u) {
            --motionSoundTransitionRemaining_;
        }
    }

    void prepareMotionSoundBlock(const Controls& controls)
    {
        prepareMotionSound(controls);
        const bool initializeMotionControls = !motionControlsInitialized_;
        motionSoundPhaseIncrementTarget_ = clamp(
            controls.motionSoundRateHz, 0.25f, 32.0f)
            / outputSampleRate_;
        motionMotorPhaseIncrementTarget_ = clamp(
            controls.motorRateHz, 0.05f, 20.0f) / outputSampleRate_;
        motionPacketDutyTarget_ = clamp(
            controls.packetDuty, 0.02f, 0.98f);
        motionMotorSymmetryTarget_ = clamp(
            controls.motorSymmetry, 0.05f, 0.95f);
        motionMotorShape_ = controls.motorShape;
        motionTravelTarget_ = clamp01(controls.motionTravel);
        motionJitterTarget_ = clamp01(controls.motionJitter);
        if (initializeMotionControls) {
            motionSoundPhaseIncrement_ = motionSoundPhaseIncrementTarget_;
            motionMotorPhaseIncrement_ = motionMotorPhaseIncrementTarget_;
            motionPacketDuty_ = motionPacketDutyTarget_;
            motionMotorSymmetry_ = motionMotorSymmetryTarget_;
            motionTravel_ = motionTravelTarget_;
            motionJitter_ = motionJitterTarget_;
            motionControlsInitialized_ = true;
        }
        const std::uint32_t requestedSeed = std::max<std::uint32_t>(1u,
            static_cast<std::uint32_t>(std::lround(clamp(
                controls.motionSeed, 1.0f, 9999.0f))));
        if (motionSeed_ != requestedSeed
            || motionSeedTransitionRemaining_ != 0u) {
            // Coalesce rapid encoder motion. Re-hashing the current
            // Zigzag/Drunk segment in place would jump its read position, so
            // an active Motion reader reaches silence before adopting it.
            pendingMotionSeed_ = requestedSeed;
            motionSeedPending_ = pendingMotionSeed_ != motionSeed_;
            if (initializeMotionControls
                || currentAlgorithm_ != PlaybackAlgorithm::motion) {
                applyPendingMotionSeed();
            } else if (motionSeedTransitionRemaining_ == 0u) {
                motionSeedTransitionRemaining_ = readerTransitionFrames_ * 2u;
            }
        }
    }

    void advanceMotionControlSmoothing()
    {
        motionSoundPhaseIncrement_ += motionControlSlew_
            * (motionSoundPhaseIncrementTarget_
                - motionSoundPhaseIncrement_);
        motionMotorPhaseIncrement_ += motionControlSlew_
            * (motionMotorPhaseIncrementTarget_
                - motionMotorPhaseIncrement_);
        motionPacketDuty_ += motionControlSlew_
            * (motionPacketDutyTarget_ - motionPacketDuty_);
        motionMotorSymmetry_ += motionControlSlew_
            * (motionMotorSymmetryTarget_ - motionMotorSymmetry_);
        motionTravel_ += motionControlSlew_
            * (motionTravelTarget_ - motionTravel_);
        motionJitter_ += motionControlSlew_
            * (motionJitterTarget_ - motionJitter_);
    }

    void applyPendingMotionSeed()
    {
        if (!motionSeedPending_) return;
        motionSeed_ = pendingMotionSeed_;
        motionRandomCycle_ = 0u;
        randomState_ = 0x51f15e5du ^ (motionSeed_ * 0x9e3779b9u);
        if (randomState_ == 0u) randomState_ = 0x6d2b79f5u;
        motionSeedPending_ = false;
    }

    float motionSeedTransitionGain()
    {
        if (motionSeedTransitionRemaining_ == 0u) return 1.0f;
        if (motionSeedTransitionRemaining_ == readerTransitionFrames_) {
            applyPendingMotionSeed();
        }
        if (motionSeedTransitionRemaining_ > readerTransitionFrames_) {
            return smoothStep(static_cast<float>(
                motionSeedTransitionRemaining_ - readerTransitionFrames_)
                / static_cast<float>(readerTransitionFrames_));
        }
        return smoothStep(1.0f
            - static_cast<float>(motionSeedTransitionRemaining_)
                / static_cast<float>(readerTransitionFrames_));
    }

    void advanceMotionSeedTransition()
    {
        if (motionSeedTransitionRemaining_ != 0u) {
            --motionSeedTransitionRemaining_;
            if (motionSeedTransitionRemaining_ == 0u
                && motionSeedPending_ && pendingMotionSeed_ != motionSeed_) {
                // A newer seed can arrive after the midpoint. Finish the
                // current fade-up, then honor the queued target without
                // restarting or reversing the transition midway through.
                motionSeedTransitionRemaining_
                    = readerTransitionFrames_ * 2u;
            }
        }
    }

    static bool isSegmentSound(MotionSound sound)
    {
        return sound == MotionSound::bounce
            || sound == MotionSound::freeze
            || sound == MotionSound::iterate
            || sound == MotionSound::pulser
            || sound == MotionSound::doublets;
    }

    float motorEnvelopeLevel(float phase) const
    {
        float level = phase <= motionMotorSymmetry_
            ? phase / motionMotorSymmetry_
            : (1.0f - phase) / (1.0f - motionMotorSymmetry_);
        level = clamp01(level);
        switch (motionMotorShape_) {
            case MotorShape::rounded: return smoothStep(level);
            case MotorShape::pulse:
                // Keep the square-like articulation, but round its threshold
                // over a narrow band so a segment never changes gain in one
                // sample.
                return smoothStep((level - 0.44f) / 0.12f);
            case MotorShape::exponential: return level * level;
            case MotorShape::plateau: return std::min(1.0f, level * 3.0f);
            case MotorShape::triangle: return level;
        }
        return level;
    }

    float motionSoundGain(float join)
    {
        float target = 1.0f;
        if (currentMotionSound_ == MotionSound::packets
            || currentMotionSound_ == MotionSound::motor) {
            float packet = 0.0f;
            if (motionInnerPhase_ < motionPacketDuty_) {
                packet = windowTableValue(GrainWindow::hann,
                    motionInnerPhase_ / motionPacketDuty_);
            }
            target = 1.0f + (packet - 1.0f) * clamp01(join);
            if (currentMotionSound_ == MotionSound::motor) {
                target *= motorEnvelopeLevel(motionOuterPhase_);
            }
        }
        motionEnvelope_ += motionEnvelopeSlew_ * (target - motionEnvelope_);
        return motionEnvelope_;
    }

    void advanceMotionSound()
    {
        switch (currentMotionSound_) {
            case MotionSound::continuous:
                break;
            case MotionSound::packets:
                motionInnerPhase_ += motionSoundPhaseIncrement_
                    * motionInnerRateScale_;
                if (motionInnerPhase_ >= 1.0f) {
                    motionInnerPhase_ -= 1.0f;
                    motionPacketEvent_ = true;
                    motionInnerRateScale_ = std::max(0.05f,
                        1.0f + (randomUnit() * 2.0f - 1.0f)
                            * motionJitter_ * 0.5f);
                }
                break;
            case MotionSound::motor:
                motionInnerPhase_ += motionSoundPhaseIncrement_
                    * motionInnerRateScale_;
                if (motionInnerPhase_ >= 1.0f) {
                    motionInnerPhase_ -= 1.0f;
                    motionPacketEvent_ = true;
                    motionInnerRateScale_ = std::max(0.05f,
                        1.0f + (randomUnit() * 2.0f - 1.0f)
                            * motionJitter_ * 0.5f);
                }
                motionOuterPhase_ += motionMotorPhaseIncrement_;
                if (motionOuterPhase_ >= 1.0f) {
                    motionOuterPhase_ -= 1.0f;
                }
                break;
            case MotionSound::bounce:
            case MotionSound::freeze:
            case MotionSound::iterate:
            case MotionSound::pulser:
            case MotionSound::doublets:
                motionInnerPhase_ += motionSoundPhaseIncrement_
                    * motionInnerRateScale_;
                if (motionInnerPhase_ >= 1.0f) {
                    motionInnerPhase_ -= 1.0f;
                    motionPacketEvent_ = true;
                    motionInnerRateScale_ = std::max(0.05f,
                        1.0f + (randomUnit() * 2.0f - 1.0f)
                            * motionJitter_ * 0.5f);
                }
                break;
        }
    }

    MotionSegment* allocateMotionSegment(const Controls& controls)
    {
        const std::size_t limit = static_cast<std::size_t>(clamp(
            std::round(controls.segmentLayerLimit), 1.0f,
            static_cast<float>(kMotionSegmentLayerCount)));
        const auto retire = [](MotionSegment& segment) {
            if (segment.active && !segment.retiring) {
                segment.retiring = true;
                segment.retirementAge = 0u;
            }
        };
        if (controls.segmentOverlap == SegmentOverlap::cut) {
            for (auto& segment : motionSegments_) retire(segment);
        } else {
            std::size_t sounding = 0u;
            for (const auto& segment : motionSegments_) {
                if (segment.active && !segment.retiring) ++sounding;
            }
            while (sounding >= limit) {
                MotionSegment* oldest = nullptr;
                for (auto& segment : motionSegments_) {
                    if (!segment.active || segment.retiring) continue;
                    if (!oldest || segment.age < oldest->age) {
                        oldest = &segment;
                    }
                }
                if (!oldest) break;
                retire(*oldest);
                --sounding;
            }
        }
        for (auto& segment : motionSegments_) {
            if (!segment.active) return &segment;
        }
        // Eight slots cover four sounding layers plus their 3 ms retirement
        // tails. At extreme trigger rates, recycle the tail nearest silence.
        MotionSegment* oldest = &motionSegments_[0u];
        for (std::size_t index = 1u;
             index < kMotionSegmentSlotCount; ++index) {
            if (motionSegments_[index].age < oldest->age) {
                oldest = &motionSegments_[index];
            }
        }
        return oldest;
    }

    float effectiveMotionEventRate(const Controls& controls) const
    {
        const float repeat = static_cast<float>(motionEventRepeatIndex_);
        const float curve = clamp(
            controls.segmentIntervalCurve * repeat, -3.0f, 3.0f);
        return clamp(controls.segmentRateHz * motionEventRateScale_
                * std::pow(2.0f, curve),
            0.03125f, 640.0f);
    }

    void startMotionSegment(const Controls& controls,
                            const Window& window,
                            double trajectoryPosition)
    {
        const double fieldWidth = std::max(1.0, window.high - window.low);
        if (!motionEventStarted_
            || motionEventBasePosition_ < window.low
            || motionEventBasePosition_ >= window.high) {
            motionEventBasePosition_ = clamp(
                trajectoryPosition, window.low, window.high - 1.0);
        }
        double center = motionEventBasePosition_;
        double width = fieldWidth;
        const std::size_t repeats = static_cast<std::size_t>(clamp(
            std::round(controls.segmentRepeats), 1.0f, 16.0f));
        switch (currentMotionSound_) {
            case MotionSound::freeze:
                center = 0.5 * (window.low + window.high);
                break;
            case MotionSound::bounce: {
                center = 0.5 * (window.low + window.high);
                const float progress = repeats > 1u
                    ? static_cast<float>(motionEventRepeatIndex_)
                        / static_cast<float>(repeats - 1u)
                    : 0.0f;
                width *= 1.0 - static_cast<double>(progress
                    * (1.0f - motionPacketDuty_));
                break;
            }
            case MotionSound::iterate:
                if (controls.segmentStep <= 0.0f) {
                    center = trajectoryPosition;
                }
                break;
            case MotionSound::pulser: {
                center = window.low + static_cast<double>(randomUnit())
                    * fieldWidth;
                const double seconds = static_cast<double>(motionPacketDuty_)
                    / std::max<double>(0.25,
                        effectiveMotionEventRate(controls));
                width = std::min(fieldWidth,
                    static_cast<double>(sourceSampleRate_) * seconds
                        * static_cast<double>(std::max(0.05f, speed_)));
                break;
            }
            case MotionSound::doublets:
            case MotionSound::continuous:
            case MotionSound::packets:
            case MotionSound::motor:
                break;
        }
        width = clamp(width, 1.0, fieldWidth);
        center = clamp(center, window.low, window.high);
        double low = center - width * 0.5;
        double high = center + width * 0.5;
        if (low < window.low) {
            high += window.low - low;
            low = window.low;
        }
        if (high > window.high) {
            low -= high - window.high;
            high = window.high;
        }
        low = std::max(window.low, low);
        high = std::min(window.high, high);

        MotionSegment* segment = allocateMotionSegment(controls);
        *segment = {};
        segment->active = true;
        segment->age = ++motionSegmentAge_;
        segment->low = low;
        segment->high = std::max(low + 1.0, high);
        segment->lanes = stackMode_
            ? makeLaneBlend(lanePhase_, controls) : LaneBlend {};
        segment->forward = motionPathForward_;
        const float semitones = (randomUnit() * 2.0f - 1.0f)
            * clamp(controls.segmentPitchScatter, 0.0f, 12.0f);
        const double pitch = std::pow(2.0,
            static_cast<double>(semitones) / 12.0);
        segment->increment = static_cast<double>(sourceSampleRate_)
            / static_cast<double>(outputSampleRate_)
            * static_cast<double>(std::max(0.05f, speed_)) * pitch
            / std::max(1.0, segment->high - segment->low);
        segment->gain = currentMotionSound_ == MotionSound::bounce
            ? std::pow(std::max(0.0f,
                1.0f - clamp01(controls.segmentLevelVariation)),
                static_cast<float>(motionEventRepeatIndex_))
            : 1.0f - randomUnit()
                * clamp01(controls.segmentLevelVariation);

        ++motionEventRepeatIndex_;
        if (motionEventRepeatIndex_ >= repeats) motionEventRepeatIndex_ = 0u;
        const double step = static_cast<double>(clamp01(controls.segmentStep))
            * static_cast<double>(sourceFrames_);
        if (currentMotionSound_ == MotionSound::doublets) {
            if (motionEventRepeatIndex_ == 0u) {
                motionEventBasePosition_ = window.low + std::fmod(
                    motionEventBasePosition_ - window.low + step,
                    fieldWidth);
            }
        } else if (currentMotionSound_ == MotionSound::iterate && step > 0.0) {
            motionEventBasePosition_ = window.low + std::fmod(
                motionEventBasePosition_ - window.low + step,
                fieldWidth);
        }
    }

    void renderMotionSegments(const Controls& controls,
                              const Window& window,
                              double trajectoryPosition,
                              float join,
                              float* wet)
    {
        bool trigger = !motionEventStarted_;
        if (controls.segmentTrigger == SegmentTrigger::clock) {
            trigger = trigger || motionEventPhase_ >= 1.0;
        } else if (controls.segmentTrigger == SegmentTrigger::packet) {
            trigger = trigger || motionPacketEvent_;
        } else {
            trigger = trigger || motionTurnEvent_;
        }
        motionPacketEvent_ = false;
        motionTurnEvent_ = false;
        if (trigger) {
            motionEventPhase_ = wrapPhase(motionEventPhase_);
            startMotionSegment(controls, window, trajectoryPosition);
            motionEventStarted_ = true;
            motionEventRateScale_ = std::max(0.10f,
                1.0f + (randomUnit() * 2.0f - 1.0f)
                    * motionJitter_ * 0.75f);
        }

        float stereo[kLaneChannels] {};
        float gainSum = 0.0f;
        MotionSegment* newest = nullptr;
        for (auto& segment : motionSegments_) {
            if (!segment.active) continue;
            const double width = std::max(1.0, segment.high - segment.low);
            const double position = segment.forward
                ? segment.low + segment.phase * width
                : segment.high - 1.0 - segment.phase * width;
            ReadHead head;
            head.a = clamp(position, segment.low,
                std::max(segment.low, segment.high - 1.0));
            head.b = head.a;
            head.position = head.a;
            const float shape = motorEnvelopeLevel(
                static_cast<float>(segment.phase));
            const float envelope = 1.0f
                + (shape - 1.0f) * clamp01(join);
            const float safetyPhase = clamp(
                static_cast<float>(segment.increment)
                    * static_cast<float>(safetyFadeFrames_),
                1.0e-6f, 0.5f);
            const float edge = std::min(
                static_cast<float>(segment.phase),
                static_cast<float>(1.0 - segment.phase));
            float retirement = 1.0f;
            if (segment.retiring) {
                retirement = 1.0f - smoothStep(static_cast<float>(
                    segment.retirementAge) / static_cast<float>(
                        std::max<std::size_t>(1u, safetyFadeFrames_)));
            }
            const float gain = segment.gain * envelope
                * smoothStep(edge / safetyPhase) * retirement;
            for (std::size_t channel = 0u;
                 channel < kLaneChannels; ++channel) {
                const float sample = stackMode_
                    ? readLaneBlendHead(channel, head, segment.lanes)
                    : readHead(mapSourceChannel(channel), head);
                stereo[channel] += sample * gain;
            }
            gainSum += gain;
            if (!newest || segment.age > newest->age) newest = &segment;
            segment.phase += segment.increment;
            if (segment.retiring
                && ++segment.retirementAge >= safetyFadeFrames_) {
                segment.active = false;
            } else if (segment.phase >= 1.0) {
                segment.active = false;
            }
        }
        if (gainSum > 1.0f) {
            stereo[0u] /= gainSum;
            stereo[1u] /= gainSum;
        }
        for (std::size_t channel = 0u; channel < kChannels; ++channel) {
            wet[channel] = stereo[channel & 1u];
        }
        if (newest) {
            const double width = newest->high - newest->low;
            displayPosition_ = newest->forward
                ? newest->low + std::min(1.0, newest->phase) * width
                : newest->high - std::min(1.0, newest->phase) * width;
        }
        if (controls.segmentTrigger == SegmentTrigger::clock) {
            motionEventPhase_ += static_cast<double>(
                effectiveMotionEventRate(controls))
                / static_cast<double>(outputSampleRate_);
        }
    }

    void renderContinuousFrame(const Controls& controls,
                               float join,
                               bool lanesAlgorithm,
                               bool stretch,
                               float* wet)
    {
        const MotionMode mode = lanesAlgorithm
            ? lanesMotionMode(controls.lanes) : controls.motion;
        if (lanesAlgorithm) {
            requestedMode_ = mode;
            movingCenterInitialized_ = false;
            lastControlLocus_ = clamp01(controls.locus);
        } else {
            updateMovingControl(controls);
        }
        const Window requested = lanesAlgorithm
            ? smoothEventWindow(controls) : requestedWindow(controls);
        if (lanesAlgorithm) {
            followStretchWindow(requested, mode);
        } else {
            beginWindowFadeIfNeeded(requested, mode);
        }

        const float windowMix = windowFadeRemaining_ == 0u
            ? 1.0f
            : smoothStep(1.0f
                - static_cast<float>(windowFadeRemaining_)
                    / static_cast<float>(windowFadeFrames_));
        const Window firstWindow = windowFadeRemaining_ == 0u
            ? currentWindow_ : fromWindow_;
        const Window secondWindow = windowFadeRemaining_ == 0u
            ? currentWindow_ : toWindow_;
        const MotionMode firstMode = windowFadeRemaining_ == 0u
            ? currentMode_ : fromMode_;
        const MotionMode secondMode = windowFadeRemaining_ == 0u
            ? currentMode_ : toMode_;
        Window firstReadWindow = firstWindow;
        Window secondReadWindow = secondWindow;
        MotionMode firstReadMode = firstMode;
        MotionMode secondReadMode = secondMode;
        const double readPhase = phase_;
        const float readJoin = join;
        const bool motorReverse = !lanesAlgorithm
            && currentMotionSound_ == MotionSound::motor
            && mode != MotionMode::forward && mode != MotionMode::reverse
            && mode != MotionMode::movingLoop
            && motionOuterPhase_ > motionMotorSymmetry_;
        const bool intrinsicForward = motionTrajectoryForward(
            secondReadMode, readPhase);
        motionPathForward_ = motorReverse
            ? !intrinsicForward : intrinsicForward;

        if (lanesAlgorithm) {
            const float stretchTarget = stretch
                    && std::abs(stretchRatio_ - 1.0f) > 1.0e-4f
                ? 1.0f : 0.0f;
            stretchMix_ += laneSlew_ * (stretchTarget - stretchMix_);
        }
        const bool renderStretch = lanesAlgorithm && stretchMix_ > 0.001f;
        const float firstStretchGain = renderStretch
            ? smoothBell(stretchPhase_) : 1.0f;
        const float secondStretchGain = renderStretch
            ? 1.0f - firstStretchGain : 0.0f;
        if (stackMode_) {
            float oldStereo[kLaneChannels] {};
            if (renderStretch) {
                readStretchedAtPhaseStereo(firstReadWindow, firstReadMode,
                    readPhase, readJoin,
                    stretchRatio_,
                    firstStretchGain, secondStretchGain, oldStereo);
            } else {
                readAtPhaseStereo(
                    firstReadWindow, firstReadMode, readPhase, readJoin,
                    oldStereo);
            }
            float newStereo[kLaneChannels] { oldStereo[0u], oldStereo[1u] };
            if (windowFadeRemaining_ != 0u) {
                if (renderStretch) {
                    readStretchedAtPhaseStereo(secondReadWindow, secondReadMode,
                        readPhase, readJoin, stretchRatio_,
                        firstStretchGain, secondStretchGain,
                        newStereo);
                } else {
                    readAtPhaseStereo(
                        secondReadWindow, secondReadMode, readPhase, readJoin,
                        newStereo);
                }
            }
            wet[0u] = oldStereo[0u]
                + (newStereo[0u] - oldStereo[0u]) * windowMix;
            wet[1u] = oldStereo[1u]
                + (newStereo[1u] - oldStereo[1u]) * windowMix;
            wet[2u] = wet[0u];
            wet[3u] = wet[1u];
        } else {
            for (std::size_t channel = 0u; channel < kChannels; ++channel) {
                const float oldWet = renderStretch
                    ? readStretchedAtPhase(channel, firstReadWindow,
                        firstReadMode, readPhase, readJoin,
                        stretchRatio_,
                        firstStretchGain, secondStretchGain)
                    : readAtPhase(
                        channel, firstReadWindow, firstReadMode,
                        readPhase, readJoin);
                const float newWet = windowFadeRemaining_ == 0u
                    ? oldWet
                    : renderStretch
                        ? readStretchedAtPhase(channel, secondReadWindow,
                            secondReadMode, readPhase, readJoin,
                            stretchRatio_,
                            firstStretchGain, secondStretchGain)
                        : readAtPhase(
                            channel, secondReadWindow, secondReadMode,
                            readPhase, readJoin);
                wet[channel] = oldWet + (newWet - oldWet) * windowMix;
            }
        }

        if (!lanesAlgorithm && isSegmentSound(currentMotionSound_)) {
            const ReadHead trajectory = makeReadHead(
                secondReadWindow, secondReadMode, readPhase, readJoin);
            renderMotionSegments(controls, secondReadWindow,
                trajectory.position, join, wet);
            advanceMotionSound();
        } else if (!lanesAlgorithm
                   && currentMotionSound_ != MotionSound::continuous) {
            const float articulation = motionSoundGain(join);
            for (std::size_t channel = 0u;
                 channel < kChannels; ++channel) {
                wet[channel] *= articulation;
            }
            advanceMotionSound();
        }

        const ReadHead firstHead = makeReadHead(firstReadWindow, firstReadMode,
            readPhase, readJoin);
        const ReadHead secondHead = windowFadeRemaining_ == 0u
            ? firstHead : makeReadHead(secondReadWindow, secondReadMode,
                readPhase, readJoin);
        if (!isSegmentSound(currentMotionSound_) || lanesAlgorithm) {
            displayPosition_ = firstHead.position
                + (secondHead.position - firstHead.position) * windowMix;
        }
        fieldLow_ = firstHead.low + (secondHead.low - firstHead.low) * windowMix;
        fieldHigh_ = firstHead.high + (secondHead.high - firstHead.high)
            * windowMix;

        const double firstPath = pathFrames(firstWindow, firstMode, join);
        const double secondPath = windowFadeRemaining_ == 0u
            ? firstPath : pathFrames(secondWindow, secondMode, join);
        const double path = std::max(1.0,
            firstPath + (secondPath - firstPath) * windowMix);
        const double baseIncrement = static_cast<double>(sourceSampleRate_)
            / static_cast<double>(outputSampleRate_) / path;
        const double pathRate = lanesAlgorithm ? 1.0
            : static_cast<double>(clamp(
                controls.motionPathRate, 0.01f, 80.0f));
        const double normalIncrement = static_cast<double>(speed_)
            * pathRate * baseIncrement;
        const double hertzIncrement = static_cast<double>(speed_)
            * static_cast<double>(clamp(
                controls.motionPathRate, 0.01f, 80.0f))
            / static_cast<double>(outputSampleRate_);
        const double pitchIncrement = !lanesAlgorithm
                && controls.motionRateBasis == MotionRateBasis::hertz
            ? hertzIncrement : normalIncrement;
        const double requestedStretch = lanesAlgorithm
            ? static_cast<double>(stretchRatio_)
            : 1.0;
        const double stretchedIncrement = pitchIncrement / requestedStretch;
        const double phaseIncrement = pitchIncrement
            + (stretchedIncrement - pitchIncrement)
                * static_cast<double>(stretchMix_);
        const double directedIncrement = motorReverse
            ? -phaseIncrement : phaseIncrement;
        lastMotionPhaseIncrement_ = directedIncrement;
        const double previousPhase = phase_;
        const bool previousDirection = motionTrajectoryForward(
            mode, previousPhase);
        phase_ += directedIncrement;
        if (renderStretch) {
            const double windowPhase = clamp(
                static_cast<double>(sourceSampleRate_) * 0.040 / path,
                1.0 / path, 0.5);
            stretchPhase_ = wrapPhase(stretchPhase_
                + std::abs(pitchIncrement - phaseIncrement) / windowPhase);
        }

        bool wrapped = false;
        while (phase_ >= 1.0) {
            if (!lanesAlgorithm && mode == MotionMode::zigzag) {
                const double width = std::max(
                    1.0, secondReadWindow.high - secondReadWindow.low);
                const double transition = clamp(
                    std::max(joinFrames(secondReadWindow, readJoin) / width,
                        std::abs(lastMotionPhaseIncrement_)
                            * static_cast<double>(safetyFadeFrames_)),
                    1.0 / width, 0.45);
                const double cycle = std::max(1.0e-9, 1.0 - transition);
                phase_ = transition + std::fmod(phase_ - 1.0, cycle);
            } else {
                phase_ -= 1.0;
            }
            wrapped = true;
        }
        while (phase_ < 0.0) {
            phase_ += 1.0;
            wrapped = true;
        }
        if (!lanesAlgorithm) {
            const bool directionTurn = !wrapped
                && previousDirection != motionTrajectoryForward(mode, phase_);
            motionTurnEvent_ = motionTurnEvent_ || wrapped || directionTurn;
            if (wrapped) ++motionRandomCycle_;
        }
        if (!lanesAlgorithm && wrapped
            && requestedMode_ == MotionMode::movingLoop) {
            stepMovingLoop(requested.high - requested.low,
                controls.motionStep);
        }
        if (windowFadeRemaining_ != 0u) {
            --windowFadeRemaining_;
            if (windowFadeRemaining_ == 0u) {
                currentWindow_ = toWindow_;
                currentMode_ = toMode_;
            }
        }
    }

    void prepareGrainControls(const Controls& controls)
    {
        const float durationSeconds
            = clamp(controls.grainSizeMs, 8.0f, 1000.0f) * 0.001f;
        grainDurationFrames_ = std::max<std::size_t>(8u,
            static_cast<std::size_t>(outputSampleRate_ * durationSeconds));
        grainVoiceLimit_ = static_cast<std::size_t>(clamp(
            std::round(controls.grainVoiceLimit), 4.0f,
            static_cast<float>(kGrainVoiceCount)));
        const float voiceSafeDensity
            = static_cast<float>(grainVoiceLimit_ - 1u) / durationSeconds;
        grainDensityHz_ = std::min(
            clamp(controls.grainDensityHz, 1.0f, 96.0f),
            voiceSafeDensity);
        grainIntervalFrames_ = static_cast<double>(outputSampleRate_)
            / static_cast<double>(grainDensityHz_);
        grainPitch_ = clamp(controls.grainPitch, 0.25f, 4.0f);
        grainGain_ = 1.0f / std::max(
            1.0f, grainDensityHz_ * durationSeconds);
    }

    static float parzenWindow(float phase)
    {
        const float triangle = phase < 0.5f
            ? phase * 2.0f : (1.0f - phase) * 2.0f;
        return triangle > 0.5f
            ? 1.0f - (1.0f - triangle) * (1.0f - triangle)
                * triangle * 6.0f
            : triangle * triangle * triangle * 2.0f;
    }

    void prepareGrainWindowTables()
    {
        constexpr float pi = 3.14159265358979323846f;
        for (std::size_t index = 0u;
             index < kGrainWindowTableSize;
             ++index) {
            const float phase = static_cast<float>(index)
                / static_cast<float>(kGrainWindowTableSize - 1u);
            const float triangle = 1.0f
                - std::abs(phase * 2.0f - 1.0f);
            grainWindowTables_[static_cast<std::size_t>(
                GrainWindow::parzen)][index] = parzenWindow(phase);
            grainWindowTables_[static_cast<std::size_t>(
                GrainWindow::sine)][index] = std::sin(pi * phase);
            grainWindowTables_[static_cast<std::size_t>(
                GrainWindow::hann)][index]
                = 0.5f - 0.5f * std::cos(2.0f * pi * phase);
            grainWindowTables_[static_cast<std::size_t>(
                GrainWindow::triangle)][index] = triangle;
            const float gaussian = (phase - 0.5f) / 0.18f;
            const float gaussianEdge = smoothStep(std::min(
                1.0f, std::min(phase, 1.0f - phase) / 0.05f));
            grainWindowTables_[static_cast<std::size_t>(
                GrainWindow::gaussian)][index]
                = std::exp(-0.5f * gaussian * gaussian) * gaussianEdge;
            grainWindowTables_[static_cast<std::size_t>(
                GrainWindow::bell)][index] = smoothBell(phase);
        }
    }

    float windowTableValue(GrainWindow window, float phase) const
    {
        const float tablePosition = clamp01(phase)
            * static_cast<float>(kGrainWindowTableSize - 1u);
        const std::size_t first = std::min<std::size_t>(
            static_cast<std::size_t>(tablePosition),
            kGrainWindowTableSize - 1u);
        const std::size_t second = std::min<std::size_t>(
            first + 1u, kGrainWindowTableSize - 1u);
        const float mix = tablePosition - static_cast<float>(first);
        const std::size_t table = std::min<std::size_t>(
            static_cast<std::size_t>(window),
            kGrainWindowCount - 1u);
        const float firstValue = grainWindowTables_[table][first];
        return firstValue
            + (grainWindowTables_[table][second] - firstValue) * mix;
    }

    float grainEnvelope(const Grain& grain) const
    {
        float phase = static_cast<float>(grain.age)
            / static_cast<float>(std::max<std::size_t>(1u,
                grain.duration - 1u));
        const float skew = clamp(grain.envelopeSkew, -0.95f, 0.95f);
        if (skew > 0.0f) {
            const float square = phase * phase;
            const float fourth = square * square;
            phase += (fourth - phase) * skew;
        } else if (skew < 0.0f) {
            const float inverse = 1.0f - phase;
            const float square = inverse * inverse;
            const float fourth = square * square;
            const float early = 1.0f - fourth;
            phase += (early - phase) * -skew;
        }
        return windowTableValue(grain.window, phase);
    }

    void prepareSorterRanking(const Window& window,
                              std::size_t regions,
                              std::size_t lane)
    {
        regions = std::max<std::size_t>(1u,
            std::min<std::size_t>(regions, kRegionCount));
        if (sorterCacheValid_ && sorterRegionCount_ == regions
            && sorterLane_ == lane
            && std::abs(sorterWindowLow_ - window.low) < 0.5
            && std::abs(sorterWindowHigh_ - window.high) < 0.5) {
            return;
        }
        const double width = std::max(1.0, window.high - window.low);
        for (std::size_t region = 0u; region < regions; ++region) {
            float energy = 0.0f;
            for (std::size_t probe = 0u; probe < 8u; ++probe) {
                const double unit = (static_cast<double>(region)
                        + (static_cast<double>(probe) + 0.5) / 8.0)
                    / static_cast<double>(regions);
                const double position = window.low + unit * width;
                for (std::size_t channel = 0u;
                     channel < kLaneChannels; ++channel) {
                    const float sample = stackMode_ && lane < kStackLanes
                        ? sampleLaneAt(lane, channel, position)
                        : sampleAt(mapSourceChannel(channel), position);
                    energy += sample * sample;
                }
            }
            sorterEnergies_[region] = energy;
            sorterRanking_[region] = static_cast<std::uint8_t>(region);
        }
        for (std::size_t index = 1u; index < regions; ++index) {
            const std::uint8_t candidate = sorterRanking_[index];
            std::size_t insertion = index;
            while (insertion > 0u) {
                const std::uint8_t previous = sorterRanking_[insertion - 1u];
                if (sorterEnergies_[previous] < sorterEnergies_[candidate]
                    || (sorterEnergies_[previous]
                            == sorterEnergies_[candidate]
                        && previous < candidate)) {
                    break;
                }
                sorterRanking_[insertion] = previous;
                --insertion;
            }
            sorterRanking_[insertion] = candidate;
        }
        sorterRegionCount_ = regions;
        sorterLane_ = lane;
        sorterWindowLow_ = window.low;
        sorterWindowHigh_ = window.high;
        sorterCacheValid_ = true;
    }

    double sorterPosition(double original,
                          const Window& window,
                          const Controls& controls,
                          const LaneBlend& lanes)
    {
        const std::size_t regions = static_cast<std::size_t>(clamp(
            std::round(controls.grainRegionCount), 1.0f,
            static_cast<float>(kRegionCount)));
        const std::size_t lane = stackMode_
            ? (lanes.secondGain > lanes.firstGain
                ? lanes.second : lanes.first)
            : kNoLane;
        prepareSorterRanking(window, regions, lane);
        const std::size_t sequence = grainProcessIndex_ % regions;
        const double width = std::max(1.0, window.high - window.low);
        const double sorted = window.low
            + (static_cast<double>(sorterRanking_[sequence]) + 0.5)
                * width / static_cast<double>(regions);
        return clamp(original + (sorted - original)
                * static_cast<double>(clamp01(controls.grainProcessAmount)),
            window.low, window.high - 1.0);
    }

    void startGrain(const Controls& controls, const Window& window)
    {
        Grain* slot = nullptr;
        for (std::size_t index = 0u; index < grainVoiceLimit_; ++index) {
            auto& grain = grains_[index];
            if (!grain.active) {
                slot = &grain;
                break;
            }
        }
        if (!slot) {
            // Never tear down a live grain. Density is already capped with
            // one voice of headroom, but a skipped event is still safer than
            // discontinuously stealing a voice during rapid parameter moves.
            return;
        }

        const double width = std::max(1.0, window.high - window.low);
        const bool doublet = controls.grainProcess == GrainProcess::doublets
            && grainDoubletPending_;
        const double sourcePhase = controls.grainAdvance == GrainAdvance::grain
            ? grainSourcePhase_ : phase_;
        double position = doublet ? grainDoubletPosition_
            : window.low + sourcePhase * width;
        switch (controls.grains) {
            case GrainMode::freeze:
                position = 0.5 * (window.low + window.high);
                break;
            case GrainMode::cloud:
                position = window.low + static_cast<double>(randomUnit())
                    * width;
                break;
            case GrainMode::slice: {
                const std::size_t regions = static_cast<std::size_t>(clamp(
                    controls.grainRegionCount, 1.0f,
                    static_cast<float>(kCutRegionCount)));
                const double slice = std::floor(sourcePhase
                    * static_cast<double>(regions));
                position = window.low + (slice + 0.5) * width
                    / static_cast<double>(regions);
                break;
            }
            case GrainMode::scan:
                break;
        }

        if (!doublet) {
            const float spray = clamp01(controls.grainSpray);
            float direction = randomUnit() * 2.0f - 1.0f;
            if (controls.grainBias == GrainBias::behind) {
                direction = -randomUnit();
            } else if (controls.grainBias == GrainBias::ahead) {
                direction = randomUnit();
            }
            position += static_cast<double>(direction * spray) * width * 0.5;
            position = clamp(position, window.low, window.high - 1.0);
        }

        const LaneBlend eventLanes = stackMode_
            ? makeLaneBlend(lanePhase_, controls) : LaneBlend {};
        const float processAmount = clamp01(controls.grainProcessAmount);
        if (controls.grainProcess == GrainProcess::sorter) {
            position = sorterPosition(position, window, controls, eventLanes);
        }
        if (controls.grainProcess == GrainProcess::stutter) {
            const std::size_t group = 1u + static_cast<std::size_t>(
                std::lround(processAmount * 15.0f));
            if (grainProcessIndex_ % group == 0u
                || grainStutterAnchor_ < window.low
                || grainStutterAnchor_ >= window.high) {
                grainStutterAnchor_ = position;
            }
            position = grainStutterAnchor_;
        }

        *slot = {};
        slot->active = true;
        const float sizeVariation = clamp01(controls.grainSizeVariation);
        float durationScale = 1.0f + (randomUnit() * 2.0f - 1.0f)
            * sizeVariation * 0.9f;
        if (controls.grainProcess == GrainProcess::shrink) {
            const float shrinkPhase = static_cast<float>(grainProcessIndex_ % 8u)
                / 7.0f;
            durationScale *= 1.0f - shrinkPhase * processAmount * 0.85f;
        }
        slot->duration = std::max<std::size_t>(8u,
            static_cast<std::size_t>(static_cast<float>(grainDurationFrames_)
                * std::max(0.08f, durationScale)));
        slot->position = position;
        const float pitchSpray = clamp(
            controls.grainPitchSpray, 0.0f, 24.0f);
        const float randomSemitones = (randomUnit() * 2.0f - 1.0f)
            * pitchSpray;
        float eventPitch = grainPitch_
            * clamp(controls.notePitch, 0.001f, 512.0f)
            * std::pow(2.0f, randomSemitones / 12.0f);
        if (randomUnit() < clamp01(controls.grainReverseChance)) {
            eventPitch = -eventPitch;
        }
        slot->increment = static_cast<double>(sourceSampleRate_)
            / static_cast<double>(outputSampleRate_)
            * static_cast<double>(eventPitch);
        slot->low = window.low;
        slot->high = window.high;
        slot->seam = joinFrames(window, controls.join);
        slot->lanes = eventLanes;
        slot->gain = grainGain_ * (1.0f - randomUnit()
            * clamp01(controls.grainLevelVariation));
        slot->envelopeSkew = clamp(
            controls.grainEnvelopeSkew, -1.0f, 1.0f);
        const float spread = clamp01(controls.grainStereoSpread);
        const float pan = spread > 0.0f
            ? (randomUnit() * 2.0f - 1.0f) * spread
            : 0.0f;
        constexpr float quarterPi = 0.78539816339744830962f;
        constexpr float rootTwo = 1.41421356237309504880f;
        const float panAngle = (pan + 1.0f) * quarterPi;
        slot->channelGains[0u] = rootTwo * std::cos(panAngle);
        slot->channelGains[1u] = rootTwo * std::sin(panAngle);
        slot->window = controls.grainWindow;
        grainDoubletPosition_ = position;
        if (controls.grainProcess == GrainProcess::doublets) {
            grainDoubletPending_ = !doublet;
        } else {
            grainDoubletPending_ = false;
        }
        if (controls.grainAdvance == GrainAdvance::grain && !doublet) {
            const double sourceAdvance = static_cast<double>(speed_)
                * static_cast<double>(sourceSampleRate_)
                / static_cast<double>(outputSampleRate_) / width
                * grainIntervalFrames_;
            grainSourcePhase_ = wrapPhase(grainSourcePhase_ + sourceAdvance);
        }
        ++grainSerial_;
        ++grainProcessIndex_;
    }

    void renderGrainsFrame(const Controls& controls, float* wet)
    {
        const Window window = smoothEventWindow(controls);
        fieldLow_ = window.low;
        fieldHigh_ = window.high;
        const double width = std::max(1.0, window.high - window.low);
        displayPosition_ = window.low + phase_ * width;

        if (grainCountdown_ <= 0.0) {
            startGrain(controls, window);
            double nextInterval = grainIntervalFrames_;
            if (controls.grainProcess == GrainProcess::doublets) {
                const float separation = 0.5f
                    - clamp01(controls.grainProcessAmount) * 0.4f;
                nextInterval *= grainDoubletPending_
                    ? static_cast<double>(separation)
                    : static_cast<double>(1.0f - separation);
            }
            if (controls.grainTiming == GrainTiming::scatter) {
                const float scatter = clamp01(controls.grainTimingScatter);
                nextInterval *= 1.0 + static_cast<double>(
                    (randomUnit() * 2.0f - 1.0f) * scatter * 0.9f);
            }
            grainCountdown_ += std::max(1.0, nextInterval);
        }
        grainCountdown_ -= 1.0;

        float stereo[kLaneChannels] {};
        float envelopeGainSums[kLaneChannels] {};
        for (auto& grain : grains_) {
            if (!grain.active) {
                continue;
            }
            const float gain = grain.gain * grainEnvelope(grain);
            const ReadHead head = makeEventReadHead(
                grain.position, grain.increment,
                grain.low, grain.high, grain.seam);
            for (std::size_t channel = 0u;
                 channel < kLaneChannels;
                 ++channel) {
                const float sample = stackMode_
                    ? readLaneBlendHead(channel, head, grain.lanes)
                    : readHead(mapSourceChannel(channel), head);
                const float spatialGain = grain.channelGains[channel];
                stereo[channel] += sample * gain * spatialGain;
                envelopeGainSums[channel] += gain * spatialGain;
            }
            advanceEventPosition(grain.position,
                                 grain.increment,
                                 grain.low,
                                 grain.high,
                                 grain.seam);
            if (++grain.age >= grain.duration) {
                grain.active = false;
            }
        }
        // A live Size/Density move can leave old grains carrying the previous
        // overlap gain while new grains enter with the new one. Keep their
        // continuously varying envelope weights a convex sum so correlated
        // full-scale material cannot briefly overload the output. Grain starts
        // and ends have zero envelope, so this guard introduces no gain step.
        for (std::size_t channel = 0u;
             channel < kLaneChannels;
             ++channel) {
            if (envelopeGainSums[channel] > 1.0f) {
                stereo[channel] /= envelopeGainSums[channel];
            }
        }
        for (std::size_t channel = 0u; channel < kChannels; ++channel) {
            wet[channel] = stereo[channel & 1u];
        }

        const double phaseIncrement = static_cast<double>(speed_)
            * static_cast<double>(sourceSampleRate_)
            / static_cast<double>(outputSampleRate_) / width;
        phase_ = wrapPhase(phase_ + phaseIncrement);
    }

    void prepareLongGrainControls(const Controls& controls)
    {
        const float durationSeconds
            = clamp(controls.longGrainLengthMs, 250.0f, 12000.0f) * 0.001f;
        longGrainDurationFrames_ = std::max<std::size_t>(8u,
            static_cast<std::size_t>(outputSampleRate_ * durationSeconds));
        longGrainLayerLimit_ = static_cast<std::size_t>(clamp(
            std::round(controls.longGrainLayers), 1.0f,
            static_cast<float>(kLongGrainVoiceCount)));
        const float maximumDurationScale = 1.0f
            + clamp01(controls.longGrainDurationVariation) * 0.75f;
        longGrainIntervalFrames_ = std::max(1.0,
            static_cast<double>(longGrainDurationFrames_)
                * static_cast<double>(maximumDurationScale)
                / static_cast<double>(longGrainLayerLimit_));
        longGrainPitch_ = clamp(controls.longGrainPitch, 0.25f, 4.0f);
    }

    float longGrainEnvelope(const LongGrain& grain) const
    {
        constexpr float pi = 3.14159265358979323846f;
        const float phase = static_cast<float>(grain.age)
            / static_cast<float>(std::max<std::size_t>(
                1u, grain.duration - 1u));
        switch (grain.shape) {
            case LongGrainShape::bell:
                return smoothBell(phase);
            case LongGrainShape::hann:
                return 0.5f - 0.5f * std::cos(2.0f * pi * phase);
            case LongGrainShape::triangle:
                return 1.0f - std::abs(phase * 2.0f - 1.0f);
            case LongGrainShape::plateau: {
                const std::size_t edge = std::max<std::size_t>(1u,
                    std::min(grain.edgeFrames, grain.duration / 2u));
                const float fadeIn = static_cast<float>(grain.age)
                    / static_cast<float>(edge);
                const float fadeOut = static_cast<float>(
                    grain.duration - 1u - grain.age)
                    / static_cast<float>(edge);
                return smoothStep(std::min(fadeIn, fadeOut));
            }
        }
        return 0.0f;
    }

    void startLongGrain(const Controls& controls, const Window& window)
    {
        LongGrain* slot = nullptr;
        for (std::size_t index = 0u;
             index < longGrainLayerLimit_;
             ++index) {
            if (!longGrains_[index].active) {
                slot = &longGrains_[index];
                break;
            }
        }
        if (!slot) return;

        const double width = std::max(1.0, window.high - window.low);
        const std::size_t layer = longGrainSerial_ % longGrainLayerLimit_;
        const double sourcePhase
            = controls.longGrainAdvance == GrainAdvance::grain
                ? longGrainSourcePhase_ : phase_;
        double origin = sourcePhase;
        switch (controls.longGrainMode) {
            case GrainMode::freeze:
                origin = 0.5;
                break;
            case GrainMode::cloud:
                origin = static_cast<double>(randomUnit());
                break;
            case GrainMode::slice:
                origin = (static_cast<double>(layer) + 0.5)
                    / static_cast<double>(longGrainLayerLimit_);
                break;
            case GrainMode::scan:
                break;
        }
        origin = wrapPhase(origin
            + static_cast<double>(clamp01(controls.longGrainPhaseSpread))
                * static_cast<double>(layer)
                / static_cast<double>(longGrainLayerLimit_));

        float sprayDirection = randomUnit() * 2.0f - 1.0f;
        if (controls.longGrainBias == GrainBias::behind) {
            sprayDirection = -randomUnit();
        } else if (controls.longGrainBias == GrainBias::ahead) {
            sprayDirection = randomUnit();
        }
        origin = wrapPhase(origin + static_cast<double>(sprayDirection)
            * static_cast<double>(clamp01(controls.longGrainSpray)) * 0.5);

        *slot = {};
        slot->active = true;
        const float durationScale = std::max(0.25f,
            1.0f + (randomUnit() * 2.0f - 1.0f)
                * clamp01(controls.longGrainDurationVariation) * 0.75f);
        slot->duration = std::max<std::size_t>(8u,
            static_cast<std::size_t>(
                static_cast<float>(longGrainDurationFrames_) * durationScale));
        slot->edgeFrames = std::max<std::size_t>(1u,
            static_cast<std::size_t>(outputSampleRate_ * 0.001f
                * clamp(controls.longGrainEdgeMs, 5.0f, 2000.0f)));
        slot->position = window.low + origin * width;
        const float randomSemitones = (randomUnit() * 2.0f - 1.0f)
            * clamp(controls.longGrainPitchSpray, 0.0f, 24.0f);
        float eventPitch = longGrainPitch_
            * clamp(controls.notePitch, 0.001f, 512.0f)
            * std::pow(2.0f, randomSemitones / 12.0f);
        if (randomUnit() < clamp01(controls.longGrainReverseChance)) {
            eventPitch = -eventPitch;
        }
        slot->increment = static_cast<double>(sourceSampleRate_)
            / static_cast<double>(outputSampleRate_)
            * static_cast<double>(eventPitch);
        slot->low = window.low;
        slot->high = window.high;
        slot->seam = joinFrames(window, controls.join);
        slot->lanes = stackMode_
            ? makeLaneBlend(lanePhase_, controls) : LaneBlend {};
        slot->gain = 1.0f - randomUnit()
            * clamp01(controls.longGrainLevelVariation);
        slot->shape = controls.longGrainShape;
        const float spread = clamp01(controls.longGrainSpread);
        const float pan = spread > 0.0f
            ? (randomUnit() * 2.0f - 1.0f) * spread : 0.0f;
        constexpr float quarterPi = 0.78539816339744830962f;
        constexpr float rootTwo = 1.41421356237309504880f;
        const float panAngle = (pan + 1.0f) * quarterPi;
        slot->channelGains[0u] = rootTwo * std::cos(panAngle);
        slot->channelGains[1u] = rootTwo * std::sin(panAngle);

        if (controls.longGrainAdvance == GrainAdvance::grain) {
            const double sourceAdvance = static_cast<double>(speed_)
                * static_cast<double>(sourceSampleRate_)
                / static_cast<double>(outputSampleRate_) / width
                * longGrainIntervalFrames_;
            longGrainSourcePhase_ = wrapPhase(
                longGrainSourcePhase_ + sourceAdvance);
        }
        ++longGrainSerial_;
    }

    void renderLongGrainsFrame(const Controls& controls, float* wet)
    {
        const Window window = smoothEventWindow(controls);
        fieldLow_ = window.low;
        fieldHigh_ = window.high;
        const double width = std::max(1.0, window.high - window.low);
        displayPosition_ = window.low + phase_ * width;

        if (longGrainCountdown_ <= 0.0) {
            startLongGrain(controls, window);
            longGrainCountdown_ += longGrainIntervalFrames_;
        }
        longGrainCountdown_ -= 1.0;

        float stereo[kLaneChannels] {};
        float envelopeGainSums[kLaneChannels] {};
        for (auto& grain : longGrains_) {
            if (!grain.active) continue;
            const float gain = grain.gain * longGrainEnvelope(grain);
            const ReadHead head = makeEventReadHead(
                grain.position, grain.increment,
                grain.low, grain.high, grain.seam);
            for (std::size_t channel = 0u;
                 channel < kLaneChannels;
                 ++channel) {
                const float sample = stackMode_
                    ? readLaneBlendHead(channel, head, grain.lanes)
                    : readHead(mapSourceChannel(channel), head);
                const float weightedGain = gain * grain.channelGains[channel];
                stereo[channel] += sample * weightedGain;
                envelopeGainSums[channel] += weightedGain;
            }
            advanceEventPosition(grain.position, grain.increment,
                                 grain.low, grain.high, grain.seam);
            if (++grain.age >= grain.duration) grain.active = false;
        }
        for (std::size_t channel = 0u;
             channel < kLaneChannels;
             ++channel) {
            if (envelopeGainSums[channel] > 1.0f) {
                stereo[channel] /= envelopeGainSums[channel];
            }
        }
        for (std::size_t channel = 0u; channel < kChannels; ++channel) {
            wet[channel] = stereo[channel & 1u];
        }

        const double phaseIncrement = static_cast<double>(speed_)
            * static_cast<double>(sourceSampleRate_)
            / static_cast<double>(outputSampleRate_) / width;
        phase_ = wrapPhase(phase_ + phaseIncrement);
    }

    std::size_t cutRegion(CutupsMode mode, std::size_t regionCount)
    {
        regionCount = std::max<std::size_t>(1u, regionCount);
        switch (mode) {
            case CutupsMode::reverse:
                return regionCount - 1u - cutStep_ % regionCount;
            case CutupsMode::palindrome: {
                if (regionCount <= 1u) return 0u;
                const std::size_t period = (regionCount - 1u) * 2u;
                const std::size_t phase = cutStep_ % period;
                return phase < regionCount ? phase : period - phase;
            }
            case CutupsMode::random:
                return static_cast<std::size_t>(randomUnit()
                    * static_cast<float>(regionCount)) % regionCount;
            case CutupsMode::randomCycle: {
                std::uint8_t bag[kRegionCount] {};
                for (std::size_t index = 0u; index < regionCount; ++index) {
                    bag[index] = static_cast<std::uint8_t>(index);
                }
                std::uint32_t state = 0x51f15e5du
                    ^ static_cast<std::uint32_t>(
                        cutStep_ / regionCount + 1u) * 0x85ebca6bu;
                for (std::size_t remaining = regionCount;
                     remaining > 1u; --remaining) {
                    state ^= state << 13u;
                    state ^= state >> 17u;
                    state ^= state << 5u;
                    std::swap(bag[remaining - 1u], bag[state % remaining]);
                }
                return bag[cutStep_ % regionCount];
            }
            case CutupsMode::walk:
                if (cutStep_ != 0u) {
                    const int move = randomUnit() < 0.5f ? -1 : 1;
                    cutWalkRegion_ = static_cast<std::size_t>((
                        static_cast<int>(cutWalkRegion_) + move
                        + static_cast<int>(regionCount))
                        % static_cast<int>(regionCount));
                }
                return cutWalkRegion_ % regionCount;
            case CutupsMode::forward:
                return cutStep_ % regionCount;
        }
        return 0u;
    }

    std::size_t cutLane(const Controls& controls)
    {
        std::size_t loaded[kStackLanes] {};
        std::size_t count = 0u;
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            if (laneFrames_[lane] != 0u) loaded[count++] = lane;
        }
        if (count == 0u) return kNoLane;
        std::size_t voiceOffset = 0u;
        const std::size_t voice = static_cast<std::size_t>(std::max(
            0.0f, std::floor(controls.cutVoiceIndex)));
        switch (controls.cutPolyPath) {
            case CutPolyPath::stagger: voiceOffset = voice; break;
            case CutPolyPath::rotate: voiceOffset = voice + cutStep_; break;
            case CutPolyPath::random:
                voiceOffset = static_cast<std::size_t>(hashUnit(
                    static_cast<std::uint32_t>(voice * 131u + cutStep_))
                    * static_cast<float>(count));
                break;
            case CutPolyPath::together: break;
        }
        const std::size_t step = cutStep_ + voiceOffset;
        std::size_t ordered = 0u;
        switch (controls.cutLaneOrder) {
            case CutLaneOrder::up:
                ordered = count - 1u - step % count;
                break;
            case CutLaneOrder::palindrome: {
                if (count <= 1u) return loaded[0u];
                const std::size_t period = (count - 1u) * 2u;
                const std::size_t phase = step % period;
                ordered = phase < count ? phase : period - phase;
                break;
            }
            case CutLaneOrder::random:
                ordered = static_cast<std::size_t>(randomUnit()
                    * static_cast<float>(count)) % count;
                break;
            case CutLaneOrder::pairs: {
                constexpr std::size_t pairOrder[4u] { 0u, 1u, 2u, 3u };
                ordered = pairOrder[step % 4u] % count;
                break;
            }
            case CutLaneOrder::outsideIn: {
                constexpr std::size_t outside[4u] { 0u, 3u, 1u, 2u };
                ordered = outside[step % 4u] % count;
                break;
            }
            case CutLaneOrder::centerOut: {
                constexpr std::size_t center[4u] { 1u, 2u, 0u, 3u };
                ordered = center[step % 4u] % count;
                break;
            }
            case CutLaneOrder::down:
                ordered = step % count;
                break;
        }
        return loaded[ordered];
    }

    void beginCut(const Controls& controls,
                  const Window& window,
                  std::size_t intervalFrames)
    {
        const std::size_t requestedJoin = static_cast<std::size_t>(
            outputSampleRate_
            * clamp(controls.cutJoinMs, 0.0f, 100.0f) * 0.001f);
        const std::size_t join = std::max<std::size_t>(1u,
            std::min<std::size_t>(
                std::max(safetyFadeFrames_, requestedJoin),
                intervalFrames / 2u));
        if (cutCurrent_.active) {
            cutTail_ = cutCurrent_;
            cutTail_.retiring = true;
            cutTail_.retirementAge = 0u;
            cutTail_.joinFrames = join;
        } else {
            cutTail_ = {};
        }

        cutCurrent_ = {};
        cutCurrent_.active = true;
        cutCurrent_.joinFrames = join;
        cutCurrent_.gateFrames = std::max<std::size_t>(1u,
            static_cast<std::size_t>(static_cast<float>(intervalFrames)
                * clamp(controls.cutGate, 0.05f, 1.0f)));
        cutCurrent_.lane = stackMode_ ? cutLane(controls) : kNoLane;
        cutCurrent_.gain = 1.0f - randomUnit()
            * clamp01(controls.cutLevelVariation);
        const double width = std::max(1.0, window.high - window.low);
        const bool transientRegions = stackMode_
            && cutCurrent_.lane < kStackLanes
            && controls.cutRegionMode == RegionMode::transients
            && laneRegionCounts_[cutCurrent_.lane] != 0u;
        const std::size_t regions = transientRegions
            ? laneRegionCounts_[cutCurrent_.lane]
            : static_cast<std::size_t>(clamp(
                controls.cutRegionCount, 1.0f,
                static_cast<float>(kCutRegionCount)));
        const std::size_t repeats = static_cast<std::size_t>(clamp(
            controls.cutRepeats, 1.0f, 16.0f));
        if (cutRepeatIndex_ == 0u) {
            cutRepeatedRegion_ = cutRegion(controls.cutups, regions);
        }
        const std::size_t region = cutRepeatedRegion_ % regions;
        const double regionLow = transientRegions
            ? window.low + width * static_cast<double>(
                laneRegionBoundaries_[cutCurrent_.lane][region])
            : window.low + static_cast<double>(region)
                * width / static_cast<double>(regions);
        const double regionHigh = transientRegions
            ? window.low + width * static_cast<double>(
                laneRegionBoundaries_[cutCurrent_.lane][region + 1u])
            : regionLow + width / static_cast<double>(regions);
        const double regionWidth = std::max(1.0, regionHigh - regionLow);
        const bool reverse = randomUnit()
            < clamp01(controls.cutReverseChance);
        cutCurrent_.position = reverse
            ? std::min(window.high - 1.0, regionLow + regionWidth - 1.0)
            : regionLow;
        const float pitchVariation = clamp(
            controls.cutPitchVariation, 0.0f, 24.0f);
        const float semitones = (randomUnit() * 2.0f - 1.0f)
            * pitchVariation;
        const double pitch = static_cast<double>(clamp(
            controls.cutPitch, 0.0625f, 16.0f))
            * std::pow(2.0, static_cast<double>(semitones) / 12.0);
        const float laneRate = cutCurrent_.lane < kStackLanes
            ? laneSampleRates_[cutCurrent_.lane] : sourceSampleRate_;
        const double laneScale = cutCurrent_.lane < kStackLanes
            ? std::max(1.0e-12, lanePositionScales_[cutCurrent_.lane]) : 1.0;
        cutCurrent_.increment = static_cast<double>(laneRate)
            / static_cast<double>(outputSampleRate_) / laneScale * pitch
            * (reverse ? -1.0 : 1.0);
        cutCurrent_.low = window.low;
        cutCurrent_.high = window.high;
        cutCurrent_.seam = joinFrames(window, controls.join);

        cutRepeatIndex_ = (cutRepeatIndex_ + 1u) % repeats;
        if (cutRepeatIndex_ == 0u) ++cutStep_;
    }

    std::size_t requestedSampleLane(const Controls& controls) const
    {
        const std::size_t requested = static_cast<std::size_t>(clamp(
            std::floor(controls.sampleLane + 0.5f), 0.0f,
            static_cast<float>(kStackLanes - 1u)));
        if (stackMode_ && laneFrames_[requested] != 0u) return requested;
        if (stackMode_) {
            for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
                if (laneFrames_[lane] != 0u) return lane;
            }
        }
        return kNoLane;
    }

    static bool sameSampleConfig(const SampleReaderConfig& a,
                                 const SampleReaderConfig& b)
    {
        return a.mode == b.mode && a.regionMode == b.regionMode
            && a.regionCount == b.regionCount && a.slice == b.slice
            && a.lane == b.lane && a.sliced == b.sliced;
    }

    SampleReaderConfig requestedSampleConfig(
        const Controls& controls, bool sliced) const
    {
        SampleReaderConfig config;
        config.mode = controls.sampleMode;
        config.regionMode = controls.sampleRegionMode;
        config.regionCount = static_cast<std::size_t>(clamp(
            std::round(controls.sampleRegionCount), 1.0f,
            static_cast<float>(kRegionCount)));
        config.slice = static_cast<std::size_t>(std::max(
            0.0f, std::floor(controls.sampleSlice)));
        config.lane = requestedSampleLane(controls);
        config.sliced = sliced;
        return config;
    }

    void prepareSampleConfig(const Controls& controls, bool sliced)
    {
        const SampleReaderConfig requested
            = requestedSampleConfig(controls, sliced);
        if (!sampleConfigInitialized_) {
            currentSampleConfig_ = requested;
            pendingSampleConfig_ = requested;
            currentSampleMode_ = requested.mode;
            sampleConfigInitialized_ = true;
            return;
        }
        if (sameSampleConfig(requested, currentSampleConfig_)
            && !sampleConfigPending_) return;
        pendingSampleConfig_ = requested;
        sampleConfigPending_ = !sameSampleConfig(
            pendingSampleConfig_, currentSampleConfig_);
        if (!sampleConfigPending_) {
            sampleTransitionRemaining_ = 0u;
        } else if (sampleTransitionRemaining_ == 0u) {
            sampleTransitionRemaining_ = readerTransitionFrames_ * 2u;
        }
    }

    float sampleTransitionGain()
    {
        if (sampleTransitionRemaining_ == 0u) return 1.0f;
        if (sampleTransitionRemaining_ == readerTransitionFrames_) {
            currentSampleConfig_ = pendingSampleConfig_;
            currentSampleMode_ = currentSampleConfig_.mode;
            phase_ = 0.0;
            sampleDone_ = false;
            sampleConfigPending_ = false;
        }
        if (sampleTransitionRemaining_ > readerTransitionFrames_) {
            return smoothStep(static_cast<float>(
                sampleTransitionRemaining_ - readerTransitionFrames_)
                / static_cast<float>(readerTransitionFrames_));
        }
        return smoothStep(1.0f
            - static_cast<float>(sampleTransitionRemaining_)
                / static_cast<float>(readerTransitionFrames_));
    }

    void advanceSampleTransition()
    {
        if (sampleTransitionRemaining_ != 0u) {
            --sampleTransitionRemaining_;
        }
    }

    void renderSampleFrame(const Controls& controls, bool sliced, float* wet)
    {
        prepareSampleConfig(controls, sliced);
        const SampleReaderConfig& config = currentSampleConfig_;
        const Window window = smoothEventWindow(controls);
        fieldLow_ = window.low;
        fieldHigh_ = window.high;
        const std::size_t lane = config.lane;
        const double fieldWidth = std::max(1.0, window.high - window.low);
        double low = window.low;
        double high = window.high;
        if (config.sliced) {
            const bool transientRegions = lane < kStackLanes
                && config.regionMode == RegionMode::transients
                && laneRegionCounts_[lane] != 0u;
            const std::size_t regions = transientRegions
                ? laneRegionCounts_[lane]
                : config.regionCount;
            const std::size_t region = config.slice % regions;
            const double start = transientRegions
                ? static_cast<double>(laneRegionBoundaries_[lane][region])
                : static_cast<double>(region) / static_cast<double>(regions);
            const double end = transientRegions
                ? static_cast<double>(laneRegionBoundaries_[lane][region + 1u])
                : static_cast<double>(region + 1u) / static_cast<double>(regions);
            low = window.low + fieldWidth * start;
            high = window.low + fieldWidth * end;
        }
        const double width = std::max(1.0, high - low);
        const double laneScale = lane < kStackLanes
            ? std::max(1.0e-12, lanePositionScales_[lane]) : 1.0;
        const float laneRate = lane < kStackLanes
            ? laneSampleRates_[lane] : sourceSampleRate_;
        double increment = static_cast<double>(speed_)
            * static_cast<double>(laneRate)
            / static_cast<double>(outputSampleRate_) / laneScale / width;
        // A ping-pong cycle contains both a forward and reverse traversal.
        // Halving its phase rate keeps the audible pitch equal to the other
        // sample modes instead of making both legs run at double speed.
        if (config.mode == SampleMode::pingPong) increment *= 0.5;
        double readPhase = phase_;
        if (config.mode == SampleMode::reverse) {
            readPhase = 1.0 - phase_;
        } else if (config.mode == SampleMode::pingPong) {
            readPhase = phase_ < 0.5 ? phase_ * 2.0 : 2.0 - phase_ * 2.0;
        }
        const double position = clamp(low + readPhase * width,
                                      low, std::max(low, high - 1.0));
        const double seam = config.mode == SampleMode::loop
            ? joinFrames({ low, high }, controls.join) : 0.0;
        const ReadHead head = makeEventReadHead(
            position, increment, low, high, seam);
        displayPosition_ = position;
        float completionGain = 1.0f;
        if ((config.mode == SampleMode::forward
                || config.mode == SampleMode::reverse)
            && !sampleDone_) {
            const double outputFramesRemaining = increment > 0.0
                ? std::max(0.0, 1.0 - phase_) / increment : 0.0;
            completionGain = smoothStep(static_cast<float>(clamp(
                outputFramesRemaining
                    / static_cast<double>(safetyFadeFrames_),
                0.0, 1.0)));
        }
        for (std::size_t channel = 0u; channel < kChannels; ++channel) {
            if (sampleDone_) {
                wet[channel] = 0.0f;
            } else if (stackMode_ && lane < kStackLanes) {
                wet[channel] = readLaneHead(lane, channel & 1u, head)
                    * activeControlsLaneGains_[lane];
            } else {
                wet[channel] = readHead(mapSourceChannel(channel), head);
            }
            wet[channel] *= completionGain;
        }
        phase_ += increment;
        if (config.mode == SampleMode::loop) {
            if (phase_ >= 1.0) {
                // The incoming seam has already advanced from low to
                // low+seam during the crossfade. Continue there after the
                // first traversal instead of replaying that segment twice.
                const double loopStart = seam / width;
                const double cycle = std::max(1.0 / width, 1.0 - loopStart);
                phase_ = loopStart + std::fmod(phase_ - 1.0, cycle);
            }
        } else if (config.mode == SampleMode::pingPong) {
            phase_ = wrapPhase(phase_);
        } else if (phase_ >= 1.0) {
            phase_ = 1.0;
            sampleDone_ = true;
        }
    }

    float readCut(const CutReader& reader, std::size_t channel) const
    {
        if (!reader.active) {
            return 0.0f;
        }
        const ReadHead head = makeEventReadHead(
            reader.position, reader.increment,
            reader.low, reader.high, reader.seam);
        const float sample = stackMode_ && reader.lane < kStackLanes
            ? readLaneHead(reader.lane, channel, head)
            : readHead(mapSourceChannel(channel), head);
        if (reader.retiring) {
            const float mix = smoothStep(static_cast<float>(
                reader.retirementAge) / static_cast<float>(
                    std::max<std::size_t>(1u, reader.joinFrames)));
            return sample * (1.0f - mix) * reader.gain;
        }
        if (reader.age < reader.joinFrames) {
            const float mix = smoothStep(static_cast<float>(reader.age + 1u)
                / static_cast<float>(reader.joinFrames));
            return sample * mix * reader.gain;
        }
        if (reader.age >= reader.gateFrames) return 0.0f;
        const std::size_t fadeStart = reader.gateFrames > reader.joinFrames
            ? reader.gateFrames - reader.joinFrames : 0u;
        if (reader.age > fadeStart) {
            const float mix = smoothStep(static_cast<float>(
                reader.age - fadeStart) / static_cast<float>(
                    std::max<std::size_t>(1u, reader.joinFrames)));
            return sample * (1.0f - mix) * reader.gain;
        }
        return sample * reader.gain;
    }

    void advanceCutReader(CutReader& reader)
    {
        if (!reader.active) {
            return;
        }
        advanceEventPosition(reader.position,
                             reader.increment,
                             reader.low,
                             reader.high,
                             reader.seam);
        ++reader.age;
        if (!reader.retiring && reader.age >= reader.gateFrames) {
            reader.active = false;
        }
        if (reader.retiring && ++reader.retirementAge >= reader.joinFrames) {
            reader.active = false;
        }
    }

    void renderCutupsFrame(const Controls& controls, float* wet)
    {
        const Window window = smoothEventWindow(controls);
        fieldLow_ = window.low;
        fieldHigh_ = window.high;
        if (cutCountdown_ <= 0.0) {
            double interval = 1.0;
            if (controls.cutClock == CutClock::tempo) {
                constexpr float beats[kCutDivisionCount] {
                    1.0f, 0.5f, 1.0f / 3.0f, 0.25f,
                    1.0f / 6.0f, 0.125f,
                };
                const std::size_t division = std::min<std::size_t>(
                    static_cast<std::size_t>(controls.cutDivision),
                    kCutDivisionCount - 1u);
                interval = static_cast<double>(outputSampleRate_) * 60.0
                    / static_cast<double>(clamp(
                        controls.cutTempoBpm, 20.0f, 300.0f))
                    * static_cast<double>(beats[division]);
                const float swing = clamp(controls.cutSwing, 0.5f, 0.75f);
                interval *= cutSwingPhase_
                    ? 2.0 * static_cast<double>(1.0f - swing)
                    : 2.0 * static_cast<double>(swing);
                cutSwingPhase_ = !cutSwingPhase_;
            } else {
                interval = static_cast<double>(outputSampleRate_)
                    / static_cast<double>(clamp(
                        controls.cutRateHz, 0.25f, 64.0f));
            }
            const float timeVariation = clamp01(controls.cutTimeVariation);
            interval *= 1.0 + static_cast<double>(
                (randomUnit() * 2.0f - 1.0f) * timeVariation * 0.9f);
            const std::size_t intervalFrames = std::max<std::size_t>(1u,
                static_cast<std::size_t>(interval));
            beginCut(controls, window, intervalFrames);
            cutCountdown_ += static_cast<double>(intervalFrames);
        }
        cutCountdown_ -= 1.0;

        float stereo[kLaneChannels] {};
        for (std::size_t channel = 0u;
             channel < kLaneChannels;
             ++channel) {
            stereo[channel] = readCut(cutTail_, channel)
                + readCut(cutCurrent_, channel);
        }
        for (std::size_t channel = 0u; channel < kChannels; ++channel) {
            wet[channel] = stereo[channel & 1u];
        }
        displayPosition_ = cutCurrent_.position;
        advanceCutReader(cutTail_);
        advanceCutReader(cutCurrent_);
    }

    std::size_t wavesetGroupSize(const Controls& controls) const
    {
        const std::size_t requested = static_cast<std::size_t>(clamp(
            std::round(controls.wavesetGroup), 1.0f, 32.0f));
        constexpr std::size_t choices[6u] { 1u, 2u, 4u, 8u, 16u, 32u };
        std::size_t selected = choices[0u];
        for (const std::size_t choice : choices) {
            if (std::abs(static_cast<int>(choice) - static_cast<int>(requested))
                < std::abs(static_cast<int>(selected)
                    - static_cast<int>(requested))) selected = choice;
        }
        return selected;
    }

    WavesetDiscreteControls wavesetDiscreteControls(
        const Controls& controls) const
    {
        WavesetDiscreteControls result;
        result.groupSize = wavesetGroupSize(controls);
        result.repeats = static_cast<std::size_t>(clamp(
            std::round(controls.wavesetRepeats), 1.0f, 16.0f));
        result.stride = static_cast<std::size_t>(clamp(
            std::round(controls.wavesetStride), 1.0f, 16.0f));
        result.advance = controls.wavesetAdvance;
        result.direction = controls.wavesetDirection;
        result.process = controls.wavesetProcess;
        result.detail = controls.wavesetDetail;
        return result;
    }

    void prepareWavesetControlsBlock(const Controls& requested,
                                     std::size_t frames,
                                     Controls& prepared)
    {
        (void)frames;
        const WavesetDiscreteControls incoming
            = wavesetDiscreteControls(requested);
        // Discrete choices are consumed at cycle/group boundaries below.
        // Do not add a second, time-based debounce: it made the audible state
        // lag behind both the encoder and the desktop plug-in.
        wavesetActiveControls_ = incoming;
        prepared.wavesetGroup
            = static_cast<float>(wavesetActiveControls_.groupSize);
        prepared.wavesetRepeats
            = static_cast<float>(wavesetActiveControls_.repeats);
        prepared.wavesetStride
            = static_cast<float>(wavesetActiveControls_.stride);
        prepared.wavesetAdvance = wavesetActiveControls_.advance;
        prepared.wavesetDirection = wavesetActiveControls_.direction;
        prepared.wavesetProcess = wavesetActiveControls_.process;
        prepared.wavesetDetail = wavesetActiveControls_.detail;
    }

    static constexpr std::size_t wavesetMinimumCycleFrames(
        WavesetDetail)
    {
        return 4u;
    }

    void prepareWavesetAnalysisFilters()
    {
        constexpr float cutoffs[kWavesetDetailCount] {
            0.0f, 8000.0f, 4000.0f, 1000.0f, 250.0f,
        };
        const float rate = std::max(1.0f, sourceSampleRate_);
        for (std::size_t index = 0u;
             index < kWavesetDetailCount; ++index) {
            const float cutoff = cutoffs[index];
            wavesetAnalysisCoefficients_[index] = cutoff > 0.0f
                ? 1.0f - std::exp(-2.0f * 3.14159265358979323846f
                    * cutoff / rate)
                : 1.0f;
            wavesetAnalysisWarmupFrames_[index] = cutoff > 0.0f
                ? std::min<std::size_t>(256u, static_cast<std::size_t>(
                    std::ceil(rate / cutoff * 4.0f)))
                : 0u;
        }
    }

    float wavesetChannelSample(std::size_t channel, double position,
                               const LaneBlend& lanes) const
    {
        const auto linearRead = [](const float* data, std::size_t frames,
                                   double at) {
            if (!data || frames == 0u) return 0.0f;
            at = clamp(at, 0.0, static_cast<double>(frames - 1u));
            const std::size_t first = static_cast<std::size_t>(at);
            const std::size_t next = std::min(first + 1u, frames - 1u);
            const float fraction = static_cast<float>(at - first);
            return data[first] + (data[next] - data[first]) * fraction;
        };
        if (!stackMode_) {
            return linearRead(storage_[mapSourceChannel(channel)],
                sourceFrames_, position);
        }
        const auto laneRead = [&](std::size_t lane) {
            if (lane >= kStackLanes || laneFrames_[lane] == 0u
                || sourceFrames_ == 0u) return 0.0f;
            const double at = clamp(position * lanePositionScales_[lane],
                0.0, static_cast<double>(laneFrames_[lane] - 1u));
            return linearRead(laneData(lane, channel), laneFrames_[lane], at)
                * activeControlsLaneGains_[lane];
        };
        const std::size_t first = std::min(lanes.first, kStackLanes - 1u);
        const std::size_t second = std::min(lanes.second, kStackLanes - 1u);
        const float a = laneRead(first);
        if (first == second || lanes.secondGain <= 0.0f) return a;
        return a * lanes.firstGain + laneRead(second) * lanes.secondGain;
    }

    std::size_t wavesetReferenceChannel(double cursor, const Window& window,
                                        const LaneBlend& lanes) const
    {
        (void)cursor;
        (void)window;
        (void)lanes;
        // True-channel playback in the desktop engine uses channel 0 as its
        // phase reference. A level-dependent channel choice made group timing
        // change unpredictably while the stack moved.
        return 0u;
    }

    bool findNextWavesetCrossing(double from, double high,
                                 const LaneBlend& lanes,
                                 std::size_t referenceChannel,
                                 WavesetDetail detail,
                                 std::size_t& searchBudget,
                                 double& crossing) const
    {
        if (!(high > from + 1.0) || searchBudget == 0u) return false;
        constexpr std::size_t kMaximumSearchFrames = 4096u;
        const std::size_t target = static_cast<std::size_t>(std::max(
            1.0, std::ceil(from)));
        const std::size_t detailIndex = std::min<std::size_t>(
            static_cast<std::size_t>(detail), kWavesetDetailCount - 1u);
        const float coefficient = wavesetAnalysisCoefficients_[detailIndex];
        const std::size_t warmup
            = wavesetAnalysisWarmupFrames_[detailIndex];
        std::size_t frame = target > warmup ? target - warmup : 1u;
        const std::size_t end = std::min<std::size_t>(
            static_cast<std::size_t>(std::floor(high)),
            target + kMaximumSearchFrames);
        float filtered = wavesetChannelSample(referenceChannel,
            static_cast<double>(frame - 1u), lanes);
        float previous = filtered;
        for (; frame <= end && searchBudget != 0u;
             ++frame, --searchBudget) {
            const float raw = wavesetChannelSample(referenceChannel,
                static_cast<double>(frame), lanes);
            filtered += coefficient * (raw - filtered);
            if (frame >= target && previous <= 0.0f && filtered > 0.0f) {
                const float denominator = filtered - previous;
                const float fraction = std::abs(denominator) > 1.0e-12f
                    ? clamp(-previous / denominator, 0.0f, 1.0f) : 0.0f;
                const double candidateCrossing
                    = static_cast<double>(frame - 1u)
                    + fraction;
                if (candidateCrossing > from + 1.0e-6) {
                    crossing = candidateCrossing;
                    return true;
                }
            }
            previous = filtered;
        }
        return false;
    }

    double wrapWavesetCursor(double cursor, const Window& window) const
    {
        const double width = std::max(1.0, window.high - window.low);
        while (cursor >= window.high) cursor -= width;
        while (cursor < window.low) cursor += width;
        return cursor;
    }

    bool nextWavesetCrossing(double from, const Window& window,
                             const LaneBlend& lanes,
                             std::size_t referenceChannel,
                             WavesetDetail detail,
                             std::size_t& searchBudget,
                             double& crossing) const
    {
        from = wrapWavesetCursor(from, window);
        if (findNextWavesetCrossing(from, window.high, lanes,
                referenceChannel, detail, searchBudget, crossing)) {
            return true;
        }
        return findNextWavesetCrossing(window.low, from, lanes,
            referenceChannel, detail, searchBudget, crossing);
    }

    float wavesetPeak(const WavesetUnit& unit,
                      const LaneBlend& lanes,
                      std::size_t referenceChannel) const
    {
        const double length = std::max(1.0, unit.end - unit.start);
        const std::size_t probes = std::min<std::size_t>(
            16u, std::max<std::size_t>(4u,
                static_cast<std::size_t>(std::ceil(length))));
        float peak = 0.0f;
        for (std::size_t probe = 0u; probe < probes; ++probe) {
            const double phase = static_cast<double>(probe)
                / static_cast<double>(probes - 1u);
            peak = std::max(peak, std::abs(wavesetChannelSample(
                referenceChannel, unit.start + phase * length, lanes)));
        }
        return peak;
    }

    double wavesetFallbackLength(const Window& window) const
    {
        return std::min(
            std::max(32.0, static_cast<double>(sourceSampleRate_) * 0.020),
            std::max(1.0, window.high - window.low));
    }

    void configureWavesetFallback(WavesetReader& reader,
                                  const Window& window) const
    {
        const double length = wavesetFallbackLength(window);
        WavesetUnit fallback;
        fallback.start = wrapWavesetCursor(reader.groupCursor, window);
        fallback.end = std::min(window.high, fallback.start + length);
        if (!(fallback.end > fallback.start + 1.0)) {
            fallback.start = window.low;
            fallback.end = std::min(window.high, window.low + length);
        }
        if (!(fallback.end > fallback.start)) {
            fallback.end = fallback.start + 1.0;
        }
        fallback.peak = wavesetPeak(fallback, reader.lanes,
            reader.referenceChannel);
        reader.units[0u] = fallback;
        reader.groupCursor = fallback.start;
        reader.groupSize = 1u;
        reader.fallback = true;
        reader.initialized = true;
        reader.adjacentUnitValid = false;
        reader.analysisRetryFrames = std::max<std::size_t>(
            1u, static_cast<std::size_t>(outputSampleRate_ * 0.10f));
    }

    bool buildWavesetGroup(const Controls& controls, const Window& window,
                           double cursor)
    {
        const WavesetReader previous = waveset_;
        WavesetReader candidate = previous;
        candidate.lanes = stackMode_
            ? makeLaneBlend(lanePhase_, controls) : LaneBlend {};
        candidate.requestedGroupSize = wavesetGroupSize(controls);
        candidate.groupSize = candidate.requestedGroupSize;
        candidate.cycleOffset = 0u;
        candidate.repeatIndex = 0u;
        // Moving to another source group must retain the fractional phase
        // left after the completed cycle. The desktop voice does this; zeroing
        // it here shortened every group by up to one output sample and caused
        // an audible impulse at ordinary group advances.
        candidate.oscillatorPhase = previous.initialized
            ? previous.oscillatorPhase : 0.0;
        candidate.groupCursor = wrapWavesetCursor(cursor, window);
        candidate.repeats = static_cast<std::size_t>(clamp(
            std::round(controls.wavesetRepeats), 1.0f, 16.0f));
        candidate.stride = static_cast<std::size_t>(clamp(
            std::round(controls.wavesetStride), 1.0f, 16.0f));
        candidate.advance = controls.wavesetAdvance;
        candidate.direction = controls.wavesetDirection;
        candidate.process = controls.wavesetProcess;
        candidate.detail = controls.wavesetDetail;
        if (previous.initialized) {
            if (candidate.direction == WavesetDirection::pendulum) {
                candidate.pendulumForward = !previous.pendulumForward;
            }
            candidate.randomState
                = previous.randomState * 1664525u + 1013904223u;
        }
        if (!candidate.controlsInitialized) {
            candidate.processAmount = clamp01(controls.wavesetProcessAmount);
            candidate.join = clamp01(controls.join);
            candidate.controlsInitialized = true;
        }
        candidate.processMultiple = 2u + static_cast<std::size_t>(
            std::lround(candidate.processAmount * 6.0f));
        candidate.referenceChannel = wavesetReferenceChannel(
            candidate.groupCursor, window, candidate.lanes);
        candidate.strongestUnit = 0u;
        candidate.analysisRetryFrames = 0u;
        candidate.fallback = false;
        candidate.adjacentUnitValid = false;
        const std::size_t minimum
            = wavesetMinimumCycleFrames(candidate.detail);
        // This is a total group budget, not a per-crossing budget. It keeps
        // silence, DC, and very low material from multiplying the worst-case
        // callback cost by group size and polyphony.
        std::size_t searchBudget = 8192u;
        double start = 0.0;
        if (!nextWavesetCrossing(candidate.groupCursor, window,
                candidate.lanes, candidate.referenceChannel,
                candidate.detail, searchBudget, start)) {
            if (previous.initialized && !previous.fallback) {
                waveset_ = previous;
                waveset_.cycleOffset = 0u;
                waveset_.repeatIndex = 0u;
                waveset_.analysisRetryFrames = std::max<std::size_t>(
                    1u, static_cast<std::size_t>(outputSampleRate_ * 0.25f));
                return false;
            }
            // Keep normal playback available while bounded analysis advances
            // past a quiet lead-in or unusually long cycle. This temporary
            // 20 ms unit is endpoint-corrected during rendering and moves
            // through the Field instead of pinning the reader at its start.
            configureWavesetFallback(candidate, window);
            waveset_ = candidate;
            return false;
        }
        for (std::size_t index = 0u; index < candidate.groupSize; ++index) {
            double end = 0.0;
            const bool found = nextWavesetCrossing(
                start + static_cast<double>(minimum), window,
                candidate.lanes, candidate.referenceChannel,
                candidate.detail, searchBudget, end);
            if (!found || end <= start) {
                // Never create a cycle through the field wrap. Restart at the
                // first crossing after the low boundary. If analysis runs out
                // of safe crossings or its real-time budget, retain the prior
                // valid group rather than manufacturing a discontinuous cut.
                double wrappedStart = 0.0;
                double wrappedEnd = 0.0;
                const bool wrapped = findNextWavesetCrossing(
                    window.low, window.high, candidate.lanes,
                    candidate.referenceChannel, candidate.detail,
                    searchBudget, wrappedStart)
                    && findNextWavesetCrossing(
                        wrappedStart + static_cast<double>(minimum),
                        window.high, candidate.lanes,
                        candidate.referenceChannel, candidate.detail,
                        searchBudget, wrappedEnd);
                if (wrapped) {
                    start = wrappedStart;
                    end = wrappedEnd;
                } else {
                    if (previous.initialized && !previous.fallback) {
                        waveset_ = previous;
                        waveset_.cycleOffset = 0u;
                        waveset_.repeatIndex = 0u;
                        waveset_.analysisRetryFrames = std::max<std::size_t>(
                            1u, static_cast<std::size_t>(
                                outputSampleRate_ * 0.25f));
                        return false;
                    }
                    if (index == 0u) {
                        configureWavesetFallback(candidate, window);
                    } else {
                        // Reuse only completed safe cycles for an incomplete
                        // initial group. Repetition is preferable to an
                        // unmeasured final boundary.
                        candidate.groupSize = index;
                    }
                    break;
                }
            }
            WavesetUnit& unit = candidate.units[index];
            unit.start = start;
            unit.end = std::max(start + 1.0, end);
            unit.peak = wavesetPeak(unit, candidate.lanes,
                candidate.referenceChannel);
            if (unit.peak > candidate.units[candidate.strongestUnit].peak) {
                candidate.strongestUnit = index;
            }
            start = unit.end;
        }
        if (candidate.groupSize == 0u) {
            waveset_ = previous;
            return false;
        }
        // Interpolate targets the adjacent source cycle, not the next cycle
        // in the selected order. Retain one look-ahead unit for the final
        // member of the group.
        double adjacentEnd = 0.0;
        if (nextWavesetCrossing(start + static_cast<double>(minimum), window,
                candidate.lanes, candidate.referenceChannel,
                candidate.detail, searchBudget, adjacentEnd)
            && adjacentEnd > start) {
            candidate.adjacentUnit.start = start;
            candidate.adjacentUnit.end = adjacentEnd;
            candidate.adjacentUnit.peak = wavesetPeak(
                candidate.adjacentUnit, candidate.lanes,
                candidate.referenceChannel);
            candidate.adjacentUnitValid = true;
        }
        candidate.initialized = true;
        if (previous.initialized
            && wavesetControlTransitionRemaining_ == 0u
            && (previous.requestedGroupSize != candidate.requestedGroupSize
                || previous.repeats != candidate.repeats
                || previous.stride != candidate.stride
                || previous.advance != candidate.advance
                || previous.direction != candidate.direction
                || previous.process != candidate.process
                || previous.detail != candidate.detail)) {
            wavesetControlTransitionFrom_ = previous;
            wavesetControlTransitionLength_ = wavesetControlTransitionFrames_;
            wavesetControlTransitionRemaining_
                = wavesetControlTransitionLength_;
        }
        waveset_ = candidate;
        return true;
    }

    std::size_t orderedWavesetIndex(const WavesetReader& reader,
                                    std::size_t offset) const
    {
        const std::size_t count = std::max<std::size_t>(
            1u, reader.groupSize);
        offset %= count;
        switch (reader.direction) {
            case WavesetDirection::reverse:
                return count - 1u - offset;
            case WavesetDirection::pendulum:
                return reader.pendulumForward ? offset
                    : count - 1u - offset;
            case WavesetDirection::shuffle: {
                std::uint32_t mixed = reader.randomState
                    ^ (static_cast<std::uint32_t>(offset) * 0x9e3779b9u
                        + 0x7f4a7c15u);
                mixed ^= mixed >> 16u;
                return static_cast<std::size_t>(mixed) % count;
            }
            case WavesetDirection::forward:
                return offset;
        }
        return offset;
    }

    float wavesetUnitSample(const WavesetReader& reader,
                            std::size_t channel,
                            const WavesetUnit& unit,
                            double phase, float join) const
    {
        phase -= std::floor(phase);
        const double length = std::max(1.0, unit.end - unit.start);
        const double position = unit.start + phase * length;
        const auto read = [&](double at) {
            return wavesetChannelSample(channel, at, reader.lanes);
        };
        const float raw = read(position);
        const float start = read(unit.start);
        const float end = read(unit.end);
        const float boundary = start + (end - start)
            * static_cast<float>(phase);
        return raw - boundary * clamp01(join);
    }

    float wavesetAverageSample(const WavesetReader& reader,
                               std::size_t channel, double phase,
                               float join) const
    {
        const std::size_t count = reader.groupSize;
        float sum = 0.0f;
        for (std::size_t index = 0u; index < count; ++index) {
            sum += wavesetUnitSample(reader, channel,
                reader.units[orderedWavesetIndex(reader, index)],
                phase, join);
        }
        return sum / static_cast<float>(std::max<std::size_t>(1u, count));
    }

    const WavesetUnit& adjacentWavesetUnit(const WavesetReader& reader,
                                           std::size_t sourceIndex) const
    {
        if (sourceIndex + 1u < reader.groupSize) {
            return reader.units[sourceIndex + 1u];
        }
        return reader.adjacentUnitValid
            ? reader.adjacentUnit : reader.units[0u];
    }

    float renderWavesetSample(const WavesetReader& reader,
                              std::size_t channel, float join,
                              std::size_t cycleOffset,
                              std::size_t repeatIndex,
                              double oscillatorPhase) const
    {
        const std::size_t index = orderedWavesetIndex(
            reader, cycleOffset);
        const WavesetUnit& unit = reader.units[index];
        const float raw = wavesetUnitSample(reader, channel, unit,
            oscillatorPhase, join);
        const float amount = clamp01(reader.processAmount);
        const auto blend = [raw, amount](float processed) {
            return raw + (processed - raw) * amount;
        };
        switch (reader.process) {
            case WavesetProcess::omit: {
                std::uint32_t hash = static_cast<std::uint32_t>(
                    std::max(0.0, std::floor(reader.groupCursor)))
                    * 747796405u + 2891336453u;
                hash = ((hash >> ((hash >> 28u) + 4u)) ^ hash)
                    * 277803737u;
                hash = (hash >> 22u) ^ hash;
                const float choice = static_cast<float>(hash & 0xffffu)
                    / 65535.0f;
                return choice < amount
                    ? raw * (1.0f - amount) : raw;
            }
            case WavesetProcess::replace: {
                return blend(wavesetUnitSample(reader, channel,
                    reader.units[reader.strongestUnit],
                    oscillatorPhase,
                    join));
            }
            case WavesetProcess::envelope: {
                constexpr double pi = 3.14159265358979323846;
                const float envelope = static_cast<float>(std::sin(
                    pi * oscillatorPhase));
                return blend(raw * envelope * envelope);
            }
            case WavesetProcess::multiply: {
                return blend(wavesetUnitSample(reader, channel, unit,
                    oscillatorPhase
                        * static_cast<double>(reader.processMultiple), join));
            }
            case WavesetProcess::average:
                return blend(wavesetAverageSample(reader, channel,
                    oscillatorPhase, join));
            case WavesetProcess::interpolate: {
                const WavesetUnit& targetUnit
                    = adjacentWavesetUnit(reader, index);
                const float target = wavesetUnitSample(reader, channel,
                    targetUnit, oscillatorPhase, join);
                const float morph = clamp01(static_cast<float>(
                    (static_cast<double>(repeatIndex)
                        + oscillatorPhase)
                    / static_cast<double>(std::max<std::size_t>(1u,
                        reader.repeats))));
                return blend(raw + (target - raw) * morph);
            }
            case WavesetProcess::fractal: {
                float sum = raw;
                float normalization = 1.0f;
                for (std::size_t level = 1u; level <= 4u; ++level) {
                    const std::size_t multiple = 1u << level;
                    const float weight = 1.0f
                        / static_cast<float>(multiple);
                    sum += wavesetUnitSample(reader, channel, unit,
                        oscillatorPhase
                            * static_cast<double>(multiple), join)
                        * weight;
                    normalization += weight;
                }
                return blend(sum / normalization);
            }
            case WavesetProcess::harmonic: {
                const std::size_t highest = reader.processMultiple;
                float sum = raw;
                float normalization = 1.0f;
                for (std::size_t harmonic = 2u;
                     harmonic <= highest; ++harmonic) {
                    const float weight = 1.0f
                        / static_cast<float>(harmonic);
                    sum += wavesetUnitSample(reader, channel, unit,
                        oscillatorPhase
                            * static_cast<double>(harmonic), join) * weight;
                    normalization += weight;
                }
                return blend(sum / normalization);
            }
            case WavesetProcess::groupReverse: {
                const std::size_t reverse = reader.groupSize - 1u
                    - (cycleOffset % reader.groupSize);
                return blend(wavesetUnitSample(reader, channel,
                    reader.units[orderedWavesetIndex(reader, reverse)],
                    1.0 - oscillatorPhase, join));
            }
            case WavesetProcess::cycleReverse:
                return blend(wavesetUnitSample(reader, channel, unit,
                    1.0 - oscillatorPhase, join));
            case WavesetProcess::telescope: {
                double sum = 0.0;
                for (std::size_t offset = 0u;
                     offset < reader.groupSize; ++offset) {
                    sum += wavesetUnitSample(reader, channel,
                        reader.units[orderedWavesetIndex(reader, offset)],
                        oscillatorPhase, join);
                }
                return blend(static_cast<float>(std::tanh(sum
                    / std::sqrt(static_cast<double>(
                        std::max<std::size_t>(1u, reader.groupSize))))));
            }
            case WavesetProcess::repeat:
                return raw;
        }
        return raw;
    }

    double wavesetPhaseIncrement(const WavesetReader& reader) const
    {
        const std::size_t index = orderedWavesetIndex(
            reader, reader.cycleOffset);
        double length = std::max(1.0,
            reader.units[index].end - reader.units[index].start);
        const double amount = static_cast<double>(clamp01(
            reader.processAmount));
        if (reader.process == WavesetProcess::interpolate) {
            const double morph = (static_cast<double>(reader.repeatIndex)
                    + reader.oscillatorPhase)
                / static_cast<double>(std::max<std::size_t>(
                    1u, reader.repeats));
            const WavesetUnit& target = adjacentWavesetUnit(reader, index);
            const double targetLength = std::max(
                1.0, target.end - target.start);
            length += (targetLength - length)
                * clamp(morph * amount, 0.0, 1.0);
        } else if (reader.process == WavesetProcess::groupReverse) {
            const std::size_t reverse = reader.groupSize - 1u
                - (reader.cycleOffset % reader.groupSize);
            const WavesetUnit& target
                = reader.units[orderedWavesetIndex(reader, reverse)];
            const double targetLength = std::max(
                1.0, target.end - target.start);
            length += (targetLength - length) * amount;
        } else if (reader.process == WavesetProcess::telescope) {
            double longest = length;
            for (std::size_t offset = 0u;
                 offset < reader.groupSize; ++offset) {
                const WavesetUnit& candidate
                    = reader.units[orderedWavesetIndex(reader, offset)];
                longest = std::max(longest,
                    candidate.end - candidate.start);
            }
            length += (longest - length) * amount;
        }
        return static_cast<double>(sourceSampleRate_)
            / static_cast<double>(outputSampleRate_)
            * static_cast<double>(speed_) / length;
    }

    void renderWavesetStereo(const WavesetReader& reader,
                             float* stereo) const
    {
        if (reader.fallback) {
            for (std::size_t channel = 0u;
                 channel < kLaneChannels; ++channel) {
                stereo[channel] = wavesetUnitSample(reader, channel,
                    reader.units[0u], reader.oscillatorPhase, 1.0f);
            }
            return;
        }
        for (std::size_t channel = 0u;
             channel < kLaneChannels; ++channel) {
            stereo[channel] = renderWavesetSample(
                reader, channel, reader.join, reader.cycleOffset,
                reader.repeatIndex, reader.oscillatorPhase);
        }
    }

    static void advanceWavesetTransitionReader(WavesetReader& reader,
                                                double increment)
    {
        reader.oscillatorPhase += increment;
        while (reader.oscillatorPhase >= 1.0) {
            reader.oscillatorPhase -= 1.0;
            std::size_t cycleAdvance = 1u;
            if (reader.process == WavesetProcess::telescope) {
                cycleAdvance += static_cast<std::size_t>(std::lround(
                    clamp01(reader.processAmount)
                        * static_cast<float>(reader.groupSize - 1u)));
            }
            reader.cycleOffset += cycleAdvance;
            if (reader.cycleOffset < reader.groupSize) continue;
            reader.cycleOffset = 0u;
            if (++reader.repeatIndex < reader.repeats) continue;
            reader.repeatIndex = 0u;
        }
    }

    void applyWavesetControlTransition(float* stereo)
    {
        if (wavesetControlTransitionRemaining_ == 0u
            || !wavesetControlTransitionFrom_.initialized) return;
        float prior[kLaneChannels] {};
        renderWavesetStereo(wavesetControlTransitionFrom_, prior);
        const float progress = wavesetControlTransitionLength_ > 1u
            ? 1.0f
                - static_cast<float>(wavesetControlTransitionRemaining_ - 1u)
                    / static_cast<float>(wavesetControlTransitionLength_ - 1u)
            : 1.0f;
        const float currentGain = smoothStep(progress);
        for (std::size_t channel = 0u;
             channel < kLaneChannels; ++channel) {
            stereo[channel] = prior[channel] * (1.0f - currentGain)
                + stereo[channel] * currentGain;
        }
        advanceWavesetTransitionReader(wavesetControlTransitionFrom_,
            wavesetPhaseIncrement(wavesetControlTransitionFrom_));
        if (--wavesetControlTransitionRemaining_ == 0u) {
            wavesetControlTransitionFrom_ = {};
        }
    }

    void advanceWavesetGroup(const Controls& controls, const Window& window)
    {
        double cursor = waveset_.groupCursor;
        if (controls.wavesetAdvance != WavesetAdvance::hold) {
            double span = waveset_.units[waveset_.groupSize - 1u].end
                - waveset_.units[0u].start;
            if (!(span > 0.0)) span = 1.0;
            const double stride = static_cast<double>(clamp(
                std::round(controls.wavesetStride), 1.0f, 16.0f));
            const double repeatScale = controls.wavesetAdvance
                    == WavesetAdvance::preserve
                ? static_cast<double>(waveset_.repeats)
                : 1.0;
            cursor = waveset_.units[0u].start
                + span * stride * repeatScale;
        }
        const bool reusableHold = controls.wavesetAdvance
                == WavesetAdvance::hold
            && waveset_.requestedGroupSize == wavesetGroupSize(controls)
            && waveset_.detail == controls.wavesetDetail;
        if (reusableHold || waveset_.analysisRetryFrames != 0u) {
            // Hold repeats a measured group in place. During a bounded retry,
            // a temporary fallback still follows source progression so a
            // quiet lead-in cannot pin both playback and its cursor.
            if (waveset_.direction == WavesetDirection::pendulum) {
                waveset_.pendulumForward = !waveset_.pendulumForward;
            }
            waveset_.randomState
                = waveset_.randomState * 1664525u + 1013904223u;
            const LaneBlend lanes = stackMode_
                ? makeLaneBlend(lanePhase_, controls) : LaneBlend {};
            waveset_.lanes = lanes;
            waveset_.referenceChannel = wavesetReferenceChannel(
                waveset_.groupCursor, window, waveset_.lanes);
            if (waveset_.fallback && !reusableHold) {
                const std::size_t retry = waveset_.analysisRetryFrames;
                waveset_.groupCursor = wrapWavesetCursor(cursor, window);
                configureWavesetFallback(waveset_, window);
                waveset_.analysisRetryFrames = retry;
            }
            waveset_.strongestUnit = 0u;
            for (std::size_t index = 0u;
                 index < waveset_.groupSize; ++index) {
                waveset_.units[index].peak = wavesetPeak(
                    waveset_.units[index], waveset_.lanes,
                    waveset_.referenceChannel);
                if (waveset_.units[index].peak
                    > waveset_.units[waveset_.strongestUnit].peak) {
                    waveset_.strongestUnit = index;
                }
            }
            if (reusableHold) {
                waveset_.repeats = static_cast<std::size_t>(clamp(
                    std::round(controls.wavesetRepeats), 1.0f, 16.0f));
            }
            waveset_.cycleOffset = 0u;
            waveset_.repeatIndex = 0u;
            return;
        }
        buildWavesetGroup(controls, window,
            wrapWavesetCursor(cursor, window));
    }

    void latchWavesetCycleControls(const Controls& controls)
    {
        const std::size_t processMultiple = 2u
            + static_cast<std::size_t>(std::lround(
                clamp01(waveset_.processAmount) * 6.0f));
        const bool topologyChanged
            = waveset_.direction != controls.wavesetDirection
            || waveset_.process != controls.wavesetProcess
            || ((controls.wavesetProcess == WavesetProcess::multiply
                    || controls.wavesetProcess == WavesetProcess::harmonic)
                && waveset_.processMultiple != processMultiple);
        if (topologyChanged && wavesetControlTransitionRemaining_ != 0u) {
            return;
        }
        if (topologyChanged) {
            wavesetControlTransitionFrom_ = waveset_;
            wavesetControlTransitionLength_ = wavesetControlTransitionFrames_;
            wavesetControlTransitionRemaining_
                = wavesetControlTransitionLength_;
        }
        waveset_.direction = controls.wavesetDirection;
        waveset_.process = controls.wavesetProcess;
        waveset_.processMultiple = processMultiple;
    }

    void renderWavesetsFrame(const Controls& controls, float* wet)
    {
        const Window window = smoothEventWindow(controls);
        fieldLow_ = window.low;
        fieldHigh_ = window.high;
        if (waveset_.initialized
            && (waveset_.detail != controls.wavesetDetail
                || waveset_.requestedGroupSize
                    != wavesetGroupSize(controls))) {
            waveset_.analysisRetryFrames = 0u;
        }
        if (waveset_.analysisRetryFrames != 0u) {
            --waveset_.analysisRetryFrames;
        }
        if (!waveset_.initialized) {
            const double launch = window.low
                + clamp(phase_, 0.0, 1.0) * (window.high - window.low);
            buildWavesetGroup(controls, window, launch);
        }
        if (!waveset_.controlsInitialized) {
            waveset_.processAmount = clamp01(controls.wavesetProcessAmount);
            waveset_.join = clamp01(controls.join);
            waveset_.controlsInitialized = true;
            latchWavesetCycleControls(controls);
        } else {
            waveset_.processAmount += wavesetControlSlew_
                * (clamp01(controls.wavesetProcessAmount)
                    - waveset_.processAmount);
            waveset_.join += wavesetControlSlew_
                * (clamp01(controls.join) - waveset_.join);
        }
        const std::size_t index = orderedWavesetIndex(
            waveset_, waveset_.cycleOffset);
        const WavesetUnit& unit = waveset_.units[index];
        displayPosition_ = unit.start + waveset_.oscillatorPhase
            * std::max(1.0, unit.end - unit.start);
        float stereo[kLaneChannels] {};
        renderWavesetStereo(waveset_, stereo);
        applyWavesetControlTransition(stereo);
        for (std::size_t channel = 0u; channel < kChannels; ++channel) {
            wet[channel] = stereo[channel & 1u];
        }

        waveset_.oscillatorPhase += wavesetPhaseIncrement(waveset_);
        while (waveset_.oscillatorPhase >= 1.0) {
            waveset_.oscillatorPhase -= 1.0;
            latchWavesetCycleControls(controls);
            std::size_t cycleAdvance = 1u;
            if (waveset_.process == WavesetProcess::telescope) {
                cycleAdvance += static_cast<std::size_t>(std::lround(
                    clamp01(waveset_.processAmount)
                        * static_cast<float>(waveset_.groupSize - 1u)));
            }
            waveset_.cycleOffset += cycleAdvance;
            if (waveset_.cycleOffset < waveset_.groupSize) continue;
            waveset_.cycleOffset = 0u;
            if (++waveset_.repeatIndex < waveset_.repeats) continue;
            waveset_.repeatIndex = 0u;
            advanceWavesetGroup(controls, window);
        }
    }

    static std::size_t wrapGlitchIndex(std::size_t index)
    {
        return index & (kGlitchBufferFrames - 1u);
    }

    double glitchIntervalFrames(const Controls& controls) const
    {
        if (controls.glitchClock == CutClock::free) {
            return static_cast<double>(outputSampleRate_)
                / static_cast<double>(clamp(
                    controls.glitchRateHz, 0.25f, 64.0f));
        }
        constexpr float beats[kCutDivisionCount] {
            1.0f, 0.5f, 1.0f / 3.0f, 0.25f,
            1.0f / 6.0f, 0.125f,
        };
        const std::size_t division = std::min<std::size_t>(
            static_cast<std::size_t>(controls.glitchDivision),
            kCutDivisionCount - 1u);
        return static_cast<double>(outputSampleRate_) * 60.0
            / static_cast<double>(clamp(
                controls.cutTempoBpm, 20.0f, 300.0f))
            * static_cast<double>(beats[division]);
    }

    bool glitchPatternHit(const Controls& controls, std::size_t step) const
    {
        const std::size_t steps = static_cast<std::size_t>(clamp(
            std::round(controls.glitchSteps), 1.0f, 16.0f));
        const std::size_t pulses = static_cast<std::size_t>(clamp(
            std::round(controls.glitchPulses), 0.0f,
            static_cast<float>(steps)));
        if (pulses == 0u) return false;
        const std::size_t rotate = static_cast<std::size_t>(clamp(
            std::round(controls.glitchRotate), 0.0f, 15.0f)) % steps;
        const std::size_t rotated = (step + steps - rotate) % steps;
        return (rotated * pulses) % steps < pulses;
    }

    void retireGlitch()
    {
        if (!glitchActive_ || glitchSliceFrames_ == 0u) return;
        glitchTailReadStart_ = glitchReadStart_;
        glitchTailSliceFrames_ = glitchSliceFrames_;
        glitchTailRepeatIndex_ = glitchRepeatIndex_;
        glitchTailReadPhase_ = glitchReadPhase_;
        glitchTailReadIncrement_ = glitchReadIncrement_;
        glitchTailMix_ = glitchMix_;
        glitchTailDecay_ = glitchDecay_;
        glitchTailAge_ = 0u;
        glitchTailActive_ = true;
    }

    void beginGlitch(const Controls& controls, std::size_t intervalFrames)
    {
        if (glitchHistoryFrames_ < 16u) return;
        if (controls.glitchBufferLocked
            && glitchHistoryFrames_ < kGlitchBufferFrames) {
            return;
        }
        retireGlitch();
        const std::size_t requested = static_cast<std::size_t>(
            outputSampleRate_ * clamp(
                controls.glitchSliceMs, 2.0f, 160.0f) * 0.001f);
        glitchSliceFrames_ = std::max<std::size_t>(8u,
            std::min({ requested, glitchHistoryFrames_,
                       kGlitchBufferFrames - 1u }));
        const std::uint32_t seed = static_cast<std::uint32_t>(clamp(
            std::round(controls.glitchSeed), 1.0f, 9999.0f));
        const std::uint32_t salt = static_cast<std::uint32_t>(
            std::max(0.0f, controls.cutVoiceIndex));
        const std::uint32_t hash = seed * 0x9e3779b9u
            + static_cast<std::uint32_t>(glitchStep_) * 0x85ebca6bu
            + glitchCycle_ * 0xc2b2ae35u + salt * 0x27d4eb2du;
        const float randomA = hashUnit(hash);
        const float randomB = hashUnit(hash ^ 0x68bc21ebu);
        const float randomC = hashUnit(hash ^ 0x02e5be93u);
        const std::size_t maximumJump = std::min<std::size_t>(
            glitchHistoryFrames_ - glitchSliceFrames_,
            static_cast<std::size_t>(outputSampleRate_
                * clamp(controls.glitchJumpMs, 0.0f, 160.0f) * 0.001f));
        std::size_t jump = 0u;
        if (controls.glitchMode == GlitchMode::scatter
            || controls.glitchMode == GlitchMode::override) {
            jump = static_cast<std::size_t>(randomA
                * static_cast<float>(maximumJump));
        }
        const std::size_t newest = wrapGlitchIndex(
            glitchWriteIndex_ + kGlitchBufferFrames
                - glitchSliceFrames_ - jump);
        glitchReadStart_ = newest;
        glitchReadPhase_ = 0.0;
        glitchRepeatIndex_ = 0u;
        glitchRepeatLimit_ = static_cast<std::size_t>(clamp(
            std::round(controls.glitchRepeats), 1.0f, 16.0f));
        if (controls.glitchMode == GlitchMode::stutter) {
            const std::size_t durationRepeats = std::max<std::size_t>(1u,
                intervalFrames / std::max<std::size_t>(1u, glitchSliceFrames_));
            glitchRepeatLimit_ = std::max(glitchRepeatLimit_, durationRepeats + 1u);
        }
        float semitones = (randomB * 2.0f - 1.0f)
            * clamp(controls.glitchPitch, 0.0f, 24.0f);
        if (controls.glitchMode != GlitchMode::override) {
            semitones = 0.0f;
        }
        glitchReadIncrement_ = std::pow(2.0,
            static_cast<double>(semitones) / 12.0);
        if (randomC < clamp01(controls.glitchReverseChance)) {
            glitchReadIncrement_ = -glitchReadIncrement_;
            glitchReadPhase_ = static_cast<double>(glitchSliceFrames_ - 1u);
        }
        glitchActive_ = true;
    }

    void renderGlitchReader(std::size_t readStart,
                            std::size_t sliceFrames,
                            double readPhase,
                            double readIncrement,
                            std::size_t repeatIndex,
                            float mix,
                            float decayControl,
                            const float* dry,
                            float* stereo) const
    {
        stereo[0u] = dry[0u];
        stereo[1u] = dry[1u];
        if (sliceFrames == 0u) return;
        const double wrappedPhase = readPhase < 0.0
            ? readPhase + static_cast<double>(sliceFrames) : readPhase;
        const std::size_t offset = std::min<std::size_t>(
            static_cast<std::size_t>(wrappedPhase), sliceFrames - 1u);
        const std::size_t read = wrapGlitchIndex(readStart + offset);
        const float decay = std::pow(clamp01(decayControl),
            static_cast<float>(repeatIndex));
        const std::size_t fadeFrames = std::min<std::size_t>(
            std::max<std::size_t>(1u, sliceFrames / 8u),
            static_cast<std::size_t>(outputSampleRate_ * 0.004f));
        const std::size_t age = readIncrement >= 0.0
            ? offset : sliceFrames - 1u - offset;
        const float edge = std::min(1.0f,
            static_cast<float>(std::min(age, sliceFrames - 1u - age))
                / static_cast<float>(fadeFrames));
        const float amount = clamp01(mix) * smoothStep(edge);
        for (std::size_t channel = 0u; channel < kLaneChannels; ++channel) {
            const float repeated = glitchBuffer_[channel][read] * decay;
            stereo[channel] = dry[channel]
                + (repeated - dry[channel]) * amount;
        }
    }

    void renderGlitchFrame(const Controls& controls, float join, float* wet)
    {
        Controls source = controls;
        source.motion = MotionMode::forward;
        source.motionSound = MotionSound::continuous;
        float dry[kChannels] {};
        renderContinuousFrame(source, join, false, false, dry);
        if (!glitchControlInitialized_) {
            glitchMix_ = clamp01(controls.glitchMix);
            glitchDecay_ = clamp01(controls.glitchDecay);
            glitchControlInitialized_ = true;
        } else {
            glitchMix_ += controlGainSlew_
                * (clamp01(controls.glitchMix) - glitchMix_);
            glitchDecay_ += controlGainSlew_
                * (clamp01(controls.glitchDecay) - glitchDecay_);
        }

        // A newly started voice always fills one complete short history before
        // honoring a global lock. This keeps notes triggered while locked from
        // inheriting an empty or unusably tiny repeat buffer.
        const bool fillingLockedHistory = controls.glitchBufferLocked
            && glitchHistoryFrames_ < kGlitchBufferFrames;
        if (!glitchActive_
            && (!controls.glitchBufferLocked || fillingLockedHistory)) {
            glitchBuffer_[0u][glitchWriteIndex_] = dry[0u];
            glitchBuffer_[1u][glitchWriteIndex_] = dry[1u];
            glitchWriteIndex_ = wrapGlitchIndex(glitchWriteIndex_ + 1u);
            glitchHistoryFrames_ = std::min(
                kGlitchBufferFrames, glitchHistoryFrames_ + 1u);
        }

        if (glitchCountdown_ <= 0.0) {
            const std::size_t intervalFrames = std::max<std::size_t>(1u,
                static_cast<std::size_t>(glitchIntervalFrames(controls)));
            const std::size_t steps = static_cast<std::size_t>(clamp(
                std::round(controls.glitchSteps), 1.0f, 16.0f));
            const bool hit = glitchPatternHit(controls, glitchStep_);
            const std::uint32_t seed = static_cast<std::uint32_t>(clamp(
                std::round(controls.glitchSeed), 1.0f, 9999.0f));
            const float chance = hashUnit(seed + glitchCycle_ * 257u
                + static_cast<std::uint32_t>(glitchStep_) * 17u);
            if (hit && chance <= clamp01(controls.glitchChance)) {
                beginGlitch(controls, intervalFrames);
            } else if (controls.glitchMode == GlitchMode::stutter) {
                retireGlitch();
                glitchActive_ = false;
            }
            glitchStep_ = (glitchStep_ + 1u) % steps;
            if (glitchStep_ == 0u) ++glitchCycle_;
            glitchCountdown_ += static_cast<double>(intervalFrames);
        }
        glitchCountdown_ -= 1.0;

        float stereo[kLaneChannels] { dry[0u], dry[1u] };
        if (glitchActive_ && glitchSliceFrames_ != 0u) {
            renderGlitchReader(glitchReadStart_, glitchSliceFrames_,
                glitchReadPhase_, glitchReadIncrement_, glitchRepeatIndex_,
                glitchMix_, glitchDecay_, dry, stereo);
            glitchReadPhase_ += glitchReadIncrement_;
            bool wrapped = false;
            if (glitchReadPhase_ >= static_cast<double>(glitchSliceFrames_)) {
                glitchReadPhase_ -= static_cast<double>(glitchSliceFrames_);
                wrapped = true;
            } else if (glitchReadPhase_ < 0.0) {
                glitchReadPhase_ += static_cast<double>(glitchSliceFrames_);
                wrapped = true;
            }
            if (wrapped) {
                ++glitchRepeatIndex_;
                if (controls.glitchMode == GlitchMode::divide
                    && glitchSliceFrames_ > 16u) {
                    glitchSliceFrames_ = std::max<std::size_t>(
                        16u, glitchSliceFrames_ / 2u);
                    glitchReadPhase_ = glitchReadIncrement_ < 0.0
                        ? static_cast<double>(glitchSliceFrames_ - 1u) : 0.0;
                }
                if (glitchRepeatIndex_ >= glitchRepeatLimit_) {
                    glitchActive_ = false;
                }
            }
        }
        if (glitchTailActive_ && glitchTailSliceFrames_ != 0u) {
            float tail[kLaneChannels] {};
            renderGlitchReader(glitchTailReadStart_, glitchTailSliceFrames_,
                glitchTailReadPhase_, glitchTailReadIncrement_,
                glitchTailRepeatIndex_, glitchTailMix_, glitchTailDecay_,
                dry, tail);
            const float transition = smoothStep(static_cast<float>(
                glitchTailAge_) / static_cast<float>(
                    std::max<std::size_t>(1u, safetyFadeFrames_)));
            stereo[0u] = tail[0u] + (stereo[0u] - tail[0u]) * transition;
            stereo[1u] = tail[1u] + (stereo[1u] - tail[1u]) * transition;
            glitchTailReadPhase_ += glitchTailReadIncrement_;
            while (glitchTailReadPhase_
                    >= static_cast<double>(glitchTailSliceFrames_)) {
                glitchTailReadPhase_
                    -= static_cast<double>(glitchTailSliceFrames_);
                ++glitchTailRepeatIndex_;
            }
            while (glitchTailReadPhase_ < 0.0) {
                glitchTailReadPhase_
                    += static_cast<double>(glitchTailSliceFrames_);
                ++glitchTailRepeatIndex_;
            }
            if (++glitchTailAge_ >= safetyFadeFrames_) {
                glitchTailActive_ = false;
            }
        }
        for (std::size_t channel = 0u; channel < kChannels; ++channel) {
            wet[channel] = stereo[channel & 1u];
        }
    }

    void advanceLaneClock()
    {
        if (!stackMode_) {
            return;
        }
        lanePhase_ += lanePhaseIncrement_;
        if (lanePhase_ >= 1.0f) {
            lanePhase_ -= 1.0f;
        }
    }

    std::size_t mapSourceChannel(std::size_t outputChannel) const
    {
        if (sourceChannels_ <= 1u) {
            return 0u;
        }
        if (sourceChannels_ == 2u) {
            return outputChannel & 1u;
        }
        return outputChannel < sourceChannels_
            ? outputChannel : sourceChannels_ - 1u;
    }

    float* storage_[kChannels] { nullptr, nullptr, nullptr, nullptr };
    std::size_t capacityFrames_ = 0u;
    std::size_t laneCapacityFrames_ = 0u;
    float outputSampleRate_ = 48000.0f;
    std::size_t sourceFrames_ = 0u;
    std::size_t sourceChannels_ = 0u;
    float sourceSampleRate_ = 48000.0f;
    std::size_t recordFrames_ = 0u;
    std::size_t recordChannels_ = 0u;
    std::size_t recordingLane_ = kNoLane;
    std::size_t lastRecordedLane_ = kNoLane;
    float recordPeak_ = 0.0f;
    float recordMeterPeak_ = 0.0f;
    float lastRecordedPeak_ = 0.0f;
    float recordMeterRelease_ = 0.9999f;
    float recordLimiterGain_ = 1.0f;
    float recordLimiterRelease_ = 0.0001f;
    float recordInputGain_ = 1.0f;
    bool recordLimited_ = false;
    bool lastRecordLimited_ = false;
    std::size_t recordInputOffset_ = 0u;
    std::size_t laneFrames_[kStackLanes] {};
    float laneSampleRates_[kStackLanes] {
        48000.0f, 48000.0f, 48000.0f, 48000.0f,
    };
    double lanePositionScales_[kStackLanes] {};
    float laneRegionBoundaries_[kStackLanes][kRegionBoundaryCount] {};
    std::size_t laneRegionCounts_[kStackLanes] {};
    std::size_t referenceLane_ = 0u;
    float lanePosition_ = 0.0f;
    float laneWeights_[kStackLanes] {};
    float laneWeightTargets_[kStackLanes] {};
    float activeControlsLaneGains_[kStackLanes] {
        1.0f, 1.0f, 1.0f, 1.0f,
    };
    float targetControlsLaneGains_[kStackLanes] {
        1.0f, 1.0f, 1.0f, 1.0f,
    };
    double phase_ = 0.0;
    float lanePhase_ = 0.0f;
    float lanePhaseIncrement_ = 0.0f;
    float laneFrozenPhase_ = 0.0f;
    bool laneScanPaused_ = false;
    double stretchPhase_ = 0.0;
    float stretchMix_ = 0.0f;
    Grain grains_[kGrainVoiceCount] {};
    std::size_t grainVoiceLimit_ = 8u;
    double grainCountdown_ = 0.0;
    double grainIntervalFrames_ = 2000.0;
    std::size_t grainDurationFrames_ = 4320u;
    float grainDensityHz_ = 24.0f;
    float grainPitch_ = 1.0f;
    float grainGain_ = 1.0f;
    std::size_t grainSerial_ = 0u;
    std::size_t grainProcessIndex_ = 0u;
    double grainSourcePhase_ = 0.0;
    double grainStutterAnchor_ = -1.0;
    bool grainDoubletPending_ = false;
    double grainDoubletPosition_ = 0.0;
    LongGrain longGrains_[kLongGrainVoiceCount] {};
    double longGrainCountdown_ = 0.0;
    double longGrainIntervalFrames_ = 48000.0;
    std::size_t longGrainDurationFrames_ = 96000u;
    std::size_t longGrainLayerLimit_ = 2u;
    float longGrainPitch_ = 1.0f;
    std::size_t longGrainSerial_ = 0u;
    double longGrainSourcePhase_ = 0.0;
    std::uint8_t sorterRanking_[kRegionCount] {};
    float sorterEnergies_[kRegionCount] {};
    std::size_t sorterRegionCount_ = 0u;
    std::size_t sorterLane_ = kNoLane;
    double sorterWindowLow_ = 0.0;
    double sorterWindowHigh_ = 0.0;
    bool sorterCacheValid_ = false;
    static constexpr std::size_t kGrainWindowTableSize = 257u;
    float grainWindowTables_[kGrainWindowCount][kGrainWindowTableSize] {};
    CutReader cutCurrent_ {};
    CutReader cutTail_ {};
    double cutCountdown_ = 0.0;
    std::size_t cutStep_ = 0u;
    std::size_t cutWalkRegion_ = 0u;
    std::size_t cutRepeatIndex_ = 0u;
    std::size_t cutRepeatedRegion_ = 0u;
    bool cutSwingPhase_ = false;
    static constexpr std::size_t kGlitchBufferFrames = 8192u;
    static_assert((kGlitchBufferFrames & (kGlitchBufferFrames - 1u)) == 0u,
                  "glitch buffer wrapping requires a power of two");
    float glitchBuffer_[kLaneChannels][kGlitchBufferFrames] {};
    std::size_t glitchWriteIndex_ = 0u;
    std::size_t glitchHistoryFrames_ = 0u;
    std::size_t glitchReadStart_ = 0u;
    std::size_t glitchSliceFrames_ = 0u;
    std::size_t glitchRepeatIndex_ = 0u;
    std::size_t glitchRepeatLimit_ = 0u;
    std::size_t glitchStep_ = 0u;
    std::uint32_t glitchCycle_ = 0u;
    double glitchCountdown_ = 0.0;
    double glitchReadPhase_ = 0.0;
    double glitchReadIncrement_ = 1.0;
    bool glitchActive_ = false;
    std::size_t glitchTailReadStart_ = 0u;
    std::size_t glitchTailSliceFrames_ = 0u;
    std::size_t glitchTailRepeatIndex_ = 0u;
    std::size_t glitchTailAge_ = 0u;
    double glitchTailReadPhase_ = 0.0;
    double glitchTailReadIncrement_ = 1.0;
    float glitchTailMix_ = 1.0f;
    float glitchTailDecay_ = 0.9f;
    float glitchMix_ = 1.0f;
    float glitchDecay_ = 0.9f;
    bool glitchTailActive_ = false;
    bool glitchControlInitialized_ = false;
    WavesetReader waveset_ {};
    WavesetReader wavesetControlTransitionFrom_ {};
    WavesetDiscreteControls wavesetActiveControls_ {};
    std::uint32_t randomState_ = 0x51f15e5du;
    float speed_ = 1.0f;
    float speedSlew_ = 1.0f;
    float join_ = 1.0f;
    float joinSlew_ = 1.0f;
    bool joinInitialized_ = false;
    float stretchRatio_ = 1.0f;
    float stretchRatioTarget_ = 1.0f;
    float stretchRatioSlew_ = 1.0f;
    bool stretchRatioInitialized_ = false;
    float laneSlew_ = 1.0f;
    float lastLaneSlewMs_ = -1.0f;
    float eventWindowSlew_ = 1.0f;
    float motionEnvelopeSlew_ = 1.0f;
    float controlGainSlew_ = 1.0f;
    float motionControlSlew_ = 1.0f;
    float wavesetControlSlew_ = 1.0f;
    std::size_t wavesetControlTransitionFrames_ = 144u;
    std::size_t wavesetControlTransitionLength_ = 144u;
    std::size_t wavesetControlTransitionRemaining_ = 0u;
    float wavesetAnalysisCoefficients_[kWavesetDetailCount] {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };
    std::size_t wavesetAnalysisWarmupFrames_[kWavesetDetailCount] {};
    float motionInnerPhase_ = 0.0f;
    float motionOuterPhase_ = 0.5f;
    float motionSoundPhaseIncrement_ = 0.0f;
    float motionSoundPhaseIncrementTarget_ = 0.0f;
    float motionMotorPhaseIncrement_ = 0.0f;
    float motionMotorPhaseIncrementTarget_ = 0.0f;
    float motionPacketDuty_ = 0.65f;
    float motionPacketDutyTarget_ = 0.65f;
    float motionMotorSymmetry_ = 0.5f;
    float motionMotorSymmetryTarget_ = 0.5f;
    float motionTravel_ = 1.0f;
    float motionTravelTarget_ = 1.0f;
    float motionJitter_ = 0.0f;
    float motionJitterTarget_ = 0.0f;
    MotorShape motionMotorShape_ = MotorShape::rounded;
    float motionEnvelope_ = 1.0f;
    double lastMotionPhaseIncrement_ = 0.0;
    float motionInnerRateScale_ = 1.0f;
    MotionSegment motionSegments_[kMotionSegmentSlotCount] {};
    std::uint32_t motionSegmentAge_ = 0u;
    double motionEventPhase_ = 0.0;
    std::size_t motionEventRepeatIndex_ = 0u;
    double motionEventBasePosition_ = 0.5;
    float motionEventRateScale_ = 1.0f;
    std::uint32_t motionSeed_ = 1u;
    std::uint32_t pendingMotionSeed_ = 1u;
    bool motionSeedPending_ = false;
    std::size_t motionSeedTransitionRemaining_ = 0u;
    bool motionControlsInitialized_ = false;
    std::uint32_t motionRandomCycle_ = 0u;
    bool motionEventStarted_ = false;
    bool motionPacketEvent_ = false;
    bool motionTurnEvent_ = false;
    bool motionPathForward_ = true;
    double displayPosition_ = 0.0;
    double fieldLow_ = 0.0;
    double fieldHigh_ = 0.0;
    Window currentWindow_ {};
    Window fromWindow_ {};
    Window toWindow_ {};
    Window eventWindow_ {};
    MotionMode currentMode_ = MotionMode::forward;
    MotionMode fromMode_ = MotionMode::forward;
    MotionMode toMode_ = MotionMode::forward;
    MotionMode requestedMode_ = MotionMode::forward;
    MotionSound currentMotionSound_ = MotionSound::continuous;
    MotionSound pendingMotionSound_ = MotionSound::continuous;
    bool motionSoundChangePending_ = false;
    SampleMode currentSampleMode_ = SampleMode::forward;
    SampleReaderConfig currentSampleConfig_ {};
    SampleReaderConfig pendingSampleConfig_ {};
    bool sampleConfigInitialized_ = false;
    bool sampleConfigPending_ = false;
    PlaybackAlgorithm currentAlgorithm_ = PlaybackAlgorithm::motion;
    PlaybackAlgorithm pendingAlgorithm_ = PlaybackAlgorithm::motion;
    std::size_t windowFadeFrames_ = 1440u;
    std::size_t windowFadeRemaining_ = 0u;
    std::size_t retriggerFadeFrames_ = 144u;
    std::size_t retriggerFadeRemaining_ = 0u;
    std::size_t retriggerTransitionFrames_ = 240u;
    std::size_t retriggerTransitionRemaining_ = 0u;
    std::size_t safetyFadeFrames_ = 144u;
    std::size_t readerTransitionFrames_ = 240u;
    std::size_t motionSoundTransitionRemaining_ = 0u;
    std::size_t sampleTransitionRemaining_ = 0u;
    double pendingSeekPhase_ = 0.0;
    bool seekPending_ = false;
    std::size_t algorithmFadeFrames_ = 480u;
    std::size_t algorithmFadeRemaining_ = 0u;
    std::size_t recordTransitionFrames_ = 240u;
    std::size_t recordTransitionRemaining_ = 0u;
    LaneTransition laneTransition_ = LaneTransition::none;
    std::size_t transitionLane_ = kNoLane;
    std::size_t transitionFrames_ = 0u;
    float transitionSampleRate_ = 48000.0f;
    double movingCenterTarget_ = 0.0;
    float lastControlLocus_ = -1.0f;
    bool prepared_ = false;
    bool playing_ = false;
    bool recording_ = false;
    bool recordWriteActive_ = false;
    bool windowInitialized_ = false;
    bool eventWindowInitialized_ = false;
    bool movingCenterInitialized_ = false;
    bool stackMode_ = false;
    bool laneWeightsInitialized_ = false;
    bool sampleDone_ = false;
};

} // namespace s3g_daisy
