    struct SliceEditor {
        bool active=false,loop=false,latched=false;
        int part=0,selected=0,boundary=1,pad=-1,voice=-1,page=0;
        float cursor=0,zoom=1,viewLow=0,viewHigh=1;
        std::uint32_t viewRequest=0,viewComplete=0;
        std::uint64_t viewRevision=0;
        std::size_t viewLane=0;
        float readyLow=0,readyHigh=1;
        WaveBins minimum{},maximum{};
    } sliceEditor;
    bool undoIsSlice=false;
    s3g_thru::SliceLaneSettings sliceUndo;
    int sliceUndoClip=-1,sliceUndoLane=0,sliceUndoPart=0,sliceUndoMode=1,sliceGesture=-1;
    std::uint64_t sliceGestureClock=0;

    std::array<float,2> sliceSourceWindow(const Engine::Controls& c) const {
        const float width=clampValue(c.field,0.000001f,1.f);
        const float low=clampValue(c.locus-width*.5f,0.f,1.f-width);
        return {low,low+width};
    }
    s3g_thru::SliceMap sliceMapForLane(std::size_t lane,const Engine::Controls& c,int& kind) const {
        s3g_thru::SliceMap map;kind=0;
        if(lane>=kLaneCount||!engine.laneLoaded(lane))return map;
        const auto* clip=activeClipSlot();const int mode=static_cast<int>(c.sampleRegionMode);
        if((mode==3||mode==4)&&clip&&clip->sliceLanes[lane].edited.count) {
            kind=3;return clip->sliceLanes[lane].edited;
        }
        if((mode==2||mode==3||mode==4)&&fileSliceMaps[lane].count) {
            kind=2;return fileSliceMaps[lane];
        }
        if(mode==1||mode==3||mode==4) {
            const auto& regions=activeRegions[lane];
            map.count=static_cast<std::uint32_t>(regions.count);
            if(map.count&&map.count<=s3g_thru::sliceCapacity) {
                map.points=regions.onsets;
                if(!s3g_thru::validSliceMap(map))map.points=regions.boundaries;
                if(s3g_thru::validSliceMap(map)){kind=1;return map;}
            }
        }
        map={};map.count=static_cast<std::uint32_t>(clampValue(static_cast<int>(std::lround(c.sampleRegionCount)),1,64));
        for(std::size_t i=0;i<=map.count;++i)map.points[i]=static_cast<float>(i)/map.count;
        return map;
    }
    float slicePreRoll(std::size_t lane,int kind) const {
        const auto* clip=activeClipSlot();
        const float value=clip?clip->sliceLanes[lane].preRollMs:-1;
        return value>=0?value:kind==1?(value== -2?1.f:3.f):0.f;
    }
    s3g_thru::SliceMap sliceEditorMap(int* source=nullptr) const {
        int kind=0;auto map=sliceMapForLane(selectedLane,liveControlsForPart(sliceEditor.part),kind);
        if(kind==0&&map.count) {
            const auto window=sliceSourceWindow(liveControlsForPart(sliceEditor.part));
            for(std::size_t i=0;i<=map.count;++i)map.points[i]=window[0]+map.points[i]*(window[1]-window[0]);
        }
        if(source)*source=kind;
        return map;
    }
    void applySliceVoiceCache(const PlaybackVoice& voice,Engine::Controls& c) const {
        if(c.algorithm!=Engine::PlaybackAlgorithm::slices||!voice.sliceCached)return;
        c.sampleRegionOverride=true;c.sampleRegionAbsolute=voice.sliceAbsolute;
        c.sampleRegionLow=voice.sliceLow;c.sampleRegionHigh=voice.sliceHigh;c.sampleRegionLead=voice.sliceLead;
    }
    int sliceVoiceLane(const PlaybackVoice& voice,const Engine::Controls& c) const {
        float position=c.performanceLaneFixed>=0?c.performanceLaneFixed:c.sampleLane;
        position+=(c.performanceLanePosition-position)*clampValue(c.performanceLaneAmount,0.f,1.f);
        const int requested=clampValue(static_cast<int>(std::floor(position+.5f)),0,3);
        if(engine.laneLoaded(requested)&&voiceOwnsLane(voice,requested))return requested;
        for(int lane=0;lane<4;++lane)if(engine.laneLoaded(lane)&&voiceOwnsLane(voice,lane))return lane;
        return -1;
    }
    void prepareSliceVoice(PlaybackVoice& voice,Engine::Controls& c) {
        if(c.algorithm!=Engine::PlaybackAlgorithm::slices||voice.bankAsset>=0)return;
        if(voice.sliceAudition) {
            normalizeSliceEditor();
            c.sampleLane=static_cast<float>(selectedLane);c.sampleEditCrossfade=true;c.locus=.5f;c.field=1;
            c.performanceLaneFixed=-1;c.performanceLaneAmount=0;
            c.sampleMode=sliceEditor.loop?Engine::SampleMode::loop:Engine::SampleMode::forward;
            c.ampAttackMs=2;c.ampDecayMs=0;c.ampSustain=1;c.ampReleaseMs=5;
        }
        const int lane=sliceVoiceLane(voice,c);if(lane<0)return;
        if(!voice.sliceCached||voice.sliceCachedLane!=lane
            ||voice.sliceSourceRevision!=wavesetSourceRevisions[lane]||voice.sliceAudition) {
            int kind=0;const auto map=voice.sliceAudition?sliceEditorMap(&kind):sliceMapForLane(lane,c,kind);
            if(!map.count)return;
            const auto index=static_cast<std::size_t>(voice.sliceAudition?sliceEditor.selected:std::max(0,static_cast<int>(c.sampleSlice)))%map.count;
            voice.sliceLow=map.points[index];voice.sliceHigh=map.points[index+1];
            voice.sliceAbsolute=voice.sliceAudition||kind!=0;
            voice.sliceLead=slicePreRoll(lane,kind)*.001f*engine.laneSampleRate(lane)/engine.laneFrames(lane);
            voice.sliceCachedLane=lane;voice.sliceSourceRevision=wavesetSourceRevisions[lane];voice.sliceCached=true;
        }
        applySliceVoiceCache(voice,c);
    }
    bool sampleLoadBusy() const {
        const auto state=static_cast<SampleLoadState>(sampleLoadState.load(std::memory_order_acquire));
        return state==SampleLoadState::requested||state==SampleLoadState::loading
            ||state==SampleLoadState::ready||state==SampleLoadState::removing||state==SampleLoadState::copying;
    }
    bool sliceEditorAllowed() const {
        return !audioRecording()&&!clipPreparationInFlight()&&!patternBusy()
            &&!sampleLoadBusy()&&!pendingClearMask
            &&selectedLane<kLaneCount&&engine.laneLoaded(selectedLane)
            &&(!splitEnabled||partOwnsLane(splitFocus,selectedLane));
    }
    PlaybackVoice* sliceAuditionVoice() {
        return sliceEditor.voice>=0&&sliceEditor.voice<static_cast<int>(voices.size())
            &&voices[sliceEditor.voice].active&&voices[sliceEditor.voice].sliceAudition?&voices[sliceEditor.voice]:nullptr;
    }
    void stopSliceAudition() {
        if(auto* v=sliceAuditionVoice()){v->held=false;beginVoiceRelease(*v);}
        sliceEditor.voice=-1;sliceEditor.pad=-1;sliceEditor.latched=false;
    }
    void closeSliceEditor() {
        stopSliceAudition();sliceEditor.active=false;
    }
    void normalizeSliceEditor() {
        const auto map=sliceEditorMap();
        sliceEditor.selected=clampValue(sliceEditor.selected,0,std::max(0,static_cast<int>(map.count)-1));
        sliceEditor.boundary=clampValue(sliceEditor.boundary,0,static_cast<int>(map.count));
        sliceEditor.cursor=clampValue(sliceEditor.cursor,0.f,1.f);
        const int perPage=splitEnabled?(isQuad()?8:16):32;
        sliceEditor.page=clampValue(sliceEditor.page,0,std::max(0,(static_cast<int>(map.count)-1)/perPage));
    }
    bool startSliceAudition(int pad=-1,bool latch=false) {
        if(!sliceEditor.active||!sliceEditorAllowed())return false;
        const auto map=sliceEditorMap();if(!map.count)return false;
        auto* existing=sliceAuditionVoice();
        auto& v=existing?*existing:allocateClipVoice(sliceEditor.part);
        const bool reusing=v.active;
        if(reusing){beginVoiceStealTransition(v);clearPadMappingForVoice(v);}
        releaseBankVoice(v);v.bankAsset=v.bankSlot=-1;
        v.note=settingsForPart(sliceEditor.part).root;v.sourcePad=-1;v.velocity=127;v.part=sliceEditor.part;
        v.active=v.held=true;v.clipOwned=false;v.clipNoteIndex=-1;v.age=++noteClock;
        v.engineSlot=-1;v.assignedAlgorithm=static_cast<int>(Engine::PlaybackAlgorithm::slices);v.entryVariation={};
        v.variantLane=selectedLane;v.sliceAudition=true;v.sliceCached=false;v.midiExternal=false;
        v.midiInput=v.midiChannel=-1;v.midiGain=1;v.midiTarget={};v.midiValue={};
        v.pressureTarget=v.pressureState=0;v.pressureBrakeHeld=false;v.laneOffset=0;v.cutLanePressure=0;
        v.liveStackPhaseValid=v.recordedStackPhaseValid=false;v.panicRelease=false;
        if(!reusing){v.stealPending=false;v.stealFadeRemaining=0;v.panState=0;v.gainState=outputGain;
            v.pressureToneMix=0;v.pressureToneCoefficient=1;v.pressureToneState={};beginVoiceAttack(v);v.engine.retrigger(false);}
        sliceEditor.voice=static_cast<int>(&v-voices.data());sliceEditor.pad=pad;sliceEditor.latched=latch;
        return true;
    }
    bool sliceEditorPadOn(int pad,int) {
        if(!sliceEditor.active||pad<0||pad>=32||partForInput(pad)!=sliceEditor.part)return false;
        if(!sliceEditorAllowed())return true;
        normalizeSliceEditor();
        const int perPage=splitEnabled?(isQuad()?8:16):32;
        const auto map=sliceEditorMap();const int index=localPad(pad)+sliceEditor.page*perPage;
        if(index>=static_cast<int>(map.count))return true;
        if(shiftHeld&&sliceEditor.latched&&sliceEditor.pad==pad){stopSliceAudition();return true;}
        sliceEditor.selected=index;sliceEditor.boundary=index?index:1;
        sliceEditor.cursor=map.points[index];startSliceAudition(pad,shiftHeld);return true;
    }
    void sliceEditorPadOff(int pad) {
        if(sliceEditor.active&&sliceEditor.pad==pad&&!sliceEditor.latched)stopSliceAudition();
    }
    void pushSliceUndo(int gesture,bool force=false) {
        const auto now=renderClockSamples.load(std::memory_order_relaxed);
        const bool continued=!force&&undoAvailable&&undoIsSlice&&sliceUndoClip==activeClip
            &&sliceUndoLane==static_cast<int>(selectedLane)&&sliceGesture==gesture
            &&now-sliceGestureClock<static_cast<std::uint64_t>(sampleRate*.35f);
        if(!continued) {
            sliceUndo=activeClipSlot()->sliceLanes[selectedLane];sliceUndoClip=activeClip;
            sliceUndoLane=static_cast<int>(selectedLane);sliceUndoPart=sliceEditor.part;
            sliceUndoMode=static_cast<int>(liveControlsForPart(sliceEditor.part).sampleRegionMode);
            undoAvailable=true;undoIsSlice=true;undoIsBank=undoIsChain=false;
        }
        sliceGesture=gesture;sliceGestureClock=now;
    }
    void storeSliceMap(const s3g_thru::SliceMap& map) {
        int kind=0;sliceEditorMap(&kind);
        auto& data=activeClipSlot()->sliceLanes[selectedLane];
        if(data.preRollMs<0)data.preRollMs=slicePreRoll(selectedLane,kind);
        data.edited=map;controls.sampleRegionMode=Engine::RegionMode::edited;
        if(splitEnabled)storeFocusedPart();
        markPersistentStateDirty();normalizeSliceEditor();
    }
    void restoreSliceUndo() {
        if(!undoAvailable||!undoIsSlice||sliceUndoClip<0||sliceUndoClip>=static_cast<int>(kClipSlotCount))return;
        if(audioRecording()||clipRecording||clipPreparationInFlight()||sampleLoadBusy()||patternBusy()) {
            setError("Stop recording and finish source/clip changes before undo");return;
        }
        auto& clip=clipSlots[sliceUndoClip];clip.sliceLanes[sliceUndoLane]=sliceUndo;
        const auto parameter=static_cast<std::size_t>(LockParameter::sampleRegionMode);
        if(activeClip==sliceUndoClip) {
            const int focus=splitFocus;
            if(splitEnabled)selectSplitPart(sliceUndoPart,false);
            controls.sampleRegionMode=static_cast<Engine::RegionMode>(sliceUndoMode);
            if(splitEnabled){storeFocusedPart();selectSplitPart(focus,false);}
        } else {clip.baseValues[parameter]=static_cast<float>(sliceUndoMode);clip.splitValues[sliceUndoPart][parameter]=static_cast<float>(sliceUndoMode);}
        undoAvailable=undoIsSlice=false;markPersistentStateDirty();normalizeSliceEditor();
    }
    bool setSliceEditorParam(const char* key,const char* value) {
        if(std::strncmp(key,"slice_editor_",13))return false;
        if(!std::strcmp(key,"slice_editor_open")) {
            if(!parseInteger(value,0)){closeSliceEditor();return true;}
            if(!sliceEditorAllowed()){setError("Load a lane and finish recording/loading first");return true;}
            stopSliceAudition();sliceEditor.active=true;sliceEditor.part=splitEnabled?splitFocus:0;
            sliceEditor.selected=0;sliceEditor.boundary=1;sliceEditor.cursor=0;sliceEditor.page=0;
            sliceEditor.zoom=1;sliceEditor.viewLow=0;sliceEditor.viewHigh=1;++sliceEditor.viewRequest;normalizeSliceEditor();return true;
        }
        if(!sliceEditor.active)return true;
        if(!std::strcmp(key,"slice_editor_lane")) {
            const auto lane=static_cast<std::size_t>(clampValue(parseInteger(value,1),1,4)-1);
            if(lane==selectedLane)return true;
            if(!engine.laneLoaded(lane)||(splitEnabled&&!partOwnsLane(sliceEditor.part,lane))) {setError("Choose a loaded lane in this part");return true;}
            stopSliceAudition();selectedLane=lane;sliceEditor.selected=0;sliceEditor.boundary=1;
            sliceEditor.cursor=0;sliceEditor.page=0;sliceEditor.viewLow=0;sliceEditor.viewHigh=1;
            ++sliceEditor.viewRequest;normalizeSliceEditor();return true;
        }
        if(!std::strcmp(key,"slice_editor_audition")) {
            if(parseInteger(value,0))startSliceAudition();else stopSliceAudition();return true;
        }
        if(!std::strcmp(key,"slice_editor_loop")) {
            sliceEditor.loop=parseInteger(value,0)!=0;
            if(sliceEditor.loop&&!sliceAuditionVoice())startSliceAudition(-1,true);
            return true;
        }
        if(!std::strcmp(key,"slice_editor_select")) {
            sliceEditor.selected=parseInteger(value,0);normalizeSliceEditor();const auto map=sliceEditorMap();
            sliceEditor.boundary=sliceEditor.selected?sliceEditor.selected:1;
            sliceEditor.cursor=map.points[sliceEditor.selected];return true;
        }
        if(!std::strcmp(key,"slice_editor_page")){sliceEditor.page=clampValue(parseInteger(value,0),0,7);return true;}
        if(!std::strcmp(key,"slice_editor_cursor")){sliceEditor.cursor=clampValue(parseFloat(value,sliceEditor.cursor),0.f,1.f);return true;}
        if(!std::strcmp(key,"slice_editor_view")) {
            float low=0,high=1;int n=0;
            if(std::sscanf(value,"%f,%f%n",&low,&high,&n)!=2||value[n]||!std::isfinite(low)||!std::isfinite(high)||low<0||high>1||high<=low)return true;
            if(low!=sliceEditor.viewLow||high!=sliceEditor.viewHigh||sliceEditor.viewLane!=selectedLane||sliceEditor.viewRevision!=wavesetSourceRevisions[selectedLane]) {
                sliceEditor.viewLow=low;sliceEditor.viewHigh=high;++sliceEditor.viewRequest;
            }
            return true;
        }
        if(!std::strcmp(key,"slice_editor_undo")){restoreSliceUndo();return true;}
        if(!sliceEditorAllowed()){setError("Finish recording or loading before editing slices");return true;}
        if(!std::strcmp(key,"slice_editor_preroll")) {
            const float next=clampValue(parseFloat(value,0),0.f,30.f);int kind=0;sliceEditorMap(&kind);
            if(next!=slicePreRoll(selectedLane,kind)){pushSliceUndo(-2);activeClipSlot()->sliceLanes[selectedLane].preRollMs=next;markPersistentStateDirty();}
            return true;
        }
        auto map=sliceEditorMap();if(!map.count)return true;
        const float gap=1.f/engine.laneFrames(selectedLane);
        if(!std::strcmp(key,"slice_editor_move")) {
            int point=0,n=0;float position=0;
            if(std::sscanf(value,"%d,%f%n",&point,&position,&n)!=2||value[n]||!std::isfinite(position)||point<0||point>static_cast<int>(map.count))return true;
            position=clampValue(position,point?map.points[point-1]+gap:0.f,point<static_cast<int>(map.count)?map.points[point+1]-gap:1.f);
            if(position==map.points[point])return true;
            pushSliceUndo(point);map.points[point]=position;
            if(!s3g_thru::validSliceMap(map))return true;
            sliceEditor.boundary=point;sliceEditor.cursor=position;storeSliceMap(map);return true;
        }
        if(!std::strcmp(key,"slice_editor_add")) {
            const float position=clampValue(parseFloat(value,sliceEditor.cursor),0.f,1.f);
            if(s3g_thru::addSlicePoint(map,position,gap)) {
                pushSliceUndo(-3,true);storeSliceMap(map);
                for(std::size_t i=1;i<map.count;++i)if(map.points[i]==position){sliceEditor.selected=static_cast<int>(i);sliceEditor.boundary=static_cast<int>(i);break;}
            } else setError(map.count==64?"Maximum 64 slices":"Cut too close to a boundary");
            return true;
        }
        if(!std::strcmp(key,"slice_editor_delete")) {
            const int point=parseInteger(value,sliceEditor.boundary);deleteHeld=false;
            if(point>=0&&s3g_thru::removeSlicePoint(map,static_cast<std::size_t>(point))) {
                pushSliceUndo(-4,true);storeSliceMap(map);
            } else setError("First and last boundaries are protected");
            return true;
        }
        return true;
    }
    int formatSliceEditor(char* buffer,int size) const {
        if(!sliceEditor.active)return std::snprintf(buffer,size,"{\"v\":1,\"active\":0}");
        int kind=0;const auto map=sliceEditorMap(&kind);const auto selected=clampValue(sliceEditor.selected,0,std::max(0,static_cast<int>(map.count)-1));
        const auto* v=sliceEditor.voice>=0&&sliceEditor.voice<static_cast<int>(voices.size())?&voices[sliceEditor.voice]:nullptr;
        const bool playing=v&&v->active&&v->held&&v->sliceAudition;
        char label[24]{};for(std::size_t i=0;i+1<sizeof(label);++i){char ch=map.labels[selected][i];if(!ch)break;label[i]=ch=='"'||ch=='\\'?' ':ch;}
        const float duration=engine.laneLoaded(selectedLane)?engine.laneFrames(selectedLane)*1000.f/engine.laneSampleRate(selectedLane):0;
        int at=std::snprintf(buffer,size,"{\"v\":1,\"active\":1,\"lane\":%zu,\"part\":%d,\"selected\":%d,\"boundary\":%d,\"cursor\":%.9g,\"count\":%u,\"preroll\":%.7g,\"duration\":%.9g,\"source\":%d,\"mode\":%d,\"overflow\":%d,\"loop\":%d,\"playing\":%d,\"latched\":%d,\"pad\":%d,\"page\":%d,\"label\":\"%s\",\"request\":%u,\"points\":[",
            selectedLane+1,sliceEditor.part,selected,sliceEditor.boundary,static_cast<double>(sliceEditor.cursor),map.count,
            static_cast<double>(slicePreRoll(selectedLane,kind)),static_cast<double>(duration),kind,
            static_cast<int>(liveControlsForPart(sliceEditor.part).sampleRegionMode),map.overflow?1:0,sliceEditor.loop?1:0,
            playing?1:0,sliceEditor.latched?1:0,sliceEditor.pad,sliceEditor.page,label,sliceEditor.viewRequest);
        if(at<0||at>=size)return -1;
        for(std::size_t i=0;i<=map.count;++i){const int n=std::snprintf(buffer+at,size-at,"%s%.9g",i?",":"",static_cast<double>(map.points[i]));if(n<0||n>=size-at)return -1;at+=n;}
        const int n=std::snprintf(buffer+at,size-at,"]}");return n<0||n>=size-at?-1:at+n;
    }
    int formatSliceEditorWave(char* buffer,int size) const {
        int at=std::snprintf(buffer,size,"%u,%zu,%.9g,%.9g",sliceEditor.viewComplete,sliceEditor.viewLane+1,
            static_cast<double>(sliceEditor.readyLow),static_cast<double>(sliceEditor.readyHigh));
        if(at<0||at>=size)return -1;
        for(std::size_t i=0;i<kWaveBins;++i) {
            const int n=std::snprintf(buffer+at,size-at,",%d,%d",static_cast<int>(sliceEditor.minimum[i]),static_cast<int>(sliceEditor.maximum[i]));
            if(n<0||n>=size-at)return -1;
            at+=n;
        }
        return at;
    }
    bool writeSliceMap(std::FILE* file,const s3g_thru::SliceMap& map) const {
        bool ok=writeU32(file,map.count)&&writeU32(file,map.overflow?1u:0u);
        for(float point:map.points)ok=ok&&writeF32(file,point);
        for(const auto& label:map.labels)ok=ok&&writeBytes(file,label.data(),label.size());
        return ok;
    }
    bool readSliceMap(std::FILE* file,s3g_thru::SliceMap& map) {
        std::uint32_t overflow=0;
        if(!readU32(file,map.count)||map.count>64||!readU32(file,overflow)||overflow>1)return false;
        map.overflow=overflow!=0;
        for(auto& point:map.points)if(!readF32(file,point)||!std::isfinite(point))return false;
        for(auto& label:map.labels) {
            if(std::fread(label.data(),1,label.size(),file)!=label.size())return false;
            for(auto& ch:label)if(ch&&(static_cast<unsigned char>(ch)<32||static_cast<unsigned char>(ch)>126))ch='?';
            label.back()=0;
        }
        return s3g_thru::validSliceMap(map);
    }
    bool writeSliceExtension(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips) const {
        bool ok=writeU32(file,0x31434c53u);
        for(const auto& clip:clips)for(const auto& lane:clip.sliceLanes)
            ok=ok&&writeF32(file,lane.preRollMs)&&writeSliceMap(file,lane.edited)&&writeSliceMap(file,lane.file);
        return ok;
    }
    bool readSliceExtension(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips) {
        std::uint32_t tag=0;if(!readU32(file,tag)||tag!=0x31434c53u)return false;
        for(auto& clip:clips)for(auto& lane:clip.sliceLanes) {
            if(!readF32(file,lane.preRollMs)||!std::isfinite(lane.preRollMs)||(lane.preRollMs<0&&lane.preRollMs!= -1&&lane.preRollMs!= -2)||lane.preRollMs>30
                ||!readSliceMap(file,lane.edited)||!readSliceMap(file,lane.file))return false;
        }
        return true;
    }
