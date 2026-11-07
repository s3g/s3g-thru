// MIDI control runs on the existing serialized control path. No MIDI output,
// allocation, file I/O or focus notification is performed by audio rendering.
using MidiParameter = s3g_thru_midi::Parameter;

// Twelve native pages, eight destinations per part. The retired private
// NRPN address range is recognized only to discard old scene packets.
constexpr int kE16NativeAliasBase=3456;
constexpr int kE16NativeAliasPages=12;
constexpr int kE16NativeAliasSlots=8;
constexpr int kE16NativeAliasCount=kE16NativeAliasPages*kE16NativeAliasSlots;
const MidiParameter* e16PageSlotParameter(ThruInstance&,int,int,int&);

bool e16NativeAlias(int id,int& page,int& slot) {
    const int offset=id-kE16NativeAliasBase;
    if(offset<0 || offset>=kE16NativeAliasCount)return false;
    page=offset/kE16NativeAliasSlots;slot=offset%kE16NativeAliasSlots;
    return true;
}

std::uint64_t e16RemoteClock() {
    timespec now{};clock_gettime(CLOCK_MONOTONIC,&now);
    return static_cast<std::uint64_t>(now.tv_sec)*1000u+static_cast<std::uint64_t>(now.tv_nsec)/1000000u;
}
bool e16RemoteLive(ThruInstance& t) {
    if((t.e16RemoteRequested || t.e16RemoteActive) && e16RemoteClock()>t.e16RemoteLease) {
        t.e16RemoteRequested=t.e16RemoteActive=false;t.e16RemoteLease=0;
        t.e16RemoteButtons=t.e16RemoteHeld=0;
    }
    return t.e16RemoteRequested || t.e16RemoteActive;
}

float midiMaximum(const ThruInstance& t,const MidiParameter& p) {
    if(std::strcmp(p.key,"manual_lane")==0)return t.isQuad()?1.0f:t.splitEnabled&&!t.sharedDual?2.0f:4.0f;
    if(std::strcmp(p.key,"cue_trigger")==0 || std::strcmp(p.key,"cue_clear")==0)return t.splitEnabled?static_cast<float>(t.cuesPerPart()-1):7.0f;
    if(std::strcmp(p.key,"record_source")==0)return t.splitEnabled?(t.isQuad()?5.0f:3.0f):1.0f;
    if(std::strcmp(p.key,"voice_engine_edit")==0)return static_cast<float>(t.engineSlotCount());
    if(std::strcmp(p.key,"split_focus")==0)return t.isQuad()?3.0f:1.0f;
    return p.maximum;
}

float midiRead(ThruInstance& t,const MidiParameter& p,int part) {
    part=clampValue(part,0,t.isQuad()?3:t.splitEnabled?1:0);
    const char* key=p.key;
    if(std::strcmp(key,"part_level")==0)return lockedValue(t.controls,t.outputGain,t.partLevelParameter(part));
    if(std::strcmp(key,"part_delay")==0)return lockedValue(t.controls,t.outputGain,t.partDelayParameter(part));
    if(std::strcmp(key,"part_reverb")==0)return lockedValue(t.controls,t.outputGain,t.partReverbParameter(part));
    // Saved automation retains retired Wavesets process IDs. MIDI and the
    // visible menu use the dense ten-choice index, just like set_param.
    if(std::strcmp(key,"waveset_process")==0)return static_cast<float>(
        Engine::wavesetProcessIndex(t.liveControlsForPart(part).wavesetProcess));
    const int index=p.lock;
    if(index>=0)return lockedValue(p.part?t.liveControlsForPart(part):t.controls,t.outputGain,index);
    if(p.part) {
        const auto settings=t.settingsForPart(part);
        const float setup=t.voiceSetupValue(settings.engines,key,t.engineSlotCount());
        if(std::isfinite(setup))return setup;
        if(std::strcmp(key,"voice_engine_mode")==0)return settings.engines.enabled;
        if(std::strcmp(key,"voice_engine_order")==0)return settings.engines.order;
        if(std::strcmp(key,"voice_engine_edit")==0)return std::min(settings.engines.edit,t.engineSlotCount()-1)+1;
        const int slot=t.engineSlotKey(key);
        if(slot>=0)return settings.engines.configured?settings.engines.engines[slot]:static_cast<int>(t.liveControlsForPart(part).algorithm);
        if(std::strcmp(key,"voice_mode")==0)return settings.voiceMode;
        if(std::strcmp(key,"pad_mode")==0)return settings.mode;
        if(std::strcmp(key,"root_note")==0)return settings.root;
        if(std::strcmp(key,"scale")==0)return settings.scale;
        if(std::strcmp(key,"pad_octave")==0)return settings.octave;
        if(std::strcmp(key,"pressure_mode")==0)return settings.pressure;
        if(std::strcmp(key,"hold_release_ms")==0)return settings.holdMs;
        if(std::strcmp(key,"stack_scan_paused")==0)return settings.stackHold;
        if(std::strcmp(key,"glitch_buffer_lock")==0)return settings.glitchHold;
        if(std::strcmp(key,"lane_variation")==0)return settings.variation;
        if(std::strcmp(key,"part_mute")==0)return settings.muted;
    }
    char value[64]{};
    if(getParamValue(&t,key,value,sizeof(value))<1)return p.initial;
    char* end=nullptr;const float result=std::strtof(value,&end);
    return end!=value && *end=='\0' && std::isfinite(result)?result:p.initial;
}

