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
    for(int mode : {0,1,2,3,5}) {
        auto t=splitFixture();t->setPerformanceMode(mode);
        if(mode==5){setParam(t.get(),"layers_surface","1");setParam(t.get(),"layers_corner","0");setParam(t.get(),"layers_fx0","2");}
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
        assert(t->patternShadow->performanceMode()==mode);
        if(mode==5)assert(t->patternShadow->layersLive.values[LayerPatch::fx]==2);
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
    for(bool useDone:{false,true}) for(int mode:{0,5}) {
        auto t=splitFixture();t->setPerformanceMode(mode);
        if(mode==5){setParam(t.get(),"layers_surface","1");setParam(t.get(),"layers_corner","0");setParam(t.get(),"layers_fx0","2");}
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
        assert(t->performanceMode()==mode);if(mode==5)assert(t->layersLive.values[LayerPatch::fx]==2);
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

void testPatternVoiceBankHandoff()
{
    for(int mode=0;mode<=6;++mode) {
        auto t=splitFixture();t->setPerformanceMode(mode);waitForPatternIdle(*t);
        t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
        if(mode==6){
            for(int p=0;p<4;++p)t->sequenceLive.tracks[p].enabled=p==0;
            t->sequenceLive.tracks[0].trigs[0].enabled=true;
            t->commitSequenceEdit();t->startClipPlaybackNow();
        }else t->padNoteOn(68,100);
        renderSplit(*t,30);
        t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);
        t->clipSlots[1]=t->clipSlots[0];
        assert(t->prepareClipBank(1));
        auto* outgoing=t->voices.data();auto* spare=t->patternShadow->voices.data();
        assert(outgoing!=spare);
        for(const auto& v:t->patternShadow->voices)assert(v.engine.prepared());
        const auto beforeAge=t->noteClock;
        t->patternFadeMs=200;t->previewActive=false;t->previewTransitionRemaining=0;
        t->clipPrepareState.store(int(ClipPrepareState::ready));
        assert(t->activatePreparedClip());
        assert(t->patternShadow->voices.data()==outgoing&&t->voices.data()==spare);
        assert(t->patternShadow->noteClock==beforeAge);
        for(std::size_t i=0;i<t->voices.size();++i){
            const auto& v=t->voices[i];
            assert(v.engine.prepared()&&!v.active);
            assert(v.part==(t->splitEnabled?int(i/t->voicesPerPart()):0));
            for(std::size_t lane=0;lane<kLaneCount;++lane)
                assert(v.engine.laneLoaded(lane)==(t->engine.laneLoaded(lane)&&t->voiceOwnsLane(v,lane)));
        }
        setParam(t.get(),"render_profile_enable","1");
        std::array<float,kBlockFrames*2u> output{};
        t->render(nullptr,kBlockFrames,output.data()); // Actual outgoing warmup.
        assert(!t->patternWarmup);
        std::uint64_t total=0;for(auto ns:t->renderProfileCurrent)total+=ns;
        // This direct API activation primed before render() began. The
        // following frame accounts only for jobs and cached output wrapping;
        // the actual in-render activation profile is tested separately.
        assert(total==t->renderProfilePeakTotal&&total>0);
        // Cancel returns the exact running outgoing bank, rather than copying
        // it back. Its asset references and output-buffer addresses stay valid.
        t->returnPatternSource();
        for(int n=0;n<300&&t->patternActive;++n){
            t->render(nullptr,kBlockFrames,output.data());
            const timespec delay{0,1000000};nanosleep(&delay,nullptr);
        }
        assert(!t->patternActive&&t->voices.data()==outgoing);
        for(auto& v:t->voices)for(std::size_t ch=0;ch<kChannelCount;++ch)
            assert(v.outputPointers[ch]==v.output[ch].data());
        waitForPatternIdle(*t);
    }
    // Scene-level pins are shared during a fade. Per-voice pins move with the
    // bank and must not be duplicated or released until outgoing retirement.
    auto t=splitFixture();waitForPatternIdle(*t);
    t->bankAssets[0]=0;t->voices[0].bankAsset=0;
    t->bankPool->assets[0].pins.store(2);
    t->patternFadeMs=200;
    assert(t->beginPatternFade(1));
    assert(t->bankPool->assets[0].pins.load()==3);
    assert(t->patternShadow->voices[0].bankAsset==0&&t->voices[0].bankAsset==-1);
    // Settle the endpoint without reading a sample in this pin-only fixture.
    t->patternWarmup=false;t->patternHeld=false;t->patternPosition=1;
    t->patternGainA=0;t->patternGainB=1;t->mixPatternFade(1);
    waitForPatternIdle(*t);
    assert(t->bankPool->assets[0].pins.load()==1);
    std::puts("Timed fade voice ownership: seven modes, preallocated banks, exact bank return, incoming lane masks, live warmup profile, stable output pointers and balanced asset pins: PASS");
}

void testPatternCompactSnapshotAndRackSwap() {
    static_assert(sizeof(s3g_thru_layers::Rack)<8192,
        "Clip handoff must not copy inline spectral histories");
    auto source=std::make_unique<ClipSlot>();auto destination=std::make_unique<ClipSlot>();
    source->baseValues[17]=.731f;source->splitSettings[2].root=67;
    source->sequence.tracks[2].rate=5;source->clipDurationMs=2345;
    for(const auto counts: {std::array<int,2>{0,0},std::array<int,2>{3,9},
                           std::array<int,2>{int(kClipNoteCapacity),int(kClipPointCapacity)}}){
        source->clipNoteCount=counts[0];source->clipPointCount=counts[1];
        for(auto& n:destination->clipNotes)n.note=91;
        for(auto& point:destination->clipPoints)point.value=.321f;
        for(int i=0;i<counts[0];++i){source->clipNotes[i].note=i%128;source->clipNotes[i].startMs=i*3;}
        for(int i=0;i<counts[1];++i){source->clipPoints[i].value=i*.125f;source->clipPoints[i].timeMs=i*7;}
        ThruInstance::copyPatternClip(*destination,*source);
        assert(!std::memcmp(destination.get(),source.get(),offsetof(ClipSlot,clipNotes)));
        constexpr auto tail=offsetof(ClipSlot,clipPointStarts);
        assert(!std::memcmp(reinterpret_cast<const char*>(destination.get())+tail,
                            reinterpret_cast<const char*>(source.get())+tail,sizeof(ClipSlot)-tail));
        assert(!std::memcmp(destination->clipNotes.data(),source->clipNotes.data(),counts[0]*sizeof(ClipNote)));
        assert(!std::memcmp(destination->clipPoints.data(),source->clipPoints.data(),counts[1]*sizeof(ClipPoint)));
        if(counts[0]<int(kClipNoteCapacity))assert(destination->clipNotes[counts[0]].note==91);
        if(counts[1]<int(kClipPointCapacity))assert(destination->clipPoints[counts[1]].value==.321f);
    }
    std::array<s3g_thru_layers::Rack,4> racks;
    std::array<LayerPatch,2> patches;
    for(int i=0;i<2;++i){
        auto& patch=patches[i];patch.values[LayerPatch::fx]=i?2:1;
        patch.values[LayerPatch::config+LayerPatch::weight0]=1;
        for(int c=1;c<4;++c)patch.values[LayerPatch::config+LayerPatch::weight0+c]=0;
        patch.values[LayerPatch::config+LayerPatch::zMode]=1;
    }
    for(int i=0;i<4;++i){assert(racks[i].prepare(44100));racks[i].prime(patches[i%2]);}
    for(int block=0;block<80;++block){
        if(block==24||block==56)racks[0].swapWith(racks[1]);
        for(int stream=0;stream<2;++stream){
            const int actual=(block>=24&&block<56)?1-stream:stream;
            std::array<std::array<float,kBlockFrames>,2> audio{},reference{};
            for(std::size_t f=0;f<kBlockFrames;++f){
                const float phase=(block*kBlockFrames+f)*(.071f+stream*.033f);
                audio[0][f]=.2f*std::sin(phase);audio[1][f]=.11f*std::cos(phase*.7f);
            }
            reference=audio;racks[actual].process(kBlockFrames,audio,patches[stream]);
            racks[stream+2].process(kBlockFrames,reference,patches[stream]);
            for(int ch=0;ch<2;++ch)for(std::size_t f=0;f<kBlockFrames;++f)
                assert(std::abs(audio[ch][f]-reference[ch][f])<1e-7f);
            assert(racks[actual].currentWeights()==racks[stream+2].currentWeights());
        }
    }
    std::puts("Timed fade compact snapshots: empty/sparse/full clip content, untouched unused capacity, small rack handoff and exact stereo/history continuity through swaps: PASS");
}

void testPatternWorkerSignal() {
    s3g_daisy::ThruThreadSignal signal;assert(signal.prepare());
    std::atomic<int> requested{0},acknowledged{0};
    std::atomic<bool> running{true};
    std::thread worker([&]{while(running.load(std::memory_order_acquire)){
        const int next=requested.load(std::memory_order_acquire);
        if(next!=acknowledged.load(std::memory_order_relaxed))acknowledged.store(next,std::memory_order_release);
        signal.wait();
    }});
    for(int batch=1;batch<=300;++batch){
        requested.store(batch,std::memory_order_release);
        for(int repeat=0;repeat<8;++repeat)signal.notify();
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(acknowledged.load(std::memory_order_acquire)!=batch&&std::chrono::steady_clock::now()<end)
            std::this_thread::yield();
        assert(acknowledged.load(std::memory_order_acquire)==batch);
    }
    running.store(false,std::memory_order_release);signal.notify();worker.join();
    std::puts("Timed fade worker wake: repeated/batched publication, concurrent drains and clean shutdown: PASS");
}

void testPatternBoundaryLaunchBudget() {
    auto t=splitFixture();t->setPerformanceMode(0);
    t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
    t->padNoteOn(68,100);renderSplit(*t,30);
    t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    t->clipSlots[0].clipRecorded=true;t->clipSlots[0].clipDurationMs=1000;
    t->clipSlots[0].clipNoteCount=t->clipSlots[0].clipPointCount=0;
    t->clipPlaying=true;t->clipNeedsStart=false;t->clipPositionMs=t->clipAbsolutePositionMs=999;
    t->queuedClip=1;assert(t->prepareClipBank(1));
    t->clipPrepareState.store(int(ClipPrepareState::ready));t->patternFadeMs=200;
    setParam(t.get(),"render_profile_enable","1");
    auto* originalBank=t->voices.data();std::array<float,kBlockFrames*2u> output{};
    t->render(nullptr,kBlockFrames,output.data());
    // The boundary callback rendered its normal source block, not an extra
    // warmup block. The source remains intact until the next callback starts.
    assert(t->patternDeferredClip==1&&!t->patternActive&&t->activeClip==0);
    assert(t->voices.data()==originalBank);
    t->render(nullptr,kBlockFrames,output.data());
    assert(t->patternDeferredClip<0&&t->activeClip==1&&t->patternActive&&!t->patternWarmup);
    assert(t->patternShadow->voices.data()==originalBank&&t->patternRead.load()==0);
    assert(t->renderProfileCurrent[2]>0); // Real outgoing render is still attributed.
    std::uint64_t total=0;for(auto ns:t->renderProfileCurrent)total+=ns;
    assert(total>0&&total<=t->renderProfilePeakTotal);
    t->finishPatternFade();
    for(int n=0;n<160&&t->patternActive;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,output.data());}
    waitForPatternIdle(*t);
    t->patternDeferredClip=0;t->patternDeferredSource=1;
    t->clipPlaying=false;t->queuedClip=-1;t->serviceDeferredPatternLaunch();
    assert(t->patternDeferredClip<0&&!t->patternActive&&t->activeClip==1);
    std::puts("Timed fade boundaries: defer prefill past an already rendered host block; launch next callback, retain source ownership, profile actual DSP and cancel superseded requests: PASS");
}

