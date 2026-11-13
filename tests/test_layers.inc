std::unique_ptr<ThruInstance> layersFixture() {
  auto t = splitFixture();
  t->setPerformanceMode(0);
  t->controls.algorithm = Engine::PlaybackAlgorithm::motion;
  t->setPerformanceMode(5);
  t->stopBackgroundWork();
  return t;
}
void testLayersGeometryAndOwnership() {
  using namespace s3g_thru_layers;
  assert(cornerForPad(24) == 0 && cornerForPad(27) == 1 &&
         cornerForPad(0) == 2 && cornerForPad(3) == 3);
  const auto center = weights({.5f, .5f});
  for (float w : center)
    assert(w == .25f);
  auto t = layersFixture();
  assert(t->performanceMode() == 5 && !t->splitEnabled);
  assert(!t->voiceEngineSettings.enabled);
  t->padNoteOn(68 + 31, 100);
  t->padNoteOn(68 + 7, 100); // Filter + Delay
  const auto offset = LayerPatch::config + LayerPatch::weight0;
  assert(t->layersLive.values[offset] == 0 &&
         t->layersLive.values[offset + 2] == 0);
  assert(std::abs(t->layersLive.values[offset + 1] - .5f) < 1e-6f &&
         std::abs(t->layersLive.values[offset + 3] - .5f) < 1e-6f);
  assert(std::none_of(t->voices.begin(), t->voices.end(),
                      [](auto &v) { return v.held; }));
  t->layersPadPressure(31, 127);
  t->layersPadPressure(7, 32);
  assert(t->layersLive.values[offset + 1] > .79f);
  t->padNoteOff(68 + 31);
  t->padNoteOff(68 + 7);
  assert(t->layersLive.values[offset + 3] ==
         1); // last position stays after release
  for (int pad : {0, 1, 2, 3, 8, 9, 10, 11})
    t->padNoteOn(68 + pad, 100);
  assert(std::count_if(t->voices.begin(), t->voices.end(),
                       [](auto &v) { return v.held; }) == 8);
  for (const auto &v : t->voices)
    if (v.active)
      assert(v.engine.loadedLaneMask() == 15u && v.part == 0);
  t->shiftHeld = true;
  t->padNoteOn(68 + 16, 100);
  t->padNoteOff(68 + 16);
  assert(t->latchedPadNotes[16] >= 0);
  t->padNoteOn(68 + 16, 100);
  t->padNoteOff(68 + 16);
  assert(t->latchedPadNotes[16] < 0);
  t->shiftHeld = false;
  t->allNotesOff();
  assert(t->heldLayersCorner() < 0);
  t->setPerformanceMode(0);
  assert(t->performanceMode() == 0);
  std::puts("Layers geometry, separate FX ownership, pressure weights, eight "
            "voices and latch: PASS");
}
void testLayersKeyboardAndLegato() {
  auto t = layersFixture();
  t->rootNote = 65;
  t->padOctave = 1;
  t->controls.locus = .37f;
  const auto savedPosition = t->controls.locus;
  for (const int pad : {0, 1, 8, 24, 27}) {
    const int expected = 65 - 12 + 12 + (pad / 8) * 5 + pad % 4;
    t->padNoteOn(68 + pad, 100);
    assert(t->padMappedNotes[pad] == expected);
    assert(t->heldLayersCorner() == -1); // Left corners are ordinary notes.
    renderSplit(*t, 2);
    assert(t->effectiveControls(newestVoice(*t, 0)).locus == savedPosition);
    t->padNoteOff(68 + pad);
  }
  assert(t->localPad(24) == 12 && t->localPad(27) == 15);
  const auto &midi = midiParameter("locus");
  midiWrite(*t, midi, 0, .73f);
  assert(std::abs(t->controls.locus - .73f) < 1e-6f);
  setParam(t.get(), "layers_corner", "3");
  midiWrite(*t, midi, 0, .67f);
  assert(std::abs(t->controls.locus - .67f) < 1e-6f);
  assert(std::abs(midiRead(*t, midi, 0) - .67f) < 1e-6f);
  setParam(t.get(), "layers_fx1", ".81");
  assert(std::abs(t->layersLive.values[LayerPatch::fx + 3 * 8 + 1] - .81f) <
         1e-6f);
  setParam(t.get(), "layers_paste", "1");
  assert(std::strstr(t->lastError.data(), "Copy an FX"));
  setParam(t.get(), "layers_copy", "1");
  assert(t->lastError[0] == '\0');
  setParam(t.get(), "layers_corner", "1");
  setParam(t.get(), "layers_paste", "1");
  assert(t->lastError[0] == '\0');
  assert(std::abs(t->layersLive.values[LayerPatch::fx + 8 + 1] - .81f) < 1e-6f);
  t->allNotesOff();
  renderSplit(*t, 80);
  t->voiceMode = 2;
  t->padNoteOn(68, 100);
  renderSplit(*t, 10);
  auto *mono = &newestVoice(*t, 0);
  const auto position = mono->engine.normalizedPosition();
  const auto envelope = mono->envelope;
  const auto oldNote = mono->note;
  t->padNoteOn(68 + 27, 100);
  assert(&newestVoice(*t, 0) == mono && mono->note != oldNote);
  assert(mono->envelope == envelope &&
         mono->engine.normalizedPosition() == position);
  t->padNoteOff(68 + 27);
  assert(mono->held && mono->sourcePad == 0 && mono->note == oldNote);
  for (int alg = 0; alg < 15; ++alg) {
    t->controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(alg);
    assert(int(t->effectiveControls(*mono).algorithm) == alg);
  }
  // Prototype engine-corner values must not alter the regular engine.
  t->layersLive.values[static_cast<std::size_t>(LockParameter::locus)] = .01f;
  assert(std::abs(t->effectiveControls(*mono).locus - .67f) < 1e-6f);
  t->padNoteOff(68);
  assert(!mono->held);
  // Export the real native UI transactions for the JavaScript contract test.
  if (const auto *path = std::getenv("S3G_THRU_EDITOR_FIXTURE")) {
    setParam(t.get(), "layers_corner", "2");
    setParam(t.get(), "layers_fx0", "2");
    setParam(t.get(), "layers_fx1", ".73");
    std::array<char, 32768> editor{};
    std::array<char, 4096> layers{};
    assert(getParam(t.get(), "ui_editor_state", editor.data(), editor.size()) >
           0);
    assert(getParam(t.get(), "layers_view", layers.data(), layers.size()) > 0);
    auto *file = std::fopen(path, "wb");
    assert(file);
    assert(std::fprintf(file, "{\"editor\":%s,\"layers\":%s}", editor.data(),
                        layers.data()) > 0);
    assert(std::fclose(file) == 0);
  }
  std::puts("Layers normal split keyboard, direct engine/MIDI controls, FX "
            "copy and legato: PASS");
}
void testLayersParallelAudio() {
  using namespace s3g_thru_layers;
  for (float rate : {44100.f, 48000.f}) {
    auto rack = std::make_unique<Rack>();
    assert(rack->prepare(rate));
    LayerPatch p;
    for (int c = 0; c < 4; ++c) {
      p.values[LayerPatch::fx + c * 8] = 0;
      p.values[LayerPatch::config + LayerPatch::weight0 + c] = .25f;
    }
    std::array<std::array<float, 64>, 2> bus{};
    int impulse = -1;
    float peak = 0;
    for (int b = 0; b < 240; ++b) {
      bus = {};
      if (b == 100) {
        bus[0][0] = .2f;
        bus[1][0] = -.1f;
      }
      rack->process(64, bus, p);
      for (int f = 0; f < 64; ++f) {
        if (std::abs(bus[0][f]) > 1e-7f) {
          if (impulse < 0)
            impulse = b * 64 + f;
          peak = std::max(peak, std::abs(bus[0][f]));
          assert(std::abs(bus[1][f] + bus[0][f] * .5f) < 1e-6f);
        }
      }
    }
    assert(impulse == 100 * 64 + 512 &&
           std::abs(peak - .2f) <
               1e-5f); // correlated corners never add four dry copies
    Slot filter;
    assert(filter.prepare(rate, 1));
    const std::array<float, 4> filterParams{.7f, .2f, 0, 0};
    filter.beginBlock(filterParams);
    int firstFilter = -1;
    for (int n = 0; n < 2200; ++n) {
      const float input = n == 1000 ? .05f : 0;
      const auto sample = filter.process({input, input}, filterParams);
      if (std::abs(sample[0]) > 1e-9f && firstFilter < 0)
        firstFilter = n;
    }
    assert(firstFilter ==
           1000 + 512); // includes the oversampled Drive's 24 frames
    Slot initialDelay;
    assert(initialDelay.prepare(rate, 3));
    const std::array<float, 4> initialDelayParams{0, 0, .7f, 0};
    initialDelay.beginBlock(initialDelayParams);
    int firstDelay = -1;
    for (int n = 0; n < 2000; ++n) {
      const auto sample = initialDelay.process(
          {n == 0 ? .1f : 0, n == 0 ? .1f : 0}, initialDelayParams);
      if (std::abs(sample[0]) > 1e-8f && firstDelay < 0)
        firstDelay = n;
    }
    assert(firstDelay >=
           512 + int(rate * .02f) - 1); // no one-frame startup echo
    Slot delay;
    assert(delay.prepare(rate, 3));
    const std::array<float, 4> delayParams{.1f, .8f, .7f, 1};
    delay.beginBlock(delayParams);
    for (int n = 0; n < 15000; ++n) {
      const float input = .04f * std::sin(n * .037f);
      const auto sample = delay.process({input, input}, delayParams);
      assert(std::abs(sample[0] - sample[1]) < 1e-7f);
    }
    Slot a, b;
    assert(a.prepare(rate, 2) && b.prepare(rate, 2));
    std::array<float, 4> pa{.02f, .1f, .5f, .5f}, pb{.85f, .9f, .5f, .9f};
    a.beginBlock(pa);
    b.beginBlock(pb);
    double energyA = 0, energyB = 0, difference = 0;
    for (int n = 0; n < int(rate * 1.5f); ++n) {
      const float input = n < int(rate * .15f)
                              ? .08f * std::sin(n * 6.28318530718f * 440 / rate)
                              : 0;
      const auto x = a.process({input, input * .8f}, pa),
                 y = b.process({input, input * .8f}, pb);
      for (float v : x)
        assert(std::isfinite(v));
      for (float v : y)
        assert(std::isfinite(v));
      if (n > int(rate * .4f)) {
        energyA += x[0] * x[0];
        energyB += y[0] * y[0];
        difference += std::abs(x[0] - y[0]);
      }
    }
    assert(energyB > energyA * 2 &&
           difference > .1); // independent spectral memory/persistence
    float previous = 0, maxStep = 0;
    for (int block = 0; block < 600; ++block) {
      for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < 64; ++n)
          bus[ch][n] =
              .04f * std::sin((block * 64 + n) * .037f) * (ch ? .7f : 1.f);
      for (int c = 0; c < 4; ++c) {
        p.values[LayerPatch::fx + c * 8] = c;
        p.values[LayerPatch::config + LayerPatch::weight0 + c] =
            block % 4 == c ? 1 : 0;
        p.values[LayerPatch::fx + c * 8 + 1] = block % 23 < 11 ? .15f : .8f;
      }
      rack->process(64, bus, p);
      for (float x : bus[0]) {
        assert(std::isfinite(x) && std::abs(x) < 2);
        maxStep = std::max(maxStep, std::abs(x - previous));
        previous = x;
      }
    }
    assert(maxStep < .35f);
  }
  std::puts("Layers four-branch stereo alignment, duplicate Blur histories and "
            "live edit "
            "continuity: PASS");
}
void testLayersClipsAndPersistence() {
  char dir[] = "/tmp/thru-layers-bank-XXXXXX";
  assert(mkdtemp(dir));
  auto t = layersFixture();
  t->padNoteOn(68 + 24, 100);
  renderSplit(*t, 20);
  t->beginClipRecording();
  renderSplit(*t, 20);
  assert(t->clipRecording && t->activeClipSlot()->clipNoteCount == 1);
  t->padNoteOn(68 + 31, 100);
  renderSplit(*t, 20);
  t->padNoteOff(68 + 31);
  setParam(t.get(), "layers_surface", "1");
  setParam(t.get(), "layers_corner", "2");
  setParam(t.get(), "layers_fx0", "2");
  setParam(t.get(), "layers_fx1", ".8");
  t->padNoteOff(68 + 24);
  renderSplit(*t, 20);
  t->finishClipRecording();
  const auto before = t->layersLive.values;
  assert(t->activeClipSlot()->takeCount == 1);
  assert(std::any_of(
      t->activeClipSlot()->clipPoints.begin(),
      t->activeClipSlot()->clipPoints.begin() +
          t->activeClipSlot()->clipPointCount,
      [](auto &p) { return p.type == ClipPointType::layersParameter; }));
  t->clipRecordMode = ClipRecordMode::overdub;
  t->beginClipRecording();
  renderSplit(*t, 20);
  setParam(t.get(), "layers_fx1", ".15");
  renderSplit(*t, 20);
  t->finishClipRecording();
  assert(t->activeClipSlot()->takeCount == 2);
  t->undoLastClipTake();
  assert(t->activeClipSlot()->takeCount == 1);
  assert(t->layersLive.values[LayerPatch::fx + 2 * 8 + 1] ==
         before[LayerPatch::fx + 2 * 8 + 1]);
  t->captureActiveClip(true);
  std::array<char, kStateTextSize> state{};
  const int length = t->serializeState(state.data(), state.size());
  assert(length > 0 && length < int(state.size()));
  auto copy = std::make_unique<ThruInstance>();
  assert(copy->prepare(nullptr));
  copy->stopBackgroundWork();
  assert(copy->restoreState(state.data()));
  assert(copy->performanceMode() == 5 &&
         copy->layersLive.values == t->layersLive.values);
  copy->clipSlots = t->clipSlots;
  copy->activeClip = 0;
  copy->activateStoredClip(0, false);
  assert(copy->performanceMode() == 5);
  copy->persistenceEnabled = true;
  std::snprintf(copy->moduleDirectory.data(), copy->moduleDirectory.size(),
                "%s", dir);
  copy->persistenceGeneration = 42;
  assert(copy->writeClipFile());
  auto disk = std::make_unique<ThruInstance>();
  assert(disk->prepare(nullptr));
  disk->stopBackgroundWork();
  assert(disk->readClipFileFromPath(
      (std::string(dir) + "/thru-clips.bin").c_str()));
  disk->activateStoredClip(0, false);
  assert(disk->performanceMode() == 5 &&
         disk->layersLive.values == t->layersLive.values);
  disk->layersOverrides.reset();
  disk->applyClipCurves(*disk->activeClipSlot(), 80);
  assert(disk->layersRecorded.active);
  // Take tags retain the full scoped field via (chunk, low-byte parameter).
  for (std::size_t i = 0; i < disk->activeClipSlot()->clipPointCount; ++i) {
    const auto &p = disk->activeClipSlot()->clipPoints[i];
    if (p.type == ClipPointType::layersParameter)
      assert(ThruInstance::clipPointLane(p) < kClipCurveLaneCount);
  }
  disk.reset();
  copy.reset();
  t.reset();
  std::filesystem::remove_all(dir);
  std::puts("Layers held-note recording, resolved FX curves, take Undo, state "
            "and clip-bank "
            "roundtrip: PASS");
}
void testLayersFxClipReplayAndIndicators() {
  auto t = layersFixture();
  const double phaseMs = 100.0 * kBlockFrames * 1000.0 / t->sampleRate;
  const auto weight = LayerPatch::config + LayerPatch::weight0;
  const auto pressure = LayerPatch::config + LayerPatch::pressure;
  t->padNoteOn(68 + 31, 100);
  t->layersPadPressure(31, 100);
  t->layersPadPressure(31, 0); // Terminal pressure can precede Note Off.
  t->padNoteOff(68 + 31);
  t->layersPadPressure(31, 0); // Or follow it.
  assert(t->layersLive.values[pressure] > .78f);
  assert(t->layersActiveFxPads() == (std::uint32_t(1) << 31));
  setParam(t.get(), "layers_return", "1");
  t->padNoteOn(68 + 7, 100);
  t->padNoteOff(68 + 7);
  assert(t->layersActiveFxPads() == 0);
  setParam(t.get(), "layers_return", "0");
  setParam(t.get(), "layers_glide", "5");
  setParam(t.get(), "layers_corner", "1");
  setParam(t.get(), "layers_fx1", "0"); // Filter cutoff 25 Hz.
  t->padNoteOn(68 + 28, 100);
  t->layersPadPressure(28, 127);
  t->padNoteOff(68 + 28);
  t->beginClipRecording();
  renderSplit(*t, 100);
  t->padNoteOn(68 + 31, 100);
  t->layersPadPressure(31, 127);
  renderSplit(*t, 100);
  t->padNoteOff(68 + 31);
  renderSplit(*t, 100);
  t->padNoteOn(68 + 4, 100);
  t->layersPadPressure(4, 90);
  renderSplit(*t, 100);
  t->padNoteOff(68 + 4);
  renderSplit(*t, 100);
  t->finishClipRecording();
  auto *clip = t->activeClipSlot();
  assert(clip && clip->clipRecorded && clip->clipNoteCount == 0);
  assert(clip->clipPointCounts[kClipLayersOffset + weight + 1] >= 3);
  t->applyClipCurves(*clip, phaseMs * .3);
  assert(t->layersRecorded.values[weight] == 1);
  assert(t->layersActiveFxPads() == (std::uint32_t(1) << 28));
  t->applyClipCurves(*clip, phaseMs * 1.5);
  assert(t->layersRecorded.values[weight + 1] == 1);
  assert(t->layersActiveFxPads() == (std::uint32_t(1) << 31));
  t->applyClipCurves(*clip, phaseMs * 3.5);
  assert(t->layersRecorded.values[weight + 2] == 1);
  assert(t->layersActiveFxPads() == (std::uint32_t(1) << 4));
  // FX-only clips affect a new live note through the actual shared audio rack.
  t->controls.algorithm = Engine::PlaybackAlgorithm::sample;
  t->controls.sampleMode = Engine::SampleMode::loop;
  t->padNoteOn(68 + 24, 100);
  renderSplit(*t, 50);
  const auto energyAt = [&](double time) {
    t->clipPositionMs = time;
    t->clipAbsolutePositionMs = time;
    t->clipNeedsStart = false;
    renderSplit(*t, 30);
    double energy = 0;
    std::array<float, kBlockFrames * 2> raw{};
    for (int b = 0; b < 10; ++b) {
      t->render(nullptr, kBlockFrames, raw.data());
      for (float x : raw)
        energy += x * x;
    }
    return energy;
  };
  const double dry = energyAt(phaseMs * .1), filtered = energyAt(phaseMs * 1.3);
  assert(dry > 1e-4 && filtered < dry * .6);
  assert(t->layersRack.currentWeights()[1] > .99f);
  // Repeating the stored live position still overrides a different replay
  // position.
  t->padNoteOn(68 + 4, 100);
  t->padNoteOff(68 + 4);
  t->applyClipCurves(*clip, phaseMs * 1.5);
  assert(t->layersRecorded.values[weight + 2] == 1);
  t->startClipPlaybackNow();
  t->applyClipCurves(*clip, phaseMs * 1.5);
  assert(t->layersRecorded.values[weight + 1] == 1);
  t->padNoteOff(68 + 24);
  t->allNotesOff();
  t->clipRecordMode = ClipRecordMode::overdub;
  t->clipPositionMs = t->clipAbsolutePositionMs = phaseMs * 1.5;
  t->padNoteOn(68 + 7, 100); // A held FX gesture must seed an overdub too.
  t->beginClipRecording();
  renderSplit(*t, 20);
  t->padNoteOff(68 + 7);
  t->finishClipRecording();
  assert(clip->takeCount == 2 && t->layersOverrides.none());
  t->applyClipCurves(*clip, phaseMs * 1.5);
  assert(t->layersRecorded.values[weight + 3] == 1);
  t->undoLastClipTake();
  t->applyClipCurves(*clip, phaseMs * 1.5);
  assert(clip->takeCount == 1 && t->layersRecorded.values[weight + 1] == 1);
  std::puts("Layers FX-only clip audio, seeded held gestures, exact plateaus, "
            "overdub Undo and "
            "retained/playback LEDs: PASS");
}
void testLayersIdleDriveAndEncoderRecording() {
  s3g_daisy::ThruDrive full, clean;
  for (int n = 0; n < 12000; ++n) {
    const std::array<float, 2> input{.07f * std::sin(n * .031f),
                                     .05f * std::cos(n * .047f)};
    assert(full.process(input, 0) == clean.processClean(input));
  }
  for (const float rate : {44100.f, 48000.f}) {
    s3g_thru_layers::Slot slot;
    assert(slot.prepare(rate, 1));
    std::array<float, 4> params{.64f, .1f, 0, 0};
    slot.prime(1, params);
    float previous = 0, maxStep = 0;
    double energy = 0;
    for (int n = 0; n < int(rate * 1.2f); ++n) {
      if (n % 2048 == 0) {
        params[2] = (n / 2048) % 2 ? .9f : 0.f;
        slot.beginBlock(params);
      }
      const float input = .006f * std::sin(n * 6.2831853f * 320 / rate);
      const auto out = slot.process({input, input * .7f}, params);
      for (float x : out)
        assert(std::isfinite(x));
      maxStep = std::max(maxStep, std::abs(out[0] - previous));
      previous = out[0];
      energy += out[0] * out[0];
    }
    assert(energy > .001 && maxStep < .009f);
    // Recursive silence tails must not settle in subnormal arithmetic.
    params[2] = 0;
    slot.beginBlock(params);
    for (int n = 0; n < int(rate * 3); ++n) {
      const auto out = slot.process({0, 0}, params);
      for (float x : out)
        assert(std::isfinite(x) && std::fpclassify(x) != FP_SUBNORMAL);
    }
  }
  auto t = layersFixture();
  t->controls.algorithm = Engine::PlaybackAlgorithm::sample;
  t->controls.sampleMode = Engine::SampleMode::loop;
  setParam(t.get(), "layers_glide", "5");
  for (const int pad : {0, 1, 2, 3, 8, 9, 10, 11})
    t->padNoteOn(68 + pad, 100);
  t->beginClipRecording();
  std::array<float, kBlockFrames * 2> raw{};
  float previous = 0, maxStep = 0;
  double energy = 0;
  for (int block = 0; block < 700; ++block) {
    char value[24]{};
    std::snprintf(value, sizeof(value), "%.6f",
                  .5f + .49f * std::sin(block * .023f));
    setParam(t.get(), "layers_fx_x", value);
    std::snprintf(value, sizeof(value), "%.6f",
                  .5f + .49f * std::sin(block * .037f));
    setParam(t.get(), "layers_fx_y", value);
    t->render(nullptr, kBlockFrames, raw.data());
    for (std::size_t n = 0; n < kBlockFrames; ++n) {
      const float x = raw[n * 2];
      assert(std::isfinite(x));
      energy += x * x;
      maxStep = std::max(maxStep, std::abs(x - previous));
      previous = x;
    }
  }
  t->finishClipRecording();
  assert(t->activeClipSlot()->clipNoteCount == 8 &&
         t->activeClipSlot()->clipPointCount > 100);
  assert(energy > .01 && maxStep < .12f);
  assert(t->layersOverrides.none());
  std::puts("Layers exact clean-drive latency, smooth nonlinear wake/tail, "
            "quiet-state "
            "protection and eight-voice X/Y recording: PASS");
}
void testLayersExpandedEffects() {
  using namespace s3g_thru_layers;
  const std::array<std::array<float, 4>, 13> controls{
      {{.35f, .8f, .4f, .3f},
       {.55f, .3f, .6f, .25f},
       {3.f / 9, .55f, .5f, .5f},
       {0, .4f, .12f, .6f},
       {.3f, .5f, .8f, .2f},
       {0, .35f, .5f, 0},
       {0, .15f, .1f, 0},
       {.75f, .5f, .63f, .1f},
       {.35f, .65f, .55f, .5f},
       {.4f, 1, .7f, .5f},
       {.4f, .5f, 0, .2f},
       {.4f, .85f, .25f, .2f},
       {.4f, 1, .3f, .4f}}};
  for (float rate : {44100.f, 48000.f})
    for (int algorithm = 4; algorithm < algorithmCount; ++algorithm) {
      Slot slot;
      assert(slot.prepare(rate, 0));
      auto p = controls[algorithm - 4];
      slot.prime(algorithm, p);
      double energy = 0;
      float maximum = 0;
      for (int n = 0; n < int(rate * 1.3f); ++n) {
        if (n % 128 == 0)
          slot.beginBlock(p);
        const float input = .04f * std::sin(n * 6.28318530718f * 220 / rate) +
                            .02f * std::sin(n * 6.28318530718f * 523 / rate);
        const auto output = slot.process({input, input * .7f}, p);
        for (float sample : output) {
          assert(std::isfinite(sample) && std::abs(sample) < 4);
          energy += sample * sample;
          maximum = std::max(maximum, std::abs(sample));
        }
      }
      std::fprintf(stderr, "Layers FX %d rate=%.0f energy=%.5f peak=%.5f\n",
                   algorithm, rate, energy, maximum);
      assert(energy > 1e-5 && maximum > 1e-5f);
      // Live macro edits, model changes and output fades stay finite.
      for (int block = 0; block < 120; ++block) {
        p[(block / 20) % 4] = (block % 40 < 20) ? .2f : .8f;
        slot.beginBlock(p);
        for (int n = 0; n < 128; ++n) {
          const float input = .035f * std::sin((block * 128 + n) * .021f);
          const auto out = slot.process({input, input * .6f}, p);
          for (float x : out)
            assert(std::isfinite(x) && std::abs(x) < 4);
        }
      }
      slot.request((algorithm + 1) % algorithmCount);
      for (int block = 0; block < 30; ++block) {
        slot.beginBlock(p);
        for (int n = 0; n < 128; ++n)
          slot.process({.01f, .007f}, p);
      }
      assert(slot.algorithm() == (algorithm + 1) % algorithmCount);
    }
  // Every Fracture processor and Shred circuit is actually audible.
  for (int algorithm : {6, 7})
    for (int mode = 0; mode < (algorithm == 6 ? 10 : 8); ++mode) {
      Slot slot;
      assert(slot.prepare(44100, 0));
      std::array<float, 4> p{float(mode) / (algorithm == 6 ? 9 : 7), .6f, .5f,
                             .5f};
      slot.prime(algorithm, p);
      double energy = 0;
      for (int n = 0; n < 24000; ++n) {
        if (n % 128 == 0)
          slot.beginBlock(p);
        const float input =
            .04f * std::sin(n * .039f) + .01f * std::sin(n * .093f);
        const auto out = slot.process({input, input * .8f}, p);
        for (float x : out) {
          assert(std::isfinite(x) && std::abs(x) < 4);
          energy += x * x;
        }
      }
      assert(energy > 1e-5);
    }
  // The displayed Buffer range reaches the actual core, without clipping at 2x.
  s3g::BufferProcessor core;
  core.prepare(44100, 2, 2.1);
  s3g::BufferProcessorParams cp;
  cp.rate = 4;
  core.setParams(cp);
  assert(core.params().rate == 4);
  cp.rate = .25f;
  core.setParams(cp);
  assert(core.params().rate == .25f);
  // The smooth-return effects must not introduce isolated edges under live
  // edits. Deliberate destruction in Fracture/Shred/Buffer is tested separately
  // above.
  for (int algorithm : {4, 9, 10}) {
    Slot slot;
    assert(slot.prepare(44100, 0));
    auto p = controls[algorithm - 4];
    slot.prime(algorithm, p);
    std::array<float, 2> last{};
    float maxStep = 0;
    double stereoDifference = 0;
    for (int n = 0; n < 96000; ++n) {
      if (n % 2048 == 0 && n > 22000) {
        const int field = (n / 2048) % 4;
        p[field] = p[field] > .5f ? .1f : .9f;
      }
      if (n % 128 == 0)
        slot.beginBlock(p);
      const auto out = slot.process(
          {.02f * std::sin(n * .027f), .017f * std::sin(n * .033f)}, p);
      if (n > 22000) {
        for (int c = 0; c < 2; ++c)
          maxStep = std::max(maxStep, std::abs(out[c] - last[c]));
        stereoDifference += std::abs(out[0] - out[1]);
      }
      last = out;
    }
    assert(maxStep < .025f && stereoDifference > 1);
  }
  auto t = layersFixture();
  setParam(t.get(), "layers_corner", "0");
  setParam(t.get(), "layers_fx0", "10");
  setParam(t.get(), "layers_fx1", ".75");
  setParam(t.get(), "layers_corner", "3");
  setParam(t.get(), "layers_fx0", "9");
  setParam(t.get(), "layers_fx1", "1");
  std::array<char, kStateTextSize> json{};
  assert(t->serializeState(json.data(), json.size()) > 0);
  auto copy = layersFixture();
  assert(copy->restoreState(json.data()));
  assert(copy->layersLive.values[LayerPatch::fx] == 10 &&
         copy->layersLive.values[LayerPatch::fx + 24] == 9);
  assert(copy->layersLive.values[LayerPatch::fx + 1] == .75f &&
         copy->layersLive.values[LayerPatch::fx + 25] == 1);
  t->beginClipRecording();
  setParam(t.get(), "layers_fx0", "5");
  renderSplit(*t, 100);
  t->finishClipRecording();
  t->applyClipCurves(*t->activeClipSlot(), 10);
  assert(t->layersRecorded.values[LayerPatch::fx + 24] == 5);
  std::puts("Layers expanded stereo FX, all Fracture/Shred models, live edits, "
            "type transitions, "
            "state and automation: PASS");
}

