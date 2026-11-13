void testFadeLookaheadBuffer(){
    const auto sample=[](std::size_t f){return std::array<float,2>{.4f*std::sin(f*.037f),.23f*std::cos(f*.019f)};};
    for(float rate:{44100.f,48000.f,96000.f}){
        s3g_daisy::ThruFadeLookahead pipe;pipe.prepare(rate);
        const auto delay=pipe.frames();assert(delay==static_cast<std::size_t>(std::ceil(rate*.012f)));
        for(std::size_t f=0;f<2000;++f)assert(pipe.process(sample(f))==sample(f));
        pipe.setEnabled(true);
        for(std::size_t f=2000;f<5000;++f){const auto out=pipe.process(sample(f));
            if(pipe.ready())assert(out==sample(f-delay));
            for(float v:out)assert(std::isfinite(v)&&std::abs(v)<=.401f);
        }
        assert(pipe.active());std::array<float,3072> pending{};pipe.copyPending(pending.data());
        for(std::size_t f=0;f<delay;++f)for(int ch=0;ch<2;++ch)assert(pending[f*2+ch]==sample(5000-delay+f)[ch]);
        pipe.clear();pipe.seedPending([&](std::size_t f){return std::array<float,2>{pending[f*2],pending[f*2+1]};});
        for(std::size_t f=5000;f<7000;++f)assert(pipe.process(sample(f))==sample(f-delay));
        pipe.setEnabled(false);
        for(std::size_t f=7000;f<8000;++f){const auto out=pipe.process(sample(f));if(pipe.ready())assert(out==sample(f));}
        assert(!pipe.enabled()&&pipe.ready());
    }
    std::puts("Fade lookahead buffer: exact Off passthrough, stereo 12 ms delay, smooth selection, chronological prefix/return seeding at 44.1/48/96 kHz: PASS");
}

void testFadeLookaheadStartupAndReturn(){
    auto t=splitFixture(),reference=splitFixture();
    for(auto* scene:{t.get(),reference.get()}){
        scene->setPerformanceMode(0);scene->currentLaneSourceKinds.fill(LaneSourceKind::capture);
        setParam(scene,"pattern_fade_lookahead","1");scene->padNoteOn(68,100);renderSplit(*scene,30);
        assert(scene->fadeLookahead.active());
    }
    t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    assert(t->prepareClipBank(1));t->clipPrepareState.store(int(ClipPrepareState::ready));
    t->patternFadeMs=200;t->patternHeld=true;t->patternWorkerTestBlocked.store(true);
    assert(t->activatePreparedClip()&&t->patternLookaheadFrames==t->fadeLookahead.frames());
    const auto delay=t->patternLookaheadFrames;
    assert(t->patternWarmupFrames==delay&&t->patternWarmupOffset==0);
    assert(t->patternShadow->renderClockSamples.load()==t->renderClockSamples.load());
    const int chunks[]{37,91,17,5,128};std::size_t done=0;int index=0;
    std::array<float,kBlockFrames*2> output{},expected{};
    while(done<delay){
        const int count=static_cast<int>(std::min<std::size_t>(chunks[index++%5],delay-done));
        reference->render(nullptr,count,expected.data());t->render(nullptr,count,output.data());
        for(int f=0;f<count*2;++f)assert(std::abs(output[f]-expected[f])<1e-7f);
        done+=count;assert(t->patternRead.load()==0&&t->patternUnderflows==0);
        assert(t->patternPosition==0&&t->patternWarmup==(done<delay));
    }
    // The real producer has been blocked for the ENTIRE prefix; this is actual
    // source audio, with no main-thread source render or shortage concealment.
    assert(t->patternWrite.load()==0);
    t->patternWorkerTestBlocked.store(false,std::memory_order_release);t->patternSignal.notify();
    for(int block=0;block<20;++block){
        waitForPatternAudio(*t);reference->render(nullptr,kBlockFrames,expected.data());t->render(nullptr,kBlockFrames,output.data());
        for(std::size_t f=0;f<output.size();++f)assert(std::abs(output[f]-expected[f])<1e-6f);
    }
    assert(t->patternUnderflows==0);
    t->returnPatternSource();
    for(int block=0;block<300&&t->patternActive;++block){
        const int count=chunks[block%5];waitForPatternAudio(*t);
        reference->render(nullptr,count,expected.data());t->render(nullptr,count,output.data());
        for(int f=0;f<count*2;++f)assert(std::abs(output[f]-expected[f])<1e-6f);
        const timespec wait{0,100000};nanosleep(&wait,nullptr);
    }
    assert(!t->patternActive&&t->activeClip==0&&t->patternUnderflows==0);
    for(int block=0;block<20;++block){
        reference->render(nullptr,kBlockFrames,expected.data());t->render(nullptr,kBlockFrames,output.data());
        for(std::size_t f=0;f<output.size();++f)assert(std::abs(output[f]-expected[f])<1e-6f);
    }
    waitForPatternIdle(*t);
    // A real shortage after exhausting the prefix is still recorded precisely.
    assert(t->prepareClipBank(1));t->clipPrepareState.store(int(ClipPrepareState::ready));
    t->patternWorkerTestBlocked.store(true);t->patternHeld=true;
    assert(t->activatePreparedClip());done=0;
    while(done<delay){const int count=static_cast<int>(std::min<std::size_t>(kBlockFrames,delay-done));
        t->render(nullptr,count,output.data());done+=count;}
    assert(t->patternUnderflows==0);t->render(nullptr,17,output.data());
    assert(t->patternStartUnderflows==1&&t->patternMissingFrames==17);
    t->patternWorkerTestBlocked.store(false);t->patternSignal.notify();t->finishPatternFade();
    for(int n=0;n<160&&t->patternActive;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,output.data());}
    waitForPatternIdle(*t);
    std::puts("Fade lookahead startup/return: real worker withheld for full prefix, exact stereo through 37/91/17/5/128-frame chunks and return, no callback wait/source render, genuine later shortage retained: PASS");
}

