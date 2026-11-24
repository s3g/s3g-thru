// Labels change on the serialized control path; disk work owns immutable inputs.
bool namingOpen=false;
std::atomic<int> namingJob{0}; // idle, waiting for save, requested, worker, result, publish refs
int namingClipCursor=0,namingOriginClip=-1,namingOriginLane=0;
std::string namingOperation,namingSource,namingDestination,namingMessage,namingValue;
bool namingSuccess=false,namingRecovered=false;
std::uint64_t namingGeneration=0;
bool namingLoading()const{const auto state=static_cast<SampleLoadState>(sampleLoadState.load(std::memory_order_acquire));return pendingClipSourceMask||(state>=SampleLoadState::requested&&state<=SampleLoadState::copying);}
bool namingBusy()const{return namingJob.load(std::memory_order_acquire)!=0;}
static bool namingFileControl(const char* key){
 return !std::strncmp(key,"project_",8)||!std::strcmp(key,"split_mode")||!std::strcmp(key,"clip_select")||!std::strcmp(key,"chain_launch")||!std::strcmp(key,"load_lane_file")||!std::strcmp(key,"record_gate")||!std::strcmp(key,"clear_lane")||!std::strcmp(key,"sample_trim_apply")||!std::strcmp(key,"sample_reverse")||!std::strcmp(key,"sample_normalize")||!std::strcmp(key,"sample_fade")||!std::strcmp(key,"state")||!std::strcmp(key,"undo")||!std::strcmp(key,"synth_save")||!std::strcmp(key,"clip_record");
}
std::string namingContext()const{return std::to_string(activeClip)+","+std::to_string(splitFocus)+","+std::to_string(selectedLane)+","+std::to_string(bankSelected)+","+std::to_string(chainBankIndex)+","+std::to_string(chainEditSlot)+","+std::to_string(performanceMode());}
static std::string namingQuoted(const char* s){std::string out="\"";for(;*s;++s){if(*s=='"'||*s=='\\')out+='\\';if(static_cast<unsigned char>(*s)>=32)out+=*s;}return out+'"';}
int formatNamingState(char* out,int cap)const {
 const int job=namingJob.load(std::memory_order_acquire);
 const auto message=job==3||job==2?"WORKING":job==1?"SAVING CURRENT SET":namingMessage.c_str();
 const auto path=job==0?namingDestination.c_str():"";
 return std::snprintf(out,cap,"{\"context\":\"%s\",\"open\":%d,\"job\":%d,\"ok\":%d,\"message\":%s,\"path\":%s}",namingContext().c_str(),namingOpen,job,(job==0||job>=4)?namingSuccess:false,namingQuoted(message).c_str(),namingQuoted(path).c_str());
}
int formatNamingLabels(char* out,int cap,int bank=-1)const{
 bank=clampValue(bank<0?activeClip/16:bank,0,3);
 std::string json="{\"bank\":"+std::to_string(bank)+",\"project\":"+namingQuoted(projectTitle.data())+",\"clips\":[";
 for(int i=0;i<16;++i){if(i)json+=',';json+=namingQuoted(clipSlots[bank*16+i].name.data());}json+="],\"chains\":[";
 for(int i=0;i<16;++i){if(i)json+=',';json+=namingQuoted(chainBanks[i].name.data());}json+="],\"pads\":[";
 for(int i=0;i<16;++i){if(i)json+=',';json+=namingQuoted(bankSlots[i].name.data());}json+="],\"lane\":"+namingQuoted(laneSourceLabels[selectedLane].data())+",\"source\":"+namingQuoted(currentLaneSourcePaths[selectedLane].data())+"}";
 return copyText(json.c_str(),out,cap);
}
static bool namingBelow(const std::string& path,const std::string& root){return !root.empty()&&path.size()>root.size()+1&&path.compare(0,root.size(),root)==0&&path[root.size()]=='/';}
bool namingManaged(const std::string& path)const{
 return namingBelow(path,std::string(moduleDirectory.data())+"/captures")||namingBelow(path,synthDirectory.data());
}
static bool namingSwapPath(std::array<char,kPersistencePathSize>& p,const std::string& from,const std::string& to){if(p.data()!=from)return false;std::snprintf(p.data(),p.size(),"%s",to.c_str());return true;}
static void namingSwapClip(ClipSlot& c,const std::string& from,const std::string& to,const std::string& label){
 for(int l=0;l<4;++l)if(namingSwapPath(c.laneSourcePaths[l],from,to)){
  const auto* old=c.laneSourceLabels[l].data();if(!old[0]||!std::strncmp(old,"TAKE ",5)||std::strstr(old,".wav"))std::snprintf(c.laneSourceLabels[l].data(),c.laneSourceLabels[l].size(),"%s",label.c_str());
 }
 for(auto& s:c.bankSlots)namingSwapPath(s.path,from,to);
}
bool setNamingParam(const char* key,const char* text){
 if(!std::strcmp(key,"naming_open")){namingOpen=isTruthy(text);return true;}
 if(std::strcmp(key,"naming_apply"))return false;
 clearError();if(namingBusy()){setError("Finish file naming first");return true;}
 std::array<std::string,4> f;const std::string request=text;std::size_t at=0;
 for(int i=0;i<3;++i){auto end=request.find('\n',at);if(end==std::string::npos){setError("Invalid name request");return true;}f[i]=request.substr(at,end-at);at=end+1;}f[3]=request.substr(at);
 if(f[1]!=namingContext()){setError("Naming target changed");return true;}
 if(!s3g_thru_names::valid(f[2])){setError("Use 40 letters/numbers");return true;}
 if(clipRecording||audioRecording()||bankJob.load()!=0||patternBusy()||clipPreparationInFlight()||projectLocked()){setError("Finish recording or fade");return true;}
 const auto& kind=f[0];
 if((kind=="clip"||kind=="chain"||kind=="bank"||kind=="lane"||kind=="project")&&!f[3].empty()){setError("Invalid name text");return true;}
 if(kind=="clip"){pushClipUndo(activeClip);s3g_thru_names::assign(clipSlots[activeClip].name,f[2]);}
 else if(kind=="chain"){pushChainUndo("name",chainEditSlot);s3g_thru_names::assign(currentChain().name,f[2]);}
 else if(kind=="bank"){pushClipUndo(activeClip);s3g_thru_names::assign(bankSlots[bankSelected].name,f[2]);clipSlots[activeClip].bankSlots[bankSelected].name=bankSlots[bankSelected].name;}
 else if(kind=="lane"){pushClipUndo(activeClip);std::snprintf(laneSourceLabels[selectedLane].data(),64,"%s",f[2].c_str());clipSlots[activeClip].laneSourceLabels[selectedLane]=laneSourceLabels[selectedLane];}
 else if(kind=="project"){s3g_thru_names::assign(projectTitle,f[2]);}
 else if(kind=="rename"||kind=="copy"||kind=="copyload"||kind=="projectfile"){
  if(f[2].empty()||f[2].find_first_not_of(' ')==std::string::npos||f[3].empty()||f[3].size()>=kPersistencePathSize||f[3].find('\n')!=std::string::npos){setError("Enter a filename");return true;}
  if(!persistenceEnabled||!backgroundStarted){setError("Storage unavailable");return true;}
  if(clipPlaying||clipLaunchPending||pendingCaptureSaveMask||namingLoading()||pendingClearMask){setError("Stop clip and finish save");return true;}
  if(kind=="rename"&&!namingManaged(f[3])){setError("Use Save Named Copy");return true;}
  captureActiveClip(false);markPersistentStateDirty();maintenanceRequested=true;
  namingOperation=kind;namingSource=f[3];namingValue=f[2];namingDestination.clear();namingMessage="SAVING CURRENT SET";namingSuccess=false;namingRecovered=false;namingOriginClip=activeClip;namingOriginLane=int(selectedLane);namingJob.store(1,std::memory_order_release);return true;
 }else{setError("Unknown naming target");return true;}
 markPersistentStateDirty();namingMessage="NAME SAVED";namingSuccess=true;return true;
}
static bool namingRegular(const std::string& path){struct stat s{};return lstat(path.c_str(),&s)==0&&S_ISREG(s.st_mode);}
bool namingJournal(const std::string& from,const std::string& to,const std::string& label){
 std::array<char,kPersistencePathSize> file{},temp{};if(!persistencePath("thru-rename.txt",file.data(),file.size())||!persistencePath("thru-rename.txt.tmp",temp.data(),temp.size()))return false;
 auto* f=std::fopen(temp.data(),"wb");if(!f)return false;const std::string body=from+"\n"+to+"\n"+label+"\n";
 const bool wrote=writeBytes(f,body.data(),body.size());if(!s3g_thru_storage::closeFile(f,temp.data(),wrote))return false;
 return s3g_thru_storage::rename(temp.data(),file.data())&&s3g_thru_storage::syncParent(file.data());
}
bool processNamingJob(){
 int pending=2;if(!namingJob.compare_exchange_strong(pending,3,std::memory_order_acq_rel))return false;
 namingSuccess=false;namingMessage="FILE SAVE FAILED";
 try{
  const bool projects=namingOperation=="projectfile",rename=namingOperation=="rename";
  std::array<char,kPersistencePathSize> root{};projectDirectory(root.data(),root.size());
  const auto allowed=[&](const std::string& path){char real[4096]{};if(!realpath(path.c_str(),real))return false;for(const char* base:std::array<const char*,4>{moduleDirectory.data(),kSampleLibraryRoot,synthDirectory.data(),root.data()}){char resolved[4096]{};if(realpath(base,resolved)&&namingBelow(real,resolved))return true;}return false;};
  if(namingRecovered&&!namingRegular(namingSource)&&namingRegular(namingDestination)&&allowed(namingDestination)){
   std::array<char,kPersistencePathSize> journal{};persistencePath("thru-rename.txt",journal.data(),journal.size());std::remove(journal.data());s3g_thru_storage::syncParent(journal.data());namingSuccess=true;namingMessage="FILE NAME RECOVERED";namingJob.store(4,std::memory_order_release);return true;
  }
  if(!allowed(namingSource)||!namingRegular(namingSource)){namingMessage="FILE UNAVAILABLE";namingJob.store(4,std::memory_order_release);return true;}
  if(projects&&!namingBelow(namingSource,root.data()))throw std::runtime_error("project path");
  if(rename&&!namingManaged(namingSource))throw std::runtime_error("managed path");
  const std::string extension=projects?".thru":".wav";const auto slash=namingSource.rfind('/');
  std::string suffix=namingSource.size()<extension.size()?"":namingSource.substr(namingSource.size()-extension.size());for(auto& c:suffix)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if(suffix!=extension)throw std::runtime_error("extension");
  std::string directory=namingSource.substr(0,slash),stem=namingValue;
  if(rename){const auto leaf=namingSource.substr(slash+1);const auto dash=leaf.find('-');if(leaf.size()>2&&leaf[0]=='t'&&dash!=std::string::npos&&leaf.substr(1,dash-1).find_first_not_of("0123456789")==std::string::npos)stem=leaf.substr(0,dash+1)+stem;}
  if(!rename&&!projects){directory=std::strncmp(moduleDirectory.data(),"/data/",6)?std::string(moduleDirectory.data())+"/named":std::string(kSampleLibraryRoot)+"s3g Thru/Named";if(!std::strncmp(moduleDirectory.data(),"/data/",6))projectMakeDirectory((std::string(kSampleLibraryRoot)+"s3g Thru").c_str());if(!projectMakeDirectory(directory.c_str()))throw std::runtime_error("directory");}
  if(!namingRecovered){for(int suffix=0;suffix<10000;++suffix){const std::string name=stem+(suffix?"-"+std::to_string(suffix):"")+extension;namingDestination=directory+"/"+name;if(namingDestination.size()>=kPersistencePathSize)throw std::runtime_error("path length");struct stat st{};if(lstat(namingDestination.c_str(),&st)<0&&errno==ENOENT)break;if(suffix==9999)throw std::runtime_error("names used");}}
  if(rename||projects){
   if(rename&&!namingRecovered&&!namingJournal(namingSource,namingDestination,namingValue))throw std::runtime_error("journal");
   if(!namingRecovered&&(!s3g_thru_storage::link(namingSource.c_str(),namingDestination.c_str())||!s3g_thru_storage::syncParent(namingDestination.c_str())))throw std::runtime_error("link");
   if(rename){
    auto snapshot=std::make_unique<PersistenceSnapshot>();if(!readProjectSnapshot(moduleDirectory.data(),*snapshot))throw std::runtime_error("metadata read");
    for(auto& c:snapshot->clips)namingSwapClip(c,namingSource,namingDestination,namingValue);
    snapshot->generation=namingGeneration;std::snprintf(snapshot->directory.data(),snapshot->directory.size(),"%s",moduleDirectory.data());
    if(!projectJsonNumber(snapshot->json,"generation",0,std::to_string(snapshot->generation).c_str())||!writeMetadataFiles(snapshot.get())||!writeMetadataFiles(snapshot.get()))throw std::runtime_error("metadata save");
   }
   if(std::remove(namingSource.c_str())!=0&&errno!=ENOENT)throw std::runtime_error("old path");
   if(!s3g_thru_storage::syncParent(namingSource.c_str()))throw std::runtime_error("directory sync");
   if(rename){std::array<char,kPersistencePathSize> journal{};persistencePath("thru-rename.txt",journal.data(),journal.size());if(std::remove(journal.data())!=0&&errno!=ENOENT)throw std::runtime_error("journal cleanup");s3g_thru_storage::syncParent(journal.data());}
  }else{
   const std::string temp=namingDestination+".tmp";auto* in=std::fopen(namingSource.c_str(),"rb");auto* out=std::fopen(temp.c_str(),"wb");bool ok=in&&out;std::array<char,65536> buffer{};
   if(in&&out){while(ok){const auto n=std::fread(buffer.data(),1,buffer.size(),in);if(n&&!writeBytes(out,buffer.data(),n)){ok=false;break;}if(n<buffer.size()){ok=!std::ferror(in);break;}}}
   if(in)std::fclose(in);
   if(out)ok=s3g_thru_storage::closeFile(out,temp.c_str(),ok)&&ok;
   if(!ok||!s3g_thru_storage::link(temp.c_str(),namingDestination.c_str())||!s3g_thru_storage::syncParent(namingDestination.c_str())){std::remove(temp.c_str());throw std::runtime_error("copy");}std::remove(temp.c_str());
  }
  namingSuccess=true;namingMessage=rename||projects?"FILE RENAMED":"NAMED COPY SAVED";
 }catch(...){if(errno==ENOSPC)namingMessage="NOT ENOUGH SPACE";}
 namingJob.store(4,std::memory_order_release);return true;
}
void serviceNaming(){
 int job=namingJob.load(std::memory_order_acquire);
 if(job==1&&(saveFailed||bankSaveFailed||customSaveFailed)){namingSuccess=false;namingMessage="RESOLVE SAVE ERROR";namingJob.store(0,std::memory_order_release);return;}
 if(job==1&&!namingLoading()&&!persistentStateDirty&&persistenceJob.load()==backgroundIdle&&laneJob.load()==backgroundIdle&&bankJob.load()==0&&!clipPreparationInFlight()){
  namingGeneration=reservePersistenceGeneration();if(namingGeneration)namingJob.store(2,std::memory_order_release);else{namingMessage="SAVE LIMIT";namingJob.store(0);}return;
 }
 if(job==4){
  if(!namingSuccess){setError(namingMessage.c_str());namingJob.store(0,std::memory_order_release);return;}
  if(namingOperation!="rename"){namingJob.store(0,std::memory_order_release);return;}
  namingClipCursor=0;namingJob.store(5,std::memory_order_release);job=5;
 }
 if(job==5){
  // One clip per callback, independent of how many references it contains.
  if(namingClipCursor<64){namingSwapClip(clipSlots[namingClipCursor++],namingSource,namingDestination,namingValue);return;}
  for(auto& path:pendingClipSourcePaths)namingSwapPath(path,namingSource,namingDestination);
  namingSwapClip(undoClip,namingSource,namingDestination,namingValue);namingSwapClip(clipClipboard,namingSource,namingDestination,namingValue);
  for(int l=0;l<4;++l)if(namingSwapPath(currentLaneSourcePaths[l],namingSource,namingDestination)){const char* old=laneSourceLabels[l].data();if(!old[0]||!std::strncmp(old,"TAKE ",5)||std::strstr(old,".wav"))std::snprintf(laneSourceLabels[l].data(),64,"%s",namingValue.c_str());}
  for(auto& slot:bankSlots)namingSwapPath(slot.path,namingSource,namingDestination);
  namingSwapPath(bankUndoSlot.path,namingSource,namingDestination);
  if(bankPool)for(auto& asset:bankPool->assets)namingSwapPath(asset.path,namingSource,namingDestination);
  persistenceGeneration=std::max(persistenceGeneration,namingGeneration);observePersistenceGeneration(namingGeneration);markPersistentStateDirty();namingJob.store(0,std::memory_order_release);
 }
}
void recoverNaming(){
 std::array<char,kPersistencePathSize> journal{};if(!persistencePath("thru-rename.txt",journal.data(),journal.size()))return;auto* file=std::fopen(journal.data(),"rb");if(!file)return;
 std::array<char,kPersistencePathSize> old{},next{},label{};const bool ok=std::fgets(old.data(),old.size(),file)&&std::fgets(next.data(),next.size(),file)&&std::fgets(label.data(),label.size(),file);std::fclose(file);if(!ok)return;
 for(auto* s:{old.data(),next.data(),label.data()})if(auto* nl=std::strchr(s,'\n'))*nl=0;
 if(!namingManaged(old.data())||!namingManaged(next.data())||!s3g_thru_names::valid(label.data()))return;
 if(!namingRegular(next.data())){std::remove(journal.data());return;}
 // Startup replay changes paths only, never playback controls or sample PCM.
 namingOperation="rename";namingSource=old.data();namingDestination=next.data();namingValue=label.data();namingRecovered=true;namingSuccess=false;namingMessage="RECOVERING FILE NAME";namingJob.store(1,std::memory_order_release);markPersistentStateDirty();maintenanceRequested=true;
}
