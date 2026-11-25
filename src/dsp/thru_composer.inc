// Browser I/O is owned by the ordinary background worker. Only fixed commands,
// small atomic telemetry and bounded clip copies cross the realtime boundary.
#include "thru_composer_codec.inc"
enum ComposerPhase {
  composerIdle,
  composerRequest,
  composerCapture,
  composerCaptured,
  composerInstall,
  composerInstalling,
  composerLoading,
  composerFinished,
  composerActive,
  composerFailure,
  composerSampleLoading
};
struct ComposerTransaction {
  std::unique_ptr<PersistenceSnapshot> original;
  std::vector<ClipSlot> candidates;
  std::vector<int> destinations, changed;
  std::array<std::array<char, kPersistencePathSize>, 4> originalPlaybackPaths{};
  SavedChainBanks chains{};
  std::uint64_t revision = 0;
  std::size_t offset = 0;
  int operation = 0, selected = 0, target = 0;
  bool replacing = false, includeChains = false, captured = false,
       playing = false, restore = false, committed = false, started = false;
  ComposerJson document;
  std::string reply, output, message;
};
std::atomic<int> composerPhase{composerIdle};
bool composerSkipFallbackWrites = false;
std::atomic<bool> composerIoFailed{false};
std::atomic<bool> composerOwned{false}, composerUndoReady{false},
    composerCancel{false};
std::atomic<std::uint64_t> composerLease{0}, composerLiveRevision{1};
std::atomic<int> composerLiveActive{0}, composerLivePlaying{0},
    composerProgress{0};
std::array<char, 224> composerPacket{};
std::array<char, 65> composerOwner{};
std::unique_ptr<ComposerTransaction> composerWork;
std::unique_ptr<PersistenceSnapshot> composerReadSnapshot;
std::string composerRoot, composerEpoch, composerClient, composerRequestId,
    composerOperation, composerFilename, composerResult;
