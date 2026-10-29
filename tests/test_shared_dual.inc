// Shared Dual reuses four PCM lanes, while keeping two independent programs.
void testSharedDualSourcesAndVoices()
{
    auto t=splitFixture();
    // Single playback retags voices; entering either dual layout must restore 4+4.
    t->setPerformanceMode(0);
    for(int note=60;note<68;++note)t->noteOn(note,100);
    t->setPerformanceMode(2);
    assert(t->splitEnabled&&t->sharedDual&&t->performanceMode()==2);
    for(std::size_t i=0;i<t->voices.size();++i) {
        assert(t->voices[i].part==static_cast<int>(i/4u));
        assert(t->voices[i].engine.loadedLaneMask()==15u);
    }
    t->controls.speed=.7f;t->controls.manualLane=3;t->setStackScanPaused(true);
    t->selectSplitPart(1);t->controls.speed=1.3f;t->controls.manualLane=0;t->setStackScanPaused(true);
    t->padNoteOn(68,100);t->padNoteOn(72,100);renderSplit(*t,80);
    assert(t->voices[0].held&&t->voices[4].held);
    assert(t->effectiveControls(t->voices[0]).manualLane==3);
    assert(t->effectiveControls(t->voices[4]).manualLane==0);
    assert(t->effectiveControls(t->voices[0]).speed<t->effectiveControls(t->voices[4]).speed);
    for(std::size_t algorithm=0;algorithm<Engine::kPlaybackAlgorithmCount;++algorithm) {
        t->controls.algorithm=static_cast<Engine::PlaybackAlgorithm>(algorithm);
        renderSplit(*t,4);
        for(const auto& v:t->voices) {
            assert(v.engine.loadedLaneMask()==15u);
            const auto c=t->effectiveControls(v);
            assert(c.stackFirstLane==0&&c.stackLastLane==3);
        }
    }
    t->controls.algorithm=Engine::PlaybackAlgorithm::sample;
    for(int lane=0;lane<4;++lane)t->controls.laneGains[lane]=.1f*(lane+1);
    t->copySplitPart(1);t->selectSplitPart(0);
    for(int lane=0;lane<4;++lane)assert(t->controls.laneGains[lane]==t->liveControlsForPart(1).laneGains[lane]);
    // Stealing within A cannot steal B, even when they read the same samples.
    const auto bAge=t->voices[4].age;
    for(int pad:{1,2,3,8,9,10})t->padNoteOn(68+pad,100);
    assert(t->voices[4].held&&t->voices[4].age==bAge);
    t->laneVariation=LaneVariation::cycle;
    bool seen[4]{};
    for(int i=0;i<8;++i)seen[t->chooseVariationLane(0)]=true;
    for(bool lane:seen)assert(lane);
    setParam(t.get(),"record_lane","4");assert(t->splitFocus==0&&t->selectedLane==3u);
    t->selectSplitPart(1);assert(t->selectedLane==3u);
    setParam(t.get(),"record_lane","1");assert(t->splitFocus==1&&t->selectedLane==0u);
    // A newly published source must be bound into both programs' source views.
    t->sampleRequestLane=0;t->sampleRequestClip=t->activeClip;t->sampleRequestPreview=false;
    t->sampleStageFrames=4096;t->sampleStageRate=t->sampleRate;
    std::fill_n(t->sampleStage.get(),4096,.1f);
    std::fill_n(t->sampleStage.get()+t->laneCapacityFrames,4096,-.05f);
    t->sampleLoadState.store(static_cast<int>(SampleLoadState::ready));
    for(int i=0;i<300&&t->sampleLoadState.load()!=static_cast<int>(SampleLoadState::complete);++i)renderSplit(*t,1);
    assert(t->sampleLoadState.load()==static_cast<int>(SampleLoadState::complete));
    renderSplit(*t,20); // voice views adopt the published lane on the next render.
    for(const auto& v:t->voices)assert(v.engine.laneFrames(0)==4096u);
    t->beginClipRecording();t->setPerformanceMode(1);
    assert(t->performanceMode()==2);t->finishClipRecording();
}

