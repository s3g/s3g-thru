void testImprintEnvelopeAndCarrier() {
    using Imprint = s3g_daisy::ThruSpectralImprint;
    Imprint::Spectrum carrier{}, shaper{};
    carrier[6] = {40, 20, 440, .2f, .125f};
    carrier[36] = {40, 20, 3100, .1f, -.2f};
    shaper[6] = {.1f, .1f, 999, 0, 0};
    shaper[36] = {20, 20, 100, 0, 0};
    const auto original = carrier;
    Imprint::apply(carrier, shaper, 0);
    assert(carrier[6].left == original[6].left);
    Imprint::apply(carrier, shaper, 1);
    assert(carrier[36].left > carrier[6].left * 50);
    assert(carrier[6].hz == 440 && carrier[36].hz == 3100 && carrier[6].delta == .125f &&
           carrier[6].phase == .2f);
    assert(carrier[6].right == carrier[6].left * .5f);
    carrier = original;
    shaper = {};
    Imprint::apply(carrier, shaper, 1);
    assert(carrier[6].left == 0 && carrier[36].right == 0);
}
void testImprintPitchAndArticulation() {
    const auto make = [](bool pulse) {
        auto f = std::make_unique<SpectralFixture>();
        std::uint32_t rng = 57;
        for (std::size_t i = 0; i < SpectralFixture::frames; ++i) {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            const float noise = ((rng & 65535) / 32768.0f - 1) * .25f;
            const float value = !pulse || i < SpectralFixture::frames / 2 ? noise : 0;
            f->audio[1][i] = f->audio[1][SpectralFixture::frames + i] = value;
        }
        f->banks[1].analyze(f->audio[1].data(), f->audio[1].data() + SpectralFixture::frames,
                            SpectralFixture::frames, 44100, 2);
        return f;
    };
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::morph;
    c.morphMode = 1;
    c.morphAmplitude = 1;
    c.morphFrequency = 1;
    c.morphBlurMs = 0;
    c.lanePath = Engine::LanePath::manual;
    c.manualLane = 0;
    c.field = 1;
    c.locus = .5f;
    for (float pitch : {1.f, 2.f}) {
        auto f = make(false);
        c.notePitch = pitch;
        const auto audio = f->render(c);
        assert(std::abs(spectralFrequency(audio) - 440 * pitch) < 30);
    }
    c.notePitch = 1;
    auto f = make(true);
    const auto phrase = f->render(c, 32768);
    double active = 0, silent = 0;
    for (int i = 4096; i < 8192; ++i) {
        active += phrase[i] * phrase[i];
        silent += phrase[i + 20480] * phrase[i + 20480];
    }
    assert(active > .01 && silent < active * .03);
    auto missing = make(false);
    missing->engine.setPreparedSpectralBank(1, nullptr);
    const auto bypass = missing->render(c);
    assert(*std::max_element(bypass.begin(), bypass.end()) > .1);
    // Repeated live mode changes retain finite, continuous stereo output.
    float previous = 0, maxJump = 0;
    for (int block = 0; block < 80; ++block) {
        c.morphMode = (block / 8) % 2;
        const auto audio = missing->render(c, 128);
        for (float x : audio) {
            assert(std::isfinite(x));
            maxJump = std::max(maxJump, std::abs(x - previous));
            previous = x;
        }
    }
    assert(maxJump < .3);
}
void testImprintStateAndFormat20() {
    char directory[] = "/tmp/thru-imprint-state-XXXXXX";
    assert(mkdtemp(directory));
    auto t = std::make_unique<ThruInstance>();
    assert(t->prepare(directory));
    t->stopBackgroundWork();
    t->setPerformanceMode(3);
    for (int part = 0; part < 4; ++part) {
        t->selectSplitPart(part);
        setParam(t.get(), "algorithm", "Morph");
        setParam(t.get(), "morph_mode", part % 2 ? "Imprint" : "Morph");
    }
    t->captureActiveClip(true);
    assert(t->savePersistentState());
    auto restored = std::make_unique<ThruInstance>();
    assert(restored->prepare(directory));
    for (int part = 0; part < 4; ++part) {
        restored->selectSplitPart(part);
        assert(restored->controls.morphMode == part % 2);
    }
    restored.reset();
    const auto path = std::string(directory) + "/thru-clips.bin";
    auto *file = std::fopen(path.c_str(), "rb");
    assert(file);
    assert(std::fseek(file, 0, SEEK_END) == 0);
    const long size = std::ftell(file);
    std::rewind(file);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size) - 4);
    assert(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size());
    std::fclose(file);
    constexpr std::size_t extension = 8 + 64 * 5 * (kLockParameterCount-kVersion19LockParameterCount) * 4;
    assert(bytes.size() > extension);
    const auto at = bytes.size() - extension;
    std::vector<unsigned char> old(bytes.begin(), bytes.begin() + at);
    old[8] = 20;
    const auto append = [&](std::uint32_t n) {
        for (int shift = 0; shift < 32; shift += 8)
            old.push_back(static_cast<unsigned char>(n >> shift));
    };
    append(0x4e455731u);
    append(19);
    for (std::size_t group = 0; group < 64 * 5; ++group) {
        const auto offset = at + 8 + group * (kLockParameterCount-kVersion19LockParameterCount) * 4;
        old.insert(old.end(), bytes.begin() + offset, bytes.begin() + offset + 19 * 4);
    }
    std::uint32_t checksum = 2166136261u;
    for (auto value : old) {
        checksum ^= value;
        checksum *= 16777619u;
    }
    append(checksum);
    file = std::fopen(path.c_str(), "wb");
    assert(file);
    assert(std::fwrite(old.data(), 1, old.size(), file) == old.size());
    assert(std::fclose(file) == 0);
    assert(t->readClipFileFromPath(path.c_str()));
    for (const auto &clip : t->clipSlots) {
        assert(clip.baseValues[static_cast<int>(LockParameter::morphMode)] == 0);
        for (const auto &part : clip.splitValues)
            assert(part[static_cast<int>(LockParameter::morphMode)] == 0);
    }
    t.reset();
    removeBackgroundTestDirectory(directory);
    auto live = splitFixture();
    live->selectSplitPart(0);
    setParam(live.get(), "algorithm", "Morph");
    live->beginClipRecording();
    renderSplit(*live, 20);
    live->applyMidiCc(45, 127);
    renderSplit(*live, 20);
    live->finishClipRecording();
    assert(live->controls.morphMode == 1);
    bool found = false;
    const auto &clip = *live->activeClipSlot();
    for (int i = 0; i < clip.clipPointCount; ++i)
        found |= clip.clipPoints[i].parameter == static_cast<int>(LockParameter::morphMode);
    assert(found);
}

