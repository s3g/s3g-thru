void testQuadGeometryAndAudio()
{
    auto t=splitFixture();t->setPerformanceMode(3);
    assert(t->isQuad()&&t->partCount()==4&&t->voicesPerPart()==2);
    for(int p=0;p<4;++p) {
        const int pad=t->padForPart(p,0);
        assert(t->partForInput(pad)==p&&t->localPad(pad)==0);
        assert(t->partForInput(t->padForPart(p,7))==p&&t->localPad(t->padForPart(p,7))==7);
        t->selectSplitPart(p);t->controls.speed=.6f+p*.3f;t->controls.ampReleaseMs=1000;
        t->rootNote=60+p;t->padNoteOn(68+pad,100);
        assert(t->selectedLane==static_cast<std::size_t>(p));
    }
    renderSplit(*t,50);
    for(int p=0;p<4;++p) {
        const auto& v=t->voices[p*2];
        assert(v.part==p&&v.held&&v.engine.loadedLaneMask()==(1u<<p));
        const auto c=t->effectiveControls(v);
        assert(c.stackFirstLane==p&&c.stackLastLane==p&&c.sampleLane==p);
        assert(t->settingsForPart(p).root==60+p);
        assert(t->padToMidiNote(68+t->padForPart(p,0))==48+p);
    }
    const auto dAge=t->voices[6].age;
    for(int i=1;i<8;++i)t->padNoteOn(68+t->padForPart(0,i),100);
    assert(t->voices[6].age==dAge&&t->voices[6].held);
    int held=0;for(const auto& v:t->voices)if(v.part==0&&v.held)++held;assert(held==2);
    t->selectSplitPart(2);t->copySplitPart(2);t->selectSplitPart(3);
    assert(t->rootNote==62&&t->controls.speed==1.2f);
    for(int p=0;p<4;++p)for(std::size_t alg=0;alg<Engine::kPlaybackAlgorithmCount;++alg) {
        t->selectSplitPart(p);t->controls.algorithm=static_cast<Engine::PlaybackAlgorithm>(alg);
        renderSplit(*t,4);assert(t->voices[p*2].engine.loadedLaneMask()==(1u<<p));
    }
    t->setPerformanceMode(0);t->setSplitMode(true);assert(t->performanceMode()==3);
    t->setPerformanceMode(2);assert(t->voices[4].engine.loadedLaneMask()==15u);
    t->setPerformanceMode(1);assert(t->voices[4].engine.loadedLaneMask()==12u);
    t->setPerformanceMode(3);assert(t->voices[6].engine.loadedLaneMask()==8u);
}

void testQuadMixerAndMidi()
{
    auto t=splitFixture();t->setPerformanceMode(3);
    std::array<Engine::Controls,4> live;
    for(auto& c:live){c=t->controls;c.effectTone=0;c.effectDrive=0;c.effectRate=1;c.effectBits=0;c.effectCrossPan=-1;c.effectRotaryMix=0;}
    t->controls.splitBlend=0; // A/B Blend must not mute the Quad right-hand zones.
    const auto feed=[&](){for(int p=0;p<4;++p)for(auto& channel:t->splitBus[p])channel.fill(.1f*(p+1));t->processSplitBuses(kBlockFrames,live,t->controls);};
    for(int i=0;i<100;++i)feed();assert(std::abs(t->voiceMix[0][127]-.5f)<.001f);
    t->splitParts[3].settings.muted=1;
    for(int i=0;i<100;++i)feed();assert(std::abs(t->voiceMix[0][127]-.3f)<.001f);
    t->controls.splitDelayC=1;t->controls.splitReverbC=1;
    for(int i=0;i<100;++i)feed();assert(std::abs(t->splitDelayInput[0][127]-.15f)<.001f);
    assert(std::abs(t->splitReverbInput[0][127]-.15f)<.001f);
    t->splitParts[3].settings.muted=0;
    for(int p=0;p<4;++p) {
        const std::uint8_t on[]{static_cast<std::uint8_t>(0x90+p),60,100};
        onMidi(t.get(),on,3,MOVE_MIDI_SOURCE_EXTERNAL);
        assert(t->voices[p*2].held&&t->voices[p*2].note==60);
    }
    const std::uint8_t cc[]{0xb2,13,32};onMidi(t.get(),cc,3,MOVE_MIDI_SOURCE_EXTERNAL);
    assert(std::abs(t->controls.splitLevelC-32.0f/127*2)<.00001f&&t->controls.splitLevelA==1);
    const std::uint8_t off[]{0x82,60,0};onMidi(t.get(),off,3,MOVE_MIDI_SOURCE_EXTERNAL);
    assert(!t->voices[4].held&&t->voices[0].held&&t->voices[6].held);
}

