#include "synth_v4_fixture.h"
void testSynthWorkshopCore() {
  using namespace s3g_thru_synth;
  std::vector<std::uint8_t> old;
  for (std::size_t i = 0; i < sizeof(synthV3Hex) - 1; i += 2) {
    unsigned n = 0;
    assert(std::sscanf(synthV3Hex + i, "%2x", &n) == 1);
    old.push_back(std::uint8_t(n));
  }
  Recipe r;
  assert(decode(old.data(), old.size(), r) && r.type == fm &&
         r.fmOsc[fAlgorithm] == 2 && r.fmOsc[fRatio2] == 1.5f &&
         (r.detailLocks & (1ULL << 61)));
  assert(encode(r).size() == recipeBytes);
  old.clear();
  for (std::size_t i = 0; i < sizeof(synthV4Hex) - 1; i += 2) {
    unsigned value = 0;
    assert(std::sscanf(synthV4Hex + i, "%2x", &value) == 1);
    old.push_back(std::uint8_t(value));
  }
  assert(old.size() == fourthRecipeBytes && decode(old.data(), old.size(), r));
  assert(r.type == drum && r.drums[dModel] == 3 && r.drums[dTune] == 630 &&
         r.drums[dForm] == 0 && r.drums[dContactX] == .6f &&
         r.drums[dContactY] == .4f && r.drums[dExtent] == .7f &&
         (r.extendedLocks & (1u << 19)));
  assert(encode(r).size() == recipeBytes && recipeBytes == 1408);

  SourceAudio p, ir;
  p.rate = ir.rate = 16000;
  p.left = {.2f, -.3f, .1f, .5f, -.2f};
  p.right = {-.4f, .7f, .2f, .1f, .3f};
  ir.left = {1, .25f, -.5f};
  ir.right = {.5f, .2f, .1f};
  std::vector<float> l, rr;
  const auto no = [] { return false; };
  const auto progress = [](unsigned) {};
  assert(convolveStereo(p, ir, 100, true, l, rr, no, progress) &&
         l.size() == 7);
  for (int channel = 0; channel < 2; ++channel) {
    const auto &a = channel ? p.right : p.left;
    const auto &b = channel ? ir.right : ir.left;
    const auto &out = channel ? rr : l;
    for (std::size_t i = 0; i < out.size(); ++i) {
      float expected = 0;
      for (std::size_t j = 0; j < a.size(); ++j)
        if (i >= j && i - j < b.size())
          expected += a[j] * b[i - j];
      assert(std::abs(out[i] - expected) < 2e-6f);
    }
  }
  std::vector<float> a, b;
  assert(convolveStereo(ir, p, 100, true, a, b, no, progress));
  for (std::size_t i = 0; i < a.size(); ++i)
    assert(std::abs(a[i] - l[i]) < 1e-6f && std::abs(b[i] - rr[i]) < 1e-6f);
  assert(convolveStereo(p, ir, 100, false, a, b, no, progress) &&
         a.size() == p.left.size());
  assert(convolveStereo(p, ir, 2, true, a, b, no, progress) && a.size() == 2 &&
         std::abs(a[1] - l[1]) < 1e-6f);
  SourceAudio delta;
  delta.rate = 16000;
  delta.left = delta.right = {1};
  assert(convolveStereo(p, delta, 100, true, a, b, no, progress));
  for (std::size_t i = 0; i < a.size(); ++i)
    assert(std::abs(a[i] - p.left[i]) < 1e-6f &&
           std::abs(b[i] - p.right[i]) < 1e-6f);
  assert(convolveStereo(p, p, 100, true, a, b, no, progress) && a.size() == 9);
  p.left.resize(4000);
  p.right.resize(4000);
  ir.left.assign(800, 0);
  ir.right.assign(800, 0);
  ir.left[0] = 1;
  ir.left[290] = .5f;
  ir.right[0] = .8f;
  ir.right[430] = .4f;
  for (std::size_t i = 0; i < p.left.size(); ++i) {
    p.left[i] = .2f * std::sin(Plan::tau * 220 * i / 16000);
    p.right[i] = .2f * std::cos(Plan::tau * 330 * i / 16000);
  }
  r = {};
  r.type = convolution;
  r.v[length] = 12;
  r.v[width] = 1;
  r.v[attack] = r.v[release] = 0;
  Audio out, other;
  const auto draw = [&](const Recipe &recipe, Audio &audio) {
    return render(recipe, 16000, audio, no, progress, &p, &ir);
  };
  const auto finite = [](const Audio &audio) {
    double power = 0;
    for (std::size_t i = 0; i < audio.left.size(); ++i) {
      assert(std::isfinite(audio.left[i]) && std::isfinite(audio.right[i]) &&
             std::abs(audio.left[i]) < .892f &&
             std::abs(audio.right[i]) < .892f);
      power += audio.left[i] * audio.left[i];
    }
    return power / std::max<std::size_t>(1, audio.left.size());
  };
  assert(draw(r, out) && out.left.size() == 4799 && finite(out) > .001 &&
         out.left != out.right);
  r.conv[cTail] = 0;
  assert(draw(r, other) && other.left.size() == 4000);
  r.conv[cTail] = 1;
  r.v[length] = .1f;
  assert(draw(r, other) && other.left.size() == 1600);
  r.v[length] = 12;
  for (int w = 0; w < 4; ++w) {
    r.conv[cWindow] = float(w);
    assert(draw(r, other) && finite(other) > .0001);
  }
  r.conv[cWindow] = 0;
  r.conv[cTone] = 1;
  assert(draw(r, other) && other.left != out.left);
  r.conv[cTone] = 0;
  r.conv[cMix] = 0;
  assert(draw(r, other) && other.left != out.left);
  r.v[width] = 0;
  assert(draw(r, other) && other.left == other.right);
  r.v[width] = 1;
  r.conv[cMix] = 1;
  r.v[loop] = 1;
  assert(draw(r, other) && other.left.size() < out.left.size() &&
         finite(other) > .001);
  r.v[loop] = 0;
  assert(!render(r, 16000, other, no, progress, &p));
  assert(!render(
      r, 16000, other, [] { return true; }, progress, &p, &ir));
  int calls = 0;
  assert(!convolveStereo(
      p, ir, 10000, true, a, b, [&] { return ++calls > 10; }, progress));
  r = {};
  r.type = drum;
  r.v[length] = .3f;
  r.v[attack] = 0;
  // Full pre-change mono renders, captured at 16 kHz. Compare samples with
  // a tight numerical tolerance: sanitizer/optimizer FP contraction can vary
  // the last float bits even when the resonator source is unchanged.
  const auto fixturePath = std::filesystem::path(__FILE__).parent_path() /
                           "fixtures/drum-v4-reference.f32";
  std::ifstream referenceFile(fixturePath, std::ios::binary);
  assert(referenceFile);
  const std::vector<unsigned char> reference(
      (std::istreambuf_iterator<char>(referenceFile)), {});
  assert(reference.size() == 6u * 4800u * 4u);
  std::vector<float> previous;
  for (int model = 0; model < drumModelCount; ++model) {
    assert(setDrum(r, dModel, float(model)));
    r.v[width] = 0;
    assert(render(r, 16000, out, no, progress) &&
           render(r, 16000, other, no, progress) && out.left == other.left &&
           out.left == out.right && finite(out) > .00001);
    if (!previous.empty())
      assert(previous != out.left);
    previous = out.left;
    if (model < 6) {
      Recipe legacy;
      legacy.type = drum;
      legacy.v[length] = .3f;
      legacy.v[attack] = 0;
      legacy.drums = {0,  48, 600, .7f, .25f, .2f, .5f, 0,
                      12, 40, .3f, .4f, 0,    .6f, .4f, .7f};
      legacy.drums[dModel] = float(model);
      legacy.drums[dTune] = drumTuneDefault[model];
      legacy.drumFlags = 1;
      legacy.drumExtra = legacyDrumAttack(legacy);
      assert(valid(legacy) && render(legacy, 16000, other, no, progress));
      assert(other.left.size() == 4800 && other.left == other.right);
      double errorPower = 0;
      float maxError = 0;
      for (std::size_t i = 0; i < other.left.size(); ++i) {
        const auto bits =
            le32(reference.data() + (std::size_t(model) * 4800 + i) * 4);
        float before;
        std::memcpy(&before, &bits, 4);
        const float error = std::abs(other.left[i] - before);
        maxError = std::max(maxError, error);
        errorPower += double(error) * error;
      }
      assert(maxError < 2e-5f &&
             std::sqrt(errorPower / other.left.size()) < 2e-6);
    }

    r.v[width] = 1;
    assert(render(r, 16000, other, no, progress) && finite(other) > .00001);
    if (model >= 6) {
      assert(other.left != other.right);
      const auto encoded = encode(r);
      Recipe reopened;
      assert(decode(encoded.data(), encoded.size(), reopened) &&
             encode(reopened) == encoded);
      Audio copy;
      assert(render(reopened, 16000, copy, no, progress) &&
             copy.left == other.left && copy.right == other.right);
    }

    assert(setDrum(r, dTune, 1e6f) && r.drums[dTune] == drumTuneHigh[model]);
    assert(setDrum(r, dDecay, 1e6f) && r.drums[dDecay] == drumDecayHigh[model]);
    assert(setDrum(r, dTune, drumTuneDefault[model]));
    assert(!setDrum(r, dTone, NAN));
    assert(!render(
        r, 16000, other, [] { return true; }, progress));
  }
  r.extendedLocks |= 1u << 15;
  r.drums[dCharacter] = .234f;
  mutate(r, .5f);
  assert(r.drums[dCharacter] == .234f);
  clean(r);
  assert(r.drums[dCharacter] == 0);
  for (float model : {NAN, INFINITY, -1.f, 11.f, 2.5f}) {
    Recipe malformed;
    malformed.drums[dModel] = model;
    assert(!valid(malformed));
    const auto bytes = encode(malformed);
    Recipe unchanged;
    const auto before = encode(unchanged);
    assert(!decode(bytes.data(), bytes.size(), unchanged) &&
           encode(unchanged) == before);
  }
  // Every shape/material/exciter is a real rendered alternative, not just a
  // label.
  for (int model = 6; model < drumModelCount; ++model) {
    Recipe base;
    base.type = drum;
    base.v[length] = .18f;
    base.v[attack] = 0;
    base.v[width] = 1;
    assert(setDrum(base, dModel, float(model)));
    const int forms = model == 6 ? 5 : model == 7 ? 16 : model == 9 ? 3 : 4;
    std::vector<float> first;
    for (int form = 0; form < forms; ++form) {
      assert(setDrum(base, dForm, float(form)) && valid(base));
      assert(render(base, 16000, out, no, progress) && finite(out) > .00001);
      if (form == 0)
        first = out.left;
      else
        assert(first != out.left);
    }
    base.extendedLocks |= (1u << 23);
    const auto extent = base.drums[dExtent];
    mutate(base, .5f);
    assert(base.drums[dExtent] == extent && valid(base));
    const auto encoded = encode(base);
    Recipe reopened;
    assert(decode(encoded.data(), encoded.size(), reopened) &&
           reopened.extendedLocks == base.extendedLocks);
  }
  {
    Recipe modal;
    modal.type = drum;
    assert(setDrum(modal, dModel, 7));
    modal.drums[dTune] = 220;
    modal.drums[dDecay] = 2000;
    ModalBodyDrumSample core(modal, 44100);
    assert(std::abs(core.core.bodyModeFrequencyHz(0, 0) - 220) < .01f);
    assert(std::abs(core.core.modeDecaySeconds(0) - 2) < .001f);
  }
  int middle = 0;
  for (int seed = 1; seed <= 32; ++seed) {
    Recipe candidate;
    candidate.type = drum;
    candidate.seed = seed;
    mutate(candidate, .25f);
    middle += candidate.drums[dTune] > 20 && candidate.drums[dTune] < 180;
  }
  assert(middle > 24);
  std::puts("Synth workshop DSP: captured v3 migration, direct/FFT stereo "
            "identity, commutativity, autoconvolution, full/trim/capped tails, "
            "windows/tone/mix, loop seam, cancellation and eleven deterministic "
            "stereo drum models with bounded tuning/decay: PASS");
}
void testSynthWorkshopWorkflow() {
  using namespace s3g_thru_synth;
  char dir[] = "/tmp/thru-workshop.XXXXXX";
  assert(mkdtemp(dir));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(dir));
  t->stopBackgroundWork();
  std::snprintf(t->synthDirectory.data(), t->synthDirectory.size(),
                "%s/captures/Synths", dir);
  setParam(t.get(), "synth_open", "1");
  setParam(t.get(), "synth_type", "0"); // Retained legacy Spline codec.
  waitSynth(*t);
  const auto spline = encode(t->synthRecipe);
  std::vector<std::string> frames;
  const auto remember = [&](const char *label) {
    std::array<char, 4096> text{};
    assert(t->formatSynth(text.data(), text.size()) > 0 &&
           std::strlen(text.data()) < 3800);
    frames.push_back(std::string("{\"label\":\"") + label +
                     "\",\"frame\":" + text.data() + "}");
  };
  setParam(t.get(), "synth_type", "7");
  assert(!t->synthWantsRender && t->synthRecipe.v[length] == 12);
  remember("conv-empty");
  setParam(t.get(), "synth_preview", "1");
  assert(!t->synthPreviewWanted);
  const auto primary = std::string(dir) + "/primary.wav",
             impulse = std::string(dir) + "/impulse.wav";
  std::vector<float> l(4000), r(4000);
  for (std::size_t i = 0; i < l.size(); ++i) {
    l[i] = .2f * std::sin(Plan::tau * 220 * i / 44100);
    r[i] = .2f * std::cos(Plan::tau * 330 * i / 44100);
  }
  assert(ThruInstance::writeStereoWavFile(l.data(), r.data(), l.size(), 44100,
                                          primary.c_str(),
                                          (primary + ".tmp").c_str(), true));
  l.assign(500, 0);
  r = l;
  l[0] = 1;
  l[200] = .3f;
  r[0] = .8f;
  r[320] = .4f;
  assert(ThruInstance::writeStereoWavFile(l.data(), r.data(), l.size(), 44100,
                                          impulse.c_str(),
                                          (impulse + ".tmp").c_str(), true));
  const auto lane = t->selectedLane;
  setParam(t.get(), "synth_primary_file", primary.c_str());
  setParam(t.get(), "synth_impulse_file", impulse.c_str());
  waitSynth(*t);
  remember("conv-ready");
  assert(t->selectedLane == lane && !t->engine.laneLoaded(lane));
  const auto expected = encode(t->synthRecipe);
  const auto original = t->synthWorker->audio[t->synthActive].left;
  assert(t->synthWorker->audio[t->synthActive].source &&
         t->synthWorker->audio[t->synthActive].impulse);
  setParam(t.get(), "synth_name", "Convolution test");
  setParam(t.get(), "synth_save", "1");
  waitSynth(*t, false);
  for (int i = 0; i < 10000 && !t->engine.laneLoaded(lane); ++i) {
    renderSplit(*t, 1);
    usleep(500);
  }
  assert(t->engine.laneLoaded(lane));
  const std::string saved = t->synthLastSaved.data();
  std::filesystem::remove(primary);
  std::filesystem::remove(impulse);
  setParam(t.get(), "synth_type", "0");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == spline);
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == expected &&
         t->synthWorker->audio[t->synthActive].left == original);
  const auto broken = saved + ".bad.wav";
  std::filesystem::copy_file(saved, broken);
  {
    std::ifstream in(broken, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
    bool changed = false;
    for (std::size_t i = 12; i + 32 < bytes.size(); ++i)
      if (!std::memcmp(bytes.data() + i, "sgSi", 4)) {
        bytes[i + 25] ^= 0x40;
        changed = true;
        break;
      }
    assert(changed);
    std::ofstream file(broken, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  }
  setParam(t.get(), "synth_type", "0");
  waitSynth(*t);
  const auto keep = encode(t->synthRecipe);
  std::snprintf(t->currentLaneSourcePaths[lane].data(), kPersistencePathSize,
                "%s", broken.c_str());
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t, false);
  assert(encode(t->synthRecipe) == keep);
  std::snprintf(t->currentLaneSourcePaths[lane].data(), kPersistencePathSize,
                "%s", saved.c_str());
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == expected);
  t->copyCurrentSourcesToClip(*t->activeClipSlot());
  t->captureActiveClip(true);
  assert(t->savePersistentState() && t->exportProjectFile());
  const std::string archive = t->projectOutputPath.data();
  t.reset();
  std::filesystem::remove_all(std::string(dir) + "/captures");
  const auto restored = std::string(dir) + "/restored";
  std::filesystem::create_directory(restored);
  t = std::make_unique<ThruInstance>();
  assert(t->prepare(restored.c_str()));
  t->stopBackgroundWork();
  assert(t->savePersistentState());
  t->projectGeneration = t->reservePersistenceGeneration();
  std::snprintf(t->projectArchivePath.data(), kPersistencePathSize, "%s",
                archive.c_str());
  assert(t->importProjectFile() && t->savePersistentState());
  t.reset();
  t = std::make_unique<ThruInstance>();
  assert(t->prepare(restored.c_str()));
  t->stopBackgroundWork();
  for (int i = 0; i < 10000 && !t->engine.laneLoaded(lane); ++i) {
    renderSplit(*t, 1);
    usleep(500);
  }
  assert(t->engine.laneLoaded(lane));
  setParam(t.get(), "record_lane", std::to_string(lane + 1).c_str());
  setParam(t.get(), "synth_open", "1");
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t);
  if (encode(t->synthRecipe) != expected ||
      t->synthWorker->audio[t->synthActive].left != original)
    std::fprintf(stderr,
                 "Conv import mismatch type=%d lane=%zu path=%s msg=%s\n",
                 t->synthRecipe.type, std::size_t(t->selectedLane),
                 t->currentLaneSourcePaths[t->selectedLane].data(),
                 t->synthMessage.data());
  assert(encode(t->synthRecipe) == expected &&
         t->synthWorker->audio[t->synthActive].left == original);
  // Imported projects can have deferred capture publication; exercise the
  // normal background service.
  assert(t->startBackgroundWork());
  setParam(t.get(), "synth_type", "8");
  setParam(t.get(), "synth_edit", "0,0.25");
  setParam(t.get(), "synth_drum", "0,1");
  setParam(t.get(), "synth_drum", "1,210");
  waitSynth(*t);
  remember("drum");
  const auto drumRecipe = encode(t->synthRecipe);
  const auto drumAudio = t->synthWorker->audio[t->synthActive].left;
  assert(t->persistencePath("captures/Synths", t->synthDirectory.data(),
                            t->synthDirectory.size()));
  setParam(t.get(), "synth_name", "Drum test");
  setParam(t.get(), "synth_save", "1");
  waitSynth(*t, false);
  const std::string drumSaved = t->synthLastSaved.data();
  for (int i = 0;
       i < 15000 &&
       (t->sampleLoadBusy() || t->synthLoadPath[0] ||
        drumSaved != t->currentLaneSourcePaths[t->selectedLane].data());
       ++i) {
    renderSplit(*t, 1);
    usleep(500);
  }
  if (drumSaved != t->currentLaneSourcePaths[t->selectedLane].data())
    std::fprintf(stderr, "Drum load incomplete expected=%s actual=%s msg=%s\n",
                 drumSaved.c_str(),
                 t->currentLaneSourcePaths[t->selectedLane].data(),
                 t->synthMessage.data());
  if (drumSaved != t->currentLaneSourcePaths[t->selectedLane].data()) {
    std::fprintf(stderr,
                 "Load diagnostics state=%d error=%s pending=%u clear=%u "
                 "master=%u copy=%zu frames=%zu request=%s\n",
                 t->sampleLoadState.load(), t->sampleLoadError.data(),
                 t->pendingCaptureSaveMask, t->pendingClearMask,
                 t->engine.loadedLaneMask(), t->sampleCopyFrame,
                 t->sampleStageFrames, t->sampleRequestPath.data());
    for (const auto &v : t->voices)
      std::fprintf(stderr, "voice loaded=%u active=%d\n",
                   v.engine.loadedLaneMask(), v.active ? 1 : 0);
  }
  assert(drumSaved == t->currentLaneSourcePaths[t->selectedLane].data());
  setParam(t.get(), "synth_type", "0");
  waitSynth(*t);
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == drumRecipe &&
         t->synthWorker->audio[t->synthActive].left == drumAudio);
  // Recall a genuine v4 Drum recipe through the worker WAV reader.
  {
    std::vector<std::uint8_t> old;
    for (std::size_t i = 0; i < sizeof(synthV4Hex) - 1; i += 2) {
      unsigned value = 0;
      assert(std::sscanf(synthV4Hex + i, "%2x", &value) == 1);
      old.push_back(std::uint8_t(value));
    }
    std::vector<std::uint8_t> wav{'R', 'I', 'F', 'F'};
    put(wav, std::uint32_t(12 + old.size()));
    for (char c : std::string("WAVEsgSy"))
      wav.push_back(std::uint8_t(c));
    put(wav, std::uint32_t(old.size()));
    wav.insert(wav.end(), old.begin(), old.end());
    const auto path = std::string(dir) + "/legacy-v4.wav";
    {
      std::ofstream file(path, std::ios::binary);
      file.write(reinterpret_cast<const char *>(wav.data()), wav.size());
    }
    std::snprintf(t->currentLaneSourcePaths[t->selectedLane].data(),
                  kPersistencePathSize, "%s", path.c_str());
    setParam(t.get(), "synth_restore", "1");
    waitSynth(*t);
    assert(t->synthRecipe.drums[dModel] == 3 &&
           t->synthRecipe.drums[dTune] == 630);
    std::snprintf(t->currentLaneSourcePaths[t->selectedLane].data(),
                  kPersistencePathSize, "%s", drumSaved.c_str());
  }
  for (int model = 6; model < drumModelCount; ++model) {
    // Bound the worker roundtrip duration independently of the legacy recipe
    // opened above. Full-length modal synthesis is covered by the core tests.
    setParam(t.get(), "synth_edit", "0,0.25");
    setParam(t.get(), "synth_drum", ("0," + std::to_string(model)).c_str());
    setParam(t.get(), "synth_drum", "12,1");
    setParam(t.get(), "synth_drum", "13,0.72");
    setParam(t.get(), "synth_drum", "14,0.24");
    setParam(t.get(), "synth_drum", "15,0.5");
    setParam(t.get(), "synth_edit", "2,1");
    waitSynth(*t);
    remember(model == 6 ? "drum-membrane"
                        : model == 7 ? "drum-modal" : "drum-medium");
    const auto recipe = encode(t->synthRecipe);
    const auto audio = t->synthWorker->audio[t->synthActive];
    setParam(t.get(), "synth_name",
             ("Modal drum " + std::to_string(model)).c_str());
    setParam(t.get(), "synth_save", "1");
    waitSynth(*t, false);
    const std::string savedModal = t->synthLastSaved.data();
    for (int i = 0;
         i < 15000 &&
         (t->sampleLoadBusy() || t->synthLoadPath[0] ||
          savedModal != t->currentLaneSourcePaths[t->selectedLane].data());
         ++i) {
      renderSplit(*t, 1);
      usleep(500);
    }
    assert(savedModal == t->currentLaneSourcePaths[t->selectedLane].data());
    setParam(t.get(), "synth_type", "1");
    waitSynth(*t);
    setParam(t.get(), "synth_restore", "1");
    waitSynth(*t);
    assert(encode(t->synthRecipe) == recipe);
    assert(t->synthWorker->audio[t->synthActive].left == audio.left &&
           t->synthWorker->audio[t->synthActive].right == audio.right);
  }
  // A saved file remains available when a deliberately invalid load path is
  // refused.
  const std::string retained =
      t->currentLaneSourcePaths[t->selectedLane].data();
  std::snprintf(t->synthDirectory.data(), t->synthDirectory.size(),
                "%s/outside-active-project", dir);
  setParam(t.get(), "synth_name", "Refused load");
  setParam(t.get(), "synth_save", "1");
  waitSynth(*t, false);
  for (int i = 0;
       i < 10000 && !std::strstr(t->synthMessage.data(), "LOAD ERROR"); ++i) {
    renderSplit(*t, 1);
    usleep(500);
  }
  assert(std::filesystem::exists(t->synthLastSaved.data()) &&
         std::strstr(t->synthMessage.data(), "LOAD ERROR") &&
         retained == t->currentLaneSourcePaths[t->selectedLane].data());
  if (const char *path = std::getenv("S3G_THRU_SYNTH_TYPES_FIXTURE")) {
    std::ifstream in(path);
    std::string all((std::istreambuf_iterator<char>(in)), {});
    assert(!all.empty() && all.back() == ']');
    all.pop_back();
    for (const auto &frame : frames)
      all += "," + frame;
    all += "]";
    std::ofstream file(path);
    file << all;
  }
  t.reset();
  std::filesystem::remove_all(dir);
  std::puts("Synth workshop workflow: isolated dual file picks, source/IR "
            "embedding, exact restore and portable project import without "
            "originals, independent workspaces, drum WAV recipe/audio recall "
            "and compact snapshots: PASS");
}
