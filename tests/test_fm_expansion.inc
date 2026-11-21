#include "fm_v30_voice.h"
void testFmExpansion(){
 namespace f=s3g_thru_fm;namespace old=s3g_thru_fm_v30;
 // Actual previous kernel: old controls and shared envelopes keep their sound.
 f::Instrument voice;old::Instrument reference;voice.prepare();reference.prepare();float maximumError=0;
 for(float sr:{44100.f,48000.f,96000.f})for(int mode:{0,1})for(int alg:{0,1,2,3}){
  f::Program p;old::Program q;
  for(int lane=0;lane<4;++lane){auto& c=q.patches[lane];c.v[old::mode]=mode;c.v[old::algorithm]=alg;c.v[old::index2]=0;c.v[old::feedback]=20;
   std::copy(c.v.begin(),c.v.end(),p.patches[lane].v.begin());f::legacyExtras(p.patches[lane]);}
  voice.reset();reference.reset();
  for(int n=0;n<6000;++n){if(n%128==0){voice.beginBlock(p.patches);reference.beginBlock(q.patches);}const float position=1.5f+1.4f*std::sin(n*.001f);const auto a=voice.tick(p.patches,position,220,sr),b=reference.tick(q.patches,position,220,sr);for(int ch=0;ch<2;++ch)maximumError=std::max(maximumError,std::abs(a[ch]-b[ch]));}
 }
 assert(maximumError==0);
 // Distinct modulation envelopes, bounded live edits and no phase restart.
 f::Config c;c.v[f::attack1]=100;c.v[f::decay1]=200;c.v[f::end1]=20;c.v[f::attack2]=0;c.v[f::decay2]=50;c.v[f::end2]=60;
 f::OperatorEnvelope a,b;a.reset(c,0,44100);b.reset(c,1,44100);
 for(int i=0;i<4410;++i){a.tick(c,0,false,.003f,1);b.tick(c,1,false,.003f,1);}assert(a.value>.95f&&b.value>.59f&&b.value<.61f);
 const float phase=a.decayPosition;c.v[f::decay1]=800;a.prepare(c,0,44100);a.tick(c,0,false,.003f,1);assert(a.decayPosition>=phase&&a.decayPosition<phase+.001f);
 for(int i=0;i<44100;++i)a.tick(c,0,false,.003f,1);assert(a.value>.19f&&a.value<.21f);
 // All explicit starts are audible, finite, original, and retain mix balance.
 for(int start=1;start<=9;++start){f::Program p;for(auto& patch:p.patches){patch.v[f::width]=42;patch.v[f::level]=61;patch=f::startingSound(start,patch);assert(f::valid(patch)&&patch.v[f::width]==42&&patch.v[f::level]==61);}voice.reset();double energy=0;
  for(int n=0;n<10000;++n){if(n%128==0)voice.beginBlock(p.patches);const auto out=voice.tick(p.patches,1.5f,220,44100);for(float x:out){assert(std::isfinite(x)&&std::abs(x)<4);energy+=x*x;}}assert(energy>.05);
 }
 // HARM zero preserves the sine; enriched cycles retain the fundamental.
 const auto& extras=f::Extras::get();assert(extras.harmonics(.17f,f::Extras::shape(0,220,44100))==0);
 for(float sign:{-100.f,100.f}){auto shape=f::Extras::shape(sign,12000,44100);assert(shape.mix>=0&&shape.mix<=1);for(int n=0;n<1024;++n)assert(std::isfinite(extras.harmonics(n/1024.f,shape)));}
 std::printf("FM expansion core: exact v30 audio (%g error), independent envelopes, live phase continuity, nine audible starts and band-limited enrichment: PASS\n",double(maximumError));
}
void testFmExpandedModule(){
 namespace f=s3g_thru_fm;
 auto t=fmFixture();const auto before=t->controls.fm.patches[0];setParam(t.get(),"fm_start","6");assert(t->controls.fm.patches[0].v[f::mode]==1&&t->controls.fm.patches[0].v[f::noise]==75&&t->undoAvailable);t->restoreUndo();assert(t->controls.fm.patches[0].v==before.v);
 t=fmFixture(6);setParam(t.get(),"seq_hold","0");setParam(t.get(),"fm_algorithm","1");setParam(t.get(),"fm_attack2","123");const auto patch=t->controls.fm.patches[0];setParam(t.get(),"fm_start","5");assert(t->controls.fm.patches[0].v==patch.v);assert(t->sequenceLive.soundLocks.resolve(0,0,0).find(4428)->value==123);int part=0;const auto* remote=e16PageSlotParameter(*t,5,4,part);assert(remote&&!std::strcmp(remote->key,"fm_attack2")&&midiRead(*t,*remote,part)==123);
 setParam(t.get(),"seq_hold","-1");t->startSequenceVoice(0,0,0,.5,true,0);assert(tableRender(*t,8)>100);
 // Real format55 migration contains old FMP1 but no new envelopes.
 char folder[]="/tmp/thru-fm-legacy-XXXXXX";assert(mkdtemp(folder));t=fmFixture();setParam(t.get(),"fm_mode","1");setParam(t.get(),"fm_index2","0");setParam(t.get(),"fm_fm_decay","93");t->captureActiveClip(true);std::snprintf(t->moduleDirectory.data(),t->moduleDirectory.size(),"%s",folder);t->persistenceEnabled=true;t->persistenceGeneration=10;assert(t->writeClipFile());const auto path=std::string(folder)+"/thru-clips.bin";std::ifstream input(path,std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});input.close();bytes.resize(bytes.size()-4);stripFmExtras(bytes);assert(bytes[8]==55);std::uint32_t hash=2166136261u;for(auto x:bytes){hash^=x;hash*=16777619u;}auto* out=std::fopen(path.c_str(),"wb");assert(out);assert(std::fwrite(bytes.data(),1,bytes.size(),out)==bytes.size()&&writeU32(out,hash));std::fclose(out);
 assert(t->readClipFileFromPath(path.c_str()));const auto& restored=t->clipSlots[0].fmPrograms[0].patches[0];assert(restored.v[f::index2]==0&&restored.v[f::decay1]==0&&restored.v[f::end1]==-1&&restored.v[f::harm]==0&&restored.v[f::fm_decay]==93);
 t->persistenceEnabled=false;t.reset();std::filesystem::remove_all(folder);
 std::puts("FM expansion module: starting-sound undo, held-trig safety, fourth E16 page, sound locks and genuine format55 migration: PASS");
}
