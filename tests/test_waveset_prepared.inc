// Different channel periods expose seams hidden by mono zero crossings.
void loadWavesetStereoSeamFixture(ClickFixture& fixture, unsigned period)
{
    for (std::size_t frame = 0u; frame < ClickFixture::capacity; ++frame) {
        fixture.storage[0u][frame] = 0.6f * static_cast<float>(std::sin(
            6.283185307179586 * (frame % period) / period));
        fixture.storage[1u][frame] = 0.75f * static_cast<float>(std::cos(
            6.283185307179586 * frame / (period * 4u)));
    }
    assert(fixture.engine.adoptSource(ClickFixture::capacity, 2u, MOVE_SAMPLE_RATE));
    fixture.prepareWavesets();
}

void testWavesetStereoSeams()
{
    using Process = ClickEngine::WavesetProcess;
    for (auto process : {Process::repeat, Process::omit, Process::replace,
            Process::envelope, Process::multiply, Process::interpolate,
            Process::fractal, Process::harmonic, Process::groupReverse,
            Process::cycleReverse}) {
        for (float join : {0.0f, 0.5f, 1.0f}) {
            ClickFixture fixture;
            loadWavesetStereoSeamFixture(fixture, 64u);
            ClickEngine::Controls control;
            control.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
            control.wavesetGroup = control.wavesetRepeats = 1.0f;
            control.wavesetAdvance = ClickEngine::WavesetAdvance::hold;
            control.wavesetProcess = process;
            control.wavesetProcessAmount = 1.0f;
            control.join = join;
            fixture.renderPeak(control, 64);
            float previous = fixture.output[1u][kFrames - 1u];
            float maximum = 0.0f;
            for (unsigned frame = 0u; frame < 4096u; ++frame) {
                fixture.engine.process(fixture.inputPointers.data(),
                    fixture.outputPointers.data(), 1u, control);
                const float value = fixture.output[1u][0u];
                assert(std::isfinite(value));
                maximum = std::max(maximum, std::abs(value - previous));
                previous = value;
            }
            if (maximum >= 0.14f)
                std::fprintf(stderr, "Stereo seam process %u Join %.1f step %.6f\n",
                    static_cast<unsigned>(process), join, maximum);
            assert(maximum < 0.14f);
        }
    }
}

void testWavesetOutgoingSeams()
{
    for (unsigned offset : {0u, 13u, 31u, 63u}) {
        ClickFixture fixture;
        loadWavesetStereoSeamFixture(fixture, 64u);
        ClickEngine::Controls control;
        control.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        control.wavesetGroup = control.wavesetRepeats = 1.0f;
        control.wavesetAdvance = ClickEngine::WavesetAdvance::hold;
        control.join = 0.0f;
        control.wavesetProcessAmount = 1.0f;
        fixture.renderPeak(control, 64);
        for (unsigned frame = 0u; frame < offset; ++frame)
            fixture.engine.process(fixture.inputPointers.data(),
                fixture.outputPointers.data(), 1u, control);
        float previous = offset ? fixture.output[1u][0u]
            : fixture.output[1u][kFrames - 1u];
        float maximum = 0.0f;
        const auto starts = fixture.engine.wavesetOverlapStartCount();
        for (unsigned frame = 0u; frame < 7200u; ++frame) {
            // Both directions include the audible outgoing reader, including
            // when it already owns a decaying boundary correction.
            control.join = (frame / 1200u) % 2u ? 0.0f : 1.0f;
            control.wavesetProcess = (frame / 1200u) % 2u
                ? ClickEngine::WavesetProcess::repeat : ClickEngine::WavesetProcess::cycleReverse;
            fixture.engine.process(fixture.inputPointers.data(),
                fixture.outputPointers.data(), 1u, control);
            const float value = fixture.output[1u][0u];
            maximum = std::max(maximum, std::abs(value - previous));
            previous = value;
        }
        assert(fixture.engine.wavesetOverlapStartCount() == starts + 6u);
        assert(maximum < 0.14f);
    }
}

