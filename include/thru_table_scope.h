#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace s3g_thru_table {
struct Scope {
  static constexpr int capacity = 2048, bins = 64;
  std::array<std::array<float, capacity>, 4> ring{};
  std::uint64_t written = 0;
  template <class Bus> void append(const Bus &bus, int frames) {
    for (int i = 0; i < frames; ++i) {
      const auto index = std::size_t(written++) & (capacity - 1);
      for (int lane = 0; lane < 4; ++lane)
        ring[lane][index] = bus[lane][i];
    }
  }
  float at(int lane, std::int64_t n) const {
    return n < 0 || std::uint64_t(n) >= written ||
                   written - std::uint64_t(n) > capacity
               ? 0
               : ring[lane][std::size_t(n) & (capacity - 1)];
  }
  void encode(int lane, float rate, float hz, char *text) const {
    const int period =
                  std::clamp(int(rate / std::max(16.f, hz)), 8, capacity / 2),
              count = std::clamp(period * 2, 128, capacity - period);
    std::int64_t begin = std::int64_t(written) - count;
    // Stable recent rising crossing; no trigger on arbitrary file positions.
    for (int i = 0; i < period && begin - i > 0; ++i)
      if (at(lane, begin - i - 1) <= 0 && at(lane, begin - i) > 0) {
        begin -= i;
        break;
      }
    constexpr char hex[] = "0123456789abcde";
    for (int bin = 0; bin < bins; ++bin) {
      float low = 1e20f, high = -1e20f;
      for (int i = bin * count / bins; i < (bin + 1) * count / bins; ++i) {
        const float x = at(lane, begin + i);
        low = std::min(low, x);
        high = std::max(high, x);
      }
      // Fixed calibrated scope gain; quiet passages remain visibly quiet.
      text[bin * 2] = hex[std::clamp(int(std::lround(low * 12)) + 7, 0, 14)];
      text[bin * 2 + 1] =
          hex[std::clamp(int(std::lround(high * 12)) + 7, 0, 14)];
    }
    text[bins * 2] = 0;
  }
};
} // namespace s3g_thru_table
