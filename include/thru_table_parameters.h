#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
namespace s3g_thru_table {
constexpr int fields = 40, controlOffset = 256;
enum Field {
  type,
  tune,
  width,
  level,
  art,
  decay,
  noise,
  drive,
  shape,
  pulse,
  detune,
  sub,
  cutoff,
  resonance,
  points,
  evolution,
  curve,
  drift,
  drift_rate,
  dynamics,
  seed,
  model,
  bank_a,
  pos_a,
  bank_b,
  pos_b,
  blend,
  ratio,
  fm,
  wt_detune,
  bend,
  pitch_decay,
  scan,
  scan_decay,
  wt_sub,
  impact,
  noise_decay,
  color,
  tone,
  compression
};
inline constexpr std::array<float, fields> low{
    0.0f,   -48.0f, 0.0f,    0.0f, 0.0f, 10.0f, 0.0f, 0.0f,   0.0f, 5.0f,
    0.0f,   0.0f,   20.0f,   0.0f, 1.0f, 1.0f,  0.0f, 0.0f,   1.0f, 0.0f,
    0.0f,   0.0f,   0.0f,    0.0f, 0.0f, 0.0f,  0.0f, 0.125f, 0.0f, 0.0f,
    -48.0f, 1.0f,   -100.0f, 1.0f, 0.0f, 0.0f,  1.0f, 0.0f,   0.0f, 0.0f},
    high{2.0f,   48.0f,   100.0f,  100.0f, 1.0f,     12000.0f, 100.0f, 100.0f,
         3.0f,   95.0f,   50.0f,   100.0f, 20000.0f, 95.0f,    32.0f,  4096.0f,
         100.0f, 12.0f,   1000.0f, 100.0f, 65535.0f, 5.0f,     7.0f,   100.0f,
         7.0f,   100.0f,  100.0f,  16.0f,  100.0f,   50.0f,    48.0f,  2000.0f,
         100.0f, 4000.0f, 100.0f,  100.0f, 4000.0f,  3.0f,     100.0f, 100.0f},
    defaults{0.0f,   0.0f,  40.0f,  75.0f, 0.0f,     650.0f, 0.0f,  0.0f,
             0.0f,   50.0f, 0.0f,   0.0f,  18000.0f, 0.0f,   4.0f,  16.0f,
             100.0f, 0.15f, 100.0f, 20.0f, 1.0f,     0.0f,   0.0f,  0.0f,
             1.0f,   25.0f, 15.0f,  1.0f,  0.0f,     0.0f,   24.0f, 30.0f,
             30.0f,  60.0f, 35.0f,  25.0f, 40.0f,    0.0f,   65.0f, 10.0f};
inline constexpr const char *keys[]{
    "table_type",        "table_tune",        "table_width",
    "table_level",       "table_art",         "table_decay",
    "table_noise",       "table_drive",       "table_shape",
    "table_pulse",       "table_detune",      "table_sub",
    "table_cutoff",      "table_resonance",   "table_points",
    "table_evolution",   "table_curve",       "table_drift",
    "table_drift_rate",  "table_dynamics",    "table_seed",
    "table_model",       "table_bank_a",      "table_pos_a",
    "table_bank_b",      "table_pos_b",       "table_blend",
    "table_ratio",       "table_fm",          "table_wt_detune",
    "table_bend",        "table_pitch_decay", "table_scan",
    "table_scan_decay",  "table_wt_sub",      "table_impact",
    "table_noise_decay", "table_color",       "table_tone",
    "table_compression"};
struct Config {
  std::array<float, fields> v = defaults;
};
inline int index(const char *key) {
  for (int i = 0; i < fields; ++i)
    if (!std::strcmp(key, keys[i]))
      return i;
  return -1;
}
inline bool discrete(int f) {
  return f == type || f == art || f == shape || f == seed || f == model ||
         f == bank_a || f == bank_b || f == color;
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
  const int t = int(c.v[type]);
  if (f == decay)
    return c.v[art] > .5f;
  if (f < 8)
    return true;
  if (f < 14)
    return t == 0 && (f != pulse || c.v[shape] == 3);
  if (f < 21)
    return t == 1;
  return t == 2 && ((f != noise_decay && f != color) || c.v[noise] > 0);
}
inline void start(Config &c, int which) {
  which = std::clamp(which, 0, 5);
  c.v[model] = float(which);
  c.v[type] = 2;
  c.v[art] = 1;
  // Original WT percussion starts, expressed through the live shared controls.
  const float starts[6][18] = {
      {650, 0, 0, 0, 1, 25, 15, 1, 0, 24, 30, 30, 60, 35, 25, 40, 65, 10},
      {240, 65, 2, 30, 3, 60, 35, 1.5f, 12, 12, 18, 50, 45, 5, 40, 140, 85, 20},
      {550, 3, 1, 20, 0, 0, 15, 1.5f, 8, 12, 60, 35, 100, 25, 25, 40, 70, 10},
      {90, 70, 4, 70, 7, 80, 50, 3.7f, 25, 7, 12, 30, 35, 0, 35, 70, 90, 10},
      {200, 85, 2, 50, 4, 80, 35, 1.7f, 10, 3, 12, 30, 40, 0, 35, 100, 90, 15},
      {1800, 20, 4, 80, 7, 70, 50, 2.71f, 45, 5, 40, 60, 400, 5, 30, 160, 75,
       20}};
  const int indices[]{decay,       noise, bank_a,     pos_a,  bank_b,
                      pos_b,       blend, ratio,      fm,     bend,
                      pitch_decay, scan,  scan_decay, wt_sub, impact,
                      noise_decay, tone,  compression};
  for (int i = 0; i < 18; ++i)
    c.v[indices[i]] = starts[which][i];
  c.v[color] = which == 3 ? 3 : 0;
}
struct Program {
  std::array<Config, 4> patches{};
  Program() {
    patches[1].v[shape] = 1;
    patches[1].v[sub] = 20;
    patches[2].v[type] = 1;
    patches[2].v[points] = 8;
    patches[3].v[shape] = 2;
    patches[3].v[cutoff] = 6000;
  }
};
} // namespace s3g_thru_table
