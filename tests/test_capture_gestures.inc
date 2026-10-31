void testCaptureGestureOwnership()
{
    auto t=splitFixture();
    t->padNoteOn(68,100);t->padNoteOn(76,100);renderSplit(*t,20);
    t->beginPerformanceCapture();
    t->padNoteOff(68);
    assert(t->padMappedNotes[0]<0); // Held-before-Capture note keeps its release.
    const int held=t->heldVoiceCount();
    t->padNoteOn(69,1);assert(t->heldVoiceCount()==held);
    assert(t->performanceLive[0][1]==.75f);
    assert(t->performancePadPressure(1,0));renderSplit(*t,100);
    assert(std::abs(t->performanceSmooth[0][1]-.2f)<.001f);
    bool found=false;
    for(const auto& voice:t->voices)if(voice.active&&voice.held&&voice.part==0) {
        assert(t->effectiveControls(voice).performanceLaneAmount>.99f);
        assert(std::abs(voice.engine.normalizedLanePosition()*3-1)<.01f);found=true;
    }
    assert(found);
    t->cancelPerformanceGestures();assert(t->performancePadOwners[1]==-2);
    t->padNoteOff(69);assert(t->heldVoiceCount()==held);
    renderSplit(*t,100);assert(t->performanceSmooth[0][1]<.001f);
    t->beginPerformanceCapture();t->padNoteOn(70,100); // A cannot ride B's lane.
    assert(t->performancePadOwners[2]==-2&&t->heldVoiceCount()==held);
    t->padNoteOff(70);t->cancelPerformanceGestures();
    t->selectSplitPart(1);t->beginPerformanceCapture();t->padNoteOn(75,100);
    assert(t->performanceLive[1][7]>.3f&&t->performanceLive[0][7]==0);
    t->selectSplitPart(0);assert(!t->performanceCaptureHeld&&t->performanceLive[1][7]==0);
    t->padNoteOff(75);
    t->setPerformanceMode(3);t->selectSplitPart(3);t->beginPerformanceCapture();
    for(int pad=0;pad<4;++pad){t->padNoteOn(68+pad,100);assert(t->performanceLive[3][pad]==0);t->padNoteOff(68+pad);}
    t->padNoteOn(72,100);assert(t->performanceLive[3][4]>.3f);
    t->allNotesOff();assert(!t->performanceCaptureHeld&&t->performanceLive[3][4]==0);
}

void testCaptureGestureCurvesAndCues()
{
    auto t=splitFixture();t->setPerformanceMode(2);
    t->padNoteOn(84,100);renderSplit(*t,20);
    t->beginPerformanceCapture();t->padNoteOn(71,100);t->performancePadPressure(3,127);
    renderSplit(*t,100);
    t->beginClipRecording();assert(t->clipRecording);
    auto* clip=t->activeClipSlot();assert(clip);
    bool initial=false;
    for(std::size_t i=0;i<clip->clipPointCount;++i) {
        const auto& point=clip->clipPoints[i];
        if(point.type==ClipPointType::performance&&(point.parameter&255u)==3u&&point.value==1)initial=true;
    }
    assert(initial);
    renderSplit(*t,20);t->padNoteOff(71);renderSplit(*t,20);t->finishClipRecording();
    assert(clip->clipRecorded&&clip->clipDurationMs>0);
    t->applyClipCurves(*clip,1);assert(t->performanceRecorded[0][3]==1);
    t->applyClipCurves(*clip,clip->clipDurationMs-1);assert(t->performanceRecorded[0][3]==0);
    t->clipPlaying=false;t->cancelPerformanceGestures();renderSplit(*t,100);
    t->controls.manualLane=2;t->controls.algorithm=Engine::PlaybackAlgorithm::sample;
    t->padNoteOn(85,100);renderSplit(*t,30);t->storeCue(0,.3f);
    assert(std::abs(clip->cueStackPositions[0]-2)<.01f);
    t->controls.manualLane=0;t->triggerCue(0);renderSplit(*t,30);
    assert(t->stackScanPaused&&t->performanceLive[0][8]>0);
    for(const auto& voice:t->voices)if(voice.active&&voice.part==0)
        assert(t->effectiveControls(voice).performanceLaneFixed==2);
    t->setStackScanPaused(false);assert(t->performanceLive[0][8]==0);
    t->clearCue(0);assert(clip->cueStackPositions[0]<0);
    Engine::Controls baseline=t->controls;
    t->performanceSmooth[0][4]=1;t->performanceSmooth[0][5]=1;
    t->applyPerformanceInsert(baseline,0);
    assert(baseline.effectTone==-1&&baseline.effectBits==.92f&&baseline.effectRate==12);
    assert(t->controls.effectTone==0&&t->controls.effectBits==0&&t->controls.effectRate==1);
    baseline=t->controls;t->performanceSmooth[1][6]=1;t->applyPerformanceSends(baseline);
    assert(baseline.splitDelayB==1&&baseline.splitDelayA==t->controls.splitDelayA);
}

