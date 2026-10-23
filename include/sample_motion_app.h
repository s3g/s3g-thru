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
 * stretched Lanes, Grains, Weave long-grain, and Cutups renderers. No renderer
 * allocates or performs file I/O in the audio callback.
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
    };

    static constexpr std::size_t kPlaybackAlgorithmCount = 8u;

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
        algorithmFadeFrames_ = std::max<std::size_t>(
            32u, static_cast<std::size_t>(outputSampleRate_ * 0.010f));
        recordTransitionFrames_ = std::max<std::size_t>(
            32u, static_cast<std::size_t>(outputSampleRate_ * 0.005f));
        speedSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.020f));
        laneSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.020f));
        eventWindowSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.040f));
        motionEnvelopeSlew_ = 1.0f
            - std::exp(-1.0f / std::max(1.0f, outputSampleRate_ * 0.002f));
        recordMeterRelease_ = std::exp(
            -1.0f / std::max(1.0f, outputSampleRate_ * 0.250f));
        recordLimiterRelease_ = 1.0f - std::exp(
            -1.0f / std::max(1.0f, outputSampleRate_ * 0.080f));
        prepareGrainWindowTables();
        clear();
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
            laneRegionCounts_[lane] = 0u;
            laneRegionBoundaries_[lane][0u] = 0.0f;
            laneRegionBoundaries_[lane][1u] = 1.0f;
        }
        phase_ = 0.0;
        speed_ = 1.0f;
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
        stackMode_ = false;
        lastRecordedLane_ = kNoLane;
        recordChannels_ = 0u;
        recordWriteActive_ = false;
        recordTransitionRemaining_ = 0u;
        resetReader();
        playing_ = sourceFrames_ != 0u;
        return playing_;
    }

    void retrigger()
    {
        if (sourceFrames_ == 0u) {
            return;
        }
        resetReader();
        retriggerFadeRemaining_ = retriggerFadeFrames_;
        playing_ = true;
    }

    void seekNormalized(float position)
    {
        if (sourceFrames_ == 0u) return;
        phase_ = currentAlgorithm_ == PlaybackAlgorithm::sample
                && currentSampleMode_ == SampleMode::reverse
            ? 1.0 - static_cast<double>(clamp01(position))
            : static_cast<double>(clamp01(position));
        resetEventReaders();
        retriggerFadeRemaining_ = retriggerFadeFrames_;
        sampleDone_ = false;
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
        const float join = clamp01(controls.join);
        recordInputGain_ = clamp(controls.recordInputGain, 0.0f, 1.0f);
        const float requestedLaneSlewMs = clamp(
            controls.laneSlewMs, 0.0f, 500.0f);
        if (std::abs(requestedLaneSlewMs - lastLaneSlewMs_) > 1.0e-3f) {
            const float laneSlewSeconds = requestedLaneSlewMs * 0.001f;
            laneSlew_ = laneSlewSeconds <= 0.0f ? 1.0f
                : 1.0f - std::exp(-1.0f / std::max(
                    1.0f, outputSampleRate_ * laneSlewSeconds));
            lastLaneSlewMs_ = requestedLaneSlewMs;
        }
        for (std::size_t lane = 0u; lane < kStackLanes; ++lane) {
            activeControlsLaneGains_[lane]
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

        std::size_t laneControlCountdown = 0u;
        for (std::size_t frame = 0u; frame < frames; ++frame) {
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
            const float algorithmGain = algorithmTransitionGain();
            const float retriggerGain = retriggerTransitionGain();
            float wetOutput[kChannels] {};
            switch (currentAlgorithm_) {
                case PlaybackAlgorithm::motion:
                    renderContinuousFrame(
                        controls, join, false, false, wetOutput);
                    break;
                case PlaybackAlgorithm::lanes:
                    renderContinuousFrame(
                        controls, join, true, controls.lanesStretch, wetOutput);
                    break;
                case PlaybackAlgorithm::grains:
                    renderGrainsFrame(controls, wetOutput);
                    break;
                case PlaybackAlgorithm::cutups:
                    renderCutupsFrame(controls, wetOutput);
                    break;
                case PlaybackAlgorithm::sample:
                    renderSampleFrame(controls, false, wetOutput);
                    break;
                case PlaybackAlgorithm::slices:
                    renderSampleFrame(controls, true, wetOutput);
                    break;
                case PlaybackAlgorithm::glitch:
                    renderGlitchFrame(controls, join, wetOutput);
                    break;
                case PlaybackAlgorithm::longGrains:
                    renderLongGrainsFrame(controls, wetOutput);
                    break;
            }
            advanceLaneClock();
            if (laneControlCountdown != 0u) {
                --laneControlCountdown;
            }

            for (std::size_t channel = 0u; channel < kChannels; ++channel) {
                float wet = wetOutput[channel] * algorithmGain * retriggerGain;
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
        std::uint32_t age = 0u;
        double phase = 0.0;
        double low = 0.0;
        double high = 1.0;
        double increment = 0.0;
        LaneBlend lanes {};
        float gain = 1.0f;
        bool forward = true;
    };

    static constexpr std::size_t kGrainVoiceCount = 16u;
    static constexpr std::size_t kLongGrainVoiceCount = 4u;
    static constexpr std::size_t kCutRegionCount = 64u;
    static constexpr std::size_t kMotionSegmentLayerCount = 4u;

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
        randomState_ = 0x51f15e5du;
        currentMotionSound_ = MotionSound::continuous;
        motionInnerPhase_ = 0.0;
        motionOuterPhase_ = 0.5;
        motionSoundPhaseIncrement_ = 0.0f;
        motionMotorPhaseIncrement_ = 0.0f;
        motionPacketDuty_ = 0.65f;
        motionMotorSymmetry_ = 0.5f;
        motionMotorShape_ = MotorShape::rounded;
        motionInnerRateScale_ = 1.0f;
        motionSeed_ = 1u;
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
            resetPlaybackForAlgorithm();
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
        if (join <= 0.0f || width < 8.0) {
            return 0.0;
        }
        return std::max(0.0, std::min(width * 0.08,
            static_cast<double>(sourceSampleRate_) * 0.020))
            * static_cast<double>(join);
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
            const std::uint32_t cycleSeed = motionSeed_
                + motionRandomCycle_ * 0x9e3779b9u;
            const double reach = 0.5 * static_cast<double>(motionTravel_);
            const double highTarget = 0.5 + reach * (1.0
                - static_cast<double>(motionJitter_) * 0.8
                    * hashUnit(cycleSeed + 0x7f4a7c15u));
            const double lowTarget = 0.5 - reach * (1.0
                - static_cast<double>(motionJitter_) * 0.8
                    * hashUnit(cycleSeed + 0x94d049bbu));
            const double firstDistance = std::max(0.001, highTarget);
            const double secondDistance = std::max(0.001,
                highTarget - lowTarget);
            const double thirdDistance = std::max(0.001, 1.0 - lowTarget);
            const double totalDistance
                = firstDistance + secondDistance + thirdDistance;
            const double firstBoundary = firstDistance / totalDistance;
            const double secondBoundary
                = (firstDistance + secondDistance) / totalDistance;
            return phase < firstBoundary || phase >= secondBoundary;
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
            const std::uint32_t cycleSeed = motionSeed_
                + motionRandomCycle_ * 0x9e3779b9u;
            const double reach = 0.5 * static_cast<double>(motionTravel_);
            const double highTarget = 0.5 + reach * (1.0
                - static_cast<double>(motionJitter_) * 0.8
                    * hashUnit(cycleSeed + 0x7f4a7c15u));
            const double lowTarget = 0.5 - reach * (1.0
                - static_cast<double>(motionJitter_) * 0.8
                    * hashUnit(cycleSeed + 0x94d049bbu));
            const double firstDistance = std::max(0.001, highTarget);
            const double secondDistance = std::max(0.001,
                highTarget - lowTarget);
            const double thirdDistance = std::max(0.001, 1.0 - lowTarget);
            const double totalDistance
                = firstDistance + secondDistance + thirdDistance;
            const double firstBoundary = firstDistance / totalDistance;
            const double secondBoundary
                = (firstDistance + secondDistance) / totalDistance;
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
            const double eased = static_cast<double>(smoothStep(
                static_cast<float>(local)));
            result.a = window.low + (from + (to - from) * eased) * width;
            result.b = result.a;
            result.position = result.a;
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
        if (!(seam > 0.0) || position < high - seam) {
            return head;
        }
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

    void prepareMotionSound(const Controls& controls)
    {
        if (currentMotionSound_ == controls.motionSound) {
            return;
        }
        currentMotionSound_ = controls.motionSound;
        motionInnerPhase_ = controls.motionSound == MotionSound::continuous
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

    void prepareMotionSoundBlock(const Controls& controls)
    {
        prepareMotionSound(controls);
        motionSoundPhaseIncrement_ = clamp(
            controls.motionSoundRateHz, 0.25f, 32.0f)
            / outputSampleRate_;
        motionMotorPhaseIncrement_ = clamp(
            controls.motorRateHz, 0.05f, 20.0f) / outputSampleRate_;
        motionPacketDuty_ = clamp(controls.packetDuty, 0.02f, 0.98f);
        motionMotorSymmetry_ = clamp(
            controls.motorSymmetry, 0.05f, 0.95f);
        motionMotorShape_ = controls.motorShape;
        motionTravel_ = clamp01(controls.motionTravel);
        motionJitter_ = clamp01(controls.motionJitter);
        const std::uint32_t requestedSeed = std::max<std::uint32_t>(1u,
            static_cast<std::uint32_t>(std::lround(clamp(
                controls.motionSeed, 1.0f, 9999.0f))));
        if (motionSeed_ != requestedSeed) {
            motionSeed_ = requestedSeed;
            motionRandomCycle_ = 0u;
            randomState_ = 0x51f15e5du ^ (motionSeed_ * 0x9e3779b9u);
            if (randomState_ == 0u) randomState_ = 0x6d2b79f5u;
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
            case MotorShape::pulse: return level > 0.5f ? 1.0f : 0.0f;
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
        for (std::size_t index = limit;
             index < kMotionSegmentLayerCount; ++index) {
            motionSegments_[index] = {};
        }
        if (controls.segmentOverlap == SegmentOverlap::cut) {
            for (auto& segment : motionSegments_) segment = {};
            return &motionSegments_[0u];
        }
        for (std::size_t index = 0u; index < limit; ++index) {
            if (!motionSegments_[index].active) return &motionSegments_[index];
        }
        MotionSegment* oldest = &motionSegments_[0u];
        for (std::size_t index = 1u; index < limit; ++index) {
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
            const float gain = segment.gain * envelope;
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
            if (segment.phase >= 1.0) segment.active = false;
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
                    && std::abs(controls.lanesStretchRatio - 1.0f) > 1.0e-4f
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
                    controls.lanesStretchRatio,
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
                        readPhase, readJoin, controls.lanesStretchRatio,
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
                        controls.lanesStretchRatio,
                        firstStretchGain, secondStretchGain)
                    : readAtPhase(
                        channel, firstReadWindow, firstReadMode,
                        readPhase, readJoin);
                const float newWet = windowFadeRemaining_ == 0u
                    ? oldWet
                    : renderStretch
                        ? readStretchedAtPhase(channel, secondReadWindow,
                            secondReadMode, readPhase, readJoin,
                            controls.lanesStretchRatio,
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
            ? static_cast<double>(clamp(
                controls.lanesStretchRatio, 0.25f, 4.0f))
            : 1.0;
        const double stretchedIncrement = pitchIncrement / requestedStretch;
        const double phaseIncrement = pitchIncrement
            + (stretchedIncrement - pitchIncrement)
                * static_cast<double>(stretchMix_);
        const double directedIncrement = motorReverse
            ? -phaseIncrement : phaseIncrement;
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
            phase_ -= 1.0;
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
            grainWindowTables_[static_cast<std::size_t>(
                GrainWindow::gaussian)][index]
                = std::exp(-0.5f * gaussian * gaussian);
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
                grain.position, grain.low, grain.high, grain.seam);
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
                grain.position, grain.low, grain.high, grain.seam);
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
        const std::size_t join = std::max<std::size_t>(1u,
            std::min<std::size_t>(
                static_cast<std::size_t>(outputSampleRate_
                    * clamp(controls.cutJoinMs, 0.0f, 100.0f) * 0.001f),
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

    void renderSampleFrame(const Controls& controls, bool sliced, float* wet)
    {
        if (currentSampleMode_ != controls.sampleMode) {
            phase_ = 0.0;
            sampleDone_ = false;
        }
        currentSampleMode_ = controls.sampleMode;
        const Window window = smoothEventWindow(controls);
        fieldLow_ = window.low;
        fieldHigh_ = window.high;
        const std::size_t lane = requestedSampleLane(controls);
        const double fieldWidth = std::max(1.0, window.high - window.low);
        double low = window.low;
        double high = window.high;
        if (sliced) {
            const bool transientRegions = lane < kStackLanes
                && controls.sampleRegionMode == RegionMode::transients
                && laneRegionCounts_[lane] != 0u;
            const std::size_t regions = transientRegions
                ? laneRegionCounts_[lane]
                : static_cast<std::size_t>(clamp(
                    controls.sampleRegionCount, 1.0f,
                    static_cast<float>(kRegionCount)));
            const std::size_t region = static_cast<std::size_t>(std::max(
                0.0f, std::floor(controls.sampleSlice))) % regions;
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
        if (controls.sampleMode == SampleMode::pingPong) increment *= 0.5;
        double readPhase = phase_;
        if (controls.sampleMode == SampleMode::reverse) {
            readPhase = 1.0 - phase_;
        } else if (controls.sampleMode == SampleMode::pingPong) {
            readPhase = phase_ < 0.5 ? phase_ * 2.0 : 2.0 - phase_ * 2.0;
        }
        const double position = clamp(low + readPhase * width,
                                      low, std::max(low, high - 1.0));
        ReadHead head;
        head.a = position;
        head.b = position;
        head.position = position;
        displayPosition_ = position;
        for (std::size_t channel = 0u; channel < kChannels; ++channel) {
            if (sampleDone_) {
                wet[channel] = 0.0f;
            } else if (stackMode_ && lane < kStackLanes) {
                wet[channel] = readLaneHead(lane, channel & 1u, head)
                    * activeControlsLaneGains_[lane];
            } else {
                wet[channel] = readHead(mapSourceChannel(channel), head);
            }
        }
        phase_ += increment;
        if (controls.sampleMode == SampleMode::loop
            || controls.sampleMode == SampleMode::pingPong) {
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
            reader.position, reader.low, reader.high, reader.seam);
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

    void beginGlitch(const Controls& controls, std::size_t intervalFrames)
    {
        if (glitchHistoryFrames_ < 16u) return;
        if (controls.glitchBufferLocked
            && glitchHistoryFrames_ < kGlitchBufferFrames) {
            return;
        }
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

    void renderGlitchFrame(const Controls& controls, float join, float* wet)
    {
        Controls source = controls;
        source.motion = MotionMode::forward;
        source.motionSound = MotionSound::continuous;
        float dry[kChannels] {};
        renderContinuousFrame(source, join, false, false, dry);

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
                glitchActive_ = false;
            }
            glitchStep_ = (glitchStep_ + 1u) % steps;
            if (glitchStep_ == 0u) ++glitchCycle_;
            glitchCountdown_ += static_cast<double>(intervalFrames);
        }
        glitchCountdown_ -= 1.0;

        float stereo[kLaneChannels] { dry[0u], dry[1u] };
        if (glitchActive_ && glitchSliceFrames_ != 0u) {
            const double wrappedPhase = glitchReadPhase_ < 0.0
                ? glitchReadPhase_ + static_cast<double>(glitchSliceFrames_)
                : glitchReadPhase_;
            const std::size_t offset = std::min<std::size_t>(
                static_cast<std::size_t>(wrappedPhase),
                glitchSliceFrames_ - 1u);
            const std::size_t read = wrapGlitchIndex(
                glitchReadStart_ + offset);
            const float mix = clamp01(controls.glitchMix);
            const float decay = std::pow(clamp01(controls.glitchDecay),
                static_cast<float>(glitchRepeatIndex_));
            const std::size_t fadeFrames = std::min<std::size_t>(
                std::max<std::size_t>(1u, glitchSliceFrames_ / 8u),
                static_cast<std::size_t>(outputSampleRate_ * 0.004f));
            const std::size_t age = glitchReadIncrement_ >= 0.0
                ? offset : glitchSliceFrames_ - 1u - offset;
            const float edge = std::min(1.0f,
                static_cast<float>(std::min(age,
                    glitchSliceFrames_ - 1u - age))
                    / static_cast<float>(fadeFrames));
            const float amount = mix * smoothStep(edge);
            for (std::size_t channel = 0u; channel < kLaneChannels; ++channel) {
                const float repeated = glitchBuffer_[channel][read]
                    * decay;
                stereo[channel] = dry[channel]
                    + (repeated - dry[channel]) * amount;
            }
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
    std::uint32_t randomState_ = 0x51f15e5du;
    float speed_ = 1.0f;
    float speedSlew_ = 1.0f;
    float laneSlew_ = 1.0f;
    float lastLaneSlewMs_ = -1.0f;
    float eventWindowSlew_ = 1.0f;
    float motionEnvelopeSlew_ = 1.0f;
    float motionInnerPhase_ = 0.0f;
    float motionOuterPhase_ = 0.5f;
    float motionSoundPhaseIncrement_ = 0.0f;
    float motionMotorPhaseIncrement_ = 0.0f;
    float motionPacketDuty_ = 0.65f;
    float motionMotorSymmetry_ = 0.5f;
    float motionTravel_ = 1.0f;
    float motionJitter_ = 0.0f;
    MotorShape motionMotorShape_ = MotorShape::rounded;
    float motionEnvelope_ = 1.0f;
    float motionInnerRateScale_ = 1.0f;
    MotionSegment motionSegments_[kMotionSegmentLayerCount] {};
    std::uint32_t motionSegmentAge_ = 0u;
    double motionEventPhase_ = 0.0;
    std::size_t motionEventRepeatIndex_ = 0u;
    double motionEventBasePosition_ = 0.5;
    float motionEventRateScale_ = 1.0f;
    std::uint32_t motionSeed_ = 1u;
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
    SampleMode currentSampleMode_ = SampleMode::forward;
    PlaybackAlgorithm currentAlgorithm_ = PlaybackAlgorithm::motion;
    PlaybackAlgorithm pendingAlgorithm_ = PlaybackAlgorithm::motion;
    std::size_t windowFadeFrames_ = 1440u;
    std::size_t windowFadeRemaining_ = 0u;
    std::size_t retriggerFadeFrames_ = 144u;
    std::size_t retriggerFadeRemaining_ = 0u;
    std::size_t retriggerTransitionFrames_ = 240u;
    std::size_t retriggerTransitionRemaining_ = 0u;
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
