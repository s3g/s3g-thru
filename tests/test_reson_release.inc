void testResonReleaseCore() {
  namespace r=s3g_thru_reson;
  for(float sr:{44100.f,48000.f,96000.f}) {
    r::Program patches;
    r::Instrument held,released;
    held.prepare();released.prepare();
    for(int f=0;f<int(sr*.3f);++f) {
      const float at=6.f*f/sr;
      assert(held.tick(patches.patches,at,110,sr)==released.tick(patches.patches,at,110,sr));
    }
    const float frozen=released.position;
    assert(frozen>1&&frozen<2);
    released.release();
    double liveEnergy=0,tailEnergy=0;
    for(int f=0;f<int(sr*3);++f) {
      const float phase=std::fmod(.3f+f/sr,1.f);
      const float target=3*(1-std::abs(2*phase-1));
      auto a=held.tick(patches.patches,target,110,sr),b=released.tick(patches.patches,target,110,sr);
      assert(released.position==frozen);
      for(float x:b)assert(std::isfinite(x));
      if(f>sr*2){liveEnergy+=double(a[0])*a[0];tailEnergy+=double(b[0])*b[0];}
    }
    assert(liveEnergy>1 && tailEnergy<1e-12 && released.finished && !held.finished);
    // A frozen blend that includes TONE must retain its normal release.
    released.reset();released.tick(patches.patches,.2f,110,sr);released.release();
    for(int f=0;f<int(sr*3);++f)released.tick(patches.patches,2,110,sr);
    assert(!released.finished&&released.position==.2f);
    // Release before the first audio callback is also a stable one-note hold.
    released.reset();released.release();released.tick(patches.patches,1.5f,110,sr);
    assert(released.released&&released.position==1.5f);
    released.tick(patches.patches,0,110,sr);assert(released.position==1.5f);
  }
  std::puts("RESON release core: held mixed-stack return retained, exact frozen HIT/TONE blends, spent HIT completion and pre-audio release at three rates: PASS");
}

void testResonNaturalCompletion() {
  namespace r=s3g_thru_reson;
  float finalPeak=0;
  for(float sr:{44100.f,48000.f,96000.f})for(int body=1;body<=8;++body) {
    r::Program p;
    for(auto& c:p.patches){c=r::startingSound(body,c);c.v[r::mode]=1;
      c.v[r::decay]=35;c.v[r::noise_decay]=15;c.v[r::hit_decay]=1;
      c.v[r::drive]=100;c.v[r::clip]=100;c.v[r::bias]=body%2?80:-80;
      c.v[r::output]=12;c.v[r::tone]=200;c.v[r::sag]=100;}
    r::Instrument voice;voice.prepare();voice.setPatchRange(0,0);
    int completed=-1;double energy=0;
    for(int f=0;f<int(sr*3);++f){
      if(f%128==0)voice.beginBlock(p.patches);
      const auto a=voice.tick(p.patches,0,110,sr);
      for(float x:a){assert(std::isfinite(x));energy+=double(x)*x;}
      if(voice.finished){completed=f;finalPeak=std::max(finalPeak,std::max(std::abs(a[0]),std::abs(a[1])));break;}
    }
    assert(completed>sr*.05f&&completed<sr*3&&energy>.001);
    assert(voice.models[0].finished&&voice.models[0].latchedHit);
    for(int f=0;f<1024;++f){assert((voice.tick(p.patches,3,440,sr)==std::array<float,2>{}));}
    voice.reset();voice.setPatchRange(0,0);double restart=0;
    for(int f=0;f<1024;++f){auto a=voice.tick(p.patches,0,110,sr);restart+=a[0]*a[0];}
    assert(restart>.001&&!voice.finished&&!voice.released);
  }
  assert(finalPeak<1e-6f);
  // Neither zero crossings/muting nor TONE count as terminal silence.
  r::Program p;for(auto& c:p.patches){c.v[r::mode]=1;c.v[r::decay]=8000;c.v[r::level]=0;}
  r::Instrument voice;voice.prepare();voice.setPatchRange(0,0);
  for(int f=0;f<22050;++f)voice.tick(p.patches,0,110,44100);
  assert(!voice.finished&&!voice.models[0].finished);
  p.patches[0].v[r::level]=80;voice.beginBlock(p.patches);double unmuted=0;
  for(int f=0;f<4096;++f){auto a=voice.tick(p.patches,0,110,44100);unmuted+=a[0]*a[0];}
  assert(unmuted>1&&!voice.finished);
  std::printf("RESON natural completion: eight HIT bodies, three rates, hard/asymmetric overload and low-pass ring-out, terminal level %.9g, reset and live unmute protection: PASS\n",finalPeak);
}

