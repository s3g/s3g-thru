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
    std::uint64_t patternUnderflows=0,patternStartUnderflows=0,patternMissingFrames=0;
    std::atomic<bool> patternWorkerMeasure{false},patternWorkerCpuAvailable{false};
    std::atomic<int> patternWorkerFlush{-1},patternWorkerNice{99};
    std::atomic<std::uint64_t> patternWorkerWallPeak{0},patternWorkerCpuPeak{0},patternMaximumQueued{0},patternWorkerWakePeak{0},patternWakeStamp{0};
    std::array<float,2u> patternLast {};
    std::unique_ptr<ThruInstance> patternShadow;
    pthread_t patternThread {};
    s3g_daisy::ThruThreadSignal patternSignal;
    std::atomic<bool> patternRunning {false};
    // 0 idle, 1 rendering, 2 retiring. Retirement clears histories off-thread.
    std::atomic<int> patternJob {0};
    // A 46 ms outgoing-only reserve tolerates scheduler stalls without adding
    // latency to the incoming instrument or normal pad playback.
    static constexpr std::size_t kPatternRingFrames=2048u;
    static_assert((kPatternRingFrames&(kPatternRingFrames-1))==0);
    std::array<float,kPatternRingFrames*2u> patternRing {};
    std::atomic<std::uint64_t> patternRead {0},patternWrite {0};
    float patternDurationMs=0;
    int patternLookahead=1;
    s3g_daisy::ThruFadeLookahead fadeLookahead;
    std::size_t patternLookaheadFrames=0;
    static_assert(kPatternRingFrames>=s3g_daisy::ThruFadeLookahead::maximumFrames+2*kBlockFrames);
    void processLiveLookahead(std::size_t frames) {
        for(std::size_t f=0;f<frames;++f){
            const auto delayed=fadeLookahead.process({voiceMix[0][f],voiceMix[1][f]});
            voiceMix[0][f]=delayed[0];voiceMix[1][f]=delayed[1];
        }
    }
    void setPatternLookahead(int value) {
        if(value==patternLookahead)return;
        if(patternBusy()){setError("Finish fade before changing lookahead");return;}
        patternLookahead=value;fadeLookahead.setEnabled(value!=0,renderClockSamples.load(std::memory_order_relaxed)==0);markPersistentStateDirty();
    }
    int patternDeferredClip=-1,patternDeferredSource=-1;
    // Without lookahead, render a single source prefix before destination
    // activation. With lookahead, use the live delay's already-rendered pending
    // output for a real 12 ms lead, without another callback source render.
    std::array<float,s3g_daisy::ThruFadeLookahead::maximumFrames*2u> patternWarmupAudio {};
    std::size_t patternWarmupFrames=kBlockFrames,patternWarmupOffset=kBlockFrames;
#ifdef S3G_THRU_FADE_TESTING
    void (*patternHandoffTestReady)(ThruInstance&)=nullptr;
    std::atomic<bool> patternWorkerTestBlocked{false};
