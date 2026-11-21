std::bitset<16*s3g_thru_scanned::fields> scannedOverrides{};
std::array<std::array<std::array<float,64>,4>,4> scannedProfiles{};
static bool
writeScannedExtension(std::FILE *file,
                    const std::array<ClipSlot, kClipSlotCount> &clips) {
  if (!writeU32(file, 0x314e4353u))
    return false;
  for (const auto &c : clips)
    for (const auto &program : c.scannedPrograms)
      for (const auto &patch : program.patches)
        for (int field=0;field<s3g_thru_scanned::legacyFields;++field)
          if (!writeF32(file, patch.v[field]))
            return false;
  return true;
}
static bool readScannedExtension(std::FILE *file,
                               std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(file, magic) || magic != 0x314e4353u)
    return false;
  for (auto &c : clips)
    for (auto &program : c.scannedPrograms)
      for (auto &patch : program.patches) {
        for (int field=0;field<s3g_thru_scanned::legacyFields;++field)
          if (!readF32(file, patch.v[field]))
            return false;
        if (!s3g_thru_scanned::valid(patch))
          return false;
      }
  return true;
}
static bool writeScannedHitExtension(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips){
 if(!writeU32(file,0x31504853u))return false;
 for(const auto& clip:clips)for(const auto& program:clip.scannedPrograms)for(const auto& patch:program.patches)for(int f=s3g_thru_scanned::legacyFields;f<s3g_thru_scanned::percussionFields;++f)if(!writeF32(file,patch.v[f]))return false;
 return true;
}
static bool readScannedHitExtension(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips){
 std::uint32_t magic=0;if(!readU32(file,magic)||magic!=0x31504853u)return false;
 for(auto& clip:clips)for(auto& program:clip.scannedPrograms)for(auto& patch:program.patches){for(int f=s3g_thru_scanned::legacyFields;f<s3g_thru_scanned::percussionFields;++f)if(!readF32(file,patch.v[f]))return false;if(!s3g_thru_scanned::valid(patch))return false;}
 return true;
}
static bool writeScannedAmpExtension(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips){
 if(!writeU32(file,0x31504153u))return false;
 for(const auto& clip:clips)for(const auto& program:clip.scannedPrograms)for(const auto& patch:program.patches)for(int f=s3g_thru_scanned::percussionFields;f<s3g_thru_scanned::fields;++f)if(!writeF32(file,patch.v[f]))return false;
 return true;
}
static bool readScannedAmpExtension(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips){
 std::uint32_t magic=0;if(!readU32(file,magic)||magic!=0x31504153u)return false;
 for(auto& clip:clips)for(auto& program:clip.scannedPrograms)for(auto& patch:program.patches){for(int f=s3g_thru_scanned::percussionFields;f<s3g_thru_scanned::fields;++f)if(!readF32(file,patch.v[f]))return false;if(!s3g_thru_scanned::valid(patch))return false;}
 return true;
}
void captureScannedPrograms(ClipSlot &clip) const {
  clip.scannedPrograms[0] =
      splitEnabled ? splitSingleControls.scanned : controls.scanned;
  for (int part = 0; part < 4; ++part)
    clip.scannedPrograms[part + 1] = liveControlsForPart(part).scanned;
}
void restoreScannedPrograms(const ClipSlot &clip) {
  splitSingleControls.scanned = clip.scannedPrograms[0];
  if (splitEnabled) {
    for (int part = 0; part < 4; ++part)
      splitParts[part].controls.scanned = clip.scannedPrograms[part + 1];
    controls.scanned = splitParts[splitFocus].controls.scanned;
  } else
    controls.scanned = clip.scannedPrograms[0];
}
bool setScannedParam(const char *key, const char *text) {
  using namespace s3g_thru_scanned;
  const int field = index(key);
  if (field < 0)
    return false;
  const float value = parseFloat(text, NAN);
  if (!std::isfinite(value))
    return true;
  const int part = splitEnabled ? splitFocus : 0, lane = tableEditLane(part);
  auto &c = controls;
  auto cfg = c.scanned.patches[lane];
  if (!set(cfg, field, value))
    return true;
  std::array<std::size_t, fields> replaced;
  replaced.fill(kClipCurveLaneCount);
  for (int f = 0; f < fields; ++f)
    if (f == field || cfg.v[f] != c.scanned.patches[lane].v[f]) {
      const auto index = (part * 4 + lane) * fields + f;
      replaced[f] = kClipScannedOffset + index;
      scannedOverrides.set(index);
    }
  if (sequenceMode() && !clipRecording)
    replaceSequenceAutomation(replaced, true);
  for (int f = 0; f < fields; ++f)
    if (cfg.v[f] != c.scanned.patches[lane].v[f])
      captureClipPoint(ClipPointType::scannedParameter, f, part * 4 + lane,
                       cfg.v[f]);
  c.scanned.patches[lane] = cfg;
  
  if (clipRecording)
    captureClipPoint(ClipPointType::scannedParameter, field, part * 4 + lane,
                     cfg.v[field]);
  if (auto *clip = activeClipSlot())
    captureScannedPrograms(*clip);
  markPersistentStateDirty();
  ++midiRevision;
  return true;
}
bool scannedParameterValue(const char *key, int part, float &value) const {
  const int field = s3g_thru_scanned::index(key);
  if (field < 0)
    return false;
  value = liveControlsForPart(part).scanned.patches[tableEditLane(part)].v[field];
  return true;
}
int formatScannedState(char *out, int capacity) const {
  const int part = splitEnabled ? splitFocus : 0, edit = tableEditLane(part);
  const auto &program = liveControlsForPart(part).scanned;
  if (liveControlsForPart(part).algorithm != Engine::PlaybackAlgorithm::scanned)
    return std::snprintf(out, capacity, "{}");
  int at = std::snprintf(
      out, capacity, "{\"part\":%d,\"edit\":%d,\"revision\":%u,\"patches\":[",
      part, edit, unsigned(midiRevision));
  for (int lane = 0; lane < 4; ++lane) {
    if (at < 0 || at >= capacity)
      return -1;
    at += std::snprintf(out + at, capacity - at, "%s[", lane ? "," : "");
    for (int f = 0; f < s3g_thru_scanned::fields; ++f) {
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

void seedScannedRecording() {
  if (!clipRecording)
    return;
  const int count = splitEnabled ? partCount() : 1;
  for (int part = 0; part < count; ++part) {
    const auto &c = liveControlsForPart(part);
    bool used=c.algorithm==Engine::PlaybackAlgorithm::scanned;
    const auto cfg=settingsForPart(part).engines;
    if(cfg.enabled&&cfg.configured)for(int entry=0;entry<engineSlotCount();++entry)used|=cfg.engines[entry]==int(Engine::PlaybackAlgorithm::scanned);
    if(!used)continue;
    const int first = splitEnabled && !sharedDual ? firstLaneForPart(part) : 0,
              last =
                  splitEnabled && !sharedDual ? first + (isQuad() ? 0 : 1) : 3;
    for (int lane = first; lane <= last; ++lane)
      for (int field = 0; field < s3g_thru_scanned::fields; ++field) {
        const auto curve = kClipScannedOffset +
                           (part * 4 + lane) * s3g_thru_scanned::fields + field;
        if (clipReplacing || !activeClipSlot()->clipPointCounts[curve])
          captureClipPoint(ClipPointType::scannedParameter, field,
                           part * 4 + lane, c.scanned.patches[lane].v[field], 0,
                           true);
      }
  }
}
bool restoreScannedJson(const char *json) {
  const char *at = std::strstr(json, "\"scanned_programs\"");
  if (!at)
    return true;
  at = std::strchr(at, '[');
  if (!at)
    return false;
  std::array<s3g_thru_scanned::Program, 5> programs{};
  for (auto &program : programs)
    for (auto &patch : program.patches)
      for (int f=0;f<s3g_thru_scanned::legacyFields;++f) {
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
      if (!s3g_thru_scanned::valid(patch))
        return false;
  splitSingleControls.scanned = programs[0];
  if (!splitEnabled)
    controls.scanned = programs[0];
  for (int part = 0; part < 4; ++part)
    splitParts[part].controls.scanned = programs[part + 1];
  if (splitEnabled)
    controls.scanned = splitParts[splitFocus].controls.scanned;
  return true;
}

bool restoreScannedHitJson(const char* json){
 const char* at=std::strstr(json,"\"scanned_hit_programs\"");if(!at)return true;at=std::strchr(at,'[');if(!at)return false;
 std::array<s3g_thru_scanned::Program,5> programs{};programs[0]=splitSingleControls.scanned;
 for(int p=0;p<4;++p)programs[p+1]=splitParts[p].controls.scanned;
 for(auto& program:programs)for(auto& patch:program.patches)for(int f=s3g_thru_scanned::legacyFields;f<s3g_thru_scanned::percussionFields;++f){
   while(*at=='['||*at==','||*at==' '||*at=='\n'||*at==']')++at;
   char* end=nullptr;const float value=std::strtof(at,&end);if(end==at||!std::isfinite(value))return false;at=end;patch.v[f]=value;
 }
 while(*at==' '||*at=='\n'){++at;}
 if(*at!=']')return false;
 for(const auto& program:programs)for(const auto& patch:program.patches)if(!s3g_thru_scanned::valid(patch))return false;
 splitSingleControls.scanned=programs[0];if(!splitEnabled)controls.scanned=programs[0];
 for(int p=0;p<4;++p){splitParts[p].controls.scanned=programs[p+1];}
 if(splitEnabled)controls.scanned=splitParts[splitFocus].controls.scanned;
 return true;
}

bool restoreScannedAmpJson(const char* json){
 const char* at=std::strstr(json,"\"scanned_amp_programs\"");if(!at)return true;at=std::strchr(at,'[');if(!at)return false;
 std::array<s3g_thru_scanned::Program,5> programs{};programs[0]=splitSingleControls.scanned;
 for(int p=0;p<4;++p)programs[p+1]=splitParts[p].controls.scanned;
 for(auto& program:programs)for(auto& patch:program.patches)for(int f=s3g_thru_scanned::percussionFields;f<s3g_thru_scanned::fields;++f){
   while(*at=='['||*at==','||*at==' '||*at=='\n'||*at==']')++at;
   char* end=nullptr;const float value=std::strtof(at,&end);if(end==at||!std::isfinite(value))return false;at=end;patch.v[f]=value;
 }
 while(*at==' '||*at=='\n'){++at;}
 if(*at!=']')return false;
 for(const auto& program:programs)for(const auto& patch:program.patches)if(!s3g_thru_scanned::valid(patch))return false;
 splitSingleControls.scanned=programs[0];if(!splitEnabled)controls.scanned=programs[0];
 for(int p=0;p<4;++p){splitParts[p].controls.scanned=programs[p+1];}
 if(splitEnabled)controls.scanned=splitParts[splitFocus].controls.scanned;
 return true;
}
