#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace s3g_thru_custom_midi {
constexpr std::size_t profileCount=4, bindingCount=128, bankCount=8;
enum Kind : std::uint8_t { parameter=0, engineSlot=1, command=2 };
enum Behavior : std::uint8_t { absolute=0, momentary=1, toggle=2, trigger=3 };
enum Command : std::int16_t { play=0,stop,record,undo,panic,previousBank,nextBank,
    bank1,bank2,bank3,bank4,bank5,bank6,bank7,bank8,takeUndo,previousClip,nextClip,patternHold,ignore,commandCount };
// part: -1 shared, 0..3 A..D, 4 current editing focus. Bank 0 is always active;
// banks 1..8 apply only in that controller bank. IDs are stable registry IDs.
struct Binding {
    std::int16_t target=-1;
    std::int8_t part=0;
    std::uint8_t channel=1,cc=0,bank=0,kind=parameter,page=0,slot=0;
    std::uint8_t behavior=absolute,pickup=1,invert=0,enabled=0;
    float low=0,high=1;
};
struct Profile {
    std::array<Binding,bindingCount> bindings{};
    std::uint8_t bank=0;
};
struct State {
    std::array<Profile,profileCount> profiles{};
    std::uint8_t active=0,factoryBank=0;
};
static_assert(sizeof(State)<=16384,"controller profile snapshot must remain bounded");
inline bool valid(const Binding& b) {
    if(!b.enabled)return b.enabled==0&&std::isfinite(b.low)&&std::isfinite(b.high)&&b.low>=0&&b.high<=1&&b.low<=b.high;
    if(b.enabled!=1||b.channel<1||b.channel>16||b.cc>127||b.bank>bankCount
        ||b.part< -1||b.part>4||b.kind>command||b.page>3||b.slot>7
        ||b.behavior>trigger||b.pickup>1||b.invert>1||!std::isfinite(b.low)
        ||!std::isfinite(b.high)||b.low<0||b.high>1||b.low>b.high)return false;
    if(b.kind==parameter)return b.target>=0;
    if(b.kind==engineSlot)return b.part>=0;
    return b.target>=0&&b.target<commandCount&&b.behavior==trigger;
}
inline int match(const Profile& profile,int channel,int cc) {
    int fallback=-1;
    for(std::size_t i=0;i<bindingCount;++i) {
        const auto& b=profile.bindings[i];
        if(!b.enabled||b.channel!=channel||b.cc!=cc)continue;
        if(b.bank==profile.bank+1)return static_cast<int>(i);
        if(b.bank==0)fallback=static_cast<int>(i);
    }
    return fallback;
}
inline int conflict(const Profile& profile,const Binding& next,int ignore=-1) {
    for(std::size_t i=0;i<bindingCount;++i) {
        const auto& b=profile.bindings[i];
        if(static_cast<int>(i)!=ignore&&b.enabled&&b.channel==next.channel
            &&b.cc==next.cc&&b.bank==next.bank)return static_cast<int>(i);
    }
    return -1;
}
inline int firstFree(const Profile& p) {
    for(std::size_t i=0;i<bindingCount;++i)if(!p.bindings[i].enabled)return static_cast<int>(i);
    return -1;
}
} // namespace s3g_thru_custom_midi
