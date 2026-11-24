std::array<s3g_thru_source::Memory, 4> liveMemories;
std::bitset<4 * s3g_thru_source::fields> sourceMotionOverrides;
std::atomic<int> liveCaptureJob{0};
int liveCapturePart = 0, liveCaptureLane = 0, liveCaptureClip = 0;
s3g_thru_source::View liveCaptureView{};
std::array<char, kPersistencePathSize> liveCapturePath{};
float liveCaptureGain = 1;
static bool sourceMotionEngine(Engine::PlaybackAlgorithm a) {
  return Engine::supportsLiveMemory(a);
}
void captureSourcePrograms(ClipSlot &clip) const {
  clip.sourcePrograms[0] =
      (splitEnabled ? splitSingleControls : controls).sourceMotion;
  for (int p = 0; p < 4; ++p)
    clip.sourcePrograms[p + 1] = liveControlsForPart(p).sourceMotion;
}
void restoreSourcePrograms(const ClipSlot &clip) {
  splitSingleControls.sourceMotion = clip.sourcePrograms[0];
  for (int p = 0; p < 4; ++p)
    splitParts[p].controls.sourceMotion = clip.sourcePrograms[p + 1];
  controls.sourceMotion =
      clip.sourcePrograms[splitEnabled ? splitFocus + 1 : 0];
}
static bool
writeSourcePrograms(std::FILE *file,
                    const std::array<ClipSlot, kClipSlotCount> &clips) {
  if (!writeU32(file, 0x31564c54u))
    return false;
  for (const auto &clip : clips)
    for (const auto &cfg : clip.sourcePrograms)
      for (float x : cfg.v)
        if (!writeF32(file, x))
          return false;
  return true;
}
static bool readSourcePrograms(std::FILE *file,
                               std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(file, magic) || magic != 0x31564c54u)
    return false;
  for (auto &clip : clips)
    for (auto &cfg : clip.sourcePrograms) {
      for (auto &x : cfg.v)
        if (!readF32(file, x))
          return false;
      if (!s3g_thru_source::valid(cfg))
        return false;
    }
  return true;
}
const s3g_thru_source::Config &sourceProgram(int part) const {
  return (clipPlaying ? (splitEnabled ? splitClipControls[part] : clipControls)
                      : liveControlsForPart(part))
      .sourceMotion;
}
bool validLiveRoute(int part, int source) const {
  if (source <= 1)
    return true;
  const int count = splitEnabled ? partCount() : 1, origin = source - 2;
  if (origin < 0 || origin >= count || origin == part)
    return false;
  int walk = origin;
  for (int n = 0; n < 4; ++n) {
    if (walk == part)
      return false;
    const int tap = int(sourceProgram(walk).v[8]);
    if (tap < 2)
      return true;
    walk = tap - 2;
    if (walk < 0 || walk >= count)
      return false;
  }
  return false;
}
bool setSourceParam(const char *key, const char *text) {
  using namespace s3g_thru_source;
  if (!std::strcmp(key, "live_capture")) {
    if (parseInteger(text, 0))
      beginLiveCapture();
    return true;
  }
  const int field = index(key);
  if (field < 0)
    return false;
  const int part = splitEnabled ? splitFocus : 0;
  const float value = parseFloat(text, NAN);
  if (!std::isfinite(value))
    return true;
  if (field >= 8 && liveCaptureJob.load(std::memory_order_acquire) &&
      liveCapturePart == part) {
    setError("Finish Live Memory capture first");
    return true;
  }
  auto next = controls.sourceMotion;
  if (!set(next, field, value))
    return true;
  if (field == 8 && !validLiveRoute(part, int(next.v[8]))) {
    setError("Choose input or an available part without feedback");
    return true;
  }
  const auto curve = kClipSourceOffset + part * fields + field;
  sourceMotionOverrides.set(part * fields + field);
  if (sequenceMode() && !clipRecording)
    replaceSequenceAutomation(std::array<std::size_t, 1>{curve}, true);
  controls.sourceMotion = next;
  if (clipRecording)
    captureClipPoint(ClipPointType::sourceParameter, field, part,
                     next.v[field]);
  if (auto *clip = activeClipSlot())
    captureSourcePrograms(*clip);
  markPersistentStateDirty();
  ++midiRevision;
  clearError();
  return true;
}
bool sourceParameterValue(const char *key, int part, float &value) const {
  const int field = s3g_thru_source::index(key);
  if (field < 0)
    return false;
  value = liveControlsForPart(part).sourceMotion.v[field];
  return true;
}
void prepareLiveInput(std::size_t count) {
  for (int part = 0; part < (splitEnabled ? partCount() : 1); ++part) {
    auto &memory = liveMemories[part];
    const auto &cfg = sourceProgram(part);
    const bool capturing = liveCaptureJob.load(std::memory_order_acquire) &&
                           liveCapturePart == part;
    if (!capturing)
      memory.source(validLiveRoute(part, int(cfg.v[8])) ? int(cfg.v[8]) : 0);
    if (memory.source() == 1)
      memory.write(
          monitorInput[0].data(), monitorInput[1].data(), count,
          (cfg.v[11] != 0 && memory.view(cfg, sampleRate).anchor >= 0) ||
              capturing || patternRenderOnly);
  }
}
void rememberLiveParts(std::size_t count) {
  for (int part = 0; part < (splitEnabled ? partCount() : 1); ++part) {
    auto &memory = liveMemories[part];
    const auto &cfg = sourceProgram(part);
    const int source = memory.source() - 2;
    const bool capturing = liveCaptureJob.load(std::memory_order_acquire) &&
                           liveCapturePart == part;
    if (splitEnabled && source >= 0 && source < partCount())
      memory.write(
          splitBus[source][0].data(), splitBus[source][1].data(), count,
          (cfg.v[11] != 0 && memory.view(cfg, sampleRate).anchor >= 0) ||
              capturing);
  }
}
void prepareLiveVoice(PlaybackVoice &voice, Engine::Controls &cfg) {
  cfg.travelReleased = !voice.held;
  cfg.liveSource = {};
  if (sourceMotionEngine(cfg.algorithm) && cfg.sourceMotion.v[8] != 0) {
    cfg.liveSource =
        liveMemories[voice.part].view(cfg.sourceMotion, sampleRate);
    const float lane =
        splitEnabled && !sharedDual ? float(firstLaneForPart(voice.part)) : 0.f;
    cfg.stackFirstLane = cfg.stackLastLane = cfg.manualLane = lane;
    cfg.laneScanPaused = true;
    for (auto &gain : cfg.laneGains)
      gain = 1;
  }
}
int formatSourceState(char *out, int size) const {
  const int focus=splitEnabled?splitFocus:0,count=splitEnabled?partCount():1;
  bool any=false;
  for(int part=0;part<count;++part)any|=sourceProgram(part).v[8]!=0;
  if(!any)return std::snprintf(out,size,"{\"active\":0,\"clip\":%d,\"mode\":%d}",activeClip,performanceMode());
  int at=0;
  const auto append=[&](const char* format,auto... args){
    if(at<0||at>=size)return;
    const int n=std::snprintf(out+at,size-at,format,args...);
    if(n<0||n>=size-at)at=size;else at+=n;
  };
  const auto partState=[&](int part,bool detail){
    const auto& c=sourceProgram(part);
    const auto& own=clipPlaying?(splitEnabled?splitClipControls[part]:clipControls):liveControlsForPart(part);
    const auto view=liveMemories[part].view(c,sampleRate);
    append("{\"part\":%d,\"available\":%d,\"source\":%d,\"filling\":%d,\"window\":%.0f,\"generation\":%u,\"wave\":[",part,
           sourceMotionEngine(own.algorithm),int(c.v[8]),view.anchor<0,
           double(view.frames)*1000/sampleRate,view.generation);
    const auto wave=liveMemories[part].waveform(view);
    for(int i=0;i<64;++i)append(i?",%d":"%d",wave[i]);
    append("%s","]");
    if(!detail) {
      const auto envelope=liveMemories[part].envelope(view);
      append(",\"envelope\":\"%s\"",envelope.data());
    }
    if(detail){
      append(",\"written\":%.0f,\"retention\":%.0f,\"capture\":%d,\"v\":[",double(liveMemories[part].written())*1000/sampleRate,
             double(view.safeFrames)*1000/sampleRate,liveCaptureJob.load());
      for(int f=0;f<s3g_thru_source::fields;++f)append(f?",%.9g":"%.9g",double(c.v[f]));
      append("%s","]");
    }
    append("%s","}");
  };
  partState(focus,true);
  if(at<1||at>=size)return -1;
  --at;append(",\"active\":1,\"clip\":%d,\"mode\":%d,\"parts\":[",activeClip,performanceMode());
  for(int part=0;part<count;++part){
    if(part)append("%s",",");
    const auto& own=clipPlaying?(splitEnabled?splitClipControls[part]:clipControls):liveControlsForPart(part);
    if(sourceProgram(part).v[8]!=0&&sourceMotionEngine(own.algorithm))partState(part,false);
    else append("%s","null");
  }
  append("%s","]}");
  return at>=size?-1:at;
}

