bool spectralRequested() const {
    const auto spectral = [](Engine::PlaybackAlgorithm a) {
        return a == Engine::PlaybackAlgorithm::prism || a == Engine::PlaybackAlgorithm::morph || a == Engine::PlaybackAlgorithm::formant;
    };
    if (listUsesAlgorithm(9)||listUsesAlgorithm(10)||listUsesAlgorithm(12)||spectral(controls.algorithm))
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
        !writeU32(file, kVersion25LockParameterCount - kVersion19LockParameterCount))
        return false;
    for (const auto &clip : clips)
        for (std::size_t group = 0; group < 5; ++group) {
            const auto &values = group == 0 ? clip.baseValues : clip.splitValues[group - 1];
            for (std::size_t p = kVersion19LockParameterCount; p < kVersion25LockParameterCount; ++p)
                if (!writeF32(file, values[p]))
                    return false;
        }
    return true;
}
static bool readNewEngineParameters(std::FILE *file, std::array<ClipSlot, kClipSlotCount> &clips,
                                    std::uint32_t version) {
    std::uint32_t tag = 0, count = 0;
    if (!readU32(file, tag) || tag != 0x4e455731u || !readU32(file, count) ||
        count != (version == 20 ? kVersion20LockParameterCount : version == 21 ? kVersion21LockParameterCount : version == 22 ? kVersion22LockParameterCount : kVersion25LockParameterCount) -
                     kVersion19LockParameterCount)
        return false;
    for (auto &clip : clips)
        for (std::size_t group = 0; group < 5; ++group) {
            auto &values = group == 0 ? clip.baseValues : clip.splitValues[group - 1];
            values[static_cast<int>(LockParameter::morphMode)] = 0;
            for (std::size_t p=kVersion21LockParameterCount; p<kVersion28LockParameterCount; ++p)
                values[p]=lockedValue(Engine::Controls{},1.f,p);
            for (std::size_t p = kVersion19LockParameterCount;
                 p < kVersion19LockParameterCount + count; ++p)
                if (!readF32(file, values[p]))
                    return false;
        }
    return true;
}

// Formats 26–28 append these controls without changing the preceding extensions.
static bool writeStochasticParameters(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips) {
    if(!writeU32(file,0x53544f31u))return false;
    for(const auto& clip:clips)for(int group=0;group<5;++group) {
        const auto& values=group?clip.splitValues[group-1]:clip.baseValues;
        for(std::size_t p=kVersion25LockParameterCount;p<kVersion28LockParameterCount;++p)if(!writeF32(file,values[p]))return false;
    }
    return true;
}
static bool readStochasticParameters(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips,std::uint32_t version) {
    std::uint32_t magic=0;if(!readU32(file,magic)||magic!=0x53544f31u)return false;
    for(auto& clip:clips)for(int group=0;group<5;++group) {
        auto& values=group?clip.splitValues[group-1]:clip.baseValues;
        for(std::size_t p=kVersion25LockParameterCount;p<kVersion28LockParameterCount;++p)values[p]=lockedValue(Engine::Controls{},1.f,p);
        for(std::size_t p=kVersion25LockParameterCount;p<(version>=28u?kVersion28LockParameterCount:version==27u?kVersion27LockParameterCount:kVersion26LockParameterCount);++p)if(!readF32(file,values[p]))return false;
    }
    return true;
}

static bool writePulsarParameters(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips) {
    if(!writeU32(file,0x50554c31u))return false;
    for(const auto& clip:clips)for(int group=0;group<5;++group)for(std::size_t p=kVersion28LockParameterCount;p<kVersion30LockParameterCount;++p)
        if(!writeF32(file,(group?clip.splitValues[group-1]:clip.baseValues)[p]))return false;
    return true;
}
static bool readPulsarParameters(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips,std::uint32_t version) {
    std::uint32_t magic=0;if(!readU32(file,magic)||magic!=0x50554c31u)return false;
    for(auto& clip:clips)for(int group=0;group<5;++group) {
        auto& values=group?clip.splitValues[group-1]:clip.baseValues;
        for(std::size_t p=kVersion28LockParameterCount;p<kVersion30LockParameterCount;++p)values[p]=lockedValue(Engine::Controls{},1.f,p);
        for(std::size_t p=kVersion28LockParameterCount;p<(version>=30u?kVersion30LockParameterCount:kVersion29LockParameterCount);++p)
            if(!readF32(file,values[p]))return false;
    }
    return true;
}

static bool writePriorityParameters(std::FILE* file,
    const std::array<ClipSlot,kClipSlotCount>& clips) {
    if(!writeU32(file,0x44554331u))return false; // DUC1
    for(const auto& clip:clips)for(int group=0;group<5;++group)
        for(std::size_t p=kVersion30LockParameterCount;p<kLockParameterCount;++p)
            if(!writeF32(file,(group?clip.splitValues[group-1]:clip.baseValues)[p]))
                return false;
    return true;
}

static bool readPriorityParameters(std::FILE* file,
    std::array<ClipSlot,kClipSlotCount>& clips) {
    std::uint32_t magic=0;
    if(!readU32(file,magic)||magic!=0x44554331u)return false;
    for(auto& clip:clips)for(int group=0;group<5;++group) {
        auto& values=group?clip.splitValues[group-1]:clip.baseValues;
        for(std::size_t p=kVersion30LockParameterCount;p<kLockParameterCount;++p)
            if(!readF32(file,values[p]))return false;
    }
    return true;
}
