    using StackPath=s3g_daisy::ThruStackPath;
    std::array<int,5u> stackPathSelections {};
    std::uint64_t stackPathLastEditFrame=0;
    int stackPathLastEdit=-1;

    int stackPathOwner() const {return splitEnabled?splitFocus+1:0;}
    void captureStackPathEdit(const StackPath& before) {
        controls.stackPath.sanitize();
        for(std::size_t i=0;i<StackPath::valueCount;++i)
            if(before.value(i)!=controls.stackPath.value(i)) {
                const int part=stackPathOwner();
                if(clipRecording && clipLastPointIndices[kClipStackPathOffset+part*50u+25u+i]<0)
                    captureClipPoint(ClipPointType::stackPath,25u+i,part,before.value(i),0);
                if(clipRecording && clipReplacing && clipCaptureTimeMs()>0
                    && clipLastPointIndices[kClipStackPathOffset+part*50u+i]<0)
                    captureClipPoint(ClipPointType::stackPath,i,part,before.value(i),0);
                captureClipPoint(ClipPointType::stackPath,i,part,controls.stackPath.value(i));
            }
        markPersistentStateDirty();
    }
    void prepareStackPathUndo(int field) {
        if(clipRecording){stackPathLastEdit=-1;return;}
        const auto frame=renderClockSamples.load(std::memory_order_relaxed);
        if(field!=stackPathLastEdit || frame-stackPathLastEditFrame>static_cast<std::uint64_t>(sampleRate*.4f))
            pushClipUndo(activeClip);
        stackPathLastEdit=field;stackPathLastEditFrame=frame;
    }
    void editStackPath(const char* operation,int point,float value) {
        clearError();auto& path=controls.stackPath;path.sanitize();
        point=clampValue(point,0,path.count-1);
        auto& selected=stackPathSelections[stackPathOwner()];
        if(std::strcmp(operation,"select")==0){selected=point;return;}
        const auto before=path;
        if(std::strcmp(operation,"count")==0) {
            if(!std::isfinite(value))return;
            const int wanted=static_cast<int>(std::round(clampValue(value,4.0f,8.0f)));
            if(wanted==path.count)return;
            prepareStackPathUndo(stackPathOwner()*100+25);
            selected=point;
            while(path.count<wanted) {
                const int added=path.insertAfter(selected);
                if(added<0){setError("Move points apart before adding");break;}
                selected=added;
            }
            while(path.count>wanted)selected=path.remove(selected);
        } else if(std::strcmp(operation,"reset")==0) {
            prepareStackPathUndo(stackPathOwner()*100+26);path=StackPath{};selected=0;
        } else {
            if(!std::isfinite(value))return;
            int field=-1;
            if(std::strcmp(operation,"time")==0)field=0;
            if(std::strcmp(operation,"lane")==0)field=1;
            if(std::strcmp(operation,"curve")==0)field=2;
            if(field<0)return;
            if(field==0 && point==0){setError("First point stays at zero time");return;}
            if(field==0) {
                const float high=(point+1<path.count?path.time[point+1]:1)-StackPath::minimumGap;
                value=clampValue(value,std::min(high,path.time[point-1]+StackPath::minimumGap),high);
            }
            const auto parameter=1u+static_cast<std::size_t>(point)*3u+field;
            value=clampValue(value,field==2?-1.0f:0.0f,1.0f);
            if(path.value(parameter)==value)return;
            prepareStackPathUndo(stackPathOwner()*100+static_cast<int>(parameter));
            path.setValue(parameter,value);selected=point;
        }
        captureStackPathEdit(before);
    }
    int formatStackPath(char* buffer,int capacity) const {
        const auto& path=controls.stackPath;
        int at=std::snprintf(buffer,capacity,"%d,%d",path.count,
            clampValue(stackPathSelections[stackPathOwner()],0,path.count-1));
        for(std::size_t i=1;i<StackPath::valueCount;++i) {
            if(at<0||at>=capacity)return -1;
            const int n=std::snprintf(buffer+at,capacity-at,",%.6f",path.value(i));
            if(n<0||n>=capacity-at)return -1;
            at+=n;
        }
        return at;
    }
    static bool writeStackPaths(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips) {
        if(!writeU32(file,0x42505431u))return false;
        for(const auto& clip:clips)for(const auto& path:clip.stackPaths)
            for(std::size_t i=0;i<StackPath::valueCount;++i)if(!writeF32(file,path.value(i)))return false;
        return true;
    }
    static bool readStackPaths(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips) {
        std::uint32_t magic=0;if(!readU32(file,magic)||magic!=0x42505431u)return false;
        for(auto& clip:clips)for(auto& path:clip.stackPaths) {
            for(std::size_t i=0;i<StackPath::valueCount;++i) {
                float value=0;if(!readF32(file,value)||!std::isfinite(value))return false;
                if(i==0 && (value<4||value>8||value!=std::round(value)))return false;
                if(i>0 && (value<((i-1)%3==2?-1.0f:0.0f)||value>1))return false;
                path.setValue(i,value);
            }
            if(!path.valid())return false;
        }
        return true;
    }
    static bool readStackPathJson(const char* json,const char* name,StackPath& path) {
        char key[64]{};std::snprintf(key,sizeof(key),"\"%s\":[",name);
        const char* text=std::strstr(json,key);if(!text)return true;text+=std::strlen(key);
        StackPath read;
        for(std::size_t i=0;i<StackPath::valueCount;++i) {
            char* end=nullptr;const float value=std::strtof(text,&end);
            if(end==text||!std::isfinite(value)||*end!=(i+1<StackPath::valueCount?',':']'))return false;
            if(i==0 && (value<4||value>8||value!=std::round(value)))return false;
            if(i>0 && (value<((i-1)%3==2?-1.0f:0.0f)||value>1))return false;
            read.setValue(i,value);text=end+1;
        }
        if(!read.valid())return false;
        path=read;
        return true;
    }

    void captureStackPathBase(ClipSlot& clip,int owner,const StackPath& live) {
        clip.stackPaths[owner]=live;
        clip.stackPaths[owner].sanitize();
    }
    void undoStackPathPoints(const ClipSlot& clip,const std::array<std::array<bool,25u>,5u>& removed,const std::array<std::array<float,25u>,5u>& prior) {
        for(int owner=0;owner<5;++owner) {
            StackPath* path=owner==0 ? (splitEnabled?&splitSingleControls.stackPath:&controls.stackPath)
                : splitEnabled&&owner-1==splitFocus ? &controls.stackPath : &splitParts[owner-1].controls.stackPath;
            bool changed=false;
            for(std::size_t p=0;p<25;++p)if(removed[owner][p]) {
                const float value=std::isfinite(prior[owner][p])?prior[owner][p]:clip.stackPaths[owner].value(p);
                path->setValue(p,value);changed=true;
            }
            if(changed)path->sanitize();
            if(owner>0)splitClipControls[owner-1].stackPath=*path;
            else if(!splitEnabled)clipControls.stackPath=*path;
        }
        stackPathLastEdit=-1;
    }

    void captureCurrentStackPaths(ClipSlot& clip) {
        captureStackPathBase(clip,0,splitEnabled?splitSingleControls.stackPath:controls.stackPath);
        for(int part=0;part<4;++part)
            captureStackPathBase(clip,part+1,splitEnabled?liveControlsForPart(part).stackPath:splitParts[part].controls.stackPath);
    }
