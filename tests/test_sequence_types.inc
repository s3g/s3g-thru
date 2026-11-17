void testSavedSequenceTypes(){
    using namespace s3g_thru_sequence;
    using namespace s3g_thru_generate;
    auto t=splitFixture();t->stopBackgroundWork();t->setPerformanceMode(6);
    for(const auto& lane:t->sequenceLive.tracks)assert(lane.timeMode&&lane.generator.type==0);
    setParam(t.get(),"seq_length","13");setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_recipe_hits","5");
    auto& lane=t->sequenceLive.tracks[0];assert(lane.generator.type==1&&lane.generator.ready&&lane.timeMode);
    const auto hits=[&](){int n=0;for(int i=0;i<lane.length;++i)n+=lane.trigs[i].enabled;return n;};assert(hits()==5);
    t->startClipPlaybackNow();renderSplit(*t,31);const auto beat=t->sequenceBeat;const auto offsets=t->sequenceBeatOffsets;
    setParam(t.get(),"seq_recipe_hits","6");assert(hits()==6&&t->sequenceBeat==beat&&t->sequenceBeatOffsets==offsets);
    setParam(t.get(),"seq_step","2");const bool on=lane.trigs[1].enabled;
    setParam(t.get(),"seq_toggle","1");assert(lane.trigs[1].enabled!=on);
    setParam(t.get(),"seq_note","7");setParam(t.get(),"seq_burst","3");
    setParam(t.get(),"seq_recipe_hits","7");
    assert(lane.trigs[1].enabled!=on&&lane.trigs[1].locks.values[1]==7&&lane.trigs[1].count==3);
    setParam(t.get(),"seq_duration_mode","0");assert(!lane.timeMode&&lane.generator.values[17]<=32);
    setParam(t.get(),"seq_type","2");assert(!lane.timeMode&&lane.generator.type==2&&lane.generator.values[2]>=1);
    assert(lane.trigs[1].locks.values[1]==7);
    setParam(t.get(),"seq_duration_mode","1");assert(lane.timeMode&&lane.generator.values[17]>=10);
    const auto generated=lane.trigs;const auto config=lane.generator.values;
    setParam(t.get(),"seq_to_normal","1");assert(lane.generator.type==0&&lane.generator.values==config);
    for(int i=0;i<32;++i)assert(sameStep(lane.trigs[i],generated[i]));
    setParam(t.get(),"seq_duration_mode","0");setParam(t.get(),"seq_type","1");assert(!lane.timeMode&&lane.generator.values[1]==config[1]&&lane.generator.values[18]<=32);
    t->selectSplitPart(1);setParam(t.get(),"seq_type","2");assert(t->sequenceLive.tracks[0].generator.type==1&&t->sequenceLive.tracks[1].generator.type==2);
    const auto saved=t->sequenceLive.tracks[1].trigs;
    setParam(t.get(),"seq_recipe_map","3");assert(std::strstr(t->sequenceTypeStatus[1].data(),"NO FILE"));
    for(int i=0;i<32;++i)assert(sameStep(saved[i],t->sequenceLive.tracks[1].trigs[i]));
    const auto* type=s3g_thru_midi::find(3886);assert(type&&std::strcmp(type->key,"seq_type")==0);
    midiWrite(*t,*type,2,1);assert(t->splitFocus==1&&t->sequenceLive.tracks[2].generator.type==1);
    assert(midiRead(*t,*type,2)==1&&midiRead(*t,*type,1)==2);
    auto undo=sequenceFixture();setParam(undo.get(),"seq_type","1");setParam(undo.get(),"seq_recipe_hits","3");undo->restoreUndo();assert(!undo->undoAvailable);
    setParam(undo.get(),"seq_recipe_hits","4");assert(undo->undoAvailable);undo->restoreUndo();assert(!undo->undoAvailable);
    setParam(undo.get(),"seq_eq_low","2");undo->restoreUndo();assert(!undo->undoAvailable);setParam(undo.get(),"seq_eq_low","3");assert(undo->undoAvailable);undo.reset();
    // Persistence includes recipes, manual overrides, cached notes and mixer banks.
    char directory[]="/tmp/thru-saved-types.XXXXXX";assert(mkdtemp(directory));
    auto disk=std::make_unique<ThruInstance>();assert(disk->prepare(directory));disk->stopBackgroundWork();disk->setPerformanceMode(6);
    setParam(disk.get(),"seq_length","11");setParam(disk.get(),"seq_type","1");setParam(disk.get(),"seq_recipe_hits","4");
    setParam(disk.get(),"seq_direction","4");setParam(disk.get(),"seq_step","3");setParam(disk.get(),"seq_note","-5");setParam(disk.get(),"seq_burst","3");setParam(disk.get(),"seq_toggle","1");
    disk->selectSplitPart(1);setParam(disk.get(),"seq_type","2");
    disk->selectSplitPart(0);setParam(disk.get(),"seq_eq_low","-3");setParam(disk.get(),"seq_pan_2",".6");
    disk->setPerformanceMode(0);setParam(disk.get(),"seq_eq_low","2");
    disk->setPerformanceMode(6);assert(disk->sequenceLive.mix.strips[0].lowDb==-3);
    disk->captureActiveClip(false);const auto before=disk->sequenceLive;assert(disk->savePersistentState());disk.reset();
    disk=std::make_unique<ThruInstance>();assert(disk->prepare(directory));disk->stopBackgroundWork();
    assert(disk->sequenceMode()&&disk->sequenceLive.tracks[0].timeMode&&disk->sequenceLive.initialized);
    for(int p=0;p<4;++p){const auto& a=before.tracks[p];const auto& b=disk->sequenceLive.tracks[p];assert(sameLane(a,b));
        assert(a.generator.type==b.generator.type&&a.generator.ready==b.generator.ready&&a.generator.values==b.generator.values&&a.generator.enabledMask==b.generator.enabledMask&&a.generator.burstMask==b.generator.burstMask);
        for(int i=0;i<32;++i)assert(sameStep(a.generator.base[i],b.generator.base[i]));}
    assert(disk->sequenceLive.mix.strips[1].pan==.6f);disk->setPerformanceMode(0);assert(disk->sequenceLive.mix.strips[0].lowDb==2);
    // A format-42 bank remains Normal and keeps an explicitly selected STEP.
    disk->setPerformanceMode(6);setParam(disk.get(),"seq_duration_mode","0");disk->captureActiveClip(false);assert(disk->savePersistentState());
    const auto path=std::string(directory)+"/thru-clips.bin";auto* file=std::fopen(path.c_str(),"rb");assert(file);
    std::fseek(file,0,SEEK_END);const auto size=std::ftell(file);std::rewind(file);
    std::vector<unsigned char> bytes(std::size_t(size)-4);assert(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);
    stripSequenceTypes(bytes);assert(bytes[8]==42);std::uint32_t checksum=2166136261u;for(auto v:bytes){checksum^=v;checksum*=16777619u;}
    file=std::fopen(path.c_str(),"wb");assert(file);assert(std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size());assert(writeU32(file,checksum));std::fclose(file);
    auto legacy=std::make_unique<ThruInstance>();assert(legacy->prepare(nullptr));legacy->stopBackgroundWork();assert(legacy->readClipFileFromPath(path.c_str()));
    assert(legacy->clipSlots[0].sequence.initialized&&!legacy->clipSlots[0].sequence.tracks[0].timeMode&&legacy->clipSlots[0].sequence.tracks[0].generator.type==0);
    assert(legacy->sequenceLive.tracks[0].direction==0&&legacy->sequenceLive.tracks[0].directionSeed!=0);
    legacy.reset();disk.reset();removeBackgroundTestDirectory(directory);
    std::puts("Saved Sequence types: TIME defaults, independent Normal/Euclidean/Onset, deterministic materialization, preserved phase/overrides, print, unavailable source, part-addressed MIDI and format-43 reopen: PASS");
}
void testCommonOutputMixer(){
    using namespace s3g_thru_sequence;
    for(int count:{1,2,4}){
        std::array<std::array<std::array<float,128>,2>,4> bus{};
        for(auto& part:bus)for(auto& side:part)side.fill(.2f);
        const auto dry=bus;Mix mix;StripProcessor p;p.process(128,bus,mix,44100,count);assert(bus==dry);
        mix.strips[count-1].pan=1;
        for(int i=0;i<80;++i){bus=dry;p.process(128,bus,mix,44100,count);}
        assert(std::abs(bus[count-1][0][127])<.0001f&&std::abs(bus[count-1][1][127]-.2f)<.0001f);
        if(count>1){assert(bus[0]==dry[0]);mix.strips[count-1].solo=true;for(int i=0;i<80;++i){bus=dry;p.process(128,bus,mix,44100,count);}assert(bus[0][0][127]<.0001f);}
        mix.strips[count-1].muted=true;for(int i=0;i<80;++i){bus=dry;p.process(128,bus,mix,44100,count);}assert(std::abs(bus[count-1][1][127])<.0001f);
    }
    auto t=splitFixture();t->stopBackgroundWork();
    for(int mode=0;mode<7;++mode){t->setPerformanceMode(mode);assert(t->performanceMode()==mode);assert(t->outputMixerCount()==(mode==3||mode==6?4:mode==0||mode==5?1:2));
        setParam(t.get(),"seq_eq_low",mode%2?"-4":"3");
        setParam(t.get(),"seq_pan_1","-.5");
        std::array<char,1200> json{};assert(t->outputMixerState(json.data(),json.size())>0&&std::strstr(json.data(),"\"strips\":["));
        if(t->outputMixerCount()>1){setParam(t.get(),"seq_duck_key","1");setParam(t.get(),"seq_duck_depth","9");float depth=0;t->outputMixerValue("seq_duck_depth",0,depth);assert(depth==9);
            if(t->legacyDualMixer())assert(t->controls.splitPriority==-9);}
    }
    for(int mode=6;mode>=0;--mode){t->setPerformanceMode(mode);assert(t->sequenceLive.mix.strips[0].lowDb==(mode%2?-4:3));}
    t->setPerformanceMode(6);t->clipRecording=true;t->clipReplacing=false;
    setParam(t.get(),"seq_strip_edit","2,5,1");assert(t->settingsForPart(2).muted&&t->splitFocus==0);t->clipRecording=false;
    std::puts("Common output mixer: exact neutral bypass, count-aware stereo pan/solo/mute, all seven mode strips/banks, canonical dual duck and focused-independent Sequence mute: PASS");
}

