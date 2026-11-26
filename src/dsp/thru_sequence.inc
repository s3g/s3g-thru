bool sequenceMode() const { return sequenceLive.active; }
bool sequenceRunning() const {
  return sequenceMode() && (clipPlaying || clipRecording);
}
void publishSequenceRates() {
  for (int part = 0; part < 4; ++part)
    sequenceRequestedRate[part].store(sequenceLive.tracks[part].rate,
                                      std::memory_order_release);
}
void resetSequenceClock() {
  if(sequenceSoundEditClip!=activeClip){sequenceEditStep=-1;sequenceSoundEditPart=sequenceSoundEditClip=-1;}
  resetSequenceSoundRuntime();
  sequenceBeat = 0;sequenceClockPrimed=false;
  sequenceBeatOffsets.fill(0);
  sequenceEventCount = {};sequenceWindowStarts={};
  sequenceCursor.fill(-1);
  for (auto &cursor : sequencePublishedCursor)
    cursor.store(-1, std::memory_order_release);
  sequenceLastRate.fill(-1);
  sequenceEmittedTick.fill(-1);
  invalidateSequenceRecipePlayback();
  for(int p=0;p<4;++p){
    auto& lane=sequenceLive.tracks[p];
    if(!lane.directionSeed)lane.directionSeed=s3g_thru_sequence::hash(0x44495231u^
        (std::uint32_t(activeClip+1)*0x9e3779b9u)^(std::uint32_t(p+1)*0x85ebca6bu))|1u;
    sequenceDirections[p].reset(lane);
  }
  sequencePlayback = sequenceLive;
  refreshSequenceGeneratePlayback();
  publishSequenceRates();
  for (auto &lane : sequenceLastEvent)
    for (auto &step : lane)
      step.fill(-1);
}
void ensureSequenceClip() {
  auto *clip = activeClipSlot();
  if (!clip)
    return;
  if (!clip->clipRecorded) {
    clip->clipRecorded = true;
    clip->takeCount = 1;
    clip->recordedTempoBpm = tempoBpm;
    clip->recordedBeats = 4;
    clip->timingMode = ClipTimingMode::beatSync;
    clip->clipDurationMs = std::uint32_t(std::lround(240000.f / tempoBpm));
  }
  clip->occupied = activeClipOccupied = true;
}
void sequenceChanged(bool undo = true) {
  if (undo && !clipRecording && !audioRecording() &&
      (sequenceUndoLane != splitFocus || sequenceUndoStep != sequenceStep)) {
    captureActiveClip(false);
    pushClipUndo(activeClip);
    sequenceUndoLane = splitFocus;
    sequenceUndoStep = sequenceStep;
  }
  ensureSequenceClip();
}
std::int64_t sequenceLaneTick(int part, int rate) const {
  const double localBeat = sequenceBeat + sequenceBeatOffsets[part];
  return std::max<std::int64_t>(
      0, std::int64_t(std::floor(localBeat / s3g_thru_sequence::rates[rate] +
                                 1e-9)));
}
int sequenceLaneCursor(int part, const s3g_thru_sequence::Lane &lane) const {
  const auto tick=sequenceLaneTick(part,lane.rate);
  const int cell=sequenceClockPrimed?sequenceDirections[part].at(lane,tick).step
      :s3g_thru_sequence::directionPosition(lane,tick).step;
  return cell<lane.length?cell:-1; // A removed cell may only finish its committed Burst.
}
void syncSequenceRatePhases() {
  for (int part = 0; part < 4; ++part) {
    const int rate = sequencePlayback.tracks[part].rate;
    const int previous = sequenceLastRate[part];
    if (rate == previous) continue;
    if (previous >= 0) {
      // Keep the absolute lane step number and fraction through a RATE edit.
      // Each lane may change independently, including recorded RATE gestures.
      const double phase = (sequenceBeat + sequenceBeatOffsets[part]) /
                           s3g_thru_sequence::rates[previous];
      sequenceBeatOffsets[part] =
          std::max(0.0, phase) * s3g_thru_sequence::rates[rate] -
          sequenceBeat;
    }
    sequenceLastRate[part] = rate;
    // The step tick remains monotonic, so the existing event deduplication
    // history stays valid across a live RATE change.
  }
}
void commitSequenceEdit(bool deferPlayback = false) {
  rememberSequenceTypeEdit();
  captureSequenceRecipeRecording(splitFocus);
  invalidateSequenceRecipePlayback();
  // Apply the RATE target and phase together before scheduling the block.
  // Schwung serializes live parameter/MIDI/render callbacks on its SPI thread;
  // atomic targets also support alternative hosts that dispatch controls separately.
  if (!deferPlayback) {
    sequencePlayback = sequenceLive;
    refreshSequenceGeneratePlayback();
    syncSequenceRatePhases();
  }
  if (auto *clip = activeClipSlot())
    clip->sequence = sequenceLive;
  markPersistentStateDirty();
  ++sequenceRevision;
}
void startSequenceVoice(int part, int row, int child, double gateBeats,
                        bool audition = false, std::int64_t tick = -1) {
  if (part < 0 || part >= 4) return;
  const auto& source=audition?liveControlsForPart(part):splitClipControls[part];
  if(!engine.laneLoaded(part) && !Engine::sourceFree(source.algorithm)
      && !(sourceMotionEngine(source.algorithm)&&source.sourceMotion.v[8]!=0)) return;
  const auto &lane = sequencePlayback.tracks[part];
  const auto &step = audition?sequenceLive.tracks[part].trigs[row]
      :tick<0?lane.trigs[row]:sequenceDirections[part].step(lane,tick,row);
  auto program =
      audition ? liveControlsForPart(part) : splitClipControls[part];
  const auto sound=sequenceTriggerSound(part,row,child,tick,audition,step);
  applySequenceVoiceSound(program,sound,part);
  const auto* sliceLock=program.algorithm==Engine::PlaybackAlgorithm::slices?sound.find(s3g_thru_sequence::sliceLockId):nullptr;
  SequenceSliceInfo slice;
  if(sliceLock){
    int kind=0;const auto map=sliceMapForLane(std::size_t(part),program,kind);
    slice=sequenceSliceInfo(program,map,kind,0,&sound);
    if(engine.laneFrames(part))slice.lead=slicePreRoll(part,kind)*.001f*engine.laneSampleRate(part)/engine.laneFrames(part);
    if(!slice.valid)return; // A missing explicit address never steals a sounding voice.
  }
  sequenceLaneSoundLocks[part]=sound;
  triggerControlLfos(part,&step.lfos,child==0);
  const float velocity =
      s3g_thru_sequence::resolve(lane, step, child, 2, lane.velocity);
  auto &voice = allocateClipVoice(part);
  const bool reuse = voice.active;
  if (reuse)
    beginVoiceStealTransition(voice);
  if (voice.active && !voice.clipOwned)
    clearPadMappingForVoice(voice);
  const auto settings = settingsForPart(part);
  voice.sequence = {};
  voice.sequence.active = true;
  voice.sequence.soundLocks = sound;
  voice.sequence.audition = audition;
  voice.sequence.generatedPreview = !audition&&sequenceGenerateActive&&sequenceGenerateValid&&sequenceGeneratePart==part;
  voice.sequence.step = row;
  voice.sequence.child = child;
  auto seekProgram = program;
  applyControlLfoWindow(seekProgram,part,true);
  float inheritedPosition=seekProgram.locus;
  if(!audition&&tick>=0) {
    auto &start=sequenceWindowStarts[part][std::size_t(tick)%8];
    // The earliest Burst child owns the tick, even when WHEN reorders it.
    if(start.tick!=tick)start={tick,inheritedPosition};
    inheritedPosition=start.position;
  }
  voice.sequence.position =
      s3g_thru_sequence::resolve(lane, step, child, 0, inheritedPosition);
  seekProgram.locus = voice.sequence.position;
  const auto window = parameterDisplayWindow(seekProgram, part);
  voice.sequence.startPhase =
      clampValue((voice.sequence.position - window[0]) /
                     std::max(.000001f, window[1] - window[0]),
                 0.f, .999999f);
  if(sliceLock){
    voice.sequence.sliceIndex=slice.number-1;
    voice.sequence.position=slice.low;
    voice.sequence.startPhase=0;
  }
  const double speed =
      activeClipSlot()
          ? clampValue(double(activeClipSlot()->clipRate), .25, 4.0)
          : 1.0;
  const double increment = tempoBpm / (60.0 * sampleRate) * speed;
  double gateMs = lane.timeMode
      ? clampValue(double(s3g_thru_sequence::resolveTime(lane, step, child)), 10.0, 30000.0)
      : std::max(.25, gateBeats / increment * 1000 / sampleRate);
  if(!audition&&tick>=0&&s3g_thru_generate::usesLiveGap(lane,row,child,&step)){
    const double stepMs=s3g_thru_sequence::rates[lane.rate]*1000.0/(increment*sampleRate);
    const double gap=s3g_thru_sequence::nextHitGap(lane,row,child,tick,&sequenceDirections[part]);
    const auto& recipe=lane.generator.values;
    const double duration=clampValue(gap*(lane.timeMode?stepMs:1.0),double(recipe[17]),double(recipe[18]));
    gateMs=std::max(.25,duration*(lane.timeMode?1.0:stepMs));
  }
  const double attack = std::max(0.f, program.ampAttackMs),
               decay = std::max(0.f, program.ampDecayMs),
               release = std::max(3.f, program.ampReleaseMs);
  const double scale =
      std::min(1.0, gateMs / (attack + decay + release + 1e-9));
  voice.sequence.attack = float(attack * scale);
  voice.sequence.decay = float(decay * scale);
  voice.sequence.release = float(release * scale);
  voice.sequence.frames =
      std::max<std::uint64_t>(1, std::llround(gateMs * sampleRate / 1000));
  const auto releaseFrames =
      std::min(std::max<std::uint64_t>(1, voice.sequence.frames - 1),
               std::uint64_t(std::max(1LL, std::llround(voice.sequence.release *
                                                        sampleRate / 1000))));
  voice.sequence.releaseAt = voice.sequence.frames - releaseFrames;
  voice.engineSlot = -1;
  voice.assignedAlgorithm = -1;
  voice.entryVariation = {};
  voice.note = clampValue(
      settings.root +
          int(std::lround(s3g_thru_sequence::resolve(lane, step, child, 1, 0))),
      0, 127);
  voice.sourcePad = -1;
  voice.velocity = int(velocity);
  voice.part = part;
  voice.variantLane = std::size_t(part);
  voice.age = ++noteClock;
  voice.clipOwned = !audition;
  voice.clipNoteIndex = -1;
  voice.pulsarSeed = audition ? 0u : sequencePulsarSeed(part, row, child, tick);
  voice.midiExternal = false;
  voice.midiGain = velocity / 127.f;
  voice.midiValue = voice.midiTarget = {};
  voice.pressureTarget = voice.pressureState = 0;
  voice.pressureBrakeHeld = false;
  voice.sliceCached = voice.sliceAudition = false;
  if(sliceLock){
    voice.sliceCached=true;voice.sliceAbsolute=true;voice.sliceCachedLane=part;
    voice.sliceSourceRevision=wavesetSourceRevisions[part];
    voice.sliceLow=slice.low;voice.sliceHigh=slice.high;voice.sliceLead=slice.lead;
  }
  voice.laneOffset = voice.cutLanePressure = 0;
  voice.liveStackPhaseValid = voice.recordedStackPhaseValid = false;
  voice.bankAsset = voice.bankSlot = -1;
  voice.active = voice.held = true;
  if (!reuse) {
    voice.envelope = 0;
    voice.gainState = (audition ? outputGain : clipOutputGain) * voice.midiGain;
    voice.envelopeSustain = program.ampSustain;
    voice.stealPending = false;
    voice.stealFadeRemaining = 0;
    voice.panState = 0;
    voice.pressureToneState = {};
    voice.pressureToneMix = 0;
    beginVoiceAttack(voice);
    voice.engine.retrigger(false);
    voice.engine.seekNormalized(voice.sequence.startPhase);
  }
}
void auditionSequenceStep() {
  if (sequenceRunning() || sequenceEditStep < 0)
    return;
  stopSequenceAudition();
  sequencePlayback = sequenceLive;
  refreshSequenceGeneratePlayback();
  const auto &lane = sequenceLive.tracks[splitFocus];
  const int child = std::max(0, sequenceChild - 1);
  const float duration = s3g_thru_sequence::resolve(
      lane, lane.trigs[sequenceEditStep], child, 3, lane.gate);
  startSequenceVoice(splitFocus, sequenceEditStep, child,
                     s3g_thru_sequence::rates[lane.rate] * duration, true);
}
void stopSequenceAudition() {
  for (auto &voice : voices)
    if (voice.sequence.active && voice.sequence.audition) {
      voice.held = false;
      beginVoiceRelease(voice);
      const auto remaining =
          voice.sequence.frames > voice.sequence.elapsed
              ? voice.sequence.frames - voice.sequence.elapsed
              : 1;
      voice.sequence.releaseAt = voice.sequence.elapsed;
      voice.sequence.frames =
          voice.sequence.elapsed +
          std::min(remaining, std::max<std::uint64_t>(
                                  1, std::llround(sampleRate * .003f)));
    }
}
void sequencePadDown(int pad) {
  if(sequenceGenerateSourceCurrent()){sequenceGenerateIgnoredPads|=1u<<pad;return;}
  const int step = s3g_thru_sequence::padStep(pad);
  sequenceStep = step;
  sequenceChild = 0;
  sequenceUndoLane = sequenceUndoStep = -1;
  sequencePadParts[pad] = splitFocus;
  sequencePadTimes[pad] = renderClockSamples.load(std::memory_order_relaxed);
  sequencePadConsumed[pad] = false;
  sequencePads |= 1u << pad;
  if (deleteHeld) {
    sequenceChanged();
    sequenceLive.tracks[splitFocus].trigs[step] = {};
    sequenceLive.soundLocks.eraseStep(splitFocus,step);
    rememberSequenceTypeEdit(splitFocus,step,0,"seq_delete");
    commitSequenceEdit();
    sequencePadConsumed[pad] = true;
  }
}
void sequencePadUp(int pad) {
  if(sequenceGenerateIgnoredPads&(1u<<pad)){sequenceGenerateIgnoredPads&=~(1u<<pad);return;}
  if (!(sequencePads & (1u << pad)))
    return;
  sequencePads &= ~(1u << pad);
  const auto held = renderClockSamples.load(std::memory_order_relaxed) -
                    sequencePadTimes[pad];
  const int step = s3g_thru_sequence::padStep(pad);
  if (!sequencePadConsumed[pad] && held < std::uint64_t(sampleRate * .3f)) {
    sequenceChanged();
    auto &target = sequenceLive.tracks[sequencePadParts[pad]].trigs[step];
    const int owner=sequencePadParts[pad];
    const bool enabled=sequenceRunning()?sequencePlayback.tracks[owner].trigs[step].enabled:target.enabled;
    target.enabled = !enabled;
    rememberSequenceTypeEdit(owner,step,0,"seq_toggle");
    commitSequenceEdit();
  }
  if (sequenceEditStep == step) {
    sequenceEditStep = -1;
    stopSequenceAudition();
  }
}
// A lane's authored settings and recorded lane gestures share one bounded
// descriptor path. Step/child edits remain clip content, never launch notes.
float sequenceLaneValue(int part, int field, bool effective=false) const {
  const bool playing=effective&&sequenceRunning();
  const auto &l = (playing?sequencePlayback:sequenceLive).tracks[part];
  const float values[]{float(l.length),
                       float(l.rate),
                       float(l.rotation),
                       l.enabled ? 1.f : 0.f,
                       playing?float(sequenceMuted[part]):float(settingsForPart(part).muted),
                       l.velocity,
                       l.gate,
                       l.scaleEntry ? 1.f : 0.f,
                       l.timeMode ? 1.f : 0.f,
                       l.timeMs,
                       float(l.direction),float(l.path)};
  return values[field];
}
void setSequenceLaneValue(s3g_thru_sequence::Pattern &patch, int part,
                          int field, float value) {
  auto &l = patch.tracks[part];
  switch (field) {
  case 0:
    l.length = clampValue(int(value), 1, 32);
    break;
  case 1:
    l.rate = clampValue(int(value), 0, 8);
    break;
  case 2:
    l.rotation = clampValue(int(value), 0, 31);
    break;
  case 3:
    l.enabled = value >= .5f;
    break;
  case 5:
    l.velocity = clampValue(std::round(value), 1.f, 127.f);
    break;
  case 6:
    l.gate = clampValue(value, .01f, 32.f);
    break;
  case 7:
    l.scaleEntry = value >= .5f;
    break;
  case 8:
    l.timeMode = value >= .5f;
    break;
  case 9:
    l.timeMs = clampValue(value, 10.f, 30000.f);
    break;
  case 10:
    l.direction = int(std::lround(clampValue(value,0.f,4.f)));
    break;
  case 11:
    l.path = int(std::lround(clampValue(value,0.f,2.f)));
    break;
  default:
    break;
  }
}
// Keep a touched overdub control audible and recorded across old gesture
// edges, while retaining every prior-take point for Undo. At most one point
// is added per encountered edge; no sorting or full-clip copy on render.
void captureSequenceOverdubHold(ClipPointType type,int part,int field,float value){
  if(!clipRecording||clipReplacing)return;
  auto* clip=activeClipSlot();if(!clip)return;
  const auto curve=type==ClipPointType::sequenceLane
      ?kClipSequenceOffset+s3g_thru_sequence::laneCurveOffset(part,field)
      :sequenceRecipeCurve(part,field);
  int previous=clipLastPointIndices[curve];if(previous<0)return;
  const auto time=clipCaptureTimeMs();
  if(clip->clipPoints[previous].timeMs>time){
    captureClipPoint(type,field,part,value,time,true);previous=clipLastPointIndices[curve];
  }
  const auto first=clip->clipPoints.begin()+clip->clipPointStarts[curve];
  const auto last=first+clip->clipPointCounts[curve];
  const auto next=std::upper_bound(first,last,time,[](auto when,const ClipPoint& point){return when<point.timeMs;});
  if(next!=first){const auto& edge=*(next-1);
    if(edge.timeMs>clip->clipPoints[previous].timeMs&&edge.value!=value)
      captureClipPoint(type,field,part,value,edge.timeMs,true);
  }
}
void seedSequenceRecording() {
  if (!sequenceMode())
    return;
  if (clipReplacing)
    resetSequenceClock();
  for (int part = 0; part < 4; ++part)
    for (int f = 0; f < s3g_thru_sequence::laneFields; ++f)
      if(clipReplacing||!activeClipSlot()->clipPointCounts[kClipSequenceOffset+s3g_thru_sequence::laneCurveOffset(part,f)])
        captureClipPoint(ClipPointType::sequenceLane, f, part,
                         sequenceLaneValue(part, f, !clipReplacing));
  seedSequenceRecipeRecording();
}
std::size_t serviceSequenceBeforeBlock(std::size_t maximum) {
  // Only scalar context checks here. Draft construction/copies stay in the
  // explicit control callbacks; no generation or full-lane comparison per block.
  if(sequenceGenerateActive&&!sequenceGenerateContextCurrent(false))cancelSequenceGenerate();
  if (sequenceMode() && !audioRecording())
    selectedLane = std::size_t(splitFocus);
  for (int p = 0; p < 4; ++p)
    sequenceMuted[p] = settingsForPart(p).muted != 0;
  if (!sequenceRunning())
    return maximum;
  refreshSequencePlayback();
  if(!sequenceClockPrimed){for(int p=0;p<4;++p)sequenceDirections[p].reset(sequencePlayback.tracks[p]);sequenceClockPrimed=true;}
  syncSequenceRatePhases();
  const double speed =
      activeClipSlot()
          ? clampValue(double(activeClipSlot()->clipRate), .25, 4.0)
          : 1.0;
  const double increment = (midiSyncSource && midiClockStopped)
                               ? 0
                               : tempoBpm / (60.0 * sampleRate) * speed;
  if (increment <= 0)
    return maximum;
  for(int p=0;p<4;++p)sequenceDirections[p].synchronize(sequencePlayback.tracks[p],
      sequenceLaneTick(p,sequencePlayback.tracks[p].rate),sequenceEmittedTick[p]);
  s3g_thru_sequence::schedule(sequencePlayback, sequenceBeat, increment,
                              int(maximum), sequenceEvents,
                              sequenceBeatOffsets,&sequenceDirections);
  return serviceSequenceEvents(maximum);
}
void refreshSequencePlayback(){
  sequencePlayback.mix = sequenceLive.mix;
  for (int p = 0; p < 4; ++p) {
    const auto &from = sequenceLive.tracks[p];
    auto &to = sequencePlayback.tracks[p];
    to.length = from.length;
    to.rate = sequenceRequestedRate[p].load(std::memory_order_acquire);
    to.rotation = from.rotation;
    to.direction = from.direction;to.directionSeed=from.directionSeed;to.path=from.path;
    to.enabled = from.enabled;
    to.velocity = from.velocity;
    to.gate = from.gate;
    to.scaleEntry = from.scaleEntry;
    to.timeMode = from.timeMode;
    to.timeMs = from.timeMs;
    sequenceMuted[p] = settingsForPart(p).muted != 0;
  }
  if (clipPlaying)
    if (const auto *clip = activeClipSlot())
      for (int p = 0; p < 4; ++p)
        for (int f = 0; f < s3g_thru_sequence::laneFields; ++f) {
          float value = 0;
          const std::size_t curve = kClipSequenceOffset
              + s3g_thru_sequence::laneCurveOffset(p,f);
          // A mute gesture recorded after the start must not wrap around and
          // replace the clip's starting mute before that gesture is reached.
          if (f == 4 && clip->clipPointCounts[curve] > 0u &&
              clipPositionMs < clip->clipPoints[clip->clipPointStarts[curve]].timeMs)
            continue;
          if(sequenceOverrides[p][f])captureSequenceOverdubHold(ClipPointType::sequenceLane,p,f,sequenceLaneValue(p,f));
          if (sampleClipCurve(*clip, curve,
                              clipPositionMs, false, value) &&
              !sequenceOverrides[p][f]) {
            if (f == 4)
              sequenceMuted[p] = value >= .5f;
            else
              setSequenceLaneValue(sequencePlayback, p, f, value);
          }
        }
  serviceSequenceRecipePlayback();
}
std::size_t serviceSequenceEvents(std::size_t maximum){
  std::size_t count = maximum;
  if(clipPlaying&&(!clipRecording||!clipReplacing)){
    const auto* clip=activeClipSlot();
    double playbackSpeed=clip?clampValue(double(clip->clipRate),.25,4.0):1.0;
    if(clip&&clip->timingMode==ClipTimingMode::beatSync)playbackSpeed*=tempoBpm/std::max(1.f,clip->recordedTempoBpm);
    const double perFrame=playbackSpeed*1000.0/sampleRate;
    if(perFrame>0)for(int p=0;p<4;++p)if(sequenceRecipeNext[p]>clipPositionMs&&std::isfinite(sequenceRecipeNext[p]))
      count=std::min(count,std::size_t(std::max(1.0,std::ceil((sequenceRecipeNext[p]-clipPositionMs)/perFrame-1e-7))));
  }
  for (int i = 0; i < sequenceEvents.size; ++i) {
    const auto &event = sequenceEvents.items[i];
    if (event.offset > 0) {
      count = std::min(count,std::size_t(event.offset));
      break;
    }
    auto &last = sequenceLastEvent[event.lane][event.step][event.child];
    if (event.tick <= last)
      continue;
    last = event.tick;
    sequenceDirections[event.lane].remember(sequencePlayback.tracks[event.lane],event.tick,event.step);
    sequenceEmittedTick[event.lane]=std::max(sequenceEmittedTick[event.lane],event.tick);
    if (!sequenceMuted[event.lane]) {
      startSequenceVoice(event.lane, event.step, event.child, event.gateBeats, false, event.tick);
      ++sequenceEventCount[event.lane];
    }
  }
  if (sequenceEvents.overflow)
    ++sequenceOverflows;
  return std::max<std::size_t>(1, count);
}
void advanceSequenceClock(std::size_t count) {
  if (!sequenceRunning())
    return;
  const double speed =
      activeClipSlot()
          ? clampValue(double(activeClipSlot()->clipRate), .25, 4.0)
          : 1.0;
  if (!midiSyncSource || !midiClockStopped)
    sequenceBeat += double(count) * tempoBpm / (60.0 * sampleRate) * speed;
  for (int part = 0; part < 4; ++part) {
    const auto &l = sequencePlayback.tracks[part];
    sequenceCursor[part] = sequenceLaneCursor(part, l);
    sequencePublishedCursor[part].store(sequenceCursor[part],
                                        std::memory_order_release);
  }
}
bool sequenceKeyKnown(const char *key) const {
  if(sequenceTypeKeyKnown(key)||!std::strcmp(key,"mix_mute"))return true;
  static constexpr const char *keys[]{
      "seq_hold", "seq_consume", "seq_step", "seq_child", "seq_length",
      "seq_rate", "seq_rotation", "seq_active", "seq_mute", "seq_velocity",
      "seq_gate", "seq_scale", "seq_duration_mode", "seq_time_ms", "seq_direction", "seq_path",
      "seq_copy", "seq_paste", "seq_pos", "seq_slice",
      "seq_note", "seq_vel", "seq_trig_gate", "seq_trig_time_ms", "seq_micro", "seq_prob",
      "seq_burst", "seq_when", "seq_cycle_hit", "seq_cycle_every",
      "seq_toggle", "seq_clear", "seq_delete",
      "seq_clip_beats", "seq_pan_1", "seq_pan_2", "seq_pan_3", "seq_pan_4",
      "seq_eq_low", "seq_eq_mid", "seq_eq_high", "seq_eq_freq", "seq_eq_solo",
      "seq_strip_edit",
      "seq_duck_key", "seq_duck_depth", "seq_duck_threshold",
      "seq_duck_attack", "seq_duck_hold", "seq_duck_release",
      "seq_duck_hpf", "seq_duck_knee"};
  for (const char *candidate : keys)
    if (!std::strcmp(key, candidate))
      return true;
  return false;
}
bool setSequenceParameter(const char *key, const char *text) {
  if(setSequenceTypeParameter(key,text)||setOutputMixerParameter(key,text))return true;
  struct AuthorScope {const char*& target;const char* previous;AuthorScope(const char*& t,const char* k):target(t),previous(t){target=k;}~AuthorScope(){target=previous;}} author(sequenceAuthorKey,key);
  if (!sequenceKeyKnown(key))
    return false;
  const bool mixEdit=!std::strncmp(key,"seq_pan_",8)||!std::strncmp(key,"seq_eq_",7)
      ||!std::strncmp(key,"seq_duck_",9)||!std::strcmp(key,"seq_strip_edit")||!std::strcmp(key,"seq_mute");
  if(!mixEdit&&sequenceGeneratePart==splitFocus)cancelSequenceGenerate();
  float value = parseNumber(text);
  if (!std::isfinite(value))
    return true;
  if (!std::strcmp(key, "seq_hold")) {
    const int row = clampValue(int(value), -1, 31);
    sequenceEditStep = row;
    sequenceSoundEditPart = row>=0?splitFocus:-1;
    sequenceSoundEditClip = row>=0?activeClip:-1;
    if (row >= 0) {
      sequenceStep = row;
      sequencePadConsumed[s3g_thru_sequence::stepPad(row)] = true;
      auditionSequenceStep();
    } else
      stopSequenceAudition();
    return true;
  }
  if (!std::strcmp(key, "seq_consume")) {
    sequencePadConsumed[s3g_thru_sequence::stepPad(
        clampValue(int(value), 0, 31))] = true;
    return true;
  }
  if (!std::strcmp(key, "seq_step")) {
    sequenceStep = clampValue(int(value) - 1, 0, 31);
    sequenceChild = 0;
    return true;
  }
  if (!std::strcmp(key, "seq_child")) {
    sequenceChild =
        clampValue(int(value), 0,
                   sequenceLive.tracks[splitFocus].trigs[sequenceStep].count);
    return true;
  }
  if (!sequenceMode())
    return true;
  const char *laneKeys[]{"seq_length", "seq_rate", "seq_rotation",
                         "seq_active", "seq_mute", "seq_velocity",
                         "seq_gate",   "seq_scale", "seq_duration_mode", "seq_time_ms", "seq_direction", "seq_path"};
  for (int f = 0; f < s3g_thru_sequence::laneFields; ++f)
    if (!std::strcmp(key, laneKeys[f])) {
      const bool replaced=replaceSequenceLaneAutomation(splitFocus,f);
      sequenceChanged(!replaced);
      if (f == 4) {
        const bool muted = value >= .5f;
        splitParts[splitFocus].settings.muted = muted;
        if (auto *clip = activeClipSlot()) {
          clip->splitSettings[splitFocus].muted = muted;
          if (!clipRecording) {
            // Manual edits change this clip's starting state. Keep later
            // recorded mute gestures at their original times.
            const std::size_t curve = kClipSequenceOffset
                + std::size_t(splitFocus) * 8u + 4u;
            const std::size_t start = clip->clipPointStarts[curve];
            const std::size_t count = clip->clipPointCounts[curve];
            for (std::size_t i = 0; i < count; ++i) {
              auto &point = clip->clipPoints[start + i];
              if (point.timeMs != 0u)
                break;
              point.value = muted ? 1.f : 0.f;
            }
          }
        }
      } else {
        // UNIT converts bounds from the currently heard context, not the
        // end-of-clip canonical setting.
        if(f==8&&sequenceRunning()){
          sequenceLive.tracks[splitFocus].generator=sequencePlayback.tracks[splitFocus].generator;
          sequenceLive.tracks[splitFocus].timeMode=sequencePlayback.tracks[splitFocus].timeMode;
        }
        setSequenceLaneValue(sequenceLive, splitFocus, f, value);
        if (f == 1)
          sequenceRequestedRate[splitFocus].store(
              sequenceLive.tracks[splitFocus].rate, std::memory_order_release);
      }
      if (clipRecording)
        captureClipPoint(ClipPointType::sequenceLane, f, splitFocus,
                         sequenceLaneValue(splitFocus, f));
      if (clipPlaying)
        sequenceOverrides[splitFocus][f] = true;
      if(f==8&&clipRecording)sequenceRecipeOverrides[splitFocus][17]=sequenceRecipeOverrides[splitFocus][18]=true;
      commitSequenceEdit(f == 1);
      return true;
    }
  auto &step = sequenceLive.tracks[splitFocus].trigs[sequenceStep];
  if (!std::strcmp(key, "seq_slice")) {
    if(liveControlsForPart(splitFocus).algorithm!=Engine::PlaybackAlgorithm::slices){setError("SLIC needs Slices engine");return true;}
    const auto info=sequenceSliceSelection(splitFocus,sequenceStep,sequenceChild,sequenceRunning()?sequencePlayback:sequenceLive);
    if(value==0){sequenceChanged();sequenceLive.soundLocks.erase(splitFocus,sequenceStep,sequenceChild,s3g_thru_sequence::sliceLockId);commitSequenceEdit();++midiRevision;clearError();auditionSequenceStep();return true;}
    if(value<1||value>info.count||value!=std::floor(value)){setError("Slice is not available");return true;}
    auto& pool=sequenceLive.soundLocks;
    if(!pool.find(splitFocus,sequenceStep,sequenceChild,s3g_thru_sequence::sliceLockId)&&(pool.count==s3g_thru_sequence::soundLockCapacity||pool.stepCount(splitFocus,sequenceStep)==s3g_thru_sequence::soundLocksPerStep)){setError("Trig lock capacity full");return true;}
    sequenceChanged();pool.set(splitFocus,sequenceStep,sequenceChild,s3g_thru_sequence::sliceLockId,value);
    commitSequenceEdit();++midiRevision;clearError();auditionSequenceStep();return true;
  }
  if (!std::strcmp(key, "seq_copy")) {
    sequenceStepClipboard = step;
    sequenceSoundClipboard=sequenceLive.soundLocks.copyStep(splitFocus,sequenceStep);
    sequenceClipboardReady = true;
    return true;
  }
  if (!std::strcmp(key, "seq_paste")) {
    if (sequenceClipboardReady) {
      if(sequenceLive.soundLocks.count-sequenceLive.soundLocks.stepCount(splitFocus,sequenceStep)+sequenceSoundClipboard.count>s3g_thru_sequence::soundLockCapacity){setError("Trig lock capacity full");return true;}
      sequenceChanged();
      sequenceLive.soundLocks.pasteStep(splitFocus,sequenceStep,sequenceSoundClipboard);
      step = sequenceStepClipboard;
      sequenceChild = 0;
      commitSequenceEdit();
    }
    return true;
  }
  sequenceChanged();
  auto &locks =
      sequenceChild ? step.events[sequenceChild - 1].locks : step.locks;
  if (!std::strcmp(key, "seq_trig_time_ms")) {
    locks.timeLocked = true;
    locks.timeMs = clampValue(value, 10.f, 30000.f);
    commitSequenceEdit();
    auditionSequenceStep();
    return true;
  }
  const char *fields[]{"seq_pos", "seq_note", "seq_vel", "seq_trig_gate"};
  for (int f = 0; f < 4; ++f)
    if (!std::strcmp(key, fields[f])) {
      const int root = settingsForPart(splitFocus).root;
      const float lo[]{0, float(std::max(-48, -root)), 1, .01f},
          hi[]{1, float(std::min(48, 127 - root)), 127, 32};
      value = clampValue(value, lo[f], hi[f]);
      if (f == 1 || f == 2)
        value = std::round(value);
      if (f == 1 && sequenceLive.tracks[splitFocus].scaleEntry) {
        const auto &scale =
            kScaleDefinitions[std::size_t(settingsForPart(splitFocus).scale)];
        const float previous = sequenceParameterValue("seq_note");
        int best = int(value), distance = 100;
        for (int n = -48; n <= 48; ++n)
          for (std::size_t d = 0; d < scale.count; ++d)
            if ((n + 120) % 12 == scale.intervals[d] &&
                (value >= previous ? n >= value : n <= value) &&
                std::abs(n - int(value)) < distance) {
              distance = std::abs(n - int(value));
              best = n;
            }
        value = float(best);
      }
      locks.mask |= std::uint8_t(1u << f);
      locks.values[f] = value;
      commitSequenceEdit();
      if (f < 2 || f == 3)
        auditionSequenceStep();
      return true;
    }
  if (!std::strcmp(key, "seq_micro"))
    step.micro = clampValue(value, -.5f, .5f);
  else if (!std::strcmp(key, "seq_prob"))
    step.probability = clampValue(value, 0.f, 1.f);
  else if (!std::strcmp(key, "seq_cycle_hit"))
    step.cycleHit = clampValue(int(std::lround(value)), 1, int(step.cycleEvery));
  else if (!std::strcmp(key, "seq_cycle_every")) {
    step.cycleEvery = clampValue(int(std::lround(value)), 1, 8);
    step.cycleHit = std::min(step.cycleHit, step.cycleEvery);
  }
  else if (!std::strcmp(key, "seq_burst")) {
    s3g_thru_sequence::setCount(step, clampValue(int(value), 1, 8));
    sequenceChild = std::min(sequenceChild, step.count);
  } else if (!std::strcmp(key, "seq_when") && sequenceChild)
    step.events[sequenceChild - 1].when = clampValue(value, 0.f, .999f);
  else if (!std::strcmp(key, "seq_toggle"))
    step.enabled = !(sequenceRunning()?sequencePlayback.tracks[splitFocus].trigs[sequenceStep].enabled:step.enabled);
  else if (!std::strcmp(key, "seq_clear")) {
    if (int(value) >= 0 && int(value) < 4)
      locks.mask &= std::uint8_t(~(1u << int(value)));
    else
      locks.mask = 0;
    if (int(value) == 3 || int(value) >= 4)
      locks.timeLocked = false;
    if(int(value)>=4){const int r=s3g_thru_sequence::SoundLocks::route(splitFocus,sequenceStep,sequenceChild);sequenceLive.soundLocks.eraseRange(sequenceLive.soundLocks.lower(r),sequenceLive.soundLocks.lower(r+1));}
  } else if (!std::strcmp(key, "seq_delete")) {
    step = {};
    sequenceLive.soundLocks.eraseStep(splitFocus,sequenceStep);
    sequenceChild = 0;
  } else if (!std::strcmp(key, "seq_clip_beats")) {
    auto *clip = activeClipSlot();
    if (clip) {
      clip->recordedBeats = clampValue(value, 1.f, 64.f);
      clip->recordedTempoBpm = tempoBpm;
      clip->clipDurationMs =
          std::uint32_t(std::lround(clip->recordedBeats * 60000 / tempoBpm));
      clip->timingMode = ClipTimingMode::beatSync;
    }
  }
  commitSequenceEdit();
  return true;
}
float sequenceParameterValue(const char *key, int part = -1) const {
  const int owner = part < 0 ? splitFocus : part;
  if(sequenceTypeKeyKnown(key))return sequenceTypeValue(key,owner);
  float mixerValue=0;if(outputMixerValue(key,owner,mixerValue))return mixerValue;
  const int row = owner == splitFocus ? sequenceStep : sequenceSteps[owner];
  const int selectedChild =
      owner == splitFocus ? sequenceChild : sequenceChildren[owner];
  const char *laneKeys[]{"seq_length", "seq_rate", "seq_rotation",
                         "seq_active", "seq_mute", "seq_velocity",
                         "seq_gate",   "seq_scale", "seq_duration_mode", "seq_time_ms", "seq_direction", "seq_path"};
  for (int f = 0; f < s3g_thru_sequence::laneFields; ++f)
    if (!std::strcmp(key, laneKeys[f]))
      return sequenceLaneValue(owner, f, true);
  static constexpr const char *panKeys[]{"seq_pan_1", "seq_pan_2",
                                         "seq_pan_3", "seq_pan_4"};
  for (int p = 0; p < 4; ++p)
    if (!std::strcmp(key, panKeys[p]))
      return sequenceLive.mix.strips[p].pan;
  const auto &strip = sequenceLive.mix.strips[owner];
  const auto &mix = sequenceLive.mix;
  const char *stripKeys[]{"seq_eq_low", "seq_eq_mid", "seq_eq_high",
                          "seq_eq_freq", "seq_eq_solo"};
  const float stripValues[]{strip.lowDb, strip.midDb, strip.highDb,
                            strip.midHz, strip.solo ? 1.f : 0.f};
  for (int f = 0; f < 5; ++f)
    if (!std::strcmp(key, stripKeys[f]))
      return stripValues[f];
  const char *duckKeys[]{"seq_duck_key", "seq_duck_depth",
                         "seq_duck_threshold", "seq_duck_attack",
                         "seq_duck_hold", "seq_duck_release",
                         "seq_duck_hpf", "seq_duck_knee"};
  const float duckValues[]{float(mix.priority), mix.depthDb, mix.thresholdDb,
                           mix.attackMs, mix.holdMs, mix.releaseMs,
                           mix.hpfHz, mix.kneeDb};
  for (int f = 0; f < 8; ++f)
    if (!std::strcmp(key, duckKeys[f]))
      return duckValues[f];
  const auto &lane = (sequenceRunning()?sequencePlayback:sequenceLive).tracks[owner];
  const auto &step = lane.trigs[row];
  if (!std::strcmp(key, "seq_slice"))
    return float(sequenceSliceSelection(owner,row,selectedChild,sequenceRunning()?sequencePlayback:sequenceLive).number);
  if (!std::strcmp(key, "seq_trig_time_ms"))
    return s3g_thru_sequence::resolveTime(lane, step, selectedChild - 1);
  const char *fields[]{"seq_pos", "seq_note", "seq_vel", "seq_trig_gate"};
  for (int f = 0; f < 4; ++f)
    if (!std::strcmp(key, fields[f]))
      return s3g_thru_sequence::resolve(
          lane, step, selectedChild - 1, f,
          f == 0 ? liveControlsForPart(owner).locus : 0);
  if (!std::strcmp(key, "seq_step"))
    return float(row + 1);
  if (!std::strcmp(key, "seq_child"))
    return float(selectedChild);
  if (!std::strcmp(key, "seq_micro"))
    return step.micro;
  if (!std::strcmp(key, "seq_prob"))
    return step.probability;
  if (!std::strcmp(key, "seq_cycle_hit"))
    return float(step.cycleHit);
  if (!std::strcmp(key, "seq_cycle_every"))
    return float(step.cycleEvery);
  if (!std::strcmp(key, "seq_burst"))
    return float(step.count);
  if (!std::strcmp(key, "seq_when"))
    return selectedChild ? step.events[selectedChild - 1].when : 0;
  if (!std::strcmp(key, "seq_clip_beats")) {
    const auto *clip = activeClipSlot();
    return clip ? clip->recordedBeats : 4;
  }
  return 0;
}
int sequenceCursorState(char *b, int capacity) const {
  if (!b || capacity <= 0) return -1;
  const bool running = sequenceRunning();
  int at = std::snprintf(b, capacity, "%d,%d,%d",
                         sequenceMode() ? 1 : 0, splitFocus, running ? 1 : 0);
  if (at < 0 || at >= capacity) return -1;
  for (int p = 0; p < 4; ++p) {
    const auto &lane = running ? sequencePlayback.tracks[p] : sequenceLive.tracks[p];
    const int cursor = running
        ? sequencePublishedCursor[p].load(std::memory_order_acquire)
        : sequenceCursor[p];
    const int n = std::snprintf(
        b + at, capacity - at, ",%d,%d,%d,%d,%d", cursor, lane.length,
        lane.rate, lane.enabled ? 1 : 0,
        (running ? sequenceMuted[p] : settingsForPart(p).muted != 0) ? 1 : 0);
    if (n < 0 || n >= capacity - at) return -1;
    at += n;
  }
  return at;
}
int sequenceRecipeSourceKind(int part,const s3g_thru_sequence::Lane& lane) const {
  const int requested=int(lane.generator.values[2]);
  if(!requested||!engine.laneLoaded(std::size_t(part)))return -1;
  auto program=liveControlsForPart(part);
  program.sampleRegionMode=static_cast<Engine::RegionMode>(requested==1?4:requested==2?1:requested==3?2:requested==4?3:0);
  program.sampleRegionCount=lane.generator.values[8];
  int kind=0;(void)sliceMapForLane(std::size_t(part),program,kind);
  return kind;
}
int sequenceState(char *b, int capacity) const {
  const bool running = sequenceRunning();
  const auto &editLane = sequenceLive.tracks[splitFocus];
  const auto &displayLane = running ? sequencePlayback.tracks[splitFocus] : editLane;
  const auto &step = displayLane.trigs[sequenceStep];
  const auto *clip = activeClipSlot();
  const double clipRate = clip ? clampValue(double(clip->clipRate), .25, 4.0) : 1.0;
  const double stepMs = s3g_thru_sequence::rates[displayLane.rate] * 60000.0 /
                        (std::max(1.0, double(tempoBpm)) * clipRate);
  int at = std::snprintf(
      b, capacity,
      "{\"active\":%d,\"clip\":%d,\"edit\":%d,\"rec\":%u,\"step\":%d,\"held\":%d,\"child\":"
      "%d,\"root\":%d,\"mask\":%u,\"length\":%d,\"playing\":%d,\"clipboard\":%d,\"step_ms\":%.4f,\"lanes\":[",
      sequenceMode() ? 1 : 0, activeClip, splitFocus, static_cast<unsigned>(selectedLane),
      sequenceStep, sequenceEditStep, sequenceChild,
      settingsForPart(splitFocus).root,
      ((sequenceChild ? step.events[sequenceChild - 1].locks.mask
                      : step.locks.mask) & ~8u) |
          (displayLane.timeMode
               ? ((sequenceChild ? step.events[sequenceChild - 1].locks.timeLocked
                                 : step.locks.timeLocked) ? 8u : 0u)
               : ((sequenceChild ? step.events[sequenceChild - 1].locks.mask
                                 : step.locks.mask) & 8u)),
      displayLane.length, running ? 1 : 0,
      sequenceClipboardReady ? 1 : 0, stepMs);
  auto append = [&](const char *fmt, auto... args) {
    if (at < 0 || at >= capacity)
      return;
    const int size = std::snprintf(b + at, capacity - at, fmt, args...);
    if (size < 0 || size >= capacity - at)
      at = capacity;
    else
      at += size;
  };
  for (int p = 0; p < 4; ++p) {
    const auto &l = running ? sequencePlayback.tracks[p] : sequenceLive.tracks[p];
    const int cursor = running
        ? sequencePublishedCursor[p].load(std::memory_order_acquire)
        : sequenceCursor[p];
    append("%s[%d,%d,%d,%d,%d]", p ? "," : "", cursor, l.length,
           l.rate, l.enabled ? 1 : 0,
           (running ? sequenceMuted[p] : settingsForPart(p).muted != 0) ? 1 : 0);
  }
  append("%s", "],\"strips\":[");
  for (int p = 0; p < 4; ++p) {
    const auto &strip = sequenceLive.mix.strips[p];
    append("%s[%.4f,%.4f,%.4f,%.4f,%d]", p ? "," : "",
           double(strip.lowDb), double(strip.midDb), double(strip.highDb),
           double(strip.midHz), strip.solo ? 1 : 0);
  }
  append("%s", "],\"heads\":[");
  bool firstHead = true;
  for (const auto &voice : voices) {
    if (!voice.active || !voice.sequence.active || voice.stealPending ||
        voice.part < 0 || voice.part >= 4)
      continue;
    const auto &position = voice.sequence;
    append("%s[%d,%.5f,%.5f,%.2f,%u]", firstHead ? "" : ",", voice.part,
           double(position.position),
           double(clampValue(voice.engine.normalizedPosition(), 0.f, 1.f)),
           double(position.frames) * 1000.0 / sampleRate, voice.age);
    firstHead = false;
  }
  append("%s", "],\"steps\":[");
  const bool slices=liveControlsForPart(splitFocus).algorithm==Engine::PlaybackAlgorithm::slices;
  const auto& pattern=running?sequencePlayback:sequenceLive;
  for (int i = 0; i < 32; ++i) {
    const auto &s = displayLane.trigs[i];
    float position=s3g_thru_sequence::resolve(displayLane,s,-1,0,liveControlsForPart(splitFocus).locus);
    if(slices&&pattern.soundLocks.find(splitFocus,i,0,s3g_thru_sequence::sliceLockId)){const auto info=sequenceSliceSelection(splitFocus,i,0,pattern);position=info.valid?info.low:position;}
    append("%s[%d,%.5f]",i?",":"",s.enabled?1:0,double(position));
  }
  append("%s", "],\"burst\":[");
  for (int i = 0; i < step.count; ++i)
    append("%s%.5f", i ? "," : "", double(step.events[i].when));
  append("%s", "],\"values\":[");
  const char *keys[]{
      "seq_length",   "seq_rate",  "seq_rotation", "seq_active",    "seq_mute",
      "seq_velocity", "seq_gate",  "seq_scale",    "seq_step",      "seq_child",
      "seq_pos",      "seq_note",  "seq_vel",      "seq_trig_gate", "seq_micro",
      "seq_prob",     "seq_burst", "seq_when",     "seq_clip_beats",
      "seq_duration_mode", "seq_time_ms", "seq_trig_time_ms",
      "seq_pan_1", "seq_pan_2", "seq_pan_3", "seq_pan_4",
      "seq_eq_low", "seq_eq_mid", "seq_eq_high", "seq_eq_freq", "seq_eq_solo",
      "seq_duck_key", "seq_duck_depth", "seq_duck_threshold",
      "seq_duck_attack", "seq_duck_hold", "seq_duck_release",
      "seq_duck_hpf", "seq_duck_knee", "seq_cycle_hit",
      "seq_cycle_every"};
  for (int i = 0; i < 41; ++i)
    append("%s%.7g", i ? "," : "", double(sequenceParameterValue(keys[i])));
  append(",%d,0,0",editLane.generator.type);
  for(int i=1;i<21;++i)append(",%.7g",double(sequenceTypeValue(sequenceRecipeKeys[i],splitFocus)));
  append(",%d,%.7g,%.7g",displayLane.direction,double(sequenceTypeValue("seq_recipe_shift",splitFocus)),double(sequenceParameterValue("seq_path")));
  append(",%.7g",double(sequenceParameterValue("seq_slice")));
  append("],\"type_status\":\"%s\",\"types\":[",sequenceTypeStatus[splitFocus].data());
  for(int p=0;p<4;++p)append(p?",%d":"%d",sequenceLive.tracks[p].generator.type);
  append("],\"source_kind\":%d",sequenceRecipeSourceKind(splitFocus,displayLane));
  if(slices){
    const auto info=sequenceSliceSelection(splitFocus,sequenceStep,sequenceChild,pattern);
    append(",\"slice\":{\"number\":%d,\"count\":%d,\"valid\":%d,\"own\":%d,\"inherited\":%d,\"start\":%.7g,\"end\":%.7g,\"label\":\"",info.number,info.count,int(info.valid),int(info.own),int(info.inherited),double(info.low),double(info.high));
    for(unsigned char c:info.label){if(!c)break;if(c=='"'||c=='\\')append("\\%c",c);else if(c<32)append("\\u%04x",unsigned(c));else append("%c",c);}
    append("%s","\",\"points\":[");for(int i=0;i<=info.count&&info.count;++i)append(i?",%.7g":"%.7g",double(info.points[i]));append("%s","]}");
  }
  append("%s","}");
  return at >= capacity ? -1 : at;
}
bool writeSequenceExtension(
    std::FILE *f, const std::array<ClipSlot, kClipSlotCount> &clips) const {
  if (!writeU32(f, 0x53455131u))
    return false;
  for (const auto &clip : clips)
    for (int i = 0; i < s3g_thru_sequence::patternSize; ++i)
      if (!writeF32(f, s3g_thru_sequence::get(clip.sequence, i)))
        return false;
  return true;
}
bool readSequenceExtension(std::FILE *f,
                           std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(f, magic) || magic != 0x53455131u)
    return false;
  for (auto &clip : clips) {
    for (int i = 0; i < s3g_thru_sequence::patternSize; ++i) {
      float v = 0;
      if (!readF32(f, v) || !s3g_thru_sequence::set(clip.sequence, i, v))
        return false;
    }
    if (clip.sequence.active && (!clip.splitEnabled || !clip.quadMode ||
                                 clip.layers.active || clip.bankMode))
      return false;
  }
  return true;
}
// Format 39 appends time-mode values and the four-lane sequence strip/duck
// settings after the unchanged format-38 sequence payload.
bool writeSequenceMixExtension(
    std::FILE *f, const std::array<ClipSlot, kClipSlotCount> &clips) const {
  if (!writeU32(f, 0x32545153u))
    return false;
  for (const auto &clip : clips) {
    for (const auto &lane : clip.sequence.tracks) {
      if (!writeF32(f, lane.timeMode ? 1.f : 0.f) ||
          !writeF32(f, lane.timeMs))
        return false;
      for (const auto &step : lane.trigs) {
        if (!writeF32(f, step.locks.timeLocked ? step.locks.timeMs : 0.f))
          return false;
        for (const auto &child : step.events)
          if (!writeF32(f, child.locks.timeLocked ? child.locks.timeMs : 0.f))
            return false;
      }
    }
    for (const auto &strip : clip.sequence.mix.strips) {
      const float values[]{strip.pan, strip.lowDb, strip.midDb, strip.highDb,
                           strip.midHz, strip.solo ? 1.f : 0.f};
      for (float value : values)
        if (!writeF32(f, value))
          return false;
    }
    const auto &mix = clip.sequence.mix;
    const float values[]{float(mix.priority), mix.depthDb, mix.thresholdDb,
                         mix.attackMs, mix.holdMs, mix.releaseMs, mix.hpfHz,
                         mix.kneeDb};
    for (float value : values)
      if (!writeF32(f, value))
        return false;
  }
  return true;
}
bool readSequenceMixExtension(
    std::FILE *f, std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(f, magic) || magic != 0x32545153u)
    return false;
  auto readRange = [&](float &value, float low, float high) {
    return readF32(f, value) && std::isfinite(value) && value >= low &&
           value <= high;
  };
  for (auto &clip : clips) {
    for (auto &lane : clip.sequence.tracks) {
      float mode = 0;
      if (!readRange(mode, 0.f, 1.f) || std::floor(mode) != mode ||
          !readRange(lane.timeMs, 10.f, 30000.f))
        return false;
      lane.timeMode = mode != 0;
      for (auto &step : lane.trigs) {
        float time = 0;
        if (!readRange(time, 0.f, 30000.f) || (time > 0 && time < 10))
          return false;
        step.locks.timeLocked = time > 0;
        if (time > 0)
          step.locks.timeMs = time;
        for (auto &child : step.events) {
          if (!readRange(time, 0.f, 30000.f) || (time > 0 && time < 10))
            return false;
          child.locks.timeLocked = time > 0;
          if (time > 0)
            child.locks.timeMs = time;
        }
      }
    }
    for (auto &strip : clip.sequence.mix.strips) {
      float solo = 0;
      if (!readRange(strip.pan, -1.f, 1.f) ||
          !readRange(strip.lowDb, -12.f, 12.f) ||
          !readRange(strip.midDb, -12.f, 12.f) ||
          !readRange(strip.highDb, -12.f, 12.f) ||
          !readRange(strip.midHz, 120.f, 8000.f) ||
          !readRange(solo, 0.f, 1.f) || std::floor(solo) != solo)
        return false;
      strip.solo = solo != 0;
    }
    auto &mix = clip.sequence.mix;
    float priority = 0;
    if (!readRange(priority, 0.f, 4.f) || std::floor(priority) != priority ||
        !readRange(mix.depthDb, 0.f, 24.f) ||
        !readRange(mix.thresholdDb, -60.f, 0.f) ||
        !readRange(mix.attackMs, 1.f, 250.f) ||
        !readRange(mix.holdMs, 0.f, 1000.f) ||
        !readRange(mix.releaseMs, 10.f, 3000.f) ||
        !readRange(mix.hpfHz, 20.f, 2000.f) ||
        !readRange(mix.kneeDb, 0.f, 24.f))
      return false;
    mix.priority = int(priority);
  }
  return true;
}

