// Clip authoring shares the live transport; no temporary preview owns the bank.
std::array<int,256> composerWatchIds{};int composerWatchCount=0,composerWatchFocus=0,composerWatchCursor=0,composerWatchClip=-1,composerWatchPatch=-1,composerWatchMode=-1;
std::array<int,256> composerWatchPendingIds{},composerWatchPendingValid{};std::array<float,256> composerWatchPendingValues{};std::array<std::uint32_t,256> composerWatchPendingRoutes{};
std::array<std::atomic<int>,256> composerReadoutIds{},composerReadoutValid{};
std::array<std::atomic<float>,256> composerReadoutValues{};std::array<std::atomic<std::uint32_t>,256> composerReadoutRoutes{};
std::uint64_t composerAuthorGesture=0,composerAuthorOwnerGeneration=0;int composerAuthorParameter=-1,composerAuthorClip=-1;
std::atomic<int> composerReadoutCount{0},composerReadoutPart{0},composerReadoutClip{-1},composerReadoutPatch{0};
std::atomic<std::uint32_t> composerReadoutSequence{0};

bool composerUndoPerformance=false;std::array<std::uint32_t,kRollCount> composerRollEditorIds{};
std::uint64_t composerPerformanceRevision=1;
std::atomic<std::uint64_t> composerPerformanceLiveRevision{1};
std::unique_ptr<ClipSlot> composerPerformanceClip;
std::atomic<int> composerPerformanceJob{0};
int composerPerformanceSlot=-1,composerPerformanceNoteCursor=0,composerPerformancePointCursor=0;
std::uint64_t composerPerformanceCapturedRevision=0,composerPerformanceCapturedOwnerGeneration=0,composerPerformancePersistentRevision=0,composerPerformanceAt=0,composerPerformancePublished=0;
std::string composerPerformancePath,composerPerformanceVersion;
std::array<char,65> composerPerformanceOwner{};
std::atomic<int> composerRecordPublished{0},composerPositionPublished{0},composerNotePublished{0},composerPointPublished{0};
std::atomic<int> composerMaskPublished{3};
std::array<int,5> composerPerformanceObserved{{-1,-1,-1,-1,-1}};
int composerRecordMask=3;
bool composerRecordConfigured=false,composerRecordSession=false,composerArmAll=true;
std::bitset<kClipCurveLaneCount> composerArms,composerTouched;
std::array<int,128> composerTouchRoutes{};std::array<float,128> composerTouchBases{};std::array<ClipPoint,128> composerTouchPoints{};
std::array<std::uint64_t,128> composerTouchLast{};
std::array<bool,128> composerTouchExplicit{};
std::array<int,128> composerTouchIndices{};
std::array<double,128> composerTouchStarts{};
std::array<std::uint64_t,kVoiceCount> composerReleaseAges{};
std::array<double,kVoiceCount> composerReleaseTimes{};
std::array<std::uint64_t,kVoiceCount> composerKeyboardAges{};bool composerKeyboardDown=false;std::uint64_t composerKeyboardLease=0;
void composerReleaseKeyboard(){if(!composerKeyboardDown)return;const int previous=midiPart;for(int part=0;part<(splitEnabled?partCount():1);++part)for(int note=0;note<128;++note)if(heldKeys[part][160+note].note>=0){midiPart=part;noteOff(note,-2);}midiPart=previous;composerKeyboardDown=false;composerKeyboardAges.fill(0);}
std::uint32_t composerNoteId(ClipNote& n){if(!n.editorId){n.editorId=nextComposerEventId();}return n.editorId;}
std::uint32_t composerPointId(ClipPoint& p){if(!p.editorId)p.editorId=nextComposerEventId();return p.editorId;}
void composerPerformanceChanged(){++composerPerformanceRevision;markPersistentStateDirty();}
static void rebuildTouchLanes(ClipSlot& c){c.touchLanes.reset();for(int i=0;i<c.touchCount;++i)if(c.touchSpans[i].lane<kClipCurveLaneCount)c.touchLanes.set(c.touchSpans[i].lane);}
static bool sampleTouchedClipCurve(const ClipSlot& c,std::size_t lane,double position,bool interpolate,float& value){
    const double duration=std::max(1u,c.clipDurationMs);const ClipTouchSpan* active=nullptr;std::bitset<256> takes;
    for(int i=0;i<c.touchCount;++i){const auto& w=c.touchSpans[i];if(w.lane!=lane)continue;takes.set(w.take);double elapsed=position-w.startMs;if(elapsed<0)elapsed+=duration;if(elapsed<w.durationMs&&(!active||w.take>=active->take))active=&w;}
    const ClipPoint* before=nullptr,*after=nullptr;double behind=duration+1,ahead=duration+1;
    const auto start=c.clipPointStarts[lane],count=c.clipPointCounts[lane];
    for(std::size_t i=start;i<std::size_t(start)+count;++i){const auto& p=c.clipPoints[i];const auto take=clipPointTake(p);
        if(active){if(take!=active->take)continue;double offset=double(p.timeMs)-active->startMs;if(offset<0)offset+=duration;if(offset>active->durationMs)continue;
            double here=position-active->startMs;if(here<0)here+=duration;const double delta=here-offset;
            if(delta>=0&&delta<=behind){before=&p;behind=delta;}if(delta<0&&-delta<ahead){after=&p;ahead=-delta;}
        }else{if(takes.test(take))continue;double b=position-p.timeMs;if(b<0)b+=duration;double a=p.timeMs-position;if(a<=0)a+=duration;if(b<=behind){before=&p;behind=b;}if(a<ahead){after=&p;ahead=a;}}
    }
    if(!before&&!after)return false;
    if(!before){value=after->value;return true;}
    value=before->value;if(interpolate&&after&&ahead+behind>0)value+=(after->value-value)*float(behind/(ahead+behind));return true;
}
static bool writeTouchExtension(std::FILE* f,const std::array<ClipSlot,kClipSlotCount>& clips){
    std::uint32_t bytes=8;for(const auto& c:clips)bytes+=4+20*c.touchCount;
    if(!writeU32(f,0x31484354u))return false;
    for(const auto& c:clips){if(!writeU32(f,c.touchCount))return false;for(int i=0;i<c.touchCount;++i){const auto& w=c.touchSpans[i];if(!writeU32(f,w.lane)||!writeU32(f,w.take)||!writeU32(f,w.startMs)||!writeU32(f,w.durationMs)||!writeU32(f,0))return false;}}
    return writeU32(f,bytes);
}
static bool readTouchExtension(std::FILE* f,std::array<ClipSlot,kClipSlotCount>& clips){
    std::uint32_t magic=0,bytes=8;if(!readU32(f,magic)||magic!=0x31484354u)return false;
    for(auto& c:clips){std::uint32_t count=0;if(!readU32(f,count)||count>c.touchSpans.size())return false;c.touchCount=std::uint16_t(count);bytes+=4+20*count;
        for(int i=0;i<c.touchCount;++i){std::uint32_t lane,take,start,length,reserved;if(!readU32(f,lane)||!readU32(f,take)||!readU32(f,start)||!readU32(f,length)||!readU32(f,reserved)||reserved||lane>=kClipCurveLaneCount||take>=std::uint32_t(std::max(1,int(c.takeCount)))||start>=c.clipDurationMs||!length||length>kClipMaximumDurationMs)return false;c.touchSpans[i]={std::uint16_t(lane),std::uint8_t(take),start,length};}rebuildTouchLanes(c);
    }std::uint32_t length=0;return readU32(f,length)&&length==bytes;
}
bool composerNotePoint(ClipPointType type)const{return type==ClipPointType::pressure||type==ClipPointType::notePressure||type==ClipPointType::midiExpression||type==ClipPointType::stackPosition||type==ClipPointType::rollPosition||type==ClipPointType::rollLanePosition;}
bool composerCaptureAllowed(ClipPointType type,std::size_t lane)const{
    if(!composerRecordConfigured)return true;
    if(composerNotePoint(type))return (composerRecordMask&1)!=0;
    return (composerRecordMask&2)&&lane<kClipCurveLaneCount&&(composerArmAll||composerArms.test(lane));
}
int composerTouchSlot(std::size_t lane)const{for(int i=0;i<128;++i)if(composerTouchIndices[i]&&composerTouchRoutes[i]==int(lane))return i;return -1;}
void composerRestoreTouchBase(const ClipPoint& point,float value){
    const int field=point.parameter&255u,owner=point.sourcePad;const bool local=point.type==ClipPointType::partParameter;auto& base=splitEnabled&&local&&owner!=splitFocus?splitParts[owner].controls:controls;
    if(point.type==ClipPointType::parameter||local){applyLockedValue(base,outputGain,field,value);if(splitMixerParameter(field))splitMixerOverrides[field]=false;return;}
    const int part=owner/4,lane=owner%4;auto& sound=splitEnabled&&part!=splitFocus?splitParts[part].controls:controls;
#define TOUCH_RESTORE(pointType,member,ns,overrides) if(point.type==ClipPointType::pointType&&owner<16&&field<ns::fields){ns::set(sound.member.patches[lane],field,value);overrides.reset(owner*ns::fields+field);captureTablePrograms(*activeClipSlot());return;}
    TOUCH_RESTORE(tableParameter,table,s3g_thru_table,tableOverrides) TOUCH_RESTORE(scannedParameter,scanned,s3g_thru_scanned,scannedOverrides) TOUCH_RESTORE(fmParameter,fm,s3g_thru_fm,fmOverrides) TOUCH_RESTORE(noiseParameter,noise,s3g_thru_noise,noiseOverrides) TOUCH_RESTORE(resonParameter,reson,s3g_thru_reson,resonOverrides) TOUCH_RESTORE(partialsParameter,partials,s3g_thru_partials,partialsOverrides) TOUCH_RESTORE(lowformParameter,lowform,s3g_thru_lowform,lowformOverrides)
#undef TOUCH_RESTORE
    if(point.type==ClipPointType::resonDensity&&owner<4){auto& c=splitEnabled&&owner!=splitFocus?splitParts[owner].controls:controls;c.reson.density=value;resonDensityOverrides[owner]=false;return;}
    if(point.type==ClipPointType::sequenceLane&&owner<4){setSequenceLaneValue(sequenceLive,owner,field,value);sequenceOverrides[owner][field]=false;sequencePlayback=sequenceLive;publishSequenceRates();syncSequenceRatePhases();invalidateSequenceRecipePlayback();activeClipSlot()->sequence=sequenceLive;return;}
    if(point.type==ClipPointType::sequenceRecipe&&owner<4){auto& recipe=sequenceLive.tracks[owner].generator;if(field==21)recipe.shift=int(value);else recipe.values[field]=value;sequenceRecipeOverrides[owner][field]=false;materializeSequenceType(owner);sequencePlayback=sequenceLive;invalidateSequenceRecipePlayback();activeClipSlot()->sequence=sequenceLive;}
}
void composerCloseTouch(int index){
    auto* c=activeClipSlot();if(index<0||index>=128||!composerTouchIndices[index])return;
    const int w=composerTouchIndices[index]-1;if(c&&w<c->touchCount)c->touchSpans[w].durationMs=std::uint32_t(std::clamp(clipCaptureAbsoluteMs()-composerTouchStarts[index],1.,double(kClipMaximumDurationMs)));
    composerTouched.reset(composerTouchRoutes[index]);composerTouchIndices[index]=0;composerRestoreTouchBase(composerTouchPoints[index],composerTouchBases[index]);
}
void composerCloseTouches(){for(int i=0;i<128;++i)composerCloseTouch(i);composerRecordSession=false;}
static float composerBaseline(const ClipSlot& c,const ClipPoint& p){
    const auto field=p.parameter&255u;const int owner=p.sourcePad;
    if(p.type==ClipPointType::parameter&&field<kLockParameterCount)return c.baseValues[field];
    if(p.type==ClipPointType::partParameter&&owner<4&&field<kLockParameterCount)return c.splitValues[owner][field];
#define TOUCH_BASE(pointType,member,ns) if(p.type==ClipPointType::pointType&&owner<16&&field<ns::fields)return c.member[c.splitEnabled?owner/4+1:0].patches[owner%4].v[field];
    TOUCH_BASE(tableParameter,tablePrograms,s3g_thru_table) TOUCH_BASE(scannedParameter,scannedPrograms,s3g_thru_scanned) TOUCH_BASE(fmParameter,fmPrograms,s3g_thru_fm) TOUCH_BASE(noiseParameter,noisePrograms,s3g_thru_noise) TOUCH_BASE(resonParameter,resonPrograms,s3g_thru_reson) TOUCH_BASE(partialsParameter,partialsPrograms,s3g_thru_partials) TOUCH_BASE(lowformParameter,lowformPrograms,s3g_thru_lowform)
#undef TOUCH_BASE
    return p.value;
}
void composerBeginTouch(const ClipPoint& point,bool explicitTouch=false,bool knownBaseline=false){
    if(!composerRecordSession||!clipRecording||clipRecordingTake==0||composerNotePoint(point.type))return;
    // Note expression and Layers gestures keep their native held/latching
    // behavior. Engine selection and patch starters also keep their new choice.
    const auto type=point.type;const int field=point.parameter&255u;
    if(type!=ClipPointType::parameter&&type!=ClipPointType::partParameter&&type!=ClipPointType::tableParameter&&type!=ClipPointType::scannedParameter&&type!=ClipPointType::fmParameter&&type!=ClipPointType::noiseParameter&&type!=ClipPointType::resonParameter&&type!=ClipPointType::partialsParameter&&type!=ClipPointType::lowformParameter&&type!=ClipPointType::resonDensity&&type!=ClipPointType::sequenceLane&&type!=ClipPointType::sequenceRecipe)return;
    if((type==ClipPointType::parameter||type==ClipPointType::partParameter)&&(field>=int(kLockParameterCount)||kComposerDiscreteLocks[field]))return;
    if((type==ClipPointType::tableParameter||type==ClipPointType::scannedParameter||type==ClipPointType::fmParameter||type==ClipPointType::noiseParameter||type==ClipPointType::resonParameter||type==ClipPointType::partialsParameter||type==ClipPointType::lowformParameter)&&field==0)return;

    if(type==ClipPointType::tableParameter&&(s3g_thru_table::discrete(field)||field==s3g_thru_table::model))return;
    if(type==ClipPointType::scannedParameter&&s3g_thru_scanned::discrete(field))return;
    if(type==ClipPointType::fmParameter&&s3g_thru_fm::discrete(field))return;
    if(type==ClipPointType::noiseParameter&&s3g_thru_noise::discrete(field))return;
    if(type==ClipPointType::resonParameter&&s3g_thru_reson::discrete(field))return;
    if(type==ClipPointType::partialsParameter&&s3g_thru_partials::discrete(field))return;
    if(type==ClipPointType::lowformParameter&&s3g_thru_lowform::discrete(field))return;
    if(type==ClipPointType::sequenceRecipe||(type==ClipPointType::sequenceLane&&field!=5&&field!=6&&field!=9))return;
    const auto lane=clipPointLane(point);if(lane>=kClipCurveLaneCount||!composerCaptureAllowed(point.type,lane))return;
    int index=composerTouchSlot(lane);
    if(index<0){auto* c=activeClipSlot();if(!c||c->touchCount>=c->touchSpans.size()||c->clipPointCount>=kClipPointCapacity-1){setError("Finish take: gesture capacity");return;}
        for(int i=0;i<128;++i)if(!composerTouchIndices[i]){index=i;break;}
        if(index<0)return;
        // A constant baseline also defines the untouched part of a new curve.
        bool baselineFound=false;for(int i=0;i<c->clipPointCount;++i)if(clipPointLane(c->clipPoints[i])==lane){baselineFound=true;break;}if(!baselineFound){ClipPoint baseline=point;baseline.editorId=nextComposerEventId();baseline.value=composerBaseline(*c,point);baseline.timeMs=0;baseline.parameter=clipPointDescriptor(point.parameter&255u,0);c->clipPoints[c->clipPointCount++]=baseline;}
        composerTouchBases[index]=(knownBaseline||explicitTouch)?point.value:composerBaseline(*c,point);composerTouchPoints[index]=point;
        const int span=c->touchCount++;c->touchSpans[span]={std::uint16_t(lane),clipRecordingTake,clipCaptureTimeMs(),1};c->touchLanes.set(lane);
        composerTouchRoutes[index]=int(lane);composerTouchIndices[index]=span+1;composerTouchStarts[index]=clipCaptureAbsoluteMs();composerTouchExplicit[index]=false;composerTouched.set(lane);
    }
    composerTouchLast[index]=renderClockSamples.load();composerTouchExplicit[index]|=explicitTouch;
    if(auto* c=activeClipSlot())c->touchSpans[composerTouchIndices[index]-1].durationMs=std::uint32_t(std::clamp(clipCaptureAbsoluteMs()-composerTouchStarts[index],1.,double(kClipMaximumDurationMs)));
}
void serviceComposerWorkspace(){
    if(!clipRecording&&composerLease.load()<composerClock()){composerRecordConfigured=false;composerArmAll=true;composerArms.reset();}
    const auto now=renderClockSamples.load();for(int i=0;i<128;++i)if(composerTouchIndices[i]&&now-composerTouchLast[i]>std::uint64_t(sampleRate*(composerTouchExplicit[i]?1.5:.14)))composerCloseTouch(i);
    for(std::size_t i=0;i<voices.size();++i){auto& v=voices[i];if(composerReleaseAges[i]&&v.active&&v.age==composerReleaseAges[i]&&(!clipPlaying||clipAbsolutePositionMs>=composerReleaseTimes[i])){v.held=false;beginVoiceRelease(v);composerReleaseAges[i]=0;}
    }
    if(composerKeyboardDown&&(!composerPanelWatching.load()||composerLease.load()<composerClock()||composerKeyboardLease<composerClock()))composerReleaseKeyboard();
    composerPerformanceLiveRevision.store(composerPerformanceRevision);composerRecordPublished.store(clipRecording);composerMaskPublished.store(composerRecordMask);composerPositionPublished.store(int(clipPositionMs));
    if(const auto* c=activeClipSlot()){composerNotePublished.store(c->clipNoteCount);composerPointPublished.store(c->clipPointCount);const std::array<int,5> state{{activeClip,c->clipNoteCount,c->clipPointCount,c->takeCount,int(clipRecording)}};if(state!=composerPerformanceObserved){composerPerformanceObserved=state;++composerPerformanceRevision;}}
}
void composerRestorePerformanceUndo(){
    if(!composerUndoPerformance||!undoAvailable||undoClipIndex!=activeClip||clipRecording||audioRecording())return;
    auto* c=activeClipSlot();const auto& from=undoClip;releaseClipVoices();
    c->clipNoteCount=from.clipNoteCount;c->clipPointCount=from.clipPointCount;std::copy_n(from.clipNotes.begin(),c->clipNoteCount,c->clipNotes.begin());std::copy_n(from.clipPoints.begin(),c->clipPointCount,c->clipPoints.begin());c->touchSpans=from.touchSpans;c->touchCount=from.touchCount;c->clipDurationMs=from.clipDurationMs;c->recordedBeats=from.recordedBeats;c->recordedTempoBpm=from.recordedTempoBpm;c->clipRate=from.clipRate;c->timingMode=from.timingMode;c->takeCount=from.takeCount;c->clipRecorded=from.clipRecorded;sequenceLive=from.sequence;c->sequence=from.sequence;sequencePlayback=sequenceLive;invalidateSequenceRecipePlayback();indexSortedClipPoints(*c);composerUndoPerformance=false;undoAvailable=false;composerPerformanceChanged();
}
bool composerPointPart(const ClipPoint& p,int part,bool all)const{
    if(all||!splitEnabled)return true;
    switch(p.type){case ClipPointType::partParameter:case ClipPointType::partHold:case ClipPointType::notePressure:case ClipPointType::midiExpression:case ClipPointType::resonDensity:case ClipPointType::sequenceLane:case ClipPointType::sequenceRecipe:return p.sourcePad==part;
    case ClipPointType::tableParameter:case ClipPointType::scannedParameter:case ClipPointType::fmParameter:case ClipPointType::noiseParameter:case ClipPointType::resonParameter:case ClipPointType::lowformParameter:case ClipPointType::partialsParameter:return p.sourcePad/4==part;default:return part==0;}
}
void composerPrepareEmptyClip(){
    auto* c=activeClipSlot();if(!c)return;
    if(c->clipDurationMs<kClipMinimumDurationMs){c->recordedTempoBpm=tempoBpm;c->recordedBeats=4;c->clipDurationMs=std::uint32_t(std::clamp(240000./tempoBpm,double(kClipMinimumDurationMs),double(kClipMaximumDurationMs)));c->recordedBeats=float(c->clipDurationMs*tempoBpm/60000.);c->timingMode=ClipTimingMode::beatSync;}
    c->occupied=true;c->clipRecorded=true;c->takeCount=std::max<std::uint8_t>(1,c->takeCount);activeClipOccupied=true;
}
void composerRemapVoices(const std::array<std::uint32_t,kVoiceCount>& ids,std::uint32_t changed=0,bool deleted=false){
    auto* c=activeClipSlot();for(std::size_t i=0;i<rolls.size();++i){auto& roll=rolls[i];const auto id=composerRollEditorIds[i];if(!id||!roll.active||!roll.clipOwned)continue;int index=-1;for(int n=0;n<c->clipNoteCount;++n)if(c->clipNotes[n].editorId==id){index=n;break;}roll.clipNoteIndex=index;if(index<0||(id==changed&&deleted)){roll.held=false;roll.target=0;}}for(std::size_t v=0;v<voices.size();++v){auto& voice=voices[v];if(!ids[v]||!voice.active||!voice.clipOwned)continue;
        if(ids[v]==changed&&deleted){composerReleaseAges[v]=0;voice.held=false;beginVoiceRelease(voice);voice.clipNoteIndex=-1;continue;}
        int index=-1;for(int n=0;n<c->clipNoteCount;++n)if(c->clipNotes[n].editorId==ids[v]){index=n;break;}voice.clipNoteIndex=index;
    }
}
std::array<std::uint32_t,kVoiceCount> composerVoiceIds(){std::array<std::uint32_t,kVoiceCount> ids{};auto* c=activeClipSlot();composerRollEditorIds.fill(0);for(std::size_t i=0;i<rolls.size();++i)if(rolls[i].active&&rolls[i].clipOwned&&rolls[i].clipNoteIndex>=0&&rolls[i].clipNoteIndex<c->clipNoteCount)composerRollEditorIds[i]=composerNoteId(c->clipNotes[rolls[i].clipNoteIndex]);for(std::size_t i=0;i<voices.size();++i)if(voices[i].active&&voices[i].clipOwned&&voices[i].clipNoteIndex>=0&&voices[i].clipNoteIndex<c->clipNoteCount)ids[i]=composerNoteId(c->clipNotes[voices[i].clipNoteIndex]);return ids;}
void composerProtectNote(std::uint32_t id){auto* c=activeClipSlot();if(!c||!clipPlaying)return;for(std::size_t i=0;i<voices.size();++i){const auto& v=voices[i];if(v.active&&v.clipOwned&&v.clipNoteIndex>=0&&v.clipNoteIndex<c->clipNoteCount){const auto& n=c->clipNotes[v.clipNoteIndex];if(n.editorId!=id)continue;double elapsed=clipPositionMs-n.startMs;if(elapsed<0)elapsed+=c->clipDurationMs;composerReleaseAges[i]=v.age;composerReleaseTimes[i]=clipAbsolutePositionMs+std::max(1.,double(n.durationMs)-elapsed);}}}
void composerCommitNotes(const std::array<std::uint32_t,kVoiceCount>& ids,std::uint32_t changed=0,bool deleted=false){auto* c=activeClipSlot();std::sort(c->clipNotes.begin(),c->clipNotes.begin()+c->clipNoteCount,[](const auto& a,const auto& b){return a.startMs!=b.startMs?a.startMs<b.startMs:a.editorId<b.editorId;});composerRemapVoices(ids,changed,deleted);composerPrepareEmptyClip();composerPerformanceChanged();}
void captureComposerPerformance(){
    if(composerPerformanceJob.load(std::memory_order_acquire)!=1||!composerPerformanceClip)return;
    auto* source=activeClipSlot();if(!source)return;auto& to=*composerPerformanceClip;
    if(composerPerformanceSlot!=activeClip||composerPerformanceCapturedRevision!=composerPerformanceRevision){composerPerformanceSlot=activeClip;composerPerformanceCapturedRevision=composerPerformanceRevision;composerPerformancePersistentRevision=persistentRevision;composerPerformanceNoteCursor=composerPerformancePointCursor=0;captureSplitClip(to);to.layers=layersLive;to.clipNoteCount=source->clipNoteCount;to.clipPointCount=source->clipPointCount;to.clipDurationMs=source->clipDurationMs;to.recordedTempoBpm=source->recordedTempoBpm;to.recordedBeats=source->recordedBeats;to.takeCount=source->takeCount;to.clipRecorded=source->clipRecorded;to.clipRate=source->clipRate;to.timingMode=source->timingMode;to.touchCount=source->touchCount;to.touchSpans=source->touchSpans;composerPerformanceOwner=composerPanelOwner;composerPerformanceCapturedOwnerGeneration=composerPanelOwnerGeneration.load();}
    if(source->clipNoteCount<to.clipNoteCount||source->clipPointCount<to.clipPointCount){composerPerformanceCapturedRevision=0;return;}
    std::size_t budget=8192;
    while(composerPerformanceNoteCursor<to.clipNoteCount&&budget>=sizeof(ClipNote)){auto& n=source->clipNotes[composerPerformanceNoteCursor];composerNoteId(n);to.clipNotes[composerPerformanceNoteCursor++]=n;budget-=sizeof(ClipNote);}
    const auto count=std::min<std::size_t>(budget/sizeof(ClipPoint),to.clipPointCount-composerPerformancePointCursor);for(std::size_t i=0;i<count;++i){auto& p=source->clipPoints[composerPerformancePointCursor+i];composerPointId(p);to.clipPoints[composerPerformancePointCursor+i]=p;}composerPerformancePointCursor+=int(count);
    if(composerPerformanceNoteCursor==to.clipNoteCount&&composerPerformancePointCursor==to.clipPointCount){if(clipRecording)for(std::size_t key=0;key<clipOpenNoteIndices.size();++key){const int index=clipOpenNoteIndices[key];if(index>=0&&index<to.clipNoteCount)to.clipNotes[index].durationMs=std::uint32_t(std::clamp(clipCaptureAbsoluteMs()-clipOpenNoteStarts[key],1.,double(std::max(1u,to.clipDurationMs))));}composerPerformanceJob.store(2,std::memory_order_release);}
}
void publishComposerPerformance(){
    std::stable_sort(composerPerformanceClip->clipPoints.begin(),composerPerformanceClip->clipPoints.begin()+composerPerformanceClip->clipPointCount,[](const auto& a,const auto& b){const auto laneA=clipPointLane(a),laneB=clipPointLane(b);return laneA!=laneB?laneA<laneB:a.timeMs!=b.timeMs?a.timeMs<b.timeMs:clipPointTake(a)<clipPointTake(b);});
    auto core=composerClip(*composerPerformanceClip,composerPerformanceSlot);auto ids=ComposerJson::array();for(int n=0;n<composerPerformanceClip->clipNoteCount;++n)ids.push(double(composerPerformanceClip->clipNotes[n].editorId));
    std::map<std::tuple<int,int,int,int>,ComposerJson> groupedIds;for(int i=0;i<composerPerformanceClip->clipPointCount;++i){const auto& p=composerPerformanceClip->clipPoints[i];const auto key=std::make_tuple(int(p.type),int(p.parameter&255u),int(p.sourcePad),int(clipPointTake(p)));auto found=groupedIds.find(key);if(found==groupedIds.end())found=groupedIds.emplace(key,ComposerJson::array()).first;found->second.push(double(p.editorId));}
    auto pointIds=ComposerJson::array();for(auto& entry:groupedIds)pointIds.push(std::move(entry.second));
    auto payload=composerObject({{"client",composerPerformanceOwner.data()},{"epoch",composerEpoch},{"clip",composerPerformanceSlot},{"revision",std::to_string(composerPerformancePersistentRevision)},{"version",std::to_string(++composerPerformancePublished)},{"notes",core.at("notes")},{"automation",core.at("automation")},{"loop",core.at("loop")},{"noteIds",std::move(ids)},{"pointIds",std::move(pointIds)}});
    auto layerValues=ComposerJson::array();for(std::size_t i=LayerPatch::advanced;i<LayerPatch::size;++i)layerValues.push(composerPerformanceClip->layers.values[i]);payload["layers"]=composerObject({{"fx",int(LayerPatch::fx)},{"advanced",int(LayerPatch::advanced)},{"config",int(LayerPatch::config)},{"fields",int(kLockParameterCount)},{"values",std::move(layerValues)}});
    composerPerformancePath=composerRoot+"/performance-"+composerPerformanceOwner.data()+".json";s3g_thru_pack::atomicFile(composerPerformancePath,s3g_thru_json::stringify(payload));composerPerformanceVersion=std::to_string(composerPerformancePublished);
}
void processComposerWorkspace(){
    if(!composerPanelWatching.load()||composerLease.load()<composerClock())return;
    if(!composerPerformanceClip)composerPerformanceClip=std::make_unique<ClipSlot>();
    if(composerPerformanceJob.load(std::memory_order_acquire)==2){try{publishComposerPerformance();}catch(const std::exception& e){composerPanelError=e.what();composerPanelErrorClip=composerPerformanceSlot;}composerPerformanceJob.store(0,std::memory_order_release);}
    if(composerPerformanceJob.load()==0&&composerClock()>=composerPerformanceAt&&(composerPerformanceLiveRevision.load()!=composerPerformanceCapturedRevision||composerLiveActive.load()!=composerPerformanceSlot||composerRecordPublished.load()||composerLiveRevision.load()!=composerPerformancePersistentRevision||composerPerformanceCapturedOwnerGeneration!=composerPanelOwnerGeneration.load())){
        composerPerformanceAt=composerClock()+160;composerPerformanceCapturedRevision=0;composerPerformanceJob.store(1,std::memory_order_release);
    }
}
