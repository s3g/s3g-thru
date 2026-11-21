std::bitset<16*s3g_thru_fm::fields> fmOverrides{};
static bool
writeFmExtension(std::FILE *file,
                    const std::array<ClipSlot, kClipSlotCount> &clips) {
  if (!writeU32(file, 0x31504d46u))
    return false;
  for (const auto &c : clips)
    for (const auto &program : c.fmPrograms)
      for (const auto &patch : program.patches)
        for (int field=0;field<s3g_thru_fm::legacyFields;++field)
          if (!writeF32(file, patch.v[field]))
            return false;
  return true;
}
static bool readFmExtension(std::FILE *file,
                               std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(file, magic) || magic != 0x31504d46u)
    return false;
  for (auto &c : clips)
    for (auto &program : c.fmPrograms)
      for (auto &patch : program.patches) {
        s3g_thru_fm::legacyExtras(patch);
        for (int field=0;field<s3g_thru_fm::legacyFields;++field)
          if (!readF32(file, patch.v[field]))
            return false;
        if (!s3g_thru_fm::valid(patch))
          return false;
      }
  return true;
}
static bool
writeFmExtraExtension(std::FILE *file,
                    const std::array<ClipSlot, kClipSlotCount> &clips) {
  if (!writeU32(file, 0x31584d46u))
    return false;
  for (const auto &c : clips)
    for (const auto &program : c.fmPrograms)
      for (const auto &patch : program.patches)
        for (int field=s3g_thru_fm::legacyFields;field<s3g_thru_fm::fields;++field)
          if (!writeF32(file, patch.v[field]))
            return false;
  return true;
}
static bool readFmExtraExtension(std::FILE *file,
                               std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(file, magic) || magic != 0x31584d46u)
    return false;
  for (auto &c : clips)
    for (auto &program : c.fmPrograms)
      for (auto &patch : program.patches) {
        for (int field=s3g_thru_fm::legacyFields;field<s3g_thru_fm::fields;++field)
          if (!readF32(file, patch.v[field]))
            return false;
        if (!s3g_thru_fm::valid(patch))
          return false;
      }
  return true;
}
void captureFmPrograms(ClipSlot &clip) const {
  clip.fmPrograms[0] =
      splitEnabled ? splitSingleControls.fm : controls.fm;
  for (int part = 0; part < 4; ++part)
    clip.fmPrograms[part + 1] = liveControlsForPart(part).fm;
}
void restoreFmPrograms(const ClipSlot &clip) {
  splitSingleControls.fm = clip.fmPrograms[0];
  if (splitEnabled) {
    for (int part = 0; part < 4; ++part)
      splitParts[part].controls.fm = clip.fmPrograms[part + 1];
    controls.fm = splitParts[splitFocus].controls.fm;
  } else
    controls.fm = clip.fmPrograms[0];
}
bool setFmParam(const char *key, const char *text) {
  using namespace s3g_thru_fm;
  if(!std::strcmp(key,"fm_start")){
    const int choice=parseInteger(text,0);if(choice<=0||choice>9)return true;
    if(controls.algorithm!=Engine::PlaybackAlgorithm::fm){setError("Select FM first");return true;}
    if(sequenceSoundEditing()){setError("Starting sound is a lane setting");return true;}
    const int part=splitEnabled?splitFocus:0,lane=tableEditLane(part);const auto before=controls.fm.patches[lane];const auto after=startingSound(choice,before);
    pushClipUndo(activeClip);std::array<std::size_t,fields> replaced{};
    for(int f=0;f<fields;++f){const auto at=(part*4+lane)*fields+f;fmOverrides.set(at);replaced[f]=kClipFmOffset+at;}
    if(sequenceMode()&&!clipRecording)replaceSequenceAutomation(replaced,true);
    controls.fm.patches[lane]=after;
    if(clipRecording)for(int f=0;f<fields;++f)if(after.v[f]!=before.v[f])captureClipPoint(ClipPointType::fmParameter,f,part*4+lane,after.v[f]);
    if(auto* clip=activeClipSlot())captureFmPrograms(*clip);
    markPersistentStateDirty();++midiRevision;clearError();return true;
  }
  const int field = index(key);
  if (field < 0)
    return false;
  const float value = parseFloat(text, NAN);
  if (!std::isfinite(value))
    return true;
  const int part = splitEnabled ? splitFocus : 0, lane = tableEditLane(part);
  auto &c = controls;
  auto cfg = c.fm.patches[lane];
  if (!set(cfg, field, value))
    return true;
  std::array<std::size_t, fields> replaced;
  replaced.fill(kClipCurveLaneCount);
  for (int f = 0; f < fields; ++f)
    if (f == field || cfg.v[f] != c.fm.patches[lane].v[f]) {
      const auto index = (part * 4 + lane) * fields + f;
      replaced[f] = kClipFmOffset + index;
      fmOverrides.set(index);
    }
  if (sequenceMode() && !clipRecording)
    replaceSequenceAutomation(replaced, true);
  for (int f = 0; f < fields; ++f)
    if (cfg.v[f] != c.fm.patches[lane].v[f])
      captureClipPoint(ClipPointType::fmParameter, f, part * 4 + lane,
                       cfg.v[f]);
  c.fm.patches[lane] = cfg;
  
  if (clipRecording)
    captureClipPoint(ClipPointType::fmParameter, field, part * 4 + lane,
                     cfg.v[field]);
  if (auto *clip = activeClipSlot())
    captureFmPrograms(*clip);
  markPersistentStateDirty();
  ++midiRevision;
  return true;
}
bool fmParameterValue(const char *key, int part, float &value) const {
  if(!std::strcmp(key,"fm_start")){value=0;return true;}
  const int field = s3g_thru_fm::index(key);
  if (field < 0)
    return false;
  value = liveControlsForPart(part).fm.patches[tableEditLane(part)].v[field];
  return true;
}
int formatFmState(char *out, int capacity) const {
  const int part = splitEnabled ? splitFocus : 0, edit = tableEditLane(part);
  const auto &program = liveControlsForPart(part).fm;
  if (liveControlsForPart(part).algorithm != Engine::PlaybackAlgorithm::fm)
    return std::snprintf(out, capacity, "{}");
  int at = std::snprintf(
      out, capacity, "{\"part\":%d,\"edit\":%d,\"revision\":%u,\"patches\":[",
      part, edit, unsigned(midiRevision));
  for (int lane = 0; lane < 4; ++lane) {
    if (at < 0 || at >= capacity)
      return -1;
    at += std::snprintf(out + at, capacity - at, "%s[", lane ? "," : "");
    for (int f = 0; f < s3g_thru_fm::fields; ++f) {
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

void seedFmRecording() {
  if (!clipRecording)
    return;
  const int count = splitEnabled ? partCount() : 1;
  for (int part = 0; part < count; ++part) {
    const auto &c = liveControlsForPart(part);
    bool used=c.algorithm==Engine::PlaybackAlgorithm::fm;
    const auto cfg=settingsForPart(part).engines;
    if(cfg.enabled&&cfg.configured)for(int entry=0;entry<engineSlotCount();++entry)used|=cfg.engines[entry]==int(Engine::PlaybackAlgorithm::fm);
    if(!used)continue;
    const int first = splitEnabled && !sharedDual ? firstLaneForPart(part) : 0,
              last =
                  splitEnabled && !sharedDual ? first + (isQuad() ? 0 : 1) : 3;
    for (int lane = first; lane <= last; ++lane)
      for (int field = 0; field < s3g_thru_fm::fields; ++field) {
        const auto curve = kClipFmOffset +
                           (part * 4 + lane) * s3g_thru_fm::fields + field;
        if (clipReplacing || !activeClipSlot()->clipPointCounts[curve])
          captureClipPoint(ClipPointType::fmParameter, field,
                           part * 4 + lane, c.fm.patches[lane].v[field], 0,
                           true);
      }
  }
}
bool restoreFmJson(const char *json) {
  const char *at = std::strstr(json, "\"fm_programs\"");
  if (!at)
    return true;
  at = std::strchr(at, '[');
  if (!at)
    return false;
  std::array<s3g_thru_fm::Program, 5> programs{};
  for(auto& program:programs)for(auto& patch:program.patches)s3g_thru_fm::legacyExtras(patch);
  for (auto &program : programs)
    for (auto &patch : program.patches)
      for (int f=0;f<s3g_thru_fm::legacyFields;++f) {
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
      if (!s3g_thru_fm::valid(patch))
        return false;
  splitSingleControls.fm = programs[0];
  if (!splitEnabled)
    controls.fm = programs[0];
  for (int part = 0; part < 4; ++part)
    splitParts[part].controls.fm = programs[part + 1];
  if (splitEnabled)
    controls.fm = splitParts[splitFocus].controls.fm;
  return true;
}


bool restoreFmExtraJson(const char *json) {
  const char *at = std::strstr(json, "\"fm_extra_programs\"");
  if (!at)
    return true;
  at = std::strchr(at, '[');
  if (!at)
    return false;
  std::array<s3g_thru_fm::Program, 5> programs{};programs[0]=splitEnabled?splitSingleControls.fm:controls.fm;for(int part=0;part<4;++part)programs[part+1]=splitParts[part].controls.fm;
  for (auto &program : programs)
    for (auto &patch : program.patches)
      for (int f=s3g_thru_fm::legacyFields;f<s3g_thru_fm::fields;++f) {
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
      if (!s3g_thru_fm::valid(patch))
        return false;
  splitSingleControls.fm = programs[0];
  if (!splitEnabled)
    controls.fm = programs[0];
  for (int part = 0; part < 4; ++part)
    splitParts[part].controls.fm = programs[part + 1];
  if (splitEnabled)
    controls.fm = splitParts[splitFocus].controls.fm;
  return true;
}

