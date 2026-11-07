// Custom profiles resolve directly into the shared MIDI parameter path.
const MidiParameter* customMidiParameter(ThruInstance& t,const ThruInstance::CustomBinding& b,int& part) {
    using namespace s3g_thru_custom_midi;
    part=b.part==4?(t.splitEnabled?t.splitFocus:0):b.part;
    const MidiParameter* p=nullptr;
    if(b.kind==parameter)p=s3g_thru_midi::find(b.target);
    else if(b.kind==engineSlot) {
        if(part<0||part>=(t.splitEnabled?t.partCount():1))return nullptr;
        p=e16PageSlotParameter(t,2+b.page,b.slot,part);
    }
    if(p&&!t.splitEnabled&&!std::strcmp(p->key,"part_level"))p=midiParameterForKey("output_gain");
    if(p&&!p->part)part=-1;
    if(p&&p->part&&(part<0||part>=(t.splitEnabled?t.partCount():1)))return nullptr;
    return p;
}
void customMidiCommand(ThruInstance& t,int id) {
    using namespace s3g_thru_custom_midi;
    if(id>=bank1&&id<=bank8){t.selectCustomBank(id-bank1);return;}
    switch(id) {
        case play:t.setClipPlaying(true);break;
        case stop:t.setClipPlaying(false);break;
        case record:t.toggleClipRecording();break;
        case undo:t.restoreUndo();break;
        case panic:setParam(&t,"panic","1");break;
        case previousBank:if(auto* p=t.currentCustomProfile())t.selectCustomBank((p->bank+7)%8);break;
        case nextBank:if(auto* p=t.currentCustomProfile())t.selectCustomBank((p->bank+1)%8);break;
        case takeUndo:t.undoLastClipTake();break;
        case previousClip:t.selectClip(std::max(0,t.activeClip-1));break;
        case nextClip:t.selectClip(std::min(63,t.activeClip+1));break;
        case patternHold:t.setPatternHeld(!t.patternHeld);break;
        default:break;
    }
}
bool handleCustomMidiCc(ThruInstance& t,int channel,int number,int value,int source) {
    using namespace s3g_thru_custom_midi;
    if(t.learnCustomCc(channel,number,value))return true;
    if(t.midiControlMode==6)return false;
    auto* profile=t.currentCustomProfile();if(!profile)return false;
    const int index=match(*profile,channel,number);if(index<0)return false;
    const auto& b=profile->bindings[index];
    auto& runtime=t.customPickups[source==MOVE_MIDI_SOURCE_HOST?1:0][index];
    if(b.kind==command) {
        const bool pressed=value>0,edge=pressed&&!runtime.down;runtime.down=pressed;
        if(edge){customMidiCommand(t,b.target);runtime.down=pressed;}
        return true;
    }
    int part=0;const auto* p=customMidiParameter(t,b,part);if(!p)return true;
    if(runtime.id!=p->id||runtime.part!=part) {runtime={};runtime.id=p->id;runtime.part=part;}
    const bool pressed=value>0,edge=pressed&&!runtime.down;runtime.down=pressed;
    const float maximum=midiMaximum(t,*p);
    const auto rawAt=[&](float fraction) {return clampValue(static_cast<int>(std::lround(fraction*16383)),0,16383);};
    const int low=rawAt(b.low),high=rawAt(b.high);
    const auto writeRaw=[&](int raw) {
        midiWrite(t,*p,part,s3g_thru_midi::decode(*p,raw,maximum));
        runtime.written=s3g_thru_midi::encode(*p,midiRead(t,*p,part),maximum);
    };
    if(b.behavior!=absolute) {
        if(b.behavior==trigger&&edge)writeRaw(b.invert?low:high);
        else if(b.behavior==toggle&&edge) {
            const int current=s3g_thru_midi::encode(*p,midiRead(t,*p,part),maximum);
            writeRaw(current>=(low+high)/2?low:high);
        } else if(b.behavior==momentary)writeRaw((pressed!=static_cast<bool>(b.invert))?high:low);
        return true;
    }
    if(p->action)return true;
    const float fraction=(b.invert?127-value:value)/127.f;
    const int raw=low+static_cast<int>(std::lround((high-low)*fraction));
    const int current=s3g_thru_midi::encode(*p,midiRead(t,*p,part),maximum);
    const bool integer=p->integer&&maximum-p->minimum<=127;
    const auto choice=[&](int v){return static_cast<int>(std::lround(s3g_thru_midi::decode(*p,v,maximum)));};
    if(b.pickup) {
        if(runtime.engaged&&runtime.written>=0&&(integer?choice(current)!=choice(runtime.written):std::abs(current-runtime.written)>2))runtime.engaged=false;
        if(!runtime.engaged) {
            const int tolerance=std::max(1,(high-low)/254);
            const bool close=integer?choice(raw)==choice(current):std::abs(raw-current)<=tolerance;
            const bool crossed=!integer&&runtime.last>=0&&(runtime.last-current)*(raw-current)<=0;
            runtime.last=raw;
            if(!close&&!crossed)return true;
            runtime.engaged=true;
        }
    }
    runtime.last=raw;writeRaw(raw);return true;
}

int formatCustomMidiTarget(ThruInstance& t,char* out,int size) {
    int part=0;const auto* p=customMidiParameter(t,t.customDraft,part);
    if(!p)return std::snprintf(out,size,"{\"id\":-1}");
    return std::snprintf(out,size,"{\"id\":%d,\"part\":%d,\"value\":%.9g,\"max\":%.9g}",p->id,part,
        static_cast<double>(midiRead(t,*p,part)),static_cast<double>(midiMaximum(t,*p)));
}
