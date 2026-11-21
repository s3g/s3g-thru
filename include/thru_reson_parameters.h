#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
namespace s3g_thru_reson {
constexpr int fields = 32, controlOffset = 544;
enum Field {
  mode,
  tune,
  width,
  level,
  model,
  decay,
  brightness,
  damping,
  stretch,
  inharmonic,
  mass,
  bend,
  sweep,
  noise,
  noise_decay,
  color,
  hit,
  hardness,
  hit_decay,
  punch,
  position,
  coupling,
  detune,
  seed,
  drive,
  clip,
  bias,
  sag,
  recovery,
  tone,
  mix,
  output
};
inline constexpr std::array<float, fields> low{
    0.0f,   -48.0f, 0.0f, 0.0f, 0.0f,    10.0f, 0.0f, 0.0f,   0.5f, 0.0f,  0.0f,
    -48.0f, 1.0f,   0.0f, 1.0f, 0.0f,    0.0f,  0.0f, 0.2f,   0.0f, 0.0f,  0.0f,
    0.0f,   0.0f,   0.0f, 0.0f, -100.0f, 0.0f,  5.0f, 200.0f, 0.0f, -18.0f};
inline constexpr std::array<float, fields> high{
    1.0f,   48.0f,  100.0f, 100.0f, 7.0f,    8000.0f,  100.0f,  100.0f,
    2.0f,   100.0f, 100.0f, 48.0f,  2000.0f, 100.0f,   4000.0f, 100.0f,
    100.0f, 100.0f, 100.0f, 100.0f, 100.0f,  100.0f,   30.0f,   65535.0f,
    100.0f, 100.0f, 100.0f, 100.0f, 500.0f,  20000.0f, 100.0f,  12.0f};
inline constexpr std::array<float, fields> defaults{
    0.0f,  0.0f,   30.0f, 80.0f, 0.0f,  1000.0f,  55.0f,  35.0f,
    1.0f,  100.0f, 50.0f, 0.0f,  25.0f, 0.0f,     120.0f, 50.0f,
    10.0f, 60.0f,  4.0f,  35.0f, 20.0f, 10.0f,    0.0f,   1.0f,
    0.0f,  20.0f,  0.0f,  15.0f, 80.0f, 20000.0f, 100.0f, 0.0f};
inline constexpr const char *keys[]{
    "reson_mode",     "reson_tune",       "reson_width",       "reson_level",
    "reson_model",    "reson_decay",      "reson_brightness",  "reson_damping",
    "reson_stretch",  "reson_inharmonic", "reson_mass",        "reson_bend",
    "reson_sweep",    "reson_noise",      "reson_noise_decay", "reson_color",
    "reson_hit",      "reson_hardness",   "reson_hit_decay",   "reson_punch",
    "reson_position", "reson_coupling",   "reson_detune",      "reson_seed",
    "reson_drive",    "reson_clip",       "reson_bias",        "reson_sag",
    "reson_recovery", "reson_tone",       "reson_mix",         "reson_output"};
struct Config {
  std::array<float, fields> v = defaults;
};
inline bool discrete(int f) { return f == mode || f == model || f == seed; }
inline int index(const char *key) {
  if (std::strncmp(key, "reson_", 6))
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
  if ((f == bend || f == sweep) && c.v[mode] < .5f)
    return false;
  if (f == sweep && c.v[bend] == 0)
    return false;
  if ((f == noise_decay || f == color) &&
      (c.v[noise] == 0 || (f == noise_decay && c.v[mode] < .5f)))
    return false;
  if ((f == hardness || f == hit_decay) && c.v[hit] == 0)
    return false;
  if ((f == clip || f == bias || f == sag || f == recovery) && c.v[drive] == 0)
    return false;
  if (f == recovery && c.v[sag] == 0)
    return false;
  return true;
}
inline Config startingSound(int which, const Config &old) {
  Config c;
  auto &p = c.v;
  p[level] = old.v[level];
  p[width] = old.v[width];
  if (which < 1 || which > 8)
    return old;
  p[model] = which - 1;
  p[mode] = which == 1 ? 0.f : 1.f;
  switch (which) {
  case 1:
    p[hit] = 0;
    p[punch] = 0;
    p[decay] = 1800;
    p[coupling] = 15;
    break;
  case 2:
    p[tune] = -24;
    p[decay] = 550;
    p[brightness] = 25;
    p[damping] = 65;
    p[mass] = 85;
    p[bend] = 28;
    p[sweep] = 22;
    p[hit] = 25;
    p[hardness] = 70;
    p[hit_decay] = 4;
    p[punch] = 75;
    p[drive] = 15;
    break;
  case 3:
    p[tune] = -6;
    p[decay] = 240;
    p[noise] = 75;
    p[noise_decay] = 150;
    p[color] = 70;
    p[brightness] = 65;
    p[bend] = 7;
    p[sweep] = 12;
    p[hit] = 45;
    p[punch] = 70;
    p[drive] = 18;
    break;
  case 4:
    p[tune] = -12;
    p[decay] = 700;
    p[brightness] = 45;
    p[damping] = 45;
    p[mass] = 65;
    p[bend] = 10;
    p[sweep] = 30;
    p[noise] = 5;
    p[noise_decay] = 65;
    p[hit] = 30;
    p[punch] = 65;
    p[drive] = 12;
    break;
  case 5:
    p[decay] = 2000;
    p[brightness] = 75;
    p[damping] = 15;
    p[mass] = 25;
    p[hit] = 35;
    p[hardness] = 80;
    p[hit_decay] = 2;
    p[punch] = 45;
    p[drive] = 8;
    break;
  case 6:
    p[tune] = 24;
    p[decay] = 90;
    p[brightness] = 90;
    p[damping] = 80;
    p[mass] = 10;
    p[noise] = 55;
    p[noise_decay] = 65;
    p[color] = 95;
    p[hit] = 25;
    p[hit_decay] = 1;
    p[punch] = 45;
    p[drive] = 12;
    break;
  case 7:
    p[tune] = 12;
    p[decay] = 1700;
    p[brightness] = 85;
    p[damping] = 25;
    p[mass] = 15;
    p[noise] = 35;
    p[noise_decay] = 1200;
    p[color] = 85;
    p[hit] = 40;
    p[coupling] = 35;
    p[detune] = 6;
    p[punch] = 45;
    p[drive] = 10;
    break;
  case 8:
    p[tune] = -30;
    p[decay] = 2600;
    p[brightness] = 35;
    p[damping] = 45;
    p[mass] = 100;
    p[bend] = 8;
    p[sweep] = 40;
    p[noise] = 25;
    p[noise_decay] = 380;
    p[color] = 18;
    p[hit] = 45;
    p[hardness] = 35;
    p[hit_decay] = 10;
    p[punch] = 80;
    p[drive] = 12;
    break;
  }
  return c;
}
struct Program {
  std::array<Config, 4> patches{};
  Program() {
    patches[1] = startingSound(2, patches[1]);
    patches[2] = startingSound(3, patches[2]);
    patches[3] = startingSound(4, patches[3]);
  }
};
} // namespace s3g_thru_reson
