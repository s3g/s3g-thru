#include "sample_motion_app.h"
#include "thru_schwung_abi.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
constexpr std::size_t kSequenceStepCount = 16u;
constexpr std::size_t kSequenceChordSize = kVoiceCount;
constexpr std::size_t kPatternBankCount = 4u;
constexpr std::size_t kPatternsPerBank = 16u;
constexpr std::size_t kPatternCount = kPatternBankCount * kPatternsPerBank;
constexpr int kFirstPadNote = 68;
constexpr int kLastPadNote = 99;
constexpr int kPadColumns = 8;
constexpr int kPadRowInterval = 5;
constexpr int kPadBaseNote = 48;
constexpr std::size_t kPersistencePathSize = 768u;
constexpr std::size_t kStateTextSize = 2048u;
constexpr std::size_t kWavIoFrames = 512u;
constexpr std::size_t kLibraryCopyFrames = 4096u;
constexpr const char* kSampleLibraryRoot = "/data/UserData/UserLibrary/Samples/";

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
    count
};

constexpr std::size_t kLockParameterCount =
    static_cast<std::size_t>(LockParameter::count);
static_assert(kLockParameterCount <= 32u, "parameter locks use a 32-bit mask");
constexpr std::uint32_t kSequenceGateLockMask = 0x80000000u;

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
    float gate = 0.8f;
    std::uint8_t noteCount = 0u;
    bool enabled = false;
    bool gateLocked = false;
};

struct Pattern {
    std::array<SequenceStep, kSequenceStepCount> steps {};
    std::array<float, kLockParameterCount> baseValues {};
    std::array<std::array<char, kPersistencePathSize>, kLaneCount>
        laneSourcePaths {};
    std::array<std::array<char, 64u>, kLaneCount> laneSourceLabels {};
    float swing = 0.5f;
    float legacyGate = 0.8f;
    std::uint8_t length = 16u;
    bool occupied = false;
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
};

const host_api_v1_t* g_host = nullptr;

constexpr const char* kAlgorithmNames[] {
    "Motion", "Lanes", "Grains", "Cutups"
};
constexpr const char* kMotionModeNames[] {
    "Forward", "Reverse", "Hover", "Round Trip", "Moving Loop", "Mirror", "Zigzag"
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
    "Down", "Up", "Triangle", "Sine", "Steps Down", "Steps Up", "Manual"
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
    "55 seq_swing,56 seq_length,57 seq_gate";

constexpr const char kChainParams[] = R"json([
{"key":"locus","name":"Position","short_name":"POS","type":"float","min":0,"max":1,"step":0.01,"default":0.5},
{"key":"field","name":"Field","short_name":"FIELD","type":"float","min":0.02,"max":1,"step":0.01,"default":1},
{"key":"speed","name":"Speed","short_name":"SPD","type":"float","min":0.05,"max":4,"step":0.01,"default":1},
{"key":"manual_lane","name":"Stack Position","short_name":"STACK","type":"float","min":1,"max":4,"step":0.01,"default":1},
{"key":"lane_cycles","name":"Stack Rate","short_name":"SCAN","type":"float","min":0.125,"max":16,"step":0.125,"default":1,"unit":"Hz"},
{"key":"algorithm","name":"Algorithm","short_name":"ALG","type":"enum","options":["Motion","Lanes","Grains","Cutups"],"default":0},
{"key":"join","name":"Join","short_name":"JOIN","type":"float","min":0,"max":1,"step":0.01,"default":1},
{"key":"output_gain","name":"Level","short_name":"LEVEL","type":"float","min":0,"max":2,"step":0.01,"default":1},
{"key":"pressure_mode","name":"Pressure","short_name":"PRESS","type":"enum","options":["Off","Offset"],"default":1},
{"key":"root_note","name":"Root Note","short_name":"ROOT","type":"note","mode":"multi","min_note":24,"max_note":96,"default":60},
{"key":"record_lane","name":"Record Lane","short_name":"LANE","type":"int","min":1,"max":4,"default":1},
{"key":"record_source","name":"Record Source","short_name":"SOURCE","type":"enum","options":["Input","Resample"],"default":0},
{"key":"record_gain","name":"Record Gain","short_name":"REC LVL","type":"float","min":0,"max":4,"step":0.01,"default":1},
{"key":"monitor_level","name":"Monitor","short_name":"MON","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"clear_lane","name":"Clear Lane","short_name":"CLEAR","type":"enum","options":["Keep","Clear"],"default":0},
{"key":"motion_mode","name":"Motion Mode","short_name":"MODE","type":"enum","options":["Forward","Reverse","Hover","Round Trip","Moving Loop","Mirror","Zigzag"],"default":0},
{"key":"motion_sound","name":"Motion Sound","short_name":"SOUND","type":"enum","options":["Continuous","Packets","Motor","Bounce"],"default":0},
{"key":"motion_rate","name":"Gesture Rate","short_name":"GEST","type":"float","min":0.25,"max":20,"step":0.25,"default":4,"unit":"Hz"},
{"key":"lanes_mode","name":"Lanes Mode","short_name":"MODE","type":"enum","options":["Forward","Reverse","Ping Pong"],"default":0},
{"key":"lane_path","name":"Stack Path","short_name":"PATH","type":"enum","options":["Down","Up","Triangle","Sine","Steps Down","Steps Up","Manual"],"default":2},
{"key":"lanes_stretch","name":"Stretch","short_name":"STRCH","type":"enum","options":["Off","On"],"default":1},
{"key":"stretch_ratio","name":"Stretch Ratio","short_name":"RATIO","type":"float","min":0.25,"max":4,"step":0.01,"default":1},
{"key":"lane_1_gain","name":"Lane 1","short_name":"L1","type":"float","min":0,"max":2,"step":0.01,"default":1},
{"key":"lane_2_gain","name":"Lane 2","short_name":"L2","type":"float","min":0,"max":2,"step":0.01,"default":1},
{"key":"lane_3_gain","name":"Lane 3","short_name":"L3","type":"float","min":0,"max":2,"step":0.01,"default":1},
{"key":"lane_4_gain","name":"Lane 4","short_name":"L4","type":"float","min":0,"max":2,"step":0.01,"default":1},
{"key":"grain_mode","name":"Grain Mode","short_name":"MODE","type":"enum","options":["Scan","Freeze","Cloud","Slice"],"default":0},
{"key":"grain_size","name":"Grain Size","short_name":"SIZE","type":"float","min":8,"max":500,"step":1,"default":90,"unit":"ms"},
{"key":"grain_density","name":"Density","short_name":"DENS","type":"float","min":1,"max":64,"step":1,"default":24,"unit":"Hz"},
{"key":"grain_pitch","name":"Grain Pitch","short_name":"PITCH","type":"float","min":0.25,"max":4,"step":0.01,"default":1},
{"key":"grain_window","name":"Window","short_name":"WIN","type":"enum","options":["Parzen","Sine","Hann","Triangle","Gaussian","Bell"],"default":5},
{"key":"grain_spread","name":"Stereo Spread","short_name":"SPREAD","type":"float","min":0,"max":1,"step":0.01,"default":0},
{"key":"cutups_mode","name":"Cutups Mode","short_name":"MODE","type":"enum","options":["Forward","Reverse","Palindrome","Random","Walk"],"default":0},
{"key":"seq_bpm","name":"Pattern Tempo","short_name":"BPM","type":"float","min":40,"max":240,"step":1,"default":120,"unit":"BPM"},
{"key":"seq_swing","name":"Pattern Swing","short_name":"SWING","type":"float","min":50,"max":75,"step":1,"default":50,"unit":"%"},
{"key":"seq_length","name":"Pattern Length","short_name":"LEN","type":"int","min":1,"max":16,"default":16},
{"key":"seq_gate","name":"Pattern Gate","short_name":"GATE","type":"float","min":5,"max":800,"step":5,"default":80,"unit":"%"},
{"key":"seq_state","name":"Sequencer State","type":"string","access":"read","live":true},
{"key":"seq_locks","name":"Step Lock Masks","type":"string","access":"read","live":true},
{"key":"seq_note_counts","name":"Step Note Counts","type":"string","access":"read","live":true},
{"key":"seq_pad_masks","name":"Step Pad Masks","type":"string","access":"read","live":true},
{"key":"seq_open_mask","name":"Open Note Pads","type":"string","access":"read","live":true},
{"key":"seq_clear_step","name":"Clear Sequence Step","type":"int","min":0,"max":15,"access":"write"},
{"key":"pattern_state","name":"Pattern Bank State","type":"string","access":"read","live":true},
{"key":"sample_load_state","name":"Sample Load State","type":"string","access":"read","live":true},
{"key":"lane_source","name":"Lane Source","type":"string","access":"read","live":true},
{"key":"recording","name":"Recording","type":"int","min":0,"max":1,"access":"read","live":true},
{"key":"ui_state","name":"UI State","type":"string","access":"read","live":true},
{"key":"playheads","name":"Voice Playheads","type":"string","access":"read","live":true},
{"key":"voice_lanes","name":"Voice Stack Positions","type":"string","access":"read","live":true},
{"key":"voice_pads","name":"Voice Source Pads","type":"string","access":"read","live":true},
{"key":"midi_map","name":"MIDI CC Map","type":"string","access":"read"},
{"key":"waveforms","name":"Waveforms","type":"string","access":"read"}
])json";

