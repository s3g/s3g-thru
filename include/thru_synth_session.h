#pragma once
#include "thru_background_thread.h"
#include "thru_storage.h"
#include "thru_synth_recipe.h"
#include "thru_thread_signal.h"
#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <sys/stat.h>

namespace s3g_thru_synth {
struct EditorState {
  int page = 0, row = 0, point = 0, spectrum = 0, component = 0;
  std::array<int, 3> groups{0, 5, 4};
  float amount = .25f, low = -12, high = 12;
  bool keepEnds = true, automaticName = true;
  std::array<char, 41> name{};
};
struct Draft {
  Recipe recipe;
  EditorState editor;
  bool used = false;
};
struct SessionSource {
  std::array<char, 768> path{};
  std::array<char, 65> label{};
  bool embedded = false;
};
struct Session {
  int type = 0;
  std::array<Draft, generatorCount> drafts{};
  std::array<SessionSource, 7>
      sources{}; // Resynth, two convolution, four hybrid.
};
inline bool validEditor(const EditorState &e) {
  if (e.page < 0 || e.page > 14 || e.row < 0 || e.row > 3 || e.point < 0 ||
      e.point > 7 || e.spectrum < 0 || e.spectrum > 3 || e.component < 0 ||
      e.component > 7)
    return false;
  for (int page : e.groups)
    if (page < 0 || page > 14)
      return false;
  if (!std::isfinite(e.amount) || !std::isfinite(e.low) ||
      !std::isfinite(e.high) || e.amount < 0 || e.amount > 1 || e.low < -32 ||
      e.high > 32 || e.low > e.high)
    return false;
  for (char c : e.name) {
    if (!c)
      break;
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_'))
      return false;
  }
  return e.name.back() == 0;
}
inline std::vector<std::uint8_t> encodeSession(const Session &s) {
  std::vector<std::uint8_t> b;
  b.reserve(32768);
  put(b, 0x53475453u);
  put(b, 1);
  put(b, std::uint32_t(s.type));
  unsigned count = 0;
  for (const auto &d : s.drafts)
    count += d.used;
  put(b, count);
  const auto string = [&](const auto &a) {
    const auto n = std::strlen(a.data());
    put(b, std::uint32_t(n));
    b.insert(b.end(), a.data(), a.data() + n);
  };
  for (int type = 0; type < generatorCount; ++type) {
    const auto &d = s.drafts[type];
    if (!d.used)
      continue;
    put(b, type);
    auto recipe = encode(d.recipe);
    put(b, std::uint32_t(recipe.size()));
    b.insert(b.end(), recipe.begin(), recipe.end());
    const auto &e = d.editor;
    for (int v : {e.page, e.row, e.point, e.spectrum, e.component, e.groups[0],
                  e.groups[1], e.groups[2]})
      put(b, std::uint32_t(v));
    for (float v : {e.amount, e.low, e.high})
      putFloat(b, v);
    put(b, e.keepEnds);
    put(b, e.automaticName);
    string(e.name);
  }
  for (const auto &source : s.sources) {
    put(b, source.embedded);
    string(source.path);
    string(source.label);
  }
  put(b, checksum(b));
  return b;
}
inline bool decodeSession(const std::vector<std::uint8_t> &b, Session &out) {
  if (b.size() < 20 || b.size() > 65536)
    return false;
  std::size_t at = 0;
  bool ok = true;
  const auto number = [&]() {
    std::uint32_t v = 0;
    if (at + 4 > b.size()) {
      ok = false;
      return v;
    }
    for (int j = 0; j < 4; ++j)
      v |= std::uint32_t(b[at++]) << (j * 8);
    return v;
  };
  if (number() != 0x53475453u || number() != 1)
    return false;
  Session next;
  next.type = int(number());
  const auto count = number();
  if (next.type < 0 || next.type >= generatorCount || !count ||
      count > generatorCount)
    return false;
  const auto scalar = [&]() {
    const auto n = number();
    float f;
    std::memcpy(&f, &n, 4);
    return f;
  };
  const auto string = [&](auto &a) {
    const auto n = number();
    if (n >= a.size() || n > b.size() - std::min(at, b.size())) {
      ok = false;
      return;
    }
    for (std::size_t j = 0; j < n; ++j) {
      const char c = char(b[at++]);
      if (!c)
        ok = false;
      a[j] = c;
    }
    a[n] = 0;
  };
  for (unsigned i = 0; i < count; ++i) {
    const auto type = number(), size = number();
    if (type >= generatorCount || next.drafts[type].used ||
        size > b.size() - std::min(at, b.size()))
      return false;
    auto &d = next.drafts[type];
    if (!decode(b.data() + at, size, d.recipe) || d.recipe.type != int(type))
      return false;
    at += size;
    d.used = true;
    auto &e = d.editor;
    e.page = int(number());
    e.row = int(number());
    e.point = int(number());
    e.spectrum = int(number());
    e.component = int(number());
    for (auto &g : e.groups)
      g = int(number());
    e.amount = scalar();
    e.low = scalar();
    e.high = scalar();
    const auto keep = number(), automatic = number();
    if (keep > 1 || automatic > 1)
      return false;
    e.keepEnds = keep;
    e.automaticName = automatic;
    string(e.name);
    if (!ok || !validEditor(e))
      return false;
  }
  for (auto &source : next.sources) {
    const auto embedded = number();
    if (embedded > 1)
      return false;
    source.embedded = embedded;
    string(source.path);
    string(source.label);
    if (!ok || (source.path[0] && source.path[0] != '/') ||
        (source.embedded && !source.path[0]))
      return false;
    for (char c : source.label) {
      if (!c)
        break;
      if (c < 32 || c > 126 || c == '"' || c == '\\')
        return false;
    }
  }
  if (!ok || !next.drafts[next.type].used || at + 4 != b.size())
    return false;
  std::vector<std::uint8_t> body(b.begin(), b.end() - 4);
  if (number() != checksum(body))
    return false;
  out = next;
  return true;
}
class SessionStore {
  struct Slot {
    Session session;
    std::atomic<int> state{0};
    std::atomic<std::uint64_t> sequence{0};
  };
  std::array<Slot, 3> slots;
  std::array<char, 1024> path{};
  pthread_t thread{};
  bool started = false;
  std::atomic<bool> running{false};
  std::uint64_t nextSequence = 0;
  s3g_daisy::ThruThreadSignal signal;
  static bool bytes(const char *path, std::vector<std::uint8_t> &out) {
    struct stat st {};
    if (::stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size < 20 ||
        st.st_size > 65536)
      return false;
    FILE *f = std::fopen(path, "rb");
    if (!f)
      return false;
    out.resize(std::size_t(st.st_size));
    const bool read = std::fread(out.data(), 1, out.size(), f) == out.size();
    const bool closed = std::fclose(f) == 0;
    return read && closed;
  }
  static bool atomicWrite(const std::string &path,
                          const std::vector<std::uint8_t> &data) {
    std::string name = path + ".tmp-XXXXXX";
    std::vector<char> temp(name.begin(), name.end());
    temp.push_back(0);
    const int fd = ::mkstemp(temp.data());
    if (fd < 0)
      return false;
    FILE *f = ::fdopen(fd, "wb");
    if (!f) {
      ::close(fd);
      ::unlink(temp.data());
      return false;
    }
    const bool wrote =
        std::fwrite(data.data(), 1, data.size(), f) == data.size();
    if (!s3g_thru_storage::closeFile(f, temp.data(), wrote)) {
      ::unlink(temp.data());
      return false;
    }
    if (!s3g_thru_storage::rename(temp.data(), path.c_str())) {
      ::unlink(temp.data());
      return false;
    }
    return s3g_thru_storage::syncParent(path.c_str());
  }
  bool write(const Session &s) {
    const auto next = encodeSession(s);
    Session checked;
    if (!decodeSession(next, checked))
      return false;
    std::vector<std::uint8_t> previous;
    if (bytes(path.data(), previous) && decodeSession(previous, checked) &&
        !atomicWrite(std::string(path.data()) + ".bak", previous))
      return false;
    return atomicWrite(path.data(), next);
  }
  static void *entry(void *opaque) {
    s3g_daisy::configureBackgroundThread();
    auto &self = *static_cast<SessionStore *>(opaque);
    for (;;) {
      int selected = -1;
      std::uint64_t newest = 0;
      for (int i = 0; i < 3; ++i)
        if (self.slots[i].state.load(std::memory_order_acquire) == 2) {
          const auto seq = self.slots[i].sequence.load();
          if (seq > newest) {
            newest = seq;
            selected = i;
          }
        }
      if (selected < 0) {
        if (!self.running.load())
          break;
        self.signal.wait(250);
        continue;
      }
      auto &slot = self.slots[selected];
      int expected = 2;
      if (!slot.state.compare_exchange_strong(expected, 3))
        continue;
      const auto sequence = slot.sequence.load();
      if (sequence > self.saved.load()) {
        bool success = false;
        try {
          success = self.write(slot.session);
        } catch (...) {
          success = false;
        }
        if (success)
          self.saved.store(sequence);
        else
          self.failed.store(sequence);
      }
      slot.state.store(0, std::memory_order_release);
    }
    return nullptr;
  }

public:
  std::atomic<std::uint64_t> saved{0}, failed{0};
  enum Load { missing, loaded, recovered, invalid };
  static Load load(const char *path, Session &out) {
    std::vector<std::uint8_t> b;
    if (bytes(path, b) && decodeSession(b, out))
      return loaded;
    if (bytes((std::string(path) + ".bak").c_str(), b) && decodeSession(b, out))
      return recovered;
    struct stat st {};
    return ::stat(path, &st) == 0 ? invalid : missing;
  }
  ~SessionStore() { stop(); }
  bool prepare(const char *file) {
    if (started)
      return true;
    if (!file || !file[0] || std::strlen(file) >= path.size() ||
        !signal.prepare())
      return false;
    std::strcpy(path.data(), file);
    running.store(true);
    if (!s3g_daisy::startBackgroundThread(thread, entry, this)) {
      running.store(false);
      return false;
    }
    started = true;
    return true;
  }
  bool post(const Session &s) {
    if (!started || !running.load())
      return false;
    for (int pass = 0; pass < 2; ++pass)
      for (auto &slot : slots) {
        int expected = pass ? 2 : 0;
        if (!slot.state.compare_exchange_strong(expected, 1))
          continue;
        slot.session = s;
        slot.sequence.store(++nextSequence);
        slot.state.store(2, std::memory_order_release);
        signal.notify();
        return true;
      }
    return false;
  }
  bool idle() const {
    for (const auto &slot : slots)
      if (slot.state.load() != 0)
        return false;
    return true;
  }
  void stop() {
    if (!started)
      return;
    running.store(false);
    signal.notify();
    pthread_join(thread, nullptr);
    started = false;
  }
};
} // namespace s3g_thru_synth
