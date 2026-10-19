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
constexpr std::size_t kWaveScanBudget = 4096u;
constexpr std::size_t kVoiceCount = 8u;
constexpr std::size_t kVisibleStepCount = 16u;
constexpr std::size_t kSequencePageCount = 4u;
constexpr std::size_t kSequenceStepCount =
    kVisibleStepCount * kSequencePageCount;
constexpr std::size_t kSequenceChordSize = kVoiceCount;
constexpr std::size_t kPatternBankCount = 4u;
constexpr std::size_t kPatternsPerBank = 16u;
constexpr std::size_t kPatternCount = kPatternBankCount * kPatternsPerBank;
constexpr std::size_t kPatternChainLength = 16u;
constexpr std::size_t kCueCount = 8u;
constexpr int kFirstPadNote = 68;
constexpr int kLastPadNote = 99;
constexpr int kPadColumns = 8;
constexpr int kPadRowInterval = 5;
constexpr int kPadBaseNote = 48;
constexpr std::size_t kPersistencePathSize = 768u;
constexpr std::size_t kStateTextSize = 8192u;
constexpr std::size_t kWavIoFrames = 512u;
constexpr std::size_t kLibraryCopyFrames = 4096u;
constexpr const char* kSampleLibraryRoot = "/data/UserData/UserLibrary/Samples/";

enum class LaneSourceKind : std::uint8_t {
    inherit = 0u,
    file,
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
    count
};

constexpr std::size_t kLockParameterCount =
    static_cast<std::size_t>(LockParameter::count);
static_assert(kLockParameterCount == 68u,
              "parameter lock persistence supports three 32-bit words");
constexpr std::size_t kLegacyLockParameterCount = 26u;
constexpr std::size_t kPreviousLockParameterCount = 64u;

struct SequenceStep {
    std::array<float, kLockParameterCount> locks {};
    std::array<std::uint8_t, kSequenceChordSize> notes {};
    std::array<std::uint8_t, kSequenceChordSize> velocities {};
    std::array<std::uint8_t, kSequenceChordSize> sourcePads {
        255u, 255u, 255u, 255u, 255u, 255u, 255u, 255u
    };
    std::array<std::uint8_t, kSequenceChordSize> lengthSteps {
        1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u
    };
    std::array<std::uint8_t, kSequenceChordSize> noteOpen {};
    std::uint32_t lockMask = 0u;
    std::uint32_t lockMaskHigh = 0u;
    std::uint32_t lockMaskExtra = 0u;
    float gate = 0.8f;
    std::uint8_t noteCount = 0u;
    std::uint8_t probability = 100u;
    std::uint8_t ratchets = 1u;
    std::int8_t microTiming = 0;
    bool enabled = false;
    bool gateLocked = false;
};

bool hasStepLock(const SequenceStep& step, std::size_t index)
{
    if (index < 32u) {
        return (step.lockMask & (1u << static_cast<unsigned>(index))) != 0u;
    }
    if (index < 64u) {
        return (step.lockMaskHigh
            & (1u << static_cast<unsigned>(index - 32u))) != 0u;
    }
    return index < 96u && (step.lockMaskExtra
        & (1u << static_cast<unsigned>(index - 64u))) != 0u;
}

void setStepLock(SequenceStep& step, std::size_t index)
{
    if (index < 32u) {
        step.lockMask |= 1u << static_cast<unsigned>(index);
    } else if (index < 64u) {
        step.lockMaskHigh |= 1u << static_cast<unsigned>(index - 32u);
    } else if (index < 96u) {
        step.lockMaskExtra |= 1u << static_cast<unsigned>(index - 64u);
    }
}

bool stepHasLocks(const SequenceStep& step)
{
    return step.lockMask != 0u || step.lockMaskHigh != 0u
        || step.lockMaskExtra != 0u;
}

struct Pattern {
    std::array<SequenceStep, kSequenceStepCount> steps {};
    std::array<float, kLockParameterCount> baseValues {};
    std::array<std::array<char, kPersistencePathSize>, kLaneCount>
        laneSourcePaths {};
    std::array<std::array<char, 64u>, kLaneCount> laneSourceLabels {};
    std::array<LaneSourceKind, kLaneCount> laneSourceKinds {
        LaneSourceKind::inherit, LaneSourceKind::inherit,
        LaneSourceKind::inherit, LaneSourceKind::inherit
    };
    std::array<float, kCueCount> cues {
        -1.0f, -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f
    };
    float swing = 0.5f;
    float legacyGate = 0.8f;
    std::uint8_t length = 16u;
    bool occupied = false;
};

struct PatternChainEntry {
    std::uint8_t pattern = 0u;
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

enum class PatternPrepareState : int {
    idle = 0,
    requested,
    loading,
    ready,
    error,
};

const host_api_v1_t* g_host = nullptr;

constexpr const char* kAlgorithmNames[] {
    "Motion", "Lanes", "Grains", "Cutups", "Sample", "Slices"
};
constexpr const char* kSampleModeNames[] {
    "Forward", "Reverse", "Loop", "Ping Pong"
};
constexpr const char* kRegionModeNames[] { "Equal", "Transients" };
constexpr const char* kPadModeNames[] { "Keys", "Slices", "Cues", "Rolls" };
constexpr const char* kLaneVariationNames[] {
    "Manual", "Cycle", "Random", "No Repeat", "Velocity"
};
constexpr const char* kMotionModeNames[] {
    "Forward", "Reverse", "Hover", "Round Trip", "Moving Loop", "Mirror", "Zigzag", "Drunk"
};
constexpr const char* kMotionSoundNames[] {
    "Continuous", "Packets", "Motor", "Bounce"
};
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
    "Forward", "Reverse", "Palindrome", "Random", "Walk"
};
constexpr const char* kLanePathNames[] {
    "Down", "Up", "Triangle", "Sine", "Steps Down", "Steps Up", "Manual", "Random"
};
constexpr const char* kMotorShapeNames[] { "Triangle", "Rounded", "Pulse" };
constexpr const char* kLaneBlendNames[] { "Crossfade", "Jump" };
constexpr const char* kLanePathShapeNames[] {
    "Linear", "Smooth", "Exponential", "Plateau"
};
constexpr const char* kGrainTimingNames[] { "Regular", "Scatter" };
constexpr const char* kGrainBiasNames[] { "Behind", "Around", "Ahead" };
constexpr const char* kGrainProcessNames[] {
    "Ordinary", "Stutter", "Shrink", "Doublets"
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
    "51 monitor_level,52 record_gate,53 clear_lane,54 seq_bpm,"
    "55 seq_swing,56 seq_length,57 seq_gate,58 seq_page,59 undo,"
    "60 seq_probability,61 seq_ratchets,62 seq_micro,63 automation_record,"
    "64 seq_capture,65 chain_enabled,66 chain_slot,67 chain_pattern,"
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
    "111 pad_mode,112 lane_variation,113 preview_level";

constexpr const char kChainParams[] = R"json([
{"key":"locus","name":"Position","short_name":"POS","type":"float","min":0,"max":1,"step":0.01,"default":0.5},
{"key":"field","name":"Field","short_name":"FIELD","type":"float","min":0.02,"max":1,"step":0.01,"default":1},
{"key":"speed","name":"Speed","short_name":"SPD","type":"float","min":0.05,"max":4,"step":0.01,"default":1},
{"key":"manual_lane","name":"Stack Position","short_name":"STACK","type":"float","min":1,"max":4,"step":0.01,"default":1},
{"key":"lane_cycles","name":"Stack Rate","short_name":"SCAN","type":"float","min":0.125,"max":16,"step":0.125,"default":1,"unit":"Hz"},
{"key":"algorithm","name":"Algorithm","short_name":"ALG","type":"enum","options":["Motion","Lanes","Grains","Cutups","Sample","Slices"],"default":0},
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
{"key":"motion_sound","name":"Motion Sound","short_name":"SOUND","type":"enum","options":["Continuous","Packets","Motor","Bounce"],"default":0},
{"key":"motion_rate","name":"Gesture Rate","short_name":"GEST","type":"float","min":0.25,"max":20,"step":0.25,"default":4,"unit":"Hz"},
{"key":"motion_travel","name":"Path Travel","short_name":"TRAVEL","type":"float","min":0,"max":1,"step":0.01,"default":1},
{"key":"motion_jitter","name":"Path Jitter","short_name":"JITTER","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"packet_duty","name":"Packet Duty","short_name":"DUTY","type":"float","min":2,"max":98,"step":1,"default":65,"unit":"%"},
{"key":"motor_rate","name":"Motor Rate","short_name":"M RATE","type":"float","min":0.05,"max":20,"step":0.05,"default":1,"unit":"Hz"},
{"key":"motor_symmetry","name":"Motor Symmetry","short_name":"SYM","type":"float","min":5,"max":95,"step":1,"default":50,"unit":"%"},
{"key":"motor_shape","name":"Motor Shape","short_name":"SHAPE","type":"enum","options":["Triangle","Rounded","Pulse"],"default":1},
{"key":"lanes_mode","name":"Lanes Mode","short_name":"MODE","type":"enum","options":["Forward","Reverse","Ping Pong"],"default":0},
{"key":"lane_path","name":"Stack Path","short_name":"PATH","type":"enum","options":["Down","Up","Triangle","Sine","Steps Down","Steps Up","Manual","Random"],"default":2},
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
{"key":"grain_process","name":"Grain Process","short_name":"PROCESS","type":"enum","options":["Ordinary","Stutter","Shrink","Doublets"],"default":0},
{"key":"grain_process_amount","name":"Process Amount","short_name":"AMOUNT","type":"float","min":0,"max":1,"step":0.01,"default":0.5},
{"key":"grain_regions","name":"Slice Regions","short_name":"REGIONS","type":"int","min":1,"max":64,"default":16},
{"key":"grain_advance","name":"Source Advance","short_name":"ADVANCE","type":"enum","options":["Scan","Per Grain"],"default":0},
{"key":"cutups_mode","name":"Cutups Mode","short_name":"MODE","type":"enum","options":["Forward","Reverse","Palindrome","Random","Walk"],"default":0},
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
{"key":"pad_mode","name":"Pad Mode","short_name":"PADS","type":"enum","options":["Keys","Slices","Cues","Rolls"],"default":0},
{"key":"lane_variation","name":"Lane Variation","short_name":"VARIATION","type":"enum","options":["Manual","Cycle","Random","No Repeat","Velocity"],"default":0},
{"key":"preview_level","name":"Browser Preview","short_name":"PREVIEW","type":"float","min":0,"max":1,"step":0.01,"default":0.7},
{"key":"seq_bpm","name":"Pattern Tempo","short_name":"BPM","type":"float","min":40,"max":240,"step":1,"default":120,"unit":"BPM"},
{"key":"seq_swing","name":"Pattern Swing","short_name":"SWING","type":"float","min":50,"max":75,"step":1,"default":50,"unit":"%"},
{"key":"seq_length","name":"Pattern Length","short_name":"LEN","type":"int","min":1,"max":64,"default":16},
{"key":"seq_gate","name":"Pattern Gate","short_name":"GATE","type":"float","min":5,"max":800,"step":5,"default":80,"unit":"%"},
{"key":"seq_page","name":"Step Page","short_name":"PAGE","type":"int","min":1,"max":4,"default":1},
{"key":"seq_probability","name":"Step Probability","short_name":"PROB","type":"int","min":0,"max":100,"default":100,"unit":"%"},
{"key":"seq_ratchets","name":"Step Ratchets","short_name":"RATCH","type":"int","min":1,"max":4,"default":1},
{"key":"seq_micro","name":"Step Microtiming","short_name":"MICRO","type":"int","min":-49,"max":49,"default":0},
{"key":"automation_record","name":"Automation Record","short_name":"AUTO","type":"enum","options":["Off","On"],"default":0},
{"key":"chain_enabled","name":"Pattern Chain","short_name":"CHAIN","type":"enum","options":["Off","On"],"default":0},
{"key":"chain_slot","name":"Chain Slot","short_name":"SLOT","type":"int","min":1,"max":16,"default":1},
{"key":"chain_pattern","name":"Chain Pattern","short_name":"PATT","type":"int","min":1,"max":64,"default":1},
{"key":"chain_repeats","name":"Chain Repeats","short_name":"REPT","type":"int","min":1,"max":16,"default":1},
{"key":"seq_state","name":"Sequencer State","type":"string","access":"read","live":true},
{"key":"seq_locks","name":"Step Lock Masks","type":"string","access":"read","live":true},
{"key":"seq_locks_hi","name":"Step Lock Masks High","type":"string","access":"read","live":true},
{"key":"seq_locks_x","name":"Step Lock Masks Extra","type":"string","access":"read","live":true},
{"key":"seq_note_counts","name":"Step Note Counts","type":"string","access":"read","live":true},
{"key":"seq_pad_masks","name":"Step Pad Masks","type":"string","access":"read","live":true},
{"key":"seq_open_mask","name":"Open Note Pads","type":"string","access":"read","live":true},
{"key":"seq_open_steps","name":"Open Note Start Steps","type":"string","access":"read","live":true},
{"key":"seq_clear_step","name":"Clear Sequence Step","type":"int","min":0,"max":63,"access":"write"},
{"key":"chain_state","name":"Pattern Chain State","type":"string","access":"read","live":true},
{"key":"chain_entries","name":"Pattern Chain Entries","type":"string","access":"read","live":true},
{"key":"undo_available","name":"Undo Available","type":"int","min":0,"max":1,"access":"read","live":true},
{"key":"pattern_state","name":"Pattern Bank State","type":"string","access":"read","live":true},
{"key":"sample_load_state","name":"Sample Load State","type":"string","access":"read","live":true},
{"key":"region_state","name":"Transient Regions","type":"string","access":"read","live":true},
{"key":"cue_state","name":"Pattern Cues","type":"string","access":"read","live":true},
{"key":"preview_file","name":"Preview Sample","type":"string","access":"write"},
{"key":"preview_stop","name":"Stop Preview","type":"int","min":0,"max":1,"access":"write"},
{"key":"cue_store","name":"Store Cue","type":"string","access":"write"},
{"key":"cue_trigger","name":"Trigger Cue","type":"int","min":0,"max":7,"access":"write"},
{"key":"cue_clear","name":"Clear Cue","type":"int","min":0,"max":7,"access":"write"},
{"key":"pattern_prepare_state","name":"Pattern Prepare State","type":"string","access":"read","live":true},
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
"root":{"name":"S3G Thru","knobs":["algorithm","locus","field","speed","manual_lane","lane_cycles","join","output_gain"],"params":[{"level":"record","label":"Record"},{"level":"motion","label":"Motion"},{"level":"lanes","label":"Lanes"},{"level":"grains","label":"Grains"},{"level":"cutups","label":"Cutups"},{"level":"sample","label":"Sample"},{"level":"slices","label":"Slices"},{"level":"lane_mix","label":"Lane Mix"},{"level":"sequence","label":"Sequence"}]},
"record":{"name":"Record","knobs":["record_lane","record_source","record_gain","monitor_level","pressure_mode","root_note","output_gain","clear_lane"],"params":[{"key":"record_lane"},{"key":"record_source"},{"key":"record_gain"},{"key":"monitor_level"},{"key":"pressure_mode"},{"key":"root_note"},{"key":"output_gain"},{"key":"clear_lane"}]},
"motion":{"name":"Motion","knobs":["locus","field","speed","join","motion_mode","motion_sound","motion_rate","output_gain"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"motion_mode"},{"key":"motion_sound"},{"key":"motion_rate"},{"key":"output_gain"}]},
"lanes":{"name":"Lanes","knobs":["locus","field","speed","join","lanes_mode","lane_path","lane_cycles","manual_lane"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"lanes_mode"},{"key":"lane_path"},{"key":"lane_cycles"},{"key":"manual_lane"},{"key":"lanes_stretch"},{"key":"stretch_ratio"}]},
"grains":{"name":"Grains","knobs":["locus","field","speed","join","grain_mode","grain_size","grain_density","grain_pitch"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"grain_mode"},{"key":"grain_size"},{"key":"grain_density"},{"key":"grain_pitch"},{"key":"grain_window"},{"key":"grain_spread"},{"key":"lane_path"},{"key":"lane_cycles"},{"key":"manual_lane"}]},
"cutups":{"name":"Cutups","knobs":["locus","field","speed","join","cutups_mode","lane_path","lane_cycles","manual_lane"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"cutups_mode"},{"key":"lane_path"},{"key":"lane_cycles"},{"key":"manual_lane"}]},
"sample":{"name":"Sample","knobs":["sample_mode","locus","field","speed","manual_lane","lane_variation","pad_mode","output_gain"],"params":[{"key":"sample_mode"},{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"manual_lane"},{"key":"lane_variation"},{"key":"pad_mode"},{"key":"output_gain"}]},
"slices":{"name":"Slices","knobs":["sample_mode","slice_region_mode","slice_regions","speed","manual_lane","lane_variation","pad_mode","output_gain"],"params":[{"key":"sample_mode"},{"key":"slice_region_mode"},{"key":"slice_regions"},{"key":"speed"},{"key":"manual_lane"},{"key":"lane_variation"},{"key":"pad_mode"},{"key":"output_gain"}]},
"lane_mix":{"name":"Lane Mix","knobs":["lane_1_gain","lane_2_gain","lane_3_gain","lane_4_gain","lanes_stretch","stretch_ratio","pressure_mode","output_gain"],"params":[{"key":"lane_1_gain"},{"key":"lane_2_gain"},{"key":"lane_3_gain"},{"key":"lane_4_gain"},{"key":"lanes_stretch"},{"key":"stretch_ratio"},{"key":"pressure_mode"},{"key":"output_gain"}]},
"sequence":{"name":"Sequence","knobs":["seq_bpm","seq_swing","seq_length","root_note","algorithm","manual_lane","join","output_gain"],"params":[{"key":"seq_bpm"},{"key":"seq_swing"},{"key":"seq_length"},{"key":"root_note"},{"key":"algorithm"},{"key":"manual_lane"},{"key":"join"},{"key":"output_gain"}]}
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
        "slice_region_mode", "slice_regions"
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
        case LockParameter::motionSound: controls.motionSound = static_cast<Engine::MotionSound>(clampValue(static_cast<int>(std::lround(value)), 0, 3)); break;
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
        case LockParameter::cutupsMode: controls.cutups = static_cast<Engine::CutupsMode>(clampValue(static_cast<int>(std::lround(value)), 0, 4)); break;
        case LockParameter::motionTravel: controls.motionTravel = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::motionJitter: controls.motionJitter = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::packetDuty: controls.packetDuty = clampValue(value / 100.0f, 0.02f, 0.98f); break;
        case LockParameter::motorRate: controls.motorRateHz = clampValue(value, 0.05f, 20.0f); break;
        case LockParameter::motorSymmetry: controls.motorSymmetry = clampValue(value / 100.0f, 0.05f, 0.95f); break;
        case LockParameter::motorShape: controls.motorShape = static_cast<Engine::MotorShape>(clampValue(static_cast<int>(std::lround(value)), 0, 2)); break;
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
        case LockParameter::grainProcess: controls.grainProcess = static_cast<Engine::GrainProcess>(clampValue(static_cast<int>(std::lround(value)), 0, 3)); break;
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

