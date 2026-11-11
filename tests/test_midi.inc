void e16UiButtons(ThruInstance& t,unsigned mask) {
    char value[32]{};std::snprintf(value,sizeof(value),"%u",mask);
    setParam(&t,"e16_remote_buttons",value);
}

const MidiParameter& midiParameter(const char* key) {
    for(const auto& p:s3g_thru_midi::parameters)if(std::strcmp(p.key,key)==0)return p;
    assert(false);return s3g_thru_midi::parameters[0];
}
void midiCc(ThruInstance& t,int channel,int cc,int value,int source=MOVE_MIDI_SOURCE_EXTERNAL) {
    const std::uint8_t bytes[]{static_cast<std::uint8_t>(0xb0+channel-1),static_cast<std::uint8_t>(cc),static_cast<std::uint8_t>(value)};
    onMidi(&t,bytes,3,source);
}
void nrpn(ThruInstance& t,int channel,const char* key,int raw) {
    const auto& p=midiParameter(key);
    midiCc(t,channel,99,p.id>>7);midiCc(t,channel,98,p.id&127);
    midiCc(t,channel,6,raw>>7);midiCc(t,channel,38,raw&127);
}
void nrpnId(ThruInstance& t,int channel,int id,int raw) {
    midiCc(t,channel,99,id>>7);midiCc(t,channel,98,id&127);
    midiCc(t,channel,6,raw>>7);midiCc(t,channel,38,raw&127);
}
void testNrpnAndMidiViews() {
    s3g_thru_midi::Decoder decoder;int calls=0,id=-1,raw=-1;
    const auto apply=[&](int p,int v){++calls;id=p;raw=v;};
    decoder.cc(99,3,apply);decoder.cc(98,4,apply);decoder.cc(6,5,apply);
    assert(calls==0);decoder.cc(38,6,apply);assert(calls==1&&id==388&&raw==646);
    decoder.cc(38,7,apply);assert(calls==1); // no repeated stale coarse value
    decoder.cc(6,8,apply);decoder.cc(99,1,apply);decoder.cc(38,9,apply);assert(calls==1);
    decoder.cc(98,2,apply);decoder.cc(6,10,apply);decoder.cc(101,0,apply);decoder.cc(38,11,apply);assert(calls==1);
    decoder.cc(99,127,apply);decoder.cc(98,127,apply);decoder.cc(6,127,apply);decoder.cc(38,127,apply);assert(calls==1);

    for(int mode:{1,2,3})for(int focus:{0,1}) {
        auto t=splitFixture();t->setPerformanceMode(mode);t->selectSplitPart(focus,false);
        t->selectSplitPart(0,false);t->controls.locus=.2f;
        t->selectSplitPart(1,false);t->controls.locus=.7f;
        t->selectSplitPart(focus,false);const auto lane=t->selectedLane;
        const auto& position=midiParameter("locus");
        midiCc(*t,6,99,position.id>>7);midiCc(*t,6,98,position.id&127);midiCc(*t,6,6,100);
        assert(t->liveControlsForPart(1).locus==.7f);
        midiCc(*t,6,38,1);
        assert(std::abs(t->liveControlsForPart(1).locus-12801.f/16383)<.000001f);
        assert(t->liveControlsForPart(0).locus==.2f&&t->splitFocus==focus&&t->selectedLane==lane);
        nrpn(*t,6,"locus",12802);assert(t->liveControlsForPart(1).locus>12801.f/16383);
        nrpn(*t,16,"locus",0);assert(t->liveControlsForPart(0).locus==.2f); // wrong scope
        nrpn(*t,5,"output_gain",0);assert(t->outputGain==1);
        nrpn(*t,16,"output_gain",8192);assert(std::abs(t->outputGain-1)<.00001f);
        const auto& speed=midiParameter("speed");
        nrpn(*t,6,"speed",s3g_thru_midi::encode(speed,1,speed.maximum));
        assert(t->liveControlsForPart(1).speed==1); // unity is exact
        nrpn(*t,6,"formant_pitch",s3g_thru_midi::encode(midiParameter("formant_pitch"),0,24));
        assert(t->liveControlsForPart(1).formantPitch==0);
        nrpn(*t,6,"part_level",4096);
        assert(std::abs(t->controls.splitLevelB-8192.f/16383)<.00001f&&t->controls.splitLevelA==1);
        const auto revision=t->midiRevision;
        nrpn(*t,6,"part_level",4096);assert(t->midiRevision==revision); // feedback echo
        if(mode==3) {
            nrpn(*t,8,"locus",12000);assert(std::abs(t->liveControlsForPart(3).locus-12000.f/16383)<.00001f);
            const auto& stack=midiParameter("manual_lane");
            assert(s3g_thru_midi::encode(stack,1,1)==0&&s3g_thru_midi::decode(stack,16383,1)==1);
        }
        char request[128]{};
        std::snprintf(request,sizeof(request),"0:%d;1:%d;-1:%d",position.id,position.id,midiParameter("output_gain").id);
        setParam(t.get(),"midi_view",request);
        char state[4096]{};assert(getParam(t.get(),"midi_state",state,sizeof(state))>0);
        assert(std::strstr(state,"\"values\":[[0,")&&std::strstr(state,"[1,")&&std::strstr(state,"[-1,"));
        assert(t->splitFocus==focus&&t->selectedLane==lane);
        const auto count=t->midiViewCount;
        setParam(t.get(),"midi_view","0:16383");assert(t->midiViewCount==count);
        char shortBuffer[8]{};assert(getParam(t.get(),"midi_state",shortBuffer,sizeof(shortBuffer))==-1&&shortBuffer[0]==0);
        char feedback[4096]{};assert(getParam(t.get(),"midi_feedback",feedback,sizeof(feedback))>0);
        bool gotB=std::strstr(feedback,"[6,")!=nullptr;
        for(int batch=0;batch<8&&!gotB;++batch) {
            assert(getParam(t.get(),"midi_feedback",feedback,sizeof(feedback))>0);
            gotB=std::strstr(feedback,"[6,")!=nullptr;
        }
        assert(gotB);
        // Sources cannot finish each other's partial NRPN transactions.
        midiCc(*t,6,99,position.id>>7);midiCc(*t,6,98,position.id&127);midiCc(*t,6,6,20);
        const auto before=t->liveControlsForPart(1).locus;
        midiCc(*t,6,38,99,MOVE_MIDI_SOURCE_HOST);assert(t->liveControlsForPart(1).locus==before);
        midiCc(*t,6,38,99);assert(std::abs(t->liveControlsForPart(1).locus-2659.f/16383)<.00001f);
        assert(t->splitFocus==focus);
    }
    std::puts("s3g Thru NRPN transactions, 14-bit precision, scopes and bounded views: PASS");
}

