    using CustomBinding=s3g_thru_custom_midi::Binding;
    using CustomProfile=s3g_thru_custom_midi::Profile;
    using CustomMapState=s3g_thru_custom_midi::State;
    CustomMapState customMaps{};
    CustomProfile customFactory{};
    struct CustomPickup {
        int id=-1,part=-2,last=-1,written=-1;
        bool engaged=false,down=false;
    };
    std::array<std::array<CustomPickup,s3g_thru_custom_midi::bindingCount>,2> customPickups{};
    struct CustomSaveSnapshot {CustomMapState state;std::uint32_t revision=0;bool success=false;};
    std::unique_ptr<CustomSaveSnapshot> customSaveSnapshot;
    std::atomic<int> customSaveJob{0}; // Idle, Requested, Working, Complete.
    bool customDirty=false,customSaveFailed=false,customRecovered=false;
    std::uint32_t customRevision=1,customUiRevision=1;
    std::uint64_t customEditFrame=0,customRetryFrame=0;
    CustomMapState customUndo{};
    bool customUndoValid=false;
    CustomBinding customDraft{};
    int customDraftIndex=-1,customConflict=-1,customResult=0,customLearnFilter=0;
    bool customEditing=false,customLearn=false;
    std::uint64_t customLearnUntil=0;
    static std::uint64_t customClock() {
        timespec now{};clock_gettime(CLOCK_MONOTONIC,&now);
        return static_cast<std::uint64_t>(now.tv_sec)*1000+static_cast<std::uint64_t>(now.tv_nsec)/1000000;
    }
    CustomProfile* currentCustomProfile() {
        if(customMaps.active>=1&&customMaps.active<=4)return &customMaps.profiles[customMaps.active-1];
        if(customMaps.active==5){customFactory.bank=customMaps.factoryBank;return &customFactory;}
        return nullptr;
    }
    const CustomProfile* currentCustomProfile() const {
        if(customMaps.active>=1&&customMaps.active<=4)return &customMaps.profiles[customMaps.active-1];
        return customMaps.active==5?&customFactory:nullptr;
    }
    bool customEditable() const {return customMaps.active>=1&&customMaps.active<=4;}
    bool validCustomBinding(const CustomBinding& b) const {
        if(!s3g_thru_custom_midi::valid(b))return false;
        if(!b.enabled||b.kind!=s3g_thru_custom_midi::parameter)return true;
        const auto* p=s3g_thru_midi::find(b.target);
        return p&&(p->part?b.part>=0:b.part== -1)&&(!p->action||b.behavior==s3g_thru_custom_midi::trigger);
    }
    void resetCustomPickups() {
        for(auto& input:customPickups)for(auto& p:input)p={};
        for(auto& input:midiDecoders)for(auto& decoder:input)decoder={};
    }
    void cancelCustomEdit() {
        customEditing=customLearn=false;customLearnUntil=0;customConflict=-1;customResult=0;++customUiRevision;
    }
    void markCustomDirty() {
        customDirty=true;customEditFrame=renderClockSamples.load(std::memory_order_relaxed);
        ++customRevision;++customUiRevision;
    }
    void rememberCustomUndo() {customUndo=customMaps;customUndoValid=true;}
    void selectCustomProfile(int profile) {
        profile=clampValue(profile,0,5);
        if(profile==customMaps.active)return;
        cancelCustomEdit();customMaps.active=static_cast<std::uint8_t>(profile);
        currentCustomProfile();resetCustomPickups();markCustomDirty();
    }
    void selectCustomBank(int bank) {
        bank=clampValue(bank,0,7);auto* p=currentCustomProfile();if(!p||bank==p->bank)return;
        p->bank=static_cast<std::uint8_t>(bank);if(customMaps.active==5)customMaps.factoryBank=p->bank;
        resetCustomPickups();markCustomDirty();
    }
    void expireCustomLearn() {
        if(customLearn&&customClock()>=customLearnUntil){customLearn=false;customResult=7;++customUiRevision;}
    }
    // Called before note-expression, NRPN and built-in CC routing. Only
    // explicitly owned CCs are consumed, and Learning never writes a sound value.
    bool learnCustomCc(int channel,int number,int value) {
        expireCustomLearn();
        if(customLearn&&(customLearnFilter==0||customLearnFilter==channel)&&number<120) {
            customDraft.channel=static_cast<std::uint8_t>(channel);customDraft.cc=static_cast<std::uint8_t>(number);
            customLearn=false;customResult=1;++customUiRevision;return true;
        }
        (void)value;
        return customEditing&&customEditable()&&channel==customDraft.channel&&number==customDraft.cc;
    }
    bool setCustomMidiParameter(const char* key,const char* value) {
        if(std::strncmp(key,"midi_custom_",12))return false;
        using namespace s3g_thru_custom_midi;
        if(!std::strcmp(key,"midi_custom_profile")){selectCustomProfile(parseInteger(value,0));return true;}
        if(!std::strcmp(key,"midi_custom_bank")){selectCustomBank(parseInteger(value,1)-1);return true;}
        if(!std::strcmp(key,"midi_custom_cancel")){cancelCustomEdit();return true;}
        if(!std::strcmp(key,"midi_custom_begin")) {
            auto* p=currentCustomProfile();if(!p){customResult=8;return true;}
            const int index=parseInteger(value,-1);if(index< -1||index>=static_cast<int>(bindingCount))return true;
            if(index<0&&!customEditable()){customResult=8;return true;}
            cancelCustomEdit();customDraftIndex=index;customEditing=true;
            customDraft=index>=0?p->bindings[index]:CustomBinding{};
            if(index<0){customDraft.enabled=1;customDraft.target=0;customDraft.part=0;}
            ++customUiRevision;return true;
        }
        if(!std::strcmp(key,"midi_custom_copy")) {
            const int destination=parseInteger(value,0);if(destination<1||destination>4)return true;
            auto* source=currentCustomProfile();if(!source){customResult=8;return true;}
            const auto copied=*source;rememberCustomUndo();customMaps.profiles[destination-1]=copied;
            customMaps.active=static_cast<std::uint8_t>(destination);cancelCustomEdit();resetCustomPickups();markCustomDirty();return true;
        }
        if(!std::strcmp(key,"midi_custom_clear")) {
            if(!customEditable()){customResult=8;return true;}
            rememberCustomUndo();*currentCustomProfile()=CustomProfile{};cancelCustomEdit();resetCustomPickups();markCustomDirty();return true;
        }
        if(!std::strcmp(key,"midi_custom_undo")) {
            if(customUndoValid){customMaps=customUndo;customUndoValid=false;cancelCustomEdit();currentCustomProfile();resetCustomPickups();markCustomDirty();}
            return true;
        }
        if(!std::strcmp(key,"midi_custom_delete")) {
            if(!customEditable()){customResult=8;return true;}
            const int index=parseInteger(value,-1);if(index<0||index>=static_cast<int>(bindingCount))return true;
            rememberCustomUndo();currentCustomProfile()->bindings[index]=CustomBinding{};
            cancelCustomEdit();resetCustomPickups();markCustomDirty();return true;
        }
        if(!customEditing||!customEditable()){customResult=8;return true;}
        if(!std::strcmp(key,"midi_custom_learn_filter")){customLearnFilter=clampValue(parseInteger(value,0),0,16);return true;}
        if(!std::strcmp(key,"midi_custom_learn")) {
            customLearn=parseInteger(value,0)!=0;customLearnUntil=customClock()+15000;customResult=0;++customUiRevision;return true;
        }
        if(!std::strcmp(key,"midi_custom_draft")) {
            int ch=0,cc=0,part=0,kind=0,target=0,page=0,slot=0,behavior=0,pickup=0,invert=0,bank=0,n=0;
            float low=0,high=1;
            if(std::sscanf(value,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%f,%f,%d%n",
                &ch,&cc,&part,&kind,&target,&page,&slot,&behavior,&pickup,&invert,&low,&high,&bank,&n)!=13||value[n]){customResult=4;return true;}
            if(ch<1||ch>16||cc<0||cc>127||part< -1||part>4||kind<0||kind>2
                ||target< -1||target>16383||page<0||page>3||slot<0||slot>7
                ||behavior<0||behavior>3||pickup<0||pickup>1||invert<0||invert>1||bank<0||bank>8){customResult=4;return true;}
            CustomBinding b;b.enabled=1;b.channel=ch;b.cc=cc;b.part=part;b.kind=kind;b.target=target;
            b.page=page;b.slot=slot;b.behavior=behavior;b.pickup=pickup;b.invert=invert;b.low=low;b.high=high;b.bank=bank;
            if(b.kind==parameter)if(const auto* p=s3g_thru_midi::find(b.target)) {
                if(!p->part)b.part=-1;
                if(p->action)b.behavior=trigger;
            }
            if(b.kind==engineSlot&&b.part<0)b.part=0;
            if(b.kind==command){b.part=-1;b.behavior=trigger;}
            if(!validCustomBinding(b)){customResult=4;return true;}
            customDraft=b;customConflict=-1;customResult=0;++customUiRevision;return true;
        }
        if(!std::strcmp(key,"midi_custom_apply")) {
            if(!validCustomBinding(customDraft)){customResult=4;return true;}
            auto& profile=*currentCustomProfile();const int other=conflict(profile,customDraft,customDraftIndex);
            if(other>=0&&parseInteger(value,0)!=2){customConflict=other;customResult=2;++customUiRevision;return true;}
            const int index=customDraftIndex>=0?customDraftIndex:other>=0?other:firstFree(profile);
            if(index<0){customResult=3;++customUiRevision;return true;}
            rememberCustomUndo();if(other>=0&&other!=index)profile.bindings[other]=CustomBinding{};
            profile.bindings[index]=customDraft;cancelCustomEdit();customDraftIndex=index;customResult=5;
            resetCustomPickups();markCustomDirty();return true;
        }
        return true;
    }
    int formatCustomMidiState(char* out,int size) {
        expireCustomLearn();const auto* p=currentCustomProfile();int count=0;
        if(p)for(const auto& b:p->bindings)count+=b.enabled?1:0;
        const auto& b=customDraft;
        return std::snprintf(out,size,"{\"v\":1,\"profile\":%u,\"bank\":%u,\"count\":%d,\"revision\":%u,\"ui\":%u,\"editing\":%d,\"index\":%d,\"learn\":%d,\"filter\":%d,\"result\":%d,\"conflict\":%d,\"undo\":%d,\"save\":%d,\"recovered\":%d,\"mode\":%d,\"draft\":[%u,%u,%d,%u,%d,%u,%u,%u,%u,%u,%.9g,%.9g,%u]}",
            customMaps.active,p?p->bank+1:1,count,customRevision,customUiRevision,customEditing?1:0,customDraftIndex,
            customLearn?1:0,customLearnFilter,customResult,customConflict,customUndoValid?1:0,
            !persistenceEnabled?0:customSaveFailed?3:customDirty||customSaveJob.load()?1:2,customRecovered?1:0,midiControlMode,
            b.channel,b.cc,b.part,b.kind,b.target,b.page,b.slot,b.behavior,b.pickup,b.invert,
            static_cast<double>(b.low),static_cast<double>(b.high),b.bank);
    }
    int formatCustomMidiEntries(char* out,int size) const {
        int at=std::snprintf(out,size,"[");if(at<0||at>=size)return -1;
        bool comma=false;const auto* p=currentCustomProfile();
        if(p)for(std::size_t i=0;i<p->bindings.size();++i) {
            const auto& b=p->bindings[i];if(!b.enabled)continue;
            const int n=std::snprintf(out+at,size-at,"%s[%zu,%u,%u,%d,%u,%d,%u,%u,%u]",comma?",":"",i,b.channel,b.cc,b.part,b.kind,b.target,b.page,b.slot,b.bank);
            if(n<0||n>=size-at)return -1;
            at+=n;comma=true;
        }
        const int n=std::snprintf(out+at,size-at,"]");return n<0||n>=size-at?-1:at+n;
    }
    bool writeCustomMidiFile(const CustomMapState& state) const {
        if(!persistenceEnabled)return false;
        std::array<char,kPersistencePathSize> path{},temp{};
        if(!controllerDataPath("thru-midi-maps.bin",path.data(),path.size())
            ||!controllerDataPath("thru-midi-maps.bin.tmp",temp.data(),temp.size()))return false;
        auto* file=std::fopen(temp.data(),"wb");if(!file)return false;
        bool ok=writeBytes(file,"THRUMAP1",8)&&writeU32(file,1)&&writeU32(file,4)&&writeU32(file,128)
            &&writeU32(file,state.active)&&writeU32(file,state.factoryBank);
        for(const auto& p:state.profiles) {
            ok=ok&&writeU32(file,p.bank);
            for(const auto& b:p.bindings) {
                const std::uint32_t words[]{b.enabled,b.channel,b.cc,b.bank,b.kind,static_cast<std::uint32_t>(b.target+1),
                    static_cast<std::uint32_t>(b.part+1),b.page,b.slot,b.behavior,b.pickup,b.invert};
                for(auto word:words)ok=ok&&writeU32(file,word);
                ok=ok&&writeF32(file,b.low)&&writeF32(file,b.high);
            }
        }
        const bool closed=std::fclose(file)==0;
        return ok&&closed&&appendFileChecksum(temp.data())&&replaceFileWithBackup(temp.data(),path.data());
    }
    bool readCustomMidiFile(const char* path,CustomMapState& result) {
        long payload=0;if(!checksummedPayloadSize(path,payload))return false;
        auto* file=std::fopen(path,"rb");if(!file)return false;
        CustomMapState next;char tag[8]{};std::uint32_t version=0,profiles=0,bindings=0,active=0,bank=0;
        bool ok=std::fread(tag,1,8,file)==8&&!std::memcmp(tag,"THRUMAP1",8)
            &&readU32(file,version)&&version==1&&readU32(file,profiles)&&profiles==4&&readU32(file,bindings)&&bindings==128
            &&readU32(file,active)&&active<=5&&readU32(file,bank)&&bank<8;
        next.active=static_cast<std::uint8_t>(active);next.factoryBank=static_cast<std::uint8_t>(bank);
        for(auto& p:next.profiles) {
            std::uint32_t selected=0;ok=ok&&readU32(file,selected)&&selected<8;p.bank=static_cast<std::uint8_t>(selected);
            for(auto& b:p.bindings) {
                std::uint32_t w[12]{};for(auto& word:w)ok=ok&&readU32(file,word);
                ok=ok&&w[0]<=1&&w[1]>=1&&w[1]<=16&&w[2]<=127&&w[3]<=8&&w[4]<=2
                    &&w[5]<=16384&&w[6]<=5&&w[7]<=3&&w[8]<=7&&w[9]<=3&&w[10]<=1&&w[11]<=1;
                b.enabled=w[0];b.channel=w[1];b.cc=w[2];b.bank=w[3];b.kind=w[4];b.target=static_cast<int>(w[5])-1;
                b.part=static_cast<int>(w[6])-1;b.page=w[7];b.slot=w[8];b.behavior=w[9];b.pickup=w[10];b.invert=w[11];
                ok=ok&&readF32(file,b.low)&&readF32(file,b.high)&&validCustomBinding(b);
            }
            if(ok)for(std::size_t i=0;i<p.bindings.size();++i)if(p.bindings[i].enabled
                &&s3g_thru_custom_midi::conflict(p,p.bindings[i],static_cast<int>(i))>=0){ok=false;break;}
        }
        ok=ok&&std::ftell(file)==payload;std::fclose(file);if(ok)result=next;return ok;
    }
    void loadCustomMidiMaps() {
        if(!persistenceEnabled)return;
        std::array<char,kPersistencePathSize> path{};
        if(controllerDataPath("thru-midi-maps.bin",path.data(),path.size())&&readCustomMidiFile(path.data(),customMaps))return;
        if(controllerDataPath("thru-midi-maps.bin.bak",path.data(),path.size())&&readCustomMidiFile(path.data(),customMaps))customRecovered=true;
    }
    void consumeCustomMidiSave() {
        if(customSaveJob.load(std::memory_order_acquire)!=3)return;
        if(customSaveSnapshot->success) {
            if(customSaveSnapshot->revision==customRevision)customDirty=false;
            customSaveFailed=false;
        } else {
            customSaveFailed=true;customRetryFrame=renderClockSamples.load(std::memory_order_relaxed)+static_cast<std::uint64_t>(sampleRate*2);
        }
        customSaveJob.store(0,std::memory_order_release);
    }
    void serviceCustomMidiSave() {
        consumeCustomMidiSave();
        const auto now=renderClockSamples.load(std::memory_order_relaxed);
        if(!customSaveSnapshot||!customDirty||customSaveJob.load(std::memory_order_acquire)!=0
            ||now<customRetryFrame||now-customEditFrame<static_cast<std::uint64_t>(sampleRate*.1f))return;
        customSaveSnapshot->state=customMaps;customSaveSnapshot->revision=customRevision;
        customSaveJob.store(1,std::memory_order_release);
    }
