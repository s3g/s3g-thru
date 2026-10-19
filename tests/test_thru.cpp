#include "thru_schwung_abi.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr int kFrames = MOVE_FRAMES_PER_BLOCK;
constexpr int kInputOffset = 4096;
std::array<std::uint8_t, 8192> mappedMemory {};
std::array<std::int16_t, kFrames * 2> output {};

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

void writePcm16Wav(const std::string& path)
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
    u32(16u); u16(1u); u16(2u); u32(MOVE_SAMPLE_RATE);
    u32(MOVE_SAMPLE_RATE * 4u); u16(4u); u16(16u);
    assert(std::fwrite("data", 1u, 4u, file) == 4u);
    u32(dataBytes);
    for (std::uint32_t frame = 0u; frame < frames; ++frame) {
        const bool burst = frame < 192u
            || (frame >= 1100u && frame < 1292u)
            || (frame >= 2200u && frame < 2392u)
            || (frame >= 3300u && frame < 3492u);
        u16(static_cast<std::uint16_t>(burst ? 12000 : 0));
        u16(static_cast<std::uint16_t>(burst ? -6000 : 0));
    }
    assert(std::fclose(file) == 0);
}

void waitForPattern(plugin_api_v2_t* api, void* instance, int pattern)
{
    for (int attempt = 0; attempt < 1200
         && stateField(get(api, instance, "pattern_state"), 0) != pattern;
         ++attempt) {
        render(api, instance, 1);
        usleep(1000);
    }
    if (stateField(get(api, instance, "pattern_state"), 0) != pattern) {
        std::fprintf(stderr, "Pattern %d failed to activate: state=%s prepare=%s error=%s\n",
                     pattern, get(api, instance, "pattern_state").c_str(),
                     get(api, instance, "pattern_prepare_state").c_str(),
                     get(api, instance, "sample_load_state").c_str());
    }
    assert(stateField(get(api, instance, "pattern_state"), 0) == pattern);
}

} // namespace

