// Event-count lane progression must remain independent of cut/region timing.
struct CutupsFixture {
    static constexpr std::size_t capacity = 8192u;
    static constexpr float sampleRate = 48000.0f;
    static constexpr float levels[4] {0.15f, 0.30f, -0.20f, -0.45f};
    Engine engine;
    std::array<std::array<float, capacity>, 4> storage {};
    std::array<float, 4> input {}, output {};
    std::array<const float*, 4> inputs {};
    std::array<float*, 4> outputs {};
    Engine::Controls controls;
    std::size_t framesRendered = 0;
    explicit CutupsFixture(unsigned mask = 15u)
    {
        for (std::size_t lane = 0; lane < 4; ++lane) {
            storage[lane].fill(levels[lane]);
            inputs[lane] = &input[lane]; outputs[lane] = &output[lane];
        }
        assert(engine.prepare(sampleRate, storage[0].data(), storage[1].data(),
            storage[2].data(), storage[3].data(), capacity));
        for (std::size_t lane = 0; lane < 4; ++lane)
            if (mask & (1u << lane))
                assert(engine.adoptLane(lane, capacity / 2u - lane * 257u, sampleRate));
        controls.algorithm = Engine::PlaybackAlgorithm::cutups;
        controls.cutClock = Engine::CutClock::free;
        controls.cutRateHz = 32; controls.cutJoinMs = 5;
        controls.cutRegionCount = 1; controls.cutGate = 1;
        controls.laneCycles = 1;
    }
    Engine::CutupsDisplayState render(std::size_t frames = 1)
    {
        for (std::size_t frame = 0; frame < frames; ++frame) {
            engine.process(inputs.data(), outputs.data(), 1, controls);
            for (float value : output) assert(std::isfinite(value));
        }
        framesRendered += frames;
        return engine.cutupsDisplayState();
    }
    Engine::CutupsDisplayState nextCut()
    {
        auto previous = engine.cutupsDisplayState();
        for (std::size_t frame = 0; frame < 10000; ++frame) {
            const auto state = render();
            if (state.lane >= 0 && (previous.lane < 0 || state.position + 0.00001f < previous.position)) return state;
            previous = state;
        }
        assert(false && "Cutups clock stopped producing events");
        return {};
    }
};

void testCutupsScanClock()
{
    const int counts[] {0,1,2,3,4,8,16};
    for (int choice = 0; choice < 7; ++choice) {
        auto f = std::make_unique<CutupsFixture>();
        f->controls.cutLaneAdvance = static_cast<Engine::CutLaneAdvance>(choice);
        f->controls.cutRepeats = 16; // Region repetition never clocks lanes.
        for (int event = 0; event < (choice ? counts[choice] * 5 : 24); ++event) {
            const auto state = f->nextCut();
            assert(state.lane == (choice ? (event / counts[choice]) % 4 : 0));
        }
    }
    auto a = std::make_unique<CutupsFixture>();
    auto b = std::make_unique<CutupsFixture>();
    a->controls.laneCycles = 0.125f; b->controls.laneCycles = 16;
    b->controls.cutLaneAdvance = Engine::CutLaneAdvance::sixteenCuts;
    for (int event = 0; event < 32; ++event) {
        a->nextCut(); b->nextCut();
        assert(a->framesRendered == b->framesRendered);
    }
    // Tempo and Free clocks produce the same cut and lane sequences.
    a = std::make_unique<CutupsFixture>(); b = std::make_unique<CutupsFixture>();
    a->controls.cutClock = Engine::CutClock::tempo;
    a->controls.cutTempoBpm = 120;
    a->controls.cutDivision = Engine::CutDivision::thirtySecond;
    b->controls.cutRateHz = 16;
    for (int event = 0; event < 16; ++event) {
        assert(a->nextCut().lane == b->nextCut().lane);
        assert(a->framesRendered == b->framesRendered);
    }
    // A fast cut clock must still visit every lane, without clock aliasing.
    a = std::make_unique<CutupsFixture>(); a->controls.cutRateHz = 64;
    for (int event = 0; event < 16; ++event) assert(a->nextCut().lane == event % 4);
    const std::vector<std::vector<int>> orders {
        {0,1,2,3}, {0,3,2,1}, {0,1,2,3,2,1}, {},
        {0,1,2,3}, {0,3,1,2}, {0,3,1,2}
    };
    for (std::size_t order = 0; order < orders.size(); ++order) {
        if (orders[order].empty()) continue;
        auto f = std::make_unique<CutupsFixture>();
        f->controls.cutLaneOrder = static_cast<Engine::CutLaneOrder>(order);
        for (std::size_t step = 0; step < orders[order].size() * 2; ++step)
            assert(f->nextCut().lane == orders[order][step % orders[order].size()]);
    }
    // Every order/poly combination must read actual loaded lanes.
    for (unsigned mask : {1u,5u,11u,15u}) {
        auto f = std::make_unique<CutupsFixture>(mask);
        for (int order = 0; order < 7; ++order)
            for (int poly = 0; poly < 4; ++poly) {
                f->controls.cutLaneOrder = static_cast<Engine::CutLaneOrder>(order);
                f->controls.cutPolyPath = static_cast<Engine::CutPolyPath>(poly);
                f->controls.cutVoiceIndex = 7;
                for (int event = 0; event < 8; ++event) {
                    const auto state = f->nextCut();
                    assert(state.lane >= 0 && (mask & (1u << state.lane)));
                }
            }
    }
}