void testWavesetShortCycleSafety()
{
    ClickFixture fixture;
    loadWavesetStereoSeamFixture(fixture, 16u);
    ClickEngine::Controls control;
    control.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
    control.wavesetGroup = control.wavesetRepeats = 1.0f;
    control.wavesetAdvance = ClickEngine::WavesetAdvance::hold;
    control.speed = 4.0f;
    control.join = 0.0f;
    fixture.renderPeak(control, 128);
    float leftPeak = 0.0f, rightPeak = 0.0f;
    double rightSum = 0.0;
    for (unsigned frame = 0u; frame < 4096u; ++frame) {
        fixture.engine.process(fixture.inputPointers.data(),
            fixture.outputPointers.data(), 1u, control);
        leftPeak = std::max(leftPeak, std::abs(fixture.output[0u][0u]));
        rightPeak = std::max(rightPeak, std::abs(fixture.output[1u][0u]));
        rightSum += fixture.output[1u][0u];
    }
    assert(leftPeak > 0.3f);
    assert(rightPeak < 0.4f);
    assert(std::abs(rightSum / 4096.0) < 0.2);

    // Contiguous raw playback must preserve even a slow right channel when
    // a fast left channel supplies the crossing map. It needs no detrending.
    control.speed = 1.0f;
    control.wavesetGroup = 8.0f;
    control.wavesetAdvance = ClickEngine::WavesetAdvance::stretch;
    fixture.engine.retrigger(false);
    double previous = 0.0;
    for (unsigned frame = 0u; frame < 5000u; ++frame) {
        fixture.engine.process(fixture.inputPointers.data(),
            fixture.outputPointers.data(), 1u, control);
        if (frame > 3500u) {
            const float expected = s3g_daisy::ThruWavesetBank::read(
                fixture.storage[1u].data(), ClickFixture::capacity, previous);
            assert(std::abs(expected - fixture.output[1u][0u]) < 0.001f);
        }
        previous = fixture.engine.wavesetLanePosition(0u) * ClickFixture::capacity;
    }
}

void testWavesetShortWindowSafety()
{
    // A tiny Field repeatedly wraps the complete source window, even with
    // Forward/Repeat 1/Stride 1. It must not count as continuous source audio.
    for (float span : {17.0f, 33.0f, 65.0f}) {
        ClickFixture fixture;
        loadWavesetStereoSeamFixture(fixture, 16u);
        ClickEngine::Controls control;
        control.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        control.wavesetGroup = control.wavesetRepeats = 1.0f;
        control.wavesetAdvance = ClickEngine::WavesetAdvance::stretch;
        control.speed = 4.0f;
        control.join = 0.0f;
        control.locus = 0.0f;
        control.field = span / ClickFixture::capacity;
        fixture.renderPeak(control, 160);
        float peak = 0.0f;
        double sum = 0.0;
        for (unsigned frame = 0u; frame < 4096u; ++frame) {
            fixture.engine.process(fixture.inputPointers.data(),
                fixture.outputPointers.data(), 1u, control);
            const float value = fixture.output[1u][0u];
            assert(std::isfinite(value));
            peak = std::max(peak, std::abs(value));
            sum += value;
        }
        assert(peak < 0.4f);
        assert(std::abs(sum / 4096.0) < 0.2);
    }
}

void testWavesetKeyboardPitch()
{
    for (float pitch : {0.5f, 1.0f, 2.0f, 4.0f, 8.0f}) {
        ClickFixture fixture;
        loadWavesetStereoSeamFixture(fixture, 64u);
        ClickEngine::Controls control;
        control.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        control.wavesetGroup = control.wavesetRepeats = 1.0f;
        control.wavesetAdvance = ClickEngine::WavesetAdvance::hold;
        control.speed = 4.0f;
        control.notePitch = pitch;
        fixture.renderPeak(control, 160);
        float previous = fixture.output[0u][kFrames - 1u];
        unsigned crossings = 0u;
        for (unsigned frame = 0u; frame < 4096u; ++frame) {
            fixture.engine.process(fixture.inputPointers.data(),
                fixture.outputPointers.data(), 1u, control);
            const float value = fixture.output[0u][0u];
            if (previous < 0.0f && value >= 0.0f) ++crossings;
            previous = value;
        }
        const float expected = 4096.0f * std::min(0.45f, 4.0f * pitch / 64.0f);
        assert(std::abs(static_cast<float>(crossings) - expected) <= 2.0f);
    }
}

