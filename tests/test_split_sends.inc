// Test actual stereo returns after the mixer and rack, beyond send-bus meters.
struct SplitReturnCapture {
    std::array<std::vector<float>,2> channels;
    double lateEnergy = 0;
    std::array<double,2> channelEnergy {};
};

SplitReturnCapture captureSplitReturn(int owner, bool reverb, float send,
                                     float returnLevel, bool fadeSource = false)
{
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(nullptr));
    t->setSplitMode(true);
    auto& c=t->controls;
    c.splitBlend=static_cast<float>(owner);c.splitCurve=0;
    c.splitDelayA=c.splitDelayB=c.splitReverbA=c.splitReverbB=0;
    if(reverb)(owner?c.splitReverbB:c.splitReverbA)=send;
    else (owner?c.splitDelayB:c.splitDelayA)=send;
    // An inactive side at full send must never leak the other side's dry audio.
    if(reverb)(owner?c.splitReverbA:c.splitReverbB)=1;
    else (owner?c.splitDelayA:c.splitDelayB)=1;
    c.effectMix=reverb?0:returnLevel;c.effectReverbMix=reverb?returnLevel:0;
    c.effectDelayMs=20;c.effectFeedback=0;c.effectWidth=0;
    c.effectReverbAlgorithm=1;c.effectReverbDecay=.7f;
    c.effectReverbMod=0;c.effectTapeMix=0;
    t->splitBlendState=c.splitBlend;
    t->effectOrderCode=c.effectOrder;t->effectOrder=s3g_daisy::decodeEffectOrder(c.effectOrder);
    auto block=[&](int impulseFrame) {
        for(auto& part:t->splitBus)for(auto& ch:part)ch.fill(0);
        if(impulseFrame>=0) {
            t->splitBus[owner][0][impulseFrame]=.4f;
            t->splitBus[owner][1][impulseFrame]=-.2f;
        }
        const std::array<Engine::Controls,2> live {c,c};
        t->processSplitBuses(kBlockFrames,live,c);
        t->processEffects(kBlockFrames,c);
    };
    for(int warm=0;warm<64;++warm)block(-1);
    SplitReturnCapture result;
    for(int b=0;b<300;++b) {
        if(fadeSource&&b==2)c.splitBlend=static_cast<float>(1-owner);
        block(b==0?37:-1);
        for(std::size_t ch=0;ch<2;++ch)for(std::size_t f=0;f<kBlockFrames;++f) {
            const float value=t->voiceMix[ch][f];assert(std::isfinite(value));
            result.channels[ch].push_back(value);
            if(b>=3) {
                result.channelEnergy[ch]+=value*value;
                result.lateEnergy+=value*value;
            }
        }
    }
    return result;
}

void testSplitSendReturns()
{
    for(bool reverb:{false,true}) {
        const auto a=captureSplitReturn(0,reverb,1,1);
        const auto b=captureSplitReturn(1,reverb,1,1);
        assert(a.lateEnergy>1e-5&&b.lateEnergy>1e-5);
        for(std::size_t ch=0;ch<2;++ch) {
            assert(a.channelEnergy[ch]>1e-6);
            for(std::size_t f=0;f<a.channels[ch].size();++f)
                assert(std::abs(a.channels[ch][f]-b.channels[ch][f])<1e-6f);
        }
        for(int owner:{0,1}) {
            const auto off=captureSplitReturn(owner,reverb,0,1);
            const auto muted=captureSplitReturn(owner,reverb,1,0);
            const auto tail=captureSplitReturn(owner,reverb,1,1,true);
            assert(off.lateEnergy<1e-15&&muted.lateEnergy<1e-15);
            assert(tail.lateEnergy>a.lateEnergy*.99);
        }
    }
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(nullptr));
    t->setSplitMode(true);
    assert(t->controls.effectMix==1&&t->controls.effectReverbMix==1);
    setParam(t.get(),"split_delay_a","0.8");setParam(t.get(),"split_reverb_b","0.6");
    setParam(t.get(),"effect_mix","45");setParam(t.get(),"effect_reverb_mix","65");
    t->selectSplitPart(1);t->selectSplitPart(0);
    assert(t->controls.splitDelayA==.8f&&t->controls.splitDelayB==0);
    assert(t->controls.splitReverbA==0&&t->controls.splitReverbB==.6f);
    assert(std::abs(t->controls.effectMix-.45f)<1e-6f&&std::abs(t->controls.effectReverbMix-.65f)<1e-6f);
}
