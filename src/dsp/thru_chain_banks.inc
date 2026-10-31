    // Stored arrangements share the existing pattern/source banks. Audio and
    // timing defaults are resolved once when an entry actually launches.
    ChainFadeSettings globalChainFade() const {
        ChainFadeSettings f;f.time=patternFadeMs;f.low=patternFadeLowMs;f.high=patternFadeHighMs;
        f.mode=static_cast<std::uint8_t>(patternTimingMode);f.curve=static_cast<std::uint8_t>(patternCurve);return f;
    }
    const ChainFadeSettings* resolveChainFade(int row,ChainFadeSettings& result,int bankIndex=-1) const {
        const auto& bank=chainBanks[static_cast<std::size_t>(bankIndex<0?chainBankIndex:bankIndex)];const auto& entry=bank.entries[static_cast<std::size_t>(row)];
        if(entry.fadeMode==0) {
            if(!bank.savedFade)return nullptr;
            result=bank.fade;
        } else {
            result=entry.fade;result.mode=entry.fadeMode==3?1:0;
            if(entry.fadeMode==1)result.time=0;
        }
        return &result;
    }
    void invalidateChainPrefetch() {
        if(!chainEnabled || chainBankIndex!=chainPlayBank)return;
        chainNextSlot=-1;queuedClip=-1;
        if(chainLaunchingFirst)chainNextSlot=std::max(0,firstEnabledChainSlot());
    }
    void selectChainBank(int bank) {
        bank=clampValue(bank,0,static_cast<int>(kSavedChainCount)-1);if(bank==chainBankIndex)return;
        chainEditSlot=0;chainBankIndex=bank;markPersistentStateDirty();
    }
    void setChainEnabled(bool enabled) {
        if(enabled==chainEnabled){if(enabled && chainBankIndex!=chainPlayBank)queueChainLaunch(chainBankIndex,chainEditSlot);return;}
        if(!enabled)chainLaunchBank=chainLaunchSlot=-1;
        chainEnabled=enabled;chainNextSlot=-1;chainAdvanceRequested=false;chainLaunchingFirst=false;
        queuedClip=-1;chainRepeat=0;
        if(enabled) {
            chainPlayBank=chainBankIndex;
            const int first=firstEnabledChainSlot();
            if(first>=0) {
                chainPosition=first;
                if(clipChain()[static_cast<std::size_t>(first)].clip!=activeClip) {
                    chainLaunchingFirst=true;chainNextSlot=first;
                }
            }
        }
        markPersistentStateDirty();
    }
    bool setChainBankParam(const char* key,const char* value) {
        if(std::strcmp(key,"chain_bank")==0) {selectChainBank(parseInteger(value,chainBankIndex+1)-1);return true;}
        if(std::strcmp(key,"chain_end")==0) {const int end=clampValue(parseInteger(value,0),0,2);if(end==(currentChain().loop?0:currentChain().stop?2:1))return true;pushChainUndo("end",-1);currentChain().loop=end==0;currentChain().stop=end==2;invalidateChainPrefetch();markPersistentStateDirty();return true;}
        if(std::strcmp(key,"chain_advance")==0) {if(chainEnabled && isTruthy(value)){chainAdvanceRequested=true;prepareNextChainClip();}return true;}
        const bool entry=std::strncmp(key,"entry_fade_",11)==0;
        if(!entry && std::strncmp(key,"chain_fade_",11)!=0)return false;

        const char* suffix=key+11;auto& bank=currentChain();auto& row=clipChain()[static_cast<std::size_t>(chainEditSlot)];
        auto& f=entry?row.fade:bank.fade;
        const int oldMode=entry?row.fadeMode:bank.savedFade?bank.fade.mode+1:0;
        if((std::strcmp(suffix,"mode")==0&&oldMode==clampValue(parseInteger(value,oldMode),0,entry?3:2))
            ||(std::strcmp(suffix,"ms")==0&&f.time==clampValue(parseFloat(value,f.time),0.0f,kMaximumPatternFadeMs))
            ||(std::strcmp(suffix,"low_ms")==0&&f.low==std::round(clampValue(parseFloat(value,f.low),0.0f,kMaximumPatternFadeMs)))
            ||(std::strcmp(suffix,"high_ms")==0&&f.high==std::round(clampValue(parseFloat(value,f.high),0.0f,kMaximumPatternFadeMs)))
            ||(std::strcmp(suffix,"curve")==0&&f.curve==clampValue(parseInteger(value,f.curve),0,4)))return true;
        pushChainUndo(key,entry?chainEditSlot:-1);
        if(std::strcmp(suffix,"mode")==0) {
            const int mode=clampValue(parseInteger(value,0),0,entry?3:2);
            if(entry) {
                if(row.fadeMode==0 && mode!=0)f=bank.savedFade?bank.fade:globalChainFade();
                row.fadeMode=static_cast<std::uint8_t>(mode);f.mode=mode==3?1:0;
            } else {
                if(!bank.savedFade && mode!=0)f=globalChainFade();
                bank.savedFade=mode!=0;f.mode=mode==2?1:0;
            }
        } else if(std::strcmp(suffix,"ms")==0)f.time=clampValue(parseFloat(value,f.time),0.0f,kMaximumPatternFadeMs);
        else if(std::strcmp(suffix,"low_ms")==0) {f.low=std::round(clampValue(parseFloat(value,f.low),0.0f,kMaximumPatternFadeMs));f.high=std::max(f.low,f.high);}
        else if(std::strcmp(suffix,"high_ms")==0) {f.high=std::round(clampValue(parseFloat(value,f.high),0.0f,kMaximumPatternFadeMs));f.low=std::min(f.low,f.high);}
        else if(std::strcmp(suffix,"curve")==0)f.curve=static_cast<std::uint8_t>(clampValue(parseInteger(value,f.curve),0,4));
        else return false;
        markPersistentStateDirty();return true;
    }
    int getChainBankParam(const char* key,char* buffer,int size) const {
        if(std::strcmp(key,"chain_fades_state")==0) {
            const auto& b=currentChain();const auto& e=clipChain()[static_cast<std::size_t>(chainEditSlot)];
            return std::snprintf(buffer,size,"%d,%.0f,%.0f,%.0f,%u,%u,%.0f,%.0f,%.0f,%u",
                b.savedFade?b.fade.mode+1:0,b.fade.time,b.fade.low,b.fade.high,b.fade.curve,
                e.fadeMode,e.fade.time,e.fade.low,e.fade.high,e.fade.curve);
        }
        if(std::strcmp(key,"chain_bank")==0)return std::snprintf(buffer,size,"%d",chainBankIndex+1);
        if(std::strcmp(key,"chain_end")==0)return std::snprintf(buffer,size,"%d",currentChain().loop?0:currentChain().stop?2:1);
        if(std::strcmp(key,"chain_advance")==0)return std::snprintf(buffer,size,"%d",chainAdvanceRequested?1:0);
        const bool entry=std::strncmp(key,"entry_fade_",11)==0;
        if(!entry && std::strncmp(key,"chain_fade_",11)!=0)return -2;
        const char* suffix=key+11;const auto& bank=currentChain();const auto& row=clipChain()[static_cast<std::size_t>(chainEditSlot)];
        const auto& f=entry?row.fade:bank.fade;
        if(std::strcmp(suffix,"mode")==0)return std::snprintf(buffer,size,"%d",entry?row.fadeMode:bank.savedFade?bank.fade.mode+1:0);
        if(std::strcmp(suffix,"ms")==0)return std::snprintf(buffer,size,"%.0f",f.time);
        if(std::strcmp(suffix,"low_ms")==0)return std::snprintf(buffer,size,"%.0f",f.low);
        if(std::strcmp(suffix,"high_ms")==0)return std::snprintf(buffer,size,"%.0f",f.high);
        if(std::strcmp(suffix,"curve")==0)return std::snprintf(buffer,size,"%u",f.curve);
        return -2;
    }
    static bool writeChainFade(std::FILE* file,const ChainFadeSettings& fade) {
        return writeF32(file,fade.time) && writeF32(file,fade.low) && writeF32(file,fade.high) && writeU32(file,fade.curve);
    }
    static bool readChainFade(std::FILE* file,ChainFadeSettings& fade) {
        std::uint32_t curve=0;
        if(!readF32(file,fade.time)||!readF32(file,fade.low)||!readF32(file,fade.high)||!readU32(file,curve)
            || fade.time<0 || fade.time>30000 || fade.low<0 || fade.high>30000 || fade.low>fade.high || curve>4)return false;
        fade.curve=static_cast<std::uint8_t>(curve);return true;
    }
    static bool writeChainBanks(std::FILE* file,const SavedChainBanks& banks,int selected) {
        if(!writeU32(file,0x43484e31u)||!writeU32(file,kSavedChainCount)||!writeU32(file,kClipChainLength)||!writeU32(file,selected))return false;
        for(const auto& bank:banks) {
            if(!writeU32(file,bank.loop?1:bank.stop?2:0)||!writeU32(file,bank.savedFade?bank.fade.mode+1:0)||!writeChainFade(file,bank.fade))return false;
            for(const auto& row:bank.entries)
                if(!writeU32(file,row.enabled?1:0)||!writeU32(file,row.clip)||!writeU32(file,row.repeats)
                    ||!writeU32(file,row.fadeMode)||!writeChainFade(file,row.fade))return false;
        }
        return true;
    }
    static bool readChainBanks(std::FILE* file,SavedChainBanks& banks,std::uint32_t& selected) {
        std::uint32_t tag=0,count=0,length=0;
        if(!readU32(file,tag)||tag!=0x43484e31u||!readU32(file,count)||count!=kSavedChainCount
            ||!readU32(file,length)||length!=kClipChainLength||!readU32(file,selected)||selected>=kSavedChainCount)return false;
        for(auto& bank:banks) {
            std::uint32_t loop=0,mode=0;
            if(!readU32(file,loop)||loop>2||!readU32(file,mode)||mode>2||!readChainFade(file,bank.fade))return false;
            bank.loop=loop==1;bank.stop=loop==2;bank.savedFade=mode!=0;bank.fade.mode=mode==2?1:0;
            for(auto& row:bank.entries) {
                std::uint32_t enabled=0,clip=0,repeats=0,fadeMode=0;
                if(!readU32(file,enabled)||enabled>1||!readU32(file,clip)||clip>=kClipSlotCount
                    ||!readU32(file,repeats)||repeats>16||!readU32(file,fadeMode)||fadeMode>3||!readChainFade(file,row.fade))return false;
                row.enabled=enabled!=0;row.clip=static_cast<std::uint8_t>(clip);row.repeats=static_cast<std::uint8_t>(repeats);
                row.fadeMode=static_cast<std::uint8_t>(fadeMode);row.fade.mode=fadeMode==3?1:0;
            }
        }
        return true;
    }