constexpr const char kUiHierarchy[] = R"json({
"pad_layout":"chromatic",
"levels":{
"root":{"name":"S3G Thru","knobs":["algorithm","locus","field","speed","manual_lane","lane_cycles","join","output_gain"],"params":[{"level":"record","label":"Record"},{"level":"motion","label":"Motion"},{"level":"lanes","label":"Lanes"},{"level":"grains","label":"Grains"},{"level":"cutups","label":"Cutups"},{"level":"lane_mix","label":"Lane Mix"},{"level":"sequence","label":"Sequence"}]},
"record":{"name":"Record","knobs":["record_lane","record_source","record_gain","monitor_level","pressure_mode","root_note","output_gain","clear_lane"],"params":[{"key":"record_lane"},{"key":"record_source"},{"key":"record_gain"},{"key":"monitor_level"},{"key":"pressure_mode"},{"key":"root_note"},{"key":"output_gain"},{"key":"clear_lane"}]},
"motion":{"name":"Motion","knobs":["locus","field","speed","join","motion_mode","motion_sound","motion_rate","output_gain"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"motion_mode"},{"key":"motion_sound"},{"key":"motion_rate"},{"key":"output_gain"}]},
"lanes":{"name":"Lanes","knobs":["locus","field","speed","join","lanes_mode","lane_path","lane_cycles","manual_lane"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"lanes_mode"},{"key":"lane_path"},{"key":"lane_cycles"},{"key":"manual_lane"},{"key":"lanes_stretch"},{"key":"stretch_ratio"}]},
"grains":{"name":"Grains","knobs":["locus","field","speed","join","grain_mode","grain_size","grain_density","grain_pitch"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"grain_mode"},{"key":"grain_size"},{"key":"grain_density"},{"key":"grain_pitch"},{"key":"grain_window"},{"key":"grain_spread"},{"key":"lane_path"},{"key":"lane_cycles"},{"key":"manual_lane"}]},
"cutups":{"name":"Cutups","knobs":["locus","field","speed","join","cutups_mode","lane_path","lane_cycles","manual_lane"],"params":[{"key":"locus"},{"key":"field"},{"key":"speed"},{"key":"join"},{"key":"cutups_mode"},{"key":"lane_path"},{"key":"lane_cycles"},{"key":"manual_lane"}]},
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
        "grain_pitch", "grain_window", "grain_spread", "cutups_mode"
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
        case LockParameter::algorithm: controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(clampValue(static_cast<int>(std::lround(value)), 0, 3)); break;
        case LockParameter::join: controls.join = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::outputGain: outputGain = clampValue(value, 0.0f, 2.0f); break;
        case LockParameter::motionMode: controls.motion = static_cast<Engine::MotionMode>(clampValue(static_cast<int>(std::lround(value)), 0, 6)); break;
        case LockParameter::motionSound: controls.motionSound = static_cast<Engine::MotionSound>(clampValue(static_cast<int>(std::lround(value)), 0, 3)); break;
        case LockParameter::motionRate: controls.motionSoundRateHz = clampValue(value, 0.25f, 20.0f); break;
        case LockParameter::lanesMode: controls.lanes = static_cast<Engine::LanesMode>(clampValue(static_cast<int>(std::lround(value)), 0, 2)); break;
        case LockParameter::lanePath: controls.lanePath = static_cast<Engine::LanePath>(clampValue(static_cast<int>(std::lround(value)), 0, 6)); break;
        case LockParameter::lanesStretch: controls.lanesStretch = value >= 0.5f; break;
        case LockParameter::stretchRatio: controls.lanesStretchRatio = clampValue(value, 0.25f, 4.0f); break;
        case LockParameter::lane1Gain: controls.laneGains[0u] = clampValue(value, 0.0f, 2.0f); break;
        case LockParameter::lane2Gain: controls.laneGains[1u] = clampValue(value, 0.0f, 2.0f); break;
        case LockParameter::lane3Gain: controls.laneGains[2u] = clampValue(value, 0.0f, 2.0f); break;
        case LockParameter::lane4Gain: controls.laneGains[3u] = clampValue(value, 0.0f, 2.0f); break;
        case LockParameter::grainMode: controls.grains = static_cast<Engine::GrainMode>(clampValue(static_cast<int>(std::lround(value)), 0, 3)); break;
        case LockParameter::grainSize: controls.grainSizeMs = clampValue(value, 8.0f, 500.0f); break;
        case LockParameter::grainDensity: controls.grainDensityHz = clampValue(value, 1.0f, 64.0f); break;
        case LockParameter::grainPitch: controls.grainPitch = clampValue(value, 0.25f, 4.0f); break;
        case LockParameter::grainWindow: controls.grainWindow = static_cast<Engine::GrainWindow>(clampValue(static_cast<int>(std::lround(value)), 0, 5)); break;
        case LockParameter::grainSpread: controls.grainStereoSpread = clampValue(value, 0.0f, 1.0f); break;
        case LockParameter::cutupsMode: controls.cutups = static_cast<Engine::CutupsMode>(clampValue(static_cast<int>(std::lround(value)), 0, 4)); break;
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
    float performanceLane = 0.0f;
    float attackCoefficient = 1.0f;
    float releaseCoefficient = 1.0f;

    std::size_t selectedLane = 0u;
    int recordSource = 0;
    float recordGain = 1.0f;
    float monitorLevel = 0.0f;
    float outputGain = 1.0f;

    std::array<SequenceStep, kSequenceStepCount> sequence {};
    std::array<Pattern, kPatternCount> patterns {};
    double sequenceSamplesUntilStep = 0.0;
    float sequenceBpm = 120.0f;
    float sequenceSwing = 0.5f;
    float sequenceGate = 0.8f;
    int sequenceLength = 16;
    int sequenceCurrentStep = -1;
    bool sequenceRunning = false;
    int activePattern = 0;
    int queuedPattern = -1;
    bool activePatternOccupied = false;
    bool patternBankInitialized = false;

    std::unique_ptr<float[]> sampleStage;
    pthread_t sampleWorker {};
    pthread_mutex_t sampleWorkerMutex {};
    pthread_cond_t sampleWorkerCondition {};
    std::atomic<int> sampleWorkerRunning { 0 };
    std::atomic<int> sampleLoadState {
        static_cast<int>(SampleLoadState::idle)
    };
    std::array<char, kPersistencePathSize> sampleRequestPath {};
    std::array<char, 96u> sampleLoadedName {};
    std::array<char, 128u> sampleLoadError {};
    int sampleRequestLane = 0;
    int sampleRequestPattern = 0;
    bool sampleRequestFromPattern = false;
    int sampleApplyLane = 0;
    std::size_t sampleStageFrames = 0u;
    float sampleStageRate = static_cast<float>(MOVE_SAMPLE_RATE);
    std::size_t sampleCopyFrame = 0u;
    bool sampleRemovalRequested = false;
    bool sampleWorkerReady = false;
    bool sampleWorkerSignaled = false;
    std::array<std::array<char, 64u>, kLaneCount> laneSourceLabels {};
    std::array<std::array<char, kPersistencePathSize>, kLaneCount>
        currentLaneSourcePaths {};
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
        sampleStage.reset(new (std::nothrow) float[planeCapacityFrames] {});
        if (!storage || !sampleStage) {
            setError("Unable to allocate the four-lane sample bank");
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

    bool loadLibraryWav(const char* requestedPath)
    {
        if (!requestedPath || requestedPath[0] == '\0') {
            setSampleLoadError("No sample was selected");
            return false;
        }
        std::array<char, PATH_MAX> resolved {};
        const bool resolvedPath = realpath(requestedPath, resolved.data()) != nullptr;
        const bool inUserLibrary = resolvedPath
            && std::strncmp(resolved.data(), kSampleLibraryRoot,
                            std::strlen(kSampleLibraryRoot)) == 0;
        std::array<char, PATH_MAX> localRoot {};
        bool inModuleLibrary = false;
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
        }
        if (!resolvedPath || (!inUserLibrary && !inModuleLibrary)) {
            setSampleLoadError("Sample must be inside UserLibrary/Samples");
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
        float* left = sampleStage.get();
        float* right = sampleStage.get() + laneCapacityFrames;
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
        sampleStageFrames = outputFrames;
        sampleStageRate = sampleRate;
        const char* leaf = std::strrchr(resolved.data(), '/');
        std::snprintf(sampleLoadedName.data(), sampleLoadedName.size(), "%.*s",
                      static_cast<int>(sampleLoadedName.size() - 1u),
                      leaf ? leaf + 1 : resolved.data());
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
                != static_cast<int>(SampleLoadState::requested)) continue;
            instance->sampleLoadState.store(
                static_cast<int>(SampleLoadState::loading), std::memory_order_release);
            if (instance->loadLibraryWav(instance->sampleRequestPath.data())) {
                instance->sampleLoadState.store(
                    static_cast<int>(SampleLoadState::ready),
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
                              bool fromPattern = false, int ownerPattern = -1)
    {
        if (!sampleWorkerReady || lane < 0 || lane >= static_cast<int>(kLaneCount)
            || !path || std::strlen(path) >= sampleRequestPath.size()) {
            setSampleLoadError("Sample loader is unavailable");
            return;
        }
        const int current = sampleLoadState.load(std::memory_order_acquire);
        if (current == static_cast<int>(SampleLoadState::requested)
            || current == static_cast<int>(SampleLoadState::loading)
            || current == static_cast<int>(SampleLoadState::ready)
            || current == static_cast<int>(SampleLoadState::removing)
            || current == static_cast<int>(SampleLoadState::copying)) {
            setError("Wait for the current sample load");
            return;
        }
        pthread_mutex_lock(&sampleWorkerMutex);
        sampleRequestLane = lane;
        sampleRequestPattern = ownerPattern >= 0 ? ownerPattern : activePattern;
        sampleRequestFromPattern = fromPattern;
        std::snprintf(sampleRequestPath.data(), sampleRequestPath.size(), "%s", path);
        sampleLoadError[0] = '\0';
        sampleLoadState.store(static_cast<int>(SampleLoadState::requested),
                              std::memory_order_release);
        sampleWorkerSignaled = true;
        pthread_cond_signal(&sampleWorkerCondition);
        pthread_mutex_unlock(&sampleWorkerMutex);
    }

    void queuePatternSources(int patternIndex)
    {
        pendingPatternSourceMask = 0u;
        if (patternIndex < 0
            || patternIndex >= static_cast<int>(patterns.size())) return;
        const Pattern& pattern = patterns[static_cast<std::size_t>(patternIndex)];
        for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
            const char* path = pattern.laneSourcePaths[lane].data();
            if (path[0] == '\0'
                || std::strcmp(path, currentLaneSourcePaths[lane].data()) == 0) {
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
            std::snprintf(pattern.laneSourcePaths[lane].data(),
                          pattern.laneSourcePaths[lane].size(), "%s",
                          sampleRequestPath.data());
            std::snprintf(pattern.laneSourceLabels[lane].data(),
                          pattern.laneSourceLabels[lane].size(), "%.*s",
                          static_cast<int>(pattern.laneSourceLabels[lane].size() - 1u),
                          sampleLoadedName.data());
            pattern.occupied = true;
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

    void noteOn(int note, int velocity, int sourcePad = -1)
    {
        note = clampValue(note, 0, 127);
        velocity = clampValue(velocity, 1, 127);
        ++noteClock;
        PlaybackVoice& voice = allocateVoice(note, sourcePad);
        voice.note = note;
        voice.sourcePad = sourcePad;
        voice.velocity = velocity;
        voice.age = noteClock;
        voice.laneOffset = midiUnit(velocity);
        voice.target = 1.0f;
        voice.active = true;
        voice.held = true;
        voice.sequencerOwned = false;
        voice.useSnapshot = false;
        voice.sequenceStepsRemaining = 0;
        performanceLane = voice.laneOffset * 3.0f;
        voice.engine.retrigger();
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
        const int row = index / kPadColumns;
        const int column = index % kPadColumns;
        return clampValue(kPadBaseNote + padOctave * 12
            + row * kPadRowInterval + column, 0, 127);
    }

    void padNoteOn(int hardwareNote, int velocity)
    {
        const int index = hardwareNote - kFirstPadNote;
        if (index < 0 || index >= static_cast<int>(padMappedNotes.size())) return;
        const int mappedNote = padToMidiNote(hardwareNote);
        padMappedNotes[static_cast<std::size_t>(index)] = mappedNote;
        noteOn(mappedNote, velocity, index);
    }

    void padNoteOff(int hardwareNote)
    {
        const int index = hardwareNote - kFirstPadNote;
        if (index < 0 || index >= static_cast<int>(padMappedNotes.size())) return;
        int mappedNote = padMappedNotes[static_cast<std::size_t>(index)];
        if (mappedNote < 0) mappedNote = padToMidiNote(hardwareNote);
        padMappedNotes[static_cast<std::size_t>(index)] = -1;
        noteOff(mappedNote, index);
    }

    Engine::Controls effectiveControls(const PlaybackVoice& voice) const
    {
        Engine::Controls effective = voice.useSnapshot
            ? voice.snapshotControls : controls;
        const float noteRatio = voice.note >= 0
            ? std::pow(2.0f, static_cast<float>(voice.note - rootNote) / 12.0f)
            : 1.0f;
        if (effective.algorithm == Engine::PlaybackAlgorithm::grains) {
            effective.grainPitch = clampValue(
                effective.grainPitch * noteRatio, 0.25f, 4.0f);
        } else {
            effective.speed = clampValue(
                effective.speed * noteRatio, 0.05f, 4.0f);
        }
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
        voice.age = noteClock;
        voice.laneOffset = midiUnit(voice.velocity);
        voice.snapshotControls = controls;
        voice.snapshotOutputGain = outputGain;
        for (std::size_t index = 0u; index < kLockParameterCount; ++index) {
            if ((step.lockMask & (1u << index)) != 0u) {
                applyLockedValue(voice.snapshotControls, voice.snapshotOutputGain,
                                 index, step.locks[index]);
            }
        }
        voice.target = 1.0f;
        voice.active = true;
        voice.held = true;
        voice.sequencerOwned = true;
        voice.useSnapshot = true;
        voice.sequenceStepsRemaining = clampValue(lengthSteps, 1, 16);
        voice.engine.retrigger();
        updateActiveNote();
    }

    int legacySequenceLength(const SequenceStep& step) const
    {
        const float gate = step.gateLocked ? step.gate : sequenceGate;
        return clampValue(static_cast<int>(std::ceil(gate)), 1, 16);
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
        return base * ((step & 1) == 0
            ? 2.0 * swingFraction : 2.0 * (1.0 - swingFraction));
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
        if (patternBoundary && queuedPattern >= 0) {
            const int nextPattern = queuedPattern;
            loadPattern(nextPattern);
            sequenceCurrentStep = -1;
        }
        sequenceCurrentStep = (sequenceCurrentStep + 1) % sequenceLength;
        const SequenceStep& step = sequence[static_cast<std::size_t>(sequenceCurrentStep)];
        const double interval = sequenceStepSamples(sequenceCurrentStep);
        sequenceSamplesUntilStep += interval;
        if (step.enabled) triggerSequenceStep(step);
    }

    void serviceSequenceBeforeBlock()
    {
        if (!sequenceRunning) return;
        while (sequenceSamplesUntilStep <= 0.0) advanceSequence();
    }

    void serviceSequenceAfterBlock(std::size_t frames)
    {
        if (!sequenceRunning) return;
        sequenceSamplesUntilStep -= static_cast<double>(frames);
    }

    void setSequenceRunning(bool running)
    {
        if (running == sequenceRunning) return;
        sequenceRunning = running;
        sequenceCurrentStep = -1;
        sequenceSamplesUntilStep = 0.0;
        if (!running) releaseSequencerVoices();
    }

    bool workingPatternHasContent() const
    {
        for (const auto& step : sequence) {
            if (step.enabled || step.noteCount != 0u || step.lockMask != 0u) {
                return true;
            }
        }
        return false;
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
        pattern.length = static_cast<std::uint8_t>(clampValue(sequenceLength, 1, 16));
        for (std::size_t index = 0u; index < kLockParameterCount; ++index) {
            pattern.baseValues[index] = lockedValue(controls, outputGain, index);
        }
        pattern.occupied = forceOccupied || activePatternOccupied
            || workingPatternHasContent();
        activePatternOccupied = pattern.occupied;
        patternBankInitialized = true;
    }

    void activateStoredPattern(int index)
    {
        if (index < 0 || index >= static_cast<int>(patterns.size())) return;
        Pattern& target = patterns[static_cast<std::size_t>(index)];
        activePattern = index;
        activePatternOccupied = target.occupied;
        sequence = target.steps;
        sequenceSwing = clampValue(target.swing, 0.5f, 0.75f);
        sequenceGate = clampValue(target.legacyGate, 0.05f, 8.0f);
        sequenceLength = clampValue(static_cast<int>(target.length), 1, 16);
        for (std::size_t parameter = 0u;
             parameter < kLockParameterCount; ++parameter) {
            applyLockedValue(controls, outputGain, parameter,
                             target.baseValues[parameter]);
        }
        queuePatternSources(index);
        queuedPattern = -1;
        if (!sequenceRunning) {
            sequenceCurrentStep = -1;
            sequenceSamplesUntilStep = 0.0;
        }
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
    }

    void selectPattern(int index)
    {
        if (index < 0 || index >= static_cast<int>(patterns.size())) return;
        if (sequenceRunning) {
            queuedPattern = index == activePattern ? -1 : index;
        } else {
            loadPattern(index);
        }
    }

    void copyActivePattern(int destination)
    {
        if (destination < 0
            || destination >= static_cast<int>(patterns.size())) return;
        captureActivePattern(true);
        patterns[static_cast<std::size_t>(destination)] =
            patterns[static_cast<std::size_t>(activePattern)];
        patterns[static_cast<std::size_t>(destination)].occupied = true;
    }

    void clearPattern(int index)
    {
        if (index < 0 || index >= static_cast<int>(patterns.size())) return;
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
    }

    std::uint64_t occupiedPatternMask()
    {
        captureActivePattern(false);
        std::uint64_t mask = 0u;
        for (std::size_t index = 0u; index < patterns.size(); ++index) {
            if (patterns[index].occupied) mask |= std::uint64_t { 1u } << index;
        }
        return mask;
    }

    std::uint32_t sequenceMask() const
    {
        std::uint32_t mask = 0u;
        for (std::size_t index = 0u; index < sequence.size(); ++index) {
            if (sequence[index].enabled) mask |= 1u << index;
        }
        return mask;
    }

    void toggleSequenceStep(int index)
    {
        if (index < 0 || index >= static_cast<int>(sequence.size())) return;
        auto& step = sequence[static_cast<std::size_t>(index)];
        step.enabled = !step.enabled;
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
            step.enabled = step.noteCount > 0u || step.lockMask != 0u;
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
    }

    void setSequenceNoteLength(int index, int pad, int length)
    {
        if (index < 0 || index >= static_cast<int>(sequence.size())
            || pad < 0 || pad >= 32) return;
        auto& step = sequence[static_cast<std::size_t>(index)];
        for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
            if (step.sourcePads[chord] == static_cast<std::uint8_t>(pad)) {
                step.lengthSteps[chord] = static_cast<std::uint8_t>(
                    clampValue(length, 1, 16));
                step.noteOpen[chord] = 0u;
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
        step.lockMask |= 1u << static_cast<unsigned>(lockIndex);
        step.enabled = true;
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
        step.gateLocked = false;
    }

    void clearSequenceStep(int stepIndex)
    {
        if (stepIndex < 0 || stepIndex >= static_cast<int>(sequence.size())) return;
        sequence[static_cast<std::size_t>(stepIndex)] = SequenceStep {};
    }

    void clearSequence()
    {
        setSequenceRunning(false);
        for (auto& step : sequence) {
            step = SequenceStep {};
        }
    }

    int formatSequenceLocks(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        for (std::size_t index = 0u; index < sequence.size(); ++index) {
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                index == 0u ? "%u" : ",%u", sequence[index].lockMask
                    | (sequence[index].gateLocked ? kSequenceGateLockMask : 0u));
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    int formatSequenceNoteCounts(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        for (std::size_t index = 0u; index < sequence.size(); ++index) {
            const int written = std::snprintf(
                buffer + position, static_cast<std::size_t>(bufferLength - position),
                index == 0u ? "%u" : ",%u",
                static_cast<unsigned>(sequence[index].noteCount));
            if (written < 0 || written >= bufferLength - position) return -1;
            position += written;
        }
        return position;
    }

    int formatSequencePadMasks(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        int position = 0;
        for (std::size_t index = 0u; index < sequence.size(); ++index) {
            std::uint32_t mask = 0u;
            const auto& step = sequence[index];
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
        for (const auto& step : sequence) {
            for (std::size_t chord = 0u; chord < step.noteCount; ++chord) {
                if (step.noteOpen[chord] != 0u && step.sourcePads[chord] < 32u) {
                    mask |= 1u << step.sourcePads[chord];
                }
            }
        }
        return mask;
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
            case 40: controls.grainSizeMs = midiRange(value, 8.0f, 500.0f); break;
            case 41: controls.grainDensityHz = midiRange(value, 1.0f, 64.0f); break;
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
                midiRange(value, 1.0f, 16.0f))), 1, 16); break;
            case 57: setSequenceGate(midiRange(value, 5.0f, 800.0f)); break;
            default: break;
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
        }
    }

    void completedRecordingLane(std::size_t lane)
    {
        recordingLaneMask = 0u;
        if (lane < kLaneCount && engine.laneFrames(lane) != 0u) {
            pendingClearMask &= ~(1u << lane);
            waveDirtyMask |= 1u << lane;
            std::snprintf(laneSourceLabels[lane].data(),
                          laneSourceLabels[lane].size(), "LIVE");
            currentLaneSourcePaths[lane][0] = '\0';
            if (activePattern >= 0
                && activePattern < static_cast<int>(patterns.size())) {
                Pattern& pattern = patterns[static_cast<std::size_t>(activePattern)];
                pattern.laneSourcePaths[lane][0] = '\0';
                std::snprintf(pattern.laneSourceLabels[lane].data(),
                              pattern.laneSourceLabels[lane].size(), "LIVE");
                pattern.occupied = true;
            }
        }
    }

    void startRecording()
    {
        if (engine.recording()) return;
        clearError();
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
        if (!engine.beginLaneRemoval(selectedLane)) {
            setError("Lane is busy and could not be cleared");
            return;
        }
        pendingClearMask |= 1u << selectedLane;
        std::snprintf(laneSourceLabels[selectedLane].data(),
                      laneSourceLabels[selectedLane].size(), "EMPTY");
        currentLaneSourcePaths[selectedLane][0] = '\0';
        if (activePattern >= 0
            && activePattern < static_cast<int>(patterns.size())) {
            Pattern& pattern = patterns[static_cast<std::size_t>(activePattern)];
            pattern.laneSourcePaths[selectedLane][0] = '\0';
            std::snprintf(pattern.laneSourceLabels[selectedLane].data(),
                          pattern.laneSourceLabels[selectedLane].size(), "EMPTY");
        }
        waveMinimum[selectedLane].fill(0);
        waveMaximum[selectedLane].fill(0);
        rebuildWaveText();
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
                completedRecordingLane(lane);
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
        const int length = std::snprintf(path, pathSize, "%s/%s",
                                         moduleDirectory.data(), leaf);
        return length > 0 && static_cast<std::size_t>(length) < pathSize;
    }

    int serializeState(char* buffer, int bufferLength) const
    {
        if (!buffer || bufferLength < 1) return -1;
        return std::snprintf(buffer, static_cast<std::size_t>(bufferLength),
            "{\"version\":2,\"locus\":%.5f,\"field\":%.5f,\"speed\":%.5f,"
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
            "\"seq_length\":%d,\"seq_gate\":%.5f}",
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
            sequenceSwing * 100.0f, sequenceLength, sequenceGate * 100.0f);
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
            && writeU32(file, 6u)
            && writeF32(file, sequenceBpm)
            && writeU32(file, static_cast<std::uint32_t>(activePattern));
        for (const auto& pattern : patterns) {
            ok = ok && writeU32(file, pattern.occupied ? 1u : 0u)
                && writeF32(file, pattern.swing)
                && writeF32(file, pattern.legacyGate)
                && writeU32(file, pattern.length);
            for (const float base : pattern.baseValues) {
                ok = ok && writeF32(file, base);
            }
            for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                ok = ok && writeBytes(file, pattern.laneSourcePaths[lane].data(),
                                      pattern.laneSourcePaths[lane].size())
                    && writeBytes(file, pattern.laneSourceLabels[lane].data(),
                                  pattern.laneSourceLabels[lane].size());
            }
            for (const auto& step : pattern.steps) {
                ok = ok && writeU32(file, step.enabled ? 1u : 0u)
                    && writeU32(file, step.noteCount)
                    && writeU32(file, step.lockMask)
                    && writeU32(file, step.gateLocked ? 1u : 0u)
                    && writeF32(file, step.gate);
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
            && (version >= 1u && version <= 6u);
        constexpr std::uint32_t validLockMask =
            (1u << static_cast<unsigned>(kLockParameterCount)) - 1u;
        if (ok && version >= 5u) {
            std::uint32_t selected = 0u;
            std::array<Pattern, kPatternCount> loadedPatterns {};
            ok = readF32(file, bpm) && readU32(file, selected)
                && selected < kPatternCount;
            for (auto& pattern : loadedPatterns) {
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
                    clampValue(patternLength, 1u, 16u));
                for (float& base : pattern.baseValues) {
                    ok = ok && readF32(file, base) && std::isfinite(base);
                }
                if (version >= 6u) {
                    for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
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
                    }
                }
                for (auto& step : pattern.steps) {
                    std::uint32_t enabled = 0u;
                    std::uint32_t noteCount = 0u;
                    std::uint32_t lockMask = 0u;
                    std::uint32_t gateLocked = 0u;
                    ok = ok && readU32(file, enabled)
                        && readU32(file, noteCount)
                        && readU32(file, lockMask)
                        && readU32(file, gateLocked)
                        && readF32(file, step.gate);
                    step.enabled = enabled != 0u;
                    step.noteCount = static_cast<std::uint8_t>(clampValue(
                        noteCount, 0u,
                        static_cast<std::uint32_t>(kSequenceChordSize)));
                    step.lockMask = lockMask & validLockMask;
                    step.gateLocked = gateLocked != 0u;
                    step.gate = clampValue(step.gate, 0.05f, 8.0f);
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
                            clampValue(lengthSteps, 1u, 16u));
                        step.noteOpen[chord] = noteOpen != 0u ? 1u : 0u;
                    }
                    for (float& lock : step.locks) {
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
        for (auto& step : loaded) {
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
            step.lockMask = lockMask & validLockMask;
            for (float& lock : step.locks) ok = ok && readF32(file, lock);
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

    bool writeLaneFile(std::size_t lane)
    {
        if (lane >= kLaneCount) return false;
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

        if (!engine.laneLoaded(lane) || (pendingClearMask & (1u << lane)) != 0u) {
            std::remove(path.data());
            std::remove(temporary.data());
            return true;
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

        std::FILE* file = std::fopen(temporary.data(), "wb");
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
        if (!ok || !closed || std::rename(temporary.data(), path.data()) != 0) {
            std::remove(temporary.data());
            return false;
        }
        return true;
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
        bool ok = writeStateFile();
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
        if (jsonNumber(json, "algorithm", value)) controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(clampValue(static_cast<int>(value), 0, 3));
        if (jsonNumber(json, "output_gain", value)) outputGain = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "record_gain", value)) recordGain = clampValue(value, 0.0f, 4.0f);
        if (jsonNumber(json, "pressure_mode", value)) pressureMode = clampValue(static_cast<int>(value), 0, 1);
        if (jsonNumber(json, "root_note", value)) rootNote = clampValue(static_cast<int>(value), 24, 96);
        if (jsonNumber(json, "pad_octave", value)) padOctave = clampValue(static_cast<int>(value), -3, 3);
        if (jsonNumber(json, "record_lane", value) && !engine.recording()) selectedLane = static_cast<std::size_t>(clampValue(static_cast<int>(value), 1, 4) - 1);
        if (jsonNumber(json, "record_source", value) && !engine.recording()) recordSource = clampValue(static_cast<int>(value), 0, 1);
        if (jsonNumber(json, "motion_mode", value)) controls.motion = static_cast<Engine::MotionMode>(clampValue(static_cast<int>(value), 0, 6));
        if (jsonNumber(json, "motion_sound", value)) controls.motionSound = static_cast<Engine::MotionSound>(clampValue(static_cast<int>(value), 0, 3));
        if (jsonNumber(json, "motion_rate", value)) controls.motionSoundRateHz = clampValue(value, 0.25f, 20.0f);
        if (jsonNumber(json, "lanes_mode", value)) controls.lanes = static_cast<Engine::LanesMode>(clampValue(static_cast<int>(value), 0, 2));
        if (jsonNumber(json, "lane_path", value)) controls.lanePath = static_cast<Engine::LanePath>(clampValue(static_cast<int>(value), 0, 6));
        if (jsonNumber(json, "lanes_stretch", value)) controls.lanesStretch = value >= 0.5f;
        if (jsonNumber(json, "stretch_ratio", value)) controls.lanesStretchRatio = clampValue(value, 0.25f, 4.0f);
        if (jsonNumber(json, "lane_1_gain", value)) controls.laneGains[0u] = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "lane_2_gain", value)) controls.laneGains[1u] = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "lane_3_gain", value)) controls.laneGains[2u] = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "lane_4_gain", value)) controls.laneGains[3u] = clampValue(value, 0.0f, 2.0f);
        if (jsonNumber(json, "grain_mode", value)) controls.grains = static_cast<Engine::GrainMode>(clampValue(static_cast<int>(value), 0, 3));
        if (jsonNumber(json, "grain_size", value)) controls.grainSizeMs = clampValue(value, 8.0f, 500.0f);
        if (jsonNumber(json, "grain_density", value)) controls.grainDensityHz = clampValue(value, 1.0f, 64.0f);
        if (jsonNumber(json, "grain_pitch", value)) controls.grainPitch = clampValue(value, 0.25f, 4.0f);
        if (jsonNumber(json, "grain_window", value)) controls.grainWindow = static_cast<Engine::GrainWindow>(clampValue(static_cast<int>(value), 0, 5));
        if (jsonNumber(json, "grain_spread", value)) controls.grainStereoSpread = clampValue(value, 0.0f, 1.0f);
        if (jsonNumber(json, "cutups_mode", value)) controls.cutups = static_cast<Engine::CutupsMode>(clampValue(static_cast<int>(value), 0, 4));
        if (jsonNumber(json, "seq_bpm", value)) sequenceBpm = clampValue(value, 40.0f, 240.0f);
        if (jsonNumber(json, "seq_swing", value)) sequenceSwing = clampValue(value / 100.0f, 0.5f, 0.75f);
        if (jsonNumber(json, "seq_length", value)) sequenceLength = clampValue(static_cast<int>(value), 1, 16);
        if (jsonNumber(json, "seq_gate", value)) setSequenceGate(value);
    }
};

