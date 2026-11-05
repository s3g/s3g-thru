void setListSlot(ThruInstance& t,int slot,int algorithm) {
    const auto key="voice_engine_"+std::to_string(slot+1);
    setParam(&t,key.c_str(),std::to_string(algorithm).c_str());
}
void testVoiceEngineAllocation() {
    for(int mode:{0,1,2,3})for(int order:{0,1,2}) {
        auto t=splitFixture();t->setPerformanceMode(mode);
        const int parts=mode?(mode==3?4:2):1,cap=t->engineSlotCount();
        for(int part=0;part<parts;++part) {
            t->selectSplitPart(part,false);setParam(t.get(),"voice_engine_mode","1");
            setParam(t.get(),"voice_engine_order",std::to_string(order).c_str());
            for(int i=0;i<cap;++i)setListSlot(*t,i,i%2?2:4);
        }
        for(int part=0;part<parts;++part) {
            t->midiPart=part;
            for(int i=0;i<cap;++i) {
                t->noteOn(48+i,100);
                const PlaybackVoice* found=nullptr;
                for(const auto& v:t->voices)if(v.held&&v.note==48+i&&(!mode||v.part==part))found=&v;
                assert(found&&found->engineSlot>=0&&found->engineSlot<cap);
                if(order<2)assert(found->engineSlot==(order?cap-1-i:i));
                assert(found->assignedAlgorithm==(found->engineSlot%2?2:4));
                assert(static_cast<int>(t->effectiveControls(*found).algorithm)==found->assignedAlgorithm);
            }
            bool used[8]{};
            for(const auto& v:t->voices)if(v.held&&(!mode||v.part==part)) {assert(!used[v.engineSlot]);used[v.engineSlot]=true;}
            for(int i=0;i<cap;++i)assert(used[i]);
        }
        renderSplit(*t,16);
        for(const auto& v:t->voices)assert(v.held&&static_cast<int>(v.engine.playbackAlgorithm())==v.assignedAlgorithm);
        // Editing an entry does not retarget an existing held note or release tail.
        t->selectSplitPart(0,false);t->midiPart=0;
        const auto& original=t->voices[0];const int old=original.assignedAlgorithm,slot=original.engineSlot;
        setListSlot(*t,slot,6);assert(original.assignedAlgorithm==old);
        t->noteOff(original.note);assert(original.assignedAlgorithm==old&&!original.held);
        t->noteOn(82,100);
        bool reused=false;for(const auto& v:t->voices)if(v.held&&v.note==82&&(!mode||v.part==0)) {assert(v.engineSlot==slot&&v.assignedAlgorithm==6);reused=true;}
        assert(reused);
    }
    auto t=splitFixture();t->setPerformanceMode(0);setParam(t.get(),"voice_engine_mode","1");
    // Duplicate entries weight RAND when all four slots are free.
    t->setPerformanceMode(2);setParam(t.get(),"voice_engine_mode","1");for(int i=0;i<4;++i)setListSlot(*t,i,i<2?2:4+i);
    setParam(t.get(),"voice_engine_order","2");int counts[8]{};
    for(int i=0;i<4096;++i) {
        for(auto& v:t->voices){v.active=v.held=false;v.stealPending=false;}
        t->noteOn(60,100);for(const auto& v:t->voices)if(v.held)++counts[v.assignedAlgorithm];
    }
    assert(counts[2]>1800&&counts[2]<2300&&counts[6]>850&&counts[6]<1200&&counts[7]>850&&counts[7]<1200);
    // All held incoming/outgoing engines survive a full voice-budget retrigger.
    t->setPerformanceMode(0);t->voiceEngineSettings.enabled=true;t->voiceEngineSettings.order=0;
    for(int i=0;i<8;++i)setListSlot(*t,i,4);
    for(auto& v:t->voices){v.active=v.held=false;v.stealPending=false;}
    for(int i=0;i<8;++i)t->noteOn(48+i,100);
    renderSplit(*t,30);setListSlot(*t,0,2);t->noteOn(83,100);
    bool steal=false;for(const auto& v:t->voices)if(v.note==83) {assert(v.assignedAlgorithm==2&&v.stealPending&&v.stealControls.algorithm==Engine::PlaybackAlgorithm::sample);steal=true;}
    assert(steal);renderSplit(*t,30);
    std::puts("s3g Thru Engine List allocation, weighting, stable held engines and stealing: PASS");
}
void testVoiceEngineCaptureAndRecall() {
    auto t=splitFixture();t->setPerformanceMode(0);setParam(t.get(),"voice_engine_mode","1");
    setListSlot(*t,0,2);setListSlot(*t,1,7);setParam(t.get(),"voice_engine_order","1");setListSlot(*t,7,4);
    t->padNoteOn(75,100); // right half of Single must capture the assigned slot too.
    t->beginClipRecording();renderSplit(*t,25);t->padNoteOff(75);
    t->padNoteOn(68,100);renderSplit(*t,25);t->padNoteOff(68);t->finishClipRecording();
    const auto& clip=*t->activeClipSlot();assert(clip.clipNoteCount==2);
    for(int i=0;i<2;++i)assert(clip.clipNotes[i].engineSlot==7&&clip.clipNotes[i].engineAlgorithm==4);
    setListSlot(*t,7,2);setParam(t.get(),"voice_engine_order","2");
    t->releaseClipVoices();t->triggerClipVoice(clip,0);
    bool found=false;for(const auto& v:t->voices)if(v.active&&v.clipOwned) {assert(v.engineSlot==7&&v.assignedAlgorithm==4);found=true;}
    assert(found);
    t->releaseClipVoices();t->clipRecordMode=ClipRecordMode::overdub;t->beginClipRecording();
    t->noteOn(84,100);renderSplit(*t,10);t->noteOff(84);t->finishClipRecording();
    assert(t->activeClipSlot()->takeCount==2&&t->activeClipSlot()->clipNoteCount>=3);
    setParam(t.get(),"clip_take_undo","1");assert(t->activeClipSlot()->clipNoteCount==2);
    for(int i=0;i<2;++i)assert(t->activeClipSlot()->clipNotes[i].engineSlot==7&&t->activeClipSlot()->clipNotes[i].engineAlgorithm==4);
    assert(t->copyClipToClipboard(t->activeClip));t->pasteClipFromClipboard(2);
    assert(t->clipSlots[2].singleSettings.engines.enabled&&t->clipSlots[2].clipNotes[0].engineSlot==7);
    // Snapshot and disk roundtrips preserve all mode/part programs.
    t->captureActiveClip(true);
    for(int mode:{1,2,3}) {
        t->setPerformanceMode(mode);
        for(int part=0;part<t->partCount();++part) {
            t->selectSplitPart(part,false);setParam(t.get(),"voice_engine_mode","1");
            setParam(t.get(),"voice_engine_order",std::to_string(part%3).c_str());
            for(int i=0;i<t->engineSlotCount();++i)setListSlot(*t,i,(part+i+8)%13);
        }
        t->captureActiveClip(true);
    }
    auto json=std::make_unique<std::array<char,kStateTextSize>>();assert(t->serializeState(json->data(),json->size())>0);
    assert(t->restoreState(json->data())&&t->performanceMode()==3);
    for(int part=0;part<4;++part) {const auto cfg=t->settingsForPart(part).engines;assert(cfg.enabled&&cfg.order==part%3&&cfg.engines[1]==(part+9)%13);}
    char directory[]="/tmp/thru-engine-list-XXXXXX";assert(mkdtemp(directory));assert(mkdir((std::string(directory)+"/captures").c_str(),0700)==0);
    std::snprintf(t->moduleDirectory.data(),t->moduleDirectory.size(),"%s",directory);t->persistenceEnabled=true;t->persistenceGeneration=1; // matches the explicit disk APIs used below.
    assert(t->writeClipFile());
    auto restored=splitFixture();assert(restored->readClipFileFromPath((std::string(directory)+"/thru-clips.bin").c_str()));
    const auto& saved=restored->clipSlots[t->activeClip];assert(saved.clipNotes[0].engineSlot==7&&saved.clipNotes[0].engineAlgorithm==4);
    assert(saved.singleSettings.engines.enabled&&saved.splitSettings[3].engines.engines[1]==12);
    // Format 23 migration leaves Engine List disabled, rather than guessing a program.
    const auto path=std::string(directory)+"/thru-clips.bin";auto* file=std::fopen(path.c_str(),"rb");assert(file);
    std::fseek(file,0,SEEK_END);const long size=std::ftell(file);std::rewind(file);
    std::vector<unsigned char> bytes(size-4);assert(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);
    // This note-bearing bank cannot be relabelled as old without removing its new metadata.
    // Use a clean bank to exercise the real old-format parser separately.
    for(auto& c:restored->clipSlots){c.clipNoteCount=0;c.singleSettings.engines={};for(auto& s:c.splitSettings)s.engines={};}
    std::snprintf(restored->moduleDirectory.data(),restored->moduleDirectory.size(),"%s",directory);restored->persistenceEnabled=true;assert(restored->writeClipFile());
    file=std::fopen(path.c_str(),"rb");assert(file);std::fseek(file,0,SEEK_END);const long cleanSize=std::ftell(file);std::rewind(file);bytes.resize(cleanSize-4);
    assert(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);stripVoiceEngineExtension(bytes);
    std::uint32_t hash=2166136261u;for(auto b:bytes){hash^=b;hash*=16777619u;}
    file=std::fopen(path.c_str(),"wb");assert(file);assert(std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size());assert(writeU32(file,hash));std::fclose(file);
    assert(restored->readClipFileFromPath(path.c_str()));assert(!restored->clipSlots[0].singleSettings.engines.enabled);
    t.reset();restored.reset();removeBackgroundTestDirectory(directory);
    std::puts("s3g Thru Engine List held-note capture, deterministic replay, JSON and bank migration: PASS");
}
void testVoiceEngineMidiAndWorkers() {
    for(int mode:{1,2,3}) {
        auto t=splitFixture();t->setPerformanceMode(mode);const int focus=t->splitFocus,lane=t->selectedLane;
        nrpn(*t,6,"voice_engine_mode",16383);nrpn(*t,6,"voice_engine_order",s3g_thru_midi::encode(midiParameter("voice_engine_order"),2,5));
        const auto& p=midiParameter("voice_engine_1");nrpn(*t,6,p.key,s3g_thru_midi::encode(p,12,14));
        assert(!t->settingsForPart(0).engines.enabled&&t->settingsForPart(1).engines.enabled);
        assert(t->settingsForPart(1).engines.order==2&&t->settingsForPart(1).engines.engines[0]==12);
        assert(t->splitFocus==focus&&static_cast<int>(t->selectedLane)==lane&&t->listUsesAlgorithm(12));
        assert(t->spectralRequested());
        nrpn(*t,6,"voice_engine_edit",16383);assert(t->settingsForPart(1).engines.edit==t->engineSlotCount()-1);
        nrpn(*t,6,"algorithm",s3g_thru_midi::encode(midiParameter("algorithm"),8,14));
        assert(t->settingsForPart(1).engines.engines[t->engineSlotCount()-1]==8&&t->splitFocus==focus);
    }
    {
        auto t=splitFixture();nrpn(*t,6,"voice_engine_mode",16383);
        nrpn(*t,6,"voice_engine_2",s3g_thru_midi::encode(midiParameter("voice_engine_2"),2,14));
        nrpn(*t,6,"voice_engine_4",s3g_thru_midi::encode(midiParameter("voice_engine_4"),12,14));
        nrpn(*t,6,"voice_engine_edit",16383);assert(t->liveControlsForPart(1).algorithm==Engine::PlaybackAlgorithm::formant);
        t->setPerformanceMode(3);
        assert(t->settingsForPart(1).engines.edit==1&&t->liveControlsForPart(1).algorithm==Engine::PlaybackAlgorithm::grains&&t->splitFocus==0);
    }
    std::puts("s3g Thru Engine List part-addressed MIDI, editor focus and spectral preparation: PASS");
}

