// Accepted test.27 TONE reference. Keep independent of current SCANNED changes.
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace s3g_thru_scanned_v27 {
constexpr int fields=16,controlOffset=320;
enum Field {excitation,pick,hit,tune,motion,damping,brightness,width,mass,tension,center,profile,force,pressure,freeze,seed};
inline constexpr std::array<float,fields> low{0.0f,1.0f,0.0f,-48.0f,0.05f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
inline constexpr std::array<float,fields> high{3.0f,99.0f,100.0f,48.0f,6.0f,100.0f,100.0f,100.0f,100.0f,100.0f,100.0f,3.0f,100.0f,100.0f,1.0f,65535.0f};
inline constexpr std::array<float,fields> defaults{0.0f,24.0f,80.0f,0.0f,1.2f,12.0f,75.0f,30.0f,30.0f,25.0f,15.0f,1.0f,0.0f,20.0f,0.0f,1.0f};
inline constexpr const char* keys[]{"scanned_excitation","scanned_pick","scanned_hit","scanned_tune","scanned_motion","scanned_damping","scanned_brightness","scanned_width","scanned_mass","scanned_tension","scanned_center","scanned_profile","scanned_force","scanned_pressure","scanned_freeze","scanned_seed"};
struct Config {std::array<float,fields> v=defaults;};
inline int index(const char* key){for(int i=0;i<fields;++i)if(!std::strcmp(key,keys[i]))return i;return -1;}
inline bool discrete(int f){return f==excitation||f==profile||f==freeze||f==seed;}
inline bool set(Config& c,int f,float v){if(f<0||f>=fields||!std::isfinite(v))return false;c.v[f]=std::clamp(discrete(f)?std::round(v):v,low[f],high[f]);return true;}
inline bool valid(const Config& c){for(int f=0;f<fields;++f)if(!std::isfinite(c.v[f])||c.v[f]<low[f]||c.v[f]>high[f]||(discrete(f)&&std::floor(c.v[f])!=c.v[f]))return false;return true;}
inline bool applicable(const Config& c,int f){
 if(f==pick)return c.v[excitation]<2;
 if(f==mass||f==tension)return c.v[profile]!=0;
 if(f==seed)return c.v[profile]==3||c.v[excitation]==2;
 return f>=0&&f<fields;
}
struct Program {std::array<Config,4> patches{};Program(){patches[1].v[pick]=39;patches[1].v[mass]=55;patches[2].v[profile]=2;patches[2].v[force]=15;patches[3].v[excitation]=2;patches[3].v[tension]=65;}};
} // namespace s3g_thru_scanned_v27
