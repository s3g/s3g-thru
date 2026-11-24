#define S3G_THRU_STORAGE_TESTING 1
#include "../src/dsp/thru_module.cpp"
#include <cassert>
#include <filesystem>
#include <iostream>
using J=s3g_thru_json::Value;
int main(){
 char folder[]="/tmp/thru-workspace.XXXXXX";assert(mkdtemp(folder));auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();t->setPerformanceMode(0);std::snprintf(t->composerPanelOwner.data(),65,"test");t->composerLease.store(ThruInstance::composerClock()+1000000);
 int serial=0;auto send=[&](const std::string& body,bool expect=true){const int id=++serial;const std::string packet="test|"+std::to_string(id)+"|"+body;setParam(t.get(),"composer_edit",packet.c_str());const bool ok=t->composerEditReceipts[id%64].load()==std::uint64_t(id*2+1);if(ok!=expect){std::cerr<<"Edit "<<body<<" result "<<ok<<" expected "<<expect<<" error "<<t->lastError.data()<<"\n";std::abort();}return id;};
 auto* c=t->activeClipSlot();send("name|0|0|42617373204f72626974");assert(std::string(c->name.data())=="Bass Orbit");send("name|0|0|-");assert(!c->name[0]);send("name|1|0|4e6f",false);
 send("note|0|0|0|100|200|60|100|255|0|255");send("note|0|0|0|100|350|60|90|255|0|255");assert(c->clipNoteCount==2&&c->clipNotes[0].editorId!=c->clipNotes[1].editorId);const auto first=c->clipNotes[0].editorId,second=c->clipNotes[1].editorId;
 send("note|0|0|"+std::to_string(first)+"|600|200|67|100|255|0|255");assert(c->clipNotes[0].editorId==second&&c->clipNotes[1].editorId==first&&c->clipNotes[0].note==60);
 send("note|0|0|"+std::to_string(first)+"|600|200|67|100|255|1|255");assert(c->clipNoteCount==1&&c->clipNotes[0].editorId==second);send("note|0|0|"+std::to_string(first)+"|600|200|67|100|255|0|255",false);
 send("point|0|0|0|0|2|255|0|0|1|0");send("point|0|0|0|0|2|255|0|1500|1|0");assert(c->clipPointCount==2);const auto point=c->clipPoints[1].editorId;send("point|0|0|"+std::to_string(point)+"|0|2|255|0|1000|1.5|0");assert(c->clipPoints[1].value==1.5f);
 send("cliptool|0|0|3|0");assert(c->clipPoints[1].value==1&&c->clipPointCount==2);assert(!t->clipPreparationInFlight());
 // Recorded gestures must replace only their touched interval and keep the
 // old take available after save, export/import and Undo Take.
 const auto* speed=midiParameterForKey("speed");assert(speed);send("armall|0|0|0");send("arm|0|0|"+std::to_string(speed->id)+"|1");send("record|0|0|1|2");assert(t->clipRecording);const auto notes=c->clipNoteCount;
 std::array<int16_t,256> out{};auto render=[&](int n){for(int i=0;i<n;++i)t->render(out.data(),128);};render(70);send("touch|0|0|"+std::to_string(speed->id)+"|1");const auto start=t->clipCaptureTimeMs();send("p|0|0|"+std::to_string(speed->id)+"|0.5");t->noteOn(72,100,-1);render(40);send("touch|0|0|"+std::to_string(speed->id)+"|0");const auto end=t->clipCaptureTimeMs();render(30);send("record|0|0|0|2");assert(!t->clipRecording&&c->clipNoteCount==notes&&c->touchCount==1&&c->takeCount==2);
 float value=0;assert(ThruInstance::sampleClipCurve(*c,int(LockParameter::speed),start-20,true,value)&&std::abs(value-1)<1e-5);assert(ThruInstance::sampleClipCurve(*c,int(LockParameter::speed),(start+end)/2,true,value)&&std::abs(value-.5f)<1e-5);assert(ThruInstance::sampleClipCurve(*c,int(LockParameter::speed),end+20,true,value)&&std::abs(value-1)<1e-5);
 t->setClipPlaying(false);assert(t->writeMetadataFiles());auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(folder));restored->stopBackgroundWork();assert(restored->clipSlots[0].touchCount==1);assert(ThruInstance::sampleClipCurve(restored->clipSlots[0],int(LockParameter::speed),(start+end)/2,true,value)&&std::abs(value-.5f)<1e-5);
 auto copy=std::make_unique<ClipSlot>();auto core=ThruInstance::composerClip(*c,0);ThruInstance::composerReadClip(*copy,core,120);assert(copy->touchCount==1);auto invalidCore=core;invalidCore["automation"].arrayValue()[0]["windows"]=J::Array{J(J::Array{J(999999),J(100)})};bool rejectedWindow=false;try{ThruInstance::composerReadClip(*copy,invalidCore,120);}catch(const std::exception&){rejectedWindow=true;}assert(rejectedWindow);ThruInstance::composerReadClip(*copy,core,120);assert(ThruInstance::sampleClipCurve(*copy,int(LockParameter::speed),end+20,true,value)&&std::abs(value-1)<1e-5);
 send("cliptool|0|0|4|0");assert(c->takeCount==1&&c->touchCount==0);assert(ThruInstance::sampleClipCurve(*c,int(LockParameter::speed),(start+end)/2,true,value)&&std::abs(value-1)<1e-5);
 // Phrase transforms preserve source/engine state and move notes with curves.
 const auto count=c->clipNoteCount;send("phrase|0|0|0|0|800|1|1|1|1|1");assert(c->clipNoteCount==count*2);send("cliptool|0|0|3|0");assert(c->clipNoteCount==count);send("phrase|0|0|3|0|800|7|1|0|1|1");assert(c->clipNotes[0].note==67);
 send("cliptool|0|0|5|0");assert(c->clipNoteCount==0&&c->clipPointCount>0);send("cliptool|0|0|3|0");assert(c->clipNoteCount>0);
 // Browser and external MIDI share pitch without sharing note-off ownership.
 send("record|0|0|1|1");t->midiPart=0;t->noteOn(60,100,-1);send("key|0|0|60|100|1");assert(t->findHeldVoiceForNote(60,-1)&&t->findHeldVoiceForNote(60,-2));render(10);send("key|0|0|60|100|0");assert(t->findHeldVoiceForNote(60,-1)&&!t->findHeldVoiceForNote(60,-2));t->noteOff(60,-1);send("record|0|0|0|1");
 // Capture runs in bounded chunks and exports exact stable identities.
 t->composerPerformanceClip=std::make_unique<ClipSlot>();t->composerPerformanceJob.store(1);for(int i=0;i<100&&t->composerPerformanceJob.load()==1;++i)t->captureComposerPerformance();assert(t->composerPerformanceJob.load()==2);assert(t->composerPerformanceClip->clipNoteCount==c->clipNoteCount);
 // Browser clock follows native applied phase, route and rate.
 t->composerPanelWatching.store(true);t->setPerformanceMode(6);t->sequenceLive.tracks[0].rate=6;t->sequenceLive.tracks[0].length=9;t->sequencePlayback=t->sequenceLive;t->resetSequenceClock();t->clipPlaying=true;t->sequenceBeat=1.125;t->captureComposerClock();const auto clock=t->composerClockJson();assert(clock.at("running").boolean());assert(clock.at("lanes").arrayValue().size()==4);assert(clock.at("lanes").arrayValue()[0].at("cells").arrayValue()[0].number()==9%9);
 // Future cell telemetry uses the same authored random/vertical traversal.
 for(int direction=0;direction<5;++direction){auto& lane=t->sequencePlayback.tracks[0];lane.direction=direction;lane.path=1;lane.rotation=3;t->sequenceDirections[0].reset(lane);t->composerClockLastFrame=0;t->renderClockSamples.fetch_add(1000);t->captureComposerClock();const auto route=t->composerClockJson().at("lanes").arrayValue()[0];const auto tick=std::int64_t(std::floor(route.at("phase").number()+1e-9));for(int next=0;next<16;++next)assert(route.at("cells").arrayValue()[next].number()==t->sequenceDirections[0].at(lane,tick+next).step);}
 // Readout collection never holds the publication seqlock between callbacks.
 t->composerWatchFocus=0;t->composerWatchCount=33;t->composerWatchCursor=0;for(int i=0;i<33;++i)t->composerWatchIds[i]=speed->id;captureComposerReadouts(*t);assert(!(t->composerReadoutSequence.load()&1u)&&t->composerWatchCursor==16);captureComposerReadouts(*t);assert(!(t->composerReadoutSequence.load()&1u));captureComposerReadouts(*t);assert(t->composerReadoutCount.load()==33&&t->composerWatchCursor==0);
 // Structural choices latch normally and cannot open touch windows.
 t->composerRecordSession=true;t->clipRecording=true;t->clipRecordingTake=1;t->composerRecordMask=2;t->composerArmAll=true;const auto spans=c->touchCount;ClipPoint structural;structural.type=ClipPointType::sequenceLane;structural.sourcePad=0;structural.parameter=1;t->composerBeginTouch(structural,true);assert(c->touchCount==spans);structural.type=ClipPointType::tableParameter;structural.parameter=s3g_thru_table::model;t->composerBeginTouch(structural,true);assert(c->touchCount==spans);t->clipRecording=false;t->composerRecordSession=false;
 // Clearing a whole clip must not leave invalid v62 intervals attached to an
 // empty loop, including when the clear originates on Move rather than browser.
 t->clipPlaying=false;c->touchCount=1;c->touchSpans[0]={std::uint16_t(LockParameter::speed),0,10,100};ThruInstance::rebuildTouchLanes(*c);t->clearClipSlot(0);assert(!c->touchCount&&c->touchLanes.none());assert(t->writeMetadataFiles());
 // Cursor authoring inserts a baseline, updates one point per gesture and
 // keeps sound defaults, selection and sample assignments intact.
 t->setPerformanceMode(0);t->clipRecording=false;t->setClipPlaying(false);t->clearClipSlot(0);
 auto author=[&](const char* key,int part,int time,float value,int gesture,bool expect=true,std::uint32_t explicitRoute=0xffffffffu){const auto* p=midiParameterForKey(key);assert(p);const auto point=composerControlPoint(*t,*p,part,value);const auto route=explicitRoute==0xffffffffu?(std::uint32_t(point.type)<<24)|(std::uint32_t(point.sourcePad)<<16)|point.parameter:explicitRoute;const auto& cfg=t->settingsForPart(part).engines;return send("at|0|"+std::to_string(part)+"|"+std::to_string(p->id)+"|"+std::to_string(time)+"|"+std::to_string(value)+"|"+std::to_string(t->tableEditLane(part))+"|"+std::to_string((cfg.enabled?16:0)+cfg.edit+1)+"|"+std::to_string(gesture)+"|"+std::to_string(route),expect);};
 const auto speedBase=t->controls.speed;author("speed",0,1000,2,1);assert(c->clipPointCount==2&&t->controls.speed==speedBase);const auto editedId=c->clipPoints[1].editorId;author("speed",0,1000,3,1);assert(c->clipPointCount==2&&c->clipPoints[1].editorId==editedId&&c->clipPoints[1].value==3);send("cliptool|0|0|3|0");assert(c->clipPointCount==0);
 author("locus",0,500,.75f,2);assert(c->clipPointCount==2&&c->clipPoints[1].value==.75f);author("fm_feedback",0,800,30,3);assert(c->clipPointCount==4);author("waveset_process",0,900,9,4);bool foundProcess=false;for(int i=0;i<c->clipPointCount;++i)if(c->clipPoints[i].parameter==int(LockParameter::wavesetProcess)&&c->clipPoints[i].timeMs==900){assert(c->clipPoints[i].value==10);foundProcess=true;}assert(foundProcess);
 const auto beforeStale=c->clipPointCount;author("speed",0,200,.5f,5,false,1);assert(c->clipPointCount==beforeStale);
 // A cursor inside a touched interval edits that take, so playback hears it.
 c->takeCount=2;c->touchCount=1;c->touchSpans[0]={std::uint16_t(LockParameter::speed),1,100,400};ClipPoint touched;touched.type=ClipPointType::parameter;touched.parameter=ThruInstance::clipPointDescriptor(int(LockParameter::speed),1);touched.timeMs=100;touched.value=.5f;c->clipPoints[c->clipPointCount++]=touched;ThruInstance::rebuildClipPointIndex(*c);author("speed",0,300,.75f,6);assert(ThruInstance::sampleClipCurve(*c,int(LockParameter::speed),300,true,value)&&std::abs(value-.75f)<1e-5);
 t->setPerformanceMode(1);const int focus=t->splitFocus;author("speed",1,600,1.5f,7);assert(t->splitFocus==focus);bool foundB=false;for(int i=0;i<c->clipPointCount;++i)if(c->clipPoints[i].type==ClipPointType::partParameter&&c->clipPoints[i].sourcePad==1&&c->clipPoints[i].timeMs==600)foundB=true;assert(foundB);
 t->setPerformanceMode(5);t->layersSelectedCorner[1]=0;const auto* layer=midiParameterForKey("layers_fx2");const auto oldRoute=composerControlPoint(*t,*layer,0,.7f);const auto oldCode=(std::uint32_t(oldRoute.type)<<24)|(std::uint32_t(oldRoute.sourcePad)<<16)|oldRoute.parameter;t->layersSelectedCorner[1]=1;author("layers_fx2",0,700,.7f,8,false,oldCode);author("layers_fx2",0,700,.7f,9);assert(t->layersSelectedCorner[1]==1);
 // A new browser owner begins a new Undo gesture even if its local gesture
 // counter happens to match the former session.
 t->composerPanelOwnerGeneration.fetch_add(1);author("layers_fx2",0,700,.8f,9);send("cliptool|0|0|3|0");bool restoredLayer=false;for(int i=0;i<c->clipPointCount;++i)if(c->clipPoints[i].type==ClipPointType::layersParameter&&c->clipPoints[i].timeMs==700&&std::abs(c->clipPoints[i].value-.7f)<1e-5)restoredLayer=true;assert(restoredLayer);
 // Piano-roll limits are transactional and independent of the current focus.
 t->setPerformanceMode(0);t->setClipPlaying(false);t->clearClipSlot(0);send("loop|0|0|2000|1");
 auto addNote=[&](int part,int at,int length,bool ok=true){send("note|0|"+std::to_string(part)+"|0|"+std::to_string(at)+"|"+std::to_string(length)+"|60|100|255|0|255",ok);};
 for(int i=0;i<8;++i)addNote(0,100,200);assert(c->clipNoteCount==8);addNote(0,100,200,false);assert(c->clipNoteCount==8);
 addNote(0,300,100);assert(c->clipNoteCount==9);const auto last=c->clipNotes[8].editorId;
 send("note|0|0|"+std::to_string(last)+"|150|100|60|100|255|0|255",false);assert(c->clipNotes[8].startMs==300);
 send("loop|0|0|200|1",false);assert(c->clipDurationMs==2000&&c->clipNoteCount==9);
 send("phrase|0|0|5|0|500|0|1|0|1|1",false);assert(c->clipNoteCount==9);
 // Refusals preserve the previous successful edit's Undo.
 send("cliptool|0|0|3|0");assert(c->clipNoteCount==8);
 const auto firstFull=c->clipNotes[0].editorId;send("note|0|0|"+std::to_string(firstFull)+"|100|200|60|100|255|1|255");assert(c->clipNoteCount==7);
 t->clearClipSlot(0);send("loop|0|0|2000|1");for(int i=0;i<8;++i)addNote(0,1900,200);addNote(0,0,100,false);assert(c->clipNoteCount==8);
 t->clearClipSlot(0);send("loop|0|0|2000|1");for(int i=0;i<8;++i)addNote(0,1900,100);addNote(0,0,100);assert(c->clipNoteCount==9);
 t->setPerformanceMode(1);t->clearClipSlot(0);send("loop|0|0|2000|1");for(int i=0;i<8;++i)addNote(i%2,100,100);addNote(1,150,100,false);assert(c->clipNoteCount==8);
 // Bounded preflight agrees with a brute-force loop oracle (no rendering work).
 std::uint32_t budgetSeed=5;auto random=[&](int n){budgetSeed=budgetSeed*1664525u+1013904223u;return int(budgetSeed%std::uint32_t(n));};
 for(int trial=0;trial<250;++trial){const int span=1+random(40),count=random(25);ComposerNoteBudget budget{std::uint32_t(span)};std::array<int,40> active{};for(int i=0;i<count;++i){const int at=random(span),length=1+random(span);budget.add(at,length);for(int j=0;j<span;++j)if((j-at+span)%span<length)++active[j];}assert(budget.fits()==(*std::max_element(active.begin(),active.end())<=8));}
 t.reset();restored.reset();std::filesystem::remove_all(folder);std::cout<<"Clip workspace: identities, guarded edits, naming, Undo, touch recording/take persistence, phrase tools and native clock: PASS\n";
}