int main()
{
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

    assert(get(api, instance, "polyphony") == "8");
    constexpr const char* noPlayheads = "-1,-1,-1,-1,-1,-1,-1,-1";
    assert(get(api, instance, "playheads") == noPlayheads);

    /* Every engine/global control has a stable absolute external MIDI CC.
       Internal Move CCs remain reserved for the takeover surface. */
    const std::string defaultState = get(api, instance, "state");
    const std::string initialLocus = get(api, instance, "locus");
    api->set_param(instance, "state", "{\"version\":4,\"locus\":0}");
    assert(get(api, instance, "locus") == initialLocus);
    std::array<char, 16> removedParam {};
    assert(api->get_param(instance, "seq_bpm", removedParam.data(),
                          static_cast<int>(removedParam.size())) < 0);
    struct CcCheck { int cc; const char* key; const char* high; };
    const CcCheck ccChecks[] {
        {20, "locus", "1.0000"}, {21, "field", "1.0000"},
        {22, "speed", "4.0000"}, {23, "manual_lane", "4.0000"},
        {24, "lane_cycles", "16.0000"}, {25, "algorithm", "5"},
        {26, "join", "1.0000"}, {27, "output_gain", "2.0000"},
        {28, "motion_mode", "7"}, {29, "motion_sound", "3"},
        {30, "motion_rate", "20.0000"}, {31, "lanes_mode", "2"},
        {32, "lane_path", "7"}, {33, "lanes_stretch", "1"},
        {34, "stretch_ratio", "4.0000"}, {35, "lane_1_gain", "2.0000"},
        {36, "lane_2_gain", "2.0000"}, {37, "lane_3_gain", "2.0000"},
        {38, "lane_4_gain", "2.0000"}, {39, "grain_mode", "3"},
        {40, "grain_size", "1000.0000"}, {41, "grain_density", "96.0000"},
        {42, "grain_pitch", "4.0000"}, {43, "grain_window", "5"},
        {44, "grain_spread", "1.0000"}, {45, "cutups_mode", "4"},
        {46, "pressure_mode", "1"}, {47, "root_note", "96"},
        {48, "record_lane", "4"}, {49, "record_source", "1"},
        {50, "record_gain", "4.0000"}, {51, "monitor_level", "1.0000"},
        {54, "tempo_bpm", "240.00"}, {55, "tempo_swing", "75.00"},
        {56, "clip_playing", "1"},
        {69, "motion_travel", "1.0000"}, {70, "motion_jitter", "1.0000"},
        {71, "packet_duty", "98.00"}, {72, "motor_rate", "20.0000"},
        {73, "motor_symmetry", "95.00"}, {74, "motor_shape", "2"},
        {75, "lane_blend", "1"}, {76, "lane_path_shape", "3"},
        {77, "lane_path_offset", "1.0000"}, {78, "lane_path_skew", "0.9800"},
        {79, "lane_path_curve", "1.0000"}, {80, "lane_slew", "500.00"},
        {81, "grain_spray", "1.0000"}, {82, "grain_bias", "2"},
        {83, "grain_pitch_spray", "24.0000"}, {84, "grain_reverse", "100.00"},
        {85, "grain_size_variation", "1.0000"},
        {86, "grain_level_variation", "1.0000"}, {87, "grain_timing", "1"},
        {88, "grain_timing_scatter", "1.0000"},
        {89, "grain_envelope_skew", "1.0000"}, {90, "grain_process", "3"},
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
    };
    for (const auto& check : ccChecks) {
        midi(api, instance, 0xB0, static_cast<std::uint8_t>(check.cc), 127,
             MOVE_MIDI_SOURCE_EXTERNAL);
        assert(get(api, instance, check.key) == check.high);
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
    const std::string firstCapture = persistenceDir
        + "/captures/p01-lane-1.wav";
    struct stat captureStatus {};
    assert(stat(firstCapture.c_str(), &captureStatus) == 0);
    assert(get(api, instance, "lane_source_state").rfind("CAPTURE:TAKE P01", 0u)
        == 0u);
    api->set_param(instance, "sample_trim_start", "0.10");
    api->set_param(instance, "sample_trim_end", "0.90");
    api->set_param(instance, "sample_trim_apply", "1");
    assert(get(api, instance, "lane_source") == "TRIMMED");
    api->set_param(instance, "sample_reverse", "1");
    midi(api, instance, 0x90, 60, 127);
    render(api, instance, 4);
    const int peakBeforeNormalize = peak();
    midi(api, instance, 0x80, 60, 0);
    api->set_param(instance, "sample_normalize", "1");
    assert(get(api, instance, "last_error").empty());
    midi(api, instance, 0x90, 60, 127);
    render(api, instance, 4);
    const int peakAfterNormalize = peak();
    midi(api, instance, 0x80, 60, 0);
    assert(peakBeforeNormalize > 0);
    assert(peakAfterNormalize > peakBeforeNormalize * 2);
    api->set_param(instance, "sample_fade", "1");
    api->set_param(instance, "maintenance", "1");
    assert(get(api, instance, "lane_source_state").rfind("CAPTURE:FADED", 0u)
        == 0u);
    const std::string editCaptureA = persistenceDir
        + "/captures/p01-lane-1-edit-a.wav";
    const std::string editCaptureB = persistenceDir
        + "/captures/p01-lane-1-edit-b.wav";
    assert(stat(editCaptureA.c_str(), &captureStatus) == 0);
    assert(stat(editCaptureB.c_str(), &captureStatus) == 0);
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
    writePcm16Wav(librarySample);
    api->set_param(instance, "record_lane", "2");
    api->set_param(instance, "preview_file", librarySample.c_str());
    int previewState = 0;
    for (int attempt = 0; attempt < 500 && previewState != 8
         && previewState != 7; ++attempt) {
        render(api, instance, 1);
        usleep(1000);
        previewState = stateField(get(api, instance, "sample_load_state"), 0);
    }
    assert(previewState == 8);
    render(api, instance, 4);
    assert(peak() > 0);
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
    api->set_param(instance, "pattern_copy", "2");
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
    assert(stateField(clipState, 6) >= 2);
    render(api, instance, 4);
    assert(peak() > 0);
    const std::string loopVoicePads = get(api, instance, "voice_pads");
    bool loopHasPad5 = false;
    for (int voice = 0; voice < 8; ++voice) {
        loopHasPad5 = loopHasPad5 || stateField(loopVoicePads, voice) == 5;
    }
    assert(loopHasPad5);

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

    /* Clearing only the performance layer is undoable and does not remove
       the pattern's sample ownership. */
    api->set_param(instance, "clip_clear", "1");
    assert(stateField(get(api, instance, "clip_state"), 4) == 0);
    assert(get(api, instance, "undo_available") == "1");
    api->set_param(instance, "undo", "1");
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);

    /* Chains are a persistent ordered list with independent repeat counts. */
    api->set_param(instance, "chain_slot", "1");
    api->set_param(instance, "chain_pattern", "2");
    api->set_param(instance, "chain_repeats", "2");
    api->set_param(instance, "chain_slot", "2");
    api->set_param(instance, "chain_pattern", "3");
    api->set_param(instance, "chain_repeats", "1");
    const std::string chainEntries = get(api, instance, "chain_entries");
    assert(chainEntries.rfind("1:1:2,1:2:1", 0u) == 0u);
    midi(api, instance, 0xB0, 58, 127, MOVE_MIDI_SOURCE_EXTERNAL);
    assert(get(api, instance, "chain_entries").rfind("1:1:2,0:0:1", 0u)
        == 0u);
    api->set_param(instance, "chain_pattern", "3");
    api->set_param(instance, "chain_repeats", "1");
    api->set_param(instance, "chain_enabled", "1");
    assert(stateField(get(api, instance, "chain_state"), 0) == 1);
    api->set_param(instance, "chain_enabled", "0");

    /* Four banks of sixteen performance clips snapshot event data, source
       assignments, and engine settings. Running changes queue at a loop seam. */
    api->set_param(instance, "pattern_copy", "1");
    assert((unsignedStateField(get(api, instance, "pattern_state"), 2) & 0x7u)
        == 0x7u);
    api->set_param(instance, "pattern_select", "1");
    waitForPattern(api, instance, 1);
    api->set_param(instance, "algorithm", "Lanes");
    api->set_param(instance, "pattern_select", "0");
    waitForPattern(api, instance, 0);
    assert(get(api, instance, "algorithm") == "0");
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);
    api->set_param(instance, "clip_playing", "1");
    api->set_param(instance, "pattern_select", "1");
    assert(stateField(get(api, instance, "pattern_state"), 1) == 1);
    for (int block = 0; block < 240
         && stateField(get(api, instance, "pattern_state"), 0) != 1; ++block) {
        render(api, instance, 1);
    }
    assert(stateField(get(api, instance, "pattern_state"), 0) == 1);
    assert(get(api, instance, "algorithm") == "1");
    api->set_param(instance, "clip_playing", "0");
    api->set_param(instance, "pattern_select", "0");
    waitForPattern(api, instance, 0);

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
        "Continuous", "Packets", "Motor", "Bounce"
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
    midi(api, instance, 0xA0, 60, 120);
    render(api, instance, 4);
    midi(api, instance, 0x80, 60, 0);
    midi(api, instance, 0x80, 67, 0);
    for (int note = 61; note <= 66; ++note) {
        midi(api, instance, 0x80, static_cast<std::uint8_t>(note), 0);
    }
    assert(get(api, instance, "playheads") == noPlayheads);
    render(api, instance, 160);
    assert(get(api, instance, "playheads") == noPlayheads);
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
    api->set_param(instance, "performance_lane", "2.5");
    assert(get(api, instance, "performance_lane") == "2.5000");

    /* Velocity chooses performance depth, never sample amplitude. With one
       populated lane, low and high strikes must converge on the same level. */
    midi(api, instance, 0x90, 60, 1);
    render(api, instance, 8);
    const int lowVelocityPeak = peak();
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 40);
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

    /* Library-lane assignments belong to patterns. Pattern 3 recalls its
       source, while A01's explicit EMPTY ownership remains silent. */
    api->set_param(instance, "pattern_select", "2");
    waitForPattern(api, instance, 2);
    assert(get(api, instance, "lane_source") == "Library Tone.wav");
    api->set_param(instance, "pattern_select", "0");
    waitForPattern(api, instance, 0);
    assert(get(api, instance, "lane_source") == "EMPTY");

    /* Monitor is explicit and independent from recording or note velocity. */
    api->set_param(instance, "monitor_level", "0.5");
    fillInput(12000);
    render(api, instance, 1);
    assert(peak() >= 5999 && peak() <= 6001);
    api->set_param(instance, "monitor_level", "0");
    render(api, instance, 2);
    assert(peak() == 0);

    /* Replace another lane, then exercise every Sample Motion algorithm. */
    recordLane(api, instance, 4, 18000, 12);
    ui = get(api, instance, "ui_state");
    assert((stateField(ui, 1) & 0x9) == 0x9);
    api->set_param(instance, "field", "1");
    for (int algorithm = 0; algorithm < 6; ++algorithm) {
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

    api->set_param(instance, "pad_mode", "Cues");
    api->set_param(instance, "cue_store", "0,0.25");
    assert(stateField(get(api, instance, "cue_state"), 0) == 250);
    api->set_param(instance, "cue_trigger", "0");
    render(api, instance, 4);
    assert(peak() > 0);
    api->set_param(instance, "cue_clear", "0");
    assert(stateField(get(api, instance, "cue_state"), 0) == -1);

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
        "Ordinary", "Stutter", "Shrink", "Doublets"
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
    api->set_param(instance, "grain_window", "Hann");
    api->set_param(instance, "lane_2_gain", "0.375");
    api->set_param(instance, "record_gain", "1.75");
    api->set_param(instance, "pad_octave", "2");
    api->set_param(instance, "algorithm", "Motion");
    api->set_param(instance, "cue_store", "2,0.625");
    const std::string state = get(api, instance, "state");
    assert(state.find("\"version\":5") != std::string::npos);
    assert(state.find("\"clip_record_mode\":1") != std::string::npos);
    assert(state.find("\"monitor_level\"") == std::string::npos);
    api->set_param(instance, "motion_mode", "Forward");
    api->set_param(instance, "grain_window", "Bell");
    api->set_param(instance, "lane_2_gain", "1");
    api->set_param(instance, "record_gain", "1");
    api->set_param(instance, "state", state.c_str());
    assert(get(api, instance, "motion_mode") == "5");
    assert(get(api, instance, "grain_window") == "2");
    assert(get(api, instance, "lane_2_gain") == "0.3750");
    assert(get(api, instance, "record_gain") == "1.7500");
    assert(get(api, instance, "pad_octave") == "2");
    assert(stateField(get(api, instance, "cue_state"), 2) == 625);
    assert(get(api, instance, "chain_entries").rfind("1:1:2,1:2:1", 0u) == 0u);

    std::array<char, 256> error {};
    assert(api->get_error(instance, error.data(), static_cast<int>(error.size())) >= 0);
    api->destroy_instance(instance);

    /* Normal overtake teardown atomically saves settings and stereo float WAV
       lanes. A new instance in the same module directory recalls both. */
    const std::string statePath = persistenceDir + "/thru-state.json";
    const std::string lanePath = persistenceDir + "/thru-lane-1.wav";
    const std::string patternPath = persistenceDir + "/thru-pattern.bin";
    std::FILE* stateFile = std::fopen(statePath.c_str(), "rb");
    std::FILE* laneFile = std::fopen(lanePath.c_str(), "rb");
    std::FILE* patternFile = std::fopen(patternPath.c_str(), "rb");
    assert(stateFile != nullptr);
    assert(laneFile != nullptr);
    assert(patternFile != nullptr);
    char patternMagic[8] {};
    std::uint32_t patternVersion = 0u;
    assert(std::fread(patternMagic, 1u, sizeof(patternMagic), patternFile)
        == sizeof(patternMagic));
    assert(std::memcmp(patternMagic, "S3GTHR2", 7u) == 0);
    assert(std::fread(&patternVersion, sizeof(patternVersion), 1u, patternFile) == 1u);
    assert(patternVersion == 1u);
    std::fclose(stateFile);
    std::fclose(laneFile);
    std::fclose(patternFile);

    instance = api->create_instance(persistenceDir.c_str(), nullptr);
    assert(instance != nullptr);
    assert(get(api, instance, "persistence") == "1,1");
    ui = get(api, instance, "ui_state");
    assert((stateField(ui, 1) & 0x1) != 0);
    assert((stateField(ui, 1) & 0x8) == 0);
    assert(get(api, instance, "motion_mode") == "5");
    assert(get(api, instance, "grain_window") == "2");
    assert(get(api, instance, "lane_2_gain") == "0.3750");
    assert(get(api, instance, "record_gain") == "1.7500");
    assert(get(api, instance, "pad_octave") == "2");
    assert(stateField(get(api, instance, "cue_state"), 2) == 625);
    assert(get(api, instance, "chain_entries").rfind("1:1:2,1:2:1", 0u) == 0u);
    assert((unsignedStateField(get(api, instance, "pattern_state"), 2) & 0x3u)
        == 0x3u);
    api->set_param(instance, "pattern_select", "1");
    waitForPattern(api, instance, 1);
    assert(get(api, instance, "algorithm") == "1");
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);
    assert(stateField(get(api, instance, "clip_state"), 6) >= 2);
    assert(get(api, instance, "clip_rate") == "4.0000");
    api->set_param(instance, "pattern_select", "0");
    waitForPattern(api, instance, 0);
    api->set_param(instance, "record_lane", "2");
    api->set_param(instance, "pattern_select", "2");
    waitForPattern(api, instance, 2);
    assert(get(api, instance, "lane_source") == "Library Tone.wav");
    api->set_param(instance, "pattern_select", "0");
    waitForPattern(api, instance, 0);
    assert(stateField(get(api, instance, "clip_state"), 4) >= 150);
    assert(stateField(get(api, instance, "clip_state"), 5) == 3);
    api->set_param(instance, "pressure_mode", "Off");
    api->set_param(instance, "performance_lane", "1");
    midi(api, instance, 0x90, 60, 100);
    render(api, instance, 12);
    assert(peak() > 8000);
    assert(static_cast<int>(output[0]) * static_cast<int>(output[1]) < 0);
    midi(api, instance, 0x80, 60, 0);
    api->destroy_instance(instance);

    std::remove(statePath.c_str());
    std::remove(lanePath.c_str());
    std::remove(patternPath.c_str());
    for (int lane = 2; lane <= 4; ++lane) {
        const std::string path = persistenceDir + "/thru-lane-"
            + std::to_string(lane) + ".wav";
        std::remove(path.c_str());
    }
    assert(std::remove(librarySample.c_str()) == 0);
    assert(std::remove(libraryDir.c_str()) == 0);
    const std::string capturesDir = persistenceDir + "/captures";
    for (int pattern = 1; pattern <= 64; ++pattern) {
        for (int lane = 1; lane <= 4; ++lane) {
            const std::string path = capturesDir + "/p"
                + (pattern < 10 ? "0" : "") + std::to_string(pattern)
                + "-lane-" + std::to_string(lane) + ".wav";
            std::remove(path.c_str());
            const std::string editBase = capturesDir + "/p"
                + (pattern < 10 ? "0" : "") + std::to_string(pattern)
                + "-lane-" + std::to_string(lane) + "-edit-";
            std::remove((editBase + "a.wav").c_str());
            std::remove((editBase + "b.wav").c_str());
        }
    }
    assert(std::remove(capturesDir.c_str()) == 0);
    assert(std::remove(persistenceDir.c_str()) == 0);
    std::puts("S3G Thru native tests: PASS");
    return 0;
}

