#include "thru_schwung_abi.h"

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
                   bool cueCrossing = false)
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
    assert(littleU32(input, 8u) == 8u);

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
        offset += 64u * 4u; // newer fidelity, Glitch/FX, Weave, ADSR, rotary
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
    assert(littleU32(input, 8u) == 8u);

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
        offset += 64u * 4u; // format-3/4 fidelity plus Weave, ADSR, rotary
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

    /* Every engine/global control has a stable absolute external MIDI CC.
       Internal Move CCs remain reserved for the takeover surface. */
    const std::string defaultState = get(api, instance, "state");
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
    struct CcCheck { int cc; const char* key; const char* high; };
    const CcCheck ccChecks[] {
        {20, "locus", "1.0000"}, {21, "field", "1.0000"},
        {22, "speed", "4.0000"}, {23, "manual_lane", "4.0000"},
        {24, "lane_cycles", "16.0000"}, {25, "algorithm", "7"},
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
    render(api, instance, 1);
    assert(peak() >= 5999 && peak() <= 6001);
    api->set_param(instance, "monitor_level", "0");
    render(api, instance, 2);
    assert(peak() == 0);

    /* The final ceiling is stereo linked and catches full-scale monitoring.
       Dry monitoring bypasses both effects and program compression. */
    api->set_param(instance, "performance_reset", "1");
    fillInput(32767);
    api->set_param(instance, "monitor_level", "1");
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
    for (int algorithm = 0; algorithm < 8; ++algorithm) {
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
    assert(longGrainState.find("\"version\":9") != std::string::npos);
    assert(longGrainState.find("\"long_grain_length\":12000.00000")
        != std::string::npos);

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
    render(api, instance, 8);
    assert(stereoDifferencePeak() <= 1);
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
    render(api, instance, 20);
    assert(stereoDifferencePeak() <= 1);
    midi(api, instance, 0x80, 60, 0);
    render(api, instance, 40);
    api->set_param(instance, "effect_cross_pan", "-100");
    api->set_param(instance, "effect_rotary_rate", "0.8");
    api->set_param(instance, "effect_rotary_depth", "70");
    api->set_param(instance, "effect_rotary_mix", "0");
    api->set_param(instance, "effect_rotary_spread", "100");

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
    api->set_param(instance, "algorithm", "Motion");
    api->set_param(instance, "cue_store", "2,0.625");
    const std::string state = get(api, instance, "state");
    assert(state.find("\"version\":9") != std::string::npos);
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
    assert(get(api, instance, "long_grain_length") == "12000.00");
    assert(get(api, instance, "long_grain_layers") == "4");
    assert(get(api, instance, "long_grain_advance") == "1");
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
    assert(clipVersion == 8u);
    std::fclose(stateFile);
    std::fclose(laneFile);
    std::fclose(clipFile);

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
    assert(clipVersion == 8u);
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

