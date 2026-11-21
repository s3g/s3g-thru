std::array<int,4> noiseBandGroups{};
std::bitset<16*s3g_thru_noise::fields> noiseOverrides{};
static bool
writeNoiseExtension(std::FILE *file,
                    const std::array<ClipSlot, kClipSlotCount> &clips) {
  if (!writeU32(file, 0x3153494eu))
    return false;
  for (const auto &c : clips)
    for (const auto &program : c.noisePrograms)
      for (const auto &patch : program.patches)
        for (int field=0;field<s3g_thru_noise::fields;++field)
          if (!writeF32(file, patch.v[field]))
            return false;
  return true;
}
static bool readNoiseExtension(std::FILE *file,
                               std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(file, magic) || magic != 0x3153494eu)
    return false;
  for (auto &c : clips)
    for (auto &program : c.noisePrograms)
      for (auto &patch : program.patches) {
        for (int field=0;field<s3g_thru_noise::fields;++field)
          if (!readF32(file, patch.v[field]))
            return false;
        if (!s3g_thru_noise::valid(patch))
          return false;
      }
  return true;
}
void captureNoisePrograms(ClipSlot &clip) const {
  clip.noisePrograms[0] =
      splitEnabled ? splitSingleControls.noise : controls.noise;
  for (int part = 0; part < 4; ++part)
    clip.noisePrograms[part + 1] = liveControlsForPart(part).noise;
}
void restoreNoisePrograms(const ClipSlot &clip) {
  splitSingleControls.noise = clip.noisePrograms[0];
  if (splitEnabled) {
    for (int part = 0; part < 4; ++part)
      splitParts[part].controls.noise = clip.noisePrograms[part + 1];
    controls.noise = splitParts[splitFocus].controls.noise;
  } else
    controls.noise = clip.noisePrograms[0];
}
bool setNoiseParam(const char *key, const char *text) {
  using namespace s3g_thru_noise;
  if(!std::strcmp(key,"noise_band_group")){noiseBandGroups[splitEnabled?splitFocus:0]=std::clamp(parseInteger(text,0),0,7);++midiRevision;return true;}
  const int field = index(key);
  if (field < 0)
    return false;
  const float value = parseFloat(text, NAN);
  if (!std::isfinite(value))
    return true;
  const int part = splitEnabled ? splitFocus : 0, lane = tableEditLane(part);
  auto &c = controls;
  auto cfg = c.noise.patches[lane];
  if (!set(cfg, field, value))
    return true;
  std::array<std::size_t, fields> replaced;
  replaced.fill(kClipCurveLaneCount);
  for (int f = 0; f < fields; ++f)
    if (f == field || cfg.v[f] != c.noise.patches[lane].v[f]) {
      const auto index = (part * 4 + lane) * fields + f;
      replaced[f] = kClipNoiseOffset + index;
      noiseOverrides.set(index);
    }
  if (sequenceMode() && !clipRecording)
    replaceSequenceAutomation(replaced, true);
  for (int f = 0; f < fields; ++f)
    if (cfg.v[f] != c.noise.patches[lane].v[f])
      captureClipPoint(ClipPointType::noiseParameter, f, part * 4 + lane,
                       cfg.v[f]);
  c.noise.patches[lane] = cfg;
  
  if (clipRecording)
    captureClipPoint(ClipPointType::noiseParameter, field, part * 4 + lane,
                     cfg.v[field]);
  if (auto *clip = activeClipSlot())
    captureNoisePrograms(*clip);
  markPersistentStateDirty();
  ++midiRevision;
  return true;
}
bool noiseParameterValue(const char *key, int part, float &value) const {
  if(!std::strcmp(key,"noise_band_group")){value=float(noiseBandGroups[std::clamp(part,0,3)]);return true;}
  const int field = s3g_thru_noise::index(key);
  if (field < 0)
    return false;
  value = s3g_thru_noise::resolved(liveControlsForPart(part).noise.patches[tableEditLane(part)],field);
  return true;
}
int formatNoiseState(char *out, int capacity) const {
  const int part = splitEnabled ? splitFocus : 0, edit = tableEditLane(part);
  const auto &program = liveControlsForPart(part).noise;
  if (liveControlsForPart(part).algorithm != Engine::PlaybackAlgorithm::noise)
    return std::snprintf(out, capacity, "{}");
  int at = std::snprintf(
      out, capacity, "{\"part\":%d,\"edit\":%d,\"revision\":%u,\"group\":%d,\"patches\":[",
      part, edit, unsigned(midiRevision),noiseBandGroups[part]);
  for (int lane = 0; lane < 4; ++lane) {
    if (at < 0 || at >= capacity)
      return -1;
    at += std::snprintf(out + at, capacity - at, "%s[", lane ? "," : "");
    for (int f = 0; f < s3g_thru_noise::fields; ++f) {
      if (at < 0 || at >= capacity)
        return -1;
      at += std::snprintf(out + at, capacity - at, "%s%.7g", f ? "," : "",
                          double(program.patches[lane].v[f]));
    }
    if (at < 0 || at >= capacity)
      return -1;
    at += std::snprintf(out + at, capacity - at, "]");
  }
  if (at < 0 || at >= capacity)
    return -1;
  at += std::snprintf(out + at, capacity - at, "]}");
  return at >= capacity ? -1 : at;
}

