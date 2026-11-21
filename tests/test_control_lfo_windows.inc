void testLfoEngineWindows() {
  using namespace s3g_thru_lfo;
  auto t=splitFixture();t->stopBackgroundWork();t->setPerformanceMode(0);
  t->controls.algorithm=Engine::PlaybackAlgorithm::sample;
  t->controls.sampleMode=Engine::SampleMode::loop;
  t->controls.field=.2f;t->controls.locus=.5f;
  lfoAssign(*t,0,0,"locus",1);setParam(t.get(),"lfo1_offset","1");
  const auto route=[&](float amount) {
    t->controlLfoRuntime={};t->controlLfoRouted=true;
    t->controlLfoRuntime[0].routes[0]={{0,-1,127},amount,amount,amount,amount,1,1};
  };
  for(float width:{.02f,.2f,.8f,1.f})for(float amount:{-1.f,0.f,1.f}){
    t->controls.field=width;route(amount);auto effective=t->controls;
    t->applyControlLfoControls(effective,0,Domain::engine);
    assert(std::abs(effective.locus-(.5f+amount*(1-width)*.5f))<.00001f);
    const auto w=t->displayWindow();assert(w[0]>=0&&w[1]<=1);
    assert(std::abs((w[1]-w[0])-width)<.00001f);
    assert(t->controlLfoValueForKey(0,"locus",.5f,4)==effective.locus);
    assert(t->controls.locus==.5f&&effective.sampleWindowModulation);
  }
  t->controls.field=1;route(1);
  // FIELD resolves before POS; both the voice and visible window use the pair.
  t->controlLfoRuntime[1].routes[0]={{0,-1,128},-1,-1,-1,-1,1,1};
  auto c=t->controls;t->applyControlLfoControls(c,0,Domain::engine);
  assert(std::abs(c.field-.02f)<.00001f&&std::abs(c.locus-.99f)<.00001f);
  assert(t->displayWindow()[0]>.979f);
  Config cfg;assert(t->controlLfoDestinationAvailable(cfg,127));
  t->padMode=PadMode::slices;assert(!t->controlLfoDestinationAvailable(cfg,127));
  t->padMode=PadMode::rolls;assert(!t->controlLfoDestinationAvailable(cfg,128));
  t->padMode=PadMode::keys;t->controls.algorithm=Engine::PlaybackAlgorithm::slices;
  assert(!t->controlLfoDestinationAvailable(cfg,127));
  t->controls.algorithm=Engine::PlaybackAlgorithm::glitch;t->controls.glitchBufferLocked=true;
  assert(!t->controlLfoDestinationAvailable(cfg,128));
  t->controls.algorithm=Engine::PlaybackAlgorithm::stochastic;
  for(const char* key:{"stochastic_size","stochastic_evolution","stochastic_amplitude","stochastic_timing","stochastic_memory","stochastic_drift"}) {
    const int id=t->controlLfoDestinationForKey(key);assert(id>0&&t->controlLfoDestinationAvailable(cfg,id));
  }
  assert(destinations[130].high==1023); // Never accidentally enter EVOL HOLD.
  // Stale routes from a formerly active layout must not leak into Bank B.
  t->setPerformanceMode(4);cfg.target=1;assert(!t->controlLfoDestinationAvailable(cfg,127));
  std::puts("LFO engine windows: coupled POS/FLD range and AUTO, truthful zoom/readouts, Stochastic targets and unavailable Slice/Roll/Bank/locked buffers: PASS");
}
void testLfoSampleWindowAudio() {
  for(bool stretch:{false,true}) {
    auto f=std::make_unique<CoreFixture>();
    for(std::size_t ch=0;ch<2;++ch)for(std::size_t n=0;n<CoreFixture::size;++n)
      f->storage[ch][n]=.2f+.5f*n/CoreFixture::size;
    Engine::Controls c;c.algorithm=Engine::PlaybackAlgorithm::sample;c.sampleMode=Engine::SampleMode::loop;
    c.field=.2f;c.speed=.05f;c.sampleWindowModulation=true;c.lanesStretch=stretch;
    f->render(c,4096);float previous=f->render(c),peak=0;
    for(int n=0;n<12000;++n){c.locus=.5f+.35f*std::sin(n*.001f);c.field=.2f+.15f*std::sin(n*.0007f);
      const float out=f->render(c);peak=std::max(peak,std::abs(out-previous));previous=out;
      assert(std::isfinite(out));}
    assert(peak<.01f); // Moving smooth source must not become a series of retriggers/clicks.
    c.sampleWindowModulation=false;
    for(int n=0;n<1200;++n){const float out=f->render(c);assert(std::abs(out-previous)<.01f);previous=out;}
  }
  auto f=std::make_unique<CoreFixture>();Engine::Controls c;c.algorithm=Engine::PlaybackAlgorithm::sample;
  c.field=.1f;c.sampleWindowModulation=true;f->render(c,16000);assert(std::abs(f->render(c))<.0001f);
  c.locus=.8f;assert(std::abs(f->render(c,4000))<.0001f); // Completed one-shot stays completed.
  std::puts("LFO Sample window audio: moving overlap readers, stretched playback, route removal and no one-shot retrigger: PASS");
}
void testLfoSequenceWindowStarts() {
  auto t=sequenceFixture();t->selectSplitPart(0,false);t->controls.algorithm=Engine::PlaybackAlgorithm::sample;
  t->controls.locus=.5f;t->controls.field=.2f;t->splitClipControls[0]=t->controls;
  t->sequencePlayback=t->sequenceLive;auto& lane=t->sequencePlayback.tracks[0];
  s3g_thru_sequence::setCount(lane.trigs[0],2);
  const auto route=[&](float amount){t->controlLfoRouted=true;t->controlLfoRuntime={};
    t->controlLfoRuntime[0].initialized=true;
    t->controlLfoRuntime[0].routes[0]={{0,-1,127},amount,amount};};
  const auto last=[&]() -> const PlaybackVoice& {const PlaybackVoice* v=nullptr;
    for(const auto& x:t->voices)if(x.active&&(!v||x.age>v->age))v=&x;assert(v);return *v;};
  route(.6f);t->startSequenceVoice(0,0,0,.25,false,0);assert(std::abs(last().sequence.position-.8f)<.00001f);
  route(-.6f);t->startSequenceVoice(0,0,1,.25,false,0);assert(std::abs(last().sequence.position-.8f)<.00001f);
  t->startSequenceVoice(0,0,0,.25,false,1);assert(std::abs(last().sequence.position-.2f)<.00001f);
  lane.trigs[0].locks.mask|=1;lane.trigs[0].locks.values[0]=.7f;
  t->startSequenceVoice(0,0,0,.25,false,2);assert(std::abs(last().sequence.position-.7f)<.00001f);
  assert(std::abs(t->effectiveControls(last()).locus-.7f)<.00001f);
  lane.trigs[0].locks.mask=0;route(.6f);t->startSequenceVoice(0,0,1,.25,false,3);
  route(-.6f);t->startSequenceVoice(0,0,0,.25,false,3);assert(std::abs(last().sequence.position-.8f)<.00001f);
  std::puts("LFO Sequence windows: sample POS at parent trig, hold across Burst, next-trig update and explicit lock priority: PASS");
}
