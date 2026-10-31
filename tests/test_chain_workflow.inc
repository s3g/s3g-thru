void testChainEditingUndoAndCopies() {
    auto t=splitFixture();t->stopSampleWorker();
    t->setChainEntry(0,2,4);t->chainEditSlot=0;
    setParam(t.get(),"entry_fade_mode","2");setParam(t.get(),"entry_fade_ms","8000");
    t->undoAvailable=false;t->clearChainEntry(0);assert(!t->clipChain()[0].enabled&&t->undoIsChain);
    t->restoreUndo();assert(t->clipChain()[0].enabled&&t->clipChain()[0].clip==2&&t->clipChain()[0].fade.time==8000);
    t->moveChainEntry(0,15);t->restoreUndo();assert(t->clipChain()[0].enabled&&!t->clipChain()[15].enabled);
    t->insertChainEntry(0,5);t->restoreUndo();assert(t->clipChain()[0].clip==2&&!t->clipChain()[1].enabled);
    setParam(t.get(),"chain_copy","1");t->selectChainBank(1);setParam(t.get(),"chain_paste","1");
    assert(t->clipChain()[0].fade.time==8000);
    t->clipChain()[0].fade.time=700;assert(t->chainBanks[0].entries[0].fade.time==8000);
    t->restoreUndo();assert(!t->clipChain()[0].enabled);t->selectChainBank(0);
    setParam(t.get(),"entry_copy","1");t->chainEditSlot=63;setParam(t.get(),"entry_paste","1");
    assert(t->clipChain()[63].clip==2&&t->clipChain()[63].fade.time==8000);
    t->restoreUndo();assert(!t->clipChain()[63].enabled);
    t->chainEditSlot=0;t->undoAvailable=false;
    setParam(t.get(),"entry_fade_ms","9000");setParam(t.get(),"entry_fade_ms","10000");t->restoreUndo();
    assert(t->clipChain()[0].fade.time==8000);
    t->undoAvailable=false;setParam(t.get(),"entry_fade_ms","8000");assert(!t->undoAvailable);
    t->pushClipUndo(0);assert(t->undoAvailable&&!t->undoIsChain);t->restoreUndo();assert(!t->undoAvailable);
    t->setChainEntry(0,3,2);assert(t->clipChain()[0].clip==3);
    t->restoreUndo();assert(t->clipChain()[0].clip==2);
}
void testChainBrowsingAndQueuedLaunch() {
    auto t=splitFixture();t->setPerformanceMode(0);renderSplit(*t,2);t->stopSampleWorker();
    t->currentLaneSourceKinds.fill(LaneSourceKind::capture);t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);
    t->clipSlots[1]=t->clipSlots[0];t->setChainEntry(0,0,0);t->setChainEnabled(true);t->clipPlaying=true;
    t->selectChainBank(1);t->setChainEntry(31,1,0);t->chainEditSlot=31;
    assert(t->chainEnabled&&t->chainPlayBank==0&&t->activeClip==0);
    setParam(t.get(),"chain_launch","1");assert(t->chainLaunchBank==1&&t->chainLaunchSlot==31&&t->activeClip==0);
    assert(t->prepareClipBank(1));t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
    t->advanceClipChain();assert(t->chainPlayBank==1&&t->chainPosition==31&&t->activeClip==1&&t->chainEnabled);
    t->selectChainBank(0);setParam(t.get(),"chain_launch","1");setParam(t.get(),"chain_launch_cancel","1");
    assert(t->chainLaunchBank<0&&t->chainPlayBank==1&&t->activeClip==1&&t->chainEnabled);
    t->setChainEnabled(false);t->selectChainBank(0);setParam(t.get(),"chain_launch","1");assert(t->chainLaunchBank==0);
    t->patternHeld=true;t->selectClip(1);assert(t->chainLaunchBank<0&&t->patternHeld); // Manual selection cancels an outstanding cue.
    char text[128]{};getParamValue(t.get(),"chain_state",text,sizeof(text));assert(std::strstr(text,",1,-1,-1"));
}
void renderSourceReturn(ThruInstance& t,std::array<float,kBlockFrames*2u>& audio) {
    for(int n=0;n<1000&&t.patternActive;++n) {
        if(t.patternJob.load(std::memory_order_acquire)==1)waitForPatternAudio(t);
        t.render(nullptr,kBlockFrames,audio.data());usleep(500);
        for(float value:audio)assert(std::isfinite(value));
    }
    assert(!t.patternActive);waitForPatternIdle(t);
}
void testPatternReturnSourceContinuity() {
    for(int mode=0;mode<4;++mode) {
        auto t=splitFixture();t->setPerformanceMode(mode);renderSplit(*t,2);t->stopSampleWorker();
        t->currentLaneSourceKinds.fill(LaneSourceKind::capture);t->padNoteOn(68,100);renderSplit(*t,20);
        t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
        const auto* source=t->engine.laneData(0,0);t->patternFadeMs=1000;
        assert(t->prepareClipBank(1));t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));assert(t->activatePreparedClip());
        std::array<float,kBlockFrames*2u> audio{};t->render(nullptr,kBlockFrames,audio.data());t->setPatternPosition(.5f);
        for(int n=0;n<80;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
        t->returnPatternSource();renderSourceReturn(*t,audio);
        assert(t->activeClip==0&&t->engine.laneData(0,0)==source&&t->performanceMode()==mode&&!t->chainEnabled);
        bool held=false;for(const auto& voice:t->voices)held|=voice.active&&voice.held;assert(held);
        t->padNoteOff(68);for(const auto& voice:t->voices)assert(!voice.held);
        // The retired scene and the loader can be used for another transition.
        assert(t->prepareClipBank(1));t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));assert(t->activatePreparedClip());
        t->render(nullptr,kBlockFrames,audio.data());t->finishPatternFade();
        for(int n=0;n<300&&t->patternActive;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
        waitForPatternIdle(*t);
    }
}
void testFormerTransitionTakeMigration() {
    char directory[]="/tmp/thru-former-take-XXXXXX";assert(mkdtemp(directory));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(directory));t->stopBackgroundWork();
    t->setChainEntry(1,5,3);t->chainEditSlot=1;setParam(t.get(),"entry_fade_mode","2");setParam(t.get(),"entry_fade_ms","8000");
    t->chainPlayBank=3;assert(t->savePersistentState());
    const auto path=std::string(directory)+"/thru-clips.bin";
    auto* file=std::fopen(path.c_str(),"rb");assert(file);assert(std::fseek(file,0,SEEK_END)==0);
    const long length=std::ftell(file);std::rewind(file);std::vector<unsigned char> bytes(static_cast<std::size_t>(length)-4u);
    assert(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);
    bytes.resize(bytes.size()-37128);bytes[8]=19;
    constexpr std::size_t footer=4104u;assert(bytes.size()>footer);
    const auto countAt=bytes.size()-footer+8u+4u; // Chain 01, entry 02.
    std::vector<unsigned char> record;
    for(std::uint32_t word:{2u,1u,0u,250u,0u,0u,3u,250u,10000u,3u})
        for(unsigned shift=0;shift<32;shift+=8)record.push_back(static_cast<unsigned char>(word>>shift));
    bytes.erase(bytes.begin()+countAt,bytes.begin()+countAt+4u);bytes.insert(bytes.begin()+countAt,record.begin(),record.end());
    const auto writeFixture=[&] {
        std::uint32_t checksum=2166136261u;for(auto byte:bytes){checksum^=byte;checksum*=16777619u;}
        auto* output=std::fopen(path.c_str(),"wb");assert(output);
        assert(std::fwrite(bytes.data(),1,bytes.size(),output)==bytes.size());assert(writeU32(output,checksum));assert(std::fclose(output)==0);
    };
    writeFixture();assert(t->readClipFileFromPath(path.c_str()));
    assert(t->chainPlayBank==3&&t->chainEditSlot==1&&t->clipChain()[1].clip==5&&t->clipChain()[1].repeats==3);
    assert(t->clipChain()[1].fadeMode==2&&t->clipChain()[1].fade.time==8000);
    char summary[128]{};getParamValue(t.get(),"entry_effective_fade",summary,sizeof(summary));assert(summary[0]=='2');
    // Invalid obsolete records still reject the complete bank transactionally.
    bytes[countAt]=1u;writeFixture();assert(!t->readClipFileFromPath(path.c_str()));
    assert(t->clipChain()[1].clip==5&&t->clipChain()[1].fade.time==8000);
    // A new save drops obsolete gesture records and keeps the zeroed format-19 footer inside format 21.
    assert(t->savePersistentState());file=std::fopen(path.c_str(),"rb");assert(file);
    assert(std::fseek(file,-(4100+37128),SEEK_END)==0);std::uint32_t count=99;
    for(std::size_t i=0;i<kSavedChainCount*kClipChainLength;++i){assert(readU32(file,count));assert(count==0u);}
    std::fclose(file);t.reset();removeBackgroundTestDirectory(directory);
}
