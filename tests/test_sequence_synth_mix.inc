// A Sequence voice owns one patch for its note lifetime. Compare its bounded
// path against the original four-patch renderer, including live sound edits.
void testFixedLaneSynthEquivalence() {
  namespace f = s3g_thru_fm;
  namespace r = s3g_thru_reson;
  for (float rate : {44100.f, 48000.f}) {
    for (int kind = 0; kind < 8; ++kind) {
      for (int lane = 0; lane < 4; ++lane) {
        f::Program fm;
        r::Program reson;
        for (int p = 0; p < 4; ++p) {
          fm.patches[p].v[f::mode] = kind / 4;
          fm.patches[p].v[f::algorithm] = kind % 4;
          fm.patches[p].v[f::index2] = 3 + p;
          fm.patches[p].v[f::harm] = p & 1 ? 40 : -35;
          reson.patches[p] = r::startingSound(kind + 1, reson.patches[p]);
        }
        f::Instrument fullFm, fixedFm;
        r::Instrument fullReson, fixedReson;
        fullFm.prepare(); fixedFm.prepare();
        fullReson.prepare(); fixedReson.prepare();
        fullFm.identity = fixedFm.identity = 81;
        fullReson.identity = fixedReson.identity = 93;
        fixedFm.setPatchRange(lane, lane);
        fixedReson.setPatchRange(lane, lane);
        for (int frame = 0; frame < 2048; ++frame) {
          if (frame % 64 == 0) {
            fm.patches[lane].v[f::index1] = 1 + frame * .001f;
            reson.patches[lane].v[r::brightness] = 30 + frame * .02f;
            fullFm.beginBlock(fm.patches); fixedFm.beginBlock(fm.patches);
            fullReson.beginBlock(reson.patches); fixedReson.beginBlock(reson.patches);
          }
          const float hz = 90 + frame * .01f;
          assert(fullFm.tick(fm.patches, float(lane), hz, rate) ==
                 fixedFm.tick(fm.patches, float(lane), hz, rate));
          assert(fullReson.tick(reson.patches, float(lane), hz, rate) ==
                 fixedReson.tick(reson.patches, float(lane), hz, rate));
        }
        int fmPrepared = 0, resonPrepared = 0;
        for (int p = 0; p < 4; ++p) {
          fmPrepared += fixedFm.models[p].preparedRate != 0;
          resonPrepared += fixedReson.models[p].srPrepared != 0;
          if (p != lane) {
            assert(fixedFm.laneOutput[p] == 0);
            assert(fixedReson.laneOutput[p] == 0);
          }
        }
        assert(fmPrepared == 1 && resonPrepared == 1);
        // A mode/owner change resets the plan before another note starts.
        fullFm.reset(); fixedFm.reset();
        fullReson.reset(); fixedReson.reset();
        assert(fixedFm.firstPatch == 0 && fixedFm.lastPatch == 3);
        assert(fixedReson.firstPatch == 0 && fixedReson.lastPatch == 3);
        for (int frame = 0; frame < 512; ++frame) {
          const float position = 1.5f + .4f * std::sin(frame * .004f);
          assert(fullFm.tick(fm.patches, position, 110, rate) ==
                 fixedFm.tick(fm.patches, position, 110, rate));
          assert(fullReson.tick(reson.patches, position, 110, rate) ==
                 fixedReson.tick(reson.patches, position, 110, rate));
        }
        f::Instrument splitFm;
        r::Instrument splitReson;
        splitFm.prepare(); splitReson.prepare();
        splitFm.identity = fullFm.identity;
        splitReson.identity = fullReson.identity;
        fullFm.reset(); fullReson.reset();
        const int first = lane < 2 ? 0 : 2;
        splitFm.setPatchRange(first, first + 1);
        splitReson.setPatchRange(first, first + 1);
        for (int frame = 0; frame < 512; ++frame) {
          const float position = first + .5f + .49f * std::sin(frame * .01f);
          assert(fullFm.tick(fm.patches, position, 110, rate) ==
                 splitFm.tick(fm.patches, position, 110, rate));
          assert(fullReson.tick(reson.patches, position, 110, rate) ==
                 splitReson.tick(reson.patches, position, 110, rate));
        }
        for (int p = 0; p < 4; ++p) {
          const bool owned = p >= first && p <= first + 1;
          assert((splitFm.models[p].preparedRate != 0) == owned);
          assert((splitReson.models[p].srPrepared != 0) == owned);
        }
      }
    }
  }
  std::puts("Fixed-lane FM/RESON: exact full-stack audio, live edits, all routes/bodies, two rates, one/two owned patches and full-scan reset: PASS");
}