void testLayersDetailsAndBusRefinement() {
  using namespace s3g_thru_layers;
  const auto &table = s3g_daisy::ThruSpectralPlan::get();
  for (float phase : {-1e-8f, -1e-12f, 1e-8f, 1e-12f})
    assert(std::abs(table.sinCycle(phase) -
                    std::sin(6.283185307179586 * phase)) < 1e-7);
  assert(table.sinCycle(std::numeric_limits<float>::infinity()) == 0);
  assert(table.sinCycle(std::numeric_limits<float>::quiet_NaN()) == 0);
  // 2x resampling has a known delay and reduces folded nonlinear harmonics.
  Oversample2 resampler;
  int peakAt = -1;
  float peak = 0;
  for (int n = 0; n < 64; ++n) {
    const auto v = resampler.process({n == 0 ? 1.f : 0, n == 0 ? 1.f : 0},
                                     [](auto x) { return x; });
    assert(std::abs(v[0] - v[1]) < 1e-8f);
    if (std::abs(v[0]) > peak) {
      peak = std::abs(v[0]);
      peakAt = n;
    }
  }
  assert(peakAt == int(Oversample2::latency));
  resampler.reset();
  double directRe = 0, directIm = 0, filteredRe = 0, filteredIm = 0,
         fundamental = 0;
  for (int n = 0; n < 24000; ++n) {
    const float x = .4f * std::sin(n * 6.28318530718f * 9000 / 44100);
    const auto y = resampler.process({x, x}, [](auto z) {
      for (auto &v : z)
        v = std::tanh(v * 8);
      return z;
    });
    if (n > 4000) {
      const double angle = n * 6.283185307179586 * 17100 / 44100;
      directRe += std::tanh(x * 8) * std::cos(angle);
      directIm += std::tanh(x * 8) * std::sin(angle);
      filteredRe += y[0] * std::cos(angle);
      filteredIm += y[0] * std::sin(angle);
      fundamental += y[0] * y[0];
    }
  }
  assert(std::hypot(filteredRe, filteredIm) <
             std::hypot(directRe, directIm) * .4 &&
         fundamental > 100);
  // Long delay endpoints are real audio times, not just display extensions.
  for (int algorithm : {3, 4}) {
    Slot slot;
    assert(slot.prepare(44100, 0));
    const std::array<float, 4> p{1, algorithm == 4 ? 1.f : 0, .5f, 0};
    auto a = detailDefaults(algorithm);
    a[0] = 1;
    slot.prime(algorithm, p, a);
    double late = 0;
    float maxOutside = 0;
    for (int n = 0; n < 8 * 44100 + 1024; ++n) {
      if (n % 128 == 0)
        slot.beginBlock(p, a);
      const auto out = slot.process({n == 0 ? .1f : 0, n == 0 ? .1f : 0}, p, a);
      if (n >= 8 * 44100 + 510 && n <= 8 * 44100 + 520)
        late += std::abs(out[0]) + std::abs(out[1]);
      if (algorithm == 3 && n < 8 * 44100 + 510)
        maxOutside = std::max(maxOutside, std::abs(out[0]));
    }
    assert(late > 1e-4 && maxOutside < 1e-7f);
  }
  // Dense chords, live macros and both high-gain families remain
  // bounded/stereo.
  for (int algorithm : {6, 7}) {
    Slot slot;
    assert(slot.prepare(44100, 0));
    std::array<float, 4> p{algorithm == 6 ? 6.f / 9 : 0, .65f, .35f, .5f};
    auto a = detailDefaults(algorithm);
    slot.prime(algorithm, p, a);
    double energy = 0;
    float maximum = 0;
    for (int n = 0; n < 72000; ++n) {
      if (n % 4096 == 0) {
        p[1] = n % 8192 ? .25f : .8f;
        a[3] = n % 8192 ? .9f : .3f;
      }
      if (n % 128 == 0)
        slot.beginBlock(p, a);
      float x = 0;
      for (int v = 0; v < 8; ++v)
        x += .09f * std::sin(n * 6.28318530718f *
                                 (110 * std::pow(2.f, v / 12.f)) / 44100 +
                             v * .2f);
      const auto out = slot.process({x, x}, p, a);
      assert(std::abs(out[0] - out[1]) < 1e-6f || algorithm == 6);
      for (float v : out) {
        assert(std::isfinite(v));
        maximum = std::max(maximum, std::abs(v));
        energy += v * v;
      }
    }
    assert(energy > 1 && maximum < 1.6f);
  }
  // Spectral damping measurably removes brittle upper-band material; freeze
  // retains its own energy and live edits stay contained on a polyphonic bus.
  for (int algorithm : {2, 5}) {
    Slot dark, bright;
    assert(dark.prepare(44100, 0) && bright.prepare(44100, 0));
    const std::array<float, 4> p{.2f, .25f, .5f, .5f};
    auto a = detailDefaults(algorithm), b = a;
    a[1] = 0;
    b[1] = 1;
    dark.prime(algorithm, p, a);
    bright.prime(algorithm, p, b);
    double low = 0, high = 0;
    for (int n = 0; n < 40000; ++n) {
      if (n % 128 == 0) {
        dark.beginBlock(p, a);
        bright.beginBlock(p, b);
      }
      const float x = .05f * std::sin(n * 6.28318530718f * 9000 / 44100);
      const auto l = dark.process({x, x}, p, a),
                 h = bright.process({x, x}, p, b);
      if (n > 12000) {
        low += l[0] * l[0];
        high += h[0] * h[0];
      }
    }
    assert(high > 1e-4 && low < high * .65);
  }
  auto t = layersFixture();
  setParam(t.get(), "layers_corner", "2");
  setParam(t.get(), "layers_fx0", "7");
  const int edited = t->layersEditCorner;
  t->layersPadOn(31, 100);
  t->layersPadOff(31);
  assert(t->layersEditCorner == edited);
  for (int f = 8; f < 12; ++f) {
    char key[24];
    std::snprintf(key, sizeof(key), "layers_fx%d", f);
    setParam(t.get(), key, ".71");
  }
  setParam(t.get(), "layers_copy", "1");
  setParam(t.get(), "layers_corner", "0");
  setParam(t.get(), "layers_paste", "1");
  for (int f = 0; f < 4; ++f)
    assert(std::abs(t->layersLive.values[LayerPatch::advanced + f] - .71f) <
           1e-6f);
  std::array<char, kStateTextSize> json{};
  assert(t->serializeState(json.data(), json.size()) > 0);
  auto copy = layersFixture();
  assert(copy->restoreState(json.data()));
  for (int f = 0; f < 4; ++f)
    assert(copy->layersLive.values[LayerPatch::advanced + f] ==
           t->layersLive.values[LayerPatch::advanced + f]);
  // Earlier test builds stored only reserved prototype values here.
  t->layersLive.values[LayerPatch::advancedVersion] = 0;
  assert(t->serializeState(json.data(), json.size()) > 0);
  assert(copy->restoreState(json.data()));
  assert(copy->layersLive.values[LayerPatch::advancedVersion] ==
         LayerPatch::advancedTag);
  assert(copy->layersLive.values[LayerPatch::advanced] == detailDefaults(7)[0]);
  t->layersLive.upgrade();
  t->beginClipRecording();
  setParam(t.get(), "layers_fx8", ".22");
  renderSplit(*t, 80);
  t->finishClipRecording();
  t->applyClipCurves(*t->activeClipSlot(), 10);
  assert(std::abs(t->layersRecorded.values[LayerPatch::advanced] - .22f) <
         1e-5);
  setParam(t.get(), "layers_fx5", "8");
  assert(t->layersLive.values[LayerPatch::fx + 5] == 8);
  std::puts("Layers advanced controls, earlier-state upgrade, copy/clip "
            "recall, 8-second audio, 2x alias rejection, polyphonic drive and "
            "spectral damping: PASS");
}