void configureResonTail(ThruInstance& t,bool hits) {
  namespace r=s3g_thru_reson;
  auto& c=t.controls;c.reson={};c.ampAttackMs=3;c.ampDecayMs=0;c.ampSustain=1;c.ampReleaseMs=3000;
  c.lanePath=Engine::LanePath::triangle;c.laneCycles=1;
  c.effectDrive=c.effectMix=c.effectReverbMix=c.effectTapeMix=c.effectRotaryMix=0;
  if(hits)for(auto& patch:c.reson.patches){patch=r::startingSound(3,patch);
    patch.v[r::decay]=25;patch.v[r::noise_decay]=10;patch.v[r::hit_decay]=1;}
}

void testResonReleaseModule() {
  // Reproduce the user's TONE/HIT/HIT/HIT stack, then release inside HITs.
  auto t=resonFixture();configureResonTail(*t,false);
  noteMidi(*t,1,48);tableRender(*t,100);
  PlaybackVoice* released=nullptr;for(auto& v:t->voices)if(v.active)released=&v;
  assert(released&&released->held);
  const float frozen=released->engine.normalizedLanePosition();assert(frozen>1.f/3&&frozen<2.f/3);
  noteMidi(*t,1,48,0,false);
  for(int b=0;b<700&&released->active;++b){tableRender(*t,1);
    assert(released->engine.normalizedLanePosition()==frozen);}
  assert(!released->active); // Natural ring-out wins over the 3s outer Release.

  // A long TONE release remains audible and active until its requested end.
  t=resonFixture();configureResonTail(*t,false);
  t->controls.lanePath=Engine::LanePath::manual;t->controls.manualLane=0;
  noteMidi(*t,1,48);tableRender(*t,30);noteMidi(*t,1,48,0,false);
  assert(tableRender(*t,350)>100);
  int active=0;for(auto& v:t->voices)active+=v.active;assert(active==1);
  tableRender(*t,750);for(auto& v:t->voices)assert(!v.active);

  // Completed held HITs leave the voice/compensation pool but key-up remains valid.
  for(int layout:{0,1,2,3,4,5,6}) {
    t=resonFixture(layout);configureResonTail(*t,true);
    t->controls.ampReleaseMs=20000;
    if(layout==6){auto& lane=t->sequenceLive.tracks[0];lane.length=1;lane.enabled=true;
      lane.timeMode=true;lane.timeMs=3000;lane.rate=0;lane.trigs[0].enabled=true;
      t->ensureSequenceClip();t->captureActiveClip(false);t->startClipPlaybackNow();}
    else noteMidi(*t,1,48);
    assert(tableRender(*t,20)>10);
    tableRender(*t,500);
    if(layout!=6){
      int owners=0;for(const auto& v:t->voices)if(v.active){++owners;assert(v.held&&v.engine.resonNoteFinished());}
      assert(owners==1&&t->activeNote==48);
      noteMidi(*t,1,48,0,false);tableRender(*t,1);for(const auto& v:t->voices)assert(!v.active);
      assert(t->activeNote==-1);
      noteMidi(*t,1,48);assert(tableRender(*t,20)>10);noteMidi(*t,1,48,0,false);
    }else{for(auto& v:t->voices)assert(!v.active);assert(t->activeNote==-1);t->setClipPlaying(false);}
  }
  // Sleeping held HITs must still finish lane-load/record maintenance fades.
  t=resonFixture();configureResonTail(*t,true);noteMidi(*t,1,48);tableRender(*t,500);
  PlaybackVoice* sleeping=nullptr;for(auto& v:t->voices)if(v.active)sleeping=&v;
  assert(sleeping&&sleeping->held&&sleeping->engine.resonNoteFinished());
  for(int ch=0;ch<2;++ch)std::fill_n(t->engine.writableLane(0,ch),1024,.1f);
  assert(t->engine.adoptLane(0,1024,t->sampleRate));t->serviceVoiceLaneSync();
  assert(sleeping->engine.laneTransitionActive());tableRender(*t,40);
  assert(sleeping->engine.laneLoaded(0)&&!sleeping->engine.laneTransitionActive());
  assert(t->engine.beginLaneRemoval(0));tableRender(*t,40);
  assert(!t->engine.laneLoaded(0)&&!sleeping->engine.laneLoaded(0)&&!sleeping->engine.laneTransitionActive());
  assert(sleeping->engine.resonNoteFinished());
  noteMidi(*t,1,48,0,false);tableRender(*t,1);
  // Voice stealing must complete the new attack rather than retiring its new note.
  t=resonFixture();configureResonTail(*t,true);
  for(int n=0;n<12;++n){noteMidi(*t,1,48+n);tableRender(*t,3);noteMidi(*t,1,48+n,0,false);}
  noteMidi(*t,1,72);assert(tableRender(*t,10)>10);
  bool newest=false;for(const auto& v:t->voices)if(v.active&&v.note==72)newest=true;assert(newest);
  tableRender(*t,500);
  for(const auto& v:t->voices)if(v.active)assert(v.note==72&&v.held&&v.engine.resonNoteFinished());
  noteMidi(*t,1,72,0,false);tableRender(*t,1);for(const auto& v:t->voices)assert(!v.active);
  std::puts("RESON module: actual MIDI release freeze, TONE ADSR, terminal held HITs across seven modes, valid late key-up/retrigger and dense steals: PASS");
}