void testCutupsHoldAndEdits()
{
    for (int order = 0; order < 7; ++order)
        for (int poly = 0; poly < 4; ++poly) {
            auto f = std::make_unique<CutupsFixture>();
            f->controls.cutLaneOrder = static_cast<Engine::CutLaneOrder>(order);
            f->controls.cutPolyPath = static_cast<Engine::CutPolyPath>(poly);
            f->controls.cutVoiceIndex = 3;
            f->controls.cutLaneAdvance = Engine::CutLaneAdvance::twoCuts;
            f->nextCut(); const auto before = f->nextCut();
            const float held = f->engine.normalizedLaneScanPhase();
            f->controls.laneScanPaused = true;
            for (int event = 0; event < 8; ++event) assert(f->nextCut().lane == before.lane);
            f->engine.retrigger(false);
            f->controls.lanePhaseOverride = true;
            f->controls.lanePhaseOverrideValue = held;
            for (int event = 0; event < 8; ++event) assert(f->nextCut().lane == before.lane);
            f->controls.laneScanPaused = f->controls.lanePhaseOverride = false;
            bool moved = false;
            for (int event = 0; event < 8; ++event) moved |= f->nextCut().lane != before.lane;
            if (order < 3 && poly < 2) assert(moved);
        }
    for (int order = 0; order < 7; ++order) {
        auto f = std::make_unique<CutupsFixture>();
        f->controls.cutLaneOrder = static_cast<Engine::CutLaneOrder>(order);
        f->controls.cutLaneAdvance = Engine::CutLaneAdvance::hold;
        assert(f->nextCut().lane == 0);
        // Physical Lane is direct for every order, including while held.
        for (int lane = 0; lane < 4; ++lane) {
            f->controls.manualLane = static_cast<float>(lane);
            assert(f->nextCut().lane == lane);
            for (int event = 0; event < 3; ++event) assert(f->nextCut().lane == lane);
        }
        f->controls.lanePhaseOffset = 1.0f / 3.0f;
        f->controls.manualLane = 1;
        assert(f->nextCut().lane == 2); // Lane 2 + one-lane pressure offset.
        f->controls.manualLane = 2;
        assert(f->nextCut().lane == 3); // Dial remains effective under pressure.
        f->controls.laneScanPaused = true;
        f->nextCut();
        f->controls.lanePhaseOffset = 0;
        assert(f->nextCut().lane == 3); // Pressure cannot disturb Loop Hold.
        f->controls.manualLane = 0;
        assert(f->nextCut().lane == 0); // Explicit Lane edit remains live.
    }
    auto f = std::make_unique<CutupsFixture>();
    f->controls.cutLaneAdvance = Engine::CutLaneAdvance::hold;
    f->render(10000);
    float previous = f->output[0];
    f->controls.manualLane = 3;
    assert(f->render().lane == 0); // Adopt only at the next event.
    bool overlap = false, changed = false;
    for (int frame = 0; frame < 5000; ++frame) {
        const auto state = f->render();
        overlap |= state.outgoingLane >= 0 && state.outgoingGain > 0;
        changed |= state.lane == 3;
        assert(std::abs(f->output[0] - previous) < 0.01f);
        previous = f->output[0];
    }
    assert(overlap && changed);
    f->controls.cutGate = 0.05f;
    bool silence = false;
    for (int frame = 0; frame < 4000; ++frame) {
        const auto state = f->render();
        if (state.lane < 0 && state.outgoingLane < 0) {
            silence = true;
            for (std::size_t lane = 0; lane < 4; ++lane) assert(f->engine.laneWeight(lane) == 0);
            assert(f->output[0] == 0);
        }
    }
    assert(silence);
}

