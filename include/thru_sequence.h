#pragma once
#include "thru_control_lfo.h"
#include "thru_sequence_sound_locks.h"
#include "thru_sequence_lock_catalog.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Scheduling adapted from s3g Opps src/core/engine.cpp and model.h,
// d3f4ed8d6e3b1c3d5299bc23212308cf932813aa (BSD-3-Clause, Shawn Greenlee).
// Thru adapter: four sample lanes, 32 steps, inherited trig values, no
// independent value cycles. Events use musical time and rounded sample offsets.
namespace s3g_thru_sequence {
constexpr int lanes = 4, steps = 32, children = 8;
constexpr std::array<double, 9> rates{4,       2,    1,        .5,   .25,
                                      1.0 / 6, .125, 1.0 / 12, .0625};
constexpr std::array<const char *, 9> rateNames{
    "1/1", "1/2", "1/4", "1/8", "1/16", "1/16T", "1/32", "1/32T", "1/64"};
struct Values {
  std::uint8_t mask = 0;
  std::array<float, 4> values{
      0, 0, 100, .8f}; // sample position, root offset, velocity, gate in steps
  bool timeLocked = false;
  float timeMs = 1000;
};
struct Child {
  float when = 0;
  Values locks;
};
struct Step {
  s3g_thru_lfo::TrigLocks lfos;
  bool enabled = false;
  float micro = 0, probability = 1;
  int count = 1;
  std::uint8_t cycleHit = 1, cycleEvery = 1; // 1:1 plays on every lane pass.
  Values locks;
  std::array<Child, children> events{};
};
inline constexpr std::array<float,21> generatorDefaults{1,8,0,0,1,0,1,1,16,-1,0,1,0,12,0,.5f,0,.25f,4,0,-1};
struct Generator {
  int type = 0; // Normal, Euclidean, Onset. One recipe per clip lane.
  int shift = 0; // Euclidean hit displacement; separate from playback ROT.
  bool ready = false, timed = false;
  std::array<float,21> values = generatorDefaults;
  std::array<Step,steps> base{};
  std::uint32_t enabledMask = 0, enabledValues = 0, burstMask = 0;
};
struct Lane {
  int length = 16, rate = 4, rotation = 0, direction = 0, path = 0;
  std::uint32_t directionSeed = 0;
  bool enabled = true, scaleEntry = false, timeMode = false;
  float velocity = 100, gate = .8f, timeMs = 1000;
  std::array<Step, steps> trigs{};
  Generator generator;
};
struct Strip {
  float pan = 0, lowDb = 0, midDb = 0, highDb = 0, midHz = 900;
  bool solo = false, muted = false;
};
struct Mix {
  std::array<Strip, lanes> strips{};
  int priority = 0; // Off, or the one-based protected lane.
  float depthDb = 0, thresholdDb = -24, attackMs = 12, holdMs = 40;
  float releaseMs = 220, hpfHz = 100, kneeDb = 6;
};
struct Pattern {
  SoundLocks soundLocks;
  bool active = false, initialized = false;
  std::array<Lane, lanes> tracks{};
  Mix mix{};
  std::array<Mix,7> modeMixes{};
  int mixerMode = 0;
};
inline std::uint32_t hash(std::uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}
// Grid geometry is separate from travel direction. Precomputed byte tables
// cover partial rows without allocation or route construction on audio render.
struct GridPathOrder {
  std::array<std::uint8_t,32> cells{},ordinals{};
};
constexpr GridPathOrder makeGridPath(int path,int length){
  GridPathOrder result{};int count=0;
  if(path==0){
    for(int i=0;i<length;++i)result.cells[count++]=std::uint8_t(i);
  } else if(path==1){
    for(int column=0;column<8;++column)for(int row=0;row<4;++row){
      const int cell=row*8+column;
      if(cell<length)result.cells[count++]=std::uint8_t(cell);
    }
  } else {
    for(int band=0;band<11;++band)for(int row=0;row<4;++row){
      const int column=band-row,cell=row*8+column;
      if(column>=0&&column<8&&cell<length)result.cells[count++]=std::uint8_t(cell);
    }
  }
  for(int i=0;i<count;++i)result.ordinals[result.cells[i]]=std::uint8_t(i);
  return result;
}
constexpr auto makeGridPaths(){
  std::array<std::array<GridPathOrder,32>,3> result{};
  for(int path=0;path<3;++path)for(int length=1;length<=32;++length)
    result[path][length-1]=makeGridPath(path,length);
  return result;
}
inline constexpr auto gridPaths=makeGridPaths();
inline const GridPathOrder& gridPath(int path,int length){
  return gridPaths[std::clamp(path,0,2)][std::clamp(length,1,32)-1];
}
// Direction changes the visited cell, while clock ticks remain monotonic.
// Every trig carries its own pitch/position/velocity/duration and Burst data.
inline int directionPeriod(int length,int direction) {
  if(length<=1)return 1;
  return direction==2?2*(length-1):direction==3?2*length:length;
}
inline int directionPeriod(const Lane& lane,int direction) {
  return directionPeriod(lane.length,direction);
}
struct DirectionPosition { int step=0; std::int64_t cycle=0; };
inline DirectionPosition directionPosition(int length,int rotation,std::uint32_t seed,
    std::int64_t tick,int direction,std::int64_t phase,std::int64_t cycle,int path) {
  length=std::max(1,length);const int period=directionPeriod(length,direction);
  const int p=int(phase%period);int ordinal=p;
  if(direction==1)ordinal=length-1-p;
  else if(direction==2)ordinal=p<length?p:period-p;
  else if(direction==3)ordinal=p<length?p:period-1-p;
  else if(direction==4)ordinal=int(hash(0x44495231u^seed^
      std::uint32_t(tick)^(std::uint32_t(tick>>32)*0x9e3779b9u))%std::uint32_t(length));
  return {gridPath(path,length).cells[(ordinal+rotation)%length],cycle+phase/period};
}
inline DirectionPosition directionPosition(const Lane& lane,std::int64_t tick,
    int direction,std::int64_t phase,std::int64_t cycle=0,int path=-1) {
  return directionPosition(lane.length,lane.rotation,lane.directionSeed,tick,
      direction,phase,cycle,path<0?lane.path:path);
}
inline DirectionPosition directionPosition(const Lane& lane,std::int64_t tick) {
  return directionPosition(lane,tick,lane.direction,tick);
}
// History keeps late Burst/early MICRO tied to all committed route geometry.
// Live edits coalesce at the next uncommitted tick, anchored to the last pad.
class DirectionTraversal {
  struct Route {
    std::int64_t tick=0,cycle=0;
    int phase=0,direction=0,path=0,length=16,rotation=0;
    std::uint32_t seed=0;
  };
  std::array<Route,8> routes_{};
  struct Commit {std::int64_t tick=-1;int row=0;bool protect=false;Step value;};
  std::array<Commit,3> committed_{};
  int count_=0;
  const Route& routeAt(std::int64_t tick) const {
    const Route* route=&routes_[0];
    for(int i=1;i<count_&&routes_[i].tick<=tick;++i)route=&routes_[i];
    return *route;
  }
 public:
  void reset(const Lane& lane){
    routes_={};routes_[0]={0,0,0,lane.direction,lane.path,lane.length,lane.rotation,lane.directionSeed};
    count_=1;committed_={};
  }
  void remember(const Lane& lane,std::int64_t tick,int row){
    auto& commit=committed_[std::uint64_t(tick)%committed_.size()];
    if(commit.tick==tick)return;
    commit.tick=tick;commit.row=row;commit.protect=false;commit.value=lane.trigs[row];
  }
  const Step& step(const Lane& lane,std::int64_t tick,int row) const {
    const auto& commit=committed_[std::uint64_t(tick)%committed_.size()];
    return commit.protect&&commit.tick==tick&&commit.row==row?commit.value:lane.trigs[row];
  }
  DirectionPosition at(const Lane& lane,std::int64_t tick) const {
    if(!count_)return directionPosition(lane,tick);
    const auto& route=routeAt(tick);
    return directionPosition(route.length,route.rotation,route.seed,tick,route.direction,
        std::max<std::int64_t>(0,tick-route.tick+route.phase),route.cycle,route.path);
  }
  void synchronize(const Lane& lane,std::int64_t tick,std::int64_t emitted) {
    if(!count_){reset(lane);return;}
    const auto& target=routes_[count_-1];
    if(target.direction==lane.direction&&target.path==lane.path
        &&target.length==lane.length&&target.rotation==lane.rotation
        &&target.seed==lane.directionSeed)return;
    const auto start=std::max(tick+1,emitted+1);
    // A recipe can rebuild or disable the current cell on LEN edits. Only
    // geometry changes protect a started tick; ordinary trig edits stay live.
    for(auto& commit:committed_)if(commit.tick>=std::max<std::int64_t>(0,tick-2)&&commit.tick<start)commit.protect=true;
    const auto previous=at(lane,start-1);
    const auto& old=routeAt(start-1);
    const int length=std::max(1,lane.length),period=directionPeriod(lane,lane.direction);
    const auto oldPhase=(start-1-old.tick+old.phase)%directionPeriod(old.length,old.direction);
    int phase=int(oldPhase);
    if(old.direction!=lane.direction||old.path!=lane.path){
      const bool descending=old.direction==lane.direction&&oldPhase>=old.length;
      const int anchor=std::min(previous.step,length-1);
      const int ordinal=(gridPath(lane.path,length).ordinals[anchor]-lane.rotation%length+length)%length;
      phase=lane.direction==1?length-1-ordinal:ordinal;
      if(descending&&lane.direction==2&&ordinal>0)phase=period-ordinal;
      if(descending&&lane.direction==3)phase=period-1-ordinal;
    }
    // ROT and LEN keep traversal phase; DIR/PATH continue from the last pad.
    const int next=phase+1;
    // Keep the route covering the scheduler's two-tick lookback and any
    // already-emitted early event. Repeated pending edits replace each other.
    while(count_>1&&routes_[1].tick<=tick-2){
      for(int i=1;i<count_;++i)routes_[i-1]=routes_[i];
      --count_;
    }
    while(count_>1&&routes_[count_-1].tick>=start)--count_;
    if(count_==int(routes_.size())){
      for(int i=1;i<count_;++i)routes_[i-1]=routes_[i];
      --count_;
    }
    routes_[count_++]={start,previous.cycle+next/period,next%period,lane.direction,
        lane.path,lane.length,lane.rotation,lane.directionSeed};
  }
};
// Nominal rhythm spacing ignores chance/cycle suppression. Include adjacent
// ticks because MICRO and late Burst events can overlap their neighbours.
// RAND has a fixed 128-tick horizon; no hit returns infinity (caller uses MAX).
inline double nextHitGap(const Lane& lane,int row,int child,std::int64_t tick,
                         const DirectionTraversal* traversal=nullptr) {
  const auto& current=traversal?traversal->step(lane,tick,row):lane.trigs[row];
  const double authored=tick+double(current.micro)+current.events[child].when;
  const double origin=tick?authored:std::max(0.0,authored);
  double best=INFINITY;
  for(auto n=std::max<std::int64_t>(0,tick-1);n<=tick+128;++n){
    if(double(n)-.5>origin+best+1e-9)break;
    const int cell=traversal?traversal->at(lane,n).step:directionPosition(lane,n).step;
    const auto& step=traversal?traversal->step(lane,n,cell):lane.trigs[cell];if(!step.enabled)continue;
    for(int c=0;c<step.count;++c){
      if(n==tick&&cell==row&&c==child)continue;
      const double time=n+double(step.micro)+step.events[c].when;
      const double delta=(n?time:std::max(0.0,time))-origin;
      if(delta>=-1e-9)best=std::min(best,std::max(0.0,delta));
    }
  }
  return best;
}
inline int padStep(int pad) { return (3 - pad / 8) * 8 + pad % 8; }
inline int stepPad(int step) { return (3 - step / 8) * 8 + step % 8; }
inline float resolve(const Lane &lane, const Step &step, int child, int field,
                     float inherited) {
  float value = field == 2 ? lane.velocity : field == 3 ? lane.gate : inherited;
  if (step.locks.mask & (1u << field))
    value = step.locks.values[field];
  if (child >= 0 && (step.events[child].locks.mask & (1u << field)))
    value = step.events[child].locks.values[field];
  return value;
}
inline float resolveTime(const Lane &lane, const Step &step, int child) {
  float ms = lane.timeMs;
  if (step.locks.timeLocked)
    ms = step.locks.timeMs;
  if (child >= 0 && step.events[child].locks.timeLocked)
    ms = step.events[child].locks.timeMs;
  return ms;
}
inline void setCount(Step &step, int count) {
  step.count = std::clamp(count, 1, children);
  for (int i = 0; i < step.count; ++i)
    step.events[i].when = float(i) / step.count;
}
struct Event {
  int offset = 0, lane = 0, step = 0, child = 0;
  double gateBeats = 0;
  std::int64_t tick = 0;
};
struct EventList {
  std::array<Event, 128> items{};
  int size = 0;
  bool overflow = false;
};
inline void schedule(const Pattern &pattern, double start, double increment,
                     int frames, EventList &out,
                     std::array<double, lanes> beatOffsets = {},
                     const std::array<DirectionTraversal,lanes>* traversal = nullptr) {
  out.size = 0;
  out.overflow = false;
  for (int t = 0; t < lanes; ++t) {
    const auto &track = pattern.tracks[t];
    if (!track.enabled)
      continue;
    const double span = rates[track.rate];
    const double localStart = start + beatOffsets[t];
    const auto first = std::int64_t(std::floor(localStart / span)) - 2;
    const auto last =
        std::int64_t(std::ceil((localStart + frames * increment) / span)) + 1;
    for (auto n = std::max<std::int64_t>(0, first); n <= last; ++n) {
      const auto position=traversal?(*traversal)[t].at(track,n):directionPosition(track,n);
      const int row=position.step;
      const auto &step = traversal?(*traversal)[t].step(track,n,row):track.trigs[row];
      if (!step.enabled || step.probability <= 0 ||
          position.cycle % step.cycleEvery != step.cycleHit - 1)
        continue;
      const auto random =
          hash(1979u ^ std::uint32_t(n) ^ std::uint32_t(n >> 32) ^
               (0x9e3779b9u * (t + 1)));
      if (double(random) / 4294967296.0 >= step.probability)
        continue;
      for (int c = 0; c < step.count; ++c) {
        const double authored = (n + step.micro + step.events[c].when) * span;
        const double onset = n == 0 ? std::max(0.0, authored) : authored;
        const double relative = (onset - localStart) / increment;
        if (relative < -1 || relative > frames + 1)
          continue;
        const int offset = int(std::floor(relative + .5 + 1e-7));
        if (offset < 0 || offset >= frames)
          continue;
        if (out.size == int(out.items.size())) {
          out.overflow = true;
          continue;
        }
        out.items[out.size++] = {
            offset, t, row, c, span * resolve(track, step, c, 3, track.gate),
            n};
      }
    }
  }
  std::sort(out.items.begin(), out.items.begin() + out.size,
            [](const auto &a, const auto &b) {
              if (a.offset != b.offset)
                return a.offset < b.offset;
              if (a.lane != b.lane)
                return a.lane < b.lane;
              if (a.step != b.step)
                return a.step < b.step;
              return a.child < b.child;
            });
}
// Fixed extension encoding, independent of C++ struct layout. Existing
// formats omit this whole extension and construct a disabled Pattern.
constexpr int valuesSize = 5, childSize = 6,
              stepSize = 9 + children * childSize;
constexpr int laneSize = 7 + steps * stepSize,
              patternSize = 1 + lanes * laneSize;
inline float &scalar(Pattern &p, int index, float &scratch) {
  if (index == 0) {
    scratch = p.active ? 1.f : 0.f;
    return scratch;
  }
  --index;
  auto &lane = p.tracks[index / laneSize];
  index %= laneSize;
  if (index < 7) {
    switch (index) {
    case 0:
      scratch = float(lane.length);
      break;
    case 1:
      scratch = float(lane.rate);
      break;
    case 2:
      scratch = float(lane.rotation);
      break;
    case 3:
      scratch = lane.enabled ? 1.f : 0.f;
      break;
    case 4:
      return lane.velocity;
    case 5:
      return lane.gate;
    default:
      scratch = lane.scaleEntry ? 1.f : 0.f;
    }
    return scratch;
  }
  index -= 7;
  auto &step = lane.trigs[index / stepSize];
  index %= stepSize;
  if (index == 0) {
    scratch = step.enabled ? 1.f : 0.f;
    return scratch;
  }
  if (index == 1)
    return step.micro;
  if (index == 2)
    return step.probability;
  if (index == 3) {
    scratch = float(step.count);
    return scratch;
  }
  index -= 4;
  auto *lock = &step.locks;
  if (index >= 5) {
    index -= 5;
    auto &child = step.events[index / childSize];
    index %= childSize;
    if (index == 0)
      return child.when;
    --index;
    lock = &child.locks;
  }
  if (index == 0) {
    scratch = float(lock->mask);
    return scratch;
  }
  return lock->values[index - 1];
}
inline float get(const Pattern &p, int index) {
  float scratch = 0;
  return scalar(const_cast<Pattern &>(p), index, scratch);
}
inline bool set(Pattern &p, int index, float value) {
  if (!std::isfinite(value) || index < 0 || index >= patternSize)
    return false;
  const auto integer = [&](int low, int high) {
    return value >= low && value <= high && std::floor(value) == value;
  };
  if (index == 0) {
    if (!integer(0, 1))
      return false;
    p.active = value != 0;
    return true;
  }
  --index;
  auto &lane = p.tracks[index / laneSize];
  index %= laneSize;
  if (index < 7) {
    switch (index) {
    case 0:
      if (!integer(1, 32))
        return false;
      lane.length = int(value);
      break;
    case 1:
      if (!integer(0, 8))
        return false;
      lane.rate = int(value);
      break;
    case 2:
      if (!integer(0, 31))
        return false;
      lane.rotation = int(value);
      break;
    case 3:
      if (!integer(0, 1))
        return false;
      lane.enabled = value != 0;
      break;
    case 4:
      if (!integer(1, 127))
        return false;
      lane.velocity = value;
      break;
    case 5:
      if (value < .01f || value > 32)
        return false;
      lane.gate = value;
      break;
    default:
      if (!integer(0, 1))
        return false;
      lane.scaleEntry = value != 0;
    }
    return true;
  }
  index -= 7;
  auto &step = lane.trigs[index / stepSize];
  index %= stepSize;
  if (index == 0) {
    if (!integer(0, 1))
      return false;
    step.enabled = value != 0;
    return true;
  }
  if (index == 1) {
    if (value < -.5f || value > .5f)
      return false;
    step.micro = value;
    return true;
  }
  if (index == 2) {
    if (value < 0 || value > 1)
      return false;
    step.probability = value;
    return true;
  }
  if (index == 3) {
    if (!integer(1, 8))
      return false;
    step.count = int(value);
    return true;
  }
  index -= 4;
  Values *lock = &step.locks;
  if (index >= 5) {
    index -= 5;
    auto &child = step.events[index / childSize];
    index %= childSize;
    if (index == 0) {
      if (value < 0 || value > .999f)
        return false;
      child.when = value;
      return true;
    }
    --index;
    lock = &child.locks;
  }
  if (index == 0) {
    if (!integer(0, 15))
      return false;
    lock->mask = std::uint8_t(value);
    return true;
  }
  const float lo[]{0, -48, 1, .01f}, hi[]{1, 48, 127, 32};
  const int field = index - 1;
  if (value < lo[field] || value > hi[field] ||
      ((field == 1 || field == 2) && std::floor(value) != value))
    return false;
  lock->values[field] = value;
  return true;
}
inline constexpr int laneFields=12;
inline constexpr int laneCurveOffset(int part,int field){return field<8?part*8+field:field<10?32+part*2+field-8:field==10?40+part:128+part;}
struct Voice {
  SoundLockSet soundLocks;
  bool active = false, audition = false, generatedPreview = false;
  int step = -1, child = -1;
  float position = 0, startPhase = 0, attack = 0, decay = 0, release = 3;
  std::uint64_t elapsed = 0, frames = 0, releaseAt = 0;
};
} // namespace s3g_thru_sequence