void testPatternEarlyWorkerHandoff() {
    auto t=splitFixture();auto reference=splitFixture();
    for(auto* scene:{t.get(),reference.get()}) {
        scene->setPerformanceMode(0);scene->currentLaneSourceKinds.fill(LaneSourceKind::capture);
        scene->padNoteOn(68,100);renderSplit(*scene,30);
    }
    t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    assert(t->prepareClipBank(1));t->patternFadeMs=200;
    t->clipPrepareState.store(int(ClipPrepareState::ready));
    // This seam is before any destination storage, engine or settings change.
    // Waiting here is test-only: prove the real worker is already eligible to
    // render, with a complete private source prefix, before activation ends.
    t->patternHandoffTestReady=[](ThruInstance& scene){
        assert(scene.activeClip==0&&scene.patternWarmup&&scene.patternWarmupOffset==0);
        assert(scene.patternJob.load(std::memory_order_acquire)==1);
        assert(scene.patternRead.load()==0);
        assert(scene.engine.laneData(0,0)==scene.patternShadow->engine.laneData(0,0));
        const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(scene.patternWrite.load(std::memory_order_acquire)<2*kBlockFrames&&std::chrono::steady_clock::now()<limit)
            std::this_thread::yield();
        assert(scene.patternWrite.load(std::memory_order_acquire)>=2*kBlockFrames);
    };
    assert(t->activatePreparedClip());
    assert(t->activeClip==1&&t->patternWarmup&&t->patternWarmupOffset==0);
    std::array<float,kBlockFrames*2u> expected{},actual{};
    reference->render(nullptr,kBlockFrames,expected.data());
    // A launch near a host-block boundary must consume every prefix sample
    // once, even when the host supplies several short chunks.
    std::size_t offset=0;
    const auto clock=t->renderClockSamples.load();
    for(int frames:{17,37,74}){
        t->render(nullptr,frames,actual.data());
        for(int f=0;f<frames*2;++f)assert(std::abs(actual[f]-expected[offset*2u+f])<1e-7f);
        offset+=frames;assert(t->patternWarmup==(offset<kBlockFrames));
        assert(t->patternRead.load()==0&&t->patternUnderflows==0);
    }
    assert(t->renderClockSamples.load()==clock+kBlockFrames);
    reference->render(nullptr,kBlockFrames,expected.data());
    for(std::size_t f=0;f<expected.size();++f)assert(std::abs(t->patternRing[f]-expected[f])<1e-7f);
    t->setPatternPosition(0);t->render(nullptr,kBlockFrames,actual.data());
    for(std::size_t f=0;f<expected.size();++f)assert(std::abs(actual[f]-expected[f])<1e-7f);
    assert(t->patternRead.load()==kBlockFrames&&t->patternUnderflows==0);
    t->finishPatternFade();
    for(int n=0;n<160&&t->patternActive;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,actual.data());}
    waitForPatternIdle(*t);
    // A single final bind must still restore all ownership masks when layouts
    // differ. Cover every source/destination layout, including Bank and Seq.
    t->activeClip=0;
    for(int from=0;from<=6;++from)for(int to=0;to<=6;++to)if(from!=to){
        t->setPerformanceMode(to);t->captureActiveClip(true);t->clipSlots[1]=*t->activeClipSlot();
        t->copyCurrentSourcesToClip(t->clipSlots[1]);
        t->setPerformanceMode(from);t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);
        assert(t->prepareClipBank(1));t->clipPrepareState.store(int(ClipPrepareState::ready));
        t->patternFadeMs=0;t->patternHeld=false;t->patternHandoffTestReady=nullptr;
        assert(t->activatePreparedClip()&&t->performanceMode()==to);
        for(std::size_t i=0;i<t->voices.size();++i){
            const auto& v=t->voices[i];assert(v.part==(t->splitEnabled?int(i/t->voicesPerPart()):0));
            for(std::size_t lane=0;lane<kLaneCount;++lane)
                assert(v.engine.laneLoaded(lane)==(t->engine.laneLoaded(lane)&&t->voiceOwnsLane(v,lane)));
        }
        // Reuse clip zero as the source for the next pair.
        t->activeClip=0;
    }
    std::puts("Timed fade startup: source prefix and live producer before destination activation; exact stereo across partial blocks; 42 layout ownership changes: PASS");
}

