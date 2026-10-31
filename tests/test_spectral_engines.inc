// Real FFT analysis, note renderers, module ownership and v19 migration.
struct SpectralFixture {
    static constexpr std::size_t frames = 32768;
    Engine engine;
    std::array<std::vector<float>, 4> audio;
    std::array<s3g_daisy::ThruSpectralBank, 4> banks;
    std::array<float, 4> in{}, out{};
    std::array<const float *, 4> ins{&in[0], &in[1], &in[2], &in[3]};
    std::array<float *, 4> outs{&out[0], &out[1], &out[2], &out[3]};
    explicit SpectralFixture(int mask = 15,float sourceRate=44100) {
        for (std::size_t lane = 0; lane < 4; ++lane) {
            audio[lane].resize(frames * 2);
            for (std::size_t i = 0; i < frames; ++i) {
                const float s = .4f * std::sin(s3g_daisy::ThruSpectralPlan::tau *
                                               (440 + lane * 40) * i / sourceRate);
                audio[lane][i] = s;
                audio[lane][frames + i] = s * .5f;
            }
        }
        assert(engine.prepare(44100, audio[0].data(), audio[1].data(), audio[2].data(),
                              audio[3].data(), frames * 2));
        for (std::size_t lane = 0; lane < 4; ++lane) {
            if (mask & (1 << lane))
                assert(engine.adoptLane(lane, frames, sourceRate));
            banks[lane].analyze(audio[lane].data(), audio[lane].data() + frames, frames, sourceRate,
                                lane + 1);
            engine.setPreparedSpectralBank(lane, &banks[lane]);
        }
    }
    std::vector<float> render(Engine::Controls c, std::size_t count = 16384) {
        std::vector<float> result;
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            engine.process(ins.data(), outs.data(), 1, c);
            assert(std::isfinite(out[0]) && std::isfinite(out[1]));
            result.push_back(out[0]);
        }
        return result;
    }
};
float spectralFrequency(const std::vector<float> &audio) {
    int crossings = 0;
    const std::size_t start = audio.size() / 3;
    for (std::size_t i = start + 1; i < audio.size(); ++i)
        if (audio[i - 1] <= 0 && audio[i] > 0)
            ++crossings;
    return crossings * 44100.0f / (audio.size() - start);
}
void testSpectralFFTAndKeyboard() {
    using Plan = s3g_daisy::ThruSpectralPlan;
    const auto &plan = Plan::get();
    std::array<Plan::Complex, Plan::size> x{};
    for (std::size_t i = 0; i < x.size(); ++i)
        x[i].real = std::sin(Plan::tau * 7 * i / x.size());
    plan.fft(x);
    assert(std::abs(std::hypot(x[7].real, x[7].imag) - 256) < .01f);
    plan.fft(x, true);
    for (std::size_t i = 0; i < x.size(); ++i)
        assert(std::abs(x[i].real - std::sin(Plan::tau * 7 * i / x.size())) < .0001f);
    auto stereo = std::make_unique<SpectralFixture>();
    for (std::size_t i = 0; i < SpectralFixture::frames; ++i)
        stereo->audio[0][SpectralFixture::frames + i] = -.5f * stereo->audio[0][i];
    stereo->banks[0].analyze(stereo->audio[0].data(),
                             stereo->audio[0].data() + SpectralFixture::frames,
                             SpectralFixture::frames, 44100, 1);
    Engine::Controls sc;
    sc.algorithm = Engine::PlaybackAlgorithm::prism;
    sc.prismMode = 1;
    sc.lanePath = Engine::LanePath::manual;
    stereo->render(sc, 4096);
    for (int i = 0; i < 4096; ++i) {
        stereo->engine.process(stereo->ins.data(), stereo->outs.data(), 1, sc);
        assert(std::abs(stereo->out[1] + .5f * stereo->out[0]) < .001f);
    }
    auto f = std::make_unique<SpectralFixture>();
    const auto analyzed = f->banks[0].sample(.4f, 5);
    assert(std::abs(analyzed.hz - 440) < 2 && analyzed.left > 30);
    for (auto algorithm : {Engine::PlaybackAlgorithm::prism, Engine::PlaybackAlgorithm::morph,
                           Engine::PlaybackAlgorithm::texture}) {
        Engine::Controls c;
        c.algorithm = algorithm;
        c.manualLane = 0;
        c.lanePath = Engine::LanePath::manual;
        c.laneCycles = 0;
        c.locus = .2f;
        c.field = .6f;
        c.notePitch = 1;
        c.textureRange = 0;
        c.textureRate = 20;
        c.textureLengthMs = 250;
        c.textureScatter = 0;
        c.morphAmplitude = 0;
        c.morphFrequency = 0;
        c.prismMode = 1;
        f = std::make_unique<SpectralFixture>();
        const auto root = f->render(c);
        const float hz = spectralFrequency(root);
        if (std::abs(hz - 440) > 15)
            std::fprintf(stderr, "New engine %d root pitch %.2f\n", static_cast<int>(algorithm),
                         hz);
        assert(std::abs(hz - 440) < 15);
        assert(*std::max_element(root.begin(), root.end()) > .02f);
        c.notePitch = 2;
        f = std::make_unique<SpectralFixture>();
        const auto upper = f->render(c);
        assert(std::abs(spectralFrequency(upper) - 880) < 30);
        double l = 0, r = 0;
        for (int i = 0; i < 8192; ++i) {
            f->engine.process(f->ins.data(), f->outs.data(), 1, c);
            l += f->out[0] * f->out[0];
            r += f->out[1] * f->out[1];
        }
        assert(r > l * .1 && r < l * .4);
    }
}
void testMorphQuadAndEmptyPartner() {
    for (int part = 0; part < 4; ++part) {
        auto f = std::make_unique<SpectralFixture>(1 << part);
        Engine::Controls c;
        c.algorithm = Engine::PlaybackAlgorithm::morph;
        c.stackFirstLane = c.stackLastLane = part;
        c.manualLane = part;
        c.laneCycles = 0;
        c.morphAmplitude = c.morphFrequency = 1;
        auto audio = f->render(c);
        const float expected = 440 + (part + 1) % 4 * 40;
        assert(std::abs(spectralFrequency(audio) - expected) < 15);
        // The partner remains readable even though the raw note engine owns one lane.
        assert(f->engine.loadedLaneMask() == static_cast<std::uint32_t>(1 << part));
        f->engine.setPreparedSpectralBank((part + 1) % 4, nullptr);
        audio = f->render(c);
        assert(std::abs(spectralFrequency(audio) - (440 + part * 40)) < 15);
    }
    auto f = std::make_unique<SpectralFixture>(12);
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::morph;
    c.stackFirstLane = 2;
    c.stackLastLane = 3;
    c.manualLane = 3;
    c.lanePath = Engine::LanePath::manual;
    c.laneCycles = 0;
    c.morphAmplitude = c.morphFrequency = 1;
    assert(std::abs(spectralFrequency(f->render(c)) - 520) < 15);
}
void testNewEngineEditsAndTextureSeed() {
    for (auto alg : {Engine::PlaybackAlgorithm::prism, Engine::PlaybackAlgorithm::morph,
                     Engine::PlaybackAlgorithm::texture}) {
        auto f = std::make_unique<SpectralFixture>();
        Engine::Controls c;
        c.algorithm = alg;
        c.field = .7f;
        c.laneCycles = .6f;
        c.prismSmear = 1;
        c.textureRate = 24;
        c.textureLengthMs = 2000;
        auto prior = f->render(c, 8192);
        float previous = prior.back(), maximum = 0;
        for (int block = 0; block < 70; ++block) {
            c.prismMode = block / 14;
            c.prismPitch = (block % 7) - 3;
            c.prismTilt = std::sin(block * .2f);
            c.morphAmplitude = (block % 11) * .1f;
            c.morphFrequency = 1 - c.morphAmplitude;
            c.morphOffset = std::sin(block * .03f);
            c.textureSeed = block / 10 + 1;
            const auto audio = f->render(c, 128);
            for (float x : audio) {
                maximum = std::max(maximum, std::abs(x - previous));
                previous = x;
                assert(std::isfinite(x) && std::abs(x) < 2);
            }
        }
        assert(maximum < .35f);
    }
    auto a = std::make_unique<SpectralFixture>(), b = std::make_unique<SpectralFixture>();
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::texture;
    c.textureMode = 2;
    c.textureRate = 24;
    c.textureSeed = 77;
    c.textureScaleMask = 0x295;
    assert(a->render(c) == b->render(c));
}
void testNewEngineParametersAndBanks() {
    auto t = splitFixture();
    t->setPerformanceMode(3);
    for (int part = 0; part < 4; ++part) {
        t->selectSplitPart(part);
        setParam(t.get(), "algorithm", part == 3 ? "Morph" : part == 2 ? "Texture" : "Prism");
        setParam(t.get(), "prism_pitch", std::to_string(part - 2).c_str());
        setParam(t.get(), "morph_amplitude", std::to_string(part * 25).c_str());
        setParam(t.get(), "texture_seed", std::to_string(90 + part).c_str());
    }
    for (int part = 0; part < 4; ++part) {
        t->selectSplitPart(part);
        assert(t->controls.prismPitch == part - 2);
        assert(t->controls.textureSeed == 90 + part);
    }
    t->selectSplitPart(2);
    t->applyMidiCc(40, 127);
    assert(t->controls.textureMode == 3);
    t->applyMidiCc(28, 127);
    assert(t->controls.textureSpread == 1);
    t->applyMidiCc(29, 0);
    assert(t->controls.textureSeed == 1);
    t->controls.textureSeed = 92;
    t->beginClipRecording();
    renderSplit(*t, 20);
    t->selectSplitPart(2);
    t->applyMidiCc(29, 0);
    renderSplit(*t, 20);
    t->controls.textureSeed = 92;
    t->selectSplitPart(3);
    t->applyMidiCc(40, 127);
    renderSplit(*t, 20);
    t->finishClipRecording();
    bool seed = false, amplitude = false;
    const auto &clip = *t->activeClipSlot();
    for (std::size_t i = 0; i < clip.clipPointCount; ++i) {
        seed |= clip.clipPoints[i].parameter == static_cast<int>(LockParameter::textureSeed);
        amplitude |=
            clip.clipPoints[i].parameter == static_cast<int>(LockParameter::morphAmplitude);
    }
    assert(seed && amplitude);
    t->refreshPlaybackVoices();
    for (std::size_t lane = 0; lane < 4; ++lane) {
        auto bank = std::make_unique<s3g_daisy::ThruSpectralBank>();
        bank->analyze(t->engine.laneData(lane, 0), t->engine.laneData(lane, 1),
                      t->engine.laneFrames(lane), t->sampleRate, t->wavesetSourceRevisions[lane]);
        t->preparedSpectralBanks[lane] = std::move(bank);
    }
    t->spectralWorker.exchangePreparedBanks(t->preparedSpectralBanks);
    t->serviceSpectralAnalysis();
    assert(t->spectralReadyMask.load() == 15);
    // A source revision invalidates only that lane and stale results never publish.
    ++t->wavesetSourceRevisions[2];
    t->serviceSpectralAnalysis();
    assert((t->spectralReadyMask.load() & 4) == 0);
    char pair[32]{};
    getParamValue(t.get(), "morph_pair", pair, sizeof(pair));
    assert(std::strcmp(pair, "4>1") == 0);
    char directory[] = "/tmp/thru-new-bank-XXXXXX";
    assert(mkdtemp(directory));
    t->stopBackgroundWork();
    std::snprintf(t->moduleDirectory.data(), t->moduleDirectory.size(), "%s", directory);
    t->persistenceEnabled = true;
    assert(mkdir((std::string(directory) + "/captures").c_str(), 0700) == 0);
    t->captureActiveClip(true);
    assert(t->savePersistentState());
    auto other = std::make_unique<ThruInstance>();
    assert(other->prepare(directory));
    for (int part = 0; part < 4; ++part) {
        other->selectSplitPart(part);
        assert(other->controls.prismPitch == part - 2);
        assert(other->controls.textureSeed == 90 + part);
    }
    other.reset();
    t.reset();
    removeBackgroundTestDirectory(directory);
}

