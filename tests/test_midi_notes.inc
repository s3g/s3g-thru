void noteMidi(ThruInstance& t,int channel,int note,int velocity=100,bool down=true,int source=MOVE_MIDI_SOURCE_EXTERNAL) {
    const std::uint8_t b[]{static_cast<std::uint8_t>((down?0x90:0x80)+channel-1),static_cast<std::uint8_t>(note),static_cast<std::uint8_t>(velocity)};
    onMidi(&t,b,3,source);
}
PlaybackVoice* externalVoice(ThruInstance& t,int part,int note,bool held=true) {
    for(auto& v:t.voices)if(v.active&&!v.clipOwned&&v.part==part&&v.note==note&&v.sourcePad<0&&(!held||v.held))return &v;
    return nullptr;
}
void realtimeMidi(ThruInstance& t,int status,int source=MOVE_MIDI_SOURCE_EXTERNAL) {
    const std::uint8_t b[]{static_cast<std::uint8_t>(status)};onMidi(&t,b,1,source);
}
void bendMidi(ThruInstance& t,int channel,int value) {
    const std::uint8_t b[]{static_cast<std::uint8_t>(0xe0+channel-1),static_cast<std::uint8_t>(value&127),static_cast<std::uint8_t>(value>>7)};onMidi(&t,b,3,MOVE_MIDI_SOURCE_EXTERNAL);
}
void channelPressureMidi(ThruInstance& t,int channel,int value) {
    const std::uint8_t b[]{static_cast<std::uint8_t>(0xd0+channel-1),static_cast<std::uint8_t>(value)};onMidi(&t,b,2,MOVE_MIDI_SOURCE_EXTERNAL);
}
void testStandardNoteRoutingAndSustain() {
    for(int mode=0;mode<4;++mode) {
        auto t=splitFixture();t->setPerformanceMode(mode);
        for(int control=0;control<7;++control) {
            setParam(t.get(),"midi_control_mode",std::to_string(control).c_str());
            for(int channel=1;channel<=4;++channel) {
                noteMidi(*t,channel,60+channel);
                assert((externalVoice(*t,channel-1,60+channel)!=nullptr)==(channel<=(t->splitEnabled?t->partCount():1)));
                noteMidi(*t,channel,60+channel,0);assert(!externalVoice(*t,channel-1,60+channel));
            }
            noteMidi(*t,10,67);assert(!externalVoice(*t,0,67));
        }
    }
    auto t=splitFixture();t->setPerformanceMode(2);setParam(t.get(),"midi_note_route","Focus");
    t->selectSplitPart(0,false);noteMidi(*t,1,60);midiCc(*t,1,64,127);noteMidi(*t,1,60,0);
    assert(externalVoice(*t,0,60));t->selectSplitPart(1,false);noteMidi(*t,1,60);
    assert(externalVoice(*t,0,60)&&externalVoice(*t,1,60));
    midiCc(*t,1,64,0);assert(!externalVoice(*t,0,60)&&externalVoice(*t,1,60));
    t->selectSplitPart(0,false);noteMidi(*t,1,60,0);assert(!externalVoice(*t,1,60));
    noteMidi(*t,1,64);noteMidi(*t,1,64);noteMidi(*t,1,64,0);assert(!externalVoice(*t,0,64));
    noteMidi(*t,1,64,0);assert(!externalVoice(*t,0,64));
    noteMidi(*t,1,65);noteMidi(*t,1,65,100,true,MOVE_MIDI_SOURCE_HOST);
    noteMidi(*t,1,65,0);assert(externalVoice(*t,0,65));
    noteMidi(*t,1,65,0,false,MOVE_MIDI_SOURCE_HOST);assert(!externalVoice(*t,0,65));
    t->shiftHeld=true;t->padNoteOn(68,100);t->padNoteOff(68);t->shiftHeld=false;
    assert(t->latchedPadNotes[0]>=0);noteMidi(*t,1,70);
    for(int channel=5;channel<=16;++channel)for(int cc:{120,123}){midiCc(*t,channel,cc,0);assert(externalVoice(*t,0,70)&&t->latchedPadNotes[0]>=0);}
    midiCc(*t,1,120,0);
    assert(!externalVoice(*t,0,70)&&t->latchedPadNotes[0]>=0);
    t->allNotesOff();renderSplit(*t,30);setParam(t.get(),"amp_release","3000");
    noteMidi(*t,1,61);renderSplit(*t,8);auto* tail=externalVoice(*t,0,61);assert(tail);
    noteMidi(*t,1,61,0);midiCc(*t,1,120,0);assert(tail->panicRelease&&tail->envelopeStage==AmpEnvelopeStage::release);
    renderSplit(*t,10);assert(!tail->active);
    t->allNotesOff();renderSplit(*t,30);setParam(t.get(),"voice_mode","2");
    noteMidi(*t,1,60);renderSplit(*t,10);auto* v=externalVoice(*t,0,60);assert(v);
    const auto position=v->engine.normalizedPosition();noteMidi(*t,1,67);assert(v->note==67&&v->engine.normalizedPosition()==position);
    noteMidi(*t,1,67,0);assert(v->held&&v->note==60);noteMidi(*t,1,60,0);assert(!v->held);
    noteMidi(*t,1,60);midiCc(*t,1,64,127);noteMidi(*t,1,60,0);noteMidi(*t,1,67);noteMidi(*t,1,67,0);
    midiCc(*t,1,64,0);assert(std::none_of(t->voices.begin(),t->voices.end(),[](const auto& voice){return voice.held&&voice.sourcePad<0;}));
    noteMidi(*t,1,60);setParam(t.get(),"midi_note_channel","2");assert(!externalVoice(*t,0,60));
    noteMidi(*t,1,61);assert(!externalVoice(*t,0,61));noteMidi(*t,2,62);assert(externalVoice(*t,0,62));
    t->setPerformanceMode(3);noteMidi(*t,2,64);assert(externalVoice(*t,0,64));
    t->setPerformanceMode(1);noteMidi(*t,2,64,0);assert(!externalVoice(*t,0,64));
    std::puts("s3g Thru MIDI Notes: all layouts/protocols, strict channels, Focus key ownership, sustain, repeats, sources, Legato and pad-latch isolation: PASS");
}
void testStandardNoteExpression() {
    auto t=splitFixture();t->setPerformanceMode(0);setParam(t.get(),"algorithm","4");
    setParam(t.get(),"midi_bend_range","12");noteMidi(*t,1,60,32);
    auto* v=externalVoice(*t,0,60);assert(v&&v->midiGain==1);
    t->padNoteOn(69,32);PlaybackVoice* pad=nullptr;for(auto& x:t->voices)if(x.held&&x.sourcePad>=0)pad=&x;
    assert(pad&&pad->midiGain==1);
    bendMidi(*t,1,16383);assert(v->midiTarget[0]==12&&v->midiValue[0]==0);renderSplit(*t,120);
    assert(std::abs(t->effectiveControls(*v).speed-2)<.0001f);assert(pad->midiValue[0]==0);
    bendMidi(*t,1,0);renderSplit(*t,120);assert(std::abs(t->effectiveControls(*v).speed-.5f)<.0001f);
    bendMidi(*t,1,8192);renderSplit(*t,120);assert(std::abs(t->effectiveControls(*v).speed-1)<.0001f);
    channelPressureMidi(*t,1,100);assert(std::abs(v->pressureTarget-100.f/127)<.0001f&&pad->pressureTarget==0);
    setParam(t.get(),"pressure_mode","Off");setParam(t.get(),"midi_mod_target","Stack");midiCc(*t,1,1,64);renderSplit(*t,120);
    assert(std::abs(t->effectiveControls(*v).lanePhaseOffset-64.f/127)<.0001f&&pad->midiValue[1]==0);
    setParam(t.get(),"algorithm","13");setParam(t.get(),"stochastic_amplitude","0");setParam(t.get(),"midi_mod_target","Character");
    midiCc(*t,1,1,127);renderSplit(*t,120);assert(t->effectiveControls(*v).stochasticAmplitude>.999f);
    midiCc(*t,1,121,0);renderSplit(*t,120);assert(v->midiTarget[0]==0&&v->midiTarget[1]==0&&v->midiTarget[2]==0);
    t->allNotesOff();renderSplit(*t,60);setParam(t.get(),"midi_velocity_depth","100");
    noteMidi(*t,1,60,32);v=externalVoice(*t,0,60);assert(v&&std::abs(v->midiGain-32.f/127)<1e-6f);
    t->padNoteOn(69,32);for(auto& x:t->voices)if(x.held&&x.sourcePad>=0)assert(x.midiGain==1);
    const std::uint8_t bad[]{0xe0,0xff,127};onMidi(t.get(),bad,3,MOVE_MIDI_SOURCE_EXTERNAL);assert(v->midiTarget[0]==0);
    onMidi(t.get(),bad,1,MOVE_MIDI_SOURCE_EXTERNAL);assert(v->midiTarget[0]==0);
    std::puts("s3g Thru MIDI expression: exact bend range/center, smooth live pitch, channel pressure, independent wheel, velocity and malformed-message safety: PASS");
}
void testStandardNoteClock() {
    auto t=splitFixture();t->setPerformanceMode(0);realtimeMidi(*t,0xfa);assert(!t->clipPlaying);
    setParam(t.get(),"midi_sync_source","Move");t->persistentStateDirty=false;
    for(int bpm:{40,73,120,197,240}) {
        t->resetMidiClock();const auto base=t->renderClockSamples.load();realtimeMidi(*t,0xfa);
        for(int tick=0;tick<100;++tick) {
            t->renderClockSamples.store(base+static_cast<std::uint64_t>(std::llround(tick*t->sampleRate*60./(24*bpm))));
            realtimeMidi(*t,0xf8);
        }
        assert(t->midiClockLocked&&std::abs(t->tempoBpm-bpm)<.02f&&!t->clipPlaying&&!t->persistentStateDirty);
        const auto ticks=t->midiClockCount;realtimeMidi(*t,0xf8,MOVE_MIDI_SOURCE_HOST);assert(t->midiClockCount==ticks);
    }
    t->padNoteOn(69,100);noteMidi(*t,1,60);midiCc(*t,1,64,127);noteMidi(*t,1,60,0);
    realtimeMidi(*t,0xfc);assert(!externalVoice(*t,0,60));assert(std::any_of(t->voices.begin(),t->voices.end(),[](const auto& v){return v.held&&v.sourcePad>=0;}));
    t->allNotesOff();renderSplit(*t,30);setParam(t.get(),"midi_sync_transport","Follow Clips");
    auto& clip=*t->activeClipSlot();clip.clipRecorded=true;clip.clipDurationMs=1000;clip.clipNoteCount=1;
    clip.clipNotes[0].startMs=0;clip.clipNotes[0].durationMs=800;clip.clipNotes[0].note=60;
    realtimeMidi(*t,0xfa);assert(t->clipPlaying&&t->clipPositionMs==0&&t->transportBeats==0);
    t->clipPositionMs=300;t->clipAbsolutePositionMs=1300;realtimeMidi(*t,0xfc);assert(!t->clipPlaying&&t->clipPositionMs==300);
    realtimeMidi(*t,0xfb);assert(t->clipPlaying&&t->clipPositionMs==300&&t->clipAbsolutePositionMs==1300);
    noteMidi(*t,1,67);realtimeMidi(*t,0xf8);t->renderClockSamples.fetch_add(static_cast<std::uint64_t>(t->sampleRate));t->serviceMidiClock();
    assert(!t->clipPlaying&&!externalVoice(*t,0,67)&&t->midiClockStopped&&!t->midiClockLocked);
    realtimeMidi(*t,0xf8);assert(!t->midiClockStopped&&!t->clipPlaying&&t->midiClockCount==1);
    for(int tick=1;tick<30;++tick){t->renderClockSamples.fetch_add(static_cast<std::uint64_t>(t->sampleRate/48));realtimeMidi(*t,0xf8);}
    assert(t->midiClockLocked&&std::abs(t->tempoBpm-120)<.1f);realtimeMidi(*t,0xfb);assert(t->clipPlaying);
    std::puts("s3g Thru MIDI sync: clock range, single source, tempo-only, no tick autosave, Start/Stop/Continue, clock loss and pad isolation: PASS");
}
void testStandardNoteRecordingAndRecall() {
    char folder[]="/tmp/thru-midi-notes.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();
    setParam(t.get(),"midi_bend_range","12");setParam(t.get(),"midi_velocity_depth","100");
    setParam(t.get(),"midi_mod_target","Stack");bendMidi(*t,1,12288);midiCc(*t,1,1,64);
    noteMidi(*t,1,60,64);midiCc(*t,1,64,127);noteMidi(*t,1,60,0);
    t->beginClipRecording();assert(t->activeClipSlot()->clipNoteCount==1);renderSplit(*t,40);
    bendMidi(*t,1,0);midiCc(*t,1,1,127);channelPressureMidi(*t,1,100);renderSplit(*t,50);
    midiCc(*t,1,64,0);const auto held=t->activeClipSlot()->clipNotes[0].durationMs;assert(held>200);
    renderSplit(*t,40);t->finishClipRecording();t->setClipPlaying(false);
    const auto& clip=*t->activeClipSlot();assert(clip.clipPointCount>0);
    bool expression=false;for(std::size_t i=0;i<clip.clipPointCount;++i)expression|=clip.clipPoints[i].type==ClipPointType::midiExpression;assert(expression);
    t->allNotesOff();t->triggerClipVoice(clip,0);PlaybackVoice* playback=nullptr;
    for(auto& v:t->voices)if(v.clipOwned&&v.held)playback=&v;
    assert(playback&&std::abs(playback->midiGain-64.f/127)<1e-6f&&playback->midiTarget[0]>5.9&&playback->midiTarget[1]>.49f);
    t->applyClipCurves(clip,200);assert(playback->midiTarget[0]<-11.9f&&playback->midiTarget[1]==1);
    t->allNotesOff();t->setClipPlaying(false);
    const auto points=clip.clipPointCount;const auto notes=clip.clipNoteCount;
    midiCc(*t,1,121,0);setParam(t.get(),"midi_velocity_depth","0");
    setParam(t.get(),"clip_record_mode","1");t->beginClipRecording();assert(!t->clipReplacing);
    noteMidi(*t,1,64);renderSplit(*t,20);noteMidi(*t,1,64,0);t->finishClipRecording();t->setClipPlaying(false);
    assert(clip.takeCount==2&&clip.clipNoteCount>notes);bool neutral=false;
    for(std::size_t i=0;i<clip.clipPointCount;++i)if(clip.clipPoints[i].type==ClipPointType::midiExpression&&ThruInstance::clipPointTake(clip.clipPoints[i])==1){assert(clip.clipPoints[i].value==0);neutral=true;}
    assert(neutral);t->undoLastClipTake();assert(clip.takeCount==1&&clip.clipNoteCount==notes&&clip.clipPointCount==points);
    setParam(t.get(),"midi_velocity_depth","100");
    t->allNotesOff();setParam(t.get(),"midi_note_route","Focus");setParam(t.get(),"midi_note_channel","2");
    setParam(t.get(),"midi_sync_source","Move");setParam(t.get(),"midi_sync_transport","Follow Clips");
    assert(t->savePersistentState());t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();
    assert(t->midiNoteRoute==1&&t->midiNoteChannel==2&&t->midiBendRange==12&&t->midiVelocityDepth==1&&t->midiModTarget==1&&t->midiSyncSource==1&&t->midiSyncTransport==1);
    assert(!t->midiClockSeen&&!t->midiClockLocked&&t->activeClipSlot()->clipNoteCount==1);
    assert(t->bankJob.load()==0);t.reset();removeBackgroundTestDirectory(folder);
    std::puts("s3g Thru MIDI performance: held/pedal notes, resolved duration, recorded expression/velocity, current-format roundtrip and global routing recall: PASS");
}
void testStandardNoteBankAndEngines() {
    char folder[]="/tmp/thru-midi-bank.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();t->backgroundStarted=true;t->setPerformanceMode(4);
    for(int note=0;note<128;++note)noteMidi(*t,2,note);
    assert(t->bankJob.load()==0&&t->bankRecordSlot==-1&&std::none_of(t->voices.begin(),t->voices.end(),[](const auto& v){return v.held;}));
    assert(t->startBankCapture(0));t->splitBus[0][0].fill(.12f);t->splitBus[0][1].fill(-.06f);
    for(int block=0;block<256;++block)t->recordBankBus(kBlockFrames);
    t->finishBankCapture();assert(t->processBankJob());t->consumeBankJob();assert(t->bankAssets[0]>=0);
    t->bankSlots[15]=t->bankSlots[0];t->bankAssets[15]=t->bankAssets[0];t->bankPool->assets[t->bankAssets[15]].pins.fetch_add(1);
    setParam(t.get(),"midi_velocity_depth","100");setParam(t.get(),"midi_bend_range","12");
    noteMidi(*t,2,60,64);auto* v=externalVoice(*t,1,60);assert(v&&v->bankSlot==0&&v->bankAsset>=0&&v->midiGain<.51f);
    noteMidi(*t,2,75);auto* last=externalVoice(*t,1,75);assert(last&&last->bankSlot==15);
    bendMidi(*t,2,16383);renderSplit(*t,120);assert(v->midiValue[0]>11.99f&&v->bankRate>1.99);
    midiCc(*t,2,64,127);noteMidi(*t,2,60,0);assert(v->held);midiCc(*t,2,64,0);assert(!v->held);
    noteMidi(*t,2,75,0);assert(!last->held&&t->bankJob.load()==0);
    t->bankSlots[15].play=1;noteMidi(*t,2,75);last=externalVoice(*t,1,75);assert(last);
    noteMidi(*t,2,75,0);midiCc(*t,2,120,0);assert(last->panicRelease&&last->envelopeStage==AmpEnvelopeStage::release);

    t->backgroundStarted=false;t.reset();removeBackgroundTestDirectory(folder);
    t=splitFixture();t->setPerformanceMode(0);noteMidi(*t,1,60);v=externalVoice(*t,0,60);assert(v);
    for(int algorithm=0;algorithm<15;++algorithm) {
        t->controls.algorithm=static_cast<Engine::PlaybackAlgorithm>(algorithm);
        v->midiValue[0]=0;const auto neutral=t->effectiveControls(*v);
        v->midiValue[0]=12;const auto raised=t->effectiveControls(*v);
        const float ratio=algorithm==3?raised.cutPitch/neutral.cutPitch:
            (algorithm==0||algorithm==1||algorithm==4||algorithm==5||algorithm==6)?raised.speed/neutral.speed:raised.notePitch/neutral.notePitch;
        assert(std::abs(ratio-2)<.0001f);
        if(algorithm==12)assert(std::abs(raised.formantNoteHz/neutral.formantNoteHz-2)<.0001f);
    }
    char editor[32768]{};assert(getParam(t.get(),"ui_editor_state",editor,sizeof(editor))>0);
    for(const auto* key:{"midi_note_route","midi_note_channel","midi_bend_range","midi_mod_target","midi_velocity_depth","midi_sync_source","midi_sync_transport"})assert(std::strstr(editor,key));
    std::puts("s3g Thru MIDI Bank: empty-slot safety, both slot endpoints, gain, live bend and sustain; fifteen-engine pitch and complete editor snapshot: PASS");
}

