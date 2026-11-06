void testLegatoKeys() {
    auto t=splitFixture();t->setPerformanceMode(0);
    setParam(t.get(),"voice_mode","2");
    t->noteOn(60,100);renderSplit(*t,20);
    auto* first=t->findHeldVoiceForNote(60,-1);assert(first);
    const auto position=first->engine.normalizedPosition();
    const auto envelope=first->envelope;
    t->noteOn(67,100);
    assert(first->note==67 && first->engine.normalizedPosition()==position);
    assert(first->envelope==envelope && !first->stealPending);
    assert(std::count_if(t->voices.begin(),t->voices.end(),[](const auto& v){return v.active;})==1);
    t->noteOn(64,100);t->noteOff(64);
    assert(first->note==67 && first->held);
    t->noteOff(67);assert(first->note==60 && first->held);
    t->noteOff(60);assert(!first->held);
    renderSplit(*t,40);
    auto& clip=*t->activeClipSlot();clip.clipDurationMs=1000;clip.clipNoteCount=2;
    clip.clipNotes[0].note=60;clip.clipNotes[0].startMs=0;clip.clipNotes[0].durationMs=800;
    clip.clipNotes[1].note=67;clip.clipNotes[1].startMs=100;clip.clipNotes[1].durationMs=300;
    t->clipControls=t->controls;t->triggerClipVoice(clip,0);renderSplit(*t,5);
    PlaybackVoice* recorded=nullptr;for(auto& voice:t->voices)if(voice.clipOwned&&voice.held)recorded=&voice;
    assert(recorded);const auto clipPosition=recorded->engine.normalizedPosition();
    t->triggerClipVoice(clip,1);assert(recorded->note==67&&recorded->engine.normalizedPosition()==clipPosition);
    t->releaseClipVoice(1);assert(recorded->note==60&&recorded->held);
    t->releaseClipVoice(0);assert(!recorded->held);
    t->allNotesOff();renderSplit(*t,40);
    t->noteOn(60,100);t->noteOn(67,100);t->beginClipRecording();
    assert(t->activeClipSlot()->clipNoteCount==2);
    assert(t->activeClipSlot()->clipNotes[0].note==60 && t->activeClipSlot()->clipNotes[1].note==67);
    t->noteOff(67);t->noteOff(60);renderSplit(*t,20);t->finishClipRecording();t->setClipPlaying(false);
    t->setPerformanceMode(2);t->selectSplitPart(0,false);setParam(t.get(),"voice_mode","2");
    t->selectSplitPart(1,false);setParam(t.get(),"voice_mode","1");
    t->captureActiveClip(true);t->activateStoredClip(t->activeClip,false);
    assert(t->settingsForPart(0).voiceMode==2 && t->settingsForPart(1).voiceMode==1);
    std::puts("s3g Thru Legato continuity, last-note priority and per-part settings: PASS");
}

