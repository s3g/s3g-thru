void removeBackgroundTestDirectory(const std::string& path)
{
    for (const auto& folder : {path + "/captures", path}) {
        DIR* directory = opendir(folder.c_str());
        assert(directory);
        while (const dirent* entry = readdir(directory)) {
            if (entry->d_name[0] == '.') continue;
            const auto file = folder + "/" + entry->d_name;
            struct stat status {};
            assert(stat(file.c_str(), &status) == 0);
            if (S_ISREG(status.st_mode)) assert(unlink(file.c_str()) == 0);
        }
        closedir(directory);
        assert(rmdir(folder.c_str()) == 0);
    }
}

void resumeTestBackgroundWorker(ThruInstance& instance)
{
    instance.backgroundRunning.store(true, std::memory_order_release);
    assert(s3g_daisy::startBackgroundThread(instance.backgroundThread,
        ThruInstance::backgroundEntry, &instance));
    instance.backgroundStarted = true;
}

void waitForTestBackground(ThruInstance& instance, bool metadata)
{
    std::array<std::int16_t, kBlockFrames * 2u> output {};
    for (int attempt = 0; attempt < 6000; ++attempt) {
        renderBlock(&instance, output.data(), kBlockFrames);
        if (instance.pendingCaptureSaveMask == 0u && instance.waveDirtyMask == 0u
            && instance.laneJob.load(std::memory_order_acquire) == ThruInstance::backgroundIdle
            && (!metadata || (!instance.persistentStateDirty
                && instance.persistenceJob.load(std::memory_order_acquire) == ThruInstance::backgroundIdle))) return;
        usleep(500);
    }
    std::fprintf(stderr, "Background job failed to settle: %s\n", instance.lastError.data());
    assert(false);
}

