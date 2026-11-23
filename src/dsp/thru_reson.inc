std::bitset<16 * s3g_thru_reson::fields> resonOverrides{};
std::bitset<4> resonDensityOverrides{};
static bool writeResonDensityExtension(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips){
  if(!writeU32(file,0x314e4452u))return false;
  for(const auto& clip:clips)for(const auto& program:clip.resonPrograms)if(!writeF32(file,program.density))return false;
  return true;
}
static bool readResonDensityExtension(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips){
  std::uint32_t magic=0;if(!readU32(file,magic)||magic!=0x314e4452u)return false;
  for(auto& clip:clips)for(auto& program:clip.resonPrograms)if(!readF32(file,program.density)||!std::isfinite(program.density)||program.density<0||program.density>100)return false;
  return true;
}
static bool
writeResonExtension(std::FILE *file,
                    const std::array<ClipSlot, kClipSlotCount> &clips) {
  if (!writeU32(file, 0x31505352u))
    return false;
  for (const auto &c : clips)
    for (const auto &program : c.resonPrograms)
      for (const auto &patch : program.patches)
        for (int field = 0; field < s3g_thru_reson::fields; ++field)
          if (!writeF32(file, patch.v[field]))
            return false;
  return true;
}
static bool readResonExtension(std::FILE *file,
                               std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(file, magic) || magic != 0x31505352u)
    return false;
  for (auto &c : clips)
    for (auto &program : c.resonPrograms)
      for (auto &patch : program.patches) {
        for (int field = 0; field < s3g_thru_reson::fields; ++field)
          if (!readF32(file, patch.v[field]))
            return false;
        if (!s3g_thru_reson::valid(patch))
          return false;
      }
  return true;
}
void captureResonPrograms(ClipSlot &clip) const {
  clip.resonPrograms[0] =
      splitEnabled ? splitSingleControls.reson : controls.reson;
  for (int part = 0; part < 4; ++part)
    clip.resonPrograms[part + 1] = liveControlsForPart(part).reson;
}
void restoreResonPrograms(const ClipSlot &clip) {
  splitSingleControls.reson = clip.resonPrograms[0];
  if (splitEnabled) {
    for (int part = 0; part < 4; ++part)
      splitParts[part].controls.reson = clip.resonPrograms[part + 1];
    controls.reson = splitParts[splitFocus].controls.reson;
  } else
    controls.reson = clip.resonPrograms[0];
}
bool setResonParam(const char *key, const char *text) {
  using namespace s3g_thru_reson;
  if(!std::strcmp(key,"reson_density")){
    const float value=parseFloat(text,NAN);if(!std::isfinite(value))return true;
    const int part=splitEnabled?splitFocus:0;
    if(sequenceSoundEditing()){setError("DENS is a part setting");return true;}
    if(sequenceMode()&&!clipRecording)replaceSequenceAutomation(std::array<std::size_t,1>{kClipResonDensityOffset+std::size_t(part)},true);
    controls.reson.density=std::clamp(value,0.f,100.f);resonDensityOverrides.set(part);
    captureClipPoint(ClipPointType::resonDensity,0,part,controls.reson.density);
    if(auto* clip=activeClipSlot())captureResonPrograms(*clip);
    markPersistentStateDirty();++midiRevision;return true;
  }
  if (!std::strcmp(key, "reson_start")) {
    const int choice = parseInteger(text, 0);
    if (choice <= 0 || choice > 8)
      return true;
    if (controls.algorithm != Engine::PlaybackAlgorithm::reson) {
      setError("Select RESON first");
      return true;
    }
    if (sequenceSoundEditing()) {
      setError("Starting sound is a lane setting");
      return true;
    }
    const int part = splitEnabled ? splitFocus : 0, lane = tableEditLane(part);
    const auto before = controls.reson.patches[lane],
               after = startingSound(choice, before);
    pushClipUndo(activeClip);
    std::array<std::size_t, fields> replaced{};
    for (int f = 0; f < fields; ++f) {
      const auto at = (part * 4 + lane) * fields + f;
      resonOverrides.set(at);
      replaced[f] = kClipResonOffset + at;
    }
    if (sequenceMode() && !clipRecording)
      replaceSequenceAutomation(replaced, true);
    controls.reson.patches[lane] = after;
    if (clipRecording)
      for (int f = 0; f < fields; ++f)
        if (before.v[f] != after.v[f])
          captureClipPoint(ClipPointType::resonParameter, f, part * 4 + lane,
                           after.v[f]);
    if (auto *clip = activeClipSlot())
      captureResonPrograms(*clip);
    markPersistentStateDirty();
    ++midiRevision;
    clearError();
    return true;
  }
  const int field = index(key);
  if (field < 0)
    return false;
  const float value = parseFloat(text, NAN);
  if (!std::isfinite(value))
    return true;
  const int part = splitEnabled ? splitFocus : 0, lane = tableEditLane(part);
  auto &c = controls;
  auto cfg = c.reson.patches[lane];
  if (!set(cfg, field, value))
    return true;
  std::array<std::size_t, fields> replaced;
  replaced.fill(kClipCurveLaneCount);
  for (int f = 0; f < fields; ++f)
    if (f == field || cfg.v[f] != c.reson.patches[lane].v[f]) {
      const auto index = (part * 4 + lane) * fields + f;
      replaced[f] = kClipResonOffset + index;
      resonOverrides.set(index);
    }
  if (sequenceMode() && !clipRecording)
    replaceSequenceAutomation(replaced, true);
  for (int f = 0; f < fields; ++f)
    if (cfg.v[f] != c.reson.patches[lane].v[f])
      captureClipPoint(ClipPointType::resonParameter, f, part * 4 + lane,
                       cfg.v[f]);
  c.reson.patches[lane] = cfg;

  if (clipRecording)
    captureClipPoint(ClipPointType::resonParameter, field, part * 4 + lane,
                     cfg.v[field]);
  if (auto *clip = activeClipSlot())
    captureResonPrograms(*clip);
  markPersistentStateDirty();
  ++midiRevision;
  return true;
}
bool resonParameterValue(const char *key, int part, float &value) const {
  if(!std::strcmp(key,"reson_density")){value=liveControlsForPart(part).reson.density;return true;}
  if (!std::strcmp(key, "reson_start")) {
    value = 0;
    return true;
  }
  const int field = s3g_thru_reson::index(key);
  if (field < 0)
    return false;
  value = liveControlsForPart(part).reson.patches[tableEditLane(part)].v[field];
  return true;
}
int formatResonState(char *out, int capacity) const {
  const int part = splitEnabled ? splitFocus : 0, edit = tableEditLane(part);
  const auto &program = liveControlsForPart(part).reson;
  if (liveControlsForPart(part).algorithm != Engine::PlaybackAlgorithm::reson)
    return std::snprintf(out, capacity, "{}");
  int at = std::snprintf(
      out, capacity, "{\"part\":%d,\"edit\":%d,\"revision\":%u,\"density\":%.7g,\"patches\":[",
      part, edit, unsigned(midiRevision),double(program.density));
  for (int lane = 0; lane < 4; ++lane) {
    if (at < 0 || at >= capacity)
      return -1;
    at += std::snprintf(out + at, capacity - at, "%s[", lane ? "," : "");
    for (int f = 0; f < s3g_thru_reson::fields; ++f) {
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

void seedResonRecording() {
  if (!clipRecording)
    return;
  const int count = splitEnabled ? partCount() : 1;
  for (int part = 0; part < count; ++part) {
    const auto &c = liveControlsForPart(part);
    bool used = c.algorithm == Engine::PlaybackAlgorithm::reson;
    const auto cfg = settingsForPart(part).engines;
    if (cfg.enabled && cfg.configured)
      for (int entry = 0; entry < engineSlotCount(); ++entry)
        used |= cfg.engines[entry] == int(Engine::PlaybackAlgorithm::reson);
    if (!used)
      continue;
    if(clipReplacing||!activeClipSlot()->clipPointCounts[kClipResonDensityOffset+part])
      captureClipPoint(ClipPointType::resonDensity,0,part,c.reson.density,0,true);
    const int first = splitEnabled && !sharedDual ? firstLaneForPart(part) : 0,
              last =
                  splitEnabled && !sharedDual ? first + (isQuad() ? 0 : 1) : 3;
    for (int lane = first; lane <= last; ++lane)
      for (int field = 0; field < s3g_thru_reson::fields; ++field) {
        const auto curve = kClipResonOffset +
                           (part * 4 + lane) * s3g_thru_reson::fields + field;
        if (clipReplacing || !activeClipSlot()->clipPointCounts[curve])
          captureClipPoint(ClipPointType::resonParameter, field,
                           part * 4 + lane, c.reson.patches[lane].v[field], 0,
                           true);
      }
  }
}
bool restoreResonJson(const char *json) {
  const char *at = std::strstr(json, "\"reson_programs\"");
  if (!at)
    return true;
  at = std::strchr(at, '[');
  if (!at)
    return false;
  std::array<s3g_thru_reson::Program, 5> programs{};
  for (auto &program : programs)
    for (auto &patch : program.patches)
      for (int f = 0; f < s3g_thru_reson::fields; ++f) {
        auto &x = patch.v[f];
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
      if (!s3g_thru_reson::valid(patch))
        return false;
  const char* densities = std::strstr(json, "\"reson_density_values\"");
  if (densities) {
    densities = std::strchr(densities, '[');
    if (!densities) return false;
    ++densities;
    const auto whitespace = [&]() {
      while (*densities == ' ' || *densities == '\n' || *densities == '\r' || *densities == '\t') {
        ++densities;
      }
    };
    for (int group = 0; group < 5; ++group) {
      char* end = nullptr;
      const float value = std::strtof(densities, &end);
      if (end == densities || !std::isfinite(value) || value < 0 || value > 100) return false;
      programs[group].density = value;
      densities = end;
      whitespace();
      if (group < 4) {
        if (*densities != ',') return false;
        ++densities;
      }
    }
    whitespace();
    if (*densities != ']') return false;
  }
  splitSingleControls.reson = programs[0];
  if (!splitEnabled)
    controls.reson = programs[0];
  for (int part = 0; part < 4; ++part)
    splitParts[part].controls.reson = programs[part + 1];
  if (splitEnabled)
    controls.reson = splitParts[splitFocus].controls.reson;
  return true;
}
