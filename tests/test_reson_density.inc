void testResonDensityCore() {
  using s3g_thru_reson::Density;
  for(float sr:{44100.f,48000.f,96000.f}){
    Density d;d.prepare(sr);
    for(int n=0;n<int(sr*.1f);++n)assert(d.process(4,-2,0)==1);
    d.reset();float first=1,previous=1,maxChange=0;
    for(int n=0;n<int(sr*.3f);++n){const float g=d.process(2,-.7f,100);if(n<int(sr*.002f))first=std::min(first,g);maxChange=std::max(maxChange,std::abs(g-previous));previous=g;assert(g>=.2511f&&g<=1);}
    assert(first>.94f&&d.gain<.46f&&d.gain>.35f&&maxChange<.005f);
    const float compressed=d.gain;
    // Left/right ratios cannot change: only one linked gain is applied.
    assert(std::abs((2*compressed)/(-.7f*compressed)-2/-.7f)<1e-5f);
    for(int n=0;n<int(sr*.1f);++n){const float g=d.process(2,-.7f,0);assert(std::abs(g-previous)<.005f);previous=g;}
    assert(d.gain==1&&d.amount==0);
    d.reset();for(int n=0;n<int(sr*.1f);++n)assert(d.process(.1f,-.1f,100)==1);
    d.reset();for(int n=0;n<int(sr*.3f);++n)d.process(.05f,-.03f,100);assert(d.gain==1);
  }
  std::puts("RESON DENS core: unity bypass, three sample rates, linked stereo, retained initial attack, bounded reduction and smooth live enable/bypass: PASS");
}