void testBankCaptureAndRecall() {
    char folder[]="/tmp/thru-bank-test.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
    t->setPerformanceMode(4);assert(t->performanceMode()==4 && t->sharedDual);
    for(int ch=0;ch<2;++ch)for(std::size_t f=0;f<8192;++f)
        t->engine.writableLane(0,ch)[f]=.3f*(ch?.5f:1.f)*std::sin(6.28318530718f*220*f/t->sampleRate);
    assert(t->engine.adoptLane(0,8192,t->sampleRate));
    t->controls.algorithm=Engine::PlaybackAlgorithm::sample;
    t->controls.sampleMode=Engine::SampleMode::loop;t->refreshPlaybackVoices();
    t->padNoteOn(68,100);renderSplit(*t,12);
    t->padNoteOn(72,100);assert(t->bankJob.load()==1);
    renderSplit(*t,60);t->padNoteOff(72);assert(t->bankJob.load()>=2);
    for(int retry=0;retry<10000 && t->bankJob.load()!=0;++retry){renderSplit(*t,1);usleep(500);}
    assert(t->bankJob.load()==0 && t->bankAssets[0]>=0 && t->bankSlots[0].path[0]);
    assert(std::none_of(t->voices.begin(),t->voices.end(),[](const auto& v){return v.active&&v.part==1;}));
    const std::string archived=t->bankSlots[0].path.data();assert(access(archived.c_str(),F_OK)==0);
    t->padNoteOff(68);renderSplit(*t,40);
    t->padNoteOn(72,100);renderSplit(*t,10);
    auto* v=t->findHeldVoiceForNote(60,4);assert(v && v->bankAsset>=0);
    assert(std::abs(v->bankLast[0])+.01f>std::abs(v->bankLast[1]));
    t->padNoteOff(72);assert(!v->held);
    t->shiftHeld=true;t->padNoteOn(72,100);t->padNoteOff(72);assert(t->latchedPadNotes[4]>=0);
    t->shiftHeld=false;t->padNoteOn(72,100);t->padNoteOff(72);assert(t->latchedPadNotes[4]>=0);t->shiftHeld=true;
    t->padNoteOn(72,100);t->padNoteOff(72);assert(t->latchedPadNotes[4]<0);t->shiftHeld=false;
    t->initializeClipIfNeeded(1);
    assert(std::string(t->clipSlots[1].bankSlots[0].path.data())==archived);
    assert(t->prepareClipBank(1));t->activateStoredClip(1,false);
    t->beginClipRecording();assert(t->clipRecording);
    t->padNoteOn(72,100);renderSplit(*t,30);
    const int captureKey=t->clipCaptureKey(60,4,1);assert(t->clipOpenNoteIndices[captureKey]>=0);
    t->clearBankSlot(0);assert(t->clipOpenNoteIndices[captureKey]<0);t->restoreUndo();
    t->padNoteOff(72);renderSplit(*t,30);t->finishClipRecording();
    auto& performance=*t->activeClipSlot();assert(performance.clipNoteCount>0);
    assert(performance.clipNotes[0].sourcePad==4 && performance.clipNotes[0].part==2);
    renderSplit(*t,5);
    assert(std::any_of(t->voices.begin(),t->voices.end(),[](const auto& voice){return voice.clipOwned&&voice.bankAsset>=0;}));
    t->setClipPlaying(false);t->allNotesOff();renderSplit(*t,40);
    t->midiPart=1;t->noteOn(60,100);assert(t->findHeldVoiceForNote(60,-1));
    t->noteOff(60);assert(!t->findHeldVoiceForNote(60,-1));t->midiPart=0;
    renderSplit(*t,40);
    t->padNoteOn(72,100);renderSplit(*t,10);
    const int asset=t->bankAssets[0],pins=t->bankPool->assets[asset].pins.load();
    t->patternFadeMs=1000;assert(t->beginPatternFade(0));
    assert(t->patternShadow->bankMode && t->patternShadow->bankAssets[0]==asset);
    renderSplit(*t,1); // Start the outgoing renderer after its warmup block.
    t->finishPatternFade();
    for(int retry=0;retry<6000&&t->patternJob.load()!=0;++retry){renderSplit(*t,1);usleep(500);}
    if(t->patternJob.load()!=0 || t->bankPool->assets[asset].pins.load()!=pins)
        std::fprintf(stderr,"Bank fade: job=%d active=%d warmup=%d pins=%d expected=%d shadowSlot=%d shadowVoice=%d\n",t->patternJob.load(),t->patternActive,t->patternWarmup,t->bankPool->assets[asset].pins.load(),pins,t->patternShadow->bankAssets[0],t->patternShadow->voices[4].bankAsset);
    assert(t->patternJob.load()==0 && t->bankPool->assets[asset].pins.load()==pins);
    t->padNoteOff(72);renderSplit(*t,40);
    // Exercise the real hardware messages, rather than setting the modifier
    // field directly: CC 50 is Menu; the physical X/Delete key is CC 119.
    midiCc(*t,1,50,127,MOVE_MIDI_SOURCE_INTERNAL);assert(!t->deleteHeld);
    const std::uint8_t padDown[]{0x90,72,100},padUp[]{0x80,72,0};
    onMidi(t.get(),padDown,3,MOVE_MIDI_SOURCE_INTERNAL);
    assert(t->bankSlots[0].path[0]);
    onMidi(t.get(),padUp,3,MOVE_MIDI_SOURCE_INTERNAL);
    midiCc(*t,1,50,0,MOVE_MIDI_SOURCE_INTERNAL);
    midiCc(*t,1,119,127);assert(!t->deleteHeld); // External CC cannot hold X.
    const auto revision=t->persistentRevision;
    setParam(t.get(),"delete_held","1");assert(t->deleteHeld && t->persistentRevision==revision);
    setParam(t.get(),"delete_held","0");assert(!t->deleteHeld);
    midiCc(*t,1,119,127,MOVE_MIDI_SOURCE_INTERNAL);assert(t->deleteHeld);
    onMidi(t.get(),padDown,3,MOVE_MIDI_SOURCE_INTERNAL);
    onMidi(t.get(),padUp,3,MOVE_MIDI_SOURCE_INTERNAL);
    // Repeating X + an empty pad must not start recording or replace Undo.
    onMidi(t.get(),padDown,3,MOVE_MIDI_SOURCE_INTERNAL);
    onMidi(t.get(),padUp,3,MOVE_MIDI_SOURCE_INTERNAL);
    assert(t->bankJob.load()==0);
    midiCc(*t,1,119,0,MOVE_MIDI_SOURCE_INTERNAL);assert(!t->deleteHeld);
    assert(!t->bankSlots[0].path[0] && access(archived.c_str(),F_OK)==0);
    t->restoreUndo();assert(t->bankSlots[0].path[0] && t->bankAssets[0]>=0);
    setParam(t.get(),"voice_mode","2");assert(t->savePersistentState());t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
    assert(t->bankMode && t->voiceMode==2 && std::string(t->bankSlots[0].path.data())==archived);
    for(int retry=0;retry<10000 && t->bankAssets[0]<0;++retry){renderSplit(*t,1);usleep(500);}
    assert(t->bankAssets[0]>=0);
    t->padNoteOn(72,100);renderSplit(*t,20);assert(t->voices[4].bankAsset>=0);
    assert(t->clipSlots[0].bankMode);
    const auto pool=t->bankPool;t.reset();
    for(const auto& entry:pool->assets)assert(entry.pins.load()==0);
    removeBackgroundTestDirectory(folder);
    std::puts("s3g Thru Bank stereo capture, no auto-play, latch, clear, undo and restart recall: PASS");
}

