#include "sample_motion_app.h"
#include "thru_schwung_abi.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <memory>
#include <new>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

using Engine = s3g_daisy::SampleMotionApp;

constexpr std::size_t kLaneCount = Engine::kStackLanes;
constexpr std::size_t kChannelCount = Engine::kChannels;
constexpr std::size_t kBlockFrames = MOVE_FRAMES_PER_BLOCK;
constexpr std::size_t kLaneSeconds = 12u;
constexpr std::size_t kWaveBins = 112u;
constexpr std::size_t kWaveTextSize = kLaneCount * kWaveBins * 2u
    + (kLaneCount - 1u) + 1u;
constexpr std::size_t kWaveScanBudget = 65536u;
constexpr std::size_t kVoiceCount = 8u;
constexpr std::size_t kClipBankCount = 4u;
constexpr std::size_t kClipsPerBank = 16u;
constexpr std::size_t kClipSlotCount = kClipBankCount * kClipsPerBank;
constexpr std::size_t kClipChainLength = 16u;
constexpr std::size_t kCueCount = 8u;
constexpr std::size_t kClipNoteCapacity = 1024u;
constexpr std::size_t kClipPointCapacity = 8192u;
constexpr std::uint8_t kClipTakeLimit = 255u;
constexpr std::uint32_t kClipMinimumDurationMs = 50u;
constexpr std::uint32_t kClipMaximumDurationMs = 120000u;
constexpr int kFirstPadNote = 68;
constexpr int kLastPadNote = 99;
constexpr int kPadColumns = 8;
constexpr int kPadRowInterval = 5;
constexpr int kPadBaseNote = 48;
constexpr std::size_t kPersistencePathSize = 768u;
constexpr std::size_t kStateTextSize = 8192u;
constexpr std::size_t kWavIoFrames = 512u;
constexpr std::size_t kLibraryCopyFrames = 4096u;
constexpr std::uint64_t kAnyPersistenceGeneration = ~std::uint64_t { 0u };
constexpr const char* kSampleLibraryRoot = "/data/UserData/UserLibrary/Samples/";

using WaveBins = std::array<std::int8_t, kWaveBins>;

enum class LaneSourceKind : std::uint8_t {
    file = 0u,
    capture,
    empty,
};

enum class PadMode : std::uint8_t {
    keys = 0u,
    slices,
    cues,
    rolls,
};

enum class LaneVariation : std::uint8_t {
    manual = 0u,
    cycle,
    random,
    noRepeat,
    velocity,
};

struct RegionMap {
    std::array<float, Engine::kRegionBoundaryCount> boundaries {};
    std::size_t count = 0u;
};

enum class ClipRecordMode : std::uint8_t {
    replace = 0u,
    overdub,
};

enum class ClipTimingMode : std::uint8_t {
    free = 0u,
    beatSync,
};

enum class LaunchQuantize : std::uint8_t {
    off = 0u,
    beat,
    bar,
};

enum class ClipPointType : std::uint8_t {
    parameter = 0u,
    pressure,
};

struct ClipNote {
    std::uint32_t startMs = 0u;
    std::uint32_t durationMs = 1u;
    std::uint8_t note = 60u;
    std::uint8_t velocity = 100u;
    std::uint8_t sourcePad = 255u;
    std::uint8_t take = 0u;
};

struct ClipPoint {
    std::uint32_t timeMs = 0u;
    float value = 0.0f;
    // Low byte: LockParameter. High byte: overdub take index.
    std::uint16_t parameter = 0u;
    std::uint8_t sourcePad = 255u;
    ClipPointType type = ClipPointType::parameter;
};
static_assert(sizeof(ClipPoint) == 12u,
              "overdub tags must not increase gesture-bank memory");

enum class LockParameter : std::uint32_t {
    locus = 0u,
    field,
    speed,
    manualLane,
    laneCycles,
    algorithm,
    join,
    outputGain,
    motionMode,
    motionSound,
    motionRate,
    lanesMode,
    lanePath,
    lanesStretch,
    stretchRatio,
    lane1Gain,
    lane2Gain,
    lane3Gain,
    lane4Gain,
    grainMode,
    grainSize,
    grainDensity,
    grainPitch,
    grainWindow,
    grainSpread,
    cutupsMode,
    motionTravel,
    motionJitter,
    packetDuty,
    motorRate,
    motorSymmetry,
    motorShape,
    laneBlend,
    lanePathShape,
    lanePathOffset,
    lanePathSkew,
    lanePathCurve,
    laneSlew,
    grainSpray,
    grainBias,
    grainPitchSpray,
    grainReverse,
    grainSizeVariation,
    grainLevelVariation,
    grainTiming,
    grainTimingScatter,
    grainEnvelopeSkew,
    grainProcess,
    grainProcessAmount,
    grainRegionCount,
    grainAdvance,
    cutClock,
    cutDivision,
    cutRate,
    cutTimeVariation,
    cutGate,
    cutJoin,
    cutRegionCount,
    cutRepeats,
    cutReverse,
    cutPitchVariation,
    cutLevelVariation,
    cutLaneOrder,
    cutPolyPath,
    cutRegionMode,
    sampleMode,
    sampleRegionMode,
    sampleRegionCount,
    motionRateBasis,
    motionPathRate,
    motionStep,
    motionSeed,
    segmentTrigger,
    segmentOverlap,
    segmentLayers,
    segmentRate,
    segmentRepeats,
    segmentStep,
    segmentPitch,
    segmentLevel,
    segmentCurve,
    glitchMode,
    glitchClock,
    glitchDivision,
    glitchRate,
    glitchSlice,
    glitchRepeats,
    glitchChance,
    glitchMix,
    glitchSteps,
    glitchPulses,
    glitchRotate,
    glitchJump,
    glitchPitch,
    glitchReverse,
    glitchDecay,
    glitchSeed,
    effectTone,
    effectDrive,
    effectBits,
    effectRate,
    effectDelay,
    effectFeedback,
    effectWidth,
    effectMix,
    count
};

constexpr std::size_t kLockParameterCount =
    static_cast<std::size_t>(LockParameter::count);
constexpr std::size_t kLegacyLockParameterCount = 68u;
constexpr std::size_t kVersion3LockParameterCount = 81u;
static_assert(kLockParameterCount == 105u,
              "clip point descriptors support up to 255 lock parameters");
constexpr std::size_t kClipCurveLaneCount = kLockParameterCount + 32u;
struct ClipSlot {
    std::array<float, kLockParameterCount> baseValues {};
    std::array<std::array<char, kPersistencePathSize>, kLaneCount>
        laneSourcePaths {};
    std::array<std::array<char, 64u>, kLaneCount> laneSourceLabels {};
    std::array<LaneSourceKind, kLaneCount> laneSourceKinds {
        LaneSourceKind::empty, LaneSourceKind::empty,
        LaneSourceKind::empty, LaneSourceKind::empty
    };
    std::array<float, kCueCount> cues {
        -1.0f, -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f
    };
    std::array<ClipNote, kClipNoteCapacity> clipNotes {};
    std::array<ClipPoint, kClipPointCapacity> clipPoints {};
    // Derived lookup data. These arrays are rebuilt after record/import and
    // are deliberately excluded from the persistent file format.
    std::array<std::uint16_t, kClipCurveLaneCount> clipPointStarts {};
    std::array<std::uint16_t, kClipCurveLaneCount> clipPointCounts {};
    std::uint32_t clipDurationMs = 0u;
    std::uint16_t clipNoteCount = 0u;
    std::uint16_t clipPointCount = 0u;
    std::uint8_t takeCount = 0u;
    float clipRate = 1.0f;
    float recordedTempoBpm = 120.0f;
    float recordedBeats = 0.0f;
    ClipTimingMode timingMode = ClipTimingMode::free;
    bool noteCapacityReached = false;
    bool pointCapacityReached = false;
    bool clipRecorded = false;
    bool occupied = false;
};

struct ClipChainEntry {
    std::uint8_t clip = 0u;
    std::uint8_t repeats = 1u;
    bool enabled = false;
};

enum class SampleLoadState : int {
    idle = 0,
    requested,
    loading,
    ready,
    removing,
    copying,
    complete,
    error,
    previewing,
};

enum class ClipPrepareState : int {
    idle = 0,
    requested,
    loading,
    ready,
    error,
};

const host_api_v1_t* g_host = nullptr;

constexpr const char* kAlgorithmNames[] {
    "Motion", "Lanes", "Grains", "Cutups", "Sample", "Slices", "Glitch"
};
constexpr const char* kSampleModeNames[] {
    "Forward", "Reverse", "Loop", "Ping Pong"
};
constexpr const char* kRegionModeNames[] { "Equal", "Transients" };
constexpr const char* kPadModeNames[] { "Keys", "Slices", "Cues", "Rolls" };
constexpr const char* kLaneVariationNames[] {
    "Manual", "Cycle", "Random", "No Repeat", "Velocity"
};
constexpr const char* kClipRecordModeNames[] { "Replace", "Overdub" };
constexpr const char* kClipTimingModeNames[] { "Free", "Beat Sync" };
constexpr const char* kLaunchQuantizeNames[] { "Off", "Beat", "Bar" };
constexpr const char* kGrainQualityNames[] { "4 Voices", "8 Voices", "16 Voices" };
constexpr const char* kMotionModeNames[] {
    "Forward", "Reverse", "Hover", "Round Trip", "Moving Loop", "Mirror", "Zigzag", "Drunk"
};
constexpr const char* kMotionSoundNames[] {
    "Continuous", "Packets", "Motor", "Bounce", "Freeze", "Iterate",
    "Pulser", "Doublets"
};
constexpr const char* kMotionRateBasisNames[] { "Normal", "Hertz" };
constexpr const char* kSegmentTriggerNames[] { "Clock", "Packet", "Turn" };
constexpr const char* kSegmentOverlapNames[] { "Cut", "Layer" };
constexpr const char* kLanesModeNames[] {
    "Forward", "Reverse", "Ping Pong"
};
constexpr const char* kGrainModeNames[] {
    "Scan", "Freeze", "Cloud", "Slice"
};
constexpr const char* kGrainWindowNames[] {
    "Parzen", "Sine", "Hann", "Triangle", "Gaussian", "Bell"
};
constexpr const char* kCutupsModeNames[] {
    "Forward", "Reverse", "Palindrome", "Random", "Walk", "Random Cycle"
};
constexpr const char* kLanePathNames[] {
    "Down", "Up", "Triangle", "Sine", "Steps Down", "Steps Up", "Manual", "Random"
};
constexpr const char* kMotorShapeNames[] {
    "Triangle", "Rounded", "Pulse", "Exponential", "Plateau"
};
constexpr const char* kLaneBlendNames[] { "Crossfade", "Jump" };
constexpr const char* kLanePathShapeNames[] {
    "Linear", "Smooth", "Exponential", "Plateau"
};
constexpr const char* kGrainTimingNames[] { "Regular", "Scatter" };
constexpr const char* kGrainBiasNames[] { "Behind", "Around", "Ahead" };
constexpr const char* kGrainProcessNames[] {
    "Ordinary", "Stutter", "Shrink", "Doublets", "Sorter"
};
constexpr const char* kGrainAdvanceNames[] { "Scan", "Per Grain" };
constexpr const char* kCutClockNames[] { "Free", "Tempo" };
constexpr const char* kCutDivisionNames[] {
    "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"
};
constexpr const char* kCutLaneOrderNames[] {
    "Down", "Up", "Palindrome", "Random", "Pairs", "Outside In", "Center Out"
};
constexpr const char* kCutPolyPathNames[] {
    "Together", "Stagger", "Rotate", "Random"
};
constexpr const char* kGlitchModeNames[] {
    "Repeat", "Stutter", "Scatter", "Divide", "Override"
};
constexpr const char* kOnOffNames[] { "Off", "On" };
constexpr const char* kPressureModeNames[] { "Off", "Offset" };
constexpr const char* kRecordSourceNames[] { "Input", "Resample" };

constexpr const char kMidiMap[] =
    "20 locus,21 field,22 speed,23 manual_lane,24 lane_cycles,25 algorithm,"
    "26 join,27 output_gain,28 motion_mode,29 motion_sound,30 motion_rate,"
    "31 lanes_mode,32 lane_path,33 lanes_stretch,34 stretch_ratio,"
    "35 lane_1_gain,36 lane_2_gain,37 lane_3_gain,38 lane_4_gain,"
    "39 grain_mode,40 grain_size,41 grain_density,42 grain_pitch,"
    "43 grain_window,44 grain_spread,45 cutups_mode,46 pressure_mode,"
    "47 root_note,48 record_lane,49 record_source,50 record_gain,"
    "51 monitor_level,52 record_gate,53 clear_lane,54 tempo_bpm,"
    "55 tempo_swing,56 clip_playing,57 undo,58 chain_clear,"
    "65 chain_enabled,66 chain_slot,67 chain_clip,"
    "68 chain_repeats,69 motion_travel,70 motion_jitter,71 packet_duty,"
    "72 motor_rate,73 motor_symmetry,74 motor_shape,75 lane_blend,"
    "76 lane_path_shape,77 lane_path_offset,78 lane_path_skew,"
    "79 lane_path_curve,80 lane_slew,81 grain_spray,82 grain_bias,"
    "83 grain_pitch_spray,84 grain_reverse,85 grain_size_variation,"
    "86 grain_level_variation,87 grain_timing,88 grain_timing_scatter,"
    "89 grain_envelope_skew,90 grain_process,91 grain_process_amount,"
    "92 grain_regions,93 grain_advance,94 cut_clock,95 cut_division,"
    "96 cut_rate,97 cut_time_variation,98 cut_gate,99 cut_join,"
    "100 cut_regions,101 cut_repeats,102 cut_reverse,"
    "103 cut_pitch_variation,104 cut_level_variation,"
    "105 cut_lane_order,106 cut_poly_path,107 cut_region_mode,"
    "108 sample_mode,109 slice_region_mode,110 slice_regions,"
    "111 pad_mode,112 lane_variation,113 preview_level,114 clip_rate,"
    "115 clip_record_mode,116 clip_record,117 clip_clear,"
    "59 motion_rate_basis,60 motion_path_rate,61 motion_step,62 motion_seed,"
    "63 segment_trigger,64 segment_overlap,118 segment_layers,"
    "119 segment_rate,121 segment_repeats,122 segment_step,"
    "124 segment_pitch,125 segment_level,126 segment_curve";

constexpr const char kChainParams[] = R"json([
{"key":"locus","name":"Position","short_name":"POS","type":"float","min":0,"max":1,"step":0.01,"default":0.5},
{"key":"field","name":"Field","short_name":"FIELD","type":"float","min":0.02,"max":1,"step":0.01,"default":1},
{"key":"speed","name":"Speed","short_name":"SPD","type":"float","min":0.05,"max":4,"step":0.01,"default":1},
{"key":"manual_lane","name":"Stack Start","short_name":"START","type":"float","min":1,"max":4,"step":0.01,"default":1},
{"key":"lane_cycles","name":"Stack Rate","short_name":"SCAN","type":"float","min":0.125,"max":16,"step":0.125,"default":1,"unit":"Hz"},
{"key":"algorithm","name":"Algorithm","short_name":"ALG","type":"enum","options":["Motion","Lanes","Grains","Cutups","Sample","Slices","Glitch"],"default":0},
{"key":"join","name":"Join","short_name":"JOIN","type":"float","min":0,"max":1,"step":0.01,"default":1},
{"key":"output_gain","name":"Level","short_name":"LEVEL","type":"float","min":0,"max":2,"step":0.01,"default":1},
{"key":"pressure_mode","name":"Pressure","short_name":"PRESS","type":"enum","options":["Off","Offset"],"default":1},
{"key":"root_note","name":"Root Note","short_name":"ROOT","type":"note","mode":"multi","min_note":24,"max_note":96,"default":60},
{"key":"record_lane","name":"Record Lane","short_name":"LANE","type":"int","min":1,"max":4,"default":1},
{"key":"record_source","name":"Record Source","short_name":"SOURCE","type":"enum","options":["Input","Resample"],"default":0},
{"key":"record_gain","name":"Record Gain","short_name":"REC LVL","type":"float","min":0,"max":4,"step":0.01,"default":1},
{"key":"monitor_level","name":"Monitor","short_name":"MON","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"clear_lane","name":"Clear Lane","short_name":"CLEAR","type":"enum","options":["Keep","Clear"],"default":0},
{"key":"sample_trim_start","name":"Trim Start","short_name":"START","type":"float","min":0,"max":0.99,"step":0.01,"default":0},
{"key":"sample_trim_end","name":"Trim End","short_name":"END","type":"float","min":0.01,"max":1,"step":0.01,"default":1},
{"key":"sample_reverse","name":"Reverse Sample","short_name":"REV","type":"enum","options":["Keep","Apply"],"default":0},
{"key":"sample_normalize","name":"Normalize Sample","short_name":"NORM","type":"enum","options":["Keep","Apply"],"default":0},
{"key":"sample_fade","name":"Edge Fade","short_name":"FADE","type":"enum","options":["Keep","Apply"],"default":0},
{"key":"sample_trim_apply","name":"Apply Trim","short_name":"TRIM","type":"enum","options":["Keep","Apply"],"default":0},
{"key":"motion_mode","name":"Motion Mode","short_name":"MODE","type":"enum","options":["Forward","Reverse","Hover","Round Trip","Moving Loop","Mirror","Zigzag","Drunk"],"default":0},
{"key":"motion_sound","name":"Motion Sound","short_name":"SOUND","type":"enum","options":["Continuous","Packets","Motor","Bounce","Freeze","Iterate","Pulser","Doublets"],"default":0},
{"key":"motion_rate","name":"Packet Rate","short_name":"PKT RATE","type":"float","min":0.25,"max":20,"step":0.25,"default":4,"unit":"Hz"},
{"key":"motion_rate_basis","name":"Path Rate Basis","short_name":"BASIS","type":"enum","options":["Normal","Hertz"],"default":0},
{"key":"motion_path_rate","name":"Path Rate","short_name":"PATH RATE","type":"float","min":0.01,"max":80,"step":0.01,"default":1},
{"key":"motion_step","name":"Moving Loop Step","short_name":"STEP","type":"float","min":0,"max":1,"step":0.01,"default":0.05},
{"key":"motion_seed","name":"Motion Seed","short_name":"SEED","type":"int","min":1,"max":9999,"default":1},
{"key":"motion_travel","name":"Path Travel","short_name":"TRAVEL","type":"float","min":0,"max":1,"step":0.01,"default":1},
{"key":"motion_jitter","name":"Path Jitter","short_name":"JITTER","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"packet_duty","name":"Packet Duty","short_name":"DUTY","type":"float","min":2,"max":98,"step":1,"default":65,"unit":"%"},
{"key":"motor_rate","name":"Motor Rate","short_name":"M RATE","type":"float","min":0.05,"max":20,"step":0.05,"default":1,"unit":"Hz"},
{"key":"motor_symmetry","name":"Motor Symmetry","short_name":"SYM","type":"float","min":5,"max":95,"step":1,"default":50,"unit":"%"},
{"key":"motor_shape","name":"Motor Shape","short_name":"SHAPE","type":"enum","options":["Triangle","Rounded","Pulse","Exponential","Plateau"],"default":1},
{"key":"segment_trigger","name":"Event Trigger","short_name":"TRIGGER","type":"enum","options":["Clock","Packet","Turn"],"default":0},
{"key":"segment_overlap","name":"Event Overlap","short_name":"OVERLAP","type":"enum","options":["Cut","Layer"],"default":0},
{"key":"segment_layers","name":"Event Layers","short_name":"LAYERS","type":"int","min":1,"max":4,"default":2},
{"key":"segment_rate","name":"Event Rate","short_name":"EV RATE","type":"float","min":0.25,"max":80,"step":0.25,"default":4,"unit":"Hz"},
{"key":"segment_repeats","name":"Event Repeats","short_name":"REPEATS","type":"int","min":1,"max":16,"default":2},
{"key":"segment_step","name":"Event Step","short_name":"EV STEP","type":"float","min":0,"max":1,"step":0.01,"default":0.05},
{"key":"segment_pitch","name":"Pitch Scatter","short_name":"PITCH","type":"float","min":0,"max":12,"step":0.1,"default":0,"unit":"st"},
{"key":"segment_level","name":"Level Variation","short_name":"LEVEL","type":"float","min":0,"max":1,"step":0.01,"default":0.15},
{"key":"segment_curve","name":"Interval Curve","short_name":"CURVE","type":"float","min":-1,"max":1,"step":0.01,"default":0},
{"key":"lanes_mode","name":"Lanes Mode","short_name":"MODE","type":"enum","options":["Forward","Reverse","Ping Pong"],"default":0},
{"key":"lane_path","name":"Stack Path","short_name":"PATH","type":"enum","options":["Down","Up","Triangle","Sine","Steps Down","Steps Up","Manual","Random"],"default":2},
{"key":"stack_scan_paused","name":"Stack Scan","short_name":"SCAN","type":"enum","options":["Run","Hold"],"default":0,"live":true},
{"key":"lane_blend","name":"Lane Transition","short_name":"BLEND","type":"enum","options":["Crossfade","Jump"],"default":0},
{"key":"lane_path_shape","name":"Path Shape","short_name":"SHAPE","type":"enum","options":["Linear","Smooth","Exponential","Plateau"],"default":0},
{"key":"lane_path_offset","name":"Path Offset","short_name":"OFFSET","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"lane_path_skew","name":"Path Skew","short_name":"SKEW","type":"float","min":0.02,"max":0.98,"step":0.01,"default":0.5},
{"key":"lane_path_curve","name":"Path Curve","short_name":"CURVE","type":"float","min":-1,"max":1,"step":0.01,"default":0},
{"key":"lane_slew","name":"Jump Slew","short_name":"SLEW","type":"float","min":0,"max":500,"step":1,"default":20,"unit":"ms"},
{"key":"lanes_stretch","name":"Stretch","short_name":"STRCH","type":"enum","options":["Off","On"],"default":1},
{"key":"stretch_ratio","name":"Stretch Ratio","short_name":"RATIO","type":"float","min":0.25,"max":4,"step":0.01,"default":1},
{"key":"lane_1_gain","name":"Lane 1","short_name":"L1","type":"float","min":0,"max":2,"step":0.01,"default":1},
{"key":"lane_2_gain","name":"Lane 2","short_name":"L2","type":"float","min":0,"max":2,"step":0.01,"default":1},
{"key":"lane_3_gain","name":"Lane 3","short_name":"L3","type":"float","min":0,"max":2,"step":0.01,"default":1},
{"key":"lane_4_gain","name":"Lane 4","short_name":"L4","type":"float","min":0,"max":2,"step":0.01,"default":1},
{"key":"grain_mode","name":"Grain Mode","short_name":"MODE","type":"enum","options":["Scan","Freeze","Cloud","Slice"],"default":0},
{"key":"grain_size","name":"Grain Size","short_name":"SIZE","type":"float","min":8,"max":1000,"step":1,"default":90,"unit":"ms"},
{"key":"grain_density","name":"Density","short_name":"DENS","type":"float","min":1,"max":96,"step":1,"default":24,"unit":"Hz"},
{"key":"grain_quality","name":"Grain Quality","short_name":"QUALITY","type":"enum","options":["4 Voices","8 Voices","16 Voices"],"default":1},
{"key":"grain_pitch","name":"Grain Pitch","short_name":"PITCH","type":"float","min":0.25,"max":4,"step":0.01,"default":1},
{"key":"grain_window","name":"Window","short_name":"WIN","type":"enum","options":["Parzen","Sine","Hann","Triangle","Gaussian","Bell"],"default":5},
{"key":"grain_spread","name":"Stereo Spread","short_name":"SPREAD","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"grain_spray","name":"Position Spray","short_name":"SPRAY","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"grain_bias","name":"Spray Bias","short_name":"BIAS","type":"enum","options":["Behind","Around","Ahead"],"default":1},
{"key":"grain_pitch_spray","name":"Pitch Spray","short_name":"P SPRAY","type":"float","min":0,"max":24,"step":0.1,"default":0,"unit":"st"},
{"key":"grain_reverse","name":"Reverse Chance","short_name":"REVERSE","type":"float","min":0,"max":100,"step":1,"default":0,"unit":"%"},
{"key":"grain_size_variation","name":"Size Variation","short_name":"SIZE VAR","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"grain_level_variation","name":"Level Variation","short_name":"LVL VAR","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"grain_timing","name":"Grain Timing","short_name":"TIMING","type":"enum","options":["Regular","Scatter"],"default":0},
{"key":"grain_timing_scatter","name":"Timing Scatter","short_name":"T SCAT","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"grain_envelope_skew","name":"Envelope Skew","short_name":"ENV SKEW","type":"float","min":-1,"max":1,"step":0.01,"default":0},
{"key":"grain_process","name":"Grain Process","short_name":"PROCESS","type":"enum","options":["Ordinary","Stutter","Shrink","Doublets","Sorter"],"default":0},
{"key":"grain_process_amount","name":"Process Amount","short_name":"AMOUNT","type":"float","min":0,"max":1,"step":0.01,"default":0.5},
{"key":"grain_regions","name":"Slice Regions","short_name":"REGIONS","type":"int","min":1,"max":64,"default":16},
{"key":"grain_advance","name":"Source Advance","short_name":"ADVANCE","type":"enum","options":["Scan","Per Grain"],"default":0},
{"key":"cutups_mode","name":"Cutups Mode","short_name":"MODE","type":"enum","options":["Forward","Reverse","Palindrome","Random","Walk","Random Cycle"],"default":0},
{"key":"cut_clock","name":"Cut Clock","short_name":"CLOCK","type":"enum","options":["Free","Tempo"],"default":1},
{"key":"cut_division","name":"Cut Division","short_name":"DIV","type":"enum","options":["1/4","1/8","1/8T","1/16","1/16T","1/32"],"default":3},
{"key":"cut_rate","name":"Free Cut Rate","short_name":"RATE","type":"float","min":0.25,"max":64,"step":0.25,"default":8,"unit":"Hz"},
{"key":"cut_time_variation","name":"Time Variation","short_name":"TIME VAR","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"cut_gate","name":"Cut Gate","short_name":"GATE","type":"float","min":5,"max":100,"step":1,"default":100,"unit":"%"},
{"key":"cut_join","name":"Cut Join","short_name":"JOIN","type":"float","min":0,"max":100,"step":1,"default":10,"unit":"ms"},
{"key":"cut_regions","name":"Cut Regions","short_name":"REGIONS","type":"int","min":1,"max":64,"default":16},
{"key":"cut_repeats","name":"Cut Repeats","short_name":"REPEATS","type":"int","min":1,"max":16,"default":1},
{"key":"cut_reverse","name":"Reverse Chance","short_name":"REVERSE","type":"float","min":0,"max":100,"step":1,"default":0,"unit":"%"},
{"key":"cut_pitch_variation","name":"Pitch Variation","short_name":"PITCH VAR","type":"float","min":0,"max":24,"step":0.1,"default":0,"unit":"st"},
{"key":"cut_level_variation","name":"Level Variation","short_name":"LEVEL VAR","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"cut_lane_order","name":"Lane Order","short_name":"LANE ORD","type":"enum","options":["Down","Up","Palindrome","Random","Pairs","Outside In","Center Out"],"default":0},
{"key":"cut_poly_path","name":"Poly Path","short_name":"POLY","type":"enum","options":["Together","Stagger","Rotate","Random"],"default":0},
{"key":"cut_region_mode","name":"Cut Region Source","short_name":"REGION SRC","type":"enum","options":["Equal","Transients"],"default":0},
{"key":"sample_mode","name":"Sample Playback","short_name":"PLAY MODE","type":"enum","options":["Forward","Reverse","Loop","Ping Pong"],"default":0},
{"key":"slice_region_mode","name":"Slice Region Source","short_name":"SLICE SRC","type":"enum","options":["Equal","Transients"],"default":1},
{"key":"slice_regions","name":"Equal Slice Count","short_name":"SLICES","type":"int","min":1,"max":64,"default":16},
{"key":"glitch_mode","name":"Glitch Mode","short_name":"MODE","type":"enum","options":["Repeat","Stutter","Scatter","Divide","Override"],"default":4},
{"key":"glitch_clock","name":"Glitch Clock","short_name":"CLOCK","type":"enum","options":["Free","Tempo"],"default":1},
{"key":"glitch_division","name":"Glitch Division","short_name":"DIV","type":"enum","options":["1/4","1/8","1/8T","1/16","1/16T","1/32"],"default":3},
{"key":"glitch_rate","name":"Free Glitch Rate","short_name":"RATE","type":"float","min":0.25,"max":64,"step":0.25,"default":8,"unit":"Hz"},
{"key":"glitch_slice","name":"Repeat Buffer","short_name":"BUFFER","type":"float","min":2,"max":160,"step":1,"default":32,"unit":"ms"},
{"key":"glitch_repeats","name":"Repeat Count","short_name":"REPEATS","type":"int","min":1,"max":16,"default":4},
{"key":"glitch_chance","name":"Hit Chance","short_name":"CHANCE","type":"float","min":0,"max":100,"step":1,"default":100,"unit":"%"},
{"key":"glitch_mix","name":"Buffer Override","short_name":"OVERRIDE","type":"float","min":0,"max":100,"step":1,"default":100,"unit":"%"},
{"key":"glitch_steps","name":"Pattern Length","short_name":"STEPS","type":"int","min":1,"max":16,"default":11},
{"key":"glitch_pulses","name":"Pattern Pulses","short_name":"PULSES","type":"int","min":0,"max":16,"default":5},
{"key":"glitch_rotate","name":"Pattern Rotate","short_name":"ROTATE","type":"int","min":0,"max":15,"default":0},
{"key":"glitch_jump","name":"History Jump","short_name":"JUMP","type":"float","min":0,"max":160,"step":1,"default":120,"unit":"ms"},
{"key":"glitch_pitch","name":"Pitch Range","short_name":"PITCH","type":"float","min":0,"max":24,"step":0.1,"default":7,"unit":"st"},
{"key":"glitch_reverse","name":"Reverse Chance","short_name":"REVERSE","type":"float","min":0,"max":100,"step":1,"default":25,"unit":"%"},
{"key":"glitch_decay","name":"Repeat Decay","short_name":"DECAY","type":"float","min":0,"max":100,"step":1,"default":90,"unit":"%"},
{"key":"glitch_seed","name":"Pattern Seed","short_name":"SEED","type":"int","min":1,"max":9999,"default":1},
{"key":"effect_tone","name":"Effect Tone","short_name":"TONE","type":"float","min":-1,"max":1,"step":0.01,"default":0},
{"key":"effect_drive","name":"Effect Drive","short_name":"DRIVE","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"effect_bits","name":"Bit Reduction","short_name":"BITS","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"effect_rate","name":"Rate Reduction","short_name":"RATE","type":"int","min":1,"max":16,"default":1},
{"key":"effect_delay","name":"Delay Time","short_name":"DELAY","type":"float","min":1,"max":750,"step":1,"default":250,"unit":"ms"},
{"key":"effect_feedback","name":"Delay Feedback","short_name":"FDBK","type":"float","min":0,"max":90,"step":1,"default":35,"unit":"%"},
{"key":"effect_width","name":"Delay Width","short_name":"WIDTH","type":"float","min":0,"max":100,"step":1,"default":100,"unit":"%"},
{"key":"effect_mix","name":"Delay Mix","short_name":"DLY MIX","type":"float","min":0,"max":100,"step":1,"default":0,"unit":"%"},
{"key":"pad_mode","name":"Pad Mode","short_name":"PADS","type":"enum","options":["Keys","Slices","Cues","Rolls"],"default":0},
{"key":"lane_variation","name":"Lane Variation","short_name":"VARIATION","type":"enum","options":["Manual","Cycle","Random","No Repeat","Velocity"],"default":0},
{"key":"preview_level","name":"Browser Preview","short_name":"PREVIEW","type":"float","min":0,"max":1,"step":0.01,"default":0.7},
{"key":"clip_rate","name":"Performance Rate","short_name":"LOOP RATE","type":"float","min":0.25,"max":4,"step":0.01,"default":1},
{"key":"clip_record_mode","name":"Performance Record","short_name":"REC MODE","type":"enum","options":["Replace","Overdub"],"default":0},
{"key":"clip_timing","name":"Clip Timing","short_name":"TIMING","type":"enum","options":["Free","Beat Sync"],"default":0},
{"key":"launch_quantize","name":"Launch Quantize","short_name":"LAUNCH","type":"enum","options":["Off","Beat","Bar"],"default":0},
{"key":"clip_record","name":"Record Performance","type":"int","min":0,"max":1,"access":"write"},
{"key":"clip_clear","name":"Clear Performance","type":"int","min":0,"max":1,"access":"write"},
{"key":"clip_take_undo","name":"Undo Last Overdub Take","type":"int","min":0,"max":1,"access":"write"},
{"key":"clip_reset","name":"Reset Clip","type":"int","min":0,"max":63,"access":"write"},
{"key":"clip_automation","name":"Record Gesture Point","type":"string","access":"write"},
{"key":"clip_state","name":"Performance Loop State","type":"string","access":"read","live":true},
{"key":"clip_take_state","name":"Performance Take State","type":"string","access":"read","live":true},
{"key":"clip_capacity_state","name":"Clip Capture Capacity","type":"string","access":"read","live":true},
{"key":"clip_playing","name":"Performance Playback","type":"int","min":0,"max":1,"access":"write"},
{"key":"tempo_bpm","name":"Tempo","short_name":"BPM","type":"float","min":40,"max":240,"step":1,"default":120,"unit":"BPM"},
{"key":"tempo_swing","name":"Tempo Swing","short_name":"SWING","type":"float","min":50,"max":75,"step":1,"default":50,"unit":"%"},
{"key":"chain_enabled","name":"Clip Chain","short_name":"CHAIN","type":"enum","options":["Off","On"],"default":0},
{"key":"chain_slot","name":"Chain Slot","short_name":"SLOT","type":"int","min":1,"max":16,"default":1},
{"key":"chain_clip","name":"Chain Clip","short_name":"CLIP","type":"int","min":1,"max":64,"default":1},
{"key":"chain_repeats","name":"Chain Repeats","short_name":"REPT","type":"int","min":1,"max":16,"default":1},
{"key":"chain_clear","name":"Clear Chain Slot","type":"int","min":0,"max":1,"access":"write"},
{"key":"chain_insert","name":"Insert Chain Entry","type":"string","access":"write"},
{"key":"chain_delete","name":"Delete Chain Entry","type":"int","min":0,"max":15,"access":"write"},
{"key":"chain_move","name":"Move Chain Entry","type":"string","access":"write"},
{"key":"chain_state","name":"Clip Chain State","type":"string","access":"read","live":true},
{"key":"chain_entries","name":"Clip Chain Entries","type":"string","access":"read","live":true},
{"key":"undo_available","name":"Undo Available","type":"int","min":0,"max":1,"access":"read","live":true},
{"key":"clip_bank_state","name":"Clip Bank State","type":"string","access":"read","live":true},
{"key":"sample_load_state","name":"Sample Load State","type":"string","access":"read","live":true},
{"key":"region_state","name":"Transient Regions","type":"string","access":"read","live":true},
{"key":"cue_state","name":"Clip Cues","type":"string","access":"read","live":true},
{"key":"preview_file","name":"Preview Sample","type":"string","access":"write"},
{"key":"preview_stop","name":"Stop Preview","type":"int","min":0,"max":1,"access":"write"},
{"key":"cue_store","name":"Store Cue","type":"string","access":"write"},
{"key":"cue_trigger","name":"Trigger Cue","type":"int","min":0,"max":7,"access":"write"},
{"key":"cue_clear","name":"Clear Cue","type":"int","min":0,"max":7,"access":"write"},
{"key":"clip_prepare_state","name":"Clip Prepare State","type":"string","access":"read","live":true},
{"key":"lane_source","name":"Lane Source","type":"string","access":"read","live":true},
{"key":"lane_source_state","name":"Lane Source State","type":"string","access":"read","live":true},
{"key":"last_error","name":"Last Error","type":"string","access":"read","live":true},
{"key":"recording","name":"Recording","type":"int","min":0,"max":1,"access":"read","live":true},
{"key":"ui_state","name":"UI State","type":"string","access":"read","live":true},
{"key":"performance_state","name":"Realtime Performance","type":"string","access":"read","live":true},
{"key":"memory_state","name":"Sampler Memory","type":"string","access":"read"},
{"key":"playheads","name":"Voice Playheads","type":"string","access":"read","live":true},
{"key":"voice_lanes","name":"Voice Stack Positions","type":"string","access":"read","live":true},
{"key":"voice_pads","name":"Voice Source Pads","type":"string","access":"read","live":true},
{"key":"midi_map","name":"MIDI CC Map","type":"string","access":"read"},
{"key":"waveforms","name":"Waveforms","type":"string","access":"read"}
])json";

constexpr const char kUiHierarchy[] = R"json({
"pad_layout":"chromatic",
"levels":{
"root":{"name":"s3g Thru","knobs":["algorithm","locus","field","speed","manual_lane","lane_cycles","join","output_gain"],"params":[{"level":"record","label":"Record"},{"level":"motion","label":"Motion"},{"level":"lanes","label":"Lanes"},{"level":"grains","label":"Grains"},{"level":"cutups","label":"Cutups"},{"level":"sample","label":"Sample"},{"level":"slices","label":"Slices"},{"level":"glitch","label":"Glitch"},{"level":"effects","label":"Effects"},{"level":"lane_mix","label":"Lane Mix"},{"level":"performance_loop","label":"Performance Loop"}]},
"record":{"name":"Record","knobs":["record_lane","record_source","record_gain","monitor_level","pressure_mode","root_note","output_gain","clear_lane"],"params":[{"key":"record_lane"},{"key":"record_source"},{"key":"record_gain"},{"key":"monitor_level"},{"key":"pressure_mode"},{"key":"root_note"},{"key":"output_gain"},{"key":"clear_lane"}]},
"motion":{"name":"Motion","knobs":["locus","field","speed","join","motion_mode","motion_sound","motion_path_rate","output_gain"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"motion_mode"},{"key":"motion_sound"},{"key":"motion_rate_basis"},{"key":"motion_path_rate"},{"key":"motion_step"},{"key":"motion_seed"},{"key":"motion_rate"},{"key":"packet_duty"},{"key":"motor_rate"},{"key":"motor_symmetry"},{"key":"motor_shape"},{"key":"segment_trigger"},{"key":"segment_overlap"},{"key":"segment_layers"},{"key":"segment_rate"},{"key":"segment_repeats"},{"key":"segment_step"},{"key":"segment_pitch"},{"key":"segment_level"},{"key":"segment_curve"},{"key":"output_gain"}]},
"lanes":{"name":"Lanes","knobs":["locus","field","speed","join","lanes_mode","lane_path","lane_cycles","manual_lane"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"lanes_mode"},{"key":"lane_path"},{"key":"lane_cycles"},{"key":"manual_lane"},{"key":"lanes_stretch"},{"key":"stretch_ratio"}]},
"grains":{"name":"Grains","knobs":["locus","field","speed","join","grain_mode","grain_size","grain_density","grain_pitch"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"grain_mode"},{"key":"grain_size"},{"key":"grain_density"},{"key":"grain_pitch"},{"key":"grain_window"},{"key":"grain_spread"},{"key":"lane_path"},{"key":"lane_cycles"},{"key":"manual_lane"}]},
"cutups":{"name":"Cutups","knobs":["locus","field","speed","join","cutups_mode","lane_path","lane_cycles","manual_lane"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"cutups_mode"},{"key":"lane_path"},{"key":"lane_cycles"},{"key":"manual_lane"}]},
"sample":{"name":"Sample","knobs":["sample_mode","locus","field","speed","manual_lane","lane_variation","pad_mode","output_gain"],"params":[{"key":"sample_mode"},{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"manual_lane"},{"key":"lane_variation"},{"key":"pad_mode"},{"key":"output_gain"}]},
"slices":{"name":"Slices","knobs":["sample_mode","slice_region_mode","slice_regions","speed","manual_lane","lane_variation","pad_mode","output_gain"],"params":[{"key":"sample_mode"},{"key":"slice_region_mode"},{"key":"slice_regions"},{"key":"speed"},{"key":"manual_lane"},{"key":"lane_variation"},{"key":"pad_mode"},{"key":"output_gain"}]},
"glitch":{"name":"Glitch","knobs":["glitch_mode","glitch_clock","glitch_division","glitch_slice","glitch_steps","glitch_pulses","glitch_jump","glitch_mix"],"params":[{"key":"glitch_mode"},{"key":"glitch_clock"},{"key":"glitch_division"},{"key":"glitch_rate"},{"key":"glitch_slice"},{"key":"glitch_repeats"},{"key":"glitch_chance"},{"key":"glitch_mix"},{"key":"glitch_steps"},{"key":"glitch_pulses"},{"key":"glitch_rotate"},{"key":"glitch_jump"},{"key":"glitch_pitch"},{"key":"glitch_reverse"},{"key":"glitch_decay"},{"key":"glitch_seed"}]},
"effects":{"name":"Effects","knobs":["effect_tone","effect_drive","effect_bits","effect_rate","effect_delay","effect_feedback","effect_width","effect_mix"],"params":[{"key":"effect_tone"},{"key":"effect_drive"},{"key":"effect_bits"},{"key":"effect_rate"},{"key":"effect_delay"},{"key":"effect_feedback"},{"key":"effect_width"},{"key":"effect_mix"}]},
"lane_mix":{"name":"Lane Mix","knobs":["lane_1_gain","lane_2_gain","lane_3_gain","lane_4_gain","lanes_stretch","stretch_ratio","pressure_mode","output_gain"],"params":[{"key":"lane_1_gain"},{"key":"lane_2_gain"},{"key":"lane_3_gain"},{"key":"lane_4_gain"},{"key":"lanes_stretch"},{"key":"stretch_ratio"},{"key":"pressure_mode"},{"key":"output_gain"}]},
"performance_loop":{"name":"Performance Loop","knobs":["clip_rate","clip_record_mode","tempo_bpm","tempo_swing","chain_enabled","chain_slot","chain_clip","chain_repeats"],"params":[{"key":"clip_rate"},{"key":"clip_record_mode"},{"key":"tempo_bpm"},{"key":"tempo_swing"},{"key":"chain_enabled"},{"key":"chain_slot"},{"key":"chain_clip"},{"key":"chain_repeats"},{"key":"chain_clear"}]}
}
})json";

template <typename T>
T clampValue(T value, T low, T high)
{
    return std::max(low, std::min(high, value));
}

bool isTruthy(const char* value)
{
    return value && (std::strcmp(value, "1") == 0
        || std::strcmp(value, "true") == 0
        || std::strcmp(value, "On") == 0
        || std::strcmp(value, "Clear") == 0);
}

float parseFloat(const char* value, float fallback)
{
    if (!value) return fallback;
    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    return end != value && std::isfinite(parsed) ? parsed : fallback;
}

int parseInteger(const char* value, int fallback)
{
    if (!value) return fallback;
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    return end != value ? static_cast<int>(parsed) : fallback;
}

float midiUnit(int value)
{
    return static_cast<float>(clampValue(value, 0, 127)) / 127.0f;
}

float midiRange(int value, float low, float high)
{
    return low + (high - low) * midiUnit(value);
}

int midiChoice(int value, int count)
{
    if (count <= 1) return 0;
    return clampValue(static_cast<int>(std::lround(
        midiUnit(value) * static_cast<float>(count - 1))), 0, count - 1);
}

template <std::size_t Count>
int parseEnum(const char* value, const char* const (&names)[Count], int fallback)
{
    if (!value) return fallback;
    for (std::size_t index = 0u; index < Count; ++index) {
        if (std::strcmp(value, names[index]) == 0) {
            return static_cast<int>(index);
        }
    }
    return clampValue(parseInteger(value, fallback), 0, static_cast<int>(Count) - 1);
}

int lockParameterIndex(const char* key)
{
    if (!key) return -1;
    static constexpr const char* keys[kLockParameterCount] {
        "locus", "field", "speed", "manual_lane", "lane_cycles",
        "algorithm", "join", "output_gain", "motion_mode", "motion_sound",
        "motion_rate", "lanes_mode", "lane_path", "lanes_stretch",
        "stretch_ratio", "lane_1_gain", "lane_2_gain", "lane_3_gain",
        "lane_4_gain", "grain_mode", "grain_size", "grain_density",
        "grain_pitch", "grain_window", "grain_spread", "cutups_mode",
        "motion_travel", "motion_jitter", "packet_duty", "motor_rate",
        "motor_symmetry", "motor_shape", "lane_blend", "lane_path_shape",
        "lane_path_offset", "lane_path_skew", "lane_path_curve", "lane_slew",
        "grain_spray", "grain_bias", "grain_pitch_spray", "grain_reverse",
        "grain_size_variation", "grain_level_variation", "grain_timing",
        "grain_timing_scatter", "grain_envelope_skew", "grain_process",
        "grain_process_amount", "grain_regions", "grain_advance", "cut_clock",
        "cut_division", "cut_rate", "cut_time_variation", "cut_gate",
        "cut_join", "cut_regions", "cut_repeats", "cut_reverse",
        "cut_pitch_variation", "cut_level_variation", "cut_lane_order",
        "cut_poly_path", "cut_region_mode", "sample_mode",
        "slice_region_mode", "slice_regions", "motion_rate_basis",
        "motion_path_rate", "motion_step", "motion_seed",
        "segment_trigger", "segment_overlap", "segment_layers",
        "segment_rate", "segment_repeats", "segment_step",
        "segment_pitch", "segment_level", "segment_curve",
        "glitch_mode", "glitch_clock", "glitch_division", "glitch_rate",
        "glitch_slice", "glitch_repeats", "glitch_chance", "glitch_mix",
        "glitch_steps", "glitch_pulses", "glitch_rotate", "glitch_jump",
        "glitch_pitch", "glitch_reverse", "glitch_decay", "glitch_seed",
        "effect_tone", "effect_drive", "effect_bits", "effect_rate",
        "effect_delay", "effect_feedback", "effect_width", "effect_mix"
    };
    for (std::size_t index = 0u; index < kLockParameterCount; ++index) {
        if (std::strcmp(key, keys[index]) == 0) return static_cast<int>(index);
    }
    return -1;
}

void applyLockedValue(Engine::Controls& controls, float& outputGain,
                      std::size_t index, float value)
{
    switch (static_cast<LockParameter>(index)) {
        case LockParameter::locus: controls.locus = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::field: controls.field = clampValue(value, 0.02f, 1.0f); break;
        case LockParameter::speed: controls.speed = clampValue(value, 0.05f, 4.0f); break;
        case LockParameter::manualLane: controls.manualLane = clampValue(value - 1.0f, 0.0f, 3.0f); break;
        case LockParameter::laneCycles: controls.laneCycles = clampValue(value, 0.125f, 16.0f); break;
        case LockParameter::algorithm: controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(clampValue(static_cast<int>(std::lround(value)), 0, static_cast<int>(Engine::kPlaybackAlgorithmCount) - 1)); break;
        case LockParameter::join: controls.join = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::outputGain: outputGain = clampValue(value, 0.0f, 2.0f); break;
        case LockParameter::motionMode: controls.motion = static_cast<Engine::MotionMode>(clampValue(static_cast<int>(std::lround(value)), 0, 7)); break;
        case LockParameter::motionSound: controls.motionSound = static_cast<Engine::MotionSound>(clampValue(static_cast<int>(std::lround(value)), 0, static_cast<int>(Engine::kMotionSoundCount) - 1)); break;
        case LockParameter::motionRate: controls.motionSoundRateHz = clampValue(value, 0.25f, 20.0f); break;
        case LockParameter::lanesMode: controls.lanes = static_cast<Engine::LanesMode>(clampValue(static_cast<int>(std::lround(value)), 0, 2)); break;
        case LockParameter::lanePath: controls.lanePath = static_cast<Engine::LanePath>(clampValue(static_cast<int>(std::lround(value)), 0, 7)); break;
        case LockParameter::lanesStretch: controls.lanesStretch = value >= 0.5f; break;
        case LockParameter::stretchRatio: controls.lanesStretchRatio = clampValue(value, 0.25f, 4.0f); break;
        case LockParameter::lane1Gain: controls.laneGains[0u] = clampValue(value, 0.0f, 2.0f); break;
        case LockParameter::lane2Gain: controls.laneGains[1u] = clampValue(value, 0.0f, 2.0f); break;
        case LockParameter::lane3Gain: controls.laneGains[2u] = clampValue(value, 0.0f, 2.0f); break;
        case LockParameter::lane4Gain: controls.laneGains[3u] = clampValue(value, 0.0f, 2.0f); break;
        case LockParameter::grainMode: controls.grains = static_cast<Engine::GrainMode>(clampValue(static_cast<int>(std::lround(value)), 0, 3)); break;
        case LockParameter::grainSize: controls.grainSizeMs = clampValue(value, 8.0f, 1000.0f); break;
        case LockParameter::grainDensity: controls.grainDensityHz = clampValue(value, 1.0f, 96.0f); break;
        case LockParameter::grainPitch: controls.grainPitch = clampValue(value, 0.25f, 4.0f); break;
        case LockParameter::grainWindow: controls.grainWindow = static_cast<Engine::GrainWindow>(clampValue(static_cast<int>(std::lround(value)), 0, 5)); break;
        case LockParameter::grainSpread: controls.grainStereoSpread = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::cutupsMode: controls.cutups = static_cast<Engine::CutupsMode>(clampValue(static_cast<int>(std::lround(value)), 0, static_cast<int>(Engine::kCutupsModeCount) - 1)); break;
        case LockParameter::motionTravel: controls.motionTravel = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::motionJitter: controls.motionJitter = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::packetDuty: controls.packetDuty = clampValue(value / 100.0f, 0.02f, 0.98f); break;
        case LockParameter::motorRate: controls.motorRateHz = clampValue(value, 0.05f, 20.0f); break;
        case LockParameter::motorSymmetry: controls.motorSymmetry = clampValue(value / 100.0f, 0.05f, 0.95f); break;
        case LockParameter::motorShape: controls.motorShape = static_cast<Engine::MotorShape>(clampValue(static_cast<int>(std::lround(value)), 0, static_cast<int>(Engine::kMotorShapeCount) - 1)); break;
        case LockParameter::laneBlend: controls.laneBlend = static_cast<Engine::LaneBlendMode>(clampValue(static_cast<int>(std::lround(value)), 0, 1)); break;
        case LockParameter::lanePathShape: controls.lanePathShape = static_cast<Engine::LanePathShape>(clampValue(static_cast<int>(std::lround(value)), 0, 3)); break;
        case LockParameter::lanePathOffset: controls.lanePathOffset = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::lanePathSkew: controls.lanePathSkew = clampValue(value, 0.02f, 0.98f); break;
        case LockParameter::lanePathCurve: controls.lanePathCurve = clampValue(value, -1.0f, 1.0f); break;
        case LockParameter::laneSlew: controls.laneSlewMs = clampValue(value, 0.0f, 500.0f); break;
        case LockParameter::grainSpray: controls.grainSpray = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::grainBias: controls.grainBias = static_cast<Engine::GrainBias>(clampValue(static_cast<int>(std::lround(value)), 0, 2)); break;
        case LockParameter::grainPitchSpray: controls.grainPitchSpray = clampValue(value, 0.0f, 24.0f); break;
        case LockParameter::grainReverse: controls.grainReverseChance = clampValue(value / 100.0f, 0.0f, 1.0f); break;
        case LockParameter::grainSizeVariation: controls.grainSizeVariation = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::grainLevelVariation: controls.grainLevelVariation = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::grainTiming: controls.grainTiming = static_cast<Engine::GrainTiming>(clampValue(static_cast<int>(std::lround(value)), 0, 1)); break;
        case LockParameter::grainTimingScatter: controls.grainTimingScatter = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::grainEnvelopeSkew: controls.grainEnvelopeSkew = clampValue(value, -1.0f, 1.0f); break;
        case LockParameter::grainProcess: controls.grainProcess = static_cast<Engine::GrainProcess>(clampValue(static_cast<int>(std::lround(value)), 0, static_cast<int>(Engine::kGrainProcessCount) - 1)); break;
        case LockParameter::grainProcessAmount: controls.grainProcessAmount = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::grainRegionCount: controls.grainRegionCount = clampValue(value, 1.0f, 64.0f); break;
        case LockParameter::grainAdvance: controls.grainAdvance = static_cast<Engine::GrainAdvance>(clampValue(static_cast<int>(std::lround(value)), 0, 1)); break;
        case LockParameter::cutClock: controls.cutClock = static_cast<Engine::CutClock>(clampValue(static_cast<int>(std::lround(value)), 0, 1)); break;
        case LockParameter::cutDivision: controls.cutDivision = static_cast<Engine::CutDivision>(clampValue(static_cast<int>(std::lround(value)), 0, 5)); break;
        case LockParameter::cutRate: controls.cutRateHz = clampValue(value, 0.25f, 64.0f); break;
        case LockParameter::cutTimeVariation: controls.cutTimeVariation = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::cutGate: controls.cutGate = clampValue(value / 100.0f, 0.05f, 1.0f); break;
        case LockParameter::cutJoin: controls.cutJoinMs = clampValue(value, 0.0f, 100.0f); break;
        case LockParameter::cutRegionCount: controls.cutRegionCount = clampValue(value, 1.0f, 64.0f); break;
        case LockParameter::cutRepeats: controls.cutRepeats = clampValue(value, 1.0f, 16.0f); break;
        case LockParameter::cutReverse: controls.cutReverseChance = clampValue(value / 100.0f, 0.0f, 1.0f); break;
        case LockParameter::cutPitchVariation: controls.cutPitchVariation = clampValue(value, 0.0f, 24.0f); break;
        case LockParameter::cutLevelVariation: controls.cutLevelVariation = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::cutLaneOrder: controls.cutLaneOrder = static_cast<Engine::CutLaneOrder>(clampValue(static_cast<int>(std::lround(value)), 0, 6)); break;
        case LockParameter::cutPolyPath: controls.cutPolyPath = static_cast<Engine::CutPolyPath>(clampValue(static_cast<int>(std::lround(value)), 0, 3)); break;
        case LockParameter::cutRegionMode: controls.cutRegionMode = static_cast<Engine::RegionMode>(clampValue(static_cast<int>(std::lround(value)), 0, 1)); break;
        case LockParameter::sampleMode: controls.sampleMode = static_cast<Engine::SampleMode>(clampValue(static_cast<int>(std::lround(value)), 0, 3)); break;
        case LockParameter::sampleRegionMode: controls.sampleRegionMode = static_cast<Engine::RegionMode>(clampValue(static_cast<int>(std::lround(value)), 0, 1)); break;
        case LockParameter::sampleRegionCount: controls.sampleRegionCount = clampValue(value, 1.0f, 64.0f); break;
        case LockParameter::motionRateBasis: controls.motionRateBasis = static_cast<Engine::MotionRateBasis>(clampValue(static_cast<int>(std::lround(value)), 0, 1)); break;
        case LockParameter::motionPathRate: controls.motionPathRate = clampValue(value, 0.01f, 80.0f); break;
        case LockParameter::motionStep: controls.motionStep = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::motionSeed: controls.motionSeed = clampValue(value, 1.0f, 9999.0f); break;
        case LockParameter::segmentTrigger: controls.segmentTrigger = static_cast<Engine::SegmentTrigger>(clampValue(static_cast<int>(std::lround(value)), 0, 2)); break;
        case LockParameter::segmentOverlap: controls.segmentOverlap = static_cast<Engine::SegmentOverlap>(clampValue(static_cast<int>(std::lround(value)), 0, 1)); break;
        case LockParameter::segmentLayers: controls.segmentLayerLimit = clampValue(value, 1.0f, 4.0f); break;
        case LockParameter::segmentRate: controls.segmentRateHz = clampValue(value, 0.25f, 80.0f); break;
        case LockParameter::segmentRepeats: controls.segmentRepeats = clampValue(value, 1.0f, 16.0f); break;
        case LockParameter::segmentStep: controls.segmentStep = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::segmentPitch: controls.segmentPitchScatter = clampValue(value, 0.0f, 12.0f); break;
        case LockParameter::segmentLevel: controls.segmentLevelVariation = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::segmentCurve: controls.segmentIntervalCurve = clampValue(value, -1.0f, 1.0f); break;
        case LockParameter::glitchMode: controls.glitchMode = static_cast<Engine::GlitchMode>(clampValue(static_cast<int>(std::lround(value)), 0, 4)); break;
        case LockParameter::glitchClock: controls.glitchClock = static_cast<Engine::CutClock>(clampValue(static_cast<int>(std::lround(value)), 0, 1)); break;
        case LockParameter::glitchDivision: controls.glitchDivision = static_cast<Engine::CutDivision>(clampValue(static_cast<int>(std::lround(value)), 0, 5)); break;
        case LockParameter::glitchRate: controls.glitchRateHz = clampValue(value, 0.25f, 64.0f); break;
        case LockParameter::glitchSlice: controls.glitchSliceMs = clampValue(value, 2.0f, 160.0f); break;
        case LockParameter::glitchRepeats: controls.glitchRepeats = clampValue(value, 1.0f, 16.0f); break;
        case LockParameter::glitchChance: controls.glitchChance = clampValue(value / 100.0f, 0.0f, 1.0f); break;
        case LockParameter::glitchMix: controls.glitchMix = clampValue(value / 100.0f, 0.0f, 1.0f); break;
        case LockParameter::glitchSteps: controls.glitchSteps = clampValue(value, 1.0f, 16.0f); break;
        case LockParameter::glitchPulses: controls.glitchPulses = clampValue(value, 0.0f, 16.0f); break;
        case LockParameter::glitchRotate: controls.glitchRotate = clampValue(value, 0.0f, 15.0f); break;
        case LockParameter::glitchJump: controls.glitchJumpMs = clampValue(value, 0.0f, 160.0f); break;
        case LockParameter::glitchPitch: controls.glitchPitch = clampValue(value, 0.0f, 24.0f); break;
        case LockParameter::glitchReverse: controls.glitchReverseChance = clampValue(value / 100.0f, 0.0f, 1.0f); break;
        case LockParameter::glitchDecay: controls.glitchDecay = clampValue(value / 100.0f, 0.0f, 1.0f); break;
        case LockParameter::glitchSeed: controls.glitchSeed = clampValue(value, 1.0f, 9999.0f); break;
        case LockParameter::effectTone: controls.effectTone = clampValue(value, -1.0f, 1.0f); break;
        case LockParameter::effectDrive: controls.effectDrive = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::effectBits: controls.effectBits = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::effectRate: controls.effectRate = clampValue(value, 1.0f, 16.0f); break;
        case LockParameter::effectDelay: controls.effectDelayMs = clampValue(value, 1.0f, 750.0f); break;
        case LockParameter::effectFeedback: controls.effectFeedback = clampValue(value / 100.0f, 0.0f, 0.9f); break;
        case LockParameter::effectWidth: controls.effectWidth = clampValue(value / 100.0f, 0.0f, 1.0f); break;
        case LockParameter::effectMix: controls.effectMix = clampValue(value / 100.0f, 0.0f, 1.0f); break;
        case LockParameter::count: break;
    }
}

float lockedValue(const Engine::Controls& controls, float outputGain,
                  std::size_t index)
{
    switch (static_cast<LockParameter>(index)) {
        case LockParameter::locus: return controls.locus;
        case LockParameter::field: return controls.field;
        case LockParameter::speed: return controls.speed;
        case LockParameter::manualLane: return controls.manualLane + 1.0f;
        case LockParameter::laneCycles: return controls.laneCycles;
        case LockParameter::algorithm: return static_cast<float>(controls.algorithm);
        case LockParameter::join: return controls.join;
        case LockParameter::outputGain: return outputGain;
        case LockParameter::motionMode: return static_cast<float>(controls.motion);
        case LockParameter::motionSound: return static_cast<float>(controls.motionSound);
        case LockParameter::motionRate: return controls.motionSoundRateHz;
        case LockParameter::lanesMode: return static_cast<float>(controls.lanes);
        case LockParameter::lanePath: return static_cast<float>(controls.lanePath);
        case LockParameter::lanesStretch: return controls.lanesStretch ? 1.0f : 0.0f;
        case LockParameter::stretchRatio: return controls.lanesStretchRatio;
        case LockParameter::lane1Gain: return controls.laneGains[0u];
        case LockParameter::lane2Gain: return controls.laneGains[1u];
        case LockParameter::lane3Gain: return controls.laneGains[2u];
        case LockParameter::lane4Gain: return controls.laneGains[3u];
        case LockParameter::grainMode: return static_cast<float>(controls.grains);
        case LockParameter::grainSize: return controls.grainSizeMs;
        case LockParameter::grainDensity: return controls.grainDensityHz;
        case LockParameter::grainPitch: return controls.grainPitch;
        case LockParameter::grainWindow: return static_cast<float>(controls.grainWindow);
        case LockParameter::grainSpread: return controls.grainStereoSpread;
        case LockParameter::cutupsMode: return static_cast<float>(controls.cutups);
        case LockParameter::motionTravel: return controls.motionTravel;
        case LockParameter::motionJitter: return controls.motionJitter;
        case LockParameter::packetDuty: return controls.packetDuty * 100.0f;
        case LockParameter::motorRate: return controls.motorRateHz;
        case LockParameter::motorSymmetry: return controls.motorSymmetry * 100.0f;
        case LockParameter::motorShape: return static_cast<float>(controls.motorShape);
        case LockParameter::laneBlend: return static_cast<float>(controls.laneBlend);
        case LockParameter::lanePathShape: return static_cast<float>(controls.lanePathShape);
        case LockParameter::lanePathOffset: return controls.lanePathOffset;
        case LockParameter::lanePathSkew: return controls.lanePathSkew;
        case LockParameter::lanePathCurve: return controls.lanePathCurve;
        case LockParameter::laneSlew: return controls.laneSlewMs;
        case LockParameter::grainSpray: return controls.grainSpray;
        case LockParameter::grainBias: return static_cast<float>(controls.grainBias);
        case LockParameter::grainPitchSpray: return controls.grainPitchSpray;
        case LockParameter::grainReverse: return controls.grainReverseChance * 100.0f;
        case LockParameter::grainSizeVariation: return controls.grainSizeVariation;
        case LockParameter::grainLevelVariation: return controls.grainLevelVariation;
        case LockParameter::grainTiming: return static_cast<float>(controls.grainTiming);
        case LockParameter::grainTimingScatter: return controls.grainTimingScatter;
        case LockParameter::grainEnvelopeSkew: return controls.grainEnvelopeSkew;
        case LockParameter::grainProcess: return static_cast<float>(controls.grainProcess);
        case LockParameter::grainProcessAmount: return controls.grainProcessAmount;
        case LockParameter::grainRegionCount: return controls.grainRegionCount;
        case LockParameter::grainAdvance: return static_cast<float>(controls.grainAdvance);
        case LockParameter::cutClock: return static_cast<float>(controls.cutClock);
        case LockParameter::cutDivision: return static_cast<float>(controls.cutDivision);
        case LockParameter::cutRate: return controls.cutRateHz;
        case LockParameter::cutTimeVariation: return controls.cutTimeVariation;
        case LockParameter::cutGate: return controls.cutGate * 100.0f;
        case LockParameter::cutJoin: return controls.cutJoinMs;
        case LockParameter::cutRegionCount: return controls.cutRegionCount;
        case LockParameter::cutRepeats: return controls.cutRepeats;
        case LockParameter::cutReverse: return controls.cutReverseChance * 100.0f;
        case LockParameter::cutPitchVariation: return controls.cutPitchVariation;
        case LockParameter::cutLevelVariation: return controls.cutLevelVariation;
        case LockParameter::cutLaneOrder: return static_cast<float>(controls.cutLaneOrder);
        case LockParameter::cutPolyPath: return static_cast<float>(controls.cutPolyPath);
        case LockParameter::cutRegionMode: return static_cast<float>(controls.cutRegionMode);
        case LockParameter::sampleMode: return static_cast<float>(controls.sampleMode);
        case LockParameter::sampleRegionMode: return static_cast<float>(controls.sampleRegionMode);
        case LockParameter::sampleRegionCount: return controls.sampleRegionCount;
        case LockParameter::motionRateBasis: return static_cast<float>(controls.motionRateBasis);
        case LockParameter::motionPathRate: return controls.motionPathRate;
        case LockParameter::motionStep: return controls.motionStep;
        case LockParameter::motionSeed: return controls.motionSeed;
        case LockParameter::segmentTrigger: return static_cast<float>(controls.segmentTrigger);
        case LockParameter::segmentOverlap: return static_cast<float>(controls.segmentOverlap);
        case LockParameter::segmentLayers: return controls.segmentLayerLimit;
        case LockParameter::segmentRate: return controls.segmentRateHz;
        case LockParameter::segmentRepeats: return controls.segmentRepeats;
        case LockParameter::segmentStep: return controls.segmentStep;
        case LockParameter::segmentPitch: return controls.segmentPitchScatter;
        case LockParameter::segmentLevel: return controls.segmentLevelVariation;
        case LockParameter::segmentCurve: return controls.segmentIntervalCurve;
        case LockParameter::glitchMode: return static_cast<float>(controls.glitchMode);
        case LockParameter::glitchClock: return static_cast<float>(controls.glitchClock);
        case LockParameter::glitchDivision: return static_cast<float>(controls.glitchDivision);
        case LockParameter::glitchRate: return controls.glitchRateHz;
        case LockParameter::glitchSlice: return controls.glitchSliceMs;
        case LockParameter::glitchRepeats: return controls.glitchRepeats;
        case LockParameter::glitchChance: return controls.glitchChance * 100.0f;
        case LockParameter::glitchMix: return controls.glitchMix * 100.0f;
        case LockParameter::glitchSteps: return controls.glitchSteps;
        case LockParameter::glitchPulses: return controls.glitchPulses;
        case LockParameter::glitchRotate: return controls.glitchRotate;
        case LockParameter::glitchJump: return controls.glitchJumpMs;
        case LockParameter::glitchPitch: return controls.glitchPitch;
        case LockParameter::glitchReverse: return controls.glitchReverseChance * 100.0f;
        case LockParameter::glitchDecay: return controls.glitchDecay * 100.0f;
        case LockParameter::glitchSeed: return controls.glitchSeed;
        case LockParameter::effectTone: return controls.effectTone;
        case LockParameter::effectDrive: return controls.effectDrive;
        case LockParameter::effectBits: return controls.effectBits;
        case LockParameter::effectRate: return controls.effectRate;
        case LockParameter::effectDelay: return controls.effectDelayMs;
        case LockParameter::effectFeedback: return controls.effectFeedback * 100.0f;
        case LockParameter::effectWidth: return controls.effectWidth * 100.0f;
        case LockParameter::effectMix: return controls.effectMix * 100.0f;
        case LockParameter::count: return 0.0f;
    }
    return 0.0f;
}

std::uint16_t memoryU16(const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t>(bytes[0])
        | static_cast<std::uint16_t>(bytes[1]) << 8u;
}

std::uint32_t memoryU32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0])
        | static_cast<std::uint32_t>(bytes[1]) << 8u
        | static_cast<std::uint32_t>(bytes[2]) << 16u
        | static_cast<std::uint32_t>(bytes[3]) << 24u;
}

int copyText(const char* text, char* buffer, int bufferLength)
{
    if (!text || !buffer || bufferLength < 1) return -1;
    const std::size_t length = std::strlen(text);
    if (length >= static_cast<std::size_t>(bufferLength)) return -1;
    std::memcpy(buffer, text, length + 1u);
    return static_cast<int>(length);
}

bool writeBytes(std::FILE* file, const void* data, std::size_t bytes)
{
    return file && data && std::fwrite(data, 1u, bytes, file) == bytes;
}

bool writeU16(std::FILE* file, std::uint16_t value)
{
    const std::uint8_t bytes[2] {
        static_cast<std::uint8_t>(value & 0xffu),
        static_cast<std::uint8_t>((value >> 8u) & 0xffu),
    };
    return writeBytes(file, bytes, sizeof(bytes));
}

bool writeU32(std::FILE* file, std::uint32_t value)
{
    const std::uint8_t bytes[4] {
        static_cast<std::uint8_t>(value & 0xffu),
        static_cast<std::uint8_t>((value >> 8u) & 0xffu),
        static_cast<std::uint8_t>((value >> 16u) & 0xffu),
        static_cast<std::uint8_t>((value >> 24u) & 0xffu),
    };
    return writeBytes(file, bytes, sizeof(bytes));
}

bool writeU64(std::FILE* file, std::uint64_t value)
{
    return writeU32(file, static_cast<std::uint32_t>(value & 0xffffffffu))
        && writeU32(file, static_cast<std::uint32_t>(value >> 32u));
}

bool readU16(std::FILE* file, std::uint16_t& value)
{
    std::uint8_t bytes[2] {};
    if (!file || std::fread(bytes, 1u, sizeof(bytes), file) != sizeof(bytes)) return false;
    value = static_cast<std::uint16_t>(bytes[0])
        | static_cast<std::uint16_t>(bytes[1]) << 8u;
    return true;
}

bool readU32(std::FILE* file, std::uint32_t& value)
{
    std::uint8_t bytes[4] {};
    if (!file || std::fread(bytes, 1u, sizeof(bytes), file) != sizeof(bytes)) return false;
    value = static_cast<std::uint32_t>(bytes[0])
        | static_cast<std::uint32_t>(bytes[1]) << 8u
        | static_cast<std::uint32_t>(bytes[2]) << 16u
        | static_cast<std::uint32_t>(bytes[3]) << 24u;
    return true;
}

bool readU64(std::FILE* file, std::uint64_t& value)
{
    std::uint32_t low = 0u;
    std::uint32_t high = 0u;
    if (!readU32(file, low) || !readU32(file, high)) return false;
    value = static_cast<std::uint64_t>(low)
        | static_cast<std::uint64_t>(high) << 32u;
    return true;
}

bool checksummedPayloadSize(const char* path, long& payloadSize)
{
    payloadSize = 0l;
    if (!path) return false;
    std::FILE* file = std::fopen(path, "rb");
    if (!file || std::fseek(file, 0l, SEEK_END) != 0) {
        if (file) std::fclose(file);
        return false;
    }
    const long totalSize = std::ftell(file);
    if (totalSize < 4l || std::fseek(file, totalSize - 4l, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    std::uint32_t expected = 0u;
    if (!readU32(file, expected) || std::fseek(file, 0l, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    std::uint32_t checksum = 2166136261u;
    std::array<std::uint8_t, 4096u> bytes {};
    long remaining = totalSize - 4l;
    while (remaining > 0l) {
        const std::size_t request = std::min<std::size_t>(bytes.size(),
            static_cast<std::size_t>(remaining));
        if (std::fread(bytes.data(), 1u, request, file) != request) {
            std::fclose(file);
            return false;
        }
        for (std::size_t index = 0u; index < request; ++index) {
            checksum ^= bytes[index];
            checksum *= 16777619u;
        }
        remaining -= static_cast<long>(request);
    }
    std::fclose(file);
    if (checksum != expected) return false;
    payloadSize = totalSize - 4l;
    return true;
}

bool appendFileChecksum(const char* path)
{
    if (!path) return false;
    std::FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    std::uint32_t checksum = 2166136261u;
    std::array<std::uint8_t, 4096u> bytes {};
    std::size_t count = 0u;
    while ((count = std::fread(bytes.data(), 1u, bytes.size(), file)) != 0u) {
        for (std::size_t index = 0u; index < count; ++index) {
            checksum ^= bytes[index];
            checksum *= 16777619u;
        }
    }
    const bool readOk = std::feof(file) != 0;
    std::fclose(file);
    if (!readOk) return false;
    file = std::fopen(path, "ab");
    if (!file) return false;
    const bool wrote = writeU32(file, checksum);
    return std::fclose(file) == 0 && wrote;
}

bool replaceFileWithBackup(const char* temporary, const char* path)
{
    if (!temporary || !path) return false;
    std::array<char, kPersistencePathSize> backup {};
    const int length = std::snprintf(backup.data(), backup.size(), "%s.bak", path);
    if (length <= 0 || static_cast<std::size_t>(length) >= backup.size()) {
        return false;
    }
    std::remove(backup.data());
    const bool hadOriginal = access(path, F_OK) == 0;
    if (hadOriginal && std::rename(path, backup.data()) != 0) return false;
    if (std::rename(temporary, path) == 0) return true;
    if (hadOriginal) std::rename(backup.data(), path);
    return false;
}

bool writeF32(std::FILE* file, float value)
{
    static_assert(sizeof(float) == sizeof(std::uint32_t),
                  "clip storage requires 32-bit floats");
    std::uint32_t bits = 0u;
    std::memcpy(&bits, &value, sizeof(bits));
    return writeU32(file, bits);
}

bool readF32(std::FILE* file, float& value)
{
    std::uint32_t bits = 0u;
    if (!readU32(file, bits)) return false;
    std::memcpy(&value, &bits, sizeof(value));
    return std::isfinite(value);
}

bool jsonNumber(const char* json, const char* key, float& result)
{
    if (!json || !key) return false;
    char clip[64] {};
    const int keyLength = std::snprintf(clip, sizeof(clip), "\"%s\"", key);
    if (keyLength <= 0 || keyLength >= static_cast<int>(sizeof(clip))) return false;
    const char* found = std::strstr(json, clip);
    if (!found) return false;
    found = std::strchr(found + keyLength, ':');
    if (!found) return false;
    char* end = nullptr;
    const float parsed = std::strtof(found + 1, &end);
    if (end == found + 1 || !std::isfinite(parsed)) return false;
    result = parsed;
    return true;
}

bool jsonUnsigned64(const char* json, const char* key, std::uint64_t& result)
{
    if (!json || !key) return false;
    char clip[64] {};
    const int keyLength = std::snprintf(clip, sizeof(clip), "\"%s\"", key);
    if (keyLength <= 0 || keyLength >= static_cast<int>(sizeof(clip))) return false;
    const char* found = std::strstr(json, clip);
    if (!found) return false;
    found = std::strchr(found + keyLength, ':');
    if (!found) return false;
    const char* number = found + 1;
    while (*number == ' ' || *number == '\t' || *number == '\r'
           || *number == '\n') ++number;
    if (*number == '-') return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(number, &end, 10);
    if (end == number || errno == ERANGE) return false;
    result = static_cast<std::uint64_t>(parsed);
    return true;
}

float softLimit(float sample)
{
    constexpr float knee = 0.90f;
    constexpr float limit = 0.999969f;
    const float magnitude = std::abs(sample);
    if (magnitude <= knee) return sample;
    const float headroom = limit - knee;
    const float over = magnitude - knee;
    const float shaped = knee + headroom * over / (over + headroom);
    return std::copysign(shaped, sample);
}

int16_t toInt16(float sample)
{
    const float limited = softLimit(sample);
    const float scaled = limited * 32767.0f;
    return static_cast<int16_t>(clampValue(std::lrintf(scaled), -32768l, 32767l));
}

struct PlaybackVoice {
    Engine engine;
    Engine::Controls snapshotControls;
    std::array<std::array<float, kBlockFrames>, kChannelCount> output {};
    std::array<float*, kChannelCount> outputPointers {};
    int note = -1;
    int sourcePad = -1;
    int velocity = 100;
    std::size_t variantLane = 0u;
    std::array<std::uint32_t, kLaneCount> regionRevisions {};
    std::uint32_t age = 0u;
    float laneOffset = 0.0f;
    float envelope = 0.0f;
    float target = 0.0f;
    float snapshotOutputGain = 1.0f;
    int clipNoteIndex = -1;
    bool active = false;
    bool held = false;
    bool clipOwned = false;
    bool useSnapshot = false;
};

struct ThruInstance {
    Engine engine;
    std::array<PlaybackVoice, kVoiceCount> voices {};
    Engine::Controls controls;
    std::unique_ptr<float[]> storage;
    std::unique_ptr<float[]> preparedClipStorage;
    std::size_t laneCapacityFrames = 0u;
    std::size_t planeCapacityFrames = 0u;
    float sampleRate = static_cast<float>(MOVE_SAMPLE_RATE);

    std::array<std::array<float, kBlockFrames>, kChannelCount> input {};
    std::array<std::array<float, kBlockFrames>, kChannelCount> output {};
    std::array<std::array<float, kBlockFrames>, kChannelCount> silentInput {};
    std::array<std::array<float, kBlockFrames>, 2u> monitorInput {};
    std::array<std::array<float, kBlockFrames>, 2u> voiceMix {};
    std::array<const float*, kChannelCount> inputPointers {};
    std::array<const float*, kChannelCount> silentInputPointers {};
    std::array<float*, kChannelCount> outputPointers {};
    std::unique_ptr<float[]> effectDelayBuffer;
    std::size_t effectDelayCapacity = 0u;
    std::size_t effectDelayWrite = 0u;
    std::array<float, 2u> effectToneState {};
    std::array<float, 2u> effectRateHold {};
    std::size_t effectRateCountdown = 0u;

    std::array<int, 32u> padMappedNotes {};
    std::uint32_t noteClock = 0u;
    int activeNote = -1;
    int rootNote = 60;
    int padOctave = 0;
    int pressureMode = 1;
    PadMode padMode = PadMode::keys;
    LaneVariation laneVariation = LaneVariation::manual;
    std::uint32_t variationCounter = 0u;
    int lastVariationLane = -1;
    float performanceLane = 0.0f;
    bool stackScanPaused = false;
    float attackCoefficient = 1.0f;
    float releaseCoefficient = 1.0f;

    std::size_t selectedLane = 0u;
    int recordSource = 0;
    float recordGain = 1.0f;
    float monitorLevel = 0.0f;
    float outputGain = 1.0f;
    float previewLevel = 0.7f;
    float sampleTrimStart = 0.0f;
    float sampleTrimEnd = 1.0f;

    std::array<ClipSlot, kClipSlotCount> clipSlots {};
    std::array<ClipChainEntry, kClipChainLength> clipChain {};
    Engine::Controls clipControls;
    float clipOutputGain = 1.0f;
    double clipPositionMs = 0.0;
    double clipAbsolutePositionMs = 0.0;
    std::atomic<std::uint64_t> renderClockSamples { 0u };
    std::uint64_t clipRecordStartSample = 0u;
    std::array<int, 160u> clipOpenNoteIndices {};
    std::array<double, 160u> clipOpenNoteStarts {};
    std::array<std::uint32_t, 32u> clipLastPressureTimes {};
    std::array<float, 32u> clipLastPressureValues {};
    std::array<int, kClipCurveLaneCount> clipLastPointIndices {};
    std::uint16_t clipRecordStartNoteCount = 0u;
    std::uint16_t clipRecordStartPointCount = 0u;
    std::uint8_t clipRecordingTake = 0u;
    ClipRecordMode clipRecordMode = ClipRecordMode::replace;
    bool clipRecording = false;
    bool clipReplacing = true;
    bool clipFinishPending = false;
    bool clipNeedsStart = false;
    float tempoBpm = 120.0f;
    float tempoSwing = 0.5f;
    LaunchQuantize launchQuantize = LaunchQuantize::off;
    double transportBeats = 0.0;
    bool clipLaunchPending = false;
    std::size_t grainVoiceLimit = 8u;
    bool clipPlaying = false;
    bool chainEnabled = false;
    int chainPosition = 0;
    int chainRepeat = 0;
    int chainEditSlot = 0;
    int chainNextSlot = -1;
    std::uint32_t performanceRandom = 0x53334754u;
    int activeClip = 0;
    int queuedClip = -1;
    bool activeClipOccupied = false;
    bool clipBankInitialized = false;
    ClipSlot undoClip {};
    int undoClipIndex = -1;
    bool undoAvailable = false;
    bool persistentStateDirty = false;
    std::uint32_t pendingCaptureSaveMask = 0u;
    std::uint32_t pendingRegionAnalysisMask = 0u;
    std::array<int, kLaneCount> pendingCaptureClips { -1, -1, -1, -1 };
    std::atomic<std::uint64_t> maximumRenderMicros { 0u };
    std::atomic<std::uint64_t> renderOverruns { 0u };

    std::unique_ptr<float[]> sampleStage;
    pthread_t sampleWorker {};
    pthread_mutex_t sampleWorkerMutex {};
    pthread_cond_t sampleWorkerCondition {};
    std::atomic<int> sampleWorkerRunning { 0 };
    std::atomic<int> sampleLoadState {
        static_cast<int>(SampleLoadState::idle)
    };
    std::atomic<int> clipPrepareState {
        static_cast<int>(ClipPrepareState::idle)
    };
    std::atomic<int> clipPrepareRequest { -1 };
    std::atomic<int> preparedClip { -1 };
    std::array<std::size_t, kLaneCount> preparedLaneFrames {};
    std::array<float, kLaneCount> preparedLaneRates {};
    std::array<LaneSourceKind, kLaneCount> preparedLaneKinds {
        LaneSourceKind::empty, LaneSourceKind::empty,
        LaneSourceKind::empty, LaneSourceKind::empty
    };
    std::array<RegionMap, kLaneCount> activeRegions {};
    std::array<RegionMap, kLaneCount> preparedRegions {};
    RegionMap sampleStageRegions {};
    std::array<WaveBins, kLaneCount> preparedWaveMinimum {};
    std::array<WaveBins, kLaneCount> preparedWaveMaximum {};
    WaveBins sampleStageWaveMinimum {};
    WaveBins sampleStageWaveMaximum {};
    std::array<std::uint32_t, kLaneCount> laneRegionRevisions {};
    std::array<std::array<char, kPersistencePathSize>, kLaneCount>
        preparedLanePaths {};
    std::array<std::array<char, 64u>, kLaneCount> preparedLaneLabels {};
    std::array<char, kPersistencePathSize> sampleRequestPath {};
    std::array<char, 96u> sampleLoadedName {};
    std::array<char, 128u> sampleLoadError {};
    int sampleRequestLane = 0;
    int sampleRequestClip = 0;
    bool sampleRequestFromClip = false;
    bool sampleRequestPreview = false;
    bool samplePreviewCancelled = false;
    int sampleApplyLane = 0;
    std::size_t sampleStageFrames = 0u;
    float sampleStageRate = static_cast<float>(MOVE_SAMPLE_RATE);
    std::size_t sampleCopyFrame = 0u;
    bool sampleRemovalRequested = false;
    std::size_t previewFrame = 0u;
    bool previewActive = false;
    bool sampleWorkerReady = false;
    bool sampleWorkerSignaled = false;
    std::array<std::array<char, 64u>, kLaneCount> laneSourceLabels {};
    std::array<std::array<char, kPersistencePathSize>, kLaneCount>
        currentLaneSourcePaths {};
    std::array<LaneSourceKind, kLaneCount> currentLaneSourceKinds {
        LaneSourceKind::empty, LaneSourceKind::empty,
        LaneSourceKind::empty, LaneSourceKind::empty
    };
    std::array<std::array<char, kPersistencePathSize>, kLaneCount>
        pendingClipSourcePaths {};
    std::uint32_t pendingClipSourceMask = 0u;

    ~ThruInstance() { stopSampleWorker(); }

    std::array<WaveBins, kLaneCount> waveMinimum {};
    std::array<WaveBins, kLaneCount> waveMaximum {};
    std::array<float, kWaveBins> scanMinimum {};
    std::array<float, kWaveBins> scanMaximum {};
    std::uint32_t waveDirtyMask = 0u;
    int scanLane = -1;
    std::size_t scanFrame = 0u;
    std::size_t scanFrames = 0u;
    bool waveTextDirty = false;
    std::array<char, kWaveTextSize> waveText {};
    std::array<char, 160u> lastError {};
    std::array<char, kPersistencePathSize> moduleDirectory {};
    std::uint32_t pendingClearMask = 0u;
    std::uint32_t recordingLaneMask = 0u;
    bool persistenceEnabled = false;
    bool persistenceRestored = false;
    std::uint64_t persistenceGeneration = 0u;

    bool prepare(const char* moduleDir)
    {
        const int hostRate = g_host && g_host->sample_rate > 0
            ? g_host->sample_rate : MOVE_SAMPLE_RATE;
        sampleRate = static_cast<float>(hostRate);
        laneCapacityFrames = static_cast<std::size_t>(hostRate) * kLaneSeconds;
        planeCapacityFrames = laneCapacityFrames * Engine::kLaneChannels;
        const std::size_t totalFloats = planeCapacityFrames * kChannelCount;
        storage.reset(new (std::nothrow) float[totalFloats] {});
        preparedClipStorage.reset(new (std::nothrow) float[totalFloats] {});
        sampleStage.reset(new (std::nothrow) float[planeCapacityFrames] {});
        effectDelayCapacity = static_cast<std::size_t>(hostRate) + 1u;
        effectDelayBuffer.reset(new (std::nothrow)
            float[effectDelayCapacity * 2u] {});
        if (!storage || !preparedClipStorage || !sampleStage
            || !effectDelayBuffer) {
            setError("Unable to allocate active and prepared sample banks");
            return false;
        }
        if (!engine.prepare(sampleRate,
                            storage.get() + planeCapacityFrames * 0u,
                            storage.get() + planeCapacityFrames * 1u,
                            storage.get() + planeCapacityFrames * 2u,
                            storage.get() + planeCapacityFrames * 3u,
                            planeCapacityFrames)) {
            setError("Sample Motion engine preparation failed");
            return false;
        }
        controls.mix = 1.0f;
        controls.join = 1.0f;
        controls.lanePath = Engine::LanePath::triangle;
        controls.recordInputGain = 1.0f;
        for (auto& label : laneSourceLabels) {
            std::snprintf(label.data(), label.size(), "EMPTY");
        }
        padMappedNotes.fill(-1);
        clipOpenNoteIndices.fill(-1);
        clipLastPressureTimes.fill(0xffffffffu);
        clipLastPressureValues.fill(-1.0f);
        clipLastPointIndices.fill(-1);
        clipControls = controls;
        clipOutputGain = outputGain;
        attackCoefficient = 1.0f - std::exp(-1.0f / (sampleRate * 0.003f));
        releaseCoefficient = 1.0f - std::exp(-1.0f / (sampleRate * 0.025f));
        for (std::size_t channel = 0u; channel < kChannelCount; ++channel) {
            inputPointers[channel] = input[channel].data();
            silentInputPointers[channel] = silentInput[channel].data();
            outputPointers[channel] = output[channel].data();
        }
        if (moduleDir && moduleDir[0] != '\0'
            && std::strlen(moduleDir) + 32u < moduleDirectory.size()) {
            std::snprintf(moduleDirectory.data(), moduleDirectory.size(), "%s", moduleDir);
            persistenceEnabled = true;
            std::array<char, kPersistencePathSize> capturesDirectory {};
            if (persistencePath("captures", capturesDirectory.data(),
                                capturesDirectory.size())) {
                if (mkdir(capturesDirectory.data(), 0700) != 0
                    && errno != EEXIST) {
                    persistenceEnabled = false;
                    setError("Unable to create the capture directory");
                }
            }
            loadPersistentState();
        }
        if (!clipBankInitialized) captureActiveClip(false);
        clipControls = controls;
        clipOutputGain = outputGain;
        for (auto& voice : voices) {
            if (!voice.engine.prepare(sampleRate,
                                      storage.get() + planeCapacityFrames * 0u,
                                      storage.get() + planeCapacityFrames * 1u,
                                      storage.get() + planeCapacityFrames * 2u,
                                      storage.get() + planeCapacityFrames * 3u,
                                      planeCapacityFrames)) {
                setError("Playback voice preparation failed");
                return false;
            }
            for (std::size_t channel = 0u; channel < kChannelCount; ++channel) {
                voice.outputPointers[channel] = voice.output[channel].data();
            }
            for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                if (engine.laneLoaded(lane)) {
                    voice.engine.adoptLane(
                        lane, engine.laneFrames(lane), engine.laneSampleRate(lane));
                }
            }
            voice.engine.stopPlayback();
        }
        sampleWorkerReady = startSampleWorker();
        if (!sampleWorkerReady && g_host && g_host->log) {
            g_host->log("s3g Thru sample-library worker unavailable");
        }
        rebuildWaveText();
        return true;
    }

    void setError(const char* message)
    {
        std::snprintf(lastError.data(), lastError.size(), "%s", message ? message : "");
    }

    void processEffects(std::size_t frames, const Engine::Controls& fx)
    {
        const float mix = clampValue(fx.effectMix, 0.0f, 1.0f);
        if (!effectDelayBuffer || effectDelayCapacity < 2u) return;
        const float tone = clampValue(fx.effectTone, -1.0f, 1.0f);
        const float cutoff = tone < 0.0f
            ? 18000.0f * std::pow(80.0f / 18000.0f, -tone)
            : 20.0f * std::pow(8000.0f / 20.0f, tone);
        const float alpha = 1.0f - std::exp(-6.28318530718f
            * clampValue(cutoff, 20.0f, sampleRate * 0.45f) / sampleRate);
        const float drive = clampValue(fx.effectDrive, 0.0f, 1.0f) * 12.0f;
        const float bitsAmount = clampValue(fx.effectBits, 0.0f, 1.0f);
        const float quantization = std::pow(2.0f,
            15.0f - bitsAmount * 12.0f);
        const std::size_t holdFrames = static_cast<std::size_t>(clampValue(
            std::round(fx.effectRate), 1.0f, 16.0f));
        const std::size_t delayFrames = std::min<std::size_t>(
            effectDelayCapacity - 1u,
            std::max<std::size_t>(1u, static_cast<std::size_t>(sampleRate
                * clampValue(fx.effectDelayMs, 1.0f, 750.0f) * 0.001f)));
        const float feedback = clampValue(fx.effectFeedback, 0.0f, 0.9f);
        const float width = clampValue(fx.effectWidth, 0.0f, 1.0f);
        for (std::size_t frame = 0u; frame < frames; ++frame) {
            float colored[2u] {};
            for (std::size_t channel = 0u; channel < 2u; ++channel) {
                const float inputSample = voiceMix[channel][frame];
                effectToneState[channel] += alpha
                    * (inputSample - effectToneState[channel]);
                float sample = tone < 0.0f ? effectToneState[channel]
                    : tone > 0.0f ? inputSample - effectToneState[channel]
                    : inputSample;
                sample = sample * (1.0f + drive)
                    / (1.0f + drive * std::abs(sample));
                colored[channel] = sample;
            }
            if (effectRateCountdown == 0u) {
                for (std::size_t channel = 0u; channel < 2u; ++channel) {
                    effectRateHold[channel] = bitsAmount > 1.0e-5f
                        ? std::round(colored[channel] * quantization)
                            / quantization
                        : colored[channel];
                }
                effectRateCountdown = holdFrames;
            }
            --effectRateCountdown;
            const std::size_t read = (effectDelayWrite
                + effectDelayCapacity - delayFrames) % effectDelayCapacity;
            const float delayed[2u] {
                effectDelayBuffer[read],
                effectDelayBuffer[effectDelayCapacity + read],
            };
            for (std::size_t channel = 0u; channel < 2u; ++channel) {
                const float feedbackSample = delayed[channel] * (1.0f - width)
                    + delayed[1u - channel] * width;
                effectDelayBuffer[channel * effectDelayCapacity
                    + effectDelayWrite] = softLimit(
                        effectRateHold[channel] + feedbackSample * feedback);
                voiceMix[channel][frame] = effectRateHold[channel]
                    + (delayed[channel] - effectRateHold[channel]) * mix;
            }
            effectDelayWrite = (effectDelayWrite + 1u) % effectDelayCapacity;
        }
    }

    void clearError() { lastError[0] = '\0'; }

    void setSampleLoadError(const char* message)
    {
        std::snprintf(sampleLoadError.data(), sampleLoadError.size(), "%s",
                      message ? message : "Sample load failed");
        sampleLoadState.store(static_cast<int>(SampleLoadState::error),
                              std::memory_order_release);
    }

    static float decodeWavSample(const std::uint8_t* data, std::size_t frame,
                                 int channel, int channels, int format,
                                 int bits, int blockAlign)
    {
        const int sourceChannel = channels == 1 ? 0 : channel;
        const int bytesPerSample = bits / 8;
        const std::uint8_t* sample = data + frame * static_cast<std::size_t>(blockAlign)
            + static_cast<std::size_t>(sourceChannel * bytesPerSample);
        if (format == 3 && bits == 32) {
            float value = 0.0f;
            std::memcpy(&value, sample, sizeof(value));
            return std::isfinite(value) ? clampValue(value, -1.0f, 1.0f) : 0.0f;
        }
        if (format != 1) return 0.0f;
        if (bits == 16) {
            const auto value = static_cast<std::int16_t>(memoryU16(sample));
            return static_cast<float>(value) / 32768.0f;
        }
        if (bits == 24) {
            std::int32_t value = static_cast<std::int32_t>(sample[0])
                | static_cast<std::int32_t>(sample[1]) << 8
                | static_cast<std::int32_t>(sample[2]) << 16;
            if ((value & 0x00800000) != 0) value |= static_cast<std::int32_t>(0xff000000u);
            return static_cast<float>(value) / 8388608.0f;
        }
        if (bits == 32) {
            const auto value = static_cast<std::int32_t>(memoryU32(sample));
            return static_cast<float>(value) / 2147483648.0f;
        }
        return 0.0f;
    }

    bool decodeLibraryWav(const char* requestedPath, float* destinationLeft,
                          float* destinationRight, std::size_t& decodedFrames,
                          float& decodedRate, char* decodedName,
                          std::size_t decodedNameSize)
    {
        if (!requestedPath || requestedPath[0] == '\0' || !destinationLeft
            || !destinationRight || !decodedName || decodedNameSize == 0u) {
            setSampleLoadError("No sample was selected");
            return false;
        }
        std::array<char, PATH_MAX> resolved {};
        const bool resolvedPath = realpath(requestedPath, resolved.data()) != nullptr;
        const bool inUserLibrary = resolvedPath
            && std::strncmp(resolved.data(), kSampleLibraryRoot,
                            std::strlen(kSampleLibraryRoot)) == 0;
        std::array<char, PATH_MAX> localRoot {};
        std::array<char, PATH_MAX> captureRoot {};
        bool inModuleLibrary = false;
        bool inModuleCaptures = false;
        if (resolvedPath && moduleDirectory[0] != '\0') {
            std::array<char, PATH_MAX> requestedLocalRoot {};
            std::snprintf(requestedLocalRoot.data(), requestedLocalRoot.size(),
                          "%s/library", moduleDirectory.data());
            if (realpath(requestedLocalRoot.data(), localRoot.data())) {
                const std::size_t length = std::strlen(localRoot.data());
                if (length + 1u < localRoot.size()) {
                    localRoot[length] = '/';
                    localRoot[length + 1u] = '\0';
                    inModuleLibrary = std::strncmp(
                        resolved.data(), localRoot.data(), length + 1u) == 0;
                }
            }
            std::snprintf(requestedLocalRoot.data(), requestedLocalRoot.size(),
                          "%s/captures", moduleDirectory.data());
            if (realpath(requestedLocalRoot.data(), captureRoot.data())) {
                const std::size_t length = std::strlen(captureRoot.data());
                if (length + 1u < captureRoot.size()) {
                    captureRoot[length] = '/';
                    captureRoot[length + 1u] = '\0';
                    inModuleCaptures = std::strncmp(
                        resolved.data(), captureRoot.data(), length + 1u) == 0;
                }
            }
        }
        if (!resolvedPath
            || (!inUserLibrary && !inModuleLibrary && !inModuleCaptures)) {
            setSampleLoadError("Sample must be in UserLibrary or Thru captures");
            return false;
        }
        const int fd = open(resolved.data(), O_RDONLY);
        if (fd < 0) {
            setSampleLoadError("Unable to open sample");
            return false;
        }
        struct stat status {};
        if (fstat(fd, &status) != 0 || status.st_size < 44) {
            close(fd);
            setSampleLoadError("Sample is not a valid WAV file");
            return false;
        }
        const std::size_t mapSize = static_cast<std::size_t>(status.st_size);
        void* mapping = mmap(nullptr, mapSize, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapping == MAP_FAILED) {
            close(fd);
            setSampleLoadError("Unable to map sample");
            return false;
        }
        const auto* bytes = static_cast<const std::uint8_t*>(mapping);
        bool ok = std::memcmp(bytes, "RIFF", 4u) == 0
            && std::memcmp(bytes + 8u, "WAVE", 4u) == 0;
        int format = 0;
        int channels = 0;
        int bits = 0;
        int blockAlign = 0;
        std::uint32_t sourceRate = 0u;
        const std::uint8_t* audio = nullptr;
        std::size_t audioBytes = 0u;
        for (std::size_t offset = 12u; ok && offset + 8u <= mapSize;) {
            const std::uint32_t chunkSize = memoryU32(bytes + offset + 4u);
            const std::size_t content = offset + 8u;
            if (content > mapSize || chunkSize > mapSize - content) {
                ok = false;
                break;
            }
            if (std::memcmp(bytes + offset, "fmt ", 4u) == 0 && chunkSize >= 16u) {
                format = memoryU16(bytes + content);
                channels = memoryU16(bytes + content + 2u);
                sourceRate = memoryU32(bytes + content + 4u);
                blockAlign = memoryU16(bytes + content + 12u);
                bits = memoryU16(bytes + content + 14u);
                if (format == 0xfffe && chunkSize >= 40u) {
                    format = memoryU16(bytes + content + 24u);
                }
            } else if (std::memcmp(bytes + offset, "data", 4u) == 0) {
                audio = bytes + content;
                audioBytes = chunkSize;
            }
            offset = content + chunkSize + (chunkSize & 1u);
        }
        const bool supported = ok && audio && audioBytes != 0u
            && (channels == 1 || channels == 2)
            && sourceRate >= 8000u && sourceRate <= 192000u
            && blockAlign > 0
            && ((format == 1 && (bits == 16 || bits == 24 || bits == 32))
                || (format == 3 && bits == 32));
        if (!supported) {
            munmap(mapping, mapSize);
            close(fd);
            setSampleLoadError("Use mono/stereo PCM or float WAV");
            return false;
        }
        const std::size_t sourceFrames = audioBytes
            / static_cast<std::size_t>(blockAlign);
        const double ratio = static_cast<double>(sampleRate)
            / static_cast<double>(sourceRate);
        const std::size_t outputFrames = std::min(laneCapacityFrames,
            static_cast<std::size_t>(std::floor(sourceFrames * ratio)));
        if (outputFrames == 0u) {
            munmap(mapping, mapSize);
            close(fd);
            setSampleLoadError("Sample contains no audio");
            return false;
        }
        float* left = destinationLeft;
        float* right = destinationRight;
        for (std::size_t frame = 0u; frame < outputFrames; ++frame) {
            if ((frame & 4095u) == 0u
                && sampleWorkerRunning.load(std::memory_order_relaxed) == 0) {
                ok = false;
                break;
            }
            const double sourcePosition = static_cast<double>(frame) / ratio;
            const std::size_t first = std::min(sourceFrames - 1u,
                static_cast<std::size_t>(sourcePosition));
            const std::size_t second = std::min(sourceFrames - 1u, first + 1u);
            const float mix = static_cast<float>(sourcePosition - first);
            const float l1 = decodeWavSample(audio, first, 0, channels,
                                             format, bits, blockAlign);
            const float l2 = decodeWavSample(audio, second, 0, channels,
                                             format, bits, blockAlign);
            const float r1 = decodeWavSample(audio, first, 1, channels,
                                             format, bits, blockAlign);
            const float r2 = decodeWavSample(audio, second, 1, channels,
                                             format, bits, blockAlign);
            left[frame] = l1 + (l2 - l1) * mix;
            right[frame] = r1 + (r2 - r1) * mix;
        }
        munmap(mapping, mapSize);
        close(fd);
        if (!ok) return false;
        decodedFrames = outputFrames;
        decodedRate = sampleRate;
        const char* leaf = std::strrchr(resolved.data(), '/');
        std::snprintf(decodedName, decodedNameSize, "%.*s",
                      static_cast<int>(decodedNameSize - 1u),
                      leaf ? leaf + 1 : resolved.data());
        return true;
    }

    bool loadLibraryWav(const char* requestedPath)
    {
        const bool loaded = decodeLibraryWav(
            requestedPath, sampleStage.get(),
            sampleStage.get() + laneCapacityFrames, sampleStageFrames,
            sampleStageRate, sampleLoadedName.data(), sampleLoadedName.size());
        if (loaded) {
            analyzeRegions(sampleStage.get(),
                           sampleStage.get() + laneCapacityFrames,
                           sampleStageFrames, sampleStageRate,
                           sampleStageRegions);
            analyzeWaveform(sampleStage.get(),
                            sampleStage.get() + laneCapacityFrames,
                            sampleStageFrames, sampleStageWaveMinimum,
                            sampleStageWaveMaximum);
        }
        return loaded;
    }

    static void quantizeWaveform(
        const std::array<float, kWaveBins>& minimum,
        const std::array<float, kWaveBins>& maximum,
        WaveBins& encodedMinimum, WaveBins& encodedMaximum)
    {
        float peak = 0.0f;
        for (std::size_t bin = 0u; bin < kWaveBins; ++bin) {
            peak = std::max(peak, std::fabs(minimum[bin]));
            peak = std::max(peak, std::fabs(maximum[bin]));
        }
        if (peak < 1.0e-5f) {
            encodedMinimum.fill(0);
            encodedMaximum.fill(0);
            return;
        }
        const float scale = 7.0f / peak;
        for (std::size_t bin = 0u; bin < kWaveBins; ++bin) {
            const float low = std::min(0.0f, minimum[bin]);
            const float high = std::max(0.0f, maximum[bin]);
            encodedMinimum[bin] = static_cast<std::int8_t>(std::lrintf(
                clampValue(low * scale, -7.0f, 7.0f)));
            encodedMaximum[bin] = static_cast<std::int8_t>(std::lrintf(
                clampValue(high * scale, -7.0f, 7.0f)));
        }
    }

    static void analyzeWaveform(const float* left, const float* right,
                                std::size_t frames,
                                WaveBins& minimum, WaveBins& maximum)
    {
        if (!left || !right || frames == 0u) {
            minimum.fill(0);
            maximum.fill(0);
            return;
        }
        std::array<float, kWaveBins> rawMinimum {};
        std::array<float, kWaveBins> rawMaximum {};
        for (std::size_t frame = 0u; frame < frames; ++frame) {
            const std::size_t bin = std::min(kWaveBins - 1u,
                frame * kWaveBins / frames);
            rawMinimum[bin] = std::min(rawMinimum[bin],
                std::min(left[frame], right[frame]));
            rawMaximum[bin] = std::max(rawMaximum[bin],
                std::max(left[frame], right[frame]));
        }
        quantizeWaveform(rawMinimum, rawMaximum, minimum, maximum);
    }

    static void analyzeRegions(const float* left, const float* right,
                               std::size_t frames, float rate,
                               RegionMap& result)
    {
        result = {};
        result.boundaries[0u] = 0.0f;
        if (!left || !right || frames < 2u || !(rate > 0.0f)) {
            result.count = 1u;
            result.boundaries[1u] = 1.0f;
            return;
        }
        constexpr std::size_t hop = 64u;
        const std::size_t minimumGap = std::max<std::size_t>(
            hop, static_cast<std::size_t>(rate * 0.020f));
        const std::size_t preRoll = static_cast<std::size_t>(rate * 0.001f);
        float fast = 0.0f;
        float slow = 0.0f;
        float previous = 0.0f;
        std::size_t last = 0u;
        std::size_t regions = 1u;
        for (std::size_t frame = 0u; frame < frames; frame += hop) {
            const std::size_t end = std::min(frames, frame + hop);
            float peak = 0.0f;
            float mean = 0.0f;
            for (std::size_t index = frame; index < end; ++index) {
                const float magnitude = std::max(
                    std::fabs(left[index]), std::fabs(right[index]));
                peak = std::max(peak, magnitude);
                mean += magnitude;
            }
            mean /= static_cast<float>(std::max<std::size_t>(1u, end - frame));
            const float energy = peak * 0.65f + mean * 0.35f;
            fast += (energy - fast) * 0.35f;
            slow += (energy - slow) * 0.015f;
            const float novelty = std::max(0.0f, fast - previous);
            const float threshold = std::max(0.0015f, slow * 0.55f);
            if (frame > minimumGap && frame - last >= minimumGap
                && energy > std::max(0.003f, slow * 1.55f)
                && novelty > threshold && regions < Engine::kRegionCount) {
                const std::size_t onset = frame > preRoll ? frame - preRoll : 0u;
                result.boundaries[regions++] = static_cast<float>(onset)
                    / static_cast<float>(frames);
                last = frame;
            }
            previous += (fast - previous) * 0.12f;
        }
        result.count = regions;
        result.boundaries[regions] = 1.0f;
    }

    void installRegionMap(std::size_t lane, const RegionMap& map)
    {
        if (lane >= kLaneCount) return;
        activeRegions[lane] = map;
        engine.setLaneRegions(lane, map.boundaries.data(),
                              std::max<std::size_t>(1u, map.count));
        ++laneRegionRevisions[lane];
        if (laneRegionRevisions[lane] == 0u) ++laneRegionRevisions[lane];
    }

    bool prepareClipBank(int clipIndex)
    {
        if (clipIndex < 0
            || clipIndex >= static_cast<int>(clipSlots.size())) return false;
        preparedLaneFrames.fill(0u);
        preparedLaneRates.fill(sampleRate);
        preparedRegions.fill(RegionMap {});
        preparedWaveMinimum.fill(WaveBins {});
        preparedWaveMaximum.fill(WaveBins {});
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            float* left = preparedClipStorage.get()
                + planeCapacityFrames * lane;
            float* right = left + laneCapacityFrames;
            const ClipSlot& clip = clipSlots[static_cast<std::size_t>(clipIndex)];
            const char* path = clip.laneSourcePaths[lane].data();
            const char* label = clip.laneSourceLabels[lane].data();
            const LaneSourceKind kind = clip.laneSourceKinds[lane];
            preparedLaneKinds[lane] = kind;
            preparedLanePaths[lane][0] = '\0';
            preparedLaneLabels[lane][0] = '\0';
            if (kind == LaneSourceKind::empty) continue;
            if (path[0] == '\0') {
                if (!engine.laneLoaded(lane)) continue;
                const std::size_t frames = engine.laneFrames(lane);
                const float* sourceLeft = engine.laneData(lane, 0u);
                const float* sourceRight = engine.laneData(lane, 1u);
                if (!sourceLeft || !sourceRight || frames > laneCapacityFrames) {
                    return false;
                }
                std::copy(sourceLeft, sourceLeft + frames, left);
                std::copy(sourceRight, sourceRight + frames, right);
                preparedLaneFrames[lane] = frames;
                preparedLaneRates[lane] = engine.laneSampleRate(lane);
                preparedLaneKinds[lane] = currentLaneSourceKinds[lane];
                preparedRegions[lane] = activeRegions[lane];
                analyzeWaveform(left, right, frames,
                                preparedWaveMinimum[lane],
                                preparedWaveMaximum[lane]);
                std::snprintf(preparedLanePaths[lane].data(),
                              preparedLanePaths[lane].size(), "%s",
                              currentLaneSourcePaths[lane].data());
                std::snprintf(preparedLaneLabels[lane].data(),
                              preparedLaneLabels[lane].size(), "%s",
                              laneSourceLabels[lane].data());
                continue;
            }
            if (!decodeLibraryWav(path, left, right, preparedLaneFrames[lane],
                                  preparedLaneRates[lane],
                                  preparedLaneLabels[lane].data(),
                                  preparedLaneLabels[lane].size())) {
                return false;
            }
            analyzeRegions(left, right, preparedLaneFrames[lane],
                           preparedLaneRates[lane], preparedRegions[lane]);
            analyzeWaveform(left, right, preparedLaneFrames[lane],
                            preparedWaveMinimum[lane],
                            preparedWaveMaximum[lane]);
            std::snprintf(preparedLanePaths[lane].data(),
                          preparedLanePaths[lane].size(), "%s", path);
            if (label[0] != '\0') {
                std::snprintf(preparedLaneLabels[lane].data(),
                              preparedLaneLabels[lane].size(), "%s", label);
            }
        }
        preparedClip.store(clipIndex, std::memory_order_relaxed);
        return true;
    }

    static void* sampleWorkerEntry(void* opaque)
    {
        auto* instance = static_cast<ThruInstance*>(opaque);
        while (true) {
            pthread_mutex_lock(&instance->sampleWorkerMutex);
            while (!instance->sampleWorkerSignaled
                   && instance->sampleWorkerRunning.load(
                       std::memory_order_acquire) != 0) {
                pthread_cond_wait(&instance->sampleWorkerCondition,
                                  &instance->sampleWorkerMutex);
            }
            instance->sampleWorkerSignaled = false;
            pthread_mutex_unlock(&instance->sampleWorkerMutex);
            if (instance->sampleWorkerRunning.load(std::memory_order_acquire) == 0) {
                break;
            }
            if (instance->sampleLoadState.load(std::memory_order_acquire)
                == static_cast<int>(SampleLoadState::requested)) {
                instance->sampleLoadState.store(
                    static_cast<int>(SampleLoadState::loading),
                    std::memory_order_release);
                if (instance->loadLibraryWav(instance->sampleRequestPath.data())) {
                    instance->sampleLoadState.store(
                        static_cast<int>(SampleLoadState::ready),
                        std::memory_order_release);
                }
                continue;
            }
            if (instance->clipPrepareState.load(std::memory_order_acquire)
                == static_cast<int>(ClipPrepareState::requested)) {
                instance->clipPrepareState.store(
                    static_cast<int>(ClipPrepareState::loading),
                    std::memory_order_release);
                const int target = instance->clipPrepareRequest.load(
                    std::memory_order_relaxed);
                const bool prepared = instance->prepareClipBank(target);
                if (prepared
                    && instance->sampleLoadState.load(std::memory_order_acquire)
                        == static_cast<int>(SampleLoadState::error)) {
                    instance->sampleLoadState.store(
                        static_cast<int>(SampleLoadState::complete),
                        std::memory_order_release);
                }
                instance->clipPrepareState.store(static_cast<int>(prepared
                    ? ClipPrepareState::ready : ClipPrepareState::error),
                    std::memory_order_release);
            }
        }
        return nullptr;
    }

    bool startSampleWorker()
    {
        if (pthread_mutex_init(&sampleWorkerMutex, nullptr) != 0) return false;
        if (pthread_cond_init(&sampleWorkerCondition, nullptr) != 0) {
            pthread_mutex_destroy(&sampleWorkerMutex);
            return false;
        }
        sampleWorkerRunning.store(1, std::memory_order_release);
        if (pthread_create(&sampleWorker, nullptr, sampleWorkerEntry, this) != 0) {
            sampleWorkerRunning.store(0, std::memory_order_release);
            pthread_cond_destroy(&sampleWorkerCondition);
            pthread_mutex_destroy(&sampleWorkerMutex);
            return false;
        }
        return true;
    }

    void stopSampleWorker()
    {
        if (!sampleWorkerReady) return;
        sampleWorkerRunning.store(0, std::memory_order_release);
        pthread_mutex_lock(&sampleWorkerMutex);
        sampleWorkerSignaled = true;
        pthread_cond_signal(&sampleWorkerCondition);
        pthread_mutex_unlock(&sampleWorkerMutex);
        pthread_join(sampleWorker, nullptr);
        pthread_cond_destroy(&sampleWorkerCondition);
        pthread_mutex_destroy(&sampleWorkerMutex);
        sampleWorkerReady = false;
    }

    void requestLibrarySample(int lane, const char* path,
                              bool fromClip = false, int ownerClip = -1,
                              bool preview = false)
    {
        if (!sampleWorkerReady || lane < 0 || lane >= static_cast<int>(kLaneCount)
            || !path || std::strlen(path) >= sampleRequestPath.size()) {
            setSampleLoadError("Sample loader is unavailable");
            return;
        }
        const auto prepareState = static_cast<ClipPrepareState>(
            clipPrepareState.load(std::memory_order_acquire));
        if (prepareState == ClipPrepareState::requested
            || prepareState == ClipPrepareState::loading) {
            setError("Wait for the queued clip to finish preparing");
            return;
        }
        const int current = sampleLoadState.load(std::memory_order_acquire);
        if (current == static_cast<int>(SampleLoadState::previewing)
            && !preview
            && std::strcmp(path, sampleRequestPath.data()) == 0) {
            if (!fromClip) pushClipUndo(activeClip);
            sampleRequestPreview = false;
            samplePreviewCancelled = false;
            previewActive = false;
            sampleLoadState.store(static_cast<int>(SampleLoadState::ready),
                                  std::memory_order_release);
            return;
        }
        if (current == static_cast<int>(SampleLoadState::previewing)) {
            previewActive = false;
            sampleLoadState.store(static_cast<int>(SampleLoadState::complete),
                                  std::memory_order_release);
        } else if (current == static_cast<int>(SampleLoadState::requested)
            || current == static_cast<int>(SampleLoadState::loading)
            || current == static_cast<int>(SampleLoadState::ready)
            || current == static_cast<int>(SampleLoadState::removing)
            || current == static_cast<int>(SampleLoadState::copying)) {
            setError("Wait for the current sample load");
            return;
        }
        if (!fromClip && !preview) pushClipUndo(activeClip);
        pthread_mutex_lock(&sampleWorkerMutex);
        sampleRequestLane = lane;
        sampleRequestClip = ownerClip >= 0 ? ownerClip : activeClip;
        sampleRequestFromClip = fromClip;
        sampleRequestPreview = preview;
        samplePreviewCancelled = false;
        std::snprintf(sampleRequestPath.data(), sampleRequestPath.size(), "%s", path);
        sampleLoadError[0] = '\0';
        sampleLoadState.store(static_cast<int>(SampleLoadState::requested),
                              std::memory_order_release);
        sampleWorkerSignaled = true;
        pthread_cond_signal(&sampleWorkerCondition);
        pthread_mutex_unlock(&sampleWorkerMutex);
    }

    bool requestClipPreparation(int clipIndex)
    {
        if (!sampleWorkerReady || engine.recording() || clipIndex < 0
            || clipIndex >= static_cast<int>(clipSlots.size())) return false;
        const auto sampleState = static_cast<SampleLoadState>(
            sampleLoadState.load(std::memory_order_acquire));
        if (sampleState >= SampleLoadState::requested
            && sampleState <= SampleLoadState::copying) return false;
        const auto prepareState = static_cast<ClipPrepareState>(
            clipPrepareState.load(std::memory_order_acquire));
        if (prepareState == ClipPrepareState::requested
            || prepareState == ClipPrepareState::loading) return false;
        initializeClipIfNeeded(clipIndex);
        pthread_mutex_lock(&sampleWorkerMutex);
        clipPrepareRequest.store(clipIndex, std::memory_order_relaxed);
        preparedClip.store(-1, std::memory_order_relaxed);
        clipPrepareState.store(
            static_cast<int>(ClipPrepareState::requested),
            std::memory_order_release);
        sampleWorkerSignaled = true;
        pthread_cond_signal(&sampleWorkerCondition);
        pthread_mutex_unlock(&sampleWorkerMutex);
        return true;
    }

    bool clipPreparationInFlight() const
    {
        const auto state = static_cast<ClipPrepareState>(
            clipPrepareState.load(std::memory_order_acquire));
        return state == ClipPrepareState::requested
            || state == ClipPrepareState::loading;
    }

    void queueClipSources(int clipIndex)
    {
        pendingClipSourceMask = 0u;
        if (clipIndex < 0
            || clipIndex >= static_cast<int>(clipSlots.size())) return;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            const ClipSlot& clip = clipSlots[static_cast<std::size_t>(clipIndex)];
            const char* path = clip.laneSourcePaths[lane].data();
            const char* label = clip.laneSourceLabels[lane].data();
            const LaneSourceKind kind = clip.laneSourceKinds[lane];
            if (kind == LaneSourceKind::empty) {
                if (engine.laneLoaded(lane)) {
                    engine.beginLaneRemoval(lane);
                    pendingClearMask |= 1u << lane;
                }
                currentLaneSourcePaths[lane][0] = '\0';
                currentLaneSourceKinds[lane] = LaneSourceKind::empty;
                std::snprintf(laneSourceLabels[lane].data(),
                              laneSourceLabels[lane].size(), "EMPTY");
                activeRegions[lane] = {};
                ++laneRegionRevisions[lane];
                clearWaveform(lane);
                continue;
            }
            if (path[0] == '\0') continue;
            if (std::strcmp(path, currentLaneSourcePaths[lane].data()) == 0) {
                currentLaneSourceKinds[lane] = kind;
                if (label[0] != '\0') {
                    std::snprintf(laneSourceLabels[lane].data(),
                                  laneSourceLabels[lane].size(), "%s", label);
                }
                continue;
            }
            std::snprintf(pendingClipSourcePaths[lane].data(),
                          pendingClipSourcePaths[lane].size(), "%s", path);
            pendingClipSourceMask |= 1u << lane;
        }
    }

    void serviceClipSourceQueue()
    {
        if (pendingClipSourceMask == 0u || !sampleWorkerReady) return;
        const int loadState = sampleLoadState.load(std::memory_order_acquire);
        if (loadState == static_cast<int>(SampleLoadState::requested)
            || loadState == static_cast<int>(SampleLoadState::loading)
            || loadState == static_cast<int>(SampleLoadState::ready)
            || loadState == static_cast<int>(SampleLoadState::removing)
            || loadState == static_cast<int>(SampleLoadState::copying)) return;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if ((pendingClipSourceMask & (1u << lane)) == 0u) continue;
            pendingClipSourceMask &= ~(1u << lane);
            requestLibrarySample(static_cast<int>(lane),
                                 pendingClipSourcePaths[lane].data(), true,
                                 activeClip);
            break;
        }
    }

    void serviceSampleLoad()
    {
        auto state = static_cast<SampleLoadState>(
            sampleLoadState.load(std::memory_order_acquire));
        if (state == SampleLoadState::ready) {
            if (engine.recording()) return;
            if (sampleRequestPreview) {
                if (samplePreviewCancelled) {
                    sampleRequestPreview = false;
                    samplePreviewCancelled = false;
                    previewActive = false;
                    sampleLoadState.store(
                        static_cast<int>(SampleLoadState::complete),
                        std::memory_order_release);
                    return;
                }
                previewFrame = 0u;
                previewActive = true;
                sampleLoadState.store(static_cast<int>(SampleLoadState::previewing),
                                      std::memory_order_release);
                return;
            }
            sampleApplyLane = sampleRequestLane;
            sampleCopyFrame = 0u;
            sampleRemovalRequested = false;
            pendingClearMask |= 1u << sampleApplyLane;
            sampleLoadState.store(static_cast<int>(SampleLoadState::removing),
                                  std::memory_order_release);
            state = SampleLoadState::removing;
        }
        if (state == SampleLoadState::removing) {
            const std::size_t lane = static_cast<std::size_t>(sampleApplyLane);
            if (engine.laneLoaded(lane) && !sampleRemovalRequested) {
                sampleRemovalRequested = engine.beginLaneRemoval(lane);
            }
            if (engine.laneLoaded(lane)) return;
            for (const auto& voice : voices) {
                if (voice.engine.laneLoaded(lane)) return;
            }
            sampleLoadState.store(static_cast<int>(SampleLoadState::copying),
                                  std::memory_order_release);
            state = SampleLoadState::copying;
        }
        if (state != SampleLoadState::copying) return;
        const std::size_t lane = static_cast<std::size_t>(sampleApplyLane);
        float* destinationLeft = engine.writableLane(lane, 0u);
        float* destinationRight = engine.writableLane(lane, 1u);
        const float* sourceLeft = sampleStage.get();
        const float* sourceRight = sampleStage.get() + laneCapacityFrames;
        const std::size_t end = std::min(
            sampleStageFrames, sampleCopyFrame + kLibraryCopyFrames);
        std::copy(sourceLeft + sampleCopyFrame, sourceLeft + end,
                  destinationLeft + sampleCopyFrame);
        std::copy(sourceRight + sampleCopyFrame, sourceRight + end,
                  destinationRight + sampleCopyFrame);
        sampleCopyFrame = end;
        if (sampleCopyFrame < sampleStageFrames) return;
        if (!engine.adoptLane(lane, sampleStageFrames, sampleStageRate)) {
            pendingClearMask &= ~(1u << lane);
            setSampleLoadError("Could not adopt loaded sample");
            return;
        }
        installRegionMap(lane, sampleStageRegions);
        pendingClearMask &= ~(1u << lane);
        waveMinimum[lane] = sampleStageWaveMinimum;
        waveMaximum[lane] = sampleStageWaveMaximum;
        waveDirtyMask &= ~(1u << lane);
        if (scanLane == static_cast<int>(lane)) scanLane = -1;
        waveTextDirty = true;
        std::snprintf(laneSourceLabels[lane].data(), laneSourceLabels[lane].size(),
                      "%.*s",
                      static_cast<int>(laneSourceLabels[lane].size() - 1u),
                      sampleLoadedName.data());
        std::snprintf(currentLaneSourcePaths[lane].data(),
                      currentLaneSourcePaths[lane].size(), "%s",
                      sampleRequestPath.data());
        const int owner = clampValue(sampleRequestClip, 0,
                                     static_cast<int>(kClipSlotCount) - 1);
        if (!sampleRequestFromClip) {
            ClipSlot& clip = clipSlots[static_cast<std::size_t>(owner)];
            clip.laneSourceKinds[lane] = LaneSourceKind::file;
            std::snprintf(clip.laneSourcePaths[lane].data(),
                          clip.laneSourcePaths[lane].size(), "%s",
                          sampleRequestPath.data());
            std::snprintf(clip.laneSourceLabels[lane].data(),
                          clip.laneSourceLabels[lane].size(), "%.*s",
                          static_cast<int>(clip.laneSourceLabels[lane].size() - 1u),
                          sampleLoadedName.data());
            clip.occupied = true;
            currentLaneSourceKinds[lane] = LaneSourceKind::file;
            markPersistentStateDirty();
        } else {
            const ClipSlot& clip = clipSlots[static_cast<std::size_t>(owner)];
            currentLaneSourceKinds[lane] = clip.laneSourceKinds[lane];
            if (clip.laneSourceLabels[lane][0] != '\0') {
                std::snprintf(laneSourceLabels[lane].data(),
                              laneSourceLabels[lane].size(), "%s",
                              clip.laneSourceLabels[lane].data());
            }
        }
        sampleLoadState.store(static_cast<int>(SampleLoadState::complete),
                              std::memory_order_release);
    }

    void allNotesOff()
    {
        if (clipRecording) {
            ClipSlot* clip = activeClipSlot();
            if (clip) {
                for (std::size_t key = 0u;
                     key < clipOpenNoteIndices.size(); ++key) {
                    const int index = clipOpenNoteIndices[key];
                    if (index < 0
                        || index >= static_cast<int>(clip->clipNoteCount)) {
                        continue;
                    }
                    const double elapsed = std::max(1.0,
                        clipCaptureAbsoluteMs() - clipOpenNoteStarts[key]);
                    const std::uint32_t maximum = clip->clipDurationMs != 0u
                        ? clip->clipDurationMs : kClipMaximumDurationMs;
                    clip->clipNotes[static_cast<std::size_t>(index)].durationMs =
                        static_cast<std::uint32_t>(clampValue(
                            elapsed, 1.0, static_cast<double>(maximum)));
                }
                clipOpenNoteIndices.fill(-1);
            }
        }
        padMappedNotes.fill(-1);
        activeNote = -1;
        for (auto& voice : voices) {
            voice.held = false;
            voice.target = 0.0f;
        }
    }

    void updateActiveNote()
    {
        activeNote = -1;
        std::uint32_t newestAge = 0u;
        for (const auto& voice : voices) {
            if (voice.active && voice.held && !voice.clipOwned
                && voice.age >= newestAge) {
                activeNote = voice.note;
                newestAge = voice.age;
            }
        }
    }

    PlaybackVoice* findVoiceForNote(int note, int sourcePad)
    {
        for (auto& voice : voices) {
            if (voice.active && !voice.clipOwned && voice.note == note
                && voice.sourcePad == sourcePad) {
                return &voice;
            }
        }
        return nullptr;
    }

    PlaybackVoice& allocateVoice(int note, int sourcePad)
    {
        if (PlaybackVoice* existing = findVoiceForNote(note, sourcePad)) return *existing;
        for (auto& voice : voices) {
            if (!voice.active) return voice;
        }
        PlaybackVoice* oldest = &voices[0u];
        for (auto& voice : voices) {
            if (!voice.held && oldest->held) oldest = &voice;
            else if (voice.held == oldest->held && voice.age < oldest->age) oldest = &voice;
        }
        return *oldest;
    }

    std::size_t chooseVariationLane(int velocity)
    {
        std::array<std::size_t, kLaneCount> loaded {};
        std::size_t count = 0u;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if (engine.laneLoaded(lane)) loaded[count++] = lane;
        }
        if (count == 0u) return 0u;
        std::size_t choice = 0u;
        switch (laneVariation) {
            case LaneVariation::cycle:
                choice = variationCounter++ % count;
                break;
            case LaneVariation::random:
            case LaneVariation::noRepeat: {
                performanceRandom ^= performanceRandom << 13u;
                performanceRandom ^= performanceRandom >> 17u;
                performanceRandom ^= performanceRandom << 5u;
                choice = performanceRandom % count;
                if (laneVariation == LaneVariation::noRepeat && count > 1u
                    && static_cast<int>(loaded[choice]) == lastVariationLane) {
                    choice = (choice + 1u) % count;
                }
                break;
            }
            case LaneVariation::velocity:
                choice = std::min<std::size_t>(count - 1u,
                    static_cast<std::size_t>(clampValue(velocity, 1, 127))
                        * count / 128u);
                break;
            case LaneVariation::manual: {
                const std::size_t requested = static_cast<std::size_t>(clampValue(
                    static_cast<int>(std::lround(controls.manualLane)), 0, 3));
                for (std::size_t index = 0u; index < count; ++index) {
                    if (loaded[index] == requested) choice = index;
                }
                break;
            }
        }
        lastVariationLane = static_cast<int>(loaded[choice]);
        return loaded[choice];
    }

    ClipSlot* activeClipSlot()
    {
        return activeClip >= 0
                && activeClip < static_cast<int>(clipSlots.size())
            ? &clipSlots[static_cast<std::size_t>(activeClip)] : nullptr;
    }

    const ClipSlot* activeClipSlot() const
    {
        return activeClip >= 0
                && activeClip < static_cast<int>(clipSlots.size())
            ? &clipSlots[static_cast<std::size_t>(activeClip)] : nullptr;
    }

    double captureElapsedMs() const
    {
        const std::uint64_t now = renderClockSamples.load(
            std::memory_order_relaxed);
        const std::uint64_t elapsed = now >= clipRecordStartSample
            ? now - clipRecordStartSample : 0u;
        return static_cast<double>(elapsed) * 1000.0
            / static_cast<double>(std::max(1.0f, sampleRate));
    }

    double clipCaptureAbsoluteMs() const
    {
        return !clipReplacing
            ? clipAbsolutePositionMs : captureElapsedMs();
    }

    std::uint32_t clipCaptureTimeMs() const
    {
        const ClipSlot* clip = activeClipSlot();
        const double absolute = clipCaptureAbsoluteMs();
        if (!clipReplacing && clip
            && clip->clipDurationMs >= kClipMinimumDurationMs) {
            return static_cast<std::uint32_t>(std::fmod(
                std::max(0.0, absolute),
                static_cast<double>(clip->clipDurationMs)));
        }
        return static_cast<std::uint32_t>(clampValue(
            absolute, 0.0, static_cast<double>(kClipMaximumDurationMs)));
    }

    static int clipCaptureKey(int note, int sourcePad)
    {
        return sourcePad >= 0 && sourcePad < 32
            ? sourcePad : 32 + clampValue(note, 0, 127);
    }

    static std::size_t clipPointLane(const ClipPoint& point)
    {
        if (point.type == ClipPointType::parameter) {
            const std::size_t parameter = point.parameter & 0xffu;
            return parameter < kLockParameterCount
                ? parameter
                : kClipCurveLaneCount;
        }
        return point.sourcePad < 32u
            ? kLockParameterCount + static_cast<std::size_t>(point.sourcePad)
            : kClipCurveLaneCount;
    }

    static std::uint8_t clipPointTake(const ClipPoint& point)
    {
        return static_cast<std::uint8_t>(point.parameter >> 8u);
    }

    static std::uint16_t clipPointDescriptor(
        std::size_t parameter, std::uint8_t take)
    {
        return static_cast<std::uint16_t>(
            std::min<std::size_t>(parameter, kLockParameterCount - 1u)
            | (static_cast<std::uint16_t>(take) << 8u));
    }

    static void rebuildClipPointIndex(ClipSlot& clip)
    {
        std::sort(clip.clipPoints.begin(),
                  clip.clipPoints.begin() + clip.clipPointCount,
                  [](const ClipPoint& left, const ClipPoint& right) {
                      const std::size_t leftLane = clipPointLane(left);
                      const std::size_t rightLane = clipPointLane(right);
                      return leftLane != rightLane
                          ? leftLane < rightLane : left.timeMs < right.timeMs;
                  });
        clip.clipPointStarts.fill(0u);
        clip.clipPointCounts.fill(0u);
        for (std::size_t index = 0u; index < clip.clipPointCount; ++index) {
            const std::size_t lane = clipPointLane(clip.clipPoints[index]);
            if (lane >= kClipCurveLaneCount) continue;
            if (clip.clipPointCounts[lane] == 0u) {
                clip.clipPointStarts[lane] =
                    static_cast<std::uint16_t>(index);
            }
            ++clip.clipPointCounts[lane];
        }
    }

    void captureClipNoteOn(int note, int velocity, int sourcePad)
    {
        ClipSlot* clip = activeClipSlot();
        if (!clipRecording || !clip) return;
        if (clip->clipNoteCount >= kClipNoteCapacity) {
            clip->noteCapacityReached = true;
            return;
        }
        const int key = clipCaptureKey(note, sourcePad);
        if (key < 0 || key >= static_cast<int>(clipOpenNoteIndices.size())) return;
        const std::size_t index = clip->clipNoteCount++;
        ClipNote& captured = clip->clipNotes[index];
        captured.startMs = clipCaptureTimeMs();
        captured.durationMs = 1u;
        captured.note = static_cast<std::uint8_t>(clampValue(note, 0, 127));
        captured.velocity = static_cast<std::uint8_t>(
            clampValue(velocity, 1, 127));
        captured.sourcePad = sourcePad >= 0 && sourcePad < 32
            ? static_cast<std::uint8_t>(sourcePad) : 255u;
        captured.take = clipRecordingTake;
        clipOpenNoteIndices[static_cast<std::size_t>(key)] =
            static_cast<int>(index);
        clipOpenNoteStarts[static_cast<std::size_t>(key)] =
            clipCaptureAbsoluteMs();
    }

    void captureClipNoteOff(int note, int sourcePad)
    {
        ClipSlot* clip = activeClipSlot();
        if (!clipRecording || !clip) return;
        const int key = clipCaptureKey(note, sourcePad);
        if (key < 0 || key >= static_cast<int>(clipOpenNoteIndices.size())) return;
        const int index = clipOpenNoteIndices[static_cast<std::size_t>(key)];
        if (index >= 0 && index < static_cast<int>(clip->clipNoteCount)) {
            const double elapsed = std::max(1.0,
                clipCaptureAbsoluteMs()
                    - clipOpenNoteStarts[static_cast<std::size_t>(key)]);
            const std::uint32_t maximum = clip->clipDurationMs != 0u
                ? clip->clipDurationMs : kClipMaximumDurationMs;
            clip->clipNotes[static_cast<std::size_t>(index)].durationMs =
                static_cast<std::uint32_t>(clampValue(
                    elapsed, 1.0, static_cast<double>(maximum)));
        }
        clipOpenNoteIndices[static_cast<std::size_t>(key)] = -1;
    }

    void captureClipPoint(ClipPointType type, std::size_t parameter,
                          int sourcePad, float value)
    {
        ClipSlot* clip = activeClipSlot();
        if (!clipRecording || !clip) return;
        const std::uint32_t time = clipCaptureTimeMs();
        const std::size_t curveLane = type == ClipPointType::parameter
            ? std::min(parameter, kLockParameterCount - 1u)
            : (sourcePad >= 0 && sourcePad < 32
                ? kLockParameterCount + static_cast<std::size_t>(sourcePad)
                : kClipCurveLaneCount);
        if (curveLane >= kClipCurveLaneCount) return;
        const float use = static_cast<float>(clip->clipPointCount)
            / static_cast<float>(kClipPointCapacity);
        const std::uint32_t minimumInterval = use >= 0.90f ? 40u
            : (use >= 0.75f ? 20u : 10u);
        const float minimumChange = use >= 0.90f ? 0.05f
            : (use >= 0.75f ? 0.03f : 0.02f);
        if (type == ClipPointType::pressure && sourcePad >= 0
            && sourcePad < static_cast<int>(clipLastPressureTimes.size())) {
            const std::size_t pad = static_cast<std::size_t>(sourcePad);
            const std::uint32_t previousTime = clipLastPressureTimes[pad];
            const float previousValue = clipLastPressureValues[pad];
            if (previousTime != 0xffffffffu && time >= previousTime
                && time - previousTime < minimumInterval
                && std::fabs(value - previousValue) < minimumChange) return;
            clipLastPressureTimes[pad] = time;
            clipLastPressureValues[pad] = value;
        }
        const int previousIndex = clipLastPointIndices[curveLane];
        if (previousIndex >= 0
            && previousIndex < static_cast<int>(clip->clipPointCount)) {
            ClipPoint& previous = clip->clipPoints[
                static_cast<std::size_t>(previousIndex)];
            if (time >= previous.timeMs
                && time - previous.timeMs < minimumInterval
                && std::fabs(value - previous.value) < minimumChange) {
                previous.timeMs = time;
                previous.value = value;
                return;
            }
        }
        if (clip->clipPointCount >= kClipPointCapacity) {
            clip->pointCapacityReached = true;
            if (previousIndex >= 0
                && previousIndex < static_cast<int>(clip->clipPointCount)) {
                ClipPoint& endpoint = clip->clipPoints[
                    static_cast<std::size_t>(previousIndex)];
                endpoint.timeMs = time;
                endpoint.value = value;
            }
            return;
        }
        const std::size_t pointIndex = clip->clipPointCount++;
        ClipPoint& point = clip->clipPoints[pointIndex];
        point.timeMs = time;
        point.value = value;
        point.parameter = clipPointDescriptor(parameter, clipRecordingTake);
        point.sourcePad = sourcePad >= 0 && sourcePad < 32
            ? static_cast<std::uint8_t>(sourcePad) : 255u;
        point.type = type;
        clipLastPointIndices[curveLane] = static_cast<int>(pointIndex);
    }

    void captureClipParameter(int parameter)
    {
        if (parameter < 0
            || parameter >= static_cast<int>(kLockParameterCount)) return;
        captureClipPoint(ClipPointType::parameter,
            static_cast<std::size_t>(parameter), -1,
            lockedValue(controls, outputGain,
                        static_cast<std::size_t>(parameter)));
    }

    void noteOn(int note, int velocity, int sourcePad = -1)
    {
        note = clampValue(note, 0, 127);
        velocity = clampValue(velocity, 1, 127);
        ++noteClock;
        PlaybackVoice& voice = allocateVoice(note, sourcePad);
        voice.note = note;
        voice.sourcePad = sourcePad;
        voice.velocity = velocity;
        voice.variantLane = chooseVariationLane(velocity);
        voice.age = noteClock;
        voice.laneOffset = clampValue(controls.manualLane / 3.0f, 0.0f, 1.0f);
        voice.target = 1.0f;
        voice.active = true;
        voice.held = true;
        voice.clipOwned = false;
        voice.clipNoteIndex = -1;
        voice.useSnapshot = false;
        performanceLane = voice.laneOffset * 3.0f;
        voice.engine.retrigger();
        captureClipNoteOn(note, velocity, sourcePad);
        updateActiveNote();
    }

    void noteOff(int note, int sourcePad = -1)
    {
        note = clampValue(note, 0, 127);
        for (auto& voice : voices) {
            if (voice.active && !voice.clipOwned && voice.note == note
                && voice.sourcePad == sourcePad) {
                voice.held = false;
                voice.target = 0.0f;
            }
        }
        captureClipNoteOff(note, sourcePad);
        updateActiveNote();
    }

    void aftertouch(int note, int pressure, int sourcePad = -1)
    {
        if (pressureMode == 0) return;
        const float offset = midiUnit(pressure);
        if (sourcePad >= 0) {
            captureClipPoint(ClipPointType::pressure, 0u, sourcePad, offset);
        }
        for (auto& voice : voices) {
            if (voice.active && voice.held && !voice.clipOwned
                && voice.note == note && voice.sourcePad == sourcePad) {
                voice.laneOffset = offset;
                if (note == activeNote) performanceLane = offset * 3.0f;
            }
        }
    }

    void setPerformanceLane(float lane)
    {
        performanceLane = clampValue(lane, 0.0f, 3.0f);
        const float offset = performanceLane / 3.0f;
        PlaybackVoice* newest = nullptr;
        for (auto& voice : voices) {
            if (voice.active && voice.held && !voice.clipOwned
                && (!newest || voice.age > newest->age)) {
                newest = &voice;
            }
        }
        if (newest) newest->laneOffset = offset;
    }

    int padToMidiNote(int hardwareNote) const
    {
        const int index = clampValue(hardwareNote - kFirstPadNote, 0, 31);
        if (padMode == PadMode::slices || padMode == PadMode::rolls) {
            return rootNote;
        }
        const int row = index / kPadColumns;
        const int column = index % kPadColumns;
        return clampValue(kPadBaseNote + padOctave * 12
            + row * kPadRowInterval + column, 0, 127);
    }

    void padNoteOn(int hardwareNote, int velocity)
    {
        const int index = hardwareNote - kFirstPadNote;
        if (index < 0 || index >= static_cast<int>(padMappedNotes.size())) return;
        if (padMode == PadMode::cues) {
            padMappedNotes[static_cast<std::size_t>(index)] = -1;
            return;
        }
        const int mappedNote = padToMidiNote(hardwareNote);
        padMappedNotes[static_cast<std::size_t>(index)] = mappedNote;
        noteOn(mappedNote, velocity, index);
        if (padMode == PadMode::rolls) {
            if (PlaybackVoice* voice = findVoiceForNote(mappedNote, index)) {
                voice->snapshotControls = controls;
                voice->snapshotControls.algorithm = Engine::PlaybackAlgorithm::cutups;
                voice->snapshotControls.cutClock = Engine::CutClock::tempo;
                voice->snapshotControls.cutDivision =
                    static_cast<Engine::CutDivision>(index %
                        static_cast<int>(Engine::kCutDivisionCount));
                voice->snapshotControls.cutGate = 1.0f;
                voice->snapshotOutputGain = outputGain;
                voice->useSnapshot = true;
            }
        }
    }

    void padNoteOff(int hardwareNote)
    {
        const int index = hardwareNote - kFirstPadNote;
        if (index < 0 || index >= static_cast<int>(padMappedNotes.size())) return;
        if (padMode == PadMode::cues) {
            noteOff(rootNote, index % static_cast<int>(kCueCount));
            return;
        }
        int mappedNote = padMappedNotes[static_cast<std::size_t>(index)];
        if (mappedNote < 0) mappedNote = padToMidiNote(hardwareNote);
        padMappedNotes[static_cast<std::size_t>(index)] = -1;
        noteOff(mappedNote, index);
    }

    void storeCue(int slot, float position)
    {
        if (activeClip < 0 || activeClip >= static_cast<int>(clipSlots.size())
            || slot < 0 || slot >= static_cast<int>(kCueCount)) return;
        pushClipUndo(activeClip);
        clipSlots[static_cast<std::size_t>(activeClip)].cues[
            static_cast<std::size_t>(slot)] = clampValue(position, 0.0f, 1.0f);
        clipSlots[static_cast<std::size_t>(activeClip)].occupied = true;
        markPersistentStateDirty();
    }

    void clearCue(int slot)
    {
        if (activeClip < 0 || activeClip >= static_cast<int>(clipSlots.size())
            || slot < 0 || slot >= static_cast<int>(kCueCount)) return;
        pushClipUndo(activeClip);
        clipSlots[static_cast<std::size_t>(activeClip)].cues[
            static_cast<std::size_t>(slot)] = -1.0f;
        markPersistentStateDirty();
    }

    void triggerCue(int slot)
    {
        if (activeClip < 0 || activeClip >= static_cast<int>(clipSlots.size())
            || slot < 0 || slot >= static_cast<int>(kCueCount)) return;
        const float cue = clipSlots[static_cast<std::size_t>(activeClip)].cues[
            static_cast<std::size_t>(slot)];
        if (cue < 0.0f) return;
        bool hasPerformanceVoice = false;
        for (const auto& voice : voices) {
            hasPerformanceVoice = hasPerformanceVoice
                || (voice.active && voice.held && !voice.clipOwned);
        }
        if (!hasPerformanceVoice) noteOn(rootNote, 100, slot);
        for (auto& voice : voices) {
            if (voice.active) voice.engine.seekNormalized(cue);
        }
    }

    Engine::Controls effectiveControls(const PlaybackVoice& voice) const
    {
        Engine::Controls effective = voice.clipOwned
            ? clipControls : (voice.useSnapshot
                ? voice.snapshotControls : controls);
        const float noteRatio = voice.note >= 0
            ? std::pow(2.0f, static_cast<float>(voice.note - rootNote) / 12.0f)
            : 1.0f;
        if (effective.algorithm == Engine::PlaybackAlgorithm::grains) {
            // Sample Grains maps note pitch to source advance. Grain pitch is
            // an independent timbral parameter and remains lockable.
            effective.speed = clampValue(
                effective.speed * noteRatio, 0.05f, 4.0f);
        } else if (effective.algorithm == Engine::PlaybackAlgorithm::cutups) {
            // Cut clock remains tempo/free-rate driven while the note retunes
            // each cut reader, matching the source-family Cutups semantics.
            effective.cutPitch = clampValue(
                effective.speed * noteRatio, 0.0625f, 16.0f);
        } else {
            effective.speed = clampValue(
                effective.speed * noteRatio, 0.05f, 4.0f);
        }
        effective.cutTempoBpm = tempoBpm;
        effective.cutSwing = tempoSwing;
        effective.cutVoiceIndex = static_cast<float>(voice.sourcePad >= 0
            ? voice.sourcePad : std::max(0, voice.note - rootNote));
        effective.sampleLane = static_cast<float>(voice.variantLane);
        effective.sampleSlice = static_cast<float>(voice.sourcePad >= 0
            ? voice.sourcePad : std::max(0, voice.note - rootNote));
        effective.lanePhaseOffset = pressureMode != 0
            ? clampValue(voice.laneOffset, 0.0f, 1.0f) : 0.0f;
        effective.laneScanPaused = stackScanPaused;
        effective.mix = 1.0f;
        effective.recordInputGain = 1.0f;
        return effective;
    }

    PlaybackVoice& allocateClipVoice()
    {
        for (auto& voice : voices) {
            if (!voice.active) return voice;
        }
        PlaybackVoice* candidate = nullptr;
        for (auto& voice : voices) {
            if (!voice.held && (!candidate || voice.age < candidate->age)) {
                candidate = &voice;
            }
        }
        if (candidate) return *candidate;
        candidate = &voices[0u];
        for (auto& voice : voices) {
            if (voice.clipOwned && !candidate->clipOwned) candidate = &voice;
            else if (voice.clipOwned == candidate->clipOwned
                     && voice.age < candidate->age) candidate = &voice;
        }
        return *candidate;
    }

    void releaseClipVoices()
    {
        for (auto& voice : voices) {
            if (!voice.clipOwned) continue;
            voice.held = false;
            voice.target = 0.0f;
            voice.clipNoteIndex = -1;
        }
    }

    void triggerClipVoice(const ClipSlot& clip, std::size_t noteIndex)
    {
        if (noteIndex >= clip.clipNoteCount) return;
        const ClipNote& event = clip.clipNotes[noteIndex];
        ++noteClock;
        PlaybackVoice& voice = allocateClipVoice();
        voice.note = event.note;
        voice.sourcePad = event.sourcePad < 32u
            ? static_cast<int>(event.sourcePad) : -1;
        voice.velocity = event.velocity;
        voice.variantLane = chooseVariationLane(voice.velocity);
        voice.age = noteClock;
        voice.laneOffset = clampValue(clipControls.manualLane / 3.0f, 0.0f, 1.0f);
        voice.target = 1.0f;
        voice.active = true;
        voice.held = true;
        voice.clipOwned = true;
        voice.useSnapshot = false;
        voice.clipNoteIndex = static_cast<int>(noteIndex);
        voice.engine.retrigger();
    }

    void releaseClipVoice(std::size_t noteIndex)
    {
        for (auto& voice : voices) {
            if (!voice.active || !voice.clipOwned
                || voice.clipNoteIndex != static_cast<int>(noteIndex)) continue;
            voice.held = false;
            voice.target = 0.0f;
            voice.clipNoteIndex = -1;
        }
    }

    static bool clipEventCrossed(std::uint32_t eventTime,
                                 double from, double to,
                                 bool includeFrom = false)
    {
        return (includeFrom ? static_cast<double>(eventTime) >= from
                            : static_cast<double>(eventTime) > from)
            && static_cast<double>(eventTime) <= to;
    }

    void processClipEvents(const ClipSlot& clip, double from, double to,
                           bool includeFrom = false)
    {
        const std::uint32_t duration = std::max<std::uint32_t>(
            kClipMinimumDurationMs, clip.clipDurationMs);
        // Releases precede attacks at the same loop position, so a note that
        // fills the complete loop retriggers cleanly at the seam.
        for (std::size_t index = 0u; index < clip.clipNoteCount; ++index) {
            const ClipNote& note = clip.clipNotes[index];
            const std::uint32_t end = static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(note.startMs)
                    + std::min(note.durationMs, duration)) % duration);
            if (clipEventCrossed(end, from, to, includeFrom)) {
                releaseClipVoice(index);
            }
        }
        // New overdub notes are appended until the pass is committed, so the
        // attack array is temporarily unsorted. Preserve correct live
        // overdub playback with a linear fallback; committed clips use the
        // bounded sorted lookup below.
        if (clipRecording && !clipReplacing) {
            for (std::size_t index = 0u; index < clip.clipNoteCount; ++index) {
                if (clipEventCrossed(clip.clipNotes[index].startMs,
                                     from, to, includeFrom)) {
                    triggerClipVoice(clip, index);
                }
            }
            return;
        }
        const auto begin = clip.clipNotes.begin();
        const auto end = begin + clip.clipNoteCount;
        const auto first = includeFrom
            ? std::lower_bound(begin, end, from,
                [](const ClipNote& note, double time) {
                    return static_cast<double>(note.startMs) < time;
                })
            : std::upper_bound(begin, end, from,
                [](double time, const ClipNote& note) {
                    return time < static_cast<double>(note.startMs);
                });
        const auto last = std::upper_bound(first, end, to,
            [](double time, const ClipNote& note) {
                return time < static_cast<double>(note.startMs);
            });
        for (auto event = first; event != last; ++event) {
            triggerClipVoice(clip,
                static_cast<std::size_t>(event - begin));
        }
    }

    void applyClipCurves(const ClipSlot& clip, double position)
    {
        clipControls = controls;
        clipOutputGain = outputGain;
        if (clip.clipPointCount == 0u || clip.clipDurationMs == 0u) return;
        const double duration = static_cast<double>(clip.clipDurationMs);
        for (std::size_t lane = 0u; lane < kClipCurveLaneCount; ++lane) {
            const std::size_t count = clip.clipPointCounts[lane];
            if (count == 0u) continue;
            const auto first = clip.clipPoints.begin()
                + clip.clipPointStarts[lane];
            const auto last = first + count;
            const auto next = std::upper_bound(first, last, position,
                [](double time, const ClipPoint& point) {
                    return time < static_cast<double>(point.timeMs);
                });
            const bool hasPrevious = next != first;
            const bool hasNext = next != last;
            const ClipPoint& before = hasPrevious ? *(next - 1) : *(last - 1);
            const ClipPoint& after = hasNext ? *next : *first;
            double beforeTime = static_cast<double>(before.timeMs);
            double afterTime = static_cast<double>(after.timeMs);
            double phase = position;
            if (!hasPrevious) beforeTime -= duration;
            if (!hasNext) afterTime += duration;
            if (phase < beforeTime) phase += duration;
            const float mix = afterTime > beforeTime
                ? static_cast<float>(clampValue(
                    (phase - beforeTime) / (afterTime - beforeTime), 0.0, 1.0))
                : 0.0f;
            const float value = before.value
                + (after.value - before.value) * mix;
            if (lane < kLockParameterCount) {
                applyLockedValue(clipControls, clipOutputGain, lane, value);
            } else {
                const int pad = static_cast<int>(lane - kLockParameterCount);
                for (auto& voice : voices) {
                    if (voice.active && voice.clipOwned
                        && voice.sourcePad == pad) {
                        voice.laneOffset = clampValue(value, 0.0f, 1.0f);
                    }
                }
            }
        }
    }

    void beginClipRecording()
    {
        ClipSlot* clip = activeClipSlot();
        if (!clip || clipPreparationInFlight()) {
            setError("Wait for the queued clip to finish preparing");
            return;
        }
        const bool overdub = clipRecordMode == ClipRecordMode::overdub
            && clip->clipRecorded
            && clip->clipDurationMs >= kClipMinimumDurationMs;
        if (overdub && clip->takeCount >= kClipTakeLimit) {
            setError("Performance take limit reached");
            return;
        }
        clearError();
        if (!overdub) pushClipUndo(activeClip);
        chainEnabled = false;
        chainNextSlot = -1;
        clipReplacing = !overdub;
        clipRecordingTake = overdub ? clip->takeCount : 0u;
        if (!overdub) {
            clip->clipNoteCount = 0u;
            clip->clipPointCount = 0u;
            clip->takeCount = 0u;
            clip->noteCapacityReached = false;
            clip->pointCapacityReached = false;
            clip->clipPointStarts.fill(0u);
            clip->clipPointCounts.fill(0u);
            clip->clipDurationMs = 0u;
            clip->clipRecorded = false;
            for (std::size_t parameter = 0u;
                 parameter < kLockParameterCount; ++parameter) {
                clip->baseValues[parameter] = lockedValue(
                    controls, outputGain, parameter);
            }
            clipPlaying = false;
            clipPositionMs = 0.0;
            clipAbsolutePositionMs = 0.0;
            releaseClipVoices();
        } else if (!clipPlaying) {
            clipPlaying = true;
            clipNeedsStart = true;
        }
        clipOpenNoteIndices.fill(-1);
        clipLastPressureTimes.fill(0xffffffffu);
        clipLastPressureValues.fill(-1.0f);
        clipLastPointIndices.fill(-1);
        // Each overdub owns its gesture points. Do not seed thinning from a
        // previous take or a new pass could mutate data that Undo Take must
        // preserve.
        clipRecordStartNoteCount = clip->clipNoteCount;
        clipRecordStartPointCount = clip->clipPointCount;
        clipRecordStartSample = renderClockSamples.load(
            std::memory_order_relaxed);
        clipRecording = true;
        // A performance may already be underway when Record is pressed. Seed
        // those held free-play voices at the record origin/current overdub
        // phase so their later note-offs close valid captured notes.
        for (const auto& voice : voices) {
            if (!voice.active || !voice.held || voice.clipOwned) continue;
            captureClipNoteOn(voice.note, voice.velocity, voice.sourcePad);
            if (voice.sourcePad >= 0 && voice.sourcePad < 32) {
                captureClipPoint(ClipPointType::pressure, 0u,
                                 voice.sourcePad, voice.laneOffset);
            }
        }
        clipFinishPending = false;
        clip->occupied = true;
        activeClipOccupied = true;
        markPersistentStateDirty();
    }

    void finishClipRecording()
    {
        ClipSlot* clip = activeClipSlot();
        if (!clipRecording || !clip) return;
        for (std::size_t key = 0u; key < clipOpenNoteIndices.size(); ++key) {
            const int index = clipOpenNoteIndices[key];
            if (index < 0 || index >= static_cast<int>(clip->clipNoteCount)) continue;
            const double elapsed = std::max(1.0,
                clipCaptureAbsoluteMs() - clipOpenNoteStarts[key]);
            clip->clipNotes[static_cast<std::size_t>(index)].durationMs =
                static_cast<std::uint32_t>(clampValue(
                    elapsed, 1.0, static_cast<double>(kClipMaximumDurationMs)));
        }
        clipOpenNoteIndices.fill(-1);
        if (clipReplacing
            || clip->clipDurationMs < kClipMinimumDurationMs) {
            clip->clipDurationMs = static_cast<std::uint32_t>(clampValue(
                captureElapsedMs(),
                static_cast<double>(kClipMinimumDurationMs),
                static_cast<double>(kClipMaximumDurationMs)));
            clip->recordedTempoBpm = tempoBpm;
            clip->recordedBeats = static_cast<float>(
                static_cast<double>(clip->clipDurationMs)
                * static_cast<double>(tempoBpm) / 60000.0);
        }
        const std::uint32_t duration = clip->clipDurationMs;
        for (std::size_t index = 0u; index < clip->clipNoteCount; ++index) {
            ClipNote& note = clip->clipNotes[index];
            note.startMs %= duration;
            note.durationMs = clampValue(note.durationMs, 1u, duration);
        }
        for (std::size_t index = 0u; index < clip->clipPointCount; ++index) {
            clip->clipPoints[index].timeMs %= duration;
        }
        std::sort(clip->clipNotes.begin(),
                  clip->clipNotes.begin() + clip->clipNoteCount,
                  [](const ClipNote& left, const ClipNote& right) {
                      return left.startMs < right.startMs;
                  });
        rebuildClipPointIndex(*clip);
        clip->clipRecorded = true;
        const bool passHasContent = clip->clipNoteCount > clipRecordStartNoteCount
            || clip->clipPointCount > clipRecordStartPointCount;
        if (clipReplacing) {
            clip->takeCount = 1u;
        } else if (passHasContent) {
            clip->takeCount = static_cast<std::uint8_t>(clipRecordingTake + 1u);
        }
        clip->occupied = true;
        clipRecording = false;
        clipFinishPending = false;
        clipPlaying = true;
        if (clipReplacing) {
            clipPositionMs = 0.0;
            clipAbsolutePositionMs = 0.0;
            clipNeedsStart = true;
        }
        markPersistentStateDirty();
    }

    void toggleClipRecording()
    {
        if (clipRecording) finishClipRecording();
        else beginClipRecording();
    }

    void clearClipSlot(int index)
    {
        if (index < 0 || index >= static_cast<int>(clipSlots.size())
            || clipRecording) return;
        pushClipUndo(index);
        ClipSlot& clip = clipSlots[static_cast<std::size_t>(index)];
        clip.clipNoteCount = 0u;
        clip.clipPointCount = 0u;
        clip.clipPointStarts.fill(0u);
        clip.clipPointCounts.fill(0u);
        clip.clipDurationMs = 0u;
        clip.clipRecorded = false;
        clip.takeCount = 0u;
        clip.noteCapacityReached = false;
        clip.pointCapacityReached = false;
        if (index == queuedClip) {
            queuedClip = -1;
            chainNextSlot = -1;
        }
        bool ownsSamples = false;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if (clip.laneSourceKinds[lane] != LaneSourceKind::empty) {
                ownsSamples = true;
                break;
            }
        }
        clip.occupied = ownsSamples;
        if (index == activeClip) {
            activeClipOccupied = clip.occupied;
            clipPositionMs = 0.0;
            clipAbsolutePositionMs = 0.0;
            clipPlaying = false;
            releaseClipVoices();
        }
        markPersistentStateDirty();
    }

    void clearActiveClip() { clearClipSlot(activeClip); }

    int nextEnabledChainSlot(int after) const
    {
        for (std::size_t offset = 1u; offset <= clipChain.size(); ++offset) {
            const int slot = (after + static_cast<int>(offset))
                % static_cast<int>(clipChain.size());
            if (clipChain[static_cast<std::size_t>(slot)].enabled) return slot;
        }
        return -1;
    }

    int firstEnabledChainSlot() const
    {
        for (std::size_t slot = 0u; slot < clipChain.size(); ++slot) {
            if (clipChain[slot].enabled) return static_cast<int>(slot);
        }
        return -1;
    }

    void prepareNextChainClip()
    {
        if (!chainEnabled) return;
        const int first = firstEnabledChainSlot();
        if (first < 0) return;
        if (chainPosition < 0
            || chainPosition >= static_cast<int>(clipChain.size())
            || !clipChain[static_cast<std::size_t>(chainPosition)].enabled) {
            chainPosition = first;
            chainRepeat = 0;
        }
        const int next = nextEnabledChainSlot(chainPosition);
        if (next < 0) return;
        chainNextSlot = next;
        const int target = clipChain[static_cast<std::size_t>(next)].clip;
        queuedClip = target;
        requestClipPreparation(target);
    }

    void advanceClipChain()
    {
        if (!chainEnabled) return;
        const int first = firstEnabledChainSlot();
        if (first < 0) return;
        if (chainPosition < 0
            || chainPosition >= static_cast<int>(clipChain.size())
            || !clipChain[static_cast<std::size_t>(chainPosition)].enabled) {
            chainPosition = first;
            chainRepeat = 0;
        }
        if (chainNextSlot < 0) prepareNextChainClip();
        const ClipChainEntry& current =
            clipChain[static_cast<std::size_t>(chainPosition)];
        ++chainRepeat;
        if (chainRepeat < std::max(1, static_cast<int>(current.repeats))) return;
        if (chainNextSlot < 0) return;
        const int target = clipChain[
            static_cast<std::size_t>(chainNextSlot)].clip;
        const auto prepareState = static_cast<ClipPrepareState>(
            clipPrepareState.load(std::memory_order_acquire));
        if (prepareState != ClipPrepareState::ready
            || preparedClip.load(std::memory_order_relaxed) != target) {
            requestClipPreparation(target);
            return;
        }
        chainPosition = chainNextSlot;
        chainNextSlot = -1;
        chainRepeat = 0;
        activatePreparedClip();
        prepareNextChainClip();
    }

    void serviceClipBeforeBlock()
    {
        auto prepareState = static_cast<ClipPrepareState>(
            clipPrepareState.load(std::memory_order_acquire));
        if (queuedClip >= 0
            && prepareState == ClipPrepareState::ready
            && preparedClip.load(std::memory_order_relaxed)
                != queuedClip) {
            preparedClip.store(-1, std::memory_order_relaxed);
            clipPrepareState.store(static_cast<int>(ClipPrepareState::idle),
                                      std::memory_order_release);
            prepareState = ClipPrepareState::idle;
        }
        if (queuedClip >= 0
            && prepareState == ClipPrepareState::idle) {
            requestClipPreparation(queuedClip);
            prepareState = ClipPrepareState::requested;
        }
        if (!clipPlaying && queuedClip >= 0
            && prepareState == ClipPrepareState::ready
            && preparedClip.load(std::memory_order_relaxed)
                == queuedClip) {
            activatePreparedClip();
        } else if (queuedClip >= 0
                   && prepareState == ClipPrepareState::error) {
            setError("Queued clip sample preparation failed");
        }
        if (!clipPlaying) return;
        ClipSlot* clip = activeClipSlot();
        if (!clip || !clip->clipRecorded
            || clip->clipDurationMs < kClipMinimumDurationMs) {
            if (queuedClip >= 0
                && prepareState == ClipPrepareState::ready
                && preparedClip.load(std::memory_order_relaxed)
                    == queuedClip) {
                activatePreparedClip();
                clipPositionMs = 0.0;
                clipAbsolutePositionMs = 0.0;
                clipNeedsStart = true;
            }
            return;
        }
        applyClipCurves(*clip, clipPositionMs);
        if (clipNeedsStart) {
            processClipEvents(*clip, 0.0, 0.0, true);
            clipNeedsStart = false;
        }
    }

    void serviceClipAfterBlock(std::size_t frames)
    {
        renderClockSamples.fetch_add(frames, std::memory_order_relaxed);
        const double previousBeat = transportBeats;
        transportBeats += static_cast<double>(frames)
            / static_cast<double>(std::max(1.0f, sampleRate))
            * static_cast<double>(tempoBpm) / 60.0;
        if (clipLaunchPending) {
            const double quantum = launchQuantize == LaunchQuantize::bar
                ? 4.0 : 1.0;
            if (std::floor(previousBeat / quantum)
                < std::floor(transportBeats / quantum)) {
                clipLaunchPending = false;
                startClipPlaybackNow();
                return;
            }
        }
        if (clipRecording && clipReplacing
            && captureElapsedMs() >= static_cast<double>(kClipMaximumDurationMs)) {
            clipFinishPending = true;
        }
        if (!clipPlaying) return;
        ClipSlot* clip = activeClipSlot();
        if (!clip || !clip->clipRecorded
            || clip->clipDurationMs < kClipMinimumDurationMs) return;
        const double duration = static_cast<double>(clip->clipDurationMs);
        double rate = clampValue(
            static_cast<double>(clip->clipRate), 0.25, 4.0);
        if (clip->timingMode == ClipTimingMode::beatSync
            && clip->recordedTempoBpm > 0.0f) {
            rate *= static_cast<double>(tempoBpm)
                / static_cast<double>(clip->recordedTempoBpm);
        }
        const double advance = static_cast<double>(frames) * 1000.0
            / static_cast<double>(std::max(1.0f, sampleRate)) * rate;
        const double previous = clipPositionMs;
        double next = previous + advance;
        clipAbsolutePositionMs += advance;
        if (next < duration) {
            processClipEvents(*clip, previous, next);
            clipPositionMs = next;
            return;
        }
        processClipEvents(*clip, previous, duration);
        const int beforeClip = activeClip;
        if (chainEnabled && firstEnabledChainSlot() >= 0) {
            advanceClipChain();
        } else if (queuedClip >= 0) {
            const auto prepareState = static_cast<ClipPrepareState>(
                clipPrepareState.load(std::memory_order_acquire));
            if (prepareState == ClipPrepareState::ready
                && preparedClip.load(std::memory_order_relaxed)
                    == queuedClip) {
                activatePreparedClip();
            } else if (prepareState == ClipPrepareState::idle
                       || prepareState == ClipPrepareState::error) {
                requestClipPreparation(queuedClip);
            }
        }
        if (activeClip != beforeClip) {
            releaseClipVoices();
            clipPositionMs = 0.0;
            clipAbsolutePositionMs = 0.0;
            clipNeedsStart = true;
            return;
        }
        next = std::fmod(next, duration);
        clipPositionMs = next;
        processClipEvents(*clip, 0.0, next, true);
    }

    void startClipPlaybackNow()
    {
        clipPlaying = true;
        releaseClipVoices();
        clipPositionMs = 0.0;
        clipAbsolutePositionMs = 0.0;
        clipNeedsStart = true;
        if (chainEnabled) {
            const int first = firstEnabledChainSlot();
            if (first >= 0) {
                chainPosition = first;
                chainRepeat = 0;
                const int target = clipChain[static_cast<std::size_t>(first)].clip;
                if (target != activeClip) {
                    chainNextSlot = first;
                    chainRepeat = std::max(1, static_cast<int>(
                        clipChain[static_cast<std::size_t>(first)].repeats)) - 1;
                    queuedClip = target;
                    requestClipPreparation(target);
                } else {
                    chainNextSlot = -1;
                    prepareNextChainClip();
                }
            }
        }
    }

    void setClipPlaying(bool running)
    {
        if (!running) {
            if (!clipPlaying && !clipLaunchPending) return;
            clipLaunchPending = false;
            clipPlaying = false;
            releaseClipVoices();
            clipPositionMs = 0.0;
            clipAbsolutePositionMs = 0.0;
            clipNeedsStart = false;
            chainNextSlot = -1;
            return;
        }
        if (clipPlaying || clipLaunchPending) return;
        if (launchQuantize == LaunchQuantize::off) {
            startClipPlaybackNow();
        } else {
            clipLaunchPending = true;
            releaseClipVoices();
            clipPositionMs = 0.0;
            clipAbsolutePositionMs = 0.0;
            clipNeedsStart = false;
        }
    }

    bool workingClipHasContent() const
    {
        const ClipSlot* clip = activeClipSlot();
        return clip && clip->clipRecorded
            && clip->clipDurationMs >= kClipMinimumDurationMs;
    }

    void markPersistentStateDirty() { persistentStateDirty = true; }

    void pushClipUndo(int index)
    {
        if (index < 0 || index >= static_cast<int>(clipSlots.size())) return;
        if (index == activeClip) captureActiveClip(false);
        undoClip = clipSlots[static_cast<std::size_t>(index)];
        undoClipIndex = index;
        undoAvailable = true;
    }

    void restoreUndo()
    {
        if (!undoAvailable || undoClipIndex < 0
            || undoClipIndex >= static_cast<int>(clipSlots.size())) return;
        if (clipPreparationInFlight()) {
            setError("Wait for the queued clip to finish preparing");
            return;
        }
        clipSlots[static_cast<std::size_t>(undoClipIndex)] = undoClip;
        if (activeClip == undoClipIndex) activateStoredClip(activeClip);
        undoAvailable = false;
        markPersistentStateDirty();
    }

    void undoLastClipTake()
    {
        ClipSlot* clip = activeClipSlot();
        if (!clip || clipRecording) {
            setError("Stop performance recording before undoing a take");
            return;
        }
        if (clipPreparationInFlight()) {
            setError("Wait for the queued clip to finish preparing");
            return;
        }
        if (!clip->clipRecorded || clip->takeCount <= 1u) {
            setError("No overdub take to undo");
            return;
        }
        clearError();
        const std::uint8_t removedTake = static_cast<std::uint8_t>(
            clip->takeCount - 1u);
        releaseClipVoices();
        std::size_t noteWrite = 0u;
        for (std::size_t index = 0u; index < clip->clipNoteCount; ++index) {
            if (clip->clipNotes[index].take == removedTake) continue;
            if (noteWrite != index) {
                clip->clipNotes[noteWrite] = clip->clipNotes[index];
            }
            ++noteWrite;
        }
        std::size_t pointWrite = 0u;
        for (std::size_t index = 0u; index < clip->clipPointCount; ++index) {
            if (clipPointTake(clip->clipPoints[index]) == removedTake) continue;
            if (pointWrite != index) {
                clip->clipPoints[pointWrite] = clip->clipPoints[index];
            }
            ++pointWrite;
        }
        clip->clipNoteCount = static_cast<std::uint16_t>(noteWrite);
        clip->clipPointCount = static_cast<std::uint16_t>(pointWrite);
        --clip->takeCount;
        clip->noteCapacityReached = false;
        clip->pointCapacityReached = false;
        rebuildClipPointIndex(*clip);
        markPersistentStateDirty();
    }

    void captureActiveClip(bool forceOccupied)
    {
        if (activeClip < 0 || activeClip >= static_cast<int>(clipSlots.size())) {
            return;
        }
        ClipSlot& clip = clipSlots[static_cast<std::size_t>(activeClip)];
        for (std::size_t index = 0u; index < kLockParameterCount; ++index) {
            clip.baseValues[index] = lockedValue(controls, outputGain, index);
        }
        clip.occupied = forceOccupied || activeClipOccupied
            || workingClipHasContent();
        activeClipOccupied = clip.occupied;
        clipBankInitialized = true;
    }

    void activateStoredClip(int index, bool queueSources = true)
    {
        if (index < 0 || index >= static_cast<int>(clipSlots.size())) return;
        ClipSlot& target = clipSlots[static_cast<std::size_t>(index)];
        activeClip = index;
        activeClipOccupied = target.occupied;
        for (std::size_t parameter = 0u;
             parameter < kLockParameterCount; ++parameter) {
            applyLockedValue(controls, outputGain, parameter,
                             target.baseValues[parameter]);
        }
        clipControls = controls;
        clipOutputGain = outputGain;
        clipPositionMs = 0.0;
        clipAbsolutePositionMs = 0.0;
        clipNeedsStart = clipPlaying;
        if (queueSources) queueClipSources(index);
        queuedClip = -1;
    }

    bool activatePreparedClip()
    {
        if (clipPrepareState.load(std::memory_order_acquire)
                != static_cast<int>(ClipPrepareState::ready)
            || preparedClip.load(std::memory_order_relaxed) < 0
            || preparedClip.load(std::memory_order_relaxed)
                >= static_cast<int>(clipSlots.size())
            || engine.recording()) return false;
        allNotesOff();
        storage.swap(preparedClipStorage);
        if (!engine.rebindStorage(storage.get() + planeCapacityFrames * 0u,
                                  storage.get() + planeCapacityFrames * 1u,
                                  storage.get() + planeCapacityFrames * 2u,
                                  storage.get() + planeCapacityFrames * 3u,
                                  planeCapacityFrames)) {
            storage.swap(preparedClipStorage);
            clipPrepareState.store(
                static_cast<int>(ClipPrepareState::error),
                std::memory_order_release);
            setError("Prepared clip bank could not be activated");
            return false;
        }
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if (preparedLaneFrames[lane] != 0u) {
                engine.adoptLane(lane, preparedLaneFrames[lane],
                                 preparedLaneRates[lane]);
                activeRegions[lane] = preparedRegions[lane];
                engine.setLaneRegions(lane,
                    activeRegions[lane].boundaries.data(),
                    std::max<std::size_t>(1u, activeRegions[lane].count));
                ++laneRegionRevisions[lane];
            } else {
                activeRegions[lane] = {};
            }
            waveMinimum[lane] = preparedWaveMinimum[lane];
            waveMaximum[lane] = preparedWaveMaximum[lane];
            currentLaneSourceKinds[lane] = preparedLaneKinds[lane];
            std::snprintf(currentLaneSourcePaths[lane].data(),
                          currentLaneSourcePaths[lane].size(), "%s",
                          preparedLanePaths[lane].data());
            std::snprintf(laneSourceLabels[lane].data(),
                          laneSourceLabels[lane].size(), "%s",
                          preparedLaneLabels[lane][0] != '\0'
                              ? preparedLaneLabels[lane].data()
                              : (preparedLaneFrames[lane] != 0u
                                  ? "SAMPLE" : "EMPTY"));
        }
        for (auto& voice : voices) {
            voice.engine.rebindStorage(
                storage.get() + planeCapacityFrames * 0u,
                storage.get() + planeCapacityFrames * 1u,
                storage.get() + planeCapacityFrames * 2u,
                storage.get() + planeCapacityFrames * 3u,
                planeCapacityFrames);
            for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                if (preparedLaneFrames[lane] != 0u) {
                    voice.engine.adoptLane(lane, preparedLaneFrames[lane],
                                           preparedLaneRates[lane]);
                    voice.engine.setLaneRegions(lane,
                        preparedRegions[lane].boundaries.data(),
                        std::max<std::size_t>(1u, preparedRegions[lane].count));
                    voice.regionRevisions[lane] = laneRegionRevisions[lane];
                }
            }
            voice.engine.stopPlayback();
        }
        pendingClearMask = 0u;
        waveDirtyMask = 0u;
        scanLane = -1;
        waveTextDirty = true;
        const int target = preparedClip.load(std::memory_order_relaxed);
        preparedClip.store(-1, std::memory_order_relaxed);
        clipPrepareState.store(static_cast<int>(ClipPrepareState::idle),
                                  std::memory_order_release);
        activateStoredClip(target, false);
        return true;
    }

    void copyCurrentSourcesToClip(ClipSlot& target)
    {
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            target.laneSourceKinds[lane] = currentLaneSourceKinds[lane];
            std::snprintf(target.laneSourcePaths[lane].data(),
                          target.laneSourcePaths[lane].size(), "%s",
                          currentLaneSourcePaths[lane].data());
            std::snprintf(target.laneSourceLabels[lane].data(),
                          target.laneSourceLabels[lane].size(), "%s",
                          laneSourceLabels[lane].data());
        }
    }

    void loadClip(int index)
    {
        if (index < 0 || index >= static_cast<int>(clipSlots.size())) return;
        captureActiveClip(false);
        ClipSlot& target = clipSlots[static_cast<std::size_t>(index)];
        if (!target.occupied) {
            target = ClipSlot {};
            for (std::size_t parameter = 0u;
                 parameter < kLockParameterCount; ++parameter) {
                target.baseValues[parameter] = lockedValue(
                    controls, outputGain, parameter);
            }
            copyCurrentSourcesToClip(target);
            target.occupied = true;
        }
        activateStoredClip(index);
        markPersistentStateDirty();
    }

    void initializeClipIfNeeded(int index)
    {
        if (index < 0 || index >= static_cast<int>(clipSlots.size())) return;
        ClipSlot& target = clipSlots[static_cast<std::size_t>(index)];
        if (target.occupied) return;
        target = ClipSlot {};
        for (std::size_t parameter = 0u;
             parameter < kLockParameterCount; ++parameter) {
            target.baseValues[parameter] = lockedValue(
                controls, outputGain, parameter);
        }
        copyCurrentSourcesToClip(target);
        target.occupied = true;
        markPersistentStateDirty();
    }

    void selectClip(int index)
    {
        if (index < 0 || index >= static_cast<int>(clipSlots.size())) return;
        if (clipRecording) {
            setError("Stop performance recording before changing clips");
            return;
        }
        if (index == activeClip) {
            queuedClip = -1;
            return;
        }
        if (chainEnabled) {
            chainEnabled = false;
            chainNextSlot = -1;
        }
        if (pendingCaptureSaveMask != 0u && !serviceMaintenance()) {
            setError("Clip change is waiting for capture save");
            return;
        }
        captureActiveClip(false);
        queuedClip = index;
        requestClipPreparation(index);
    }

    bool clipPreparationBusy(int index) const
    {
        const auto state = static_cast<ClipPrepareState>(
            clipPrepareState.load(std::memory_order_acquire));
        return index == clipPrepareRequest.load(std::memory_order_relaxed)
            && (state == ClipPrepareState::requested
                || state == ClipPrepareState::loading);
    }

    void copyActiveClip(int destination)
    {
        if (destination < 0
            || destination >= static_cast<int>(clipSlots.size())) return;
        if (clipPreparationInFlight()) {
            setError("Queued clip is still preparing");
            return;
        }
        captureActiveClip(true);
        pushClipUndo(destination);
        clipSlots[static_cast<std::size_t>(destination)] =
            clipSlots[static_cast<std::size_t>(activeClip)];
        clipSlots[static_cast<std::size_t>(destination)].occupied = true;
        markPersistentStateDirty();
    }

    void resetClip(int index)
    {
        if (index < 0 || index >= static_cast<int>(clipSlots.size())) return;
        if (clipRecording) {
            setError("Stop performance recording before clearing a clip");
            return;
        }
        if (clipPreparationInFlight()) {
            setError("Queued clip is still preparing");
            return;
        }
        pushClipUndo(index);
        clipSlots[static_cast<std::size_t>(index)] = ClipSlot {};
        if (queuedClip == index) queuedClip = -1;
        if (activeClip == index) {
            activeClipOccupied = false;
            clipPlaying = false;
            clipPositionMs = 0.0;
            clipAbsolutePositionMs = 0.0;
            clipNeedsStart = false;
            releaseClipVoices();
            queueClipSources(index);
        }
        markPersistentStateDirty();
    }

    std::uint64_t occupiedClipMask() const
    {
        std::uint64_t mask = 0u;
        for (std::size_t index = 0u; index < clipSlots.size(); ++index) {
            if (clipSlots[index].occupied) mask |= std::uint64_t { 1u } << index;
        }
        return mask;
    }

    void setChainEntry(int slot, int clip, int repeats)
    {
        if (slot < 0 || slot >= static_cast<int>(clipChain.size())) return;
        auto& entry = clipChain[static_cast<std::size_t>(slot)];
        entry.clip = static_cast<std::uint8_t>(clampValue(
            clip, 0, static_cast<int>(kClipSlotCount) - 1));
        entry.repeats = static_cast<std::uint8_t>(clampValue(repeats, 1, 16));
        entry.enabled = true;
        markPersistentStateDirty();
    }

    void clearChainEntry(int slot)
    {
        if (slot < 0 || slot >= static_cast<int>(clipChain.size())) return;
        clipChain[static_cast<std::size_t>(slot)] = ClipChainEntry {};
        if (chainPosition == slot) chainRepeat = 0;
        markPersistentStateDirty();
    }

    void insertChainEntry(int slot, int clip)
    {
        if (slot < 0 || slot >= static_cast<int>(clipChain.size())) return;
        for (std::size_t index = clipChain.size() - 1u;
             index > static_cast<std::size_t>(slot); --index) {
            clipChain[index] = clipChain[index - 1u];
        }
        setChainEntry(slot, clip, 1);
        chainEditSlot = slot;
    }

    void deleteChainEntry(int slot)
    {
        if (slot < 0 || slot >= static_cast<int>(clipChain.size())) return;
        for (std::size_t index = static_cast<std::size_t>(slot);
             index + 1u < clipChain.size(); ++index) {
            clipChain[index] = clipChain[index + 1u];
        }
        clipChain.back() = ClipChainEntry {};
        if (chainPosition > slot) --chainPosition;
        else if (chainPosition == slot) chainRepeat = 0;
        chainEditSlot = std::min(chainEditSlot,
            static_cast<int>(clipChain.size()) - 1);
        markPersistentStateDirty();
    }

    void moveChainEntry(int source, int destination)
    {
        if (source < 0 || destination < 0
            || source >= static_cast<int>(clipChain.size())
            || destination >= static_cast<int>(clipChain.size())
            || source == destination) return;
        const ClipChainEntry moved = clipChain[static_cast<std::size_t>(source)];
        if (source < destination) {
            for (int index = source; index < destination; ++index) {
                clipChain[static_cast<std::size_t>(index)] =
                    clipChain[static_cast<std::size_t>(index + 1)];
            }
        } else {
            for (int index = source; index > destination; --index) {
                clipChain[static_cast<std::size_t>(index)] =
                    clipChain[static_cast<std::size_t>(index - 1)];
            }
        }
        clipChain[static_cast<std::size_t>(destination)] = moved;
        chainEditSlot = destination;
        markPersistentStateDirty();
    }

    int formatClipChain(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        for (std::size_t slot = 0u; slot < clipChain.size(); ++slot) {
            const auto& entry = clipChain[slot];
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                slot == 0u ? "%u:%u:%u" : ",%u:%u:%u",
                entry.enabled ? 1u : 0u, static_cast<unsigned>(entry.clip),
                static_cast<unsigned>(entry.repeats));
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    const PlaybackVoice* displayVoice() const
    {
        const PlaybackVoice* newest = nullptr;
        for (const auto& voice : voices) {
            if (voice.active && voice.held
                && (!newest || voice.age > newest->age)) newest = &voice;
        }
        return newest;
    }

    int heldVoiceCount() const
    {
        int count = 0;
        for (const auto& voice : voices) {
            if (voice.active && voice.held) ++count;
        }
        return count;
    }

    int formatVoicePositions(char* buffer, int bufferLength, bool lanePosition) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        for (std::size_t index = 0u; index < voices.size(); ++index) {
            const auto& voice = voices[index];
            int value = -1;
            if (voice.active && voice.held) {
                const float normalized = lanePosition
                    ? voice.engine.normalizedLanePosition()
                    : voice.engine.normalizedPosition();
                value = static_cast<int>(std::lrintf(normalized * 1000.0f));
            }
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                index == 0u ? "%d" : ",%d", value);
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    int formatVoicePads(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        for (std::size_t index = 0u; index < voices.size(); ++index) {
            const auto& voice = voices[index];
            const int pad = voice.active && voice.held ? voice.sourcePad : -1;
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                index == 0u ? "%d" : ",%d", pad);
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    int formatRegionState(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1 || selectedLane >= kLaneCount) return -1;
        const RegionMap& map = activeRegions[selectedLane];
        int position = std::snprintf(buffer, static_cast<std::size_t>(bufferLength),
                                     "%zu", map.count);
        if (position < 0 || position >= bufferLength) return -1;
        for (std::size_t index = 0u; index <= map.count; ++index) {
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                ",%d", static_cast<int>(std::lround(
                    clampValue(map.boundaries[index], 0.0f, 1.0f) * 1000.0f)));
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    int formatCueState(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1 || activeClip < 0
            || activeClip >= static_cast<int>(clipSlots.size())) return -1;
        const auto& cues = clipSlots[static_cast<std::size_t>(activeClip)].cues;
        int position = 0;
        for (std::size_t index = 0u; index < cues.size(); ++index) {
            const int value = cues[index] < 0.0f ? -1
                : static_cast<int>(std::lround(cues[index] * 1000.0f));
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                index == 0u ? "%d" : ",%d", value);
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    void applyMidiCc(int cc, int value)
    {
        value = clampValue(value, 0, 127);
        switch (cc) {
            case 20: controls.locus = midiRange(value, 0.0f, 1.0f); break;
            case 21: controls.field = midiRange(value, 0.02f, 1.0f); break;
            case 22: controls.speed = midiRange(value, 0.05f, 4.0f); break;
            case 23: controls.manualLane = midiRange(value, 0.0f, 3.0f); break;
            case 24: controls.laneCycles = midiRange(value, 0.125f, 16.0f); break;
            case 25: controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(
                midiChoice(value, static_cast<int>(Engine::kPlaybackAlgorithmCount))); break;
            case 26: controls.join = midiUnit(value); break;
            case 27: outputGain = midiRange(value, 0.0f, 2.0f); break;
            case 28: controls.motion = static_cast<Engine::MotionMode>(
                midiChoice(value, static_cast<int>(Engine::kMotionModeCount))); break;
            case 29: controls.motionSound = static_cast<Engine::MotionSound>(
                midiChoice(value, static_cast<int>(Engine::kMotionSoundCount))); break;
            case 30: controls.motionSoundRateHz = midiRange(value, 0.25f, 20.0f); break;
            case 31: controls.lanes = static_cast<Engine::LanesMode>(
                midiChoice(value, static_cast<int>(Engine::kLanesModeCount))); break;
            case 32: controls.lanePath = static_cast<Engine::LanePath>(
                midiChoice(value, static_cast<int>(Engine::kLanePathCount))); break;
            case 33: controls.lanesStretch = value >= 64; break;
            case 34: controls.lanesStretchRatio = midiRange(value, 0.25f, 4.0f); break;
            case 35: controls.laneGains[0u] = midiRange(value, 0.0f, 2.0f); break;
            case 36: controls.laneGains[1u] = midiRange(value, 0.0f, 2.0f); break;
            case 37: controls.laneGains[2u] = midiRange(value, 0.0f, 2.0f); break;
            case 38: controls.laneGains[3u] = midiRange(value, 0.0f, 2.0f); break;
            case 39: controls.grains = static_cast<Engine::GrainMode>(
                midiChoice(value, static_cast<int>(Engine::kGrainModeCount))); break;
            case 40: controls.grainSizeMs = midiRange(value, 8.0f, 1000.0f); break;
            case 41: controls.grainDensityHz = midiRange(value, 1.0f, 96.0f); break;
            case 42: controls.grainPitch = midiRange(value, 0.25f, 4.0f); break;
            case 43: controls.grainWindow = static_cast<Engine::GrainWindow>(
                midiChoice(value, static_cast<int>(Engine::kGrainWindowCount))); break;
            case 44: controls.grainStereoSpread = midiUnit(value); break;
            case 45: controls.cutups = static_cast<Engine::CutupsMode>(
                midiChoice(value, static_cast<int>(Engine::kCutupsModeCount))); break;
            case 46: pressureMode = value >= 64 ? 1 : 0; break;
            case 47: rootNote = static_cast<int>(std::lround(midiRange(value, 24.0f, 96.0f))); break;
            case 48:
                if (!engine.recording()) selectedLane = static_cast<std::size_t>(
                    midiChoice(value, static_cast<int>(kLaneCount)));
                break;
            case 49:
                if (!engine.recording()) recordSource = value >= 64 ? 1 : 0;
                break;
            case 50: recordGain = midiRange(value, 0.0f, 4.0f); break;
            case 51: monitorLevel = midiUnit(value); break;
            case 52:
                if (value >= 64) startRecording();
                else finishRecording();
                break;
            case 53:
                if (value >= 64) clearSelectedLane();
                break;
            case 54: tempoBpm = midiRange(value, 40.0f, 240.0f); break;
            case 55: tempoSwing = midiRange(value, 50.0f, 75.0f) / 100.0f; break;
            case 56: setClipPlaying(value >= 64); break;
            case 57: if (value >= 64) restoreUndo(); break;
            case 58: if (value >= 64) clearChainEntry(chainEditSlot); break;
            case 59: controls.motionRateBasis =
                value >= 64 ? Engine::MotionRateBasis::hertz
                    : Engine::MotionRateBasis::normal; break;
            case 60: controls.motionPathRate = midiRange(value, 0.01f, 80.0f); break;
            case 61: controls.motionStep = midiUnit(value); break;
            case 62: controls.motionSeed = 1.0f
                + static_cast<float>(midiChoice(value, 9999)); break;
            case 63: controls.segmentTrigger = static_cast<Engine::SegmentTrigger>(
                midiChoice(value, static_cast<int>(Engine::kSegmentTriggerCount))); break;
            case 64: controls.segmentOverlap = value >= 64
                ? Engine::SegmentOverlap::layer
                : Engine::SegmentOverlap::cut; break;
            case 65: chainEnabled = value >= 64; break;
            case 66: chainEditSlot = midiChoice(
                value, static_cast<int>(kClipChainLength)); break;
            case 67: setChainEntry(chainEditSlot, midiChoice(
                value, static_cast<int>(kClipSlotCount)),
                clipChain[static_cast<std::size_t>(chainEditSlot)].repeats); break;
            case 68: setChainEntry(chainEditSlot,
                clipChain[static_cast<std::size_t>(chainEditSlot)].clip,
                1 + midiChoice(value, 16)); break;
            case 69: controls.motionTravel = midiUnit(value); break;
            case 70: controls.motionJitter = midiUnit(value); break;
            case 71: controls.packetDuty = midiRange(value, 0.02f, 0.98f); break;
            case 72: controls.motorRateHz = midiRange(value, 0.05f, 20.0f); break;
            case 73: controls.motorSymmetry = midiRange(value, 0.05f, 0.95f); break;
            case 74: controls.motorShape = static_cast<Engine::MotorShape>(
                midiChoice(value, static_cast<int>(Engine::kMotorShapeCount))); break;
            case 75: controls.laneBlend = static_cast<Engine::LaneBlendMode>(
                midiChoice(value, static_cast<int>(Engine::kLaneBlendModeCount))); break;
            case 76: controls.lanePathShape = static_cast<Engine::LanePathShape>(
                midiChoice(value, static_cast<int>(Engine::kLanePathShapeCount))); break;
            case 77: controls.lanePathOffset = midiUnit(value); break;
            case 78: controls.lanePathSkew = midiRange(value, 0.02f, 0.98f); break;
            case 79: controls.lanePathCurve = midiRange(value, -1.0f, 1.0f); break;
            case 80: controls.laneSlewMs = midiRange(value, 0.0f, 500.0f); break;
            case 81: controls.grainSpray = midiUnit(value); break;
            case 82: controls.grainBias = static_cast<Engine::GrainBias>(
                midiChoice(value, static_cast<int>(Engine::kGrainBiasCount))); break;
            case 83: controls.grainPitchSpray = midiRange(value, 0.0f, 24.0f); break;
            case 84: controls.grainReverseChance = midiUnit(value); break;
            case 85: controls.grainSizeVariation = midiUnit(value); break;
            case 86: controls.grainLevelVariation = midiUnit(value); break;
            case 87: controls.grainTiming = static_cast<Engine::GrainTiming>(
                midiChoice(value, static_cast<int>(Engine::kGrainTimingCount))); break;
            case 88: controls.grainTimingScatter = midiUnit(value); break;
            case 89: controls.grainEnvelopeSkew = midiRange(value, -1.0f, 1.0f); break;
            case 90: controls.grainProcess = static_cast<Engine::GrainProcess>(
                midiChoice(value, static_cast<int>(Engine::kGrainProcessCount))); break;
            case 91: controls.grainProcessAmount = midiUnit(value); break;
            case 92: controls.grainRegionCount = 1.0f + static_cast<float>(
                midiChoice(value, 64)); break;
            case 93: controls.grainAdvance = static_cast<Engine::GrainAdvance>(
                midiChoice(value, static_cast<int>(Engine::kGrainAdvanceCount))); break;
            case 94: controls.cutClock = static_cast<Engine::CutClock>(
                midiChoice(value, static_cast<int>(Engine::kCutClockCount))); break;
            case 95: controls.cutDivision = static_cast<Engine::CutDivision>(
                midiChoice(value, static_cast<int>(Engine::kCutDivisionCount))); break;
            case 96: controls.cutRateHz = midiRange(value, 0.25f, 64.0f); break;
            case 97: controls.cutTimeVariation = midiUnit(value); break;
            case 98: controls.cutGate = midiRange(value, 0.05f, 1.0f); break;
            case 99: controls.cutJoinMs = midiRange(value, 0.0f, 100.0f); break;
            case 100: controls.cutRegionCount = 1.0f + static_cast<float>(
                midiChoice(value, 64)); break;
            case 101: controls.cutRepeats = 1.0f + static_cast<float>(
                midiChoice(value, 16)); break;
            case 102: controls.cutReverseChance = midiUnit(value); break;
            case 103: controls.cutPitchVariation = midiRange(value, 0.0f, 24.0f); break;
            case 104: controls.cutLevelVariation = midiUnit(value); break;
            case 105: controls.cutLaneOrder = static_cast<Engine::CutLaneOrder>(
                midiChoice(value, static_cast<int>(Engine::kCutLaneOrderCount))); break;
            case 106: controls.cutPolyPath = static_cast<Engine::CutPolyPath>(
                midiChoice(value, static_cast<int>(Engine::kCutPolyPathCount))); break;
            case 107: controls.cutRegionMode = static_cast<Engine::RegionMode>(
                midiChoice(value, static_cast<int>(Engine::kRegionModeCount))); break;
            case 108: controls.sampleMode = static_cast<Engine::SampleMode>(
                midiChoice(value, static_cast<int>(Engine::kSampleModeCount))); break;
            case 109: controls.sampleRegionMode = static_cast<Engine::RegionMode>(
                midiChoice(value, static_cast<int>(Engine::kRegionModeCount))); break;
            case 110: controls.sampleRegionCount = 1.0f + static_cast<float>(
                midiChoice(value, 64)); break;
            case 111: padMode = static_cast<PadMode>(midiChoice(value, 4)); break;
            case 112: laneVariation = static_cast<LaneVariation>(
                midiChoice(value, 5)); break;
            case 113: previewLevel = midiUnit(value); break;
            case 114: {
                ClipSlot* clip = activeClipSlot();
                if (clip) clip->clipRate = 0.25f
                    + midiUnit(value) * 3.75f;
                break;
            }
            case 115: clipRecordMode = value >= 64
                ? ClipRecordMode::overdub : ClipRecordMode::replace; break;
            case 116: if (value >= 64) toggleClipRecording(); break;
            case 117: if (value >= 64) clearActiveClip(); break;
            case 118: controls.segmentLayerLimit = 1.0f
                + static_cast<float>(midiChoice(value, 4)); break;
            case 119: controls.segmentRateHz = midiRange(value, 0.25f, 80.0f); break;
            case 121: controls.segmentRepeats = 1.0f
                + static_cast<float>(midiChoice(value, 16)); break;
            case 122: controls.segmentStep = midiUnit(value); break;
            case 124: controls.segmentPitchScatter = midiRange(value, 0.0f, 12.0f); break;
            case 125: controls.segmentLevelVariation = midiUnit(value); break;
            case 126: controls.segmentIntervalCurve = midiRange(value, -1.0f, 1.0f); break;
            default: break;
        }
        if (cc >= 20 && cc <= 126 && cc != 52) markPersistentStateDirty();
        int lockParameter = cc >= 20 && cc <= 45
            ? cc - 20 : (cc >= 69 && cc <= 110 ? cc - 43 : -1);
        if (cc >= 59 && cc <= 64) lockParameter = 68 + cc - 59;
        if (cc == 118) lockParameter = 74;
        else if (cc == 119) lockParameter = 75;
        else if (cc == 121) lockParameter = 76;
        else if (cc == 122) lockParameter = 77;
        else if (cc == 124) lockParameter = 78;
        else if (cc == 125) lockParameter = 79;
        else if (cc == 126) lockParameter = 80;
        if (clipRecording && lockParameter >= 0
            && lockParameter < static_cast<int>(kLockParameterCount)) {
            captureClipParameter(lockParameter);
        }
    }

    void serviceVoiceLaneSync()
    {
        const std::uint32_t desiredMask = engine.loadedLaneMask()
            & ~pendingClearMask & ~recordingLaneMask;
        for (auto& voice : voices) {
            for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                const bool desired = (desiredMask & (1u << lane)) != 0u;
                if (desired == voice.engine.laneLoaded(lane)) continue;
                if (desired) {
                    voice.engine.beginLaneAdoption(
                        lane, engine.laneFrames(lane), engine.laneSampleRate(lane));
                } else {
                    voice.engine.beginLaneRemoval(lane);
                }
                break;
            }
            for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                if (!voice.engine.laneLoaded(lane)
                    || voice.regionRevisions[lane] == laneRegionRevisions[lane]) {
                    continue;
                }
                const RegionMap& map = activeRegions[lane];
                voice.engine.setLaneRegions(lane, map.boundaries.data(),
                    std::max<std::size_t>(1u, map.count));
                voice.regionRevisions[lane] = laneRegionRevisions[lane];
            }
        }
    }

    void completedRecordingLane(std::size_t lane, bool analyzeNow = true)
    {
        recordingLaneMask = 0u;
        if (lane < kLaneCount && engine.laneFrames(lane) != 0u) {
            if (analyzeNow) {
                RegionMap regions;
                analyzeRegions(engine.laneData(lane, 0u),
                               engine.laneData(lane, 1u),
                               engine.laneFrames(lane),
                               engine.laneSampleRate(lane), regions);
                installRegionMap(lane, regions);
                pendingRegionAnalysisMask &= ~(1u << lane);
            } else {
                pendingRegionAnalysisMask |= 1u << lane;
            }
            pendingClearMask &= ~(1u << lane);
            markWaveformDirty(lane);
            std::snprintf(laneSourceLabels[lane].data(),
                          laneSourceLabels[lane].size(), "SAVING TAKE");
            currentLaneSourceKinds[lane] = LaneSourceKind::capture;
            if (activeClip >= 0
                && activeClip < static_cast<int>(clipSlots.size())) {
                ClipSlot& clip = clipSlots[static_cast<std::size_t>(activeClip)];
                std::array<char, kPersistencePathSize> leaf {};
                std::snprintf(leaf.data(), leaf.size(),
                              "captures/c%02d-lane-%u.wav", activeClip + 1,
                              static_cast<unsigned>(lane + 1u));
                persistencePath(leaf.data(), clip.laneSourcePaths[lane].data(),
                                clip.laneSourcePaths[lane].size());
                std::snprintf(currentLaneSourcePaths[lane].data(),
                              currentLaneSourcePaths[lane].size(), "%s",
                              clip.laneSourcePaths[lane].data());
                clip.laneSourceKinds[lane] = LaneSourceKind::capture;
                std::snprintf(clip.laneSourceLabels[lane].data(),
                              clip.laneSourceLabels[lane].size(), "SAVING TAKE");
                clip.occupied = true;
                pendingCaptureClips[lane] = activeClip;
                pendingCaptureSaveMask |= 1u << lane;
                markPersistentStateDirty();
            }
        }
    }

    void startRecording()
    {
        if (engine.recording()) return;
        const auto prepareState = static_cast<ClipPrepareState>(
            clipPrepareState.load(std::memory_order_acquire));
        if (prepareState == ClipPrepareState::requested
            || prepareState == ClipPrepareState::loading) {
            setError("Wait for the queued clip to finish preparing");
            return;
        }
        clearError();
        pendingRegionAnalysisMask &= ~(1u << selectedLane);
        recordingLaneMask = 1u << selectedLane;
        if (!engine.beginLaneRecording(selectedLane, 0u)) {
            recordingLaneMask = 0u;
            setError("Lane recording could not start");
        }
    }

    void finishRecording()
    {
        if (!engine.recording()) return;
        const bool adopted = engine.endRecording();
        const std::size_t lane = engine.lastRecordedLane();
        completedRecordingLane(lane);
        if ((lane >= kLaneCount || engine.laneFrames(lane) == 0u) && !adopted) {
            setError("Recording was too short or empty");
        }
    }

    void clearSelectedLane()
    {
        if (!engine.laneLoaded(selectedLane)) return;
        if (clipPreparationInFlight()) {
            setError("Wait for the queued clip to finish preparing");
            return;
        }
        pushClipUndo(activeClip);
        if (!engine.beginLaneRemoval(selectedLane)) {
            setError("Lane is busy and could not be cleared");
            return;
        }
        pendingClearMask |= 1u << selectedLane;
        pendingRegionAnalysisMask &= ~(1u << selectedLane);
        std::snprintf(laneSourceLabels[selectedLane].data(),
                      laneSourceLabels[selectedLane].size(), "EMPTY");
        currentLaneSourcePaths[selectedLane][0] = '\0';
        currentLaneSourceKinds[selectedLane] = LaneSourceKind::empty;
        activeRegions[selectedLane] = {};
        ++laneRegionRevisions[selectedLane];
        if (activeClip >= 0
            && activeClip < static_cast<int>(clipSlots.size())) {
            ClipSlot& clip = clipSlots[static_cast<std::size_t>(activeClip)];
            clip.laneSourcePaths[selectedLane][0] = '\0';
            clip.laneSourceKinds[selectedLane] = LaneSourceKind::empty;
            std::snprintf(clip.laneSourceLabels[selectedLane].data(),
                          clip.laneSourceLabels[selectedLane].size(), "EMPTY");
        }
        markPersistentStateDirty();
        clearWaveform(selectedLane);
    }

    void refreshPlaybackVoices()
    {
        allNotesOff();
        for (auto& voice : voices) {
            voice.engine.rebindStorage(
                storage.get() + planeCapacityFrames * 0u,
                storage.get() + planeCapacityFrames * 1u,
                storage.get() + planeCapacityFrames * 2u,
                storage.get() + planeCapacityFrames * 3u,
                planeCapacityFrames);
            for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                if (engine.laneLoaded(lane)) {
                    voice.engine.adoptLane(lane, engine.laneFrames(lane),
                                           engine.laneSampleRate(lane));
                    voice.engine.setLaneRegions(lane,
                        activeRegions[lane].boundaries.data(),
                        std::max<std::size_t>(1u, activeRegions[lane].count));
                    voice.regionRevisions[lane] = laneRegionRevisions[lane];
                }
            }
            voice.engine.stopPlayback();
        }
    }

    bool beginSampleEdit()
    {
        if (engine.recording()) {
            setError("Stop recording before editing");
            return false;
        }
        if (!engine.laneLoaded(selectedLane)) {
            setError("Selected lane is empty");
            return false;
        }
        if (clipPreparationInFlight()) {
            setError("Wait for the queued clip to finish preparing");
            return false;
        }
        if (pendingCaptureSaveMask != 0u && !serviceMaintenance()) {
            setError("Save the current take before editing");
            return false;
        }
        clearError();
        pushClipUndo(activeClip);
        allNotesOff();
        return true;
    }

    void finishSampleEdit(const char* label)
    {
        if (activeClip < 0
            || activeClip >= static_cast<int>(clipSlots.size())) return;
        ClipSlot& clip = clipSlots[static_cast<std::size_t>(activeClip)];
        const char* currentPath =
            clip.laneSourcePaths[selectedLane].data();
        const char alternate = std::strstr(currentPath, "-edit-a.wav")
            ? 'b' : 'a';
        std::array<char, kPersistencePathSize> leaf {};
        std::snprintf(leaf.data(), leaf.size(),
                      "captures/c%02d-lane-%u-edit-%c.wav",
                      activeClip + 1,
                      static_cast<unsigned>(selectedLane + 1u), alternate);
        persistencePath(leaf.data(), clip.laneSourcePaths[selectedLane].data(),
                        clip.laneSourcePaths[selectedLane].size());
        clip.laneSourceKinds[selectedLane] = LaneSourceKind::capture;
        std::snprintf(clip.laneSourceLabels[selectedLane].data(),
                      clip.laneSourceLabels[selectedLane].size(), "%s",
                      label ? label : "EDIT");
        clip.occupied = true;
        currentLaneSourceKinds[selectedLane] = LaneSourceKind::capture;
        std::snprintf(currentLaneSourcePaths[selectedLane].data(),
                      currentLaneSourcePaths[selectedLane].size(), "%s",
                      clip.laneSourcePaths[selectedLane].data());
        std::snprintf(laneSourceLabels[selectedLane].data(),
                      laneSourceLabels[selectedLane].size(), "%s",
                      clip.laneSourceLabels[selectedLane].data());
        pendingCaptureClips[selectedLane] = activeClip;
        pendingCaptureSaveMask |= 1u << selectedLane;
        markWaveformDirty(selectedLane);
        RegionMap regions;
        analyzeRegions(engine.laneData(selectedLane, 0u),
                       engine.laneData(selectedLane, 1u),
                       engine.laneFrames(selectedLane),
                       engine.laneSampleRate(selectedLane), regions);
        installRegionMap(selectedLane, regions);
        refreshPlaybackVoices();
        markPersistentStateDirty();
    }

    void reverseSelectedLane()
    {
        if (!beginSampleEdit()) return;
        const std::size_t frames = engine.laneFrames(selectedLane);
        for (std::size_t channel = 0u; channel < 2u; ++channel) {
            float* data = engine.writableLane(selectedLane, channel);
            std::reverse(data, data + frames);
        }
        finishSampleEdit("REVERSED");
    }

    void normalizeSelectedLane()
    {
        if (!beginSampleEdit()) return;
        const std::size_t frames = engine.laneFrames(selectedLane);
        float peak = 0.0f;
        for (std::size_t channel = 0u; channel < 2u; ++channel) {
            const float* data = engine.laneData(selectedLane, channel);
            for (std::size_t frame = 0u; frame < frames; ++frame) {
                peak = std::max(peak, std::fabs(data[frame]));
            }
        }
        if (peak > 0.000001f) {
            const float scale = std::min(16.0f, 0.98f / peak);
            for (std::size_t channel = 0u; channel < 2u; ++channel) {
                float* data = engine.writableLane(selectedLane, channel);
                for (std::size_t frame = 0u; frame < frames; ++frame) {
                    data[frame] = clampValue(data[frame] * scale, -1.0f, 1.0f);
                }
            }
        }
        finishSampleEdit("NORMALIZED");
    }

    void fadeSelectedLane()
    {
        if (!beginSampleEdit()) return;
        const std::size_t frames = engine.laneFrames(selectedLane);
        const std::size_t fadeFrames = std::min<std::size_t>(
            frames / 2u, static_cast<std::size_t>(sampleRate * 0.005f));
        for (std::size_t channel = 0u; channel < 2u; ++channel) {
            float* data = engine.writableLane(selectedLane, channel);
            for (std::size_t frame = 0u; frame < fadeFrames; ++frame) {
                const float gain = static_cast<float>(frame)
                    / static_cast<float>(std::max<std::size_t>(1u, fadeFrames));
                data[frame] *= gain;
                data[frames - 1u - frame] *= gain;
            }
        }
        finishSampleEdit("FADED");
    }

    void trimSelectedLane()
    {
        if (!beginSampleEdit()) return;
        const std::size_t frames = engine.laneFrames(selectedLane);
        const float start = clampValue(sampleTrimStart, 0.0f, 0.99f);
        const float end = clampValue(sampleTrimEnd,
                                     start + 0.01f, 1.0f);
        const std::size_t first = std::min(frames - 1u,
            static_cast<std::size_t>(std::floor(start * frames)));
        const std::size_t last = std::min(frames,
            std::max(first + 1u,
                static_cast<std::size_t>(std::ceil(end * frames))));
        const std::size_t trimmedFrames = last - first;
        for (std::size_t channel = 0u; channel < 2u; ++channel) {
            float* data = engine.writableLane(selectedLane, channel);
            std::memmove(data, data + first, trimmedFrames * sizeof(float));
        }
        engine.adoptLane(selectedLane, trimmedFrames,
                         engine.laneSampleRate(selectedLane));
        sampleTrimStart = 0.0f;
        sampleTrimEnd = 1.0f;
        finishSampleEdit("TRIMMED");
    }

    void cancelWaveScan(std::size_t lane)
    {
        if (lane < kLaneCount && scanLane == static_cast<int>(lane)) {
            scanLane = -1;
            scanFrame = 0u;
            scanFrames = 0u;
        }
    }

    void markWaveformDirty(std::size_t lane)
    {
        if (lane >= kLaneCount) return;
        cancelWaveScan(lane);
        waveDirtyMask |= 1u << lane;
    }

    void clearWaveform(std::size_t lane)
    {
        if (lane >= kLaneCount) return;
        cancelWaveScan(lane);
        waveDirtyMask &= ~(1u << lane);
        waveMinimum[lane].fill(0);
        waveMaximum[lane].fill(0);
        waveTextDirty = true;
    }

    void rebuildWaveText()
    {
        static constexpr char hex[] = "0123456789abcdef";
        std::size_t position = 0u;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if (lane != 0u) waveText[position++] = '|';
            for (std::size_t bin = 0u; bin < kWaveBins; ++bin) {
                const int low = clampValue(static_cast<int>(waveMinimum[lane][bin]) + 7, 0, 14);
                const int high = clampValue(static_cast<int>(waveMaximum[lane][bin]) + 7, 0, 14);
                waveText[position++] = hex[low];
                waveText[position++] = hex[high];
            }
        }
        waveText[position] = '\0';
        waveTextDirty = false;
    }

    void beginNextWaveScan()
    {
        if (scanLane >= 0 || waveDirtyMask == 0u) return;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if ((waveDirtyMask & (1u << lane)) == 0u) continue;
            scanLane = static_cast<int>(lane);
            scanFrame = 0u;
            scanFrames = engine.laneFrames(lane);
            scanMinimum.fill(0.0f);
            scanMaximum.fill(0.0f);
            if (scanFrames == 0u || !engine.laneData(lane, 0u)) {
                waveMinimum[lane].fill(0);
                waveMaximum[lane].fill(0);
                waveDirtyMask &= ~(1u << lane);
                scanLane = -1;
                waveTextDirty = true;
            }
            return;
        }
    }

    void serviceWaveScan()
    {
        const auto loadState = static_cast<SampleLoadState>(
            sampleLoadState.load(std::memory_order_acquire));
        if (engine.recording()
            || pendingClearMask != 0u
            || (loadState >= SampleLoadState::requested
                && loadState <= SampleLoadState::copying)) return;
        beginNextWaveScan();
        if (scanLane < 0) return;
        const std::size_t lane = static_cast<std::size_t>(scanLane);
        const float* left = engine.laneData(lane, 0u);
        const float* right = engine.laneData(lane, 1u);
        const std::size_t currentFrames = engine.laneFrames(lane);
        if (!left || !right || currentFrames != scanFrames || scanFrames == 0u) {
            scanLane = -1;
            waveDirtyMask |= 1u << lane;
            return;
        }

        const std::size_t end = std::min(scanFrames, scanFrame + kWaveScanBudget);
        for (std::size_t frame = scanFrame; frame < end; ++frame) {
            const std::size_t bin = std::min(kWaveBins - 1u,
                frame * kWaveBins / scanFrames);
            const float low = std::min(left[frame], right[frame]);
            const float high = std::max(left[frame], right[frame]);
            scanMinimum[bin] = std::min(scanMinimum[bin], low);
            scanMaximum[bin] = std::max(scanMaximum[bin], high);
        }
        scanFrame = end;
        if (scanFrame < scanFrames) return;

        quantizeWaveform(scanMinimum, scanMaximum,
                         waveMinimum[lane], waveMaximum[lane]);
        waveDirtyMask &= ~(1u << lane);
        scanLane = -1;
        waveTextDirty = true;
    }

    void render(int16_t* interleaved, int frames)
    {
        if (!interleaved || frames <= 0) return;
        const int16_t* audioInput = nullptr;
        if (g_host && g_host->mapped_memory) {
            audioInput = reinterpret_cast<const int16_t*>(
                g_host->mapped_memory + g_host->audio_in_offset);
        }

        int rendered = 0;
        while (rendered < frames) {
            const std::size_t count = std::min<std::size_t>(
                kBlockFrames, static_cast<std::size_t>(frames - rendered));
            serviceSampleLoad();
            serviceClipSourceQueue();
            for (std::size_t frame = 0u; frame < count; ++frame) {
                const std::size_t sourceFrame = static_cast<std::size_t>(rendered) + frame;
                const float left = audioInput
                    ? static_cast<float>(audioInput[sourceFrame * 2u]) / 32768.0f : 0.0f;
                const float right = audioInput
                    ? static_cast<float>(audioInput[sourceFrame * 2u + 1u]) / 32768.0f : 0.0f;
                input[0u][frame] = left * recordGain;
                input[1u][frame] = right * recordGain;
                monitorInput[0u][frame] = input[0u][frame];
                monitorInput[1u][frame] = input[1u][frame];
                input[2u][frame] = 0.0f;
                input[3u][frame] = 0.0f;
                voiceMix[0u][frame] = 0.0f;
                voiceMix[1u][frame] = 0.0f;
            }

            serviceClipBeforeBlock();
            serviceVoiceLaneSync();
            for (auto& voice : voices) {
                // Inactive voices have no envelope or playback state to
                // advance. Processing all eight dormant engines multiplied
                // baseline DSP load and could miss Move's block deadline. A
                // lane transition is the one maintenance task that must run
                // to completion even while its voice is dormant.
                if (!voice.active && !voice.engine.laneTransitionActive()) {
                    continue;
                }
                const Engine::Controls effective = effectiveControls(voice);
                voice.engine.process(silentInputPointers.data(),
                                     voice.outputPointers.data(), count, effective);
                if (!voice.active) continue;
                const float voiceGain = voice.clipOwned
                    ? clipOutputGain : (voice.useSnapshot
                        ? voice.snapshotOutputGain : outputGain);
                for (std::size_t frame = 0u; frame < count; ++frame) {
                    const float coefficient = voice.target > voice.envelope
                        ? attackCoefficient : releaseCoefficient;
                    voice.envelope += (voice.target - voice.envelope) * coefficient;
                    voiceMix[0u][frame] += voice.output[0u][frame]
                        * voice.envelope * voiceGain;
                    voiceMix[1u][frame] += voice.output[1u][frame]
                        * voice.envelope * voiceGain;
                }
                if (!voice.held && voice.envelope < 1.0e-4f) {
                    voice.envelope = 0.0f;
                    voice.target = 0.0f;
                    voice.active = false;
                    voice.note = -1;
                    voice.sourcePad = -1;
                    voice.clipOwned = false;
                    voice.clipNoteIndex = -1;
                    voice.useSnapshot = false;
                    voice.engine.stopPlayback();
                }
            }

            if (previewActive && sampleStageFrames != 0u) {
                const std::size_t available = previewFrame < sampleStageFrames
                    ? sampleStageFrames - previewFrame : 0u;
                const std::size_t previewFrames = std::min(count, available);
                const float* previewLeft = sampleStage.get();
                const float* previewRight = sampleStage.get() + laneCapacityFrames;
                for (std::size_t frame = 0u; frame < previewFrames; ++frame) {
                    voiceMix[0u][frame] += previewLeft[previewFrame + frame]
                        * previewLevel;
                    voiceMix[1u][frame] += previewRight[previewFrame + frame]
                        * previewLevel;
                }
                previewFrame += previewFrames;
                if (previewFrame >= sampleStageFrames) previewActive = false;
            }

            processEffects(count, clipPlaying ? clipControls : controls);

            if (engine.recording() && recordSource == 1) {
                for (std::size_t frame = 0u; frame < count; ++frame) {
                    input[0u][frame] = voiceMix[0u][frame];
                    input[1u][frame] = voiceMix[1u][frame];
                }
            }
            const bool wasRecording = engine.recording();
            Engine::Controls bankControls = controls;
            bankControls.mix = 1.0f;
            bankControls.recordInputGain = 1.0f;
            bankControls.lanePhaseOffset = 0.0f;
            engine.stopPlayback();
            engine.process(inputPointers.data(), outputPointers.data(), count, bankControls);
            if (wasRecording && !engine.recording()) {
                const std::size_t lane = engine.lastRecordedLane();
                // The engine can reach its recording capacity in this real-time
                // callback. Defer the bounded transient scan to maintenance.
                completedRecordingLane(lane, false);
            }
            serviceVoiceLaneSync();

            for (std::size_t frame = 0u; frame < count; ++frame) {
                for (std::size_t channel = 0u; channel < 2u; ++channel) {
                    const float wet = voiceMix[channel][frame];
                    const float monitor = monitorInput[channel][frame] * monitorLevel;
                    interleaved[(static_cast<std::size_t>(rendered) + frame) * 2u + channel]
                        = toInt16(wet + monitor);
                }
            }
            serviceClipAfterBlock(count);
            rendered += static_cast<int>(count);
        }
    }

    bool persistencePath(const char* leaf, char* path, std::size_t pathSize) const
    {
        if (!persistenceEnabled || !leaf || !path || pathSize == 0u) return false;
        const std::size_t directoryLength = std::strlen(moduleDirectory.data());
        const std::size_t leafLength = std::strlen(leaf);
        if (directoryLength == 0u
            || directoryLength + 1u + leafLength + 1u > pathSize) return false;
        std::memcpy(path, moduleDirectory.data(), directoryLength);
        path[directoryLength] = '/';
        std::memcpy(path + directoryLength + 1u, leaf, leafLength);
        path[directoryLength + 1u + leafLength] = '\0';
        return true;
    }

    int serializeState(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        return std::snprintf(buffer, static_cast<std::size_t>(bufferLength),
            "{\"version\":6,\"locus\":%.5f,\"field\":%.5f,\"speed\":%.5f,"
            "\"manual_lane\":%.5f,\"lane_cycles\":%.5f,\"algorithm\":%d,"
            "\"join\":%.5f,\"output_gain\":%.5f,\"record_gain\":%.5f,"
            "\"pressure_mode\":%d,\"root_note\":%d,\"pad_octave\":%d,"
            "\"record_lane\":%u,\"record_source\":%d,\"motion_mode\":%d,"
            "\"motion_sound\":%d,\"motion_rate\":%.5f,"
            "\"motion_rate_basis\":%d,\"motion_path_rate\":%.5f,"
            "\"motion_step\":%.5f,\"motion_seed\":%.0f,\"lanes_mode\":%d,"
            "\"lane_path\":%d,\"lanes_stretch\":%d,\"stretch_ratio\":%.5f,"
            "\"lane_1_gain\":%.5f,\"lane_2_gain\":%.5f,"
            "\"lane_3_gain\":%.5f,\"lane_4_gain\":%.5f,"
            "\"grain_mode\":%d,\"grain_size\":%.5f,"
            "\"grain_density\":%.5f,\"grain_pitch\":%.5f,"
            "\"grain_window\":%d,\"grain_spread\":%.5f,"
            "\"cutups_mode\":%d,\"tempo_bpm\":%.5f,\"tempo_swing\":%.5f,"
            "\"chain_enabled\":%d,"
            "\"motion_travel\":%.5f,\"motion_jitter\":%.5f,"
            "\"packet_duty\":%.5f,\"motor_rate\":%.5f,"
            "\"motor_symmetry\":%.5f,\"motor_shape\":%d,"
            "\"segment_trigger\":%d,\"segment_overlap\":%d,"
            "\"segment_layers\":%.0f,\"segment_rate\":%.5f,"
            "\"segment_repeats\":%.0f,\"segment_step\":%.5f,"
            "\"segment_pitch\":%.5f,\"segment_level\":%.5f,"
            "\"segment_curve\":%.5f,"
            "\"lane_blend\":%d,\"lane_path_shape\":%d,"
            "\"lane_path_offset\":%.5f,\"lane_path_skew\":%.5f,"
            "\"lane_path_curve\":%.5f,\"lane_slew\":%.5f,"
            "\"grain_spray\":%.5f,\"grain_bias\":%d,"
            "\"grain_pitch_spray\":%.5f,\"grain_reverse\":%.5f,"
            "\"grain_size_variation\":%.5f,\"grain_level_variation\":%.5f,"
            "\"grain_timing\":%d,\"grain_timing_scatter\":%.5f,"
            "\"grain_envelope_skew\":%.5f,\"grain_process\":%d,"
            "\"grain_process_amount\":%.5f,\"grain_regions\":%.0f,"
            "\"grain_advance\":%d,\"cut_clock\":%d,\"cut_division\":%d,"
            "\"cut_rate\":%.5f,\"cut_time_variation\":%.5f,"
            "\"cut_gate\":%.5f,\"cut_join\":%.5f,\"cut_regions\":%.0f,"
            "\"cut_repeats\":%.0f,\"cut_reverse\":%.5f,"
            "\"cut_pitch_variation\":%.5f,\"cut_level_variation\":%.5f,"
            "\"cut_lane_order\":%d,\"cut_poly_path\":%d,"
            "\"cut_region_mode\":%d,\"sample_mode\":%d,"
            "\"slice_region_mode\":%d,\"slice_regions\":%.0f,"
            "\"glitch_mode\":%d,\"glitch_clock\":%d,"
            "\"glitch_division\":%d,\"glitch_rate\":%.5f,"
            "\"glitch_slice\":%.5f,\"glitch_repeats\":%.0f,"
            "\"glitch_chance\":%.5f,\"glitch_mix\":%.5f,"
            "\"glitch_steps\":%.0f,\"glitch_pulses\":%.0f,"
            "\"glitch_rotate\":%.0f,\"glitch_jump\":%.5f,"
            "\"glitch_pitch\":%.5f,\"glitch_reverse\":%.5f,"
            "\"glitch_decay\":%.5f,\"glitch_seed\":%.0f,"
            "\"effect_tone\":%.5f,\"effect_drive\":%.5f,"
            "\"effect_bits\":%.5f,\"effect_rate\":%.0f,"
            "\"effect_delay\":%.5f,\"effect_feedback\":%.5f,"
            "\"effect_width\":%.5f,\"effect_mix\":%.5f,"
            "\"pad_mode\":%d,\"lane_variation\":%d,\"clip_record_mode\":%d,"
            "\"preview_level\":%.5f,\"grain_quality\":%zu,"
            "\"launch_quantize\":%d,\"generation\":%llu,\"complete\":1}",
            controls.locus, controls.field, controls.speed,
            controls.manualLane + 1.0f, controls.laneCycles,
            static_cast<int>(controls.algorithm), controls.join,
            outputGain, recordGain, pressureMode, rootNote, padOctave,
            static_cast<unsigned>(selectedLane + 1u), recordSource,
            static_cast<int>(controls.motion), static_cast<int>(controls.motionSound),
            controls.motionSoundRateHz,
            static_cast<int>(controls.motionRateBasis), controls.motionPathRate,
            controls.motionStep, controls.motionSeed,
            static_cast<int>(controls.lanes),
            static_cast<int>(controls.lanePath), controls.lanesStretch ? 1 : 0,
            controls.lanesStretchRatio,
            controls.laneGains[0u], controls.laneGains[1u],
            controls.laneGains[2u], controls.laneGains[3u],
            static_cast<int>(controls.grains), controls.grainSizeMs,
            controls.grainDensityHz, controls.grainPitch,
            static_cast<int>(controls.grainWindow), controls.grainStereoSpread,
            static_cast<int>(controls.cutups), tempoBpm,
            tempoSwing * 100.0f,
            chainEnabled ? 1 : 0,
            controls.motionTravel, controls.motionJitter, controls.packetDuty,
            controls.motorRateHz, controls.motorSymmetry,
            static_cast<int>(controls.motorShape),
            static_cast<int>(controls.segmentTrigger),
            static_cast<int>(controls.segmentOverlap),
            controls.segmentLayerLimit, controls.segmentRateHz,
            controls.segmentRepeats, controls.segmentStep,
            controls.segmentPitchScatter, controls.segmentLevelVariation,
            controls.segmentIntervalCurve,
            static_cast<int>(controls.laneBlend),
            static_cast<int>(controls.lanePathShape), controls.lanePathOffset,
            controls.lanePathSkew, controls.lanePathCurve, controls.laneSlewMs,
            controls.grainSpray, static_cast<int>(controls.grainBias),
            controls.grainPitchSpray, controls.grainReverseChance,
            controls.grainSizeVariation, controls.grainLevelVariation,
            static_cast<int>(controls.grainTiming), controls.grainTimingScatter,
            controls.grainEnvelopeSkew, static_cast<int>(controls.grainProcess),
            controls.grainProcessAmount, controls.grainRegionCount,
            static_cast<int>(controls.grainAdvance),
            static_cast<int>(controls.cutClock),
            static_cast<int>(controls.cutDivision), controls.cutRateHz,
            controls.cutTimeVariation, controls.cutGate, controls.cutJoinMs,
            controls.cutRegionCount, controls.cutRepeats,
            controls.cutReverseChance, controls.cutPitchVariation,
            controls.cutLevelVariation, static_cast<int>(controls.cutLaneOrder),
            static_cast<int>(controls.cutPolyPath),
            static_cast<int>(controls.cutRegionMode),
            static_cast<int>(controls.sampleMode),
            static_cast<int>(controls.sampleRegionMode),
            controls.sampleRegionCount,
            static_cast<int>(controls.glitchMode),
            static_cast<int>(controls.glitchClock),
            static_cast<int>(controls.glitchDivision), controls.glitchRateHz,
            controls.glitchSliceMs, controls.glitchRepeats,
            controls.glitchChance, controls.glitchMix,
            controls.glitchSteps, controls.glitchPulses,
            controls.glitchRotate, controls.glitchJumpMs,
            controls.glitchPitch, controls.glitchReverseChance,
            controls.glitchDecay, controls.glitchSeed,
            controls.effectTone, controls.effectDrive, controls.effectBits,
            controls.effectRate, controls.effectDelayMs,
            controls.effectFeedback, controls.effectWidth, controls.effectMix,
            static_cast<int>(padMode),
            static_cast<int>(laneVariation), static_cast<int>(clipRecordMode),
            previewLevel, grainVoiceLimit,
            static_cast<int>(launchQuantize),
            static_cast<unsigned long long>(persistenceGeneration));
    }

    bool writeStateFile()
    {
        std::array<char, kPersistencePathSize> path {};
        std::array<char, kPersistencePathSize> temporary {};
        std::array<char, kStateTextSize> text {};
        if (!persistencePath("thru-state.json", path.data(), path.size())
            || !persistencePath("thru-state.json.tmp", temporary.data(), temporary.size())) {
            return false;
        }
        const int length = serializeState(text.data(), static_cast<int>(text.size()));
        if (length <= 0 || static_cast<std::size_t>(length) >= text.size()) return false;
        std::FILE* file = std::fopen(temporary.data(), "wb");
        if (!file) return false;
        const bool wrote = writeBytes(file, text.data(), static_cast<std::size_t>(length));
        const bool closed = std::fclose(file) == 0;
        if (!wrote || !closed
            || !replaceFileWithBackup(temporary.data(), path.data())) {
            std::remove(temporary.data());
            return false;
        }
        return true;
    }

    bool writeClipFile()
    {
        std::array<char, kPersistencePathSize> path {};
        std::array<char, kPersistencePathSize> temporary {};
        if (!persistencePath("thru-clips.bin", path.data(), path.size())
            || !persistencePath("thru-clips.bin.tmp", temporary.data(),
                                temporary.size())) {
            return false;
        }
        captureActiveClip(false);
        std::FILE* file = std::fopen(temporary.data(), "wb");
        if (!file) return false;
        const char magic[8] { 'S', '3', 'G', 'T', 'H', 'R', '3', '\0' };
        bool ok = writeBytes(file, magic, sizeof(magic))
            && writeU32(file, 4u)
            && writeU64(file, persistenceGeneration)
            && writeF32(file, tempoBpm)
            && writeF32(file, tempoSwing)
            && writeU32(file, static_cast<std::uint32_t>(launchQuantize))
            && writeU32(file, static_cast<std::uint32_t>(grainVoiceLimit))
            && writeU32(file, static_cast<std::uint32_t>(activeClip))
            && writeU32(file, static_cast<std::uint32_t>(chainPosition))
            && writeU32(file, static_cast<std::uint32_t>(chainEditSlot));
        for (const auto& entry : clipChain) {
            ok = ok && writeU32(file, entry.enabled ? 1u : 0u)
                && writeU32(file, entry.clip)
                && writeU32(file, entry.repeats);
        }
        for (const auto& clip : clipSlots) {
            ok = ok && writeU32(file, clip.occupied ? 1u : 0u);
            for (const float base : clip.baseValues) {
                ok = ok && writeF32(file, base);
            }
            for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                ok = ok && writeU32(file, static_cast<std::uint32_t>(
                                      clip.laneSourceKinds[lane]))
                    && writeBytes(file, clip.laneSourcePaths[lane].data(),
                                  clip.laneSourcePaths[lane].size())
                    && writeBytes(file, clip.laneSourceLabels[lane].data(),
                                  clip.laneSourceLabels[lane].size());
            }
            for (const float cue : clip.cues) {
                ok = ok && writeF32(file, cue);
            }
            ok = ok && writeU32(file, clip.clipRecorded ? 1u : 0u)
                && writeU32(file, clip.clipDurationMs)
                && writeF32(file, clip.clipRate)
                && writeF32(file, clip.recordedTempoBpm)
                && writeF32(file, clip.recordedBeats)
                && writeU32(file, static_cast<std::uint32_t>(clip.timingMode))
                && writeU32(file, clip.noteCapacityReached ? 1u : 0u)
                && writeU32(file, clip.pointCapacityReached ? 1u : 0u)
                && writeU32(file, clip.takeCount)
                && writeU32(file, clip.clipNoteCount)
                && writeU32(file, clip.clipPointCount);
            for (std::size_t index = 0u;
                 index < clip.clipNoteCount; ++index) {
                const ClipNote& note = clip.clipNotes[index];
                ok = ok && writeU32(file, note.startMs)
                    && writeU32(file, note.durationMs)
                    && writeU32(file, note.note)
                    && writeU32(file, note.velocity)
                    && writeU32(file, note.sourcePad)
                    && writeU32(file, note.take);
            }
            for (std::size_t index = 0u;
                 index < clip.clipPointCount; ++index) {
                const ClipPoint& point = clip.clipPoints[index];
                ok = ok && writeU32(file, point.timeMs)
                    && writeF32(file, point.value)
                    && writeU32(file, point.parameter & 0xffu)
                    && writeU32(file, point.sourcePad)
                    && writeU32(file, static_cast<std::uint32_t>(point.type))
                    && writeU32(file, clipPointTake(point));
            }
        }
        const bool closed = std::fclose(file) == 0;
        ok = ok && closed && appendFileChecksum(temporary.data());
        if (!ok || !replaceFileWithBackup(temporary.data(), path.data())) {
            std::remove(temporary.data());
            return false;
        }
        return true;
    }

    bool writeMetadataFiles()
    {
        ++persistenceGeneration;
        if (!writeClipFile()) return false;
        return writeStateFile();
    }

    bool clipFileGeneration(const char* path, std::uint64_t& generation) const
    {
        long payloadSize = 0l;
        if (!checksummedPayloadSize(path, payloadSize)
            || payloadSize < 20l) return false;
        std::FILE* file = std::fopen(path, "rb");
        if (!file) return false;
        char magic[8] {};
        std::uint32_t version = 0u;
        const bool ok = std::fread(magic, 1u, sizeof(magic), file) == sizeof(magic)
            && std::memcmp(magic, "S3GTHR3", 7u) == 0
            && readU32(file, version)
            && (version == 1u || version == 2u || version == 3u
                || version == 4u)
            && readU64(file, generation) && generation != 0u;
        std::fclose(file);
        return ok;
    }

    bool readClipFileFromPath(
        const char* path,
        std::uint64_t expectedGeneration = kAnyPersistenceGeneration)
    {
        long payloadSize = 0l;
        if (!checksummedPayloadSize(path, payloadSize)) return false;
        std::FILE* file = std::fopen(path, "rb");
        if (!file) return false;
        char magic[8] {};
        std::uint32_t version = 0u;
        std::uint32_t selected = 0u;
        std::uint32_t loadedChainPosition = 0u;
        std::uint32_t loadedChainEditSlot = 0u;
        std::uint32_t loadedLaunchQuantize = 0u;
        std::uint32_t loadedGrainVoiceLimit = 8u;
        std::uint64_t loadedGeneration = 0u;
        float bpm = 120.0f;
        float swing = 0.5f;
        std::array<ClipChainEntry, kClipChainLength> loadedChain {};
        auto loadedClips = std::unique_ptr<std::array<ClipSlot, kClipSlotCount>>(
            new (std::nothrow) std::array<ClipSlot, kClipSlotCount> {});
        bool ok = loadedClips
            && std::fread(magic, 1u, sizeof(magic), file) == sizeof(magic)
            && std::memcmp(magic, "S3GTHR3", 7u) == 0
            && readU32(file, version)
            && (version == 1u || version == 2u || version == 3u
                || version == 4u)
            && readU64(file, loadedGeneration)
            && loadedGeneration != 0u
            && (expectedGeneration == kAnyPersistenceGeneration
                || loadedGeneration == expectedGeneration)
            && readF32(file, bpm) && readF32(file, swing)
            && readU32(file, loadedLaunchQuantize)
            && loadedLaunchQuantize <= static_cast<std::uint32_t>(LaunchQuantize::bar)
            && readU32(file, loadedGrainVoiceLimit)
            && (loadedGrainVoiceLimit == 4u || loadedGrainVoiceLimit == 8u
                || loadedGrainVoiceLimit == 16u)
            && readU32(file, selected) && selected < kClipSlotCount
            && readU32(file, loadedChainPosition)
            && readU32(file, loadedChainEditSlot);
        for (auto& entry : loadedChain) {
            std::uint32_t enabled = 0u;
            std::uint32_t clip = 0u;
            std::uint32_t repeats = 1u;
            ok = ok && readU32(file, enabled)
                && readU32(file, clip) && clip < kClipSlotCount
                && readU32(file, repeats);
            entry.enabled = enabled != 0u;
            entry.clip = static_cast<std::uint8_t>(clip);
            entry.repeats = static_cast<std::uint8_t>(clampValue(
                repeats, 1u, 16u));
        }
        if (loadedClips) {
            for (auto& clip : *loadedClips) {
                std::uint32_t occupied = 0u;
                ok = ok && readU32(file, occupied);
                clip.occupied = occupied != 0u;
                const std::size_t storedBaseCount = version >= 4u
                    ? kLockParameterCount : (version >= 3u
                        ? kVersion3LockParameterCount
                        : kLegacyLockParameterCount);
                for (std::size_t parameter = 0u;
                     parameter < storedBaseCount; ++parameter) {
                    ok = ok && readF32(file, clip.baseValues[parameter]);
                }
                for (std::size_t parameter = storedBaseCount;
                     parameter < kLockParameterCount; ++parameter) {
                    clip.baseValues[parameter] = lockedValue(
                        controls, outputGain, parameter);
                }
                for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                    std::uint32_t kind = 0u;
                    ok = ok && readU32(file, kind)
                        && kind <= static_cast<std::uint32_t>(LaneSourceKind::empty)
                        && std::fread(clip.laneSourcePaths[lane].data(), 1u,
                            clip.laneSourcePaths[lane].size(), file)
                            == clip.laneSourcePaths[lane].size()
                        && std::fread(clip.laneSourceLabels[lane].data(), 1u,
                            clip.laneSourceLabels[lane].size(), file)
                            == clip.laneSourceLabels[lane].size();
                    clip.laneSourceKinds[lane] =
                        static_cast<LaneSourceKind>(kind);
                    clip.laneSourcePaths[lane].back() = '\0';
                    clip.laneSourceLabels[lane].back() = '\0';
                }
                for (float& cue : clip.cues) {
                    ok = ok && readF32(file, cue);
                    cue = clampValue(cue, -1.0f, 1.0f);
                }
                std::uint32_t recorded = 0u;
                std::uint32_t durationMs = 0u;
                std::uint32_t noteCount = 0u;
                std::uint32_t pointCount = 0u;
                std::uint32_t timingMode = 0u;
                std::uint32_t noteCapacityReached = 0u;
                std::uint32_t pointCapacityReached = 0u;
                std::uint32_t takeCount = 0u;
                ok = ok && readU32(file, recorded)
                    && readU32(file, durationMs)
                    && readF32(file, clip.clipRate)
                    && readF32(file, clip.recordedTempoBpm)
                    && readF32(file, clip.recordedBeats)
                    && readU32(file, timingMode)
                    && timingMode <= static_cast<std::uint32_t>(ClipTimingMode::beatSync)
                    && readU32(file, noteCapacityReached)
                    && readU32(file, pointCapacityReached)
                    && (version < 2u || readU32(file, takeCount))
                    && readU32(file, noteCount)
                    && readU32(file, pointCount)
                    && noteCount <= kClipNoteCapacity
                    && pointCount <= kClipPointCapacity;
                if (version < 2u) takeCount = recorded != 0u ? 1u : 0u;
                ok = ok && takeCount <= kClipTakeLimit
                    && ((recorded != 0u && takeCount >= 1u)
                        || (recorded == 0u && takeCount == 0u));
                clip.clipRecorded = recorded != 0u;
                clip.clipDurationMs = clampValue(
                    durationMs, 0u, kClipMaximumDurationMs);
                if (clip.clipRecorded) {
                    clip.clipDurationMs = std::max(
                        kClipMinimumDurationMs, clip.clipDurationMs);
                }
                clip.clipRate = clampValue(clip.clipRate, 0.25f, 4.0f);
                clip.recordedTempoBpm = clampValue(
                    clip.recordedTempoBpm, 40.0f, 240.0f);
                clip.recordedBeats = std::max(0.0f, clip.recordedBeats);
                clip.timingMode = static_cast<ClipTimingMode>(timingMode);
                clip.noteCapacityReached = noteCapacityReached != 0u;
                clip.pointCapacityReached = pointCapacityReached != 0u;
                clip.takeCount = static_cast<std::uint8_t>(takeCount);
                clip.clipNoteCount = static_cast<std::uint16_t>(noteCount);
                clip.clipPointCount = static_cast<std::uint16_t>(pointCount);
                for (std::size_t index = 0u; ok && index < noteCount; ++index) {
                    ClipNote& note = clip.clipNotes[index];
                    std::uint32_t pitch = 60u;
                    std::uint32_t velocity = 100u;
                    std::uint32_t sourcePad = 255u;
                    std::uint32_t take = 0u;
                    ok = readU32(file, note.startMs)
                        && readU32(file, note.durationMs)
                        && readU32(file, pitch)
                        && readU32(file, velocity)
                        && readU32(file, sourcePad)
                        && (version < 2u || readU32(file, take))
                        && take < std::max(1u, takeCount);
                    if (clip.clipDurationMs != 0u) {
                        note.startMs %= clip.clipDurationMs;
                    }
                    note.durationMs = clampValue(note.durationMs, 1u,
                        std::max(1u, clip.clipDurationMs));
                    note.note = static_cast<std::uint8_t>(
                        clampValue(pitch, 0u, 127u));
                    note.velocity = static_cast<std::uint8_t>(
                        clampValue(velocity, 1u, 127u));
                    note.sourcePad = static_cast<std::uint8_t>(
                        sourcePad < 32u ? sourcePad : 255u);
                    note.take = static_cast<std::uint8_t>(take);
                }
                for (std::size_t index = 0u; ok && index < pointCount; ++index) {
                    ClipPoint& point = clip.clipPoints[index];
                    std::uint32_t parameter = 0u;
                    std::uint32_t sourcePad = 255u;
                    std::uint32_t type = 0u;
                    std::uint32_t take = 0u;
                    ok = readU32(file, point.timeMs)
                        && readF32(file, point.value)
                        && readU32(file, parameter)
                        && readU32(file, sourcePad)
                        && readU32(file, type)
                        && (version < 2u || readU32(file, take))
                        && parameter < kLockParameterCount
                        && type <= static_cast<std::uint32_t>(
                            ClipPointType::pressure)
                        && take < std::max(1u, takeCount);
                    if (clip.clipDurationMs != 0u) {
                        point.timeMs %= clip.clipDurationMs;
                    }
                    point.parameter = clipPointDescriptor(parameter,
                        static_cast<std::uint8_t>(take));
                    point.sourcePad = static_cast<std::uint8_t>(
                        sourcePad < 32u ? sourcePad : 255u);
                    point.type = static_cast<ClipPointType>(type);
                }
                if (ok) {
                    std::sort(clip.clipNotes.begin(),
                              clip.clipNotes.begin() + clip.clipNoteCount,
                              [](const ClipNote& left, const ClipNote& right) {
                                  return left.startMs < right.startMs;
                              });
                    rebuildClipPointIndex(clip);
                }
            }
        }
        const bool atEnd = ok && std::ftell(file) == payloadSize;
        std::fclose(file);
        if (!atEnd || !loadedClips) return false;
        clipSlots = *loadedClips;
        clipChain = loadedChain;
        tempoBpm = clampValue(bpm, 40.0f, 240.0f);
        tempoSwing = clampValue(swing, 0.5f, 0.75f);
        launchQuantize = static_cast<LaunchQuantize>(loadedLaunchQuantize);
        grainVoiceLimit = loadedGrainVoiceLimit;
        controls.grainVoiceLimit = static_cast<float>(grainVoiceLimit);
        persistenceGeneration = loadedGeneration;
        activeClip = static_cast<int>(selected);
        chainPosition = clampValue(static_cast<int>(loadedChainPosition),
            0, static_cast<int>(kClipChainLength) - 1);
        chainEditSlot = clampValue(static_cast<int>(loadedChainEditSlot),
            0, static_cast<int>(kClipChainLength) - 1);
        clipBankInitialized = true;
        activateStoredClip(activeClip);
        return true;
    }

    bool readClipFile(
        std::uint64_t expectedGeneration = kAnyPersistenceGeneration)
    {
        std::array<char, kPersistencePathSize> path {};
        std::array<char, kPersistencePathSize> backup {};
        if (!persistencePath("thru-clips.bin", path.data(), path.size())
            || std::snprintf(backup.data(), backup.size(), "%s.bak", path.data())
                >= static_cast<int>(backup.size())) return false;
        return readClipFileFromPath(path.data(), expectedGeneration)
            || readClipFileFromPath(backup.data(), expectedGeneration);
    }

    bool writeLaneWavFile(std::size_t lane, const char* path,
                          const char* temporaryPath, bool removeIfEmpty)
    {
        if (lane >= kLaneCount || !path || !temporaryPath) return false;
        if (!engine.laneLoaded(lane) || (pendingClearMask & (1u << lane)) != 0u) {
            if (removeIfEmpty) {
                std::remove(path);
                std::remove(temporaryPath);
                return true;
            }
            return false;
        }
        const float* left = engine.laneData(lane, 0u);
        const float* right = engine.laneData(lane, 1u);
        const std::size_t frames = engine.laneFrames(lane);
        if (!left || !right || frames == 0u || frames > laneCapacityFrames) return false;
        const std::uint64_t dataBytes64 = frames * 2u * sizeof(float);
        if (dataBytes64 > 0xffffffffu - 36u) return false;
        const std::uint32_t dataBytes = static_cast<std::uint32_t>(dataBytes64);
        const std::uint32_t rate = static_cast<std::uint32_t>(std::lround(
            clampValue(engine.laneSampleRate(lane), 8000.0f, 192000.0f)));

        std::FILE* file = std::fopen(temporaryPath, "wb");
        if (!file) return false;
        bool ok = writeBytes(file, "RIFF", 4u)
            && writeU32(file, 36u + dataBytes)
            && writeBytes(file, "WAVEfmt ", 8u)
            && writeU32(file, 16u)
            && writeU16(file, 3u)
            && writeU16(file, 2u)
            && writeU32(file, rate)
            && writeU32(file, rate * 8u)
            && writeU16(file, 8u)
            && writeU16(file, 32u)
            && writeBytes(file, "data", 4u)
            && writeU32(file, dataBytes);
        std::array<float, kWavIoFrames * 2u> interleaved {};
        for (std::size_t offset = 0u; ok && offset < frames; offset += kWavIoFrames) {
            const std::size_t count = std::min(kWavIoFrames, frames - offset);
            for (std::size_t frame = 0u; frame < count; ++frame) {
                interleaved[frame * 2u] = left[offset + frame];
                interleaved[frame * 2u + 1u] = right[offset + frame];
            }
            ok = writeBytes(file, interleaved.data(), count * 2u * sizeof(float));
        }
        const bool closed = std::fclose(file) == 0;
        if (!ok || !closed || std::rename(temporaryPath, path) != 0) {
            std::remove(temporaryPath);
            return false;
        }
        return true;
    }

    bool writeLaneFile(std::size_t lane)
    {
        char leaf[32] {};
        char temporaryLeaf[40] {};
        std::snprintf(leaf, sizeof(leaf), "thru-lane-%u.wav",
                      static_cast<unsigned>(lane + 1u));
        std::snprintf(temporaryLeaf, sizeof(temporaryLeaf), "thru-lane-%u.wav.tmp",
                      static_cast<unsigned>(lane + 1u));
        std::array<char, kPersistencePathSize> path {};
        std::array<char, kPersistencePathSize> temporary {};
        if (!persistencePath(leaf, path.data(), path.size())
            || !persistencePath(temporaryLeaf, temporary.data(), temporary.size())) {
            return false;
        }
        return writeLaneWavFile(lane, path.data(), temporary.data(), true);
    }

    bool writeClipCapture(std::size_t lane, int clipIndex)
    {
        if (lane >= kLaneCount || clipIndex < 0
            || clipIndex >= static_cast<int>(kClipSlotCount)) return false;
        ClipSlot& clip = clipSlots[static_cast<std::size_t>(clipIndex)];
        std::array<char, kPersistencePathSize> path {};
        std::array<char, kPersistencePathSize> temporary {};
        if (clip.laneSourceKinds[lane] == LaneSourceKind::capture
            && clip.laneSourcePaths[lane][0] != '\0') {
            std::snprintf(path.data(), path.size(), "%s",
                          clip.laneSourcePaths[lane].data());
        } else {
            std::array<char, kPersistencePathSize> leaf {};
            std::snprintf(leaf.data(), leaf.size(),
                          "captures/c%02d-lane-%u.wav", clipIndex + 1,
                          static_cast<unsigned>(lane + 1u));
            if (!persistencePath(leaf.data(), path.data(), path.size())) {
                return false;
            }
        }
        if (std::snprintf(temporary.data(), temporary.size(), "%s.tmp",
                          path.data()) >= static_cast<int>(temporary.size())) {
            return false;
        }
        if (!writeLaneWavFile(lane, path.data(), temporary.data(), false)) {
            return false;
        }
        clip.laneSourceKinds[lane] = LaneSourceKind::capture;
        std::snprintf(clip.laneSourcePaths[lane].data(),
                      clip.laneSourcePaths[lane].size(), "%s", path.data());
        const bool edited = std::strcmp(clip.laneSourceLabels[lane].data(),
                                        "SAVING TAKE") != 0;
        if (!edited) {
            std::snprintf(clip.laneSourceLabels[lane].data(),
                          clip.laneSourceLabels[lane].size(), "TAKE C%02d",
                          clipIndex + 1);
        }
        if (clipIndex == activeClip) {
            currentLaneSourceKinds[lane] = LaneSourceKind::capture;
            std::snprintf(currentLaneSourcePaths[lane].data(),
                          currentLaneSourcePaths[lane].size(), "%s", path.data());
            std::snprintf(laneSourceLabels[lane].data(),
                          laneSourceLabels[lane].size(), "%s",
                          clip.laneSourceLabels[lane].data());
        }
        return true;
    }

    bool serviceMaintenance()
    {
        if (engine.recording()) return false;
        if (clipFinishPending && clipRecording) finishClipRecording();
        bool ok = true;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if ((pendingRegionAnalysisMask & (1u << lane)) == 0u) continue;
            if (engine.laneFrames(lane) != 0u) {
                RegionMap regions;
                analyzeRegions(engine.laneData(lane, 0u),
                               engine.laneData(lane, 1u),
                               engine.laneFrames(lane),
                               engine.laneSampleRate(lane), regions);
                installRegionMap(lane, regions);
            }
            pendingRegionAnalysisMask &= ~(1u << lane);
        }
        if (!persistenceEnabled) return true;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if ((pendingCaptureSaveMask & (1u << lane)) == 0u) continue;
            const int owner = pendingCaptureClips[lane];
            if (!writeClipCapture(lane, owner)) {
                ok = false;
                continue;
            }
            pendingCaptureSaveMask &= ~(1u << lane);
            pendingCaptureClips[lane] = -1;
        }
        if (persistentStateDirty || pendingCaptureSaveMask != 0u) {
            captureActiveClip(false);
            const bool metadataSaved = writeMetadataFiles();
            ok = metadataSaved && ok;
            if (metadataSaved && pendingCaptureSaveMask == 0u) {
                persistentStateDirty = false;
            }
        }
        return ok;
    }

    bool readLaneFile(std::size_t lane)
    {
        if (lane >= kLaneCount) return false;
        char leaf[32] {};
        std::snprintf(leaf, sizeof(leaf), "thru-lane-%u.wav",
                      static_cast<unsigned>(lane + 1u));
        std::array<char, kPersistencePathSize> path {};
        if (!persistencePath(leaf, path.data(), path.size())) return false;
        std::FILE* file = std::fopen(path.data(), "rb");
        if (!file) return false;

        char id[4] {};
        std::uint32_t riffSize = 0u;
        bool ok = std::fread(id, 1u, 4u, file) == 4u
            && std::memcmp(id, "RIFF", 4u) == 0
            && readU32(file, riffSize)
            && std::fread(id, 1u, 4u, file) == 4u
            && std::memcmp(id, "WAVE", 4u) == 0;
        (void)riffSize;
        std::uint16_t format = 0u;
        std::uint16_t channels = 0u;
        std::uint16_t bits = 0u;
        std::uint16_t blockAlign = 0u;
        std::uint32_t rate = 0u;
        std::uint32_t dataBytes = 0u;
        long dataOffset = -1l;
        bool haveFormat = false;
        while (ok && std::fread(id, 1u, 4u, file) == 4u) {
            std::uint32_t chunkSize = 0u;
            if (!readU32(file, chunkSize)) { ok = false; break; }
            const long chunkStart = std::ftell(file);
            if (chunkStart < 0l) { ok = false; break; }
            if (std::memcmp(id, "fmt ", 4u) == 0 && chunkSize >= 16u) {
                std::uint32_t byteRate = 0u;
                ok = readU16(file, format) && readU16(file, channels)
                    && readU32(file, rate) && readU32(file, byteRate)
                    && readU16(file, blockAlign) && readU16(file, bits);
                (void)byteRate;
                haveFormat = ok;
            } else if (std::memcmp(id, "data", 4u) == 0) {
                dataOffset = chunkStart;
                dataBytes = chunkSize;
            }
            const long next = chunkStart + static_cast<long>(chunkSize + (chunkSize & 1u));
            if (ok && std::fseek(file, next, SEEK_SET) != 0) ok = false;
            if (haveFormat && dataOffset >= 0l) break;
        }
        const std::size_t frames = blockAlign == 0u ? 0u : dataBytes / blockAlign;
        ok = ok && haveFormat && dataOffset >= 0l && format == 3u
            && channels == 2u && bits == 32u && blockAlign == 8u
            && rate >= 8000u && rate <= 192000u
            && frames != 0u && frames <= laneCapacityFrames
            && std::fseek(file, dataOffset, SEEK_SET) == 0;

        float* left = engine.writableLane(lane, 0u);
        float* right = engine.writableLane(lane, 1u);
        std::array<float, kWavIoFrames * 2u> interleaved {};
        for (std::size_t offset = 0u; ok && offset < frames; offset += kWavIoFrames) {
            const std::size_t count = std::min(kWavIoFrames, frames - offset);
            ok = std::fread(interleaved.data(), sizeof(float), count * 2u, file)
                == count * 2u;
            for (std::size_t frame = 0u; ok && frame < count; ++frame) {
                const float l = interleaved[frame * 2u];
                const float r = interleaved[frame * 2u + 1u];
                left[offset + frame] = std::isfinite(l) ? clampValue(l, -1.0f, 1.0f) : 0.0f;
                right[offset + frame] = std::isfinite(r) ? clampValue(r, -1.0f, 1.0f) : 0.0f;
            }
        }
        std::fclose(file);
        if (!ok || !engine.adoptLane(lane, frames, static_cast<float>(rate))) return false;
        RegionMap regions;
        analyzeRegions(left, right, frames, static_cast<float>(rate), regions);
        installRegionMap(lane, regions);
        analyzeWaveform(left, right, frames,
                        waveMinimum[lane], waveMaximum[lane]);
        waveDirtyMask &= ~(1u << lane);
        waveTextDirty = true;
        std::snprintf(laneSourceLabels[lane].data(), laneSourceLabels[lane].size(),
                      "SAVED");
        return true;
    }

    void reconcileRestoredClipSources()
    {
        if (activeClip < 0
            || activeClip >= static_cast<int>(clipSlots.size())) return;
        const ClipSlot& clip = clipSlots[static_cast<std::size_t>(activeClip)];
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if (!engine.laneLoaded(lane)
                || clip.laneSourcePaths[lane][0] == '\0') continue;
            std::snprintf(currentLaneSourcePaths[lane].data(),
                          currentLaneSourcePaths[lane].size(), "%s",
                          clip.laneSourcePaths[lane].data());
            if (clip.laneSourceLabels[lane][0] != '\0') {
                std::snprintf(laneSourceLabels[lane].data(),
                              laneSourceLabels[lane].size(), "%s",
                              clip.laneSourceLabels[lane].data());
            }
        }
        queueClipSources(activeClip);
    }

    bool readStateTextFromPath(
        const char* path,
        std::array<char, kStateTextSize>& text) const
    {
        if (!path) return false;
        std::FILE* file = std::fopen(path, "rb");
        if (!file) return false;
        const std::size_t length = std::fread(
            text.data(), 1u, text.size() - 1u, file);
        const bool complete = std::feof(file) != 0;
        std::fclose(file);
        if (!complete || length == 0u) return false;
        text[length] = '\0';
        return true;
    }

    bool stateFileGeneration(const char* path, std::uint64_t& generation) const
    {
        std::array<char, kStateTextSize> text {};
        float version = 0.0f;
        float complete = 0.0f;
        return readStateTextFromPath(path, text)
            && jsonNumber(text.data(), "version", version)
            && static_cast<int>(version) == 6
            && jsonNumber(text.data(), "complete", complete)
            && static_cast<int>(complete) == 1
            && jsonUnsigned64(text.data(), "generation", generation)
            && generation != 0u;
    }

    bool readStateFileFromPath(const char* path)
    {
        std::array<char, kStateTextSize> text {};
        if (!readStateTextFromPath(path, text)) return false;
        return restoreState(text.data());
    }

    void loadPersistentState()
    {
        if (!persistenceEnabled) return;
        std::array<std::array<char, kPersistencePathSize>, 2u> statePaths {};
        std::array<std::array<char, kPersistencePathSize>, 2u> clipPaths {};
        bool pathsReady = persistencePath(
                "thru-state.json", statePaths[0u].data(), statePaths[0u].size())
            && persistencePath(
                "thru-clips.bin", clipPaths[0u].data(), clipPaths[0u].size());
        if (pathsReady) {
            const int stateLength = std::snprintf(
                statePaths[1u].data(), statePaths[1u].size(), "%s.bak",
                statePaths[0u].data());
            const int clipLength = std::snprintf(
                clipPaths[1u].data(), clipPaths[1u].size(), "%s.bak",
                clipPaths[0u].data());
            pathsReady = stateLength > 0 && clipLength > 0
                && static_cast<std::size_t>(stateLength) < statePaths[1u].size()
                && static_cast<std::size_t>(clipLength) < clipPaths[1u].size();
        }

        bool stateLoaded = false;
        bool clipsLoaded = false;
        if (pathsReady) {
            std::array<std::uint64_t, 2u> stateGenerations {};
            std::array<std::uint64_t, 2u> clipGenerations {};
            std::array<bool, 2u> stateValid {};
            std::array<bool, 2u> clipValid {};
            for (std::size_t index = 0u; index < 2u; ++index) {
                stateValid[index] = stateFileGeneration(
                    statePaths[index].data(), stateGenerations[index]);
                clipValid[index] = clipFileGeneration(
                    clipPaths[index].data(), clipGenerations[index]);
            }
            int stateChoice = -1;
            int clipChoice = -1;
            std::uint64_t newest = 0u;
            for (std::size_t state = 0u; state < 2u; ++state) {
                for (std::size_t clips = 0u; clips < 2u; ++clips) {
                    if (!stateValid[state] || !clipValid[clips]
                        || stateGenerations[state] != clipGenerations[clips]
                        || stateGenerations[state] <= newest) continue;
                    newest = stateGenerations[state];
                    stateChoice = static_cast<int>(state);
                    clipChoice = static_cast<int>(clips);
                }
            }
            if (stateChoice >= 0 && clipChoice >= 0) {
                stateLoaded = readStateFileFromPath(
                    statePaths[static_cast<std::size_t>(stateChoice)].data());
                clipsLoaded = stateLoaded && readClipFileFromPath(
                    clipPaths[static_cast<std::size_t>(clipChoice)].data(), newest);
            }
            if (!stateLoaded) {
                stateLoaded = readStateFileFromPath(statePaths[0u].data())
                    || readStateFileFromPath(statePaths[1u].data());
            }
            if (!clipsLoaded) {
                clipsLoaded = readClipFile();
            }
        }
        persistenceRestored = stateLoaded || clipsLoaded;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if (readLaneFile(lane)) persistenceRestored = true;
        }
        reconcileRestoredClipSources();
    }

    bool savePersistentState()
    {
        if (!persistenceEnabled) return false;
        if (engine.recording()) finishRecording();
        if (clipRecording) finishClipRecording();
        bool ok = true;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if ((pendingCaptureSaveMask & (1u << lane)) == 0u) continue;
            const int owner = pendingCaptureClips[lane];
            const bool captureSaved = writeClipCapture(lane, owner);
            ok = captureSaved && ok;
            if (captureSaved) {
                pendingCaptureSaveMask &= ~(1u << lane);
                pendingCaptureClips[lane] = -1;
            }
        }
        ok = writeMetadataFiles() && ok;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            ok = writeLaneFile(lane) && ok;
        }
        if (!ok && g_host && g_host->log) {
            g_host->log("s3g Thru could not save all persistent state");
        }
        return ok;
    }

    bool restoreState(const char* json)
    {
        float value = 0.0f;
        std::uint64_t loadedGeneration = 0u;
        if (!jsonNumber(json, "version", value)
            || static_cast<int>(value) != 6
            || !jsonNumber(json, "complete", value)
            || static_cast<int>(value) != 1
            || !jsonUnsigned64(json, "generation", loadedGeneration)) return false;
        if (jsonNumber(json, "locus", value)) controls.locus = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "field", value)) controls.field = clampValue(value, 0.02f, 1.0f);
        if (jsonNumber(json, "speed", value)) controls.speed = clampValue(value, 0.05f, 4.0f);
        if (jsonNumber(json, "join", value)) controls.join = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "manual_lane", value)) controls.manualLane = clampValue(value - 1.0f, 0.0f, 3.0f);
        if (jsonNumber(json, "lane_cycles", value)) controls.laneCycles = clampValue(value, 0.125f, 16.0f);
        if (jsonNumber(json, "algorithm", value)) controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(clampValue(static_cast<int>(value), 0, static_cast<int>(Engine::kPlaybackAlgorithmCount) - 1));
        if (jsonNumber(json, "sample_mode", value)) controls.sampleMode = static_cast<Engine::SampleMode>(clampValue(static_cast<int>(value), 0, static_cast<int>(Engine::kSampleModeCount) - 1));
        if (jsonNumber(json, "slice_region_mode", value)) controls.sampleRegionMode = static_cast<Engine::RegionMode>(clampValue(static_cast<int>(value), 0, static_cast<int>(Engine::kRegionModeCount) - 1));
        if (jsonNumber(json, "slice_regions", value)) controls.sampleRegionCount = clampValue(value, 1.0f, 64.0f);
        if (jsonNumber(json, "pad_mode", value)) padMode = static_cast<PadMode>(clampValue(static_cast<int>(value), 0, 3));
        if (jsonNumber(json, "lane_variation", value)) laneVariation = static_cast<LaneVariation>(clampValue(static_cast<int>(value), 0, 4));
        if (jsonNumber(json, "clip_record_mode", value)) clipRecordMode = static_cast<ClipRecordMode>(clampValue(static_cast<int>(value), 0, 1));
        if (jsonNumber(json, "preview_level", value)) previewLevel = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "grain_quality", value)) {
            const int voices = clampValue(static_cast<int>(std::lround(value)), 4, 16);
            grainVoiceLimit = voices <= 4 ? 4u : (voices <= 8 ? 8u : 16u);
            controls.grainVoiceLimit = static_cast<float>(grainVoiceLimit);
        }
        if (jsonNumber(json, "launch_quantize", value)) launchQuantize =
            static_cast<LaunchQuantize>(clampValue(static_cast<int>(value), 0, 2));
        if (jsonNumber(json, "output_gain", value)) outputGain = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "record_gain", value)) recordGain = clampValue(value, 0.0f, 4.0f);
        if (jsonNumber(json, "pressure_mode", value)) pressureMode = clampValue(static_cast<int>(value), 0, 1);
        if (jsonNumber(json, "root_note", value)) rootNote = clampValue(static_cast<int>(value), 24, 96);
        if (jsonNumber(json, "pad_octave", value)) padOctave = clampValue(static_cast<int>(value), -3, 3);
        if (jsonNumber(json, "record_lane", value) && !engine.recording()) selectedLane = static_cast<std::size_t>(clampValue(static_cast<int>(value), 1, 4) - 1);
        if (jsonNumber(json, "record_source", value) && !engine.recording()) recordSource = clampValue(static_cast<int>(value), 0, 1);
        if (jsonNumber(json, "motion_mode", value)) controls.motion = static_cast<Engine::MotionMode>(clampValue(static_cast<int>(value), 0, 7));
        if (jsonNumber(json, "motion_sound", value)) controls.motionSound = static_cast<Engine::MotionSound>(clampValue(static_cast<int>(value), 0, static_cast<int>(Engine::kMotionSoundCount) - 1));
        if (jsonNumber(json, "motion_rate", value)) controls.motionSoundRateHz = clampValue(value, 0.25f, 20.0f);
        if (jsonNumber(json, "motion_rate_basis", value)) controls.motionRateBasis = static_cast<Engine::MotionRateBasis>(clampValue(static_cast<int>(value), 0, 1));
        if (jsonNumber(json, "motion_path_rate", value)) controls.motionPathRate = clampValue(value, 0.01f, 80.0f);
        if (jsonNumber(json, "motion_step", value)) controls.motionStep = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "motion_seed", value)) controls.motionSeed = clampValue(value, 1.0f, 9999.0f);
        if (jsonNumber(json, "lanes_mode", value)) controls.lanes = static_cast<Engine::LanesMode>(clampValue(static_cast<int>(value), 0, 2));
        if (jsonNumber(json, "lane_path", value)) controls.lanePath = static_cast<Engine::LanePath>(clampValue(static_cast<int>(value), 0, 7));
        if (jsonNumber(json, "lanes_stretch", value)) controls.lanesStretch = value >= 0.5f;
        if (jsonNumber(json, "stretch_ratio", value)) controls.lanesStretchRatio = clampValue(value, 0.25f, 4.0f);
        if (jsonNumber(json, "lane_1_gain", value)) controls.laneGains[0u] = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "lane_2_gain", value)) controls.laneGains[1u] = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "lane_3_gain", value)) controls.laneGains[2u] = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "lane_4_gain", value)) controls.laneGains[3u] = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "grain_mode", value)) controls.grains = static_cast<Engine::GrainMode>(clampValue(static_cast<int>(value), 0, 3));
        if (jsonNumber(json, "grain_size", value)) controls.grainSizeMs = clampValue(value, 8.0f, 1000.0f);
        if (jsonNumber(json, "grain_density", value)) controls.grainDensityHz = clampValue(value, 1.0f, 96.0f);
        if (jsonNumber(json, "grain_pitch", value)) controls.grainPitch = clampValue(value, 0.25f, 4.0f);
        if (jsonNumber(json, "grain_window", value)) controls.grainWindow = static_cast<Engine::GrainWindow>(clampValue(static_cast<int>(value), 0, 5));
        if (jsonNumber(json, "grain_spread", value)) controls.grainStereoSpread = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "cutups_mode", value)) controls.cutups = static_cast<Engine::CutupsMode>(clampValue(static_cast<int>(value), 0, static_cast<int>(Engine::kCutupsModeCount) - 1));
        if (jsonNumber(json, "motion_travel", value)) controls.motionTravel = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "motion_jitter", value)) controls.motionJitter = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "packet_duty", value)) controls.packetDuty = clampValue(value, 0.02f, 0.98f);
        if (jsonNumber(json, "motor_rate", value)) controls.motorRateHz = clampValue(value, 0.05f, 20.0f);
        if (jsonNumber(json, "motor_symmetry", value)) controls.motorSymmetry = clampValue(value, 0.05f, 0.95f);
        if (jsonNumber(json, "motor_shape", value)) controls.motorShape = static_cast<Engine::MotorShape>(clampValue(static_cast<int>(value), 0, static_cast<int>(Engine::kMotorShapeCount) - 1));
        if (jsonNumber(json, "segment_trigger", value)) controls.segmentTrigger = static_cast<Engine::SegmentTrigger>(clampValue(static_cast<int>(value), 0, 2));
        if (jsonNumber(json, "segment_overlap", value)) controls.segmentOverlap = static_cast<Engine::SegmentOverlap>(clampValue(static_cast<int>(value), 0, 1));
        if (jsonNumber(json, "segment_layers", value)) controls.segmentLayerLimit = clampValue(value, 1.0f, 4.0f);
        if (jsonNumber(json, "segment_rate", value)) controls.segmentRateHz = clampValue(value, 0.25f, 80.0f);
        if (jsonNumber(json, "segment_repeats", value)) controls.segmentRepeats = clampValue(value, 1.0f, 16.0f);
        if (jsonNumber(json, "segment_step", value)) controls.segmentStep = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "segment_pitch", value)) controls.segmentPitchScatter = clampValue(value, 0.0f, 12.0f);
        if (jsonNumber(json, "segment_level", value)) controls.segmentLevelVariation = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "segment_curve", value)) controls.segmentIntervalCurve = clampValue(value, -1.0f, 1.0f);
        if (jsonNumber(json, "lane_blend", value)) controls.laneBlend = static_cast<Engine::LaneBlendMode>(clampValue(static_cast<int>(value), 0, 1));
        if (jsonNumber(json, "lane_path_shape", value)) controls.lanePathShape = static_cast<Engine::LanePathShape>(clampValue(static_cast<int>(value), 0, 3));
        if (jsonNumber(json, "lane_path_offset", value)) controls.lanePathOffset = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "lane_path_skew", value)) controls.lanePathSkew = clampValue(value, 0.02f, 0.98f);
        if (jsonNumber(json, "lane_path_curve", value)) controls.lanePathCurve = clampValue(value, -1.0f, 1.0f);
        if (jsonNumber(json, "lane_slew", value)) controls.laneSlewMs = clampValue(value, 0.0f, 500.0f);
        if (jsonNumber(json, "grain_spray", value)) controls.grainSpray = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "grain_bias", value)) controls.grainBias = static_cast<Engine::GrainBias>(clampValue(static_cast<int>(value), 0, 2));
        if (jsonNumber(json, "grain_pitch_spray", value)) controls.grainPitchSpray = clampValue(value, 0.0f, 24.0f);
        if (jsonNumber(json, "grain_reverse", value)) controls.grainReverseChance = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "grain_size_variation", value)) controls.grainSizeVariation = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "grain_level_variation", value)) controls.grainLevelVariation = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "grain_timing", value)) controls.grainTiming = static_cast<Engine::GrainTiming>(clampValue(static_cast<int>(value), 0, 1));
        if (jsonNumber(json, "grain_timing_scatter", value)) controls.grainTimingScatter = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "grain_envelope_skew", value)) controls.grainEnvelopeSkew = clampValue(value, -1.0f, 1.0f);
        if (jsonNumber(json, "grain_process", value)) controls.grainProcess = static_cast<Engine::GrainProcess>(clampValue(static_cast<int>(value), 0, static_cast<int>(Engine::kGrainProcessCount) - 1));
        if (jsonNumber(json, "grain_process_amount", value)) controls.grainProcessAmount = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "grain_regions", value)) controls.grainRegionCount = clampValue(value, 1.0f, 64.0f);
        if (jsonNumber(json, "grain_advance", value)) controls.grainAdvance = static_cast<Engine::GrainAdvance>(clampValue(static_cast<int>(value), 0, 1));
        if (jsonNumber(json, "cut_clock", value)) controls.cutClock = static_cast<Engine::CutClock>(clampValue(static_cast<int>(value), 0, 1));
        if (jsonNumber(json, "cut_division", value)) controls.cutDivision = static_cast<Engine::CutDivision>(clampValue(static_cast<int>(value), 0, 5));
        if (jsonNumber(json, "cut_rate", value)) controls.cutRateHz = clampValue(value, 0.25f, 64.0f);
        if (jsonNumber(json, "cut_time_variation", value)) controls.cutTimeVariation = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "cut_gate", value)) controls.cutGate = clampValue(value, 0.05f, 1.0f);
        if (jsonNumber(json, "cut_join", value)) controls.cutJoinMs = clampValue(value, 0.0f, 100.0f);
        if (jsonNumber(json, "cut_regions", value)) controls.cutRegionCount = clampValue(value, 1.0f, 64.0f);
        if (jsonNumber(json, "cut_repeats", value)) controls.cutRepeats = clampValue(value, 1.0f, 16.0f);
        if (jsonNumber(json, "cut_reverse", value)) controls.cutReverseChance = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "cut_pitch_variation", value)) controls.cutPitchVariation = clampValue(value, 0.0f, 24.0f);
        if (jsonNumber(json, "cut_level_variation", value)) controls.cutLevelVariation = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "cut_lane_order", value)) controls.cutLaneOrder = static_cast<Engine::CutLaneOrder>(clampValue(static_cast<int>(value), 0, 6));
        if (jsonNumber(json, "cut_poly_path", value)) controls.cutPolyPath = static_cast<Engine::CutPolyPath>(clampValue(static_cast<int>(value), 0, 3));
        if (jsonNumber(json, "cut_region_mode", value)) controls.cutRegionMode = static_cast<Engine::RegionMode>(clampValue(static_cast<int>(value), 0, 1));
        if (jsonNumber(json, "glitch_mode", value)) controls.glitchMode = static_cast<Engine::GlitchMode>(clampValue(static_cast<int>(value), 0, 4));
        if (jsonNumber(json, "glitch_clock", value)) controls.glitchClock = static_cast<Engine::CutClock>(clampValue(static_cast<int>(value), 0, 1));
        if (jsonNumber(json, "glitch_division", value)) controls.glitchDivision = static_cast<Engine::CutDivision>(clampValue(static_cast<int>(value), 0, 5));
        if (jsonNumber(json, "glitch_rate", value)) controls.glitchRateHz = clampValue(value, 0.25f, 64.0f);
        if (jsonNumber(json, "glitch_slice", value)) controls.glitchSliceMs = clampValue(value, 2.0f, 160.0f);
        if (jsonNumber(json, "glitch_repeats", value)) controls.glitchRepeats = clampValue(value, 1.0f, 16.0f);
        if (jsonNumber(json, "glitch_chance", value)) controls.glitchChance = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "glitch_mix", value)) controls.glitchMix = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "glitch_steps", value)) controls.glitchSteps = clampValue(value, 1.0f, 16.0f);
        if (jsonNumber(json, "glitch_pulses", value)) controls.glitchPulses = clampValue(value, 0.0f, 16.0f);
        if (jsonNumber(json, "glitch_rotate", value)) controls.glitchRotate = clampValue(value, 0.0f, 15.0f);
        if (jsonNumber(json, "glitch_jump", value)) controls.glitchJumpMs = clampValue(value, 0.0f, 160.0f);
        if (jsonNumber(json, "glitch_pitch", value)) controls.glitchPitch = clampValue(value, 0.0f, 24.0f);
        if (jsonNumber(json, "glitch_reverse", value)) controls.glitchReverseChance = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "glitch_decay", value)) controls.glitchDecay = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "glitch_seed", value)) controls.glitchSeed = clampValue(value, 1.0f, 9999.0f);
        if (jsonNumber(json, "effect_tone", value)) controls.effectTone = clampValue(value, -1.0f, 1.0f);
        if (jsonNumber(json, "effect_drive", value)) controls.effectDrive = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "effect_bits", value)) controls.effectBits = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "effect_rate", value)) controls.effectRate = clampValue(value, 1.0f, 16.0f);
        if (jsonNumber(json, "effect_delay", value)) controls.effectDelayMs = clampValue(value, 1.0f, 750.0f);
        if (jsonNumber(json, "effect_feedback", value)) controls.effectFeedback = clampValue(value, 0.0f, 0.9f);
        if (jsonNumber(json, "effect_width", value)) controls.effectWidth = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "effect_mix", value)) controls.effectMix = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "tempo_bpm", value)) {
            tempoBpm = clampValue(value, 40.0f, 240.0f);
        }
        if (jsonNumber(json, "tempo_swing", value)) {
            tempoSwing = clampValue(value / 100.0f, 0.5f, 0.75f);
        }
        if (jsonNumber(json, "chain_enabled", value)) chainEnabled = value >= 0.5f;
        persistenceGeneration = loadedGeneration;
        return true;
    }
};

void* createInstance(const char* moduleDir, const char*)
{
    std::unique_ptr<ThruInstance> instance(new (std::nothrow) ThruInstance());
    if (!instance) {
        if (g_host && g_host->log) g_host->log("s3g Thru instance allocation failed");
        return nullptr;
    }
    if (!instance->prepare(moduleDir)) {
        if (g_host && g_host->log) g_host->log(instance->lastError.data());
        return nullptr;
    }
    if (g_host && g_host->log) g_host->log("s3g Thru instance created");
    return instance.release();
}

void destroyInstance(void* opaque)
{
    auto* instance = static_cast<ThruInstance*>(opaque);
    if (!instance) return;
    instance->stopSampleWorker();
    instance->savePersistentState();
    delete instance;
}

void onMidi(void* opaque, const uint8_t* message, int length, int source)
{
    auto* instance = static_cast<ThruInstance*>(opaque);
    if (!instance || !message || length < 3) return;
    const int type = message[0] & 0xf0;
    const int note = message[1] & 0x7f;
    const int value = message[2] & 0x7f;
    const bool hardwarePad = source == MOVE_MIDI_SOURCE_INTERNAL
        && note >= kFirstPadNote && note <= kLastPadNote;
    if (source == MOVE_MIDI_SOURCE_INTERNAL
        && (type == 0x90 || type == 0x80) && !hardwarePad) {
        return;
    }
    if (type == 0x90 && value != 0) {
        if (hardwarePad) instance->padNoteOn(note, value);
        else instance->noteOn(note, value);
    } else if (type == 0x80 || (type == 0x90 && value == 0)) {
        if (hardwarePad) instance->padNoteOff(note);
        else instance->noteOff(note);
    } else if (type == 0xa0) {
        int pressureNote = note;
        int sourcePad = -1;
        if (source == MOVE_MIDI_SOURCE_INTERNAL && hardwarePad) {
            const int index = note - kFirstPadNote;
            pressureNote = instance->padMappedNotes[static_cast<std::size_t>(index)];
            sourcePad = index;
        }
        if (pressureNote >= 0) instance->aftertouch(pressureNote, value, sourcePad);
    } else if (type == 0xb0) {
        if (note == 120 || note == 123) instance->allNotesOff();
        else if (source != MOVE_MIDI_SOURCE_INTERNAL) instance->applyMidiCc(note, value);
    }
}

void setParam(void* opaque, const char* key, const char* value)
{
    auto* instance = static_cast<ThruInstance*>(opaque);
    if (!instance || !key || !value) return;
    auto& controls = instance->controls;

    if (std::strcmp(key, "maintenance") != 0
        && std::strcmp(key, "performance_reset") != 0
        && std::strcmp(key, "all_notes_off") != 0
        && std::strcmp(key, "stack_scan_paused") != 0
        && std::strcmp(key, "state") != 0) {
        instance->markPersistentStateDirty();
    }

    if (std::strcmp(key, "state") == 0) { instance->restoreState(value); return; }
    if (std::strcmp(key, "locus") == 0) { controls.locus = clampValue(parseFloat(value, controls.locus), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "field") == 0) { controls.field = clampValue(parseFloat(value, controls.field), 0.02f, 1.0f); return; }
    if (std::strcmp(key, "speed") == 0) { controls.speed = clampValue(parseFloat(value, controls.speed), 0.05f, 4.0f); return; }
    if (std::strcmp(key, "manual_lane") == 0) { controls.manualLane = clampValue(parseFloat(value, controls.manualLane + 1.0f) - 1.0f, 0.0f, 3.0f); return; }
    if (std::strcmp(key, "lane_cycles") == 0) { controls.laneCycles = clampValue(parseFloat(value, controls.laneCycles), 0.125f, 16.0f); return; }
    if (std::strcmp(key, "stack_scan_paused") == 0) { instance->stackScanPaused = isTruthy(value); return; }
    if (std::strcmp(key, "algorithm") == 0) { controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(parseEnum(value, kAlgorithmNames, static_cast<int>(controls.algorithm))); return; }
    if (std::strcmp(key, "sample_mode") == 0) { controls.sampleMode = static_cast<Engine::SampleMode>(parseEnum(value, kSampleModeNames, static_cast<int>(controls.sampleMode))); return; }
    if (std::strcmp(key, "slice_region_mode") == 0) { controls.sampleRegionMode = static_cast<Engine::RegionMode>(parseEnum(value, kRegionModeNames, static_cast<int>(controls.sampleRegionMode))); return; }
    if (std::strcmp(key, "slice_regions") == 0) { controls.sampleRegionCount = clampValue(parseFloat(value, controls.sampleRegionCount), 1.0f, 64.0f); return; }
    if (std::strcmp(key, "pad_mode") == 0) { instance->padMode = static_cast<PadMode>(parseEnum(value, kPadModeNames, static_cast<int>(instance->padMode))); instance->allNotesOff(); return; }
    if (std::strcmp(key, "lane_variation") == 0) { instance->laneVariation = static_cast<LaneVariation>(parseEnum(value, kLaneVariationNames, static_cast<int>(instance->laneVariation))); return; }
    if (std::strcmp(key, "preview_level") == 0) { instance->previewLevel = clampValue(parseFloat(value, instance->previewLevel), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "clip_rate") == 0) { if (ClipSlot* clip = instance->activeClipSlot()) clip->clipRate = clampValue(parseFloat(value, clip->clipRate), 0.25f, 4.0f); return; }
    if (std::strcmp(key, "clip_record_mode") == 0) { instance->clipRecordMode = static_cast<ClipRecordMode>(parseEnum(value, kClipRecordModeNames, static_cast<int>(instance->clipRecordMode))); return; }
    if (std::strcmp(key, "clip_timing") == 0) { if (ClipSlot* clip = instance->activeClipSlot()) clip->timingMode = static_cast<ClipTimingMode>(parseEnum(value, kClipTimingModeNames, static_cast<int>(clip->timingMode))); return; }
    if (std::strcmp(key, "launch_quantize") == 0) { instance->launchQuantize = static_cast<LaunchQuantize>(parseEnum(value, kLaunchQuantizeNames, static_cast<int>(instance->launchQuantize))); return; }
    if (std::strcmp(key, "clip_record") == 0) { if (isTruthy(value)) instance->toggleClipRecording(); return; }
    if (std::strcmp(key, "clip_clear") == 0) { if (isTruthy(value)) instance->clearActiveClip(); return; }
    if (std::strcmp(key, "clip_take_undo") == 0) { if (isTruthy(value)) instance->undoLastClipTake(); return; }
    if (std::strcmp(key, "clip_reset") == 0) { instance->resetClip(parseInteger(value, -1)); return; }
    if (std::strcmp(key, "clip_automation") == 0) { char automationKey[32] {}; if (std::sscanf(value, "%31[^,]", automationKey) == 1) instance->captureClipParameter(lockParameterIndex(automationKey)); return; }
    if (std::strcmp(key, "join") == 0) { controls.join = clampValue(parseFloat(value, controls.join), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "output_gain") == 0) { instance->outputGain = clampValue(parseFloat(value, instance->outputGain), 0.0f, 2.0f); return; }
    if (std::strcmp(key, "pressure_mode") == 0) { instance->pressureMode = parseEnum(value, kPressureModeNames, instance->pressureMode); return; }
    if (std::strcmp(key, "root_note") == 0) { instance->rootNote = clampValue(parseInteger(value, instance->rootNote), 24, 96); return; }
    if (std::strcmp(key, "pad_octave") == 0) { instance->padOctave = clampValue(parseInteger(value, instance->padOctave), -3, 3); return; }
    if (std::strcmp(key, "performance_lane") == 0) { instance->setPerformanceLane(parseFloat(value, instance->performanceLane + 1.0f) - 1.0f); return; }
    if (std::strcmp(key, "record_lane") == 0 && !instance->engine.recording()) { instance->selectedLane = static_cast<std::size_t>(clampValue(parseInteger(value, static_cast<int>(instance->selectedLane) + 1), 1, 4) - 1); return; }
    if (std::strcmp(key, "record_source") == 0 && !instance->engine.recording()) { instance->recordSource = parseEnum(value, kRecordSourceNames, instance->recordSource); return; }
    if (std::strcmp(key, "record_gain") == 0) { instance->recordGain = clampValue(parseFloat(value, instance->recordGain), 0.0f, 4.0f); return; }
    if (std::strcmp(key, "monitor_level") == 0) { instance->monitorLevel = clampValue(parseFloat(value, instance->monitorLevel), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "record_gate") == 0) { if (isTruthy(value)) instance->startRecording(); else instance->finishRecording(); return; }
    if (std::strcmp(key, "clear_lane") == 0) { if (isTruthy(value)) instance->clearSelectedLane(); return; }
    if (std::strcmp(key, "sample_trim_start") == 0) { instance->sampleTrimStart = clampValue(parseFloat(value, instance->sampleTrimStart), 0.0f, 0.99f); if (instance->sampleTrimEnd <= instance->sampleTrimStart) instance->sampleTrimEnd = std::min(1.0f, instance->sampleTrimStart + 0.01f); return; }
    if (std::strcmp(key, "sample_trim_end") == 0) { instance->sampleTrimEnd = clampValue(parseFloat(value, instance->sampleTrimEnd), 0.01f, 1.0f); if (instance->sampleTrimStart >= instance->sampleTrimEnd) instance->sampleTrimStart = std::max(0.0f, instance->sampleTrimEnd - 0.01f); return; }
    if (std::strcmp(key, "sample_reverse") == 0) { if (isTruthy(value)) instance->reverseSelectedLane(); return; }
    if (std::strcmp(key, "sample_normalize") == 0) { if (isTruthy(value)) instance->normalizeSelectedLane(); return; }
    if (std::strcmp(key, "sample_fade") == 0) { if (isTruthy(value)) instance->fadeSelectedLane(); return; }
    if (std::strcmp(key, "sample_trim_apply") == 0) { if (isTruthy(value)) instance->trimSelectedLane(); return; }
    if (std::strcmp(key, "all_notes_off") == 0) { instance->allNotesOff(); return; }
    if (std::strcmp(key, "clip_playing") == 0) { instance->setClipPlaying(isTruthy(value)); return; }
    if (std::strcmp(key, "tempo_bpm") == 0) { instance->tempoBpm = clampValue(parseFloat(value, instance->tempoBpm), 40.0f, 240.0f); return; }
    if (std::strcmp(key, "tempo_swing") == 0) { instance->tempoSwing = clampValue(parseFloat(value, instance->tempoSwing * 100.0f) / 100.0f, 0.5f, 0.75f); return; }
    if (std::strcmp(key, "undo") == 0) { if (isTruthy(value)) instance->restoreUndo(); return; }
    if (std::strcmp(key, "maintenance") == 0) { if (isTruthy(value)) instance->serviceMaintenance(); return; }
    if (std::strcmp(key, "performance_reset") == 0) { if (isTruthy(value)) { instance->maximumRenderMicros.store(0u, std::memory_order_relaxed); instance->renderOverruns.store(0u, std::memory_order_relaxed); } return; }
    if (std::strcmp(key, "chain_enabled") == 0) { instance->chainEnabled = isTruthy(value); instance->markPersistentStateDirty(); return; }
    if (std::strcmp(key, "chain_slot") == 0) { instance->chainEditSlot = clampValue(parseInteger(value, instance->chainEditSlot + 1) - 1, 0, static_cast<int>(kClipChainLength) - 1); return; }
    if (std::strcmp(key, "chain_clip") == 0) { const auto& entry = instance->clipChain[static_cast<std::size_t>(instance->chainEditSlot)]; instance->setChainEntry(instance->chainEditSlot, clampValue(parseInteger(value, static_cast<int>(entry.clip) + 1) - 1, 0, static_cast<int>(kClipSlotCount) - 1), entry.repeats); return; }
    if (std::strcmp(key, "chain_repeats") == 0) { const auto& entry = instance->clipChain[static_cast<std::size_t>(instance->chainEditSlot)]; instance->setChainEntry(instance->chainEditSlot, entry.clip, parseInteger(value, entry.repeats)); return; }
    if (std::strcmp(key, "chain_clear") == 0) { if (isTruthy(value)) instance->clearChainEntry(instance->chainEditSlot); return; }
    if (std::strcmp(key, "chain_insert") == 0) { int slot = -1; int clip = -1; if (std::sscanf(value, "%d,%d", &slot, &clip) == 2) instance->insertChainEntry(slot, clip); return; }
    if (std::strcmp(key, "chain_delete") == 0) { instance->deleteChainEntry(parseInteger(value, -1)); return; }
    if (std::strcmp(key, "chain_move") == 0) { int source = -1; int destination = -1; if (std::sscanf(value, "%d,%d", &source, &destination) == 2) instance->moveChainEntry(source, destination); return; }
    if (std::strcmp(key, "clip_select") == 0) {
        instance->selectClip(parseInteger(value, -1));
        return;
    }
    if (std::strcmp(key, "clip_copy") == 0) {
        instance->copyActiveClip(parseInteger(value, -1));
        return;
    }
    if (std::strcmp(key, "load_lane_file") == 0) {
        char* end = nullptr;
        const long lane = std::strtol(value, &end, 10);
        if (end && *end == ',' && end[1] != '\0') {
            instance->requestLibrarySample(
                clampValue(static_cast<int>(lane), 1, 4) - 1, end + 1);
        }
        return;
    }
    if (std::strcmp(key, "preview_file") == 0) {
        if (value[0] != '\0') {
            instance->requestLibrarySample(static_cast<int>(instance->selectedLane),
                                           value, false, -1, true);
        }
        return;
    }
    if (std::strcmp(key, "preview_stop") == 0) {
        if (isTruthy(value)) {
            instance->previewActive = false;
            if (instance->sampleLoadState.load(std::memory_order_acquire)
                == static_cast<int>(SampleLoadState::previewing)) {
                instance->sampleRequestPreview = false;
                instance->sampleLoadState.store(
                    static_cast<int>(SampleLoadState::complete),
                    std::memory_order_release);
            } else if (instance->sampleRequestPreview) {
                instance->samplePreviewCancelled = true;
            }
        }
        return;
    }
    if (std::strcmp(key, "cue_store") == 0) {
        int slot = -1;
        float position = 0.0f;
        if (std::sscanf(value, "%d,%f", &slot, &position) == 2
            && std::isfinite(position)) instance->storeCue(slot, position);
        return;
    }
    if (std::strcmp(key, "cue_trigger") == 0) { instance->triggerCue(parseInteger(value, -1)); return; }
    if (std::strcmp(key, "cue_clear") == 0) { instance->clearCue(parseInteger(value, -1)); return; }

    if (std::strcmp(key, "motion_mode") == 0) { controls.motion = static_cast<Engine::MotionMode>(parseEnum(value, kMotionModeNames, static_cast<int>(controls.motion))); return; }
    if (std::strcmp(key, "motion_sound") == 0) { controls.motionSound = static_cast<Engine::MotionSound>(parseEnum(value, kMotionSoundNames, static_cast<int>(controls.motionSound))); return; }
    if (std::strcmp(key, "motion_rate") == 0) { controls.motionSoundRateHz = clampValue(parseFloat(value, controls.motionSoundRateHz), 0.25f, 20.0f); return; }
    if (std::strcmp(key, "motion_rate_basis") == 0) { controls.motionRateBasis = static_cast<Engine::MotionRateBasis>(parseEnum(value, kMotionRateBasisNames, static_cast<int>(controls.motionRateBasis))); return; }
    if (std::strcmp(key, "motion_path_rate") == 0) { controls.motionPathRate = clampValue(parseFloat(value, controls.motionPathRate), 0.01f, 80.0f); return; }
    if (std::strcmp(key, "motion_step") == 0) { controls.motionStep = clampValue(parseFloat(value, controls.motionStep), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "motion_seed") == 0) { controls.motionSeed = clampValue(parseFloat(value, controls.motionSeed), 1.0f, 9999.0f); return; }
    if (std::strcmp(key, "motion_travel") == 0) { controls.motionTravel = clampValue(parseFloat(value, controls.motionTravel), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "motion_jitter") == 0) { controls.motionJitter = clampValue(parseFloat(value, controls.motionJitter), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "packet_duty") == 0) { controls.packetDuty = clampValue(parseFloat(value, controls.packetDuty * 100.0f) / 100.0f, 0.02f, 0.98f); return; }
    if (std::strcmp(key, "motor_rate") == 0) { controls.motorRateHz = clampValue(parseFloat(value, controls.motorRateHz), 0.05f, 20.0f); return; }
    if (std::strcmp(key, "motor_symmetry") == 0) { controls.motorSymmetry = clampValue(parseFloat(value, controls.motorSymmetry * 100.0f) / 100.0f, 0.05f, 0.95f); return; }
    if (std::strcmp(key, "motor_shape") == 0) { controls.motorShape = static_cast<Engine::MotorShape>(parseEnum(value, kMotorShapeNames, static_cast<int>(controls.motorShape))); return; }
    if (std::strcmp(key, "segment_trigger") == 0) { controls.segmentTrigger = static_cast<Engine::SegmentTrigger>(parseEnum(value, kSegmentTriggerNames, static_cast<int>(controls.segmentTrigger))); return; }
    if (std::strcmp(key, "segment_overlap") == 0) { controls.segmentOverlap = static_cast<Engine::SegmentOverlap>(parseEnum(value, kSegmentOverlapNames, static_cast<int>(controls.segmentOverlap))); return; }
    if (std::strcmp(key, "segment_layers") == 0) { controls.segmentLayerLimit = clampValue(parseFloat(value, controls.segmentLayerLimit), 1.0f, 4.0f); return; }
    if (std::strcmp(key, "segment_rate") == 0) { controls.segmentRateHz = clampValue(parseFloat(value, controls.segmentRateHz), 0.25f, 80.0f); return; }
    if (std::strcmp(key, "segment_repeats") == 0) { controls.segmentRepeats = clampValue(parseFloat(value, controls.segmentRepeats), 1.0f, 16.0f); return; }
    if (std::strcmp(key, "segment_step") == 0) { controls.segmentStep = clampValue(parseFloat(value, controls.segmentStep), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "segment_pitch") == 0) { controls.segmentPitchScatter = clampValue(parseFloat(value, controls.segmentPitchScatter), 0.0f, 12.0f); return; }
    if (std::strcmp(key, "segment_level") == 0) { controls.segmentLevelVariation = clampValue(parseFloat(value, controls.segmentLevelVariation), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "segment_curve") == 0) { controls.segmentIntervalCurve = clampValue(parseFloat(value, controls.segmentIntervalCurve), -1.0f, 1.0f); return; }
    if (std::strcmp(key, "lanes_mode") == 0) { controls.lanes = static_cast<Engine::LanesMode>(parseEnum(value, kLanesModeNames, static_cast<int>(controls.lanes))); return; }
    if (std::strcmp(key, "lane_path") == 0) { controls.lanePath = static_cast<Engine::LanePath>(parseEnum(value, kLanePathNames, static_cast<int>(controls.lanePath))); return; }
    if (std::strcmp(key, "lane_blend") == 0) { controls.laneBlend = static_cast<Engine::LaneBlendMode>(parseEnum(value, kLaneBlendNames, static_cast<int>(controls.laneBlend))); return; }
    if (std::strcmp(key, "lane_path_shape") == 0) { controls.lanePathShape = static_cast<Engine::LanePathShape>(parseEnum(value, kLanePathShapeNames, static_cast<int>(controls.lanePathShape))); return; }
    if (std::strcmp(key, "lane_path_offset") == 0) { controls.lanePathOffset = clampValue(parseFloat(value, controls.lanePathOffset), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "lane_path_skew") == 0) { controls.lanePathSkew = clampValue(parseFloat(value, controls.lanePathSkew), 0.02f, 0.98f); return; }
    if (std::strcmp(key, "lane_path_curve") == 0) { controls.lanePathCurve = clampValue(parseFloat(value, controls.lanePathCurve), -1.0f, 1.0f); return; }
    if (std::strcmp(key, "lane_slew") == 0) { controls.laneSlewMs = clampValue(parseFloat(value, controls.laneSlewMs), 0.0f, 500.0f); return; }
    if (std::strcmp(key, "lanes_stretch") == 0) { controls.lanesStretch = parseEnum(value, kOnOffNames, controls.lanesStretch ? 1 : 0) != 0; return; }
    if (std::strcmp(key, "stretch_ratio") == 0) { controls.lanesStretchRatio = clampValue(parseFloat(value, controls.lanesStretchRatio), 0.25f, 4.0f); return; }
    if (std::strncmp(key, "lane_", 5u) == 0 && std::strlen(key) == 11u && std::strcmp(key + 6, "_gain") == 0) {
        const int lane = key[5] - '1';
        if (lane >= 0 && lane < 4) controls.laneGains[lane] = clampValue(parseFloat(value, controls.laneGains[lane]), 0.0f, 2.0f);
        return;
    }
    if (std::strcmp(key, "grain_mode") == 0) { controls.grains = static_cast<Engine::GrainMode>(parseEnum(value, kGrainModeNames, static_cast<int>(controls.grains))); return; }
    if (std::strcmp(key, "grain_size") == 0) { controls.grainSizeMs = clampValue(parseFloat(value, controls.grainSizeMs), 8.0f, 1000.0f); return; }
    if (std::strcmp(key, "grain_density") == 0) { controls.grainDensityHz = clampValue(parseFloat(value, controls.grainDensityHz), 1.0f, 96.0f); return; }
    if (std::strcmp(key, "grain_quality") == 0) { const int quality = parseEnum(value, kGrainQualityNames, instance->grainVoiceLimit == 4u ? 0 : (instance->grainVoiceLimit == 8u ? 1 : 2)); instance->grainVoiceLimit = quality == 0 ? 4u : (quality == 1 ? 8u : 16u); controls.grainVoiceLimit = static_cast<float>(instance->grainVoiceLimit); instance->clipControls.grainVoiceLimit = controls.grainVoiceLimit; return; }
    if (std::strcmp(key, "grain_pitch") == 0) { controls.grainPitch = clampValue(parseFloat(value, controls.grainPitch), 0.25f, 4.0f); return; }
    if (std::strcmp(key, "grain_window") == 0) { controls.grainWindow = static_cast<Engine::GrainWindow>(parseEnum(value, kGrainWindowNames, static_cast<int>(controls.grainWindow))); return; }
    if (std::strcmp(key, "grain_spread") == 0) { controls.grainStereoSpread = clampValue(parseFloat(value, controls.grainStereoSpread), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "grain_spray") == 0) { controls.grainSpray = clampValue(parseFloat(value, controls.grainSpray), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "grain_bias") == 0) { controls.grainBias = static_cast<Engine::GrainBias>(parseEnum(value, kGrainBiasNames, static_cast<int>(controls.grainBias))); return; }
    if (std::strcmp(key, "grain_pitch_spray") == 0) { controls.grainPitchSpray = clampValue(parseFloat(value, controls.grainPitchSpray), 0.0f, 24.0f); return; }
    if (std::strcmp(key, "grain_reverse") == 0) { controls.grainReverseChance = clampValue(parseFloat(value, controls.grainReverseChance * 100.0f) / 100.0f, 0.0f, 1.0f); return; }
    if (std::strcmp(key, "grain_size_variation") == 0) { controls.grainSizeVariation = clampValue(parseFloat(value, controls.grainSizeVariation), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "grain_level_variation") == 0) { controls.grainLevelVariation = clampValue(parseFloat(value, controls.grainLevelVariation), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "grain_timing") == 0) { controls.grainTiming = static_cast<Engine::GrainTiming>(parseEnum(value, kGrainTimingNames, static_cast<int>(controls.grainTiming))); return; }
    if (std::strcmp(key, "grain_timing_scatter") == 0) { controls.grainTimingScatter = clampValue(parseFloat(value, controls.grainTimingScatter), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "grain_envelope_skew") == 0) { controls.grainEnvelopeSkew = clampValue(parseFloat(value, controls.grainEnvelopeSkew), -1.0f, 1.0f); return; }
    if (std::strcmp(key, "grain_process") == 0) { controls.grainProcess = static_cast<Engine::GrainProcess>(parseEnum(value, kGrainProcessNames, static_cast<int>(controls.grainProcess))); return; }
    if (std::strcmp(key, "grain_process_amount") == 0) { controls.grainProcessAmount = clampValue(parseFloat(value, controls.grainProcessAmount), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "grain_regions") == 0) { controls.grainRegionCount = clampValue(parseFloat(value, controls.grainRegionCount), 1.0f, 64.0f); return; }
    if (std::strcmp(key, "grain_advance") == 0) { controls.grainAdvance = static_cast<Engine::GrainAdvance>(parseEnum(value, kGrainAdvanceNames, static_cast<int>(controls.grainAdvance))); return; }
    if (std::strcmp(key, "cutups_mode") == 0) { controls.cutups = static_cast<Engine::CutupsMode>(parseEnum(value, kCutupsModeNames, static_cast<int>(controls.cutups))); return; }
    if (std::strcmp(key, "cut_clock") == 0) { controls.cutClock = static_cast<Engine::CutClock>(parseEnum(value, kCutClockNames, static_cast<int>(controls.cutClock))); return; }
    if (std::strcmp(key, "cut_division") == 0) { controls.cutDivision = static_cast<Engine::CutDivision>(parseEnum(value, kCutDivisionNames, static_cast<int>(controls.cutDivision))); return; }
    if (std::strcmp(key, "cut_rate") == 0) { controls.cutRateHz = clampValue(parseFloat(value, controls.cutRateHz), 0.25f, 64.0f); return; }
    if (std::strcmp(key, "cut_time_variation") == 0) { controls.cutTimeVariation = clampValue(parseFloat(value, controls.cutTimeVariation), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "cut_gate") == 0) { controls.cutGate = clampValue(parseFloat(value, controls.cutGate * 100.0f) / 100.0f, 0.05f, 1.0f); return; }
    if (std::strcmp(key, "cut_join") == 0) { controls.cutJoinMs = clampValue(parseFloat(value, controls.cutJoinMs), 0.0f, 100.0f); return; }
    if (std::strcmp(key, "cut_regions") == 0) { controls.cutRegionCount = clampValue(parseFloat(value, controls.cutRegionCount), 1.0f, 64.0f); return; }
    if (std::strcmp(key, "cut_repeats") == 0) { controls.cutRepeats = clampValue(parseFloat(value, controls.cutRepeats), 1.0f, 16.0f); return; }
    if (std::strcmp(key, "cut_reverse") == 0) { controls.cutReverseChance = clampValue(parseFloat(value, controls.cutReverseChance * 100.0f) / 100.0f, 0.0f, 1.0f); return; }
    if (std::strcmp(key, "cut_pitch_variation") == 0) { controls.cutPitchVariation = clampValue(parseFloat(value, controls.cutPitchVariation), 0.0f, 24.0f); return; }
    if (std::strcmp(key, "cut_level_variation") == 0) { controls.cutLevelVariation = clampValue(parseFloat(value, controls.cutLevelVariation), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "cut_lane_order") == 0) { controls.cutLaneOrder = static_cast<Engine::CutLaneOrder>(parseEnum(value, kCutLaneOrderNames, static_cast<int>(controls.cutLaneOrder))); return; }
    if (std::strcmp(key, "cut_poly_path") == 0) { controls.cutPolyPath = static_cast<Engine::CutPolyPath>(parseEnum(value, kCutPolyPathNames, static_cast<int>(controls.cutPolyPath))); return; }
    if (std::strcmp(key, "cut_region_mode") == 0) { controls.cutRegionMode = static_cast<Engine::RegionMode>(parseEnum(value, kRegionModeNames, static_cast<int>(controls.cutRegionMode))); return; }
    if (std::strcmp(key, "glitch_mode") == 0) { controls.glitchMode = static_cast<Engine::GlitchMode>(parseEnum(value, kGlitchModeNames, static_cast<int>(controls.glitchMode))); return; }
    if (std::strcmp(key, "glitch_clock") == 0) { controls.glitchClock = static_cast<Engine::CutClock>(parseEnum(value, kCutClockNames, static_cast<int>(controls.glitchClock))); return; }
    if (std::strcmp(key, "glitch_division") == 0) { controls.glitchDivision = static_cast<Engine::CutDivision>(parseEnum(value, kCutDivisionNames, static_cast<int>(controls.glitchDivision))); return; }
    if (std::strcmp(key, "glitch_rate") == 0) { controls.glitchRateHz = clampValue(parseFloat(value, controls.glitchRateHz), 0.25f, 64.0f); return; }
    if (std::strcmp(key, "glitch_slice") == 0) { controls.glitchSliceMs = clampValue(parseFloat(value, controls.glitchSliceMs), 2.0f, 160.0f); return; }
    if (std::strcmp(key, "glitch_repeats") == 0) { controls.glitchRepeats = clampValue(parseFloat(value, controls.glitchRepeats), 1.0f, 16.0f); return; }
    if (std::strcmp(key, "glitch_chance") == 0) { controls.glitchChance = clampValue(parseFloat(value, controls.glitchChance * 100.0f) / 100.0f, 0.0f, 1.0f); return; }
    if (std::strcmp(key, "glitch_mix") == 0) { controls.glitchMix = clampValue(parseFloat(value, controls.glitchMix * 100.0f) / 100.0f, 0.0f, 1.0f); return; }
    if (std::strcmp(key, "glitch_steps") == 0) { controls.glitchSteps = clampValue(parseFloat(value, controls.glitchSteps), 1.0f, 16.0f); return; }
    if (std::strcmp(key, "glitch_pulses") == 0) { controls.glitchPulses = clampValue(parseFloat(value, controls.glitchPulses), 0.0f, 16.0f); return; }
    if (std::strcmp(key, "glitch_rotate") == 0) { controls.glitchRotate = clampValue(parseFloat(value, controls.glitchRotate), 0.0f, 15.0f); return; }
    if (std::strcmp(key, "glitch_jump") == 0) { controls.glitchJumpMs = clampValue(parseFloat(value, controls.glitchJumpMs), 0.0f, 160.0f); return; }
    if (std::strcmp(key, "glitch_pitch") == 0) { controls.glitchPitch = clampValue(parseFloat(value, controls.glitchPitch), 0.0f, 24.0f); return; }
    if (std::strcmp(key, "glitch_reverse") == 0) { controls.glitchReverseChance = clampValue(parseFloat(value, controls.glitchReverseChance * 100.0f) / 100.0f, 0.0f, 1.0f); return; }
    if (std::strcmp(key, "glitch_decay") == 0) { controls.glitchDecay = clampValue(parseFloat(value, controls.glitchDecay * 100.0f) / 100.0f, 0.0f, 1.0f); return; }
    if (std::strcmp(key, "glitch_seed") == 0) { controls.glitchSeed = clampValue(parseFloat(value, controls.glitchSeed), 1.0f, 9999.0f); return; }
    if (std::strcmp(key, "effect_tone") == 0) { controls.effectTone = clampValue(parseFloat(value, controls.effectTone), -1.0f, 1.0f); return; }
    if (std::strcmp(key, "effect_drive") == 0) { controls.effectDrive = clampValue(parseFloat(value, controls.effectDrive), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "effect_bits") == 0) { controls.effectBits = clampValue(parseFloat(value, controls.effectBits), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "effect_rate") == 0) { controls.effectRate = clampValue(parseFloat(value, controls.effectRate), 1.0f, 16.0f); return; }
    if (std::strcmp(key, "effect_delay") == 0) { controls.effectDelayMs = clampValue(parseFloat(value, controls.effectDelayMs), 1.0f, 750.0f); return; }
    if (std::strcmp(key, "effect_feedback") == 0) { controls.effectFeedback = clampValue(parseFloat(value, controls.effectFeedback * 100.0f) / 100.0f, 0.0f, 0.9f); return; }
    if (std::strcmp(key, "effect_width") == 0) { controls.effectWidth = clampValue(parseFloat(value, controls.effectWidth * 100.0f) / 100.0f, 0.0f, 1.0f); return; }
    if (std::strcmp(key, "effect_mix") == 0) { controls.effectMix = clampValue(parseFloat(value, controls.effectMix * 100.0f) / 100.0f, 0.0f, 1.0f); return; }
}

int getParam(void* opaque, const char* key, char* buffer, int bufferLength)
{
    auto* instance = static_cast<ThruInstance*>(opaque);
    if (!instance || !key || !buffer || bufferLength < 1) return -1;
    const auto& controls = instance->controls;

    if (std::strcmp(key, "chain_params") == 0) return copyText(kChainParams, buffer, bufferLength);
    if (std::strcmp(key, "ui_hierarchy") == 0) return copyText(kUiHierarchy, buffer, bufferLength);
    if (std::strcmp(key, "preset_name") == 0) { buffer[0] = '\0'; return 0; }
    if (std::strcmp(key, "name") == 0) return std::snprintf(buffer, bufferLength, "s3g Thru");
    if (std::strcmp(key, "polyphony") == 0) return std::snprintf(
        buffer, bufferLength, "%u", static_cast<unsigned>(kVoiceCount));
    if (std::strcmp(key, "surface") == 0) { buffer[0] = '\0'; return 0; }
    if (std::strcmp(key, "midi_map") == 0) return copyText(kMidiMap, buffer, bufferLength);
    if (std::strcmp(key, "waveforms") == 0) {
        instance->serviceWaveScan();
        if (instance->waveTextDirty) instance->rebuildWaveText();
        return copyText(instance->waveText.data(), buffer, bufferLength);
    }
    if (std::strcmp(key, "region_state") == 0) return instance->formatRegionState(buffer, bufferLength);
    if (std::strcmp(key, "cue_state") == 0) return instance->formatCueState(buffer, bufferLength);
    if (std::strcmp(key, "playheads") == 0) {
        return instance->formatVoicePositions(buffer, bufferLength, false);
    }
    if (std::strcmp(key, "voice_lanes") == 0) {
        return instance->formatVoicePositions(buffer, bufferLength, true);
    }
    if (std::strcmp(key, "voice_pads") == 0) {
        return instance->formatVoicePads(buffer, bufferLength);
    }
    if (std::strcmp(key, "recording") == 0 || std::strcmp(key, "recording:effective") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->engine.recording() ? 1 : 0);
    if (std::strcmp(key, "ui_state") == 0 || std::strcmp(key, "ui_state:effective") == 0) {
        const PlaybackVoice* display = instance->displayVoice();
        const int position = display
            ? static_cast<int>(std::lrintf(display->engine.normalizedPosition() * 1000.0f)) : 0;
        const int fieldStart = static_cast<int>(std::lrintf(
            (display ? display->engine.normalizedFieldStart()
                     : instance->engine.normalizedFieldStart()) * 1000.0f));
        const int fieldEnd = static_cast<int>(std::lrintf(
            (display ? display->engine.normalizedFieldEnd()
                     : instance->engine.normalizedFieldEnd()) * 1000.0f));
        const int meter = static_cast<int>(std::lrintf(clampValue(instance->engine.recordMeterPeak(), 0.0f, 1.0f) * 1000.0f));
        const int limited = instance->engine.recording()
            ? (instance->engine.recordLimited() ? 1 : 0)
            : (instance->engine.lastRecordLimited() ? 1 : 0);
        const std::size_t laneFrames = instance->engine.laneFrames(instance->selectedLane);
        const int tenths = static_cast<int>(std::llround(
            static_cast<double>(laneFrames) * 10.0 / instance->sampleRate));
        return std::snprintf(buffer, bufferLength,
            "%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            static_cast<unsigned>(instance->selectedLane + 1u),
            static_cast<unsigned>(instance->engine.loadedLaneMask()),
            instance->engine.recording() ? 1 : 0,
            meter, position, fieldStart, fieldEnd,
            display ? static_cast<int>(std::lrintf(display->engine.laneWeight(0u) * 1000.0f)) : 0,
            display ? static_cast<int>(std::lrintf(display->engine.laneWeight(1u) * 1000.0f)) : 0,
            display ? static_cast<int>(std::lrintf(display->engine.laneWeight(2u) * 1000.0f)) : 0,
            display ? static_cast<int>(std::lrintf(display->engine.laneWeight(3u) * 1000.0f)) : 0,
            limited, tenths,
            instance->scanLane >= 0 || instance->waveDirtyMask != 0u ? 1 : 0,
            instance->persistenceEnabled ? 1 : 0,
            instance->persistenceRestored ? 1 : 0,
            instance->heldVoiceCount());
    }
    if (std::strcmp(key, "state") == 0) {
        return instance->serializeState(buffer, bufferLength);
    }
    if (std::strcmp(key, "persistence") == 0) return std::snprintf(
        buffer, bufferLength, "%d,%d", instance->persistenceEnabled ? 1 : 0,
        instance->persistenceRestored ? 1 : 0);
    if (std::strcmp(key, "performance_state") == 0) return std::snprintf(
        buffer, bufferLength, "%llu,%llu",
        static_cast<unsigned long long>(instance->maximumRenderMicros.load(
            std::memory_order_relaxed)),
        static_cast<unsigned long long>(instance->renderOverruns.load(
            std::memory_order_relaxed)));
    if (std::strcmp(key, "memory_state") == 0) return std::snprintf(
        buffer, bufferLength, "%zu,%zu,%zu",
        instance->planeCapacityFrames * kChannelCount * sizeof(float),
        instance->planeCapacityFrames * kChannelCount * sizeof(float),
        instance->planeCapacityFrames * sizeof(float));
    if (std::strcmp(key, "chain_state") == 0) return std::snprintf(
        buffer, bufferLength, "%d,%d,%d,%d,%d",
        instance->chainEnabled ? 1 : 0, instance->chainPosition,
        instance->chainRepeat, instance->chainEditSlot,
        instance->chainNextSlot);
    if (std::strcmp(key, "chain_entries") == 0) {
        return instance->formatClipChain(buffer, bufferLength);
    }
    if (std::strcmp(key, "undo_available") == 0) return std::snprintf(
        buffer, bufferLength, "%d", instance->undoAvailable ? 1 : 0);
    if (std::strcmp(key, "clip_bank_state") == 0) {
        const std::uint64_t mask = instance->occupiedClipMask();
        return std::snprintf(buffer, bufferLength, "%d,%d,%u,%u",
            instance->activeClip, instance->queuedClip,
            static_cast<unsigned>(mask & 0xffffffffu),
            static_cast<unsigned>(mask >> 32u));
    }
    if (std::strcmp(key, "sample_load_state") == 0) {
        const int loadState = instance->sampleLoadState.load(
            std::memory_order_acquire);
        int progress = 0;
        if (loadState == static_cast<int>(SampleLoadState::complete)) {
            progress = 1000;
        } else if (loadState == static_cast<int>(SampleLoadState::copying)
                   && instance->sampleStageFrames != 0u) {
            progress = static_cast<int>(std::min<std::size_t>(1000u,
                instance->sampleCopyFrame * 1000u / instance->sampleStageFrames));
        }
        return std::snprintf(buffer, bufferLength, "%d,%d,%d,%s",
            loadState, instance->sampleRequestLane + 1, progress,
            loadState == static_cast<int>(SampleLoadState::error)
                ? instance->sampleLoadError.data()
                : instance->sampleLoadedName.data());
    }
    if (std::strcmp(key, "clip_prepare_state") == 0) return std::snprintf(
        buffer, bufferLength, "%d,%d",
        instance->clipPrepareState.load(std::memory_order_acquire),
        instance->clipPrepareRequest.load(std::memory_order_relaxed));
    if (std::strcmp(key, "lane_source") == 0) return copyText(
        instance->laneSourceLabels[instance->selectedLane].data(),
        buffer, bufferLength);
    if (std::strcmp(key, "last_error") == 0) return copyText(
        instance->lastError.data(), buffer, bufferLength);
    if (std::strcmp(key, "lane_source_state") == 0) {
        const char* kinds[] { "FILE", "CAPTURE", "EMPTY" };
        const int loadState = instance->sampleLoadState.load(
            std::memory_order_acquire);
        const bool selectedLoad = instance->sampleRequestLane
            == static_cast<int>(instance->selectedLane);
        const bool loading = selectedLoad
            && loadState >= static_cast<int>(SampleLoadState::requested)
            && loadState <= static_cast<int>(SampleLoadState::copying);
        const bool missing = selectedLoad
            && loadState == static_cast<int>(SampleLoadState::error);
        LaneSourceKind ownership = instance->currentLaneSourceKinds[
            instance->selectedLane];
        if (instance->activeClip >= 0
            && instance->activeClip < static_cast<int>(kClipSlotCount)) {
            ownership = instance->clipSlots[static_cast<std::size_t>(
                instance->activeClip)].laneSourceKinds[
                    instance->selectedLane];
        }
        const auto kind = static_cast<std::size_t>(ownership);
        return std::snprintf(buffer, bufferLength, "%s:%s",
            loading ? "LOADING" : (missing ? "MISSING"
                : (kind < 3u ? kinds[kind] : "MISSING")),
            instance->laneSourceLabels[instance->selectedLane].data());
    }

    if (std::strcmp(key, "locus") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.locus);
    if (std::strcmp(key, "field") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.field);
    if (std::strcmp(key, "speed") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.speed);
    if (std::strcmp(key, "manual_lane") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.manualLane + 1.0f);
    if (std::strcmp(key, "lane_cycles") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.laneCycles);
    if (std::strcmp(key, "stack_scan_paused") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->stackScanPaused ? 1 : 0);
    if (std::strcmp(key, "algorithm") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.algorithm));
    if (std::strcmp(key, "sample_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.sampleMode));
    if (std::strcmp(key, "slice_region_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.sampleRegionMode));
    if (std::strcmp(key, "slice_regions") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.sampleRegionCount);
    if (std::strcmp(key, "pad_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->padMode));
    if (std::strcmp(key, "lane_variation") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->laneVariation));
    if (std::strcmp(key, "preview_level") == 0) return std::snprintf(buffer, bufferLength, "%.4f", instance->previewLevel);
    if (std::strcmp(key, "clip_rate") == 0) { const ClipSlot* clip = instance->activeClipSlot(); return std::snprintf(buffer, bufferLength, "%.4f", clip ? clip->clipRate : 1.0f); }
    if (std::strcmp(key, "clip_record_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->clipRecordMode));
    if (std::strcmp(key, "clip_timing") == 0) { const ClipSlot* clip = instance->activeClipSlot(); return std::snprintf(buffer, bufferLength, "%d", clip ? static_cast<int>(clip->timingMode) : 0); }
    if (std::strcmp(key, "launch_quantize") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->launchQuantize));
    if (std::strcmp(key, "clip_state") == 0) { const ClipSlot* clip = instance->activeClipSlot(); const std::uint32_t duration = clip ? clip->clipDurationMs : 0u; const int position = duration != 0u ? static_cast<int>(std::lround(clampValue(instance->clipPositionMs / static_cast<double>(duration), 0.0, 1.0) * 1000.0)) : 0; return std::snprintf(buffer, bufferLength, "%d,%d,%d,%d,%u,%u,%u,%d,%d,%d,%d,%d", instance->clipRecording ? 1 : 0, instance->clipPlaying ? 1 : 0, instance->clipReplacing ? 0 : 1, position, duration, clip ? static_cast<unsigned>(clip->clipNoteCount) : 0u, clip ? static_cast<unsigned>(clip->clipPointCount) : 0u, clip ? static_cast<int>(std::lround(clip->clipRate * 1000.0f)) : 1000, instance->clipLaunchPending ? 1 : 0, clip ? static_cast<int>(clip->timingMode) : 0, clip ? static_cast<int>(std::lround(clip->recordedTempoBpm * 100.0f)) : 12000, clip ? static_cast<int>(std::lround(clip->recordedBeats * 1000.0f)) : 0); }
    if (std::strcmp(key, "clip_take_state") == 0) { const ClipSlot* clip = instance->activeClipSlot(); return std::snprintf(buffer, bufferLength, "%u,%d,%u", clip ? static_cast<unsigned>(clip->takeCount) : 0u, clip && clip->takeCount > 1u && !instance->clipRecording ? 1 : 0, instance->clipRecording ? static_cast<unsigned>(instance->clipRecordingTake + 1u) : 0u); }
    if (std::strcmp(key, "clip_capacity_state") == 0) { const ClipSlot* clip = instance->activeClipSlot(); return std::snprintf(buffer, bufferLength, "%u,%u,%u,%u,%d,%d", clip ? static_cast<unsigned>(clip->clipNoteCount) : 0u, static_cast<unsigned>(kClipNoteCapacity), clip ? static_cast<unsigned>(clip->clipPointCount) : 0u, static_cast<unsigned>(kClipPointCapacity), clip && clip->noteCapacityReached ? 1 : 0, clip && clip->pointCapacityReached ? 1 : 0); }
    if (std::strcmp(key, "join") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.join);
    if (std::strcmp(key, "output_gain") == 0) return std::snprintf(buffer, bufferLength, "%.4f", instance->outputGain);
    if (std::strcmp(key, "pressure_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->pressureMode);
    if (std::strcmp(key, "root_note") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->rootNote);
    if (std::strcmp(key, "pad_octave") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->padOctave);
    if (std::strcmp(key, "performance_lane") == 0) return std::snprintf(buffer, bufferLength, "%.4f", instance->performanceLane + 1.0f);
    if (std::strcmp(key, "active_note") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->activeNote);
    if (std::strcmp(key, "record_lane") == 0) return std::snprintf(buffer, bufferLength, "%u", static_cast<unsigned>(instance->selectedLane + 1u));
    if (std::strcmp(key, "record_source") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->recordSource);
    if (std::strcmp(key, "record_gain") == 0) return std::snprintf(buffer, bufferLength, "%.4f", instance->recordGain);
    if (std::strcmp(key, "monitor_level") == 0) return std::snprintf(buffer, bufferLength, "%.4f", instance->monitorLevel);
    if (std::strcmp(key, "record_gate") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->engine.recording() ? 1 : 0);
    if (std::strcmp(key, "clear_lane") == 0) return std::snprintf(buffer, bufferLength, "0");
    if (std::strcmp(key, "sample_trim_start") == 0) return std::snprintf(buffer, bufferLength, "%.4f", instance->sampleTrimStart);
    if (std::strcmp(key, "sample_trim_end") == 0) return std::snprintf(buffer, bufferLength, "%.4f", instance->sampleTrimEnd);
    if (std::strcmp(key, "sample_reverse") == 0 || std::strcmp(key, "sample_normalize") == 0 || std::strcmp(key, "sample_fade") == 0 || std::strcmp(key, "sample_trim_apply") == 0) return std::snprintf(buffer, bufferLength, "0");
    if (std::strcmp(key, "clip_playing") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->clipPlaying ? 1 : 0);
    if (std::strcmp(key, "tempo_bpm") == 0) return std::snprintf(buffer, bufferLength, "%.2f", instance->tempoBpm);
    if (std::strcmp(key, "tempo_swing") == 0) return std::snprintf(buffer, bufferLength, "%.2f", instance->tempoSwing * 100.0f);
    if (std::strcmp(key, "chain_enabled") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->chainEnabled ? 1 : 0);
    if (std::strcmp(key, "chain_slot") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->chainEditSlot + 1);
    if (std::strcmp(key, "chain_clip") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->clipChain[static_cast<std::size_t>(instance->chainEditSlot)].clip) + 1);
    if (std::strcmp(key, "chain_repeats") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->clipChain[static_cast<std::size_t>(instance->chainEditSlot)].repeats));

    if (std::strcmp(key, "motion_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.motion));
    if (std::strcmp(key, "motion_sound") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.motionSound));
    if (std::strcmp(key, "motion_rate") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motionSoundRateHz);
    if (std::strcmp(key, "motion_rate_basis") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.motionRateBasis));
    if (std::strcmp(key, "motion_path_rate") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motionPathRate);
    if (std::strcmp(key, "motion_step") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motionStep);
    if (std::strcmp(key, "motion_seed") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.motionSeed);
    if (std::strcmp(key, "motion_travel") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motionTravel);
    if (std::strcmp(key, "motion_jitter") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motionJitter);
    if (std::strcmp(key, "packet_duty") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.packetDuty * 100.0f);
    if (std::strcmp(key, "motor_rate") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motorRateHz);
    if (std::strcmp(key, "motor_symmetry") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.motorSymmetry * 100.0f);
    if (std::strcmp(key, "motor_shape") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.motorShape));
    if (std::strcmp(key, "segment_trigger") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.segmentTrigger));
    if (std::strcmp(key, "segment_overlap") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.segmentOverlap));
    if (std::strcmp(key, "segment_layers") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.segmentLayerLimit);
    if (std::strcmp(key, "segment_rate") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.segmentRateHz);
    if (std::strcmp(key, "segment_repeats") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.segmentRepeats);
    if (std::strcmp(key, "segment_step") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.segmentStep);
    if (std::strcmp(key, "segment_pitch") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.segmentPitchScatter);
    if (std::strcmp(key, "segment_level") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.segmentLevelVariation);
    if (std::strcmp(key, "segment_curve") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.segmentIntervalCurve);
    if (std::strcmp(key, "lanes_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.lanes));
    if (std::strcmp(key, "lane_path") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.lanePath));
    if (std::strcmp(key, "lane_blend") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.laneBlend));
    if (std::strcmp(key, "lane_path_shape") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.lanePathShape));
    if (std::strcmp(key, "lane_path_offset") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.lanePathOffset);
    if (std::strcmp(key, "lane_path_skew") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.lanePathSkew);
    if (std::strcmp(key, "lane_path_curve") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.lanePathCurve);
    if (std::strcmp(key, "lane_slew") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.laneSlewMs);
    if (std::strcmp(key, "lanes_stretch") == 0) return std::snprintf(buffer, bufferLength, "%d", controls.lanesStretch ? 1 : 0);
    if (std::strcmp(key, "stretch_ratio") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.lanesStretchRatio);
    if (std::strncmp(key, "lane_", 5u) == 0 && std::strlen(key) == 11u && std::strcmp(key + 6, "_gain") == 0) {
        const int lane = key[5] - '1';
        if (lane >= 0 && lane < 4) return std::snprintf(buffer, bufferLength, "%.4f", controls.laneGains[lane]);
    }
    if (std::strcmp(key, "grain_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.grains));
    if (std::strcmp(key, "grain_size") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainSizeMs);
    if (std::strcmp(key, "grain_density") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainDensityHz);
    if (std::strcmp(key, "grain_quality") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->grainVoiceLimit == 4u ? 0 : (instance->grainVoiceLimit == 8u ? 1 : 2));
    if (std::strcmp(key, "grain_pitch") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainPitch);
    if (std::strcmp(key, "grain_window") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.grainWindow));
    if (std::strcmp(key, "grain_spread") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainStereoSpread);
    if (std::strcmp(key, "grain_spray") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainSpray);
    if (std::strcmp(key, "grain_bias") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.grainBias));
    if (std::strcmp(key, "grain_pitch_spray") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainPitchSpray);
    if (std::strcmp(key, "grain_reverse") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.grainReverseChance * 100.0f);
    if (std::strcmp(key, "grain_size_variation") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainSizeVariation);
    if (std::strcmp(key, "grain_level_variation") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainLevelVariation);
    if (std::strcmp(key, "grain_timing") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.grainTiming));
    if (std::strcmp(key, "grain_timing_scatter") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainTimingScatter);
    if (std::strcmp(key, "grain_envelope_skew") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainEnvelopeSkew);
    if (std::strcmp(key, "grain_process") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.grainProcess));
    if (std::strcmp(key, "grain_process_amount") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.grainProcessAmount);
    if (std::strcmp(key, "grain_regions") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.grainRegionCount);
    if (std::strcmp(key, "grain_advance") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.grainAdvance));
    if (std::strcmp(key, "cutups_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.cutups));
    if (std::strcmp(key, "cut_clock") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.cutClock));
    if (std::strcmp(key, "cut_division") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.cutDivision));
    if (std::strcmp(key, "cut_rate") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.cutRateHz);
    if (std::strcmp(key, "cut_time_variation") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.cutTimeVariation);
    if (std::strcmp(key, "cut_gate") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.cutGate * 100.0f);
    if (std::strcmp(key, "cut_join") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.cutJoinMs);
    if (std::strcmp(key, "cut_regions") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.cutRegionCount);
    if (std::strcmp(key, "cut_repeats") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.cutRepeats);
    if (std::strcmp(key, "cut_reverse") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.cutReverseChance * 100.0f);
    if (std::strcmp(key, "cut_pitch_variation") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.cutPitchVariation);
    if (std::strcmp(key, "cut_level_variation") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.cutLevelVariation);
    if (std::strcmp(key, "cut_lane_order") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.cutLaneOrder));
    if (std::strcmp(key, "cut_poly_path") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.cutPolyPath));
    if (std::strcmp(key, "cut_region_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.cutRegionMode));
    if (std::strcmp(key, "glitch_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.glitchMode));
    if (std::strcmp(key, "glitch_clock") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.glitchClock));
    if (std::strcmp(key, "glitch_division") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.glitchDivision));
    if (std::strcmp(key, "glitch_rate") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.glitchRateHz);
    if (std::strcmp(key, "glitch_slice") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.glitchSliceMs);
    if (std::strcmp(key, "glitch_repeats") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.glitchRepeats);
    if (std::strcmp(key, "glitch_chance") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.glitchChance * 100.0f);
    if (std::strcmp(key, "glitch_mix") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.glitchMix * 100.0f);
    if (std::strcmp(key, "glitch_steps") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.glitchSteps);
    if (std::strcmp(key, "glitch_pulses") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.glitchPulses);
    if (std::strcmp(key, "glitch_rotate") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.glitchRotate);
    if (std::strcmp(key, "glitch_jump") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.glitchJumpMs);
    if (std::strcmp(key, "glitch_pitch") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.glitchPitch);
    if (std::strcmp(key, "glitch_reverse") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.glitchReverseChance * 100.0f);
    if (std::strcmp(key, "glitch_decay") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.glitchDecay * 100.0f);
    if (std::strcmp(key, "glitch_seed") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.glitchSeed);
    if (std::strcmp(key, "effect_tone") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.effectTone);
    if (std::strcmp(key, "effect_drive") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.effectDrive);
    if (std::strcmp(key, "effect_bits") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.effectBits);
    if (std::strcmp(key, "effect_rate") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.effectRate);
    if (std::strcmp(key, "effect_delay") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.effectDelayMs);
    if (std::strcmp(key, "effect_feedback") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.effectFeedback * 100.0f);
    if (std::strcmp(key, "effect_width") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.effectWidth * 100.0f);
    if (std::strcmp(key, "effect_mix") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.effectMix * 100.0f);
    return -1;
}

int getError(void* opaque, char* buffer, int bufferLength)
{
    const auto* instance = static_cast<const ThruInstance*>(opaque);
    if (!instance) return -1;
    return copyText(instance->lastError.data(), buffer, bufferLength);
}

void renderBlock(void* opaque, int16_t* output, int frames)
{
    auto* instance = static_cast<ThruInstance*>(opaque);
    if (!output || frames <= 0) return;
    if (!instance) {
        std::memset(output, 0, static_cast<std::size_t>(frames) * 2u * sizeof(int16_t));
        return;
    }
    timespec before {};
    timespec after {};
    clock_gettime(CLOCK_MONOTONIC, &before);
    instance->render(output, frames);
    clock_gettime(CLOCK_MONOTONIC, &after);
    const std::uint64_t micros = static_cast<std::uint64_t>(
        (after.tv_sec - before.tv_sec) * 1000000ll
        + (after.tv_nsec - before.tv_nsec) / 1000ll);
    std::uint64_t maximum = instance->maximumRenderMicros.load(
        std::memory_order_relaxed);
    while (micros > maximum
           && !instance->maximumRenderMicros.compare_exchange_weak(
               maximum, micros, std::memory_order_relaxed)) {}
    const double deadline = static_cast<double>(frames) * 1000000.0
        / std::max(1.0f, instance->sampleRate);
    if (static_cast<double>(micros) > deadline) {
        instance->renderOverruns.fetch_add(1u, std::memory_order_relaxed);
    }
}

plugin_api_v2_t g_pluginApi {
    MOVE_PLUGIN_API_VERSION_2,
    createInstance,
    destroyInstance,
    onMidi,
    setParam,
    getParam,
    getError,
    renderBlock,
};

} // namespace

extern "C" S3G_THRU_EXPORT plugin_api_v2_t*
move_plugin_init_v2(const host_api_v1_t* host)
{
    g_host = host;
    return &g_pluginApi;
}