bool writeF32(std::FILE* file, float value)
{
    static_assert(sizeof(float) == sizeof(std::uint32_t),
                  "pattern storage requires 32-bit floats");
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
    char pattern[64] {};
    const int patternLength = std::snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (patternLength <= 0 || patternLength >= static_cast<int>(sizeof(pattern))) return false;
    const char* found = std::strstr(json, pattern);
    if (!found) return false;
    found = std::strchr(found + patternLength, ':');
    if (!found) return false;
    char* end = nullptr;
    const float parsed = std::strtof(found + 1, &end);
    if (end == found + 1 || !std::isfinite(parsed)) return false;
    result = parsed;
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
    int sequenceStepsRemaining = 0;
    bool active = false;
    bool held = false;
    bool sequencerOwned = false;
    bool useSnapshot = false;
};

struct ThruInstance {
    Engine engine;
    std::array<PlaybackVoice, kVoiceCount> voices {};
    Engine::Controls controls;
    std::unique_ptr<float[]> storage;
    std::unique_ptr<float[]> preparedPatternStorage;
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

    std::array<SequenceStep, kSequenceStepCount> sequence {};
    std::array<Pattern, kPatternCount> patterns {};
    std::array<SequenceStep, kSequenceStepCount> performanceCapture {};
    std::array<int, 32u> captureStartSteps {};
    std::array<PatternChainEntry, kPatternChainLength> patternChain {};
    double sequenceSamplesUntilStep = 0.0;
    double ratchetSamplesUntilTrigger = 0.0;
    float sequenceBpm = 120.0f;
    float sequenceSwing = 0.5f;
    float sequenceGate = 0.8f;
    int sequenceLength = 16;
    int sequenceCurrentStep = -1;
    int sequencePage = 0;
    int sequenceEditStep = 0;
    int ratchetStep = -1;
    int ratchetsRemaining = 0;
    bool sequenceRunning = false;
    bool automationRecord = false;
    bool chainEnabled = false;
    int chainPosition = 0;
    int chainRepeat = 0;
    int chainEditSlot = 0;
    int chainNextSlot = -1;
    std::uint32_t sequenceRandom = 0x53334754u;
    int activePattern = 0;
    int queuedPattern = -1;
    bool activePatternOccupied = false;
    bool patternBankInitialized = false;
    Pattern undoPattern {};
    int undoPatternIndex = -1;
    bool undoAvailable = false;
    bool persistentStateDirty = false;
    std::uint32_t pendingCaptureSaveMask = 0u;
    std::uint32_t pendingRegionAnalysisMask = 0u;
    std::array<int, kLaneCount> pendingCapturePatterns { -1, -1, -1, -1 };
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
    std::atomic<int> patternPrepareState {
        static_cast<int>(PatternPrepareState::idle)
    };
    std::atomic<int> patternPrepareRequest { -1 };
    std::atomic<int> preparedPattern { -1 };
    std::array<std::size_t, kLaneCount> preparedLaneFrames {};
    std::array<float, kLaneCount> preparedLaneRates {};
    std::array<LaneSourceKind, kLaneCount> preparedLaneKinds {
        LaneSourceKind::empty, LaneSourceKind::empty,
        LaneSourceKind::empty, LaneSourceKind::empty
    };
    std::array<RegionMap, kLaneCount> activeRegions {};
    std::array<RegionMap, kLaneCount> preparedRegions {};
    RegionMap sampleStageRegions {};
    std::array<std::uint32_t, kLaneCount> laneRegionRevisions {};
    std::array<std::array<char, kPersistencePathSize>, kLaneCount>
        preparedLanePaths {};
    std::array<std::array<char, 64u>, kLaneCount> preparedLaneLabels {};
    std::array<char, kPersistencePathSize> sampleRequestPath {};
    std::array<char, 96u> sampleLoadedName {};
    std::array<char, 128u> sampleLoadError {};
    int sampleRequestLane = 0;
    int sampleRequestPattern = 0;
    bool sampleRequestFromPattern = false;
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
        pendingPatternSourcePaths {};
    std::uint32_t pendingPatternSourceMask = 0u;

    ~ThruInstance() { stopSampleWorker(); }

    std::array<std::array<std::int8_t, kWaveBins>, kLaneCount> waveMinimum {};
    std::array<std::array<std::int8_t, kWaveBins>, kLaneCount> waveMaximum {};
    std::array<float, kWaveBins> scanMinimum {};
    std::array<float, kWaveBins> scanMaximum {};
    std::uint32_t waveDirtyMask = 0u;
    int scanLane = -1;
    std::size_t scanFrame = 0u;
    std::size_t scanFrames = 0u;
    std::array<char, kWaveTextSize> waveText {};
    std::array<char, 160u> lastError {};
    std::array<char, kPersistencePathSize> moduleDirectory {};
    std::uint32_t pendingClearMask = 0u;
    std::uint32_t recordingLaneMask = 0u;
    bool persistenceEnabled = false;
    bool persistenceRestored = false;

    bool prepare(const char* moduleDir)
    {
        const int hostRate = g_host && g_host->sample_rate > 0
            ? g_host->sample_rate : MOVE_SAMPLE_RATE;
        sampleRate = static_cast<float>(hostRate);
        laneCapacityFrames = static_cast<std::size_t>(hostRate) * kLaneSeconds;
        planeCapacityFrames = laneCapacityFrames * Engine::kLaneChannels;
        const std::size_t totalFloats = planeCapacityFrames * kChannelCount;
        storage.reset(new (std::nothrow) float[totalFloats] {});
        preparedPatternStorage.reset(new (std::nothrow) float[totalFloats] {});
        sampleStage.reset(new (std::nothrow) float[planeCapacityFrames] {});
        if (!storage || !preparedPatternStorage || !sampleStage) {
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
        captureStartSteps.fill(-1);
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
        if (!patternBankInitialized) captureActivePattern(false);
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
            g_host->log("S3G Thru sample-library worker unavailable");
        }
        rebuildWaveText();
        return true;
    }

    void setError(const char* message)
    {
        std::snprintf(lastError.data(), lastError.size(), "%s", message ? message : "");
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
        }
        return loaded;
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