void seedNoiseRecording() {
  if (!clipRecording)
    return;
  const int count = splitEnabled ? partCount() : 1;
  for (int part = 0; part < count; ++part) {
    const auto &c = liveControlsForPart(part);
    bool used=c.algorithm==Engine::PlaybackAlgorithm::noise;
    const auto cfg=settingsForPart(part).engines;
    if(cfg.enabled&&cfg.configured)for(int entry=0;entry<engineSlotCount();++entry)used|=cfg.engines[entry]==int(Engine::PlaybackAlgorithm::noise);
    if(!used)continue;
    const int first = splitEnabled && !sharedDual ? firstLaneForPart(part) : 0,
              last =
                  splitEnabled && !sharedDual ? first + (isQuad() ? 0 : 1) : 3;
    for (int lane = first; lane <= last; ++lane)
      for (int field = 0; field < s3g_thru_noise::fields; ++field) {
        const auto curve = kClipNoiseOffset +
                           (part * 4 + lane) * s3g_thru_noise::fields + field;
        if (clipReplacing || !activeClipSlot()->clipPointCounts[curve])
          captureClipPoint(ClipPointType::noiseParameter, field,
                           part * 4 + lane, c.noise.patches[lane].v[field], 0,
                           true);
      }
  }
}
bool restoreNoiseJson(const char *json) {
  const char *at = std::strstr(json, "\"noise_programs\"");
  if (!at)
    return true;
  at = std::strchr(at, '[');
  if (!at)
    return false;
  std::array<s3g_thru_noise::Program, 5> programs{};
  for (auto &program : programs)
    for (auto &patch : program.patches)
      for (int f=0;f<s3g_thru_noise::fields;++f) {
        auto& x=patch.v[f];
        while (*at == '[' || *at == ',' || *at == ' ' || *at == '\n' ||
               *at == ']')
          ++at;
        char *end = nullptr;
        x = std::strtof(at, &end);
        if (end == at || !std::isfinite(x))
          return false;
        at = end;
      }
  while (*at == ' ' || *at == '\n')
    ++at;
  if (*at != ']')
    return false;
  for (const auto &program : programs)
    for (const auto &patch : program.patches)
      if (!s3g_thru_noise::valid(patch))
        return false;
  splitSingleControls.noise = programs[0];
  if (!splitEnabled)
    controls.noise = programs[0];
  for (int part = 0; part < 4; ++part)
    splitParts[part].controls.noise = programs[part + 1];
  if (splitEnabled)
    controls.noise = splitParts[splitFocus].controls.noise;
  return true;
}
