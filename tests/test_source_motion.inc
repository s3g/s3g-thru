void testSourceMotion(){
 using namespace s3g_thru_source;
 Config c;c.v[0]=1;c.v[1]=10;c.v[2]=1;c.v[3]=20;c.v[4]=-1;c.v[5]=0;c.v[6]=2;c.v[7]=0;
 Travel a,b;a.prepare(c,1000);b.prepare(c,1000);
 for(int n=0;n<10;++n)assert(a.tick(c,1000,false)==1);
 for(int n=0;n<20;++n)assert(a.tick(c,1000,false)==-1);
 assert(a.tick(c,1000,false)==0);assert(b.tick(c,1000,false)==1);assert(a.tick(c,1000,true)==2);
 Memory memory;assert(memory.prepare(44100));memory.source(1);std::array<float,128> left{},right{};
 for(std::size_t n=0;n<6000;++n){for(int i=0;i<128;++i){const float x=.4f*std::sin(float(n*128+i)*.031f);left[i]=x;right[i]=-.5f*x;}memory.write(left.data(),right.data(),128,false);}
 c.v[9]=25;c.v[10]=200;auto v=memory.view(c,44100);assert(v&&v.safeFrames>1000);
 for(int n=100;n<1000;n+=31){const double at=v.anchor+n;assert(std::abs(v.sample(1,at)+v.sample(0,at)*.5f)<1e-6f);}
 const auto written=memory.written();memory.write(left.data(),right.data(),128,true);assert(memory.written()==written);
 memory.source(2);assert(memory.written()==0&&memory.view(c,44100).generation!=v.generation);

 // Every live reader works with no source file. Source anchors must be
 // captured per event: moving only the view must not retune an active head.
 for(const auto algorithm:{Engine::PlaybackAlgorithm::grains,Engine::PlaybackAlgorithm::longGrains,
       Engine::PlaybackAlgorithm::cutups,Engine::PlaybackAlgorithm::glitch,
       Engine::PlaybackAlgorithm::pulsar,Engine::PlaybackAlgorithm::texture}) {
   Memory history;assert(history.prepare(44100));history.source(1);Config cfg;cfg.v[9]=25;cfg.v[10]=400;
   std::array<float,128> l{},rr{},zero{};std::array<std::array<float,128>,2> one{},two{};
   for(int block=0;block<400;++block){for(int i=0;i<128;++i){l[i]=.35f*std::sin(float(block*128+i)*.03917f);rr[i]=-.5f*l[i];}history.write(l.data(),rr.data(),128,false);}
   auto e1=std::make_unique<Engine>(),e2=std::make_unique<Engine>();
   std::array<std::array<float,64>,4> storage{};
   assert(e1->prepare(44100,storage[0].data(),storage[1].data(),storage[2].data(),storage[3].data(),64));
   assert(e2->prepare(44100,storage[0].data(),storage[1].data(),storage[2].data(),storage[3].data(),64));
   Engine::Controls controls;controls.algorithm=algorithm;controls.liveSource=history.view(cfg,44100);
   controls.grainDensityHz=1;controls.grainSizeMs=200;controls.longGrainLengthMs=400;
   controls.longGrainLayers=1;controls.longGrainSpread=0;controls.cutRateHz=1;controls.cutGate=1;
   controls.pulsarRate=1;controls.pulsarForm=5;controls.pulsarSize=100;
   controls.textureRate=1;controls.textureLengthMs=300;controls.textureRange=0;controls.textureSpread=0;
   const float* input[]{zero.data(),zero.data(),zero.data(),zero.data()};
   float* output1[]{one[0].data(),one[1].data(),one[0].data(),one[1].data()};
   float* output2[]{two[0].data(),two[1].data(),two[0].data(),two[1].data()};
   // All four core outputs preserve stereo channel mapping.
   for(int block=0;block<12;++block){e1->process(input,output1,128,controls);e2->process(input,output2,128,controls);}
   auto shifted=controls;shifted.liveSource.anchor+=2048;shifted.liveSource.frames-=1024;
   for(int block=0;block<2;++block){e1->process(input,output1,128,controls);e2->process(input,output2,128,shifted);for(int i=0;i<128;++i)assert(std::abs(one[0][i]-two[0][i])<1e-6f);}
   long double energy=0;float peak=0;
   for(int block=0;block<4600;++block){
     for(int i=0;i<128;++i){l[i]=.35f*std::sin(float((block+400)*128+i)*.03917f);rr[i]=-.5f*l[i];}
     history.write(l.data(),rr.data(),128,false);controls.liveSource=history.view(cfg,44100);
     e1->process(input,output1,128,controls);
     for(int i=0;i<128;++i){assert(std::isfinite(one[0][i])&&std::isfinite(one[1][i]));assert(std::abs(one[1][i]+.5f*one[0][i])<.0002f);energy+=one[0][i]*one[0][i];peak=std::max(peak,std::abs(one[0][i]));}
   }
   assert(energy>1&&peak<2);
   // Freeze is a stable, still playable window after a complete ring wrap.
   const auto written=history.written();history.write(l.data(),rr.data(),128,true);assert(history.written()==written);
   controls.liveSource=history.view(cfg,44100);energy=0;
   for(int block=0;block<500;++block){e1->process(input,output1,128,controls);for(float x:one[0])energy+=x*x;}
   assert(energy>.01);
   // A changed source generation must discard all old pulse/cut/repeat tails.
   history.source(2);l.fill(0);rr.fill(0);
   for(int block=0;block<400;++block)history.write(l.data(),rr.data(),128,false);
   controls.liveSource=history.view(cfg,44100);
   for(int block=0;block<100;++block)e1->process(input,output1,128,controls);
   for(float x:one[0])assert(std::abs(x)<1e-5f);
   std::printf("Live reader %d: pinned events, stereo, full ring wrap, freeze and source reset PASS\n",int(algorithm));
 }
 // The real render path: B granulates synth A, before A's mute/Blend, with no PCM loaded.
 char folder[]="/tmp/thru-live-test.XXXXXX";assert(mkdtemp(folder));auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();setParam(t.get(),"pattern_fade_lookahead","0");t->setPerformanceMode(2);t->engine.clear();t->refreshPlaybackVoices();
 t->selectSplitPart(0);setParam(t.get(),"algorithm","17");setParam(t.get(),"fm_mode","0");setParam(t.get(),"amp_release","200");
 t->selectSplitPart(1);setParam(t.get(),"algorithm","2");setParam(t.get(),"live_source","2");setParam(t.get(),"live_lookback","25");setParam(t.get(),"live_window","200");setParam(t.get(),"grain_size","60");setParam(t.get(),"grain_density","32");setParam(t.get(),"split_blend","1");
 t->padNoteOn(68,100);t->padNoteOn(72,100);
 long double energy=0;std::array<int16_t,256> out{};
 for(int block=0;block<700;++block){t->render(out.data(),128);if(block>300)for(float x:t->splitBus[1][0])energy+=double(x)*x;}
 std::printf("Live B raw energy %.9g\n",double(energy));assert(energy>.01);
 assert(t->liveMemories[1].written()>50000);const auto previous=t->liveMemories[1].written();setParam(t.get(),"live_freeze","1");renderSplit(*t,50);assert(t->liveMemories[1].written()==previous);
 const auto before=t->liveControlsForPart(0).sourceMotion;t->selectSplitPart(0);setParam(t.get(),"live_source","3");assert(t->controls.sourceMotion.v==before.v);assert(std::strlen(t->lastError.data())>0);
 t->selectSplitPart(1);setParam(t.get(),"travel_enabled","1");setParam(t.get(),"travel_attack_rate","-1");t->captureActiveClip(true);
 const auto slot=t->activeClip;assert(t->clipSlots[slot].sourcePrograms[2].v[2]==-1);assert(t->clipSlots[slot].sourcePrograms[2].v[8]==2);
 char state[kStateTextSize]{};assert(t->serializeState(state,sizeof(state))>0);assert(std::strstr(state,"source_motion_v1"));
 assert(t->writeMetadataFiles());auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(t->moduleDirectory.data()));restored->stopBackgroundWork();
 assert(restored->clipSlots[slot].sourcePrograms[2].v==t->clipSlots[slot].sourcePrograms[2].v);
 restored->selectClip(slot);assert(restored->liveControlsForPart(1).sourceMotion.v[8]==2);


 for(const int algorithm:{3,6,11,14}) {
   t->allNotesOff();renderSplit(*t,100);t->selectSplitPart(1);
   setParam(t.get(),"algorithm",std::to_string(algorithm).c_str());setParam(t.get(),"live_freeze","0");setParam(t.get(),"live_source","2");
   t->padNoteOn(68,100);t->padNoteOn(72,100);energy=0;
   for(int block=0;block<500;++block){t->render(out.data(),128);if(block>200)for(float x:t->splitBus[1][0])energy+=double(x)*x;}
   std::printf("Live B engine %d energy %.9g\n",algorithm,double(energy));assert(energy>.001);
   t->selectSplitPart(0);char source[8192]{};assert(t->formatSourceState(source,sizeof(source))>0);
   const auto state=s3g_thru_json::parse(source);assert(state.at("parts").arrayValue()[1].at("source").number()==2);
   assert(state.at("parts").arrayValue()[1].at("available").number()==1);assert(state.at("parts").arrayValue()[1].at("wave").arrayValue().size()==64);assert(state.at("parts").arrayValue()[1].at("envelope").string().size()==480);
 }
 // Sequence trigs can start a live-input voice without a stored lane sample.
 t->allNotesOff();t->setPerformanceMode(6);t->selectSplitPart(1);setParam(t.get(),"algorithm","14");setParam(t.get(),"live_source","1");
 t->sequencePlayback=t->sequenceLive;t->startSequenceVoice(1,0,0,.25,true,0);
 assert(std::any_of(t->voices.begin(),t->voices.end(),[](const auto& voice){return voice.active&&voice.part==1&&voice.sequence.active;}));
 t->allNotesOff();t->setPerformanceMode(2);t->selectSplitPart(1);setParam(t.get(),"algorithm","2");setParam(t.get(),"live_source","2");
 // Live patches can recover a frozen configuration after reopening: fill first.
 restored->selectSplitPart(1);setParam(restored.get(),"live_freeze","1");restored->padNoteOn(68,100);renderSplit(*restored,200);
 assert(restored->liveMemories[1].view(restored->controls.sourceMotion,restored->sampleRate).anchor>=0);
 const auto frozen=restored->liveMemories[1].written();renderSplit(*restored,20);assert(restored->liveMemories[1].written()==frozen);
 // Travel copies with the instrument, while routing remains destination-owned.
 t->selectSplitPart(0);const auto route=t->controls.sourceMotion.v[8];t->copySplitPart(1);assert(t->liveControlsForPart(0).sourceMotion.v[8]==route);assert(t->liveControlsForPart(0).sourceMotion.v[2]==-1);
 // Direct assignment guards explicit part/engine/slot ownership and preserves an
 // existing route's depth and engine filter when merely opening its editor.
 t->allNotesOff();t->selectSplitPart(1);const auto* size=s3g_thru_midi::find(5200);assert(size);
 const auto* grain=midiParameterForKey("grain_size");assert(grain);const auto old=t->controlLfoPool[7];
 assert(t->assignModulation(1,7,grain->id,int(old.v[0]),old.target,2));
 const auto dest=t->controlLfoPool[7].v[0];assert(dest>0&&t->controlLfoPool[7].target==1);
 t->controlLfoPool[7].v[4]=.31f;t->controlLfoPool[7].engine=2;
 assert(t->assignModulation(1,7,grain->id,int(dest),1,2));assert(t->controlLfoPool[7].v[4]==.31f&&t->controlLfoPool[7].engine==2);
 assert(!t->assignModulation(1,7,grain->id,0,1,2));assert(!t->assignModulation(1,7,grain->id,int(dest),1,7));
 t->clipRecording=true;assert(!t->assignModulation(1,6,grain->id,0,0,2));t->clipRecording=false;
 // Source-less notes created in the browser illuminate their current keyboard
 // pitch without claiming ownership of a physical pad or another part.
 t->setPerformanceMode(0);setParam(t.get(),"algorithm","17");setParam(t.get(),"scale","1");setParam(t.get(),"root_note","62");
 const int played=t->padToMidiNote(68);t->noteOn(played,100,-2);renderSplit(*t,3);
 char pads[128]{};t->formatVoicePads(pads,sizeof(pads));assert(std::strtol(pads,nullptr,10)==0);assert(t->voices[0].sourcePad==-2);
 t->noteOff(played,-2);t->formatVoicePads(pads,sizeof(pads));assert(std::strtol(pads,nullptr,10)==-1);
 t->setPerformanceMode(1);t->selectSplitPart(1);setParam(t.get(),"algorithm","17");const int bNote=t->padToMidiNote(72);t->midiPart=1;t->noteOn(bNote,100,-2);renderSplit(*t,3);t->midiPart=-1;
 t->formatVoicePads(pads,sizeof(pads));assert(std::string(pads).find(",4")!=std::string::npos);
 t->allNotesOff();renderSplit(*t,200);for(const auto& voice:t->voices)assert(!voice.active);

 // Exercise the real input path and save the rolling window through the normal
 // archive and sample loader. Stereo polarity must survive both operations.
 std::array<std::int16_t,kBlockFrames*2> adc{};
 for(std::size_t i=0;i<kBlockFrames;++i){adc[2*i]=std::int16_t(8000*std::sin(i*.1));adc[2*i+1]=-adc[2*i]/2;}
 host_api_v1_t host{};host.sample_rate=44100;host.frames_per_block=kBlockFrames;host.mapped_memory=reinterpret_cast<std::uint8_t*>(adc.data());const auto* priorHost=g_host;g_host=&host;
 t->allNotesOff();t->setPerformanceMode(0);setParam(t.get(),"algorithm","7");setParam(t.get(),"live_source","1");setParam(t.get(),"live_freeze","0");setParam(t.get(),"live_window","200");setParam(t.get(),"live_lookback","25");
 t->padNoteOn(68,100);renderSplit(*t,250);const auto input=t->liveMemories[0].view(t->controls.sourceMotion,t->sampleRate);assert(input.anchor>=0);
 long double inputEnergy=0;for(int i=50;i<200;++i){const auto x=input.sample(0,input.anchor+i);inputEnergy+=x*x;assert(std::abs(input.sample(1,input.anchor+i)+x*.5f)<.0001f);}assert(inputEnergy>.01);

 // Exercise the real input/voice allocator at the full eight-note budget.
 for(int algorithm:{2,3,6,7,11,14}) {
   t->allNotesOff();renderSplit(*t,120);setParam(t.get(),"algorithm",std::to_string(algorithm).c_str());
   for(int pad=68;pad<76;++pad)t->padNoteOn(pad,100);
   energy=0;
   for(int block=0;block<240;++block){t->render(out.data(),128);if(block>100)for(auto x:out)energy+=double(x)*x;}
   assert(energy>1);
   assert(std::count_if(t->voices.begin(),t->voices.end(),[](const auto& voice){return voice.active&&voice.held;})==8);
   for(int pad=68;pad<76;++pad)t->padNoteOff(pad);renderSplit(*t,300);
   assert(std::none_of(t->voices.begin(),t->voices.end(),[](const auto& voice){return voice.active;}));
 }
 setParam(t.get(),"algorithm","7");t->padNoteOn(68,100);
 // Native coherent fast state must carry every live part within its bound.
 char fast[32768]{};assert(getParamValue(t.get(),"ui_poll_fast",fast,sizeof(fast))>0);
 assert(s3g_thru_json::parse(fast).at("values").at("source_motion_state").isString());
 t->backgroundStarted=true;t->beginLiveCapture();assert(t->liveCaptureJob.load()==1);const auto saved=t->liveCapturePath;
 const auto writing=t->liveMemories[0].written();renderSplit(*t,20);assert(t->liveMemories[0].written()==writing);
 t->selectClip((t->activeClip+1)%64);assert(t->activeClip==t->liveCaptureClip);
 assert(t->processLiveCapture()&&t->liveCaptureJob.load()==3);assert(std::filesystem::exists(saved.data()));
 t->serviceLiveCapture();assert(t->liveCaptureJob.load()==5);
 for(int retry=0;retry<3000&&t->liveCaptureJob.load();++retry){renderSplit(*t,1);usleep(500);}
 assert(!t->liveCaptureJob.load());assert(t->controls.sourceMotion.v[8]==0);assert(std::string(t->currentLaneSourcePaths[t->liveCaptureLane].data())==saved.data());
 assert(t->engine.laneFrames(t->liveCaptureLane)>8000);
 // Capture failures leave the frozen memory audible and do not point FILE at
 // a sample that was never published.
 setParam(t.get(),"live_source","1");setParam(t.get(),"live_freeze","0");renderSplit(*t,100);t->beginLiveCapture();assert(t->liveCaptureJob.load()==1);std::snprintf(t->liveCapturePath.data(),t->liveCapturePath.size(),"/dev/null/unwritable.wav");assert(t->processLiveCapture());t->serviceLiveCapture();assert(t->liveCaptureJob.load()==0&&t->controls.sourceMotion.v[8]==1&&t->controls.sourceMotion.v[11]==1);t->backgroundStarted=false;t->allNotesOff();renderSplit(*t,200);g_host=priorHost;

 t->setPerformanceMode(3);
 for(int part=0;part<4;++part){t->selectSplitPart(part);setParam(t.get(),"algorithm","2");setParam(t.get(),"live_source","1");}
 char largest[8192]{};assert(getParamValue(t.get(),"ui_poll_fast",largest,sizeof(largest))>0);
 const auto payload=s3g_thru_json::parse(largest);
 const auto sourcePacket=s3g_thru_json::parse(payload.at("values").at("source_motion_state").string());
 assert(sourcePacket.at("parts").arrayValue().size()==4);
 for(const auto& part:sourcePacket.at("parts").arrayValue())assert(part.at("envelope").string().size()==480);
 std::printf("Four-part compact live envelope packet: %zu / %zu bytes PASS\n",std::strlen(largest),sizeof(largest));
 restored.reset();t.reset();std::filesystem::remove_all(folder);
 std::puts("Source motion: independent travel stages, stereo rolling buffer/wrap/freeze, synth A to empty-source granular B, feedback guard and source-state recall: PASS");
}
