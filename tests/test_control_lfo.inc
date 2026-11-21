void lfoAssign(ThruInstance &t, int slot, int part, const char *key,
               float depth = .25f) {
  const auto set = [&](const char *field, float v) {
    const auto k = "lfo" + std::to_string(slot + 1) + "_" + field;
    setParam(&t, k.c_str(), std::to_string(v).c_str());
  };
  set("target", float(part));
  set("clock", 1);
  set("rate", 1.f / 256);
  set("wave", 4);
  set("depth", depth);
  set("dest", float(t.controlLfoDestinationForKey(key)));
  assert(int(t.controlLfoPool[slot].v[0]) ==
         t.controlLfoDestinationForKey(key));
}
void testControlLfoMath() {
  using namespace s3g_thru_lfo;
  Config c;
  c.v[destination] = 1;
  c.v[depth] = .75f;
  c.v[phase] = .25f;
  Runtime r;
  r.advance(c, 128, 44100, 120, 1, false);
  r.route(c, true, -1);
  assert(r.initialized && r.position > .25 && r.output > 0);
  set(c, s3g_thru_lfo::clock, 1);
  set(c, rate, 1);
  set(c, mode, transport);
  r.reset(c);
  r.advance(c, 4410, 44100, 120, 1, false);
  assert(r.position == .25);
  r.advance(c, 4410, 44100, 120, 1, true);
  assert(std::abs(r.position - .35) < 1e-7);
  set(c, mode, noteRestart);
  r.trigger(c);
  assert(r.position == .25);
  r.advance(c, 4410, 44100, 120, 1, false);
  r.trigger(c, false);
  assert(r.position > .34);
  r.trigger(c);
  assert(r.position == .25);
  set(c, mode, oneShot);
  r.reset(c);
  assert(!r.running);
  r.trigger(c);
  r.advance(c, 44100, 44100, 120, 1, false);
  assert(!r.running && r.raw(c) == 0);
  c.captured = c.runningAnchor = true;
  c.anchor = .37;
  c.cycle = 123;
  c.outputAnchor = -.6f;
  c.poolRoutes[0] = {{0, -1, 1}, -.45f, -.45f};
  r.reset(c);
  assert(r.running && r.position == .37 && r.routes[0].value == -.45f);
  c.v[mode] = freeRun;
  c.v[flow] = 1;
  r.reset(c);
  r.advance(c, 4410, 44100, 120, .5, true);
  assert(std::abs(r.position - .47) < 1e-7);
  c.v[flow] = 0;
  r.reset(c);
  r.advance(c, 4410, 44100, 120, .5, true);
  assert(std::abs(r.position - .42) < 1e-7);
  c.captured = false;
  c.v[wave] = 4;
  c.v[phase] = .1f;
  c.v[smooth] = 10;
  r.reset(c);
  for (int i = 0; i < 100; ++i) {
    r.advance(c, 128, 44100, 120, 1, false);
    r.route(c, true, -1);
  }
  const auto old = r.routes[0].value;
  c.target = 1;
  r.advance(c, 128, 44100, 120, 1, false);
  r.route(c, true, -1);
  assert(r.routes[0].from == old && r.routes[0].value > 0 &&
         r.routes[0].value < old);
  bool newRoute = false;
  for (const auto &cell : r.routes)
    newRoute |= cell.id.part == 1 && cell.value > 0;
  assert(newRoute);
  for (int shape = 0; shape < 7; ++shape)
    for (int i = 0; i < 1000; ++i) {
      const auto x = value(shape, i / 1000., 199, 0xffffffffu);
      assert(std::isfinite(x) && x >= -1.0001 && x <= 1.0001 &&
             x == value(shape, i / 1000., 199, 0xffffffffu));
    }
  c = {};
  for (int i = 0; i < int(beats.size()); ++i) {
    set(c, rate, 1 / beats[i]);
    assert(beatIndex(c.v[rate]) == i);
  }
  assert(!set(c, rate, NAN));
  c.v[mode] = noteRestart;
  c.v[phase] = 1;
  c.v[wave] = 4;
  c.v[depth] = .5f;
  r.trigger(c);
  assert(r.position == 0 && r.raw(c) > 0);
  std::puts("Control LFO math: tempo/Hz, four run modes, deterministic random, "
            "captured one-shot/routes, phase wrap and smooth cross-part "
            "reassignment: PASS");
}
void testControlLfoPool() {
  using namespace s3g_thru_lfo;
  auto t = splitFixture();
  t->stopBackgroundWork();
  t->setPerformanceMode(0);
  t->controls.algorithm = Engine::PlaybackAlgorithm::sample;
  for (int i = 0; i < 8; ++i)
    lfoAssign(*t, i, 0, "speed", .1f);
  t->noteOn(60, 100);
  renderSplit(*t, 100);
  assert(t->effectiveControls(t->voices[0]).speed > 2.5f &&
         t->controls.speed == 1);
  for (int mode = 0; mode < 7; ++mode) {
    t->setPerformanceMode(mode);
    assert(t->controlLfoPool[7].target == 0 && t->controlLfoPool[7].v[0] == 1);
    for (int p = 0; p < t->controlLfoPartCount(); ++p) {
      t->selectSplitPart(p, false);
      for (int alg = 0; alg < 15; ++alg) {
        t->controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(alg);
        t->voiceEngineSettings = {};
        for (auto &v : t->voices)
          v.active = v.held = false;
        Config c;
        c.target = p;
        assert(t->controlLfoDestinationAvailable(
                   c, t->controlLfoDestinationForKey("speed")) ==
               !(mode == 4 && p == 1));
        assert(t->controlLfoDestinationAvailable(
                   c, t->controlLfoDestinationForKey("grain_size")) ==
               (!(mode == 4 && p == 1) && alg == 2));
      }
    }
  }
  t->setPerformanceMode(1);
  for (int i = 0; i < 8; ++i) {
    const auto key = "lfo" + std::to_string(i + 1) + "_dest";
    setParam(t.get(), key.c_str(), "0");
  }
  lfoAssign(*t, 7, 1, "part_pan", .5f);
  renderSplit(*t, 100);
  assert(t->controlLfoPan(1, 0) > .49f && t->controlLfoPan(0, 0) == 0 &&
         t->sequenceLive.mix.strips[1].pan == 0);
  const auto prior = t->controls.effectReverbMix;
  lfoAssign(*t, 0, 4, "effect_reverb_mix", -.4f);
  renderSplit(*t, 100);
  auto fx = t->controls;
  t->applyControlLfoControls(fx, 4, Domain::shared);
  assert(fx.effectReverbMix < prior - .19f &&
         t->controls.effectReverbMix == prior);
  t->selectSplitPart(1, false);
  t->controls.algorithm = Engine::PlaybackAlgorithm::grains;
  lfoAssign(*t, 1, 1, "grain_size", .5f);
  renderSplit(*t, 100);
  auto grains = t->controls;
  t->applyControlLfoControls(grains, 1, Domain::engine);
  assert(grains.grainSizeMs > t->controls.grainSizeMs * 1.9f);
  // The DSP and menu observation read the same units; small buffers reject
  // truncation.
  std::array<char, 4096> state{}, live{};
  assert(getParamValue(t.get(), "lfo_state", state.data(), state.size()) > 0 &&
         std::strstr(state.data(), "\"selected\""));
  assert(getParamValue(t.get(), "lfo_live", live.data(), live.size()) > 0 &&
         std::strstr(live.data(), "\"grain_size\",180,90"));
  char small[8]{};
  assert(getParamValue(t.get(), "lfo_live", small, 8) == -1 && small[0] == 0);
  if (const char *fixture = std::getenv("S3G_THRU_LFO_FIXTURE")) {
    auto *file = std::fopen(fixture, "wb");
    assert(file);
    std::fprintf(file, "{\"state\":%s,\"live\":%s}", state.data(), live.data());
    assert(std::fclose(file) == 0);
  }
  // Report the exact playback base rather than the inactive part snapshot.
  t->setPerformanceMode(0);
  t->clipPlaying = true;
  t->clipControls.speed = 1.75f;
  t->splitClipControls[0].speed = .5f;
  assert(t->controlLfoManualBase(0, t->controlLfoDestinationForKey("speed")) ==
         1.75f);
  t->setPerformanceMode(1);
  t->clipPlaying = true;
  t->clipControls.splitBlend = .1f;
  t->controls.splitBlend = .7f;
  t->splitMixerOverrides[static_cast<std::size_t>(LockParameter::splitBlend)] =
      true;
  assert(t->controlLfoManualBase(
             4, t->controlLfoDestinationForKey("split_blend")) == .7f);
  t->clipPlaying = false;
  t->selectSplitPart(1, false);
  t->controls.algorithm = Engine::PlaybackAlgorithm::grains;
  // Explicit engine filtering leaves other Engine List voices and their display
  // untouched.
  lfoAssign(*t, 2, 1, "speed", .25f);
  setParam(t.get(), "lfo3_engine", "2");
  renderSplit(*t, 100);
  auto sample = grains;
  sample.algorithm = Engine::PlaybackAlgorithm::sample;
  sample.speed = 1;
  t->applyControlLfoControls(sample, 1, Domain::engine);
  assert(sample.speed == 1);
  for (const char *key : {"manual_lane", "lane_cycles", "algorithm",
                          "grain_density", "grain_regions"})
    assert(t->controlLfoDestinationForKey(key) == 0);
  std::puts("Control LFO pool: eight on one part, seven modes/15 engines, "
            "actual speed/level/pan/inserts/sends, preserved base values, "
            "full-name metadata and engine filtering: PASS");
}
void testControlLfoRecording() {
  using namespace s3g_thru_lfo;
  auto t = sequenceFixture();
  t->selectSplitPart(0, false);
  lfoAssign(*t, 7, 0, "speed", .3f);
  setParam(t.get(), "lfo8_mode", "2");
  setParam(t.get(), "lfo8_phase", "0.1");
  setParam(t.get(), "seq_step", "1");
  setParam(t.get(), "seq_lfo8_rate", "2");
  setParam(t.get(), "seq_lfo8_depth", "0.4");
  assert(t->sequenceLive.tracks[0].trigs[0].lfos.mask == (3u << 14));
  t->sequencePlayback = t->sequenceLive;
  t->splitClipControls[0] = t->controls;
  t->startSequenceVoice(0, 0, 0, .25, true);
  assert(t->controlLfoRuntime[7].overrides == 3);
  t->advanceControlLfos(128);
  const auto phase = t->controlLfoRuntime[7].position;
  t->startSequenceVoice(0, 0, 1, .25, true);
  assert(t->controlLfoRuntime[7].position == phase);
  setParam(t.get(), "seq_lfo8_rate", "reset");
  assert(t->sequenceLive.tracks[0].trigs[0].lfos.mask == (2u << 14));
  setParam(t.get(), "seq_lfo_clear", "1");
  assert(t->sequenceLive.tracks[0].trigs[0].lfos.mask == 0);
  t->setPerformanceMode(0);
  setParam(t.get(), "lfo8_mode", "0");
  setParam(t.get(), "lfo8_wave", "6");
  renderSplit(*t, 20);
  const auto anchor = t->controlLfoRuntime[7];
  t->beginClipRecording();
  assert(t->controlLfoPool[7].captured &&
         t->controlLfoPool[7].anchor == anchor.position &&
         t->controlLfoPool[7].poolRoutes[0].value == anchor.routes[0].value);
  const auto points = t->activeClipSlot()->clipPointCount;
  renderSplit(*t, 200);
  assert(t->activeClipSlot()->clipPointCount == points);
  setParam(t.get(), "lfo8_flow", "1");
  setParam(t.get(), "lfo8_flow", "0");
  assert(t->activeClipSlot()->clipPointCount == points);
  setParam(t.get(), "lfo8_depth", "0.7");
  assert(t->activeClipSlot()->clipPointCount == points + 1);
  renderSplit(*t, 20);
  t->finishClipRecording();
  t->resetControlLfos(true);
  assert(t->controlLfoRuntime[7].position == anchor.position &&
         t->controlLfoRuntime[7].cycle == anchor.cycle &&
         t->controlLfoRuntime[7].output == anchor.output);
  setParam(t.get(), "lfo8_flow", "1");
  t->advanceControlLfos(128);
  const auto live = t->controlLfoRuntime[7].position;
  t->resetControlLfos(true);
  assert(t->controlLfoRuntime[7].position == live);
  t->setPerformanceMode(6);
  setParam(t.get(), "lfo8_flow", "0");
  t->clipPlaying = false;
  t->beginClipRecording();
  renderSplit(*t, 100);
  setParam(t.get(), "lfo8_depth", "0.3");
  renderSplit(*t, 50);
  t->finishClipRecording();
  const auto curve = kClipPooledLfoOffset + 7 * 11 + s3g_thru_lfo::depth;
  assert(t->activeClipSlot()->clipPointCounts[curve] >= 2);
  setParam(t.get(), "lfo8_depth", "0.8");
  assert(t->activeClipSlot()->clipPointCounts[curve] == 0);
  // New part and engine assignments are recorded as settings, without changing
  // focus.
  t->selectSplitPart(2, false);
  nrpn(*t, 16, "lfo8_target",
       s3g_thru_midi::encode(midiParameter("lfo8_target"), 1, 4));
  assert(t->splitFocus == 2 && t->controlLfoPool[7].target == 1);
  std::puts(
      "Control LFO clips: global recipe automation, captured "
      "phase/random/routes, static FLOW, no generated events, scoped triggers, "
      "slot-eight locks/Burst and Sequence manual takeover: PASS");
}
void testControlLfoPersistence() {
  using namespace s3g_thru_lfo;
  auto t = splitFixture();
  t->stopBackgroundWork();
  t->setPerformanceMode(0);
  for (int i = 0; i < 8; ++i)
    lfoAssign(*t, i, 0, "speed", .05f * (i + 1));
  t->noteOn(60, 100);
  renderSplit(*t, 40);
  t->beginClipRecording();
  renderSplit(*t, 100);
  t->finishClipRecording();
  t->setClipPlaying(false);
  t->sequenceLive.tracks[1].trigs[7].lfos.mask = 65535;
  t->sequenceLive.tracks[1].trigs[7].lfos.depths[7] = -.6f;
  t->sequenceLive.tracks[1].generator.base[7].lfos =
      t->sequenceLive.tracks[1].trigs[7].lfos;
  t->captureActiveClip(true);
  auto json = std::make_unique<std::array<char, kStateTextSize>>();
  assert(t->serializeState(json->data(), json->size()) > 0);
  auto copy = splitFixture();
  copy->stopBackgroundWork();
  assert(copy->restoreState(json->data()));
  for (int i = 0; i < 8; ++i) {
    assert(copy->controlLfoPool[i].v == t->controlLfoPool[i].v &&
           copy->controlLfoPool[i].target == 0);
    assert(copy->controlLfoPool[i].poolRoutes[0].value ==
           t->controlLfoPool[i].poolRoutes[0].value);
  }
  char directory[] = "/tmp/thru-lfo-pool-state.XXXXXX";
  assert(mkdtemp(directory));
  std::snprintf(t->moduleDirectory.data(), t->moduleDirectory.size(), "%s",
                directory);
  t->persistenceEnabled = true;
  t->persistenceGeneration = 1;
  assert(t->writeClipFile());
  const auto path = std::string(directory) + "/thru-clips.bin";
  assert(copy->readClipFileFromPath(path.c_str()));
  assert(
      copy->controlLfoPool[7].v == t->controlLfoPool[7].v &&
      copy->clipSlots[0].sequence.tracks[1].trigs[7].lfos.mask == 65535 &&
      copy->clipSlots[0].sequence.tracks[1].generator.base[7].lfos.depths[7] ==
          -.6f);
  // A genuine legacy extension is preserved by the writer; strip only the new
  // tail/points.
  t->controlLfoPool = {};
  t->legacyControlLfos[0].v[0] = 4;
  t->legacyControlLfos[0].v[4] = .5f;
  t->captureActiveClip(true);
  assert(t->writeClipFile());
  const auto read = [&]() {
    std::ifstream f(path, std::ios::binary);
    std::vector<unsigned char> b((std::istreambuf_iterator<char>(f)), {});
    b.resize(b.size() - 4);
    return b;
  };
  auto old = read();
  stripPooledLfos(old);
  assert(old[8] == 47);
  const auto write = [&]() {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char *>(old.data()), old.size());
    f.close();
    assert(appendFileChecksum(path.c_str()));
  };
  write();
  assert(copy->readClipFileFromPath(path.c_str()));
  assert(copy->controlLfoPool[0].v[0] ==
             copy->controlLfoDestinationForKey("speed") &&
         copy->controlLfoPool[0].v[4] == .25f);
  stripControlLfos(old);
  assert(old[8] == 46);
  write();
  assert(copy->readClipFileFromPath(path.c_str()));
  for (const auto &c : copy->controlLfoPool)
    assert(c.v[0] == 0);
  t.reset();
  copy.reset();
  std::filesystem::remove_all(directory);
  std::puts("Control LFO storage: eight shared assignments, all-route "
            "JSON/format-48 capture, 16-bit trig locks, generator bases, real "
            "format-47 migration and disabled format-46 recall: PASS");
}
#include "test_control_lfo_auto.inc"
#include "test_control_lfo_windows.inc"
void testControlLfos() {
  testControlLfoAuto();
  testLfoEngineWindows();
  testLfoSampleWindowAudio();
  testLfoSequenceWindowStarts();
  testControlLfoMath();
  testControlLfoPool();
  testControlLfoRecording();
  testControlLfoPersistence();
}
