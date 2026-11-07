void testMidiVoiceModesAcrossPadLayouts() {
    const auto count=[](const ThruInstance& t,bool clip=false) {
        return std::count_if(t.voices.begin(),t.voices.end(),[&](const auto& v){return v.active&&v.held&&v.sourcePad<0&&v.clipOwned==clip;});
    };
    for(auto pads:{PadMode::keys,PadMode::slices,PadMode::cues,PadMode::rolls})for(int mode:{1,2}) {
        auto t=splitFixture();t->setPerformanceMode(0);t->padMode=pads;t->voiceMode=mode;
        noteMidi(*t,1,60);renderSplit(*t,12);auto* first=externalVoice(*t,0,60);assert(first);
        const auto position=first->engine.normalizedPosition();const auto envelope=first->envelope;
        noteMidi(*t,1,67);assert(count(*t)==1&&externalVoice(*t,0,67)==first);
        if(mode==2)assert(first->engine.normalizedPosition()==position&&first->envelope==envelope&&!first->stealPending);
        renderSplit(*t,4);noteMidi(*t,1,67,0);assert(first->held&&first->note==60);
        noteMidi(*t,1,60,0);assert(!first->held);
        t->allNotesOff();renderSplit(*t,32);
        noteMidi(*t,1,60);midiCc(*t,1,64,127);noteMidi(*t,1,60,0);
        noteMidi(*t,1,67);noteMidi(*t,1,67,0);assert(count(*t)==1);
        midiCc(*t,1,64,0);assert(count(*t)==0);

        // Starting Record during a mono performance must seed every held
        // key so last-note priority is reconstructible in the saved clip.
        t->allNotesOff();renderSplit(*t,32);
        noteMidi(*t,1,60);noteMidi(*t,1,67);t->beginClipRecording();
        assert(t->activeClipSlot()->clipNoteCount==2);
        renderSplit(*t,12);noteMidi(*t,1,67,0);renderSplit(*t,12);
        noteMidi(*t,1,60,0);renderSplit(*t,8);t->finishClipRecording();t->setClipPlaying(false);
        const auto& clip=*t->activeClipSlot();assert(clip.clipNoteCount==2);
        assert(clip.clipNotes[0].note==60&&clip.clipNotes[1].note==67);
        assert(clip.clipNotes[0].durationMs>clip.clipNotes[1].durationMs&&clip.clipNotes[1].durationMs>0);
        t->allNotesOff();t->applyClipCurves(clip,0);
        t->triggerClipVoice(clip,0);t->triggerClipVoice(clip,1);assert(count(*t,true)==1);
        PlaybackVoice* replay=nullptr;for(auto& v:t->voices)if(v.active&&v.held&&v.clipOwned)replay=&v;
        assert(replay&&replay->note==67);t->releaseClipVoice(1);assert(replay->held&&replay->note==60);
        t->releaseClipVoice(0);assert(!replay->held);
    }
    // Non-key pad voices share the budget but must not be absorbed into
    // the external keyboard's mono voice or its last-note priority stack.
    for(int mode:{1,2}) {
        auto t=splitFixture();t->setPerformanceMode(0);t->padMode=PadMode::slices;t->voiceMode=mode;
        noteMidi(*t,1,62);t->padNoteOn(68,100);t->padNoteOn(69,100);
        auto* first=externalVoice(*t,0,62);assert(first);
        noteMidi(*t,1,67);assert(count(*t)==1&&externalVoice(*t,0,67)==first);
        assert(std::count_if(t->voices.begin(),t->voices.end(),[](const auto& v){return v.active&&v.held&&v.sourcePad>=0;})==2);
        t->beginClipRecording();assert(t->activeClipSlot()->clipNoteCount==4);renderSplit(*t,8);
        noteMidi(*t,1,67,0);assert(first->held&&first->note==62);renderSplit(*t,8);
        noteMidi(*t,1,62,0);assert(!first->held);
        assert(std::count_if(t->voices.begin(),t->voices.end(),[](const auto& v){return v.active&&v.held&&v.sourcePad>=0;})==2);
        t->padNoteOff(68);t->padNoteOff(69);renderSplit(*t,8);
        t->finishClipRecording();t->setClipPlaying(false);t->allNotesOff();renderSplit(*t,32);
        const auto& clip=*t->activeClipSlot();t->applyClipCurves(clip,0);int highest=-1;
        for(std::size_t i=0;i<clip.clipNoteCount;++i) {
            t->triggerClipVoice(clip,i);if(clip.clipNotes[i].note==67)highest=static_cast<int>(i);
        }
        assert(highest>=0&&count(*t,true)==1);
        t->releaseClipVoice(highest);
        assert(std::count_if(t->voices.begin(),t->voices.end(),[](const auto& v){return v.active&&v.held&&v.clipOwned&&v.sourcePad>=0;})==2);
        for(const auto& v:t->voices)if(v.active&&v.held&&v.clipOwned&&v.sourcePad<0)assert(v.note==62);
    }
    for(int layout:{1,2,3,4}) {
        auto t=splitFixture();t->setPerformanceMode(layout);
        const int parts=t->bankMode?1:t->partCount();
        for(int part=0;part<parts;++part) {
            t->selectSplitPart(part,false);t->padMode=PadMode::rolls;t->voiceMode=2;
        }
        t->selectSplitPart(0,false);
        for(int part=0;part<parts;++part) {
            noteMidi(*t,part+1,60);auto* first=externalVoice(*t,part,60);assert(first);
            noteMidi(*t,part+1,67);assert(externalVoice(*t,part,67)==first&&!externalVoice(*t,part,60));
            noteMidi(*t,part+1,67,0);assert(first->note==60&&first->held&&t->splitFocus==0);
            noteMidi(*t,part+1,60,0);assert(!first->held);
        }
    }
    std::puts("s3g Thru external Mono/Legato: all pad layouts, sustain, held-key capture/clip priority, non-key pad isolation and per-part routing: PASS");
}