void testMidiSlices() {
    auto slices=[] {
        auto t=splitFixture();t->setPerformanceMode(0);
        t->controls.algorithm=Engine::PlaybackAlgorithm::slices;
        t->controls.sampleMode=Engine::SampleMode::loop;
        t->controls.sampleRegionMode=Engine::RegionMode::equal;
        t->controls.sampleRegionCount=8;t->controls.speed=.75f;
        t->rootNote=60;renderSplit(*t,40);return t;
    };
    for(int note:{59,60,61,67,68,72,127}) {
        auto t=slices();noteMidi(*t,1,note);
        auto* v=externalVoice(*t,0,note);assert(v);
        const auto c=t->effectiveControls(*v);
        assert(c.sampleSlice==std::max(0,note-60)&&std::abs(c.speed-.75f)<1e-6f);
        renderSplit(*t,20);
        const int region=std::max(0,note-60)%8;
        assert(v->engine.normalizedPosition()>=region/8.f);
        assert(v->engine.normalizedPosition()<(region+1)/8.f);
        noteMidi(*t,1,note,0);assert(!v->held);
    }
    // Two MIDI keys wrapping onto the same region must render identically,
    // including its loop seam, rather than playing an octave apart.
    auto a=slices(),b=slices();noteMidi(*a,1,60);noteMidi(*b,1,68);
    std::array<std::int16_t,kBlockFrames*2> ao{},bo{};double energy=0;
    for(int block=0;block<24;++block) {
        a->render(ao.data(),kBlockFrames);b->render(bo.data(),kBlockFrames);
        assert(ao==bo);for(auto sample:ao)energy+=std::abs(sample);
    }
    assert(energy>1000);
    // Bend remains a deliberate pitch control, independent of slice number.
    auto t=slices();setParam(t.get(),"midi_bend_range","12");noteMidi(*t,1,61);
    auto* v=externalVoice(*t,0,61);assert(v);
    bendMidi(*t,1,16383);renderSplit(*t,120);
    assert(t->effectiveControls(*v).sampleSlice==1);
    assert(std::abs(t->effectiveControls(*v).speed-1.5f)<.0001f);
    bendMidi(*t,1,8192);renderSplit(*t,120);
    assert(std::abs(t->effectiveControls(*v).speed-.75f)<.0001f);
    v->entryVariation.octave=1;
    assert(t->effectiveControls(*v).sampleSlice==1&&std::abs(t->effectiveControls(*v).speed-1.5f)<.0001f);
    v->entryVariation={};
    // Equal and transient region readers share the MIDI slice-address rule.
    t->controls.sampleRegionMode=Engine::RegionMode::transients;
    const float boundaries[]{0,.2f,.55f,1};
    auto& map=t->activeRegions[v->variantLane];map.count=3;
    std::copy_n(boundaries,4,map.onsets.begin());std::copy_n(boundaries,4,map.boundaries.begin());
    renderSplit(*t,20);
    assert(v->sliceLow==.125f&&v->sliceHigh==.25f); // held bounds remain unchanged
    noteMidi(*t,1,61,0);renderSplit(*t,16);noteMidi(*t,1,61);renderSplit(*t,20);
    v=externalVoice(*t,0,61);assert(v&&v->sliceLow==.2f&&v->sliceHigh==.55f);
    assert(v->engine.normalizedPosition()>=.18f&&v->engine.normalizedPosition()<.55f);
    assert(std::abs(t->effectiveControls(*v).speed-.75f)<.0001f);
    // Record and replay the original key as a slice address, with no need
    // for the external keyboard to remain connected.
    t=slices();t->beginClipRecording();noteMidi(*t,1,67);renderSplit(*t,12);
    noteMidi(*t,1,67,0);renderSplit(*t,4);t->finishClipRecording();t->setClipPlaying(false);
    const auto& clip=*t->activeClipSlot();assert(clip.clipNoteCount==1);
    assert(clip.clipNotes[0].note==67&&clip.clipNotes[0].sourcePad>=32);
    t->allNotesOff();t->applyClipCurves(clip,0);t->triggerClipVoice(clip,0);PlaybackVoice* playback=nullptr;
    for(auto& voice:t->voices)if(voice.active&&voice.clipOwned&&voice.held)playback=&voice;
    assert(playback&&t->effectiveControls(*playback).sampleSlice==7);
    assert(std::abs(t->effectiveControls(*playback).speed-.75f)<1e-6f);
    // Physical Keys keeps its pitched slices; physical SLIC keeps Root pitch.
    t=slices();t->padNoteOn(69,100);PlaybackVoice* pad=nullptr;
    for(auto& voice:t->voices)if(voice.held&&voice.sourcePad==1)pad=&voice;
    assert(pad);const auto expected=clampValue(.75f*std::exp2((pad->note-60)/12.f),.05f,4.f);
    assert(t->effectiveControls(*pad).sampleSlice==1&&std::abs(t->effectiveControls(*pad).speed-expected)<1e-6f);
    t=slices();t->padMode=PadMode::slices;t->padNoteOn(69,100);pad=nullptr;
    for(auto& voice:t->voices)if(voice.held&&voice.sourcePad==1)pad=&voice;
    assert(pad&&t->effectiveControls(*pad).sampleSlice==1&&std::abs(t->effectiveControls(*pad).speed-.75f)<1e-6f);
    // The rule follows the actual voice engine, including Engine Lists.
    t=slices();t->controls.algorithm=Engine::PlaybackAlgorithm::sample;noteMidi(*t,1,72);
    v=externalVoice(*t,0,72);assert(v&&std::abs(t->effectiveControls(*v).speed-1.5f)<1e-6f);
    v->assignedAlgorithm=static_cast<int>(Engine::PlaybackAlgorithm::slices);
    assert(t->effectiveControls(*v).sampleSlice==12&&std::abs(t->effectiveControls(*v).speed-.75f)<1e-6f);
    v->assignedAlgorithm=-1;assert(std::abs(t->effectiveControls(*v).speed-1.5f)<1e-6f);
    // Each part uses its own Root and SPEED without changing editing focus.
    for(int mode:{1,2,3}) {
        t=splitFixture();t->setPerformanceMode(mode);
        for(int part=0;part<t->partCount();++part) {
            t->selectSplitPart(part,false);t->rootNote=48+part*5;
            t->controls.algorithm=Engine::PlaybackAlgorithm::slices;t->controls.speed=.5f+part*.2f;
        }
        t->selectSplitPart(0,false);
        for(int part=0;part<t->partCount();++part) {
            const int note=48+part*5+3;noteMidi(*t,part+1,note);
            v=externalVoice(*t,part,note);assert(v);
            const auto c=t->effectiveControls(*v);
            assert(c.sampleSlice==3&&std::abs(c.speed-(.5f+part*.2f))<1e-6f&&t->splitFocus==0);
            noteMidi(*t,part+1,note,0);assert(!v->held);
        }
    }
    std::puts("s3g Thru MIDI Slices: fixed-pitch keys, region wrapping/audio, transients, bend, recorded clips, pad isolation and per-part Roots: PASS");
}
