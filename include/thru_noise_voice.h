#pragma once
#include "thru_noise_parameters.h"
#include "thru_table_voice.h"
namespace s3g_thru_noise {
constexpr float tau=6.2831853071795864769f;
inline float random(std::uint32_t& state){state=s3g_thru_table::Library::hash(state+0x9e3779b9u);return float(state>>8)*(2.f/16777215.f)-1;}
struct Band {float a=0,b=0,c=.8f,s=0,tc=.8f,ts=0,g=0,tg=0,pan=0,white=0,warm=0,dark=0;};
struct Event {float age=0,length=1,stride=1,gain=0,pan=0,polarity=1;std::uint32_t seed=1;int repeats=0,kind=1;std::array<float,2> last{};bool active()const{return repeats>0&&age<stride*repeats;}};
struct Model {
 Config current{},target{};std::array<Band,32> filters{};std::array<Event,8> events{};
 std::array<float,4> typeMix{},colorMix{};float bankMix=1,staticMix=0,eventNorm=1,position=0,hz=261.625565f,slew=0,coefficient=.8f,normal=1,readGain=0;
 std::array<float,2> low{},smooth{},dc{},last{},pink{},brown{},prior{},tail{};float pinkPole=.1f,brownPole=.01f,cluster=1,clusterTarget=1,clusterPhase=1;
 float eventProbability=0,eventSize=1,bitLow=8388608,bitHigh=8388608,bitBlend=0,bandNorm=1;std::uint32_t rng=1,serial=0;Config bandConfig{};float preparedHz=-1;int pendingBands=0,countdown=0,bandPhase=0,savedSeed=-1;bool smoothing=false,wasAudible=false;
 void reset(const Config& c,std::uint32_t identity,float sr,float note){*this={};current=target=c;rng=s3g_thru_table::Library::hash(identity+std::uint32_t(c.v[seed])+1);savedSeed=int(c.v[seed]);hz=note*s3g_thru_table::Library::get().pitchRatio(c.v[tune]);slew=1-std::exp(-1/(sr*.008f));bankMix=c.v[type]==0?1.f:0.f;staticMix=c.v[type]==3?1.f:0.f;colorMix[int(c.v[color])]=1;pinkPole=1-std::exp(-tau*180/sr);brownPole=1-std::exp(-tau*35/sr);prepare(sr);for(auto& b:filters)b.pan=random(rng);}
 void prepare(float sr){for(int c=0;c<2;++c)for(float* x:{&low[c],&smooth[c],&dc[c],&last[c],&tail[c]})if(std::abs(*x)<1e-20f)*x=0;const auto& p=current.v;const float pitch=hz/261.625565f;coefficient=1-std::exp(-tau*std::clamp(p[tone]*pitch,20.f,sr*.42f)/sr);eventProbability=std::min(.25f,p[rate]/sr);eventSize=std::clamp(p[size]*.001f*sr/std::max(.0625f,pitch),1.f,sr*2);const float bitsValue=std::clamp(p[bits],2.f,24.f),floor=std::floor(bitsValue);bitLow=std::exp2(floor-1);bitHigh=std::exp2(std::min(24.f,floor+1)-1);bitBlend=bitsValue-floor;
  if(int(p[seed])!=savedSeed){savedSeed=int(p[seed]);rng=s3g_thru_table::Library::hash(rng^std::uint32_t(savedSeed+1));}
 }
 void bandTargets(float sr){const auto& p=current.v;auto combined=target;std::copy(current.v.begin(),current.v.begin()+coreFields,combined.v.begin());const bool changed=combined.v!=bandConfig.v||std::abs(hz-preparedHz)>.01f||p[motion]>0;if(changed){bandConfig=combined;preparedHz=hz;pendingBands=32;}if(!pendingBands)return;pendingBands=std::max(0,pendingBands-8);float sum=0;for(int j=0;j<32;++j){const float w=target.v[bands+j]*std::clamp(p[count]-j,0.f,1.f);sum+=w*w;}bandNorm=1/std::sqrt(std::max(1.f,sum));
  // Eight staggered coefficient updates: no full resonator rebuild on a note or knob edit.
  for(int n=0;n<8;++n){const int j=bandPhase++%32;auto& b=filters[j];const int grid=int(p[style]);float ratio=grid==0?float(j+1):grid==1?float(2*j+1):grid==2?1.f+1.41421356f*j:target.v[ratios+j];ratio=std::pow(ratio,p[stretch]);const float motion=std::sin(position*tau+j*1.73f)*p[s3g_thru_noise::motion];const float frequency=hz*ratio*std::exp2(motion/12);const float widthHz=std::max(2.f,frequency*2*std::sinh(.69314718f*p[bandwidth]/24));const float radius=std::clamp(std::exp(-3.14159265f*widthHz/sr),0.f,.9998f),angle=tau*std::clamp(frequency,10.f,sr*.43f)/sr;b.tc=radius*std::cos(angle);b.ts=radius*std::sin(angle);const float tilt=std::clamp(std::pow(ratio,p[s3g_thru_noise::tilt]/6.0206f),0.f,4.f);const float guard=std::clamp((sr*.44f-frequency)/(sr*.05f),0.f,1.f);b.tg=target.v[bands+j]*std::clamp(p[count]-j,0.f,1.f)*tilt*guard*std::sqrt(std::max(0.f,1-radius*radius));}
 }
 std::array<float,2> tick(float noteHz,float sr,bool audible){
  if(smoothing){smoothing=false;for(int f=0;f<coreFields;++f){const float d=target.v[f]-current.v[f];if(discrete(f)||std::abs(d)<1e-5f*(1+std::abs(target.v[f])))current.v[f]=target.v[f];else{current.v[f]+=slew*d;smoothing=true;}}}
  hz+=(noteHz*s3g_thru_table::Library::get().pitchRatio(current.v[tune])-hz)*slew;position+=current.v[motion_rate]/sr;if(position>=1)position-=1;
  const auto& p=current.v;const auto approach=[&](float& x,float goal){const float d=goal-x;if(std::abs(d)<(goal>.5f?3e-5f:1e-8f))x=goal;else x+=d*slew;};approach(bankMix,float(p[type]==0));approach(staticMix,float(p[type]==3));for(int c=0;c<4;++c)approach(colorMix[c],float(c==int(p[color])));
  if(countdown--<=0){countdown=31;prepare(sr);if(audible&&bankMix>1e-6f)bandTargets(sr);}
  if(!audible){for(auto& e:events)if(e.active())e.age+=1;wasAudible=false;return {};}
  if(!wasAudible){for(auto& b:filters)b.a=b.b=0;low={};smooth={};dc={};last={};readGain=0;wasAudible=true;}
  readGain+=(1-readGain)*.01f;
  std::array<float,2> bedSignal{};const float white=random(rng),other=random(rng),stereo=p[width]*.01f;
  for(int c=0;c<2;++c){const float w=c?white+(other-white)*stereo:white;pink[c]+=(w-pink[c])*pinkPole;brown[c]+=(w-brown[c])*brownPole;const float blue=(w-prior[c])*.7f;prior[c]=w;bedSignal[c]=colorMix[0]*w+colorMix[1]*pink[c]*3+colorMix[2]*brown[c]*6+colorMix[3]*blue;}
  std::array<float,2> bank{};
  if(bankMix>1e-6f){for(auto& b:filters){b.g+=(b.tg-b.g)*slew;if(std::abs(b.g)+std::abs(b.tg)<1e-9f){b.a=b.b=b.g=0;b.c=b.tc;b.s=b.ts;continue;}b.c+=(b.tc-b.c)*slew;b.s+=(b.ts-b.s)*slew;const float input=random(rng);b.warm+=(input-b.warm)*pinkPole;b.dark+=(input-b.dark)*brownPole;const float colored=colorMix[0]*input+colorMix[1]*b.warm*3+colorMix[2]*b.dark*6+colorMix[3]*(input-b.white)*.7f;b.white=input;const float real=b.c*b.a-b.s*b.b+colored*b.g;b.b=b.s*b.a+b.c*b.b;b.a=std::abs(real)<1e-20f?0:real;const float pan=b.pan*stereo;bank[0]+=b.a*(1-std::max(0.f,pan));bank[1]+=b.a*(1+std::min(0.f,pan));}for(int c=0;c<2;++c)bank[c]=bank[c]*bandNorm+bedSignal[c]*p[leak]*.005f;}
  std::array<float,2> eventOut{};
  if(bankMix<.999999f){clusterPhase+=3.5f/sr;if(clusterPhase>=1){clusterPhase-=1;const float u=(random(rng)+1)*.5f;clusterTarget=.12f+4*u*u*u;}cluster+=(clusterTarget-cluster)*.001f;const float chance=eventProbability*(1-p[clump]*.01f+p[clump]*.01f*cluster);
   if((random(rng)+1)*.5f<chance||serial==0){Event* e=&events[0];for(auto& v:events)if(!v.active()){e=&v;break;}else if(v.age/v.stride>e->age/e->stride)e=&v;if(e->active())for(int c=0;c<2;++c)tail[c]=std::clamp(tail[c]+e->last[c]*eventNorm,-.5f,.5f);*e={};e->kind=int(p[type]);e->seed=s3g_thru_table::Library::hash(rng+(++serial));e->length=std::max(1.f,eventSize*(1+random(rng)*p[variation]*.0045f));e->stride=e->length*1.5f+1;e->gain=1-p[variation]*.009f*(random(rng)+1)*.5f;e->pan=random(rng);e->polarity=random(rng)<0?-1.f:1.f;e->repeats=int(p[repeat]);}
   float weight=0,activity=0;for(auto& e:events){if(!e.active())continue;const float phase=std::fmod(e.age,e.stride),u=phase/std::max(1.f,e.length);if(phase<e.length){const float fade=std::max(1.f,e.length*std::max(.002f,p[edge]*.005f));const float edgeGain=std::clamp(std::min(phase+.5f,e.length-phase-.5f)/fade,0.f,1.f);const float window=edgeGain*edgeGain*(3-2*edgeGain);const int t=e.kind;const float rem=1-u,rem2=rem*rem;float v=0;if(t==1)v=e.polarity*rem2*rem2*rem;else if(t==2)v=e.polarity*(1-8*u)*rem2*rem2;else if(t==4)v=e.polarity*std::clamp((.5f-u)/std::max(.005f,p[edge]*.0025f),-1.f,1.f);else if(t==6){const int rep=int(e.age/e.stride),address=int(u*32);const auto word=s3g_thru_table::Library::hash(e.seed^std::uint32_t(address/4)^((float(e.seed&255)/255<p[fault]*.01f)?std::uint32_t(rep):0u));v=float(word&65535)/32767.5f-1;}const float g=e.gain*window;weight+=g*g;activity+=g;for(int c=0;c<2;++c){const float x=t==5?bedSignal[c]:v*(.8f+.2f*bedSignal[c]);e.last[c]=x*g*(c?1+std::min(0.f,e.pan*stereo):1-std::max(0.f,e.pan*stereo));eventOut[c]+=e.last[c];}}e.age+=1;}
   const float norm=1/std::sqrt(std::max(1.f,weight));eventNorm=norm;for(int c=0;c<2;++c){eventOut[c]=staticMix*bedSignal[c]*std::max(.05f,1-activity*.8f)+(1-staticMix)*eventOut[c]*norm;}
  }
  std::array<float,2> out{};for(int c=0;c<2;++c){const float eventSignal=eventOut[c]+(1-staticMix)*bedSignal[c]*p[s3g_thru_noise::bed]*.005f;float x=bank[c]*bankMix+eventSignal*(1-bankMix)+tail[c];tail[c]*=.99f;const float q1=std::round(x*bitLow)/bitLow,q2=std::round(x*bitHigh)/bitHigh;x=p[bits]>=23.999f?x:q1+(q2-q1)*bitBlend;low[c]+=(x-low[c])*coefficient;smooth[c]+=(low[c]-smooth[c])*coefficient;x=smooth[c];const float high=x-last[c]+.999f*dc[c];last[c]=x;dc[c]=high;out[c]=high/(1+.12f*std::abs(high))*readGain;}
  return out;
 }
};
struct Instrument {
 std::array<Model,4> models{};std::array<float,4> laneOutput{};float position=0,noteHz=261.625565f,positionPole=0,lastSlew=-1;bool started=false;int firstPatch=0,lastPatch=3;std::uint32_t identity=1;
 void prepare(){(void)s3g_thru_table::Library::get();}void reset(){started=false;firstPatch=0;lastPatch=3;lastSlew=-1;}
 // Fixed for this note. Reachable patches still age while scanning between them.
 void setPatchRange(int first,int last){if(!started){const bool valid=first>=0&&last<4&&first<=last;firstPatch=valid?first:0;lastPatch=valid?last:3;}}

