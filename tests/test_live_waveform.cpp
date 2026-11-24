#include "thru_source_motion.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
using namespace s3g_thru_source;
static int byte(const char* at){char s[]{at[0],at[1],0};return std::stoi(s,nullptr,16)-128;}
int main(){
 Memory memory;assert(memory.prepare(1000));memory.source(1);Config c;c.v[9]=25;c.v[10]=1000;
 std::array<float,128> l{},r{};l.fill(.5f);r.fill(-.25f);
 for(int n=0;n<100;++n)memory.write(l.data(),r.data(),128,false);
 auto view=memory.view(c,1000);auto envelope=memory.envelope(view);assert(std::strlen(envelope.data())==480);
 for(std::size_t i=0;i<120;++i){assert(std::abs(byte(envelope.data()+i*4)+32)<=1);assert(std::abs(byte(envelope.data()+i*4+2)-64)<=1);}
 const auto frozen=envelope;memory.write(l.data(),r.data(),128,true);assert(memory.envelope(view)==frozen);
 for(int n=0;n<200;++n){for(int i=0;i<128;++i){l[i]=.1f+.8f*float((n*128+i)%8192)/8192;r[i]=-l[i]*.5f;}memory.write(l.data(),r.data(),128,false);}
 c.v[10]=6000;view=memory.view(c,1000);envelope=memory.envelope(view);std::set<int> levels;
 for(std::size_t i=0;i<120;++i){const int low=byte(envelope.data()+i*4),high=byte(envelope.data()+i*4+2);assert(low<=0&&high>=0&&high<=127);levels.insert(high);}
 assert(levels.size()>50); // True extra detail, beyond the former 32 columns / 7 levels.
 auto outside=view;outside.anchor=-10000;auto silent=memory.envelope(outside);
 for(std::size_t i=0;i<240;++i)assert(byte(silent.data()+i*2)==0);
 memory.source(2);auto empty=memory.envelope(memory.view(c,1000));
 for(std::size_t i=0;i<240;++i)assert(byte(empty.data()+i*2)==0);
 l.fill(.05f);r.fill(-.025f);memory.write(l.data(),r.data(),128,false);empty=memory.envelope(memory.view(c,1000));
 for(std::size_t i=0;i<240;++i)assert(std::abs(byte(empty.data()+i*2))<=7); // No previous-source peaks.
 // A wide/short-window query always performs the same 120 cache lookups.
 for(int i=0;i<80;++i)memory.write(l.data(),r.data(),128,false);
 auto full=memory.view(c,1000);
 volatile unsigned checksum=0;const auto start=std::chrono::steady_clock::now();
 for(int i=0;i<10000;++i){auto moving=full;moving.anchor+=i%64;auto wave=memory.envelope(moving);checksum+=unsigned(wave[i%480]);}
 const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/10000;
 std::printf("Live waveform: stereo peaks, >50 distinct levels, wrapped/frozen history, empty/source guards and 120-column packet PASS (host %.2f us/query; checksum %u)\n",us,unsigned(checksum));
}
