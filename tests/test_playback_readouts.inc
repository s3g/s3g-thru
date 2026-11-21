void testPlaybackReadouts() {
  auto t = splitFixture();
  t->stopBackgroundWork();
  t->setPerformanceMode(0);
  t->controls.algorithm = Engine::PlaybackAlgorithm::grains;
  t->controls.grainSizeMs = 90;
  auto *clip = t->activeClipSlot();
  assert(clip);
  clip->clipDurationMs = 1000;
  clip->clipRecorded = true;
  const auto point = [&](ClipPointType type, int field, int owner, float value,
                         unsigned at) {
    auto &p = clip->clipPoints[clip->clipPointCount++];
    p.timeMs = at;
    p.value = value;
    p.parameter = std::uint16_t(field);
    p.sourcePad = std::uint8_t(owner);
    p.type = type;
    ThruInstance::rebuildClipPointIndex(*clip);
  };
  point(ClipPointType::parameter, int(LockParameter::grainSize), -1, 90, 0);
  point(ClipPointType::parameter, int(LockParameter::grainSize), -1, 210, 800);
  t->clipPlaying = true;
  t->clipPositionMs = 400;
  t->applyClipCurves(*clip, 400);
  float shown = 0;
  const auto &size = midiParameter("grain_size");
  assert(midiPlaybackReadout(*t, size, 0, shown) &&
         std::abs(shown - 150) < .001f);
  assert(midiRead(*t, size, 0) == 90 && t->controls.grainSizeMs == 90);
  const auto &wave = midiParameter("waveset_process");
  point(ClipPointType::parameter, int(LockParameter::wavesetProcess), -1, 6, 0);
  t->applyClipCurves(*clip, 400);
  assert(midiPlaybackReadout(*t, wave, 0, shown) &&
         shown == 5); // dense menu choice
  std::snprintf(t->moduleDirectory.data(), t->moduleDirectory.size(),
                "/tmp/no-readout-io");
  const auto revision = t->midiRevision;
  char view[128]{};
  std::snprintf(view, sizeof(view), "0:%d;0:%d", size.id, wave.id);
  setParam(t.get(), "midi_view", view);
  std::array<char, 4096> packet{};
  assert(formatMidiState(*t, packet.data(), packet.size()) > 0);
  assert(std::strstr(packet.data(), "\"playback\":[[0,") &&
         std::strstr(packet.data(), ",150]"));
  assert(t->midiRevision == revision && t->controls.grainSizeMs == 90);
  if (const char *fixture = std::getenv("S3G_THRU_PLAYBACK_FIXTURE")) {
    auto *f = std::fopen(fixture, "wb");
    assert(f);
    assert(std::fputs(packet.data(), f) >= 0);
    assert(std::fclose(f) == 0);
  }
  char small[8]{};
  assert(formatMidiState(*t, small, 8) == -1 && small[0] == 0);
  std::array<char, 16384> fast{};
  assert(getParamValue(t.get(), "ui_poll_fast", fast.data(), fast.size()) > 0);
  t->clipPlaying = false;
  assert(!midiPlaybackReadout(*t, size, 0, shown));
  t->clipPlaying = true;
  t->clipRecording = true;
  assert(!midiPlaybackReadout(*t, size, 0, shown));
  t->clipRecording = false;
  // The two part snapshots are independent, and shared mixer overrides agree
  // with the actual audio path. A query never changes focus or controls.
  t->setPerformanceMode(1);
  t->clipPlaying = false;
  t->selectSplitPart(0, false);
  t->controls.grainSizeMs = 80;
  t->selectSplitPart(1, false);
  t->controls.grainSizeMs = 120;
  point(ClipPointType::partParameter, int(LockParameter::grainSize), 0, 160, 0);
  point(ClipPointType::partParameter, int(LockParameter::grainSize), 1, 240, 0);
  point(ClipPointType::parameter, int(LockParameter::splitBlend), -1, .25f, 0);
  t->clipPlaying = true;
  t->applyClipCurves(*clip, 400);
  assert(midiPlaybackReadout(*t, size, 0, shown) && shown == 160);
  assert(midiPlaybackReadout(*t, size, 1, shown) && shown == 240 &&
         t->splitFocus == 1);
  assert(midiPlaybackReadout(*t, midiParameter("split_blend"), -1, shown) &&
         shown == .25f);
  t->splitMixerOverrides[static_cast<std::size_t>(LockParameter::splitBlend)] =
      true;
  assert(!midiPlaybackReadout(*t, midiParameter("split_blend"), -1, shown));
  // Playback of LFO recipe changes uses the resolved rate, not its authored
  // base; LIVE ownership deliberately ignores the recorded curve.
  point(ClipPointType::pooledLfo, 3, 7, .25f, 0);
  point(ClipPointType::pooledLfo, 3, 7, 4.f, 800);
  t->controlLfoPool[7].v[2] = 1;
  assert(midiPlaybackReadout(*t, midiParameter("lfo8_rate"), -1, shown) &&
         std::abs(shown - 2.125f) < .001f);
  assert(t->controlLfoPool[7].v[3] == 1);
  t->controlLfoPool[7].v[8] = 1;
  assert(!midiPlaybackReadout(*t, midiParameter("lfo8_rate"), -1, shown));
  t->controlLfoPool[7].v[8] = 0;
  // Layers aliases address the edited corner's recorded patch, not a copied
  // live base, and ownership returns as soon as that field is edited.
  t->clipPlaying = false;
  t->setPerformanceMode(5);
  t->layersSelectedCorner[1] = 2;
  const int index = t->layerAlias("layers_fx1");
  point(ClipPointType::layersParameter, index % 192, index / 192, .82f, 0);
  t->clipPlaying = true;
  t->applyClipCurves(*clip, 400);
  assert(midiPlaybackReadout(*t, midiParameter("layers_fx1"), -1, shown) &&
         std::abs(shown - .82f) < .001f);
  t->layersOverrides[index] = true;
  assert(!midiPlaybackReadout(*t, midiParameter("layers_fx1"), -1, shown));
  // Sequence lane and recipe readouts retain their existing effective timing.
  t->clipPlaying = false;
  t->setPerformanceMode(6);
  point(ClipPointType::sequenceLane, 5, 0, 72, 0);
  t->clipPlaying = true;
  t->refreshSequencePlayback();
  assert(midiPlaybackReadout(*t, midiParameter("seq_velocity"), 0, shown) &&
         shown == 72);
  t->sequenceOverrides[0][5] = true;
  assert(!midiPlaybackReadout(*t, midiParameter("seq_velocity"), 0, shown));
  // Actual clip capture of RATE changes alters the oscillator's advance on
  // replay, while the menu observes exactly that same resolved recipe.
  auto rateTake = splitFixture();
  rateTake->stopBackgroundWork();
  rateTake->setPerformanceMode(0);
  lfoAssign(*rateTake, 0, 0, "speed", .2f);
  setParam(rateTake.get(), "lfo1_rate", "1");
  rateTake->beginClipRecording();
  renderSplit(*rateTake, 100);
  setParam(rateTake.get(), "lfo1_rate", "4");
  renderSplit(*rateTake, 40);
  rateTake->finishClipRecording();
  const auto curve = kClipPooledLfoOffset + s3g_thru_lfo::rate;
  assert(rateTake->activeClipSlot()->clipPointCounts[curve] >= 2);
  rateTake->clipPlaying = true;
  rateTake->clipPositionMs = 100;
  const auto resolved = rateTake->controlLfoConfiguration(0);
  assert(resolved.v[s3g_thru_lfo::rate] > 1 &&
         resolved.v[s3g_thru_lfo::rate] < 4);
  assert(midiPlaybackReadout(*rateTake, midiParameter("lfo1_rate"), -1, shown));
  assert(std::abs(shown - resolved.v[s3g_thru_lfo::rate]) < .00001f);
  const auto phase = rateTake->controlLfoRuntime[0].position;
  rateTake->advanceControlLfos(128);
  assert(std::abs(rateTake->controlLfoRuntime[0].position - phase -
                  resolved.v[s3g_thru_lfo::rate] * 128 / rateTake->sampleRate) <
         .00001);
  std::puts(
      "Playback readouts: bounded eight-control snapshots, actual clip curves, "
      "saved-base isolation, dense enums, A/B/shared ownership, manual "
      "overrides, LFO RATE/Live, Layers corners and Sequence gestures: PASS");
}
void testExpandedLfoDestinations() {
  using namespace s3g_thru_lfo;
  auto t = splitFixture();
  t->stopBackgroundWork();
  t->setPerformanceMode(0);
  assert(destinations.size() == 209);
  for (int id = 69; id < int(destinations.size()); ++id) {
    const auto &d = destinations[id];
    if(d.condition==15)continue;
    int alg = 0;
    while (!(d.engines & (1u << alg)))
      ++alg;
    t->controls = {};
    t->controls.algorithm = static_cast<Engine::PlaybackAlgorithm>(alg);
    auto &c = t->controls;
    if(alg==15)for(auto& patch:c.table.patches){patch.v[s3g_thru_table::type]=d.condition==17?1:d.condition==18?2:0;patch.v[s3g_thru_table::art]=1;patch.v[s3g_thru_table::shape]=3;patch.v[s3g_thru_table::noise]=50;}

    if(alg==17)for(auto& patch:c.fm.patches){patch.v[s3g_thru_fm::mode]=1;patch.v[s3g_thru_fm::algorithm]=1;patch.v[s3g_thru_fm::noise]=30;patch.v[s3g_thru_fm::decay1]=patch.v[s3g_thru_fm::decay2]=0;}
    if(d.parameter>=336&&d.parameter<384)for(auto& patch:c.scanned.patches){patch.v[s3g_thru_scanned::art]=1;patch.v[s3g_thru_scanned::bend]=12;patch.v[s3g_thru_scanned::amp_mode]=1;}
    c.sampleMode = Engine::SampleMode::loop;
    c.motionSound = static_cast<Engine::MotionSound>(
        d.condition == 4 ? 2 : d.condition >= 5 && d.condition <= 7 ? 3 : 1);
    c.grainProcess = static_cast<Engine::GrainProcess>(1);
    c.cutClock = c.glitchClock = static_cast<Engine::CutClock>(0);
    c.wavesetProcess = Engine::WavesetProcess::multiply;
    c.prismMode = 4;
    c.stochasticLaw = d.condition == 14 ? 3 : 0;
    const float base = d.octaves > 0 ? std::sqrt(d.low * d.high)
                                     : d.low + (d.high - d.low) * .4f;
    float ignored = t->outputGain;
    applyLockedValue(c, ignored, d.parameter, base);
    const float saved = lockedValue(c, ignored, d.parameter);
    t->controlLfoPool = {};
    t->controlLfoRuntime = {};
    lfoAssign(*t, 0, 0, d.key, .35f);
    t->advanceControlLfos(44100);
    auto effective = c, expected = c;
    t->applyControlLfoControls(effective, 0, Domain::engine);
    applyLockedValue(expected, ignored, d.parameter,
                     ThruInstance::controlLfoMapped(id, saved, .35f));
    assert(std::abs(lockedValue(effective, ignored, d.parameter) -
                    lockedValue(expected, ignored, d.parameter)) < .002f);
    assert(lockedValue(c, ignored, d.parameter) == saved);
    assert(t->controlLfoDestinationAvailable(t->controlLfoPool[0], id));
  }
  const auto available = [&](const char *key) {
    return t->controlLfoDestinationAvailable(
        Config{}, t->controlLfoDestinationForKey(key));
  };
  t->controls.algorithm = Engine::PlaybackAlgorithm::prism;
  t->controls.prismMode = 0;
  assert(!available("prism_decay"));
  t->controls.prismMode = 4;
  assert(available("prism_decay"));
  t->controls.algorithm = Engine::PlaybackAlgorithm::stochastic;
  t->controls.stochasticLaw = 0;
  assert(available("stochastic_pull") && !available("stochastic_capture"));
  t->controls.stochasticLaw = 3;
  assert(!available("stochastic_pull") && available("stochastic_capture"));
  // Conditional eligibility follows replayed engine settings, including
  // an automated algorithm before any note has started sounding it.
  t->controls.algorithm = Engine::PlaybackAlgorithm::sample;
  t->clipControls = t->controls;
  t->clipControls.algorithm = Engine::PlaybackAlgorithm::prism;
  t->clipControls.prismMode = 4;
  t->controls.prismMode = 0;
  t->clipPlaying = true;
  assert(available("prism_decay"));
  t->clipControls.prismMode = 0;
  assert(!available("prism_decay"));
  t->clipControls.algorithm = Engine::PlaybackAlgorithm::stochastic;
  t->clipControls.stochasticLaw = 3;
  t->controls.stochasticLaw = 0;
  assert(available("stochastic_capture") && !available("stochastic_pull"));
  t->clipPlaying = false;
  t->setPerformanceMode(1);
  t->selectSplitPart(0, false);
  t->controlLfoPool[7] = {};
  t->controlLfoPool[7].target = 3;
  setParam(t.get(), "lfo_select", "8");
  assert(t->controlLfoPool[7].target == 0);
  t->controlLfoPool[7].target = 3;
  t->controlLfoPool[7].v[s3g_thru_lfo::rate] = .5f;
  setParam(t.get(), "lfo_select", "8");
  assert(t->controlLfoPool[7].target == 3);
  t->controlLfoPool[7] = {};
  t->controlLfoPool[7].target = 3;
  const auto recorded = kClipPooledLfoOffset + 7 * 11;
  t->activeClipSlot()->clipPointCounts[recorded] = 1;
  setParam(t.get(), "lfo_select", "8");
  assert(t->controlLfoPool[7].target == 3);
  t->activeClipSlot()->clipPointCounts[recorded] = 0;
  t->controlLfoPool[7].target = 3;
  t->controlLfoPool[7].v[0] = 1;
  setParam(t.get(), "lfo_select", "8");
  assert(t->controlLfoPool[7].target == 3);
  assert(!t->controlLfoDestinationAvailable(t->controlLfoPool[7], 1));
  t->setPerformanceMode(5);
  Config b;
  b.target = 1;
  assert(t->controlLfoPartCount() == 1 &&
         !t->controlLfoDestinationAvailable(b, 2));
  char state[4096]{};
  assert(t->formatControlLfo(state, sizeof(state)) > 0 &&
         std::strstr(state, "\"parts\":[0,4]"));
  std::puts("Expanded LFO destinations: 58 real engine controls, all "
            "mappings/base isolation, conditional targets, active-part lists, "
            "empty-slot defaults and retained inactive assignments: PASS");
}