void testMidiScopeRegistryAndScene() {
    for(const auto& p:s3g_thru_midi::parameters) {
        const int index=lockParameterIndex(p.key);
        if(index>=0)assert(p.part==ThruInstance::splitPartParameter(index));
        assert(s3g_thru_midi::find(p.id)==&p);
    }
    for(int mode:{0,1,2,3}) {
        auto t=splitFixture();t->setPerformanceMode(mode);t->selectSplitPart(0,false);
        const auto lane=t->selectedLane;
        const auto press=[&](int channel,int note,int velocity=127) {
            const std::uint8_t bytes[]{static_cast<std::uint8_t>(0x90+channel-1),static_cast<std::uint8_t>(note),static_cast<std::uint8_t>(velocity)};
            onMidi(t.get(),bytes,3,MOVE_MIDI_SOURCE_EXTERNAL);
        };
        press(5,0);assert(t->settingsForPart(0).stackHold);
        press(5,0,0);assert(t->settingsForPart(0).stackHold); // release does not toggle
        press(5,0);assert(!t->settingsForPart(0).stackHold);
        press(6,0);assert(t->settingsForPart(1).stackHold==(mode>0));
        assert(!t->settingsForPart(0).stackHold&&t->splitFocus==0&&t->selectedLane==lane);
        for(const auto& v:t->voices)assert(!v.held);
        press(16,0);assert(t->patternHeld);press(16,0,0);assert(t->patternHeld);
        press(16,0);assert(!t->patternHeld);
        nrpn(*t,16,"pattern_fade_position",3000);assert(t->patternHeld);
        press(16,1);assert(!t->patternHeld); // idle Done disarms hold
        const auto epoch=t->midiFeedbackEpoch;
        press(16,2);press(16,2,0);assert(t->midiFeedbackEpoch==epoch+1);
        char state[1024]{};assert(getParam(t.get(),"midi_state",state,sizeof(state))>0);
        char expected[48]{};std::snprintf(expected,sizeof(expected),"\"epoch\":%u",epoch+1);assert(std::strstr(state,expected));
        // Registry defaults receive feedback before a controller edits them.
        assert(!t->midiFeedbackInitialized);
        char feedback[4096]{};
        constexpr auto count=sizeof(s3g_thru_midi::startupFeedback)/sizeof(s3g_thru_midi::startupFeedback[0]);
        std::array<bool,count> seen{};
        for(int page=0;page<12;++page) {
            assert(getParam(t.get(),"midi_feedback",feedback,sizeof(feedback))>0);
            for(std::size_t i=0;i<count;++i) {
                const auto& entry=s3g_thru_midi::startupFeedback[i];
                std::snprintf(expected,sizeof(expected),"[%d,%d,",entry.part<0?16:entry.part+5,entry.id);
                if(std::strstr(feedback,expected))seen[i]=true;
            }
        }
        for(std::size_t i=0;i<count;++i)assert(seen[i]==(s3g_thru_midi::startupFeedback[i].part<(mode>0?t->partCount():1)));
        assert(t->splitFocus==0&&t->selectedLane==lane);
    }
    auto t=splitFixture();t->selectSplitPart(0,false);
    t->beginClipRecording();nrpn(*t,6,"locus",5652);
    assert(t->splitFocus==0&&std::abs(t->liveControlsForPart(1).locus-5652.f/16383)<.00001f);
    assert(t->activeClipSlot()->clipPointCount>0);t->finishClipRecording();
    std::puts("s3g Thru scene pushes, initial feedback, sync, scopes and recorded NRPN gestures: PASS");
}