void testCutupsModuleDisplay()
{
    auto instance = std::make_unique<ThruInstance>();
    assert(instance->prepare(nullptr));
    for (std::size_t lane = 0; lane < 4; ++lane) {
        for (std::size_t channel = 0; channel < 2; ++channel)
            std::fill_n(instance->engine.writableLane(lane,channel),4096,0.2f);
        assert(instance->engine.adoptLane(lane,4096,instance->sampleRate));
    }
    instance->refreshPlaybackVoices();
    instance->controls.algorithm = Engine::PlaybackAlgorithm::cutups;
    instance->controls.cutClock = Engine::CutClock::free;
    instance->controls.cutRateHz = 32;
    instance->controls.cutLaneAdvance = Engine::CutLaneAdvance::hold;
    instance->controls.manualLane = 3;
    instance->controls.ampReleaseMs = 500;
    instance->pressureMode = 1;
    for (int note = 0; note < 8; ++note) instance->noteOn(60+note,100,note);
    assert(instance->effectiveControls(instance->voices[0]).lanePhaseOffset == 0);
    std::array<std::int16_t,kBlockFrames*2> output {};
    const auto render = [&](int blocks) {
        for (int block=0;block<blocks;++block) instance->render(output.data(),kBlockFrames);
    };
    render(32);
    for (const auto& voice:instance->voices) assert(voice.engine.cutupsDisplayState().lane == 3);
    instance->aftertouch(60,42,0); render(16);
    assert(instance->voices[0].engine.cutupsDisplayState().lane == 0);
    instance->controls.manualLane = 1; render(16);
    assert(instance->voices[0].engine.cutupsDisplayState().lane == 2);
    assert(instance->voices[1].engine.cutupsDisplayState().lane == 1);
    char state[1024] {};
    assert(instance->formatCutupsState(state,sizeof(state)) > 0);
    assert(std::count(state,state+std::strlen(state),';') == 7);
    instance->setStackScanPaused(true); render(16);
    // A live Lane edit during Loop Hold updates the retrigger/recorded token.
    instance->controls.manualLane = 3; render(16);
    assert(instance->voices[0].engine.cutupsDisplayState().lane == 0);
    assert(instance->voices[0].liveStackPhase == 0);
    instance->noteOff(60,0); render(200);
    instance->noteOn(60,100,0); render(16);
    assert(instance->voices[0].engine.cutupsDisplayState().lane == 0);
    instance->noteOff(60,0); render(1);
    assert(instance->formatCutupsState(state,sizeof(state)) > 0);
    assert(std::strstr(state,"0,") == state); // Release tails stay visible.
    assert(instance->formatCutupsState(state,3) == -1);
    // Stepped automation must not interpolate through intermediate choices.
    ClipSlot clip; clip.clipDurationMs = 1000; clip.clipPointCount = 4;
    clip.clipPoints[0] = {100,1,static_cast<std::uint16_t>(LockParameter::cutLaneAdvance),255,ClipPointType::parameter};
    clip.clipPoints[1] = {600,6,static_cast<std::uint16_t>(LockParameter::cutLaneAdvance),255,ClipPointType::parameter};
    clip.clipPoints[2] = {100,1,static_cast<std::uint16_t>(LockParameter::manualLane),255,ClipPointType::parameter};
    clip.clipPoints[3] = {600,4,static_cast<std::uint16_t>(LockParameter::manualLane),255,ClipPointType::parameter};
    instance->rebuildClipPointIndex(clip); instance->applyClipCurves(clip,400);
    assert(instance->clipControls.cutLaneAdvance == Engine::CutLaneAdvance::everyCut);
    assert(instance->clipControls.manualLane == 0);
    instance->applyClipCurves(clip,600);
    assert(instance->clipControls.cutLaneAdvance == Engine::CutLaneAdvance::sixteenCuts);
    assert(instance->clipControls.manualLane == 3);
    assert(static_cast<std::size_t>(LockParameter::cutLaneAdvance) == 161u);
    instance->controls.cutLaneAdvance = Engine::CutLaneAdvance::sixteenCuts;
    instance->captureActiveClip(true);
    instance->activateStoredClip(1, false);
    instance->controls.cutLaneAdvance = Engine::CutLaneAdvance::twoCuts;
    instance->captureActiveClip(true);
    instance->activateStoredClip(0, false);
    assert(instance->controls.cutLaneAdvance == Engine::CutLaneAdvance::sixteenCuts);
    std::array<char,kStateTextSize> json {};
    assert(instance->serializeState(json.data(),json.size()) > 0);
    instance->controls.cutLaneAdvance = Engine::CutLaneAdvance::hold;
    assert(instance->restoreState(json.data()));
    assert(instance->controls.cutLaneAdvance == Engine::CutLaneAdvance::sixteenCuts);
    // Exercise capture through the real controller path, including held
    // notes and their physical-lane Hold positions at recording start.
    instance->setStackScanPaused(false);
    instance->allNotesOff(); render(200);
    instance->controls.manualLane = 3;
    instance->setStackScanPaused(true);
    instance->noteOn(80,100,20); render(32);
    const auto* freshHeld = instance->findHeldVoiceForNote(80,20);
    assert(freshHeld && freshHeld->engine.cutupsDisplayState().lane == 3);
    instance->toggleClipRecording();
    assert(instance->clipRecording);
    instance->applyMidiCc(19,127); render(16);
    instance->toggleClipRecording();
    bool automation = false, heldPosition = false;
    const auto& captured = instance->clipSlots[0];
    for (std::size_t i=0;i<captured.clipPointCount;++i) {
        const auto& point = captured.clipPoints[i];
        automation |= point.type == ClipPointType::parameter
            && (point.parameter & 255u) == 161u && point.value == 6;
        heldPosition |= point.type == ClipPointType::stackPosition;
    }
    assert(automation && heldPosition);
    instance->clipPlaying = false;
    instance->allNotesOff(); render(200);
    instance->controls.manualLane = 2;
    for (int i=0;i<8;++i) instance->noteOn(60+i,100,i);
    render(32);
    instance->noteOn(100,100,30); // A fresh held pad must survive voice stealing.
    render(32);
    freshHeld = instance->findHeldVoiceForNote(100,30);
    assert(freshHeld && freshHeld->engine.cutupsDisplayState().lane == 2);
}
