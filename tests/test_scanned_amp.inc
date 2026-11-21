#include "scanned_percussion_reference.h"
void testScannedAmpCore(){
 namespace sc=s3g_thru_scanned;namespace prior=s3g_thru_scanned_v28;
 sc::Instrument current;prior::Instrument reference;current.prepare();reference.prepare();float error=0;
 for(float sr:{44100.f,48000.f,96000.f})for(int art:{0,1}){sc::Program p;prior::Program q;for(int lane=0;lane<4;++lane){p.patches[lane].v[sc::art]=art;p.patches[lane].v[sc::decay]=1000;for(int f=0;f<23;++f)q.patches[lane].v[f]=p.patches[lane].v[f];}current.reset();reference.reset();
  for(int n=0;n<9000;++n){if(n%128==0){current.beginBlock(p.patches);reference.beginBlock(q.patches);}const float pos=1.5f+1.45f*std::sin(n*.0002f);auto a=current.tick(p.patches,pos,220,sr),b=reference.tick(q.patches,pos,220,sr);for(int c=0;c<2;++c)error=std::max(error,std::abs(a[c]-b[c]));}
 }
 assert(error==0);
 auto energy=[](sc::Config c,float hz){sc::BassAmp amp;double sum=0;for(int n=0;n<18000;++n){if(n%88==0)amp.prepare(c,88200);const float x=.005f*std::sin(2*sc::pi*hz*n/88200);auto y=amp.tick({x,x});assert(y[0]==y[1]);if(n>8000)sum+=y[0]*y[0];}return sum;};
 sc::Config c;c.v[sc::amp_mode]=1;c.v[sc::amp_gain]=0;c.v[sc::amp_cab]=0;const auto bassFlat=energy(c,40),cabOff=energy(c,7000);c.v[sc::amp_bass]=12;assert(energy(c,40)>bassFlat*6);c.v[sc::amp_bass]=0;c.v[sc::amp_cab]=100;assert(energy(c,7000)<cabOff*.15);
 // No gate is used to hide DC, mode changes or parameter-edit artifacts.
 for(int mode:{1,2}){sc::BassAmp amp;c={};c.v[sc::amp_mode]=mode;c.v[sc::amp_gain]=100;float noise=0;for(int n=0;n<20000;++n){if(n%88==0){const int field=23+(n/88)%8;c.v[field]=(n/176)%2?sc::high[field]:sc::low[field];amp.prepare(c,88200);}auto y=amp.tick({0,0});for(float x:y){assert(std::isfinite(x));noise=std::max(noise,std::abs(x));}}assert(noise<1e-6);}
 // Latched and active notes can change all amp controls without nonfinite
 // filters, hard routing changes or stale histories on re-entering a patch.
 for(float sr:{44100.f,48000.f,96000.f}){sc::Program p;current.reset();float peak=0;for(auto& patch:p.patches){patch.v[sc::width]=0;patch.v[sc::amp_mode]=1;}
  for(int n=0;n<16000;++n){if(n%128==0){const int field=23+(n/128)%8;for(auto& patch:p.patches)patch.v[field]=(n/256)%2?sc::high[field]:sc::low[field];current.beginBlock(p.patches);}auto y=current.tick(p.patches,1.5f+1.49f*std::sin(n*.001f),110,sr,5,nullptr,1);assert(y[0]==y[1]);for(float x:y){assert(std::isfinite(x));peak=std::max(peak,std::abs(x));}}assert(peak<4);
 }
 std::printf("SCANNED amp core: exact test.28 bypass (%.9f), bass/cab responses, silent live edits, linked stereo and finite rates/extremes: PASS\n",error);
}
void testScannedAmpModule(){
 namespace sc=s3g_thru_scanned;
 for(int mode=0;mode<7;++mode){auto t=scannedFixture(mode);for(int part=0;part<(t->splitEnabled?t->partCount():1);++part){if(t->splitEnabled)t->selectSplitPart(part,false);for(auto& p:t->controls.scanned.patches){p.v[sc::amp_mode]=2;p.v[sc::amp_gain]=60;}t->midiPart=part;t->noteOn(60+part,100);}assert(tableRender(*t,10)>100);t->allNotesOff();tableRender(*t,120);for(const auto& v:t->voices)assert(!v.active);}
 auto t=scannedFixture();setParam(t.get(),"scanned_amp_mode","1");for(int n=0;n<8;++n)t->noteOn(48+n,100);assert(tableRender(*t,20)>100);t->noteOn(72,100);tableRender(*t,10);t->allNotesOff();tableRender(*t,120);for(const auto& v:t->voices)assert(!v.active);
 t=scannedFixture(2);t->selectSplitPart(1,false);nrpn(*t,5,"scanned_amp_gain",s3g_thru_midi::encode(midiParameter("scanned_amp_gain"),70,100));assert(t->splitFocus==1&&std::abs(t->liveControlsForPart(0).scanned.patches[0].v[sc::amp_gain]-70)<.01);
 t=scannedFixture(6);setParam(t.get(),"seq_hold","0");setParam(t.get(),"scanned_amp_mode","2");setParam(t.get(),"scanned_amp_gain","80");assert(t->controls.scanned.patches[0].v[sc::amp_mode]==0);const auto locks=t->sequenceLive.soundLocks.resolve(0,0,0);assert(locks.find(4323)->value==2&&locks.find(4324)->value==80);int part=0;const auto* remote=e16PageSlotParameter(*t,5,1,part);assert(remote&&!std::strcmp(remote->key,"scanned_amp_gain")&&midiRead(*t,*remote,part)==80);setParam(t.get(),"seq_hold","-1");t->startSequenceVoice(0,0,0,.5,true,0);assert(tableRender(*t,8)>100);
 std::puts("SCANNED amp module: seven layouts, eight notes/steals/releases, isolated MIDI parts and contextual E16 trig locks: PASS");
}
void testScannedAmpStorage(){
 namespace sc=s3g_thru_scanned;char folder[]="/tmp/thru-amp-state-XXXXXX";assert(mkdtemp(folder));auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();setParam(t.get(),"algorithm","16");setParam(t.get(),"scanned_amp_mode","2");t->beginClipRecording();t->noteOn(60,100);tableRender(*t,8);setParam(t.get(),"scanned_amp_gain","72");tableRender(*t,8);t->noteOff(60);t->finishClipRecording();t->captureActiveClip(true);assert(t->savePersistentState());auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(folder));restored->stopBackgroundWork();assert(restored->controls.scanned.patches[0].v[sc::amp_mode]==2&&restored->controls.scanned.patches[0].v[sc::amp_gain]==72);
 bool gesture=false;for(int i=0;i<restored->activeClipSlot()->clipPointCount;++i){const auto& p=restored->activeClipSlot()->clipPoints[i];gesture|=p.type==ClipPointType::scannedParameter&&(p.parameter&255)==sc::amp_gain&&p.value==72;}assert(gesture);
 // Real format53 has PERC data, but no amp block, points or locks.
 auto legacy=scannedFixture();setParam(legacy.get(),"scanned_art","1");setParam(legacy.get(),"scanned_decay","620");legacy->captureActiveClip(true);std::snprintf(legacy->moduleDirectory.data(),legacy->moduleDirectory.size(),"%s",folder);legacy->persistenceEnabled=true;legacy->persistenceGeneration=10;assert(legacy->writeClipFile());const auto file=std::string(folder)+"/thru-clips.bin";std::ifstream in(file,std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});in.close();bytes.resize(bytes.size()-4);stripScannedAmpPrograms(bytes);assert(bytes[8]==53);std::uint32_t hash=2166136261u;for(auto x:bytes){hash^=x;hash*=16777619u;}auto* out=std::fopen(file.c_str(),"wb");assert(out);assert(std::fwrite(bytes.data(),1,bytes.size(),out)==bytes.size()&&writeU32(out,hash));std::fclose(out);assert(restored->readClipFileFromPath(file.c_str()));const auto& p=restored->clipSlots[0].scannedPrograms[0].patches[0];assert(p.v[sc::art]==1&&p.v[sc::decay]==620&&p.v[sc::amp_mode]==0&&p.v[sc::amp_gain]==35);
 restored.reset();legacy.reset();t->persistenceEnabled=false;t.reset();std::filesystem::remove_all(folder);
 std::puts("SCANNED amp storage: format54/JSON restart, gain gestures, portable projects and genuine format53 PERC migration: PASS");
}
void testScannedAmp(){testScannedAmpCore();testScannedAmpModule();testScannedAmpStorage();}