void testSequenceMixedSynths() {
  const std::array<std::array<int, 4>, 4> algorithms{{
      {{17, 17, 19, 19}}, // User setup: FM, FM, Concert Bass, Snare.
      {{15, 16, 17, 19}}, // Spline, Scanned, FM, Reson.
      {{18, 20, 21, 19}}, // Noise, Partials, Lowform, Reson.
      {{4, 2, 17, 19}},   // Sample, Grains, FM, Reson, with real lane audio.
  }};
  for (int scenario = 0; scenario < int(algorithms.size()); ++scenario) {
    for (int lookahead : {0, 1}) {
      auto t = splitFixture(lookahead == 0);
      t->stopBackgroundWork();
      t->setPerformanceMode(6);
      for (int part = 0; part < 4; ++part) {
        t->selectSplitPart(part, false);
        char text[32]; std::snprintf(text, sizeof(text), "%d", algorithms[scenario][part]);
        setParam(t.get(), "algorithm", text);
        auto &c = t->controls;
        c.ampAttackMs = 3; c.ampReleaseMs = 20;
        c.effectDrive = c.effectMix = c.effectReverbMix = c.effectTapeMix = 0;
        for (auto &patch : c.fm.patches) {
          patch = {};
          patch.v[s3g_thru_fm::algorithm] = part % 4;
          patch.v[s3g_thru_fm::index2] = 3;
        }
        for (auto &patch : c.reson.patches)
          patch = s3g_thru_reson::startingSound(part % 2 ? 3 : 8, patch);
        for (auto &patch : c.table.patches) patch.v[s3g_thru_table::type] = 1;
        for (auto &patch : c.scanned.patches) patch.v[s3g_thru_scanned::art] = 1;
        for (auto &patch : c.lowform.patches)
          patch = s3g_thru_lowform::startingSound(2, patch);
        auto &lane = t->sequenceLive.tracks[part];
        lane.enabled = true; lane.length = 7 + part * 2;
        lane.rate = scenario == 0 ? 4 : 3 + part;
        lane.timeMode = true; lane.timeMs = 350;
        for (int step = 0; step < 32; ++step) {
          auto &trig = lane.trigs[step];
          trig.enabled = true;
          trig.locks.mask = 2;
          trig.locks.values[1] = float((step + part) % 7);
          if (step == 3) s3g_thru_sequence::setCount(trig, 3);
        }
      }
      t->selectSplitPart(0, false);
      t->sequenceLive.soundLocks.set(0, 0, 0, midiParameter("fm_index1").id, 4);
      t->sequenceLive.soundLocks.set(3, 1, 0, midiParameter("reson_decay").id, 170);
      t->ensureSequenceClip(); t->captureActiveClip(false); t->startClipPlaybackNow();
      std::array<std::int16_t, kBlockFrames * 2> out{};
      std::array<double, 4> energy{};
      int maxVoices = 0;
      for (int block = 0; block < 900; ++block) {
        t->render(out.data(), kBlockFrames);
        int voices = 0;
        for (auto &v : t->voices) {
          if (!v.active) continue;
          ++voices;
          assert(v.part >= 0 && v.part < 4 && v.sequence.active);
          assert(int(t->effectiveControls(v).algorithm) == algorithms[scenario][v.part]);
          assert(t->effectiveControls(v).stackFirstLane == v.part);
          assert(t->effectiveControls(v).stackLastLane == v.part);
        }
        maxVoices = std::max(maxVoices, voices);
        for (const auto &channel : t->voiceMix)
          for (float x : channel) assert(std::isfinite(x));
        for (auto x : out) assert(std::abs(int(x)) < 32767);
        for (int part = 0; part < 4; ++part)
          for (float x : t->splitBus[part][0]) energy[part] += double(x) * x;
        if (block == 400) {
          t->selectSplitPart(2, false);
          nrpn(*t, 5, "fm_index1", 6000);
          nrpn(*t, 8, "reson_decay", 8000);
          assert(t->splitFocus == 2);
        }
      }
      assert(maxVoices == 8);
      for (double e : energy) assert(e > .001);
      t->setClipPlaying(false);
      for (int block = 0; block < 600; ++block) t->render(out.data(), kBlockFrames);
      for (auto &v : t->voices) assert(!v.active);
      // Existing mode transitions clear fixed-lane readers before changing ownership.
      for (int mode : {2, 3, 0, 6}) {
        t->setPerformanceMode(mode);
        for (auto &v : t->voices) assert(!v.active);
      }
    }
  }
  std::puts("Mixed Sequence: FM/FM/CBASS/SNARE, all seven synths and sample engines, eight voices, polymetric clocks, Burst/stealing, sound locks, MIDI, releases and 0/12ms lookahead: PASS");
}

