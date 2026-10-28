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
        return sourcePad >= 0 && sourcePad < 32 ? ((sourcePad % 8) >= 4 ? 1 : 0) : midiPart;
    }

    int localPad(int sourcePad) const
    {
        return splitEnabled && sourcePad >= 0 && sourcePad < 32
            ? (sourcePad / 8) * 4 + sourcePad % 4 : sourcePad;
    }

    bool voiceOwnsLane(const PlaybackVoice& voice, std::size_t lane) const
    {
        return !splitEnabled || static_cast<int>(lane / 2u) == voice.part;
    }

    void selectSplitPart(int part, bool notify = true)
    {
        part = clampValue(part,0,1);
        if (!splitEnabled || part == splitFocus) return;
        storeFocusedPart(); splitFocus = part; loadPartEditor(part);
        if (notify && !engine.recording()) selectedLane = static_cast<std::size_t>(part) * 2u;
        if (notify) markPersistentStateDirty();
    }

    void setSplitMode(bool enabled)
    {
        if (enabled == splitEnabled) return;
        if (engine.recording() || clipRecording) { setError("Stop recording before changing mode"); return; }
        captureActiveClip(false); clipPlaying = false; releaseClipVoices();
        beginPreviewTransitionTail(); previewActive = false;
        if (enabled) {
            splitSingleControls = controls; splitSingleSettings = currentPartSettings();
            if (!splitInitialized) {
                for (auto& part : splitParts) { part.controls = controls; part.settings = splitSingleSettings;
                    part.controls.manualLane = clampValue(part.controls.manualLane,0.0f,1.0f);
                    part.controls.effectOrder=7;part.controls.effectMix=1.0f;part.controls.effectReverbMix=1.0f; }
                splitInitialized = true;
            }
            splitEnabled = true; splitFocus = 0;
            float ignored=outputGain;
            for(std::size_t p=0;p<kLockParameterCount;++p)if(!splitPartParameter(p))
                applyLockedValue(controls,ignored,p,lockedValue(splitParts[0].controls,outputGain,p));
            loadPartEditor(0);
            splitBlendState = controls.splitBlend; splitCurveState = static_cast<float>(controls.splitCurve);
        } else {
            storeFocusedPart(); splitEnabled = false;
            controls = splitSingleControls; applyPartSettings(splitSingleSettings);
        }
        for(auto& insert:splitInserts)insert.reset();
        splitRollWrite={};splitRollFrames={};
        refreshPlaybackVoices();
        for(auto& roll:rolls){roll.active=roll.held=false;roll.envelope=roll.target=0;}
        captureActiveClip(true); markPersistentStateDirty(); clearError();
    }

    void copySplitPart(int source)
    {
        if (!splitEnabled) return;
        source = clampValue(source,0,1); storeFocusedPart();
        const int target = 1-source;
        auto& to = splitParts[static_cast<std::size_t>(target)];
        const auto& from = splitParts[static_cast<std::size_t>(source)];
        for (std::size_t p=0;p<kLockParameterCount;++p)
            if (splitPartParameter(p)) applyLockedValue(to.controls,outputGain,p,lockedValue(from.controls,outputGain,p));
        for (int lane=0;lane<2;++lane) to.controls.laneGains[target*2+lane] = from.controls.laneGains[source*2+lane];
        to.settings = from.settings;
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
        clip.splitEnabled = splitEnabled; clip.splitInitialized = splitInitialized; clip.splitFocus = splitFocus;
        clip.singleSettings = splitEnabled ? splitSingleSettings : currentPartSettings();
        if (!splitEnabled) splitSingleControls = controls;
        for (std::size_t p=0;p<kLockParameterCount;++p) {
            clip.baseValues[p] = lockedValue(splitEnabled ? splitSingleControls : controls,outputGain,p);
            for (int part=0;part<2;++part) {
                const auto& c = splitParts[static_cast<std::size_t>(part)].controls;
                clip.splitValues[static_cast<std::size_t>(part)][p] = lockedValue(
                    splitEnabled && !splitPartParameter(p) ? controls : c,outputGain,p);
            }
        }
        for (int part=0;part<2;++part) clip.splitSettings[static_cast<std::size_t>(part)] = splitParts[static_cast<std::size_t>(part)].settings;
    }

    void restoreSplitClip(const ClipSlot& clip)
    {
        splitSingleControls = controls; splitSingleSettings = clip.singleSettings;
        splitEnabled = clip.splitEnabled; splitInitialized = clip.splitInitialized; splitFocus = clampValue(clip.splitFocus,0,1);
        for (int part=0;part<2;++part) {
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
        splitClipControls[0] = liveControlsForPart(0); splitClipControls[1] = liveControlsForPart(1);
    }

    int formatSplitState(char* buffer, int capacity) const
    {
        const auto a=settingsForPart(0), b=settingsForPart(1);
        const auto position=[&](int part) { const PlaybackVoice* found=nullptr;
            for(const auto& v:voices)if(v.active&&v.held&&v.part==part&&(!found||v.age>found->age))found=&v;
            return found?found->engine.normalizedPosition():0.0f; };
        return std::snprintf(buffer,capacity,
            "{\"enabled\":%d,\"focus\":%d,\"blend\":%.5f,\"parts\":["
            "{\"algorithm\":%d,\"root\":%d,\"octave\":%d,\"scale\":%d,\"mode\":%d,\"hold\":%d,\"mute\":%d,\"position\":%.5f},"
            "{\"algorithm\":%d,\"root\":%d,\"octave\":%d,\"scale\":%d,\"mode\":%d,\"hold\":%d,\"mute\":%d,\"position\":%.5f}]}",
            splitEnabled?1:0,splitFocus,splitBlendState,
            static_cast<int>(liveControlsForPart(0).algorithm),a.root,a.octave,a.scale,a.mode,a.stackHold,a.muted,position(0),
            static_cast<int>(liveControlsForPart(1).algorithm),b.root,b.octave,b.scale,b.mode,b.stackHold,b.muted,position(1));
    }

    void clearSplitHoldKeys(int part)
    {
        for(std::size_t key=0;key<liveStackHoldPhaseValid.size();++key) {
            const int owner=key<32u?partForInput(static_cast<int>(key)):(key>=160u?1:0);
            if(owner==part)liveStackHoldPhaseValid[key]=false;
        }
    }

    void processSplitBuses(std::size_t frames, const std::array<Engine::Controls,2u>& live,
                           const Engine::Controls& recorded)
    {
        const Engine::Controls& mixer = clipPlaying ? recorded : controls;
        for (std::size_t part=0;part<2;++part) {
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
        const float targetBlend = clipPlaying && renderClockSamples.load() >= splitBlendOverrideUntil
            ? mixer.splitBlend : controls.splitBlend;
        const float levels[2] {mixer.splitLevelA,mixer.splitLevelB};
        const float delay[2] {mixer.splitDelayA,mixer.splitDelayB};
        const float rev[2] {mixer.splitReverbA,mixer.splitReverbB};
        for(std::size_t frame=0;frame<frames;++frame) {
            splitBlendState+=(targetBlend-splitBlendState)*effectMixSlew;
            if(std::abs(targetBlend-splitBlendState)<1e-6f)splitBlendState=targetBlend;
            splitCurveState+=(static_cast<float>(mixer.splitCurve)-splitCurveState)*effectControlSlew;
            const float linear[2] {1-splitBlendState,splitBlendState};
            const float power[2] {std::cos(splitBlendState*1.57079632679f),std::sin(splitBlendState*1.57079632679f)};
            float gains[2] {};
            for(std::size_t part=0;part<2;++part) {
                const float level=settingsForPart(static_cast<int>(part)).muted?0:levels[part];
                splitLevelState[part]+=(level-splitLevelState[part])*effectMixSlew;
                splitDelayState[part]+=(delay[part]-splitDelayState[part])*effectMixSlew;
                splitReverbState[part]+=(rev[part]-splitReverbState[part])*effectMixSlew;
                gains[part]=std::max(0.0f,linear[part]+(power[part]-linear[part])*splitCurveState)*splitLevelState[part];
                splitFrameGain[part][frame]=gains[part];
            }
            for(std::size_t channel=0;channel<2;++channel) {
                const float a=splitBus[0][channel][frame]*gains[0];
                const float b=splitBus[1][channel][frame]*gains[1];
                voiceMix[channel][frame]=a+b;
                splitDelayInput[channel][frame]=a*splitDelayState[0]+b*splitDelayState[1];
                splitReverbInput[channel][frame]=a*splitReverbState[0]+b*splitReverbState[1];
            }
        }
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
        if(!writeU32(file,0x53504c31u))return false;
        for(const auto& clip:clips) {
            if(!writeU32(file,clip.splitEnabled?1u:0u)||!writeU32(file,clip.splitInitialized?1u:0u)
               ||!writeU32(file,static_cast<std::uint32_t>(clip.splitFocus))||!writePartSettings(file,clip.singleSettings))return false;
            for(std::size_t part=0;part<2;++part) {
                for(float value:clip.splitValues[part])if(!writeF32(file,value))return false;
                if(!writePartSettings(file,clip.splitSettings[part]))return false;
            }
        }
        return true;
    }
    static bool readSplitExtension(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips)
    {
        std::uint32_t magic=0;if(!readU32(file,magic)||magic!=0x53504c31u)return false;
        for(auto& clip:clips) {
            std::uint32_t mode=0,initialized=0,focus=0;
            if(!readU32(file,mode)||mode>1||!readU32(file,initialized)||initialized>1||!readU32(file,focus)||focus>1
               ||!readPartSettings(file,clip.singleSettings))return false;
            clip.splitEnabled=mode!=0;clip.splitInitialized=initialized!=0;clip.splitFocus=static_cast<int>(focus);
            for(std::size_t part=0;part<2;++part) {
                for(float& value:clip.splitValues[part])if(!readF32(file,value))return false;
                if(!readPartSettings(file,clip.splitSettings[part]))return false;
            }
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
        append(",\"split_enabled\":%d,\"split_initialized\":%d,\"split_focus\":%d",splitEnabled?1:0,splitInitialized?1:0,splitFocus);
        const char* names[]{"split_single","split_a","split_b"};
        for(int group=0;group<3;++group) {
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
        float enabled=0,initialized=0,focus=0;
        if(!jsonNumber(json,"split_enabled",enabled))return true;
        jsonNumber(json,"split_initialized",initialized);jsonNumber(json,"split_focus",focus);
        std::array<Engine::Controls,3> restored{};std::array<PartSettings,3> settings{};
        const char* names[]{"\"split_single\":[","\"split_a\":[","\"split_b\":["};
        for(std::size_t group=0;group<3;++group) {
            const char* text=std::strstr(json,names[group]);if(!text)return false;text+=std::strlen(names[group]);
            float ignoredGain=outputGain;
            for(std::size_t p=0;p<kLockParameterCount;++p) {
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
        splitSingleControls=restored[0];splitSingleSettings=settings[0];
        for(std::size_t p=0;p<2;++p){splitParts[p].controls=restored[p+1];splitParts[p].settings=settings[p+1];}
        splitEnabled=enabled>=.5f;splitInitialized=initialized>=.5f;splitFocus=clampValue(static_cast<int>(focus),0,1);
        if(splitEnabled) {
            float ignored=outputGain;
            for(std::size_t p=0;p<kLockParameterCount;++p)if(!splitPartParameter(p))applyLockedValue(controls,ignored,p,lockedValue(restored[1],outputGain,p));
            loadPartEditor(splitFocus);
        }
        splitClipControls[0]=liveControlsForPart(0);splitClipControls[1]=liveControlsForPart(1);
        return true;
    }