 void beginBlock(const std::array<Config,4>& p){for(int i=firstPatch;i<=lastPatch;++i)if(models[i].target.v!=p[i].v){models[i].target=p[i];models[i].smoothing=true;}}
 void endBlock(){}
 std::array<float,2> tick(const std::array<Config,4>& p,float targetPosition,float hz,float sr,float slewMs=5,const float* gains=nullptr){if(!started){for(int i=firstPatch;i<=lastPatch;++i)models[i].reset(p[i],identity+i*7919,sr,hz);position=targetPosition;noteHz=hz;started=true;}if(lastSlew!=slewMs){lastSlew=slewMs;positionPole=1-std::exp(-1/(sr*std::max(.005f,slewMs*.001f)));}position+=(targetPosition-position)*positionPole;position=std::clamp(position,0.f,3.f);noteHz+=(hz-noteHz)*.005f;const int first=int(position),second=std::min(3,first+1);const float x=position-first,mix=x*x*(3-2*x);laneOutput={};std::array<float,2> out{};for(int lane=firstPatch;lane<=lastPatch;++lane){const float gain=((lane==first?1-mix:0)+(lane==second?mix:0))*(gains?gains[lane]:1.f);auto a=models[lane].tick(noteHz,sr,gain>1e-7f);for(int c=0;c<2;++c){a[c]*=gain*models[lane].current.v[level]*.0075f;out[c]+=a[c];}laneOutput[lane]=(a[0]+a[1])*.5f;}return out;}
};
}