void testTypedSequencePhysicalPads(){
    using namespace s3g_thru_sequence;
    for(int type:{1,2}){
        auto t=sequenceFixture();setParam(t.get(),"seq_length","8");setParam(t.get(),"seq_type",type==1?"1":"2");setParam(t.get(),"seq_recipe_hits","2");
        assert(t->activeClipSlot()->clipPointCount==0);
        auto& lane=t->sequenceLive.tracks[0];
        t->startClipPlaybackNow();renderSplit(*t,1);
        std::uint32_t enabled=0;
        for(int row=0;row<8;++row){
            const bool before=t->sequencePlayback.tracks[0].trigs[row].enabled;
            const int pad=68+stepPad(row);t->padNoteOn(pad,100);t->padNoteOff(pad);renderSplit(*t,1);
            assert(lane.generator.type==type&&lane.trigs[row].enabled!=before&&t->sequencePlayback.tracks[0].trigs[row].enabled!=before);
            if(!before)enabled|=1u<<row;
        }
        assert(lane.generator.enabledMask==255&&lane.generator.enabledValues==enabled);
        setParam(t.get(),"seq_recipe_hits","7");setParam(t.get(),"seq_recipe_seed","1979");++t->wavesetSourceRevisions[0];t->serviceSequenceTypes();renderSplit(*t,1);
        for(int row=0;row<8;++row)assert(t->sequencePlayback.tracks[0].trigs[row].enabled==bool(enabled&(1u<<row)));
        // Holding a step and releasing it must not toggle the trig.
        const bool heldState=lane.trigs[3].enabled;t->padNoteOn(68+stepPad(3),100);setParam(t.get(),"seq_hold","3");setParam(t.get(),"seq_note","7");
        t->padNoteOff(68+stepPad(3));renderSplit(*t,1);assert(lane.trigs[3].enabled==heldState&&lane.trigs[3].locks.values[1]==7);
        t->deleteHeld=true;t->padNoteOn(68+stepPad(3),100);t->padNoteOff(68+stepPad(3));t->deleteHeld=false;renderSplit(*t,1);
        assert(!lane.trigs[3].enabled&&!t->sequencePlayback.tracks[0].trigs[3].enabled&&lane.trigs[3].locks.mask==0&&lane.generator.base[3].locks.mask==0);
        // A pad belongs to the lane selected on press, even if focus changes.
        const bool ownerState=t->sequencePlayback.tracks[0].trigs[2].enabled;
        t->padNoteOn(68+stepPad(2),100);t->selectSplitPart(1);t->padNoteOff(68+stepPad(2));renderSplit(*t,1);
        assert(t->splitFocus==1&&lane.trigs[2].enabled!=ownerState&&t->sequencePlayback.tracks[0].trigs[2].enabled!=ownerState);
        assert(t->sequenceLive.tracks[1].generator.enabledMask==0);
        t->selectSplitPart(0);setParam(t.get(),"seq_regenerate","1");renderSplit(*t,1);
        assert(lane.generator.type==type&&lane.generator.enabledMask==0);
        // Physical pad edits during a performance take remain authored trigs.
        t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();renderSplit(*t,1);
        const bool recordState=lane.trigs[0].enabled;t->padNoteOn(92,100);t->padNoteOff(92);renderSplit(*t,1);
        assert(lane.trigs[0].enabled!=recordState&&lane.generator.enabledMask==1);
        t->finishClipRecording();t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);
        assert(t->sequencePlayback.tracks[0].trigs[0].enabled!=recordState&&lane.generator.type==type);
    }
    std::puts("Typed physical sequence pads: EUC/ONSET tap overrides, source/recipe rebuilds, held editor, X+pad, press-owner focus and recording/restart retain manual trigs without PRINT: PASS");
}
void testTypedPadsOverRecordedRecipes(){
    using namespace s3g_thru_sequence;
    for(int type:{1,2}){
        auto t=sequenceFixture();setParam(t.get(),"seq_length","8");setParam(t.get(),"seq_type",type==1?"1":"2");
        if(type==2)setParam(t.get(),"seq_recipe_regions","2");
        setParam(t.get(),"seq_recipe_hits","2");t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();renderSplit(*t,20);
        const char* key=type==1?"seq_recipe_hits":"seq_recipe_regions";
        setParam(t.get(),key,"7");renderSplit(*t,20);setParam(t.get(),key,"2");renderSplit(*t,20);t->finishClipRecording();t->setClipPlaying(false);
        t->startClipPlaybackNow();renderSplit(*t,1);
        const auto* clip=t->activeClipSlot();const auto curve=kClipRecipeOffset+(type==1?0:7);
        assert(clip->clipPointCounts[curve]==3);const auto first=clip->clipPointStarts[curve];const auto change=clip->clipPoints[first+1].timeMs;
        t->clipPositionMs=change;t->serviceSequenceBeforeBlock(1);
        std::array<int,2> different{-1,-1};int found=0;
        for(int i=0;i<8&&found<2;++i)if(t->sequencePlayback.tracks[0].trigs[i].enabled!=t->sequenceLive.tracks[0].trigs[i].enabled)different[found++]=i;
        assert(found==2);
        // Native pad and MIDI/API toggle must invert what is currently heard,
        // rather than the final stored recipe's opposite hit pattern.
        const bool before=t->sequencePlayback.tracks[0].trigs[different[0]].enabled;
        t->padNoteOn(68+stepPad(different[0]),100);t->padNoteOff(68+stepPad(different[0]));t->serviceSequenceBeforeBlock(1);
        assert(t->sequencePlayback.tracks[0].trigs[different[0]].enabled!=before);
        const bool midiBefore=t->sequencePlayback.tracks[0].trigs[different[1]].enabled;
        setParam(t.get(),"seq_step",std::to_string(different[1]+1).c_str());setParam(t.get(),"seq_toggle","1");t->serviceSequenceBeforeBlock(1);
        assert(t->sequencePlayback.tracks[0].trigs[different[1]].enabled!=midiBefore);
        for(double position:{0.0,double(change),double(clip->clipDurationMs-1),0.0}){
            t->clipPositionMs=position;t->serviceSequenceBeforeBlock(1);
            assert(t->sequencePlayback.tracks[0].trigs[different[0]].enabled!=before&&t->sequencePlayback.tracks[0].trigs[different[1]].enabled!=midiBefore);
        }
        t->captureActiveClip(false);t->setClipPlaying(false);t->activateStoredClip(0,false);t->startClipPlaybackNow();renderSplit(*t,1);
        assert(t->sequenceLive.tracks[0].generator.type==type&&t->sequencePlayback.tracks[0].trigs[different[0]].enabled!=before);
    }
    std::puts("Recorded EUC/ONSET recipes: native and MIDI toggles use current audible state, manual overrides survive later automation/loop wraps/clip recall and TYPE stays unchanged: PASS");
}
void testClearRecordedPerformance(){
    using namespace s3g_thru_sequence;
    for(int type:{0,1,2}){
        auto t=sequenceFixture();setParam(t.get(),"seq_length","8");setParam(t.get(),"seq_type",std::to_string(type).c_str());
        setParam(t.get(),"seq_recipe_hits","5");
        if(!type){setParam(t.get(),"seq_step","1");setParam(t.get(),"seq_toggle","1");}
        t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();renderSplit(*t,20);t->finishClipRecording();t->setClipPlaying(false);
        auto* clip=t->activeClipSlot();assert(clip->clipPointCount>0);
        setParam(t.get(),"seq_recipe_hits","8");
        t->sequenceStep=3;setParam(t.get(),"seq_note","7");t->padNoteOn(68+stepPad(2),100);t->padNoteOff(68+stepPad(2));
        const auto authored=t->sequenceLive;
        const auto sourcePaths=clip->laneSourcePaths;const auto kinds=clip->laneSourceKinds;
        const auto duration=clip->clipDurationMs;const auto beats=clip->recordedBeats;
        const auto points=clip->clipPointCount;
        t->startClipPlaybackNow();renderSplit(*t,1);
        if(type)assert(t->sequencePlayback.tracks[0].generator.values[1]==8); // editing replaces the static recording seed.
        const auto beat=t->sequenceBeat;const auto offsets=t->sequenceBeatOffsets;
        std::array<std::uint32_t,8> ages{};std::array<bool,8> held{};for(std::size_t i=0;i<t->voices.size();++i)if(t->voices[i].active&&t->voices[i].sequence.active){ages[i]=t->voices[i].age;held[i]=t->voices[i].held;}
        setParam(t.get(),"clip_clear_recording","1");assert(!t->lastError[0]);
        assert(t->clipPlaying&&t->sequenceBeat==beat&&t->sequenceBeatOffsets==offsets);
        assert(clip->clipPointCount==0&&clip->clipNoteCount==0&&clip->clipDurationMs==duration&&clip->recordedBeats==beats);
        assert(clip->laneSourcePaths==sourcePaths&&clip->laneSourceKinds==kinds);
        for(int p=0;p<4;++p){assert(s3g_thru_generate::sameLane(t->sequenceLive.tracks[p],authored.tracks[p]));assert(t->engine.laneLoaded(p));}
        for(std::size_t i=0;i<t->voices.size();++i)if(ages[i])assert(t->voices[i].age==ages[i]&&t->voices[i].held==held[i]);
        renderSplit(*t,1);if(type)assert(t->sequenceParameterValue("seq_recipe_hits")==8);
        assert(t->sequenceLive.tracks[0].generator.type==type);
        setParam(t.get(),"clip_clear_recording","1");assert(t->undoAvailable&&t->undoClip.clipPointCount==points); // repeated clear protects Undo.
        t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);
        if(type)assert(t->sequenceParameterValue("seq_recipe_hits")==8);
        t->restoreUndo();assert(t->activeClipSlot()->clipPointCount==points);
        t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);
        if(type)assert(t->sequenceParameterValue("seq_recipe_hits")==8);
        t->setClipPlaying(false);t->clipRecording=true;const auto revision=t->persistentRevision;
        setParam(t.get(),"clip_clear_recording","1");assert(t->lastError[0]&&t->activeClipSlot()->clipPointCount==points&&t->persistentRevision==revision);t->clipRecording=false;
    }
    // Non-Sequence recorded notes stop without releasing live notes or samples.
    auto t=splitFixture();t->stopBackgroundWork();t->clipRecordMode=ClipRecordMode::replace;
    t->beginClipRecording();t->padNoteOn(68,100);renderSplit(*t,10);t->padNoteOff(68);renderSplit(*t,10);t->finishClipRecording();
    assert(t->activeClipSlot()->clipNoteCount>0);const auto duration=t->activeClipSlot()->clipDurationMs;
    t->padNoteOn(72,100);const auto* clear=s3g_thru_midi::find(2701);assert(clear&&!clear->part);
    midiWrite(*t,*clear,0,1);assert(!t->lastError[0]&&t->activeClipSlot()->clipNoteCount==0&&t->activeClipSlot()->clipPointCount==0&&!t->clipPlaying);
    assert(t->activeClipSlot()->clipDurationMs==duration&&t->engine.laneLoaded(0));
    bool liveHeld=false;for(const auto& v:t->voices)liveHeld|=v.active&&v.held&&!v.clipOwned;assert(liveHeld);
    std::puts("Clear Recording: static HITS seed removed, Normal/EUC/ONSET trigs/locks/overrides/samples/span preserved, running clocks and heads retained, restart, idempotent Undo, recording refusal and MIDI/live-note scope: PASS");
}

