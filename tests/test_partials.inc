std::unique_ptr<ThruInstance> partialsFixture(int mode = 0) {
  auto t = fmFixture(mode);
  for (int part = 0; part < (t->splitEnabled ? t->partCount() : 1); ++part) {
    if (t->splitEnabled)
      t->selectSplitPart(part, false);
    setParam(t.get(), "algorithm", "20");
  }
  if (t->splitEnabled)
    t->selectSplitPart(0, false);
  return t;
}
void testPartialsCore() {
  namespace p = s3g_thru_partials;
  p::Instrument voice;
  voice.prepare();
  for (float sr : {44100.f, 48000.f, 96000.f})
    for (int kind = 0; kind < 3; ++kind) {
      p::Program cfg;
      for (auto &c : cfg.patches) {
        c.v[p::type] = kind;
        c.v[p::width] = 0;
      }
      voice.reset();
      double energy = 0;
      for (int f = 0; f < 24000; ++f) {
        if (f % 128 == 0)
          voice.beginBlock(cfg.patches);
        auto out = voice.tick(cfg.patches, 1.5f, 220, sr);
        assert(out[0] == out[1]);
        for (float x : out) {
          assert(std::isfinite(x) && std::abs(x) < 8);
          energy += x * x;
        }
      }
      assert(energy > 1);
    }
  // Single partials and explicit subharmonic dividers follow the keyboard.
  for (int mode = 0; mode < 2; ++mode)
    for (float hz : {110.f, 220.f, 440.f}) {
      p::Program cfg;
      for (auto &c : cfg.patches) {
        c.v[p::type] = mode;
        c.v[p::count] = 1;
        c.v[p::harmonics] = 1;
        c.v[p::subs] = 1;
        c.v[p::balance] = 100;
        c.v[p::divisors] = 2;
        c.v[p::width] = 0;
      }
      voice.reset();
      std::array<float, 16000> audio{};
      for (int f = 0; f < 20000; ++f) {
        auto a = voice.tick(cfg.patches, 0, hz, 44100);
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
      const float wanted = hz / (mode ? 2 : 1);
      assert(power(wanted) >
             4 * std::max(power(wanted * .5f), power(wanted * 2)));
    }
  // Type changes cannot overwrite the two independently authored component
  // grids.
  p::Config c;
  p::set(c, p::ratios + 31, 39.5f);
  p::set(c, p::divisors + 15, 11.25f);
  p::set(c, p::type, 1);
  assert(c.v[p::ratios + 31] == 39.5f && c.v[p::divisors + 15] == 11.25f);
  p::set(c, p::type, 0);
  assert(c.v[p::divisors + 15] == 11.25f);
  // Inactive patches run their packet clocks, so entering a lane cannot re-hit
  // it.
  p::Program cfg;
  for (auto &patch : cfg.patches) {
    patch.v[p::type] = 2;
    patch.v[p::gap] = 2000;
    patch.v[p::rise] = 1;
    patch.v[p::sustain] = 0;
    patch.v[p::decay] = 10;
    patch.v[p::variation] = 0;
  }
  voice.reset();
  for (int f = 0; f < 20000; ++f)
    voice.tick(cfg.patches, 0, 220, 44100);
  double late = 0;
  for (int f = 0; f < 1000; ++f) {
    auto a = voice.tick(cfg.patches, 3, 220, 44100);
    late += a[0] * a[0];
  }
  assert(late < 1e-8);
  for (float sr : {44100.f, 48000.f, 96000.f}) {
    voice.reset();
    for (auto &patch : cfg.patches)
      patch = {};
    for (int f = 0; f < 60000; ++f) {
      if (f % 128 == 0) {
        const int field = f / 128 % p::fields;
        for (auto &patch : cfg.patches)
          p::set(patch, field, (f / 128) % 2 ? p::low[field] : p::high[field]);
        voice.beginBlock(cfg.patches);
      }
      for (float x : voice.tick(cfg.patches, 1.5f + 1.49f * std::sin(f * .001f),
                                110 + f * .003f, sr))
        assert(std::isfinite(x) && std::abs(x) < 16);
      for (const auto &m : voice.models) {
        int active = 0;
        for (const auto &packet : m.packets)
          active += packet.active() ? 1 : 0;
        assert(active <= 4);
      }
    }
  }
  // Silent banks remain silent through the same live edits.
  for (auto &patch : cfg.patches) {
    patch = {};
    for (int j = 0; j < 32; ++j)
      patch.v[p::bands + j] = 0;
  }
  voice.reset();
  for (int f = 0; f < 18000; ++f) {
    if (f % 128 == 0) {
      for (auto &patch : cfg.patches) {
        patch.v[p::type] = (f / 128) % 3;
        patch.v[p::tilt] = (f / 128) % 2 ? 12 : -24;
      }
      voice.beginBlock(cfg.patches);
    }
    for (float x : voice.tick(cfg.patches, 1.5, 220, 44100))
      assert(x == 0);
  }
  std::puts("PARTIALS core: BANK/SUB/PACKET at three rates, keyboard/divider "
            "tuning, exact mono, independent grids, aged patches, bounded "
            "packet pool, normalized live extremes and silent edits: PASS");
}
void testPartialsModule() {
  namespace p = s3g_thru_partials;
  for (int mode = 0; mode < 7; ++mode) {
    auto t = partialsFixture(mode);
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
  auto t = partialsFixture();
  setParam(t.get(), "partials_type", "2");
  assert(midiMaximum(*t, midiParameter("partials_count"), 0) == 8);
  assert(midiRead(*t, midiParameter("partials_count"), 0) == 8);
  setParam(t.get(), "partials_type", "0");
  assert(midiMaximum(*t, midiParameter("partials_count"), 0) == 32);
  for (int v = 0; v < 8; ++v)
    t->noteOn(48 + v, 100);
  assert(tableRender(*t, 20) > 100);
  t->noteOn(72, 100);
  tableRender(*t, 8);
  t->allNotesOff();
  tableRender(*t, 120);
  for (const auto &v : t->voices)
    assert(!v.active);
  t = partialsFixture(2);
  t->selectSplitPart(1, false);
  nrpn(*t, 5, "partials_tilt",
       s3g_thru_midi::encode(midiParameter("partials_tilt"), -7, 12));
  assert(t->splitFocus == 1 &&
         std::abs(t->liveControlsForPart(0).partials.patches[0].v[p::tilt] +
                  7) < .01);
  t->selectSplitPart(0, false);
  setParam(t.get(), "partials_type", "1");
  setParam(t.get(), "partials_band_group", "7");
  t->selectSplitPart(1, false);
  int part = 0;
  const auto *control = e16PageSlotParameter(*t, 5, 3, part);
  assert(control && !std::strcmp(control->key, "partials_divisor_16"));
  t = partialsFixture(6);
  setParam(t.get(), "seq_hold", "0");
  setParam(t.get(), "partials_type", "1");
  setParam(t.get(), "partials_divisor_16", "11.25");
  auto locks = t->sequenceLive.soundLocks.resolve(0, 0, 0);
  assert(locks.find(4800)->value == 1 && locks.find(4903)->value == 11.25);
  setParam(t.get(), "seq_hold", "-1");
  t->startSequenceVoice(0, 0, 0, .5, true, 0);
  assert(tableRender(*t, 16) > 100);
  t = partialsFixture();
  setParam(t.get(), "voice_engine_mode", "1");
  for (int i = 0; i < 8; ++i)
    setListSlot(*t, i, 20);
  t->noteOn(60, 100);
  assert(tableRender(*t, 10) > 100);
  s3g_thru_synth::Recipe recipe;
  s3g_thru_synth::initializeGenerator(recipe, s3g_thru_synth::partialsType);
  recipe.ratios[3][7] = 39.5f;
  recipe.partials[3][7] = -.75f;
  t->restoreLivePartialsRecipe(recipe);
  assert(t->controls.partials.patches[0].v[p::ratios + 31] == 39.5f &&
         t->controls.partials.patches[0].v[p::bands + 31] == -.75f);
  s3g_thru_synth::initializeGenerator(recipe, s3g_thru_synth::subharmonic);
  recipe.ratios[3][7] = 11.25f;
  t->restoreLivePartialsRecipe(recipe);
  assert(t->controls.partials.patches[0].v[p::divisors + 15] == 11.25f);
  s3g_thru_synth::initializeGenerator(recipe, s3g_thru_synth::bloom);
  for (int lane = 0; lane < 4; ++lane)
    recipe.ratios[lane][0] = 1 + lane * .25f;
  t->restoreLivePartialsRecipe(recipe);
  for (int lane = 0; lane < 4; ++lane)
    assert(t->controls.partials.patches[lane].v[p::ratios] == 1 + lane * .25f);
  if (const char *path = std::getenv("S3G_THRU_PARTIALS_FIXTURE")) {
    t = partialsFixture();
    t->noteOn(60, 100);
    tableRender(*t, 16);
    std::array<p::Instrument, 4> previews;
    for (auto &v : previews)
      v.prepare();
    for (int block = 0; block < 32; ++block) {
      std::array<std::array<float, 128>, 4> bus{};
      for (int lane = 0; lane < 4; ++lane)
        previews[lane].beginBlock(t->controls.partials.patches);
      for (int frame = 0; frame < 128; ++frame)
        for (int lane = 0; lane < 4; ++lane) {
          previews[lane].tick(t->controls.partials.patches, float(lane),
                              261.625565f, 44100);
          bus[lane][frame] = previews[lane].laneOutput[lane];
        }
      t->tableScopes[0].append(bus, 128);
    }
    char state[16384], scope[8192];
    assert(t->formatPartialsState(state, sizeof(state)) > 0 &&
           t->formatTableScopes(scope, sizeof(scope)) > 0);
    auto *file = std::fopen(path, "w");
    assert(file);
    std::fprintf(file, "{\"state\":%s,\"scope\":%s}\n", state, scope);
    std::fclose(file);
  }
  std::puts("PARTIALS module: seven layouts, eight voices/steals/releases, "
            "NRPN isolation, grouped E16 dividers, Sequence locks, Engine List "
            "and all three legacy recipe transfers: PASS");
}
void testPartialsStorage() {
  namespace n = s3g_thru_partials;
  char folder[] = "/tmp/thru-partials-state-XXXXXX";
  assert(mkdtemp(folder));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(folder));
  t->stopBackgroundWork();
  setParam(t.get(), "algorithm", "20");
  setParam(t.get(), "partials_ratio_32", "39.5");
  setParam(t.get(), "partials_band_32", "-0.75");
  t->beginClipRecording();
  t->noteOn(60, 100);
  tableRender(*t, 8);
  setParam(t.get(), "partials_drift", "7");
  t->noteOff(60);
  t->finishClipRecording();
  t->captureActiveClip(true);
  assert(t->savePersistentState());
  auto restored = std::make_unique<ThruInstance>();
  assert(restored->prepare(folder));
  restored->stopBackgroundWork();
  const auto &c = restored->controls.partials.patches[0];
  assert(c.v[n::ratios + 31] == 39.5f && c.v[n::bands + 31] == -.75f &&
         c.v[n::drift] == 7);
  bool recorded = false;
  for (int i = 0; i < restored->activeClipSlot()->clipPointCount; ++i) {
    const auto &x = restored->activeClipSlot()->clipPoints[i];
    recorded |= x.type == ClipPointType::partialsParameter &&
                x.parameter == n::drift && x.value == 7;
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
  assert(restored->controls.algorithm == Engine::PlaybackAlgorithm::partials &&
         restored->controls.partials.patches[0].v[n::ratios + 31] == 39.5f &&
         restored->controls.partials.patches[0].v[n::drift] == 7);
  restored.reset();
  t->persistenceEnabled = false;
  t.reset();
  std::filesystem::remove_all(folder);
  std::puts("PARTIALS storage: format59/JSON, all components, captured "
            "gestures and real portable export/import/restart: PASS");
}
void testPartialsMigration() {
  char folder[] = "/tmp/thru-partials-migrate-XXXXXX";
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
  stripPartialsPrograms(bytes);
  assert(bytes[8] == 58);
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
  assert(s3g_thru_partials::valid(
      t->activeClipSlot()->partialsPrograms[0].patches[0]));
  t->persistenceEnabled = false;
  t.reset();
  std::filesystem::remove_all(folder);
  std::puts("PARTIALS migration: genuine format58 payload preserves NOISE data "
            "and initializes missing PARTIALS patches: PASS");
}

void testPartialsDenseState() {
  auto t = partialsFixture();
  const auto populate = [](auto &program, const auto &low, const auto &high,
                           auto discrete) {
    for (auto &patch : program.patches)
      for (int f = 0; f < int(patch.v.size()); ++f) {
        const float value = low[f] + (high[f] - low[f]) * .43218765f;
        patch.v[f] = discrete(f) ? std::round(value) : value;
      }
  };
  const auto fill = [&](Engine::Controls &c) {
    populate(c.table, s3g_thru_table::low, s3g_thru_table::high,
             s3g_thru_table::discrete);
    populate(c.scanned, s3g_thru_scanned::low, s3g_thru_scanned::high,
             s3g_thru_scanned::discrete);
    populate(c.fm, s3g_thru_fm::low, s3g_thru_fm::high, s3g_thru_fm::discrete);
    populate(c.noise, s3g_thru_noise::low, s3g_thru_noise::high,
             s3g_thru_noise::discrete);
    populate(c.reson, s3g_thru_reson::low, s3g_thru_reson::high,
             s3g_thru_reson::discrete);
    populate(c.partials, s3g_thru_partials::low, s3g_thru_partials::high,
             s3g_thru_partials::discrete);
  };
  fill(t->controls);
  fill(t->splitSingleControls);
  for (auto &part : t->splitParts)
    fill(part.controls);
  std::vector<char> state(kStateTextSize);
  const int size = t->serializeState(state.data(), int(state.size()));
  assert(size > 32768 && size < int(state.size()));
  auto other = partialsFixture();
  assert(other->restoreState(state.data()));
  assert(other->controls.partials.patches[0].v ==
         t->controls.partials.patches[0].v);
  std::printf("PARTIALS dense state: all synth programs edited, %d-byte JSON, "
              "bounded metadata and exact restore: PASS\n",
              size);
}
void testPartialsScopePitch() {
  auto t = partialsFixture();
  t->noteOn(60, 100);
  tableRender(*t, 16);
  const PlaybackVoice *voice = nullptr;
  for (const auto &v : t->voices)
    if (v.active) {
      voice = &v;
      break;
    }
  assert(voice);
  const float before = voice->engine.tableScopeFrequency(0);
  setParam(t.get(), "partials_tune", "12");
  tableRender(*t, 48);
  assert(std::abs(voice->engine.tableScopeFrequency(0) - before * 2) < .01f);
  std::puts("PARTIALS scope: held-note TUNE changes the displayed waveform "
            "pitch reference: PASS");
}
void testPartials() {
  testPartialsScopePitch();
  testPartialsDenseState();
  testPartialsCore();
  testPartialsModule();
  testPartialsStorage();
  testPartialsMigration();
}
