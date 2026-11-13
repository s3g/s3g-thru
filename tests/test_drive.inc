// A deliberately direct convolution reference: shift the time-domain history
// instead of sharing the production filter's circular/mirrored indexing or
// zero-residual scheduling. This verifies signal math across idle/wake/tails.
struct ReferenceDrive {
    struct Fir {
        std::array<float,33> history{};
        float process(float sample) {
            for(int i=32;i>0;--i)history[i]=history[i-1];
            history[0]=sample;
            static constexpr float taps[16] {
                -.0000746390270651f,.000853939593532f,-.00322899956814f,.00878936036566f,
                -.0201708049171f,.0424680634392f,-.0919110080345f,.313274088148f,
                .313274088148f,-.0919110080345f,.0424680634392f,-.0201708049171f,
                .00878936036566f,-.00322899956814f,.000853939593532f,-.0000746390270651f
            };
            float sum=history[16]*.5f;
            for(int i=0;i<16;++i)sum+=history[i*2+1]*taps[i];
            return sum;
        }
    };
    struct Channel {Fir up1,up2,down2,down1;std::array<float,24> dry{};};
    std::array<Channel,2> channels{};
    void reset(){channels={};}
    void resetNonlinearHistory(){for(auto& c:channels)c.up1=c.up2=c.down1=c.down2=Fir{};}
    std::array<float,2> process(const std::array<float,2>& input,float drive,float gain=1,bool clean=false){
        std::array<float,2> output{};
        for(int ch=0;ch<2;++ch){
            auto& c=channels[ch];output[ch]=c.dry.back();
            for(int i=23;i>0;--i)c.dry[i]=c.dry[i-1];
            c.dry[0]=input[ch];
            if(clean)continue;
            float residual=0;
            for(int p=0;p<2;++p){
                const float up=c.up1.process(p==0?input[ch]*2.f:0.f);float down=0;
                for(int q=0;q<2;++q){
                    const float sample=c.up2.process(q==0?up*2.f:0.f);
                    const float shaped=sample*(1+drive)/(1+drive*std::abs(sample));
                    const float filtered=c.down2.process(shaped-sample);
                    if(q==0)down=filtered;
                }
                const float filtered=c.down1.process(down);if(p==0)residual=filtered;
            }
            output[ch]+=residual*gain;
        }
        return output;
    }
};

void testDriveFilterAndIdleContinuity(){
    std::array<s3g_daisy::ThruDrive,2> actual;
    std::array<ReferenceDrive,2> reference;
    std::uint32_t word=0x12345678;float maximumError=0;
    for(int frame=0;frame<20000;++frame){
        if(frame==4101){for(auto& d:actual)d.resetNonlinearHistory();for(auto& d:reference)d.resetNonlinearHistory();}
        if(frame==6207){for(auto& d:actual)d.reset();for(auto& d:reference)d.reset();}
        if(frame==8001||frame==16001){std::swap(actual[0],actual[1]);std::swap(reference[0],reference[1]);}
        for(int stream=0;stream<2;++stream){
            word^=word<<13;word^=word>>17;word^=word<<5;
            const float noise=(static_cast<int>(word&65535)-32768)/32768.f;
            const float left=.35f*noise+.21f*std::sin(frame*.037f+stream);
            const std::array<float,2> input{left,stream?.17f-noise*.4f:-left*.73f};
            const int phase=(frame+stream*137)%800;
            const float drive=phase<300?0.f:phase<320?(phase-300)*.6f:phase<500?12.f
                :phase<600?(phase%7)*.31f:phase<700?0.f:.00001f;
            const float gain=frame%1500<300?0.f:.73f;
            const bool clean=frame>=10020&&frame<10120;
            const auto output=clean?actual[stream].processClean(input):actual[stream].process(input,drive,gain);
            const auto expected=reference[stream].process(input,drive,gain,clean);
            for(int ch=0;ch<2;++ch){
                assert(std::isfinite(output[ch]));
                maximumError=std::max(maximumError,std::abs(output[ch]-expected[ch]));
                assert(std::abs(output[ch]-expected[ch])<1e-6f);
            }
        }
    }
    // Full-level impulses remain exactly delayed by 24 frames, including
    // after prior nonlinear audio has completely drained back to zero drive.
    for(bool previouslyDriven:{false,true}){
        s3g_daisy::ThruDrive drive;
        if(previouslyDriven){for(int f=0;f<1000;++f)drive.process({.8f,-.6f},12);
            for(int f=0;f<256;++f)drive.process({0,0},0);}
        for(int f=0;f<256;++f){
            const auto output=drive.process(f==0?std::array<float,2>{1,-.7f}:std::array<float,2>{0,0},0);
            assert(output[0]==(f==24?1.f:0.f)&&output[1]==(f==24?-.7f:0.f));
        }
    }
    std::printf("Drive reference: impulse/delay, stereo, circular wraps, zero/nonzero edits, residual drain, clean bypass, resets and swaps; max error %.9g: PASS\n",maximumError);
}