void testPartCaptureAndQuadRollOwnership()
{
    for(int mode:{1,2,3}) {
        auto t=splitFixture();t->setPerformanceMode(mode);
        const int source=mode==3?3:0;
        t->selectSplitPart(source);
        t->padNoteOn(68+t->padForPart(source,0),100);renderSplit(*t,30);
        setParam(t.get(),"part_mute","1");
        t->controls.splitBlend=1;t->controls.splitLevelA=0;
        t->selectedLane=mode==2?0:mode==3?0:2;
        t->recordSource=source+2;
        const auto oldFrames=t->engine.laneFrames(t->selectedLane);
        const float oldSample=t->engine.laneData(t->selectedLane,0)[100];
        t->queuedClip=1;t->startRecording();assert(!t->audioRecording());t->queuedClip=-1;
        t->startRecording();assert(t->partCaptureRecording&&t->audioRecording());
        t->restoreUndo();assert(t->partCaptureRecording&&t->undoAvailable);
        const int oldMode=t->performanceMode();t->setPerformanceMode(0);assert(t->performanceMode()==oldMode);
        setParam(t.get(),"record_source","0");assert(t->recordSource==source+2);
        renderSplit(*t,32);
        assert(t->engine.laneFrames(t->selectedLane)==oldFrames);
        assert(t->engine.laneData(t->selectedLane,0)[100]==oldSample);
        t->finishRecording();assert(!t->audioRecording()&&t->partCapturePublishing);
        t->applyMidiCc(49,0);assert(t->recordSource==source+2);
        t->restoreUndo();assert(t->partCapturePublishing);
        const auto frames=t->partRecorder.laneFrames(0);assert(frames==32*kBlockFrames);
        double energy=0;for(std::size_t f=0;f<frames;++f)energy+=std::abs(t->partRecorder.laneData(0,0)[f]);
        assert(energy/frames>.005); // Captured before mute, level and Blend.
        renderSplit(*t,100);
        assert(!t->partCapturePublishing&&t->engine.laneFrames(t->selectedLane)==frames);
        assert(t->engine.laneData(t->selectedLane,0)[200]==t->partRecorder.laneData(0,0)[200]);
    }
    auto t=splitFixture();t->setPerformanceMode(3);
    for(int i=0;i<8;++i) {
        auto& roll=t->rolls[i];roll.active=true;roll.held=true;roll.part=i/2;
        roll.sourcePad=-1;roll.age=100+i;
    }
    for(int part=0;part<4;++part) {
        const auto* result=&t->allocateAudioRoll(t->padForPart(part,0),false,-1);
        assert(result==&t->rolls[part*2]);
    }
}

