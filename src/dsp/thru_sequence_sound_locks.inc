// Bounded authored sound locks. Trig identities use existing MIDI parameter IDs.
int sequenceSoundEditPart=-1,sequenceSoundEditClip=-1;
s3g_thru_sequence::StepSoundClipboard sequenceSoundClipboard;
std::array<s3g_thru_sequence::SoundLockSet,4> sequenceLaneSoundLocks{};
struct SequenceSoundTick {
  std::int64_t tick=-1;
  std::array<s3g_thru_sequence::SoundLockSet,8> children;
};
std::array<std::array<SequenceSoundTick,8>,4> sequenceSoundTicks;
bool sequenceSoundEditing(int part=-1)const {
  return sequenceMode()&&sequenceEditStep>=0&&sequenceSoundEditClip==activeClip&&sequenceSoundEditPart==splitFocus
    &&(part<0||part==sequenceSoundEditPart);
}
void resetSequenceSoundRuntime(){
  for(auto& set:sequenceLaneSoundLocks)set.count=0;
  for(auto& part:sequenceSoundTicks)for(auto& tick:part)tick.tick=-1;
}
const s3g_thru_midi::Parameter* sequenceSoundParameter(const char* key,int& part)const {
  const char* canonical=key;
  for(int p=0;p<4;++p){
    if(!std::strcmp(key,kLockParameterKeys[partLevelParameter(p)])){part=p;canonical="part_level";}
    if(!std::strcmp(key,kLockParameterKeys[partDelayParameter(p)])){part=p;canonical="part_delay";}
    if(!std::strcmp(key,kLockParameterKeys[partReverbParameter(p)])){part=p;canonical="part_reverb";}
  }
  if(!std::strncmp(key,"seq_pan_",8)&&key[8]>='1'&&key[8]<='4'&&!key[9]){part=key[8]-'1';canonical="seq_pan_1";}
  for(const auto& p:s3g_thru_midi::parameters)if(!std::strcmp(p.key,canonical))return &p;
  return nullptr;
}
bool sequenceSoundEditValue(int id,int part,float& value)const {
  if(!sequenceSoundEditing(part))return false;
  const auto set=sequenceLive.soundLocks.resolve(part,sequenceEditStep,sequenceChild);
  const auto* found=set.find(id);if(!found)return false;value=found->value;return true;
}
bool editSequenceSoundLock(int id,float value,bool clear=false){
  if(!sequenceSoundEditing())return false;
  const auto* p=s3g_thru_midi::find(id);const auto* d=s3g_thru_sequence::soundDescriptor(id);
  if(!p||!d||(!clear&&!std::isfinite(value)))return true;
  const int part=sequenceSoundEditPart,step=sequenceEditStep,child=sequenceChild;
  if(id==s3g_thru_sequence::sliceLockId&&!clear){
    const auto info=sequenceSliceSelection(part,step,child,sequenceRunning()?sequencePlayback:sequenceLive);
    if(liveControlsForPart(part).algorithm!=Engine::PlaybackAlgorithm::slices||value<1||value>info.count||value!=std::floor(value)){setError("Slice is not available");return true;}
  }
  value=clampValue(value,p->minimum,p->maximum);if(p->integer)value=std::round(value);
  auto& pool=sequenceLive.soundLocks;
  if(!clear&&!pool.find(part,step,child,id)&&(pool.count==s3g_thru_sequence::soundLockCapacity||pool.stepCount(part,step)==s3g_thru_sequence::soundLocksPerStep)){
    setError("Trig lock capacity full");return true;
  }
  sequenceChanged();
  if(clear)pool.erase(part,step,child,id);else pool.set(part,step,child,id,value);
  commitSequenceEdit();++midiRevision;clearError();
  // Edits during playback belong to the next occurrence. Stopped editing is
  // heard immediately through the same two-voice lane and release path.
  auditionSequenceStep();return true;
}
bool setSequenceSoundParameter(const char* key,const char* text){
  if(!std::strcmp(key,"seq_sound_edit")||!std::strcmp(key,"seq_sound_clear")){
    if(!sequenceSoundEditing())return true;
    int id=-1;float value=0;char tail=0;
    if(!std::strcmp(key,"seq_sound_clear")){if(std::sscanf(text,"%d %c",&id,&tail)==1)editSequenceSoundLock(id,0,true);}
    else if(std::sscanf(text,"%d,%f %c",&id,&value,&tail)==2)editSequenceSoundLock(id,value);
    return true;
  }
  if(!sequenceSoundEditing())return false;
  if(!std::strcmp(key,"clip_automation"))return true;
  int part=splitFocus;const char* mapped=key;
  static constexpr const char* stripKeys[]{"seq_eq_low","seq_eq_mid","seq_eq_high","seq_eq_freq","seq_eq_solo","mix_mute"};
  char number[48]{};
  if(!std::strcmp(key,"seq_strip_edit")){
    int field=-1;float value=0;char tail=0;
    if(std::sscanf(text,"%d,%d,%f %c",&part,&field,&value,&tail)!=3||field<0||field>5)return true;
    mapped=stripKeys[field];std::snprintf(number,sizeof(number),"%.9g",value);text=number;
  }
  const auto* p=sequenceSoundParameter(mapped,part);
  if(!p||part!=sequenceSoundEditPart)return false;
  if(!std::strcmp(mapped,"locus")){
    if(liveControlsForPart(part).algorithm!=Engine::PlaybackAlgorithm::table)setSequenceParameter("seq_pos",text);
    return true;
  }
  if(!std::strcmp(key,"clip_automation"))return true;
  if(!s3g_thru_sequence::soundDescriptor(p->id)){
    if(!std::strcmp(mapped,"algorithm")||!std::strcmp(mapped,"table_model")||!std::strcmp(mapped,"fm_start")||!std::strcmp(mapped,"reson_start")||!std::strcmp(mapped,"lowform_start")){setError("Lane setting, not a trig lock");return true;}
    return false;
  }
  return editSequenceSoundLock(p->id,parseFloat(text,NAN));
}
void applySequenceVoiceSound(Engine::Controls& c,const s3g_thru_sequence::SoundLockSet& set,int part)const {
  float ignored=1;
  for(int n=0;n<set.count;++n){const auto& x=set.values[n];const auto* d=s3g_thru_sequence::soundDescriptor(x.id);if(!d||d->lane)continue;
    if(x.id>=5000&&x.id<5000+s3g_thru_lowform::fields){if(c.algorithm==Engine::PlaybackAlgorithm::lowform)s3g_thru_lowform::set(c.lowform.patches[part],x.id-5000,x.value);}
    else if(x.id>=4800&&x.id<4800+s3g_thru_partials::fields){if(c.algorithm==Engine::PlaybackAlgorithm::partials)s3g_thru_partials::set(c.partials.patches[part],x.id-4800,x.value);}
    else if(x.id>=4600&&x.id<4600+s3g_thru_reson::fields){if(c.algorithm==Engine::PlaybackAlgorithm::reson)s3g_thru_reson::set(c.reson.patches[part],x.id-4600,x.value);}
    else if(x.id>=4500&&x.id<4500+s3g_thru_noise::fields){if(c.algorithm==Engine::PlaybackAlgorithm::noise)s3g_thru_noise::set(c.noise.patches[part],x.id-4500,x.value);}
    else if(x.id>=4400&&x.id<4400+s3g_thru_fm::fields){if(c.algorithm==Engine::PlaybackAlgorithm::fm)s3g_thru_fm::set(c.fm.patches[part],x.id-4400,x.value);}
    else if(x.id>=4300&&x.id<4300+s3g_thru_scanned::fields){if(c.algorithm==Engine::PlaybackAlgorithm::scanned)s3g_thru_scanned::set(c.scanned.patches[part],x.id-4300,x.value);}
    else if(x.id>=4200&&x.id<4240){if(c.algorithm==Engine::PlaybackAlgorithm::table)s3g_thru_table::set(c.table.patches[part],x.id-4200,x.value);}
    else if(d->target>=0)applyLockedValue(c,ignored,d->target,x.value);
  }
}
s3g_thru_sequence::SoundLockSet sequenceTriggerSound(int part,int step,int child,std::int64_t tick,bool audition,const s3g_thru_sequence::Step& trig){
  if(audition||tick<0)return (audition?sequenceLive:sequencePlayback).soundLocks.resolve(part,step,child+1);
  auto& bank=sequenceSoundTicks[part][std::size_t(tick)%8];
  if(bank.tick!=tick){bank.tick=tick;for(int c=0;c<trig.count;++c)bank.children[c]=sequencePlayback.soundLocks.resolve(part,step,c+1);}
  return bank.children[child];
}
bool sequenceLaneSoundActive(int part)const {
  if(!sequenceMode())return false;
  if(sequenceRunning())return true;
  for(const auto& v:voices)if(v.active&&v.sequence.audition&&v.part==part)return true;
  return false;
}
void applySequenceLaneInsert(Engine::Controls& c,int part)const {
  if(!sequenceLaneSoundActive(part))return;
  float ignored=1;const auto& set=sequenceLaneSoundLocks[part];
  for(int n=0;n<set.count;++n){const auto& x=set.values[n];const auto* d=s3g_thru_sequence::soundDescriptor(x.id);if(d&&d->lane&&d->target>=0)applyLockedValue(c,ignored,d->target,x.value);}
}
void applySequenceLaneMixer(Engine::Controls& c)const {
  if(!sequenceMode())return;
  float ignored=1;
  for(int part=0;part<4;++part)if(sequenceLaneSoundActive(part)){
    const auto& set=sequenceLaneSoundLocks[part];
    for(int n=0;n<set.count;++n){const auto& x=set.values[n];const int index=x.id==6?int(partLevelParameter(part)):x.id==7?int(partDelayParameter(part)):x.id==8?int(partReverbParameter(part)):-1;
      if(index>=0)applyLockedValue(c,ignored,index,x.value);
    }
  }
}
void applySequenceLaneStrips(s3g_thru_sequence::Mix& mix)const {
  if(!sequenceMode())return;
  for(int part=0;part<4;++part)if(sequenceLaneSoundActive(part)){
    auto& strip=mix.strips[part];const auto& set=sequenceLaneSoundLocks[part];
    for(int n=0;n<set.count;++n){const auto& x=set.values[n];switch(x.id){case 3867:strip.pan=x.value;break;case 3871:strip.lowDb=x.value;break;case 3872:strip.midDb=x.value;break;case 3873:strip.highDb=x.value;break;case 3874:strip.midHz=x.value;break;default:break;}}
  }
}
int formatSequenceSoundState(char* out,int capacity)const {
  const bool active=sequenceSoundEditing();
  int at=std::snprintf(out,capacity,"{\"active\":%d,\"clip\":%d,\"part\":%d,\"step\":%d,\"event\":%d,\"used\":%u,\"values\":[",active?1:0,activeClip,sequenceSoundEditPart,sequenceEditStep,sequenceChild,unsigned(sequenceLive.soundLocks.count));
  if(active){const auto set=sequenceLive.soundLocks.resolve(sequenceSoundEditPart,sequenceEditStep,sequenceChild);
    for(int i=0;i<set.count;++i){if(at<0||at>=capacity)return -1;const auto& x=set.values[i];const bool own=sequenceLive.soundLocks.find(sequenceSoundEditPart,sequenceEditStep,sequenceChild,x.id);
      at+=std::snprintf(out+at,capacity-at,"%s[%u,%.9g,%d]",i?",":"",unsigned(x.id),double(x.value),own?1:0);}
  }
  if(at<0||at>=capacity)return -1;
  at+=std::snprintf(out+at,capacity-at,"]}");
  return at<capacity?at:-1;
}
static bool writeSequenceSoundLocks(std::FILE* f,const std::array<ClipSlot,kClipSlotCount>& clips){
  std::uint32_t bytes=4+64*4+4;
  if(!writeU32(f,0x314b4c53u))return false;
  for(const auto& clip:clips){const auto& p=clip.sequence.soundLocks;if(!writeU32(f,p.count))return false;
    for(int i=0;i<p.count;++i){const auto& x=p.records[i];if(!writeU32(f,(std::uint32_t(x.route)<<16)|x.id)||!writeF32(f,x.value))return false;bytes+=8;}
  }
  return writeU32(f,bytes);
}
static bool readSequenceSoundLocks(std::FILE* f,std::array<ClipSlot,kClipSlotCount>& clips){
  std::uint32_t magic=0,bytes=4+64*4+4;
  if(!readU32(f,magic)||magic!=0x314b4c53u)return false;
  for(auto& clip:clips){std::uint32_t count=0;if(!readU32(f,count)||count>s3g_thru_sequence::soundLockCapacity)return false;auto& pool=clip.sequence.soundLocks;pool.count=0;std::uint32_t previous=0;
    for(std::uint32_t i=0;i<count;++i){std::uint32_t packed=0;float value=0;if(!readU32(f,packed)||!readF32(f,value)||(i&&packed<=previous))return false;
      const int route=int(packed>>16),id=int(packed&65535);const auto* p=s3g_thru_midi::find(id);
      if(route>=4*32*9||!p||!s3g_thru_sequence::soundDescriptor(id)||!std::isfinite(value)||value<p->minimum||value>p->maximum||(p->integer&&value!=std::round(value)))return false;
      if(!pool.set(route/(32*9),(route/9)%32,route%9,id,value))return false;
      previous=packed;bytes+=8;
    }
  }
  std::uint32_t size=0;return readU32(f,size)&&size==bytes;
}