#endif

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
            else if(c.type==7)s.tempoBpm=c.value*.001f;
            else if(c.type==8)s.releaseMidiInput(c.value);
            else if(c.type==6){s.releaseExternalNotes();if(c.value)s.midiPauseClip();}
            else if(c.type==5){const std::uint8_t bytes[]{static_cast<std::uint8_t>(c.value),static_cast<std::uint8_t>(c.value>>8),static_cast<std::uint8_t>(c.value>>16)};s.handleNoteMidi(bytes,3,c.source);}
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
        for(auto& voice:scene.voices){scene.releaseBankVoice(voice);voice.active=false;}
        scene.releaseBankIndices(scene.bankAssets);scene.bankLoadedClip=-1;
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
        scene.layersRack.reset();scene.clearLayersTouches();
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
        // Retain SCHED_OTHER, off core 3 and below every host realtime task.
        // Give live audio more CFS weight than UI/file jobs; record the actual
        // niceness because a host may deny the negative-nice request.
        s3g_daisy::configureBackgroundThread(-10);
        self.patternWorkerNice.store(s3g_daisy::currentBackgroundNiceness(),std::memory_order_release);
        self.patternWorkerFlush.store(s3g_daisy::enableAudioDenormalFlush()?1:0,std::memory_order_release);
        std::array<float,kBlockFrames*2u> block {};
        while(self.patternRunning.load(std::memory_order_acquire)) {
            const auto job=self.patternJob.load(std::memory_order_acquire);
            if(job==3) {
                // Return with one complete live-delay prefix still queued.
                // Two extra blocks cover a callback already in progress when
                // pause is acknowledged. Keep producing until that reserve is
                // ready instead of parking with an incomplete prefix.
                const auto queued=self.patternWrite.load(std::memory_order_relaxed)-self.patternRead.load(std::memory_order_acquire);
                if(!self.patternLookaheadFrames || queued>=self.patternLookaheadFrames+2*kBlockFrames){
                    self.applyPatternCommands();self.patternJob.store(4,std::memory_order_release);continue;
                }
            }
            if(job==2) {
                self.resetPatternHistories();
                self.patternJob.store(0,std::memory_order_release);continue;
            }
#ifdef S3G_THRU_FADE_TESTING
            if(self.patternWorkerTestBlocked.load(std::memory_order_acquire)) {self.patternSignal.wait();continue;}
#endif
            const auto write=self.patternWrite.load(std::memory_order_relaxed);
            if((job!=1 && job!=3) || write-self.patternRead.load(std::memory_order_acquire)>kPatternRingFrames-kBlockFrames) {
                self.patternSignal.wait();continue;
            }
            self.applyPatternCommands();
            const bool measure=self.patternWorkerMeasure.load(std::memory_order_acquire);
            timespec wallBefore{},cpuBefore{},wallAfter{},cpuAfter{};
            if(measure){
                clock_gettime(CLOCK_MONOTONIC,&wallBefore);
                if(write==0){
                    const auto published=self.patternWakeStamp.load(std::memory_order_acquire);
                    const auto now=std::uint64_t(wallBefore.tv_sec)*1000000000u+std::uint64_t(wallBefore.tv_nsec);
                    if(published&&now>=published)recordMaximum(self.patternWorkerWakePeak,now-published);
                }
            }
            const bool cpuStarted=measure&&readRenderCpuClock(cpuBefore);
            self.patternShadow->render(nullptr,kBlockFrames,block.data());
            if(measure){
                const bool cpuAvailable=cpuStarted&&readRenderCpuClock(cpuAfter);
                clock_gettime(CLOCK_MONOTONIC,&wallAfter);
                recordMaximum(self.patternWorkerWallPeak,elapsedNanos(wallBefore,wallAfter));
                self.patternWorkerCpuAvailable.store(cpuAvailable,std::memory_order_relaxed);
                if(cpuAvailable)recordMaximum(self.patternWorkerCpuPeak,elapsedNanos(cpuBefore,cpuAfter));
            }
            for(std::size_t f=0;f<kBlockFrames;++f) {
                const auto at=((write+f)&(kPatternRingFrames-1u))*2u;
                self.patternRing[at]=block[f*2u];self.patternRing[at+1]=block[f*2u+1];
            }
            self.patternWrite.store(write+kBlockFrames,std::memory_order_release);
            const auto queued=write+kBlockFrames-self.patternRead.load(std::memory_order_acquire);
            recordMaximum(self.patternMaximumQueued,std::min<std::uint64_t>(queued,kPatternRingFrames));
        }
        return nullptr;
    }
    bool preparePatternFades() {
        timespec now{};
        if(clock_gettime(CLOCK_MONOTONIC,&now)==0)
            patternTimingRandom^=static_cast<std::uint32_t>(now.tv_nsec)^static_cast<std::uint32_t>(now.tv_sec);
        patternShadow.reset(new(std::nothrow) ThruInstance);
        if(!patternShadow || !patternShadow->prepare(nullptr,true))return false;
        patternShadow->bankPool=bankPool;
        // Prepare the spare bank once, off the callback. A bank transferred to
        // the main scene must already own its window tables and coefficients.
        for(auto& voice:patternShadow->voices) {
            if(!voice.engine.prepare(sampleRate,storage.get(),storage.get()+planeCapacityFrames,
                storage.get()+planeCapacityFrames*2u,storage.get()+planeCapacityFrames*3u,
                planeCapacityFrames))return false;
            voice.engine.stopPlayback();
        }
        if(!patternSignal.prepare())return false;
        patternRunning.store(true,std::memory_order_release);
        if(!s3g_daisy::startBackgroundThread(patternThread,patternThreadEntry,this,true)) {
            patternRunning.store(false,std::memory_order_release);return false;
        }
        return true;
    }
    void stopPatternFades() {
        if(!patternRunning.exchange(false,std::memory_order_acq_rel))return;
        patternSignal.notify();
        pthread_join(patternThread,nullptr);
        patternActive=false;patternWarmup=false;patternHeld=false;patternFinishRequested=false;
    }
    static void copyPatternClip(ClipSlot& destination,const ClipSlot& source) {
        static_assert(std::is_trivially_copyable_v<ClipSlot> && std::is_standard_layout_v<ClipSlot>);
        // Preserve all metadata (including future fields), but leave unused
        // note/automation capacity untouched. Only counted entries are read.
        constexpr auto header=offsetof(ClipSlot,clipNotes);
        constexpr auto tail=offsetof(ClipSlot,clipPointStarts);
        std::memcpy(reinterpret_cast<unsigned char*>(&destination),
                    reinterpret_cast<const unsigned char*>(&source),header);
        std::memcpy(reinterpret_cast<unsigned char*>(&destination)+tail,
                    reinterpret_cast<const unsigned char*>(&source)+tail,sizeof(ClipSlot)-tail);
        destination.clipNoteCount=std::min<std::size_t>(source.clipNoteCount,kClipNoteCapacity);
        destination.clipPointCount=std::min<std::size_t>(source.clipPointCount,kClipPointCapacity);
        std::copy_n(source.clipNotes.begin(),destination.clipNoteCount,destination.clipNotes.begin());
        std::copy_n(source.clipPoints.begin(),destination.clipPointCount,destination.clipPoints.begin());
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
        s.releaseBankIndices(s.bankAssets);
        for(auto& voice:s.voices)s.releaseBankVoice(voice);
        s.bankMasterRate=bankMasterRate;s.bankRecordSource=bankRecordSource;
        s.bankMode=bankMode;s.bankSlots=bankSlots;s.bankAssets=bankAssets;s.bankLoadedClip=0;
        s.bankSelected=bankSelected;s.voiceMode=voiceMode;s.heldKeys=heldKeys;
        s.midiKeys=midiKeys;s.midiKeyOwner=midiKeyOwner;s.midiChannels=midiChannels;s.midiPartExpression=midiPartExpression;
        s.midiNoteRoute=midiNoteRoute;s.midiNoteChannel=midiNoteChannel;s.midiBendRange=midiBendRange;s.midiModTarget=midiModTarget;s.midiVelocityDepth=midiVelocityDepth;
        s.midiSyncSource=0;
        for(int index:s.bankAssets)if(index>=0)bankPool->assets[index].pins.fetch_add(1,std::memory_order_acq_rel);
        s.engine=engine;voices.swap(s.voices);s.stochasticPartMeters=stochasticPartMeters;
        // Voice asset pins travel with their owning bank. Do not duplicate
        // them: only the scene-level bankAssets references above are shared.
        s.waveMinimum=waveMinimum;s.waveMaximum=waveMaximum;
        s.currentLaneSourceKinds=currentLaneSourceKinds;s.currentLaneSourcePaths=currentLaneSourcePaths;
        s.laneSourceLabels=laneSourceLabels;s.wavesetSourceRevisions=wavesetSourceRevisions;
        s.selectedLane=selectedLane;s.padMappedNotes=padMappedNotes;
        s.ignorePadRelease=ignorePadRelease;
        for(auto& voice:s.voices)for(std::size_t ch=0;ch<kChannelCount;++ch)voice.outputPointers[ch]=voice.output[ch].data();
        s.sampleRate=sampleRate;s.laneCapacityFrames=laneCapacityFrames;s.planeCapacityFrames=planeCapacityFrames;
        s.controls=controls;s.clipControls=clipControls;s.outputGain=outputGain;s.clipOutputGain=clipOutputGain;
        copyLayersRuntimeTo(s);copySequenceRuntimeTo(s);
        copyPatternClip(s.clipSlots[0],*activeClipSlot());s.activeClip=0;s.clipPlaying=clipPlaying;s.clipNeedsStart=clipNeedsStart;
        s.clipPositionMs=clipPositionMs;s.clipAbsolutePositionMs=clipAbsolutePositionMs;
        s.pulsarClipCycle=pulsarClipCycle;
        s.tempoBpm=tempoBpm;s.tempoSwing=tempoSwing;s.transportBeats=transportBeats;
        s.renderClockSamples.store(renderClockSamples.load(std::memory_order_relaxed),std::memory_order_relaxed);
        s.rootNote=rootNote;s.padOctave=padOctave;s.padScale=padScale;s.padMode=padMode;s.pressureMode=pressureMode;
        s.stackScanPaused=stackScanPaused;s.glitchBufferLocked=glitchBufferLocked;s.noteClock=noteClock;
        s.activeNote=activeNote;s.midiPart=midiPart;s.laneVariation=laneVariation;s.variationCounter=variationCounter;
        s.lastVariationLane=lastVariationLane;s.performanceRandom=performanceRandom;s.performanceLane=performanceLane;
        s.grainVoiceLimit=grainVoiceLimit;s.polyphonyCompensation=polyphonyCompensation;s.polyphonyGain=polyphonyGain;
        s.padMappedNotes=padMappedNotes;s.latchedPadNotes=latchedPadNotes;s.latchedPadDeadlines=latchedPadDeadlines;
        s.holdReleaseMs=holdReleaseMs;s.liveStackHoldPhases=liveStackHoldPhases;s.liveStackHoldPhaseValid=liveStackHoldPhaseValid;
        s.fileSliceMaps=fileSliceMaps;s.activeRegions=activeRegions;s.laneRegionRevisions=laneRegionRevisions;
        s.splitEnabled=splitEnabled;s.sharedDual=sharedDual;s.quadMode=quadMode;s.splitFocus=splitFocus;s.voiceEngineSettings=voiceEngineSettings;s.voiceEngineRandom=voiceEngineRandom;s.voiceEngineTraversal=voiceEngineTraversal;
        s.splitInitialized=splitInitialized;s.quadInitialized=quadInitialized;
        s.splitParts=splitParts;s.splitSingleControls=splitSingleControls;s.splitSingleSettings=splitSingleSettings;
        s.splitLanePreferences=preferredSplitLanes();s.splitLaneApplied=splitLaneApplied;
        s.splitClipControls=splitClipControls;s.splitMixerOverrides=splitMixerOverrides;
        s.splitPolyGain=splitPolyGain;s.splitBlendState=splitBlendState;s.splitCurveState=splitCurveState;
        s.splitLevelState=splitLevelState;s.splitDelayState=splitDelayState;s.splitReverbState=splitReverbState;
        s.splitRollWrite=splitRollWrite;s.splitRollFrames=splitRollFrames;
        s.splitVariationCounter=splitVariationCounter;s.splitLastVariationLane=splitLastVariationLane;
        s.performanceLive=performanceLive;s.performanceRecorded=performanceRecorded;
        s.performanceSmooth=performanceSmooth;s.performanceLaneEngagement=performanceLaneEngagement;
        s.performancePadOwners=performancePadOwners;s.performanceCaptureHeld=performanceCaptureHeld;s.performancePart=performancePart;
        layersRack.swapWith(s.layersRack);
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
        patternWakeStamp.store(0,std::memory_order_relaxed);
        patternCommandRead.store(0,std::memory_order_relaxed);patternCommandWrite.store(0,std::memory_order_relaxed);
        patternStarved=false;patternBridge={};patternLast={};patternActive=true;patternWarmup=true;
        patternWarmupFrames=kBlockFrames;patternWarmupOffset=kBlockFrames;
        patternLookaheadFrames=fadeLookahead.active()?fadeLookahead.frames():0;
        if(patternLookaheadFrames){
            // This is already-rendered future output, not repeated history.
            // Keep playing it while both incoming and outgoing scenes run.
            patternWarmupFrames=patternLookaheadFrames;patternWarmupOffset=0;
            fadeLookahead.copyPending(patternWarmupAudio.data());fadeLookahead.clear();
        }
        sourceTransitionRemaining=0;
        // A lookahead prefix is already complete. Publish the worker here as
        // well as through prepared activation, so direct handoffs do not wait
        // for the legacy synchronous warmup path. No child state changes after
        // this publication; primePatternWarmup is idempotent for the caller.
        if(patternLookaheadFrames)primePatternWarmup();
        return true;
    }
    void returnPatternSource() {
        if(!patternActive){cancelChainLaunch();patternHeld=false;return;}

        patternReturnRequested=true;patternHeld=true;patternFinishRequested=false;patternPositionTarget=0;
        chainLaunchBank=chainLaunchSlot=-1;chainNextSlot=-1;queuedClip=-1;
    }
    void servicePatternReturn() {
        if(!patternReturnRequested || patternJob.load(std::memory_order_acquire)!=4 || patternWarmup)return;
        const auto read=patternRead.load(std::memory_order_relaxed),write=patternWrite.load(std::memory_order_acquire);
        if(write-read!=patternLookaheadFrames)return;
        // The worker is stopped. Move its final pending audio into the live
        // delay, so its DSP state stays exactly one delay ahead of audibility.
        if(patternLookaheadFrames){
            fadeLookahead.seedPending([&](std::size_t f){
                const auto at=((read+f)&(kPatternRingFrames-1))*2;
                return std::array<float,2>{patternRing[at],patternRing[at+1]};
            });
            patternRead.store(write,std::memory_order_release);
        }
        captureActiveClip(false);
        applyPatternCommands();
        auto& s=*patternShadow;
        bankMasterRate=s.bankMasterRate;bankRecordSource=s.bankRecordSource;
        bankMode=s.bankMode;bankSlots=s.bankSlots;bankAssets.swap(s.bankAssets);
        bankLoadedClip=patternFrom;bankSelected=s.bankSelected;voiceMode=s.voiceMode;heldKeys=s.heldKeys;
        storage.swap(preparedClipStorage);
        std::swap(engine,s.engine);std::swap(voices,s.voices);std::swap(stochasticPartMeters,s.stochasticPartMeters);
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
        s.copyLayersRuntimeTo(*this);s.copySequenceRuntimeTo(*this);
        controls=s.controls;
        clipControls=s.clipControls;
        outputGain=s.outputGain;
        clipOutputGain=s.clipOutputGain;
        clipPlaying=s.clipPlaying;
        clipNeedsStart=s.clipNeedsStart;
        clipPositionMs=s.clipPositionMs;
        clipAbsolutePositionMs=s.clipAbsolutePositionMs;
        pulsarClipCycle=s.pulsarClipCycle;
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
        midiKeys=s.midiKeys;midiKeyOwner=s.midiKeyOwner;midiChannels=s.midiChannels;midiPartExpression=s.midiPartExpression;
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
        fileSliceMaps=s.fileSliceMaps;activeRegions=s.activeRegions;
        laneRegionRevisions=s.laneRegionRevisions;
        splitEnabled=s.splitEnabled;
        sharedDual=s.sharedDual;
        quadMode=s.quadMode;
        splitFocus=s.splitFocus;
        voiceEngineSettings=s.voiceEngineSettings;
        voiceEngineRandom=s.voiceEngineRandom;
        voiceEngineTraversal=s.voiceEngineTraversal;
        splitInitialized=s.splitInitialized;
        quadInitialized=s.quadInitialized;
        splitParts=s.splitParts;
        splitLanePreferences=s.splitLanePreferences;splitLaneApplied=s.splitLaneApplied;
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
        layersRack.swapWith(s.layersRack);
        std::swap(splitInserts,s.splitInserts);splitRollHistory.swap(s.splitRollHistory);
        std::swap(static_cast<PlaybackEffectsState&>(*this),static_cast<PlaybackEffectsState&>(s));
        wavesetWorker.exchangeBanks(s.wavesetWorker);spectralWorker.exchangeBanks(s.spectralWorker);
        activeClip=patternFrom;activeClipOccupied=clipSlots[static_cast<std::size_t>(activeClip)].occupied;
        chainPlayBank=patternSourceChainBank;chainPosition=patternSourceChainPosition;
        chainEnabled=false;chainRepeat=0;chainAdvanceRequested=false;chainLaunchingFirst=false;
        pendingClipSourceMask=0;pendingClearMask=0;waveDirtyMask=0;waveTextDirty=true;
        patternActive=false;patternHeld=false;patternReturnRequested=false;patternFinishRequested=false;
        patternWarmup=false;patternGainA=1;patternGainB=0;
        patternJob.store(2,std::memory_order_release);patternSignal.notify();markPersistentStateDirty();
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
        PersistentStateSnapshot snapshot; snapshot.capture(*this);
        return snapshot.appendPatternFadeJson(buffer,capacity,length);
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
#ifdef S3G_THRU_FADE_TESTING
    // Deterministically publish a producer block between consumer frames.
    // Compiled out of the device module and ordinary native core tests.
    void (*patternMixTestPublication)(ThruInstance&,std::size_t)=nullptr;
#endif
    std::size_t patternReturnRenderFrames(std::size_t frames) const {
        if(!patternActive || !patternReturnRequested || patternWarmup || !patternLookaheadFrames
            || patternJob.load(std::memory_order_acquire)!=4)return frames;
        const auto available=patternWrite.load(std::memory_order_acquire)-patternRead.load(std::memory_order_relaxed);
        return available>patternLookaheadFrames?std::min<std::uint64_t>(frames,available-patternLookaheadFrames):frames;
    }
    void mixPatternFade(std::size_t frames) {
        if(!patternActive || (patternWarmup&&!patternLookaheadFrames))return;
        const auto read=patternRead.load(std::memory_order_relaxed);
        auto write=patternWrite.load(std::memory_order_acquire);
        std::size_t consumed=0,missing=0;
        const bool canFinish=!patternReturnRequested&&(!patternHeld||patternFinishRequested);
        bool completed=false;
        const float slew=1-std::exp(-1/(sampleRate*.005f));
        for(std::size_t f=0;f<frames;++f) {
#ifdef S3G_THRU_FADE_TESTING
            if(patternMixTestPublication)patternMixTestPublication(*this,f);
#endif
            if(patternWarmup){
                // Incoming audio is filling its own delay during this prefix.
                // Begin the fade on the exact first available incoming sample,
                // including fades shorter than the lookahead duration.
                const auto at=patternWarmupOffset*2;
                patternLast={patternWarmupAudio[at],patternWarmupAudio[at+1]};
                voiceMix[0][f]=patternLast[0];voiceMix[1][f]=patternLast[1];
                if(++patternWarmupOffset==patternWarmupFrames)patternWarmup=false;
                continue;
            }
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
            if(canFinish&&patternPosition>=1&&patternGainA<1e-5f&&patternGainB>.99999f){
                // The incoming bus is already rendered. Leave this frame and
                // the rest of the chunk untouched; outgoing audio is no longer
                // required and must not be requested or counted as missing.
                completed=true;break;
            }
            const auto cursor=read+consumed;
            // Check again exactly when the cached prefix is exhausted. A
            // producer block published during mixing is usable immediately;
            // this is a bounded load, never a wait or a retry loop.
            if(cursor==write)write=patternWrite.load(std::memory_order_acquire);
            if(cursor<write) {
                const auto at=(cursor&(kPatternRingFrames-1u))*2u;
                if(patternStarved) {
                    patternBridge={patternLast[0]-patternRing[at],patternLast[1]-patternRing[at+1]};
                    patternStarved=false;
                }
                for(std::size_t ch=0;ch<2;++ch) {
                    patternLast[ch]=patternRing[at+ch]+patternBridge[ch];patternBridge[ch]*=1-slew;
                }
                ++consumed;
            } else {
                ++missing;for(auto& value:patternLast)value*=1-slew;patternStarved=true;
            }
            for(std::size_t ch=0;ch<2;++ch)
                voiceMix[ch][f]=voiceMix[ch][f]*patternGainB+patternLast[ch]*patternGainA;
        }
        // Count only source frames that were actually unavailable when used.
        // Data arriving during a chunk and an already completed fade are not
        // underruns. Genuine starvation still receives the same bridge.
        if(missing){++patternUnderflows;if(read==0)++patternStartUnderflows;patternMissingFrames+=missing;}
        const auto nextRead=read+consumed;
        patternRead.store(nextRead,std::memory_order_release);
        if(read/kBlockFrames!=nextRead/kBlockFrames)patternSignal.notify();
        if(patternReturnRequested && patternPosition<=1e-5 && patternGainB<1e-5f && patternGainA>.99999f
            && patternJob.load(std::memory_order_acquire)==1){patternJob.store(3,std::memory_order_release);patternSignal.notify();}
        if(completed) {
            patternGainA=0;patternGainB=1;
            patternActive=false;patternHeld=false;patternFinishRequested=false;patternJob.store(2,std::memory_order_release);patternSignal.notify();
        }
    }
    void serviceDeferredPatternLaunch() {
        if(patternDeferredClip<0)return;
        const int target=patternDeferredClip,source=patternDeferredSource;
        patternDeferredClip=patternDeferredSource=-1;
        // A boundary reached at the end of the previous callback retries at
        // the start of this one. Preserve the usual chain launch/bookkeeping
        // path, and discard a deferred request superseded by user controls.
        if(!clipPlaying || activeClip!=source || queuedClip!=target
            || preparedClip.load(std::memory_order_relaxed)!=target)return;
        if(chainLaunchBank>=0)launchQueuedChain();
        else if(chainEnabled)advanceClipChain();
        else activatePreparedClip();
    }
    void primePatternWarmup() {
        if(!patternWarmup || patternJob.load(std::memory_order_acquire)!=0)return;
        if(!patternLookaheadFrames){
        applyPatternCommands();
        const bool priorProfile=patternShadow->renderProfileEnabled;
        const bool profile=renderProfileEnabled&&renderProfileActive;
        if(profile)markRenderProfile(1);
        patternShadow->renderProfileEnabled=profile;
        patternShadow->render(nullptr,kBlockFrames,patternWarmupAudio.data());
        if(profile)mergeRenderProfile(patternShadow->renderProfileCurrent);
        patternShadow->renderProfileEnabled=priorProfile;
        patternWarmupOffset=0;
        }
        // Every pointer, source event and DSP history is now owned by the
        // outgoing scene. Start its next block BEFORE restoring the incoming
        // scene. The callback consumes only the completed private prefix;
        // neither scene waits for or concurrently renders the other's state.
        if(patternWorkerMeasure.load(std::memory_order_acquire)){
            timespec now{};clock_gettime(CLOCK_MONOTONIC,&now);
            patternWakeStamp.store(std::uint64_t(now.tv_sec)*1000000000u+std::uint64_t(now.tv_nsec),std::memory_order_release);
        }
        patternJob.store(1,std::memory_order_release);patternSignal.notify();
    }
    std::size_t renderPatternWarmup(int16_t* output,float* raw,int rendered,std::size_t count) {
        // Direct beginPatternFade callers use this fallback; prepared clip
        // activation already primed before touching the incoming scene.
        primePatternWarmup();
        count=std::min(count,patternWarmupFrames-patternWarmupOffset);
        const float* old=patternWarmupAudio.data()+patternWarmupOffset*2u;
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
        patternWarmupOffset+=count;
        if(patternWarmupOffset==patternWarmupFrames)patternWarmup=false;
        renderClockSamples.fetch_add(count,std::memory_order_relaxed);
        markRenderProfile(5);
        return count;
    }
    int formatPatternWorker(char* buffer,int capacity) const {
        const auto write=patternWrite.load(std::memory_order_acquire);
        const auto read=patternRead.load(std::memory_order_relaxed);
        const double rate=std::max(1.0f,sampleRate);
        const double queued=patternActive?std::min<std::uint64_t>(write-read,kPatternRingFrames)*1000.0/rate:0;
        return std::snprintf(buffer,capacity,"%d,%d,%llu,%llu,%llu,%llu,%.3f,%.3f,%d,%.3f,%llu",
            patternWorkerFlush.load(std::memory_order_acquire),patternWorkerCpuAvailable.load(std::memory_order_relaxed)?1:0,
            static_cast<unsigned long long>(patternWorkerWallPeak.load(std::memory_order_relaxed)/1000u),
            static_cast<unsigned long long>(patternWorkerCpuPeak.load(std::memory_order_relaxed)/1000u),
            static_cast<unsigned long long>(patternStartUnderflows),
            static_cast<unsigned long long>(patternUnderflows-patternStartUnderflows),
            patternMissingFrames*1000.0/rate,queued,
            patternWorkerNice.load(std::memory_order_acquire),
            patternMaximumQueued.load(std::memory_order_relaxed)*1000.0/rate,
            static_cast<unsigned long long>(patternWorkerWakePeak.load(std::memory_order_relaxed)/1000u));
    }
    int formatPatternFade(char* buffer,int capacity) const {
        return std::snprintf(buffer,capacity,"%d,%d,%d,%d,%d,%d,%d,%llu,%d,%.0f,%.0f,%.0f,%d",patternActive?1:0,patternHeld?1:0,
            patternFrom,patternTo,static_cast<int>(std::lround(patternPosition*10000)),
            static_cast<int>(std::lround(patternFadeMs)),patternActive?patternPlaybackCurve:patternCurve,static_cast<unsigned long long>(patternUnderflows),
            patternTimingMode,patternFadeLowMs,patternFadeHighMs,patternDurationMs,patternActive&&patternFadeOverridden?1:0);
    }
