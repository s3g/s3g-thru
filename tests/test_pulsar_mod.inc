// Modulation must alter already-held notes, keep stereo/clock identity, and
// preserve prior programs when every new depth is zero.
void testPulsarModulationSound() {
    using P = s3g_daisy::ThruPulsar;
    for (int i = 0; i <= 1000; ++i) {
        const double phase = i / 1000.;
        assert(std::abs(P::modulation(phase) - std::sin(6.283185307179586 * phase)) < .0001);
    }
    for (float semi : {-24.f, -12.f, 0.f, 12.f, 24.f})
        assert(std::abs(P::emissionRatio(semi) - std::exp2(semi / 12)) < .00001f);
    for (bool prepared : {false, true})
        for (float rate : {.5f, 160.f, 2000.f}) {
            auto a = std::make_unique<StochasticFixture>(44100, prepared, false, true, 1),
                 b = std::make_unique<StochasticFixture>(44100, prepared, false, true, 1);
            Engine::Controls c;
            c.algorithm = Engine::PlaybackAlgorithm::pulsar;
            c.pulsarRate = rate;
            c.pulsarSourceHold = 1;
            c.pulsarHoldPosition = .23456f;
            c.pulsarJitter = .3f;
            c.pulsarOmit = .2f;
            c.pulsarBurst = 1;
            auto d = c;
            d.pulsarEmitModRate = 20;
            d.pulsarFormModRate = .01f;
            for (int block = 0; block < 240; ++block) {
                a->render(c);
                b->render(d);
                assert(a->output == b->output);
            }
        }
    {
        auto f = std::make_unique<StochasticFixture>();
        Engine::Controls c;
        c.algorithm = Engine::PlaybackAlgorithm::pulsar;
        c.pulsarSourceHold = 1;
        c.pulsarRate = 100;
        c.pulsarEmitModRate = 2;
        f->render(c, 120);
        c.pulsarEmitModDepth = 12;
        float low = 10000, high = 0;
        for (int b = 0; b < 1000; ++b) {
            f->render(c);
            const auto p = f->engine.pulsarStatus();
            low = std::min(low, p[4]);
            high = std::max(high, p[4]);
            assert(p[2] <= 4);
        }
        assert(low < 65 && high > 160);
        c.pulsarEmitModDepth = 0;
        f->render(c, 500);
        assert(std::abs(f->engine.pulsarStatus()[4] - 100) < .01f);
    }
    // Each source/timbre control has an audible independent effect. All edits
    // happen after the note starts; no retrigger is used in this comparison.
    for (int control = 0; control < 6; ++control) {
        auto a = std::make_unique<StochasticFixture>(44100, true, false, true, 1),
             b = std::make_unique<StochasticFixture>(44100, true, false, true, 1);
        Engine::Controls c;
        c.algorithm = Engine::PlaybackAlgorithm::pulsar;
        c.pulsarSourceHold = 1;
        c.pulsarHoldPosition = .23456f;
        c.locus = .5f;
        c.field = 1;
        c.lanePath = Engine::LanePath::manual;
        c.pulsarRate = 100;
        c.notePitch = 2;
        a->render(c, 100);
        b->render(c, 100);
        auto d = c;
        if (control == 0) {
            d.pulsarFormModRate = 2;
            d.pulsarFormModDepth = 12;
        }
        if (control == 1)
            d.pulsarSpray = 1;
        if (control == 2)
            d.pulsarFormVariation = 24;
        if (control == 3)
            d.pulsarReverse = 1;
        if (control == 4)
            d.pulsarKeytrack = 1;
        if (control == 5) {
            d.pulsarEmitModRate = 20;
            d.pulsarEmitModDepth = 24;
            d.pulsarFormModRate = 20;
            d.pulsarFormModDepth = 24;
            d.pulsarSpray = d.pulsarReverse = d.pulsarKeytrack = 1;
            d.pulsarFormVariation = 24;
        }
        double difference = 0;
        float first = 1, last = 0;
        for (int block = 0; block < 700; ++block) {
            a->render(c);
            b->render(d);
            for (int i = 0; i < 64; ++i) {
                const float x = b->output[0][i];
                assert(std::isfinite(x) && std::abs(x) < 1.5f &&
                       std::abs(x - b->output[1][i]) < 1e-6f);
                const float delta = x - a->output[0][i];
                difference += delta * delta;
            }
            const auto p = b->engine.pulsarStatus();
            assert(p[0] >= 0 && p[1] <= 1 && p[0] <= p[1] && p[2] <= 4);
            first = std::min(first, p[0]);
            last = std::max(last, p[0]);
            assert(std::abs(b->engine.normalizedPosition() - .23456f) < .001f);
            if (control != 5)
                assert(std::abs(p[4] - 200) < .001f);
        }
        std::fprintf(stderr, "Pulsar modulation control=%d audio difference=%g\n", control,
                     difference);
        assert(difference > .1);
        if (control == 1)
            assert(last - first > .05f);
    }
    std::puts("s3g Thru Pulsar neutral modulation, live clock/timbre changes, stereo and "
              "held-anchor scatter: PASS");
}
void testPulsarModulationPersistence() {
    char dir[] = "/tmp/thru-pulsar-mod-bank-XXXXXX";
    assert(mkdtemp(dir));
    auto t = std::make_unique<ThruInstance>();
    assert(t->prepare(dir));
    t->stopBackgroundWork();
    t->setPerformanceMode(3);
    for (int p = 0; p < 4; ++p) {
        t->selectSplitPart(p, false);
        setParam(t.get(), "algorithm", "Pulsar");
        setParam(t.get(), "pulsar_emit_mod_depth", std::to_string(3 + p).c_str());
        setParam(t.get(), "pulsar_form_mod_depth", "12");
        setParam(t.get(), "pulsar_reverse", "40");
        setParam(t.get(), "pulsar_spray", "20");
        setParam(t.get(), "pulsar_form_variation", "7");
        setParam(t.get(), "pulsar_keytrack", "80");
        setParam(t.get(), "pulsar_emit_mod_rate", "0.23");
        setParam(t.get(), "pulsar_form_mod_rate", "2.5");
        setListSlot(*t, 0, 14);
    }
    t->captureActiveClip(true);
    t->midiPart = 0;
    t->noteOn(60, 100);
    t->beginClipRecording();
    renderSplit(*t, 20);
    const int focus = t->splitFocus;
    nrpn(*t, 5, "pulsar_form_variation",
         s3g_thru_midi::encode(midiParameter("pulsar_form_variation"), 17, 24));
    assert(t->splitFocus == focus);
    renderSplit(*t, 20);
    t->finishClipRecording();
    assert(t->savePersistentState());
    bool automation = false;
    for (std::size_t i = 0; i < t->activeClipSlot()->clipPointCount; ++i)
        automation |= t->activeClipSlot()->clipPoints[i].parameter ==
                      static_cast<int>(LockParameter::pulsarFormVariation);
    assert(automation);
    t->e16Page = 3;
    int part = -1;
    const auto *param = e16Parameter(*t, 0, part);
    assert(param && param->id == 3338 && part == 0);
    param = e16Parameter(*t, 15, part);
    assert(param && param->id == 3345 && part == 1);
    std::array<char, kStateTextSize> json{};
    assert(t->serializeState(json.data(), json.size()) > 0);
    auto copy = std::make_unique<ThruInstance>();
    assert(copy->prepare(dir));
    copy->stopBackgroundWork();
    assert(copy->restoreState(json.data()));
    for (int p = 0; p < 4; ++p) {
        copy->selectSplitPart(p, false);
        const auto &c = copy->controls;
        assert(c.pulsarEmitModDepth == 3 + p && c.pulsarFormModDepth == 12 &&
               std::abs(c.pulsarReverse - .4f) < .001f &&
               std::abs(c.pulsarSpray - .2f) < .001f &&
               std::abs(c.pulsarKeytrack - .8f) < .001f &&
               std::abs(c.pulsarEmitModRate - .23f) < .001f && c.pulsarFormModRate == 2.5f);
    }
    assert(copy->activeClipSlot()
               ->splitValues[3][static_cast<int>(LockParameter::pulsarFormVariation)] == 7);
    // A real format-29 PUL1 stores ten floats per group, not the new eighteen.
    // Preserve existing notes and old settings while removing new automation.
    for (auto &clip : t->clipSlots)
        clip.clipPointCount = 0;
    t->controls.pulsarRate = 123;
    t->controls.pulsarForm = 777;
    assert(t->writeClipFile());
    const auto path = std::string(dir) + "/thru-clips.bin";
    auto *file = std::fopen(path.c_str(), "rb");
    assert(file);
    std::fseek(file, 0, SEEK_END);
    const auto length = std::ftell(file);
    std::rewind(file);
    std::vector<unsigned char> bytes(length - 4);
    assert(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size());
    std::fclose(file);
    stripPriorityExtension(bytes);
    constexpr std::size_t stride = 18 * 4;
    const auto start = bytes.size() - (4 + 64 * 5 * stride);
    assert(bytes[start] == 0x31 && bytes[start + 1] == 0x4c && bytes[start + 2] == 0x55 &&
           bytes[start + 3] == 0x50);
    std::vector<unsigned char> old(bytes.begin(), bytes.begin() + start + 4);
    for (int group = 0; group < 64 * 5; ++group)
        old.insert(old.end(), bytes.begin() + start + 4 + group * stride,
                   bytes.begin() + start + 4 + group * stride + 10 * 4);
    old[8] = 29;
    std::uint32_t hash = 2166136261u;
    for (auto x : old) {
        hash ^= x;
        hash *= 16777619u;
    }
    for (int b = 0; b < 4; ++b)
        old.push_back(hash >> (8 * b));
    file = std::fopen(path.c_str(), "wb");
    assert(file);
    assert(std::fwrite(old.data(), 1, old.size(), file) == old.size());
    assert(std::fclose(file) == 0);
    assert(copy->readClipFileFromPath(path.c_str()));
    for (const auto &clip : copy->clipSlots)
        for (int group = 0; group < 5; ++group) {
            const auto &v = group ? clip.splitValues[group - 1] : clip.baseValues;
            for (auto key :
                 {LockParameter::pulsarEmitModDepth, LockParameter::pulsarFormModDepth,
                  LockParameter::pulsarSpray, LockParameter::pulsarFormVariation,
                  LockParameter::pulsarReverse, LockParameter::pulsarKeytrack})
                assert(v[static_cast<int>(key)] == 0);
            assert(std::abs(v[static_cast<int>(LockParameter::pulsarEmitModRate)] - .13f) <
                       .001f &&
                   std::abs(v[static_cast<int>(LockParameter::pulsarFormModRate)] - 1.7f) <
                       .001f);
        }
    assert(
        copy->activeClipSlot()->splitValues[3][static_cast<int>(LockParameter::pulsarRate)] ==
        123);
    assert(
        copy->activeClipSlot()->splitValues[3][static_cast<int>(LockParameter::pulsarForm)] ==
        777);
    copy.reset();
    t.reset();
    removeBackgroundTestDirectory(dir);
    std::puts("s3g Thru Pulsar MOD per-part state, automation, E16 ENG 2 and neutral format-29 "
              "import: PASS");
}
