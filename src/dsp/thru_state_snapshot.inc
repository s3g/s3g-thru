// An owned, numeric snapshot for metadata formatting on the existing worker.
// No live ThruInstance data is read after the job is published. The JSON shape
// and scalar formatting are shared with explicit state/project serialization.
    struct PersistentStateSnapshot {
        decltype(ThruInstance::controls) controls {};
        decltype(ThruInstance::outputGain) outputGain {};
        decltype(ThruInstance::polyphonyCompensation) polyphonyCompensation {};
        decltype(ThruInstance::recordGain) recordGain {};
        decltype(ThruInstance::pressureMode) pressureMode {};
        decltype(ThruInstance::holdReleaseMs) holdReleaseMs {};
        decltype(ThruInstance::rootNote) rootNote {};
        decltype(ThruInstance::padOctave) padOctave {};
        decltype(ThruInstance::padScale) padScale {};
        decltype(ThruInstance::selectedLane) selectedLane {};
        decltype(ThruInstance::recordSource) recordSource {};
        decltype(ThruInstance::tempoBpm) tempoBpm {};
        decltype(ThruInstance::tempoSwing) tempoSwing {};
        decltype(ThruInstance::chainEnabled) chainEnabled {};
        decltype(ThruInstance::padMode) padMode {};
        decltype(ThruInstance::laneVariation) laneVariation {};
        decltype(ThruInstance::clipRecordMode) clipRecordMode {};
        decltype(ThruInstance::previewLevel) previewLevel {};
        decltype(ThruInstance::previewRoute) previewRoute {};
        decltype(ThruInstance::grainVoiceLimit) grainVoiceLimit {};
        decltype(ThruInstance::launchQuantize) launchQuantize {};
        decltype(ThruInstance::midiControlMode) midiControlMode {};
        decltype(ThruInstance::midiCcChannel) midiCcChannel {};
        decltype(ThruInstance::midiCcBase) midiCcBase {};
        decltype(ThruInstance::midiTakeover) midiTakeover {};
        decltype(ThruInstance::midiControlPart) midiControlPart {};
        decltype(ThruInstance::midiNoteRoute) midiNoteRoute {};
        decltype(ThruInstance::midiNoteChannel) midiNoteChannel {};
        decltype(ThruInstance::midiBendRange) midiBendRange {};
        decltype(ThruInstance::midiModTarget) midiModTarget {};
        decltype(ThruInstance::midiVelocityDepth) midiVelocityDepth {};
        decltype(ThruInstance::midiSyncSource) midiSyncSource {};
        decltype(ThruInstance::midiSyncTransport) midiSyncTransport {};
        decltype(ThruInstance::nextCaptureId) nextCaptureId {};
        decltype(ThruInstance::persistenceGeneration) persistenceGeneration {};
        decltype(ThruInstance::splitEnabled) splitEnabled {};
        decltype(ThruInstance::splitFocus) splitFocus {};
        decltype(ThruInstance::sharedDual) sharedDual {};
        decltype(ThruInstance::splitInitialized) splitInitialized {};
        decltype(ThruInstance::quadMode) quadMode {};
        decltype(ThruInstance::quadInitialized) quadInitialized {};
        decltype(ThruInstance::bankMode) bankMode {};
        decltype(ThruInstance::bankMasterRate) bankMasterRate {};
        decltype(ThruInstance::bankRecordSource) bankRecordSource {};
        decltype(ThruInstance::splitSingleControls) splitSingleControls {};
        decltype(ThruInstance::splitParts) splitParts {};
        decltype(ThruInstance::splitLanePreferences) splitLanePreferences {};
        decltype(ThruInstance::splitSingleSettings) splitSingleSettings {};
        decltype(ThruInstance::layersLive) layersLive {};
        float patternFadeMs=0,patternFadeLowMs=0,patternFadeHighMs=0;
        int patternCurve=0,patternTimingMode=0,patternLookahead=0,returnMode=0;
        bool sequenceActive=false;
        std::array<std::uint32_t,4> pulsarSeeds {};
        PartSettings currentSettings {};
        void capture(const ThruInstance& source) {
            controls=source.controls;
            const auto* clip=source.activeClipSlot();
            for(int part=0;part<4;++part)pulsarSeeds[part]=clip&&clip->pulsarSeeds[part]
                ?clip->pulsarSeeds[part]:ThruInstance::defaultPulsarSeed(source.activeClip,part);
            outputGain=source.outputGain;
            polyphonyCompensation=source.polyphonyCompensation;
            recordGain=source.recordGain;
            pressureMode=source.pressureMode;
            holdReleaseMs=source.holdReleaseMs;
            rootNote=source.rootNote;
            padOctave=source.padOctave;
            padScale=source.padScale;
            selectedLane=source.selectedLane;
            recordSource=source.recordSource;
            tempoBpm=source.tempoBpm;
            tempoSwing=source.tempoSwing;
            chainEnabled=source.chainEnabled;
            padMode=source.padMode;
            laneVariation=source.laneVariation;
            clipRecordMode=source.clipRecordMode;
            previewLevel=source.previewLevel;
            previewRoute=source.previewRoute;
            grainVoiceLimit=source.grainVoiceLimit;
            launchQuantize=source.launchQuantize;
            midiControlMode=source.midiControlMode;
            midiCcChannel=source.midiCcChannel;
            midiCcBase=source.midiCcBase;
            midiTakeover=source.midiTakeover;
            midiControlPart=source.midiControlPart;
            midiNoteRoute=source.midiNoteRoute;
            midiNoteChannel=source.midiNoteChannel;
            midiBendRange=source.midiBendRange;
            midiModTarget=source.midiModTarget;
            midiVelocityDepth=source.midiVelocityDepth;
            midiSyncSource=source.midiSyncSource;
            midiSyncTransport=source.midiSyncTransport;
            nextCaptureId=source.nextCaptureId;
            persistenceGeneration=source.persistenceGeneration;
            splitEnabled=source.splitEnabled;
            splitFocus=source.splitFocus;
            sharedDual=source.sharedDual;
            splitInitialized=source.splitInitialized;
            quadMode=source.quadMode;
            quadInitialized=source.quadInitialized;
            bankMode=source.bankMode;
            bankMasterRate=source.bankMasterRate;
            bankRecordSource=source.bankRecordSource;
            splitSingleControls=source.splitSingleControls;
            splitParts=source.splitParts;
            splitLanePreferences=source.preferredSplitLanes();
            splitSingleSettings=source.splitSingleSettings;
            layersLive=source.layersLive;
            patternFadeMs=source.patternFadeMs;
            patternFadeLowMs=source.patternFadeLowMs;
            patternFadeHighMs=source.patternFadeHighMs;
            patternCurve=source.patternCurve;
            patternTimingMode=source.patternTimingMode;
            patternLookahead=source.patternLookahead;
            returnMode=source.copyReturnMode();
            sequenceActive=source.sequenceMode();
            currentSettings=source.currentPartSettings();
        }
        int copyReturnMode() const {return returnMode;}
        bool sequenceMode() const {return sequenceActive;}
        PartSettings currentPartSettings() const {return currentSettings;}
        PartSettings settingsForPart(int part) const {
            return !splitEnabled||part==splitFocus?currentSettings:splitParts[std::size_t(part)].settings;
        }
        const Engine::Controls& liveControlsForPart(int part) const {
            return !splitEnabled||part==splitFocus?controls:splitParts[std::size_t(part)].controls;
        }
        int appendSplitJson(char* buffer,int capacity,int length) const
        {
            if(length<=0||length>=capacity)return length;
            int at=length-1;
            const auto append=[&](const char* format,auto...values) {
                if(at<0||at>=capacity)return;
                const int n=std::snprintf(buffer+at,static_cast<std::size_t>(capacity-at),format,values...);
                if(n<0||n>=capacity-at)at=capacity;else at+=n;
            };
            for(int part=0;part<4;++part)append(",\"pulsar_seed_%d\":%u",part,unsigned(pulsarSeeds[part]));
            append(",\"split_return_mode\":%d",copyReturnMode());
            append(",\"split_lane_preferences\":[%.9g,%.9g,%.9g,%.9g]",splitLanePreferences[0],splitLanePreferences[1],splitLanePreferences[2],splitLanePreferences[3]);
            append(",\"bank_mode\":%d,\"bank_master_rate\":%.9g,\"bank_record_source\":%d,\"bank_source_version\":2",bankMode?1:0,bankMasterRate,bankRecordSource);
            append(",\"split_enabled\":%d,\"split_shared\":%d,\"split_initialized\":%d,\"split_focus\":%d",splitEnabled?1:0,sharedDual?1:0,splitInitialized?1:0,splitFocus);
            append(",\"split_params_count\":%zu,\"split_parts\":4,\"split_quad\":%d,\"quad_initialized\":%d",kLockParameterCount,quadMode?1:0,quadInitialized?1:0);
            const char* names[]{"split_single","split_a","split_b","split_c","split_d"};
            for(int group=0;group<5;++group) {
                append(",\"%s\":[",names[group]);
                const auto& c=group==0?(splitEnabled?splitSingleControls:controls)
                    :(splitEnabled?liveControlsForPart(group-1):splitParts[static_cast<std::size_t>(group-1)].controls);
                for(std::size_t p=0;p<kLockParameterCount;++p)
                    append(p?",%.9g":"%.9g",static_cast<double>(lockedValue(group>0&&splitEnabled&&!splitPartParameter(p)?controls:c,outputGain,p)));
                const auto ui=group==0?(splitEnabled?splitSingleSettings:currentPartSettings())
                    :(splitEnabled?settingsForPart(group-1):splitParts[static_cast<std::size_t>(group-1)].settings);
                for(int value:settingsValues(ui))append(",%d",value);
                append("%s","]");
                append(",\"%s_voice_mode\":%d",names[group],ui.voiceMode);
                appendVoiceEngineJson(append,group,ui.engines);
                appendVoicePlayJson(append,group,ui.engines);
            }
            const char* pathNames[]{"stack_path_single","stack_path_a","stack_path_b","stack_path_c","stack_path_d"};
            for(int group=0;group<5;++group) {
                const auto& path=group==0?(splitEnabled?splitSingleControls.stackPath:controls.stackPath)
                    :(splitEnabled?liveControlsForPart(group-1).stackPath:splitParts[group-1].controls.stackPath);
                append(",\"%s\":[",pathNames[group]);
                for(std::size_t p=0;p<StackPath::valueCount;++p)append(p?",%.9g":"%.9g",static_cast<double>(path.value(p)));
                append("%s","]");
            }
            append("%s","}");return at;
        }
        int appendPatternFadeJson(char* buffer,int capacity,int length) const {
            if(length<1 || length>=capacity || buffer[length-1]!='}')return -1;
            const int n=std::snprintf(buffer+length-1,capacity-length+1,
                ",\"pattern_fade_ms\":%.0f,\"pattern_fade_curve\":%d,\"pattern_fade_mode\":%d,\"pattern_fade_low_ms\":%.0f,\"pattern_fade_high_ms\":%.0f,\"pattern_fade_lookahead\":%d}",
                patternFadeMs,patternCurve,patternTimingMode,patternFadeLowMs,patternFadeHighMs,patternLookahead);
            return n<0 || n>=capacity-length+1 ? -1 : length-1+n;
        }
        int appendLayersJson(char *b, int capacity, int length) const {
          if (length <= 0 || length >= capacity)
            return -1;
          int at = length - 1;
          const auto append = [&](const char *fmt, auto... args) {
            const int n = std::snprintf(b + at, capacity - at, fmt, args...);
            if (n < 0 || n >= capacity - at)
              return false;
            at += n;
            return true;
          };
          if (!append(
                  ",\"layers_mode\":%d,\"layers_initialized\":%d,\"layers_values\":[",
                  layersLive.active ? 1 : 0, layersLive.initialized ? 1 : 0))
            return -1;
          for (std::size_t i = 0; i < LayerPatch::size; ++i)
            if (!append("%s%.9g", i ? "," : "", layersLive.values[i]))
              return -1;
          return append("%s", "]}") ? at : -1;
        }
        int serializeState(char* buffer, int bufferLength,
            std::uint64_t generation = kAnyPersistenceGeneration) const
        {
            if (!buffer || bufferLength < 1) return -1;
            const int length = std::snprintf(buffer, static_cast<std::size_t>(bufferLength),
                "{\"version\":19,\"locus\":%.5f,\"field\":%.5f,\"speed\":%.5f,"
                "\"manual_lane\":%.5f,\"lane_cycles\":%.5f,\"algorithm\":%d,"
                "\"join\":%.5f,\"output_gain\":%.5f,\"polyphony_comp\":%d,"
                "\"record_gain\":%.5f,"
                "\"amp_attack\":%.5f,\"amp_decay\":%.5f,"
                "\"amp_sustain\":%.5f,\"amp_release\":%.5f,"
                "\"pressure_mode\":%d,\"hold_release_ms\":%d,"
                "\"root_note\":%d,\"pad_octave\":%d,\"scale\":%d,"
                "\"record_lane\":%u,\"record_source\":%d,\"motion_mode\":%d,"
                "\"motion_sound\":%d,\"motion_rate\":%.5f,"
                "\"motion_rate_basis\":%d,\"motion_path_rate\":%.5f,"
                "\"motion_step\":%.5f,\"motion_seed\":%.0f,\"lanes_mode\":%d,"
                "\"lane_path\":%d,\"lanes_stretch\":%d,\"stretch_ratio\":%.5f,"
                "\"lane_1_gain\":%.5f,\"lane_2_gain\":%.5f,"
                "\"lane_3_gain\":%.5f,\"lane_4_gain\":%.5f,"
                "\"grain_mode\":%d,\"grain_size\":%.5f,"
                "\"grain_density\":%.5f,\"grain_pitch\":%.5f,"
                "\"grain_window\":%d,\"grain_spread\":%.5f,"
                "\"cutups_mode\":%d,\"tempo_bpm\":%.5f,\"tempo_swing\":%.5f,"
                "\"chain_enabled\":%d,"
                "\"motion_travel\":%.5f,\"motion_jitter\":%.5f,"
                "\"packet_duty\":%.5f,\"motor_rate\":%.5f,"
                "\"motor_symmetry\":%.5f,\"motor_shape\":%d,"
                "\"segment_trigger\":%d,\"segment_overlap\":%d,"
                "\"segment_layers\":%.0f,\"segment_rate\":%.5f,"
                "\"segment_repeats\":%.0f,\"segment_step\":%.5f,"
                "\"segment_pitch\":%.5f,\"segment_level\":%.5f,"
                "\"segment_curve\":%.5f,"
                "\"lane_blend\":%d,\"lane_path_shape\":%d,"
                "\"lane_path_offset\":%.5f,\"lane_path_skew\":%.5f,"
                "\"lane_path_curve\":%.5f,\"lane_slew\":%.5f,"
                "\"grain_spray\":%.5f,\"grain_bias\":%d,"
                "\"grain_pitch_spray\":%.5f,\"grain_reverse\":%.5f,"
                "\"grain_size_variation\":%.5f,\"grain_level_variation\":%.5f,"
                "\"grain_timing\":%d,\"grain_timing_scatter\":%.5f,"
                "\"grain_envelope_skew\":%.5f,\"grain_process\":%d,"
                "\"grain_process_amount\":%.5f,\"grain_regions\":%.0f,"
                "\"grain_advance\":%d,\"cut_clock\":%d,\"cut_division\":%d,"
                "\"cut_rate\":%.5f,\"cut_time_variation\":%.5f,"
                "\"cut_gate\":%.5f,\"cut_join\":%.5f,\"cut_regions\":%.0f,"
                "\"cut_repeats\":%.0f,\"cut_reverse\":%.5f,"
                "\"cut_pitch_variation\":%.5f,\"cut_level_variation\":%.5f,"
                "\"cut_lane_advance\":%d,\"cut_lane_order\":%d,\"cut_poly_path\":%d,"
                "\"cut_region_mode\":%d,\"sample_mode\":%d,"
                "\"slice_region_mode\":%d,\"slice_regions\":%.0f,"
                "\"glitch_mode\":%d,\"glitch_clock\":%d,"
                "\"glitch_division\":%d,\"glitch_rate\":%.5f,"
                "\"glitch_slice\":%.5f,\"glitch_repeats\":%.0f,"
                "\"glitch_chance\":%.5f,\"glitch_mix\":%.5f,"
                "\"glitch_steps\":%.0f,\"glitch_pulses\":%.0f,"
                "\"glitch_rotate\":%.0f,\"glitch_jump\":%.5f,"
                "\"glitch_pitch\":%.5f,\"glitch_reverse\":%.5f,"
                "\"glitch_decay\":%.5f,\"glitch_seed\":%.0f,"
                "\"effect_tone\":%.5f,\"effect_drive\":%.5f,"
                "\"effect_bits\":%.5f,\"effect_rate\":%.0f,"
                "\"effect_delay\":%.5f,\"effect_feedback\":%.5f,"
                "\"effect_width\":%.5f,\"effect_mix\":%.5f,"
                "\"effect_cross_pan\":%.5f,\"effect_rotary_rate\":%.5f,"
                "\"effect_rotary_depth\":%.5f,\"effect_rotary_mix\":%.5f,"
                "\"effect_rotary_ramp\":%.5f,\"effect_rotary_balance\":%.5f,"
                "\"effect_rotary_crossover\":%.5f,\"effect_rotary_spread\":%.5f,"
                "\"effect_tape_format\":%d,\"effect_tape_drive\":%.5f,"
                "\"effect_tape_wow_rate\":%.5f,\"effect_tape_wow_depth\":%.5f,"
                "\"effect_tape_flutter_rate\":%.5f,\"effect_tape_flutter_depth\":%.5f,"
                "\"effect_tape_hiss\":%.5f,\"effect_tape_mix\":%.5f,"
                "\"effect_tape_age\":%.5f,\"effect_tape_crinkle\":%.5f,"
                "\"effect_tape_mechanics\":%.5f,\"effect_tape_inertia\":%.5f,"
                "\"long_grain_mode\":%d,\"long_grain_length\":%.5f,"
                "\"long_grain_layers\":%.0f,\"long_grain_pitch\":%.5f,"
                "\"long_grain_shape\":%d,\"long_grain_edge\":%.5f,"
                "\"long_grain_spread\":%.5f,\"long_grain_reverse\":%.5f,"
                "\"long_grain_spray\":%.5f,\"long_grain_bias\":%d,"
                "\"long_grain_duration_variation\":%.5f,"
                "\"long_grain_pitch_spray\":%.5f,"
                "\"long_grain_level_variation\":%.5f,"
                "\"long_grain_phase\":%.5f,\"long_grain_advance\":%d,"
                "\"waveset_group\":%.0f,\"waveset_repeats\":%.0f,"
                "\"waveset_stride\":%.0f,\"waveset_advance\":%d,"
                "\"waveset_direction\":%d,\"waveset_process\":%d,"
                "\"waveset_amount\":%.5f,\"waveset_detail\":%d,"
                "\"effect_reverb_algo\":%.5f,\"effect_reverb_decay\":%.5f,\"effect_reverb_pre\":%.5f,\"effect_reverb_damp\":%.5f,\"effect_reverb_diff\":%.5f,\"effect_reverb_mod\":%.5f,\"effect_reverb_width\":%.5f,\"effect_reverb_mix\":%.5f,\"effect_order\":%.5f,"
                "\"pad_mode\":%d,\"lane_variation\":%d,\"clip_record_mode\":%d,"
                "\"preview_level\":%.5f,\"preview_route\":%d,\"grain_quality\":%zu,"
                "\"launch_quantize\":%d,\"midi_control_mode\":%d,"
                "\"midi_cc_channel\":%d,\"midi_cc_base\":%d,"
                "\"midi_takeover\":%d,\"midi_control_part\":%d,"
                "\"midi_note_route\":%d,\"midi_note_channel\":%d,\"midi_bend_range\":%.4f,\"midi_mod_target\":%d,\"midi_velocity_depth\":%.4f,\"midi_sync_source\":%d,\"midi_sync_transport\":%d,"
                "\"next_capture_id\":%llu,"
                "\"generation\":%llu,\"complete\":1}",
                controls.locus, controls.field, controls.speed,
                controls.manualLane + 1.0f, controls.laneCycles,
                static_cast<int>(controls.algorithm), controls.join,
                outputGain, polyphonyCompensation ? 1 : 0, recordGain,
                controls.ampAttackMs,
                controls.ampDecayMs, controls.ampSustain * 100.0f,
                controls.ampReleaseMs, pressureMode, holdReleaseMs,
                rootNote, padOctave, static_cast<int>(padScale),
                static_cast<unsigned>(selectedLane + 1u), recordSource,
                static_cast<int>(controls.motion), static_cast<int>(controls.motionSound),
                controls.motionSoundRateHz,
                static_cast<int>(controls.motionRateBasis), controls.motionPathRate,
                controls.motionStep, controls.motionSeed,
                static_cast<int>(controls.lanes),
                static_cast<int>(controls.lanePath), controls.lanesStretch ? 1 : 0,
                controls.lanesStretchRatio,
                controls.laneGains[0u], controls.laneGains[1u],
                controls.laneGains[2u], controls.laneGains[3u],
                static_cast<int>(controls.grains), controls.grainSizeMs,
                controls.grainDensityHz, controls.grainPitch,
                static_cast<int>(controls.grainWindow), controls.grainStereoSpread,
                static_cast<int>(controls.cutups), tempoBpm,
                tempoSwing * 100.0f,
                chainEnabled ? 1 : 0,
                controls.motionTravel, controls.motionJitter, controls.packetDuty,
                controls.motorRateHz, controls.motorSymmetry,
                static_cast<int>(controls.motorShape),
                static_cast<int>(controls.segmentTrigger),
                static_cast<int>(controls.segmentOverlap),
                controls.segmentLayerLimit, controls.segmentRateHz,
                controls.segmentRepeats, controls.segmentStep,
                controls.segmentPitchScatter, controls.segmentLevelVariation,
                controls.segmentIntervalCurve,
                static_cast<int>(controls.laneBlend),
                static_cast<int>(controls.lanePathShape), controls.lanePathOffset,
                controls.lanePathSkew, controls.lanePathCurve, controls.laneSlewMs,
                controls.grainSpray, static_cast<int>(controls.grainBias),
                controls.grainPitchSpray, controls.grainReverseChance,
                controls.grainSizeVariation, controls.grainLevelVariation,
                static_cast<int>(controls.grainTiming), controls.grainTimingScatter,
                controls.grainEnvelopeSkew, static_cast<int>(controls.grainProcess),
                controls.grainProcessAmount, controls.grainRegionCount,
                static_cast<int>(controls.grainAdvance),
                static_cast<int>(controls.cutClock),
                static_cast<int>(controls.cutDivision), controls.cutRateHz,
                controls.cutTimeVariation, controls.cutGate, controls.cutJoinMs,
                controls.cutRegionCount, controls.cutRepeats,
                controls.cutReverseChance, controls.cutPitchVariation,
                controls.cutLevelVariation, static_cast<int>(controls.cutLaneAdvance), static_cast<int>(controls.cutLaneOrder),
                static_cast<int>(controls.cutPolyPath),
                static_cast<int>(controls.cutRegionMode),
                static_cast<int>(controls.sampleMode),
                static_cast<int>(controls.sampleRegionMode),
                controls.sampleRegionCount,
                static_cast<int>(controls.glitchMode),
                static_cast<int>(controls.glitchClock),
                static_cast<int>(controls.glitchDivision), controls.glitchRateHz,
                controls.glitchSliceMs, controls.glitchRepeats,
                controls.glitchChance, controls.glitchMix,
                controls.glitchSteps, controls.glitchPulses,
                controls.glitchRotate, controls.glitchJumpMs,
                controls.glitchPitch, controls.glitchReverseChance,
                controls.glitchDecay, controls.glitchSeed,
                controls.effectTone, controls.effectDrive, controls.effectBits,
                controls.effectRate, controls.effectDelayMs,
                controls.effectFeedback, controls.effectWidth, controls.effectMix,
                controls.effectCrossPan, controls.effectRotaryRateHz,
                controls.effectRotaryDepth, controls.effectRotaryMix,
                controls.effectRotaryRampMs, controls.effectRotaryBalance,
                controls.effectRotaryCrossoverHz, controls.effectRotarySpread,
                controls.effectTapeFormat, controls.effectTapeDrive,
                controls.effectTapeWowRateHz, controls.effectTapeWowDepth,
                controls.effectTapeFlutterRateHz, controls.effectTapeFlutterDepth,
                controls.effectTapeHissDb, controls.effectTapeMix,
                controls.effectTapeAge, controls.effectTapeCrinkle,
                controls.effectTapeMechanics, controls.effectTapeInertia,
                static_cast<int>(controls.longGrainMode),
                controls.longGrainLengthMs, controls.longGrainLayers,
                controls.longGrainPitch, static_cast<int>(controls.longGrainShape),
                controls.longGrainEdgeMs, controls.longGrainSpread,
                controls.longGrainReverseChance, controls.longGrainSpray,
                static_cast<int>(controls.longGrainBias),
                controls.longGrainDurationVariation, controls.longGrainPitchSpray,
                controls.longGrainLevelVariation, controls.longGrainPhaseSpread,
                static_cast<int>(controls.longGrainAdvance),
                controls.wavesetGroup, controls.wavesetRepeats,
                controls.wavesetStride,
                static_cast<int>(controls.wavesetAdvance),
                static_cast<int>(controls.wavesetDirection),
                static_cast<int>(controls.wavesetProcess),
                controls.wavesetProcessAmount,
                static_cast<int>(controls.wavesetDetail),
                static_cast<double>(controls.effectReverbAlgorithm),
                static_cast<double>(controls.effectReverbDecay),
                static_cast<double>(controls.effectReverbPre),
                static_cast<double>(controls.effectReverbDamp),
                controls.effectReverbDiff * 100.0f,
                controls.effectReverbMod * 100.0f,
                controls.effectReverbWidth * 100.0f,
                controls.effectReverbMix * 100.0f,
                static_cast<double>(controls.effectOrder),
                static_cast<int>(padMode),
                static_cast<int>(laneVariation), static_cast<int>(clipRecordMode),
                previewLevel, previewRoute, grainVoiceLimit,
                static_cast<int>(launchQuantize),
                midiControlMode, midiCcChannel, midiCcBase,
                midiTakeover, midiControlPart,
                midiNoteRoute,midiNoteChannel,midiBendRange,midiModTarget,midiVelocityDepth*100,midiSyncSource,midiSyncTransport,
                static_cast<unsigned long long>(nextCaptureId),
                static_cast<unsigned long long>(generation == kAnyPersistenceGeneration
                    ? persistenceGeneration : generation));
            int result=appendLayersJson(buffer,bufferLength,appendPatternFadeJson(buffer,bufferLength,appendSplitJson(buffer,bufferLength,length)));
            if(result<=0||result>=bufferLength)return -1;
            const int extra=std::snprintf(buffer+result-1,bufferLength-result+1,",\"sequence_mode\":%d}",sequenceMode()?1:0);
            return extra<0||extra>=bufferLength-result+1?-1:result-1+extra;
        }
    };
    static_assert(std::is_trivially_copyable<PersistentStateSnapshot>::value,
        "metadata snapshot must be an owned plain value");
    static_assert(sizeof(PersistentStateSnapshot)<=32768u,
        "metadata capture must stay bounded independently of clip content");
