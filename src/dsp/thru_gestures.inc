// Transient performance controls are kept apart from saved engine settings.
    static constexpr std::size_t kPerformanceControls = 9u;
    std::array<std::array<float,9u>,4u> performanceLive {}, performanceRecorded {};
    std::array<std::array<float,8u>,4u> performanceSmooth {};
    std::array<int,8u> performancePadOwners {{-1,-1,-1,-1,-1,-1,-1,-1}};
    bool performanceCaptureHeld = false;
    int performancePart = 0;
    std::array<float,4u> performanceLaneEngagement {};

    int activePartCount() const { return splitEnabled ? partCount() : 1; }
    bool performanceLaneAvailable(int part, int lane) const
    {
        return !isQuad() && lane >= 0 && lane < 4 && engine.laneLoaded(lane)
            && (!splitEnabled || partOwnsLane(part,lane));
    }
    void setPerformanceValue(int part, int index, float value)
    {
        if (part<0 || part>=4 || index<0 || index>=9) return;
        value=clampValue(value,0.0f,index==8?4.0f:1.0f);
        if(value==performanceLive[part][index]) return;
        if(clipRecording && clipReplacing && clipCaptureTimeMs()>0
            && clipLastPointIndices[kClipPerformanceOffset+part*9u+index]<0)
            captureClipPoint(ClipPointType::performance,index,part,performanceLive[part][index],0);
        performanceLive[part][index]=value;
        captureClipPoint(ClipPointType::performance,index,part,performanceLive[part][index]);
    }
    void cancelPerformanceGestures()
    {
        performanceCaptureHeld=false;
        for (auto& owner:performancePadOwners) if(owner>=0) owner=-2;
        for(int part=0;part<4;++part)
            for(int index=0;index<8;++index)
                if(performanceLive[part][index]!=0) setPerformanceValue(part,index,0);
    }
    void beginPerformanceCapture()
    {
        if (shiftHeld) return;
        performanceCaptureHeld=true;
        performancePart=splitEnabled?splitFocus:0;
    }
    bool performancePadOn(int pad)
    {
        if(pad<0 || pad>=8) return false;
        if(performancePadOwners[pad]!=-1) return true;
        if(!performanceCaptureHeld || shiftHeld) return false;
        // This applies only to a new strike. Notes held before Capture retain
        // their ordinary release and pressure routing.
        if(padMappedNotes[pad]>=0 && latchedPadNotes[pad]<0) return false;
        const int owner=performancePart;
        if(pad<4 && !performanceLaneAvailable(owner,pad)) {
            performancePadOwners[pad]=-2;
            return true;
        }
        performancePadOwners[pad]=owner;
        setPerformanceValue(owner,pad,0.75f);
        return true;
    }
    bool performancePadOff(int pad)
    {
        if(pad<0 || pad>=8 || performancePadOwners[pad]==-1) return false;
        const int owner=performancePadOwners[pad];
        performancePadOwners[pad]=-1;
        if(owner>=0) setPerformanceValue(owner,pad,0);
        return true;
    }
    static float performancePressureAmount(int pad, int value)
    {
        const float pressure=clampValue(value/64.0f,0.0f,1.0f);
        const float curve=pressure*(2-pressure);
        const float floor=pad<4 ? 0.2f : 0.65f;
        return floor+(1-floor)*curve;
    }
    bool performancePadPressure(int pad, int value)
    {
        if(pad<0 || pad>=8 || performancePadOwners[pad]==-1) return false;
        const int owner=performancePadOwners[pad];
        if(owner>=0) setPerformanceValue(owner,pad,performancePressureAmount(pad,value));
        return true;
    }
    void capturePerformanceInitialState()
    {
        for(int part=0;part<activePartCount();++part)
            for(int index=0;index<9;++index)
                if(performanceLive[part][index]>0)
                    captureClipPoint(ClipPointType::performance,index,part,performanceLive[part][index]);
    }
    void advancePerformanceControls(std::size_t frames)
    {
        const float slew=1.0f-std::exp(-static_cast<float>(frames)/(sampleRate*0.035f));
        const float laneAttack=1.0f-std::exp(-static_cast<float>(frames)/(sampleRate*0.015f));
        for(int part=0;part<4;++part) {
            bool riding=false;
            for(int lane=0;lane<4;++lane) {
                const float value=performancePadOwners[lane]==part ? performanceLive[part][lane]
                    : clipPlaying ? performanceRecorded[part][lane] : 0.0f;
                riding=riding || (value>0 && performanceLaneAvailable(part,lane));
            }
            const float target=riding?1.0f:0.0f;
            auto& amount=performanceLaneEngagement[part];
            amount+=(target-amount)*(riding?laneAttack:slew);
            if(std::abs(target-amount)<1e-5f)amount=target;
        }
        for(int part=0;part<4;++part) for(int index=0;index<8;++index) {
            bool liveOwned=performancePadOwners[index]==part;
            const float target=liveOwned ? performanceLive[part][index]
                : clipPlaying ? performanceRecorded[part][index] : 0.0f;
            performanceSmooth[part][index]+=(target-performanceSmooth[part][index])*slew;
            if(std::abs(target-performanceSmooth[part][index])<1e-5f)
                performanceSmooth[part][index]=target;
        }
    }
    void applyPerformanceLane(Engine::Controls& effective, int part) const
    {
        const auto& values=performanceSmooth[part];
        float sum=0, weighted=0;
        for(int lane=0;lane<4;++lane) if(performanceLaneAvailable(part,lane)) {
            sum+=values[lane];weighted+=values[lane]*lane;
        }
        float cue=performanceLive[part][8];
        if(cue<=0 && clipPlaying) cue=performanceRecorded[part][8];
        effective.performanceLaneFixed=effective.laneScanPaused && cue>0 ? cue-1 : -1;
        effective.performanceLanePosition=sum>0 ? weighted/sum : 0;
        effective.performanceLaneAmount=performanceLaneEngagement[part];
    }
    void applyPerformanceInsert(Engine::Controls& target, int part) const
    {
        const auto& value=performanceSmooth[part];
        target.effectTone += (-1.0f-target.effectTone)*value[4];
        target.effectBits += (std::max(target.effectBits,0.92f)-target.effectBits)*value[5];
        target.effectRate += (std::max(target.effectRate,12.0f)-target.effectRate)*value[5];
    }
    void applyPerformanceSends(Engine::Controls& target) const
    {
        if(!splitEnabled) {
            applyPerformanceInsert(target,0);
            target.effectMix+=(std::max(target.effectMix,0.85f)-target.effectMix)*performanceSmooth[0][6];
            target.effectReverbMix+=(std::max(target.effectReverbMix,0.85f)-target.effectReverbMix)*performanceSmooth[0][7];
            return;
        }
        float* delay[4] {&target.splitDelayA,&target.splitDelayB,&target.splitDelayC,&target.splitDelayD};
        float* reverb[4] {&target.splitReverbA,&target.splitReverbB,&target.splitReverbC,&target.splitReverbD};
        for(int part=0;part<partCount();++part) {
            *delay[part]+=(1.0f-*delay[part])*performanceSmooth[part][6];
            *reverb[part]+=(1.0f-*reverb[part])*performanceSmooth[part][7];
        }
    }
    void holdCueBlend(int part, float position)
    {
        if(isQuad())return;
        const int previous=splitFocus;
        if(splitEnabled) selectSplitPart(part,false);
        controls.manualLane=clampValue(position-(splitEnabled&&!sharedDual?firstLaneForPart(part):0),
            0.0f,splitEnabled&&!sharedDual?1.0f:3.0f);
        if(clipRecording)captureClipParameter(static_cast<int>(LockParameter::manualLane));
        setStackScanPaused(true);
        setPerformanceValue(part,8,clampValue(position,0.0f,3.0f)+1);
        markPersistentStateDirty();
        if(splitEnabled) selectSplitPart(previous,false);
    }

    void setManualStackStart(float local)
    {
        const int part=splitEnabled?splitFocus:0;
        controls.manualLane=clampValue(local,0.0f,splitEnabled&&!sharedDual?(isQuad()?0.0f:1.0f):3.0f);
        const float held=performanceLive[part][8]>0 ? performanceLive[part][8]
            : clipPlaying ? performanceRecorded[part][8] : 0;
        const bool paused=stackScanPaused || (clipPlaying && (splitEnabled?splitClipControls[part].laneScanPaused:clipControls.laneScanPaused));
        if(held>0 && paused) {
            const float physical=controls.manualLane+(splitEnabled&&!sharedDual?firstLaneForPart(part):0);
            setPerformanceValue(part,8,physical+1);
        }
    }
