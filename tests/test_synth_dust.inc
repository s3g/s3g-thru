#include "test_synth_dust_core.inc"
void testSynthDustWorkflow() {
  using namespace s3g_thru_synth;
  char directory[] = "/tmp/thru-dust.XXXXXX";
  assert(mkdtemp(directory));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(directory));
  t->stopBackgroundWork();
  assert(t->persistencePath("captures/Synths", t->synthDirectory.data(),
                            t->synthDirectory.size()));
  setParam(t.get(), "synth_open", "1");
  setParam(t.get(), "synth_type", "12");
  setParam(t.get(), "synth_edit", "0,0.18");
  setParam(t.get(), "synth_edit", "2,1");
  setParam(t.get(), "synth_dust", "1,30");
  setParam(t.get(), "synth_dust", "2,6");
  setParam(t.get(), "synth_dust", "10,3");
  std::vector<std::string> frames;
  for (int mode = 0; mode < 6; ++mode) {
    setParam(t.get(), "synth_dust", ("0," + std::to_string(mode)).c_str());
    waitSynth(*t);
    assert(t->synthRecipe.type == dust && t->synthReady());
    std::array<char, 4096> json{};
    const int count =
        getParamValue(t.get(), "synth_state", json.data(), json.size());
    assert(count > 0 && count < 3800);
    frames.push_back(std::string("{\"label\":\"dust-") + std::to_string(mode) +
                     "\",\"frame\":" + json.data() + "}");
    const auto recipe = encode(t->synthRecipe);
    const auto audio = t->synthWorker->audio[t->synthActive];
    setParam(t.get(), "synth_name", ("Dust " + std::to_string(mode)).c_str());
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
  const auto modeLock = t->synthRecipe.dustLocks;
  setParam(t.get(), "synth_dust_lock", "0");
  assert(t->synthRecipe.dustLocks == (modeLock ^ 1u));
  setParam(t.get(), "synth_dust_lock", "0");
  const auto mode = t->synthRecipe.dustParams[uMode];
  setParam(t.get(), "synth_mutate", "0.25");
  waitSynth(*t);
  assert(t->synthRecipe.dustParams[uMode] == mode);
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
  std::puts("Synth Dust workflow: six-mode native controls, background worker, "
            "stereo Save & Load, exact recipe/audio rebuild, "
            "protection/mutation and callback UI snapshots: PASS");
}
