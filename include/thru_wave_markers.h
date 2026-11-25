#pragma once
// RIFF/WAVE cue + LIST/adtl/labl editing. Audio and other chunks are copied
// byte for byte, on a file worker. Offsets are frames in the ORIGINAL WAV.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>
namespace s3g_thru_wave {
struct Marker {
  double position = 0;
  std::string label;
};
inline std::uint32_t u32(const unsigned char *p) {
  return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
         (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
inline void need(bool ok, const char *reason) {
  if (!ok)
    throw std::runtime_error(reason);
}
inline void put(std::FILE *f, std::uint32_t n) {
  const unsigned char p[]{
      static_cast<unsigned char>(n), static_cast<unsigned char>(n >> 8),
      static_cast<unsigned char>(n >> 16), static_cast<unsigned char>(n >> 24)};
  need(std::fwrite(p, 1, 4, f) == 4, "Cannot write WAV metadata");
}
inline void raw(std::FILE *f, const char *p, std::size_t n) {
  need(std::fwrite(p, 1, n, f) == n, "Cannot write marked WAV");
}
inline void copy(std::FILE *from, std::FILE *to, std::uint64_t start,
                 std::uint64_t n) {
  need(fseeko(from, off_t(start), SEEK_SET) == 0, "Cannot seek WAV");
  std::array<char, 65536> block{};
  while (n) {
    const auto count = std::size_t(std::min<std::uint64_t>(n, block.size()));
    need(std::fread(block.data(), 1, count, from) == count,
         "Truncated source WAV");
    raw(to, block.data(), count);
    n -= count;
  }
}
inline void write(std::FILE *input, std::FILE *output,
                  const std::vector<Marker> &markers, std::size_t loadedFrames,
                  double loadedRate) {
  need(input && output && loadedFrames && loadedRate > 0 &&
           std::isfinite(loadedRate) && !markers.empty() &&
           markers.size() <= 64,
       "Invalid marker map");
  struct stat before {};
  need(fstat(fileno(input), &before) == 0 && S_ISREG(before.st_mode) &&
           before.st_size >= 44,
       "Cannot read source WAV");
  unsigned char header[16]{};
  need(std::fread(header, 1, 12, input) == 12 &&
           !std::memcmp(header, "RIFF", 4) &&
           !std::memcmp(header + 8, "WAVE", 4),
       "Choose a RIFF WAV file");
  const std::uint64_t end = std::uint64_t(u32(header + 4)) + 8;
  need(end == std::uint64_t(before.st_size),
       "WAV length is inconsistent; original left intact");
  struct Chunk {
    std::uint64_t offset, length;
  };
  std::vector<Chunk> chunks;
  std::uint32_t rate = 0, align = 0;
  std::uint64_t audioBytes = 0;
  int dataChunks = 0;
  for (std::uint64_t at = 12; at < end;) {
    need(end - at >= 8 && fseeko(input, off_t(at), SEEK_SET) == 0 &&
             std::fread(header, 1, 8, input) == 8,
         "Invalid WAV chunk");
    const auto n = u32(header + 4);
    const auto total = std::uint64_t(n) + 8 + (n & 1u);
    need(total <= end - at, "Truncated WAV chunk");
    bool keep = std::memcmp(header, "cue ", 4) != 0 &&
                std::memcmp(header, "plst", 4) != 0;
    if (!std::memcmp(header, "fmt ", 4)) {
      need(n >= 16 && std::fread(header, 1, 16, input) == 16,
           "Invalid WAV format");
      rate = u32(header + 4);
      align = std::uint32_t(header[12]) | (std::uint32_t(header[13]) << 8);
    } else if (!std::memcmp(header, "data", 4)) {
      audioBytes = n;
      ++dataChunks;
    } else if (!std::memcmp(header, "LIST", 4) && n >= 4) {
      need(std::fread(header, 1, 4, input) == 4, "Invalid WAV list");
      if (!std::memcmp(header, "adtl", 4))
        keep = false;
    }
    if (keep)
      chunks.push_back({at, total});
    need(chunks.size() <= 100000, "Too many WAV chunks");
    at += total;
  }
  need(rate >= 8000 && rate <= 192000 && align > 0 && dataChunks == 1 &&
           audioBytes >= align && audioBytes % align == 0,
       "Unsupported source WAV");
  std::vector<std::uint32_t> frames;
  frames.reserve(markers.size());
  double prior = -1;
  for (const auto &m : markers) {
    need(std::isfinite(m.position) && m.position >= 0 && m.position < 1 &&
             m.position > prior && m.label.size() <= 23,
         "Invalid marker positions or labels");
    for (unsigned char c : m.label)
      need(c >= 32 && c < 127,
           "Marker labels use up to 23 plain text characters");
    const double value =
        std::round(m.position * double(loadedFrames) * rate / loadedRate);
    need(value >= 0 && value < audioBytes / align && value <= 4294967295.0,
         "Marker outside source audio");
    const auto frame = std::uint32_t(value);
    need(frames.empty() || frame > frames.back(),
         "Markers must be at least one source frame apart");
    frames.push_back(frame);
    prior = m.position;
  }
  need(frames.front() == 0, "The first marker must start at zero");
  raw(output, "RIFF", 4);
  put(output, 0);
  raw(output, "WAVE", 4);
  for (const auto &c : chunks)
    copy(input, output, c.offset, c.length);
  raw(output, "cue ", 4);
  put(output, std::uint32_t(4 + 24 * frames.size()));
  put(output, std::uint32_t(frames.size()));
  for (std::size_t i = 0; i < frames.size(); ++i) {
    put(output, std::uint32_t(i + 1));
    put(output, frames[i]);
    raw(output, "data", 4);
    put(output, 0);
    put(output, 0);
    put(output, frames[i]);
  }
  std::uint32_t labelBytes = 4;
  for (const auto &m : markers)
    if (!m.label.empty()) {
      const auto n = std::uint32_t(5 + m.label.size());
      labelBytes += 8 + n + (n & 1u);
    }
  if (labelBytes > 4) {
    raw(output, "LIST", 4);
    put(output, labelBytes);
    raw(output, "adtl", 4);
    for (std::size_t i = 0; i < markers.size(); ++i) {
      const auto &label = markers[i].label;
      if (label.empty())
        continue;
      const auto n = std::uint32_t(5 + label.size());
      raw(output, "labl", 4);
      put(output, n);
      put(output, std::uint32_t(i + 1));
      raw(output, label.c_str(), label.size() + 1);
      if (n & 1u)
        raw(output, "\0", 1);
    }
  }
  const auto size = ftello(output);
  need(size >= 12 && std::uint64_t(size) - 8 <= 4294967295ULL,
       "Marked WAV exceeds RIFF size limit");
  need(fseeko(output, 4, SEEK_SET) == 0, "Cannot finish WAV");
  put(output, std::uint32_t(size - 8));
  need(fseeko(output, 0, SEEK_END) == 0, "Cannot finish WAV");
  struct stat after {};
  need(fstat(fileno(input), &after) == 0 && after.st_size == before.st_size &&
           after.st_mtime == before.st_mtime && after.st_ino == before.st_ino,
       "Source changed while saving markers");
#if defined(__APPLE__)
  need(after.st_mtimespec.tv_nsec == before.st_mtimespec.tv_nsec,
       "Source changed while saving markers");
#else
  need(after.st_mtim.tv_nsec == before.st_mtim.tv_nsec,
       "Source changed while saving markers");
#endif
}
} // namespace s3g_thru_wave
