    // Arrangement edits share the physical Undo key with clip/sample edits.
    // A short run of edits to one field is one reversible gesture.
    void pushChainUndo(const char* key,int slot,bool force=false) {
        undoHasChainClip=false;
        const auto now=renderClockSamples.load(std::memory_order_relaxed);
        if(force || !undoAvailable || !undoIsChain || chainUndoBank!=chainBankIndex
            || chainUndoSlot!=slot || std::strcmp(chainUndoKey.data(),key)!=0
            || now-chainUndoFrame>static_cast<std::uint64_t>(sampleRate*.5f)) {
            chainUndo=currentChain();chainUndoBank=chainBankIndex;chainUndoSlot=slot;
            std::snprintf(chainUndoKey.data(),chainUndoKey.size(),"%s",key);
        }
        chainUndoFrame=now;undoAvailable=true;undoIsChain=true;
    }
    void restoreChainUndo() {
        if(clipRecording || audioRecording()){setError("Stop recording before undoing");return;}
        if(undoHasChainClip&&clipPreparationInFlight()){setError("Wait: clip or fade");return;}
        if(chainUndoBank<0 || chainUndoBank>=static_cast<int>(kSavedChainCount))return;
        chainBanks[static_cast<std::size_t>(chainUndoBank)]=chainUndo;
        if(chainUndoBank==chainPlayBank) {
            const auto& entries=playingChain().entries;
            if(!entries[static_cast<std::size_t>(chainPosition)].enabled
                || entries[static_cast<std::size_t>(chainPosition)].clip!=activeClip) {
                for(std::size_t i=0;i<entries.size();++i)if(entries[i].enabled && entries[i].clip==activeClip){chainPosition=static_cast<int>(i);break;}
            }
            chainNextSlot=-1;queuedClip=-1;chainRepeat=0;
        }
        restoreChainClipUndo();undoAvailable=false;markPersistentStateDirty();
    }
    void copyChain(bool entry) {
        if(entry){entryClipboard=clipChain()[static_cast<std::size_t>(chainEditSlot)];entryClipboardValid=true;}
        else {chainClipboard=currentChain();chainClipboardValid=true;}
    }
    void pasteChain(bool entry) {
        if(entry?!entryClipboardValid:!chainClipboardValid){setError("Copy a chain or entry first");return;}
        pushChainUndo(entry?"paste entry":"paste chain",entry?chainEditSlot:-1,true);
        if(entry)clipChain()[static_cast<std::size_t>(chainEditSlot)]=entryClipboard;
        else currentChain()=chainClipboard;
        invalidateChainPrefetch();markPersistentStateDirty();
    }
    void cancelChainLaunch() {
        chainLaunchBank=chainLaunchSlot=-1;queuedClip=-1;chainNextSlot=-1;
        if(chainEnabled && !patternBusy())prepareNextChainClip();
    }
    bool queueChainLaunch(int bank,int slot) {
        if(bank<0 || bank>=static_cast<int>(kSavedChainCount) || slot<0 || slot>=static_cast<int>(kClipChainLength))return false;
        if(clipRecording || audioRecording()){setError("Stop recording before launching a chain");return false;}
        const auto& entries=chainBanks[static_cast<std::size_t>(bank)].entries;
        if(!entries[static_cast<std::size_t>(slot)].enabled){setError("Choose an occupied chain entry");return false;}
        chainLaunchBank=bank;chainLaunchSlot=slot;chainNextSlot=-1;
        queuedClip=entries[static_cast<std::size_t>(slot)].clip;
        prepareChainLaunch();return true;
    }
    void prepareChainLaunch() {
        if(chainLaunchBank<0 || patternBusy())return;
        const auto& row=chainBanks[static_cast<std::size_t>(chainLaunchBank)].entries[static_cast<std::size_t>(chainLaunchSlot)];
        if(!row.enabled){cancelChainLaunch();return;}
        queuedClip=row.clip;
        const auto state=static_cast<ClipPrepareState>(clipPrepareState.load(std::memory_order_acquire));
        if(state==ClipPrepareState::ready && preparedClip.load(std::memory_order_relaxed)==queuedClip)return;
        if((state==ClipPrepareState::requested || state==ClipPrepareState::loading)
            && clipPrepareRequest.load(std::memory_order_relaxed)==queuedClip)return;
        requestClipPreparation(queuedClip);
    }
    void launchQueuedChain() {
        if(chainLaunchBank<0 || patternBusy())return;
        const int bank=chainLaunchBank,slot=chainLaunchSlot;
        const auto& row=chainBanks[static_cast<std::size_t>(bank)].entries[static_cast<std::size_t>(slot)];
        if(!row.enabled){cancelChainLaunch();return;}
        if(clipPrepareState.load(std::memory_order_acquire)!=static_cast<int>(ClipPrepareState::ready)
            || preparedClip.load(std::memory_order_relaxed)!=row.clip){prepareChainLaunch();return;}
        ChainFadeSettings fade;const auto* settings=resolveChainFade(slot,fade,bank);
        if(!activatePreparedClip(settings))return;
        chainPlayBank=bank;chainPosition=slot;chainRepeat=0;chainEnabled=true;
        chainLaunchBank=chainLaunchSlot=-1;chainNextSlot=-1;chainAdvanceRequested=false;chainLaunchingFirst=false;
        markPersistentStateDirty();prepareNextChainClip();
    }
    bool setChainPerformanceParam(const char* key,const char* value) {
        if(std::strcmp(key,"chain_clip_copy")==0){clearError();if(isTruthy(value))copyChainClip();return true;}
        if(std::strcmp(key,"chain_clip_paste")==0){clearError();if(isTruthy(value))pasteChainClip();return true;}

        if(std::strcmp(key,"chain_launch") == 0 || std::strcmp(key,"chain_launch_cancel") == 0 || std::strcmp(key,"chain_copy") == 0 || std::strcmp(key,"chain_paste") == 0 || std::strcmp(key,"entry_copy") == 0 || std::strcmp(key,"entry_paste") == 0 || std::strcmp(key,"pattern_fade_return") == 0)clearError();
        if(std::strcmp(key,"chain_launch")==0){if(isTruthy(value))queueChainLaunch(chainBankIndex,chainEditSlot);return true;}
        if(std::strcmp(key,"chain_launch_cancel")==0){if(isTruthy(value))cancelChainLaunch();return true;}
        if(std::strcmp(key,"chain_copy")==0){if(isTruthy(value))copyChain(false);return true;}
        if(std::strcmp(key,"chain_paste")==0){if(isTruthy(value))pasteChain(false);return true;}
        if(std::strcmp(key,"entry_copy")==0){if(isTruthy(value))copyChain(true);return true;}
        if(std::strcmp(key,"entry_paste")==0){if(isTruthy(value))pasteChain(true);return true;}
        if(std::strcmp(key,"pattern_fade_return")==0){if(isTruthy(value))returnPatternSource();return true;}
        return false;
    }
    int getChainPerformanceParam(const char* key,char* buffer,int size) const {
        if(std::strcmp(key,"chain_effective_fade")==0 || std::strcmp(key,"entry_effective_fade")==0) {
            const bool entry=key[0]=='e';const auto& bank=currentChain();const auto& row=bank.entries[static_cast<std::size_t>(chainEditSlot)];
            ChainFadeSettings fade=globalChainFade();int source=0;
            if(bank.savedFade){fade=bank.fade;source=1;}
            if(entry && row.fadeMode!=0){resolveChainFade(chainEditSlot,fade);source=2;}
            return std::snprintf(buffer,size,"%d,%u,%.0f,%.0f,%.0f,%u",source,fade.mode,fade.time,fade.low,fade.high,fade.curve);
        }
        if(std::strcmp(key,"chain_clipboard")==0)return std::snprintf(buffer,size,"%d,%d",chainClipboardValid?1:0,entryClipboardValid?1:0);
        return -2;
    }
    static bool writeChainPlaybackState(std::FILE* file,int playingBank) {
        // Preserve format 19 and the independent performing-chain selection.
        // The removed experimental gesture records are always written empty.
        if(!writeU32(file,0x54524e31u)||!writeU32(file,playingBank))return false;
        for(std::size_t i=0;i<kSavedChainCount*kClipChainLength;++i)
            if(!writeU32(file,0u))return false;
        return true;
    }
    static bool readChainPlaybackState(std::FILE* file,std::uint32_t& playingBank) {
        std::uint32_t tag=0;
        if(!readU32(file,tag)||tag!=0x54524e31u||!readU32(file,playingBank)||playingBank>=kSavedChainCount)return false;
        // Consume and validate test.12 records without retaining or replaying
        // them. Pattern data, chain entries and timed fades remain intact.
        for(std::size_t row=0;row<kSavedChainCount*kClipChainLength;++row) {
            std::uint32_t count=0,enabled=0,source=0,duration=0;
            if(!readU32(file,count)||count>256u||count==1u)return false;
            if(!count)continue;
            if(!readU32(file,enabled)||enabled>1u||!readU32(file,source)||source>=kClipSlotCount
                ||!readU32(file,duration)||duration==0u||duration>122000u)return false;
            std::uint32_t previous=0,lastPosition=0;
            for(std::size_t i=0;i<count;++i) {
                std::uint32_t ms=0,position=0,curve=0;
                if(!readU32(file,ms)||!readU32(file,position)||!readU32(file,curve)
                    ||position>10000u||curve>4u||ms>duration||(i==0?ms!=0:ms<=previous))return false;
                previous=ms;lastPosition=position;
            }
            if(previous!=duration||lastPosition!=10000u)return false;
        }
        return true;
    }