void testFadeLookaheadShortFadeAndState(){
    auto t=std::make_unique<ThruInstance>();t->sampleRate=44100;
    t->fadeLookahead.prepare(t->sampleRate);t->fadeLookahead.setEnabled(true);
    for(int f=0;f<1000;++f)t->fadeLookahead.process({.4f,.2f});
    t->patternActive=true;t->patternWarmup=true;t->patternLookaheadFrames=t->fadeLookahead.frames();
    t->patternWarmupFrames=t->patternLookaheadFrames;t->patternWarmupOffset=0;
    t->fadeLookahead.copyPending(t->patternWarmupAudio.data());t->fadeLookahead.clear();
    t->patternDurationMs=1;
    float minimum=1;
    for(int block=0;block<80;++block){
        const auto read=t->patternRead.load();
        for(std::size_t f=0;f<kBlockFrames;++f){const auto at=((read+f)&(ThruInstance::kPatternRingFrames-1))*2;
            t->patternRing[at]=.4f;t->patternRing[at+1]=.2f;
            const auto delayed=t->fadeLookahead.process({.4f,.2f});t->voiceMix[0][f]=delayed[0];t->voiceMix[1][f]=delayed[1];}
        t->patternWrite.store(read+kBlockFrames);t->mixPatternFade(kBlockFrames);
        for(std::size_t f=0;f<kBlockFrames;++f)minimum=std::min(minimum,t->voiceMix[0][f]);
    }
    assert(!t->patternActive&&minimum>.39999f&&t->patternUnderflows==0);
    t->patternJob.store(0); // No worker is owned by this pure mixer fixture.
    auto state=splitFixture();setParam(state.get(),"pattern_fade_lookahead","1");
    std::array<char,kStateTextSize> json{};assert(state->serializeState(json.data(),json.size())>0);
    ThruInstance::PersistentStateSnapshot frozen;frozen.capture(*state);
    setParam(state.get(),"pattern_fade_lookahead","0");
    std::array<char,kStateTextSize> saved{};assert(frozen.serializeState(saved.data(),saved.size())>0);
    assert(!std::strcmp(json.data(),saved.data()));assert(state->restoreState(json.data())&&state->patternLookahead==1);
    std::string legacy=json.data();const auto at=legacy.find(",\"pattern_fade_lookahead\":");assert(at!=std::string::npos);
    legacy.erase(at,legacy.find_first_of(",}",at+1)-at);
    assert(state->restoreState(legacy.c_str())&&state->patternLookahead==1);
    setParam(state.get(),"pattern_fade_lookahead","0");
    state->patternActive=true;setParam(state.get(),"pattern_fade_lookahead","1");assert(state->patternLookahead==0);
    state->patternActive=false;
    std::puts("Fade lookahead short fades/state: 1 ms fade has no zero-filled gap; frozen/persistent recall, old-state 12ms default and active-fade setting guard: PASS");
}

