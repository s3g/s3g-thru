float renderPriorityRight(ThruInstance& t, Engine::Controls& controls,
                          float keyLevel, int blocks, bool measureSend=false)
{
    float total=0;
    const std::array<Engine::Controls,2> live {controls,controls};
    for(int block=0;block<blocks;++block) {
        for(auto& part:t.splitBus)for(auto& channel:part)channel.fill(0);
        for(std::size_t frame=0;frame<kBlockFrames;++frame) {
            // A is the left-only key. B is deliberately right-only: one
            // detector must produce the same subordinate gain in both stereo
            // channels without crosstalk obscuring the measurement.
            t.splitBus[0][0][frame]=(frame&1u)?keyLevel:-keyLevel;
            t.splitBus[1][1][frame]=.5f;
        }
        t.processSplitBuses(kBlockFrames,live,controls);
        for(std::size_t frame=0;frame<kBlockFrames;++frame)
            total+=std::abs(measureSend?t.splitDelayInput[1][frame]
                                      :t.voiceMix[1][frame]);
    }
    return total/(blocks*kBlockFrames);
}

void testPriorityDuckingAudio()
{
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(nullptr));
    t->setSplitMode(true);
    auto& c=t->controls;
    c.splitBlend=.5f;c.splitCurve=0;
    c.splitLevelA=c.splitLevelB=1;
    t->splitBlendState=.5f;t->splitCurveState=0;
    c.splitDuckThresholdDb=-36;c.splitDuckAttackMs=1;
    c.splitDuckHoldMs=0;c.splitDuckReleaseMs=20;
    c.splitDuckHpfHz=20;c.splitDuckKneeDb=0;
    c.splitPriority=0;
    const float open=renderPriorityRight(*t,c,.5f,40);
    assert(std::abs(open-.25f)<.002f);

    c.splitPriority=-24;
    const float ducked=renderPriorityRight(*t,c,.5f,120);
    assert(ducked<open*.3f);
    // The detector is stereo linked: a left-only A key ducks right-only B.
    assert(t->splitDuckGainState[1]<.3f&&t->splitDuckGainState[0]>.999f);

    // Signals below threshold must leave the subordinate part open again.
    c.splitDuckThresholdDb=-12;c.splitDuckReleaseMs=10;
    renderPriorityRight(*t,c,.001f,180);
    const float below=renderPriorityRight(*t,c,.001f,1);
    assert(below>open*.98f);

    // Reversing priority releases B and assigns gain reduction to A without
    // copying B's old reduction abruptly onto the new subordinate side.
    c.splitDuckThresholdDb=-36;c.splitPriority=24;
    renderPriorityRight(*t,c,.5f,2);
    assert(t->splitDuckGainState[1]>t->splitDuckGainState[0]);
}

void testPriorityDuckingRoutingAndBypass()
{
    for(int route:{0,1}) {
        auto t=std::make_unique<ThruInstance>();assert(t->prepare(nullptr));
        t->setSplitMode(true);
        auto& c=t->controls;
        c.splitBlend=.5f;c.splitCurve=0;c.splitDelayB=1;
        t->splitBlendState=.5f;t->splitCurveState=0;
        c.splitPriority=-24;c.splitDuckThresholdDb=-36;
        c.splitDuckAttackMs=1;c.splitDuckHoldMs=0;c.splitDuckReleaseMs=20;
        c.splitDuckHpfHz=20;c.splitDuckKneeDb=0;c.splitDuckRoute=route;
        renderPriorityRight(*t,c,.5f,100);
        const float dry=renderPriorityRight(*t,c,.5f,20);
        const float send=renderPriorityRight(*t,c,.5f,20,true);
        assert(dry<.08f);
        if(route==0)assert(std::abs(send-.25f)<.002f);
        else assert(std::abs(send-dry)<.002f);
    }

    auto q=std::make_unique<ThruInstance>();assert(q->prepare(nullptr));
    q->setSplitMode(true);
    auto& c=q->controls;c.splitPriority=-24;c.splitDuckThresholdDb=-60;
    c.splitLevelA=c.splitLevelB=1;
    renderPriorityRight(*q,c,.5f,120);
    assert(q->splitDuckGainState[1]<.3f);
    q->setPerformanceMode(3);
    const std::array<Engine::Controls,4> live {c,c,c,c};
    for(auto& part:q->splitBus)for(auto& channel:part)channel.fill(0);
    for(std::size_t frame=0;frame<kBlockFrames;++frame) {
        q->splitBus[0][0][frame]=(frame&1u)?.5f:-.5f;
        q->splitBus[1][1][frame]=.5f;
    }
    q->processSplitBuses(kBlockFrames,live,c);
    // Quad bypasses the dual-part detector immediately, even while its
    // dormant smoothing state is still returning to unity.
    assert(q->splitFrameGain[1][0]>.49f);
}

void testPriorityDuckingControls()
{
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(nullptr));
    t->setSplitMode(true);
    setParam(t.get(),"split_priority","-18");
    setParam(t.get(),"split_duck_threshold","-30");
    setParam(t.get(),"split_duck_attack","7");
    setParam(t.get(),"split_duck_hold","55");
    setParam(t.get(),"split_duck_release","410");
    setParam(t.get(),"split_duck_hpf","160");
    setParam(t.get(),"split_duck_knee","9");
    setParam(t.get(),"split_duck_route","0");
    assert(t->controls.splitPriority==-18);
    assert(t->controls.splitDuckThresholdDb==-30);
    assert(t->controls.splitDuckAttackMs==7&&t->controls.splitDuckHoldMs==55);
    assert(t->controls.splitDuckReleaseMs==410&&t->controls.splitDuckHpfHz==160);
    assert(t->controls.splitDuckKneeDb==9&&t->controls.splitDuckRoute==0);
    t->storeFocusedPart();t->selectSplitPart(1);
    assert(t->controls.splitPriority==-18&&t->controls.splitDuckThresholdDb==-30);
}