void testCapturePersistenceAndRawMidi()
{
    char directory[]="/tmp/thru-part-capture-XXXXXX";assert(mkdtemp(directory));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(directory));
    for(int lane=0;lane<4;++lane) {
        for(int channel=0;channel<2;++channel)for(int frame=0;frame<4096;++frame)
            t->engine.writableLane(lane,channel)[frame]=.1f*std::sin(frame*.03f)*(channel?.6f:1.0f);
        assert(t->engine.adoptLane(lane,4096,t->sampleRate));
    }
    t->controls.algorithm=Engine::PlaybackAlgorithm::sample;
    t->controls.sampleMode=Engine::SampleMode::loop;t->setPerformanceMode(2);
    const uint8_t capture[]{0xb0,52,127},dark[]{0x90,72,100},pressure[]{0xa0,72,127};
    onMidi(t.get(),capture,3,MOVE_MIDI_SOURCE_INTERNAL);onMidi(t.get(),dark,3,MOVE_MIDI_SOURCE_INTERNAL);
    onMidi(t.get(),pressure,3,MOVE_MIDI_SOURCE_INTERNAL);
    assert(t->performanceLive[0][4]==1&&t->heldVoiceCount()==0);
    t->beginClipRecording();renderSplit(*t,16);
    const uint8_t release[]{0xb0,52,0},off[]{0x80,72,0};
    onMidi(t.get(),release,3,MOVE_MIDI_SOURCE_INTERNAL);onMidi(t.get(),off,3,MOVE_MIDI_SOURCE_INTERNAL);
    renderSplit(*t,16);t->finishClipRecording();
    t->clipRecordMode=ClipRecordMode::overdub;t->beginClipRecording();
    t->beginPerformanceCapture();t->padNoteOn(73,100);renderSplit(*t,16);
    t->cancelPerformanceGestures();t->padNoteOff(73);t->finishClipRecording();
    assert(t->activeClipSlot()->takeCount==2);
    setParam(t.get(),"clip_take_undo","1");assert(t->activeClipSlot()->takeCount==1);
    t->applyClipCurves(*t->activeClipSlot(),1);assert(t->performanceRecorded[0][4]==1&&t->performanceRecorded[0][5]==0);
    t->clipPlaying=false;
    t->clipSlots[0].cues[0]=.25f;t->clipSlots[0].cueStackPositions[0]=1.5f;
    t->padNoteOn(84,100);renderSplit(*t,20);t->selectedLane=0;t->recordSource=2;
    t->startRecording();assert(t->partCaptureRecording);renderSplit(*t,16);t->finishRecording();
    assert(t->partCapturePublishing);t->stopSampleWorker();assert(t->savePersistentState());
    assert(!t->partCapturePublishing&&t->pendingCaptureSaveMask==0);
    const std::string capturePath=t->currentLaneSourcePaths[0].data();
    assert(!capturePath.empty());struct stat status{};assert(stat(capturePath.c_str(),&status)==0&&status.st_size>44);
    auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(directory));
    restored->applyClipCurves(restored->clipSlots[0],1);
    assert(restored->performanceRecorded[0][4]==1&&restored->performanceRecorded[0][5]==0);
    assert(restored->clipSlots[0].cueStackPositions[0]==1.5f);
    assert(restored->clipSlots[0].cues[0]==.25f);
    assert(std::strcmp(restored->clipSlots[0].laneSourcePaths[0].data(),capturePath.c_str())==0);
    restored.reset();t.reset();
    std::string command="rm -rf ";command+=directory;assert(std::system(command.c_str())==0);
}

void testLaneGrabAndCueManualControl()
{
    auto t=splitFixture();t->setPerformanceMode(2);
    t->beginPerformanceCapture();t->padNoteOn(68,100);t->padNoteOn(71,100);
    t->performancePadPressure(0,0);t->performancePadPressure(3,64);renderSplit(*t,180);
    auto effective=t->controls;t->applyPerformanceLane(effective,0);
    assert(effective.performanceLaneAmount==1);
    assert(std::abs(effective.performanceLanePosition-2.5f)<.001f);
    t->padNoteOff(68);renderSplit(*t,180);t->applyPerformanceLane(effective,0);
    assert(effective.performanceLaneAmount==1&&effective.performanceLanePosition==3);
    t->cancelPerformanceGestures();t->padNoteOff(71);renderSplit(*t,180);
    t->applyPerformanceLane(effective,0);assert(effective.performanceLaneAmount==0);

    for(int mode:{0,1,2}) {
        t->setPerformanceMode(mode);
        const int owner=mode==0?0:1;
        if(mode!=0)t->selectSplitPart(owner);
        const int slot=mode==0?0:4;
        const float original=mode==1?2.75f:1.75f;
        auto& clip=t->clipSlots[t->activeClip];clip.cues[slot]=.3f;clip.cueStackPositions[slot]=original;
        t->triggerCue(slot);renderSplit(*t,40);
        const float first=mode==1?2.0f:0.0f;
        assert(t->stackScanPaused&&std::abs(t->controls.manualLane-(original-first))<1e-5f);
        setParam(t.get(),"manual_lane","1.25");
        assert(t->stackScanPaused);
        assert(std::abs(t->performanceLive[owner][8]-(first+1.25f))<1e-5f);
        assert(clip.cueStackPositions[slot]==original);
        for(const auto& voice:t->voices)if(voice.active&&voice.part==owner)
            assert(std::abs(t->effectiveControls(voice).performanceLaneFixed-(first+.25f))<1e-5f);
        t->applyMidiCc(23,64);
        assert(std::abs(t->performanceLive[owner][8]-(first+t->controls.manualLane+1))<1e-5f);
        t->triggerCue(slot);assert(clip.cueStackPositions[slot]==original);
        assert(std::abs(t->controls.manualLane-(original-first))<1e-5f);
        t->setStackScanPaused(false);assert(t->performanceLive[owner][8]==0);
    }
    Engine::Controls baseline;
    baseline.effectBits=1;baseline.effectRate=16;baseline.effectMix=.95f;baseline.effectReverbMix=1;
    t->setPerformanceMode(0);for(int i=4;i<8;++i)t->performanceSmooth[0][i]=1;
    t->applyPerformanceSends(baseline);
    assert(baseline.effectBits==1&&baseline.effectRate==16&&baseline.effectMix==.95f&&baseline.effectReverbMix==1);
}

