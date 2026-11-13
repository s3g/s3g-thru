void testModeStatePreservation()
{
    auto t=splitFixture();t->setPerformanceMode(2);
    setParam(t.get(),"algorithm","14");
    setParam(t.get(),"pulsar_rate","17.125");setParam(t.get(),"pulsar_size","873.25");
    setParam(t.get(),"manual_lane","2.71321");
    t->selectSplitPart(1);setParam(t.get(),"manual_lane","1.23456");t->selectSplitPart(0);
    const auto pulse=t->controls;
    const auto check=[&]{
        assert(t->performanceMode()==2);
        assert(std::fabs(t->liveControlsForPart(0).manualLane-1.71321f)<1e-5f);
        assert(std::fabs(t->liveControlsForPart(1).manualLane-.23456f)<1e-5f);
        assert(t->controls.pulsarRate==pulse.pulsarRate&&t->controls.pulsarSize==pulse.pulsarSize);
    };
    for(int repeat=0;repeat<3;++repeat)for(int via:{0,1,3,4,5,6}) {
        t->setPerformanceMode(via);assert(t->performanceMode()==via);
        if(via==1)assert(t->controls.manualLane<=1);
        if(via==3||via==6)assert(t->controls.manualLane==0);
        t->setPerformanceMode(2);check();
    }
    // Saving a restricted layout must retain the unclamped preference.
    t->setPerformanceMode(1);
    auto json=std::make_unique<std::array<char,kStateTextSize>>();
    assert(t->serializeState(json->data(),json->size())>0);
    auto restored=splitFixture();assert(restored->restoreState(json->data()));
    restored->setPerformanceMode(2);
    assert(std::fabs(restored->controls.manualLane-1.71321f)<1e-5f);
    t->captureActiveClip(true);const int original=t->activeClip;
    t->activateStoredClip((original+1)%64,false);t->activateStoredClip(original,false);
    t->setPerformanceMode(2);check();
    // A deliberate edit replaces the preference, even at the clamped endpoint.
    t->setPerformanceMode(1);setParam(t.get(),"manual_lane","2");
    t->setPerformanceMode(2);assert(t->controls.manualLane==1);
    setParam(t.get(),"manual_lane","2.71321");t->setPerformanceMode(1);
    t->captureActiveClip(true);
    auto file=std::tmpfile();assert(file);
    assert(ThruInstance::writeSplitLanePreferences(file,t->clipSlots));std::rewind(file);
    auto clips=std::make_unique<std::array<ClipSlot,kClipSlotCount>>();
    assert(ThruInstance::readSplitLanePreferences(file,*clips));std::fclose(file);
    assert((*clips)[original].splitLanePreferences==t->activeClipSlot()->splitLanePreferences);
    // An older JSON state has no preference hint and uses its saved layout.
    std::string legacy=json->data();const auto start=legacy.find(",\"split_lane_preferences\":[");assert(start!=std::string::npos);
    legacy.erase(start,legacy.find(']',start)-start+1);
    assert(restored->restoreState(legacy.c_str()));restored->setPerformanceMode(2);
    assert(restored->controls.manualLane==1);
    // Full bank reopen and an actual preceding-format bank, including checksum.
    char directory[]="/tmp/thru-mode-state.XXXXXX";assert(mkdtemp(directory));
    auto saved=std::make_unique<ThruInstance>();assert(saved->prepare(directory));saved->stopBackgroundWork();
    saved->setPerformanceMode(2);setParam(saved.get(),"manual_lane","2.71321");saved->setPerformanceMode(1);
    assert(saved->savePersistentState());saved.reset();
    saved=std::make_unique<ThruInstance>();assert(saved->prepare(directory));saved->stopBackgroundWork();
    assert(saved->persistenceRestored&&saved->performanceMode()==1);saved->setPerformanceMode(2);
    assert(std::fabs(saved->controls.manualLane-1.71321f)<1e-5f);
    const std::string current=std::string(directory)+"/thru-clips.bin",oldBank=std::string(directory)+"/legacy.bin";
    auto input=std::fopen(current.c_str(),"rb");assert(input);assert(std::fseek(input,0,SEEK_END)==0);
    const auto size=std::ftell(input);std::rewind(input);std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    assert(std::fread(bytes.data(),1,bytes.size(),input)==bytes.size());std::fclose(input);
    bytes.resize(bytes.size()-4);stripSplitLanePreferences(bytes);assert(bytes[8]==40);
    auto output=std::fopen(oldBank.c_str(),"wb");assert(output);
    assert(std::fwrite(bytes.data(),1,bytes.size(),output)==bytes.size());assert(std::fclose(output)==0);
    assert(appendFileChecksum(oldBank.c_str()));assert(saved->readClipFileFromPath(oldBank.c_str()));
    saved->setPerformanceMode(2);assert(saved->controls.manualLane==1);
    saved.reset();removeBackgroundTestDirectory(directory);
    std::array<char,4096> state{};t->setPerformanceMode(6);
    assert(t->formatSplitState(state.data(),state.size())>0);
    assert(std::strstr(state.data(),"\"mode\":6"));
    std::puts("Mode state preservation: Shared/Split/Quad/Bank/Layers/Sequence roundtrips, independent parts, explicit edits, clip recall, frozen JSON, binary preference extension, legacy defaults and coherent mode identity: PASS");
}
