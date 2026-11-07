void testExclusiveMidiInbox() {
    auto t=splitFixture();t->setPerformanceMode(0);
    std::array<std::uint8_t,4096> memory{};
    host_api_v1_t host{};host.api_version=1;host.sample_rate=44100;host.frames_per_block=128;
    host.audio_in_offset=2304;host.audio_out_offset=256;host.mapped_memory=memory.data();
    struct RestoreHost {const host_api_v1_t* old;~RestoreHost(){g_host=old;}} restore{g_host};g_host=&host;
    noteMidi(*t,1,61);assert(!externalVoice(*t,0,61)); // no startup echo before the first inbox event
    const auto packet=[&](int slot,int channel,int status,int number,int value,std::uint32_t stamp,int cable=2) {
        auto* p=memory.data()+2048+slot*8;p[0]=static_cast<std::uint8_t>((cable<<4)|(status>>4));
        p[1]=static_cast<std::uint8_t>(status|channel);p[2]=number;p[3]=value;
        for(int i=0;i<4;++i)p[4+i]=static_cast<std::uint8_t>(stamp>>(i*8));
    };
    const auto clear=[&](){std::fill_n(memory.data()+2048,248,0);};
    for(int i=248;i<256;++i)memory[2048+i]=static_cast<std::uint8_t>(i);
    packet(0,0,0x90,60,100,1);packet(1,0,0x90,68,100,2,0);
    packet(7,0,0x90,64,100,3);packet(30,2,0x90,67,100,4);
    auto expected=memory;
    for(int slot:{0,7,30}) {auto* p=expected.data()+2048+slot*8;p[0]=0x28;p[1]=static_cast<std::uint8_t>(0x80|(p[1]&15));p[3]=64;}
    renderSplit(*t,1);assert(memory==expected&&t->midiInboxActive);
    assert(externalVoice(*t,0,60)&&externalVoice(*t,0,64)&&!externalVoice(*t,0,67));
    assert(t->midiKeys[0][0][0][60].down==1);
    // Stale buffers, shifted hardware slots and the native MIDI_OUT echo do
    // not turn one key press into multiple owners or immediate key releases.
    for(int frame=0;frame<10;++frame){clear();packet(0,0,0x90,60,100,1);renderSplit(*t,1);}
    assert(externalVoice(*t,0,60)&&t->midiKeys[0][0][0][60].down==1);
    clear();packet(12,0,0x90,60,100,1);renderSplit(*t,1);assert(t->midiKeys[0][0][0][60].down==1);
    noteMidi(*t,1,60);assert(t->midiKeys[0][0][0][60].down==1);
    clear();packet(0,0,0x90,60,100,5);renderSplit(*t,1);assert(t->midiKeys[0][0][0][60].down==1);
    clear();packet(0,0,0x80,60,0,6);const auto off=memory;renderSplit(*t,1);
    assert(memory==off&&t->midiKeys[0][0][0][60].down==0);
    clear();packet(0,0,0x90,60,0,7);renderSplit(*t,1);assert(!externalVoice(*t,0,60));
    clear();packet(0,0,0xb0,64,127,8);packet(1,0,0x80,64,0,9);renderSplit(*t,1);assert(externalVoice(*t,0,64));
    clear();packet(0,0,0xb0,64,0,10);packet(1,0,0xe0,127,127,11);
    packet(2,0,0x90,65,100,12);packet(3,0,0xd0,100,0xff,13);renderSplit(*t,1);
    auto* voice=externalVoice(*t,0,65);assert(voice&&!externalVoice(*t,0,64));
    assert(voice->midiTarget[0]==2&&voice->pressureTarget>0.78f);
    const auto before=t->controls.locus;setParam(t.get(),"midi_control_mode","MNLT");setParam(t.get(),"midi_takeover","Jump");
    midiCc(*t,9,17,127);assert(t->controls.locus==1&&before!=1); // controller callbacks retain their path
    clear();packet(0,0,0xb0,1,127,14);setParam(t.get(),"midi_mod_target","Stack");
    renderSplit(*t,1);assert(voice->midiTarget[1]==1);
    noteMidi(*t,1,70,100,true,MOVE_MIDI_SOURCE_HOST);assert(externalVoice(*t,0,70));
    clear();packet(0,0,0x80,65,0,15);renderSplit(*t,1);assert(!externalVoice(*t,0,65)&&externalVoice(*t,0,70));
    // Eight-step timestamp stride: reject mismatched CIN/data, leave system,
    // internal packets and the receive-status word alone.
    clear();packet(0,0,0x90,66,100,16);memory[2048]=0x2b;
    packet(1,0,0x90,67,128,17);packet(2,0,0x90,68,128,0);
    packet(3,0,0xf0,1,2,18);const auto malformed=memory;renderSplit(*t,1);assert(memory==malformed);
    t->patternRenderOnly=true;clear();packet(0,0,0x90,71,100,19);
    const auto shadowInput=memory;t->captureMidiInbox();assert(memory==shadowInput);t->patternRenderOnly=false;
    host.audio_in_offset=4096;const auto fallback=memory;noteMidi(*t,1,72);assert(externalVoice(*t,0,72));
    t->captureMidiInbox();assert(memory==fallback);
    std::puts("s3g Thru exclusive USB MIDI: raw delivery, native suppression, timestamp dedup, retriggers, expression, callback echoes, controller/Host isolation and mailbox bounds: PASS");
}

