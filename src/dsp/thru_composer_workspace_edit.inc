static ClipPoint composerControlPoint(ThruInstance&,const MidiParameter&,int,float);
// Collect a bounded batch per callback, then publish a complete immutable set.
// The seqlock covers only publication, never several audio callbacks.
void captureComposerReadouts(ThruInstance& t){
 if(!t.composerWatchCount||!t.composerPanelWatching.load()||t.composerLease.load()<ThruInstance::composerClock())return;
 const int patch=t.tableEditLane(t.composerWatchFocus),mode=t.performanceMode();
 if(t.composerWatchCursor&&(t.composerWatchClip!=t.activeClip||t.composerWatchPatch!=patch||t.composerWatchMode!=mode))t.composerWatchCursor=0;
 if(t.composerWatchCursor==0){t.composerWatchClip=t.activeClip;t.composerWatchPatch=patch;t.composerWatchMode=mode;}
 const int end=std::min(t.composerWatchCount,t.composerWatchCursor+16);
 for(int i=t.composerWatchCursor;i<end;++i){const auto* p=s3g_thru_midi::find(t.composerWatchIds[i]);float value=0;const bool valid=p&&midiPlaybackReadout(t,*p,t.composerWatchFocus,value);t.composerWatchPendingIds[i]=t.composerWatchIds[i];t.composerWatchPendingValues[i]=value;t.composerWatchPendingValid[i]=valid;const auto route=p?composerControlPoint(t,*p,t.composerWatchFocus,0):ClipPoint{};t.composerWatchPendingRoutes[i]=p&&ThruInstance::clipPointLane(route)<kClipCurveLaneCount?(std::uint32_t(route.type)<<24)|(std::uint32_t(route.sourcePad)<<16)|route.parameter:0xffffffffu;}
 t.composerWatchCursor=end;if(end!=t.composerWatchCount)return;
 t.composerReadoutSequence.fetch_add(1,std::memory_order_acq_rel);
 for(int i=0;i<end;++i){t.composerReadoutIds[i].store(t.composerWatchPendingIds[i]);t.composerReadoutValues[i].store(t.composerWatchPendingValues[i]);t.composerReadoutValid[i].store(t.composerWatchPendingValid[i]);t.composerReadoutRoutes[i].store(t.composerWatchPendingRoutes[i]);}
 t.composerReadoutClip.store(t.activeClip);t.composerReadoutPart.store(t.composerWatchFocus);t.composerReadoutPatch.store(patch);t.composerReadoutCount.store(end);t.composerWatchCursor=0;t.composerReadoutSequence.fetch_add(1,std::memory_order_release);
}
// Bound authored piano-roll polyphony before touching notes, Undo or voices.
// Encoded edges sort note-offs before note-ons at the same time. Wrapping
// intervals include notes still held from the preceding clip repetition.
struct ComposerNoteBudget {
    std::array<std::uint32_t,kClipNoteCapacity*2> edges{};
    std::uint32_t span;std::size_t count=0;int active=0;
    explicit ComposerNoteBudget(std::uint32_t duration):span(std::max(1u,duration)){}
    void add(std::uint32_t at,std::uint32_t length){
        at%=span;length=std::clamp(length,1u,span);if(length==span){++active;return;}
        const auto end=at+length;if(end>span){++active;edges[count++]=(end-span)*2;}else edges[count++]=end*2;
        edges[count++]=at*2+1;
    }
    bool fits(){if(active>int(kVoiceCount))return false;std::sort(edges.begin(),edges.begin()+count);for(std::size_t i=0;i<count;++i){active+=(edges[i]&1u)?1:-1;if(active>int(kVoiceCount))return false;}return true;}
};
// Explicit event identities survive sorting, overlapping notes and repeated edits.
static bool composerWorkspaceReady(const ThruInstance& t,int clip){return clip==t.activeClip&&!t.composerOwned.load()&&t.composerLease.load()>=ThruInstance::composerClock()&&!t.projectLocked()&&!t.namingBusy()&&!t.clipPreparationInFlight()&&!t.audioRecording();}
static ClipPoint composerControlPoint(ThruInstance& t,const MidiParameter& p,int part,float value){
    ClipPoint x;x.value=value;const int patch=t.tableEditLane(part);int field=-1;x.sourcePad=255;
    int lock=p.lock;if(!std::strcmp(p.key,"part_level"))lock=int(t.partLevelParameter(part));if(!std::strcmp(p.key,"part_delay"))lock=int(t.partDelayParameter(part));if(!std::strcmp(p.key,"part_reverb"))lock=int(t.partReverbParameter(part));
    if(lock>=0){x.type=t.splitEnabled&&ThruInstance::splitPartParameter(lock)?ClipPointType::partParameter:ClipPointType::parameter;x.parameter=std::uint16_t(lock);if(x.type==ClipPointType::partParameter)x.sourcePad=std::uint8_t(part);return x;}
#define COMPOSER_ROUTE(ns,kind) if((field=ns::index(p.key))>=0){x.type=ClipPointType::kind;x.parameter=std::uint16_t(field);x.sourcePad=std::uint8_t(part*4+patch);return x;}
    COMPOSER_ROUTE(s3g_thru_table,tableParameter) COMPOSER_ROUTE(s3g_thru_scanned,scannedParameter) COMPOSER_ROUTE(s3g_thru_fm,fmParameter) COMPOSER_ROUTE(s3g_thru_noise,noiseParameter) COMPOSER_ROUTE(s3g_thru_reson,resonParameter) COMPOSER_ROUTE(s3g_thru_partials,partialsParameter) COMPOSER_ROUTE(s3g_thru_lowform,lowformParameter)
#undef COMPOSER_ROUTE
    if(!std::strcmp(p.key,"reson_density")){x.type=ClipPointType::resonDensity;x.sourcePad=std::uint8_t(part);return x;}
    if(!std::strcmp(p.key,"stack_scan_paused")){x.type=t.splitEnabled?ClipPointType::partHold:ClipPointType::stackHold;x.sourcePad=t.splitEnabled?std::uint8_t(part):255;return x;}
    if(!std::strncmp(p.key,"layers_",7)){const int index=t.layerAlias(p.key);if(index>=0){x.type=ClipPointType::layersParameter;x.parameter=std::uint16_t(index%192);x.sourcePad=std::uint8_t(index/192);return x;}}
    int lfoField=0;const int lfo=t.controlLfoKey(p.key,lfoField);if(lfo>=0&&lfoField<11){x.type=ClipPointType::pooledLfo;x.parameter=std::uint16_t(lfoField);x.sourcePad=std::uint8_t(lfo);return x;}
    const char* laneKeys[]{"seq_length","seq_rate","seq_rotation","seq_active","seq_mute","seq_velocity","seq_gate","seq_scale","seq_duration_mode","seq_time_ms","seq_direction","seq_path"};
    for(int i=0;i<12;++i)if(!std::strcmp(p.key,laneKeys[i])){x.type=ClipPointType::sequenceLane;x.parameter=std::uint16_t(i);x.sourcePad=std::uint8_t(part);return x;}
    for(int i=1;i<22;++i)if(!std::strcmp(p.key,ThruInstance::sequenceRecipeKeys[i])){x.type=ClipPointType::sequenceRecipe;x.parameter=std::uint16_t(i);x.sourcePad=std::uint8_t(part);return x;}
    // Unsupported settings remain ordinary controls, without an automation arm.
    x.parameter=65535;return x;
}
static void composerBeginParameterTouch(ThruInstance& t,const char* key){
    if(!t.clipRecording||!t.composerRecordSession)return;
    const int part=t.splitEnabled?t.splitFocus:0;if(t.sequenceSoundEditing(part))return;const auto* p=midiParameterForKey(key);if(!p||p->action)return;const auto route=composerControlPoint(t,*p,part,midiRead(t,*p,part));if(ThruInstance::clipPointLane(route)<kClipCurveLaneCount)t.composerBeginTouch(route,false,true);
}
template<class Finish>
bool composerWorkspaceEdit(ThruInstance& t,char** fields,int n,const std::array<double,25>& a,Finish finish){
    const std::string kind=fields[2];const auto integer=[](double v,int lo,int hi){return std::isfinite(v)&&v>=lo&&v<=hi&&v==std::floor(v);};
    const int part=int(a[1]);auto* c=t.activeClipSlot();const auto done=[&](bool ok){finish(ok);return true;};
    if(kind=="watch"&&n>=6){if(!integer(a[2],0,1))return done(false);for(int i=3;i<n-3;++i)if(!integer(a[i],0,16383)||!s3g_thru_midi::find(int(a[i])))return done(false);
        if(a[2]>0){t.composerWatchCount=0;t.composerWatchCursor=0;t.composerWatchFocus=part;}if(t.composerWatchFocus!=part||t.composerWatchCount+n-6>256)return done(false);for(int i=3;i<n-3;++i)t.composerWatchIds[t.composerWatchCount++]=int(a[i]);return done(true);}
    if(kind=="record"&&n==7){if(!integer(a[2],0,1)||!integer(a[3],1,3))return done(false);if(a[2]>0){if(t.clipRecording)return done(true);t.composerRecordConfigured=true;t.composerRecordMask=int(a[3]);t.composerPrepareEmptyClip();t.clipRecordMode=ClipRecordMode::overdub;t.beginClipRecording();return done(t.clipRecording);}t.finishClipRecording();return done(!t.clipRecording);}
    if(kind=="armall"&&n==6){if(!integer(a[2],0,1))return done(false);t.composerRecordConfigured=true;t.composerArmAll=a[2]>0;return done(true);}
    if((kind=="arm"||kind=="touch")&&n==7){const auto* p=midiParameterForKey("");for(const auto& m:s3g_thru_midi::parameters)if(m.id==int(a[2]))p=&m;if(!p||!integer(a[3],0,1))return done(false);auto point=composerControlPoint(t,*p,part,midiRead(t,*p,part));const auto lane=ThruInstance::clipPointLane(point);if(lane>=kClipCurveLaneCount)return done(false);
        if(kind=="arm"){t.composerRecordConfigured=true;t.composerArms.set(lane,a[3]>0);}else if(a[3]>0)t.composerBeginTouch(point,true);else t.composerCloseTouch(t.composerTouchSlot(lane));return done(true);}
    if(kind=="key"&&n==8){if(!integer(a[2],0,127)||!integer(a[3],0,127)||!integer(a[4],0,1))return done(false);const int old=t.midiPart,focus=t.splitFocus;t.midiPart=part;if(t.splitEnabled)t.selectSplitPart(part,false);
        if(a[4]>0){const auto age=t.noteClock;t.composerKeyboardDown=true;t.composerKeyboardLease=ThruInstance::composerClock()+1500;t.noteOn(int(a[2]),int(a[3]),-2);for(std::size_t i=0;i<t.voices.size();++i)if(t.voices[i].age>age&&t.voices[i].part==part)t.composerKeyboardAges[i]=t.voices[i].age;}
        else{t.noteOff(int(a[2]),-2);for(auto& age:t.composerKeyboardAges)for(const auto& v:t.voices)if(v.age==age&&v.note==int(a[2])&&v.part==part)age=0;}
        if(t.splitEnabled)t.selectSplitPart(focus,false);
        t.midiPart=old;return done(true);}
    if(kind=="keysalive"&&n==6){if(t.composerKeyboardDown)t.composerKeyboardLease=ThruInstance::composerClock()+1500;return done(true);}
    if(kind=="gestureoff"&&n==6){for(int i=0;i<128;++i)if(t.composerTouchExplicit[i])t.composerCloseTouch(i);if(a[2]>0)t.composerReleaseKeyboard();return done(true);}
    if(kind=="keyboardoff"&&n==6){t.composerReleaseKeyboard();return done(true);}
    if(t.clipRecording)return kind=="note"||kind=="point"||kind=="at"||kind=="phrase"||kind=="cliptool"||kind=="loop"?done(false):false;
    if(kind=="at"&&n==12){
        if(!integer(a[2],0,16383)||!integer(a[3],0,kClipMaximumDurationMs)||!integer(a[5],0,3)||!integer(a[6],1,24)||!integer(a[7],1,2147483647))return done(false);
        const auto* p=s3g_thru_midi::find(int(a[2]));if(!p||p->action||a[4]<p->minimum||a[4]>midiMaximum(t,*p,part)||(p->integer&&a[4]!=std::floor(a[4])))return done(false);
        const auto& cfg=t.settingsForPart(part).engines;const int token=(cfg.enabled?16:0)+std::clamp(cfg.edit,0,t.engineSlotCount()-1)+1;
        if(t.tableEditLane(part)!=int(a[5])||token!=int(a[6]))return done(false);
        auto point=composerControlPoint(t,*p,part,float(a[4]));const auto lane=ThruInstance::clipPointLane(point);if(lane>=kClipCurveLaneCount)return done(false);
        const auto route=(std::uint32_t(point.type)<<24)|(std::uint32_t(point.sourcePad)<<16)|point.parameter;if(!integer(a[8],0,2147483647)||std::uint32_t(a[8])!=route)return done(false);
        if(!std::strcmp(p->key,"waveset_process"))point.value=float(Engine::wavesetProcessAt(std::size_t(a[4])));
        const double duration=std::max(kClipMinimumDurationMs,c->clipDurationMs?c->clipDurationMs:std::uint32_t(std::clamp(240000./t.tempoBpm,50.,120000.)));if(a[3]>=duration)return done(false);point.timeMs=std::uint32_t(a[3]);
        std::bitset<256> windowTakes;int take=0;const ClipTouchSpan* active=nullptr;for(int i=0;i<c->touchCount;++i){const auto& w=c->touchSpans[i];if(w.lane!=lane)continue;windowTakes.set(w.take);double elapsed=double(point.timeMs)-w.startMs;if(elapsed<0)elapsed+=duration;if(elapsed<w.durationMs&&(!active||w.take>=active->take))active=&w;}
        if(active)take=active->take;else for(int i=0;i<c->clipPointCount;++i)if(ThruInstance::clipPointLane(c->clipPoints[i])==lane&&!windowTakes.test(ThruInstance::clipPointTake(c->clipPoints[i])))take=std::max(take,int(ThruInstance::clipPointTake(c->clipPoints[i])));
        point.parameter=ThruInstance::clipPointDescriptor(point.parameter&255u,take);if(!ThruInstance::composerValidPoint(point,std::max(int(c->takeCount),take+1)))return done(false);
        int index=-1;bool found=false;for(int i=0;i<c->clipPointCount;++i){const auto& old=c->clipPoints[i];if(ThruInstance::clipPointLane(old)!=lane)continue;found=true;if(old.timeMs==point.timeMs&&ThruInstance::clipPointTake(old)==take)index=i;}
        const int needed=(index<0?1:0)+(!found&&point.timeMs>0?1:0);if(std::size_t(c->clipPointCount+needed)>kClipPointCapacity)return done(false);
        const bool same=t.composerAuthorOwnerGeneration==t.composerPanelOwnerGeneration.load()&&t.composerUndoPerformance&&t.undoAvailable&&t.undoClipIndex==t.activeClip&&t.composerAuthorClip==t.activeClip&&t.composerAuthorParameter==p->id&&t.composerAuthorGesture==std::uint64_t(a[7]);
        if(!same){t.pushClipUndo(t.activeClip);t.composerUndoPerformance=true;}t.composerAuthorOwnerGeneration=t.composerPanelOwnerGeneration.load();t.composerAuthorClip=t.activeClip;t.composerAuthorParameter=p->id;t.composerAuthorGesture=std::uint64_t(a[7]);
        if(!found&&point.timeMs>0){auto baseline=composerControlPoint(t,*p,part,midiRead(t,*p,part));if(!std::strcmp(p->key,"waveset_process"))baseline.value=float(Engine::wavesetProcessAt(std::size_t(baseline.value)));baseline.timeMs=0;baseline.editorId=nextComposerEventId();c->clipPoints[c->clipPointCount++]=baseline;}
        if(index<0){index=c->clipPointCount++;point.editorId=nextComposerEventId();}else point.editorId=c->clipPoints[index].editorId;c->clipPoints[index]=point;c->takeCount=std::max<std::uint8_t>(c->takeCount,take+1);
        t.composerPrepareEmptyClip();ThruInstance::rebuildClipPointIndex(*c);t.composerPerformanceChanged();return done(true);
    }
    if(kind=="loop"&&n==7){if(!integer(a[2],kClipMinimumDurationMs,kClipMaximumDurationMs)||!integer(a[3],0,1))return done(false);ComposerNoteBudget budget{std::uint32_t(a[2])};for(int i=0;i<c->clipNoteCount;++i)budget.add(c->clipNotes[i].startMs,c->clipNotes[i].durationMs);if(!budget.fits())return done(false);t.pushClipUndo(t.activeClip);t.composerUndoPerformance=true;c->clipDurationMs=std::uint32_t(a[2]);if(!c->clipRecorded)c->recordedTempoBpm=t.tempoBpm;c->recordedBeats=float(c->clipDurationMs*c->recordedTempoBpm/60000.);c->timingMode=a[3]>0?ClipTimingMode::beatSync:ClipTimingMode::free;
        for(int i=0;i<c->clipNoteCount;++i){c->clipNotes[i].startMs%=c->clipDurationMs;c->clipNotes[i].durationMs=std::min(c->clipNotes[i].durationMs,c->clipDurationMs);}for(int i=0;i<c->clipPointCount;++i)c->clipPoints[i].timeMs%=c->clipDurationMs;for(int i=0;i<c->touchCount;++i)c->touchSpans[i].startMs%=c->clipDurationMs;t.clipPositionMs=std::fmod(t.clipPositionMs,c->clipDurationMs);ThruInstance::rebuildClipPointIndex(*c);auto ids=t.composerVoiceIds();t.composerCommitNotes(ids);return done(true);}
    if(kind=="note"&&n==13){if(!integer(a[2],0,2147483647)||!integer(a[3],0,kClipMaximumDurationMs)||!integer(a[4],1,kClipMaximumDurationMs)||!integer(a[5],0,127)||!integer(a[6],1,127)||!integer(a[7],0,255)||!integer(a[8],0,1)||!(integer(a[9],0,t.engineSlotCount()-1)||a[9]==255))return done(false);
        const auto id=std::uint32_t(a[2]);int index=-1;for(int i=0;i<c->clipNoteCount;++i)if(c->clipNotes[i].editorId==id&&id)index=i;
        if((id&&index<0)||(!id&&(a[8]>0||c->clipNoteCount>=kClipNoteCapacity)))return done(false);
        if(c->clipDurationMs&&(a[3]>=c->clipDurationMs||a[4]>c->clipDurationMs))return done(false);
        if(a[8]==0){ComposerNoteBudget budget{c->clipDurationMs?c->clipDurationMs:std::uint32_t(std::clamp(240000./t.tempoBpm,50.,120000.))};for(int i=0;i<c->clipNoteCount;++i)if(i!=index)budget.add(c->clipNotes[i].startMs,c->clipNotes[i].durationMs);budget.add(std::uint32_t(a[3]),std::uint32_t(a[4]));if(!budget.fits())return done(false);}
        t.pushClipUndo(t.activeClip);t.composerUndoPerformance=true;auto ids=t.composerVoiceIds();if(id)t.composerProtectNote(id);
        if(a[8]>0){t.releaseClipVoice(std::size_t(index));std::move(c->clipNotes.begin()+index+1,c->clipNotes.begin()+c->clipNoteCount,c->clipNotes.begin()+index);--c->clipNoteCount;}
        else{if(index<0){index=c->clipNoteCount++;c->clipNotes[index]={};c->clipNotes[index].editorId=nextComposerEventId();}auto& event=c->clipNotes[index];event.startMs=std::uint32_t(a[3]);event.durationMs=std::uint32_t(a[4]);event.note=std::uint8_t(a[5]);event.velocity=std::uint8_t(a[6]);event.sourcePad=std::uint8_t(a[7]);event.part=t.splitEnabled?std::uint8_t(part+1):0;if(event.engineSlot!=int(a[9])){event.engineSlot=std::uint8_t(a[9]);if(a[9]<8){const auto& cfg=t.settingsForPart(part).engines;event.engineAlgorithm=cfg.engines[int(a[9])];event.engineVariation=cfg.variations[int(a[9])];}else event.engineAlgorithm=255;}}
        t.composerCommitNotes(ids,id,a[8]>0);return done(true);}
    if(kind=="point"&&n==13){if(!integer(a[2],0,2147483647)||!integer(a[3],0,int(ClipPointType::resonDensity))||!integer(a[4],0,255)||!integer(a[5],0,255)||!integer(a[6],0,kClipTakeLimit-1)||!integer(a[7],0,kClipMaximumDurationMs)||!integer(a[9],0,1))return done(false);
        const auto id=std::uint32_t(a[2]);int index=-1;for(int i=0;i<c->clipPointCount;++i)if(c->clipPoints[i].editorId==id&&id)index=i;
        if((id&&index<0)||(!id&&(a[9]>0||c->clipPointCount>=kClipPointCapacity)))return done(false);
        ClipPoint next=index>=0?c->clipPoints[index]:ClipPoint{};next.type=static_cast<ClipPointType>(int(a[3]));next.parameter=ThruInstance::clipPointDescriptor(int(a[4]),int(a[6]));next.sourcePad=std::uint8_t(a[5]);next.timeMs=std::uint32_t(a[7]);next.value=a[8];
        if(ThruInstance::clipPointLane(next)>=kClipCurveLaneCount||!ThruInstance::composerValidPoint(next,std::max(1,int(a[6])+1))|| (c->clipDurationMs&&next.timeMs>=c->clipDurationMs))return done(false);
        if(index>=0&&(next.type!=c->clipPoints[index].type||next.parameter!=c->clipPoints[index].parameter||next.sourcePad!=c->clipPoints[index].sourcePad))return done(false);
        t.pushClipUndo(t.activeClip);t.composerUndoPerformance=true;
        if(a[9]>0){std::move(c->clipPoints.begin()+index+1,c->clipPoints.begin()+c->clipPointCount,c->clipPoints.begin()+index);--c->clipPointCount;}
        else{if(index<0){index=c->clipPointCount++;next.editorId=nextComposerEventId();}c->clipPoints[index]=next;c->takeCount=std::max<std::uint8_t>(c->takeCount,int(a[6])+1);}
        t.composerPrepareEmptyClip();ThruInstance::rebuildClipPointIndex(*c);t.composerPerformanceChanged();return done(true);}
    if(kind=="cliptool"&&n==7){if(!integer(a[2],0,8)||!integer(a[3],0,63))return done(false);const int operation=int(a[2]),destination=int(a[3]);t.clearError();
        if(operation==0)t.copyClipToClipboard(t.activeClip);
        else if(operation==1)t.pasteClipFromClipboard(destination);
        else if(operation==2){if(destination==t.activeClip||t.clipSlots[destination].occupied)return done(false);t.copyClipToClipboard(t.activeClip);t.pasteClipFromClipboard(destination);}
        else if(operation==3){if(t.composerUndoPerformance)t.composerRestorePerformanceUndo();else t.restoreUndo();}
        else if(operation==4)t.undoLastClipTake();
        else {t.pushClipUndo(t.activeClip);t.composerUndoPerformance=true;if(operation==5){t.releaseClipVoices();c->clipNoteCount=0;}if(operation==6){c->clipPointCount=0;c->touchCount=0;ThruInstance::indexSortedClipPoints(*c);}if(operation==7){t.releaseClipVoices();c->clipNoteCount=c->clipPointCount=c->touchCount=0;ThruInstance::indexSortedClipPoints(*c);}if(operation==8){if(!t.sequenceMode())return done(false);for(auto& lane:t.sequenceLive.tracks){lane.trigs={};lane.generator={};}t.sequenceLive.soundLocks.count=0;t.sequencePlayback=t.sequenceLive;c->sequence=t.sequenceLive;t.invalidateSequenceRecipePlayback();}t.composerPerformanceChanged();}
        return done(!t.lastError[0]);}
    if(kind=="phrase"&&n==13){if(!integer(a[2],0,6)||a[3]<0||a[4]<=a[3]||a[4]>kClipMaximumDurationMs||!std::isfinite(a[5])||!integer(a[6],0,1)||!integer(a[7],0,1)||!integer(a[8],0,1)||!integer(a[9],1,16))return done(false);
        const int op=int(a[2]),copies=int(a[9]);const double start=a[3],end=a[4],amount=a[5],span=end-start;const bool notes=a[6]>0,curves=a[7]>0,allParts=a[8]>0;
        if((op==2&& (amount<.0625||amount>16))||(op==3&&!integer(float(amount),-48,48)))return done(false);
        const auto belongs=[&](const ClipNote& note){return allParts||int(note.part?note.part-1:0)==part;};int countNotes=0,countPoints=0;std::bitset<kClipCurveLaneCount> selectedRoutes;
        for(int i=0;i<c->clipNoteCount;++i)if(notes&&belongs(c->clipNotes[i])&&c->clipNotes[i].startMs>=start&&c->clipNotes[i].startMs<end)++countNotes;
        for(int i=0;i<c->clipPointCount;++i)if(curves&&c->clipPoints[i].timeMs>=start&&c->clipPoints[i].timeMs<end&&t.composerPointPart(c->clipPoints[i],part,allParts)){++countPoints;selectedRoutes.set(ThruInstance::clipPointLane(c->clipPoints[i]));}
        if((op==0||op==5)&&(std::size_t(c->clipNoteCount+countNotes*copies)>kClipNoteCapacity||std::size_t(c->clipPointCount+countPoints*copies)>kClipPointCapacity))return done(false);
        const double maxTime=std::max(double(c->clipDurationMs),op==0?end+span*copies:op==2?start+span*amount:op==1||op==5?end+std::max(0.,amount)*(op==5?copies:1):double(c->clipDurationMs));if(maxTime>kClipMaximumDurationMs)return done(false);
        std::array<ClipTouchSpan,128> transformedWindows{};int windowCount=0;bool windowsFit=true;
        const auto addWindow=[&](ClipTouchSpan w,double from,double length){if(length<=0)return;if(windowCount>=128){windowsFit=false;return;}w.startMs=std::uint32_t(std::max(0.,from));w.durationMs=std::uint32_t(std::max(1.,length));transformedWindows[windowCount++]=w;};
        for(int i=0;i<c->touchCount;++i){const auto w=c->touchSpans[i];if(!curves||op==3||!selectedRoutes.test(w.lane)){addWindow(w,w.startMs,w.durationMs);continue;}
            const double oldDuration=std::max(1u,c->clipDurationMs);const int pieces=w.durationMs>=oldDuration?1:double(w.startMs)+w.durationMs>oldDuration?2:1;
            for(int piece=0;piece<pieces;++piece){const double lo=w.durationMs>=oldDuration?0:piece?0:w.startMs,hi=w.durationMs>=oldDuration?oldDuration:piece?double(w.startMs)+w.durationMs-oldDuration:std::min(oldDuration,double(w.startMs)+w.durationMs);const double a=std::max(lo,start),b=std::min(hi,end);
                if(a>=b){addWindow(w,lo,hi-lo);continue;}if(op==0||op==5){addWindow(w,lo,hi-lo);for(int copy=1;copy<=copies;++copy)addWindow(w,a+(op==0?span*copy:amount*copy),b-a);}
                else{addWindow(w,lo,a-lo);addWindow(w,b,hi-b);if(op==2)addWindow(w,start+(a-start)*amount,(b-a)*amount);else if(op==4)addWindow(w,start+end-b,b-a);else if(op==6){const double grid=std::max(1.,amount),at=std::round(a/grid)*grid,to=std::round(b/grid)*grid;addWindow(w,at,std::max(1.,to-at));}else addWindow(w,a+amount,b-a);}
            }
        }
        if(!windowsFit)return done(false);
        if(notes){ComposerNoteBudget budget{std::uint32_t(maxTime)};
            for(int i=0;i<c->clipNoteCount;++i){const auto& event=c->clipNotes[i];double at=event.startMs,length=event.durationMs;const bool selected=belongs(event)&&at>=start&&at<end;
                if(selected&&(op==0||op==5)){for(int copy=1;copy<=copies;++copy)budget.add(std::uint32_t(std::max(0.,at+(op==0?span*copy:amount*copy))),std::uint32_t(length));}
                else if(selected&&op!=3){if(op==2){at=start+(at-start)*amount;length=std::max(1.,length*amount);}else if(op==4)at=std::max(start,start+end-at-length);else if(op==6)at=std::round(at/std::max(1.,amount))*std::max(1.,amount);else at+=amount;}
                budget.add(std::uint32_t(std::max(0.,at)),std::uint32_t(length));
            }if(!budget.fits())return done(false);
        }
        t.pushClipUndo(t.activeClip);t.composerUndoPerformance=true;auto ids=t.composerVoiceIds();const auto noteCount=c->clipNoteCount,pointCount=c->clipPointCount;
        auto time=[&](double x,double offset){return op==2?start+(x-start)*amount:op==4?start+end-x:op==6?std::round(x/std::max(1.,amount))*std::max(1.,amount):x+offset;};
        for(int i=0;i<noteCount;++i){auto& event=c->clipNotes[i];if(!notes||!belongs(event)||event.startMs<start||event.startMs>=end)continue;t.composerProtectNote(t.composerNoteId(event));
            if(op==0||op==5){for(int copy=1;copy<=copies;++copy){auto n=event;n.editorId=nextComposerEventId();n.startMs=std::uint32_t(std::max(0.,double(n.startMs)+(op==0?span*copy:amount*copy)));c->clipNotes[c->clipNoteCount++]=n;}}
            else if(op==3)event.note=std::uint8_t(std::clamp(int(event.note)+int(amount),0,127));
            else{event.startMs=std::uint32_t(std::max(0.,op==4?std::max(start,start+end-event.startMs-event.durationMs):time(event.startMs,amount)));if(op==2)event.durationMs=std::uint32_t(std::max(1.,event.durationMs*amount));}
        }
        for(int i=0;i<pointCount;++i){auto& event=c->clipPoints[i];if(!curves||event.timeMs<start||event.timeMs>=end||!t.composerPointPart(event,part,allParts))continue;
            if(op==0||op==5){for(int copy=1;copy<=copies;++copy){auto p=event;p.editorId=nextComposerEventId();p.timeMs=std::uint32_t(std::max(0.,double(p.timeMs)+(op==0?span*copy:amount*copy)));c->clipPoints[c->clipPointCount++]=p;}}
            else if(op!=3)event.timeMs=std::uint32_t(std::max(0.,time(event.timeMs,amount)));
        }
        // Transform selected window sections and retain ownership outside the
        // selected phrase. Preparation below was capacity-checked before edits.
        c->touchCount=std::uint16_t(windowCount);c->touchSpans=transformedWindows;
        c->clipDurationMs=std::uint32_t(maxTime);c->recordedBeats=float(maxTime*c->recordedTempoBpm/60000.);for(int i=0;i<c->clipNoteCount;++i){c->clipNotes[i].startMs%=c->clipDurationMs;c->clipNotes[i].durationMs=std::min(c->clipNotes[i].durationMs,c->clipDurationMs);}for(int i=0;i<c->touchCount;++i)c->touchSpans[i].startMs%=c->clipDurationMs;for(int i=0;i<c->clipPointCount;++i)c->clipPoints[i].timeMs%=c->clipDurationMs;ThruInstance::rebuildClipPointIndex(*c);t.composerCommitNotes(ids);return done(true);
    }
    return false;
}
