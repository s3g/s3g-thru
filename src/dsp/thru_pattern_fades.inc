    // The live scene stays on the audio callback. A pinned outgoing scene runs
    // ahead on a companion thread; neither its samples nor DSP histories are
    // touched by the incoming scene, UI, loader, or persistence worker.
    bool patternRenderOnly=false,patternActive=false,patternWarmup=false,patternHeld=false;
    bool patternFinishRequested=false,patternReturnRequested=false;
    int patternSourceChainBank=0,patternSourceChainPosition=0;
    static constexpr float kMaximumPatternFadeMs=30000.0f;
    float patternFadeMs=0;
    int patternTimingMode=0; // Fixed / Random; global, outside clip automation.
    float patternFadeLowMs=2000,patternFadeHighMs=12000;
    std::uint32_t patternTimingRandom=0x54494d45u;
    double patternPosition=0,patternPositionTarget=0;
    float patternGainA=1,patternGainB=0;
    bool patternStarved=false;
    std::array<float,2u> patternBridge {};
    int patternCurve=0,patternPlaybackCurve=0,patternFrom=-1,patternTo=-1;
    bool patternFadeOverridden=false;
    double patternBoundaryPosition=-1;
    std::uint64_t patternUnderflows=0;
    std::array<float,2u> patternLast {};
    std::unique_ptr<ThruInstance> patternShadow;
    pthread_t patternThread {};
    std::atomic<bool> patternRunning {false};
    // 0 idle, 1 rendering, 2 retiring. Retirement clears histories off-thread.
    std::atomic<int> patternJob {0};
    static constexpr std::size_t kPatternRingFrames=1024u;
    std::array<float,kPatternRingFrames*2u> patternRing {};
    std::atomic<std::uint64_t> patternRead {0},patternWrite {0};
    float patternDurationMs=0;

    struct PatternCommand {int type=0,value=0,source=-1,part=0;};
    std::array<PatternCommand,128u> patternCommands {};
    std::atomic<std::uint64_t> patternCommandRead{0},patternCommandWrite{0};
    void postPatternCommand(int type,int value=0,int source=-1) {
        if(patternRenderOnly || !patternActive)return;
        const auto write=patternCommandWrite.load(std::memory_order_relaxed);
        if(write-patternCommandRead.load(std::memory_order_acquire)>=patternCommands.size())return;
        patternCommands[write%patternCommands.size()]={type,value,source,midiPart};
        patternCommandWrite.store(write+1,std::memory_order_release);
    }
    void applyPatternCommands() {
        const auto write=patternCommandWrite.load(std::memory_order_acquire);
        auto read=patternCommandRead.load(std::memory_order_relaxed);
        for(;read<write;++read) {
            const auto c=patternCommands[read%patternCommands.size()];auto& s=*patternShadow;s.midiPart=c.part;
            if(c.type==0)s.noteOff(c.value,c.source);
            else if(c.type==1)s.padNoteOff(c.value);
            else if(c.type==2)s.setClipPlaying(c.value!=0);
            else if(c.type==3)s.panic();
            else if(c.type==4)s.cancelPerformanceGestures();
        }
        patternCommandRead.store(read,std::memory_order_release);
    }
    void setPatternFadeLow(float value) {
        if(!std::isfinite(value))return;
        patternFadeLowMs=std::round(clampValue(value,0.0f,kMaximumPatternFadeMs));
        patternFadeHighMs=std::max(patternFadeHighMs,patternFadeLowMs);
    }
    void setPatternFadeHigh(float value) {
        if(!std::isfinite(value))return;
        patternFadeHighMs=std::round(clampValue(value,0.0f,kMaximumPatternFadeMs));
        patternFadeLowMs=std::min(patternFadeLowMs,patternFadeHighMs);
    }
    float choosePatternDuration(const ChainFadeSettings* fade=nullptr) {
        if((fade?fade->mode:patternTimingMode)==0)return fade?fade->time:patternFadeMs;
        const auto low=static_cast<std::uint32_t>(fade?fade->low:patternFadeLowMs);
        const auto high=static_cast<std::uint32_t>(fade?fade->high:patternFadeHighMs);
        if(low==high)return static_cast<float>(low);
        auto word=patternTimingRandom?patternTimingRandom:0x54494d45u;
        word^=word<<13u;word^=word>>17u;word^=word<<5u;patternTimingRandom=word;
        // Uniform in milliseconds, independent of encoder detent spacing and
        // the playback engines' random stream. No allocation or retry loop.
        return static_cast<float>(low+((static_cast<std::uint64_t>(word)*(high-low+1u))>>32u));
    }
    bool wantsPatternFade(const ChainFadeSettings* fade=nullptr) const {
        return patternHeld || ((fade?fade->mode:patternTimingMode)==1?(fade?fade->high:patternFadeHighMs)>0:(fade?fade->time:patternFadeMs)>0);
    }
    bool patternBusy() const {return patternActive||patternJob.load(std::memory_order_acquire)!=0;}
    void resetPatternHistories() {
        auto& scene=*patternShadow;
        for(auto* buffer:{scene.effectDelayBuffer.get(),scene.rotaryDelayBuffer.get(),scene.tapeDelayBuffer.get()}) {
            const auto size=buffer==scene.effectDelayBuffer.get()?scene.effectDelayCapacity:
                buffer==scene.rotaryDelayBuffer.get()?scene.rotaryDelayCapacity:scene.tapeDelayCapacity;
            std::fill_n(buffer,size*2u,0.0f);
        }
        std::fill_n(scene.rollStorage.get(),(kRollCount+1u)*2u*scene.rollCapacityFrames,0.0f);
        std::fill_n(scene.splitRollHistory.get(),8u*scene.rollCapacityFrames,0.0f);
        scene.effectToneState={};scene.tapeBandwidthState={};scene.tapeHeadState={};
        scene.effectLoFi.prepare(sampleRate);scene.effectDrive.reset();scene.reverb.reset();
        for(auto& insert:scene.splitInserts)insert.reset();
        scene.rollHistoryWrite=scene.rollHistoryFrames=0;
        scene.splitRollWrite={};scene.splitRollFrames={};
        for(std::size_t i=0;i<scene.rolls.size();++i) {
            scene.rolls[i]=AudioRoll{};
            scene.rolls[i].left=scene.rollStorage.get()+(i+1u)*2u*scene.rollCapacityFrames;
            scene.rolls[i].right=scene.rolls[i].left+scene.rollCapacityFrames;
        }
        scene.wavesetWorker.clearBanks();scene.spectralWorker.clearBanks();
    }
    static void* patternThreadEntry(void* opaque) {
        auto& self=*static_cast<ThruInstance*>(opaque);
        // This thread supplies live audio; keep it ahead of file/analysis jobs.
        s3g_daisy::configureBackgroundThread(0);
        std::array<float,kBlockFrames*2u> block {};
        while(self.patternRunning.load(std::memory_order_acquire)) {
            const auto job=self.patternJob.load(std::memory_order_acquire);
            if(job==3) {
                self.applyPatternCommands();self.patternJob.store(4,std::memory_order_release);continue;
            }
            if(job==2) {
                self.resetPatternHistories();
                self.patternJob.store(0,std::memory_order_release);continue;
            }
            const auto write=self.patternWrite.load(std::memory_order_relaxed);
            if(job!=1 || write-self.patternRead.load(std::memory_order_acquire)>kPatternRingFrames-kBlockFrames) {
                const timespec delay{0,250000};nanosleep(&delay,nullptr);continue;
            }
            self.applyPatternCommands();
            self.patternShadow->render(nullptr,kBlockFrames,block.data());
            for(std::size_t f=0;f<kBlockFrames;++f) {
                const auto at=((write+f)&(kPatternRingFrames-1u))*2u;
                self.patternRing[at]=block[f*2u];self.patternRing[at+1]=block[f*2u+1];
            }
            self.patternWrite.store(write+kBlockFrames,std::memory_order_release);
        }
        return nullptr;
    }
    bool preparePatternFades() {
        timespec now{};
        if(clock_gettime(CLOCK_MONOTONIC,&now)==0)
            patternTimingRandom^=static_cast<std::uint32_t>(now.tv_nsec)^static_cast<std::uint32_t>(now.tv_sec);
        patternShadow.reset(new(std::nothrow) ThruInstance);
        if(!patternShadow || !patternShadow->prepare(nullptr,true))return false;
        patternRunning.store(true,std::memory_order_release);
        if(!s3g_daisy::startBackgroundThread(patternThread,patternThreadEntry,this,true)) {
            patternRunning.store(false,std::memory_order_release);return false;
        }
        return true;
    }
    void stopPatternFades() {
        if(!patternRunning.exchange(false,std::memory_order_acq_rel))return;
        pthread_join(patternThread,nullptr);
        patternActive=false;patternWarmup=false;patternHeld=false;patternFinishRequested=false;
    }
    bool beginPatternFade(int target,const ChainFadeSettings* fade=nullptr) {
        if(patternRenderOnly || !patternShadow || patternBusy() || target==activeClip)return false;
        const bool startHeld=patternHeld;
        patternReturnRequested=false;
        patternSourceChainBank=chainPlayBank;patternSourceChainPosition=chainPosition;
        // Draw only when the prepared destination is actually launched. Hold,
        // scrubbing, queries, prefetch and edits never redraw this duration.
        patternDurationMs=choosePatternDuration(fade);patternFrom=activeClip;patternTo=target;
        patternPlaybackCurve=fade?fade->curve:patternCurve;patternFadeOverridden=fade!=nullptr;
        if(patternDurationMs<=0 && !startHeld)return false;
        captureActiveClip(false);
        auto& s=*patternShadow;
        s.engine=engine;s.voices=voices;
        s.waveMinimum=waveMinimum;s.waveMaximum=waveMaximum;
        s.currentLaneSourceKinds=currentLaneSourceKinds;s.currentLaneSourcePaths=currentLaneSourcePaths;
        s.laneSourceLabels=laneSourceLabels;s.wavesetSourceRevisions=wavesetSourceRevisions;
        s.selectedLane=selectedLane;s.padMappedNotes=padMappedNotes;
        s.ignorePadRelease=ignorePadRelease;
        for(auto& voice:s.voices)for(std::size_t ch=0;ch<kChannelCount;++ch)voice.outputPointers[ch]=voice.output[ch].data();
        s.sampleRate=sampleRate;s.laneCapacityFrames=laneCapacityFrames;s.planeCapacityFrames=planeCapacityFrames;
        s.controls=controls;s.clipControls=clipControls;s.outputGain=outputGain;s.clipOutputGain=clipOutputGain;
        s.clipSlots[0]=*activeClipSlot();s.activeClip=0;s.clipPlaying=clipPlaying;s.clipNeedsStart=clipNeedsStart;
        s.clipPositionMs=clipPositionMs;s.clipAbsolutePositionMs=clipAbsolutePositionMs;
        s.tempoBpm=tempoBpm;s.tempoSwing=tempoSwing;s.transportBeats=transportBeats;
        s.renderClockSamples.store(renderClockSamples.load(std::memory_order_relaxed),std::memory_order_relaxed);
        s.rootNote=rootNote;s.padOctave=padOctave;s.padScale=padScale;s.padMode=padMode;s.pressureMode=pressureMode;
        s.stackScanPaused=stackScanPaused;s.glitchBufferLocked=glitchBufferLocked;s.noteClock=noteClock;
        s.activeNote=activeNote;s.midiPart=midiPart;s.laneVariation=laneVariation;s.variationCounter=variationCounter;
        s.lastVariationLane=lastVariationLane;s.performanceRandom=performanceRandom;s.performanceLane=performanceLane;
        s.grainVoiceLimit=grainVoiceLimit;s.polyphonyCompensation=polyphonyCompensation;s.polyphonyGain=polyphonyGain;
        s.padMappedNotes=padMappedNotes;s.latchedPadNotes=latchedPadNotes;s.latchedPadDeadlines=latchedPadDeadlines;
        s.holdReleaseMs=holdReleaseMs;s.liveStackHoldPhases=liveStackHoldPhases;s.liveStackHoldPhaseValid=liveStackHoldPhaseValid;
        s.activeRegions=activeRegions;s.laneRegionRevisions=laneRegionRevisions;
        s.splitEnabled=splitEnabled;s.sharedDual=sharedDual;s.quadMode=quadMode;s.splitFocus=splitFocus;
        s.splitInitialized=splitInitialized;s.quadInitialized=quadInitialized;
        s.splitParts=splitParts;s.splitSingleControls=splitSingleControls;s.splitSingleSettings=splitSingleSettings;
        s.splitClipControls=splitClipControls;s.splitMixerOverrides=splitMixerOverrides;
        s.splitPolyGain=splitPolyGain;s.splitBlendState=splitBlendState;s.splitCurveState=splitCurveState;
        s.splitLevelState=splitLevelState;s.splitDelayState=splitDelayState;s.splitReverbState=splitReverbState;
        s.splitRollWrite=splitRollWrite;s.splitRollFrames=splitRollFrames;
        s.splitVariationCounter=splitVariationCounter;s.splitLastVariationLane=splitLastVariationLane;
        s.performanceLive=performanceLive;s.performanceRecorded=performanceRecorded;
        s.performanceSmooth=performanceSmooth;s.performanceLaneEngagement=performanceLaneEngagement;
        s.performancePadOwners=performancePadOwners;s.performanceCaptureHeld=performanceCaptureHeld;s.performancePart=performancePart;
        std::swap(s.splitInserts,splitInserts);s.splitRollHistory.swap(splitRollHistory);
        std::swap(static_cast<PlaybackEffectsState&>(s),static_cast<PlaybackEffectsState&>(*this));
        s.wavesetWorker.exchangeBanks(wavesetWorker);s.spectralWorker.exchangeBanks(spectralWorker);
        s.clipRecording=false;s.chainEnabled=false;s.clipLaunchPending=false;s.queuedClip=-1;
        s.previewActive=false;s.previewTransitionRemaining=0;
        s.sourceTransitionTail=sourceTransitionTail;s.sourceTransitionRemaining=sourceTransitionRemaining;
        if(patternBoundaryPosition>=0 && s.clipPlaying) {
            s.clipPositionMs=patternBoundaryPosition;s.clipNeedsStart=false;
            s.processClipEvents(*s.activeClipSlot(),0,patternBoundaryPosition,true);
        }
        patternPosition=patternPositionTarget=0;patternGainA=1;patternGainB=0;patternHeld=startHeld;patternFinishRequested=false;
        patternRead.store(0,std::memory_order_relaxed);patternWrite.store(0,std::memory_order_relaxed);
        patternCommandRead.store(0,std::memory_order_relaxed);patternCommandWrite.store(0,std::memory_order_relaxed);
        patternStarved=false;patternBridge={};patternLast={};patternActive=true;patternWarmup=true;sourceTransitionRemaining=0;
        return true;
    }
    void returnPatternSource() {
        if(!patternActive){cancelChainLaunch();patternHeld=false;return;}

        patternReturnRequested=true;patternHeld=true;patternFinishRequested=false;patternPositionTarget=0;
        chainLaunchBank=chainLaunchSlot=-1;chainNextSlot=-1;queuedClip=-1;
    }
    void servicePatternReturn() {
        if(!patternReturnRequested || patternJob.load(std::memory_order_acquire)!=4
            || patternRead.load(std::memory_order_relaxed)!=patternWrite.load(std::memory_order_acquire))return;
        // The worker has stopped and every queued source frame has played.
        // Its DSP state now corresponds exactly to the next audible sample.
        captureActiveClip(false);
        applyPatternCommands();
        auto& s=*patternShadow;
        storage.swap(preparedClipStorage);
        std::swap(engine,s.engine);std::swap(voices,s.voices);
        for(auto& voice:voices)for(std::size_t ch=0;ch<kChannelCount;++ch)voice.outputPointers[ch]=voice.output[ch].data();
        waveMinimum=s.waveMinimum;
        waveMaximum=s.waveMaximum;
        currentLaneSourceKinds=s.currentLaneSourceKinds;
        currentLaneSourcePaths=s.currentLaneSourcePaths;
        laneSourceLabels=s.laneSourceLabels;
        wavesetSourceRevisions=s.wavesetSourceRevisions;
        selectedLane=s.selectedLane;
        padMappedNotes=s.padMappedNotes;
        ignorePadRelease=s.ignorePadRelease;
        controls=s.controls;
        clipControls=s.clipControls;
        outputGain=s.outputGain;
        clipOutputGain=s.clipOutputGain;
        clipPlaying=s.clipPlaying;
        clipNeedsStart=s.clipNeedsStart;
        clipPositionMs=s.clipPositionMs;
        clipAbsolutePositionMs=s.clipAbsolutePositionMs;
        tempoBpm=s.tempoBpm;
        tempoSwing=s.tempoSwing;
        transportBeats=s.transportBeats;
        rootNote=s.rootNote;
        padOctave=s.padOctave;
        padScale=s.padScale;
        padMode=s.padMode;
        pressureMode=s.pressureMode;
        stackScanPaused=s.stackScanPaused;
        glitchBufferLocked=s.glitchBufferLocked;
        noteClock=s.noteClock;
        activeNote=s.activeNote;
        midiPart=s.midiPart;
        laneVariation=s.laneVariation;
        variationCounter=s.variationCounter;
        lastVariationLane=s.lastVariationLane;
        performanceRandom=s.performanceRandom;
        performanceLane=s.performanceLane;
        grainVoiceLimit=s.grainVoiceLimit;
        polyphonyCompensation=s.polyphonyCompensation;
        polyphonyGain=s.polyphonyGain;
        latchedPadNotes=s.latchedPadNotes;
        latchedPadDeadlines=s.latchedPadDeadlines;
        holdReleaseMs=s.holdReleaseMs;
        liveStackHoldPhases=s.liveStackHoldPhases;
        liveStackHoldPhaseValid=s.liveStackHoldPhaseValid;
        activeRegions=s.activeRegions;
        laneRegionRevisions=s.laneRegionRevisions;
        splitEnabled=s.splitEnabled;
        sharedDual=s.sharedDual;
        quadMode=s.quadMode;
        splitFocus=s.splitFocus;
        splitInitialized=s.splitInitialized;
        quadInitialized=s.quadInitialized;
        splitParts=s.splitParts;
        splitSingleControls=s.splitSingleControls;
        splitSingleSettings=s.splitSingleSettings;
        splitClipControls=s.splitClipControls;
        splitMixerOverrides=s.splitMixerOverrides;
        splitPolyGain=s.splitPolyGain;
        splitBlendState=s.splitBlendState;
        splitCurveState=s.splitCurveState;
        splitLevelState=s.splitLevelState;
        splitDelayState=s.splitDelayState;
        splitReverbState=s.splitReverbState;
        splitRollWrite=s.splitRollWrite;
        splitRollFrames=s.splitRollFrames;
        splitVariationCounter=s.splitVariationCounter;
        splitLastVariationLane=s.splitLastVariationLane;
        performanceLive=s.performanceLive;
        performanceRecorded=s.performanceRecorded;
        performanceSmooth=s.performanceSmooth;
        performanceLaneEngagement=s.performanceLaneEngagement;
        performancePadOwners=s.performancePadOwners;
        performanceCaptureHeld=s.performanceCaptureHeld;
        performancePart=s.performancePart;
        sourceTransitionTail=s.sourceTransitionTail;
        sourceTransitionRemaining=s.sourceTransitionRemaining;
        std::swap(splitInserts,s.splitInserts);splitRollHistory.swap(s.splitRollHistory);
        std::swap(static_cast<PlaybackEffectsState&>(*this),static_cast<PlaybackEffectsState&>(s));
        wavesetWorker.exchangeBanks(s.wavesetWorker);spectralWorker.exchangeBanks(s.spectralWorker);
        activeClip=patternFrom;activeClipOccupied=clipSlots[static_cast<std::size_t>(activeClip)].occupied;
        chainPlayBank=patternSourceChainBank;chainPosition=patternSourceChainPosition;
        chainEnabled=false;chainRepeat=0;chainAdvanceRequested=false;chainLaunchingFirst=false;
        pendingClipSourceMask=0;pendingClearMask=0;waveDirtyMask=0;waveTextDirty=true;
        patternActive=false;patternHeld=false;patternReturnRequested=false;patternFinishRequested=false;
        patternWarmup=false;patternGainA=1;patternGainB=0;
        patternJob.store(2,std::memory_order_release);markPersistentStateDirty();
    }
    static std::array<float,2> patternGains(float position,int curve) {
        float u=clampValue(position,0.0f,1.0f);
        if(curve==1)u*=u;
        else if(curve==2)u=1-(1-u)*(1-u);
        else if(curve==3)u=u*u*(3-2*u);
        if(curve==4)return {std::sqrt(1-u),std::sqrt(u)};
        return {1-u,u};
    }
    int appendPatternFadeJson(char* buffer,int capacity,int length) const {
        if(length<1 || length>=capacity || buffer[length-1]!='}')return -1;
        const int n=std::snprintf(buffer+length-1,capacity-length+1,
            ",\"pattern_fade_ms\":%.0f,\"pattern_fade_curve\":%d,\"pattern_fade_mode\":%d,\"pattern_fade_low_ms\":%.0f,\"pattern_fade_high_ms\":%.0f}",
            patternFadeMs,patternCurve,patternTimingMode,patternFadeLowMs,patternFadeHighMs);
        return n<0 || n>=capacity-length+1 ? -1 : length-1+n;
    }
    void setPatternHeld(bool held) {
        if(patternReturnRequested)return;

        // Holding while idle arms a manual handoff, including TIME=CUT.
        // Repeated touches must not discard an encoder target still slewing.
        if(!held || !patternHeld || patternFinishRequested)patternPositionTarget=patternPosition;
        patternHeld=held;patternFinishRequested=false;
    }
    void setPatternPosition(float position) {
        if(!patternActive || patternReturnRequested)return;

        patternPositionTarget=clampValue(position,0.0f,1.0f);
        patternHeld=true;patternFinishRequested=false;
    }
    void nudgePatternPosition(float percent) {
        if(!std::isfinite(percent))return;
        setPatternHeld(true);
        if(patternActive)setPatternPosition((patternPositionTarget*100.0f+percent)/100.0f);
    }
    void finishPatternFade() {
        if(patternReturnRequested)return;

        if(!patternActive){patternHeld=false;patternFinishRequested=false;return;}
        patternPositionTarget=1;patternHeld=true;patternFinishRequested=true;
    }
    void mixPatternFade(std::size_t frames) {
        if(!patternActive || patternWarmup)return;
        auto read=patternRead.load(std::memory_order_relaxed);
        const auto write=patternWrite.load(std::memory_order_acquire);
        const auto available=std::min<std::uint64_t>(frames,write-read);
        if(available<frames)++patternUnderflows;
        const float slew=1-std::exp(-1/(sampleRate*.005f));
        for(std::size_t f=0;f<frames;++f) {
            if(f<available) {
                const auto at=((read+f)&(kPatternRingFrames-1u))*2u;
                if(patternStarved) {
                    patternBridge={patternLast[0]-patternRing[at],patternLast[1]-patternRing[at+1]};
                    patternStarved=false;
                }
                for(std::size_t ch=0;ch<2;++ch) {
                    patternLast[ch]=patternRing[at+ch]+patternBridge[ch];patternBridge[ch]*=1-slew;
                }
            } else {for(auto& value:patternLast)value*=1-slew;patternStarved=true;}
            // The audio clock and manual/DONE controls belong to the live
            // scene. A late outgoing bus must never pin its gain at 100%.
            if(patternHeld)patternPosition+=(patternPositionTarget-patternPosition)*slew;
            else patternPosition=std::min(1.0,patternPosition+1000.0/(static_cast<double>(sampleRate)*std::max(1.0f,patternDurationMs)));
            if(patternHeld && std::abs(patternPositionTarget-patternPosition)<1e-5f)patternPosition=patternPositionTarget;
            const auto gains=patternGains(patternPosition,patternPlaybackCurve);
            patternGainA+=(gains[0]-patternGainA)*slew;patternGainB+=(gains[1]-patternGainB)*slew;
            // An indefinitely parked endpoint must not leave subnormal gains.
            if(gains[0]==0 && patternGainA<1e-12f)patternGainA=0;
            if(gains[1]==0 && patternGainB<1e-12f)patternGainB=0;
            for(std::size_t ch=0;ch<2;++ch)
                voiceMix[ch][f]=voiceMix[ch][f]*patternGainB+patternLast[ch]*patternGainA;
        }
        patternRead.store(read+available,std::memory_order_release);
        if(patternReturnRequested && patternPosition<=1e-5 && patternGainB<1e-5f && patternGainA>.99999f
            && patternJob.load(std::memory_order_acquire)==1)patternJob.store(3,std::memory_order_release);
        if(!patternReturnRequested && (!patternHeld || patternFinishRequested) && patternPosition>=1 && patternGainA<1e-5f && patternGainB>.99999f) {
            patternActive=false;patternHeld=false;patternFinishRequested=false;patternJob.store(2,std::memory_order_release);
        }
    }
    void renderPatternWarmup(int16_t* output,float* raw,int rendered,std::size_t count) {
        std::array<float,kBlockFrames*2u> old {};
        applyPatternCommands();
        patternShadow->render(nullptr,static_cast<int>(count),old.data());
        for(std::size_t f=0;f<count;++f) {
            const auto at=(static_cast<std::size_t>(rendered)+f)*2u;
            if(raw) {raw[at]=old[f*2u];raw[at+1]=old[f*2u+1];continue;}
            float left,right;processProgramBus(old[f*2u],old[f*2u+1],left,right);
            monitorGainState+=(monitorLevel-monitorGainState)*gainControlSlew;
            left+=monitorInput[0][f]*monitorGainState;right+=monitorInput[1][f]*monitorGainState;
            processOutputLimiter(left,right,left,right);
            output[at]=toInt16(left,ditherState[0]);output[at+1]=toInt16(right,ditherState[1]);
        }
        patternLast={old[(count-1u)*2u],old[(count-1u)*2u+1u]};
        renderClockSamples.fetch_add(count,std::memory_order_relaxed);
        patternWarmup=false;patternJob.store(1,std::memory_order_release);
    }
    int formatPatternFade(char* buffer,int capacity) const {
        return std::snprintf(buffer,capacity,"%d,%d,%d,%d,%d,%d,%d,%llu,%d,%.0f,%.0f,%.0f,%d",patternActive?1:0,patternHeld?1:0,
            patternFrom,patternTo,static_cast<int>(std::lround(patternPosition*10000)),
            static_cast<int>(std::lround(patternFadeMs)),patternActive?patternPlaybackCurve:patternCurve,static_cast<unsigned long long>(patternUnderflows),
            patternTimingMode,patternFadeLowMs,patternFadeHighMs,patternDurationMs,patternActive&&patternFadeOverridden?1:0);
    }
