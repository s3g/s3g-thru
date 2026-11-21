static bool partialsRecipeType(int type){return type==s3g_thru_synth::partialsType||type==s3g_thru_synth::subharmonic||type==s3g_thru_synth::bloom;}
void restoreLivePartialsRecipe(const s3g_thru_synth::Recipe& recipe){
 namespace t=s3g_thru_partials;namespace s=s3g_thru_synth;const int part=splitEnabled?splitFocus:0,selected=tableEditLane(part);const auto assign=[&](t::Config& c,int f,float x){t::set(c,f,x);};
 const auto configure=[&](int spectrum){t::Config c;assign(c,t::width,recipe.v[s::width]*100);assign(c,t::level,recipe.v[s::level]*100);const int root=settingsForPart(part).root;assign(c,t::tune,recipe.v[s::root]-root);assign(c,t::time,recipe.v[s::length]*1000);
  for(int j=0;j<32;++j){assign(c,t::ratios+j,recipe.ratios[j/8][j%8]);assign(c,t::bands+j,recipe.partials[j/8][j%8]);}
  if(recipe.type==s::partialsType){assign(c,t::type,0);assign(c,t::count,recipe.additive[s::aCount]);assign(c,t::tilt,recipe.additive[s::aTilt]);assign(c,t::stretch,recipe.additive[s::aStretch]);assign(c,t::detune,recipe.additive[s::aDetune]);assign(c,t::phase,recipe.additive[s::aPhase]*100);assign(c,t::drift,recipe.additive[s::aDrift]*30);assign(c,t::drift_rate,recipe.additive[s::aDriftRate]);assign(c,t::damping,recipe.additive[s::aDecay]*100);}
  else if(recipe.type==s::subharmonic){assign(c,t::type,1);assign(c,t::harmonics,recipe.sub[s::sHarmonics]);assign(c,t::subs,recipe.sub[s::sSubs]);assign(c,t::balance,recipe.sub[s::sBalance]*100);assign(c,t::tilt,recipe.sub[s::sTilt]);assign(c,t::detune,recipe.sub[s::sDetune]);assign(c,t::drift,recipe.sub[s::sDrift]);assign(c,t::drift_rate,recipe.sub[s::sRate]);assign(c,t::tune,recipe.v[s::root]-root+recipe.sub[s::sRegister]*12);for(int j=0;j<16;++j)assign(c,t::divisors+j,recipe.ratios[2+j/8][j%8]);}
  else {assign(c,t::type,2);assign(c,t::count,recipe.packets[s::bCount]);assign(c,t::gap,recipe.packets[s::bGap]);assign(c,t::scatter,recipe.packets[s::bScatter]*100);assign(c,t::rise,recipe.packets[s::bRise]);assign(c,t::sustain,recipe.packets[s::bSustain]);assign(c,t::decay,recipe.packets[s::bDecay]);assign(c,t::variation,std::max({recipe.packets[s::bRiseVar],recipe.packets[s::bSustainVar],recipe.packets[s::bDecayVar],recipe.packets[s::bAmpJitter]})*100);assign(c,t::pitch_jitter,recipe.packets[s::bPitchJitter]);assign(c,t::shape,recipe.packets[s::bShape]);for(int j=0;j<8;++j){assign(c,t::ratios+j,recipe.ratios[spectrum][j]);assign(c,t::bands+j,recipe.partials[spectrum][j]);}}
  return c;};
 pushClipUndo(activeClip);
 if(recipe.type==s::bloom){const int first=splitEnabled&&!sharedDual?firstLaneForPart(part):0,count=splitEnabled&&!sharedDual?(isQuad()?1:2):4;const int chosen=int(recipe.packets[s::bSpectrum]);for(int j=0;j<count;++j){const int lane=first+(selected-first+j)%count;controls.partials.patches[lane]=configure((chosen+j)%4);}}
 else controls.partials.patches[selected]=configure(0);
 controls.algorithm=Engine::PlaybackAlgorithm::partials;if(auto* clip=activeClipSlot())capturePartialsPrograms(*clip);synthPreviewWanted=false;synthPreviewGain=0;synthOpened=false;++midiRevision;markPersistentStateDirty();
}
static bool resonRecipeType(int type,int model){return type==s3g_thru_synth::drum&&((model>=0&&model<=5)||model==9);}
void restoreLiveResonRecipe(const s3g_thru_synth::Recipe& recipe){
 namespace t=s3g_thru_reson;namespace s=s3g_thru_synth;const int part=splitEnabled?splitFocus:0,lane=tableEditLane(part),body=recipe.drums[s::dModel]==9?7:int(recipe.drums[s::dModel])+1;auto c=t::startingSound(body+1,controls.reson.patches[lane]);const auto assign=[&](int f,float v){t::set(c,f,v);};
 assign(t::mode,1);assign(t::model,float(body));assign(t::width,recipe.v[s::width]*100);assign(t::level,recipe.v[s::level]*100);const float rootHz=440*std::exp2(float(settingsForPart(part).root-69)/12);assign(t::tune,12*std::log2(std::max(1.f,recipe.drums[s::dTune])/rootHz));assign(t::decay,recipe.drums[s::dDecay]);assign(t::brightness,recipe.drums[s::dTone]*100);assign(t::damping,recipe.drums[s::dDamping]*100);assign(t::mass,recipe.drums[s::dBody]*100);assign(t::inharmonic,recipe.drums[s::dRing]*100);assign(t::bend,recipe.drums[s::dBend]);assign(t::sweep,recipe.drums[s::dSweep]);assign(t::noise,recipe.drums[s::dNoise]*100);assign(t::noise_decay,recipe.drumExtra[s::xNoiseDecay]);assign(t::color,recipe.drums[s::dTone]*100);assign(t::hit,recipe.drums[s::dHit]*100);assign(t::hit_decay,recipe.drumExtra[s::xHitDecay]);assign(t::punch,(recipe.drumExtra[s::xPunch]+.5f*recipe.drumExtra[s::xSnap])*100);assign(t::drive,recipe.drums[s::dCharacter]*100);assign(t::bias,recipe.drumExtra[s::xBias]*200);assign(t::tone,recipe.drumColor[s::cLowpass]);assign(t::seed,float(recipe.seed&65535u));
 controls.reson.patches[lane]=c;controls.algorithm=Engine::PlaybackAlgorithm::reson;if(auto* clip=activeClipSlot())captureResonPrograms(*clip);synthPreviewWanted=false;synthPreviewGain=0;synthOpened=false;++midiRevision;markPersistentStateDirty();
}
static bool noiseRecipeType(int type){return type==s3g_thru_synth::noiseBank||type==s3g_thru_synth::dust;}
void restoreLiveNoiseRecipe(const s3g_thru_synth::Recipe& recipe){
 namespace n=s3g_thru_noise;namespace s=s3g_thru_synth;const int part=splitEnabled?splitFocus:0,lane=tableEditLane(part);n::Config cfg;
 const auto assign=[&](int field,float value){n::set(cfg,field,value);};assign(n::width,recipe.v[s::width]*100);assign(n::level,recipe.v[s::level]*100);assign(n::tune,recipe.v[s::root]-settingsForPart(part).root);assign(n::seed,float(recipe.seed&65535u));
 if(recipe.type==s::noiseBank){const int fields[]{n::count,n::bandwidth,n::tilt,n::stretch,n::color,n::motion,n::motion_rate,n::leak};for(int f=0;f<8;++f)assign(fields[f],recipe.noise[f]*(f==7?100.f:1.f));for(int j=0;j<32;++j){cfg.v[n::ratios+j]=recipe.ratios[j/8][j%8];cfg.v[n::bands+j]=recipe.partials[j/8][j%8];}cfg.v[n::style]=3;}
 else{assign(n::type,recipe.dustParams[s::uMode]+1);const int fields[]{n::type,n::rate,n::size,n::clump,n::tone,n::edge,n::bits,n::variation,n::bed,n::color,n::repeat,n::fault};for(int f=1;f<s::dustFields;++f)assign(fields[f],recipe.dustParams[f]*((f==3||f==5||f==7||f==8||f==11)?100.f:1.f));}
 controls.noise.patches[lane]=cfg;controls.algorithm=Engine::PlaybackAlgorithm::noise;if(auto* clip=activeClipSlot())captureNoisePrograms(*clip);synthPreviewWanted=false;synthPreviewGain=0;synthOpened=false;++midiRevision;markPersistentStateDirty();
}
static bool fmRecipeType(int type){return type==s3g_thru_synth::fm||type==s3g_thru_synth::fmDrum;}
void restoreLiveFmRecipe(const s3g_thru_synth::Recipe& recipe){
 namespace f=s3g_thru_fm;namespace s=s3g_thru_synth;const int part=splitEnabled?splitFocus:0,lane=tableEditLane(part);f::Config cfg;
 const auto assign=[&](int field,float value){f::set(cfg,field,value);};assign(f::width,recipe.v[s::width]*100);assign(f::level,recipe.v[s::level]*100);assign(f::tune,recipe.v[s::root]-settingsForPart(part).root);
 if(recipe.type==s::fm){const int target[]{f::algorithm,f::ratio1,f::index1,f::ratio2,f::index2,f::feedback,f::detune,f::mix};for(int i=0;i<8;++i)assign(target[i],recipe.fmOsc[i]*((i==s::fFeedback||i==s::fMix)?100.f:1.f));}
 else{assign(f::mode,1);assign(f::ratio1,recipe.drums[s::dRing]);assign(f::index1,12*recipe.drums[s::dContactX]);assign(f::feedback,90*recipe.drums[s::dDamping]);assign(f::decay,recipe.drums[s::dDecay]);assign(f::fm_decay,2*std::pow(2000.f,recipe.drums[s::dContactY]));assign(f::bend,recipe.drums[s::dBend]);assign(f::sweep_time,recipe.drums[s::dSweep]);assign(f::impact,100*recipe.drums[s::dHit]);assign(f::noise,100*recipe.drums[s::dNoise]);assign(f::noise_decay,recipe.drumExtra[s::xNoiseDecay]);assign(f::cutoff,300*std::pow(60.f,recipe.drums[s::dTone]));const float rootHz=440*std::exp2(float(settingsForPart(part).root-69)/12);assign(f::tune,12*std::log2(std::max(1.f,recipe.drums[s::dTune])/rootHz));}
 controls.fm.patches[lane]=cfg;controls.algorithm=Engine::PlaybackAlgorithm::fm;if(auto* clip=activeClipSlot())captureFmPrograms(*clip);synthPreviewWanted=false;synthPreviewGain=0;synthOpened=false;++midiRevision;markPersistentStateDirty();
}
static bool tableRecipeType(int type){return type==s3g_thru_synth::wave||type==s3g_thru_synth::spline||type==s3g_thru_synth::wtDrum;}
void restoreLiveTableRecipe(const s3g_thru_synth::Recipe& recipe){
 namespace t=s3g_thru_table;namespace s=s3g_thru_synth;
 const int part=splitEnabled?splitFocus:0,lane=tableEditLane(part);t::Config config;
 const auto assign=[&](int field,float value){t::set(config,field,value);};
 assign(t::type,recipe.type==s::wave?0:recipe.type==s::spline?1:2);
 assign(t::width,recipe.v[s::width]*100);assign(t::level,recipe.v[s::level]*100);
 assign(t::tune,recipe.v[s::root]-settingsForPart(part).root);
 if(recipe.type==s::wave){const int target[]{t::shape,t::pulse,t::detune,t::sub,t::cutoff,t::resonance,t::noise};
  for(int f=0;f<s::waveFields;++f)assign(target[f],recipe.osc[f]*((f==s::wPulse||f==s::wSub||f==s::wResonance||f==s::wNoise)?100.f:1.f));}
 if(recipe.type==s::spline){const int target[]{t::points,t::evolution,t::drift,t::drift_rate,t::curve,t::dynamics};const int from[]{s::points,s::evolution,s::drift,s::driftRate,s::curvature,s::dynamics};
  for(int f=0;f<6;++f)assign(target[f],recipe.v[from[f]]*(f>=4?100.f:1.f));
  assign(t::seed,float(recipe.seed&65535u));}
 if(recipe.type==s::wtDrum){t::start(config,int(recipe.wt[s::wtModel]));
  const int target[]{t::model,t::decay,t::bend,t::pitch_decay,t::bank_a,t::pos_a,t::bank_b,t::pos_b,t::scan,t::scan_decay,t::blend,t::ratio,t::fm,t::wt_sub,t::noise,t::noise_decay,t::color,t::impact,t::tone,t::drive,t::compression,t::wt_detune};
  const int from[]{s::wtModel,s::wtDecay,s::wtBend,s::wtPitchDecay,s::wtTableA,s::wtPosA,s::wtTableB,s::wtPosB,s::wtScan,s::wtScanDecay,s::wtBlend,s::wtRatio,s::wtFm,s::wtSub,s::wtNoise,s::wtNoiseDecay,s::wtNoiseColor,s::wtImpact,s::wtTone,s::wtDrive,s::wtComp,s::wtDetune};
  for(int f=0;f<22;++f){const bool percent=f==5||f==7||f==8||f==10||f==12||f==13||f==14||f==17||f==18||f==19||f==20;assign(target[f],recipe.wt[from[f]]*(percent?100.f:1.f));}
  const float rootHz=440.f*std::exp2(float(settingsForPart(part).root-69)/12);assign(t::tune,12*std::log2(std::max(1.f,recipe.wt[s::wtTune])/rootHz));
 }
 controls.table.patches[lane]=config;controls.algorithm=Engine::PlaybackAlgorithm::table;
 if(auto* clip=activeClipSlot())captureTablePrograms(*clip);
 synthPreviewWanted=false;synthPreviewGain=0;synthOpened=false;++midiRevision;markPersistentStateDirty();
}
std::unique_ptr<s3g_thru_synth::Worker> synthWorker;
std::unique_ptr<s3g_thru_synth::Session> synthSession;
std::unique_ptr<s3g_thru_synth::SessionStore> synthSessionStore;
s3g_thru_synth::Worker::Request synthQueuedRender;
bool synthSessionDirty = false;
std::uint64_t synthSessionFailure = 0;
void captureSynthDraft() {
  if (!synthSession || !synthInitialized)
    return;
  auto &d = synthSession->drafts[synthRecipe.type];
  d.used = true;
  d.recipe = synthRecipe;
  auto &e = d.editor;
  e.row = synthRow;
  e.point = synthPoint;
  e.spectrum = synthSpectrum;
  e.component = synthComponent;
  e.name = synthName;
  e.automaticName = synthNameAutomatic;
  synthWorkspaces[synthRecipe.type] = synthRecipe;
  synthWorkspaceValid[synthRecipe.type] = true;
}
void saveSynthSession() {
  if (!synthSessionDirty || !synthSession || !synthSessionStore ||
      !synthInitialized)
    return;
  captureSynthDraft();
  synthSession->type = synthRecipe.type;
  auto &sources = synthSession->sources;
  sources[0].path = synthSourcePath;
  sources[0].label = synthSourceLabel;
  sources[0].embedded = synthSourceEmbedded;
  for (int i = 0; i < 2; ++i) {
    sources[i + 1].path = synthConvPaths[i];
    sources[i + 1].label = synthConvLabels[i];
    sources[i + 1].embedded = synthConvEmbedded[i];
  }
  for (int i = 0; i < 4; ++i) {
    sources[i + 3].path = synthHybridPaths[i];
    sources[i + 3].label = synthHybridLabels[i];
    sources[i + 3].embedded = synthHybridEmbedded[i];
  }
  if (synthSessionStore->post(*synthSession))
    synthSessionDirty = false;
}
void restoreSynthEditor(int type) {
  using namespace s3g_thru_synth;
  if (!synthSession)
    return;
  auto &d = synthSession->drafts[type];
  auto &e = d.editor;
  if (d.used) {
    synthRow = e.row;
    synthPoint = e.point;
    synthSpectrum = e.spectrum;
    synthComponent = e.component;
    synthName = e.name;
    synthNameAutomatic = e.automaticName;
  } else {
    e = EditorState{};
    e.low = -limits(synthRecipe)[0] / 2;
    e.high = limits(synthRecipe)[0] / 2;
    synthRow = synthPoint = synthSpectrum = synthComponent = 0;
    synthNameAutomatic = true;
    std::snprintf(synthName.data(), synthName.size(), "%s-%d",
                  generatorName(type), int(synthRecipe.v[root]));
  }
}
int formatSynthEditor(char *out, int size) const {
  if (!out || size < 1)
    return -1;
  const auto e = synthSession ? synthSession->drafts[synthRecipe.type].editor
                              : s3g_thru_synth::EditorState{};
  const int n = std::snprintf(
      out, size, "%d,%d,%d,%d,%d,%.6g,%.6g,%.6g,%d", synthRecipe.type, e.page,
      e.groups[0], e.groups[1], e.groups[2], double(e.amount), double(e.low),
      double(e.high), e.keepEnds ? 1 : 0);
  return n >= size ? -1 : n;
}
bool requestSynthRender() {
  using namespace s3g_thru_synth;
  using W = Worker;
  if (!synthWorker)
    return false;
  if (synthWorker->state.load() != W::idle &&
      synthWorker->request.op != W::generate) {
    setError("Wait for synthesis job");
    return false;
  }
  if (synthHybridMissing() ||
      (synthRecipe.type == resynth && !synthSourcePath[0]) ||
      (synthRecipe.type == convolution &&
       (!synthConvPaths[0][0] || !synthConvPaths[1][0]))) {
    setError("Choose synthesis sources first");
    std::snprintf(synthMessage.data(), synthMessage.size(),
                  "CHOOSE SOURCE FILES");
    return false;
  }
  W::Request job;
  job.op = W::generate;
  job.recipe = synthRecipe;
  job.revision = synthRevision;
  job.rate = sampleRate;
  job.source = synthSourcePath;
  job.embeddedSource = synthSourceEmbedded;
  job.sourceEpoch = synthSourceEpoch;
  if (synthRecipe.type == convolution) {
    job.source = synthConvPaths[0];
    job.impulse = synthConvPaths[1];
    job.sourceEpoch = synthConvEpochs[0];
    job.impulseEpoch = synthConvEpochs[1];
    job.embeddedSource = synthConvEmbedded[0];
    job.embeddedImpulse = synthConvEmbedded[1];
  }
  if (synthRecipe.type == hybrid) {
    job.hybridPaths = synthHybridPaths;
    job.hybridEpochs = synthHybridEpochs;
    job.hybridEmbedded = synthHybridEmbedded;
  }
  synthQueuedRender = job;
  synthWantsRender = true;
  if (synthWorker->state.load() != W::idle)
    synthWorker->cancel.store(true);
  saveSynthSession();
  clearError();
  return true;
}
s3g_thru_synth::Recipe synthRecipe;
std::array<s3g_thru_synth::Recipe, s3g_thru_synth::generatorCount>
    synthWorkspaces{};
