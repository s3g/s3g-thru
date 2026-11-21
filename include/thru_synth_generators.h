#pragma once
#include "thru_spline.h"
#include "thru_synth_source.h"
namespace s3g_thru_synth {
inline void bandlimit(Table &out, int harmonics) {
    std::array<Plan::Complex, Plan::size> spec{};
    for (std::size_t i = 0; i < out.size(); ++i)
        spec[i].real = out[i];
    Plan::get().fft(spec);
    spec[0] = {};
    spec[Plan::size / 2] = {};
    for (int i = harmonics + 1; i < int(Plan::size / 2); ++i) {
        spec[i] = {};
        spec[Plan::size - i] = {};
    }
    Plan::get().fft(spec, true);
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = spec[i].real;
}
inline float tablePhase(double p) { return float(p - std::floor(p)); }
struct Lowpass {
    float a = 0, b = 0;
    float tick(float in, float cutoff, float resonance, float rate) {
        const float g = std::tan(3.14159265359f * std::clamp(cutoff, 20.f, rate * .44f) / rate),
                    k = 2 - 1.95f * resonance;
        const float v1 = (a + g * (in - b)) / (1 + g * (g + k)), v2 = b + g * v1;
        a = 2 * v1 - a;
        b = 2 * v2 - b;
        return v2;
    }
};
inline float fileEnvelope(const Recipe &r, std::size_t i, std::size_t frames, std::size_t splice,
                          float rate) {
    if (splice)
        return 1;
    const double attack = r.v[s3g_thru_synth::attack] * rate * .001,
                 release = r.v[s3g_thru_synth::release] * rate * .001;
    const double scale = std::min(1.0, double(frames - 1) / std::max(1.0, attack + release));
    const float t = float(std::min({1.0, double(i) / std::max(1.0, attack * scale),
                                    double(frames - 1 - i) / std::max(1.0, release * scale)}));
    return t * t * (3 - 2 * t);
}
template <class Cancel, class Progress>
bool renderWave(const Recipe &r, float rate, Audio &out, Cancel cancel, Progress progress) {
    const auto frames = std::size_t(std::llround(r.v[length] * rate)),
               splice = r.v[loop] > .5f
                            ? std::min<std::size_t>(frames / 4, std::size_t(rate * .015f))
                            : 0,
               total = frames + splice;
    out.left.resize(total);
    out.right.resize(total);
    out.recipe = r;
    float high = 0;
    for (int i = 0; i < 8; ++i)
        if (r.curves[0].enabled & (1u << i))
            high = std::max(high, r.curves[0].p[i].value);
    const float hzMax =
        440 *
        std::exp2((r.v[root] + high +
                   (modActive(r) && r.mod[mDestination] == 1 ? 24 * r.mod[mAmount] : 0) - 69) /
                  12) *
        std::exp2(r.osc[wDetune] / 1200);
    const int h = std::clamp(int(rate * .44f / std::max(1.f, hzMax)), 1, 128);
    Table sine{}, triangle{}, saw{}, subSine{};
    for (std::size_t i = 0; i < Plan::size; ++i) {
        const float p = float(i) / Plan::size;
        sine[i] = std::sin(Plan::tau * p);
        triangle[i] = 2 / 3.14159265359f * std::asin(sine[i]);
        saw[i] = 2 * p - 1;
    }
    subSine = sine;
    bandlimit(triangle, h);
    bandlimit(saw, h);
    Lowpass filters[2];
    double phase = 0, detuned = 0, subPhase = 0;
    float pulse = r.osc[wPulse], cutoff = r.osc[wCutoff],
          levelGain = std::pow(10.f, animatedCurve(r, 3, 0, 0) / 20),
          slew = 1 - std::exp(-1 / (rate * .003f));
    const bool filtered = toneActive(r);
    const auto osc = [&](double p) {
        if (r.osc[wShape] == 0)
            return lookup(sine, p);
        if (r.osc[wShape] == 1)
            return lookup(triangle, p);
        if (r.osc[wShape] == 2)
            return lookup(saw, p);
        return (lookup(saw, p) - lookup(saw, tablePhase(p + pulse))) /
               (2 * std::max(pulse, 1 - pulse));
    };
    for (std::size_t i = 0; i < total; ++i) {
        if ((i & 1023u) == 0) {
            if (cancel())
                return false;
            progress(unsigned(i * 88 / total));
        }
        const float t = std::min(1.f, float(i % frames) / std::max(1.f, float(frames - 1))),
                    seconds = float(i) / rate;
        pulse += (std::clamp(r.osc[wPulse] + animatedCurve(r, 1, t, seconds), .05f, .95f) - pulse) *
                 slew;
        cutoff += (std::clamp(r.osc[wCutoff] * std::exp2(animatedCurve(r, 2, t, seconds)), 20.f,
                              20000.f) -
                   cutoff) *
                  slew;
        levelGain += (std::pow(10.f, animatedCurve(r, 3, t, seconds) / 20) - levelGain) * slew;
        const float hz = std::min(
            rate * .44f, 440 * std::exp2((r.v[root] + animatedCurve(r, 0, t, seconds) - 69) / 12));
        const float a = osc(phase), b = osc(detuned), mono = (a + b) * .5f,
                    sub = lookup(subSine, subPhase) * r.osc[wSub];
        float left = mono + (a - mono) * r.v[width] + sub,
              right = mono + (b - mono) * r.v[width] + sub;
        const float nl = 2 * random(r.seed ^ 0x4e4f4953u, std::uint32_t(i)) - 1,
                    nr = 2 * random(r.seed ^ 0x524e4f49u, std::uint32_t(i)) - 1;
        left += nl * r.osc[wNoise];
        right += (nl + (nr - nl) * r.v[width]) * r.osc[wNoise];
        if (filtered) {
            left = filters[0].tick(left, cutoff, r.osc[wResonance], rate);
            right = filters[1].tick(right, cutoff, r.osc[wResonance], rate);
        }
        const float gain = fileEnvelope(r, i, frames, splice, rate) * levelGain;
        out.left[i] = left * gain;
        out.right[i] = right * gain;
        phase = tablePhase(phase + hz / rate);
        detuned = tablePhase(detuned + hz / rate * std::exp2(r.osc[wDetune] / 1200));
        subPhase = tablePhase(subPhase + hz / (rate * 2));
    }
    finishSound(r, rate, out, frames, splice);
    progress(100);
    return !cancel();
}
inline float packetEnvelope(std::size_t i, std::size_t total, float rise, float sustain,
                            float decay, float shape, float rate) {
    const float ms = float(i) * 1000 / rate;
    float e = 1;
    if (ms < rise)
        e = std::pow(std::clamp(ms / rise, 0.f, 1.f), shape);
    else if (ms > rise + sustain)
        e = std::pow(std::clamp((rise + sustain + decay - ms) / decay, 0.f, 1.f), shape);
    const float edge = std::clamp(
        std::min(float(i), float(total - 1 - i)) / std::max(1.f, rate * .002f), 0.f, 1.f);
    return e * edge * edge * (3 - 2 * edge);
}
template <class Cancel, class Progress>
bool renderBloom(const Recipe &r, float rate, Audio &out, Cancel cancel, Progress progress) {
    const auto frames = std::size_t(std::llround(r.v[length] * rate)),
               splice = r.v[loop] > .5f
                            ? std::min<std::size_t>(frames / 4, std::size_t(rate * .015f))
                            : 0,
               total = frames + splice;
    out.left.assign(total, 0);
    out.right.assign(total, 0);
    out.recipe = r;
    const float maximumDuration =
        std::min(200.f, r.packets[bRise] * (1 + r.packets[bRiseVar])) +
        std::min(200.f, r.packets[bSustain] * (1 + r.packets[bSustainVar])) +
        std::min(2000.f, r.packets[bDecay] * (1 + r.packets[bDecayVar]));
    const float minimumGap = std::max(5.f, maximumDuration / 32);
    std::vector<float> packet, envelope;
    std::size_t onset = 0;
    std::uint32_t event = 0;
    while (onset < total) {
        if (cancel())
            return false;
        progress(unsigned(onset * 80 / total));
        const float t = std::min(1.f, float(onset % frames) / std::max(1.f, float(frames - 1))),
                    seconds = float(onset) / rate;
        const auto rand = [&](int salt) {
            return random(r.seed ^ hash(event + 1), std::uint32_t(salt));
        };
        const auto timing = [&](int base, int variance) {
            return std::clamp(r.packets[base] *
                                  (1 + (rand(base + 5) * 2 - 1) * r.packets[variance]),
                              bloomMin[base], bloomMax[base]);
        };
        const float rise = timing(bRise, bRiseVar), sus = timing(bSustain, bSustainVar),
                    decay = timing(bDecay, bDecayVar);
        const auto duration =
            std::max<std::size_t>(2, std::size_t(std::llround((rise + sus + decay) * rate * .001)));
        packet.assign(duration, 0);
        envelope.resize(duration);
        const float pitch = r.v[root] + animatedCurve(r, 0, t, seconds) +
                            (rand(71) * 2 - 1) * r.packets[bPitchJitter];
        const float count =
            std::clamp(r.packets[bCount] + animatedCurve(r, 1, t, seconds), 1.f, 8.f);
        int a = int(r.packets[bSpectrum]), b = a;
        float blend = 0;
        if (r.packets[bMode] == 1) {
            const float at = t * 3;
            a = std::min(3, int(at));
            b = std::min(3, a + 1);
            blend = at - a;
            blend = blend * blend * (3 - 2 * blend);
        }
        if (r.packets[bMode] == 2)
            a = b = std::min(3, int(rand(72) * 4));
        for (int partial = 0; partial < 8; ++partial) {
            const float weight = std::clamp(count - partial, 0.f, 1.f);
            if (weight == 0)
                break;
            const float ratio = r.ratios[a][partial] +
                                (r.ratios[b][partial] - r.ratios[a][partial]) * blend,
                        amp = (r.partials[a][partial] +
                               (r.partials[b][partial] - r.partials[a][partial]) * blend) *
                              weight;
            for (int octave = 0; octave < 2; ++octave) {
                const float gain = amp * (octave ? r.packets[bOctave] : 1.f);
                if (std::abs(gain) < 1e-8f)
                    continue;
                const double hz = 440 * std::exp2((pitch - 69) / 12) * ratio * (octave ? .5 : 1),
                             highest = hz * std::exp2(r.packets[bBend] / 12);
                const float alias =
                    std::clamp(float((rate * .44 - highest) / (rate * .06)), 0.f, 1.f);
                if (alias == 0)
                    continue;
                double sin = 0, cos = 1, cs = 1, sn = 0;
                for (std::size_t i = 0; i < duration; ++i) {
                    if ((i & 1023u) == 0 && cancel())
                        return false;
                    if ((i & 31u) == 0) {
                        const double omega =
                            Plan::tau * hz *
                            std::exp2(r.packets[bBend] * double(i) / duration / 12) / rate;
                        cs = std::cos(omega);
                        sn = std::sin(omega);
                    }
                    packet[i] += float(sin) * gain * alias;
                    const double next = sin * cs + cos * sn;
                    cos = cos * cs - sin * sn;
                    sin = next;
                }
            }
        }
        double dc = 0, sum = 0;
        for (std::size_t i = 0; i < duration; ++i) {
            envelope[i] = packetEnvelope(i, duration, rise, sus, decay, r.packets[bShape], rate);
            dc += packet[i] * envelope[i];
            sum += envelope[i];
        }
        dc /= std::max(1e-9, sum);
        const float amp = 1 - r.packets[bAmpJitter] * rand(73),
                    pan = (rand(74) * 2 - 1) * r.v[width], left = 1 - std::max(0.f, pan),
                    right = 1 + std::min(0.f, pan);
        for (std::size_t i = 0; i < duration && onset + i < total; ++i) {
            const float v = (packet[i] - float(dc)) * envelope[i] * amp;
            out.left[onset + i] += v * left;
            out.right[onset + i] += v * right;
        }
        const float gap = r.packets[bGap] * std::exp2(animatedCurve(r, 2, t, seconds)) *
                          (1 + (rand(75) * 2 - 1) * r.packets[bScatter] * .9f);
        onset += std::max<std::size_t>(std::size_t(std::ceil(rate * minimumGap * .001f)),
                                       std::size_t(std::llround(gap * rate * .001)));
        ++event;
    }
    float gain = std::pow(10.f, animatedCurve(r, 3, 0, 0) / 20),
          slew = 1 - std::exp(-1 / (rate * .003f));
    for (std::size_t i = 0; i < total; ++i) {
        const float t = std::min(1.f, float(i % frames) / std::max(1.f, float(frames - 1)));
        gain += (std::pow(10.f, animatedCurve(r, 3, t, float(i) / rate) / 20) - gain) * slew;
        const float g = gain * fileEnvelope(r, i, frames, splice, rate);
        out.left[i] *= g;
        out.right[i] *= g;
    }
    finishSound(r, rate, out, frames, splice);
    progress(100);
    return !cancel();
}
} // namespace s3g_thru_synth
#include "thru_resynth.h"
#include "thru_synth_banks.h"
#include "thru_convolution.h"
#include "thru_synth_drum.h"
#include "thru_synth_workshop.h"
#include "thru_synth_dust.h"
#include "thru_synth_wt_drum.h"
namespace s3g_thru_synth {
template <class Cancel, class Progress>
bool renderBase(const Recipe &r, float rate, Audio &out, Cancel cancel, Progress progress,
            const SourceAudio *source,const SourceAudio*impulse,const HybridInputs*inputs) {
    if (!valid(r) || !std::isfinite(rate) || rate < 8000 || rate > 192000)
        return false;
    if(r.type==pluck)return renderPluck(r,rate,out,cancel,progress);
    if(r.type==subharmonic)return renderSubharmonic(r,rate,out,cancel,progress);
    if(r.type==hybrid)return inputs&&renderHybrid(r,rate,out,*inputs,cancel,progress);
    if(r.type==convolution)return source&&impulse&&renderConvolution(r,rate,out,*source,*impulse,cancel,progress);
    if(r.type==wtDrum)return renderWtDrum(r,rate,out,cancel,progress);
    if(r.type==dust)return renderDust(r,rate,out,cancel,progress);
    if(isDrumType(r.type))return renderDrum(r,rate,out,cancel,progress);
    if (r.type == partialsType) return renderPartials(r,rate,out,cancel,progress);
    if (r.type == noiseBank) return renderNoiseBank(r,rate,out,cancel,progress);
    if (r.type == fm) return renderFm(r,rate,out,cancel,progress);
    if (r.type == wave)
        return renderWave(r, rate, out, cancel, progress);
    if (r.type == bloom)
        return renderBloom(r, rate, out, cancel, progress);
    if (r.type == resynth)
        return source && renderResynth(r, rate, out, *source, cancel, progress);
    return renderSpline(r, rate, out, cancel, progress);
}
template<class Cancel,class Progress>
bool render(const Recipe&r,float rate,Audio&out,Cancel cancel,Progress progress,const SourceAudio*source=nullptr,const SourceAudio*impulse=nullptr,const HybridInputs*inputs=nullptr){
  if(!valid(r)||!std::isfinite(rate)||rate<8000||rate>192000)return false;
  if(supportsEvents(r.type)&&r.events[eMode]>0)return renderEventField(r,rate,out,cancel,progress,[&](const Recipe&child,Audio&audio,auto report){return renderBase(child,rate,audio,cancel,report,nullptr,nullptr,nullptr);});
  return renderBase(r,rate,out,cancel,progress,source,impulse,inputs);
}
} // namespace s3g_thru_synth
