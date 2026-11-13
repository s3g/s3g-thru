#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace s3g_daisy {
// A per-scene delay BEFORE the fade mixer. Its pending audio can be moved to
// the outgoing prefix, giving the worker actual source audio to play while it
// fills its ring. A delay after the mixer cannot provide that lead.
class ThruFadeLookahead {
public:
    static constexpr std::size_t maximumFrames=1536;
    using Stereo=std::array<float,2>;
    void prepare(float rate) {
        frames_=std::clamp<std::size_t>(static_cast<std::size_t>(std::ceil(rate*.012f)),1,maximumFrames);
        rampFrames_=std::max<std::size_t>(1,static_cast<std::size_t>(std::ceil(rate*.006f)));
        enabled_=false;mix_=from_=0;remaining_=0;clear();
    }
    void setEnabled(bool enabled, bool immediate=false) {
        if(immediate){enabled_=enabled;mix_=from_=enabled?1.f:0.f;remaining_=0;return;}
        if(enabled==enabled_)return;
        enabled_=enabled;from_=mix_;remaining_=mix_==(enabled?1.f:0.f)?0:rampFrames_;
    }
    bool enabled() const {return enabled_;}
    bool ready() const {return remaining_==0;}
    bool active() const {return enabled_&&ready();}
    std::size_t frames() const {return frames_;}
    void clear() {
        std::fill_n(history_.begin(),frames_,Stereo{});write_=0;
    }
    Stereo process(const Stereo& input) {
        const Stereo delayed=history_[write_];history_[write_]=input;
        if(++write_==frames_)write_=0;
        if(remaining_) {
            const float phase=1-static_cast<float>(--remaining_)/rampFrames_;
            const float shape=phase*phase*(3-2*phase);
            mix_=from_+((enabled_?1.f:0.f)-from_)*shape;
            if(!remaining_)mix_=enabled_?1.f:0.f;
        }
        if(mix_==0)return input;
        if(mix_==1)return delayed;
        return {input[0]+(delayed[0]-input[0])*mix_,input[1]+(delayed[1]-input[1])*mix_};
    }
    void copyPending(float* destination) const {
        auto cursor=write_;
        for(std::size_t f=0;f<frames_;++f){
            destination[2*f]=history_[cursor][0];destination[2*f+1]=history_[cursor][1];
            if(++cursor==frames_)cursor=0;
        }
    }
    template<class Reader> void seedPending(Reader read) {
        for(std::size_t f=0;f<frames_;++f)history_[f]=read(f);
        write_=0;
    }
private:
    std::array<Stereo,maximumFrames> history_{};
    std::size_t frames_=1,write_=0,rampFrames_=1,remaining_=0;
    bool enabled_=false;
    float mix_=0,from_=0;
};
}