void testLayersAudibleGlide() {
  using namespace s3g_thru_layers;
  // A constant source and different dry trims make the audible gain an exact
  // observable of the four smoothed branch weights (without other DSP effects).
  for (float glide : {5.f, 120.f, 1500.f}) {
    Rack rack;
    assert(rack.prepare(44100));
    LayerPatch patch;
    patch.values[LayerPatch::config + LayerPatch::glide] = glide;
    patch.values[LayerPatch::config + LayerPatch::zMode] = 1;
    for (int c = 0; c < 4; ++c) {
      patch.values[LayerPatch::fx + c * 8] = 0;
      patch.values[LayerPatch::fx + c * 8 + 7] = c == 0 ? .2f : .8f;
    }
    rack.prime(patch);
    std::array<std::array<float, 1>, 2> bus{{{.1f}, {.1f}}};
    for (int n = 0; n < 1024; ++n) {
      bus[0][0] = bus[1][0] = .1f;
      rack.process(1, bus, patch);
    }
    patch.values[LayerPatch::config + LayerPatch::weight0] = 0;
    patch.values[LayerPatch::config + LayerPatch::weight1] = 1;
    float previous = 0;
    for (int n = 0; n < 12000; ++n) {
      bus[0][0] = bus[1][0] = .1f;
      rack.process(1, bus, patch);
      const auto &w = rack.currentWeights();
      assert(w[1] >= previous && w[1] - previous < .005f);
      const float expected = .1f * (w[0] * .2f + (w[1] + w[2] + w[3]) * .8f);
      for (const auto &ch : bus)
        assert(std::abs(ch[0] - expected) < 1e-6f);
      previous = w[1];
    }
  }
  auto t = layersFixture();
  setParam(t.get(), "layers_glide", "1500");
  setParam(t.get(), "layers_z_mode", "1");
  std::FILE *fixture = nullptr;
  if (const auto *path = std::getenv("S3G_THRU_GLIDE_FIXTURE")) {
    fixture = std::fopen(path, "wb");
    assert(fixture);
    std::fputs("[", fixture);
  }
  bool comma = false;
  const auto snapshot = [&]() {
    if (!fixture)
      return;
    std::array<char, 4096> text{};
    assert(getParam(t.get(), "layers_view", text.data(), text.size()) > 0);
    assert(std::fprintf(fixture, "%s%s", comma ? "," : "", text.data()) > 0);
    comma = true;
  };
  renderSplit(*t, 10);
  const auto start = t->layersRack.currentWeights();
  snapshot();
  t->padNoteOn(68 + 31, 100);
  assert(t->layersRack.currentWeights() == start);
  snapshot();
  renderSplit(*t, 1);
  auto held = t->layersRack.currentWeights();
  assert(held[1] > start[1] && held[1] < .01f);
  snapshot();
  t->padNoteOff(68 + 31);
  assert(t->layersRack.currentWeights() == held);
  snapshot();
  renderSplit(*t, 10);
  auto released = t->layersRack.currentWeights();
  assert(released[1] > held[1] && released[1] < .1f);
  snapshot();
  t->shiftHeld = true;
  t->padNoteOn(68 + 7, 100);
  t->padNoteOff(68 + 7);
  t->shiftHeld = false;
  assert(t->layersLatch[7]);
  assert(t->layersRack.currentWeights() == released);
  snapshot();
  renderSplit(*t, 1);
  assert(t->layersRack.currentWeights()[3] > released[3] &&
         t->layersRack.currentWeights()[3] < .01f);
  snapshot();
  if (fixture) {
    std::fputs("]", fixture);
    assert(std::fclose(fixture) == 0);
  }
  std::puts("Layers glide: continuous audible branch gains, native "
            "press/release/latch and cursor snapshots: PASS");
}