void testResonSilentVoiceCompensationAndFade() {
  namespace r=s3g_thru_reson;
  auto t=resonFixture();configureResonTail(*t,true);
  t->controls.lanePath=Engine::LanePath::manual;t->controls.manualLane=0;
  for(auto& c:t->controls.reson.patches)c.v[r::mode]=0;
  noteMidi(*t,1,48);tableRender(*t,10);
  for(auto& c:t->controls.reson.patches)c.v[r::mode]=1;
  for(int n=0;n<7;++n){noteMidi(*t,1,49+n);tableRender(*t,5);}
  int active=0;for(const auto& v:t->voices)active+=v.active;assert(active==8);
  tableRender(*t,600);
  int audible=0;for(const auto& v:t->voices)if(v.active&&!v.engine.resonNoteFinished()){++audible;assert(v.note==48);}
  assert(audible==1&&t->polyphonyGain>.999f);
  // Silent held touches retain their list entries until key-up.
  for(int n=0;n<7;++n)noteMidi(*t,1,49+n,0,false);
  tableRender(*t,1);active=0;for(const auto& v:t->voices)if(v.active){++active;assert(v.note==48);}
  assert(active==1);
  // A completed but physically held HIT still reserves its Touch-list entry.
  t=resonFixture();configureResonTail(*t,true);
  setParam(t.get(),"voice_engine_mode","1");setParam(t.get(),"voice_engine_order","0");
  setListSlot(*t,0,19);setListSlot(*t,1,15);
  noteMidi(*t,1,48);tableRender(*t,500);
  noteMidi(*t,1,49);assert(tableRender(*t,20)>10);
  bool firstHeld=false,secondHeld=false;
  for(const auto& v:t->voices)if(v.active&&v.held){
    if(v.note==48){firstHeld=true;assert(v.engineSlot==0&&v.engine.resonNoteFinished());}
    if(v.note==49){secondHeld=true;assert(v.engineSlot==1&&v.assignedAlgorithm==15);}
  }
  assert(firstHeld&&secondHeld);
  // A source scene copied for clip interpolation retains its individual release hold.
  t=resonFixture();configureResonTail(*t,false);t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
  noteMidi(*t,1,48);tableRender(*t,100);noteMidi(*t,1,48,0,false);tableRender(*t,1);
  float frozen=-1;for(auto& v:t->voices)if(v.active)frozen=v.engine.normalizedLanePosition();assert(frozen>=0);
  t->captureActiveClip(true);t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
  assert(t->prepareClipBank(1));t->clipPrepareState.store(int(ClipPrepareState::ready));t->patternFadeMs=1500;
  t->patternWorkerTestBlocked.store(true);
  assert(t->activatePreparedClip()&&t->patternActive);
  for(const auto& v:t->patternShadow->voices)if(v.active){assert(v.engine.normalizedLanePosition()==frozen);
    assert(t->patternShadow->effectiveControls(v).resonNoteReleased);}
  t->patternWorkerTestBlocked.store(false);t->patternSignal.notify();
  std::array<std::int16_t,kBlockFrames*2> out{};int latePeak=0;
  for(int b=0;b<900&&t->patternActive;++b){waitForPatternAudio(*t);t->render(out.data(),kBlockFrames);
    if(b>180)for(auto x:out)latePeak=std::max(latePeak,std::abs(int(x)));}
  assert(!t->patternActive&&t->patternUnderflows==0&&latePeak<64);waitForPatternIdle(*t);
  std::puts("RESON quiet HITs leave gain compensation, TONE stays held, and clip-fade source copies retain per-voice release positions: PASS");
}

void testResonReleaseTails(){testResonReleaseCore();testResonNaturalCompletion();testResonReleaseModule();testResonSilentVoiceCompensationAndFade();}