void midiWrite(ThruInstance& t,const MidiParameter& p,int part,float value) {
    if(!std::isfinite(value))return;
    if(p.part && (part<0 || part>=(t.splitEnabled?t.partCount():1)))return;
    const int focus=t.splitFocus,previousMidi=t.midiPart;
    if(p.part && t.splitEnabled)t.selectSplitPart(part,false);
    t.midiPart=p.part?part:0;
    value=clampValue(value,p.minimum,midiMaximum(t,p));
    if(p.integer)value=std::round(value);
    const char* key=p.key;
    if(std::strcmp(key,"part_level")==0)key=kLockParameterKeys[t.partLevelParameter(part)];
    if(std::strcmp(key,"part_delay")==0)key=kLockParameterKeys[t.partDelayParameter(part)];
    if(std::strcmp(key,"part_reverb")==0)key=kLockParameterKeys[t.partReverbParameter(part)];
    if(p.part && t.splitEnabled && (std::strcmp(key,"cue_trigger")==0 || std::strcmp(key,"cue_clear")==0))value+=part*t.cuesPerPart();
    if(std::strcmp(key,"pattern_fade_position")==0)t.setPatternHeld(true);
    char text[48]{};std::snprintf(text,sizeof(text),"%.9g",value);
    setParam(&t,key,text);
    const int index=lockParameterIndex(key);
    if(index>=0 && t.clipRecording)t.captureClipParameter(index);
    if(p.part && t.splitEnabled)t.selectSplitPart(focus,false);
    t.midiPart=previousMidi;++t.midiRevision;
}

bool midiModeAllowsNrpn(const ThruInstance& t) {
    return t.midiControlMode==0 || t.midiControlMode==1 || t.midiControlMode==5;
}
bool midiModeAllowsGenericCc(const ThruInstance& t) {
    return (t.midiControlMode==0 || t.midiControlMode==2)&&t.customMaps.active==0;
}
bool midiModeAllowsMonolit(const ThruInstance& t) {
    return t.midiControlMode==3 || t.midiControlMode==5;
}
bool midiModeAllowsE16(const ThruInstance& t) {
    return t.midiControlMode==0 || t.midiControlMode==4;
}
void resetMidiControlPickups(ThruInstance& t) {
    for(auto& source:t.midiCcPickups)for(auto& pickup:source)pickup={};
}

const MidiParameter* midiParameterForKey(const char* key) {
    for(const auto& p:s3g_thru_midi::parameters)
        if(std::strcmp(p.key,key)==0)return &p;
    return nullptr;
}

bool applyAbsoluteCc(ThruInstance& t,const MidiParameter& p,int part,
    int value,int slot,int source) {
    if(p.action || slot<0
        || slot>=static_cast<int>(t.midiCcPickups[0].size()))return true;
    auto& pickup=t.midiCcPickups[source==MOVE_MIDI_SOURCE_HOST?1:0][slot];
    if(pickup.id!=p.id || pickup.part!=part) {
        pickup={};pickup.id=p.id;pickup.part=part;
    }
    const float maximum=midiMaximum(t,p);
    const bool boundedInteger=p.integer && maximum-p.minimum<=127.0f;
    const auto decodedInteger=[&](int cc) {
        const int raw=cc>=127?16383:clampValue(cc,0,127)*129;
        return static_cast<int>(std::lround(s3g_thru_midi::decode(p,raw,maximum)));
    };
    const int currentInteger=boundedInteger
        ?static_cast<int>(std::lround(midiRead(t,p,part))):0;
    const int currentRaw=s3g_thru_midi::encode(p,midiRead(t,p,part),maximum);
    const int current7=static_cast<int>(std::lround(currentRaw*127.0/16383.0));
    if(t.midiTakeover==0) {
        if(pickup.engaged && pickup.last>=0) {
            const bool changedOutsideController=boundedInteger
                ?currentInteger!=decodedInteger(pickup.last)
                :std::abs(current7-pickup.last)>2;
            if(changedOutsideController)pickup.engaged=false;
        }
        if(!pickup.engaged) {
            // A short enum owns a region of the fader, not one canonical CC.
            // Engage anywhere in the current choice's region so moving on to
            // the next choice does not immediately drop Pickup again.
            const bool close=boundedInteger
                ?decodedInteger(value)==currentInteger
                :std::abs(value-current7)<=1;
            const bool crossed=!boundedInteger && pickup.last>=0
                && (pickup.last-current7)*(value-current7)<=0;
            pickup.last=value;
            if(!close && !crossed)return true;
            pickup.engaged=true;
        }
    } else pickup.engaged=true;
    pickup.last=value;
    const int raw=value>=127?16383:value*129;
    midiWrite(t,p,part,s3g_thru_midi::decode(p,raw,maximum));
    return true;
}

