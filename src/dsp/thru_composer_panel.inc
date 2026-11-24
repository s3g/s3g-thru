// The panel observes ordinary live clip state. It never owns a temporary bank.
std::unique_ptr<ClipSlot> composerPanelClip;
std::atomic<int> composerPanelJob{0};
std::array<char,65> composerPanelOwner{},composerPanelSnapshotOwner{};
std::array<bool,64> composerPanelOccupied{};
std::array<std::array<char,41>,64> composerPanelNames{};
std::array<int,64> composerPanelModes{};
std::array<std::atomic<std::uint64_t>,64> composerEditReceipts{};
std::array<std::array<std::atomic<char>,128>,64> composerEditErrors{};
std::atomic<std::uint64_t> composerEditVersion{0};
std::uint64_t composerPanelEditVersion=0;
std::atomic<bool> composerPanelWatching{false};
std::atomic<std::uint64_t> composerPanelOwnerGeneration{1};
std::uint64_t composerPanelCapturedGeneration=0;
std::uint64_t composerPanelAt=0,composerPanelRevision=0;
int composerPanelActive=-1,composerPanelSelectedLane=0,composerPanelFocus=0;
std::atomic<int> composerPanelLiveLane{0},composerPanelLiveFocus{0};
float composerPanelTempo=120;int composerPanelLfoEdit=0;
std::array<bool,s3g_thru_lfo::destinations.size()> composerPanelLfoAllowed{};
std::array<bool,22> composerPanelLfoEngines{};
bool composerPanelPolyphony=true;int composerPanelFxCorner=0;
std::string composerPanelError;
int composerPanelErrorClip=-1;
std::string composerPanelPath,composerPanelPublishedVersion;
std::array<WaveBins,4> composerPanelMinimum{},composerPanelMaximum{};
std::array<std::size_t,4> composerPanelFrames{};
std::array<float,4> composerPanelRates{};
std::atomic<std::uint64_t> composerPanelWaveVersion{0};
std::uint64_t composerPanelCapturedWaveVersion=0;
void captureComposerPanel() {
  if(composerPanelJob.load(std::memory_order_acquire)!=1||!composerPanelClip)return;
  auto& c=*composerPanelClip;
  composerPanelEditVersion=composerEditVersion.load(std::memory_order_acquire);
  composerPanelSelectedLane=int(selectedLane);composerPanelFocus=splitEnabled?splitFocus:0;
  composerPanelSnapshotOwner=composerPanelOwner;composerPanelCapturedGeneration=composerPanelOwnerGeneration.load();
  for(int i=0;i<64;++i){composerPanelOccupied[i]=clipSlots[i].occupied;composerPanelNames[i]=clipSlots[i].name;composerPanelModes[i]=composerMode(clipSlots[i]);}
  captureSplitClip(c);c.layers=layersLive;composerPanelFxCorner=layersSelectedCorner[1];
  composerPanelMinimum=waveMinimum;composerPanelMaximum=waveMaximum;composerPanelCapturedWaveVersion=composerPanelWaveVersion.load();
  for(int i=0;i<4;++i){composerPanelFrames[i]=engine.laneFrames(i);composerPanelRates[i]=engine.laneSampleRate(i);}
  c.laneSourcePaths=currentLaneSourcePaths;c.laneSourceKinds=currentLaneSourceKinds;c.laneSourceLabels=laneSourceLabels;
  if(const auto* source=activeClipSlot()){
    c.name=source->name;c.clipDurationMs=source->clipDurationMs;c.recordedTempoBpm=source->recordedTempoBpm;c.recordedBeats=source->recordedBeats;
    c.clipRate=source->clipRate;c.timingMode=source->timingMode;c.takeCount=source->takeCount;c.clipRecorded=source->clipRecorded;c.occupied=source->occupied;
  }
  composerPanelLfoEdit=controlLfoEdit;
  const auto& lfo=controlLfoPool[controlLfoEdit];
  for(std::size_t d=0;d<composerPanelLfoAllowed.size();++d)composerPanelLfoAllowed[d]=controlLfoDestinationAvailable(lfo,int(d));
  for(int a=0;a<22;++a)composerPanelLfoEngines[a]=lfo.target<controlLfoPartCount()&&controlLfoHasEngine(lfo.target,a);
  c.clipNoteCount=c.clipPointCount=0; // Performance data is read only by explicit Export/Import.
  composerPanelActive=activeClip;composerPanelRevision=persistentRevision;composerPanelTempo=tempoBpm;composerPanelPolyphony=polyphonyCompensation;
  composerPanelJob.store(2,std::memory_order_release);
}
void publishComposerPanel() {
  using namespace s3g_thru_pack;
  auto& c=*composerPanelClip;
  auto assets=ComposerJson::array();
  for(int l=0;l<4;++l){
    if(!c.laneSourcePaths[l][0]&&!composerPanelFrames[l])continue;
    const std::string path=c.laneSourcePaths[l][0]?c.laneSourcePaths[l].data():"Lane "+std::to_string(l+1);
    const auto id=sha256(path).substr(0,24);
    auto a=composerObject({{"id",id}});
    const auto slash=path.find_last_of('/');a["name"]=path.substr(slash==std::string::npos?0:slash+1);
    if(composerPanelFrames[l]){
      a["frames"]=double(composerPanelFrames[l]);a["sampleRate"]=double(composerPanelRates[l]);
      auto wave=ComposerJson::array();for(std::size_t bin=0;bin<kWaveBins;++bin){wave.push(float(composerPanelMinimum[l][bin])/7.f);wave.push(float(composerPanelMaximum[l][bin])/7.f);}a["waveform"]=std::move(wave);
    }
    bool seen=false;for(const auto& prior:assets.arrayValue())seen|=prior.at("id").string()==id;
    if(!seen)assets.push(std::move(a));
    std::snprintf(c.laneSourcePaths[l].data(),kPersistencePathSize,"asset:%s",id.c_str());
  }
  auto doc=composerObject({{"format","s3g-thrupack"},{"version",1},{"title","Move live clip"},{"tempo",composerPanelTempo},{"assets",std::move(assets)},
      {"clips",ComposerJson::Array{composerClip(c,composerPanelActive)}},{"chains",ComposerJson::array()}});
  auto settings=ComposerJson::array();
  const int count=c.splitEnabled?(c.quadMode?4:2):1,slots=c.splitEnabled?(c.quadMode?2:4):8;
  for(int part=0;part<count;++part){
    const auto& p=c.splitEnabled?c.splitSettings[part]:c.singleSettings;const auto& cfg=p.engines;
    const int entry=std::clamp(cfg.edit,0,slots-1);const auto& variation=cfg.variations[entry];
    auto values=composerObject({{"pad_mode",p.mode},{"voice_mode",p.voiceMode},{"hold_release_ms",p.holdMs},{"root_note",p.root},{"scale",p.scale},
      {"pad_octave",p.octave},{"pressure_mode",p.pressure},{"lane_variation",p.variation},{"part_mute",p.muted},{"stack_scan_paused",p.stackHold},{"glitch_buffer_lock",p.glitchHold},{"polyphony_comp",composerPanelPolyphony?1:0},
      {"voice_engine_mode",cfg.enabled?1:0},{"voice_engine_order",cfg.order},{"voice_engine_edit",entry+1},{"voice_engine_assign",cfg.assignment},{"voice_engine_depth",cfg.pressureDepth*100},
      {"voice_engine_octave",variation.octave},{"voice_engine_position",variation.position*100},{"voice_engine_stack",variation.stack*100},{"voice_engine_pan",variation.pan*100}});
    for(int slot=0;slot<8;++slot){const std::string key="voice_engine_"+std::to_string(slot+1);values[key]=cfg.configured?int(cfg.engines[slot]):int((c.splitEnabled?c.splitValues[part]:c.baseValues)[int(LockParameter::algorithm)]);}
    if(c.layers.active){values["layers_corner"]=composerPanelFxCorner;values["layers_surface"]=1;const char* names[]{"layers_glide","layers_depth","layers_z_mode","layers_z","layers_return"};const int fields[]{LayerPatch::glide,LayerPatch::depth,LayerPatch::zMode,LayerPatch::pressure,LayerPatch::returnDry};for(int i=0;i<5;++i)values[names[i]]=c.layers.values[LayerPatch::config+fields[i]];for(int i=0;i<12;++i)values["layers_fx"+std::to_string(i)]=c.layers.values[i<8?LayerPatch::fx+composerPanelFxCorner*8+i:LayerPatch::advanced+composerPanelFxCorner*4+i-8];}
    if(c.layers.active){float x=0,y=0;for(int corner=0;corner<4;++corner){const float w=c.layers.values[LayerPatch::config+LayerPatch::weight0+corner];x+=w*float(corner%2);y+=w*float(corner/2);}values["layers_fx_x"]=x;values["layers_fx_y"]=y;}
    auto lfos=ComposerJson::array();for(int slot=0;slot<8;++slot){auto row=ComposerJson::array();const auto& cfg=c.lfos[slot];const char* names[]{"dest","wave","clock","rate","depth","mode","phase","smooth","flow","target","engine","offset"};for(int f=0;f<12;++f){const float v=controlLfoField(cfg,f);row.push(v);values["lfo"+std::to_string(slot+1)+"_"+names[f]]=v;}lfos.push(std::move(row));}values["__lfos"]=std::move(lfos);values["__lfoEdit"]=composerPanelLfoEdit;values["lfo_select"]=composerPanelLfoEdit+1;
    auto allowed=ComposerJson::array(),engines=ComposerJson::array();engines.push(-1);
    for(std::size_t d=0;d<composerPanelLfoAllowed.size();++d){if(composerPanelLfoAllowed[d])allowed.push(int(d));}
    for(int a=0;a<22;++a){if(composerPanelLfoEngines[a])engines.push(a);}values["__lfoAllowed"]=std::move(allowed);values["__lfoEngines"]=std::move(engines);
    settings.push(std::move(values));
  }
  auto patches=ComposerJson::array();
  auto& core=doc["clips"].arrayValue()[0];
  for(int part=0;part<int(core.at("parts").arrayValue().size());++part){
    const int first=c.splitEnabled&&!c.sharedDual?(c.quadMode?part:part*2):0,last=c.splitEnabled&&!c.sharedDual?first+(c.quadMode?0:1):3;
    const int physical=std::clamp(composerPanelSelectedLane,first,last);patches.push(physical);
    for(const auto& p:s3g_thru_midi::parameters)if(p.lock<0&&s3g_thru_pack::soundParameter(p))if(const auto* value=composerProgramValue(c,p.id,c.splitEnabled?part+1:0,physical))
      core["parts"].arrayValue()[part]["parameters"][p.key]=std::clamp(double(*value),composerBoundary(p.minimum),composerBoundary(p.maximum));
  }
  s3g_thru_pack::validate(doc);
  auto frame=composerObject({{"editSequence",double(composerPanelEditVersion)},{"protocol",2},{"build","0.34.0"},{"focus",composerPanelFocus},{"epoch",composerEpoch},{"client",composerPanelSnapshotOwner.data()},{"revision",std::to_string(composerPanelRevision)},
      {"activeClip",composerPanelActive},{"settings",std::move(settings)},{"waveVersion",double(composerPanelCapturedWaveVersion)},{"patch",composerPanelSelectedLane},{"patches",std::move(patches)},{"document",std::move(doc)}});
  auto occupied=ComposerJson::array();for(bool yes:composerPanelOccupied)occupied.push(yes);frame["occupied"]=std::move(occupied);
  auto clips=ComposerJson::array();for(int i=0;i<64;++i)clips.push(composerObject({{"slot",i},{"name",composerPanelNames[i].data()},{"mode",s3g_thru_pack::modes[composerPanelModes[i]]},{"occupied",composerPanelOccupied[i]}}));frame["clips"]=std::move(clips);
  composerPanelPath=composerRoot+"/panel-"+composerPanelSnapshotOwner.data()+".json";
  atomicFile(composerPanelPath,s3g_thru_json::stringify(frame));
  composerPanelPublishedVersion=std::to_string(composerPanelActive)+":"+std::to_string(composerPanelRevision)+":"+std::to_string(composerPanelSelectedLane)+":"+std::to_string(composerPanelCapturedWaveVersion)+":"+std::to_string(composerPanelFocus)+":"+std::to_string(composerPanelEditVersion);
}
