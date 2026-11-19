void testSynthBanksCore() {
  using namespace s3g_thru_synth;
  const auto draw = [](const Recipe &r, Audio &a, float rate = 16000.f) {
    return render(
        r, rate, a, [] { return false; }, [](unsigned) {});
  };
  const auto power = [](const std::vector<float> &v) {
    double sum = 0;
    for (float x : v) {
      assert(std::isfinite(x) && std::abs(x) <= .892f);
      sum += x * x;
    }
    return sum / v.size();
  };
  const auto amplitude = [](const Audio &a, float rate, float hz) {
    double re = 0, im = 0, win = 0;
    const auto start = a.left.size() / 4, n = a.left.size() / 2;
    for (std::size_t i = 0; i < n; ++i) {
      const double w = .5 - .5 * std::cos(6.283185307179586 * i / (n - 1)),
                   p = 6.283185307179586 * hz * (start + i) / rate;
      re += a.left[start + i] * w * std::cos(p);
      im += a.left[start + i] * w * std::sin(p);
      win += w;
    }
    return 2 * std::sqrt(re * re + im * im) / win;
  };
  // A fixed v2 fixture was captured before extending the codec.
  std::vector<std::uint8_t> old;
  for (std::size_t i = 0; i < sizeof(synthV2Hex) - 1; i += 2) {
    unsigned v = 0;
    assert(std::sscanf(synthV2Hex + i, "%2x", &v) == 1);
    old.push_back(std::uint8_t(v));
  }
  Recipe restored;
  assert(decode(old.data(), old.size(), restored) && restored.type == resynth &&
         restored.seed == 43799 && restored.spectral[rsPitch] == -7.5f &&
         restored.spectral[rsPartials] == 47);
  assert((restored.detailLocks & (1ULL << 37)) &&
         (restored.detailLocks & (1ULL << 54)));
  assert(encode(restored).size() == recipeBytes);
  auto corrupted = encode(restored);
  corrupted.back() ^= 1;
  const auto original = encode(restored);
  assert(!decode(corrupted.data(), corrupted.size(), restored) &&
         encode(restored) == original);
  Audio a, b;
  Recipe r;
  initializeGenerator(r, partialsType);
  r.v[length] = .5f;
  r.v[root] = 57;
  r.additive[aCount] = 1;
  assert(draw(r, a) && a.left == a.right && power(a.left) > .01 &&
         amplitude(a, 16000, 220) > .5);
  assert(draw(r, b) && a.left == b.left);
  for (auto &row : r.partials)
    row.fill(0);
  r.additive[aCount] = 32;
  r.ratios[3][7] = 2.7f;
  r.partials[3][7] = -.8f;
  assert(draw(r, a) && amplitude(a, 16000, 594) > .5);
  r.additive[aCount] = 31;
  assert(draw(r, b) && power(b.left) == 0);
  initializeGenerator(r, partialsType);
  r.additive[aCount] = 32;
  r.additive[aPhase] = .8f;
  r.additive[aDetune] = 12;
  r.additive[aDrift] = .7f;
  r.additive[aDecay] = .6f;
  r.v[width] = 1;
  assert(draw(r, a) && draw(r, b) && a.left == b.left && a.left != a.right &&
         power(a.left) > .001);
  ++r.seed;
  assert(draw(r, b) && a.left != b.left);
  for (int f = 0; f < 8; ++f) {
    assert(!setAdditive(r, f, NAN) && !setNoise(r, f, NAN) &&
           !setFm(r, f, INFINITY));
  }
  clean(r);
  assert(r.additive[aCount] == 32 && r.additive[aDetune] == 0 &&
         r.additive[aPhase] == 0 && r.additive[aDrift] == 0 &&
         r.additive[aDecay] == 0);
  initializeGenerator(r, noiseBank);
  r.v[width] = 0;
  r.noise[nCount] = 1;
  r.noise[nBandwidth] = .5f;
  assert(draw(r, a) && draw(r, b) && a.left == a.right && a.left == b.left &&
         power(a.left) > .001);
  double inside = 0, outside = 0;
  for (float hz = 200; hz <= 240; hz += 2)
    inside += amplitude(a, 16000, hz);
  for (float hz = 1100; hz <= 1140; hz += 2)
    outside += amplitude(a, 16000, hz);
  assert(inside > outside * 20);
  for (int color = 0; color < 3; ++color) {
    r.noise[nColor] = float(color);
    assert(draw(r, b) && power(b.left) > .001);
  }
  r.noise[nCount] = 32;
  r.noise[nBandwidth] = 48;
  r.noise[nStretch] = 4;
  r.noise[nTilt] = 12;
  r.noise[nMotion] = 24;
  r.noise[nRate] = 20;
  r.noise[nLeak] = .5f;
  r.v[width] = 1;
  assert(draw(r, a) && a.left != a.right && power(a.left) > .001);
  clean(r);
  assert(r.noise[nMotion] == 0 && r.noise[nLeak] == 0);
  initializeGenerator(r, fm);
  r.v[root] = 69;
  r.v[width] = 0;
  r.fmOsc[fIndex1] = 0;
  assert(draw(r, a) && amplitude(a, 16000, 440) > .5 && a.left == a.right);
  r.fmOsc[fIndex1] = 2;
  r.fmOsc[fIndex2] = 3;
  std::vector<float> previous;
  for (int alg = 0; alg < 4; ++alg) {
    r.fmOsc[fAlgorithm] = float(alg);
    assert(draw(r, a) && draw(r, b) && a.left == b.left && a.left == a.right &&
           power(a.left) > .001);
    if (!previous.empty())
      assert(previous != a.left);
    previous = a.left;
  }
  r.fmOsc[fAlgorithm] = 0;
  r.fmOsc[fRatio1] = 7.3f;
  r.fmOsc[fIndex1] = 1;
  r.fmOsc[fIndex2] = 0;
  assert(draw(r, a));
  const auto pass = amplitude(a, 16000, 3652),
             alias = amplitude(a, 16000, 5924);
  assert(pass > .05 && alias < pass * .02);
  r.fmOsc[fDetune] = 20;
  r.v[width] = 1;
  assert(draw(r, a) && a.left != a.right);
  r.v[width] = 0;
  assert(draw(r, a) && a.left == a.right);
  r.fmOsc[fFeedback] = .9f;
  clean(r);
  assert(r.fmOsc[fFeedback] == 0 && r.fmOsc[fDetune] == 0 &&
         r.fmOsc[fRatio1] == 7.3f);
  r.fmOsc[fMix] = .372f;
  r.detailLocks |= 1ULL << 61;
  mutate(r, .8f);
  assert(r.fmOsc[fMix] == .372f);
  for (int type = partialsType; type <= fm; ++type) {
    r = {};
    initializeGenerator(r, type);
    r.v[length] = .04f;
    r.v[loop] = 1;
    for (float rate : {8000.f, 44100.f, 96000.f}) {
      assert(draw(r, a, rate) && power(a.left) > .0001 &&
             a.left.size() == std::size_t(std::llround(rate * .04)));
      assert(std::abs(a.left.front() - a.left.back()) < .35f);
    }
    assert(!render(
        r, 16000, a, [] { return true; }, [](unsigned) {}));
    r.v[loop] = 0;
    r.v[level] = 0;
    assert(draw(r, a) && power(a.left) == 0);
  }
  std::puts(
      "Synth banks: v2 migration, 32 additive components, signed/inharmonic "
      "editing, drift/stereo, tuned noise bandwidth/color/extremes, FM "
      "routing/antialiasing, protected bit61, loops/rates/cancellation: PASS");
}
