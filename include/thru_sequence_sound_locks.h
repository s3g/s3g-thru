#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace s3g_thru_sequence {
inline constexpr int soundLockCapacity=1024, soundLocksPerStep=32;
struct SoundValue {std::uint16_t id=0;float value=0;};
struct SoundLockSet {
  std::array<SoundValue,soundLocksPerStep> values{};
  std::uint8_t count=0;
  const SoundValue* find(int id)const {for(int i=0;i<count;++i)if(values[i].id==id)return &values[i];return nullptr;}
  void put(int id,float value){for(int i=0;i<count;++i)if(values[i].id==id){values[i].value=value;return;}if(count<values.size())values[count++]={std::uint16_t(id),value};}
};
struct SoundRecord {std::uint16_t route=0,id=0;float value=0;};
struct StepSoundClipboard {std::array<SoundRecord,soundLocksPerStep> records{};std::uint8_t count=0;};
struct SoundLocks {
  std::array<SoundRecord,soundLockCapacity> records{};
  std::uint16_t count=0;
  static int route(int part,int step,int child){return (part*32+step)*9+child;}
  static std::uint32_t key(int route,int id){return (std::uint32_t(route)<<16)|std::uint32_t(id);}
  int lower(int route,int id=0)const {
    int first=0,last=count;const auto wanted=key(route,id);
    while(first<last){const int mid=(first+last)/2;if(key(records[mid].route,records[mid].id)<wanted)first=mid+1;else last=mid;}
    return first;
  }
  const SoundRecord* find(int part,int step,int child,int id)const {
    const int r=route(part,step,child),i=lower(r,id);
    return i<count&&records[i].route==r&&records[i].id==id?&records[i]:nullptr;
  }
  int stepCount(int part,int step)const {return lower(route(part,step,0)+9)-lower(route(part,step,0));}
  bool set(int part,int step,int child,int id,float value){
    const int r=route(part,step,child),i=lower(r,id);
    if(i<count&&records[i].route==r&&records[i].id==id){records[i].value=value;return true;}
    if(count==soundLockCapacity||stepCount(part,step)==soundLocksPerStep)return false;
    std::move_backward(records.begin()+i,records.begin()+count,records.begin()+count+1);
    records[i]={std::uint16_t(r),std::uint16_t(id),value};++count;return true;
  }
  void eraseRange(int first,int last){std::move(records.begin()+last,records.begin()+count,records.begin()+first);count-=std::uint16_t(last-first);}
  void erase(int part,int step,int child,int id){const int r=route(part,step,child),i=lower(r,id);if(i<count&&records[i].route==r&&records[i].id==id)eraseRange(i,i+1);}
  void eraseStep(int part,int step){const int r=route(part,step,0);eraseRange(lower(r),lower(r+9));}
  void eraseLane(int part){eraseRange(lower(part*32*9),lower((part+1)*32*9));}
  SoundLockSet resolve(int part,int step,int child)const {
    SoundLockSet result;
    for(int r:{route(part,step,0),route(part,step,child)}){
      for(int i=lower(r);i<count&&records[i].route==r;++i)result.put(records[i].id,records[i].value);
      if(child==0)break;
    }
    return result;
  }
  StepSoundClipboard copyStep(int part,int step)const {
    StepSoundClipboard result;const int r=route(part,step,0);
    for(int i=lower(r);i<count&&records[i].route<r+9;++i){auto x=records[i];x.route-=std::uint16_t(r);result.records[result.count++]=x;}
    return result;
  }
  bool pasteStep(int part,int step,const StepSoundClipboard& source){
    if(count-stepCount(part,step)+source.count>soundLockCapacity)return false;
    eraseStep(part,step);for(int i=0;i<source.count;++i){const auto& x=source.records[i];if(!set(part,step,x.route,x.id,x.value))return false;}return true;
  }
};
} // namespace s3g_thru_sequence
