// TABLE programs are independent of the legacy 242 parameter identities.
std::array<s3g_thru_table::Scope, 4> tableScopes{};
std::bitset<16 * s3g_thru_table::fields> tableOverrides{};
std::array<std::array<std::array<float, kBlockFrames>, 4>, 4> tableScopeBus{};
int tableEditLane(int part) const {
  const int first = splitEnabled && !sharedDual ? firstLaneForPart(part) : 0,
            last = splitEnabled && !sharedDual ? first + (isQuad() ? 0 : 1) : 3;
  return std::clamp(int(selectedLane), first, last);
}
void captureTableScopes(std::size_t count) {
  for (int part = 0; part < 4; ++part){
    tableScopes[part].append(tableScopeBus[part], int(count));
    const PlaybackVoice* latest=nullptr;
    for(const auto& v:voices)if(v.active&&v.part==part&&v.engine.playbackAlgorithm()==Engine::PlaybackAlgorithm::scanned&&(!latest||v.age>latest->age))latest=&v;
    if(latest)for(int lane=0;lane<4;++lane)for(int i=0;i<64;++i)scannedProfiles[part][lane][i]=latest->engine.scannedProfile(lane,i);
  }
}
static bool
writeTableExtension(std::FILE *file,
                    const std::array<ClipSlot, kClipSlotCount> &clips) {
  if (!writeU32(file, 0x31424154u))
    return false;
  for (const auto &c : clips)
    for (const auto &program : c.tablePrograms)
      for (const auto &patch : program.patches)
        for (float x : patch.v)
          if (!writeF32(file, x))
            return false;
  return true;
}
static bool readTableExtension(std::FILE *file,
                               std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(file, magic) || magic != 0x31424154u)
    return false;
  for (auto &c : clips)
    for (auto &program : c.tablePrograms)
      for (auto &patch : program.patches) {
        for (auto &x : patch.v)
          if (!readF32(file, x))
            return false;
        if (!s3g_thru_table::valid(patch))
          return false;
      }
  return true;
}
void captureTablePrograms(ClipSlot &clip) const {
  captureScannedPrograms(clip);captureFmPrograms(clip);captureNoisePrograms(clip);captureResonPrograms(clip);captureLowformPrograms(clip);capturePartialsPrograms(clip);
  clip.tablePrograms[0] =
      splitEnabled ? splitSingleControls.table : controls.table;
  for (int part = 0; part < 4; ++part)
    clip.tablePrograms[part + 1] = liveControlsForPart(part).table;
}
void restoreTablePrograms(const ClipSlot &clip) {
  restoreScannedPrograms(clip);restoreFmPrograms(clip);restoreNoisePrograms(clip);restoreResonPrograms(clip);restoreLowformPrograms(clip);restorePartialsPrograms(clip);
  splitSingleControls.table = clip.tablePrograms[0];
  if (splitEnabled) {
    for (int part = 0; part < 4; ++part)
      splitParts[part].controls.table = clip.tablePrograms[part + 1];
    controls.table = splitParts[splitFocus].controls.table;
  } else
    controls.table = clip.tablePrograms[0];
}
bool setTableParam(const char *key, const char *text) {
  using namespace s3g_thru_table;
  const int field = index(key);
  if (field < 0)
    return false;
  const float value = parseFloat(text, NAN);
  if (!std::isfinite(value))
    return true;
  const int part = splitEnabled ? splitFocus : 0, lane = tableEditLane(part);
  auto &c = controls;
  auto cfg = c.table.patches[lane];
  if (!set(cfg, field, value))
    return true;
  if (field == model)
    start(cfg, int(cfg.v[model]));
  else if (field == type && cfg.v[type] != c.table.patches[lane].v[type])
    cfg.v[art] = cfg.v[type] == 2 ? 1.f : 0.f;
  std::array<std::size_t, fields> replaced;
  replaced.fill(kClipCurveLaneCount);
  for (int f = 0; f < fields; ++f)
    if (f == field || cfg.v[f] != c.table.patches[lane].v[f]) {
      const auto index = (part * 4 + lane) * fields + f;
      replaced[f] = kClipTableOffset + index;
      tableOverrides.set(index);
    }
  if (sequenceMode() && !clipRecording)
    replaceSequenceAutomation(replaced, true);
  for (int f = 0; f < fields; ++f)
    if (cfg.v[f] != c.table.patches[lane].v[f])
      captureClipPoint(ClipPointType::tableParameter, f, part * 4 + lane,
                       cfg.v[f]);
  c.table.patches[lane] = cfg;
  c.tableEditor = lane;
  if (clipRecording)
    captureClipPoint(ClipPointType::tableParameter, field, part * 4 + lane,
                     cfg.v[field]);
  if (auto *clip = activeClipSlot())
    captureTablePrograms(*clip);
  markPersistentStateDirty();
  ++midiRevision;
  return true;
}
bool tableParameterValue(const char *key, int part, float &value) const {
  const int field = s3g_thru_table::index(key);
  if (field < 0)
    return false;
  value = liveControlsForPart(part).table.patches[tableEditLane(part)].v[field];
  return true;
}
int formatTableState(char *out, int capacity) const {
  const int part = splitEnabled ? splitFocus : 0, edit = tableEditLane(part);
  const auto &program = liveControlsForPart(part).table;
  if (liveControlsForPart(part).algorithm != Engine::PlaybackAlgorithm::table)
    return std::snprintf(out, capacity, "{}");
  int at = std::snprintf(
      out, capacity, "{\"part\":%d,\"edit\":%d,\"revision\":%u,\"patches\":[",
      part, edit, unsigned(midiRevision));
  for (int lane = 0; lane < 4; ++lane) {
    if (at < 0 || at >= capacity)
      return -1;
    at += std::snprintf(out + at, capacity - at, "%s[", lane ? "," : "");
    for (int f = 0; f < s3g_thru_table::fields; ++f) {
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
int formatTableScopes(char *out, int capacity) const {
  int mask = 0;
  const int focus = bankMode ? 0 : splitEnabled && sharedDual ? splitFocus : -1;
  std::array<int, 4> owners{};
  for (int lane = 0; lane < 4; ++lane) {
    const int part =
        bankMode ? 0
                 : splitEnabled
                       ? (sharedDual ? splitFocus : isQuad() ? lane : lane / 2)
                       : 0;
    owners[lane] = part;
    if (Engine::sourceFree(liveControlsForPart(part).algorithm))
      mask |= 1 << lane;
  }
  int at = std::snprintf(out, capacity,
                         "{\"mode\":%d,\"focus\":%d,\"mask\":%d,\"waves\":[",
                         performanceMode(), focus, mask);
  for (int lane = 0; lane < 4; ++lane) {
    const int part = owners[lane];
    float hz = 440.f * std::exp2(float(settingsForPart(part).root - 69) / 12);
    const PlaybackVoice *last = nullptr;
    for (const auto &v : voices)
      if (v.active && v.part == part && (!last || v.age > last->age))
        last = &v;
    if (last)
      hz = last->engine.tableScopeFrequency(lane);
    else if(liveControlsForPart(part).algorithm==Engine::PlaybackAlgorithm::lowform)hz*=std::exp2(liveControlsForPart(part).lowform.patches[lane].v[s3g_thru_lowform::tune]/12);
    else if(liveControlsForPart(part).algorithm==Engine::PlaybackAlgorithm::reson)hz*=std::exp2(liveControlsForPart(part).reson.patches[lane].v[s3g_thru_reson::tune]/12);
    else if(liveControlsForPart(part).algorithm==Engine::PlaybackAlgorithm::partials)hz*=std::exp2(liveControlsForPart(part).partials.patches[lane].v[s3g_thru_partials::tune]/12);
    else if(liveControlsForPart(part).algorithm==Engine::PlaybackAlgorithm::noise)hz*=std::exp2(liveControlsForPart(part).noise.patches[lane].v[s3g_thru_noise::tune]/12);
    else if(liveControlsForPart(part).algorithm==Engine::PlaybackAlgorithm::fm)hz*=std::exp2(liveControlsForPart(part).fm.patches[lane].v[s3g_thru_fm::tune]/12);
    else
      hz *= std::exp2(liveControlsForPart(part)
                          .table.patches[lane]
                          .v[s3g_thru_table::tune] /
                      12);
    char wave[129]{};
    if(liveControlsForPart(part).algorithm==Engine::PlaybackAlgorithm::scanned){
      for(int i=0;i<64;++i){const int q=std::clamp(int(std::lround(scannedProfiles[part][lane][i]*7)), -7,7)+7;wave[i*2]=wave[i*2+1]="0123456789abcde"[q];}wave[128]=0;
    }else if (mask & (1 << lane))
      tableScopes[part].encode(lane, sampleRate, hz, wave);
    else {
      std::fill_n(wave, 128, '7');
      wave[128] = 0;
    }
    if (at < 0 || at >= capacity)
      return -1;
    at += std::snprintf(out + at, capacity - at, "%s\"%s\"", lane ? "," : "",
                        wave);
  }
  if (at < 0 || at >= capacity)
    return -1;
  at += std::snprintf(out + at, capacity - at, "]}");
  return at >= capacity ? -1 : at;
}

void seedTableRecording() {
  seedScannedRecording();seedFmRecording();seedNoiseRecording();seedResonRecording();seedLowformRecording();seedPartialsRecording();
  if (!clipRecording)
    return;
  const int count = splitEnabled ? partCount() : 1;
  for (int part = 0; part < count; ++part) {
    const auto &c = liveControlsForPart(part);
    if (c.algorithm != Engine::PlaybackAlgorithm::table)
      continue;
    const int first = splitEnabled && !sharedDual ? firstLaneForPart(part) : 0,
              last =
                  splitEnabled && !sharedDual ? first + (isQuad() ? 0 : 1) : 3;
    for (int lane = first; lane <= last; ++lane)
      for (int field = 0; field < s3g_thru_table::fields; ++field) {
        const auto curve = kClipTableOffset +
                           (part * 4 + lane) * s3g_thru_table::fields + field;
        if (clipReplacing || !activeClipSlot()->clipPointCounts[curve])
          captureClipPoint(ClipPointType::tableParameter, field,
                           part * 4 + lane, c.table.patches[lane].v[field], 0,
                           true);
      }
  }
}
bool restoreTableJson(const char *json) {
  const char *at = std::strstr(json, "\"table_programs\"");
  if (!at)
    return true;
  at = std::strchr(at, '[');
  if (!at)
    return false;
  std::array<s3g_thru_table::Program, 5> programs{};
  for (auto &program : programs)
    for (auto &patch : program.patches)
      for (auto &x : patch.v) {
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
      if (!s3g_thru_table::valid(patch))
        return false;
  splitSingleControls.table = programs[0];
  if (!splitEnabled)
    controls.table = programs[0];
  for (int part = 0; part < 4; ++part)
    splitParts[part].controls.table = programs[part + 1];
  if (splitEnabled)
    controls.table = splitParts[splitFocus].controls.table;
  return true;
}
