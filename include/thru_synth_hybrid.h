#pragma once
namespace s3g_thru_synth {
// Worker-only analysis. Audio threads only ever receive the completed PCM.
struct HybridFeature { float rms=0, hz=0, confidence=0, onset=0; };
struct HybridProfile {
  std::vector<HybridFeature> frames;
  float maximum=0;
  HybridFeature at(double position) const {
    if(position<0 || position>=double(frames.size()-1)) return {};
    const auto i=std::size_t(position); const float f=float(position-double(i));
    const auto &a=frames[i], &b=frames[i+1];
    const auto lerp=[f](float x,float y){return x+(y-x)*f;};
    const bool pitched=a.hz>0&&b.hz>0;
    return {lerp(a.rms,b.rms),pitched?std::exp(lerp(std::log(a.hz),std::log(b.hz))):0,
      pitched?lerp(a.confidence,b.confidence):0,lerp(a.onset,b.onset)};
  }
};
inline const std::array<float,ResynthPlan::size/2>& hybridWindowCorrelation(){
  static const auto result=[](){
    std::array<float,ResynthPlan::size/2> a{};
    const auto &w=ResynthPlan::get().window;
    for(int lag=0;lag<ResynthPlan::size/2;++lag){double v=0;
      for(int i=lag;i<ResynthPlan::size;++i)v+=double(w[i])*w[i-lag];
      a[lag]=float(v);
    }
    const float zero=a[0];for(float &v:a)v=std::max(.08f,v/zero);return a;
  }();return result;
}
template<class Cancel> bool hybridAnalyze(ResynthAnalysis& analysis,float rate,
    HybridProfile& out,Cancel cancel){
  using P=ResynthPlan; const auto &plan=P::get();
  const auto count=(analysis.end-analysis.start+P::hop-1)/P::hop+2;
  out.frames.resize(count);
  float previousRms=0,slowFlux=0,onset=0;
  std::array<float,P::bins> previous{};
  for(std::size_t i=0;i<count;++i){
    if(cancel())return false;
    const auto &a=analysis.frame(int(i));P::Block power{};
    double energy=0;float flux=0,total=0;
    for(int k=1;k<P::bins-1;++k){
      const float q=a.left[k]*a.left[k]+a.right[k]*a.right[k];
      const float magnitude=std::sqrt(q);
      flux+=std::max(0.f,magnitude-previous[k]);total+=magnitude;previous[k]=magnitude;
      power[k]=power[P::size-k]=q;energy+=2*double(q);
    }
    auto &f=out.frames[i];f.rms=float(std::sqrt(energy/(P::size*P::size*.75)));
    out.maximum=std::max(out.maximum,f.rms);
    const float relativeFlux=flux/std::max(1e-9f,total);
    const bool attack=f.rms>1e-5f&&f.rms>previousRms*1.08f&&
      (relativeFlux>std::max(.18f,slowFlux*1.7f)||previousRms<1e-6f);
    onset=std::max(attack?1.f:0.f,onset*std::exp(-P::hop/(rate*.022f)));
    f.onset=onset;previousRms=f.rms;slowFlux+=.08f*(relativeFlux-slowFlux);
    if(f.rms<1e-7f)continue;
    plan.fft(power,true);const float zero=std::max(1e-15f,power[0].real());
    const auto &window=hybridWindowCorrelation();
    const int first=std::max(2,int(rate/1500)),last=std::min(P::size/2-2,int(rate/55));
    const auto corr=[&](int lag){return std::clamp(power[lag].real()/(zero*window[lag]),-1.f,1.f);};
    float best=0;int chosen=0;
    for(int lag=first;lag<=last;++lag){const float v=corr(lag);
      if(v>=corr(lag-1)&&v>corr(lag+1)&&v>best){best=v;chosen=lag;}}
    if(best<.65f)continue;
    // Prefer the first credible periodic peak, avoiding sub-octave aliases.
    for(int lag=first;lag<=chosen;++lag){const float v=corr(lag);
      if(v>=best*.94f&&v>=corr(lag-1)&&v>corr(lag+1)){chosen=lag;break;}}
    const float a0=corr(chosen-1),b=corr(chosen),c=corr(chosen+1);
    const float denominator=a0-2*b+c;
    const float offset=std::abs(denominator)>1e-8f?std::clamp(.5f*(a0-c)/denominator,-.5f,.5f):0;
    f.hz=rate/(chosen+offset);f.confidence=std::clamp((b-.6f)/.3f,0.f,1.f);
  }
  return !cancel();
}
inline double hybridDonorPosition(const Recipe& r,std::size_t samples,
    std::size_t center,std::size_t frames,float rate){
  const int align=int(r.transfer[htAlign]);
  double sample=align==0?double(center)*samples/frames:double(center);
  sample+=double(r.transfer[htOffset])*rate*.001;
  if(align==2){sample=std::fmod(sample,double(samples));if(sample<0)sample+=samples;}
  return sample/ResynthPlan::hop;
}
template<class Cancel,class Progress>
bool renderHybridTransfer(const Recipe& r,float rate,Audio& out,
    const HybridInputs& inputs,Cancel cancel,Progress progress){
  using P=ResynthPlan;constexpr int n=P::size,hop=P::hop,bins=P::bins;
  const int count=hybridSourceCount(r);
  for(int i=0;i<count;++i)
    if(!inputs[i]||inputs[i]->left.empty()||inputs[i]->left.size()!=inputs[i]->right.size()||
       std::abs(inputs[i]->rate-rate)>.5f)return false;
  WorkshopSpan span(r,rate,out);
  std::array<std::unique_ptr<ResynthAnalysis>,4> analysis;
  std::array<HybridProfile,4> profiles;
  for(int i=0;i<count;++i){
    analysis[i]=std::make_unique<ResynthAnalysis>(ResynthAnalysis{*inputs[i],0,inputs[i]->left.size(),{}});
    if(!hybridAnalyze(*analysis[i],rate,profiles[i],cancel))return false;
    progress(unsigned((i+1)*25/count));
  }
  const auto &plan=P::get();
  std::vector<float> normalization(span.total,0);
  std::array<float,bins> phase{},previousDifference{};
  std::array<bool,bins> validPhase{},referenceRight{};
  float envelopeGain=1,logPitch=0;
  const float smoothing=1-std::exp(-hop/(rate*r.transfer[htSmooth]*.001f));
  const auto windows=(span.total+n/2+hop-1)/hop;
  for(std::size_t w=0;w<windows;++w){
    if(cancel())return false;
    progress(25+unsigned(w*65/windows));
    const auto center=w*hop;
    const float u=std::min(1.f,float(center)/std::max(1.f,float(span.frames-1))),seconds=float(center)/rate;
    const double sourcePosition=double(center)*inputs[0]->left.size()/span.frames/hop;
    const int index=int(sourcePosition);const float fraction=float(sourcePosition-index);
    // The three cache slots are not queried again until all A values are copied.
    const auto &before=analysis[0]->frame(index-1),&a=analysis[0]->frame(index),&b=analysis[0]->frame(index+1);
    std::array<float,bins> left{},right{},phaseL{},phaseR{},frequency{},colour{},tonal{};
    double sourceEnergy=0;
    for(int k=0;k<bins;++k){
      const auto mix=[fraction](float x,float y){return x+(y-x)*fraction;};
      left[k]=mix(a.left[k],b.left[k]);right[k]=mix(a.right[k],b.right[k]);
      phaseL[k]=a.phaseLeft[k]+wrapPhase(b.phaseLeft[k]-a.phaseLeft[k])*fraction;
      phaseR[k]=a.phaseRight[k]+wrapPhase(b.phaseRight[k]-a.phaseRight[k])*fraction;
      const bool ref=right[k]>left[k];
      const auto &old=ref?before.phaseRight:before.phaseLeft,&now=ref?a.phaseRight:a.phaseLeft,&next=ref?b.phaseRight:b.phaseLeft;
      const float base=Plan::tau*k/n,expected=base*hop;
      frequency[k]=base+mix(wrapPhase(now[k]-old[k]-expected),wrapPhase(next[k]-now[k]-expected))/hop;
      sourceEnergy+=double(left[k])*left[k]+double(right[k])*right[k];
    }
    const auto af=profiles[0].at(sourcePosition);
    const int es=int(r.transfer[htEnvSource]),ss=int(r.transfer[htSpecSource]),ps=int(r.transfer[htPitchSource]);
    std::array<double,4> positions{};std::array<HybridFeature,4> features{};
    for(int i=1;i<count;++i){positions[i]=hybridDonorPosition(r,inputs[i]->left.size(),center,span.frames,rate);features[i]=profiles[i].at(positions[i]);}
    const float envAmount=std::clamp(r.transfer[htEnvelope]+animatedCurve(r,1,u,seconds),0.f,1.f),
      specAmount=std::clamp(r.transfer[htSpectrum]+animatedCurve(r,2,u,seconds),0.f,1.f);
    const float donorLevel=features[es].rms/std::max(1e-8f,profiles[es].maximum),
      sourceLevel=af.rms/std::max(1e-8f,profiles[0].maximum);
    const float envTarget=1+envAmount*(std::clamp(donorLevel/std::max(.01f,sourceLevel),0.f,8.f)-1);
    if(w==0)envelopeGain=envTarget;else envelopeGain+=smoothing*(envTarget-envelopeGain);
    float pitchTarget=0;
    if(af.hz>0&&features[ps].hz>0)
      pitchTarget=std::clamp(std::log2(features[ps].hz/af.hz),-2.f,2.f)*
        std::min(af.confidence,features[ps].confidence)*r.transfer[htPitch];
    if(w==0)logPitch=pitchTarget;else logPitch+=smoothing*(pitchTarget-logPitch);
    const float variation=workshopRandom(r.seed^0x48595452u,seconds*2)*r.transfer[htRandom];
    const float pitch=std::clamp(std::exp2(logPitch+(r.transfer[htTune]+animatedCurve(r,0,u,seconds)+variation*.5f)/12),.125f,8.f);
    // Broad linked-channel envelopes transfer colour without exchanging peaks.
    std::array<float,bins> donor{};double donorEnergy=0;
    if(positions[ss]>=0&&positions[ss]<double(inputs[ss]->left.size())/hop){
      const int f=int(positions[ss]);const float q=float(positions[ss]-f);
      const auto &x=analysis[ss]->frame(f),&y=analysis[ss]->frame(f+1);
      for(int k=0;k<bins;++k){const float l=x.left[k]+q*(y.left[k]-x.left[k]),rr=x.right[k]+q*(y.right[k]-x.right[k]);
        donor[k]=std::sqrt(l*l+rr*rr);donorEnergy+=double(donor[k])*donor[k];}
    }
    const float donorScale=float(std::sqrt(sourceEnergy/std::max(1e-18,donorEnergy)));
    const bool donorPresent=features[ss].rms>std::max(1e-7f,profiles[ss].maximum*.001f);
    for(int k=0;k<bins;++k){
      const int radius=std::clamp(4+k/18,4,28);double sa=0,sb=0;int terms=0;
      for(int j=std::max(0,k-radius);j<=std::min(bins-1,k+radius);++j){
        sa+=std::sqrt(left[j]*left[j]+right[j]*right[j]);sb+=donor[j];++terms;}
      const float sourceEnvelope=float(sa/terms),donorEnvelope=float(sb/terms)*donorScale;
      const float gain=donorPresent?std::clamp(donorEnvelope/std::max(1e-6f,sourceEnvelope),.0631f,15.8489f):1.f;
      colour[k]=std::pow(gain,specAmount);
      const float peak=std::sqrt(left[k]*left[k]+right[k]*right[k]);
      tonal[k]=std::clamp((peak/std::max(1e-8f,sourceEnvelope)-1.f)*.65f,0.f,1.f);
    }
    P::Block outL{},outR{};
    const float preserve=std::clamp(af.onset*r.transfer[htAttack],0.f,1.f);
    const bool natural=std::abs(pitch-1)<1e-5f && inputs[0]->left.size()==span.frames;
    for(int k=1;k<bins-1;++k){
      const float at=k/pitch;const int j=int(at);
      if(j<1||j>=bins-1){validPhase[k]=false;continue;}
      const float f=at-j;const auto interp=[j,f](const auto &v){return v[j]+(v[j+1]-v[j])*f;};
      const auto angle=[j,f](const auto &v){return v[j]+wrapPhase(v[j+1]-v[j])*f;};
      const float l=interp(left),rr=interp(right),difference=wrapPhase(angle(phaseR)-angle(phaseL));
      const bool ref=rr>l*2;
      if(natural||!validPhase[k]||af.onset>.98f){phase[k]=angle(ref?phaseR:phaseL);}
      else {if(ref!=referenceRight[k])phase[k]+=ref?previousDifference[k]:-previousDifference[k];
        phase[k]=wrapPhase(phase[k]+std::clamp(interp(frequency)*pitch,0.f,Plan::tau*.5f)*hop);}
      referenceRight[k]=ref;previousDifference[k]=difference;
      validPhase[k]=std::max(l,rr)>1e-8f;
      const float residual=interp(tonal)+(1-interp(tonal))*r.transfer[htNoise];
      const float gain=interp(colour)*residual*envelopeGain;
      const auto pl=std::polar(l*gain,phase[k]-(ref?difference:0)),pr=std::polar(rr*gain,phase[k]+(ref?0:difference));
      outL[k]=pl*(1-preserve)+std::polar(left[k],phaseL[k])*preserve;
      outR[k]=pr*(1-preserve)+std::polar(right[k],phaseR[k])*preserve;
      outL[n-k]=std::conj(outL[k]);outR[n-k]=std::conj(outR[k]);
    }
    // Retain DC/Nyquist at the unshifted neutral setting as well.
    if(natural)for(int k:{0,bins-1}){
      outL[k]=std::polar(left[k],phaseL[k])*(envelopeGain*(1-preserve)+preserve);
      outR[k]=std::polar(right[k],phaseR[k])*(envelopeGain*(1-preserve)+preserve);
    }
    plan.fft(outL,true);plan.fft(outR,true);
    for(int i=0;i<n;++i){const auto at=std::int64_t(center)+i-n/2;
      if(at<0||at>=std::int64_t(span.total))continue;
      const float window=plan.window[i];const auto pos=std::size_t(at);
      out.left[pos]+=outL[i].real()*window;out.right[pos]+=outR[i].real()*window;
      normalization[pos]+=window*window;
    }
  }
  for(std::size_t i=0;i<span.total;++i){if((i&1023u)==0&&cancel())return false;
    if(normalization[i]>1e-8f){out.left[i]/=normalization[i];out.right[i]/=normalization[i];}}
  if(!finishWorkshop(r,rate,out,span,cancel,true))return false;
  progress(100);return true;
}
} // namespace s3g_thru_synth
