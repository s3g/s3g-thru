void testClipClipboardSnapshotsAndUndo() {
    auto t = splitFixture();
    t->setPerformanceMode(3);
    t->beginClipRecording();
    t->padNoteOn(68, 100);
    renderSplit(*t, 30);
    t->padNoteOff(68);
    renderSplit(*t, 30);
    t->finishClipRecording();
    t->clipRecordMode = ClipRecordMode::overdub;
    t->beginClipRecording();
    t->padNoteOn(72, 100);
    renderSplit(*t, 30);
    t->padNoteOff(72);
    renderSplit(*t, 30);
    t->finishClipRecording();
    t->captureActiveClip(true);
    auto &source = t->clipSlots[0];
    assert(source.takeCount == 2 && source.clipNoteCount == 2);
    source.clipPoints[0].value = .37f;
    source.clipPointCount = 1;
    t->rebuildClipPointIndex(source);
    source.laneSourceKinds[2] = LaneSourceKind::file;
    std::snprintf(source.laneSourcePaths[2].data(), source.laneSourcePaths[2].size(),
                  "library/source-three.wav");
    setParam(t.get(), "clip_clipboard_copy", "0");
    assert(t->clipClipboardSource == 0 && t->lastError[0] == '\0');
    const auto originalCount = t->clipClipboard.clipNoteCount;
    t->controls.speed = 2;
    source.clipPoints[0].value = .91f;
    source.laneSourcePaths[2][0] = 'X';
    t->clipSlots[35].occupied = true;
    t->clipSlots[35].baseValues[static_cast<int>(LockParameter::speed)] = 3;
    const auto *audio = t->engine.laneData(0, 0);
    const double position = t->clipPositionMs;
    t->preparedClip.store(35);
    t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
    setParam(t.get(), "clip_clipboard_paste", "35");
    assert(t->preparedClip.load() == -1 &&
           t->clipPrepareState.load() == static_cast<int>(ClipPrepareState::idle));
    const auto &target = t->clipSlots[35];
    assert(t->lastError[0] == '\0' && t->activeClip == 0 && t->controls.speed == 2 &&
           t->clipPositionMs == position && t->engine.laneData(0, 0) == audio);
    assert(target.occupied && target.quadMode && target.takeCount == 2 &&
           target.clipNoteCount == originalCount);
    assert(target.clipPoints[0].value == .37f && target.laneSourcePaths[2][0] == 'l' &&
           target.laneSourceKinds[2] == LaneSourceKind::file);
    t->restoreUndo();
    assert(t->clipSlots[35].occupied &&
           t->clipSlots[35].baseValues[static_cast<int>(LockParameter::speed)] == 3);
    setParam(t.get(), "clip_clipboard_copy", "63");
    assert(t->clipClipboardSource == 0 && t->lastError[0]);
    t->clipRecording = true;
    setParam(t.get(), "clip_clipboard_paste", "35");
    assert(t->lastError[0]);
    t->clipRecording = false;
    t->patternActive = true;
    setParam(t.get(), "clip_clipboard_paste", "35");
    assert(t->lastError[0]);
    t->patternActive = false;
    t->clipPlaying = true;
    setParam(t.get(), "clip_clipboard_paste", "0");
    assert(t->lastError[0] && t->controls.speed == 2);
    t->clipPlaying = false;
    t->clipLaunchPending = false;
    setParam(t.get(), "clip_clipboard_paste", "0");
    assert(t->lastError[0] == '\0' && t->clipSlots[0].clipPoints[0].value == .37f);
    assert(t->clipSlots[0].takeCount == 2 && t->activeClip == 0 && !t->clipPlaying);
    t->restoreUndo();
    assert(t->clipSlots[0].clipPoints[0].value == .91f && t->controls.speed == 2);
    // A fresh clipboard reports its empty state; a rejected paste creates no undo.
    auto fresh = splitFixture();
    setParam(fresh.get(), "clip_clipboard_paste", "2");
    assert(fresh->lastError[0] && !fresh->clipSlots[2].occupied);
}
void testPerformanceLoopGlobalsAndPartPressure() {
    auto t = splitFixture();
    setParam(t.get(), "clip_record_mode", "1");
    setParam(t.get(), "clip_rate", "1.5");
    setParam(t.get(), "tempo_bpm", "105");
    setParam(t.get(), "pressure_mode", "0");
    t->selectSplitPart(1);
    assert(t->clipRecordMode == ClipRecordMode::overdub && t->tempoBpm == 105);
    assert(t->pressureMode == 1);
    setParam(t.get(), "clip_record_mode", "0");
    setParam(t.get(), "clip_rate", ".75");
    t->selectSplitPart(0);
    assert(t->clipRecordMode == ClipRecordMode::replace && t->pressureMode == 0);
    char value[32]{};
    assert(getParamValue(t.get(), "clip_rate", value, sizeof(value)) > 0 &&
           std::abs(std::strtof(value, nullptr) - .75f) < .0001f);
}