void testBankSaveRecovery() {
    char folder[]="/tmp/thru-bank-retry.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
    t->stopBackgroundWork();t->backgroundStarted=true;t->setPerformanceMode(4);
    t->patternActive=true;assert(!t->startBankCapture(0));t->patternActive=false;
    assert(t->startBankCapture(0));
    t->selectClip(1);assert(t->activeClip==0);
    t->clearClipSlot(0);assert(t->clipSlots[0].bankMode);
    assert(!t->clipClipboardEditAllowed());t->startRecording();assert(!t->engine.recording());
    t->bankRecordFrames=512;
    std::fill_n(t->bankRecordAudio.get(),512,.125f);
    std::fill_n(t->bankRecordAudio.get()+t->laneCapacityFrames,512,-.0625f);
    const auto original=t->bankRecordPath;
    std::snprintf(t->bankRecordPath.data(),t->bankRecordPath.size(),"%s/missing/take.wav",folder);
    t->finishBankCapture();assert(t->processBankJob());t->consumeBankJob();
    assert(t->bankJob.load()==5 && t->bankSaveFailed && t->saveStatus()==4);
    assert(t->bankRecordFrames==512 && t->bankRecordAudio[0]==.125f && !t->bankSlots[0].path[0]);
    t->bankRecordPath=original;t->bankRetryFrame=0;t->consumeBankJob();
    assert(t->bankJob.load()==2);assert(t->processBankJob());t->consumeBankJob();
    assert(t->bankJob.load()==0 && !t->bankSaveFailed && t->bankAssets[0]>=0);
    assert(t->bankPool->assets[t->bankAssets[0]].audio[0]==.125f);
    t->backgroundStarted=false;resumeTestBackgroundWorker(*t);assert(t->savePersistentState());
    t.reset();removeBackgroundTestDirectory(folder);
    std::puts("s3g Thru Bank capture guards and retained-audio save retry: PASS");
}

