std::unique_ptr<ThruInstance> resonFixture(int mode = 0) {
  auto t = fmFixture(mode);
  for (int part = 0; part < (t->splitEnabled ? t->partCount() : 1); ++part) {
    if (t->splitEnabled)
      t->selectSplitPart(part, false);
    setParam(t.get(), "algorithm", "19");
  }
  if (t->splitEnabled)
    t->selectSplitPart(0, false);
  return t;
}
void testResonCore() {
  namespace r = s3g_thru_reson;
  r::Instrument voice;
  voice.prepare();
  for (float sr : {44100.f, 48000.f, 96000.f})
    for (int start = 1; start <= 8; ++start)
      for (int mode = 0; mode < 2; ++mode) {
        r::Program p;
        for (auto &c : p.patches) {
          c = r::startingSound(start, c);
          c.v[r::mode] = mode;
          c.v[r::width] = 0;
        }
        voice.reset();
        double energy = 0, attack = 0;
        for (int f = 0; f < 16000; ++f) {
          if (f % 128 == 0)
            voice.beginBlock(p.patches);
          const auto out = voice.tick(p.patches, 1.5f, 220, sr);
          assert(out[0] == out[1]);
          for (float x : out) {
            assert(std::isfinite(x) && std::abs(x) < 8);
            energy += x * x;
            if (f < sr * .01f)
              attack += x * x;
          }
        }
        assert(energy > .1 && attack > .001);
      }
  // A harmonic, fundamental-heavy body must follow the played key.
  for (float hz : {110.f, 220.f, 440.f}) {
    r::Program p;
    for (auto &c : p.patches) {
      c = r::startingSound(1, c);
      c.v[r::inharmonic] = 0;
      c.v[r::brightness] = 0;
      c.v[r::width] = 0;
    }
    voice.reset();
    std::array<float, 16000> audio{};
    for (int f = 0; f < 20000; ++f) {
      auto a = voice.tick(p.patches, 0, hz, 44100);
      if (f >= 4000)
        audio[f - 4000] = a[0];
    }
    const auto power = [&](float at) {
      double re = 0, im = 0;
      for (int i = 0; i < int(audio.size()); ++i) {
        const double phase = 6.283185307179586 * at * i / 44100;
        re += audio[i] * std::cos(phase);
        im += audio[i] * std::sin(phase);
      }
      return re * re + im * im;
    };
    assert(power(hz) > 4 * std::max(power(hz * .5f), power(hz * 2)));
  }
  // Every patch ages even while inaudible; late scan entry cannot re-strike it.
  r::Program p;
  for (auto &c : p.patches) {
    c = r::startingSound(2, c);
    c.v[r::decay] = 50;
    c.v[r::hit] = c.v[r::noise] = 0;
    c.v[r::bend] = 0;
  }
  voice.reset();
  for (int f = 0; f < 30000; ++f)
    voice.tick(p.patches, 0, 110, 44100);
  double late = 0;
  for (int f = 0; f < 3000; ++f) {
    auto a = voice.tick(p.patches, 3, 110, 44100);
    late += a[0] * a[0];
  }
  assert(late < 1e-8);
  for (float sr : {44100.f, 48000.f, 96000.f}) {
    r::Config c;
    r::Overload circuit;
    circuit.reset(c, sr);
    for (int f = 0; f < 30000; ++f) {
      if (f % 32 == 0) {
        for (int field = r::drive; field < r::fields; ++field)
          c.v[field] = (f / 32) % 2 ? r::low[field] : r::high[field];
        circuit.prepare(c, sr);
      }
      for (auto &sub : circuit.tick({0, 0}))
        for (float x : sub)
          assert(x == 0);
    }
  }
  // With an actual signal, asymmetric hard saturation has a substantial DC
  // component. The shared post-mix coupling stage must remove it at all rates.
  for (float sr : {44100.f, 48000.f, 96000.f}) {
    r::Config c;
    c.v[r::drive] = 100;
    c.v[r::bias] = 100;
    c.v[r::clip] = 100;
    r::Overload circuit;
    circuit.reset(c, sr);
    double sum = 0;
    int count = 0;
    for (int f = 0; f < int(sr * 2); ++f) {
      const float x = .3f * std::sin(r::tau * 200 * f / sr);
      const auto output = circuit.tick({x, x});
      if (f >= sr)
        for (const auto &sub : output) {
          sum += sub[0];
          ++count;
        }
    }
    assert(std::abs(sum / count) < .0001);
  }
  // The dry/wet paths have the same delay when the nonlinear circuit is clean.
  r::Config c;
  c.v[r::drive] = 0;
  c.v[r::tone] = 20000;
  r::Overload dry, wet;
  c.v[r::mix] = 0;
  dry.reset(c, 44100);
  c.v[r::mix] = 100;
  wet.reset(c, 44100);
  for (int f = 0; f < 6000; ++f) {
    const float x = .25f * std::sin(f * .04f);
    assert(dry.tick({x, x}) == wet.tick({x, x}));
  }
  for (float sr : {44100.f, 48000.f, 96000.f}) {
    voice.reset();
    for (auto &c : p.patches)
      c = {};
    for (int f = 0; f < 24000; ++f) {
      if (f % 128 == 0) {
        const int field = f / 128 % r::fields;
        for (auto &c : p.patches)
          r::set(c, field, (f / 256) % 2 ? r::low[field] : r::high[field]);
        voice.beginBlock(p.patches);
      }
      for (float x : voice.tick(p.patches, 1.5f + 1.49f * std::sin(f * .001f),
                                55 + f * .03f, sr))
        assert(std::isfinite(x) && std::abs(x) < 16);
    }
  }
  std::puts("RESON core: eight bodies/Tone/Hit, three rates, prompt attacks, "
            "keyboard pitch, exact mono, aged stacks, silent live overload "
            "edits and aligned dry/wet: PASS");
}
void testResonModule() {
  namespace r = s3g_thru_reson;
  for (int mode = 0; mode < 7; ++mode) {
    auto t = resonFixture(mode);
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
  auto t = resonFixture();
  for (int v = 0; v < 8; ++v)
    t->noteOn(48 + v, 100);
  assert(tableRender(*t, 20) > 100);
  t->noteOn(72, 100);
  tableRender(*t, 8);
  t->allNotesOff();
  tableRender(*t, 120);
  for (const auto &v : t->voices)
    assert(!v.active);
  t = resonFixture(2);
  t->selectSplitPart(1, false);
  nrpn(*t, 5, "reson_decay",
       s3g_thru_midi::encode(midiParameter("reson_decay"), 700, 8000));
  assert(t->splitFocus == 1 &&
         std::abs(t->liveControlsForPart(0).reson.patches[0].v[r::decay] -
                  700) < 1);
  t = resonFixture(6);
  setParam(t.get(), "seq_hold", "0");
  setParam(t.get(), "reson_mode", "1");
  setParam(t.get(), "reson_drive", "77");
  auto locks = t->sequenceLive.soundLocks.resolve(0, 0, 0);
  assert(locks.find(4600)->value == 1 && locks.find(4624)->value == 77);
  int part = 0;
  const auto *remote = e16PageSlotParameter(*t, 5, 7, part);
  assert(remote && !std::strcmp(remote->key, "reson_output"));
  const auto before = t->controls.reson.patches[0].v;
  setParam(t.get(), "reson_start", "2");
  assert(t->controls.reson.patches[0].v == before);
  setParam(t.get(), "seq_hold", "-1");
  t->startSequenceVoice(0, 0, 0, .5, true, 0);
  assert(tableRender(*t, 16) > 100);
  t = resonFixture();
  setParam(t.get(), "voice_engine_mode", "1");
  for (int i = 0; i < 8; ++i)
    setListSlot(*t, i, 19);
  t->noteOn(60, 100);
  assert(tableRender(*t, 10) > 100);
  t->captureActiveClip(true);
  const auto undoPatch = t->controls.reson.patches[0].v;
  setParam(t.get(), "reson_start", "2");
  assert(t->controls.reson.patches[0].v[r::model] == 1 && t->undoAvailable);
  assert(t->controls.reson.patches[0].v[r::mode] == 1);
  t->restoreUndo();
  assert(t->controls.reson.patches[0].v == undoPatch);
  s3g_thru_synth::Recipe recipe;
  s3g_thru_synth::initializeGenerator(recipe, s3g_thru_synth::drum);
  for (int body : {0, 1, 2, 3, 4, 5, 9}) {
    recipe.drums[s3g_thru_synth::dModel] = body;
    assert(t->resonRecipeType(recipe.type, body));
    t->restoreLiveResonRecipe(recipe);
    assert(t->controls.reson.patches[0].v[r::model] ==
           (body == 9 ? 7 : body + 1));
  }
  for (int heavy : {6, 7, 8})
    assert(!t->resonRecipeType(recipe.type, heavy));
  if (const char *path = std::getenv("S3G_THRU_RESON_FIXTURE")) {
    t = resonFixture();
    t->noteOn(60, 100);
    tableRender(*t, 16);
    std::array<r::Instrument, 4> previews;
    for (auto &v : previews)
      v.prepare();
    for (int block = 0; block < 32; ++block) {
      std::array<std::array<float, 128>, 4> bus{};
      for (int lane = 0; lane < 4; ++lane)
        previews[lane].beginBlock(t->controls.reson.patches);
      for (int frame = 0; frame < 128; ++frame)
        for (int lane = 0; lane < 4; ++lane) {
          previews[lane].tick(t->controls.reson.patches, float(lane),
                              261.625565f, 44100);
          bus[lane][frame] = previews[lane].laneOutput[lane];
        }
      t->tableScopes[0].append(bus, 128);
    }
    char state[8192], scope[8192];
    assert(t->formatResonState(state, sizeof(state)) > 0 &&
           t->formatTableScopes(scope, sizeof(scope)) > 0);
    auto *file = std::fopen(path, "w");
    assert(file);
    std::fprintf(file, "{\"state\":%s,\"scope\":%s}\n", state, scope);
    std::fclose(file);
  }
  std::puts("RESON module: seven layouts, eight voices/steals/releases, NRPN "
            "isolation, four E16 pages, trig locks, starting sounds, Engine "
            "List and compatible light drum recipes: PASS");
}
void testResonStorage() {
  namespace n = s3g_thru_reson;
  char folder[] = "/tmp/thru-reson-state-XXXXXX";
  assert(mkdtemp(folder));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(folder));
  t->stopBackgroundWork();
  setParam(t.get(), "algorithm", "19");
  setParam(t.get(), "reson_decay", "730");
  setParam(t.get(), "reson_drive", "55");
  t->beginClipRecording();
  t->noteOn(60, 100);
  tableRender(*t, 8);
  setParam(t.get(), "reson_coupling", "7");
  t->noteOff(60);
  t->finishClipRecording();
  t->captureActiveClip(true);
  assert(t->savePersistentState());
  auto restored = std::make_unique<ThruInstance>();
  assert(restored->prepare(folder));
  restored->stopBackgroundWork();
  const auto &c = restored->controls.reson.patches[0];
  assert(c.v[n::decay] == 730.f && c.v[n::drive] == 55.f &&
         c.v[n::coupling] == 7);
  bool recorded = false;
  for (int i = 0; i < restored->activeClipSlot()->clipPointCount; ++i) {
    const auto &x = restored->activeClipSlot()->clipPoints[i];
    recorded |= x.type == ClipPointType::resonParameter &&
                x.parameter == n::coupling && x.value == 7;
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
  assert(restored->controls.algorithm == Engine::PlaybackAlgorithm::reson &&
         restored->controls.reson.patches[0].v[n::decay] == 730.f &&
         restored->controls.reson.patches[0].v[n::coupling] == 7);
  restored.reset();
  t->persistenceEnabled = false;
  t.reset();
  std::filesystem::remove_all(folder);
  std::puts("RESON storage: format58/JSON, all 32 controls, captured gestures "
            "and real portable export/import/restart: PASS");
}
void testResonMigration() {
  char folder[] = "/tmp/thru-reson-migrate-XXXXXX";
  assert(mkdtemp(folder));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(folder));
  t->stopBackgroundWork();
  setParam(t.get(), "algorithm", "18");
  setParam(t.get(), "noise_ratio_32", "39.5");
  t->captureActiveClip(true);
  assert(t->savePersistentState());
  const auto path = std::string(folder) + "/thru-clips.bin";
  std::ifstream input(path, std::ios::binary);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
  input.close();
  bytes.resize(bytes.size() - 4);
  stripResonPrograms(bytes);
  assert(bytes[8] == 57);
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
             ->noisePrograms[0]
             .patches[0]
             .v[s3g_thru_noise::ratios + 31] == 39.5f);
  assert(
      s3g_thru_reson::valid(t->activeClipSlot()->resonPrograms[0].patches[0]));
  t->persistenceEnabled = false;
  t.reset();
  std::filesystem::remove_all(folder);
  std::puts("RESON migration: genuine format57 payload preserves NOISE data "
            "and initializes missing RESON patches: PASS");
}
void testReson() {
  testResonCore();
  testResonModule();
  testResonStorage();
  testResonMigration();
}
