void testSequenceSoundPool(){
  using namespace s3g_thru_sequence;
  SoundLocks pool;
  assert(pool.set(0,3,0,4201,-12));assert(pool.set(0,3,0,4212,800));
  assert(pool.set(0,3,2,4201,7));
  const auto parent=pool.resolve(0,3,0),event=pool.resolve(0,3,2);
  assert(parent.find(4201)->value==-12&&event.find(4201)->value==7&&event.find(4212)->value==800);
  const auto clip=pool.copyStep(0,3);assert(pool.pasteStep(3,9,clip));
  assert(pool.resolve(3,9,2).find(4201)->value==7);
  pool.erase(3,9,2,4201);assert(pool.resolve(3,9,2).find(4201)->value==-12);
  pool.eraseStep(3,9);assert(pool.stepCount(3,9)==0&&pool.stepCount(0,3)==3);
  pool={};for(int i=0;i<32;++i)assert(pool.set(0,0,i%9,500+i,float(i)));
  assert(!pool.set(0,0,0,900,0));assert(pool.set(0,0,0,500,99));
  pool={};for(int p=0;p<4;++p)for(int s=0;s<32;++s)for(int f=0;f<8;++f)assert(pool.set(p,s,0,4200+f,float(f)));
  assert(pool.count==1024&&!pool.set(0,0,0,4300,0));
  StepSoundClipboard extra;extra.count=9;for(int i=0;i<9;++i)extra.records[i]={0,std::uint16_t(4200+i),float(i)};
  assert(!pool.pasteStep(0,0,extra)&&pool.count==1024&&pool.stepCount(0,0)==8);
  std::puts("Sequence sound pool: bounded sorted sparse storage, parent/Burst inheritance, scoped copy/clear and atomic capacity rejection: PASS");
}
void testSequenceSoundVoices(){
  namespace tab=s3g_thru_table;
  auto t=tableFixture(6);
  setParam(t.get(),"seq_duration_mode","1");setParam(t.get(),"seq_time_ms","200");
  setParam(t.get(),"seq_hold","0");
  const auto base=t->controls;
  setParam(t.get(),"table_type","2");setParam(t.get(),"table_tune","-12");
  setParam(t.get(),"amp_attack","600");setParam(t.get(),"amp_release","900");
  assert(t->controls.table.patches[0].v[tab::type]==base.table.patches[0].v[tab::type]);
  assert(t->controls.ampAttackMs==base.ampAttackMs&&t->controls.ampReleaseMs==base.ampReleaseMs);
  auto sound=t->sequenceLive.soundLocks.resolve(0,0,0);
  assert(sound.find(4200)->value==2&&sound.find(4201)->value==-12);
  float feedback=0;assert(t->sequenceSoundEditValue(4201,0,feedback)&&feedback==-12);
  assert(midiRead(*t,midiParameter("table_tune"),0)==-12);
  if(const auto* path=std::getenv("S3G_THRU_SEQ_SOUND_FIXTURE")){
    char seq[8192]{},soundState[2048]{},table[4096]{};assert(t->sequenceState(seq,sizeof(seq))>0&&t->formatSequenceSoundState(soundState,sizeof(soundState))>0&&t->formatTableState(table,sizeof(table))>0);
    auto* f=std::fopen(path,"wb");assert(f);assert(std::fprintf(f,"{\"sequence\":%s,\"sound\":%s,\"table\":%s}",seq,soundState,table)>0);std::fclose(f);
  }
  setParam(t.get(),"seq_hold","-1");tableRender(*t,8);
  t->startSequenceVoice(0,0,0,.25,true,0);
  PlaybackVoice* first=nullptr;for(auto& v:t->voices)if(v.active&&v.sequence.audition&&v.held)first=&v;assert(first);
  auto effective=t->effectiveControls(*first);
  assert(effective.table.patches[0].v[tab::type]==2&&effective.table.patches[0].v[tab::tune]==-12);
  assert(first->sequence.attack+first->sequence.decay+first->sequence.release<=200.01f);
  assert(first->sequence.frames==8820);
  t->sequenceLive.soundLocks.set(0,1,0,4200,1);t->sequenceLive.soundLocks.set(0,1,0,4201,7);t->sequencePlayback=t->sequenceLive;
  t->startSequenceVoice(0,1,0,.25,true,1);
  assert(t->effectiveControls(*first).table.patches[0].v[tab::type]==2);
  int held=0;for(auto& v:t->voices)if(v.active&&v.held&&v.part==0){++held;if(v.sequence.step==1)assert(t->effectiveControls(v).table.patches[0].v[tab::tune]==7);}
  assert(held==2&&tableRender(*t,8)>100);
  // Neither an LFO nor later clip/base changes can replace explicit locks.
  t->controlLfoRouted=true;t->controlLfoRuntime={};t->controlLfoRuntime[0].initialized=true;t->controlLfoRuntime[0].routes[0]={{0,15,131},.5f,.5f};
  t->controls.table.patches[0].v[tab::tune]=20;
  assert(t->effectiveControls(*first).table.patches[0].v[tab::tune]==-12);
  // Lock application is sparse: an unlocked value continues following base.
  t->controls.table.patches[0].v[tab::level]=42;
  assert(t->effectiveControls(*first).table.patches[0].v[tab::level]==42);
  // A new event without locks restores lane defaults, without changing old notes.
  t->startSequenceVoice(0,2,0,.25,true,2);tableRender(*t,20);
  assert(t->sequenceLaneSoundLocks[0].count==0);
  // Burst captures the authored sound at the parent occurrence, even if its
  // remaining children are rendered after a live edit.
  auto trig=t->sequenceLive.tracks[0].trigs[0];trig.count=2;t->sequencePlayback=t->sequenceLive;
  const auto a=t->sequenceTriggerSound(0,0,0,40,false,trig);
  t->sequencePlayback.soundLocks.set(0,0,0,4201,23);
  const auto b=t->sequenceTriggerSound(0,0,1,40,false,trig);
  assert(a.find(4201)->value==-12&&b.find(4201)->value==-12);
  assert(t->sequenceTriggerSound(0,0,0,41,false,trig).find(4201)->value==23);
  std::puts("Sequence sound voices: TABLE type/pitch per note, two overlapping voices, duration-fit ADSR, actual audio, LFO/automation priority, unlocked inheritance and stable Burst occurrences: PASS");
}
void testSequenceSoundEditing(){
  auto t=sequenceFixture();setParam(t.get(),"seq_hold","0");
  const auto base=t->controls;const auto strip=t->sequenceLive.mix.strips[0];
  setParam(t.get(),"grain_size","340");setParam(t.get(),"effect_drive",".7");setParam(t.get(),"seq_pan_1","-.75");setParam(t.get(),"seq_strip_edit","0,1,6");setParam(t.get(),"part_delay",".9");
  assert(t->controls.grainSizeMs==base.grainSizeMs&&t->controls.effectDrive==base.effectDrive);
  assert(t->sequenceLive.mix.strips[0].pan==strip.pan&&t->sequenceLive.mix.strips[0].midDb==strip.midDb);
  auto& locks=t->sequenceLaneSoundLocks[0];locks=t->sequenceLive.soundLocks.resolve(0,0,0);
  t->clipPlaying=true;
  auto c=t->controls;t->applySequenceLaneInsert(c,0);assert(std::abs(c.effectDrive-.7f)<1e-6);
  auto mixer=t->controls;t->applySequenceLaneMixer(mixer);assert(std::abs(mixer.splitDelayA-.9f)<1e-6);
  auto mix=t->sequenceLive.mix;t->applySequenceLaneStrips(mix);assert(mix.strips[0].pan==-.75f&&mix.strips[0].midDb==6&&mix.strips[1].pan==0);
  t->clipPlaying=false;
  setParam(t.get(),"seq_copy","1");setParam(t.get(),"seq_hold","-1");
  setParam(t.get(),"record_lane","2");setParam(t.get(),"seq_step","5");setParam(t.get(),"seq_paste","1");
  assert(t->sequenceLive.soundLocks.stepCount(1,4)==5);
  setParam(t.get(),"seq_hold","4");setParam(t.get(),"seq_sound_clear","3867");assert(!t->sequenceLive.soundLocks.resolve(1,4,0).find(3867));
  setParam(t.get(),"seq_delete","1");assert(t->sequenceLive.soundLocks.stepCount(1,4)==0&&t->sequenceLive.soundLocks.stepCount(0,0)==5);
  // Controller edits on another part must leave the held trig alone.
  const int count=t->sequenceLive.soundLocks.count;nrpn(*t,5,"grain_size",5000);assert(t->sequenceLive.soundLocks.count==count);
  char packet[2048]{};assert(t->formatSequenceSoundState(packet,sizeof(packet))>0&&std::strstr(packet,"\"active\":1"));
  std::puts("Sequence sound editing: ordinary setters leave bases intact, lane EQ/pan/insert/sends, scoped copy/delete/clear, MIDI feedback and part isolation: PASS");
}
void testSequenceSoundUndoAndScope(){
  auto t=tableFixture(6);setParam(t.get(),"seq_hold","0");setParam(t.get(),"table_tune","5");assert(t->sequenceLive.soundLocks.count==1);setParam(t.get(),"seq_hold","-1");
  t->restoreUndo();assert(t->sequenceLive.soundLocks.count==0&&t->activeClipSlot()->sequence.soundLocks.count==0);
  setParam(t.get(),"seq_hold","0");setParam(t.get(),"table_tune","9");
  t->activeClip=1;t->resetSequenceClock();assert(!t->sequenceSoundEditing()&&t->sequenceEditStep<0);
  // Algorithm and preset actions remain lane settings. Supported edits do not
  // change engine choice or store an action disguised as a value.
  t->activeClip=0;setParam(t.get(),"seq_hold","0");const auto before=t->controls.algorithm;
  setParam(t.get(),"algorithm","4");assert(t->controls.algorithm==before);setParam(t.get(),"table_model","3");assert(!t->sequenceLive.soundLocks.resolve(0,0,0).find(4221));
  std::puts("Sequence sound ownership: Undo restores authored locks, clip changes cancel stale targets and engine/preset actions remain lane-level: PASS");
}
void testSequenceSoundPersistence(){
  char folder[]="/tmp/thru-sound-locks-XXXXXX";assert(mkdtemp(folder));
  auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();t->setPerformanceMode(6);setParam(t.get(),"algorithm","15");
  setParam(t.get(),"seq_hold","0");setParam(t.get(),"table_type","2");setParam(t.get(),"table_tune","-7");
  setParam(t.get(),"seq_burst","3");setParam(t.get(),"seq_child","2");setParam(t.get(),"table_tune","11");setParam(t.get(),"seq_hold","-1");
  t->captureActiveClip(false);assert(t->savePersistentState());
  auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(folder));restored->stopBackgroundWork();
  assert(restored->sequenceLive.soundLocks.resolve(0,0,2).find(4201)->value==11);
  assert(restored->sequenceLive.soundLocks.resolve(0,0,0).find(4201)->value==-7);
  assert(t->exportProjectFile());
  const std::string archive=t->projectOutputPath.data(),target=std::string(folder)+"/project";std::filesystem::create_directory(target);
  auto imported=std::make_unique<ThruInstance>();assert(imported->prepare(target.c_str()));imported->stopBackgroundWork();assert(imported->savePersistentState());imported->projectGeneration=imported->reservePersistenceGeneration();std::snprintf(imported->projectArchivePath.data(),imported->projectArchivePath.size(),"%s",archive.c_str());assert(imported->importProjectFile()&&imported->savePersistentState());
  imported.reset();imported=std::make_unique<ThruInstance>();assert(imported->prepare(target.c_str()));imported->stopBackgroundWork();
  assert(imported->sequenceLive.soundLocks.resolve(0,0,2).find(4201)->value==11);
  // The format-50 migration must be genuinely extension-free.
  const auto path=std::string(folder)+"/thru-clips.bin";std::ifstream in(path,std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});in.close();bytes.resize(bytes.size()-4);
  auto format51=bytes;stripScannedPrograms(format51);assert(format51[8]==51);std::uint32_t hash51=2166136261u;for(auto x:format51){hash51^=x;hash51*=16777619u;}
  auto* f51=std::fopen(path.c_str(),"wb");assert(f51);assert(std::fwrite(format51.data(),1,format51.size(),f51)==format51.size()&&writeU32(f51,hash51));std::fclose(f51);
  auto previous=tableFixture(6);assert(previous->readClipFileFromPath(path.c_str())&&previous->clipSlots[0].sequence.soundLocks.resolve(0,0,2).find(4201)->value==11);previous.reset();
  stripSequenceSoundLocks(bytes);std::uint32_t hash=2166136261u;for(auto x:bytes){hash^=x;hash*=16777619u;}
  auto* f=std::fopen(path.c_str(),"wb");assert(f);assert(std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size()&&writeU32(f,hash));std::fclose(f);
  auto legacy=tableFixture(6);assert(legacy->readClipFileFromPath(path.c_str())&&legacy->clipSlots[0].sequence.soundLocks.count==0);
  imported.reset();restored.reset();t.reset();std::filesystem::remove_all(folder);
  std::puts("Sequence sound storage: format52 parent/Burst roundtrip, real portable export/import/restart and genuine format51/50 migration: PASS");
}
void testSequenceSoundLocks(){testSequenceSoundPool();testSequenceSoundVoices();testSequenceSoundEditing();testSequenceSoundUndoAndScope();testSequenceSoundPersistence();}