bool handleGenericCc(ThruInstance& t,int channel,int number,int value,int source) {
    if(!midiModeAllowsGenericCc(t) || channel!=t.midiCcChannel
        || number<t.midiCcBase || number>=t.midiCcBase+8)return false;
    const int slot=number-t.midiCcBase;
    if(static_cast<std::size_t>(slot)>=t.midiViewCount)return true;
    const auto& view=t.midiView[slot];
    const auto* p=s3g_thru_midi::find(view.id);
    if(!p || p->action)return true;
    const int part=p->part?view.part:-1;
    if(p->part && (part<0 || part>=(t.splitEnabled?t.partCount():1)))return true;
    return applyAbsoluteCc(t,*p,part,value,slot,source);
}

void applyNrpn(ThruInstance& t,int channel,int id,int raw,int /*source*/) {
    int nativePage=0,nativeSlot=0;
    // Retired absolute aliases cannot replace a live project.
    if(e16NativeAlias(id,nativePage,nativeSlot))return;
    const auto* p=s3g_thru_midi::find(id);if(!p)return;
    const int part=p->part?channel-5:-1;
    if(p->part?(part<0 || part>=(t.splitEnabled?t.partCount():1)):channel!=16)return;
    if(!p->action) {
        const auto index=static_cast<std::size_t>(p-s3g_thru_midi::parameters);
        t.midiWatched[static_cast<std::size_t>(part+1)*s3g_thru_midi::count+index]=true;
        // Echoing feedback must not write state, record redundant automation,
        // or start another feedback exchange.
        if(s3g_thru_midi::encode(*p,midiRead(t,*p,part),midiMaximum(t,*p))==raw)return;
    }
    midiWrite(t,*p,part,s3g_thru_midi::decode(*p,raw,midiMaximum(t,*p)));
}

int e16HoldTime(int index) {
    index=clampValue(index,0,67);
    if(index<=20)return index*50;
    if(index<=36)return 1000+(index-20)*250;
    if(index<=61)return 5000+(index-36)*1000;
    return 30000+(index-61)*5000;
}
int e16HoldIndex(float value) {
    int nearest=0;float distance=std::abs(value);
    for(int index=1;index<=67;++index) {
        const float next=std::abs(value-e16HoldTime(index));
        if(next<distance){nearest=index;distance=next;}
    }
    return nearest;
}
bool handleE16NativeResetCc(ThruInstance& t,int channel,int number,int value) {
    if(t.midiControlMode!=5 || (channel!=14 && channel!=15))return false;
    // CC 127 carries a one-based destination for default pushes. CCs 0-95
    // carry explicit signed turns (CC Rel1), independent of ring feedback.
    if(channel==14 && number==125){t.e16NativeToken=value;return true;}
    if(channel==14 && number==126){if(value>=1 && value<=12){t.e16NativePage=value-1;t.e16NativeLease=e16RemoteClock()+3000;}return true;}
    if(number!=127 && (number<0 || number>=kE16NativeAliasCount))return true;
    const bool reset=number==127;
    if(reset && (value<1 || value>kE16NativeAliasCount))return true;
    const int offset=reset?value-1:number;
    int part=channel-14;
    if(part>=(t.splitEnabled?t.partCount():1))return true;
    const auto* p=e16PageSlotParameter(t,offset/8,offset%8,part);
    if(!p || p->action)return true;
    // Normalize THRU EM's hardware turn direction: clockwise arrives as
    // 127 (or 120..126 for larger turns); counterclockwise as 1..8.
    // Ring feedback remains an absolute reading and never changes this sign.
    const int ticks=value>0 && value<64?-value:value>64?128-value:0;
    if(!reset && (ticks==0 || std::abs(ticks)>8))return true;
    const float current=midiRead(t,*p,part),maximum=midiMaximum(t,*p);
    float next=p->initial;
    if(!reset) {
        const bool lane=std::strcmp(p->key,"manual_lane")==0
            && t.liveControlsForPart(part).algorithm==Engine::PlaybackAlgorithm::cutups;
        if(std::strcmp(p->key,"hold_release_ms")==0)next=static_cast<float>(e16HoldTime(e16HoldIndex(current)+ticks));
        else if(p->integer || lane)next=clampValue(current+ticks,p->minimum,maximum);
        else {
            // Retain the small-size turn increment after extending SIZE to 500 ms.
            const int step=std::strcmp(p->key,"pulsar_size")==0?37:48;
            next=s3g_thru_midi::decode(*p,clampValue(
                s3g_thru_midi::encode(*p,current,maximum)+ticks*step,0,16383),maximum);
        }
    }
    if(next!=current)midiWrite(t,*p,part,next);
    return true;
}

