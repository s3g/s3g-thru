double formantCorrelation(const std::vector<float> &x, int lag) {
    double a = 0, b = 0, c = 0;
    for (std::size_t i = x.size() / 3 + lag; i < x.size(); ++i) {
        a += double(x[i]) * x[i - lag];
        b += double(x[i]) * x[i];
        c += double(x[i - lag]) * x[i - lag];
    }
    return a / std::max(1e-20, std::sqrt(b * c));
}
double formantLineEnergy(const std::vector<float> &x, float hz) {
    const double coefficient = 2 * std::cos(6.283185307179586 * hz / 44100);
    double previous = 0, prior = 0;
    for (std::size_t i = x.size() / 3; i < x.size(); ++i) {
        const double q = x[i] + coefficient * previous - prior;
        prior = previous;
        previous = q;
    }
    return previous * previous + prior * prior - coefficient * previous * prior;
}
void testFormantTuneAndTracking() {
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::formant;
    c.lanePath = Engine::LanePath::manual;
    c.field = 1;
    c.locus = .5f;
    c.formantFreeze = 1;
    c.formantFreezePosition = .4f;
    for (float hz : {261.625565f, 440.f}) {
        auto f = formantFixture();
        c.formantTune = 1;
        c.formantNoteHz = hz;
        c.notePitch = 1;
        const auto x = f->render(c, 24000);
        assert(formantCorrelation(x, static_cast<int>(std::lround(44100 / hz))) > .9);
        assert(formantCorrelation(x, 294) < .85);
    }
    auto f = formantFixture();
    c.formantTune = 0;
    c.formantNoteHz = 440;
    const auto original = f->render(c, 24000);
    assert(formantCorrelation(original, 294) > .95);
    c.formantTune = 1;
    c.formantPitch = 12;
    f = formantFixture();
    const auto octave = f->render(c, 24000);
    assert(formantCorrelation(octave, 50) > .9);
    // Keyboard tracking moves the formant region with the played transposition.
    c.formantTune = 0;
    c.formantPitch = 0;
    c.notePitch = 2;
    c.formantKeytrack = 1;
    f = formantFixture();
    const auto tracked = f->render(c, 24000);
    c.formantKeytrack = 0;
    f = formantFixture();
    const auto fixed = f->render(c, 24000);
    const auto ratio = [](const auto &x) {
        return formantBandEnergy(x, 1500, 2100) / std::max(1e-10, formantBandEnergy(x, 650, 1150));
    };
    assert(ratio(tracked) > ratio(fixed) * 3);
    auto native = splitFixture();
    native->setPerformanceMode(0);
    setParam(native.get(), "algorithm", "Formant");
    native->rootNote = 67;
    native->noteOn(69, 100);
    const PlaybackVoice *voice = nullptr;
    for (const auto &v : native->voices)
        if (v.active && v.note == 69)
            voice = &v;
    assert(voice);
    const auto effective =
        native->effectiveControls(*voice, native->controls, native->clipControls);
    assert(std::abs(effective.formantNoteHz - 440) < .001f);
}
void testFormantFreezeAcrossNotes() {
    auto t = splitFixture();
    t->setPerformanceMode(2);
    setParam(t.get(), "algorithm", "Formant");
    t->controls.laneCycles = 2;
    t->padNoteOn(68, 100);
    renderSplit(*t, 20);
    setParam(t.get(), "formant_freeze", "On");
    const float anchor = t->controls.formantFreezePosition;
    assert(anchor > .01f && anchor < .99f);
    renderSplit(*t, 10);
    float beforeLane = 0;
    for (const auto &v : t->voices)
        if (v.active && v.part == 0)
            beforeLane = v.engine.normalizedLanePosition();
    t->padNoteOn(69, 100);
    renderSplit(*t, 30);
    bool moved = false;
    int active = 0;
    for (const auto &v : t->voices)
        if (v.active && v.part == 0) {
            ++active;
            assert(std::abs(v.engine.normalizedPosition() - anchor) < .001f);
            moved |= std::abs(v.engine.normalizedLanePosition() - beforeLane) > .001f;
        }
    assert(active == 2 && moved);
    setParam(t.get(), "formant_freeze", "On");
    assert(t->controls.formantFreezePosition == anchor); // repeat On retains the captured point
    t->storeCue(0, t->controls.locus);
    const float cue = t->activeClipSlot()->cues[0];
    t->triggerCue(0);
    renderSplit(*t, 10);
    assert(t->controls.formantFreeze == 1 && t->controls.formantFreezePosition == cue);
    for (const auto &v : t->voices)
        if (v.active && v.part == 0)
            assert(std::abs(v.engine.normalizedPosition() - cue) < .001f);
    t->selectSplitPart(1);
    assert(t->controls.formantFreeze == 0);
    t->selectSplitPart(0);
    setParam(t.get(), "formant_freeze", "Off");
    renderSplit(*t, 5);
    for (const auto &v : t->voices)
        if (v.active && v.part == 0)
            assert(std::abs(v.engine.normalizedPosition() - cue) > .002f);
    t->beginClipRecording();
    renderSplit(*t, 10);
    t->applyMidiCc(44, 127);
    renderSplit(*t, 1);
    const auto countBeforeRepeat=t->activeClipSlot()->clipPointCount;
    t->applyMidiCc(44,127);
    assert(t->activeClipSlot()->clipPointCount==countBeforeRepeat);
    renderSplit(*t, 9);
    t->finishClipRecording();
    bool flag = false, point = false;
    const auto &clip = *t->activeClipSlot();
    for (int i = 0; i < clip.clipPointCount; ++i) {
        const auto p = clip.clipPoints[i].parameter & 255u;
        flag |= p == static_cast<int>(LockParameter::formantFreeze);
        point |= p == static_cast<int>(LockParameter::formantFreezePosition);
    }
    assert(flag && point);
    std::uint32_t onTime = 0;
    for (int i = 0; i < clip.clipPointCount; ++i)
        if ((clip.clipPoints[i].parameter & 255u) ==
                static_cast<int>(LockParameter::formantFreeze) &&
            clip.clipPoints[i].value >= .5f)
            onTime = clip.clipPoints[i].timeMs;
    assert(onTime > 0);
    t->applyClipCurves(clip, onTime - 1);
    assert(t->splitClipControls[0].formantFreeze == 0);
    t->applyClipCurves(clip, onTime);
    assert(t->splitClipControls[0].formantFreeze == 1 &&
           std::abs(t->splitClipControls[0].formantFreezePosition -
                    t->controls.formantFreezePosition) < 1e-6f);
}
void testFormantRoughSubAndEdits() {
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::formant;
    c.lanePath = Engine::LanePath::manual;
    c.formantFreeze = 1;
    c.formantFreezePosition = .4f;
    c.field = 1;
    auto f = formantFixture();
    const auto plain = f->render(c, 30000);
    c.formantSub = 1;
    f = formantFixture();
    const auto sub = f->render(c, 30000);
    assert(formantLineEnergy(sub, 75) > formantLineEnergy(plain, 75) * 20 + 1);
    c.formantSub = 0;
    c.formantRough = 1;
    f = formantFixture();
    const auto rough = f->render(c, 30000);
    assert(formantCorrelation(rough, 294) < formantCorrelation(plain, 294) - .05);
    auto repeat = formantFixture();
    assert(rough == repeat->render(c, 30000));
    float previous = rough.back(), jump = 0, peak = 0;
    for (int block = 0; block < 300; ++block) {
        if (block % 8 == 0) {
            c.formantTune = (block / 8) % 2;
            c.formantNoteHz = 220 + block % 4 * 110;
            c.formantKeytrack = (block % 3 - 1);
            c.formantSub = (block / 8) % 2;
            c.formantRough = (block / 16) % 2;
            c.formantShift = (block % 3 - 1) * 12;
        }
        for (float x : f->render(c, 128)) {
            assert(std::isfinite(x));
            jump = std::max(jump, std::abs(x - previous));
            peak = std::max(peak, std::abs(x));
            previous = x;
        }
    }
    std::fprintf(stderr, "Formant articulation peak %.4f jump %.4f\n", peak, jump);
    assert(peak < .6f && jump < .15f);
    auto native = splitFixture();
    setParam(native.get(), "algorithm", "Formant");
    native->beginClipRecording();
    renderSplit(*native, 10);
    for (int cc : {43, 44, 45, 28, 29})
        native->applyMidiCc(cc, 127);
    renderSplit(*native, 10);
    native->finishClipRecording();
    assert(native->controls.formantTune == 1 && native->controls.formantFreeze == 1 &&
           native->controls.formantKeytrack == 1 && native->controls.formantRough == 1 &&
           native->controls.formantSub == 1);
    std::array<bool, 6> found{};
    const auto &clip = *native->activeClipSlot();
    for (int i = 0; i < clip.clipPointCount; ++i) {
        const auto p = clip.clipPoints[i].parameter & 255u;
        if (p >= 199 && p <= 204)
            found[p - 199] = true;
    }
    for (bool b : found)
        assert(b);
}
void testFormantFormat22AndRecall() {
    char directory[] = "/tmp/thru-formant-controls-state-XXXXXX";
    assert(mkdtemp(directory));
    auto t = std::make_unique<ThruInstance>();
    assert(t->prepare(directory));
    t->stopBackgroundWork();
    t->setPerformanceMode(3);
    for (int part = 0; part < 4; ++part) {
        t->selectSplitPart(part);
        setParam(t.get(), "algorithm", "Formant");
        setParam(t.get(), "formant_smoothing", "75");
        setParam(t.get(), "formant_tune", std::to_string(20 * part).c_str());
        setParam(t.get(), "formant_freeze", "On");
        setParam(t.get(), "formant_keytrack", std::to_string(-25 * part).c_str());
        setParam(t.get(), "formant_rough", std::to_string(10 * part).c_str());
        setParam(t.get(), "formant_sub", std::to_string(15 * part).c_str());
        setParam(t.get(), "formant_freeze_position", std::to_string(.2f + part * .1f).c_str());
    }
    t->captureActiveClip(true);
    assert(t->copyClipToClipboard(0));
    t->pasteClipFromClipboard(1);
    for (int part = 0; part < 4; ++part)
        assert(t->clipSlots[1].splitValues[part] == t->clipSlots[0].splitValues[part]);
    assert(t->savePersistentState());
    auto restored = std::make_unique<ThruInstance>();
    assert(restored->prepare(directory));
    for (int part = 0; part < 4; ++part) {
        restored->selectSplitPart(part);
        const auto &c = restored->controls;
        assert(c.formantFreeze == 1 && std::abs(c.formantTune - .2f * part) < 1e-5f &&
               std::abs(c.formantKeytrack + .25f * part) < 1e-5f &&
               std::abs(c.formantRough - .1f * part) < 1e-5f &&
               std::abs(c.formantSub - .15f * part) < 1e-5f &&
               std::abs(c.formantFreezePosition - (.2f + part * .1f)) < 1e-5f);
    }
    restored.reset();
    const auto path = std::string(directory) + "/thru-clips.bin";
    auto *file = std::fopen(path.c_str(), "rb");
    assert(file);
    std::fseek(file, 0, SEEK_END);
    const auto size = std::ftell(file);
    std::rewind(file);
    std::vector<unsigned char> bytes(size - 4);
    assert(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size());
    std::fclose(file);
    constexpr std::size_t extension =
        8 + 64 * 5 * (kLockParameterCount - kVersion19LockParameterCount) * 4;
    const auto at = bytes.size() - extension;
    std::vector<unsigned char> old(bytes.begin(), bytes.begin() + at);
    old[8] = 22;
    const auto append = [&](std::uint32_t x) {
        for (int b = 0; b < 4; ++b)
            old.push_back(static_cast<unsigned char>(x >> (b * 8)));
    };
    append(0x4e455731u);
    append(23);
    for (std::size_t group = 0; group < 64 * 5; ++group) {
        const auto offset = at + 8 + group * 29 * 4;
        old.insert(old.end(), bytes.begin() + offset, bytes.begin() + offset + 23 * 4);
    }
    std::uint32_t checksum = 2166136261u;
    for (auto b : old) {
        checksum ^= b;
        checksum *= 16777619u;
    }
    append(checksum);
    file = std::fopen(path.c_str(), "wb");
    assert(file);
    assert(std::fwrite(old.data(), 1, old.size(), file) == old.size());
    assert(std::fclose(file) == 0);
    assert(t->readClipFileFromPath(path.c_str()));
    const auto check = [](const auto &values) {
        for (int p = 199; p < 204; ++p)
            assert(values[p] == 0);
        assert(values[204] == .5f);
    };
    for (const auto &clip : t->clipSlots) {
        check(clip.baseValues);
        for (const auto &part : clip.splitValues)
            check(part);
    }
    for (int part = 0; part < 4; ++part)
        assert(t->activeClipSlot()->splitValues[part][198] == 75);
    // A format-22 JSON array has the three original Formant controls only.
    std::array<char, kStateTextSize> json{};
    std::strcpy(json.data(), "{}");
    assert(t->appendSplitJson(json.data(), json.size(), 2) > 0);
    std::string text = json.data();
    const auto atCount = text.find("\"split_params_count\":205");
    assert(atCount != std::string::npos);
    text.replace(atCount, std::strlen("\"split_params_count\":205"), "\"split_params_count\":199");
    for (const auto *name : {"split_single", "split_a", "split_b", "split_c", "split_d"}) {
        const auto begin = text.find(std::string("\"") + name + "\":[");
        assert(begin != std::string::npos);
        auto cut = text.find('[', begin) + 1;
        for (std::size_t i = 0; i < kVersion22LockParameterCount; ++i)
            cut = text.find(',', cut) + 1;
        auto after = cut;
        for (int i = 0; i < 6; ++i)
            after = text.find(',', after) + 1;
        text.erase(cut, after - cut);
    }
    assert(t->restoreSplitJson(text.c_str()));
    for (int part = 0; part < 4; ++part) {
        t->selectSplitPart(part);
        const auto &c = t->controls;
        assert(c.formantTune == 0 && c.formantFreeze == 0 && c.formantKeytrack == 0 &&
               c.formantRough == 0 && c.formantSub == 0 && c.formantFreezePosition == .5f &&
               c.formantSmoothingMs == 75);
    }
    t.reset();
    removeBackgroundTestDirectory(directory);
}

