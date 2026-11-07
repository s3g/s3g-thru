// Slice metadata, actual voice/render paths, background zoom and migration.
using SliceBytes=std::vector<std::uint8_t>;
void sliceAppendU32(SliceBytes& b,std::uint32_t n) {for(int i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
void sliceChunk(SliceBytes& b,const char* tag,const SliceBytes& body) {
    b.insert(b.end(),tag,tag+4);sliceAppendU32(b,static_cast<std::uint32_t>(body.size()));
    b.insert(b.end(),body.begin(),body.end());if(body.size()&1)b.push_back(0);
}
SliceBytes sliceWave(const std::vector<std::uint32_t>& offsets,std::size_t frames=8192) {
    SliceBytes wave{'R','I','F','F',0,0,0,0,'W','A','V','E'};
    SliceBytes fmt{1,0,1,0};sliceAppendU32(fmt,44100);sliceAppendU32(fmt,88200);
    fmt.insert(fmt.end(),{2,0,16,0});sliceChunk(wave,"fmt ",fmt);
    // adtl intentionally precedes cue, with odd padding and printable labels.
    SliceBytes list{'a','d','t','l'},label;sliceAppendU32(label,1);
    label.insert(label.end(),{'S','n','a','r','e',0});sliceChunk(list,"labl",label);sliceChunk(wave,"LIST",list);
    SliceBytes cues;sliceAppendU32(cues,static_cast<std::uint32_t>(offsets.size()));
    for(std::size_t i=0;i<offsets.size();++i) {
        sliceAppendU32(cues,static_cast<std::uint32_t>(i+1));sliceAppendU32(cues,0);
        cues.insert(cues.end(),{'d','a','t','a'});sliceAppendU32(cues,0);sliceAppendU32(cues,0);sliceAppendU32(cues,offsets[i]);
    }
    sliceChunk(wave,"cue ",cues);sliceChunk(wave,"data",SliceBytes(frames*2,0));
    const auto size=static_cast<std::uint32_t>(wave.size()-8);for(int i=0;i<4;++i)wave[4+i]=size>>(i*8);
    return wave;
}
void testSliceMarkersAndMaps() {
    using namespace s3g_thru;
    auto bytes=sliceWave({4096,1024,4096,0,9000});
    auto map=readWaveSliceMarkers(bytes.data(),bytes.size(),1,8192);
    assert(map.count==3&&map.points[0]==0&&map.points[1]==.125f&&map.points[2]==.5f&&map.points[3]==1);
    assert(!std::strcmp(map.labels[2].data(),"Snare"));
    map=readWaveSliceMarkers(bytes.data(),bytes.size(),.5,4096);
    assert(map.count==3&&map.points[1]==.125f&&map.points[2]==.5f);
    map=readWaveSliceMarkers(bytes.data(),bytes.size(),.5,1024);
    assert(map.count==2&&map.points[1]==.5f); // discard markers beyond truncated decoded audio
    assert(!readWaveSliceMarkers(bytes.data(),11,1,8192).count);
    assert(!readWaveSliceMarkers(bytes.data(),bytes.size(),NAN,8192).count);
    for(std::size_t at=0;at<bytes.size();at+=37) {
        const auto parsed=readWaveSliceMarkers(bytes.data(),at,1,8192);assert(validSliceMap(parsed));
    }
    std::vector<std::uint32_t> many;for(int i=1;i<=100;++i)many.push_back(i*64);
    bytes=sliceWave(many);map=readWaveSliceMarkers(bytes.data(),bytes.size(),1,8192);
    assert(map.count==64&&map.overflow&&validSliceMap(map));
    assert(!addSlicePoint(map,.99f,1.f/8192));
    map={};map.count=2;map.points={{0,.5f,1}};
    assert(addSlicePoint(map,.25f,1.f/8192)&&map.count==3&&map.points[1]==.25f);
    assert(!addSlicePoint(map,.25f,1.f/8192));
    assert(removeSlicePoint(map,1)&&map.count==2&&map.points[1]==.5f);
    assert(!removeSlicePoint(map,0)&&!removeSlicePoint(map,map.count));
    map.points={{0,.25f,1}};std::snprintf(map.labels[0].data(),24,"first");
    const auto reversed=reverseSliceMap(map);assert(reversed.points[1]==.75f&&!std::strcmp(reversed.labels[1].data(),"first"));
    assert(reverseSliceMap(reversed).points==map.points);
    const auto trimmed=trimSliceMap(map,.125f,.625f);assert(trimmed.count==2&&trimmed.points[0]==0&&trimmed.points[1]==.25f&&trimmed.points[2]==1);
    char dir[]="/tmp/thru-slice-cues.XXXXXX";assert(mkdtemp(dir));const auto library=std::string(dir)+"/library";
    assert(mkdir(library.c_str(),0700)==0);const auto path=library+"/cues.wav";const int fd=open(path.c_str(),O_CREAT|O_WRONLY,0600);assert(fd>=0);
    bytes=sliceWave({0,1024,4096});assert(write(fd,bytes.data(),bytes.size())==static_cast<ssize_t>(bytes.size()));assert(close(fd)==0);
    auto t=splitFixture();std::snprintf(t->moduleDirectory.data(),t->moduleDirectory.size(),"%s",dir);
    std::vector<float> left(8192),right(8192);std::size_t frames=0;float rate=0;char name[32]{};
    assert(t->decodeLibraryWav(path.c_str(),left.data(),right.data(),frames,rate,name,sizeof(name),&map));
    assert(frames==8192&&map.count==3&&map.points[1]==.125f&&map.points[2]==.5f);
    assert(unlink(path.c_str())==0);assert(rmdir(library.c_str())==0);assert(rmdir(dir)==0);
    std::puts("s3g Thru WAV cues: ordering, labels, padding, duplicate/out-of-range/malformed markers, resampling, truncation, cap and real WAV decoder: PASS");
}
void testSliceEditorVoicesAndUndo() {
    auto t=splitFixture();t->setPerformanceMode(0);t->controls.algorithm=Engine::PlaybackAlgorithm::slices;
    t->controls.sampleRegionMode=Engine::RegionMode::transients;t->controls.sampleMode=Engine::SampleMode::loop;
    t->controls.manualLane=0;renderSplit(*t,12);
    t->controls.sampleRegionMode=Engine::RegionMode::equal;t->controls.sampleRegionCount=4;t->controls.field=.5f;t->controls.locus=.5f;
    char positions[1024]{};assert(t->formatRegionState(positions,sizeof(positions))>0);
    assert(!std::strcmp(positions,"4,250,375,500,625,750"));
    t->controls.field=1;t->controls.sampleRegionMode=Engine::RegionMode::transients;
    auto& regions=t->activeRegions[0];regions.count=3;regions.onsets={{0,.3f,.7f,1}};regions.boundaries={{0,.29f,.69f,1}};
    noteMidi(*t,1,61);renderSplit(*t,16);auto* held=externalVoice(*t,0,61);assert(held&&held->sliceCached);
    assert(held->sliceLow==.3f&&held->sliceHigh==.7f);
    assert(std::abs(held->sliceLead-3.f*.001f*t->sampleRate/8192)<1e-7f);
    t->setSliceEditorParam("slice_editor_open","1");assert(t->sliceEditor.active);
    t->setSliceEditorParam("slice_editor_select","1");
    t->setSliceEditorParam("slice_editor_preroll","8.5");assert(t->sliceEditorMap().points[1]==.3f);
    t->setSliceEditorParam("slice_editor_move","1,0.4");
    auto map=t->sliceEditorMap();assert(map.points[1]==.4f&&t->controls.sampleRegionMode==Engine::RegionMode::edited);
    const auto oldPosition=held->engine.normalizedPosition();renderSplit(*t,3);
    assert(held->sliceLow==.3f&&held->sliceHigh==.7f&&held->engine.normalizedPosition()!=oldPosition);
    ++t->wavesetSourceRevisions[0];renderSplit(*t,3); // actual source replacement invalidates old cached bounds
    assert(held->sliceLow==.4f&&held->sliceHigh==.7f);
    noteMidi(*t,1,62);renderSplit(*t,8);auto* next=externalVoice(*t,0,62);assert(next&&next->sliceLow==.7f&&next->sliceHigh==1);
    // Preview is not a performed note, neither on record start nor while
    // recording external keys at the same Root. It consumes one normal voice.
    t->setSliceEditorParam("slice_editor_loop","1");renderSplit(*t,10);assert(t->sliceAuditionVoice());
    t->beginClipRecording();assert(t->activeClipSlot()->clipNoteCount==2);
    const auto before=t->activeClipSlot()->clipNoteCount;t->shiftHeld=true;t->padNoteOn(68,100);t->padNoteOff(68);t->shiftHeld=false;
    assert(t->sliceAuditionVoice()&&t->sliceAuditionVoice()->held&&t->sliceEditor.latched);
    assert(t->activeClipSlot()->clipNoteCount==before);
    t->padNoteOn(69,100);t->padNoteOff(69);renderSplit(*t,8);assert(!t->sliceAuditionVoice());
    t->finishClipRecording();t->setClipPlaying(false);
    t->setSliceEditorParam("slice_editor_move","1,0.45");
    t->clipPrepareState.store(static_cast<int>(ClipPrepareState::loading));t->restoreUndo();
    assert(t->sliceEditorMap().points[1]==.45f&&t->undoAvailable);
    t->clipPrepareState.store(static_cast<int>(ClipPrepareState::idle));t->restoreUndo();assert(t->sliceEditorMap().points[1]==.4f);
    t->setSliceEditorParam("slice_editor_add","0.55");assert(t->sliceEditorMap().count==4);
    t->setSliceEditorParam("slice_editor_delete","2");assert(t->sliceEditorMap().count==3);
    t->setSliceEditorParam("slice_editor_delete","0");assert(t->sliceEditorMap().count==3);
    t->restoreUndo();assert(t->sliceEditorMap().count==4);
    t->setSliceEditorParam("slice_editor_lane","4");assert(t->selectedLane==3&&!t->sliceAuditionVoice());
    t->closeSliceEditor();assert(!t->sliceEditor.active);
    // Every layout keeps its existing voice limit and only the focused pads
    // become an editor keyboard. Other parts retain normal performance.
    for(int mode:{1,2,3,4}) {
        t=splitFixture();t->setPerformanceMode(mode);t->controls.algorithm=Engine::PlaybackAlgorithm::slices;
        t->controls.sampleRegionMode=Engine::RegionMode::equal;t->controls.sampleRegionCount=64;
        t->setSliceEditorParam("slice_editor_open","1");assert(t->sliceEditor.active);
        const int per=mode==3?8:16;t->setSliceEditorParam("slice_editor_page","1");
        const int pad=t->padForPart(0,per-1);t->padNoteOn(68+pad,100);
        assert(t->sliceEditor.selected==per*2-1&&t->sliceAuditionVoice()->part==0);
        if(mode!=4) {
            t->padNoteOn(68+t->padForPart(1,0),100);renderSplit(*t,8);
            assert(std::any_of(t->voices.begin(),t->voices.end(),[](const auto& v){return v.active&&v.held&&v.part==1&&!v.sliceAudition;}));
        }
        t->setSliceEditorParam("slice_editor_lane",mode==2?"4":"3");
        assert(t->selectedLane==static_cast<std::size_t>(mode==2?3:mode==4?2:0));
        t->selectSplitPart(1);assert(!t->sliceEditor.active&&!t->sliceAuditionVoice());
    }
    std::puts("s3g Thru Slice Edit: nominal/pre-roll bounds, held-reader isolation, audition latch/record isolation, Undo/add/merge, source lanes and all part layouts: PASS");
}
void testSliceEditorOverlap() {
    auto a=std::make_unique<CoreFixture>(),reference=std::make_unique<CoreFixture>();
    for(std::size_t channel=0;channel<2;++channel)for(std::size_t f=0;f<CoreFixture::size;++f) {
        const float value=(f<4096?.5f:-.5f)+.04f*std::sin(f*.03f+channel*.5f);
        a->storage[channel][f]=reference->storage[channel][f]=value;
    }
    Engine::Controls c;c.algorithm=Engine::PlaybackAlgorithm::slices;c.sampleMode=Engine::SampleMode::loop;
    c.sampleRegionOverride=c.sampleRegionAbsolute=c.sampleEditCrossfade=true;c.sampleRegionLow=0;c.sampleRegionHigh=.5f;
    c.join=0;
    for(int f=0;f<3500;++f){a->render(c,1);reference->render(c,1);}
    float previous=a->output[0],peak=0;const auto original=c;
    c.sampleRegionLow=.5f;c.sampleRegionHigh=1;
    for(int f=0;f<1800;++f){
        const float now=a->render(c,1);reference->render(original,1);
        assert(std::isfinite(now));peak=std::max(peak,std::abs(now-previous));previous=now;
        if(f==0)assert(std::abs(now-reference->output[0])<1e-6f); // true outgoing reader, not a hard reset
        if(f==10)assert(std::abs(now-reference->output[0])<.005f);
        if(f==30){c.sampleRegionLow=.6f;c.sampleRegionHigh=.95f;} // retarget mid-overlap
        if(f==80){c.sampleRegionLow=.55f;c.sampleRegionHigh=.9f;}
    }
    assert(peak<.02f&&previous<-.4f);
    // Ordinary held readers must not reset when only the source selector or
    // slice count changes; their explicit cached bounds own the interval.
    c.sampleEditCrossfade=false;c.sampleRegionLow=0;c.sampleRegionHigh=.5f;
    a->render(c,3000);c.sampleRegionMode=Engine::RegionMode::edited;c.sampleRegionCount=57;c.sampleSlice=30;
    previous=a->output[0];for(int f=0;f<700;++f){const float now=a->render(c,1);assert(now>.4f);assert(std::abs(now-previous)<.01f);previous=now;}
    auto cropped=std::make_unique<CoreFixture>();c.sampleRegionLow=.2f;c.sampleRegionHigh=.4f;
    c.locus=.1f;c.field=.1f;cropped->render(c,4000);assert(cropped->output[0]==0);
    previous=0;peak=0;float level=0;
    for(int f=0;f<12000;++f) {
        c.locus=.1f+f*.00002f;const float now=cropped->render(c,1);
        peak=std::max(peak,std::abs(now-previous));level=std::max(level,std::abs(now));previous=now;
    }
    assert(peak<.02f&&level>.4f); // entering/leaving a cropped loop is tapered
    std::puts("s3g Thru slice audition overlap: moving stereo readers, queued retargets, bounded transition slope and held-config stability: PASS");
}
void testSliceEditorBackgroundZoom() {
    auto t=splitFixture();t->setPerformanceMode(0);renderSplit(*t,10);t->stopBackgroundWork();
    t->waveDirtyMask=t->pendingRegionAnalysisMask=t->pendingCaptureSaveMask=0;
    t->backgroundStarted=true; // inspect bounded callback transfer with worker stopped
    t->setSliceEditorParam("slice_editor_open","1");t->setSliceEditorParam("slice_editor_view","0.125,0.875");
    t->serviceBackgroundWork();assert(t->laneSnapshot.editorZoom&&t->laneSnapshot.copied<=4096);
    assert(t->laneSnapshot.firstFrame==1024&&t->laneSnapshot.frames==6144);
    t->setSliceEditorParam("slice_editor_view","0.5,0.75");t->serviceBackgroundWork();
    assert(t->laneSnapshot.viewToken==t->sliceEditor.viewRequest&&t->laneSnapshot.firstFrame==4096&&t->laneSnapshot.frames==2048);
    assert(t->laneJob.load()==ThruInstance::backgroundRequested);
    t->backgroundStarted=false;resumeTestBackgroundWorker(*t);
    for(int n=0;n<4000&&t->sliceEditor.viewComplete!=t->sliceEditor.viewRequest;++n){renderSplit(*t,1);usleep(200);}
    assert(t->sliceEditor.viewComplete==t->sliceEditor.viewRequest&&t->sliceEditor.readyLow==.5f&&t->sliceEditor.readyHigh==.75f);
    assert(t->waveDirtyMask==0&&t->pendingRegionAnalysisMask==0);
    // One-frame zoom fills the view rather than putting all signal in bin 1.
    t->setSliceEditorParam("slice_editor_view","0.50001,0.50002");
    for(int n=0;n<4000&&t->sliceEditor.viewComplete!=t->sliceEditor.viewRequest;++n){renderSplit(*t,1);usleep(200);}
    assert(t->sliceEditor.viewComplete==t->sliceEditor.viewRequest);
    assert(t->sliceEditor.minimum.front()==t->sliceEditor.minimum.back());
    assert(t->sliceEditor.maximum.front()==t->sliceEditor.maximum.back());
    t->stopBackgroundWork();t->backgroundStarted=true;
    t->setSliceEditorParam("slice_editor_view","0,1");t->serviceBackgroundWork();assert(t->laneJob.load()==ThruInstance::backgroundCopying);
    ++t->wavesetSourceRevisions[0];t->closeSliceEditor();t->serviceBackgroundWork();assert(t->laneJob.load()==ThruInstance::backgroundIdle);
    t->backgroundStarted=false;
    std::puts("s3g Thru slice waveform zoom: bounded snapshot copy, rapid coalescing, off-thread results, one-frame detail and stale-source cancellation: PASS");
}
void testSliceEditorPersistence() {
    char dir[]="/tmp/thru-slice-state.XXXXXX";assert(mkdtemp(dir));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(dir));t->stopBackgroundWork();t->stopSampleWorker();
    t->persistenceGeneration=2;
    auto& lane=t->clipSlots[9].sliceLanes[2];lane.preRollMs=6.7f;lane.edited.count=3;lane.edited.points={{.1f,.3f,.8f,.99f}};
    std::snprintf(lane.edited.labels[1].data(),24,"Clap");lane.file=lane.edited;t->controls.sampleRegionMode=Engine::RegionMode::transients;
    assert(t->writeClipFile());const auto path=std::string(dir)+"/thru-clips.bin";
    auto fresh=std::make_unique<ThruInstance>();assert(fresh->prepare(nullptr));assert(fresh->readClipFileFromPath(path.c_str()));
    const auto& copied=fresh->clipSlots[9].sliceLanes[2];assert(copied.preRollMs==6.7f&&copied.edited.count==3&&copied.edited.points==lane.edited.points);
    assert(!std::strcmp(copied.edited.labels[1].data(),"Clap")&&copied.file.count==3&&copied.file.points==lane.file.points);
    auto* f=std::fopen(path.c_str(),"rb");assert(f);std::fseek(f,0,SEEK_END);const auto size=std::ftell(f);std::rewind(f);
    SliceBytes bytes(static_cast<std::size_t>(size)-4);assert(std::fread(bytes.data(),1,bytes.size(),f)==bytes.size());std::fclose(f);
    assert(bytes[8]==36);stripSliceExtension(bytes);assert(bytes[8]==35);
    std::uint32_t checksum=2166136261u;for(auto b:bytes){checksum^=b;checksum*=16777619u;}
    f=std::fopen(path.c_str(),"wb");assert(f);assert(std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size());assert(writeU32(f,checksum));std::fclose(f);
    assert(fresh->readClipFileFromPath(path.c_str()));assert(!fresh->clipSlots[9].sliceLanes[2].edited.count);
    assert(fresh->clipSlots[0].sliceLanes[0].preRollMs== -2);
    for(int laneIndex=0;laneIndex<4;++laneIndex) {
        assert(fresh->slicePreRoll(laneIndex,1)==1); // legacy Transients, regardless of part
        assert(fresh->slicePreRoll(laneIndex,0)==0&&fresh->slicePreRoll(laneIndex,2)==0);
    }
    f=std::tmpfile();assert(f);assert(t->writeSliceExtension(f,t->clipSlots));std::rewind(f);
    assert(fresh->readSliceExtension(f,fresh->clipSlots));std::fseek(f,8,SEEK_SET);assert(writeU32(f,65));std::rewind(f);
    assert(!fresh->readSliceExtension(f,fresh->clipSlots));std::fclose(f);
    t->persistentStateDirty=false;t.reset();fresh.reset();removeBackgroundTestDirectory(dir);
    std::puts("s3g Thru slice storage: full checksummed format-36 bank, per-clip/lane labels/edits/pre-roll, genuine format-35 migration and corrupt-map rejection: PASS");
}

void testSliceSampleEditAlignment() {
    char folder[]="/tmp/thru-slice-edits.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
    for(std::size_t ch=0;ch<2;++ch)for(std::size_t f=0;f<8192;++f)t->engine.writableLane(0,ch)[f]=.1f*std::sin(f*.04f);
    assert(t->engine.adoptLane(0,8192,t->sampleRate));
    t->controls.algorithm=Engine::PlaybackAlgorithm::slices;t->controls.sampleRegionMode=Engine::RegionMode::file;
    auto& data=t->activeClipSlot()->sliceLanes[0];data.file.count=3;data.file.points={{0,.25f,.5f,1}};
    data.edited.count=3;data.edited.points={{0,.3f,.8f,1}};data.preRollMs=6;
    std::snprintf(data.file.labels[0].data(),24,"first");t->fileSliceMaps[0]=data.file;
    t->normalizeSelectedLane();waitForTestBackground(*t,true);assert(t->fileSliceMaps[0].count==3&&data.file.points[1]==.25f);
    t->reverseSelectedLane();waitForTestBackground(*t,true);
    assert(data.file.points[1]==.5f&&data.file.points[2]==.75f&&!std::strcmp(data.file.labels[2].data(),"first"));
    assert(std::abs(data.edited.points[1]-.2f)<1e-6f&&std::abs(data.edited.points[2]-.7f)<1e-6f);
    t->sampleTrimStart=.25f;t->sampleTrimEnd=.75f;t->trimSelectedLane();waitForTestBackground(*t,true);
    assert(t->engine.laneFrames(0)==4096&&data.file.count==2&&data.file.points[1]==.5f);
    assert(data.edited.count==2&&std::abs(data.edited.points[1]-.9f)<1e-6f&&data.preRollMs==6);
    // Archive WAVs contain audio without cue chunks: their bank metadata must
    // restore the imported map rather than silently falling back to Equal.
    assert(t->prepareClipBank(0));assert(t->preparedFileSlices[0].count==2&&t->preparedFileSlices[0].points==data.file.points);
    t->restoreUndo();assert(t->activeClipSlot()->sliceLanes[0].file.count==3);
    for(int i=0;i<4000&&(t->engine.laneFrames(0)!=8192||t->sampleLoadBusy()||t->pendingClipSourceMask);++i){renderSplit(*t,1);usleep(200);}
    assert(t->engine.laneFrames(0)==8192&&t->fileSliceMaps[0].count==3&&t->fileSliceMaps[0].points[1]==.5f);
    t.reset();removeBackgroundTestDirectory(folder);
    std::puts("s3g Thru slice source edits: Normalize, Reverse, Trim, labels, archived-WAV recall and general Undo retain aligned file/edited maps: PASS");
}
