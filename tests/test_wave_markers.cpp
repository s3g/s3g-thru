#include "thru_slice_model.h"
#include "thru_wave_markers.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <unistd.h>
using namespace s3g_thru_wave;
static std::vector<unsigned char> bytes(std::FILE *f) {
  assert(std::fflush(f) == 0);
  const auto size = ftello(f);
  assert(size >= 0);
  std::vector<unsigned char> b(std::size_t(size), 0);
  rewind(f);
  assert(std::fread(b.data(), 1, b.size(), f) == b.size());
  return b;
}
static void chunk(std::FILE *f, const char *id, const std::string &data) {
  raw(f, id, 4);
  put(f, std::uint32_t(data.size()));
  raw(f, data.data(), data.size());
  if (data.size() & 1u)
    raw(f, "!", 1);
}
static std::vector<std::pair<std::string, std::vector<unsigned char>>>
chunks(const std::vector<unsigned char> &b) {
  std::vector<std::pair<std::string, std::vector<unsigned char>>> c;
  for (std::size_t p = 12; p + 8 <= b.size();) {
    const auto n = u32(b.data() + p + 4);
    const auto total = 8 + std::size_t(n) + (n & 1u);
    assert(total <= b.size() - p);
    c.push_back({std::string(reinterpret_cast<const char *>(b.data() + p), 4),
                 {b.begin() + p, b.begin() + p + total}});
    p += total;
  }
  return c;
}
int main() {
  auto *input = tmpfile();
  assert(input);
  raw(input, "RIFF", 4);
  put(input, 0);
  raw(input, "WAVE", 4);
  raw(input, "fmt ", 4);
  put(input, 16);
  const unsigned char fmt[]{1,    0,    2, 0, 0x22, 0x56, 0,  0,
                            0xcc, 0x04, 2, 0, 6,    0,    24, 0};
  assert(std::fwrite(fmt, 1, 16, input) == 16);
  std::string audio(12000, '\0');
  for (std::size_t i = 0; i < audio.size(); ++i)
    audio[i] = char(i * 17);
  chunk(input, "data", audio);
  chunk(input, "JUNK", "odd");
  chunk(input, "bext", "metadata");
  chunk(input, "LIST", "INFOdetails");
  raw(input, "cue ", 4);
  put(input, 28);
  put(input, 1);
  put(input, 99);
  put(input, 100);
  raw(input, "data", 4);
  put(input, 0);
  put(input, 0);
  put(input, 100);
  chunk(input, "LIST", "adtloldlabels");
  const auto size = ftello(input);
  fseeko(input, 4, SEEK_SET);
  put(input, std::uint32_t(size - 8));
  fseeko(input, 0, SEEK_END);
  const auto original = bytes(input);
  auto *output = tmpfile();
  assert(output);
  rewind(input);
  write(input, output, {{0, "start"}, {.25, "kick"}, {.75, "tail"}}, 2000,
        44100);
  const auto marked = bytes(output);
  assert(u32(marked.data() + 4) + 8 == marked.size());
  const auto map =
      s3g_thru::readWaveSliceMarkers(marked.data(), marked.size(), 2, 2000);
  assert(map.count == 3 && map.points[0] == 0 && map.points[1] == .25f &&
         map.points[2] == .75f && map.points[3] == 1);
  assert(std::string(map.labels[1].data()) == "kick");
  const auto a = chunks(original), b = chunks(marked);
  for (const auto &c : a) {
    if (c.first == "cue " ||
        (c.first == "LIST" &&
         std::string(reinterpret_cast<const char *>(c.second.data() + 8), 4) ==
             "adtl"))
      continue;
    bool found = false;
    for (const auto &d : b)
      found |= c == d;
    assert(found);
  }
  fseeko(input, 0, SEEK_END);
  assert(bytes(input) == original);
  const auto reject = [&](std::vector<Marker> points) {
    rewind(input);
    auto *bad = tmpfile();
    assert(bad);
    bool failed = false;
    try {
      write(input, bad, points, 2000, 44100);
    } catch (const std::exception &) {
      failed = true;
    }
    std::fclose(bad);
    assert(failed);
  };
  reject({});
  reject({{.1, "missing zero"}});
  reject({{0, ""}, {0, "duplicate"}});
  reject({{0, ""}, {1, "end"}});
  reject({{0, ""}, {.00001, "same source frame"}});
  reject({{0, std::string(24, 'x')}});
  reject({{0, "\n"}});
  std::fclose(input);
  std::fclose(output);
  std::cout
      << "WAV cue writer: original-frame offsets, labels, "
         "resampling/truncation roundtrip, byte-identical audio/fmt/metadata, "
         "RIFF sizes/padding and invalid maps: PASS\n";
}