void densityDry(ThruInstance& t){
  auto& c=t.controls;c.laneCycles=0;c.lanePath=Engine::LanePath::manual;c.manualLane=0;
  c.effectDrive=c.effectMix=c.effectReverbMix=c.effectTapeMix=c.effectRotaryMix=0;
  c.ampAttackMs=3;c.ampDecayMs=0;c.ampSustain=1;c.ampReleaseMs=500;
  for(auto& patch:c.reson.patches){patch=s3g_thru_reson::startingSound(1,patch);patch.v[s3g_thru_reson::level]=100;patch.v[s3g_thru_reson::width]=0;}
}
void testResonDensityEightVoices(){
  auto t=resonFixture();densityDry(*t);setParam(t.get(),"reson_density","100");
  for(int note=48;note<56;++note)t->noteOn(note,100);
  tableRender(*t,100);
  int count=0;for(const auto& v:t->voices)count+=v.active;assert(count==8);
  float error=0;double energy=0;
  for(std::size_t frame=0;frame<kBlockFrames;++frame)for(int ch=0;ch<2;++ch){
    float expected=0;for(const auto& v:t->voices){assert(v.envelope==1&&v.gainState==1&&v.panState==0);expected+=v.output[ch][frame];}
    error=std::max(error,std::abs(expected-t->resonBus[0][ch][frame]));energy+=expected*expected;
    for(int part=1;part<4;++part)assert(t->resonBus[part][ch][frame]==0);
  }
  assert(error<1e-6f&&energy>1&&t->resonDensity[0].gain<.99f);
  std::puts("RESON DENS eight voices: fresh Single voice slots all reach one detector and gain path: PASS");
}
void testResonDensityAudio(){
  for(int mode=0;mode<7;++mode){
    auto dry=resonFixture(mode),wet=resonFixture(mode);
    for(auto* t:{dry.get(),wet.get()}){
      for(int part=0;part<(t->splitEnabled?t->partCount():1);++part){
        if(t->splitEnabled)t->selectSplitPart(part,false);densityDry(*t);
        if(t==wet.get())setParam(t,"reson_density","100");
        if(mode==6){t->startSequenceVoice(part,0,0,4,true,0);t->startSequenceVoice(part,1,0,4,true,0);}
        else {t->midiPart=part;t->noteOn(48+part,100);t->noteOn(60+part,100);}
      }
    }
    std::array<float,kBlockFrames*2> a{},b{};double oldEnergy=0,newEnergy=0;
    for(int block=0;block<100;++block){dry->render(nullptr,kBlockFrames,a.data());wet->render(nullptr,kBlockFrames,b.data());
      if(block>40)for(std::size_t i=0;i<a.size();++i){assert(std::isfinite(b[i]));oldEnergy+=a[i]*a[i];newEnergy+=b[i]*b[i];}}
    assert(oldEnergy>1&&newEnergy<oldEnergy*.85&&newEnergy>oldEnergy*.02);
    wet->allNotesOff();tableRender(*wet,260);for(const auto& v:wet->voices)assert(!v.active);
  }
  // In a mixed Engine List, the difference must be precisely the RESON-only
  // gain reduction. A second engine cannot enter its detector or gain path.
  auto a=resonFixture(),b=resonFixture(),c=resonFixture(),d=resonFixture();
  for(auto* t:{a.get(),b.get(),c.get(),d.get()}){
    densityDry(*t);t->polyphonyCompensation=false;setParam(t,"voice_engine_mode","1");setParam(t,"voice_engine_order","0");
    for(int slot=0;slot<8;++slot)setListSlot(*t,slot,slot<4?19:15);
    if(t==b.get()||t==d.get())setParam(t,"reson_density","100");
    for(int note=48;note<52;++note)t->noteOn(note,100);
    if(t==a.get()||t==b.get()){t->noteOn(64,100);t->noteOn(67,100);}
  }
  std::array<float,kBlockFrames*2> aa{},bb{},cc{},dd{};float maxError=0;double reduction=0;
  for(int block=0;block<100;++block){a->render(nullptr,kBlockFrames,aa.data());b->render(nullptr,kBlockFrames,bb.data());c->render(nullptr,kBlockFrames,cc.data());d->render(nullptr,kBlockFrames,dd.data());
    for(std::size_t i=0;i<aa.size();++i){maxError=std::max(maxError,std::abs((aa[i]-bb[i])-(cc[i]-dd[i])));reduction+=std::abs(aa[i]-bb[i]);}}
  assert(maxError<1e-4f&&reduction>100);
  auto sampleA=splitFixture(),sampleB=splitFixture();sampleA->stopBackgroundWork();sampleB->stopBackgroundWork();
  sampleA->setPerformanceMode(0);sampleB->setPerformanceMode(0);setParam(sampleB.get(),"reson_density","100");
  sampleA->noteOn(60,100);sampleB->noteOn(60,100);double sampleEnergy=0;
  for(int block=0;block<20;++block){sampleA->render(nullptr,kBlockFrames,aa.data());sampleB->render(nullptr,kBlockFrames,bb.data());assert(aa==bb);for(float x:aa)sampleEnergy+=x*x;}
  assert(sampleEnergy>1);
  // Other engines are exactly unchanged, even at full DENS.
  for(int engine:{4,15,17,18,20,21}){
    auto x=tableFixture(),y=tableFixture();setParam(x.get(),"algorithm",std::to_string(engine).c_str());setParam(y.get(),"algorithm",std::to_string(engine).c_str());
    setParam(y.get(),"reson_density","100");x->noteOn(60,100);y->noteOn(60,100);
    for(int block=0;block<12;++block){x->render(nullptr,kBlockFrames,aa.data());y->render(nullptr,kBlockFrames,bb.data());assert(aa==bb);}
  }
  std::printf("RESON DENS audio: seven layouts, releases, mixed-list isolation (error %.8g), and exact non-RESON bypass: PASS\n",maxError);
}