    bool preparePatternBank(int patternIndex)
    {
        if (patternIndex < 0
            || patternIndex >= static_cast<int>(patterns.size())) return false;
        preparedLaneFrames.fill(0u);
        preparedLaneRates.fill(sampleRate);
        preparedRegions.fill(RegionMap {});
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            float* left = preparedPatternStorage.get()
                + planeCapacityFrames * lane;
            float* right = left + laneCapacityFrames;
            const char* path = "";
            const char* label = "";
            const LaneSourceKind kind = resolvePatternLaneSource(
                patternIndex, lane, path, label);
            preparedLaneKinds[lane] = kind;
            preparedLanePaths[lane][0] = '\0';
            preparedLaneLabels[lane][0] = '\0';
            if (kind == LaneSourceKind::empty) continue;
            if (kind == LaneSourceKind::inherit || path[0] == '\0') {
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
            std::snprintf(preparedLanePaths[lane].data(),
                          preparedLanePaths[lane].size(), "%s", path);
            if (label[0] != '\0') {
                std::snprintf(preparedLaneLabels[lane].data(),
                              preparedLaneLabels[lane].size(), "%s", label);
            }
        }
        preparedPattern.store(patternIndex, std::memory_order_relaxed);
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
            if (instance->patternPrepareState.load(std::memory_order_acquire)
                == static_cast<int>(PatternPrepareState::requested)) {
                instance->patternPrepareState.store(
                    static_cast<int>(PatternPrepareState::loading),
                    std::memory_order_release);
                const int target = instance->patternPrepareRequest.load(
                    std::memory_order_relaxed);
                const bool prepared = instance->preparePatternBank(target);
                if (prepared
                    && instance->sampleLoadState.load(std::memory_order_acquire)
                        == static_cast<int>(SampleLoadState::error)) {
                    instance->sampleLoadState.store(
                        static_cast<int>(SampleLoadState::complete),
                        std::memory_order_release);
                }
                instance->patternPrepareState.store(static_cast<int>(prepared
                    ? PatternPrepareState::ready : PatternPrepareState::error),
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
                              bool fromPattern = false, int ownerPattern = -1,
                              bool preview = false)
    {
        if (!sampleWorkerReady || lane < 0 || lane >= static_cast<int>(kLaneCount)
            || !path || std::strlen(path) >= sampleRequestPath.size()) {
            setSampleLoadError("Sample loader is unavailable");
            return;
        }
        const auto prepareState = static_cast<PatternPrepareState>(
            patternPrepareState.load(std::memory_order_acquire));
        if (prepareState == PatternPrepareState::requested
            || prepareState == PatternPrepareState::loading) {
            setError("Wait for the queued pattern to finish preparing");
            return;
        }
        const int current = sampleLoadState.load(std::memory_order_acquire);
        if (current == static_cast<int>(SampleLoadState::previewing)
            && !preview
            && std::strcmp(path, sampleRequestPath.data()) == 0) {
            if (!fromPattern) pushPatternUndo(activePattern);
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
        if (!fromPattern && !preview) pushPatternUndo(activePattern);
        pthread_mutex_lock(&sampleWorkerMutex);
        sampleRequestLane = lane;
        sampleRequestPattern = ownerPattern >= 0 ? ownerPattern : activePattern;
        sampleRequestFromPattern = fromPattern;
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

    bool requestPatternPreparation(int patternIndex)
    {
        if (!sampleWorkerReady || engine.recording() || patternIndex < 0
            || patternIndex >= static_cast<int>(patterns.size())) return false;
        const auto sampleState = static_cast<SampleLoadState>(
            sampleLoadState.load(std::memory_order_acquire));
        if (sampleState >= SampleLoadState::requested
            && sampleState <= SampleLoadState::copying) return false;
        const auto prepareState = static_cast<PatternPrepareState>(
            patternPrepareState.load(std::memory_order_acquire));
        if (prepareState == PatternPrepareState::requested
            || prepareState == PatternPrepareState::loading) return false;
        initializePatternIfNeeded(patternIndex);
        pthread_mutex_lock(&sampleWorkerMutex);
        patternPrepareRequest.store(patternIndex, std::memory_order_relaxed);
        preparedPattern.store(-1, std::memory_order_relaxed);
        patternPrepareState.store(
            static_cast<int>(PatternPrepareState::requested),
            std::memory_order_release);
        sampleWorkerSignaled = true;
        pthread_cond_signal(&sampleWorkerCondition);
        pthread_mutex_unlock(&sampleWorkerMutex);
        return true;
    }

    bool patternPreparationInFlight() const
    {
        const auto state = static_cast<PatternPrepareState>(
            patternPrepareState.load(std::memory_order_acquire));
        return state == PatternPrepareState::requested
            || state == PatternPrepareState::loading;
    }

    LaneSourceKind resolvePatternLaneSource(
        int patternIndex, std::size_t lane,
        const char*& path, const char*& label) const
    {
        path = "";
        label = "";
        if (lane >= kLaneCount) return LaneSourceKind::empty;
        for (int index = patternIndex; index >= 0; --index) {
            const Pattern& pattern = patterns[static_cast<std::size_t>(index)];
            const LaneSourceKind kind = pattern.laneSourceKinds[lane];
            if (kind == LaneSourceKind::inherit) continue;
            path = pattern.laneSourcePaths[lane].data();
            label = pattern.laneSourceLabels[lane].data();
            return kind;
        }
        return LaneSourceKind::inherit;
    }

    void queuePatternSources(int patternIndex)
    {
        pendingPatternSourceMask = 0u;
        if (patternIndex < 0
            || patternIndex >= static_cast<int>(patterns.size())) return;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            const char* path = "";
            const char* label = "";
            const LaneSourceKind kind = resolvePatternLaneSource(
                patternIndex, lane, path, label);
            if (kind == LaneSourceKind::empty) {
                if (engine.laneLoaded(lane)) {
                    engine.beginLaneRemoval(lane);
                    pendingClearMask |= 1u << lane;
                }
                currentLaneSourcePaths[lane][0] = '\0';
                currentLaneSourceKinds[lane] = LaneSourceKind::empty;
                std::snprintf(laneSourceLabels[lane].data(),
                              laneSourceLabels[lane].size(), "EMPTY");
                continue;
            }
            if (kind == LaneSourceKind::inherit || path[0] == '\0') continue;
            if (std::strcmp(path, currentLaneSourcePaths[lane].data()) == 0) {
                currentLaneSourceKinds[lane] = kind;
                if (label[0] != '\0') {
                    std::snprintf(laneSourceLabels[lane].data(),
                                  laneSourceLabels[lane].size(), "%s", label);
                }
                continue;
            }
            std::snprintf(pendingPatternSourcePaths[lane].data(),
                          pendingPatternSourcePaths[lane].size(), "%s", path);
            pendingPatternSourceMask |= 1u << lane;
        }
    }

    void servicePatternSourceQueue()
    {
        if (pendingPatternSourceMask == 0u || !sampleWorkerReady) return;
        const int loadState = sampleLoadState.load(std::memory_order_acquire);
        if (loadState == static_cast<int>(SampleLoadState::requested)
            || loadState == static_cast<int>(SampleLoadState::loading)
            || loadState == static_cast<int>(SampleLoadState::ready)
            || loadState == static_cast<int>(SampleLoadState::removing)
            || loadState == static_cast<int>(SampleLoadState::copying)) return;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if ((pendingPatternSourceMask & (1u << lane)) == 0u) continue;
            pendingPatternSourceMask &= ~(1u << lane);
            requestLibrarySample(static_cast<int>(lane),
                                 pendingPatternSourcePaths[lane].data(), true,
                                 activePattern);
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
        waveDirtyMask |= 1u << lane;
        std::snprintf(laneSourceLabels[lane].data(), laneSourceLabels[lane].size(),
                      "%.*s",
                      static_cast<int>(laneSourceLabels[lane].size() - 1u),
                      sampleLoadedName.data());
        std::snprintf(currentLaneSourcePaths[lane].data(),
                      currentLaneSourcePaths[lane].size(), "%s",
                      sampleRequestPath.data());
        const int owner = clampValue(sampleRequestPattern, 0,
                                     static_cast<int>(kPatternCount) - 1);
        if (!sampleRequestFromPattern) {
            Pattern& pattern = patterns[static_cast<std::size_t>(owner)];
            pattern.laneSourceKinds[lane] = LaneSourceKind::file;
            std::snprintf(pattern.laneSourcePaths[lane].data(),
                          pattern.laneSourcePaths[lane].size(), "%s",
                          sampleRequestPath.data());
            std::snprintf(pattern.laneSourceLabels[lane].data(),
                          pattern.laneSourceLabels[lane].size(), "%.*s",
                          static_cast<int>(pattern.laneSourceLabels[lane].size() - 1u),
                          sampleLoadedName.data());
            pattern.occupied = true;
            currentLaneSourceKinds[lane] = LaneSourceKind::file;
            markPersistentStateDirty();
        } else {
            const char* resolvedPath = "";
            const char* resolvedLabel = "";
            currentLaneSourceKinds[lane] = resolvePatternLaneSource(
                owner, lane, resolvedPath, resolvedLabel);
            if (resolvedLabel[0] != '\0') {
                std::snprintf(laneSourceLabels[lane].data(),
                              laneSourceLabels[lane].size(), "%s",
                              resolvedLabel);
            }
        }
        sampleLoadState.store(static_cast<int>(SampleLoadState::complete),
                              std::memory_order_release);
    }

    void allNotesOff()
    {
        padMappedNotes.fill(-1);
        activeNote = -1;
        for (auto& voice : voices) {
            voice.held = false;
            voice.target = 0.0f;
            voice.sequenceStepsRemaining = 0;
        }
    }

    void releaseSequencerVoices()
    {
        for (auto& voice : voices) {
            if (!voice.sequencerOwned) continue;
            voice.held = false;
            voice.target = 0.0f;
            voice.sequenceStepsRemaining = 0;
        }
        updateActiveNote();
    }

    void updateActiveNote()
    {
        activeNote = -1;
        std::uint32_t newestAge = 0u;
        for (const auto& voice : voices) {
            if (voice.active && voice.held && !voice.sequencerOwned
                && voice.age >= newestAge) {
                activeNote = voice.note;
                newestAge = voice.age;
            }
        }
    }

    PlaybackVoice* findVoiceForNote(int note, int sourcePad)
    {
        for (auto& voice : voices) {
            if (voice.active && !voice.sequencerOwned && voice.note == note
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
                sequenceRandom ^= sequenceRandom << 13u;
                sequenceRandom ^= sequenceRandom >> 17u;
                sequenceRandom ^= sequenceRandom << 5u;
                choice = sequenceRandom % count;
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
        voice.sequencerOwned = false;
        voice.useSnapshot = false;
        voice.sequenceStepsRemaining = 0;
        performanceLane = voice.laneOffset * 3.0f;
        voice.engine.retrigger();
        if (sequenceRunning && sequenceCurrentStep >= 0 && sourcePad >= 0
            && sourcePad < static_cast<int>(captureStartSteps.size())) {
            auto& capture = performanceCapture[
                static_cast<std::size_t>(sequenceCurrentStep)];
            std::size_t chord = capture.noteCount;
            for (std::size_t index = 0u; index < capture.noteCount; ++index) {
                if (capture.sourcePads[index]
                    == static_cast<std::uint8_t>(sourcePad)) {
                    chord = index;
                    break;
                }
            }
            if (chord < kSequenceChordSize) {
                if (chord == capture.noteCount) ++capture.noteCount;
                capture.notes[chord] = static_cast<std::uint8_t>(note);
                capture.velocities[chord] = static_cast<std::uint8_t>(velocity);
                capture.sourcePads[chord] = static_cast<std::uint8_t>(sourcePad);
                capture.lengthSteps[chord] = 1u;
                capture.noteOpen[chord] = 1u;
                capture.enabled = true;
                captureStartSteps[static_cast<std::size_t>(sourcePad)] =
                    sequenceCurrentStep;
            }
        }
        updateActiveNote();
    }

    void noteOff(int note, int sourcePad = -1)
    {
        note = clampValue(note, 0, 127);
        for (auto& voice : voices) {
            if (voice.active && !voice.sequencerOwned && voice.note == note
                && voice.sourcePad == sourcePad) {
                voice.held = false;
                voice.target = 0.0f;
            }
        }
        if (sequenceRunning && sequenceCurrentStep >= 0 && sourcePad >= 0
            && sourcePad < static_cast<int>(captureStartSteps.size())) {
            const int start = captureStartSteps[static_cast<std::size_t>(sourcePad)];
            if (start >= 0 && start < static_cast<int>(performanceCapture.size())) {
                auto& capture = performanceCapture[static_cast<std::size_t>(start)];
                int length = (sequenceCurrentStep - start + sequenceLength)
                    % sequenceLength;
                if (length == 0) length = sequenceLength;
                for (std::size_t chord = 0u; chord < capture.noteCount; ++chord) {
                    if (capture.sourcePads[chord]
                        != static_cast<std::uint8_t>(sourcePad)) continue;
                    capture.lengthSteps[chord] = static_cast<std::uint8_t>(
                        clampValue(length, 1, static_cast<int>(kSequenceStepCount)));
                    capture.noteOpen[chord] = 0u;
                    break;
                }
            }
            captureStartSteps[static_cast<std::size_t>(sourcePad)] = -1;
        }
        updateActiveNote();
    }

    void aftertouch(int note, int pressure, int sourcePad = -1)
    {
        if (pressureMode == 0) return;
        const float offset = midiUnit(pressure);
        for (auto& voice : voices) {
            if (voice.active && voice.held && !voice.sequencerOwned
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
            if (voice.active && voice.held && !voice.sequencerOwned
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
        if (activePattern < 0 || activePattern >= static_cast<int>(patterns.size())
            || slot < 0 || slot >= static_cast<int>(kCueCount)) return;
        pushPatternUndo(activePattern);
        patterns[static_cast<std::size_t>(activePattern)].cues[
            static_cast<std::size_t>(slot)] = clampValue(position, 0.0f, 1.0f);
        patterns[static_cast<std::size_t>(activePattern)].occupied = true;
        markPersistentStateDirty();
    }

    void clearCue(int slot)
    {
        if (activePattern < 0 || activePattern >= static_cast<int>(patterns.size())
            || slot < 0 || slot >= static_cast<int>(kCueCount)) return;
        pushPatternUndo(activePattern);
        patterns[static_cast<std::size_t>(activePattern)].cues[
            static_cast<std::size_t>(slot)] = -1.0f;
        markPersistentStateDirty();
    }

    void triggerCue(int slot)
    {
        if (activePattern < 0 || activePattern >= static_cast<int>(patterns.size())
            || slot < 0 || slot >= static_cast<int>(kCueCount)) return;
        const float cue = patterns[static_cast<std::size_t>(activePattern)].cues[
            static_cast<std::size_t>(slot)];
        if (cue < 0.0f) return;
        bool hasPerformanceVoice = false;
        for (const auto& voice : voices) {
            hasPerformanceVoice = hasPerformanceVoice
                || (voice.active && voice.held && !voice.sequencerOwned);
        }
        if (!hasPerformanceVoice) noteOn(rootNote, 100, slot);
        for (auto& voice : voices) {
            if (voice.active) voice.engine.seekNormalized(cue);
        }
    }

    Engine::Controls effectiveControls(const PlaybackVoice& voice) const
    {
        Engine::Controls effective = voice.useSnapshot
            ? voice.snapshotControls : controls;
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
        effective.cutTempoBpm = sequenceBpm;
        effective.cutSwing = sequenceSwing;
        effective.cutVoiceIndex = static_cast<float>(voice.sourcePad >= 0
            ? voice.sourcePad : std::max(0, voice.note - rootNote));
        effective.sampleLane = static_cast<float>(voice.variantLane);
        effective.sampleSlice = static_cast<float>(voice.sourcePad >= 0
            ? voice.sourcePad : std::max(0, voice.note - rootNote));
        effective.lanePhaseOffset = pressureMode != 0
            ? clampValue(voice.laneOffset, 0.0f, 1.0f) : 0.0f;
        effective.mix = 1.0f;
        effective.recordInputGain = 1.0f;
        return effective;
    }

    PlaybackVoice& allocateSequenceVoice()
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
            if (voice.sequencerOwned && !candidate->sequencerOwned) candidate = &voice;
            else if (voice.sequencerOwned == candidate->sequencerOwned
                     && voice.age < candidate->age) candidate = &voice;
        }
        return *candidate;
    }

    void triggerSequenceVoice(const SequenceStep& step, int note, int velocity,
                              int sourcePad, int lengthSteps)
    {
        ++noteClock;
        PlaybackVoice& voice = allocateSequenceVoice();
        voice.note = clampValue(note, 0, 127);
        voice.sourcePad = sourcePad;
        voice.velocity = clampValue(velocity, 1, 127);
        voice.variantLane = chooseVariationLane(voice.velocity);
        voice.age = noteClock;
        voice.laneOffset = clampValue(controls.manualLane / 3.0f, 0.0f, 1.0f);
        voice.snapshotControls = controls;
        voice.snapshotOutputGain = outputGain;
        for (std::size_t index = 0u; index < kLockParameterCount; ++index) {
            if (hasStepLock(step, index)) {
                applyLockedValue(voice.snapshotControls, voice.snapshotOutputGain,
                                 index, step.locks[index]);
            }
        }
        voice.target = 1.0f;
        voice.active = true;
        voice.held = true;
        voice.sequencerOwned = true;
        voice.useSnapshot = true;
        voice.sequenceStepsRemaining = clampValue(
            lengthSteps, 1, static_cast<int>(kSequenceStepCount));
        voice.engine.retrigger();
        updateActiveNote();
    }

    int legacySequenceLength(const SequenceStep& step) const
    {
        const float gate = step.gateLocked ? step.gate : sequenceGate;
        return clampValue(static_cast<int>(std::ceil(gate)), 1,
                          static_cast<int>(kSequenceStepCount));
    }

    void triggerSequenceStep(const SequenceStep& step)
    {
        if (step.noteCount == 0u) {
            triggerSequenceVoice(step, rootNote, 100, -1,
                                 legacySequenceLength(step));
            return;
        }
        const std::size_t count = std::min<std::size_t>(
            step.noteCount, kSequenceChordSize);
        for (std::size_t index = 0u; index < count; ++index) {
            const int sourcePad = step.sourcePads[index] < 32u
                ? static_cast<int>(step.sourcePads[index]) : -1;
            const int length = step.lengthSteps[index] > 0u
                ? static_cast<int>(step.lengthSteps[index])
                : legacySequenceLength(step);
            triggerSequenceVoice(step, step.notes[index], step.velocities[index],
                                 sourcePad, length);
        }
    }

    double sequenceStepSamples(int step) const
    {
        const double base = static_cast<double>(sampleRate) * 15.0
            / static_cast<double>(sequenceBpm);
        const double swingFraction = clampValue(
            static_cast<double>(sequenceSwing), 0.5, 0.75);
        const double swung = base * ((step & 1) == 0
            ? 2.0 * swingFraction : 2.0 * (1.0 - swingFraction));
        const int next = (step + 1) % std::max(1, sequenceLength);
        const double currentOffset = base
            * static_cast<double>(sequence[static_cast<std::size_t>(step)].microTiming)
            / 100.0;
        const double nextOffset = base
            * static_cast<double>(sequence[static_cast<std::size_t>(next)].microTiming)
            / 100.0;
        return std::max<double>(static_cast<double>(kBlockFrames),
                                swung + nextOffset - currentOffset);
    }

    bool sequenceProbabilityPasses(const SequenceStep& step)
    {
        if (step.probability >= 100u) return true;
        if (step.probability == 0u) return false;
        sequenceRandom ^= sequenceRandom << 13u;
        sequenceRandom ^= sequenceRandom >> 17u;
        sequenceRandom ^= sequenceRandom << 5u;
        return sequenceRandom % 100u < step.probability;
    }

    int nextEnabledChainSlot(int after) const
    {
        for (std::size_t offset = 1u; offset <= patternChain.size(); ++offset) {
            const int slot = (after + static_cast<int>(offset))
                % static_cast<int>(patternChain.size());
            if (patternChain[static_cast<std::size_t>(slot)].enabled) return slot;
        }
        return -1;
    }

    int firstEnabledChainSlot() const
    {
        for (std::size_t slot = 0u; slot < patternChain.size(); ++slot) {
            if (patternChain[slot].enabled) return static_cast<int>(slot);
        }
        return -1;
    }

    void prepareNextChainPattern()
    {
        if (!chainEnabled) return;
        const int first = firstEnabledChainSlot();
        if (first < 0) return;
        if (chainPosition < 0
            || chainPosition >= static_cast<int>(patternChain.size())
            || !patternChain[static_cast<std::size_t>(chainPosition)].enabled) {
            chainPosition = first;
            chainRepeat = 0;
        }
        const int next = nextEnabledChainSlot(chainPosition);
        if (next < 0) return;
        chainNextSlot = next;
        const int target = patternChain[static_cast<std::size_t>(next)].pattern;
        queuedPattern = target;
        requestPatternPreparation(target);
    }

    void advancePatternChain()
    {
        if (!chainEnabled) return;
        const int first = firstEnabledChainSlot();
        if (first < 0) return;
        if (chainPosition < 0
            || chainPosition >= static_cast<int>(patternChain.size())
            || !patternChain[static_cast<std::size_t>(chainPosition)].enabled) {
            chainPosition = first;
            chainRepeat = 0;
        }
        if (chainNextSlot < 0) prepareNextChainPattern();
        const PatternChainEntry& current =
            patternChain[static_cast<std::size_t>(chainPosition)];
        ++chainRepeat;
        if (chainRepeat < std::max(1, static_cast<int>(current.repeats))) return;
        if (chainNextSlot < 0) return;
        const int target = patternChain[
            static_cast<std::size_t>(chainNextSlot)].pattern;
        const auto prepareState = static_cast<PatternPrepareState>(
            patternPrepareState.load(std::memory_order_acquire));
        if (prepareState != PatternPrepareState::ready
            || preparedPattern.load(std::memory_order_relaxed) != target) {
            requestPatternPreparation(target);
            return;
        }
        chainPosition = chainNextSlot;
        chainNextSlot = -1;
        chainRepeat = 0;
        activatePreparedPattern();
        sequenceCurrentStep = -1;
        prepareNextChainPattern();
    }

    void advanceSequence()
    {
        for (auto& voice : voices) {
            if (!voice.active || !voice.held || !voice.sequencerOwned) continue;
            if (--voice.sequenceStepsRemaining <= 0) {
                voice.held = false;
                voice.target = 0.0f;
                voice.sequenceStepsRemaining = 0;
            }
        }
        const bool patternBoundary = sequenceCurrentStep >= 0
            && (sequenceCurrentStep + 1) % sequenceLength == 0;
        if (patternBoundary && chainEnabled && firstEnabledChainSlot() >= 0) {
            advancePatternChain();
        } else if (patternBoundary && queuedPattern >= 0) {
            const auto prepareState = static_cast<PatternPrepareState>(
                patternPrepareState.load(std::memory_order_acquire));
            if (prepareState == PatternPrepareState::ready
                && preparedPattern.load(std::memory_order_relaxed)
                    == queuedPattern) {
                activatePreparedPattern();
                sequenceCurrentStep = -1;
            } else if (prepareState == PatternPrepareState::idle
                       || prepareState == PatternPrepareState::error) {
                requestPatternPreparation(queuedPattern);
            }
        }
        sequenceCurrentStep = (sequenceCurrentStep + 1) % sequenceLength;
        const SequenceStep& step = sequence[static_cast<std::size_t>(sequenceCurrentStep)];
        const double interval = sequenceStepSamples(sequenceCurrentStep);
        sequenceSamplesUntilStep += interval;
        ratchetStep = -1;
        ratchetsRemaining = 0;
        if (step.enabled && sequenceProbabilityPasses(step)) {
            triggerSequenceStep(step);
            const int ratchets = clampValue(static_cast<int>(step.ratchets), 1, 4);
            if (ratchets > 1) {
                ratchetStep = sequenceCurrentStep;
                ratchetsRemaining = ratchets - 1;
                ratchetSamplesUntilTrigger = interval / static_cast<double>(ratchets);
            }
        }
    }

    void serviceSequenceBeforeBlock()
    {
        auto prepareState = static_cast<PatternPrepareState>(
            patternPrepareState.load(std::memory_order_acquire));
        if (queuedPattern >= 0
            && prepareState == PatternPrepareState::ready
            && preparedPattern.load(std::memory_order_relaxed)
                != queuedPattern) {
            preparedPattern.store(-1, std::memory_order_relaxed);
            patternPrepareState.store(static_cast<int>(PatternPrepareState::idle),
                                      std::memory_order_release);
            prepareState = PatternPrepareState::idle;
        }
        if (queuedPattern >= 0
            && prepareState == PatternPrepareState::idle) {
            requestPatternPreparation(queuedPattern);
            prepareState = PatternPrepareState::requested;
        }
        if (!sequenceRunning && queuedPattern >= 0
            && prepareState == PatternPrepareState::ready
            && preparedPattern.load(std::memory_order_relaxed)
                == queuedPattern) {
            activatePreparedPattern();
        } else if (queuedPattern >= 0
                   && prepareState == PatternPrepareState::error) {
            setError("Queued pattern sample preparation failed");
        }
        if (!sequenceRunning) return;
        while (sequenceSamplesUntilStep <= 0.0) advanceSequence();
        while (ratchetsRemaining > 0 && ratchetSamplesUntilTrigger <= 0.0
               && ratchetStep >= 0) {
            triggerSequenceStep(sequence[static_cast<std::size_t>(ratchetStep)]);
            --ratchetsRemaining;
            const int ratchets = clampValue(static_cast<int>(
                sequence[static_cast<std::size_t>(ratchetStep)].ratchets), 1, 4);
            ratchetSamplesUntilTrigger += sequenceStepSamples(ratchetStep)
                / static_cast<double>(ratchets);
        }
    }

    void serviceSequenceAfterBlock(std::size_t frames)
    {
        if (!sequenceRunning) return;
        sequenceSamplesUntilStep -= static_cast<double>(frames);
        if (ratchetsRemaining > 0) {
            ratchetSamplesUntilTrigger -= static_cast<double>(frames);
        }
    }

    void setSequenceRunning(bool running)
    {
        if (running == sequenceRunning) return;
        sequenceRunning = running;
        sequenceCurrentStep = -1;
        sequenceSamplesUntilStep = 0.0;
        ratchetStep = -1;
        ratchetsRemaining = 0;
        if (running && chainEnabled) {
            const int first = firstEnabledChainSlot();
            if (first >= 0) {
                chainPosition = first;
                chainRepeat = 0;
                const int target = patternChain[static_cast<std::size_t>(first)].pattern;
                if (target != activePattern) {
                    chainNextSlot = first;
                    chainRepeat = std::max(1, static_cast<int>(
                        patternChain[static_cast<std::size_t>(first)].repeats)) - 1;
                    queuedPattern = target;
                    requestPatternPreparation(target);
                } else {
                    chainNextSlot = -1;
                    prepareNextChainPattern();
                }
            }
        } else if (!running) {
            releaseSequencerVoices();
            captureStartSteps.fill(-1);
            chainNextSlot = -1;
        }
    }

    bool workingPatternHasContent() const
    {
        for (const auto& step : sequence) {
            if (step.enabled || step.noteCount != 0u || stepHasLocks(step)) {
                return true;
            }
        }
        return false;
    }

    void markPersistentStateDirty() { persistentStateDirty = true; }

    void pushPatternUndo(int index)
    {
        if (index < 0 || index >= static_cast<int>(patterns.size())) return;
        if (index == activePattern) captureActivePattern(false);
        undoPattern = patterns[static_cast<std::size_t>(index)];
        undoPatternIndex = index;
        undoAvailable = true;
    }

    void restoreUndo()
    {
        if (!undoAvailable || undoPatternIndex < 0
            || undoPatternIndex >= static_cast<int>(patterns.size())) return;
        if (patternPreparationInFlight()) {
            setError("Wait for the queued pattern to finish preparing");
            return;
        }
        patterns[static_cast<std::size_t>(undoPatternIndex)] = undoPattern;
        if (activePattern == undoPatternIndex) activateStoredPattern(activePattern);
        undoAvailable = false;
        markPersistentStateDirty();
    }

    void captureActivePattern(bool forceOccupied)
    {
        if (activePattern < 0 || activePattern >= static_cast<int>(patterns.size())) {
            return;
        }
        Pattern& pattern = patterns[static_cast<std::size_t>(activePattern)];
        pattern.steps = sequence;
        pattern.swing = sequenceSwing;
        pattern.legacyGate = sequenceGate;
        pattern.length = static_cast<std::uint8_t>(clampValue(
            sequenceLength, 1, static_cast<int>(kSequenceStepCount)));
        for (std::size_t index = 0u; index < kLockParameterCount; ++index) {
            pattern.baseValues[index] = lockedValue(controls, outputGain, index);
        }
        pattern.occupied = forceOccupied || activePatternOccupied
            || workingPatternHasContent();
        activePatternOccupied = pattern.occupied;
        patternBankInitialized = true;
    }

    void activateStoredPattern(int index, bool queueSources = true)
    {
        if (index < 0 || index >= static_cast<int>(patterns.size())) return;
        Pattern& target = patterns[static_cast<std::size_t>(index)];
        activePattern = index;
        activePatternOccupied = target.occupied;
        sequence = target.steps;
        sequenceSwing = clampValue(target.swing, 0.5f, 0.75f);
        sequenceGate = clampValue(target.legacyGate, 0.05f, 8.0f);
        sequenceLength = clampValue(static_cast<int>(target.length), 1,
                                    static_cast<int>(kSequenceStepCount));
        for (std::size_t parameter = 0u;
             parameter < kLockParameterCount; ++parameter) {
            applyLockedValue(controls, outputGain, parameter,
                             target.baseValues[parameter]);
        }
        if (queueSources) queuePatternSources(index);
        queuedPattern = -1;
        if (!sequenceRunning) {
            sequenceCurrentStep = -1;
            sequenceSamplesUntilStep = 0.0;
        }
    }

    bool activatePreparedPattern()
    {
        if (patternPrepareState.load(std::memory_order_acquire)
                != static_cast<int>(PatternPrepareState::ready)
            || preparedPattern.load(std::memory_order_relaxed) < 0
            || preparedPattern.load(std::memory_order_relaxed)
                >= static_cast<int>(patterns.size())
            || engine.recording()) return false;
        allNotesOff();
        storage.swap(preparedPatternStorage);
        if (!engine.rebindStorage(storage.get() + planeCapacityFrames * 0u,
                                  storage.get() + planeCapacityFrames * 1u,
                                  storage.get() + planeCapacityFrames * 2u,
                                  storage.get() + planeCapacityFrames * 3u,
                                  planeCapacityFrames)) {
            storage.swap(preparedPatternStorage);
            patternPrepareState.store(
                static_cast<int>(PatternPrepareState::error),
                std::memory_order_release);
            setError("Prepared pattern bank could not be activated");
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
            currentLaneSourceKinds[lane] = preparedLaneKinds[lane];
            std::snprintf(currentLaneSourcePaths[lane].data(),
                          currentLaneSourcePaths[lane].size(), "%s",
                          preparedLanePaths[lane].data());
            std::snprintf(laneSourceLabels[lane].data(),
                          laneSourceLabels[lane].size(), "%s",
                          preparedLaneLabels[lane][0] != '\0'
                              ? preparedLaneLabels[lane].data()
                              : (preparedLaneFrames[lane] != 0u
                                  ? "INHERIT" : "EMPTY"));
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
        waveDirtyMask = (1u << kLaneCount) - 1u;
        scanLane = -1;
        const int target = preparedPattern.load(std::memory_order_relaxed);
        preparedPattern.store(-1, std::memory_order_relaxed);
        patternPrepareState.store(static_cast<int>(PatternPrepareState::idle),
                                  std::memory_order_release);
        activateStoredPattern(target, false);
        return true;
    }

    void loadPattern(int index)
    {
        if (index < 0 || index >= static_cast<int>(patterns.size())) return;
        captureActivePattern(false);
        Pattern& target = patterns[static_cast<std::size_t>(index)];
        if (!target.occupied) {
            target = Pattern {};
            target.swing = sequenceSwing;
            target.legacyGate = sequenceGate;
            target.length = static_cast<std::uint8_t>(sequenceLength);
            for (std::size_t parameter = 0u;
                 parameter < kLockParameterCount; ++parameter) {
                target.baseValues[parameter] = lockedValue(
                    controls, outputGain, parameter);
            }
            if (activePattern >= 0
                && activePattern < static_cast<int>(patterns.size())) {
                const Pattern& previous = patterns[static_cast<std::size_t>(
                    activePattern)];
                target.laneSourcePaths = previous.laneSourcePaths;
                target.laneSourceLabels = previous.laneSourceLabels;
            }
            target.occupied = true;
        }
        activateStoredPattern(index);
        markPersistentStateDirty();
    }

    void initializePatternIfNeeded(int index)
    {
        if (index < 0 || index >= static_cast<int>(patterns.size())) return;
        Pattern& target = patterns[static_cast<std::size_t>(index)];
        if (target.occupied) return;
        target = Pattern {};
        target.swing = sequenceSwing;
        target.legacyGate = sequenceGate;
        target.length = static_cast<std::uint8_t>(sequenceLength);
        for (std::size_t parameter = 0u;
             parameter < kLockParameterCount; ++parameter) {
            target.baseValues[parameter] = lockedValue(
                controls, outputGain, parameter);
        }
        target.occupied = true;
        markPersistentStateDirty();
    }

    void selectPattern(int index)
    {
        if (index < 0 || index >= static_cast<int>(patterns.size())) return;
        if (index == activePattern) {
            queuedPattern = -1;
            return;
        }
        if (chainEnabled) {
            chainEnabled = false;
            chainNextSlot = -1;
        }
        if (pendingCaptureSaveMask != 0u && !serviceMaintenance()) {
            setError("Pattern change is waiting for capture save");
            return;
        }
        captureActivePattern(false);
        queuedPattern = index;
        requestPatternPreparation(index);
    }

    bool patternPreparationBusy(int index) const
    {
        const auto state = static_cast<PatternPrepareState>(
            patternPrepareState.load(std::memory_order_acquire));
        return index == patternPrepareRequest.load(std::memory_order_relaxed)
            && (state == PatternPrepareState::requested
                || state == PatternPrepareState::loading);
    }

    void copyActivePattern(int destination)
    {
        if (destination < 0
            || destination >= static_cast<int>(patterns.size())) return;
        if (patternPreparationInFlight()) {
            setError("Queued pattern is still preparing");
            return;
        }
        captureActivePattern(true);
        pushPatternUndo(destination);
        patterns[static_cast<std::size_t>(destination)] =
            patterns[static_cast<std::size_t>(activePattern)];
        patterns[static_cast<std::size_t>(destination)].occupied = true;
        markPersistentStateDirty();
    }

    void clearPattern(int index)
    {
        if (index < 0 || index >= static_cast<int>(patterns.size())) return;
        if (patternPreparationInFlight()) {
            setError("Queued pattern is still preparing");
            return;
        }
        pushPatternUndo(index);
        patterns[static_cast<std::size_t>(index)] = Pattern {};
        if (queuedPattern == index) queuedPattern = -1;
        if (activePattern == index) {
            sequence = {};
            sequenceLength = 16;
            sequenceSwing = 0.5f;
            sequenceGate = 0.8f;
            activePatternOccupied = false;
            if (sequenceRunning) releaseSequencerVoices();
        }
        markPersistentStateDirty();
    }

    std::uint64_t occupiedPatternMask() const
    {
        std::uint64_t mask = 0u;
        for (std::size_t index = 0u; index < patterns.size(); ++index) {
            if (patterns[index].occupied) mask |= std::uint64_t { 1u } << index;
        }
        return mask;
    }

    std::size_t visibleSequenceOffset() const
    {
        return static_cast<std::size_t>(clampValue(
            sequencePage, 0, static_cast<int>(kSequencePageCount) - 1))
            * kVisibleStepCount;
    }

    std::uint32_t sequenceMask() const
    {
        std::uint32_t mask = 0u;
        const std::size_t offset = visibleSequenceOffset();
        for (std::size_t index = 0u; index < kVisibleStepCount; ++index) {
            if (sequence[offset + index].enabled) mask |= 1u << index;
        }
        return mask;
    }

    void toggleSequenceStep(int index)
    {
        if (index < 0 || index >= static_cast<int>(sequence.size())) return;
        auto& step = sequence[static_cast<std::size_t>(index)];
        step.enabled = !step.enabled;
        markPersistentStateDirty();
    }

    void setSequenceNote(int index, int note, int velocity)
    {
        if (index < 0 || index >= static_cast<int>(sequence.size())) return;
        auto& step = sequence[static_cast<std::size_t>(index)];
        step.notes[0u] = static_cast<std::uint8_t>(clampValue(note, 0, 127));
        step.velocities[0u] = static_cast<std::uint8_t>(
            clampValue(velocity, 1, 127));
        step.sourcePads[0u] = 255u;
        step.lengthSteps[0u] = static_cast<std::uint8_t>(
            legacySequenceLength(step));
        step.noteOpen[0u] = 0u;
        step.noteCount = 1u;
        step.enabled = true;
        markPersistentStateDirty();
    }

    void removeSequenceNote(SequenceStep& step, std::size_t chord)
    {
        if (chord >= step.noteCount) return;
        for (std::size_t move = chord + 1u; move < step.noteCount; ++move) {
            step.notes[move - 1u] = step.notes[move];
            step.velocities[move - 1u] = step.velocities[move];
            step.sourcePads[move - 1u] = step.sourcePads[move];
            step.lengthSteps[move - 1u] = step.lengthSteps[move];
            step.noteOpen[move - 1u] = step.noteOpen[move];
        }
        --step.noteCount;
        const std::size_t tail = step.noteCount;
        step.notes[tail] = 0u;
        step.velocities[tail] = 0u;
        step.sourcePads[tail] = 255u;
        step.lengthSteps[tail] = 1u;
        step.noteOpen[tail] = 0u;
    }

    void toggleSequenceNote(int index, int note, int velocity)
    {
        if (index < 0 || index >= static_cast<int>(sequence.size())) return;
        auto& step = sequence[static_cast<std::size_t>(index)];
        const std::uint8_t storedNote = static_cast<std::uint8_t>(
            clampValue(note, 0, 127));
        for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
            if (step.notes[chord] != storedNote) continue;
            removeSequenceNote(step, chord);
            step.enabled = true;
            markPersistentStateDirty();
            return;
        }
        if (step.noteCount >= kSequenceChordSize) return;
        const std::size_t chord = step.noteCount++;
        step.notes[chord] = storedNote;
        step.velocities[chord] = static_cast<std::uint8_t>(
            clampValue(velocity, 1, 127));
        step.sourcePads[chord] = 255u;
        step.lengthSteps[chord] = static_cast<std::uint8_t>(
            legacySequenceLength(step));
        step.noteOpen[chord] = 0u;
        step.enabled = true;
        markPersistentStateDirty();
    }

    void toggleSequencePadNote(int index, int pad, int note, int velocity)
    {
        if (index < 0 || index >= static_cast<int>(sequence.size())
            || pad < 0 || pad >= 32) return;
        auto& step = sequence[static_cast<std::size_t>(index)];
        const std::uint8_t storedPad = static_cast<std::uint8_t>(pad);
        for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
            if (step.sourcePads[chord] != storedPad) continue;
            removeSequenceNote(step, chord);
            step.enabled = step.noteCount > 0u || stepHasLocks(step);
            markPersistentStateDirty();
            return;
        }
        if (step.noteCount >= kSequenceChordSize) return;
        const std::size_t chord = step.noteCount++;
        step.notes[chord] = static_cast<std::uint8_t>(clampValue(note, 0, 127));
        step.velocities[chord] = static_cast<std::uint8_t>(
            clampValue(velocity, 1, 127));
        step.sourcePads[chord] = storedPad;
        step.lengthSteps[chord] = static_cast<std::uint8_t>(sequenceLength);
        step.noteOpen[chord] = 1u;
        step.enabled = true;
        markPersistentStateDirty();
    }

    void setSequenceNoteLength(int index, int pad, int length)
    {
        if (index < 0 || index >= static_cast<int>(sequence.size())
            || pad < 0 || pad >= 32) return;
        auto& step = sequence[static_cast<std::size_t>(index)];
        for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
            if (step.sourcePads[chord] == static_cast<std::uint8_t>(pad)) {
                step.lengthSteps[chord] = static_cast<std::uint8_t>(
                    clampValue(length, 1,
                               static_cast<int>(kSequenceStepCount)));
                step.noteOpen[chord] = 0u;
                markPersistentStateDirty();
                return;
            }
        }
    }

    void setSequenceLock(int stepIndex, const char* key, float value)
    {
        if (stepIndex < 0 || stepIndex >= static_cast<int>(sequence.size())) return;
        const int lockIndex = lockParameterIndex(key);
        if (lockIndex < 0) return;
        auto& step = sequence[static_cast<std::size_t>(stepIndex)];
        step.locks[static_cast<std::size_t>(lockIndex)] = value;
        setStepLock(step, static_cast<std::size_t>(lockIndex));
        step.enabled = true;
        markPersistentStateDirty();
    }

    void setSequenceProbability(int percent)
    {
        auto& step = sequence[static_cast<std::size_t>(clampValue(
            sequenceEditStep, 0, static_cast<int>(sequence.size()) - 1))];
        step.probability = static_cast<std::uint8_t>(clampValue(percent, 0, 100));
        step.enabled = step.enabled || stepHasLocks(step) || step.noteCount != 0u;
        markPersistentStateDirty();
    }

    void setSequenceRatchets(int count)
    {
        auto& step = sequence[static_cast<std::size_t>(clampValue(
            sequenceEditStep, 0, static_cast<int>(sequence.size()) - 1))];
        step.ratchets = static_cast<std::uint8_t>(clampValue(count, 1, 4));
        markPersistentStateDirty();
    }

    void setSequenceMicroTiming(int amount)
    {
        auto& step = sequence[static_cast<std::size_t>(clampValue(
            sequenceEditStep, 0, static_cast<int>(sequence.size()) - 1))];
        step.microTiming = static_cast<std::int8_t>(clampValue(amount, -49, 49));
        markPersistentStateDirty();
    }

    void commitPerformanceCapture()
    {
        bool hasContent = false;
        for (const auto& step : performanceCapture) {
            hasContent = hasContent || step.noteCount != 0u;
        }
        if (!hasContent) return;
        pushPatternUndo(activePattern);
        for (std::size_t index = 0u; index < sequence.size(); ++index) {
            const auto& captured = performanceCapture[index];
            if (captured.noteCount == 0u) continue;
            auto& target = sequence[index];
            for (std::size_t capturedChord = 0u;
                 capturedChord < captured.noteCount; ++capturedChord) {
                std::size_t destination = target.noteCount;
                for (std::size_t chord = 0u; chord < target.noteCount; ++chord) {
                    if (captured.sourcePads[capturedChord] < 32u
                        && target.sourcePads[chord]
                            == captured.sourcePads[capturedChord]) {
                        destination = chord;
                        break;
                    }
                }
                if (destination >= kSequenceChordSize) continue;
                if (destination == target.noteCount) ++target.noteCount;
                target.notes[destination] = captured.notes[capturedChord];
                target.velocities[destination] = captured.velocities[capturedChord];
                target.sourcePads[destination] = captured.sourcePads[capturedChord];
                target.lengthSteps[destination] = captured.lengthSteps[capturedChord];
                target.noteOpen[destination] = 0u;
                target.enabled = true;
            }
        }
        performanceCapture = {};
        captureStartSteps.fill(-1);
        activePatternOccupied = true;
        markPersistentStateDirty();
    }

    void setChainEntry(int slot, int pattern, int repeats)
    {
        if (slot < 0 || slot >= static_cast<int>(patternChain.size())) return;
        auto& entry = patternChain[static_cast<std::size_t>(slot)];
        entry.pattern = static_cast<std::uint8_t>(clampValue(
            pattern, 0, static_cast<int>(kPatternCount) - 1));
        entry.repeats = static_cast<std::uint8_t>(clampValue(repeats, 1, 16));
        entry.enabled = true;
        markPersistentStateDirty();
    }

    void clearChainEntry(int slot)
    {
        if (slot < 0 || slot >= static_cast<int>(patternChain.size())) return;
        patternChain[static_cast<std::size_t>(slot)] = PatternChainEntry {};
        if (chainPosition == slot) chainRepeat = 0;
        markPersistentStateDirty();
    }

    int formatPatternChain(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        for (std::size_t slot = 0u; slot < patternChain.size(); ++slot) {
            const auto& entry = patternChain[slot];
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                slot == 0u ? "%u:%u:%u" : ",%u:%u:%u",
                entry.enabled ? 1u : 0u, static_cast<unsigned>(entry.pattern),
                static_cast<unsigned>(entry.repeats));
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    void setSequenceStepGate(int stepIndex, float percent)
    {
        if (stepIndex < 0 || stepIndex >= static_cast<int>(sequence.size())) return;
        auto& step = sequence[static_cast<std::size_t>(stepIndex)];
        step.gate = clampValue(percent / 100.0f, 0.05f, 8.0f);
        step.gateLocked = true;
        const std::uint8_t migratedLength = static_cast<std::uint8_t>(
            legacySequenceLength(step));
        for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
            if (step.sourcePads[chord] == 255u) {
                step.lengthSteps[chord] = migratedLength;
            }
        }
        step.enabled = true;
    }

    void setSequenceGate(float percent)
    {
        sequenceGate = clampValue(percent / 100.0f, 0.05f, 8.0f);
        for (auto& step : sequence) {
            if (step.gateLocked) continue;
            const std::uint8_t migratedLength = static_cast<std::uint8_t>(
                legacySequenceLength(step));
            for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
                if (step.sourcePads[chord] == 255u) {
                    step.lengthSteps[chord] = migratedLength;
                }
            }
        }
    }

    void clearSequenceLocks(int stepIndex)
    {
        if (stepIndex < 0 || stepIndex >= static_cast<int>(sequence.size())) return;
        auto& step = sequence[static_cast<std::size_t>(stepIndex)];
        step.lockMask = 0u;
        step.lockMaskHigh = 0u;
        step.lockMaskExtra = 0u;
        step.gateLocked = false;
        markPersistentStateDirty();
    }

    void clearSequenceStep(int stepIndex)
    {
        if (stepIndex < 0 || stepIndex >= static_cast<int>(sequence.size())) return;
        pushPatternUndo(activePattern);
        sequence[static_cast<std::size_t>(stepIndex)] = SequenceStep {};
        markPersistentStateDirty();
    }

    void clearSequence()
    {
        pushPatternUndo(activePattern);
        setSequenceRunning(false);
        for (auto& step : sequence) {
            step = SequenceStep {};
        }
        markPersistentStateDirty();
    }

    int formatSequenceLocks(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        const std::size_t offset = visibleSequenceOffset();
        for (std::size_t index = 0u; index < kVisibleStepCount; ++index) {
            const auto& step = sequence[offset + index];
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                index == 0u ? "%u" : ",%u", step.lockMask);
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    int formatSequenceLocksHigh(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        const std::size_t offset = visibleSequenceOffset();
        for (std::size_t index = 0u; index < kVisibleStepCount; ++index) {
            const auto& step = sequence[offset + index];
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                index == 0u ? "%u" : ",%u", step.lockMaskHigh);
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    int formatSequenceLocksExtra(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        const std::size_t offset = visibleSequenceOffset();
        for (std::size_t index = 0u; index < kVisibleStepCount; ++index) {
            const auto& step = sequence[offset + index];
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                index == 0u ? "%u" : ",%u", step.lockMaskExtra);
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    int formatSequenceNoteCounts(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        const std::size_t offset = visibleSequenceOffset();
        for (std::size_t index = 0u; index < kVisibleStepCount; ++index) {
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                index == 0u ? "%u" : ",%u",
                static_cast<unsigned>(sequence[offset + index].noteCount));
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    int formatSequencePadMasks(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        const std::size_t offset = visibleSequenceOffset();
        for (std::size_t index = 0u; index < kVisibleStepCount; ++index) {
            std::uint32_t mask = 0u;
            const auto& step = sequence[offset + index];
            for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
                if (step.sourcePads[chord] < 32u) {
                    mask |= 1u << step.sourcePads[chord];
                }
            }
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                index == 0u ? "%u" : ",%u", mask);
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    std::uint32_t sequenceOpenPadMask() const
    {
        std::uint32_t mask = 0u;
        const std::size_t offset = visibleSequenceOffset();
        for (std::size_t index = 0u; index < kVisibleStepCount; ++index) {
            const auto& step = sequence[offset + index];
            for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
                if (step.noteOpen[chord] != 0u && step.sourcePads[chord] < 32u) {
                    mask |= 1u << step.sourcePads[chord];
                }
            }
        }
        return mask;
    }

    int formatSequenceOpenSteps(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        std::array<int, 32u> starts {};
        starts.fill(-1);
        for (std::size_t stepIndex = 0u; stepIndex < sequence.size(); ++stepIndex) {
            const auto& step = sequence[stepIndex];
            for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
                if (step.noteOpen[chord] != 0u && step.sourcePads[chord] < 32u) {
                    starts[step.sourcePads[chord]] = static_cast<int>(stepIndex);
                }
            }
        }
        int position = 0;
        for (std::size_t pad = 0u; pad < starts.size(); ++pad) {
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                pad == 0u ? "%d" : ",%d", starts[pad]);
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
        if (!buffer || bufferLength < 1 || activePattern < 0
            || activePattern >= static_cast<int>(patterns.size())) return -1;
        const auto& cues = patterns[static_cast<std::size_t>(activePattern)].cues;
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
            case 54: sequenceBpm = midiRange(value, 40.0f, 240.0f); break;
            case 55: sequenceSwing = midiRange(value, 50.0f, 75.0f) / 100.0f; break;
            case 56: sequenceLength = clampValue(static_cast<int>(std::lround(
                midiRange(value, 1.0f, static_cast<float>(kSequenceStepCount)))),
                1, static_cast<int>(kSequenceStepCount)); break;
            case 57: setSequenceGate(midiRange(value, 5.0f, 800.0f)); break;
            case 58: sequencePage = midiChoice(
                value, static_cast<int>(kSequencePageCount)); break;
            case 59: if (value >= 64) restoreUndo(); break;
            case 60: setSequenceProbability(static_cast<int>(std::lround(
                midiRange(value, 0.0f, 100.0f)))); break;
            case 61: setSequenceRatchets(1 + midiChoice(value, 4)); break;
            case 62: setSequenceMicroTiming(static_cast<int>(std::lround(
                midiRange(value, -49.0f, 49.0f)))); break;
            case 63: automationRecord = value >= 64; break;
            case 64: if (value >= 64) commitPerformanceCapture(); break;
            case 65: chainEnabled = value >= 64; break;
            case 66: chainEditSlot = midiChoice(
                value, static_cast<int>(kPatternChainLength)); break;
            case 67: setChainEntry(chainEditSlot, midiChoice(
                value, static_cast<int>(kPatternCount)),
                patternChain[static_cast<std::size_t>(chainEditSlot)].repeats); break;
            case 68: setChainEntry(chainEditSlot,
                patternChain[static_cast<std::size_t>(chainEditSlot)].pattern,
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
            default: break;
        }
        if (cc >= 20 && cc <= 113 && cc != 52) markPersistentStateDirty();
        const int lockParameter = cc >= 20 && cc <= 45
            ? cc - 20 : (cc >= 69 && cc <= 110 ? cc - 43 : -1);
        if (automationRecord && sequenceRunning && sequenceCurrentStep >= 0
            && lockParameter >= 0
            && lockParameter < static_cast<int>(kLockParameterCount)) {
            auto& step = sequence[static_cast<std::size_t>(sequenceCurrentStep)];
            const std::size_t parameter = static_cast<std::size_t>(lockParameter);
            step.locks[parameter] = lockedValue(controls, outputGain, parameter);
            setStepLock(step, parameter);
            step.enabled = true;
            markPersistentStateDirty();
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
            waveDirtyMask |= 1u << lane;
            std::snprintf(laneSourceLabels[lane].data(),
                          laneSourceLabels[lane].size(), "SAVING TAKE");
            currentLaneSourceKinds[lane] = LaneSourceKind::capture;
            if (activePattern >= 0
                && activePattern < static_cast<int>(patterns.size())) {
                Pattern& pattern = patterns[static_cast<std::size_t>(activePattern)];
                std::array<char, kPersistencePathSize> leaf {};
                std::snprintf(leaf.data(), leaf.size(),
                              "captures/p%02d-lane-%u.wav", activePattern + 1,
                              static_cast<unsigned>(lane + 1u));
                persistencePath(leaf.data(), pattern.laneSourcePaths[lane].data(),
                                pattern.laneSourcePaths[lane].size());
                std::snprintf(currentLaneSourcePaths[lane].data(),
                              currentLaneSourcePaths[lane].size(), "%s",
                              pattern.laneSourcePaths[lane].data());
                pattern.laneSourceKinds[lane] = LaneSourceKind::capture;
                std::snprintf(pattern.laneSourceLabels[lane].data(),
                              pattern.laneSourceLabels[lane].size(), "SAVING TAKE");
                pattern.occupied = true;
                pendingCapturePatterns[lane] = activePattern;
                pendingCaptureSaveMask |= 1u << lane;
                markPersistentStateDirty();
            }
        }
    }

    void startRecording()
    {
        if (engine.recording()) return;
        const auto prepareState = static_cast<PatternPrepareState>(
            patternPrepareState.load(std::memory_order_acquire));
        if (prepareState == PatternPrepareState::requested
            || prepareState == PatternPrepareState::loading) {
            setError("Wait for the queued pattern to finish preparing");
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
        if (patternPreparationInFlight()) {
            setError("Wait for the queued pattern to finish preparing");
            return;
        }
        pushPatternUndo(activePattern);
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
        if (activePattern >= 0
            && activePattern < static_cast<int>(patterns.size())) {
            Pattern& pattern = patterns[static_cast<std::size_t>(activePattern)];
            pattern.laneSourcePaths[selectedLane][0] = '\0';
            pattern.laneSourceKinds[selectedLane] = LaneSourceKind::empty;
            std::snprintf(pattern.laneSourceLabels[selectedLane].data(),
                          pattern.laneSourceLabels[selectedLane].size(), "EMPTY");
        }
        markPersistentStateDirty();
        waveMinimum[selectedLane].fill(0);
        waveMaximum[selectedLane].fill(0);
        rebuildWaveText();
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
        if (patternPreparationInFlight()) {
            setError("Wait for the queued pattern to finish preparing");
            return false;
        }
        if (pendingCaptureSaveMask != 0u && !serviceMaintenance()) {
            setError("Save the current take before editing");
            return false;
        }
        clearError();
        pushPatternUndo(activePattern);
        allNotesOff();
        return true;
    }

    void finishSampleEdit(const char* label)
    {
        if (activePattern < 0
            || activePattern >= static_cast<int>(patterns.size())) return;
        Pattern& pattern = patterns[static_cast<std::size_t>(activePattern)];
        const char* currentPath =
            pattern.laneSourcePaths[selectedLane].data();
        const char alternate = std::strstr(currentPath, "-edit-a.wav")
            ? 'b' : 'a';
        std::array<char, kPersistencePathSize> leaf {};
        std::snprintf(leaf.data(), leaf.size(),
                      "captures/p%02d-lane-%u-edit-%c.wav",
                      activePattern + 1,
                      static_cast<unsigned>(selectedLane + 1u), alternate);
        persistencePath(leaf.data(), pattern.laneSourcePaths[selectedLane].data(),
                        pattern.laneSourcePaths[selectedLane].size());
        pattern.laneSourceKinds[selectedLane] = LaneSourceKind::capture;
        std::snprintf(pattern.laneSourceLabels[selectedLane].data(),
                      pattern.laneSourceLabels[selectedLane].size(), "%s",
                      label ? label : "EDIT");
        pattern.occupied = true;
        currentLaneSourceKinds[selectedLane] = LaneSourceKind::capture;
        std::snprintf(currentLaneSourcePaths[selectedLane].data(),
                      currentLaneSourcePaths[selectedLane].size(), "%s",
                      pattern.laneSourcePaths[selectedLane].data());
        std::snprintf(laneSourceLabels[selectedLane].data(),
                      laneSourceLabels[selectedLane].size(), "%s",
                      pattern.laneSourceLabels[selectedLane].data());
        pendingCapturePatterns[selectedLane] = activePattern;
        pendingCaptureSaveMask |= 1u << selectedLane;
        waveDirtyMask |= 1u << selectedLane;
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
    }

    void beginNextWaveScan()
    {
        if (scanLane >= 0 || waveDirtyMask == 0u) return;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if ((waveDirtyMask & (1u << lane)) == 0u) continue;
            scanLane = static_cast<int>(lane);
            scanFrame = 0u;
            scanFrames = engine.laneFrames(lane);
            scanMinimum.fill(1.0f);
            scanMaximum.fill(-1.0f);
            if (scanFrames == 0u || !engine.laneData(lane, 0u)) {
                waveMinimum[lane].fill(0);
                waveMaximum[lane].fill(0);
                waveDirtyMask &= ~(1u << lane);
                scanLane = -1;
                rebuildWaveText();
            }
            return;
        }
    }

    void serviceWaveScan()
    {
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

        for (std::size_t bin = 0u; bin < kWaveBins; ++bin) {
            const float low = scanMinimum[bin] > scanMaximum[bin] ? 0.0f : scanMinimum[bin];
            const float high = scanMinimum[bin] > scanMaximum[bin] ? 0.0f : scanMaximum[bin];
            waveMinimum[lane][bin] = static_cast<std::int8_t>(std::lrintf(
                clampValue(low, -1.0f, 1.0f) * 7.0f));
            waveMaximum[lane][bin] = static_cast<std::int8_t>(std::lrintf(
                clampValue(high, -1.0f, 1.0f) * 7.0f));
        }
        waveDirtyMask &= ~(1u << lane);
        scanLane = -1;
        rebuildWaveText();
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
            servicePatternSourceQueue();
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

            serviceSequenceBeforeBlock();
            serviceVoiceLaneSync();
            for (auto& voice : voices) {
                if (!voice.active) voice.engine.stopPlayback();
                const Engine::Controls effective = effectiveControls(voice);
                voice.engine.process(silentInputPointers.data(),
                                     voice.outputPointers.data(), count, effective);
                if (!voice.active) continue;
                const float voiceGain = voice.useSnapshot
                    ? voice.snapshotOutputGain : outputGain;
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
                    voice.sequencerOwned = false;
                    voice.useSnapshot = false;
                    voice.sequenceStepsRemaining = 0;
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
            serviceSequenceAfterBlock(count);
            rendered += static_cast<int>(count);
        }
        serviceWaveScan();
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
            "{\"version\":3,\"locus\":%.5f,\"field\":%.5f,\"speed\":%.5f,"
            "\"manual_lane\":%.5f,\"lane_cycles\":%.5f,\"algorithm\":%d,"
            "\"join\":%.5f,\"output_gain\":%.5f,\"record_gain\":%.5f,"
            "\"pressure_mode\":%d,\"root_note\":%d,\"pad_octave\":%d,"
            "\"record_lane\":%u,\"record_source\":%d,\"motion_mode\":%d,"
            "\"motion_sound\":%d,\"motion_rate\":%.5f,\"lanes_mode\":%d,"
            "\"lane_path\":%d,\"lanes_stretch\":%d,\"stretch_ratio\":%.5f,"
            "\"lane_1_gain\":%.5f,\"lane_2_gain\":%.5f,"
            "\"lane_3_gain\":%.5f,\"lane_4_gain\":%.5f,"
            "\"grain_mode\":%d,\"grain_size\":%.5f,"
            "\"grain_density\":%.5f,\"grain_pitch\":%.5f,"
            "\"grain_window\":%d,\"grain_spread\":%.5f,"
            "\"cutups_mode\":%d,\"seq_bpm\":%.5f,\"seq_swing\":%.5f,"
            "\"seq_length\":%d,\"seq_gate\":%.5f,\"seq_page\":%d,"
            "\"automation_record\":%d,\"chain_enabled\":%d,"
            "\"motion_travel\":%.5f,\"motion_jitter\":%.5f,"
            "\"packet_duty\":%.5f,\"motor_rate\":%.5f,"
            "\"motor_symmetry\":%.5f,\"motor_shape\":%d,"
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
            "\"pad_mode\":%d,\"lane_variation\":%d,"
            "\"preview_level\":%.5f}",
            controls.locus, controls.field, controls.speed,
            controls.manualLane + 1.0f, controls.laneCycles,
            static_cast<int>(controls.algorithm), controls.join,
            outputGain, recordGain, pressureMode, rootNote, padOctave,
            static_cast<unsigned>(selectedLane + 1u), recordSource,
            static_cast<int>(controls.motion), static_cast<int>(controls.motionSound),
            controls.motionSoundRateHz, static_cast<int>(controls.lanes),
            static_cast<int>(controls.lanePath), controls.lanesStretch ? 1 : 0,
            controls.lanesStretchRatio,
            controls.laneGains[0u], controls.laneGains[1u],
            controls.laneGains[2u], controls.laneGains[3u],
            static_cast<int>(controls.grains), controls.grainSizeMs,
            controls.grainDensityHz, controls.grainPitch,
            static_cast<int>(controls.grainWindow), controls.grainStereoSpread,
            static_cast<int>(controls.cutups), sequenceBpm,
            sequenceSwing * 100.0f, sequenceLength, sequenceGate * 100.0f,
            sequencePage + 1, automationRecord ? 1 : 0,
            chainEnabled ? 1 : 0,
            controls.motionTravel, controls.motionJitter, controls.packetDuty,
            controls.motorRateHz, controls.motorSymmetry,
            static_cast<int>(controls.motorShape),
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
            controls.sampleRegionCount, static_cast<int>(padMode),
            static_cast<int>(laneVariation), previewLevel);
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
        if (!wrote || !closed || std::rename(temporary.data(), path.data()) != 0) {
            std::remove(temporary.data());
            return false;
        }
        return true;
    }

    bool writePatternFile()
    {
        std::array<char, kPersistencePathSize> path {};
        std::array<char, kPersistencePathSize> temporary {};
        if (!persistencePath("thru-pattern.bin", path.data(), path.size())
            || !persistencePath("thru-pattern.bin.tmp", temporary.data(),
                                temporary.size())) {
            return false;
        }
        captureActivePattern(false);
        std::FILE* file = std::fopen(temporary.data(), "wb");
        if (!file) return false;
        const char magic[8] { 'S', '3', 'G', 'T', 'S', 'E', 'Q', '\0' };
        bool ok = writeBytes(file, magic, sizeof(magic))
            && writeU32(file, 9u)
            && writeF32(file, sequenceBpm)
            && writeU32(file, static_cast<std::uint32_t>(activePattern))
            && writeU32(file, static_cast<std::uint32_t>(chainPosition))
            && writeU32(file, static_cast<std::uint32_t>(chainEditSlot));
        for (const auto& entry : patternChain) {
            ok = ok && writeU32(file, entry.enabled ? 1u : 0u)
                && writeU32(file, entry.pattern)
                && writeU32(file, entry.repeats);
        }
        for (const auto& pattern : patterns) {
            ok = ok && writeU32(file, pattern.occupied ? 1u : 0u)
                && writeF32(file, pattern.swing)
                && writeF32(file, pattern.legacyGate)
                && writeU32(file, pattern.length);
            for (const float base : pattern.baseValues) {
                ok = ok && writeF32(file, base);
            }
            for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                ok = ok && writeU32(file, static_cast<std::uint32_t>(
                                      pattern.laneSourceKinds[lane]))
                    && writeBytes(file, pattern.laneSourcePaths[lane].data(),
                                      pattern.laneSourcePaths[lane].size())
                    && writeBytes(file, pattern.laneSourceLabels[lane].data(),
                                  pattern.laneSourceLabels[lane].size());
            }
            for (const float cue : pattern.cues) {
                ok = ok && writeF32(file, cue);
            }
            for (const auto& step : pattern.steps) {
                ok = ok && writeU32(file, step.enabled ? 1u : 0u)
                    && writeU32(file, step.noteCount)
                    && writeU32(file, step.lockMask)
                    && writeU32(file, step.lockMaskHigh)
                    && writeU32(file, step.lockMaskExtra)
                    && writeU32(file, step.gateLocked ? 1u : 0u)
                    && writeF32(file, step.gate)
                    && writeU32(file, step.probability)
                    && writeU32(file, step.ratchets)
                    && writeU32(file, static_cast<std::uint32_t>(
                        static_cast<std::int32_t>(step.microTiming)));
                for (std::size_t chord = 0u; chord < kSequenceChordSize; ++chord) {
                    ok = ok && writeU32(file, step.notes[chord])
                        && writeU32(file, step.velocities[chord])
                        && writeU32(file, step.sourcePads[chord])
                        && writeU32(file, step.lengthSteps[chord])
                        && writeU32(file, step.noteOpen[chord]);
                }
                for (const float lock : step.locks) {
                    ok = ok && writeF32(file, lock);
                }
            }
        }
        const bool closed = std::fclose(file) == 0;
        if (!ok || !closed || std::rename(temporary.data(), path.data()) != 0) {
            std::remove(temporary.data());
            return false;
        }
        return true;
    }

    bool readPatternFile()
    {
        std::array<char, kPersistencePathSize> path {};
        if (!persistencePath("thru-pattern.bin", path.data(), path.size())) {
            return false;
        }
        std::FILE* file = std::fopen(path.data(), "rb");
        if (!file) return false;
        char magic[8] {};
        std::uint32_t version = 0u;
        std::uint32_t length = 0u;
        float bpm = 120.0f;
        float swing = 0.5f;
        float gate = 0.8f;
        std::array<SequenceStep, kSequenceStepCount> loaded {};
        bool ok = std::fread(magic, 1u, sizeof(magic), file) == sizeof(magic)
            && std::memcmp(magic, "S3GTSEQ", 7u) == 0
            && readU32(file, version)
            && (version >= 1u && version <= 9u);
        if (ok && version >= 5u) {
            std::uint32_t selected = 0u;
            std::array<Pattern, kPatternCount> loadedPatterns {};
            ok = readF32(file, bpm) && readU32(file, selected)
                && selected < kPatternCount;
            if (version >= 7u) {
                std::uint32_t loadedChainPosition = 0u;
                std::uint32_t loadedChainEditSlot = 0u;
                ok = ok && readU32(file, loadedChainPosition)
                    && readU32(file, loadedChainEditSlot);
                chainPosition = clampValue(static_cast<int>(loadedChainPosition),
                    0, static_cast<int>(kPatternChainLength) - 1);
                chainEditSlot = clampValue(static_cast<int>(loadedChainEditSlot),
                    0, static_cast<int>(kPatternChainLength) - 1);
                for (auto& entry : patternChain) {
                    std::uint32_t enabled = 0u;
                    std::uint32_t pattern = 0u;
                    std::uint32_t repeats = 1u;
                    ok = ok && readU32(file, enabled)
                        && readU32(file, pattern) && readU32(file, repeats);
                    entry.enabled = enabled != 0u;
                    entry.pattern = static_cast<std::uint8_t>(clampValue(
                        pattern, 0u, static_cast<std::uint32_t>(kPatternCount - 1u)));
                    entry.repeats = static_cast<std::uint8_t>(clampValue(
                        repeats, 1u, 16u));
                }
            }
            for (auto& pattern : loadedPatterns) {
                for (std::size_t parameter = 0u;
                     parameter < kLockParameterCount; ++parameter) {
                    pattern.baseValues[parameter] = lockedValue(
                        controls, outputGain, parameter);
                }
                std::uint32_t occupied = 0u;
                std::uint32_t patternLength = 16u;
                ok = ok && readU32(file, occupied)
                    && readF32(file, pattern.swing)
                    && readF32(file, pattern.legacyGate)
                    && readU32(file, patternLength);
                pattern.occupied = occupied != 0u;
                pattern.swing = clampValue(pattern.swing, 0.5f, 0.75f);
                pattern.legacyGate = clampValue(pattern.legacyGate, 0.05f, 8.0f);
                pattern.length = static_cast<std::uint8_t>(
                    clampValue(patternLength, 1u,
                        version >= 7u
                            ? static_cast<std::uint32_t>(kSequenceStepCount)
                            : 16u));
                const std::size_t storedParameterCount = version >= 9u
                    ? kLockParameterCount : (version >= 8u
                        ? kPreviousLockParameterCount : kLegacyLockParameterCount);
                for (std::size_t parameter = 0u;
                     parameter < storedParameterCount; ++parameter) {
                    float& base = pattern.baseValues[parameter];
                    ok = ok && readF32(file, base) && std::isfinite(base);
                }
                if (version >= 6u) {
                    for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                        if (version >= 7u) {
                            std::uint32_t kind = 0u;
                            ok = ok && readU32(file, kind);
                            pattern.laneSourceKinds[lane] =
                                static_cast<LaneSourceKind>(clampValue(
                                    kind, 0u, 3u));
                        }
                        ok = ok && std::fread(
                            pattern.laneSourcePaths[lane].data(), 1u,
                            pattern.laneSourcePaths[lane].size(), file)
                                == pattern.laneSourcePaths[lane].size()
                            && std::fread(
                                pattern.laneSourceLabels[lane].data(), 1u,
                                pattern.laneSourceLabels[lane].size(), file)
                                == pattern.laneSourceLabels[lane].size();
                        pattern.laneSourcePaths[lane].back() = '\0';
                        pattern.laneSourceLabels[lane].back() = '\0';
                        if (version == 6u) {
                            pattern.laneSourceKinds[lane] =
                                pattern.laneSourcePaths[lane][0] != '\0'
                                    ? LaneSourceKind::file
                                    : LaneSourceKind::inherit;
                        }
                    }
                }
                if (version >= 9u) {
                    for (float& cue : pattern.cues) {
                        ok = ok && readF32(file, cue);
                        cue = clampValue(cue, -1.0f, 1.0f);
                    }
                }
                const std::size_t storedStepCount = version >= 7u
                    ? kSequenceStepCount : kVisibleStepCount;
                for (std::size_t stepIndex = 0u;
                     stepIndex < storedStepCount; ++stepIndex) {
                    auto& step = pattern.steps[stepIndex];
                    std::uint32_t enabled = 0u;
                    std::uint32_t noteCount = 0u;
                    std::uint32_t lockMask = 0u;
                    std::uint32_t lockMaskHigh = 0u;
                    std::uint32_t lockMaskExtra = 0u;
                    std::uint32_t gateLocked = 0u;
                    ok = ok && readU32(file, enabled)
                        && readU32(file, noteCount)
                        && readU32(file, lockMask);
                    if (version >= 8u) {
                        ok = ok && readU32(file, lockMaskHigh);
                    }
                    if (version >= 9u) {
                        ok = ok && readU32(file, lockMaskExtra);
                    }
                    ok = ok
                        && readU32(file, gateLocked)
                        && readF32(file, step.gate);
                    step.enabled = enabled != 0u;
                    step.noteCount = static_cast<std::uint8_t>(clampValue(
                        noteCount, 0u,
                        static_cast<std::uint32_t>(kSequenceChordSize)));
                    step.lockMask = lockMask;
                    step.lockMaskHigh = lockMaskHigh;
                    step.lockMaskExtra = lockMaskExtra;
                    step.gateLocked = gateLocked != 0u;
                    step.gate = clampValue(step.gate, 0.05f, 8.0f);
                    if (version >= 7u) {
                        std::uint32_t probability = 100u;
                        std::uint32_t ratchets = 1u;
                        std::uint32_t microTiming = 0u;
                        ok = ok && readU32(file, probability)
                            && readU32(file, ratchets)
                            && readU32(file, microTiming);
                        step.probability = static_cast<std::uint8_t>(clampValue(
                            probability, 0u, 100u));
                        step.ratchets = static_cast<std::uint8_t>(clampValue(
                            ratchets, 1u, 4u));
                        step.microTiming = static_cast<std::int8_t>(clampValue(
                            static_cast<std::int32_t>(microTiming), -49, 49));
                    }
                    for (std::size_t chord = 0u;
                         chord < kSequenceChordSize; ++chord) {
                        std::uint32_t note = 0u;
                        std::uint32_t velocity = 1u;
                        std::uint32_t sourcePad = 255u;
                        std::uint32_t lengthSteps = 1u;
                        std::uint32_t noteOpen = 0u;
                        ok = ok && readU32(file, note)
                            && readU32(file, velocity)
                            && readU32(file, sourcePad)
                            && readU32(file, lengthSteps)
                            && readU32(file, noteOpen);
                        step.notes[chord] = static_cast<std::uint8_t>(
                            clampValue(note, 0u, 127u));
                        step.velocities[chord] = static_cast<std::uint8_t>(
                            clampValue(velocity, 1u, 127u));
                        step.sourcePads[chord] = static_cast<std::uint8_t>(
                            sourcePad < 32u ? sourcePad : 255u);
                        step.lengthSteps[chord] = static_cast<std::uint8_t>(
                            clampValue(lengthSteps, 1u,
                                version >= 7u
                                    ? static_cast<std::uint32_t>(kSequenceStepCount)
                                    : 16u));
                        step.noteOpen[chord] = noteOpen != 0u ? 1u : 0u;
                    }
                    for (std::size_t parameter = 0u;
                         parameter < storedParameterCount; ++parameter) {
                        float& lock = step.locks[parameter];
                        ok = ok && readF32(file, lock) && std::isfinite(lock);
                    }
                }
            }
            const bool atEnd = ok && std::fgetc(file) == EOF;
            std::fclose(file);
            if (!atEnd) return false;
            patterns = loadedPatterns;
            sequenceBpm = clampValue(bpm, 40.0f, 240.0f);
            activePattern = static_cast<int>(selected);
            patternBankInitialized = true;
            activateStoredPattern(activePattern);
            return true;
        }
        ok = ok && version >= 1u && version <= 4u
            && readF32(file, bpm) && readF32(file, swing)
            && readF32(file, gate) && readU32(file, length);
        for (std::size_t stepIndex = 0u; stepIndex < kVisibleStepCount;
             ++stepIndex) {
            auto& step = loaded[stepIndex];
            std::uint32_t enabled = 0u;
            std::uint32_t lockMask = 0u;
            ok = ok && readU32(file, enabled);
            if (version == 1u) {
                std::uint32_t note = 60u;
                std::uint32_t velocity = 100u;
                ok = ok && readU32(file, note) && readU32(file, velocity)
                    && readU32(file, lockMask);
                step.noteCount = 1u;
                step.notes[0u] = static_cast<std::uint8_t>(
                    clampValue(note, 0u, 127u));
                step.velocities[0u] = static_cast<std::uint8_t>(
                    clampValue(velocity, 1u, 127u));
            } else {
                std::uint32_t noteCount = 0u;
                ok = ok && readU32(file, noteCount) && readU32(file, lockMask);
                step.noteCount = static_cast<std::uint8_t>(
                    clampValue(noteCount, 0u,
                               static_cast<std::uint32_t>(kSequenceChordSize)));
                if (version >= 3u) {
                    std::uint32_t gateLocked = 0u;
                    float stepGate = 0.8f;
                    ok = ok && readU32(file, gateLocked)
                        && readF32(file, stepGate);
                    step.gateLocked = gateLocked != 0u;
                    step.gate = clampValue(stepGate, 0.05f, 8.0f);
                }
                for (std::size_t chord = 0u; chord < kSequenceChordSize; ++chord) {
                    std::uint32_t note = 0u;
                    std::uint32_t velocity = 1u;
                    ok = ok && readU32(file, note) && readU32(file, velocity);
                    step.notes[chord] = static_cast<std::uint8_t>(
                        clampValue(note, 0u, 127u));
                    step.velocities[chord] = static_cast<std::uint8_t>(
                        clampValue(velocity, 1u, 127u));
                    if (version >= 4u) {
                        std::uint32_t sourcePad = 255u;
                        std::uint32_t lengthSteps = 1u;
                        ok = ok && readU32(file, sourcePad)
                            && readU32(file, lengthSteps);
                        step.sourcePads[chord] = static_cast<std::uint8_t>(
                            sourcePad < 32u ? sourcePad : 255u);
                        step.lengthSteps[chord] = static_cast<std::uint8_t>(
                            clampValue(lengthSteps, 1u, 16u));
                    }
                }
            }
            if (version < 4u) {
                const float effectiveGate = step.gateLocked ? step.gate : gate;
                const std::uint8_t migratedLength = static_cast<std::uint8_t>(
                    clampValue(static_cast<int>(std::ceil(effectiveGate)), 1, 16));
                for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
                    step.sourcePads[chord] = 255u;
                    step.lengthSteps[chord] = migratedLength;
                }
            }
            step.enabled = enabled != 0u;
            step.lockMask = lockMask;
            for (std::size_t parameter = 0u;
                 parameter < kLegacyLockParameterCount; ++parameter) {
                ok = ok && readF32(file, step.locks[parameter]);
            }
        }
        const bool atEnd = ok && std::fgetc(file) == EOF;
        std::fclose(file);
        if (!atEnd) return false;
        sequence = loaded;
        sequenceBpm = clampValue(bpm, 40.0f, 240.0f);
        sequenceSwing = clampValue(swing, 0.5f, 0.75f);
        sequenceGate = clampValue(gate, 0.05f, 8.0f);
        sequenceLength = clampValue(static_cast<int>(length), 1, 16);
        activePattern = 0;
        activePatternOccupied = workingPatternHasContent();
        captureActivePattern(activePatternOccupied);
        return true;
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

    bool writePatternCapture(std::size_t lane, int patternIndex)
    {
        if (lane >= kLaneCount || patternIndex < 0
            || patternIndex >= static_cast<int>(kPatternCount)) return false;
        Pattern& pattern = patterns[static_cast<std::size_t>(patternIndex)];
        std::array<char, kPersistencePathSize> path {};
        std::array<char, kPersistencePathSize> temporary {};
        if (pattern.laneSourceKinds[lane] == LaneSourceKind::capture
            && pattern.laneSourcePaths[lane][0] != '\0') {
            std::snprintf(path.data(), path.size(), "%s",
                          pattern.laneSourcePaths[lane].data());
        } else {
            std::array<char, kPersistencePathSize> leaf {};
            std::snprintf(leaf.data(), leaf.size(),
                          "captures/p%02d-lane-%u.wav", patternIndex + 1,
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
        pattern.laneSourceKinds[lane] = LaneSourceKind::capture;
        std::snprintf(pattern.laneSourcePaths[lane].data(),
                      pattern.laneSourcePaths[lane].size(), "%s", path.data());
        const bool edited = std::strcmp(pattern.laneSourceLabels[lane].data(),
                                        "SAVING TAKE") != 0;
        if (!edited) {
            std::snprintf(pattern.laneSourceLabels[lane].data(),
                          pattern.laneSourceLabels[lane].size(), "TAKE P%02d",
                          patternIndex + 1);
        }
        if (patternIndex == activePattern) {
            currentLaneSourceKinds[lane] = LaneSourceKind::capture;
            std::snprintf(currentLaneSourcePaths[lane].data(),
                          currentLaneSourcePaths[lane].size(), "%s", path.data());
            std::snprintf(laneSourceLabels[lane].data(),
                          laneSourceLabels[lane].size(), "%s",
                          pattern.laneSourceLabels[lane].data());
        }
        return true;
    }

    bool serviceMaintenance()
    {
        if (engine.recording()) return false;
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
            const int owner = pendingCapturePatterns[lane];
            if (!writePatternCapture(lane, owner)) {
                ok = false;
                continue;
            }
            pendingCaptureSaveMask &= ~(1u << lane);
            pendingCapturePatterns[lane] = -1;
        }
        if (persistentStateDirty || pendingCaptureSaveMask != 0u) {
            captureActivePattern(false);
            const bool metadataSaved = writeStateFile() && writePatternFile();
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
        waveDirtyMask |= 1u << lane;
        std::snprintf(laneSourceLabels[lane].data(), laneSourceLabels[lane].size(),
                      "SAVED");
        return true;
    }

    void reconcileRestoredPatternSources()
    {
        if (activePattern < 0
            || activePattern >= static_cast<int>(patterns.size())) return;
        const Pattern& pattern = patterns[static_cast<std::size_t>(activePattern)];
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if (!engine.laneLoaded(lane)
                || pattern.laneSourcePaths[lane][0] == '\0') continue;
            std::snprintf(currentLaneSourcePaths[lane].data(),
                          currentLaneSourcePaths[lane].size(), "%s",
                          pattern.laneSourcePaths[lane].data());
            if (pattern.laneSourceLabels[lane][0] != '\0') {
                std::snprintf(laneSourceLabels[lane].data(),
                              laneSourceLabels[lane].size(), "%s",
                              pattern.laneSourceLabels[lane].data());
            }
        }
        queuePatternSources(activePattern);
    }

    void loadPersistentState()
    {
        if (!persistenceEnabled) return;
        std::array<char, kPersistencePathSize> path {};
        if (persistencePath("thru-state.json", path.data(), path.size())) {
            std::FILE* file = std::fopen(path.data(), "rb");
            if (file) {
                std::array<char, kStateTextSize> text {};
                const std::size_t length = std::fread(text.data(), 1u, text.size() - 1u, file);
                const bool complete = std::feof(file) != 0;
                std::fclose(file);
                if (complete && length != 0u) {
                    text[length] = '\0';
                    restoreState(text.data());
                    persistenceRestored = true;
                }
            }
        }
        if (readPatternFile()) persistenceRestored = true;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if (readLaneFile(lane)) persistenceRestored = true;
        }
        reconcileRestoredPatternSources();
    }

    bool savePersistentState()
    {
        if (!persistenceEnabled) return false;
        if (engine.recording()) finishRecording();
        bool ok = true;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            if ((pendingCaptureSaveMask & (1u << lane)) == 0u) continue;
            const int owner = pendingCapturePatterns[lane];
            const bool captureSaved = writePatternCapture(lane, owner);
            ok = captureSaved && ok;
            if (captureSaved) {
                pendingCaptureSaveMask &= ~(1u << lane);
                pendingCapturePatterns[lane] = -1;
            }
        }
        ok = writeStateFile() && ok;
        ok = writePatternFile() && ok;
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            ok = writeLaneFile(lane) && ok;
        }
        if (!ok && g_host && g_host->log) {
            g_host->log("S3G Thru could not save all persistent state");
        }
        return ok;
    }

