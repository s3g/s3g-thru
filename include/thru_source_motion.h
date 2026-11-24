#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <new>
#include <string_view>
namespace s3g_thru_source {
constexpr int fields = 13, controlOffset = 800;
inline constexpr const char *keys[]{"travel_enabled",
                                    "travel_attack",
                                    "travel_attack_rate",
                                    "travel_decay",
                                    "travel_decay_rate",
                                    "travel_sustain_rate",
                                    "travel_release_rate",
                                    "travel_smooth",
                                    "live_source",
                                    "live_lookback",
                                    "live_window",
                                    "live_freeze",
                                    "live_gain"};
inline constexpr float low[]{0, 0, -4, 0, -4, -4, -4, 0, 0, 10, 50, 0, 0};
inline constexpr float high[]{1,   10000, 4,     10000, 4, 4, 4,
                              200, 5,     10000, 12000, 1, 2};
struct Config {
  std::array<float, fields> v{0, 100, 1, 500, 1, 0, 1, 20, 0, 250, 1000, 0, 1};
};
inline int index(const char *key) {
  for (int i = 0; i < fields; ++i)
    if (std::string_view(key) == keys[i])
      return i;
  return -1;
}
inline bool discrete(int f) { return f == 0 || f == 8 || f == 11; }
inline bool set(Config &c, int f, float x) {
  if (f < 0 || f >= fields || !std::isfinite(x))
    return false;
  c.v[f] = std::clamp(discrete(f) ? std::round(x) : x, low[f], high[f]);
  return true;
}
inline bool valid(const Config &c) {
  for (int f = 0; f < fields; ++f)
    if (!std::isfinite(c.v[f]) || c.v[f] < low[f] || c.v[f] > high[f] ||
        (discrete(f) && c.v[f] != std::floor(c.v[f])))
      return false;
  return true;
}
struct Travel {
  std::uint64_t age = 0;
  float rate = 1, pole = 1, lastSmooth = -1;
  bool started = false, released = false;
  void reset() { *this = {}; }
  void prepare(const Config &c, float sr) {
    if (c.v[7] != lastSmooth) {
      lastSmooth = c.v[7];
      pole = c.v[7] <= 0 ? 1 : 1 - std::exp(-1 / (sr * c.v[7] * .001f));
    }
  }
  float tick(const Config &c, float sr, bool off) {
    if (!c.v[0]) {
      if (started) {
        rate += (1 - rate) * pole;
        if (std::abs(rate - 1) < 1e-6f)
          rate = 1;
      }
      return rate;
    }
    if (!started) {
      started = true;
      rate = c.v[2];
      age = 0;
    }
    released |= off;
    const auto a = std::uint64_t(c.v[1] * sr * .001f),
               d = std::uint64_t(c.v[3] * sr * .001f);
    const float target =
        released ? c.v[6] : age < a ? c.v[2] : age - a < d ? c.v[4] : c.v[5];
    rate += (target - rate) * pole;
    ++age;
    return rate;
  }
};
struct View {
  const float *left = nullptr;
  const float *right = nullptr;
  std::size_t capacity = 0, mask = 0, frames = 0, safeFrames = 0;
  std::uint64_t written = 0;
  std::uint32_t generation = 0;
  double anchor = 0;
  bool advancing = true;
  explicit operator bool() const {
    return left && right && frames > 1 && capacity > 1;
  }
  float sample(int channel, double at) const {
    if (!*this || !std::isfinite(at))
      return 0;
    const double oldest = written > capacity ? double(written - capacity) : 0;
    if (at < oldest || at >= double(written) - 1)
      return 0;
    const auto n = std::uint64_t(at);
    const float t = float(at - double(n));
    const auto *data = channel ? right : left;
    const float a = data[std::size_t(n) & mask],
                b = data[std::size_t(n + 1) & mask];
    const float edge = float(
        std::min({1., (at - oldest) / 32., (double(written) - 1 - at) / 32.}));
    return (a + (b - a) * t) * std::max(0.f, edge);
  }
};
inline constexpr std::size_t envelopeBins = 120;
class Memory {
  std::unique_ptr<float[]> data_;
  std::size_t capacity_ = 0;
  std::uint64_t written_ = 0;
  std::uint32_t generation_ = 1;
  int source_ = 0;

public:
  bool prepare(float sr) {
    std::size_t n = 1;
    while (n * 2 <= std::size_t(sr * 12))
      n *= 2;
    data_.reset(new (std::nothrow) float[n * 2 + 8192]{});
    capacity_ = n;
    written_ = 0;
    return bool(data_);
  }
  void source(int s) {
    if (s != source_) {
      source_ = s;
      written_ = 0;
      ++generation_;
    }
  }
  int source() const { return source_; }
  std::uint64_t written() const { return written_; }
  void write(const float *left, const float *right, std::size_t n,
             bool freeze) {
    if (!data_ || freeze || !source_)
      return;
    const auto mask = capacity_ - 1;
    const auto stride = std::max<std::size_t>(1, capacity_ / 4096);
    float *lows = data_.get() + capacity_ * 2;
    float *highs = lows + 4096;
    for (std::size_t i = 0; i < n; ++i) {
      const auto at = std::size_t(written_++) & mask, bin = at / stride;
      if (at % stride == 0)
        lows[bin] = highs[bin] = 0;
      data_[at] = left[i];
      data_[capacity_ + at] = right[i];
      lows[bin] = std::min({lows[bin], left[i], right[i]});
      highs[bin] = std::max({highs[bin], left[i], right[i]});
    }
  }
  std::array<int, 64> waveform(const View &view) const {
    std::array<int, 64> result{};
    if (!data_ || !view)
      return result;
    const auto stride = std::max<std::size_t>(1, capacity_ / 4096);
    const float *lows = data_.get() + capacity_ * 2;
    const float *highs = lows + 4096;
    for (int bin = 0; bin < 32; ++bin) {
      const double at = view.anchor + (bin + .5) * view.frames / 32.;
      if (at < 0 || at >= double(written_))
        continue;
      const auto index = (std::size_t(at) & (capacity_ - 1)) / stride;
      result[bin * 2] =
          int(std::lround(std::clamp(lows[index], -1.f, 1.f) * 7));
      result[bin * 2 + 1] =
          int(std::lround(std::clamp(highs[index], -1.f, 1.f) * 7));
    }
    return result;
  }
  // Fixed work over the existing peak cache, with no PCM scan, allocation or
  // changes to the audio writer. Two signed 8-bit amplitudes per column are
  // hex-encoded directly, avoiding hundreds of additional snprintf calls.
  std::array<char, envelopeBins * 4 + 1> envelope(const View &view) const {
    std::array<char, envelopeBins * 4 + 1> result{};
    constexpr char hex[]="0123456789abcdef";
    const auto stride=std::max<std::size_t>(1,capacity_/4096);
    const auto oldest=written_>capacity_?written_-capacity_:0;
    const auto firstBlock=(oldest+stride-1)/stride;
    const auto lastBlock=written_?(written_-1)/stride:0;
    const float* lows=data_?data_.get()+capacity_*2:nullptr;
    const float* highs=lows?lows+4096:nullptr;
    const double step=double(view.frames)/envelopeBins,inverseStride=1./double(stride);
    for(std::size_t bin=0;bin<envelopeBins;++bin) {
      float low=0,high=0;
      const double at=view.anchor+(bin+.5)*step;
      if(data_&&view&&written_&&at>=double(oldest)&&at<double(written_)&&firstBlock<=lastBlock) {
        const double block=std::clamp(at*inverseStride-.5,double(firstBlock),double(lastBlock));
        const auto a=std::uint64_t(block),b=std::min(a+1,lastBlock);
        const float mix=float(block-double(a));
        const auto ia=std::size_t(a)%4096,ib=std::size_t(b)%4096;
        low=lows[ia]+(lows[ib]-lows[ia])*mix;
        high=highs[ia]+(highs[ib]-highs[ia])*mix;
      }
      const float levels[]{low,high};
      for(int channel=0;channel<2;++channel) {
        const float value=std::isfinite(levels[channel])?std::clamp(levels[channel],-1.f,1.f):0;
        const float scaled=value*127;
        const auto byte=unsigned(int(scaled+(scaled>=0?.5f:-.5f))+128);
        const auto offset=bin*4+std::size_t(channel)*2;
        result[offset]=hex[byte>>4];result[offset+1]=hex[byte&15];
      }
    }
    return result;
  }
  View view(const Config &c, float sr) const {
    if (!data_ || !source_)
      return {};
    const auto reserve = std::min(capacity_ / 2, std::size_t(sr * .25f) + 256);
    const auto window = std::clamp(std::size_t(c.v[10] * sr * .001f),
                                   std::size_t(8), capacity_ - reserve);
    const auto look = std::min(std::size_t(c.v[9] * sr * .001f),
                               capacity_ - window - reserve);
    return {data_.get(),
            data_.get() + capacity_,
            capacity_,
            capacity_ - 1,
            window,
            capacity_ - window - look - 256,
            written_,
            generation_,
            double(written_) - double(look) - double(window),
            c.v[11] == 0 || written_ < look + window};
  }
};
} // namespace s3g_thru_source