void testFormantSingleFreezeCurveEdges() {
    auto t = splitFixture();
    t->setPerformanceMode(0);
    setParam(t.get(), "algorithm", "Formant");
    t->beginClipRecording();
    renderSplit(*t, 8);
    t->applyMidiCc(44, 127);
    renderSplit(*t, 8);
    t->applyMidiCc(44, 0);
    renderSplit(*t, 8);
    t->finishClipRecording();
    const auto &clip = *t->activeClipSlot();
    std::uint32_t on = 0, off = 0;
    for (int i = 0; i < clip.clipPointCount; ++i)
        if ((clip.clipPoints[i].parameter & 255u) ==
            static_cast<int>(LockParameter::formantFreeze)) {
            const auto &p = clip.clipPoints[i];
            if (p.value >= .5f)
                on = p.timeMs;
            else if (p.timeMs > 0)
                off = p.timeMs;
        }
    assert(on > 0 && off > on);
    t->applyClipCurves(clip, on - 1);
    assert(t->clipControls.formantFreeze == 0);
    t->applyClipCurves(clip, on);
    assert(t->clipControls.formantFreeze == 1);
    t->applyClipCurves(clip, off - 1);
    assert(t->clipControls.formantFreeze == 1);
    t->applyClipCurves(clip, off);
    assert(t->clipControls.formantFreeze == 0);
}

