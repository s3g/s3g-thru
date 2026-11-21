std::array<int, 4> partialsBandGroups{};
std::bitset<16 * s3g_thru_partials::fields> partialsOverrides{};
static bool
writePartialsExtension(std::FILE *file,
                       const std::array<ClipSlot, kClipSlotCount> &clips) {
  if (!writeU32(file, 0x31505450u))
    return false;
  for (const auto &c : clips)
    for (const auto &program : c.partialsPrograms)
      for (const auto &patch : program.patches)
        for (int field = 0; field < s3g_thru_partials::fields; ++field)
          if (!writeF32(file, patch.v[field]))
            return false;
  return true;
}
static bool readPartialsExtension(std::FILE *file,
                                  std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(file, magic) || magic != 0x31505450u)
    return false;
  for (auto &c : clips)
    for (auto &program : c.partialsPrograms)
      for (auto &patch : program.patches) {
        for (int field = 0; field < s3g_thru_partials::fields; ++field)
          if (!readF32(file, patch.v[field]))
            return false;
        if (!s3g_thru_partials::valid(patch))
          return false;
      }
  return true;
}
void capturePartialsPrograms(ClipSlot &clip) const {
  clip.partialsPrograms[0] =
      splitEnabled ? splitSingleControls.partials : controls.partials;
  for (int part = 0; part < 4; ++part)
    clip.partialsPrograms[part + 1] = liveControlsForPart(part).partials;
}
void restorePartialsPrograms(const ClipSlot &clip) {
  splitSingleControls.partials = clip.partialsPrograms[0];
  if (splitEnabled) {
    for (int part = 0; part < 4; ++part)
      splitParts[part].controls.partials = clip.partialsPrograms[part + 1];
    controls.partials = splitParts[splitFocus].controls.partials;
  } else
    controls.partials = clip.partialsPrograms[0];
}
bool setPartialsParam(const char *key, const char *text) {
  using namespace s3g_thru_partials;
  if (!std::strcmp(key, "partials_band_group")) {
    partialsBandGroups[splitEnabled ? splitFocus : 0] =
        std::clamp(parseInteger(text, 0), 0, 7);
    ++midiRevision;
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
  auto cfg = c.partials.patches[lane];
  if (!set(cfg, field, value))
    return true;
  std::array<std::size_t, fields> replaced;
  replaced.fill(kClipCurveLaneCount);
  for (int f = 0; f < fields; ++f)
    if (f == field || cfg.v[f] != c.partials.patches[lane].v[f]) {
      const auto index = (part * 4 + lane) * fields + f;
      replaced[f] = kClipPartialsOffset + index;
      partialsOverrides.set(index);
    }
  if (sequenceMode() && !clipRecording)
    replaceSequenceAutomation(replaced, true);
  for (int f = 0; f < fields; ++f)
    if (cfg.v[f] != c.partials.patches[lane].v[f])
      captureClipPoint(ClipPointType::partialsParameter, f, part * 4 + lane,
                       cfg.v[f]);
  c.partials.patches[lane] = cfg;

  if (clipRecording)
    captureClipPoint(ClipPointType::partialsParameter, field, part * 4 + lane,
                     cfg.v[field]);
  if (auto *clip = activeClipSlot())
    capturePartialsPrograms(*clip);
  markPersistentStateDirty();
  ++midiRevision;
  return true;
}
bool partialsParameterValue(const char *key, int part, float &value) const {
  if (!std::strcmp(key, "partials_band_group")) {
    value = float(partialsBandGroups[std::clamp(part, 0, 3)]);
    return true;
  }
  const int field = s3g_thru_partials::index(key);
  if (field < 0)
    return false;
  value = s3g_thru_partials::resolved(
      liveControlsForPart(part).partials.patches[tableEditLane(part)], field);
  return true;
}
int formatPartialsState(char *out, int capacity) const {
  const int part = splitEnabled ? splitFocus : 0, edit = tableEditLane(part);
  const auto &program = liveControlsForPart(part).partials;
  if (liveControlsForPart(part).algorithm !=
      Engine::PlaybackAlgorithm::partials)
    return std::snprintf(out, capacity, "{}");
  int at = std::snprintf(
      out, capacity,
      "{\"part\":%d,\"edit\":%d,\"revision\":%u,\"group\":%d,\"patches\":[",
      part, edit, unsigned(midiRevision), partialsBandGroups[part]);
  for (int lane = 0; lane < 4; ++lane) {
    if (at < 0 || at >= capacity)
      return -1;
    at += std::snprintf(out + at, capacity - at, "%s[", lane ? "," : "");
    for (int f = 0; f < s3g_thru_partials::fields; ++f) {
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

void seedPartialsRecording() {
  if (!clipRecording)
    return;
  const int count = splitEnabled ? partCount() : 1;
  for (int part = 0; part < count; ++part) {
    const auto &c = liveControlsForPart(part);
    bool used = c.algorithm == Engine::PlaybackAlgorithm::partials;
    const auto cfg = settingsForPart(part).engines;
    if (cfg.enabled && cfg.configured)
      for (int entry = 0; entry < engineSlotCount(); ++entry)
        used |= cfg.engines[entry] == int(Engine::PlaybackAlgorithm::partials);
    if (!used)
      continue;
    const int first = splitEnabled && !sharedDual ? firstLaneForPart(part) : 0,
              last =
                  splitEnabled && !sharedDual ? first + (isQuad() ? 0 : 1) : 3;
    for (int lane = first; lane <= last; ++lane)
      for (int field = 0; field < s3g_thru_partials::fields; ++field) {
        const auto curve = kClipPartialsOffset +
                           (part * 4 + lane) * s3g_thru_partials::fields +
                           field;
        if (clipReplacing || !activeClipSlot()->clipPointCounts[curve])
          captureClipPoint(ClipPointType::partialsParameter, field,
                           part * 4 + lane, c.partials.patches[lane].v[field],
                           0, true);
      }
  }
}
bool restorePartialsJson(const char *json) {
  const char *at = std::strstr(json, "\"partials_programs\"");
  if (!at)
    return true;
  at = std::strchr(at, '[');
  if (!at)
    return false;
  std::array<s3g_thru_partials::Program, 5> programs{};
  for (auto &program : programs)
    for (auto &patch : program.patches)
      for (int f = 0; f < s3g_thru_partials::fields; ++f) {
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
      if (!s3g_thru_partials::valid(patch))
        return false;
  splitSingleControls.partials = programs[0];
  if (!splitEnabled)
    controls.partials = programs[0];
  for (int part = 0; part < 4; ++part)
    splitParts[part].controls.partials = programs[part + 1];
  if (splitEnabled)
    controls.partials = splitParts[splitFocus].controls.partials;
  return true;
}