void testSpectralSnapshotWorker() {
    using Worker = s3g_daisy::ThruSpectralWorker;
    Worker worker;
    assert(worker.prepare(4096));
    std::array<float, 4096> left{}, right{};
    for (std::size_t i = 0; i < left.size(); ++i)
        left[i] = right[i] = .2f * std::sin(s3g_daisy::ThruSpectralPlan::tau * 440 * i / 44100);
    worker.begin(2, 7, left.size(), 44100);
    for (int i = 0; i < 64; ++i)
        assert(worker.copy(left.data(), right.data(), 64) == 64);
    left.fill(0);
    right.fill(0); // Analysis owns its snapshot, not this mutable source.
    for (int i = 0; i < 4000 && worker.state() != Worker::complete; ++i)
        usleep(500);
    assert(worker.state() == Worker::complete && worker.accept(true));
    const auto *bank = worker.bank(2);
    assert(bank && bank->revision == 7 && bank->sample(.5, 5).left > 10);
    worker.releaseResult();
    worker.begin(2, 8, left.size(), 44100);
    assert(worker.copy(left.data(), right.data(), left.size()) == left.size());
    for (int i = 0; i < 4000 && worker.state() != Worker::complete; ++i)
        usleep(500);
    assert(worker.state() == Worker::complete && !worker.accept(false));
    assert(worker.bank(2) == bank);
    worker.releaseResult();
    worker.stop();
}
void testSpectralPatternSources() {
    for (auto algorithm : {Engine::PlaybackAlgorithm::prism, Engine::PlaybackAlgorithm::morph, Engine::PlaybackAlgorithm::formant}) {
        auto t = splitFixture();
        t->setPerformanceMode(0);
        t->controls.algorithm = algorithm;
        t->currentLaneSourceKinds.fill(LaneSourceKind::capture);
        t->captureActiveClip(true);
        t->copyCurrentSourcesToClip(t->clipSlots[0]);
        t->clipSlots[1] = t->clipSlots[0];
        assert(t->prepareClipBank(1) && t->preparedSpectralBanks[0]);
        t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
        assert(t->activatePreparedClip());
        const auto *source = t->spectralWorker.bank(0);
        assert(source && source->revision == t->wavesetSourceRevisions[0]);
        t->padNoteOn(68, 100);
        renderSplit(*t, 30);
        assert(t->prepareClipBank(0));
        t->clipPrepareState.store(static_cast<int>(ClipPrepareState::ready));
        t->patternFadeMs = 200;
        assert(t->activatePreparedClip() && t->patternShadow->spectralWorker.bank(0) == source &&
               t->spectralWorker.bank(0) != source);
        t->padNoteOn(68, 100);
        std::array<float, kBlockFrames * 2> audio{};
        t->render(nullptr, kBlockFrames, audio.data());
        float peak = 0;
        for (int i = 0; i < 120 && t->patternActive; ++i) {
            waitForPatternAudio(*t);
            t->render(nullptr, kBlockFrames, audio.data());
            for (float x : audio) {
                assert(std::isfinite(x));
                peak = std::max(peak, std::abs(x));
            }
        }
        assert(peak > .001 && !t->patternActive);
        waitForPatternIdle(*t);
    }
}

