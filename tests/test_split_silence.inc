// Reproduce silence/recovery through the same controls used on the surface.
void testSplitMuteRecovery()
{
    auto t=splitFixture();
    t->padNoteOn(68,100);t->padNoteOn(72,100);
    t->selectSplitPart(1);setParam(t.get(),"part_mute","1");
    renderSplit(*t,100);
    assert(t->splitFrameGain[1].back()<1e-6f&&t->splitFrameGain[0].back()>.6f);
    t->setSplitMode(false);t->setSplitMode(true);
    assert(t->settingsForPart(1).muted==1); // intentional saved mute, not missing samples
    t->selectSplitPart(1);setParam(t.get(),"part_mute","0");
    // Copying a muted source's sound must not silently mute the destination.
    t->selectSplitPart(0);setParam(t.get(),"part_mute","1");t->copySplitPart(0);
    assert(t->settingsForPart(0).muted==1&&t->settingsForPart(1).muted==0);
    t->selectSplitPart(1);setParam(t.get(),"part_mute","1");
    t->selectSplitPart(0);setParam(t.get(),"part_mute","0");t->copySplitPart(0);
    assert(t->settingsForPart(1).muted==1); // copy also preserves an intentional destination mute
    t->selectSplitPart(1);setParam(t.get(),"part_mute","0");
    t->padNoteOn(68,100);t->padNoteOn(72,100);renderSplit(*t,100);
    for(std::size_t part=0;part<2;++part) {
        assert(t->splitFrameGain[part].back()>.6f);
        double energy=0;for(float v:t->splitBus[part][0])energy+=v*v;
        assert(energy>1e-5);
    }
}

void testSplitMixerLiveRecovery()
{
    for(int owner:{0,1}) {
        auto t=splitFixture();
        t->beginClipRecording();
        setParam(t.get(),"split_blend",owner?"0":"1");
        const char* level=owner?"split_level_b":"split_level_a";
        setParam(t.get(),level,"0");
        t->captureClipParameter(static_cast<std::size_t>(LockParameter::splitBlend));
        t->captureClipParameter(static_cast<std::size_t>(owner?LockParameter::splitLevelB:LockParameter::splitLevelA));
        renderSplit(*t,120);t->finishClipRecording();renderSplit(*t,250);
        assert(t->clipPlaying&&t->splitFrameGain[owner].back()<1e-6f);
        // Recover while that recording is still looping. A live knob must not
        // appear to work for half a second and then silently lose to the clip.
        setParam(t.get(),"split_blend","0.5");setParam(t.get(),level,"1");
        renderSplit(*t,300);
        assert(t->splitFrameGain[owner].back()>.6f);
        // Browser preview must recover with the same instrument's live gain.
        setParam(t.get(),"preview_level","1");
        t->sampleRequestLane=owner*2;t->sampleRequestPreview=true;t->samplePreviewCancelled=false;
        t->sampleStageFrames=4096;t->sampleStageRate=t->sampleRate;
        std::fill_n(t->sampleStage.get(),4096,.2f);
        std::fill_n(t->sampleStage.get()+t->laneCapacityFrames,4096,-.1f);
        t->sampleLoadState.store(static_cast<int>(SampleLoadState::ready));
        renderSplit(*t,4);
        assert(t->lastPreviewSample[0]>.12f&&t->lastPreviewSample[1]<-.06f);
        t->stopPreviewWithFade();renderSplit(*t,4);
        // Overrides are performance gestures: restart restores the recording.
        t->setClipPlaying(false);t->setClipPlaying(true);renderSplit(*t,300);
        assert(t->splitFrameGain[owner].back()<1e-6f);
        // The same recovery must work from the documented external CCs.
        t->applyMidiCc(12,64);t->applyMidiCc(owner?14:13,64);
        renderSplit(*t,300);assert(t->splitFrameGain[owner].back()>.6f);
        // Launching a stored clip also returns ownership to recorded controls.
        t->captureActiveClip(true);t->activateStoredClip(t->activeClip,false);
        renderSplit(*t,300);assert(t->splitFrameGain[owner].back()<1e-6f);
    }
}

void testSplitPreviewDestination()
{
    auto t=splitFixture();
    // Reuse decoded audio while changing the destination from A to B.
    const char* path="/tmp/thru-existing-preview.wav";
    std::snprintf(t->sampleRequestPath.data(),t->sampleRequestPath.size(),"%s",path);
    t->sampleRequestLane=0;t->sampleRequestClip=t->activeClip;t->sampleRequestPreview=true;
    t->sampleStageFrames=4096;t->sampleStageRate=t->sampleRate;
    std::fill_n(t->sampleStage.get(),4096,.2f);
    std::fill_n(t->sampleStage.get()+t->laneCapacityFrames,4096,-.1f);
    t->previewActive=true;t->previewFrame=0;
    t->sampleLoadState.store(static_cast<int>(SampleLoadState::previewing));
    t->requestLibrarySample(2,path);
    assert(t->sampleRequestLane==2);
    for(int block=0;block<300&&t->sampleLoadState.load()!=static_cast<int>(SampleLoadState::complete);++block)
        renderSplit(*t,1);
    assert(t->sampleLoadState.load()==static_cast<int>(SampleLoadState::complete));
    assert(t->engine.laneFrames(2)==4096&&t->engine.laneFrames(0)==8192);
}