void testWavesetContinuousEdits()
{
    using Process = ClickEngine::WavesetProcess;
    for (auto process : {Process::repeat, Process::omit, Process::replace,
            Process::envelope, Process::multiply, Process::interpolate,
            Process::fractal, Process::harmonic, Process::groupReverse,
            Process::cycleReverse}) {
        auto edited = std::make_unique<ClickFixture>();
        auto reference = std::make_unique<ClickFixture>();
        loadWavesetStereoSeamFixture(*edited, 64u);
        loadWavesetStereoSeamFixture(*reference, 64u);
        ClickEngine::Controls original;
        original.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        original.wavesetGroup = 8.0f;
        original.wavesetRepeats = 16.0f;
        original.wavesetAdvance = ClickEngine::WavesetAdvance::hold;
        original.wavesetProcess = process;
        original.wavesetProcessAmount = 0.0f;
        original.join = 0.0f;
        edited->renderPeak(original, 64);
        reference->renderPeak(original, 64);
        const auto builds = edited->engine.wavesetGroupBuildCount();
        const auto starts = edited->engine.wavesetOverlapStartCount();
        float previous = edited->output[1u][kFrames - 1u], maximum = 0.0f;
        auto controls = original;
        for (unsigned frame = 0u; frame < 6000u; ++frame) {
            const float triangle = static_cast<float>(frame % 3000u) / 2999.0f;
            controls.join = frame < 3000u ? triangle : 1.0f - triangle;
            controls.wavesetProcessAmount = controls.join;
            edited->engine.process(edited->inputPointers.data(),
                edited->outputPointers.data(), 1u, controls);
            reference->engine.process(reference->inputPointers.data(),
                reference->outputPointers.data(), 1u, original);
            const auto observed = edited->engine.wavesetDisplayState();
            const auto expected = reference->engine.wavesetDisplayState();
            assert(observed.cycleOffset == expected.cycleOffset);
            assert(observed.repeatIndex == expected.repeatIndex);
            assert(std::abs(observed.oscillatorPhase - expected.oscillatorPhase) < 0.0001f);
            assert(!edited->engine.wavesetControlChangePending());
            const float value = edited->output[1u][0u];
            assert(std::isfinite(value));
            maximum = std::max(maximum, std::abs(value - previous));
            previous = value;
        }
        // Uniform-length cycles have unchanged timing even in the two
        // processes whose pitch otherwise follows the interpolated lengths.
        assert(edited->engine.wavesetGroupBuildCount() == builds);
        assert(edited->engine.wavesetOverlapStartCount() == starts);
        if (maximum >= 0.14f)
            std::fprintf(stderr, "Continuous Wavesets process %u R step %.6f\n",
                static_cast<unsigned>(process), maximum);
        assert(maximum < 0.14f);
    }
}

