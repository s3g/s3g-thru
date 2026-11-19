#include "synth_v8_fixture.h"
void testSynthDrumColorCore() {
  using namespace s3g_thru_synth;
  const auto no = [] { return false; };
  const auto progress = [](unsigned) {};
  const auto power = [](const Audio &a) {
    double p = 0;
    assert(!a.left.empty() && a.left.size() == a.right.size());
    for (std::size_t i = 0; i < a.left.size(); ++i) {
      assert(std::isfinite(a.left[i]) && std::isfinite(a.right[i]) &&
             std::abs(a.left[i]) < .892f && std::abs(a.right[i]) < .892f);
      p += double(a.left[i]) * a.left[i];
    }
    assert(a.left.front() == 0 && a.left.back() == 0 && a.right.front() == 0 &&
           a.right.back() == 0);
    return p;
  };
  std::vector<std::uint8_t> bytes;
  for (std::size_t i = 0; i < sizeof(synthV8Hex) - 1; i += 2) {
    unsigned v = 0;
    assert(std::sscanf(synthV8Hex + i, "%2x", &v) == 1);
    bytes.push_back(std::uint8_t(v));
  }
  Recipe old;
  assert(bytes.size() == eighthRecipeBytes &&
         decode(bytes.data(), bytes.size(), old));
  assert(old.drums[dModel] == 9 && old.drums[dForm] == 2 &&
         old.drumColor == colorDefaults && (old.workshopLocks & (15ULL << 56)));
  auto packed = encode(old);
  Recipe restored;
  assert(packed.size() == recipeBytes &&
         decode(packed.data(), packed.size(), restored) &&
         encode(restored) == packed);
  // Exact bypass, stereo-linked fractional hold, quantization and post tone.
  Audio source;
  source.left.resize(512);
  source.right.resize(512);
  for (int i = 0; i < 512; ++i)
    source.left[i] = source.right[i] = .8f * std::sin(i * .071f);
  Audio a = source;
  assert(colorDrum(old, 44100, a, no) && a.left == source.left &&
         a.right == source.right);
  old.drumColor[cBits] = 4;
  old.drumColor[cRate] = 11025;
  assert(colorDrum(old, 44100, a, no));
  for (int i = 0; i < 512; i += 4)
    for (int j = 1; j < 4; ++j)
      assert(a.left[i] == a.left[i + j]);
  assert(a.left == a.right && a.left != source.left);
  old.drumColor[cBlend] = 0;
  a = source;
  assert(colorDrum(old, 44100, a, no) && a.left == source.left);
  old.drumColor[cBlend] = 1;
  old.drumColor[cLowpass] = 2000;
  assert(!colorDrum(old, 44100, a, [] { return true; }));
  for (int model = 0; model < drumModelCount; ++model) {
    Recipe r;
    initializeGenerator(r, drum);
    initializeDrumModel(r, model);
    r.v[length] = .18f;
    r.v[attack] = 0;
    r.v[width] = 1;
    Audio dry, colored, other;
    assert(render(r, 16000, dry, no, progress) && power(dry) > 0);
    r.drumColor = {8, 4000, 6000, .75f, 0, 0, 0, 0};
    assert(render(r, 16000, colored, no, progress) && power(colored) > 0 &&
           dry.left != colored.left);
    packed = encode(r);
    assert(decode(packed.data(), packed.size(), restored));
    assert(render(restored, 16000, other, no, progress) &&
           colored.left == other.left && colored.right == other.right);
    r.v[width] = 0;
    assert(render(r, 16000, other, no, progress) && other.left == other.right);
    // New processing defaults remain protected during bounded mutation.
    const auto color = r.drumColor;
    mutate(r, .5f);
    assert(r.drumColor == color && valid(r));
    clean(r);
    assert(r.drumColor == colorDefaults && valid(r));
  }
  Recipe r;
  initializeGenerator(r, drum);
  initializeDrumModel(r, 10);
  r.v[length] = .3f;
  r.v[attack] = 0;
  r.v[width] = 1;
  Audio base, varied;
  assert(render(r, 44100, base, no, progress) && power(base) > 0);
  const auto checkChange = [&](int field, float value) {
    Recipe v = r;
    assert(setDrum(v, field, value) && valid(v));
    assert(render(v, 44100, varied, no, progress) && power(varied) > 0 &&
           varied.left != base.left);
  };
  for (int shape = 1; shape < 4; ++shape)
    checkChange(dForm, float(shape));
  checkChange(dRing, 3.7f);
  checkChange(dContactX, .75f);
  checkChange(dContactY, .9f);
  checkChange(dDamping, .8f);
  checkChange(dBend, 0);
  checkChange(dSweep, 120);
  checkChange(dExtent, .6f);
  checkChange(dHit, 0);
  checkChange(dNoise, .4f);
  checkChange(dCharacter, .7f);
  Recipe extreme = r;
  for (int f = 1; f < drumFields; ++f)
    assert(setDrum(extreme, f, 1e6f));
  assert(valid(extreme) && render(extreme, 16000, varied, no, progress) &&
         power(varied) > 0);
  assert(!render(
      r, 44100, varied, [] { return true; }, progress));
  // Drive increases level-matched RMS, rather than changing the peak ceiling.
  r.drums[dContactX] = 0;
  r.drums[dNoise] = r.drums[dHit] = 0;
  r.drums[dBend] = 0;
  r.drumExtra[xSnap] = 0;
  assert(render(r, 44100, base, no, progress));
  r.drums[dCharacter] = .8f;
  assert(render(r, 44100, varied, no, progress) &&
         power(varied) > power(base) * 1.15);
  std::puts("Synth Drum Color: eleven-model rendering, exact bypass, linked "
            "holds, bit/rate/tone, dry mix, native drive RMS, FM "
            "controls/waveforms/extremes, protected mutation, v8 migration, v9 "
            "recall and cancellation: PASS");
}
