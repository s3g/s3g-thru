// Live parameter getters are serviced on Schwung's SPI/audio callback.
// Keep snapshots bounded and separate moving cursors from slower menu data.
// This uses the existing single-get ABI, including on hosts without bulk APIs.
int formatUiPoll(void* opaque, const char* group, char* buffer, int capacity)
{
    if (!opaque || !group || !buffer || capacity<1) return -1;
    const char* const* keys=nullptr;
    std::size_t count=0;
    static constexpr const char* fast[]{"seq_cursor_state","ui_state","clip_state","playheads","voice_lanes","voice_pads","voice_notes","midi_state"};
    if(!std::strcmp(group,"fast")){keys=fast;count=sizeof(fast)/sizeof(*fast);}
    static constexpr const char* sequence[]{"seq_state"};
    if(!std::strcmp(group,"sequence")){keys=sequence;count=sizeof(sequence)/sizeof(*sequence);}
    static constexpr const char* status[]{"algorithm","split_state","voice_engine_state","voice_algorithms","save_status","mix_status","preview_route","pad_mode","scale","root_note","undo_available","stack_scan_paused","clip_stack_hold","glitch_buffer_lock"};
    if(!std::strcmp(group,"status")){keys=status;count=sizeof(status)/sizeof(*status);}
    static constexpr const char* sources[]{"waveforms","region_state","cue_state","sample_load_state","lane_source","lane_source_state"};
    if(!std::strcmp(group,"sources")){keys=sources;count=sizeof(sources)/sizeof(*sources);}
    static constexpr const char* composition[]{"clip_bank_state","clip_take_state","clip_capacity_state","clip_prepare_state","chain_state","chain_entries","pattern_fade_state","chain_fades_state","effect_order_state"};
    if(!std::strcmp(group,"composition")){keys=composition;count=sizeof(composition)/sizeof(*composition);}
    static constexpr const char* diagnostics[]{"performance_state","callback_performance_state","render_timing_state","render_profile_state","pattern_worker_state","limiter_state","spectral_analysis_state","waveset_analysis_state","waveset_state","cutups_state","roll_pads","roll_notes","pulsar_state"};
    if(!std::strcmp(group,"diagnostics")){keys=diagnostics;count=sizeof(diagnostics)/sizeof(*diagnostics);}
    static constexpr const char* midi[]{"midi_custom_state","midi_control_mode"};
    if(!std::strcmp(group,"midi")){keys=midi;count=sizeof(midi)/sizeof(*midi);}
    if(!keys){buffer[0]=0;return -1;}
    int at=0;bool ok=true;
    const auto put=[&](char c){if(at>=capacity-1){ok=false;return;}buffer[at++]=c;};
    const auto literal=[&](const char* text){while(*text)put(*text++);};
    const auto quoted=[&](const char* text){
        put('"');
        for(const auto* p=reinterpret_cast<const unsigned char*>(text);*p;++p){
            if(*p=='"'||*p=='\\'){put('\\');put(char(*p));}
            else if(*p<32){
                static constexpr char hex[]="0123456789abcdef";
                literal("\\u00");put(hex[*p>>4]);put(hex[*p&15]);
            }else put(char(*p));
        }
        put('"');
    };
    literal("{\"v\":1,\"values\":{");
    char value[8192]{};
    for(std::size_t i=0;i<count;++i){
        const int length=getParamValue(opaque,keys[i],value,sizeof(value));
        if(length<0||length>=int(sizeof(value))){ok=false;break;}
        value[length]=0;
        if(i)put(',');
        quoted(keys[i]);put(':');quoted(value);
    }
    literal("}}");
    if(!ok){buffer[0]=0;return -1;}
    buffer[at]=0;return at;
}