void testPatternWorkerFloatAndReport() {
    std::atomic<bool> flushed{false};
    std::thread canary([&]{
#if defined(__aarch64__)
        unsigned long mode;__asm__ __volatile__("mrs %0, fpcr":"=r"(mode));
        mode&=~(1UL<<24);__asm__ __volatile__("msr fpcr, %0"::"r"(mode));
#elif defined(__SSE__)
        _mm_setcsr(_mm_getcsr()&~0x8040u);
#endif
        assert(s3g_daisy::enableAudioDenormalFlush());
        volatile float normal=std::numeric_limits<float>::min();
        volatile float quarter=.25f;
        const float tiny=normal*quarter;
        flushed.store(tiny==0,std::memory_order_release);
    });
    canary.join();assert(flushed.load(std::memory_order_acquire));
    auto t=splitFixture();t->setPerformanceMode(0);waitForPatternIdle(*t);
    t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
    t->padNoteOn(68,100);renderSplit(*t,10);
    t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    assert(t->prepareClipBank(1));t->patternFadeMs=200;
    t->previewActive=false;t->previewTransitionRemaining=0;
    t->clipPrepareState.store(int(ClipPrepareState::ready));
    setParam(t.get(),"render_profile_enable","1");assert(t->activatePreparedClip());
    std::array<float,kBlockFrames*2> output{};t->render(nullptr,kBlockFrames,output.data());
    waitForPatternAudio(*t);
    assert(t->patternWorkerFlush.load()==1&&t->patternWorkerWallPeak.load()>0);
    assert(t->patternWorkerWakePeak.load()>0);
    assert(t->patternMaximumQueued.load()>0&&t->patternMaximumQueued.load()<=ThruInstance::kPatternRingFrames);
    const int nice=t->patternWorkerNice.load();assert((nice>=-20&&nice<=19)||nice==99);
    timespec clock{};if(readRenderCpuClock(clock))assert(t->patternWorkerCpuAvailable.load()&&t->patternWorkerCpuPeak.load()>0);
    char report[256];assert(getParamValue(t.get(),"pattern_worker_state",report,sizeof(report))>0);
    assert(report[0]=='1');t->finishPatternFade();
    for(int n=0;n<160&&t->patternActive;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,output.data());}
    waitForPatternIdle(*t);setParam(t.get(),"performance_reset","1");
    assert(t->patternWorkerWallPeak.load()==0&&t->patternWorkerCpuPeak.load()==0&&t->patternMaximumQueued.load()==0&&t->patternWorkerWakePeak.load()==0);
    assert(t->patternWorkerFlush.load()==1); // Reset retains the actual thread mode.

    auto missing=std::make_unique<ThruInstance>();missing->patternActive=missing->patternHeld=true;
    missing->patternPosition=missing->patternPositionTarget=.5;
    missing->mixPatternFade(17); // No source block has arrived.
    missing->patternWrite.store(8);missing->mixPatternFade(12); // Eight real frames; four missing.
    missing->mixPatternFade(5); // After consuming the first source frames.
    assert(missing->patternUnderflows==3&&missing->patternStartUnderflows==2&&missing->patternMissingFrames==26);
    assert(getParamValue(missing.get(),"pattern_worker_state",report,sizeof(report))>0);
    int flush,cpu;unsigned long long wall,cpuUs,start,run;double ms,queue;
    assert(std::sscanf(report,"%d,%d,%llu,%llu,%llu,%llu,%lf,%lf",&flush,&cpu,&wall,&cpuUs,&start,&run,&ms,&queue)==8);
    assert(start==2&&run==1&&std::abs(ms-26000.0/missing->sampleRate)<.001);
    setParam(missing.get(),"performance_reset","1");
    assert(missing->patternUnderflows==0&&missing->patternStartUnderflows==0&&missing->patternMissingFrames==0);
    std::puts("Fade worker: per-thread denormal flush, real asynchronous CPU/wall profile, partial-frame startup/run accounting and diagnostic reset: PASS");
}

