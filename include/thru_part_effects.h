#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <memory>
#include "thru_drive.h"
#include "thru_lofi.h"

namespace s3g_daisy {
// Split's per-instrument inserts. Buffers are allocated once in prepare;
// all live edits use the same smoothing/oversampled drive as the main rack.
class ThruPartEffects {
public:
    bool prepare(float rate) {
        rate_ = rate; size_ = static_cast<std::size_t>(std::ceil(rate * .025f)) + 4;
        history_.reset(new (std::nothrow) float[size_ * 2] {});
        slew_ = 1-std::exp(-1/(rate*.005f)); mixSlew_=1-std::exp(-1/(rate*.012f));
        loFi_.prepare(rate); drive_.reset(); return bool(history_);
    }
    void reset() { valid_=write_=0; low_={}; crossoverState_=0; drive_.reset(); loFi_.prepare(rate_); }
    template<class Controls, class Buffer>
    void process(std::size_t frames, Buffer& bus, const Controls& c) {
        const float pi=6.28318530718f;
        const float tone=std::clamp(c.effectTone,-1.0f,1.0f);
        const float cutoff=tone<0?18000*std::pow(80.0f/18000,-tone):20*std::pow(8000.0f/20,tone);
        const float alpha=1-std::exp(-pi*std::clamp(cutoff,20.0f,rate_*.45f)/rate_);
        const float cross=1-std::exp(-pi*std::clamp(c.effectRotaryCrossoverHz,120.0f,5000.0f)/rate_);
        const float ramp=std::clamp(c.effectRotaryRampMs,50.0f,8000.0f)*.001f;
        hornRate_+=(std::clamp(c.effectRotaryRateHz,0.0f,8.0f)-hornRate_)*(1-std::exp(-static_cast<float>(frames)/(rate_*ramp)));
        drumRate_+=(std::clamp(c.effectRotaryRateHz,0.0f,8.0f)*.82f-drumRate_)*(1-std::exp(-static_cast<float>(frames)/(rate_*ramp*1.6f)));
        const float hs=std::sin(pi*hornRate_/rate_),hc=std::cos(pi*hornRate_/rate_);
        const float ds=std::sin(pi*drumRate_/rate_),dc=std::cos(pi*drumRate_/rate_);
        loFi_.request(c.effectBits,static_cast<std::size_t>(std::clamp(std::round(c.effectRate),1.0f,16.0f)));
        for(std::size_t f=0;f<frames;++f) {
            tone_+=(tone-tone_)*slew_; alpha_+=(alpha-alpha_)*slew_;
            driveValue_+=(std::clamp(c.effectDrive,0.0f,1.0f)*12-driveValue_)*slew_;
            pan_+=(std::clamp(c.effectCrossPan,-1.0f,1.0f)-pan_)*slew_;
            depth_+=(std::clamp(c.effectRotaryDepth,0.0f,1.0f)-depth_)*slew_;
            mix_+=(std::clamp(c.effectRotaryMix,0.0f,1.0f)-mix_)*mixSlew_;
            crossover_+=(cross-crossover_)*slew_;
            balance_+=(std::clamp(c.effectRotaryBalance,-1.0f,1.0f)-balance_)*slew_;
            spread_+=(std::clamp(c.effectRotarySpread,0.0f,2.0f)-spread_)*slew_;
            std::array<float,2> signal {bus[0][f],bus[1][f]};
            for(std::size_t ch=0;ch<2;++ch) {
                low_[ch]+=alpha_*(signal[ch]-low_[ch]);
                const float filtered=tone_<0?low_[ch]:signal[ch]-low_[ch];
                signal[ch]+=(filtered-signal[ch])*std::abs(tone_);
            }
            signal=loFi_.process(drive_.process(signal,driveValue_));
            const float mono=(signal[0]+signal[1])*.5f;
            if(pan_<=0) for(float& v:signal) v+=(mono-v)*(pan_+1);
            else signal={mono+(signal[1]-mono)*pan_,mono+(signal[0]-mono)*pan_};
            const float middle=(signal[0]+signal[1])*.5f,side=(signal[0]-signal[1])*.5f;
            crossoverState_+=crossover_*(middle-crossoverState_);
            history_[write_]=crossoverState_; history_[size_+write_]=middle-crossoverState_;
            if(valid_<size_-2)++valid_;
            if(mix_>1e-5f) {
                const float hm=rate_*.00085f*depth_*hornCos_,dm=rate_*.00038f*depth_*drumCos_;
                const float hp=hornSin_*depth_,dp=drumSin_*depth_;
                const float hg=1+std::min(0.0f,balance_),dg=1-std::max(0.0f,balance_);
                const float sideLevel=.55f+.15f*depth_*hornCos_;
                const float l=hg*read(1,rate_*.0032f+hm)*(1-.52f*hp)+dg*read(0,rate_*.0046f+dm)*(1-.34f*dp)+side*sideLevel;
                const float rr=hg*read(1,rate_*.0032f-hm)*(1+.52f*hp)+dg*read(0,rate_*.0046f-dm)*(1+.34f*dp)-side*sideLevel;
                const float mid=(l+rr)*.5f,st=(l-rr)*.5f*spread_;
                signal[0]+=(mid+st-signal[0])*mix_; signal[1]+=(mid-st-signal[1])*mix_;
            }
            bus[0][f]=signal[0]; bus[1][f]=signal[1];
            if(++write_==size_)write_=0;
            const float nextHorn=hornCos_*hc-hornSin_*hs;
            hornSin_=hornSin_*hc+hornCos_*hs; hornCos_=nextHorn;
            const float nextDrum=drumCos_*dc-drumSin_*ds;
            drumSin_=drumSin_*dc+drumCos_*ds; drumCos_=nextDrum;
        }
        const float hn=1/std::sqrt(std::max(1e-10f,hornSin_*hornSin_+hornCos_*hornCos_));
        const float dn=1/std::sqrt(std::max(1e-10f,drumSin_*drumSin_+drumCos_*drumCos_));
        hornSin_*=hn;hornCos_*=hn;drumSin_*=dn;drumCos_*=dn;
    }
private:
    float read(std::size_t band,float lag) const {
        lag=std::clamp(lag,1.0f,static_cast<float>(size_-2));
        if(lag>static_cast<float>(valid_))return 0;
        float position=static_cast<float>(write_)-lag;if(position<0)position+=size_;
        const auto first=static_cast<std::size_t>(position),second=(first+1)%size_;
        const auto* data=history_.get()+band*size_;return data[first]+(data[second]-data[first])*(position-first);
    }
    float rate_=44100,slew_=0,mixSlew_=0;
    std::unique_ptr<float[]> history_;std::size_t size_=0,write_=0,valid_=0;
    ThruDrive drive_;ThruLoFi loFi_;
    std::array<float,2> low_{};
    float tone_=0,alpha_=1,driveValue_=0,pan_=-1,depth_=0,mix_=0,crossover_=.1f,crossoverState_=0,balance_=0,spread_=1;
    float hornRate_=.8f,drumRate_=.656f,hornSin_=0,hornCos_=1,drumSin_=1,drumCos_=0;
};
}