void* createInstance(const char* moduleDir, const char*)
{
    std::unique_ptr<ThruInstance> instance(new (std::nothrow) ThruInstance());
    if (!instance || !instance->prepare(moduleDir)) return nullptr;
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

    if (std::strcmp(key, "state") == 0) { instance->restoreState(value); return; }
    if (std::strcmp(key, "locus") == 0) { controls.locus = clampValue(parseFloat(value, controls.locus), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "field") == 0) { controls.field = clampValue(parseFloat(value, controls.field), 0.02f, 1.0f); return; }
    if (std::strcmp(key, "speed") == 0) { controls.speed = clampValue(parseFloat(value, controls.speed), 0.05f, 4.0f); return; }
    if (std::strcmp(key, "manual_lane") == 0) { controls.manualLane = clampValue(parseFloat(value, controls.manualLane + 1.0f) - 1.0f, 0.0f, 3.0f); return; }
    if (std::strcmp(key, "lane_cycles") == 0) { controls.laneCycles = clampValue(parseFloat(value, controls.laneCycles), 0.125f, 16.0f); return; }
    if (std::strcmp(key, "algorithm") == 0) { controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(parseEnum(value, kAlgorithmNames, static_cast<int>(controls.algorithm))); return; }
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
    if (std::strcmp(key, "all_notes_off") == 0) { instance->allNotesOff(); return; }
    if (std::strcmp(key, "seq_running") == 0) { instance->setSequenceRunning(isTruthy(value)); return; }
    if (std::strcmp(key, "seq_bpm") == 0) { instance->sequenceBpm = clampValue(parseFloat(value, instance->sequenceBpm), 40.0f, 240.0f); return; }
    if (std::strcmp(key, "seq_swing") == 0) { instance->sequenceSwing = clampValue(parseFloat(value, instance->sequenceSwing * 100.0f) / 100.0f, 0.5f, 0.75f); return; }
    if (std::strcmp(key, "seq_length") == 0) { instance->sequenceLength = clampValue(parseInteger(value, instance->sequenceLength), 1, 16); return; }
    if (std::strcmp(key, "seq_gate") == 0) { instance->setSequenceGate(parseFloat(value, instance->sequenceGate * 100.0f)); return; }
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

    if (std::strcmp(key, "motion_mode") == 0) { controls.motion = static_cast<Engine::MotionMode>(parseEnum(value, kMotionModeNames, static_cast<int>(controls.motion))); return; }
    if (std::strcmp(key, "motion_sound") == 0) { controls.motionSound = static_cast<Engine::MotionSound>(parseEnum(value, kMotionSoundNames, static_cast<int>(controls.motionSound))); return; }
    if (std::strcmp(key, "motion_rate") == 0) { controls.motionSoundRateHz = clampValue(parseFloat(value, controls.motionSoundRateHz), 0.25f, 20.0f); return; }
    if (std::strcmp(key, "lanes_mode") == 0) { controls.lanes = static_cast<Engine::LanesMode>(parseEnum(value, kLanesModeNames, static_cast<int>(controls.lanes))); return; }
    if (std::strcmp(key, "lane_path") == 0) { controls.lanePath = static_cast<Engine::LanePath>(parseEnum(value, kLanePathNames, static_cast<int>(controls.lanePath))); return; }
    if (std::strcmp(key, "lanes_stretch") == 0) { controls.lanesStretch = parseEnum(value, kOnOffNames, controls.lanesStretch ? 1 : 0) != 0; return; }
    if (std::strcmp(key, "stretch_ratio") == 0) { controls.lanesStretchRatio = clampValue(parseFloat(value, controls.lanesStretchRatio), 0.25f, 4.0f); return; }
    if (std::strncmp(key, "lane_", 5u) == 0 && std::strlen(key) == 11u && std::strcmp(key + 6, "_gain") == 0) {
        const int lane = key[5] - '1';
        if (lane >= 0 && lane < 4) controls.laneGains[lane] = clampValue(parseFloat(value, controls.laneGains[lane]), 0.0f, 2.0f);
        return;
    }
    if (std::strcmp(key, "grain_mode") == 0) { controls.grains = static_cast<Engine::GrainMode>(parseEnum(value, kGrainModeNames, static_cast<int>(controls.grains))); return; }
    if (std::strcmp(key, "grain_size") == 0) { controls.grainSizeMs = clampValue(parseFloat(value, controls.grainSizeMs), 8.0f, 500.0f); return; }
    if (std::strcmp(key, "grain_density") == 0) { controls.grainDensityHz = clampValue(parseFloat(value, controls.grainDensityHz), 1.0f, 64.0f); return; }
    if (std::strcmp(key, "grain_pitch") == 0) { controls.grainPitch = clampValue(parseFloat(value, controls.grainPitch), 0.25f, 4.0f); return; }
    if (std::strcmp(key, "grain_window") == 0) { controls.grainWindow = static_cast<Engine::GrainWindow>(parseEnum(value, kGrainWindowNames, static_cast<int>(controls.grainWindow))); return; }
    if (std::strcmp(key, "grain_spread") == 0) { controls.grainStereoSpread = clampValue(parseFloat(value, controls.grainStereoSpread), 0.0f, 1.0f); return; }
    if (std::strcmp(key, "cutups_mode") == 0) { controls.cutups = static_cast<Engine::CutupsMode>(parseEnum(value, kCutupsModeNames, static_cast<int>(controls.cutups))); return; }
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
    if (std::strcmp(key, "seq_state") == 0) return std::snprintf(
        buffer, bufferLength, "%d,%d,%u,%.2f,%d,%.2f,%.2f",
        instance->sequenceRunning ? 1 : 0, instance->sequenceCurrentStep,
        instance->sequenceMask(), instance->sequenceBpm,
        instance->sequenceLength, instance->sequenceSwing * 100.0f,
        instance->sequenceGate * 100.0f);
    if (std::strcmp(key, "seq_locks") == 0) {
        return instance->formatSequenceLocks(buffer, bufferLength);
    }
    if (std::strcmp(key, "seq_note_counts") == 0) {
        return instance->formatSequenceNoteCounts(buffer, bufferLength);
    }
    if (std::strcmp(key, "seq_pad_masks") == 0) {
        return instance->formatSequencePadMasks(buffer, bufferLength);
    }
    if (std::strcmp(key, "seq_open_mask") == 0) return std::snprintf(
        buffer, bufferLength, "%u", instance->sequenceOpenPadMask());
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
    if (std::strcmp(key, "lane_source") == 0) return copyText(
        instance->laneSourceLabels[instance->selectedLane].data(),
        buffer, bufferLength);

    if (std::strcmp(key, "locus") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.locus);
    if (std::strcmp(key, "field") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.field);
    if (std::strcmp(key, "speed") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.speed);
    if (std::strcmp(key, "manual_lane") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.manualLane + 1.0f);
    if (std::strcmp(key, "lane_cycles") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.laneCycles);
    if (std::strcmp(key, "algorithm") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.algorithm));
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
    if (std::strcmp(key, "seq_running") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->sequenceRunning ? 1 : 0);
    if (std::strcmp(key, "seq_bpm") == 0) return std::snprintf(buffer, bufferLength, "%.2f", instance->sequenceBpm);
    if (std::strcmp(key, "seq_swing") == 0) return std::snprintf(buffer, bufferLength, "%.2f", instance->sequenceSwing * 100.0f);
    if (std::strcmp(key, "seq_length") == 0) return std::snprintf(buffer, bufferLength, "%d", instance->sequenceLength);
    if (std::strcmp(key, "seq_gate") == 0) return std::snprintf(buffer, bufferLength, "%.2f", instance->sequenceGate * 100.0f);

    if (std::strcmp(key, "motion_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.motion));
    if (std::strcmp(key, "motion_sound") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.motionSound));
    if (std::strcmp(key, "motion_rate") == 0) return std::snprintf(buffer, bufferLength, "%.4f", controls.motionSoundRateHz);
    if (std::strcmp(key, "lanes_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.lanes));
    if (std::strcmp(key, "lane_path") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.lanePath));
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
    if (std::strcmp(key, "cutups_mode") == 0) return std::snprintf(buffer, bufferLength, "%d", static_cast<int>(controls.cutups));
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
    instance->render(output, frames);
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

