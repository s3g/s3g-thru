// Accepted test.27 TONE reference. Keep independent of current SCANNED changes.
#pragma once
#include "scanned_tone_reference_parameters.h"
#include "thru_table_tables.h"
#include <cstdint>
namespace s3g_thru_scanned_v27 {
// Mechanical time is independent of keyboard pitch. Sparse 64-node ring,
// fixed 1 kHz symplectic integration, positive masses/springs and implicit
// damping. At the control limits dt*omega_max stays below the stability bound.
constexpr int nodes=64,cycleSize=128;
constexpr float physicsRate=1000,pi=3.14159265358979323846f;
struct Complex {float r=0,i=0;};
struct Prepared {
  std::array<Complex,64> roots{};
  std::array<float,nodes> sine{},cosine{};
  Prepared(){for(int i=0;i<64;++i){roots[i]={std::cos(-2*pi*i/128),std::sin(-2*pi*i/128)};sine[i]=std::sin(2*pi*i/nodes);cosine[i]=std::cos(2*pi*i/nodes);}}
  static const Prepared& get(){static const Prepared p;return p;}
  void fft(std::array<Complex,128>& a,int n,bool inverse)const {
    for(int i=1,j=0;i<n;++i){int bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
    for(int len=2;len<=n;len*=2)for(int first=0;first<n;first+=len)for(int j=0;j<len/2;++j){const auto w=roots[j*128/len];const float wi=inverse?-w.i:w.i;const auto q=a[first+j+len/2];const Complex t{q.r*w.r-q.i*wi,q.r*wi+q.i*w.r},u=a[first+j];a[first+j]={u.r+t.r,u.i+t.i};a[first+j+len/2]={u.r-t.r,u.i-t.i};}
    if(inverse)for(int i=0;i<n;++i){a[i].r/=n;a[i].i/=n;}
  }
};
struct Model {
  std::array<float,nodes> y{},v{},inverseMass{},springs{},anchors{},shape{};
  std::array<float,cycleSize> oldCycle{},newCycle{};
  Config current{},target{};
  float audioWidth=0,audioFrequency=0,temporal=1,frequency=0,lastFrequency=0,pressureValue=0,energy=0,readGain=0;
  int projection=0;bool cycleReady=false,initialized=false;
  float sineForce=0,cosineForce=1;
  void reset(const Config& c,const std::array<float,nodes>* sample=nullptr){
    *this={};current=target=c;initialized=true;
    const auto& prepared=Prepared::get();
    const float at=c.v[pick]*.01f,amount=c.v[hit]*.01f;
    float mean=0;
    for(int i=0;i<nodes;++i){const float t=float(i)/nodes;
      float x=t<at?t/at:(1-t)/(1-at);x=2*x-1;
      if(int(c.v[excitation])==2)x=s3g_thru_table::Library::random(std::uint32_t(c.v[seed])+std::uint32_t(i)*16777619u)*2-1;
      if(int(c.v[excitation])==3&&sample)x=(*sample)[i];
      y[i]=x*amount;mean+=y[i];inverseMass[i]=springs[i]=1;anchors[i]=0;
      shape[i]=prepared.cosine[i];
    }
    audioWidth=c.v[width]*.001f;
    mean/=nodes;for(int i=0;i<nodes;++i){y[i]-=mean;if(int(c.v[excitation])==1){v[i]=y[i]*c.v[motion]*2*pi;y[i]=0;}}
  }
  void coefficients(){
    const auto& p=current.v;const auto& lut=Prepared::get();
    for(int i=0;i<nodes;++i){
      float z=0;
      switch(int(p[profile])){case 1:z=2*float(i)/float(nodes-1)-1;break;case 2:z=lut.cosine[i];break;case 3:z=2*s3g_thru_table::Library::random(std::uint32_t(p[seed])+std::uint32_t(i)*0x9e3779b9u)-1;break;default:break;}
      // One spring per edge: equal/opposite force preserves a symmetric graph.
      const float m=1+.65f*p[mass]*.01f*z,t=1+.65f*p[tension]*.01f*z;
      inverseMass[i]=1/std::max(.35f,m);springs[i]=std::max(.35f,t);
      anchors[i]=p[center]*.01f*.012f*(1+.5f*z);
    }
  }
  void step(float pressureInput,bool audible,float hz,float sr){
    for(int f=0;f<fields;++f){if(discrete(f))current.v[f]=target.v[f];else current.v[f]+=(target.v[f]-current.v[f])*.08f;}
    pressureValue+=(std::clamp(pressureInput,0.f,1.f)-pressureValue)*.095f;
    coefficients();
    const auto& p=current.v;
    if(p[freeze]<.5f){
      const float omega=nodes*p[motion],k=omega*omega,dt=1/physicsRate;
      const float damping=.02f+12*std::pow(p[s3g_thru_scanned_v27::damping]*.01f,2),pole=1/(1+dt*damping);
      const float angle=2*pi*p[motion]/physicsRate,cs=std::cos(angle),sn=std::sin(angle);
      const float s=sineForce*cs+cosineForce*sn;cosineForce=cosineForce*cs-sineForce*sn;sineForce=s;
      const float forceAmount=(p[force]+pressureValue*p[pressure])*.01f;
      const float driveForce=k*.003f*forceAmount/(1+energy);
      float dc=0,nextEnergy=0;
      for(int i=0;i<nodes;++i){const int prev=(i+nodes-1)%nodes,next=(i+1)%nodes;
        const float force=springs[i]*(y[next]-y[i])-springs[prev]*(y[i]-y[prev])-anchors[i]*y[i];
        v[i]=(v[i]+dt*inverseMass[i]*(k*force+driveForce*(shape[i]*sineForce+Prepared::get().sine[i]*cosineForce)))*pole;
        dc+=v[i];
      }
      dc/=nodes;
      for(int i=0;i<nodes;++i){v[i]-=dc;y[i]+=dt*v[i];nextEnergy+=y[i]*y[i]+v[i]*v[i]/(k+1);}
      energy=nextEnergy/nodes;
      // Emergency energy bound is global (never clips individual waveform points).
      // Normal presets and maximum continuous excitation stay below this guard.
      if(energy>16){const float gain=4/std::sqrt(energy);for(int i=0;i<nodes;++i){y[i]*=gain;v[i]*=gain;}energy=16;}
      for(int i=0;i<nodes;++i){if(std::abs(y[i])<1e-20f)y[i]=0;if(std::abs(v[i])<1e-20f)v[i]=0;}
    }
    if(--projection<=0){projection=4;if(audible)project(hz,sr);}
  }
  void project(float hz,float sr){
    std::array<Complex,128> a{},b{};
    for(int i=0;i<nodes;++i)a[i].r=y[i];
    Prepared::get().fft(a,nodes,false);
    const float cutoff=std::clamp(sr*.36f/std::max(5.f,hz),1.f,31.f);
    const float tilt=current.v[brightness]*.01f;
    for(int h=1;h<32;++h){
      const float fade=h==1?1.f:std::clamp((cutoff-h)/std::max(1.f,cutoff*.18f),0.f,1.f);
      const float g=2*fade*fade*(3-2*fade)*std::exp(-h*(1-tilt)*.18f);
      b[h]={a[h].r*g,a[h].i*g};b[128-h]={a[64-h].r*g,a[64-h].i*g};
    }
    Prepared::get().fft(b,128,true);
    for(int i=0;i<128;++i){const float t=temporal*temporal*(3-2*temporal);oldCycle[i]=cycleReady?oldCycle[i]+(newCycle[i]-oldCycle[i])*t:b[i].r;newCycle[i]=b[i].r;}
    temporal=0;cycleReady=true;lastFrequency=hz;
  }
  static float read(const std::array<float,128>& cycle,float phase){
    const float f=phase*128;const int i=int(f);const float t=f-i;
    const float a=cycle[(i+127)&127],b=cycle[i&127],c=cycle[(i+1)&127],d=cycle[(i+2)&127];
    return b+t*(.5f*(c-a)+t*(a-2.5f*b+2*c-.5f*d+t*(.5f*(d-a)+1.5f*(b-c))));
  }
  float read(float phase)const{const float t=temporal*temporal*(3-2*temporal),a=read(oldCycle,phase);return a+(read(newCycle,phase)-a)*t;}
};
struct Instrument {
  std::array<Model,4> models{};
  std::array<s3g_thru_table::Decimator,2> decimator{};
  std::array<float,4> laneOutput{};
  std::array<float,2> dc{},last{};
  std::array<std::array<float,nodes>,4> sampleShape{};
  std::array<bool,4> hasSample{};
  float position=0,hz=261.625565f,physicsClock=0,positionPole=0,lastSlew=-1;
  std::uint32_t identity=1;bool started=false;
  void prepare(){(void)Prepared::get();(void)s3g_thru_table::Library::get();}
  void reset(){started=false;for(auto& m:models)m.initialized=false;decimator={};dc={};last={};mPhase={};physicsClock=0;lastSlew=-1;}
  void beginBlock(const std::array<Config,4>& patches){for(int i=0;i<4;++i)models[i].target=patches[i];}
  void endBlock(){}
  std::array<float,2> tick(const std::array<Config,4>& patches,float targetPosition,float noteHz,float sr,float slewMs=5,const float* gains=nullptr,float pressureInput=0){
    if(!started){for(int i=0;i<4;++i){models[i].reset(patches[i],hasSample[i]?&sampleShape[i]:nullptr);models[i].projection=1+i;}position=targetPosition;hz=noteHz;started=true;}
    if(slewMs!=lastSlew){lastSlew=slewMs;positionPole=1-std::exp(-1/(sr*std::max(.005f,slewMs*.001f)));}
    position+=(targetPosition-position)*positionPole;position=std::clamp(position,0.f,3.f);
    hz+=(noteHz-hz)*.003f;
    const int first=int(position),second=std::min(3,first+1);const float mix=position-first,w=mix*mix*(3-2*mix);
    if(physicsClock<=0){for(int lane=0;lane<4;++lane){auto& m=models[lane];const float f=std::clamp(hz*s3g_thru_table::Library::get().pitchRatio(m.current.v[tune]),5.f,sr*.35f);m.frequency=f;m.step(pressureInput,lane==first||lane==second,f,sr);}physicsClock+=sr/physicsRate;}
    physicsClock-=1;
    laneOutput={};std::array<float,2> out{};
    for(int sub=0;sub<2;++sub){std::array<float,2> raw{};
      for(int which=0;which<2;++which){const int lane=which?second:first;const float gain=(which?w:1-w)*(gains?gains[lane]:1.f);if(gain<1e-7f)continue;
        auto& m=models[lane];if(!m.cycleReady)m.project(m.frequency,sr);
        // On entering a patch, fade its real current model into the running note.
        m.readGain+=(1-m.readGain)*.004f;
        m.audioWidth+=(m.current.v[width]*.001f-m.audioWidth)*.002f;
        const float left=m.read(mPhase[lane]),right=m.read(std::fmod(mPhase[lane]+m.audioWidth,1.f));
        raw[0]+=left*gain*m.readGain;raw[1]+=right*gain*m.readGain;laneOutput[lane]+=left*gain*.5f;
      }
      for(int c=0;c<2;++c){const float high=raw[c]-last[c]+.9992f*dc[c];last[c]=raw[c];dc[c]=high;out[c]=decimator[c].tick(high)*.65f;}
      for(int lane=0;lane<4;++lane){auto& model=models[lane];if(model.audioFrequency<=0)model.audioFrequency=model.frequency;model.audioFrequency+=(model.frequency-model.audioFrequency)*.002f;mPhase[lane]+=model.audioFrequency/(sr*2);mPhase[lane]-=std::floor(mPhase[lane]);}
    }
    for(auto& m:models)m.temporal=std::min(1.f,m.temporal+physicsRate/(4*sr));
    return out;
  }
  std::array<float,4> mPhase{};
};
} // namespace s3g_thru_scanned_v27