void testPatternSchedulerReserve() {
    // A deterministic producer pause exercises the actual fade mixer without
    // relying on the host OS to reproduce a particular scheduling accident.
    const auto source=[](std::uint64_t frame,int channel){
        return .2f*std::sin(float(frame)*.019f+float(channel));
    };
    for(const auto reserve:{std::size_t(1024),ThruInstance::kPatternRingFrames}){
        auto t=std::make_unique<ThruInstance>();
        t->patternActive=t->patternHeld=true;t->patternGainA=1;t->patternGainB=0;
        const auto pauseFrames=std::size_t(t->sampleRate*.035f);
        const auto totalFrames=std::size_t(t->sampleRate*.16f);
        auto fill=[&]{
            auto write=t->patternWrite.load();const auto read=t->patternRead.load();
            while(write-read+kBlockFrames<=reserve){
                for(std::size_t f=0;f<kBlockFrames;++f){
                    const auto at=((write+f)&(ThruInstance::kPatternRingFrames-1))*2;
                    for(int ch=0;ch<2;++ch)t->patternRing[at+ch]=source(write+f,ch);
                }
                write+=kBlockFrames;
            }
            t->patternWrite.store(write);
            assert(write-read<=reserve);
        };
        fill();
        const std::array<std::size_t,8> chunks{31,97,64,64,5,123,128,17};
        std::size_t frame=0,index=0;
        while(frame<totalFrames){
            if(frame>=pauseFrames)fill();
            const auto count=std::min(chunks[index++%chunks.size()],totalFrames-frame);
            for(auto& channel:t->voiceMix)channel.fill(.1f);
            t->mixPatternFade(count);
            for(int ch=0;ch<2;++ch)for(std::size_t f=0;f<count;++f){
                assert(std::isfinite(t->voiceMix[ch][f]));
                if(reserve==ThruInstance::kPatternRingFrames)
                    assert(std::abs(t->voiceMix[ch][f]-source(frame+f,ch))<1e-6f);
            }
            frame+=count;
        }
        if(reserve==1024)assert(t->patternUnderflows>0&&t->patternMissingFrames>0);
        else assert(t->patternUnderflows==0&&t->patternMissingFrames==0);
    }
    std::puts("Fade scheduler reserve: injected 35 ms producer pause, old-reserve underrun reproduced, new reserve exact stereo timing, mixed Sequence chunks and circular wraps: PASS");
}

