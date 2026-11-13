    void normalizeVoiceEngineEditors() {
        for(auto& part:splitParts) {
            auto& cfg=part.settings.engines;
            cfg.edit=clampValue(cfg.edit,0,engineSlotCount()-1);
            if(cfg.enabled)part.controls.algorithm=static_cast<Engine::PlaybackAlgorithm>(cfg.engines[cfg.edit]);
        }
    }
    bool engineListVoicesActive() const {
        return std::any_of(voices.begin(),voices.end(),[&](const auto& v){return v.active&&(!splitEnabled||v.part==splitFocus)&&v.assignedAlgorithm>=0;});
    }
    int engineSlotCount() const {return splitEnabled?voicesPerPart():8;}
    static int engineSlotKey(const char* key) {
        constexpr const char* prefix="voice_engine_";
        return std::strncmp(key,prefix,13)==0&&key[13]>='1'&&key[13]<='8'&&key[14]=='\0'?key[13]-'1':-1;
    }
    void ensureVoiceEngineList() {
        if(voiceEngineSettings.configured)return;
        voiceEngineSettings.engines.fill(static_cast<std::uint8_t>(controls.algorithm));
        voiceEngineSettings.configured=true;
    }
    std::uint32_t nextVoiceEngineRandom() {
        auto x=voiceEngineRandom;x^=x<<13;x^=x>>17;x^=x<<5;voiceEngineRandom=x;return x;
    }
    int keyboardEngineSlot(int note,const PartSettings& owner) const {
        const auto& scale=kScaleDefinitions[static_cast<std::size_t>(owner.scale)];
        const int cap=engineSlotCount();
        if(owner.engines.assignment==1) {
            const int pitch=(note-owner.root+1320)%12;int degree=0,distance=99;
            for(std::size_t i=0;i<scale.count;++i) {
                const int gap=std::abs(pitch-scale.intervals[i]);const int d=std::min(gap,12-gap);
                if(d<distance){distance=d;degree=static_cast<int>(i);}
            }
            return degree%cap;
        }
        const int rows=isQuad()?2:4,columns=splitEnabled||layersLive.active?4:8;
        const int origin=(!splitEnabled&&owner.scale==0?kPadBaseNote:owner.root-12)+owner.octave*12;
        const int low=clampValue(origin,0,127);
        const int degree=(rows-1)*3+columns-1;
        const int high=clampValue(origin+(owner.scale?degree/static_cast<int>(scale.count)*12+scale.intervals[degree%scale.count]:(rows-1)*kPadRowInterval+columns-1),0,127);
        return clampValue((note-low)*cap/std::max(1,high-low+1),0,cap-1);
    }
    void assignVoiceEngine(PlaybackVoice& target,int note) {
        const auto owner=settingsForPart(target.part);const auto& cfg=owner.engines;
        target.engineSlot=target.assignedAlgorithm=-1;target.entryVariation={};
        if(!cfg.enabled)return;
        const int cap=engineSlotCount(),part=splitEnabled?target.part:0;
        int slot=0;
        if(cfg.assignment!=0)slot=keyboardEngineSlot(note,owner);
        else {
            std::array<int,8> available{};int count=0;unsigned freeMask=0;
            for(int entry=0;entry<cap;++entry) {
                bool used=false;
                for(const auto& v:voices)if(&v!=&target&&(!splitEnabled||v.part==target.part)&&v.active&&v.held&&v.engineSlot==entry){used=true;break;}
                if(!used){available[count++]=entry;freeMask|=1u<<entry;}
            }
            auto& traversal=voiceEngineTraversal[part];
            if(count>0) {
                if(cfg.order==3||cfg.order==4) {
                    const int direction=cfg.order==4?-1:1;
                    if(traversal.next<0)traversal.next=direction<0?cap-1:0;
                    for(int n=0;n<cap;++n) {
                        const int candidate=(traversal.next+direction*n+cap*2)%cap;
                        if(freeMask&(1u<<candidate)){slot=candidate;break;}
                    }
                    traversal.next=(slot+direction+cap)%cap;
                } else if(cfg.order==5) {
                    unsigned pool=traversal.shuffle&freeMask;
                    if(!pool){traversal.shuffle=static_cast<std::uint8_t>((1u<<cap)-1u);pool=freeMask;}
                    std::array<int,8> choices{};int n=0;
                    for(int i=0;i<cap;++i)if(pool&(1u<<i))choices[n++]=i;
                    slot=choices[(static_cast<std::uint64_t>(nextVoiceEngineRandom())*n)>>32u];
                    traversal.shuffle&=static_cast<std::uint8_t>(~(1u<<slot));
                } else {
                    int index=cfg.order==1?count-1:0;
                    if(cfg.order==2)index=static_cast<int>((static_cast<std::uint64_t>(nextVoiceEngineRandom())*count)>>32u);
                    slot=available[index];
                }
            }
        }
        target.engineSlot=slot;target.assignedAlgorithm=cfg.engines[slot];target.entryVariation=cfg.variations[slot];
    }
    std::size_t voiceVariationLane(std::size_t base,const PlaybackVoice& voice) const {
        if(voice.entryVariation.stack==0)return base;
        const int algorithm=voice.assignedAlgorithm>=0?voice.assignedAlgorithm:static_cast<int>(liveControlsForPart(voice.part).algorithm);
        if(algorithm!=4&&algorithm!=5)return base;
        std::array<std::size_t,4> lanes{};int n=0,at=0;
        for(std::size_t i=0;i<4;++i)if(engine.laneLoaded(i)&&(!splitEnabled||partOwnsLane(voice.part,i))) {if(i==base)at=n;lanes[n++]=i;}
        if(!n)return base;
        return lanes[clampValue(at+static_cast<int>(std::lround(voice.entryVariation.stack*(n-1))),0,n-1)];
    }
    static float voiceSetupValue(const VoiceEngineSettings& cfg,const char* key,int cap) {
        const auto& entry=cfg.variations[clampValue(cfg.edit,0,cap-1)];
        if(std::strcmp(key,"voice_engine_assign")==0)return static_cast<float>(cfg.assignment);
        if(std::strcmp(key,"voice_engine_depth")==0)return cfg.pressureDepth*100;
        if(std::strcmp(key,"voice_engine_octave")==0)return static_cast<float>(entry.octave);
        if(std::strcmp(key,"voice_engine_position")==0)return entry.position*100;
        if(std::strcmp(key,"voice_engine_stack")==0)return entry.stack*100;
        if(std::strcmp(key,"voice_engine_pan")==0)return entry.pan*100;
        return std::numeric_limits<float>::quiet_NaN();
    }
    void advanceVoiceExpression(PlaybackVoice& voice,std::size_t frames) {
        for(int field=0;field<3;++field)if(voice.midiValue[field]!=voice.midiTarget[field]) {
            const float slew=1-std::exp(-static_cast<float>(frames)/(sampleRate*.02f));
            voice.midiValue[field]+=(voice.midiTarget[field]-voice.midiValue[field])*slew;
            if(std::abs(voice.midiValue[field]-voice.midiTarget[field])<1e-6f)voice.midiValue[field]=voice.midiTarget[field];
        }
        const auto owner=settingsForPart(voice.part);const float depth=owner.engines.pressureDepth;
        const bool character=owner.pressure==3||(!isQuad()&&owner.pressure==4);
        const float target=character?clampValue(voice.pressureTarget*depth,0.0f,1.0f):0;
        if(target!=voice.pressureState) {
            const float slew=1-std::exp(-static_cast<float>(frames)/(sampleRate*.02f));
            voice.pressureState+=(target-voice.pressureState)*slew;
        }
        if(std::abs(target-voice.pressureState)<1e-6f)voice.pressureState=target;
        const bool brake=owner.pressure==2&&!isQuad()&&depth>0;
        const float threshold=.2f+.6f*(1-depth);
        if(!brake||voice.pressureTarget<threshold-.1f)voice.pressureBrakeHeld=false;
        else if(!voice.pressureBrakeHeld&&voice.pressureTarget>=threshold) {
            voice.pressureBrakePhase=currentVoiceStackPhase(voice);voice.pressureBrakeHeld=true;
        }
    }
    static void applyVoiceCharacter(Engine::Controls& c,float amount) {
        if(amount<=0)return;
        switch(c.algorithm) {
            case Engine::PlaybackAlgorithm::grains:c.grainDensityHz=clampValue(c.grainDensityHz*std::exp2(2*amount),1.0f,96.0f);break;
            case Engine::PlaybackAlgorithm::longGrains:c.longGrainSpread=clampValue(c.longGrainSpread+(1-c.longGrainSpread)*amount,0.0f,1.0f);break;
            case Engine::PlaybackAlgorithm::prism:c.prismSmear=clampValue(c.prismSmear+(1-c.prismSmear)*amount,0.0f,1.0f);break;
            case Engine::PlaybackAlgorithm::morph:c.morphAmplitude=clampValue(c.morphAmplitude+(1-c.morphAmplitude)*amount,0.0f,1.0f);break;
            case Engine::PlaybackAlgorithm::texture:c.textureScatter=clampValue(c.textureScatter+(1-c.textureScatter)*amount,0.0f,1.0f);break;
            case Engine::PlaybackAlgorithm::pulsar:c.pulsarForm=clampValue(c.pulsarForm*std::exp2(2*amount),s3g_daisy::ThruPulsar::minimumFormHz,s3g_daisy::ThruPulsar::maximumFormHz);break;
            case Engine::PlaybackAlgorithm::stochastic:c.stochasticAmplitude=clampValue(c.stochasticAmplitude+(1-c.stochasticAmplitude)*amount,0.f,1.f);c.stochasticTiming=clampValue(c.stochasticTiming+(1-c.stochasticTiming)*amount,0.f,1.f);break;
            case Engine::PlaybackAlgorithm::formant:c.formantShift=clampValue(c.formantShift+12*amount,-24.0f,24.0f);break;
            default:break;
        }
    }
    float voiceToneAmount(const PlaybackVoice& voice) const {
        const int algorithm=voice.assignedAlgorithm>=0?voice.assignedAlgorithm:static_cast<int>((voice.clipOwned?splitEnabled?splitClipControls[voice.part]:clipControls:liveControlsForPart(voice.part)).algorithm);
        return algorithm==2||algorithm==7||algorithm==9||algorithm==10||algorithm==11||algorithm==12||algorithm==13||algorithm==14?0:std::max(voice.pressureState,voice.midiValue[2]);
    }
    bool setVoiceEngineParam(const char* key,const char* value) {
        auto& cfg=voiceEngineSettings;const int slot=engineSlotKey(key);
        auto& entry=cfg.variations[clampValue(cfg.edit,0,engineSlotCount()-1)];
        auto& traversal=voiceEngineTraversal[splitEnabled?splitFocus:0];
        if(std::strcmp(key,"voice_engine_mode")==0) {
            const bool enable=isTruthy(value)||std::strcmp(value,"LIST")==0;
            if(enable&&!cfg.enabled) {
                for(auto& v:voices)if(v.active&&(!splitEnabled||v.part==splitFocus)&&v.assignedAlgorithm<0)
                    v.assignedAlgorithm=static_cast<int>(effectiveControls(v).algorithm);
                ensureVoiceEngineList();
            }
            if(enable!=cfg.enabled)traversal={};
            cfg.enabled=enable;
            if(enable)controls.algorithm=static_cast<Engine::PlaybackAlgorithm>(cfg.engines[std::min(cfg.edit,engineSlotCount()-1)]);
        } else if(std::strcmp(key,"voice_engine_order")==0) {
            const int order=clampValue(parseInteger(value,cfg.order),0,5);
            if(order!=cfg.order){traversal={};if(order==4)traversal.next=engineSlotCount()-1;}
            cfg.order=order;
        } else if(std::strcmp(key,"voice_engine_assign")==0){const int assignment=clampValue(parseInteger(value,cfg.assignment),0,2);if(assignment!=cfg.assignment){cfg.assignment=assignment;traversal={};if(cfg.order==4)traversal.next=engineSlotCount()-1;}}
        else if(std::strcmp(key,"voice_engine_depth")==0)cfg.pressureDepth=clampValue(parseFloat(value,cfg.pressureDepth*100)*.01f,0.0f,1.0f);
        else if(std::strcmp(key,"voice_engine_octave")==0)entry.octave=clampValue(parseInteger(value,entry.octave),-2,2);
        else if(std::strcmp(key,"voice_engine_position")==0)entry.position=clampValue(parseFloat(value,entry.position*100)*.01f,-1.0f,1.0f);
        else if(std::strcmp(key,"voice_engine_stack")==0)entry.stack=clampValue(parseFloat(value,entry.stack*100)*.01f,-1.0f,1.0f);
        else if(std::strcmp(key,"voice_engine_pan")==0)entry.pan=clampValue(parseFloat(value,entry.pan*100)*.01f,-1.0f,1.0f);
        else if(std::strcmp(key,"voice_engine_edit")==0) {
            cfg.edit=clampValue(parseInteger(value,cfg.edit+1)-1,0,engineSlotCount()-1);
            if(cfg.enabled)controls.algorithm=static_cast<Engine::PlaybackAlgorithm>(cfg.engines[cfg.edit]);
        } else if(slot>=0) {
            if(slot>=engineSlotCount())return true;
            ensureVoiceEngineList();
            cfg.engines[slot]=static_cast<std::uint8_t>(clampValue(parseInteger(value,cfg.engines[slot]),0,static_cast<int>(Engine::kPlaybackAlgorithmCount)-1));
            if(cfg.enabled&&slot==cfg.edit)controls.algorithm=static_cast<Engine::PlaybackAlgorithm>(cfg.engines[slot]);
        } else return false;
        markPersistentStateDirty();return true;
    }
    int getVoiceEngineParam(const char* key,char* buffer,int size) const {
        const auto& cfg=voiceEngineSettings;
        const float setup=voiceSetupValue(cfg,key,engineSlotCount());
        if(std::isfinite(setup))return std::snprintf(buffer,size,"%.6g",static_cast<double>(setup));
        if(std::strcmp(key,"voice_engine_state")==0) {
            int at=std::snprintf(buffer,size,"%d,%d,%d",cfg.enabled,cfg.order,std::min(cfg.edit,engineSlotCount()-1)+1);
            if(at<0||at>=size)return -1;
            for(int i=0;i<8;++i) {
                const int n=std::snprintf(buffer+at,size-at,",%d",cfg.configured?cfg.engines[i]:static_cast<int>(controls.algorithm));
                if(n<0||n>=size-at)return -1;
                at+=n;
            }
            const auto& entry=cfg.variations[clampValue(cfg.edit,0,engineSlotCount()-1)];
            const int n=std::snprintf(buffer+at,size-at,",%d,%d,%.6g,%d,%.6g,%.6g,%.6g",engineListVoicesActive()?1:0,cfg.assignment,static_cast<double>(cfg.pressureDepth*100),entry.octave,static_cast<double>(entry.position*100),static_cast<double>(entry.stack*100),static_cast<double>(entry.pan*100));
            return n<0||n>=size-at?-1:at+n;
        }
        int value=0;const int slot=engineSlotKey(key);
        if(std::strcmp(key,"voice_engine_active")==0)value=engineListVoicesActive()?1:0;
        else if(std::strcmp(key,"voice_engine_mode")==0)value=cfg.enabled;
        else if(std::strcmp(key,"voice_engine_order")==0)value=cfg.order;
        else if(std::strcmp(key,"voice_engine_edit")==0)value=std::min(cfg.edit,engineSlotCount()-1)+1;
        else if(slot>=0)value=cfg.configured?cfg.engines[slot]:static_cast<int>(controls.algorithm);
        else return -2;
        return std::snprintf(buffer,size,"%d",value);
    }
    bool listUsesAlgorithm(int algorithm) const {
        for(int part=0;part<(splitEnabled?partCount():1);++part) {
            const auto cfg=settingsForPart(part).engines;
            if(cfg.enabled)for(int i=0;i<engineSlotCount();++i)if(cfg.engines[i]==algorithm)return true;
        }
        return false;
    }
    static bool clipUsesVoiceEngine(const ClipSlot& clip,int algorithm) {
        for(int part=0;part<(clip.splitEnabled?(clip.quadMode?4:2):1);++part) {
            const auto& cfg=(clip.splitEnabled?clip.splitSettings[part]:clip.singleSettings).engines;
            if(cfg.enabled)for(int slot=0;slot<(clip.splitEnabled?(clip.quadMode?2:4):8);++slot)
                if(cfg.engines[slot]==algorithm)return true;
        }
        for(std::size_t i=0;i<clip.clipNoteCount;++i)if(clip.clipNotes[i].engineAlgorithm==algorithm)return true;
        return false;
    }
    static bool writeVoiceEngineExtension(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips) {
        if(!writeU32(file,0x56454e31u))return false;
        for(const auto& clip:clips)for(int group=0;group<5;++group) {
            const auto& c=(group?clip.splitSettings[group-1]:clip.singleSettings).engines;
            if(!writeU32(file,c.enabled)||!writeU32(file,c.order)||!writeU32(file,c.edit)||!writeU32(file,c.configured))return false;
            for(auto engine:c.engines)if(!writeU32(file,engine))return false;
        }
        return true;
    }
    static bool readVoiceEngineExtension(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips,std::uint32_t version=24u) {
        std::uint32_t magic=0;if(!readU32(file,magic)||magic!=0x56454e31u)return false;
        for(auto& clip:clips)for(int group=0;group<5;++group) {
            auto& c=(group?clip.splitSettings[group-1]:clip.singleSettings).engines;
            std::uint32_t enabled=0,order=0,edit=0,configured=0;
            if(!readU32(file,enabled)||enabled>1||!readU32(file,order)||order>(version>=25u?5u:2u)||!readU32(file,edit)||edit>7||!readU32(file,configured)||configured>1)return false;
            c.enabled=enabled;c.order=order;c.edit=edit;c.configured=configured;
            for(auto& engine:c.engines){std::uint32_t n=0;if(!readU32(file,n)||n>=(version>=29u?15u:version>=26u?14u:13u))return false;engine=static_cast<std::uint8_t>(n);}
        }
        return true;
    }
    template<class Append>static void appendVoiceEngineJson(const Append& append,int group,const VoiceEngineSettings& c) {
        append(",\"voice_engines_%d\":[%d,%d,%d,%d",group,c.enabled?1:0,c.order,c.edit,c.configured?1:0);
        for(auto a:c.engines)append(",%d",static_cast<int>(a));
        append("%s","]");
    }
    static bool readVoiceEngineJson(const char* json,int group,VoiceEngineSettings& c) {
        char key[32]{};std::snprintf(key,sizeof(key),"\"voice_engines_%d\":[",group);
        const auto* at=std::strstr(json,key);if(!at)return true;at+=std::strlen(key);
        std::array<int,12> values{};
        for(std::size_t i=0;i<values.size();++i) {
            char* end=nullptr;const long n=std::strtol(at,&end,10);
            if(end==at||n<0||n>(i==0||i==3?1:i==1?5:i==2?7:static_cast<int>(Engine::kPlaybackAlgorithmCount)-1)||*end!=(i+1<values.size()?',':']'))return false;
            values[i]=static_cast<int>(n);at=end+1;
        }
        c.enabled=values[0];c.order=values[1];c.edit=values[2];c.configured=values[3];
        for(int i=0;i<8;++i)c.engines[i]=static_cast<std::uint8_t>(values[i+4]);
        return true;
    }

    static bool writeVoiceEntry(std::FILE* file,const VoiceEntryVariation& entry) {
        return writeU32(file,static_cast<std::uint32_t>(entry.octave+2))&&writeF32(file,entry.position)&&writeF32(file,entry.stack)&&writeF32(file,entry.pan);
    }
    static bool readVoiceEntry(std::FILE* file,VoiceEntryVariation& entry) {
        std::uint32_t octave=0;
        if(!readU32(file,octave)||octave>4||!readF32(file,entry.position)||std::abs(entry.position)>1||!readF32(file,entry.stack)||std::abs(entry.stack)>1||!readF32(file,entry.pan)||std::abs(entry.pan)>1)return false;
        entry.octave=static_cast<int>(octave)-2;return true;
    }
    static bool writeVoicePlayExtension(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips) {
        const long start=std::ftell(file);if(start<0||!writeU32(file,0x56504c31u))return false;
        for(const auto& clip:clips)for(int group=0;group<5;++group) {
            const auto& cfg=(group?clip.splitSettings[group-1]:clip.singleSettings).engines;
            if(!writeU32(file,cfg.assignment)||!writeF32(file,cfg.pressureDepth))return false;
            for(const auto& entry:cfg.variations)if(!writeVoiceEntry(file,entry))return false;
        }
        for(const auto& clip:clips) {
            if(!writeU32(file,clip.clipNoteCount))return false;
            for(std::size_t n=0;n<clip.clipNoteCount;++n)if(!writeVoiceEntry(file,clip.clipNotes[n].engineVariation)||!writeF32(file,clip.clipNotes[n].initialPressure))return false;
        }
        const long size=std::ftell(file)-start+4;
        return size>0&&size<2000000&&writeU32(file,static_cast<std::uint32_t>(size));
    }
    static bool readVoicePlayExtension(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips) {
        const long start=std::ftell(file);std::uint32_t magic=0;
        if(start<0||!readU32(file,magic)||magic!=0x56504c31u)return false;
        for(auto& clip:clips)for(int group=0;group<5;++group) {
            auto& cfg=(group?clip.splitSettings[group-1]:clip.singleSettings).engines;std::uint32_t assign=0;
            if(!readU32(file,assign)||assign>2||!readF32(file,cfg.pressureDepth)||cfg.pressureDepth<0||cfg.pressureDepth>1)return false;
            cfg.assignment=assign;for(auto& entry:cfg.variations)if(!readVoiceEntry(file,entry))return false;
        }
        for(auto& clip:clips) {
            std::uint32_t count=0;if(!readU32(file,count)||count!=clip.clipNoteCount)return false;
            for(std::size_t n=0;n<count;++n) {
                auto& note=clip.clipNotes[n];
                if(!readVoiceEntry(file,note.engineVariation)||!readF32(file,note.initialPressure)||note.initialPressure<0||note.initialPressure>1)return false;
            }
        }
        const long size=std::ftell(file)-start+4;std::uint32_t stored=0;
        return readU32(file,stored)&&size==stored;
    }
    template<class Append>static void appendVoicePlayJson(const Append& append,int group,const VoiceEngineSettings& cfg) {
        append(",\"voice_play_%d\":[%d,%.9g",group,cfg.assignment,static_cast<double>(cfg.pressureDepth));
        for(const auto& entry:cfg.variations)append(",%d,%.9g,%.9g,%.9g",entry.octave,static_cast<double>(entry.position),static_cast<double>(entry.stack),static_cast<double>(entry.pan));
        append("%s","]");
    }
    static bool readVoicePlayJson(const char* json,int group,VoiceEngineSettings& cfg) {
        char key[32]{};std::snprintf(key,sizeof(key),"\"voice_play_%d\":[",group);
        const auto* at=std::strstr(json,key);if(!at)return true;at+=std::strlen(key);
        std::array<float,34> values{};
        for(std::size_t n=0;n<values.size();++n) {
            char* end=nullptr;const float value=std::strtof(at,&end);
            const bool integer=n==0||(n>=2&&(n-2)%4==0);
            const float low=n<2?0.f:integer?-2.f:-1.f,high=n==0?2.f:n==1?1.f:integer?2.f:1.f;
            if(end==at||!std::isfinite(value)||value<low||value>high||(integer&&value!=std::round(value))||*end!=(n+1<values.size()?',':']'))return false;
            values[n]=value;at=end+1;
        }
        cfg.assignment=static_cast<int>(values[0]);cfg.pressureDepth=values[1];
        for(int n=0;n<8;++n)cfg.variations[n]={static_cast<int>(values[2+n*4]),values[3+n*4],values[4+n*4],values[5+n*4]};
        return true;
    }
