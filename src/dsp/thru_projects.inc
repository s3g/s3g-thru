    // Project I/O runs on the existing background worker. Only atomic progress
    // and the request/result handshake cross into the render callback.
    std::array<char,kPersistencePathSize> moduleRootDirectory{};
    enum { projectIdle,projectWaiting,projectRequested,projectWorking,projectComplete };
    std::atomic<int> projectJob{projectIdle};
    std::atomic<bool> projectCancel{false};
    std::atomic<std::uint64_t> projectBytes{0},projectTotalBytes{0};
    int projectAction=0,projectPage=0,projectFileCount=0,projectListed=0;
    bool projectSuccess=false;
    std::atomic<bool> projectImportInstalled{false};
    bool projectPreferRelocatedPrimary=false;
    bool projectDataSelected=false,projectRecoveredRelativePaths=false;
    std::array<std::array<char,81>,32> projectFiles{};
    std::array<char,kPersistencePathSize> projectArchivePath{},projectOutputPath{};
    std::array<char,96> projectMessage{};
    std::array<char,64> projectActivation{};
    std::uint64_t projectGeneration=0;
    static bool projectIdValid(const char* id) {
        if(!id||id[0]!='p'||id[1]!='-'||std::strlen(id)>60)return false;
        for(const char* p=id+2;*p;++p)if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||*p=='-'))return false;
        return id[2]!=0;
    }
    static bool projectJoinPath(const char* root,const char* leaf,char* out,std::size_t size) {
        if(!root||!root[0]||!leaf||!out)return false;
        std::array<char,kPersistencePathSize> joined;
        const auto capacity=std::min(size,joined.size());
        const auto rootLength=std::strlen(root),leafLength=std::strlen(leaf);
        if(capacity<2||rootLength>capacity-2||leafLength>capacity-rootLength-2)return false;
        // Stage both components before publishing, including aliasing callers.
        std::memcpy(joined.data(),root,rootLength);joined[rootLength]='/';
        std::memcpy(joined.data()+rootLength+1,leaf,leafLength+1);
        std::memmove(out,joined.data(),rootLength+leafLength+2);return true;
    }
    bool rootDataPath(const char* leaf,char* out,std::size_t size) const {
        const char* root=moduleRootDirectory[0]?moduleRootDirectory.data():moduleDirectory.data();
        return projectJoinPath(root,leaf,out,size);
    }
    bool controllerDataPath(const char* leaf,char* out,std::size_t size) const {return rootDataPath(leaf,out,size);}
    bool metadataPath(const PersistenceSnapshot* snapshot,const char* leaf,char* out,std::size_t size) const {
        if(!snapshot||!snapshot->directory[0])return persistencePath(leaf,out,size);
        const int n=std::snprintf(out,size,"%s/%s",snapshot->directory.data(),leaf);return n>0&&static_cast<std::size_t>(n)<size;
    }
    bool projectDirectory(char* out,std::size_t size) const {
        if(!std::strncmp(moduleRootDirectory.data(),"/data/",6)) {
            const int n=std::snprintf(out,size,"%sThru Projects",kSampleLibraryRoot);return n>0&&static_cast<std::size_t>(n)<size;
        }
        return rootDataPath("projects",out,size);
    }
    static bool projectMakeDirectory(const char* path) {
        if(::mkdir(path,0700)==0)return s3g_thru_storage::syncParent(path);
        struct stat info{};return errno==EEXIST&&::lstat(path,&info)==0&&S_ISDIR(info.st_mode);
    }
    static bool projectReadId(const char* path,char* id,std::size_t size) {
        auto* file=std::fopen(path,"rb");if(!file)return false;
        std::array<char,96> line{};const auto n=std::fread(line.data(),1,line.size()-1,file);
        const bool ok=std::feof(file)!=0;std::fclose(file);
        if(!ok||n<15||std::strncmp(line.data(),"THRUPROJECT1 ",13))return false;
        auto* end=std::strchr(line.data()+13,'\n');if(end)*end=0;
        if(!projectIdValid(line.data()+13)||std::strlen(line.data()+13)>=size)return false;
        std::strcpy(id,line.data()+13);return true;
    }
    void selectProjectDataDirectory() {
        if(!moduleRootDirectory[0])return;
        for(const char* leaf:{"thru-active-project.txt","thru-active-project.txt.bak"}) {
            std::array<char,kPersistencePathSize> pointer{},directory{},state{},clips{};
            std::array<char,64> id{};std::uint64_t sg=0,cg=0;
            if(!rootDataPath(leaf,pointer.data(),pointer.size())||!projectReadId(pointer.data(),id.data(),id.size()))continue;
            const std::string relative=std::string("project-data/")+id.data();
            if(!rootDataPath(relative.c_str(),directory.data(),directory.size()))continue;
            struct stat info{};if(::lstat(directory.data(),&info)!=0||!S_ISDIR(info.st_mode))continue;
            if(std::snprintf(state.data(),state.size(),"%s/thru-state.json",directory.data())>=static_cast<int>(state.size())
                ||std::snprintf(clips.data(),clips.size(),"%s/thru-clips.bin",directory.data())>=static_cast<int>(clips.size()))continue;
            bool paired=false;
            for(const char* suffix:{"",".bak"})for(const char* other:{"",".bak"})
                if(stateFileGeneration((std::string(state.data())+suffix).c_str(),sg)
                    &&clipFileGeneration((std::string(clips.data())+other).c_str(),cg)&&sg==cg)paired=true;
            if(!paired)continue;
            moduleDirectory=directory;projectDataSelected=true;
            projectPreferRelocatedPrimary=preferRelocatedProjectPrimary(directory.data());
            if(projectPreferRelocatedPrimary&&g_host&&g_host->log)
                g_host->log("s3g Thru recovered imported project sample links");
            return;
        }
    }
    bool projectLocked() const {return projectImportInstalled||(projectAction!=3&&projectJob.load(std::memory_order_acquire)!=projectIdle);}
    void projectNote(const char* text) {std::snprintf(projectMessage.data(),projectMessage.size(),"%s",text);}
    static void projectRemoveTree(const std::string& root) {
        auto* dir=opendir(root.c_str());if(!dir)return;
        while(auto* entry=readdir(dir)) {
            if(!std::strcmp(entry->d_name,".")||!std::strcmp(entry->d_name,".."))continue;
            const std::string path=root+"/"+entry->d_name;struct stat info{};
            if(lstat(path.c_str(),&info)==0&&S_ISDIR(info.st_mode))projectRemoveTree(path);
            else std::remove(path.c_str());
        }
        closedir(dir);::rmdir(root.c_str());
    }
    static bool projectAssetName(const std::string& name) {
        if(name.size()!=18||name.compare(0,8,"assets/a")||name.compare(14,4,".wav"))return false;
        for(std::size_t i=8;i<14;++i)if(name[i]<'0'||name[i]>'9')return false;
        return true;
    }
    static bool projectWavValid(const std::string& path) {
        struct stat info{};if(::stat(path.c_str(),&info)!=0||!S_ISREG(info.st_mode)||info.st_size<44)return false;
        auto* file=std::fopen(path.c_str(),"rb");if(!file)return false;
        char tag[4]{};std::uint32_t riff=0;bool formatOk=false,dataOk=false;
        bool ok=std::fread(tag,1,4,file)==4&&!std::memcmp(tag,"RIFF",4)&&readU32(file,riff)
            &&std::fread(tag,1,4,file)==4&&!std::memcmp(tag,"WAVE",4)&&std::uint64_t(riff)+8<=std::uint64_t(info.st_size);
        while(ok&&std::ftell(file)+8<=info.st_size) {
            std::uint32_t size=0;if(std::fread(tag,1,4,file)!=4||!readU32(file,size))break;
            const auto at=std::ftell(file);const std::uint64_t next=std::uint64_t(at)+size+(size&1u);
            if(next>std::uint64_t(info.st_size)){ok=false;break;}
            if(!std::memcmp(tag,"fmt ",4)&&size>=16) {
                std::uint16_t kind=0,channels=0,align=0,bits=0;std::uint32_t rate=0,bytes=0;
                formatOk=readU16(file,kind)&&readU16(file,channels)&&readU32(file,rate)&&readU32(file,bytes)
                    &&readU16(file,align)&&readU16(file,bits)&&(channels==1||channels==2)&&rate>=8000&&rate<=192000
                    &&((kind==1&&(bits==16||bits==24||bits==32))||(kind==3&&bits==32))&&align==channels*(bits/8);
            }else if(!std::memcmp(tag,"data",4))dataOk=size>0;
            if(std::fseek(file,static_cast<long>(next),SEEK_SET)!=0){ok=false;break;}
        }
        std::fclose(file);return ok&&formatOk&&dataOk;
    }
    bool readProjectSnapshot(const std::string& directory,PersistenceSnapshot& snapshot) {
        const auto state=directory+"/thru-state.json",clips=directory+"/thru-clips.bin";
        std::uint64_t sg=0,cg=0;
        return stateFileGeneration(state.c_str(),sg)&&clipFileGeneration(clips.c_str(),cg)&&sg==cg
            &&readStateTextFromPath(state.c_str(),snapshot.json)&&readClipFileFromPath(clips.c_str(),sg,&snapshot);
    }
    // Early project imports could retain the portable archive metadata as a
    // numerically newer backup. Prefer the complete relocated primary only
    // when every backup source maps to exactly its installed capture copy.
    // Keep both files intact; normal persistence observes both generations
    // and publishes the repaired state above them after reopening.
    bool preferRelocatedProjectPrimary(const std::string& directory) {
        const auto state=directory+"/thru-state.json",clips=directory+"/thru-clips.bin";
        std::uint64_t primaryState=0,primaryClips=0,backupState=0,backupClips=0;
        if(!stateFileGeneration(state.c_str(),primaryState)
            ||!clipFileGeneration(clips.c_str(),primaryClips)||primaryState!=primaryClips
            ||!stateFileGeneration((state+".bak").c_str(),backupState)
            ||!clipFileGeneration((clips+".bak").c_str(),backupClips)
            ||backupState!=backupClips||backupState<=primaryState)return false;
        auto primary=std::unique_ptr<PersistenceSnapshot>(new(std::nothrow) PersistenceSnapshot);
        auto backup=std::unique_ptr<PersistenceSnapshot>(new(std::nothrow) PersistenceSnapshot);
        if(!primary||!backup||!readProjectSnapshot(directory,*primary)
            ||!readClipFileFromPath((clips+".bak").c_str(),backupClips,backup.get()))return false;
        std::vector<std::string> expected;
        std::set<std::string> checked;
        if(!projectPaths(*backup,[&](auto& path,LaneSourceKind*,int){
            if(!path[0]){expected.emplace_back();return true;}
            const std::string name=path.data();if(!projectAssetName(name))return false;
            const auto relocated=directory+"/captures/"+name.substr(7);
            if(checked.insert(relocated).second&&!projectWavValid(relocated))return false;
            expected.push_back(relocated);return true;
        })||checked.empty())return false;
        std::size_t index=0;
        return projectPaths(*primary,[&](auto& path,LaneSourceKind*,int){
            return index<expected.size()&&expected[index++]==path.data();
        })&&index==expected.size();
    }
    // A failed early import may already have been saved again. In that case
    // both generations can contain archive-relative names. Resolve only the
    // generated asset names inside a selected imported project's captures;
    // detached ZIP snapshots must keep their portable names until installation.
    bool rebaseImportedProjectSources(std::array<ClipSlot,kClipSlotCount>& clips,bool& recovered) const {
        recovered=false;if(!projectDataSelected)return true;
        const auto rebase=[&](auto& path,LaneSourceKind* kind){
            if(!projectAssetName(path.data()))return true;
            const std::string relocated=std::string(moduleDirectory.data())+"/captures/"+std::string(path.data()+7);
            if(relocated.size()>=path.size())return false;
            std::strcpy(path.data(),relocated.c_str());
            if(kind)*kind=LaneSourceKind::capture;
            recovered=true;return true;
        };
        for(auto& clip:clips) {
            for(std::size_t lane=0;lane<kLaneCount;++lane)
                if(!rebase(clip.laneSourcePaths[lane],&clip.laneSourceKinds[lane]))return false;
            for(auto& slot:clip.bankSlots)if(!rebase(slot.path,nullptr))return false;
        }
        return true;
    }
    static bool projectJsonNumber(std::array<char,kStateTextSize>& json,const char* key,double value,const char* literal=nullptr) {
        const std::string needle=std::string("\"")+key+"\":";std::string text=json.data();
        const auto at=text.find(needle);if(at==std::string::npos)return false;
        const auto start=at+needle.size();auto end=start;
        while(end<text.size()&&(text[end]==' '||text[end]=='-'||text[end]=='+'||text[end]=='.'||(text[end]>='0'&&text[end]<='9')||text[end]=='e'||text[end]=='E'))++end;
        char number[48]{};std::snprintf(number,sizeof(number),"%.17g",value);text.replace(start,end-start,literal?literal:number);
        if(text.size()>=json.size())return false;
        std::strcpy(json.data(),text.c_str());return true;
    }
    template<class F> static bool projectPaths(PersistenceSnapshot& snapshot,F visit) {
        for(auto& clip:snapshot.clips) {
            for(std::size_t lane=0;lane<kLaneCount;++lane)
                if(!visit(clip.laneSourcePaths[lane],&clip.laneSourceKinds[lane],static_cast<int>(lane)))return false;
            for(auto& slot:clip.bankSlots)if(!visit(slot.path,nullptr,-1))return false;
        }
        return true;
    }
    struct ProjectFileIo {ThruInstance* self;std::FILE* file;};
    static std::size_t projectZipRead(void* opaque,mz_uint64 at,void* data,std::size_t size) {
        auto& io=*static_cast<ProjectFileIo*>(opaque);if(io.self->projectCancel.load())return 0;
        if(::fseeko(io.file,static_cast<off_t>(at),SEEK_SET)!=0)return 0;
        const auto count=std::fread(data,1,size,io.file);io.self->projectBytes.fetch_add(count);return count;
    }
    static std::size_t projectZipWrite(void* opaque,mz_uint64 at,const void* data,std::size_t size) {
        auto& io=*static_cast<ProjectFileIo*>(opaque);if(io.self->projectCancel.load())return 0;
        if(::fseeko(io.file,static_cast<off_t>(at),SEEK_SET)!=0)return 0;
        const auto count=std::fwrite(data,1,size,io.file);io.self->projectBytes.fetch_add(count);return count;
    }
    bool addProjectFile(mz_zip_archive& zip,const std::string& name,const std::string& path) {
        struct stat info{};if(::stat(path.c_str(),&info)!=0||!S_ISREG(info.st_mode)||info.st_size<0)return false;
        auto* file=std::fopen(path.c_str(),"rb");if(!file)return false;ProjectFileIo io{this,file};
        const bool ok=mz_zip_writer_add_read_buf_callback(&zip,name.c_str(),projectZipRead,&io,info.st_size,nullptr,nullptr,0,0,nullptr,0,nullptr,0)!=0;
        std::fclose(file);return ok;
    }
    bool exportProjectFile(const char* prefix="Thru") {
        std::array<char,kPersistencePathSize> directory{},stageName{};
        if(!projectDirectory(directory.data(),directory.size())||!projectMakeDirectory(directory.data())){projectNote("PROJECT FOLDER FAILED");return false;}
        if(std::snprintf(stageName.data(),stageName.size(),"%s/.export-XXXXXX",directory.data())>=static_cast<int>(stageName.size())||!mkdtemp(stageName.data()))return false;
        const std::string stage=stageName.data();
        struct Cleanup {std::string path;~Cleanup(){ThruInstance::projectRemoveTree(path);}} cleanup{stage};
        auto snapshot=std::unique_ptr<PersistenceSnapshot>(new(std::nothrow) PersistenceSnapshot);
        if(!snapshot||!readProjectSnapshot(moduleDirectory.data(),*snapshot)){projectNote("PROJECT DATA ERROR");return false;}
        std::map<std::string,std::string> assets;
        if(!projectPaths(*snapshot,[&](auto& path,LaneSourceKind* kind,int lane){
            if(!path[0]&&kind&&*kind!=LaneSourceKind::empty&&lane>=0) {
                char leaf[32]{};std::snprintf(leaf,sizeof(leaf),"thru-lane-%d.wav",lane+1);
                std::array<char,kPersistencePathSize> fallback{};
                if(persistencePath(leaf,fallback.data(),fallback.size())&&::access(fallback.data(),F_OK)==0)path=fallback;
            }
            if(!path[0])return true;
            const std::string source=path.data();auto it=assets.find(source);
            if(it==assets.end()) {
                if(assets.size()>=2048||!projectWavValid(source))return false;
                char name[32]{};std::snprintf(name,sizeof(name),"assets/a%06zu.wav",assets.size());it=assets.emplace(source,name).first;
            }
            std::snprintf(path.data(),path.size(),"%s",it->second.c_str());if(kind)*kind=LaneSourceKind::capture;return true;
        })){projectNote("SAMPLE MISSING");return false;}
        std::snprintf(snapshot->directory.data(),snapshot->directory.size(),"%s",stage.c_str());
        if(!writeMetadataFiles(snapshot.get())){projectNote("PROJECT SAVE ERROR");return false;}
        std::uint64_t total=0;for(const auto& asset:assets){struct stat info{};if(::stat(asset.first.c_str(),&info)!=0)return false;total+=info.st_size;}
        for(const char* leaf:{"thru-clips.bin","thru-state.json"}) {
            struct stat info{};if(::stat((stage+"/"+leaf).c_str(),&info)!=0)return false;total+=info.st_size;
        }
        if(total>16ull*1024*1024*1024-4096){projectNote("PROJECT TOO LARGE");return false;}
        struct statvfs space{};
        if(::statvfs(directory.data(),&space)!=0||total+1024u*1024u>static_cast<std::uint64_t>(space.f_bavail)*space.f_frsize){projectNote("NOT ENOUGH SPACE");return false;}
        projectBytes.store(0);projectTotalBytes.store(std::max<std::uint64_t>(1,total));
        const std::string temporary=stage+"/archive.tmp";mz_zip_archive zip{};
        s3g_thru_project::ZipBudget zipBudget;s3g_thru_project::setupZip(zip,zipBudget);
        if(!mz_zip_writer_init_file_v2(&zip,temporary.c_str(),0,MZ_ZIP_FLAG_WRITE_ZIP64))return false;
        const std::string manifest="{\"format\":\"s3g-thru-project\",\"version\":1,\"clip_format\":40,\"assets\":"+std::to_string(assets.size())+"}";
        bool ok=mz_zip_writer_add_mem(&zip,"manifest.json",manifest.data(),manifest.size(),0)!=0
            &&addProjectFile(zip,"thru-state.json",stage+"/thru-state.json")&&addProjectFile(zip,"thru-clips.bin",stage+"/thru-clips.bin");
        for(const auto& asset:assets)if(ok)ok=addProjectFile(zip,asset.second,asset.first);
        ok=ok&&!projectCancel.load()&&mz_zip_writer_finalize_archive(&zip);const bool ended=mz_zip_writer_end(&zip)!=0;ok=ok&&ended;
        if(!ok){projectNote(projectCancel.load()?"PROJECT CANCELLED":"PROJECT WRITE FAILED");return false;}
        auto* archive=std::fopen(temporary.c_str(),"r+b");if(!s3g_thru_storage::closeFile(archive,temporary.c_str()))return false;
        timespec now{};clock_gettime(CLOCK_REALTIME,&now);std::tm utc{};gmtime_r(&now.tv_sec,&utc);char date[32]{};std::strftime(date,sizeof(date),"%Y%m%d-%H%M%S",&utc);
        if(std::snprintf(projectOutputPath.data(),projectOutputPath.size(),"%s/%s-%s-%06ld.thru",directory.data(),prefix,date,now.tv_nsec/1000)>=static_cast<int>(projectOutputPath.size()))return false;
        if(!s3g_thru_storage::link(temporary.c_str(),projectOutputPath.data())||!s3g_thru_storage::syncParent(projectOutputPath.data()))return false;
        projectNote("PROJECT EXPORTED");return true;
    }
    bool importProjectFile() {
        mz_zip_archive zip{};s3g_thru_project::ZipBudget zipBudget;s3g_thru_project::setupZip(zip,zipBudget);
        if(!mz_zip_reader_init_file(&zip,projectArchivePath.data(),0)){projectNote("INVALID PROJECT ZIP");return false;}
        struct ZipEnd {mz_zip_archive* zip;~ZipEnd(){mz_zip_reader_end(zip);}} zipEnd{&zip};
        const auto count=mz_zip_reader_get_num_files(&zip);if(count<3||count>2051){projectNote("INVALID PROJECT FILES");return false;}
        std::set<std::string> names;std::uint64_t total=0;int manifestIndex=-1;
        for(mz_uint i=0;i<count;++i){mz_zip_archive_file_stat st{};if(!mz_zip_reader_file_stat(&zip,i,&st))return false;
            const std::string name=st.m_filename;
            if(name.size()>=sizeof(st.m_filename)-1||!names.insert(name).second||st.m_is_directory||st.m_is_encrypted||!st.m_is_supported
                ||((st.m_external_attr>>16)&0170000)==0120000
                ||(name!="manifest.json"&&name!="thru-state.json"&&name!="thru-clips.bin"&&!projectAssetName(name)))return false;
            const std::uint64_t limit=name=="manifest.json"?4096:name=="thru-state.json"?kStateTextSize-1:name=="thru-clips.bin"?64u*1024u*1024u:8ull*1024*1024*1024;
            if(st.m_uncomp_size>limit||total>16ull*1024*1024*1024-st.m_uncomp_size)return false;
            total+=st.m_uncomp_size;if(name=="manifest.json")manifestIndex=static_cast<int>(i);
        }
        if(manifestIndex<0||!names.count("thru-state.json")||!names.count("thru-clips.bin"))return false;
        std::array<char,4097> manifest{};float version=0,format=0,assetCount=0;
        if(!mz_zip_reader_extract_to_mem(&zip,manifestIndex,manifest.data(),4096,0)
            ||!std::strstr(manifest.data(),"\"format\":\"s3g-thru-project\"")||!jsonNumber(manifest.data(),"version",version)||version!=1
            ||!jsonNumber(manifest.data(),"clip_format",format)||(format!=36 && format!=37 && format!=38 && format!=39 && format!=40)
            ||!jsonNumber(manifest.data(),"assets",assetCount)||assetCount!=count-3){projectNote("UNSUPPORTED PROJECT");return false;}
        std::array<char,kPersistencePathSize> dataRoot{},stageName{};
        if(!rootDataPath("project-data",dataRoot.data(),dataRoot.size())||!projectMakeDirectory(dataRoot.data()))return false;
        struct statvfs space{};
        if(::statvfs(dataRoot.data(),&space)!=0||total+32u*1024u*1024u>static_cast<std::uint64_t>(space.f_bavail)*space.f_frsize){projectNote("NOT ENOUGH SPACE");return false;}
        if(std::snprintf(stageName.data(),stageName.size(),"%s/.import-XXXXXX",dataRoot.data())>=static_cast<int>(stageName.size())||!mkdtemp(stageName.data()))return false;
        const std::string stage=stageName.data();struct Cleanup {std::string path;~Cleanup(){if(!path.empty())ThruInstance::projectRemoveTree(path);}} cleanup{stage};
        if(!projectMakeDirectory((stage+"/assets").c_str()))return false;
        projectBytes.store(0);projectTotalBytes.store(std::max<std::uint64_t>(1,total));
        for(mz_uint i=0;i<count;++i) {
            mz_zip_archive_file_stat st{};mz_zip_reader_file_stat(&zip,i,&st);const std::string name=st.m_filename,path=stage+"/"+name;
            auto* file=std::fopen(path.c_str(),"wb");if(!file)return false;ProjectFileIo io{this,file};
            const bool read=mz_zip_reader_extract_to_callback(&zip,i,projectZipWrite,&io,0)!=0;
            if(!s3g_thru_storage::closeFile(file,path.c_str(),read))return false;
            if(projectAssetName(name)&&!projectWavValid(path))return false;
        }
        auto snapshot=std::unique_ptr<PersistenceSnapshot>(new(std::nothrow) PersistenceSnapshot);
        if(!snapshot||!readProjectSnapshot(stage,*snapshot)){projectNote("PROJECT DATA INVALID");return false;}
        std::set<std::string> referenced;
        timespec now{};clock_gettime(CLOCK_REALTIME,&now);
        std::snprintf(projectActivation.data(),projectActivation.size(),"p-%llx-%lx",static_cast<unsigned long long>(now.tv_sec),static_cast<unsigned long>(now.tv_nsec));
        const std::string finalDirectory=std::string(dataRoot.data())+"/"+projectActivation.data();
        if(finalDirectory.size()+32>=kPersistencePathSize)return false;
        if(!projectPaths(*snapshot,[&](auto& path,LaneSourceKind* kind,int){
            if(!path[0])return true;
            const std::string name=path.data();if(!projectAssetName(name)||!names.count(name))return false;
            referenced.insert(name);const std::string destination=finalDirectory+"/captures/"+name.substr(7);
            if(destination.size()>=path.size())return false;
            std::strcpy(path.data(),destination.c_str());if(kind)*kind=LaneSourceKind::capture;return true;
        })||referenced.size()!=static_cast<std::size_t>(assetCount)){projectNote("SAMPLE LINK INVALID");return false;}
        // Hardware controller configuration belongs to this Move. Keep it when
        // opening a project from another user or controller setup.
        std::array<char,kStateTextSize> current{};std::array<char,kPersistencePathSize> currentState{};
        if(!persistencePath("thru-state.json",currentState.data(),currentState.size())||!readStateTextFromPath(currentState.data(),current))return false;
        for(const char* key:{"midi_control_mode","midi_cc_channel","midi_cc_base","midi_takeover","midi_control_part","midi_note_route","midi_note_channel","midi_bend_range","midi_mod_target","midi_velocity_depth","midi_sync_source","midi_sync_transport"}) {
            float value=0;if(jsonNumber(current.data(),key,value)&&!projectJsonNumber(snapshot->json,key,value))return false;
        }
        // The extracted primary becomes the first recovery backup when the
        // relocated metadata is published. Its generation must never outrank
        // the installed paths, including imports into a fresh destination.
        if(snapshot->generation==std::numeric_limits<std::uint64_t>::max()){
            projectNote("PROJECT SAVE LIMIT");return false;
        }
        snapshot->generation=std::max(projectGeneration,snapshot->generation+1u);
        if(!projectJsonNumber(snapshot->json,"generation",0,std::to_string(snapshot->generation).c_str()))return false;
        std::snprintf(snapshot->directory.data(),snapshot->directory.size(),"%s",stage.c_str());
        if(!writeMetadataFiles(snapshot.get())||!writeMetadataFiles(snapshot.get())||projectCancel.load())return false;
        // Make an independently usable backup before changing the active pointer.
        if(!exportProjectFile("BeforeImport"))return false;
        if(projectCancel.load())return false;
        if(!s3g_thru_storage::rename((stage+"/assets").c_str(),(stage+"/captures").c_str())
            ||!s3g_thru_storage::syncParent((stage+"/captures").c_str())
            ||!s3g_thru_storage::rename(stage.c_str(),finalDirectory.c_str())
            ||!s3g_thru_storage::syncParent(finalDirectory.c_str()))return false;
        cleanup.path.clear();
        std::array<char,kPersistencePathSize> pointer{},temp{};
        if(!rootDataPath("thru-active-project.txt",pointer.data(),pointer.size())||!rootDataPath("thru-active-project.txt.tmp",temp.data(),temp.size()))return false;
        auto* file=std::fopen(temp.data(),"wb");if(!file)return false;
        const std::string text=std::string("THRUPROJECT1 ")+projectActivation.data()+"\n";
        const bool wrote=writeBytes(file,text.data(),text.size());if(!s3g_thru_storage::closeFile(file,temp.data(),wrote))return false;
        const bool committed=replaceFileWithBackup(temp.data(),pointer.data());
        std::array<char,64> active{};
        projectImportInstalled=projectReadId(pointer.data(),active.data(),active.size())&&!std::strcmp(active.data(),projectActivation.data());
        projectNote(committed?"IMPORTED: REOPEN THRU":"IMPORT COMMIT FAILED");return committed;
    }
    bool scanProjectFiles() {
        std::array<char,kPersistencePathSize> root{};if(!projectDirectory(root.data(),root.size())||!projectMakeDirectory(root.data()))return false;
        auto* dir=opendir(root.data());if(!dir)return false;std::vector<std::string> names;
        while(auto* entry=readdir(dir)) {const std::string name=entry->d_name;
            if(name.size()>80||name.size()<6||name[0]=='.'||name.compare(name.size()-5,5,".thru"))continue;
            struct stat info{};const std::string path=std::string(root.data())+"/"+name;
            if(lstat(path.c_str(),&info)==0&&S_ISREG(info.st_mode)&&names.size()<4096)names.push_back(name);
        }
        closedir(dir);std::sort(names.rbegin(),names.rend());projectFileCount=static_cast<int>(names.size());
        projectPage=clampValue(projectPage,0,std::max(0,(projectFileCount-1)/32));projectListed=0;
        for(int i=projectPage*32;i<projectFileCount&&projectListed<32;++i)std::snprintf(projectFiles[projectListed++].data(),81,"%s",names[i].c_str());
        projectNote("PROJECTS READY");return true;
    }
    bool processProjectJob() {
        int requested=projectRequested;if(!projectJob.compare_exchange_strong(requested,projectWorking,std::memory_order_acq_rel))return false;
        projectSuccess=false;errno=0;projectNote("PROJECT FAILED");
        try {projectSuccess=projectAction==1?exportProjectFile():projectAction==2?importProjectFile():scanProjectFiles();}
        catch(...){projectNote("PROJECT MEMORY ERROR");}
        if(!projectSuccess&&errno==ENOSPC)projectNote("NOT ENOUGH SPACE");
        if(projectCancel.load()&&!projectImportInstalled)projectNote("PROJECT CANCELLED");
        projectJob.store(projectComplete,std::memory_order_release);return true;
    }
    void consumeProjectJob() {
        if(projectJob.load(std::memory_order_acquire)==projectComplete)projectJob.store(projectIdle,std::memory_order_release);
    }
    void serviceProjectJob() {
        consumeProjectJob();
        if(projectJob.load(std::memory_order_acquire)!=projectWaiting)return;
        if(saveFailed||bankSaveFailed||customSaveFailed){projectNote("RESOLVE SAVE ERROR");projectJob.store(projectIdle);return;}
        if(!persistentStateDirty&&pendingCaptureSaveMask==0&&!customDirty&&bankJob.load()==0&&persistenceJob.load()==backgroundIdle&&laneJob.load()==backgroundIdle&&customSaveJob.load()==0)
            projectJob.store(projectRequested,std::memory_order_release);
    }
    bool setProjectParam(const char* key,const char* value) {
        if(std::strncmp(key,"project_",8))return false;
        if(!std::strcmp(key,"project_cancel")){projectCancel.store(true);if(projectJob.load()==projectWaiting){projectJob.store(projectIdle);projectNote("PROJECT CANCELLED");}return true;}
        if(projectJob.load()!=projectIdle||projectImportInstalled)return true;
        if(!persistenceEnabled||!backgroundStarted){projectNote("STORAGE UNAVAILABLE");return true;}
        projectCancel.store(false);projectBytes.store(0);projectTotalBytes.store(0);projectSuccess=false;
        if(!std::strcmp(key,"project_scan")){projectPage=std::max(0,parseInteger(value,0));projectAction=3;projectJob.store(projectRequested);return true;}
        const bool importing=!std::strcmp(key,"project_import");
        if(std::strcmp(key,"project_export")&&!importing)return true;
        if(audioRecording()||clipRecording||bankJob.load()!=0||patternBusy()||clipPreparationInFlight()||partCapturePublishing||partCaptureRecording||saveFailed||customSaveFailed){projectNote("FINISH RECORD / SAVE");return true;}
        if(importing) {
            const int index=parseInteger(value,-1);if(index<0||index>=projectListed){projectNote("CHOOSE A PROJECT");return true;}
            std::array<char,kPersistencePathSize> root{};projectDirectory(root.data(),root.size());
            if(!projectJoinPath(root.data(),projectFiles[index].data(),projectArchivePath.data(),projectArchivePath.size()))return true;
        }
        setClipPlaying(false);allNotesOff();captureActiveClip(true);markPersistentStateDirty();maintenanceRequested=true;
        projectGeneration=reservePersistenceGeneration();projectAction=importing?2:1;projectNote("SAVING CURRENT SET");projectJob.store(projectWaiting,std::memory_order_release);return true;
    }
    int formatProjectState(char* out,int size) const {
        const int job=projectJob.load(std::memory_order_acquire);
        if(job==projectWorking||job==projectRequested)return std::snprintf(out,size,"{\"v\":1,\"job\":3,\"action\":%d,\"ok\":0,\"reopen\":0,\"page\":0,\"count\":0,\"bytes\":%llu,\"total\":%llu,\"message\":\"WORKING\"}",projectAction,static_cast<unsigned long long>(projectBytes.load()),static_cast<unsigned long long>(projectTotalBytes.load()));
        return std::snprintf(out,size,"{\"v\":1,\"job\":%d,\"action\":%d,\"ok\":%d,\"reopen\":%d,\"page\":%d,\"count\":%d,\"bytes\":%llu,\"total\":%llu,\"message\":\"%s\"}",job,projectAction,projectSuccess?1:0,projectImportInstalled?1:0,projectPage,projectFileCount,
            static_cast<unsigned long long>(projectBytes.load()),static_cast<unsigned long long>(projectTotalBytes.load()),job==projectWorking?"WORKING":projectMessage.data());
    }
    int formatProjectFiles(char* out,int size) const {
        if(projectJob.load(std::memory_order_acquire)!=projectIdle)return std::snprintf(out,size,"[]");
        std::string json="[";for(int i=0;i<projectListed;++i){if(i)json+=',';json+='"';for(const char* p=projectFiles[i].data();*p;++p){if(*p=='"'||*p=='\\')json+='\\';if(static_cast<unsigned char>(*p)>=32)json+=*p;}json+='"';}json+=']';
        return copyText(json.c_str(),out,size);
    }
