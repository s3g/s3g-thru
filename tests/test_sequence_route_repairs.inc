void testCommittedSequenceGeometry(){
    using namespace s3g_thru_sequence;
    for(int edit=0;edit<3;++edit){
        Pattern pattern;pattern.active=true;auto& lane=pattern.tracks[0];lane.length=16;
        lane.trigs[8].enabled=true;setCount(lane.trigs[8],2);lane.trigs[8].events[1].when=.8f;
        for(int p=1;p<4;++p)pattern.tracks[p].enabled=false;
        std::array<DirectionTraversal,4> routes;for(int p=0;p<4;++p)routes[p].reset(pattern.tracks[p]);
        EventList first,pending;schedule(pattern,2.,.001,100,first,{},&routes);assert(first.size==1);
        if(edit==0)lane.rotation=1;else if(edit==1)lane.length=8;else {lane.length=7;lane.rotation=3;lane.path=1;lane.direction=2;}
        routes[0].synchronize(lane,8,8);assert(routes[0].at(lane,8).step==8);
        schedule(pattern,2.1,.001,140,pending,{},&routes);
        assert(pending.size==1&&pending.items[0].step==8&&pending.items[0].child==1&&pending.items[0].tick==8);
        assert(routes[0].at(lane,9).step<lane.length);
        if(edit==0)assert(routes[0].at(lane,9).step==10); // ROT really takes effect.
        if(edit==1)assert(routes[0].at(lane,9).step==1); // LEN keeps clock phase.
    }
    // Repeated edits cannot rewrite an early event which has already sounded.
    Lane lane;lane.length=17;DirectionTraversal route;route.reset(lane);
    for(int tick=0;tick<1000;++tick){
        const auto committed=route.at(lane,tick+1);
        lane.length=1+tick%32;lane.rotation=tick%32;lane.direction=tick%5;lane.path=tick%3;
        route.synchronize(lane,tick,tick+1);
        const auto held=route.at(lane,tick+1);assert(held.step==committed.step&&held.cycle==committed.cycle);
        assert(route.at(lane,tick+2).step<lane.length);
    }
    auto t=sequenceFixture();auto& l=t->sequenceLive.tracks[0];l.trigs[8].enabled=true;setCount(l.trigs[8],2);l.trigs[8].events[1].when=.8f;
    for(int p=1;p<4;++p)t->sequenceLive.tracks[p].enabled=false;t->commitSequenceEdit();
    for(int p=0;p<4;++p)t->sequenceDirections[p].reset(t->sequencePlayback.tracks[p]);
    setParam(t.get(),"seq_rotation","1");t->sequenceDirections[0].synchronize(t->sequencePlayback.tracks[0],8,8);
    EventList late;schedule(t->sequencePlayback,2.1,.001,140,late,{},&t->sequenceDirections);
    assert(late.size==1&&late.items[0].step==8&&late.items[0].child==1);
    // A generator may remove the old cell altogether on a length edit.
    for(const char* type:{"1","2"}){
        auto generated=sequenceFixture();setParam(generated.get(),"seq_type",type);
        setParam(generated.get(),"seq_recipe_map","5");setParam(generated.get(),"seq_recipe_regions","4");
        setParam(generated.get(),"seq_step","9");setParam(generated.get(),"seq_burst","2");
        setParam(generated.get(),"seq_child","2");setParam(generated.get(),"seq_when",".8");setParam(generated.get(),"seq_pos",".7");
        generated->startClipPlaybackNow();generated->sequenceBeat=2.;generated->serviceSequenceBeforeBlock(1);
        const auto sounded=generated->sequenceEventCount[0];assert(sounded==1);
        setParam(generated.get(),"seq_length","8");generated->sequenceBeat=2.1;generated->serviceSequenceBeforeBlock(1);
        assert(generated->sequenceLaneCursor(0,generated->sequencePlayback.tracks[0])==-1);
        EventList pending;schedule(generated->sequencePlayback,2.1,.001,140,pending,{},&generated->sequenceDirections);
        assert(pending.size==1&&pending.items[0].step==8&&pending.items[0].child==1);
        generated->sequenceEvents=pending;generated->sequenceEvents.items[0].offset=0;generated->serviceSequenceEvents(1);
        assert(generated->sequenceEventCount[0]==sounded+1);
        const PlaybackVoice* voice=nullptr;for(const auto& v:generated->voices)if(v.active&&(!voice||v.age>voice->age))voice=&v;
        assert(voice&&voice->sequence.child==1&&std::abs(voice->sequence.position-.7f)<1e-6);
    }
    // A normal WHEN edit still changes a pending child immediately.
    lane={};lane.trigs[0].enabled=true;setCount(lane.trigs[0],2);route.reset(lane);route.remember(lane,0,0);
    lane.trigs[0].events[1].when=.7f;route.synchronize(lane,0,0);assert(route.step(lane,0,0).events[1].when==.7f);
    std::puts("Sequence committed geometry: real controls, generated/Normal late Burst, shortened LEN, frozen source locks, combined edits and early-MICRO commitment: PASS");
}

