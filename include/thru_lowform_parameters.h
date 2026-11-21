#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
namespace s3g_thru_lowform {
constexpr int fields = 32, controlOffset = 704;
enum Field {
  body,
  tune,
  width,
  level,
  foundation_wave,
  foundation_octave,
  foundation_level,
  body_level,
  a,
  b,
  c,
  d,
  e,
  body_decay,
  texture_type,
  texture_level,
  texture_color,
  texture_track,
  texture_width,
  filter_type,
  cutoff,
  resonance,
  filter_drive,
  keytrack,
  filter_env,
  filter_decay,
  punch,
  punch_time,
  drive,
  low_return,
  texture_decay,
  texture_attack
};
inline constexpr std::array<float, fields> low{
    0.0f, -48.0f,  0.0f,  0.0f, 0.0f,   -1.0f, 0.0f, 0.0f, 0.0f,  0.0f,  0.0f,
    0.0f, 0.0f,    20.0f, 0.0f, 0.0f,   0.0f,  0.0f, 0.0f, 0.0f,  20.0f, 0.0f,
    0.0f, -100.0f, -8.0f, 5.0f, -24.0f, 1.0f,  0.0f, 0.0f, 20.0f, 0.0f};
inline constexpr std::array<float, fields> high{
    5.0f,   48.0f,   100.0f, 100.0f,  2.0f,     0.0f,     100.0f,   100.0f,
    100.0f, 100.0f,  100.0f, 100.0f,  100.0f,   12000.0f, 4.0f,     100.0f,
    100.0f, 100.0f,  100.0f, 5.0f,    18000.0f, 95.0f,    100.0f,   100.0f,
    8.0f,   4000.0f, 48.0f,  1000.0f, 100.0f,   100.0f,   12000.0f, 2000.0f};
inline constexpr std::array<float, fields> defaults{
    0.0f,  0.0f,   45.0f, 80.0f, 0.0f,    0.0f,     80.0f,    65.0f,
    42.0f, 48.0f,  40.0f, 24.0f, 45.0f,   12000.0f, 0.0f,     0.0f,
    52.0f, 72.0f,  45.0f, 2.0f,  1200.0f, 18.0f,    25.0f,    34.0f,
    1.4f,  180.0f, 3.0f,  48.0f, 0.0f,    72.0f,    12000.0f, 0.0f};
inline constexpr const char *keys[]{"lowform_body",
                                    "lowform_tune",
                                    "lowform_width",
                                    "lowform_level",
                                    "lowform_foundation_wave",
                                    "lowform_foundation_octave",
                                    "lowform_foundation_level",
                                    "lowform_body_level",
                                    "lowform_a",
                                    "lowform_b",
                                    "lowform_c",
                                    "lowform_d",
                                    "lowform_e",
                                    "lowform_body_decay",
                                    "lowform_texture_type",
                                    "lowform_texture_level",
                                    "lowform_texture_color",
                                    "lowform_texture_track",
                                    "lowform_texture_width",
                                    "lowform_filter_type",
                                    "lowform_cutoff",
                                    "lowform_resonance",
                                    "lowform_filter_drive",
                                    "lowform_keytrack",
                                    "lowform_filter_env",
                                    "lowform_filter_decay",
                                    "lowform_punch",
                                    "lowform_punch_time",
                                    "lowform_drive",
                                    "lowform_low_return",
                                    "lowform_texture_decay",
                                    "lowform_texture_attack"};
struct Config {
  std::array<float, fields> v = defaults;
};
inline bool discrete(int f) {
  return f == body || f == foundation_wave || f == foundation_octave ||
         f == texture_type || f == filter_type;
}
inline int index(const char *key) {
  if (std::strncmp(key, "lowform_", 8))
    return -1;
  for (int f = 0; f < fields; ++f)
    if (!std::strcmp(key, keys[f]))
      return f;
  return -1;
}
inline bool set(Config &c, int f, float x) {
  if (f < 0 || f >= fields || !std::isfinite(x))
    return false;
  c.v[f] = std::clamp(discrete(f) ? std::round(x) : x, low[f], high[f]);
  return true;
}
inline bool valid(const Config &c) {
  for (int f = 0; f < fields; ++f)
    if (!std::isfinite(c.v[f]) || c.v[f] < low[f] || c.v[f] > high[f] ||
        (discrete(f) && std::floor(c.v[f]) != c.v[f]))
      return false;
  return true;
}
inline bool applicable(const Config &c, int f) {
  if (f < 0 || f >= fields)
    return false;
  if (f == foundation_wave || f == foundation_octave || f == low_return)
    return c.v[foundation_level] > 0;
  if (f >= a && f <= body_decay)
    return c.v[body_level] > 0;
  if ((f >= texture_level && f <= texture_width) || f == texture_decay ||
      f == texture_attack)
    return c.v[texture_type] > 0 &&
           (f == texture_level || c.v[texture_level] > 0) &&
           (f != texture_track || c.v[texture_type] == 2 ||
            c.v[texture_type] == 4);
  if (f >= cutoff && f <= filter_decay)
    return (c.v[filter_type] > 0 || (c.v[body] == 2 && c.v[body_level] > 0)) &&
           (f != filter_decay || c.v[filter_env] != 0);
  if (f == punch_time)
    return c.v[punch] != 0;
  return true;
}
inline Config startingSound(int which, const Config &old) {
  if (which < 1 || which > 6)
    return old;
  Config patch;
  auto &p = patch.v;
  p[level] = old.v[level];
  p[width] = old.v[width];
  p[body] = float(which - 1);
  if (which == 2) {
    p[a] = 35;
    p[b] = 38;
    p[c] = 50;
    p[d] = 25;
    p[e] = 75;
    p[cutoff] = 2600;
  }
  if (which == 3) {
    p[a] = 35;
    p[b] = 70;
    p[c] = 38;
    p[d] = 50;
    p[e] = 50;
    p[cutoff] = 900;
    p[filter_env] = 3;
    p[filter_decay] = 220;
  }
  if (which == 4) {
    p[a] = 60;
    p[b] = 65;
    p[c] = 35;
    p[d] = 12;
    p[e] = 55;
    p[cutoff] = 1600;
  }
  if (which == 5) {
    p[a] = 30;
    p[b] = 50;
    p[c] = 65;
    p[d] = 40;
    p[e] = 35;
    p[cutoff] = 6000;
  }
  if (which == 6) {
    p[a] = 40;
    p[b] = 30;
    p[c] = 45;
    p[d] = 35;
    p[e] = 20;
    p[cutoff] = 3500;
  }
  return patch;
}
struct Program {
  std::array<Config, 4> patches{};
  Program() {
    patches[1] = startingSound(2, patches[1]);
    patches[2] = startingSound(3, patches[2]);
    patches[3] = startingSound(5, patches[3]);
  }
};
inline constexpr const char *bodyLabels[6][5] = {
    {"WAVE", "PRES", "PW", "CROS", "DRIV"},
    {"DRIV", "DETN", "VOIC", "DRFT", "TONE"},
    {"WAVE", "RES", "PW", "DRIV", "CUT"},
    {"WAVE", "SUB", "SMER", "NOIS", "DRIV"},
    {"VOWL", "SHIFT", "FOCS", "SRC", "DRIV"},
    {"WAVE", "RATI", "SWP", "DEC", "RING"}};
} // namespace s3g_thru_lowform