void testE16RemoteControl() {
    auto t=splitFixture();t->setPerformanceMode(2);t->selectSplitPart(0,false);
    const int focus=t->splitFocus;const auto lane=t->selectedLane;
    setParam(t.get(),"e16_remote","0,1");assert(t->e16RemoteRequested&&!t->e16RemoteActive);
    const float waiting=t->liveControlsForPart(0).locus;midiCc(*t,1,2,1);
    assert(t->liveControlsForPart(0).locus==waiting);
    setParam(t.get(),"e16_remote","1,1");assert(t->e16RemoteActive);
    midiCc(*t,1,2,1);assert(t->liveControlsForPart(0).locus>waiting);
    const float coarseStart=t->liveControlsForPart(0).field;
    e16UiButtons(*t,1u<<2);midiCc(*t,1,3,127);const float fine=t->liveControlsForPart(0).field;
    e16UiButtons(*t,0);midiCc(*t,1,3,127);const float coarse=t->liveControlsForPart(0).field;
    assert(coarseStart-fine>0&&fine-coarse>(coarseStart-fine)*4);
    assert(t->splitFocus==focus&&t->selectedLane==lane);

    t->e16Page=3;e16UiButtons(*t,1u<<16);
    char state[4096]{};assert(getParam(t.get(),"e16_remote_state",state,sizeof(state))>0&&std::strstr(state,"\"shift\":1"));
    midiCc(*t,1,1,1);assert(t->e16Page==4);
    midiCc(*t,1,1,127);assert(t->e16Page==3);
    t->e16Page=11;midiCc(*t,1,1,1);assert(t->e16Page==0);
    e16UiButtons(*t,0);
    assert(getParam(t.get(),"e16_remote_state",state,sizeof(state))>0);
    assert(std::strstr(state,"\"active\":1")&&std::strstr(state,"\"pages\":[0,1,2,3,4,5,6,7,8,9,10,11]"));
    setParam(t.get(),"e16_remote_page","10");assert(t->e16Page==10);
    setParam(t.get(),"e16_remote_page","12");assert(t->e16Page==10);
    const float jumpGuard=t->liveControlsForPart(0).locus;
    e16UiButtons(*t,1u<<17);midiCc(*t,1,2,8);assert(t->liveControlsForPart(0).locus==jumpGuard);
    e16UiButtons(*t,0);assert(t->splitFocus==focus&&t->selectedLane==lane);
    setParam(t.get(),"e16_remote_page","0");assert(t->e16Page==0);
    t->setPerformanceMode(3);setParam(t.get(),"e16_remote_pair","2");assert(t->e16Pair==2);
    const auto d=t->liveControlsForPart(3).algorithm;midiCc(*t,1,9,1);
    assert(static_cast<int>(t->liveControlsForPart(3).algorithm)==static_cast<int>(d)+1);
    setParam(t.get(),"e16_remote","0,0");assert(!t->e16RemoteActive&&!t->e16RemoteRequested&&!t->e16RemoteButtons);
    setParam(t.get(),"e16_remote","1,1");t->e16RemoteLease=1;
    const std::uint8_t turn[]{0xb0,2,1};assert(!handleE16RemoteMidi(*t,turn,3,MOVE_MIDI_SOURCE_EXTERNAL));
    assert(!t->e16RemoteActive&&!t->e16RemoteRequested);
    std::puts("s3g Thru Remote fixed pages, direct/fine turns, focus, Quad pair, ownership and lease: PASS");
}

