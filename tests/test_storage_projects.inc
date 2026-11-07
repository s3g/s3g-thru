void testProjectPathBounds() {
    std::array<char,64> path{};
    assert(ThruInstance::projectJoinPath("/tmp/thru","projects",path.data(),path.size()));
    assert(std::string(path.data())=="/tmp/thru/projects");
    const auto unchanged=path;
    assert(!ThruInstance::projectJoinPath("/tmp/thru","projects",path.data(),18)&&path==unchanged);
    assert(ThruInstance::projectJoinPath("/tmp/thru","projects",path.data(),19));
    std::strcpy(path.data(),"/tmp/thru");
    assert(ThruInstance::projectJoinPath(path.data(),"projects",path.data(),path.size()));
    assert(std::string(path.data())=="/tmp/thru/projects");
    std::strcpy(path.data(),"take.wav");
    assert(ThruInstance::projectJoinPath("/tmp",path.data(),path.data(),path.size()));
    assert(std::string(path.data())=="/tmp/take.wav");
    assert(!ThruInstance::projectJoinPath(nullptr,"x",path.data(),path.size()));
    std::puts("s3g Thru project paths: exact capacity, overflow rejection, unchanged failure output and aliased components: PASS");
}
using StorageOp=s3g_thru_storage::Operation;
struct StorageEvent {StorageOp op;bool renderThread;};
std::vector<StorageEvent> storageTrace;
int storageFailAt=0;
pthread_t storageRenderThread{};
bool storageHook(StorageOp op,const char*,const char*) {
    storageTrace.push_back({op,pthread_equal(pthread_self(),storageRenderThread)!=0});
    return !storageFailAt||static_cast<int>(storageTrace.size())!=storageFailAt;
}
void testBankExitRecoveryAndDiscard() {
    char folder[]="/tmp/thru-exit-retry.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();t->backgroundStarted=true;t->setPerformanceMode(4);
    const auto failTake=[&](int slot){
        assert(t->startBankCapture(slot));t->splitBus[0][0].fill(.12f);t->splitBus[0][1].fill(-.06f);
        for(int block=0;block<256;++block)t->recordBankBus(64);
        const auto intended=t->bankRecordPath;std::snprintf(t->bankRecordPath.data(),t->bankRecordPath.size(),"%s/missing/take.wav",folder);
        t->finishBankCapture();assert(t->processBankJob());t->consumeBankJob();assert(t->bankJob.load()==5);
        t->bankRecordPath=intended;return intended;
    };
    auto path=failTake(0);t->backgroundStarted=false;assert(t->savePersistentState());
    assert(t->bankJob.load()==0&&t->bankAssets[0]>=0&&access(path.data(),F_OK)==0);
    const auto old=t->bankSlots[0];const auto undo=t->bankUndoIndex;
    t->backgroundStarted=true;path=failTake(1);const auto token=t->bankTakeToken;
    assert(!t->retryBankTake(token-1)&&!t->discardBankTake(token-1));
    assert(t->retryBankTake(token)&&t->bankJob.load()==2);
    assert(t->discardBankTake(token)&&t->bankJob.load()==0&&!t->processBankJob());
    assert(t->bankSlots[0].path==old.path&&t->bankUndoIndex==undo&&!t->bankSlots[1].path[0]&&access(path.data(),F_OK)!=0);
    path=failTake(1);assert(!t->discardBankTake(token));
    t->bankJob.store(3);assert(!t->discardBankTake(t->bankTakeToken));t->bankJob.store(5);
    std::snprintf(t->bankRecordPath.data(),t->bankRecordPath.size(),"%s/still-missing/take.wav",folder);
    t->backgroundStarted=false;assert(!t->savePersistentState());assert(t->bankJob.load()==5&&t->bankRecordFrames==16384);
    assert(t->discardBankTake(t->bankTakeToken)&&t->savePersistentState());
    t.reset();std::filesystem::remove_all(folder);
    std::puts("s3g Thru Bank exit recovery: final retry, persistent failure, explicit retry/discard, stale token, worker race and Undo preservation: PASS");
}
void testDurableStorageCommits() {
    char folder[]="/tmp/thru-durable.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();
    t->controls.locus=.2f;assert(t->savePersistentState());assert(t->savePersistentState());
    const std::string baseline=std::string(folder)+"/baseline";std::filesystem::create_directory(baseline);
    const char* files[]{"thru-state.json","thru-clips.bin","thru-state.json.bak","thru-clips.bin.bak"};
    for(const auto* name:files)std::filesystem::copy_file(std::string(folder)+"/"+name,baseline+"/"+name);
    storageRenderThread=pthread_self();storageFailAt=0;storageTrace.clear();s3g_thru_storage::testHook=storageHook;
    assert(t->writeMetadataFiles());s3g_thru_storage::testHook=nullptr;
    const int operations=static_cast<int>(storageTrace.size());assert(operations>=10);
    assert(storageTrace[0].op==StorageOp::fileSync&&storageTrace[1].op==StorageOp::fileSync);
    for(int failed=1;failed<=operations;++failed) {
        const std::string trial=std::string(folder)+"/trial-"+std::to_string(failed);std::filesystem::create_directory(trial);
        for(const auto* name:files)std::filesystem::copy_file(baseline+"/"+name,trial+"/"+name);
        std::snprintf(t->moduleDirectory.data(),t->moduleDirectory.size(),"%s",trial.c_str());
        storageFailAt=failed;storageTrace.clear();s3g_thru_storage::testHook=storageHook;
        assert(!t->writeMetadataFiles());s3g_thru_storage::testHook=nullptr;
        bool paired=false;
        for(const auto* a:{"thru-state.json","thru-state.json.bak"})for(const auto* b:{"thru-clips.bin","thru-clips.bin.bak"}) {
            std::uint64_t sg=0,cg=0;if(t->stateFileGeneration((trial+"/"+a).c_str(),sg)&&t->clipFileGeneration((trial+"/"+b).c_str(),cg)&&sg==cg)paired=true;
        }
        assert(paired);assert(t->writeMetadataFiles());
        std::uint64_t sg=0,cg=0;
        assert(t->stateFileGeneration((trial+"/thru-state.json.bak").c_str(),sg));
        assert(t->clipFileGeneration((trial+"/thru-clips.bin.bak").c_str(),cg)&&sg==cg);
    }
    std::snprintf(t->moduleDirectory.data(),t->moduleDirectory.size(),"%s",folder);
    std::array<float,64> audio{};audio.fill(.1f);
    const std::string wav=std::string(folder)+"/durable.wav",tmp=wav+".tmp";
    for(int failed:{1,3}){
        storageTrace.clear();storageFailAt=failed;s3g_thru_storage::testHook=storageHook;
        bool collision=false;assert(!t->writeStereoWavFile(audio.data(),audio.data(),64,44100,wav.c_str(),tmp.c_str(),true,&collision));
        s3g_thru_storage::testHook=nullptr;assert(!collision&&access(wav.c_str(),F_OK)!=0);
    }
    assert(t->writeStereoWavFile(audio.data(),audio.data(),64,44100,wav.c_str(),tmp.c_str(),true));
    // A normal live save issues its flushes on the background worker.
    storageTrace.clear();storageFailAt=0;s3g_thru_storage::testHook=storageHook;
    resumeTestBackgroundWorker(*t);t->selectCustomProfile(1);
    for(int i=0;i<4000&&t->customDirty;++i){renderSplit(*t,1);usleep(200);}
    t->stopBackgroundWork();s3g_thru_storage::testHook=nullptr;
    assert(!t->customDirty&&!storageTrace.empty());for(const auto& event:storageTrace)assert(!event.renderThread);
    t.reset();std::filesystem::remove_all(folder);
    std::puts("s3g Thru durable storage: flush-before-publish, every metadata rename/flush fault, coherent backup repair, archive failure and background-only live flushes: PASS");
}
void testPortableProjects(bool bank=true,int legacyImport=0) {
    char source[]="/tmp/thru-project-source.XXXXXX",target[]="/tmp/thru-project-target.XXXXXX";assert(mkdtemp(source)&&mkdtemp(target));
    const std::string library=std::string(source)+"/library";std::filesystem::create_directory(library);
    const std::string wav=library+"/a stereo sound.wav";
    std::vector<float> left(16384,.125f),right(16384,-.0625f);
    assert(ThruInstance::writeStereoWavFile(left.data(),right.data(),left.size(),44100,wav.c_str(),(wav+".tmp").c_str(),true));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(source));t->stopBackgroundWork();if(bank)t->setPerformanceMode(4);
    t->controls.algorithm=Engine::PlaybackAlgorithm::stochastic;t->controls.stochasticAmplitude=.73f;
    std::copy(left.begin(),left.end(),t->engine.writableLane(0,0));std::copy(right.begin(),right.end(),t->engine.writableLane(0,1));assert(t->engine.adoptLane(0,left.size(),44100));
    t->currentLaneSourceKinds[0]=LaneSourceKind::file;std::snprintf(t->currentLaneSourcePaths[0].data(),kPersistencePathSize,"%s",wav.c_str());
    std::snprintf(t->bankSlots[0].path.data(),kPersistencePathSize,"%s",wav.c_str());t->bankSlots[1]=t->bankSlots[0];t->bankSlots[1].pan=.4f;
    t->copyCurrentSourcesToClip(*t->activeClipSlot());t->captureActiveClip(true);t->initializeClipIfNeeded(1);
    auto& slices=t->clipSlots[1].sliceLanes[0];slices.preRollMs=7;slices.edited.count=2;slices.edited.points[0]=0;slices.edited.points[1]=.4f;slices.edited.points[2]=1;
    t->clipSlots[1].clipDurationMs=2000;t->clipSlots[1].clipNoteCount=1;t->clipSlots[1].clipNotes[0].note=67;t->clipSlots[1].clipNotes[0].durationMs=1200;
    t->observePersistenceGeneration(100000u);
    assert(t->savePersistentState());const bool exportedOk=t->exportProjectFile();if(!exportedOk)std::fprintf(stderr,"Export error: %s errno=%d cancel=%d\n",t->projectMessage.data(),errno,t->projectCancel.load());assert(exportedOk);const std::string exported=t->projectOutputPath.data();
    if(const char* fixture=std::getenv("S3G_THRU_PROJECT_FIXTURE"))std::filesystem::copy_file(exported,fixture,std::filesystem::copy_options::overwrite_existing);
    mz_zip_archive zip{};assert(mz_zip_reader_init_file(&zip,exported.c_str(),0));assert(mz_zip_reader_get_num_files(&zip)==4);mz_zip_reader_end(&zip);
    const std::string targetProjects=std::string(target)+"/projects";std::filesystem::create_directory(targetProjects);
    const std::string imported=targetProjects+"/portable.thru";std::filesystem::copy_file(exported,imported);
    t.reset();std::filesystem::remove_all(source); // Original sample is gone.
    t=std::make_unique<ThruInstance>();assert(t->prepare(target));t->stopBackgroundWork();t->selectCustomProfile(2);customBinding(*t,3,75,0,"field");t->midiControlMode=5;
    assert(t->savePersistentState());const auto oldState=std::filesystem::file_size(std::string(target)+"/thru-state.json");
    t->projectGeneration=t->reservePersistenceGeneration();std::snprintf(t->projectArchivePath.data(),kPersistencePathSize,"%s",imported.c_str());
    assert(t->importProjectFile());assert(t->projectImportInstalled&&t->projectSuccess==false);
    const std::string activeDirectory=std::string(target)+"/project-data/"+t->projectActivation.data();
    assert(std::filesystem::exists(activeDirectory+"/captures/a000000.wav"));
    assert(std::filesystem::file_size(std::string(target)+"/thru-state.json")==oldState);
    assert(t->savePersistentState());
    if(legacyImport) {
        // Recreate an import made before the generation-order fix: installed
        // primaries use generation 1; portable backups retain generation 100001.
        auto installed=std::make_unique<ThruInstance::PersistenceSnapshot>();
        assert(t->readProjectSnapshot(activeDirectory,*installed));
        installed->generation=legacyImport==1?1u:100002u;
        const auto generation=std::to_string(installed->generation);
        assert(t->projectJsonNumber(installed->json,"generation",0,generation.c_str()));
        if(legacyImport==2)assert(t->projectPaths(*installed,[&](auto& path,LaneSourceKind*,int){
            if(path[0]){
                const std::string basename=std::strrchr(path.data(),'/')+1;
                std::snprintf(path.data(),path.size(),"assets/%s",basename.c_str());
            }
            return true;
        }));
        std::snprintf(installed->directory.data(),kPersistencePathSize,"%s",activeDirectory.c_str());
        assert(t->writeClipFile(installed.get())&&t->writeStateFile(installed.get()));
        mz_zip_archive original{};assert(mz_zip_reader_init_file(&original,imported.c_str(),0));
        for(const char* name:{"thru-state.json","thru-clips.bin"})
            assert(mz_zip_reader_extract_file_to_file(&original,name,(activeDirectory+"/"+name+".bak").c_str(),0));
        mz_zip_reader_end(&original);
    }
    t.reset();
    t=std::make_unique<ThruInstance>();assert(t->prepare(target));
    assert(t->projectPreferRelocatedPrimary==(legacyImport==1));
    assert(t->projectRecoveredRelativePaths==(legacyImport==2));
    assert(t->moduleDirectory.data()==activeDirectory&&t->moduleRootDirectory.data()==std::string(target));
    assert(t->bankMode==bank&&t->customMaps.active==2&&t->midiControlMode==5);
    assert(t->customMaps.profiles[1].bindings[0].cc==75);
    assert(t->clipSlots[1].sliceLanes[0].edited.count==2&&t->clipSlots[1].sliceLanes[0].preRollMs==7);
    assert(t->clipSlots[1].clipNoteCount==1&&t->clipSlots[1].clipNotes[0].note==67);
    assert(t->bankSlots[1].pan==.4f&&std::string(t->bankSlots[0].path.data()).find(activeDirectory)==0);
    for(int i=0;i<10000&&(!t->engine.laneLoaded(0)||(bank&&t->bankAssets[0]<0));++i){renderSplit(*t,1);usleep(200);}
    if(!t->engine.laneLoaded(0)||(bank&&t->bankAssets[0]<0))std::fprintf(stderr,"Import load: lane=%d asset=%d clipjob=%d samplejob=%d error=%s last=%s path=%s kind=%d queued=%d active=%d\n",t->engine.laneLoaded(0),t->bankAssets[0],t->clipPrepareState.load(),t->sampleLoadState.load(),t->sampleLoadError.data(),t->lastError.data(),t->clipSlots[0].laneSourcePaths[0].data(),static_cast<int>(t->clipSlots[0].laneSourceKinds[0]),t->queuedClip,t->activeClip);
    assert(t->engine.laneLoaded(0)&&(!bank||t->bankAssets[0]>=0));
    assert(std::abs(t->engine.laneData(0,1)[100]+.0625f)<1e-6f);
    t->stopSampleWorker();t->stopBackgroundWork();assert(t->savePersistentState());
    std::uint64_t restoredGeneration=0;
    assert(t->clipFileGeneration((activeDirectory+"/thru-clips.bin").c_str(),restoredGeneration));
    assert(restoredGeneration>100000u);
    // A second reopen must also use relocated sources after recovery is saved.
    if(legacyImport) {
        t.reset();t=std::make_unique<ThruInstance>();assert(t->prepare(target));
        for(int i=0;i<10000&&!t->engine.laneLoaded(0);++i){renderSplit(*t,1);usleep(200);}
        assert(!t->projectPreferRelocatedPrimary&&t->engine.laneLoaded(0)&&t->midiControlMode==5);
        t->stopSampleWorker();t->stopBackgroundWork();assert(t->savePersistentState());
    }
    assert(t->exportProjectFile());
    // Invalid archives never change the active pointer or existing project.
    const auto pointer=std::string(target)+"/thru-active-project.txt";
    const auto readBytes=[](const std::string& path){std::ifstream file(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(file),{});};
    const std::string before=readBytes(pointer);
    for(const std::string& bad:{"../escape.wav","/absolute.wav","assets/a000000.wav/evil","assets/a000001.wav"}) {
        const std::string attack=targetProjects+"/invalid.thru";mz_zip_archive z{};assert(mz_zip_writer_init_file(&z,attack.c_str(),0));
        const std::string manifest="{\"format\":\"s3g-thru-project\",\"version\":1,\"clip_format\":36,\"assets\":1}";
        assert(mz_zip_writer_add_mem(&z,"manifest.json",manifest.data(),manifest.size(),0));
        assert(mz_zip_writer_add_mem(&z,"thru-state.json","bad",3,0)&&mz_zip_writer_add_mem(&z,"thru-clips.bin","bad",3,0));
        const std::string placeholder(bad.size(),'x');assert(mz_zip_writer_add_mem(&z,placeholder.c_str(),"not a wave",10,0));assert(mz_zip_writer_finalize_archive(&z)&&mz_zip_writer_end(&z));
        auto encoded=readBytes(attack);std::size_t at=0;while((at=encoded.find(placeholder,at))!=std::string::npos){encoded.replace(at,placeholder.size(),bad);at+=bad.size();}
        {std::ofstream file(attack,std::ios::binary|std::ios::trunc);file.write(encoded.data(),encoded.size());}
        std::snprintf(t->projectArchivePath.data(),kPersistencePathSize,"%s",attack.c_str());t->projectImportInstalled=false;
        assert(!t->importProjectFile());assert(readBytes(pointer)==before&&t->moduleDirectory.data()==activeDirectory);
    }
    // A damaged payload with an otherwise correct manifest and metadata fails
    // CRC validation before activation. Preserve the existing project pointer.
    const std::string corrupt=targetProjects+"/corrupt.thru";
    std::filesystem::copy_file(imported,corrupt);
    mz_zip_archive check{};assert(mz_zip_reader_init_file(&check,corrupt.c_str(),0));
    const int assetIndex=mz_zip_reader_locate_file(&check,"assets/a000000.wav",nullptr,0);assert(assetIndex>=0);
    mz_zip_archive_file_stat stat{};assert(mz_zip_reader_file_stat(&check,assetIndex,&stat));mz_zip_reader_end(&check);
    auto* damaged=std::fopen(corrupt.c_str(),"r+b");assert(damaged);assert(std::fseek(damaged,static_cast<long>(stat.m_local_header_ofs)+26,SEEK_SET)==0);
    std::uint16_t nameSize=0,extraSize=0;assert(readU16(damaged,nameSize)&&readU16(damaged,extraSize));
    const auto payload=stat.m_local_header_ofs+30+nameSize+extraSize+60;assert(std::fseek(damaged,static_cast<long>(payload),SEEK_SET)==0);
    const int byte=std::fgetc(damaged);assert(byte!=EOF&&std::fseek(damaged,static_cast<long>(payload),SEEK_SET)==0);assert(std::fputc(byte^0x40,damaged)!=EOF);std::fclose(damaged);
    std::snprintf(t->projectArchivePath.data(),kPersistencePathSize,"%s",corrupt.c_str());
    assert(!t->importProjectFile()&&readBytes(pointer)==before);
    s3g_thru_project::ZipBudget budget;
    auto* bounded=s3g_thru_project::zipAllocate(&budget,1,budget.limit);assert(bounded&&budget.used==budget.limit);
    assert(!s3g_thru_project::zipAllocate(&budget,1,1));
    assert(!s3g_thru_project::zipReallocate(&budget,bounded,2,budget.limit));
    s3g_thru_project::zipFree(&budget,bounded);assert(budget.used==0);
    t->projectCancel.store(true);assert(!t->exportProjectFile());assert(readBytes(pointer)==before);
    t.reset();std::filesystem::remove_all(target);
    std::printf("s3g Thru portable projects (%s%s): ZIP64 dedup, all clip/Bank/slice data, cross-directory audio without original files, pre-import backup, isolated activation, controller preservation, invalid paths/WAVs and cancellation: PASS\n",bank?"Bank":"Single",legacyImport==2?", recovered resaved import":legacyImport==1?", recovered early import":"");
}

