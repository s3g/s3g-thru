void testAuthoredPathMath()
{
    using Path=s3g_daisy::ThruStackPath;
    Path p;assert(p.valid());
    assert(p.evaluate(0)==0&&p.evaluate(.25f)==.5f&&p.evaluate(.5f)==1&&p.evaluate(1)==0);
    p.time[1]=.1f;p.time[2]=.3f;p.time[3]=.9f;
    p.lane={{.2f,.2f,.8f,.2f,0,0,0,0}};p.curve[0]=1;
    for(int i=0;i<100;++i)assert(p.evaluate(i*.001f)==.2f);
    assert(std::abs(p.evaluate(.2f)-.5f)<1e-6f);
    for(float bend:{-1.0f,-.7f,0.0f,.8f,1.0f}) {
        Path original;original.curve={{bend,-bend,bend,-bend,0,0,0,0}};
        Path split=original;int point=0;
        while(split.count<8){point=split.insertAfter(point);assert(point>0&&split.valid());}
        for(int i=0;i<=1024;++i)assert(std::abs(original.evaluate(i/1024.0f)-split.evaluate(i/1024.0f))<2e-6f);
        assert(split.insertAfter(0)<0);
        while(split.count>4){point=split.remove(point);assert(point>=0&&split.valid());}
        assert(split.remove(0)<0);
    }
    Path broken;broken.count=100;broken.time.fill(1);broken.time[0]=NAN;
    broken.lane.fill(INFINITY);broken.curve.fill(NAN);broken.sanitize();assert(broken.valid());
    for(int i=-100;i<200;++i)assert(std::isfinite(broken.evaluate(i*.011f)));
    broken.count=4;broken.time[7]=NAN;broken.sanitize();assert(broken.valid());
    Path closed;closed.lane[0]=.7f;closed.lane[3]=.1f;
    assert(std::abs(closed.evaluate(.999999f)-closed.evaluate(0))<1e-5f);
    assert(std::abs(closed.evaluate(-.01f)-closed.evaluate(.99f))<1e-6f);
}

void testAuthoredPathVoicesAndRecording()
{
    auto t=splitFixture();t->setPerformanceMode(0);
    t->controls.algorithm=Engine::PlaybackAlgorithm::lanes;t->controls.lanePath=Engine::LanePath::custom;
    t->controls.laneCycles=.5f;t->controls.lanePathOffset=0;
    // Authored timings and levels bypass the named-shape warp controls.
    t->controls.lanePathSkew=.9f;t->controls.lanePathCurve=.8f;
    t->controls.lanePathShape=Engine::LanePathShape::plateau;
    t->padNoteOn(68,100);renderSplit(*t,50);t->padNoteOn(76,100);renderSplit(*t,30);
    auto& first=t->voices[0];auto& second=t->voices[1];assert(first.held&&second.held);
    const auto a=first.engine.normalizedLaneScanPhase(),b=second.engine.normalizedLaneScanPhase();assert(a-b>.05f);
    assert(std::abs(first.engine.normalizedLanePosition()*3-t->controls.stackPath.evaluate(a)*3)<.003f);
    t->editStackPath("lane",1,.1f);
    assert(first.engine.normalizedLaneScanPhase()==a&&second.engine.normalizedLaneScanPhase()==b);
    renderSplit(*t,1);
    const auto advance=kBlockFrames*.5f/t->sampleRate;
    assert(std::abs(first.engine.normalizedLaneScanPhase()-a-advance)<.0001f);
    assert(std::abs(second.engine.normalizedLaneScanPhase()-b-advance)<.0001f);
    assert(std::abs((first.engine.normalizedLaneScanPhase()-second.engine.normalizedLaneScanPhase())-(a-b))<.0001f);
    t->setStackScanPaused(true);renderSplit(*t,10);const auto held=first.engine.normalizedLaneScanPhase();
    t->editStackPath("curve",0,.75f);renderSplit(*t,10);assert(first.engine.normalizedLaneScanPhase()==held);
    t->setStackScanPaused(false);
    t->controls.stackPath=s3g_daisy::ThruStackPath{};
    t->beginClipRecording();assert(t->clipRecording);renderSplit(*t,50);
    t->editStackPath("lane",1,.1f);renderSplit(*t,50);t->finishClipRecording();
    auto& clip=t->clipSlots[t->activeClip];assert(clip.clipRecorded);
    t->applyClipCurves(clip,1);assert(t->clipControls.stackPath.lane[1]==.5f);
    t->applyClipCurves(clip,clip.clipDurationMs-1);assert(t->clipControls.stackPath.lane[1]==.1f);
    t->editStackPath("curve",1,.2f); // A live setting made immediately before overdubbing.
    t->clipRecordMode=ClipRecordMode::overdub;t->beginClipRecording();assert(t->clipRecording);
    t->editStackPath("curve",1,.8f);renderSplit(*t,20);t->finishClipRecording();
    assert(clip.takeCount==2);t->undoLastClipTake();assert(clip.takeCount==1);
    t->applyClipCurves(clip,clip.clipDurationMs-1);assert(t->clipControls.stackPath.curve[1]==.2f);
    t->editStackPath("lane",1,.7f);t->captureActiveClip(true);
    assert(t->controls.stackPath.lane[1]==.7f&&clip.stackPaths[0].lane[1]==.7f);
    t->applyClipCurves(clip,1);assert(t->clipControls.stackPath.lane[1]==.5f);
    t->restoreUndo();assert(t->controls.stackPath.lane[1]==.1f);
}

