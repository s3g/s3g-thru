#pragma once
#include "thru_synth_recipe.h"
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
namespace s3g_thru_synth {
struct SourceAudio {
    std::vector<float> left, right;
    float rate = 44100;
};
inline std::uint16_t le16(const unsigned char *p) {
    return std::uint16_t(p[0]) | (std::uint16_t(p[1]) << 8);
}
inline std::uint32_t le32(const unsigned char *p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}
inline std::uint32_t sourceChecksum(const SourceAudio &a) {
    std::uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < a.left.size(); ++i)
        for (float v : {a.left[i], a.right[i]}) {
            std::uint32_t n;
            std::memcpy(&n, &v, 4);
            for (int j = 0; j < 4; ++j) {
                h ^= std::uint8_t(n >> (8 * j));
                h *= 16777619u;
            }
        }
    return h;
}
template <class Cancel>
std::shared_ptr<SourceAudio> loadSource(const char *path, bool embedded, float rate,
                                        Cancel cancel,const char* embeddedTag="sgSo") {
    if (!path || !path[0] || rate < 8000 || rate > 192000)
        return {};
    const int fd = ::open(path, O_RDONLY);
    if (fd < 0)
        return {};
    struct stat st {};
    if (::fstat(fd, &st) || st.st_size < 44) {
        ::close(fd);
        return {};
    }
    const auto size = std::size_t(st.st_size);
    void *map = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);
    if (map == MAP_FAILED)
        return {};
    struct Unmap {
        void *ptr;
        std::size_t size;
        ~Unmap() { ::munmap(ptr, size); }
    } guard{map, size};
    const auto *bytes = static_cast<const unsigned char *>(map);
    if (std::memcmp(bytes, "RIFF", 4) || std::memcmp(bytes + 8, "WAVE", 4))
        return {};
    const unsigned char *data = nullptr;
    std::size_t dataSize = 0;
    int format = 0, channels = 0, bits = 0, align = 0;
    std::uint32_t sourceRate = 0, expectedChecksum = 0;
    for (std::size_t at = 12; at + 8 <= size;) {
        const auto count = std::size_t(le32(bytes + at + 4)), next = at + 8 + count + (count & 1);
        if (next < at || next > size)
            return {};
        if (!embedded && !std::memcmp(bytes + at, "fmt ", 4) && count >= 16) {
            format = le16(bytes + at + 8);
            channels = le16(bytes + at + 10);
            sourceRate = le32(bytes + at + 12);
            align = le16(bytes + at + 20);
            bits = le16(bytes + at + 22);
            if (format == 65534 && count >= 40) {
                const auto *guid = bytes + at + 8 + 24;
                const unsigned char tail[] = {0, 0, 0x10, 0, 0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71};
                if (le32(guid) > 3 || std::memcmp(guid + 4, tail, 12))
                    return {};
                format = int(le32(guid));
            }
        }
        if (!embedded && !std::memcmp(bytes + at, "data", 4)) {
            data = bytes + at + 8;
            dataSize = count;
        }
        if (embedded && !std::memcmp(bytes + at, embeddedTag, 4)) {
            if (count < 16 || le32(bytes + at + 8) != 1)
                return {};
            sourceRate = le32(bytes + at + 12);
            const auto frames = std::size_t(le32(bytes + at + 16));
            expectedChecksum = le32(bytes + at + 20);
            if (frames > (192000u * 12u) || count != 16 + frames * 8)
                return {};
            data = bytes + at + 24;
            dataSize = frames * 8;
            format = 3;
            channels = 2;
            bits = 32;
            align = 8;
            break;
        }
        at = next;
    }
    if (!data || !dataSize || sourceRate < 8000 || sourceRate > 192000 || channels < 1 ||
        channels > 2 ||
        !((format == 1 && (bits == 16 || bits == 24 || bits == 32)) ||
          (format == 3 && bits == 32)) ||
        align < channels * (bits / 8) || align > 64)
        return {};
    const std::size_t sourceFrames =
                          std::min<std::size_t>(dataSize / align, std::size_t(sourceRate) * 12),
                      frames = std::min<std::size_t>(
                          std::size_t(rate * 12),
                          std::size_t(double(sourceFrames) * rate / sourceRate));
    if (!frames)
        return {};
    const auto sample = [&](std::size_t frame, int ch) {
        const auto *p = data + frame * align + (channels == 1 ? 0 : ch) * bits / 8;
        float v = 0;
        if (format == 3) {
            const auto n = le32(p);
            std::memcpy(&v, &n, 4);
        } else if (bits == 16)
            v = std::int16_t(le16(p)) / 32768.f;
        else if (bits == 24) {
            std::int32_t n = p[0] | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16);
            if (n & 0x800000)
                n |= ~0xffffff;
            v = n / 8388608.f;
        } else
            v = std::int32_t(le32(p)) / 2147483648.f;
        return std::isfinite(v) ? std::clamp(v, -16.f, 16.f) : 0.f;
    };
    if (embedded) {
        std::uint32_t h = 2166136261u;
        for (std::size_t i = 0; i < dataSize; ++i) {
            h ^= data[i];
            h *= 16777619u;
        }
        if (h != expectedChecksum)
            return {};
    }
    auto out = std::make_shared<SourceAudio>();
    out->rate = rate;
    out->left.resize(frames);
    out->right.resize(frames);
    const double ratio = double(sourceRate) / rate, cut = std::min(1.0, 1 / ratio) * .95;
    for (std::size_t i = 0; i < frames; ++i) {
        if ((i & 1023u) == 0 && cancel())
            return {};
        if (std::abs(ratio - 1) < 1e-9) {
            out->left[i] = sample(i, 0);
            out->right[i] = sample(i, 1);
            continue;
        }
        const double pos = i * ratio;
        const auto centre = std::int64_t(pos);
        double left = 0, right = 0, weight = 0;
        for (int tap = -15; tap <= 16; ++tap) {
            const auto at =
                std::clamp<std::int64_t>(centre + tap, 0, std::int64_t(sourceFrames - 1));
            const double d = pos - (centre + tap), x = 3.141592653589793 * cut * d;
            const double w = cut * (std::abs(x) < 1e-12 ? 1 : std::sin(x) / x) *
                             (.5 + .5 * std::cos(3.141592653589793 * d / 17));
            left += sample(std::size_t(at), 0) * w;
            right += sample(std::size_t(at), 1) * w;
            weight += w;
        }
        out->left[i] = float(left / weight);
        out->right[i] = float(right / weight);
    }
    return out;
}
} // namespace s3g_thru_synth
