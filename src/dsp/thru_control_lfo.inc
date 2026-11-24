// Shared eight-slot modulation pool. Rendering only changes effective
// snapshots.
s3g_thru_lfo::Part legacyControlLfos{};
s3g_thru_lfo::Pool controlLfoPool{}, controlLfoRecordBases{};
std::array<s3g_thru_lfo::Runtime, 8> controlLfoRuntime{};
std::array<s3g_thru_lfo::Config, 8> controlLfoEffective{};
std::array<std::array<bool, 11>, 8> controlLfoOverrides{};
int controlLfoEdit = 0;
bool controlLfoRouted = false;
int controlLfoPartCount() const {
  return splitEnabled && !layersLive.active ? partCount() : 1;
}
static int controlLfoKey(const char *key, int &field) {
  int slot = 0, used = 0;
  char name[16]{};
  if (std::strncmp(key, "lfo", 3) ||
      std::sscanf(key, "lfo%d_%15s%n", &slot, name, &used) != 2 || key[used] ||
      slot < 1 || slot > 8)
    return -1;
  const char *names[]{"dest",  "wave",   "clock", "rate",   "depth", "mode",
                      "phase", "smooth", "flow",  "target", "engine", "offset"};
  for (int f = 0; f < 12; ++f)
    if (!std::strcmp(name, names[f])) {
      field = f;
      return slot - 1;
    }
  return -1;
}
static float controlLfoField(const s3g_thru_lfo::Config &c, int field) {
  return field < 9 ? c.v[field] : float(field == 9 ? c.target : field == 10 ? c.engine : c.offset);
}
static bool setControlLfoField(s3g_thru_lfo::Config &c, int field, float v) {
  if (!std::isfinite(v) || field < 0 || field > 11)
    return false;
  if (field < 9)
    return s3g_thru_lfo::set(c, field, v);
  if (field == 9)
    c.target = clampValue(int(std::lround(v)), 0, 4);
  else if(field==10)
    c.engine = clampValue(int(std::lround(v)), -1, int(Engine::kPlaybackAlgorithmCount)-1);
  else c.offset = clampValue(int(std::lround(v)),0,1);
  return true;
}
static int controlLfoDestinationForKey(const char *key) {
  for (std::size_t i = 0; i < s3g_thru_lfo::destinations.size(); ++i)
    if (!std::strcmp(key, s3g_thru_lfo::destinations[i].key))
      return int(i);
  return 0;
}
const Engine::Controls &controlLfoProgram(int part) const {
  return clipPlaying ? (splitEnabled ? splitClipControls[part] : clipControls)
                     : liveControlsForPart(part);
}
bool controlLfoHasEngine(int part, int algorithm) const {
  if (part < 0 || part >= controlLfoPartCount() || (bankMode && part == 1))
    return false;
  if (int(liveControlsForPart(part).algorithm) == algorithm ||
      (clipPlaying && int(controlLfoProgram(part).algorithm) == algorithm))
    return true;
  const auto cfg = settingsForPart(part).engines;
  if (cfg.enabled && cfg.configured)
    for (int i = 0; i < engineSlotCount(); ++i)
      if (cfg.engines[i] == algorithm)
        return true;
  for (const auto &v : voices)
    if (v.active && v.part == part &&
        int(v.engine.playbackAlgorithm()) == algorithm)
      return true;
  return false;
}
bool controlLfoDestinationAvailable(const s3g_thru_lfo::Config &c,
                                    int dest) const {
  using namespace s3g_thru_lfo;
  if (dest < 0 || dest >= int(destinations.size()))
    return false;
  if (!dest)
    return true;
  const auto &d = destinations[dest];
  if (!(d.modes & (1u << performanceMode())))
    return false;
  const bool global = d.domain == Domain::shared || d.domain == Domain::layers;
  if (global)
    return c.target == 4;
  if (c.target < 0 || c.target >= controlLfoPartCount())
    return false;
  if (d.domain != Domain::engine)
    return true;
  const auto &program = controlLfoProgram(c.target);
  if (!std::strcmp(d.key, "stretch_ratio") && !program.lanesStretch)
    return false;
  if (!std::strcmp(d.key, "morph_frequency") && program.morphMode == 1)
    return false;

  if (d.condition==15 && !sequenceMode()) {
    const auto &settings=settingsForPart(c.target);
    if(settings.mode==int(PadMode::slices) || settings.mode==int(PadMode::rolls))return false;
  }
  const auto acceptsEngine=[&](int algorithm) {
    return d.condition!=15 || algorithm!=int(Engine::PlaybackAlgorithm::glitch)
        || (!program.glitchBufferLocked && !settingsForPart(c.target).glitchHold);
  };
  if(d.parameter>=s3g_thru_lowform::controlOffset&&d.parameter<s3g_thru_lowform::controlOffset+s3g_thru_lowform::fields){bool used=false;const int first=splitEnabled&&!sharedDual?firstLaneForPart(c.target):0,last=splitEnabled&&!sharedDual?first+(isQuad()?0:1):3;
    for(int lane=first;lane<=last;++lane)used|=s3g_thru_lowform::applicable(program.lowform.patches[lane],d.parameter-s3g_thru_lowform::controlOffset);
    if(!used)return false;
  }
  if(d.parameter>=s3g_thru_reson::controlOffset&&d.parameter<s3g_thru_reson::controlOffset+s3g_thru_reson::fields){bool used=false;const int first=splitEnabled&&!sharedDual?firstLaneForPart(c.target):0,last=splitEnabled&&!sharedDual?first+(isQuad()?0:1):3;
    for(int lane=first;lane<=last;++lane)used|=s3g_thru_reson::applicable(program.reson.patches[lane],d.parameter-s3g_thru_reson::controlOffset);
    if(!used)return false;
  }
  if(d.parameter>=s3g_thru_partials::controlOffset&&d.parameter<s3g_thru_partials::controlOffset+s3g_thru_partials::fields){bool used=false;const int first=splitEnabled&&!sharedDual?firstLaneForPart(c.target):0,last=splitEnabled&&!sharedDual?first+(isQuad()?0:1):3;
    for(int lane=first;lane<=last;++lane)used|=s3g_thru_partials::applicable(program.partials.patches[lane],d.parameter-s3g_thru_partials::controlOffset);
    if(!used)return false;
  }
  if(d.parameter>=s3g_thru_noise::controlOffset&&d.parameter<s3g_thru_noise::controlOffset+s3g_thru_noise::fields){bool used=false;const int first=splitEnabled&&!sharedDual?firstLaneForPart(c.target):0,last=splitEnabled&&!sharedDual?first+(isQuad()?0:1):3;
    for(int lane=first;lane<=last;++lane)used|=s3g_thru_noise::applicable(program.noise.patches[lane],d.parameter-s3g_thru_noise::controlOffset);
    if(!used)return false;
  }
  if(d.parameter>=s3g_thru_fm::controlOffset&&d.parameter<s3g_thru_fm::controlOffset+s3g_thru_fm::fields){bool used=false;const int first=splitEnabled&&!sharedDual?firstLaneForPart(c.target):0,last=splitEnabled&&!sharedDual?first+(isQuad()?0:1):3;
    for(int lane=first;lane<=last;++lane)used|=s3g_thru_fm::applicable(program.fm.patches[lane],d.parameter-s3g_thru_fm::controlOffset);
    if(!used)return false;
  }
  if(d.parameter>=s3g_thru_scanned::controlOffset&&d.parameter<s3g_thru_scanned::controlOffset+s3g_thru_scanned::fields){bool used=false;const int first=splitEnabled&&!sharedDual?firstLaneForPart(c.target):0,last=splitEnabled&&!sharedDual?first+(isQuad()?0:1):3;
    for(int lane=first;lane<=last;++lane)used|=s3g_thru_scanned::applicable(program.scanned.patches[lane],d.parameter-s3g_thru_scanned::controlOffset);
    if(!used)return false;
  }
  if(d.parameter>=s3g_thru_table::controlOffset&&d.parameter<s3g_thru_table::controlOffset+s3g_thru_table::fields){bool used=false;const int first=splitEnabled&&!sharedDual?firstLaneForPart(c.target):0,last=splitEnabled&&!sharedDual?first+(isQuad()?0:1):3;
    for(int lane=first;lane<=last;++lane)used|=s3g_thru_table::applicable(program.table.patches[lane],d.parameter-s3g_thru_table::controlOffset);
    if(!used)return false;}
  if(d.condition>=16&&d.condition<=19){bool used=false;const int first=splitEnabled&&!sharedDual?firstLaneForPart(c.target):0,last=splitEnabled&&!sharedDual?first+(isQuad()?0:1):3;
    for(int lane=first;lane<=last;++lane){const auto& p=program.table.patches[lane];used|=d.condition==19?p.v[s3g_thru_table::art]>.5f:int(p.v[s3g_thru_table::type])==d.condition-16;}if(!used)return false;}
  const int sound = int(program.motionSound);
  switch (d.condition) {
  case 1: {
    bool used = int(program.sampleMode) == 2;
    if (!used) {
      if (c.engine >= 0)
        used = c.engine != 4 && c.engine != 5;
      else
        for (int alg = 0; alg < int(Engine::kPlaybackAlgorithmCount); ++alg)
          if (alg != 4 && alg != 5 && controlLfoHasEngine(c.target, alg))
            used = true;
    }
    if (!used)
      return false;
    break;
  }
  case 2:
    if (sound != 1 && sound != 2)
      return false;
    break;
  case 3:
    if (sound < 1)
      return false;
    break;
  case 4:
    if (sound != 2)
      return false;
    break;
  case 5:
    if (sound < 2)
      return false;
    break;
  case 6:
    if (sound < 3 || int(program.segmentTrigger) != 0)
      return false;
    break;
  case 7:
    if (sound < 3)
      return false;
    break;
  case 8:
    if (int(program.grainProcess) == 0)
      return false;
    break;
  case 9:
    if (int(program.cutClock) != 0)
      return false;
    break;
  case 10:
    if (int(program.glitchClock) != 0)
      return false;
    break;
  case 11:
    if (program.wavesetProcess == Engine::WavesetProcess::repeat)
      return false;
    break;
  case 12:
    if (int(program.prismMode) != 4)
      return false;
    break;
  case 13:
    if (program.stochasticLaw >= 2.5f)
      return false;
    break;
  case 14:
    if (program.stochasticLaw < 2.5f)
      return false;
    break;
  default:
    break;
  }
  if (c.engine >= 0)
    return (d.engines & (1u << c.engine)) && acceptsEngine(c.engine) &&
           controlLfoHasEngine(c.target, c.engine);
  for (int alg = 0; alg < int(Engine::kPlaybackAlgorithmCount); ++alg)
    if ((d.engines & (1u << alg)) && acceptsEngine(alg) && controlLfoHasEngine(c.target, alg)) {
      if (!std::strcmp(d.key, "stretch_ratio") && !program.lanesStretch)
        continue;
      if (!std::strcmp(d.key, "morph_frequency") && program.morphMode == 1)
        continue;
      return true;
    }
  return false;
}
s3g_thru_lfo::Config controlLfoConfiguration(int slot,
                                             double position = -1) const {
  auto c = controlLfoPool[slot];
  const auto *clip = activeClipSlot();
  if (clip && !clipRecording && c.v[s3g_thru_lfo::flow] == 0 &&
      (clipPlaying || position >= 0))
    for (int f = 0; f < 11; ++f) {
      if (f == s3g_thru_lfo::flow || controlLfoOverrides[slot][f])
        continue;
      float x = 0;
      if (sampleClipCurve(*clip, kClipPooledLfoOffset + slot * 11 + f,
                          position < 0 ? clipPositionMs : position,
                          f < 9 && !s3g_thru_lfo::integerField(f) &&
                              f != s3g_thru_lfo::phase,
                          x))
        setControlLfoField(c, f, x);
    }
  c.seed ^= s3g_thru_lfo::hash(std::uint32_t(slot + 1));
  return c;
}
void resetControlLfos(bool fresh) {
  if (!fresh)
    return;
  controlLfoOverrides = {};
  for (int i = 0; i < 8; ++i) {
    auto c = controlLfoConfiguration(i, 0);
    if (c.v[s3g_thru_lfo::flow] == 0)
      controlLfoRuntime[i].reset(c);
  }
}
void advanceControlLfos(std::size_t count) {
  using namespace s3g_thru_lfo;
  controlLfoRouted = false;
  const double rate = activeClipSlot() ? activeClipSlot()->clipRate : 1;
  for (int i = 0; i < 8; ++i) {
    auto c = controlLfoConfiguration(i);
    controlLfoEffective[i] = c;
    auto &r = controlLfoRuntime[i];
    r.advance(c, int(count), sampleRate, tempoBpm, rate,
              clipPlaying || clipRecording);
    const int dest =
        std::clamp(int(c.v[destination]), 0, int(destinations.size()) - 1);
    const auto &d = destinations[dest];
    r.route(c, controlLfoDestinationAvailable(c, dest),
            d.domain == Domain::engine ? c.engine : -1);
    controlLfoRouted |= r.routing();
  }
}
float controlLfoOffset(int target, int dest, int algorithm = -1,
                       float fraction = 1) const {
  if (!controlLfoRouted)
    return 0;
  float result = 0;
  for (const auto &r : controlLfoRuntime)
    for (const auto &route : r.routes)
      if (route.id.part == target && route.id.dest == dest &&
          (route.id.engine < 0 || route.id.engine == algorithm))
        result += route.from + (route.value - route.from) * fraction;
  return clampValue(result, -1.f, 1.f);
}
s3g_thru_lfo::Modulation controlLfoModulation(int target,int dest,int algorithm=-1,float fraction=1) const {
  s3g_thru_lfo::Modulation m{};
  if(!controlLfoRouted)return m;
  for(const auto&r:controlLfoRuntime)for(const auto&route:r.routes)
    if(route.id.part==target&&route.id.dest==dest&&(route.id.engine<0||route.id.engine==algorithm)) {
      const float automatic=route.autoFrom+(route.automatic-route.autoFrom)*fraction;
      m.routed=true;
      m.base+=route.from+(route.value-route.from)*fraction-automatic;
      m.automatic+=automatic;m.span+=route.spanFrom+(route.span-route.spanFrom)*fraction;
    }
  return m;
}
static float controlLfoMapped(int dest,float base,float amount) {
  return s3g_thru_lfo::mapped(s3g_thru_lfo::destinations[dest],base,{amount,0,0});
}
float controlLfoPositionValue(const Engine::Controls &c,int part,float base,
                              s3g_thru_lfo::Modulation m) const {
  if(!m.routed)return base;
  auto d=s3g_thru_lfo::destinations[127];
  if(!sequenceMode()) {
    const auto window=parameterDisplayWindow(c,part);
    const float width=clampValue(window[1]-window[0],.02f,1.f);
    d.low=width*.5f;d.high=1-width*.5f;
    base=clampValue(base,d.low,d.high);
  }
  return s3g_thru_lfo::mapped(d,base,m);
}
void applyControlLfoWindow(Engine::Controls &c,int part,bool position=true) const {
  c.sampleWindowModulation=false;
  if(!controlLfoRouted||Engine::sourceFree(c.algorithm)||c.algorithm==Engine::PlaybackAlgorithm::slices||(bankMode&&part==1))return;
  const int alg=int(c.algorithm);
  const auto field=controlLfoModulation(part,128,alg);
  const auto pos=controlLfoModulation(part,127,alg);
  if(field.routed)c.field=s3g_thru_lfo::mapped(s3g_thru_lfo::destinations[128],c.field,field);
  if(position)c.locus=controlLfoPositionValue(c,part,c.locus,pos);
  c.sampleWindowModulation=field.routed||(position&&pos.routed);
}
float controlLfoValueForKey(int target,const char*key,float base,int algorithm=-1,float fraction=1) const {
  const int id=controlLfoDestinationForKey(key);
  if(id&& !std::strcmp(key,"partials_count")){const auto c=controlLfoProgram(target);const auto& patch=c.partials.patches[tableEditLane(target)];if(patch.v[s3g_thru_partials::type]>1.5f){auto bounded=s3g_thru_lfo::destinations[id];bounded.high=8;return s3g_thru_lfo::mapped(bounded,std::min(8.f,base),controlLfoModulation(target,id,algorithm,fraction));}}
  if(id==127) {
    auto c=controlLfoProgram(target);
    c.field=s3g_thru_lfo::mapped(s3g_thru_lfo::destinations[128],c.field,controlLfoModulation(target,128,algorithm,fraction));
    return controlLfoPositionValue(c,target,base,controlLfoModulation(target,id,algorithm,fraction));
  }
  return id?s3g_thru_lfo::mapped(s3g_thru_lfo::destinations[id],base,controlLfoModulation(target,id,algorithm,fraction)):base;
}
void applyControlLfoControls(Engine::Controls &c, int part,
                             s3g_thru_lfo::Domain domain) const {
  using namespace s3g_thru_lfo;
  if(domain==Domain::engine)c.sampleWindowModulation=false;
  if (!controlLfoRouted)
    return;
  std::array<Modulation, destinations.size()> amounts{};
  for (const auto &lfo : controlLfoRuntime)
    for (const auto &r : lfo.routes) {
      if (r.id.part != part || r.id.dest <= 0)
        continue;
      const auto &d = destinations[r.id.dest];
      if (d.domain != domain || d.parameter < 0 ||
          !(d.modes & (1u << performanceMode())))
        continue;
      if (domain == Domain::engine &&
          (!(d.engines & (1u << int(c.algorithm))) ||
           (r.id.engine >= 0 && r.id.engine != int(c.algorithm))))
        continue;
      amounts[r.id.dest].routed = true;
      amounts[r.id.dest].base += r.value-r.automatic;
      amounts[r.id.dest].automatic += r.automatic;
      amounts[r.id.dest].span += r.span;
    }
  float ignored = outputGain;
  for (std::size_t i = 1; i < amounts.size(); ++i)
    if (i!=127 && i!=128 && (amounts[i].base != 0 || amounts[i].automatic != 0 || amounts[i].span > 0)) {
      const auto &d = destinations[i];
      if(d.parameter>=s3g_thru_lowform::controlOffset&&d.parameter<s3g_thru_lowform::controlOffset+s3g_thru_lowform::fields){const int field=d.parameter-s3g_thru_lowform::controlOffset;
        for(auto& patch:c.lowform.patches)if(s3g_thru_lowform::applicable(patch,field))s3g_thru_lowform::set(patch,field,mapped(d,patch.v[field],amounts[i]));
      }else if(d.parameter>=s3g_thru_reson::controlOffset&&d.parameter<s3g_thru_reson::controlOffset+s3g_thru_reson::fields){const int field=d.parameter-s3g_thru_reson::controlOffset;
        for(auto& patch:c.reson.patches)if(s3g_thru_reson::applicable(patch,field))s3g_thru_reson::set(patch,field,mapped(d,patch.v[field],amounts[i]));
      }else if(d.parameter>=s3g_thru_partials::controlOffset&&d.parameter<s3g_thru_partials::controlOffset+s3g_thru_partials::fields){const int field=d.parameter-s3g_thru_partials::controlOffset;
        for(auto& patch:c.partials.patches)if(s3g_thru_partials::applicable(patch,field)){auto bounded=d;if(field==s3g_thru_partials::count&&patch.v[s3g_thru_partials::type]>1.5f)bounded.high=8;s3g_thru_partials::set(patch,field,mapped(bounded,s3g_thru_partials::resolved(patch,field),amounts[i]));}
      }else if(d.parameter>=s3g_thru_noise::controlOffset&&d.parameter<s3g_thru_noise::controlOffset+s3g_thru_noise::fields){const int field=d.parameter-s3g_thru_noise::controlOffset;
        for(auto& patch:c.noise.patches)if(s3g_thru_noise::applicable(patch,field))s3g_thru_noise::set(patch,field,mapped(d,patch.v[field],amounts[i]));
      }else if(d.parameter>=s3g_thru_fm::controlOffset&&d.parameter<s3g_thru_fm::controlOffset+s3g_thru_fm::fields){const int field=d.parameter-s3g_thru_fm::controlOffset;
        for(auto& patch:c.fm.patches)if(s3g_thru_fm::applicable(patch,field))s3g_thru_fm::set(patch,field,mapped(d,s3g_thru_fm::resolved(patch,field),amounts[i]));
      }else if(d.parameter>=s3g_thru_scanned::controlOffset&&d.parameter<s3g_thru_scanned::controlOffset+s3g_thru_scanned::fields){const int field=d.parameter-s3g_thru_scanned::controlOffset;
        for(auto& patch:c.scanned.patches)if(s3g_thru_scanned::applicable(patch,field))s3g_thru_scanned::set(patch,field,mapped(d,patch.v[field],amounts[i]));
      }else if(d.parameter>=s3g_thru_table::controlOffset&&d.parameter<s3g_thru_table::controlOffset+s3g_thru_table::fields){const int field=d.parameter-s3g_thru_table::controlOffset;
        for(auto& patch:c.table.patches)if(s3g_thru_table::applicable(patch,field))s3g_thru_table::set(patch,field,mapped(d,patch.v[field],amounts[i]));
      }else applyLockedValue(c, ignored, d.parameter,mapped(d,lockedValue(c, ignored, d.parameter),amounts[i]));
    }
  if(domain==Domain::engine) {
    if(amounts[128].routed)c.field=mapped(destinations[128],c.field,amounts[128]);
    if(!sequenceMode())c.locus=controlLfoPositionValue(c,part,c.locus,amounts[127]);
    c.sampleWindowModulation=amounts[128].routed||(!sequenceMode()&&amounts[127].routed);
  }
}
void applyControlLfoVoice(Engine::Controls &c,
                          const PlaybackVoice &voice) const {
  applyControlLfoControls(c, voice.part, s3g_thru_lfo::Domain::engine);
}
float controlLfoOutputGain(int part, float base) const {
  return splitEnabled ? base : controlLfoValueForKey(part, "part_level", base);
}
float controlLfoPan(int part, float base) const {
  return controlLfoValueForKey(part, "part_pan", base);
}
float controlLfoLayerBase(bool y) const {
  const auto &patch = clipPlaying ? layersRecorded : layersLive;
  float v = 0;
  for (int c = 0; c < 4; ++c)
    v += patch.values[LayerPatch::config + LayerPatch::weight0 + c] *
         float(y ? c / 2 : c % 2);
  return v;
}
s3g_thru_layers::Point controlLfoLayerOffset() const {
  const float x = controlLfoLayerBase(false), y = controlLfoLayerBase(true);
  return {controlLfoValueForKey(4, "layers_fx_x", x) - x,
          controlLfoValueForKey(4, "layers_fx_y", y) - y};
}
void applyControlLfoMixer(Engine::Controls &mixer) const {
  if (!controlLfoRouted)
    return;
  float ignored = outputGain;
  for (int p = 0; p < controlLfoPartCount(); ++p) {
    const std::size_t ids[]{partLevelParameter(p), partDelayParameter(p),
                            partReverbParameter(p)};
    const char *keys[]{"part_level", "part_delay", "part_reverb"};
    for (int i = 0; i < 3; ++i)
      applyLockedValue(mixer, ignored, ids[i],
                       controlLfoValueForKey(
                           p, keys[i], lockedValue(mixer, ignored, ids[i])));
  }
  applyControlLfoControls(mixer, 4, s3g_thru_lfo::Domain::shared);
}
static bool validControlLfoPoint(int parameter, float value) {
  const int field = parameter % 9;
  return parameter >= 0 && parameter < 18 && std::isfinite(value) &&
         value >= s3g_thru_lfo::low[field] &&
         value <= (field == 0 ? 14 : s3g_thru_lfo::high[field]) &&
         (!s3g_thru_lfo::integerField(field) || value == std::round(value));
}
static bool validPooledLfoPoint(int field, float value) {
  if (field < 0 || field > 10 || field == s3g_thru_lfo::flow ||
      !std::isfinite(value))
    return false;
  if (field >= 9)
    return value == std::round(value) && value >= (field == 9 ? 0 : -1) &&
           value <= (field == 9 ? 4 : int(Engine::kPlaybackAlgorithmCount)-1);
  return value >= s3g_thru_lfo::low[field] &&
         value <= s3g_thru_lfo::high[field] &&
         (!s3g_thru_lfo::integerField(field) || value == std::round(value));
}
static int sequenceLfoKey(const char *key, int &field) {
  int slot = 0, used = 0;
  char name[16]{};
  if (std::strncmp(key, "seq_lfo", 7) ||
      std::sscanf(key, "seq_lfo%d_%15s%n", &slot, name, &used) != 2 ||
      key[used] || slot < 1 || slot > 8)
    return -1;
  const char *names[]{"reset", "rate", "depth"};
  for (int f = 0; f < 3; ++f)
    if (!std::strcmp(name, names[f])) {
      field = f;
      return slot - 1;
    }
  return -1;
}
bool sequenceLfoParameterValue(const char *key, int part, float &value) const {
  int f = 0, slot = sequenceLfoKey(key, f);
  if (slot < 0)
    return false;
  part = clampValue(part, 0, 3);
  const auto &l =
      sequenceLive.tracks[part]
          .trigs[part == splitFocus ? sequenceStep : sequenceSteps[part]]
          .lfos;
  value = f == 0 ? float((l.reset >> slot) & 1)
                 : f == 1 ? (l.mask & (1u << (slot * 2))
                                 ? l.rates[slot]
                                 : controlLfoPool[slot].v[s3g_thru_lfo::rate])
                          : (l.mask & (2u << (slot * 2))
                                 ? l.depths[slot]
                                 : controlLfoPool[slot].v[s3g_thru_lfo::depth]);
  return true;
}
bool setSequenceLfoParam(const char *key, const char *text) {
  int f = 0, slot = sequenceLfoKey(key, f);
  const bool clear = !std::strcmp(key, "seq_lfo_clear");
  if (slot < 0 && !clear)
    return false;
  if (!sequenceMode())
    return true;
  if (!clear && std::strcmp(text, "reset") &&
      !std::isfinite(parseFloat(text, NAN)))
    return true;
  sequenceChanged();
  auto &l = sequenceLive.tracks[splitFocus].trigs[sequenceStep].lfos;
  if (clear)
    l = {};
  else if (!std::strcmp(text, "reset")) {
    if (f == 0)
      l.reset |= std::uint8_t(1u << slot);
    else
      l.mask &= ~std::uint16_t((f == 1 ? 1u : 2u) << (slot * 2));
  } else {
    float value = parseFloat(text, 0);
    if (f == 0) {
      if (value > .5)
        l.reset |= std::uint8_t(1u << slot);
      else
        l.reset &= ~std::uint8_t(1u << slot);
    } else if (f == 1) {
      auto c = controlLfoPool[slot];
      s3g_thru_lfo::set(c, s3g_thru_lfo::rate, value);
      l.rates[slot] = c.v[s3g_thru_lfo::rate];
      l.mask |= std::uint16_t(1u << (slot * 2));
    } else {
      l.depths[slot] = clampValue(value, -1.f, 1.f);
      l.mask |= std::uint16_t(2u << (slot * 2));
    }
  }
  commitSequenceEdit();
  return true;
}
void triggerControlLfos(int part,
                        const s3g_thru_lfo::TrigLocks *locks = nullptr,
                        bool restart = true) {
  if (!restart || part < 0 || part >= controlLfoPartCount())
    return;
  for (int i = 0; i < 8; ++i) {
    auto c = controlLfoConfiguration(i);
    if (c.target != part && c.target != 4)
      continue;
    auto &r = controlLfoRuntime[i];
    if (!r.initialized)
      r.reset(c);
    r.overrides = 0;
    if (locks) {
      if (locks->mask & (1u << (i * 2))) {
        r.rateOverride = locks->rates[i];
        r.overrides |= 1;
      }
      if (locks->mask & (2u << (i * 2))) {
        r.depthOverride = locks->depths[i];
        r.overrides |= 2;
      }
    }
    r.trigger(c, !locks || (locks->reset & (1u << i)));
    controlLfoEffective[i] = c;
  }
}
bool controlLfoParameterValue(const char *key, int part, float &value) const {
  if (!std::strcmp(key, "lfo_select")) {
    value = float(controlLfoEdit + 1);
    return true;
  }
  if (sequenceLfoParameterValue(key, part, value))
    return true;
  int f = 0, slot = controlLfoKey(key, f);
  if (slot < 0)
    return false;
  value = controlLfoField(controlLfoPool[slot], f);
  return true;
}
void seedControlLfoRecording() {
  if (!clipRecording)
    return;
  for (int i = 0; i < 8; ++i) {
    auto &c = controlLfoPool[i];
    const auto &r = controlLfoRuntime[i];
    if (clipReplacing) {
      c.anchor = r.position;
      c.cycle = r.cycle;
      c.captured = true;
      c.outputAnchor = r.output;
      c.runningAnchor = r.running;
      c.poolRoutes = r.routes;
      c.spanAnchor = r.spanEnvelope;
    }
    controlLfoRecordBases[i] = c;
    if (c.v[s3g_thru_lfo::destination] == 0)
      continue;
    for (int f = 0; f < 11; ++f)
      if (f != s3g_thru_lfo::flow &&
          (clipReplacing ||
           !activeClipSlot()
                ->clipPointCounts[kClipPooledLfoOffset + i * 11 + f]))
        captureClipPoint(ClipPointType::pooledLfo, f, i, controlLfoField(c, f),
                         0, true);
  }
  activeClipSlot()->lfos = controlLfoPool;
}
bool setControlLfoParam(const char *key, const char *text) {
  if (!std::strcmp(key, "lfo_select")) {
    controlLfoEdit = clampValue(parseInteger(text, 1) - 1, 0, 7);
    auto &selected = controlLfoPool[controlLfoEdit];
    bool unused = selected.v == s3g_thru_lfo::defaults && selected.engine == -1 && selected.offset == 0;
    if (const auto *clip = activeClipSlot())
      for (int f = 0; f < 11; ++f)
        unused &= clip->clipPointCounts[kClipPooledLfoOffset +
                                        controlLfoEdit * 11 + f] == 0;
    if (unused && selected.target != 4 &&
        selected.target >= controlLfoPartCount()) {
      const int target =
          splitEnabled && splitFocus < controlLfoPartCount() ? splitFocus : 0;
      char targetKey[24]{}, targetValue[16]{};
      std::snprintf(targetKey, sizeof(targetKey), "lfo%d_target",
                    controlLfoEdit + 1);
      std::snprintf(targetValue, sizeof(targetValue), "%d", target);
      setControlLfoParam(targetKey, targetValue);
    }
    return true;
  }
  if (setSequenceLfoParam(key, text))
    return true;
  int field = 0, slot = controlLfoKey(key, field);
  if (slot < 0)
    return false;
  auto &c = controlLfoPool[slot];
  const auto before = c;
  const float value = parseFloat(text, NAN);
  if (!std::isfinite(value)) {
    setError("Invalid LFO value");
    return true;
  }
  if (field == 0 &&
      !controlLfoDestinationAvailable(c, int(std::lround(value)))) {
    setError("LFO destination unavailable");
    return true;
  }
  if (!setControlLfoField(c, field, value))
    return true;
  if (clipRecording && field < 11 && field != s3g_thru_lfo::flow)
    for (int f = 0; f < 11; ++f)
      if (f != s3g_thru_lfo::flow &&
          !activeClipSlot()
               ->clipPointCounts[kClipPooledLfoOffset + slot * 11 + f])
        captureClipPoint(ClipPointType::pooledLfo, f, slot,
                         controlLfoField(controlLfoRecordBases[slot], f), 0,
                         true);
  for (int f = 0; f < 11; ++f)
    if (f != s3g_thru_lfo::flow &&
        controlLfoField(c, f) != controlLfoField(before, f)) {
      const auto curve = kClipPooledLfoOffset + slot * 11 + f;
      if (clipRecording) {
        controlLfoOverrides[slot][f] = false;
        captureClipPoint(ClipPointType::pooledLfo, f, slot,
                         controlLfoField(c, f), 0xffffffffu, true);
      } else if (sequenceMode()) {
        replaceSequenceAutomation(std::array<std::size_t, 1>{curve});
        controlLfoOverrides[slot][f] = false;
      } else
        controlLfoOverrides[slot][f] = clipPlaying;
    }
  if (field == s3g_thru_lfo::phase) {
    controlLfoRuntime[slot].position = std::fmod(double(c.v[field]), 1.);
    controlLfoRuntime[slot].cycle = 0;
    c.captured = false;
  }
  if (auto *clip = activeClipSlot())
    clip->lfos = controlLfoPool;
  markPersistentStateDirty();
  clearError();
  return true;
}
int formatControlLfo(char *out, int capacity) const {
  int at =
      std::snprintf(out, capacity, "{\"selected\":%d,\"part\":%d,\"values\":[",
                    controlLfoEdit, splitEnabled ? splitFocus : 0);
  const auto add = [&](const char *fmt, auto... v) {
    if (at < 0 || at >= capacity)
      return;
    int n = std::snprintf(out + at, capacity - at, fmt, v...);
    at = n < 0 || n >= capacity - at ? capacity : at + n;
  };
  for (int i = 0; i < 8; ++i) {
    add("%s[", i ? "," : "");
    for (int f = 0; f < 12; ++f)
      add("%s%.9g", f ? "," : "",
          double(controlLfoField(controlLfoPool[i], f)));
    add("%s", "]");
  }
  const auto &c = controlLfoPool[controlLfoEdit];
  add("%s", "],\"parts\":[0");
  for (int p = 1; p < controlLfoPartCount(); ++p)
    add(",%d", p);
  add("%s", ",4],\"allowed\":[0");
  for (int d = 1; d < int(s3g_thru_lfo::destinations.size()); ++d)
    if (controlLfoDestinationAvailable(c, d))
      add(",%d", d);
  add("],\"primary\":%d,\"engines\":[-1",
      c.target < controlLfoPartCount()
          ? int(controlLfoProgram(c.target).algorithm)
          : -1);
  if (c.target < controlLfoPartCount())
    for (int alg = 0; alg < int(Engine::kPlaybackAlgorithmCount); ++alg)
      if (controlLfoHasEngine(c.target, alg))
        add(",%d", alg);
  add("] ,\"motionSound\":%d,\"trig\":[",
      c.target < controlLfoPartCount()
          ? int(controlLfoProgram(c.target).motionSound)
          : 0);
  if (sequenceMode()) {
    const auto &l = sequenceLive.tracks[splitFocus].trigs[sequenceStep].lfos;
    add("%u,%u", unsigned(l.mask), unsigned(l.reset));
    for (int i = 0; i < 8; ++i)
      add(",%.9g,%.9g", double(l.rates[i]), double(l.depths[i]));
  }
  add("%s", "]}");
  if (at < 0 || at >= capacity) {
    out[0] = 0;
    return -1;
  }
  return at;
}
const char *controlLfoResolvedKey(int target, int dest) const {
  const auto &d = s3g_thru_lfo::destinations[dest];
  if (d.parameter >= 0)
    return d.key;
  if (d.parameter == -1)
    return splitEnabled ? kLockParameterKeys[partLevelParameter(target)]
                        : "output_gain";
  if (d.parameter == -2) {
    const char *keys[]{"seq_pan_1", "seq_pan_2", "seq_pan_3", "seq_pan_4"};
    return keys[target];
  }
  if (d.parameter == -3)
    return kLockParameterKeys[partDelayParameter(target)];
  if (d.parameter == -4)
    return kLockParameterKeys[partReverbParameter(target)];
  return d.key;
}
float controlLfoManualBase(int target, int dest) const {
  const auto &d = s3g_thru_lfo::destinations[dest];
  const bool global = target == 4;
  const auto &c =
      global ? (clipPlaying ? clipControls : controls)
             : (clipPlaying
                    ? (splitEnabled ? splitClipControls[target] : clipControls)
                    : liveControlsForPart(target));
  if (d.parameter == int(LockParameter::splitBlend))
    return lockedValue(effectiveSplitMixer(clipControls), outputGain,
                       d.parameter);
  if(d.parameter>=s3g_thru_lowform::controlOffset&&d.parameter<s3g_thru_lowform::controlOffset+s3g_thru_lowform::fields)return c.lowform.patches[tableEditLane(target)].v[d.parameter-s3g_thru_lowform::controlOffset];
  if(d.parameter>=s3g_thru_reson::controlOffset&&d.parameter<s3g_thru_reson::controlOffset+s3g_thru_reson::fields)return c.reson.patches[tableEditLane(target)].v[d.parameter-s3g_thru_reson::controlOffset];
  if(d.parameter>=s3g_thru_partials::controlOffset&&d.parameter<s3g_thru_partials::controlOffset+s3g_thru_partials::fields)return c.partials.patches[tableEditLane(target)].v[d.parameter-s3g_thru_partials::controlOffset];
  if(d.parameter>=s3g_thru_noise::controlOffset&&d.parameter<s3g_thru_noise::controlOffset+s3g_thru_noise::fields)return c.noise.patches[tableEditLane(target)].v[d.parameter-s3g_thru_noise::controlOffset];
  if(d.parameter>=s3g_thru_fm::controlOffset&&d.parameter<s3g_thru_fm::controlOffset+s3g_thru_fm::fields)return s3g_thru_fm::resolved(c.fm.patches[tableEditLane(target)],d.parameter-s3g_thru_fm::controlOffset);
  if(d.parameter>=s3g_thru_scanned::controlOffset&&d.parameter<s3g_thru_scanned::controlOffset+s3g_thru_scanned::fields)return c.scanned.patches[tableEditLane(target)].v[d.parameter-s3g_thru_scanned::controlOffset];
  if(d.parameter>=s3g_thru_table::controlOffset&&d.parameter<s3g_thru_table::controlOffset+s3g_thru_table::fields)return c.table.patches[tableEditLane(target)].v[d.parameter-s3g_thru_table::controlOffset];
  if (d.parameter >= 0)
    return lockedValue(c, clipPlaying ? clipOutputGain : outputGain,
                       d.parameter);
  if (d.parameter == -1)
    return splitEnabled ? lockedValue(effectiveSplitMixer(clipControls),
                                      outputGain, partLevelParameter(target))
                        : (clipPlaying ? clipOutputGain : outputGain);
  if (d.parameter == -2)
    return (sequenceMode() ? sequencePlayback.mix : sequenceLive.mix)
        .strips[target]
        .pan;
  if (d.parameter == -3 || d.parameter == -4)
    return lockedValue(effectiveSplitMixer(clipControls), outputGain,
                       d.parameter == -3 ? partDelayParameter(target)
                                         : partReverbParameter(target));
  return controlLfoLayerBase(d.parameter == -6);
}
int formatControlLfoLive(char *out, int capacity) const {
  using namespace s3g_thru_lfo;
  if (!controlLfoRouted)
    return std::snprintf(out, capacity,
                         "{\"owner\":%d,\"v\":[],\"phase\":[],\"output\":[]}",
                         splitEnabled ? splitFocus : 0);
  int at = std::snprintf(out, capacity, "{\"owner\":%d,\"v\":[",
                         splitEnabled ? splitFocus : 0);
  const auto add = [&](const char *fmt, auto... v) {
    if (at < 0 || at >= capacity)
      return;
    int n = std::snprintf(out + at, capacity - at, fmt, v...);
    at = n < 0 || n >= capacity - at ? capacity : at + n;
  };
  std::array<std::array<int, 2>, 24> seen{};
  int count = 0;
  const auto entry = [&](int target, int dest) {
    if (dest <= 0 || target < 0 || target > 4 ||
        (target < 4 && target >= controlLfoPartCount()))
      return;
    for (int i = 0; i < count; ++i)
      if (seen[i][0] == target && seen[i][1] == dest)
        return;
    if (count == 24)
      return;
    const auto &d = destinations[dest];
    if (!(d.modes & (1u << performanceMode())))
      return;
    int alg = target < 4 ? int(liveControlsForPart(target).algorithm) : -1;
    if (d.domain == Domain::engine && !(d.engines & (1u << alg)))
      return;
    if (d.domain == Domain::engine) {
      bool applies = false;
      for (const auto &runtime : controlLfoRuntime)
        for (const auto &route : runtime.routes)
          if (route.id.part == target && route.id.dest == dest &&
              (route.id.engine < 0 || route.id.engine == alg))
            applies = true;
      if (!applies)
        return;
    }

    seen[count++] = {target, dest};
    const float base = controlLfoManualBase(target, dest);
    add("%s[%d,\"%s\",%.7g,%.7g]", count > 1 ? "," : "", target,
        controlLfoResolvedKey(target, dest),
        double(controlLfoValueForKey(target,d.key,base,alg)),
        double(base));
  };
  // Current assignments precede fading former assignments; the packet stays
  // bounded.
  for (int i = 0; i < 8; ++i) {
    const auto &c = controlLfoEffective[i];
    if (controlLfoDestinationAvailable(c, int(c.v[0])))
      entry(c.target, int(c.v[0]));
  }
  for (const auto &r : controlLfoRuntime)
    for (const auto &route : r.routes)
      if (route.id.part >= 0)
        entry(route.id.part, route.id.dest);
  add("%s", "],\"phase\":[");
  for (int i = 0; i < 8; ++i)
    add("%s%.5g", i ? "," : "", controlLfoRuntime[i].position);
  add("%s", "],\"output\":[");
  for (int i = 0; i < 8; ++i)
    add("%s%.5g", i ? "," : "", double(controlLfoRuntime[i].output));
  add("%s", "]}");
  if (at < 0 || at >= capacity) {
    out[0] = 0;
    return -1;
  }
  return at;
}

