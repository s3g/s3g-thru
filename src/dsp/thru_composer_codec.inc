// Worker-only conversion. Stable core values overlay the optional native image.
using ComposerJson = s3g_thru_json::Value;
static ComposerJson composerObject(
    std::initializer_list<std::pair<const std::string, ComposerJson>> values) {
  return ComposerJson::Object(values);
}
static int composerMode(const ClipSlot &c) {
  // Split/Quad preferences survive in a Single clip for its return mode.
  // Only active layout flags determine the visible instrument/part count.
  return c.sequence.active ? 6 : c.layers.active ? 5 : !c.splitEnabled ? 0
      : c.bankMode ? 4 : c.quadMode ? 3 : c.sharedDual ? 2 : 1;
}
static bool composerPanelSetting(const char* key) {
  if(!std::strncmp(key,"layers_",7))return true;
  static constexpr const char* keys[]{"pad_mode","voice_mode","hold_release_ms","scale","pad_octave","pressure_mode","lane_variation","part_mute","stack_scan_paused","glitch_buffer_lock","polyphony_comp",
    "voice_engine_mode","voice_engine_order","voice_engine_edit","voice_engine_assign","voice_engine_depth","voice_engine_octave","voice_engine_position","voice_engine_stack","voice_engine_pan",
    "voice_engine_1","voice_engine_2","voice_engine_3","voice_engine_4","voice_engine_5","voice_engine_6","voice_engine_7","voice_engine_8"};
  for(const auto* candidate:keys)if(!std::strcmp(key,candidate))return true;
  return false;
}
static double composerNumber(const ComposerJson &v, const char *key,
                             double fallback) {
  const auto *x = v.find(key);
  return x ? x->number() : fallback;
}
static bool composerBoolean(const ComposerJson &v, const char *key,
                            bool fallback) {
  const auto *x = v.find(key);
  return x ? x->boolean() : fallback;
}
static ComposerJson composerDuration(bool timed, float value) {
  return composerObject({{"unit", timed ? "ms" : "steps"},
                         {"value", std::clamp(double(value), timed ? 10.0 : .01,
                                              timed ? 30000.0 : 32.0)}});
}
static float *composerProgramValue(ClipSlot &c, int id, int group, int lane) {
#define COMPOSER_PROGRAM(base, name)                                           \
  if (id >= base && id < base + s3g_thru_##name::fields)                       \
    return &c.name##Programs[group].patches[lane].v[id - base];
  COMPOSER_PROGRAM(4200, table);
  COMPOSER_PROGRAM(4300, scanned);
  COMPOSER_PROGRAM(4400, fm);
  COMPOSER_PROGRAM(4500, noise);
  COMPOSER_PROGRAM(4600, reson);
  COMPOSER_PROGRAM(4800, partials);
  COMPOSER_PROGRAM(5000, lowform);
#undef COMPOSER_PROGRAM
  if (id == 5091)
    return &c.resonPrograms[group].density;
  return nullptr;
}
static double composerBoundary(float x) {
  char text[32]{};
  std::snprintf(text, sizeof(text), "%.7g", double(x));
  return std::strtod(text, nullptr);
}
static ComposerJson composerParameters(const ClipSlot &clip, int part) {
  auto out = ComposerJson::object();
  const bool split = clip.splitEnabled;
  const int group = split ? part + 1 : 0,
            lane = split && !clip.sharedDual ? (clip.quadMode ? part : part * 2)
                                             : 0;
  for (const auto &p : s3g_thru_midi::parameters) {
    if (!s3g_thru_pack::soundParameter(p))
      continue;
    if (p.lock >= 0)
      out[p.key] =
          std::clamp(double((part < 0 ? clip.baseValues
                                      : split ? clip.splitValues[part]
                                              : clip.baseValues)[p.lock]),
                     composerBoundary(p.minimum), composerBoundary(p.maximum));
    else if (part >= 0)
      if (const auto *v = composerProgramValue(const_cast<ClipSlot &>(clip),
                                               p.id, group, lane))
        out[p.key] = std::clamp(double(*v), composerBoundary(p.minimum),
                                composerBoundary(p.maximum));
  }
  return out;
}
static void composerReadParameters(ClipSlot &c, const ComposerJson &values,
                                   int part) {
  for (const auto &[key, v] : values.objectValue()) {
    const auto *p = s3g_thru_pack::parameter(key);
    s3g_thru_pack::need(p && s3g_thru_pack::soundParameter(*p), key,
                        "unsupported parameter");
    if (p->lock >= 0) {
      if (part < 0)
        c.baseValues[p->lock] = float(v.number());
      else
        (c.splitEnabled ? c.splitValues[part] : c.baseValues)[p->lock] =
            float(v.number());
    } else if (part >= 0) {
      const int group = c.splitEnabled ? part + 1 : 0,
                first = c.splitEnabled && !c.sharedDual
                            ? (c.quadMode ? part : part * 2)
                            : 0,
                last = c.splitEnabled && !c.sharedDual
                           ? (c.quadMode ? part : first + 1)
                           : 3;
      const auto *prior = composerProgramValue(c, p->id, group, first);
      if (!prior || *prior == float(v.number()))
        continue;
      for (int l = first; l <= last; ++l)
        if (auto *x = composerProgramValue(c, p->id, group, l))
          *x = float(v.number());
    }
  }
}
static ComposerJson
composerLocks(const s3g_thru_sequence::Values &v, bool timed,
              const s3g_thru_sequence::SoundLocks *sounds = nullptr,
              int lane = 0, int step = 0, int child = 0) {
  auto j = ComposerJson::object();
  if (v.mask & 1)
    j["position"] = v.values[0];
  if (v.mask & 2)
    j["pitch"] = v.values[1];
  if (v.mask & 4)
    j["velocity"] = double(v.values[2]) / 127;
  if (timed && v.timeLocked)
    j["duration"] = composerDuration(true, v.timeMs);
  else if (!timed && (v.mask & 8))
    j["duration"] = composerDuration(false, v.values[3]);
  if (sounds) {
    auto s = ComposerJson::object();
    const auto route = s3g_thru_sequence::SoundLocks::route(lane, step, child);
    for (int i = sounds->lower(route);
         i < sounds->count && sounds->records[i].route == route; ++i) {
      const auto &x = sounds->records[i];
      for (const auto &p : s3g_thru_midi::parameters)
        if (p.id == x.id) {
          s[p.key] = x.value;
          break;
        }
    }
    if (!s.objectValue().empty())
      j["sound"] = std::move(s);
  }
  return j;
}
static void composerReadLocks(s3g_thru_sequence::Values &v,
                              const ComposerJson &j, bool timed,
                              s3g_thru_sequence::SoundLocks *sounds = nullptr,
                              int lane = 0, int step = 0, int child = 0) {
  v.mask = 0;
  v.timeLocked = false;
  const char *keys[]{"position", "pitch", "velocity"};
  for (int i = 0; i < 3; ++i)
    if (const auto *x = j.find(keys[i])) {
      v.mask |= 1 << i;
      v.values[i] = float(x->number() * (i == 2 ? 127 : 1));
    }
  if (const auto *d = j.find("duration")) {
    if (timed) {
      v.timeLocked = true;
      v.timeMs = float(d->at("value").number());
    } else {
      v.mask |= 8;
      v.values[3] = float(d->at("value").number());
    }
  }
  if (const auto *s = j.find("sound")) {
    s3g_thru_pack::need(sounds || s->objectValue().empty(), "locks.sound",
                        "no sound-lock storage");
    for (const auto &[key, x] : s->objectValue())
      s3g_thru_pack::need(sounds->set(lane, step, child,
                                      s3g_thru_pack::parameter(key)->id,
                                      float(x.number())),
                          "sound locks", "clip or trig capacity exceeded");
  }
}
static ComposerJson
composerTrig(const s3g_thru_sequence::Step &t, bool timed,
             const s3g_thru_sequence::SoundLocks *sounds = nullptr,
             int lane = 0, int step = -1) {
  auto j =
      composerObject({{"enabled", t.enabled},
                      {"microtime", t.micro},
                      {"probability", t.probability},
                      {"cycle", composerObject({{"hit", int(t.cycleHit)},
                                                {"every", int(t.cycleEvery)}})},
                      {"locks", composerLocks(t.locks, timed, sounds, lane,
                                              std::max(0, step), 0)}});
  if (step >= 0)
    j["step"] = step;
  auto children = ComposerJson::array();
  for (int i = 0; i < t.count; ++i)
    children.push(composerObject(
        {{"offset", t.events[i].when},
         {"locks", composerLocks(t.events[i].locks, timed, sounds, lane,
                                 std::max(0, step), i + 1)}}));
  j["burst"] = std::move(children);
  return j;
}
static void composerReadTrig(s3g_thru_sequence::Step &t, const ComposerJson &j,
                             bool timed,
                             s3g_thru_sequence::SoundLocks *sounds = nullptr,
                             int lane = 0, int step = 0) {
  // Native LFO locks remain in their own scope when a core trig is edited.
  t.enabled = composerBoolean(j, "enabled", false);
  t.micro = float(composerNumber(j, "microtime", 0));
  t.probability = float(composerNumber(j, "probability", 1));
  t.cycleEvery = 1;
  t.cycleHit = 1;
  if (const auto *c = j.find("cycle")) {
    t.cycleEvery = std::uint8_t(c->at("every").number());
    t.cycleHit = std::uint8_t(c->at("hit").number());
  }
  if (const auto *l = j.find("locks"))
    composerReadLocks(t.locks, *l, timed, sounds, lane, step, 0);
  else
    t.locks = {};
  t.count = 1;
  t.events = {};
  if (const auto *b = j.find("burst")) {
    t.count = int(b->arrayValue().size());
    for (int i = 0; i < t.count; ++i) {
      const auto &child = b->arrayValue()[i];
      t.events[i].when = float(child.at("offset").number());
      if (const auto *l = child.find("locks"))
        composerReadLocks(t.events[i].locks, *l, timed, sounds, lane, step,
                          i + 1);
    }
  }
}
static ComposerJson composerMix(const s3g_thru_sequence::Mix &m) {
  auto j = composerObject({{"priority", m.priority},
                           {"depthDb", m.depthDb},
                           {"thresholdDb", m.thresholdDb},
                           {"attackMs", m.attackMs},
                           {"holdMs", m.holdMs},
                           {"releaseMs", m.releaseMs},
                           {"hpfHz", m.hpfHz},
                           {"kneeDb", m.kneeDb}}),
       strips = ComposerJson::array();
  for (const auto &s : m.strips)
    strips.push(composerObject({{"pan", s.pan},
                                {"lowDb", s.lowDb},
                                {"midDb", s.midDb},
                                {"highDb", s.highDb},
                                {"midHz", s.midHz},
                                {"solo", s.solo},
                                {"muted", s.muted}}));
  j["strips"] = std::move(strips);
  return j;
}
static void composerReadMix(s3g_thru_sequence::Mix &m, const ComposerJson &j) {
#define COMPOSER_MIX(x) m.x = decltype(m.x)(composerNumber(j, #x, m.x));
  COMPOSER_MIX(priority);
  COMPOSER_MIX(depthDb);
  COMPOSER_MIX(thresholdDb);
  COMPOSER_MIX(attackMs);
  COMPOSER_MIX(holdMs);
  COMPOSER_MIX(releaseMs);
  COMPOSER_MIX(hpfHz);
  COMPOSER_MIX(kneeDb);
#undef COMPOSER_MIX
  if (const auto *ss = j.find("strips")) {
    for (int i = 0; i < 4; ++i) {
      auto &s = m.strips[i];
      const auto &v = ss->arrayValue()[i];
#define COMPOSER_STRIP(x) s.x = float(composerNumber(v, #x, s.x));
      COMPOSER_STRIP(pan);
      COMPOSER_STRIP(lowDb);
      COMPOSER_STRIP(midDb);
      COMPOSER_STRIP(highDb);
      COMPOSER_STRIP(midHz);
#undef COMPOSER_STRIP
      s.solo = composerBoolean(v, "solo", false);
      s.muted = composerBoolean(v, "muted", false);
    }
  }
}
static ComposerJson composerSequence(const ClipSlot &c) {
  auto tracks = ComposerJson::array();
  for (int l = 0; l < 4; ++l) {
    const auto &t = c.sequence.tracks[l];
    auto trigs = ComposerJson::array();
    for (int s = 0; s < 32; ++s)
      trigs.push(
          composerTrig(t.trigs[s], t.timeMode, &c.sequence.soundLocks, l, s));
    auto defaults = composerObject(
        {{"position", c.splitValues[l][int(LockParameter::locus)]},
         {"pitch", 0},
         {"velocity", double(t.velocity) / 127},
         {"duration",
          composerDuration(t.timeMode, t.timeMode ? t.timeMs : t.gate)}});
    auto j =
        composerObject({{"length", t.length},
                        {"rate", s3g_thru_pack::rates[t.rate]},
                        {"rotation", t.rotation},
                        {"direction", s3g_thru_pack::directions[t.direction]},
                        {"path", s3g_thru_pack::paths[t.path]},
                        {"seed", double(t.directionSeed)},
                        {"enabled", t.enabled},
                        {"scaleEntry", t.scaleEntry},
                        {"defaults", std::move(defaults)},
                        {"trigs", std::move(trigs)}});
    const auto &g = t.generator;
    auto values = ComposerJson::array(), base = ComposerJson::array();
    for (int i = 0; i < 21; ++i)
      values.push((i == 17 || i == 18) ? std::max(.01, double(g.values[i]))
                                       : double(g.values[i]));
    for (const auto &s : g.base)
      base.push(composerTrig(s, g.timed));
    j["generator"] = composerObject(
        {{"type", g.type == 1 ? "euclidean" : g.type == 2 ? "onset" : "normal"},
         {"shift", g.shift},
         {"ready", g.ready},
         {"timed", g.timed},
         {"values", std::move(values)},
         {"base", std::move(base)},
         {"enabledMask", double(g.enabledMask)},
         {"enabledValues", double(g.enabledValues)},
         {"burstMask", double(g.burstMask)}});
    tracks.push(std::move(j));
  }
  return composerObject(
      {{"tracks", std::move(tracks)}, {"mix", composerMix(c.sequence.mix)}});
}
static void composerReadSequence(ClipSlot &c, const ComposerJson &j) {
  c.sequence.initialized = true;
  c.sequence.soundLocks.count = 0;
  for (int l = 0; l < 4; ++l) {
    const auto &a = j.at("tracks").arrayValue()[l];
    auto &t = c.sequence.tracks[l];
    t.length = int(a.at("length").number());
    t.rate = s3g_thru_pack::choice(a.at("rate"), "rate", s3g_thru_pack::rates);
    t.rotation = int(composerNumber(a, "rotation", 0));
    t.directionSeed = std::uint32_t(composerNumber(a, "seed", 0));
    t.direction = s3g_thru_pack::optChoice(a, "direction", "track", 0,
                                           s3g_thru_pack::directions, 5);
    t.path = s3g_thru_pack::optChoice(a, "path", "track", 0,
                                      s3g_thru_pack::paths, 3);
    t.enabled = composerBoolean(a, "enabled", true);
    t.scaleEntry = composerBoolean(a, "scaleEntry", false);
    t.timeMode = true;
    t.velocity = 100;
    t.timeMs = 200;
    if (const auto *d = a.find("defaults")) {
      t.velocity = float(composerNumber(*d, "velocity", 100.0 / 127) * 127);
      if (const auto *dur = d->find("duration")) {
        t.timeMode = dur->at("unit").string() == "ms";
        (t.timeMode ? t.timeMs : t.gate) = float(dur->at("value").number());
      }
      if (const auto *p = d->find("position"))
        c.splitValues[l][int(LockParameter::locus)] = float(p->number());
    }
    // Missing trigs are off; preserve native-only per-trig LFO data.
    for (auto &s : t.trigs) {
      const auto locks = s.lfos;
      s = {};
      s.lfos = locks;
    }
    for (const auto &x : a.at("trigs").arrayValue())
      composerReadTrig(t.trigs[int(x.at("step").number())], x, t.timeMode,
                       &c.sequence.soundLocks, l, int(x.at("step").number()));
    if (const auto *g = a.find("generator")) {
      auto &v = t.generator;
      v.type = g->at("type").string() == "euclidean"
                   ? 1
                   : g->at("type").string() == "onset" ? 2 : 0;
      v.shift = int(composerNumber(*g, "shift", 0));
      v.ready = composerBoolean(*g, "ready", false);
      v.timed = composerBoolean(*g, "timed", false);
      v.enabledMask = std::uint32_t(composerNumber(*g, "enabledMask", 0));
      v.enabledValues = std::uint32_t(composerNumber(*g, "enabledValues", 0));
      v.burstMask = std::uint32_t(composerNumber(*g, "burstMask", 0));
      if (const auto *x = g->find("values"))
        for (int i = 0; i < 21; ++i)
          v.values[i] = float(x->arrayValue()[i].number());
      if (const auto *x = g->find("base"))
        for (int i = 0; i < 32; ++i)
          composerReadTrig(v.base[i], x->arrayValue()[i], v.timed);
    } else
      t.generator = {};
  }
  if (const auto *m = j.find("mix"))
    composerReadMix(c.sequence.mix, *m);
  c.sequence.modeMixes[6] = c.sequence.mix;
  c.sequence.mixerMode = 6;
}
static ComposerJson composerClip(const ClipSlot &c, int index) {
  using namespace s3g_thru_pack;
  const int mode = composerMode(c),
            parts =
                (mode == 0 || mode == 5) ? 1 : mode == 3 || mode == 6 ? 4 : 2;
  auto j = composerObject({{"id", "clip-" + std::to_string(index + 1)},
                           {"name", c.name.data()},
                           {"mode", modes[mode]},
                           {"parameters", composerParameters(c, -1)}});
  auto ss = ComposerJson::array(), ps = ComposerJson::array(),
       notes = ComposerJson::array(), curves = ComposerJson::array();
  for (int l = 0; l < 4; ++l)
    ss.push(composerObject(
        {{"asset", c.laneSourcePaths[l][0]
                       ? ComposerJson(c.laneSourcePaths[l].data() + 6)
                       : ComposerJson()},
         {"label", c.laneSourceLabels[l].data()}}));
  for (int p = 0; p < parts; ++p) {
    const auto &setting =
        c.splitEnabled ? c.splitSettings[p] : c.singleSettings;
    const auto &v = c.splitEnabled ? c.splitValues[p] : c.baseValues;
    ps.push(composerObject(
        {{"engine",
          engines[std::clamp(int(v[int(LockParameter::algorithm)]), 0, 21)]},
         {"root", setting.root},
         {"scale", setting.scale},
         {"parameters", composerParameters(c, p)}}));
  }
  for (int n = 0; n < c.clipNoteCount; ++n) {
    const auto &x = c.clipNotes[n];
    notes.push(composerObject(
        {{"timeMs", x.startMs},
         {"durationMs", x.durationMs},
         {"note", int(x.note)},
         {"velocity", int(x.velocity)},
         {"pad", int(x.sourcePad)},
         {"part", x.part ? int(x.part) - 1 : 0},
         {"take", int(x.take)},
         {"engineEntry", int(x.engineSlot)},
         {"engine", x.engineAlgorithm < 22
                        ? ComposerJson(engines[x.engineAlgorithm])
                        : ComposerJson()},
         {"pressure", x.initialPressure},
         {"variation", composerObject({{"octave", x.engineVariation.octave},
                                       {"position", x.engineVariation.position},
                                       {"stack", x.engineVariation.stack},
                                       {"pan", x.engineVariation.pan}})}}));
  }
  std::map<std::tuple<int, int, int, int>, ComposerJson> groups;
  for (int n = 0; n < c.clipPointCount; ++n) {
    const auto &x = c.clipPoints[n];
    auto key = std::make_tuple(int(x.type), x.parameter & 255, x.sourcePad,
                               clipPointTake(x));
    auto found = groups.find(key);
    if (found == groups.end()) {
      auto target = composerObject({{"type", pointTypes[int(x.type)]},
                                    {"index", x.parameter & 255},
                                    {"owner", int(x.sourcePad)},
                                    {"take", int(clipPointTake(x))}});
      if (x.type == ClipPointType::parameter ||
          x.type == ClipPointType::partParameter)
        for (const auto &p : s3g_thru_midi::parameters)
          if (p.lock == (x.parameter & 255)) {
            target["key"] = p.key;
            break;
          }
      found =
          groups
              .emplace(key, composerObject({{"target", std::move(target)},
                                            {"points", ComposerJson::array()}}))
              .first;
    }
    found->second["points"].push(
        ComposerJson::Array{ComposerJson(x.timeMs), ComposerJson(x.value)});
  }
  for (auto &[key, curve] : groups) {
    (void)key;
    auto &values = curve["points"].arrayValue();
    std::stable_sort(
        values.begin(), values.end(), [](const auto &a, const auto &b) {
          return a.arrayValue()[0].number() < b.arrayValue()[0].number();
        });
    auto windows=ComposerJson::array();for(int i=0;i<c.touchCount;++i){const auto& w=c.touchSpans[i];
      const auto [type,parameter,owner,take]=key;ClipPoint route;route.type=static_cast<ClipPointType>(type);route.parameter=clipPointDescriptor(parameter,take);route.sourcePad=std::uint8_t(owner);if(w.lane==clipPointLane(route)&&w.take==take)windows.push(ComposerJson::Array{ComposerJson(w.startMs),ComposerJson(w.durationMs)});}
    if(!windows.arrayValue().empty())curve["windows"]=std::move(windows);
    curves.push(std::move(curve));
  }
  j["sources"] = std::move(ss);
  j["parts"] = std::move(ps);
  j["notes"] = std::move(notes);
  j["automation"] = std::move(curves);
  j["loop"] = composerObject(
      {{"durationMs", std::max(1u, c.clipDurationMs)},
       {"beats", (c.recordedBeats > 0 ? double(c.recordedBeats) : 4.0)},
       {"rate", c.clipRate},
       {"recordedTempo", c.recordedTempoBpm},
       {"timing", c.timingMode == ClipTimingMode::beatSync ? "beats" : "free"},
       {"takes", int(c.takeCount)},
       {"recorded", c.clipRecorded}});
  if (mode == 6)
    j["sequence"] = composerSequence(c);
  return j;
}
static bool composerValidPoint(const ClipPoint &x, int takes) {
  const int f = x.parameter & 255;
  const float value = x.value;
  if (!std::isfinite(value) || clipPointTake(x) >= std::max(1, takes))
    return false;
  if (clipPointLane(x) >= kClipCurveLaneCount &&
      !(x.type == ClipPointType::layersBaseline && x.sourcePad < 6 &&
        x.sourcePad * 192 + f < int(LayerPatch::size)))
    return false;
#define COMPOSER_POINT(pointType, name)                                        \
  if (x.type == ClipPointType::pointType)                                      \
    return f < s3g_thru_##name::fields && value >= s3g_thru_##name::low[f] &&  \
           value <= s3g_thru_##name::high[f] &&                                \
           (!s3g_thru_##name::discrete(f) || value == std::floor(value));
  COMPOSER_POINT(tableParameter, table);
  COMPOSER_POINT(scannedParameter, scanned);
  COMPOSER_POINT(fmParameter, fm) COMPOSER_POINT(noiseParameter, noise);
  COMPOSER_POINT(resonParameter, reson)
  COMPOSER_POINT(partialsParameter, partials);
  COMPOSER_POINT(lowformParameter, lowform);
#undef COMPOSER_POINT
  if (x.type == ClipPointType::resonDensity)
    return value >= 0 && value <= 100;
  if (x.type == ClipPointType::controlLfo)
    return validControlLfoPoint(f, value);
  if (x.type == ClipPointType::pooledLfo)
    return validPooledLfoPoint(f, value);
  if (x.type == ClipPointType::sequenceRecipe)
    return validSequenceRecipePoint(f, value);
  if (x.type == ClipPointType::notePressure)
    return value >= 0 && value <= 1;
  if (x.type == ClipPointType::midiExpression)
    return value >= (f == 0 ? -24.f : 0.f) && value <= (f == 0 ? 24.f : 1.f);
  if (x.type == ClipPointType::parameter ||
      x.type == ClipPointType::partParameter) {
    if(f==int(LockParameter::wavesetProcess))return value==std::floor(value)&&value>=0&&value<=10&&float(Engine::sanitizeWavesetProcess(static_cast<Engine::WavesetProcess>(int(value))))==value;
    for (const auto &p : s3g_thru_midi::parameters)
      if (p.lock == f)
        return value >= p.minimum && value <= p.maximum &&
               (!p.integer || value == std::floor(value));
    return false;
  }
  if (x.type == ClipPointType::layersParameter ||
      x.type == ClipPointType::layersBaseline)
    return LayerPatch::bound(x.sourcePad * 192 + f, value) == value;
  if (x.type == ClipPointType::sequenceLane) {
    static constexpr float low[]{1, 0, 0, 0, 0, 1, .01f, 0, 0, 10, 0, 0},
        high[]{32, 8, 31, 1, 1, 127, 32, 1, 1, 30000, 4, 2};
    return f < 12 && value >= low[f] && value <= high[f] &&
           ((f == 6 || f == 9) || value == std::floor(value));
  }
  return true;
}

static void composerReadClip(ClipSlot &c, const ComposerJson &j, float tempo) {
  using namespace s3g_thru_pack;
  const int mode = choice(j.at("mode"), "mode", modes);
  c.occupied = true;
  c.splitEnabled = mode != 0 && mode != 5;
  c.splitInitialized = c.splitInitialized || c.splitEnabled;
  if(c.splitEnabled){c.sharedDual = mode == 2 || mode == 4;c.quadMode = mode == 3 || mode == 6;}
  c.quadInitialized = c.quadInitialized || c.quadMode;
  c.bankMode = mode == 4;
  c.layers.active = mode == 5;
  c.sequence.active = mode == 6;
  if (c.splitEnabled)
    c.splitFocus =
        std::clamp(c.splitFocus, 0, (mode == 3 || mode == 6) ? 3 : 1);
  std::snprintf(c.name.data(), c.name.size(), "%s",
                j.at("name").string().c_str());
  if (const auto *p = j.find("parameters"))
    composerReadParameters(c, *p, -1);
  const auto &parts = j.at("parts").arrayValue();
  for (int p = 0; p < int(parts.size()); ++p) {
    const auto &part = parts[p];
    if (const auto *v = part.find("parameters"))
      composerReadParameters(c, *v, p);
    auto &setting = c.splitEnabled ? c.splitSettings[p] : c.singleSettings;
    setting.root = int(composerNumber(part, "root", 60));
    setting.scale = int(composerNumber(part, "scale", 0));
    (c.splitEnabled ? c.splitValues[p]
                    : c.baseValues)[int(LockParameter::algorithm)] =
        float(choice(part.at("engine"), "engine", engines));
  }
  for (int l = 0; l < 4; ++l) {
    const auto &source = j.at("sources").arrayValue()[l];
    const auto *a = source.find("asset");
    const auto previousPath = c.laneSourcePaths[l];
    const auto previousKind = c.laneSourceKinds[l];
    std::snprintf(c.laneSourcePaths[l].data(), c.laneSourcePaths[l].size(),
                  "%s",
                  a && !a->isNull() ? ("asset:" + a->string()).c_str() : "");
    const auto *label = source.find("label");
    std::snprintf(c.laneSourceLabels[l].data(), 64, "%s",
                  label ? label->string().c_str() : "");
    c.laneSourceKinds[l] =
        a && !a->isNull()
            ? (previousPath == c.laneSourcePaths[l] ? previousKind
                                                    : LaneSourceKind::file)
            : LaneSourceKind::empty;
  }
  if (const auto *q = j.find("sequence"))
    composerReadSequence(c, *q);
  c.clipNoteCount = 0;
  if (const auto *notes = j.find("notes"))
    for (const auto &n : notes->arrayValue()) {
      auto &x = c.clipNotes[c.clipNoteCount++];
      x = {};
      x.startMs = std::uint32_t(n.at("timeMs").number());
      x.durationMs = std::uint32_t(n.at("durationMs").number());
      x.note = std::uint8_t(n.at("note").number());
      x.velocity = std::uint8_t(n.at("velocity").number());
      x.sourcePad = std::uint8_t(composerNumber(n, "pad", 255));
      x.take = std::uint8_t(composerNumber(n, "take", 0));
      x.part =
          c.splitEnabled ? std::uint8_t(composerNumber(n, "part", 0) + 1) : 0;
      x.engineSlot = std::uint8_t(composerNumber(n, "engineEntry", 255));
      if (const auto *e = n.find("engine"); e && !e->isNull())
        x.engineAlgorithm = std::uint8_t(choice(*e, "note.engine", engines));
      x.initialPressure = float(composerNumber(n, "pressure", 0));
      if (const auto *v = n.find("variation")) {
        x.engineVariation.octave = int(composerNumber(*v, "octave", 0));
        x.engineVariation.position = float(composerNumber(*v, "position", 0));
        x.engineVariation.stack = float(composerNumber(*v, "stack", 0));
        x.engineVariation.pan = float(composerNumber(*v, "pan", 0));
      }
    }
  c.clipPointCount = 0;c.touchCount=0;c.touchLanes.reset();
  if (const auto *curves = j.find("automation"))
    for (const auto &curve : curves->arrayValue()) {
      const auto &t = curve.at("target");
      const auto type =
          ClipPointType(choice(t.at("type"), "curve.type", pointTypes));
      const int parameter = int(composerNumber(t, "index", 0)),
                owner = int(composerNumber(t, "owner", 255)),
                take = int(composerNumber(t, "take", 0));
      if (const auto *key = t.find("key")) {
        const auto *p = s3g_thru_pack::parameter(key->string());
        need((type == ClipPointType::parameter ||
              type == ClipPointType::partParameter) &&
                 p && p->lock == parameter,
             "curve.key", "key and recorded parameter identity disagree");
      }
      if(const auto* windows=curve.find("windows"))for(const auto& window:windows->arrayValue()){
        need(c.touchCount<c.touchSpans.size(),"curve.windows","too many touch windows");ClipPoint route;route.type=type;route.parameter=std::uint16_t(parameter);route.sourcePad=std::uint8_t(owner);const auto lane=clipPointLane(route);need(lane<kClipCurveLaneCount,"curve.windows","invalid touch target");c.touchSpans[c.touchCount++]={std::uint16_t(lane),std::uint8_t(take),std::uint32_t(window.arrayValue()[0].number()),std::uint32_t(window.arrayValue()[1].number())};
      }
      for (const auto &point : curve.at("points").arrayValue()) {
        ClipPoint x;
        x.type = type;
        x.parameter = std::uint16_t(parameter | (take << 8));
        x.sourcePad = std::uint8_t(owner);
        x.timeMs = std::uint32_t(point.arrayValue()[0].number());
        x.value = float(point.arrayValue()[1].number());
        need(clipPointLane(x) < kClipCurveLaneCount ||
                 (type == ClipPointType::layersBaseline && owner < 6 &&
                  owner * 192 + parameter < int(LayerPatch::size)),
             "curve.target", "invalid ownership");
        c.clipPoints[c.clipPointCount++] = x;
      }
    }
  c.recordedTempoBpm = tempo;
  c.recordedBeats = 4;
  c.clipDurationMs = std::uint32_t(std::lround(240000 / tempo));
  c.clipRate = 1;
  c.timingMode = ClipTimingMode::beatSync;
  c.takeCount = 1;
  c.clipRecorded = mode == 6 || c.clipNoteCount || c.clipPointCount;
  if (const auto *loop = j.find("loop")) {
    c.clipDurationMs =
        std::uint32_t(composerNumber(*loop, "durationMs", c.clipDurationMs));
    c.recordedBeats = float(composerNumber(*loop, "beats", 4));
    c.recordedTempoBpm = float(composerNumber(*loop, "recordedTempo", tempo));
    c.clipRate = float(composerNumber(*loop, "rate", 1));
    c.takeCount = std::uint8_t(composerNumber(*loop, "takes", 1));
    c.clipRecorded = composerBoolean(*loop, "recorded", true);
    c.timingMode = loop->find("timing") && loop->at("timing").string() == "free"
                       ? ClipTimingMode::free
                       : ClipTimingMode::beatSync;
  }
  for (int n = 0; n < c.clipNoteCount; ++n)
    s3g_thru_pack::need(c.clipNotes[n].take < std::max(1, int(c.takeCount)),
                        "note.take", "take exceeds clip take count");
  for (int n = 0; n < c.clipPointCount; ++n)
    s3g_thru_pack::need(composerValidPoint(c.clipPoints[n], c.takeCount),
                        "curve", "invalid value, target or take");
  for(int n=0;n<c.touchCount;++n){const auto& window=c.touchSpans[n];need(window.take<std::max(1,int(c.takeCount))&&window.startMs<c.clipDurationMs&&window.durationMs>0&&window.durationMs<=kClipMaximumDurationMs,"curve.windows","touch interval exceeds clip or take limits");}
  rebuildClipPointIndex(c);
}
static void composerDefaultClip(ClipSlot &c) {
  Engine::Controls defaults;
  for (std::size_t p = 0; p < kLockParameterCount; ++p) {
    c.baseValues[p] = lockedValue(defaults, 1, p);
    for (auto &part : c.splitValues)
      part[p] = c.baseValues[p];
  }
  for (auto &t : c.sequence.tracks) {
    t.timeMode = true;
    t.timeMs = 200;
  }
  c.sequence.initialized = true;
}
static ComposerJson composerFade(const ChainFadeSettings &f, int mode) {
  return composerObject(
      {{"mode", mode == 0 ? "default"
                          : mode == 1 ? "cut" : mode == 2 ? "fixed" : "random"},
       {"timeMs", f.time},
       {"lowMs", f.low},
       {"highMs", f.high},
       {"curve",
        f.curve == 0
            ? "linear"
            : f.curve == 1
                  ? "in"
                  : f.curve == 2 ? "out" : f.curve == 3 ? "smooth" : "power"}});
}
static void composerReadFade(ChainFadeSettings &f, const ComposerJson &j) {
  const auto &mode = j.at("mode").string();
  f.mode = mode == "random" ? 1 : 0;
  f.time = float(composerNumber(j, "timeMs", 0));
  f.low = float(composerNumber(j, "lowMs", 2000));
  f.high = float(composerNumber(j, "highMs", 12000));
  const auto *c = j.find("curve");
  const std::string curve = c ? c->string() : "linear";
  f.curve = curve == "in"
                ? 1
                : curve == "out"
                      ? 2
                      : curve == "smooth" ? 3 : curve == "power" ? 4 : 0;
}
