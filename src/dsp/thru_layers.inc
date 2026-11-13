LayerPatch layersLive, layersRecorded;
s3g_thru_layers::Rack layersRack;
std::bitset<LayerPatch::size> layersOverrides;
std::array<bool, 32> layersDown{}, layersLatch{};
std::array<float, 32> layersPressure{};
std::array<std::uint32_t, 32> layersOrder{};
std::uint32_t layersClock = 0, layersEditSerial = 0;
int layersEditSurface = 1, layersEditCorner = 0;
std::array<int, 2> layersSelectedCorner{};
std::array<float, 12> layersFxClipboard{};
bool layersClipboardReady = false;
void copyLayersRuntimeTo(ThruInstance &destination) const {
  destination.layersLive = layersLive;
  destination.layersRecorded = layersRecorded;
  destination.layersOverrides = layersOverrides;
  destination.layersDown = layersDown;
  destination.layersLatch = layersLatch;
  destination.layersPressure = layersPressure;
  destination.layersOrder = layersOrder;
  destination.layersClock = layersClock;
  destination.layersEditSurface = layersEditSurface;
  destination.layersEditCorner = layersEditCorner;
  destination.layersSelectedCorner = layersSelectedCorner;
  destination.layersEditSerial = layersEditSerial;
}
void initializeLayers() {
  // Format 37 retains the initial prototype's engine fields as reserved data.
  // Engine playback and note ownership use the normal instrument controls.
  layersLive.upgrade();
  if (layersLive.initialized)
    return;
  layersLive.initialized = true;
  layersLive.values[LayerPatch::config + LayerPatch::play] = 1;
  layersEditSurface = 1;
  layersEditCorner = layersSelectedCorner[1];
  layersRecorded = layersLive;
}
void clearLayersTouches() {
  layersDown = {};
  layersLatch = {};
  layersPressure = {};
  layersOrder = {};
  layersEditSerial++;
}
void layersWrite(std::size_t index, float value, bool record = true) {
  if (!LayerPatch::editable(index))
    return;
  const float next = LayerPatch::bound(index, value);
  const float old = layersLive.values[index];
  if (next == old) {
    // A repeated physical position still takes ownership from automation.
    if (clipPlaying)
      layersOverrides.set(index);
    if (!clipRecording || clipLastPointIndices[kClipLayersOffset + index] >= 0)
      return;
  }
  const int chunk = static_cast<int>(index / 192u);
  const auto field = index % 192u, lane = kClipLayersOffset + index;
  if (record && clipRecording) {
    if (!clipReplacing && clipLastPointIndices[lane] < 0) {
      auto *clip = activeClipSlot();
      if (clip && clip->clipPointCount < kClipPointCapacity) {
        auto &baseline = clip->clipPoints[clip->clipPointCount++];
        baseline.timeMs = clipCaptureTimeMs();
        baseline.value = old;
        baseline.parameter = clipPointDescriptor(field, clipRecordingTake);
        baseline.sourcePad = static_cast<std::uint8_t>(chunk);
        baseline.type = ClipPointType::layersBaseline;
      }
    }
    if (clipReplacing && clipLastPointIndices[lane] < 0)
      captureClipPoint(ClipPointType::layersParameter, field, chunk, old, 0);
    captureClipPoint(ClipPointType::layersParameter, field, chunk, next);
  }
  layersLive.values[index] = next;
  layersOverrides.set(index);
  markPersistentStateDirty();
}
void seedLayersRecording() {
  if (!layersLive.active || !clipRecording)
    return;
  auto *clip = activeClipSlot();
  if (!clip)
    return;
  bool held = false;
  for (int pad = 4; pad < 32; ++pad)
    if (pad % 8 >= 4 && (layersDown[pad] || layersLatch[pad])) {
      held = true;
      break;
    }
  if (!clipReplacing && !held)
    return;
  const auto seed = [&](std::size_t index) {
    const auto field = index % 192u, chunk = index / 192u,
               lane = kClipLayersOffset + index;
    if (!clipReplacing && clipLastPointIndices[lane] < 0 &&
        clip->clipPointCount < kClipPointCapacity) {
      float previous = clip->layers.values[index];
      sampleClipCurve(*clip, lane, clipPositionMs, false, previous);
      auto &baseline = clip->clipPoints[clip->clipPointCount++];
      baseline.timeMs = clipCaptureTimeMs();
      baseline.value = previous;
      baseline.parameter = clipPointDescriptor(field, clipRecordingTake);
      baseline.sourcePad = static_cast<std::uint8_t>(chunk);
      baseline.type = ClipPointType::layersBaseline;
    }
    captureClipPoint(ClipPointType::layersParameter, field,
                     static_cast<int>(chunk), layersLive.values[index]);
    if (!clipReplacing)
      layersOverrides.set(index);
  };
  if (clipReplacing) {
    for (std::size_t i = LayerPatch::advanced; i < LayerPatch::config; ++i)
      if (LayerPatch::editable(i))
        seed(i);
    for (const auto setting : {LayerPatch::glide, LayerPatch::depth,
                               LayerPatch::zMode, LayerPatch::returnDry})
      seed(LayerPatch::config + setting);
  }
  for (int c = 0; c < 4; ++c)
    seed(LayerPatch::config + LayerPatch::weight0 + c);
  seed(LayerPatch::config + LayerPatch::pressure);
}
std::uint32_t layersActiveFxPads() const {
  std::uint32_t mask = 0;
  for (int p = 0; p < 32; ++p)
    if (p % 8 >= 4 && (layersDown[p] || layersLatch[p]))
      mask |= std::uint32_t(1) << p;
  if (mask)
    return mask;
  const auto &patch = clipPlaying ? layersRecorded : layersLive;
  if (patch.setting(LayerPatch::depth) <= .0001f ||
      (patch.setting(LayerPatch::pressure) <= .0001f &&
       (patch.setting(LayerPatch::zMode) < .5f ||
        patch.setting(LayerPatch::returnDry) >= .5f)))
    return 0;
  float sum = 0, x = 0, y = 0;
  for (int c = 0; c < 4; ++c) {
    const float w = patch.setting(
        static_cast<LayerPatch::Setting>(LayerPatch::weight0 + c));
    sum += w;
    x += w * (c % 2);
    y += w * (c / 2);
  }
  if (sum <= .0001f)
    return 0;
  const int col = std::clamp(int(std::lround(x / sum * 3)), 0, 3),
            row = std::clamp(int(std::lround(y / sum * 3)), 0, 3);
  return std::uint32_t(1) << ((3 - row) * 8 + 4 + col);
}
int heldLayersCorner() const {
  int found = -1;
  for (int p = 0; p < 32; ++p)
    if (p % 8 >= 4 && layersDown[p] && s3g_thru_layers::cornerForPad(p) >= 0 &&
        (found < 0 || layersOrder[p] > layersOrder[found]))
      found = p;
  return found;
}
void focusLayersCorner() {
  const int p = heldLayersCorner();
  if (p >= 0) {
    layersEditSurface = p % 8 >= 4;
    layersEditCorner = s3g_thru_layers::cornerForPad(p);
    layersSelectedCorner[layersEditSurface] = layersEditCorner;
  }
}
void layersResolveFx() {
  std::array<float, 4> result{};
  float total = 0, z = 0;
  for (int pad = 0; pad < 32; ++pad)
    if (pad % 8 >= 4 && (layersDown[pad] || layersLatch[pad])) {
      const float pressure = std::max(.005f, layersPressure[pad]);
      const auto w = s3g_thru_layers::weights(s3g_thru_layers::padPoint(pad));
      for (int c = 0; c < 4; ++c)
        result[c] += w[c] * pressure;
      total += pressure;
      z = std::max(z, pressure);
    }
  if (total <= 0) {
    if (layersLive.setting(LayerPatch::returnDry) < .5f)
      return;
    result = {1, 0, 0, 0};
    total = 1;
    z = 0;
  }
  for (int c = 0; c < 4; ++c)
    layersWrite(LayerPatch::config + LayerPatch::weight0 + c,
                result[c] / total);
  layersWrite(LayerPatch::config + LayerPatch::pressure, z);
}
bool layersPadOn(int pad, int) {
  if (!layersLive.active || pad < 0 || pad >= 32 || pad % 8 < 4)
    return false;
  if (performancePadOn(pad))
    return true;
  layersDown[pad] = true;
  layersPressure[pad] = .75f;
  layersOrder[pad] = ++layersClock;
  if (shiftHeld)
    layersLatch[pad] = !layersLatch[pad];
  layersResolveFx();
  layersEditSerial++;
  return true;
}
bool layersPadOff(int pad) {
  if (!layersLive.active || pad < 0 || pad >= 32 || pad % 8 < 4)
    return false;
  if (performancePadOff(pad))
    return true;
  layersDown[pad] = false;
  layersEditSerial++;
  layersResolveFx();
  return true;
}
bool layersPadPressure(int pad, int value) {
  if (!layersLive.active || pad < 0 || pad >= 32 || pad % 8 < 4)
    return false;
  // Move may send terminal zero pressure before or after Note Off.
  // STAY retains the last positive pressure; RLS DRY is handled on release.
  if (value <= 0)
    return true;
  layersPressure[pad] = unitLayer(float(value) / 127);
  if (layersDown[pad] || layersLatch[pad])
    layersResolveFx();
  return true;
}
static float unitLayer(float v) { return s3g_thru_layers::unit(v); }
int layerAlias(const char *key) const {
  int f = -1;
  char tail = 0;
  if (std::sscanf(key, "layers_fx%d%c", &f, &tail) == 1 && f >= 0 && f < 12)
    return f < 8 ? int(LayerPatch::fx) + layersSelectedCorner[1] * 8 + f
                 : int(LayerPatch::advanced) + layersSelectedCorner[1] * 4 + f -
                       8;
  const char *names[]{"layers_glide", "layers_depth", "layers_z_mode",
                      "layers_z", "layers_return"};
  const LayerPatch::Setting settings[]{LayerPatch::glide, LayerPatch::depth,
                                       LayerPatch::zMode, LayerPatch::pressure,
                                       LayerPatch::returnDry};
  for (std::size_t i = 0; i < 5; ++i)
    if (!std::strcmp(key, names[i]))
      return int(LayerPatch::config + settings[i]);
  return -1;
}
bool setLayersParam(const char *key, const char *value) {
  if (std::strncmp(key, "layers_", 7))
    return false;
  if (!std::strcmp(key, "layers_surface")) {
    layersEditSurface = 1;
    layersEditCorner = layersSelectedCorner[layersEditSurface];
    return true;
  }
  if (!std::strcmp(key, "layers_corner")) {
    layersEditCorner = clampValue(parseInteger(value, 0), 0, 3);
    layersSelectedCorner[layersEditSurface] = layersEditCorner;
    return true;
  }
  if (!std::strcmp(key, "layers_cancel_edit")) {
    layersEditSerial++;
    return true;
  }
  if (!std::strcmp(key, "layers_fx_x") || !std::strcmp(key, "layers_fx_y")) {
    float x = 0, y = 0;
    for (int c = 0; c < 4; ++c) {
      const float w =
          layersLive.values[LayerPatch::config + LayerPatch::weight0 + c];
      x += w * float(c % 2);
      y += w * float(c / 2);
    }
    if (key[10] == 'x')
      x = unitLayer(parseFloat(value, x));
    else
      y = unitLayer(parseFloat(value, y));
    const auto w = s3g_thru_layers::weights({x, y});
    for (int c = 0; c < 4; ++c)
      layersWrite(LayerPatch::config + LayerPatch::weight0 + c, w[c]);
    return true;
  }
  const int i = layerAlias(key);
  if (i >= 0) {
    initializeLayers();
    const auto previous = layersLive.values[i];
    float requested = parseFloat(value, previous);
    if (i >= int(LayerPatch::fx) && i < int(LayerPatch::config) &&
        (i - LayerPatch::fx) % 8 == 5 &&
        (int(layersLive.values[i - 5]) <= 1 ||
         int(layersLive.values[i - 5]) == 11))
      requested = requested >= 8.5f ? 9.f : std::min(requested, 4.f);
    layersWrite(std::size_t(i), requested);
    if (i >= int(LayerPatch::fx) && i < int(LayerPatch::config) &&
        (i - LayerPatch::fx) % 8 == 0 && previous != layersLive.values[i]) {
      const auto algorithm = int(layersLive.values[i]);
      if (algorithm >= 11) {
        const std::array<std::array<float, 4>, 6> defaults{
            {{.5f, .5f, .63092975f, 0},
             {.35355339f, .65f, .5463034f, .5f},
             {3.f / 7, 1, 1, .5f},
             {4.f / 7, .55f, 0, 0},
             {6.f / 7, 4.f / 7, .25f, .15f},
             {.5f, 1, 0, .5f}}};
        const auto &primary = defaults[std::clamp(algorithm - 11, 0, 5)];
        for (int f = 0; f < 4; ++f)
          layersWrite(std::size_t(i) + 1 + f, primary[f]);
      }
      const auto defaults = s3g_thru_layers::detailDefaults(algorithm);
      for (int f = 0; f < 4; ++f)
        layersWrite(LayerPatch::advanced + layersSelectedCorner[1] * 4 + f,
                    defaults[f]);
    }
    return true;
  }
  if (!std::strcmp(key, "layers_copy") || !std::strcmp(key, "layers_paste")) {
    clearError();
    initializeLayers();
    const bool copy = std::strcmp(key, "layers_copy") == 0;
    if (!copy && !layersClipboardReady) {
      setError("Copy an FX corner first");
      return true;
    }
    if (!copy) {
      captureActiveClip(false);
      pushClipUndo(activeClip);
    }
    for (int f = 0; f < 12; ++f) {
      const auto index =
          f < 8 ? LayerPatch::fx + layersSelectedCorner[1] * 8 + f
                : LayerPatch::advanced + layersSelectedCorner[1] * 4 + f - 8;
      if (copy)
        layersFxClipboard[f] = layersLive.values[index];
      else
        layersWrite(index, layersFxClipboard[f]);
    }
    if (copy)
      layersClipboardReady = true;
    return true;
  }
  return false;
}
int getLayersParam(const char *key, char *b, int capacity) const {
  if (std::strncmp(key, "layers_", 7))
    return -2;
  const int i = layerAlias(key);
  if (i >= 0)
    return std::snprintf(b, capacity, "%.9g", layersLive.values[i]);
  if (!std::strcmp(key, "layers_fx_x") || !std::strcmp(key, "layers_fx_y")) {
    float v = 0;
    for (int c = 0; c < 4; ++c)
      v += layersLive.values[LayerPatch::config + LayerPatch::weight0 + c] *
           float(key[10] == 'x' ? c % 2 : c / 2);
    return std::snprintf(b, capacity, "%.6f", v);
  }
  if (!std::strcmp(key, "layers_surface"))
    return std::snprintf(b, capacity, "%d", layersEditSurface);
  if (!std::strcmp(key, "layers_corner"))
    return std::snprintf(b, capacity, "%d", layersEditCorner);
  if (std::strcmp(key, "layers_view"))
    return -2;
  if (!layersLive.active)
    return std::snprintf(b, capacity, "{\"active\":0}");
  int at =
      std::snprintf(b, capacity,
                    "{\"active\":%d,\"surface\":%d,\"corner\":%d,\"held\":%d,"
                    "\"serial\":%u,\"values\":[",
                    layersLive.active ? 1 : 0, layersEditSurface,
                    layersEditCorner, heldLayersCorner(), layersEditSerial);
  if (at < 0 || at >= capacity)
    return -1;
  const auto append = [&](const char *fmt, auto... args) {
    const int n = std::snprintf(b + at, capacity - at, fmt, args...);
    if (n < 0 || n >= capacity - at)
      return false;
    at += n;
    return true;
  };
  for (std::size_t p = LayerPatch::fx; p < LayerPatch::size; ++p)
    if (!append("%s%.7g", p == LayerPatch::fx ? "" : ",", layersLive.values[p]))
      return -1;
  if (!append("%s", "],\"extra\":["))
    return -1;
  for (std::size_t i = 0; i < LayerPatch::advancedCount; ++i)
    if (!append("%s%.7g", i ? "," : "",
                layersLive.values[LayerPatch::advanced + i]))
      return -1;
  if (!append("],\"fxCorner\":%d,\"fxPads\":%u,\"touches\":[",
              layersSelectedCorner[1], layersActiveFxPads()))
    return -1;
  for (int p = 0; p < 32; ++p)
    if (!append("%s%d", p ? "," : "",
                (layersDown[p] ? 1 : 0) + (layersLatch[p] ? 2 : 0)))
      return -1;
  const auto &mix = layersRack.currentWeights();
  const auto &audible = clipPlaying ? layersRecorded : layersLive;
  if (!append("],\"mix\":[%.5f,%.5f,%.5f,%.5f],\"z\":%.5f,\"voices\":[", mix[0],
              mix[1], mix[2], mix[3], audible.setting(LayerPatch::pressure)))
    return -1;
  bool comma = false;
  for (const auto &v : voices)
    if (v.active) {
      if (!append("%s[%d,%d,%d]", comma ? "," : "", v.sourcePad, v.note,
                  int(&v - voices.data())))
        return -1;
      comma = true;
    }
  return append("%s", " ]}") ? at : -1;
}
void applyLayersCurves(const ClipSlot &clip, double position) {
  layersRecorded = clip.layers.active ? clip.layers : layersLive;
  for (std::size_t i = LayerPatch::advanced; i < LayerPatch::size; ++i) {
    if (!LayerPatch::editable(i))
      continue;
    if (layersOverrides[i])
      layersRecorded.set(i, layersLive.values[i]);
    const auto lane = kClipLayersOffset + i;
    float value = 0;
    if (clip.clipPointCounts[lane] && !layersOverrides[i] &&
        // Targets change at the recorded event; Rack glide/slew supplies
        // the same smooth movement as live playing, retaining plateaus.
        sampleClipCurve(clip, lane, position, false, value)) {
      layersRecorded.set(i, value);
    }
  }
}
bool writeLayersExtension(
    std::FILE *f, const std::array<ClipSlot, kClipSlotCount> &clips) const {
  if (!writeU32(f, 0x4c415931u))
    return false;
  for (const auto &clip : clips) {
    if (!writeU32(f, clip.layers.active ? 1u : 0u) ||
        !writeU32(f, clip.layers.initialized ? 1u : 0u))
      return false;
    for (float v : clip.layers.values)
      if (!writeF32(f, v))
        return false;
  }
  return true;
}
bool readLayersExtension(std::FILE *f,
                         std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(f, magic) || magic != 0x4c415931u)
    return false;
  for (auto &clip : clips) {
    std::uint32_t active = 0, initialized = 0;
    if (!readU32(f, active) || !readU32(f, initialized) || active > 1 ||
        initialized > 1)
      return false;
    clip.layers.active = active != 0;
    clip.layers.initialized = initialized != 0;
    if (clip.layers.active && (clip.splitEnabled || !clip.layers.initialized))
      return false;
    for (std::size_t i = 0; i < LayerPatch::size; ++i) {
      float v = 0;
      if (!readF32(f, v) || !std::isfinite(v))
        return false;
      clip.layers.set(i, v);
    }
    clip.layers.upgrade();
  }
  return true;
}
int appendLayersJson(char *b, int capacity, int length) const {
        PersistentStateSnapshot snapshot; snapshot.capture(*this);
        return snapshot.appendLayersJson(b,capacity,length);
    }
bool restoreLayersJson(const char *json) {
  LayerPatch next;
  float v = 0;
  jsonNumber(json, "layers_mode", v);
  next.active = v >= .5f;
  v = 0;
  jsonNumber(json, "layers_initialized", v);
  next.initialized = v >= .5f;
  const char *at = std::strstr(json, "\"layers_values\":[");
  if (at) {
    at += 17;
    for (std::size_t i = 0; i < LayerPatch::size; ++i) {
      char *end = nullptr;
      const float x = std::strtof(at, &end);
      if (end == at || !std::isfinite(x) ||
          *end != (i + 1 == LayerPatch::size ? ']' : ','))
        return false;
      next.set(i, x);
      at = end + 1;
    }
  } else if (next.active || next.initialized)
    return false;
  if (next.active && (splitEnabled || !next.initialized))
    return false;
  next.upgrade();
  layersLive = layersRecorded = next;
  layersOverrides.reset();
  clearLayersTouches();
  layersRack.prime(layersLive);
  return true;
}
