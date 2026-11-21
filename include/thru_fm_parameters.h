#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace s3g_thru_fm {
constexpr int legacyFields=23,fields=32,controlOffset=384;
enum Field {mode,tune,width,level,algorithm,ratio1,index1,feedback,ratio2,index2,detune,mix,cutoff,resonance,drive,decay,fm_decay,bend,sweep_time,impact,noise,noise_decay,hardness,harm,attack1,decay1,end1,curve1,attack2,decay2,end2,curve2};
inline constexpr std::array<float,fields> low{0.0f,-48.0f,0.0f,0.0f,0.0f,0.125f,0.0f,0.0f,0.125f,0.0f,0.0f,0.0f,20.0f,0.0f,0.0f,10.0f,2.0f,-48.0f,1.0f,0.0f,0.0f,1.0f,0.0f,-100.0f,0.0f,0.0f,-1.0f,0.0f,0.0f,0.0f,-1.0f,0.0f};
inline constexpr std::array<float,fields> high{1.0f,48.0f,100.0f,100.0f,3.0f,32.0f,12.0f,90.0f,32.0f,12.0f,30.0f,100.0f,20000.0f,85.0f,100.0f,8000.0f,4000.0f,48.0f,2000.0f,100.0f,100.0f,4000.0f,100.0f,100.0f,4000.0f,8000.0f,100.0f,2.0f,4000.0f,8000.0f,100.0f,2.0f};
inline constexpr std::array<float,fields> defaults{0.0f,0.0f,35.0f,75.0f,0.0f,1.0f,1.0f,0.0f,2.0f,2.0f,0.0f,100.0f,18000.0f,0.0f,0.0f,600.0f,120.0f,24.0f,25.0f,20.0f,0.0f,70.0f,60.0f,0.0f,0.0f,750.0f,35.0f,1.0f,0.0f,1200.0f,20.0f,1.0f};
inline constexpr const char* keys[]{"fm_mode","fm_tune","fm_width","fm_level","fm_algorithm","fm_ratio1","fm_index1","fm_feedback","fm_ratio2","fm_index2","fm_detune","fm_mix","fm_cutoff","fm_resonance","fm_drive","fm_decay","fm_fm_decay","fm_bend","fm_sweep_time","fm_impact","fm_noise","fm_noise_decay","fm_hardness","fm_harm","fm_attack1","fm_decay1","fm_end1","fm_curve1","fm_attack2","fm_decay2","fm_end2","fm_curve2"};
struct Config {std::array<float,fields> v=defaults;};
inline int index(const char* key){if(std::strncmp(key,"fm_",3))return -1;for(int f=0;f<fields;++f)if(!std::strcmp(key,keys[f]))return f;return -1;}
inline bool discrete(int f){return f==mode||f==algorithm||f==curve1||f==curve2;}
inline bool set(Config& c,int f,float x){if(f<0||f>=fields||!std::isfinite(x))return false;c.v[f]=std::clamp(discrete(f)?std::round(x):x,low[f],high[f]);return true;}
inline bool valid(const Config& c){for(int f=0;f<fields;++f)if(!std::isfinite(c.v[f])||c.v[f]<low[f]||c.v[f]>high[f]||(discrete(f)&&std::floor(c.v[f])!=c.v[f]))return false;return true;}
inline void legacyExtras(Config& c){for(int f=legacyFields;f<fields;++f)c.v[f]=defaults[f];c.v[attack1]=c.v[attack2]=c.v[decay1]=c.v[decay2]=0;c.v[end1]=c.v[end2]=-1;}
inline float resolved(const Config& c,int f){if((f==end1||f==end2)&&c.v[f]<0)return c.v[mode]>.5f?0.f:100.f;if((f==decay1||f==decay2)&&c.v[f]==0)return c.v[fm_decay];return c.v[f];}
inline bool applicable(const Config& c,int f){if(f<0||f>=fields)return false;if((f==ratio2||f==index2||f>=attack2)&&c.v[algorithm]==0)return false;if(f>=decay&&f<=hardness&&f!=fm_decay&&c.v[mode]<.5f)return false;if(f==fm_decay&&!((c.v[decay1]==0&&(c.v[end1]>=0?c.v[end1]<100:c.v[mode]>.5f))||(c.v[algorithm]!=0&&c.v[decay2]==0&&(c.v[end2]>=0?c.v[end2]<100:c.v[mode]>.5f))))return false;if(f==sweep_time&&c.v[bend]==0)return false;if(f==noise_decay&&c.v[noise]==0)return false;if(f==hardness&&c.v[impact]==0)return false;return true;}
struct Program {std::array<Config,4> patches{};Program(){patches[1].v[ratio1]=2;patches[1].v[index1]=2;patches[2].v[algorithm]=1;patches[2].v[index2]=1.5f;patches[3].v[ratio1]=1.4142f;patches[3].v[index1]=4;}};
}
