// Regressions from the current-build audit: exercise real record/render/save
// paths, including an untouched overdub and a replayed duration-unit edge.
std::vector<ClipPoint> sequenceRecordedPoints(const ClipSlot& clip){
    std::vector<ClipPoint> result;
    for(std::size_t i=0;i<clip.clipPointCount;++i){const auto& p=clip.clipPoints[i];
        if(p.type==ClipPointType::sequenceLane||p.type==ClipPointType::sequenceRecipe)result.push_back(p);
    }
    return result;
}
void assertSequencePointsEqual(const std::vector<ClipPoint>& a,const std::vector<ClipPoint>& b){
    assert(a.size()==b.size());
    for(std::size_t i=0;i<a.size();++i)assert(a[i].type==b[i].type&&a[i].timeMs==b[i].timeMs&&a[i].value==b[i].value&&a[i].parameter==b[i].parameter&&a[i].sourcePad==b[i].sourcePad);
}
void testSequenceSourceRangeSafety(){
    using namespace s3g_thru_generate;
    Config config;assert(config.set(low,1,32,false)&&config.v[low]<config.v[high]);
    assert(config.set(high,0,32,false)&&config.v[low]<config.v[high]);
    char folder[]="/tmp/thru-range-safety.XXXXXX";assert(mkdtemp(folder));
    auto t=sequenceFixture();setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_recipe_map","5");
    for(const char* key:{"seq_recipe_from","seq_recipe_to"}){
        setParam(t.get(),key,!std::strcmp(key,"seq_recipe_from")?"1":"0");t->captureActiveClip(false);
        const auto& values=t->sequenceLive.tracks[0].generator.values;assert(values[10]<values[11]);
        auto snapshot=std::make_unique<ThruInstance::PersistenceSnapshot>();snapshot->clips=t->clipSlots;snapshot->generation=1;
        std::snprintf(snapshot->directory.data(),snapshot->directory.size(),"%s",folder);assert(t->writeClipFile(snapshot.get()));snapshot.reset();
        auto read=sequenceFixture();assert(read->readClipFileFromPath((std::string(folder)+"/thru-clips.bin").c_str()));
        assert(read->sequenceLive.tracks[0].generator.values==values);
    }
    // Reject an invalid in-memory bank without replacing the last good file.
    auto snapshot=std::make_unique<ThruInstance::PersistenceSnapshot>();snapshot->clips=t->clipSlots;snapshot->generation=2;
    std::snprintf(snapshot->directory.data(),snapshot->directory.size(),"%s",folder);
    snapshot->clips[0].sequence.tracks[0].generator.values[10]=.5f;snapshot->clips[0].sequence.tracks[0].generator.values[11]=.5f;
    assert(!t->writeClipFile(snapshot.get()));snapshot.reset();
    auto read=sequenceFixture();assert(read->readClipFileFromPath((std::string(folder)+"/thru-clips.bin").c_str()));read.reset();
    // A checksum-valid older bank may contain the known 0/0 or 1/1 bug.
    // Exercise the actual extension reader; corruption elsewhere stays rejected.
    const long rangeOffset=4+8+7*(4*7+8)*4+6*4+10*4;
    for(float endpoint:{0.f,1.f,.5f}){
        auto* f=std::tmpfile();assert(f&&t->writeSequenceTypes(f,t->clipSlots));
        assert(std::fseek(f,rangeOffset,SEEK_SET)==0&&writeF32(f,endpoint)&&writeF32(f,endpoint));std::rewind(f);
        auto clips=std::make_unique<std::array<ClipSlot,kClipSlotCount>>();
        const bool ok=t->readSequenceTypes(f,*clips);assert(ok==(endpoint!=.5f));
        if(ok){const auto& v=(*clips)[0].sequence.tracks[0].generator.values;assert(v[10]<v[11]&&v[10]>=0&&v[11]<=1);}
        std::fclose(f);
    }
    for(const auto range:{std::array<float,2>{.999999f,1.f},std::array<float,2>{0.f,.000001f}}){
        auto* f=std::tmpfile();assert(f&&t->writeSequenceTypes(f,t->clipSlots));
        assert(std::fseek(f,rangeOffset,SEEK_SET)==0&&writeF32(f,range[0])&&writeF32(f,range[1]));std::rewind(f);
        auto clips=std::make_unique<std::array<ClipSlot,kClipSlotCount>>();assert(t->readSequenceTypes(f,*clips));
        const auto& v=(*clips)[0].sequence.tracks[0].generator.values;assert(v[10]==range[0]&&v[11]==range[1]);std::fclose(f);
    }
    t.reset();std::filesystem::remove_all(folder);
    std::puts("Sequence range safety: endpoint edits save/reopen, writer protects good bank, known endpoint recovery and unrelated-invalid rejection: PASS");
}
void testSequenceOverdubSafety(){
    for(int type:{1,2}){
        auto t=sequenceFixture();setParam(t.get(),"seq_length","8");setParam(t.get(),"seq_type",type==1?"1":"2");setParam(t.get(),"seq_recipe_hits","2");
        t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();renderSplit(*t,40);
        setParam(t.get(),"seq_recipe_hits","6");setParam(t.get(),"seq_recipe_span","17");renderSplit(*t,40);
        setParam(t.get(),"seq_recipe_hits","3");setParam(t.get(),"seq_recipe_span","9");renderSplit(*t,40);t->finishClipRecording();
        t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);
        const auto original=sequenceRecordedPoints(*t->activeClipSlot());
        t->clipRecordMode=ClipRecordMode::overdub;t->beginClipRecording();renderSplit(*t,50);
        float expected=0;assert(ThruInstance::sampleClipCurve(*t->activeClipSlot(),kClipRecipeOffset,t->clipPositionMs,false,expected));
        assert(expected==6&&t->sequencePlayback.tracks[0].generator.values[1]==6&&t->sequenceParameterValue("seq_recipe_hits")==6);
        const auto builds=t->sequenceRecipeBuilds;for(int i=0;i<100;++i)t->serviceSequenceRecipePlayback();assert(builds==t->sequenceRecipeBuilds);
        t->finishClipRecording();assertSequencePointsEqual(original,sequenceRecordedPoints(*t->activeClipSlot()));
        t->undoLastClipTake();assertSequencePointsEqual(original,sequenceRecordedPoints(*t->activeClipSlot()));
        // Touch only HITS and LEN/RATE. Untouched SPAN still follows its curve.
        t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);t->beginClipRecording();
        const auto* rate=s3g_thru_midi::find(3841);assert(rate);
        setParam(t.get(),"seq_length","5");midiWrite(*t,*rate,0,6);setParam(t.get(),"seq_recipe_hits","4");renderSplit(*t,50);
        assert(t->sequencePlayback.tracks[0].length==5&&t->sequencePlayback.tracks[0].rate==6&&t->sequencePlayback.tracks[0].generator.values[1]==4);
        assert(t->sequenceParameterValue("seq_length")==5&&t->sequenceParameterValue("seq_rate")==6);
        assert(t->sequencePlayback.tracks[0].generator.values[13]==17);
        renderSplit(*t,40); // Cross the later original HITS edge as well.
        assert(t->sequencePlayback.tracks[0].generator.values[1]==4);
        t->finishClipRecording();assert(!t->sequenceOverrides[0][0]&&!t->sequenceRecipeOverrides[0][1]);
        t->clipPositionMs=150;t->serviceSequenceBeforeBlock(1);assert(t->sequencePlayback.tracks[0].generator.values[1]==4);
        t->clipPositionMs=250;t->serviceSequenceBeforeBlock(1);assert(t->sequencePlayback.tracks[0].generator.values[1]==4);
        t->undoLastClipTake();assertSequencePointsEqual(original,sequenceRecordedPoints(*t->activeClipSlot()));
        t->clipPositionMs=150;t->serviceSequenceBeforeBlock(1);assert(t->sequencePlayback.tracks[0].length==8&&t->sequencePlayback.tracks[0].rate==4&&t->sequencePlayback.tracks[0].generator.values[1]==6);
        // Starting overdub while stopped seeds from the replay context too.
        t->setClipPlaying(false);t->clipPositionMs=150;t->beginClipRecording();renderSplit(*t,1);
        assert(t->sequencePlayback.tracks[0].generator.values[1]==6);t->finishClipRecording();
    }
    std::puts("Sequence overdub: untouched recipe/scalar curves preserved, effective HITS readback, immediate LEN/RATE/HITS takeover, independent SPAN, cached work, take Undo and stopped entry: PASS");
}
void testSequenceUnitReadback(){
    auto t=sequenceFixture();setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_duration_mode","1");setParam(t.get(),"seq_recipe_duration","1");
    t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();renderSplit(*t,30);setParam(t.get(),"seq_duration_mode","0");renderSplit(*t,30);t->finishClipRecording();
    t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);
    assert(t->sequencePlayback.tracks[0].timeMode&&t->sequenceParameterValue("seq_duration_mode")==1);
    const auto* unit=s3g_thru_midi::find(3864);assert(unit&&midiRead(*t,*unit,0)==1);
    char timeState[4096]{},stepState[4096]{};assert(t->sequenceState(timeState,sizeof(timeState))>0);
    t->clipPositionMs=t->activeClipSlot()->clipDurationMs-1;t->serviceSequenceBeforeBlock(1);
    assert(!t->sequencePlayback.tracks[0].timeMode&&t->sequenceParameterValue("seq_duration_mode")==0);assert(t->sequenceState(stepState,sizeof(stepState))>0);
    if(const char* path=std::getenv("S3G_THRU_UNIT_FIXTURE")){auto* f=std::fopen(path,"wb");assert(f);assert(std::fprintf(f,"[%s,%s]\n",timeState,stepState)>0);std::fclose(f);}
    // A TIME-bound edit must use milliseconds even if the stored end UNIT is STEP.
    t->clipPositionMs=0;t->serviceSequenceBeforeBlock(1);setParam(t.get(),"seq_recipe_min","250");renderSplit(*t,1);
    assert(t->sequencePlayback.tracks[0].timeMode&&t->sequencePlayback.tracks[0].generator.values[17]==250);
    assert(t->sequenceParameterValue("seq_recipe_min")==250);
    t->clipPositionMs=t->activeClipSlot()->clipDurationMs-1;t->serviceSequenceBeforeBlock(1);
    assert(!t->sequencePlayback.tracks[0].timeMode&&std::abs(t->sequenceParameterValue("seq_recipe_min")-2)<.0001f);
    t->clipPositionMs=0;t->serviceSequenceBeforeBlock(1);assert(t->sequenceParameterValue("seq_recipe_min")==250);
    std::puts("Sequence duration units: effective TIME/STEP getter and JSON, replay-aware MIN editing and native UI fixture: PASS");
}
void testSequenceInvalidEditSafety(){
    auto t=sequenceFixture();t->beginClipRecording();setParam(t.get(),"speed",".75");t->captureClipParameter(int(LockParameter::speed));renderSplit(*t,30);t->finishClipRecording();t->setClipPlaying(false);
    const auto curve=kClipPartParameterOffset+std::size_t(LockParameter::speed);const auto count=t->activeClipSlot()->clipPointCounts[curve];assert(count>0);
    const auto revision=t->persistentRevision;
    for(const char* invalid:{"invalid","","nan","inf","0.75garbage","1e100"}){
        setParam(t.get(),"speed",invalid);assert(t->controls.speed==.75f&&t->activeClipSlot()->clipPointCounts[curve]==count&&t->persistentRevision==revision);
    }
    setParam(t.get(),"seq_rate","invalid");assert(t->sequenceLive.tracks[0].rate==4);
    setParam(t.get(),"speed"," .75 ");assert(t->activeClipSlot()->clipPointCounts[curve]==0);t->restoreUndo();assert(t->activeClipSlot()->clipPointCounts[curve]==count);
    assert(validLockParameterText("algorithm","Pulsar")&&!validLockParameterText("algorithm","Bad engine"));
    assert(validLockParameterText("stochastic_source_hold","On")&&validLockParameterText("waveset_group","4"));
    setParam(t.get(),"algorithm","Pulsar");assert(int(t->controls.algorithm)==int(Engine::PlaybackAlgorithm::pulsar));
    std::puts("Sequence input safety: malformed/nonfinite/trailing-junk API values preserve automation/state, valid same-value takeover and Undo, textual engine choices: PASS");
}
void testSequenceAuditRepairs(){testSequenceSourceRangeSafety();testSequenceOverdubSafety();testSequenceUnitReadback();testSequenceInvalidEditSafety();}