void testBoundedBackgroundPersistence()
{
    char path[] = "/tmp/thru-background-save.XXXXXX";
    assert(mkdtemp(path));
    auto instance = std::make_unique<ThruInstance>();
    assert(instance->prepare(path));
    // Hold the worker stopped while real callback entry points transfer the
    // snapshot. Any synchronous writer here would create files before resume.
    instance->stopBackgroundWork();
    instance->backgroundStarted = true;
    for (std::size_t index = 1u; index <= 4u; ++index) {
        auto& clip = instance->clipSlots[index];
        clip.occupied = clip.clipRecorded = true;
        clip.takeCount = 1u;
        clip.clipDurationMs = 16000u;
        clip.clipPointCount = 4000u;
        for (std::size_t point = 0u; point < clip.clipPointCount; ++point) {
            clip.clipPoints[point].timeMs = point * 4u;
            clip.clipPoints[point].value = 0.25f;
            clip.clipPoints[point].parameter = static_cast<std::uint16_t>(LockParameter::effectDrive);
        }
    }
    instance->chainBanks[15].entries[63].enabled=true;instance->chainBanks[15].entries[63].clip=6;
    setParam(instance.get(), "effect_drive", "0.25");
    setParam(instance.get(), "maintenance", "1");
    std::array<std::int16_t, kBlockFrames * 2u> output {};
    const std::string clips = std::string(path) + "/thru-clips.bin";
    for (int block = 0; block < 2000
         && instance->persistenceJob.load() != ThruInstance::backgroundRequested; ++block) {
        const auto previous = instance->persistenceCopyOffset;
        renderBlock(instance.get(), output.data(), kBlockFrames);
        assert(instance->persistenceCopyOffset - previous <= 16384u);
        assert(access(clips.c_str(), F_OK) != 0);
        assert(access((clips + ".tmp").c_str(), F_OK) != 0);
    }
    assert(instance->persistenceJob.load() == ThruInstance::backgroundRequested);
    assert(instance->persistenceCopyOffset == sizeof(instance->clipSlots)+sizeof(instance->chainBanks));

    // The worker must save only its published image, even if live data is
    // edited before it begins. Its acknowledgement cannot clear newer edits.
    instance->chainBanks[15].entries[63].clip=7;
    setParam(instance.get(), "effect_drive", "0.75");
    instance->clipSlots[1u].clipPoints[0u].value = 0.75f;
    resumeTestBackgroundWorker(*instance);
    for (int attempt = 0; attempt < 4000
         && instance->persistenceJob.load(std::memory_order_acquire)
            != ThruInstance::backgroundComplete; ++attempt) usleep(500);
    assert(instance->persistenceJob.load() == ThruInstance::backgroundComplete);
    assert(instance->persistenceSnapshot->success);
    auto restored = std::make_unique<ThruInstance>();
    assert(restored->prepare(path));
    assert(restored->controls.effectDrive == 0.25f);
    assert(restored->chainBanks[15].entries[63].clip==6);
    assert(restored->clipSlots[1u].clipPointCount == 4000u);
    assert(restored->clipSlots[1u].clipPoints[0u].value == 0.25f);
    restored.reset();
    instance->consumeBackgroundResults();
    assert(instance->persistentStateDirty);
    waitForTestBackground(*instance, true);
    restored = std::make_unique<ThruInstance>();
    assert(restored->prepare(path));
    assert(restored->controls.effectDrive == 0.75f);
    assert(restored->chainBanks[15].entries[63].clip==7);
    assert(restored->clipSlots[1u].clipPoints[0u].value == 0.75f);
    assert(restored->persistenceGeneration == 2u);
    restored.reset();
    // Failure staging one file must leave the matching current pair and its
    // backup untouched, even though the other temporary file was written.
    instance->stopBackgroundWork(); instance->backgroundStarted = true;
    const auto blocked = std::string(path) + "/thru-state.json.tmp";
    assert(mkdir(blocked.c_str(), 0700) == 0);
    setParam(instance.get(), "effect_drive", "0.90");
    for (int attempt = 0; attempt < 5000
         && instance->persistenceJob.load() != ThruInstance::backgroundRequested; ++attempt)
        renderBlock(instance.get(), output.data(), kBlockFrames);
    assert(instance->persistenceJob.load() == ThruInstance::backgroundRequested);
    resumeTestBackgroundWorker(*instance);
    for (int attempt = 0; attempt < 4000
         && instance->persistenceJob.load(std::memory_order_acquire)
            != ThruInstance::backgroundComplete; ++attempt) usleep(500);
    assert(instance->persistenceJob.load() == ThruInstance::backgroundComplete);
    assert(!instance->persistenceSnapshot->success);
    restored = std::make_unique<ThruInstance>();
    assert(restored->prepare(path));
    assert(restored->controls.effectDrive == 0.75f);
    assert(restored->persistenceGeneration == 2u);
    restored.reset();
    instance->consumeBackgroundResults();
    assert(instance->persistentStateDirty);
    assert(rmdir(blocked.c_str()) == 0);
    instance->saveRetryFrame = 0u;
    waitForTestBackground(*instance, true);
    instance.reset();
    // A valid newer JSON without its matching bank must not make recovery
    // reuse that generation for an older state.
    restored = std::make_unique<ThruInstance>(); assert(restored->prepare(path));
    std::array<char, kStateTextSize> future {};
    const int length = restored->serializeState(future.data(), future.size(), 999u);
    const auto statePath = std::string(path) + "/thru-state.json";
    auto* file = std::fopen(statePath.c_str(), "wb"); assert(file);
    assert(std::fwrite(future.data(), 1u, length, file) == static_cast<std::size_t>(length));
    std::fclose(file); restored.reset();
    restored = std::make_unique<ThruInstance>(); assert(restored->prepare(path));
    assert(restored->persistenceGeneration == 2u);
    assert(restored->nextPersistenceGeneration == 1000u);
    assert(restored->controls.effectDrive == 0.75f);
    assert(restored->savePersistentState());
    assert(restored->persistenceGeneration == 1000u);
    restored.reset();
    removeBackgroundTestDirectory(path);
}

