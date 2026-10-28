void testReverbAndRouting()
{
    for (int code = 0; code < 720; ++code) {
        auto order = s3g_daisy::decodeEffectOrder(code);
        std::sort(order.begin(), order.end());
        assert((order == s3g_daisy::ThruEffectOrder {0,1,2,3,4,5}));
    }
    for (float rate : {44100.0f, 48000.0f, 96000.0f}) {
        for (int algorithm = 0; algorithm < 2; ++algorithm) {
            s3g_daisy::ThruReverb reverb;
            assert(reverb.prepare(rate));
            // Full-wet impulse has a finite, decaying, genuinely stereo tail.
            double early = 0, late = 0, stereo = 0;
            for (std::size_t frame = 0; frame < std::size_t(rate * 5); ++frame) {
                if (frame % 128 == 0)
                    reverb.beginBlock(algorithm, .7f, 0, 8000, .7f, .15f, 1, 1);
                const float impulse = frame == 2000 ? .5f : 0;
                const auto out = reverb.process({impulse, -impulse});
                for (float value : out) assert(std::isfinite(value) && std::abs(value) < 2);
                if (frame > 4000 && frame < std::size_t(rate * 1.5f))
                    early += out[0]*out[0] + out[1]*out[1];
                if (frame > std::size_t(rate * 4)) late += out[0]*out[0] + out[1]*out[1];
                stereo += std::abs(out[0]-out[1]);
            }
            assert(early > .00001 && late < early * .01 && stereo > .01);
        }
    }
    s3g_daisy::ThruReverb bypass;
    assert(bypass.prepare(44100));
    bypass.beginBlock(0, 2, 0, 8000, .7f, .1f, 1, 0);
    assert((bypass.process({.3f,-.4f}) == std::array<float,2>{.3f,-.4f}));

    auto instance = std::make_unique<ThruInstance>();
    assert(instance->prepare(nullptr));
    auto fx = instance->controls;
    float previous = 0;
    for (int block = 0; block < 220; ++block) {
        if (block == 80) fx.effectOrder = 719;
        if (block == 85) fx.effectOrder = 320; // newest request at quiet midpoint
        if (block == 95) fx.effectOrder = 0; // queue while fade-up is in progress
        for (auto& channel : instance->voiceMix) channel.fill(.3f);
        instance->processEffects(kBlockFrames, fx);
        for (float value : instance->voiceMix[0]) {
            if (block > 2) assert(std::abs(value - previous) < .003f);
            previous = value;
        }
    }
    assert(instance->effectOrderCode == 0 && instance->effectOrderTransition == 0);
    assert(previous > .299f);
    // All eight controls change at a block boundary with an established tail.
    for (int algorithm = 0; algorithm < 2; ++algorithm) {
        for (int parameter = 0; parameter < 8; ++parameter) {
            auto edited = std::make_unique<ThruInstance>();
            auto reference = std::make_unique<ThruInstance>();
            assert(edited->prepare(nullptr)); assert(reference->prepare(nullptr));
            auto a = edited->controls; a.effectReverbMix = .6f; a.effectReverbAlgorithm = algorithm;
            const auto original = a;
            for (int block = 0; block <= 220; ++block) {
                for (std::size_t c = 0; c < 2; ++c)
                    for (std::size_t frame = 0; frame < kBlockFrames; ++frame) {
                        const float value = .25f * std::sin((block*kBlockFrames+frame)*.037f+c);
                        edited->voiceMix[c][frame] = reference->voiceMix[c][frame] = value;
                    }
                if (block == 220) {
                    switch (parameter) {
                        case 0: a.effectReverbAlgorithm = 1-algorithm; break;
                        case 1: a.effectReverbDecay = 20; break;
                        case 2: a.effectReverbPre = 200; break;
                        case 3: a.effectReverbDamp = 500; break;
                        case 4: a.effectReverbDiff = 0; break;
                        case 5: a.effectReverbMod = 1; break;
                        case 6: a.effectReverbWidth = 0; break;
                        case 7: a.effectReverbMix = 1; break;
                    }
                }
                edited->processEffects(kBlockFrames, a);
                reference->processEffects(kBlockFrames, original);
                if (block == 220)
                    for (std::size_t c = 0; c < 2; ++c)
                        assert(std::abs(edited->voiceMix[c][0] - reference->voiceMix[c][0]) < .004f);
            }
        }
    }
    // Route and algorithm automation stays stepped, including across the seam.
    ClipSlot clip;
    clip.clipDurationMs = 1000;
    clip.clipPointCount = 2;
    clip.clipPoints[0] = {100,0,static_cast<std::uint16_t>(LockParameter::effectOrder),255,ClipPointType::parameter};
    clip.clipPoints[1] = {600,719,static_cast<std::uint16_t>(LockParameter::effectOrder),255,ClipPointType::parameter};
    instance->rebuildClipPointIndex(clip);
    instance->applyClipCurves(clip, 599);
    assert(instance->clipControls.effectOrder == 0);
    instance->applyClipCurves(clip, 600);
    assert(instance->clipControls.effectOrder == 719);
    instance->applyClipCurves(clip, 50);
    assert(instance->clipControls.effectOrder == 719);
}
