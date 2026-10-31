void waitForPatternAudio(ThruInstance& t)
{
    for(int n=0;n<2000 && t.patternActive && !t.patternWarmup
        && t.patternWrite.load(std::memory_order_acquire)-t.patternRead.load(std::memory_order_relaxed)<kBlockFrames;++n) {
        const timespec delay{0,1000000};nanosleep(&delay,nullptr);
    }
    if(t.patternActive && !t.patternWarmup && t.patternWrite.load()-t.patternRead.load()<kBlockFrames)
        std::fprintf(stderr,"WAIT FAIL active=%d job=%d running=%d read=%llu write=%llu pos=%f mode=%d shadowClock=%llu\n",t.patternActive,t.patternJob.load(),t.patternRunning.load(),(unsigned long long)t.patternRead.load(),(unsigned long long)t.patternWrite.load(),t.patternPosition,t.performanceMode(),(unsigned long long)t.patternShadow->renderClockSamples.load());
    assert(!t.patternActive || t.patternWarmup || t.patternWrite.load(std::memory_order_acquire)-t.patternRead.load(std::memory_order_relaxed)>=kBlockFrames);
}
void waitForPatternIdle(ThruInstance& t)
{
    for(int n=0;n<2000&&t.patternJob.load(std::memory_order_acquire)!=0;++n) {
        const timespec delay{0,1000000};nanosleep(&delay,nullptr);
    }
    assert(!t.patternBusy());
}
void testPatternFadeCurvesAndRecall()
{
    for(int curve=0;curve<5;++curve) {
        float last=-1;
        for(int i=0;i<=1000;++i) {
            const auto gains=ThruInstance::patternGains(i/1000.0f,curve);
            assert(gains[1]>=last&&gains[0]>=0&&gains[1]<=1);last=gains[1];
            const float sum=curve==4?gains[0]*gains[0]+gains[1]*gains[1]:gains[0]+gains[1];
            assert(std::abs(sum-1)<1e-6f);
        }
        assert(ThruInstance::patternGains(0,curve)[0]==1&&ThruInstance::patternGains(1,curve)[1]==1);
    }
    auto t=splitFixture();setParam(t.get(),"pattern_fade_ms","30000");setParam(t.get(),"pattern_fade_curve","4");
    std::array<char,kStateTextSize> json{};assert(t->serializeState(json.data(),json.size())>0);
    auto restored=splitFixture();assert(restored->restoreState(json.data()));
    assert(restored->patternFadeMs==30000&&restored->patternCurve==4&&!restored->patternActive);
    char text[256]{};assert(getParamValue(t.get(),"pattern_fade_ms",text,sizeof(text))>0&&std::strcmp(text,"30000")==0);
    assert(getParamValue(t.get(),"pattern_fade_state",text,sizeof(text))>0);
    setParam(t.get(),"pattern_fade_ms","60000");assert(t->patternFadeMs==30000);
    setParam(t.get(),"pattern_fade_ms","-100");assert(t->patternFadeMs==0);
}
void testPatternFadePlaybackAndHeldMidpoint()
{
    for(int mode=0;mode<4;++mode) {
        auto t=splitFixture();t->setPerformanceMode(mode);
        t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
        t->controls.algorithm=Engine::PlaybackAlgorithm::sample;t->controls.sampleMode=Engine::SampleMode::loop;
        t->beginClipRecording();t->padNoteOn(68,100);renderSplit(*t,30);t->padNoteOff(68);renderSplit(*t,60);t->finishClipRecording();
        t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
        assert(t->prepareClipBank(1));
        for(std::size_t lane=0;lane<kLaneCount;++lane)for(std::size_t ch=0;ch<2;++ch)
            std::fill_n(t->preparedClipStorage.get()+t->planeCapacityFrames*lane+t->laneCapacityFrames*ch,t->preparedLaneFrames[lane],-.3f);
        const float* old=t->engine.laneData(0,0);t->patternFadeMs=200;
        t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready),std::memory_order_release);
        assert(t->activatePreparedClip()&&t->patternActive&&t->patternWarmup&&t->activeClip==1);
        assert(t->patternShadow->engine.laneData(0,0)==old&&t->engine.laneData(0,0)!=old);
        for(const auto& voice:t->voices)assert(!voice.active);
        assert(t->polyphonyGain==1);
        assert(!t->requestClipPreparation(2)); // Outgoing samples cannot be reused by the loader.
        std::array<float,kBlockFrames*2> audio{};t->render(nullptr,kBlockFrames,audio.data());
        for(int n=0;n<20;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
        assert(t->patternPosition>0&&t->patternPosition<1);
        t->setPatternPosition(.5f);
        for(int n=0;n<50;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
        assert(std::abs(t->patternPosition-.5f)<1e-6f&&t->patternHeld);
        // Both data loops continue even when the interpolation is held.
        const auto oldClock=t->patternShadow->renderClockSamples.load(std::memory_order_relaxed);
        const auto newClock=t->clipAbsolutePositionMs;
        for(int n=0;n<150;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
        assert(t->patternPosition==.5f&&t->clipAbsolutePositionMs>newClock);
        assert(t->patternShadow->renderClockSamples.load(std::memory_order_relaxed)>oldClock);
        assert(t->patternUnderflows==0);
        t->setPatternHeld(false);
        for(int n=0;n<200&&t->patternActive;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
        assert(!t->patternActive&&t->activeClip==1);waitForPatternIdle(*t);
        // A second overlap can reuse the scene and its cleared histories safely.
        assert(t->prepareClipBank(0));t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
        assert(t->activatePreparedClip()&&t->patternActive);t->render(nullptr,kBlockFrames,audio.data());
        t->finishPatternFade();
        for(int n=0;n<150&&t->patternActive;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
        assert(!t->patternActive);waitForPatternIdle(*t);
    }
}

void testPatternFadeAudioAndReleases()
{
    auto t=splitFixture();t->setPerformanceMode(0);t->outputGain=1;
    t->controls.ampAttackMs=0;t->controls.ampReleaseMs=10;
    t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
    for(std::size_t lane=0;lane<4;++lane)for(std::size_t ch=0;ch<2;++ch)
        std::fill_n(t->engine.writableLane(lane,ch),8192,.4f);
    t->padNoteOn(68,100);renderSplit(*t,50);t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    assert(t->prepareClipBank(1));
    for(std::size_t lane=0;lane<4;++lane)for(std::size_t ch=0;ch<2;++ch)
        std::fill_n(t->preparedClipStorage.get()+t->planeCapacityFrames*lane+t->laneCapacityFrames*ch,8192,-.4f);
    t->patternFadeMs=1000;t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
    assert(t->activatePreparedClip());t->padNoteOn(68,100);
    std::array<float,kBlockFrames*2> audio{};t->render(nullptr,kBlockFrames,audio.data());
    assert(audio[0]>.05f);float previous=audio.back(),jump=0;
    auto render=[&](int blocks) {
        for(int n=0;n<blocks;++n) {
            waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());
            for(std::size_t f=0;f<kBlockFrames;++f) {
                jump=std::max(jump,std::abs(audio[f*2]-previous));previous=audio[f*2];
                assert(std::isfinite(previous));
            }
        }
    };
    t->setPatternPosition(.5f);render(80);
    assert(std::abs(audio[0])<.025f); // Distinct +A/-B samples cancel at a linear midpoint.
    t->padNoteOff(68);render(80);assert(std::abs(audio[0])<.001f);
    assert(t->patternCommandRead.load(std::memory_order_acquire)==t->patternCommandWrite.load(std::memory_order_relaxed));
    t->padNoteOn(68,100);render(50);assert(audio[0]<-.05f); // A's released note is not re-triggered by a new B note.
    t->finishPatternFade();render(100);assert(!t->patternActive&&audio[0]<-.1f);
    assert(jump<.012f);waitForPatternIdle(*t);
}

void testPatternFadeUnderrunBridge()
{
    auto t=std::make_unique<ThruInstance>();t->patternActive=true;t->patternHeld=true;
    t->patternPosition=t->patternPositionTarget=0;t->patternLast={.4f,.4f};
    float previous=.4f,jump=0;
    for(int block=0;block<4;++block) {
        if(block>=2) {
            const auto at=t->patternWrite.load();
            for(std::size_t f=0;f<kBlockFrames;++f)for(std::size_t ch=0;ch<2;++ch)
                t->patternRing[((at+f)&(ThruInstance::kPatternRingFrames-1u))*2u+ch]=-.4f;
            t->patternWrite.store(at+kBlockFrames,std::memory_order_release);
        }
        t->mixPatternFade(kBlockFrames);
        for(std::size_t f=0;f<kBlockFrames;++f) {
            jump=std::max(jump,std::abs(t->voiceMix[0][f]-previous));previous=t->voiceMix[0][f];
        }
    }
    assert(t->patternUnderflows==2&&jump<.004f&&t->patternPosition==0);
    setParam(t.get(),"performance_reset","1");assert(t->patternUnderflows==0);
}
void testPatternFadePreparedWavesetBanks()
{
    auto t=splitFixture();t->setPerformanceMode(0);t->controls.algorithm=Engine::PlaybackAlgorithm::wavesets;
    t->currentLaneSourceKinds.fill(LaneSourceKind::capture);t->captureActiveClip(true);
    t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    assert(t->prepareClipBank(1));assert(t->preparedWavesetBanks[0]);
    t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
    assert(t->activatePreparedClip());
    const auto* oldBank=t->wavesetWorker.bank(0);assert(oldBank&&oldBank->revision==t->wavesetSourceRevisions[0]);
    t->padNoteOn(68,100);renderSplit(*t,30);
    assert(t->prepareClipBank(0));t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
    t->patternFadeMs=200;assert(t->activatePreparedClip());
    assert(t->patternShadow->wavesetWorker.bank(0)==oldBank);
    assert(t->wavesetWorker.bank(0)&&t->wavesetWorker.bank(0)!=oldBank);
    t->padNoteOn(68,100);std::array<float,kBlockFrames*2u> audio{};
    t->render(nullptr,kBlockFrames,audio.data());float peak=0;
    for(int i=0;i<120&&t->patternActive;++i) {
        waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());
        for(auto value:audio){assert(std::isfinite(value));peak=std::max(peak,std::abs(value));}
    }
    assert(peak>.001f&&!t->patternActive);waitForPatternIdle(*t);
}

void testManualPatternCrossfader()
{
    for(bool useDone:{false,true}) {
        auto t=splitFixture();t->setPerformanceMode(0);
        t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
        t->beginClipRecording();t->padNoteOn(68,100);renderSplit(*t,30);
        t->padNoteOff(68);renderSplit(*t,60);t->finishClipRecording();
        t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
        assert(t->patternFadeMs==0&&!t->wantsPatternFade());
        setParam(t.get(),"pattern_fade_hold","1");assert(t->patternHeld&&t->wantsPatternFade());
        t->finishPatternFade();assert(!t->patternHeld&&!t->wantsPatternFade());
        setParam(t.get(),"pattern_fade_hold","1");
        assert(t->prepareClipBank(1));t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
        assert(t->activatePreparedClip()&&t->patternActive&&t->patternHeld&&t->patternPosition==0);
        std::array<float,kBlockFrames*2u> audio{};t->render(nullptr,kBlockFrames,audio.data());
        auto render=[&](int blocks) {
            for(int i=0;i<blocks;++i){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
        };
        render(40);assert(t->patternPosition==0&&t->patternActive);
        // Relative turns accumulate without requiring a render between messages.
        for(int i=0;i<10;++i)setParam(t.get(),"pattern_fade_nudge","1");
        assert(std::abs(t->patternPositionTarget-.1f)<1e-6f);
        t->setPatternHeld(true);assert(std::abs(t->patternPositionTarget-.1f)<1e-6f);
        setParam(t.get(),"pattern_fade_nudge","-5");assert(std::abs(t->patternPositionTarget-.05f)<1e-6f);
        t->setPatternPosition(1);render(100);
        assert(t->patternPosition==1&&t->patternHeld&&t->patternActive&&t->patternGainA<1e-5f);
        const auto oldClock=t->patternShadow->renderClockSamples.load(std::memory_order_relaxed);
        const auto targetClock=t->clipAbsolutePositionMs;
        setParam(t.get(),"pattern_fade_nudge","-100");render(100);
        assert(t->patternPosition==0&&t->patternActive&&t->patternGainB<1e-5f);
        assert(t->patternShadow->renderClockSamples.load(std::memory_order_relaxed)>oldClock);
        assert(t->clipAbsolutePositionMs>targetClock&&t->patternUnderflows==0);
        if(useDone)t->finishPatternFade();else t->setPatternHeld(false);
        for(int i=0;i<150&&t->patternActive;++i)render(1);
        assert(!t->patternActive&&!t->patternHeld);waitForPatternIdle(*t);
    }
}

// Exercise the actual fade mixer clock at both supported test sample rates.
void testThirtySecondPatternFadeClock()
{
    for(float rate:{44100.0f,48000.0f}) {
        auto t=std::make_unique<ThruInstance>();t->sampleRate=rate;
        t->patternActive=true;t->patternDurationMs=30000;
        std::uint64_t frames=0;
        while(t->patternActive && frames<static_cast<std::uint64_t>(rate*31)) {
            t->patternWrite.store(t->patternRead.load()+kBlockFrames,std::memory_order_release);
            t->mixPatternFade(kBlockFrames);frames+=kBlockFrames;
        }
        const double seconds=frames/static_cast<double>(rate);
        assert(!t->patternActive&&seconds>=30&&seconds<30.1&&t->patternUnderflows==0);
    }
}

void testRandomPatternTimingBoundsAndRecall()
{
    auto t=splitFixture();t->patternTimingMode=1;t->patternTimingRandom=0x12345678u;
    const auto engineRandom=t->performanceRandom;double sum=0;float smallest=30000,largest=0;
    for(int i=0;i<10000;++i) {
        const float value=t->choosePatternDuration();assert(value>=2000&&value<=12000&&value==std::round(value));
        sum+=value;smallest=std::min(smallest,value);largest=std::max(largest,value);
    }
    assert(std::abs(sum/10000-7000)<100&&smallest<2100&&largest>11900);
    assert(t->performanceRandom==engineRandom);
    setParam(t.get(),"pattern_fade_low_ms","15000");assert(t->patternFadeHighMs==15000);
    const auto seed=t->patternTimingRandom;assert(t->choosePatternDuration()==15000&&t->patternTimingRandom==seed);
    setParam(t.get(),"pattern_fade_high_ms","500");assert(t->patternFadeLowMs==500);
    setParam(t.get(),"pattern_fade_low_ms","-25");assert(t->patternFadeLowMs==0);
    setParam(t.get(),"pattern_fade_high_ms","99999");assert(t->patternFadeHighMs==30000);
    setParam(t.get(),"pattern_fade_low_ms","1250");
    std::array<char,kStateTextSize> json{};assert(t->serializeState(json.data(),json.size())>0);
    auto restored=splitFixture();assert(restored->restoreState(json.data()));
    assert(restored->patternTimingMode==1&&restored->patternFadeLowMs==1250&&restored->patternFadeHighMs==30000);
    // Banks and JSON saved before range timing open in Fixed mode.
    std::string legacy=json.data();
    for(const char* key:{"pattern_fade_mode","pattern_fade_low_ms","pattern_fade_high_ms"}) {
        const auto at=legacy.find(std::string(",\"")+key+"\":");assert(at!=std::string::npos);
        const auto end=legacy.find_first_of(",}",at+1);assert(end!=std::string::npos);legacy.erase(at,end-at);
    }
    assert(restored->restoreState(legacy.c_str()));
    assert(restored->patternTimingMode==0&&restored->patternFadeLowMs==2000&&restored->patternFadeHighMs==12000);
    t->patternTimingMode=0;t->patternFadeMs=30000;
    assert(t->choosePatternDuration()==30000&&t->patternTimingRandom==seed);
}
void testRandomPatternTimingChosenOnce()
{
    auto t=splitFixture();t->setPerformanceMode(0);renderSplit(*t,2);t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
    t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    t->patternTimingMode=1;t->patternTimingRandom=123;
    const auto initialSeed=t->patternTimingRandom;
    assert(!t->beginPatternFade(t->activeClip)&&t->patternTimingRandom==initialSeed);
    assert(t->prepareClipBank(1)&&t->patternTimingRandom==initialSeed);
    t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
    const bool started=t->activatePreparedClip();
    assert(started&&t->patternActive);
    const auto duration=t->patternDurationMs;const auto afterDraw=t->patternTimingRandom;
    assert(duration>=2000&&duration<=12000&&afterDraw!=initialSeed);
    setParam(t.get(),"pattern_fade_low_ms","18000");setParam(t.get(),"pattern_fade_high_ms","25000");
    setParam(t.get(),"pattern_fade_mode","0");setParam(t.get(),"pattern_fade_ms","30000");
    t->setPatternHeld(true);t->setPatternPosition(.5f);t->nudgePatternPosition(1);
    char status[256]{};
    for(int i=0;i<10;++i)assert(t->formatPatternFade(status,sizeof(status))>0);
    assert(t->patternDurationMs==duration&&t->patternTimingRandom==afterDraw);
    std::array<float,kBlockFrames*2u> audio{};t->render(nullptr,kBlockFrames,audio.data());
    t->setPatternHeld(false);
    for(int i=0;i<20;++i){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
    assert(t->patternDurationMs==duration&&t->patternTimingRandom==afterDraw);
    t->finishPatternFade();
    for(int i=0;i<150&&t->patternActive;++i){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
    assert(!t->patternActive);waitForPatternIdle(*t);
    assert(t->prepareClipBank(0));t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
    assert(t->activatePreparedClip()&&t->patternDurationMs==30000&&t->patternTimingRandom==afterDraw);
}

void testPatternFadeStarvationCanFinish() {
    for(bool manual:{false,true}) {
        auto t=splitFixture();t->patternActive=true;t->patternWarmup=false;t->patternHeld=manual;t->patternDurationMs=100;
        t->patternRead.store(0);t->patternWrite.store(0);t->patternLast={.2f,.2f};
        if(manual){t->patternPosition=0;t->patternPositionTarget=0;t->finishPatternFade();}
        int blocks=0;
        for(;blocks<160&&t->patternActive;++blocks){for(auto& ch:t->voiceMix)ch.fill(.3f);t->mixPatternFade(kBlockFrames);for(float x:t->voiceMix[0])assert(std::isfinite(x));}
        assert(!t->patternActive&&t->patternPosition==1&&t->patternGainB>.9999f&&t->patternUnderflows>0);
        assert(blocks<80);assert(t->voiceMix[0].back()>.29f);
        // This fixture owns no worker job; avoid touching its parked scene.
        t->patternJob.store(0);
    }
}