void testWavesetProtectionGlides()
{
    ClickFixture fixture;
    for (std::size_t frame = 0u; frame < ClickFixture::capacity; ++frame) {
        fixture.storage[0u][frame] = 0.5f * static_cast<float>(std::sin(
            6.283185307179586 * (frame % 16u) / 16.0));
        fixture.storage[1u][frame] = 0.4f + 0.25f * static_cast<float>(std::cos(
            6.283185307179586 * frame / ClickFixture::capacity));
    }
    assert(fixture.engine.adoptSource(ClickFixture::capacity, 2u, MOVE_SAMPLE_RATE));
    fixture.prepareWavesets();
    ClickEngine::Controls controls;
    controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
    controls.wavesetGroup = controls.wavesetRepeats = 1.0f;
    controls.locus = 0.0f;
    controls.field = 128.0f / ClickFixture::capacity;
    controls.join = 0.0f;
    controls.speed = 1.8f;
    fixture.renderPeak(controls, 160);
    float previous = fixture.output[1u][kFrames - 1u], maximum = 0.0f;
    float smallest = previous, largest = previous;
    for (unsigned sweep = 0u; sweep < 4u; ++sweep) {
        for (unsigned frame = 0u; frame < 6000u; ++frame) {
            const float progress = static_cast<float>(frame) / 5999.0f;
            if (sweep < 2u) controls.speed = sweep ? 2.1f - 0.3f * progress
                : 1.8f + 0.3f * progress;
            else controls.field = (sweep == 2u ? 128.0f - 96.0f * progress
                : 32.0f + 96.0f * progress) / ClickFixture::capacity;
            fixture.engine.process(fixture.inputPointers.data(),
                fixture.outputPointers.data(), 1u, controls);
            const float value = fixture.output[1u][0u];
            assert(std::isfinite(value));
            maximum = std::max(maximum, std::abs(value - previous));
            smallest = std::min(smallest, value);
            largest = std::max(largest, value);
            previous = value;
        }
    }
    // This sweep formerly hard-switched a 0.65-amplitude channel to zero.
    assert(largest - smallest > 0.3f);
    if (maximum >= 0.02f) std::fprintf(stderr, "Protection glide step %.6f\n", maximum);
    assert(maximum < 0.02f);
    assert(fixture.engine.wavesetOverlapStartCount() == 0u);
}

void testWavesetOutgoingProgression()
{
    auto edited = std::make_unique<ClickFixture>();
    auto reference = std::make_unique<ClickFixture>();
    loadWavesetStereoSeamFixture(*edited, 64u);
    loadWavesetStereoSeamFixture(*reference, 64u);
    ClickEngine::Controls original;
    original.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
    original.wavesetGroup = original.wavesetRepeats = 1.0f;
    edited->renderPeak(original, 160);
    reference->renderPeak(original, 160);
    auto changed = original;
    changed.wavesetProcess = ClickEngine::WavesetProcess::envelope;
    changed.wavesetProcessAmount = 1.0f;
    unsigned checked = 0u;
    for (unsigned frame = 0u; frame < 1800u; ++frame) {
        edited->engine.process(edited->inputPointers.data(), edited->outputPointers.data(), 1u, changed);
        reference->engine.process(reference->inputPointers.data(), reference->outputPointers.data(), 1u, original);
        const auto state = edited->engine.wavesetDisplayState();
        if (state.overlap) {
            const auto expected = reference->engine.wavesetDisplayState();
            assert(std::abs(state.secondarySourcePosition - expected.sourcePosition) < 0.00001f);
            ++checked;
        }
    }
    // An outgoing group must keep following its original progression rather
    // than repeatedly looping the captured group for the entire crossfade.
    assert(checked > 600u);
}

