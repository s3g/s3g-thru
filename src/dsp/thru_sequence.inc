bool sequenceMode() const { return sequenceLive.active; }
bool sequenceRunning() const {
  return sequenceMode() && (clipPlaying || clipRecording);
}
void resetSequenceClock() {
  sequenceBeat = 0;
  sequenceEventCount = {};
  sequenceCursor.fill(-1);
  sequenceLastRate.fill(-1);
  sequencePlayback = sequenceLive;
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
void commitSequenceEdit() {
  sequencePlayback = sequenceLive;
  if (auto *clip = activeClipSlot())
    clip->sequence = sequenceLive;
  markPersistentStateDirty();
  ++sequenceRevision;
}
void startSequenceVoice(int part, int row, int child, double gateBeats,
                        bool audition = false) {
  if (part < 0 || part >= 4 || !engine.laneLoaded(part))
    return;
  const auto &lane = sequencePlayback.tracks[part];
  const auto &step = sequenceLive.tracks[part].trigs[row];
  const auto &program =
      audition ? liveControlsForPart(part) : splitClipControls[part];
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
  voice.sequence.audition = audition;
  voice.sequence.step = row;
  voice.sequence.child = child;
  voice.sequence.position =
      s3g_thru_sequence::resolve(lane, step, child, 0, program.locus);
  auto seekProgram = program;
  seekProgram.locus = voice.sequence.position;
  const auto window = parameterDisplayWindow(seekProgram, part);
  voice.sequence.startPhase =
      clampValue((voice.sequence.position - window[0]) /
                     std::max(.000001f, window[1] - window[0]),
                 0.f, .999999f);
  const double speed =
      activeClipSlot()
          ? clampValue(double(activeClipSlot()->clipRate), .25, 4.0)
          : 1.0;
  const double increment = tempoBpm / (60.0 * sampleRate) * speed;
  const double gateMs = lane.timeMode
      ? clampValue(double(s3g_thru_sequence::resolveTime(lane, step, child)), 10.0, 30000.0)
      : std::max(.25, gateBeats / increment * 1000 / sampleRate);
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
  voice.midiExternal = false;
  voice.midiGain = velocity / 127.f;
  voice.midiValue = voice.midiTarget = {};
  voice.pressureTarget = voice.pressureState = 0;
  voice.pressureBrakeHeld = false;
  voice.sliceCached = voice.sliceAudition = false;
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
    commitSequenceEdit();
    sequencePadConsumed[pad] = true;
  }
}
void sequencePadUp(int pad) {
  if (!(sequencePads & (1u << pad)))
    return;
  sequencePads &= ~(1u << pad);
  const auto held = renderClockSamples.load(std::memory_order_relaxed) -
                    sequencePadTimes[pad];
  const int step = s3g_thru_sequence::padStep(pad);
  if (!sequencePadConsumed[pad] && held < std::uint64_t(sampleRate * .3f)) {
    sequenceChanged();
    auto &target = sequenceLive.tracks[sequencePadParts[pad]].trigs[step];
    target.enabled = !target.enabled;
    commitSequenceEdit();
  }
  if (sequenceEditStep == step) {
    sequenceEditStep = -1;
    stopSequenceAudition();
  }
}
// A lane's authored settings and recorded lane gestures share one bounded
// descriptor path. Step/child edits remain clip content, never launch notes.
float sequenceLaneValue(int part, int field) const {
  const auto &l = sequenceLive.tracks[part];
  const float values[]{float(l.length),
                       float(l.rate),
                       float(l.rotation),
                       l.enabled ? 1.f : 0.f,
                       float(settingsForPart(part).muted),
                       l.velocity,
                       l.gate,
                       l.scaleEntry ? 1.f : 0.f,
                       l.timeMode ? 1.f : 0.f,
                       l.timeMs};
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
  default:
    break;
  }
}
void seedSequenceRecording() {
  if (!sequenceMode())
    return;
  if (clipReplacing)
    resetSequenceClock();
  for (int part = 0; part < 4; ++part)
    for (int f = 0; f < 10; ++f)
      captureClipPoint(ClipPointType::sequenceLane, f, part,
                       sequenceLaneValue(part, f));
}
std::size_t serviceSequenceBeforeBlock(std::size_t maximum) {
  if (sequenceMode() && !audioRecording())
    selectedLane = std::size_t(splitFocus);
  for (int p = 0; p < 4; ++p)
    sequenceMuted[p] = settingsForPart(p).muted != 0;
  if (!sequenceRunning())
    return maximum;
  sequencePlayback.mix = sequenceLive.mix;
  for (int p = 0; p < 4; ++p) {
    const auto &from = sequenceLive.tracks[p];
    auto &to = sequencePlayback.tracks[p];
    to.length = from.length;
    to.rate = from.rate;
    to.rotation = from.rotation;
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
        for (int f = 0; f < 10; ++f) {
          float value = 0;
          const std::size_t curve = kClipSequenceOffset
              + (f < 8 ? p * 8 + f : 32 + p * 2 + f - 8);
          // A mute gesture recorded after the start must not wrap around and
          // replace the clip's starting mute before that gesture is reached.
          if (f == 4 && clip->clipPointCounts[curve] > 0u &&
              clipPositionMs < clip->clipPoints[clip->clipPointStarts[curve]].timeMs)
            continue;
          if (sampleClipCurve(*clip, curve,
                              clipPositionMs, false, value) &&
              !sequenceOverrides[p][f]) {
            if (f == 4)
              sequenceMuted[p] = value >= .5f;
            else
              setSequenceLaneValue(sequencePlayback, p, f, value);
          }
        }
  // Deduplication ticks are numbered in the current rate's step units.
  // A slower live rate lowers that number; keeping the previous rate's ticks
  // would suppress new triggers until the old count was reached again.
  for (int p = 0; p < 4; ++p) {
    const int rate = sequencePlayback.tracks[p].rate;
    if (sequenceLastRate[p] == rate)
      continue;
    sequenceLastRate[p] = rate;
    for (auto &step : sequenceLastEvent[p])
      step.fill(-1);
  }
  const double speed =
      activeClipSlot()
          ? clampValue(double(activeClipSlot()->clipRate), .25, 4.0)
          : 1.0;
  const double increment = (midiSyncSource && midiClockStopped)
                               ? 0
                               : tempoBpm / (60.0 * sampleRate) * speed;
  if (increment <= 0)
    return maximum;
  s3g_thru_sequence::schedule(sequencePlayback, sequenceBeat, increment,
                              int(maximum), sequenceEvents);
  std::size_t count = maximum;
  for (int i = 0; i < sequenceEvents.size; ++i) {
    const auto &event = sequenceEvents.items[i];
    if (event.offset > 0) {
      count = std::size_t(event.offset);
      break;
    }
    auto &last = sequenceLastEvent[event.lane][event.step][event.child];
    if (event.tick <= last)
      continue;
    last = event.tick;
    if (!sequenceMuted[event.lane]) {
      startSequenceVoice(event.lane, event.step, event.child, event.gateBeats);
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
    sequenceCursor[part] =
        int((std::int64_t(
                 std::floor(sequenceBeat / s3g_thru_sequence::rates[l.rate])) +
             l.rotation) %
            l.length);
  }
}
bool sequenceKeyKnown(const char *key) const {
  static constexpr const char *keys[]{
      "seq_hold", "seq_consume", "seq_step", "seq_child", "seq_length",
      "seq_rate", "seq_rotation", "seq_active", "seq_mute", "seq_velocity",
      "seq_gate", "seq_scale", "seq_duration_mode", "seq_time_ms",
      "seq_copy", "seq_paste", "seq_pos",
      "seq_note", "seq_vel", "seq_trig_gate", "seq_trig_time_ms", "seq_micro", "seq_prob",
      "seq_burst", "seq_when", "seq_toggle", "seq_clear", "seq_delete",
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
  if (!sequenceKeyKnown(key))
    return false;
  const bool directStrip = !std::strcmp(key, "seq_strip_edit");
  int stripPart = splitFocus, stripField = -1;
  float value = parseFloat(text, 0);
  if (directStrip) {
    char trailing = 0;
    if (std::sscanf(text, "%d,%d,%f %c", &stripPart, &stripField,
                    &value, &trailing) != 3 || stripPart < 0 || stripPart >= 4 ||
        stripField < 0 || stripField >= 5)
      return true;
  }
  if (!std::isfinite(value))
    return true;
  if (!std::strcmp(key, "seq_hold")) {
    const int row = clampValue(int(value), -1, 31);
    sequenceEditStep = row;
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
                         "seq_gate",   "seq_scale", "seq_duration_mode", "seq_time_ms"};
  for (int f = 0; f < 10; ++f)
    if (!std::strcmp(key, laneKeys[f])) {
      sequenceChanged();
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
      } else
        setSequenceLaneValue(sequenceLive, splitFocus, f, value);
      if (clipRecording)
        captureClipPoint(ClipPointType::sequenceLane, f, splitFocus,
                         sequenceLaneValue(splitFocus, f));
      if (clipPlaying && !clipRecording)
        sequenceOverrides[splitFocus][f] = true;
      commitSequenceEdit();
      return true;
    }
  static constexpr const char *panKeys[]{"seq_pan_1", "seq_pan_2",
                                         "seq_pan_3", "seq_pan_4"};
  for (int p = 0; p < 4; ++p)
    if (!std::strcmp(key, panKeys[p])) {
      sequenceChanged();
      sequenceLive.mix.strips[p].pan = clampValue(value, -1.f, 1.f);
      commitSequenceEdit();
      return true;
    }
  auto &strip = sequenceLive.mix.strips[directStrip ? stripPart : splitFocus];
  auto &mix = sequenceLive.mix;
  const char *stripKeys[]{"seq_eq_low", "seq_eq_mid", "seq_eq_high",
                          "seq_eq_freq", "seq_eq_solo"};
  for (int f = 0; f < 5; ++f)
    if ((directStrip && f == stripField) ||
        (!directStrip && !std::strcmp(key, stripKeys[f]))) {
      sequenceChanged();
      switch (f) {
      case 0: strip.lowDb = clampValue(value, -12.f, 12.f); break;
      case 1: strip.midDb = clampValue(value, -12.f, 12.f); break;
      case 2: strip.highDb = clampValue(value, -12.f, 12.f); break;
      case 3: strip.midHz = clampValue(value, 120.f, 8000.f); break;
      default: strip.solo = value >= .5f; break;
      }
      commitSequenceEdit();
      return true;
    }
  const char *duckKeys[]{"seq_duck_key", "seq_duck_depth",
                         "seq_duck_threshold", "seq_duck_attack",
                         "seq_duck_hold", "seq_duck_release",
                         "seq_duck_hpf", "seq_duck_knee"};
  for (int f = 0; f < 8; ++f)
    if (!std::strcmp(key, duckKeys[f])) {
      sequenceChanged();
      switch (f) {
      case 0: mix.priority = clampValue(int(std::round(value)), 0, 4); break;
      case 1: mix.depthDb = clampValue(value, 0.f, 24.f); break;
      case 2: mix.thresholdDb = clampValue(value, -60.f, 0.f); break;
      case 3: mix.attackMs = clampValue(value, 1.f, 250.f); break;
      case 4: mix.holdMs = clampValue(value, 0.f, 1000.f); break;
      case 5: mix.releaseMs = clampValue(value, 10.f, 3000.f); break;
      case 6: mix.hpfHz = clampValue(value, 20.f, 2000.f); break;
      default: mix.kneeDb = clampValue(value, 0.f, 24.f); break;
      }
      commitSequenceEdit();
      return true;
    }
  auto &step = sequenceLive.tracks[splitFocus].trigs[sequenceStep];
  if (!std::strcmp(key, "seq_copy")) {
    sequenceStepClipboard = step;
    sequenceClipboardReady = true;
    return true;
  }
  if (!std::strcmp(key, "seq_paste")) {
    if (sequenceClipboardReady) {
      sequenceChanged();
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
  else if (!std::strcmp(key, "seq_burst")) {
    s3g_thru_sequence::setCount(step, clampValue(int(value), 1, 8));
    sequenceChild = std::min(sequenceChild, step.count);
  } else if (!std::strcmp(key, "seq_when") && sequenceChild)
    step.events[sequenceChild - 1].when = clampValue(value, 0.f, .999f);
  else if (!std::strcmp(key, "seq_toggle"))
    step.enabled = !step.enabled;
  else if (!std::strcmp(key, "seq_clear")) {
    if (int(value) >= 0 && int(value) < 4)
      locks.mask &= std::uint8_t(~(1u << int(value)));
    else
      locks.mask = 0;
    if (int(value) == 3 || int(value) >= 4)
      locks.timeLocked = false;
  } else if (!std::strcmp(key, "seq_delete")) {
    step = {};
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
  const int row = owner == splitFocus ? sequenceStep : sequenceSteps[owner];
  const int selectedChild =
      owner == splitFocus ? sequenceChild : sequenceChildren[owner];
  const char *laneKeys[]{"seq_length", "seq_rate", "seq_rotation",
                         "seq_active", "seq_mute", "seq_velocity",
                         "seq_gate",   "seq_scale", "seq_duration_mode", "seq_time_ms"};
  for (int f = 0; f < 10; ++f)
    if (!std::strcmp(key, laneKeys[f]))
      return sequenceLaneValue(owner, f);
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
  const auto &lane = sequenceLive.tracks[owner];
  const auto &step = lane.trigs[row];
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
int sequenceState(char *b, int capacity) const {
  const auto &lane = sequenceLive.tracks[splitFocus];
  const auto &step = lane.trigs[sequenceStep];
  const auto *clip = activeClipSlot();
  const double clipRate = clip ? clampValue(double(clip->clipRate), .25, 4.0) : 1.0;
  const double stepMs = s3g_thru_sequence::rates[lane.rate] * 60000.0 /
                        (std::max(1.0, double(tempoBpm)) * clipRate);
  int at = std::snprintf(
      b, capacity,
      "{\"active\":%d,\"edit\":%d,\"rec\":%u,\"step\":%d,\"held\":%d,\"child\":"
      "%d,\"root\":%d,\"mask\":%u,\"length\":%d,\"playing\":%d,\"clipboard\":%d,\"step_ms\":%.4f,\"lanes\":[",
      sequenceMode() ? 1 : 0, splitFocus, static_cast<unsigned>(selectedLane),
      sequenceStep, sequenceEditStep, sequenceChild,
      settingsForPart(splitFocus).root,
      (sequenceChild ? step.events[sequenceChild - 1].locks.mask
                    : step.locks.mask) |
          (lane.timeMode &&
                   (sequenceChild ? step.events[sequenceChild - 1].locks.timeLocked
                                  : step.locks.timeLocked)
               ? 8u : 0u),
      lane.length, sequenceRunning() ? 1 : 0,
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
    const auto &l = sequenceLive.tracks[p];
    append("%s[%d,%d,%d,%d,%d]", p ? "," : "", sequenceCursor[p], l.length,
           l.rate, l.enabled ? 1 : 0, settingsForPart(p).muted ? 1 : 0);
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
  for (int i = 0; i < 32; ++i) {
    const auto &s = lane.trigs[i];
    append("%s[%d,%.5f]", i ? "," : "", s.enabled ? 1 : 0,
           s3g_thru_sequence::resolve(lane, s, -1, 0,
                                      liveControlsForPart(splitFocus).locus));
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
      "seq_duck_hpf", "seq_duck_knee"};
  for (int i = 0; i < 39; ++i)
    append("%s%.7g", i ? "," : "", double(sequenceParameterValue(keys[i])));
  append("%s", "]}");
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

void copySequenceRuntimeTo(ThruInstance &other) const {
  other.sequenceLive = sequenceLive;
  other.sequencePlayback = sequencePlayback;
  other.sequenceBeat = sequenceBeat;
  other.sequenceLastEvent = sequenceLastEvent;
  other.sequenceLastRate = sequenceLastRate;
  other.sequenceMuted = sequenceMuted;
  other.sequenceOverrides = sequenceOverrides;
  other.sequenceCursor = sequenceCursor;
  other.sequenceEventCount = sequenceEventCount;
  other.sequenceSteps = sequenceSteps;
  other.sequenceChildren = sequenceChildren;
  other.sequenceStep = sequenceStep;
  other.sequenceChild = sequenceChild;
}

void clearSequenceTrigs(s3g_thru_sequence::Pattern &pattern) {
  for (auto &lane : pattern.tracks)
    for (auto &step : lane.trigs)
      step = {};
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
