// Directory and upload I/O stays on the existing ordinary-priority worker.
// The callback only validates the frozen destination and requests the native
// loader.
std::atomic<int> composerSampleJob{0},
    composerSampleProgress{
        0}; // 0 idle, 1 prepared, 2 loading, 3 done, 4 failed
std::array<char, kPersistencePathSize> composerSamplePath{};
std::array<char, 128> composerSampleError{};
int composerSampleClip = 0, composerSamplePart = 0, composerSampleLane = 0,
    composerSampleMode = 0;
bool composerSampleMarkers = false;
bool composerSampleUseFile = false, composerSamplePreview = false,
     composerSampleUploaded = false;
std::atomic<bool> composerSampleAudible{false};
double composerSamplePreviewLow = 0, composerSamplePreviewHigh = 1;
std::uint64_t composerSampleGeneration = 0, composerSampleRevision = 0;
std::string composerLibraryRoot(int root) const {
  if (root == 1)
    return std::string(moduleDirectory.data()) + "/captures";
  if (root == 2)
    return synthDirectory.data();
  return std::strncmp(moduleRootDirectory.data(), "/data/", 6)
             ? std::string(moduleRootDirectory.data()) + "/library"
             : std::string(kSampleLibraryRoot);
}
static bool composerRelativeSamplePath(const std::string &path) {
  if (path.empty())
    return true;
  if (path.front() == '/' || path.find('\0') != std::string::npos ||
      path.find('\\') != std::string::npos)
    return false;
  std::size_t begin = 0;
  while (begin <= path.size()) {
    const auto end = path.find('/', begin);
    const auto piece =
        path.substr(begin, end == std::string::npos ? end : end - begin);
    if (piece.empty() || piece == "." || piece == "..")
      return false;
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return true;
}
static bool composerWavName(const std::string &path) {
  return path.size() > 4 && path[path.size() - 4] == '.' &&
         std::tolower(static_cast<unsigned char>(path[path.size() - 3])) ==
             'w' &&
         std::tolower(static_cast<unsigned char>(path[path.size() - 2])) ==
             'a' &&
         std::tolower(static_cast<unsigned char>(path.back())) == 'v';
}
std::string composerSampleResolve(int root, const std::string &relative,
                                  bool directory) const {
  using s3g_thru_pack::need;
  need(root >= 0 && root < 3 && composerRelativeSamplePath(relative), "sample",
       "invalid sample folder");
  const auto base = composerLibraryRoot(root);
  std::array<char, PATH_MAX> resolvedRoot{}, resolved{};
  need(realpath(base.c_str(), resolvedRoot.data()), "sample",
       "sample folder is unavailable");
  const auto path = relative.empty() ? base : base + "/" + relative;
  need(path.size() < kPersistencePathSize &&
           realpath(path.c_str(), resolved.data()),
       "sample", "sample file or folder is unavailable");
  const std::string own = resolvedRoot.data(), target = resolved.data();
  need(target == own || target.rfind(own + "/", 0) == 0, "sample",
       "sample path leaves its library");
  struct stat st {};
  need(::stat(target.c_str(), &st) == 0 &&
           (directory ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode)),
       "sample", directory ? "not a sample folder" : "not a sample file");
  need(directory || composerWavName(target), "sample", "choose a WAV file");
  return target;
}
ComposerJson composerSampleRequest() const {
  using namespace s3g_thru_pack;
  need(composerFilename.size() > 5 &&
           composerFilename.substr(composerFilename.size() - 5) == ".json" &&
           safeId(composerFilename.substr(0, composerFilename.size() - 5)),
       "sample", "invalid request file");
  return s3g_thru_json::parse(
      readFile(composerRoot + "/uploads/" + composerFilename, 16384));
}
static int composerSampleInteger(const ComposerJson &request, const char *key,
                                 int low, int high) {
  const auto n = request.at(key).number();
  s3g_thru_pack::need(n >= low && n <= high && n == std::floor(n), "sample",
                      "invalid destination or folder page");
  return int(n);
}
void composerBrowseSamples(const ComposerJson &request) {
  using namespace s3g_thru_pack;
  keys(request, "samples", {"root", "path", "offset", "search"});
  const int root = composerSampleInteger(request, "root", 0, 2),
            offset = composerSampleInteger(request, "offset", 0, 8192);
  const auto relative = request.at("path").string(),
             search = request.at("search").string();
  need(search.size() <= 100, "samples", "search is too long");
  const auto directory = composerSampleResolve(root, relative, true);
  std::unique_ptr<DIR, int (*)(DIR *)> dir(::opendir(directory.c_str()),
                                           ::closedir);
  need(bool(dir), "samples", "cannot read sample folder");
  struct Entry {
    std::string name;
    bool folder;
    double bytes;
  };
  std::vector<Entry> entries;
  const auto lower = [](std::string value) {
    for (auto &c : value)
      c = char(std::tolower(static_cast<unsigned char>(c)));
    return value;
  };
  const auto filter = lower(search);
  int inspected = 0;
  while (const auto *row = ::readdir(dir.get())) {
    const std::string name = row->d_name;
    if (name.empty() || name[0] == '.')
      continue;
    need(++inspected <= 8192, "samples", "folder is too large; use subfolders");
    const auto rel = relative.empty() ? name : relative + "/" + name;
    try {
      const auto full = composerSampleResolve(root, rel, true);
      if (full == composerRoot)
        continue;
      if (filter.empty() || lower(name).find(filter) != std::string::npos)
        entries.push_back({name, true, 0});
      continue;
    } catch (const std::exception &) {
    }
    if (!composerWavName(name))
      continue;
    try {
      const auto full = composerSampleResolve(root, rel, false);
      struct stat st {};
      if (::stat(full.c_str(), &st) == 0 &&
          (filter.empty() || lower(name).find(filter) != std::string::npos))
        entries.push_back({name, false, double(st.st_size)});
    } catch (const std::exception &) {
    }
  }
  std::sort(entries.begin(), entries.end(), [&](const auto &a, const auto &b) {
    return a.folder != b.folder ? a.folder > b.folder
                                : lower(a.name) < lower(b.name);
  });
  auto list = ComposerJson::array();
  for (std::size_t i = std::size_t(offset);
       i < entries.size() && i < std::size_t(offset + 64); ++i)
    list.push(composerObject({{"name", entries[i].name},
                              {"directory", entries[i].folder},
                              {"bytes", entries[i].bytes}}));
  auto data = composerObject({{"root", root},
                              {"path", relative},
                              {"offset", offset},
                              {"total", int(entries.size())},
                              {"entries", std::move(list)}});
  composerResult = composerRoot + "/samples-" + composerClient + ".json";
  atomicFile(composerResult, s3g_thru_json::stringify(data));
}
std::string composerUploadDirectory(const ComposerJson &request) const {
  using s3g_thru_pack::need;
  std::string relative;
  if (const auto *folder = request.find("folder"))
    relative = folder->string();
  else {
    auto directory = composerLibraryRoot(0);
    need(projectMakeDirectory(directory.c_str()), "sample",
         "cannot create sample library");
    if (!std::strncmp(moduleRootDirectory.data(), "/data/", 6)) {
      directory += "s3g Thru";
      need(projectMakeDirectory(directory.c_str()), "sample",
           "cannot create Thru folder");
      relative = "s3g Thru/Browser Samples";
    } else
      relative = "Browser Samples";
    directory += "/Browser Samples";
    need(projectMakeDirectory(directory.c_str()), "sample",
         "cannot create Browser Samples");
  }
  const auto directory = composerSampleResolve(0, relative, true);
  std::array<char, PATH_MAX> internal{};
  if (realpath(composerRoot.c_str(), internal.data())) {
    const std::string path = internal.data();
    need(directory != path && directory.rfind(path + "/", 0) != 0, "sample",
         "choose a sample folder outside Composer's working files");
  }
  return directory;
}
// Publish readable names atomically. Never replace an existing directory/file.
std::string composerStoreNamedFile(const std::string &source,
                                   const std::string &folder,
                                   const std::string &name,
                                   bool readyTemporary = false) {
  using s3g_thru_pack::need;
  const auto hash = composerHashFile(source);
  struct stat incoming {};
  need(::stat(source.c_str(), &incoming) == 0, "sample", "cannot inspect WAV");
  std::string temporary;
  struct Cleanup {
    std::string &path;
    ~Cleanup() {
      if (!path.empty())
        std::remove(path.c_str());
    }
  } cleanup{temporary};
  for (int serial = 1; serial <= 9999; ++serial) {
    const auto target = folder + "/" +
                        (serial == 1 ? name
                                     : name.substr(0, name.size() - 4) + "-" +
                                           std::to_string(serial) +
                                           name.substr(name.size() - 4));
    need(target.size() < kPersistencePathSize, "sample",
         "sample path is too long");
    struct stat st {};
    if (::lstat(target.c_str(), &st) == 0) {
      if (S_ISREG(st.st_mode) && st.st_size == incoming.st_size &&
          composerHashFile(target) == hash)
        return target;
      continue;
    }
    need(errno == ENOENT, "sample", "cannot inspect destination folder");
    if (temporary.empty() && !readyTemporary) {
      auto pattern = folder + "/.thru-upload-XXXXXX";
      std::vector<char> path(pattern.begin(), pattern.end());
      path.push_back(0);
      const int fd = mkstemp(path.data());
      need(fd >= 0, "sample", "cannot create upload file");
      temporary = path.data();
      if (::fchmod(fd, 0644) != 0) {
        ::close(fd);
        throw std::runtime_error("Cannot set sample file permissions");
      }
      auto *out = fdopen(fd, "wb");
      if (!out) {
        ::close(fd);
        throw std::runtime_error("Cannot open upload file");
      }
      std::ifstream in(source, std::ios::binary);
      bool ok = bool(in);
      s3g_thru_pack::Sha256 digest;
      std::array<char, 65536> block{};
      while (ok && in) {
        in.read(block.data(), block.size());
        const auto n = std::size_t(in.gcount());
        digest.update(block.data(), n);
        ok = std::fwrite(block.data(), 1, n, out) == n;
      }
      ok = ok && in.eof() && digest.finish() == hash;
      ok = s3g_thru_storage::closeFile(out, temporary.c_str(), ok) && ok;
      need(ok, "sample", "cannot store uploaded WAV");
    }
    if (s3g_thru_storage::link((readyTemporary ? source : temporary).c_str(),
                               target.c_str())) {
      need(s3g_thru_storage::syncParent(target.c_str()), "sample",
           "cannot finish storing WAV");
      return target;
    }
    need(errno == EEXIST, "sample",
         "cannot publish WAV in the destination folder");
  }
  throw std::runtime_error(
      "Too many files with that name; choose another filename");
}
int composerSampleDestinationPart() const {
  return composerSampleMode == 1
             ? composerSampleLane / 2
             : composerSampleMode == 3 || composerSampleMode == 6
                   ? composerSampleLane
                   : composerSampleMode == 2 ? composerSamplePart : 0;
}
void composerPrepareSample(const ComposerJson &request) {
  using namespace s3g_thru_pack;
  keys(request, "sample",
       {"clip", "part", "lane", "mode", "root", "path", "upload", "name",
        "file", "folder", "preview"});
  const auto integer = [&](const char *key, int low, int high) {
    return composerSampleInteger(request, key, low, high);
  };
  composerSampleClip = integer("clip", 0, 63);
  composerSamplePart = integer("part", 0, 3);
  composerSampleLane = integer("lane", 0, 3);
  composerSampleMode = integer("mode", 0, 6);
  composerSampleMarkers = false;
  composerSamplePreviewLow = 0;
  composerSamplePreviewHigh = 1;
  composerSampleUseFile = request.at("file").boolean();
  composerSamplePreview =
      request.find("preview") && request.at("preview").boolean();
  composerSampleUploaded = bool(request.find("upload"));
  std::string source;
  if (const auto *upload = request.find("upload")) {
    const auto leaf = upload->string(), name = request.at("name").string();
    need(composerWavName(leaf) && safeId(leaf.substr(0, leaf.size() - 4)),
         "sample", "invalid upload");
    need(!name.empty() && name.size() <= 180 &&
             name.find('/') == std::string::npos &&
             name.find('\\') == std::string::npos &&
             name.find('\0') == std::string::npos && name[0] != '.' &&
             composerWavName(name),
         "sample", "choose a WAV filename");
    for (unsigned char c : name)
      need(c >= 32 && c != 127, "sample", "invalid filename");
    const auto incoming = composerRoot + "/uploads/" + leaf;
    struct stat st {};
    need(::lstat(incoming.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
             st.st_size >= 44 && st.st_size <= 128 * 1024 * 1024,
         "sample", "upload a WAV smaller than 128 MiB");
    if (!projectWavValid(incoming)) {
      std::remove(incoming.c_str());
      throw std::runtime_error("Invalid WAV audio");
    }
    if (composerSamplePreview)
      source = incoming;
    else {
      const auto folder = composerUploadDirectory(request);
      source = composerStoreNamedFile(incoming, folder, name);
      std::remove(incoming.c_str());
    }
  } else {
    const int root = integer("root", 0, 2);
    source = composerSampleResolve(root, request.at("path").string(), false);
  }
  need(source.size() < composerSamplePath.size(), "sample",
       "sample path is too long");
  std::snprintf(composerSamplePath.data(), composerSamplePath.size(), "%s",
                source.c_str());
  composerSampleError[0] = 0;
  composerSampleProgress.store(0);
  composerSampleGeneration = composerPanelOwnerGeneration.load();
  composerSampleRevision = composerLiveRevision.load();
  composerSampleJob.store(1, std::memory_order_release);
  composerPhase.store(composerSampleLoading, std::memory_order_release);
}
void serviceComposerSample() {
  const int job = composerSampleJob.load(std::memory_order_acquire);
  if (job != 1 && job != 2 && job != 5)
    return;
  const auto fail = [&](const char *reason) {
    std::snprintf(composerSampleError.data(), composerSampleError.size(), "%.*s",
                  int(composerSampleError.size()) - 1, reason);
    composerSampleJob.store(4, std::memory_order_release);
  };
  if (job == 1 || job == 5) {
    const int part = composerSamplePart, lane = composerSampleLane;
    if (std::strcmp(composerPanelOwner.data(), composerClient.c_str()) ||
        composerSampleGeneration != composerPanelOwnerGeneration.load() ||
        composerLease.load() < composerClock() ||
        composerSampleClip != activeClip ||
        composerSampleMode != performanceMode() ||
        part != (splitEnabled ? splitFocus : 0) || (bankMode && part == 1)) {
      fail("Clip, part or mode changed; reopen Samples");
      return;
    }
    if (composerSampleRevision != persistentRevision) {
      fail("Settings changed; try loading the sample again");
      return;
    }
    if (audioRecording() || clipRecording || namingBusy() || synthOpened ||
        projectJob.load() != projectIdle || partCaptureRecording ||
        partCapturePublishing || bankJob.load() || liveCaptureJob.load() ||
        pendingCaptureSaveMask || patternBusy() || clipPreparationInFlight() ||
        queuedClip >= 0) {
      fail("Finish recording, file work or the clip transition first");
      return;
    }
    const int state = sampleLoadState.load();
    if (state >= int(SampleLoadState::requested) &&
        state <= int(SampleLoadState::copying)) {
      fail("Wait for the current sample load");
      return;
    }
    if (job == 5) {
      captureComposerMarkerSource();
      return;
    }
    clearError();
    requestLibrarySample(lane, composerSamplePath.data(), false, activeClip,
                         composerSamplePreview, composerSamplePreviewLow,
                         composerSamplePreviewHigh);
    const auto next = SampleLoadState(sampleLoadState.load());
    if (next != SampleLoadState::requested &&
        next != SampleLoadState::loading && next != SampleLoadState::ready) {
      fail(lastError[0] ? lastError.data() : sampleLoadError.data());
      return;
    }
    if (!composerSamplePreview) {
      composerUndoPerformance = false;
      composerHistoryClip = -1;
      if (!splitEnabled || sharedDual || partOwnsLane(splitFocus, lane))
        selectedLane = std::size_t(lane);
    }
    composerSampleJob.store(2, std::memory_order_release);
    return;
  }
  if (sampleRequestClip != composerSampleClip ||
      sampleRequestLane != composerSampleLane ||
      std::strcmp(sampleRequestPath.data(), composerSamplePath.data())) {
    fail("Sample request changed on Move; check the destination lane");
    return;
  }
  const auto state =
      SampleLoadState(sampleLoadState.load(std::memory_order_acquire));
  if (state == SampleLoadState::error) {
    fail(sampleLoadError.data());
    return;
  }
  if (composerSamplePreview) {
    if (state == SampleLoadState::previewing ||
        state == SampleLoadState::complete) {
      composerSampleProgress.store(1000);
      composerSampleJob.store(3, std::memory_order_release);
    }
    return;
  }
  if (state != SampleLoadState::complete) {
    composerSampleProgress.store(
        state == SampleLoadState::copying && sampleStageFrames
            ? int(sampleCopyFrame * 1000 / sampleStageFrames)
            : 0);
    return;
  }
  if (activeClip != composerSampleClip ||
      performanceMode() != composerSampleMode) {
    fail("Clip or mode changed while loading; check the lane");
    return;
  }
  if (composerSampleMarkers) {
    if (auto *clip = activeClipSlot())
      clip->sliceLanes[composerSampleLane].preRollMs = composerMarkerPreRoll;
  }
  if (composerSampleUseFile) {
    const int focus = splitFocus;
    const auto selected = selectedLane;
    if (splitEnabled)
      selectSplitPart(composerSampleDestinationPart(), false);
    setSourceParam("live_source", "0");
    if (splitEnabled)
      selectSplitPart(focus, false);
    selectedLane = selected;
  }
  composerPanelWaveVersion.fetch_add(1);
  composerSampleProgress.store(1000);
  composerSampleJob.store(3, std::memory_order_release);
}

void cleanupComposerSamplePreview() {
  if (composerSamplePreview && composerSampleUploaded)
    std::remove(composerSamplePath.data());
}
