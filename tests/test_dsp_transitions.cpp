// Include the real module so its pre-compressor effects and preview routing
// can be measured without exposing test hooks in the device ABI.
#include "../src/dsp/thru_module.cpp"
#include <cassert>
#include <string>
#include <vector>

namespace {
struct CoreFixture {
    static constexpr std::size_t size = 8192u;
    Engine engine;
    s3g_daisy::ThruWavesetBank bank;
    std::array<std::array<float, size>, 4u> storage {};
    std::array<float, 4u> input {}, output {};
    std::array<const float*, 4u> inputs {};
    std::array<float*, 4u> outputs {};

    explicit CoreFixture(float sampleRate = 44100.0f)
    {
        for (std::size_t channel = 0u; channel < 4u; ++channel) {
            storage[channel].fill(0.6f);
            inputs[channel] = &input[channel];
            outputs[channel] = &output[channel];
        }
        assert(engine.prepare(sampleRate, storage[0u].data(), storage[1u].data(),
            storage[2u].data(), storage[3u].data(), size));
        assert(engine.adoptSource(size, 2u, sampleRate));
    }

    float render(const Engine::Controls& controls, std::size_t frames = 1u)
    {
        for (std::size_t frame = 0u; frame < frames; ++frame)
            engine.process(inputs.data(), outputs.data(), 1u, controls);
        assert(std::isfinite(output[0u]));
        return output[0u];
    }
};

void testRetargetedReaders()
{
    auto f = std::make_unique<CoreFixture>();
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::sample;
    c.sampleMode = Engine::SampleMode::loop;
    f->render(c, 4096u);
    c.sampleMode = Engine::SampleMode::reverse;
    float previous = f->render(c, 110u);
    c.sampleMode = Engine::SampleMode::loop;
    for (std::size_t frame = 0u; frame < 4096u; ++frame) {
        // Return to the source, then retarget again during fade-up/fade-down.
        if (frame == 240u) c.sampleMode = Engine::SampleMode::pingPong;
        if (frame == 340u) c.sampleMode = Engine::SampleMode::loop;
        const float sample = f->render(c);
        assert(std::abs(sample - previous) < 0.006f);
        previous = sample;
    }
    assert(std::abs(previous - 0.6f) < 0.0001f);

    f = std::make_unique<CoreFixture>();
    c = {}; c.algorithm = Engine::PlaybackAlgorithm::sample;
    c.sampleMode = Engine::SampleMode::loop;
    f->render(c, 4096u);
    c.algorithm = Engine::PlaybackAlgorithm::grains;
    previous = f->render(c, 200u);
    c.algorithm = Engine::PlaybackAlgorithm::lanes;
    assert(std::abs(f->render(c) - previous) < 0.006f);
    // Structural edits after the silent midpoint must not restart the fade-up.
    previous = f->render(c, 600u);
    c.algorithm = Engine::PlaybackAlgorithm::sample;
    assert(std::abs(f->render(c) - previous) < 0.006f);
    assert(f->render(c, 8192u) > 0.59f);

    f = std::make_unique<CoreFixture>();
    for (std::size_t channel = 0u; channel < 2u; ++channel)
        for (std::size_t frame = 0u; frame < CoreFixture::size; ++frame)
            f->storage[channel][frame] = -0.6f + 1.2f * frame / CoreFixture::size;
    c = {}; c.algorithm = Engine::PlaybackAlgorithm::lanes; c.speed = 0.05f;
    f->render(c, 4096u);
    f->engine.seekNormalized(0.25f); f->render(c, 1000u);
    c.lanes = Engine::LanesMode::reverse;
    previous = f->render(c, 600u);
    c.lanes = Engine::LanesMode::forward;
    assert(std::abs(f->render(c) - previous) < 0.003f);

    f = std::make_unique<CoreFixture>();
    c = {}; c.motionSound = Engine::MotionSound::freeze; c.join = 1.0f;
    previous = f->render(c, 1800u);
    c.motorShape = Engine::MotorShape::plateau;
    for (std::size_t frame = 0u; frame < 4096u; ++frame) {
        if (frame == 100u) c.motorShape = Engine::MotorShape::exponential;
        if (frame == 500u) c.motorShape = Engine::MotorShape::rounded;
        const float sample = f->render(c);
        assert(std::abs(sample - previous) < 0.006f);
        previous = sample;
    }
}

void testShortCutGates()
{
    for (float rate : {44100.0f, 48000.0f, 96000.0f})
        for (float gate : {0.05f, 0.20f, 0.40f, 0.60f, 0.80f, 1.0f}) {
            auto f = std::make_unique<CoreFixture>(rate);
            Engine::Controls c;
            c.algorithm = Engine::PlaybackAlgorithm::cutups;
            c.cutRateHz = 4.0f; c.cutClock = Engine::CutClock::free;
            c.cutJoinMs = 100.0f; c.cutGate = gate;
            float previous = 0.0f, peak = 0.0f;
            for (std::size_t frame = 0u; frame < static_cast<std::size_t>(rate); ++frame) {
                const float sample = f->render(c);
                if (frame > 1000u) assert(std::abs(sample - previous) < 0.006f);
                peak = std::max(peak, sample);
                previous = sample;
            }
            assert(peak > 0.59f);
        }
}

void testLoFiEditContinuity()
{
    for (float rate : {44100.0f, 48000.0f, 96000.0f}) {
        s3g_daisy::ThruLoFi effect; effect.prepare(rate);
        float previous = 0.57f;
        for (std::size_t frame = 0u; frame < 12000u; ++frame) {
            if (frame == 100u) effect.request(1.0f, 16u);
            if (frame == 220u) effect.request(0.0f, 1u);
            if (frame == 650u) effect.request(0.75f, 7u);
            if (frame == 1500u) effect.request(0.0f, 1u);
            const auto out = effect.process({0.57f, -0.57f});
            assert(std::abs(out[0u] - previous) < 0.001f);
            assert(out[1u] == -out[0u]);
            previous = out[0u];
        }
        assert(previous == 0.57f); // exact full-resolution return
        effect.request(1.0f, 1u);
        for (std::size_t frame = 0u; frame < 2000u; ++frame)
            previous = effect.process({0.57f, 0.57f})[0u];
        assert(previous == std::round(0.57f * 8.0f) / 8.0f);
    }
}

void testWavesetsIntoEffects()
{
    // Check the edit boundary itself against a processor receiving exactly the
    // same Wavesets stream and retaining the previous control settings. Native
    // Wavesets/quantizer/reducer waveform edges are not labelled as edit clicks.
    for (int parameter = 0; parameter < 8; ++parameter) {
        auto source = std::make_unique<CoreFixture>();
        for (std::size_t channel = 0u; channel < 2u; ++channel)
            for (std::size_t frame = 0u; frame < CoreFixture::size; ++frame)
                source->storage[channel][frame] = 0.65f * std::sin(
                    6.28318530718f * 37.0f * frame / CoreFixture::size + channel * 0.2f);
        source->bank.analyze(source->storage[0u].data(), source->storage[1u].data(),
            CoreFixture::size, 44100.0f);
        source->engine.setPreparedWavesetBank(0u, &source->bank);
        Engine::Controls playback;
        playback.algorithm = Engine::PlaybackAlgorithm::wavesets;
        playback.wavesetGroup = 4.0f; playback.wavesetRepeats = 2.0f;
        auto changed = std::make_unique<ThruInstance>();
        auto reference = std::make_unique<ThruInstance>();
        assert(changed->prepare(nullptr)); assert(reference->prepare(nullptr));
        auto fx = changed->controls;
        // Fill delay history and put its mixer into an audible steady state.
        fx.effectMix = 0.5f; fx.effectFeedback = 0.5f;
        const auto original = fx;
        float sourcePeak = 0.0f;
        for (std::size_t block = 0u; block < 400u; ++block) {
            for (std::size_t frame = 0u; frame < kBlockFrames; ++frame) {
                source->render(playback);
                for (std::size_t channel = 0u; channel < 2u; ++channel) {
                    const float sample = source->output[channel];
                    sourcePeak = std::max(sourcePeak, std::abs(sample));
                    changed->voiceMix[channel][frame] = reference->voiceMix[channel][frame] = sample;
                }
            }
            if (block == 350u) {
                switch (parameter) {
                    case 0: fx.effectTone = -1.0f; break;
                    case 1: fx.effectDrive = 1.0f; break;
                    case 2: fx.effectBits = 1.0f; break;
                    case 3: fx.effectRate = 16.0f; break;
                    case 4: fx.effectDelayMs = 17.0f; break;
                    case 5: fx.effectFeedback = 0.9f; break;
                    case 6: fx.effectWidth = 0.0f; break;
                    case 7: fx.effectMix = 1.0f; break;
                }
            }
            changed->processEffects(kBlockFrames, fx);
            reference->processEffects(kBlockFrames, original);
            if (block == 350u)
                for (std::size_t channel = 0u; channel < 2u; ++channel)
                    assert(std::abs(changed->voiceMix[channel][0u]
                        - reference->voiceMix[channel][0u]) < 0.02f);
            for (std::size_t channel = 0u; channel < 2u; ++channel)
                for (std::size_t frame = 0u; frame < kBlockFrames; ++frame)
                    assert(std::isfinite(changed->voiceMix[channel][frame]));
        }
        assert(sourcePeak > 0.1f);
    }
}

void testTapeGestureDuration()
{
    auto edited = std::make_unique<ThruInstance>();
    auto reference = std::make_unique<ThruInstance>();
    assert(edited->prepare(nullptr)); assert(reference->prepare(nullptr));
    auto c = edited->controls;
    c.effectTapeMix = 1.0f; c.effectTapeHissDb = -84.0f;
    c.effectTapeMechanics = c.effectTapeAge = c.effectTapeCrinkle = 0.0f;
    c.effectTapeInertia = 0.16f;
    auto original = c;
    std::size_t position = 0u;
    for (std::size_t block = 0u; block < 1700u; ++block) {
        if (block == 512u) edited->stackScanPaused = reference->stackScanPaused = true;
        if (block == 519u) c.effectTapeInertia = 1.0f;
        if (block == 600u) edited->stackScanPaused = reference->stackScanPaused = false;
        for (std::size_t frame = 0u; frame < kBlockFrames; ++frame) {
            const float sample = 0.5f * std::sin(6.28318530718f * 220.0f * position++ / 44100.0f);
            for (std::size_t channel = 0u; channel < 2u; ++channel)
                edited->voiceMix[channel][frame] = reference->voiceMix[channel][frame] = sample;
        }
        edited->processEffects(kBlockFrames, c);
        reference->processEffects(kBlockFrames, original);
        for (std::size_t frame = 0u; frame < kBlockFrames; ++frame)
            assert(edited->voiceMix[0u][frame] == reference->voiceMix[0u][frame]);
    }
    assert(edited->tapeTransportProgress == 0.0f);
    assert(std::abs(edited->tapeTransportSeconds - 4.0f) < 0.001f);
    edited->stackScanPaused = true;
    edited->processEffects(kBlockFrames, c);
    assert(edited->tapeTransportProgress > 0.0f && edited->tapeTransportProgress < 0.001f);
}

void testPreviewIsolation()
{
    auto instance = std::make_unique<ThruInstance>();
    assert(instance->prepare(nullptr));
    instance->controls.algorithm = Engine::PlaybackAlgorithm::sample;
    instance->controls.sampleMode = Engine::SampleMode::loop;
    for (std::size_t channel = 0u; channel < 2u; ++channel)
        std::fill_n(instance->engine.writableLane(0u, channel), 4096u, 0.4f);
    assert(instance->engine.adoptLane(0u, 4096u, 44100.0f));
    instance->noteOn(60, 100);
    std::array<std::int16_t, kBlockFrames * 2u> output {};
    for (std::size_t block = 0u; block < 160u; ++block)
        instance->render(output.data(), kBlockFrames);
    const float performance = instance->lastVoiceMixSample[0u];
    for (std::size_t channel = 0u; channel < 2u; ++channel)
        std::fill_n(instance->sampleStage.get() + channel * instance->laneCapacityFrames, 4096u, 0.2f);
    instance->sampleStageFrames = 4096u; instance->previewFrame = 0u;
    instance->previewActive = true;
    instance->previewLevel = instance->previewLevelState = 1.0f;
    instance->previewFadeInRemaining = 0u;
    instance->sampleLoadState.store(static_cast<int>(SampleLoadState::previewing));
    instance->render(output.data(), kBlockFrames);
    const float before = instance->lastVoiceMixSample[0u];
    assert(std::abs(before - performance - 0.2f) < 0.0001f);
    // This synchronous replacement path executes before any file decoding.
    instance->requestLibrarySample(0u, "/tmp/thru-test-missing-preview.wav", false, -1, true);
    instance->render(output.data(), kBlockFrames);
    assert(std::abs(instance->voiceMix[0u][0u] - before) < 0.0001f);
    instance->render(output.data(), kBlockFrames);
    instance->render(output.data(), kBlockFrames);
    assert(std::abs(instance->lastVoiceMixSample[0u] - performance) < 0.0001f);
    assert(instance->lastPreviewSample[0u] == 0.0f);
}
#include "test_background.inc"
} // namespace

int main()
{
    testBoundedBackgroundPersistence();
    testBackgroundAnalysisOwnership();
    testBackgroundArchiveAndQueuedRecording();
    testCallbackWorkTelemetry();
    testAntialiasedDrive();
    testRetargetedReaders();
    testShortCutGates();
    testLoFiEditContinuity();
    testWavesetsIntoEffects();
    testTapeGestureDuration();
    testPreviewIsolation();
    std::puts("s3g Thru DSP transition regressions: PASS");
}
