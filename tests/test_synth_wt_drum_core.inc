#include "synth_v10_fixture.h"
void testSynthWtDrumCore() {
  using namespace s3g_thru_synth;
  const auto no = [] { return false; };
  const auto report = [](unsigned) {};
  Recipe old;
  assert(decode(synthV10Fixture.data(), synthV10Fixture.size(), old));
  assert(old.type == dust && old.seed == 19231 &&
         old.dustParams[uRate] == 37.5f);
  assert(encode(old) == std::vector<std::uint8_t>(synthV10Fixture.begin(),
                                                  synthV10Fixture.end()));
  const auto check = [](const Audio &a) {
    double energy = 0, mean = 0;
    float peak = 0;
    assert(a.left.size() == a.right.size() && !a.left.empty());
    for (std::size_t i = 0; i < a.left.size(); ++i) {
      const float l = a.left[i], rr = a.right[i];
      assert(std::isfinite(l) && std::isfinite(rr) && std::abs(l) < .892f &&
             std::abs(rr) < .892f);
      energy += double(l) * l;
      mean += l;
      peak = std::max(peak, std::abs(l));
    }
    assert(a.left.front() == 0 && a.right.front() == 0 && a.left.back() == 0 &&
           a.right.back() == 0);
    assert(std::abs(mean / a.left.size()) < .012);
    return std::make_pair(energy, peak);
  };
  for (int m = 0; m < wtModelCount; ++m) {
    Recipe r;
    initializeGenerator(r, wtDrum);
    initializeWtDrumModel(r, m);
    r.v[length] = .3f;
    assert(valid(r));
    const auto packed = encode(r);
    Recipe restored;
    assert(packed.size() == wtRecipeBytes && packed[0] == 11 &&
           decode(packed.data(), packed.size(), restored) &&
           encode(restored) == packed);
    for (std::size_t n = 0; n < packed.size(); ++n)
      assert(!decode(packed.data(), n, restored));
    auto corrupt = packed;
    corrupt[packed.size() - 5] ^= 0x10;
    assert(!decode(corrupt.data(), corrupt.size(), restored));
    Audio a, b;
    assert(render(r, 48000, a, no, report) && render(r, 48000, b, no, report) &&
           a.left == b.left && a.right == b.right && a.left == a.right);
    assert(check(a).first > 1 && check(a).second > .5f);
    double rms = 0;
    float transient = 0;
    for (int i = 0; i < 4800; ++i) {
      rms += a.left[i] * a.left[i];
      if (i < 480)
        transient = std::max(transient, std::abs(a.left[i]));
    }
    assert(std::sqrt(rms / 4800) > .06 && transient > .1f);
    r.v[width] = 1;
    r.wt[wtDetune] = 12;
    assert(render(r, 48000, b, no, report) && b.left != b.right &&
           check(b).first > 1);
    double stereo = 0, mono = 0;
    for (std::size_t i = 0; i < b.left.size(); ++i) {
      stereo += b.left[i] * b.left[i] + b.right[i] * b.right[i];
      const double x = (b.left[i] + b.right[i]) * .5;
      mono += x * x;
    }
    assert(mono > stereo * .08);
    for (float sr : {8000.f, 44100.f, 96000.f, 192000.f}) {
      r.v[length] = .04f;
      assert(render(r, sr, b, no, report));
      check(b);
    }
    r.v[length] = .2f;
    r.v[loop] = 1;
    assert(render(r, 16000, b, no, report));
    for (float x : b.left)
      assert(std::isfinite(x));
    assert(!render(
        r, 48000, b, [] { return true; }, report));
    r.v[loop] = 0;
    r.v[level] = 0;
    assert(render(r, 16000, b, no, report) && check(b).first == 0);
    for (int seed = 1; seed < 20; ++seed) {
      r.seed = seed;
      const float model = r.wt[wtModel], tune = r.wt[wtTune];
      mutate(r, .6f);
      assert(valid(r) && r.wt[wtModel] == model && r.wt[wtTune] == tune);
    }
  }
  // Every table and frame is continuous at the cycle seam; mips share phase.
  Recipe r;
  initializeGenerator(r, wtDrum);
  auto tables = std::make_unique<WtDrumTables>();
  for (int bank = 0; bank < wtTableCount; ++bank) {
    r.wt[wtTableA] = r.wt[wtTableB] = float(bank);
    assert(tables->prepare(r, no));
    for (const auto &osc : tables->banks)
      for (const auto &frame : osc)
        for (const auto &cycle : frame) {
          assert(cycle.front() == cycle.back());
          for (float x : cycle)
            assert(std::isfinite(x));
        }
    for (float pos : {0.f, .33f, 1.f})
      assert(std::abs(tables->read(0, 1 - 1e-9, pos, 500, 48000) -
                      tables->read(0, 1e-9, pos, 500, 48000)) < 1e-5);
  }
  for (int f = 0; f < wtFields; ++f) {
    Recipe x = r;
    assert(!setWtDrum(x, f, NAN));
    assert(setWtDrum(x, f, -1e9) && valid(x));
    assert(setWtDrum(x, f, 1e9) && valid(x));
  }
  // Combined extrema stay bounded through oversampling, filtering and
  // finishing.
  for (int high = 0; high < 2; ++high) {
    Recipe x;
    initializeGenerator(x, wtDrum);
    for (int f = 1; f < wtFields; ++f)
      setWtDrum(x, f, high ? wtMax[f] : wtMin[f]);
    x.v[length] = .08f;
    x.v[width] = 1;
    Audio a;
    assert(render(x, 8000, a, no, report));
    check(a);
  }
  // Parameter and modulation changes alter the generated sample, not just
  // metadata.
  r = {};
  initializeGenerator(r, wtDrum);
  r.v[length] = .12f;
  r.wt[wtTone] = .5f;
  Audio base, changed;
  assert(render(r, 16000, base, no, report));
  for (int f : {wtTableA, wtTableB, wtBlend, wtScan, wtImpact, wtRatio, wtFm,
                wtSub, wtNoise, wtTone, wtBend, wtDrive, wtComp, wtEdge}) {
    auto x = r;
    setWtDrum(x, f, f == wtTone ? wtMin[f] : wtMax[f]);
    assert(render(x, 16000, changed, no, report) && changed.left != base.left);
  }
  for (int row = 0; row < 4; ++row) {
    auto x = r;
    x.curves[row].p.back().value = limits(x)[row] * .2f;
    assert(render(x, 16000, changed, no, report) && changed.left != base.left);
  }
  std::puts("WT Drum DSP: six punchy starts, eight continuous mipmapped "
            "tables, body/transient RMS, stereo/mono, deterministic seed, "
            "8–192 kHz, extrema/cancellation, curves, protection and v10 byte "
            "identity/v11 recall: PASS");
}
