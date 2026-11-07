#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace s3g_thru {
constexpr std::size_t sliceCapacity=64;
struct SliceMap {
    std::array<float,sliceCapacity+1> points{};
    std::array<std::array<char,24>,sliceCapacity> labels{};
    std::uint32_t count=0;
    bool overflow=false;
};
struct SliceLaneSettings {
    SliceMap edited, file;
    float preRollMs=-1; // -1: new defaults (3/0 ms); -2: legacy defaults (1/0 ms).
};
inline bool validSliceMap(const SliceMap& map) {
    if(map.count>sliceCapacity)return false;
    if(!map.count)return true;
    for(std::size_t i=0;i<=map.count;++i)
        if(!std::isfinite(map.points[i])||map.points[i]<0||map.points[i]>1
            ||(i&&map.points[i]<=map.points[i-1]))return false;
    return true;
}
inline bool addSlicePoint(SliceMap& map,float position,float gap) {
    if(!validSliceMap(map)||!map.count||map.count==sliceCapacity||!std::isfinite(position))return false;
    std::size_t at=1;while(at<map.count&&map.points[at]<position)++at;
    if(position-map.points[at-1]<gap||map.points[at]-position<gap)return false;
    for(std::size_t i=map.count+1;i>at;--i)map.points[i]=map.points[i-1];
    for(std::size_t i=map.count;i>at;--i)map.labels[i]=map.labels[i-1];
    map.labels[at]={};map.points[at]=position;++map.count;return true;
}
inline bool removeSlicePoint(SliceMap& map,std::size_t point) {
    if(!point||point>=map.count)return false;
    for(std::size_t i=point;i<map.count;++i)map.points[i]=map.points[i+1];
    for(std::size_t i=point;i+1<map.count;++i)map.labels[i]=map.labels[i+1];
    map.labels[map.count-1]={};--map.count;return true;
}
inline SliceMap reverseSliceMap(const SliceMap& source) {
    if(!source.count)return source;
    SliceMap map=source;
    for(std::size_t i=0;i<=map.count;++i)map.points[i]=1-source.points[map.count-i];
    for(std::size_t i=0;i<map.count;++i)map.labels[i]=source.labels[map.count-1-i];
    return map;
}
inline SliceMap trimSliceMap(const SliceMap& source,float low,float high) {
    SliceMap map;map.overflow=source.overflow;if(!(high>low))return map;
    for(std::size_t i=0;i<source.count;++i) {
        const auto start=std::max(low,source.points[i]),end=std::min(high,source.points[i+1]);
        if(!(end>start))continue;
        map.points[map.count]=(start-low)/(high-low);map.labels[map.count]=source.labels[i];
        ++map.count;map.points[map.count]=(end-low)/(high-low);
    }
    return map;
}
inline std::uint32_t sliceU32(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
// Plain PCM/float WAVE cue points; all metadata is parsed off the audio thread.
// Offsets refer to source frames and are converted to the decoded/truncated lane.
inline SliceMap readWaveSliceMarkers(const std::uint8_t* bytes,std::size_t size,
                                     double ratio,std::size_t decodedFrames) {
    SliceMap map;
    if(!bytes||size<12||!decodedFrames||!(ratio>0)||!std::isfinite(ratio)
        ||std::memcmp(bytes,"RIFF",4)||std::memcmp(bytes+8,"WAVE",4))return map;
    const auto declared=sliceU32(bytes+4);
    if(declared<4)return map;
    size=std::min(size,static_cast<std::size_t>(declared)+8);
    struct Cue {float position=0;std::uint32_t id=~std::uint32_t(0);};
    std::array<Cue,sliceCapacity> cues{};std::size_t count=1,valid=0;
    bool overflow=false;
    for(std::size_t offset=12;offset+8<=size;) {
        const std::size_t length=sliceU32(bytes+offset+4),start=offset+8;
        if(length>size-start)break;
        if(!std::memcmp(bytes+offset,"cue ",4)&&length>=4) {
            const auto n=sliceU32(bytes+start);
            if(n<=(length-4)/24)for(std::size_t i=0;i<n;++i) {
                const auto* p=bytes+start+4+i*24;
                if(std::memcmp(p+8,"data",4)||sliceU32(p+12)||sliceU32(p+16))continue;
                const double frame=sliceU32(p+20)*ratio;
                if(!(frame>=0&&frame<decodedFrames))continue;
                const float position=static_cast<float>(frame/decodedFrames);
                if(position>=1)continue;
                ++valid;
                std::size_t at=0;while(at<count&&cues[at].position<position)++at;
                if(at<count&&std::abs(cues[at].position-position)<.5/decodedFrames) {
                    if(cues[at].id==~std::uint32_t(0))cues[at].id=sliceU32(p);
                    continue;
                }
                if(count==cues.size()){overflow=true;continue;}
                for(std::size_t j=count;j>at;--j)cues[j]=cues[j-1];
                cues[at]={position,sliceU32(p)};++count;
            }
        }
        const auto next=start+length+(length&1u);if(next>size)break;offset=next;
    }
    if(!valid)return map;
    map.count=static_cast<std::uint32_t>(count);map.overflow=overflow;
    for(std::size_t i=0;i<count;++i)map.points[i]=cues[i].position;
    map.points[count]=1;
    // LIST/adtl labels may precede or follow the cue chunk.
    for(std::size_t offset=12;offset+8<=size;) {
        const std::size_t length=sliceU32(bytes+offset+4),start=offset+8;
        if(length>size-start)break;
        if(!std::memcmp(bytes+offset,"LIST",4)&&length>=4&&!std::memcmp(bytes+start,"adtl",4)) {
            const auto end=start+length;
            for(std::size_t sub=start+4;sub+8<=end;) {
                const std::size_t n=sliceU32(bytes+sub+4),data=sub+8;
                if(n>end-data)break;
                if(!std::memcmp(bytes+sub,"labl",4)&&n>=5) {
                    const auto id=sliceU32(bytes+data);
                    for(std::size_t i=0;i<count;++i)if(cues[i].id==id) {
                        std::size_t at=0;
                        for(std::size_t c=4;c<n&&bytes[data+c]&&at+1<map.labels[i].size();++c) {
                            const auto ch=bytes[data+c];map.labels[i][at++]=ch>=32&&ch<127?static_cast<char>(ch):'?';
                        }
                    }
                }
                const auto next=data+n+(n&1u);if(next>end)break;sub=next;
            }
        }
        const auto next=start+length+(length&1u);if(next>size)break;offset=next;
    }
    return validSliceMap(map)?map:SliceMap{};
}
} // namespace s3g_thru