void beginLiveCapture() {
  const int part = splitEnabled ? splitFocus : 0;
  if (liveCaptureJob.load() || audioRecording() || clipRecording ||
      patternBusy() || clipPreparationInFlight() || previewActive) {
    setError("Finish recording, preview or clip change first");
    return;
  }
  const auto &cfg = sourceProgram(part);
  const auto view = liveMemories[part].view(cfg, sampleRate);
  if (!view || view.anchor < 0) {
    setError("Wait for Live Memory to fill");
    return;
  }
  if (!backgroundStarted ||
      !allocateCapturePath(activeClip, selectedLane, "live",
                           liveCapturePath.data(), liveCapturePath.size())) {
    setError("Live capture storage unavailable");
    return;
  }
  setSourceParam("live_freeze", "1");
  liveCapturePart = part;
  liveCaptureLane = int(selectedLane);
  liveCaptureClip = activeClip;
  liveCaptureView = view;
  liveCaptureGain = cfg.v[12];
  liveCaptureJob.store(1, std::memory_order_release);
}
bool processLiveCapture() {
  if (liveCaptureJob.load(std::memory_order_acquire) != 1)
    return false;
  liveCaptureJob.store(2, std::memory_order_release);
  const auto view = liveCaptureView;
  std::vector<float> left(view.frames), right(view.frames);
  const std::size_t edge =
      std::min<std::size_t>(view.frames / 2, std::size_t(sampleRate * .005f));
  for (std::size_t i = 0; i < view.frames; ++i) {
    const float fade = edge ? std::min({1.f, float(i) / edge,
                                        float(view.frames - 1 - i) / edge})
                            : 1.f;
    left[i] = view.sample(0, view.anchor + i) * liveCaptureGain * fade;
    right[i] = view.sample(1, view.anchor + i) * liveCaptureGain * fade;
  }
  const std::string path = liveCapturePath.data();
  const bool ok = writeStereoWavFile(left.data(), right.data(), view.frames,
                                     int(sampleRate), path.c_str(),
                                     (path + ".tmp").c_str(), true);
  liveCaptureJob.store(ok ? 3 : 4, std::memory_order_release);
  return true;
}
void serviceLiveCapture() {
  const int state = liveCaptureJob.load(std::memory_order_acquire);
  if (state == 5) {
    const auto load = static_cast<SampleLoadState>(
        sampleLoadState.load(std::memory_order_acquire));
    if (load != SampleLoadState::complete && load != SampleLoadState::error)
      return;
    liveCaptureJob.store(0, std::memory_order_release);
    if (load == SampleLoadState::error ||
        std::strcmp(currentLaneSourcePaths[liveCaptureLane].data(),
                    liveCapturePath.data())) {
      setError("Live capture saved; sample load failed");
      return;
    }
    if (activeClip == liveCaptureClip) {
      auto &cfg = splitEnabled && splitFocus != liveCapturePart
                      ? splitParts[liveCapturePart].controls.sourceMotion
                      : controls.sourceMotion;
      cfg.v[8] = 0;
      sourceMotionOverrides.set(liveCapturePart * s3g_thru_source::fields + 8);
      captureActiveClip(true);
      markPersistentStateDirty();
    }
    return;
  }
  if (state != 3 && state != 4)
    return;
  liveCaptureJob.store(0, std::memory_order_release);
  if (state == 4) {
    setError("Live capture save failed; memory remains frozen");
    return;
  }
  if (activeClip != liveCaptureClip) {
    setError("Live capture saved; clip changed before loading");
    return;
  }
  requestLibrarySample(liveCaptureLane, liveCapturePath.data());
  const auto load = static_cast<SampleLoadState>(
      sampleLoadState.load(std::memory_order_acquire));
  if (load < SampleLoadState::requested || load > SampleLoadState::copying ||
      std::strcmp(sampleRequestPath.data(), liveCapturePath.data())) {
    setError("Live capture saved; load it from the library");
    return;
  }
  liveCaptureJob.store(5, std::memory_order_release);
  // Retain the frozen source until the ordinary loader has published the file.
  captureActiveClip(true);
  markPersistentStateDirty();
  clearError();
}

