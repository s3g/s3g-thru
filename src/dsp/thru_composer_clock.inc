// Predict from the sequencer's own applied phase and traversal, never UI RATE.
std::atomic<std::uint32_t> composerClockSequence{0};
std::atomic<double> composerClockAt{0},composerClockPosition{0},composerClockSpeed{1};
std::atomic<int> composerClockTransport{0};
std::atomic<int> composerClockClip{-1},composerClockRunning{0};
std::atomic<std::uint64_t> composerClockLaunch{0};
std::array<std::atomic<double>,4> composerClockPhases{},composerClockSpans{};
std::array<std::array<std::atomic<int>,16>,4> composerClockCells{};
std::uint64_t composerClockLastFrame=0;
void captureComposerClock(){
    if(!composerPanelWatching.load()||composerLease.load()<composerClock())return;
    const bool transport=clipPlaying&&(!midiSyncSource||!midiClockStopped);const bool sequence=sequenceRunning()&&(!midiSyncSource||!midiClockStopped);const auto frame=renderClockSamples.load();if(frame-composerClockLastFrame<std::uint64_t(sampleRate*.016)&&composerClockClip.load()==activeClip&&composerClockRunning.load()==int(sequence)&&composerClockTransport.load()==int(transport))return;composerClockLastFrame=frame;
    composerClockSequence.fetch_add(1,std::memory_order_acq_rel);composerClockAt.store(double(composerClock()));composerClockClip.store(activeClip);composerClockLaunch.store(clipLaunchSerial);const bool running=sequence;composerClockRunning.store(running);composerClockTransport.store(transport);composerClockPosition.store(clipPositionMs);const auto* clip=activeClipSlot();composerClockSpeed.store(clip?double(clip->clipRate)*(clip->timingMode==ClipTimingMode::beatSync?double(tempoBpm)/std::max(1.,double(clip->recordedTempoBpm)):1.):1.);
    for(int p=0;p<4;++p){const auto& lane=sequencePlayback.tracks[p];const double phase=std::max(0.,(sequenceBeat+sequenceBeatOffsets[p])/s3g_thru_sequence::rates[lane.rate]);composerClockPhases[p].store(phase);composerClockSpans[p].store(60000.*s3g_thru_sequence::rates[lane.rate]/std::max(1.,double(tempoBpm)*(activeClipSlot()?activeClipSlot()->clipRate:1.)));
        const auto tick=std::int64_t(std::floor(phase+1e-9));for(int next=0;next<16;++next){const auto step=sequenceDirections[p].at(lane,tick+next).step;composerClockCells[p][next].store(running&&step<lane.length?step:-1);}}
    composerClockSequence.fetch_add(1,std::memory_order_release);
}
ComposerJson composerClockJson()const{
    for(int attempt=0;attempt<3;++attempt){const auto begin=composerClockSequence.load(std::memory_order_acquire);if(begin&1u)continue;auto lanes=ComposerJson::array();for(int p=0;p<4;++p){auto cells=ComposerJson::array();for(const auto& c:composerClockCells[p])cells.push(c.load());lanes.push(composerObject({{"phase",composerClockPhases[p].load()},{"stepMs",composerClockSpans[p].load()},{"cells",std::move(cells)}}));}
        auto out=composerObject({{"at",composerClockAt.load()},{"clip",composerClockClip.load()},{"launch",double(composerClockLaunch.load())},{"running",bool(composerClockRunning.load())},{"transport",bool(composerClockTransport.load())},{"positionMs",composerClockPosition.load()},{"speed",composerClockSpeed.load()},{"lanes",std::move(lanes)}});if(begin==composerClockSequence.load(std::memory_order_acquire))return out;
    }return ComposerJson();
}
