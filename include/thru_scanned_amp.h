#pragma once
#include "thru_scanned_parameters.h"
namespace s3g_thru_scanned {
// Original bass-amplifier voicing, not a circuit emulation. Runs inside the
// existing 2x SCANNED readout; all voices/patches own their filter histories.
struct BassAmp {
  struct Saturator {
    float previous=0,root=1;
    void reset(float bias){previous=bias;root=std::sqrt(1+bias*bias);}
    float tick(float x){const float next=std::sqrt(1+x*x);
      // Rationalized first-order ADAA of x/sqrt(1+x*x), including x==previous.
      const float result=(x+previous)/(next+root);previous=x;root=next;return result;
    }
  };
  struct Channel {Saturator pre,power;float inputDc=0,bass=0,treble=0,ic1=0,ic2=0,outputDc=0,cabDc=0,cabLow1=0,cabLow2=0,cabBass=0;};
  std::array<Channel,2> channels{};
  bool enabled=false,warm=false;float rate=0,wet=0,fuzz=0,fuzzTarget=0,envelope=0;
  float transition=0,parameterPole=0,attack=0,release=0,hp=0,outputHp=0,bassPole=0,treblePole=0,cabHp=0,cabPole=0,cabBassPole=0;
  float frequency=800,a1=1,a2=0,a3=0;
  float drive=.35f,bass=1,mid=1,treble=1,cab=.7f,level=1;
  std::array<float,8> previousControls{};
  float targetDrive=.35f,targetBass=1,targetMid=1,targetTreble=1,targetCab=.7f,targetLevel=1;
  static constexpr float preBias=.18f,powerBias=.08f;
  static constexpr float preDc=preBias/1.01607086367044f,powerDc=powerBias/1.00319489631876f;
  bool running()const{return enabled||wet>0;}
  void sleep(){wet=0;warm=false;envelope=0;}
  void prepare(const Config& c,float sr){
    enabled=c.v[amp_mode]>.5f;fuzzTarget=c.v[amp_mode]>1.5f?1.f:0.f;
    if(!running())return;
    const bool fresh=rate!=sr;
    if(fresh){rate=sr;const float pi=3.14159265358979323846f;
      auto pole=[&](float hz){return 1-std::exp(-2*pi*hz/sr);};
      transition=1-std::exp(-1/(sr*.008f));parameterPole=1-std::exp(-1/(sr*.004f));
      attack=1-std::exp(-1/(sr*.002f));release=1-std::exp(-1/(sr*.09f));
      hp=pole(18);outputHp=pole(12);bassPole=pole(90);treblePole=pole(3500);cabHp=pole(32);cabPole=pole(4200);cabBassPole=pole(160);
    }
    constexpr float centers[]{220,450,800,1600,3000};const float desired=centers[std::clamp(int(c.v[amp_freq]),0,4)];
    if(fresh||frequency!=desired){frequency=fresh?desired:frequency+(desired-frequency)*.08f;if(std::abs(frequency-desired)<.01f)frequency=desired;
      const float g=std::tan(3.14159265358979323846f*frequency/sr),k=1.25f;a1=1/(1+g*(g+k));a2=g*a1;a3=g*a2;
    }
    targetDrive=c.v[amp_gain]*.01f;targetCab=c.v[amp_cab]*.01f;
    if(fresh||previousControls[2]!=c.v[amp_bass])targetBass=std::pow(10.f,c.v[amp_bass]*.05f);
    if(fresh||previousControls[3]!=c.v[amp_mid])targetMid=std::pow(10.f,c.v[amp_mid]*.05f);
    if(fresh||previousControls[5]!=c.v[amp_treble])targetTreble=std::pow(10.f,c.v[amp_treble]*.05f);
    if(fresh||previousControls[7]!=c.v[amp_level])targetLevel=std::pow(10.f,c.v[amp_level]*.05f);
    for(int i=0;i<8;++i)previousControls[i]=c.v[percussionFields+i];
  }
  std::array<float,2> tick(std::array<float,2> input){
    if(!running())return input;
    if(!warm){channels={};for(auto& c:channels){c.pre.reset(preBias);c.power.reset(powerBias);}drive=targetDrive;bass=targetBass;mid=targetMid;treble=targetTreble;cab=targetCab;level=targetLevel;fuzz=fuzzTarget;envelope=0;warm=true;}
    wet+=(float(enabled)-wet)*transition;fuzz+=(fuzzTarget-fuzz)*transition;
    if(!enabled&&wet<1e-6f){sleep();return input;}
    drive+=(targetDrive-drive)*parameterPole;bass+=(targetBass-bass)*parameterPole;mid+=(targetMid-mid)*parameterPole;treble+=(targetTreble-treble)*parameterPole;cab+=(targetCab-cab)*parameterPole;level+=(targetLevel-level)*parameterPole;
    const float detector=std::max(std::abs(input[0]),std::abs(input[1]));envelope+=(detector-envelope)*(detector>envelope?attack:release);
    const float preGain=1+(18+72*fuzz)*drive*drive,powerGain=1.3f+drive*(2+6*fuzz),sag=1/(1+envelope*drive*.45f),normal=.95f/(1+.35f*drive);
    for(int i=0;i<2;++i){auto& c=channels[i];const float dry=input[i];c.inputDc+=hp*(dry-c.inputDc);
      float x=c.pre.tick((dry-c.inputDc)*preGain*sag+preBias)-preDc;
      c.bass+=bassPole*(x-c.bass);c.treble+=treblePole*(x-c.treble);
      const float v3=x-c.ic2,v1=a1*c.ic1+a2*v3,v2=c.ic2+a2*c.ic1+a3*v3;c.ic1=2*v1-c.ic1;c.ic2=2*v2-c.ic2;
      x+=(bass-1)*c.bass+(treble-1)*(x-c.treble)+(mid-1)*1.25f*v1;
      x=(c.power.tick(x*powerGain+powerBias)-powerDc)*normal;
      c.outputDc+=outputHp*(x-c.outputDc);x-=c.outputDc;
      c.cabDc+=cabHp*(x-c.cabDc);const float speaker=x-c.cabDc;
      c.cabLow1+=cabPole*(speaker-c.cabLow1);c.cabLow2+=cabPole*(c.cabLow1-c.cabLow2);c.cabBass+=cabBassPole*(speaker-c.cabBass);
      const float colored=(c.cabLow2*.92f+c.cabBass*.12f);const float out=(x+(colored-x)*cab)*level;
      input[i]=dry+(out-dry)*wet;
    }
    return input;
  }
};
} // namespace s3g_thru_scanned
