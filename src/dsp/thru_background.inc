    enum BackgroundJob { backgroundIdle, backgroundCopying, backgroundRequested,
                         backgroundWorking, backgroundComplete };
    struct PersistenceSnapshot {
        std::array<ClipSlot, kClipSlotCount> clips;
        std::array<ClipChainEntry, kClipChainLength> chain;
        std::array<char, kStateTextSize> json {};
        std::uint64_t revision = 0u, generation = 0u;
        float tempo = 120.0f, swing = 0.5f;
        LaunchQuantize quantize = LaunchQuantize::off;
        std::size_t grainLimit = 8u;
        int active = 0, chainPosition = 0, chainEdit = 0;
        bool success = false;
    };
    struct LaneSnapshot {
        std::size_t lane = 0u, frames = 0u, copied = 0u;
        std::uint64_t revision = 0u;
        float rate = 44100.0f;
        bool archive = false, success = false, collision = false;
        int clip = -1;
        std::array<char, kPersistencePathSize> path {};
        RegionMap regions;
        WaveBins minimum {}, maximum {};
    };
    std::unique_ptr<PersistenceSnapshot> persistenceSnapshot;
    std::unique_ptr<float[]> backgroundAudio;
    LaneSnapshot laneSnapshot;
    pthread_t backgroundThread {};
    std::atomic<bool> backgroundRunning {false};
    std::atomic<int> persistenceJob {backgroundIdle}, laneJob {backgroundIdle};
    bool backgroundStarted = false, maintenanceRequested = false;
    std::uint64_t persistentRevision = 1u, lastPersistentEditFrame = 0u;
    std::size_t persistenceCopyOffset = 0u;
    std::uint64_t nextPersistenceGeneration = 1u;
    std::uint64_t laneRetryFrame = 0u, saveRetryFrame = 0u;
    bool recordStartPending = false;
    std::size_t pendingRecordLane = 0u;
    std::atomic<std::uint64_t> maximumControlMicros {0u}, maximumQueryMicros {0u};
    std::atomic<std::uint64_t> maximumWorkMicros {0u}, workOverruns {0u};
    std::atomic<std::uint64_t> pendingCallbackNanos {0u};

    void observePersistenceGeneration(std::uint64_t generation)
    {
        const auto next = generation == ~std::uint64_t {0u} ? generation : generation + 1u;
        nextPersistenceGeneration = std::max(nextPersistenceGeneration, next);
    }

    std::uint64_t reservePersistenceGeneration()
    {
        if (nextPersistenceGeneration == ~std::uint64_t {0u}) {
            setError("Saved generation counter exhausted");
            return 0u;
        }
        return nextPersistenceGeneration++;
    }

    static void* backgroundEntry(void* opaque)
    {
        auto& self = *static_cast<ThruInstance*>(opaque);
        s3g_daisy::configureBackgroundThread();
        while (self.backgroundRunning.load(std::memory_order_acquire)
            || self.laneJob.load(std::memory_order_acquire) == backgroundRequested
            || self.persistenceJob.load(std::memory_order_acquire) == backgroundRequested) {
            if (self.laneJob.load(std::memory_order_acquire) == backgroundRequested) {
                self.laneJob.store(backgroundWorking, std::memory_order_release);
                auto& job = self.laneSnapshot;
                const float* left = self.backgroundAudio.get();
                const float* right = left + self.laneCapacityFrames;
                analyzeRegions(left, right, job.frames, job.rate, job.regions);
                analyzeWaveform(left, right, job.frames, job.minimum, job.maximum);
                job.success = true;
                if (job.archive) {
                    std::array<char, kPersistencePathSize> temporary {};
                    const int length = std::snprintf(temporary.data(), temporary.size(),
                        "%s.tmp", job.path.data());
                    job.success = length > 0 && static_cast<std::size_t>(length) < temporary.size()
                        && writeStereoWavFile(left, right, job.frames, job.rate,
                            job.path.data(), temporary.data(), true, &job.collision);
                }
                self.laneJob.store(backgroundComplete, std::memory_order_release);
                continue;
            }
            if (self.persistenceJob.load(std::memory_order_acquire) == backgroundRequested) {
                self.persistenceJob.store(backgroundWorking, std::memory_order_release);
                auto& job = *self.persistenceSnapshot;
                job.success = self.writeMetadataFiles(&job);
                self.persistenceJob.store(backgroundComplete, std::memory_order_release);
                continue;
            }
            const timespec delay {0, 1000000};
            nanosleep(&delay, nullptr);
        }
        return nullptr;
    }

    bool startBackgroundWork()
    {
        backgroundAudio.reset(new (std::nothrow) float[laneCapacityFrames * 2u]);
        if (persistenceEnabled) persistenceSnapshot.reset(new (std::nothrow) PersistenceSnapshot);
        if (!backgroundAudio || (persistenceEnabled && !persistenceSnapshot)) return false;
        backgroundRunning.store(true, std::memory_order_release);
        if (!s3g_daisy::startBackgroundThread(backgroundThread, backgroundEntry, this)) {
            backgroundRunning.store(false, std::memory_order_release);
            return false;
        }
        backgroundStarted = true;
        return true;
    }

    void stopBackgroundWork()
    {
        if (!backgroundStarted) return;
        backgroundRunning.store(false, std::memory_order_release);
        pthread_join(backgroundThread, nullptr);
        backgroundStarted = false;
        consumeBackgroundResults();
    }

    void consumeBackgroundResults()
    {
        if (laneJob.load(std::memory_order_acquire) == backgroundComplete) {
            const auto& job = laneSnapshot;
            const bool current = job.revision == wavesetSourceRevisions[job.lane]
                && engine.laneFrames(job.lane) == job.frames
                && (recordingLaneMask & (1u << job.lane)) == 0u
                && (pendingClearMask & (1u << job.lane)) == 0u;
            if (current) {
                installRegionMap(job.lane, job.regions, false);
                pendingRegionAnalysisMask &= ~(1u << job.lane);
                waveMinimum[job.lane] = job.minimum;
                waveMaximum[job.lane] = job.maximum;
                waveDirtyMask &= ~(1u << job.lane);
                waveTextDirty = true;
                if (job.archive && job.success
                    && pendingCaptureClips[job.lane] == job.clip) {
                    pendingCaptureSaveMask &= ~(1u << job.lane);
                    pendingCaptureClips[job.lane] = -1;
                }
            }
            if (!job.success) {
                if (current && job.archive && job.collision) {
                    // A restored counter or external file may collide. Allocate
                    // another monotonic name without querying storage here.
                    std::array<char, kPersistencePathSize> path {};
                    const char* suffix = std::strrchr(job.path.data(), '-');
                    std::array<char, 32u> kind {};
                    const char* extension = suffix ? std::strrchr(suffix, '.') : nullptr;
                    std::snprintf(kind.data(), kind.size(), "%.*s",
                        extension ? static_cast<int>(extension - suffix - 1) : 5,
                        suffix ? suffix + 1 : "retry");
                    std::uint64_t id = 0u;
                    if (allocateCapturePath(job.clip, job.lane, kind.data(), path.data(), path.size(), &id)) {
                        currentLaneSourcePaths[job.lane] = path;
                        clipSlots[static_cast<std::size_t>(job.clip)].laneSourcePaths[job.lane] = path;
                        if (std::strncmp(laneSourceLabels[job.lane].data(), "TAKE ", 5u) == 0) {
                            std::snprintf(laneSourceLabels[job.lane].data(), laneSourceLabels[job.lane].size(),
                                "TAKE %010llu", static_cast<unsigned long long>(id));
                            clipSlots[static_cast<std::size_t>(job.clip)].laneSourceLabels[job.lane]
                                = laneSourceLabels[job.lane];
                        }
                        markPersistentStateDirty();
                    }
                } else {
                    laneRetryFrame = renderClockSamples.load(std::memory_order_relaxed)
                        + static_cast<std::uint64_t>(sampleRate * 2.0f);
                    setError("Recording save failed; take retained in memory");
                }
            }
            laneJob.store(backgroundIdle, std::memory_order_release);
        }
        if (persistenceJob.load(std::memory_order_acquire) == backgroundComplete) {
            const auto& job = *persistenceSnapshot;
            if (job.success) {
                persistenceGeneration = std::max(persistenceGeneration, job.generation);
                if (job.revision == persistentRevision) persistentStateDirty = false;
            } else {
                setError("Background save failed; previous saved set retained");
                saveRetryFrame = renderClockSamples.load(std::memory_order_relaxed)
                    + static_cast<std::uint64_t>(sampleRate * 2.0f);
            }
            persistenceJob.store(backgroundIdle, std::memory_order_release);
        }
    }

    void serviceBackgroundWork()
    {
        if (!backgroundStarted) return;
        if (clipFinishPending && clipRecording) finishClipRecording();
        consumeBackgroundResults();
        auto state = laneJob.load(std::memory_order_acquire);
        if (state == backgroundCopying
            && (laneSnapshot.revision != wavesetSourceRevisions[laneSnapshot.lane]
                || engine.laneFrames(laneSnapshot.lane) != laneSnapshot.frames
                || (recordingLaneMask & (1u << laneSnapshot.lane)) != 0u
                || (pendingClearMask & (1u << laneSnapshot.lane)) != 0u)) {
            laneJob.store(backgroundIdle, std::memory_order_release);
            state = backgroundIdle;
        }
        if (state == backgroundIdle
            && renderClockSamples.load(std::memory_order_relaxed) >= laneRetryFrame) {
            const std::uint32_t wanted = pendingCaptureSaveMask != 0u
                ? pendingCaptureSaveMask : (waveDirtyMask | pendingRegionAnalysisMask);
            for (std::size_t lane = 0u; lane < kLaneCount; ++lane) {
                const auto bit = 1u << lane;
                if ((wanted & bit) == 0u || (recordingLaneMask & bit) != 0u
                    || (pendingClearMask & bit) != 0u) continue;
                if (!engine.laneLoaded(lane)) {
                    waveMinimum[lane].fill(0); waveMaximum[lane].fill(0);
                    waveDirtyMask &= ~bit; pendingRegionAnalysisMask &= ~bit;
                    waveTextDirty = true;
                    continue;
                }
                laneSnapshot.lane = lane;
                laneSnapshot.frames = engine.laneFrames(lane);
                laneSnapshot.rate = engine.laneSampleRate(lane);
                laneSnapshot.revision = wavesetSourceRevisions[lane];
                laneSnapshot.copied = 0u;
                laneSnapshot.archive = persistenceEnabled && (pendingCaptureSaveMask & bit) != 0u;
                laneSnapshot.collision = false;
                laneSnapshot.clip = pendingCaptureClips[lane];
                laneSnapshot.path = currentLaneSourcePaths[lane];
                laneJob.store(backgroundCopying, std::memory_order_release);
                state = backgroundCopying;
                break;
            }
        }
        if (state == backgroundCopying) {
            auto& job = laneSnapshot;
            const std::size_t count = std::min<std::size_t>(4096u, job.frames - job.copied);
            std::copy_n(engine.laneData(job.lane, 0u) + job.copied, count,
                backgroundAudio.get() + job.copied);
            std::copy_n(engine.laneData(job.lane, 1u) + job.copied, count,
                backgroundAudio.get() + laneCapacityFrames + job.copied);
            job.copied += count;
            if (job.copied == job.frames) laneJob.store(backgroundRequested, std::memory_order_release);
        }
        // Never snapshot a bank while its notes/curves are being recorded or
        // before referenced archive files exist. Restart stale copies, retaining
        // the newest live state rather than publishing a torn generation.
        auto saveState = persistenceJob.load(std::memory_order_acquire);
        if (saveState == backgroundCopying
            && (persistenceSnapshot->revision != persistentRevision
                || clipRecording || engine.recording() || pendingCaptureSaveMask != 0u)) {
            persistenceJob.store(backgroundIdle, std::memory_order_release);
            saveState = backgroundIdle;
        }
        const auto clock = renderClockSamples.load(std::memory_order_relaxed);
        if (saveState == backgroundIdle && persistenceSnapshot && maintenanceRequested
            && persistentStateDirty && !clipRecording && !engine.recording()
            && pendingCaptureSaveMask == 0u
            && clock >= saveRetryFrame
            && clock - lastPersistentEditFrame >= static_cast<std::uint64_t>(sampleRate * 0.100f)) {
            captureActiveClip(false);
            auto& job = *persistenceSnapshot;
            job.revision = persistentRevision; job.generation = reservePersistenceGeneration();
            job.chain = clipChain; job.tempo = tempoBpm; job.swing = tempoSwing;
            job.quantize = launchQuantize; job.grainLimit = grainVoiceLimit;
            job.active = activeClip; job.chainPosition = chainPosition; job.chainEdit = chainEditSlot;
            const int length = serializeState(job.json.data(), job.json.size(), job.generation);
            if (job.generation != 0u && length > 0 && static_cast<std::size_t>(length) < job.json.size()) {
                persistenceCopyOffset = 0u;
                persistenceJob.store(backgroundCopying, std::memory_order_release);
                saveState = backgroundCopying;
            } else setError("Background save snapshot could not be serialized");
        }
        if (saveState == backgroundCopying) {
            static_assert(std::is_trivially_copyable<ClipSlot>::value,
                "background clip snapshots require plain owned value storage");
            // Fixed 16 KiB maximum per render callback, independent of how
            // many clips/points are stored. No allocation, locks or file I/O.
            const std::size_t count = std::min<std::size_t>(16384u,
                sizeof(clipSlots) - persistenceCopyOffset);
            std::memcpy(reinterpret_cast<char*>(persistenceSnapshot->clips.data()) + persistenceCopyOffset,
                reinterpret_cast<const char*>(clipSlots.data()) + persistenceCopyOffset, count);
            persistenceCopyOffset += count;
            if (persistenceCopyOffset == sizeof(clipSlots))
                persistenceJob.store(backgroundRequested, std::memory_order_release);
        }
        if (waveTextDirty) rebuildWaveText();
        if (recordStartPending && pendingCaptureSaveMask == 0u) {
            recordStartPending = false;
            selectedLane = pendingRecordLane;
            startRecording();
        }
    }

    bool serviceMaintenance()
    {
        maintenanceRequested = true;
        return pendingCaptureSaveMask == 0u;
    }