void testBackgroundAnalysisOwnership()
{
    auto instance = std::make_unique<ThruInstance>();
    assert(instance->prepare(nullptr));
    instance->stopBackgroundWork(); instance->backgroundStarted = true;
    constexpr std::size_t frames = 16384u;
    for (std::size_t channel = 0u; channel < 2u; ++channel)
        std::fill_n(instance->engine.writableLane(0u, channel), frames, 0.2f);
    assert(instance->engine.adoptLane(0u, frames, 44100.0f));
    instance->markWaveformDirty(0u);
    std::array<char, 2048u> before {}, after {};
    getParam(instance.get(), "waveforms", before.data(), before.size());
    for (int read = 0; read < 100; ++read)
        getParam(instance.get(), "waveforms", after.data(), after.size());
    assert(before == after);
    assert(instance->laneJob.load() == ThruInstance::backgroundIdle);
    assert(instance->laneSnapshot.copied == 0u);
    std::array<std::int16_t, kBlockFrames * 2u> output {};
    for (int block = 0; block < 4; ++block) {
        const auto previous = instance->laneSnapshot.copied;
        renderBlock(instance.get(), output.data(), kBlockFrames);
        assert(instance->laneSnapshot.copied - previous <= 4096u);
    }
    assert(instance->laneJob.load() == ThruInstance::backgroundRequested);
    for (std::size_t channel = 0u; channel < 2u; ++channel)
        std::fill_n(instance->engine.writableLane(0u, channel), frames, 0.75f);
    instance->markWaveformDirty(0u);
    resumeTestBackgroundWorker(*instance);
    for (int attempt = 0; attempt < 4000 && instance->laneJob.load(std::memory_order_acquire)
        != ThruInstance::backgroundComplete; ++attempt) usleep(500);
    assert(instance->laneJob.load() == ThruInstance::backgroundComplete);
    instance->consumeBackgroundResults();
    getParam(instance.get(), "waveforms", after.data(), after.size());
    assert(before == after); // stale map was not published
    assert(instance->waveDirtyMask != 0u);
    waitForTestBackground(*instance, false);
    getParam(instance.get(), "waveforms", after.data(), after.size());
    assert(before != after);
    assert(instance->engine.laneFrames(0u) == frames);
}

