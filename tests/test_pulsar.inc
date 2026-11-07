void testPulsarWindowsAndClock() {
    using P = s3g_daisy::ThruPulsar;
    for (int shape = 0; shape < 5; ++shape)
        for (float skew : {0.f, .5f, 1.f}) {
            assert(P::window(0, shape, skew) == 0 && P::window(1, shape, skew) == 0);
            for (int i = 0; i <= 1000; ++i) {
                const float x = P::window(i / 1000.f, shape, skew);
                assert(std::isfinite(x) && x >= 0 && x <= 1.00001f);
            }
            assert(P::window(.00001f, shape, skew) < .001f &&
                   P::window(.99999f, shape, skew) < .001f);
        }
    for (int mode = 0; mode < 9; ++mode) {
        int on = 0;
        const auto &b = P::bursts[mode];
        for (int i = 0; i < b[0] + b[1]; ++i)
            on += P::burstAllows(i, mode);
        assert(on == b[0]);
    }
    for (bool prepared : {false, true})
        for (float rate : {22050.f, 44100.f, 48000.f, 96000.f}) {
            std::array<float, 2> pitch{};
            for (int octave = 0; octave < 2; ++octave) {
                auto f = std::make_unique<StochasticFixture>(rate, prepared, false, true, 1);
                Engine::Controls c;
                c.algorithm = Engine::PlaybackAlgorithm::pulsar;
                c.pulsarRate = 160;
                c.notePitch = octave ? 2 : 1;
                c.pulsarForm = 500;
                c.pulsarSourceHold = 1;
                c.pulsarHoldPosition = .25f;
                c.lanePath = Engine::LanePath::manual;
                f->render(c, 100);
                std::vector<float> audio;
                for (int b = 0; b < 400; ++b) {
                    f->render(c);
                    for (int i = 0; i < 64; ++i) {
                        audio.push_back(f->output[0][i]);
                        assert(std::isfinite(audio.back()) &&
                               std::abs(f->output[1][i] - audio.back()) < 1e-6f);
                    }
                }
                pitch[octave] = spectralFrequency(audio);
                const auto status = f->engine.pulsarStatus();
                assert(std::abs(status[4] - (octave ? 320 : 160)) < .01f && status[2] <= 4);
            }
            // Harmonics can dominate a sampled pulsaret. Its exact periodic clock
            // is the musical reference; use autocorrelation at the expected period.
            std::fprintf(stderr, "Pulsar source=%g prepared=%d spectral peaks %g / %g\n", rate,
                         prepared, pitch[0], pitch[1]);
            assert(pitch[0] > 100 && pitch[1] > pitch[0] * 1.8f && pitch[1] < pitch[0] * 2.2f);
        }
    {
        auto f = std::make_unique<StochasticFixture>();
        Engine::Controls c;
        c.algorithm = Engine::PlaybackAlgorithm::pulsar;
        c.pulsarSourceHold = 1;
        c.pulsarRate = .1f;
        f->render(c, 80);
        c.pulsarRate = 1000;
        double energy = 0;
        for (int b = 0; b < 100; ++b) {
            f->render(c);
            for (float x : f->output[0])
                energy += x * x;
        }
        assert(energy > 1); // a faster rate does not wait out the previous ten-second interval
        c.pulsarOmit = 1;
        f->render(c, 1000);
        for (float x : f->output[0])
            assert(std::abs(x) < 1e-6f);
    }
    for (bool stack : {false, true}) {
        auto f = std::make_unique<StochasticFixture>(44100, true, true, stack);
        Engine::Controls c;
        c.algorithm = Engine::PlaybackAlgorithm::pulsar;
        c.pulsarRate = 2000;
        c.pulsarForm = 20;
        c.pulsarSize = 500;
        f->render(c, 200);
        for (const auto &ch : f->output)
            for (float x : ch)
                assert(x == 0);
        assert(f->engine.pulsarStatus()[2] <= 4 && f->engine.pulsarStatus()[3] == 1);
    }
    std::puts("s3g Thru Pulsar zero-edge windows, burst masks, stereo PCM, silence and exact "
              "note clocks: PASS");
}
void testPulsarEditsAndParts() {
    auto f = std::make_unique<StochasticFixture>(44100, true, false, true, 1),
         reference = std::make_unique<StochasticFixture>();
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::pulsar;
    c.pulsarSourceHold = 1;
    f->render(c, 60);
    double energy = 0;
    for (int edit = 0; edit < 100; ++edit) {
        reference->engine = f->engine;
        reference->render(c);
        c.pulsarRate = (edit & 1) ? 2000 : .1f;
        c.pulsarForm = (edit & 1) ? 20 : 2000;
        c.pulsarSize = (edit & 1) ? .5f : 100;
        c.pulsarWindow = edit % 5;
        c.pulsarSkew = (edit & 1);
        c.pulsarOmit = edit % 3 == 0 ? 1 : 0;
        c.pulsarBurst = edit % 9;
        c.pulsarJitter = (edit & 1);
        f->render(c);
        assert(std::abs(f->output[0][0] - reference->output[0][0]) < .03f);
        for (int b = 0; b < 10; ++b) {
            f->render(c);
            for (int i = 0; i < 64; ++i) {
                const float x = f->output[0][i];
                assert(std::isfinite(x) && std::abs(x) < 1.5f);
                assert(std::abs(x - f->output[1][i]) < 1e-6f);
                energy += x * x;
            }
        }
        assert(f->engine.pulsarStatus()[2] <= 4);
    }
    assert(energy > 1);
    // Per-layout ownership, hold capture, scans, notes held before recording,
    // list assignment, and active readers survive removal of prepared analysis.
    for (int mode = 0; mode < 4; ++mode) {
        auto t = splitFixture();
        t->setPerformanceMode(mode);
        const int parts = mode ? t->partCount() : 1;
        for (int p = 0; p < parts; ++p) {
            t->selectSplitPart(p, false);
            setParam(t.get(), "algorithm", "Pulsar");
            setParam(t.get(), "hold_release_ms", "0");
            setParam(t.get(), "pulsar_rate", "220");
            setParam(t.get(),"pulsar_emit_mod_rate","20");setParam(t.get(),"pulsar_emit_mod_depth","24");
            setParam(t.get(),"pulsar_form_mod_rate","20");setParam(t.get(),"pulsar_form_mod_depth","24");
            setParam(t.get(),"pulsar_form_variation","24");setParam(t.get(),"pulsar_spray","100");
            setParam(t.get(),"pulsar_reverse","50");setParam(t.get(),"pulsar_keytrack","100");
            const int pad = mode ? t->padForPart(p, 0) : 0;
            t->shiftHeld = true;
            t->padNoteOn(68 + pad, 100);
            t->shiftHeld = false;
            t->padNoteOff(68 + pad);
        }
        for (int part = 0; part < parts; ++part) {
            t->midiPart = part;
            for (int n = 1; n < (mode ? t->voicesPerPart() : 8); ++n)
                t->noteOn(60 + n * 3, 100);
        }
        renderSplit(*t, 80);
        assert(std::count_if(t->voices.begin(), t->voices.end(),
                             [](const auto &v) { return v.active; }) == 8);
        for (int p = 0; p < parts; ++p) {
            t->selectSplitPart(p, false);
            setParam(t.get(), "pulsar_source_hold", "On");
            auto &v = newestVoice(*t, p);
            assert(v.held);
            const auto pos = t->controls.pulsarHoldPosition;
            renderSplit(*t, 70);
            assert(std::abs(v.engine.normalizedPosition() - pos) < .001f);
            assert(v.engine.pulsarStatus()[4] > 0);
            setParam(t.get(), "voice_engine_mode", "1");
            setListSlot(*t, 0, 14);
        }
        std::array<std::int16_t, kBlockFrames * 2> audio{};
        double output = 0;
        for (int b = 0; b < 20; ++b) {
            t->render(audio.data(), kBlockFrames);
            for (auto x : audio)
                output += std::abs(x);
        }
        assert(output > 0);
        char state[1024]{};
        assert(getParam(t.get(), "pulsar_state", state, sizeof(state)) > 0);
        t->beginClipRecording();
        renderSplit(*t, 30);
        t->finishClipRecording();
        assert(t->activeClipSlot()->clipNoteCount > 0);
        t->allNotesOff();
        renderSplit(*t, 100);
    }
    auto a = std::make_unique<StochasticFixture>();
    Engine::Controls d;
    d.algorithm = Engine::PlaybackAlgorithm::pulsar;
    d.pulsarRate = 2000;
    d.pulsarForm = 20;
    d.pulsarSize = 500;
    a->render(d, 50);
    for (int lane = 0; lane < 4; ++lane)
        a->engine.setPreparedWavesetBank(lane, nullptr);
    a->render(d, 50);
    for (const auto &ch : a->output)
        for (float x : ch)
            assert(std::isfinite(x));
    std::puts("s3g Thru Pulsar live edit continuity, reader budget, holds, layouts, list "
              "selection and PCM fallback: PASS");
}
void testPulsarPersistenceAndMidi() {
    char dir[] = "/tmp/thru-pulsar-bank-XXXXXX";
    assert(mkdtemp(dir));
    auto t = std::make_unique<ThruInstance>();
    assert(t->prepare(dir));
    t->stopBackgroundWork();
    t->setPerformanceMode(3);
    for (int p = 0; p < 4; ++p) {
        t->selectSplitPart(p, false);
        setParam(t.get(), "algorithm", "Pulsar");
        setParam(t.get(), "pulsar_rate", std::to_string(100 + p * 20).c_str());
        setParam(t.get(), "pulsar_form", "700");
        setParam(t.get(), "pulsar_size", "12.5");
        setParam(t.get(), "pulsar_window", "Reverse");
        setParam(t.get(), "pulsar_burst", "2:1");
        setParam(t.get(), "pulsar_source_hold", "On");
        setListSlot(*t, 0, 14);
        setParam(t.get(), "voice_engine_mode", "1");
    }
    t->midiPart = 0;
    t->noteOn(60, 100);
    t->beginClipRecording();
    renderSplit(*t, 30);
    for (const char *key : {"pulsar_rate", "pulsar_form", "pulsar_size"}) {
        const auto &p = midiParameter(key);
        assert(p.logarithmic);
        const int middle =
            s3g_thru_midi::encode(p, std::sqrt(p.minimum * p.maximum), p.maximum);
        assert(std::abs(middle - 8192) <= 1);
        const float lo = s3g_thru_midi::decode(p, 0, p.maximum),
                    hi = s3g_thru_midi::decode(p, 16383, p.maximum);
        assert(lo == p.minimum && hi == p.maximum);
    }
    const int focus = t->splitFocus;
    nrpn(*t, 5, "pulsar_jitter", 8000);
    assert(t->splitFocus == focus);
    renderSplit(*t, 30);
    t->finishClipRecording();
    t->captureActiveClip(true);
    assert(t->savePersistentState());
    assert(t->activeClipSlot()->clipNotes[0].engineAlgorithm == 14);
    bool automated = false;
    for (std::size_t i = 0; i < t->activeClipSlot()->clipPointCount; ++i)
        automated |= t->activeClipSlot()->clipPoints[i].parameter ==
                     static_cast<int>(LockParameter::pulsarJitter);
    assert(automated);
    t->e16Page = 2;
    int owner = -1;
    const auto *param = e16Parameter(*t, 0, owner);
    assert(param && param->id == 3328 && owner == 0);
    std::array<char, kStateTextSize> json{};
    assert(t->serializeState(json.data(), json.size()) > 0);
    auto copy = std::make_unique<ThruInstance>();
    assert(copy->prepare(dir));
    copy->stopBackgroundWork();
    assert(copy->restoreState(json.data()));
    for (int p = 0; p < 4; ++p) {
        copy->selectSplitPart(p, false);
        const auto &c = copy->controls;
        assert(c.algorithm == Engine::PlaybackAlgorithm::pulsar &&
               c.pulsarRate == 100 + p * 20 && c.pulsarForm == 700 && c.pulsarSize == 12.5f &&
               c.pulsarWindow == 4 && c.pulsarBurst == 1 && c.pulsarSourceHold == 1 &&
               copy->voiceEngineSettings.engines[0] == 14);
    }
    assert(copy->activeClipSlot()->clipNotes[0].engineAlgorithm == 14);
    // Genuine prior-format bank: omit the PUL1 extension and its checksum,
    // remove the new algorithm from its older setup/recorded descriptors.
    for (auto &clip : t->clipSlots) {
        clip.clipNoteCount = clip.clipPointCount = 0;
        clip.baseValues[static_cast<int>(LockParameter::algorithm)] = 13;
        for (auto &v : clip.splitValues)
            v[static_cast<int>(LockParameter::algorithm)] = 13;
        clip.singleSettings.engines = {};
        for (auto &cfg : clip.splitSettings)
            cfg.engines = {};
    }
    t->controls.algorithm = Engine::PlaybackAlgorithm::stochastic;
    t->voiceEngineSettings = {};
    for (auto &part : t->splitParts) {
        part.controls.algorithm = Engine::PlaybackAlgorithm::stochastic;
        part.settings.engines = {};
    }
    t->activeClipOccupied = false;
    assert(t->writeClipFile());
    const auto path = std::string(dir) + "/thru-clips.bin";
    auto *f = std::fopen(path.c_str(), "rb");
    assert(f);
    std::fseek(f, 0, SEEK_END);
    const auto n = std::ftell(f);
    std::rewind(f);
    std::vector<unsigned char> bytes(n - 4);
    assert(std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size());
    std::fclose(f);
    stripPulsarExtension(bytes);
    std::uint32_t hash = 2166136261u;
    for (auto x : bytes) {
        hash ^= x;
        hash *= 16777619u;
    }
    for (int b = 0; b < 4; ++b)
        bytes.push_back(hash >> (8 * b));
    f = std::fopen(path.c_str(), "wb");
    assert(f);
    assert(std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size());
    std::fclose(f);
    assert(copy->readClipFileFromPath(path.c_str()));
    assert(copy->activeClipSlot()->baseValues[static_cast<int>(LockParameter::pulsarForm)] ==
           500);
    copy.reset();
    t.reset();
    removeBackgroundTestDirectory(dir);
    std::puts("s3g Thru Pulsar per-part MIDI, independent state, list descriptors, automation "
              "and format-28 import: PASS");
}