void testSharedDualWindowsAndRecall()
{
    auto t=splitFixture();t->controls.manualLane=1;t->selectSplitPart(1);t->controls.manualLane=.25f;
    t->setPerformanceMode(2);
    assert(t->splitFocus==1&&t->controls.manualLane==2.25f);
    assert(t->liveControlsForPart(0).manualLane==1);
    t->controls.locus=.8f;t->controls.field=.2f;t->selectSplitPart(0);
    t->controls.locus=.2f;t->controls.field=.2f;
    const auto close=[](float a,float b){return std::abs(a-b)<.0001f;};
    auto w=t->sharedDisplayWindow();assert(close(w[0],.1f)&&close(w[1],.3f));
    t->padNoteOn(68,100);t->padNoteOn(72,100);renderSplit(*t,100);
    w=t->sharedDisplayWindow();assert(close(w[0],.1f)&&close(w[1],.3f));
    t->selectSplitPart(1);w=t->sharedDisplayWindow();assert(close(w[0],.7f)&&close(w[1],.9f));
    t->captureActiveClip(true);
    auto text=std::make_unique<std::array<char,kStateTextSize>>();
    assert(t->serializeState(text->data(),text->size())>0);
    t->setPerformanceMode(1);assert(t->controls.manualLane==.25f);
    assert(t->restoreState(text->data())&&t->performanceMode()==2);
    assert(t->voices[0].engine.loadedLaneMask()==15u&&t->voices[4].engine.loadedLaneMask()==15u);
    t->setPerformanceMode(0);assert(t->sharedDual);
    assert(t->serializeState(text->data(),text->size())>0);
    t->sharedDual=false;assert(t->restoreState(text->data())&&!t->splitEnabled&&t->sharedDual);
    t->setSplitMode(true);assert(t->performanceMode()==2);
    // SPL1 saves predate the additive shared flag and remain readable.
    t->setPerformanceMode(1);t->captureActiveClip(true);
    auto file=std::tmpfile();assert(file);
    assert(writeU32(file,0x53504c31u));
    for(const auto& clip:t->clipSlots) {
        assert(writeU32(file,clip.splitEnabled?1u:0u)&&writeU32(file,clip.splitInitialized?1u:0u)&&writeU32(file,clip.splitFocus));
        assert(ThruInstance::writePartSettings(file,clip.singleSettings));
        for(int part=0;part<2;++part) {
            for(std::size_t p=0;p<kVersion14LockParameterCount;++p)assert(writeF32(file,clip.splitValues[part][p]));
            assert(ThruInstance::writePartSettings(file,clip.splitSettings[part]));
        }
    }
    std::rewind(file);
    auto clips=std::make_unique<std::array<ClipSlot,kClipSlotCount>>();
    assert(ThruInstance::readSplitExtension(file,*clips));std::fclose(file);
    assert((*clips)[t->activeClip].splitEnabled&&!(*clips)[t->activeClip].sharedDual);
    assert(t->serializeState(text->data(),text->size())>0);
    std::string legacy(text->data());
    const auto flag=legacy.find(",\"split_shared\":0");assert(flag!=std::string::npos);
    legacy.erase(flag,std::strlen(",\"split_shared\":0"));
    assert(t->restoreState(legacy.c_str())&&t->performanceMode()==1);
}