// Compare the part-owned plan with the full-stack renderer. Reachable but
// inaudible patches must keep their phase/envelopes for later scan re-entry.
template<class Instrument, class Program, class Setup, class Change, class Check>
void compareSynthPatchPlans(int variants, Setup setup, Change change, Check check) {
  const std::array<std::array<int, 2>, 7> ranges{{
    {{0,0}}, {{1,1}}, {{2,2}}, {{3,3}}, {{0,1}}, {{2,3}}, {{0,3}}
  }};
  for (float sr : {44100.f, 48000.f}) for (int variant=0;variant<variants;++variant) {
    for (const auto &range : ranges) {
      Program program;
      setup(program, variant);
      Instrument full, owned;
      full.prepare(); owned.prepare();
      full.identity = owned.identity = 53;
      owned.setPatchRange(range[0], range[1]);
      const int frames = range[0] == range[1] ? 1024 : 4096;
      double energy = 0;
      for (int frame=0;frame<frames;++frame) {
        if (frame%64==0) {
          change(program, frame);
          full.beginBlock(program.patches); owned.beginBlock(program.patches);
        }
        // Silent and audible re-entry while pitch and sound targets change.
        std::array<float,4> gains{{1,1,1,1}};
        if (frame>=300 && frame<600) gains[range[0]]=0;
        const float position=range[0]+(range[1]-range[0])*(.5f+.5f*std::sin(frame*.015f));
        const float hz=100+frame*.005f;
        const auto a=full.tick(program.patches,position,hz,sr,5,gains.data());
        const auto b=owned.tick(program.patches,position,hz,sr,5,gains.data());
        assert(a==b);
        for (float x:b) {assert(std::isfinite(x));energy+=double(x)*x;}
        for(int p=0;p<4;++p) if(p<range[0]||p>range[1]) assert(owned.laneOutput[p]==0);
      }
      assert(energy>1e-9);
      check(owned,range[0],range[1]);
      // Mid-note changes cannot silently rebind ownership.
      owned.setPatchRange(0,3);
      assert(owned.firstPatch==range[0]&&owned.lastPatch==range[1]);
      full.reset();owned.reset();
      assert(owned.firstPatch==0&&owned.lastPatch==3);
      for(int frame=0;frame<1024;++frame) {
        if(frame%64==0){full.beginBlock(program.patches);owned.beginBlock(program.patches);}
        const float position=1.5f+1.49f*std::sin(frame*.01f);
        assert(full.tick(program.patches,position,110,sr)==owned.tick(program.patches,position,110,sr));
      }
      for(const auto &bad:std::array<std::array<int,2>,4>{{{{-1,0}},{{0,4}},{{3,2}},{{4,4}}}}) {
        owned.reset();owned.setPatchRange(bad[0],bad[1]);
        assert(owned.firstPatch==0&&owned.lastPatch==3);
      }
    }
  }
}

