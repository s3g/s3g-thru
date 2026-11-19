#include "synth_v6_fixture.h"
void testSynthMusicCore() {
  using namespace s3g_thru_synth;
  const auto no = [] { return false; };
  const auto progress = [](unsigned) {};
  std::vector<std::uint8_t> bytes;
  for (std::size_t i = 0; i < sizeof(synthV6Hex) - 1; i += 2) {
    unsigned value = 0;
    assert(std::sscanf(synthV6Hex + i, "%2x", &value) == 1);
    bytes.push_back(std::uint8_t(value));
  }
  Recipe migrated;
  assert(bytes.size() == sixthRecipeBytes &&
         decode(bytes.data(), bytes.size(), migrated));
  assert(migrated.type == subharmonic && migrated.sub[sHarmonics] == 8 &&
         migrated.sub[sSubs] == 4 && migrated.sub[sDetune] == 0 &&
         migrated.sub[sDrift] == 0 && migrated.ratios[2][0] == 2);
  assert(encode(migrated).size() == recipeBytes);
  auto bad = bytes;
  bad[bad.size() - 7] |= 1; // Legacy reserved protection bit 40.
  const auto check =
      checksum(std::vector<std::uint8_t>(bad.begin(), bad.end() - 4));
  for (int i = 0; i < 4; ++i)
    bad[bad.size() - 4 + i] = std::uint8_t(check >> (i * 8));
  Recipe rejected;
  assert(!decode(bad.data(), bad.size(), rejected));
  const auto energy = [](const Audio &a, float hz) {
    double re = 0, im = 0;
    for (std::size_t i = 1600; i < 6400; ++i) {
      re += a.left[i] * std::cos(Plan::tau * hz * i / 16000);
      im += a.left[i] * std::sin(Plan::tau * hz * i / 16000);
    }
    return re * re + im * im;
  };
  Recipe bank;
  initializeGenerator(bank, subharmonic);
  bank.v[length] = .5f;
  bank.v[root] = 69;
  bank.v[attack] = 0;
  bank.v[release] = 0;
  bank.sub[sHarmonics] = 1;
  bank.sub[sSubs] = 1;
  bank.sub[sBalance] = 0;
  Audio harmonic, sub, mixed, edited;
  assert(render(bank, 16000, harmonic, no, progress));
  bank.sub[sBalance] = 1;
  assert(render(bank, 16000, sub, no, progress));
  assert(energy(harmonic, 440) > energy(harmonic, 220) * 1e6 &&
         energy(sub, 220) > energy(sub, 440) * 1e6);
  assert(energy(harmonic, 880) < energy(harmonic, 440) * 1e-6 &&
         energy(sub, 660) < energy(sub, 220) * 1e-6);
  bank.sub[sBalance] = .5f;
  assert(render(bank, 16000, mixed, no, progress));
  assert(energy(mixed, 440) > 1 && energy(mixed, 220) > 1);
  bank.ratios[2][0] = 4;
  assert(render(bank, 16000, edited, no, progress) &&
         energy(edited, 110) > energy(edited, 220) * 1e5);
  bank.partials[2][0] = 0;
  assert(render(bank, 16000, edited, no, progress) &&
         energy(edited, 220) < energy(edited, 440) * 1e-6);
  for (int field = 0; field < 8; ++field)
    for (float value : {subMin[field], subMax[field]}) {
      Recipe extreme;
      initializeGenerator(extreme, subharmonic);
      extreme.v[length] = .1f;
      setWorkshop(extreme, 1, field, value);
      Audio a;
      assert(render(extreme, 48000, a, no, progress));
      for (float x : a.left)
        assert(std::isfinite(x) && std::abs(x) < .892f);
    }
  Recipe rhythmic;
  initializeGenerator(rhythmic, drum);
  rhythmic.v[length] = 2;
  rhythmic.events[eMode] = 1;
  rhythmic.events[eCount] = 16;
  auto positions = eventPlacements(rhythmic, 16000);
  assert(positions.size() == 8);
  for (std::size_t i = 0; i < positions.size(); ++i)
    assert(positions[i].frame == i * 4000);
  rhythmic.timing[eDivision] = 7;
  positions = eventPlacements(rhythmic, 16000);
  assert(positions.size() == 16);
  for (std::size_t i = 0; i < positions.size(); ++i)
    assert(positions[i].frame == i * 2000);
  rhythmic.timing[eDivision] = 6;
  rhythmic.timing[eBpm] = 120;
  positions = eventPlacements(rhythmic, 44100);
  for (std::size_t i = 0; i < positions.size(); ++i)
    assert(std::abs(double(positions[i].frame) - i * 7350.) < .6);
  rhythmic.timing[eDivision] = 7;
  rhythmic.timing[eSwing] = .5;
  positions = eventPlacements(rhythmic, 16000);
  assert(positions[1].frame == 2500 && positions[2].frame == 4000 &&
         positions[3].frame == 6500);
  rhythmic.events[eJitter] = 1;
  rhythmic.timing[eSwing] = .75;
  for (int seed = 0; seed < 50; ++seed) {
    rhythmic.seed = seed;
    positions = eventPlacements(rhythmic, 44100);
    assert(positions.front().frame == 0);
    for (std::size_t i = 1; i < positions.size(); ++i)
      assert(positions[i].frame > positions[i - 1].frame + 2000);
  }
  // At every subdivision, the audio gate closes before the next grid point.
  for (int division : {5, 6, 7, 8, 9, 11}) {
    Recipe r;
    initializeGenerator(r, drum);
    r.v[length] = 1;
    r.v[attack] = 0;
    r.v[release] = 0;
    r.events[eMode] = 1;
    r.events[eCount] = 32;
    r.timing[eDivision] = float(division);
    r.timing[eGate] = .5f;
    r.drumExtra[xEdge] =
        2; // Symmetric test signal isolates gate spacing from onset shaping.
    const auto gate = eventWindowFrames(r, 16000);
    Audio audio;
    const auto child = [&](const Recipe &c, Audio &a, const auto &) {
      const auto n = std::size_t(std::llround(c.v[length] * 16000));
      a.left.resize(n);
      a.right.resize(n);
      for (std::size_t i = 0; i < n; ++i)
        a.left[i] = a.right[i] =
            std::sin(Plan::tau * 8 * (double(i) + .5) / gate);
      return true;
    };
    assert(renderEventField(r, 16000, audio, no, progress, child));
    const auto schedule = eventPlacements(r, 16000);
    for (std::size_t i = 0; i + 1 < schedule.size(); ++i) {
      const auto start = schedule[i].frame, next = schedule[i + 1].frame;
      float peak = 0;
      for (std::size_t j = start + gate + 4; j + 4 < next; ++j)
        peak = std::max(peak, std::abs(audio.left[j]));
      assert(peak < 1e-5f);
    }
  }
  // FREE uses the original absolute lifetime and ignores retained swing.
  rhythmic.events[eJitter] = 0;
  rhythmic.timing[eClock] = 0;
  rhythmic.events[eLife] = 90;
  positions = eventPlacements(rhythmic, 16000);
  assert(positions.size() == 16 && positions[1].frame == 2000 &&
         eventWindowFrames(rhythmic, 16000) == 1440);
  std::puts("Synth musical banks: clean harmonic/subharmonic spectral "
            "isolation, editable divisors/levels, v6 migration, extremes, "
            "exact straight/triplet/swing timing, ordered humanization, dense "
            "audible gates and free-time recall: PASS");
}
