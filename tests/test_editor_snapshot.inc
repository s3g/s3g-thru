void testEditorSnapshot()
{
    auto t=splitFixture();
    t->controls.locus=.17f;t->controls.effectDrive=.12f;
    t->controls.ampReleaseMs=250;t->padNoteOn(68,100);t->padNoteOn(72,100);
    renderSplit(*t,30);t->selectSplitPart(1);
    t->controls.algorithm=Engine::PlaybackAlgorithm::grains;
    t->controls.locus=.83f;t->controls.effectDrive=.42f;
    t->controls.ampReleaseMs=1234;t->padOctave=1;t->rootNote=67;
    t->setStackScanPaused(true);setParam(t.get(),"part_mute","1");
    const auto aAge=t->voices[0].age,bAge=t->voices[4].age;
    const auto aPosition=t->voices[0].engine.normalizedPosition();
    std::array<char,16384> text {};
    auto verify=[&]() {
        const int length=getParam(t.get(),"ui_editor_state",text.data(),text.size());
        assert(length>0&&length<8192&&std::strlen(text.data())==static_cast<std::size_t>(length));
        for(const auto* key:kLockParameterKeys) {
            char scalar[64] {};assert(getParam(t.get(),key,scalar,sizeof(scalar))>0);
            float value=0;assert(jsonNumber(text.data(),key,value));
            assert(value==std::strtof(scalar,nullptr)); // identical units, enums, and rounding
        }
        for(const char* key:{"part_mute","pad_mode","root_note","pad_octave","scale","record_lane",
                "preview_level","pressure_mode","hold_release_ms","stack_scan_paused","glitch_buffer_lock",
                "split_mode","split_focus","polyphony_comp","grain_quality","lane_variation",
                "record_gain","record_source","monitor_level","clip_rate","clip_record_mode",
                "clip_timing","launch_quantize","tempo_bpm","tempo_swing","chain_enabled",
                "chain_slot","chain_clip","chain_repeats","sample_trim_start","sample_trim_end"}) {
            char scalar[64] {};assert(getParam(t.get(),key,scalar,sizeof(scalar))>0);
            float value=0;assert(jsonNumber(text.data(),key,value));
            assert(value==std::strtof(scalar,nullptr));
        }
    };
    verify();float value=0;
    assert(jsonNumber(text.data(),"effect_drive",value)&&value==.42f);
    assert(jsonNumber(text.data(),"amp_release",value)&&value==1234);
    assert(jsonNumber(text.data(),"part_mute",value)&&value==1);
    assert(t->voices[0].held&&t->voices[4].held);
    assert(t->voices[0].age==aAge&&t->voices[4].age==bAge);
    assert(t->voices[0].engine.normalizedPosition()==aPosition);
    assert(t->engine.loadedLaneMask()==15u);
    // A short host buffer must fail cleanly without partial JSON or overflow.
    std::array<char,16> tiny;tiny.fill('X');
    assert(getParam(t.get(),"ui_editor_state",tiny.data(),8)==-1&&tiny[0]=='\0');
    for(std::size_t i=8;i<tiny.size();++i)assert(tiny[i]=='X');
    t->selectSplitPart(0);verify();
    assert(jsonNumber(text.data(),"effect_drive",value)&&value==.12f);
    assert(jsonNumber(text.data(),"amp_release",value)&&value==250);
    assert(jsonNumber(text.data(),"part_mute",value)&&value==0);
    t->setSplitMode(false);verify();
}

