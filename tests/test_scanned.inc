// Real module path, including source-free allocation and serialized projects.
std::unique_ptr<ThruInstance> scannedFixture(int mode=0){
 auto t=tableFixture(mode);for(int part=0;part<(t->splitEnabled?t->partCount():1);++part){if(t->splitEnabled)t->selectSplitPart(part,false);setParam(t.get(),"algorithm","16");}
 if(t->splitEnabled)t->selectSplitPart(0,false);return t;
}
void testScannedCore(){
 namespace sc=s3g_thru_scanned;sc::Instrument v;v.prepare();sc::Program p;
 for(auto& x:p.patches)x={};
 for(int excitation=0;excitation<4;++excitation){p.patches[0].v[sc::excitation]=excitation;v.reset();v.beginBlock(p.patches);double energy=0;
  for(int n=0;n<16000;++n){auto x=v.tick(p.patches,0,220,44100);for(float a:x){assert(std::isfinite(a)&&std::abs(a)<4);energy+=a*a;}}
  assert(energy>.1);
 }
 // Freeze locks mechanics, but retains a periodic, correctly pitched reader.
 p.patches[0]={};p.patches[0].v[sc::freeze]=1;p.patches[0].v[sc::width]=0;
 for(float hz:{110.f,220.f,440.f,1760.f}){v.reset();v.beginBlock(p.patches);int crossings=0;float last=0;
  for(int n=0;n<16000;++n){auto x=v.tick(p.patches,0,hz,44100);if(n>3000&&last<=0&&x[0]>0)++crossings;assert(std::abs(x[0]-x[1])<1e-6);last=x[0];}
  assert(std::abs(crossings-hz*12999/44100)<2);const auto held=v.models[0].y;
  for(int n=0;n<1000;++n)v.tick(p.patches,0,hz,44100);assert(held==v.models[0].y);
 }
 // Sample initialization uses recorded material and has a non-silent fallback.
 std::array<float,64> sample{};for(int i=0;i<64;++i)sample[i]=std::sin(sc::pi*6*i/64);
 sc::Model a,b;sc::Config c;c.v[sc::excitation]=3;a.reset(c,&sample);b.reset(c,nullptr);assert(a.y!=b.y);
 // Live extremes, pressure, seeded profiles, freeze/resume, patch transitions,
 // rate changes and high keyboard notes must remain finite and bounded.
 for(float sr:{44100.f,48000.f,96000.f}){v.reset();for(auto& x:p.patches)x={};
  for(int n=0;n<20000;++n){if(n%128==0){int field=(n/128)%sc::fields;for(auto& x:p.patches)x.v[field]=(n/256)%2?sc::high[field]:sc::low[field];v.beginBlock(p.patches);}
   auto x=v.tick(p.patches,float(n%12000)/4000,110+float(n)/2,sr,5,nullptr,float(n%128)/127);
   for(float f:x)assert(std::isfinite(f)&&std::abs(f)<8);
  }
 }
 // Projection drops out-of-band partials before 2x readout; a frozen
 // high-frequency noise cycle must not retain above-cutoff FFT bins.
 c={};c.v[sc::excitation]=2;c.v[sc::brightness]=100;a.reset(c);a.project(4000,44100);
 std::array<sc::Complex,128> bins{};for(int i=0;i<128;++i)bins[i].r=a.newCycle[i];sc::Prepared::get().fft(bins,128,false);
 for(int h=4;h<64;++h)assert(std::hypot(bins[h].r,bins[h].i)<1e-4);
 // Snapshot interpolation remains continuous during both control and model motion.
 for(auto& x:p.patches){x={};x.v[sc::width]=0;}v.reset();float last=0,jump=0;
 for(int n=0;n<20000;++n){if(n%128==0){p.patches[0].v[sc::brightness]=n>8000?10:90;p.patches[0].v[sc::mass]=n>10000?90:10;p.patches[0].v[sc::freeze]=n>12000&&n<14000;v.beginBlock(p.patches);}
  auto x=v.tick(p.patches,0,110,44100);if(n>1000)jump=std::max(jump,std::abs(x[0]-last));last=x[0];}
 assert(jump<.15);
 std::puts("SCANNED core: pitch/mechanics separation, ring/strike/noise/sample excitation, freeze, pressure, finite limits, alias-limited projection and continuous edits: PASS");
}
void testScannedModule(){
 namespace sc=s3g_thru_scanned;
 for(int mode=0;mode<7;++mode){auto t=scannedFixture(mode);const int parts=mode==0?1:mode==3||mode==6?4:mode==4||mode==5?1:2;
  for(int p=0;p<parts;++p){t->midiPart=p;t->noteOn(60+p,100);}
  assert(tableRender(*t,40)>100);for(auto& v:t->voices)if(v.active){assert(v.engine.playbackAlgorithm()==Engine::PlaybackAlgorithm::scanned);assert(v.engine.normalizedPosition()==-1);}
 }
 auto t=scannedFixture();for(int i=0;i<8;++i)t->noteOn(48+i,100);assert(tableRender(*t,30)>100);t->noteOn(72,100);tableRender(*t,8);int held=0;for(auto& v:t->voices)held+=v.held;assert(held==8);
 t->allNotesOff();tableRender(*t,120);for(auto& v:t->voices)assert(!v.active);
 t=scannedFixture();t->controls.lanePath=Engine::LanePath::up;t->controls.laneCycles=1;t->noteOn(60,100);tableRender(*t,50);
 const auto start=t->voices[0].engine.normalizedLanePosition();tableRender(*t,30);assert(t->voices[0].engine.normalizedLanePosition()!=start);
 setParam(t.get(),"stack_scan_paused","1");tableRender(*t,5);const auto lane=t->voices[0].engine.normalizedLanePosition();tableRender(*t,20);assert(std::abs(lane-t->voices[0].engine.normalizedLanePosition())<.001);
 setParam(t.get(),"record_lane","2");setParam(t.get(),"scanned_freeze","1");setParam(t.get(),"scanned_force","35");assert(t->controls.scanned.patches[1].v[sc::freeze]==1&&t->controls.scanned.patches[0].v[sc::freeze]==0);
 // Physical profiles and parameters are real native fixtures for UI/docs.
 char state[4096]{},scope[4096]{};assert(t->formatScannedState(state,sizeof(state))>0&&t->formatTableScopes(scope,sizeof(scope))>0);
 if(const auto* path=std::getenv("S3G_THRU_SCANNED_FIXTURE")){std::ofstream out(path);out<<"{\"state\":"<<state<<",\"scope\":"<<scope<<"}";}
 auto pair=scannedFixture(2);setParam(pair.get(),"record_lane","2");setParam(pair.get(),"scanned_tune","-12");pair->selectSplitPart(1,false);setParam(pair.get(),"record_lane","4");setParam(pair.get(),"scanned_tune","7");pair->selectSplitPart(0,false);assert(pair->controls.scanned.patches[1].v[sc::tune]==-12);pair->selectSplitPart(1,false);assert(pair->controls.scanned.patches[3].v[sc::tune]==7);
 nrpn(*pair,5,"scanned_tune",s3g_thru_midi::encode(midiParameter("scanned_tune"),5,48));assert(pair->splitFocus==1&&std::abs(pair->liveControlsForPart(0).scanned.patches[3].v[sc::tune]-5)<.02);
 pair->copySplitPart(0);assert(pair->liveControlsForPart(1).scanned.patches[1].v[sc::tune]==pair->liveControlsForPart(0).scanned.patches[1].v[sc::tune]);
 // Profile-dependent destinations and surface cells reflect actual use.
 pair->selectSplitPart(0,false);for(auto& patch:pair->controls.scanned.patches)patch.v[sc::profile]=0;
 s3g_thru_lfo::Config lfo;lfo.target=0;lfo.engine=16;
 const int massDest=pair->controlLfoDestinationForKey("scanned_mass");assert(!pair->controlLfoDestinationAvailable(lfo,massDest));
 const auto* massParam=e16PageSlotParameter(*pair,3,0,lfo.target);assert(!massParam);
 pair->controls.scanned.patches[0].v[sc::profile]=3;assert(pair->controlLfoDestinationAvailable(lfo,massDest));
 // Full note path with a mixed sample/synth Engine List (no sample required
 // for the SCANNED entry).
 t=scannedFixture();setParam(t.get(),"voice_engine_mode","1");setParam(t.get(),"voice_engine_order","0");
 for(int i=0;i<8;++i)setListSlot(*t,i,16);t->noteOn(60,100);assert(tableRender(*t,20)>100);
 // A mixed list also captures its initial SCANNED patch values when
 // recording begins with a sample engine selected in the editor.
 t=scannedFixture();setParam(t.get(),"voice_engine_mode","1");for(int i=0;i<8;++i)setListSlot(*t,i,i?16:0);setParam(t.get(),"voice_engine_edit","1");
 assert(t->controls.algorithm==Engine::PlaybackAlgorithm::motion);t->beginClipRecording();
 assert(t->activeClipSlot()->clipPointCounts[kClipScannedOffset+sc::motion]>0||t->clipLastPointIndices[kClipScannedOffset+sc::motion]>=0);
 // Source-free Sequence, independent overlapping trig locks and inheritance.
 t=scannedFixture(6);auto& track=t->sequenceLive.tracks[0];track.length=4;track.rate=4;track.trigs[0].enabled=true;track.timeMs=200;track.timeMode=true;
 setParam(t.get(),"seq_hold","0");setParam(t.get(),"scanned_tune","-12");setParam(t.get(),"scanned_motion","2.5");assert(t->controls.scanned.patches[0].v[sc::tune]==0);assert(t->sequenceLive.soundLocks.resolve(0,0,0).find(4303)->value==-12);setParam(t.get(),"seq_hold","-1");
 t->captureActiveClip(true);t->commitSequenceEdit();setParam(t.get(),"clip_playing","1");assert(tableRender(*t,100)>100);
 std::puts("SCANNED module: all seven layouts, eight notes/steals/releases, scan hold, per-patch editing, A/B copies, MIDI focus isolation, Engine List and source-free Sequence sound locks: PASS");
}
void testScannedPersistence(){
 namespace sc=s3g_thru_scanned;auto t=scannedFixture(2);setParam(t.get(),"record_lane","2");setParam(t.get(),"scanned_motion","2.5");t->beginClipRecording();t->midiPart=0;t->noteOn(60,100);tableRender(*t,20);setParam(t.get(),"scanned_motion","3.5");t->selectSplitPart(1,false);setParam(t.get(),"record_lane","4");setParam(t.get(),"scanned_mass","80");setParam(t.get(),"scanned_amp_mode","2");setParam(t.get(),"scanned_amp_gain","63");tableRender(*t,50);t->noteOff(60);t->finishClipRecording();t->captureActiveClip(true);
 bool curve=false;for(int i=0;i<t->activeClipSlot()->clipPointCount;++i)curve|=t->activeClipSlot()->clipPoints[i].type==ClipPointType::scannedParameter;assert(curve);
 auto text=std::make_unique<std::array<char,kStateTextSize>>();assert(t->serializeState(text->data(),text->size())>0);auto loaded=scannedFixture();assert(loaded->restoreState(text->data()));assert(loaded->liveControlsForPart(1).scanned.patches[3].v[sc::mass]==80);assert(loaded->liveControlsForPart(1).scanned.patches[3].v[sc::amp_gain]==63);
 char folder[]="/tmp/thru-scanned-XXXXXX";assert(mkdtemp(folder));std::filesystem::create_directory(std::string(folder)+"/restored");std::snprintf(t->moduleDirectory.data(),t->moduleDirectory.size(),"%s",folder);t->persistenceEnabled=true;t->persistenceGeneration=1;
 assert(t->savePersistentState()&&t->exportProjectFile());assert(loaded->readClipFileFromPath((std::string(folder)+"/thru-clips.bin").c_str()));assert(loaded->clipSlots[t->activeClip].scannedPrograms[2].patches[3].v[sc::mass]==80);
 auto restored=std::make_unique<ThruInstance>();const auto target=std::string(folder)+"/restored";assert(restored->prepare(target.c_str()));restored->stopBackgroundWork();assert(restored->savePersistentState());restored->projectGeneration=restored->reservePersistenceGeneration();std::snprintf(restored->projectArchivePath.data(),restored->projectArchivePath.size(),"%s",t->projectOutputPath.data());assert(restored->importProjectFile()&&restored->savePersistentState());restored.reset();restored=std::make_unique<ThruInstance>();assert(restored->prepare(target.c_str()));restored->stopBackgroundWork();assert(restored->liveControlsForPart(1).scanned.patches[3].v[sc::mass]==80);assert(restored->liveControlsForPart(1).scanned.patches[3].v[sc::amp_mode]==2&&restored->liveControlsForPart(1).scanned.patches[3].v[sc::amp_gain]==63);
 restored.reset();loaded.reset();t.reset();std::filesystem::remove_all(folder);
 std::puts("SCANNED storage: independent programs, recorded gestures, JSON/format52, real portable export/import and restart: PASS");
}
#include "test_scanned_percussion.inc"
#include "test_scanned_amp.inc"
void testScanned(){testScannedCore();testScannedModule();testScannedPersistence();testScannedPercussion();testScannedAmp();}

#include "test_fm.inc"
