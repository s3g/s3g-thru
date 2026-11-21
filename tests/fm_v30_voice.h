// Accepted test.30 FM reference.
#pragma once
#include "fm_v30_parameters.h"
#include "thru_table_voice.h"
namespace s3g_thru_fm_v30 {
constexpr float tau=6.2831853071795864769f;
struct Sines {std::array<float,2049> values{};Sines(){for(int i=0;i<=2048;++i)values[i]=std::sin(tau*i/2048);}
 static const Sines& get(){static const Sines x;return x;}
 float read(float phase)const{phase-=std::floor(phase);const float at=phase*2048;const int i=int(at);return values[i]+(values[i+1]-values[i])*(at-i);}
};
struct Model {
 Config current{},target{};std::array<std::array<float,3>,2> phases{};
 std::array<float,2> feedbackPlain{},feedbackSerial{},noiseLow{},hitLow{},dc{},last{};
 std::array<s3g_thru_table::Filter,2> filters{};
 std::array<float,4> algorithmMix{1,0,0,0};
 bool smoothing=false,drum=false;float body=1,fmEnvelope=1,pitchEnvelope=1,noiseEnvelope=1,hitEnvelope=1;
 float bodyPole=1,fmPole=1,pitchPole=1,noisePole=1,hitPole=1,hz=261.625565f,frequency=261.625565f,detuneRatio=1,parameterPole=0,readGain=0;
 std::array<float,2> channelHz{};float noisePoleFilter=0,hitPoleFilter=0,preparedRate=0,driveGain=1,driveNormalizer=1;Config preparedConfig{};std::uint32_t random=1;int countdown=0;
 void reset(const Config& cfg,std::uint32_t id,float sr,float noteHz){*this={};current=target=cfg;drum=cfg.v[mode]>.5f;algorithmMix={};algorithmMix[int(cfg.v[algorithm])]=1;random=s3g_thru_table::Library::hash(id+1);parameterPole=1-std::exp(-1/(sr*.008f));prepare(noteHz,sr);hz=frequency;}
 void prepare(float noteHz,float sr){
  const auto& p=current.v;const auto& lib=s3g_thru_table::Library::get();frequency=std::clamp(noteHz*lib.pitchRatio(p[tune]+(drum?p[bend]*pitchEnvelope:0)),5.f,sr*.35f);
  const bool fresh=preparedRate!=sr;const auto& old=preparedConfig.v;
  detuneRatio=lib.pitchRatio(p[detune]/200);
  if(fresh)noisePoleFilter=1-std::exp(-tau*2200/(sr*2));
  if(fresh||old[hardness]!=p[hardness])hitPoleFilter=1-std::exp(-tau*(800+7200*p[hardness]*.01f)/(sr*2));
  if(drum){auto pole=[&](float ms){return std::exp(-6.907755f/(sr*std::max(.001f,ms*.001f)));};
    if(fresh||old[decay]!=p[decay])bodyPole=pole(p[decay]);
    if(fresh||old[fm_decay]!=p[fm_decay])fmPole=pole(p[fm_decay]);
    if(fresh||old[sweep_time]!=p[sweep_time])pitchPole=pole(p[sweep_time]);
    if(fresh||old[noise_decay]!=p[noise_decay])noisePole=pole(p[noise_decay]);
    if(fresh||old[hardness]!=p[hardness])hitPole=pole(2+(1-p[hardness]*.01f)*10);
  }
  if(fresh||old[cutoff]!=p[cutoff]||old[resonance]!=p[resonance])for(auto& filter:filters)filter.setup(p[cutoff],p[resonance]*.01f,sr*2);
  if(fresh||old[drive]!=p[drive]){driveGain=1+p[drive]*.08f;driveNormalizer=1/std::sqrt(driveGain);}
  for(int ch=0;ch<2;++ch)for(float* state:{&filters[ch].a,&filters[ch].b,&dc[ch],&last[ch],&noiseLow[ch],&hitLow[ch]})if(std::abs(*state)<1e-20f)*state=0;
  preparedConfig=current;preparedRate=sr;
 }
 std::array<std::array<float,2>,2> tick(float noteHz,float sr,bool audible){
  if(smoothing){smoothing=false;for(int f=0;f<fields;++f){const float delta=target.v[f]-current.v[f];if(discrete(f)||std::abs(delta)<1e-5f*(1+std::abs(target.v[f])))current.v[f]=target.v[f];else{current.v[f]+=delta*parameterPole;smoothing=true;}}}
  if(countdown--<=0){countdown=31;prepare(noteHz,sr);}
  hz+=(frequency-hz)*parameterPole;
  const auto& p=current.v;const int alg=int(p[algorithm]);for(int a=0;a<4;++a)algorithmMix[a]+=(float(a==alg)-algorithmMix[a])*parameterPole;
  const float env=drum?body:1,depthEnv=drum?fmEnvelope:1,wet=p[mix]*.01f;
  std::array<std::array<float,2>,2> result{};const auto& sine=Sines::get();
  for(int sub=0;sub<2;++sub)for(int ch=0;ch<2;++ch){
   auto& phase=phases[ch];const float f=hz*(ch?detuneRatio:1/detuneRatio),m1Hz=f*p[ratio1],m2Hz=f*p[ratio2];channelHz[ch]=f;
   if(audible){
    const float active2=1-algorithmMix[0],budget=sr*.39f;
    const float safety=std::min(1.f,std::max(0.f,budget-f)/std::max(1.f,m1Hz*(p[index1]*depthEnv+1)+m2Hz*p[index2]*depthEnv*(active2+algorithmMix[1]*p[index1]*depthEnv)));
    const float guard1=std::clamp((sr*.45f-m1Hz)/(sr*.08f),0.f,1.f),guard2=std::clamp((sr*.45f-m2Hz)/(sr*.08f),0.f,1.f);
    const float i1=p[index1]*depthEnv*safety*guard1,i2=p[index2]*depthEnv*safety*guard2;
    const float fb=p[feedback]*.02f*depthEnv*safety;
    const float m2=sine.read(phase[2]),m1=sine.read(phase[1]+feedbackPlain[ch]*fb/tau),carrier=sine.read(phase[0]);feedbackPlain[ch]+=(m1-feedbackPlain[ch])*.5f;
    const float basic=sine.read(phase[0]+i1*m1/tau);float out=algorithmMix[0]*basic;
    if(algorithmMix[1]>1e-7f){const float serial=sine.read(phase[1]+(feedbackSerial[ch]*fb+i2*m2)/tau);feedbackSerial[ch]+=(serial-feedbackSerial[ch])*.5f;out+=algorithmMix[1]*sine.read(phase[0]+i1*serial/tau);}
    if(algorithmMix[2]>1e-7f)out+=algorithmMix[2]*sine.read(phase[0]+(i1*m1+i2*m2)/tau);
    if(algorithmMix[3]>1e-7f)out+=algorithmMix[3]*basic*(1-i2/12+i2/12*m2);
    out=(carrier+(out-carrier)*wet)*env;
    if(drum){random=s3g_thru_table::Library::hash(random+0x9e3779b9u);const float white=float(random>>8)*(2.f/16777215.f)-1;noiseLow[ch]+=noisePoleFilter*(white-noiseLow[ch]);hitLow[ch]+=hitPoleFilter*(white-hitLow[ch]);out+=noiseLow[ch]*p[noise]*.01f*noiseEnvelope+(white-hitLow[ch])*p[impact]*.006f*hitEnvelope;}
    out=filters[ch].tick(out);if(p[drive]>1e-6f){const float x=out*driveGain;out=(x/(1+std::abs(x)))*driveNormalizer;}
    const float high=out-last[ch]+.9992f*dc[ch];last[ch]=out;dc[ch]=high;result[sub][ch]=high;
   }
   for(int op=0;op<3;++op){const float opHz=op==0?f:op==1?m1Hz:m2Hz;phase[op]+=std::min(opHz,sr*.45f)/(sr*2);if(phase[op]>=1)phase[op]-=1;}
  }
  if(drum){const auto age=[](float& value,float pole){value*=pole;if(value<1e-20f)value=0;};age(body,bodyPole);age(fmEnvelope,fmPole);age(pitchEnvelope,pitchPole);age(noiseEnvelope,noisePole);age(hitEnvelope,hitPole);}
  readGain+=(float(audible)-readGain)*.04f;return result;
 }
};
struct Instrument {
 std::array<Model,4> models{};std::array<s3g_thru_table::Decimator,2> decimator{};std::array<float,4> laneOutput{};
 float position=0,noteHz=261.625565f,positionPole=0,lastSlew=-1;bool started=false;std::uint32_t identity=1;
 void prepare(){(void)Sines::get();(void)s3g_thru_table::Library::get();}
 void reset(){started=false;decimator={};lastSlew=-1;}
 void beginBlock(const std::array<Config,4>& patches){for(int i=0;i<4;++i)if(models[i].target.v!=patches[i].v){models[i].target=patches[i];models[i].smoothing=true;}}
 void endBlock(){}
 std::array<float,2> tick(const std::array<Config,4>& patches,float targetPosition,float hz,float sr,float slewMs=5,const float* gains=nullptr){
  if(!started){for(int lane=0;lane<4;++lane)models[lane].reset(patches[lane],identity+lane*7919,sr,hz);position=targetPosition;noteHz=hz;started=true;}
  if(slewMs!=lastSlew){lastSlew=slewMs;positionPole=1-std::exp(-1/(sr*std::max(.005f,slewMs*.001f)));}
  position+=(targetPosition-position)*positionPole;position=std::clamp(position,0.f,3.f);noteHz+=(hz-noteHz)*.005f;
  const int first=int(position),second=std::min(3,first+1);const float x=position-first,mix=x*x*(3-2*x);
  std::array<std::array<float,2>,2> summed{};laneOutput={};
  for(int lane=0;lane<4;++lane){const float gain=((lane==first?1-mix:0)+(lane==second?mix:0))*(gains?gains[lane]:1.f);auto& model=models[lane];const auto audio=model.tick(noteHz,sr,gain>1e-7f);if(gain<=1e-7f)continue;
   const float amount=model.current.v[level]*.0075f*model.readGain*gain,width=model.current.v[s3g_thru_fm_v30::width]*.01f;
   for(int sub=0;sub<2;++sub){const float mid=(audio[sub][0]+audio[sub][1])*.5f;for(int ch=0;ch<2;++ch)summed[sub][ch]+=(mid+(audio[sub][ch]-mid)*width)*amount;laneOutput[lane]+=mid*amount*.5f;}
  }
  std::array<float,2> out{};for(int sub=0;sub<2;++sub)for(int ch=0;ch<2;++ch)out[ch]=decimator[ch].tick(summed[sub][ch]);return out;
 }
};
} // namespace s3g_thru_fm_v30
