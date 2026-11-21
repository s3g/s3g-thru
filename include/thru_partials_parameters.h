#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
namespace s3g_thru_partials {
constexpr int fields = 104, controlOffset = 576, macros = 24, ratios = 24,
              bands = 56, divisors = 88;
enum Field {
  type,
  tune,
  width,
  level,
  count,
  tilt,
  stretch,
  phase,
  harmonics,
  subs,
  balance,
  detune,
  drift,
  drift_rate,
  damping,
  time,
  gap,
  scatter,
  rise,
  sustain,
  decay,
  variation,
  pitch_jitter,
  shape
};
inline constexpr std::array<float, fields> low{
    0.0f,  -48.0f, 0.0f,  0.0f,  1.0f,  -24.0f, 0.25f, 0.0f,  1.0f,  0.0f,
    0.0f,  0.0f,   0.0f,  0.01f, 0.0f,  10.0f,  10.0f, 0.0f,  0.5f,  0.0f,
    1.0f,  0.0f,   0.0f,  0.25f, 1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  1.0f,   -1.0f, -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  -1.0f, -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  -1.0f, -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  -1.0f, -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,
    1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,   1.0f,  1.0f};
inline constexpr std::array<float, fields> high{
    2.0f,   48.0f,   100.0f,  100.0f, 32.0f, 12.0f,  4.0f,     100.0f,  16.0f,
    16.0f,  100.0f,  30.0f,   30.0f,  20.0f, 100.0f, 12000.0f, 2000.0f, 100.0f,
    200.0f, 1000.0f, 4000.0f, 100.0f, 12.0f, 4.0f,   64.0f,    64.0f,   64.0f,
    64.0f,  64.0f,   64.0f,   64.0f,  64.0f, 64.0f,  64.0f,    64.0f,   64.0f,
    64.0f,  64.0f,   64.0f,   64.0f,  64.0f, 64.0f,  64.0f,    64.0f,   64.0f,
    64.0f,  64.0f,   64.0f,   64.0f,  64.0f, 64.0f,  64.0f,    64.0f,   64.0f,
    64.0f,  64.0f,   1.0f,    1.0f,   1.0f,  1.0f,   1.0f,     1.0f,    1.0f,
    1.0f,   1.0f,    1.0f,    1.0f,   1.0f,  1.0f,   1.0f,     1.0f,    1.0f,
    1.0f,   1.0f,    1.0f,    1.0f,   1.0f,  1.0f,   1.0f,     1.0f,    1.0f,
    1.0f,   1.0f,    1.0f,    1.0f,   1.0f,  1.0f,   1.0f,     64.0f,   64.0f,
    64.0f,  64.0f,   64.0f,   64.0f,  64.0f, 64.0f,  64.0f,    64.0f,   64.0f,
    64.0f,  64.0f,   64.0f,   64.0f,  64.0f};
inline constexpr std::array<float, fields> defaults{
    0.0f,   0.0f,  35.0f, 80.0f, 16.0f, 0.0f,    1.0f,   0.0f,  8.0f,  4.0f,
    25.0f,  0.0f,  0.0f,  0.5f,  0.0f,  2000.0f, 150.0f, 20.0f, 10.0f, 20.0f,
    120.0f, 20.0f, 0.0f,  1.0f,  1.0f,  2.0f,    3.0f,   4.0f,  5.0f,  6.0f,
    7.0f,   8.0f,  9.0f,  10.0f, 11.0f, 12.0f,   13.0f,  14.0f, 15.0f, 16.0f,
    17.0f,  18.0f, 19.0f, 20.0f, 21.0f, 22.0f,   23.0f,  24.0f, 25.0f, 26.0f,
    27.0f,  28.0f, 29.0f, 30.0f, 31.0f, 32.0f,   1.0f,   1.0f,  1.0f,  1.0f,
    1.0f,   1.0f,  1.0f,  1.0f,  1.0f,  1.0f,    1.0f,   1.0f,  1.0f,  1.0f,
    1.0f,   1.0f,  1.0f,  1.0f,  1.0f,  1.0f,    1.0f,   1.0f,  1.0f,  1.0f,
    1.0f,   1.0f,  1.0f,  1.0f,  1.0f,  1.0f,    1.0f,   1.0f,  1.0f,  2.0f,
    3.0f,   4.0f,  5.0f,  6.0f,  7.0f,  8.0f,    9.0f,   10.0f, 11.0f, 12.0f,
    13.0f,  14.0f, 15.0f, 16.0f};
inline constexpr const char *keys[]{
    "partials_type",       "partials_tune",         "partials_width",
    "partials_level",      "partials_count",        "partials_tilt",
    "partials_stretch",    "partials_phase",        "partials_harmonics",
    "partials_subs",       "partials_balance",      "partials_detune",
    "partials_drift",      "partials_drift_rate",   "partials_damping",
    "partials_time",       "partials_gap",          "partials_scatter",
    "partials_rise",       "partials_sustain",      "partials_decay",
    "partials_variation",  "partials_pitch_jitter", "partials_shape",
    "partials_ratio_1",    "partials_ratio_2",      "partials_ratio_3",
    "partials_ratio_4",    "partials_ratio_5",      "partials_ratio_6",
    "partials_ratio_7",    "partials_ratio_8",      "partials_ratio_9",
    "partials_ratio_10",   "partials_ratio_11",     "partials_ratio_12",
    "partials_ratio_13",   "partials_ratio_14",     "partials_ratio_15",
    "partials_ratio_16",   "partials_ratio_17",     "partials_ratio_18",
    "partials_ratio_19",   "partials_ratio_20",     "partials_ratio_21",
    "partials_ratio_22",   "partials_ratio_23",     "partials_ratio_24",
    "partials_ratio_25",   "partials_ratio_26",     "partials_ratio_27",
    "partials_ratio_28",   "partials_ratio_29",     "partials_ratio_30",
    "partials_ratio_31",   "partials_ratio_32",     "partials_band_1",
    "partials_band_2",     "partials_band_3",       "partials_band_4",
    "partials_band_5",     "partials_band_6",       "partials_band_7",
    "partials_band_8",     "partials_band_9",       "partials_band_10",
    "partials_band_11",    "partials_band_12",      "partials_band_13",
    "partials_band_14",    "partials_band_15",      "partials_band_16",
    "partials_band_17",    "partials_band_18",      "partials_band_19",
    "partials_band_20",    "partials_band_21",      "partials_band_22",
    "partials_band_23",    "partials_band_24",      "partials_band_25",
    "partials_band_26",    "partials_band_27",      "partials_band_28",
    "partials_band_29",    "partials_band_30",      "partials_band_31",
    "partials_band_32",    "partials_divisor_1",    "partials_divisor_2",
    "partials_divisor_3",  "partials_divisor_4",    "partials_divisor_5",
    "partials_divisor_6",  "partials_divisor_7",    "partials_divisor_8",
    "partials_divisor_9",  "partials_divisor_10",   "partials_divisor_11",
    "partials_divisor_12", "partials_divisor_13",   "partials_divisor_14",
    "partials_divisor_15", "partials_divisor_16"};
struct Config {
  std::array<float, fields> v = defaults;
};
inline bool discrete(int f) { return f == type; }
inline int index(const char *key) {
  if (std::strncmp(key, "partials_", 9))
    return -1;
  for (int f = 0; f < fields; ++f)
    if (!std::strcmp(key, keys[f]))
      return f;
  return -1;
}
inline float resolved(const Config &c, int f) {
  return f == count && c.v[type] > 1.5f ? std::min(8.f, c.v[f]) : c.v[f];
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
  const int mode = int(c.v[type]);
  if (f >= harmonics && f <= balance)
    return mode == 1 && (f != balance || c.v[subs] > 0);
  if (f == count)
    return mode != 1;
  if (f >= gap && f <= shape)
    return mode == 2;
  if (f == drift_rate && c.v[drift] == 0)
    return false;
  if (f == damping || f == time)
    return mode != 2 && (f != time || c.v[damping] > 0);
  if (f >= divisors)
    return mode == 1;
  if (f >= ratios && f < bands && mode == 1 && f >= ratios + 16)
    return false;
  return true;
}
inline int componentField(const Config &c, int component) {
  return int(c.v[type]) == 1 && component >= 16 ? divisors + component - 16
                                                : ratios + component;
}
struct Program {
  std::array<Config, 4> patches{};
  Program() {
    patches[1].v[type] = 1;
    patches[2].v[type] = 2;
    patches[2].v[count] = 8;
    patches[3].v[stretch] = 1.12f;
    patches[3].v[tilt] = -6;
    patches[3].v[phase] = 75;
  }
};
} // namespace s3g_thru_partials
