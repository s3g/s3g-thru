// Sequence marker addresses: the real Move/MIDI/audio and persistence paths.
#define S3G_THRU_STORAGE_TESTING 1
#include "../src/dsp/thru_module.cpp"
#include <cassert>
#include <filesystem>
#include <iostream>
static s3g_thru::SliceMap marked() {
  s3g_thru::SliceMap m;m.count=4;m.points={0,.1f,.37f,.83f,1};
  std::snprintf(m.labels[2].data(),m.labels[2].size(),"Snare \\\"tight\\\"");
  return m;
}
static void source(ThruInstance& t,int part) {
  constexpr int frames=44100;
  for(int c=0;c<2;++c)for(int i=0;i<frames;++i)t.engine.writableLane(part,c)[i]=.25f*std::sin((.025f+part*.01f)*i)*(c?-.75f:1.f);
  assert(t.engine.adoptLane(part,frames,44100));t.fileSliceMaps[part]=marked();
  t.selectSplitPart(part,false);t.controls.algorithm=Engine::PlaybackAlgorithm::slices;
  t.controls.sampleRegionMode=Engine::RegionMode::file;t.controls.sampleRegionCount=16;
  t.controls.locus=.5f;t.controls.field=1;t.controls.ampAttackMs=2;t.controls.ampReleaseMs=5;
  t.splitClipControls[part]=t.controls;
}
int main(int argc,char** argv) {
  char folder[]="/tmp/thru-sequence-slices-test.XXXXXX";assert(mkdtemp(folder));
  auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();t->setPerformanceMode(6);
  for(int p=0;p<4;++p)source(*t,p);
  t->selectSplitPart(0,false);t->resetSequenceClock();
  auto& lane=t->sequenceLive.tracks[0];lane.timeMode=true;lane.timeMs=900;lane.trigs[0].enabled=true;
  setParam(t.get(),"seq_hold","0");assert(t->sequenceSoundEditing());
  // The old POS-only interpretation remains exact until a SLIC lock is written.
  const auto legacy=t->sequenceSliceSelection(0,0,0,t->sequenceLive);
  assert(legacy.number==1&&legacy.count==4&&!legacy.explicitAddress);
  const auto* parameter=s3g_thru_midi::find(s3g_thru_sequence::sliceLockId);assert(parameter);
  assert(midiMaximum(*t,*parameter,0)==4);
  setParam(t.get(),"seq_slice","3");
  assert(t->sequenceLive.soundLocks.find(0,0,0,3913)->value==3);
  auto info=t->sequenceSliceSelection(0,0,0,t->sequenceLive);assert(info.valid&&info.number==3&&info.low==.37f&&info.high==.83f&&info.own);
  auto* voice=t->sliceAuditionVoice();(void)voice;
  bool seen=false;for(auto& v:t->voices)if(v.active&&v.sequence.audition&&v.held){seen=true;assert(v.sequence.sliceIndex==2&&v.sequence.startPhase==0);assert(v.sliceLow==.37f&&v.sliceHigh==.83f&&v.sliceAbsolute);}
  assert(seen);
  std::array<int16_t,256> block{};double energy=0;
  for(int n=0;n<8;++n){t->render(block.data(),128);for(auto x:block)energy+=std::abs(int(x));}
  assert(energy>1000);
  for(const auto& v:t->voices)if(v.active&&v.sequence.audition&&v.held)assert(v.engine.normalizedPosition()>=.37f&&v.engine.normalizedPosition()<.83f);
  // Pitch never selects a different slice; each Burst child can override it.
  setParam(t.get(),"seq_note","12");assert(t->sequenceSliceSelection(0,0,0,t->sequenceLive).number==3);
  setParam(t.get(),"seq_burst","2");setParam(t.get(),"seq_child","2");
  info=t->sequenceSliceSelection(0,0,2,t->sequenceLive);assert(info.number==3&&info.inherited&&!info.own);
  setParam(t.get(),"seq_slice","4");assert(t->sequenceSliceSelection(0,0,2,t->sequenceLive).number==4);
  setParam(t.get(),"seq_sound_clear","3913");assert(t->sequenceSliceSelection(0,0,2,t->sequenceLive).number==3);
  setParam(t.get(),"seq_slice","4");setParam(t.get(),"seq_copy","1");setParam(t.get(),"seq_hold","1");setParam(t.get(),"seq_paste","1");
  assert(t->sequenceLive.soundLocks.find(0,1,0,3913)->value==3&&t->sequenceLive.soundLocks.find(0,1,2,3913)->value==4);
  // Invalid edits leave the record untouched, and recalled missing IDs are silent.
  setParam(t.get(),"seq_slice","5");assert(t->sequenceLive.soundLocks.find(0,1,0,3913)->value==3);
  setParam(t.get(),"seq_hold","-1");t->sequenceLive.soundLocks.set(0,1,0,3913,12);t->sequencePlayback=t->sequenceLive;
  std::array<unsigned,8> ages{};for(std::size_t i=0;i<ages.size();++i)ages[i]=t->voices[i].age;
  t->startSequenceVoice(0,1,0,.25,false,10);
  for(std::size_t i=0;i<ages.size();++i)assert(ages[i]==t->voices[i].age);
  info=t->sequenceSliceSelection(0,1,0,t->sequenceLive);assert(!info.valid&&info.number==12&&info.count==4);
  // All four lanes have independent addressing and two-voice ownership.
  for(int p=0;p<4;++p){t->selectSplitPart(p,false);setParam(t.get(),"seq_hold","2");applyNrpn(*t,5+p,3913,16383,MOVE_MIDI_SOURCE_EXTERNAL);assert(t->sequenceLive.soundLocks.find(p,2,0,3913)->value==4);setParam(t.get(),"seq_hold","-1");}
  t->selectSplitPart(0,false);t->sequencePlayback=t->sequenceLive;
  for(int p=0;p<4;++p)for(int repeat=0;repeat<3;++repeat)t->startSequenceVoice(p,2,0,.25,false,20+repeat);
  for(int n=0;n<20;++n){t->render(block.data(),128);}
  for(int p=0;p<4;++p){int active=0;for(const auto& v:t->voices)if(v.active&&v.sequence.active&&v.part==p)++active;assert(active<=2);}
  // Browser receipts reject a stale map count, retain Move focus and support clear.
  std::snprintf(t->composerPanelOwner.data(),65,"slice-test");t->composerLease.store(ThruInstance::composerClock()+1000000);
  setParam(t.get(),"composer_edit","slice-test|1|s|0|0|0|3913|3|0|2");assert(t->composerEditReceipts[1].load()==3);
  setParam(t.get(),"composer_edit","slice-test|2|s|0|0|0|3913|5|0|2");assert(t->composerEditReceipts[2].load()==4);
  assert(t->sequenceLive.soundLocks.find(0,0,2,3913)->value==3);
  setParam(t.get(),"composer_edit","slice-test|3|s|0|0|0|3913|3|1|2");assert(t->composerEditReceipts[3].load()==7&&!t->sequenceLive.soundLocks.find(0,0,2,3913));
  // Source/count locks belong to the same trig and Burst scope as SLIC.
  auto edited=marked();edited.count=2;edited.points={0,.6f,1};t->activeClipSlot()->sliceLanes[0].edited=edited;
  t->sequenceLive.soundLocks.set(0,0,0,1031,3);
  auto scoped=t->sequenceSliceSelection(0,0,0,t->sequenceLive);assert(scoped.count==2&&!scoped.valid);
  t->sequenceLive.soundLocks.set(0,0,1,1031,2);
  scoped=t->sequenceSliceSelection(0,0,1,t->sequenceLive);assert(scoped.count==4&&scoped.number==3&&scoped.valid&&scoped.low==.37f);
  t->sequenceLive.soundLocks.erase(0,0,0,1031);t->sequenceLive.soundLocks.erase(0,0,1,1031);
  // Existing typed lock storage/portable JSON needs no new core offsets.
  auto j=t->composerLocks(lane.trigs[0].locks,true,&t->sequenceLive.soundLocks,0,0,0);
  s3g_thru_pack::locks(j,"locks");assert(j.at("sound").at("seq_slice").number()==3);
  s3g_thru_sequence::Values restored; s3g_thru_sequence::SoundLocks pool;t->composerReadLocks(restored,j,true,&pool,0,0,0);assert(pool.find(0,0,0,3913)->value==3);
  setParam(t.get(),"seq_hold","0");std::array<char,8192> state{};assert(t->sequenceState(state.data(),state.size())>0);
  const auto snapshot=s3g_thru_json::parse(state.data());assert(snapshot.at("slice").at("number").number()==3&&snapshot.at("slice").at("count").number()==4);
  if(argc>1){std::FILE* f=std::fopen(argv[1],"wb");assert(f);std::fputs(state.data(),f);std::fclose(f);}
  setParam(t.get(),"seq_hold","-1");assert(t->savePersistentState());
  auto reloaded=std::make_unique<ThruInstance>();assert(reloaded->prepare(folder));reloaded->stopBackgroundWork();
  assert(reloaded->activeClipSlot()->sequence.soundLocks.find(0,0,0,3913)->value==3);
  assert(reloaded->activeClipSlot()->sequence.soundLocks.find(0,1,2,3913)->value==4);
  // A missing recalled address never wraps; clearing restores untouched POS.
  reloaded->setPerformanceMode(6);source(*reloaded,0);reloaded->sequenceStep=1;reloaded->sequenceChild=0;
  assert(!reloaded->sequenceSliceSelection(0,1,0,reloaded->sequenceLive).valid);
  setParam(reloaded.get(),"seq_slice","0");assert(!reloaded->sequenceLive.soundLocks.find(0,1,0,3913));
  assert(reloaded->sequenceLive.tracks[0].trigs[1].locks.values[0]==lane.trigs[0].locks.values[0]);
  reloaded->fileSliceMaps[0]={};reloaded->sequenceLive.soundLocks.set(0,0,0,3913,3);reloaded->sequencePlayback=reloaded->sequenceLive;
  const auto unavailable=reloaded->sequenceSliceSelection(0,0,0,reloaded->sequenceLive);assert(!unavailable.valid&&unavailable.count==0);
  setParam(reloaded.get(),"seq_hold","0");setParam(reloaded.get(),"seq_slice","1");assert(reloaded->sequenceLive.soundLocks.find(0,0,0,3913)->value==3);
  setParam(reloaded.get(),"seq_sound_clear","3913");assert(!reloaded->sequenceLive.soundLocks.find(0,0,0,3913));
  t.reset();reloaded.reset();std::filesystem::remove_all(folder);
  std::cout<<"Sequence explicit slices: irregular WAV maps, exact stereo starts, pitch, Burst inheritance, four lanes, MIDI, stealing, missing IDs, copy, JSON and restart: PASS\n";
}
