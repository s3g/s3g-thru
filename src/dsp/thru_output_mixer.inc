int outputMixerCount() const {return splitEnabled?partCount():1;}
void resetOutputMixerRuntime(){
    sequenceStripProcessor.reset();splitDuckGainState.fill(1);splitDuckKey=-1;
    splitDuckEnvelope=0;splitDuckHpfInput={};splitDuckHpfOutput={};splitDuckHoldRemaining={};
}
void syncOutputMixerMode(int target){
    if(target<0||target>6||sequenceLive.mixerMode==target)return;
    sequenceLive.modeMixes[sequenceLive.mixerMode]=sequenceLive.mix;
    sequenceLive.mix=sequenceLive.modeMixes[target];sequenceLive.mixerMode=target;
    sequencePlayback.mix=sequenceLive.mix;resetOutputMixerRuntime();
}
bool legacyDualMixer() const {return splitEnabled&&!isQuad();}
int outputMixerField(const char* key) const {
    static constexpr const char* keys[]{"seq_eq_low","seq_eq_mid","seq_eq_high","seq_eq_freq","seq_eq_solo","mix_mute"};
    for(int i=0;i<6;++i)if(!std::strcmp(key,keys[i]))return i;
    return -1;
}
int outputDuckField(const char* key) const {
    static constexpr const char* keys[]{"seq_duck_key","seq_duck_depth","seq_duck_threshold","seq_duck_attack","seq_duck_hold","seq_duck_release","seq_duck_hpf","seq_duck_knee"};
    for(int i=0;i<8;++i)if(!std::strcmp(key,keys[i]))return i;
    return -1;
}
bool outputMixerValue(const char* key,int part,float& value) const {
    const auto& mix=sequenceLive.mix;part=clampValue(part,0,3);
    if(!std::strncmp(key,"seq_pan_",8)&&key[8]>='1'&&key[8]<='4'&&key[9]==0){value=mix.strips[key[8]-'1'].pan;return true;}
    const auto& strip=mix.strips[part];const int field=outputMixerField(key);
    if(field>=0){const float values[]{strip.lowDb,strip.midDb,strip.highDb,strip.midHz,strip.solo?1.f:0.f,splitEnabled?float(settingsForPart(part).muted):strip.muted?1.f:0.f};value=values[field];return true;}
    const int duck=outputDuckField(key);if(duck<0)return false;
    const auto& c=controls;
    const float values[]{float(mix.priority),mix.depthDb,mix.thresholdDb,mix.attackMs,mix.holdMs,mix.releaseMs,mix.hpfHz,mix.kneeDb};
    const float dual[]{c.splitPriority<-.001f?1.f:c.splitPriority>.001f?2.f:0.f,std::abs(c.splitPriority),c.splitDuckThresholdDb,c.splitDuckAttackMs,c.splitDuckHoldMs,c.splitDuckReleaseMs,c.splitDuckHpfHz,c.splitDuckKneeDb};
    value=legacyDualMixer()?dual[duck]:values[duck];return true;
}
std::array<char,32> outputMixerUndoKey{};
int outputMixerUndoPart=-1,outputMixerUndoClip=-1;
std::uint64_t outputMixerUndoAt=0;
void commitOutputMixer(int part,const char* key){
    const auto now=renderClockSamples.load(std::memory_order_relaxed);
    if(!undoAvailable||activeClip!=outputMixerUndoClip||part!=outputMixerUndoPart||std::strcmp(key,outputMixerUndoKey.data())||now-outputMixerUndoAt>std::uint64_t(sampleRate*2)){
        // Undo snapshot is captured by the caller before its first mutation.
        outputMixerUndoClip=activeClip;outputMixerUndoPart=part;std::snprintf(outputMixerUndoKey.data(),outputMixerUndoKey.size(),"%s",key);
    }
    outputMixerUndoAt=now;
    sequenceLive.modeMixes[sequenceLive.mixerMode]=sequenceLive.mix;
    sequencePlayback.mix=sequenceLive.mix;
    if(auto* clip=activeClipSlot()){clip->sequence.mix=sequenceLive.mix;clip->sequence.modeMixes=sequenceLive.modeMixes;clip->sequence.mixerMode=sequenceLive.mixerMode;clip->occupied=true;}
    activeClipOccupied=true;++sequenceRevision;markPersistentStateDirty();
}
bool setOutputMixerParameter(const char* key,const char* text){
    int part=splitEnabled?splitFocus:0,field=outputMixerField(key),pan=-1,duck=outputDuckField(key);
    const bool direct=!std::strcmp(key,"seq_strip_edit");float value=parseFloat(text,NAN);
    if(!std::strncmp(key,"seq_pan_",8)&&key[8]>='1'&&key[8]<='4'&&key[9]==0)pan=key[8]-'1';
    if(!direct&&field<0&&duck<0&&pan<0)return false;
    if(direct){char tail=0;if(std::sscanf(text,"%d,%d,%f %c",&part,&field,&value,&tail)!=3||field<0||field>5)return true;}
    if(pan>=0)part=pan;
    if(part<0||part>=outputMixerCount()||!std::isfinite(value))return true;
    if(projectLocked()){setError("Finish project operation first");return true;}
    if(field==5&&sequenceMode()){
        const int previous=splitFocus;selectSplitPart(part,false);
        setSequenceParameter("seq_mute",value>=.5f?"1":"0");selectSplitPart(previous,false);return true;
    }
    const auto now=renderClockSamples.load(std::memory_order_relaxed);
    if(!undoAvailable||activeClip!=outputMixerUndoClip||part!=outputMixerUndoPart||std::strcmp(key,outputMixerUndoKey.data())||now-outputMixerUndoAt>std::uint64_t(sampleRate*2))pushClipUndo(activeClip);
    auto& mix=sequenceLive.mix;auto& strip=mix.strips[part];
    if(pan>=0)strip.pan=clampValue(value,-1.f,1.f);
    else if(field>=0){
        if(field==0)strip.lowDb=clampValue(value,-12.f,12.f);
        if(field==1)strip.midDb=clampValue(value,-12.f,12.f);
        if(field==2)strip.highDb=clampValue(value,-12.f,12.f);
        if(field==3)strip.midHz=clampValue(value,120.f,8000.f);
        if(field==4)strip.solo=value>=.5f;
        if(field==5){if(splitEnabled){splitParts[part].settings.muted=value>=.5f;sequenceMuted[part]=value>=.5f;}else strip.muted=value>=.5f;}
    } else {
        if(duck==0)mix.priority=clampValue(int(std::lround(value)),0,outputMixerCount());
        if(duck==1)mix.depthDb=clampValue(value,0.f,24.f);
        if(duck==2)mix.thresholdDb=clampValue(value,-60.f,0.f);
        if(duck==3)mix.attackMs=clampValue(value,1.f,250.f);
        if(duck==4)mix.holdMs=clampValue(value,0.f,1000.f);
        if(duck==5)mix.releaseMs=clampValue(value,10.f,3000.f);
        if(duck==6)mix.hpfHz=clampValue(value,20.f,2000.f);
        if(duck==7)mix.kneeDb=clampValue(value,0.f,24.f);
        if(legacyDualMixer()){
            if(duck==0){if(mix.priority&&mix.depthDb<=0)mix.depthDb=std::max(6.f,std::abs(controls.splitPriority));controls.splitPriority=mix.priority==1?-mix.depthDb:mix.priority==2?mix.depthDb:0;}
            if(duck==1){int selected=controls.splitPriority<0?1:controls.splitPriority>0?2:mix.priority;controls.splitPriority=selected==1?-mix.depthDb:selected==2?mix.depthDb:0;}
            if(duck==2)controls.splitDuckThresholdDb=mix.thresholdDb;
            if(duck==3)controls.splitDuckAttackMs=mix.attackMs;
            if(duck==4)controls.splitDuckHoldMs=mix.holdMs;
            if(duck==5)controls.splitDuckReleaseMs=mix.releaseMs;
            if(duck==6)controls.splitDuckHpfHz=mix.hpfHz;
            if(duck==7)controls.splitDuckKneeDb=mix.kneeDb;
            const char* legacy[]{"split_priority","split_priority","split_duck_threshold","split_duck_attack","split_duck_hold","split_duck_release","split_duck_hpf","split_duck_knee"};
            const int parameter=lockParameterIndex(legacy[duck]);touchSplitMixerParameter(parameter);captureClipParameter(parameter);
        }
    }
    clearError();commitOutputMixer(part,key);return true;
}
void processSingleOutputMixer(std::size_t count){
    const auto& s=sequenceLive.mix.strips[0];
    if(!sequenceStripProcessor.active()&&s.pan==0&&s.lowDb==0&&s.midDb==0&&s.highDb==0&&!s.solo&&!s.muted)return;
    for(int ch=0;ch<2;++ch)std::copy_n(voiceMix[ch].data(),count,splitBus[0][ch].data());
    sequenceStripProcessor.process(count,splitBus,sequenceLive.mix,sampleRate,1);
    for(int ch=0;ch<2;++ch)std::copy_n(splitBus[0][ch].data(),count,voiceMix[ch].data());
}
int outputMixerState(char* out,int capacity) const {
    int at=std::snprintf(out,capacity,"{\"mode\":%d,\"count\":%d,\"layers\":%d,\"strips\":[",performanceMode(),outputMixerCount(),layersLive.active?1:0);
    auto add=[&](const char* fmt,auto... args){if(at<0||at>=capacity)return;const int n=std::snprintf(out+at,capacity-at,fmt,args...);at=n<0||n>=capacity-at?capacity:at+n;};
    for(int p=0;p<4;++p){const auto& s=sequenceLive.mix.strips[p];add("%s[%.6g,%.6g,%.6g,%.6g,%d,%d]",p?",":"",double(s.lowDb),double(s.midDb),double(s.highDb),double(s.midHz),s.solo?1:0,splitEnabled?settingsForPart(p).muted:s.muted?1:0);}
    add("%s","],\"pan\":[");for(int p=0;p<4;++p)add(p?",%.6g":"%.6g",double(sequenceLive.mix.strips[p].pan));
    add("%s","],\"duck\":[");const char* keys[]{"seq_duck_key","seq_duck_depth","seq_duck_threshold","seq_duck_attack","seq_duck_hold","seq_duck_release","seq_duck_hpf","seq_duck_knee"};
    for(int i=0;i<8;++i){float v=0;outputMixerValue(keys[i],0,v);add(i?",%.6g":"%.6g",double(v));}add("%s", "]}");return at<capacity?at:-1;
}
