// Format 43 appends saved recipes and mode-specific output strips. Legacy
// trig/MIDI identities stay intact; cached generated trigs remain ordinary data.
static int storedPerformanceMode(const ClipSlot& clip){return clip.sequence.active?6:clip.layers.active?5:clip.bankMode?4:clip.splitEnabled?(clip.quadMode?3:clip.sharedDual?2:1):0;}
static bool writeGeneratorValues(std::FILE* f,const s3g_thru_sequence::Values& v){
    bool ok=writeU32(f,v.mask);for(float x:v.values)ok=ok&&writeF32(f,x);return ok&&writeU32(f,v.timeLocked?1:0)&&writeF32(f,v.timeMs);
}
static bool readGeneratorValues(std::FILE* f,s3g_thru_sequence::Values& v){
    std::uint32_t mask=0,timed=0;if(!readU32(f,mask)||mask>15)return false;v.mask=std::uint8_t(mask);
    const float lo[]{0,-48,1,.01f},hi[]{1,48,127,32};
    for(int i=0;i<4;++i)if(!readF32(f,v.values[i])||!std::isfinite(v.values[i])||v.values[i]<lo[i]||v.values[i]>hi[i])return false;
    if(!readU32(f,timed)||timed>1||!readF32(f,v.timeMs)||!std::isfinite(v.timeMs)||v.timeMs<10||v.timeMs>30000)return false;
    v.timeLocked=timed!=0;return true;
}
static bool writeGeneratorStep(std::FILE* f,const s3g_thru_sequence::Step& step){
    bool ok=writeU32(f,step.enabled?1:0)&&writeF32(f,step.micro)&&writeF32(f,step.probability)&&writeU32(f,step.count)&&writeU32(f,step.cycleHit|(step.cycleEvery<<8))&&writeGeneratorValues(f,step.locks);
    for(const auto& child:step.events){ok=ok&&writeF32(f,child.when)&&writeGeneratorValues(f,child.locks);}
    return ok;
}
static bool readGeneratorStep(std::FILE* f,s3g_thru_sequence::Step& step){
    std::uint32_t on=0,count=0,cycle=0;
    if(!readU32(f,on)||on>1||!readF32(f,step.micro)||!std::isfinite(step.micro)||std::abs(step.micro)>.5f||!readF32(f,step.probability)||!std::isfinite(step.probability)||step.probability<0||step.probability>1||!readU32(f,count)||count<1||count>8||!readU32(f,cycle))return false;
    const auto hit=cycle&255u,every=cycle>>8;if(hit<1||hit>every||every>8)return false;
    step.enabled=on!=0;step.count=int(count);step.cycleHit=std::uint8_t(hit);step.cycleEvery=std::uint8_t(every);
    if(!readGeneratorValues(f,step.locks))return false;
    for(auto& child:step.events)if(!readF32(f,child.when)||!std::isfinite(child.when)||child.when<0||child.when>=1||!readGeneratorValues(f,child.locks))return false;
    return true;
}
static bool writeOutputMix(std::FILE* f,const s3g_thru_sequence::Mix& mix){
    bool ok=true;for(const auto& s:mix.strips){const float fields[]{s.pan,s.lowDb,s.midDb,s.highDb,s.midHz,s.solo?1.f:0.f,s.muted?1.f:0.f};for(float v:fields)ok=ok&&writeF32(f,v);}
    const float fields[]{float(mix.priority),mix.depthDb,mix.thresholdDb,mix.attackMs,mix.holdMs,mix.releaseMs,mix.hpfHz,mix.kneeDb};for(float v:fields)ok=ok&&writeF32(f,v);return ok;
}
static bool readOutputMix(std::FILE* f,s3g_thru_sequence::Mix& mix){
    const auto read=[&](float& v,float lo,float hi){return readF32(f,v)&&std::isfinite(v)&&v>=lo&&v<=hi;};
    for(auto& s:mix.strips){float solo=0,mute=0;if(!read(s.pan,-1,1)||!read(s.lowDb,-12,12)||!read(s.midDb,-12,12)||!read(s.highDb,-12,12)||!read(s.midHz,120,8000)||!read(solo,0,1)||std::floor(solo)!=solo||!read(mute,0,1)||std::floor(mute)!=mute)return false;s.solo=solo!=0;s.muted=mute!=0;}
    float priority=0;if(!read(priority,0,4)||std::floor(priority)!=priority||!read(mix.depthDb,0,24)||!read(mix.thresholdDb,-60,0)||!read(mix.attackMs,1,250)||!read(mix.holdMs,0,1000)||!read(mix.releaseMs,10,3000)||!read(mix.hpfHz,20,2000)||!read(mix.kneeDb,0,24))return false;mix.priority=int(priority);return true;
}
static bool validStoredRecipe(const s3g_thru_sequence::Generator& g){
    if(g.shift<0||g.shift>31||g.type<0||g.type>2||(g.type&&!g.ready)||(g.enabledValues&~g.enabledMask))return false;
    s3g_thru_generate::Config check;check.v=g.values;
    for(int i=0;i<21;++i){
        if(!std::isfinite(g.values[i]))return false;
        // Preserve valid narrow ranges written before endpoint clamping.
        if(i==10||i==11){if(g.values[i]<0||g.values[i]>1)return false;continue;}
        auto copy=check;if(!copy.set(i,g.values[i],32,g.timed)||copy.v[i]!=g.values[i])return false;
    }
    return g.values[10]<g.values[11]&&g.values[17]<=g.values[18];
}
bool writeSequenceTypes(std::FILE* f,const std::array<ClipSlot,kClipSlotCount>& clips) const {
    // Refuse invalid recipes before publishing an unreadable bank.
    for(const auto& clip:clips)for(const auto& lane:clip.sequence.tracks)if(!validStoredRecipe(lane.generator))return false;
    const auto start=std::ftell(f);bool ok=start>=0&&writeU32(f,0x31545953u);
    for(const auto& clip:clips){
        const auto& pattern=clip.sequence;ok=ok&&writeU32(f,pattern.initialized?1:0)&&writeU32(f,pattern.mixerMode);
        for(int m=0;m<7;++m)ok=ok&&writeOutputMix(f,m==pattern.mixerMode?pattern.mix:pattern.modeMixes[m]);
        for(const auto& lane:pattern.tracks){const auto& g=lane.generator;
            ok=ok&&writeU32(f,g.type)&&writeU32(f,g.ready?1:0)&&writeU32(f,g.timed?1:0)&&writeU32(f,g.enabledMask)&&writeU32(f,g.enabledValues)&&writeU32(f,g.burstMask);
            for(float v:g.values)ok=ok&&writeF32(f,v);
            if(g.ready)for(const auto& step:g.base)ok=ok&&writeGeneratorStep(f,step);
        }
    }
    const auto end=std::ftell(f);return ok&&end>=start&&end-start<0x7fffffff&&writeU32(f,std::uint32_t(end-start));
}
bool readSequenceTypes(std::FILE* f,std::array<ClipSlot,kClipSlotCount>& clips){
    const auto start=std::ftell(f);std::uint32_t magic=0;if(start<0||!readU32(f,magic)||magic!=0x31545953u)return false;
    for(auto& clip:clips){
        auto& pattern=clip.sequence;std::uint32_t initialized=0,mode=0;
        if(!readU32(f,initialized)||initialized>1||!readU32(f,mode)||mode>6)return false;
        pattern.initialized=initialized!=0;pattern.mixerMode=int(mode);
        for(auto& mix:pattern.modeMixes){if(!readOutputMix(f,mix))return false;}
        pattern.mix=pattern.modeMixes[mode];
        for(auto& lane:pattern.tracks){auto& g=lane.generator;std::uint32_t type=0,ready=0,timed=0;
            if(!readU32(f,type)||type>2||!readU32(f,ready)||ready>1||!readU32(f,timed)||timed>1||!readU32(f,g.enabledMask)||!readU32(f,g.enabledValues)||!readU32(f,g.burstMask))return false;
            g.type=int(type);g.ready=ready!=0;g.timed=timed!=0;
            for(int i=0;i<21;++i){float value=0;if(!readF32(f,value)||!std::isfinite(value))return false;g.values[i]=value;}
            // Recover only the two known endpoint-equality cases emitted by
            // earlier test builds; other invalid ranges still fail validation.
            if(g.values[10]==1.f&&g.values[11]==1.f)g.values[10]=1.f-.00001f;
            if(g.values[10]==0.f&&g.values[11]==0.f)g.values[11]=.00001f;
            if(!validStoredRecipe(g))return false;
            if(g.ready)for(auto& step:g.base)if(!readGeneratorStep(f,step))return false;
            if(g.type&&!g.ready)return false;
        }
    }
    const auto end=std::ftell(f);std::uint32_t length=0;return end>=start&&readU32(f,length)&&length==std::uint32_t(end-start);
}
static void migrateSequenceTypes(ClipSlot& clip,std::uint32_t version){
    auto& pattern=clip.sequence;pattern.initialized=version>=38;
    const auto old=pattern.mix;pattern.modeMixes={};pattern.modeMixes[6]=old;
    pattern.mixerMode=storedPerformanceMode(clip);pattern.mix=pattern.modeMixes[pattern.mixerMode];
}