void testWavesetStructuralTransitionQueue()
{
    for (float cycles : {128.0f, 2.0f}) {
        ClickFixture fixture;
        fixture.loadSine(cycles);
        ClickEngine::Controls controls;
        controls.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
        controls.wavesetGroup = controls.wavesetRepeats = 1.0f;
        controls.wavesetAdvance = ClickEngine::WavesetAdvance::hold;
        controls.wavesetProcessAmount = 1.0f;
        fixture.renderPeak(controls, 160);
        const auto starts = fixture.engine.wavesetOverlapStartCount();
        controls.wavesetProcess = ClickEngine::WavesetProcess::cycleReverse;
        bool began = false, ended = false;
        unsigned overlapFrames = 0u;
        for (unsigned frame = 0u; frame < 6000u; ++frame) {
            if (began && overlapFrames == 50u) {
                controls.wavesetDirection = ClickEngine::WavesetDirection::reverse;
                controls.wavesetProcess = ClickEngine::WavesetProcess::multiply;
            } else if (began && overlapFrames == 100u) {
                controls.wavesetDirection = ClickEngine::WavesetDirection::shuffle;
                controls.wavesetProcess = ClickEngine::WavesetProcess::fractal;
            }
            fixture.engine.process(fixture.inputPointers.data(),
                fixture.outputPointers.data(), 1u, controls);
            if (!ended && fixture.engine.wavesetOverlapActive()) {
                began = true;
                ++overlapFrames;
                assert(fixture.engine.wavesetDisplayState().process
                    == ClickEngine::WavesetProcess::cycleReverse);
                assert(fixture.engine.wavesetOverlapStartCount() == starts + 1u);
            } else if (began && !ended) ended = true;
        }
        assert(began && ended);
        const auto expectedFrames = static_cast<unsigned>(MOVE_SAMPLE_RATE
            * (cycles > 2.0f ? 0.015f : 0.030f));
        assert(overlapFrames + 1u == expectedFrames);
        assert(fixture.engine.wavesetOverlapStartCount() == starts + 2u);
        assert(!fixture.engine.wavesetOverlapActive());
        assert(!fixture.engine.wavesetControlChangePending());
        assert(fixture.engine.wavesetDisplayState().process == ClickEngine::WavesetProcess::fractal);
        assert(fixture.engine.wavesetDisplayState().direction == ClickEngine::WavesetDirection::shuffle);
    }
}

// Real-time architecture regressions, independent of subjective click limits.
void testPreparedWavesetProgression()
{
    ClickFixture fixture;
    constexpr std::size_t periods[] {32u, 64u, 48u, 80u};
    std::size_t at = 0u, cycle = 0u;
    while (at < ClickFixture::capacity) {
        const auto length = periods[cycle % 4u];
        const float amplitude = 0.18f + static_cast<float>(cycle % 7u) * 0.07f;
        for (std::size_t offset = 0u; offset < length && at < ClickFixture::capacity;
             ++offset, ++at) {
            const float phase = 6.283185307179586f * offset / length;
            fixture.storage[0u][at] = amplitude * std::sin(phase);
            fixture.storage[1u][at] = amplitude * 0.7f * std::sin(phase);
        }
        ++cycle;
    }
    assert(fixture.engine.adoptSource(ClickFixture::capacity, 2u, MOVE_SAMPLE_RATE));
    fixture.prepareWavesets();
    ClickEngine::Controls control;
    control.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
    control.locus = 0.0f;
    control.wavesetGroup = 8.0f;
    control.wavesetRepeats = 1.0f;
    control.join = 0.0f;
    bool started = false;
    double previous = 0.0;
    unsigned checked = 0u;
    for (unsigned frame = 0u; frame < 5000u; ++frame) {
        fixture.engine.process(fixture.inputPointers.data(),
            fixture.outputPointers.data(), 1u, control);
        if (!fixture.engine.wavesetDisplayState().active) continue;
        const double position = fixture.engine.wavesetLanePosition(0u)
            * ClickFixture::capacity;
        if (started) {
            // Variable-length, amplitude-labelled cycles expose skipped cycles.
            if (std::abs(position - previous - 1.0) >= 0.004)
                std::fprintf(stderr, "Prepared progression at %u: %.9f -> %.9f base %zu\n",
                    frame, previous, position, fixture.engine.wavesetLaneBaseUnit(0u));
            assert(std::abs(position - previous - 1.0) < 0.004);
            if (frame > 1000u) {
                const float expected = s3g_daisy::ThruWavesetBank::read(
                    fixture.storage[0u].data(), ClickFixture::capacity, previous);
                assert(std::abs(expected - fixture.output[0u][0u]) < 0.001f);
                ++checked;
            }
        }
        previous = position;
        started = true;
    }
    assert(checked > 3000u);
    assert(fixture.engine.wavesetOverlapStartCount() == 0u);
    assert(fixture.engine.wavesetSeamCorrectionCount() == 0u);

    for (const auto advance : {ClickEngine::WavesetAdvance::stretch,
                               ClickEngine::WavesetAdvance::preserve}) {
        control.wavesetAdvance = advance;
        control.wavesetStride = 3.0f;
        control.wavesetRepeats = 2.0f;
        fixture.engine.retrigger(false);
        std::size_t oldBase = 0u;
        bool baseValid = false;
        unsigned moves = 0u;
        const auto count = fixture.wavesetBanks[0u].crossings[0u].size() - 1u;
        for (unsigned frame = 0u; frame < 16000u; ++frame) {
            fixture.engine.process(fixture.inputPointers.data(),
                fixture.outputPointers.data(), 1u, control);
            const auto base = fixture.engine.wavesetLaneBaseUnit(0u);
            if (baseValid && base != oldBase) {
                const auto step = advance == ClickEngine::WavesetAdvance::stretch ? 24u : 48u;
                assert(base == (oldBase + step) % count);
                ++moves;
            }
            oldBase = base;
            baseValid = true;
        }
        assert(moves > 5u);
    }
}

