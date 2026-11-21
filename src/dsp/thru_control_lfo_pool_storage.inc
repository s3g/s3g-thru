static int migrateControlLfoDest(int old, int algorithm, bool split,
                                 int &target, float &depthScale) {
  const char *key = "off";
  depthScale = 1;
  switch (old) {
  case 1:
    key = "part_level";
    break;
  case 2:
    key = "part_pan";
    break;
  case 3:
    key = "effect_tone";
    break;
  case 4:
  case 12:
    key = "speed";
    depthScale = .5f;
    break;
  case 5:
    key = algorithm == 7 ? "long_grain_length" : "grain_size";
    break;
  case 6:
    key = algorithm == 7 ? "long_grain_spray" : "grain_spray";
    break;
  case 7:
    key = split ? "part_delay" : "effect_mix";
    if (!split)
      target = 4;
    break;
  case 8:
    key = split ? "part_reverb" : "effect_reverb_mix";
    if (!split)
      target = 4;
    break;
  case 9:
    key = "layers_fx_x";
    target = 4;
    break;
  case 10:
    key = "layers_fx_y";
    target = 4;
    break;
  case 13:
    key = "part_level";
    depthScale = .4f;
    break;
  default:
    break;
  }
  return controlLfoDestinationForKey(key);
}
static s3g_thru_lfo::Config migrateControlLfoConfig(s3g_thru_lfo::Config old,
                                                    int part, int algorithm,
                                                    bool split) {
  float scale = 1;
  old.target = part;
  old.engine = -1;
  old.v[0] = float(migrateControlLfoDest(int(old.v[0]), algorithm, split,
                                         old.target, scale));
  old.v[4] *= scale;
  old.outputAnchor *= scale;
  old.poolRoutes = {};
  return old;
}
static void migrateControlLfoPool(ClipSlot &clip, bool recorded) {
  for (int p = 0; p < 4; ++p)
    for (int s = 0; s < 2; ++s) {
      if (!clip.splitEnabled && p > 0)
        continue;
      const auto &legacy = clip.splitEnabled ? clip.splitSettings[p].lfos[s]
                                             : clip.singleSettings.lfos[s];
      const int algorithm =
          int(clip.splitEnabled ? clip.splitValues[p][5] : clip.baseValues[5]);
      clip.lfos[p * 2 + s] =
          migrateControlLfoConfig(legacy, p, algorithm, clip.splitEnabled);
    }
  if (!recorded)
    return;
  for (int p = 0; p < 4; ++p)
    for (int set = 0; set < 2; ++set)
      for (auto &step : set ? clip.sequence.tracks[p].generator.base
                            : clip.sequence.tracks[p].trigs) {
        const auto old = step.lfos;
        step.lfos = {};
        for (int j = 0; j < 2; ++j) {
          const int i = p * 2 + j;
          step.lfos.rates[i] = old.rates[j];
          step.lfos.depths[i] = old.depths[j];
          step.lfos.mask |=
              std::uint16_t(((old.mask >> (j * 2)) & 3u) << (i * 2));
          if (!(old.reset & (1u << j)))
            step.lfos.reset &= ~std::uint8_t(1u << i);
        }
      }
  for (std::size_t i = 0; i < clip.clipPointCount; ++i) {
    auto &point = clip.clipPoints[i];
    if (point.type != ClipPointType::controlLfo)
      continue;
    const int field = (point.parameter & 255u) % 9,
              slot = int(point.sourcePad) * 2 + (point.parameter & 255u) / 9;
    const auto owner = int(point.sourcePad);
    if (slot < 0 || slot > 7)
      continue;
    if (field == 0) {
      float scale = 1;
      int target = owner;
      point.value = float(migrateControlLfoDest(
          int(point.value),
          int(clip.splitEnabled ? clip.splitValues[owner][5]
                                : clip.baseValues[5]),
          clip.splitEnabled, target, scale));
    } else if (field == 4) {
      const auto &legacy = clip.splitEnabled
                               ? clip.splitSettings[owner].lfos[slot % 2]
                               : clip.singleSettings.lfos[slot % 2];
      if (legacy.v[0] == 4 || legacy.v[0] == 12)
        point.value *= .5f;
      else if (legacy.v[0] == 13)
        point.value *= .4f;
    }
    point.type = ClipPointType::pooledLfo;
    point.sourcePad = std::uint8_t(slot);
    point.parameter = clipPointDescriptor(field, clipPointTake(point));
  }
  std::sort(
      clip.clipPoints.begin(), clip.clipPoints.begin() + clip.clipPointCount,
      [](const ClipPoint &a, const ClipPoint &b) {
        const auto x = clipPointLane(a), y = clipPointLane(b);
        return x < y ||
               (x == y && (a.timeMs < b.timeMs || (a.timeMs == b.timeMs &&
                                                   a.parameter < b.parameter)));
      });
  indexSortedClipPoints(clip);
}
static bool writePoolConfig(std::FILE *f, const s3g_thru_lfo::Config &c) {
  if (!s3g_thru_lfo::valid(c))
    return false;
  bool ok = true;
  for (float v : c.v)
    ok = ok && writeF32(f, v);
  ok = ok && writeU32(f, c.target) && writeU32(f, c.engine + 1) &&
       writeLfoAnchor(f, c.anchor) && writeU32(f, c.seed) &&
       writeU32(f, c.cycle) && writeU32(f, c.captured ? 1 : 0) &&
       writeF32(f, c.outputAnchor) && writeU32(f, c.runningAnchor ? 1 : 0);
  for (const auto &r : c.poolRoutes)
    ok = ok && writeU32(f, r.id.part + 1) && writeU32(f, r.id.engine + 1) &&
         writeU32(f, r.id.dest) && writeF32(f, r.value);
  return ok;
}
static bool readPoolConfig(std::FILE *f, s3g_thru_lfo::Config &c) {
  for (float &v : c.v)
    if (!readF32(f, v))
      return false;
  std::uint32_t target = 0, engine = 0, captured = 0, running = 0;
  if (!readU32(f, target) || target > 4 || !readU32(f, engine) || engine > Engine::kPlaybackAlgorithmCount ||
      !readLfoAnchor(f, c.anchor) || !readU32(f, c.seed) ||
      !readU32(f, c.cycle) || !readU32(f, captured) || captured > 1 ||
      !readF32(f, c.outputAnchor) || !readU32(f, running) || running > 1)
    return false;
  c.target = int(target);
  c.engine = int(engine) - 1;
  c.captured = captured;
  c.runningAnchor = running;
  for (auto &r : c.poolRoutes) {
    std::uint32_t part = 0, alg = 0, dest = 0;
    if (!readU32(f, part) || part > 5 || !readU32(f, alg) || alg > Engine::kPlaybackAlgorithmCount ||
        !readU32(f, dest) || dest >= s3g_thru_lfo::destinations.size() ||
        !readF32(f, r.value))
      return false;
    r.id = {int(part) - 1, int(alg) - 1, int(dest)};
    r.from = r.value;
  }
  return s3g_thru_lfo::valid(c);
}
static bool writePooledLfos(std::FILE *f,
                            const std::array<ClipSlot, kClipSlotCount> &clips) {
  bool ok = writeU32(f, 0x3250464cu);
  for (const auto &clip : clips) {
    for (const auto &c : clip.lfos)
      ok = ok && writePoolConfig(f, c);
    for (const auto &lane : clip.sequence.tracks)
      for (int set = 0; set < 2; ++set)
        for (const auto &step : set ? lane.generator.base : lane.trigs) {
          const auto &l = step.lfos;
          ok = ok && s3g_thru_lfo::valid(l) && writeU32(f, l.mask) &&
               writeU32(f, l.reset);
          for (int i = 0; i < 8; ++i)
            ok = ok && writeF32(f, l.rates[i]) && writeF32(f, l.depths[i]);
        }
  }
  return ok;
}
static bool readPooledLfos(std::FILE *f,
                           std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t tag = 0;
  if (!readU32(f, tag) || tag != 0x3250464cu)
    return false;
  for (auto &clip : clips) {
    for (auto &c : clip.lfos)
      if (!readPoolConfig(f, c))
        return false;
    for (auto &lane : clip.sequence.tracks)
      for (int set = 0; set < 2; ++set)
        for (auto &step : set ? lane.generator.base : lane.trigs) {
          std::uint32_t mask = 0, reset = 0;
          if (!readU32(f, mask) || mask > 65535 || !readU32(f, reset) ||
              reset > 255)
            return false;
          auto &l = step.lfos;
          l.mask = std::uint16_t(mask);
          l.reset = std::uint8_t(reset);
          for (int i = 0; i < 8; ++i)
            if (!readF32(f, l.rates[i]) || !readF32(f, l.depths[i]))
              return false;
          if (!s3g_thru_lfo::valid(l))
            return false;
        }
  }
  return true;
}
// Format 49 appends AUTO state; the preceding format-48 pool stays
// byte-identical.
static bool
writeLfoAutoExtension(std::FILE *f,
                      const std::array<ClipSlot, kClipSlotCount> &clips) {
  bool ok = writeU32(f, 0x3154414cu);
  for (const auto &clip : clips)
    for (const auto &c : clip.lfos) {
      ok = ok && s3g_thru_lfo::valid(c) && writeU32(f, c.offset) &&
           writeF32(f, c.spanAnchor);
      for (const auto &r : c.poolRoutes)
        ok = ok && writeF32(f, r.automatic) && writeF32(f, r.span);
    }
  return ok;
}
static bool readLfoAutoExtension(std::FILE *f,
                                 std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t tag = 0;
  if (!readU32(f, tag) || tag != 0x3154414cu)
    return false;
  for (auto &clip : clips)
    for (auto &c : clip.lfos) {
      std::uint32_t offset = 0;
      if (!readU32(f, offset) || offset > 1 || !readF32(f, c.spanAnchor))
        return false;
      c.offset = int(offset);
      for (auto &r : c.poolRoutes) {
        if (!readF32(f, r.automatic) || !readF32(f, r.span))
          return false;
        r.autoFrom = r.automatic;
        r.spanFrom = r.span;
      }
      if (!s3g_thru_lfo::valid(c))
        return false;
    }
  return true;
}
static bool readLfoAutoJson(const char *json, s3g_thru_lfo::Pool &pool) {
  const char *at = std::strstr(json, "\"lfo_auto\"");
  if (!at)
    return true;
  at = std::strchr(at, ':');
  if (!at)
    return false;
  ++at;
  const auto token = [&](char c) {
    while (*at == ' ' || *at == '\n' || *at == '\r' || *at == '\t')
      ++at;
    if (*at != c)
      return false;
    ++at;
    return true;
  };
  if (!token('['))
    return false;
  for (int i = 0; i < 8; ++i) {
    if ((i && !token(',')) || !token('['))
      return false;
    double n[18]{};
    for (int j = 0; j < 18; ++j) {
      if (j && !token(','))
        return false;
      char *end = nullptr;
      n[j] = std::strtod(at, &end);
      if (end == at || !std::isfinite(n[j]))
        return false;
      at = end;
    }
    if (!token(']') || n[0] < 0 || n[0] > 1 || n[0] != std::round(n[0]))
      return false;
    auto &c = pool[i];
    c.offset = int(n[0]);
    c.spanAnchor = float(n[1]);
    for (int j = 0; j < 8; ++j) {
      auto &r = c.poolRoutes[j];
      r.automatic = r.autoFrom = float(n[2 + j * 2]);
      r.span = r.spanFrom = float(n[3 + j * 2]);
    }
    if (!s3g_thru_lfo::valid(c))
      return false;
  }
  return token(']');
}
bool readControlLfoJson(const char *json) {
  if (!readLegacyControlLfoJson(json))
    return false;
  const char *at = std::strstr(json, "\"lfo_pool\"");
  if (!at) {
    s3g_thru_lfo::Pool migrated{};
    for (int p = 0; p < controlLfoPartCount(); ++p)
      for (int i = 0; i < 2; ++i)
        migrated[p * 2 + i] = migrateControlLfoConfig(
            settingsForPart(p).lfos[i], p,
            int(liveControlsForPart(p).algorithm), splitEnabled);
    controlLfoPool = migrated;
    return true;
  }
  at = std::strchr(at, ':');
  if (!at)
    return false;
  ++at;
  const auto whitespace = [&]() {
    while (*at == ' ' || *at == '\n' || *at == '\r' || *at == '\t')
      ++at;
  };
  const auto token = [&](char c) {
    whitespace();
    if (*at != c)
      return false;
    ++at;
    return true;
  };
  if (!token('['))
    return false;
  s3g_thru_lfo::Pool pool{};
  for (int slot = 0; slot < 8; ++slot) {
    if (slot && !token(','))
      return false;
    if (!token('['))
      return false;
    double n[49]{};
    for (int j = 0; j < 49; ++j) {
      if (j && !token(','))
        return false;
      whitespace();
      char *end = nullptr;
      n[j] = std::strtod(at, &end);
      if (end == at || !std::isfinite(n[j]))
        return false;
      at = end;
    }
    if (!token(']'))
      return false;
    auto &c = pool[slot];
    for (int f = 0; f < 9; ++f)
      c.v[f] = float(n[f]);
    const auto integer = [&](int i, double lo, double hi) {
      return n[i] >= lo && n[i] <= hi && n[i] == std::floor(n[i]);
    };
    if (!integer(9, 0, 4) || !integer(10, -1, int(Engine::kPlaybackAlgorithmCount)-1) ||
        !integer(12, 0, 4294967295.) || !integer(13, 0, 4294967295.) ||
        !integer(14, 0, 1) || !integer(16, 0, 1))
      return false;
    c.target = int(n[9]);
    c.engine = int(n[10]);
    c.anchor = n[11];
    c.seed = std::uint32_t(n[12]);
    c.cycle = std::uint32_t(n[13]);
    c.captured = n[14] != 0;
    c.outputAnchor = float(n[15]);
    c.runningAnchor = n[16] != 0;
    for (int i = 0; i < 8; ++i) {
      int o = 17 + i * 4;
      if (!integer(o, -1, 4) || !integer(o + 1, -1, int(Engine::kPlaybackAlgorithmCount)-1) ||
          !integer(o + 2, 0, s3g_thru_lfo::destinations.size() - 1))
        return false;
      c.poolRoutes[i].id = {int(n[o]), int(n[o + 1]), int(n[o + 2])};
      c.poolRoutes[i].value = c.poolRoutes[i].from = float(n[o + 3]);
    }
    if (!s3g_thru_lfo::valid(c))
      return false;
  }
  if (!token(']'))
    return false;
  if (!readLfoAutoJson(json, pool))
    return false;
  controlLfoPool = pool;
  float selected = 0;
  if (jsonNumber(json, "lfo_edit", selected))
    controlLfoEdit = clampValue(int(selected), 0, 7);
  return true;
}