static std::uint32_t defaultSequenceDirectionSeed(std::size_t clip,int part){
    return s3g_thru_sequence::hash(0x44495231u^(std::uint32_t(clip+1)*0x9e3779b9u)^(std::uint32_t(part+1)*0x85ebca6bu))|1u;
}
bool writeSequenceDirections(std::FILE* f,const std::array<ClipSlot,kClipSlotCount>& clips) const {
    bool ok=writeU32(f,0x31494453u);
    for(std::size_t i=0;i<clips.size();++i)for(int p=0;p<4;++p){const auto& lane=clips[i].sequence.tracks[p];
        ok=ok&&writeU32(f,std::uint32_t(lane.direction))&&writeU32(f,lane.directionSeed?lane.directionSeed:defaultSequenceDirectionSeed(i,p));
    }
    return ok;
}
static bool readSequenceDirections(std::FILE* f,std::array<ClipSlot,kClipSlotCount>& clips){
    std::uint32_t magic=0;if(!readU32(f,magic)||magic!=0x31494453u)return false;
    for(auto& clip:clips)for(auto& lane:clip.sequence.tracks){std::uint32_t direction=0;
        if(!readU32(f,direction)||direction>4||!readU32(f,lane.directionSeed)||!lane.directionSeed)return false;
        lane.direction=int(direction);
    }
    return true;
}
static void migrateSequenceDirections(std::array<ClipSlot,kClipSlotCount>& clips){
    for(std::size_t i=0;i<clips.size();++i)for(int p=0;p<4;++p){auto& lane=clips[i].sequence.tracks[p];lane.direction=0;lane.directionSeed=defaultSequenceDirectionSeed(i,p);}
}