void testBankPadControlsAndSources() {
    char folder[]="/tmp/thru-bank-controls.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
    t->stopBackgroundWork();t->backgroundStarted=true;t->setPerformanceMode(4);
    setParam(t.get(),"bank_record_source","0");
    setParam(t.get(),"bank_pan","-50");setParam(t.get(),"bank_rate",".5");
    assert(t->startBankCapture(0));
    setParam(t.get(),"bank_record_source","2"); // active take keeps the starting source
    t->monitorInput[0].fill(.25f);t->monitorInput[1].fill(-.125f);
    t->splitBus[0][0].fill(.75f);t->splitBus[0][1].fill(.5f);
    for(int i=0;i<128;++i)t->recordBankBus(64);
    assert(t->bankRecordAudio[0]==.25f && t->bankRecordAudio[t->laneCapacityFrames]==-.125f);
    t->finishBankCapture();assert(t->processBankJob());t->consumeBankJob();
    assert(t->bankJob.load()==0 && t->bankSlots[0].pan==-.5f && t->bankSlots[0].rate==.5f);
    setParam(t.get(),"bank_master_rate","2");
    t->startBankVoice(0,4,100);auto* voice=t->findHeldVoiceForNote(60,4);assert(voice);
    Engine::Controls effective=t->controls;
    t->renderBankVoice(*voice,64,effective);
    assert(std::abs(voice->bankPosition-64)<.001 && voice->bankRate==1);
    assert(std::abs(voice->output[0][32]-.25f)<.001 && std::abs(voice->output[1][32]+.0625f)<.001);
    setParam(t.get(),"bank_pan","100");
    for(int i=0;i<100;++i)t->renderBankVoice(*voice,64,effective);
    assert(std::abs(voice->output[0][32])<.001 && std::abs(voice->output[1][32]+.125f)<.001);
    const auto last=voice->bankLast;
    setParam(t.get(),"bank_play","1");t->renderBankVoice(*voice,1,effective);
    assert(voice->bankTailFrames>0 && voice->output[0][0]==last[0] && voice->output[1][0]==last[1]);
    t->startBankVoice(0,4,100);
    voice=t->findHeldVoiceForNote(60,4);assert(voice);t->noteOff(60,4);
    assert(!voice->held && voice->envelopeStage!=AmpEnvelopeStage::release);
    for(int i=0;i<140;++i)t->renderBankVoice(*voice,64,effective);
    assert(voice->bankEnded && voice->output[0][63]==0 && voice->envelopeStage==AmpEnvelopeStage::release);
    t->allNotesOff();renderSplit(*t,40);
    setParam(t.get(),"bank_choke","2");t->startBankVoice(0,4,100);
    auto* old=t->findHeldVoiceForNote(60,4);assert(old);
    // A second pad in the same group releases the former reader smoothly.
    t->bankSlots[1]=t->bankSlots[0];t->bankAssets[1]=t->bankAssets[0];
    t->bankPool->assets[t->bankAssets[1]].pins.fetch_add(1);
    t->startBankVoice(1,5,100);
    assert(!old->held && old->envelopeStage==AmpEnvelopeStage::release);
    assert(t->findHeldVoiceForNote(61,5));
    t->allNotesOff();renderSplit(*t,40);
    setParam(t.get(),"bank_slot","3");assert(t->startBankCapture(2));
    for(int block=0;block<8;++block)t->recordBankBus(64);
    assert(t->bankRecordAudio[0]==t->splitBus[0][0][0]);
    t->finishBankCapture();assert(t->processBankJob());t->consumeBankJob();
    assert(t->bankJob.load()==0);
    setParam(t.get(),"bank_record_source","3");t->captureActiveClip(true);
    t->backgroundStarted=false;resumeTestBackgroundWorker(*t);assert(t->savePersistentState());t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
    assert(t->bankMasterRate==2 && t->bankRecordSource==3);
    assert(t->bankSlots[0].rate==.5f && t->bankSlots[0].pan==1 && t->bankSlots[0].play==1 && t->bankSlots[0].choke==2);
    // A test.47 format-32 set retains its original per-pad rate at master 1x.
    t->stopBackgroundWork();
    const std::string current=std::string(folder)+"/thru-clips.bin",legacy=std::string(folder)+"/legacy.bin";
    auto* file=std::fopen(current.c_str(),"rb");assert(file);assert(std::fseek(file,0,SEEK_END)==0);
    const auto size=std::ftell(file);std::rewind(file);std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    assert(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);
    bytes.resize(bytes.size()-4);
    // Test.48-50 format 33 stored only A=0 / IN=1. Both retain their
    // source when imported into the new four-source layout.
    const auto original=bytes;
    constexpr std::size_t extension=4+64*(8+16*12);
    const auto sourceAt=bytes.size()-extension+8;
    for(int oldSource:{0,1}) {
        bytes=original;bytes[8]=33;
        for(std::size_t clip=0;clip<64;++clip)for(int b=0;b<4;++b)
            bytes[sourceAt+clip*200+b]=static_cast<unsigned char>(oldSource>>(b*8));
        file=std::fopen(legacy.c_str(),"wb");assert(file);
        assert(std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);
        assert(appendFileChecksum(legacy.c_str()));assert(t->readClipFileFromPath(legacy.c_str()));
        assert(t->bankRecordSource==(oldSource==0?2:0));
    }
    bytes=original;stripBankPadExtension(bytes);
    file=std::fopen(legacy.c_str(),"wb");assert(file);
    assert(std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);assert(appendFileChecksum(legacy.c_str()));
    assert(t->readClipFileFromPath(legacy.c_str()));
    assert(t->bankMasterRate==1 && t->bankRecordSource==2 && t->bankSlots[0].rate==.5f);
    assert(t->bankSlots[0].pan==0 && t->bankSlots[0].play==0 && t->bankSlots[0].choke==0);
    t.reset();std::remove(legacy.c_str());removeBackgroundTestDirectory(folder);
    std::puts("s3g Thru Bank master/ratio, live stereo balance, one-shot, choke, input source and restart recall: PASS");
}


