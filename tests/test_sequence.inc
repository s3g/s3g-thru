std::unique_ptr<ThruInstance> sequenceFixture() {
  auto t = splitFixture();
  t->stopBackgroundWork();
  t->setPerformanceMode(6);
  assert(t->performanceMode() == 6 && t->isQuad() && t->voicesPerPart() == 2);
  t->launchQuantize = LaunchQuantize::off;
  return t;
}
void testSequenceScheduler() {
  using namespace s3g_thru_sequence;
  Pattern p;
  p.active = true;
  for (int lane = 0; lane < 4; ++lane) {
    auto &l = p.tracks[lane];
    l.length = 5 + lane * 2;
    l.rotation = lane + 1;
    l.rate = 4 + lane;
    for (int step = 0; step < 32; ++step) {
      auto &s = l.trigs[step];
      s.enabled = step % 3 != 1;
      s.micro = step % 2 ? .41f : -.37f;
      s.probability = .72f;
      setCount(s, 1 + step % 8);
    }
  }
  for (int rate : {44100, 48000}) {
    const auto timeline = [&](int block) {
      std::vector<std::array<std::int64_t, 5>> all;
      EventList events;
      const int total = rate * 3;
      for (int n = 0; n < total; n += block) {
        schedule(p, n * 2.0 / rate, 2.0 / rate, std::min(block, total - n),
                 events);
        assert(!events.overflow);
        for (int i = 0; i < events.size; ++i) {
          const auto &e = events.items[i];
          all.push_back({n + e.offset, e.lane, e.step, e.child, e.tick});
        }
      }
      return all;
    };
    assert(timeline(1) == timeline(31));
    assert(timeline(31) == timeline(128));
    assert(timeline(128) == timeline(511));
  }
  Pattern copy;
  for (int i = 0; i < patternSize; ++i)
    assert(set(copy, i, get(p, i)));
  for (int i = 0; i < patternSize; ++i)
    assert(get(p, i) == get(copy, i));
  assert(!set(copy, 0, NAN) && !set(copy, 1, 0));
  for (int i = 0; i < 32; ++i)
    assert(padStep(stepPad(i)) == i);
  assert(padStep(24) == 0 && padStep(31) == 7 && padStep(0) == 24 &&
         padStep(7) == 31);
  {
    Pattern cycles; cycles.active = true;
    auto &lane = cycles.tracks[0];
    lane.length = 3; lane.rate = 4; lane.rotation = 1;
    for (auto &other : cycles.tracks) other.enabled = false;
    lane.enabled = true;
    auto &trig = lane.trigs[1]; trig.enabled = true;
    trig.cycleHit = 1; trig.cycleEvery = 2; setCount(trig, 3);
    EventList events;
    schedule(cycles, 0, .25, 18, events);
    assert(events.size == 9);
    for (int i = 0; i < events.size; ++i)
      assert(events.items[i].tick == (i / 3) * 6);
    trig.cycleHit = 2;
    schedule(cycles, 0, .25, 18, events);
    assert(events.size == 9);
    for (int i = 0; i < events.size; ++i)
      assert(events.items[i].tick == 3 + (i / 3) * 6);
    trig.cycleHit = trig.cycleEvery = 1;
    schedule(cycles, 0, .25, 18, events);
    assert(events.size == 18);
  }
  std::puts("Sequence scheduler: sample-offset partition invariance, four "
            "polymeters, rotation, signed microtime, probability, bounded "
            "Burst and strict encoding: PASS");
}
void testSequenceAudioAndTransport() {
  auto t = sequenceFixture();
  for (int p = 0; p < 4; ++p) {
    auto &lane = t->sequenceLive.tracks[p];
    lane.length = 5 + 2 * p;
    lane.rate = 4;
    for (auto &s : lane.trigs)
      s.enabled = true;
  }
  t->commitSequenceEdit();
  t->startClipPlaybackNow();
  std::array<float, kBlockFrames * 2> audio{};
  double energy = 0;
  const int frames = 44100 * 5;
  for (int n = 0; n < frames; n += kBlockFrames) {
    const int count = std::min(int(kBlockFrames), frames - n);
    t->render(nullptr, count, audio.data());
    for (int i = 0; i < count * 2; ++i) {
      assert(std::isfinite(audio[i]));
      energy += audio[i] * audio[i];
    }
  }
  assert(energy > 1 && t->activeClipSlot()->clipNoteCount == 0 &&
         t->sequenceBeat > 9.99);
  for (int p = 0; p < 4; ++p) {
    assert(t->sequenceEventCount[p] >= 40 && t->sequenceEventCount[p] <= 41);
    int count = 0;
    for (const auto &voice : t->voices)
      if (voice.active && voice.part == p) {
        ++count;
        const auto effective = t->effectiveControls(voice);
        assert(effective.manualLane == p && effective.laneScanPaused &&
               effective.stackFirstLane == p && effective.stackLastLane == p);
      }
    assert(count <= 2);
  }
  const auto beat = t->sequenceBeat;
  t->captureActiveClip(true);
  t->activateStoredClip(0, false);
  assert(t->sequenceBeat == beat);
  t->clipSlots[1] = t->clipSlots[0];
  t->activateStoredClip(1, false);
  assert(t->sequenceBeat == 0);
  t->setClipPlaying(false);
  renderSplit(*t, 100);
  for (const auto &voice : t->voices)
    assert(!voice.held);
  t->startClipPlaybackNow();
  assert(t->sequenceBeat == 0);
  renderSplit(*t, 1);
  assert(t->sequenceEventCount[0] == 1);
  const auto phase = t->sequenceBeat;
  const auto serial = t->clipLaunchSerial;
  const auto clock = t->noteClock;
  t->preparedClip.store(t->activeClip);
  t->clipPrepareState.store(int(ClipPrepareState::ready));
  assert(t->activatePreparedClip() && t->sequenceBeat == phase &&
         t->clipLaunchSerial == serial && t->noteClock == clock);
  std::puts("Sequence audio: authored playback without launch notes, four "
            "fixed sample lanes/two voices, phase-preserving repeats and fresh "
            "clip/start resets: PASS");
}
void testSequenceLiveRateChange() {
  auto t = sequenceFixture();
  for (int p = 0; p < 4; ++p)
    t->sequenceLive.tracks[p].enabled = p == 0;
  auto &lane = t->sequenceLive.tracks[0];
  lane.length = 1;
  lane.rate = 8; // 1/64: accumulate more clock ticks than the slower rate.
  lane.trigs[0].enabled = true;
  t->commitSequenceEdit();
  t->startClipPlaybackNow();
  renderSplit(*t, 700);
  const auto prior = t->sequenceEventCount[0];
  const auto beat = t->sequenceBeat;
  assert(prior > 50 && beat > 4);
  setParam(t.get(), "seq_rate", "0"); // 1/1, while the clip keeps running.
  assert(t->sequenceBeat == beat && lane.rate == 0);
  std::array<std::int16_t, kBlockFrames * 2> audio{};
  std::uint64_t audible = 0;
  for (int block = 0; block < 1600; ++block) {
    t->render(audio.data(), kBlockFrames);
    if (block > 650)
      for (const auto sample : audio)
        audible += std::uint64_t(std::abs(int(sample)));
  }
  assert(t->engine.laneLoaded(0));
  assert(t->sequenceEventCount[0] >= prior + 2 && audible > 100000);
  std::puts("Sequence live RATE: fast-to-slow edits retain audible triggers "
            "and source samples: PASS");
}
void testSequenceRecordedRateChange() {
  auto t = sequenceFixture();
  for (int p = 0; p < 4; ++p)
    t->sequenceLive.tracks[p].enabled = p == 0;
  auto &lane = t->sequenceLive.tracks[0];
  lane.length = 1;
  lane.rate = 8;
  lane.trigs[0].enabled = true;
  t->commitSequenceEdit();
  t->beginClipRecording();
  assert(t->clipRecording);
  renderSplit(*t, 700);
  setParam(t.get(), "seq_rate", "0");
  renderSplit(*t, 1600);
  t->finishClipRecording();
  assert(!t->clipRecording && t->activeClipSlot()->clipPointCount > 0);
  t->setClipPlaying(false);
  t->startClipPlaybackNow();
  renderSplit(*t, 700);
  const auto fast = t->sequenceEventCount[0];
  assert(fast > 50);
  renderSplit(*t, 1600);
  assert(t->sequenceEventCount[0] >= fast + 2);
  std::puts("Sequence recorded RATE: performance-gesture replay changes speed "
            "without silencing the sample lane: PASS");
}
void testSequenceGatesAndEditing() {
  auto t = sequenceFixture();
  t->tempoBpm = 120;
  t->sequenceLive.tracks[0].rate = 4;
  auto *clip = t->activeClipSlot();
  assert(clip);
  char durationState[4096]{};
  assert(t->sequenceState(durationState, sizeof(durationState)) > 0);
  assert(std::strstr(durationState, "\"step_ms\":125.0000"));
  clip->clipRate = 2;
  assert(t->sequenceState(durationState, sizeof(durationState)) > 0);
  assert(std::strstr(durationState, "\"step_ms\":62.5000"));
  t->sequencePlayback = t->sequenceLive;
  t->startSequenceVoice(0, 0, 0, .2, true);
  assert(t->voices[0].sequence.frames >= 2204 &&
         t->voices[0].sequence.frames <= 2206);
  assert(t->sequenceState(durationState, sizeof(durationState)) > 0);
  assert(std::strstr(durationState, "\"heads\":[[0,"));
  assert(std::strstr(durationState, "50.00,"));
  const float beforeCursor = t->voices[0].engine.normalizedPosition();
  std::array<float, kBlockFrames * 2> cursorAudio{};
  t->render(nullptr, kBlockFrames, cursorAudio.data());
  assert(t->voices[0].engine.normalizedPosition() > beforeCursor);
  assert(t->sequenceState(durationState, sizeof(durationState)) > 0 &&
         std::strstr(durationState, "\"heads\":[[0,"));
  t->refreshPlaybackVoices();
  clip->clipRate = 1;
  auto &auditionStep = t->sequenceLive.tracks[0].trigs[0];
  auditionStep.locks.mask |= 8;
  auditionStep.locks.values[3] = 2;
  t->sequenceEditStep = 0;
  t->auditionSequenceStep();
  assert(t->voices[0].sequence.frames >= 11024 &&
         t->voices[0].sequence.frames <= 11026);
  t->refreshPlaybackVoices();
  setParam(t.get(), "seq_duration_mode", "1");
  assert(t->sequenceState(durationState, sizeof(durationState)) > 0 &&
         std::strstr(durationState, "\"mask\":0"));
  setParam(t.get(), "seq_time_ms", "5000");
  t->sequencePlayback = t->sequenceLive;
  t->startSequenceVoice(0, 0, 0, .2, true);
  assert(t->voices[0].sequence.frames == 220500);
  clip->clipRate = 4;
  t->refreshPlaybackVoices();
  t->startSequenceVoice(0, 0, 0, .2, true);
  assert(t->voices[0].sequence.frames == 220500);
  setParam(t.get(), "seq_trig_time_ms", "12000");
  t->refreshPlaybackVoices();
  t->sequencePlayback = t->sequenceLive;
  t->startSequenceVoice(0, 0, 0, .2, true);
  assert(t->voices[0].sequence.frames == 529200);
  setParam(t.get(), "seq_clear", "3");
  assert(!t->sequenceLive.tracks[0].trigs[0].locks.timeLocked);
  setParam(t.get(), "seq_duration_mode", "0");
  clip->clipRate = 1;
  t->refreshPlaybackVoices();
  t->sequenceEditStep = -1;
  auditionStep.locks.mask = 0;
  for (int p = 0; p < 4; ++p)
    t->sequenceLive.tracks[p].enabled = false;
  t->sequenceLive.tracks[0].enabled = true;
  t->sequencePlayback = t->sequenceLive;
  t->controls.ampAttackMs = 500;
  t->controls.ampDecayMs = 500;
  t->controls.ampReleaseMs = 3000;
  t->splitClipControls[0] = t->controls;
  t->startSequenceVoice(0, 0, 0, .2, true);
  auto *voice = &t->voices[0];
  assert(voice->active && voice->sequence.active);
  const auto &seq = voice->sequence;
  assert(std::abs(seq.attack + seq.decay + seq.release - 100) < .01f);
  assert(seq.frames >= 4409 && seq.frames <= 4411);
  renderSplit(*t, 36);
  assert(!voice->active);
  // Tap toggles only on release. A hold edits without disabling the trig.
  t->padNoteOn(92, 100);
  assert(!t->sequenceLive.tracks[0].trigs[0].enabled);
  t->padNoteOff(92);
  assert(t->sequenceLive.tracks[0].trigs[0].enabled);
  t->padNoteOn(92, 100);
  setParam(t.get(), "seq_hold", "0");
  renderSplit(*t, 2);
  t->padNoteOff(92);
  assert(t->sequenceLive.tracks[0].trigs[0].enabled);
  setParam(t.get(), "seq_pos", ".73");
  setParam(t.get(), "seq_note", "12");
  setParam(t.get(), "seq_vel", "61");
  auto &step = t->sequenceLive.tracks[0].trigs[0];
  assert(step.locks.mask == 7 && step.locks.values[1] == 12);
  setParam(t.get(), "seq_clear", "0");
  assert(!(step.locks.mask & 1));
  assert(t->sequenceParameterValue("seq_pos") == t->controls.locus);
  setParam(t.get(), "seq_burst", "8");
  for (int c = 0; c < 8; ++c)
    assert(step.events[c].when == c / 8.f);
  setParam(t.get(), "seq_copy", "1");
  setParam(t.get(), "seq_step", "32");
  setParam(t.get(), "seq_paste", "1");
  assert(t->sequenceLive.tracks[0].trigs[31].locks.values[1] == 12);
  setParam(t.get(), "seq_delete", "1");
  assert(!t->sequenceLive.tracks[0].trigs[31].enabled);
  setParam(t.get(), "seq_step", "1");
  setParam(t.get(), "seq_child", "0");
  t->padScale = PadScale::major;
  setParam(t.get(), "seq_scale", "1");
  setParam(t.get(), "seq_note", "1");
  assert(t->sequenceParameterValue("seq_note") == 0 ||
         t->sequenceParameterValue("seq_note") == 2);
  t->sequenceLive.tracks[0].length = 1;
  t->sequenceLive.tracks[0].rate = 8;
  step.enabled = true;
  step.micro = 0;
  step.probability = 1;
  t->commitSequenceEdit();
  t->startClipPlaybackNow();
  renderSplit(*t, 120);
  assert(t->sequenceEventCount[0] > 20);
  t->setClipPlaying(false);
  renderSplit(*t, 160);
  for (const auto &v : t->voices)
    assert(!v.held && !v.active);
  std::puts(
      "Sequence gates/editing: ADSR fits gate, safe dense Burst stealing, "
      "tap/hold, inherited locks, pitch entry and step copy/paste: PASS");
}
void testSequenceDisplayedLength() {
  auto t = sequenceFixture();
  t->startClipPlaybackNow();
  t->sequenceLive.tracks[0].length = 9;
  t->sequencePlayback.tracks[0].length = 24;
  t->sequenceBeat = 23 * s3g_thru_sequence::rates[4];
  t->sequenceCursor[0] = 23;
  char state[4096]{};
  assert(t->sequenceState(state, sizeof(state)) > 0);
  assert(std::strstr(state, "\"length\":24") &&
         std::strstr(state, "\"lanes\":[[23,24,"));
  t->sequencePlayback.tracks[0].length = 9;
  assert(t->sequenceState(state, sizeof(state)) > 0);
  assert(std::strstr(state, "\"length\":9") &&
         std::strstr(state, "\"lanes\":[[5,9,"));
  std::puts("Sequence overview: displayed cursor and length follow the same "
            "effective playback clock after recorded LEN gestures: PASS");
}
void testSequenceRecordingAndMidi() {
  auto t = sequenceFixture();
  setParam(t.get(), "seq_toggle", "1");
  const auto saved = t->sequenceLive.tracks[0].trigs[0].enabled;
  t->startRecording();
  assert(t->audioRecording() && t->selectedLane == 0);
  setParam(t.get(), "record_lane", "3");
  assert(t->splitFocus == 2 && t->selectedLane == 0);
  renderSplit(*t, 100);
  assert(t->selectedLane == 0);
  t->finishRecording();
  renderSplit(*t, 1);
  assert(t->selectedLane == 2);
  assert(t->sequenceLive.tracks[0].trigs[0].enabled == saved);
  // All part-scoped sequencer controls use the real MIDI descriptor adapter.
  t->selectSplitPart(0);
  const auto *length = s3g_thru_midi::find(3840);
  const auto *pitch = s3g_thru_midi::find(3851);
  const auto *every = s3g_thru_midi::find(3885);
  const auto *hit = s3g_thru_midi::find(3884);
  assert(length && pitch && every && hit);
  midiWrite(*t, *length, 1, 7);
  assert(t->splitFocus == 0 && t->sequenceLive.tracks[1].length == 7 &&
         midiRead(*t, *length, 1) == 7);
  midiWrite(*t, *pitch, 1, -12);
  assert(t->sequenceLive.tracks[1].trigs[0].locks.values[1] == -12 &&
         t->sequenceLive.tracks[0].trigs[0].locks.values[1] == 0);
  midiWrite(*t, *every, 1, 3);
  midiWrite(*t, *hit, 1, 2);
  assert(t->sequenceLive.tracks[1].trigs[0].cycleHit == 2 &&
         t->sequenceLive.tracks[1].trigs[0].cycleEvery == 3 &&
         midiRead(*t, *hit, 1) == 2);
  t->pendingCaptureSaveMask = 0;
  t->recordStartPending = false;
  t->beginClipRecording();
  assert(t->clipRecording);
  setParam(t.get(), "seq_active", "0");
  renderSplit(*t, 50);
  setParam(t.get(), "seq_active", "1");
  setParam(t.get(), "part_mute", "1");
  renderSplit(*t, 50);
  t->finishClipRecording();
  auto *clip = t->activeClipSlot();
  int gestures = 0;
  for (int i = 0; i < clip->clipPointCount; ++i)
    if (clip->clipPoints[i].type == ClipPointType::sequenceLane)
      ++gestures;
  assert(gestures >= 34 && clip->clipNoteCount == 0 && clip->sequence.active);
  // Use a clean native snapshot to exercise the exact JavaScript response.
  char state[4096]{};
  assert(t->sequenceState(state, sizeof(state)) > 0 &&
         std::strstr(state, "\"values\""));
  if (const char *path = std::getenv("S3G_THRU_SEQUENCE_FIXTURE")) {
    std::array<char, 16384> editor{};
    assert(formatEditorState(t.get(), editor.data(), editor.size()) > 0);
    std::ofstream out(path);
    out << "{\"editor\":" << editor.data() << ",\"sequence\":" << state << "}";
  }
  std::puts("Sequence recording/MIDI: fixed audio take destination, "
            "independent edit selection, retained trigs, part-scoped MIDI and "
            "clip lane gestures: PASS");
}
void testSequencePersistence() {
  auto t = sequenceFixture();
  setParam(t.get(), "seq_length", "29");
  setParam(t.get(), "seq_step", "30");
  setParam(t.get(), "seq_toggle", "1");
  setParam(t.get(), "seq_note", "-7");
  setParam(t.get(), "seq_burst", "4");
  setParam(t.get(), "seq_child", "3");
  setParam(t.get(), "seq_pos", ".61");
  setParam(t.get(), "seq_duration_mode", "1");
  setParam(t.get(), "seq_time_ms", "7000");
  setParam(t.get(), "seq_trig_time_ms", "13000");
  setParam(t.get(), "seq_cycle_every", "3");
  setParam(t.get(), "seq_cycle_hit", "2");
  setParam(t.get(), "seq_pan_1", "-.8");
  setParam(t.get(), "seq_eq_low", "3");
  setParam(t.get(), "seq_duck_key", "2");
  setParam(t.get(), "seq_duck_depth", "12");
  setParam(t.get(), "tempo_bpm", "1");
  setParam(t.get(), "seq_clip_beats", "64");
  assert(t->tempoBpm == 1 && t->activeClipSlot()->clipDurationMs == 3840000u);
  t->captureActiveClip(true);
  char folder[] = "/tmp/thru-sequence-XXXXXX";
  assert(mkdtemp(folder));
  t->persistenceEnabled = true;
  std::snprintf(t->moduleDirectory.data(), t->moduleDirectory.size(), "%s",
                folder);
  t->persistenceGeneration = 39;
  assert(t->writeClipFile());
  auto copy = sequenceFixture();
  assert(copy->readClipFileFromPath(
      (std::string(folder) + "/thru-clips.bin").c_str()));
  copy->activateStoredClip(0, false);
  assert(copy->tempoBpm == 1 && copy->activeClipSlot()->clipDurationMs == 3840000u
         && copy->activeClipSlot()->recordedTempoBpm == 1);
  assert(copy->sequenceMode() && copy->sequenceLive.tracks[0].length == 29 &&
         copy->sequenceLive.tracks[0].trigs[29].events[2].locks.values[0] ==
             .61f);
  assert(copy->sequenceLive.tracks[0].timeMode &&
         copy->sequenceLive.tracks[0].timeMs == 7000 &&
         copy->sequenceLive.tracks[0].trigs[29].events[2].locks.timeMs == 13000 &&
         copy->sequenceLive.mix.strips[0].pan == -.8f &&
         copy->sequenceLive.mix.strips[0].lowDb == 3 &&
         copy->sequenceLive.mix.priority == 2 &&
         copy->sequenceLive.mix.depthDb == 12 &&
         copy->sequenceLive.tracks[0].trigs[29].cycleHit == 2 &&
         copy->sequenceLive.tracks[0].trigs[29].cycleEvery == 3);
  std::vector<unsigned char> bytes;
  {
    std::ifstream in(std::string(folder) + "/thru-clips.bin", std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(in), {});
  }
  bytes.resize(bytes.size() - 4);
  auto prior=bytes;
  stripSequenceMixExtension(prior);
  assert(copy->sequenceLive.tracks[0].trigs[29].cycleEvery == 3);
  assert(prior[8] == 38);
  std::uint32_t priorSum = 2166136261u;
  for (auto byte : prior) { priorSum ^= byte; priorSum *= 16777619u; }
  for (int i = 0; i < 4; ++i)
    prior.push_back(std::uint8_t(priorSum >> (8 * i)));
  const auto priorPath=std::string(folder)+"/old38.bin";
  { std::ofstream out(priorPath, std::ios::binary);
    out.write(reinterpret_cast<const char *>(prior.data()), prior.size()); }
  assert(copy->readClipFileFromPath(priorPath.c_str()));
  copy->activateStoredClip(0, false);
  assert(copy->sequenceMode() && !copy->sequenceLive.tracks[0].timeMode &&
         copy->sequenceLive.mix.priority == 0 &&
         copy->sequenceLive.tracks[0].trigs[29].cycleEvery == 1);
  stripSequenceExtension(bytes);
  assert(bytes[8] == 37);
  std::uint32_t sum = 2166136261u;
  for (auto byte : bytes) {
    sum ^= byte;
    sum *= 16777619u;
  }
  for (int i = 0; i < 4; ++i)
    bytes.push_back(std::uint8_t(sum >> (8 * i)));
  const auto old = std::string(folder) + "/old37.bin";
  {
    std::ofstream out(old, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  }
  assert(copy->readClipFileFromPath(old.c_str()));
  copy->activateStoredClip(0, false);
  assert(!copy->sequenceMode() && copy->performanceMode() == 3);
  std::filesystem::remove_all(folder);
  t->persistenceEnabled = false;
  t->loadClip(2);
  assert(t->sequenceMode());
  for (const auto &lane : t->sequenceLive.tracks)
    for (const auto &step : lane.trigs)
      assert(!step.enabled);
  setParam(t.get(), "seq_toggle", "1");
  t->clearActiveClip();
  for (const auto &lane : t->sequenceLive.tracks)
    for (const auto &step : lane.trigs)
      assert(!step.enabled);
  std::puts("Sequence storage: format-40 cycle conditions, clip recall and "
            "format-38/37 migration: PASS");
}
void testSequenceEnginesAndNoteMidi() {
  auto t = sequenceFixture();
  std::array<float, kBlockFrames * 2> audio{};
  for (int algorithm = 0; algorithm < int(Engine::kPlaybackAlgorithmCount);
       ++algorithm) {
    t->refreshPlaybackVoices();
    t->controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(algorithm);
    t->storeFocusedPart();
    t->sequencePlayback = t->sequenceLive;
    t->startSequenceVoice(0, 0, 0, 1, true);
    double energy = 0;
    for (int block = 0; block < 80; ++block) {
      t->render(nullptr, kBlockFrames, audio.data());
      for (float sample : audio) {
        assert(std::isfinite(sample));
        energy += sample * sample;
      }
    }
    if (energy < 1e-7)
      std::fprintf(stderr, "Sequence silent engine=%d energy=%g\n", algorithm,
                   energy);
    assert(energy > 1e-7);
  }
  t->refreshPlaybackVoices();
  for (int channel = 1; channel <= 4; ++channel) {
    noteMidi(*t, channel, 60 + channel);
    assert(externalVoice(*t, channel - 1, 60 + channel));
    noteMidi(*t, channel, 60 + channel, 0);
    assert(!externalVoice(*t, channel - 1, 60 + channel));
  }
  std::puts(
      "Sequence engines: audible gated playback through all fifteen engines, "
      "fixed source ownership and four-channel note MIDI releases: PASS");
}
void testSequenceMixer() {
  using namespace s3g_thru_sequence;
  using Bus = std::array<std::array<std::array<float, kBlockFrames>, 2>, lanes>;
  StripProcessor strip;
  Mix mix;
  Bus bus{};
  auto fill = [&] {
    for (auto &lane : bus)
      for (auto &channel : lane)
        channel.fill(.25f);
  };
  fill();
  strip.process(kBlockFrames, bus, mix, 44100);
  assert(bus[0][0][0] == .25f && bus[3][1][kBlockFrames - 1] == .25f);
  mix.strips[0].lowDb = 6;
  for (int n = 0; n < 200; ++n) { fill(); strip.process(kBlockFrames, bus, mix, 44100); }
  assert(bus[0][0][kBlockFrames - 1] > .35f);
  assert(bus[1][0][kBlockFrames - 1] == .25f);
  mix.strips[0].pan = 1;
  mix.strips[0].solo = true;
  for (int n = 0; n < 200; ++n) { fill(); strip.process(kBlockFrames, bus, mix, 44100); }
  assert(std::abs(bus[0][0][kBlockFrames - 1]) < .001f);
  assert(bus[0][1][kBlockFrames - 1] > .35f);
  assert(bus[1][1][kBlockFrames - 1] < .001f);

  auto t = sequenceFixture();
  t->sequencePlayback.mix.priority = 1;
  t->sequencePlayback.mix.depthDb = 18;
  t->sequencePlayback.mix.thresholdDb = -60;
  t->sequencePlayback.mix.attackMs = 1;
  t->sequencePlayback.mix.holdMs = 0;
  std::array<Engine::Controls, 4> live{};
  for (int n = 0; n < 150; ++n) {
    for (int p = 0; p < 4; ++p)
      for (int ch = 0; ch < 2; ++ch)
        t->splitBus[p][ch].fill(p < 2 ? .5f : 0.f);
    t->processSplitBuses(kBlockFrames, live, t->controls);
  }
  const float protectedGain = t->splitFrameGain[0][kBlockFrames - 1];
  const float subordinateGain = t->splitFrameGain[1][kBlockFrames - 1];
  assert(protectedGain > 0 && subordinateGain < protectedGain * .5f);
  t->sequencePlayback.mix.priority = 0;
  for (int n = 0; n < 600; ++n) {
    for (int p = 0; p < 4; ++p)
      for (int ch = 0; ch < 2; ++ch)
        t->splitBus[p][ch].fill(p < 2 ? .5f : 0.f);
    t->processSplitBuses(kBlockFrames, live, t->controls);
  }
  assert(t->splitFrameGain[1][kBlockFrames - 1] > protectedGain * .99f);
  const int editPart = t->splitFocus;
  setParam(t.get(), "seq_strip_edit", "2,0,4.5");
  setParam(t.get(), "seq_strip_edit", "2,1,-2");
  setParam(t.get(), "seq_strip_edit", "2,3,1800");
  setParam(t.get(), "seq_strip_edit", "2,4,1");
  const auto &third = t->sequenceLive.mix.strips[2];
  assert(third.lowDb == 4.5f && third.midDb == -2.f &&
         third.midHz == 1800.f && third.solo);
  assert(t->sequenceLive.mix.strips[0].lowDb == 0 &&
         t->splitFocus == editPart &&
         t->activeClipSlot()->sequence.mix.strips[2].lowDb == 4.5f);
  setParam(t.get(), "seq_strip_edit", "4,0,12");
  setParam(t.get(), "seq_strip_edit", "2,0,NaN");
  assert(t->sequenceLive.mix.strips[2].lowDb == 4.5f);
  char sequenceView[4096]{};
  assert(t->sequenceState(sequenceView, sizeof(sequenceView)) > 0 &&
         std::strstr(sequenceView, "\"strips\":[") &&
         std::strstr(sequenceView, "[4.5000,-2.0000,"));
  std::puts("Sequence mixer: neutral channel path, stereo pan, EQ, solo and "
            "protected-lane priority duck, direct strip pages and clip ownership: PASS");
}
void testSequenceStepClipboard() {
  auto t=sequenceFixture();
  setParam(t.get(),"seq_step","1");
  setParam(t.get(),"seq_toggle","1");
  setParam(t.get(),"seq_pos",".42");
  setParam(t.get(),"seq_burst","4");
  setParam(t.get(),"seq_child","2");
  setParam(t.get(),"seq_note","7");
  const auto source=t->sequenceLive.tracks[0].trigs[0];
  assert(source.enabled && source.count==4);
  setParam(t.get(),"seq_cycle_every","4");
  setParam(t.get(),"seq_cycle_hit","3");
  t->padNoteOn(92,100);
  setParam(t.get(),"seq_consume","0");
  setParam(t.get(),"seq_copy","1");
  t->padNoteOff(92);
  assert(t->sequenceLive.tracks[0].trigs[0].enabled);
  char state[4096]{};
  assert(t->sequenceState(state,sizeof(state))>0 &&
         std::strstr(state,"\"clipboard\":1"));
  t->padNoteOn(93,100);
  setParam(t.get(),"seq_consume","1");
  setParam(t.get(),"seq_step","2");
  setParam(t.get(),"seq_paste","1");
  t->padNoteOff(93);
  const auto &pasted=t->sequenceLive.tracks[0].trigs[1];
  assert(pasted.enabled && pasted.count==4 &&
         pasted.locks.values[0]==.42f &&
         pasted.events[1].locks.values[1]==7 &&
         pasted.cycleHit==3 && pasted.cycleEvery==4);
  std::puts("Sequence step clipboard: consumed Copy/Shift pad gestures preserve "
            "source and paste trig, Burst and event locks: PASS");
}
void testSequenceMuteAndPosition() {
  auto t=sequenceFixture();
  setParam(t.get(),"part_mute","1");
  assert(t->settingsForPart(0).muted && t->activeClipSlot()->splitSettings[0].muted);
  t->startClipPlaybackNow();renderSplit(*t,2);
  assert(t->sequenceMuted[0]);
  t->setClipPlaying(false);renderSplit(*t,1);
  t->activateStoredClip(0,false);
  assert(t->settingsForPart(0).muted);
  setParam(t.get(),"part_mute","0");
  assert(!t->activeClipSlot()->splitSettings[0].muted);
  t->activateStoredClip(0,false);
  assert(!t->settingsForPart(0).muted);

  auto recorded=sequenceFixture();
  recorded->beginClipRecording();
  assert(recorded->clipRecording);
  renderSplit(*recorded,70);
  recorded->finishClipRecording();
  assert(recorded->activeClipSlot()->clipPointCount>0);
  recorded->setClipPlaying(false);
  recorded->startClipPlaybackNow();renderSplit(*recorded,2);
  setParam(recorded.get(),"part_mute","1");
  assert(recorded->settingsForPart(0).muted &&
         recorded->activeClipSlot()->splitSettings[0].muted);
  recorded->setClipPlaying(false);
  recorded->startClipPlaybackNow();renderSplit(*recorded,2);
  assert(recorded->sequenceMuted[0] &&
         recorded->activeClipSlot()->splitSettings[0].muted);
  const std::size_t muteCurve = kClipSequenceOffset + 4u;
  const auto *recordedClip = recorded->activeClipSlot();
  assert(recordedClip->clipPointCounts[muteCurve] > 0u);
  assert(recordedClip->clipPoints[recordedClip->clipPointStarts[muteCurve]].timeMs == 0u);
  assert(recordedClip->clipPoints[recordedClip->clipPointStarts[muteCurve]].value == 1.f);

  // A timed mute gesture still plays at its recorded time after a manual
  // change to the starting mute state.
  auto gestures=sequenceFixture();
  gestures->beginClipRecording();
  renderSplit(*gestures,70);
  setParam(gestures.get(),"part_mute","1");
  renderSplit(*gestures,100);
  gestures->finishClipRecording();
  const auto *gestureClip=gestures->activeClipSlot();
  const std::size_t gestureStart=gestureClip->clipPointStarts[muteCurve];
  assert(gestureClip->clipPointCounts[muteCurve]>=2u);
  assert(gestureClip->clipPoints[gestureStart].timeMs==0u);
  assert(gestureClip->clipPoints[gestureStart].value==0.f);
  assert(gestureClip->clipPoints[gestureStart+1].timeMs>0u);
  assert(gestureClip->clipPoints[gestureStart+1].value==1.f);
  setParam(gestures.get(),"part_mute","0");
  gestures->setClipPlaying(false);
  gestures->startClipPlaybackNow();renderSplit(*gestures,2);
  assert(!gestures->sequenceMuted[0]);
  renderSplit(*gestures,100);
  assert(gestures->sequenceMuted[0]);
  assert(gestureClip->clipPoints[gestureStart+1].value==1.f);

  setParam(t.get(),"seq_pos",".8");
  setParam(t.get(),"seq_burst","2");
  setParam(t.get(),"seq_child","1");
  setParam(t.get(),"seq_pos",".2");
  setParam(t.get(),"seq_child","0");
  char state[4096]{};
  assert(t->sequenceState(state,sizeof(state))>0 &&
         std::strstr(state,"[0,0.80000]"));
  t->controls.algorithm=Engine::PlaybackAlgorithm::sample;
  t->controls.field=1;
  t->storeFocusedPart();
  for (auto mode : {Engine::SampleMode::forward,Engine::SampleMode::reverse}) {
    t->refreshPlaybackVoices();
    t->controls.sampleMode=mode;t->storeFocusedPart();
    t->splitClipControls[0]=t->controls;
    t->sequenceLive.tracks[0].trigs[0].events[0].locks.mask=0;
    t->sequencePlayback=t->sequenceLive;
    t->startSequenceVoice(0,0,0,.2,true);
    renderSplit(*t,1);
    const float physical=t->voices[0].engine.normalizedPosition();
    assert(std::abs(physical-.8f)<.05f);
  }
  std::puts("Sequence mute/POS: lane mute survives restart and clip recall; "
            "overview marker follows parent trig and sample starts match "
            "source position in forward/reverse modes: PASS");
}
void testSequenceRepeatedPlaybackPosition() {
  auto t=sequenceFixture();
  for(int p=0;p<4;++p)t->sequenceLive.tracks[p].enabled=p==0;
  auto &lane=t->sequenceLive.tracks[0];
  lane.length=1;lane.rate=4;lane.timeMode=true;lane.timeMs=1000;
  lane.trigs[0].enabled=true;lane.trigs[0].locks.mask|=1;
  lane.trigs[0].locks.values[0]=.75f;
  t->controls.algorithm=Engine::PlaybackAlgorithm::sample;
  t->controls.sampleMode=Engine::SampleMode::loop;
  t->controls.field=1;
  t->storeFocusedPart();t->commitSequenceEdit();
  t->startClipPlaybackNow();
  auto checkNewest=[&] {
    int newest=-1;
    for(std::size_t i=0;i<t->voices.size();++i) {
      const auto &voice=t->voices[i];
      if(voice.active&&voice.part==0&&voice.sequence.active&&!voice.stealPending
          &&(newest<0||voice.age>t->voices[std::size_t(newest)].age))newest=int(i);
    }
    assert(newest>=0);
    const auto &voice=t->voices[std::size_t(newest)];
    assert(std::abs(voice.sequence.position-.75f)<.0001f);
    const float cursor=voice.engine.normalizedPosition();
    if(!(cursor>=.73f&&cursor<.9f))
      std::fprintf(stderr,"Sequence POS cursor: start=.75 current=%.3f events=%llu\n",
                   cursor,(unsigned long long)t->sequenceEventCount[0]);
    assert(cursor>=.73f&&cursor<.9f);
  };
  renderSplit(*t,2);checkNewest();
  for(int block=0;block<200&&t->sequenceEventCount[0]<3;++block)
    renderSplit(*t,1);
  assert(t->sequenceEventCount[0]>=3);
  renderSplit(*t,3);checkNewest();

  auto reused=sequenceFixture();
  for(int p=0;p<4;++p)reused->sequenceLive.tracks[p].enabled=p==0;
  auto &other=reused->sequenceLive.tracks[0];
  other.length=1;other.rate=4;other.timeMode=true;other.timeMs=1000;
  other.trigs[0].enabled=true;other.trigs[0].locks.mask|=1;
  other.trigs[0].locks.values[0]=.25f;
  reused->controls.algorithm=Engine::PlaybackAlgorithm::sample;
  reused->controls.sampleMode=Engine::SampleMode::loop;
  reused->controls.field=.2f;
  reused->storeFocusedPart();
  reused->splitClipControls[0]=reused->controls;
  reused->sequencePlayback=reused->sequenceLive;
  reused->startSequenceVoice(0,0,0,.2,false);
  reused->startSequenceVoice(0,0,0,.2,false);
  renderSplit(*reused,20); // Both readers now have a window near 25%.
  setParam(reused.get(),"seq_pos",".75");
  reused->startClipPlaybackNow();
  renderSplit(*reused,4);
  int newest=-1;
  for(std::size_t i=0;i<reused->voices.size();++i)
    if(reused->voices[i].active&&reused->voices[i].sequence.active
       &&!reused->voices[i].stealPending
       &&(newest<0||reused->voices[i].age>reused->voices[std::size_t(newest)].age))
      newest=int(i);
  assert(newest>=0);
  const float restartCursor=reused->voices[std::size_t(newest)].engine.normalizedPosition();
  if(!(restartCursor>=.72f&&restartCursor<.9f))
    std::fprintf(stderr,"Sequence restart POS cursor: expected=.75 actual=%.3f\n",restartCursor);
  assert(restartCursor>=.72f&&restartCursor<.9f);
  std::puts("Sequence repeated POS: actual Sample reader starts at the locked "
            "source position on first, stolen and restarted trigs: PASS");
}
void testSequenceTempoEdges() {
  auto t = sequenceFixture();
  setParam(t.get(), "tempo_bpm", "1");
  assert(t->tempoBpm == 1);
  setParam(t.get(), "tempo_bpm", "999");
  assert(t->tempoBpm == 999);
  setParam(t.get(), "tempo_bpm", "1000");
  assert(t->tempoBpm == 999);
  setParam(t.get(), "tempo_bpm", "0");
  assert(t->tempoBpm == 1);
  using namespace s3g_thru_sequence;
  Pattern p; p.active = true;
  for (auto &lane : p.tracks) {
    lane.length = 1; lane.rate = 8; lane.trigs[0].enabled = true;
    setCount(lane.trigs[0], 8);
  }
  EventList events;
  schedule(p, 0.0, 999.0 * 4.0 / (60.0 * 44100.0), 128, events);
  assert(events.size > 0 && !events.overflow);
  std::puts("Sequence tempo: 1–999 BPM clamp, slow clip span and fast bounded scheduling: PASS");
}
void testSequence() {
  testSequenceTempoEdges();
  testSequenceEnginesAndNoteMidi();
  testSequenceScheduler();
  testSequenceAudioAndTransport();
  testSequenceDisplayedLength();
  testSequenceLiveRateChange();
  testSequenceRecordedRateChange();
  testSequenceGatesAndEditing();
  testSequenceRecordingAndMidi();
  testSequencePersistence();
  testSequenceMixer();
  testSequenceStepClipboard();
  testSequenceMuteAndPosition();
  testSequenceRepeatedPlaybackPosition();
}
