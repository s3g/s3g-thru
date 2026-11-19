#include "synth_v7_fixture.h"
void testSynthDrumAttackCore() {
  using namespace s3g_thru_synth;
  const auto no = [] { return false; };
  const auto progress = [](unsigned) {};
  const auto energy = [](const Audio &a, float start, float end, float rate) {
    double sum = 0;
    for (auto i = std::size_t(start * rate);
         i < std::min(a.left.size(), std::size_t(end * rate)); ++i)
      sum += double(a.left[i]) * a.left[i];
    return sum;
  };
  std::vector<std::uint8_t> bytes;
  for (std::size_t i = 0; i < sizeof(synthV7Hex) - 1; i += 2) {
    unsigned v = 0;
    assert(std::sscanf(synthV7Hex + i, "%2x", &v) == 1);
    bytes.push_back(std::uint8_t(v));
  }
  Recipe legacy;
  assert(bytes.size() == seventhRecipeBytes &&
         decode(bytes.data(), bytes.size(), legacy));
  assert(legacy.type == drum && legacy.drumFlags == 1 &&
         legacy.drumExtra[xSnap] == 0 && legacy.drumExtra[xEdge] == 2 &&
         legacy.drumExtra[xPunch] == .72f);
  const auto next = encode(legacy);
  Recipe restored;
  assert(next.size() == recipeBytes &&
         decode(next.data(), next.size(), restored) &&
         encode(restored) == next);
  for (int model = 0; model < drumModelCount; ++model) {
    Recipe r;
    initializeGenerator(r, drum);
    initializeDrumModel(r, model);
    r.v[length] = .3f;
    r.v[attack] = 0;
    r.v[release] = 2;
    r.v[width] = 1;
    r.drumExtra[xSnap] = 0;
    assert(valid(r) && r.drumFlags == 0);
    Audio shortEdge, longEdge, snap;
    assert(render(r, 16000, shortEdge, no, progress));
    r.drumExtra[xEdge] = 2;
    assert(render(r, 16000, longEdge, no, progress));
    const auto head = energy(shortEdge, 0, .002f, 16000),
               soft = energy(longEdge, 0, .002f, 16000);
    assert(head > soft * 1.1);
    r.drumExtra[xEdge] = .15f;
    r.drumExtra[xSnap] = 1;
    assert(render(r, 16000, snap, no, progress));
    const auto a = energy(shortEdge, 0, .02f, 16000) /
                   std::max(1e-20, energy(shortEdge, .08f, .2f, 16000));
    const auto b = energy(snap, 0, .02f, 16000) /
                   std::max(1e-20, energy(snap, .08f, .2f, 16000));
    assert(b > a * 2);
    assert(snap.left.front() == 0 && snap.left.back() == 0 &&
           snap.left != snap.right);
    for (float x : snap.left)
      assert(std::isfinite(x) && std::abs(x) < .892f);
    for (int field = 0; field < 8; ++field) {
      Recipe varied = r;
      assert(!setWorkshop(varied, 6, field, NAN));
      for (float value : {-1e6f, 1e6f}) {
        assert(setWorkshop(varied, 6, field, value) && valid(varied));
      }
    }
  }
  // The wrapper is checked against an independently configured source core.
  {
    Recipe r;
    initializeGenerator(r, drum);
    initializeDrumModel(r, 9);
    r.v[length] = .25f;
    r.v[attack] = 0;
    r.v[width] = 1;
    r.drumExtra[xSnap] = 0;
    s3g::DrumConcertBassParams p;
    p.noteTracking = 0;
    p.stereoWidth = 1;
    p.velocitySensitivity = 0;
    p.outputGainDb = -6;
    p.impact = .72f;
    p.bloom = .5f;
    p.beaterHardness = .65f;
    s3g::DrumConcertBass core;
    core.prepare(44100);
    core.setParams(p);
    core.trigger(s3g::DrumConcertBassArticulation::Center, 1);
    Audio expected, actual;
    assert(renderDrumHit(core, r, 44100, expected, no, progress) &&
           render(r, 44100, actual, no, progress));
    assert(expected.left == actual.left && expected.right == actual.right);
  }
  std::vector<float> previous;
  for (int art = 0; art < 3; ++art) {
    Recipe r;
    initializeGenerator(r, drum);
    initializeDrumModel(r, 9);
    r.v[length] = .22f;
    r.v[width] = 1;
    assert(setDrum(r, dForm, float(art)) && valid(r));
    Audio a, b;
    assert(render(r, 44100, a, no, progress) &&
           render(r, 44100, b, no, progress) && a.left == b.left &&
           a.right == b.right && energy(a, 0, .22f, 44100) > 0);
    if (!previous.empty())
      assert(previous != a.left);
    previous = a.left;
    r.v[width] = 0;
    assert(render(r, 48000, b, no, progress) && b.left == b.right);
    assert(!render(
        r, 16000, b, [] { return true; }, progress));
  }
  // Boundary cleanup defaults for all other Synth types are untouched.
  {
    Recipe r;
    r.v[length] = .2f;
    Audio a, b;
    a.left.resize(3200);
    a.right.resize(3200);
    for (int i = 0; i < 3200; ++i)
      a.left[i] = a.right[i] = std::sin(i * .09f);
    b = a;
    finishSound(r, 16000, a, 3200, 0);
    finishSound(r, 16000, b, 3200, 0, 2);
    assert(a.left == b.left && a.right == b.right);
  }
  std::puts("Synth Drum attack: native Concert Bass identity, three "
            "articulations, eleven onset-energy gains, level-matched transient "
            "contrast, model bounds, captured v7 migration, clean endpoints, "
            "stereo/mono and cancellation: PASS");
}
void testSynthDrumAttackWorkflow() {
  using namespace s3g_thru_synth;
  char dir[] = "/tmp/thru-drum-attack.XXXXXX";
  assert(mkdtemp(dir));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(dir));
  t->stopBackgroundWork();
  assert(t->persistencePath("captures/Synths", t->synthDirectory.data(),
                            t->synthDirectory.size()));
  setParam(t.get(), "synth_open", "1");
  setParam(t.get(), "synth_type", "8");
  setParam(t.get(), "synth_drum", "0,9");
  setParam(t.get(), "synth_edit", "0,0.22");
  setParam(t.get(), "synth_edit", "2,1");
  std::vector<std::string> frames;
  const auto remember = [&](const char *label) {
    std::array<char, 4096> out{};
    assert(getParamValue(t.get(), "synth_state", out.data(), int(out.size())) >
               0 &&
           std::strlen(out.data()) < 3800);
    frames.push_back(std::string("{\"label\":\"") + label +
                     "\",\"frame\":" + out.data() + "}");
  };
  setParam(t.get(), "synth_attack", "0,0.82");
  setParam(t.get(), "synth_attack", "6,0.6");
  setParam(t.get(), "synth_attack", "7,0.12");
  waitSynth(*t);
  assert(t->synthRecipe.drumExtra[xPunch] == .82f &&
         t->synthRecipe.drumExtra[xSnap] == .6f &&
         t->synthRecipe.drumExtra[xEdge] == .12f);
  remember("drum-attack");
  for (int art = 0; art < 3; ++art) {
    setParam(t.get(), "synth_drum", ("12," + std::to_string(art)).c_str());
    waitSynth(*t);
    remember(art == 0 ? "concert-center"
                      : art == 1 ? "concert-muted" : "concert-rim");
    const auto recipe = encode(t->synthRecipe);
    const auto audio = t->synthWorker->audio[t->synthActive];
    setParam(t.get(), "synth_name", ("Concert " + std::to_string(art)).c_str());
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
  setParam(t.get(), "synth_drum", "0,10");
  setParam(t.get(), "synth_drum", "7,0.5");
  setParam(t.get(), "synth_color", "0,8");
  setParam(t.get(), "synth_color", "1,8000");
  setParam(t.get(), "synth_color", "2,6000");
  setParam(t.get(), "synth_color", "3,0.7");
  setParam(t.get(), "synth_attack", "5,0.2");
  waitSynth(*t);
  assert(t->synthRecipe.drums[dModel] == 10 &&
         t->synthRecipe.drumColor[cBits] == 8);
  remember("fm-drum-color");
  {
    const auto savedRecipe = encode(t->synthRecipe);
    const auto savedAudio = t->synthWorker->audio[t->synthActive];
    setParam(t.get(), "synth_name", "FM Drum Color");
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
    assert(encode(t->synthRecipe) == savedRecipe &&
           t->synthWorker->audio[t->synthActive].left == savedAudio.left &&
           t->synthWorker->audio[t->synthActive].right == savedAudio.right);
  }
  setParam(t.get(), "synth_drum", "0,9");
  setParam(t.get(), "synth_attack", "0,0.1");
  setParam(t.get(), "synth_drum_start", "1");
  waitSynth(*t);
  assert(t->synthRecipe.drums[dModel] == 9 &&
         t->synthRecipe.drums[dForm] == 0 &&
         t->synthRecipe.drumExtra[xPunch] == .65f &&
         t->synthRecipe.drumFlags == 0);
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
  std::puts(
      "Synth Drum attack workflow: real native setters, current-model reset, "
      "three Concert articulations and colored FM Drum, stereo WAV and exact "
      "recipe/audio recall, "
      "new protection bits and callback UI snapshots: PASS");
}
