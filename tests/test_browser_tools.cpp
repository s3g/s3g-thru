#define S3G_THRU_STORAGE_TESTING 1
#include "../src/dsp/thru_module.cpp"
#include <cassert>
#include <filesystem>
#include <iostream>
int main(){
 char folder[]="/tmp/thru-browser-tools.XXXXXX";assert(mkdtemp(folder));auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();t->setPerformanceMode(0);std::snprintf(t->composerPanelOwner.data(),65,"test");t->composerLease.store(ThruInstance::composerClock()+1000000);int serial=0;
 auto send=[&](const std::string& text,bool expected=true){const int id=++serial;const auto packet="test|"+std::to_string(id)+"|"+text;setParam(t.get(),"composer_edit",packet.c_str());const bool ok=t->composerEditReceipts[id%64].load()==std::uint64_t(id*2+1);if(ok!=expected){std::cerr<<text<<" expected "<<expected<<" got "<<ok<<"\n";std::abort();}};
 auto* c=t->activeClipSlot();send("loop|0|0|2000|1");for(int at:{100,500,1000})send("point|0|0|0|0|2|255|0|"+std::to_string(at)+"|1|0");const auto one=c->clipPoints[0].editorId,two=c->clipPoints[1].editorId;
 auto batch=[&](int token,int operation,const std::string& body,int count=2,bool expected=true){send("points|0|0|0|"+std::to_string(token)+"|"+std::to_string(operation)+"|"+std::to_string(count));send("points|0|0|1|"+std::to_string(token)+"|"+body);send("points|0|0|2|"+std::to_string(token),expected);};
 batch(1,0,std::to_string(one)+"|200|1.25|"+std::to_string(two)+"|600|1.75");assert(c->clipPointCount==3&&c->clipPoints[0].timeMs==200&&c->clipPoints[1].value==1.75f);
 send("cliptool|0|0|3|0");assert(c->clipPoints[0].timeMs==100&&!t->undoAvailable&&t->composerHistoryUndone);send("cliptool|0|0|9|0");assert(c->clipPoints[0].timeMs==200&&t->undoAvailable&&!t->composerHistoryUndone);
 batch(2,1,std::to_string(one)+"|1200|1.25|"+std::to_string(two)+"|1600|1.75");assert(c->clipPointCount==5&&c->clipPoints[3].editorId!=one);send("cliptool|0|0|3|0");assert(c->clipPointCount==3);send("cliptool|0|0|9|0");assert(c->clipPointCount==5);
 batch(3,2,std::to_string(one)+"|200|1.25|"+std::to_string(two)+"|600|1.75");assert(c->clipPointCount==3);send("cliptool|0|0|3|0");assert(c->clipPointCount==5);
 // Malformed/stale batches are atomic and preserve a working Redo.
 batch(4,0,std::to_string(one)+"|2100|1.25|"+std::to_string(two)+"|700|1.75",2,false);assert(c->clipPointCount==5&&t->composerHistoryUndone);
 batch(5,0,std::to_string(one)+"|300|1.25|"+std::to_string(one)+"|700|1.75",2,false);assert(c->clipPointCount==5);
 send("points|0|0|0|6|0|1");send("points|0|0|1|6|"+std::to_string(one)+"|300|1.25");t->markPersistentStateDirty();send("points|0|0|2|6",false);send("cliptool|0|0|9|0",false);assert(c->clipPoints[0].timeMs==200);
 send("point|0|0|0|0|1|255|0|1500|0.5|0");const auto other=c->clipPoints[0].editorId;batch(7,0,std::to_string(one)+"|300|1.25|"+std::to_string(other)+"|700|.5",2,false);
 // A new edit or a new browser owner invalidates a previously available Redo.
 send("cliptool|0|0|3|0");assert(t->composerHistoryUndone);send("point|0|0|0|0|2|255|0|1800|2|0");send("cliptool|0|0|9|0",false);send("cliptool|0|0|3|0");t->composerPanelOwnerGeneration.fetch_add(1);send("cliptool|0|0|9|0",false);
 // Take ownership remains audible after transforms; moving outside the
 // selected take's interval is refused without consuming the previous Undo.
 t->clearClipSlot(0);send("loop|0|0|2000|1");c->takeCount=2;send("point|0|0|0|0|2|255|1|300|1|0");const auto touched=c->clipPoints[0].editorId;c->touchCount=1;c->touchSpans[0]={std::uint16_t(LockParameter::speed),1,200,400};ThruInstance::rebuildTouchLanes(*c);
 batch(8,0,std::to_string(touched)+"|900|2",1,false);assert(c->clipPoints[0].timeMs==300);batch(9,0,std::to_string(touched)+"|400|2",1);assert(c->clipPoints[0].timeMs==400);send("cliptool|0|0|3|0");assert(c->clipPoints[0].timeMs==300&&c->touchCount==1);send("cliptool|0|0|9|0");assert(c->clipPoints[0].timeMs==400&&c->touchCount==1);
 t->setPerformanceMode(5);send("cliptool|0|0|9|0",false);assert(t->performanceMode()==5);t->setPerformanceMode(0);
 std::cout<<"Browser point batches: atomic move/copy/delete, shared Undo/Redo, bad IDs/ranges/routes, stale revisions and owner guards: PASS\n";
 // A clip formerly used as Single must capture and recall the Layers layout
 // and physical right-pad gestures through the same Move recorder entry point.
 t->clearClipSlot(0);t->composerRecordConfigured=false;t->setPerformanceMode(0);t->captureActiveClip(true);t->setPerformanceMode(5);t->clipRecordMode=ClipRecordMode::replace;
 const auto weight=LayerPatch::config+LayerPatch::weight0;t->padNoteOn(68+28,100);t->layersPadPressure(28,127);t->padNoteOff(68+28);t->toggleClipRecording();assert(t->clipRecording);
 std::array<int16_t,256> out{};auto render=[&](int blocks){for(int i=0;i<blocks;++i)t->render(out.data(),128);};render(60);const auto atB=t->clipCaptureTimeMs();t->padNoteOn(68+31,100);t->layersPadPressure(31,127);render(60);t->padNoteOff(68+31);t->toggleClipRecording();assert(!t->clipRecording&&t->performanceMode()==5&&c->layers.active);assert(c->clipPointCounts[kClipLayersOffset+weight+1]>=2);
 t->setClipPlaying(false);t->setClipPlaying(true);t->applyClipCurves(*c,atB+10);assert(t->layersRecorded.values[weight+1]==1);t->applyClipCurves(*c,atB*.5);assert(t->layersRecorded.values[weight]==1);
 t->setClipPlaying(false);t->captureActiveClip(true);t->clipSlots[1]=*c;t->activateStoredClip(1,false);t->setClipPlaying(true);t->applyClipCurves(*t->activeClipSlot(),atB+10);assert(t->layersRecorded.values[weight+1]==1&&t->performanceMode()==5);t->setClipPlaying(false);assert(t->writeMetadataFiles());
 auto disk=std::make_unique<ThruInstance>();assert(disk->prepare(folder));disk->stopBackgroundWork();disk->activateStoredClip(1,false);disk->setClipPlaying(true);disk->applyClipCurves(*disk->activeClipSlot(),atB+10);assert(disk->performanceMode()==5&&disk->layersRecorded.values[weight+1]==1);
 std::cout<<"Former Single clip: Move-recorded Layers FX, Stop/Play, clip recall and disk restore: PASS\n";
 t.reset();disk.reset();std::filesystem::remove_all(folder);
}
