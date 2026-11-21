// Accepted test.28 reference: AMP OFF must retain this output.
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace s3g_thru_scanned_v28 {
constexpr int legacyFields=16,fields=23,controlOffset=320;
enum Field {excitation,pick,hit,tune,motion,damping,brightness,width,mass,tension,center,profile,force,pressure,freeze,seed,art,impact,hardness,decay,bend,sweep_time,loss};
inline constexpr std::array<float,fields> low{0.0f,1.0f,0.0f,-48.0f,0.05f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,10.0f,-48.0f,5.0f,0.0f};
inline constexpr std::array<float,fields> high{3.0f,99.0f,100.0f,48.0f,6.0f,100.0f,100.0f,100.0f,100.0f,100.0f,100.0f,3.0f,100.0f,100.0f,1.0f,65535.0f,1.0f,100.0f,100.0f,4000.0f,48.0f,2000.0f,100.0f};
inline constexpr std::array<float,fields> defaults{0.0f,24.0f,80.0f,0.0f,1.2f,12.0f,75.0f,30.0f,30.0f,25.0f,15.0f,1.0f,0.0f,20.0f,0.0f,1.0f,0.0f,35.0f,60.0f,350.0f,0.0f,35.0f,50.0f};
inline constexpr const char* keys[]{"scanned_excitation","scanned_pick","scanned_hit","scanned_tune","scanned_motion","scanned_damping","scanned_brightness","scanned_width","scanned_mass","scanned_tension","scanned_center","scanned_profile","scanned_force","scanned_pressure","scanned_freeze","scanned_seed","scanned_art","scanned_impact","scanned_hardness","scanned_decay","scanned_bend","scanned_sweep_time","scanned_loss"};
struct Config {std::array<float,fields> v=defaults;};
inline int index(const char* key){if(std::strncmp(key,"scanned_",8))return -1;for(int i=0;i<fields;++i)if(!std::strcmp(key,keys[i]))return i;return -1;}
inline bool discrete(int f){return f==excitation||f==profile||f==freeze||f==seed||f==art;}
inline bool set(Config& c,int f,float v){if(f<0||f>=fields||!std::isfinite(v))return false;c.v[f]=std::clamp(discrete(f)?std::round(v):v,low[f],high[f]);return true;}
inline bool valid(const Config& c){for(int f=0;f<fields;++f)if(!std::isfinite(c.v[f])||c.v[f]<low[f]||c.v[f]>high[f]||(discrete(f)&&std::floor(c.v[f])!=c.v[f]))return false;return true;}
inline bool applicable(const Config& c,int f){
 if(f==pick)return c.v[excitation]<2||c.v[art]>.5f;
 if(f==mass||f==tension)return c.v[profile]!=0;
 if(f==seed)return c.v[profile]==3||c.v[excitation]==2||(c.v[art]>.5f&&c.v[impact]>0);
 if(f==sweep_time&&c.v[bend]==0)return false;
 if(f>art)return f<fields&&c.v[art]>.5f;
 return f>=0&&f<fields;
}
struct Program {std::array<Config,4> patches{};Program(){patches[1].v[pick]=39;patches[1].v[mass]=55;patches[2].v[profile]=2;patches[2].v[force]=15;patches[3].v[excitation]=2;patches[3].v[tension]=65;}};
} // namespace s3g_thru_scanned_v28
