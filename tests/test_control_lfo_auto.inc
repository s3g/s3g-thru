void testControlLfoAuto() {
  using namespace s3g_thru_lfo;
  const auto &linear = destinations[14]; // Grain position spray: 0..1.
  for (float base : {0.f, .2f, .5f, .9f, 1.f}) {
    assert(mapped(linear, base, {0, 0, 0}) == base);
    for (float phase : {-.99f, -.7f, -.3f, 0.f, .3f, .7f, .99f})
      assert(std::abs(mapped(linear, base, {0, phase, 1}) - (phase + 1) * .5f) <
             1e-6f);
    assert(mapped(linear, base, {0, -1, 1}) == 0 &&
           mapped(linear, base, {0, 1, 1}) == 1);
  }
  assert(mapped(linear, 0, {0, 0, .2f}) == .1f);
  assert(std::abs(mapped(linear, 1, {0, 0, .2f}) - .9f) < 1e-6f);
  assert(mapped(linear, .5f, {0, -2, 2}) == 0 &&
         mapped(linear, .5f, {0, 2, 2}) == 1);
  // All destinations, including zero-based log ranges and manual sentinels.
  for (const auto &d : destinations) {
    if (d.domain == Domain::off)
      continue;
    for (float base : {d.low, (d.low + d.high) * .5f, d.high}) {
      assert(mapped(d, base, {0, 0, 0}) == base);
      float previous = d.low - 1;
      for (int n = 0; n <= 200; ++n) {
        const float value = mapped(d, base, {0, float(n) / 100 - 1, 1});
        assert(std::isfinite(value) && value >= d.low - 1e-4f &&
               value <= d.high + 1e-4f && value > previous);
        previous = value;
      }
    }
  }
  auto t = splitFixture();
  t->stopBackgroundWork();
  t->setPerformanceMode(0);
  setParam(t.get(), "algorithm", "2");
  setParam(t.get(), "grain_spray", "0");
  lfoAssign(*t, 0, 0, "grain_spray", 1);
  setParam(t.get(), "lfo1_offset", "1");
  setParam(t.get(), "lfo1_wave", "0");
  setParam(t.get(), "lfo1_rate", "1");
  setParam(t.get(), "lfo1_smooth", "3");
  renderSplit(*t, 100);
  float minimum = 1, maximum = 0;
  int zeros = 0;
  for (int n = 0; n < 360; ++n) {
    renderSplit(*t, 1);
    const auto m = t->controlLfoModulation(
        0, t->controlLfoDestinationForKey("grain_spray"), 2);
    const float x = mapped(linear, 0, m);
    minimum = std::min(minimum, x);
    maximum = std::max(maximum, x);
    zeros += x < 1e-4f;
  }
  assert(minimum < .08f && maximum > .9f && zeros < 20);
  const auto base = t->controls.grainSpray;
  auto controls = t->controls;
  t->applyControlLfoControls(controls, 0, Domain::engine);
  assert(t->controls.grainSpray == base && controls.grainSpray != base);
  const float shown = t->controlLfoValueForKey(0, "grain_spray", base, 2);
  assert(std::abs(shown - controls.grainSpray) < 1e-6f);
  // Mode changes and zero depth settle through existing SMTH, without latching
  // a center.
  setParam(t.get(), "lfo1_smooth", "20");
  float old = shown, largest = 0, sampleStep = 0;
  for (int toggle = 0; toggle < 6; ++toggle) {
    setParam(t.get(), "lfo1_offset", toggle % 2 ? "1" : "0");
    for (int n = 0; n < 50; ++n) {
      renderSplit(*t, 1);
      float previous = old;
      for (int frame = 0; frame <= 128; ++frame) {
        const float value = t->controlLfoValueForKey(0, "grain_spray", base, 2,
                                                     float(frame) / 128);
        sampleStep = std::max(sampleStep, std::abs(value - previous));
        previous = value;
      }
      const float x = t->controlLfoValueForKey(0, "grain_spray", base, 2);
      largest = std::max(largest, std::abs(x - old));
      old = x;
    }
  }
  assert(largest < .15f && sampleStep < .005f);
  setParam(t.get(), "lfo1_depth", "0");
  renderSplit(*t, 800);
  assert(t->controlLfoValueForKey(0, "grain_spray", base, 2) == base);
  setParam(t.get(), "lfo1_offset", "1");
  setParam(t.get(), "lfo1_depth", "-1");
  setParam(t.get(), "lfo1_mode", "3");
  t->triggerControlLfos(0);
  renderSplit(*t, 500);
  assert(!t->controlLfoRuntime[0].running &&
         std::abs(t->controlLfoValueForKey(0, "grain_spray", base, 2) - base) <
             1e-5f);
  // Static clip setting like FLOW: changing it does not add generated gestures.
  setParam(t.get(), "lfo1_mode", "0");
  t->beginClipRecording();
  const auto points = t->activeClipSlot()->clipPointCount;
  setParam(t.get(), "lfo1_offset", "0");
  setParam(t.get(), "lfo1_offset", "1");
  assert(t->activeClipSlot()->clipPointCount == points);
  renderSplit(*t, 50);
  t->finishClipRecording();
  auto json = std::make_unique<std::array<char, kStateTextSize>>();
  assert(t->serializeState(json->data(), json->size()) > 0);
  auto copy = splitFixture();
  copy->stopBackgroundWork();
  assert(copy->restoreState(json->data()) &&
         copy->controlLfoPool[0].offset == 1);
  assert(copy->controlLfoPool[0].spanAnchor == t->controlLfoPool[0].spanAnchor);
  for (int i = 0; i < 8; ++i) {
    const auto &a = t->controlLfoPool[0].poolRoutes[i];
    const auto &b = copy->controlLfoPool[0].poolRoutes[i];
    assert(a.automatic == b.automatic && a.span == b.span);
  }
  char dir[] = "/tmp/thru-auto.XXXXXX";
  assert(mkdtemp(dir));
  std::snprintf(t->moduleDirectory.data(), t->moduleDirectory.size(), "%s",
                dir);
  t->persistenceEnabled = true;
  t->persistenceGeneration = 1;
  t->captureActiveClip(true);
  assert(t->writeClipFile());
  const auto path = std::string(dir) + "/thru-clips.bin";
  assert(copy->readClipFileFromPath(path.c_str()) &&
         copy->controlLfoPool[0].offset == 1);
  std::ifstream in(path, std::ios::binary);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
  bytes.resize(bytes.size() - 4);
  stripLfoAuto(bytes);
  assert(bytes[8] == 48);
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  }
  assert(appendFileChecksum(path.c_str()));
  assert(copy->readClipFileFromPath(path.c_str()));
  for (const auto &c : copy->controlLfoPool)
    assert(c.offset == 0);
  const auto before = t->splitFocus;
  nrpn(*t, 16, "lfo8_offset", 16383);
  assert(t->controlLfoPool[7].offset == 1 && t->splitFocus == before);
  t.reset();
  copy.reset();
  std::filesystem::remove_all(dir);
  std::puts("Control LFO AUTO: full signed sweep/all destinations, zero-based "
            "log controls, mixed routes, smooth switches, neutral "
            "depth/one-shot, visible/base isolation, static clip ownership, "
            "JSON/format49, legacy48 and NRPN: PASS");
}