void testSequencePersistentEdits(){
    auto t=sequenceFixture();setParam(t.get(),"seq_length","8");setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_recipe_hits","5");
    t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();
    const int speed=int(LockParameter::speed),delay=int(LockParameter::effectDelay);
    const auto speedA=kClipPartParameterOffset+speed,speedB=kClipPartParameterOffset+kLockParameterCount+speed;
    setParam(t.get(),"speed",".75");t->captureClipParameter(speed);setParam(t.get(),"effect_delay","100");t->captureClipParameter(delay);
    t->selectSplitPart(1);setParam(t.get(),"speed",".4");t->captureClipParameter(speed);t->selectSplitPart(0);
    renderSplit(*t,20);setParam(t.get(),"speed","1.25");t->captureClipParameter(speed);setParam(t.get(),"effect_delay","450");t->captureClipParameter(delay);
    setParam(t.get(),"seq_recipe_hits","3");setParam(t.get(),"seq_recipe_span","24");setParam(t.get(),"seq_rate","6");renderSplit(*t,20);t->finishClipRecording();
    auto* clip=t->activeClipSlot();
    assert(clip->clipPointCounts[speedA]==2&&clip->clipPointCounts[speedB]==1&&clip->clipPointCounts[delay]==2);
    // Some legitimate undo-baseline points have no indexed playback curve.
    auto& baseline=clip->clipPoints[clip->clipPointCount++];baseline.type=ClipPointType::layersBaseline;baseline.sourcePad=0;baseline.parameter=0;baseline.timeMs=0;baseline.value=.5f;
    t->rebuildClipPointIndex(*clip);t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);
    assert(t->splitClipControls[0].speed==.75f);
    const auto beat=t->sequenceBeat;setParam(t.get(),"speed","1.6");
    assert(t->sequenceBeat==beat&&clip->clipPointCounts[speedA]==0&&clip->clipPointCounts[speedB]==1&&clip->clipPointCounts[delay]==2);
    bool keptBaseline=false;for(int i=0;i<clip->clipPointCount;++i)keptBaseline|=clip->clipPoints[i].type==ClipPointType::layersBaseline;assert(keptBaseline);
    t->restoreUndo();assert(t->activeClipSlot()->clipPointCounts[speedA]==2&&t->controls.speed==1.25f);
    setParam(t.get(),"speed","1.6");setParam(t.get(),"seq_recipe_hits","8");setParam(t.get(),"seq_rate","3");setParam(t.get(),"effect_delay","675");
    assert(clip->clipPointCounts[kClipRecipeOffset]==0&&clip->clipPointCounts[kClipSequenceOffset+1]==0&&clip->clipPointCounts[delay]==0);
    assert(clip->clipPointCounts[kClipRecipeOffset+12]==2&&clip->clipPointCounts[speedB]==1); // untouched SPAN and B speed.
    t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);
    assert(t->sequenceParameterValue("seq_recipe_hits")==8&&t->sequencePlayback.tracks[0].rate==3&&t->splitClipControls[0].speed==1.6f&&t->clipControls.effectDelayMs==675);
    assert(t->sequenceParameterValue("seq_recipe_span")==12&&t->splitClipControls[1].speed==.4f);
    const auto spanStart=clip->clipPointStarts[kClipRecipeOffset+12];t->clipPositionMs=clip->clipPoints[spanStart+1].timeMs;t->serviceSequenceBeforeBlock(1);
    assert(t->sequenceParameterValue("seq_recipe_span")==24&&t->sequenceParameterValue("seq_recipe_hits")==8);
    const s3g_thru_midi::Parameter* speedParam=nullptr;for(const auto& p:s3g_thru_midi::parameters)if(!std::strcmp(p.key,"speed"))speedParam=&p;assert(speedParam);
    midiWrite(*t,*speedParam,1,1.7f);assert(t->splitFocus==0&&clip->clipPointCounts[speedB]==0&&t->liveControlsForPart(1).speed==1.7f);
    // Reopening keeps the edited settings and only the untouched automation.
    t->setClipPlaying(false);t->captureActiveClip(false);char folder[]="/tmp/thru-authored-edits.XXXXXX";assert(mkdtemp(folder));
    auto snapshot=std::make_unique<ThruInstance::PersistenceSnapshot>();snapshot->clips=t->clipSlots;snapshot->generation=1;std::snprintf(snapshot->directory.data(),snapshot->directory.size(),"%s",folder);assert(t->writeClipFile(snapshot.get()));snapshot.reset();
    auto restored=sequenceFixture();assert(restored->readClipFileFromPath((std::string(folder)+"/thru-clips.bin").c_str()));restored->startClipPlaybackNow();renderSplit(*restored,1);
    assert(restored->sequenceParameterValue("seq_recipe_hits")==8&&restored->sequencePlayback.tracks[0].rate==3&&restored->splitClipControls[0].speed==1.6f&&restored->splitClipControls[1].speed==1.7f&&restored->clipControls.effectDelayMs==675);
    assert(restored->activeClipSlot()->clipPointCounts[kClipRecipeOffset+12]==2);restored.reset();std::filesystem::remove_all(folder);
    // Legacy direct CC is also an authored edit, with the original value in Undo.
    auto legacy=sequenceFixture();auto* old=legacy->activeClipSlot();legacy->ensureSequenceClip();old->clipPointCount=1;old->clipPoints[0]={};old->clipPoints[0].type=ClipPointType::partParameter;old->clipPoints[0].sourcePad=0;old->clipPoints[0].parameter=speed;old->clipPoints[0].value=.5f;legacy->rebuildClipPointIndex(*old);
    legacy->applyMidiCc(22,100);assert(old->clipPointCounts[speedA]==0);legacy->restoreUndo();assert(old->clipPointCounts[speedA]==1&&legacy->controls.speed==1);
    legacy->setPerformanceMode(1);setParam(legacy.get(),"speed","1.7");assert(old->clipPointCounts[speedA]==1); // other modes retain their automation.
    legacy.reset();t.reset();
    auto bounded=sequenceFixture();setParam(bounded.get(),"seq_type","1");setParam(bounded.get(),"seq_duration_mode","1");setParam(bounded.get(),"seq_recipe_map","5");setParam(bounded.get(),"seq_recipe_to",".9");
    bounded->clipRecordMode=ClipRecordMode::replace;bounded->beginClipRecording();renderSplit(*bounded,20);
    setParam(bounded.get(),"seq_recipe_to",".3");setParam(bounded.get(),"seq_recipe_max","100");renderSplit(*bounded,20);bounded->finishClipRecording();bounded->setClipPlaying(false);bounded->startClipPlaybackNow();renderSplit(*bounded,1);
    setParam(bounded.get(),"seq_recipe_from",".5");setParam(bounded.get(),"seq_recipe_min","150");
    const auto* boundsClip=bounded->activeClipSlot();const auto endStart=boundsClip->clipPointStarts[kClipRecipeOffset+10];assert(boundsClip->clipPointCounts[kClipRecipeOffset+10]==2&&boundsClip->clipPointCounts[kClipRecipeOffset+17]==2);
    bounded->clipPositionMs=boundsClip->clipPoints[endStart+1].timeMs;bounded->serviceSequenceBeforeBlock(1);
    const auto& values=bounded->sequencePlayback.tracks[0].generator.values;
    assert(values[10]==.5f&&values[11]>.5f&&values[17]==150&&values[18]>=150);
    std::puts("Sequence authored edits: HITS/RATE/engine/global FX survive Stop/Play and reopen, only touched curves removed, other motion and unindexed baselines retained, correct Undo, part-addressed MIDI/legacy CC and unchanged other-mode behavior: PASS");
}
