// Exercise the real mixer, allocation, recording and state paths together.
std::unique_ptr<ThruInstance> splitFixture()
{
    auto t=std::make_unique<ThruInstance>();
    assert(t->prepare(nullptr));
    for(std::size_t lane=0;lane<4;++lane) {
        for(std::size_t channel=0;channel<2;++channel)
            for(std::size_t f=0;f<8192;++f)
                t->engine.writableLane(lane,channel)[f]=.16f*std::sin(
                    6.28318530718f*(110.0f+lane*70.0f)*f/t->sampleRate)*(channel? .7f:1.0f);
        assert(t->engine.adoptLane(lane,8192,t->sampleRate));
    }
    t->controls.algorithm=Engine::PlaybackAlgorithm::sample;
    t->controls.sampleMode=Engine::SampleMode::loop;
    t->setSplitMode(true);
    return t;
}
void renderSplit(ThruInstance& t,int blocks)
{
    std::array<std::int16_t,kBlockFrames*2> out{};
    for(int i=0;i<blocks;++i)t.render(out.data(),kBlockFrames);
}
void testSplitInstruments()
{
    auto t=splitFixture();
    t->controls.speed=.7f;t->controls.manualLane=1;
    t->selectSplitPart(1);t->controls.speed=1.4f;t->rootNote=67;t->padScale=PadScale::major;
    t->laneVariation=LaneVariation::cycle;t->holdReleaseMs=1800;
    t->selectSplitPart(0);
    assert(t->controls.speed==.7f&&t->rootNote==60&&t->holdReleaseMs==0);
    assert(t->padToMidiNote(68)==48&&t->padToMidiNote(72)==55);
    t->padNoteOn(68,100);t->padNoteOn(72,100);renderSplit(*t,40);
    auto& a=t->voices[0];auto& b=t->voices[4];
    assert(a.held&&b.held&&a.part==0&&b.part==1);
    assert(a.engine.loadedLaneMask()==3u&&b.engine.loadedLaneMask()==12u);
    assert(a.variantLane==1&&b.variantLane==2);
    const auto bAge=b.age;const auto bPosition=b.engine.normalizedPosition();
    for(int pad: {1,2,3,8,9,10})t->padNoteOn(68+pad,100);
    assert(b.held&&b.age==bAge);renderSplit(*t,12);
    assert(b.engine.normalizedPosition()!=bPosition);
    int heldA=0;for(auto& v:t->voices)if(v.held&&v.part==0)++heldA;
    assert(heldA==4);
    t->selectSplitPart(1);
    assert(t->controls.speed==1.4f&&t->holdReleaseMs==1800&&t->laneVariation==LaneVariation::cycle);
    assert(t->effectiveControls(a).speed< t->effectiveControls(b).speed);
    for(std::size_t alg=0;alg<Engine::kPlaybackAlgorithmCount;++alg) {
        t->controls.algorithm=static_cast<Engine::PlaybackAlgorithm>(alg);
        renderSplit(*t,4);
        assert(a.engine.loadedLaneMask()==3u&&b.engine.loadedLaneMask()==12u);
        assert(t->effectiveControls(b).algorithm==static_cast<Engine::PlaybackAlgorithm>(alg));
        for(int lane=0;lane<2;++lane)assert(b.engine.laneWeight(lane)==0);
    }
    t->copySplitPart(1);t->selectSplitPart(0);
    assert(t->controls.speed==1.4f&&t->rootNote==67&&t->holdReleaseMs==1800);
    assert(t->engine.loadedLaneMask()==15u); // copying settings never copies/replaces audio
    t->setSplitMode(false);
    assert(t->controls.algorithm==Engine::PlaybackAlgorithm::sample);
    assert(t->rootNote==60&&t->controls.speed==1);
    t->setSplitMode(true);assert(t->controls.speed==1.4f&&t->rootNote==67);
}
void testSplitCuesRollsAndLoad()
{
    auto t=splitFixture();
    t->padNoteOn(68,100);renderSplit(*t,70);
    t->selectSplitPart(1);t->padMode=PadMode::cues;
    t->storeCue(4,.4f);t->triggerCue(4);
    assert(t->voices[0].held&&t->voices[4].held);
    t->padNoteOff(72);assert(t->voices[0].held&&!t->voices[4].held);
    t->padMode=PadMode::keys;t->padNoteOn(72,100);renderSplit(*t,60);
    t->selectSplitPart(0);t->padMode=PadMode::rolls;
    t->padNoteOn(69,100);assert(t->rolls[0].active&&t->rolls[0].part==0);
    t->selectSplitPart(1);t->padMode=PadMode::rolls;
    t->padNoteOn(73,100);assert(t->rolls[4].active&&t->rolls[4].part==1);
    for(int pad: {5,6,7,12,13,14})t->padNoteOn(68+pad,100);
    assert(t->rolls[0].held); // B roll stealing cannot take A's captures
    const auto age=t->voices[0].age;
    // Simulate a decoded B file through the exact bounded publication path.
    t->sampleRequestLane=2;t->sampleRequestClip=t->activeClip;t->sampleRequestPreview=false;
    t->sampleStageFrames=4096;t->sampleStageRate=t->sampleRate;
    std::fill_n(t->sampleStage.get(),4096,.1f);
    std::fill_n(t->sampleStage.get()+t->laneCapacityFrames,4096,-.05f);
    t->sampleLoadState.store(static_cast<int>(SampleLoadState::ready));
    for(int block=0;block<300&&t->sampleLoadState.load()!=static_cast<int>(SampleLoadState::complete);++block)renderSplit(*t,1);
    assert(t->sampleLoadState.load()==static_cast<int>(SampleLoadState::complete));
    assert(t->voices[0].held&&t->voices[0].age==age);
    assert(t->engine.laneFrames(2)==4096);
    t->padNoteOff(69);assert(!t->rolls[0].held);
}
void testSplitMixer()
{
    auto t=splitFixture();
    t->controls.effectDrive=0;t->controls.effectTone=0;
    t->controls.effectBits=0;t->controls.effectRate=1;
    t->controls.effectCrossPan=-1;t->controls.effectRotaryMix=0;
    t->controls.splitBlend=0;t->controls.splitDelayA=1;t->controls.splitDelayB=0;
    std::array<Engine::Controls,2> live{t->controls,t->controls};
    auto feed=[&]() {
        for(auto& ch:t->splitBus[0])ch.fill(.2f);
        for(auto& ch:t->splitBus[1])ch.fill(.6f);
        t->processSplitBuses(kBlockFrames,live,t->controls);
    };
    for(int i=0;i<180;++i)feed();
    assert(std::abs(t->voiceMix[0][127]-.2f)<.001f);
    assert(std::abs(t->splitDelayInput[0][127]-.2f)<.001f);
    t->controls.splitBlend=1;float previous=t->voiceMix[0][127];float maximum=0;
    for(int i=0;i<180;++i){feed();for(float value:t->voiceMix[0]){maximum=std::max(maximum,std::abs(value-previous));previous=value;}}
    assert(maximum<.002f&&std::abs(previous-.6f)<.001f);
    assert(std::abs(t->splitDelayInput[0][127])<.00001f);
    t->controls.splitBlend=.5f;t->controls.splitCurve=0;
    for(int i=0;i<180;++i)feed();assert(std::abs(t->voiceMix[0][127]-.4f)<.001f);
    t->controls.splitCurve=1;
    for(int i=0;i<180;++i)feed();assert(std::abs(t->voiceMix[0][127]-.8f/std::sqrt(2.0f))<.001f);
    t->splitParts[1].settings.muted=1;
    for(int i=0;i<180;++i)feed();assert(std::abs(t->voiceMix[0][127]-.2f/std::sqrt(2.0f))<.001f);
}
void testSplitRecordingAndState()
{
    auto t=splitFixture();
    t->controls.speed=.65f;t->selectSplitPart(1);t->controls.speed=1.35f;t->rootNote=65;
    t->selectSplitPart(0);t->padNoteOn(68,100);t->padNoteOn(72,100);
    t->beginClipRecording();renderSplit(*t,70);
    assert(t->activeClipSlot()->clipNoteCount==2);
    t->controls.speed=.8f;t->captureClipParameter(static_cast<std::size_t>(LockParameter::speed));
    t->selectSplitPart(1);t->controls.speed=1.6f;t->captureClipParameter(static_cast<std::size_t>(LockParameter::speed));
    t->setStackScanPaused(true);t->controls.splitBlend=.75f;
    t->captureClipParameter(static_cast<std::size_t>(LockParameter::splitBlend));
    t->setSplitMode(false);assert(t->splitEnabled); // an ongoing take cannot change topology
    renderSplit(*t,70);t->padNoteOff(68);t->padNoteOff(72);t->finishClipRecording();
    auto& clip=*t->activeClipSlot();
    assert(clip.splitEnabled&&clip.clipNotes[0].part==1&&clip.clipNotes[1].part==2);
    bool automationA=false,automationB=false,blend=false,hold=false;
    for(std::size_t i=0;i<clip.clipPointCount;++i){const auto& p=clip.clipPoints[i];
        if(p.type==ClipPointType::partParameter&&p.parameter==static_cast<unsigned>(LockParameter::speed))
            (p.sourcePad==0?automationA:automationB)=true;
        blend|=p.type==ClipPointType::parameter&&p.parameter==static_cast<unsigned>(LockParameter::splitBlend);
        hold|=p.type==ClipPointType::partHold&&p.sourcePad==1&&p.value==1;
    }
    assert(automationA&&automationB&&blend&&hold);
    t->applyClipCurves(clip,clip.clipDurationMs*.8);
    assert(t->splitClipControls[0].speed!=t->splitClipControls[1].speed);
    t->triggerClipVoice(clip,0);t->triggerClipVoice(clip,1);
    bool a=false,b=false;for(const auto& v:t->voices)if(v.held&&v.clipOwned)(v.part?b:a)=true;
    assert(a&&b);
    // Two external channels with the same pitch have separate note-off keys.
    t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();
    t->midiPart=0;t->noteOn(60,100);t->midiPart=1;t->noteOn(60,100);
    renderSplit(*t,10);t->midiPart=0;t->noteOff(60);
    assert(t->findHeldVoiceForNote(60,-1)==nullptr);t->midiPart=1;
    assert(t->findHeldVoiceForNote(60,-1)!=nullptr);t->noteOff(60);renderSplit(*t,90);t->finishClipRecording();
    t->captureActiveClip(true);
    auto text=std::make_unique<std::array<char,kStateTextSize>>();
    assert(t->serializeState(text->data(),text->size())>0);
    const auto oldSpeed=t->controls.speed;t->setSplitMode(false);assert(!t->splitEnabled);
    assert(t->restoreState(text->data())&&t->splitEnabled&&t->controls.speed==oldSpeed);
    assert(t->voices[0].engine.loadedLaneMask()==3&&t->voices[4].engine.loadedLaneMask()==12);
    // Save both instruments, including private keyboard/lane settings, in the bank extension.
    assert(t->settingsForPart(1).root==65);t->captureActiveClip(true);
    auto file=std::tmpfile();assert(file);
    assert(ThruInstance::writeSplitExtension(file,t->clipSlots));std::rewind(file);
    auto clips=std::make_unique<std::array<ClipSlot,kClipSlotCount>>();
    assert(ThruInstance::readSplitExtension(file,*clips));std::fclose(file);
    const auto& saved=(*clips)[t->activeClip];assert(saved.splitEnabled&&saved.splitSettings[1].root==65);
    assert(saved.splitValues[0][static_cast<std::size_t>(LockParameter::speed)]!=saved.splitValues[1][static_cast<std::size_t>(LockParameter::speed)]);
    t->clipRecordMode=ClipRecordMode::overdub;t->beginClipRecording();t->padNoteOn(73,100);
    renderSplit(*t,100);t->padNoteOff(73);t->finishClipRecording();assert(t->activeClipSlot()->takeCount==2);
    t->undoLastClipTake();assert(t->activeClipSlot()->takeCount==1);
}
void testSplitBankRoundTrip()
{
    char path[]="/tmp/thru-split-bank.XXXXXX";assert(mkdtemp(path));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(path));t->stopBackgroundWork();
    t->setSplitMode(true);t->controls.algorithm=Engine::PlaybackAlgorithm::cutups;
    t->controls.cutLaneAdvance=Engine::CutLaneAdvance::eightCuts;
    t->selectSplitPart(1);t->controls.algorithm=Engine::PlaybackAlgorithm::longGrains;
    t->controls.longGrainLengthMs=4000;t->rootNote=67;t->laneVariation=LaneVariation::noRepeat;
    t->beginClipRecording();t->midiPart=0;t->noteOn(60,100);t->midiPart=1;t->noteOn(60,100);
    renderSplit(*t,40);t->controls.speed=.75f;t->captureClipParameter(static_cast<int>(LockParameter::speed));
    t->midiPart=0;t->noteOff(60);t->midiPart=1;t->noteOff(60);renderSplit(*t,100);t->finishClipRecording();
    const auto noteCount=t->activeClipSlot()->clipNoteCount;
    assert(t->savePersistentState());t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(path));t->stopBackgroundWork();
    assert(t->persistenceRestored&&t->splitEnabled&&t->splitFocus==1);
    assert(t->controls.algorithm==Engine::PlaybackAlgorithm::longGrains&&t->controls.longGrainLengthMs==4000);
    assert(t->rootNote==67&&t->laneVariation==LaneVariation::noRepeat);
    assert(t->activeClipSlot()->clipNoteCount==noteCount);
    bool a=false,b=false;for(std::size_t i=0;i<noteCount;++i){auto part=t->activeClipSlot()->clipNotes[i].part;a|=part==1;b|=part==2;}
    assert(a&&b);
    t->selectSplitPart(0);assert(t->controls.algorithm==Engine::PlaybackAlgorithm::cutups);
    assert(t->controls.cutLaneAdvance==Engine::CutLaneAdvance::eightCuts);
    assert(t->laneVariation==LaneVariation::manual);
    t.reset();removeBackgroundTestDirectory(path);
}
