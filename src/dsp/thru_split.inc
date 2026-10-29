    bool isQuad() const {return splitEnabled&&quadMode;}
    int partCount() const {return isQuad()?4:2;}
    int voicesPerPart() const {return isQuad()?2:4;}
    int cuesPerPart() const {return isQuad()?2:4;}
    int firstLaneForPart(int part) const {return isQuad()?part:part*2;}
    bool partOwnsLane(int part,std::size_t lane) const
    {return sharedDual||static_cast<int>(lane)==firstLaneForPart(part)||(!isQuad()&&static_cast<int>(lane)==firstLaneForPart(part)+1);}
    static std::size_t partLevelParameter(int p) {const LockParameter keys[]{LockParameter::splitLevelA,LockParameter::splitLevelB,LockParameter::splitLevelC,LockParameter::splitLevelD};return static_cast<std::size_t>(keys[p]);}
    static std::size_t partDelayParameter(int p) {const LockParameter keys[]{LockParameter::splitDelayA,LockParameter::splitDelayB,LockParameter::splitDelayC,LockParameter::splitDelayD};return static_cast<std::size_t>(keys[p]);}
    static std::size_t partReverbParameter(int p) {const LockParameter keys[]{LockParameter::splitReverbA,LockParameter::splitReverbB,LockParameter::splitReverbC,LockParameter::splitReverbD};return static_cast<std::size_t>(keys[p]);}
    int padForPart(int part,int local) const {return isQuad()?(1-part/2)*16+(part%2)*4+(local/4)*8+local%4:part*4+(local/4)*8+local%4;}
    int cueForPad(int pad) const {const int local=localPad(pad);return splitEnabled?(local<cuesPerPart()?partForInput(pad)*cuesPerPart()+local:-1):(pad<8?pad:-1);}

    int rollOwner(int sourcePad,bool recorded,int noteIndex) const
    {
        if(!splitEnabled)return 0;
        const auto* clip=activeClipSlot();
        if(recorded&&clip&&noteIndex>=0&&noteIndex<clip->clipNoteCount&&clip->clipNotes[noteIndex].part)
            return clip->clipNotes[noteIndex].part-1;
        return partForInput(sourcePad);
    }
    int displayedPad(int sourcePad,int part,bool recorded) const
    {
        if(sourcePad<0||sourcePad>=32)return sourcePad;
        return recorded&&splitEnabled&&partForInput(sourcePad)!=part
            ?padForPart(part,localPad(sourcePad)%(isQuad()?8:16)):sourcePad;
    }

    static bool splitPartParameter(std::size_t index)
    {
        const auto p = static_cast<LockParameter>(index);
        if (p == LockParameter::outputGain || index >= static_cast<std::size_t>(LockParameter::splitBlend)) return false;
        if (index >= static_cast<std::size_t>(LockParameter::effectTone)
            && index <= static_cast<std::size_t>(LockParameter::effectMix))
            return index <= static_cast<std::size_t>(LockParameter::effectRate);
        if (index >= static_cast<std::size_t>(LockParameter::effectTapeFormat)
            && index <= static_cast<std::size_t>(LockParameter::effectTapeInertia)) return false;
        if (index >= static_cast<std::size_t>(LockParameter::effectReverbAlgorithm)
            && index <= static_cast<std::size_t>(LockParameter::effectOrder)) return false;
        return true;
    }

    PartSettings currentPartSettings() const
    {
        return {rootNote, padOctave, static_cast<int>(padScale), static_cast<int>(padMode),
            pressureMode, holdReleaseMs, stackScanPaused ? 1 : 0, glitchBufferLocked ? 1 : 0,
            splitEnabled ? splitParts[static_cast<std::size_t>(splitFocus)].settings.muted : 0,
            static_cast<int>(laneVariation)};
    }

    void applyPartSettings(const PartSettings& s)
    {
        rootNote = clampValue(s.root,24,96); padOctave = clampValue(s.octave,-3,3);
        padScale = static_cast<PadScale>(clampValue(s.scale,0,24));
        padMode = static_cast<PadMode>(clampValue(s.mode,0,3)); pressureMode = s.pressure != 0;
        holdReleaseMs = clampValue(s.holdMs,0,60000);
        stackScanPaused = s.stackHold != 0; glitchBufferLocked = s.glitchHold != 0;
        laneVariation = static_cast<LaneVariation>(clampValue(s.variation,0,4));
    }

    PartSettings settingsForPart(int part) const
    {
        return !splitEnabled || part == splitFocus ? currentPartSettings()
            : splitParts[static_cast<std::size_t>(part)].settings;
    }

    const Engine::Controls& liveControlsForPart(int part) const
    {
        return !splitEnabled || part == splitFocus ? controls
            : splitParts[static_cast<std::size_t>(part)].controls;
    }

    void storeFocusedPart()
    {
        if (!splitEnabled) return;
        auto& target = splitParts[static_cast<std::size_t>(splitFocus)];
        target.controls = controls; target.settings = currentPartSettings();
        for(auto& part:splitParts) {
            float ignored=outputGain;
            for(std::size_t p=0;p<kLockParameterCount;++p)if(!splitPartParameter(p))
                applyLockedValue(part.controls,ignored,p,lockedValue(controls,outputGain,p));
        }
    }

    void loadPartEditor(int part)
    {
        const auto& source = splitParts[static_cast<std::size_t>(part)];
        for (std::size_t p=0;p<kLockParameterCount;++p)
            if (splitPartParameter(p)) applyLockedValue(controls,outputGain,p,lockedValue(source.controls,outputGain,p));
        applyPartSettings(source.settings);
    }

    void initializeSplitState()
    {
        splitSingleControls = controls; splitSingleSettings = currentPartSettings();
        for (auto& part : splitParts) { part.controls = controls; part.settings = splitSingleSettings; }
    }

    int partForInput(int sourcePad) const
    {
        return sourcePad >= 0 && sourcePad < 32 ? (isQuad()?(1-sourcePad/16)*2+(sourcePad%8>=4?1:0):(sourcePad%8>=4?1:0)) : midiPart;
    }

    int localPad(int sourcePad) const
    {
        return splitEnabled && sourcePad >= 0 && sourcePad < 32
            ? (isQuad()?sourcePad/8%2:sourcePad/8)*4+sourcePad%4 : sourcePad;
    }

    bool voiceOwnsLane(const PlaybackVoice& voice, std::size_t lane) const
    {
        return !splitEnabled || partOwnsLane(voice.part,lane);
    }

    void selectSplitPart(int part, bool notify = true)
    {
        part = clampValue(part,0,partCount()-1);
        if (!splitEnabled || part == splitFocus) return;
        storeFocusedPart(); splitFocus = part; loadPartEditor(part);
        if (notify && !engine.recording() && !sharedDual) selectedLane = static_cast<std::size_t>(firstLaneForPart(part));
        if (notify) markPersistentStateDirty();
    }

    int performanceMode() const {return splitEnabled?(quadMode?3:sharedDual?2:1):0;}

    void setSplitMode(bool enabled) {setPerformanceMode(enabled?(quadMode?3:sharedDual?2:1):0);}

    void setPerformanceMode(int mode)
    {
        const bool enabled=mode!=0,shared=mode==2,quad=mode==3;
        if(enabled==splitEnabled&&(!enabled||(shared==sharedDual&&quad==quadMode)))return;
        if(engine.recording()||clipRecording){setError("Stop recording before changing mode");return;}
        const bool wasMulti=splitEnabled,wasQuad=quadMode;
        captureActiveClip(false);clipPlaying=false;releaseClipVoices();
        splitMixerOverrides.fill(false);beginPreviewTransitionTail();previewActive=false;
        if(enabled) {
            if(!wasMulti){splitSingleControls=controls;splitSingleSettings=currentPartSettings();}
            if(!splitInitialized) {
                for(auto& part:splitParts) {
                    part.controls=controls;part.settings=splitSingleSettings;
                    part.controls.manualLane=clampValue(part.controls.manualLane,0.0f,shared?3.0f:quad?0.0f:1.0f);
                    part.controls.effectOrder=7;part.controls.effectMix=part.controls.effectReverbMix=1.0f;
                }
                splitInitialized=true;
            }
            if(quad&&!quadInitialized) {
                for(int p=2;p<4;++p){splitParts[p].controls=controls;splitParts[p].settings=currentPartSettings();splitParts[p].settings.muted=0;}
                quadInitialized=true;
            }
            if(sharedDual!=shared||wasQuad!=quad)for(int part=0;part<4;++part) {
                auto& lane=splitParts[part].controls.manualLane;
                const float physical=lane+(sharedDual?0.0f:wasQuad?static_cast<float>(part):part*2.0f);
                lane=clampValue(physical-(shared?0.0f:quad?static_cast<float>(part):part*2.0f),0.0f,shared?3.0f:quad?0.0f:1.0f);
            }
            sharedDual=shared;quadMode=quad;splitEnabled=true;
            if(!wasMulti)splitFocus=0;
            splitFocus=clampValue(splitFocus,0,partCount()-1);
            float ignored=outputGain;
            for(std::size_t p=0;p<kLockParameterCount;++p)if(!splitPartParameter(p))
                applyLockedValue(controls,ignored,p,lockedValue(splitParts[0].controls,outputGain,p));
            loadPartEditor(splitFocus);
            if(!sharedDual&&!partOwnsLane(splitFocus,selectedLane))selectedLane=firstLaneForPart(splitFocus);
            splitBlendState=controls.splitBlend;splitCurveState=static_cast<float>(controls.splitCurve);
        } else {
            storeFocusedPart();splitEnabled=false;splitFocus=0;controls=splitSingleControls;applyPartSettings(splitSingleSettings);
        }
        for(auto& insert:splitInserts)insert.reset();
        splitRollWrite={};splitRollFrames={};refreshPlaybackVoices();
        for(auto& roll:rolls){roll.active=roll.held=false;roll.envelope=roll.target=0;}
        captureActiveClip(true);markPersistentStateDirty();clearError();
    }

    void copySplitPart(int source)
    {
        if (!splitEnabled) return;
        source = clampValue(source,0,partCount()-1); storeFocusedPart();
        const int target = isQuad()?(source+1)%4:1-source;
        auto& to = splitParts[static_cast<std::size_t>(target)];
        const auto& from = splitParts[static_cast<std::size_t>(source)];
        for (std::size_t p=0;p<kLockParameterCount;++p)
            if (splitPartParameter(p)) applyLockedValue(to.controls,outputGain,p,lockedValue(from.controls,outputGain,p));
        if(!sharedDual)for(int lane=0;lane<(isQuad()?1:2);++lane)to.controls.laneGains[firstLaneForPart(target)+lane]=from.controls.laneGains[firstLaneForPart(source)+lane];
        // Copy the sound, preserving the destination's intentional mute.
        const int destinationMute = to.settings.muted;
        to.settings = from.settings;
        to.settings.muted = destinationMute;
        if (target == splitFocus) loadPartEditor(target);
        if (clipRecording) {
            for (std::size_t p=0;p<kLockParameterCount;++p) if (splitPartParameter(p))
                captureClipPoint(ClipPointType::partParameter,p,target,lockedValue(to.controls,outputGain,p));
        }
        markPersistentStateDirty();
    }

    void releaseSplitPart(int part)
    {
        if (!splitEnabled) { allNotesOff(); return; }
        for (auto& voice : voices) if (voice.active && voice.part == part) {
            if (!voice.clipOwned) captureClipNoteOff(voice.note,voice.sourcePad,part);
            voice.held = false; beginVoiceRelease(voice); clearPadMappingForVoice(voice);
        }
        for (auto& roll : rolls) if (roll.active && roll.part == part) {
            if (!roll.clipOwned) captureClipNoteOff(roll.note,roll.sourcePad,part);
            roll.held = false; roll.target = 0;
        }
        for (std::size_t pad=0;pad<32;++pad) if (partForInput(static_cast<int>(pad)) == part) {
            padMappedNotes[pad] = latchedPadNotes[pad] = -1; latchedPadDeadlines[pad] = 0u;
        }
    }

    void captureSplitClip(ClipSlot& clip)
    {
        storeFocusedPart();
        clip.splitEnabled = splitEnabled; clip.sharedDual=sharedDual;clip.quadMode=quadMode;clip.quadInitialized=quadInitialized; clip.splitInitialized = splitInitialized; clip.splitFocus = splitFocus;
        clip.singleSettings = splitEnabled ? splitSingleSettings : currentPartSettings();
        if (!splitEnabled) splitSingleControls = controls;
        for (std::size_t p=0;p<kLockParameterCount;++p) {
            clip.baseValues[p] = lockedValue(splitEnabled ? splitSingleControls : controls,outputGain,p);
            for (int part=0;part<4;++part) {
                const auto& c = splitParts[static_cast<std::size_t>(part)].controls;
                clip.splitValues[static_cast<std::size_t>(part)][p] = lockedValue(
                    splitEnabled && !splitPartParameter(p) ? controls : c,outputGain,p);
            }
        }
        for (int part=0;part<4;++part) clip.splitSettings[static_cast<std::size_t>(part)] = splitParts[static_cast<std::size_t>(part)].settings;
    }

    void restoreSplitClip(const ClipSlot& clip)
    {
        splitSingleControls = controls; splitSingleSettings = clip.singleSettings;
        splitEnabled = clip.splitEnabled; sharedDual=clip.sharedDual;quadMode=clip.quadMode;quadInitialized=clip.quadInitialized; splitInitialized = clip.splitInitialized; splitFocus = clampValue(clip.splitFocus,0,partCount()-1);
        for (int part=0;part<4;++part) {
            auto& target = splitParts[static_cast<std::size_t>(part)]; target.controls = controls;
            float ignoredGain=outputGain;
            for (std::size_t p=0;p<kLockParameterCount;++p)
                applyLockedValue(target.controls,ignoredGain,p,clip.splitValues[static_cast<std::size_t>(part)][p]);
            target.settings = clip.splitSettings[static_cast<std::size_t>(part)];
        }
        if (splitEnabled) {
            // Shared return/mixer values have a canonical copy in part A.
            for (std::size_t p=0;p<kLockParameterCount;++p) if (!splitPartParameter(p))
                applyLockedValue(controls,outputGain,p,clip.splitValues[0][p]);
            loadPartEditor(splitFocus);
        } else applyPartSettings(clip.singleSettings);
        for(int part=0;part<4;++part)splitClipControls[part]=liveControlsForPart(part);
    }

    std::array<float,2> parameterDisplayWindow(const Engine::Controls& c,int part=0) const
    {
        const auto first=splitEnabled&&!sharedDual?static_cast<std::size_t>(firstLaneForPart(part)):0u;
        const auto last=splitEnabled&&!sharedDual?first+(isQuad()?1u:2u):kLaneCount;
        std::size_t frames=0u;
        for(auto lane=first;lane<last;++lane)frames=std::max(frames,engine.laneFrames(lane));
        const float minimum=frames?std::min(4.0f,static_cast<float>(frames))/static_cast<float>(frames):0.0f;
        const float width=clampValue(c.field,std::max(.02f,minimum),1.0f);
        const float center=clampValue(c.locus,width*.5f,1-width*.5f);
        return {center-width*.5f,center+width*.5f};
    }

    std::array<float,2> displayWindow(int part=0) const
    {
        const auto& c=splitEnabled
            ?(clipPlaying?splitClipControls[static_cast<std::size_t>(part)]:liveControlsForPart(part))
            :(clipPlaying?clipControls:controls);
        auto result=parameterDisplayWindow(c,part);
        bool active=false;
        for(const auto& voice:voices) {
            if(!voice.active||(splitEnabled&&voice.part!=part)||(!voice.held&&voice.envelope<=.00001f))continue;
            const float low=voice.engine.normalizedFieldStart(),high=voice.engine.normalizedFieldEnd();
            if(high<=low)continue;
            if(!active){result={low,high};active=true;}
            else {result[0]=std::min(result[0],low);result[1]=std::max(result[1],high);}
            const auto include=[&](float position){if(position>=0){result[0]=std::min(result[0],position);result[1]=std::max(result[1],position);}};
            include(voice.engine.normalizedPosition());
            if(voice.engine.playbackAlgorithm()==Engine::PlaybackAlgorithm::cutups) {
                const auto display=voice.engine.cutupsDisplayState();
                if(display.gain>.00001f)include(display.position);
                if(display.outgoingGain>.00001f)include(display.outgoingPosition);
            } else if(voice.engine.playbackAlgorithm()==Engine::PlaybackAlgorithm::wavesets) {
                const auto display=voice.engine.wavesetDisplayState();
                include(display.sourcePosition);include(display.secondarySourcePosition);
            }
        }
        result[0]=clampValue(result[0],0.0f,1.0f);result[1]=clampValue(result[1],0.0f,1.0f);
        return result;
    }

    std::array<float,2> sharedDisplayWindow() const
    {
        // Shared sources need one edit window. Following the focused controls
        // keeps POS/FLD responsive even when the other program, recorded voices,
        // or releasing readers still use a wider or different source region.
        return parameterDisplayWindow(liveControlsForPart(splitFocus),splitFocus);
    }

    int formatSplitState(char* buffer,int capacity) const
    {
        int at=std::snprintf(buffer,capacity,"{\"enabled\":%d,\"shared\":%d,\"quad\":%d,\"focus\":%d,\"blend\":%.5f,\"parts\":[",splitEnabled?1:0,sharedDual?1:0,quadMode?1:0,splitFocus,splitBlendState);
        for(int p=0;p<partCount();++p) {
            if(at<0||at>=capacity)return -1;
            const auto ui=settingsForPart(p);const auto& c=liveControlsForPart(p);
            const auto window=isQuad()?parameterDisplayWindow(c,p):displayWindow(p);
            const PlaybackVoice* head=nullptr;
            for(const auto& v:voices)if(v.active&&v.held&&v.part==p&&(!head||v.age>head->age))head=&v;
            const int n=std::snprintf(buffer+at,capacity-at,"%s{\"algorithm\":%d,\"root\":%d,\"octave\":%d,\"scale\":%d,\"mode\":%d,\"hold\":%d,\"mute\":%d,\"position\":%.5f,\"window\":[%.7f,%.7f]}",p?",":"",static_cast<int>(c.algorithm),ui.root,ui.octave,ui.scale,ui.mode,ui.stackHold,ui.muted,head?head->engine.normalizedPosition():0,window[0],window[1]);
            if(n<0||n>=capacity-at)return -1;
            at+=n;
        }
        if(at<0||at+3>=capacity)return -1;
        return at+std::snprintf(buffer+at,capacity-at,"]}");
    }

    void clearSplitHoldKeys(int part)
    {
        for(std::size_t key=0;key<liveStackHoldPhaseValid.size();++key) {
            const int owner=key<32u?partForInput(static_cast<int>(key)):static_cast<int>((key-32u)/128u);
            if(owner==part)liveStackHoldPhaseValid[key]=false;
        }
    }

    void touchSplitMixerParameter(std::size_t parameter)
    {
        const auto first = static_cast<std::size_t>(LockParameter::splitBlend);
        if (splitEnabled && clipPlaying && parameter >= first && parameter < kLockParameterCount)
            splitMixerOverrides[parameter-first] = true;
    }

    Engine::Controls effectiveSplitMixer(const Engine::Controls& recorded) const
    {
        Engine::Controls mixer = clipPlaying ? recorded : controls;
        if (clipPlaying) {
            float ignored = outputGain;
            const auto first = static_cast<std::size_t>(LockParameter::splitBlend);
            for (std::size_t i=0;i<splitMixerOverrides.size();++i) if (splitMixerOverrides[i])
                applyLockedValue(mixer,ignored,first+i,lockedValue(controls,outputGain,first+i));
        }
        return mixer;
    }

    template<std::size_t N>
    void processSplitBuses(std::size_t frames, const std::array<Engine::Controls,N>& live,
                           const Engine::Controls& recorded)
    {
        const Engine::Controls mixer = effectiveSplitMixer(recorded);
        for (std::size_t part=0;part<static_cast<std::size_t>(partCount());++part) {
            for (std::size_t frame=0;frame<frames;++frame) {
                const float power = splitEnvelopePower[part][frame];
                const float target = polyphonyCompensation && power>1 ? 1/std::sqrt(power) : 1;
                splitPolyGain[part] += (target-splitPolyGain[part])
                    * (target<splitPolyGain[part] ? polyphonyAttackCoefficient : polyphonyReleaseCoefficient);
                for(auto& channel:splitBus[part])channel[frame]*=splitPolyGain[part];
            }
            rememberSplitRollHistory(part,frames);
            processAudioRolls(frames,static_cast<int>(part));
            splitInserts[part].process(frames,splitBus[part],clipPlaying?splitClipControls[part]:live[part]);
        }
        const float targetBlend = mixer.splitBlend;
        const float levels[4] {mixer.splitLevelA,mixer.splitLevelB,mixer.splitLevelC,mixer.splitLevelD};
        const float delay[4] {mixer.splitDelayA,mixer.splitDelayB,mixer.splitDelayC,mixer.splitDelayD};
        const float rev[4] {mixer.splitReverbA,mixer.splitReverbB,mixer.splitReverbC,mixer.splitReverbD};
        std::array<float,4u> peaks {};
        for(std::size_t frame=0;frame<frames;++frame) {
            splitBlendState+=(targetBlend-splitBlendState)*effectMixSlew;
            if(std::abs(targetBlend-splitBlendState)<1e-6f)splitBlendState=targetBlend;
            splitCurveState+=(static_cast<float>(mixer.splitCurve)-splitCurveState)*effectControlSlew;
            const float linear[2] {1-splitBlendState,splitBlendState};
            const float power[2] {std::cos(splitBlendState*1.57079632679f),std::sin(splitBlendState*1.57079632679f)};
            float gains[4] {};
            for(std::size_t part=0;part<static_cast<std::size_t>(partCount());++part) {
                const float level=settingsForPart(static_cast<int>(part)).muted?0:levels[part];
                splitLevelState[part]+=(level-splitLevelState[part])*effectMixSlew;
                splitDelayState[part]+=(delay[part]-splitDelayState[part])*effectMixSlew;
                splitReverbState[part]+=(rev[part]-splitReverbState[part])*effectMixSlew;
                gains[part]=(isQuad()?.5f:std::max(0.0f,linear[part]+(power[part]-linear[part])*splitCurveState))*splitLevelState[part];
                splitFrameGain[part][frame]=gains[part];
            }
            for(std::size_t channel=0;channel<2;++channel) {
                float dry=0,sendDelay=0,sendReverb=0;
                for(int p=0;p<partCount();++p) {
                    const float value=splitBus[p][channel][frame]*gains[p];
                    peaks[p]=std::max(peaks[p],std::abs(value));
                    dry+=value;sendDelay+=value*splitDelayState[p];sendReverb+=value*splitReverbState[p];
                }
                voiceMix[channel][frame]=dry;
                splitDelayInput[channel][frame]=sendDelay;splitReverbInput[channel][frame]=sendReverb;
            }
        }
        const float decay=std::exp(-static_cast<float>(frames)/(sampleRate*.15f));
        for(std::size_t part=0;part<static_cast<std::size_t>(partCount());++part)splitMeter[part]=std::max(peaks[part],splitMeter[part]*decay);
    }

    // Readouts use the actual post-insert/post-Blend bus, excluding shared tails.
    int mixStatus(char* buffer,int length) const
    {
        const auto mixer=effectiveSplitMixer(clipPlaying?clipControls:controls);
        int count[4]{},reason[4]{};bool sounding[4]{};
        for(const auto& v:voices)if(v.active){++count[v.part];sounding[v.part]=true;}
        for(const auto& roll:rolls)if(roll.active)sounding[clampValue(roll.part,0,3)]=true;
        for(int p=0;p<partCount();++p) {
            if(splitParts[p].settings.muted)reason[p]=1;
            else if(lockedValue(mixer,outputGain,partLevelParameter(p))<=1e-6f)reason[p]=2;
            else if(!isQuad()&&(p?mixer.splitBlend:1-mixer.splitBlend)<=1e-6f)reason[p]=3;
            else {bool loaded=false;for(std::size_t lane=0;lane<4;++lane)loaded|=engine.laneLoaded(lane)&&partOwnsLane(p,lane);
                if(!loaded)reason[p]=4;else if(!sounding[p])reason[p]=5;}
        }
        if(isQuad())return std::snprintf(buffer,length,"%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%d,%d,%d,%d",splitMeter[0],splitMeter[1],splitMeter[2],splitMeter[3],count[0],count[1],count[2],count[3],reason[0],reason[1],reason[2],reason[3]);
        return std::snprintf(buffer,length,"%.4f,%.4f,%d,%d,%d,%d",splitMeter[0],splitMeter[1],count[0],count[1],reason[0],reason[1]);
    }

    void rememberSplitRollHistory(std::size_t part, std::size_t frames)
    {
        float* left=splitRollHistory.get()+part*2u*rollCapacityFrames;
        float* right=left+rollCapacityFrames;
        for(std::size_t frame=0;frame<frames;++frame) {
            left[splitRollWrite[part]]=splitBus[part][0][frame];
            right[splitRollWrite[part]]=splitBus[part][1][frame];
            splitRollWrite[part]=(splitRollWrite[part]+1)%rollCapacityFrames;
        }
        splitRollFrames[part]=std::min(rollCapacityFrames,splitRollFrames[part]+frames);
    }

    static bool splitParameterInterpolates(std::size_t index)
    {
        switch(static_cast<LockParameter>(index)) {
            case LockParameter::algorithm: case LockParameter::motionMode: case LockParameter::motionSound:
            case LockParameter::lanesMode: case LockParameter::lanePath: case LockParameter::lanesStretch:
            case LockParameter::grainMode: case LockParameter::grainWindow: case LockParameter::cutupsMode:
            case LockParameter::motorShape: case LockParameter::laneBlend: case LockParameter::lanePathShape:
            case LockParameter::grainBias: case LockParameter::grainTiming: case LockParameter::grainProcess:
            case LockParameter::grainAdvance: case LockParameter::cutClock: case LockParameter::cutDivision:
            case LockParameter::cutLaneOrder: case LockParameter::cutPolyPath: case LockParameter::cutLaneAdvance:
            case LockParameter::cutRegionMode: case LockParameter::sampleMode: case LockParameter::sampleRegionMode:
            case LockParameter::motionRateBasis: case LockParameter::segmentTrigger: case LockParameter::segmentOverlap:
            case LockParameter::glitchMode: case LockParameter::glitchClock: case LockParameter::glitchDivision:
            case LockParameter::longGrainMode: case LockParameter::longGrainShape: case LockParameter::longGrainBias:
            case LockParameter::longGrainAdvance: case LockParameter::wavesetGroup: case LockParameter::wavesetRepeats:
            case LockParameter::wavesetStride: case LockParameter::wavesetAdvance: case LockParameter::wavesetDirection:
            case LockParameter::wavesetProcess: case LockParameter::wavesetDetail: case LockParameter::splitCurve:
                return false;
            default:return true;
        }
    }

    static std::array<int,10> settingsValues(const PartSettings& s)
    { return {s.root,s.octave+3,s.scale,s.mode,s.pressure,s.holdMs,s.stackHold,s.glitchHold,s.muted,s.variation}; }

    static bool decodeSettings(const std::array<int,10>& v, PartSettings& s)
    {
        if(v[0]<24||v[0]>96||v[1]<0||v[1]>6||v[2]<0||v[2]>24||v[3]<0||v[3]>3
           ||v[4]<0||v[4]>1||v[5]<0||v[5]>60000||v[6]<0||v[6]>1||v[7]<0||v[7]>1||v[8]<0||v[8]>1||v[9]<0||v[9]>4)return false;
        s={v[0],v[1]-3,v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9]};return true;
    }

    static bool writePartSettings(std::FILE* file,const PartSettings& s)
    {
        for(int value:settingsValues(s))if(!writeU32(file,static_cast<std::uint32_t>(value)))return false;
        return true;
    }
    static bool readPartSettings(std::FILE* file,PartSettings& s)
    {
        std::array<int,10> values{};
        for(auto& value:values){std::uint32_t n=0;if(!readU32(file,n)||n>60000)return false;value=static_cast<int>(n);}
        return decodeSettings(values,s);
    }
    static bool writeSplitExtension(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips)
    {
        if(!writeU32(file,0x53504c33u))return false;
        for(const auto& clip:clips) {
            if(!writeU32(file,(clip.splitEnabled?1u:0u)|(clip.sharedDual?2u:0u)|(clip.quadMode?4u:0u)|(clip.quadInitialized?8u:0u))||!writeU32(file,clip.splitInitialized?1u:0u)
               ||!writeU32(file,static_cast<std::uint32_t>(clip.splitFocus))||!writePartSettings(file,clip.singleSettings))return false;
            for(std::size_t part=0;part<4;++part) {
                for(float value:clip.splitValues[part])if(!writeF32(file,value))return false;
                if(!writePartSettings(file,clip.splitSettings[part]))return false;
            }
        }
        return true;
    }
    static bool readSplitExtension(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips)
    {
        std::uint32_t magic=0;if(!readU32(file,magic)||(magic!=0x53504c31u&&magic!=0x53504c32u&&magic!=0x53504c33u))return false;
        const bool quadFormat=magic==0x53504c33u;
        for(auto& clip:clips) {
            std::uint32_t mode=0,initialized=0,focus=0;
            if(!readU32(file,mode)||mode>(quadFormat?15u:magic==0x53504c31u?1u:3u)||((mode&6u)==6u)||!readU32(file,initialized)||initialized>1||!readU32(file,focus)||focus>(quadFormat&&(mode&5u)==5u?3u:1u)||!readPartSettings(file,clip.singleSettings))return false;
            clip.splitEnabled=mode&1u;clip.sharedDual=mode&2u;clip.quadMode=mode&4u;clip.quadInitialized=mode&8u;clip.splitInitialized=initialized!=0;clip.splitFocus=focus;
            for(std::size_t part=0;part<(quadFormat?4u:2u);++part) {
                clip.splitValues[part]=clip.baseValues;
                for(std::size_t p=0;p<(quadFormat?kLockParameterCount:kVersion14LockParameterCount);++p)if(!readF32(file,clip.splitValues[part][p]))return false;
                if(!readPartSettings(file,clip.splitSettings[part]))return false;
            }
            if(!quadFormat)for(int part=2;part<4;++part){clip.splitValues[part]=clip.splitValues[0];clip.splitSettings[part]=clip.splitSettings[0];}
        }
        return true;
    }

    int appendSplitJson(char* buffer,int capacity,int length) const
    {
        if(length<=0||length>=capacity)return length;
        int at=length-1;
        const auto append=[&](const char* format,auto...values) {
            if(at<0||at>=capacity)return;
            const int n=std::snprintf(buffer+at,static_cast<std::size_t>(capacity-at),format,values...);
            if(n<0||n>=capacity-at)at=capacity;else at+=n;
        };
        append(",\"split_enabled\":%d,\"split_shared\":%d,\"split_initialized\":%d,\"split_focus\":%d",splitEnabled?1:0,sharedDual?1:0,splitInitialized?1:0,splitFocus);
        append(",\"split_parts\":4,\"split_quad\":%d,\"quad_initialized\":%d",quadMode?1:0,quadInitialized?1:0);
        const char* names[]{"split_single","split_a","split_b","split_c","split_d"};
        for(int group=0;group<5;++group) {
            append(",\"%s\":[",names[group]);
            const auto& c=group==0?(splitEnabled?splitSingleControls:controls)
                :(splitEnabled?liveControlsForPart(group-1):splitParts[static_cast<std::size_t>(group-1)].controls);
            for(std::size_t p=0;p<kLockParameterCount;++p)
                append(p?",%.9g":"%.9g",static_cast<double>(lockedValue(group>0&&splitEnabled&&!splitPartParameter(p)?controls:c,outputGain,p)));
            const auto ui=group==0?(splitEnabled?splitSingleSettings:currentPartSettings())
                :(splitEnabled?settingsForPart(group-1):splitParts[static_cast<std::size_t>(group-1)].settings);
            for(int value:settingsValues(ui))append(",%d",value);
            append("%s","]");
        }
        append("%s","}");return at;
    }

    bool restoreSplitJson(const char* json)
    {
        float enabled=0,initialized=0,focus=0,shared=0,quad=0,quadInit=0,parts=2;
        if(!jsonNumber(json,"split_enabled",enabled))return true;
        jsonNumber(json,"split_shared",shared);jsonNumber(json,"split_quad",quad);jsonNumber(json,"quad_initialized",quadInit);jsonNumber(json,"split_parts",parts);
        if((parts!=2&&parts!=4)||(quad>=.5f&&shared>=.5f))return false;
        const std::size_t storedCount=parts==4?kLockParameterCount:kVersion14LockParameterCount;
        jsonNumber(json,"split_initialized",initialized);jsonNumber(json,"split_focus",focus);
        std::array<Engine::Controls,5> restored{};std::array<PartSettings,5> settings{};
        const char* names[]{"\"split_single\":[","\"split_a\":[","\"split_b\":[","\"split_c\":[","\"split_d\":["};
        for(std::size_t group=0;group<static_cast<std::size_t>(parts+1);++group) {
            const char* text=std::strstr(json,names[group]);if(!text)return false;text+=std::strlen(names[group]);
            float ignoredGain=outputGain;
            for(std::size_t p=0;p<storedCount;++p) {
                char* end=nullptr;const float value=std::strtof(text,&end);
                if(end==text||!std::isfinite(value)||*end!=',')return false;
                applyLockedValue(restored[group],ignoredGain,p,value);text=end+1;
            }
            std::array<int,10> v{};
            for(std::size_t i=0;i<v.size();++i) {
                char* end=nullptr;const long value=std::strtol(text,&end,10);
                if(end==text||value<0||value>60000||*end!=(i+1<v.size()?',':']'))return false;
                v[i]=static_cast<int>(value);text=end+1;
            }
            if(!decodeSettings(v,settings[group]))return false;
        }
        if(parts==2)for(int group=3;group<5;++group){restored[group]=restored[1];settings[group]=settings[1];}
        splitMixerOverrides.fill(false);
        splitSingleControls=restored[0];splitSingleSettings=settings[0];
        for(std::size_t p=0;p<4;++p){splitParts[p].controls=restored[p+1];splitParts[p].settings=settings[p+1];}
        splitEnabled=enabled>=.5f;sharedDual=shared>=.5f;quadMode=quad>=.5f;quadInitialized=quadInit>=.5f;splitInitialized=initialized>=.5f;splitFocus=clampValue(static_cast<int>(focus),0,partCount()-1);
        if(splitEnabled) {
            float ignored=outputGain;
            for(std::size_t p=0;p<kLockParameterCount;++p)if(!splitPartParameter(p))applyLockedValue(controls,ignored,p,lockedValue(restored[1],outputGain,p));
            loadPartEditor(splitFocus);
        }
        for(int part=0;part<4;++part)splitClipControls[part]=liveControlsForPart(part);
        return true;
    }