void testVoiceEngineMixedRenderingAndFades() {
    for(int mode=0;mode<4;++mode) {
        auto t=splitFixture();t->setPerformanceMode(mode);t->stopSampleWorker();
        for(std::size_t lane=0;lane<4;++lane) {
            auto spectral=std::make_unique<s3g_daisy::ThruSpectralBank>();
            auto wavesets=std::make_unique<s3g_daisy::ThruWavesetBank>();
            spectral->analyze(t->engine.laneData(lane,0),t->engine.laneData(lane,1),t->engine.laneFrames(lane),t->sampleRate,t->wavesetSourceRevisions[lane]);
            wavesets->analyze(t->engine.laneData(lane,0),t->engine.laneData(lane,1),t->engine.laneFrames(lane),t->sampleRate,t->wavesetSourceRevisions[lane]);
            t->preparedSpectralBanks[lane]=std::move(spectral);t->preparedWavesetBanks[lane]=std::move(wavesets);
        }
        t->spectralWorker.exchangePreparedBanks(t->preparedSpectralBanks);t->wavesetWorker.exchangePreparedBanks(t->preparedWavesetBanks);
        const int parts=mode?t->partCount():1,cap=t->engineSlotCount();
        std::array<float,kBlockFrames*2> audio{};
        for(int pass=0;pass<2;++pass) {
            t->panic();
            for(int part=0;part<parts;++part) {
                t->selectSplitPart(part,false);setParam(t.get(),"voice_engine_mode","1");
                for(int slot=0;slot<cap;++slot)setListSlot(*t,slot,(part*cap+slot+pass*5)%13);
                t->midiPart=part;for(int note=0;note<cap;++note)t->noteOn(60+note,100);
            }
            float peak=0;
            for(int block=0;block<100;++block) {
                t->render(nullptr,kBlockFrames,audio.data());
                for(float value:audio){assert(std::isfinite(value));peak=std::max(peak,std::abs(value));}
            }
            assert(peak>.001f);
            for(const auto& v:t->voices)if(v.active)assert(static_cast<int>(v.engine.playbackAlgorithm())==v.assignedAlgorithm);
        }
    }
    // Cancel a transition to a different list and recover the source program and RNG.
    auto t=splitFixture();t->setPerformanceMode(0);renderSplit(*t,2);t->stopSampleWorker();
    setParam(t.get(),"voice_engine_mode","1");setListSlot(*t,0,4);setListSlot(*t,1,7);setParam(t.get(),"voice_engine_order","2");
    t->currentLaneSourceKinds.fill(LaneSourceKind::capture);t->padNoteOn(68,100);renderSplit(*t,20);
    t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    auto& destination=t->clipSlots[1].singleSettings.engines;destination.enabled=false;destination.order=1;destination.engines[0]=2;
    t->patternFadeMs=1000;const auto random=t->voiceEngineRandom;t->voiceEngineTraversal[0].next=3;t->voiceEngineTraversal[0].shuffle=7;
    assert(t->prepareClipBank(1));t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));assert(t->activatePreparedClip());
    std::array<float,kBlockFrames*2> audio{};t->render(nullptr,kBlockFrames,audio.data());t->setPatternPosition(.5f);
    for(int n=0;n<20;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
    t->returnPatternSource();renderSourceReturn(*t,audio);
    assert(t->voiceEngineSettings.enabled&&t->voiceEngineSettings.order==2&&t->voiceEngineSettings.engines[1]==7&&t->voiceEngineRandom==random&&t->voiceEngineTraversal[0].next==3&&t->voiceEngineTraversal[0].shuffle==7);
    t->captureActiveClip(true);assert(ThruInstance::clipUsesVoiceEngine(*t->activeClipSlot(),7));
    setListSlot(*t,7,12);t->captureActiveClip(true);assert(ThruInstance::clipUsesVoiceEngine(*t->activeClipSlot(),12));
    assert(t->prepareClipBank(0));assert(t->preparedSpectralBanks[0]);
    std::puts("s3g Thru mixed-engine eight-voice rendering, clip preparation and fade cancellation: PASS");
}
