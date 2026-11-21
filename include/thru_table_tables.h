#pragma once
#include "thru_spectral.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace s3g_thru_table {
struct WtBankData {
  static constexpr int size = 512, frames = 16, levels = 6;
  using Cycle = std::array<float, size + 1>;
  using Frame = std::array<Cycle, levels>;
  using Data = std::array<Frame, frames>;
  Data values{};
  template <class Cancel>
  static bool build(int bank, Data &values, Cancel cancel) {
    for (int f = 0; f < frames; ++f) {
      if (cancel())
        return false;
      const double morph = double(f) / (frames - 1);
      std::array<double, 32> amp{}, angle{};
      double normalization = 0;
      for (int h = 1; h <= 32; ++h) {
        double a = h == 1 ? 1 : 0, p = 0;
        const double aperture = std::exp(-double(h - 1) / (1.5 + morph * 26));
        switch (bank) {
        case 0:
          a = h == 1 ? 1 : std::pow(morph, 2) * aperture / (h * h);
          break;
        case 1:
          a = h == 1 ? 1 : aperture * (.22 + .6 * morph) / std::pow(h, 1.2);
          break;
        case 2:
          a = aperture / std::pow(h, .65 + (1 - morph) * .65);
          break;
        case 3:
          a = (h & 1) ? aperture / std::pow(h, .6 + 1.4 * (1 - morph))
                      : .12 * morph * aperture / h;
          p = (h % 4 == 3 ? 3.141592653589793 : 0);
          break;
        case 4:
          a = h == 1
                  ? .7
                  : aperture * (.15 + .85 * std::abs(std::sin(h * 2.39996))) /
                        std::sqrt(double(h));
          p = h * .61 * morph;
          break;
        case 5:
          a = h == 1
                  ? 1
                  : aperture * std::abs(std::sin(h * (1.2 + morph * 1.8))) / h;
          break;
        case 6:
          a = (h & 1) ? aperture / std::pow(h, .9) : .04 * aperture / h;
          break;
        case 7:
          a = h == 1
                  ? .8
                  : aperture *
                        std::pow(std::abs(std::cos(h * (1.618 + morph))), 3) /
                        std::sqrt(double(h));
          p = h * .33;
          break;
        }
        amp[h - 1] = a;
        angle[h - 1] = p;
        normalization += a;
      }
      // A single scale per frame is shared by all mip levels, avoiding
      // harmonic-limit changes that change the fundamental's gain.
      const double scale = 1 / std::max(1.0, normalization * .68);
      for (int i = 0; i < size; ++i) {
        double sum = 0;
        for (int h = 1; h <= 32; ++h) {
          sum += amp[h - 1] *
                 std::sin(6.283185307179586 * h * i / size + angle[h - 1]);
          for (int l = 0; l < levels; ++l)
            if (h == (32 >> l))
              values[f][l][i] = float(sum * scale);
        }
      }
      for (auto &cycle : values[f])
        cycle[size] = cycle[0];
    }
    return true;
  }
  static float readArray(const Data &values, double phase, float position,
                         float hz, float rate) {
    phase -= std::floor(phase);
    const float at = float(phase * size),
                pt = std::clamp(position, 0.f, 1.f) * (frames - 1);
    const int i = std::min(size - 1, int(at)),
              f = std::min(frames - 2, int(pt));
    const float t = at - i, u = pt - f;
    const float lod =
        std::clamp(std::log2(std::max(1.f, 32 * hz / (rate * .38f))), 0.f,
                   float(levels - 1));
    const int l = std::min(levels - 2, int(lod));
    const float v = lod - l;
    const auto sample = [&](int frame, int mip) {
      const auto &c = values[frame][mip];
      return c[i] + (c[i + 1] - c[i]) * t;
    };
    const float a = sample(f, l) + (sample(f + 1, l) - sample(f, l)) * u;
    const float b =
        sample(f, l + 1) + (sample(f + 1, l + 1) - sample(f, l + 1)) * u;
    return a + (b - a) * v;
  }
};
struct Decimator {
  // Fixed, well-conditioned sections use float throughout the float audio
  // path; this avoids repeated float/double conversions at twice sample rate.
  struct Section {
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setup(double normalized, double q) {
      const double w = 6.283185307179586 * normalized, c = std::cos(w),
                   s = std::sin(w), a = s / (2 * q), d = 1 + a;
      b0 = (1 - c) * .5 / d;
      b1 = (1 - c) / d;
      b2 = b0;
      a1 = -2 * c / d;
      a2 = (1 - a) / d;
    }
    float tick(float x) {
      const float y = b0 * x + z1;
      z1 = b1 * x - a1 * y + z2;
      z2 = b2 * x - a2 * y;
      return float(y);
    }
  };
  std::array<Section, 3> sections{};
  Decimator() {
    // Sixth-order low-pass at 0.4 of the output rate, for 2x rendering.
    sections[0] = {0.18007009801992388, 0.36014019603984776, 0.18007009801992388, -0.3221191839100801, 0.042399575989775534, 0, 0};
    sections[1] = {0.20657208382614795, 0.41314416765229589, 0.20657208382614795, -0.36952737735124136, 0.19581571265583311, 0, 0};
    sections[2] = {0.27724678090334365, 0.5544935618066873, 0.27724678090334365, -0.49595411891429358, 0.60494124252766812, 0, 0};
  }
  float tick(float x) {
    for (auto &s : sections)
      x = s.tick(x);
    return x;
  }
};

