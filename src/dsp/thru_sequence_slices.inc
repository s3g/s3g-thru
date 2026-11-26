// Explicit one-based slice addresses live in the existing typed trig-lock pool.
// POS-only projects retain the legacy percent-to-index interpretation.
struct SequenceSliceInfo {
  int number=1,count=0,kind=0;
  bool valid=false,explicitAddress=false,own=false,inherited=false;
  float low=0,high=1,lead=0;
  std::array<char,24> label{};
  std::array<float,65> points{};
};
SequenceSliceInfo sequenceSliceInfo(const Engine::Controls& program,
    const s3g_thru::SliceMap& map,int kind,float position,
    const s3g_thru_sequence::SoundLockSet* sound=nullptr) const {
  SequenceSliceInfo out;out.count=int(map.count);out.kind=kind;
  const auto* address=sound?sound->find(s3g_thru_sequence::sliceLockId):nullptr;
  out.explicitAddress=address!=nullptr;
  if(program.sampleRegionMode==Engine::RegionMode::file&&kind!=2){out.count=0;out.number=address?int(std::lround(address->value)):1;return out;}
  const int index=address?int(std::lround(address->value))-1
    :map.count?std::max(0,int(std::floor(position*std::max(1.f,program.sampleRegionCount))))%int(map.count):0;
  out.number=index+1;
  if(index<0||index>=int(map.count))return out;
  out.valid=true;out.low=map.points[index];out.high=map.points[index+1];out.label=map.labels[index];
  const auto window=sliceSourceWindow(program);
  for(std::uint32_t i=0;i<=map.count;++i)out.points[i]=kind==0?window[0]+(window[1]-window[0])*map.points[i]:map.points[i];
  out.low=out.points[index];out.high=out.points[index+1];
  return out;
}
SequenceSliceInfo sequenceSliceSelection(int part,int row,int child,
    const s3g_thru_sequence::Pattern& pattern) const {
  if(part<0||part>=4||row<0||row>=32)return {};
  auto program=sequenceRunning()?splitClipControls[part]:liveControlsForPart(part);
  if(program.algorithm!=Engine::PlaybackAlgorithm::slices)return {};
  const auto& lane=pattern.tracks[part];const auto& step=lane.trigs[row];
  const auto sound=pattern.soundLocks.resolve(part,row,child);
  applySequenceVoiceSound(program,sound,part);
  int kind=0;const auto map=sliceMapForLane(std::size_t(part),program,kind);
  auto out=sequenceSliceInfo(program,map,kind,s3g_thru_sequence::resolve(lane,step,child-1,0,program.locus),&sound);
  out.own=pattern.soundLocks.find(part,row,child,s3g_thru_sequence::sliceLockId)!=nullptr;
  out.inherited=child>0&&!out.own&&out.explicitAddress;
  return out;
}