void testFadeLookaheadMonitor(){
    const auto* previous=g_host;
    std::array<std::int16_t,kBlockFrames*2> input{};
    for(std::size_t f=0;f<kBlockFrames;++f){input[f*2]=3000;input[f*2+1]=-1900;}
    host_api_v1_t host{};host.sample_rate=44100;host.frames_per_block=kBlockFrames;
    host.mapped_memory=reinterpret_cast<std::uint8_t*>(input.data());g_host=&host;
    {
        auto off=std::make_unique<ThruInstance>(),on=std::make_unique<ThruInstance>();
        assert(off->prepare(nullptr)&&on->prepare(nullptr));
        setParam(off.get(),"pattern_fade_lookahead","0");
        off->monitorLevel=off->monitorGainState=on->monitorLevel=on->monitorGainState=1;
        setParam(on.get(),"pattern_fade_lookahead","1");
        for(int f=0;f<1000;++f)on->fadeLookahead.process({0,0});
        std::array<std::int16_t,kBlockFrames*2> a{},b{};bool audible=false;
        for(int n=0;n<8;++n){off->render(a.data(),kBlockFrames);on->render(b.data(),kBlockFrames);
            assert(a==b);for(auto v:a)audible|=std::abs(static_cast<int>(v))>100;}
        assert(audible);
    }
    g_host=previous;
    std::puts("Fade lookahead input monitor: native mapped stereo input/limiter/dither output identical to Off, with no added monitoring delay: PASS");
}

