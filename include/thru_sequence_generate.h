#pragma once
#include "thru_sequence.h"

// Drafting only: bounded arrays, deterministic choices, no sample scanning,
// scheduler ownership, allocations or persistent-format additions.
namespace s3g_thru_generate {
using namespace s3g_thru_sequence;
enum Field { rhythm, hits, map, order, repeat, writePolicy, amount, seed,
             regions, preroll, low, high, pitch, span, accents, depth,
             duration, minimum, maximum, human, probability, fields };
struct Config {
    std::array<float,fields> v = generatorDefaults;
    int shift=0;
    bool set(int field,float value,int length,bool timed) {
        if(field<0||field>=fields||!std::isfinite(value))return false;
        const float lo[]{0,0,0,0,1,0,0,0,1,-1,0,0,0,0,0,0,0,timed?10.f:.01f,timed?10.f:.01f,0,-1};
        const float hi[]{2,float(length),5,6,16,1,1,65535,64,30,1,1,5,24,32,1,3,timed?30000.f:32.f,timed?30000.f:32.f,.25f,1};
        value=std::clamp(value,lo[field],hi[field]);
        if(field<=writePolicy||field==seed||field==regions||field==pitch||field==span||field==accents||field==duration)value=std::round(value);
        if(field==preroll&&value<0)value=-1;
        if(field==probability&&value<0)value=-1;
        v[field]=value;
        if(v[rhythm]==2&&v[map]==0)v[map]=1;
        if(field==low){v[low]=std::min(v[low],1.f-.00001f);v[high]=std::max(v[high],v[low]+.00001f);}
        if(field==high){v[high]=std::max(v[high],.00001f);v[low]=std::min(v[low],v[high]-.00001f);}
        if(field==minimum)v[maximum]=std::max(v[maximum],v[minimum]);
        if(field==maximum)v[minimum]=std::min(v[minimum],v[maximum]);
        return true;
    }
};
struct Context {
    std::array<float,64> points{};
    int count=0,root=60;
    std::uint16_t scale=0xfff;
    float preRollNormalized=0,stepMs=125;
};
struct Result {
    Lane lane;
    std::array<bool,32> changed{};
    int changedSteps=0,enabled=0,events=0,dropped=0;
};
inline float unit(std::uint32_t seed,int field,int step,int child=0) {
    return float(hash(seed ^ (0x9e3779b9u*std::uint32_t(field+1))
        ^ (0x85ebca6bu*std::uint32_t(step+1)) ^ (0xc2b2ae35u*std::uint32_t(child+1)))>>8)/16777216.f;
}
inline bool euclid(int step,int count,int length) {
    return count>0&&((step*count)%length)<count;
}
inline bool sameValues(const Values& a,const Values& b) {
    return a.mask==b.mask&&a.values==b.values&&a.timeLocked==b.timeLocked&&a.timeMs==b.timeMs;
}
inline bool sameStep(const Step& a,const Step& b) {
    if(a.enabled!=b.enabled||a.micro!=b.micro||a.probability!=b.probability||a.count!=b.count||a.cycleHit!=b.cycleHit||a.cycleEvery!=b.cycleEvery||!sameValues(a.locks,b.locks))return false;
    for(int i=0;i<children;++i)if(a.events[i].when!=b.events[i].when||!sameValues(a.events[i].locks,b.events[i].locks))return false;
    return true;
}
inline bool sameLane(const Lane& a,const Lane& b) {
    if(a.length!=b.length||a.rate!=b.rate||a.rotation!=b.rotation||a.direction!=b.direction||a.path!=b.path||a.directionSeed!=b.directionSeed||a.enabled!=b.enabled||a.scaleEntry!=b.scaleEntry||a.timeMode!=b.timeMode||a.velocity!=b.velocity||a.gate!=b.gate||a.timeMs!=b.timeMs)return false;
    for(int i=0;i<steps;++i)if(!sameStep(a.trigs[i],b.trigs[i]))return false;
    return true;
}
struct Traversal {
    int mode=0,count=1,repeats=1,walk=0,cycle=-1,visits=1;
    std::uint32_t random=1;
    std::array<int,97> shuffled{};
    std::uint32_t next(){random=hash(random+0x9e3779b9u);return random;}
    int at(int ordinal) {
        const int n=ordinal/repeats;
        if(ordinal%repeats)return walk;
        if(mode==0)walk=n%count;
        else if(mode==1)walk=count-1-n%count;
        else if(mode==2){const int period=std::max(1,2*(count-1));const int p=n%period;walk=p<count?p:period-p;}
        else if(mode==3)walk=int(next()%std::uint32_t(count));
        else if(mode==4){if(n)walk=(walk+(next()&1u?1:count-1))%count;}
        else if(mode==6)walk=visits<=1?0:int(std::lround(double(n%visits)*(count-1)/(visits-1)));
        else {
            if(n/count!=cycle){cycle=n/count;for(int i=0;i<count;++i)shuffled[i]=i;
                for(int i=count-1;i>0;--i)std::swap(shuffled[i],shuffled[next()%std::uint32_t(i+1)]);}
            walk=shuffled[n%count];
        }
        return walk;
    }
};
inline bool durationLocked(const Values& value,bool timed){
    return timed?value.timeLocked:bool(value.mask&8u);
}
inline bool ownsGapLock(const Step& original,bool timed,const Config& config,int row,int child){
    if(config.v[duration]!=2)return false;
    const auto stream=std::uint32_t(config.v[seed])+1979u;
    if(unit(stream,4,row,child+1)>=config.v[amount])return false;
    return config.v[writePolicy]>=.5f||(!durationLocked(original.locks,timed)
        &&(child<0||!durationLocked(original.events[child].locks,timed)));
}
inline void refitGaps(Lane& lane,const std::array<Step,32>& original,
                      const Config& config,float stepMs){
    if(config.v[duration]!=2)return;
    std::array<std::int64_t,32> visits;visits.fill(-1);
    const int horizon=lane.direction==4?128:directionPeriod(lane,lane.direction);
    for(int tick=0;tick<horizon;++tick){const int row=directionPosition(lane,tick).step;
        if(visits[row]<0)visits[row]=tick;}
    for(int row=0;row<lane.length;++row){auto& step=lane.trigs[row];if(!step.enabled)continue;
        const auto fit=[&](Values& value,int child){
            if(!ownsGapLock(original[row],lane.timeMode,config,row,child))return;
            const double gap=visits[row]<0?INFINITY:nextHitGap(lane,row,std::max(0,child),visits[row]);
            const float durationValue=std::clamp(float(gap*(lane.timeMode?stepMs:1.f)),config.v[minimum],config.v[maximum]);
            if(lane.timeMode){value.timeLocked=true;value.timeMs=durationValue;}
            else {value.mask|=8u;value.values[3]=durationValue;}
        };
        fit(step.locks,-1);
        for(int child=0;child<step.count;++child)
            if(step.count>1||config.v[writePolicy]>=.5f||durationLocked(step.events[child].locks,lane.timeMode))
                fit(step.events[child].locks,child);
    }
}
// Determine ownership from the original manual locks and the same deterministic
// generation decisions. No additional saved masks or parameter IDs are needed.
inline bool usesLiveGap(const Lane& lane,int row,int child,const Step* sounding=nullptr){
    const auto& g=lane.generator;if(!g.type||!g.ready||g.values[duration]!=2)return false;
    Config config;config.v=g.values;
    const auto& step=sounding?*sounding:lane.trigs[row];
    if(durationLocked(step.events[child].locks,lane.timeMode))
        return ownsGapLock(g.base[row],lane.timeMode,config,row,child);
    return durationLocked(step.locks,lane.timeMode)&&ownsGapLock(g.base[row],lane.timeMode,config,row,-1);
}
inline Result build(const Lane& base,const Config& config,const Context& source) {
    Result out;out.lane=base;
    const auto& v=config.v;const int length=base.length;
    const auto stream=std::uint32_t(v[seed])+1979u;
    const auto allowed=[&](int field,int step,int child=0){return unit(stream,field,step,child)<v[amount];};
    const auto writable=[&](int step,int field,int child=-1){
        if(v[writePolicy]>=.5f)return true;
        const auto& original=base.trigs[step];
        const auto locked=[&](const Values& x){return field==3&&base.timeMode?x.timeLocked:bool(x.mask&(1u<<field));};
        return !locked(original.locks)&&(child<0||!locked(original.events[child].locks));
    };
    const auto lock=[&](int step,int field,float value,int child=-1){
        if(!allowed(field+1,step,child+1)||!writable(step,field,child))return;
        auto& dest=child<0?out.lane.trigs[step].locks:out.lane.trigs[step].events[child].locks;
        if(field==3&&base.timeMode){dest.timeLocked=true;dest.timeMs=value;}
        else {dest.mask|=std::uint8_t(1u<<field);dest.values[field]=value;}
    };
    // Raw onset times establish rhythm. Pre-roll changes the source start only.
    std::array<std::array<int,8>,32> audioPoints{};
    std::array<int,32> audioCounts{};
    std::array<int,64> sourceOrder{};int usableSources=0;
    for(int i=0;i<source.count;++i)sourceOrder[i]=i;
    if(v[rhythm]==2)for(int i=0;i<source.count;++i){
        const float t=std::clamp((source.points[i]-v[low])/std::max(.00001f,v[high]-v[low])*length,0.f,float(length)-.00001f);
        const int step=int(t),child=audioCounts[step];
        if(child>=children){++out.dropped;continue;}
        audioPoints[step][child]=i;++audioCounts[step];sourceOrder[usableSources++]=i;
    }
    for(int i=0;i<length;++i)if(v[rhythm]>0&&allowed(0,i)){
        auto& step=out.lane.trigs[i];
        step.enabled=v[rhythm]==1?euclid((i+length-std::clamp(config.shift,0,31)%length)%length,int(v[hits]),length):audioCounts[i]>0;
        if(v[rhythm]==2&&step.enabled){
            step.count=audioCounts[i];step.micro=0;
            for(int child=0;child<step.count;++child){
                const float t=(source.points[audioPoints[i][child]]-v[low])/std::max(.00001f,v[high]-v[low])*length;
                step.events[child].when=std::clamp(t-i,0.f,.999f);
            }
        }
    }
    Traversal positions;positions.mode=int(v[order]);positions.count=std::max(1,v[rhythm]==2?usableSources:source.count);positions.repeats=int(v[repeat]);positions.random=hash(stream^0x504f53u);
    std::array<int,97> pitches{};int pitchCount=0;
    for(int n=-int(v[span]);n<=int(v[span]);++n)if(source.root+n>=0&&source.root+n<=127&&(!base.scaleEntry||(source.scale&(1u<<((n+120)%12)))))pitches[pitchCount++]=n;
    if(!pitchCount)pitches[pitchCount++]=0;
    Traversal notes;notes.mode=std::max(0,int(v[pitch])-1);notes.count=pitchCount;notes.random=hash(stream^0x4e4f54u);
    std::array<int,32> enabled{};int count=0;
    for(int i=0;i<length;++i)if(out.lane.trigs[i].enabled)enabled[count++]=i;
    int positionOrdinal=0,totalPositionEvents=0;
    for(int ordinal=0;ordinal<count;++ordinal){const int i=enabled[ordinal];totalPositionEvents+=v[rhythm]==2&&audioCounts[i]>0?out.lane.trigs[i].count:1;}
    positions.visits=std::max(1,(totalPositionEvents+positions.repeats-1)/positions.repeats);
    for(int ordinal=0;ordinal<count;++ordinal){
        const int i=enabled[ordinal];auto& step=out.lane.trigs[i];
        if(v[map]>0&&source.count){
            const int total=v[rhythm]==2&&audioCounts[i]>0?step.count:1;
            for(int child=0;child<total;++child){
                const int at=positions.at(positionOrdinal++);
                const float pos=std::clamp(source.points[sourceOrder[at]]-source.preRollNormalized,v[low],v[high]);
                if(child==0)lock(i,0,pos);
                if(total>1)lock(i,0,pos,child);
                else if(v[writePolicy]>=.5f)for(int c=0;c<step.count;++c)lock(i,0,pos,c);
            }
        }
        if(v[pitch]>0){const float note=float(pitches[notes.at(ordinal)]);lock(i,1,note);
            if(v[writePolicy]>=.5f)for(int child=0;child<step.count;++child)lock(i,1,note,child);}

        if(v[accents]>0){
            const bool accent=euclid(ordinal,std::min(count,int(v[accents])),count);
            const float velocity=std::clamp(std::round(base.velocity*(accent?1.f:1-v[depth])),1.f,127.f);
            lock(i,2,velocity);if(v[writePolicy]>=.5f)for(int child=0;child<step.count;++child)lock(i,2,velocity,child);
        }
        if(v[duration]>0){
            float dur=v[minimum];
            if(v[duration]==2){const int next=enabled[(ordinal+1)%count];dur=float((next-i+length-1)%length+1);
                if(base.timeMode)dur*=source.stepMs;
                dur=std::clamp(dur,v[minimum],v[maximum]);}
            else if(v[duration]==3)dur=v[minimum]+(v[maximum]-v[minimum])*unit(stream,7,i);
            if(v[duration]==2&&step.count>1){
                for(int child=0;child<step.count;++child){
                    const int next=enabled[(ordinal+1)%count];
                    const float nextWhen=child+1<step.count?step.events[child+1].when
                        :float((next-i+length-1)%length+1)+out.lane.trigs[next].events[0].when;
                    float gap=std::max(.0001f,nextWhen-step.events[child].when);
                    if(base.timeMode)gap*=source.stepMs;
                    gap=std::clamp(gap,v[minimum],v[maximum]);
                    if(child==0)dur=gap;
                    lock(i,3,gap,child);
                }
            } else if(v[writePolicy]>=.5f)for(int child=0;child<step.count;++child)lock(i,3,dur,child);
            lock(i,3,dur);
        }
        if(v[human]>0&&allowed(8,i))step.micro=std::clamp(step.micro+(unit(stream,9,i)*2-1)*v[human],-.5f,.5f);
        if(v[probability]>=0&&allowed(10,i))step.probability=v[probability];
    }
    refitGaps(out.lane,base.trigs,config,source.stepMs);
    for(int i=0;i<length;++i){
        out.changed[i]=!sameStep(base.trigs[i],out.lane.trigs[i]);out.changedSteps+=out.changed[i];
        if(out.lane.trigs[i].enabled){++out.enabled;out.events+=out.lane.trigs[i].count;}
    }
    return out;
}
} // namespace s3g_thru_generate
