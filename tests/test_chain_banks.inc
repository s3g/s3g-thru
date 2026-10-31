void testSavedChainBanksAndFadeRecall()
{
    char directory[]="/tmp/thru-chain-banks-XXXXXX";assert(mkdtemp(directory));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(directory));t->stopBackgroundWork();
    setParam(t.get(),"pattern_fade_ms","7000");setParam(t.get(),"pattern_fade_curve","3");
    t->setChainEntry(0,2,4);t->chainEditSlot=0;
    setParam(t.get(),"entry_fade_mode","2");setParam(t.get(),"entry_fade_ms","1800");setParam(t.get(),"entry_fade_curve","4");
    t->setChainEntry(15,22,1);t->selectChainBank(7);setParam(t.get(),"chain_end","2");t->selectChainBank(15);t->setChainEntry(63,63,0);t->chainEditSlot=63;
    setParam(t.get(),"chain_end","1");setParam(t.get(),"chain_fade_mode","2");
    setParam(t.get(),"chain_fade_low_ms","5000");setParam(t.get(),"chain_fade_high_ms","8000");
    std::array<char,32768> editor{};assert(formatEditorState(t.get(),editor.data(),editor.size())>0);
    assert(std::strstr(editor.data(),"\"chain_bank\":16")&&std::strstr(editor.data(),"\"chain_end\":1"));
    assert(t->savePersistentState());t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(directory));t->stopBackgroundWork();
    assert(t->chainBanks[7].stop&&!t->chainBanks[7].loop);
    assert(t->persistenceRestored&&t->chainBankIndex==15&&t->chainEditSlot==63&&!t->currentChain().loop);
    assert(t->clipChain()[63].enabled&&t->clipChain()[63].clip==63&&t->clipChain()[63].repeats==0);
    ChainFadeSettings fade;assert(t->resolveChainFade(63,fade)&&fade.mode==1&&fade.low==5000&&fade.high==8000&&fade.curve==3);
    assert(t->patternFadeMs==7000&&t->patternCurve==3&&t->patternTimingMode==0);
    t->selectChainBank(0);assert(t->clipChain()[0].clip==2&&t->clipChain()[0].repeats==4&&t->clipChain()[15].clip==22);
    assert(t->resolveChainFade(0,fade)&&fade.time==1800&&fade.curve==4);
    assert(!t->resolveChainFade(15,fade)); // Original chain keeps following global settings.
    t->selectChainBank(1);for(const auto& row:t->clipChain())assert(!row.enabled);
    t->setChainEntry(0,9,2);assert(t->chainBanks[0].entries[0].clip==2);
    t->selectChainBank(0);t->moveChainEntry(0,31);assert(t->clipChain()[31].fadeMode==2&&t->clipChain()[31].fade.time==1800);
    t->insertChainEntry(31,11);assert(t->clipChain()[31].fadeMode==0&&t->clipChain()[32].fade.time==1800);
    t->deleteChainEntry(31);assert(t->clipChain()[31].fade.time==1800);
    t.reset();removeBackgroundTestDirectory(directory);
}
void testChainLaunchAndManualHandoff()
{
    auto t=splitFixture();t->setPerformanceMode(0);renderSplit(*t,2);t->stopSampleWorker();
    t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
    t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    t->setChainEntry(0,0,2);t->setChainEntry(1,1,0);t->chainEditSlot=1;
    setParam(t.get(),"chain_fade_mode","1");setParam(t.get(),"chain_fade_ms","8000");
    setParam(t.get(),"entry_fade_mode","3");setParam(t.get(),"entry_fade_low_ms","2200");
    setParam(t.get(),"entry_fade_high_ms","2400");setParam(t.get(),"entry_fade_curve","4");
    t->patternTimingRandom=123;t->setChainEnabled(true);
    t->advanceClipChain();assert(t->chainPosition==0&&t->chainRepeat==1&&t->activeClip==0);
    assert(t->prepareClipBank(1));t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
    // A draining preview must leave the row cursor and random stream intact.
    const auto seed=t->patternTimingRandom;t->previewTransitionRemaining=64;
    t->advanceClipChain();assert(t->chainPosition==0&&t->activeClip==0&&t->patternTimingRandom==seed);
    t->previewTransitionRemaining=0;t->advanceClipChain();
    assert(t->chainPosition==1&&t->activeClip==1&&t->patternActive&&t->patternFadeOverridden);
    assert(t->patternDurationMs>=2200&&t->patternDurationMs<=2400&&t->patternPlaybackCurve==4&&t->patternCurve==0);
    const auto chosen=t->patternDurationMs;const auto draw=t->patternTimingRandom;
    t->setPatternPosition(.5f);setParam(t.get(),"entry_fade_high_ms","15000");
    setParam(t.get(),"chain_advance","1");t->advanceClipChain();
    assert(t->chainPosition==1&&t->patternHeld&&t->patternDurationMs==chosen&&t->patternTimingRandom==draw);
    std::array<float,kBlockFrames*2u> audio{};t->render(nullptr,kBlockFrames,audio.data());t->finishPatternFade();
    for(int i=0;i<150&&t->patternActive;++i){waitForPatternAudio(*t);t->render(nullptr,kBlockFrames,audio.data());}
    waitForPatternIdle(*t);t->chainAdvanceRequested=false;t->advanceClipChain();
    assert(t->chainPosition==1&&t->chainRepeat==0); // WAIT keeps playing until NEXT.
    t->currentChain().loop=false;t->chainNextSlot=-1;setParam(t.get(),"chain_advance","1");t->advanceClipChain();
    assert(!t->chainEnabled&&t->activeClip==1); // STAY keeps the final pattern.
    const auto position=t->clipPositionMs;t->selectChainBank(2);
    assert(!t->chainEnabled&&t->activeClip==1&&t->clipPositionMs==position&&t->queuedClip==-1);
    t->preparedClip.store(0);t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
    t->serviceClipBeforeBlock();assert(!t->clipPreparationInFlight()&&t->activeClip==1);
}
void testLegacyChainMigration()
{
    char directory[]="/tmp/thru-chain-legacy-XXXXXX";assert(mkdtemp(directory));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(directory));t->stopBackgroundWork();
    t->setChainEntry(0,17,3);t->setChainEntry(15,63,16);assert(t->savePersistentState());
    const auto path=std::string(directory)+"/thru-clips.bin";
    auto* file=std::fopen(path.c_str(),"rb");assert(file);assert(std::fseek(file,0,SEEK_END)==0);
    const long length=std::ftell(file);assert(length>0);std::rewind(file);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));assert(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);
    constexpr std::size_t extension=16u+16u*(24u+64u*32u);
    assert(bytes.size()>extension+4);bytes.resize(bytes.size()-extension-4104-37128-4);bytes[8]=17;
    std::uint32_t checksum=2166136261u;for(auto byte:bytes){checksum^=byte;checksum*=16777619u;}
    file=std::fopen(path.c_str(),"wb");assert(file);assert(std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size());assert(writeU32(file,checksum));assert(std::fclose(file)==0);
    t->selectChainBank(15);t->setChainEntry(63,44,0);
    assert(t->readClipFileFromPath(path.c_str()));assert(t->chainBankIndex==0);
    assert(t->clipChain()[0].clip==17&&t->clipChain()[0].repeats==3&&t->clipChain()[15].clip==63&&t->clipChain()[15].repeats==16);
    for(std::size_t i=16;i<64;++i)assert(!t->clipChain()[i].enabled);
    for(std::size_t bank=1;bank<16;++bank)for(const auto& row:t->chainBanks[bank].entries)assert(!row.enabled);
    assert(!t->currentChain().savedFade&&t->currentChain().loop);
    t.reset();removeBackgroundTestDirectory(directory);
}
void testSavedChainPlaybackBoundaries()
{
    auto t=splitFixture();t->setPerformanceMode(0);renderSplit(*t,2);
    t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
    t->beginClipRecording();t->padNoteOn(68,100);renderSplit(*t,8);t->padNoteOff(68);renderSplit(*t,32);t->finishClipRecording();
    t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    t->selectChainBank(3);t->setChainEntry(17,0,2);t->setChainEntry(63,1,0);t->chainEditSlot=63;
    setParam(t.get(),"entry_fade_mode","2");setParam(t.get(),"entry_fade_ms","200");setParam(t.get(),"entry_fade_curve","3");
    setParam(t.get(),"chain_end","1");t->setChainEnabled(true);t->setClipPlaying(false);t->setClipPlaying(true);
    bool sawFade=false;
    for(int block=0;block<1500;++block) {
        if(t->patternActive)waitForPatternAudio(*t);renderSplit(*t,1);usleep(500);
        sawFade|=t->patternActive;
        if(t->activeClip==1&&!t->patternBusy())break;
    }
    assert(sawFade&&t->activeClip==1&&t->chainPosition==63&&t->chainEnabled&&t->clipPlaying);
    assert(t->patternDurationMs==200&&t->patternPlaybackCurve==3&&t->patternFadeMs==0);
    renderSplit(*t,100);assert(t->chainPosition==63&&t->chainRepeat==0&&t->chainEnabled);
    setParam(t.get(),"chain_advance","1");
    for(int block=0;block<100&&t->chainEnabled;++block)renderSplit(*t,1);
    assert(!t->chainEnabled&&t->activeClip==1&&t->clipPlaying);
    // Selecting another chain while its loader is active cannot launch the
    // old target or permanently block subsequent edit/record operations.
    t->currentChain().loop=true;t->setChainEnabled(true);setParam(t.get(),"chain_advance","1");
    t->selectChainBank(4);
    for(int block=0;block<1500&&t->clipPreparationInFlight();++block){renderSplit(*t,1);usleep(500);}
    assert(!t->clipPreparationInFlight()&&t->activeClip==1&&t->clipPlaying&&t->chainEnabled&&t->chainPlayBank==3);
}
void testChainStopTransport()
{
    for(bool wait:{false,true}) {
        auto t=splitFixture();t->setPerformanceMode(0);renderSplit(*t,2);t->stopSampleWorker();
        t->beginClipRecording();t->padNoteOn(68,100);renderSplit(*t,20);t->finishClipRecording();t->padNoteOff(68);
        t->setChainEntry(63,0,wait?0:2);setParam(t.get(),"chain_end","2");
        assert(!t->currentChain().loop&&t->currentChain().stop);
        t->setClipPlaying(false);t->setChainEnabled(true);t->setClipPlaying(true);
        if(wait) {
            renderSplit(*t,100);assert(t->clipPlaying&&t->chainRepeat==0);
            setParam(t.get(),"chain_advance","1");
        }
        for(int block=0;block<100&&t->clipPlaying;++block)renderSplit(*t,1);
        assert(!t->clipPlaying&&!t->clipLaunchPending&&t->chainEnabled);
        assert(t->clipPositionMs==0&&t->clipAbsolutePositionMs==0&&!t->clipNeedsStart);
        assert(t->queuedClip==-1&&t->chainNextSlot==-1&&!t->chainAdvanceRequested);
        for(const auto& voice:t->voices)assert(!voice.clipOwned||!voice.held);
        renderSplit(*t,50);assert(!t->clipPlaying&&t->clipPositionMs==0);
        t->setClipPlaying(true);renderSplit(*t,1);assert(t->clipPlaying&&t->chainPosition==63);
        bool held=false;for(const auto& voice:t->voices)held|=voice.clipOwned&&voice.held;assert(held);
    }
}
