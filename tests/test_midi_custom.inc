void customBinding(ThruInstance& t,int channel,int cc,int part,const char* key,
                   int bank=0,int behavior=0,bool pickup=false,float low=0,float high=1,bool invert=false) {
    const auto* p=midiParameterForKey(key);assert(p);
    t.setCustomMidiParameter("midi_custom_begin","-1");
    char text[256]{};std::snprintf(text,sizeof(text),"%d,%d,%d,0,%d,0,0,%d,%d,%d,%.8g,%.8g,%d",
        channel,cc,part,p->id,behavior,pickup?1:0,invert?1:0,static_cast<double>(low),static_cast<double>(high),bank);
    t.setCustomMidiParameter("midi_custom_draft",text);assert(t.customResult!=4);
    t.setCustomMidiParameter("midi_custom_apply","1");assert(t.customResult==5);
}
void testCustomMidiRoutingAndPickup() {
    auto t=splitFixture();t->setPerformanceMode(2);t->selectCustomProfile(1);
    const int focus=t->splitFocus;const float field=t->controls.field;
    customBinding(*t,1,1,1,"locus");
    midiCc(*t,1,1,127);assert(t->splitParts[1].controls.locus==1&&t->controls.field==field&&t->splitFocus==focus);
    assert(t->midiChannels[0][0].wheel==0); // claimed channel-1 CC1 never becomes note expression
    customBinding(*t,1,2,0,"field");midiCc(*t,1,2,0);assert(t->controls.field==.02f&&t->splitFocus==focus);
    t->controls.locus=.75f;customBinding(*t,1,3,0,"locus",0,0,true);
    midiCc(*t,1,3,0);assert(t->controls.locus==.75f);
    midiCc(*t,1,3,90);assert(t->controls.locus==.75f);
    midiCc(*t,1,3,100);assert(t->controls.locus>.78f);
    t->controls.locus=.2f;midiCc(*t,1,3,101);assert(t->controls.locus==.2f);
    midiCc(*t,1,3,0);assert(t->controls.locus==0); // pickup re-arms after Move edit
    customBinding(*t,1,4,0,"speed",0,0,false,.25f,.75f,true);
    const auto& speed=*midiParameterForKey("speed");midiCc(*t,1,4,0);
    assert(std::abs(t->controls.speed-s3g_thru_midi::decode(speed,12287,4))<.001f);
    midiCc(*t,1,4,127);assert(std::abs(t->controls.speed-s3g_thru_midi::decode(speed,4096,4))<.001f);
    customBinding(*t,1,5,0,"algorithm",0,0,true);t->controls.algorithm=Engine::PlaybackAlgorithm::sample;
    for(int value:{33,34,35,36})midiCc(*t,1,5,value);
    assert(static_cast<int>(t->controls.algorithm)==4);
    midiCc(*t,1,5,50);assert(static_cast<int>(t->controls.algorithm)>=5);
    // Explicit parts ignore missing instruments; Focus follows editing focus
    // without stealing it. Runtime pickup state resolves again at each target.
    customBinding(*t,1,6,4,"join");t->selectSplitPart(1);midiCc(*t,1,6,0);assert(t->controls.join==0&&t->splitFocus==1);
    t->selectSplitPart(0);midiCc(*t,1,6,127);assert(t->controls.join==1&&t->splitFocus==0);
    t->setPerformanceMode(0);t->controls.locus=.4f;midiCc(*t,1,1,127);assert(t->controls.locus==.4f);
    // Off disables parameter mappings, and returning to Auto clears button /
    // pickup runtime. Notes and ordinary expression still keep their routes.
    setParam(t.get(),"midi_control_mode","6");const auto join=t->controls.join;midiCc(*t,1,6,0);assert(t->controls.join==join);
    setParam(t.get(),"midi_control_mode","0");midiCc(*t,1,6,0);assert(t->controls.join==0);
    setParam(t.get(),"midi_cc_channel","9");setParam(t.get(),"midi_view","0:0");
    t->controls.locus=.6f;midiCc(*t,9,20,0);assert(t->controls.locus==.6f); // no hidden current-page mapping
    t->setPerformanceMode(3);t->selectSplitPart(1);customBinding(*t,9,17,3,"locus");
    setParam(t.get(),"midi_control_mode","3");const auto a=t->splitParts[0].controls.locus;
    midiCc(*t,9,17,127);assert(t->splitParts[3].controls.locus==1&&t->splitParts[0].controls.locus==a&&t->splitFocus==1);
    t->selectCustomProfile(0);midiCc(*t,9,17,0);assert(t->splitParts[0].controls.locus==0); // built-in MNLT resumes
    std::puts("s3g Thru Custom CC: explicit A/B/Focus routing, channel-1 expression isolation, range/invert/curves, pickup/re-arm/enums, unavailable parts and Off: PASS");
}
void testCustomMidiLearnAndConflicts() {
    auto t=splitFixture();t->setPerformanceMode(2);t->selectCustomProfile(1);
    customBinding(*t,1,9,0,"locus");t->controls.locus=.6f;
    t->setCustomMidiParameter("midi_custom_begin","-1");
    t->setCustomMidiParameter("midi_custom_learn_filter","2");t->setCustomMidiParameter("midi_custom_learn","1");
    midiCc(*t,1,9,0);assert(t->customLearn&&t->controls.locus==0); // unmatched filter retains existing route
    t->setCustomMidiParameter("midi_custom_learn_filter","0");
    midiCc(*t,1,9,127);assert(!t->customLearn&&t->customDraft.cc==9&&t->customResult==1&&t->controls.locus==0);
    midiCc(*t,1,9,64);assert(t->controls.locus==0); // continued fader motion is suppressed during draft
    t->setCustomMidiParameter("midi_custom_apply","1");assert(t->customResult==2&&t->customConflict==0);
    assert(t->currentCustomProfile()->bindings[0].enabled);
    t->setCustomMidiParameter("midi_custom_apply","2");assert(t->customResult==5);
    assert(std::count_if(t->currentCustomProfile()->bindings.begin(),t->currentCustomProfile()->bindings.end(),[](const auto& b){return b.enabled;})==1);
    t->setCustomMidiParameter("midi_custom_begin","0");t->setCustomMidiParameter("midi_custom_learn","1");
    const std::uint8_t realtime[]{0xf8};dispatchMidi(t.get(),realtime,1,MOVE_MIDI_SOURCE_EXTERNAL,false);assert(t->customLearn);
    const std::uint8_t malformed[]{0xb0,128,64};dispatchMidi(t.get(),malformed,3,MOVE_MIDI_SOURCE_EXTERNAL,false);assert(t->customLearn);
    t->customLearnUntil=0;char state[2048]{};assert(t->formatCustomMidiState(state,sizeof(state))>0);assert(!t->customLearn&&t->customResult==7);
    const auto original=t->currentCustomProfile()->bindings[0];
    t->setCustomMidiParameter("midi_custom_draft","1,2,0,0,9999,0,0,0,1,0,0,1,0");assert(t->customResult==4);
    t->setCustomMidiParameter("midi_custom_cancel","1");assert(t->currentCustomProfile()->bindings[0].cc==original.cc);
    t->setCustomMidiParameter("midi_custom_delete","0");assert(!t->currentCustomProfile()->bindings[0].enabled);
    t->setCustomMidiParameter("midi_custom_undo","1");assert(t->currentCustomProfile()->bindings[0].enabled);
    const auto locus=t->controls.locus;t->selectCustomProfile(5);assert(t->controls.locus==locus);
    t->setCustomMidiParameter("midi_custom_copy","2");assert(t->customMaps.active==2&&t->controls.locus==locus);
    assert(t->currentCustomProfile()->bindings[0].enabled&&t->customFactory.bindings[0].enabled);
    t->setCustomMidiParameter("midi_custom_clear","1");assert(!t->currentCustomProfile()->bindings[0].enabled&&t->customFactory.bindings[0].enabled);
    t->setCustomMidiParameter("midi_custom_undo","1");assert(t->currentCustomProfile()->bindings[0].enabled);
    std::puts("s3g Thru Custom Learn: no value writes, filter/timeout/malformed input, duplicate confirmation, cancel, delete/Undo and non-destructive factory copy: PASS");
}
void testCustomMidiBanksAndButtons() {
    auto t=splitFixture();t->setPerformanceMode(2);t->selectCustomProfile(1);
    customBinding(*t,1,20,0,"locus");customBinding(*t,1,20,0,"field",1);
    midiCc(*t,1,20,0);assert(t->controls.field==.02f&&t->controls.locus==.5f);
    t->selectCustomBank(1);midiCc(*t,1,20,0);assert(t->controls.locus==0);
    customBinding(*t,1,21,0,"stack_scan_paused",0,2);
    midiCc(*t,1,21,127);assert(t->stackScanPaused);midiCc(*t,1,21,127);assert(t->stackScanPaused);
    midiCc(*t,1,21,0);midiCc(*t,1,21,127);assert(!t->stackScanPaused);
    customBinding(*t,1,22,1,"part_mute",0,1);
    midiCc(*t,1,22,127);assert(t->splitParts[1].settings.muted);midiCc(*t,1,22,0);assert(!t->splitParts[1].settings.muted);
    // A binding to a page slot follows the actual engine, including dense
    // Wavesets choices and newly reserved / unavailable slots.
    t->setCustomMidiParameter("midi_custom_begin","-1");
    t->setCustomMidiParameter("midi_custom_draft","1,23,0,1,-1,0,0,0,0,0,0,1,0");t->setCustomMidiParameter("midi_custom_apply","1");assert(t->customResult==5);
    t->controls.algorithm=Engine::PlaybackAlgorithm::grains;midiCc(*t,1,23,127);assert(static_cast<int>(t->controls.grains)==3);
    t->controls.algorithm=Engine::PlaybackAlgorithm::sample;midiCc(*t,1,23,127);assert(static_cast<int>(t->controls.sampleMode)==3);
    t->selectCustomProfile(5);t->selectCustomBank(0);
    midiCc(*t,1,59,127);assert(t->customMaps.factoryBank==1);
    midiCc(*t,1,59,127);assert(t->customMaps.factoryBank==1); // bank reset cannot retrigger the same held button
    midiCc(*t,1,59,0);midiCc(*t,1,59,127);assert(t->customMaps.factoryBank==2);
    midiCc(*t,1,32,127);assert(t->customMaps.factoryBank==0);
    midiCc(*t,1,64,127);assert(!t->midiChannels[0][0].sustain); // unused Nano R is consumed, not a sustain pedal
    t->selectCustomProfile(1);t->beginClipRecording();midiCc(*t,1,20,64);renderSplit(*t,8);t->finishClipRecording();
    assert(t->activeClipSlot()->clipPointCount>0);
    t->setClipPlaying(false);t->setPerformanceMode(0);t->selectCustomProfile(5);
    midiCc(*t,1,3,0);midiCc(*t,1,3,127);assert(t->outputGain==2); // Single uses OUT, not a dormant part level
    std::puts("s3g Thru Custom banks/buttons: per-bank override, fixed faders, edge toggles, momentary values, adaptive engine slots, Nano bank edges and clip automation: PASS");
}
void testCustomMidiStorage() {
    char folder[]="/tmp/thru-custom-midi.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();t->backgroundStarted=true;
    t->selectCustomProfile(1);customBinding(*t,3,70,0,"locus",0,0,true,.1f,.9f,true);
    t->selectCustomBank(4);const auto revision=t->customRevision;
    const std::string path=std::string(folder)+"/thru-midi-maps.bin";
    assert(access(path.c_str(),F_OK)!=0);renderSplit(*t,80);
    assert(access(path.c_str(),F_OK)!=0&&t->customSaveJob.load()==1); // no I/O in callbacks
    t->backgroundStarted=false;resumeTestBackgroundWorker(*t);
    for(int i=0;i<4000&&t->customDirty;++i){renderSplit(*t,1);usleep(200);}
    assert(!t->customDirty&&t->customRevision==revision&&access(path.c_str(),F_OK)==0);
    const auto before=t->controls.locus;ThruInstance::CustomMapState read{};
    assert(t->readCustomMidiFile(path.c_str(),read));assert(read.active==1&&read.profiles[0].bank==4);
    assert(read.profiles[0].bindings[0].channel==3&&read.profiles[0].bindings[0].cc==70&&read.profiles[0].bindings[0].invert);
    assert(t->controls.locus==before);
    t->stopBackgroundWork();t->selectCustomBank(2);assert(t->savePersistentState());t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(folder));assert(t->customMaps.active==1&&t->customMaps.profiles[0].bank==2);
    // A corrupt primary uses the previous profile bank from the atomic backup.
    t->stopBackgroundWork();auto* file=std::fopen(path.c_str(),"r+b");assert(file);assert(std::fputc('X',file)!=EOF);std::fclose(file);
    t->customMaps={};t->loadCustomMidiMaps();assert(t->customRecovered&&t->customMaps.active==1&&t->customMaps.profiles[0].bank==4);
    t->customMaps.profiles[0].bindings[0].low=NAN;
    assert(!t->validCustomBinding(t->customMaps.profiles[0].bindings[0]));
    t->customDirty=false;t.reset();removeBackgroundTestDirectory(folder);
    std::puts("s3g Thru Custom storage: off-callback save, separate checksummed profile file, bank/restart recall, backup recovery and invalid data rejection: PASS");
}