std::array<bool, s3g_thru_synth::generatorCount> synthWorkspaceValid{};
bool synthNameAutomatic = true, synthSourceEmbedded = false;
std::array<char, 768> synthSourcePath{};
std::array<char, 65> synthSourceLabel{};
std::uint64_t synthSourceEpoch = 1;
int synthSpectrum = 0, synthComponent = 0;
std::array<std::array<char, 768>, 2> synthConvPaths{};
std::array<std::array<char, 65>, 2> synthConvLabels{};
std::array<std::uint64_t, 2> synthConvEpochs{};
std::array<bool, 2> synthConvEmbedded{};
bool synthWorkerReady = false, synthSavedLoading = false;
bool synthOpened = false, synthInitialized = false, synthWantsRender = false,
     synthPreviewWanted = false;
std::uint64_t synthRevision = 1;
int synthActive = -1, synthFade = -1, synthRow = 0, synthPoint = 0;
std::size_t synthPosition = 0, synthOldPosition = 0, synthFadeRemaining = 0;
float synthPreviewGain = 0;
std::array<char, 512> synthDirectory{}, synthLastSaved{}, synthLoadPath{};
std::array<char, 41> synthName{};
std::array<char, 96> synthMessage{};
std::array<std::array<char, 768>, 4> synthHybridPaths{};
std::array<std::array<char, 65>, 4> synthHybridLabels{};
std::array<std::uint64_t, 4> synthHybridEpochs{};
std::array<bool, 4> synthHybridEmbedded{};
int synthHybridMissing() const {
  if (synthRecipe.type != s3g_thru_synth::hybrid)
    return 0;
  for (int i = 0; i < s3g_thru_synth::hybridSourceCount(synthRecipe);
       ++i)
    if (!synthHybridPaths[i][0])
      return i + 1;
  return 0;
}
int synthLoadLane = 0, synthLoadClip = -1;
void selectSynthType(int wanted) {
  using namespace s3g_thru_synth;
  if (wanted < 0 || wanted >= generatorCount || wanted == synthRecipe.type)
    return;
  captureSynthDraft();
  synthWorkspaces[synthRecipe.type] = synthRecipe;
  synthWorkspaceValid[synthRecipe.type] = true;
  synthWantsRender = false;
  synthPreviewWanted = false;
  if (synthWorker &&
      synthWorker->state.load() != s3g_thru_synth::Worker::idle &&
      synthWorker->request.op == s3g_thru_synth::Worker::generate)
    synthWorker->cancel.store(true);
  if (synthWorkspaceValid[wanted])
    synthRecipe = synthWorkspaces[wanted];
  else {
    Recipe next;
    initializeGenerator(next, wanted);
    for (int i = 0; i <= level; ++i)
      next.v[i] = synthRecipe.v[i];
    next.v[loop] = synthRecipe.v[loop];
    next.seed = synthRecipe.seed;
    next.scale = synthRecipe.scale;
    if (wanted == resynth || wanted == hybrid)
      next.v[width] = 1;
    if (wanted == convolution) {
      next.v[length] = 12;
      next.v[width] = 1;
      next.v[loop] = 0;
    }
    if (isDrumType(wanted) || wanted == dust || wanted == wtDrum) {
      next.v[length] = 2;
      next.v[attack] = 0;
      next.v[release] = 2;
      next.v[loop] = 0;
    }
    synthRecipe = next;
  }
  restoreSynthEditor(wanted);
  synthEdited();
  if (synthHybridMissing()) {
    synthWantsRender = false;
    synthPreviewWanted = false;
    std::snprintf(synthMessage.data(), synthMessage.size(),
                  "PICK HYBRID SOURCES");
  }
  if ((wanted == resynth && !synthSourcePath[0]) ||
      (wanted == convolution &&
       (!synthConvPaths[0][0] || !synthConvPaths[1][0]))) {
    synthWantsRender = false;
    synthPreviewWanted = false;
    std::snprintf(synthMessage.data(), synthMessage.size(),
                  wanted == convolution ? "PICK PRIMARY AND IR"
                                        : "USE LANE SOURCE");
  }
}
bool prepareSynth() {
  synthWorker.reset(new (std::nothrow) s3g_thru_synth::Worker);
  std::snprintf(synthDirectory.data(), synthDirectory.size(),
                "/data/UserData/UserLibrary/Samples/s3g Thru/Synths");
  std::snprintf(synthName.data(), synthName.size(), "Spline");
  synthWorkerReady = synthWorker && synthWorker->prepare();
  synthSession = std::make_unique<s3g_thru_synth::Session>();
  synthSessionStore = std::make_unique<s3g_thru_synth::SessionStore>();
  if (persistenceEnabled && moduleRootDirectory[0]) {
    std::array<char, 1024> file{};
    std::snprintf(file.data(), file.size(), "%s/thru-synth-session.bin",
                  moduleRootDirectory.data());
    const auto loaded =
        s3g_thru_synth::SessionStore::load(file.data(), *synthSession);
    if (loaded == s3g_thru_synth::SessionStore::loaded ||
        loaded == s3g_thru_synth::SessionStore::recovered) {
      for (int type = 0; type < s3g_thru_synth::generatorCount; ++type)
        if (synthSession->drafts[type].used) {
          synthWorkspaces[type] = synthSession->drafts[type].recipe;
          synthWorkspaceValid[type] = true;
        }
      synthRecipe = synthWorkspaces[synthSession->type];
      synthInitialized = true;
      restoreSynthEditor(synthSession->type);
      const auto &sources = synthSession->sources;
      synthSourcePath = sources[0].path;
      synthSourceLabel = sources[0].label;
      synthSourceEmbedded = sources[0].embedded;
      for (int i = 0; i < 2; ++i) {
        synthConvPaths[i] = sources[i + 1].path;
        synthConvLabels[i] = sources[i + 1].label;
        synthConvEmbedded[i] = sources[i + 1].embedded;
      }
      for (int i = 0; i < 4; ++i) {
        synthHybridPaths[i] = sources[i + 3].path;
        synthHybridLabels[i] = sources[i + 3].label;
        synthHybridEmbedded[i] = sources[i + 3].embedded;
      }
    }
    synthSessionStore->prepare(file.data());
  }
  return synthWorkerReady;
}
void stopSynth() {
  if (!synthWorkerReady)
    return;
  saveSynthSession();
  synthWorkerReady = false;
  if (synthWorker)
    synthWorker->stop();
  if (synthSessionStore)
    synthSessionStore->stop();
}
bool synthReady() const {
  return synthWorker && synthActive >= 0 &&
         synthWorker->audio[synthActive].revision == synthRevision;
}
void synthEdited() {
  ++synthRevision;
  synthSessionDirty = true;
  std::snprintf(synthMessage.data(), synthMessage.size(), "CAPTURE: RENDER");
}
void synthSetName(const char *text) {
  std::size_t n = 0;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text);
       *p && n + 1 < synthName.size(); ++p)
    if ((std::isalnum(*p) && *p < 128) || *p == ' ' || *p == '-' || *p == '_')
      synthName[n++] = char(*p);
  synthName[n] = 0;
}
void serviceSynth() {
  using W = s3g_thru_synth::Worker;
  if (!synthWorker)
    return;
  auto &worker = *synthWorker;
  if (synthSessionStore) {
    const auto failed = synthSessionStore->failed.load();
    if (failed > synthSessionFailure) {
      synthSessionFailure = failed;
      synthSessionDirty = true;
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    "DRAFT SAVE FAILED");
    }
  }
  if (worker.state.load(std::memory_order_acquire) == W::complete) {
    const auto &job = worker.request;
    if (job.op == W::generate && worker.success && !worker.cancel.load() &&
        job.recipe.type == synthRecipe.type) {
      if (synthFadeRemaining)
        return; // Adopt on a later block, never retire a reader still fading.
      synthFade = synthPreviewGain > .0001f ? synthActive : -1;
      synthOldPosition = synthPosition;
      synthFadeRemaining =
          synthFade >= 0
              ? std::max<std::size_t>(1, std::size_t(sampleRate * .012f))
              : 0;
      synthActive = job.slot;
      synthPosition = 0;
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    synthReady() ? "READY" : "CAPTURE: RENDER");
    } else if (job.op == W::save) {
      if (worker.success) {
        std::snprintf(synthLastSaved.data(), synthLastSaved.size(), "%s",
                      worker.savedPath.data());
        std::snprintf(synthMessage.data(), synthMessage.size(), "SAVED");
        if (job.load) {
          synthLoadLane = job.lane;
          synthLoadClip = job.clip;
          std::snprintf(synthLoadPath.data(), synthLoadPath.size(), "%s",
                        worker.savedPath.data());
          synthPreviewWanted = false;
        }
      } else
        std::snprintf(synthMessage.data(), synthMessage.size(), "%s",
                      worker.error.data());
    } else if (job.op == W::restore && worker.success &&
               job.revision == synthRevision) {
      captureSynthDraft();
      synthWorkspaces[synthRecipe.type] = synthRecipe;
      synthWorkspaceValid[synthRecipe.type] = true;
      synthRecipe = worker.restored;
      synthWorkspaces[synthRecipe.type] = synthRecipe;
      synthWorkspaceValid[synthRecipe.type] = true;
      synthInitialized = true;
      synthNameAutomatic = false;
      if (synthRecipe.type == s3g_thru_synth::resynth) {
        synthSourcePath = job.source;
        synthSourceEmbedded = true;
        synthSourceEpoch = job.sourceEpoch;
        std::snprintf(synthSourceLabel.data(), synthSourceLabel.size(),
                      "EMBEDDED SOURCE");
      }
      if (synthRecipe.type == s3g_thru_synth::convolution)
        for (int i = 0; i < 2; ++i) {
          synthConvPaths[i] = job.source;
          synthConvEpochs[i] = job.sourceEpoch;
          synthConvEmbedded[i] = true;
          std::snprintf(synthConvLabels[i].data(), synthConvLabels[i].size(),
                        "EMBEDDED %s", i ? "IMPULSE" : "PRIMARY");
        }
      if (synthRecipe.type == s3g_thru_synth::hybrid)
        for (int i = 0;
             i < s3g_thru_synth::hybridSourceCount(synthRecipe); ++i) {
          synthHybridPaths[i] = job.source;
          synthHybridEpochs[i] = job.sourceEpoch;
          synthHybridEmbedded[i] = true;
          std::snprintf(synthHybridLabels[i].data(),
                        synthHybridLabels[i].size(), "EMBEDDED SOURCE %c",
                        char('A' + i));
        }
      synthRow = 0;
      synthPoint = 0;
      synthSpectrum = int(synthRecipe.packets[s3g_thru_synth::bSpectrum]);
      synthComponent = 0;
      const char *name = std::strrchr(job.source.data(), '/');
      std::array<char, 768> leaf{};
      std::snprintf(leaf.data(), leaf.size(), "%s",
                    name ? name + 1 : job.source.data());
      if (char *dot = std::strrchr(leaf.data(), '.'))
        *dot = 0;
      synthSetName(leaf.data());
      synthEdited();
      std::snprintf(synthMessage.data(), synthMessage.size(), "RECIPE OPENED");
      if(partialsRecipeType(synthRecipe.type)){restoreLivePartialsRecipe(synthRecipe);std::snprintf(synthMessage.data(),synthMessage.size(),"LOADED TO PARTIALS");}
      else if(resonRecipeType(synthRecipe.type,int(synthRecipe.drums[s3g_thru_synth::dModel]))){restoreLiveResonRecipe(synthRecipe);std::snprintf(synthMessage.data(),synthMessage.size(),"LOADED TO RESON");}
      else if(noiseRecipeType(synthRecipe.type)){restoreLiveNoiseRecipe(synthRecipe);std::snprintf(synthMessage.data(),synthMessage.size(),"LOADED TO NOISE");}
      else if(fmRecipeType(synthRecipe.type)){restoreLiveFmRecipe(synthRecipe);std::snprintf(synthMessage.data(),synthMessage.size(),"LOADED TO FM");}
      else if(tableRecipeType(synthRecipe.type)){restoreLiveTableRecipe(synthRecipe);std::snprintf(synthMessage.data(),synthMessage.size(),"LOADED TO TABLE");}
    } else if (!worker.success && job.revision == synthRevision)
      std::snprintf(synthMessage.data(), synthMessage.size(), "%s",
                    worker.error.data());
    worker.state.store(W::idle, std::memory_order_release);
    if (job.op == W::save || job.op == W::restore)
      saveSynthSession();
  }
  if (synthLoadPath[0]) {
    if (activeClip != synthLoadClip) {
      synthLoadPath[0] = 0;
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    "SAVED - CLIP CHANGED");
    } else if (!audioRecording() && !clipRecording &&
               !clipPreparationInFlight() && !sampleLoadBusy()) {
      requestLibrarySample(synthLoadLane, synthLoadPath.data());
      synthLoadPath[0] = 0;
      synthSavedLoading = sampleLoadState.load() != int(SampleLoadState::error);
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    synthSavedLoading ? "SAVED AND LOADING"
                                      : "SAVED - LOAD ERROR");
    }
  }
  if (synthSavedLoading) {
    const auto state = static_cast<SampleLoadState>(
        sampleLoadState.load(std::memory_order_acquire));
    if (std::strcmp(sampleRequestPath.data(), synthLastSaved.data()))
      synthSavedLoading = false;
    else if (state == SampleLoadState::error) {
      synthSavedLoading = false;
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    "SAVED - LOAD ERROR");
    } else if (state == SampleLoadState::complete) {
      synthSavedLoading = false;
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    "SAVED AND LOADED");
    }
  }
  if (!synthOpened || !synthWantsRender ||
      worker.state.load(std::memory_order_acquire) != W::idle)
    return;
  int slot = 0;
  while (slot == synthActive || slot == synthFade)
    ++slot;
  if (slot >= 3)
    return;
  auto job = synthQueuedRender;
  job.slot = slot;
  if (worker.post(job))
    synthWantsRender = false;
}
void mixSynth(std::size_t count) {
  if (!synthWorker || synthActive < 0)
    return;
  const auto read = [&](int slot, std::size_t &position, float &l, float &rr) {
    if (slot < 0)
      return;
    const auto &a = synthWorker->audio[slot];
    if (a.left.empty())
      return;
    if (position >= a.left.size()) {
      if (a.recipe.v[s3g_thru_synth::loop] > .5f)
        position = 0;
      else
        return;
    }
    l = a.left[position];
    rr = a.right[position];
    ++position;
  };
  const float step = 1.f / std::max(1.f, sampleRate * .008f);
  const auto &a = synthWorker->audio[synthActive];
  const bool drumOneShot = (s3g_thru_synth::isDrumType(a.recipe.type) ||
                            a.recipe.type == s3g_thru_synth::wtDrum) &&
                           a.recipe.v[s3g_thru_synth::loop] < .5f;
  for (std::size_t i = 0; i < count; ++i) {
    const float target = synthPreviewWanted ? 1.f : 0.f;
    synthPreviewGain += std::clamp(target - synthPreviewGain, -step, step);
    float l = 0, rr = 0;
    // Settle the audition route and retire its old tail before starting a
    // one-shot drum. Reading during the 8 ms ramp consumes the transient and
    // can attenuate its peak by several dB. The file already has clean edges.
    const bool waitForAttack = drumOneShot && synthPosition == 0 &&
                               (synthPreviewGain < 1 || synthFadeRemaining);
    if (!waitForAttack && (synthPreviewGain > 0 || synthPreviewWanted))
      read(synthActive, synthPosition, l, rr);
    if (synthFadeRemaining) {
      float oldL = 0, oldR = 0;
      read(synthFade, synthOldPosition, oldL, oldR);
      float t =
          1 - float(synthFadeRemaining) / std::max(1.f, sampleRate * .012f);
      t = t * t * (3 - 2 * t);
      l = oldL + (l - oldL) * t;
      rr = oldR + (rr - oldR) * t;
      if (--synthFadeRemaining == 0)
        synthFade = -1;
    }
    previewLevelState += (previewLevel - previewLevelState) * gainControlSlew;
    directPreview[0][i] += l * previewLevelState * synthPreviewGain;
    directPreview[1][i] += rr * previewLevelState * synthPreviewGain;
    if (synthPosition >= a.left.size() &&
        a.recipe.v[s3g_thru_synth::loop] < .5f)
      synthPreviewWanted = false;
  }
}
bool setSynthParam(const char *key, const char *text) {
  if (std::strncmp(key, "synth_", 6))
    return false;
  using namespace s3g_thru_synth;
  using W = s3g_thru_synth::Worker;
  if (!std::strcmp(key, "synth_close")) {
    saveSynthSession();
    synthOpened = false;
    synthPreviewWanted = false;
    synthWantsRender = false;
    if (synthWorker && synthWorker->request.op != W::save)
      synthWorker->cancel.store(true);
    return true;
  }
  if (!synthWorkerReady) {
    setError("Synthesis worker unavailable");
    return true;
  }
  if (!std::strcmp(key, "synth_open")) {
    if (audioRecording() || clipRecording || bankRecordSlot >= 0 ||
        clipPreparationInFlight() || projectLocked()) {
      setError("Finish recording or transition first");
      return true;
    }
    if (!synthInitialized) {
      const auto settings = settingsForPart(splitEnabled ? splitFocus : 0);
      synthRecipe.v[root] = float(settings.root);
      synthRecipe.scale = 0;
      for (std::size_t i = 0;
           i < kScaleDefinitions[std::size_t(settings.scale)].count; ++i)
        synthRecipe.scale |= std::uint16_t(
            1u << kScaleDefinitions[std::size_t(settings.scale)].intervals[i]);
      synthInitialized = true;
      synthSessionDirty = true;
      std::snprintf(synthName.data(), synthName.size(), "Spline-%d",
                    settings.root);
    }
    sequencePads = 0;
    sequencePadConsumed.fill(true);
    sequenceEditStep = -1;
    stopSequenceAudition();
    cancelPerformanceGestures();
    stopPreviewWithFade();
    synthOpened = true;
    if(partialsRecipeType(synthRecipe.type)||tableRecipeType(synthRecipe.type)||fmRecipeType(synthRecipe.type)||noiseRecipeType(synthRecipe.type)||synthRecipe.type==s3g_thru_synth::pluck||synthRecipe.type==s3g_thru_synth::drum)selectSynthType(s3g_thru_synth::resynth);
    if (!synthReady())
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    "CAPTURE: RENDER");
    clearError();
    return true;
  }
  if (!synthOpened)
    return true;
  if (std::strcmp(key, "synth_preview") && std::strcmp(key, "synth_render") &&
      std::strcmp(key, "synth_cancel"))
    synthSessionDirty = true;
  if (!std::strcmp(key, "synth_editor")) {
    int type = -1, page = 0, g0 = 0, g1 = 5, g2 = 4, keep = 1;
    float amount = .25f, low = -12, high = 12;
    char extra = 0;
    if (synthSession &&
        std::sscanf(text, "%d,%d,%d,%d,%d,%f,%f,%f,%d %c", &type, &page, &g0,
                    &g1, &g2, &amount, &low, &high, &keep, &extra) == 9 &&
        type == synthRecipe.type) {
      auto editor = synthSession->drafts[type].editor;
      editor.page = page;
      editor.groups = {g0, g1, g2};
      editor.amount = amount;
      editor.low = low;
      editor.high = high;
      editor.keepEnds = keep != 0;
      if ((keep == 0 || keep == 1) && validEditor(editor))
        synthSession->drafts[type].editor = editor;
    }
    return true;
  }
  if (!std::strcmp(key, "synth_preview")) {
    const bool preview = parseInteger(text, 0) != 0;
    if (preview &&
        (synthActive < 0 ||
         synthWorker->audio[synthActive].recipe.type != synthRecipe.type)) {
      synthPreviewWanted = false;
      std::snprintf(synthMessage.data(), synthMessage.size(),
                    "CAPTURE: RENDER");
      return true;
    }
    synthPreviewWanted = preview;
    if (synthPreviewWanted) {
      stopPreviewWithFade();
      if (synthActive >= 0 && synthPreviewGain > .0001f) {
        synthFade = synthActive;
        synthOldPosition = synthPosition;
        synthFadeRemaining =
            std::max<std::size_t>(1, std::size_t(sampleRate * .012f));
      }
      synthPosition = 0;
    }
    return true;
  }
  if (!std::strcmp(key, "synth_name")) {
    synthNameAutomatic = false;
    synthSetName(text);
    return true;
  }
  if (!std::strcmp(key, "synth_cancel")) {
    synthWorker->cancel.store(true);
    synthWantsRender = false;
    std::snprintf(synthMessage.data(), synthMessage.size(), "CANCELLED");
    return true;
  }
  if (!std::strcmp(key, "synth_render")) {
    requestSynthRender();
    return true;
  }
  if (!std::strcmp(key, "synth_save") || !std::strcmp(key, "synth_restore")) {
    if (synthWorker->state.load(std::memory_order_acquire) != W::idle) {
      setError("Wait for synthesis job");
      return true;
    }
    W::Request job;
    job.revision = synthRevision;
    job.rate = sampleRate;
    job.lane = int(selectedLane);
    job.clip = activeClip;
    job.sourceEpoch = ++synthSourceEpoch;
    if (!std::strcmp(key, "synth_save")) {
      if (!synthReady()) {
        setError("Render current settings before saving");
        return true;
      }
      job.op = W::save;
      job.recipe = synthRecipe; // Capture current protection without changing
                                // preview audio.
      job.slot = synthActive;
      job.load = parseInteger(text, 0) > 0;
      std::snprintf(job.directory.data(), job.directory.size(), "%s",
                    synthDirectory.data());
      std::snprintf(job.name.data(), job.name.size(), "%s", synthName.data());
    } else {
      job.op = W::restore;
      std::snprintf(job.source.data(), job.source.size(), "%s",
                    currentLaneSourcePaths[selectedLane].data());
    }
    clearError();
    synthWorker->post(job);
    return true;
  }
  if (!std::strcmp(key, "synth_type")) {
    selectSynthType(parseInteger(text, synthRecipe.type));
    return true;
  }
  if (!std::strcmp(key, "synth_primary_file") ||
      !std::strcmp(key, "synth_impulse_file")) {
    const int which = !std::strcmp(key, "synth_impulse_file") ? 1 : 0;
    if (synthRecipe.type != convolution || text[0] != '/' ||
        std::strlen(text) >= synthConvPaths[which].size()) {
      setError("Choose an absolute WAV path");
      return true;
    }
    std::snprintf(synthConvPaths[which].data(), synthConvPaths[which].size(),
                  "%s", text);
    synthConvEmbedded[which] = false;
    synthConvEpochs[which] = ++synthSourceEpoch;
    const char *leaf = std::strrchr(text, '/');
    leaf = leaf ? leaf + 1 : text;
    std::size_t n = 0;
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(leaf);
         *c && n + 1 < synthConvLabels[which].size(); ++c)
      if (*c >= 32 && *c < 127 && *c != '"' && *c != '\\')
        synthConvLabels[which][n++] = char(*c);
    synthConvLabels[which][n] = 0;
    clearError();
    synthEdited();
    return true;
  }
  if (!std::strncmp(key, "synth_hybrid", 12) && key[12] >= '0' &&
      key[12] <= '3' && !std::strcmp(key + 13, "_file")) {
    const int index = key[12] - '0';
    if (synthRecipe.type != hybrid || text[0] != '/' ||
        std::strlen(text) >= synthHybridPaths[index].size()) {
      setError("Choose an absolute WAV path");
      return true;
    }
    std::snprintf(synthHybridPaths[index].data(),
                  synthHybridPaths[index].size(), "%s", text);
    synthHybridEpochs[index] = ++synthSourceEpoch;
    synthHybridEmbedded[index] = false;
    const char *leaf = std::strrchr(text, '/');
    leaf = leaf ? leaf + 1 : text;
    std::size_t at = 0;
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(leaf);
         *c && at + 1 < synthHybridLabels[index].size(); ++c)
      if (*c >= 32 && *c < 127 && *c != '"' && *c != '\\')
        synthHybridLabels[index][at++] = char(*c);
    synthHybridLabels[index][at] = 0;
    clearError();
    synthEdited();
    return true;
  }
  if (!std::strcmp(key, "synth_transfer") && synthRecipe.type == hybrid && synthRecipe.hybridStyle) {
    int field = -1; float value = 0; char extra = 0;
    if (std::sscanf(text, "%d,%f %c", &field, &value, &extra) == 2 &&
        setHybridTransfer(synthRecipe, field, value)) synthEdited();
    return true;
  }
  if (!std::strcmp(key, "synth_transfer_lock")) {
    const int field = parseInteger(text, -1);
    if (field >= 0 && field < hybridTransferFields) synthRecipe.hybridLocks ^= 1u << field;
    return true;
  }
  if (!std::strcmp(key, "synth_hybrid_upgrade") && synthRecipe.type == hybrid) {
    synthRecipe.hybridStyle = 1; synthRecipe.transfer = hybridTransferDefaults;
    synthRecipe.transfer[htSources] = synthRecipe.hybridParams[hSources];
    synthEdited(); return true;
  }
  if (!std::strcmp(key, "synth_work_lock")) {
    const int bit = parseInteger(text, -1);
    if (bit >= 0 && (bit < 45 || (bit >= 48 && bit < 60)))
      synthRecipe.workshopLocks ^= 1ULL << bit;
    return true;
  }
  if ((!std::strcmp(key, "synth_pluck") || !std::strcmp(key, "synth_sub") ||
       !std::strcmp(key, "synth_hybrid") || !std::strcmp(key, "synth_trace") ||
       !std::strcmp(key, "synth_events") || !std::strcmp(key, "synth_timing") ||
       !std::strcmp(key, "synth_attack") || !std::strcmp(key, "synth_color"))) {
    int field = -1;
    float value = 0;
    char extra = 0;
    const int group =
        !std::strcmp(key, "synth_pluck")
            ? 0
            : !std::strcmp(key, "synth_sub")
                  ? 1
                  : !std::strcmp(key, "synth_hybrid")
                        ? 2
                        : !std::strcmp(key, "synth_trace")
                              ? 3
                              : !std::strcmp(key, "synth_events")
                                    ? 4
                                    : !std::strcmp(key, "synth_timing")
                                          ? 5
                                          : !std::strcmp(key, "synth_attack")
                                                ? 6
                                                : 7;
    const bool relevant =
        (group == 0 && synthRecipe.type == pluck) ||
        (group == 1 && synthRecipe.type == subharmonic) ||
        (group == 2 && synthRecipe.type == hybrid) ||
        (group == 3 && synthRecipe.type == resynth) ||
        ((group == 4 || group == 5) && supportsEvents(synthRecipe.type)) ||
        ((group == 6 || group == 7) && isDrumType(synthRecipe.type));
    if (relevant &&
        std::sscanf(text, "%d,%f %c", &field, &value, &extra) == 2) {
      const auto old = workshopGroup(synthRecipe, group);
      if (setWorkshop(synthRecipe, group, field, value) &&
          old != workshopGroup(synthRecipe, group))
        synthEdited();
    }
    clearError();
    return true;
  }
  if (!std::strcmp(key, "synth_extended_lock")) {
    const int bit = parseInteger(text, -1);
    if (bit >= 0 && bit < 24)
      synthRecipe.extendedLocks ^= 1u << bit;
    return true;
  }
  if (!std::strcmp(key, "synth_wt_start")) {
    if (synthRecipe.type == wtDrum && parseInteger(text, 0) > 0) {
      initializeWtDrumModel(synthRecipe, int(synthRecipe.wt[wtModel]));
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_wt_lock")) {
    const int f = parseInteger(text, -1);
    if (f >= 0 && f < wtFields) synthRecipe.wtLocks ^= 1u << f;
    return true;
  }
  if (!std::strcmp(key, "synth_drum_start")) {
    if (isDrumType(synthRecipe.type) && parseInteger(text, 0) > 0) {
      initializeDrumModel(synthRecipe, int(synthRecipe.drums[dModel]));
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_clean")) {
    if (parseInteger(text, 0) > 0) {
      clean(synthRecipe);
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_source")) {
    const auto &path = currentLaneSourcePaths[selectedLane];
    if (audioRecording() || !path[0]) {
      setError("Finish saving a lane sample first");
      return true;
    }
    synthSourcePath = path;
    synthSourceEmbedded = false;
    ++synthSourceEpoch;
    const char *leaf = std::strrchr(path.data(), '/');
    leaf = leaf ? leaf + 1 : path.data();
    std::size_t at = 0;
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(leaf);
         *c && at + 1 < synthSourceLabel.size(); ++c)
      if (*c >= 32 && *c < 127 && *c != '"' && *c != '\\')
        synthSourceLabel[at++] = char(*c);
    synthSourceLabel[at] = 0;
    if (!at)
      std::snprintf(synthSourceLabel.data(), synthSourceLabel.size(),
                    "LANE %d SOURCE", int(selectedLane) + 1);
    if (synthRecipe.type == resynth)
      synthEdited();
    clearError();
    return true;
  }
  if (!std::strcmp(key, "synth_dust_lock")) {
    const int bit = parseInteger(text, -1);
    if (bit >= 0 && bit < dustFields)
      synthRecipe.dustLocks ^= 1u << bit;
    return true;
  }
  int f = -1;
  float value = 0;
  char extra = 0;
  if (!std::strcmp(key, "synth_edit") &&
      std::sscanf(text, "%d,%f %c", &f, &value, &extra) == 2) {
    if (f == fields && std::isfinite(value)) {
      const auto seed =
          std::uint32_t(std::lround(std::clamp(value, 0.f, 65535.f)));
      if (seed == synthRecipe.seed)
        return true;
      synthRecipe.seed = seed;
    } else {
      if (f < 0 || f >= fields)
        return true;
      const float before = synthRecipe.v[f];
      if (!set(synthRecipe, f, value) || before == synthRecipe.v[f])
        return true;
    }
    synthEdited();
    return true;
  }
  if ((!std::strcmp(key, "synth_wave") || !std::strcmp(key, "synth_bloom") ||
       !std::strcmp(key, "synth_mod") || !std::strcmp(key, "synth_resynth") ||
       !std::strcmp(key, "synth_additive") ||
       !std::strcmp(key, "synth_noise") || !std::strcmp(key, "synth_fm") ||
       !std::strcmp(key, "synth_conv") || !std::strcmp(key, "synth_drum") ||
       !std::strcmp(key, "synth_dust") || !std::strcmp(key, "synth_wt")) &&
      std::sscanf(text, "%d,%f %c", &f, &value, &extra) == 2) {
    bool changed = false;
    if (!std::strcmp(key, "synth_wave") && synthRecipe.type == wave) {
      const auto before = synthRecipe.osc;
      changed = setWave(synthRecipe, f, value) && before != synthRecipe.osc;
    }
    if (!std::strcmp(key, "synth_bloom") && synthRecipe.type == bloom) {
      const auto before = synthRecipe.packets;
      changed =
          setBloom(synthRecipe, f, value) && before != synthRecipe.packets;
      if (f == bSpectrum)
        synthSpectrum = int(synthRecipe.packets[bSpectrum]);
    }
    if (!std::strcmp(key, "synth_mod")) {
      const auto before = synthRecipe.mod;
      changed = setMod(synthRecipe, f, value) && before != synthRecipe.mod;
    }
    if (!std::strcmp(key, "synth_resynth") && synthRecipe.type == resynth) {
      const auto before = synthRecipe.spectral;
      changed =
          setResynth(synthRecipe, f, value) && before != synthRecipe.spectral;
    }
    if (!std::strcmp(key, "synth_additive") &&
        synthRecipe.type == partialsType) {
      const auto before = synthRecipe.additive;
      changed =
          setAdditive(synthRecipe, f, value) && before != synthRecipe.additive;
    }
    if (!std::strcmp(key, "synth_noise") && synthRecipe.type == noiseBank) {
      const auto before = synthRecipe.noise;
      changed = setNoise(synthRecipe, f, value) && before != synthRecipe.noise;
    }
    if (!std::strcmp(key, "synth_fm") && synthRecipe.type == fm) {
      const auto before = synthRecipe.fmOsc;
      changed = setFm(synthRecipe, f, value) && before != synthRecipe.fmOsc;
    }
    if (!std::strcmp(key, "synth_conv") && synthRecipe.type == convolution) {
      const auto before = synthRecipe.conv;
      changed =
          setConvolution(synthRecipe, f, value) && before != synthRecipe.conv;
    }
    if (!std::strcmp(key, "synth_drum") && isDrumType(synthRecipe.type)) {
      const auto before = synthRecipe.drums;
      changed = setDrum(synthRecipe, f, value) && before != synthRecipe.drums;
    }
    if (!std::strcmp(key, "synth_dust") && synthRecipe.type == dust) {
      const auto before = synthRecipe.dustParams;
      changed =
          setDust(synthRecipe, f, value) && before != synthRecipe.dustParams;
    }
    if (!std::strcmp(key, "synth_wt") && synthRecipe.type == wtDrum) {
      const auto before = synthRecipe.wt;
      changed = setWtDrum(synthRecipe, f, value) && before != synthRecipe.wt;
    }
    if (changed)
      synthEdited();
    return true;
  }
  if (!std::strcmp(key, "synth_detail_lock")) {
    f = parseInteger(text, -1);
    if (f >= 0 && f < detailLockCount)
      synthRecipe.detailLocks ^= 1ULL << f;
    return true;
  }
  int spectrum = -1, component = -1, partField = -1;
  if (!std::strcmp(key, "synth_spec_select") &&
      (synthRecipe.type == bloom || synthRecipe.type == partialsType ||
       synthRecipe.type == noiseBank || synthRecipe.type == subharmonic) &&
      std::sscanf(text, "%d,%d %c", &spectrum, &component, &extra) == 2 &&
      spectrum >= 0 && spectrum < 4 && component >= 0 && component < 8) {
    synthSpectrum = spectrum;
    synthComponent = component;
    if (synthRecipe.type == bloom && synthRecipe.packets[bMode] == 0 &&
        synthRecipe.packets[bSpectrum] != spectrum) {
      setBloom(synthRecipe, bSpectrum, float(spectrum));
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_spec_edit") &&
      (synthRecipe.type == bloom || synthRecipe.type == partialsType ||
       synthRecipe.type == noiseBank || synthRecipe.type == subharmonic) &&
      std::sscanf(text, "%d,%d,%d,%f %c", &spectrum, &component, &partField,
                  &value, &extra) == 4) {
    if (setPartial(synthRecipe, spectrum, component, partField, value))
      synthEdited();
    return true;
  }
  if (!std::strcmp(key, "synth_seed_next")) {
    synthRecipe.seed = (synthRecipe.seed + 1) % 65536;
    synthEdited();
    return true;
  }
  if (!std::strcmp(key, "synth_mutate")) {
    value = parseFloat(text, 0);
    if (value > 0 && std::isfinite(value)) {
      mutate(synthRecipe, value);
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_lock")) {
    f = parseInteger(text, -1);
    if (f >= 0 && f < fields)
      synthRecipe.locks ^= 1u << f;
    return true;
  }
  int row = -1, at = -1, field = -1;
  if ((!std::strcmp(key, "synth_point") || !std::strcmp(key, "synth_select") ||
       !std::strcmp(key, "synth_delete")) &&
      std::sscanf(text, "%d,%d %c", &row, &at, &extra) == 2 && row >= 0 &&
      row < 4 && at >= 0 && at < 8) {
    synthRow = row;
    synthPoint = at;
    auto &c = synthRecipe.curves[row];
    if (!std::strcmp(key, "synth_delete")) {
      c.enabled &= ~(1u << at);
      c.enabled |= 0x81;
      c.p[at].value = 0;
      synthEdited();
    } else if (!std::strcmp(key, "synth_point") && !(c.enabled & (1u << at))) {
      c.enabled |= 1u << at;
      synthEdited();
    }
    return true;
  }
  if (!std::strcmp(key, "synth_point_edit") &&
      std::sscanf(text, "%d,%d,%d,%f %c", &row, &at, &field, &value, &extra) ==
          4) {
    if (editPoint(synthRecipe, row, at, field, value))
      synthEdited();
    return true;
  }
  float low = 0, high = 0, amount = 0;
  int ends = 0;
  if (!std::strcmp(key, "synth_curve_random") &&
      std::sscanf(text, "%d,%f,%f,%f,%d %c", &row, &low, &high, &amount, &ends,
                  &extra) == 5 &&
      std::isfinite(low) && std::isfinite(high) && std::isfinite(amount) &&
      amount > 0 && row >= 0 && row < 4) {
    randomCurve(synthRecipe, row, low, high, std::clamp(amount, 0.f, 1.f),
                ends != 0);
    synthEdited();
    return true;
  }
  return true;
}
int formatSynthSpectrum(int row, char *out, int capacity) const {
  if (row < 0 || row > 3 || !out || capacity < 129)
    return -1;
  constexpr char hex[] = "0123456789abcdef";
  std::size_t at = 0;
  for (int i = 0; i < 8; ++i)
    for (float value :
         {synthRecipe.ratios[row][i], synthRecipe.partials[row][i]}) {
      std::uint32_t bits;
      std::memcpy(&bits, &value, 4);
      for (int j = 0; j < 4; ++j) {
        const auto b = std::uint8_t(bits >> (j * 8));
        out[at++] = hex[b >> 4];
        out[at++] = hex[b & 15];
      }
    }
  out[at] = 0;
  return int(at);
}
int formatSynth(char *out, int capacity) const {
  if (!out || capacity < 1)
    return -1;
  std::array<char, s3g_thru_synth::maximumRecipeBytes * 2 + 1> model{};
  std::size_t at = 0;
  constexpr char hex[] = "0123456789abcdef";
  s3g_thru_synth::serializeRecipe(synthRecipe, [&](std::uint8_t b) {
    model[at++] = hex[b >> 4];
    model[at++] = hex[b & 15];
  });
  const int count = std::snprintf(
      out, capacity,
      "{\"open\":%d,\"job\":%d,\"op\":%d,\"progress\":%u,\"ready\":%d,"
      "\"preview\":%d,\"rev\":%"
      "llu,\"row\":%d,\"point\":%d,\"spectrum\":%d,\"component\":%d,\"name\":"
      "\"%s\",\"message\":"
      "\"%s\",\"source\":\"%s\",\"primary\":\"%s\",\"impulse\":\"%s\","
      "\"inputs\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"eventfit\":%d,\"seconds\":%."
      "5f,\"recipe\":\"%"
      "s\",\"wave\":\"%s\"}",
      synthOpened ? 1 : 0,
      synthWorker ? synthWorker->state.load(std::memory_order_acquire) : -1,
      synthWorker ? int(synthWorker->request.op) : 0,
      synthWorker ? synthWorker->progress.load() : 0, synthReady() ? 1 : 0,
      synthPreviewWanted ? 1 : 0,
      static_cast<unsigned long long>(synthRevision), synthRow, synthPoint,
      synthSpectrum, synthComponent, synthName.data(), synthMessage.data(),
      synthRecipe.type == s3g_thru_synth::resynth ? synthSourceLabel.data() : "",
      synthRecipe.type == s3g_thru_synth::convolution ? synthConvLabels[0].data() : "",
      synthRecipe.type == s3g_thru_synth::convolution ? synthConvLabels[1].data() : "",
      synthRecipe.type == s3g_thru_synth::hybrid ? synthHybridLabels[0].data() : "",
      synthRecipe.type == s3g_thru_synth::hybrid ? synthHybridLabels[1].data() : "",
      synthRecipe.type == s3g_thru_synth::hybrid ? synthHybridLabels[2].data() : "",
      synthRecipe.type == s3g_thru_synth::hybrid ? synthHybridLabels[3].data() : "",
      s3g_thru_synth::eventFitCount(synthRecipe, sampleRate),
      synthActive >= 0 &&
              synthWorker->audio[synthActive].recipe.type == synthRecipe.type
          ? double(synthWorker->audio[synthActive].left.size()) / sampleRate
          : 0.0,
      model.data(),
      synthActive >= 0 &&
              synthWorker->audio[synthActive].recipe.type == synthRecipe.type
          ? synthWorker->audio[synthActive].waveText.data()
          : "");
  return count >= capacity ? -1 : count;
}
