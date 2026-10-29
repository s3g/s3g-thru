void testHardwarePanic()
{
    for(bool split:{false,true}) {
        auto t=splitFixture();if(!split)t->setSplitMode(false);
        t->controls.ampReleaseMs=10000;
        if(split){t->selectSplitPart(1);t->controls.ampReleaseMs=10000;t->selectSplitPart(0);}
        t->shiftHeld=true;t->padNoteOn(68,100);t->padNoteOn(72,100);t->shiftHeld=false;
        renderSplit(*t,20);t->beginClipRecording();renderSplit(*t,40);
        t->padMode=PadMode::rolls;t->padNoteOn(69,100);renderSplit(*t,3);
        assert(t->rolls[0].active);
        const auto sources=t->engine.loadedLaneMask();
        for(int note=40;note<52;++note)t->noteOn(note,100);
        std::size_t pending=0;for(const auto& v:t->voices)if(v.stealPending)++pending;
        assert(pending>0);
        setParam(t.get(),"panic","1");
        assert(!t->clipRecording&&!t->clipPlaying&&!t->clipLaunchPending&&t->queuedClip==-1);
        assert(t->activeClipSlot()->clipNoteCount>=2);
        assert(t->controls.ampReleaseMs==10000&&t->engine.loadedLaneMask()==sources);
        for(int note:t->latchedPadNotes)assert(note==-1);
        for(const auto& r:t->rolls)assert(!r.held&&r.target==0);
        std::size_t retained=0;
        for(const auto& v:t->voices){assert(!v.held);if(v.stealPending)++retained;}
        assert(retained==pending); // Keep the outgoing controls until that fade completes.
        renderSplit(*t,12);
        for(const auto& v:t->voices)assert(!v.active);
        for(const auto& r:t->rolls)assert(!r.active);
        t->padMode=PadMode::keys;t->padNoteOn(70,100);renderSplit(*t,5);
        bool held=false;for(const auto& v:t->voices)if(v.held){held=true;assert(!v.panicRelease);}
        assert(held);
    }
}

void testPreviewRoutesAndTransitions()
{
    for(bool split:{false,true})for(int owner:{0,1}) {
        auto t=splitFixture();if(!split)t->setSplitMode(false);
        t->controls.effectDrive=1;t->controls.effectTapeMix=1;
        t->controls.effectTapeDrive=1;t->controls.effectTapeHissDb=-84;
        if(split){t->splitParts[owner].settings.muted=true;t->controls.splitBlend=owner?0:1;}
        renderSplit(*t,30);
        t->sampleRequestLane=owner*2;t->sampleRequestPreview=true;
        t->sampleStageFrames=65536;t->sampleStageRate=t->sampleRate;
        std::fill_n(t->sampleStage.get(),65536,.2f);
        std::fill_n(t->sampleStage.get()+t->laneCapacityFrames,65536,-.1f);
        t->previewLevel=t->previewLevelState=1;
        t->sampleLoadState.store(static_cast<int>(SampleLoadState::ready));
        renderSplit(*t,30);
        if(split)assert(std::abs(t->voiceMix[0].back())<1e-5f);
        setParam(t.get(),"preview_route","1");
        float previous=t->voiceMix[0].back(),largest=0;
        for(int block=0;block<40;++block) {
            renderSplit(*t,1);
            for(float sample:t->voiceMix[0]){largest=std::max(largest,std::abs(sample-previous));previous=sample;}
        }
        assert(largest<.04f);
        assert(std::abs(t->directPreview[0].back()-.2f)<1e-5f);
        assert(std::abs(t->directPreview[1].back()+.1f)<1e-5f);
        // The previous Part audition can leave a decaying effect history.
        assert(std::abs(t->voiceMix[0].back()-.2f)<.002f);
        if(split)for(float input:t->splitDelayInput[0])assert(input==0);
        // Retargeting a Direct preview retains its own fade on the dry bus.
        t->beginPreviewTransitionTail();t->previewActive=false;
        renderSplit(*t,1);assert(t->directPreview[0][0]>.19f);
        renderSplit(*t,5);assert(t->directPreview[0].back()==0);
        char state[16384] {};assert(t->serializeState(state,sizeof(state))>0);
        t->previewRoute=0;t->restoreState(state);assert(t->previewRoute==1);
        std::string oversized(state);
        const std::string field="\"preview_route\":1,";
        const auto start=oversized.find(field);assert(start!=std::string::npos);
        oversized.replace(start,field.size(),"\"preview_route\":100000000000,");
        assert(t->restoreState(oversized.c_str())&&t->previewRoute==1);
        std::string legacy(state),property="\"preview_route\":1,";
        const auto position=legacy.find(property);assert(position!=std::string::npos);
        legacy.erase(position,property.size());assert(t->restoreState(legacy.c_str()));
        assert(t->previewRoute==0);
    }
}

void testMixReadouts()
{
    auto t=splitFixture();char text[128] {};
    auto read=[&](int reasonB) {
        assert(getParam(t.get(),"mix_status",text,sizeof(text))>0);
        float a=0,b=0;int ac=0,bc=0,ar=0,br=0;
        assert(std::sscanf(text,"%f,%f,%d,%d,%d,%d",&a,&b,&ac,&bc,&ar,&br)==6);
        assert(br==reasonB);return std::array<float,4>{a,b,float(ac),float(bc)};
    };
    read(5);t->padNoteOn(68,100);t->padNoteOn(72,100);renderSplit(*t,30);
    const auto v=read(0);assert(v[0]>.01f&&v[1]>.01f&&v[2]==1&&v[3]==1);
    t->splitParts[1].settings.muted=true;read(1);
    t->splitParts[1].settings.muted=false;t->controls.splitLevelB=0;read(2);
    t->controls.splitLevelB=1;t->controls.splitBlend=0;read(3);
    renderSplit(*t,500);assert(read(3)[1]<1e-5f);
    auto empty=std::make_unique<ThruInstance>();assert(empty->prepare(nullptr));empty->setSplitMode(true);
    assert(getParam(empty.get(),"mix_status",text,sizeof(text))>0);
    assert(std::strstr(text,",0,0,4,4"));
}

void testSaveStatusAndRecovery()
{
    char path[]="/tmp/thru-save-status.XXXXXX";assert(mkdtemp(path));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(path));
    assert(t->saveStatus()==3);setParam(t.get(),"locus","0.2");assert(t->saveStatus()==2);
    t->serviceMaintenance();waitForTestBackground(*t,true);assert(t->saveStatus()==3);
    const std::string captures=std::string(path)+"/captures";
    assert(rmdir(captures.c_str())==0);
    FILE* blocked=std::fopen(captures.c_str(),"wb");assert(blocked);std::fclose(blocked);
    t->startRecording();assert(t->engine.recording()&&t->saveStatus()==1);
    renderSplit(*t,20);t->finishRecording();assert(t->saveStatus()==2);
    for(int attempt=0;attempt<3000&&!t->saveFailed;++attempt){renderSplit(*t,1);usleep(500);}
    assert(t->saveFailed&&t->saveStatus()==4&&t->pendingCaptureSaveMask!=0);
    assert(t->engine.laneFrames(t->selectedLane)>0);
    assert(unlink(captures.c_str())==0&&mkdir(captures.c_str(),0700)==0);
    t->laneRetryFrame=0;t->serviceMaintenance();waitForTestBackground(*t,true);
    assert(t->saveStatus()==3&&!t->saveFailed);
    t.reset();removeBackgroundTestDirectory(path);
}
