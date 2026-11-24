void namingApply(ThruInstance& t,const char* kind,const std::string& name,const std::string& path=""){
 const auto request=std::string(kind)+"\n"+t.namingContext()+"\n"+name+"\n"+path;setParam(&t,"naming_apply",request.c_str());if(t.lastError[0])std::fprintf(stderr,"Naming %s / %s: %s\n",kind,name.c_str(),t.lastError.data());assert(!t.lastError[0]);
}
void namingRun(ThruInstance& t){
 t.backgroundStarted=false;assert(t.writeMetadataFiles());t.persistentStateDirty=false;t.serviceNaming();assert(t.namingJob.load()==2);
 std::thread worker([&]{assert(t.processNamingJob());});worker.join();
 for(int i=0;i<70&&t.namingBusy();++i)t.serviceNaming();
 assert(!t.namingBusy());
}
void testNaming(){
 char folder[]="/tmp/thru-names.XXXXXX";assert(mkdtemp(folder));const std::string base=folder;
 auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();
 namingApply(*t,"clip","Organic Pulse");namingApply(*t,"chain","Slow Drift");namingApply(*t,"project","Evening Study");
 t->setPerformanceMode(4);namingApply(*t,"bank","Metal Hit");t->captureActiveClip(false);
 assert(std::string(t->clipSlots[0].name.data())=="Organic Pulse"&&std::string(t->bankSlots[0].name.data())=="Metal Hit");
 assert(t->savePersistentState());t.reset();
 t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();
 assert(std::string(t->clipSlots[0].name.data())=="Organic Pulse"&&std::string(t->chainBanks[0].name.data())=="Slow Drift"&&std::string(t->projectTitle.data())=="Evening Study"&&std::string(t->bankSlots[0].name.data())=="Metal Hit");
 auto snapshot=std::make_unique<ThruInstance::PersistenceSnapshot>();assert(t->readProjectSnapshot(base,*snapshot));assert(std::string(snapshot->clips[0].bankSlots[0].name.data())=="Metal Hit");snapshot.reset();
 namingApply(*t,"clip","Renamed");t->restoreUndo();assert(std::string(t->clipSlots[0].name.data())=="Organic Pulse");
 std::array<char,kStateTextSize> numericState{};assert(t->serializeState(numericState.data(),int(numericState.size()))>0);assert(t->restoreState(numericState.data()));
 assert(std::string(t->clipSlots[0].name.data())=="Organic Pulse"&&std::string(t->bankSlots[0].name.data())=="Metal Hit");
 const auto prior=t->clipSlots[0].name;
 assert(!t->restoreState("{\"version\":999,\"complete\":1,\"generation\":1,\"names_v1\":\"C0:Wrong\"}"));assert(t->clipSlots[0].name==prior);
 assert(!s3g_thru_names::read("{\"names_v1\":\"C0:Wrong|bad:Oops\"}",t->clipSlots,t->chainBanks,t->projectTitle));assert(t->clipSlots[0].name==prior);

 for(const auto& invalid:{std::string("bad/name"),std::string(41,'X'),std::string("a\nb"),std::string("\"oops")}){
  const auto request=std::string("clip\n")+t->namingContext()+"\n"+invalid+"\n";setParam(t.get(),"naming_apply",request.c_str());assert(t->lastError[0]&&t->clipSlots[0].name==prior);
 }
 const auto stale=std::string("clip\n")+t->namingContext()+"\nWrong clip\n";++t->activeClip;setParam(t.get(),"naming_apply",stale.c_str());assert(t->lastError[0]);--t->activeClip;t->clearError();
 // Maximum labels remain bounded in state JSON and in GUI bank queries.
 for(int c=0;c<64;++c){s3g_thru_names::assign(t->clipSlots[c].name,std::string(40,'C'));for(auto& b:t->clipSlots[c].bankSlots)s3g_thru_names::assign(b.name,std::string(40,'B'));}
 for(auto& b:t->chainBanks)s3g_thru_names::assign(b.name,std::string(40,'K'));
 // Leave room for a dense numeric state as well as every possible label.
 std::array<char,kStateTextSize> dense{};const auto numeric=std::string("{\"numeric_state\":\"")+std::string(90000,'0')+"\"}";std::memcpy(dense.data(),numeric.c_str(),numeric.size()+1);
 assert(s3g_thru_names::append(dense.data(),dense.size(),t->clipSlots,t->chainBanks,t->projectTitle));assert(std::strlen(dense.data())>131072);
 assert(t->savePersistentState());char label[4096]{};assert(t->formatNamingLabels(label,sizeof(label))<int(sizeof(label)));
 std::array<char,kStateTextSize> state{};assert(t->readStateTextFromPath((base+"/thru-state.json").c_str(),state));assert(s3g_thru_names::blob(state.data()).size()>40000);
 // Reset the test bank; a real stereo file is shared by lanes and Bank pads.
 for(auto& c:t->clipSlots)c={};t->bankSlots={};t->setPerformanceMode(0);t->activeClip=0;
 const std::string source=base+"/captures/t0000000001-input.wav";std::array<float,128> left{},right{};for(int i=0;i<128;++i){left[i]=.2f*std::sin(i*.1f);right[i]=left[i]*.7f;}
 assert(ThruInstance::writeStereoWavFile(left.data(),right.data(),128,44100,source.c_str(),(source+".tmp").c_str(),true));
 const auto assign=[&](auto& out,const std::string& path){std::snprintf(out.data(),out.size(),"%s",path.c_str());};
 assign(t->currentLaneSourcePaths[0],source);assign(t->clipSlots[0].laneSourcePaths[0],source);t->currentLaneSourceKinds[0]=LaneSourceKind::capture;t->clipSlots[0].laneSourceKinds[0]=LaneSourceKind::capture;
 assign(t->clipSlots[1].laneSourcePaths[2],source);t->clipSlots[1].laneSourceKinds[2]=LaneSourceKind::capture;assign(t->clipSlots[2].bankSlots[4].path,source);t->clipSlots[1].occupied=t->clipSlots[2].occupied=true;
 t->backgroundStarted=true;namingApply(*t,"rename","Dust Loop",source);
 t->setClipPlaying(true);assert(!t->clipPlaying);t->selectClip(1);assert(t->activeClip==0);t->midiSyncSource=1;t->midiSyncTransport=1;t->handleMidiRealtime(0xfa,MOVE_MIDI_SOURCE_EXTERNAL);assert(!t->clipPlaying);t->clearError();
 namingRun(*t);assert(t->namingSuccess);
 const auto renamed=t->namingDestination;assert(renamed.find("t0000000001-Dust Loop.wav")!=std::string::npos);assert(access(source.c_str(),F_OK)!=0&&access(renamed.c_str(),F_OK)==0);
 assert(std::string(t->currentLaneSourcePaths[0].data())==renamed&&std::string(t->clipSlots[1].laneSourcePaths[2].data())==renamed&&std::string(t->clipSlots[2].bankSlots[4].path.data())==renamed);
 // Both durable generations use the new file before the old path disappears.
 for(const auto& suffix:{"", ".bak"}){auto image=std::make_unique<ThruInstance::PersistenceSnapshot>();std::array<char,kStateTextSize> json{};assert(t->readStateTextFromPath((base+"/thru-state.json"+suffix).c_str(),json));assert(t->readClipFileFromPath((base+"/thru-clips.bin"+suffix).c_str(),kAnyPersistenceGeneration,image.get()));assert(std::string(image->clips[1].laneSourcePaths[2].data())==renamed);}
 t->backgroundStarted=true;namingApply(*t,"copy","Shared Copy",renamed);namingRun(*t);assert(t->namingSuccess&&access(renamed.c_str(),F_OK)==0&&access(t->namingDestination.c_str(),F_OK)==0);
 const auto bytes=[](const std::string& file){std::ifstream in(file,std::ios::binary);return std::vector<char>(std::istreambuf_iterator<char>(in),{});};
 assert(bytes(renamed)==bytes(t->namingDestination));
 const auto copy=t->namingDestination;t->backgroundStarted=true;namingApply(*t,"copy","Shared Copy",renamed);namingRun(*t);assert(t->namingSuccess&&copy!=t->namingDestination);
 // A denied publish never removes the usable original.
 t->backgroundStarted=true;namingApply(*t,"rename","Faulted",renamed);t->backgroundStarted=false;assert(t->writeMetadataFiles());t->persistentStateDirty=false;t->serviceNaming();
 s3g_thru_storage::testHook=[](StorageOp op,const char*,const char*){return op!=StorageOp::link;};std::thread failed([&]{assert(t->processNamingJob());});failed.join();s3g_thru_storage::testHook=nullptr;t->serviceNaming();assert(!t->namingSuccess&&access(renamed.c_str(),F_OK)==0);
 // Recovery completes a rename interrupted after publishing the new link.
 const auto recovered=base+"/captures/t0000000001-Recovered.wav";assert(t->namingJournal(renamed,recovered,"Recovered"));assert(::link(renamed.c_str(),recovered.c_str())==0);
 assert(t->savePersistentState());t.reset();t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();assert(t->namingBusy());waitProjectSources(*t,[&]{return !t->namingLoading();});namingRun(*t);
 assert(t->namingSuccess&&access(renamed.c_str(),F_OK)!=0&&std::string(t->clipSlots[1].laneSourcePaths[2].data())==recovered);
 // Exit immediately after accepting a request still completes safely off audio.
 t->backgroundStarted=true;namingApply(*t,"rename","Exit Saved",recovered);t->backgroundStarted=false;assert(t->savePersistentState());const auto finalPath=t->namingDestination;
 assert(t->namingSuccess&&!t->namingBusy()&&access(finalPath.c_str(),F_OK)==0&&access(recovered.c_str(),F_OK)!=0);
 // Process-compatible keyboard names and stable names survive portable export/import.
 t->clearError();namingApply(*t,"project","Named Export");assert(t->savePersistentState());assert(t->exportProjectFile());assert(std::string(t->projectOutputPath.data()).find("Named Export-")!=std::string::npos);
 const auto exported=t->projectOutputPath;
 const auto target=base+"/restored";std::filesystem::create_directory(target);auto restored=std::make_unique<ThruInstance>();assert(restored->prepare(target.c_str()));restored->stopBackgroundWork();assert(restored->savePersistentState());
 restored->projectGeneration=restored->reservePersistenceGeneration();std::snprintf(restored->projectArchivePath.data(),restored->projectArchivePath.size(),"%s",exported.data());assert(restored->importProjectFile()&&restored->savePersistentState());restored.reset();
 restored=std::make_unique<ThruInstance>();assert(restored->prepare(target.c_str()));restored->stopBackgroundWork();assert(std::string(restored->projectTitle.data())=="Named Export"&&std::string(restored->chainBanks[0].name.data())==std::string(40,'K'));waitProjectSources(*restored,[&]{return restored->engine.laneFrames(0)>0;});restored.reset();
 // Naming pads never start a voice or an empty Bank recording.
 t->setPerformanceMode(4);setParam(t.get(),"naming_open","1");const uint8_t press[3]{0x90,72,100},release[3]{0x80,72,0};onMidi(t.get(),press,3,MOVE_MIDI_SOURCE_INTERNAL);onMidi(t.get(),release,3,MOVE_MIDI_SOURCE_INTERNAL);assert(t->bankJob.load()==0);setParam(t.get(),"naming_open","0");
 t.reset();std::filesystem::remove_all(base);
 std::puts("Shared naming: restart, full label budgets, Undo, stale/invalid names, shared lane/Bank rename, durable backups, collision-safe exact copies, fault preservation, interrupted/exit recovery, portable import and text-pad isolation: PASS");
}