// Controller scene pushes use dedicated control channels, separate from notes 1-4.
void applyMidiSceneNote(ThruInstance& t,int channel,int note) {
    if(channel==16) {
        if(note==0)t.setPatternHeld(!t.patternHeld);
        else if(note==1)t.finishPatternFade();
        else if(note==2)++t.midiFeedbackEpoch;
    } else if(note==0) {
        const int part=channel-5;
        if(part<0 || part>=(t.splitEnabled?t.partCount():1))return;
        for(const auto& p:s3g_thru_midi::parameters)if(std::strcmp(p.key,"stack_scan_paused")==0) {
            midiWrite(t,p,part,t.settingsForPart(part).stackHold?0.0f:1.0f);
            break;
        }
    }
}

void e16RemoteNormalizePage(ThruInstance& t);

bool handleMidiControl(ThruInstance& t,const char* key,const char* value) {
    if(std::strcmp(key,"midi_control_mode")==0) {
        const int mode=clampValue(parseEnum(value,kMidiControlModeNames,t.midiControlMode),0,6);
        if(mode!=t.midiControlMode) {
            t.midiControlMode=mode;resetMidiControlPickups(t);t.resetCustomPickups();
            t.e16NativePage=-1;t.e16NativeLease=0;
            if(!midiModeAllowsE16(t)) {
                t.e16RemoteRequested=t.e16RemoteActive=false;t.e16RemoteLease=0;
                t.e16RemoteButtons=t.e16RemoteHeld=0;
            }
            t.markPersistentStateDirty();
        }
        return true;
    }
    if(std::strcmp(key,"midi_cc_channel")==0) {
        const int channel=clampValue(parseInteger(value,t.midiCcChannel),1,16);
        if(channel!=t.midiCcChannel){t.midiCcChannel=channel;resetMidiControlPickups(t);t.markPersistentStateDirty();}
        return true;
    }
    if(std::strcmp(key,"midi_cc_base")==0) {
        const int base=clampValue(parseInteger(value,t.midiCcBase),0,120);
        if(base!=t.midiCcBase){t.midiCcBase=base;resetMidiControlPickups(t);t.markPersistentStateDirty();}
        return true;
    }
    if(std::strcmp(key,"midi_takeover")==0) {
        const int takeover=clampValue(parseEnum(value,kMidiTakeoverNames,t.midiTakeover),0,1);
        if(takeover!=t.midiTakeover){t.midiTakeover=takeover;resetMidiControlPickups(t);t.markPersistentStateDirty();}
        return true;
    }
    if(std::strcmp(key,"midi_control_part")==0) {
        const int part=clampValue(parseInteger(value,t.midiControlPart),0,3);
        if(part!=t.midiControlPart){t.midiControlPart=part;resetMidiControlPickups(t);t.markPersistentStateDirty();}
        return true;
    }
    if(std::strcmp(key,"e16_remote")==0) {
        int active=-1,requested=-1,n=0;
        if(std::sscanf(value,"%d,%d%n",&active,&requested,&n)!=2 || value[n]
            || active<0 || active>1 || requested<0 || requested>1 || active>requested)return true;
        if(!midiModeAllowsE16(t))active=requested=0;
        if(requested && !e16RemoteLive(t)) {
            t.e16Page=0;t.e16Pair=0;t.e16RemoteHeld=0;t.e16RemoteButtons=0;
        }
        t.e16RemoteActive=active;t.e16RemoteRequested=requested;
        if(!active)t.e16RemoteButtons=0;
        t.e16RemoteLease=requested?e16RemoteClock()+8000u:0;
        return true;
    }
    if(std::strcmp(key,"e16_remote_buttons")==0) {
        unsigned mask=0;int n=0;
        if(std::sscanf(value,"%u%n",&mask,&n)==1 && !value[n] && mask<=0x3ffffu
            && e16RemoteLive(t) && t.e16RemoteActive) {
            t.e16RemoteButtons=mask;t.e16RemoteHeld|=mask;
            t.e16RemoteLease=e16RemoteClock()+8000u;
        }
        return true;
    }
    if(std::strcmp(key,"e16_remote_page")==0) {
        int page=-1,n=0;
        if(std::sscanf(value,"%d%n",&page,&n)==1 && !value[n] && page>=0 && page<12
            && e16RemoteLive(t) && t.e16RemoteActive && page!=t.e16Page) {
            t.e16Page=page;e16RemoteNormalizePage(t);
            t.e16RemoteLease=e16RemoteClock()+8000u;
        }
        return true;
    }
    if(std::strcmp(key,"e16_remote_pair")==0) {
        int pair=-1,n=0;
        if(std::sscanf(value,"%d%n",&pair,&n)==1 && !value[n] && (pair==0 || pair==2)
            && e16RemoteLive(t) && t.e16RemoteActive && t.isQuad() && pair!=t.e16Pair) {
            t.e16Pair=pair;e16RemoteNormalizePage(t);
            t.e16RemoteLease=e16RemoteClock()+8000u;
        }
        return true;
    }

    if(std::strcmp(key,"midi_view")==0) {
        std::array<ThruInstance::MidiView,8> next{};std::size_t count=0;
        const char* cursor=value;
        while(*cursor && count<next.size()) {
            int part=-2,id=-1,n=0;
            if(std::sscanf(cursor,"%d:%d%n",&part,&id,&n)!=2 || n<1)return true;
            const auto* p=s3g_thru_midi::find(id);
            const bool reserved=part==-1 && id==-1;
            if(!reserved && (!p || p->action || (p->part?part<0||part>3:part!=-1)))return true;
            next[count++]={part,id};cursor+=n;
            if(*cursor==';')++cursor;else if(*cursor)return true;
        }
        if(*cursor)return true;
        t.midiView=next;t.midiViewCount=count;return true;
    }
    return false;
}