void testProjectBackgroundWorkflow() {
    char folder[]="/tmp/thru-project-worker.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
    storageTrace.clear();storageFailAt=0;storageRenderThread=pthread_self();s3g_thru_storage::testHook=storageHook;
    setParam(t.get(),"project_export","1");assert(t->projectJob.load()==ThruInstance::projectWaiting);
    const auto field=t->controls.field;const auto mode=t->voiceMode;
    setParam(t.get(),"field",".3");setParam(t.get(),"voice_mode","2");
    assert(t->controls.field==field&&t->voiceMode==mode);
    for(int i=0;i<20000&&t->projectJob.load()!=ThruInstance::projectIdle;++i){renderSplit(*t,1);usleep(100);}
    assert(t->projectJob.load()==ThruInstance::projectIdle&&t->projectSuccess&&access(t->projectOutputPath.data(),F_OK)==0);
    t->stopBackgroundWork();s3g_thru_storage::testHook=nullptr;
    assert(!storageTrace.empty());for(const auto& e:storageTrace)assert(!e.renderThread);
    t->backgroundStarted=true;setParam(t.get(),"project_scan","0");assert(t->processProjectJob());t->consumeProjectJob();assert(t->projectListed==1);
    t->backgroundStarted=false;resumeTestBackgroundWorker(*t);setParam(t.get(),"project_import","0");
    for(int i=0;i<20000&&t->projectJob.load()!=ThruInstance::projectIdle;++i){renderSplit(*t,1);usleep(100);}
    if(!t->projectSuccess)std::fprintf(stderr,"Worker import: job=%d message=%s dirty=%d save=%d\n",t->projectJob.load(),t->projectMessage.data(),t->persistentStateDirty,t->saveStatus());
    assert(t->projectImportInstalled&&t->projectSuccess&&t->projectJob.load()==ThruInstance::projectIdle);
    setParam(t.get(),"field",".3");assert(t->controls.field==field);assert(t->savePersistentState());
    t.reset();t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
    assert(t->moduleDirectory!=t->moduleRootDirectory);assert(t->savePersistentState());
    t.reset();std::filesystem::remove_all(folder);
    std::puts("s3g Thru project worker: save-before-export, bounded callback handoff, control isolation, background-only flushes, scan/import/backup and reopen: PASS");
}

