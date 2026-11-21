// A read-only view of the existing eight visible MIDI bindings. Saved values
// and controller feedback stay separate from the values consumed by playback.
bool midiPlaybackReadout(const ThruInstance &t, const MidiParameter &p,
                         int requestedPart, float &value) {
  const auto *clip = t.activeClipSlot();
  if (!t.clipPlaying || t.clipRecording || !clip || p.action)
    return false;
  const int part = p.part ? requestedPart : (t.splitEnabled ? t.splitFocus : 0);
  if (part < 0 || part >= (t.splitEnabled ? t.partCount() : 1))
    return false;
  const auto has = [&](std::size_t curve) {
    return curve < kClipCurveLaneCount && clip->clipPointCounts[curve] > 0;
  };
  const char *key = p.key;
  const int fmField=s3g_thru_fm::index(key);
  if(fmField>=0){const int lane=t.tableEditLane(part);if(t.fmOverrides.test((part*4+lane)*s3g_thru_fm::fields+fmField)||!has(kClipFmOffset+(part*4+lane)*s3g_thru_fm::fields+fmField))return false;
    value=(t.splitEnabled?t.splitClipControls[part]:t.clipControls).fm.patches[lane].v[fmField];return true;}

  const int scannedField=s3g_thru_scanned::index(key);
  if(scannedField>=0){const int lane=t.tableEditLane(part);if(t.scannedOverrides.test((part*4+lane)*s3g_thru_scanned::fields+scannedField)||!has(kClipScannedOffset+(part*4+lane)*s3g_thru_scanned::fields+scannedField))return false;
    value=(t.splitEnabled?t.splitClipControls[part]:t.clipControls).scanned.patches[lane].v[scannedField];return true;}

  const int tableField=s3g_thru_table::index(key);
  if(tableField>=0){const int lane=t.tableEditLane(part);if(t.tableOverrides.test((part*4+lane)*s3g_thru_table::fields+tableField)||!has(kClipTableOffset+(part*4+lane)*s3g_thru_table::fields+tableField))return false;
    value=(t.splitEnabled?t.splitClipControls[part]:t.clipControls).table.patches[lane].v[tableField];return true;}

  int field = 0;
  const int lfo = t.controlLfoKey(key, field);
  if (lfo >= 0) {
    if (field >= 11 || t.controlLfoPool[lfo].v[s3g_thru_lfo::flow] != 0 ||
        t.controlLfoOverrides[lfo][field] ||
        !has(kClipPooledLfoOffset + lfo * 11 + field))
      return false;
    value = t.controlLfoField(t.controlLfoConfiguration(lfo), field);
    return true;
  }
  if (!std::strncmp(key, "layers_", 7) && t.layersLive.active) {
    const int index = t.layerAlias(key);
    if (index >= 0) {
      if (t.layersOverrides[index] || !has(kClipLayersOffset + index))
        return false;
      value = t.layersRecorded.values[index];
      return true;
    }
    if (!std::strcmp(key, "layers_fx_x") || !std::strcmp(key, "layers_fx_y")) {
      bool recorded = false;
      for (int c = 0; c < 4; ++c) {
        const auto i = LayerPatch::config + LayerPatch::weight0 + c;
        recorded |= has(kClipLayersOffset + i) && !t.layersOverrides[i];
      }
      if (!recorded)
        return false;
      value = t.controlLfoLayerBase(key[10] == 'y');
      return true;
    }
  }
  if (t.sequenceMode()) {
    const char *keys[]{"seq_length",  "seq_rate",      "seq_rotation",
                       "seq_active",  "seq_mute",      "seq_velocity",
                       "seq_gate",    "seq_scale",     "seq_duration_mode",
                       "seq_time_ms", "seq_direction", "seq_path"};
    for (int f = 0; f < s3g_thru_sequence::laneFields; ++f)
      if (!std::strcmp(key, keys[f]) ||
          (f == 4 && !std::strcmp(key, "mix_mute"))) {
        if (t.sequenceOverrides[part][f] ||
            !has(kClipSequenceOffset +
                 s3g_thru_sequence::laneCurveOffset(part, f)))
          return false;
        value = t.sequenceLaneValue(part, f, true);
        return true;
      }
    for (int f = 1; f < 22; ++f)
      if (!std::strcmp(key, ThruInstance::sequenceRecipeKeys[f])) {
        if (t.sequenceRecipeOverrides[part][f] ||
            !has(sequenceRecipeCurve(part, f)))
          return false;
        value = t.sequenceTypeValue(key, part);
        return true;
      }
  }
  if (!std::strcmp(key, "stack_scan_paused")) {
    const auto curve =
        t.splitEnabled ? kClipPartHoldOffset + part : kClipStackHoldLane;
    if (!has(curve))
      return false;
    value = float((t.splitEnabled ? t.splitClipControls[part] : t.clipControls)
                      .laneScanPaused);
    return true;
  }
  int index = p.lock;
  if (!std::strcmp(key, "part_level"))
    index = int(t.partLevelParameter(part));
  if (!std::strcmp(key, "part_delay"))
    index = int(t.partDelayParameter(part));
  if (!std::strcmp(key, "part_reverb"))
    index = int(t.partReverbParameter(part));
  const int duck = t.legacyDualMixer() ? t.outputDuckField(key) : -1;
  if (duck >= 0) {
    const char *keys[]{"split_priority",       "split_priority",
                       "split_duck_threshold", "split_duck_attack",
                       "split_duck_hold",      "split_duck_release",
                       "split_duck_hpf",       "split_duck_knee"};
    index = lockParameterIndex(keys[duck]);
  }
  if (index < 0 || index >= int(kLockParameterCount))
    return false;
  const bool local = t.splitEnabled && ThruInstance::splitPartParameter(index);
  const auto curve =
      local ? kClipPartParameterOffset + part * kLockParameterCount + index
            : std::size_t(index);
  if (!has(curve))
    return false;
  if (ThruInstance::splitMixerParameter(index) && t.splitMixerOverrides[index])
    return false;
  if (ThruInstance::splitMixerParameter(index)) {
    const auto mixer = t.effectiveSplitMixer(t.clipControls);
    if (duck == 0)
      value = mixer.splitPriority < -.001f
                  ? 1
                  : mixer.splitPriority > .001f ? 2 : 0;
    else if (duck == 1)
      value = std::abs(mixer.splitPriority);
    else
      value = lockedValue(mixer, t.clipOutputGain, index);
  } else {
    const auto &controls = local ? t.splitClipControls[part] : t.clipControls;
    value = lockedValue(controls, t.clipOutputGain, index);
    if (!std::strcmp(key, "waveset_process"))
      value = float(Engine::wavesetProcessIndex(controls.wavesetProcess));
  }
  return std::isfinite(value);
}