void testBackgroundArchiveAndQueuedRecording()
{
    char path[] = "/tmp/thru-background-record.XXXXXX";
    assert(mkdtemp(path));
    assert(mkdir((std::string(path) + "/captures").c_str(), 0700) == 0);
    const auto collision = std::string(path) + "/captures/t0000000001-c01-l1-input.wav";
    auto* sentinel = std::fopen(collision.c_str(), "wb"); assert(sentinel);
    assert(std::fwrite("KEEP", 1u, 4u, sentinel) == 4u); std::fclose(sentinel);
    auto instance = std::make_unique<ThruInstance>(); assert(instance->prepare(path));
    assert(instance->nextCaptureId == 2u); // archive scan survives an old JSON counter
    instance->nextCaptureId = 1u; // force a live collision to test no-replace publication
    constexpr std::size_t frames = 32768u;
    for (std::size_t channel = 0u; channel < 2u; ++channel)
        std::fill_n(instance->engine.writableLane(0u, channel), frames,
            channel == 0u ? 0.2f : -0.35f);
    assert(instance->engine.adoptLane(0u, frames, 44100.0f));
    instance->completedRecordingLane(0u);
    setParam(instance.get(), "record_gate", "1");
    assert(instance->recordStartPending);
    assert(!instance->engine.recording());
    std::array<std::int16_t, kBlockFrames * 2u> output {};
    for (int attempt = 0; attempt < 4000 && !instance->engine.recording(); ++attempt) {
        renderBlock(instance.get(), output.data(), kBlockFrames); usleep(500);
    }
    assert(instance->engine.recording());
    assert(!instance->recordStartPending);
    const auto archived = std::string(instance->currentLaneSourcePaths[0u].data());
    assert(archived.find("t0000000002-") != std::string::npos);
    auto* wav = std::fopen(archived.c_str(), "rb"); assert(wav);
    assert(std::fseek(wav, 44l, SEEK_SET) == 0);
    std::array<float, 2u> first {};
    assert(std::fread(first.data(), sizeof(float), 2u, wav) == 2u);
    assert(first[0u] == 0.2f && first[1u] == -0.35f); std::fclose(wav);
    for (int block = 0; block < 8; ++block)
        renderBlock(instance.get(), output.data(), kBlockFrames);
    setParam(instance.get(), "record_gate", "0");
    waitForTestBackground(*instance, false);
    const auto second = std::string(instance->currentLaneSourcePaths[0u].data());
    assert(second != archived && second != collision);
    assert(access(second.c_str(), F_OK) == 0);
    sentinel = std::fopen(collision.c_str(), "rb"); assert(sentinel);
    char text[4]; assert(std::fread(text, 1u, 4u, sentinel) == 4u);
    assert(std::memcmp(text, "KEEP", 4u) == 0); std::fclose(sentinel);
    waitForTestBackground(*instance, true);
    const auto expectedFrames = instance->engine.laneFrames(0u);
    instance.reset();
    // Simulate re-entry using the live background save only: no final legacy
    // lane-file/teardown flush has run. The archived source must reload itself.
    instance = std::make_unique<ThruInstance>(); assert(instance->prepare(path));
    for (int attempt = 0; attempt < 4000 && !instance->engine.laneLoaded(0u); ++attempt) {
        renderBlock(instance.get(), output.data(), kBlockFrames); usleep(500);
    }
    assert(instance->engine.laneLoaded(0u));
    assert(instance->engine.laneFrames(0u) == expectedFrames);
    assert(std::string(instance->currentLaneSourcePaths[0u].data()) == second);
    instance.reset();
    removeBackgroundTestDirectory(path);
}

void testCallbackWorkTelemetry()
{
    auto instance = std::make_unique<ThruInstance>(); assert(instance->prepare(nullptr));
    setParam(instance.get(), "performance_reset", "1");
    {
        CallbackWorkTimer timer(*instance, false);
        usleep(6000); // deterministic simulated slow control operation, outside render
    }
    std::array<std::int16_t, kBlockFrames * 2u> output {};
    renderBlock(instance.get(), output.data(), kBlockFrames);
    assert(instance->maximumControlMicros.load() >= 6000u);
    assert(instance->workOverruns.load() >= 1u);
    assert(instance->maximumWorkMicros.load() >= 6000u);
    std::array<char, 128u> report {};
    assert(getParam(instance.get(), "callback_performance_state", report.data(), report.size()) > 0);
    setParam(instance.get(), "performance_reset", "1");
    assert(instance->workOverruns.load() == 0u);
    assert(instance->maximumWorkMicros.load() == 0u);
    {
        CallbackWorkTimer timer(*instance, true);
        usleep(4000);
    }
    renderBlock(instance.get(), output.data(), kBlockFrames);
    assert(instance->maximumQueryMicros.load() >= 4000u);
    assert(instance->workOverruns.load() >= 1u);
}

