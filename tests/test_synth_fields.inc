#include "synth_v5_fixture.h"
#include "test_synth_drum_attack.inc"
#include "test_synth_drum_color.inc"
#include "test_synth_music.inc"
#include "test_synth_dust.inc"
void testSynthFieldsCore() {
  using namespace s3g_thru_synth;
  const auto no = [] { return false; };
  const auto progress = [](unsigned) {};
  const auto check = [](const Audio &a) {
    assert(!a.left.empty() && a.left.size() == a.right.size());
    double power = 0;
    for (std::size_t i = 0; i < a.left.size(); ++i) {
      assert(std::isfinite(a.left[i]) && std::isfinite(a.right[i]) &&
             std::abs(a.left[i]) < .892f && std::abs(a.right[i]) < .892f);
      power += double(a.left[i]) * a.left[i];
    }
    assert(a.left.front() == 0 && a.left.back() == 0);
    return power;
  };
  std::vector<std::uint8_t> old;
  for (std::size_t i = 0; i < sizeof(synthV5Hex) - 1; i += 2) {
    unsigned v = 0;
    assert(std::sscanf(synthV5Hex + i, "%2x", &v) == 1);
    old.push_back(std::uint8_t(v));
  }
  Recipe migrated;
  assert(old.size() == fifthRecipeBytes &&
         decode(old.data(), old.size(), migrated));
  assert(migrated.type == drum && migrated.drums[dModel] == 8 &&
         migrated.events == eventDefaults && migrated.trace == traceDefaults &&
         migrated.plucked == pluckDefaults);
  for (int group = 0; group < 8; ++group)
    for (int f = 0; f < 8; ++f) {
      Recipe r;
      assert(!setWorkshop(r, group, f, NAN));
      assert(setWorkshop(r, group, f, -1e9f) &&
             workshopGroup(r, group)[f] == (group == 6
                                                ? drumAttackRange(0, f)[0]
                                                : workshopMinimum(group)[f]));
      assert(setWorkshop(r, group, f, 1e9f) &&
             workshopGroup(r, group)[f] == (group == 6
                                                ? drumAttackRange(0, f)[1]
                                                : workshopMaximum(group)[f]));
      assert(valid(r));
    }
  for (int type : {pluck, subharmonic}) {
    Recipe r;
    initializeGenerator(r, type);
    r.v[length] = .25f;
    r.v[attack] = 0;
    r.events[eMode] = 0;
    Audio a, b;
    assert(render(r, 16000, a, no, progress) &&
           render(r, 16000, b, no, progress) && a.left == b.left &&
           a.right == b.right && a.left == a.right && check(a) > .001);
    r.v[width] = 1;
    assert(render(r, 16000, b, no, progress) && check(b) > .001 &&
           b.left != b.right);
    const auto encoded = encode(r);
    Recipe restored;
    assert(encoded.size() == recipeBytes &&
           decode(encoded.data(), encoded.size(), restored) &&
           encode(restored) == encoded);
    assert(render(restored, 16000, a, no, progress) && a.left == b.left &&
           a.right == b.right);
    auto silent = r;
    silent.v[level] = 0;
    assert(render(silent, 16000, a, no, progress) && check(a) == 0);
    assert(!render(
        r, 16000, a, [] { return true; }, progress));
    for (int loopMode = 0; loopMode < 2; ++loopMode) {
      r.v[loop] = float(loopMode);
      for (float rate : {8000.f, 44100.f, 48000.f}) {
        assert(render(r, rate, a, no, progress) &&
               a.left.size() == std::size_t(std::llround(r.v[length] * rate)));
        for (float x : a.left)
          assert(std::isfinite(x) && std::abs(x) < .892f);
      }
    }
    for (int seed = 0; seed < 20; ++seed) {
      Recipe varied = r;
      varied.seed = std::uint32_t(seed);
      mutate(varied, .7f);
      assert(valid(varied));
    }
  }
  // Pluck tuning follows an audible fundamental rather than an excitation rate.
  {
    Recipe r;
    r.type = pluck;
    r.v[length] = .6f;
    r.v[attack] = 0;
    r.plucked[pDetune] = 0;
    r.plucked[pBrightness] = .3f;
    r.plucked[pHardness] = 0;
    r.plucked[pDispersion] = 0;
    Audio a;
    for (float hz : {110.f, 220.f}) {
      r.plucked[pTune] = hz;
      assert(render(r, 16000, a, no, progress));
      float best = 0;
      double energy = -1;
      for (float f = hz * .9f; f <= hz * 1.1f; f += .5f) {
        double re = 0, im = 0;
        for (std::size_t i = 1600; i < 6400; ++i) {
          re += a.left[i] * std::cos(Plan::tau * f * i / 16000);
          im += a.left[i] * std::sin(Plan::tau * f * i / 16000);
        }
        const double e = re * re + im * im;
        if (e > energy) {
          energy = e;
          best = f;
        }
      }
      assert(std::abs(best - hz) < hz * .035f);
    }
  }
  {
    Recipe r;
    initializeGenerator(r, subharmonic);
    r.sub[sDrift] = 0;
    assert(!workshopAvailable(r, 1, sRate));
    r.sub[sDrift] = 1;
    assert(workshopAvailable(r, 1, sRate));
    r.type = drum;
    r.events[eMode] = 1;
    r.v[width] = 0;
    assert(!workshopAvailable(r, 4, ePan));
    r.v[width] = 1;
    assert(workshopAvailable(r, 4, ePan));
  }
  for (int type : {drum, bloom, partialsType, pluck}) {
    Recipe r;
    initializeGenerator(r, type);
    r.v[length] = .45f;
    r.timing[eBpm] = 240;
    r.timing[eDivision] = 7;
    r.v[attack] = 0;
    r.v[width] = 1;
    r.events[eCount] = 4;
    r.events[eLife] = 90;
    r.events[eJitter] = 0;
    std::vector<float> previous;
    for (int mode = 1; mode <= 4; ++mode) {
      r.events[eMode] = float(mode);
      Audio a, b;
      assert(render(r, 16000, a, no, progress) &&
             render(r, 16000, b, no, progress) && a.left == b.left &&
             a.right == b.right && check(a) > 0);
      if (!previous.empty())
        assert(a.left != previous);
      previous = a.left;
      r.v[width] = 0;
      assert(render(r, 16000, b, no, progress) && b.left == b.right);
      r.v[width] = 1;
    }
    r.events[eDensity] = 0;
    Audio a;
    assert(render(r, 16000, a, no, progress) && check(a) == 0);
    r.events[eDensity] = 1;
    int checks = 0;
    assert(!render(
        r, 16000, a, [&] { return ++checks > 6; }, progress));
    r.workshopLocks |= 1ULL << 33;
    const auto count = r.events[eCount];
    mutate(r, .8f);
    assert(r.events[eCount] == count && valid(r));
  }
  HybridInputs inputs;
  for (int j = 0; j < 4; ++j) {
    auto source = std::make_shared<SourceAudio>();
    source->rate = 16000;
    source->left.resize(8000);
    source->right.resize(8000);
    for (int i = 0; i < 8000; ++i) {
      const float hz = i < 4000 ? 220.f : 660.f,
                  env = .45f + .4f * std::sin(Plan::tau * (j + 1) * i / 16000);
      source->left[i] =
          .2f * std::sin(Plan::tau * hz * i / 16000) * env +
          .06f * std::sin(Plan::tau * (330 + j * 110) * i / 16000);
      source->right[i] =
          .16f * std::sin(Plan::tau * hz * i / 16000 + .3f) * env;
    }
    inputs[j] = source;
  }
  for (int mode = 0; mode < 4; ++mode) {
    Recipe r;
    r.type = resynth;
    r.v[length] = .4f;
    r.v[attack] = 0;
    r.v[width] = 1;
    r.spectral[rsMode] = 1;
    r.trace[tMode] = float(mode);
    Audio a, b;
    assert(render(r, 16000, a, no, progress, inputs[0].get()) &&
           render(r, 16000, b, no, progress, inputs[0].get()) &&
           a.left == b.left && a.right == b.right && check(a) > 0);
    r.v[width] = 0;
    assert(render(r, 16000, b, no, progress, inputs[0].get()) &&
           b.left == b.right);
    r.trace[tDensity] = 0;
    assert(render(r, 16000, b, no, progress, inputs[0].get()) && check(b) == 0);
  }
  {
    Recipe r;
    r.type = resynth;
    r.v[length] = .35f;
    r.spectral[rsMode] = 1;
    r.trace[tMode] = 3;
    r.trace[tFreeze] = .1f;
    Audio a, b;
    assert(render(r, 16000, a, no, progress, inputs[0].get()));
    r.trace[tFreeze] = .9f;
    assert(render(r, 16000, b, no, progress, inputs[0].get()) &&
           a.left != b.left);
  }
  // Atom traces keep the shared Blur/Smooth motion path audible in all modes.
  for (int mode = 1; mode <= 3; ++mode) {
    Recipe r;
    r.type = resynth;
    r.v[length] = .25f;
    r.spectral[rsMode] = 1;
    r.trace[tMode] = float(mode);
    Audio original, changed;
    assert(render(r, 16000, original, no, progress, inputs[0].get()));
    r.curves[1].p.front().value = r.curves[1].p.back().value = .8f;
    assert(render(r, 16000, changed, no, progress, inputs[0].get()) &&
           changed.left != original.left);
    r.curves = {};
    r.curves[2].p.front().value = r.curves[2].p.back().value = 1.f;
    assert(render(r, 16000, changed, no, progress, inputs[0].get()) &&
           changed.left != original.left);
  }
  for (int count = 2; count <= 4; ++count)
    for (int mode = 0; mode < 5; ++mode) {
      Recipe r;
      r.type = hybrid;
      r.v[length] = .3f;
      r.v[attack] = 0;
      r.v[width] = 1;
      r.hybridParams[hSources] = float(count);
      r.hybridParams[hMode] = float(mode);
      Audio a, b;
      assert(render(r, 16000, a, no, progress, nullptr, nullptr, &inputs) &&
             render(r, 16000, b, no, progress, nullptr, nullptr, &inputs) &&
             a.left == b.left && a.right == b.right && check(a) > 0);
      r.v[width] = 0;
      assert(render(r, 16000, b, no, progress, nullptr, nullptr, &inputs) &&
             b.left == b.right);
      r.hybridParams[hDensity] = 0;
      assert(render(r, 16000, b, no, progress, nullptr, nullptr, &inputs) &&
             check(b) == 0);
      assert(!render(r, 16000, b, no, progress));
      auto missing = inputs;
      missing[count - 1].reset();
      assert(!render(r, 16000, b, no, progress, nullptr, nullptr, &missing));
      assert(!render(
          r, 16000, b, [] { return true; }, progress, nullptr, nullptr,
          &inputs));
    }
  testSynthMusicCore();
  testSynthDrumAttackCore();
  testSynthDrumColorCore();
  testSynthDustCore();
  std::puts("Synth fields DSP: captured v5 migration, thirteen stable types, "
            "pluck tuning, band-limited Sub banks, shared event modes, "
            "linked/point/smear/frozen traces, four-source trait hybrids, "
            "stereo/mono, deterministic variation, cancellation and "
            "protection: PASS");
}
void testSynthFieldsWorkflow() {
  using namespace s3g_thru_synth;
  char dir[] = "/tmp/thru-fields.XXXXXX";
  assert(mkdtemp(dir));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(dir));
  t->stopBackgroundWork();
  assert(t->persistencePath("captures/Synths", t->synthDirectory.data(),
                            t->synthDirectory.size()));
  setParam(t.get(), "synth_open", "1");
  setParam(t.get(), "synth_edit", "0,0.18");
  std::vector<std::string> frames;
  const auto remember = [&](const char *label) {
    std::array<char, 4096> text{};
    assert(getParamValue(t.get(), "synth_state", text.data(),
                         int(text.size())) > 0 &&
           std::strlen(text.data()) < 3800);
    frames.push_back(std::string("{\"label\":\"") + label +
                     "\",\"frame\":" + text.data() + "}");
  };
  const auto restoreSaved = [&] {
    const auto recipe = encode(t->synthRecipe);
    const auto left = t->synthWorker->audio[t->synthActive].left,
               right = t->synthWorker->audio[t->synthActive].right;
    setParam(t.get(), "synth_save", "1");
    waitSynth(*t, false);
    const std::string saved = t->synthLastSaved.data();
    assert(!saved.empty() && std::filesystem::exists(saved));
    for (int i = 0;
         i < 15000 &&
         (t->sampleLoadBusy() || t->synthLoadPath[0] ||
          saved != t->currentLaneSourcePaths[t->selectedLane].data());
         ++i) {
      renderSplit(*t, 1);
      usleep(500);
    }
    assert(saved == t->currentLaneSourcePaths[t->selectedLane].data());
    setParam(t.get(), "synth_type", "0");
    waitSynth(*t);
    setParam(t.get(), "synth_restore", "1");
    waitSynth(*t);
    if (encode(t->synthRecipe) != recipe ||
        t->synthWorker->audio[t->synthActive].left != left ||
        t->synthWorker->audio[t->synthActive].right != right) {
      const auto &actual = t->synthWorker->audio[t->synthActive];
      float maxDiff = 0;
      for (std::size_t i = 0; i < std::min(left.size(), actual.left.size());
           ++i)
        maxDiff = std::max(maxDiff, std::abs(left[i] - actual.left[i]));
      std::fprintf(
          stderr,
          "Fields restore mismatch expectedType=%u actualType=%d "
          "recipeEqual=%d frames=%zu/%zu maxDiff=%g message=%s path=%s\n",
          le32(recipe.data() + 4), t->synthRecipe.type,
          int(encode(t->synthRecipe) == recipe), left.size(),
          actual.left.size(), maxDiff, t->synthMessage.data(), saved.c_str());
    }
    assert(encode(t->synthRecipe) == recipe &&
           t->synthWorker->audio[t->synthActive].left == left &&
           t->synthWorker->audio[t->synthActive].right == right);
    return saved;
  };
  for (int type : {9, 10}) {
    setParam(t.get(), "synth_type", std::to_string(type).c_str());
    setParam(t.get(), "synth_edit", "0,0.18");
    waitSynth(*t);
    if (type == 10) {
      setParam(t.get(), "synth_sub", "0,6");
      setParam(t.get(), "synth_sub", "1,3");
      setParam(t.get(), "synth_spec_select", "2,1");
      setParam(t.get(), "synth_spec_edit", "2,1,0,3.5");
      setParam(t.get(), "synth_spec_edit", "2,1,1,0.7");
      waitSynth(*t);
      assert(t->synthRecipe.ratios[2][1] == 3.5f);
    }
    const auto revision = t->synthRevision;
    const auto unchangedAudio = t->synthWorker->audio[t->synthActive].left;
    setParam(t.get(), "synth_lock", "3");
    setParam(t.get(), "synth_detail_lock", "61");
    setParam(t.get(), "synth_extended_lock", "23");
    setParam(t.get(), "synth_work_lock", "44");
    assert(t->synthReady() && t->synthRevision == revision &&
           t->synthWorker->audio[t->synthActive].left == unchangedAudio);
    remember(type == 9 ? "pluck" : "subharmonic");
    setParam(t.get(), "synth_name", type == 9 ? "Pluck field" : "Sub bank");
    restoreSaved();
  }
  std::array<std::string, 4> paths;
  std::vector<float> l(6000), r(6000);
  for (int j = 0; j < 4; ++j) {
    for (std::size_t i = 0; i < l.size(); ++i) {
      l[i] = .2f * std::sin(Plan::tau * (220 + j * 110) * i / 44100) +
             .1f * std::sin(Plan::tau * 220 * i / 44100);
      r[i] = .18f * std::sin(Plan::tau * (220 + j * 110) * i / 44100 + .4f);
    }
    paths[j] = std::string(dir) + "/source-" + std::to_string(j) + ".wav";
    assert(ThruInstance::writeStereoWavFile(l.data(), r.data(), l.size(), 44100,
                                            paths[j].c_str(),
                                            (paths[j] + ".tmp").c_str(), true));
  }
  // Use a recorded lane source for the RSYN extension, then save each behavior.
  std::snprintf(t->currentLaneSourcePaths[t->selectedLane].data(),
                kPersistencePathSize, "%s", paths[0].c_str());
  setParam(t.get(), "synth_type", "3");
  setParam(t.get(), "synth_source", "1");
  setParam(t.get(), "synth_resynth", "0,1");
  setParam(t.get(), "synth_edit", "0,0.18");
  for (int mode = 0; mode < 4; ++mode) {
    setParam(t.get(), "synth_trace", ("0," + std::to_string(mode)).c_str());
    waitSynth(*t);
    remember(mode == 0
                 ? "trace-linked"
                 : mode == 1 ? "trace-point"
                             : mode == 2 ? "trace-smear" : "trace-freeze");
    setParam(t.get(), "synth_name", ("Trace " + std::to_string(mode)).c_str());
    restoreSaved();
  }
  setParam(t.get(), "synth_type", "11");
  assert(t->synthHybridMissing());
  setParam(t.get(), "synth_preview", "1");
  assert(!t->synthPreviewWanted);
  setParam(t.get(), "synth_hybrid", "1,4");
  setParam(t.get(), "synth_edit", "0,0.18");
  for (int j = 0; j < 4; ++j)
    setParam(t.get(), ("synth_hybrid" + std::to_string(j) + "_file").c_str(),
             paths[j].c_str());
  waitSynth(*t);
  assert(t->synthReady() && t->synthHybridMissing() == 0);
  remember("hybrid-four");
  {
    const auto name = t->synthName;
    const auto source = t->synthSourceLabel;
    const auto conv = t->synthConvLabels;
    const auto labels = t->synthHybridLabels;
    const auto fill = [](auto &text) {
      std::fill(text.begin(), text.end() - 1, 'F');
      text.back() = 0;
    };
    fill(t->synthName);
    fill(t->synthSourceLabel);
    for (auto &text : t->synthConvLabels)
      fill(text);
    for (auto &text : t->synthHybridLabels)
      fill(text);
    remember("workshop-max-labels");
    t->synthName = name;
    t->synthSourceLabel = source;
    t->synthConvLabels = conv;
    t->synthHybridLabels = labels;
  }
  const auto recipe = encode(t->synthRecipe);
  const auto original = t->synthWorker->audio[t->synthActive];
  setParam(t.get(), "synth_name", "Trait hybrid");
  const auto hybridFile = restoreSaved();
  for (const auto &path : paths)
    std::filesystem::remove(path);
  setParam(t.get(), "synth_type", "0");
  waitSynth(*t);
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == recipe &&
         t->synthWorker->audio[t->synthActive].left == original.left &&
         t->synthWorker->audio[t->synthActive].right == original.right);
  // A corrupt third embedded input must never replace the open workspace.
  const auto damaged = hybridFile + ".bad.wav";
  std::filesystem::copy_file(hybridFile, damaged);
  {
    std::ifstream in(damaged, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), {});
    bool changed = false;
    for (std::size_t i = 12; i + 40 < bytes.size(); ++i)
      if (!std::memcmp(bytes.data() + i, "sgH2", 4)) {
        bytes[i + 27] ^= 0x20;
        changed = true;
        break;
      }
    assert(changed);
    std::ofstream out(damaged, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), bytes.size());
  }
  setParam(t.get(), "synth_type", "1");
  waitSynth(*t);
  const auto keep = encode(t->synthRecipe);
  std::snprintf(t->currentLaneSourcePaths[t->selectedLane].data(),
                kPersistencePathSize, "%s", damaged.c_str());
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t, false);
  assert(encode(t->synthRecipe) == keep);
  std::snprintf(t->currentLaneSourcePaths[t->selectedLane].data(),
                kPersistencePathSize, "%s", hybridFile.c_str());
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == recipe);
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
  for (int i = 0; i < 10000 && !t->engine.laneLoaded(0); ++i) {
    renderSplit(*t, 1);
    usleep(500);
  }
  assert(t->engine.laneLoaded(0));
  setParam(t.get(), "synth_open", "1");
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == recipe &&
         t->synthWorker->audio[t->synthActive].left == original.left &&
         t->synthWorker->audio[t->synthActive].right == original.right);
  for (int type : {2, 4, 8}) {
    setParam(t.get(), "synth_type", std::to_string(type).c_str());
    setParam(t.get(), "synth_edit", "0,0.18");
    setParam(t.get(), "synth_events", "0,2");
    setParam(t.get(), "synth_events", "1,3");
    setParam(t.get(), "synth_events", "5,60");
    setParam(t.get(), "synth_timing", "1,240");
    setParam(t.get(), "synth_timing", "2,7");
    setParam(t.get(), "synth_timing", "3,0.5");
    waitSynth(*t);
    remember(type == 2 ? "bloom-events"
                       : type == 4 ? "partial-events" : "drum-events");
  }
  if (const char *file = std::getenv("S3G_THRU_SYNTH_TYPES_FIXTURE")) {
    std::ifstream in(file);
    std::string all((std::istreambuf_iterator<char>(in)), {});
    assert(!all.empty() && all.back() == ']');
    all.pop_back();
    for (const auto &frame : frames)
      all += "," + frame;
    all += "]";
    std::ofstream out(file);
    out << all;
  }
  t.reset();
  std::filesystem::remove_all(dir);
  testSynthDrumAttackWorkflow();
  testSynthDustWorkflow();
  std::puts("Synth fields workflow: Pluck/Sub save and stereo recall, all RSYN "
            "trace behaviors, isolated four-file Hybrid input, all sources "
            "embedded, corrupt-source refusal, exact stereo rebuild after "
            "original deletion/project import and native snapshots: PASS");
}