void testPerformanceModeCopyReturn() {
  auto t = layersFixture();
  t->setPerformanceMode(0);
  const auto readReturn = [](ThruInstance &instance) {
    std::array<char, 32> b{};
    assert(getParam(&instance, "split_return_mode", b.data(), b.size()) > 0);
    return std::atoi(b.data());
  };
  for (int mode = 1; mode <= 5; ++mode) {
    t->controls.locus = .21f + mode * .09f;
    t->controls.field = .57f;
    t->controls.speed = .73f;
    t->controls.algorithm = Engine::PlaybackAlgorithm::grains;
    t->rootNote = 60 + mode;
    t->voiceMode = 2;
    const auto source = t->controls;
    const auto root = t->rootNote;
    t->setPerformanceMode(mode);
    assert(t->performanceMode() == mode && readReturn(*t) == mode);
    if (mode == 5) {
      assert(t->controls.locus == source.locus &&
             t->controls.speed == source.speed && t->rootNote == root);
    }
    t->setPerformanceMode(0);
    assert(t->performanceMode() == 0 && readReturn(*t) == mode);
    assert(t->controls.locus == source.locus &&
           t->controls.field == source.field &&
           t->controls.speed == source.speed &&
           t->controls.algorithm == source.algorithm);
    assert(t->rootNote == root && t->voiceMode == 2);
    std::array<char, kStateTextSize> text{};
    assert(t->serializeState(text.data(), text.size()) > 0);
    auto restored = layersFixture();
    assert(restored->restoreState(text.data()));
    assert(restored->performanceMode() == 0 && readReturn(*restored) == mode);
    restored->setPerformanceMode(readReturn(*restored));
    assert(restored->performanceMode() == mode);
    std::string legacy(text.data()),
        field = ",\"split_return_mode\":" + std::to_string(mode);
    auto pos = legacy.find(field);
    assert(pos != std::string::npos);
    legacy.erase(pos, field.size());
    assert(restored->restoreState(legacy.c_str()));
    assert(readReturn(*restored) >= 1 && readReturn(*restored) <= 5);
  }
  t->setPerformanceMode(5);
  setParam(t.get(), "layers_corner", "2");
  setParam(t.get(), "layers_fx0", "9");
  setParam(t.get(), "layers_fx2", ".71");
  const auto fx = t->layersLive.values;
  t->setPerformanceMode(0);
  t->controls.locus = .91f;
  t->rootNote = 73;
  t->setPerformanceMode(readReturn(*t));
  assert(t->performanceMode() == 5 && t->controls.locus == .91f &&
         t->rootNote == 73 && t->layersLive.values == fx);
  t->beginClipRecording();
  assert(t->clipRecording);
  t->setPerformanceMode(4);
  assert(t->performanceMode() == 5 && readReturn(*t) == 5);
  t->finishClipRecording();
  std::puts(
      "Copy mode return: exact layouts, saved Single recall, legacy state, "
      "recording guard and Single/Layers source/FX preservation: PASS");
}
void testLayersPitchAndRotary() {
  using namespace s3g_thru_layers;
  constexpr float pi = 6.283185307179586f;
  const float hundredMs = std::log(5.f) / std::log(9.f);
  for (float rate : {44100.f, 48000.f}) {
    // Measure the actual transposed tone at +/- one and two octaves.
    for (int semitones : {-24, -12, 12, 24}) {
      StereoPitch pitch;
      assert(pitch.prepare(rate));
      std::array<float, 4> p{float(semitones + 24) / 48, .5f, hundredMs, 0};
      pitch.beginBlock(p);
      const float wanted = 220 * std::exp2(semitones / 12.f);
      double re = 0, im = 0, energy = 0;
      int measured = 0;
      for (int n = 0; n < int(rate * 1.4f); ++n) {
        const float x = .08f * std::sin(n * pi * 220 / rate);
        const auto out = pitch.process({x, x});
        assert(std::isfinite(out[0]) && std::abs(out[0] - out[1]) < 1e-7f);
        if (n >= int(rate * .4f)) {
          const double phase = n * 6.283185307179586 * wanted / rate;
          re += out[0] * std::cos(phase);
          im += out[0] * std::sin(phase);
          energy += out[0] * out[0];
          ++measured;
        }
      }
      const double amplitude = std::hypot(re, im) * 2 / measured;
      std::fprintf(
          stderr,
          "Layers Pitch rate=%.0f transpose=%d measured %.5f at %.1f Hz\n",
          rate, semitones, amplitude, wanted);
      assert(amplitude > .055 && amplitude < .09 && energy > 10);
    }
    // Transparent settings add exactly the rack's fixed 512-frame delay.
    for (int algorithm : {11, 12}) {
      Slot slot;
      assert(slot.prepare(rate, 0));
      const std::array<float, 4> p =
          algorithm == 11 ? std::array<float, 4>{.5f, .5f, hundredMs, 0}
                          : std::array<float, 4>{.35f, 0, .5f, .5f};
      slot.prime(algorithm, p);
      std::array<std::array<float, 512>, 2> previous{};
      int at = 0;
      for (int n = 0; n < 16000; ++n) {
        const std::array<float, 2> in{.09f * std::sin(n * .031f),
                                      .07f * std::sin(n * .053f)};
        const auto out = slot.process(in, p);
        for (int c = 0; c < 2; ++c) {
          assert(std::abs(out[c] - previous[c][at]) < 1e-7f);
          previous[c][at] = in[c];
        }
        at = (at + 1) % 512;
      }
    }
    // Live direction/unison crossings, window/spread changes and all Rotary
    // controls retain continuity on a sustained, low-frequency stereo source.
    for (int algorithm : {11, 12}) {
      Slot slot, silent;
      assert(slot.prepare(rate, 0) && silent.prepare(rate, 1));
      std::array<float, 4> p =
          algorithm == 11 ? std::array<float, 4>{.5f, .5f, hundredMs, 0}
                          : std::array<float, 4>{.35f, .65f, .5f, .5f};
      auto a = detailDefaults(algorithm);
      slot.prime(algorithm, p, a);
      silent.prime(algorithm, p, a);
      float maxStep = 0;
      double energy = 0, stereo = 0;
      std::array<float, 2> previous{};
      for (int n = 0; n < int(rate * 2.5f); ++n) {
        if (n % 2048 == 0 && n > 16000) {
          const int field = (n / 2048) % 4;
          p[field] = n % 4096 ? .2f : .8f;
          a[field] = n % 4096 ? .15f : .85f;
          if (algorithm == 11 && field == 1)
            p[1] = n % 4096 ? .495f : .505f;
        }
        if (n % 128 == 0) {
          slot.beginBlock(p, a);
          silent.beginBlock(p, a);
        }
        const auto out = slot.process({.035f * std::sin(n * pi * 90 / rate),
                                       .027f * std::sin(n * pi * 140 / rate)},
                                      p, a);
        const auto empty = silent.process({0, 0}, p, a);
        assert(empty[0] == 0 && empty[1] == 0);
        if (n > 16000) {
          for (int c = 0; c < 2; ++c) {
            assert(std::isfinite(out[c]) && std::abs(out[c]) < .15f);
            maxStep = std::max(maxStep, std::abs(out[c] - previous[c]));
            energy += out[c] * out[c];
          }
          stereo += std::abs(out[0] - out[1]);
        }
        previous = out;
      }
      std::fprintf(stderr,
                   "Layers FX %d edit continuity rate=%.0f max step=%.6f\n",
                   algorithm, rate, maxStep);
      assert(maxStep < .012f && energy > 1 && stereo > 1);
      slot.reset();
      slot.beginBlock(p, a);
      for (int n = 0; n < 12000; ++n) {
        const auto out = slot.process({0, 0}, p, a);
        assert(out[0] == 0 && out[1] == 0);
      }
    }
    StereoPitch aliasGuard;
    assert(aliasGuard.prepare(rate));
    aliasGuard.beginBlock({1, .5f, hundredMs, 0});
    double aliased = 0;
    for (int n = 0; n < int(rate * .5f); ++n) {
      const float input = .08f * std::sin(n * pi * 18000 / rate);
      const auto out = aliasGuard.process({input, input});
      if (n > int(rate * .2f))
        aliased += out[0] * out[0];
    }
    assert(aliased / (rate * .3f) < 1e-6);
    // Independent detuning turns a mono source into a stereo field.
    StereoPitch detune;
    assert(detune.prepare(rate));
    detune.beginBlock({.5f, .5f, hundredMs, .24f});
    double difference = 0;
    for (int n = 0; n < int(rate); ++n) {
      const float input = .05f * std::sin(n * pi * 440 / rate);
      const auto out = detune.process({input, input});
      if (n > int(rate * .2f))
        difference += std::abs(out[0] - out[1]);
    }
    assert(difference > 10);
    RotaryLayer rotor;
    assert(rotor.prepare(rate));
    rotor.beginBlock({.5f, 1, .5f, 0}, detailDefaults(12));
    for (int n = 0; n < int(rate * .3f); ++n) {
      const auto out = rotor.process(
          {.1f * std::sin(n * .041f), .08f * std::sin(n * .029f)});
      assert(std::abs(out[0] - out[1]) < 1e-7f);
    }
  }
  // New assignments retain stable MIDI IDs, copy all controls, record both
  // algorithms and survive JSON plus the existing format-37 clip bank.
  auto t = layersFixture();
  setParam(t.get(), "layers_corner", "0");
  setParam(t.get(), "layers_fx0", "11");
  assert(t->layersLive.values[LayerPatch::fx + 1] == .5f &&
         t->layersLive.values[LayerPatch::fx + 2] == .5f);
  setParam(t.get(), "layers_fx5", "8");
  assert(t->layersLive.values[LayerPatch::fx + 5] == 4);
  setParam(t.get(), "layers_fx1", ".75");
  setParam(t.get(), "layers_copy", "1");
  setParam(t.get(), "layers_corner", "2");
  setParam(t.get(), "layers_paste", "1");
  assert(t->layersLive.values[LayerPatch::fx + 16] == 11 &&
         t->layersLive.values[LayerPatch::fx + 17] == .75f);
  setParam(t.get(), "layers_corner", "1");
  const auto &fx = midiParameter("layers_fx0");
  assert(fx.id == 3729 && fx.maximum == 16);
  midiWrite(*t, fx, 0, 12);
  assert(t->layersLive.values[LayerPatch::fx + 8] == 12);
  setParam(t.get(), "layers_fx8", ".7");
  setParam(t.get(), "layers_fx9", ".3");
  t->beginClipRecording();
  for (int pad : {0, 1, 2, 3, 8, 9, 10, 11})
    t->padNoteOn(68 + pad, 100);
  setParam(t.get(), "layers_fx_x", ".5");
  setParam(t.get(), "layers_fx_y", ".5");
  renderSplit(*t, 20);
  setParam(t.get(), "layers_fx3", ".8");
  setParam(t.get(), "layers_fx10", ".4");
  renderSplit(*t, 20);
  for (int pad : {0, 1, 2, 3, 8, 9, 10, 11})
    t->padNoteOff(68 + pad);
  t->finishClipRecording();
  assert(t->activeClipSlot()->clipNoteCount == 8);
  t->captureActiveClip(true);
  const auto expected = t->layersLive.values;
  std::array<char, kStateTextSize> text{};
  assert(t->serializeState(text.data(), text.size()) > 0);
  auto restored = layersFixture();
  assert(restored->restoreState(text.data()));
  assert(restored->layersLive.values == expected);
  char dir[] = "/tmp/thru-layers-modulation-XXXXXX";
  assert(mkdtemp(dir));
  t->persistenceEnabled = true;
  std::snprintf(t->moduleDirectory.data(), t->moduleDirectory.size(), "%s",
                dir);
  t->persistenceGeneration = 42;
  assert(t->writeClipFile());
  assert(restored->readClipFileFromPath(
      (std::string(dir) + "/thru-clips.bin").c_str()));
  restored->activateStoredClip(0, false);
  assert(restored->layersLive.values == expected);
  std::filesystem::remove_all(dir);
  std::puts("Layers Pitch/Detune and Rotary: measured octaves, transparent "
            "latency, stereo, live edits, independent histories, MIDI, "
            "copy/clip and project recall: PASS");
}