void testSelectableMidiModesAndMonolit() {
    auto t=splitFixture();t->setPerformanceMode(2);t->selectSplitPart(0,false);
    const int focus=t->splitFocus;const auto lane=t->selectedLane;
    const auto& position=midiParameter("locus");
    char view[64]{};std::snprintf(view,sizeof(view),"0:%d",position.id);
    setParam(t.get(),"midi_view",view);
    t->controls.locus=.25f;midiCc(*t,1,20,127);
    assert(t->controls.locus==.25f); // obsolete hard-coded CC map is not routed
    setParam(t.get(),"midi_control_mode","Generic CC");
    setParam(t.get(),"midi_cc_channel","9");
    setParam(t.get(),"midi_cc_base","20");
    setParam(t.get(),"midi_takeover","Jump");
    t->controls.locus=.25f;
    midiCc(*t,9,20,127);assert(t->liveControlsForPart(0).locus==1.0f);
    midiCc(*t,8,20,0);assert(t->liveControlsForPart(0).locus==1.0f);
    nrpn(*t,5,"locus",0);assert(t->liveControlsForPart(0).locus==1.0f);
    // Parameter-control channels never fall back to playing Part A.
    const std::uint8_t on[]{0x94,60,100};onMidi(t.get(),on,3,MOVE_MIDI_SOURCE_EXTERNAL);
    bool held=false;for(const auto& voice:t->voices)held|=voice.held&&voice.note==60;
    assert(!held);t->allNotesOff();

    setParam(t.get(),"midi_takeover","Pickup");
    t->controls.locus=.5f;resetMidiControlPickups(*t);
    midiCc(*t,9,20,0);assert(t->liveControlsForPart(0).locus==.5f);
    midiCc(*t,9,20,64);assert(std::abs(t->liveControlsForPart(0).locus-64.f*129.f/16383.f)<.00001f);
    midiCc(*t,9,20,127);assert(t->liveControlsForPart(0).locus==1.0f);
    setParam(t.get(),"midi_control_mode","Off");
    midiCc(*t,9,20,0);assert(t->liveControlsForPart(0).locus==1.0f);

    setParam(t.get(),"midi_control_mode","NRPN");
    midiCc(*t,9,20,0);assert(t->liveControlsForPart(0).locus==1.0f);
    nrpn(*t,5,"locus",0);assert(t->liveControlsForPart(0).locus==0.0f);

    setParam(t.get(),"midi_control_mode","MNLT");
    setParam(t.get(),"midi_takeover","Jump");
    // Bank A, slider 2 is POS. Channels 9 and 10 address A and B at once.
    midiCc(*t,9,17,127);assert(t->liveControlsForPart(0).locus==1.0f);
    midiCc(*t,10,17,0);assert(t->liveControlsForPart(1).locus==0.0f);
    assert(t->splitFocus==focus&&t->selectedLane==lane);
    assert(t->liveControlsForPart(0).locus==1.0f);
    midiCc(*t,11,17,127);assert(t->liveControlsForPart(0).locus==1.0f
        && t->liveControlsForPart(1).locus==0.0f); // C is unavailable in Split
    // Channel 13 buttons 16 and 17 toggle Stack Hold for A and B.
    midiCc(*t,13,16,127);assert(t->settingsForPart(0).stackHold);
    midiCc(*t,13,16,0);assert(t->settingsForPart(0).stackHold);
    midiCc(*t,13,17,127);assert(t->settingsForPart(1).stackHold);
    midiCc(*t,13,17,0);assert(t->settingsForPart(1).stackHold);
    midiCc(*t,13,17,127);assert(!t->settingsForPart(1).stackHold);
    // Bank G and the Rotary extension remain part-specific. The same CC
    // numbers on channel 13 address the shared Performance bank.
    midiCc(*t,10,64,0);assert(t->controls.splitLevelB==0.0f);
    midiCc(*t,10,64,127);assert(t->controls.splitLevelB==2.0f);
    midiCc(*t,9,72,127);assert(t->controls.effectCrossPan==1.0f);
    midiCc(*t,10,73,127);assert(t->splitParts[1].controls.effectRotaryRateHz==8.0f);
    midiCc(*t,10,74,0);assert(t->splitParts[1].controls.effectRotaryDepth==0.0f);
    midiCc(*t,10,75,0);assert(t->splitParts[1].controls.effectRotarySpread==0.0f);
    midiCc(*t,10,79,127);assert(t->splitParts[1].controls.effectRotaryMix==1.0f);
    // Shared A/B crossfade is fixed at channel 13 CC 24. It does not change
    // Move's current part focus and remains independent of part effect CCs.
    midiCc(*t,13,24,0);assert(t->controls.splitBlend==0.0f);
    midiCc(*t,13,24,64);assert(std::abs(t->controls.splitBlend-64.f*129.f/16383.f)<.00001f);
    midiCc(*t,13,24,127);assert(t->controls.splitBlend==1.0f);
    midiCc(*t,13,25,0);assert(t->controls.splitCurve==0);
    midiCc(*t,13,25,127);assert(t->controls.splitCurve==1);
    setParam(t.get(),"midi_control_mode","E16 + MNLT");
    midiCc(*t,13,26,0);assert(t->controls.splitPriority==-24);
    midiCc(*t,13,26,127);assert(t->controls.splitPriority==24);
    midiCc(*t,13,27,0);assert(t->controls.splitDuckThresholdDb==-60);
    midiCc(*t,13,27,127);assert(t->controls.splitDuckThresholdDb==0);
    midiCc(*t,13,28,0);assert(t->controls.splitDuckAttackMs==1);
    midiCc(*t,13,29,127);assert(t->controls.splitDuckHoldMs==1000);
    midiCc(*t,13,30,127);assert(t->controls.splitDuckReleaseMs==3000);
    midiCc(*t,13,31,0);assert(t->controls.splitDuckHpfHz==20);
    midiCc(*t,13,32,127);assert(t->controls.splitDuckKneeDb==24);
    midiCc(*t,13,33,0);assert(t->controls.splitDuckRoute==0);
    midiCc(*t,13,33,127);assert(t->controls.splitDuckRoute==1);
    nrpn(*t,16,"split_priority",0);assert(t->controls.splitPriority==-24);
    nrpn(*t,16,"split_priority",16383);assert(t->controls.splitPriority==24);
    setParam(t.get(),"midi_control_mode","MNLT");
    assert(t->splitFocus==focus&&t->selectedLane==lane);
    midiCc(*t,13,76,127);assert(t->tempoBpm==999.0f);
    // Pickup state for the same CC is independent between part channels.
    setParam(t.get(),"midi_takeover","Pickup");
    t->controls.locus=.5f;t->splitParts[1].controls.locus=.5f;
    resetMidiControlPickups(*t);
    midiCc(*t,9,17,0);midiCc(*t,10,17,0);
    assert(t->liveControlsForPart(0).locus==.5f&&t->liveControlsForPart(1).locus==.5f);
    midiCc(*t,9,17,64);midiCc(*t,10,17,64);
    assert(std::abs(t->liveControlsForPart(0).locus-64.f*129.f/16383.f)<.00001f
        && std::abs(t->liveControlsForPart(1).locus-64.f*129.f/16383.f)<.00001f);
    // Bounded enum Pickup engages anywhere within the current choice and
    // remains engaged while the fader crosses later choices. Texture MODE is
    // ENG 1 knob 1, hence CC 32 on the addressed part channel.
    t->controls.algorithm=Engine::PlaybackAlgorithm::texture;
    t->controls.textureMode=0;resetMidiControlPickups(*t);
    midiCc(*t,9,32,10);assert(t->controls.textureMode==0);
    midiCc(*t,9,32,50);assert(t->controls.textureMode==1);
    midiCc(*t,9,32,100);assert(t->controls.textureMode==2);
    midiCc(*t,9,32,127);assert(t->controls.textureMode==3);
    // A value changed elsewhere disengages until the fader returns to the
    // externally selected choice's region.
    t->controls.textureMode=0;
    midiCc(*t,9,32,100);assert(t->controls.textureMode==0);
    midiCc(*t,9,32,10);assert(t->controls.textureMode==0);
    midiCc(*t,9,32,50);assert(t->controls.textureMode==1);
    assert(t->persistentStateDirty);
    char value[32]{};
    assert(getParam(t.get(),"midi_control_mode",value,sizeof(value))>0&&std::strcmp(value,"3")==0);
    std::array<char,kStateTextSize> state{};assert(t->serializeState(state.data(),state.size())>0);
    auto restored=splitFixture();assert(restored->restoreState(state.data()));
    assert(restored->midiControlMode==3&&restored->midiCcChannel==9
        && restored->midiCcBase==20&&restored->midiTakeover==0);
    std::puts("s3g Thru selectable MIDI modes, Generic CC pickup and channel-addressed MNLT banks: PASS");
}