void testSequenceGapRoutes(){
    using namespace s3g_thru_sequence;
    using namespace s3g_thru_generate;
    // Independent oracle: schedule real events in short blocks and compare
    // their sounding intervals, including unordered Burst and overlapping MICRO.
    for(int path=0;path<3;++path)for(int direction=0;direction<5;++direction){
        Pattern pattern;pattern.active=true;auto& lane=pattern.tracks[0];
        lane.length=13;lane.path=path;lane.direction=direction;lane.rotation=3;lane.directionSeed=1979;
        for(int p=1;p<4;++p)pattern.tracks[p].enabled=false;
        for(int row:{0,3,8,12}){auto& step=lane.trigs[row];step.enabled=true;step.micro=row%2?.41f:-.37f;
            setCount(step,3);step.events[0].when=.23f;step.events[1].when=.91f;step.events[2].when=0;}
        struct Heard {int frame;Event event;};std::vector<Heard> heard;EventList events;
        constexpr int resolution=1024,total=80*resolution;const double increment=rates[4]/resolution;
        for(int frame=0;frame<total;){const int count=std::min(137,total-frame);
            schedule(pattern,frame*increment,increment,count,events);assert(!events.overflow);
            for(int i=0;i<events.size;++i)heard.push_back({frame+events.items[i].offset,events.items[i]});frame+=count;}
        assert(heard.size()>20);
        for(std::size_t i=0;i+1<heard.size();++i){const auto& current=heard[i];
            const double actual=double(heard[i+1].frame-current.frame)/resolution;
            const double predicted=nextHitGap(lane,current.event.step,current.event.child,current.event.tick);
            if((i&&heard[i-1].frame==current.frame)||heard[i+1].frame==current.frame)continue; // Coincident children are bounded by MIN.
            assert(std::abs(actual-predicted)<2.0/resolution);
        }
    }
    Lane base;base.length=16;base.path=1;Config config;Context context;
    config.v[hits]=4;config.v[duration]=2;config.v[minimum]=.01f;config.v[maximum]=32;config.v[writePolicy]=1;
    auto result=build(base,config,context);
    assert(std::abs(resolve(result.lane,result.lane.trigs[0],0,3,0)-1)<1e-6);
    assert(std::abs(resolve(result.lane,result.lane.trigs[8],0,3,0)-7)<1e-6);
    base.path=2;result=build(base,config,context);assert(resolve(result.lane,result.lane.trigs[0],0,3,0)==2);
    // Bounds, protected manual durations, coincident retriggers and silent patterns.
    base.path=1;base.trigs[0].locks.mask=8;base.trigs[0].locks.values[3]=.375f;config.v[writePolicy]=0;
    result=build(base,config,context);assert(result.lane.trigs[0].locks.values[3]==.375f);
    Lane simultaneous;simultaneous.length=1;simultaneous.trigs[0].enabled=true;setCount(simultaneous.trigs[0],2);simultaneous.trigs[0].events[1].when=0;
    assert(nextHitGap(simultaneous,0,0,0)==0);simultaneous.trigs[0].enabled=false;
    assert(std::isinf(nextHitGap(simultaneous,0,0,0)));
    std::puts("Sequence GAP core: all PATH/DIR against scheduled events, independent Burst timings/MICRO, VERT/DIAG refits, bounds, protected locks and finite search: PASS");
}