void testLayersRhythmEffects() {
  using namespace s3g_thru_layers;
  assert(rhythmMilliseconds(13, 0, false, 120) == 20);
  assert(rhythmMilliseconds(13, 1, false, 120) == 2000);
  assert(rhythmMilliseconds(15, 1, false, 120) == 2000);
  assert(rhythmMilliseconds(16, 1, false, 120) == 4000);
  assert(std::abs(rhythmMilliseconds(14, 1, false, 120) - 12.5f) < 1e-5f);
  assert(rhythmMilliseconds(13, 1, true, 40) == 6000);
  for (int algorithm = 1; algorithm < 17; ++algorithm) {
    std::array<float, 4> p{.5f, .5f, .5f, .5f}, a = p, original = p;
    pressureMacro(algorithm, 0, p, a);
    assert(p == original && a == original);
    pressureMacro(algorithm, .8f, p, a);
    assert(p != original || a != original);
    pressureMacro(algorithm, -1, p, a);
    for (float v : p)
      assert(v >= 0 && v <= 1);
    for (float v : a)
      assert(v >= 0 && v <= 1);
  }
  for (float rate : {44100.f, 48000.f}) {
    // Gating remains at the selected beat division on the shared clock.
    for (float bpm : {120.f, 240.f}) {
      Rhythm gate;
      assert(gate.prepare(rate, nullptr, 0));
      const std::array<float, 4> p{3.f / 7, .5f, 0, 0}, a{1, 0, .5f, 0};
      std::vector<int> rises;
      float prior = 1;
      for (int n = 0; n < int(rate * 1.2f); ++n) {
        if (n % 128 == 0) {
          gate.context({bpm, n * bpm / (60.0 * rate), -1, 0, false, true},
                       true);
          gate.beginBlock(14, p, a);
        }
        const auto out = gate.process({.2f, .1f});
        assert(std::abs(out[1] - out[0] * .5f) < 1e-7f);
        if (n > int(rate * .2f) && prior <= .1f && out[0] > .1f)
          rises.push_back(n);
        prior = out[0];
      }
      assert(rises.size() >= 6);
      for (std::size_t i = 1; i < rises.size(); ++i)
        assert(std::abs(float(rises[i] - rises[i - 1]) - rate * 15 / bpm) < 4);
    }
    Rhythm gate;
    assert(gate.prepare(rate, nullptr, 0));
    gate.context({}, true);
    gate.beginBlock(14, {.4f, 1, 0, 0}, {0, 0, .5f, 0});
    for (int n = 0; n < 12000; ++n) {
      const std::array<float, 2> x{.03f * std::sin(n * .03f),
                                   .04f * std::cos(n * .07f)};
      assert(gate.process(x) == x);
    }
    // Hard-to-repeat live edits exercise tiny slices, both resampling
    // directions, dense stereo inputs, geometry queues and bank handoffs.
    for (int algorithm = 13; algorithm <= 16; ++algorithm) {
      Slot slot, silent;
      assert(slot.prepare(rate, 0) && silent.prepare(rate, 1));
      std::array<float, 4> p{.3f, algorithm == 14 ? .6f : 1.f,
                             algorithm == 13 ? 1.f : .3f, .5f};
      auto a = detailDefaults(algorithm);
      a[0] = 0;
      slot.prime(algorithm, p, a);
      silent.prime(algorithm, p, a);
      std::array<float, 2> last{};
      float peak = 0, jump = 0;
      double energy = 0;
      for (int n = 0; n < int(rate * 2.5f); ++n) {
        if (n % 4096 == 0 && n > 16000) {
          const int field = (n / 4096) % 4;
          p[field] = n % 8192 ? .15f : .7f;
          if (algorithm == 13 && field == 3)
            p[3] = n % 8192 ? 0 : 1;
          a[(field + 1) % 4] = n % 8192 ? .2f : .8f;
          a[0] = 0;
        }
        if (n % 128 == 0) {
          slot.context({}, true);
          silent.context({}, true);
          slot.beginBlock(p, a);
          silent.beginBlock(p, a);
        }
        float x = 0;
        for (int voice = 0; voice < 8; ++voice)
          x += .015f * std::sin(n * .009f * (1 + voice * .13f));
        const auto out =
            slot.process({x, x * .8f + .009f * std::sin(n * .037f)}, p, a);
        const auto zero = silent.process({0, 0}, p, a);
        assert(zero[0] == 0 && zero[1] == 0);
        if (n > 16000)
          for (int ch = 0; ch < 2; ++ch) {
            assert(std::isfinite(out[ch]));
            peak = std::max(peak, std::abs(out[ch]));
            jump = std::max(jump, std::abs(out[ch] - last[ch]));
            energy += out[ch] * out[ch];
          }
        last = out;
      }
      std::fprintf(stderr, "Layers rhythm %d rate %.0f peak %.5f step %.6f\n",
                   algorithm, rate, peak, jump);
      assert(peak < .4f && jump < .04f && energy > .1);
      slot.reset();
      slot.context({}, true);
      slot.beginBlock(p, a);
      for (int n = 0; n < 10000; ++n) {
        const auto zero = slot.process({0, 0}, p, a);
        assert(zero[0] == 0 && zero[1] == 0);
      }
    }
  }
  // Choosing a held effect before playing arms it without freezing silence.
  for (int algorithm : {13, 15, 16}) {
    Rhythm armed;
    assert(armed.prepare(44100, nullptr, 0));
    const std::array<float, 4> p{.1f, 1, algorithm == 13 ? 1.f : 0.f, .5f},
        a{0, .5f, .2f, .2f};
    armed.context({}, true);
    armed.beginBlock(algorithm, p, a);
    for (int n = 0; n < 44100; ++n)
      assert(armed.process({0, 0})[0] == 0);
    assert(armed.captures() == 0);
    for (int n = 0; n < 22050; ++n) {
      const auto out = armed.process({.1f, .05f});
      assert(std::isfinite(out[0]));
    }
    assert(armed.captured());
  }
  // A held capture survives multiple writer wraps. Releasing HOLD takes
  // genuinely newer audio, rather than a second view into overwritten data.
  Rhythm repeat;
  assert(repeat.prepare(44100, nullptr, 0));
  std::array<float, 4> rp{std::log(6.25f) / std::log(100.f), 1, 1, .5f},
      ra{0, 0, 0, .25f};
  repeat.context({}, true);
  repeat.beginBlock(13, rp, ra);
  for (int n = 0; n < 44100 * 9; ++n) {
    const float x = n < 13230 ? 1.f : -.2f;
    const auto out = repeat.process({x, x * .5f});
    if (n > 44100) {
      assert(std::abs(out[0] - 1) < 1e-5f && std::abs(out[1] - .5f) < 1e-5f);
    }
  }
  assert(repeat.captured() && repeat.captures() == 1);
  rp[1] = 0;
  repeat.beginBlock(13, rp, ra);
  std::array<float, 2> newest{};
  for (int n = 0; n < 44100; ++n)
    newest = repeat.process({-.2f, -.1f});
  assert(repeat.captures() > 1 && std::abs(newest[0] + .2f) < 1e-5f);
  // Held repeats retime at a boundary after a BPM change without discarding
  // the captured sound or exposing stale writer history.
  Rhythm timed;
  assert(timed.prepare(44100, nullptr, 0));
  const std::array<float, 4> tp{3.f / 7, 1, 1, .5f}, ta{1, 0, 0, .25f};
  double beat = 0;
  for (int n = 0; n < 44100; ++n) {
    if (n % 64 == 0) {
      timed.context({120, beat, -1, 0, false, true}, true);
      timed.beginBlock(13, tp, ta);
    }
    timed.process({.1f, .1f});
    beat += 120.0 / (60 * 44100);
  }
  const auto captures = timed.captures();
  const double duration = timed.repeatSamples();
  assert(captures == 1 && std::abs(duration - 5512.5) < 2);
  for (int n = 0; n < 22050; ++n) {
    if (n % 64 == 0) {
      timed.context({240, beat, -1, 0, false, true}, true);
      timed.beginBlock(13, tp, ta);
    }
    const auto out = timed.process({.1f, .1f});
    assert(std::abs(out[0] - .1f) < 1e-6f);
    beat += 240.0 / (60 * 44100);
  }
  assert(timed.captures() == captures &&
         std::abs(timed.repeatSamples() * 2 - duration) < 2);
  // Pattern choices depend on saved seed and clip time, not old ring or RNG
  // state. A clip restart at a different transport beat reproduces the path.
  const auto path = [](float seed) {
    Rhythm shuffle;
    assert(shuffle.prepare(44100, nullptr, 0));
    const std::array<float, 4> p{std::log(500.f / 80) / std::log(25.f), 6.f / 7,
                                 .2f, .4f},
        a{0, .5f, seed, .2f};
    std::array<std::vector<int>, 2> paths;
    for (int launch = 0; launch < 2; ++launch) {
      for (int n = 0; n < 44100 * 2; ++n) {
        if (n % 64 == 0) {
          shuffle.context({120, launch * 20 + n / 22050.0, n / 44.1,
                           std::uint64_t(launch + 1), true, true},
                          true);
          shuffle.beginBlock(15, p, a);
        }
        shuffle.process(
            {.03f * std::sin(n * .031f), .04f * std::cos(n * .043f)});
        if (n % 2756 == 1400)
          paths[launch].push_back(shuffle.selectedSlice());
      }
    }
    assert(paths[0] == paths[1]);
    return paths[0];
  };
  assert(path(.17f) != path(.73f));
  auto t = layersFixture();
  setParam(t.get(), "layers_z_mode", "1");
  for (int corner = 0; corner < 4; ++corner) {
    setParam(t.get(), "layers_corner", std::to_string(corner).c_str());
    setParam(t.get(), "layers_fx0", std::to_string(13 + corner).c_str());
    setParam(t.get(), "layers_fx5", "9");
    setParam(t.get(), "layers_fx6", ".5");
  }
  t->beginClipRecording();
  for (int pad : {0, 1, 2, 3, 8, 9, 10, 11})
    t->padNoteOn(68 + pad, 100);
  setParam(t.get(), "layers_fx_x", ".5");
  setParam(t.get(), "layers_fx_y", ".5");
  renderSplit(*t, 100);
  setParam(t.get(), "layers_z", ".83");
  renderSplit(*t, 100);
  for (int pad : {0, 1, 2, 3, 8, 9, 10, 11})
    t->padNoteOff(68 + pad);
  t->finishClipRecording();
  t->captureActiveClip(true);
  assert(t->activeClipSlot()->clipNoteCount == 8);
  std::array<char, kStateTextSize> json{};
  assert(t->serializeState(json.data(), json.size()) > 0);
  auto copy = layersFixture();
  assert(copy->restoreState(json.data()));
  assert(copy->layersLive.values == t->layersLive.values);
  char folder[] = "/tmp/thru-rhythm-clips-XXXXXX";
  assert(mkdtemp(folder));
  t->persistenceEnabled = true;
  std::snprintf(t->moduleDirectory.data(), t->moduleDirectory.size(), "%s",
                folder);
  t->persistenceGeneration = 37;
  assert(t->writeClipFile());
  assert(copy->readClipFileFromPath(
      (std::string(folder) + "/thru-clips.bin").c_str()));
  copy->activateStoredClip(0, false);
  assert(copy->layersLive.values == t->layersLive.values);
  std::filesystem::remove_all(folder);
  std::puts("Layers rhythm: tempo/free timing, transparent Gate, live "
            "polyphonic edits, stable held banks, deterministic clip paths, "
            "pressure macros and format-37 recall: PASS");
}