void testPreparedWavesetControlLatency()
{
    ClickFixture fixture;
    for (std::size_t frame = 0u; frame < ClickFixture::capacity; ++frame) {
        const float value = 0.6f * std::sin(6.283185307179586f * (frame % 16u) / 16.0f);
        fixture.storage[0u][frame] = fixture.storage[1u][frame] = value;
    }
    assert(fixture.engine.adoptSource(ClickFixture::capacity, 2u, MOVE_SAMPLE_RATE));
    fixture.prepareWavesets();
    ClickEngine::Controls control;
    control.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
    control.wavesetGroup = control.wavesetRepeats = 1.0f;
    control.speed = 4.0f;
    fixture.renderPeak(control, 64);
    const auto starts = fixture.engine.wavesetOverlapStartCount();
    control.wavesetProcess = ClickEngine::WavesetProcess::interpolate;
    control.wavesetProcessAmount = 0.75f;
    bool transition = false;
    unsigned committedAt = 0u;
    for (unsigned frame = 0u; frame < 1400u; ++frame) {
        fixture.engine.process(fixture.inputPointers.data(),
            fixture.outputPointers.data(), 1u, control);
        transition = transition || fixture.engine.wavesetOverlapActive();
        if (!fixture.engine.wavesetControlChangePending()
            && fixture.engine.wavesetActiveProcessAmount() == 0.75f
            && !fixture.engine.wavesetOverlapActive()) {
            committedAt = frame + 1u;
            break;
        }
    }
    assert(transition && committedAt > 0u && committedAt < 1200u);
    assert(fixture.engine.wavesetOverlapStartCount() == starts + 1u);
    control.wavesetProcessAmount = 0.25f;
    fixture.engine.process(fixture.inputPointers.data(), fixture.outputPointers.data(), 1u, control);
    for (unsigned frame = 0u; frame < 1400u; ++frame) {
        if (frame < 100u) control.wavesetProcessAmount = (frame & 8u) ? 0.4f : 0.8f;
        else control.wavesetProcessAmount = 0.2f;
        fixture.engine.process(fixture.inputPointers.data(), fixture.outputPointers.data(), 1u, control);
        assert(std::isfinite(fixture.output[0u][0u]));
    }
    assert(!fixture.engine.wavesetControlChangePending());
    assert(fixture.engine.wavesetActiveProcessAmount() == 0.2f);
    assert(!fixture.engine.wavesetOverlapActive());
    assert(fixture.engine.wavesetOverlapStartCount() == starts + 1u);
}