void testSequencerMidiGates() {
    auto t=splitFixture();t->setPerformanceMode(0);
    std::array<std::uint8_t,4096> memory{};
    host_api_v1_t host{};host.api_version=1;host.sample_rate=44100;host.frames_per_block=128;
    host.audio_in_offset=2304;host.audio_out_offset=256;host.mapped_memory=memory.data();
    struct RestoreGateHost {const host_api_v1_t* old;~RestoreGateHost(){g_host=old;}} restore{g_host};g_host=&host;
    const auto feed=[&](int status,int note,int value,unsigned stamp) {
        std::fill_n(memory.data()+2048,248,0);auto* p=memory.data()+2048;
        p[0]=static_cast<std::uint8_t>(0x20|(status>>4));p[1]=status;p[2]=note;p[3]=value;
        for(int i=0;i<4;++i)p[4+i]=static_cast<std::uint8_t>(stamp>>(i*8));
        renderSplit(*t,1);
    };
    // A physical release must not collide with the synthetic native note-off
    // produced for the attack. Some inbox batches share timestamp values.
    feed(0x90,60,100,100);assert(externalVoice(*t,0,60));
    feed(0x80,60,64,100);assert(!externalVoice(*t,0,60));
    feed(0x90,62,100,101);feed(0x80,62,0,0);assert(!externalVoice(*t,0,62));
    feed(0x90,64,100,102);feed(0x90,64,100,103);
    feed(0x80,64,64,104);assert(!externalVoice(*t,0,64));
    for(int event=0;event<500;++event) {
        const int note=48+event%24;
        feed(0x90,note,100,200+event*2);assert(externalVoice(*t,0,note));
        feed(0x90,note,0,201+event*2);assert(!externalVoice(*t,0,note));
    }
    // Real post-transfer batches can contain an entire short gate followed
    // by a new attack. Last-event order matters when timestamps are zero.
    feed(0x90,60,100,0);
    std::fill_n(memory.data()+2048,248,0);
    for(int slot=0;slot<3;++slot) {
        auto* p=memory.data()+2048+slot*8;p[0]=slot==1?0x28:0x29;p[1]=slot==1?0x80:0x90;
        p[2]=60;p[3]=slot==1?64:100;
    }
    renderSplit(*t,1);assert(externalVoice(*t,0,60));
    feed(0x80,60,64,0);assert(!externalVoice(*t,0,60));
    for(int event=0;event<100;++event){feed(0x90,62,100,0);assert(externalVoice(*t,0,62));feed(0x80,62,64,0);assert(!externalVoice(*t,0,62));}
    for(int blocks:{1,4,16,64,128}) {
        feed(0x90,65,100,10000+blocks*2);
        std::fill_n(memory.data()+2048,248,0);renderSplit(*t,blocks);
        assert(externalVoice(*t,0,65));feed(0x80,65,64,10001+blocks*2);assert(!externalVoice(*t,0,65));
    }
    std::puts("s3g Thru sequencer gates: real releases cannot collide with native suppression; zero timestamps, retriggers and 500 timed gates: PASS");
}

void testMidiPlayheadParity() {
    auto pads=splitFixture(),midi=splitFixture();
    for(auto* t:{pads.get(),midi.get()}) {
        t->setPerformanceMode(0);t->controls.algorithm=Engine::PlaybackAlgorithm::motion;
        t->controls.manualLane=1.25f;t->controls.laneCycles=1;t->controls.speed=1;
    }
    const int note=pads->padToMidiNote(68);pads->padNoteOn(68,100);noteMidi(*midi,1,note);
    auto* pv=pads->findHeldVoiceForNote(note,0);auto* mv=externalVoice(*midi,0,note);assert(pv&&mv);
    assert(std::abs(pads->effectiveControls(*pv).lanePhaseOffset-midi->effectiveControls(*mv).lanePhaseOffset)<1e-6f);
    for(int block=0;block<100;++block){renderSplit(*pads,1);renderSplit(*midi,1);}
    const auto position=mv->engine.normalizedPosition(),phase=midi->currentVoiceStackPhase(*mv);
    renderSplit(*pads,10);renderSplit(*midi,10);
    assert(std::abs(mv->engine.normalizedPosition()-position)>.001f);
    assert(std::abs(midi->currentVoiceStackPhase(*mv)-phase)>.001f);
    assert(std::abs(pv->engine.normalizedPosition()-mv->engine.normalizedPosition())<.0001f);
    assert(std::abs(pads->currentVoiceStackPhase(*pv)-midi->currentVoiceStackPhase(*mv))<.0001f);
    midi->setStackScanPaused(true);const auto heldPhase=midi->currentVoiceStackPhase(*mv);
    const auto heldPosition=mv->engine.normalizedPosition();renderSplit(*midi,10);
    assert(std::abs(midi->currentVoiceStackPhase(*mv)-heldPhase)<1e-6f);
    assert(std::abs(mv->engine.normalizedPosition()-heldPosition)>.001f);
    noteMidi(*midi,1,note,0);assert(!mv->held);pads->padNoteOff(68);
    midi->setStackScanPaused(false);midi->clipPlaying=true;
    noteMidi(*midi,1,66);noteMidi(*midi,1,67,100,true,MOVE_MIDI_SOURCE_HOST);
    midi->padNoteOn(68,100);realtimeMidi(*midi,0xfc);
    assert(!externalVoice(*midi,0,66)&&externalVoice(*midi,0,67)&&midi->clipPlaying);
    assert(midi->findHeldVoiceForNote(note,0));
    std::puts("s3g Thru MIDI/pad parity: manual stack start, independent playhead and stack motion, explicit scan hold and gate release: PASS");
}