void testChainRecordedClipDuplication() {
    auto t = splitFixture();
    t->stopSampleWorker();
    t->setPerformanceMode(3);
    t->captureActiveClip(true);
    auto &source = t->clipSlots[0];
    source.clipRecorded = true;
    source.clipDurationMs = 1000;
    source.takeCount = 2;
    source.clipNoteCount = 1;
    source.clipNotes[0].note = 60;
    source.laneSourceKinds[0] = LaneSourceKind::capture;
    std::snprintf(source.laneSourcePaths[0].data(), source.laneSourcePaths[0].size(),
                  "captures/source.wav");
    t->setChainEntry(0, 0, 3);
    t->chainEditSlot = 0;
    setParam(t.get(), "entry_fade_mode", "2");
    setParam(t.get(), "entry_fade_ms", "6000");
    // Reserve unrecorded slots referenced by another arrangement as well.
    t->chainBanks[4].entries[0].enabled = true;
    t->chainBanks[4].entries[0].clip = 1;
    setParam(t.get(), "chain_clip_copy", "1");
    assert(t->clipClipboardSource == 0 && t->lastError[0] == '\0');
    t->chainEditSlot = 7;
    t->setChainEntry(7, 5, 1);
    setParam(t.get(), "chain_clip_paste", "1");
    const int clone = t->clipChain()[7].clip;
    assert(clone == 2 && t->clipChain()[7].repeats == 3 && t->clipChain()[7].fade.time == 6000);
    assert(t->clipSlots[clone].occupied && t->clipSlots[clone].quadMode &&
           t->clipSlots[clone].takeCount == 2 && t->clipSlots[clone].clipNotes[0].note == 60);
    assert(t->clipChain()[0].clip == 0 && t->activeClip == 0 &&
           t->clipSlots[0].clipNotes[0].note == 60);
    t->clipSlots[clone].clipNotes[0].note = 72;
    assert(t->clipSlots[0].clipNotes[0].note == 60);
    t->clipSlots[clone].clipNotes[0].note = 60;
    t->restoreUndo();
    assert(t->clipChain()[7].clip == 5 && !t->clipSlots[clone].occupied);
    // Paste cannot overwrite any of the existing 64 clip slots.
    for (auto &clip : t->clipSlots)
        clip.occupied = true;
    const auto before = t->clipChain()[7];
    setParam(t.get(), "chain_clip_paste", "1");
    assert(t->lastError[0] && t->clipChain()[7].clip == before.clip);
    // Copying a chain stays an explicit, separately functioning menu action.
    setParam(t.get(), "chain_copy", "1");
    t->selectChainBank(2);
    setParam(t.get(), "chain_paste", "1");
    assert(t->clipChain()[0].clip == 0 && t->clipChain()[7].clip == 5);
}