int formatMidiState(ThruInstance& t,char* buffer,int size) {
    int at=0;bool ok=true;
    const auto append=[&](const char* f,auto... args) {
        if(!ok)return;
        const int n=std::snprintf(buffer+at,size-at,f,args...);
        if(n<0 || n>=size-at){ok=false;return;}at+=n;
    };
    append("{\"v\":1,\"epoch\":%u,\"values\":[",t.midiFeedbackEpoch);
    const int count=t.splitEnabled?t.partCount():1;
    bool comma=false;
    for(std::size_t i=0;i<t.midiViewCount;++i) {
        const auto& entry=t.midiView[i];const auto* p=s3g_thru_midi::find(entry.id);
        if(!p || (p->part && entry.part>=count))continue;
        append("%s[%d,%d,%.8g]",comma?",":"",entry.part,entry.id,midiRead(t,*p,entry.part));comma=true;
    }
    append("%s","]}");if(!ok){buffer[0]=0;return -1;}return at;
}

int formatMidiFeedback(ThruInstance& t,char* buffer,int size) {
    if(!t.midiFeedbackInitialized) {
        // Common control defaults belong to the MIDI registry. Other mappings
        // subscribe when their first complete NRPN value arrives.
        for(const auto& entry:s3g_thru_midi::startupFeedback) {
            const auto* p=s3g_thru_midi::find(entry.id);
            if(!p)continue;
            const auto index=static_cast<std::size_t>(p-s3g_thru_midi::parameters);
            t.midiWatched[static_cast<std::size_t>(entry.part+1)*s3g_thru_midi::count+index]=true;
        }
        t.midiFeedbackInitialized=true;
    }
    // Never send absolute values onto relative turn CCs.
    if(t.midiControlMode==5)return std::snprintf(buffer,size,"[]");

    int at=0,count=0;bool ok=true;
    const auto append=[&](const char* f,auto... args) {
        if(!ok)return;
        const int n=std::snprintf(buffer+at,size-at,f,args...);
        if(n<0 || n>=size-at){ok=false;return;}at+=n;
    };
    append("%s","[");
    for(std::size_t scan=0;scan<t.midiWatched.size() && count<8;++scan) {
        const auto index=t.midiFeedbackCursor;t.midiFeedbackCursor=(index+1)%t.midiWatched.size();
        if(!t.midiWatched[index])continue;
        const auto& p=s3g_thru_midi::parameters[index%s3g_thru_midi::count];
        const int part=static_cast<int>(index/s3g_thru_midi::count)-1;
        if(part>=(t.splitEnabled?t.partCount():1))continue;
        append("%s[%d,%d,%d]",count?",":"",part<0?16:part+5,p.id,
            s3g_thru_midi::encode(p,midiRead(t,p,part),midiMaximum(t,p)));++count;
    }
    append("%s","]");if(!ok){buffer[0]=0;return -1;}return at;
}

int e16RowForPage(ThruInstance& t,int part,int page) {
    const auto& c=t.liveControlsForPart(part);
    const int algorithm=clampValue(static_cast<int>(c.algorithm),0,static_cast<int>(Engine::kPlaybackAlgorithmCount)-1);
    const auto* k=s3g_thru_midi::e16Keys;
    if(page==0)return k[0];
    if(page==1)return !t.isQuad() && algorithm!=3 && algorithm!=4 && algorithm!=5?k[1]:0;
    if(page<=5) {
        const int detail=page-2;
        if(algorithm==0 && detail>=2) {
            const int sound=static_cast<int>(c.motionSound);
            if(detail==2 && sound==1)return k[2];
            if(detail==2 && sound==2)return k[2]+1;
            if(sound>=3)return k[2]+detail;
            return 0;
        }
        return s3g_thru_midi::e16EngineRows[algorithm][detail];
    }
    if(page==6)return t.isQuad()?k[6]:t.splitEnabled?k[5]:0;
    constexpr int slots[]{3,4,7,8,9};return k[slots[page-7]];
}
int e16Row(ThruInstance& t,int part) {return e16RowForPage(t,part,t.e16Page);}

