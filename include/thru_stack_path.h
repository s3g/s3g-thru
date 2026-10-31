#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace s3g_daisy {
// A closed, bounded path. Point curvature describes travel to the next point;
// the final segment returns to point zero at phase one.
struct ThruStackPath {
    static constexpr std::size_t maximumPoints=8, valueCount=25;
    static constexpr float minimumGap=.01f;
    int count=4;
    std::array<float,maximumPoints> time {{0,.25f,.5f,.75f,.8f,.85f,.9f,.95f}};
    std::array<float,maximumPoints> lane {{0,.5f,1,.5f,.4f,.3f,.2f,.1f}};
    std::array<float,maximumPoints> curve {};

    static float finiteClamp(float value,float low,float high,float fallback=0) {
        return std::clamp(std::isfinite(value)?value:fallback,low,high);
    }
    void sanitize() {
        count=std::clamp(count,4,8);time[0]=0;
        for(std::size_t i=0;i<maximumPoints;++i) {
            lane[i]=finiteClamp(lane[i],0,1);
            curve[i]=finiteClamp(curve[i],-1,1);
            if(i>=static_cast<std::size_t>(count))time[i]=finiteClamp(time[i],0,1,static_cast<float>(i)/8);
            if(i>0 && i<static_cast<std::size_t>(count)) {
                const float high=1-minimumGap*(count-static_cast<int>(i));
                time[i]=finiteClamp(time[i],std::min(high,time[i-1]+minimumGap),high,static_cast<float>(i)/count);
            }
        }
    }
    bool valid() const {
        if(count<4 || count>8 || time[0]!=0)return false;
        for(std::size_t i=0;i<maximumPoints;++i) {
            if(!std::isfinite(time[i]) || !std::isfinite(lane[i]) || !std::isfinite(curve[i])
                || time[i]<0 || time[i]>1 || lane[i]<0 || lane[i]>1 || curve[i]<-1 || curve[i]>1)return false;
            if(i>0 && i<static_cast<std::size_t>(count)
                && (time[i]<time[i-1]+minimumGap-1e-6f || time[i]>1-minimumGap+1e-6f))return false;
        }
        return true;
    }
    float value(std::size_t index) const {
        if(index==0)return static_cast<float>(count);
        if(index>=valueCount)return 0;
        const auto point=(index-1)/3,field=(index-1)%3;
        return field==0?time[point]:field==1?lane[point]:curve[point];
    }
    void setValue(std::size_t index,float value) {
        if(!std::isfinite(value))return;
        if(index==0){count=static_cast<int>(std::round(std::clamp(value,4.0f,8.0f)));return;}
        if(index>=valueCount)return;
        const auto point=(index-1)/3,field=(index-1)%3;
        if(field==0)time[point]=point?std::clamp(value,0.0f,1.0f):0;
        else if(field==1)lane[point]=std::clamp(value,0.0f,1.0f);
        else curve[point]=std::clamp(value,-1.0f,1.0f);
    }
    float evaluate(float phase) const {
        ThruStackPath p=*this;p.sanitize();
        phase=std::isfinite(phase)?phase-std::floor(phase):0;
        int before=0;
        while(before+1<p.count && phase>=p.time[before+1])++before;
        const int after=(before+1)%p.count;
        const float end=after?p.time[after]:1;
        const float u=std::clamp((phase-p.time[before])/(end-p.time[before]),0.0f,1.0f);
        const float bent=u+p.curve[before]*(u*u-u);
        return p.lane[before]+(p.lane[after]-p.lane[before])*bent;
    }
    int insertAfter(int point) {
        sanitize();if(count==8)return -1;
        point=std::clamp(point,0,count-1);
        const int next=(point+1)%count;
        const float end=next?time[next]:1;
        if(end-time[point]<minimumGap*2-1e-6f)return -1;
        const float bend=curve[point],fraction=.5f-.25f*bend;
        const float position=lane[point]+(lane[next]-lane[point])*fraction;
        const float when=(time[point]+end)*.5f;
        for(int i=count;i>point+1;--i){time[i]=time[i-1];lane[i]=lane[i-1];curve[i]=curve[i-1];}
        ++count;const int inserted=point+1;
        time[inserted]=when;lane[inserted]=position;
        curve[point]=bend*.25f/fraction;
        curve[inserted]=bend*.25f/(1-fraction);
        return inserted;
    }
    int remove(int point) {
        sanitize();if(count==4)return -1;
        point=std::clamp(point,0,count-1);
        if(point==0)point=count-1; // Keep the loop anchor at time zero.
        for(int i=point;i+1<count;++i){time[i]=time[i+1];lane[i]=lane[i+1];curve[i]=curve[i+1];}
        --count;return std::min(point,count-1);
    }
};
}
