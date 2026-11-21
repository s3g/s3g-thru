#include "scanned_tone_reference.h"
void testScannedToneReference(){
 namespace sc=s3g_thru_scanned;namespace prior=s3g_thru_scanned_v27;
 sc::Instrument voice;prior::Instrument reference;voice.prepare();reference.prepare();sc::Program now;prior::Program before;
 float error=0;
 for(float sr:{44100.f,48000.f,96000.f}){voice.reset();reference.reset();
  for(int n=0;n<12000;++n){if(n%128==0){for(int lane=0;lane<4;++lane){now.patches[lane].v[sc::motion]=n>5000?3.5f:1.2f;now.patches[lane].v[sc::freeze]=n>7000&&n<9000;for(int f=0;f<16;++f)before.patches[lane].v[f]=now.patches[lane].v[f];}voice.beginBlock(now.patches);reference.beginBlock(before.patches);}
   const float position=1.5f+1.4f*std::sin(n*.0002f),hz=n<4000?220:330;
   const auto a=voice.tick(now.patches,position,hz,sr,5,nullptr,.5),b=reference.tick(before.patches,position,hz,sr,5,nullptr,.5);
   for(int c=0;c<2;++c)error=std::max(error,std::abs(a[c]-b[c]));
  }
 }
 assert(error<2e-6f);std::printf("SCANNED TONE compatibility: independent test.27 reference across rates, scan, pitch, force and freeze; max error %.9f: PASS\n",error);
}
void testScannedPercussionCore(){
 namespace sc=s3g_thru_scanned;sc::Instrument voice;voice.prepare();sc::Program p;
 for(int exc=0;exc<4;++exc){for(auto& c:p.patches){c={};c.v[sc::art]=1;c.v[sc::excitation]=exc;c.v[sc::motion]=.05f;c.v[sc::decay]=200;c.v[sc::force]=100;c.v[sc::pressure]=100;}
  voice.reset();double attack=0,tail=0;float peak=0;
  for(int n=0;n<30000;++n){if(n%128==0)voice.beginBlock(p.patches);auto audio=voice.tick(p.patches,0,220,44100,5,nullptr,1);
   for(float x:audio){assert(std::isfinite(x)&&std::abs(x)<4);peak=std::max(peak,std::abs(x));if(n<441)attack+=x*x;if(n>22050)tail+=x*x;}}
  assert(attack>.001&&tail<1e-6&&peak>.05&&voice.models[0].impactEnvelope==0);
 }
 // Entering a patch later follows the note age; it cannot launch another impact.
 for(auto& c:p.patches){c={};c.v[sc::art]=1;c.v[sc::decay]=100;}
 voice.reset();voice.beginBlock(p.patches);for(int n=0;n<22050;++n)voice.tick(p.patches,0,220,44100);
 double late=0;for(int n=0;n<4000;++n){auto a=voice.tick(p.patches,3,220,44100);late+=a[0]*a[0];}assert(late<1e-9);
 // Articulation changes are staged for the next note. A held TONE is intact.
 for(auto& c:p.patches)c={};voice.reset();voice.beginBlock(p.patches);voice.tick(p.patches,0,220,44100);
 for(auto& c:p.patches)c.v[sc::art]=1;voice.beginBlock(p.patches);for(int n=0;n<1000;++n)voice.tick(p.patches,0,220,44100);assert(!voice.models[0].percussive);
 voice.reset();voice.beginBlock(p.patches);voice.tick(p.patches,0,220,44100);assert(voice.models[0].percussive);
 // Loss removes upper harmonics while preserving the fundamental.
 sc::Config bright;bright.v[sc::art]=1;bright.v[sc::brightness]=100;bright.v[sc::hardness]=90;
 sc::Model dry,dark;dry.reset(bright);dark.reset(bright);dark.lossIntegral=1;dry.project(220,44100);dark.project(220,44100);
 std::array<sc::Complex,128> a{},b{};for(int i=0;i<128;++i){a[i].r=dry.newCycle[i];b[i].r=dark.newCycle[i];}sc::Prepared::get().fft(a,128,false);sc::Prepared::get().fft(b,128,false);
 assert(std::abs(std::hypot(a[1].r,a[1].i)-std::hypot(b[1].r,b[1].i))<1e-4);assert(std::hypot(b[8].r,b[8].i)<std::hypot(a[8].r,a[8].i)*.001f);
 // Audible sweep and held-tail edits never restart an envelope or step gain.
 for(auto& c:p.patches){c={};c.v[sc::art]=1;c.v[sc::decay]=4000;c.v[sc::impact]=0;c.v[sc::width]=0;}
 voice.reset();float last=0,jump=0,previousEnvelope=1;
 for(int n=0;n<18000;++n){if(n%128==0){auto& c=p.patches[0];c.v[sc::decay]=n>7000?1000:4000;c.v[sc::loss]=n>9000?90:10;c.v[sc::bend]=n>11000?7:0;c.v[sc::sweep_time]=n>11000?100:2000;voice.beginBlock(p.patches);}
  const auto out=voice.tick(p.patches,0,110,44100);if(n>500)jump=std::max(jump,std::abs(out[0]-last));last=out[0];assert(voice.models[0].bodyEnvelope<=previousEnvelope);previousEnvelope=voice.models[0].bodyEnvelope;
 }
 assert(jump<.15);
 for(float sr:{44100.f,48000.f,96000.f}){voice.reset();for(auto& c:p.patches){c={};c.v[sc::art]=1;}
  for(int n=0;n<8000;++n){if(n%128==0){const int f=17+(n/128)%6;for(auto& c:p.patches)c.v[f]=(n/256)%2?sc::high[f]:sc::low[f];voice.beginBlock(p.patches);}const auto out=voice.tick(p.patches,n/2666.f,55+n*.2f,sr,5,nullptr,1);for(float x:out)assert(std::isfinite(x)&&std::abs(x)<8);}
 }
 std::puts("SCANNED percussion core: immediate slow-motion strikes, shaped bounded impacts, held decay, late-patch age, next-note articulation, spectral loss, live sweep/tail continuity and rate/control extremes: PASS");
}
void testScannedPercussionModule(){
 namespace sc=s3g_thru_scanned;
 for(int mode=0;mode<7;++mode){auto t=scannedFixture(mode);for(int part=0;part<(t->splitEnabled?t->partCount():1);++part){if(t->splitEnabled)t->selectSplitPart(part,false);for(auto& p:t->controls.scanned.patches)p.v[sc::art]=1;t->midiPart=part;t->noteOn(60+part,100);}
  assert(tableRender(*t,8)>100);tableRender(*t,250);for(const auto& v:t->voices)if(v.active)assert(v.held);}
 auto t=scannedFixture();for(auto& p:t->controls.scanned.patches){p.v[sc::art]=1;p.v[sc::decay]=4000;}for(int n=0;n<8;++n)t->noteOn(48+n,100);assert(tableRender(*t,20)>100);t->noteOn(72,100);tableRender(*t,10);int held=0;for(const auto& v:t->voices)held+=v.held;assert(held==8);t->allNotesOff();tableRender(*t,120);for(const auto& v:t->voices)assert(!v.active);
 t=scannedFixture(2);setParam(t.get(),"record_lane","2");setParam(t.get(),"scanned_art","1");t->selectSplitPart(1,false);nrpn(*t,5,"scanned_decay",s3g_thru_midi::encode(midiParameter("scanned_decay"),800,4000));assert(t->splitFocus==1&&std::abs(t->liveControlsForPart(0).scanned.patches[1].v[sc::decay]-800)<1);
 t=scannedFixture(6);setParam(t.get(),"seq_hold","0");setParam(t.get(),"scanned_art","1");setParam(t.get(),"scanned_bend","24");setParam(t.get(),"scanned_decay","300");assert(t->controls.scanned.patches[0].v[sc::art]==0);auto locks=t->sequenceLive.soundLocks.resolve(0,0,0);assert(locks.find(4316)->value==1&&locks.find(4320)->value==24);int part=0;const auto* remoteDecay=e16PageSlotParameter(*t,4,3,part);assert(remoteDecay&&!std::strcmp(remoteDecay->key,"scanned_decay"));assert(midiRead(*t,*remoteDecay,0)==300);
 setParam(t.get(),"seq_hold","-1");t->startSequenceVoice(0,0,0,.5,true,0);assert(tableRender(*t,5)>100);
 std::puts("SCANNED percussion module: seven layouts, held-key decay, eight voices/steal/release, NRPN part isolation and independent Sequence articulation/sweep locks: PASS");
}
void testScannedPercussionStorage(){
 namespace sc=s3g_thru_scanned;char folder[]="/tmp/thru-perc-state-XXXXXX";assert(mkdtemp(folder));
 auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();setParam(t.get(),"algorithm","16");setParam(t.get(),"scanned_art","1");setParam(t.get(),"scanned_decay","456");t->beginClipRecording();t->noteOn(60,100);tableRender(*t,10);setParam(t.get(),"scanned_loss","80");tableRender(*t,20);t->noteOff(60);t->finishClipRecording();t->captureActiveClip(true);assert(t->savePersistentState());
 auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(folder));restored->stopBackgroundWork();assert(restored->controls.scanned.patches[0].v[sc::art]==1&&restored->controls.scanned.patches[0].v[sc::decay]==456&&restored->controls.scanned.patches[0].v[sc::loss]==80);
 bool gesture=false;for(int i=0;i<restored->activeClipSlot()->clipPointCount;++i){const auto& p=restored->activeClipSlot()->clipPoints[i];gesture|=p.type==ClipPointType::scannedParameter&&(p.parameter&255)==sc::loss&&p.value==80;}assert(gesture);
 // A genuine format52 file has no PERC fields or PERC gestures. It must
 // retain its complete original program and initialize TONE, not PERC.
 auto legacy=scannedFixture();setParam(legacy.get(),"scanned_motion","2.5");legacy->captureActiveClip(true);std::snprintf(legacy->moduleDirectory.data(),legacy->moduleDirectory.size(),"%s",folder);legacy->persistenceEnabled=true;legacy->persistenceGeneration=10;assert(legacy->writeClipFile());
 const auto file=std::string(folder)+"/thru-clips.bin";std::ifstream in(file,std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});in.close();bytes.resize(bytes.size()-4);stripScannedHitPrograms(bytes);assert(bytes[8]==52);std::uint32_t hash=2166136261u;for(auto x:bytes){hash^=x;hash*=16777619u;}auto* out=std::fopen(file.c_str(),"wb");assert(out);assert(std::fwrite(bytes.data(),1,bytes.size(),out)==bytes.size()&&writeU32(out,hash));std::fclose(out);
 assert(restored->readClipFileFromPath(file.c_str()));const auto& patch=restored->clipSlots[0].scannedPrograms[0].patches[0];assert(patch.v[sc::motion]==2.5f&&patch.v[sc::art]==0&&patch.v[sc::decay]==350);
 restored.reset();legacy.reset();t->persistenceEnabled=false;t.reset();std::filesystem::remove_all(folder);
 std::puts("SCANNED percussion storage: format53/JSON restart, recorded Loss gestures and genuine format52 TONE migration with original settings: PASS");
}
void testScannedPercussion(){testScannedToneReference();testScannedPercussionCore();testScannedPercussionModule();testScannedPercussionStorage();}