void testAuthoredPathPartsAndPersistence()
{
    char directory[]="/tmp/thru-path-bank-XXXXXX";assert(mkdtemp(directory));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(directory));
    t->controls.lanePath=Engine::LanePath::custom;t->editStackPath("lane",1,.3f);
    const auto single=t->controls.stackPath;t->setPerformanceMode(2);
    t->editStackPath("count",0,6);t->editStackPath("curve",1,.6f);const auto a=t->controls.stackPath;
    t->selectSplitPart(1);t->editStackPath("time",1,.2f);t->editStackPath("lane",2,.4f);const auto b=t->controls.stackPath;
    assert(t->liveControlsForPart(0).stackPath.lane!=b.lane);
    t->captureActiveClip(true);assert(t->clipSlots[0].stackPaths[1].count==6);
    t->loadClip(1);t->selectSplitPart(1);t->editStackPath("lane",2,.9f);t->loadClip(0);
    assert(t->controls.stackPath.lane==b.lane&&t->liveControlsForPart(0).stackPath.curve==a.curve);
    t->editStackPath("curve",1,-.8f);t->restoreUndo();assert(t->controls.stackPath.curve==b.curve);
    std::array<char,kStateTextSize> json{};assert(t->serializeState(json.data(),json.size())>0);
    auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(nullptr));assert(restored->restoreState(json.data()));
    assert(restored->controls.stackPath.time==b.time&&restored->liveControlsForPart(0).stackPath.count==a.count);
    assert(restored->splitSingleControls.stackPath.lane==single.lane);
    assert(t->savePersistentState());auto disk=std::make_unique<ThruInstance>();assert(disk->prepare(directory));
    assert(disk->controls.lanePath==Engine::LanePath::custom);
    assert(disk->controls.stackPath.time==b.time&&disk->controls.stackPath.lane==b.lane);
    assert(disk->liveControlsForPart(0).stackPath.curve==a.curve);
    assert(disk->splitSingleControls.stackPath.lane==single.lane);
    disk.reset();restored.reset();t.reset();std::string cleanup="rm -rf ";cleanup+=directory;assert(std::system(cleanup.c_str())==0);
}

void testAuthoredPathsStayWithTheirPart()
{
    auto t=splitFixture();t->setPerformanceMode(2);t->controls.lanePath=Engine::LanePath::custom;
    t->beginClipRecording();renderSplit(*t,30);t->editStackPath("lane",1,.2f);
    t->selectSplitPart(1);renderSplit(*t,30);t->editStackPath("curve",1,.6f);
    renderSplit(*t,30);t->finishClipRecording();const auto duration=t->activeClipSlot()->clipDurationMs;
    t->applyClipCurves(*t->activeClipSlot(),duration-1);
    assert(t->splitClipControls[0].stackPath.lane[1]==.2f);
    assert(t->splitClipControls[1].stackPath.curve[1]==.6f);
    assert(t->splitClipControls[0].stackPath.curve[1]==0);
    t->setPerformanceMode(0);t->applyClipCurves(*t->activeClipSlot(),duration-1);
    assert(t->clipControls.stackPath.lane[1]==.5f&&t->clipControls.stackPath.curve[1]==0);
    t->setPerformanceMode(2);t->selectSplitPart(0);t->copySplitPart(0);t->selectSplitPart(1);
    assert(t->controls.stackPath.lane==t->liveControlsForPart(0).stackPath.lane);
}

void testAuthoredPathTakeUndoAfterReload()
{
    char directory[]="/tmp/thru-path-takes-XXXXXX";assert(mkdtemp(directory));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(directory));
    t->controls.lanePath=Engine::LanePath::custom;t->editStackPath("curve",1,.25f);
    t->beginClipRecording();renderSplit(*t,20);t->editStackPath("lane",1,.3f);
    renderSplit(*t,80);t->finishClipRecording();
    t->clipRecordMode=ClipRecordMode::overdub;t->beginClipRecording();renderSplit(*t,10);
    t->editStackPath("curve",1,.8f);renderSplit(*t,20);t->finishClipRecording();
    assert(t->activeClipSlot()->takeCount==2);assert(t->savePersistentState());t.reset();
    auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(directory));
    assert(restored->controls.stackPath.curve[1]==.8f);
    restored->undoLastClipTake();assert(restored->activeClipSlot()->takeCount==1);
    assert(restored->controls.stackPath.curve[1]==.25f);
    restored->applyClipCurves(*restored->activeClipSlot(),restored->activeClipSlot()->clipDurationMs-1);
    assert(restored->clipControls.stackPath.lane[1]==.3f&&restored->clipControls.stackPath.curve[1]==.25f);
    restored.reset();std::string cleanup="rm -rf ";cleanup+=directory;assert(std::system(cleanup.c_str())==0);
}
