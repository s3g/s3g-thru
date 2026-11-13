// Reproducibility is measured on actual stereo engine output, with modulation
// and every probabilistic Pulsar control enabled. Live notes retain variability.
Engine::Controls recallPulsarProgram() {
    Engine::Controls c;
    c.algorithm=Engine::PlaybackAlgorithm::pulsar;
    c.mix=c.join=1;c.locus=.5f;c.field=.8f;c.lanePath=Engine::LanePath::manual;
    c.pulsarRate=47;c.pulsarForm=90;c.pulsarSize=60;
    c.pulsarJitter=.42f;c.pulsarOmit=.23f;c.pulsarSpray=.7f;
    c.pulsarFormVariation=7;c.pulsarReverse=.37f;
    c.pulsarEmitModRate=1.3f;c.pulsarEmitModDepth=4;
    c.pulsarFormModRate=2.1f;c.pulsarFormModDepth=6;
    return c;
}
void testPulsarSeededAudio() {
    for(bool prepared:{false,true}) {
        auto f=std::make_unique<StochasticFixture>(44100,prepared);
        auto c=recallPulsarProgram();c.pulsarSeed=0xfedcba98u;
        const auto render=[&](const Engine::Controls& program) {
            f->engine.stopPlayback();f->engine.retrigger(false);
            std::vector<float> audio;
            for(int block=0;block<140;++block) {
                f->render(program);
                for(int ch=0;ch<2;++ch)for(float v:f->output[ch])audio.push_back(v);
            }
            return audio;
        };
        const auto a=render(c);render(Engine::Controls{});
        const auto b=render(c);assert(a==b);
        auto different=c;different.pulsarSeed^=0x12345u;assert(render(different)!=a);
        c.pulsarSeed=0;const auto live=render(c);assert(render(c)!=live);
        double energy=0;for(float v:a){assert(std::isfinite(v));energy+=v*v;}assert(energy>1);
    }
    std::puts("Pulsar seeded stereo audio: exact recall after unrelated resets, prepared/fallback PCM, modulation, omission, jitter, scatter, reverse and fresh live streams: PASS");
}
void testPulsarRecordedIdentity() {
    auto t=splitFixture();t->stopBackgroundWork();t->setPerformanceMode(0);
    t->controls=recallPulsarProgram();t->captureActiveClip(true);
    auto& clip=*t->activeClipSlot();clip.clipRecorded=true;clip.clipDurationMs=250;
    clip.clipNoteCount=1;clip.clipNotes[0]={};auto& event=clip.clipNotes[0];
    event.note=60;event.velocity=100;event.durationMs=200;event.sourcePad=0;
    t->activateStoredClip(t->activeClip,false);
    const auto seeds=clip.pulsarSeeds;
    const auto first=t->recordedPulsarSeed(clip,event,0,0);
    assert(first&&first!=t->recordedPulsarSeed(clip,event,0,1));
    auto edited=event;edited.velocity=50;edited.durationMs=120;
    assert(first==t->recordedPulsarSeed(clip,edited,0,0));
    edited=event;edited.note++;assert(first!=t->recordedPulsarSeed(clip,edited,0,0));
    edited=event;edited.take++;assert(first!=t->recordedPulsarSeed(clip,edited,0,0));
    assert(first!=t->recordedPulsarSeed(clip,event,1,0));
    t->triggerClipVoice(clip,0);
    auto* original=&t->voices[0];assert(original->clipOwned&&original->pulsarSeed==first);
    assert(t->effectiveControls(*original).pulsarSeed==first);
    // Force another physical voice and then stealing through all eight slots.
    t->triggerClipVoice(clip,0);assert(t->voices[1].pulsarSeed==first);
    for(int i=0;i<16;++i)t->triggerClipVoice(clip,0);
    for(auto& voice:t->voices) {
        assert(voice.clipOwned&&voice.pulsarSeed==first);
        assert(t->effectiveControls(voice).pulsarSeed==first);
        voice.clipOwned=false;assert(t->effectiveControls(voice).pulsarSeed==0);
        voice.clipOwned=true;
    }
    t->setClipPlaying(false);renderSplit(*t,30);t->startClipPlaybackNow();
    assert(t->pulsarClipCycle==0);renderSplit(*t,1);
    bool heard=false;for(const auto& voice:t->voices)if(voice.active&&voice.held&&voice.clipOwned) {
        assert(voice.pulsarSeed==first);heard=true;
    }assert(heard);
    renderSplit(*t,180);assert(t->pulsarClipCycle>=1);
    const auto cycle=t->pulsarClipCycle;
    heard=false;for(const auto& voice:t->voices)if(voice.active&&voice.held&&voice.clipOwned) {
        assert(voice.pulsarSeed==t->recordedPulsarSeed(clip,event,0,cycle));heard=true;
    }assert(heard);
    t->setClipPlaying(false);t->startClipPlaybackNow();assert(t->pulsarClipCycle==0);
    assert(t->recordedPulsarSeed(clip,event,0,0)==first);
    t->setClipPlaying(false);assert(t->copyClipToClipboard(0));t->pasteClipFromClipboard(7);
    assert(t->clipSlots[7].pulsarSeeds==seeds);
    t->activateStoredClip(7,false);
    assert(t->recordedPulsarSeed(t->clipSlots[7],t->clipSlots[7].clipNotes[0],0,0)==first);
    // Seeded readers are not restarted by legato note changes.
    t->voiceMode=2;t->clipSlots[7].clipNoteCount=2;
    t->clipSlots[7].clipNotes[1]=event;t->clipSlots[7].clipNotes[1].note=64;
    t->releaseClipVoices();renderSplit(*t,100);t->triggerClipVoice(t->clipSlots[7],0);
    auto* legato=static_cast<PlaybackVoice*>(nullptr);
    for(auto& v:t->voices)if(v.active&&v.held&&v.clipOwned)legato=&v;
    assert(legato);const auto stream=legato->pulsarSeed;
    t->triggerClipVoice(t->clipSlots[7],1);assert(legato->note==64&&legato->pulsarSeed==stream);
    t->setClipPlaying(false);t->pulsarClipCycle=9;
    t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();assert(t->pulsarClipCycle==0);
    renderSplit(*t,200);t->finishClipRecording();assert(t->pulsarClipCycle==0);
    std::puts("Pulsar recorded streams: semantic note/part/take identity, loop evolution, fresh Play, copy/paste, eight voices, stealing, live isolation and legato continuity: PASS");
}
void testPulsarClipAudioRecall() {
    auto t=splitFixture();t->stopBackgroundWork();t->setPerformanceMode(0);
    t->controls=recallPulsarProgram();t->controls.ampReleaseMs=6;t->captureActiveClip(true);
    auto& clip=*t->activeClipSlot();clip.clipRecorded=true;clip.clipDurationMs=1000;
    clip.clipNoteCount=2;
    for(int i=0;i<2;++i) {
        clip.clipNotes[i]={};clip.clipNotes[i].note=60+i*7;
        clip.clipNotes[i].sourcePad=i;clip.clipNotes[i].velocity=100;
        clip.clipNotes[i].durationMs=900;
    }
    t->activateStoredClip(t->activeClip,false);
    // Compare the same prepared PCM path. Initial background anti-alias
    // analysis deliberately crossfades from the immediate fallback reader.
    for(int n=0;n<1000&&t->wavesetReadyMask.load()!=15;++n){renderSplit(*t,1);usleep(1000);}
    assert(t->wavesetReadyMask.load()==15);
    const auto play=[&] {
        t->startClipPlaybackNow();std::vector<float> audio;
        for(int block=0;block<120;++block) {
            renderSplit(*t,1);
            for(int ch=0;ch<2;++ch)for(std::size_t frame=0;frame<kBlockFrames;++frame) {
                float sum=0;for(const auto& v:t->voices)if(v.active&&v.clipOwned)sum+=v.output[ch][frame];
                audio.push_back(sum);
            }
        }
        t->setClipPlaying(false);renderSplit(*t,200);return audio;
    };
    const auto a=play();
    t->padNoteOn(67,100);renderSplit(*t,80);t->padNoteOff(67);renderSplit(*t,100);
    const auto b=play();
    assert(a.size()==b.size());float difference=0;double energy=0;
    for(std::size_t i=0;i<a.size();++i){difference=std::max(difference,std::abs(a[i]-b[i]));energy+=a[i]*a[i];}
    std::fprintf(stderr,"Pulsar polyphonic clip fresh-Play raw stereo max difference: %.9g\n",difference);
    assert(energy>1&&difference<1e-6f);
    std::puts("Pulsar complete clip/audio path: identical polyphonic onset, phases and random choices after unrelated live playing: PASS");
}
void testPulsarSequenceIdentity() {
    auto t=sequenceFixture();t->controls=recallPulsarProgram();t->captureActiveClip(true);
    t->sequencePlayback=t->sequenceLive;
    t->startSequenceVoice(0,0,0,.25,false,0);
    std::uint32_t first=0;
    for(const auto& voice:t->voices)if(voice.active&&voice.sequence.active) {
        first=voice.pulsarSeed;assert(first&&first==t->sequencePulsarSeed(0,0,0,0));
    }assert(first);
    t->startSequenceVoice(1,0,0,.25,false,0); // unrelated lane has no influence
    t->startSequenceVoice(0,0,0,.25,false,0);
    for(const auto& voice:t->voices)if(voice.active&&voice.part==0)assert(voice.pulsarSeed==first);
    assert(first!=t->sequencePulsarSeed(0,0,1,0)); // burst child
    assert(first!=t->sequencePulsarSeed(0,0,0,32)); // later track cycle
    t->resetSequenceClock();assert(first==t->sequencePulsarSeed(0,0,0,0));
    t->startSequenceVoice(3,0,0,.25,true,0);
    for(const auto& voice:t->voices)if(voice.active&&voice.part==3)assert(t->effectiveControls(voice).pulsarSeed==0);
    std::puts("Pulsar Sequence streams: stable track tick and burst child, independent lanes, fresh clock and live audition: PASS");
}
void testPulsarSeedPersistence() {
    char directory[]="/tmp/thru-pulsar-recall.XXXXXX";assert(mkdtemp(directory));
    auto saved=std::make_unique<ThruInstance>();assert(saved->prepare(directory));saved->stopBackgroundWork();
    const std::array<std::uint32_t,4> seeds{0xffffffffu,0x80000001u,0xabcdef01u,123456789u};
    saved->activeClipSlot()->pulsarSeeds=seeds;saved->captureActiveClip(true);
    saved->initializeClipIfNeeded(17);saved->clipSlots[17].pulsarSeeds={1u,2u,3u,4u};
    auto json=std::make_unique<std::array<char,kStateTextSize>>();
    assert(saved->serializeState(json->data(),json->size())>0);
    auto copy=std::make_unique<ThruInstance>();assert(copy->prepare(nullptr));copy->stopBackgroundWork();
    assert(copy->restoreState(json->data()));assert(copy->activeClipSlot()->pulsarSeeds==seeds);
    for(const char* invalid:{"0","-1","4294967296","1.5","1e3"}) {
        std::string broken=json->data();const auto at=broken.find("\"pulsar_seed_0\":");assert(at!=std::string::npos);
        const auto begin=broken.find(':',at)+1,end=broken.find(',',begin);
        broken.replace(begin,end-begin,invalid);
        assert(!copy->restoreState(broken.c_str()));assert(copy->activeClipSlot()->pulsarSeeds==seeds);
    }

    ThruInstance::PersistentStateSnapshot snapshot;snapshot.capture(*saved);
    saved->activeClipSlot()->pulsarSeeds={4,3,2,1};
    assert(snapshot.pulsarSeeds==seeds);saved->activeClipSlot()->pulsarSeeds=seeds;
    assert(saved->savePersistentState());saved.reset();
    saved=std::make_unique<ThruInstance>();assert(saved->prepare(directory));saved->stopBackgroundWork();
    assert(saved->persistenceRestored&&saved->activeClipSlot()->pulsarSeeds==seeds);
    assert((saved->clipSlots[17].pulsarSeeds==std::array<std::uint32_t,4>{1,2,3,4}));
    // A real format-41 bank with checksum receives deterministic defaults.
    const auto current=std::string(directory)+"/thru-clips.bin",old=std::string(directory)+"/old.bin";
    auto file=std::fopen(current.c_str(),"rb");assert(file);assert(std::fseek(file,0,SEEK_END)==0);
    const auto size=std::ftell(file);std::rewind(file);std::vector<unsigned char> bytes(std::size_t(size),0);
    assert(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);
    bytes.resize(bytes.size()-4);stripPulsarSeedExtension(bytes);assert(bytes[8]==41);
    file=std::fopen(old.c_str(),"wb");assert(file);assert(std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size());
    assert(std::fclose(file)==0&&appendFileChecksum(old.c_str()));
    assert(saved->readClipFileFromPath(old.c_str()));
    const auto defaults=saved->clipSlots[17].pulsarSeeds;
    assert(saved->readClipFileFromPath(old.c_str()));assert(defaults==saved->clipSlots[17].pulsarSeeds);
    for(int part=0;part<4;++part)assert(defaults[part]==ThruInstance::defaultPulsarSeed(17,part));
    // Reject corrupt seeds rather than silently replacing an authored stream.
    file=std::tmpfile();assert(file);assert(ThruInstance::writePulsarSeeds(file,saved->clipSlots));
    assert(std::fseek(file,4,SEEK_SET)==0&&writeU32(file,0));std::rewind(file);
    auto bank=std::make_unique<std::array<ClipSlot,kClipSlotCount>>();
    assert(!ThruInstance::readPulsarSeeds(file,*bank));std::fclose(file);
    saved.reset();copy.reset();removeBackgroundTestDirectory(directory);
    std::puts("Pulsar seeds: exact unsigned JSON and frozen snapshot, full multi-clip save/reopen, format-41 migration and corrupt-stream rejection: PASS");
}
void testPulsarFadeRecall() {
    auto t=splitFixture();t->setPerformanceMode(0);t->stopSampleWorker();
    t->controls=recallPulsarProgram();t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
    t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);
    auto& clip=t->clipSlots[0];clip.clipRecorded=true;clip.clipDurationMs=4000;clip.clipNoteCount=1;
    clip.clipNotes[0]={};clip.clipNotes[0].note=60;clip.clipNotes[0].sourcePad=0;
    clip.clipNotes[0].velocity=100;clip.clipNotes[0].durationMs=3900;
    t->clipSlots[1]=clip;t->activateStoredClip(0,false);t->startClipPlaybackNow();renderSplit(*t,10);
    t->pulsarClipCycle=7;const auto seeds=clip.pulsarSeeds;
    std::uint32_t activeStream=0;for(const auto& v:t->voices)if(v.active&&v.held)activeStream=v.pulsarSeed;
    assert(activeStream);t->patternFadeMs=1000;
    assert(t->prepareClipBank(1));t->clipPrepareState.store(int(ClipPrepareState::ready));
    assert(t->activatePreparedClip());
    assert(t->patternShadow->pulsarClipCycle==7&&t->pulsarClipCycle==0);
    assert(t->patternShadow->activeClipSlot()->pulsarSeeds==seeds);
    assert(t->patternShadow->recordedPulsarSeed(*t->patternShadow->activeClipSlot(),clip.clipNotes[0],0,7)
        ==t->recordedPulsarSeed(clip,clip.clipNotes[0],0,7));
    std::array<float,kBlockFrames*2> audio{};t->render(nullptr,kBlockFrames,audio.data());t->setPatternPosition(.5f);
    for(int n=0;n<8;++n){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
    t->returnPatternSource();renderSourceReturn(*t,audio);
    assert(t->activeClip==0&&t->pulsarClipCycle==7&&t->activeClipSlot()->pulsarSeeds==seeds);
    bool found=false;for(const auto& v:t->voices)if(v.active&&v.held) {
        assert(v.pulsarSeed==activeStream);found=true;
    }assert(found);
    std::puts("Pulsar timed fade and Return: saved streams, source loop ordinal, held reader identity and incoming fresh start: PASS");
}