void testQuadRollsAndPublication()
{
    auto t=splitFixture();t->setPerformanceMode(3);
    for(int p=0;p<4;++p){t->padNoteOn(68+t->padForPart(p,0),100);t->padNoteOn(68+t->padForPart(p,1),100);}
    renderSplit(*t,90);
    for(int p=2;p<4;++p) {
        t->selectSplitPart(p);t->padMode=PadMode::rolls;
        t->padNoteOn(68+t->padForPart(p,2),100);
        assert(t->rolls[p*2].active&&t->rolls[p*2].part==p&&t->rolls[p*2].capturedSourceCount==2);
    }
    const auto a=t->voices[0].age,d=t->voices[6].age;
    setParam(t.get(),"record_lane","3");assert(t->splitFocus==2&&t->selectedLane==2u);
    t->sampleRequestLane=2;t->sampleRequestClip=t->activeClip;t->sampleRequestPreview=false;
    t->sampleStageFrames=4096;t->sampleStageRate=t->sampleRate;
    std::fill_n(t->sampleStage.get(),4096,.1f);std::fill_n(t->sampleStage.get()+t->laneCapacityFrames,4096,-.05f);
    t->sampleLoadState.store(static_cast<int>(SampleLoadState::ready));
    for(int i=0;i<300&&t->sampleLoadState.load()!=static_cast<int>(SampleLoadState::complete);++i)renderSplit(*t,1);
    assert(t->sampleLoadState.load()==static_cast<int>(SampleLoadState::complete));renderSplit(*t,20);
    assert(t->voices[4].engine.laneFrames(2)==4096&&t->voices[5].engine.laneFrames(2)==4096);
    assert(!t->voices[0].engine.laneLoaded(2)&&!t->voices[6].engine.laneLoaded(2));
    assert(t->voices[0].held&&t->voices[0].age==a&&t->voices[6].held&&t->voices[6].age==d);
}

void testQuadRecordingAndRecall()
{
    char path[]="/tmp/thru-quad-bank.XXXXXX";assert(mkdtemp(path));
    auto t=splitFixture();t->setPerformanceMode(3);
    for(int p=0;p<4;++p){t->selectSplitPart(p);t->rootNote=60+p;t->controls.speed=.5f+p*.2f;t->padNoteOn(68+t->padForPart(p,0),100);}
    t->beginClipRecording();renderSplit(*t,20);
    for(int p=0;p<4;++p){t->selectSplitPart(p);t->controls.locus=.2f+p*.1f;t->captureClipParameter(static_cast<int>(LockParameter::locus));t->setStackScanPaused(true);}
    setParam(t.get(),"split_delay_d","0.7");t->captureClipParameter(static_cast<int>(LockParameter::splitDelayD));
    renderSplit(*t,30);for(int p=0;p<4;++p)t->padNoteOff(68+t->padForPart(p,0));renderSplit(*t,100);t->finishClipRecording();
    auto& clip=*t->activeClipSlot();assert(clip.quadMode&&clip.clipNoteCount==4);
    bool parts[4]{};for(int i=0;i<4;++i)parts[clip.clipNotes[i].part-1]=true;for(bool p:parts)assert(p);
    bool c=false,d=false;for(int i=0;i<clip.clipPointCount;++i){const auto& point=clip.clipPoints[i];c|=point.type==ClipPointType::partParameter&&point.sourcePad==2;d|=point.type==ClipPointType::partParameter&&point.sourcePad==3;}assert(c&&d);
    t->applyClipCurves(clip,clip.clipDurationMs*.8);
    assert(t->splitClipControls[2].locus!=t->splitClipControls[3].locus);
    for(int i=0;i<4;++i)t->triggerClipVoice(clip,i);
    for(int p=0;p<4;++p)assert(t->voices[p*2].held&&t->voices[p*2].part==p);
    t->captureActiveClip(true);
    // C/D events remain stored when switching to two instruments, but cannot
    // allocate nonexistent part slots. They play again after returning to Quad.
    t->setPerformanceMode(1);
    for(int i=0;i<4;++i)t->triggerClipVoice(clip,i);
    for(const auto& voice:t->voices)if(voice.held)assert(voice.part<2);
    t->setPerformanceMode(3);t->selectSplitPart(3);
    t->captureActiveClip(true);
    auto text=std::make_unique<std::array<char,kStateTextSize>>();assert(t->serializeState(text->data(),text->size())>0);
    t->setPerformanceMode(1);assert(t->restoreState(text->data())&&t->isQuad()&&t->splitFocus==3);
    assert(t->rootNote==63&&t->controls.speed==1.1f&&t->controls.splitDelayD==.7f);
    // Exercise the full bank extension and checksum, including C/D note ownership.
    std::snprintf(t->moduleDirectory.data(),t->moduleDirectory.size(),"%s",path);
    t->persistenceEnabled=true;assert(t->savePersistentState());t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(path));t->stopBackgroundWork();
    assert(t->persistenceRestored&&t->isQuad()&&t->rootNote==63);
    assert(t->activeClipSlot()->clipNoteCount==4&&t->controls.splitDelayD==.7f);
    for(int p=0;p<4;++p)assert(t->voices[p*2].engine.loadedLaneMask()==(1u<<p));
    t->selectSplitPart(2);t->padMode=PadMode::cues;t->storeCue(4,.3f);t->triggerCue(4);assert(t->voices[4].held&&t->voices[4].sourcePad==0);
    t->padNoteOff(68); // C's first physical pad releases its own cue voice.
    t.reset();removeBackgroundTestDirectory(path);
}
