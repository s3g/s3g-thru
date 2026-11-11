    // Musical input is independent of the parameter-controller protocol.
    int midiNoteRoute=0,midiNoteChannel=1,midiModTarget=0;
    int midiSyncSource=0,midiSyncTransport=0;
    float midiBendRange=2,midiVelocityDepth=0;
    struct MidiKey {bool down=false,pedal=false;};
    struct MidiChannel {float bend=0,wheel=0,pressure=0;bool sustain=false,pressureSeen=false;};
    std::array<std::array<MidiChannel,4>,2> midiChannels {};
    std::array<std::array<std::array<std::array<MidiKey,128>,4>,4>,2> midiKeys {};
    std::array<std::array<std::array<std::uint8_t,128>,4>,2> midiKeyOwner {};
    std::array<std::array<float,3>,4> midiPartExpression {};
    std::array<std::array<float,4>,4> midiRecordInitial {};
    std::array<std::uint64_t,24> midiClockHistory {};
    std::uint64_t midiClockLast=0,midiClockCount=0;
    int midiClockInputSource=-1;
    bool midiClockSeen=false,midiClockLocked=false,midiClockStopped=false;
    bool midiClockTimedOut=false;
    double midiClockAnchor=0,midiResumePosition=0,midiResumeAbsolute=0;
    bool midiClipResume=false,midiReleasingExternal=false;
    int midiResumeClip=-1;

    static int midiSourceIndex(int source) {return source==MOVE_MIDI_SOURCE_EXTERNAL?0:source==MOVE_MIDI_SOURCE_HOST?1:-1;}
    int midiNotePart(int channel) const {
        if(channel<0||channel>=4)return -1;
        if(midiNoteRoute)return channel==midiNoteChannel-1?(splitEnabled?splitFocus:0):-1;
        return channel<(splitEnabled?partCount():1)?channel:-1;
    }
    void clearMidiKeyState(int part=-1) {
        for(auto& input:midiKeys)for(auto& channel:input)for(int p=0;p<4;++p)
            if(part<0||p==part)channel[p]={};
        for(auto& input:midiKeyOwner)for(auto& channel:input)for(auto& owner:channel)
            if(part<0||owner==part+1)owner=0;
        if(part<0){midiChannels={};midiPartExpression={};}
        else midiPartExpression[part]={};
    }
    bool midiKeyActive(int part,int note) const {
        for(const auto& input:midiKeys)for(const auto& channel:input) {
            const auto& key=channel[part][note];if(key.down||key.pedal)return true;
        }
        return false;
    }
    void midiReleaseKey(int part,int note,bool hard=false) {
        if(midiKeyActive(part,note))return;
        const int previous=midiPart;midiPart=part;
        midiReleasingExternal=true;noteOff(note);midiReleasingExternal=false;midiPart=previous;
        if(hard)for(auto& voice:voices)if(voice.active&&!voice.clipOwned&&voice.sourcePad<0&&voice.part==part&&voice.note==note) {
            voice.panicRelease=true;voice.envelopeStageStart=voice.envelope;
            voice.envelopeStageFrames=0;voice.envelopeDurationSet=false;
            voice.envelopeStage=AmpEnvelopeStage::release;
        }
    }
    void releaseMidiChannel(int source,int channel,bool hard=false) {
        midiChannels[source][channel].sustain=false;
        midiKeyOwner[source][channel]={};
        for(int part=0;part<4;++part)for(int note=0;note<128;++note) {
            auto& key=midiKeys[source][channel][part][note];
            if(!key.down&&!key.pedal)continue;
            key={};midiReleaseKey(part,note,hard);
        }
        // Released tails and one-shots retain their origin for All Sound Off.
        if(hard)for(auto& voice:voices)if(voice.active&&!voice.clipOwned&&voice.midiExternal
            &&voice.midiInput==source&&voice.midiChannel==channel&&!midiKeyActive(voice.part,voice.note)) {
            voice.held=false;voice.panicRelease=true;voice.envelopeStage=AmpEnvelopeStage::release;
            voice.envelopeStageStart=voice.envelope;voice.envelopeStageFrames=0;voice.envelopeDurationSet=false;
        }
    }
    void releaseMidiInput(int input) {
        for(int channel=0;channel<4;++channel)releaseMidiChannel(input,channel);
    }
    void releaseExternalNotes(bool hard=false) {
        for(int source=0;source<2;++source)for(int channel=0;channel<4;++channel)releaseMidiChannel(source,channel,hard);
    }
    void captureMidiExpression(int part,int parameter,float value) {
        if(!clipRecording)return;
        const auto lane=kClipMidiExpressionOffset+static_cast<std::size_t>(part*4+parameter);
        if(clipLastPointIndices[lane]<0) {
            const float initial=midiRecordInitial[part][parameter];
            if(value==0&&initial==0)return;
            captureClipPoint(ClipPointType::midiExpression,parameter,part,initial,0);
        }
        const auto* clip=activeClipSlot();const int previous=clipLastPointIndices[lane];
        if(clip&&previous>=0) {
            const auto& point=clip->clipPoints[previous];
            if(point.value==value)return;
            const auto time=clipCaptureTimeMs();
            if(time>point.timeMs+40u)captureClipPoint(ClipPointType::midiExpression,parameter,part,point.value,time-1);
        }
        captureClipPoint(ClipPointType::midiExpression,parameter,part,value);
    }
    void seedMidiRecording() {
        for(int part=0;part<4;++part) {
            for(int field=0;field<3;++field)midiRecordInitial[part][field]=midiPartExpression[part][field];
            midiRecordInitial[part][3]=midiVelocityDepth;
            const auto* clip=activeClipSlot();
            for(int field=0;field<4;++field) {
                const auto lane=kClipMidiExpressionOffset+part*4+field;
                if(midiRecordInitial[part][field]!=0||(!clipReplacing&&clip&&clip->clipPointCounts[lane]))
                    captureClipPoint(ClipPointType::midiExpression,field,part,midiRecordInitial[part][field],clipReplacing?0:clipCaptureTimeMs());
            }
        }
    }
    void updateMidiPart(int source,int channel,int part) {
        auto& expression=midiPartExpression[part];const auto& input=midiChannels[source][channel];
        expression={input.bend*midiBendRange,midiModTarget==1?input.wheel:0.f,midiModTarget==2?input.wheel:0.f};
        for(int field=0;field<3;++field)captureMidiExpression(part,field,expression[field]);
        for(auto& voice:voices)if(voice.active&&!voice.clipOwned&&voice.midiExternal&&voice.part==part)
            voice.midiTarget=expression;
    }
    void updateMidiChannel(int source,int channel,bool pressure=false) {
        std::array<bool,4> affected{};const int target=midiNotePart(channel);
        if(target>=0)affected[target]=true;
        for(int part=0;part<4;++part)for(const auto& key:midiKeys[source][channel][part])
            if(key.down||key.pedal){affected[part]=true;break;}
        for(int part=0;part<partCount();++part)if(affected[part]) {
            updateMidiPart(source,channel,part);
            if(pressure)for(int note=0;note<128;++note) {
                const auto& key=midiKeys[source][channel][part][note];if(!key.down&&!key.pedal)continue;
                const int previous=midiPart;midiPart=part;
                aftertouch(note,static_cast<int>(std::lround(midiChannels[source][channel].pressure*127)));
                midiPart=previous;
            }
        }
    }
    void initializeMidiVoice(PlaybackVoice& voice,const ClipSlot* clip=nullptr,double position=0) {
        voice.midiExternal=voice.sourcePad<0;
        voice.midiTarget={};voice.midiValue={};voice.midiGain=1;
        voice.midiInput=voice.midiChannel=-1;
        if(!voice.midiExternal)return;
        float depth=0;
        if(clip) {
            for(int field=0;field<3;++field)
                sampleClipCurve(*clip,kClipMidiExpressionOffset+voice.part*4+field,position,true,voice.midiTarget[field]);
            sampleClipCurve(*clip,kClipMidiExpressionOffset+voice.part*4+3,position,false,depth);
        } else {
            voice.midiTarget=midiPartExpression[voice.part];depth=midiVelocityDepth;
            for(int input=0;input<2;++input)for(int channel=0;channel<4;++channel) {
                const auto& key=midiKeys[input][channel][voice.part][voice.note];
                if(key.down||key.pedal){voice.midiInput=input;voice.midiChannel=channel;}
            }
        }
        voice.midiGain=1-clampValue(depth,0.f,1.f)*(1-midiUnit(voice.velocity));
        voice.midiValue=voice.midiTarget;
        if(!voice.stealPending&&voice.envelopeStageFrames==0)
            voice.gainState=(voice.clipOwned?clipOutputGain:outputGain)*voice.midiGain;
    }
    bool handleNoteMidi(const std::uint8_t* message,int length,int source) {
        const int input=midiSourceIndex(source);if(input<0)return false;
        const int channel=message[0]&15,type=message[0]&0xf0;
        const bool notes=type==0x80||type==0x90,expression=type==0xa0||type==0xd0||type==0xe0;
        const int number=length>1?message[1]:0,value=length>2?message[2]:0;
        const bool control=type==0xb0&&(number==1||number==64||number==120||number==121||number==123);
        if(!notes&&!expression&&!control)return false;
        if(channel>=4)return notes||expression||(control&&number>=120); // No fallback panic on control channels.
        auto& state=midiChannels[input][channel];
        if(notes) {
            auto& owner=midiKeyOwner[input][channel][number];
            if(type==0x90&&value) {
                const int part=owner?owner-1:midiNotePart(channel);if(part<0||part>=partCount())return true;
                if(bankMode&&part==1&&(number<60||number>75||bankAssets[number-60]<0))return true;
                auto& key=midiKeys[input][channel][part][number];
                // One live gate per input/channel/key; a retrigger replaces it.
                key.down=true;key.pedal=false;owner=part+1;
                updateMidiPart(input,channel,part);
                const int previous=midiPart;midiPart=part;
                captureClipNoteOff(number,-1,part);
                noteOn(number,value);
                if(auto* voice=findHeldVoiceForNote(number,-1)) {
                    // A legato pitch change keeps the existing expression slew.
                    const auto values=voice->midiValue;const bool legato=voice->midiExternal&&settingsForPart(part).voiceMode==2;
                    initializeMidiVoice(*voice);
                    voice->midiInput=input;voice->midiChannel=channel;
                    if(legato)voice->midiValue=values;
                    if(state.pressureSeen)aftertouch(number,static_cast<int>(std::lround(state.pressure*127)));
                }
                captureMidiExpression(part,3,midiVelocityDepth);
                midiPart=previous;
            } else if(owner) {
                const int part=owner-1;auto& key=midiKeys[input][channel][part][number];
                key.down=false;owner=0;key.pedal=state.sustain;midiReleaseKey(part,number);
            }
            return true;
        }
        if(type==0xe0) {
            const int raw=number|(value<<7);state.bend=raw>=8192?(raw-8192)/8191.f:(raw-8192)/8192.f;
            updateMidiChannel(input,channel);return true;
        }
        if(type==0xd0){state.pressure=midiUnit(number);state.pressureSeen=true;updateMidiChannel(input,channel,true);return true;}
        if(type==0xa0) {
            for(int part=0;part<partCount();++part) {
                const auto& key=midiKeys[input][channel][part][number];if(!key.down&&!key.pedal)continue;
                const int previous=midiPart;midiPart=part;aftertouch(number,value);midiPart=previous;
            }
            return true;
        }
        if(number==1){state.wheel=midiUnit(value);updateMidiChannel(input,channel);}
        else if(number==64) {
            const bool down=value>=64;state.sustain=down;
            if(!down)for(int part=0;part<4;++part)for(int note=0;note<128;++note) {
                auto& key=midiKeys[input][channel][part][note];
                if(!key.pedal)continue;
                key.pedal=false;midiReleaseKey(part,note);
            }
        } else if(number==121) {
            state={};
            for(int part=0;part<4;++part)for(int note=0;note<128;++note) {
                auto& key=midiKeys[input][channel][part][note];if(key.pedal){key.pedal=false;midiReleaseKey(part,note);}
            }
            updateMidiChannel(input,channel,true);
        } else releaseMidiChannel(input,channel,number==120);
        return true;
    }
    void midiPauseClip() {
        midiClipResume=clipPlaying;midiResumeClip=activeClip;midiResumePosition=clipPositionMs;midiResumeAbsolute=clipAbsolutePositionMs;
        clipPlaying=false;clipLaunchPending=false;clipNeedsStart=false;releaseClipVoices();
        chainLaunchBank=chainLaunchSlot=-1;queuedClip=-1;chainNextSlot=-1;chainAdvanceRequested=chainLaunchingFirst=false;
    }
    void midiContinueClip() {
        if(!midiClipResume||midiResumeClip!=activeClip){startClipPlaybackNow();return;}
        const auto* clip=activeClipSlot();if(!clip||!clip->clipRecorded)return;
        clipPositionMs=midiResumePosition;clipAbsolutePositionMs=midiResumeAbsolute;
        clipPlaying=true;clipNeedsStart=false;applyClipCurves(*clip,clipPositionMs);
        const double duration=std::max(1u,clip->clipDurationMs);
        for(std::size_t i=0;i<clip->clipNoteCount;++i) {
            const auto& note=clip->clipNotes[i];
            const double elapsed=std::fmod(clipPositionMs+duration-note.startMs,duration);
            if(elapsed<note.durationMs)triggerClipVoice(*clip,i);
        }
        applyClipCurves(*clip,clipPositionMs);
        if(chainEnabled)prepareNextChainClip();
    }
    void resetMidiClock() {
        midiClockInputSource=-1;
        midiClockHistory={};midiClockLast=midiClockCount=0;
        midiClockSeen=midiClockLocked=midiClockStopped=midiClockTimedOut=false;
        midiClockAnchor=transportBeats;midiClipResume=false;
    }
    void handleMidiRealtime(std::uint8_t status,int source) {
        const int input=midiSourceIndex(source);
        if(input<0||(status!=0xf8&&status!=0xfa&&status!=0xfb&&status!=0xfc))return;
        // Stop closes this input's notes even when tempo/clip sync is off.
        if(status==0xfc){postPatternCommand(8,input);releaseMidiInput(input);}
        if(!midiSyncSource)return;
        if(midiClockInputSource<0) {
            midiClockInputSource=input;midiClockCount=0;midiClockHistory={};midiClockSeen=false;
            if(midiClockTimedOut){midiClockStopped=false;midiClockTimedOut=false;}
        }
        if(midiClockInputSource!=input)return;
        const auto now=renderClockSamples.load(std::memory_order_relaxed);
        if(status==0xfa||status==0xfb) {
            midiClockStopped=false;midiClockTimedOut=false;midiClockSeen=false;midiClockLocked=false;midiClockCount=0;midiClockHistory={};
            if(status==0xfa)transportBeats=0;
            midiClockAnchor=transportBeats;
            if(midiSyncTransport&&!clipRecording) {
                if(status==0xfa){releaseClipVoices();clipPositionMs=clipAbsolutePositionMs=0;startClipPlaybackNow();}
                else midiContinueClip();
            }
        } else if(status==0xfc) {
            midiClockStopped=true;postPatternCommand(6,midiSyncTransport);releaseExternalNotes();
            if(midiSyncTransport){if(clipRecording)finishClipRecording();midiPauseClip();}
        } else if(status==0xf8) {
            if(!midiClockSeen)midiClockAnchor=transportBeats;
            const auto intervals=std::min<std::uint64_t>(midiClockCount,24);
            if(intervals>=6) {
                const auto then=midiClockHistory[(midiClockCount-intervals)%24];
                const auto elapsed=now>then?now-then:0;
                if(elapsed) {
                    const float estimate=60.f*sampleRate*intervals/(24.f*elapsed);
                    if(estimate>=.95f&&estimate<=1050.f) {
                        const float target=clampValue(estimate,1.f,999.f);
                        tempoBpm=midiClockLocked?tempoBpm+(target-tempoBpm)*.2f:target;
                        midiClockLocked=true;postPatternCommand(7,static_cast<int>(std::lround(tempoBpm*1000)));
                    }
                }
            }
            midiClockHistory[midiClockCount%24]=now;
            if(!midiClockStopped) {
                const double phase=midiClockAnchor+static_cast<double>(midiClockCount)/24;
                transportBeats+=clampValue((phase-transportBeats)*.15,-.025,.025);
            }
            ++midiClockCount;midiClockLast=now;midiClockSeen=true;
        }
    }
    void serviceMidiClock() {
        if(!midiSyncSource||!midiClockSeen||midiClockStopped)return;
        const auto now=renderClockSamples.load(std::memory_order_relaxed);
        const double tickSeconds=60.0/(24.0*std::max(1.0f,tempoBpm));
        const double timeoutSeconds=midiClockLocked
            ? std::max(.75,1.5*tickSeconds) : 3.75;
        if(now-midiClockLast>static_cast<std::uint64_t>(sampleRate*timeoutSeconds)) {
            midiClockLocked=false;midiClockStopped=true;midiClockTimedOut=true;midiClockInputSource=-1;
            postPatternCommand(6,midiSyncTransport);releaseExternalNotes();
            if(midiSyncTransport){if(clipRecording)finishClipRecording();midiPauseClip();}
        }
    }
    bool setMidiNoteParam(const char* key,const char* text) {
        int* integer=nullptr;int maximum=0,minimum=0;const char* const* choices=nullptr;
        static constexpr const char* route[]{"Parts","Focus"};
        static constexpr const char* mod[]{"Off","Stack","Character"};
        static constexpr const char* sync[]{"Internal","Move"};
        static constexpr const char* transport[]{"Tempo Only","Follow Clips"};
        if(!std::strcmp(key,"midi_note_route")){integer=&midiNoteRoute;maximum=1;choices=route;}
        else if(!std::strcmp(key,"midi_note_channel")){integer=&midiNoteChannel;minimum=1;maximum=4;}
        else if(!std::strcmp(key,"midi_mod_target")){integer=&midiModTarget;maximum=2;choices=mod;}
        else if(!std::strcmp(key,"midi_sync_source")){integer=&midiSyncSource;maximum=1;choices=sync;}
        else if(!std::strcmp(key,"midi_sync_transport")){integer=&midiSyncTransport;maximum=1;choices=transport;}
        if(integer) {
            int value=parseInteger(text,*integer);
            if(choices)for(int i=0;i<=maximum;++i)if(!std::strcmp(text,choices[i]))value=i;
            value=clampValue(value,minimum,maximum);
            if(*integer==value)return true;
            if(integer==&midiNoteRoute||integer==&midiNoteChannel){postPatternCommand(6);releaseExternalNotes();}
            if(integer==&midiSyncSource||integer==&midiSyncTransport)resetMidiClock();
            *integer=value;
        } else if(!std::strcmp(key,"midi_bend_range"))midiBendRange=clampValue(parseFloat(text,midiBendRange),0.f,24.f);
        else if(!std::strcmp(key,"midi_velocity_depth"))midiVelocityDepth=clampValue(parseFloat(text,midiVelocityDepth*100)*.01f,0.f,1.f);
        else return false;
        for(int input=0;input<2;++input)for(int channel=0;channel<4;++channel) {
            bool used=false;for(const auto& part:midiKeys[input][channel])for(const auto& note:part)used|=note.down||note.pedal;
            if(used)updateMidiChannel(input,channel);
        }
        markPersistentStateDirty();return true;
    }
    int getMidiNoteParam(const char* key,char* buffer,int size) const {
        float value=0;
        if(!std::strcmp(key,"midi_note_route"))value=midiNoteRoute;
        else if(!std::strcmp(key,"midi_note_channel"))value=midiNoteChannel;
        else if(!std::strcmp(key,"midi_mod_target"))value=midiModTarget;
        else if(!std::strcmp(key,"midi_sync_source"))value=midiSyncSource;
        else if(!std::strcmp(key,"midi_sync_transport"))value=midiSyncTransport;
        else if(!std::strcmp(key,"midi_bend_range"))value=midiBendRange;
        else if(!std::strcmp(key,"midi_velocity_depth"))value=midiVelocityDepth*100;
        else if(!std::strcmp(key,"midi_clock_state"))return std::snprintf(buffer,size,"%d,%d,%.2f",midiClockLocked?1:0,midiClockStopped?1:0,tempoBpm);
        else return -2;
        return std::snprintf(buffer,size,"%.6g",static_cast<double>(value));
    }