    void restoreState(const char* json)
    {
        float value = 0.0f;
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
        if (jsonNumber(json, "preview_level", value)) previewLevel = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "output_gain", value)) outputGain = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "record_gain", value)) recordGain = clampValue(value, 0.0f, 4.0f);
        if (jsonNumber(json, "pressure_mode", value)) pressureMode = clampValue(static_cast<int>(value), 0, 1);
        if (jsonNumber(json, "root_note", value)) rootNote = clampValue(static_cast<int>(value), 24, 96);
        if (jsonNumber(json, "pad_octave", value)) padOctave = clampValue(static_cast<int>(value), -3, 3);
        if (jsonNumber(json, "record_lane", value) && !engine.recording()) selectedLane = static_cast<std::size_t>(clampValue(static_cast<int>(value), 1, 4) - 1);
        if (jsonNumber(json, "record_source", value) && !engine.recording()) recordSource = clampValue(static_cast<int>(value), 0, 1);
        if (jsonNumber(json, "motion_mode", value)) controls.motion = static_cast<Engine::MotionMode>(clampValue(static_cast<int>(value), 0, 7));
        if (jsonNumber(json, "motion_sound", value)) controls.motionSound = static_cast<Engine::MotionSound>(clampValue(static_cast<int>(value), 0, 3));
        if (jsonNumber(json, "motion_rate", value)) controls.motionSoundRateHz = clampValue(value, 0.25f, 20.0f);
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
        if (jsonNumber(json, "cutups_mode", value)) controls.cutups = static_cast<Engine::CutupsMode>(clampValue(static_cast<int>(value), 0, 4));
        if (jsonNumber(json, "motion_travel", value)) controls.motionTravel = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "motion_jitter", value)) controls.motionJitter = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "packet_duty", value)) controls.packetDuty = clampValue(value, 0.02f, 0.98f);
        if (jsonNumber(json, "motor_rate", value)) controls.motorRateHz = clampValue(value, 0.05f, 20.0f);
        if (jsonNumber(json, "motor_symmetry", value)) controls.motorSymmetry = clampValue(value, 0.05f, 0.95f);
        if (jsonNumber(json, "motor_shape", value)) controls.motorShape = static_cast<Engine::MotorShape>(clampValue(static_cast<int>(value), 0, 2));
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
        if (jsonNumber(json, "grain_process", value)) controls.grainProcess = static_cast<Engine::GrainProcess>(clampValue(static_cast<int>(value), 0, 3));
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
        if (jsonNumber(json, "seq_bpm", value)) sequenceBpm = clampValue(value, 40.0f, 240.0f);
        if (jsonNumber(json, "seq_swing", value)) sequenceSwing = clampValue(value / 100.0f, 0.5f, 0.75f);
        if (jsonNumber(json, "seq_length", value)) sequenceLength = clampValue(
            static_cast<int>(value), 1, static_cast<int>(kSequenceStepCount));
        if (jsonNumber(json, "seq_page", value)) sequencePage = clampValue(
            static_cast<int>(value) - 1, 0,
            static_cast<int>(kSequencePageCount) - 1);
        if (jsonNumber(json, "automation_record", value)) {
            automationRecord = value >= 0.5f;
        }
        if (jsonNumber(json, "chain_enabled", value)) chainEnabled = value >= 0.5f;
        if (jsonNumber(json, "seq_gate", value)) setSequenceGate(value);
    }
};

