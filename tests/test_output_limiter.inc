void testOutputLimiterScheduling()
{
    for (float rate : {44100.0f, 48000.0f, 96000.0f}) {
        s3g_daisy::ThruOutputLimiter limiter;
        assert(limiter.prepare(rate, kOutputCeiling));
        const auto delay = limiter.lookaheadFrames();
        std::vector<float> input(2048u);
        for (std::size_t frame = 0u; frame < input.size(); ++frame) {
            input[frame] = 0.25f * std::sin(static_cast<float>(frame) * 0.017f);
            float left = 0.0f, right = 0.0f;
            limiter.process(input[frame], -input[frame] * 0.5f, left, right);
            const float expected = frame < delay ? 0.0f : input[frame - delay];
            assert(left == expected);
            assert(right == -expected * 0.5f);
            assert(limiter.gain() == 1.0f);
        }

        // Sweep every relative peak timing, including the last sample of
        // an earlier attack. A second peak must keep its own full ramp.
        for (std::size_t separation = 1u; separation <= delay + 1u; ++separation) {
            limiter.reset();
            float previousGain = 1.0f, previousOutput = 0.0f;
            for (std::size_t frame = 0u; frame < 400u + delay; ++frame) {
                const float value = frame == 100u ? 1.2f
                    : (frame == 100u + separation ? 2.0f : 0.6f);
                float left = 0.0f, right = 0.0f;
                limiter.process(value, -value * 0.5f, left, right);
                assert(std::abs(left) <= kOutputCeiling);
                assert(std::abs(right + left * 0.5f) < 0.000001f);
                const float bound = 1.5707963268f / static_cast<float>(delay) + 0.00001f;
                assert(previousGain - limiter.gain() <= bound);
                if (frame == 100u + delay - 1u)
                    assert(std::abs(left - previousOutput) < 0.02f);
                previousGain = limiter.gain();
                previousOutput = left;
            }
        }

        // Equal peaks can skip additional scheduling once current gain is safe.
        // Hold through the last delayed peak rather than relying on hard clamp.
        for (float quiet : {0.0f, 0.1f}) {
            limiter.reset();
            for (std::size_t frame = 0u; frame < 500u + delay * 2u; ++frame) {
                const float value = frame < 500u ? 2.0f : quiet;
                float left = 0.0f, right = 0.0f;
                limiter.process(value, -value * 0.5f, left, right);
                const float delayed = frame < delay ? 0.0f
                    : (frame - delay < 500u ? 2.0f : quiet);
                const float planned = delayed * limiter.gain();
                assert(planned <= kOutputCeiling + 0.0000001f);
                assert(std::abs(left - planned) < 0.0000001f);
            }
        }

        // Long trains exercise schedule wrap, increasing peaks, continuous
        // overload, hold/release and stereo linking. The final clamp should
        // not be needed to satisfy the ceiling on these finite signals.
        limiter.reset();
        std::vector<float> history(50000u);
        std::uint32_t seed = 0x719ac425u;
        float previousGain = 1.0f;
        for (std::size_t frame = 0u; frame < history.size(); ++frame) {
            seed = seed * 1664525u + 1013904223u;
            const float random = static_cast<float>(seed >> 8u) / 16777216.0f;
            const float level = frame < 10000u ? 0.7f : (frame < 30000u ? 6.0f : 0.15f);
            const float value = (random * 2.0f - 1.0f) * level;
            history[frame] = value;
            float left = 0.0f, right = 0.0f;
            limiter.process(value, -value * 0.5f, left, right);
            const float expected = frame < delay ? 0.0f : history[frame - delay] * limiter.gain();
            assert(std::abs(expected) <= kOutputCeiling + 0.000002f);
            assert(std::abs(expected - left) < 0.000002f);
            assert(std::abs(right + left * 0.5f) < 0.000002f);
            assert(previousGain - limiter.gain() <= 1.5707963268f / delay + 0.00001f);
            previousGain = limiter.gain();
        }
        // Release time is in seconds, not a fixed frame count at every rate.
        for (std::size_t frame = 0u; frame < static_cast<std::size_t>(rate * 0.4f); ++frame) {
            float left = 0.0f, right = 0.0f;
            const float before = limiter.gain();
            limiter.process(0.0f, 0.0f, left, right);
            assert(limiter.gain() >= before);
        }
        assert(limiter.gain() > 0.99f);
    }
}