// Format 45 adds hit displacement without changing earlier recipe/curve IDs.
bool writeSequenceShifts(std::FILE* f,const std::array<ClipSlot,kClipSlotCount>& clips) const {
    bool ok=writeU32(f,0x31544853u); // SHT1
    for(const auto& clip:clips)for(const auto& lane:clip.sequence.tracks)
        ok=ok&&lane.generator.shift>=0&&lane.generator.shift<=31&&writeU32(f,std::uint32_t(lane.generator.shift));
    return ok;
}
static bool readSequenceShifts(std::FILE* f,std::array<ClipSlot,kClipSlotCount>& clips){
    std::uint32_t magic=0;if(!readU32(f,magic)||magic!=0x31544853u)return false;
    for(auto& clip:clips)for(auto& lane:clip.sequence.tracks){std::uint32_t shift=0;
        if(!readU32(f,shift)||shift>31)return false;
        lane.generator.shift=int(shift);
    }
    return true;
}

// Format 46 appends grid paths; older banks default to HORIZ.
bool writeSequencePaths(std::FILE* f,const std::array<ClipSlot,kClipSlotCount>& clips) const {
    bool ok=writeU32(f,0x31485450u); // PTH1
    for(const auto& clip:clips)for(const auto& lane:clip.sequence.tracks)
        ok=ok&&lane.path>=0&&lane.path<=2&&writeU32(f,std::uint32_t(lane.path));
    return ok;
}
static bool readSequencePaths(std::FILE* f,std::array<ClipSlot,kClipSlotCount>& clips){
    std::uint32_t magic=0;if(!readU32(f,magic)||magic!=0x31485450u)return false;
    for(auto& clip:clips)for(auto& lane:clip.sequence.tracks){std::uint32_t path=0;
        if(!readU32(f,path)||path>2)return false;
        lane.path=int(path);
    }
    return true;
}