void testNativeE16AndMonolitMode() {
    auto t=splitFixture();t->setPerformanceMode(2);t->selectSplitPart(0,false);
    setParam(t.get(),"midi_control_mode","E16 + MNLT");
    assert(t->midiControlMode==5 && !midiModeAllowsE16(*t) && midiModeAllowsMonolit(*t));
    setParam(t.get(),"e16_remote","1,1");assert(!t->e16RemoteRequested && !t->e16RemoteActive);
    const int focus=t->splitFocus;const auto lane=t->selectedLane;
    t->controls.locus=.5f;t->splitParts[1].controls.locus=.25f;
    midiCc(*t,14,1,127);const float first=t->controls.locus;
    midiCc(*t,14,1,127);assert(t->controls.locus>first);
    const float second=t->controls.locus;
    // Larger hardware turns keep their magnitude, while 0/64 stay neutral.
    midiCc(*t,14,1,120);assert(t->controls.locus>second);
    midiCc(*t,14,1,8);assert(std::abs(t->controls.locus-second)<.000001f);
    midiCc(*t,14,1,0);midiCc(*t,14,1,64);assert(std::abs(t->controls.locus-second)<.000001f);
    // Delayed retired NRPN feedback, old scene recall and pressure cannot
    // reinterpret the direction of the next physical turn.
    nrpnId(*t,5,3457,8192);assert(t->controls.locus==second);
    midiCc(*t,14,1,127);assert(t->controls.locus>second);
    midiCc(*t,14,1,1);assert(std::abs(t->controls.locus-second)<.000001f);
    midiCc(*t,15,1,127);assert(t->liveControlsForPart(1).locus>.25f);
    assert(t->splitFocus==focus && t->selectedLane==lane);
    setParam(t.get(),"algorithm","8");
    for(std::size_t choice=1;choice<10;++choice) {
        midiCc(*t,14,21,127);assert(Engine::wavesetProcessIndex(t->controls.wavesetProcess)==choice);
    }
    midiCc(*t,14,21,1);assert(Engine::wavesetProcessIndex(t->controls.wavesetProcess)==8);
    midiCc(*t,14,127,22);assert(Engine::wavesetProcessIndex(t->controls.wavesetProcess)==0);
    midiCc(*t,14,21,127);assert(Engine::wavesetProcessIndex(t->controls.wavesetProcess)==1);
    // Release packets do not repeat the reset and no direction is latched.
    midiCc(*t,14,127,0);midiCc(*t,14,21,127);
    assert(Engine::wavesetProcessIndex(t->controls.wavesetProcess)==2);
    setParam(t.get(),"midi_takeover","Jump");midiCc(*t,10,17,127);
    assert(t->liveControlsForPart(1).locus==1 && t->splitFocus==focus);
    char feedback[1024]{};formatMidiFeedback(*t,feedback,sizeof(feedback));
    assert(std::strcmp(feedback,"[]")==0);
    const auto old=t->controls.locus;setParam(t.get(),"midi_control_mode","Off");
    midiCc(*t,14,1,127);assert(t->controls.locus==old);
    std::puts("s3g Thru E16 clockwise/counterclockwise turns, magnitude, default pushes, A/B isolation and Monolit: PASS");
}
void testNativeE16HeldNoteResponse() {
    for(int mode:{0,1,2}) {
        auto t=splitFixture();t->setPerformanceMode(mode);
        setParam(t.get(),"midi_control_mode","E16 + MNLT");
        setParam(t.get(),"algorithm","8");
        if(mode){t->selectSplitPart(1,false);setParam(t.get(),"algorithm","8");t->selectSplitPart(0,false);}
        for(int pad:{0,1,2,3,4,5,6,7})t->padNoteOn(68+pad,100);
        for(int step=0;step<100;++step) {
            const auto choice=Engine::wavesetProcessIndex(t->controls.wavesetProcess);
            midiCc(*t,14,21,choice==9?1:127);
            assert(Engine::wavesetProcessIndex(t->controls.wavesetProcess)==(choice==9?8:choice+1));
            const float before=t->controls.locus;
            midiCc(*t,14,1,before>.9f?1:127);
            assert(before>.9f?t->controls.locus<before:t->controls.locus>before);
            t->aftertouch(t->padMappedNotes[0],step%127,0);renderSplit(*t,1);
        }
    }
    std::puts("s3g Thru relative encoder edits with held polyphonic notes: PASS");
}