void testSequenceGapRuntime(){
    using namespace s3g_thru_sequence;
    const auto configure=[](){auto t=sequenceFixture();t->tempoBpm=120;
        setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_recipe_hits","4");
        setParam(t.get(),"seq_recipe_write","1");setParam(t.get(),"seq_recipe_max","32");
        setParam(t.get(),"seq_recipe_min",".01");setParam(t.get(),"seq_recipe_duration","2");setParam(t.get(),"seq_path","1");return t;};
    auto t=configure();auto& lane=t->sequencePlayback.tracks[0];
    assert(lane.trigs[0].locks.values[3]==1&&lane.trigs[8].locks.values[3]==7);
    const auto start=[&](int row,int child,std::int64_t tick,double expectedMs){
        t->startSequenceVoice(0,row,child,rates[lane.rate]*resolve(lane,lane.trigs[row],child,3,lane.gate),false,tick);
        const PlaybackVoice* newest=nullptr;for(const auto& voice:t->voices)if(voice.active&&voice.sequence.active&&(!newest||voice.age>newest->age))newest=&voice;
        if(!newest||std::abs(double(newest->sequence.frames)-expectedMs*t->sampleRate/1000)>=1.01)
            std::fprintf(stderr,"GAP voice row=%d child=%d tick=%lld expected=%.3fms actual=%.3fms unit=%d live=%d bounds=%.3f/%.3f cliprate=%.2f\n",row,child,(long long)tick,expectedMs,newest?double(newest->sequence.frames)*1000/t->sampleRate:-1.,int(lane.timeMode),int(s3g_thru_generate::usesLiveGap(lane,row,child)),lane.generator.values[17],lane.generator.values[18],t->activeClipSlot()->clipRate);
        assert(newest&&std::abs(double(newest->sequence.frames)-expectedMs*t->sampleRate/1000)<1.01);
        assert(newest->sequence.releaseAt<=newest->sequence.frames);
    };
    t->sequenceDirections[0].reset(lane);start(0,0,0,125);start(8,0,1,875);
    setParam(t.get(),"seq_step","2");setParam(t.get(),"seq_toggle","1");
    t->sequenceDirections[0].reset(lane);start(8,0,1,125);start(1,0,2,750);
    setParam(t.get(),"seq_toggle","1");
    // One pad can need a different GAP on the outbound and return visits.
    setParam(t.get(),"seq_direction","2");setParam(t.get(),"seq_path","0");
    setParam(t.get(),"seq_recipe_hits","3");
    t->sequenceDirections[0].reset(lane);start(6,0,6,5*125);start(6,0,24,6*125);
    // A manual lock with KEEP owns its duration, including during live playback.
    setParam(t.get(),"seq_recipe_write","0");setParam(t.get(),"seq_step","1");setParam(t.get(),"seq_trig_gate",".375");
    t->sequenceDirections[0].reset(lane);start(0,0,0,.375*125);
    // TIME GAP follows tempo/rate while staying within its absolute bounds.
    setParam(t.get(),"seq_recipe_hits","4");
    setParam(t.get(),"seq_duration_mode","1");setParam(t.get(),"seq_recipe_write","1");
    setParam(t.get(),"seq_recipe_min","10");setParam(t.get(),"seq_recipe_max","30000");
    setParam(t.get(),"seq_direction","0");setParam(t.get(),"seq_path","1");
    t->sequenceDirections[0].reset(lane);start(0,0,0,125);
    t->tempoBpm=60;start(0,0,0,250);
    // STEP keeps the original long-duration range at very slow tempos.
    setParam(t.get(),"seq_duration_mode","0");setParam(t.get(),"seq_recipe_max","32");
    setParam(t.get(),"seq_recipe_min",".01");t->tempoBpm=1;t->sequenceDirections[0].reset(lane);start(0,0,0,15000);
    setParam(t.get(),"seq_recipe_hits","1");t->sequenceDirections[0].reset(lane);start(0,0,0,16*15000);
    // Route automation refits once at its edge, then stays cached per block.
    auto automation=configure();automation->startClipPlaybackNow();renderSplit(*automation,1);
    const auto count=automation->sequenceRecipeBuilds[0];renderSplit(*automation,4);assert(automation->sequenceRecipeBuilds[0]==count);
    automation->sequenceLive.tracks[0].path=2;automation->serviceSequenceBeforeBlock(1);
    assert(automation->sequencePlayback.tracks[0].trigs[0].locks.values[3]==2);
    const auto updated=automation->sequenceRecipeBuilds[0];automation->serviceSequenceBeforeBlock(1);assert(automation->sequenceRecipeBuilds[0]==updated);
    std::puts("Sequence GAP runtime: actual voice durations in STEP/TIME, bounce revisits, manual ownership, tempo/bounds, long durations, route edges and cached render work: PASS");
}

void testSequenceResolvedSource(){
    auto t=sequenceFixture();setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_recipe_map","1");
    auto& edited=t->activeClipSlot()->sliceLanes[0].edited;auto& file=t->fileSliceMaps[0];auto& transient=t->activeRegions[0];
    edited={};file={};transient={};std::vector<std::string> snapshots;
    const auto check=[&](int expected){char text[32768];const int n=t->sequenceState(text,sizeof(text));assert(n>0);
        const auto match="\"source_kind\":"+std::to_string(expected);assert(std::strstr(text,match.c_str()));snapshots.emplace_back(text);};
    check(0); // AUTO equal fallback.
    transient.count=2;transient.onsets[0]=0;transient.onsets[1]=.5f;transient.onsets[2]=1;check(1);
    file.count=2;file.points[0]=0;file.points[1]=.4f;file.points[2]=1;check(2);
    edited.count=3;edited.points={{0,.2f,.7f,1}};check(3);
    setParam(t.get(),"seq_recipe_map","5");check(0); // EQUAL is explicit.
    setParam(t.get(),"seq_recipe_map","0");check(-1);
    t->clipPlaying=true;t->sequencePlayback=t->sequenceLive;t->sequencePlayback.tracks[0].generator.values[2]=1;check(3);
    t->clipPlaying=false;
    if(const char* path=std::getenv("S3G_THRU_SOURCE_KIND_FIXTURE")){auto* f=std::fopen(path,"wb");assert(f);std::fputs("[",f);
        for(std::size_t i=0;i<snapshots.size();++i){if(i)std::fputs(",",f);std::fputs(snapshots[i].c_str(),f);}std::fputs("]",f);std::fclose(f);}
    std::puts("Sequence resolved source: AUTO equal/transient/file/edited precedence, explicit maps, KEEP and current playback snapshot: PASS");
}

void testSequenceRouteRepairs(){testCommittedSequenceGeometry();testSequenceGapRoutes();testSequenceGapRuntime();testSequenceResolvedSource();}