void testSplitDisplayWindows()
{
    auto t=splitFixture();t->controls.locus=.25f;t->controls.field=.2f;
    t->selectSplitPart(1);t->controls.locus=.75f;t->controls.field=.4f;
    const auto close=[](float a,float b){return std::abs(a-b)<.00001f;};
    auto a=t->displayWindow(0),b=t->displayWindow(1);
    assert(close(a[0],.15f)&&close(a[1],.35f));
    assert(close(b[0],.55f)&&close(b[1],.95f));
    t->selectSplitPart(0);assert(t->displayWindow(0)==a&&t->displayWindow(1)==b);
    // Both simultaneous instruments keep their own actual rendered fields.
    t->padNoteOn(68,100);t->padNoteOn(72,100);renderSplit(*t,100);
    a=t->displayWindow(0);b=t->displayWindow(1);
    assert(close(a[0],.15f)&&close(a[1],.35f));
    assert(close(b[0],.55f)&&close(b[1],.95f));
    t->selectSplitPart(1);assert(t->displayWindow(0)==a&&t->displayWindow(1)==b);
    char text[2048] {};assert(t->formatSplitState(text,sizeof(text))>0);
    const char* aw=std::strstr(text,"\"window\":[");assert(aw);
    const char* bw=std::strstr(aw+1,"\"window\":[");assert(bw);
    float low=0,high=0;assert(std::sscanf(aw,"\"window\":[%f,%f",&low,&high)==2);
    assert(close(low,a[0])&&close(high,a[1]));
    assert(std::sscanf(bw,"\"window\":[%f,%f",&low,&high)==2);
    assert(close(low,b[0])&&close(high,b[1]));
    // A live voice and a recorded voice may have different fields. Show their
    // union instead of cropping one sounding reader out of the display.
    t->selectSplitPart(0);t->padNoteOn(69,100);
    t->voices[1].clipOwned=true;t->splitClipControls[0]=t->controls;
    t->splitClipControls[0].locus=.8f;t->splitClipControls[0].field=.1f;
    renderSplit(*t,100);a=t->displayWindow(0);
    assert(a[0]<=.15001f&&a[1]>=.84999f);
    assert(t->displayWindow(1)==b);
    t->allNotesOff();renderSplit(*t,100);
    t->controls.locus=0;t->controls.field=.2f;a=t->displayWindow(0);
    assert(close(a[0],0)&&close(a[1],.2f));
    t->controls.locus=1;a=t->displayWindow(0);
    assert(close(a[0],.8f)&&close(a[1],1));
    // Idle clip parts follow their recorded field, not the other editor.
    t->clipPlaying=true;t->splitClipControls[1].locus=.12f;t->splitClipControls[1].field=.08f;
    b=t->displayWindow(1);assert(close(b[0],.08f)&&close(b[1],.16f));
}

void testSingleDisplayWindow()
{
    auto t=splitFixture();t->setSplitMode(false);
    const auto close=[](float a,float b){return std::abs(a-b)<.00001f;};
    t->controls.locus=.25f;t->controls.field=.2f;
    auto window=t->displayWindow();
    assert(close(window[0],.15f)&&close(window[1],.35f));
    for(int note=60;note<68;++note)t->noteOn(note,100);
    renderSplit(*t,100);window=t->displayWindow();
    for(const auto& voice:t->voices)assert(voice.active&&voice.held);
    assert(close(window[0],.15f)&&close(window[1],.35f));
    // Slot 7 must participate in Single's view, even though it is reserved
    // for B when Split is active. Its recorded field differs from live notes.
    t->voices[7].clipOwned=true;t->clipControls=t->controls;
    t->clipControls.locus=.8f;t->clipControls.field=.1f;
    renderSplit(*t,100);window=t->displayWindow();
    assert(window[0]<=.15001f&&window[1]>.8f);
    assert(close(window[1],t->voices[7].engine.normalizedFieldEnd()));
    t->allNotesOff();renderSplit(*t,100);
    t->controls.locus=.32715f;t->controls.field=.02337f;window=t->displayWindow();
    std::array<char,1024> text{};assert(getParam(t.get(),"ui_state",text.data(),text.size())>0);
    std::vector<float> values;const char* from=text.data();
    while(*from) {
        char* end=nullptr;const float value=std::strtof(from,&end);assert(end!=from);
        values.push_back(value);if(*end!=',')break;from=end+1;
    }
    assert(values.size()==19u);
    assert(close(values[17],window[0])&&close(values[18],window[1]));
    assert(std::abs(values[17]-values[5]/1000.0f)>.0001f);
    assert(std::abs(values[18]-values[6]/1000.0f)>.0001f);
    t->controls.locus=0;t->controls.field=.2f;window=t->displayWindow();
    assert(close(window[0],0)&&close(window[1],.2f));
    t->controls.locus=1;window=t->displayWindow();
    assert(close(window[0],.8f)&&close(window[1],1));
    t->clipPlaying=true;t->clipControls.locus=.12f;t->clipControls.field=.08f;
    window=t->displayWindow();assert(close(window[0],.08f)&&close(window[1],.16f));
}
