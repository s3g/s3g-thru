#pragma once
#include "thru_fm_parameters.h"
namespace s3g_thru_fm {
// Original harmonic palettes and envelope curves, built before rendering.
// Upper harmonics alone are stored: every mip preserves the sine fundamental.
struct Extras {
 static constexpr int size=1024,levels=5;
 using Cycle=std::array<float,size+1>;
 std::array<std::array<Cycle,levels>,2> upper{};
 std::array<Cycle,3> curves{};
 Extras(){constexpr double twopi=6.2831853071795864769;
  for(int i=0;i<=size;++i){const double t=double(i)/size;curves[0][i]=float(t);curves[1][i]=float((1-std::exp(-6.90775527898*t))/.999);curves[2][i]=float((std::exp(6.90775527898*t)-1)/999);
   for(int family=0;family<2;++family)for(int mip=0;mip<levels;++mip){double sum=0;for(int h=2;h<=(16>>mip);++h){if(family==0&&!(h&1))continue;sum+=std::sin(twopi*h*t)*(.6/std::pow(double(h),1.2));}upper[family][mip][i]=float(sum);}
  }
  for(auto& family:upper)for(auto& c:family)c[size]=c[0];
  for(auto& c:curves){c[0]=0;c[size]=1;}
 }
 static const Extras& get(){static const Extras x;return x;}
 static float read(const Cycle& c,float position){const float at=std::clamp(position,0.f,1.f)*size;const int i=std::min(size-1,int(at));return c[i]+(c[i+1]-c[i])*(at-i);}
 float curve(float position,int kind)const{return read(curves[std::clamp(kind,0,2)],position);}
 struct Shape {int mip=0;float mix=0,amount=0,family=0;};
 static Shape shape(float harmonic,float bandwidth,float rate){Shape out;out.amount=std::clamp(std::abs(harmonic)*.01f,0.f,1.f);out.family=std::clamp((out.amount-.5f)*2,0.f,1.f);const float allowed=rate*.38f/std::max(5.f,bandwidth);while(out.mip<levels-2&&allowed<float(16>>(out.mip+1)))++out.mip;const float high=float(16>>out.mip),low=high*.5f;out.mix=std::clamp((high-allowed)/(high-low),0.f,1.f);return out;}
 float harmonics(float phase,const Shape& shape)const{phase-=std::floor(phase);const auto at=[&](int family){const auto& a=upper[family][shape.mip];const auto& b=upper[family][shape.mip+1];const float x=read(a,phase);return x+(read(b,phase)-x)*shape.mix;};const float odd=at(0);return shape.amount*(odd+(at(1)-odd)*shape.family);}
};
struct OperatorEnvelope {
 float attackPosition=1,decayPosition=0,value=1,attackRate=1,decayRate=1;
 void reset(const Config& c,int which,float rate){const int a=which?attack2:attack1;attackPosition=c.v[a]>0?0.f:1.f;decayPosition=0;value=attackPosition;prepare(c,which,rate);}
 void prepare(const Config& c,int which,float rate){attackRate=1000/(rate*std::max(1.f,c.v[which?attack2:attack1]));decayRate=1000/(rate*std::max(1.f,resolved(c,which?decay2:decay1)));}
 float tick(const Config& c,int which,bool drum,float slew,float legacy){const int a=which?attack2:attack1,d=which?decay2:decay1,e=which?end2:end1,curve=which?curve2:curve1;
  float raw=1;
  if(attackPosition<1){attackPosition=std::min(1.f,attackPosition+attackRate);raw=Extras::get().curve(attackPosition,int(c.v[curve]));}
  else{decayPosition=std::min(1.f,decayPosition+decayRate);const float end=c.v[e]<0?(drum?0.f:1.f):c.v[e]*.01f;raw=1+(end-1)*Extras::get().curve(decayPosition,int(c.v[curve]));}
  // Legacy files retain their original shared FM envelope exactly. New live
  // edits change the remaining rates/endpoints without restarting either phase.
  if(c.v[a]==0&&c.v[d]==0&&c.v[e]<0){value=legacy;return legacy;}
  value+=(raw-value)*slew;if(std::abs(value)<1e-20f)value=0;return std::clamp(value,0.f,1.f);
 }
};
} // namespace s3g_thru_fm