void testSpectralReleaseAndDecay() {
    using Voice = s3g_daisy::ThruSpectralVoice;
    Voice::Spectrum tone{};
    tone[5] = {48, 24, 440, 0, 0};
    for (bool accumulate : {false, true}) {
        Voice voice;
        voice.reset();
        for (int i = 0; i < 8192; ++i) {
            if (voice.needsFrame())
                voice.synthesize(tone, 44100, 80, 100, accumulate, 0);
            voice.next();
        }
        Voice::Spectrum silent{};
        std::vector<float> fade;
        fade.reserve(8192);
        for (int i = 0; i < 8192; ++i) {
            if (voice.needsFrame())
                voice.synthesize(silent, 44100, 80, 100, accumulate, 0);
            fade.push_back(voice.next()[0]);
        }
        assert(std::abs(spectralFrequency(fade) - 440) < 12);
        if (accumulate) {
            double early = 0, late = 0;
            for (int i = 512; i < 1024; ++i) {
                early += fade[i] * fade[i];
                late += fade[i + 4410] * fade[i + 4410];
            }
            const double ratio = std::sqrt(late / early);
            assert(ratio > .25 && ratio < .5);
        }
    }
}

void testAllEnginesAfterSpectralSwitching() {
    for(int mode=0;mode<4;++mode) {
        auto t=splitFixture();t->setPerformanceMode(mode);
        for(std::size_t lane=0;lane<4;++lane){auto bank=std::make_unique<s3g_daisy::ThruSpectralBank>();bank->analyze(t->engine.laneData(lane,0),t->engine.laneData(lane,1),t->engine.laneFrames(lane),t->sampleRate,t->wavesetSourceRevisions[lane]);t->preparedSpectralBanks[lane]=std::move(bank);}
        t->spectralWorker.exchangePreparedBanks(t->preparedSpectralBanks);
        for(int part=0;part<t->partCount();++part)for(int n=0;n<t->voicesPerPart();++n)t->padNoteOn(68+t->padForPart(part,n),100);
        for(int algorithm:{4,9,4,10,4,11,4,12,4,0,1,2,3,5,6,7,8}) {
            for(int part=0;part<t->partCount();++part){t->selectSplitPart(part);setParam(t.get(),"algorithm",std::to_string(algorithm).c_str());}
            std::array<float,kBlockFrames*2> audio{};float peak=0;
            for(int block=0;block<64;++block){t->render(nullptr,kBlockFrames,audio.data());if(block>16)for(float x:audio){assert(std::isfinite(x));peak=std::max(peak,std::abs(x));}}
            assert(peak>.0001f&&t->engine.loadedLaneMask()==15);
        }
    }
}