// These inputs isolate implementation artifacts from the intended stutter,
// gating and reversal. Keep absolute DC jumps, pitch error and alias tests
// separate: a smooth envelope alone cannot detect a broken sample reader.
void testLayersRhythmArtifacts() {
  using namespace s3g_thru_layers;
  constexpr double twoPi = 6.283185307179586;
  const auto begin = [](Rhythm &fx, int alg, const std::array<float, 4> &p,
                        const std::array<float, 4> &a, bool engaged = true) {
    fx.context({}, engaged);
    fx.beginBlock(alg, p, a);
  };
  for (int rate : {44100, 48000}) {
    // Identical periodic history must sound identical at different offsets
    // in a long capture. Float absolute read positions used to lose precision.
    for (int alg : {13, 15}) {
      const auto audio = [&](int seconds) {
        Rhythm fx;
        assert(fx.prepare(float(rate), nullptr, 0));
        std::array<float, 4> p{std::log(6.25f) / std::log(100.f), 1, 1,
                               17.f / 48},
            a{0, 0, 0, .25f};
        if (alg == 15)
          p = {std::log(125.f / 80) / std::log(25.f), 0, 0, 0};
        begin(fx, alg, p, a, false);
        const auto input = [&](int n) {
          return float(.12 * std::sin((n % rate) * twoPi * 12000 / rate));
        };
        for (int n = 0; n < seconds * rate; ++n) {
          const float x = input(n);
          fx.process({x, x});
        }
        std::vector<float> out;
        for (int n = 0; n < 5000; ++n) {
          if (n % 127 == 0) {
            fx.context({120, n * 2.0 / rate, n * 1000.0 / rate, 1, true, true},
                       true);
            fx.beginBlock(alg, p, a);
          }
          const float x = input(n);
          out.push_back(fx.process({x, x})[0]);
        }
        return out;
      };
      const auto early = audio(1), late = audio(6);
      float error = 0;
      for (int n = 1000; n < 4500; ++n)
        error = std::max(error, std::abs(early[n] - late[n]));
      assert(error < 2e-6f);
    }
    {
      Rhythm shutter;
      assert(shutter.prepare(float(rate), nullptr, 0));
      const std::array<float, 4> p{std::log(20.f) / std::log(160.f), .55f, 0,
                                   0},
          a{0, 0, .5f, 0};
      begin(shutter, 14, p, a);
      float previous = 0, previous2 = 0, step = 0;
      double curvature = 0;
      for (int n = 0; n < rate; ++n) {
        const auto out = shutter.process({.25f, .125f});
        assert(out[1] == out[0] * .5f);
        if (n > rate / 4) {
          const double d = out[0] - 2 * previous + previous2;
          curvature += d * d;
          step = std::max(step, std::abs(out[0] - previous));
        }
        previous2 = previous;
        previous = out[0];
      }
      assert(step < .0012f && curvature < 1e-6);
      // Full-open pulses retain exact unity. Live PAN must not bypass the
      // gain smoothing and introduce a full-scale sample discontinuity.
      Rhythm pan;
      assert(pan.prepare(float(rate), nullptr, 0));
      auto pp = p, pa = a;
      pp[1] = 1;
      pa[2] = 0;
      begin(pan, 14, pp, pa);
      std::array<float, 2> last{}, final{};
      step = 0;
      for (int n = 0; n < rate; ++n) {
        if (n % 31 == 0) {
          pa[2] = n >= rate / 3 ? 1 : 0;
          begin(pan, 14, pp, pa);
        }
        final = pan.process({.25f, -.2f});
        if (n > rate / 4)
          for (int ch = 0; ch < 2; ++ch)
            step = std::max(step, std::abs(final[ch] - last[ch]));
        last = final;
      }
      assert(step < .0012f && std::abs(final[0]) < 2e-5f &&
             std::abs(final[1] + .2f) < 2e-5f);
    }
    // Re-enter near both zero-speed crossings. A new reader must not unmute
    // the outgoing stopped reader using the incoming reader's gain.
    for (float reentry : {.1f, .12f, .15f, .2f}) {
      Rhythm spool;
      assert(spool.prepare(float(rate), nullptr, 0));
      const std::array<float, 4> p{std::log(2.f) / std::log(80.f), 1, 1,
                                   std::log(2.f) / std::log(80.f)},
          a{0, 0, 0, .25f};
      begin(spool, 16, p, a, false);
      for (int n = 0; n < rate; ++n)
        spool.process({.25f, -.125f});
      begin(spool, 16, p, a);
      float last = .25f, step = 0;
      const int at = int(rate * reentry);
      for (int n = 0; n < rate / 2; ++n) {
        if (n == at)
          begin(spool, 16, p, a, false);
        if (n == at + 1)
          begin(spool, 16, p, a);
        const auto out = spool.process({.25f, -.125f});
        step = std::max(step, std::abs(out[0] - last));
        assert(std::abs(out[1] + out[0] * .5f) < 1e-6f);
        last = out[0];
      }
      assert(spool.captures() >= 2 && step < .003f);
    }
    // Changing overlap length must not repitch an ordinary forward or
    // reverse slice. Inspect the stable middle of a 125 ms slice at 1 kHz.
    for (float seam : {0.f, 1.f})
      for (float reverse : {0.f, 1.f}) {
        Rhythm tumble;
        assert(tumble.prepare(float(rate), nullptr, 0));
        const std::array<float, 4> p{std::log(500.f / 80) / std::log(25.f), 0,
                                     0, reverse},
            a{0, 0, 0, seam};
        begin(tumble, 15, p, a, false);
        const auto input = [&](int n) {
          return float(.1 * std::sin(twoPi * 1000 * n / rate));
        };
        for (int n = 0; n < rate; ++n) {
          const float x = input(n);
          tumble.process({x, x});
        }
        begin(tumble, 15, p, a);
        double error = 0, energy = 0;
        float prior = 0, prior2 = 0;
        for (int n = 0; n < rate / 8; ++n) {
          const float x = input(n), y = tumble.process({x, x})[0];
          if (n > rate * .025 && n < rate * .1) {
            const double d =
                y - 2 * std::cos(twoPi * 1000 / rate) * prior + prior2;
            error += d * d;
            energy += y * y;
          }
          prior2 = prior;
          prior = y;
        }
        assert(tumble.captured() && energy > 1 &&
               std::sqrt(error / energy) < 1e-5);
      }
    // Bright input just above the output Nyquist boundary exposed aliasing
    // missed by the older far-stopband test. Also protect audible passband.
    for (int pitch : {12, 24})
      for (int hz : {1000, pitch == 12 ? 12000 : 6000}) {
        Rhythm rerun;
        assert(rerun.prepare(float(rate), nullptr, 0));
        const std::array<float, 4> p{std::log(25.f) / std::log(100.f), 1, 1,
                                     float(pitch + 24) / 48},
            a{0, 0, 0, .25f};
        begin(rerun, 13, p, a, false);
        const auto input = [&](int n) {
          return float(.1 * std::sin(twoPi * hz * n / rate));
        };
        for (int n = 0; n < rate; ++n) {
          const float x = input(n);
          rerun.process({x, x});
        }
        begin(rerun, 13, p, a);
        double energy = 0;
        int count = 0;
        for (int n = 0; n < rate / 2; ++n) {
          const float x = input(n), y = rerun.process({x, x})[0];
          if (n > rate / 20) {
            energy += y * y;
            ++count;
          }
        }
        const double rms = std::sqrt(energy / count);
        assert(hz == 1000 ? rms > .06 && rms < .08 : rms < .0003);
      }
  }
  std::puts("Layers rhythm artifacts: long-capture precision, windowed Shutter "
            "edges/pan, Spool re-entry, pitch-neutral Tumble and near-fold "
            "Rerun alias rejection at 44.1/48 kHz: PASS");
}

