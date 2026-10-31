bool spectralRequested() const {
    const auto spectral = [](Engine::PlaybackAlgorithm a) {
        return a == Engine::PlaybackAlgorithm::prism || a == Engine::PlaybackAlgorithm::morph || a == Engine::PlaybackAlgorithm::formant;
    };
    if (spectral(controls.algorithm))
        return true;
    if (splitEnabled)
        for (int part = 0; part < partCount(); ++part)
            if (spectral(liveControlsForPart(part).algorithm))
                return true;
    for (const auto &voice : voices)
        if (voice.active && spectral(voice.engine.playbackAlgorithm()))
            return true;
    return false;
}
void serviceSpectralAnalysis() {
    using Worker = s3g_daisy::ThruSpectralWorker;
    const bool needed = spectralRequested();
    auto state = spectralWorker.state();
    if (!needed && state == Worker::idle) {
        spectralPendingMask.store(0);
        return;
    }
    const auto available = engine.loadedLaneMask() & ~recordingLaneMask & ~pendingClearMask;
    if (state == Worker::complete) {
        const auto lane = spectralWorker.lane();
        spectralWorker.accept(needed && (available & (1u << lane)) &&
                              spectralWorker.revision() == wavesetSourceRevisions[lane]);
    }
    std::uint32_t ready = 0;
    for (std::size_t lane = 0; lane < kLaneCount; ++lane) {
        const auto *bank = spectralWorker.bank(lane);
        if (!(available & (1u << lane)) || !bank || bank->revision != wavesetSourceRevisions[lane])
            bank = nullptr;
        engine.setPreparedSpectralBank(lane, bank);
        for (auto &voice : voices)
            voice.engine.setPreparedSpectralBank(lane, bank);
        if (bank)
            ready |= 1u << lane;
    }
    spectralReadyMask.store(ready);
    spectralPendingMask.store(needed ? available & ~ready : 0);
    if (state == Worker::complete) {
        spectralWorker.releaseResult();
        state = Worker::idle;
    }
    if (state == Worker::copying) {
        const auto lane = spectralWorker.lane();
        if (!needed || !(available & (1u << lane)) ||
            spectralWorker.revision() != wavesetSourceRevisions[lane]) {
            spectralWorker.cancelCopy();
            state = Worker::idle;
        }
    }
    if (needed && state == Worker::idle)
        for (std::size_t offset = 0; offset < kLaneCount; ++offset) {
            const auto lane = (spectralNextLane + offset) % kLaneCount;
            const auto *bank = spectralWorker.bank(lane);
            if (!(available & (1u << lane)) ||
                (bank && bank->revision == wavesetSourceRevisions[lane]))
                continue;
            spectralWorker.begin(lane, wavesetSourceRevisions[lane], engine.laneFrames(lane),
                                 engine.laneSampleRate(lane));
            spectralNextLane = (lane + 1) % kLaneCount;
            break;
        }
    // At most 16 KiB, never an FFT, allocation, mutex or file operation.
    if (spectralWorker.state() == Worker::copying) {
        const auto lane = spectralWorker.lane();
        spectralWorker.copy(engine.laneData(lane, 0), engine.laneData(lane, 1), 2048);
    }
}
static bool writeNewEngineParameters(std::FILE *file,
                                     const std::array<ClipSlot, kClipSlotCount> &clips) {
    if (!writeU32(file, 0x4e455731u) ||
        !writeU32(file, kLockParameterCount - kVersion19LockParameterCount))
        return false;
    for (const auto &clip : clips)
        for (std::size_t group = 0; group < 5; ++group) {
            const auto &values = group == 0 ? clip.baseValues : clip.splitValues[group - 1];
            for (std::size_t p = kVersion19LockParameterCount; p < kLockParameterCount; ++p)
                if (!writeF32(file, values[p]))
                    return false;
        }
    return true;
}
static bool readNewEngineParameters(std::FILE *file, std::array<ClipSlot, kClipSlotCount> &clips,
                                    std::uint32_t version) {
    std::uint32_t tag = 0, count = 0;
    if (!readU32(file, tag) || tag != 0x4e455731u || !readU32(file, count) ||
        count != (version == 20 ? kVersion20LockParameterCount : version == 21 ? kVersion21LockParameterCount : version == 22 ? kVersion22LockParameterCount : kLockParameterCount) -
                     kVersion19LockParameterCount)
        return false;
    for (auto &clip : clips)
        for (std::size_t group = 0; group < 5; ++group) {
            auto &values = group == 0 ? clip.baseValues : clip.splitValues[group - 1];
            values[static_cast<int>(LockParameter::morphMode)] = 0;
            for (std::size_t p=kVersion21LockParameterCount; p<kLockParameterCount; ++p)
                values[p]=lockedValue(Engine::Controls{},1.f,p);
            for (std::size_t p = kVersion19LockParameterCount;
                 p < kVersion19LockParameterCount + count; ++p)
                if (!readF32(file, values[p]))
                    return false;
        }
    return true;
}
