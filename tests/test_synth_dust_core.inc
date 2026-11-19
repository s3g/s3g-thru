#include "synth_v9_fixture.h"
void testSynthDustCore() {
  using namespace s3g_thru_synth;
  const auto no = [] { return false; };
  const auto report = [](unsigned) {};
  const auto power = [](const Audio &a) {
    double p = 0;
    assert(!a.left.empty() && a.left.size() == a.right.size());
    for (std::size_t i = 0; i < a.left.size(); ++i) {
      assert(std::isfinite(a.left[i]) && std::isfinite(a.right[i]) &&
             std::abs(a.left[i]) < .892f && std::abs(a.right[i]) < .892f);
      p += double(a.left[i]) * a.left[i];
    }
    return p;
  };
  std::vector<std::uint8_t> bytes;
  for (std::size_t i = 0; i < sizeof(synthV9Hex) - 1; i += 2) {
    unsigned v = 0;
    assert(std::sscanf(synthV9Hex + i, "%2x", &v) == 1);
    bytes.push_back(std::uint8_t(v));
  }
  Recipe legacy;
  assert(bytes.size() == ninthRecipeBytes &&
         decode(bytes.data(), bytes.size(), legacy));
  assert(legacy.type == drum && legacy.drums[dModel] == 10 &&
         legacy.drumColor[cBits] == 8 && legacy.dustParams == dustDefaults &&
         legacy.dustLocks == 1);
  // Old formats must not accept a generator which did not exist then.
  bytes[4] = 12;
  std::vector<std::uint8_t> body(bytes.begin(), bytes.end() - 4);
  const auto crc = checksum(body);
  for (int j = 0; j < 4; ++j)
    bytes[bytes.size() - 4 + j] = std::uint8_t(crc >> (8 * j));
  Recipe rejected;
  assert(!decode(bytes.data(), bytes.size(), rejected));
  Recipe r;
  initializeGenerator(r, dust);
  r.v[length] = .3f;
  r.v[attack] = 0;
  r.v[release] = 2;
  r.v[width] = 1;
  r.dustParams[uRate] = 40;
  r.dustParams[uSize] = 4;
  r.dustParams[uRepeat] = 3;
  std::vector<float> previous;
  for (int mode = 0; mode < 6; ++mode) {
    assert(setDust(r, uMode, float(mode)) && valid(r));
    Audio a, b;
    assert(render(r, 16000, a, no, report) && render(r, 16000, b, no, report) &&
           a.left == b.left && a.right == b.right && power(a) > 0 &&
           a.left != a.right);
    assert(a.left.front() == 0 && a.left.back() == 0 && a.right.front() == 0 &&
           a.right.back() == 0);
    if (!previous.empty())
      assert(previous != a.left);
    previous = a.left;
    const auto packed = encode(r);
    Recipe copy;
    assert(packed.size() == 1408 &&
           decode(packed.data(), packed.size(), copy) &&
           encode(copy) == packed);
    assert(render(copy, 16000, b, no, report) && a.left == b.left &&
           a.right == b.right);
    copy.seed++;
    assert(render(copy, 16000, b, no, report) && a.left != b.left);
    copy = r;
    copy.v[width] = 0;
    assert(render(copy, 48000, b, no, report) && b.left == b.right &&
           power(b) > 0);
    copy = r;
    copy.v[loop] = 1;
    assert(render(copy, 16000, b, no, report) && power(b) > 0);
    Audio again;
    assert(render(copy, 16000, again, no, report) && again.left == b.left &&
           again.right == b.right && b.left.size() == 4800);
    for (int field = 0; field < dustFields; ++field) {
      for (float limit : {-1e9f, 1e9f}) {
        copy = r;
        assert(setDust(copy, field, limit) && valid(copy));
        assert(render(copy, 8000, b, no, report) && power(b) > 0);
      }
      copy = r;
      assert(!setDust(copy, field, NAN));
      copy.dustParams[field] = NAN;
      assert(!valid(copy));
    }
    copy = r;
    copy.dustLocks = 0xfff;
    const auto controls = copy.dustParams;
    mutate(copy, 1);
    assert(copy.dustParams == controls && valid(copy));
    copy.dustLocks = 0;
    mutate(copy, .5f);
    assert(copy.dustParams != controls && valid(copy));
    const auto modeBefore = copy.dustParams[uMode];
    clean(copy);
    assert(copy.dustParams[uMode] == modeBefore &&
           copy.dustParams[uFloor] == 0 && copy.dustParams[uRepeat] == 1 &&
           copy.dustParams[uBits] == 24 && valid(copy));
  }
  // Combined extrema: low-bit brown material and very short softened events
  // retain audible energy instead of rounding/windowing everything to zero.
  for (int mode = 0; mode < 6; ++mode) {
    Recipe quiet;
    quiet.type = dust;
    quiet.v[length] = .3f;
    quiet.v[attack] = 0;
    quiet.dustParams[uMode] = float(mode);
    quiet.dustParams[uColor] = 2;
    quiet.dustParams[uBits] = 2;
    Audio audio;
    assert(render(quiet, 44100, audio, no, report) && power(audio) > 0);
    quiet.dustParams[uSize] = .02f;
    quiet.dustParams[uEdge] = 1;
    assert(render(quiet, 8000, audio, no, report) && power(audio) > 0);
  }
  // Greater density must produce more occupied audio, not just a label change.
  r = {};
  initializeGenerator(r, dust);
  r.v[length] = 1;
  r.v[attack] = 0;
  r.dustParams = {4, 5, 1, 0, 20000, 0, 24, 0, 0, 0, 1, 0};
  Audio sparse, dense;
  assert(render(r, 16000, sparse, no, report));
  r.dustParams[uRate] = 160;
  assert(render(r, 16000, dense, no, report) &&
         power(dense) > power(sparse) * 3);
  r.dustParams[uRate] = .1f;
  r.v[loop] = 1;
  assert(render(r, 16000, dense, no, report));
  assert(std::abs(dense.left.front() - dense.left.back()) < 1e-5f);
  r.v[loop] = 0;
  // Event-edge control softens the selected click, including its polarity edge.
  r.dustParams = {3, 10, 8, 0, 20000, 0, 24, 0, 0, 0, 1, 0};
  assert(render(r, 16000, sparse, no, report));
  r.dustParams[uEdge] = 1;
  assert(render(r, 16000, dense, no, report));
  const auto step = [](const Audio &a) {
    float m = 0;
    for (std::size_t i = 1; i < a.left.size(); ++i)
      m = std::max(m, std::abs(a.left[i] - a.left[i - 1]));
    return m;
  };
  assert(step(dense) < step(sparse) * .5f);
  // Density is not a pitch row and must not be quantized to the captured scale.
  r.scale = 1;
  r.curves = {};
  const auto seed = r.seed;
  randomCurve(r, 0, .15f, .35f, 1, false);
  assert(r.seed != seed && r.curves[0].p[2].value >= .15f &&
         r.curves[0].p[2].value <= .35f);
  r.dustParams[uMode] = 5;
  r.dustParams[uFloor] = 0;
  assert(!dustAvailable(r, uColor));
  r.dustParams[uFloor] = .5f;
  assert(dustAvailable(r, uColor));
  r.dustLocks = 1u << 12;
  assert(!valid(r));
  r.dustLocks = 1;
  int polls = 0;
  assert(!render(
      r, 16000, dense, [&] { return ++polls >= 3; }, report));
  std::puts("Synth Dust: six distinct deterministic modes, stereo/mono, "
            "density/edge response, bounded controls/extremes, loop seams, v9 "
            "migration/type guard, v10 recall, protected mutation, continuous "
            "density curves and cancellation: PASS");
}