void testPatternFadeChunkBoundary() {
    const auto fixture=[](bool done){
        auto t=std::make_unique<ThruInstance>();
        t->patternActive=true;t->patternHeld=done;t->patternFinishRequested=done;
        t->patternPosition=t->patternPositionTarget=1;
        t->patternGainA=1.01e-5f;t->patternGainB=1-t->patternGainA;
        for(std::size_t f=0;f<3;++f){t->patternRing[f*2]=-.2f;t->patternRing[f*2+1]=.3f;}
        t->patternWrite.store(3);t->voiceMix[0].fill(.6f);t->voiceMix[1].fill(-.4f);
        return t;
    };
    for(bool done:{false,true}){
        auto t=fixture(done);t->mixPatternFade(16);
        assert(!t->patternActive&&t->patternJob.load()==2);
        // The last thirteen frames need only the already rendered incoming
        // bus. No outgoing audio is missing after the fade has completed.
        assert(t->patternUnderflows==0&&t->patternMissingFrames==0);
        for(std::size_t f=3;f<16;++f){
            assert(t->voiceMix[0][f]==.6f&&t->voiceMix[1][f]==-.4f);
        }
    }
    auto held=fixture(false);held->patternHeld=true;
    held->mixPatternFade(16);
    assert(held->patternActive&&held->patternUnderflows==1&&held->patternMissingFrames==13);
    auto active=fixture(false);active->patternHeld=true;
    active->patternPosition=active->patternPositionTarget=.5;
    active->patternGainA=active->patternGainB=.5f;active->mixPatternFade(16);
    assert(active->patternActive&&active->patternUnderflows==1&&active->patternMissingFrames==13);
    std::puts("Fade chunk boundaries: timed and DONE tail stop on the exact completed frame; genuine and manually parked shortages remain counted: PASS");
}

