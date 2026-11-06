    bool bankMode=false,deleteHeld=false;
    float bankMasterRate=1;
    int bankRecordSource=2,bankRecordSourceActive=2;
    void setDeleteHeld(bool held) {
        if(held && !deleteHeld)cancelPerformanceGestures();
        deleteHeld=held;
    }
    std::array<BankSlot,16> bankSlots {};
    std::array<BankSlot,16> bankPrepareSlots {};
    std::shared_ptr<BankPool> bankPool;
    std::array<int,16> bankAssets=[] {std::array<int,16> a{};a.fill(-1);return a;}();
    std::array<int,16> preparedBankAssets=[] {std::array<int,16> a{};a.fill(-1);return a;}();
    int bankPreparedClip=-1,bankLoadedClip=-1,bankSelected=0;
    std::unique_ptr<float[]> bankRecordAudio;
    int bankRecordSlot=-1,bankRecordClip=-1,bankResultAsset=-1;
    std::size_t bankRecordFrames=0;
    std::atomic<int> bankJob {0}; // idle, recording, requested, working, complete, retry
    bool bankRecordSuccess=false;
    bool bankSaveFailed=false,bankAudioArchived=false,bankRecordCollision=false;
    std::uint64_t bankRetryFrame=0;
    std::array<char,kPersistencePathSize> bankRecordPath {};
    bool undoIsBank=false;
    BankSlot bankUndoSlot {};
    int bankUndoIndex=-1,bankUndoClip=-1,bankUndoAsset=-1;

    static void bankUnpin(BankPool& pool,int index) {
        if(index>=0)pool.assets[static_cast<std::size_t>(index)].pins.fetch_sub(1,std::memory_order_release);
    }
    void releaseBankVoice(PlaybackVoice& voice) {
        if(voice.bankAsset>=0 && bankPool)bankUnpin(*bankPool,voice.bankAsset);
        voice.bankAsset=voice.bankSlot=-1;
    }
    void releaseBankIndices(std::array<int,16>& indices) {
        if(bankPool)for(int index:indices)bankUnpin(*bankPool,index);
        indices.fill(-1);
    }
    // Called only by a worker. Pin before examining metadata, so another
    // worker cannot recycle the PCM while it is being inspected or read.
    int loadBankAsset(const char* path,const float* recorded=nullptr,std::size_t frames=0) {
        if(!bankPool || !path[0])return -1;
        for(std::size_t i=0;i<bankPool->assets.size();++i) {
            auto& asset=bankPool->assets[i];int pins=asset.pins.load(std::memory_order_acquire);
            while(pins>=0 && !asset.pins.compare_exchange_weak(pins,pins+1,std::memory_order_acq_rel)){}
            if(pins<0)continue;
            if(asset.frames && std::strcmp(asset.path.data(),path)==0)return static_cast<int>(i);
            bankUnpin(*bankPool,static_cast<int>(i));
        }
        for(std::size_t i=0;i<bankPool->assets.size();++i) {
            auto& asset=bankPool->assets[i];int available=0;
            if(!asset.pins.compare_exchange_strong(available,-1,std::memory_order_acq_rel))continue;
            if(!asset.audio)asset.audio.reset(new(std::nothrow) float[laneCapacityFrames*2]);
            bool ok=bool(asset.audio);asset.frames=0;asset.path[0]=0;
            if(ok && recorded) {
                std::copy_n(recorded,frames,asset.audio.get());
                std::copy_n(recorded+laneCapacityFrames,frames,asset.audio.get()+laneCapacityFrames);
                asset.frames=frames;asset.rate=sampleRate;
            } else if(ok) {
                char label[64]{};
                ok=decodeLibraryWav(path,asset.audio.get(),asset.audio.get()+laneCapacityFrames,
                    asset.frames,asset.rate,label,sizeof(label));
            }
            if(ok && asset.frames>3) {
                std::snprintf(asset.path.data(),asset.path.size(),"%s",path);
                analyzeWaveform(asset.audio.get(),asset.audio.get()+laneCapacityFrames,
                    asset.frames,asset.minimum,asset.maximum);
                asset.pins.store(1,std::memory_order_release);return static_cast<int>(i);
            }
            asset.frames=0;asset.pins.store(0,std::memory_order_release);return -1;
        }
        return -1;
    }
    bool prepareBankSources(int clip,bool requested=false) {
        releaseBankIndices(preparedBankAssets);bankPreparedClip=-1;
        const auto& slots=requested?bankPrepareSlots:clipSlots[clip].bankSlots;
        for(std::size_t i=0;i<16;++i) {
            if(!slots[i].path[0])continue;
            preparedBankAssets[i]=loadBankAsset(slots[i].path.data());
            if(preparedBankAssets[i]<0) {
                releaseBankIndices(preparedBankAssets);setSampleLoadError("Bank sample missing or unreadable");return false;
            }
        }
        bankPreparedClip=clip;return true;
    }
    void activateBankSources(int clip) {
        if(bankPreparedClip!=clip)return;
        releaseBankIndices(bankAssets);bankAssets.swap(preparedBankAssets);
        bankPreparedClip=-1;bankLoadedClip=clip;
    }
    void requestBankSourcesIfNeeded() {
        bool any=false,matched=true;
        for(std::size_t slot=0;slot<16;++slot) {
            const auto* path=bankSlots[slot].path.data();any|=path[0]!=0;
            const int asset=bankAssets[slot];
            matched&=path[0]?(asset>=0 && std::strcmp(bankPool->assets[asset].path.data(),path)==0):asset<0;
        }
        if(!any){releaseBankIndices(bankAssets);bankLoadedClip=activeClip;return;}
        if(matched){bankLoadedClip=activeClip;return;}
        if(bankMode){bankLoadedClip=-1;queuedClip=activeClip;requestClipPreparation(activeClip);}
    }
    void rememberBankUndo(int slot) {
        if(bankUndoAsset>=0)bankUnpin(*bankPool,bankUndoAsset);
        bankUndoSlot=bankSlots[slot];bankUndoIndex=slot;bankUndoClip=activeClip;
        bankUndoAsset=bankAssets[slot];
        if(bankUndoAsset>=0)bankPool->assets[bankUndoAsset].pins.fetch_add(1,std::memory_order_acq_rel);
        undoAvailable=true;undoIsChain=false;undoIsBank=true;
    }
    bool undoBankEdit() {
        if(bankUndoIndex<0)return false;
        if(bankJob.load(std::memory_order_acquire) || clipPreparationInFlight()) {
            setError("Finish Bank loading or capture before undo");return true;
        }
        if(bankUndoClip>=0 && bankUndoClip<static_cast<int>(kClipSlotCount)) {
            clipSlots[bankUndoClip].bankSlots[bankUndoIndex]=bankUndoSlot;
            if(activeClip==bankUndoClip) {
                bankUnpin(*bankPool,bankAssets[bankUndoIndex]);
                bankAssets[bankUndoIndex]=bankUndoAsset;bankUndoAsset=-1;
                bankSlots[bankUndoIndex]=bankUndoSlot;
            }
        }
        if(bankUndoAsset>=0)bankUnpin(*bankPool,bankUndoAsset);
        bankUndoAsset=bankUndoIndex=bankUndoClip=-1;undoAvailable=false;undoIsBank=false;
        markPersistentStateDirty();return true;
    }
    void clearBankSlot(int slot) {
        if(bankJob.load(std::memory_order_acquire)!=0 || clipPreparationInFlight()){setError("Finish Bank loading or capture first");return;}
        if(!bankSlots[slot].path[0])return;
        rememberBankUndo(slot);
        for(auto& voice:voices)if(voice.bankSlot==slot && voice.active) {
            if(voice.held && !voice.clipOwned)captureClipNoteOff(voice.note,voice.sourcePad,voice.part);
            voice.held=false;beginVoiceRelease(voice);
            if(voice.sourcePad>=0){latchedPadNotes[voice.sourcePad]=-1;padMappedNotes[voice.sourcePad]=-1;}
        }
        bankUnpin(*bankPool,bankAssets[slot]);bankAssets[slot]=-1;bankSlots[slot]={};
        captureActiveClip(true);markPersistentStateDirty();
    }
    bool startBankCapture(int slot) {
        if(patternBusy()){setError("Finish pattern fade before Bank capture");return false;}
        if(bankJob.load(std::memory_order_acquire)!=0 || audioRecording() || clipPreparationInFlight()) {
            setError("Finish the current recording first");return false;
        }
        if(!persistenceEnabled || !backgroundStarted){setError("Bank capture needs writable storage");return false;}
        char kind[32]{};std::snprintf(kind,sizeof(kind),"bank-%02d",slot+1);
        if(!allocateCapturePath(activeClip,0,kind,bankRecordPath.data(),bankRecordPath.size())) {
            setError("Cannot create Bank recording");return false;
        }
        bankRecordSlot=slot;bankRecordClip=activeClip;bankRecordFrames=0;
        bankRecordSourceActive=bankRecordSource;
        bankSaveFailed=bankAudioArchived=bankRecordCollision=false;
        bankJob.store(1,std::memory_order_release);clearError();return true;
    }
    void finishBankCapture() {
        if(bankJob.load(std::memory_order_acquire)!=1)return;
        const auto minimumFrames=std::max<std::size_t>(4,
            static_cast<std::size_t>(std::ceil(sampleRate*.010)));
        // Buffer from the initial press, but do not archive an accidental tap.
        if(bankRecordFrames<minimumFrames) {
            bankRecordSlot=-1;bankRecordFrames=0;
            bankJob.store(0,std::memory_order_release);return;
        }
        bankJob.store(2,std::memory_order_release);
    }
    void recordBankBus(std::size_t frames) {
        if(bankJob.load(std::memory_order_acquire)!=1)return;
        const std::size_t count=std::min(frames,laneCapacityFrames-bankRecordFrames);
        // The caller is after both private part inserts and the completed
        // main effects bus, at the same MIX tap as Mix/Rec. Part buses stay
        // pre-level/crossfade; IN already includes recording gain.
        const auto& source=bankRecordSourceActive==0?monitorInput
            :bankRecordSourceActive==1?voiceMix:splitBus[bankRecordSourceActive-2];
        for(std::size_t channel=0;channel<2;++channel)
            std::copy_n(source[channel].data(),count,
                bankRecordAudio.get()+channel*laneCapacityFrames+bankRecordFrames);
        bankRecordFrames+=count;
        if(bankRecordFrames==laneCapacityFrames)finishBankCapture();
    }
    bool processBankJob() {
        if(bankJob.load(std::memory_order_acquire)!=2)return false;
        bankJob.store(3,std::memory_order_release);
        // Commit immutable audio before publishing any slot reference.
        std::array<char,kPersistencePathSize+4> temporary {};
        std::snprintf(temporary.data(),temporary.size(),"%s.tmp",bankRecordPath.data());
        bankRecordCollision=false;
        if(!bankAudioArchived)bankAudioArchived=writeStereoWavFile(bankRecordAudio.get(),bankRecordAudio.get()+laneCapacityFrames,
            bankRecordFrames,sampleRate,bankRecordPath.data(),temporary.data(),true,&bankRecordCollision);
        bankRecordSuccess=bankAudioArchived;
        bankResultAsset=bankRecordSuccess?loadBankAsset(bankRecordPath.data(),bankRecordAudio.get(),bankRecordFrames):-1;
        bankRecordSuccess=bankRecordSuccess && bankResultAsset>=0;
        bankJob.store(4,std::memory_order_release);return true;
    }
    void consumeBankJob() {
        if(bankJob.load(std::memory_order_acquire)==5) {
            if(renderClockSamples.load(std::memory_order_relaxed)>=bankRetryFrame)bankJob.store(2,std::memory_order_release);
            return;
        }
        if(bankJob.load(std::memory_order_acquire)!=4)return;
        if(bankRecordSuccess && bankRecordClip!=activeClip) {
            // A programmatic state restore can change clips outside the pad
            // UI. Keep a completed take attached to its originating clip.
            auto& slot=clipSlots[bankRecordClip].bankSlots[bankRecordSlot];
            std::snprintf(slot.path.data(),slot.path.size(),"%s",bankRecordPath.data());
            bankUnpin(*bankPool,bankResultAsset);bankResultAsset=-1;bankSaveFailed=false;
            bankRecordSlot=-1;bankJob.store(0,std::memory_order_release);
            markPersistentStateDirty();return;
        }
        if(bankRecordSuccess && bankRecordClip==activeClip) {
            rememberBankUndo(bankRecordSlot);
            bankUnpin(*bankPool,bankAssets[bankRecordSlot]);
            bankAssets[bankRecordSlot]=bankResultAsset;bankResultAsset=-1;
            auto& slot=bankSlots[bankRecordSlot];
            std::snprintf(slot.path.data(),slot.path.size(),"%s",bankRecordPath.data());
            bankSaveFailed=false;
            bankLoadedClip=activeClip;captureActiveClip(true);markPersistentStateDirty();clearError();
        } else {
            if(bankResultAsset>=0)bankUnpin(*bankPool,bankResultAsset);
            bankResultAsset=-1;bankSaveFailed=true;
            if(bankRecordCollision) {
                char kind[32]{};std::snprintf(kind,sizeof(kind),"bank-%02d",bankRecordSlot+1);
                allocateCapturePath(bankRecordClip,0,kind,bankRecordPath.data(),bankRecordPath.size());
            }
            bankRetryFrame=renderClockSamples.load(std::memory_order_relaxed)+static_cast<std::uint64_t>(sampleRate);
            bankJob.store(5,std::memory_order_release);
            setError("Bank save failed: retrying; audio retained");return;
        }
        bankRecordSlot=-1;bankJob.store(0,std::memory_order_release);
    }
    void startBankVoice(int slot,int pad,int velocity,bool recorded=false,int noteIndex=-1) {
        bankSelected=slot;
        if(bankLoadedClip!=activeClip || bankAssets[slot]<0){setError("Bank slot not ready");return;}
        const int group=bankSlots[slot].choke;
        if(group)for(auto& other:voices)if(other.active && other.bankAsset>=0
            && bankSlots[other.bankSlot].choke==group) {
            if(other.held && !other.clipOwned)captureClipNoteOff(other.note,other.sourcePad,other.part);
            if(!other.clipOwned)clearPadMappingForVoice(other);
            other.held=false;beginVoiceRelease(other);
        }
        ++noteClock;
        auto& voice=recorded?allocateClipVoice(1):allocateVoice(60+slot,pad);
        const auto tail=voice.bankLast;
        const bool stealing=voice.active;
        const float envelope=stealing?voice.envelope:0;
        const float gain=stealing?voice.gainState:outputGain;
        if(stealing && !voice.clipOwned)clearPadMappingForVoice(voice);
        releaseBankVoice(voice);
        voice.bankSlot=slot;voice.bankAsset=bankAssets[slot];
        bankPool->assets[voice.bankAsset].pins.fetch_add(1,std::memory_order_acq_rel);
        voice.bankRate=bankMasterRate*bankSlots[slot].rate;voice.bankLevel=bankSlots[slot].level;
        voice.bankPan=bankSlots[slot].pan;voice.bankPlay=bankSlots[slot].play;voice.bankEnded=false;
        voice.bankStart=bankSlots[slot].start;voice.bankEnd=bankSlots[slot].end;voice.bankReverse=bankSlots[slot].reverse;
        voice.bankPosition=0;voice.bankTail=tail;voice.bankTailFrames=stealing?voiceStealFadeFrames:0;
        voice.bankLast={};voice.note=60+slot;voice.sourcePad=pad;voice.velocity=velocity;
        voice.age=noteClock;voice.active=voice.held=true;voice.clipOwned=recorded;voice.clipNoteIndex=noteIndex;
        voice.stealPending=voice.panicRelease=false;voice.engineSlot=voice.assignedAlgorithm=-1;
        voice.entryVariation={};voice.pressureTarget=voice.pressureState=voice.panState=0;
        voice.pressureToneMix=0;voice.envelope=envelope;voice.gainState=gain;
        beginVoiceAttack(voice);
        if(!recorded)captureClipNoteOn(voice.note,velocity,pad);
        updateActiveNote();
    }
    bool bankPadOn(int pad,int velocity) {
        if(!bankMode || pad<0 || pad>=32 || pad%8<4)return false;
        const int slot=(pad/8)*4+pad%4;bankSelected=slot;
        if(deleteHeld){clearBankSlot(slot);ignorePadRelease[pad]=true;return true;}
        if(!bankSlots[slot].path[0]) {
            if(!shiftHeld)startBankCapture(slot);
            ignorePadRelease[pad]=false;return true;
        }
        if(shiftHeld && latchedPadNotes[pad]>=0) {
            noteOff(latchedPadNotes[pad],pad);latchedPadNotes[pad]=-1;
            for(auto& v:voices)if(v.active && v.bankSlot==slot && v.sourcePad==pad){v.held=false;beginVoiceRelease(v);}
            ignorePadRelease[pad]=true;return true;
        }
        const bool wasLatched=latchedPadNotes[pad]>=0;
        startBankVoice(slot,pad,velocity);
        padMappedNotes[pad]=60+slot;
        if(shiftHeld || wasLatched){latchedPadNotes[pad]=60+slot;armPadHoldDeadline(pad,renderClockSamples.load(std::memory_order_relaxed));}
        return true;
    }
    bool bankPadOff(int pad) {
        if(!bankMode || pad<0 || pad>=32 || pad%8<4)return false;
        if(bankRecordSlot==(pad/8)*4+pad%4)finishBankCapture();
        if(ignorePadRelease[pad]){ignorePadRelease[pad]=false;return true;}
        if(latchedPadNotes[pad]>=0)return true;
        noteOff(60+(pad/8)*4+pad%4,pad);padMappedNotes[pad]=-1;return true;
    }
    float bankVoicePosition(const PlaybackVoice& voice) const {
        const auto& slot=bankSlots[voice.bankSlot];const auto& asset=bankPool->assets[voice.bankAsset];
        return clampValue(slot.reverse?slot.end-static_cast<float>(voice.bankPosition/asset.frames)
            :slot.start+static_cast<float>(voice.bankPosition/asset.frames),0.f,1.f);
    }
    void renderBankVoice(PlaybackVoice& voice,std::size_t count,Engine::Controls& effective) {
        const auto& slot=bankSlots[voice.bankSlot];const auto& asset=bankPool->assets[voice.bankAsset];
        effective.ampAttackMs=slot.attack;effective.ampReleaseMs=slot.release;effective.ampSustain=1;effective.ampDecayMs=0;
        const auto start=std::min(asset.frames-4,static_cast<std::size_t>(slot.start*asset.frames));
        const auto end=std::max(start+4,std::min(asset.frames,static_cast<std::size_t>(slot.end*asset.frames)));
        const double length=static_cast<double>(end-start);
        const double fade=std::min(length*.25,std::max(2.0,static_cast<double>(slot.fade)*.001*asset.rate));
        if(voice.bankStart!=slot.start || voice.bankEnd!=slot.end || voice.bankReverse!=slot.reverse || voice.bankPlay!=slot.play) {
            voice.bankTail=voice.bankLast;voice.bankTailFrames=voiceStealFadeFrames;
            voice.bankStart=slot.start;voice.bankEnd=slot.end;voice.bankReverse=slot.reverse;voice.bankPlay=slot.play;
        }
        const auto read=[&](double phase,int channel) {
            const double point=clampValue(slot.reverse?end-1-phase:start+phase,
                static_cast<double>(start),static_cast<double>(end-1));
            const auto a=static_cast<std::size_t>(point);
            const auto b=std::min(a+1,end-1);const float fraction=static_cast<float>(point-a);
            const auto* audio=asset.audio.get()+channel*laneCapacityFrames;
            return audio[a]+(audio[b]-audio[a])*fraction;
        };
        for(std::size_t f=0;f<count;++f) {
            voice.bankRate+=(bankMasterRate*slot.rate-voice.bankRate)*gainControlSlew;
            voice.bankPan+=(slot.pan-voice.bankPan)*gainControlSlew;
            voice.bankLevel+=(slot.level-voice.bankLevel)*gainControlSlew;
            if(voice.bankPosition>=length) {
                if(slot.play==1) {
                    if(!voice.bankEnded) {
                        voice.bankEnded=true;
                        if(voice.held && !voice.clipOwned)captureClipNoteOff(voice.note,voice.sourcePad,voice.part);
                        if(voice.held && !voice.clipOwned)clearPadMappingForVoice(voice);
                        voice.held=false;beginVoiceRelease(voice);
                    }
                } else voice.bankPosition=fade+std::fmod(voice.bankPosition-length,std::max(1.0,length-fade));
            }
            for(int channel=0;channel<2;++channel) {
                float value=voice.bankEnded?0:read(voice.bankPosition,channel);
                if(!voice.bankEnded && voice.bankPosition>length-fade && slot.play==0) {
                    const double phase=voice.bankPosition-(length-fade);
                    const float blend=static_cast<float>(phase/fade);
                    value+=(read(phase,channel)-value)*blend;
                }
                if(slot.play==1)value*=static_cast<float>(clampValue((length-voice.bankPosition)/fade,0.0,1.0));
                // Stereo balance keeps the original channels intact at centre.
                const float balance=channel==0?1-std::max(0.f,voice.bankPan):1+std::min(0.f,voice.bankPan);
                value*=voice.bankLevel*balance;
                if(voice.bankTailFrames) {
                    const float blend=1-static_cast<float>(voice.bankTailFrames)/voiceStealFadeFrames;
                    value=voice.bankTail[channel]+(value-voice.bankTail[channel])*blend;
                }
                voice.bankLast[channel]=value;voice.output[channel][f]=value;
            }
            if(voice.bankTailFrames)--voice.bankTailFrames;
            voice.bankPosition+=voice.bankRate*asset.rate/sampleRate;
        }
    }

    bool setBankParam(const char* key,const char* value) {
        if(std::strncmp(key,"bank_",5))return false;
        if(std::strcmp(key,"bank_slot")==0){bankSelected=clampValue(parseInteger(value,1)-1,0,15);return true;}
        if(std::strcmp(key,"bank_clear")==0){if(isTruthy(value))clearBankSlot(bankSelected);return true;}
        const float v=parseFloat(value,0);
        if(std::strcmp(key,"bank_master_rate")==0)bankMasterRate=clampValue(v,.125f,8.f);
        else if(std::strcmp(key,"bank_record_source")==0)bankRecordSource=parseEnum(value,kBankRecordSourceNames,bankRecordSource);
        else {
        auto& s=bankSlots[bankSelected];
        if(std::strcmp(key,"bank_start")==0)s.start=clampValue(v,0.f,std::max(0.f,s.end-.001f));
        else if(std::strcmp(key,"bank_end")==0)s.end=clampValue(v,std::min(1.f,s.start+.001f),1.f);
        else if(std::strcmp(key,"bank_rate")==0)s.rate=clampValue(v,.125f,8.f);
        else if(std::strcmp(key,"bank_pan")==0)s.pan=clampValue(v*.01f,-1.f,1.f);
        else if(std::strcmp(key,"bank_play")==0)s.play=clampValue(parseInteger(value,0),0,1);
        else if(std::strcmp(key,"bank_choke")==0)s.choke=clampValue(parseInteger(value,0),0,4);
        else if(std::strcmp(key,"bank_reverse")==0)s.reverse=isTruthy(value);
        else if(std::strcmp(key,"bank_level")==0)s.level=clampValue(v,0.f,2.f);
        else if(std::strcmp(key,"bank_attack")==0)s.attack=clampValue(v,1.f,10000.f);
        else if(std::strcmp(key,"bank_release")==0)s.release=clampValue(v,5.f,10000.f);
        else if(std::strcmp(key,"bank_fade")==0)s.fade=clampValue(v,2.f,100.f);
        else return false;
        }
        captureActiveClip(true);markPersistentStateDirty();return true;
    }
    int getBankParam(const char* key,char* buffer,int size) {
        const auto& s=bankSlots[bankSelected];
        if(std::strcmp(key,"bank_master_rate")==0)return std::snprintf(buffer,size,"%.6g",bankMasterRate);
        if(std::strcmp(key,"bank_record_source")==0)return std::snprintf(buffer,size,"%d",bankRecordSource);
        if(std::strcmp(key,"bank_pan")==0)return std::snprintf(buffer,size,"%.6g",s.pan*100);
        if(std::strcmp(key,"bank_play")==0)return std::snprintf(buffer,size,"%d",s.play);
        if(std::strcmp(key,"bank_choke")==0)return std::snprintf(buffer,size,"%d",s.choke);
        if(std::strcmp(key,"bank_slot")==0)return std::snprintf(buffer,size,"%d",bankSelected+1);
        const char* keys[]{"bank_start","bank_end","bank_rate","bank_reverse","bank_level","bank_attack","bank_release","bank_fade"};
        const float values[]{s.start,s.end,s.rate,static_cast<float>(s.reverse),s.level,s.attack,s.release,s.fade};
        for(int i=0;i<8;++i)if(std::strcmp(key,keys[i])==0)return std::snprintf(buffer,size,"%.6g",values[i]);
        if(std::strcmp(key,"bank_state")==0) {
            int at=std::snprintf(buffer,size,"{\"selected\":%d,\"slots\":[",bankSelected);
            for(int slot=0;slot<16;++slot) {
                int state=bankSlots[slot].path[0]?(bankLoadedClip==activeClip&&bankAssets[slot]>=0?1:5):0;
                for(const auto& v:voices)if(v.active && v.bankSlot==slot)state=4;
                if(bankRecordSlot==slot && bankJob.load(std::memory_order_acquire))state=bankJob.load()==1?2:3;
                const int n=std::snprintf(buffer+at,size-at,"%s%d",slot?",":"",state);
                if(n<0||n>=size-at)return -1;
                at+=n;
            }
            const int n=std::snprintf(buffer+at,size-at,"]}");return n<0||n>=size-at?-1:at+n;
        }
        if(std::strcmp(key,"bank_waveform")==0) {
            const int index=bankAssets[bankSelected];int at=0;
            for(std::size_t bin=0;bin<kWaveBins;++bin) {
                const int low=index>=0?bankPool->assets[index].minimum[bin]:0;
                const int high=index>=0?bankPool->assets[index].maximum[bin]:0;
                const int n=std::snprintf(buffer+at,size-at,"%s%d,%d",bin?",":"",low,high);
                if(n<0||n>=size-at)return -1;
                at+=n;
            }
            return at;
        }
        return -2;
    }

    static bool writeBankExtension(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips) {
        if(!writeU32(file,0x42414e31))return false;
        for(const auto& clip:clips) {
            if(!writeU32(file,clip.bankMode)||!writeU32(file,clip.singleSettings.voiceMode))return false;
            for(const auto& part:clip.splitSettings)if(!writeU32(file,part.voiceMode))return false;
            for(const auto& s:clip.bankSlots) {
                if(!writeBytes(file,s.path.data(),s.path.size())||!writeF32(file,s.start)||!writeF32(file,s.end)
                    ||!writeF32(file,s.rate)||!writeF32(file,s.level)||!writeF32(file,s.attack)||!writeF32(file,s.release)
                    ||!writeF32(file,s.fade)||!writeU32(file,s.reverse))return false;
            }
        }
        return true;
    }
    static bool readBankExtension(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips) {
        std::uint32_t word=0;
        if(!readU32(file,word)||word!=0x42414e31)return false;
        for(auto& clip:clips) {
            if(!readU32(file,word)||word>1)return false;
            clip.bankMode=word!=0;
            if(clip.bankMode && (!clip.splitEnabled||!clip.sharedDual||clip.quadMode))return false;
            if(!readU32(file,word)||word>2)return false;
            clip.singleSettings.voiceMode=static_cast<int>(word);
            for(auto& part:clip.splitSettings) {
                if(!readU32(file,word)||word>2)return false;
                part.voiceMode=static_cast<int>(word);
            }
            for(auto& s:clip.bankSlots) {
                if(std::fread(s.path.data(),1,s.path.size(),file)!=s.path.size()||s.path.back()!=0
                    ||!readF32(file,s.start)||!readF32(file,s.end)||!readF32(file,s.rate)||!readF32(file,s.level)
                    ||!readF32(file,s.attack)||!readF32(file,s.release)||!readF32(file,s.fade)||!readU32(file,word)||word>1
                    ||s.start<0||s.end>1||s.end<=s.start||s.rate<.125f||s.rate>8||s.level<0||s.level>2
                    ||s.attack<1||s.attack>10000||s.release<5||s.release>10000||s.fade<2||s.fade>100)return false;
                s.reverse=static_cast<int>(word);
            }
        }
        return true;
    }

    // Additive format 33 extension leaves the format 32 sample/rate layout intact.
    static bool writeBankPadExtension(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips) {
        if(!writeU32(file,0x42504431))return false;
        for(const auto& clip:clips) {
            if(!writeF32(file,clip.bankMasterRate)||!writeU32(file,clip.bankRecordSource))return false;
            for(const auto& s:clip.bankSlots)
                if(!writeF32(file,s.pan)||!writeU32(file,s.play)||!writeU32(file,s.choke))return false;
        }
        return true;
    }
    static bool readBankPadExtension(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips,std::uint32_t version) {
        std::uint32_t tag=0,source=0,play=0,choke=0;
        if(!readU32(file,tag)||tag!=0x42504431)return false;
        for(auto& clip:clips) {
            if(!readF32(file,clip.bankMasterRate)||clip.bankMasterRate<.125f||clip.bankMasterRate>8
                ||!readU32(file,source)||source>(version<34u?1u:3u))return false;
            // Format 33 used A=0 / IN=1; format 34 follows Mix/Rec order.
            clip.bankRecordSource=version<34u?(source==0?2:0):static_cast<int>(source);
            for(auto& s:clip.bankSlots) {
                if(!readF32(file,s.pan)||s.pan< -1||s.pan>1||!readU32(file,play)||play>1
                    ||!readU32(file,choke)||choke>4)return false;
                s.play=static_cast<int>(play);s.choke=static_cast<int>(choke);
            }
        }
        return true;
    }