void testBankFourRecordingSources() {
    char folder[]="/tmp/thru-bank-four-sources.XXXXXX";assert(mkdtemp(folder));
    std::array<std::int16_t,kBlockFrames*2> adc{};
    for(std::size_t f=0;f<kBlockFrames;++f){adc[2*f]=4096;adc[2*f+1]=-8192;}
    host_api_v1_t host{};host.sample_rate=44100;host.frames_per_block=kBlockFrames;
    host.mapped_memory=reinterpret_cast<std::uint8_t*>(adc.data());
    const auto* priorHost=g_host;g_host=&host;
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
    t->stopBackgroundWork();t->backgroundStarted=true;t->setPerformanceMode(4);
    assert(t->bankRecordSource==2);t->recordGain=2;
    // Seed one real, archived Bank sample; leave the other pads empty.
    t->splitBus[0][0].fill(.3f);t->splitBus[0][1].fill(.15f);
    assert(t->startBankCapture(0));for(int block=0;block<128;++block)t->recordBankBus(64);
    t->finishBankCapture();assert(t->processBankJob());t->consumeBankJob();assert(t->bankAssets[0]>=0);
    for(int channel=0;channel<2;++channel)
        std::fill_n(t->engine.writableLane(0,channel),8192,channel?.4f:.2f);
    assert(t->engine.adoptLane(0,8192,t->sampleRate));
    t->controls.algorithm=Engine::PlaybackAlgorithm::sample;t->controls.sampleMode=Engine::SampleMode::loop;
    t->refreshPlaybackVoices();t->padNoteOn(68,100);t->padNoteOn(72,100);renderSplit(*t,32);
    assert(t->findHeldVoiceForNote(60,4));
    std::array<float,kBlockFrames*2> raw{};
    for(int source=0;source<4;++source) {
        if(source==3) {
            // PT B must still record the Bank readers when B is down in the mix.
            t->selectSplitPart(1,false);setParam(t.get(),"part_mute","1");t->selectSplitPart(0,false);
            renderSplit(*t,80);
        }
        setParam(t.get(),"bank_record_source",kBankRecordSourceNames[source]);
        assert(t->bankRecordSource==source);assert(t->startBankCapture(source+1));
        if(source==0)setParam(t.get(),"bank_record_source","Part A");
        double energy=0,mixDifference=0;
        for(std::size_t block=0;block<4;++block) {
            t->render(nullptr,kBlockFrames,raw.data());
            assert(t->bankRecordFrames==(block+1)*kBlockFrames); // one copy per main block
            for(std::size_t f=0;f<kBlockFrames;++f)for(int channel=0;channel<2;++channel) {
                const float actual=t->bankRecordAudio[channel*t->laneCapacityFrames+block*kBlockFrames+f];
                const float expected=source==0?(channel?-.5f:.25f)
                    :source==1?raw[f*2+channel]:t->splitBus[source-2][channel][f];
                assert(actual==expected);energy+=actual*actual;
                if(source==1)mixDifference+=std::abs(actual-t->splitBus[0][channel][f])
                    +std::abs(actual-t->splitBus[1][channel][f]);
            }
        }
        assert(energy>.001 && t->monitorLevel==0);
        if(source==1)assert(mixDifference>.001);
        if(source==3)assert(t->splitFrameGain[1].back()<1e-5f);
        t->finishBankCapture();assert(t->processBankJob());t->consumeBankJob();assert(t->bankJob.load()==0);
        assert(t->bankAssets[source+1]>=0);
        assert(std::none_of(t->voices.begin(),t->voices.end(),[&](const auto& v){return v.active&&v.bankSlot==source+1;}));
    }
    // New state roundtrips all four sources. Old JSON A/IN selections migrate.
    t->allNotesOff();renderSplit(*t,40);
    std::array<char,kStateTextSize> json{};
    for(int source=0;source<4;++source) {
        t->bankRecordSource=source;assert(t->serializeState(json.data(),json.size())>0);
        t->bankRecordSource=2;assert(t->restoreSplitJson(json.data()));assert(t->bankRecordSource==source);
    }
    t->bankRecordSource=2;assert(t->serializeState(json.data(),json.size())>0);
    std::string old=json.data();const std::string tag=",\"bank_source_version\":2";
    const auto tagAt=old.find(tag);assert(tagAt!=std::string::npos);old.erase(tagAt,tag.size());
    const std::string key="\"bank_record_source\":";const auto valueAt=old.find(key)+key.size();
    for(int legacy:{0,1}) {
        old[valueAt]=static_cast<char>('0'+legacy);assert(t->restoreSplitJson(old.c_str()));
        assert(t->bankRecordSource==(legacy==0?2:0));
    }
    t->backgroundStarted=false;t.reset();g_host=priorHost;removeBackgroundTestDirectory(folder);
    std::puts("s3g Thru Bank IN/MIX/PT A/PT B: stereo taps, own-pad resampling, mute isolation, frozen source and JSON migration: PASS");
}