void testDirectEngineReservedRate() {
    auto t=splitFixture();t->setPerformanceMode(0);
    for(int algorithm=0;algorithm<15;++algorithm) {
        t->controls.algorithm=static_cast<Engine::PlaybackAlgorithm>(algorithm);int part=0;
        const auto* p=e16PageSlotParameter(*t,0,5,part);
        if(algorithm==4||algorithm==5) {
            assert(!p);char remote[16384]{};assert(formatE16RemotePageState(*t,remote,sizeof(remote),true)>0);
            assert(!std::strstr(remote,",384,"));
        } else assert(p&&!std::strcmp(p->key,algorithm==3?"cut_lane_advance":"lane_cycles"));
    }
    // Generic CC uses the same eight physical slots. A blank RATE cell
    // must not shift JOIN or LEVL into an earlier controller position.
    const char* keys[]{"algorithm","locus","field","speed","manual_lane",nullptr,"join","output_gain"};
    std::string request;
    for(const char* key:keys) {
        if(!request.empty())request+=';';
        if(!key){request+="-1:-1";continue;}
        const auto& p=midiParameter(key);request+=(p.part?"0:":"-1:")+std::to_string(p.id);
    }
    setParam(t.get(),"midi_control_mode","2");setParam(t.get(),"midi_takeover","1");
    setParam(t.get(),"midi_view",request.c_str());assert(t->midiViewCount==8&&t->midiView[5].id==-1);
    const auto join=t->controls.join,scan=t->controls.laneCycles;
    midiCc(*t,9,25,127);assert(t->controls.join==join&&t->controls.laneCycles==scan);
    midiCc(*t,9,26,0);assert(t->controls.join==0);
    midiCc(*t,9,27,127);assert(t->outputGain==2);
    t->setPerformanceMode(2);t->controls.algorithm=Engine::PlaybackAlgorithm::slices;
    t->selectSplitPart(1,false);t->controls.algorithm=Engine::PlaybackAlgorithm::motion;
    t->selectSplitPart(0,false);int a=0,b=1;
    assert(!e16PageSlotParameter(*t,0,5,a));
    assert(e16PageSlotParameter(*t,0,5,b)&&t->splitFocus==0);
    std::puts("s3g Thru reserved direct-engine RATE: Sample/Slices E16 bindings/Remote cells, Cutups ADV, moving engines, Generic CC slot alignment and A/B isolation: PASS");
}