void* createInstance(const char* moduleDir, const char*)
{
    std::unique_ptr<ThruInstance> instance(new (std::nothrow) ThruInstance());
    if (!instance) {
        if (g_host && g_host->log) g_host->log("S3G Thru instance allocation failed");
        return nullptr;
    }
    if (!instance->prepare(moduleDir)) {
        if (g_host && g_host->log) g_host->log(instance->lastError.data());
        return nullptr;
    }
    if (g_host && g_host->log) g_host->log("S3G Thru instance created");
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
        && std::strcmp(key, "state") != 0) {
        instance->markPersistentStateDirty();
    }

    if (std::strcmp(key, "state") == 0) { instance->restoreState(value); return; }
    if (std::strcmp(key, "locus") == 0) { controls.locus = clampValue(parseFloat(value, controls.locus), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "field") == 0) { controls.field = clampValue(parseFloat(value, controls.field), 0.02f, 1.0f); return; }
    if (std::strcmp(key, "speed") == 0) { controls.speed = clampValue(parseFloat(value, controls.speed), 0.05f, 4.0f); return; }
    if (std::strcmp(key, "manual_lane") == 0) { controls.manualLane = clampValue(parseFloat(value, controls.manualLane + 1.0f) - 1.0f, 0.0f, 3.0f); return; }
    if (std::strcmp(key, "lane_cycles") == 0) { controls.laneCycles = clampValue(parseFloat(value, controls.laneCycles), 0.125f, 16.0f); return; }
    if (std::strcmp(key, "algorithm") == 0) { controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(parseEnum(value, kAlgorithmNames, static_cast<int>(controls.algorithm))); return; }
    if (std::strcmp(key, "sample_mode") == 0) { controls.sampleMode = static_cast<Engine::SampleMode>(parseEnum(value, kSampleModeNames, static_cast<int>(controls.sampleMode))); return; }
    if (std::strcmp(key, "slice_region_mode") == 0) { controls.sampleRegionMode = static_cast<Engine::RegionMode>(parseEnum(value, kRegionModeNames, static_cast<int>(controls.sampleRegionMode))); return; }
    if (std::strcmp(key, "slice_regions") == 0) { controls.sampleRegionCount = clampValue(parseFloat(value, controls.sampleRegionCount), 1.0f, 64.0f); return; }
    if (std::strcmp(key, "pad_mode") == 0) { instance->padMode = static_cast<PadMode>(parseEnum(value, kPadModeNames, static_cast<int>(instance->padMode))); instance->allNotesOff(); return; }
    if (std::strcmp(key, "lane_variation") == 0) { instance->laneVariation = static_cast<LaneVariation>(parseEnum(value, kLaneVariationNames, static_cast<int>(instance->laneVariation))); return; }
    if (std::strcmp(key, "preview_level") == 0) { instance->previewLevel = clampValue(parseFloat(value, instance->previewLevel), 0.0f, 1.0f); return; }
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
    if (std::strcmp(key, "seq_running") == 0) { instance->setSequenceRunning(isTruthy(value)); return; }
    if (std::strcmp(key, "seq_bpm") == 0) { instance->sequenceBpm = clampValue(parseFloat(value, instance->sequenceBpm), 40.0f, 240.0f); return; }
    if (std::strcmp(key, "seq_swing") == 0) { instance->sequenceSwing = clampValue(parseFloat(value, instance->sequenceSwing * 100.0f) / 100.0f, 0.5f, 0.75f); return; }
    if (std::strcmp(key, "seq_length") == 0) { instance->sequenceLength = clampValue(parseInteger(value, instance->sequenceLength), 1, static_cast<int>(kSequenceStepCount)); instance->markPersistentStateDirty(); return; }
    if (std::strcmp(key, "seq_gate") == 0) { instance->setSequenceGate(parseFloat(value, instance->sequenceGate * 100.0f)); return; }
    if (std::strcmp(key, "seq_page") == 0) { instance->sequencePage = clampValue(parseInteger(value, instance->sequencePage + 1) - 1, 0, static_cast<int>(kSequencePageCount) - 1); return; }
    if (std::strcmp(key, "seq_edit_step") == 0) { instance->sequenceEditStep = clampValue(parseInteger(value, instance->sequenceEditStep), 0, static_cast<int>(kSequenceStepCount) - 1); return; }
    if (std::strcmp(key, "seq_probability") == 0) { instance->setSequenceProbability(parseInteger(value, 100)); return; }
    if (std::strcmp(key, "seq_ratchets") == 0) { instance->setSequenceRatchets(parseInteger(value, 1)); return; }
    if (std::strcmp(key, "seq_micro") == 0) { instance->setSequenceMicroTiming(parseInteger(value, 0)); return; }
    if (std::strcmp(key, "automation_record") == 0) { instance->automationRecord = isTruthy(value); instance->markPersistentStateDirty(); return; }
    if (std::strcmp(key, "seq_capture") == 0) { if (isTruthy(value)) instance->commitPerformanceCapture(); return; }
    if (std::strcmp(key, "undo") == 0) { if (isTruthy(value)) instance->restoreUndo(); return; }
    if (std::strcmp(key, "maintenance") == 0) { if (isTruthy(value)) instance->serviceMaintenance(); return; }
    if (std::strcmp(key, "performance_reset") == 0) { if (isTruthy(value)) { instance->maximumRenderMicros.store(0u, std::memory_order_relaxed); instance->renderOverruns.store(0u, std::memory_order_relaxed); } return; }
    if (std::strcmp(key, "chain_enabled") == 0) { instance->chainEnabled = isTruthy(value); instance->markPersistentStateDirty(); return; }
    if (std::strcmp(key, "chain_slot") == 0) { instance->chainEditSlot = clampValue(parseInteger(value, instance->chainEditSlot + 1) - 1, 0, static_cast<int>(kPatternChainLength) - 1); return; }
    if (std::strcmp(key, "chain_pattern") == 0) { const auto& entry = instance->patternChain[static_cast<std::size_t>(instance->chainEditSlot)]; instance->setChainEntry(instance->chainEditSlot, clampValue(parseInteger(value, static_cast<int>(entry.pattern) + 1) - 1, 0, static_cast<int>(kPatternCount) - 1), entry.repeats); return; }
    if (std::strcmp(key, "chain_repeats") == 0) { const auto& entry = instance->patternChain[static_cast<std::size_t>(instance->chainEditSlot)]; instance->setChainEntry(instance->chainEditSlot, entry.pattern, parseInteger(value, entry.repeats)); return; }
    if (std::strcmp(key, "chain_clear") == 0) { if (isTruthy(value)) instance->clearChainEntry(instance->chainEditSlot); return; }
    if (std::strcmp(key, "seq_toggle_step") == 0) { instance->toggleSequenceStep(parseInteger(value, -1)); return; }
    if (std::strcmp(key, "seq_note") == 0) {
        int step = -1;
        int note = instance->rootNote;
        int velocity = 100;
        if (std::sscanf(value, "%d,%d,%d", &step, &note, &velocity) >= 2) {
            instance->setSequenceNote(step, note, velocity);
        }
        return;
    }
    if (std::strcmp(key, "seq_note_toggle") == 0) {
        int step = -1;
        int note = instance->rootNote;
        int velocity = 100;
        if (std::sscanf(value, "%d,%d,%d", &step, &note, &velocity) >= 2) {
            instance->toggleSequenceNote(step, note, velocity);
        }
        return;
    }
    if (std::strcmp(key, "seq_note_pad_toggle") == 0) {
        int step = -1;
        int pad = -1;
        int note = instance->rootNote;
        int velocity = 100;
        if (std::sscanf(value, "%d,%d,%d,%d", &step, &pad, &note, &velocity)
            >= 3) {
            instance->toggleSequencePadNote(step, pad, note, velocity);
        }
        return;
    }
    if (std::strcmp(key, "seq_note_length") == 0) {
        int step = -1;
        int pad = -1;
        int length = 1;
        if (std::sscanf(value, "%d,%d,%d", &step, &pad, &length) == 3) {
            instance->setSequenceNoteLength(step, pad, length);
        }
        return;
    }
    if (std::strcmp(key, "seq_step_gate") == 0) {
        int step = -1;
        float gate = instance->sequenceGate * 100.0f;
        if (std::sscanf(value, "%d,%f", &step, &gate) == 2
            && std::isfinite(gate)) {
            instance->setSequenceStepGate(step, gate);
        }
        return;
    }
    if (std::strcmp(key, "seq_lock") == 0) {
        int step = -1;
        float lockValue = 0.0f;
        char lockKey[32] {};
        if (std::sscanf(value, "%d,%31[^,],%f", &step, lockKey, &lockValue) == 3
            && std::isfinite(lockValue)) {
            instance->setSequenceLock(step, lockKey, lockValue);
        }
        return;
    }
    if (std::strcmp(key, "seq_clear_locks") == 0) { instance->clearSequenceLocks(parseInteger(value, -1)); return; }
    if (std::strcmp(key, "seq_clear_step") == 0) { instance->clearSequenceStep(parseInteger(value, -1)); return; }
    if (std::strcmp(key, "seq_clear") == 0) { if (isTruthy(value)) instance->clearSequence(); return; }
    if (std::strcmp(key, "pattern_select") == 0) {
        instance->selectPattern(parseInteger(value, -1));
        return;
    }
    if (std::strcmp(key, "pattern_copy") == 0) {
        instance->copyActivePattern(parseInteger(value, -1));
        return;
    }
    if (std::strcmp(key, "pattern_clear") == 0) {
        instance->clearPattern(parseInteger(value, -1));
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
    if (std::strcmp(key, "motion_travel") == 0) { controls.motionTravel = clampValue(parseFloat(value, controls.motionTravel), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "motion_jitter") == 0) { controls.motionJitter = clampValue(parseFloat(value, controls.motionJitter), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "packet_duty") == 0) { controls.packetDuty = clampValue(parseFloat(value, controls.packetDuty * 100.0f) / 100.0f, 0.02f, 0.98f); return; }
    if (std::strcmp(key, "motor_rate") == 0) { controls.motorRateHz = clampValue(parseFloat(value, controls.motorRateHz), 0.05f, 20.0f); return; }
    if (std::strcmp(key, "motor_symmetry") == 0) { controls.motorSymmetry = clampValue(parseFloat(value, controls.motorSymmetry * 100.0f) / 100.0f, 0.05f, 0.95f); return; }
    if (std::strcmp(key, "motor_shape") == 0) { controls.motorShape = static_cast<Engine::MotorShape>(parseEnum(value, kMotorShapeNames, static_cast<int>(controls.motorShape))); return; }
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
}

