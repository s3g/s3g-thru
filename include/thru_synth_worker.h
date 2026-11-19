#pragma once
#include "thru_background_thread.h"
#include "thru_spline.h"
#include "thru_storage.h"
#include "thru_thread_signal.h"
#include <atomic>
#include <cstdio>
#include <memory>
#include <new>
#include <pthread.h>
#include <string>
#include <sys/stat.h>
#include <thread>

namespace s3g_thru_synth {
class Worker {
public:
  enum { idle, requested, working, complete };
  enum Operation { generate, save, restore };
  struct Request {
    Recipe recipe;
    Operation op = generate;
    int slot = 0, lane = 0, clip = 0;
    bool load = false, embeddedSource = false, embeddedImpulse = false;
    std::uint64_t sourceEpoch = 0, impulseEpoch = 0;
    std::uint64_t revision = 0;
    float rate = 44100;
    std::array<char, 512> directory{};
    std::array<char, 768> source{}, impulse{};
    std::array<std::array<char, 768>, 4> hybridPaths{};
    std::array<std::uint64_t, 4> hybridEpochs{};
    std::array<bool, 4> hybridEmbedded{};
    std::array<char, 41> name{};
  } request;
  std::array<Audio, 3> audio;
  Recipe restored;
  std::array<char, 512> savedPath{};
  std::array<char, 96> error{};
  bool success = false;
  std::atomic<int> state{idle};
  std::atomic<unsigned> progress{0};
  std::atomic<bool> cancel{false};

private:
  pthread_t thread_{};
  bool started_ = false;
  std::atomic<bool> running_{false};
  s3g_daisy::ThruThreadSignal signal_;
  std::array<std::shared_ptr<SourceAudio>, 6> sourceCaches{};
  std::array<std::array<char, 768>, 6> sourceCachePaths{};
  std::array<std::uint64_t, 6> sourceCacheEpochs{};
  std::array<bool, 6> sourceCacheEmbedded{};
  std::shared_ptr<SourceAudio> inputSource(bool embedded,
                                           bool secondary = false) {
    const int i = secondary ? 1 : 0;
    const auto &path =
        secondary && request.op != restore ? request.impulse : request.source;
    const auto epoch = secondary && request.op != restore ? request.impulseEpoch
                                                          : request.sourceEpoch;
    auto &cache = sourceCaches[i];
    if (!cache || sourceCachePaths[i] != path ||
        sourceCacheEpochs[i] != epoch || sourceCacheEmbedded[i] != embedded ||
        cache->rate != request.rate) {
      auto next = loadSource(
          path.data(), embedded, request.rate,
          [&] {
            return cancel.load(std::memory_order_relaxed) ||
                   !running_.load(std::memory_order_relaxed);
          },
          secondary ? "sgSi" : "sgSo");
      if (!next)
        return {};
      cache = std::move(next);
      sourceCachePaths[i] = path;
      sourceCacheEpochs[i] = epoch;
      sourceCacheEmbedded[i] = embedded;
    }
    return cache;
  }
  std::shared_ptr<SourceAudio> hybridInput(int index, bool embedded) {
    const int cacheIndex = 2 + index;
    const auto &path =
        request.op == restore ? request.source : request.hybridPaths[index];
    const auto epoch = request.op == restore ? request.sourceEpoch
                                             : request.hybridEpochs[index];
    auto &cache = sourceCaches[cacheIndex];
    if (!cache || sourceCachePaths[cacheIndex] != path ||
        sourceCacheEpochs[cacheIndex] != epoch ||
        sourceCacheEmbedded[cacheIndex] != embedded ||
        cache->rate != request.rate) {
      char tagName[5] = {'s', 'g', 'H', char('0' + index), 0};
      auto next = loadSource(
          path.data(), embedded, request.rate,
          [&] {
            return cancel.load(std::memory_order_relaxed) ||
                   !running_.load(std::memory_order_relaxed);
          },
          tagName);
      if (!next)
        return {};
      cache = std::move(next);
      sourceCachePaths[cacheIndex] = path;
      sourceCacheEpochs[cacheIndex] = epoch;
      sourceCacheEmbedded[cacheIndex] = embedded;
    }
    return cache;
  }
  static bool directory(const char *text) {
    std::array<char, 512> path{};
    if (!text || std::strlen(text) >= path.size())
      return false;
    std::strcpy(path.data(), text);
    for (char *p = path.data() + 1; *p; ++p)
      if (*p == '/') {
        *p = 0;
        if (::mkdir(path.data(), 0755) != 0 && errno != EEXIST)
          return false;
        *p = '/';
      }
    if (::mkdir(path.data(), 0755) != 0 && errno != EEXIST)
      return false;
    struct stat status {};
    return ::stat(path.data(), &status) == 0 && S_ISDIR(status.st_mode);
  }
  static void tag(std::vector<std::uint8_t> &out, const char *text) {
    out.insert(out.end(), text, text + 4);
  }
  bool writeWav() {
    const auto &sound = audio[request.slot];
    const auto frames = sound.left.size();
    if (!frames || sound.right.size() != frames ||
        sound.revision != request.revision ||
        !directory(request.directory.data())) {
      std::snprintf(error.data(), error.size(), "Cannot create Synths folder");
      return false;
    }
    std::string name;
    for (const unsigned char *c =
             reinterpret_cast<const unsigned char *>(request.name.data());
         *c; ++c)
      if ((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') ||
          (*c >= '0' && *c <= '9') || *c == ' ' || *c == '-' || *c == '_')
        name += char(*c);
    while (!name.empty() && name.back() == ' ')
      name.pop_back();
    while (!name.empty() && name.front() == ' ')
      name.erase(name.begin());
    if (name.empty())
      name = generatorName(sound.recipe.type);
    std::array<char, 560> temp{};
    std::snprintf(temp.data(), temp.size(), "%s/.synth-XXXXXX",
                  request.directory.data());
    const int fd = ::mkstemp(temp.data());
    if (fd < 0)
      return false;
    auto *file = ::fdopen(fd, "wb");
    if (!file) {
      ::close(fd);
      std::remove(temp.data());
      return false;
    }
    struct TempGuard {
      std::FILE *file;
      const char *path;
      ~TempGuard() {
        if (file)
          std::fclose(file);
        std::remove(path);
      }
    } guard{file, temp.data()};
    // Audio controls come from the immutable render. Protection is editor-only
    // metadata and can change while that same render remains ready to save.
    Recipe snapshot = sound.recipe;
    snapshot.locks = request.recipe.locks;
    snapshot.detailLocks = request.recipe.detailLocks;
    snapshot.extendedLocks = request.recipe.extendedLocks;
    snapshot.workshopLocks = request.recipe.workshopLocks;
    const auto recipe = encode(snapshot);
    std::vector<std::uint8_t> header;
    tag(header, "RIFF");
    put(header, 0);
    tag(header, "WAVE");
    tag(header, "fmt ");
    put(header, 16); // FLOAT32 stereo.
    header.insert(header.end(), {3, 0, 2, 0});
    const auto rate = std::uint32_t(std::lround(request.rate));
    put(header, rate);
    put(header, rate * 8);
    header.insert(header.end(), {8, 0, 32, 0});
    tag(header, "fact");
    put(header, 4);
    put(header, std::uint32_t(frames));
    tag(header, "smpl");
    put(header, 36);
    for (int i = 0; i < 9; ++i)
      put(header, i == 2 ? std::uint32_t(1000000000.0 / rate)
                         : i == 3 ? std::uint32_t(sound.recipe.v[root]) : 0);
    tag(header, "sgSy");
    put(header, std::uint32_t(recipe.size()));
    header.insert(header.end(), recipe.begin(), recipe.end());
    tag(header, "data");
    put(header, std::uint32_t(frames * 8));
    std::size_t hybridBytes = 0;
    for (const auto &input : sound.inputs)
      if (input)
        hybridBytes += 24 + input->left.size() * 8;
    const auto sourceBytes =
        (sound.source ? 24 + sound.source->left.size() * 8 : 0) +
        (sound.impulse ? 24 + sound.impulse->left.size() * 8 : 0);
    const auto size = std::uint32_t(header.size() - 8 + frames * 8 +
                                    sourceBytes + hybridBytes);
    for (int i = 0; i < 4; ++i)
      header[4 + i] = std::uint8_t(size >> (i * 8));
    bool ok =
        std::fwrite(header.data(), 1, header.size(), file) == header.size();
    std::vector<std::uint8_t> data;
    data.reserve(8192);
    for (std::size_t first = 0; ok && first < frames; first += 1024) {
      data.clear();
      const auto end = std::min(frames, first + 1024);
      for (auto i = first; i < end; ++i) {
        putFloat(data, sound.left[i]);
        putFloat(data, sound.right[i]);
      }
      ok = std::fwrite(data.data(), 1, data.size(), file) == data.size();
      if (cancel.load(std::memory_order_relaxed)) {
        ok = false;
        break;
      }
    }
    for (int which = 0; ok && which < 6; ++which) {
      const auto &ptr = which < 2 ? (which ? sound.impulse : sound.source)
                                  : sound.inputs[which - 2];
      if (!ptr)
        continue;
      const auto &source = *ptr;
      std::vector<std::uint8_t> chunk;
      char hybridTag[5] = {'s', 'g', 'H', char('0' + which - 2), 0};
      tag(chunk, which < 2 ? (which ? "sgSi" : "sgSo") : hybridTag);
      put(chunk, std::uint32_t(16 + source.left.size() * 8));
      put(chunk, 1);
      put(chunk, std::uint32_t(std::lround(source.rate)));
      put(chunk, std::uint32_t(source.left.size()));
      put(chunk, sourceChecksum(source));
      ok = std::fwrite(chunk.data(), 1, chunk.size(), file) == chunk.size();
      for (std::size_t first = 0; ok && first < source.left.size();
           first += 1024) {
        data.clear();
        const auto end = std::min(source.left.size(), first + 1024);
        for (auto i = first; i < end; ++i) {
          putFloat(data, source.left[i]);
          putFloat(data, source.right[i]);
        }
        ok = std::fwrite(data.data(), 1, data.size(), file) == data.size() &&
             !cancel.load(std::memory_order_relaxed);
      }
    }
    ok = s3g_thru_storage::closeFile(file, temp.data(), ok) && ok;
    guard.file = nullptr;
    ok = ok && !cancel.load(std::memory_order_relaxed);
    bool published = false;
    for (int suffix = 0; ok && suffix < 10000; ++suffix) {
      const int n =
          suffix
              ? std::snprintf(savedPath.data(), savedPath.size(),
                              "%s/%s-%03d.wav", request.directory.data(),
                              name.c_str(), suffix)
              : std::snprintf(savedPath.data(), savedPath.size(), "%s/%s.wav",
                              request.directory.data(), name.c_str());
      if (n < 0 || n >= int(savedPath.size())) {
        ok = false;
        break;
      }
      if (s3g_thru_storage::link(temp.data(), savedPath.data())) {
        published = true;
        break;
      }
      if (errno != EEXIST)
        break;
    }
    if (published && !s3g_thru_storage::syncParent(savedPath.data())) {
      std::remove(savedPath.data());
      s3g_thru_storage::syncParent(savedPath.data());
      published = false;
    }
    std::remove(temp.data());
    return ok && published;
  }
  bool readRecipe() {
    auto *file = std::fopen(request.source.data(), "rb");
    if (!file)
      return false;
    unsigned char prefix[12];
    bool ok = std::fread(prefix, 1, 12, file) == 12 &&
              !std::memcmp(prefix, "RIFF", 4) &&
              !std::memcmp(prefix + 8, "WAVE", 4);
    struct stat status {};
    ok = ok && ::fstat(fileno(file), &status) == 0 && status.st_size >= 44;
    bool found = false;
    while (ok && !found) {
      unsigned char chunk[8];
      if (std::fread(chunk, 1, 8, file) != 8)
        break;
      const std::uint32_t size =
          std::uint32_t(chunk[4]) | (std::uint32_t(chunk[5]) << 8) |
          (std::uint32_t(chunk[6]) << 16) | (std::uint32_t(chunk[7]) << 24);
      const auto pos = std::ftell(file);
      if (pos < 0 || std::uint64_t(pos) + size + (size & 1u) >
                         std::uint64_t(status.st_size)) {
        ok = false;
        break;
      }
      if (!std::memcmp(chunk, "sgSy", 4)) {
        if (size != recipeBytes && size != previousRecipeBytes &&
            size != thirdRecipeBytes && size != fourthRecipeBytes &&
            size != fifthRecipeBytes && size != sixthRecipeBytes &&
            size != seventhRecipeBytes && size != eighthRecipeBytes &&
            size != ninthRecipeBytes && size != legacyRecipeBytes) {
          ok = false;
          break;
        }
        std::vector<std::uint8_t> bytes(size);
        found = std::fread(bytes.data(), 1, size, file) == size &&
                decode(bytes.data(), bytes.size(), restored);
        break;
      }
      if (std::fseek(file, long(size + (size & 1u)), SEEK_CUR) != 0) {
        ok = false;
        break;
      }
    }
    std::fclose(file);
    if (ok && found &&
        (restored.type == resynth || restored.type == convolution) &&
        (!inputSource(true) ||
         (restored.type == convolution && !inputSource(true, true)))) {
      std::snprintf(error.data(), error.size(),
                    "Embedded source missing or damaged");
      return false;
    }
    if (ok && found && restored.type == hybrid)
      for (int i = 0; i < int(restored.hybridParams[hSources]); ++i)
        if (!hybridInput(i, true)) {
          std::snprintf(error.data(), error.size(),
                        "Embedded Hybrid source damaged");
          return false;
        }
    return ok && found;
  }
  static void *entry(void *opaque) {
    s3g_daisy::configureBackgroundThread();
    auto &self = *static_cast<Worker *>(opaque);
    while (self.running_.load(std::memory_order_acquire)) {
      if (self.state.load(std::memory_order_acquire) != requested) {
        self.signal_.wait(250);
        continue;
      }
      self.state.store(working, std::memory_order_release);
      self.error[0] = 0;
      self.savedPath[0] = 0;
      self.success = false;
      try {
        if (self.request.op == generate) {
          auto &out = self.audio[self.request.slot];
          out.source.reset();
          out.impulse.reset();
          for (auto &input : out.inputs)
            input.reset();
          HybridInputs hybridSources{};
          bool hybridReady = true;
          if (self.request.recipe.type == hybrid)
            for (int i = 0; i < int(self.request.recipe.hybridParams[hSources]);
                 ++i) {
              hybridSources[i] =
                  self.hybridInput(i, self.request.hybridEmbedded[i]);
              if (!hybridSources[i]) {
                hybridReady = false;
                std::snprintf(self.error.data(), self.error.size(),
                              "Cannot read Hybrid source %c", char('A' + i));
                break;
              }
            }
          const bool needsSource = self.request.recipe.type == resynth ||
                                   self.request.recipe.type == convolution,
                     needsImpulse = self.request.recipe.type == convolution;
          const auto source =
                         needsSource
                             ? self.inputSource(self.request.embeddedSource)
                             : std::shared_ptr<SourceAudio>{},
                     impulse = needsImpulse
                                   ? self.inputSource(
                                         self.request.embeddedImpulse, true)
                                   : std::shared_ptr<SourceAudio>{};
          if ((needsSource && !source) || (needsImpulse && !impulse))
            std::snprintf(self.error.data(), self.error.size(),
                          "Cannot read selected source file");
          self.success =
              hybridReady && (!needsSource || bool(source)) &&
              (!needsImpulse || bool(impulse)) &&
              render(
                  self.request.recipe, self.request.rate, out,
                  [&] {
                    return !self.running_.load(std::memory_order_relaxed) ||
                           self.cancel.load(std::memory_order_relaxed);
                  },
                  [&](unsigned value) {
                    self.progress.store(value, std::memory_order_relaxed);
                    std::this_thread::yield();
                  },
                  source.get(), impulse.get(), &hybridSources);
          if (self.success) {
            out.source = source;
            out.impulse = impulse;
            out.inputs = hybridSources;
          }
          if (self.success)
            out.revision = self.request.revision;
        } else if (self.request.op == save)
          self.success = self.writeWav();
        else
          self.success = self.readRecipe();
      } catch (const std::bad_alloc &) {
        std::snprintf(self.error.data(), self.error.size(),
                      "Not enough synthesis memory");
        self.success = false;
      }
      if (!self.success && !self.error[0])
        std::snprintf(self.error.data(), self.error.size(), "%s",
                      self.cancel.load() ? "Cancelled"
                                         : self.request.op == restore
                                               ? "No Synth recipe in source"
                                               : "Synthesis job failed");
      self.state.store(complete, std::memory_order_release);
    }
    return nullptr;
  }

public:
  ~Worker() { stop(); }
  bool prepare() {
    if (started_)
      return true;
    if (!signal_.prepare())
      return false;
    running_.store(true, std::memory_order_release);
    if (!s3g_daisy::startBackgroundThread(thread_, entry, this)) {
      running_.store(false);
      return false;
    }
    started_ = true;
    return true;
  }
  void stop() {
    if (!started_)
      return;
    running_.store(false, std::memory_order_release);
    cancel.store(true);
    signal_.notify();
    pthread_join(thread_, nullptr);
    started_ = false;
  }
  bool post(const Request &next) {
    if (!started_ || state.load(std::memory_order_acquire) != idle)
      return false;
    request = next;
    cancel.store(false, std::memory_order_release);
    progress.store(0);
    state.store(requested, std::memory_order_release);
    signal_.notify();
    return true;
  }
};
} // namespace s3g_thru_synth
