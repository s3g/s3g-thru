void testSequenceGenerateMath() {
    using namespace s3g_thru_generate;
    for(int length=1;length<=32;++length)for(int hitCount=0;hitCount<=length;++hitCount){
        Lane base;base.length=length;
        for(int i=0;i<32;++i){auto& s=base.trigs[i];s.enabled=i%3==0;s.locks.mask=15;s.locks.values={.61f,7,91,1.2f};s.micro=.12f;s.probability=.7f;s.cycleEvery=3;s.cycleHit=2;setCount(s,3);s.events[1].locks.mask=2;s.events[1].locks.values[1]=-5;}
        Config c;c.set(hits,float(hitCount),length,false);Context source;
        const auto r=build(base,c,source);assert(r.enabled==hitCount);
        std::array<int,32> on{};int n=0;
        for(int i=0;i<length;++i){auto expected=base.trigs[i];expected.enabled=r.lane.trigs[i].enabled;assert(sameStep(expected,r.lane.trigs[i]));if(expected.enabled)on[n++]=i;}
        for(int i=length;i<32;++i)assert(sameStep(base.trigs[i],r.lane.trigs[i]));
        if(hitCount)for(int i=0;i<n;++i){const int gap=(on[(i+1)%n]-on[i]+length-1)%length+1;assert(gap==length/hitCount||gap==(length+hitCount-1)/hitCount);}
        c.v[amount]=0;assert(sameLane(build(base,c,source).lane,base));
    }
    for(int mode=0;mode<6;++mode)for(int repeats:{1,2,16}){
        Traversal a,b;a.mode=b.mode=mode;a.count=b.count=7;a.repeats=b.repeats=repeats;
        std::array<bool,7> seen{};
        for(int i=0;i<7*repeats*4;++i){const int p=a.at(i);assert(p==b.at(i)&&p>=0&&p<7);if(i%repeats)assert(p==a.walk);
            if(mode==5&&i%repeats==0){if(i%(7*repeats)==0)seen.fill(false);assert(!seen[p]);seen[p]=true;}}
    }
    Traversal spread;spread.mode=6;spread.count=7;spread.visits=3;
    assert(spread.at(0)==0&&spread.at(1)==3&&spread.at(2)==6);
    Lane base;Config c;Context ctx;ctx.count=4;ctx.points={.1f,.3f,.5f,.7f};ctx.preRollNormalized=.01f;
    c.v[hits]=16;c.v[map]=2;c.v[repeat]=2;
    base.trigs[0].locks.mask=1;base.trigs[0].locks.values[0]=.93f;
    base.trigs[1].events[0].locks.mask=1;base.trigs[1].events[0].locks.values[0]=.81f;
    auto r=build(base,c,ctx);assert(r.lane.trigs[0].locks.values[0]==.93f);
    assert(r.lane.trigs[1].events[0].locks.values[0]==.81f);
    assert(std::abs(r.lane.trigs[2].locks.values[0]-.29f)<1e-6f&&r.lane.trigs[2].locks.values[0]==r.lane.trigs[3].locks.values[0]);
    c.v[writePolicy]=1;r=build(base,c,ctx);assert(std::abs(r.lane.trigs[0].locks.values[0]-.09f)<1e-6f);
    assert(std::abs(r.lane.trigs[1].events[0].locks.values[0]-.09f)<1e-6f);
    c.v[amount]=.4f;const auto first=build(base,c,ctx);assert(sameLane(first.lane,build(base,c,ctx).lane));
    for(int root:{0,60,127})for(int mode=1;mode<=5;++mode){
        base={};base.scaleEntry=true;ctx.root=root;ctx.scale=(1u<<0)|(1u<<3)|(1u<<7);
        c={};c.v[hits]=16;c.v[pitch]=float(mode);c.v[span]=24;
        const auto pitched=build(base,c,ctx);
        for(const auto& s:pitched.lane.trigs)if(s.enabled){const int n=int(s.locks.values[1]);assert(n+root>=0&&n+root<=127&&std::abs(n)<=24&&(ctx.scale&(1u<<((n+120)%12))));}
    }
    base={};c={};c.v[hits]=4;c.v[duration]=2;c.v[minimum]=.1f;c.v[maximum]=8;
    r=build(base,c,ctx);for(const auto& s:r.lane.trigs)if(s.enabled)assert(s.locks.values[3]==4);
    base.timeMode=true;c.v[minimum]=50;c.v[maximum]=1000;ctx.stepMs=125;
    r=build(base,c,ctx);for(const auto& s:r.lane.trigs)if(s.enabled)assert(s.locks.timeLocked&&s.locks.timeMs==500);
    c.v[duration]=3;r=build(base,c,ctx);for(const auto& s:r.lane.trigs)if(s.enabled)assert(s.locks.timeMs>=50&&s.locks.timeMs<=1000);
    c.v[accents]=2;c.v[depth]=.5f;auto accented=build(base,c,ctx);int loud=0,quiet=0;
    for(const auto& s:accented.lane.trigs)if(s.enabled){loud+=s.locks.values[2]==100;quiet+=s.locks.values[2]==50;}assert(loud==2&&quiet==2);
    base={};c={};c.set(rhythm,2,16,false);assert(c.v[map]==1);ctx={};ctx.count=64;
    for(int i=0;i<64;++i)ctx.points[i]=i*.0001f;
    auto audio=build(base,c,ctx);assert(audio.enabled==1&&audio.events==8&&audio.dropped==56);
    for(int i=0;i<8;++i){assert(audio.lane.trigs[0].events[i].when>=0&&audio.lane.trigs[0].events[i].when<1);if(i)assert(audio.lane.trigs[0].events[i].when>=audio.lane.trigs[0].events[i-1].when);}
    const auto unchanged=c.v;assert(!c.set(0,NAN,16,false)&&!c.set(88,1,16,false)&&c.v==unchanged);
    std::puts("Sequence Generate core: all Euclidean lengths/counts, uniform gaps, untouched locks/tails, six traversals/repeats, deterministic mutation, scale/root bounds, duration units, accents and bounded audio-onset Bursts: PASS");
}
void testSequenceGenerateTransaction() {
    using namespace s3g_thru_generate;
    auto t=sequenceFixture();t->setSequenceParameter("seq_length","13");
    t->sequenceLive.tracks[0].trigs[3].enabled=true;t->sequenceLive.tracks[0].trigs[3].locks.mask=2;t->sequenceLive.tracks[0].trigs[3].locks.values[1]=7;
    t->commitSequenceEdit();t->captureActiveClip(true);
    const auto original=t->sequenceLive;const auto dirty=t->persistentRevision;
    setParam(t.get(),"seq_gen_begin","1");assert(t->sequenceGenerateOwner());
    setParam(t.get(),"seq_gen_edit","1,5");setParam(t.get(),"seq_gen_edit","2,5");setParam(t.get(),"seq_gen_edit","3,5");
    setParam(t.get(),"seq_gen_edit","8,7");
    assert(t->sequenceGenerateValid&&t->sequenceGenerateDraft.enabled==5);
    assert(t->persistentRevision==dirty&&sameLane(t->sequenceLive.tracks[0],original.tracks[0]));
    t->sequencePadDown(24);t->sequencePadUp(24);assert(sameLane(t->sequenceLive.tracks[0],original.tracks[0]));
    std::array<char,4096> state{};const int n=t->sequenceGenerateState(state.data(),state.size());assert(n>0&&n<int(state.size())&&std::strstr(state.data(),"\"config\":["));
    const std::string expected=state.data();for(int i=0;i<20;++i){t->sequenceGenerateState(state.data(),state.size());assert(expected==state.data());}
    if(const auto* path=std::getenv("S3G_THRU_GENERATOR_FIXTURE")){auto file=std::fopen(path,"wb");assert(file);assert(std::fwrite(state.data(),1,n,file)==std::size_t(n));std::fclose(file);}
    setParam(t.get(),"seq_gen_cancel","1");assert(!t->sequenceGenerateOwner()&&t->persistentRevision==dirty);
    setParam(t.get(),"seq_gen_begin","1");const auto draft=t->sequenceGenerateDraft.lane;
    t->startClipPlaybackNow();t->startSequenceVoice(0,3,0,4,false,0);renderSplit(*t,17);
    const auto heldAge=t->voices[0].age;const auto heldElapsed=t->voices[0].sequence.elapsed;
    assert(t->voices[0].active&&t->voices[0].held);
    const auto beat=t->sequenceBeat;const auto offsets=t->sequenceBeatOffsets;
    const auto counters=t->sequenceEventCount;
    setParam(t.get(),"seq_gen_apply","1");assert(!t->sequenceGenerateActive&&t->undoAvailable&&t->undoIsSequenceGenerate);
    assert(t->sequenceBeat==beat&&t->sequenceBeatOffsets==offsets&&t->sequenceEventCount==counters);
    assert(sameLane(t->sequenceLive.tracks[0],draft)&&sameLane(t->activeClipSlot()->sequence.tracks[0],draft));
    assert(t->voices[0].age==heldAge&&t->voices[0].sequence.elapsed==heldElapsed&&t->voices[0].held);
    for(int p=1;p<4;++p)assert(sameLane(t->sequenceLive.tracks[p],original.tracks[p]));
    t->restoreUndo();assert(t->voices[0].age==heldAge&&t->voices[0].sequence.elapsed==heldElapsed&&t->voices[0].held);assert(!t->undoAvailable&&sameLane(t->sequenceLive.tracks[0],original.tracks[0]));
    assert(t->sequenceBeat==beat&&t->sequenceBeatOffsets==offsets&&t->sequenceEventCount==counters);
    setParam(t.get(),"seq_gen_begin","1");setParam(t.get(),"seq_gen_edit","6,0");
    t->undoAvailable=true;setParam(t.get(),"seq_gen_apply","1");assert(t->undoAvailable);
    setParam(t.get(),"seq_gen_begin","1");setParam(t.get(),"seq_note","9");assert(!t->sequenceGenerateActive);
    setParam(t.get(),"seq_gen_begin","1");
    setParam(t.get(),"seq_eq_low","3");assert(t->sequenceGenerateOwner());
    t->selectSplitPart(1,false);setParam(t.get(),"seq_note","4");t->selectSplitPart(0,false);assert(t->sequenceGenerateOwner());
    const auto manual=t->sequenceLive.tracks[0];++t->wavesetSourceRevisions[0];
    setParam(t.get(),"seq_gen_apply","1");assert(!t->sequenceGenerateActive&&sameLane(t->sequenceLive.tracks[0],manual));
    setParam(t.get(),"seq_gen_begin","1");t->selectSplitPart(1);assert(!t->sequenceGenerateActive);
    setParam(t.get(),"seq_gen_begin","1");t->clipRecording=true;setParam(t.get(),"seq_gen_apply","1");assert(t->sequenceLive.tracks[1].trigs[t->sequenceStep].locks.values[1]==4);t->clipRecording=false;t->cancelSequenceGenerate();
    t->setClipPlaying(false);t->selectSplitPart(0);t->sequenceGenerateConfigs[0].v[amount]=1;
    // Explicit missing maps remain invalid; Auto offers the existing fallback.
    setParam(t.get(),"seq_gen_begin","1");setParam(t.get(),"seq_gen_edit","2,3");assert(!t->sequenceGenerateValid);
    setParam(t.get(),"seq_gen_edit","2,1");assert(t->sequenceGenerateValid);
    setParam(t.get(),"seq_gen_apply","1");
    t->startClipPlaybackNow();std::array<std::int16_t,kBlockFrames*2> audio{};long long energy=0;
    for(int block=0;block<700;++block){t->render(audio.data(),kBlockFrames);for(auto x:audio)energy+=std::int64_t(x)*x;
        int active=0;for(const auto& voice:t->voices)if(voice.active&&voice.part==0)++active;assert(active<=2);}
    assert(energy>100000);t->setClipPlaying(false);
    assert(t->copyClipToClipboard(0));t->pasteClipFromClipboard(4);
    assert(sameLane(t->clipSlots[4].sequence.tracks[0],t->sequenceLive.tracks[0]));
    std::puts("Sequence Generate transaction: cached native preview, clean cancellation, pad selection, explicit Apply, phase/other-lane/held-note preservation, grouped Undo, no-op, stale source/manual edit guards, map readiness and clip copying: PASS");
}
void testSequenceGenerateLive() {
    using namespace s3g_thru_generate;
    auto t=sequenceFixture();
    const auto original=t->sequenceLive;
    const auto saved=t->activeClipSlot()->sequence;
    const bool recorded=t->activeClipSlot()->clipRecorded;
    const auto revision=t->persistentRevision;
    setParam(t.get(),"seq_gen_begin","1");
    setParam(t.get(),"seq_gen_edit","1,4");setParam(t.get(),"seq_gen_edit","2,5");
    setParam(t.get(),"seq_gen_edit","8,4");setParam(t.get(),"seq_gen_edit","5,1");
    setParam(t.get(),"seq_gen_edit","12,1");setParam(t.get(),"seq_gen_edit","13,12");
    setParam(t.get(),"seq_gen_edit","14,2");setParam(t.get(),"seq_gen_edit","16,1");
    setParam(t.get(),"seq_gen_edit","17,16");
    assert(sameLane(t->sequencePlayback.tracks[0],t->sequenceGenerateDraft.lane));
    t->launchQuantize=LaunchQuantize::bar;
    setParam(t.get(),"seq_gen_play","1");assert(t->clipPlaying&&!t->clipLaunchPending);
    std::array<float,kBlockFrames*2> out{};double energy=0;
    for(int b=0;b<50;++b){t->render(nullptr,kBlockFrames,out.data());for(float x:out){assert(std::isfinite(x));energy+=x*x;}}
    assert(energy>.01&&t->sequenceEventCount[0]>0);
    assert(t->sequenceGenerateActive&&t->persistentRevision==revision&&t->activeClipSlot()->clipRecorded==recorded);
    assert(sameLane(t->sequenceLive.tracks[0],original.tracks[0])&&sameLane(t->activeClipSlot()->sequence.tracks[0],saved.tracks[0]));
    // The actual scheduled voices must resolve every generated field, not the authored trigs.
    for(const auto& voice:t->voices)if(voice.active&&voice.part==0){
        assert(voice.sequence.generatedPreview);
        const auto& lane=t->sequencePlayback.tracks[0];const auto& step=lane.trigs[voice.sequence.step];
        assert(voice.note==t->settingsForPart(0).root+int(resolve(lane,step,voice.sequence.child,1,0)));
        assert(voice.velocity==int(resolve(lane,step,voice.sequence.child,2,lane.velocity)));
        assert(voice.sequence.position==resolve(lane,step,voice.sequence.child,0,t->controls.locus));
    }
    const auto phase=t->sequenceBeat;const auto offsets=t->sequenceBeatOffsets;
    setParam(t.get(),"seq_gen_edit","1,7");assert(t->sequenceBeat==phase&&t->sequenceBeatOffsets==offsets);
    assert(sameLane(t->sequencePlayback.tracks[0],t->sequenceGenerateDraft.lane));
    setParam(t.get(),"seq_eq_low","2");
    t->selectSplitPart(1,false);setParam(t.get(),"seq_note","4");t->selectSplitPart(0,false);
    assert(t->sequenceGenerateActive&&sameLane(t->sequencePlayback.tracks[0],t->sequenceGenerateDraft.lane));
    // Cancel only retires preview-owned notes; unrelated voices keep their phase and gate.
    t->startSequenceVoice(1,0,0,4);const auto otherAge=t->voices[2].age;
    auto& retiring=t->voices[0];assert(retiring.active&&retiring.sequence.generatedPreview);
    retiring.held=false;t->beginVoiceRelease(retiring);
    retiring.envelopeDurationSet=true;retiring.envelopeStageFrames=43;
    setParam(t.get(),"seq_gen_cancel","1");
    assert(!retiring.envelopeDurationSet&&retiring.envelopeStageFrames==0);
    assert(t->sequenceBeat==phase&&t->sequenceBeatOffsets==offsets);
    assert(sameLane(t->sequencePlayback.tracks[0],original.tracks[0]));
    assert(t->voices[2].active&&t->voices[2].held&&t->voices[2].age==otherAge);
    for(const auto& voice:t->voices)if(voice.active&&voice.part==0){
        assert(!voice.sequence.generatedPreview&&!voice.held);
        assert(voice.sequence.frames-voice.sequence.elapsed<=std::uint64_t(std::llround(t->sampleRate*.005f)));
    }
    renderSplit(*t,5);for(const auto& voice:t->voices)if(voice.part==0)assert(!voice.active);
    const auto events=t->sequenceEventCount[0];renderSplit(*t,100);assert(t->sequenceEventCount[0]==events);
    setParam(t.get(),"seq_gen_begin","1");setParam(t.get(),"seq_gen_play","1");assert(!t->clipPlaying);
    setParam(t.get(),"seq_gen_play","1");assert(t->clipPlaying&&!t->clipLaunchPending);
    renderSplit(*t,4);assert(t->sequenceEventCount[0]>0);
    assert(sameLane(t->sequencePlayback.tracks[0],t->sequenceGenerateDraft.lane));
    auto& held=t->voices[0];assert(held.active&&held.held&&held.sequence.generatedPreview);
    const auto age=held.age;const auto elapsed=held.sequence.elapsed;const auto applyPhase=t->sequenceBeat;
    setParam(t.get(),"seq_gen_apply","1");
    assert(!t->sequenceGenerateActive&&held.active&&held.held&&!held.sequence.generatedPreview);
    assert(held.age==age&&held.sequence.elapsed==elapsed&&t->sequenceBeat==applyPhase);
    assert(sameLane(t->sequenceLive.tracks[0],t->sequencePlayback.tracks[0]));
    const auto applied=t->sequenceLive.tracks[0];
    // A missing explicit map restores the original audition instead of leaving a stale draft active.
    setParam(t.get(),"seq_gen_begin","1");setParam(t.get(),"seq_gen_edit","1,2");
    setParam(t.get(),"seq_gen_edit","2,3");assert(!t->sequenceGenerateValid);
    assert(sameLane(t->sequencePlayback.tracks[0],applied));
    setParam(t.get(),"seq_gen_cancel","1");assert(held.held&&held.age==age);
    // A scene fade must copy the authored sequence, never a temporary playback overlay.
    t->patternFadeMs=200;setParam(t.get(),"seq_gen_begin","1");setParam(t.get(),"seq_gen_edit","2,5");
    assert(t->beginPatternFade(1)&&!t->sequenceGenerateActive);
    assert(sameLane(t->patternShadow->sequencePlayback.tracks[0],applied));
    std::puts("Sequence Generate live: empty-clip audible draft, immediate Play, field-resolved voices, next-trig edits, phase-preserving Cancel/Apply, 5ms preview release, other-part/mix edits, invalid-map fallback and fade handoff: PASS");
}
void testSequenceGeneratePersistence() {
    using namespace s3g_thru_generate;
    char directory[]="/tmp/thru-generate.XXXXXX";assert(mkdtemp(directory));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(directory));t->stopBackgroundWork();t->setPerformanceMode(6);
    t->setSequenceParameter("seq_length","19");setParam(t.get(),"seq_gen_begin","1");setParam(t.get(),"seq_gen_edit","1,7");
    setParam(t.get(),"seq_gen_edit","12,4");setParam(t.get(),"seq_gen_edit","14,3");setParam(t.get(),"seq_gen_apply","1");
    const auto generated=t->sequenceLive.tracks[0];
    setParam(t.get(),"seq_gen_begin","1");setParam(t.get(),"seq_gen_edit","1,2");
    assert(!sameLane(t->sequencePlayback.tracks[0],generated));
    assert(t->savePersistentState());t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(directory));t->stopBackgroundWork();
    assert(t->sequenceMode()&&sameLane(t->sequenceLive.tracks[0],generated)&&!t->sequenceGenerateActive);
    t.reset();removeBackgroundTestDirectory(directory);
    std::puts("Sequence Generate persistence: ordinary clip-format-43 trigs/locks, reopen and inactive ephemeral preview: PASS");
}
void testSequenceGenerate(){testSequenceGenerateMath();testSequenceGenerateTransaction();testSequenceGenerateLive();testSequenceGeneratePersistence();}
