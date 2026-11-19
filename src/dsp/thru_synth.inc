std::unique_ptr<s3g_thru_synth::Worker> synthWorker;
s3g_thru_synth::Recipe synthRecipe;
std::array<s3g_thru_synth::Recipe, s3g_thru_synth::generatorCount>
    synthWorkspaces{};
std::array<bool, s3g_thru_synth::generatorCount> synthWorkspaceValid{};
bool synthNameAutomatic = true, synthSourceEmbedded = false;
std::array<char, 768> synthSourcePath{};
std::array<char, 65> synthSourceLabel{};
std::uint64_t synthSourceEpoch = 1;
int synthSpectrum = 0, synthComponent = 0;
std::array<std::array<char, 768>, 2> synthConvPaths{};
std::array<std::array<char, 65>, 2> synthConvLabels{};
std::array<std::uint64_t, 2> synthConvEpochs{};
std::array<bool, 2> synthConvEmbedded{};
bool synthWorkerReady = false, synthSavedLoading = false;
bool synthOpened = false, synthInitialized = false, synthWantsRender = false,
     synthPreviewWanted = false;
std::uint64_t synthRevision = 1, synthDebounce = 0;
int synthActive = -1, synthFade = -1, synthRow = 0, synthPoint = 0;
std::size_t synthPosition = 0, synthOldPosition = 0, synthFadeRemaining = 0;
float synthPreviewGain = 0;
std::array<char, 512> synthDirectory{}, synthLastSaved{}, synthLoadPath{};
std::array<char, 41> synthName{};
std::array<char, 96> synthMessage{};
std::array<std::array<char, 768>, 4> synthHybridPaths{};
std::array<std::array<char, 65>, 4> synthHybridLabels{};
std::array<std::uint64_t, 4> synthHybridEpochs{};
std::array<bool, 4> synthHybridEmbedded{};
int synthHybridMissing() const {
  if (synthRecipe.type != s3g_thru_synth::hybrid)
    return 0;
  for (int i = 0; i < int(synthRecipe.hybridParams[s3g_thru_synth::hSources]);
       ++i)
    if (!synthHybridPaths[i][0])
      return i + 1;
  return 0;
}
int synthLoadLane = 0, synthLoadClip = -1;
void selectSynthType(int wanted) {
  using namespace s3g_thru_synth;
  if (wanted < 0 || wanted >= generatorCount || wanted == synthRecipe.type)
    return;
  synthWorkspaces[synthRecipe.type] = synthRecipe;
  synthWorkspaceValid[synthRecipe.type] = true;
  if (synthWorkspaceValid[wanted])
    synthRecipe = synthWorkspaces[wanted];
  else {
    Recipe next;
    initializeGenerator(next, wanted);
    for (int i = 0; i <= level; ++i)
      next.v[i] = synthRecipe.v[i];
    next.v[loop] = synthRecipe.v[loop];
    next.seed = synthRecipe.seed;
    next.scale = synthRecipe.scale;
    if (wanted == resynth || wanted == hybrid)
      next.v[width] = 1;
    if (wanted == convolution) {
      next.v[length] = 12;
      next.v[width] = 1;
      next.v[loop] = 0;
    }
    if (wanted == drum || wanted == dust) {
      next.v[length] = 2;
      next.v[attack] = 0;
      next.v[release] = 2;
      next.v[loop] = 0;
    }
    synthRecipe = next;
  }
  synthRow = 0;
  synthPoint = 0;
  synthSpectrum = int(synthRecipe.packets[bSpectrum]);
  synthComponent = 0;
  if (synthNameAutomatic)
    std::snprintf(synthName.data(), synthName.size(), "%s-%d",
                  generatorName(wanted), int(synthRecipe.v[root]));
  synthEdited();
  if (synthHybridMissing()) {
    synthWantsRender = false;
    synthPreviewWanted = false;
    std::snprintf(synthMessage.data(), synthMessage.size(),
                  "PICK HYBRID SOURCES");
  }
  if ((wanted == resynth && !synthSourcePath[0]) ||
      (wanted == convolution &&
       (!synthConvPaths[0][0] || !synthConvPaths[1][0]))) {
    synthWantsRender = false;
    synthPreviewWanted = false;
    std::snprintf(synthMessage.data(), synthMessage.size(),
                  wanted == convolution ? "PICK PRIMARY AND IR"
                                        : "USE LANE SOURCE");
  }
}
bool prepareSynth() {
  synthWorker.reset(new (std::nothrow) s3g_thru_synth::Worker);
  std::snprintf(synthDirectory.data(), synthDirectory.size(),
                "/data/UserData/UserLibrary/Samples/s3g Thru/Synths");
  std::snprintf(synthName.data(), synthName.size(), "Spline");
  synthWorkerReady = synthWorker && synthWorker->prepare();
  return synthWorkerReady;
}
void stopSynth() {
  synthWorkerReady = false;
  if (synthWorker)
    synthWorker->stop();
}
bool synthReady() const {
  return synthWorker && synthActive >= 0 &&
         synthWorker->audio[synthActive].revision == synthRevision;
}
void synthEdited() {
  ++synthRevision;
  synthWantsRender = true;
  synthMessage[0] = 0;
  synthDebounce = renderClockSamples.load(std::memory_order_relaxed) +
                  std::uint64_t(sampleRate * .2f);
  if (synthWorker) {
    const int job = synthWorker->state.load(std::memory_order_acquire);
    if (job != s3g_thru_synth::Worker::idle &&
        synthWorker->request.op != s3g_thru_synth::Worker::save)
      synthWorker->cancel.store(true, std::memory_order_relaxed);
  }
}
void synthSetName(const char *text) {
  std::size_t n = 0;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text);
       *p && n + 1 < synthName.size(); ++p)
    if ((std::isalnum(*p) && *p < 128) || *p == ' ' || *p == '-' || *p == '_')
      synthName[n++] = char(*p);
  synthName[n] = 0;
}
void serviceSynth() {
  using W = s3g_thru_synth::Worker;
  if (!synthWorker)
    return;
  auto &worker = *synthWorker;
  if (worker.state.load(std::memory_order_acquire) == W::complete) {
    const auto &job = worker.request;
    if (job.op == W::generate && worker.success && !worker.cancel.load() &&
        job.revision == synthRevision) {
      if (synthFadeRemaining)
        return; // Adopt on a later block, never retire a reader still fading.
      synthFade = synthPreviewGain > .0001f ? synthActive : -1;
      synthOldPosition = synthPosition;
      synthFadeRemaining =
          synthFade >= 0
              ? std::max<std::size_t>(1, std::size_t(sampleRate * .012f))
              : 0;
      synthActive = job.slot;
      synthPosition = 0;
      synthWantsRender = false;
      std::snprintf(synthMessage.data(), synthMessage.size(), "READY");
    } else if (job.op == W::save) {
      if (worker.success) {
        std::snprintf(synthLastSaved.data(), synthLastSaved.size(), "%s",
                      worker.savedPath.data());
        std::snprintf(synthMessage.data(), synthMessage.size(), "SAVED");
        if (job.load) {
          synthLoadLane = job.lane;
          synthLoadClip = job.clip;
          std::snprintf(synthLoadPath.data(), synthLoadPath.size(), "%s",
                        worker.savedPath.data());
          synthPreviewWanted = false;
        }
      } else
        std::snprintf(synthMessage.data(), synthMessage.size(), "%s",
                      worker.error.data());
    } else if (job.op == W::restore && worker.success &&
               job.revision == synthRevision) {
      synthWorkspaces[synthRecipe.type] = synthRecipe;
      synthWorkspaceValid[synthRecipe.type] = true;
      synthRecipe = worker.restored;
      synthWorkspaces[synthRecipe.type] = synthRecipe;
      synthWorkspaceValid[synthRecipe.type] = true;
      synthInitialized = true;
      synthNameAutomatic = false;
      if (synthRecipe.type == s3g_thru_synth::resynth) {
        synthSourcePath = job.source;
        synthSourceEmbedded = true;
        synthSourceEpoch = job.sourceEpoch;
        std::snprintf(synthSourceLabel.data(), synthSourceLabel.size(),
                      "EMBEDDED SOURCE");
      }
      if (synthRecipe.type == s3g_thru_synth::convolution)
        for (int i = 0; i < 2; ++i) {
          synthConvPaths[i] = job.source;
          synthConvEpochs[i] = job.sourceEpoch;
          synthConvEmbedded[i] = true;
          std::snprintf(synthConvLabels[i].data(), synthConvLabels[i].size(),
                        "EMBEDDED %s", i ? "IMPULSE" : "PRIMARY");
        }
      if (synthRecipe.type == s3g_thru_synth::hybrid)
        for (int i = 0;
             i < int(synthRecipe.hybridParams[s3g_thru_synth::hSources]); ++i) {
          synthHybridPaths[i] = job.source;
          synthHybridEpochs[i] = job.sourceEpoch;
          synthHybridEmbedded[i] = true;
          std::snprintf(synthHybridLabels[i].data(),
                        synthHybridLabels[i].size(), "EMBEDDED SOURCE %c",
                        char('A' + i));
        }
      synthRow = 0;
      synthPoint = 0;
      synthSpectrum = int(synthRecipe.packets[s3g_thru_synth::bSpectrum]);
      synthComponent = 0;
      const char *name = std::strrchr(job.source.data(), '/');
      std::array<char, 768> leaf{};
      std::snprintf(leaf.data(), leaf.size(), "%s",
                    name ? name + 1 : job.source.data());
      if (char *dot = std::strrchr(leaf.data(), '.'))
        *dot = 0;
      synthSetName(leaf.data());
      synthEdited();
      std::snprintf(synthMessage.data(), synthMessage.size(), "RECIPE OPENED");
    } else if (!worker.success && job.revision == synthRevision)
      std::snprintf(synthMessage.data(), synthMessage.size(), "%s",
                    worker.error.data());
    worker.state.store(W::idle, std::memory_order_release);
  }
  if (synthLoadPath[0]) {
    if (activeClip != synthLoadClip) {
      synthLoadPath[0] = 0;
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    "SAVED - CLIP CHANGED");
    } else if (!audioRecording() && !clipRecording &&
               !clipPreparationInFlight() && !sampleLoadBusy()) {
      requestLibrarySample(synthLoadLane, synthLoadPath.data());
      synthLoadPath[0] = 0;
      synthSavedLoading = sampleLoadState.load() != int(SampleLoadState::error);
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    synthSavedLoading ? "SAVED AND LOADING"
                                      : "SAVED - LOAD ERROR");
    }
  }
  if (synthSavedLoading) {
    const auto state = static_cast<SampleLoadState>(
        sampleLoadState.load(std::memory_order_acquire));
    if (std::strcmp(sampleRequestPath.data(), synthLastSaved.data()))
      synthSavedLoading = false;
    else if (state == SampleLoadState::error) {
      synthSavedLoading = false;
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    "SAVED - LOAD ERROR");
    } else if (state == SampleLoadState::complete) {
      synthSavedLoading = false;
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    "SAVED AND LOADED");
    }
  }
  if (!synthOpened || !synthWantsRender ||
      worker.state.load(std::memory_order_acquire) != W::idle ||
      renderClockSamples.load(std::memory_order_relaxed) < synthDebounce)
    return;
  if (synthHybridMissing()) {
    std::snprintf(synthMessage.data(), synthMessage.size(), "PICK SOURCE %c",
                  char('A' + synthHybridMissing() - 1));
    return;
  }
  if (synthRecipe.type == s3g_thru_synth::resynth && !synthSourcePath[0])
    return;
  if (synthRecipe.type == s3g_thru_synth::convolution &&
      (!synthConvPaths[0][0] || !synthConvPaths[1][0]))
    return;
  int slot = 0;
  while (slot == synthActive || slot == synthFade)
    ++slot;
  if (slot >= 3)
    return;
  W::Request job;
  job.op = W::generate;
  job.recipe = synthRecipe;
  job.revision = synthRevision;
  job.slot = slot;
  job.rate = sampleRate;
  job.source = synthSourcePath;
  job.embeddedSource = synthSourceEmbedded;
  job.sourceEpoch = synthSourceEpoch;
  if (synthRecipe.type == s3g_thru_synth::convolution) {
    job.source = synthConvPaths[0];
    job.impulse = synthConvPaths[1];
    job.sourceEpoch = synthConvEpochs[0];
    job.impulseEpoch = synthConvEpochs[1];
    job.embeddedSource = synthConvEmbedded[0];
    job.embeddedImpulse = synthConvEmbedded[1];
  }
  if (synthRecipe.type == s3g_thru_synth::hybrid) {
    job.hybridPaths = synthHybridPaths;
    job.hybridEpochs = synthHybridEpochs;
    job.hybridEmbedded = synthHybridEmbedded;
  }
  if (worker.post(job))
    synthWantsRender = false;
}
void mixSynth(std::size_t count) {
  if (!synthWorker || synthActive < 0)
    return;
  const auto read = [&](int slot, std::size_t &position, float &l, float &rr) {
    if (slot < 0)
      return;
    const auto &a = synthWorker->audio[slot];
    if (a.left.empty())
      return;
    if (position >= a.left.size()) {
      if (a.recipe.v[s3g_thru_synth::loop] > .5f)
        position = 0;
      else
        return;
    }
    l = a.left[position];
    rr = a.right[position];
    ++position;
  };
  const float step = 1.f / std::max(1.f, sampleRate * .008f);
  for (std::size_t i = 0; i < count; ++i) {
    const float target = synthPreviewWanted ? 1.f : 0.f;
    synthPreviewGain += std::clamp(target - synthPreviewGain, -step, step);
    float l = 0, rr = 0;
    if (synthPreviewGain > 0 || synthPreviewWanted)
      read(synthActive, synthPosition, l, rr);
    if (synthFadeRemaining) {
      float oldL = 0, oldR = 0;
      read(synthFade, synthOldPosition, oldL, oldR);
      float t =
          1 - float(synthFadeRemaining) / std::max(1.f, sampleRate * .012f);
      t = t * t * (3 - 2 * t);
      l = oldL + (l - oldL) * t;
      rr = oldR + (rr - oldR) * t;
      if (--synthFadeRemaining == 0)
        synthFade = -1;
    }
    previewLevelState += (previewLevel - previewLevelState) * gainControlSlew;
    directPreview[0][i] += l * previewLevelState * synthPreviewGain;
    directPreview[1][i] += rr * previewLevelState * synthPreviewGain;
    const auto &a = synthWorker->audio[synthActive];
    if (synthPosition >= a.left.size() &&
        a.recipe.v[s3g_thru_synth::loop] < .5f)
      synthPreviewWanted = false;
  }
}
bool setSynthParam(const char *key, const char *text) {
  if (std::strncmp(key, "synth_", 6))
    return false;
  using namespace s3g_thru_synth;
  using W = s3g_thru_synth::Worker;
  if (!std::strcmp(key, "synth_close")) {
    synthOpened = false;
    synthPreviewWanted = false;
    synthWantsRender = false;
    if (synthWorker && synthWorker->request.op != W::save)
      synthWorker->cancel.store(true);
    return true;
  }
  if (!synthWorkerReady) {
    setError("Synthesis worker unavailable");
    return true;
  }
  if (!std::strcmp(key, "synth_open")) {
    if (audioRecording() || clipRecording || bankRecordSlot >= 0 ||
        clipPreparationInFlight() || projectLocked()) {
      setError("Finish recording or transition first");
      return true;
    }
    if (!synthInitialized) {
      const auto settings = settingsForPart(splitEnabled ? splitFocus : 0);
      synthRecipe.v[root] = float(settings.root);
      synthRecipe.scale = 0;
      for (std::size_t i = 0;
           i < kScaleDefinitions[std::size_t(settings.scale)].count; ++i)
        synthRecipe.scale |= std::uint16_t(
            1u << kScaleDefinitions[std::size_t(settings.scale)].intervals[i]);
      synthInitialized = true;
      std::snprintf(synthName.data(), synthName.size(), "Spline-%d",
                    settings.root);
    }
    sequencePads = 0;
    sequencePadConsumed.fill(true);
    sequenceEditStep = -1;
    stopSequenceAudition();
    cancelPerformanceGestures();
    stopPreviewWithFade();
    synthOpened = true;
    if (!synthReady()) {
      synthWantsRender = true;
      synthDebounce = 0;
    }
    clearError();
    return true;
  }
  if (!synthOpened)
    return true;
  if (!std::strcmp(key, "synth_preview")) {
    synthPreviewWanted = parseInteger(text, 0) != 0;
    if (synthPreviewWanted && synthHybridMissing()) {
      synthPreviewWanted = false;
      setError("Pick Hybrid source files first");
      return true;
    }
    if (synthPreviewWanted &&
        ((synthRecipe.type == resynth && !synthSourcePath[0]) ||
         (synthRecipe.type == convolution &&
          (!synthConvPaths[0][0] || !synthConvPaths[1][0])))) {
      synthPreviewWanted = false;
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    synthRecipe.type == convolution ? "PICK PRIMARY AND IR"
                                                    : "USE LANE SOURCE");
      return true;
    }
    if (synthPreviewWanted) {
      stopPreviewWithFade();
      if (synthActive >= 0 && synthPreviewGain > .0001f) {
        synthFade = synthActive;
        synthOldPosition = synthPosition;
        synthFadeRemaining =
            std::max<std::size_t>(1, std::size_t(sampleRate * .012f));
      }
      synthPosition = 0;
    }
    return true;
  }
  if (!std::strcmp(key, "synth_name")) {
    synthNameAutomatic = false;
    synthSetName(text);
    return true;
  }
  if (!std::strcmp(key, "synth_cancel")) {
    synthWorker->cancel.store(true);
    synthWantsRender = false;
    std::snprintf(synthMessage.data(), synthMessage.size(), "CANCELLED");
    return true;
  }
  if (!std::strcmp(key, "synth_render")) {
    synthWantsRender = true;
    synthDebounce = 0;
    return true;
  }
  if (!std::strcmp(key, "synth_save") || !std::strcmp(key, "synth_restore")) {
    if (synthWorker->state.load(std::memory_order_acquire) != W::idle) {
      setError("Wait for synthesis job");
      return true;
    }
    W::Request job;
    job.revision = synthRevision;
    job.rate = sampleRate;
    job.lane = int(selectedLane);
    job.clip = activeClip;
    job.sourceEpoch = ++synthSourceEpoch;
    if (!std::strcmp(key, "synth_save")) {
      if (!synthReady()) {
        setError("Render current settings before saving");
        return true;
      }
      job.op = W::save;
      job.recipe = synthRecipe; // Capture current protection without changing
                                // preview audio.
      job.slot = synthActive;
      job.load = parseInteger(text, 0) > 0;
      std::snprintf(job.directory.data(), job.directory.size(), "%s",
                    synthDirectory.data());
      std::snprintf(job.name.data(), job.name.size(), "%s", synthName.data());
    } else {
      job.op = W::restore;
      std::snprintf(job.source.data(), job.source.size(), "%s",
                    currentLaneSourcePaths[selectedLane].data());
    }
    clearError();
    synthWorker->post(job);
    return true;
  }
  if (!std::strcmp(key, "synth_type")) {
    selectSynthType(parseInteger(text, synthRecipe.type));
    return true;
  }
  if (!std::strcmp(key, "synth_primary_file") ||
      !std::strcmp(key, "synth_impulse_file")) {
    const int which = !std::strcmp(key, "synth_impulse_file") ? 1 : 0;
    if (synthRecipe.type != convolution || text[0] != '/' ||
        std::strlen(text) >= synthConvPaths[which].size()) {
      setError("Choose an absolute WAV path");
      return true;
    }
    std::snprintf(synthConvPaths[which].data(), synthConvPaths[which].size(),
                  "%s", text);
    synthConvEmbedded[which] = false;
    synthConvEpochs[which] = ++synthSourceEpoch;
    const char *leaf = std::strrchr(text, '/');
    leaf = leaf ? leaf + 1 : text;
    std::size_t n = 0;
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(leaf);
         *c && n + 1 < synthConvLabels[which].size(); ++c)
      if (*c >= 32 && *c < 127 && *c != '"' && *c != '\\')
        synthConvLabels[which][n++] = char(*c);
    synthConvLabels[which][n] = 0;
    clearError();
    synthEdited();
    return true;
  }
  if (!std::strncmp(key, "synth_hybrid", 12) && key[12] >= '0' &&
      key[12] <= '3' && !std::strcmp(key + 13, "_file")) {
    const int index = key[12] - '0';
    if (synthRecipe.type != hybrid || text[0] != '/' ||
        std::strlen(text) >= synthHybridPaths[index].size()) {
      setError("Choose an absolute WAV path");
      return true;
    }
    std::snprintf(synthHybridPaths[index].data(),
                  synthHybridPaths[index].size(), "%s", text);
    synthHybridEpochs[index] = ++synthSourceEpoch;
    synthHybridEmbedded[index] = false;
    const char *leaf = std::strrchr(text, '/');
    leaf = leaf ? leaf + 1 : text;
    std::size_t at = 0;
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(leaf);
         *c && at + 1 < synthHybridLabels[index].size(); ++c)
      if (*c >= 32 && *c < 127 && *c != '"' && *c != '\\')
        synthHybridLabels[index][at++] = char(*c);
    synthHybridLabels[index][at] = 0;
    clearError();
    synthEdited();
    return true;
  }
  if (!std::strcmp(key, "synth_work_lock")) {
    const int bit = parseInteger(text, -1);
    if (bit >= 0 && (bit < 45 || (bit >= 48 && bit < 60)))
      synthRecipe.workshopLocks ^= 1ULL << bit;
    return true;
  }
  if ((!std::strcmp(key, "synth_pluck") || !std::strcmp(key, "synth_sub") ||
       !std::strcmp(key, "synth_hybrid") || !std::strcmp(key, "synth_trace") ||
       !std::strcmp(key, "synth_events") || !std::strcmp(key, "synth_timing") ||
       !std::strcmp(key, "synth_attack") || !std::strcmp(key, "synth_color"))) {
    int field = -1;
    float value = 0;
    char extra = 0;
    const int group =
        !std::strcmp(key, "synth_pluck")
            ? 0
            : !std::strcmp(key, "synth_sub")
                  ? 1
                  : !std::strcmp(key, "synth_hybrid")
                        ? 2
                        : !std::strcmp(key, "synth_trace")
                              ? 3
                              : !std::strcmp(key, "synth_events")
                                    ? 4
                                    : !std::strcmp(key, "synth_timing") ? 5 : !std::strcmp(key, "synth_attack") ? 6 : 7;
    const bool relevant =
        (group == 0 && synthRecipe.type == pluck) ||
        (group == 1 && synthRecipe.type == subharmonic) ||
        (group == 2 && synthRecipe.type == hybrid) ||
        (group == 3 && synthRecipe.type == resynth) ||
        ((group == 4 || group == 5) && supportsEvents(synthRecipe.type)) ||
        ((group == 6 || group == 7) && synthRecipe.type == drum);
    if (relevant &&
        std::sscanf(text, "%d,%f %c", &field, &value, &extra) == 2) {
      const auto old = workshopGroup(synthRecipe, group);
      if (setWorkshop(synthRecipe, group, field, value) &&
          old != workshopGroup(synthRecipe, group))
        synthEdited();
    }
    clearError();
    return true;
  }
  if (!std::strcmp(key, "synth_extended_lock")) {
    const int bit = parseInteger(text, -1);
    if (bit >= 0 && bit < 24)
      synthRecipe.extendedLocks ^= 1u << bit;
    return true;
  }
  if (!std::strcmp(key, "synth_drum_start")) {
    if (synthRecipe.type == drum && parseInteger(text, 0) > 0) {
      initializeDrumModel(synthRecipe, int(synthRecipe.drums[dModel]));
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_clean")) {
    if (parseInteger(text, 0) > 0) {
      clean(synthRecipe);
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_source")) {
    const auto &path = currentLaneSourcePaths[selectedLane];
    if (audioRecording() || !path[0]) {
      setError("Finish saving a lane sample first");
      return true;
    }
    synthSourcePath = path;
    synthSourceEmbedded = false;
    ++synthSourceEpoch;
    const char *leaf = std::strrchr(path.data(), '/');
    leaf = leaf ? leaf + 1 : path.data();
    std::size_t at = 0;
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(leaf);
         *c && at + 1 < synthSourceLabel.size(); ++c)
      if (*c >= 32 && *c < 127 && *c != '"' && *c != '\\')
        synthSourceLabel[at++] = char(*c);
    synthSourceLabel[at] = 0;
    if (!at)
      std::snprintf(synthSourceLabel.data(), synthSourceLabel.size(),
                    "LANE %d SOURCE", int(selectedLane) + 1);
    if (synthRecipe.type == resynth)
      synthEdited();
    clearError();
    return true;
  }
  if(!std::strcmp(key,"synth_dust_lock")){
    const int bit=parseInteger(text,-1);if(bit>=0 && bit<dustFields) synthRecipe.dustLocks ^= 1u<<bit;
    return true;
  }
  int f = -1;
  float value = 0;
  char extra = 0;
  if (!std::strcmp(key, "synth_edit") &&
      std::sscanf(text, "%d,%f %c", &f, &value, &extra) == 2) {
    if (f == fields && std::isfinite(value)) {
      const auto seed =
          std::uint32_t(std::lround(std::clamp(value, 0.f, 65535.f)));
      if (seed == synthRecipe.seed)
        return true;
      synthRecipe.seed = seed;
    } else {
      if (f < 0 || f >= fields)
        return true;
      const float before = synthRecipe.v[f];
      if (!set(synthRecipe, f, value) || before == synthRecipe.v[f])
        return true;
    }
    synthEdited();
    return true;
  }
  if ((!std::strcmp(key, "synth_wave") || !std::strcmp(key, "synth_bloom") ||
       !std::strcmp(key, "synth_mod") || !std::strcmp(key, "synth_resynth") ||
       !std::strcmp(key, "synth_additive") ||
       !std::strcmp(key, "synth_noise") || !std::strcmp(key, "synth_fm") ||
       !std::strcmp(key, "synth_conv") || !std::strcmp(key, "synth_drum") || !std::strcmp(key,"synth_dust")) &&
      std::sscanf(text, "%d,%f %c", &f, &value, &extra) == 2) {
    bool changed = false;
    if (!std::strcmp(key, "synth_wave") && synthRecipe.type == wave) {
      const auto before = synthRecipe.osc;
      changed = setWave(synthRecipe, f, value) && before != synthRecipe.osc;
    }
    if (!std::strcmp(key, "synth_bloom") && synthRecipe.type == bloom) {
      const auto before = synthRecipe.packets;
      changed =
          setBloom(synthRecipe, f, value) && before != synthRecipe.packets;
      if (f == bSpectrum)
        synthSpectrum = int(synthRecipe.packets[bSpectrum]);
    }
    if (!std::strcmp(key, "synth_mod")) {
      const auto before = synthRecipe.mod;
      changed = setMod(synthRecipe, f, value) && before != synthRecipe.mod;
    }
    if (!std::strcmp(key, "synth_resynth") && synthRecipe.type == resynth) {
      const auto before = synthRecipe.spectral;
      changed =
          setResynth(synthRecipe, f, value) && before != synthRecipe.spectral;
    }
    if (!std::strcmp(key, "synth_additive") &&
        synthRecipe.type == partialsType) {
      const auto before = synthRecipe.additive;
      changed =
          setAdditive(synthRecipe, f, value) && before != synthRecipe.additive;
    }
    if (!std::strcmp(key, "synth_noise") && synthRecipe.type == noiseBank) {
      const auto before = synthRecipe.noise;
      changed = setNoise(synthRecipe, f, value) && before != synthRecipe.noise;
    }
    if (!std::strcmp(key, "synth_fm") && synthRecipe.type == fm) {
      const auto before = synthRecipe.fmOsc;
      changed = setFm(synthRecipe, f, value) && before != synthRecipe.fmOsc;
    }
    if (!std::strcmp(key, "synth_conv") && synthRecipe.type == convolution) {
      const auto before = synthRecipe.conv;
      changed =
          setConvolution(synthRecipe, f, value) && before != synthRecipe.conv;
    }
    if (!std::strcmp(key, "synth_drum") && synthRecipe.type == drum) {
      const auto before = synthRecipe.drums;
      changed = setDrum(synthRecipe, f, value) && before != synthRecipe.drums;
    }
    if(!std::strcmp(key,"synth_dust") && synthRecipe.type==dust){const auto before=synthRecipe.dustParams; changed=setDust(synthRecipe,f,value) && before!=synthRecipe.dustParams;}
    if (changed)
      synthEdited();
    return true;
  }
  if (!std::strcmp(key, "synth_detail_lock")) {
    f = parseInteger(text, -1);
    if (f >= 0 && f < detailLockCount)
      synthRecipe.detailLocks ^= 1ULL << f;
    return true;
  }
  int spectrum = -1, component = -1, partField = -1;
  if (!std::strcmp(key, "synth_spec_select") &&
      (synthRecipe.type == bloom || synthRecipe.type == partialsType ||
       synthRecipe.type == noiseBank || synthRecipe.type == subharmonic) &&
      std::sscanf(text, "%d,%d %c", &spectrum, &component, &extra) == 2 &&
      spectrum >= 0 && spectrum < 4 && component >= 0 && component < 8) {
    synthSpectrum = spectrum;
    synthComponent = component;
    if (synthRecipe.type == bloom && synthRecipe.packets[bMode] == 0 &&
        synthRecipe.packets[bSpectrum] != spectrum) {
      setBloom(synthRecipe, bSpectrum, float(spectrum));
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_spec_edit") &&
      (synthRecipe.type == bloom || synthRecipe.type == partialsType ||
       synthRecipe.type == noiseBank || synthRecipe.type == subharmonic) &&
      std::sscanf(text, "%d,%d,%d,%f %c", &spectrum, &component, &partField,
                  &value, &extra) == 4) {
    if (setPartial(synthRecipe, spectrum, component, partField, value))
      synthEdited();
    return true;
  }
  if (!std::strcmp(key, "synth_seed_next")) {
    synthRecipe.seed = (synthRecipe.seed + 1) % 65536;
    synthEdited();
    return true;
  }
  if (!std::strcmp(key, "synth_mutate")) {
    value = parseFloat(text, 0);
    if (value > 0 && std::isfinite(value)) {
      mutate(synthRecipe, value);
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_lock")) {
    f = parseInteger(text, -1);
    if (f >= 0 && f < fields)
      synthRecipe.locks ^= 1u << f;
    return true;
  }
  int row = -1, at = -1, field = -1;
  if ((!std::strcmp(key, "synth_point") || !std::strcmp(key, "synth_select") ||
       !std::strcmp(key, "synth_delete")) &&
      std::sscanf(text, "%d,%d %c", &row, &at, &extra) == 2 && row >= 0 &&
      row < 4 && at >= 0 && at < 8) {
    synthRow = row;
    synthPoint = at;
    auto &c = synthRecipe.curves[row];
    if (!std::strcmp(key, "synth_delete")) {
      c.enabled &= ~(1u << at);
      c.enabled |= 0x81;
      c.p[at].value = 0;
      synthEdited();
    } else if (!std::strcmp(key, "synth_point") && !(c.enabled & (1u << at))) {
      c.enabled |= 1u << at;
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_point_edit") &&
      std::sscanf(text, "%d,%d,%d,%f %c", &row, &at, &field, &value, &extra) ==
          4) {
    if (editPoint(synthRecipe, row, at, field, value))
      synthEdited();
    return true;
  }
  float low = 0, high = 0, amount = 0;
  int ends = 0;
  if (!std::strcmp(key, "synth_curve_random") &&
      std::sscanf(text, "%d,%f,%f,%f,%d %c", &row, &low, &high, &amount, &ends,
                  &extra) == 5 &&
      std::isfinite(low) && std::isfinite(high) && std::isfinite(amount) &&
      amount > 0 && row >= 0 && row < 4) {
    randomCurve(synthRecipe, row, low, high, std::clamp(amount, 0.f, 1.f),
                ends != 0);
    synthEdited();
    return true;
  }
  return true;
}
int formatSynthSpectrum(int row, char *out, int capacity) const {
  if (row < 0 || row > 3 || !out || capacity < 129)
    return -1;
  constexpr char hex[] = "0123456789abcdef";
  std::size_t at = 0;
  for (int i = 0; i < 8; ++i)
    for (float value :
         {synthRecipe.ratios[row][i], synthRecipe.partials[row][i]}) {
      std::uint32_t bits;
      std::memcpy(&bits, &value, 4);
      for (int j = 0; j < 4; ++j) {
        const auto b = std::uint8_t(bits >> (j * 8));
        out[at++] = hex[b >> 4];
        out[at++] = hex[b & 15];
      }
    }
  out[at] = 0;
  return int(at);
}
int formatSynth(char *out, int capacity) const {
  if (!out || capacity < 1)
    return -1;
  std::array<char, s3g_thru_synth::recipeBytes * 2 + 1> model{};
  std::size_t at = 0;
  constexpr char hex[] = "0123456789abcdef";
  s3g_thru_synth::serializeRecipe(synthRecipe, [&](std::uint8_t b) {
    model[at++] = hex[b >> 4];
    model[at++] = hex[b & 15];
  });
  const int count = std::snprintf(
      out, capacity,
      "{\"open\":%d,\"job\":%d,\"op\":%d,\"progress\":%u,\"ready\":%d,"
      "\"preview\":%d,\"rev\":%"
      "llu,\"row\":%d,\"point\":%d,\"spectrum\":%d,\"component\":%d,\"name\":"
      "\"%s\",\"message\":"
      "\"%s\",\"source\":\"%s\",\"primary\":\"%s\",\"impulse\":\"%s\","
      "\"inputs\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"eventfit\":%d,\"seconds\":%."
      "5f,\"recipe\":\"%"
      "s\",\"wave\":\"%s\"}",
      synthOpened ? 1 : 0,
      synthWorker ? synthWorker->state.load(std::memory_order_acquire) : -1,
      synthWorker ? int(synthWorker->request.op) : 0,
      synthWorker ? synthWorker->progress.load() : 0, synthReady() ? 1 : 0,
      synthPreviewWanted ? 1 : 0,
      static_cast<unsigned long long>(synthRevision), synthRow, synthPoint,
      synthSpectrum, synthComponent, synthName.data(), synthMessage.data(),
      synthSourceLabel.data(), synthConvLabels[0].data(),
      synthConvLabels[1].data(), synthHybridLabels[0].data(),
      synthHybridLabels[1].data(), synthHybridLabels[2].data(),
      synthHybridLabels[3].data(),
      s3g_thru_synth::eventFitCount(synthRecipe, sampleRate),
      synthActive >= 0 &&
              synthWorker->audio[synthActive].recipe.type == synthRecipe.type
          ? double(synthWorker->audio[synthActive].left.size()) / sampleRate
          : 0.0,
      model.data(),
      synthActive >= 0 &&
              synthWorker->audio[synthActive].recipe.type == synthRecipe.type
          ? synthWorker->audio[synthActive].waveText.data()
          : "");
  return count >= capacity ? -1 : count;
}