#include "thru_control_lfo_legacy.inc"
#include "thru_control_lfo_pool_storage.inc"
// Direct assignment uses the same curated destination checks as the picker.
int modulationDestination(const char* key,int part,int& target)const{
 target=part;const char* name=key;
 if(!std::strcmp(key,"output_gain"))name="part_level";
 for(int p=0;p<4;++p){const std::string suffix(1,char('a'+p));
  if(std::string(key)=="split_level_"+suffix){name="part_level";target=p;}
  if(std::string(key)=="split_delay_"+suffix){name="part_delay";target=p;}
  if(std::string(key)=="split_reverb_"+suffix){name="part_reverb";target=p;}
  if(std::string(key)=="seq_pan_"+std::to_string(p+1)){name="part_pan";target=p;}
 }
 const int id=controlLfoDestinationForKey(name);if(!id)return 0;
 const auto domain=s3g_thru_lfo::destinations[id].domain;
 if(domain==s3g_thru_lfo::Domain::shared||domain==s3g_thru_lfo::Domain::layers)target=4;
 s3g_thru_lfo::Config probe;probe.target=target;probe.engine=-1;probe.v[0]=float(id);
 return controlLfoDestinationAvailable(probe,id)?id:0;
}
bool assignModulation(int part,int slot,int parameterId,int expectedDest,int expectedTarget,int algorithm){
 if(part<0||part>=controlLfoPartCount()||slot<0||slot>7||clipRecording||audioRecording()||patternBusy())return false;
 const auto* p=s3g_thru_midi::find(parameterId);if(!p||p->action||int(controlLfoProgram(part).algorithm)!=algorithm)return false;
 const auto& current=controlLfoPool[slot];if(int(current.v[0])!=expectedDest||current.target!=expectedTarget)return false;
 int target=part;const int dest=modulationDestination(p->key,part,target);if(!dest)return false;
 if(int(current.v[0])==dest&&current.target==target){controlLfoEdit=slot;return true;}
 pushClipUndo(activeClip);
 const auto write=[&](const char* field,int value){const auto key="lfo"+std::to_string(slot+1)+"_"+field;const auto text=std::to_string(value);setControlLfoParam(key.c_str(),text.c_str());};
 write("target",target);write("engine",-1);write("dest",dest);controlLfoEdit=slot;
 clearError();return true;
}
