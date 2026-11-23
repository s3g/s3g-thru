#pragma once
#include <algorithm>
#include <cmath>

namespace s3g_thru_reson {
// Part-owned stereo-linked overlap compression. No added delay, makeup,
// clipping, allocation or shared detector with other engines.
struct Density {
  float envelope=0, gain=1, amount=0;
  float attack=1, release=1, slew=1, gainSlew=1;
  float targetGain=1;
  unsigned countdown=0;
  void reset() { envelope=0;gain=targetGain=1;amount=0;countdown=0; }
  void prepare(float sr) {
    attack=1-std::exp(-1/(sr*.003f));
    release=1-std::exp(-1/(sr*.150f));
    slew=1-std::exp(-1/(sr*.005f));
    gainSlew=1-std::exp(-1/(sr*.001f));
    reset();
  }
  float process(float left,float right,float percent) {
    const float requested=std::isfinite(percent)?std::clamp(percent*.01f,0.f,1.f):0.f;
    amount+=(requested-amount)*slew;
    if(std::abs(requested-amount)<1e-6f)amount=requested;
    const float peak=std::max(std::abs(left),std::abs(right));
    envelope+=(peak-envelope)*(peak>envelope?attack:release);
    if(envelope<1e-20f)envelope=0;
    // A 6 dB knee at -6 dBFS, progressing from unity to 3:1.
    // Refreshing this slow gain law at 16 samples bounds logarithm cost.
    if(countdown++%16==0) {
      float reduction=0;
      if(amount>0 && envelope>.000001f) {
        const float over=20*std::log10(envelope)+6;
        const float knee=over<=-3?0:over>=3?over:(over+3)*(over+3)/12;
        reduction=std::min(12.f,knee*(2.f/3)*amount);
      }
      targetGain=reduction>0?std::pow(10.f,-reduction*.05f):1.f;
    }
    gain+=(targetGain-gain)*gainSlew;
    if(targetGain==1 && gain>1-4e-6f)gain=1;
    // At zero, eventually regain exact unity, including after a live bypass.
    if(requested==0 && amount==0 && gain>1-1e-6f)gain=targetGain=1;
    return gain;
  }
};
} // namespace s3g_thru_reson