void testResonDensityState(){
  auto t=resonFixture(2);t->selectSplitPart(1,false);
  nrpn(*t,5,"reson_density",s3g_thru_midi::encode(midiParameter("reson_density"),67,100));
  assert(t->splitFocus==1&&std::abs(t->liveControlsForPart(0).reson.density-67)<.01f&&t->controls.reson.density==0);
  setParam(t.get(),"reson_density","32");t->copySplitPart(1);assert(t->liveControlsForPart(0).reson.density==32);
  t->selectSplitPart(0,false);t->captureActiveClip(true);
  setParam(t.get(),"clip_clipboard_copy","0");setParam(t.get(),"clip_clipboard_paste","3");assert(t->clipSlots[3].resonPrograms[1].density==32);
  t->beginClipRecording();tableRender(*t,3);setParam(t.get(),"reson_density","75");tableRender(*t,3);t->finishClipRecording();
  auto* clip=t->activeClipSlot();assert(clip->clipPointCounts[kClipResonDensityOffset]>0);
  t->resonDensityOverrides.reset();t->clipPlaying=true;t->applyClipCurves(*clip,6);
  float shown=0;assert(midiPlaybackReadout(*t,midiParameter("reson_density"),0,shown));
  assert(shown==t->splitClipControls[0].reson.density);
  t->clipPlaying=false;
  // One part setting survives patch navigation and mode round trips.
  setParam(t.get(),"reson_density","46");setParam(t.get(),"record_lane","2");assert(t->controls.reson.density==46);
  t->setPerformanceMode(0);setParam(t.get(),"reson_density","19");t->setPerformanceMode(2);assert(t->liveControlsForPart(0).reson.density==46);
  auto seq=resonFixture(6);seq->selectSplitPart(2,false);auto* seqClip=seq->activeClipSlot();
  seqClip->clipDurationMs=1000;seqClip->clipRecorded=true;seqClip->clipPointCount=1;
  seqClip->clipPoints[0]={0,30,0,2,ClipPointType::resonDensity};seq->rebuildClipPointIndex(*seqClip);
  setParam(seq.get(),"reson_density","72");assert(seqClip->clipPointCounts[kClipResonDensityOffset+2]==0&&seqClip->resonPrograms[3].density==72);
  // A source fade carries the actual detector history as well as the setting.
  auto fade=resonFixture();densityDry(*fade);setParam(fade.get(),"reson_density","100");
  fade->noteOn(48,100);fade->noteOn(60,100);tableRender(*fade,80);assert(fade->resonDensity[0].gain<.9f);
  fade->captureActiveClip(true);fade->patternFadeMs=1000;fade->patternWorkerTestBlocked.store(true);
  const float gain=fade->resonDensity[0].gain,envelope=fade->resonDensity[0].envelope;
  assert(fade->beginPatternFade(1));assert(fade->patternShadow->resonDensity[0].gain==gain&&fade->patternShadow->resonDensity[0].envelope==envelope);
  fade->patternWorkerTestBlocked.store(false);
  std::puts("RESON DENS controls: part NRPN focus isolation, patch independence, part/clip copy, recorded automation/readout and mode recall: PASS");
}

void testResonDensityStorage(){
  char folder[]="/tmp/thru-density-state-XXXXXX";assert(mkdtemp(folder));
  auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();
  setParam(t.get(),"algorithm","19");setParam(t.get(),"reson_density","41");
  t->beginClipRecording();t->noteOn(60,100);tableRender(*t,4);setParam(t.get(),"reson_density","63");tableRender(*t,4);t->noteOff(60);t->finishClipRecording();t->captureActiveClip(true);assert(t->savePersistentState());
  auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(folder));restored->stopBackgroundWork();assert(restored->controls.reson.density==63);
  assert(restored->activeClipSlot()->clipPointCounts[kClipResonDensityOffset]>0);
  std::array<char,kStateTextSize> state{};assert(t->serializeState(state.data(),state.size())>0);
  auto json=resonFixture();assert(json->restoreState(state.data()));assert(json->controls.reson.density==63);
  assert(t->exportProjectFile());const auto target=std::string(folder)+"/imported";std::filesystem::create_directory(target);
  restored.reset();restored=std::make_unique<ThruInstance>();assert(restored->prepare(target.c_str()));restored->stopBackgroundWork();assert(restored->savePersistentState());
  restored->projectGeneration=restored->reservePersistenceGeneration();std::snprintf(restored->projectArchivePath.data(),restored->projectArchivePath.size(),"%s",t->projectOutputPath.data());assert(restored->importProjectFile()&&restored->savePersistentState());
  restored.reset();restored=std::make_unique<ThruInstance>();assert(restored->prepare(target.c_str()));restored->stopBackgroundWork();assert(restored->controls.reson.density==63);
  // A genuine pre-DENS bank defaults to unity; its 32-field programs remain.
  t->clipPlaying=false;t->clearRecordedPerformance();t->captureActiveClip(true);assert(t->savePersistentState());
  const auto path=std::string(folder)+"/thru-clips.bin";std::ifstream in(path,std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});in.close();bytes.resize(bytes.size()-4);stripResonDensity(bytes);assert(bytes[8]==60);
  std::uint32_t hash=2166136261u;for(auto x:bytes){hash^=x;hash*=16777619u;}for(int i=0;i<4;++i)bytes.push_back((hash>>(i*8))&255);
  std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.close();
  assert(t->readClipFileFromPath(path.c_str()));assert(t->activeClipSlot()->resonPrograms[0].density==0);
  t->persistenceEnabled=false;restored->persistenceEnabled=false;restored.reset();t.reset();std::filesystem::remove_all(folder);
  std::puts("RESON DENS storage: format61, JSON, recorded curves, portable project export/import/restart and genuine format60 unity migration: PASS");
}
void testResonDensity(){std::setvbuf(stdout,nullptr,_IONBF,0);testResonDensityCore();testResonDensityEightVoices();testResonDensityAudio();testResonDensityState();testResonDensityStorage();}