void testRemainingSynthPatchPlans() {
  compareSynthPatchPlans<s3g_thru_table::Instrument,s3g_thru_table::Program>(3,
    [](auto &p,int type){for(auto &c:p.patches){c.v[s3g_thru_table::type]=float(type);c.v[s3g_thru_table::art]=type==2?1:0;}},
    [](auto &p,int f){for(auto &c:p.patches)c.v[s3g_thru_table::blend]=20+f*.01f;},
    [](const auto &v,int first,int last){for(int lane:v.lanes)assert(lane<0||(lane>=first&&lane<=last));
      if(first==last)assert(v.voices[0].initialized&&!v.voices[1].initialized);});
  compareSynthPatchPlans<s3g_thru_scanned::Instrument,s3g_thru_scanned::Program>(2,
    [](auto &p,int art){for(auto &c:p.patches)c.v[s3g_thru_scanned::art]=float(art);},
    [](auto &p,int f){for(auto &c:p.patches)c.v[s3g_thru_scanned::brightness]=30+f*.01f;},
    [](const auto &v,int first,int last){for(int lane=0;lane<4;++lane)
      assert(v.models[lane].initialized==(lane>=first&&lane<=last));});
  compareSynthPatchPlans<s3g_thru_noise::Instrument,s3g_thru_noise::Program>(7,
    [](auto &p,int type){for(auto &c:p.patches){c.v[s3g_thru_noise::type]=float(type);c.v[s3g_thru_noise::rate]=400;}},
    [](auto &p,int f){for(auto &c:p.patches)c.v[s3g_thru_noise::tone]=1000+f;},
    [](const auto &v,int first,int last){for(int lane=0;lane<4;++lane)
      assert((v.models[lane].slew>0)==(lane>=first&&lane<=last));});
  compareSynthPatchPlans<s3g_thru_partials::Instrument,s3g_thru_partials::Program>(3,
    [](auto &p,int type){for(auto &c:p.patches){c.v[s3g_thru_partials::type]=float(type);c.v[s3g_thru_partials::count]=32;}},
    [](auto &p,int f){for(auto &c:p.patches)c.v[s3g_thru_partials::tilt]=-6+f*.001f;},
    [](const auto &v,int first,int last){for(int lane=0;lane<4;++lane)
      assert((v.models[lane].age>0)==(lane>=first&&lane<=last));});
  compareSynthPatchPlans<s3g_thru_lowform::Instrument,s3g_thru_lowform::Program>(6,
    [](auto &p,int type){for(auto &c:p.patches)c=s3g_thru_lowform::startingSound(type+1,c);},
    [](auto &p,int f){for(auto &c:p.patches)c.v[s3g_thru_lowform::cutoff]=1000+f;},
    [](const auto &v,int first,int last){for(int lane=0;lane<4;++lane)
      assert((v.models[lane].rate>0)==(lane>=first&&lane<=last));});
  std::puts("TABLE/SCANNED/NOISE/PARTIALS/LOWFORM patch plans: exact fixed/split/full-stack audio, aging/re-entry, live edits, ownership, reset and invalid-range fallback: PASS");
}