void testLayersEngineList() {
  auto t = layersFixture();
  setParam(t.get(), "voice_engine_mode", "1");
  assert(t->voiceEngineSettings.enabled && t->engineSlotCount() == 8);
  setListSlot(*t, 0, 4); // Sample
  setListSlot(*t, 1, 2); // Grains
  setParam(t.get(), "voice_engine_order", "0");
  t->padNoteOn(68, 100);
  t->padNoteOn(69, 100);
  int held = 0;
  for (auto &v : t->voices)
    if (v.held) {
      assert(v.part == 0 && v.engine.loadedLaneMask() == 15u);
      assert(v.assignedAlgorithm == (v.engineSlot == 0 ? 4 : 2));
      assert(static_cast<int>(t->effectiveControls(v).algorithm) ==
             v.assignedAlgorithm);
      ++held;
    }
  assert(held == 2);
  renderSplit(*t, 8);
  for (auto &v : t->voices)
    if (v.held)
      assert(static_cast<int>(v.engine.playbackAlgorithm()) ==
             v.assignedAlgorithm);
  t->padNoteOn(68 + 31, 100); // Right FX pad changes no source ownership.
  assert(std::count_if(t->voices.begin(), t->voices.end(),
                       [](auto &v) { return v.held; }) == 2);
  renderSplit(*t, 8);
  t->padNoteOff(68 + 31);
  setListSlot(*t, 0, 3);
  for (auto &v : t->voices)
    if (v.held && v.engineSlot == 0)
      assert(v.assignedAlgorithm == 4);
  t->padNoteOff(68);
  t->padNoteOff(69);
  t->padNoteOn(68, 100);
  assert(newestVoice(*t, 0).assignedAlgorithm == 3);
  t->padNoteOff(68);
  t->setPerformanceMode(0);
  assert(t->voiceEngineSettings.enabled);
  t->setPerformanceMode(5);
  assert(t->voiceEngineSettings.enabled && t->voiceEngineSettings.engines[0] == 3);
  setListSlot(*t, 0, 4);
  setListSlot(*t, 1, 2);
  t->padNoteOn(68, 100);
  t->beginClipRecording();
  renderSplit(*t, 5);
  t->padNoteOn(69, 100);
  renderSplit(*t, 5);
  t->padNoteOff(68);
  t->padNoteOff(69);
  t->finishClipRecording();
  const auto *clip = t->activeClipSlot();
  assert(clip && clip->clipNoteCount >= 2 && clip->singleSettings.engines.enabled);
  bool sample = false, grains = false;
  for (int i = 0; i < clip->clipNoteCount; ++i) {
    const auto &note = clip->clipNotes[i];
    sample |= note.engineSlot == 0 && note.engineAlgorithm == 4;
    grains |= note.engineSlot == 1 && note.engineAlgorithm == 2;
  }
  assert(sample && grains);
  t->releaseClipVoices();
  t->triggerClipVoice(*clip, 0);
  assert(std::any_of(t->voices.begin(), t->voices.end(),
                     [](auto &v) { return v.active && v.clipOwned &&
                                            v.assignedAlgorithm >= 0; }));
  t->releaseClipVoices();
  setParam(t.get(), "voice_engine_assign", "2"); // Register uses left 4x4 keyboard.
  t->padNoteOn(68, 100);
  assert(newestVoice(*t, 0).engineSlot == 0);
  t->padNoteOn(68 + 27, 100);
  assert(newestVoice(*t, 0).engineSlot == 7);
  std::puts("Layers Engine List: eight slots, mixed voice rendering, FX ownership, clip recall and keyboard register: PASS");
}

