#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace s3g_daisy {
// Immutable, source-rate pitch periods and closure marks. Built only by the
// analysis worker from its owned stereo snapshot; never from mutable live PCM.
struct ThruPitchBank {
    struct Point {
        float period = 0, confidence = 0;
    };
    std::vector<Point> points;
    std::vector<double> marks;
    float rate = 44100;
    std::size_t hop = 441;
    Point sample(double position) const {
        if (points.empty())
            return {rate / 150, 0};
        const double at = std::clamp(position / hop, 0.0, double(points.size() - 1));
        const auto i = static_cast<std::size_t>(at), j = std::min(i + 1, points.size() - 1);
        const float u = static_cast<float>(at - i);
        return {points[i].period + (points[j].period - points[i].period) * u,
                points[i].confidence + (points[j].confidence - points[i].confidence) * u};
    }
    double nearestMark(double position, double period) const {
        if (marks.empty())
            return position;
        const auto it = std::lower_bound(marks.begin(), marks.end(), position);
        double result = it == marks.end() ? marks.back() : *it;
        if (it != marks.begin() && std::abs(*(it - 1) - position) < std::abs(result - position))
            result = *(it - 1);
        return std::abs(result - position) <= period * 1.5 ? result : position;
    }
    void analyze(const float *left, const float *right, std::size_t count, float sampleRate) {
        points.clear();
        marks.clear();
        if (!left || !right || !count || !std::isfinite(sampleRate) || sampleRate < 1000)
            return;
        rate = sampleRate;
        hop = std::max<std::size_t>(1, std::lround(rate * .01f));
        const auto safe = [](float x) { return std::isfinite(x) ? std::clamp(x, -8.f, 8.f) : 0.f; };
        double el = 0, er = 0;
        for (std::size_t i = 0; i < count; ++i) {
            el += double(safe(left[i])) * safe(left[i]);
            er += double(safe(right[i])) * safe(right[i]);
        }
        const float *source = er > el ? right : left;
        const auto stride = std::max<std::size_t>(1, std::lround(rate / 8000));
        const float analysisRate = rate / stride;
        std::vector<float> mono((count + stride - 1) / stride);
        for (std::size_t i = 0; i < mono.size(); ++i) {
            float value = 0;
            const auto n = std::min(stride, count - i * stride);
            for (std::size_t j = 0; j < n; ++j)
                value += safe(source[i * stride + j]);
            mono[i] = value / n;
        }
        const auto minLag = std::max<std::size_t>(2, analysisRate / 1200),
                   maxLag = std::min<std::size_t>(254, std::ceil(analysisRate / 55));
        const auto window = maxLag * 2;
        points.resize((count + hop - 1) / hop + 1, {rate / 150, 0});
        std::array<float, 768> x{};
        std::array<float, 256> difference{};
        for (std::size_t f = 0; f < points.size(); ++f) {
            const auto start = static_cast<std::int64_t>(f * hop / stride) -
                               static_cast<std::int64_t>(maxLag * 3 / 2);
            double energy = 0;
            for (std::size_t j = 0; j < maxLag * 3 + 1; ++j) {
                const auto at = start + static_cast<std::int64_t>(j);
                x[j] = at >= 0 && static_cast<std::size_t>(at) < mono.size() ? mono[at] : 0;
                energy += double(x[j]) * x[j];
            }
            if (energy < window * 1e-8)
                continue;
            double cumulative = 0;
            for (std::size_t lag = 1; lag <= maxLag; ++lag) {
                double sum = 0;
                for (std::size_t j = 0; j < window; ++j) {
                    const double d = x[j] - x[j + lag];
                    sum += d * d;
                }
                cumulative += sum;
                difference[lag] =
                    cumulative > 1e-20 ? static_cast<float>(sum * lag / cumulative) : 1;
            }
            std::size_t best = minLag;
            for (std::size_t lag = minLag; lag < maxLag; ++lag) {
                if (difference[lag] < difference[best])
                    best = lag;
                if (difference[lag] < .18f) {
                    while (lag + 1 <= maxLag && difference[lag + 1] < difference[lag])
                        ++lag;
                    best = lag;
                    break;
                }
            }
            if (difference[best] >= .35f)
                continue;
            float fractional = static_cast<float>(best);
            if (best > minLag && best < maxLag) {
                const float a = difference[best - 1], b = difference[best],
                            c = difference[best + 1];
                const float denominator = a - 2 * b + c;
                if (denominator > 1e-8f)
                    fractional += std::clamp(.5f * (a - c) / denominator, -.5f, .5f);
            }
            points[f] = {fractional * stride, std::clamp(1 - difference[best], 0.f, 1.f)};
        }
        double cursor = 0;
        while (cursor + 2 < count) {
            const auto p = sample(cursor);
            if (p.confidence < .6f) {
                cursor += hop;
                continue;
            }
            const double radius = marks.empty() || cursor - marks.back() > p.period * 2
                                      ? p.period * .55
                                      : p.period * .23;
            const auto low = static_cast<std::size_t>(std::max(1.0, cursor - radius)),
                       high =
                           static_cast<std::size_t>(std::min(double(count - 2), cursor + radius));
            std::size_t peak = low;
            for (auto i = low; i <= high; ++i)
                if (safe(source[i]) > safe(source[peak]))
                    peak = i;
            const float a = safe(source[peak - 1]), b = safe(source[peak]),
                        c = safe(source[peak + 1]);
            const float denominator = a - 2 * b + c;
            const double mark =
                peak +
                (denominator < -1e-8f ? std::clamp(.5f * (a - c) / denominator, -.5f, .5f) : 0);
            if (marks.empty() || mark > marks.back() + p.period * .45)
                marks.push_back(mark);
            cursor = std::max(cursor + p.period * .5, mark + p.period);
        }
    }
};

} // namespace s3g_daisy