template<class F> void waitProjectSources(ThruInstance& t,F ready) {
    for(int i=0;i<50000&&!ready();++i){renderSplit(t,1);usleep(100);}
    if(!ready())std::fprintf(stderr,"Wait failed: project=%s sample=%s last=%s job=%d dirty=%d mask=%u\n",t.projectMessage.data(),t.sampleLoadError.data(),t.lastError.data(),t.projectJob.load(),t.persistentStateDirty,t.pendingClipSourceMask);
    assert(ready());
}
void testProjectLoadedAndRecordedSources() {
    for(int mode=0;mode<5;++mode) {
        char folder[]="/tmp/thru-project-sources.XXXXXX";assert(mkdtemp(folder));
        const std::string library=std::string(folder)+"/library";std::filesystem::create_directory(library);
        auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
        t->setPerformanceMode(mode);
        for(int lane=0;lane<4;++lane) {
            std::vector<float> left(16384),right(16384);
            for(std::size_t i=0;i<left.size();++i){left[i]=.12f*std::sin(i*(lane+1)*.03f);right[i]=.1f*std::cos(i*(lane+1)*.03f);}
            const std::string path=library+"/sample"+std::to_string(lane)+".wav";
            assert(t->writeStereoWavFile(left.data(),right.data(),left.size(),44100,path.c_str(),(path+".tmp").c_str(),true));
            t->requestLibrarySample(lane,path.c_str());
            waitProjectSources(*t,[&]{return t->engine.laneLoaded(lane)&&t->sampleLoadState.load()==static_cast<int>(SampleLoadState::complete);});
            if(mode==4){std::snprintf(t->bankSlots[lane].path.data(),kPersistencePathSize,"%s",path.c_str());}
        }
        // Replace lane four through the input recorder, including the normal
        // completion/archive handoff, instead of manually installing a source.
        {
            std::array<std::int16_t,128*2> adc{};adc.fill(4096);
            host_api_v1_t host{};host.sample_rate=44100;host.frames_per_block=128;
            host.mapped_memory=reinterpret_cast<std::uint8_t*>(adc.data());host.audio_in_offset=0;
            const auto* prior=g_host;g_host=&host;
            t->selectedLane=3;t->recordSource=0;t->startRecording();assert(t->audioRecording());
            renderSplit(*t,256);t->finishRecording();g_host=prior;
            waitProjectSources(*t,[&]{return t->pendingCaptureSaveMask==0;});
            assert(t->currentLaneSourceKinds[3]==LaneSourceKind::capture);
        }
        t->copyActiveClip(7);t->selectClip(7);
        waitProjectSources(*t,[&]{return t->activeClip==7&& !t->clipPreparationInFlight();});
        setParam(t.get(),"project_export","1");
        waitProjectSources(*t,[&]{return t->projectJob.load()==ThruInstance::projectIdle;});
        assert(t->projectSuccess);
        t->stopSampleWorker();assert(t->savePersistentState());t.reset();
        // Import into the same existing module, as on Move.
        t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
        waitProjectSources(*t,[&]{return t->engine.loadedLaneMask()==15&&!t->clipPreparationInFlight()&&t->queuedClip<0;});
        setParam(t.get(),"project_scan","0");waitProjectSources(*t,[&]{return t->projectJob.load()==ThruInstance::projectIdle;});
        assert(t->projectListed==1);setParam(t.get(),"project_import","0");
        waitProjectSources(*t,[&]{return t->projectJob.load()==ThruInstance::projectIdle;});
        if(!t->projectImportInstalled||!t->projectSuccess)std::fprintf(stderr,"Import not active: %s pending=%d\n",t->projectMessage.data(),t->queuedClip);
        assert(t->projectImportInstalled&&t->projectSuccess);
        t->stopSampleWorker();assert(t->savePersistentState());t.reset();
        std::filesystem::remove_all(library);
        std::filesystem::remove_all(std::string(folder)+"/captures");
        t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
        waitProjectSources(*t,[&]{return t->engine.loadedLaneMask()==15;});
        assert(t->activeClip==7);
        for(int lane=0;lane<3;++lane)assert(std::abs(t->engine.laneData(lane,1)[100]-.1f*std::cos(100*(lane+1)*.03f))<1e-6f);
        assert(std::abs(t->engine.laneData(3,1)[100]-.125f)<1e-6f);

        t->stopSampleWorker();assert(t->savePersistentState());t.reset();
        t=std::make_unique<ThruInstance>();assert(t->prepare(folder));
        waitProjectSources(*t,[&]{return t->engine.loadedLaneMask()==15;});
        t->stopSampleWorker();assert(t->savePersistentState());t.reset();
        std::filesystem::remove_all(folder);
    }

    std::puts("s3g Thru project sources: actual library loader, stereo input recording, active clip 8, asynchronous same-device import, deleted originals, all five modes and repeated reopen: PASS");
}