std::array<std::string, 16> composerCompleted{};
int composerCompletedAt = 0;
std::uint64_t composerStatusAt = 0, composerResultRevision = 0;
int composerReadSelection = -1;
std::atomic<int> composerCopyTotal{0};
static std::uint64_t composerClock() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return std::uint64_t(t.tv_sec) * 1000 + std::uint64_t(t.tv_nsec) / 1000000;
}
#include "thru_composer_panel.inc"
#include "thru_composer_workspace.inc"
#include "thru_composer_clock.inc"
bool composerBusy() const {
  return composerOwned.load(std::memory_order_acquire) ||
         (composerPhase.load(std::memory_order_acquire) != composerIdle &&
          composerPhase.load(std::memory_order_acquire) != composerActive);
}
void composerInitWorker() {
  if (!composerRoot.empty())
    return;
  composerRoot = std::strncmp(moduleRootDirectory.data(), "/data/", 6)
                     ? std::string(moduleRootDirectory.data()) + "/composer"
                     : std::string(kSampleLibraryRoot) + "s3g Thru/Composer";
  if (!std::strncmp(moduleRootDirectory.data(), "/data/", 6))
    s3g_thru_pack::need(
        projectMakeDirectory(
            (std::string(kSampleLibraryRoot) + "s3g Thru").c_str()),
        "composer", "cannot create sample folder");
  for (const auto &d : std::array<std::string, 5>{
           composerRoot, composerRoot + "/uploads", composerRoot + "/cache",
           composerRoot + "/replies", composerRoot + "/exports"})
    s3g_thru_pack::need(projectMakeDirectory(d.c_str()), "composer",
                        "cannot create transfer folder");
  composerEpoch =
      std::to_string(::getpid()) + "-" + std::to_string(composerClock());
}
static bool composerToken(const char *text) {
  const auto n = std::strlen(text);
  if (!n || n > 64 || !std::strcmp(text, "__proto__") ||
      !std::strcmp(text, "constructor") || !std::strcmp(text, "prototype"))
    return false;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text);
       *p; ++p)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
          (*p >= '0' && *p <= '9') || *p == '-' || *p == '_'))
      return false;
  return true;
}
bool setComposerParam(const char *key, const char *value) {
  if (!std::strcmp(key, "composer_cancel")) {
    if (isTruthy(value))
      composerCancel.store(true, std::memory_order_release);
    return true;
  }
  if (std::strcmp(key, "composer_command"))
    return false;
  const std::size_t size = std::strlen(value);
  if (size >= composerPacket.size())
    return true;
  // A heartbeat never queues worker work and cannot write any sound setting.
  char client[65]{}, request[65]{}, op[20]{};
  if (std::sscanf(value, "%64[^|]|%64[^|]|%19[^|]", client, request, op) != 3)
    return true;
  if (!composerToken(client) || !composerToken(request))
    return true;
  if (!std::strcmp(op,"hello")) {
    if (std::strcmp(composerPanelOwner.data(),client)) {composerReleaseKeyboard();composerPanelOwnerGeneration.fetch_add(1);for(auto& receipt:composerEditReceipts)receipt.store(0,std::memory_order_release);composerEditVersion.store(0);}
    std::snprintf(composerPanelOwner.data(),composerPanelOwner.size(),"%s",client);
    composerPanelWatching.store(true,std::memory_order_release);
  }
  if (composerOwned.load(std::memory_order_acquire) &&
      std::strcmp(composerOwner.data(), client))
    return true;
  if (!std::strcmp(op, "heartbeat")) {
    composerLease.store(composerClock() + 30000, std::memory_order_release);
    return true;
  }
  const int phase = composerPhase.load(std::memory_order_acquire);
  if (phase != composerIdle && phase != composerActive)
    return true;
  std::memcpy(composerPacket.data(), value, size + 1);
  composerLiveRevision.store(persistentRevision, std::memory_order_release);
  composerLiveActive.store(activeClip);
  composerPanelLiveLane.store(int(selectedLane));composerPanelLiveFocus.store(splitEnabled?splitFocus:0);
  composerLivePlaying.store(clipPlaying);
  composerLease.store(composerClock() + 30000, std::memory_order_release);
  composerPhase.store(composerRequest, std::memory_order_release);
  return true;
}
static std::string composerHashFile(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  s3g_thru_pack::need(bool(f), path, "cannot open sample");
  s3g_thru_pack::Sha256 hash;
  std::array<char, 65536> block{};
  while (f) {
    f.read(block.data(), block.size());
    hash.update(block.data(), std::size_t(f.gcount()));
  }
  s3g_thru_pack::need(f.eof(), path, "sample read failed");
  return hash.finish();
}
static void composerCopyFile(const std::string &source,
                             const std::string &target, const std::string &hash,
                             bool durable) {
  struct stat st {};
  if (::lstat(target.c_str(), &st) == 0) {
    s3g_thru_pack::need(S_ISREG(st.st_mode) && composerHashFile(target) == hash,
                        target, "existing asset hash mismatch");
    return;
  }
  const auto temporary = target + ".tmp";
  std::ifstream in(source, std::ios::binary);
  auto *out = std::fopen(temporary.c_str(), "wb");
  bool ok = bool(in) && out;
  std::array<char, 65536> b{};
  s3g_thru_pack::Sha256 digest;
  while (ok && in) {
    in.read(b.data(), b.size());
    const auto n = std::size_t(in.gcount());
    digest.update(b.data(), n);
    ok = std::fwrite(b.data(), 1, n, out) == n;
  }
  ok = ok && in.eof() && digest.finish() == hash;
  if (out) {
    if (durable)
      ok = s3g_thru_storage::closeFile(out, temporary.c_str(), ok) && ok;
    else
      ok = (std::fclose(out) == 0) && ok;
  }
  if (ok)
    ok = s3g_thru_storage::link(temporary.c_str(), target.c_str());
  if (ok && durable)
    ok = s3g_thru_storage::syncParent(target.c_str());
  std::remove(temporary.c_str());
  s3g_thru_pack::need(ok, target, "asset copy failed");
}
static ComposerJson composerWavMetadata(const std::string &path) {
  using namespace s3g_thru_pack;
  need(projectWavValid(path), path, "invalid WAV audio");
  const int fd = ::open(path.c_str(), O_RDONLY);
  need(fd >= 0, path, "cannot open WAV");
  struct stat st {};
  if (::fstat(fd, &st) != 0 || st.st_size < 44) {
    ::close(fd);
    throw std::runtime_error("Invalid WAV");
  }
  const auto size = std::size_t(st.st_size);
  auto *data = static_cast<const std::uint8_t *>(
      ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
  ::close(fd);
  need(data != MAP_FAILED, path, "cannot map WAV");
  struct Unmap {
    const std::uint8_t *p;
    std::size_t n;
    ~Unmap() { ::munmap(const_cast<std::uint8_t *>(p), n); }
  } guard{data, size};
  const auto u16 = [&](std::size_t p) {
    return int(data[p]) | (int(data[p + 1]) << 8);
  };
  const auto u32 = [&](std::size_t p) {
    return std::uint32_t(u16(p)) | (std::uint32_t(u16(p + 2)) << 16);
  };
  int format = 0, channels = 0, bits = 0, align = 0;
  std::uint32_t rate = 0;
  std::size_t start = 0, bytes = 0;
  for (std::size_t p = 12; p + 8 <= size;) {
    const auto n = u32(p + 4);
    need(n <= size - p - 8, path, "truncated WAV");
    if (!std::memcmp(data + p, "fmt ", 4)) {
      need(n >= 16, path, "invalid WAV format");
      format = u16(p + 8);
      channels = u16(p + 10);
      rate = u32(p + 12);
      align = u16(p + 20);
      bits = u16(p + 22);
    } else if (!std::memcmp(data + p, "data", 4)) {
      start = p + 8;
      bytes = n;
    }
    p += 8 + n + (n & 1);
  }
  need(((format == 1 && (bits == 16 || bits == 24 || bits == 32)) ||
        (format == 3 && bits == 32)) &&
           channels >= 1 && channels <= 2 && align == channels * bits / 8 &&
           bytes >= std::size_t(align) && bytes % align == 0 && rate >= 8000 &&
           rate <= 384000,
       path, "unsupported WAV encoding");
  const auto frames = bytes / align;
  auto wave = ComposerJson::array();
  for (int bin = 0; bin < 128; ++bin) {
    float low = 0, high = 0;
    const auto first = frames * bin / 128,
               last = std::min(frames,
                               std::max(first + 1, frames * (bin + 1) / 128));
    for (auto f = first; f < last; ++f) {
      float x = 0;
      for (int c = 0; c < channels; ++c)
        x +=
            decodeWavSample(data + start, f, c, channels, format, bits, align) /
            channels;
      need(std::isfinite(x), path, "non-finite WAV audio");
      low = std::min(low, x);
      high = std::max(high, x);
    }
    wave.push(std::max(-1.f, low));
    wave.push(std::min(1.f, high));
  }
  return composerObject({{"frames", double(frames)},
                         {"sampleRate", double(rate)},
                         {"waveform", std::move(wave)}});
}
ComposerJson composerAsset(ComposerJson &doc, const std::string &original) {
  using namespace s3g_thru_pack;
  char real[PATH_MAX]{};
  need(::realpath(original.c_str(), real), original, "sample missing");
  const std::string source = real;
  char root[PATH_MAX]{};
  const char *r = moduleRootDirectory[0] ? moduleRootDirectory.data()
                                         : moduleDirectory.data();
  const bool inRoot = ::realpath(r, root) && namingBelow(source, root);
  need(inRoot || source.rfind(kSampleLibraryRoot, 0) == 0, source,
       "sample is outside the library");
  const auto hash = composerHashFile(source),
             target = composerRoot + "/cache/" + hash + ".wav";
  composerCopyFile(source, target, hash, false);
  for (const auto &a : doc.at("assets").arrayValue())
    if (a.at("id").string() == hash)
      return a;
  auto a = composerWavMetadata(target);
  struct stat st {};
  need(::stat(target.c_str(), &st) == 0, "sample", "cannot stat");
  a["id"] = hash;
  a["sha256"] = hash;
  a["size"] = double(st.st_size);
  const auto slash = source.find_last_of('/');
  a["name"] = source.substr(slash == std::string::npos ? 0 : slash + 1);
  a["file"] = "assets/" + hash + ".wav";
  a["url"] = "/files/download?path=" + target;
  doc["assets"].push(a);
  return a;
}
void composerPrepareOriginalPlayback() {
  auto &work = *composerWork;
  const auto &original = work.original->clips[work.original->active];
  work.originalPlaybackPaths = original.laneSourcePaths;
  for (int lane = 0; lane < 4; ++lane) {
    if (original.laneSourceKinds[lane] == LaneSourceKind::empty ||
        original.laneSourcePaths[lane][0])
      continue;
    const auto source = std::string(work.original->directory.data()) +
                        "/thru-lane-" + std::to_string(lane + 1) + ".wav";
    s3g_thru_pack::need(
        projectWavValid(source), "original sample",
        "save or restore the current lane audio before Preview");
    const bool host =
        std::strncmp(moduleRootDirectory.data(), "/data/", 6) != 0;
    const auto directory =
        host ? std::string(moduleRootDirectory.data()) + "/library/composer"
             : composerRoot + "/cache";
    if (host)
      s3g_thru_pack::need(
          projectMakeDirectory(
              (std::string(moduleRootDirectory.data()) + "/library").c_str()),
          "original sample", "cannot create library");
    s3g_thru_pack::need(projectMakeDirectory(directory.c_str()),
                        "original sample", "cannot create cache");
    const auto hash = composerHashFile(source),
               destination = directory + "/p" + hash + ".wav";
    composerCopyFile(source, destination, hash, false);
    std::snprintf(work.originalPlaybackPaths[lane].data(), kPersistencePathSize,
                  "%s", destination.c_str());
  }
}
void composerResolvePaths(ClipSlot &clip,
                          const std::map<std::string, std::string> &assets) {
  const auto resolve = [&](auto &p) {
    if (!p[0])
      return;
    const std::string path = p.data();
    s3g_thru_pack::need(path.rfind("asset:", 0) == 0 &&
                            assets.count(path.substr(6)),
                        "source", "undeclared or missing sample");
    const auto &resolved = assets.at(path.substr(6));
    s3g_thru_pack::need(resolved.size() < p.size(), "source",
                        "path is too long");
    std::snprintf(p.data(), p.size(), "%s", resolved.c_str());
  };
  for (auto &path : clip.laneSourcePaths)
    resolve(path);
  for (auto &slot : clip.bankSlots)
    resolve(slot.path);
}
void composerEncode(PersistenceSnapshot &snapshot, int selection) {
  using namespace s3g_thru_pack;
  auto doc = composerObject({{"format", "s3g-thrupack"},
                             {"version", 1},
                             {"title", snapshot.state.projectTitle[0]
                                           ? snapshot.state.projectTitle.data()
                                           : "Thru Composition"},
                             {"tempo", snapshot.tempo},
                             {"assets", ComposerJson::array()},
                             {"clips", ComposerJson::array()},
                             {"chains", ComposerJson::array()}});
  auto records = ComposerJson::object();
  std::vector<std::pair<std::string, std::string>> files;
  auto capsule = std::make_unique<PersistenceSnapshot>();
  capsule->generation = 1;
  const auto stage =
      composerRoot + "/exports/" + composerClient + "-" + composerRequestId;
  need(projectMakeDirectory(stage.c_str()), "export",
       "cannot create staging folder");
  std::snprintf(capsule->directory.data(), capsule->directory.size(), "%s",
                stage.c_str());
  struct ComposerStageCleanup {
    std::string path;
    ~ComposerStageCleanup() { ThruInstance::projectRemoveTree(path); }
  } cleanup{stage};
  std::set<int> included;
  std::map<std::string, std::string> sourceAssets;
  for (int i = 0; i < 64; ++i) {
    if (selection >= 0 ? i != selection : !snapshot.clips[i].occupied)
      continue;
    included.insert(i);
    auto &c = capsule->clips[0];
    c = snapshot.clips[i];
    auto ids = ComposerJson::array();
    std::set<std::string> seen;
    const auto encode = [&](auto &p, LaneSourceKind *kind, int lane) {
      if (!p[0] && kind && *kind != LaneSourceKind::empty && lane >= 0) {
        const auto fallback = std::string(snapshot.directory.data()) +
                              "/thru-lane-" + std::to_string(lane + 1) + ".wav";
        if (::access(fallback.c_str(), R_OK) == 0)
          std::snprintf(p.data(), p.size(), "%s", fallback.c_str());
      }
      if (!p[0]) {
        p.fill(0);
        return;
      }
      const std::string source = p.data();
      auto found = sourceAssets.find(source);
      if (found == sourceAssets.end())
        found =
            sourceAssets
                .emplace(source, composerAsset(doc, source).at("id").string())
                .first;
      const auto id = found->second;
      if (seen.insert(id).second)
        ids.push(id);
      p.fill(0);
      std::snprintf(p.data(), p.size(), "asset:%s", id.c_str());
      if (kind)
        *kind = LaneSourceKind::capture;
    };
    for (int l = 0; l < 4; ++l)
      encode(c.laneSourcePaths[l], &c.laneSourceKinds[l], l);
    for (auto &b : c.bankSlots)
      encode(b.path, nullptr, -1);
    const auto id = "native-" + std::to_string(i + 1),
               entry = "extensions/s3g-thru/" + id + ".bin";
    need(writeClipFile(capsule.get()), "export", "native clip encoding failed");
    const auto native = stage + "/" + id + ".bin";
    need(std::rename((stage + "/thru-clips.bin").c_str(), native.c_str()) == 0,
         "export", "cannot stage native clip");
    auto names = ComposerJson::array();
    for (const auto &b : c.bankSlots)
      names.push(b.name.data());
    records[id] = composerObject({{"file", entry},
                                  {"assets", std::move(ids)},
                                  {"bankNames", std::move(names)}});
    files.emplace_back(entry, native);
    auto core = composerClip(c, i);
    core["extensions"] =
        composerObject({{"s3gThru", composerObject({{"base", id}})}});
    doc["clips"].push(std::move(core));
  }
  need(!included.empty(), "export", "no clips selected");
  for (const auto &b : snapshot.banks) {
    auto entries = ComposerJson::array();
    bool has = false, complete = true;
    for (const auto &e : b.entries)
      if (e.enabled) {
        has = true;
        complete &= included.count(e.clip) != 0;
      }
    if (!has || !complete)
      continue;
    for (const auto &e : b.entries)
      if (e.enabled)
        entries.push(
            composerObject({{"clip", "clip-" + std::to_string(int(e.clip) + 1)},
                            {"enabled", e.enabled},
                            {"repeats", int(e.repeats)},
                            {"fade", composerFade(e.fade, e.fadeMode)}}));
    doc["chains"].push(
        composerObject({{"name", b.name.data()},
                        {"end", b.stop ? "stop" : b.loop ? "loop" : "stay"},
                        {"fade", composerFade(b.fade, b.fade.mode ? 3 : 2)},
                        {"entries", std::move(entries)}}));
  }
  doc["extensions"] = composerObject(
      {{"s3gThru",
        composerObject({{"version", 1}, {"clips", std::move(records)}})}});
  validate(doc);
  const auto text = s3g_thru_json::stringify(doc);
  auto list = ComposerJson::array();
  for (const auto &[name, path] : files) {
    const auto bytes = readFile(path, 64u * 1024 * 1024);
    list.push(composerObject({{"path", name},
                              {"size", double(bytes.size())},
                              {"sha256", sha256(bytes)}}));
  }
  const auto manifest = s3g_thru_json::stringify(
      composerObject({{"format", "s3g-thrupack"},
                      {"version", 1},
                      {"composition", "composition.json"},
                      {"sha256", sha256(text)},
                      {"portable", false},
                      {"files", std::move(list)}}));
  const auto output = composerRoot + "/exports/" + composerClient + "-" +
                      composerRequestId + ".thrupack";
  mz_zip_archive zip{};
  s3g_thru_project::ZipBudget budget;
  s3g_thru_project::setupZip(zip, budget);
  need(mz_zip_writer_init_file(&zip, (output + ".tmp").c_str(), 0), "export",
       "cannot open archive");
  bool ok = mz_zip_writer_add_mem(&zip, "composition.json", text.data(),
                                  text.size(), 0) &&
            mz_zip_writer_add_mem(&zip, "manifest.json", manifest.data(),
                                  manifest.size(), 0);
  for (const auto &[name, path] : files)
    if (ok)
      ok = mz_zip_writer_add_file(&zip, name.c_str(), path.c_str(), nullptr, 0,
                                  MZ_BEST_SPEED) != 0;
  ok = ok && mz_zip_writer_finalize_archive(&zip);
  const bool ended = mz_zip_writer_end(&zip) != 0;
  need(ok && ended &&
           std::rename((output + ".tmp").c_str(), output.c_str()) == 0,
       "export", "cannot finalize archive");
  composerResult = output;
  projectRemoveTree(stage);
}
void composerDecode(const ComposerJson &request) {
  using namespace s3g_thru_pack;
  const auto &name = string(request.at("pack"), "request.pack", 100);
  need(name.size() > 9 && name.substr(name.size() - 9) == ".thrupack" &&
           safeId(name.substr(0, name.size() - 9)),
       "request.pack", "invalid upload name");
  Archive pack(composerRoot + "/uploads/" + name);
  auto &work = *composerWork;
  work.document = pack.document;
  std::map<std::string, std::string> assets;
  const bool durable = work.operation == 2;
  const auto destination =
      durable
          ? std::string(moduleDirectory.data()) + "/captures"
          : std::strncmp(moduleRootDirectory.data(), "/data/", 6)
                ? std::string(moduleRootDirectory.data()) + "/library/composer"
                : composerRoot + "/cache";
  if (destination.find("/library/composer") != std::string::npos)
    need(projectMakeDirectory(
             (std::string(moduleRootDirectory.data()) + "/library").c_str()),
         "samples", "cannot create library");
  need(projectMakeDirectory(destination.c_str()), "samples",
       "cannot create folder");
  for (const auto &a : pack.document.at("assets").arrayValue()) {
    const auto &id = a.at("id").string();
    const auto *h = a.find("sha256");
    need(h && hashId(h->string()) && a.find("size") && a.find("file"), "asset",
         "relink missing audio before preview");
    const auto hash = h->string(),
               cache = composerRoot + "/cache/" + hash + ".wav",
               file = a.at("file").string();
    if (pack.entries.count(file))
      pack.extractFile(file, cache);
    struct stat st {};
    need(::lstat(cache.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
             double(st.st_size) == a.at("size").number() &&
             composerHashFile(cache) == hash,
         "asset", "missing or corrupt audio; relink it in the browser");
    (void)composerWavMetadata(cache);
    const auto target = destination + "/p" + hash + ".wav";
    if (target != cache)
      composerCopyFile(cache, target, hash, durable);
    assets[id] = target;
  }
  work.candidates.clear();
  work.destinations.clear();
  const auto &mapping = array(request.at("mapping"), "request.mapping", 64, 1);
  std::set<int> destinations;
  for (const auto &m : mapping) {
    keys(m, "mapping", {"clip", "slot"});
    const auto &id = string(m.at("clip"), "mapping.clip", 64);
    const int slot = int(number(m.at("slot"), "mapping.slot", 0, 63, true));
    need(destinations.insert(slot).second, "mapping", "destination used twice");
    const ComposerJson *core = nullptr;
    for (const auto &c : pack.document.at("clips").arrayValue())
      if (c.at("id").string() == id)
        core = &c;
    need(core, "mapping.clip", "unknown clip");
    work.candidates.emplace_back();
    auto &candidate = work.candidates.back();
    composerDefaultClip(candidate);
    if (const auto *extension = core->find("extensions"))
      if (const auto *n = extension->find("s3gThru")) {
        const auto &record = pack.document.at("extensions")
                                 .at("s3gThru")
                                 .at("clips")
                                 .at(n->at("base").string());
        const auto file = record.at("file").string(),
                   path = composerRoot + "/cache/" + n->at("base").string() +
                          ".bin";
        pack.extractFile(file, path);
        auto native = std::make_unique<PersistenceSnapshot>();
        need(readClipFileFromPath(path.c_str(), kAnyPersistenceGeneration,
                                  native.get()),
             "native clip", "invalid capsule");
        candidate = native->clips[0];
        if (const auto *names = record.find("bankNames"))
          for (int b = 0; b < 16; ++b)
            std::snprintf(candidate.bankSlots[b].name.data(),
                          candidate.bankSlots[b].name.size(), "%s",
                          names->arrayValue()[b].string().c_str());
      }
    composerReadClip(candidate, *core,
                     float(pack.document.at("tempo").number()));
    composerResolvePaths(candidate, assets);
    work.destinations.push_back(slot);
  }
  need(work.operation != 1 || work.candidates.size() == 1, "preview",
       "select one clip to audition");
  work.selected = int(composerNumber(request, "selected", 0));
  need(work.selected >= 0 && work.selected < int(work.candidates.size()),
       "selected", "invalid clip index");
  work.target = work.destinations[work.selected];
  work.replacing = composerBoolean(request, "replace", false);
  work.includeChains = composerBoolean(request, "chains", false);
  work.chains = {};
  if (work.includeChains) {
    const auto *chains = pack.document.find("chains");
    need(chains, "chains", "no arrangement in this pack");
    int b = 0;
    for (const auto &chain : chains->arrayValue()) {
      auto &bank = work.chains[b++];
      std::snprintf(bank.name.data(), bank.name.size(), "%s",
                    chain.at("name").string().c_str());
      const auto end = chain.at("end").string();
      bank.loop = end == "loop";
      bank.stop = end == "stop";
      if (const auto *f = chain.find("fade")) {
        composerReadFade(bank.fade, *f);
        bank.savedFade = true;
      }
      int row = 0;
      for (const auto &e : chain.at("entries").arrayValue()) {
        auto &target = bank.entries[row++];
        const auto clip = e.at("clip").string();
        bool found = false;
        for (std::size_t k = 0; k < mapping.size(); ++k)
          if (mapping[k].at("clip").string() == clip) {
            target.clip = std::uint8_t(work.destinations[k]);
            found = true;
            break;
          }
        need(found, "chain", "all referenced clips must have destinations");
        target.enabled = composerBoolean(e, "enabled", true);
        target.repeats = std::uint8_t(composerNumber(e, "repeats", 1));
        if (const auto *f = e.find("fade")) {
          const auto mode = f->at("mode").string();
          target.fadeMode =
              mode == "cut" ? 1
                            : mode == "fixed" ? 2 : mode == "random" ? 3 : 0;
          composerReadFade(target.fade, *f);
        }
      }
    }
  }
}
void composerStatus(const std::string &phase, bool ok,
                    const std::string &message, bool reply) noexcept {
  try {
    auto state = composerObject(
        {{"protocol", 1},
         {"epoch", composerEpoch},
         {"client", composerClient},
         {"request", composerRequestId},
         {"phase", phase},
         {"ok", ok},
         {"message", message},
         {"revision",
          std::to_string(
              (reply && composerOperation == "read" && composerResultRevision)
                  ? composerResultRevision
                  : composerLiveRevision.load(std::memory_order_acquire))},
         {"activeClip", composerLiveActive.load()},
         {"playing", bool(composerLivePlaying.load())},
         {"preview", composerOwned.load()},
         {"progress", composerProgress.load()},
         {"total", composerCopyTotal.load()},
         {"root", composerRoot},
         {"result", composerResult},
         {"undo", composerUndoReady.load()}, {"panel",composerPanelPath},{"panelVersion",composerPanelPublishedVersion},{"panelError",composerPanelError},{"panelErrorClip",composerPanelErrorClip},{"panelProtocol",2},{"panelBuild","0.34.1"}});
    for(int attempt=0;attempt<3;++attempt){const auto before=composerLayersSequence.load(std::memory_order_acquire);if(before&1u)continue;const bool active=composerLayersActive.load(),override=composerLayersOverride.load();const int layerPoints=composerLayersPointCount.load();std::array<float,11> values{};for(std::size_t i=0;i<values.size();++i)values[i]=composerLayersPublished[i].load();if(composerLayersSequence.load(std::memory_order_acquire)!=before)continue;
      state["layers"]=composerObject({{"active",active},{"recorded",layerPoints},{"override",override},{"xy",ComposerJson::Array{values[0],values[1]}},{"weights",ComposerJson::Array{values[2],values[3],values[4],values[5]}},{"pressure",values[6]},{"algorithms",ComposerJson::Array{values[7],values[8],values[9],values[10]}}});break;}
    const int history=composerHistoryPublished.load();state["history"]=composerObject({{"undo",bool(history&1)},{"redo",bool(history&2)},{"action",history>>2}});
    auto receipts=ComposerJson::array();for(const auto& v:composerEditReceipts){const auto n=v.load(std::memory_order_acquire);if(n)receipts.push(double(n));}state["edits"]=std::move(receipts);
    auto editErrors=ComposerJson::array();for(std::size_t i=0;i<composerEditReceipts.size();++i){const auto receipt=composerEditReceipts[i].load(std::memory_order_acquire);if(!receipt||(receipt&1u))continue;
      std::string reason;for(const auto& c:composerEditErrors[i]){const auto value=c.load(std::memory_order_relaxed);if(!value)break;reason+=value;}
      if(!reason.empty()&&receipt==composerEditReceipts[i].load(std::memory_order_acquire))editErrors.push(ComposerJson::Array{double(receipt>>1),reason});
    }state["editErrors"]=std::move(editErrors);
    auto cursors = ComposerJson::array();
    for (const auto &c : sequencePublishedCursor)
      cursors.push(c.load(std::memory_order_acquire));
    state["cursors"] = std::move(cursors);
    for(int attempt=0;attempt<2;++attempt){const auto seq=composerReadoutSequence.load(std::memory_order_acquire);if(seq&1u)continue;auto bindings=ComposerJson::array();for(int i=0;i<composerReadoutCount.load();++i){const auto route=composerReadoutRoutes[i].load();if(route!=0xffffffffu)bindings.push(ComposerJson::Array{ComposerJson(composerReadoutIds[i].load()),ComposerJson(int(route>>24)),ComposerJson(int(route&65535u)),ComposerJson(int((route>>16)&255u))});}auto values=ComposerJson::array();for(int i=0;i<composerReadoutCount.load();++i)if(composerReadoutValid[i].load())values.push(ComposerJson::Array{ComposerJson(composerReadoutIds[i].load()),ComposerJson(composerReadoutValues[i].load())});auto readout=composerObject({{"clip",composerReadoutClip.load()},{"part",composerReadoutPart.load()},{"patch",composerReadoutPatch.load()},{"values",std::move(values)},{"bindings",std::move(bindings)}});if(seq==composerReadoutSequence.load(std::memory_order_acquire)){state["readout"]=std::move(readout);break;}}
state["sampleLoad"]=composerObject({{"job",composerSampleJob.load()},{"progress",composerSampleProgress.load()},{"audition",composerSampleAudible.load()}});
    state["sequenceClock"]=composerClockJson();state["emittedAt"]=double(composerClock());
    state["performance"]=composerPerformancePath;state["performanceVersion"]=composerPerformanceVersion;state["recording"]=bool(composerRecordPublished.load());state["positionMs"]=composerPositionPublished.load();state["noteCount"]=composerNotePublished.load();state["pointCount"]=composerPointPublished.load();state["recordMask"]=composerMaskPublished.load();
    // A refused sample operation is a local dialog error, not a lost connection.
    const bool sampleError=!ok&&(composerOperation=="samples"||composerOperation=="sample"||composerOperation=="markers");
    if(sampleError){state["ok"]=true;state["phase"]="ready";state["message"]="";}
    s3g_thru_pack::atomicFile(composerRoot + "/status.json", s3g_thru_json::stringify(state));
    if(sampleError){state["ok"]=false;state["phase"]=phase;state["message"]=message;}
    const auto text = s3g_thru_json::stringify(state);
    if (reply && !composerRequestId.empty())
      s3g_thru_pack::atomicFile(composerRoot + "/replies/" + composerClient +
                                    "-" + composerRequestId + ".json",
                                text);
  } catch (...) {
    composerIoFailed.store(true, std::memory_order_release);
  }
}
#include "thru_composer_samples.inc"
#include "thru_composer_markers.inc"
bool processComposerWorker() {
  const int phase = composerPhase.load(std::memory_order_acquire);
  if (phase == composerRequest) {
    try {
      composerInitWorker();
      const std::string packet = composerPacket.data();
      std::vector<std::string> tokens;
      std::size_t start = 0;
      for (std::size_t i = 0; i <= packet.size(); ++i)
        if (i == packet.size() || packet[i] == '|') {
          tokens.push_back(packet.substr(start, i - start));
          start = i + 1;
        }
      s3g_thru_pack::need(tokens.size() == 5, "command", "invalid command");
      composerClient = tokens[0];
      composerRequestId = tokens[1];
      composerOperation = tokens[2];
      composerFilename = tokens[3];
      composerResult.clear();
      composerResultRevision = 0;
      const auto completed = composerClient + "-" + composerRequestId;
      for (const auto &old : composerCompleted)
        if (old == completed) {
          composerPhase.store(composerOwned.load() ? composerActive
                                                   : composerIdle,
                              std::memory_order_release);
          return true;
        }
      if (composerOperation == "hello") {
        composerPanelPath.clear();composerPanelRevision=0;
        composerStatus("ready", true, "Connected to s3g Thru", true);
        composerPhase.store(composerOwned.load() ? composerActive
                                                 : composerIdle,
                            std::memory_order_release);
        return true;
      }
      s3g_thru_pack::need(
          !projectImportInstalled.load(std::memory_order_acquire), "project",
          "reopen Thru after importing a project before using the composer");
      if (composerOperation == "cancel" && !composerOwned.load()) {
        composerStatus("ready", true, "No preview is active", true);
        composerPhase.store(composerIdle, std::memory_order_release);
        return true;
      }
      if (composerOperation == "cancel" || composerOperation == "undo") {
        s3g_thru_pack::need(composerWork && composerWork->original &&
                                composerWork->captured,
                            "composer", "no preview or import to restore");
        if (composerOperation == "undo")
          s3g_thru_pack::need(composerUndoReady.load(), "undo",
                              "import Undo is no longer available");
        composerWork->restore = true;
        composerWork->operation = composerOperation == "undo" ? 4 : 3;
        composerWork->offset = 0;
        composerWork->started = false;
        composerUndoReady.store(false);
        composerOwned.store(true, std::memory_order_release);
        composerPhase.store(composerInstall, std::memory_order_release);
        return true;
      }
      if(composerOperation=="samples"||composerOperation=="sample"||composerOperation=="markers"){
        s3g_thru_pack::need(!std::strcmp(composerPanelOwner.data(),composerClient.c_str()),"sample","Reconnect the live panel before loading samples");
        const auto request=composerSampleRequest();
        if(composerOperation=="samples"){composerBrowseSamples(request);composerStatus("ready",true,"Sample folder read",true);composerCompleted[composerCompletedAt++%16]=completed;composerPhase.store(composerOwned.load()?composerActive:composerIdle);}
        else{ s3g_thru_pack::need(!composerOwned.load(),"sample","finish preview first");s3g_thru_pack::need(tokens[4]==std::to_string(composerLiveRevision.load()),"sample","settings changed; retry loading");if(composerOperation=="markers")composerPrepareMarkers(request);else composerPrepareSample(request); }
        return true;
      }
      s3g_thru_pack::need(
          composerOperation == "read" ||
              tokens[4] == std::to_string(composerLiveRevision.load()),
          "project",
          "project changed; read from Move again before applying this draft");
      if (composerOperation == "read") {
        if (composerOwned.load()) {
          composerResultRevision = composerWork->original->revision;
          composerEncode(*composerWork->original,
                         composerFilename == "all" ? -1
                                                   : composerLiveActive.load());
          composerStatus("ready", true, "Original saved clips read", true);
          composerPhase.store(composerActive, std::memory_order_release);
          return true;
        }
        composerReadSelection = composerFilename == "all"
                                    ? -1
                                    : composerFilename.empty()
                                          ? composerLiveActive.load()
                                          : std::stoi(composerFilename);
        s3g_thru_pack::need(composerReadSelection >= -1 &&
                                composerReadSelection < 64,
                            "slot", "invalid clip selection");
        composerReadSnapshot = std::make_unique<PersistenceSnapshot>();
        composerProgress.store(0);
        composerPhase.store(composerCapture, std::memory_order_release);
        return true;
      }
      s3g_thru_pack::need(composerOperation == "preview" ||
                              composerOperation == "apply",
                          "command", "unsupported operation");
      s3g_thru_pack::need(
          composerFilename.size() > 5 &&
              composerFilename.substr(composerFilename.size() - 5) == ".json" &&
              s3g_thru_pack::safeId(
                  composerFilename.substr(0, composerFilename.size() - 5)),
          "command", "invalid request file");
      const auto request = s3g_thru_json::parse(s3g_thru_pack::readFile(
          composerRoot + "/uploads/" + composerFilename, 65536));
      s3g_thru_pack::keys(request, "request",
                          {"pack", "mapping", "selected", "replace", "chains"});
      if (!composerOwned.load()) {
        composerUndoReady.store(false, std::memory_order_release);
        composerWork = std::make_unique<ComposerTransaction>();
        composerWork->original = std::make_unique<PersistenceSnapshot>();
        composerWork->changed.reserve(64);
      }
      auto &job = *composerWork;
      job.operation = composerOperation == "preview" ? 1 : 2;
      job.restore = false;
      job.started = false;
      job.offset = 0;
      job.revision = composerLiveRevision.load();
      composerDecode(request);
      composerProgress.store(0);
      composerPhase.store(job.captured ? composerInstall : composerCapture,
                          std::memory_order_release);
    } catch (const std::exception &e) {
      composerStatus("error", false, e.what(), true);
      composerPhase.store(composerOwned.load() ? composerActive : composerIdle,
                          std::memory_order_release);
    }
    return true;
  }
  if(phase==composerSampleLoading){
    const int state=composerSampleJob.load(std::memory_order_acquire);
    if(state==6){processComposerMarkerWrite();return true;}
    if(state==3||state==4){cleanupComposerSamplePreview();composerStatus(state==3?"ready":"error",state==3,state==3?(composerSamplePreview?"Audition started":"Sample loaded"):composerSampleError.data(),true);composerCompleted[composerCompletedAt++%16]=composerClient+"-"+composerRequestId;composerSampleJob.store(0);composerPhase.store(composerIdle);return true;}
  }
  if (phase == composerCaptured) {
    try {
      if (composerOperation == "read") {
        composerResultRevision = composerReadSnapshot->revision;
        composerEncode(*composerReadSnapshot,
                       composerFilename == "all"
                           ? -1
                           : int(composerFilename.empty()
                                     ? composerReadSnapshot->active
                                     : std::stoi(composerFilename)));
        composerReadSnapshot.reset();
        composerStatus("ready", true, "Clips read into browser draft", true);
        composerPhase.store(composerIdle, std::memory_order_release);
      } else {
        auto &job = *composerWork;
        composerPrepareOriginalPlayback();
        for (int slot : job.destinations)
          if (job.operation == 2)
            s3g_thru_pack::need(
                job.replacing || !job.original->clips[slot].occupied,
                "destination", "occupied clip requires Replace confirmation");
        composerPhase.store(composerInstall, std::memory_order_release);
      }
    } catch (const std::exception &e) {
      composerStatus("error", false, e.what(), true);
      composerOwned.store(false);
      composerPhase.store(composerIdle, std::memory_order_release);
    }
    return true;
  }
  if (phase == composerFinished || phase == composerFailure) {
    const bool ok = phase == composerFinished;
    try {
      auto &j = *composerWork;
      composerStatus(
          ok ? j.operation == 1 ? "preview" : "ready" : "error", ok,
          ok ? j.operation == 1
                   ? "Preview on Move; Back or Cancel restores the original"
                   : j.operation == 2 ? "Clips applied; Undo is available"
                                      : "Original clips restored"
             : j.message,
          true);
      composerCompleted[composerCompletedAt++ % 16] =
          composerClient + "-" + composerRequestId;
    } catch (...) {
    }
    composerPhase.store(composerOwned.load() ? composerActive : composerIdle,
                        std::memory_order_release);
    return true;
  }
  if (composerPanelWatching.load() && !composerRoot.empty() && !composerOwned.load() && composerLease.load()>composerClock()) {
    if (!composerPanelClip) composerPanelClip=std::make_unique<ClipSlot>();
    processComposerWorkspace();
    const int job=composerPanelJob.load(std::memory_order_acquire);
    if(job==2){
      try{publishComposerPanel();composerPanelError.clear();composerPanelErrorClip=-1;}catch(const std::exception& e){composerPanelError=e.what();composerPanelErrorClip=composerPanelActive;}catch(...){composerPanelError="Cannot publish live clip state";composerPanelErrorClip=composerPanelActive;}
      composerPanelJob.store(0,std::memory_order_release);
    }
    if(composerPanelJob.load()==0&&composerClock()>=composerPanelAt
        &&(composerPanelPath.empty()||composerPanelOwnerGeneration.load()!=composerPanelCapturedGeneration||composerLiveRevision.load()!=composerPanelRevision||composerLiveActive.load()!=composerPanelActive||composerPanelLiveLane.load()!=composerPanelSelectedLane||composerPanelWaveVersion.load()!=composerPanelCapturedWaveVersion||composerPanelLiveFocus.load()!=composerPanelFocus||composerEditVersion.load()!=composerPanelEditVersion)){
      composerPanelAt=composerClock()+100;
      composerPanelJob.store(1,std::memory_order_release);
    }
  }
  if (!composerRoot.empty() && composerClock() > composerStatusAt &&
      composerLease.load() > composerClock()) {
    composerStatusAt = composerClock() + 80;
    try {
      composerStatus(phase == composerActive
                         ? "preview"
                         : phase == composerIdle ? "ready" : "busy",
                     true,
                     phase == composerCapture
                         ? "Reading clip data"
                         : phase == composerInstalling ? "Loading draft" : "",
                     false);
    } catch (...) {
    }
    return false;
  }
  return false;
}
void composerRestoreGlobals(bool original) {
  if (!composerWork || !composerWork->original)
    return;
  const auto &s = composerWork->original->state;
  tempoBpm = original ? s.tempoBpm
                      : float(composerWork->document.at("tempo").number());
  selectedLane = s.selectedLane;
  midiSyncSource = s.midiSyncSource;
  midiSyncTransport = s.midiSyncTransport;
  patternFadeMs = s.patternFadeMs;
  patternFadeLowMs = s.patternFadeLowMs;
  patternFadeHighMs = s.patternFadeHighMs;
  patternCurve = s.patternCurve;
  patternTimingMode = s.patternTimingMode;
  patternHeld = false;
  launchQuantize = s.launchQuantize;
}
void serviceComposer() {
  if (composerIoFailed.exchange(false)) {
    setError("Browser transfer failed; check storage space");
    composerCancel.store(true);
  }
  const auto now = composerClock();
  serviceComposerSample();
  composerSampleAudible.store(previewActive&&sampleRequestPreview,std::memory_order_relaxed);
  if(!composerOwned.load()){serviceComposerWorkspace();captureComposerClock();captureComposerReadouts(*this);captureComposerPanel();captureComposerPerformance();}
  composerLiveRevision.store(persistentRevision, std::memory_order_release);
  composerLiveActive.store(activeClip);
  composerPanelLiveLane.store(int(selectedLane));composerPanelLiveFocus.store(splitEnabled?splitFocus:0);
  composerLivePlaying.store(clipPlaying);
  auto phase = composerPhase.load(std::memory_order_acquire);
  if (composerOwned.load() &&
      (composerCancel.load() ||
       (composerLease.load() && now > composerLease.load())) &&
      (phase == composerActive || phase == composerIdle)) {
    composerCancel.store(false);
    auto &j = *composerWork;
    j.restore = true;
    j.operation = 3;
    j.started = false;
    j.offset = 0;
    composerPhase.store(composerInstall, std::memory_order_release);
    phase = composerInstall;
  }
  if (phase == composerCapture) {
    if (audioRecording() || clipRecording || namingBusy() || namingLoading() ||
        synthOpened || partCaptureRecording || partCapturePublishing ||
        projectJob.load() != projectIdle || pendingCaptureSaveMask ||
        patternBusy() || clipPreparationInFlight() || bankJob.load())
      return;
    if (persistenceJob.load() != backgroundIdle || persistentStateDirty) {
      maintenanceRequested = true;
      return;
    }
    if (composerOperation != "read" && composerLease.load() &&
        now > composerLease.load()) {
      composerWork->message = "Browser disconnected before Preview";
      composerOwned.store(false);
      composerPhase.store(composerFailure, std::memory_order_release);
      return;
    }
    auto &snapshot = composerOperation == "read" ? *composerReadSnapshot
                                                 : *composerWork->original;
    if (snapshot.revision == 0) {
      if (composerOperation != "read" &&
          composerWork->revision != persistentRevision) {
        composerWork->message =
            "Project changed while preparing; read from Move again";
        composerPhase.store(composerFailure, std::memory_order_release);
        return;
      }
      captureActiveClip(false);
      snapshot.revision = persistentRevision;
      snapshot.state.capture(*this);
      snapshot.tempo = tempoBpm;
      snapshot.swing = tempoSwing;
      snapshot.quantize = launchQuantize;
      snapshot.grainLimit = grainVoiceLimit;
      snapshot.active = activeClip;
      snapshot.chainBank = chainBankIndex;
      snapshot.chainPlayBank = chainPlayBank;
      snapshot.chainPosition = chainPosition;
      snapshot.chainEdit = chainEditSlot;
      snapshot.banks = chainBanks;
      snapshot.directory = moduleDirectory;
      if (composerOperation != "read") {
        composerWork->playing = clipPlaying;
        std::snprintf(composerOwner.data(), composerOwner.size(), "%s",
                      composerClient.c_str());
        composerOwned.store(true, std::memory_order_release);
      }
    }
    if (snapshot.revision != persistentRevision) {
      snapshot.revision = 0;
      composerProgress.store(0);
      return;
    }
    // Read-only snapshots can retry without blocking live editing. Preview
    // takes ownership before copying; temporary edits never enter
    // autosave/state.
    const bool selectedOnly =
        composerOperation == "read" && composerReadSelection >= 0;
    const std::size_t base = selectedOnly ? std::size_t(composerReadSelection) *
                                                sizeof(ClipSlot)
                                          : 0,
                      total = selectedOnly ? sizeof(ClipSlot)
                                           : sizeof(snapshot.clips);
    composerCopyTotal.store(int(total));
    const auto offset = std::size_t(composerProgress.load());
    const auto count = std::min<std::size_t>(16384, total - offset);
    std::memcpy(reinterpret_cast<char *>(snapshot.clips.data()) + base + offset,
                reinterpret_cast<const char *>(clipSlots.data()) + base +
                    offset,
                count);
    composerProgress.store(int(offset + count));
    if (offset + count == total) {
      if (composerOperation != "read")
        composerWork->captured = true;
      composerPhase.store(composerCaptured, std::memory_order_release);
    }
    return;
  }
  if (phase == composerInstall || phase == composerInstalling) {
    auto &j = *composerWork;
    if (!j.started) {
      if (patternBusy() || clipPreparationInFlight() || audioRecording() ||
          clipRecording || namingBusy() || bankJob.load())
        return;
      if (!j.restore && j.operation == 2 && !j.replacing) {
        for (int slot : j.destinations)
          if (j.original->clips[slot].occupied) {
            j.message = "Destination occupied; confirm Replace";
            composerPhase.store(composerFailure, std::memory_order_release);
            return;
          }
      }
      setClipPlaying(false);
      chainEnabled = false;
      cancelChainLaunch();
      midiSyncSource = 0;
      midiSyncTransport = 0;
      patternFadeMs = 0;
      patternFadeHighMs = 0;
      patternTimingMode = 0;
      patternHeld = false;
      launchQuantize = LaunchQuantize::off;
      allNotesOff();
      stopPreviewWithFade();
      synthPreviewWanted = false;
      beginSourceTransitionTail();
      stopSequenceAudition();
      j.started = true;
      j.offset = 0;
      composerProgress.store(0);
      if (!j.restore)
        for (int slot : j.destinations)
          if (std::find(j.changed.begin(), j.changed.end(), slot) ==
              j.changed.end())
            j.changed.push_back(slot);
      composerPhase.store(composerInstalling, std::memory_order_release);
    }
    const auto size = sizeof(ClipSlot), total = j.changed.size() * size;
    std::size_t budget = 16384;
    while (j.offset < total && budget) {
      const auto at = j.offset / size, in = j.offset % size;
      const int slot = j.changed[at];
      const ClipSlot *from = &j.original->clips[slot];
      if (!j.restore)
        for (std::size_t i = 0; i < j.destinations.size(); ++i)
          if (j.destinations[i] == slot)
            from = &j.candidates[i];
      const auto n = std::min({budget, size - in, total - j.offset});
      std::memcpy(reinterpret_cast<char *>(&clipSlots[slot]) + in,
                  reinterpret_cast<const char *>(from) + in, n);
      j.offset += n;
      budget -= n;
    }
    composerProgress.store(int(j.offset));
    if (j.offset == total) {
      if (j.restore) {
        chainBanks = j.original->banks;
        chainBankIndex = j.original->chainBank;
        chainPlayBank = j.original->chainPlayBank;
        chainPosition = j.original->chainPosition;
        chainEditSlot = j.original->chainEdit;
      } else if (j.includeChains) {
        chainBanks = j.chains;
        chainBankIndex = chainPlayBank = chainPosition = chainEditSlot = 0;
      }
      composerCopyTotal.store(int(total));
      const int target = j.restore ? j.original->active : j.target;
      tempoBpm = j.restore ? j.original->tempo
                           : float(j.document.at("tempo").number());
      if (j.restore)
        clipSlots[target].laneSourcePaths = j.originalPlaybackPaths;
      queuedClip = target;
      requestClipPreparation(target);
      composerPhase.store(composerLoading, std::memory_order_release);
    }
    return;
  }
  if (phase == composerLoading) {
    auto &j = *composerWork;
    const auto prep =
        ClipPrepareState(clipPrepareState.load(std::memory_order_acquire));
    if (prep == ClipPrepareState::error) {
      if (!j.restore) {
        j.message = "Sample preparation failed; restoring original";
        j.restore = true;
        j.operation = 3;
        j.started = false;
        j.offset = 0;
        clipPrepareState.store(int(ClipPrepareState::idle));
        composerPhase.store(composerInstall, std::memory_order_release);
      } else {
        j.message =
            "Original audio could not be prepared; clip data was restored";
        activateStoredClip(j.original->active, false);
        composerRestoreGlobals(true);
        currentLaneSourcePaths =
            j.original->clips[j.original->active].laneSourcePaths;
        currentLaneSourceKinds =
            j.original->clips[j.original->active].laneSourceKinds;
        laneSourceLabels =
            j.original->clips[j.original->active].laneSourceLabels;
        engine.clear();
        for (auto &v : voices) {
          v.active = false;
          v.held = false;
        }
        composerSkipFallbackWrites = true;
        queuedClip = -1;
        clipPrepareState.store(int(ClipPrepareState::idle));
        composerOwned.store(false);
        composerPhase.store(composerFailure, std::memory_order_release);
      }
      return;
    }
    const int expected = j.restore ? j.original->active : j.target;
    if (prep != ClipPrepareState::idle || activeClip != expected ||
        queuedClip >= 0)
      return;
    if (!j.restore && (composerCancel.load() ||
                       (composerLease.load() && now > composerLease.load()))) {
      composerCancel.store(false);
      j.restore = true;
      j.operation = 3;
      j.started = false;
      j.offset = 0;
      composerPhase.store(composerInstall, std::memory_order_release);
      return;
    }
    if (j.restore) {
      clipSlots[j.original->active].laneSourcePaths =
          j.original->clips[j.original->active].laneSourcePaths;
      currentLaneSourcePaths = clipSlots[j.original->active].laneSourcePaths;
      composerRestoreGlobals(true);
      composerCancel.store(false);
      chainEnabled = j.original->state.chainEnabled;
      composerOwned.store(false, std::memory_order_release);
      if (j.operation == 4)
        markPersistentStateDirty();
      if (j.playing)
        setClipPlaying(true);
      composerUndoReady.store(false);
    } else if (j.operation == 2) {
      composerRestoreGlobals(false);
      maintenanceRequested = true;
      composerOwned.store(false, std::memory_order_release);
      markPersistentStateDirty();
      j.committed = true;
      undoAvailable = false;
      composerUndoReady.store(true, std::memory_order_release);
      if (j.playing)
        setClipPlaying(true);
    } else {
      setClipPlaying(true);
    }
    composerLiveRevision.store(persistentRevision);
    composerPhase.store(composerFinished, std::memory_order_release);
  }
}
void composerRollbackOffline() {
  if (!composerOwned.load() || !composerWork || !composerWork->captured)
    return;
  // Called only after background/sample threads stop, never in render/control.
  auto &j = *composerWork;
  for (int slot : j.changed)
    clipSlots[slot] = j.original->clips[slot];
  chainBanks = j.original->banks;
  chainBankIndex = j.original->chainBank;
  chainPlayBank = j.original->chainPlayBank;
  chainPosition = j.original->chainPosition;
  chainEditSlot = j.original->chainEdit;
  activateStoredClip(j.original->active, false);
  composerRestoreGlobals(true);
  chainEnabled = j.original->state.chainEnabled;
  composerOwned.store(false);
  composerPhase.store(composerIdle);
}
