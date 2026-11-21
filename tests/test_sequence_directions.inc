void testSequenceDirections(){
    using namespace s3g_thru_sequence;
    Lane l;l.length=4;
    const int expected[4][8]={{0,1,2,3,0,1,2,3},{3,2,1,0,3,2,1,0},{0,1,2,3,2,1,0,1},{0,1,2,3,3,2,1,0}};
    for(int d=0;d<4;++d){l.direction=d;for(int n=0;n<8;++n)assert(directionPosition(l,n).step==expected[d][n]);}
    for(int length=1;length<=32;++length)for(int d=0;d<5;++d)for(int rot=0;rot<32;++rot){
        l.length=length;l.direction=d;l.rotation=rot;l.directionSeed=1979;
        for(int tick=0;tick<128;++tick){const auto v=directionPosition(l,tick);assert(v.step>=0&&v.step<length&&v.cycle==tick/directionPeriod(l,d));}
    }
    l={};l.length=4;DirectionTraversal route;route.reset(l);
    assert(route.at(l,2).step==2);l.direction=1;route.synchronize(l,2,2);
    assert(route.at(l,2).step==2&&route.at(l,3).step==1&&route.at(l,4).step==0);
    l.direction=2;route.synchronize(l,3,4); // tick 4 already emitted early.
    assert(route.at(l,4).step==0&&route.at(l,5).step==1);
    for(int tick=5;tick<1000;++tick){l.direction=tick%5;route.synchronize(l,tick,tick);for(int k=std::max(0,tick-2);k<tick+3;++k)assert(route.at(l,k).step>=0&&route.at(l,k).step<4);}
    for(int direction=0;direction<5;++direction){
        Pattern pattern;pattern.active=true;
        for(int p=0;p<4;++p){auto& lane=pattern.tracks[p];lane.direction=direction;lane.directionSeed=1979+p;lane.length=4+p;lane.rotation=p;lane.rate=6+p%3;
            for(int i=0;i<32;++i){auto& step=lane.trigs[i];step.enabled=i%3!=1;step.micro=i%2?.41f:-.37f;step.probability=.72f;setCount(step,1+i%8);step.cycleEvery=2;step.cycleHit=1;}}
        for(int rate:{44100,48000}){
            const auto timeline=[&](int block){std::vector<std::array<std::int64_t,5>> all;EventList events;
                for(int n=0;n<rate;++n){const int count=std::min(block,rate-n);schedule(pattern,n*2.0/rate,2.0/rate,count,events);assert(!events.overflow);
                    for(int i=0;i<events.size;++i){const auto& e=events.items[i];all.push_back({n+e.offset,e.lane,e.step,e.child,e.tick});}n+=count-1;}return all;};
            const auto reference=timeline(1);for(int block:{31,128,511})assert(reference==timeline(block));
        }
    }
    // Repeated endpoints remain separate ticks, not a duplicate-row suppression.
    Pattern endpoints;endpoints.active=true;for(int p=1;p<4;++p)endpoints.tracks[p].enabled=false;
    auto& lane=endpoints.tracks[0];lane.length=4;lane.direction=3;lane.rate=4;for(auto& step:lane.trigs)step.enabled=true;
    EventList events;schedule(endpoints,0,.001,2000,events);assert(events.size==8&&events.items[3].step==3&&events.items[4].step==3&&events.items[3].tick!=events.items[4].tick);
    lane.direction=4;lane.directionSeed=1979;const auto first=directionPosition(lane,37).step;assert(directionPosition(lane,37).step==first);
    auto t=sequenceFixture();setParam(t.get(),"seq_direction","3");setParam(t.get(),"seq_length","4");
    for(auto& step:t->sequenceLive.tracks[0].trigs)step.enabled=true;t->commitSequenceEdit();
    const auto* midi=s3g_thru_midi::find(3910);assert(midi);midiWrite(*t,*midi,1,4);assert(t->splitFocus==0&&t->sequenceLive.tracks[1].direction==4&&midiRead(*t,*midi,1)==4);
    t->captureActiveClip(false);t->clipSlots[1]=t->clipSlots[0];t->clipSlots[1].sequence.tracks[0].direction=1;
    t->startClipPlaybackNow();renderSplit(*t,1);t->activateStoredClip(1,false);renderSplit(*t,1);
    assert(t->sequencePlayback.tracks[0].direction==1&&t->sequenceLaneCursor(0,t->sequencePlayback.tracks[0])==3);
    std::puts("Sequence DIR: five bounded routes, distinct endpoint semantics, cycle periods, deterministic RAND, block-independent micro/Burst timing, safe live edits, MIDI ownership and new-clip phase reset: PASS");
}
void testSequenceRecipeRecording(){
    auto t=sequenceFixture();setParam(t.get(),"seq_duration_mode","1");setParam(t.get(),"seq_length","13");setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_recipe_hits","4");
    t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();assert(t->clipRecording);
    renderSplit(*t,30);setParam(t.get(),"seq_recipe_hits","7");setParam(t.get(),"seq_direction","1");
    assert(t->sequenceLive.tracks[0].generator.values[1]==7&&t->sequenceLive.tracks[0].direction==1);
    renderSplit(*t,30);setParam(t.get(),"seq_recipe_hits","3");setParam(t.get(),"seq_recipe_span","12");setParam(t.get(),"seq_recipe_min","2000");
    assert(t->sequenceLive.tracks[0].generator.values[18]==2000); // linked MAX is captured too.
    setParam(t.get(),"seq_type","0");assert(t->sequenceLive.tracks[0].generator.type==1);
    setParam(t.get(),"seq_to_normal","1");assert(t->sequenceLive.tracks[0].generator.type==1);
    renderSplit(*t,35);t->finishClipRecording();t->setClipPlaying(false);
    const auto* clip=t->activeClipSlot();const auto first=clip->clipPointStarts[kClipRecipeOffset];
    assert(clip->clipPointCounts[kClipRecipeOffset]==3&&clip->clipPoints[first].timeMs==0&&clip->clipPoints[first].value==4);
    const auto change1=clip->clipPoints[first+1].timeMs,change2=clip->clipPoints[first+2].timeMs;
    assert(change1>0&&change2>change1);
    assert(clip->clipPointCounts[kClipRecipeOffset+17]==2); // MAX seed plus linked change.
    t->startClipPlaybackNow();renderSplit(*t,1);
    assert(t->sequencePlayback.tracks[0].generator.values[1]==4&&t->sequenceParameterValue("seq_recipe_hits")==4);
    const auto hitCount=[&](){int count=0;const auto& lane=t->sequencePlayback.tracks[0];for(int i=0;i<lane.length;++i)count+=lane.trigs[i].enabled;return count;};assert(hitCount()==4);
    auto builds=t->sequenceRecipeBuilds[0];
    t->clipPositionMs=change1-1;t->serviceSequenceRecipePlayback();assert(t->sequenceRecipeBuilds[0]==builds);
    t->clipPositionMs=change1;t->serviceSequenceRecipePlayback();assert(hitCount()==7&&t->sequenceParameterValue("seq_recipe_hits")==7&&t->sequenceRecipeBuilds[0]==builds+1);
    t->clipPositionMs=change2;t->serviceSequenceRecipePlayback();assert(hitCount()==3&&t->sequencePlayback.tracks[0].generator.values[17]==2000&&t->sequencePlayback.tracks[0].generator.values[18]==2000);
    builds=t->sequenceRecipeBuilds[0];for(int i=0;i<100;++i)t->serviceSequenceRecipePlayback();assert(t->sequenceRecipeBuilds[0]==builds);
    t->clipPositionMs=0;t->serviceSequenceRecipePlayback();assert(hitCount()==4&&t->sequencePlayback.tracks[0].generator.values[17]==50);
    char state[4096]{};assert(t->sequenceState(state,sizeof(state))>0&&std::strstr(state,"\"steps\":["));
    setParam(t.get(),"seq_recipe_hits","6");t->clipPositionMs=change2;t->serviceSequenceRecipePlayback();assert(hitCount()==6); // authored override survives restart.
    t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);assert(hitCount()==6&&clip->clipPointCounts[kClipRecipeOffset]==0);
    // Untouched duration and pitch motion still plays across the loop.
    bool sawLow=false,sawHigh=false;for(int i=0;i<100;++i){renderSplit(*t,1);assert(hitCount()==6);const float value=t->sequencePlayback.tracks[0].generator.values[17];sawLow|=value==50;sawHigh|=value==2000;}assert(sawLow&&sawHigh);
    assert(t->sequenceEventCount[0]>0&&t->sequenceLive.tracks[0].generator.type==1);
    // Native save/reopen keeps recorded recipe curves and the random seed.
    t->setClipPlaying(false);t->captureActiveClip(false);
    char directory[]="/tmp/thru-recipe-record.XXXXXX";assert(mkdtemp(directory));
    auto snapshot=std::make_unique<ThruInstance::PersistenceSnapshot>();snapshot->clips=t->clipSlots;snapshot->generation=1;
    std::snprintf(snapshot->directory.data(),snapshot->directory.size(),"%s",directory);
    assert(t->writeClipFile(snapshot.get()));snapshot.reset();
    auto restored=sequenceFixture();assert(restored->readClipFileFromPath((std::string(directory)+"/thru-clips.bin").c_str()));
    assert(restored->activeClipSlot()->clipPointCounts[kClipRecipeOffset]==0&&restored->activeClipSlot()->clipPointCounts[kClipRecipeOffset+17]==2);
    assert(restored->sequenceLive.tracks[0].direction==t->sequenceLive.tracks[0].direction&&restored->sequenceLive.tracks[0].directionSeed==t->sequenceLive.tracks[0].directionSeed);
    restored->startClipPlaybackNow();restored->clipPositionMs=change1;restored->serviceSequenceRecipePlayback();
    assert(restored->sequenceParameterValue("seq_recipe_hits")==6);
    // Malformed direction data cannot partially replace the current project.
    const auto path=std::string(directory)+"/thru-clips.bin";auto* file=std::fopen(path.c_str(),"rb");assert(file);
    std::fseek(file,0,SEEK_END);const auto size=std::ftell(file);std::rewind(file);std::vector<unsigned char> bytes(std::size_t(size)-4);
    assert(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);
    const auto offset=clipLegacyTailSize(bytes)-2*(4+64*4*4)-(4+64*4*8)+4;bytes[offset]=5;
    std::uint32_t checksum=2166136261u;for(auto v:bytes){checksum^=v;checksum*=16777619u;}
    file=std::fopen(path.c_str(),"wb");assert(file);assert(std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size());assert(writeU32(file,checksum));std::fclose(file);
    const auto retained=restored->sequenceLive.tracks[0].generator.values;
    assert(!restored->readClipFileFromPath(path.c_str())&&restored->sequenceLive.tracks[0].generator.values==retained);
    restored.reset();std::filesystem::remove_all(directory);
    std::puts("Sequence recipe recording: live HITS/DIR edits, timed discrete replay, linked duration bounds, setup-action guard, UI values, loop seed, live override and no per-block rebuild: PASS");
}
void testAllRecipeGestures(){
    auto t=sequenceFixture();t->selectSplitPart(1);setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_duration_mode","1");t->selectSplitPart(0);
    t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();
    const float values[]{0,5,5,3,2,1,.7f,1979,8,5,.1f,.9f,4,12,2,.8f,3,50,200,.05f,.8f};
    for(int field=1;field<21;++field){
        renderSplit(*t,4);const auto* parameter=s3g_thru_midi::find(3888+field);assert(parameter);
        midiWrite(*t,*parameter,1,values[field]);
        assert(t->clipRecording&&t->splitFocus==0&&t->sequenceLive.tracks[1].generator.values[field]==values[field]);
    }
    renderSplit(*t,12);t->finishClipRecording();t->setClipPlaying(false);
    const auto* clip=t->activeClipSlot();
    for(int field=1;field<21;++field){const auto curve=kClipRecipeOffset+20+field-1;assert(clip->clipPointCounts[curve]>=1);
        const auto last=clip->clipPointStarts[curve]+clip->clipPointCounts[curve]-1;assert(clip->clipPoints[last].value==values[field]);}
    t->startClipPlaybackNow();t->clipPositionMs=clip->clipDurationMs-1;t->serviceSequenceRecipePlayback();
    for(int field=1;field<21;++field)assert(t->sequencePlayback.tracks[1].generator.values[field]==values[field]);
    assert(t->sequenceLive.tracks[0].generator.type==0&&t->sequenceLive.tracks[1].generator.type==1);
    const auto builds=t->sequenceRecipeBuilds;for(int i=0;i<1000;++i)t->serviceSequenceRecipePlayback();assert(t->sequenceRecipeBuilds==builds);
    // TIME -> STEP captures converted recipe bounds along with the UNIT edge.
    t->setClipPlaying(false);t->selectSplitPart(1);t->beginClipRecording();renderSplit(*t,15);
    setParam(t.get(),"seq_duration_mode","0");const auto bounds=t->sequenceLive.tracks[1].generator.values;
    renderSplit(*t,15);t->finishClipRecording();t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);
    assert(t->sequencePlayback.tracks[1].timeMode);
    t->clipPositionMs=t->activeClipSlot()->clipDurationMs-1;t->serviceSequenceBeforeBlock(1);
    assert(!t->sequencePlayback.tracks[1].timeMode&&t->sequencePlayback.tracks[1].generator.values[17]==bounds[17]&&t->sequencePlayback.tracks[1].generator.values[18]==bounds[18]);
    // Removing an overdub invalidates the cached recipe immediately.
    const auto originalPoints=t->activeClipSlot()->clipPointCount;
    t->clipRecordMode=ClipRecordMode::overdub;t->beginClipRecording();assert(t->clipRecording);
    renderSplit(*t,5);setParam(t.get(),"seq_recipe_hits","2");renderSplit(*t,5);t->finishClipRecording();
    assert(t->activeClipSlot()->takeCount==2);t->serviceSequenceRecipePlayback();
    t->undoLastClipTake();assert(t->activeClipSlot()->takeCount==1&&t->activeClipSlot()->clipPointCount==originalPoints);
    t->clipPositionMs=0;t->serviceSequenceRecipePlayback();assert(t->sequencePlayback.tracks[1].generator.values[1]==5);
    std::puts("All 20 recipe gestures: part-addressed MIDI during recording, linked bounds, discrete replay, UNIT conversion, independent lanes and bounded cached audio work: PASS");
}
