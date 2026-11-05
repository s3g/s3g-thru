PlaybackVoice& newestVoice(ThruInstance& t,int part=0) {
    PlaybackVoice* result=nullptr;for(auto& v:t.voices)if(v.active&&(!t.splitEnabled||v.part==part)&&(!result||v.age>result->age))result=&v;
    assert(result);return *result;
}
void silenceVoicePart(ThruInstance& t,int part=0) {
    for(auto& v:t.voices)if(!t.splitEnabled||v.part==part){v.active=v.held=false;v.stealPending=false;v.engine.stopPlayback();}
}
void testVoicePlayOrdersAndKeyboard() {
    for(int mode=0;mode<4;++mode) {
        auto t=splitFixture();t->setPerformanceMode(mode);
        const int parts=mode?t->partCount():1,cap=t->engineSlotCount();
        for(int part=0;part<parts;++part) {
            t->selectSplitPart(part,false);setParam(t.get(),"voice_engine_mode","1");t->midiPart=part;
            for(int order:{3,4,5}) {
                setParam(t.get(),"voice_engine_order",std::to_string(order).c_str());
                for(int pass=0;pass<3;++pass) {
                    unsigned seen=0;
                    for(int n=0;n<cap;++n) {
                        silenceVoicePart(*t,part);t->noteOn(60+n,100);const auto& v=newestVoice(*t,part);
                        assert(v.engineSlot>=0&&v.engineSlot<cap);assert(!(seen&(1u<<v.engineSlot)));seen|=1u<<v.engineSlot;
                        if(order!=5)assert(v.engineSlot==(order==3?n:cap-1-n));
                    }
                    assert(seen==(1u<<cap)-1u);
                }
                silenceVoicePart(*t,part);
            }
            setParam(t.get(),"voice_engine_order","3");
            t->noteOn(60,100);assert(newestVoice(*t,part).engineSlot==0);
            for(int n=1;n<cap;++n){t->noteOn(60+n,100);assert(newestVoice(*t,part).engineSlot==n);}
            t->noteOff(61);t->noteOn(84,100);assert(newestVoice(*t,part).engineSlot==1);
            silenceVoicePart(*t,part);
            // Degree selection is independent of playing order, octave and other held notes.
            setParam(t.get(),"voice_engine_assign","1");t->padScale=PadScale::major;t->rootNote=60;
            for(const auto pair:std::array<std::array<int,2>,6>{{{60,0},{64,2},{67,4},{72,0},{63,1},{59,6}}}) {
                t->noteOn(pair[0],100);assert(newestVoice(*t,part).engineSlot==pair[1]%cap);
            }
            silenceVoicePart(*t,part);
            setParam(t.get(),"voice_engine_assign","2");
            int low=127,high=0;
            for(int pad=0;pad<32;++pad)if(!mode||t->partForInput(pad)==part) {const int note=t->padToMidiNote(kFirstPadNote+pad);low=std::min(low,note);high=std::max(high,note);}
            for(int note=low;note<=high;++note) {
                silenceVoicePart(*t,part);t->noteOn(note,100);
                assert(newestVoice(*t,part).engineSlot==clampValue((note-low)*cap/(high-low+1),0,cap-1));
            }
            silenceVoicePart(*t,part);t->noteOn(0,100);assert(newestVoice(*t,part).engineSlot==0);
            silenceVoicePart(*t,part);t->noteOn(127,100);assert(newestVoice(*t,part).engineSlot==cap-1);
            silenceVoicePart(*t,part);
        }
    }
    auto t=splitFixture();
    nrpn(*t,5,"voice_engine_mode",16383);nrpn(*t,6,"voice_engine_mode",16383);
    nrpn(*t,5,"voice_engine_order",s3g_thru_midi::encode(midiParameter("voice_engine_order"),3,5));
    nrpn(*t,6,"voice_engine_order",s3g_thru_midi::encode(midiParameter("voice_engine_order"),3,5));
    t->midiPart=0;t->noteOn(60,100);assert(newestVoice(*t).engineSlot==0);silenceVoicePart(*t);
    nrpn(*t,6,"locus",8000); // Editing B must not reset either traversal or change focus.
    t->midiPart=0;t->noteOn(61,100);assert(newestVoice(*t).engineSlot==1);
    t->midiPart=1;t->noteOn(60,100);assert(newestVoice(*t,1).engineSlot==0&&t->splitFocus==0);
    const int focused=t->splitFocus;const auto lane=t->selectedLane;
    nrpn(*t,6,"voice_engine_edit",16383);
    nrpn(*t,6,"voice_engine_octave",s3g_thru_midi::encode(midiParameter("voice_engine_octave"),-1,2));
    nrpn(*t,6,"voice_engine_position",8192);const float before=t->settingsForPart(1).engines.variations[3].position;
    nrpn(*t,6,"voice_engine_position",8193);assert(t->settingsForPart(1).engines.variations[3].position>before);
    nrpn(*t,6,"voice_engine_pan",16383);nrpn(*t,6,"voice_engine_depth",8192);
    const auto cfg=t->settingsForPart(1).engines;
    assert(cfg.variations[3].octave==-1&&cfg.variations[3].pan==1&&std::abs(cfg.pressureDepth-.5f)<.001f);
    assert(t->splitFocus==focused&&t->selectedLane==lane&&midiRead(*t,midiParameter("voice_engine_pan"),1)==100);
    std::puts("s3g Thru Cycle, Shuffle, scale degree/register maps and independent part traversal: PASS");
}
void testVoicePlayVariationsAndPan() {
    auto plain=std::make_unique<CoreFixture>(),offset=std::make_unique<CoreFixture>();
    Engine::Controls direct;direct.algorithm=Engine::PlaybackAlgorithm::sample;direct.sampleMode=Engine::SampleMode::loop;
    plain->render(direct,1000);direct.noteStartOffset=.25f;offset->render(direct,1000);
    const float phase=offset->engine.normalizedPosition()-plain->engine.normalizedPosition();
    assert(std::abs(phase-.25f)<.01f); // Offset must work at a full-width Field too.

    auto t=splitFixture();t->setPerformanceMode(0);setParam(t.get(),"voice_engine_mode","1");setListSlot(*t,0,4);
    setParam(t.get(),"voice_engine_assign","1");setParam(t.get(),"voice_engine_octave","1");
    setParam(t.get(),"voice_engine_position","25");setParam(t.get(),"voice_engine_stack","100");setParam(t.get(),"voice_engine_pan","-100");
    t->noteOn(60,100);auto& v=newestVoice(*t);auto c=t->effectiveControls(v);
    assert(c.speed==2&&c.locus==.5f&&c.noteStartOffset==.25f&&c.sampleLane==3&&v.entryVariation.pan==-1);
    std::array<float,kBlockFrames*2> audio{};double left=0,right=0;
    for(int n=0;n<40;++n){t->render(nullptr,kBlockFrames,audio.data());for(std::size_t f=0;f<kBlockFrames;++f){left+=audio[f*2]*audio[f*2];right+=audio[f*2+1]*audio[f*2+1];}}
    assert(left>.01&&right<left*1e-8);
    setParam(t.get(),"voice_engine_octave","-1");setParam(t.get(),"voice_engine_position","-25");setParam(t.get(),"voice_engine_pan","100");
    assert(t->effectiveControls(v).speed==2&&v.entryVariation.pan==-1);
    t->noteOn(72,100);auto& upper=newestVoice(*t);c=t->effectiveControls(upper);
    assert(upper.engineSlot==0&&upper.entryVariation.pan==1&&c.speed==1&&c.locus==.5f&&c.noteStartOffset==-.25f);
    // A replacement's panning must wait for the old reader's fade to finish.
    t->panic();silenceVoicePart(*t);t->voiceEngineSettings.assignment=0;t->voiceEngineSettings.order=0;
    for(int i=0;i<8;++i){setListSlot(*t,i,4);t->voiceEngineSettings.variations[i].pan=-1;}
    for(int i=0;i<8;++i)t->noteOn(48+i,100);renderSplit(*t,30);
    t->voiceEngineSettings.variations[0].pan=1;t->noteOn(90,100);auto& replacement=newestVoice(*t);
    assert(replacement.stealPending&&replacement.stealPan==-1&&replacement.entryVariation.pan==1);
    renderSplit(*t,30);assert(!replacement.stealPending&&replacement.panState==1);
    std::puts("s3g Thru per-entry launch variation, stereo pan, immutable held values and steal protection: PASS");
}
void testVoicePlayPressure() {
    auto t=splitFixture();t->setPerformanceMode(0);setParam(t.get(),"algorithm","2");setParam(t.get(),"pressure_mode","3");
    t->noteOn(60,100);auto& a=newestVoice(*t);t->noteOn(67,100);auto& b=newestVoice(*t);
    t->aftertouch(60,127);renderSplit(*t,40);
    assert(a.pressureState>.9f&&b.pressureState==0&&t->effectiveControls(a).grainDensityHz>t->effectiveControls(b).grainDensityHz*3);
    assert(t->effectiveControls(a).lanePhaseOffset==0&&t->effectiveControls(b).lanePhaseOffset==0);
    setParam(t.get(),"pressure_mode","0");renderSplit(*t,120);assert(a.pressureState<1e-5f);
    t->panic();silenceVoicePart(*t);setParam(t.get(),"algorithm","0");setParam(t.get(),"pressure_mode","2");
    t->noteOn(60,100);auto& brake=newestVoice(*t);t->noteOn(67,100);auto& running=newestVoice(*t);
    renderSplit(*t,70);t->aftertouch(60,127);renderSplit(*t,1);
    assert(brake.pressureBrakeHeld&&!running.pressureBrakeHeld);const float held=brake.pressureBrakePhase;
    const float other=running.engine.normalizedLaneScanPhase();renderSplit(*t,70);
    assert(std::abs(brake.engine.normalizedLaneScanPhase()-held)<.001f&&running.engine.normalizedLaneScanPhase()!=other);
    t->aftertouch(60,0);renderSplit(*t,1);assert(!brake.pressureBrakeHeld);
    const float difference=std::abs(brake.engine.normalizedLaneScanPhase()-held);assert(std::min(difference,1-difference)<.02f);
    t->setPerformanceMode(3);setParam(t.get(),"pressure_mode","3");t->midiPart=0;t->noteOn(60,100);t->aftertouch(60,127);renderSplit(*t,30);
    assert(newestVoice(*t).pressureState>.9f);
    // Every character map remains finite; fallback tone is bounded and never adds voices.
    for(int algorithm=0;algorithm<static_cast<int>(Engine::kPlaybackAlgorithmCount);++algorithm) {
        auto c=Engine::Controls{};c.algorithm=static_cast<Engine::PlaybackAlgorithm>(algorithm);ThruInstance::applyVoiceCharacter(c,1);
        assert(std::isfinite(c.grainDensityHz)&&c.grainDensityHz<=96&&c.formantShift<=24&&c.morphAmplitude<=1&&c.textureScatter<=1);
    }
    std::puts("s3g Thru per-note character pressure, scan braking/resume, Quad expression and bounds: PASS");
}
void testVoicePlayRecordingAndPersistence() {
    char directory[]="/tmp/thru-voice-play-XXXXXX";assert(mkdtemp(directory));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(directory));t->stopBackgroundWork();
    setParam(t.get(),"voice_engine_mode","1");setParam(t.get(),"voice_engine_order","3");setParam(t.get(),"voice_engine_assign","1");
    setListSlot(*t,0,10);setParam(t.get(),"voice_engine_octave","-1");setParam(t.get(),"voice_engine_position","20");
    setParam(t.get(),"voice_engine_stack","-25");setParam(t.get(),"voice_engine_pan","70");setParam(t.get(),"pressure_mode","3");setParam(t.get(),"voice_engine_depth","60");
    t->noteOn(60,100);t->aftertouch(60,100);t->beginClipRecording();renderSplit(*t,10);
    t->aftertouch(60,50);renderSplit(*t,10);t->noteOff(60);renderSplit(*t,10);t->finishClipRecording();
    auto& clip=*t->activeClipSlot();assert(clip.clipNoteCount==1&&clip.clipNotes[0].engineVariation.octave==-1);
    assert(std::abs(clip.clipNotes[0].initialPressure-100.f/127)<1e-6f);bool curve=false;
    for(std::size_t i=0;i<clip.clipPointCount;++i)curve|=clip.clipPoints[i].type==ClipPointType::notePressure;
    assert(curve);t->releaseClipVoices();t->triggerClipVoice(clip,0);auto& replay=newestVoice(*t);
    assert(replay.clipOwned&&replay.entryVariation.octave==-1&&std::abs(replay.entryVariation.pan-.7f)<1e-6f);
    t->applyClipCurves(clip,0);assert(std::abs(replay.pressureTarget-100.f/127)<1e-6f);
    setParam(t.get(),"voice_engine_pan","-100");assert(replay.entryVariation.pan>.69f);
    assert(t->savePersistentState());
    auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(directory));restored->stopBackgroundWork();
    assert(restored->voiceEngineSettings.assignment==1&&restored->voiceEngineSettings.order==3&&restored->pressureMode==3);
    assert(std::abs(restored->voiceEngineSettings.pressureDepth-.6f)<1e-6f);
    const auto& note=restored->activeClipSlot()->clipNotes[0];assert(note.engineVariation.octave==-1&&note.engineVariation.stack==-.25f&&note.initialPressure>.78f);
    auto text=std::make_unique<std::array<char,kStateTextSize>>();assert(t->serializeState(text->data(),text->size())>0);
    assert(restored->restoreState(text->data()));assert(restored->voiceEngineSettings.variations[0].pan==-1);
    // Reject malformed extension data without replacing the current bank.
    auto* file=std::tmpfile();assert(file);assert(ThruInstance::writeVoicePlayExtension(file,t->clipSlots));
    std::rewind(file);auto copy=std::make_unique<std::array<ClipSlot,kClipSlotCount>>(t->clipSlots);assert(ThruInstance::readVoicePlayExtension(file,*copy));
    assert(std::fseek(file,4,SEEK_SET)==0);assert(writeU32(file,99));std::rewind(file);assert(!ThruInstance::readVoicePlayExtension(file,*copy));std::fclose(file);
    // Import an authentic test.5 payload: its body retains resolved engines,
    // while the new launch/expression trailer is absent and defaults are neutral.
    setParam(t.get(),"voice_engine_order","2");setParam(t.get(),"pressure_mode","1");
    // Format 24 never recorded external per-note pressure lanes.
    std::size_t kept=0;for(std::size_t n=0;n<clip.clipPointCount;++n)if(clip.clipPoints[n].type!=ClipPointType::notePressure)clip.clipPoints[kept++]=clip.clipPoints[n];
    clip.clipPointCount=static_cast<std::uint16_t>(kept);ThruInstance::rebuildClipPointIndex(clip);assert(t->writeClipFile());
    const auto path=std::string(directory)+"/thru-clips.bin";file=std::fopen(path.c_str(),"rb");assert(file);
    assert(std::fseek(file,0,SEEK_END)==0);const long bytes=std::ftell(file);std::rewind(file);
    std::vector<unsigned char> legacy(static_cast<std::size_t>(bytes)-4u);assert(std::fread(legacy.data(),1,legacy.size(),file)==legacy.size());std::fclose(file);
    stripVoicePlayExtension(legacy);assert(legacy[8]==24);
    std::uint32_t hash=2166136261u;for(auto b:legacy){hash^=b;hash*=16777619u;}
    file=std::fopen(path.c_str(),"wb");assert(file);assert(std::fwrite(legacy.data(),1,legacy.size(),file)==legacy.size());assert(writeU32(file,hash));assert(std::fclose(file)==0);
    assert(restored->readClipFileFromPath(path.c_str()));
    assert(restored->voiceEngineSettings.enabled&&restored->voiceEngineSettings.assignment==0&&restored->voiceEngineSettings.pressureDepth==1);
    const auto& prior=restored->activeClipSlot()->clipNotes[0];assert(prior.engineAlgorithm==10&&prior.engineSlot==0&&prior.engineVariation.octave==0&&prior.initialPressure==0);
    assert(restored->voiceEngineSettings.variations[0].pan==0&&restored->pressureMode==1);
    copy.reset();restored.reset();t.reset();removeBackgroundTestDirectory(directory);
    std::puts("s3g Thru entry/pressure recording, held-note capture, JSON/bank roundtrip and malformed data: PASS");
}