struct Library {
  static constexpr int size = 512, mips = 9, palettes = 16;
  using Cycle = std::array<float, size + 1>;
  using Mips = std::array<Cycle, mips>;
  std::array<Mips, 4> wave{};
  std::array<float, 1025> pitchRatios{};
  // Fractional-octave interpolation avoids libm in the per-sample drift path.
  // The full +/-96-semitone domain includes TABLE tuning plus drum bend.
  float pitchRatio(float semitones) const {
    constexpr float octaves[]{1.f/256,1.f/128,1.f/64,1.f/32,1.f/16,
      1.f/8,1.f/4,1.f/2,1,2,4,8,16,32,64,128};
    const float at=(std::clamp(semitones,-96.f,96.f)+96.f)*(1024.f/12.f);
    const int i=std::min(16383,int(at)), fraction=i&1023;
    return (pitchRatios[fraction]+(pitchRatios[fraction+1]-pitchRatios[fraction])*(at-i))*octaves[i>>10];
  }
  std::array<WtBankData, 8> drum{};
  std::array<std::array<std::array<Mips, 2>, palettes>, 8> spline{};
  static constexpr int pointCounts[]{1, 2, 4, 8, 12, 16, 24, 32};
  static std::uint32_t hash(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
  }
  static float random(std::uint32_t x) {
    return float(hash(x) >> 8) * (1.f / 16777216.f);
  }
  static void prepareMips(const Cycle &source, Mips &out) {
    using Plan = s3g_daisy::ThruSpectralPlan;
    std::array<Plan::Complex, size> spectrum{};
    const auto &plan = Plan::get();
    for (int i = 0; i < size; ++i)
      spectrum[i].real = source[i];
    plan.fft(spectrum);
    spectrum[0] = {};
    spectrum[size / 2] = {};
    for (int l = 0; l < mips; ++l) {
      auto work = spectrum;
      const int count = 256 >> l;
      for (int h = count + 1; h < size - count; ++h)
        work[h] = {};
      plan.fft(work, true);
      for (int i = 0; i < size; ++i)
        out[l][i] = work[i].real;
      out[l][size] = out[l][0];
    }
  }
  Library() {
    for(int i=0;i<=1024;++i) pitchRatios[i]=std::exp2(float(i)/1024);
    constexpr float tau = 6.283185307179586f;
    for (int shape = 0; shape < 4; ++shape) {
      Cycle raw{};
      for (int i = 0; i < size; ++i) {
        const float p = float(i) / size;
        raw[i] = shape == 0
                     ? std::sin(tau * p)
                     : shape == 1
                           ? 2.f / 3.14159265359f * std::asin(std::sin(tau * p))
                           : shape == 2 ? 2 * p - 1 : (p < .5f ? 1.f : -1.f);
      }
      prepareMips(raw, wave[shape]);
    }
    for (int bank = 0; bank < 8; ++bank)
      WtBankData::build(bank, drum[bank].values, [] { return false; });
    for (int count = 0; count < 8; ++count)
      for (int shape = 0; shape < palettes; ++shape) {
        const int half = pointCounts[count] + 1, total = 2 * half;
        std::array<float, 66> nodes{};
        for (int n = 1; n < total; ++n)
          if (n != half)
            nodes[n] = (n < half ? 1.f : -1.f) *
                       (.05f + .95f * random(0x53504c4eu ^ hash(shape + 1) ^
                                             hash(n + 1)));
        for (int curve = 0; curve < 2; ++curve) {
          Cycle raw{};
          float peak = 1e-6f;
          for (int i = 0; i < size; ++i) {
            const float x = float(i) * total / size;
            const int n = int(x);
            const float t = x - n;
            const float a = nodes[(n + total - 1) % total],
                        b = nodes[n % total], c = nodes[(n + 1) % total],
                        d = nodes[(n + 2) % total];
            raw[i] = curve ? b + .5f * t *
                                     (c - a +
                                      t * (2 * a - 5 * b + 4 * c - d +
                                           t * (3 * (b - c) + d - a)))
                           : b + (c - b) * t;
            peak = std::max(peak, std::abs(raw[i]));
          }
          for (auto &x : raw)
            x /= peak;
          prepareMips(raw, spline[count][shape][curve]);
        }
      }
  }
  static const Library &get() {
    static const Library library;
    return library;
  }
  struct Index {
    int i;
    float mix;
    explicit Index(double phase) {
      phase -= std::floor(phase);
      const float at = float(phase * size);
      i = std::min(size - 1, int(at));
      mix = at - i;
    }
  };
  static float sample(const Cycle &c, const Index &p) {
    return c[p.i] + (c[p.i + 1] - c[p.i]) * p.mix;
  }
  static float sample(const Cycle &c, double phase) {
    return sample(c, Index(phase));
  }
  static float read(const Mips &bank, const Index &p, float lod) {
    const int l = std::min(mips - 2, int(lod));
    const float mix = lod - l;
    const float a = sample(bank[l], p);
    if (mix == 0) return a;
    return a + (sample(bank[l + 1], p) - a) * mix;
  }
  static float read(const Mips &bank, double phase, float lod) {
    return read(bank, Index(phase), lod);
  }
  static float drumRead(const WtBankData::Data &bank, double phase,
                        float position, float lod) {
    const Index p(phase);
    const float pt = std::clamp(position, 0.f, 1.f) * 15;
    const int f = std::min(14, int(pt)), l = std::min(4, int(lod));
    const float t = pt - f, v = lod - l;
    const float a = sample(bank[f][l], p),
                b = sample(bank[f + 1][l], p);
    const float first = a + (b - a) * t;
    if (v == 0) return first;
    const float c = sample(bank[f][l + 1], p),
                d = sample(bank[f + 1][l + 1], p);
    return first + (c + (d - c) * t - first) * v;
  }

};
} // namespace s3g_thru_table