void testLayers() {
  testLayersRhythmArtifacts();
  testLayersRhythmEffects();
  testLayersPitchAndRotary();
  testPerformanceModeCopyReturn();
  testLayersAudibleGlide();
  testLayersGeometryAndOwnership();
  testLayersEngineList();
  testLayersKeyboardAndLegato();
  testLayersParallelAudio();
  testLayersClipsAndPersistence();
  testLayersFxClipReplayAndIndicators();
  testLayersIdleDriveAndEncoderRecording();
  testLayersExpandedEffects();
  testLayersDetailsAndBusRefinement();
}

void testLayersSequenceModeCostIsolation() {
    auto t=layersFixture();
    t->padNoteOn(68+31,100);renderSplit(*t,2);t->padNoteOff(68+31);
    const auto runningWeights=t->layersRack.currentWeights();
    const auto patch=t->layersLive.values;
    std::array<const float*,4> samples{};std::array<std::size_t,4> lengths{};
    for(std::size_t lane=0;lane<4;++lane){samples[lane]=t->engine.laneData(lane,0);lengths[lane]=t->engine.laneFrames(lane);}
    t->setPerformanceMode(6);
    assert(t->sequenceMode()&&!t->layersLive.active);
    assert(t->layersRack.currentWeights()==runningWeights); // Inactive rack is untouched.
    assert(t->layersLive.values==patch);
    for(std::size_t i=0;i<t->voices.size();++i){
        assert(t->voices[i].part==int(i/2));
        for(std::size_t lane=0;lane<4;++lane)
            assert(t->voices[i].engine.laneLoaded(lane)==(lane==i/2&&lengths[lane]>0));
    }
    t->captureActiveClip(true);t->clipSlots[1]=t->clipSlots[0];
    for(int corner=0;corner<4;++corner)
        t->clipSlots[1].layers.values[LayerPatch::config+LayerPatch::weight0+corner]=corner==2?1:0;
    t->activateStoredClip(1,false);
    assert(t->sequenceMode()&&t->layersRack.currentWeights()==runningWeights);
    t->setPerformanceMode(5);
    assert(t->layersLive.active&&t->layersRack.currentWeights()[2]==1);
    for(std::size_t lane=0;lane<4;++lane)
        assert(t->engine.laneData(lane,0)==samples[lane]&&t->engine.laneFrames(lane)==lengths[lane]);
    t->padNoteOn(68,100);renderSplit(*t,10);t->padNoteOff(68);
    std::puts("Layers/Sequence transitions: inactive spectral histories untouched, patches/samples retained, Quad voice masks rebuilt and Layers re-entry primed: PASS");
}
