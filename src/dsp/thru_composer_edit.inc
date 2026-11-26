// Browser targets are explicit. Borrow the native trig editor only for this
// callback, preserving Move's held step, part, Burst event and recording lane.
struct ComposerSequenceTarget {
  ThruInstance& t;
  int focus,step,child,held,owner,clip;
  std::size_t selected;
  std::array<int,4> steps,children;
  ComposerSequenceTarget(ThruInstance& instance,int part,int row,int event):t(instance),focus(t.splitFocus),step(t.sequenceStep),child(t.sequenceChild),held(t.sequenceEditStep),owner(t.sequenceSoundEditPart),clip(t.sequenceSoundEditClip),selected(t.selectedLane),steps(t.sequenceSteps),children(t.sequenceChildren){
    t.selectSplitPart(part,false);t.sequenceStep=row;t.sequenceChild=event;
    t.sequenceEditStep=row;t.sequenceSoundEditPart=part;t.sequenceSoundEditClip=t.activeClip;
  }
  ~ComposerSequenceTarget(){
    t.selectSplitPart(focus,false);t.sequenceStep=step;t.sequenceChild=child;
    t.sequenceSteps=steps;t.sequenceChildren=children;t.selectedLane=selected;
    t.sequenceEditStep=held;t.sequenceSoundEditPart=owner;t.sequenceSoundEditClip=clip;
  }
};
// Parsed and applied in the same serialized callback path as Move/MIDI edits.
bool handleComposerEdit(ThruInstance& t,const char* key,const char* value){
  if(std::strcmp(key,"composer_edit"))return false;
  if(std::strlen(value)>=224)return true;
  char text[224]{};std::strcpy(text,value);std::array<char*,28> fields{};int n=0;char* save=nullptr;
  for(char* f=strtok_r(text,"|",&save);f&&n<int(fields.size());f=strtok_r(nullptr,"|",&save))fields[n++]=f;
  if(n<6||std::strcmp(fields[0],t.composerPanelOwner.data()))return true;
  char* end=nullptr;const auto sequence=std::strtoull(fields[1],&end,10);
  if(!sequence||*end||sequence>2147483647u)return true;
  const auto receipt=(sequence<<1)|1u;
  for(const auto& old:t.composerEditReceipts)if((old.load(std::memory_order_acquire)>>1)==sequence)return true;
  const auto finish=[&](bool ok,const char* reason=nullptr){
    auto& published=t.composerEditReceipts[sequence%64];auto& error=t.composerEditErrors[sequence%64];
    published.store(0,std::memory_order_release);std::size_t i=0;
    if(!ok&&reason)for(;i+1<error.size()&&reason[i];++i)error[i].store(reason[i],std::memory_order_relaxed);
    error[i].store(0,std::memory_order_relaxed);published.store(ok?receipt:sequence<<1,std::memory_order_release);
    t.composerEditVersion.store(std::max<std::uint64_t>(sequence,t.composerEditVersion.load()),std::memory_order_release);
  };
  if(!std::strcmp(fields[2],"name")&&n==6){
    char* endClip=nullptr;const long slot=std::strtol(fields[3],&endClip,10);std::string name;const std::string hex=fields[5];bool valid=hex=="-"||(hex.size()<=80&&hex.size()%2==0);
    for(std::size_t i=0;valid&&hex!="-"&&i<hex.size();i+=2){const auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};const int a=digit(hex[i]),b=digit(hex[i+1]);if(a<0||b<0)valid=false;else name+=char(a*16+b);}
    if(!endClip||*endClip||!composerWorkspaceReady(t,int(slot))||!valid||!s3g_thru_names::valid(name)||t.clipRecording){finish(false);return true;}
    t.pushClipUndo(t.activeClip);s3g_thru_names::assign(t.clipSlots[t.activeClip].name,name);t.composerPerformanceChanged();finish(true);return true;
  }
  std::array<double,25> a{};
  for(int i=3;i<n;++i){a[i-3]=std::strtod(fields[i],&end);if(end==fields[i]||*end||!std::isfinite(a[i-3])){finish(false);return true;}}
  const auto integer=[](double v,int low,int high){return v>=low&&v<=high&&v==std::floor(v);};
  if(!integer(a[0],0,63)||int(a[0])!=t.activeClip||t.composerOwned.load()||t.composerLease.load()<ThruInstance::composerClock()
      ||t.projectLocked()||t.namingBusy()||t.clipPreparationInFlight()||t.audioRecording()){finish(false,!std::strcmp(fields[2],"mode")?"Finish recording, loading or project operations before changing mode":nullptr);return true;}
  if(!integer(a[1],0,(t.splitEnabled?t.partCount():1)-1)){finish(false);return true;}
  const int part=int(a[1]);
  if(!std::strcmp(fields[2],"mode")&&n==7){
    if(!integer(a[2],0,6)||!integer(a[3],0,6)){finish(false,"Invalid instrument mode");return true;}
    if(t.performanceMode()!=int(a[3])){finish(false,"Mode changed on Move; choose again from its current setting");return true;}
    if(t.clipRecording||t.patternBusy()){finish(false,"Finish recording or the clip transition before changing mode");return true;}
    t.clearError();t.setPerformanceMode(int(a[2]));
    finish(t.performanceMode()==int(a[2])&&!t.lastError[0],t.lastError[0]?t.lastError.data():"Mode change was not applied");return true;
  }
  if(composerWorkspaceEdit(t,fields.data(),n,a,finish))return true;
  if(!std::strcmp(fields[2],"mod")&&n==10){
    if(!integer(a[2],0,7)||!integer(a[3],0,16383)||!integer(a[4],0,1024)||!integer(a[5],0,4)||!integer(a[6],0,21)){finish(false);return true;}
    finish(t.assignModulation(part,int(a[2]),int(a[3]),int(a[4]),int(a[5]),int(a[6])));return true;
  }

  if(!std::strcmp(fields[2],"f")&&n==6){t.selectSplitPart(part);finish(true);return true;}
  if(!std::strcmp(fields[2],"p")&&(n==7||n==8||n==9)){
    if(!integer(a[2],0,16383)){finish(false);return true;}
    const MidiParameter* parameter=nullptr;for(const auto& p:s3g_thru_midi::parameters)if(p.id==int(a[2]))parameter=&p;
    if(!parameter||parameter->action||a[3]<parameter->minimum||a[3]>midiMaximum(t,*parameter,part)||(parameter->integer&&a[3]!=std::floor(a[3]))){finish(false);return true;}
    // Selection and routing are separate from sound edits; these controls are not panel parameters.
    if(!s3g_thru_pack::soundParameter(*parameter)&&!ThruInstance::composerPanelSetting(parameter->key)
        &&std::strcmp(parameter->key,"seq_length")&&std::strcmp(parameter->key,"seq_rate")&&std::strcmp(parameter->key,"seq_rotation")
        &&std::strcmp(parameter->key,"seq_active")&&std::strcmp(parameter->key,"seq_velocity")&&std::strcmp(parameter->key,"seq_gate")
        &&std::strcmp(parameter->key,"seq_scale")&&std::strcmp(parameter->key,"seq_duration_mode")&&std::strcmp(parameter->key,"seq_time_ms")
        &&std::strcmp(parameter->key,"seq_direction")&&std::strcmp(parameter->key,"seq_path")&&std::strcmp(parameter->key,"seq_type")
        &&std::strcmp(parameter->key,"seq_clip_beats")&&std::strncmp(parameter->key,"seq_recipe_",11)){finish(false);return true;}
    if(!std::strncmp(parameter->key,"seq_",4)&&!t.sequenceMode()){finish(false);return true;}
    if(n>=8&&(!integer(a[4],-1,3)||(a[4]>=0&&t.tableEditLane(part)!=int(a[4])))){finish(false);return true;}
    const auto& cfg=t.settingsForPart(part).engines;
    const int token=(cfg.enabled?16:0)+std::clamp(cfg.edit,0,t.engineSlotCount()-1)+1;
    if(n==9&&(!integer(a[5],-1,24)||(a[5]>=0&&int(a[5])!=token))){finish(false);return true;}
    if(t.engineSlotKey(parameter->key)>=t.engineSlotCount()){finish(false);return true;}
    const auto selected=t.selectedLane;const int held=t.sequenceEditStep;t.sequenceEditStep=-1;t.clearError();midiWrite(t,*parameter,part,a[3]);t.sequenceEditStep=held;t.selectedLane=selected;finish(!t.lastError[0],t.lastError.data());return true;
  }
  if(!std::strcmp(fields[2],"root")&&n==6){
    if(!integer(a[2],24,96)){finish(false);return true;}
    const int focus=t.splitFocus;if(t.splitEnabled)t.selectSplitPart(part,false);
    char v[16]{};std::snprintf(v,sizeof(v),"%d",int(a[2]));setParam(&t,"root_note",v);
    if(t.splitEnabled)t.selectSplitPart(focus,false);
    finish(true);return true;
  }
  if(!std::strcmp(fields[2],"v")&&n==6){
    const int first=t.splitEnabled&&!t.sharedDual?t.firstLaneForPart(part):0,last=t.splitEnabled&&!t.sharedDual?first+(t.isQuad()?0:1):3;
    if(!integer(a[2],first,last)){finish(false);return true;}t.selectedLane=int(a[2]);t.markPersistentStateDirty();finish(true);return true;
  }
  if(!std::strcmp(fields[2],"r")&&n==6){
    if(a[2]<1||a[2]>999){finish(false);return true;}char v[32]{};std::snprintf(v,sizeof(v),"%.8g",a[2]);setParam(&t,"tempo_bpm",v);finish(true);return true;
  }
  if(!std::strcmp(fields[2],"play")&&n==6){
    if(!integer(a[2],0,1)){finish(false);return true;}t.setClipPlaying(a[2]>0);finish(true);return true;
  }
  if(!std::strcmp(fields[2],"c")&&n==6){if(!integer(a[2],0,63)||t.clipRecording){finish(false);return true;}t.selectClip(int(a[2]));finish(true);return true;}
  if(!t.sequenceMode()){finish(false);return true;}
  auto& lane=t.sequenceLive.tracks[part];
  if(!std::strcmp(fields[2],"l")&&n==17){
    if(!integer(a[2],1,32)||!integer(a[3],0,8)||!integer(a[4],0,31)||!integer(a[5],0,4)||!integer(a[6],0,2)||!integer(a[7],0,1)
        ||a[8]<1||a[8]>127||!integer(a[9],0,1)||a[10]<(a[9]>0?10:.01f)||a[10]>(a[9]>0?30000:32)||a[11]<0||a[11]>1||!integer(a[12],0,127)||!integer(a[13],0,34)){finish(false);return true;}
    const int f[]{0,1,2,10,11,3,5,8,a[9]>0?9:6};
    for(int i=0;i<9;++i){t.setSequenceLaneValue(t.sequenceLive,part,f[i],a[i+2]);t.sequenceOverrides[part][f[i]]=true;}
    t.sequenceRequestedRate[part].store(lane.rate,std::memory_order_release);
    auto& controls=!t.splitEnabled||part==t.splitFocus?t.controls:t.splitParts[part].controls;
    controls.locus=a[11];
    t.splitParts[part].settings.root=int(a[12]);t.splitParts[part].settings.scale=int(a[13]);
    if(part==t.splitFocus){t.rootNote=int(a[12]);t.padScale=static_cast<PadScale>(int(a[13]));}
    t.sequencePlayback.tracks[part]=lane;
  } else if(!std::strcmp(fields[2],"t")&&n>=19){
    // clip,part,step,on,micro,prob,hit,every,mask,pos,note,vel,unit,dur,count,offsets...
    if(!integer(a[14],1,8)){finish(false);return true;}
    const int count=int(a[14]);
    if(!integer(a[2],0,31)||!integer(a[3],0,1)||a[4]<-.5f||a[4]>.5f||a[5]<0||a[5]>1||!integer(a[6],1,8)||!integer(a[7],1,8)||a[6]>a[7]
        ||!integer(a[8],0,7)||a[9]<0||a[9]>1||a[10]<-48||a[10]>48||a[11]<1||a[11]>127||!integer(a[12],-1,1)
        ||(a[12]>=0&&(a[13]<(a[12]>0?10:.01f)||a[13]>(a[12]>0?30000:32)))||!integer(a[14],1,8)||n!=18+count){finish(false);return true;}
    for(int i=0;i<count;++i)if(a[15+i]<0||a[15+i]>=1||(i&&a[15+i]<a[14+i])){finish(false);return true;}
    const int row=int(a[2]);auto& step=lane.trigs[row];
    step.enabled=a[3]>0;step.micro=a[4];step.probability=a[5];step.cycleHit=int(a[6]);step.cycleEvery=int(a[7]);
    step.locks.mask=std::uint8_t(a[8]);step.locks.values[0]=a[9];step.locks.values[1]=a[10];step.locks.values[2]=a[11];
    step.locks.timeLocked=a[12]>0;if(a[12]==0){step.locks.mask|=8;step.locks.values[3]=a[13];}else if(a[12]>0)step.locks.timeMs=a[13];
    for(int i=0;i<count;++i){if(i>=step.count)step.events[i]={};step.events[i].when=a[15+i];}step.count=count;
    if(lane.generator.type){auto& g=lane.generator;g.base[row]=step;g.enabledMask|=1u<<row;g.enabledValues=(g.enabledValues&~(1u<<row))|(step.enabled?1u<<row:0u);g.burstMask|=1u<<row;}
    t.sequencePlayback.tracks[part].trigs[row]=step;t.sequencePlayback.tracks[part].generator=lane.generator;
  } else if(!std::strcmp(fields[2],"s")&&(n==9||n==10)){
    if(!integer(a[2],0,lane.length-1)||!integer(a[3],0,16383)||!integer(a[5],0,1)||(n==10&&!integer(a[6],0,lane.trigs[int(a[2])].count))){finish(false);return true;}
    const int child=n==10?int(a[6]):0;
    const auto* parameter=s3g_thru_midi::find(int(a[3]));
    if(!parameter||!s3g_thru_sequence::soundDescriptor(parameter->id)||a[4]<parameter->minimum||a[4]>parameter->maximum||(parameter->integer&&a[4]!=std::floor(a[4]))){finish(false);return true;}
    const auto& pool=t.sequenceLive.soundLocks;
    if(!a[5]&&!pool.find(part,int(a[2]),child,parameter->id)&&(pool.count==s3g_thru_sequence::soundLockCapacity||pool.stepCount(part,int(a[2]))==s3g_thru_sequence::soundLocksPerStep)){finish(false);return true;}
    if(parameter->id==s3g_thru_sequence::sliceLockId&&!a[5]){
      const auto info=t.sequenceSliceSelection(part,int(a[2]),child,t.sequenceRunning()?t.sequencePlayback:t.sequenceLive);
      if(t.liveControlsForPart(part).algorithm!=Engine::PlaybackAlgorithm::slices||a[4]>info.count){finish(false,"Slice is not available");return true;}
    }
    ComposerSequenceTarget target(t,part,int(a[2]),child);
    t.editSequenceSoundLock(parameter->id,a[4],a[5]>0);finish(true);return true;
  } else if(!std::strcmp(fields[2],"q")&&n==10){
    // clip,part,step,event,field,value,clear. Core fields use native setters,
    // including recipe overrides, duration units, pitch/scale and clip Undo.
    if(!integer(a[2],0,lane.length-1)||!integer(a[3],0,lane.trigs[int(a[2])].count)||!integer(a[4],0,12)||!integer(a[6],0,1)){finish(false);return true;}
    const int field=int(a[4]),child=int(a[3]);
    constexpr float lo[]{0,0,-48,1,10,.01f,-.5f,0,1,1,1,0,0};
    constexpr float hi[]{1,1,48,127,30000,32,.5f,1,8,8,8,.999f,0};
    const bool discrete=field==0||field==2||field==3||field==8||field==9||field==10;
    if((a[6]>0&&(field<1||field>5))||a[5]<lo[field]||a[5]>hi[field]||(discrete&&a[5]!=std::floor(a[5]))
        ||(field==11&&!child)||(field==4&&!lane.timeMode)||(field==5&&lane.timeMode)||(field==10&&child)){finish(false);return true;}
    if(field==11){const auto& trig=lane.trigs[int(a[2])];if((child>1&&a[5]<trig.events[child-2].when)||(child<trig.count&&a[5]>trig.events[child].when)){finish(false);return true;}}
    ComposerSequenceTarget target(t,part,int(a[2]),child);char number[32]{};
    if(a[6]>0){std::snprintf(number,sizeof(number),"%d",field>=4?3:field-1);t.setSequenceParameter("seq_clear",number);}
    else if(field==0){const auto& current=(t.sequenceRunning()?t.sequencePlayback:t.sequenceLive).tracks[part].trigs[int(a[2])];if(current.enabled!=(a[5]>0))t.setSequenceParameter("seq_toggle","1");}
    else{constexpr const char* keys[]{"seq_toggle","seq_pos","seq_note","seq_vel","seq_trig_time_ms","seq_trig_gate","seq_micro","seq_prob","seq_cycle_hit","seq_cycle_every","seq_burst","seq_when","seq_clear"};
      std::snprintf(number,sizeof(number),"%.8g",field==12?4:a[5]);t.setSequenceParameter(keys[field],number);}
    finish(true);return true;
  } else if(!std::strcmp(fields[2],"a")&&n==7){
    if(!integer(a[2],0,lane.length-1)||!integer(a[3],0,lane.trigs[int(a[2])].count)){finish(false);return true;}
    ComposerSequenceTarget target(t,part,int(a[2]),int(a[3]));t.auditionSequenceStep();finish(true);return true;
  } else {finish(false);return true;}
  t.invalidateSequenceRecipePlayback();if(auto* clip=t.activeClipSlot())clip->sequence=t.sequenceLive;
  ++t.sequenceRevision;t.markPersistentStateDirty();finish(true);return true;
}