void testPulsarExtendedSize() {
    for(float rate:{22050.f,44100.f,48000.f,96000.f})for(bool prepared:{false,true})for(bool stack:{false,true}) {
        const auto frames=static_cast<std::size_t>(rate);
        std::array<std::vector<float>,4> pcm;
        std::array<s3g_daisy::ThruWavesetBank,4> banks;
        std::array<std::array<float,64>,4> input{},output{};
        std::array<const float*,4> in{};std::array<float*,4> out{};
        for(int lane=0;lane<4;++lane){
            pcm[lane].resize(frames*2);
            for(std::size_t f=0;f<frames;++f){const float x=.2f*std::sin(6.28318530718f*(80+lane*30)*f/rate);pcm[lane][f]=x;pcm[lane][frames+f]=-.5f*x;}
            in[lane]=input[lane].data();out[lane]=output[lane].data();
        }
        auto engine=std::make_unique<Engine>();
        assert(engine->prepare(44100,pcm[0].data(),pcm[1].data(),pcm[2].data(),pcm[3].data(),frames*2));
        if(stack)for(int lane=0;lane<4;++lane)assert(engine->adoptLane(lane,frames,rate));
        else {std::copy_n(pcm[0].data()+frames,frames,pcm[1].data());assert(engine->adoptSource(frames,2,rate));}
        if(prepared)for(int lane=0;lane<(stack?4:1);++lane){banks[lane].analyze(pcm[lane].data(),pcm[lane].data()+frames,frames,rate);engine->setPreparedWavesetBank(lane,&banks[lane]);}
        Engine::Controls c;c.algorithm=Engine::PlaybackAlgorithm::pulsar;c.lanePath=Engine::LanePath::manual;
        c.pulsarRate=40;c.pulsarForm=20;c.pulsarSize=500;c.pulsarSourceHold=1;c.pulsarHoldPosition=.9f;
        // Let the normal algorithm-entry crossfade finish before measuring a pulsaret.
        for(int block=0;block<40;++block)engine->process(in.data(),out.data(),64,c);
        const auto first=engine->pulsarStatus();
        assert(std::abs(first[1]-first[0]-.5f)<.0001f); // reads a genuine 500 ms source span
        for(int block=0;block<120;++block){
            if(block==30)c.pulsarSize=.5f;
            if(block==60)c.pulsarSize=500;
            engine->process(in.data(),out.data(),64,c);
            const auto status=engine->pulsarStatus();assert(status[0]>=0 && status[1]<=1 && status[2]<=4);
            for(int f=0;f<64;++f){assert(std::isfinite(output[0][f]) && std::abs(output[0][f])<2);assert(std::abs(output[1][f]+.5f*output[0][f])<1e-5f);}
        }
        c.field=.02f;
        for(int block=0;block<400;++block)engine->process(in.data(),out.data(),64,c);
        const auto cropped=engine->pulsarStatus();assert(cropped[1]-cropped[0]<=.021f);
    }
    char folder[]="/tmp/thru-pulsar-500.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();
    setParam(t.get(),"algorithm","Pulsar");const auto& size=midiParameter("pulsar_size");
    assert(size.id==3330 && size.maximum==1000 && size.initial==4 && size.logarithmic);
    nrpn(*t,5,"pulsar_size",16383);assert(t->controls.pulsarSize==1000);
    nrpn(*t,5,"pulsar_size",0);assert(t->controls.pulsarSize==.5f);
    setParam(t.get(),"midi_control_mode","E16 + MNLT");
    midiCc(*t,14,127,19);assert(t->controls.pulsarSize==4);
    midiCc(*t,14,18,127);assert(t->controls.pulsarSize>4 && t->controls.pulsarSize<4.08f);
    setParam(t.get(),"pulsar_size","1000");t->captureActiveClip(true);assert(t->savePersistentState());t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(folder));assert(t->controls.pulsarSize==1000);
    t.reset();removeBackgroundTestDirectory(folder);
    std::puts("s3g Thru Pulsar genuine 500 ms spans: source rates, stereo, fixed readers, live/crop safety, MIDI, fine E16 and restart recall: PASS");
}