void testSustainedEventsFollowLaneGrab()
{
    struct Fixture {
        enum : std::size_t { frames=32768 };
        Engine engine;
        std::array<std::array<float,frames*2>,4> storage {};
        std::array<std::array<float,kBlockFrames>,4> inputs {},outputs {};
        std::array<const float*,4> in {};
        std::array<float*,4> out {};
        Fixture() {
            for(int channel=0;channel<4;++channel){in[channel]=inputs[channel].data();out[channel]=outputs[channel].data();}
            assert(engine.prepare(44100,storage[0].data(),storage[1].data(),storage[2].data(),storage[3].data(),frames*2));
            for(int lane=0;lane<2;++lane){storage[lane].fill(lane?-.2f:.2f);assert(engine.adoptLane(lane,frames,44100));}
        }
        float run(const Engine::Controls& c,int blocks) {
            float previous=outputs[0].back();double total=0;
            for(int block=0;block<blocks;++block) {
                engine.process(in.data(),out.data(),kBlockFrames,c);
                for(float sample:outputs[0]) {
                    assert(std::isfinite(sample));
                    assert(std::abs(sample-previous)<.025f);previous=sample;total+=sample;
                }
            }
            return static_cast<float>(total/(blocks*kBlockFrames));
        }
    };
    for(auto algorithm:{Engine::PlaybackAlgorithm::grains,Engine::PlaybackAlgorithm::longGrains,Engine::PlaybackAlgorithm::motion}) {
        auto f=std::make_unique<Fixture>();Engine::Controls c;
        c.algorithm=algorithm;c.lanePath=Engine::LanePath::manual;c.manualLane=0;
        c.grainSizeMs=500;c.grainDensityHz=4;c.longGrainLengthMs=4000;c.longGrainLayers=2;
        c.longGrainSpread=0;c.longGrainEdgeMs=20;c.motionSound=Engine::MotionSound::packets;
        assert(f->run(c,120)>.0001f);
        c.performanceLanePosition=1;c.performanceLaneAmount=1;
        assert(f->run(c,100)<-.0001f); // Existing four-second grains must follow now.
        c.performanceLaneAmount=0;
        assert(f->run(c,100)>.0001f); // Their natural lane survives the temporary grab.
        c.performanceLaneFixed=1;
        assert(f->run(c,100)<-.0001f); // Recalled cue blends affect existing events too.
        c.performanceLaneFixed=-1;c.manualLane=1;
        assert(f->run(c,100)<-.0001f); // Manual Stack Start retains control after cue hold ends.
        c.manualLane=0;assert(f->run(c,100)>.0001f);
    }
}

void testPerformancePunchStrength()
{
    auto t=splitFixture();t->setPerformanceMode(0);
    s3g_daisy::ThruPartEffects effects;assert(effects.prepare(44100));
    std::array<std::array<float,kBlockFrames>,2> bus{};
    std::size_t clock=0;
    const auto run=[&](const Engine::Controls& c,bool error) {
        double energy=0;std::size_t measured=0;
        for(int block=0;block<80;++block) {
            const auto start=clock;
            for(std::size_t frame=0;frame<kBlockFrames;++frame,++clock)
                for(auto& channel:bus)channel[frame]=.15f*std::sin(6.28318530718*6000*clock/44100);
            effects.process(kBlockFrames,bus,c);
            if(block<40)continue;
            for(std::size_t frame=0;frame<kBlockFrames;++frame) {
                assert(std::isfinite(bus[0][frame])&&std::abs(bus[0][frame])<.5f);
                const float clean=.15f*std::sin(6.28318530718*6000*(start+frame)/44100);
                const float sample=error?bus[0][frame]-clean:bus[0][frame];
                energy+=sample*sample;++measured;
            }
        }
        return std::sqrt(energy/measured);
    };
    Engine::Controls base;const auto clean=run(base,false);
    auto c=base;t->performanceSmooth[0][4]=.75f;t->applyPerformanceInsert(c,0);
    assert(run(c,false)<clean*.45);
    c=base;t->performanceSmooth[0][4]=1;t->applyPerformanceInsert(c,0);
    assert(run(c,false)<clean*.05);
    t->performanceSmooth[0][4]=0;t->performanceSmooth[0][5]=.75f;
    c=base;t->applyPerformanceInsert(c,0);assert(run(c,true)>.05);
}