const MidiParameter* e16PageSlotParameter(ThruInstance& t,int page,int slot,int& part) {
    if(t.bankMode && part==1 && page<=5) {
        const char* sample[]{"bank_start","bank_end","bank_master_rate","bank_reverse","bank_level","bank_attack","bank_release","bank_fade"};
        const char* pads[]{"bank_slot","bank_rate","bank_pan","bank_play","bank_choke","hold_release_ms","bank_record_source",nullptr};
        const char* key=slot>=0 && slot<8?(page==2?sample[slot]:page==3?pads[slot]:nullptr):nullptr;
        if(!key)return nullptr;
        const auto* p=midiParameterForKey(key);if(p && !p->part)part=-1;
        return p;
    }
    const int row=e16RowForPage(t,part,page);if(row<1 || slot<0 || slot>=8)return nullptr;
    int id=s3g_thru_midi::e16Rows[row-1][slot];
    const auto& c=t.liveControlsForPart(part);const auto* k=s3g_thru_midi::e16Keys;
    if(page==0) {
        if(slot==7 && t.splitEnabled)id=k[10];
        if(slot==5 && static_cast<int>(c.algorithm)==3)id=k[11];
        if(slot==5 && (c.algorithm==Engine::PlaybackAlgorithm::sample
            || c.algorithm==Engine::PlaybackAlgorithm::slices))return nullptr;
        if(t.isQuad() && (slot==4 || slot==5))return nullptr;
    }
    if((id==k[12] && static_cast<int>(c.prismMode)!=4)
        || (id==k[13] && static_cast<int>(c.morphMode)==1)
        || (id==k[14] && static_cast<int>(c.textureMode)==0)
        || (t.isQuad() && (id==k[15] || id==k[16])))return nullptr;
    if(id==3205 && c.algorithm==Engine::PlaybackAlgorithm::stochastic
        && c.stochasticLaw>=2.5f)id=3210;
    const auto* p=s3g_thru_midi::find(id);if(p && !p->part)part=-1;
    return p;
}

const MidiParameter* e16Parameter(ThruInstance& t,int index,int& part) {
    const bool shared=t.e16Page==6 || t.e16Page>=9;
    part=shared?0:(t.isQuad()?t.e16Pair:0)+(index%4>=2?1:0);
    if(shared?index>=8:part>=(t.splitEnabled?t.partCount():1))return nullptr;
    const int slot=shared?index:(index/4)*2+index%2;
    return e16PageSlotParameter(t,t.e16Page,slot,part);
}

// Remote uses fixed page slots; unavailable cells remain reserved.
bool e16SharedPage(int page) { return page==6 || page>=9; }
const MidiParameter* e16PhysicalParameter(ThruInstance& t,int physical,int& part) {
    if(physical<0 || physical>=16)return nullptr;
    const bool shared=e16SharedPage(t.e16Page);
    if(shared && physical>=8)return nullptr;
    const int wire=shared?physical:(physical%8/2)*4+(physical>=8?2:0)+physical%2;
    return e16Parameter(t,wire,part);
}

const MidiParameter* monolitParameter(ThruInstance& t,int bank,int slot,int& part) {
    if(bank>=0 && bank<=5)return e16PageSlotParameter(t,bank,slot,part);
    static constexpr const char* mixColor[]{
        "part_level","part_delay","part_reverb","part_mute",
        "effect_tone","effect_drive","effect_bits","effect_rate"
    };
    static constexpr const char* rotary[]{
        "effect_cross_pan","effect_rotary_rate","effect_rotary_depth",
        "effect_rotary_spread","effect_rotary_ramp","effect_rotary_balance",
        "effect_rotary_crossover","effect_rotary_mix"
    };
    static constexpr const char* performance[]{
        "clip_rate","clip_record_mode","clip_timing","launch_quantize",
        "tempo_bpm","tempo_swing","pattern_fade_ms","pattern_fade_curve"
    };
    const char* key=bank==6?mixColor[slot]:bank==7?rotary[slot]
        :bank==8?performance[slot]:nullptr;
    const auto* p=key?midiParameterForKey(key):nullptr;
    if(p && !p->part)part=-1;
    return p;
}

bool applyMonolitCc(ThruInstance& t,const MidiParameter& p,int part,
    int value,int slot,int source) {
    const bool handled=applyAbsoluteCc(t,p,part,value,slot,source);
    return handled;
}