void testBankMinimumTake() {
    char folder[]="/tmp/thru-bank-minimum.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
    t->stopBackgroundWork();t->backgroundStarted=true;t->setPerformanceMode(4);
    const auto minimum=static_cast<std::size_t>(std::ceil(t->sampleRate*.010));
    t->splitBus[0][0].fill(.125f);t->splitBus[0][1].fill(-.0625f);
    const auto feed=[&](std::size_t frames){while(frames){const auto n=std::min(frames,kBlockFrames);t->recordBankBus(n);frames-=n;}};
    assert(t->startBankCapture(0));feed(minimum);t->finishBankCapture();
    assert(t->processBankJob());t->consumeBankJob();const auto asset=t->bankAssets[0];assert(asset>=0);
    t->clearBankSlot(0);assert(t->undoAvailable && t->bankUndoAsset==asset);
    for(int slot=0;slot<16;++slot)for(std::size_t frames:{std::size_t(0),std::size_t(1),minimum-1}) {
        assert(t->startBankCapture(slot));const std::string path=t->bankRecordPath.data();
        feed(frames);t->finishBankCapture();
        assert(t->bankJob.load()==0 && t->bankRecordSlot==-1 && t->bankRecordFrames==0);
        assert(!t->bankSlots[slot].path[0] && t->bankAssets[slot]<0 && access(path.c_str(),F_OK)!=0);
        assert(t->undoAvailable && t->undoIsBank && t->bankUndoIndex==0 && t->bankUndoAsset==asset);
    }
    t->restoreUndo();assert(t->bankAssets[0]==asset);
    // Filled pads still respond immediately: this is a capture guard, not a note debounce.
    t->padNoteOn(72,100);assert(t->findHeldVoiceForNote(60,4));t->padNoteOff(72);
    assert(t->bankJob.load()==0);t->allNotesOff();renderSplit(*t,40);
    t->splitBus[0][0].fill(.125f);t->splitBus[0][0][0]=.375f;
    assert(t->startBankCapture(1));feed(minimum);t->finishBankCapture();assert(t->bankJob.load()==2);
    assert(t->processBankJob());t->consumeBankJob();const auto accepted=t->bankAssets[1];assert(accepted>=0);
    assert(t->bankPool->assets[accepted].frames==minimum && t->bankPool->assets[accepted].audio[0]==.375f);
    t->backgroundStarted=false;t.reset();removeBackgroundTestDirectory(folder);
    std::puts("s3g Thru Bank 10 ms minimum: all empty pads, no saved tap/Undo loss, immediate filled-pad playback and retained attack: PASS");
}
