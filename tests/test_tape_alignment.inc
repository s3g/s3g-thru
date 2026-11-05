// Exercise the complete rack before the program compressor and limiter.
// These tones landed on the former 7 ms dry/wet cancellation notches.
float tapeToneGain(ThruInstance& t, Engine::Controls c, float hz)
{
    c.effectTapeDrive=0; c.effectTapeMechanics=0; c.effectTapeCrinkle=0;
    c.effectTapeInertia=0; c.effectTapeHissDb=-84;
    c.effectMix=c.effectReverbMix=c.effectRotaryMix=0;
    c.effectCrossPan=-1;
    const auto warm=static_cast<std::size_t>(std::lround(t.sampleRate*.30));
    const auto measure=static_cast<std::size_t>(std::lround(t.sampleRate*.14));
    constexpr float amplitude=.12f;
    double sine=0,cosine=0;
    for(std::size_t at=0;at<warm+measure;) {
        const auto count=std::min(kBlockFrames,warm+measure-at);
        for(std::size_t f=0;f<count;++f) {
            const float x=amplitude*std::sin(6.283185307179586*hz*(at+f)/t.sampleRate);
            t.voiceMix[0][f]=t.voiceMix[1][f]=x;
        }
        t.processEffects(count,c);
        for(std::size_t f=0;f<count;++f)if(at+f>=warm) {
            const double angle=6.283185307179586*hz*(at+f)/t.sampleRate;
            sine+=t.voiceMix[0][f]*std::sin(angle);
            cosine+=t.voiceMix[0][f]*std::cos(angle);
            assert(std::abs(t.voiceMix[0][f]-t.voiceMix[1][f])<1e-6f);
        }
        at+=count;
    }
    return static_cast<float>(2*std::hypot(sine,cosine)/(measure*amplitude));
}

void testTapeAlignedMixResponse()
{
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(nullptr));
    auto c=t->controls;
    float minimum=100;
    for(int format=0;format<5;++format)for(float age:{0.f,1.f})
        for(float mix:{0.f,.25f,.5f,.70710678f,1.f})
            for(float hz:{71.4285714f,214.285714f}) {
                c.effectTapeFormat=format;c.effectTapeAge=age;c.effectTapeMix=mix;
                const float gain=tapeToneGain(*t,c,hz);
                minimum=std::min(minimum,gain);
                // Allow coloration, but reject deep periodic comb notches.
                if(gain<.85f)std::fprintf(stderr,"Tape notch: FMT=%d AGE=%g MIX=%g Hz=%g gain=%g\n",format,age,mix,hz,gain);
                assert(gain>.85f);
                if(mix==0)assert(std::abs(gain-1)<1e-5f);
            }
    std::printf("s3g Thru aligned Tape response: PASS (minimum %.2f dB across formats, ages and MIX)\n",20*std::log10(minimum));
}

void testTapeBypassTiming()
{
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(nullptr));
    auto c=t->controls;c.effectTapeMix=0;
    for(auto& channel:t->voiceMix)channel.fill(0);
    constexpr std::size_t impulse=8;
    t->voiceMix[0][impulse]=.25f;t->voiceMix[1][impulse]=-.125f;
    t->processEffects(kBlockFrames,c);
    // Uncolored bypass has a constant two-sample Tape reference, alongside
    // the rack's existing 24-sample Drive latency. No hidden 7 ms lookback.
    constexpr auto at=impulse+s3g_daisy::ThruDrive::latencyFrames+2;
    static_assert(at<kBlockFrames);
    for(std::size_t f=0;f<kBlockFrames;++f) {
        assert(t->voiceMix[0][f]==(f==at?.25f:0));
        assert(t->voiceMix[1][f]==(f==at?-.125f:0));
    }
}

void testTapeLinkedMotionAndEdits()
{
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(nullptr));
    auto c=t->controls;
    c.effectTapeMix=.70710678f;c.effectTapeDrive=.4f;c.effectTapeAge=1;
    c.effectTapeMechanics=1;c.effectTapeCrinkle=1;c.effectTapeInertia=.3f;
    c.effectTapeHissDb=-84;t->tapeHissGainValue=0;
    float previous=0,maximumStep=0;
    std::size_t position=0;
    bool stopped=false;
    for(int block=0;block<4000;++block) {
        if(block%160==0) {
            c.effectTapeFormat=(block/160)%5;
            c.effectTapeAge=(block/160)&1?1.f:0.f;
        }
        if(block==240)c.effectTapeMix=0;
        if(block==360)c.effectTapeMix=1;
        if(block==500)t->stackScanPaused=true;
        if(block==1000)t->stackScanPaused=false;
        if(block==1400)c.effectTapeMechanics=0;
        if(block==1700)c.effectTapeMix=.70710678f;
        if(block==2000)c.effectTapeMechanics=1;
        for(std::size_t f=0;f<kBlockFrames;++f) {
            const float x=.15f*std::sin(6.283185307179586*440*position++/t->sampleRate);
            t->voiceMix[0][f]=t->voiceMix[1][f]=x;
        }
        t->processEffects(kBlockFrames,c);
        stopped=stopped||t->tapeTransportProgress==1;
        for(std::size_t f=0;f<kBlockFrames;++f) {
            const float l=t->voiceMix[0][f],r=t->voiceMix[1][f];
            assert(std::isfinite(l)&&std::isfinite(r));
            assert(std::abs(l-r)<1e-6f); // shared wow, flutter, drag and inertia
            maximumStep=std::max(maximumStep,std::abs(l-previous));previous=l;
        }
    }
    assert(stopped&&t->tapeTransportProgress==0);
    if(maximumStep>=.08f)std::fprintf(stderr,"Tape live edit step: %g\n",maximumStep);
    assert(maximumStep<.08f);
    std::printf("s3g Thru Tape linked stereo, edits and stop/return: PASS (peak step %.5f)\n",maximumStep);
}