bool handleMonolitCc(ThruInstance& t,int channel,int number,int value,int source) {
    if(!midiModeAllowsMonolit(t))return false;
    // Part controls use the same CC numbers on dedicated channels, allowing
    // A-D to be changed at the same time without a mutable target selector.
    if(channel>=9 && channel<=12 && number>=16 && number<80) {
        const int partChannel=channel-9;
        if(partChannel>=(t.splitEnabled?t.partCount():1))return true;
        const int index=number-16,bank=index/8,slot=index%8;
        int part=partChannel;const auto* p=monolitParameter(t,bank,slot,part);
        if(!p || p->action)return true;
        return applyMonolitCc(t,*p,part,value,partChannel*64+index,source);
    }
    // Bank H is shared and has one unambiguous home regardless of part mode.
    if(channel==13 && number>=72 && number<80) {
        const int slot=number-72;int part=0;
        const auto* p=monolitParameter(t,8,slot,part);
        if(!p || p->action)return true;
        return applyMonolitCc(t,*p,part,value,256+slot,source);
    }
    // Fixed shared controls bridge the two dual-mode instruments. They are
    // separate from the part channels and from the channel-13 command range.
    if(channel==13 && number>=24 && number<=33) {
        if(!t.splitEnabled || t.isQuad())return true;
        static constexpr const char* keys[]{
            "split_blend","split_curve","split_priority","split_duck_threshold",
            "split_duck_attack","split_duck_hold","split_duck_release",
            "split_duck_hpf","split_duck_knee","split_duck_route"
        };
        const auto* p=midiParameterForKey(keys[number-24]);
        return p?applyMonolitCc(t,*p,-1,value,264+number-24,source):true;
    }
    if(channel!=13 || number<16 || number>=24)return false;
    if(value==0)return true;
    const int command=number-16;
    if(command<4) {
        const int part=command;
        if(part>=(t.splitEnabled?t.partCount():1))return true;
        if(const auto* p=midiParameterForKey("stack_scan_paused"))
            midiWrite(t,*p,part,t.settingsForPart(part).stackHold?0.0f:1.0f);
    } else if(command==4)setParam(&t,"clip_record","1");
    else if(command==5)t.setPatternHeld(!t.patternHeld);
    else if(command==6)setParam(&t,"panic","1");
    // CC 23 is intentionally reserved for a future global command.
    return true;
}

// Remote retains twelve stable page positions. Unused engine pages display
// reserved cells instead of changing page numbers when an algorithm changes.
void e16RemoteNormalizePage(ThruInstance& t) {
    t.e16Page=clampValue(t.e16Page,0,11);
}
void e16RemoteNavigate(ThruInstance& t,int direction) {
    e16RemoteNormalizePage(t);
    t.e16Page=(t.e16Page+(direction<0?11:1))%12;
}

// Remote channel 1 relative turns go through serialized DSP control. This
// path does no screen work, allocation or file I/O on the audio thread.
bool handleE16RemoteMidi(ThruInstance& t,const std::uint8_t* b,int length,int source) {
    if(source!=MOVE_MIDI_SOURCE_EXTERNAL || length<3 || (b[0]&15)!=0)return false;
    if(!midiModeAllowsE16(t))return false;
    const int type=b[0]&0xf0,n=b[1]&127,value=b[2]&127;
    const bool note=(type==0x90 || type==0x80) && n<=16;
    const std::uint32_t bit=note?1u<<n:0u;
    const bool down=type==0x90 && value>0;
    if(note && !down && (t.e16RemoteHeld&bit)) {t.e16RemoteHeld&=~bit;return true;}
    if(!e16RemoteLive(t)) {
        if(note && down)t.e16RemoteHeld&=~bit;
        return false;
    }
    t.e16RemoteLease=e16RemoteClock()+8000u;
    if(note) {
        if(down)t.e16RemoteHeld|=bit;
        return true;
    }
    if(type!=0xb0 || n<1 || n>16)return false;
    if(!t.e16RemoteActive)return true;
    // Bit 17 is the UI-only Page Jump overlay. Consume encoder turns while it
    // is open so browsing pages can never alter an underlying sound parameter.
    if(t.e16RemoteButtons&(1u<<17))return true;
    const int ticks=value>0 && value<64?value:value>64?value-128:0;
    if(ticks==0 || std::abs(ticks)>8)return true;
    if(t.e16RemoteButtons&(1u<<16)) {
        if(n==1)e16RemoteNavigate(t,ticks>0?1:-1);
        return true;
    }
    e16RemoteNormalizePage(t);
    const int index=n-1;
    int part=-1;const auto* p=e16PhysicalParameter(t,index,part);if(!p || p->action)return true;
    const float maximum=midiMaximum(t,*p),current=midiRead(t,*p,part);
    const bool lane=p->id==3 && static_cast<int>(t.liveControlsForPart(part).algorithm)==3;
    float next=current;
    if(p->integer || lane)next=clampValue(current+(ticks>0?1.0f:-1.0f),p->minimum,maximum);
    else {
        const bool fine=(t.e16RemoteButtons&(1u<<index))!=0;
        const int delta=ticks*(fine?6:48);
        const int raw=clampValue(s3g_thru_midi::encode(*p,current,maximum)+delta,0,16383);
        next=s3g_thru_midi::decode(*p,raw,maximum);
    }
    if(next!=current)midiWrite(t,*p,part,next);
    return true;
}