void testPulsarLongDuration() {
    for (float pulseMs : {500.f, 1000.f})
    for (float outputRate : {44100.f, 48000.f})
        for (float sourceRate : {22050.f, 44100.f, 48000.f, 96000.f})
            for (bool prepared : {false, true}) {
                const auto frames = static_cast<std::size_t>(sourceRate * 2);
                std::array<std::vector<float>, 4> pcm;
                std::array<std::array<float, 64>, 4> input{}, output{};
                std::array<const float*, 4> in{};
                std::array<float*, 4> out{};
                for (int lane = 0; lane < 4; ++lane) {
                    pcm[lane].resize(frames * 2);
                    for (std::size_t i = 0; i < frames; ++i) {
                        const float x = .3f * std::sin(6.28318530718 * 220 * i / sourceRate);
                        pcm[lane][i] = x;
                        pcm[lane][frames + i] = -.5f * x;
                    }
                    in[lane] = input[lane].data(); out[lane] = output[lane].data();
                }
                auto engine = std::make_unique<Engine>();
                assert(engine->prepare(outputRate, pcm[0].data(), pcm[1].data(),
                                       pcm[2].data(), pcm[3].data(), frames * 2));
                for (int lane = 0; lane < 4; ++lane)
                    assert(engine->adoptLane(lane, frames, sourceRate));
                s3g_daisy::ThruWavesetBank bank;
                if (prepared) {
                    bank.analyze(pcm[0].data(), pcm[0].data() + frames, frames, sourceRate);
                    engine->setPreparedWavesetBank(0, &bank);
                }
                Engine::Controls c;
                c.algorithm = Engine::PlaybackAlgorithm::pulsar;
                c.lanePath = Engine::LanePath::manual;
                c.pulsarRate = 500 / pulseMs; c.pulsarForm = 1000 / pulseMs; c.pulsarSize = pulseMs;
                c.pulsarSourceHold = 1; c.pulsarHoldPosition = .25f;
                int eventCount = 0;
                float previousEvent = 0;
                double sinceLaunch = 0, lateEnergy = 0, gapEnergy = 0;
                int lateCount = 0, gapCount = 0;
                for (int block = 0; block < static_cast<int>(outputRate * pulseMs * .0038 / 64); ++block) {
                    engine->process(in.data(), out.data(), 64, c);
                    const auto p = engine->pulsarStatus();
                    assert(p[2] <= 4);
                    if (p[5] != previousEvent) {
                        previousEvent = p[5]; sinceLaunch = 0; ++eventCount;
                        assert(std::abs(p[6] - pulseMs) < .001f && p[3] == 0);
                        assert(std::abs(p[1] - p[0] - pulseMs * .0005f) < .0001f);
                    }
                    for (int i = 0; i < 64; ++i) {
                        const float x = output[0][i];
                        assert(std::isfinite(x) && std::abs(x) < 1);
                        assert(std::abs(output[1][i] + .5f * x) < 1e-5f);
                        // Measure the second pulse, after algorithm-entry fades.
                        const double age = sinceLaunch + i / outputRate;
                        if (eventCount >= 2 && age >= pulseMs * .0007 && age < pulseMs * .00088) {
                            lateEnergy += x*x; ++lateCount;
                        }
                        if (eventCount >= 2 && age >= pulseMs * .0014 && age < pulseMs * .0016) {
                            gapEnergy += x*x; ++gapCount;
                        }
                    }
                    sinceLaunch += 64 / outputRate;
                }
                assert(lateCount > 100 && gapCount > 100);
                assert(std::sqrt(lateEnergy / lateCount) > .01);
                assert(std::sqrt(gapEnergy / gapCount) < .0001);
                // Higher clock speeds and jitter shorten new pulses; long
                // existing readers finish instead of being stolen.
                const auto previousPulse = engine->pulsarStatus()[5];
                for (int block = 0; block < static_cast<int>(outputRate * pulseMs * .002 / 64) + 2
                     && engine->pulsarStatus()[5] == previousPulse; ++block)
                    engine->process(in.data(), out.data(), 64, c);
                assert(engine->pulsarStatus()[5] != previousPulse);
                c.pulsarRate = 100; c.pulsarJitter = 1;
                for (int block = 0; block < 700; ++block) {
                    engine->process(in.data(), out.data(), 64, c);
                    assert(engine->pulsarStatus()[2] <= 4);
                    for (float x : output[0]) assert(std::isfinite(x) && std::abs(x) < 1);
                }
                const auto limited = engine->pulsarStatus();
                assert(limited[3] == 1 && limited[6] > 15.9 && limited[6] < 16.1);
                // Keyboard/form modulation cannot shrink FORM below 1 Hz.
                c.pulsarRate = 1; c.pulsarJitter = 0;
                c.notePitch = .5f; c.pulsarKeytrack = 1;
                for (int block = 0; block < static_cast<int>(outputRate * 5 / 64); ++block)
                    engine->process(in.data(), out.data(), 64, c);
                assert(std::abs(engine->pulsarStatus()[6] - 1000) < .001f);
            }
    char folder[] = "/tmp/thru-pulsar-duration.XXXXXX"; assert(mkdtemp(folder));
    auto t = std::make_unique<ThruInstance>(); assert(t->prepare(folder));
    t->stopBackgroundWork();
    const auto& form = midiParameter("pulsar_form");
    assert(form.id == 3329 && form.minimum == 1 && form.maximum == 2000 && form.initial == 500);
    nrpn(*t, 5, "pulsar_form", 0); assert(t->controls.pulsarForm == 1);
    setParam(t.get(), "pulsar_form", "0.5"); assert(t->controls.pulsarForm == 1);
    setParam(t.get(), "pulsar_form", "1");
    t->captureActiveClip(true); assert(t->savePersistentState()); t.reset();
    t = std::make_unique<ThruInstance>(); assert(t->prepare(folder));
    assert(t->controls.pulsarForm == 1);
    Engine::Controls c; c.algorithm = Engine::PlaybackAlgorithm::pulsar; c.pulsarForm = 1;
    ThruInstance::applyVoiceCharacter(c, .5f); assert(c.pulsarForm == 2);
    t.reset(); removeBackgroundTestDirectory(folder);
    std::puts("s3g Thru Pulsar audible 500/1000 ms windows, stereo/rates, safe overlap, low FORM tracking and saved/MIDI ranges: PASS");
}
