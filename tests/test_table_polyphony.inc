#include "table_spline_reference.h"
void testTableSplineReadPlans() {
  namespace tab=s3g_thru_table;const auto& library=tab::Library::get();float error=0;
  for(float rate:{44100.f,48000.f,96000.f})for(float points:{1.f,4.f,8.f,13.25f,32.f})
   for(float curve:{0.f,37.f,100.f})for(float hz:{55.f,220.f,2000.f,10000.f}){
    tab::Config p;p.v[tab::type]=1;p.v[tab::points]=points;p.v[tab::curve]=curve;
    tab::State state;state.reset(p,hz,rate,51);
    for(int frame=0;frame<96;++frame){
      if(frame==48)p.v[tab::seed]=27;
      state.tick(library,p,hz,rate);assert(state.splinePlansReady);
      const tab::Library::Index index(std::fmod(frame*.61803398875,1.0));
      for(int ch=0;ch<2;++ch){
        const float direct=state.splineRead(library,index,ch,.37f,.71f);
        const float planned=state.splinePlannedRead(index,ch,.37f,.71f);
        error=std::max(error,std::abs(direct-planned));
        for(const auto& plan:state.splinePlans[ch]){assert(plan.count>0&&plan.count<=8);for(int i=0;i<plan.count;++i)assert(plan.parts[i].cycle&&plan.parts[i].gain>0);}
      }
    }
   }
  assert(error<3e-6f);
  tab::Config p;p.v[tab::type]=1;p.v[tab::points]=8;tab::State state;state.reset(p,220,44100,9);
  state.tick(library,p,220,44100);assert(state.splinePlansReady);
  p.v[tab::points]=20;p.v[tab::curve]=37;
  state.tick(library,p,220,44100);assert(!state.splinePlansReady);
  for(int frame=0;frame<12000;++frame)state.tick(library,p,220,44100);
  assert(state.splinePlansReady);
  for(int frame=0;frame<1024;++frame){p.v[tab::points]=10+2*std::sin(frame*.071f);state.tick(library,p,220,44100);assert(!state.splinePlansReady);}
  std::printf("TABLE Spline read plans: independent direct-reader agreement %.9f, stereo/rates/mips, seed/evolution invalidation and smooth-control direct fallback: PASS\n",error);
}
void testTableSplinePrecision() {
  namespace tab=s3g_thru_table;
  const auto& library=tab::Library::get();double cents=0;
  for(int i=0;i<=200000;++i){
    const float pitch=-96+192.f*i/200000;
    cents=std::max(cents,std::abs(1200*std::log2(library.pitchRatio(pitch)/std::exp2(double(pitch)/12))));
  }
  assert(cents<.003);
  float error=0;
  for(const auto& ref:splineReferences){
    tab::Config p;p.v[tab::type]=1;p.v[tab::width]=75;p.v[tab::drift]=0;p.v[tab::points]=ref.points;p.v[tab::curve]=ref.curve;
    tab::State state;state.reset(p,220,44100,51);int sample=0;
    for(int n=0;n<=4095;++n){
      if(n==1600){p.v[tab::points]=33-ref.points;p.v[tab::curve]=100-ref.curve;}
      const auto out=state.tick(library,p,220,44100);
      if(n==600||n==1200||n==2400||n==4095){for(int ch=0;ch<2;++ch)error=std::max(error,std::abs(out[ch]-ref.samples[sample][ch]));++sample;}
    }
  }
  assert(error<3e-6f);
  for(float rate:{44100.f,48000.f,96000.f}){
    tab::State state;tab::Config p;p.v[tab::type]=1;state.reset(p,220,rate,7);
    for(int n=0;n<int(rate);++n)state.tick(library,p,220,rate);
    for(float value:{state.body,state.pitchEnv,state.scanEnv,state.noiseEnv,state.hitEnv})assert(std::fpclassify(value)!=FP_SUBNORMAL);
    assert(state.hitEnv==0);
  }
  std::printf("TABLE Spline precision: bounded pitch error %.7f cents, frozen test.24 stereo/live-edit error %.9f and normal-or-zero silent envelopes: PASS\n",cents,error);
}
void testTableSplinePolyphony() {
  namespace tab=s3g_thru_table;
  testTableSplinePrecision();
  testTableSplineReadPlans();
  float largestStep=0;
  for(bool scan:{false,true})for(int notes:{5,6,7,8}){
    auto t=tableFixture();
    for(auto& p:t->controls.table.patches){p.v[tab::type]=1;p.v[tab::points]=8;p.v[tab::width]=65;}
    if(scan){t->controls.laneCycles=1;t->controls.lanePath=Engine::LanePath::down;}
    setParam(t.get(),"pattern_fade_lookahead","1");
    for(int i=0;i<notes;++i){
      const std::uint8_t message[]{0x90,std::uint8_t(68+i),100};onMidi(t.get(),message,3,MOVE_MIDI_SOURCE_INTERNAL);tableRender(*t,8);
    }
    int held=0;for(const auto& v:t->voices){held+=v.active&&v.held;assert(!v.stealPending);}assert(held==notes);
    std::array<std::int16_t,kBlockFrames*2> audio{};float previous=0;
    for(int block=0;block<512;++block){
      if(block==128)t->controls.table.patches[0].v[tab::seed]=19;
      t->render(audio.data(),kBlockFrames);double energy=0;
      for(int frame=0;frame<int(kBlockFrames);++frame){
        const float left=audio[frame*2]/32768.f,right=audio[frame*2+1]/32768.f;
        energy+=double(left)*left+double(right)*right;
        if(block>8)largestStep=std::max(largestStep,std::abs(left-previous));previous=left;
        assert(std::abs(left)<.9f&&std::abs(right)<.9f);
      }
      if(block>8)assert(energy>1e-8);
      held=0;for(const auto& v:t->voices){held+=v.active&&v.held;assert(!v.stealPending);}assert(held==notes);
    }
    // Only the ninth pad should need the existing eight-voice steal path.
    if(notes==8){const std::uint8_t ninth[]{0x90,76,100};onMidi(t.get(),ninth,3,MOVE_MIDI_SOURCE_INTERNAL);bool pending=false;for(const auto& v:t->voices)pending|=v.stealPending;assert(pending);tableRender(*t,12);}
    t->allNotesOff();tableRender(*t,100);for(const auto& v:t->voices)assert(!v.active);
  }
  assert(largestStep<.35f);
  std::printf("TABLE Spline polyphony: real pad MIDI, five/six/seven/eight held voices, stereo, stack motion, lookahead, live seed crossfade, ninth-note stealing and releases (max step %.6f): PASS\n",largestStep);
}
