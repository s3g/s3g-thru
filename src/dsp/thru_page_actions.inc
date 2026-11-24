struct PageActionBatch {
    ThruInstance& t;
    explicit PageActionBatch(ThruInstance& instance):t(instance){t.pageActionApplying=true;}
    ~PageActionBatch(){t.pageActionApplying=false;}
};
// Scoped control-thread adapter. No work, allocation or queries in render().
const MidiParameter* pageParameter(const std::string& key) {
    for(const auto& p:s3g_thru_midi::parameters)if(key==p.key)return &p;
    return nullptr;
}
bool pageScalar(const MidiParameter& p) {
    const std::string k=p.key;
    if(p.action||k=="algorithm"||k=="motion_sound"||k=="record_lane")return false;
    if(p.lock>=0)return true;
    for(const char* prefix:{"table_","scanned_","fm_","noise_","reson_","partials_","lowform_","travel_","live_"})
        if(k.rfind(prefix,0)==0)return true;
    return k=="pad_mode"||k=="voice_mode"||k=="root_note"||k=="scale"||k=="pressure_mode"||k=="hold_release_ms"||k=="polyphony_comp";
}
int formatPageActions(ThruInstance& t,char* out,int cap) {
    std::string ids,safe;
    for(const auto& p:s3g_thru_midi::parameters)safe+=pageScalar(p)?'1':'0';
    for(int id:t.pageClipboardIds){if(!ids.empty())ids+=',';ids+=std::to_string(id);}
    return std::snprintf(out,cap,"{\"context\":\"%s\",\"group\":\"%s\",\"engine\":%d,\"ids\":\"%s\",\"lane\":%d,\"path\":%d,\"generator\":%d,\"trig\":%d,\"clip\":%d,\"chain\":%d,\"layer\":%d,\"safe\":\"%s\",\"patch\":%d}",
        t.pageActionContext().c_str(),t.pageClipboardGroup.c_str(),t.pageClipboardEngine,ids.c_str(),t.pageLaneReady,t.pagePathReady,t.pageGeneratorReady,t.sequenceClipboardReady,t.clipClipboardSource>=0,t.chainClipboardValid,t.layersClipboardReady,safe.c_str(),t.tableEditLane(t.splitEnabled?t.splitFocus:0));
}
void performPageAction(ThruInstance& t,const char* text) {
    std::array<std::string,4> fields;const std::string request=text;std::size_t first=0;
    for(int i=0;i<3;++i){const auto at=request.find('|',first);if(at==std::string::npos){t.setError("Invalid page action");return;}fields[i]=request.substr(first,at-first);first=at+1;}
    fields[3]=request.substr(first);
    if(fields[1]!=t.pageActionContext()){t.setError("Page target changed; open Actions again");return;}
    if(fields[2].empty()||fields[2].size()>40||fields[2].find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 & _/-")!=std::string::npos){t.setError("Invalid page scope");return;}
    const auto& op=fields[0];const auto& payload=fields[3];
    const bool copying=op=="check"||op=="genCopy"||op=="copy"||op=="laneCopy"||op=="pathCopy"||(op=="run"&&(payload.rfind("seq_copy:",0)==0||payload.rfind("clip_clipboard_copy:",0)==0||payload.rfind("chain_copy:",0)==0));
    if(!copying&&(t.audioRecording()||t.clipRecording||t.patternBusy()||t.clipPreparationInFlight()||t.bankJob.load(std::memory_order_acquire)||t.pendingCaptureSaveMask)){t.setError("Finish recording or clip transition first");return;}
    t.clearError();
    const int part=t.splitEnabled?t.splitFocus:0;
    if(op=="check")return;
    if(op=="mod"){
        int slot=-1,id=-1,dest=-1,target=-1,algorithm=-1,n=0;
        if(std::sscanf(payload.c_str(),"%d,%d,%d,%d,%d%n",&slot,&id,&dest,&target,&algorithm,&n)!=5||std::size_t(n)!=payload.size()||!t.assignModulation(part,slot,id,dest,target,algorithm))t.setError("Modulation target changed or unavailable");
        return;
    }

    if(op=="set"){
        const auto colon=payload.find(':');const auto* p=pageParameter(payload.substr(0,colon));
        char* end=nullptr;const float value=colon==std::string::npos?NAN:std::strtof(payload.c_str()+colon+1,&end);
        if(!p||!pageScalar(*p)||!end||end==payload.c_str()+colon+1||*end||!std::isfinite(value)||value<p->minimum||value>p->maximum||t.sequenceSoundEditing()){t.setError("Invalid control value");return;}
        t.pushClipUndo(t.activeClip);midiWrite(t,*p,part,value);t.markPersistentStateDirty();return;
    }
    if(op=="genCopy"){t.pageGeneratorClipboard=t.sequenceLive.tracks[part].generator;t.pageGeneratorReady=true;return;}
    if(op=="genPaste"){
        if(!t.sequenceMode()||!t.pageGeneratorReady){t.setError("Copy generator settings first");return;}
        t.pushClipUndo(t.activeClip);auto& g=t.sequenceLive.tracks[part].generator;
        g.base=t.sequenceLive.tracks[part].trigs;g.ready=true;g.timed=t.pageGeneratorClipboard.timed;g.enabledMask=g.enabledValues=g.burstMask=0;
        g.type=t.pageGeneratorClipboard.type;g.values=t.pageGeneratorClipboard.values;g.shift=t.pageGeneratorClipboard.shift;
        PageActionBatch batch(t);setParam(&t,"seq_regenerate","1");return;
    }
    if(op=="copy"||op=="paste"||op=="reset"){
        if(t.sequenceSoundEditing()){t.setError("Use trig Copy/Paste or Clear locks");return;}
        std::vector<const MidiParameter*> parameters;std::vector<int> ids;first=0;
        while(first<payload.size()){
            const auto end=payload.find(',',first);const auto key=payload.substr(first,end==std::string::npos?end:end-first);
            const auto* p=pageParameter(key);
            if(!p||!pageScalar(*p)||parameters.size()>=128||std::find(ids.begin(),ids.end(),p->id)!=ids.end()){t.setError("Unsupported page control");return;}
            parameters.push_back(p);ids.push_back(p->id);if(end==std::string::npos)break;first=end+1;
        }
        if(parameters.empty()){t.setError("No editable controls on this page");return;}
        if(op=="copy"){
            std::vector<float> values;for(auto* p:parameters)values.push_back(midiRead(t,*p,part));
            t.pageClipboardIds=ids;t.pageClipboardValues=values;t.pageClipboardGroup=fields[2];t.pageClipboardEngine=int(t.controls.algorithm);return;
        }
        if(op=="paste"&&(ids!=t.pageClipboardIds||fields[2]!=t.pageClipboardGroup||(fields[2]=="PATCH"&&int(t.controls.algorithm)!=t.pageClipboardEngine))){t.setError("Copy matching settings first");return;}
        // All addresses and values are checked before changing anything.
        std::vector<float> values;
        for(std::size_t i=0;i<parameters.size();++i){const auto& p=*parameters[i];const float value=op=="reset"?p.initial:t.pageClipboardValues[i];if(!std::isfinite(value)||value<p.minimum||value>p.maximum){t.setError("Settings do not fit this target");return;}values.push_back(value);}
        t.pushClipUndo(t.activeClip);const bool compensation=t.polyphonyCompensation;
        PageActionBatch batch(t);
        for(std::size_t i=0;i<parameters.size();++i)midiWrite(t,*parameters[i],part,values[i]);
        t.undoPageCompValid=std::any_of(parameters.begin(),parameters.end(),[](const auto* p){return !std::strcmp(p->key,"polyphony_comp");});t.undoPageCompValue=compensation;
        t.markPersistentStateDirty();return;
    }
    if(op=="laneCopy"){
        t.pageLaneClipboard=t.sequenceLive.tracks[part];t.pageLaneSounds={};
        for(int step=0;step<32;++step)t.pageLaneSounds.pasteStep(0,step,t.sequenceLive.soundLocks.copyStep(part,step));
        t.pageLaneReady=true;return;
    }
    if(op=="lanePaste"||op=="laneClear"){
        if(!t.sequenceMode()||t.sequenceSoundEditing()||(op=="lanePaste"&&!t.pageLaneReady)){t.setError("No matching sequence lane");return;}
        auto sounds=t.sequenceLive.soundLocks;sounds.eraseLane(part);
        if(op=="lanePaste")for(int step=0;step<32;++step)if(!sounds.pasteStep(part,step,t.pageLaneSounds.copyStep(0,step))){t.setError("Sequence lock capacity reached");return;}
        t.pushClipUndo(t.activeClip);
        if(op=="lanePaste")t.sequenceLive.tracks[part]=t.pageLaneClipboard;
        else for(int step=0;step<32;++step){t.sequenceLive.tracks[part].trigs[step]={};t.rememberSequenceTypeEdit(part,step,0,"seq_delete");}
        t.sequenceLive.soundLocks=sounds;t.commitSequenceEdit();return;
    }
    if(op=="trigClearLocks"){
        if(!t.sequenceSoundEditing()){t.setError("Hold a trig to clear locks");return;}
        t.pushClipUndo(t.activeClip);auto& step=t.sequenceLive.tracks[part].trigs[t.sequenceEditStep];step.locks={};step.lfos={};
        for(auto& event:step.events)event.locks={};
        t.sequenceLive.soundLocks.eraseStep(part,t.sequenceEditStep);
        for(int child=0;child<=8;++child)t.rememberSequenceTypeEdit(part,t.sequenceEditStep,child,"seq_clear");
        t.commitSequenceEdit();return;
    }
    if(op=="release"){
        for(int pad=0;pad<32;++pad)if(t.latchedPadNotes[pad]>=0&&(!t.splitEnabled||t.partForInput(pad)==part))t.releaseLatchedPad(pad,true);
        return;
    }
    if(op=="pathCopy"){t.pagePathClipboard=t.controls.stackPath;t.pagePathReady=true;return;}
    if(op=="pathPaste"||op=="pathReset"){
        if(op=="pathPaste"&&!t.pagePathReady){t.setError("Copy a path first");return;}
        const auto before=t.controls.stackPath;t.pushClipUndo(t.activeClip);
        t.controls.stackPath=op=="pathReset"?s3g_daisy::ThruStackPath{}:t.pagePathClipboard;
        t.captureStackPathEdit(before);t.storeFocusedPart();t.markPersistentStateDirty();return;
    }
    if(op=="stripReset"){
        char* end=nullptr;const long strip=std::strtol(payload.c_str(),&end,10);
        if(!end||*end||strip<0||strip>=t.outputMixerCount()){t.setError("Invalid mixer strip");return;}
        t.pushClipUndo(t.activeClip);
        PageActionBatch batch(t);for(int field=0;field<4;++field){const std::string v=std::to_string(strip)+","+std::to_string(field)+","+(field==3?"900":"0");setParam(&t,"seq_strip_edit",v.c_str());}return;
    }
    if(op=="run"){
        const auto colon=payload.find(':');if(colon==std::string::npos){t.setError("Missing action value");return;}
        const auto key=payload.substr(0,colon),value=payload.substr(colon+1);
        const std::array<const char*,22> allowed{{"seq_copy","seq_paste","seq_regenerate","seq_to_normal","sample_normalize","sample_reverse","sample_trim_apply","clip_clipboard_copy","clip_clipboard_paste","clip_clear_recording","clip_take_undo","chain_copy","chain_paste","chain_enabled","chain_insert","seq_strip_edit","fm_start","reson_start","lowform_start","layers_copy","layers_paste","layers_reset"}};
        if(std::none_of(allowed.begin(),allowed.end(),[&](const char* k){return key==k;})){t.setError("Unsupported page action");return;}
        setParam(&t,key.c_str(),value.c_str());return;
    }
    t.setError("Unknown page action");
}
