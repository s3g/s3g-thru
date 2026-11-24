std::string pageRequest(ThruInstance& t,const char* op,const char* keys="",const char* group="TEST"){
  return std::string(op)+"|"+t.pageActionContext()+"|"+group+"|"+keys;
}
void pageAct(ThruInstance& t,const char* op,const char* keys="",const char* group="TEST"){
  const auto v=pageRequest(t,op,keys,group);setParam(&t,"page_action",v.c_str());assert(t.lastError[0]==0);
}
void testPageActions(){
  {auto t=tableFixture();char state[4096]{};const int size=formatPageActions(*t,state,sizeof(state));assert(size>0&&size<int(sizeof(state)));assert(std::strstr(state,"\"safe\":\""));}
  // Every performance layout keeps copies/edits on the selected part and lane.
  for(int mode=0;mode<=6;++mode){
    auto t=tableFixture(mode);const int part=t->splitEnabled?t->partCount()-1:0;
    if(t->splitEnabled)t->selectSplitPart(part,false);
    setParam(t.get(),"table_tune","7");const int lane=t->tableEditLane(part);
    pageAct(*t,"copy","table_type,table_tune,table_level","PATCH");
    setParam(t.get(),"table_tune","-9");const auto other=t->splitParts[0].controls.table;
    pageAct(*t,"paste","table_type,table_tune,table_level","PATCH");
    assert(t->controls.table.patches[lane].v[s3g_thru_table::tune]==7&&t->splitFocus==part);
    if(part>0)assert(t->splitParts[0].controls.table.patches[0].v==other.patches[0].v);
    t->restoreUndo();assert(t->controls.table.patches[lane].v[s3g_thru_table::tune]==-9);
    pageAct(*t,"reset","table_tune");assert(t->controls.table.patches[lane].v[s3g_thru_table::tune]==0);
    t->restoreUndo();assert(t->controls.table.patches[lane].v[s3g_thru_table::tune]==-9);
    const auto before=t->controls.table;
    for(const auto& bad: {pageRequest(*t,"reset","table_tune,record_gate"),pageRequest(*t,"reset","table_tune,table_tune"),pageRequest(*t,"paste","table_tune","WRONG")}){
      setParam(t.get(),"page_action",bad.c_str());assert(t->lastError[0]);assert(t->controls.table.patches[lane].v==before.patches[lane].v);
    }
    const auto stale=pageRequest(*t,"reset","table_tune");++t->selectedLane;
    setParam(t.get(),"page_action",stale.c_str());assert(t->lastError[0]);--t->selectedLane;
    t->clearError();assert(t->controls.table.patches[lane].v==before.patches[lane].v);
    if(mode==0){
      t->polyphonyCompensation=false;pageAct(*t,"reset","polyphony_comp");assert(t->polyphonyCompensation);t->restoreUndo();assert(!t->polyphonyCompensation);
      for(int n=48;n<56;++n)t->noteOn(n,100);tableRender(*t,3);
      const auto voices=std::count_if(t->voices.begin(),t->voices.end(),[](auto& v){return v.active;});
      pageAct(*t,"copy","table_tune","PATCH");assert(std::count_if(t->voices.begin(),t->voices.end(),[](auto& v){return v.active;})==voices);
      assert(!t->audioRecording()&&!t->clipRecording);
    }
  }
  // Batch patch copying cannot let TABLE's model starter overwrite earlier fields.
  {
    auto t=tableFixture();std::string keys;
    for(const auto& p:s3g_thru_midi::parameters)if(std::string(p.key).rfind("table_",0)==0&&!p.action){if(!keys.empty())keys+=',';keys+=p.key;}
    setParam(t.get(),"table_type","2");setParam(t.get(),"table_model","4");setParam(t.get(),"table_level","36");setParam(t.get(),"table_tune","11");const auto source=t->controls.table.patches[0];
    pageAct(*t,"copy",keys.c_str(),"PATCH");setParam(t.get(),"table_model","1");pageAct(*t,"paste",keys.c_str(),"PATCH");assert(t->controls.table.patches[0].v==source.v);
    t->beginClipRecording();const auto before=t->controls.table.patches[0];const auto request=pageRequest(*t,"reset","table_tune");setParam(t.get(),"page_action",request.c_str());assert(t->lastError[0]&&t->controls.table.patches[0].v==before.v);t->finishClipRecording();
  }
  {
    auto t=tableFixture(6);setParam(t.get(),"seq_type","1");setParam(t.get(),"seq_recipe_hits","7");
    assert(t->sequenceLive.soundLocks.set(0,3,0,4201,9));pageAct(*t,"laneCopy");
    t->selectSplitPart(1,false);const auto engine=t->controls.algorithm;pageAct(*t,"lanePaste");
    assert(t->sequenceLive.tracks[1].generator.values[1]==7&&t->controls.algorithm==engine);
    assert(t->sequenceLive.soundLocks.resolve(1,3,0).find(4201)->value==9);
    pageAct(*t,"laneClear");for(const auto& step:t->sequenceLive.tracks[1].trigs)assert(!step.enabled);
    t->setClipPlaying(true);tableRender(*t,2);for(const auto& step:t->sequenceLive.tracks[1].trigs)assert(!step.enabled);t->setClipPlaying(false);
    t->selectSplitPart(0,false);setParam(t.get(),"seq_hold","3");setParam(t.get(),"table_tune","5");
    auto& step=t->sequenceLive.tracks[0].trigs[3];step.enabled=true;step.probability=.4f;step.count=3;step.events[0].locks.mask=1;step.locks.mask=15;
    const auto heldRequest=pageRequest(*t,"trigClearLocks");setParam(t.get(),"seq_hold","-1");setParam(t.get(),"page_action",heldRequest.c_str());assert(t->lastError[0]&&step.locks.mask==15);
    setParam(t.get(),"seq_hold","3");pageAct(*t,"trigClearLocks");assert(step.enabled&&step.probability==.4f&&step.count==3&&step.locks.mask==0&&step.events[0].locks.mask==0&&t->sequenceLive.soundLocks.stepCount(0,3)==0);
    setParam(t.get(),"seq_hold","-1");pageAct(*t,"genCopy");t->selectSplitPart(2,false);pageAct(*t,"genPaste");assert(t->sequenceLive.tracks[2].generator.type==1&&t->sequenceLive.tracks[2].generator.values[1]==7);t->restoreUndo();assert(t->sequenceLive.tracks[2].generator.type==0);
  }
  {
    auto t=tableFixture(1);t->selectSplitPart(1,false);t->editStackPath("lane",1,.8f);const auto path=t->controls.stackPath;pageAct(*t,"pathCopy");t->selectSplitPart(0,false);pageAct(*t,"pathPaste");assert(t->controls.stackPath.time==path.time&&t->controls.stackPath.lane==path.lane&&t->controls.stackPath.curve==path.curve);
  }
  std::puts("Page Actions: seven layouts, exact patch copies, atomic validation, target/release guards, scoped sequence copying/clearing, Undo, live voices and recording protection: PASS");
}
