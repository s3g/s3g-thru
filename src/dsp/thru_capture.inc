    // A separate, preallocated bank preserves the source during Shared Dual
    // and self captures. Publication uses the bounded sample-load handoff.
    Engine partRecorder;
    std::unique_ptr<float[]> partCaptureStorage;
    std::array<std::array<float,kBlockFrames>,2u> partCaptureBus {};
    bool partCaptureRecording=false, partCapturePublishing=false;
    std::size_t partCaptureDestination=0;

    bool audioRecording() const {return engine.recording() || partCaptureRecording;}
    const Engine& recordingEngine() const {return recordSource>=2 ? partRecorder : engine;}
    bool validRecordSource(int source) const
    {
        return source>=0 && source<(splitEnabled?partCount()+2:2);
    }
    bool beginPartCapture()
    {
        if(patternBusy()){setError("Finish pattern fade before recording");return false;}
        const auto prepared=static_cast<ClipPrepareState>(clipPrepareState.load(std::memory_order_acquire));
        if(queuedClip>=0 || pendingClipSourceMask!=0 || prepared==ClipPrepareState::ready) {
            setError("Finish queued pattern and source changes before capture");return false;
        }
        if(!validRecordSource(recordSource) || !splitEnabled) {
            setError("Choose an available recording source");return false;
        }
        const auto state=static_cast<SampleLoadState>(sampleLoadState.load(std::memory_order_acquire));
        if(state==SampleLoadState::requested || state==SampleLoadState::loading
            || state==SampleLoadState::ready || state==SampleLoadState::removing
            || state==SampleLoadState::copying || state==SampleLoadState::previewing) {
            setError("Stop preview and finish sample loading before capture");return false;
        }
        if(!partCaptureStorage) {setError("Part capture buffer is unavailable");return false;}
        partRecorder.clear();
        partCaptureDestination=selectedLane;
        if(!partRecorder.beginLaneRecording(0,0)) {
            setError("Part capture could not start");return false;
        }
        partCaptureRecording=true;
        pushClipUndo(activeClip);
        return true;
    }
    void finishPartCapture()
    {
        if(!partCaptureRecording) return;
        partCaptureRecording=false;
        if(partRecorder.recording()) partRecorder.endRecording();
        const auto frames=partRecorder.laneFrames(0);
        if(frames==0) {setError("Recording was too short or empty");return;}
        partCapturePublishing=true;
        sampleRequestLane=static_cast<int>(partCaptureDestination);
        sampleRequestClip=activeClip;
        sampleRequestPreview=false;
        sampleRequestFromClip=false;
        sampleRequestPath.fill(0);
        std::snprintf(sampleLoadedName.data(),sampleLoadedName.size(),"PART %c TAKE",'A'+recordSource-2);
        sampleStageFrames=frames;sampleStageRate=sampleRate;
        sampleStageRegions={};
        sampleStageRegions.count=sampleStageRegions.boundaries.size()-1u;
        for(std::size_t i=0;i<sampleStageRegions.boundaries.size();++i)
            sampleStageRegions.boundaries[i]=static_cast<float>(i)/sampleStageRegions.count;
        sampleStageWaveMinimum.fill(0);sampleStageWaveMaximum.fill(0);
        sampleLoadState.store(static_cast<int>(SampleLoadState::ready),std::memory_order_release);
    }
    void renderPartCapture(std::size_t frames, const Engine::Controls& bankControls)
    {
        if(!partCaptureRecording) return;
        for(std::size_t frame=0;frame<frames;++frame) {
            input[0][frame]=partCaptureBus[0][frame]*recordGain;
            input[1][frame]=partCaptureBus[1][frame]*recordGain;
        }
        partRecorder.stopPlayback();
        partRecorder.process(inputPointers.data(),outputPointers.data(),frames,bankControls);
        if(!partRecorder.recording()) finishPartCapture();
    }

    // Shutdown has stopped the audio callback and sample worker. Finish the
    // in-memory handoff before the ordinary synchronous archive flush.
    void finalizePartCaptureOffline()
    {
        if(!partCapturePublishing) return;
        const auto frames=partRecorder.laneFrames(0);
        for(std::size_t channel=0;channel<2;++channel)
            std::copy_n(partCaptureStorage.get()+channel*laneCapacityFrames,frames,
                engine.writableLane(partCaptureDestination,channel));
        if(engine.adoptLane(partCaptureDestination,frames,sampleRate)) {
            installRegionMap(partCaptureDestination,sampleStageRegions);
            completedRecordingLane(partCaptureDestination);
        }
        else setError("Part capture could not be published");
        pendingClearMask&=~(1u<<partCaptureDestination);
        partCapturePublishing=false;
        sampleLoadState.store(static_cast<int>(SampleLoadState::complete),std::memory_order_release);
    }
