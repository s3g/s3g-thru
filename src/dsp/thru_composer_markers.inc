// Only fixed source identity/length fields are captured in the serialized
// callback. WAV parsing, metadata writing and durable publication use the
// worker.
std::array<char, kPersistencePathSize> composerMarkerSource{};
std::size_t composerMarkerFrames = 0;
float composerMarkerRate = 0, composerMarkerPreRoll = -1;
std::uint64_t composerMarkerWave = 0;
std::string composerMarkerAsset, composerMarkerName;
bool composerMarkerAudition = false;
ComposerJson composerMarkerDestination;
std::vector<s3g_thru_wave::Marker> composerMarkerPoints;
void composerPrepareMarkers(const ComposerJson &request) {
  using namespace s3g_thru_pack;
  keys(request, "markers",
       {"clip", "part", "lane", "mode", "asset", "wave", "points", "name",
        "folder", "audition", "start", "end"});
  composerSampleClip = composerSampleInteger(request, "clip", 0, 63);
  composerSamplePart = composerSampleInteger(request, "part", 0, 3);
  composerSampleLane = composerSampleInteger(request, "lane", 0, 3);
  composerSampleMode = composerSampleInteger(request, "mode", 0, 6);
  composerMarkerAsset = request.at("asset").string();
  need(composerMarkerAsset.size() == 24 && safeId(composerMarkerAsset),
       "markers", "invalid source identity");
  const double wave = request.at("wave").number();
  need(wave >= 0 && wave <= 9007199254740991. && wave == std::floor(wave),
       "markers", "invalid waveform identity");
  composerMarkerWave = std::uint64_t(wave);
  composerMarkerAudition =
      request.find("audition") && request.at("audition").boolean();
  composerSamplePreviewLow = 0;
  composerSamplePreviewHigh = 1;
  if (composerMarkerAudition) {
    const double low = request.at("start").number(),
                 high = request.at("end").number();
    need(std::isfinite(low) && std::isfinite(high) && low >= 0 && high <= 1 &&
             low < high,
         "markers", "invalid audition slice");
    composerSamplePreviewLow = low;
    composerSamplePreviewHigh = high;
  } else {
    composerMarkerName = request.at("name").string();
    need(composerMarkerName.size() > 4 && composerMarkerName.size() <= 180 &&
             composerWavName(composerMarkerName) &&
             composerMarkerName.front() != '.' &&
             composerMarkerName.find('/') == std::string::npos &&
             composerMarkerName.find('\\') == std::string::npos,
         "markers", "choose a WAV filename");
    for (unsigned char c : composerMarkerName)
      need(c >= 32 && c != 127, "markers", "invalid filename");
    const auto &points = request.at("points").arrayValue();
    need(!points.empty() && points.size() <= 64, "markers",
         "use 1 to 64 slice starts");
    composerMarkerPoints.clear();
    double previous = -1;
    for (const auto &p : points) {
      keys(p, "marker", {"position", "label"});
      const double at = p.at("position").number();
      const auto label = p.at("label").string();
      need(std::isfinite(at) && at >= 0 && at < 1 && at > previous &&
               label.size() <= 23,
           "markers",
           "markers must be ordered and labels at most 23 characters");
      for (unsigned char c : label)
        need(c >= 32 && c < 127, "markers",
             "marker labels use plain text characters");
      composerMarkerPoints.push_back({at, label});
      previous = at;
    }
    need(composerMarkerPoints.front().position == 0, "markers",
         "the first marker must remain at zero");
    composerMarkerDestination = ComposerJson::object();
    if (const auto *folder = request.find("folder"))
      composerMarkerDestination["folder"] = *folder;
  }
  composerSampleMarkers = !composerMarkerAudition;
  composerSampleUseFile = false;
  composerSamplePreview = composerMarkerAudition;
  composerSampleUploaded = false;
  composerSampleProgress.store(0);
  composerSampleError[0] = 0;
  composerSampleGeneration = composerPanelOwnerGeneration.load();
  composerSampleRevision = composerLiveRevision.load();
  composerSampleJob.store(5, std::memory_order_release);
  composerPhase.store(composerSampleLoading, std::memory_order_release);
}
void captureComposerMarkerSource() {
  const auto lane = std::size_t(composerSampleLane);
  if (composerMarkerWave != composerPanelWaveVersion.load() ||
      !engine.laneLoaded(lane) || !currentLaneSourcePaths[lane][0]) {
    std::snprintf(composerSampleError.data(), composerSampleError.size(),
                  "Source changed or is still saving; reopen WAV markers");
    composerSampleJob.store(4, std::memory_order_release);
    return;
  }
  composerMarkerSource = currentLaneSourcePaths[lane];
  composerMarkerPreRoll = activeClipSlot()->sliceLanes[lane].preRollMs;
  composerMarkerFrames = engine.laneFrames(lane);
  composerMarkerRate = engine.laneSampleRate(lane);
  composerSampleJob.store(6, std::memory_order_release);
}
void processComposerMarkerWrite() {
  try {
    using s3g_thru_pack::need;
    const std::string source = composerMarkerSource.data();
    need(s3g_thru_pack::sha256(source).substr(0, 24) == composerMarkerAsset,
         "markers", "source changed; reopen WAV markers");
    if (composerMarkerAudition) {
      std::snprintf(composerSamplePath.data(), composerSamplePath.size(), "%s",
                    source.c_str());
      composerSampleJob.store(1, std::memory_order_release);
      return;
    }
    const auto folder = composerUploadDirectory(composerMarkerDestination);
    auto pattern = folder + "/.thru-markers-XXXXXX";
    std::vector<char> path(pattern.begin(), pattern.end());
    path.push_back(0);
    const int fd = mkstemp(path.data());
    need(fd >= 0, "markers", "cannot create marked WAV");
    const std::string temporary = path.data();
    struct Cleanup {
      const std::string &p;
      ~Cleanup() { std::remove(p.c_str()); }
    } cleanup{temporary};
    if (::fchmod(fd, 0644) != 0) {
      ::close(fd);
      throw std::runtime_error("Cannot set marked WAV permissions");
    }
    std::unique_ptr<std::FILE, int (*)(std::FILE *)> input(
        std::fopen(source.c_str(), "rb"), std::fclose),
        output(fdopen(fd, "wb"), std::fclose);
    if (!output) {
      ::close(fd);
      throw std::runtime_error("Cannot open marked WAV");
    }
    need(bool(input), "markers", "source WAV is unavailable");
    s3g_thru_wave::write(input.get(), output.get(), composerMarkerPoints,
                         composerMarkerFrames, composerMarkerRate);
    need(s3g_thru_storage::closeFile(output.release(), temporary.c_str()),
         "markers", "cannot finish marked WAV");
    const auto target =
        composerStoreNamedFile(temporary, folder, composerMarkerName, true);
    std::snprintf(composerSamplePath.data(), composerSamplePath.size(), "%s",
                  target.c_str());
    composerSampleJob.store(1, std::memory_order_release);
  } catch (const std::exception &e) {
    std::snprintf(composerSampleError.data(), composerSampleError.size(), "%s",
                  e.what());
    composerSampleJob.store(4, std::memory_order_release);
  }
}
