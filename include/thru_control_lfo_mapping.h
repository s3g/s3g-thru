#pragma once
#include "thru_control_lfo.h"
namespace s3g_thru_lfo {
inline float mapped(const DestinationInfo &d, float base, Modulation m) {
  if (!(d.high > d.low)) return d.low;
  // BASE keeps its existing excursion, taper and exact neutral bypass.
  const float amount = std::clamp(m.base, -1.f, 1.f);
  const float manual =
      amount == 0
          ? base
          : std::clamp(d.octaves ? base * std::exp2(amount * d.octaves)
                                 : base + amount * (d.high - d.low) * .5f,
                       d.low, d.high);
  if (m.span <= 1e-8f)
    return manual;
  // Include valid manual sentinel settings (for example Tape Hiss Off) in the
  // sweep, so enabling AUTO never jumps from that setting to the curated floor.
  const double low = std::min(d.low, manual), high = std::max(d.high, manual),
               range = high - low;
  const double span = std::min(1.f, m.span), radius = span * .5;
  const double signal =
      std::clamp(double(m.automatic) / std::max(1.f, m.span), -span, span);
  const double pivot = low > 0 ? low : range / 1023;
  const double logarithmic = d.octaves ? std::log1p(range / pivot) : 1;
  const auto unit = [&](double x) {
    return d.octaves ? std::log1p((x - low) / pivot) / logarithmic
                     : (x - low) / range;
  };
  const double center = std::clamp(unit(manual), radius, 1 - radius),
               position = std::clamp(center + signal * .5, 0.0, 1.0);
  return float(
      position <= 0
          ? low
          : position >= 1
                ? high
                : d.octaves ? low + pivot * std::expm1(position * logarithmic)
                            : low + position * range);
}
} // namespace s3g_thru_lfo