void testNativeE16RingSnapshots() {
    auto t=splitFixture();t->setPerformanceMode(2);
    setParam(t.get(),"midi_control_mode","E16 + MNLT");
    char state[2048]{};assert(formatE16NativeState(*t,state,sizeof(state))==2);
    const int focus=t->splitFocus;const auto revision=t->persistentRevision;
    midiCc(*t,14,125,37);midiCc(*t,14,126,1);
    t->controls.locus=.5f;t->splitParts[1].controls.locus=1;
    formatE16NativeState(*t,state,sizeof(state));
    assert(std::strstr(state,"\"page\":0,\"token\":37"));
    assert(std::strstr(state,"[1,8192,30]") && std::strstr(state,"[9,16383,100]"));
    assert(t->splitFocus==focus && t->persistentRevision==revision);
    midiCc(*t,14,1,127);assert(t->controls.locus>.5f);
    formatE16NativeState(*t,state,sizeof(state));
    const auto* position=midiParameterForKey("locus");assert(position);
    const int raw=s3g_thru_midi::encode(*position,t->controls.locus,1);
    assert(raw>8192);char expected[64]{};std::snprintf(expected,sizeof(expected),"[1,%d,30]",raw);
    assert(std::strstr(state,expected));
    midiCc(*t,14,1,1);assert(std::abs(t->controls.locus-.5f)<.000001f);
    setParam(t.get(),"midi_takeover","Jump");midiCc(*t,10,17,0);
    formatE16NativeState(*t,state,sizeof(state));assert(std::strstr(state,"[9,0,100]"));
    t->setPerformanceMode(4);midiCc(*t,14,126,4);
    setParam(t.get(),"bank_pan","100");
    formatE16NativeState(*t,state,sizeof(state));assert(std::strstr(state,"[10,16383,100]"));
    midiCc(*t,15,26,1);assert(t->bankSlots[t->bankSelected].pan<1);
    midiCc(*t,15,29,127);assert(t->settingsForPart(1).holdMs==50);
    midiCc(*t,15,29,127);assert(t->settingsForPart(1).holdMs==100);
    midiCc(*t,15,29,1);assert(t->settingsForPart(1).holdMs==50);
    midiCc(*t,15,127,30);assert(t->settingsForPart(1).holdMs==0);
    for(int index=0;index<=67;++index)assert(e16HoldIndex(e16HoldTime(index))==index);
    const auto* source=midiParameterForKey("bank_record_source");assert(source && source->maximum==3 && source->initial==2);
    midiCc(*t,15,30,127);assert(t->bankRecordSource==3);
    midiCc(*t,15,30,1);assert(t->bankRecordSource==2);
    midiCc(*t,15,30,1);assert(t->bankRecordSource==1);
    midiCc(*t,15,30,1);assert(t->bankRecordSource==0);
    midiCc(*t,15,127,31);assert(t->bankRecordSource==2);
    setParam(t.get(),"midi_control_mode","Off");formatE16NativeState(*t,state,sizeof(state));
    assert(std::strcmp(state,"{}")==0);
    std::puts("s3g Thru E16 native page snapshots, safe startup, external edits and Bank bindings: PASS");
}
