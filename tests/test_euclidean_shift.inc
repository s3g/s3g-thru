void testEuclideanHitShift(){
    using namespace s3g_thru_generate;
    for(int length=1;length<=32;++length)for(int hits=0;hits<=length;++hits)for(int shift=0;shift<length;++shift){
        Lane base;base.length=length;Config config;config.v[s3g_thru_generate::hits]=float(hits);config.shift=shift;Context source;
        const auto result=build(base,config,source);assert(result.enabled==hits);
        for(int i=0;i<length;++i)assert(result.lane.trigs[i].enabled==euclid((i+length-shift)%length,hits,length));
        assert(result.lane.rotation==base.rotation);
    }
    auto t=sequenceFixture();setParam(t.get(),"seq_length","8");setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_recipe_hits","3");
    const auto check=[&](int shift){const auto& lane=t->sequencePlayback.tracks[0];for(int i=0;i<lane.length;++i)assert(lane.trigs[i].enabled==euclid((i+lane.length-shift)%lane.length,3,lane.length));};
    setParam(t.get(),"seq_recipe_shift","1");check(1);assert(t->sequenceLive.tracks[0].rotation==0);
    t->startClipPlaybackNow();renderSplit(*t,10);const auto beat=t->sequenceBeat;const auto offsets=t->sequenceBeatOffsets;
    setParam(t.get(),"seq_recipe_shift","2");assert(t->sequenceBeat==beat&&t->sequenceBeatOffsets==offsets);renderSplit(*t,1);check(2);
    setParam(t.get(),"seq_rotation","3");renderSplit(*t,1);check(2);assert(t->sequencePlayback.tracks[0].rotation==3);
    const auto* midi=s3g_thru_midi::find(3911);assert(midi&&std::strcmp(midi->key,"seq_recipe_shift")==0);
    t->selectSplitPart(1);setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_length","5");t->selectSplitPart(0);
    midiWrite(*t,*midi,1,4);renderSplit(*t,1);assert(t->splitFocus==0&&midiRead(*t,*midi,1)==4&&t->sequenceLive.tracks[0].generator.shift==2);
    assert(midiMaximum(*t,*midi,1)==4);
    // Manual overrides retain their cell ownership through a generated shift.
    setParam(t.get(),"seq_step","1");setParam(t.get(),"seq_toggle","1");const bool manual=t->sequenceLive.tracks[0].trigs[0].enabled;
    setParam(t.get(),"seq_recipe_shift","4");renderSplit(*t,1);assert(t->sequencePlayback.tracks[0].trigs[0].enabled==manual);
    setParam(t.get(),"seq_regenerate","1");renderSplit(*t,1);check(4);
    t->setClipPlaying(false);setParam(t.get(),"seq_recipe_shift","0");
    t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();renderSplit(*t,30);
    setParam(t.get(),"seq_recipe_shift","3");renderSplit(*t,30);setParam(t.get(),"seq_recipe_shift","1");renderSplit(*t,30);t->finishClipRecording();t->setClipPlaying(false);
    const auto curve=sequenceRecipeCurve(0,21);const auto* clip=t->activeClipSlot();assert(clip->clipPointCounts[curve]==3);
    const auto first=clip->clipPointStarts[curve];const auto change=clip->clipPoints[first+1].timeMs;
    t->startClipPlaybackNow();renderSplit(*t,1);check(0);t->clipPositionMs=change;t->serviceSequenceBeforeBlock(1);check(3);
    assert(t->sequenceParameterValue("seq_recipe_shift")==3);
    const auto before=sequenceRecordedPoints(*clip);t->clipRecordMode=ClipRecordMode::overdub;t->beginClipRecording();renderSplit(*t,10);t->finishClipRecording();
    assertSequencePointsEqual(before,sequenceRecordedPoints(*clip));t->undoLastClipTake();assertSequencePointsEqual(before,sequenceRecordedPoints(*clip));
    // Save/reload the new field and its clip gestures; all previous curve IDs stay fixed.
    t->setClipPlaying(false);t->captureActiveClip(false);char directory[]="/tmp/thru-shift-save.XXXXXX";assert(mkdtemp(directory));
    const auto save=[&](){auto snapshot=std::make_unique<ThruInstance::PersistenceSnapshot>();snapshot->clips=t->clipSlots;snapshot->generation=1;
        std::snprintf(snapshot->directory.data(),snapshot->directory.size(),"%s",directory);assert(t->writeClipFile(snapshot.get()));};save();
    const auto path=std::string(directory)+"/thru-clips.bin";auto restored=sequenceFixture();assert(restored->readClipFileFromPath(path.c_str()));
    assert(restored->sequenceLive.tracks[0].generator.shift==1&&restored->sequenceLive.tracks[1].generator.shift==4);
    restored->startClipPlaybackNow();restored->clipPositionMs=change;restored->serviceSequenceBeforeBlock(1);
    assert(restored->sequenceParameterValue("seq_recipe_shift")==3);restored.reset();
    // Older format-44 banks load with zero shift.
    t->clearRecordedPerformance();setParam(t.get(),"seq_recipe_shift","0");t->selectSplitPart(1);setParam(t.get(),"seq_recipe_shift","0");t->selectSplitPart(0);t->captureActiveClip(false);save();
    auto* file=std::fopen(path.c_str(),"rb");assert(file);std::fseek(file,0,SEEK_END);const auto size=std::ftell(file);std::rewind(file);
    std::vector<unsigned char> bytes(std::size_t(size)-4);assert(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);
    const auto writePayload=[&](const auto& payload){std::uint32_t checksum=2166136261u;for(auto value:payload){checksum^=value;checksum*=16777619u;}
        auto* output=std::fopen(path.c_str(),"wb");assert(output&&std::fwrite(payload.data(),1,payload.size(),output)==payload.size()&&writeU32(output,checksum));std::fclose(output);};
    auto corrupt=bytes;corrupt[corrupt.size()-2*(4+64*4*4)+4]=32;writePayload(corrupt);
    restored=sequenceFixture();const auto retained=restored->sequenceLive;
    assert(!restored->readClipFileFromPath(path.c_str())&&sameLane(retained.tracks[0],restored->sequenceLive.tracks[0]));restored.reset();
    stripSequenceShifts(bytes);assert(bytes[8]==44);writePayload(bytes);
    restored=sequenceFixture();assert(restored->readClipFileFromPath(path.c_str()));for(const auto& lane:restored->sequenceLive.tracks)assert(lane.generator.shift==0);
    restored.reset();t.reset();std::filesystem::remove_all(directory);
    std::puts("Euclidean SHIFT: exhaustive lengths/hits/offsets, visible trig displacement, independent ROT/phase, manual overrides, MIDI ownership/bounds, recorded replay/overdub/Undo and format-45/44 recall: PASS");
}
