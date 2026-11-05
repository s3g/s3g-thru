// The listener must restrain deformation without following gain/effects,
// sharing capture between unrelated parts, or changing keyboard/source/scan
// controls.
void testStochasticListenerMath() {
    using namespace s3g_daisy;
    ThruStochasticListener l;
    for (int i = 0; i < 100; ++i)
        l.update(false, 1, 1, 64, 48000);
    assert(l.effective() == 0 && l.mutation == 1 && l.evolution == 1 && l.retention == 1);
    for (int i = 0; i < 105; ++i)
        l.update(true, 1, 1, 64, 48000);
    assert(l.effective() > .62f && l.effective() < .64f);
    for (int i = 0; i < 1000; ++i)
        l.update(true, 1, 1, 64, 48000);
    assert(l.effective() > .999f && l.mutation >= .04f && l.evolution >= .03f);
    assert(l.mutation < .05f && l.evolution < .031f && l.retention < .14f);
    const float before = l.capture;
    for (int i = 0; i < 1650; ++i)
        l.update(true, 1, 0, 64, 48000);
    assert(std::abs(l.capture / before - std::exp(-1.f)) < .002f);
    for (int i = 0; i < 1000; ++i)
        l.update(false, 1, 1, 64, 48000);
    assert(l.effective() == 0 && l.mutation == 1 && l.evolution == 1);
    ThruStochasticPartMeter m;
    m.observe(.02f * 64, 64, 48000);
    assert(m.target(std::sqrt(.02f)) > .9f && m.target(0) == 0);
    assert(m.target(.01f) < m.target(.1f));
    m.observe(0, 64, 48000);
    assert(m.target(.1f) == 0);
    ThruStochasticWalk a, b;
    a.reset(3872);
    b.reset(3872);
    double freeTravel = 0, heldTravel = 0;
    for (int step = 0; step < 2000; ++step) {
        const auto ap = a.ampHistory, bp = b.ampHistory;
        a.evolve(.9f, 0, 2);
        b.evolve(.9f, 0, 2, .04f, .14f);
        for (int i = 0; i < ThruStochasticWalk::segments; ++i) {
            freeTravel += std::abs(a.ampHistory[i] - ap[i]);
            heldTravel += std::abs(b.ampHistory[i] - bp[i]);
        }
    }
    assert(heldTravel < freeTravel * .1 && heldTravel > 0);
    // CAPT=0 must be WIDE with zero attraction, and changing capture affects
    // evolution, not the PCM output gain. Test at both free and cycle clocks.
    for (float evolution : {120.f, 1024.f}) {
        auto f = std::make_unique<StochasticFixture>(),
             neutral = std::make_unique<StochasticFixture>();
        Engine::Controls c;
        c.algorithm = Engine::PlaybackAlgorithm::stochastic;
        c.stochasticEvolution = evolution;
        c.stochasticLaw = 3;
        c.stochasticPull = 1;
        c.stochasticCapture = 0;
        c.stochasticListenTarget = 1;
        c.stochasticSourceHold = 1;
        for (int block = 0; block < 100; ++block) {
            f->render(c);
            auto d = c;
            d.stochasticLaw = 2;
            d.stochasticPull = 0;
            neutral->render(d);
            assert(f->output == neutral->output);
        }
        c.stochasticCapture = 1;
        f->render(c, 1000);
        assert(f->engine.stochasticListenerCapture() > .999f);
        c.stochasticAmplitude = c.stochasticTiming = c.stochasticDrift = 0;
        f->render(c, 500);
        neutral->engine = f->engine;
        auto d = c;
        d.stochasticLaw = 2;
        d.stochasticPull = 0;
        for (int block = 0; block < 100; ++block) {
            f->render(c);
            neutral->render(d);
            assert(f->output == neutral->output);
        }
    }
    std::puts("s3g Thru listener attack/release, no hard freeze, retained shape "
              "and neutral PCM gain: PASS");
}
void testStochasticListenerRoutingAndRecall() {
    for (int mode = 0; mode < 4; ++mode) {
        auto t = splitFixture();
        t->setPerformanceMode(mode);
        const int parts = mode ? t->partCount() : 1;
        for (int p = 0; p < parts; ++p) {
            t->selectSplitPart(p, false);
            setParam(t.get(), "algorithm", "Stochastic");
            setParam(t.get(), "stochastic_law", "Listen");
            setParam(t.get(), "stochastic_capture", p == 0 ? "100" : "80");
            setParam(t.get(), "stochastic_pull", "37");
            setParam(t.get(), "stochastic_source_hold", "On");
        }
        t->midiPart = 0;
        t->noteOn(60, 100);
        renderSplit(*t, 400);
        auto &v = newestVoice(*t, 0);
        assert(v.engine.stochasticListenerCapture() > .15f);
        for (int p = 1; p < parts; ++p)
            assert(t->stochasticPartMeters[p].rms == 0);
        const auto control = t->effectiveControls(v);
        const float target = control.stochasticListenTarget;
        assert(target > 0);
        t->outputGain = t->clipOutputGain = 0;
        t->controls.effectDrive = 1;
        t->controls.effectMix = 1;
        t->controls.effectTapeMix = 1;
        t->controls.splitLevelA = 0;
        const auto quieter = t->effectiveControls(v);
        assert(quieter.stochasticListenTarget == target &&
               quieter.notePitch == control.notePitch && quieter.speed == control.speed);
        assert(quieter.stochasticSourceHold == control.stochasticSourceHold &&
               quieter.laneScanPaused == control.laneScanPaused);
        renderSplit(*t, 100);
        assert(v.engine.stochasticListenerCapture() > .1f);
        if (parts > 1) {
            t->selectSplitPart(1, false);
            setParam(t.get(), "stochastic_law", "Listen");
            t->midiPart = 1;
            t->noteOn(67, 100);
            renderSplit(*t, 300);
            assert(newestVoice(*t, 1).engine.stochasticListenerCapture() > 0);
            assert(t->effectiveControls(v).stochasticListenTarget > 0);
        }
        t->selectSplitPart(0, false);
        nrpn(*t, 5, "stochastic_capture",
             s3g_thru_midi::encode(midiParameter("stochastic_capture"), 73, 100));
        assert(std::abs(t->controls.stochasticCapture - .73f) < .001f &&
               std::abs(t->controls.stochasticPull - .37f) < .001f);
        char state[65536]{};
        assert(getParam(t.get(), "state", state, sizeof(state)) > 0);
        auto recalled = splitFixture();
        setParam(recalled.get(), "state", state);
        assert(recalled->performanceMode() == mode);
        assert(recalled->controls.stochasticLaw == 3 &&
               std::abs(recalled->controls.stochasticCapture - .73f) < .001f);
        assert(std::abs(recalled->controls.stochasticPull - .37f) < .001f);
        assert(recalled->stochasticPartMeters[0].rms == 0); // no transient feedback saved
        t->captureActiveClip(true);
        t->controls.stochasticCapture = 0;
        t->activateStoredClip(t->activeClip, false);
        assert(std::abs(t->controls.stochasticCapture - .73f) < .001f);
    }
    std::puts("s3g Thru listener part isolation, pre-gain/effects routing, MIDI "
              "and independent recall: PASS");
}