void testFormantArticulationAllLayouts() {
    auto material = formantFixture();
    for (int mode = 0; mode < 4; ++mode) {
        auto t = splitFixture();
        t->setPerformanceMode(mode);
        for (std::size_t lane = 0; lane < 4; ++lane) {
            std::copy_n(material->audio[lane].data(), 8192, t->engine.writableLane(lane, 0));
            std::copy_n(material->audio[lane].data() + SpectralFixture::frames, 8192,
                        t->engine.writableLane(lane, 1));
        }
        const int parts = mode ? t->partCount() : 1, notes = mode ? t->voicesPerPart() : 8;
        for (int part = 0; part < parts; ++part) {
            if (mode)
                t->selectSplitPart(part);
            setParam(t.get(), "algorithm", "Formant");
            setParam(t.get(), "formant_tune", "100");
            setParam(t.get(), "formant_freeze", "On");
            setParam(t.get(), "formant_keytrack", part % 2 ? "25" : "-25");
            setParam(t.get(), "formant_rough", "60");
            setParam(t.get(), "formant_sub", "50");
            setParam(t.get(), "formant_shift", "-12");
            t->controls.effectDrive = .2f;
        }
        t->controls.effectTapeMix = .2f;
        t->controls.effectReverbMix = .2f;
        t->refreshPlaybackVoices();
        for (std::size_t lane = 0; lane < 4; ++lane) {
            auto bank = std::make_unique<s3g_daisy::ThruSpectralBank>();
            bank->analyze(t->engine.laneData(lane, 0), t->engine.laneData(lane, 1),
                          t->engine.laneFrames(lane), t->sampleRate,
                          t->wavesetSourceRevisions[lane]);
            t->preparedSpectralBanks[lane] = std::move(bank);
        }
        t->spectralWorker.exchangePreparedBanks(t->preparedSpectralBanks);
        t->serviceSpectralAnalysis();
        for (int part = 0; part < parts; ++part)
            for (int n = 0; n < notes; ++n)
                t->padNoteOn(68 + (mode ? t->padForPart(part, n) : n), 100);
        float peak = 0;
        std::array<float, kBlockFrames * 2> audio{};
        for (int block = 0; block < 48; ++block) {
            t->render(nullptr, kBlockFrames, audio.data());
            for (float x : audio) {
                assert(std::isfinite(x));
                peak = std::max(peak, std::abs(x));
            }
        }
        int active = 0;
        for (const auto &voice : t->voices)
            active += voice.active && voice.held;
        assert(active == 8 && peak > .0001f && peak <= 1);
    }
}
