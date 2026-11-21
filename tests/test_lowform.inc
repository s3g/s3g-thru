std::unique_ptr<ThruInstance> lowformFixture(int mode = 0) {
  auto t = fmFixture(mode);
  for (int part = 0; part < (t->splitEnabled ? t->partCount() : 1); ++part) {
    if (t->splitEnabled)
      t->selectSplitPart(part, false);
    setParam(t.get(), "algorithm", "21");
  }
  if (t->splitEnabled)
    t->selectSplitPart(0, false);
  return t;
}
void testLowformCore() {
  namespace l = s3g_thru_lowform;
  l::Instrument voice;
  voice.prepare();
  for (float sr : {44100.f, 48000.f, 96000.f})
    for (int body = 1; body <= 6; ++body) {
      l::Program cfg;
      for (auto &patch : cfg.patches) {
        patch = l::startingSound(body, patch);
        patch.v[l::width] = 0;
      }
      voice.reset();
      double energy = 0, attack = 0;
      for (int f = 0; f < 12000; ++f) {
        if (f % 128 == 0)
          voice.beginBlock(cfg.patches);
        const auto out = voice.tick(cfg.patches, 1.5f, 110, sr);
        assert(out[0] == out[1]);
        for (float x : out) {
          assert(std::isfinite(x) && std::abs(x) < 4);
          energy += x * x;
          if (f < sr * .01f)
            attack += x * x;
        }
      }
      assert(energy > 1 && attack > .001);
    }
  // The centred Foundation is keyboard-pitched and independent of Body/Texture.
  for (float hz : {110.f, 220.f, 440.f}) {
    l::Program cfg;
    for (auto &patch : cfg.patches) {
      patch.v[l::body_level] = 0;
      patch.v[l::foundation_level] = 100;
      patch.v[l::punch] = 0;
      patch.v[l::filter_type] = 0;
      patch.v[l::low_return] = 0;
    }
    voice.reset();
    std::array<float, 12000> audio{};
    for (int f = 0; f < 16000; ++f) {
      auto a = voice.tick(cfg.patches, 0, hz, 44100);
      if (f >= 4000)
        audio[f - 4000] = a[0];
    }
    auto power = [&](float at) {
      double re = 0, im = 0;
      for (int i = 0; i < int(audio.size()); ++i) {
        const double phase = 6.283185307179586 * at * i / 44100;
        re += audio[i] * std::cos(phase);
        im += audio[i] * std::sin(phase);
      }
      return re * re + im * im;
    };
    assert(power(hz) > 20 * std::max(power(hz * .5f), power(hz * 2)));
  }
  // Matching Foundations keep identical phase during changing stack focus.
  {
    l::Program matching;
    for (auto &patch : matching.patches) {
      patch.v[l::body_level] = 0;
      patch.v[l::punch] = 12;
      patch.v[l::punch_time] = 250;
    }
    voice.reset();
    for (int n = 0; n < 18000; ++n) {
      voice.tick(matching.patches, 1.5f + 1.49f * std::sin(n * .002f), 220,
                 44100);
      for (int lane = 1; lane < 4; ++lane) {
        assert(voice.models[lane].basePhase == voice.models[0].basePhase);
        assert(voice.models[lane].subPhase == voice.models[0].subPhase);
      }
    }
  }
  // Inaudible patches age their real envelopes and do not re-strike on entry.
  l::Program cfg;
  for (auto &patch : cfg.patches) {
    patch.v[l::foundation_level] = 0;
    patch.v[l::body_decay] = 35;
    patch.v[l::texture_type] = 1;
    patch.v[l::texture_level] = 50;
    patch.v[l::texture_attack] = 20;
    patch.v[l::texture_decay] = 40;
  }
  voice.reset();
  for (int f = 0; f < 44100; ++f)
    voice.tick(cfg.patches, 0, 220, 44100);
  double late = 0;
  for (int f = 0; f < 4000; ++f) {
    auto a = voice.tick(cfg.patches, 3, 220, 44100);
    late += a[0] * a[0];
  }
  assert(late < 1e-8);
  for (float sr : {44100.f, 48000.f, 96000.f}) {
    cfg = {};
    voice.reset();
    for (int f = 0; f < 24000; ++f) {
      if (f % 128 == 0) {
        int field = f / 128 % l::fields;
        for (auto &patch : cfg.patches)
          l::set(patch, field, f / 256 % 2 ? l::low[field] : l::high[field]);
        voice.beginBlock(cfg.patches);
      }
      for (float x : voice.tick(cfg.patches, 1.5f + 1.49f * std::sin(f * .001f),
                                55 + f * .03f, sr))
        assert(std::isfinite(x) && std::abs(x) < 8);
    }
    cfg = {};
    for (auto &patch : cfg.patches)
      patch.v[l::foundation_level] = patch.v[l::body_level] =
          patch.v[l::texture_level] = 0;
    voice.reset();
    for (int f = 0; f < 12000; ++f) {
      if (f % 128 == 0) {
        for (auto &patch : cfg.patches) {
          patch.v[l::body] = float(f / 128 % 6);
          patch.v[l::filter_type] = float(f / 128 % 6);
          patch.v[l::drive] = float(f / 128 % 2) * 100;
          patch.v[l::texture_type] = float(f / 128 % 5);
        }
        voice.beginBlock(cfg.patches);
      }
      for (float x : voice.tick(cfg.patches, 1.5f, 220, sr))
        assert(x == 0);
    }
  }
  std::puts("LOWFORM core: six bodies, three rates, prompt attacks, keyboard "
            "tuning, exact mono, aged envelopes, silent edits and finite live "
            "extremes: PASS");
}
void testLowformModule() {
  namespace l = s3g_thru_lowform;
  for (int mode = 0; mode < 7; ++mode) {
    auto t = lowformFixture(mode);
    for (int part = 0; part < (t->splitEnabled ? t->partCount() : 1); ++part) {
      if (t->splitEnabled)
        t->selectSplitPart(part, false);
      t->midiPart = part;
      t->noteOn(60 + part, 100);
    }
    assert(tableRender(*t, 30) > 100);
    t->allNotesOff();
    tableRender(*t, 120);
    for (const auto &v : t->voices)
      assert(!v.active);
  }
  auto t = lowformFixture();
  for (int i = 0; i < 8; ++i)
    t->noteOn(48 + i, 100);
  assert(tableRender(*t, 20) > 100);
  t->noteOn(72, 100);
  tableRender(*t, 8);
  t->allNotesOff();
  tableRender(*t, 120);
  for (const auto &v : t->voices)
    assert(!v.active);
  t = lowformFixture(2);
  t->selectSplitPart(1, false);
  nrpn(*t, 5, "lowform_cutoff",
       s3g_thru_midi::encode(midiParameter("lowform_cutoff"), 2700, 18000));
  assert(t->splitFocus == 1 &&
         std::abs(t->liveControlsForPart(0).lowform.patches[0].v[l::cutoff] -
                  2700) < 2);
  assert(std::abs(midiRead(*t, midiParameter("lowform_cutoff"), 0) - 2700) < 2);
  t = lowformFixture(6);
  setParam(t.get(), "seq_hold", "0");
  setParam(t.get(), "lowform_body", "4");
  setParam(t.get(), "lowform_cutoff", "3700");
  auto locks = t->sequenceLive.soundLocks.resolve(0, 0, 0);
  assert(locks.find(5000)->value == 4 && locks.find(5020)->value == 3700);
  int part = 0;
  const auto *remote = e16PageSlotParameter(*t, 5, 7, part);
  assert(remote && !std::strcmp(remote->key, "lowform_drive"));
  const auto before = t->controls.lowform.patches[0].v;
  setParam(t.get(), "lowform_start", "2");
  assert(t->controls.lowform.patches[0].v == before);
  setParam(t.get(), "seq_hold", "-1");
  t->startSequenceVoice(0, 0, 0, .5, true, 0);
  assert(tableRender(*t, 16) > 100);
  t = lowformFixture();
  setParam(t.get(), "voice_engine_mode", "1");
  for (int i = 0; i < 8; ++i)
    setListSlot(*t, i, 21);
  t->noteOn(60, 100);
  assert(tableRender(*t, 10) > 100);
  t->captureActiveClip(true);
  const auto undoPatch = t->controls.lowform.patches[0].v;
  setParam(t.get(), "lowform_start", "3");
  assert(t->controls.lowform.patches[0].v[l::body] == 2 && t->undoAvailable);
  t->restoreUndo();
  assert(t->controls.lowform.patches[0].v == undoPatch);
  if (const char *path = std::getenv("S3G_THRU_LOWFORM_FIXTURE")) {
    t = lowformFixture();
    t->noteOn(60, 100);
    tableRender(*t, 16);
    std::array<l::Instrument, 4> previews;
    for (auto &v : previews)
      v.prepare();
    for (int b = 0; b < 30; ++b) {
      float bus[4][128]{};
      for (int lane = 0; lane < 4; ++lane)
        for (int f = 0; f < 128; ++f) {
          previews[lane].tick(t->controls.lowform.patches, float(lane),
                              261.625565f, 44100);
          bus[lane][f] = previews[lane].laneOutput[lane];
        }
      t->tableScopes[0].append(bus, 128);
    }
    char state[8192], scope[8192];
    assert(t->formatLowformState(state, sizeof(state)) > 0 &&
           t->formatTableScopes(scope, sizeof(scope)) > 0);
    auto *f = std::fopen(path, "w");
    assert(f);
    std::fprintf(f, "{\"state\":%s,\"scope\":%s}\n", state, scope);
    std::fclose(f);
  }
  std::puts("LOWFORM module: seven layouts, eight voices/steals/releases, NRPN "
            "focus isolation/readback, four E16 pages, trig locks, starting "
            "sound Undo and Engine List: PASS");
}
void testLowformStorage() {
  namespace n = s3g_thru_lowform;
  char folder[] = "/tmp/thru-lowform-state-XXXXXX";
  assert(mkdtemp(folder));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(folder));
  t->stopBackgroundWork();
  setParam(t.get(), "algorithm", "21");
  setParam(t.get(), "lowform_body_decay", "730");
  setParam(t.get(), "lowform_drive", "55");
  t->beginClipRecording();
  t->noteOn(60, 100);
  tableRender(*t, 8);
  setParam(t.get(), "lowform_a", "7");
  t->noteOff(60);
  t->finishClipRecording();
  t->captureActiveClip(true);
  assert(t->savePersistentState());
  auto restored = std::make_unique<ThruInstance>();
  assert(restored->prepare(folder));
  restored->stopBackgroundWork();
  const auto &c = restored->controls.lowform.patches[0];
  assert(c.v[n::body_decay] == 730.f && c.v[n::drive] == 55.f &&
         c.v[n::a] == 7);
  bool recorded = false;
  for (int i = 0; i < restored->activeClipSlot()->clipPointCount; ++i) {
    const auto &x = restored->activeClipSlot()->clipPoints[i];
    recorded |= x.type == ClipPointType::lowformParameter &&
                x.parameter == n::a && x.value == 7;
  }
  assert(recorded);
  assert(t->exportProjectFile());
  const auto target = std::string(folder) + "/restored";
  std::filesystem::create_directory(target);
  restored.reset();
  restored = std::make_unique<ThruInstance>();
  assert(restored->prepare(target.c_str()));
  restored->stopBackgroundWork();
  assert(restored->savePersistentState());
  restored->projectGeneration = restored->reservePersistenceGeneration();
  std::snprintf(restored->projectArchivePath.data(),
                restored->projectArchivePath.size(), "%s",
                t->projectOutputPath.data());
  assert(restored->importProjectFile() && restored->savePersistentState());
  restored.reset();
  restored = std::make_unique<ThruInstance>();
  assert(restored->prepare(target.c_str()));
  restored->stopBackgroundWork();
  assert(restored->controls.algorithm == Engine::PlaybackAlgorithm::lowform &&
         restored->controls.lowform.patches[0].v[n::body_decay] == 730.f &&
         restored->controls.lowform.patches[0].v[n::a] == 7);
  restored.reset();
  t->persistenceEnabled = false;
  t.reset();
  std::filesystem::remove_all(folder);
  std::puts(
      "LOWFORM storage: format60/JSON, all 32 controls, captured gestures "
      "and real portable export/import/restart: PASS");
}
void testLowformMigration() {
  char dir[] = "/tmp/thru-lowform-migrate-XXXXXX";
  assert(mkdtemp(dir));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(dir));
  t->stopBackgroundWork();
  setParam(t.get(), "algorithm", "20");
  setParam(t.get(), "partials_ratio_32", "39.5");
  t->captureActiveClip(true);
  assert(t->savePersistentState());
  const auto path = std::string(dir) + "/thru-clips.bin";
  std::ifstream input(path, std::ios::binary);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
  input.close();
  bytes.resize(bytes.size() - 4);
  stripLowformPrograms(bytes);
  assert(bytes[8] == 59);
  std::uint32_t hash = 2166136261u;
  for (auto byte : bytes) {
    hash ^= byte;
    hash *= 16777619u;
  }
  for (int i = 0; i < 4; ++i)
    bytes.push_back((hash >> (8 * i)) & 255);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  output.close();
  assert(t->readClipFileFromPath(path.c_str()));
  assert(t->activeClipSlot()
             ->partialsPrograms[0]
             .patches[0]
             .v[s3g_thru_partials::ratios + 31] == 39.5f);
  assert(s3g_thru_lowform::valid(
      t->activeClipSlot()->lowformPrograms[0].patches[0]));
  t->persistenceEnabled = false;
  t.reset();
  std::filesystem::remove_all(dir);
  std::puts("LOWFORM migration: genuine format59 retains PARTIALS and "
            "initializes missing Lowform patches: PASS");
}
void testLowform() {
  testLowformCore();
  testLowformModule();
  testLowformStorage();
  testLowformMigration();
}