void testPatternFadePublication() {
    const auto source=[](std::uint64_t frame,int channel){return .2f*std::sin(float(frame)*.013f+float(channel));};
    auto t=std::make_unique<ThruInstance>();t->patternActive=t->patternHeld=true;
    t->patternGainA=1;t->patternGainB=0;t->patternRead.store(96);t->patternWrite.store(128);
    for(std::uint64_t i=96;i<128;++i)for(int ch=0;ch<2;++ch)t->patternRing[i*2+ch]=source(i,ch);
    t->patternMixTestPublication=[](ThruInstance& scene,std::size_t frame){
        if(frame!=32)return;
        for(std::uint64_t i=128;i<256;++i)for(int ch=0;ch<2;++ch)
            scene.patternRing[i*2+ch]=.2f*std::sin(float(i)*.013f+float(ch));
        scene.patternWrite.store(256,std::memory_order_release);
    };
    t->mixPatternFade(64);
    assert(t->patternRead.load()==160&&t->patternUnderflows==0&&t->patternMissingFrames==0);
    for(int ch=0;ch<2;++ch)for(std::size_t f=0;f<64;++f)
        assert(std::abs(t->voiceMix[ch][f]-source(96+f,ch))<1e-6f);
    auto late=std::make_unique<ThruInstance>();late->patternActive=late->patternHeld=true;
    late->patternGainA=1;late->patternGainB=0;late->patternLast={.4f,.2f};
    late->patternMixTestPublication=[](ThruInstance& scene,std::size_t frame){
        if(frame!=3)return;
        for(std::size_t i=0;i<kBlockFrames;++i){scene.patternRing[i*2]=.6f;scene.patternRing[i*2+1]=.3f;}
        scene.patternWrite.store(kBlockFrames,std::memory_order_release);
    };
    late->mixPatternFade(64);
    assert(late->patternRead.load()==61&&late->patternUnderflows==1&&late->patternMissingFrames==3);
    assert(!late->patternStarved);
    float previous=.4f;
    for(std::size_t f=0;f<64;++f){assert(std::abs(late->voiceMix[0][f]-previous)<.01f);previous=late->voiceMix[0][f];}
    std::puts("Fade publication: exact stereo prefix/next-block continuity, no stale-read underrun, true three-frame shortage counted and smoothly bridged: PASS");
}
