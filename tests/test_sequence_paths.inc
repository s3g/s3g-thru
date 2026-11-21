void testSequenceGridPaths(){
    using namespace s3g_thru_sequence;
    const int vertical13[]{0,8,1,9,2,10,3,11,4,12,5,6,7};
    const int diagonal16[]{0,1,8,2,9,3,10,4,11,5,12,6,13,7,14,15};
    for(int i=0;i<13;++i)assert(gridPath(1,13).cells[i]==vertical13[i]);
    for(int i=0;i<16;++i)assert(gridPath(2,16).cells[i]==diagonal16[i]);
    for(int length=1;length<=32;++length)for(int path=0;path<3;++path){
        const auto& order=gridPath(path,length);std::array<bool,32> seen{};
        for(int i=0;i<length;++i){const int cell=order.cells[i];assert(cell>=0&&cell<length&&!seen[cell]);seen[cell]=true;assert(order.ordinals[cell]==i);}
        for(int direction=0;direction<5;++direction)for(int rotation=0;rotation<32;++rotation){
            Lane lane;lane.length=length;lane.path=path;lane.direction=direction;lane.rotation=rotation;lane.directionSeed=1979;
            DirectionTraversal traversal;traversal.reset(lane);
            for(int tick=0;tick<128;++tick){const auto at=directionPosition(lane,tick),actual=traversal.at(lane,tick);
                assert(at.step>=0&&at.step<length&&at.step==actual.step&&at.cycle==actual.cycle&&at.cycle==tick/directionPeriod(lane,direction));}
            if(direction<2){seen.fill(false);for(int tick=0;tick<length;++tick){const int cell=directionPosition(lane,tick).step;assert(!seen[cell]);seen[cell]=true;}}
        }
    }
    Lane lane;lane.length=16;DirectionTraversal traversal;traversal.reset(lane);
    assert(traversal.at(lane,3).step==3);lane.path=1;traversal.synchronize(lane,3,3);
    assert(traversal.at(lane,3).step==3&&traversal.at(lane,4).step==11); // Continue down the same column.
    lane.path=0;traversal.reset(lane);lane.path=1;traversal.synchronize(lane,3,4);
    assert(traversal.at(lane,4).step==4&&traversal.at(lane,5).step==12); // Tick 4 already sounded early.
    lane.path=2;traversal.synchronize(lane,3,4);assert(traversal.at(lane,4).step==4);
    for(int direction:{2,3}){
        lane={};lane.length=16;lane.direction=direction;traversal.reset(lane);
        const auto previous=traversal.at(lane,18);lane.path=1;traversal.synchronize(lane,18,18);
        const int ordinal=gridPath(1,16).ordinals[previous.step];
        assert(traversal.at(lane,18).step==previous.step&&traversal.at(lane,19).step==gridPath(1,16).cells[ordinal-1]);
    }
    // Historical early/late events retain their prior route during repeated edits.
    lane={};lane.length=13;traversal.reset(lane);
    for(int tick=0;tick<1000;++tick){
        const auto committed=traversal.at(lane,tick+1);
        lane.path=tick%3;lane.direction=tick%5;traversal.synchronize(lane,tick,tick+1);
        const auto retained=traversal.at(lane,tick+1);assert(retained.step==committed.step&&retained.cycle==committed.cycle);
        for(int n=std::max(0,tick-2);n<tick+4;++n)assert(traversal.at(lane,n).step<13);
    }
    // Clock scheduling is independent of host block size across every route.
    for(int path=0;path<3;++path)for(int direction=0;direction<5;++direction){
        Pattern pattern;pattern.active=true;
        for(int p=0;p<4;++p){auto& l=pattern.tracks[p];l.path=path;l.direction=direction;l.directionSeed=1979+p;l.length=7+p*3;l.rotation=p;l.rate=6+p%3;
            for(int i=0;i<32;++i){auto& step=l.trigs[i];step.enabled=i%3!=1;step.micro=i%2?.41f:-.37f;step.probability=.72f;setCount(step,1+i%8);step.cycleEvery=2;step.cycleHit=1;}}
        for(int rate:{44100,48000}){
            const auto timeline=[&](int block){std::vector<std::array<std::int64_t,5>> all;EventList events;
                for(int n=0;n<rate/4;){const int frames=std::min(block,rate/4-n);schedule(pattern,n*2.0/rate,2.0/rate,frames,events);assert(!events.overflow);
                    for(int i=0;i<events.size;++i){const auto& e=events.items[i];all.push_back({n+e.offset,e.lane,e.step,e.child,e.tick});}n+=frames;}return all;};
            const auto reference=timeline(1);for(int block:{31,128,511})assert(reference==timeline(block));
        }
    }
    // Rests consume clock ticks; every-two-cycles conditions use the full path.
    Pattern condition;condition.active=true;for(int p=1;p<4;++p)condition.tracks[p].enabled=false;
    auto& l=condition.tracks[0];l.length=13;l.path=1;l.rate=4;
    for(int i=0;i<13;++i){l.trigs[i].enabled=true;l.trigs[i].cycleEvery=2;l.trigs[i].cycleHit=2;}
    EventList events;schedule(condition,0,.001,8000,events);assert(events.size==13);
    for(int i=0;i<13;++i)assert(events.items[i].tick==i+13&&events.items[i].step==vertical13[i]);
    std::puts("Sequence PATH core: all lengths/paths/DIR/ROT, complete ragged grids, independent phases, early/late history, descending bounce continuity, block-independent Burst/probability/cycles: PASS");
}
void testSequencePathRuntime(){
    using namespace s3g_thru_sequence;
    auto t=sequenceFixture();for(int part=0;part<4;++part){t->selectSplitPart(part);setParam(t.get(),"seq_length","13");setParam(t.get(),"seq_rate","6");
        for(int row=0;row<13;++row){auto& step=t->sequenceLive.tracks[part].trigs[row];step.enabled=true;step.locks.mask=3;step.locks.values[0]=float(row)/16;step.locks.values[1]=float(row%5-2);}
    }
    t->selectSplitPart(0);t->commitSequenceEdit();t->startClipPlaybackNow();renderSplit(*t,20);
    const auto phase=t->sequenceBeat;const auto offsets=t->sequenceBeatOffsets;
    const auto* midi=s3g_thru_midi::find(3912);assert(midi&&std::strcmp(midi->key,"seq_path")==0);
    midiWrite(*t,*midi,1,1);assert(t->splitFocus==0&&t->sequenceLive.tracks[1].path==1&&t->sequenceBeat==phase&&t->sequenceBeatOffsets==offsets);
    renderSplit(*t,1);assert(midiRead(*t,*midi,1)==1&&t->sequenceLive.tracks[0].path==0);
    double energy=0;
    for(int path=0;path<3;++path){setParam(t.get(),"seq_path",std::to_string(path).c_str());
        for(int block=0;block<100;++block){std::array<float,kBlockFrames*2> output{};t->render(nullptr,kBlockFrames,output.data());
            for(float sample:output){assert(std::isfinite(sample));energy+=sample*sample;}
            std::array<int,4> counts{};for(const auto& voice:t->voices)if(voice.active&&voice.sequence.active){++counts[voice.part];
                const auto& step=t->sequencePlayback.tracks[voice.part].trigs[voice.sequence.step];
                assert(voice.sequence.position==step.locks.values[0]&&voice.note==t->settingsForPart(voice.part).root+int(step.locks.values[1]));}
            for(int count:counts)assert(count<=2);
        }
    }
    assert(energy>.001);for(auto count:t->sequenceEventCount)assert(count>0);
    t->setClipPlaying(false);renderSplit(*t,300);for(const auto& voice:t->voices)assert(!voice.active||!voice.sequence.active);
    // Record all lane paths through the real MIDI/clip/render path.
    for(int part=0;part<4;++part){t->selectSplitPart(part);setParam(t.get(),"seq_path","0");}t->selectSplitPart(0);
    t->clipRecordMode=ClipRecordMode::replace;t->beginClipRecording();renderSplit(*t,30);midiWrite(*t,*midi,1,1);renderSplit(*t,30);midiWrite(*t,*midi,1,2);renderSplit(*t,30);t->finishClipRecording();t->setClipPlaying(false);
    const auto curve=kClipSequencePathOffset+1;const auto* clip=t->activeClipSlot();const auto first=clip->clipPointStarts[curve];assert(clip->clipPointCounts[curve]==3);
    const auto change=clip->clipPoints[first+1].timeMs;
    t->startClipPlaybackNow();renderSplit(*t,1);assert(midiRead(*t,*midi,1)==0);t->clipPositionMs=change;t->serviceSequenceBeforeBlock(1);assert(midiRead(*t,*midi,1)==1);
    const auto recorded=sequenceRecordedPoints(*clip);t->clipRecordMode=ClipRecordMode::overdub;t->beginClipRecording();renderSplit(*t,5);t->finishClipRecording();assertSequencePointsEqual(recorded,sequenceRecordedPoints(*clip));t->undoLastClipTake();
    t->beginClipRecording();midiWrite(*t,*midi,1,0);renderSplit(*t,60);assert(t->sequencePlayback.tracks[1].path==0);t->finishClipRecording();t->clipPositionMs=change+20;t->serviceSequenceBeforeBlock(1);assert(midiRead(*t,*midi,1)==0);t->undoLastClipTake();assertSequencePointsEqual(recorded,sequenceRecordedPoints(*clip));
    // A normal authored edit replaces only this part's recorded PATH curve.
    midiWrite(*t,*midi,1,2);assert(clip->clipPointCounts[curve]==0&&clip->clipPointCounts[kClipSequencePathOffset]>0);
    t->setClipPlaying(false);t->startClipPlaybackNow();renderSplit(*t,1);assert(midiRead(*t,*midi,1)==2);
    std::puts("Sequence PATH runtime: lane-addressed MIDI, retained phase, rendered trig locks/pitch, two-voice stealing/release, recorded replay/overdub/Undo and control-specific authored replacement: PASS");
}
void testSequencePathStorage(){
    auto t=sequenceFixture();setParam(t.get(),"seq_path","1");t->selectSplitPart(1);setParam(t.get(),"seq_path","2");t->selectSplitPart(0);t->captureActiveClip(false);
    t->clipSlots[1]=t->clipSlots[0];t->clipSlots[1].sequence.tracks[0].path=2;
    char directory[]="/tmp/thru-path-storage.XXXXXX";assert(mkdtemp(directory));const auto path=std::string(directory)+"/thru-clips.bin";
    const auto save=[&](ThruInstance& source){auto snapshot=std::make_unique<ThruInstance::PersistenceSnapshot>();snapshot->clips=source.clipSlots;snapshot->generation=1;
        std::snprintf(snapshot->directory.data(),snapshot->directory.size(),"%s",directory);assert(source.writeClipFile(snapshot.get()));};save(*t);
    auto restored=sequenceFixture();assert(restored->readClipFileFromPath(path.c_str()));assert(restored->sequenceLive.tracks[0].path==1&&restored->sequenceLive.tracks[1].path==2);
    restored->activateStoredClip(1,false);assert(restored->sequenceLive.tracks[0].path==2);restored.reset();
    const auto readBytes=[&](){auto* f=std::fopen(path.c_str(),"rb");assert(f);std::fseek(f,0,SEEK_END);const auto size=std::ftell(f);std::rewind(f);std::vector<unsigned char> bytes(std::size_t(size)-4);assert(std::fread(bytes.data(),1,bytes.size(),f)==bytes.size());std::fclose(f);return bytes;};
    const auto writeBytes=[&](const auto& bytes){std::uint32_t checksum=2166136261u;for(auto value:bytes){checksum^=value;checksum*=16777619u;}auto* f=std::fopen(path.c_str(),"wb");assert(f&&std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size()&&writeU32(f,checksum));std::fclose(f);};
    auto bytes=readBytes();bytes[clipLegacyTailSize(bytes)-(4+64*4*4)+4]=3;writeBytes(bytes);
    restored=sequenceFixture();const auto before=restored->sequenceLive;assert(!restored->readClipFileFromPath(path.c_str())&&s3g_thru_generate::sameLane(before.tracks[0],restored->sequenceLive.tracks[0]));restored.reset();
    // A format-45 Euclidean bank keeps SHIFT and defaults PATH to HORIZ.
    auto legacy=sequenceFixture();setParam(legacy.get(),"seq_type","1");setParam(legacy.get(),"seq_recipe_shift","2");legacy->captureActiveClip(false);save(*legacy);legacy.reset();
    bytes=readBytes();stripSequencePaths(bytes);assert(bytes[8]==45);writeBytes(bytes);
    assert(t->readClipFileFromPath(path.c_str()));for(const auto& lane:t->sequenceLive.tracks)assert(lane.path==0);assert(t->sequenceLive.tracks[0].generator.shift==2);
    if(const char* fixture=std::getenv("S3G_THRU_PATH_FIXTURE")){auto* file=std::fopen(fixture,"wb");assert(file);std::fputc('[',file);bool comma=false;
        for(int type=0;type<3;++type)for(int pathMode=0;pathMode<3;++pathMode){setParam(t.get(),"seq_type",std::to_string(type).c_str());setParam(t.get(),"seq_path",std::to_string(pathMode).c_str());
            for(int position=0;position<4;++position){t->clipPlaying=true;t->sequencePlayback=t->sequenceLive;t->sequencePublishedCursor[0].store(s3g_thru_sequence::directionPosition(t->sequenceLive.tracks[0],position).step);
                char state[4096]{};assert(t->sequenceState(state,sizeof(state))>0);assert(std::fprintf(file,"%s%s",comma?",":"",state)>0);comma=true;t->clipPlaying=false;}}
        std::fputs("]\n",file);std::fclose(file);
    }
    t.reset();std::filesystem::remove_all(directory);
    std::puts("Sequence PATH storage: format-46 clip ownership/copies, malformed-path atomic rejection, legacy-45 HORIZ default with retained SHIFT and native UI fixtures: PASS");
}
void testSequencePaths(){testSequenceGridPaths();testSequencePathRuntime();testSequencePathStorage();}
