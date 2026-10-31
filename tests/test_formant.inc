std::unique_ptr<SpectralFixture> formantFixture(float rate = 44100) {
    auto f = std::make_unique<SpectralFixture>(15, rate);
    for (std::size_t lane = 0; lane < 4; ++lane) {
        for (std::size_t i = 0; i < SpectralFixture::frames; ++i) {
            float value = 0;
            for (int h = 1; h <= 35; ++h) {
                const float hz = h * 150.f;
                const float envelope = .01f / h + .025f * std::exp(-std::pow((hz - 900) / 240, 2)) +
                                       .02f * std::exp(-std::pow((hz - 2250) / 300, 2));
                value += envelope * std::cos(s3g_daisy::ThruSpectralPlan::tau * hz * i / rate);
            }
            f->audio[lane][i] = value;
            f->audio[lane][i + SpectralFixture::frames] = -.5f * value;
        }
        f->banks[lane].analyze(f->audio[lane].data(),
                               f->audio[lane].data() + SpectralFixture::frames,
                               SpectralFixture::frames, rate, lane + 1);
        f->engine.setPreparedSpectralBank(lane, &f->banks[lane]);
    }
    return f;
}
double formantBandEnergy(const std::vector<float> &audio, float lo, float hi) {
    using Plan = s3g_daisy::ThruSpectralPlan;
    std::array<Plan::Complex, Plan::size> work{};
    double energy = 0;
    for (std::size_t at = audio.size() / 3; at + Plan::size < audio.size(); at += Plan::size) {
        for (std::size_t j = 0; j < Plan::size; ++j)
            work[j] = {audio[at + j] * Plan::get().window[j], 0};
        Plan::get().fft(work);
        for (std::size_t k = 1; k < Plan::bins; ++k)
            if (k * 44100.f / Plan::size >= lo && k * 44100.f / Plan::size <= hi)
                energy += work[k].real * work[k].real + work[k].imag * work[k].imag;
    }
    return energy;
}
void testFormantPitchMarks() {
    for (float rate : {22050.f, 44100.f, 48000.f, 96000.f}) {
        auto f = formantFixture(rate);
        const auto &pitch = f->banks[0].pitch;
        const auto p = pitch.sample(rate * .3);
        assert(p.confidence > .9f);
        assert(std::abs(rate / p.period - 150) < 2);
        assert(pitch.marks.size() > SpectralFixture::frames / rate * 120);
        for (std::size_t i = 4; i + 4 < pitch.marks.size(); ++i) {
            const auto period = pitch.marks[i] - pitch.marks[i - 1];
            assert(period > rate / 160 && period < rate / 140);
        }
    }
    s3g_daisy::ThruPitchBank bank;
    std::vector<float> quiet(8192), right(8192);
    for (std::size_t i = 0; i < right.size(); ++i)
        right[i] = .3f * std::sin(6.283185307179586 * 220 * i / 44100);
    bank.analyze(quiet.data(), right.data(), right.size(), 44100);
    assert(bank.sample(4000).confidence > .9 &&
           std::abs(44100 / bank.sample(4000).period - 220) < 2);
    bank.analyze(quiet.data(), quiet.data(), quiet.size(), 44100);
    assert(bank.marks.empty() && bank.sample(4000).confidence == 0);
    bank.analyze(quiet.data(), quiet.data(), 1, 44100);
    assert(bank.marks.empty());
}
void testFormantKeyboardAndEnvelope() {
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::formant;
    c.lanePath = Engine::LanePath::manual;
    c.manualLane = 0;
    c.field = 1;
    c.locus = .2f;
    std::array<std::vector<float>, 3> recordings;
    for (int i = 0; i < 3; ++i) {
        auto f = formantFixture();
        c.notePitch = i == 1 ? 2 : 1;
        c.formantShift = i == 2 ? 12 : 0;
        recordings[i] = f->render(c, 24000);
        assert(formantBandEnergy(recordings[i], 100, 6000) > .1);
        // A common PSOLA clock retains the recorded stereo relationship.
        for (int j = 0; j < 1024; ++j) {
            f->engine.process(f->ins.data(), f->outs.data(), 1, c);
            assert(std::abs(f->out[1] + .5f * f->out[0]) < 1e-5f);
        }
    }
    // Pitch doubles the harmonic spacing while retaining the original formant
    // regions. Formant shift moves the 900 Hz region toward 1800 Hz.
    const auto low = formantBandEnergy(recordings[0], 650, 1150),
               high = formantBandEnergy(recordings[0], 1550, 2050);
    assert(formantBandEnergy(recordings[1], 650, 1150) >
           formantBandEnergy(recordings[1], 1550, 2050) * 1.3);
    const auto shiftedLow = formantBandEnergy(recordings[2], 650, 1150),
               shiftedHigh = formantBandEnergy(recordings[2], 1550, 2050);
    std::fprintf(stderr, "Formant envelope %.4f %.4f shifted %.4f %.4f\n", low, high, shiftedLow,
                 shiftedHigh);
    assert(shiftedHigh / std::max(1e-10, shiftedLow) > high / std::max(1e-10, low) * 3);
    // Periodicity is checked directly, independent of which vowel harmonic is strongest.
    const auto periodicity = [](const std::vector<float> &x, int lag) {
        double a = 0, b = 0;
        for (std::size_t i = x.size() / 3 + lag; i < x.size(); ++i) {
            a += double(x[i]) * x[i - lag];
            b += double(x[i]) * x[i];
        }
        return a / b;
    };
    assert(periodicity(recordings[0], 294) > .9);
    assert(periodicity(recordings[1], 147) > .9);
    assert(periodicity(recordings[1], 74) < .8);
    assert(periodicity(recordings[2], 294) > .9);
    for (float rate : {22050.f, 48000.f}) {
        auto f = formantFixture(rate);
        c.notePitch = 2;
        c.formantShift = 0;
        const auto x = f->render(c, 24000);
        assert(periodicity(x, 147) > .9);
    }
}
void testFormantLiveSafety() {
    auto f = formantFixture();
    Engine::Controls c;
    c.algorithm = Engine::PlaybackAlgorithm::formant;
    c.lanePath = Engine::LanePath::manual;
    c.manualLane = 0;
    c.field = .7f;
    c.locus = .5f;
    f->render(c, 4096);
    float previous = f->out[0], jump = 0, peak = 0;
    for (int block = 0; block < 400; ++block) {
        if (block % 9 == 0) {
            c.formantShift = (block % 5 - 2) * 12;
            c.formantPitch = (block % 3 - 1) * 12;
            c.manualLane = (block / 9) % 4;
            c.locus = .1f + block % 7 * .1f;
            c.field = block % 4 == 0 ? .02f : .6f;
        }
        if (block == 80)
            for (int lane = 0; lane < 4; ++lane)
                f->engine.setPreparedSpectralBank(lane, nullptr);
        if (block == 120)
            for (int lane = 0; lane < 4; ++lane)
                f->engine.setPreparedSpectralBank(lane, &f->banks[lane]);
        for (float value : f->render(c, 128)) {
            assert(std::isfinite(value));
            peak = std::max(peak, std::abs(value));
            jump = std::max(jump, std::abs(value - previous));
            previous = value;
        }
    }
    std::fprintf(stderr, "Formant live peak %.4f jump %.4f\n", peak, jump);
    assert(peak < .6f && jump < .12f);
}
void testFormantPersistenceAndFormat21() {
    char directory[] = "/tmp/thru-formant-state-XXXXXX";
    assert(mkdtemp(directory));
    auto t = std::make_unique<ThruInstance>();
    assert(t->prepare(directory));
    t->stopBackgroundWork();
    t->setPerformanceMode(3);
    for (int part = 0; part < 4; ++part) {
        t->selectSplitPart(part);
        setParam(t.get(), "algorithm", "Formant");
        setParam(t.get(), "formant_pitch", std::to_string(part * 3).c_str());
        setParam(t.get(), "formant_shift", std::to_string(-part * 2).c_str());
        setParam(t.get(), "formant_smoothing", "75");
    }
    t->captureActiveClip(true);
    assert(t->savePersistentState());
    auto restored = std::make_unique<ThruInstance>();
    assert(restored->prepare(directory));
    for (int part = 0; part < 4; ++part) {
        restored->selectSplitPart(part);
        assert(restored->controls.algorithm == Engine::PlaybackAlgorithm::formant);
        assert(restored->controls.formantPitch == part * 3 &&
               restored->controls.formantShift == -part * 2 &&
               restored->controls.formantSmoothingMs == 75);
    }
    restored.reset();
    // Construct an authentic pre-Formant bank: only existing algorithms and
    // a mix of Morph/Imprint modes, then remove the three appended fields.
    for (int part = 0; part < 4; ++part) {
        t->selectSplitPart(part);
        setParam(t.get(), "algorithm", "Morph");
        setParam(t.get(), "morph_mode", part % 2 ? "Imprint" : "Morph");
    }
    t->captureActiveClip(true);
    assert(t->savePersistentState());
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
    old[8] = 21;
    const auto append = [&](std::uint32_t n) {
        for (int b = 0; b < 4; ++b)
            old.push_back(static_cast<unsigned char>(n >> (b * 8)));
    };
    append(0x4e455731u);
    append(20);
    for (std::size_t group = 0; group < 64 * 5; ++group) {
        const auto offset = at + 8 + group * (kLockParameterCount-kVersion19LockParameterCount) * 4;
        old.insert(old.end(), bytes.begin() + offset, bytes.begin() + offset + 20 * 4);
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
    for (const auto &clip : t->clipSlots) {
        assert(clip.baseValues[196] == 0 && clip.baseValues[197] == 0 &&
               clip.baseValues[198] == 30);
        for (const auto &part : clip.splitValues)
            assert(part[196] == 0 && part[197] == 0 && part[198] == 30);
    }
    const auto &oldClip = *t->activeClipSlot();
    for (int part = 0; part < 4; ++part)
        assert(oldClip.splitValues[part][static_cast<int>(LockParameter::morphMode)] == part % 2);
    // The old JSON parameter arrays also end before Formant's appended IDs.
    std::array<char, kStateTextSize> json{};
    std::strcpy(json.data(), "{}");
    assert(t->appendSplitJson(json.data(), json.size(), 2) > 0);
    std::string text = json.data();
    auto atCount = text.find("\"split_params_count\":205");
    assert(atCount != std::string::npos);
    text.replace(atCount, std::strlen("\"split_params_count\":205"), "\"split_params_count\":196");
    for (const auto *name : {"split_single", "split_a", "split_b", "split_c", "split_d"}) {
        const auto begin = text.find(std::string("\"") + name + "\":[");
        assert(begin != std::string::npos);
        auto cut = text.find('[', begin) + 1;
        for (std::size_t field = 0; field < kVersion21LockParameterCount; ++field)
            cut = text.find(',', cut) + 1;
        auto after = cut;
        for (std::size_t field = 0; field < kLockParameterCount-kVersion21LockParameterCount; ++field)
            after = text.find(',', after) + 1;
        text.erase(cut, after - cut);
    }
    assert(t->restoreSplitJson(text.c_str()));
    for (int part = 0; part < 4; ++part) {
        t->selectSplitPart(part);
        assert(t->controls.morphMode == part % 2);
        assert(t->controls.formantPitch == 0 && t->controls.formantShift == 0 &&
               t->controls.formantSmoothingMs == 30);
    }
    t.reset();
    removeBackgroundTestDirectory(directory);
    auto live = splitFixture();
    setParam(live.get(), "algorithm", "Formant");
    live->beginClipRecording();
    renderSplit(*live, 20);
    live->applyMidiCc(40, 127);
    live->applyMidiCc(41, 0);
    live->applyMidiCc(42, 127);
    renderSplit(*live, 20);
    live->finishClipRecording();
    assert(live->controls.formantPitch == 24 && live->controls.formantShift == -24 &&
           live->controls.formantSmoothingMs == 200);
    std::array<bool, 3> found{};
    const auto &clip = *live->activeClipSlot();
    for (int i = 0; i < clip.clipPointCount; ++i) {
        const auto p = clip.clipPoints[i].parameter & 255u;
        if (p >= 196 && p <= 198)
            found[p - 196] = true;
    }
    assert(found[0] && found[1] && found[2]);
}
