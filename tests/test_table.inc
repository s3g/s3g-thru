// TABLE is a source-free voice: test the real module, not a stand-in renderer.
std::unique_ptr<ThruInstance> tableFixture(int mode = 0) {
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(nullptr));
  t->stopBackgroundWork();
  setParam(t.get(), "pattern_fade_lookahead", "0");
  t->setPerformanceMode(mode);
  for (int p = 0; p < (t->splitEnabled ? t->partCount() : 1); ++p) {
    if (t->splitEnabled)
      t->selectSplitPart(p, false);
    setParam(t.get(), "algorithm", "15");
    auto &c = t->controls;
    c.laneCycles = 0;
    c.lanePath = Engine::LanePath::manual;
    c.laneSlewMs = 5;
    c.ampAttackMs = 3;
    c.ampReleaseMs = 20;
    for (auto &patch : c.table.patches) {
      patch = {};
      patch.v[s3g_thru_table::width] = 0;
    }
  }
  if (t->splitEnabled)
    t->selectSplitPart(0, false);
  assert(t->engine.loadedLaneMask() == 0);
  return t;
}
float tableRender(ThruInstance &t, int blocks) {
  float peak = 0;
  std::array<std::int16_t, kBlockFrames * 2> out{};
  for (int b = 0; b < blocks; ++b) {
    t.render(out.data(), kBlockFrames);
    for (auto x : out)
      peak = std::max(peak, float(std::abs(int(x))));
  }
  return peak;
}
void testTableCore() {
  namespace tab = s3g_thru_table;
  tab::Instrument voice;
  voice.prepare();
  tab::Program program;
  for (auto &p : program.patches) {
    p = {};
    p.v[tab::width] = 0;
  }
  for (int variant = 0; variant < 3; ++variant) {
    auto &p = program.patches[0];
    p = {};
    p.v[tab::type] = float(variant);
    p.v[tab::width] = 0;
    if (variant == 2)
      tab::start(p, 0);
    for (float hz : {130.81278f, 261.625565f, 523.25113f}) {
      voice.reset(77);
      double energy = 0, monoError = 0;
      int crossings = 0;
      float last = 0;
      for (int i = 0; i < 12000; ++i) {
        const auto a = voice.tick(program.patches, 0, hz, 44100);
        assert(std::isfinite(a[0]) && std::isfinite(a[1]) &&
               std::abs(a[0]) < 2);
        if (i > 3000) {
          energy += a[0] * a[0];
          monoError += std::abs(a[0] - a[1]);
          if (variant == 0 && last <= 0 && a[0] > 0)
            ++crossings;
        }
        last = a[0];
      }
      assert(energy > .0001 && monoError < .001);
      if (variant == 0)
        assert(std::abs(crossings - hz * 8999 / 44100) < 2);
    }
  }
  // All control limits and random structures remain finite at supported rates.
  for (float rate : {44100.f, 48000.f, 96000.f})
    for (int variant = 0; variant < 3; ++variant) {
      for (auto &p : program.patches) {
        p = {};
        p.v[tab::type] = float(variant);
      }
      voice.reset(123);
      for (int i = 0; i < 4000; ++i) {
        if (i % 50 == 0) {
          const int f = (i / 50) % tab::fields;
          for (auto &p : program.patches)
            if (!tab::discrete(f))
              p.v[f] = (i / 100) % 2 ? tab::high[f] : tab::low[f];
        }
        const auto a = voice.tick(program.patches, float(i) / 1333, 1400, rate);
        for (float x : a)
          assert(std::isfinite(x) && std::abs(x) < 8);
      }
    }
  // Finite-precision slew must settle after manual edits and legato pitch.
  {
    tab::State state;
    tab::Config p;
    state.reset(p, 220, 44100, 1);
    p.v[tab::cutoff] = 12345;
    p.v[tab::tune] = 7;
    for (int n = 0; n < 12000; ++n)
      state.tick(tab::Library::get(), p, 440, 44100);
    assert(!state.smoothing && state.current.v[tab::cutoff] == 12345 &&
           state.current.v[tab::tune] == 7 && state.hz == 440);
  }
  // A smooth oscillator makes edit impulses measurable, unlike bright saw
  // harmonics.
  for (auto &p : program.patches) {
    p = {};
    p.v[tab::width] = 0;
  }
  voice.reset(19);
  float last = 0, maxStep = 0;
  for (int i = 0; i < 16000; ++i) {
    if (i == 3000)
      program.patches[0].v[tab::tune] = 12;
    if (i == 6000)
      program.patches[0].v[tab::level] = 25;
    if (i == 9000)
      program.patches[0].v[tab::shape] = 1;
    if (i == 9200)
      program.patches[0].v[tab::shape] = 0;
    const auto a = voice.tick(program.patches, 0, 130.81278f, 44100);
    if (i > 1000)
      maxStep = std::max(maxStep, std::abs(a[0] - last));
    last = a[0];
  }
  assert(maxStep < .05);
  // Seeded Spline is repeatable, while another seed changes its audio.
  std::array<float, 2048> reference{};
  for (auto &p : program.patches) {
    p = {};
    p.v[tab::type] = 1;
    p.v[tab::width] = 30;
  }
  voice.reset(44);
  for (auto &x : reference)
    x = voice.tick(program.patches, 0, 220, 44100)[0];
  voice.reset(44);
  for (float x : reference)
    assert(x == voice.tick(program.patches, 0, 220, 44100)[0]);
  program.patches[0].v[tab::seed] = 42;
  voice.reset(44);
  double difference = 0;
  for (float x : reference)
    difference += std::abs(x - voice.tick(program.patches, 0, 220, 44100)[0]);
  assert(difference > 1);
  std::puts(
      "TABLE core: keyboard pitch, three variants, mono compatibility, seeded "
      "Spline, live edit continuity and finite control extremes: PASS");
}
void testTablePreparedRendering() {
  namespace tab = s3g_thru_table;
  const auto &library = tab::Library::get();
  // Preparing a block must preserve all sample-time pitch, lane and smoothing
  // behavior, including a new reader entering a lane halfway through the block.
  for (float rate : {44100.f, 48000.f}) {
    tab::Instrument reference, prepared;
    reference.prepare(); prepared.prepare();
    reference.reset(456); prepared.reset(456);
    tab::Program program;
    for (int lane=0;lane<4;++lane) program.patches[lane].v[tab::type] = lane % 3;
    for (int block=0;block<96;++block) {
      if (block % 7 == 0) {
        auto &p = program.patches[(block / 7) % 4];
        p.v[tab::type] = (int(p.v[tab::type]) + 1) % 3;
        p.v[tab::shape] = (int(p.v[tab::shape]) + 1) % 4;
        p.v[tab::seed] = block + 1;
        p.v[tab::level] = 35 + block % 60;
        p.v[tab::cutoff] = 2500 + block * 137;
      }
      prepared.beginBlock(program.patches);
      const float hz = block < 40 ? 220 : 330;
      for (int frame=0;frame<128;++frame) {
        const float lane = float((block * 128 + frame) % 8192) * 3 / 8192;
        const auto a = reference.tick(program.patches,lane,hz,rate);
        const auto b = prepared.tick(program.patches,lane,hz,rate);
        assert(std::isfinite(b[0]) && std::isfinite(b[1]));
        assert(std::abs(a[0]-b[0]) < 2e-6f && std::abs(a[1]-b[1]) < 2e-6f);
      }
      prepared.endBlock();
    }
  }
  // A structural edit must really reach the running generator, with its old
  // reader fading out. A former target assignment silently consumed the edit.
  tab::Voice voice;
  tab::Config p;
  voice.tick(library,p,220,44100,9,0,0);
  for (int variant : {1,2,0}) {
    p.v[tab::type] = variant;
    p.v[tab::shape] = 3;
    voice.setTarget(p);
    assert(voice.fade > 0);
    voice.tick(library,p,220,44100,9,0,0,true);
    assert(voice.current.current.v[tab::type] == variant &&
           voice.current.current.v[tab::shape] == 3);
    for (int n=0;n<400;++n) voice.tick(library,p,220,44100,9,0,0,true);
    assert(voice.fade == 0);
  }
  // Independently exercise the actual anti-alias filter at the new internal
  // rate: its passband retains body and its stopband attenuates folded audio.
  const auto gain=[](float cycles) {
    tab::Decimator decimator;
    double sum=0;
    for(int n=0;n<16384;++n) {
      const float y=decimator.tick(std::sin(6.283185307179586*cycles*n));
      if(n>=4096)sum+=double(y)*y;
    }
    return std::sqrt(sum*2/12288);
  };
  assert(gain(.025f) > .99 && gain(.025f) < 1.01);
  assert(gain(.325f) < .02 && gain(.45f) < .0001);
  std::puts("TABLE prepared rendering: sample-equivalent block controls, live structural edits, cross-lane/pitch continuity and 2x anti-alias pass/stop bands: PASS");
}
void testTableLayoutsAndScopes() {
  namespace tab = s3g_thru_table;
  for (int mode = 0; mode < 7; ++mode) {
    auto t = tableFixture(mode);
    const int parts =
        mode == 0 ? 1
                  : mode == 3 || mode == 6 ? 4 : mode == 5 || mode == 4 ? 1 : 2;
    for (int p = 0; p < parts; ++p) {
      t->midiPart = p;
      t->noteOn(60 + p, 100);
    }
    assert(tableRender(*t, 50) > 100);
    for (const auto &v : t->voices)
      if (v.active) {
        assert(v.engine.normalizedPosition() == -1);
        const float lane = v.engine.normalizedLanePosition() * 3;
        const int first = t->splitEnabled && !t->sharedDual
                              ? t->firstLaneForPart(v.part)
                              : 0,
                  last = t->splitEnabled && !t->sharedDual
                             ? first + (t->isQuad() ? 0 : 1)
                             : 3;
        assert(lane >= first - .001 && lane <= last + .001);
      }
    char state[4096]{};
    assert(t->formatTableState(state, sizeof(state)) > 0);
    assert(t->formatTableScopes(state, sizeof(state)) > 0);
    assert(std::strstr(state, "\"waves\":[\"") && std::strlen(state) < 1024);
    t->allNotesOff();
    tableRender(*t, 100);
    char flat[129]{};
    for (int p = 0; p < parts; ++p)
      for (int lane = 0; lane < 4; ++lane) {
        t->tableScopes[p].encode(lane, t->sampleRate, 220, flat);
        assert(std::string(flat) == std::string(128, '7'));
      }
  }
  auto t = tableFixture();
  t->controls.laneCycles = 1;
  t->controls.lanePath = Engine::LanePath::up;
  t->noteOn(60, 100);
  tableRender(*t, 50);
  const auto before = t->voices[0].engine.normalizedLanePosition();
  setParam(t.get(), "record_lane", "4");
  setParam(t.get(), "table_shape", "2");
  const auto after = t->voices[0].engine.normalizedLanePosition();
  assert(after == before && t->controls.table.patches[0].v[tab::shape] == 0 &&
         t->controls.table.patches[3].v[tab::shape] == 2);
  tableRender(*t, 2);
  assert(t->voices[0].engine.normalizedLanePosition() != before);
  setParam(t.get(), "stack_scan_paused", "1");
  tableRender(*t, 5);
  const float held = t->voices[0].engine.normalizedLanePosition();
  tableRender(*t, 10);
  assert(std::abs(t->voices[0].engine.normalizedLanePosition() - held) < .001);
  // Eight notes, stealing and releases retain the existing note allocator.
  t = tableFixture();
  for (int i = 0; i < 8; ++i)
    t->noteOn(48 + i, 100);
  assert(tableRender(*t, 12) > 100);
  t->noteOn(75, 100);
  tableRender(*t, 8);
  int heldCount = 0;
  for (const auto &v : t->voices)
    heldCount += v.held;
  assert(heldCount == 8);
  t->allNotesOff();
  tableRender(*t, 100);
  for (const auto &v : t->voices)
    assert(!v.active);
  // Shared Dual rejects a stale part image; Bank always displays A audio.
  {
    auto pair = tableFixture(2);
    char packet[4096]{};
    assert(pair->formatTableScopes(packet, sizeof(packet)) > 0);
    assert(std::strstr(packet, "\"focus\":0"));
    pair->selectSplitPart(1, false);
    assert(pair->formatTableScopes(packet, sizeof(packet)) > 0);
    assert(std::strstr(packet, "\"focus\":1"));
  }
  {
    auto bank = tableFixture(4);
    bank->midiPart = 0;
    bank->noteOn(60, 100);
    tableRender(*bank, 20);
    char a[4096]{}, b[4096]{};
    assert(bank->formatTableScopes(a, sizeof(a)) > 0);
    bank->selectSplitPart(1, false);
    assert(bank->formatTableScopes(b, sizeof(b)) > 0);
    assert(!std::strcmp(a, b));
  }
  // Actual module scope packet is used by UI and documentation fixtures.
  t = tableFixture();
  t->controls.manualLane = 1.5f;
  t->controls.table.patches[1].v[tab::shape] = 1;
  t->controls.table.patches[2].v[tab::type] = 1;
  t->noteOn(60, 100);
  tableRender(*t, 60);
  if (const char *file = std::getenv("S3G_THRU_TABLE_FIXTURE")) {
    char state[4096]{}, scope[4096]{};
    assert(t->formatTableState(state, sizeof(state)) > 0 &&
           t->formatTableScopes(scope, sizeof(scope)) > 0);
    std::ofstream out(file);
    out << "{\"state\":" << state << ",\"scope\":" << scope << "}";
  }
  std::puts("TABLE module: source-free audio in seven layouts, four-patch "
            "scanning/hold, independent editing, eight voices and actual live "
            "scopes: PASS");
}
void testTableClipsAndControllers() {
  namespace tab = s3g_thru_table;
  auto t = tableFixture(1);
  setParam(t.get(), "record_lane", "2");
  setParam(t.get(), "table_shape", "1");
  t->beginClipRecording();
  t->midiPart = 0;
  t->noteOn(60, 100);
  tableRender(*t, 20);
  setParam(t.get(), "table_level", "25");
  tableRender(*t, 20);
  t->selectSplitPart(1, false);
  setParam(t.get(), "record_lane", "4");
  setParam(t.get(), "table_type", "1");
  setParam(t.get(), "table_points", "13");
  t->midiPart = 1;
  t->noteOn(64, 100);
  tableRender(*t, 30);
  t->noteOff(64);
  t->finishClipRecording();
  auto &clip = *t->activeClipSlot();
  bool a = false, b = false;
  for (int i = 0; i < clip.clipPointCount; ++i) {
    const auto &p = clip.clipPoints[i];
    if (p.type != ClipPointType::tableParameter)
      continue;
    a |= p.sourcePad == 1 && (p.parameter & 255) == tab::level && p.value == 25;
    b |=
        p.sourcePad == 7 && (p.parameter & 255) == tab::points && p.value == 13;
  }
  assert(a && b);
  assert(clip.tablePrograms[1].patches[1].v[tab::level] == 25 &&
         clip.tablePrograms[2].patches[3].v[tab::points] == 13);
  t->applyClipCurves(clip, 20);
  assert(t->splitClipControls[0].table.patches[1].v[tab::level] > 25);
  t->applyClipCurves(clip, 250);
  assert(t->splitClipControls[0].table.patches[1].v[tab::level] == 25);
  const int focus = t->splitFocus, selected = t->selectedLane;
  nrpn(*t, 5, "table_tune",
       s3g_thru_midi::encode(midiParameter("table_tune"), 7, 48));
  assert(std::abs(t->liveControlsForPart(0).table.patches[1].v[tab::tune] - 7) <
             .02 &&
         t->splitFocus == focus && int(t->selectedLane) == selected);
  // Manual takeover suppresses the old curve only on the edited patch/control.
  setParam(t.get(), "table_points", "20");
  t->applyClipCurves(clip, 600);
  assert(t->splitClipControls[1].table.patches[3].v[tab::points] == 20);
  // Program state and clip snapshots preserve independent programs and old IDs.
  t->captureActiveClip(true);
  auto json = std::make_unique<std::array<char, kStateTextSize>>();
  assert(t->serializeState(json->data(), json->size()) > 0);
  auto restored = tableFixture();
  assert(restored->restoreState(json->data()));
  assert(restored->liveControlsForPart(1).table.patches[3].v[tab::points] ==
         20);
  char directory[] = "/tmp/thru-table-XXXXXX";
  assert(mkdtemp(directory));
  std::filesystem::create_directory(std::string(directory) + "/captures");
  std::snprintf(t->moduleDirectory.data(), t->moduleDirectory.size(), "%s",
                directory);
  t->persistenceEnabled = true;
  t->persistenceGeneration = 1;
  assert(t->writeClipFile());
  const auto path = std::string(directory) + "/thru-clips.bin";
  assert(restored->readClipFileFromPath(path.c_str()));
  const auto &saved = restored->clipSlots[t->activeClip];
  assert(saved.tablePrograms[2].patches[3].v[tab::points] == 20);
  bool hasTable = false;
  for (int i = 0; i < saved.clipPointCount; ++i)
    hasTable |= saved.clipPoints[i].type == ClipPointType::tableParameter;
  assert(hasTable);
  // A truncated or corrupt TABLE footer must not partially publish clip data.
  auto bytes = std::vector<unsigned char>(
      std::istreambuf_iterator<char>(
          std::ifstream(path, std::ios::binary).rdbuf()),
      {});
  assert(bytes.size() > 204808);
  bytes.resize(bytes.size() - 16);
  std::ofstream broken(path, std::ios::binary);
  broken.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  broken.close();
  assert(!restored->readClipFileFromPath(path.c_str()));
  t.reset();
  restored.reset();
  removeBackgroundTestDirectory(directory);
  // Sequence launches authored TABLE trigs immediately without any sample.
  t = tableFixture(6);
  auto &lane = t->sequenceLive.tracks[0];
  lane.length = 4;
  lane.rate = 4;
  lane.trigs[0].enabled = true;
  lane.timeMs = 200;
  lane.timeMode = true;
  t->captureActiveClip(true);
  t->commitSequenceEdit();
  setParam(t.get(), "clip_playing", "1");
  assert(tableRender(*t, 100) > 100);
  std::puts(
      "TABLE clips: scoped gestures/readouts, NRPN focus isolation, manual "
      "takeover, JSON/format50 roundtrip and source-free Sequence trigs: PASS");
}
void testTableAutomationAndList() {
  namespace tab = s3g_thru_table;
  auto t = tableFixture();
  t->controls.table.patches[0].v[tab::tune] = -12;
  t->controls.table.patches[1].v[tab::tune] = 5;
  {
    auto dual = tableFixture(2);
    dual->controls.table.patches[0].v[tab::tune] = -7;
    dual->selectSplitPart(1, false);
    assert(dual->controls.table.patches[0].v[tab::tune] == 0);
    dual->controls.table.patches[0].v[tab::tune] = 9;
    for (int n = 0; n < 3; ++n) {
      dual->selectSplitPart(0, false);
      assert(dual->controls.table.patches[0].v[tab::tune] == -7);
      dual->selectSplitPart(1, false);
      assert(dual->controls.table.patches[0].v[tab::tune] == 9);
    }
  }
  {
    auto saved = tableFixture();
    saved->controls.table.patches[0].v[tab::tune] = 13;
    saved->captureActiveClip(false);
    assert(saved->activeClipSlot()->occupied);
    saved->clipSlots[1] = *saved->activeClipSlot();
    saved->clipSlots[1].tablePrograms[0].patches[0].v[tab::tune] = -9;
    saved->activateStoredClip(1, false);
    assert(saved->controls.table.patches[0].v[tab::tune] == -9);
    saved->activateStoredClip(0, false);
    assert(saved->controls.table.patches[0].v[tab::tune] == 13);
  }
  for (int mode : {1, 2, 3}) {
    auto copy = tableFixture(mode);
    const int count = copy->sharedDual ? 4 : copy->isQuad() ? 1 : 2;
    for (int lane = 0; lane < count; ++lane)
      copy->controls.table.patches[lane].v[tab::tune] = -12 + 7 * lane;
    copy->selectSplitPart(1, false);
    copy->copySplitPart(0);
    const int first = copy->sharedDual ? 0 : copy->firstLaneForPart(1);
    for (int lane = 0; lane < count; ++lane)
      assert(copy->controls.table.patches[first + lane].v[tab::tune] ==
             -12 + 7 * lane);
  }
  for (int stored = 0; stored < 4; ++stored) {
    auto keys = tableFixture();
    keys->padMode = static_cast<PadMode>(stored);
    setParam(keys.get(), "voice_mode", "2");
    assert(keys->padToMidiNote(68) == 48 && keys->padToMidiNote(69) == 49);
    keys->padNoteOn(68, 100);
    tableRender(*keys, 20);
    PlaybackVoice *first = nullptr;
    for (auto &v : keys->voices)
      if (v.held)
        first = &v;
    assert(first);
    const auto phase = first->engine.normalizedLaneScanPhase();
    const float envelope = first->envelope;
    keys->padNoteOn(69, 100);
    assert(first->note == 49 && first->envelope == envelope &&
           first->engine.normalizedLaneScanPhase() == phase);
    assert(std::count_if(keys->voices.begin(), keys->voices.end(),
                         [](const auto &v) { return v.active; }) == 1);
    keys->padNoteOff(69);
    assert(first->note == 48 && first->held);
    keys->padNoteOff(68);
    assert(!first->held);
    assert(int(keys->padMode) == stored);
  }
  // Several MIDI/UI edits before the next frame leave one final point.
  t->beginClipRecording();
  setParam(t.get(), "table_tune", "2");
  setParam(t.get(), "table_tune", "7");
  int coincident = 0;
  for (int n = 0; n < t->activeClipSlot()->clipPointCount; ++n) {
    const auto &point = t->activeClipSlot()->clipPoints[n];
    if (point.type == ClipPointType::tableParameter && point.sourcePad == 0 &&
        (point.parameter & 255) == tab::tune && point.timeMs == 0) {
      ++coincident;
      assert(point.value == 7);
    }
  }
  assert(coincident == 1);
  tableRender(*t, 40);
  t->finishClipRecording();
  t = tableFixture();
  t->controls.table.patches[0].v[tab::tune] = -12;
  t->controls.table.patches[1].v[tab::tune] = 5;
  t->noteOn(60, 100);
  tableRender(*t, 3);
  t->controlLfoRouted = true;
  t->controlLfoRuntime = {};
  t->controlLfoRuntime[0].initialized = true;
  t->controlLfoRuntime[0].routes[0] = {{0, 15, 131}, .1f, .1f};
  const auto effective = t->effectiveControls(t->voices[0]);
  assert(effective.table.patches[0].v[tab::tune] > -12 &&
         effective.table.patches[1].v[tab::tune] > 5);
  assert(std::abs((effective.table.patches[0].v[tab::tune] + 12) -
                  (effective.table.patches[1].v[tab::tune] - 5)) < .0001);
  assert(t->controls.table.patches[0].v[tab::tune] == -12 &&
         t->controls.table.patches[1].v[tab::tune] == 5);
  s3g_thru_lfo::Config cfg;
  cfg.target = 0;
  assert(t->controlLfoDestinationAvailable(cfg, 131));
  assert(!t->controlLfoDestinationAvailable(cfg, 127));
  t = tableFixture();
  setParam(t.get(), "voice_engine_mode", "1");
  for (int slot = 0; slot < 8; ++slot)
    setListSlot(*t, slot, 15);
  t->beginClipRecording();
  t->noteOn(60, 100);
  assert(tableRender(*t, 40) > 100);
  t->noteOff(60);
  t->finishClipRecording();
  assert(t->activeClipSlot()->clipNotes[0].engineAlgorithm == 15);
  t->releaseClipVoices();
  t->triggerClipVoice(*t->activeClipSlot(), 0);
  assert(tableRender(*t, 10) > 100);
  char directory[] = "/tmp/thru-table-list-XXXXXX";
  assert(mkdtemp(directory));
  std::filesystem::create_directory(std::string(directory) + "/captures");
  t->captureActiveClip(true);
  std::snprintf(t->moduleDirectory.data(), t->moduleDirectory.size(), "%s",
                directory);
  t->persistenceEnabled = true;
  t->persistenceGeneration = 1;
  assert(t->writeClipFile());
  auto restored = tableFixture();
  assert(restored->readClipFileFromPath(
      (std::string(directory) + "/thru-clips.bin").c_str()));
  assert(restored->clipSlots[0].singleSettings.engines.engines[7] == 15 &&
         restored->clipSlots[0].clipNotes[0].engineAlgorithm == 15);
  t.reset();
  restored.reset();
  removeBackgroundTestDirectory(directory);
  // Record-off Sequence preset changes replace every control changed by the
  // start.
  t = tableFixture(6);
  auto &clip = *t->activeClipSlot();
  clip.clipDurationMs = 1000;
  clip.clipRecorded = true;
  for (int field : {tab::decay, tab::noise, tab::bank_a}) {
    auto &point = clip.clipPoints[clip.clipPointCount++];
    point.type = ClipPointType::tableParameter;
    point.sourcePad = 0;
    point.parameter = field;
    point.timeMs = 0;
    point.value = tab::defaults[field];
  }
  ThruInstance::rebuildClipPointIndex(clip);
  setParam(t.get(), "table_type", "2");
  setParam(t.get(), "table_model", "1");
  for (int field : {tab::decay, tab::noise, tab::bank_a})
    assert(clip.clipPointCounts[kClipTableOffset + field] == 0);
  std::puts("TABLE integration: per-patch LFO bases, sample-window exclusion, "
            "source-free Engine List recording/format50 and complete preset "
            "automation takeover: PASS");
}
void testTablePortableProject() {
  namespace tab = s3g_thru_table;
  char folder[] = "/tmp/thru-table-project-XXXXXX";
  assert(mkdtemp(folder));
  const auto target = std::string(folder) + "/restored";
  std::filesystem::create_directory(target);
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(folder));
  t->stopBackgroundWork();
  setParam(t.get(), "algorithm", "15");
  setParam(t.get(), "table_tune", "13");
  t->captureActiveClip(false);
  assert(t->activeClipSlot()->occupied && t->engine.loadedLaneMask() == 0);
  assert(t->savePersistentState() && t->exportProjectFile());
  const std::string archive = t->projectOutputPath.data();
  auto restored = std::make_unique<ThruInstance>();
  assert(restored->prepare(target.c_str()));
  restored->stopBackgroundWork();
  assert(restored->savePersistentState());
  restored->projectGeneration = restored->reservePersistenceGeneration();
  std::snprintf(restored->projectArchivePath.data(),
                restored->projectArchivePath.size(), "%s", archive.c_str());
  assert(restored->importProjectFile() && restored->savePersistentState());
  restored.reset();
  restored = std::make_unique<ThruInstance>();
  assert(restored->prepare(target.c_str()));
  restored->stopBackgroundWork();
  assert(restored->controls.algorithm == Engine::PlaybackAlgorithm::table &&
         restored->controls.table.patches[0].v[tab::tune] == 13 &&
         restored->engine.loadedLaneMask() == 0 &&
         restored->activeClipSlot()->occupied);
  restored.reset();
  t.reset();
  std::filesystem::remove_all(folder);
  std::puts("TABLE project: asset-free patch occupancy, real export/import and "
            "restart without samples or recorded launch notes: PASS");
}
#include "test_table_polyphony.inc"
void testTable() {
  testTableCore();
  testTableSplinePolyphony();
  testTablePreparedRendering();
  testTableLayoutsAndScopes();
  testTableClipsAndControllers();
  testTableAutomationAndList();
  testTablePortableProject();
}
