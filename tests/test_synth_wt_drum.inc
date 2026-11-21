#include "test_synth_wt_drum_core.inc"
void testSynthWtDrumWorkflow() {
  using namespace s3g_thru_synth;
  char directory[] = "/tmp/thru-wtdrum.XXXXXX";
  assert(mkdtemp(directory));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(directory));
  t->stopBackgroundWork();
  assert(t->persistencePath("captures/Synths", t->synthDirectory.data(),
                            t->synthDirectory.size()));
  setParam(t.get(), "synth_open", "1");
  setParam(t.get(), "synth_type", "13");
  setParam(t.get(), "synth_edit", "0,0.18");
  setParam(t.get(), "synth_edit", "2,1");
  assert(!t->synthWantsRender && t->synthWorker->state.load() == Worker::idle);
  std::vector<std::string> frames;
  for (int model = 0; model < wtModelCount; ++model) {
    setParam(t.get(), "synth_wt", ("0," + std::to_string(model)).c_str());
    assert(!t->synthWantsRender);
    waitSynth(*t);
    assert(t->synthRecipe.type == wtDrum && t->synthReady());
    std::array<char, 4096> json{};
    const int count =
        getParamValue(t.get(), "synth_state", json.data(), json.size());
    assert(count > 0 && count < 3800);
    frames.push_back(std::string("{\"label\":\"wt-drum-") +
                     std::to_string(model) + "\",\"frame\":" + json.data() +
                     "}");
    const auto recipe = encode(t->synthRecipe);
    const auto audio = t->synthWorker->audio[t->synthActive];
    setParam(t.get(), "synth_name",
             ("WT Drum " + std::to_string(model)).c_str());
    setParam(t.get(), "synth_save", "1");
    waitSynth(*t, false);
    const std::string path = t->synthLastSaved.data();
    assert(!path.empty() && std::filesystem::exists(path));
    for (int i = 0; i < 15000 &&
                    (t->sampleLoadBusy() || t->synthLoadPath[0] ||
                     path != t->currentLaneSourcePaths[t->selectedLane].data());
         ++i) {
      renderSplit(*t, 1);
      usleep(500);
    }
    assert(path == t->currentLaneSourcePaths[t->selectedLane].data());
    setParam(t.get(), "synth_type", "1");
    waitSynth(*t);
    setParam(t.get(), "synth_restore", "1");
    waitSynth(*t);
    assert(encode(t->synthRecipe) == recipe &&
           t->synthWorker->audio[t->synthActive].left == audio.left &&
           t->synthWorker->audio[t->synthActive].right == audio.right);
  }
  setParam(t.get(), "synth_wt", "1,333");
  setParam(t.get(), "synth_wt_start", "1");
  assert(t->synthRecipe.wt[wtTune] == 490);
  const auto locks = t->synthRecipe.wtLocks;
  setParam(t.get(), "synth_wt_lock", "4");
  assert(t->synthRecipe.wtLocks == (locks ^ 16));
  // Dormant long source names must not inflate this generator's UI snapshot.
  for (auto &name : t->synthHybridLabels) {
    name.fill('x');
    name.back() = 0;
  }
  for (auto &name : t->synthConvLabels) {
    name.fill('x');
    name.back() = 0;
  }
  t->synthSourceLabel.fill('x');
  t->synthSourceLabel.back() = 0;
  std::array<char, 4096> json{};
  assert(t->formatSynth(json.data(), json.size()) > 0);
  std::array<char, 50> small{};
  assert(t->formatSynth(small.data(), small.size()) < 0);
  if (const char *file = std::getenv("S3G_THRU_SYNTH_TYPES_FIXTURE")) {
    std::ifstream in(file);
    std::string all((std::istreambuf_iterator<char>(in)), {});
    assert(!all.empty() && all.back() == ']');
    all.pop_back();
    for (const auto &f : frames)
      all += "," + f;
    all += "]";
    std::ofstream out(file);
    out << all;
  }
  t.reset();
  std::filesystem::remove_all(directory);
  std::puts("WT Drum workflow: all six starts through real worker, manual "
            "Render, Save & Load, portable v11 WAV recipe and exact stereo "
            "rebuild, model reset, locks, bounded native snapshots and "
            "unchanged legacy types: PASS");
}