// Format 40 appends per-trig lane-cycle conditions. Older clips default to 1:1.
bool writeSequenceCycleExtension(
    std::FILE *f, const std::array<ClipSlot, kClipSlotCount> &clips) const {
  if (!writeU32(f, 0x31594353u)) // SCY1
    return false;
  for (const auto &clip : clips)
    for (const auto &lane : clip.sequence.tracks) {
      std::array<std::uint8_t, s3g_thru_sequence::steps * 2> encoded{};
      for (int i = 0; i < s3g_thru_sequence::steps; ++i) {
        encoded[2 * i] = lane.trigs[i].cycleHit;
        encoded[2 * i + 1] = lane.trigs[i].cycleEvery;
      }
      if (!writeBytes(f, encoded.data(), encoded.size()))
        return false;
    }
  return true;
}
bool readSequenceCycleExtension(
    std::FILE *f, std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(f, magic) || magic != 0x31594353u)
    return false;
  for (auto &clip : clips)
    for (auto &lane : clip.sequence.tracks) {
      std::array<std::uint8_t, s3g_thru_sequence::steps * 2> encoded{};
      if (std::fread(encoded.data(), 1, encoded.size(), f) != encoded.size())
        return false;
      for (int i = 0; i < s3g_thru_sequence::steps; ++i) {
        const auto hit = encoded[2 * i], every = encoded[2 * i + 1];
        if (every < 1 || every > 8 || hit < 1 || hit > every)
          return false;
        lane.trigs[i].cycleHit = hit;
        lane.trigs[i].cycleEvery = every;
      }
    }
  return true;
}
void copySequenceRuntimeTo(ThruInstance &other) const {
  other.sequenceStripProcessor=sequenceStripProcessor;
  other.splitDuckGainState=splitDuckGainState;other.splitDuckHoldRemaining=splitDuckHoldRemaining;
  other.splitDuckHpfInput=splitDuckHpfInput;other.splitDuckHpfOutput=splitDuckHpfOutput;
  other.splitDuckEnvelope=splitDuckEnvelope;other.splitDuckKey=splitDuckKey;
  other.sequenceLive = sequenceLive;
  other.sequencePlayback = sequencePlayback;
  other.sequenceBeat = sequenceBeat;
  other.sequenceClockPrimed=sequenceClockPrimed;
  other.sequenceBeatOffsets = sequenceBeatOffsets;
  other.sequenceLastEvent = sequenceLastEvent;
  other.sequenceWindowStarts=sequenceWindowStarts;
  other.sequenceLastRate = sequenceLastRate;
  other.sequenceDirections=sequenceDirections;other.sequenceEmittedTick=sequenceEmittedTick;
  for (int part = 0; part < 4; ++part)
    other.sequenceRequestedRate[part].store(
        sequenceRequestedRate[part].load(std::memory_order_acquire),
        std::memory_order_release);
  other.sequenceMuted = sequenceMuted;
  other.sequenceOverrides = sequenceOverrides;
  other.sequenceRecipeOverrides=sequenceRecipeOverrides;
  other.invalidateSequenceRecipePlayback();
  other.sequenceCursor = sequenceCursor;
  for (int part = 0; part < 4; ++part)
    other.sequencePublishedCursor[part].store(
        sequencePublishedCursor[part].load(std::memory_order_acquire),
        std::memory_order_release);
  other.sequenceEventCount = sequenceEventCount;
  other.sequenceSteps = sequenceSteps;
  other.sequenceChildren = sequenceChildren;
  other.sequenceStep = sequenceStep;
  other.sequenceChild = sequenceChild;
  other.sequenceSoundEditPart=sequenceSoundEditPart;other.sequenceSoundEditClip=sequenceSoundEditClip;other.sequenceSoundClipboard=sequenceSoundClipboard;
  other.sequenceLaneSoundLocks=sequenceLaneSoundLocks;other.sequenceSoundTicks=sequenceSoundTicks;
}

void clearSequenceTrigs(s3g_thru_sequence::Pattern &pattern) {
  pattern.soundLocks.count=0;
  for (auto &lane : pattern.tracks) {
    for (auto &step : lane.trigs)step = {};
    lane.generator={};
  }
}
void initializeSequenceClip(ClipSlot &clip) {
  clearSequenceTrigs(clip.sequence);
  if (!clip.sequence.active)
    return;
  clip.clipRecorded = true;
  clip.takeCount = 1;
  clip.recordedTempoBpm = tempoBpm;
  clip.recordedBeats = 4;
  clip.clipDurationMs = std::uint32_t(std::lround(240000.f / tempoBpm));
  clip.timingMode = ClipTimingMode::beatSync;
}