void testAntialiasedDrive()
{
    s3g_daisy::ThruDrive drive;
    std::array<float, 512u> history {};
    for (std::size_t frame = 0u; frame < history.size(); ++frame) {
        history[frame] = 0.7f * std::sin(static_cast<float>(frame * frame) * 0.13f);
        const auto output = drive.process({history[frame], -history[frame]}, 0.0f);
        const float expected = frame < drive.latencyFrames ? 0.0f : history[frame - drive.latencyFrames];
        assert(output[0u] == expected && output[1u] == -expected);
    }
    drive.reset();
    constexpr double pi = 3.14159265358979323846;
    constexpr std::size_t count = 14700u;
    double rawReal = 0.0, rawImag = 0.0, filteredReal = 0.0, filteredImag = 0.0;
    for (std::size_t frame = 0u; frame < count * 2u; ++frame) {
        const float input = 0.3f * std::sin(2.0 * pi * 20.0 * frame / 147.0);
        const float raw = input * 13.0f / (1.0f + 12.0f * std::abs(input));
        const auto output = drive.process({input, -input}, 12.0f);
        assert(output[1u] == -output[0u]);
        if (frame < count) continue;
        const double phase = 2.0 * pi * 47.0 * frame / 147.0;
        rawReal += raw * std::cos(phase); rawImag += raw * std::sin(phase);
        filteredReal += output[0u] * std::cos(phase); filteredImag += output[0u] * std::sin(phase);
    }
    const double rawAlias = std::hypot(rawReal, rawImag);
    const double filteredAlias = std::hypot(filteredReal, filteredImag);
    assert(rawAlias > 1.0);
    assert(filteredAlias < rawAlias * 0.01); // at least 40 dB suppression
    // Fine control changes at a held DC level must also remain continuous.
    drive.reset(); float previous = 0.0f;
    for (std::size_t frame = 0u; frame < 16000u; ++frame) {
        const float amount = frame < 5000u ? 0.0f
            : (frame < 11000u ? static_cast<float>(frame - 5000u) / 500.0f : 12.0f);
        const float output = drive.process({0.2f, 0.2f}, amount)[0u];
        if (frame > 100u) assert(std::abs(output - previous) < 0.005f);
        previous = output;
    }
}

void testRenderCpuAndWaitTelemetry()
{
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(nullptr));
    setParam(t.get(),"performance_reset","1");
    recordRenderTiming(*t,kBlockFrames,3304000,39000,true,1800000);
    assert(t->maximumRenderMicros.load()==3304&&t->maximumWorkMicros.load()==3343);
    assert(t->renderOverruns.load()==1&&t->workOverruns.load()==1&&t->renderCpuOverruns.load()==0);
    assert(t->maximumRenderCpuMicros.load()==1800&&t->maximumRenderWaitMicros.load()==1504);
    char text[128]{};assert(getParam(t.get(),"render_timing_state",text,sizeof(text))>0);
    assert(std::strcmp(text,"1,1800,1504,0")==0);
    recordRenderTiming(*t,kBlockFrames,3400000,0,true,3200000);
    assert(t->renderCpuOverruns.load()==1&&t->maximumRenderCpuMicros.load()==3200);
    setParam(t.get(),"performance_reset","1");
    assert(t->maximumRenderCpuMicros.load()==0&&t->maximumRenderWaitMicros.load()==0&&t->renderCpuOverruns.load()==0&&!t->renderCpuClockAvailable.load());
    // The displayed ceil(2902.494 us) must not create a false WORK MISS.
    const auto below=static_cast<std::uint64_t>(kBlockFrames*1000000000.0/t->sampleRate);
    recordRenderTiming(*t,kBlockFrames,below,0,true,below);
    assert(t->renderOverruns.load()==0&&t->workOverruns.load()==0&&t->renderCpuOverruns.load()==0);
    recordRenderTiming(*t,kBlockFrames,below,1,false,0);
    assert(t->renderOverruns.load()==0&&t->workOverruns.load()==1&&!t->renderCpuClockAvailable.load());
    setParam(t.get(),"performance_reset","1");
    std::array<std::int16_t,kBlockFrames*2> audio{};renderBlock(t.get(),audio.data(),kBlockFrames);
    timespec cpu{};assert(t->renderCpuClockAvailable.load()==readRenderCpuClock(cpu));
}