void testMixedSynthEffectsAndFades() {
  constexpr std::array<int,4> source{{15,16,17,19}},destination{{18,20,21,15}};
  constexpr std::array<const char*,4> srcDest{{"table_tune","scanned_force","fm_index1","reson_brightness"}},
    dstDest{{"noise_tone","partials_tilt","lowform_cutoff","table_tune"}};
  for(int lookahead:{0,1}) {
    auto t=splitFixture(lookahead==0);
    t->stopBackgroundWork();t->setPerformanceMode(6);
    t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
    for(int part=0;part<4;++part) {
      t->selectSplitPart(part,false);
      setParam(t.get(),"algorithm",std::to_string(source[part]).c_str());
      auto &c=t->controls;
      c.ampAttackMs=2;c.ampReleaseMs=3000;
      c.effectDrive=.1f;c.effectMix=.2f;c.effectReverbMix=.2f;
      c.effectTapeMix=.2f;c.effectRotaryMix=.2f;
      for(auto &patch:c.table.patches)patch.v[s3g_thru_table::type]=1;
      for(auto &patch:c.scanned.patches)patch.v[s3g_thru_scanned::art]=1;
      for(auto &patch:c.reson.patches)patch=s3g_thru_reson::startingSound(part%2?3:8,patch);
      auto &lane=t->sequenceLive.tracks[part];
      lane.enabled=true;lane.length=7+part*2;lane.rate=4+part%2;
      lane.timeMode=true;lane.timeMs=600;
      for(int step=0;step<32;++step){auto &trig=lane.trigs[step];trig.enabled=true;
        trig.locks.mask=2;trig.locks.values[1]=float((step+part)%7);
        if(step%4==0)s3g_thru_sequence::setCount(trig,3);}
      lfoAssign(*t,part,part,srcDest[part],.2f);
      const auto prefix="lfo"+std::to_string(part+1);
      setParam(t.get(),(prefix+"_wave").c_str(),"0");
      setParam(t.get(),(prefix+"_rate").c_str(),"3");
    }
    t->selectSplitPart(0,false);t->ensureSequenceClip();t->captureActiveClip(true);
    t->copyCurrentSourcesToClip(t->clipSlots[0]);t->clipSlots[1]=t->clipSlots[0];
    for(int part=0;part<4;++part) {
      auto &clip=t->clipSlots[1];
      clip.splitValues[part][static_cast<int>(LockParameter::algorithm)]=float(destination[part]);
      clip.lfos[part].v[s3g_thru_lfo::destination]=float(t->controlLfoDestinationForKey(dstDest[part]));
    }
    t->startClipPlaybackNow();
    std::array<std::int16_t,kBlockFrames*2> out{};
    int peakVoices=0;double energy=0;
    const auto render=[&](){
      waitForPatternAudio(*t);t->render(out.data(),kBlockFrames);
      int active=0;for(const auto &v:t->voices)if(v.active)++active;
      peakVoices=std::max(peakVoices,active);assert(active<=8);
      for(auto pcm:out){assert(std::abs(int(pcm))<32767);energy+=double(pcm)*pcm;}
      for(const auto &ch:t->voiceMix)for(float x:ch)assert(std::isfinite(x));
    };
    for(int block=0;block<350;++block)render();
    assert(peakVoices==8&&energy>100);
    // Repeated timed transitions exercise the real outgoing-scene producer.
    for(int target:{1,0,1}) {
      assert(t->prepareClipBank(target));
      t->clipPrepareState.store(int(ClipPrepareState::ready));t->patternFadeMs=200;
      assert(t->activatePreparedClip()&&t->patternActive);
      for(int block=0;block<300&&t->patternActive;++block)render();
      assert(!t->patternActive&&t->activeClip==target&&t->patternUnderflows==0);
      waitForPatternIdle(*t);
      for(int part=0;part<4;++part)
        assert(int(t->liveControlsForPart(part).algorithm)==(target?destination:source)[part]);
      for(int block=0;block<60;++block)render();
    }
    t->setClipPlaying(false);
    for(int block=0;block<1200;++block)render();
    for(const auto &v:t->voices)assert(!v.active);
  }
  std::puts("Mixed synth stress: eight voices, clustered Burst, 3s release settings, effects, moving engine LFOs, repeated cross-engine clip fades, output protection and 0/12ms lookahead: PASS");
}

void testSequenceSynthMix() {
  testFixedLaneSynthEquivalence();
  testRemainingSynthPatchPlans();
  testSequenceMixedSynths();
  testMixedSynthEffectsAndFades();
}