void testSharedDualLiveZoom()
{
    auto t=splitFixture();t->setPerformanceMode(2);
    t->controls.locus=.5f;t->controls.field=1;
    t->selectSplitPart(1);t->controls.locus=.5f;t->controls.field=1;
    t->padNoteOn(68,100);t->padNoteOn(72,100);renderSplit(*t,100);
    const auto aAge=t->voices[0].age,bAge=t->voices[4].age;
    const auto close=[](float a,float b){return std::abs(a-b)<.00001f;};
    auto verify=[&](float low,float high) {
        const auto window=t->sharedDisplayWindow();
        assert(close(window[0],low)&&close(window[1],high));
        std::array<char,1024> text{};assert(getParam(t.get(),"ui_state",text.data(),text.size())>0);
        const auto* end=std::strrchr(text.data(),',');assert(end);
        const auto* start=end;while(start>text.data()&&start[-1]!=',')--start;
        assert(close(std::strtof(start,nullptr),low)&&close(std::strtof(end+1,nullptr),high));
    };
    // Neither the other part nor existing wide readers can hold the zoom open.
    t->selectSplitPart(0);setParam(t.get(),"field","0.1");setParam(t.get(),"locus","0.2");
    verify(.15f,.25f);
    const auto position=t->voices[0].engine.normalizedPosition();
    setParam(t.get(),"locus","0.6");verify(.55f,.65f);
    assert(t->voices[0].engine.normalizedPosition()==position); // drawing cannot seek audio.
    renderSplit(*t,100);verify(.55f,.65f);
    t->selectSplitPart(1);verify(0,1);
    setParam(t.get(),"field","0.04");setParam(t.get(),"locus","0.8");verify(.78f,.82f);
    assert(close(t->liveControlsForPart(0).locus,.6f)&&close(t->liveControlsForPart(0).field,.1f));
    // A playing data loop retains its own recorded controls; zoom stays editable.
    t->splitClipControls[1].locus=.5f;t->splitClipControls[1].field=1;
    t->clipPlaying=true;t->voices[4].clipOwned=true;verify(.78f,.82f);
    setParam(t.get(),"locus","0.3");verify(.28f,.32f);
    assert(t->splitClipControls[1].field==1&&t->splitClipControls[1].locus==.5f);
    t->clipPlaying=false;t->voices[4].clipOwned=false;
    for(std::size_t algorithm=0;algorithm<Engine::kPlaybackAlgorithmCount;++algorithm) {
        for(int part=0;part<2;++part) {
            t->selectSplitPart(part);t->controls.algorithm=static_cast<Engine::PlaybackAlgorithm>(algorithm);
            setParam(t.get(),"field","0.1");setParam(t.get(),"locus",part?"0.75":"0.25");
            verify(part?.7f:.2f,part?.8f:.3f);
            renderSplit(*t,4);verify(part?.7f:.2f,part?.8f:.3f);
        }
    }
    assert(t->voices[0].age==aAge&&t->voices[4].age==bAge);
    setParam(t.get(),"locus","0");verify(0,.1f);
    setParam(t.get(),"locus","1");verify(.9f,1);
    setParam(t.get(),"field","0.02");verify(.98f,1);
}

void testSharedDualBankRoundTrip()
{
    char path[]="/tmp/thru-shared-bank.XXXXXX";assert(mkdtemp(path));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(path));t->stopBackgroundWork();
    t->setPerformanceMode(2);t->controls.manualLane=3;
    t->selectSplitPart(1);t->controls.manualLane=0;t->rootNote=67;
    t->beginClipRecording();t->padNoteOn(68,100);t->padNoteOn(72,100);renderSplit(*t,20);
    t->controls.speed=.8f;t->captureClipParameter(static_cast<int>(LockParameter::speed));
    t->padNoteOff(68);t->padNoteOff(72);renderSplit(*t,100);t->finishClipRecording();
    assert(t->savePersistentState());t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(path));t->stopBackgroundWork();
    assert(t->persistenceRestored&&t->performanceMode()==2&&t->splitFocus==1);
    assert(t->controls.manualLane==0&&t->rootNote==67&&t->controls.speed==.8f);
    assert(t->liveControlsForPart(0).manualLane==3);
    auto& clip=*t->activeClipSlot();assert(clip.sharedDual&&clip.clipNoteCount==2);
    assert(clip.clipNotes[0].part==1&&clip.clipNotes[1].part==2);
    const int target=(t->activeClip+1)%kClipSlotCount;
    t->clipSlots[target]=clip;
    t->setPerformanceMode(1);t->activateStoredClip(target,false);
    assert(t->performanceMode()==2);
    t.reset();removeBackgroundTestDirectory(path);
}