int getParam(void* opaque, const char* key, char* buffer, int bufferLength)
{
    auto* instance = static_cast<ThruInstance*>(opaque);
    if (!instance || !key || !buffer || bufferLength < 1) return -1;
    const auto& controls = instance->controls;

    if (std::strcmp(key, "chain_params") == 0) return copyText(kChainParams, buffer, bufferLength);
    if (std::strcmp(key, "ui_hierarchy") == 0) return copyText(kUiHierarchy, buffer, bufferLength);
    if (std::strcmp(key, "preset_name") == 0) { buffer[0] = '\0'; return 0; }
    if (std::strcmp(key, "name") == 0) return std::snprintf(buffer, bufferLength, "S3G Thru");
    if (std::strcmp(key, "polyphony") == 0) return std::snprintf(
        buffer, bufferLength, "%u", static_cast<unsigned>(kVoiceCount));
    if (std::strcmp(key, "surface") == 0) { buffer[0] = '\0'; return 0; }
    if (std::strcmp(key, "midi_map") == 0) return copyText(kMidiMap, buffer, bufferLength);
    if (std::strcmp(key, "waveforms") == 0) return copyText(instance->waveText.data(), buffer, bufferLength);
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
    if (std::strcmp(key, "seq_state") == 0) return std::snprintf(
        buffer, bufferLength, "%d,%d,%u,%.2f,%d,%.2f,%.2f,%d",
        instance->sequenceRunning ? 1 : 0, instance->sequenceCurrentStep,
        instance->sequenceMask(), instance->sequenceBpm,
        instance->sequenceLength, instance->sequenceSwing * 100.0f,
        instance->sequenceGate * 100.0f, instance->sequencePage + 1);
    if (std::strcmp(key, "seq_locks") == 0) {
        return instance->formatSequenceLocks(buffer, bufferLength);
    }
    if (std::strcmp(key, "seq_locks_hi") == 0) {
        return instance->formatSequenceLocksHigh(buffer, bufferLength);
    }
    if (std::strcmp(key, "seq_locks_x") == 0) {
        return instance->formatSequenceLocksExtra(buffer, bufferLength);
    }
    if (std::strcmp(key, "seq_note_counts") == 0) {
        return instance->formatSequenceNoteCounts(buffer, bufferLength);
    }
    if (std::strcmp(key, "seq_pad_masks") == 0) {
        return instance->formatSequencePadMasks(buffer, bufferLength);
    }
    if (std::strcmp(key, "seq_open_mask") == 0) return std::snprintf(
        buffer, bufferLength, "%u", instance->sequenceOpenPadMask());
    if (std::strcmp(key, "seq_open_steps") == 0) {
        return instance->formatSequenceOpenSteps(buffer, bufferLength);
    }
    if (std::strcmp(key, "chain_state") == 0) return std::snprintf(
        buffer, bufferLength, "%d,%d,%d,%d",
        instance->chainEnabled ? 1 : 0, instance->chainPosition,
        instance->chainRepeat, instance->chainEditSlot);
    if (std::strcmp(key, "chain_entries") == 0) {
        return instance->formatPatternChain(buffer, bufferLength);
    }
    if (std::strcmp(key, "undo_available") == 0) return std::snprintf(
        buffer, bufferLength, "%d", instance->undoAvailable ? 1 : 0);
    if (std::strcmp(key, "pattern_state") == 0) {
        const std::uint64_t mask = instance->occupiedPatternMask();
        return std::snprintf(buffer, bufferLength, "%d,%d,%u,%u",
            instance->activePattern, instance->queuedPattern,
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
    if (std::strcmp(key, "pattern_prepare_state") == 0) return std::snprintf(
        buffer, bufferLength, "%d,%d",
        instance->patternPrepareState.load(std::memory_order_acquire),
        instance->patternPrepareRequest.load(std::memory_order_relaxed));
    if (std::strcmp(key, "lane_source") == 0) return copyText(
        instance->laneSourceLabels[instance->selectedLane].data(),
        buffer, bufferLength);
    if (std::strcmp(key, "last_error") == 0) return copyText(
        instance->lastError.data(), buffer, bufferLength);
    if (std::strcmp(key, "lane_source_state") == 0) {
        const char* kinds[] { "INHERIT", "FILE", "CAPTURE", "EMPTY" };
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
        if (instance->activePattern >= 0
            && instance->activePattern < static_cast<int>(kPatternCount)) {
            ownership = instance->patterns[static_cast<std::size_t>(
                instance->activePattern)].laneSourceKinds[
                    instance->selectedLane];
        }
        const auto kind = static_cast<std::size_t>(ownership);
        return std::snprintf(buffer, bufferLength, "%s:%s",
            loading ? "LOADING" : (missing ? "MISSING"
                : (kind < 4u ? kinds[kind] : "MISSING")),
            instance->laneSourceLabels[instance->selectedLane].data());
    }

    if (std::strcmp(key, "locus") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.locus);
    if (std::strcmp(key, "field") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.field);
    if (std::strcmp(key, "speed") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.speed);
    if (std::strcmp(key, "manual_lane") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.manualLane + 1.0f);
    if (std::strcmp(key, "lane_cycles") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.laneCycles);
    if (std::strcmp(key, "algorithm") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.algorithm));
    if (std::strcmp(key, "sample_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.sampleMode));
    if (std::strcmp(key, "slice_region_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.sampleRegionMode));
    if (std::strcmp(key, "slice_regions") == 0) return std::snprintf(buffer, bufferLength, "%.0f", controls.sampleRegionCount);
    if (std::strcmp(key, "pad_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->padMode));
    if (std::strcmp(key, "lane_variation") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->laneVariation));
    if (std::strcmp(key, "preview_level") == 0) return std::snprintf(buffer, bufferLength, "%.4f", instance->previewLevel);
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
    if (std::strcmp(key, "seq_running") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->sequenceRunning ? 1 : 0);
    if (std::strcmp(key, "seq_bpm") == 0) return std::snprintf(buffer, bufferLength, "%.2f", instance->sequenceBpm);
    if (std::strcmp(key, "seq_swing") == 0) return std::snprintf(buffer, bufferLength, "%.2f", instance->sequenceSwing * 100.0f);
    if (std::strcmp(key, "seq_length") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->sequenceLength);
    if (std::strcmp(key, "seq_gate") == 0) return std::snprintf(buffer, bufferLength, "%.2f", instance->sequenceGate * 100.0f);
    if (std::strcmp(key, "seq_page") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->sequencePage + 1);
    if (std::strcmp(key, "seq_probability") == 0) return std::snprintf(buffer, bufferLength, "%u", static_cast<unsigned>(instance->sequence[static_cast<std::size_t>(clampValue(instance->sequenceEditStep, 0, static_cast<int>(kSequenceStepCount) - 1))].probability));
    if (std::strcmp(key, "seq_ratchets") == 0) return std::snprintf(buffer, bufferLength, "%u", static_cast<unsigned>(instance->sequence[static_cast<std::size_t>(clampValue(instance->sequenceEditStep, 0, static_cast<int>(kSequenceStepCount) - 1))].ratchets));
    if (std::strcmp(key, "seq_micro") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->sequence[static_cast<std::size_t>(clampValue(instance->sequenceEditStep, 0, static_cast<int>(kSequenceStepCount) - 1))].microTiming));
    if (std::strcmp(key, "automation_record") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->automationRecord ? 1 : 0);
    if (std::strcmp(key, "chain_enabled") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->chainEnabled ? 1 : 0);
    if (std::strcmp(key, "chain_slot") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->chainEditSlot + 1);
    if (std::strcmp(key, "chain_pattern") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->patternChain[static_cast<std::size_t>(instance->chainEditSlot)].pattern) + 1);
    if (std::strcmp(key, "chain_repeats") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(instance->patternChain[static_cast<std::size_t>(instance->chainEditSlot)].repeats));

    if (std::strcmp(key, "motion_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.motion));
    if (std::strcmp(key, "motion_sound") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.motionSound));
    if (std::strcmp(key, "motion_rate") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motionSoundRateHz);
    if (std::strcmp(key, "motion_travel") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motionTravel);
    if (std::strcmp(key, "motion_jitter") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motionJitter);
    if (std::strcmp(key, "packet_duty") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.packetDuty * 100.0f);
    if (std::strcmp(key, "motor_rate") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motorRateHz);
    if (std::strcmp(key, "motor_symmetry") == 0) return std::snprintf(buffer, bufferLength, "%.2f", controls.motorSymmetry * 100.0f);
    if (std::strcmp(key, "motor_shape") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.motorShape));
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