int formatE16RemotePageState(ThruInstance& t,char* buffer,int size,bool active) {
    int at=0;bool ok=true;
    const auto append=[&](const char* fmt,auto... args) {
        if(!ok)return;
        const int n=std::snprintf(buffer+at,size-at,fmt,args...);
        if(n<0 || n>=size-at){ok=false;return;}at+=n;
    };
    const int pair=t.isQuad()?t.e16Pair:0;
    const bool shared=e16SharedPage(t.e16Page);
    append("{\"v\":2,\"active\":%d,\"mode\":%d,\"pair\":%d,\"page\":%d,\"shared\":%d,\"shift\":%d,\"pages\":[",
        active?1:0,t.performanceMode(),pair,t.e16Page,shared?1:0,
        (t.e16RemoteButtons&(1u<<16))?1:0);
    for(int i=0;i<12;++i)append("%s%d",i?",":"",i);
    append("],\"rows\":[%d,%d],\"alg\":[",e16Row(t,pair),!shared && t.splitEnabled?e16Row(t,pair+1):0);
    for(int part=0;part<4;++part)append("%s%d",part?",":"",static_cast<int>(t.liveControlsForPart(part<(t.splitEnabled?t.partCount():1)?part:0).algorithm));
    append("%s","],\"context\":[");
    for(int field=0;field<5;++field)for(int part=0;part<4;++part) {
        const auto& c=t.liveControlsForPart(part<(t.splitEnabled?t.partCount():1)?part:0);
        const int value=field==0?static_cast<int>(c.algorithm):field==1?static_cast<int>(c.motionSound):field==2?static_cast<int>(c.prismMode):field==3?static_cast<int>(c.morphMode):static_cast<int>(c.algorithm==Engine::PlaybackAlgorithm::stochastic?c.stochasticLaw:c.textureMode);
        append("%s%d",field || part?",":"",value);
    }
    append("%s","],\"values\":[");bool comma=false;
    for(int physical=0;physical<16;++physical) {
        int part=-1;const auto* p=e16PhysicalParameter(t,physical,part);if(!p)continue;
        const float maximum=midiMaximum(t,*p);
        const bool lane=p->id==3 && static_cast<int>(t.liveControlsForPart(part).algorithm)==3;
        const int steps=(p->integer || lane) && maximum-p->minimum<=127.0f?static_cast<int>(std::round(maximum-p->minimum)):0;
        append("%s[%d,%d,%d,%d,%d,%.8g]",comma?",":"",physical,
            s3g_thru_midi::encode(*p,midiRead(t,*p,part),maximum),p->id,part,steps,maximum);comma=true;
    }
    append("%s","]}");if(!ok){buffer[0]=0;return -1;}return at;
}

int formatE16RemoteState(ThruInstance& t,char* buffer,int size) {
    if(!e16RemoteLive(t))return std::snprintf(buffer,size,"{}");
    e16RemoteNormalizePage(t);
    return formatE16RemotePageState(t,buffer,size,t.e16RemoteActive);
}

// Snapshot only: no output or UI-focus changes on the DSP callback path.
int formatE16NativeState(ThruInstance& t,char* buffer,int size) {
    if(t.midiControlMode!=5 || t.e16NativePage<0 || e16RemoteClock()>t.e16NativeLease)return std::snprintf(buffer,size,"{}");
    int at=0;bool ok=true;
    const auto append=[&](const char* f,auto... args) {
        if(!ok)return;
        const int n=std::snprintf(buffer+at,size-at,f,args...);
        if(n<0 || n>=size-at){ok=false;return;}at+=n;
    };
    const int page=t.e16NativePage;const bool shared=e16SharedPage(page);
    append("{\"v\":1,\"page\":%d,\"token\":%d,\"values\":[",page,t.e16NativeToken);
    for(int physical=0;physical<16;++physical) {
        int part=physical/8,raw=0,color=0;
        if(part<(t.splitEnabled?t.partCount():1) && (!shared || physical<8)) {
            const auto* p=e16PageSlotParameter(t,page,physical%8,part);
            if(p && !p->action) {
                const float value=midiRead(t,*p,part);
                raw=std::strcmp(p->key,"hold_release_ms")==0
                    ?static_cast<int>(std::lround(e16HoldIndex(value)*16383.0/67))
                    :s3g_thru_midi::encode(*p,value,midiMaximum(t,*p));
                color=shared || physical<8?30:100;
            }
        }
        append("%s[%d,%d,%d]",physical?",":"",physical,raw,color);
    }
    append("%s","]}");if(!ok){buffer[0]=0;return -1;}return at;
}
