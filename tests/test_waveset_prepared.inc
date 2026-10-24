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
    assert(fixture.engine.wavesetOverlapStartCount() <= starts + 4u);
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
