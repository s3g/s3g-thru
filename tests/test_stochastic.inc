// Sample-derived deformation, independent voices and fixed-cost live edits.
void testStochasticWalk() {
    s3g_daisy::ThruStochasticWalk a, b;
    a.reset(123);
    b.reset(123);
    double variation = 0;
    for (int n = 0; n < 2000; ++n) {
        a.evolve(.95f, 0, n % 3);
        b.evolve(.95f, 0, n % 3);
        const auto shape = a.shape(1, 1, 1), copy = b.shape(1, 1, 1), neutral = a.shape(0, 0, 0);
        assert(shape.time == copy.time && shape.gain == copy.gain && shape.drift == copy.drift);
        assert(shape.drift >= .7 && shape.drift <= 1.42);
        float previous = -1;
        for (int i = 0; i <= 128; ++i) {
            const float phase = i / 128.f;
            const auto p = shape.at(phase), q = neutral.at(phase);
            assert(std::isfinite(p.slope) && p.slope > 0 && p.position >= previous - 1e-6f &&
                   p.position >= 0 && p.position <= 1);
            assert(p.gain >= -2.2f - 1e-6f && p.gain <= 4.2f + 1e-6f && p.drive >= 1 &&
                   p.drive <= 48.001f);
            assert(std::abs(q.position - phase) < 1e-6f && q.gain == 1 && neutral.drift == 1);
            previous = p.position;
            variation += std::abs(p.position - phase);
        }
        assert(std::abs(shape.at(0).gain - shape.at(1).gain) < 1e-6f);
        assert(std::abs(shape.at(0).slope - shape.at(1).slope) < 1e-3f);
    }
    assert(variation > 100);
    std::puts(
        "s3g Thru Stochastic bounded reproducible walks, identity and monotone C1 maps: PASS");
}
struct StochasticFixture {
    static constexpr std::size_t frames = 16384;
    Engine engine;
    std::array<std::vector<float>, 4> pcm;
    std::array<s3g_daisy::ThruWavesetBank, 4> banks;
    std::array<std::array<float, 64>, 4> input{}, output{};
    std::array<const float *, 4> in{};
    std::array<float *, 4> out{};
    StochasticFixture(float rate = 44100, bool prepared = true, bool silent = false,
                      bool stack = true, float rightScale = -.5f) {
        for (int lane = 0; lane < 4; ++lane) {
            pcm[lane].resize(frames * 2);
            for (std::size_t i = 0; i < frames; ++i) {
                const float x =
                    silent ? 0 : .35f * std::sin(6.283185307179586 * (220 + lane * 30) * i / rate);
                pcm[lane][i] = x;
                pcm[lane][frames + i] = rightScale * x;
            }
            in[lane] = input[lane].data();
            out[lane] = output[lane].data();
        }
        assert(engine.prepare(44100, pcm[0].data(), pcm[1].data(), pcm[2].data(), pcm[3].data(),
                              frames * 2));
        if (stack)
            for (int lane = 0; lane < 4; ++lane) {
                assert(engine.adoptLane(lane, frames, rate));
                if (prepared) {
                    banks[lane].analyze(pcm[lane].data(), pcm[lane].data() + frames, frames, rate);
                    engine.setPreparedWavesetBank(lane, &banks[lane]);
                }
            }
        else {
            std::copy_n(pcm[0].data() + frames, frames, pcm[1].data());
            assert(engine.adoptSource(frames, 2, rate));
        }
    }
    void render(const Engine::Controls &c, int blocks = 1) {
        for (int i = 0; i < blocks; ++i)
            engine.process(in.data(), out.data(), 64, c);
    }
};
void testStochasticSourceAndPitch() {
    for (float rate : {22050.f, 44100.f, 48000.f, 96000.f})
        for (bool prepared : {false, true}) {
            std::array<float, 2> frequency{};
            for (int octave = 0; octave < 2; ++octave) {
                auto f = std::make_unique<StochasticFixture>(rate, prepared);
                Engine::Controls c;
                c.algorithm = Engine::PlaybackAlgorithm::stochastic;
                c.lanePath = Engine::LanePath::manual;
                c.stochasticSize = 4;
                c.stochasticAmplitude = c.stochasticTiming = c.stochasticDrift = 0;
                c.notePitch = octave ? 2 : 1;
                f->render(c, 80);
                std::vector<float> audio;
                for (int b = 0; b < 400; ++b) {
                    f->render(c);
                    for (int i = 0; i < 64; ++i) {
                        audio.push_back(f->output[0][i]);
                        assert(std::abs(f->output[1][i] + .5f * f->output[0][i]) < 1e-6f);
                    }
                }
                frequency[octave] = spectralFrequency(audio);
                assert(frequency[octave] > 100);
            }
            // Prepared crossings repeat entire cycles; fallback preserves a broader
            // band around the source pitch through overlapping arbitrary fragments.
            std::fprintf(stderr, "Stochastic pitch rate=%g prepared=%d low=%g high=%g\n", rate,
                         prepared, frequency[0], frequency[1]);
            assert(frequency[1] > frequency[0] * 1.65f && frequency[1] < frequency[0] * 2.4f);
        }
    for (bool stack : {false, true}) {
        auto f = std::make_unique<StochasticFixture>(44100, true, true, stack);
        Engine::Controls c;
        c.algorithm = Engine::PlaybackAlgorithm::stochastic;
        c.stochasticAmplitude = c.stochasticTiming = c.stochasticDrift = 1;
        c.stochasticEvolution = 30;
        f->render(c, 400);
        for (const auto &channel : f->output)
            for (float x : channel)
                assert(x == 0);
    }
    // Independent note seeds lead to distinct deformations of the same PCM.
    auto a = std::make_unique<StochasticFixture>(), b = std::make_unique<StochasticFixture>();
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::stochastic;
    c.stochasticAmplitude = c.stochasticTiming = 1;
    c.stochasticEvolution = 30;
    c.cutVoiceIndex = 1;
    a->render(c, 600);
    c.cutVoiceIndex = 2;
    b->render(c, 600);
    double difference = 0;
    for (int i = 0; i < 64; ++i)
        difference += std::abs(a->output[0][i] - b->output[0][i]);
    assert(difference > .01);
    // Even a brief note at EVOL Hold receives a distinct captured shape.
    auto shortA = std::make_unique<StochasticFixture>(),
         shortB = std::make_unique<StochasticFixture>();
    c.stochasticEvolution = 0;
    c.cutVoiceIndex = 1;
    shortA->render(c, 24);
    c.cutVoiceIndex = 2;
    shortB->render(c, 24);
    double shortDifference = 0;
    for (int i = 0; i < 64; ++i)
        shortDifference += std::abs(shortA->output[0][i] - shortB->output[0][i]);
    assert(shortDifference > .001);
    std::puts("s3g Thru Stochastic sample-only silence, source rates, octave pitch and stereo "
              "linkage: PASS");
}
void testStochasticLiveEditsAndLayouts() {
    for (bool prepared : {false, true}) {
        auto f = std::make_unique<StochasticFixture>(44100, prepared, false, true, 1);
        Engine::Controls c;
        c.algorithm = Engine::PlaybackAlgorithm::stochastic;
        c.lanePath = Engine::LanePath::manual;
        c.stochasticAmplitude = c.stochasticTiming = 0;
        c.stochasticEvolution = 30;
        f->render(c, 100);
        float previous = f->output[0].back(), jump = 0, peak = 0;
        double energy = 0;
        for (int b = 0; b < 1800; ++b) {
            if (b % 19 == 0) {
                const bool high = (b / 19) & 1;
                c.stochasticSize = high ? 64 : 1;
                c.stochasticAmplitude = c.stochasticTiming = c.stochasticDrift = high ? 1 : 0;
                c.stochasticMemory = high ? 1 : 0;
                c.stochasticPull = high ? 0 : 1;
                c.stochasticLaw = (b / 19) % 3;
                c.manualLane = (b / 19) % 4;
                c.locus = high ? .8f : 0;
                c.field = high ? .02f : 1;
            }
            f->render(c);
            for (int i = 0; i < 64; ++i) {
                const float x = f->output[0][i];
                assert(std::isfinite(x));
                jump = std::max(jump, std::abs(x - previous));
                peak = std::max(peak, std::abs(x));
                energy += x * x;
                previous = x;
                assert(std::abs(f->output[1][i] - x) < 1e-6f);
            }
        }
        std::fprintf(stderr, "Stochastic live edits prepared=%d peak %.4f max step %.4f\n",
                     prepared, peak, jump);
        assert(energy > 1 && peak < 1.5f && jump < 2.0f);
    }
    // Eight physical voices in each layout, including assigned engines.
    for (int mode = 0; mode < 4; ++mode) {
        auto t = splitFixture();
        t->setPerformanceMode(mode);
        for (int part = 0; part < (mode ? t->partCount() : 1); ++part) {
            t->selectSplitPart(part, false);
            setParam(t.get(), "algorithm", "Stochastic");
            setParam(t.get(), "stochastic_timing", "80");
            t->midiPart = part;
            for (int i = 0; i < t->engineSlotCount(); ++i)
                t->noteOn(48 + i * 2, 100);
        }
        renderSplit(*t, 120);
        int count = 0;
        for (const auto &v : t->voices)
            if (v.active) {
                ++count;
                assert(v.engine.playbackAlgorithm() == Engine::PlaybackAlgorithm::stochastic);
                assert(t->effectiveControls(v).notePitch > 0);
            }
        assert(count == 8);
    }
    std::puts(
        "s3g Thru Stochastic live edits, lane/window moves and eight voices in every layout: PASS");
}
void testStochasticPersistenceAndMidi() {
    char directory[] = "/tmp/thru-stochastic-state-XXXXXX";
    assert(mkdtemp(directory));
    auto t = std::make_unique<ThruInstance>();
    assert(t->prepare(directory));
    t->stopBackgroundWork();
    t->setPerformanceMode(3);
    for (int part = 0; part < 4; ++part) {
        t->selectSplitPart(part, false);
        setParam(t.get(), "algorithm", "13");
        setParam(t.get(), "stochastic_size", std::to_string(part + 2).c_str());
        setParam(t.get(), "stochastic_timing", std::to_string(part * 20).c_str());
        setParam(t.get(), "stochastic_law", part & 1 ? "Listen" : "Wide");
        setParam(t.get(), "stochastic_capture", std::to_string(20 + part * 15).c_str());
        setParam(t.get(), "stochastic_pull", "37");
        setParam(t.get(), "voice_engine_mode", "1");
        setListSlot(*t, 0, 13);
    }
    const int focus = t->splitFocus;
    nrpn(*t, 5, "stochastic_evolution",
         s3g_thru_midi::encode(midiParameter("stochastic_evolution"), 7.25, 1024));
    assert(t->splitFocus == focus &&
           std::abs(t->liveControlsForPart(0).stochasticEvolution - 7.25) < .04);
    t->e16Page = 2;
    t->e16Pair = 0;
    assert(e16Row(*t, 0) > 0); // E16 Remote selects the new detail row.
    t->captureActiveClip(true);
    t->midiPart = 0;
    t->noteOn(60, 100);
    assert(newestVoice(*t, 0).assignedAlgorithm == 13);
    t->beginClipRecording();
    renderSplit(*t, 20);
    nrpn(*t, 5, "stochastic_amplitude", 12000);
    nrpn(*t, 6, "stochastic_capture", s3g_thru_midi::encode(midiParameter("stochastic_capture"), 74, 100));
    renderSplit(*t, 20);
    t->finishClipRecording();
    assert(t->activeClipSlot()->clipNoteCount > 0 &&
           t->activeClipSlot()->clipNotes[0].engineAlgorithm == 13);
    bool automation = false, captureAutomation = false;
    for (std::size_t i = 0; i < t->activeClipSlot()->clipPointCount; ++i) {
        const auto &p = t->activeClipSlot()->clipPoints[i];
        automation |= p.parameter == static_cast<int>(LockParameter::stochasticAmplitude);
        captureAutomation |= p.parameter == static_cast<int>(LockParameter::stochasticCapture) && p.sourcePad==1;
    }
    assert(automation && captureAutomation);
    assert(t->savePersistentState());
    std::array<char, kStateTextSize> json{};
    assert(t->serializeState(json.data(), json.size()) > 0);
    auto copy = std::make_unique<ThruInstance>();
    assert(copy->prepare(directory));
    copy->stopBackgroundWork();
    assert(copy->restoreState(json.data()));
    for (int part = 0; part < 4; ++part) {
        copy->selectSplitPart(part, false);
        assert(copy->controls.algorithm == Engine::PlaybackAlgorithm::stochastic &&
               copy->controls.stochasticSize == part + 2 &&
               std::abs(copy->controls.stochasticTiming - part * .2f) < 1e-6f &&
               copy->voiceEngineSettings.engines[0] == 13 &&
               copy->controls.stochasticLaw == (part & 1 ? 3 : 2) &&
               std::abs(copy->controls.stochasticCapture - (part==1 ? .74f : .2f + part * .15f)) < .001f &&
               std::abs(copy->controls.stochasticPull-.37f)<.001f);
    }
    const auto &saved = *copy->activeClipSlot();
    assert(saved.clipNoteCount > 0 && saved.clipNotes[0].engineAlgorithm == 13);
    assert(std::abs(saved.splitValues[3][static_cast<int>(LockParameter::stochasticCapture)]-65)<.001f);
    copy->triggerClipVoice(saved, 0);
    bool played = false;
    for (const auto &v : copy->voices)
        if (v.active && v.clipOwned) {
            assert(v.assignedAlgorithm == 13);
            played = true;
        }
    assert(played);
    // A genuine format-25 payload still imports with neutral defaults for the
    // new controls. It does not carry algorithm 13, which did not exist then.
    for (auto &clip : t->clipSlots) {
        clip.clipNoteCount = clip.clipPointCount = 0;
        clip.baseValues[static_cast<int>(LockParameter::algorithm)] = 0;
        for (auto &values : clip.splitValues)
            values[static_cast<int>(LockParameter::algorithm)] = 0;
        clip.singleSettings.engines = {};
        for (auto &settings : clip.splitSettings)
            settings.engines = {};
    }
    t->controls.algorithm = Engine::PlaybackAlgorithm::motion;
    t->voiceEngineSettings = {};
    for (auto &p : t->splitParts) {
        p.controls.algorithm = Engine::PlaybackAlgorithm::motion;
        p.settings.engines = {};
    }
    t->persistenceGeneration = 1;
    assert(t->writeClipFile());
    const auto path = std::string(directory) + "/thru-clips.bin";
    auto *file = std::fopen(path.c_str(), "rb");
    assert(file);
    std::fseek(file, 0, SEEK_END);
    const auto size = std::ftell(file);
    std::rewind(file);
    std::vector<unsigned char> bytes(size - 4);
    assert(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size());
    std::fclose(file);
    stripStochasticExtension(bytes);
    std::uint32_t checksum = 2166136261u;
    for (auto byte : bytes) {
        checksum ^= byte;
        checksum *= 16777619u;
    }
    for (int b = 0; b < 4; ++b)
        bytes.push_back(checksum >> (8 * b));
    file = std::fopen(path.c_str(), "wb");
    assert(file);
    assert(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
    assert(std::fclose(file) == 0);
    assert(copy->readClipFileFromPath(path.c_str()));
    for (const auto &clip : copy->clipSlots)
        for (int g = 0; g < 5; ++g) {
            const auto &values = g ? clip.splitValues[g - 1] : clip.baseValues;
            assert(values[static_cast<int>(LockParameter::stochasticSize)] == 2 &&
                   values[static_cast<int>(LockParameter::stochasticTiming)] == 70);
        }
    copy.reset();
    t.reset();
    removeBackgroundTestDirectory(directory);
    std::puts(
        "s3g Thru Stochastic per-part state, NRPN/E16, list capture and format-25 migration: PASS");
}

void testStochasticExtremeFilteringAndHistory() {
    using Folder = s3g_daisy::ThruStochasticFolder;
    Folder silence;
    for (int i = 0; i < 2000; ++i)
        assert(silence.process(0, 48, 1) == 0);
    Folder a, b;
    double naiveError = 0, adaaError = 0;
    float previous = 0;
    for (int i = 0; i < 10000; ++i) {
        const float x = .7f * std::sin(i * .31f);
        const float y = a.process(x, 24, 1), other = b.process(-x, 24, 1);
        assert(std::isfinite(y) && std::abs(y) <= 1.00001f && std::abs(y + other) < 1e-5f);
        if (i) {
            double reference = 0;
            for (int n = 0; n < 128; ++n)
                reference += Folder::fold((previous + (x - previous) * (n + .5) / 128) * 24);
            reference /= 128;
            adaaError += (y - reference) * (y - reference);
            const auto naive = Folder::fold(x * 24);
            naiveError += (naive - reference) * (naive - reference);
        }
        previous = x;
    }
    assert(adaaError < naiveError * .01);
    constexpr std::size_t count = 8192;
    std::vector<float> left(count), right(count);
    for (std::size_t i = 0; i < count; ++i)
        left[i] = right[i] = std::sin(6.28318530718 * 9000 * i / 44100);
    s3g_daisy::ThruSamplePyramid pyramid;
    pyramid.analyze(left.data(), right.data(), count);
    double rawEnergy = 0, filteredEnergy = 0;
    for (int i = 64; i < 1900; ++i) {
        const double p = i * 4.0;
        const float raw = pyramid.linear(left.data(), count, p),
                    filtered = pyramid.read(pyramid.prepare(left.data(), count, 0, p, 2));
        rawEnergy += raw * raw;
        filteredEnergy += filtered * filtered;
    }
    assert(filteredEnergy < rawEnergy * .001);
    for (int level = 0; level < 9; ++level)
        for (float v : pyramid.data[level].pcm[0])
            assert(std::isfinite(v));
    s3g_daisy::ThruStochasticWalk walk;
    walk.reset(42);
    int inverted = 0;
    float ratio = 0;
    for (int n = 0; n < 3000; ++n) {
        walk.evolve(.95f, 0, 2);
        const auto shape = walk.shape(1, 1, 0);
        float smallest = 1, largest = 0;
        for (int i = 0; i < walk.segments; ++i) {
            inverted += shape.gain[i] < 0;
            const float width = shape.time[i + 1] - shape.time[i];
            smallest = std::min(smallest, width);
            largest = std::max(largest, width);
        }
        ratio = std::max(ratio, largest / std::max(1e-8f, smallest));
        assert(shape.drift == 1);
    }
    assert(inverted > 100 && ratio > 100);
    std::puts("s3g Thru Stochastic signed/folded PCM, accumulated maps, ADAA and source anti-alias "
              "rejection: PASS");
}
void testStochasticHoldLatchAndRecording() {
    for (int mode = 0; mode < 4; ++mode) {
        auto t = splitFixture();
        t->setPerformanceMode(mode);
        for (int part = 0; part < (mode ? t->partCount() : 1); ++part) {
            t->selectSplitPart(part, false);
            setParam(t.get(), "algorithm", "Stochastic");
            setParam(t.get(), "hold_release_ms", "0");
            setParam(t.get(), "pad_mode", "Keys");
            const int pad = mode ? t->padForPart(part, 0) : 0;
            t->shiftHeld = true;
            t->padNoteOn(68 + pad, 100);
            t->shiftHeld = false;
            t->padNoteOff(68 + pad);
            renderSplit(*t, 80);
            assert(t->latchedPadNotes[pad] >= 0);
            auto &voice = newestVoice(*t, part);
            assert(voice.held);
            setParam(t.get(), "stochastic_source_hold", "On");
            const float point = t->controls.stochasticHoldPosition;
            renderSplit(*t, 150);
            assert(std::abs(voice.engine.normalizedPosition() - point) < .001f);
            t->midiPart = part;
            t->noteOn(72, 100);
            auto &next = newestVoice(*t, part);
            renderSplit(*t, 40);
            assert(std::abs(next.engine.normalizedPosition() - point) < .001f);
            const auto age = voice.age;
            setParam(t.get(), "stochastic_evolution", "1024");
            renderSplit(*t, 50);
            assert(voice.held && voice.age == age);
            setParam(t.get(), "stochastic_source_hold", "Off");
            renderSplit(*t, 15);
            assert(std::abs(next.engine.normalizedPosition() - point) > .005f);
            t->allNotesOff();
            renderSplit(*t, 100);
        }
    }
    for (int mode : {0, 1}) {
        auto t = splitFixture();
        t->setPerformanceMode(mode);
        setParam(t.get(), "algorithm", "13");
        t->noteOn(60, 100);
        renderSplit(*t, 50);
        t->beginClipRecording();
        renderSplit(*t, 30);
        const auto before = static_cast<std::uint32_t>(t->clipCaptureTimeMs());
        setParam(t.get(), "stochastic_source_hold", "1");
        const float position = t->controls.stochasticHoldPosition;
        renderSplit(*t, 50);
        setParam(t.get(), "stochastic_source_hold", "0");
        renderSplit(*t, 10);
        t->finishClipRecording();
        const auto &clip = *t->activeClipSlot();
        assert(clip.clipPointCount > 0);
        t->applyClipCurves(clip, before * .5);
        const auto &c = mode ? t->splitClipControls[0] : t->clipControls;
        assert(c.stochasticSourceHold == 0);
        t->applyClipCurves(clip, before + 10);
        assert(c.stochasticSourceHold == 1 &&
               std::abs(c.stochasticHoldPosition - position) < 1e-5f);
    }
    {
        auto t = splitFixture();
        t->setPerformanceMode(0);
        setParam(t.get(), "algorithm", "13");
        setParam(t.get(), "stochastic_source_hold", "1");
        setParam(t.get(), "stochastic_hold_position", "0.25");
        t->beginClipRecording();
        renderSplit(*t, 30);
        const auto before = t->clipCaptureTimeMs();
        nrpn(*t, 5, "stochastic_hold_position", 12000);
        renderSplit(*t, 30);
        t->finishClipRecording();
        const auto &clip = *t->activeClipSlot();
        t->applyClipCurves(clip, before * .5);
        assert(t->clipControls.stochasticHoldPosition == .25f);
        t->applyClipCurves(clip, before + 10);
        assert(t->clipControls.stochasticHoldPosition > .7f);
    }
    std::puts("s3g Thru Stochastic source dwell, new-note recall, manual latch and discrete hold "
              "recording: PASS");
}
void testStochasticFormat26Import() {
    for (int version : {26, 27}) {
    char directory[] = "/tmp/thru-stochastic-v26-XXXXXX";
    assert(mkdtemp(directory));
    auto t = std::make_unique<ThruInstance>();
    assert(t->prepare(directory));
    t->stopBackgroundWork();
    setParam(t.get(), "algorithm", "13");
    setParam(t.get(), "stochastic_size", "9");
    setParam(t.get(), "stochastic_evolution", "17");
    setParam(t.get(), "stochastic_source_hold", "On");
    setParam(t.get(), "stochastic_pull", "37");
    setParam(t.get(), "stochastic_capture", "90");
    t->persistenceGeneration = 1;
    assert(t->writeClipFile());
    const auto path = std::string(directory) + "/thru-clips.bin";
    auto *file = std::fopen(path.c_str(), "rb");
    assert(file);
    std::fseek(file, 0, SEEK_END);
    const auto size = std::ftell(file);
    std::rewind(file);
    std::vector<unsigned char> bytes(size - 4);
    assert(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size());
    std::fclose(file);
    stripPulsarExtension(bytes);
    const auto start = bytes.size() - (4 + 64 * 5 * 11 * 4);
    assert(bytes[start] == 0x31 && bytes[start + 1] == 0x4f && bytes[start + 2] == 0x54 &&
           bytes[start + 3] == 0x53);
    std::vector<unsigned char> old(bytes.begin(), bytes.begin() + start + 4);
    for (int group = 0; group < 64 * 5; ++group)
        old.insert(old.end(), bytes.begin() + start + 4 + group * 44,
                   bytes.begin() + start + 4 + group * 44 + (version == 26 ? 32 : 40));
    old[8] = version;
    std::uint32_t sum = 2166136261u;
    for (auto v : old) {
        sum ^= v;
        sum *= 16777619u;
    }
    for (int b = 0; b < 4; ++b)
        old.push_back(sum >> (8 * b));
    file = std::fopen(path.c_str(), "wb");
    assert(file);
    assert(std::fwrite(old.data(), 1, old.size(), file) == old.size());
    assert(std::fclose(file) == 0);
    assert(t->readClipFileFromPath(path.c_str()));
    const auto &values = t->activeClipSlot()->baseValues;
    assert(values[static_cast<int>(LockParameter::stochasticSize)] == 9 &&
           values[static_cast<int>(LockParameter::stochasticEvolution)] == 17 &&
           values[static_cast<int>(LockParameter::stochasticSourceHold)] == (version == 26 ? 0 : 1) &&
           values[static_cast<int>(LockParameter::stochasticHoldPosition)] == .5f &&
           std::abs(values[static_cast<int>(LockParameter::stochasticPull)] - 37) < .001f &&
           std::abs(values[static_cast<int>(LockParameter::stochasticCapture)] - 65) < .001f);
    t.reset();
    removeBackgroundTestDirectory(directory);
    }
    std::puts("s3g Thru Stochastic formats 26/27 import with independent listener default: PASS");
}

void testStochasticControlEdgesAndTransformation() {
    auto f = std::make_unique<StochasticFixture>(44100, true, false, true, 1),
         reference = std::make_unique<StochasticFixture>();
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::stochastic;
    c.stochasticSourceHold = 1;
    c.stochasticHoldPosition = .25f;
    c.stochasticSize = 2;
    f->render(c, 100);
    for (int change = 0; change < 80; ++change) {
        reference->engine = f->engine;
        reference->render(c);
        c.stochasticAmplitude = (change & 1) ? 0 : 1;
        c.stochasticTiming = (change & 1) ? 1 : 0;
        c.stochasticEvolution = change % 3 == 0 ? 1024 : change % 3 == 1 ? 0 : 500;
        c.stochasticLaw = change % 4;
        c.stochasticListenTarget = change & 1;
        c.stochasticCapture = change & 1;
        c.stochasticMemory = (change & 1) ? 0 : 1;
        c.stochasticPull = (change & 1) ? 1 : 0;
        c.stochasticDrift = (change & 1) ? 0 : 1;
        f->render(c);
        assert(std::abs(f->output[0][0] - reference->output[0][0]) < 1e-6f);
        f->render(c, 10);
    }
    std::array<double, 2> high{};
    for (int mode = 0; mode < 2; ++mode) {
        auto source = std::make_unique<StochasticFixture>();
        c.stochasticDrift = 0;
        c.stochasticAmplitude = c.stochasticTiming = mode ? 1 : 0;
        c.stochasticEvolution = mode ? 1024 : 0;
        c.stochasticPull = 0;
        c.stochasticMemory = .9f;
        source->render(c, 200);
        std::vector<float> audio;
        for (int block = 0; block < 400; ++block) {
            source->render(c);
            for (float x : source->output[0])
                audio.push_back(x);
        }
        high[mode] = formantBandEnergy(audio, 1000, 16000);
    }
    std::fprintf(stderr, "Stochastic upper-band energy neutral %.6f / extreme %.6f\n", high[0],
                 high[1]);
    assert(high[1] > high[0] * 10);
    std::puts("s3g Thru Stochastic no immediate control edge and substantial sample "
              "transformation: PASS");
}
