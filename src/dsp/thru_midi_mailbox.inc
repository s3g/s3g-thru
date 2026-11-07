    // Schwung's canonical shadow mailbox is what Move will read AFTER this
    // render callback. Never write the hardware mmap. Each input event is a
    // four-byte USB-MIDI packet plus a four-byte XMOS event timestamp.
    static constexpr std::size_t kMidiInboxOffset=2048,kMidiInboxBytes=31u*8u;
    bool midiInboxActive=false;
    std::array<std::array<std::uint8_t,8>,256> midiInboxSeen {};
    std::size_t midiInboxSeenCount=0,midiInboxSeenWrite=0;
    std::array<std::array<std::uint8_t,8>,31> midiInboxPrevious {};
    std::size_t midiInboxPreviousCount=0;
    bool midiInboxAvailable() const {
        return !patternRenderOnly&&g_host&&g_host->mapped_memory
            &&g_host->audio_in_offset==2304&&g_host->audio_out_offset==256
            &&g_host->frames_per_block==128&&g_host->sample_rate==44100;
    }
    static bool midiInboxMessage(int status,int number) {
        const int type=status&0xf0;
        return type==0x80||type==0x90||type==0xa0||type==0xd0||type==0xe0
            ||(type==0xb0&&(status&15)<4&&(number==1||number==64||number==120||number==121||number==123));
    }
    bool midiInboxAlreadySeen(const std::array<std::uint8_t,8>& event) const {
        for(std::size_t i=0;i<midiInboxSeenCount;++i)if(midiInboxSeen[i]==event)return true;
        return false;
    }
    void rememberMidiInboxEvent(const std::array<std::uint8_t,8>& event) {
        midiInboxSeen[midiInboxSeenWrite]=event;
        midiInboxSeenWrite=(midiInboxSeenWrite+1)%midiInboxSeen.size();
        midiInboxSeenCount=std::min(midiInboxSeenCount+1,midiInboxSeen.size());
    }
    static bool midiInboxSameLane(const std::array<std::uint8_t,8>& a,const std::array<std::uint8_t,8>& b) {
        const int at=(a[1]&0xe0)==0x80?0x80:a[1]&0xf0;
        const int bt=(b[1]&0xe0)==0x80?0x80:b[1]&0xf0;
        if(at!=bt||(a[1]&15)!=(b[1]&15))return false;
        return at==0x80||at==0xa0||at==0xb0?a[2]==b[2]:true;
    }
    void captureMidiInbox() {
        if(!midiInboxAvailable())return;
        auto* input=g_host->mapped_memory+kMidiInboxOffset;
        std::array<std::array<std::uint8_t,8>,31> currentImage {};
        std::size_t currentCount=0;
        for(std::size_t slot=0;slot<kMidiInboxBytes;slot+=8) {
            auto* packet=input+slot;
            const int status=packet[1],type=status&0xf0,cin=packet[0]&15;
            if((packet[0]>>4)!=2||type<0x80||type>0xe0||cin!=(type>>4))continue;
            const int length=type==0xc0||type==0xd0?2:3;
            if(packet[2]>=128||(length==3&&packet[3]>=128))continue;
            if(!midiInboxMessage(status,packet[2]))continue;
            std::array<std::uint8_t,8> event{};
            std::copy_n(packet,8,event.begin());
            const bool timestamped=(packet[4]|packet[5]|packet[6]|packet[7])!=0;
            bool seen=false;
            if(timestamped) {
                seen=midiInboxAlreadySeen(event);
                if(!seen)rememberMidiInboxEvent(event);
            } else {
                // Zero timestamps are valid. Only suppress an unchanged
                // last message on this lane in consecutive raw inbox images.
                // On/off/on in one image must remain three ordered events.
                bool located=false;
                const auto previousOnLane=[&](const auto& image,std::size_t count) {
                    for(std::size_t i=count;i>0;--i)if(midiInboxSameLane(image[i-1],event)) {
                        seen=image[i-1]==event;located=true;return;
                    }
                };
                previousOnLane(currentImage,currentCount);
                if(!located)previousOnLane(midiInboxPrevious,midiInboxPreviousCount);
            }
            currentImage[currentCount++]=event;
            midiInboxActive=true;
            if(type==0x90&&packet[3]) {
                // Preserve geometry, channel, note, timestamp and every
                // following event. A zeroed slot would truncate Move input.
                packet[0]=static_cast<std::uint8_t>((packet[0]&0xf0)|8);
                packet[1]=static_cast<std::uint8_t>(0x80|(status&15));packet[3]=64;
                // Only ORIGINAL events enter dedup. A synthetic native
                // release can equal a real release in the same time batch.
                // Schwung refreshes the shadow input before every render.
            }
            if(!seen)dispatchMidi(this,event.data()+1,length,MOVE_MIDI_SOURCE_EXTERNAL,true);
        }
        midiInboxPrevious=currentImage;midiInboxPreviousCount=currentCount;
    }