void testFadeLookaheadEarlyReturnAndModes(){
    auto t=splitFixture();t->setPerformanceMode(0);t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
    for(std::size_t lane=0;lane<4;++lane)for(std::size_t ch=0;ch<2;++ch)
        std::fill_n(t->engine.writableLane(lane,ch),8192,ch?.11f:.2f);
    setParam(t.get(),"pattern_fade_lookahead","1");t->padNoteOn(68,100);renderSplit(*t,30);
    t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    assert(t->prepareClipBank(1));t->clipPrepareState.store(int(ClipPrepareState::ready));
    t->patternFadeMs=200;t->patternHeld=true;t->patternWorkerTestBlocked.store(true);
    assert(t->activatePreparedClip());t->returnPatternSource();
    std::array<float,kBlockFrames*2> output{};
    const auto prefix=t->patternLookaheadFrames;std::size_t done=0;
    while(done<prefix){const int count=static_cast<int>(std::min<std::size_t>(kBlockFrames,prefix-done));
        t->render(nullptr,count,output.data());done+=count;}
    assert(t->patternJob.load()==3&&t->patternWrite.load()==0&&t->patternUnderflows==0);
    const float last=output[0];assert(last>.05f);
    t->patternWorkerTestBlocked.store(false);t->patternSignal.notify();
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(t->patternJob.load(std::memory_order_acquire)!=4&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
    assert(t->patternJob.load()==4&&t->patternWrite.load()-t->patternRead.load()>=prefix+2*kBlockFrames);
    for(int n=0;n<150&&t->patternActive;++n){t->render(nullptr,37,output.data());
        for(int f=0;f<37;++f)assert(std::abs(output[f*2]-last)<1e-5f);}
    assert(!t->patternActive&&t->activeClip==0&&t->patternUnderflows==0);waitForPatternIdle(*t);
    t.reset();
    for(int mode=0;mode<=6;++mode){
        auto scene=splitFixture(false);scene->setPerformanceMode(mode);scene->currentLaneSourceKinds.fill(LaneSourceKind::capture);
        assert(scene->patternLookahead==1&&scene->fadeLookahead.active());
        if(mode==6){scene->sequenceLive.tracks[0].trigs[0].enabled=true;scene->commitSequenceEdit();scene->startClipPlaybackNow();}
        else scene->padNoteOn(68,100);
        renderSplit(*scene,30);scene->captureActiveClip(true);scene->copyCurrentSourcesToClip(scene->clipSlots[0]);scene->clipSlots[1]=scene->clipSlots[0];
        assert(scene->prepareClipBank(1));scene->clipPrepareState.store(int(ClipPrepareState::ready));scene->patternFadeMs=25;
        assert(scene->activatePreparedClip());
        for(int n=0;n<250&&scene->patternActive;++n){
            // Native calls are not hardware paced. Before a call can consume
            // ring audio, let the real producer publish one block even when
            // only the final partial prefix is left.
            if(!scene->patternWarmup||scene->patternWarmupFrames-scene->patternWarmupOffset<kBlockFrames){
                const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(2);
                while(scene->patternWrite.load(std::memory_order_acquire)-scene->patternRead.load()<kBlockFrames
                    &&std::chrono::steady_clock::now()<limit)std::this_thread::yield();
                assert(scene->patternWrite.load()-scene->patternRead.load()>=kBlockFrames);
            }
            scene->render(nullptr,kBlockFrames,output.data());for(float value:output)assert(std::isfinite(value));
        }
        assert(!scene->patternActive&&scene->patternUnderflows==0&&scene->performanceMode()==mode);
        waitForPatternIdle(*scene);
    }
    std::puts("Fade lookahead early return/layouts: producer builds full return reserve, exact pending-audio handoff and timed fades across all seven modes including Layers/Sequence: PASS");
}

void testFadeLookaheadDefaultAndSavedChoice(){
    auto fresh=std::make_unique<ThruInstance>();assert(fresh->prepare(nullptr));
    assert(fresh->patternLookahead==1&&fresh->fadeLookahead.active()&&fresh->fadeLookahead.ready());
    char choice[8]{};assert(getParam(fresh.get(),"pattern_fade_lookahead",choice,sizeof(choice))>0&&!std::strcmp(choice,"1"));
    // The default is a complete delay from the first sample, rather than a
    // startup ramp that lets an early note leak out before the reserve exists.
    const auto frames=fresh->fadeLookahead.frames();
    for(std::size_t f=0;f<=frames;++f){
        const auto output=fresh->fadeLookahead.process(f==0?std::array<float,2>{.7f,-.3f}:std::array<float,2>{0,0});
        assert(output[0]==(f==frames?.7f:0.f)&&output[1]==(f==frames?-.3f:0.f));
    }
    setParam(fresh.get(),"pattern_fade_lookahead","0");
    assert(fresh->patternLookahead==0&&!fresh->fadeLookahead.enabled()&&fresh->fadeLookahead.ready());
    assert((fresh->fadeLookahead.process({.2f,-.4f})==std::array<float,2>{.2f,-.4f}));
    std::array<char,kStateTextSize> saved{};assert(fresh->serializeState(saved.data(),saved.size())>0);
    auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(nullptr));
    assert(restored->restoreState(saved.data()));
    assert(restored->patternLookahead==0&&!restored->fadeLookahead.enabled()&&restored->fadeLookahead.ready());
    assert((restored->fadeLookahead.process({.3f,-.1f})==std::array<float,2>{.3f,-.1f}));
    std::string legacy=saved.data();const auto at=legacy.find(",\"pattern_fade_lookahead\":");assert(at!=std::string::npos);
    legacy.erase(at,legacy.find_first_of(",}",at+1)-at);
    assert(restored->restoreState(legacy.c_str()));
    assert(restored->patternLookahead==1&&restored->fadeLookahead.active());
    restored->patternFadeMs=100;
    assert(restored->beginPatternFade(1));
    assert(restored->patternWarmup&&restored->patternLookaheadFrames>0&&restored->patternJob.load()==1);
    std::puts("Fade lookahead default: new/unspecified projects use 12ms immediately, saved Off retains exact first-sample passthrough, legacy recall enables the reserve and direct handoffs start their worker: PASS");
}

void testFadeLookahead(){testFadeLookaheadDefaultAndSavedChoice();testFadeLookaheadBuffer();testFadeLookaheadStartupAndReturn();testFadeLookaheadShortFadeAndState();testFadeLookaheadMonitor();testFadeLookaheadEarlyReturnAndModes();}