float dominantSpectralPitch(const std::vector<float> &audio, float expected) {
    double best = 0;
    float result = 0;
    for (int hz = static_cast<int>(expected * .5f); hz <= expected * 2; hz += 2) {
        const double coefficient = 2 * std::cos(6.283185307179586 * hz / 44100);
        double previous = 0, prior = 0;
        for (std::size_t i = audio.size() / 3; i < audio.size(); ++i) {
            const double q = audio[i] + coefficient * previous - prior;
            prior = previous;
            previous = q;
        }
        const double power = previous * previous + prior * prior - coefficient * previous * prior;
        if (power > best) {
            best = power;
            result = static_cast<float>(hz);
        }
    }
    return result;
}
void testSpectralSourceSampleRates() {
    for (float rate : {22050.f, 48000.f})
        for (auto algorithm :
             {Engine::PlaybackAlgorithm::prism, Engine::PlaybackAlgorithm::morph}) {
            auto f = std::make_unique<SpectralFixture>(15, rate);
            Engine::Controls c;
            c.algorithm = algorithm;
            c.prismMode = 1;
            c.lanePath = Engine::LanePath::manual;
            c.morphAmplitude = c.morphFrequency = 0;
            const auto root = f->render(c);
            assert(std::abs(dominantSpectralPitch(root, 440) - 440) < 12);
            c.notePitch = 2;
            f = std::make_unique<SpectralFixture>(15, rate);
            const auto upper = f->render(c);
            assert(std::abs(dominantSpectralPitch(upper, 880) - 880) < 12);
        }
}
