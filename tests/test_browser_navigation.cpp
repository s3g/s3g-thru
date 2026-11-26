#define S3G_THRU_STORAGE_TESTING 1
#include "../src/dsp/thru_module.cpp"
#include <cassert>
#include <filesystem>
#include <iostream>
int main(){
 char folder[]="/tmp/thru-browser-navigation.XXXXXX";assert(mkdtemp(folder));
 auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();
 std::snprintf(t->composerPanelOwner.data(),65,"navigation");t->composerLease.store(ThruInstance::composerClock()+1000000);
 int serial=0;std::string lastPacket;
 auto send=[&](const std::string& body,bool expected=true){const int id=++serial;lastPacket="navigation|"+std::to_string(id)+"|"+body;setParam(t.get(),"composer_edit",lastPacket.c_str());assert(t->composerEditReceipts[id%64].load()==std::uint64_t(id*2+int(expected)));};
 auto choose=[&](int wanted){const int previous=t->performanceMode();send("mode|0|0|"+std::to_string(wanted)+"|"+std::to_string(previous));assert(t->performanceMode()==wanted);};
 auto& lane=t->sequenceLive.tracks[0];lane.length=9;lane.trigs[4].enabled=true;lane.trigs[4].locks.mask=3;lane.trigs[4].locks.values[0]=.7f;
 assert(t->sequenceLive.soundLocks.set(0,4,0,3913,3));
 auto* clip=t->activeClipSlot();clip->occupied=clip->clipRecorded=true;clip->clipDurationMs=2000;clip->recordedBeats=4;clip->recordedTempoBpm=120;clip->takeCount=1;clip->clipNoteCount=1;clip->clipNotes[0].note=60;clip->clipNotes[0].durationMs=400;
 assert(t->engine.adoptLane(0,2048,44100));
 for(int mode:{1,2,3,4,5,6,0,6,2,5,0,3}){
  t->clipPlaying=true;choose(mode);assert(!t->clipPlaying);
  assert(t->sequenceLive.tracks[0].length==9&&t->sequenceLive.tracks[0].trigs[4].locks.values[0]==.7f);
  assert(t->sequenceLive.soundLocks.find(0,4,0,3913)->value==3);
  assert(t->activeClipSlot()->clipNoteCount==1&&t->engine.laneFrames(0)==2048);
 }
 t->selectSplitPart(3,false);choose(0);assert(t->splitFocus==0&&t->activeClipSlot()->clipNoteCount==1);
 // A no-op is acknowledged; a delayed repeat cannot reapply an older choice.
 t->clipPlaying=true;choose(0);assert(t->clipPlaying);t->clipPlaying=false;
 choose(1);const auto repeated=lastPacket;choose(2);setParam(t.get(),"composer_edit",repeated.c_str());assert(t->performanceMode()==2);
 send("mode|0|0|5|0",false);assert(t->performanceMode()==2);
 send("mode|0|0|7|2",false);send("mode|0|0|5|-1",false);send("mode|1|0|5|2",false);assert(t->performanceMode()==2);
 // Browser mode changes obey the same recording/publication protection as Move.
 t->clipRecording=true;send("mode|0|0|5|2",false);assert(t->performanceMode()==2&&t->clipRecording);t->clipRecording=false;
 t->partCapturePublishing=true;send("mode|0|0|5|2",false);assert(t->performanceMode()==2);t->partCapturePublishing=false;
 t->liveCaptureJob.store(1);send("mode|0|0|5|2",false);assert(t->performanceMode()==2);t->liveCaptureJob.store(0);
 // Grid on/off changes keep the selected trig's musical values and sound locks.
 choose(6);send("q|0|0|4|0|0|0|0");assert(!t->sequenceLive.tracks[0].trigs[4].enabled);
 assert(t->sequenceLive.tracks[0].trigs[4].locks.mask==3&&t->sequenceLive.tracks[0].trigs[4].locks.values[0]==.7f&&t->sequenceLive.soundLocks.find(0,4,0,3913)->value==3);
 send("q|0|0|4|0|0|1|0");assert(t->sequenceLive.tracks[0].trigs[4].enabled);
 assert(t->savePersistentState());
 auto reopened=std::make_unique<ThruInstance>();assert(reopened->prepare(folder));reopened->stopBackgroundWork();
 assert(reopened->performanceMode()==6&&reopened->sequenceLive.tracks[0].length==9);
 assert(reopened->sequenceLive.tracks[0].trigs[4].enabled&&reopened->sequenceLive.soundLocks.find(0,4,0,3913)->value==3);
 t.reset();reopened.reset();std::filesystem::remove_all(folder);
 std::cout<<"Browser navigation: seven native modes, focus/transport/clip retention, stale and busy rejection, idempotent replies, non-destructive trig toggles and restart: PASS\n";
}
