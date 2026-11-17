// Non-destructive sequence drafting. The preview owns one copied lane and
// overlays playback while open; only Apply writes authored/persistent data.
    std::array<s3g_thru_generate::Config,4> sequenceGenerateConfigs{};
    std::array<bool,4> sequenceGenerateConfigReady{};
    std::array<int,4> sequenceGenerateUnits{-1,-1,-1,-1};
    std::array<std::array<float,2>,4> sequenceGenerateWindows{};
    std::array<std::uint64_t,4> sequenceGenerateConfigSources{};
    s3g_thru_sequence::Lane sequenceGenerateBase;
    s3g_thru_generate::Result sequenceGenerateDraft;
    s3g_thru_generate::Context sequenceGenerateSource;
    bool sequenceGenerateActive=false,sequenceGenerateValid=false,undoIsSequenceGenerate=false;
    int sequenceGenerateClip=-1,sequenceGeneratePart=-1,sequenceGenerateSelected=0,sequenceGenerateUndoPart=0;
    std::uint64_t sequenceGenerateSourceRevision=0;
    std::uint32_t sequenceGenerateIgnoredPads=0;
    std::array<char,64> sequenceGenerateStatus{};
    const char* sequenceGenerateMapName="KEEP";
    std::array<char,4096> sequenceGenerateCache{};
    float sequenceGenerateLocus=0,sequenceGenerateField=1;
    int sequenceGenerateRoot=60,sequenceGenerateScale=0;

    void releaseSequenceGenerateVoices(bool keep=false) {
        for(auto& voice:voices)if(voice.sequence.active&&voice.sequence.generatedPreview){
            voice.sequence.generatedPreview=false;
            if(keep)continue;
            voice.held=false;
            const auto remaining=voice.sequence.frames>voice.sequence.elapsed?voice.sequence.frames-voice.sequence.elapsed:1;
            const auto fade=std::min(remaining,std::max<std::uint64_t>(1,std::llround(sampleRate*.005f)));
            voice.sequence.releaseAt=voice.sequence.elapsed;
            voice.sequence.frames=voice.sequence.elapsed+fade;
            voice.sequence.release=float(fade)*1000.f/sampleRate;
            // A note can already be in a long release. Restart that ramp at
            // its current amplitude so the shortened preview tail reaches zero.
            if(voice.envelopeStage==AmpEnvelopeStage::release){
                voice.envelopeStageStart=voice.envelope;
                voice.envelopeStageFrames=0;voice.envelopeDurationSet=false;
            }
            beginVoiceRelease(voice);
        }
    }
    void cancelSequenceGenerate(bool keepVoices=false) {
        if(sequenceGenerateActive){
            if(sequenceGenerateClip==activeClip&&sequenceGeneratePart>=0&&sequenceGeneratePart<4)
                sequencePlayback.tracks[sequenceGeneratePart].trigs=sequenceLive.tracks[sequenceGeneratePart].trigs;
            releaseSequenceGenerateVoices(keepVoices);
        }
        sequenceGenerateActive=sequenceGenerateValid=false;sequenceGenerateIgnoredPads=0;
    }
    bool sequenceGenerateOwner() const {
        return sequenceGenerateActive&&sequenceMode()&&!clipRecording&&!audioRecording()&&sequenceGenerateClip==activeClip
            &&sequenceGeneratePart==splitFocus;
    }
    bool sequenceGenerateContextCurrent(bool focus=true) const {
        if(!sequenceGenerateActive||!sequenceMode()||clipRecording||audioRecording()||sequenceGenerateClip!=activeClip
            ||sequenceGeneratePart<0||sequenceGeneratePart>=4||(focus&&sequenceGeneratePart!=splitFocus))return false;
        const auto& program=liveControlsForPart(sequenceGeneratePart);
        const auto settings=settingsForPart(sequenceGeneratePart);
        return sequenceGenerateSourceRevision==wavesetSourceRevisions[sequenceGeneratePart]
            &&program.locus==sequenceGenerateLocus&&program.field==sequenceGenerateField
            &&settings.root==sequenceGenerateRoot&&settings.scale==sequenceGenerateScale;
    }
    bool sequenceGenerateSourceCurrent() const {
        return sequenceGenerateContextCurrent()
            &&s3g_thru_generate::sameLane(sequenceLive.tracks[sequenceGeneratePart],sequenceGenerateBase);
    }
    void refreshSequenceGeneratePlayback() {
        if(!sequenceGenerateActive)return;
        if(!sequenceGenerateContextCurrent(false)
            ||!s3g_thru_generate::sameLane(sequenceLive.tracks[sequenceGeneratePart],sequenceGenerateBase)){
            cancelSequenceGenerate();return;
        }
        sequencePlayback.tracks[sequenceGeneratePart].trigs=sequenceGenerateValid
            ?sequenceGenerateDraft.lane.trigs:sequenceLive.tracks[sequenceGeneratePart].trigs;
        if(!sequenceGenerateValid)releaseSequenceGenerateVoices();
    }
    void rebuildSequenceGenerate() {
        using namespace s3g_thru_generate;
        struct CacheOnExit {ThruInstance& owner;~CacheOnExit(){owner.refreshSequenceGeneratePlayback();owner.formatSequenceGenerateState(owner.sequenceGenerateCache.data(),owner.sequenceGenerateCache.size());}} cache{*this};
        sequenceGenerateValid=false;
        if(!sequenceGenerateSourceCurrent()) {
            std::snprintf(sequenceGenerateStatus.data(),sequenceGenerateStatus.size(),"LANE CHANGED: REOPEN");return;
        }
        sequenceGenerateDraft={};sequenceGenerateDraft.lane=sequenceGenerateBase;
        for(int i=0;i<sequenceGenerateBase.length;++i)if(sequenceGenerateBase.trigs[i].enabled){++sequenceGenerateDraft.enabled;sequenceGenerateDraft.events+=sequenceGenerateBase.trigs[i].count;}
        auto& config=sequenceGenerateConfigs[sequenceGeneratePart];
        auto& v=config.v;
        sequenceGenerateSource={};
        auto& context=sequenceGenerateSource;
        const auto settings=settingsForPart(sequenceGeneratePart);
        context.root=settings.root;context.scale=0;
        const auto& scale=kScaleDefinitions[std::size_t(settings.scale)];
        for(std::size_t i=0;i<scale.count;++i)context.scale|=std::uint16_t(1u<<scale.intervals[i]);
        const auto* clip=activeClipSlot();
        context.stepMs=float(s3g_thru_sequence::rates[sequenceGenerateBase.rate]*60000.0
            /(std::max(1.f,tempoBpm)*(clip?clampValue(clip->clipRate,.25f,4.f):1.f)));
        sequenceGenerateMapName="KEEP";
        if(v[map]>0) {
            const auto lane=std::size_t(sequenceGeneratePart);
            if(!engine.laneLoaded(lane)){std::snprintf(sequenceGenerateStatus.data(),sequenceGenerateStatus.size(),"LOAD LANE SAMPLE");return;}
            if(v[high]-v[low]<.000001f){std::snprintf(sequenceGenerateStatus.data(),sequenceGenerateStatus.size(),"SET SOURCE RANGE");return;}
            auto program=liveControlsForPart(sequenceGeneratePart);
            program.sampleRegionMode=static_cast<Engine::RegionMode>(v[map]==1?4:v[map]==2?1:v[map]==3?2:v[map]==4?3:0);
            program.sampleRegionCount=v[regions];
            int kind=0;const auto source=sliceMapForLane(lane,program,kind);
            const int requested=int(v[map]);
            if((requested==2&&kind!=1)||(requested==3&&kind!=2)||(requested==4&&kind!=3)) {
                std::snprintf(sequenceGenerateStatus.data(),sequenceGenerateStatus.size(),requested==2?"TRANSIENTS NOT READY":requested==3?"NO FILE MARKERS":"NO EDITED MAP");return;
            }
            sequenceGenerateMapName=kind==1?"TRANS":kind==2?"FILE":kind==3?"EDIT":"EQUAL";
            for(std::uint32_t i=0;i<source.count;++i){
                const float p=kind==0?v[low]+source.points[i]*(v[high]-v[low]):source.points[i];
                if(p>=v[low]&&p<v[high]&&context.count<64)context.points[context.count++]=p;
            }
            if(!context.count){std::snprintf(sequenceGenerateStatus.data(),sequenceGenerateStatus.size(),"NO STARTS IN RANGE");return;}
            const float pre=v[preroll]<0?slicePreRoll(lane,kind):v[preroll];
            context.preRollNormalized=pre*engine.laneSampleRate(lane)/(1000.f*std::max<std::size_t>(1,engine.laneFrames(lane)));
        }
        sequenceGenerateDraft=build(sequenceGenerateBase,config,context);
        sequenceGenerateValid=true;
        std::snprintf(sequenceGenerateStatus.data(),sequenceGenerateStatus.size(),"%d/%d CLICK:APPLY",sequenceGenerateDraft.enabled,sequenceGenerateBase.length);
    }
    void beginSequenceGenerate() {
        if(!sequenceMode()){setError("Sequence mode required");return;}
        if(audioRecording()||clipRecording){setError("Stop recording before generating");return;}
        if(patternBusy()||clipPreparationInFlight()){setError("Finish clip transition first");return;}
        clearError();cancelSequenceGenerate();stopSequenceAudition();sequenceEditStep=-1;
        sequencePadConsumed.fill(true);
        sequenceGenerateClip=activeClip;sequenceGeneratePart=splitFocus;
        sequenceGenerateBase=sequenceLive.tracks[splitFocus];
        sequenceGenerateSourceRevision=wavesetSourceRevisions[splitFocus];
        sequenceGenerateLocus=liveControlsForPart(splitFocus).locus;
        sequenceGenerateField=liveControlsForPart(splitFocus).field;
        sequenceGenerateRoot=settingsForPart(splitFocus).root;
        sequenceGenerateScale=settingsForPart(splitFocus).scale;
        sequenceGenerateSelected=0;
        auto& c=sequenceGenerateConfigs[splitFocus];
        c.set(s3g_thru_generate::hits,c.v[s3g_thru_generate::hits],sequenceGenerateBase.length,sequenceGenerateBase.timeMode);
        const auto window=sliceSourceWindow(liveControlsForPart(splitFocus));
        const auto part=std::size_t(splitFocus);
        if(!sequenceGenerateConfigReady[part]||sequenceGenerateWindows[part]!=window||sequenceGenerateConfigSources[part]!=sequenceGenerateSourceRevision){
            c.v[s3g_thru_generate::low]=window[0];c.v[s3g_thru_generate::high]=window[1];
        }
        if(sequenceGenerateUnits[part]!=int(sequenceGenerateBase.timeMode)){
            c.v[s3g_thru_generate::minimum]=sequenceGenerateBase.timeMode?50.f:.25f;
            c.v[s3g_thru_generate::maximum]=sequenceGenerateBase.timeMode?1000.f:4.f;
        }
        sequenceGenerateConfigReady[part]=true;sequenceGenerateWindows[part]=window;
        sequenceGenerateConfigSources[part]=sequenceGenerateSourceRevision;
        sequenceGenerateUnits[part]=int(sequenceGenerateBase.timeMode);
        sequenceGenerateActive=true;rebuildSequenceGenerate();
    }
    void applySequenceGenerate() {
        if(!sequenceGenerateSourceCurrent()){setError("Lane or sample changed; reopen generator");cancelSequenceGenerate();return;}
        if(audioRecording()||clipRecording||patternBusy()||clipPreparationInFlight()){setError("Stop recording or finish transition first");return;}
        if(!sequenceGenerateValid){setError(sequenceGenerateStatus.data());return;}
        if(!sequenceGenerateDraft.changedSteps){clearError();cancelSequenceGenerate(true);return;}
        const int part=sequenceGeneratePart;
        captureActiveClip(false);pushClipUndo(activeClip);
        undoIsSequenceGenerate=true;sequenceGenerateUndoPart=part;
        ensureSequenceClip();
        sequenceLive.tracks[part].trigs=sequenceGenerateDraft.lane.trigs;
        sequencePlayback.tracks[part].trigs=sequenceGenerateDraft.lane.trigs;
        if(auto* clip=activeClipSlot())clip->sequence=sequenceLive;
        sequenceUndoLane=sequenceUndoStep=-1;
        ++sequenceRevision;markPersistentStateDirty();clearError();cancelSequenceGenerate(true);
        // No clock, cursor, held voice or other lane is restarted.
    }
    bool undoSequenceGenerate() {
        if(!undoIsSequenceGenerate||!undoAvailable)return false;
        if(clipRecording||audioRecording()||patternBusy()||clipPreparationInFlight()) {
            setError("Stop recording or finish transition before undo");return true;
        }
        cancelSequenceGenerate();
        auto& clip=clipSlots[std::size_t(undoClipIndex)];
        const int part=sequenceGenerateUndoPart;
        clip.sequence.tracks[part]=undoClip.sequence.tracks[part];
        if(!undoClip.clipRecorded) {
            clip.clipRecorded=false;clip.clipDurationMs=undoClip.clipDurationMs;
            clip.takeCount=undoClip.takeCount;clip.recordedBeats=undoClip.recordedBeats;
            clip.recordedTempoBpm=undoClip.recordedTempoBpm;clip.timingMode=undoClip.timingMode;
            clip.occupied=undoClip.occupied;
        }
        if(activeClip==undoClipIndex){
            sequenceLive.tracks[part]=undoClip.sequence.tracks[part];
            sequencePlayback.tracks[part].trigs=undoClip.sequence.tracks[part].trigs;
            activeClipOccupied=clip.occupied;
        }
        undoAvailable=undoIsSequenceGenerate=false;sequenceUndoLane=sequenceUndoStep=-1;
        ++sequenceRevision;markPersistentStateDirty();return true;
    }
    bool setSequenceGenerateParameter(const char* key,const char* text) {
        if(std::strncmp(key,"seq_gen_",8))return false;
        if(!std::strcmp(key,"seq_gen_begin")){beginSequenceGenerate();return true;}
        if(!std::strcmp(key,"seq_gen_cancel")){cancelSequenceGenerate();return true;}
        if(!sequenceGenerateOwner())return true;
        if(!std::strcmp(key,"seq_gen_play")){
            if(!sequenceGenerateValid){setError(sequenceGenerateStatus.data());return true;}
            clearError();
            if(clipPlaying)setClipPlaying(false);
            else {clipLaunchPending=false;startClipPlaybackNow();}
            return true;
        }
        if(!std::strcmp(key,"seq_gen_apply")){applySequenceGenerate();return true;}
        if(!std::strcmp(key,"seq_gen_refresh")){rebuildSequenceGenerate();return true;}
        if(!std::strcmp(key,"seq_gen_select")){const int step=parseInteger(text,-1);if(step>=0&&step<sequenceGenerateBase.length){sequenceGenerateSelected=step;formatSequenceGenerateState(sequenceGenerateCache.data(),sequenceGenerateCache.size());}return true;}
        if(!std::strcmp(key,"seq_gen_edit")){
            int field=-1;float value=0;char tail=0;
            if(std::sscanf(text,"%d,%f %c",&field,&value,&tail)==2
                &&sequenceGenerateConfigs[sequenceGeneratePart].set(field,value,sequenceGenerateBase.length,sequenceGenerateBase.timeMode))rebuildSequenceGenerate();
        }
        return true;
    }
    int sequenceGenerateState(char* buffer,int capacity) const {
        const int n=!sequenceGenerateOwner()||!sequenceGenerateSourceCurrent()
            ?std::snprintf(buffer,capacity,"{\"active\":0}")
            :std::snprintf(buffer,capacity,"%s",sequenceGenerateCache.data());
        return n>=0&&n<capacity?n:0;
    }
    int formatSequenceGenerateState(char* buffer,int capacity) const {
        const bool active=sequenceGenerateOwner(),current=active&&sequenceGenerateSourceCurrent();
        int at=std::snprintf(buffer,capacity,"{\"active\":%d,\"valid\":%d,\"clip\":%d,\"part\":%d,\"length\":%d,\"selected\":%d,\"timed\":%d,\"changed\":%d,\"events\":%d,\"dropped\":%d,\"status\":\"%s\",\"source_kind\":\"%s\",\"config\":[",
            active?1:0,current&&sequenceGenerateValid?1:0,sequenceGenerateClip,sequenceGeneratePart,sequenceGenerateBase.length,sequenceGenerateSelected,sequenceGenerateBase.timeMode?1:0,sequenceGenerateDraft.changedSteps,sequenceGenerateDraft.events,sequenceGenerateDraft.dropped,
            active&&!current?"LANE CHANGED: REOPEN":sequenceGenerateStatus.data(),sequenceGenerateMapName);
        auto append=[&](const char* format,auto... values){if(at<0||at>=capacity)return;const int n=std::snprintf(buffer+at,std::size_t(capacity-at),format,values...);at=n<0||n>=capacity-at?capacity:at+n;};
        const auto& config=sequenceGenerateConfigs[std::size_t(std::max(0,sequenceGeneratePart))];
        for(int i=0;i<s3g_thru_generate::fields;++i)append(i?",%.7g":"%.7g",double(config.v[i]));
        append("%s", "],\"steps\":[");
        for(int i=0;i<32;++i){const auto& step=sequenceGenerateDraft.lane.trigs[i];append("%s[%d,%.7g,%d]",i?",":"",step.enabled?1:0,double(s3g_thru_sequence::resolve(sequenceGenerateDraft.lane,step,-1,0,liveControlsForPart(std::max(0,sequenceGeneratePart)).locus)),sequenceGenerateDraft.changed[i]?1:0);}
        append("%s", "],\"source\":[");
        for(int i=0;i<sequenceGenerateSource.count;++i)append(i?",%.7g":"%.7g",double(sequenceGenerateSource.points[i]));
        append("%s", "]}");
        return at>=0&&at<capacity?at:0;
    }
