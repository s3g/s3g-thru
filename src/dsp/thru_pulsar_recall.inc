// Saved per-clip/part streams, with no dependency on voice allocation or UI time.
// A zero in an older clip is normalized once to a stable slot/part default.
    static std::uint32_t defaultPulsarSeed(int clip, int part) {
        return s3g_daisy::ThruPulsar::mixSeed(0x50554c53u
            ^ (std::uint32_t(clip + 1) * 0x9e3779b9u)
            ^ (std::uint32_t(part + 1) * 0x85ebca6bu));
    }
    static std::array<std::uint32_t,4> normalizedPulsarSeeds(const ClipSlot& clip, int index) {
        auto seeds=clip.pulsarSeeds;
        for(int part=0;part<4;++part)if(!seeds[part])seeds[part]=defaultPulsarSeed(index,part);
        return seeds;
    }
    static void ensurePulsarSeeds(ClipSlot& clip, int index) {
        clip.pulsarSeeds=normalizedPulsarSeeds(clip,index);
    }
    std::uint32_t recordedPulsarSeed(const ClipSlot& clip, const ClipNote& event,
                                      int part, std::uint64_t cycle) const {
        auto seed=normalizedPulsarSeeds(clip,activeClip)[part];
        const auto add=[&](std::uint32_t value) {seed=s3g_daisy::ThruPulsar::combineSeed(seed,value);};
        // Semantic event identity survives sorting and unrelated note edits.
        // Exact duplicate events deliberately share a stream. Velocity and
        // duration edits retain it; legato pitch changes keep the running stream.
        add(event.startMs);add(event.note);add(event.sourcePad);add(event.part);
        add(event.take);add(event.engineSlot);
        add(std::uint32_t(cycle));add(std::uint32_t(cycle>>32));
        return seed;
    }
    std::uint32_t sequencePulsarSeed(int part,int step,int child,std::int64_t tick) const {
        const auto* clip=activeClipSlot();
        auto seed=clip?normalizedPulsarSeeds(*clip,activeClip)[part]:defaultPulsarSeed(activeClip,part);
        seed=s3g_daisy::ThruPulsar::combineSeed(seed,std::uint32_t(step));
        seed=s3g_daisy::ThruPulsar::combineSeed(seed,std::uint32_t(child));
        const auto position=std::uint64_t(tick>=0?tick:0);
        seed=s3g_daisy::ThruPulsar::combineSeed(seed,std::uint32_t(position));
        return s3g_daisy::ThruPulsar::combineSeed(seed,std::uint32_t(position>>32));
    }
    static bool writePulsarSeeds(std::FILE* file,const std::array<ClipSlot,kClipSlotCount>& clips) {
        if(!writeU32(file,0x31455350u))return false; // PSE1
        for(std::size_t i=0;i<clips.size();++i)
            for(auto seed:normalizedPulsarSeeds(clips[i],int(i)))if(!writeU32(file,seed))return false;
        return true;
    }
    static bool readPulsarSeeds(std::FILE* file,std::array<ClipSlot,kClipSlotCount>& clips) {
        std::uint32_t magic=0;
        if(!readU32(file,magic)||magic!=0x31455350u)return false;
        for(auto& clip:clips)for(auto& seed:clip.pulsarSeeds)
            if(!readU32(file,seed)||!seed)return false;
        return true;
    }
    bool restorePulsarSeedsJson(const char* json) {
        std::array<std::uint32_t,4> seeds {};
        // Parse as integers; float JSON helpers cannot preserve a 32-bit seed.
        for(int part=0;part<4;++part) {
            char key[32];std::snprintf(key,sizeof(key),"\"pulsar_seed_%d\"",part);
            const char* field=std::strstr(json,key);
            if(field) {
                field=std::strchr(field+std::strlen(key),':');if(!field)return false;
                ++field;while(*field==' '||*field=='\t'||*field=='\r'||*field=='\n')++field;
                if(*field<'0'||*field>'9')return false;
                errno=0;char* end=nullptr;const auto value=std::strtoull(field,&end,10);
                if(errno==ERANGE||!value||value>0xffffffffu)return false;
                while(*end==' '||*end=='\t'||*end=='\r'||*end=='\n')++end;
                if(*end!=','&&*end!='}')return false;
                seeds[part]=std::uint32_t(value);
            } else seeds[part]=defaultPulsarSeed(activeClip,part);
        }
        if(auto* clip=activeClipSlot())clip->pulsarSeeds=seeds;
        pulsarClipCycle=0;
        return true;
    }
