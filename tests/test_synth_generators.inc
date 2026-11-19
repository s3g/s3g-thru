#include "synth_v2_fixture.h"
#include "test_synth_banks.inc"
void testSynthGeneratorsCore() {
  testSynthBanksCore();
  using namespace s3g_thru_synth;
  Recipe legacy;
  assert(decode(legacySynthRecipe, sizeof(legacySynthRecipe), legacy) &&
         legacy.type == spline);
  Audio a, b;
  const auto draw = [&](const Recipe &r, Audio &out,
                        const SourceAudio *source = nullptr) {
    return render(
        r, 16000, out, [] { return false; }, [](unsigned) {}, source);
  };
  assert(draw(legacy, a));
  for (int i = 0; i < 32; ++i)
    assert(std::abs(a.left[30 + i * 47] - legacySynthAudio[i]) < 1e-6f);
  const auto energy = [](const std::vector<float> &x) {
    double sum = 0;
    for (float v : x) {
      assert(std::isfinite(v) && std::abs(v) <= .892f);
      sum += v * v;
    }
    return sum / x.size();
  };
  const auto frequency = [](const Audio &audio, float low, float high) {
    float best = 0, peak = 0;
    for (float hz = low; hz <= high; hz += 1) {
      double re = 0, im = 0;
      for (std::size_t i = 1200; i + 1200 < audio.left.size(); ++i) {
        re += audio.left[i] * std::cos(Plan::tau * hz * i / 16000);
        im += audio.left[i] * std::sin(Plan::tau * hz * i / 16000);
      }
      const float e = float(re * re + im * im);
      if (e > best) {
        best = e;
        peak = hz;
      }
    }
    return peak;
  };
  for (int type = 0; type < generatorCount; ++type) {
    Recipe r;
    r.type = type;
    const auto encoded = encode(r);
    assert(encoded.size() == recipeBytes);
    Recipe restored;
    assert(decode(encoded.data(), encoded.size(), restored) &&
           encode(restored) == encoded);
  }
  Recipe r;
  r.type = wave;
  r.v[length] = .5f;
  for (int shape = 0; shape < 4; ++shape) {
    setWave(r, wShape, float(shape));
    assert(draw(r, a) && energy(a.left) > .01 && a.left == a.right);
    assert(std::abs(frequency(a, 250, 275) - 261.625f) < 1.1f);
    assert(draw(r, b) && a.left == b.left);
  }
  r.osc[wShape] = 0;
  r.v[width] = 1;
  r.osc[wDetune] = 0;
  assert(draw(r, a) && a.left == a.right);
  r.osc[wDetune] = 12;
  assert(draw(r, a) && a.left != a.right);
  r.osc[wNoise] = .2f;
  r.seed = 123;
  assert(draw(r, a) && draw(r, b) && a.left == b.left);
  ++r.seed;
  assert(draw(r, b) && a.left != b.left);
  r.osc[wShape] = 3;
  r.mod[mDestination] = 2;
  r.mod[mAmount] = .7f;
  r.mod[mRate] = 3;
  r.mod[mShape] = 3;
  assert(draw(r, a) && energy(a.left) > .01);
  r.mod[mDestination] = 3;
  r.mod[mAmount] = 1;
  r.osc[wResonance] = .95f;
  r.osc[wCutoff] = 800;
  assert(draw(r, a) && energy(a.left) > .001);
  const auto rootNote = r.v[root], lengthSeconds = r.v[s3g_thru_synth::length];
  clean(r);
  assert(r.v[s3g_thru_synth::root] == rootNote &&
         r.v[s3g_thru_synth::length] == lengthSeconds && r.osc[wShape] == 3 &&
         r.mod[mAmount] == 0 && r.osc[wCutoff] == 20000 && r.osc[wDetune] == 0);
  for (int f = 0; f < waveFields; ++f)
    assert(!setWave(r, f, NAN));
  for (int f = 0; f < modFields; ++f)
    assert(!setMod(r, f, INFINITY));
  setMod(r, mFit, 1);
  setMod(r, mRate, 5);
  r.mod[mAmount] = 1;
  r.mod[mDestination] = 1;
  r.mod[mFade] = 0;
  r.mod[mShape] = 0;
  assert(std::abs(lfo(r, .037f) - lfo(r, .037f + r.v[s3g_thru_synth::length])) <
         1e-4f);
  r.type = bloom;
  r.v[width] = 0;
  r.v[s3g_thru_synth::length] = .4f;
  r.mod = modDefaults;
  for (int mode = 0; mode < 3; ++mode) {
    r.packets[bMode] = float(mode);
    assert(draw(r, a) && draw(r, b) && a.left == b.left && a.left == a.right &&
           energy(a.left) > .001);
  }
  r.packets[bMode] = 0;
  r.packets[bGap] = 250;
  r.packets[bScatter] = 0;
  r.packets[bRise] = 8;
  r.packets[bSustain] = 8;
  r.packets[bDecay] = 24;
  r.packets[bRiseVar] = r.packets[bSustainVar] = r.packets[bDecayVar] = 0;
  r.packets[bAmpJitter] = 0;
  assert(draw(r, a));
  for (std::size_t i = 1000; i < 3000; ++i)
    assert(std::abs(a.left[i]) <
           .003f); // Distinct packets retain their quiet gaps.
  setPartial(r, 0, 2, 0, 2.7f);
  setPartial(r, 0, 2, 1, -.5f);
  r.packets[bBend] = 1;
  r.packets[bOctave] = .5f;
  r.packets[bCount] = 2.4f;
  r.v[width] = 1;
  r.packets[bScatter] = .4f;
  assert(draw(r, a) && energy(a.left) > .001 && a.left != a.right);
  SourceAudio source;
  source.rate = 16000;
  source.left.resize(8000);
  source.right.resize(8000);
  for (std::size_t i = 0; i < 8000; ++i)
    source.left[i] = source.right[i] =
        .3f * std::sin(Plan::tau * 220 * i / 16000);
  r = {};
  r.type = resynth;
  r.v[width] = 1;
  r.v[attack] = r.v[release] = 0;
  for (int mode = 0; mode < 2; ++mode)
    for (int stretch = 1; stretch <= 2; ++stretch) {
      r.spectral[rsMode] = float(mode);
      r.v[s3g_thru_synth::length] = .5f * stretch;
      assert(draw(r, a, &source) &&
             a.left.size() == std::size_t(8000 * stretch) &&
             a.left == a.right && energy(a.left) > .01);
      assert(std::abs(frequency(a, 210, 230) - 220) < 1.1f);
      float delta = 0;
      for (std::size_t i = 1; i < a.left.size(); ++i)
        delta = std::max(delta, std::abs(a.left[i] - a.left[i - 1]));
      assert(delta < .16f);
      if (stretch == 1 && mode == 0) {
        double dot = 0, denom = 0, error = 0, power = 0;
        for (std::size_t i = 1200; i + 1200 < a.left.size(); ++i) {
          dot += a.left[i] * source.left[i];
          denom += source.left[i] * source.left[i];
        }
        const double gain = dot / denom;
        for (std::size_t i = 1200; i + 1200 < a.left.size(); ++i) {
          const double d = a.left[i] - source.left[i] * gain;
          error += d * d;
          power += a.left[i] * a.left[i];
        }
        assert(error / power < .001);
      }
    }
  r.v[s3g_thru_synth::length] = .5f;
  r.spectral[rsPitch] = 12;
  for (int mode = 0; mode < 2; ++mode) {
    r.spectral[rsMode] = float(mode);
    assert(draw(r, a, &source));
    assert(std::abs(frequency(a, 430, 450) - 440) < 2);
  }
  for (std::size_t i = 0; i < 8000; ++i)
    source.right[i] = .3f * std::cos(Plan::tau * 220 * i / 16000);
  r.spectral[rsPitch] = 0;
  for (int mode = 0; mode < 2; ++mode) {
    r.spectral[rsMode] = float(mode);
    assert(draw(r, a, &source));
    double cross = 0, l = 0, rr = 0;
    for (std::size_t i = 1500; i < 6500; ++i) {
      cross += a.left[i] * a.right[i];
      l += a.left[i] * a.left[i];
      rr += a.right[i] * a.right[i];
    }
    assert(std::abs(cross / std::sqrt(l * rr)) < .2);
  }
  r.spectral[rsBlur] = .6f;
  r.spectral[rsSmooth] = .8f;
  r.mod[mAmount] = .5f;
  r.mod[mDestination] = 2;
  r.mod[mRate] = 2;
  assert(draw(r, a, &source) && energy(a.left) > .001);
  clean(r);
  assert(r.v[width] == 1 && r.spectral[rsMode] == 0 && r.mod[mAmount] == 0);
  assert(!draw(r, a));
  assert(!render(
      r, 16000, a, [] { return true; }, [](unsigned) {}, &source));
  std::puts("Synth generators: legacy Spline identity, four clean waves, "
            "deterministic "
            "noise/PWM/filter modulation, fit cycles, Bloom spectra/envelopes, "
            "PVOC identity, "
            "independent pitch/stretch, sinusoidal tracks, stereo phase and "
            "cancellation: PASS");
}
void testSynthGeneratorWorkflow() {
  using namespace s3g_thru_synth;
  char folder[] = "/tmp/thru-synth-generators.XXXXXX";
  assert(mkdtemp(folder));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(folder));
  t->stopBackgroundWork();
  std::snprintf(t->synthDirectory.data(), t->synthDirectory.size(),
                "%s/captures/Synths", folder);
  setParam(t.get(), "synth_open", "1");
  setParam(t.get(), "synth_edit", "0,0.12");
  setParam(t.get(), "synth_edit", "6,9");
  waitSynth(*t);
  std::vector<std::string> uiFrames;
  const auto remember = [&](const char *label) {
    std::array<char, 4096> data{};
    const int n = t->formatSynth(data.data(), data.size());
    assert(n > 0 && n < 3800);
    uiFrames.push_back(std::string("{\"label\":\"") + label +
                       "\",\"frame\":" + data.data() + "}");
  };
  remember("spline");
  const auto splineRecipe = encode(t->synthRecipe);
  const auto splineAudio = t->synthWorker->audio[t->synthActive].left;
  setParam(t.get(), "synth_type", "1");
  setParam(t.get(), "synth_wave", "0,2");
  setParam(t.get(), "synth_wave", "2,9.5");
  setParam(t.get(), "synth_mod", "2,0.2");
  waitSynth(*t);
  remember("wave");
  const auto waveRecipe = encode(t->synthRecipe);
  setParam(t.get(), "synth_type", "0");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == splineRecipe &&
         t->synthWorker->audio[t->synthActive].left == splineAudio);
  setParam(t.get(), "synth_type", "1");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == waveRecipe);
  setParam(t.get(), "synth_type", "2");
  setParam(t.get(), "synth_bloom", "0,2");
  setParam(t.get(), "synth_bloom", "2,3.4");
  setParam(t.get(), "synth_spec_edit", "1,2,0,2.76");
  setParam(t.get(), "synth_spec_edit", "1,2,1,-0.4");
  waitSynth(*t);
  assert(std::abs(t->synthRecipe.ratios[1][2] - 2.76f) < 1e-6f);
  setParam(t.get(), "synth_detail_lock", "37");
  assert(t->synthRecipe.detailLocks & (1ULL << 37));
  setParam(t.get(), "synth_spec_select", "1,2");
  remember("bloom");
  for (int type = partialsType; type <= fm; ++type) {
    setParam(t.get(), "synth_type", std::to_string(type).c_str());
    const char *command = type == partialsType
                              ? "synth_additive"
                              : type == noiseBank ? "synth_noise" : "synth_fm";
    setParam(t.get(), command, type == fm ? "0,2" : "0,32");
    if (type != fm) {
      setParam(t.get(), "synth_spec_edit", "3,7,0,2.7");
      setParam(t.get(), "synth_spec_edit", "3,7,1,-0.4");
      setParam(t.get(), "synth_spec_select", "3,7");
    }
    setParam(t.get(), "synth_detail_lock", "61");
    waitSynth(*t);
    remember(generatorName(type));
    const auto expected = encode(t->synthRecipe);
    const auto audio = t->synthWorker->audio[t->synthActive].left;
    setParam(t.get(), "synth_name", generatorName(type));
    setParam(t.get(), "synth_save", "0");
    waitSynth(*t, false);
    const std::string saved = t->synthLastSaved.data();
    assert(std::filesystem::exists(saved));
    setParam(t.get(), "synth_type", "0");
    waitSynth(*t);
    assert(encode(t->synthRecipe) == splineRecipe);
    std::snprintf(t->currentLaneSourcePaths[0].data(), kPersistencePathSize,
                  "%s", saved.c_str());
    setParam(t.get(), "synth_restore", "1");
    waitSynth(*t);
    assert(encode(t->synthRecipe) == expected &&
           t->synthWorker->audio[t->synthActive].left == audio);
    if (type != fm)
      assert(t->synthRecipe.ratios[3][7] == 2.7f &&
             t->synthRecipe.partials[3][7] == -.4f);
  }
  setParam(t.get(), "synth_type", "3");
  assert(t->synthRecipe.type == resynth && !t->synthWantsRender &&
         !t->synthSourcePath[0]);
  setParam(t.get(), "synth_preview", "1");
  assert(!t->synthPreviewWanted);
  remember("resynth-empty");
  const std::string source = std::string(folder) + "/captures/input.wav";
  std::filesystem::create_directories(std::string(folder) + "/captures");
  std::vector<float> left(12000), right(12000);
  for (std::size_t i = 0; i < left.size(); ++i) {
    left[i] = .2f * std::sin(Plan::tau * 220 * i / 48000);
    right[i] = .2f * std::cos(Plan::tau * 220 * i / 48000);
  }
  assert(ThruInstance::writeStereoWavFile(left.data(), right.data(),
                                          left.size(), 48000, source.c_str(),
                                          (source + ".tmp").c_str(), true));
  t->currentLaneSourceKinds[0] = LaneSourceKind::file;
  std::snprintf(t->currentLaneSourcePaths[0].data(), kPersistencePathSize, "%s",
                source.c_str());
  setParam(t.get(), "record_lane", "1");
  setParam(t.get(), "synth_source", "1");
  setParam(t.get(), "synth_resynth", "0,1");
  setParam(t.get(), "synth_mod", "3,4");
  setParam(t.get(), "synth_mod", "2,0.15");
  waitSynth(*t);
  remember("resynth-ready");
  const auto &sound = t->synthWorker->audio[t->synthActive];
  assert(sound.source && sound.source->left.size() == 11025 &&
         sound.left.size() == 5292);
  const auto originalSource = sound.source->left, originalAudio = sound.left;
  const auto recipe = encode(t->synthRecipe);
  setParam(t.get(), "synth_name", "Resynth take");
  setParam(t.get(), "record_lane", "2");
  setParam(t.get(), "synth_save", "1");
  waitSynth(*t, false);
  for (int i = 0; i < 10000 && !t->engine.laneLoaded(1); ++i) {
    renderSplit(*t, 1);
    usleep(500);
  }
  assert(t->engine.laneLoaded(1));
  const std::string saved = t->synthLastSaved.data();
  std::filesystem::remove(source);
  t->currentLaneSourcePaths[0].fill(0);
  t->currentLaneSourceKinds[0] = LaneSourceKind::empty;
  setParam(t.get(), "synth_type", "1");
  waitSynth(*t);
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t);
  assert(t->synthRecipe.type == resynth && encode(t->synthRecipe) == recipe &&
         t->synthSourceEmbedded);
  assert(t->synthWorker->audio[t->synthActive].source->left == originalSource &&
         t->synthWorker->audio[t->synthActive].left == originalAudio);
  // The embedded source has an independent checksum, checked before replacing
  // the workspace.
  const std::string corrupt = saved + ".bad.wav";
  std::filesystem::copy_file(saved, corrupt);
  auto bytes = std::vector<unsigned char>(std::filesystem::file_size(corrupt));
  {
    std::ifstream file(corrupt, std::ios::binary);
    file.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
  }
  bool changed = false;
  for (std::size_t i = 12; i + 32 < bytes.size(); ++i)
    if (!std::memcmp(bytes.data() + i, "sgSo", 4)) {
      bytes[i + 25] ^= 0x40;
      changed = true;
      break;
    }
  assert(changed);
  {
    std::ofstream file(corrupt, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  }
  setParam(t.get(), "synth_type", "1");
  waitSynth(*t);
  const auto unchanged = encode(t->synthRecipe);
  std::snprintf(t->currentLaneSourcePaths[1].data(), kPersistencePathSize, "%s",
                corrupt.c_str());
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t, false);
  assert(encode(t->synthRecipe) == unchanged);
  std::snprintf(t->currentLaneSourcePaths[1].data(), kPersistencePathSize, "%s",
                saved.c_str());
  t->copyCurrentSourcesToClip(*t->activeClipSlot());
  t->captureActiveClip(true);
  assert(t->savePersistentState() && t->exportProjectFile());
  const std::string archive = t->projectOutputPath.data();
  t.reset();
  std::filesystem::remove_all(std::string(folder) + "/captures");
  const auto target = std::string(folder) + "/restored";
  std::filesystem::create_directory(target);
  t = std::make_unique<ThruInstance>();
  assert(t->prepare(target.c_str()));
  t->stopBackgroundWork();
  assert(t->savePersistentState());
  t->projectGeneration = t->reservePersistenceGeneration();
  std::snprintf(t->projectArchivePath.data(), kPersistencePathSize, "%s",
                archive.c_str());
  assert(t->importProjectFile() && t->savePersistentState());
  t.reset();
  t = std::make_unique<ThruInstance>();
  assert(t->prepare(target.c_str()));
  t->stopBackgroundWork();
  for (int i = 0;
       i < 15000 && (!t->engine.laneLoaded(1) || t->sampleLoadBusy() ||
                     t->clipPreparationInFlight());
       ++i) {
    renderSplit(*t, 1);
    usleep(500);
  }
  assert(t->engine.laneLoaded(1));
  setParam(t.get(), "record_lane", "2");
  setParam(t.get(), "synth_open", "1");
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == recipe &&
         t->synthWorker->audio[t->synthActive].left == originalAudio);
  if (const char *path = std::getenv("S3G_THRU_SYNTH_TYPES_FIXTURE")) {
    std::ofstream file(path);
    file << "[";
    for (std::size_t i = 0; i < uiFrames.size(); ++i)
      file << (i ? "," : "") << uiFrames[i];
    file << "]";
  }
  t.reset();
  std::filesystem::remove_all(folder);
  std::puts("Synth workflow: independent generator workspaces, Spline "
            "preserved, Bloom edits, "
            "high lock bits, Resynth source SRC, self-contained recipe, "
            "corrupt-source refusal "
            "and project roundtrip without originals: PASS");
}