bool readSourceJson(const char *json) {
  const auto *at = std::strstr(json, "\"source_motion_v1\"");
  if (!at) {
    splitSingleControls.sourceMotion = {};
    for (auto &part : splitParts)
      part.controls.sourceMotion = {};
    controls.sourceMotion = {};
    return true;
  }
  at = std::strchr(at, '[');
  if (!at)
    return false;
  ++at;
  std::array<s3g_thru_source::Config, 5> configs{};
  for (auto &c : configs)
    for (auto &x : c.v) {
      while (*at == ' ' || *at == ',' || *at == '\n')
        ++at;
      char *end = nullptr;
      x = std::strtof(at, &end);
      if (end == at)
        return false;
      at = end;
    }
  for (const auto &c : configs)
    if (!s3g_thru_source::valid(c))
      return false;
  while (*at == ' ' || *at == '\n')
    ++at;
  if (*at != ']')
    return false;
  splitSingleControls.sourceMotion = configs[0];
  for (int p = 0; p < 4; ++p)
    splitParts[p].controls.sourceMotion = configs[p + 1];
  controls.sourceMotion = configs[splitEnabled ? splitFocus + 1 : 0];
  return true;
}

void seedSourceRecording() {
  if (!clipRecording)
    return;
  for (int part = 0; part < (splitEnabled ? partCount() : 1); ++part) {
    const auto &own = liveControlsForPart(part);
    const auto &c = own.sourceMotion;
    if (!sourceMotionEngine(own.algorithm) && c.v[0] == 0 && c.v[8] == 0)
      continue;
    for (int f = 0; f < s3g_thru_source::fields; ++f)
      if (clipReplacing ||
          !activeClipSlot()
               ->clipPointCounts[kClipSourceOffset +
                                 part * s3g_thru_source::fields + f])
        captureClipPoint(ClipPointType::sourceParameter, f, part, c.v[f], 0,
                         true);
  }
}