void testPreparedWavesetLaneReaders()
{
    ClickFixture fixture;
    fixture.loadWavesetStack();
    ClickEngine::Controls control;
    control.algorithm = ClickEngine::PlaybackAlgorithm::wavesets;
    control.wavesetGroup = control.wavesetRepeats = 1.0f;
    control.wavesetAdvance = ClickEngine::WavesetAdvance::hold;
    control.lanePath = ClickEngine::LanePath::manual;
    control.manualLane = 0.5f;
    control.laneScanPaused = true;
    fixture.renderPeak(control, 64);
    assert(fixture.engine.wavesetLanePosition(0u) >= 0.0f);
    assert(fixture.engine.wavesetLanePosition(1u) >= 0.0f);
    assert(std::abs(fixture.engine.wavesetLanePosition(0u)
        - fixture.engine.wavesetLanePosition(1u)) > 0.001f);
    const auto analyzed = fixture.wavesetBanks[0u].analyzedFrames;
    auto voices = std::make_unique<std::array<ClickFixture, 8u>>();
    for (auto& voice : *voices) {
        voice.loadWavesetStack();
        for (std::size_t lane = 0u; lane < 4u; ++lane)
            voice.engine.setPreparedWavesetBank(lane, &fixture.wavesetBanks[lane]);
    }
    control.wavesetAdvance = ClickEngine::WavesetAdvance::stretch;
    control.wavesetGroup = 8.0f;
    for (int block = 0; block < 128; ++block) {
        for (std::size_t voice = 0u; voice < voices->size(); ++voice) {
            control.speed = std::pow(2.0f, static_cast<float>(voice) / 12.0f);
            auto& current = (*voices)[voice];
            current.engine.process(current.inputPointers.data(), current.outputPointers.data(),
                kFrames, control);
            assert(std::isfinite(current.output[0u][0u]));
            assert(current.engine.wavesetOverlapStartCount() == 0u);
            assert(!current.engine.wavesetFallbackActive());
        }
    }
    assert(fixture.wavesetBanks[0u].analyzedFrames == analyzed);
    assert((*voices)[0u].engine.wavesetLanePosition(0u)
        != (*voices)[7u].engine.wavesetLanePosition(0u));
}

void testWavesetSnapshotWorker()
{
    using Worker = s3g_daisy::ThruWavesetWorker;
    constexpr std::size_t size = 1024u;
    std::array<float, size> left {}, right {};
    for (std::size_t frame = 0u; frame < size; ++frame)
        left[frame] = right[frame] = 0.5f * std::sin(6.283185307179586f * (frame % 64u) / 64.0f);
    Worker worker;
    assert(worker.prepare(size));
    const auto wait = [&] {
        for (int attempt = 0; attempt < 1000 && worker.state() != Worker::complete; ++attempt)
            usleep(1000);
        assert(worker.state() == Worker::complete);
    };
    worker.begin(0u, 1u, size, MOVE_SAMPLE_RATE);
    std::size_t copied = 0u;
    while (worker.state() == Worker::copying) {
        const auto count = worker.copy(left.data(), right.data(), 127u);
        assert(count <= 127u);
        copied += count;
    }
    assert(copied == size);
    // The worker must see its immutable snapshot even if the source changes.
    left.fill(0.5f); right.fill(0.5f);
    wait();
    assert(worker.accept(true));
    const auto* first = worker.bank(0u);
    assert(first && first->revision == 1u);
    assert(first->crossings[0u].size() > 10u);
    assert(first->analyzedFrames == size - 1u);
    worker.releaseResult();
    worker.begin(0u, 2u, size, MOVE_SAMPLE_RATE);
    assert(worker.copy(left.data(), right.data(), 32u) == 32u);
    worker.cancelCopy();
    assert(worker.state() == Worker::idle);
    worker.begin(0u, 3u, size, MOVE_SAMPLE_RATE);
    assert(worker.copy(left.data(), right.data(), size) == size);
    wait();
    assert(!worker.accept(false)); // Source revision changed before publication.
    assert(worker.bank(0u) == first);
    worker.releaseResult();
    worker.stop();
}
