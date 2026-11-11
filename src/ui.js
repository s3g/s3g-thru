/* s3g Thru 0.31.0 — polyphonic sampler/performance-looper takeover for Move. */

import * as moveConstants from '/data/UserData/schwung/shared/constants.mjs';
import * as moveInputFilter from '/data/UserData/schwung/shared/input_filter.mjs';
import * as os from 'os';
import * as thruMidi from './midi-map.mjs';

function constant(name, fallback) {
    const value = moveConstants[name];
    return Number.isFinite(value) ? value : fallback;
}

const MoveMainKnob = constant('MoveMainKnob', 14);
const MoveMainButton = constant('MoveMainButton', 3);
const MoveShift = constant('MoveShift', 49);
const MoveMenu = constant('MoveMenu', 50);
const MoveBack = constant('MoveBack', 51);
const MoveUndo = constant('MoveUndo', 56);
const MoveUp = constant('MoveUp', 55);
const MoveDown = constant('MoveDown', 54);
const MoveLeft = constant('MoveLeft', 62);
const MoveRight = constant('MoveRight', 63);
const MoveRecord = constant('MoveRec', 86);
const MoveRecordAlt = constant('MoveRecord', 118);
const MovePlay = constant('MovePlay', 85);
const MoveLoop = constant('MoveLoop', 58);
const MoveDelete = constant('MoveDelete', 119);
const MoveKnob1 = constant('MoveKnob1', 71);
// Move's eight capacitive knob touches are note numbers 0..7 (Schwung page_input.mjs).
const MoveCrossfaderTouch = 2;
const MoveCopy = constant('MoveCopy', 60);
const MoveCapture = constant('MoveCapture', 52);
const MoveMute = constant('MoveMute', 88);
const MovePads = Array.isArray(moveConstants.MovePads)
    ? moveConstants.MovePads : Array.from({ length: 32 }, (_, i) => 68 + i);
const MoveSteps = Array.isArray(moveConstants.MoveSteps)
    ? moveConstants.MoveSteps : Array.from({ length: 16 }, (_, i) => 16 + i);
const MoveTracks = [
    constant('MoveRow1', 43), constant('MoveRow2', 42),
    constant('MoveRow3', 41), constant('MoveRow4', 40),
];

const Black = constant('Black', 0);
const ScaleNoteGrey = constant('DarkGrey3', 123);
// Dim teal stays visibly colored beside A's neutral scale map.
const SplitBIdle = constant('DeepTealCyan', 93);
const SplitBRoot = constant('TealCyan', 15);
const SharedBIdle = constant('Mauve',35);
const SharedBRoot = constant('PalePink',52);
const White = constant('White', 120);
const Cyan = constant('Cyan', 14);
const BrightRed = constant('BrightRed', 1);
const BrightOrange = constant('BrightOrange', 3);
const BrightYellow = constant('BrightYellow', 8);
const NeonGreen = constant('NeonGreen', 11);
const AzureBlue = constant('AzureBlue', 16);
const BlueViolet = constant('BlueViolet', 18);
const Purple = constant('Purple', 22);
const LightMagenta = constant('LightMagenta', 26);
const DeepRed = constant('DeepRed', 65);
const BurntSienna = constant('BurntSienna', 69);
const DarkYellowOlive = constant('DarkYellowOlive', 79);
const DarkGrass = constant('DarkGrass', 85);
const DarkAzure = constant('DarkAzure', 95);
const DarkBlueViolet = constant('DarkBlueViolet', 99);
const DarkPurple = constant('DarkPurple', 107);
const DarkLightMagenta = constant('DarkLightMagenta', 115);
const WhiteLedDim = constant('WhiteLedDim', 8);
const WhiteLedBright = constant('WhiteLedBright', 127);
// These icon keys use white brightness, independently of the RGB palette.
const IconLedDim = constant('WhiteLedDim', 16);
const IconLedMedium = constant('WhiteLedMedium', 64);
const ICON_BUTTONS = [MoveMenu,MoveBack,MoveUndo,MoveShift,MoveDelete,MoveUp,MoveDown,
    MoveLeft,MoveRight,MoveCopy,MoveCapture,MoveMute,MoveLoop];

const setLED = typeof moveInputFilter.setLED === 'function'
    ? moveInputFilter.setLED
    : function(note, color) {
        if (typeof move_midi_internal_send === 'function') {
            move_midi_internal_send([0x09, 0x90, note, color]);
        }
    };

const setButtonLED = typeof moveInputFilter.setButtonLED === 'function'
    ? moveInputFilter.setButtonLED
    : function(cc, color) {
        if (typeof move_midi_internal_send === 'function') {
            move_midi_internal_send([0x0B, 0xB0, cc, color]);
        }
    };

const UI_VERSION = '0.31.0';
const HELP_PAGE_COUNT=5;
const WAVE_BINS = 112;
const ALGORITHMS = ['MOTION', 'LANES', 'GRAINS', 'CUTUPS', 'SAMPLE', 'SLICES',
    'GLITCH', 'WEAVE', 'WAVESETS', 'PRISM', 'MORPH', 'TEXTURE', 'FORMANT', 'STOCH', 'PULSAR'];
const ROLL_DIVISIONS = ['1/4', '1/8', '1/8T', '1/16', '1/16T', '1/32'];
const SCALE_LABELS = [
    'CHROMATIC', 'MAJOR', 'MINOR', 'DORIAN', 'MIXOLYDIAN', 'LYDIAN',
    'PHRYGIAN', 'LOCRIAN', 'WHOLE TONE', 'HALF-WHOLE DIM',
    'WHOLE-HALF DIM', 'MINOR BLUES', 'MINOR PENTATONIC',
    'MAJOR PENTATONIC', 'HARMONIC MINOR', 'HARMONIC MAJOR', 'DORIAN #4',
    'PHRYGIAN DOMINANT', 'MELODIC MINOR', 'LYDIAN AUGMENTED',
    'LYDIAN DOMINANT', 'SUPER LOCRIAN', '8-TONE SPANISH', 'BHAIRAV',
    'HUNGARIAN MINOR',
];
const SCALE_ABBREVIATIONS = [
    'CHR', 'MAJ', 'MIN', 'DOR', 'MIX', 'LYD', 'PHR', 'LOC', 'WT', 'H-WD',
    'W-HD', 'MBLU', 'MPEN', 'JPEN', 'HMIN', 'HMAJ', 'DOR#', 'PDOM', 'MMIN',
    'LYD+', 'LYDD', 'SLOC', '8SPA', 'BHAI', 'HUNG',
];
const SCALE_INTERVALS = [
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
    [0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 8, 10],
    [0, 2, 3, 5, 7, 9, 10], [0, 2, 4, 5, 7, 9, 10],
    [0, 2, 4, 6, 7, 9, 11], [0, 1, 3, 5, 7, 8, 10],
    [0, 1, 3, 5, 6, 8, 10], [0, 2, 4, 6, 8, 10],
    [0, 1, 3, 4, 6, 7, 9, 10], [0, 2, 3, 5, 6, 8, 9, 11],
    [0, 3, 5, 6, 7, 10], [0, 3, 5, 7, 10], [0, 2, 4, 7, 9],
    [0, 2, 3, 5, 7, 8, 11], [0, 2, 4, 5, 7, 8, 11],
    [0, 2, 3, 6, 7, 9, 10], [0, 1, 4, 5, 7, 8, 10],
    [0, 2, 3, 5, 7, 9, 11], [0, 2, 4, 6, 8, 9, 11],
    [0, 2, 4, 6, 7, 9, 10], [0, 1, 3, 4, 6, 8, 10],
    [0, 1, 3, 4, 5, 6, 8, 10], [0, 1, 4, 5, 7, 8, 11],
    [0, 2, 3, 6, 7, 8, 11],
];
const LEDS_PER_TICK = 8;
const GLITCH_LOCK_TAP_TICKS = 30;
const PARAMETER_ENCODER_RESPONSE = 0.375;
const CHAIN_BANK_ENCODER_RESPONSE = 0.25;
const VOICE_COLORS = [BrightRed, BrightOrange, BrightYellow, NeonGreen,
    AzureBlue, BlueViolet, Purple, LightMagenta];
const VOICE_DIM_COLORS = [DeepRed, BurntSienna, DarkYellowOlive, DarkGrass,
    DarkAzure, DarkBlueViolet, DarkPurple, DarkLightMagenta];
const LIBRARY_ROOT = '/data/UserData/UserLibrary/Samples';
const LIBRARY_HOME = '@s3g-thru-sources';
const BANK_NAMES = ['A', 'B', 'C', 'D'];
const parameterEncoderMotion = Array.from({ length: 8 }, () => ({
    key: '', remainder: 0,
}));

const PARAMS = {
    bank_slot:{key:'bank_slot',label:'BANK PAD',shortLabel:'PAD',min:1,max:16,step:1,value:1,integer:true},
    bank_start:{key:'bank_start',label:'START',shortLabel:'STRT',min:0,max:1,step:.005,value:0,format:v=>Math.round(v*100)+'%'},
    bank_end:{key:'bank_end',label:'END',min:0,max:1,step:.005,value:1,format:v=>Math.round(v*100)+'%'},
    bank_rate:{key:'bank_rate',label:'PAD RATIO',shortLabel:'RTO',min:.125,max:8,step:.01,value:1,format:v=>v.toFixed(2)+'x'},
    bank_master_rate:{key:'bank_master_rate',label:'BANK MASTER RATE',shortLabel:'RATE',min:.125,max:8,step:.01,value:1},
    bank_pan:{key:'bank_pan',label:'PAD PAN',shortLabel:'PAN',min:-100,max:100,step:1,value:0},
    bank_play:{key:'bank_play',label:'PAD PLAYBACK',shortLabel:'PLAY',min:0,max:1,step:1,value:0,integer:true,labels:['LOOP','ONE']},
    bank_choke:{key:'bank_choke',label:'CHOKE GROUP',shortLabel:'CHOK',min:0,max:4,step:1,value:0,integer:true,labels:['OFF','1','2','3','4']},
    bank_record_source:{key:'bank_record_source',label:'RECORD SOURCE',shortLabel:'SRC',min:0,max:3,step:1,value:2,integer:true,labels:['IN','MIX','PT A','PT B']},
    bank_reverse:{key:'bank_reverse',label:'REVERSE',shortLabel:'REV',min:0,max:1,step:1,value:0,integer:true,labels:['OFF','ON']},
    bank_level:{key:'bank_level',label:'LEVEL',shortLabel:'LVL',min:0,max:2,step:.01,value:1,format:v=>Math.round(v*100)+'%'},
    bank_attack:{key:'bank_attack',label:'ATTACK',shortLabel:'ATK',min:1,max:10000,step:1,value:5,format:v=>Math.round(v)+'ms'},
    bank_release:{key:'bank_release',label:'RELEASE',shortLabel:'REL',min:5,max:10000,step:1,value:40,format:v=>Math.round(v)+'ms'},
    bank_fade:{key:'bank_fade',label:'LOOP FADE',shortLabel:'FADE',min:2,max:100,step:1,value:8,format:v=>Math.round(v)+'ms'},
    voice_mode:{key:'voice_mode',label:'KEY VOICES',shortLabel:'VOICE',min:0,max:2,step:1,value:0,integer:true,labels:['POLY','MONO','LEGATO']},
    midi_note_route:{key:'midi_note_route',label:'NOTE ROUTE',shortLabel:'ROUT',min:0,max:1,step:1,value:0,integer:true,labels:['PARTS','FOCUS']},
    midi_note_channel:{key:'midi_note_channel',label:'FOCUS CHANNEL',shortLabel:'CHAN',min:1,max:4,step:1,value:1,integer:true},
    midi_bend_range:{key:'midi_bend_range',label:'BEND RANGE',shortLabel:'BEND',min:0,max:24,step:1,value:2,integer:true,unit:'st'},
    midi_mod_target:{key:'midi_mod_target',label:'MOD WHEEL',shortLabel:'WHEL',min:0,max:2,step:1,value:0,integer:true,labels:['OFF','STACK','CHAR']},
    midi_velocity_depth:{key:'midi_velocity_depth',label:'NOTE VELOCITY',shortLabel:'VELO',min:0,max:100,step:1,value:0,integer:true,unit:'%'},
    midi_sync_source:{key:'midi_sync_source',label:'CLOCK SOURCE',shortLabel:'SYNC',min:0,max:1,step:1,value:0,integer:true,labels:['INT','MOVE']},
    midi_sync_transport:{key:'midi_sync_transport',label:'TRANSPORT',shortLabel:'RUN',min:0,max:1,step:1,value:0,integer:true,labels:['TEMPO','CLIPS']},
    midi_control_mode:{key:'midi_control_mode',label:'MIDI MODE',shortLabel:'MODE',min:0,max:6,step:1,value:0,integer:true,labels:['AUTO','NRPN','CC','MNLT','E16','E+M','OFF']},
    midi_cc_channel:{key:'midi_cc_channel',label:'CC CHANNEL',shortLabel:'CCCH',min:1,max:16,step:1,value:9,integer:true},
    midi_cc_base:{key:'midi_cc_base',label:'CC BASE',shortLabel:'BASE',min:0,max:120,step:1,value:20,integer:true},
    midi_takeover:{key:'midi_takeover',label:'TAKEOVER',shortLabel:'TAKE',min:0,max:1,step:1,value:0,integer:true,labels:['PICK','JUMP']},
    midi_control_part:{key:'midi_control_part',label:'CONTROL PART',shortLabel:'PART',min:0,max:3,step:1,value:0,integer:true,labels:['A','B','C','D']},
    voice_engine_assign:{key:'voice_engine_assign',label:'ASSIGNMENT',shortLabel:'ASSN',min:0,max:2,step:1,value:0,integer:true,labels:['TOUCH','DEGREE','REGISTER']},
    voice_engine_octave:{key:'voice_engine_octave',label:'ENTRY OCTAVE',shortLabel:'OCTV',min:-2,max:2,step:1,value:0,integer:true},
    voice_engine_position:{key:'voice_engine_position',label:'SOURCE OFFSET',shortLabel:'SPOS',min:-100,max:100,step:2,value:0,integer:false},
    voice_engine_stack:{key:'voice_engine_stack',label:'STACK OFFSET',shortLabel:'STAK',min:-100,max:100,step:2,value:0,integer:false},
    voice_engine_pan:{key:'voice_engine_pan',label:'ENTRY PAN',shortLabel:'PAN',min:-100,max:100,step:2,value:0,integer:false},
    voice_engine_depth:{key:'voice_engine_depth',label:'PRESSURE DEPTH',shortLabel:'DPTH',min:0,max:100,step:2,value:100,integer:false},
    voice_engine_mode:{key:'voice_engine_mode',label:'ENGINES',shortLabel:'ENGS',min:0,max:1,step:1,value:0,integer:true,labels:['ONE','LIST']},
    voice_engine_order:{key:'voice_engine_order',label:'LIST ORDER',shortLabel:'ORDR',min:0,max:5,step:1,value:0,integer:true,labels:['UP','DOWN','RAND','CYC+','CYC-','SHUF']},
    voice_engine_edit:{key:'voice_engine_edit',label:'EDIT SLOT',min:1,max:8,step:1,value:1,integer:true},
    ...Object.fromEntries(Array.from({length:8},(_,i)=>['voice_engine_'+(i+1),{key:'voice_engine_'+(i+1),label:'SLOT '+(i+1),shortLabel:'E'+(i+1),min:0,max:14,step:1,value:0,integer:true,labels:ALGORITHMS}])),
    split_mode: {key:'split_mode',label:'MODE',min:0,max:6,step:1,value:0,integer:true,labels:['SINGLE','SPLIT','SHARED','QUAD','BANK','LAYERS','SEQUENCE']},
    split_focus: {key:'split_focus',label:'EDIT PART',min:0,max:3,step:1,value:0,integer:true,labels:['A','B','C','D']},
    split_copy: {key:'split_copy',label:'COPY OTHER',min:0,max:1,step:1,value:0,integer:true,labels:['TURN','COPY'],momentary:true,result:'COPIED OTHER PART'},
    part_mute: {key:'part_mute',label:'MUTE',min:0,max:1,step:1,value:0,integer:true,labels:['OFF','ON']},
    split_level_c: {key:'split_level_c',label:'C LEVEL',min:0,max:2,step:.01,value:1},
    split_delay_c: {key:'split_delay_c',label:'C DELAY',min:0,max:1,step:.01,value:0},
    split_reverb_c: {key:'split_reverb_c',label:'C REVERB',min:0,max:1,step:.01,value:0},
    split_level_d: {key:'split_level_d',label:'D LEVEL',min:0,max:2,step:.01,value:1},
    split_delay_d: {key:'split_delay_d',label:'D DELAY',min:0,max:1,step:.01,value:0},
    split_reverb_d: {key:'split_reverb_d',label:'D REVERB',min:0,max:1,step:.01,value:0},
    split_blend: {key:'split_blend',label:'A B BLEND',min:0,max:1,step:.01,value:.5},
    split_level_a: {key:'split_level_a',label:'A LEVEL',min:0,max:2,step:.02,value:1},
    split_level_b: {key:'split_level_b',label:'B LEVEL',min:0,max:2,step:.02,value:1},
    split_delay_a: {key:'split_delay_a',label:'A DELAY',min:0,max:1,step:.01,value:0},
    split_delay_b: {key:'split_delay_b',label:'B DELAY',min:0,max:1,step:.01,value:0},
    split_reverb_a: {key:'split_reverb_a',label:'A REVERB',min:0,max:1,step:.01,value:0},
    split_reverb_b: {key:'split_reverb_b',label:'B REVERB',min:0,max:1,step:.01,value:0},
    split_curve: {key:'split_curve',label:'CURVE',min:0,max:1,step:1,value:1,integer:true,labels:['LINEAR','POWER']},
    split_priority: {key:'split_priority',label:'A B PRIORITY',min:-24,max:24,step:.5,value:0,unit:'dB'},
    split_duck_threshold: {key:'split_duck_threshold',label:'THRESHOLD',min:-60,max:0,step:.5,value:-24,unit:'dBFS'},
    split_duck_attack: {key:'split_duck_attack',label:'ATTACK',min:1,max:250,step:1,value:12,unit:'ms'},
    split_duck_hold: {key:'split_duck_hold',label:'HOLD',min:0,max:1000,step:5,value:40,unit:'ms'},
    split_duck_release: {key:'split_duck_release',label:'RELEASE',min:10,max:3000,step:10,value:220,unit:'ms'},
    split_duck_hpf: {key:'split_duck_hpf',label:'DETECTOR HPF',min:20,max:2000,step:10,value:100,unit:'Hz'},
    split_duck_knee: {key:'split_duck_knee',label:'KNEE',min:0,max:24,step:.5,value:6,unit:'dB'},
    split_duck_route: {key:'split_duck_route',label:'ROUTE',min:0,max:1,step:1,value:1,integer:true,labels:['DRY','DRY + SENDS']},
    locus: { key: 'locus', label: 'POSITION', min: 0, max: 1, step: 0.01, value: 0.5 },
    field: { key: 'field', label: 'FIELD', min: 0.02, max: 1, step: 0.01, value: 1 },
    speed: { key: 'speed', label: 'SPEED', min: 0.05, max: 4, step: 0.02, value: 1 },
    manual_lane: { key: 'manual_lane', label: 'STACK START', min: 1, max: 4, step: 0.04, value: 1 },
    cut_lane_advance: { key: 'cut_lane_advance', label: 'ADVANCE', min: 0, max: 6, step: 1, value: 1, integer: true, labels: ['HOLD', '1', '2', '3', '4', '8', '16'] },
    lane_cycles: { key: 'lane_cycles', label: 'SCAN RATE', min: 0.125, max: 16, step: 0.125, value: 1 },
    prism_mode: {"key":"prism_mode","min":0,"max":4,"step":1,"label":"PRISM MODE","value":0,"integer":true,"labels":["SCAN","HOLD","AMP","FREQ","ACCU"]},
    prism_blur: {"key":"prism_blur","min":0,"max":2000,"step":10,"label":"SPECTRAL BLUR","value":80},
    prism_decay: {"key":"prism_decay","min":50,"max":20000,"step":100,"label":"ACCUMULATION DECAY","value":2000},
    prism_pitch: {"key":"prism_pitch","min":-24,"max":24,"step":1,"label":"PRISM TRANSPOSE","value":0},
    prism_tilt: {"key":"prism_tilt","min":-100,"max":100,"step":2,"label":"SPECTRAL TILT","value":0},
    prism_smear: {"key":"prism_smear","min":0,"max":100,"step":2,"label":"FREQUENCY SMEAR","value":0},
    pulsar_emit_mod_rate: {"key":"pulsar_emit_mod_rate","label":"ERAT","min":0.01,"max":20,"value":0.13,"step":0.01,"unit":"Hz"},
    pulsar_emit_mod_depth: {"key":"pulsar_emit_mod_depth","label":"EDEP","min":0,"max":24,"value":0,"step":0.25,"unit":"st"},
    pulsar_form_mod_rate: {"key":"pulsar_form_mod_rate","label":"FRAT","min":0.01,"max":20,"value":1.7,"step":0.01,"unit":"Hz"},
    pulsar_form_mod_depth: {"key":"pulsar_form_mod_depth","label":"FDEP","min":0,"max":24,"value":0,"step":0.25,"unit":"st"},
    pulsar_spray: {"key":"pulsar_spray","label":"SPRY","min":0,"max":100,"value":0,"step":2,"unit":"%"},
    pulsar_form_variation: {"key":"pulsar_form_variation","label":"FVAR","min":0,"max":24,"value":0,"step":0.25,"unit":"st"},
    pulsar_reverse: {"key":"pulsar_reverse","label":"REV%","min":0,"max":100,"value":0,"step":2,"unit":"%"},
    pulsar_keytrack: {"key":"pulsar_keytrack","label":"KEYT","min":0,"max":100,"value":0,"step":2,"unit":"%"},
    pulsar_rate: {"key":"pulsar_rate","label":"RATE","min":0.1,"max":2000,"value":261.625565,"step":0.1,"unit":"Hz"},
    pulsar_form: {"key":"pulsar_form","label":"FORM","min":1,"max":2000,"value":500,"step":0.1,"unit":"Hz"},
    pulsar_size: {"key":"pulsar_size","label":"SIZE","min":0.5,"max":1000,"value":4,"step":0.01,"unit":"ms"},
    pulsar_window: {"key":"pulsar_window","label":"WIN","min":0,"max":4,"value":0,"step":1,"integer":true,"labels":["HANN","BELL","TAPR","PERC","REV"]},
    pulsar_skew: {"key":"pulsar_skew","label":"SKEW","min":0,"max":100,"value":50,"step":2,"unit":"%"},
    pulsar_omit: {"key":"pulsar_omit","label":"OMIT","min":0,"max":100,"value":0,"step":2,"unit":"%"},
    pulsar_burst: {"key":"pulsar_burst","label":"BRST","min":0,"max":8,"value":0,"step":1,"integer":true,"labels":["CONT","2:1","3:1","4:1","1:1","2:2","3:3","4:4","8:8"]},
    pulsar_jitter: {"key":"pulsar_jitter","label":"JITT","min":0,"max":100,"value":0,"step":2,"unit":"%"},
    pulsar_source_hold: {"key":"pulsar_source_hold","label":"SOURCE HOLD","min":0,"max":1,"value":0,"step":1,"integer":true,"labels":["OFF","ON"]},
    pulsar_hold_position: {"key":"pulsar_hold_position","label":"HELD POSITION","min":0,"max":1,"value":0.5,"step":0.1},
    stochastic_source_hold: {key:"stochastic_source_hold",label:"SOURCE HOLD",min:0,max:1,step:1,value:0,integer:true},
    stochastic_hold_position: {key:"stochastic_hold_position",label:"HELD SOURCE POSITION",min:0,max:1,step:0.001,value:.5},
    stochastic_size: {"key":"stochastic_size","label":"SIZE","min":1,"max":64,"step":1,"value":2,"integer":true},
    stochastic_amplitude: {"key":"stochastic_amplitude","label":"AMPLITUDE MUTATION","min":0,"max":100,"step":2,"value":65,"unit":"%"},
    stochastic_timing: {"key":"stochastic_timing","label":"TIMING MUTATION","min":0,"max":100,"step":2,"value":70,"unit":"%"},
    stochastic_evolution: {"key":"stochastic_evolution","label":"EVOLUTION RATE","min":0,"max":1024,"step":0.25,"value":100},
    stochastic_memory: {"key":"stochastic_memory","label":"WALK MEMORY","min":0,"max":100,"step":2,"value":80,"unit":"%"},
    stochastic_capture: {key:"stochastic_capture",label:"LISTENER CAPTURE",min:0,max:100,step:2,value:65,unit:"%"},
    stochastic_pull: {"key":"stochastic_pull","label":"SOURCE ATTRACTION","min":0,"max":100,"step":2,"value":10,"unit":"%"},
    stochastic_law: {"key":"stochastic_law","label":"WALK LAW","min":0,"max":3,"step":1,"value":2,"integer":true,"labels":["SMTH","FLAT","WIDE","LSTN"]},
    stochastic_drift: {"key":"stochastic_drift","label":"PERIOD DRIFT","min":0,"max":100,"step":2,"value":0,"unit":"%"},
    formant_pitch: {key:'formant_pitch',label:'FORMANT TRANSPOSE',min:-24,max:24,step:1,value:0},
    formant_shift: {key:'formant_shift',label:'FORMANT SHIFT',min:-24,max:24,step:1,value:0},
    formant_smoothing: {key:'formant_smoothing',label:'FORMANT SMOOTHING',min:5,max:200,step:5,value:30},
    formant_tune: {key:'formant_tune',label:'PITCH LOCK',min:0,max:100,step:2,value:0},
    formant_freeze: {key:'formant_freeze',label:'FREEZE SOURCE',min:0,max:1,step:1,value:0,integer:true,labels:['OFF','ON']},
    formant_keytrack: {key:'formant_keytrack',label:'FORMANT KEY TRACK',min:-100,max:100,step:2,value:0},
    formant_rough: {key:'formant_rough',label:'VOICING ROUGHNESS',min:0,max:100,step:2,value:0},
    formant_sub: {key:'formant_sub',label:'SUBHARMONIC',min:0,max:100,step:2,value:0},
    morph_mode: {key:"morph_mode",label:"MORPH MODE",min:0,max:1,step:1,value:0,integer:true,labels:["MORPH","IMPR"]},
    morph_amplitude: {"key":"morph_amplitude","min":0,"max":100,"step":2,"label":"AMPLITUDE MORPH","value":50},
    morph_frequency: {"key":"morph_frequency","min":0,"max":100,"step":2,"label":"FREQUENCY MORPH","value":50},
    morph_offset: {"key":"morph_offset","min":-100,"max":100,"step":2,"label":"PARTNER TIME OFFSET","value":0},
    morph_blur: {"key":"morph_blur","min":0,"max":2000,"step":10,"label":"MORPH SMOOTHING","value":40},
    morph_pitch: {"key":"morph_pitch","min":-24,"max":24,"step":1,"label":"MORPH TRANSPOSE","value":0},
    texture_mode: {"key":"texture_mode","min":0,"max":3,"step":1,"label":"TEXTURE MODE","value":0,"integer":true,"labels":["SPRAY","GROUP","MOTIF","ORN"]},
    texture_rate: {"key":"texture_rate","min":0.25,"max":24,"step":0.25,"label":"TEXTURE EVENT RATE","value":4},
    texture_length: {"key":"texture_length","min":20,"max":2000,"step":10,"label":"TEXTURE EVENT LENGTH","value":250},
    texture_scatter: {"key":"texture_scatter","min":0,"max":100,"step":2,"label":"TEXTURE TIMING SCATTER","value":20},
    texture_range: {"key":"texture_range","min":0,"max":24,"step":1,"label":"TEXTURE PITCH RANGE","value":7,"integer":true},
    texture_group: {"key":"texture_group","min":2,"max":8,"step":1,"label":"TEXTURE GROUP SIZE","value":4,"integer":true},
    texture_spread: {"key":"texture_spread","min":0,"max":100,"step":2,"label":"TEXTURE STEREO SPREAD","value":50},
    texture_seed: {"key":"texture_seed","min":1,"max":9999,"step":1,"label":"TEXTURE SEED","value":1,"integer":true},
    algorithm: { key: 'algorithm', label: 'ALGORITHM', min: 0, max: 14, step: 1, value: 0, integer: true, labels: ALGORITHMS },
    join: { key: 'join', label: 'JOIN', min: 0, max: 1, step: 0.01, value: 1 },
    output_gain: { key: 'output_gain', label: 'LEVEL', min: 0, max: 2, step: 0.02, value: 1 },
    polyphony_comp: { key: 'polyphony_comp', label: 'POLYPHONY COMP', shortLabel:'COMP', min: 0, max: 1, step: 1, value: 1, integer: true, labels: ['OFF', 'ON'] },
    amp_attack: { key: 'amp_attack', label: 'ATTACK', min: 0, max: 10000, step: 1, value: 3, integer: true },
    amp_decay: { key: 'amp_decay', label: 'DECAY', min: 0, max: 10000, step: 1, value: 0, integer: true },
    amp_sustain: { key: 'amp_sustain', label: 'SUSTAIN', min: 0, max: 100, step: 1, value: 100, integer: true },
    amp_release: { key: 'amp_release', label: 'RELEASE', min: 0, max: 20000, step: 1, value: 25, integer: true },
    motion_mode: { key: 'motion_mode', label: 'MODE', min: 0, max: 7, step: 1, value: 0, integer: true, labels: ['FWD', 'REV', 'HOVER', 'ROUND', 'LOOP', 'MIRROR', 'ZIGZAG', 'DRUNK'] },
    motion_sound: { key: 'motion_sound', label: 'SOUND', min: 0, max: 7, step: 1, value: 0, integer: true, labels: ['CONT', 'PACKETS', 'MOTOR', 'BOUNCE', 'FREEZE', 'ITERATE', 'PULSER', 'DOUBLETS'] },
    motion_rate: { key: 'motion_rate', label: 'PACKET RATE', min: 0.25, max: 20, step: 0.25, value: 4 },
    motion_rate_basis: { key: 'motion_rate_basis', label: 'RATE BASIS', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['NORMAL', 'HERTZ'] },
    motion_path_rate: { key: 'motion_path_rate', label: 'PATH RATE', min: 0.01, max: 80, step: 0.05, value: 1 },
    motion_step: { key: 'motion_step', label: 'LOOP STEP', min: 0, max: 1, step: 0.01, value: 0.05 },
    motion_seed: { key: 'motion_seed', label: 'SEED', min: 1, max: 9999, step: 1, value: 1, integer: true },
    motion_travel: { key: 'motion_travel', label: 'TRAVEL', min: 0, max: 1, step: 0.01, value: 1 },
    motion_jitter: { key: 'motion_jitter', label: 'JITTER', min: 0, max: 1, step: 0.01, value: 0 },
    packet_duty: { key: 'packet_duty', label: 'PACKET DUTY', min: 2, max: 98, step: 1, value: 65, integer: true },
    motor_rate: { key: 'motor_rate', label: 'MOTOR RATE', min: 0.05, max: 20, step: 0.05, value: 1 },
    motor_symmetry: { key: 'motor_symmetry', label: 'MOTOR SYM', min: 5, max: 95, step: 1, value: 50, integer: true },
    motor_shape: { key: 'motor_shape', label: 'MOTOR SHAPE', min: 0, max: 4, step: 1, value: 1, integer: true, labels: ['LINEAR', 'ROUND', 'PULSE', 'EXP', 'PLATEAU'] },
    segment_trigger: { key: 'segment_trigger', label: 'EVENT TRIGGER', min: 0, max: 2, step: 1, value: 0, integer: true, labels: ['CLOCK', 'PACKET', 'TURN'] },
    segment_overlap: { key: 'segment_overlap', label: 'OVERLAP', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['CUT', 'LAYER'] },
    segment_layers: { key: 'segment_layers', label: 'LAYERS', min: 1, max: 4, step: 1, value: 2, integer: true },
    segment_rate: { key: 'segment_rate', label: 'EVENT RATE', min: 0.25, max: 80, step: 0.25, value: 4 },
    segment_repeats: { key: 'segment_repeats', label: 'REPEATS', min: 1, max: 16, step: 1, value: 2, integer: true },
    segment_step: { key: 'segment_step', label: 'EVENT STEP', min: 0, max: 1, step: 0.01, value: 0.05 },
    segment_pitch: { key: 'segment_pitch', label: 'PITCH SCAT', min: 0, max: 12, step: 0.1, value: 0 },
    segment_level: { key: 'segment_level', label: 'LEVEL VAR', min: 0, max: 1, step: 0.01, value: 0.15 },
    segment_curve: { key: 'segment_curve', label: 'INT CURVE', min: -1, max: 1, step: 0.01, value: 0 },
    lanes_mode: { key: 'lanes_mode', label: 'MODE', min: 0, max: 2, step: 1, value: 0, integer: true, labels: ['FWD', 'REV', 'PINGPONG'] },
    lane_path: { key: 'lane_path', label: 'PATH', min: 0, max: 8, step: 1, value: 2, integer: true, labels: ['DOWN', 'UP', 'TRI', 'SINE', 'STEP DN', 'STEP UP', 'MANUAL', 'RANDOM', 'DRAW'] },
    lane_blend: { key: 'lane_blend', label: 'TRANSITION', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['XFADE', 'JUMP'] },
    lane_path_shape: { key: 'lane_path_shape', label: 'PATH SHAPE', min: 0, max: 3, step: 1, value: 0, integer: true, labels: ['LINEAR', 'SMOOTH', 'EXP', 'PLATEAU'] },
    lane_path_offset: { key: 'lane_path_offset', label: 'PATH OFFSET', min: 0, max: 1, step: 0.01, value: 0 },
    lane_path_skew: { key: 'lane_path_skew', label: 'PATH SKEW', min: 0.02, max: 0.98, step: 0.01, value: 0.5 },
    lane_path_curve: { key: 'lane_path_curve', label: 'PATH CURVE', min: -1, max: 1, step: 0.01, value: 0 },
    lane_slew: { key: 'lane_slew', label: 'JUMP SLEW', min: 0, max: 500, step: 2, value: 20, integer: true },
    lanes_stretch: { key: 'lanes_stretch', label: 'STRETCH', min: 0, max: 1, step: 1, value: 1, integer: true, labels: ['OFF', 'ON'] },
    stretch_ratio: { key: 'stretch_ratio', label: 'RATIO', min: 0.25, max: 4, step: 0.02, value: 1 },
    grain_mode: { key: 'grain_mode', label: 'MODE', min: 0, max: 3, step: 1, value: 0, integer: true, labels: ['SCAN', 'FREEZE', 'CLOUD', 'SLICE'] },
    grain_size: { key: 'grain_size', label: 'SIZE MS', min: 8, max: 1000, step: 2, value: 90, integer: true },
    grain_density: { key: 'grain_density', label: 'DENSITY', min: 1, max: 96, step: 1, value: 24, integer: true },
    grain_quality: { key: 'grain_quality', label: 'QUALITY', min: 0, max: 2, step: 1, value: 1, integer: true, labels: ['4 VOICES', '8 VOICES', '16 VOICES'] },
    grain_pitch: { key: 'grain_pitch', label: 'PITCH', min: 0.25, max: 4, step: 0.02, value: 1 },
    grain_window: { key: 'grain_window', label: 'WINDOW', min: 0, max: 5, step: 1, value: 5, integer: true, labels: ['PARZEN', 'SINE', 'HANN', 'TRI', 'GAUSS', 'BELL'] },
    grain_spread: { key: 'grain_spread', label: 'SPREAD', min: 0, max: 1, step: 0.01, value: 0 },
    grain_spray: { key: 'grain_spray', label: 'POS SPRAY', min: 0, max: 1, step: 0.01, value: 0 },
    grain_bias: { key: 'grain_bias', label: 'SPRAY BIAS', min: 0, max: 2, step: 1, value: 1, integer: true, labels: ['BEHIND', 'AROUND', 'AHEAD'] },
    grain_pitch_spray: { key: 'grain_pitch_spray', label: 'PITCH SPRAY', min: 0, max: 24, step: 0.1, value: 0 },
    grain_reverse: { key: 'grain_reverse', label: 'REVERSE %', min: 0, max: 100, step: 1, value: 0, integer: true },
    grain_size_variation: { key: 'grain_size_variation', label: 'SIZE VAR', min: 0, max: 1, step: 0.01, value: 0 },
    grain_level_variation: { key: 'grain_level_variation', label: 'LEVEL VAR', min: 0, max: 1, step: 0.01, value: 0 },
    grain_timing: { key: 'grain_timing', label: 'TIMING', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['REGULAR', 'SCATTER'] },
    grain_timing_scatter: { key: 'grain_timing_scatter', label: 'TIME SCAT', min: 0, max: 1, step: 0.01, value: 0 },
    grain_envelope_skew: { key: 'grain_envelope_skew', label: 'ENV SKEW', min: -1, max: 1, step: 0.01, value: 0 },
    grain_process: { key: 'grain_process', label: 'PROCESS', min: 0, max: 4, step: 1, value: 0, integer: true, labels: ['ORDINARY', 'STUTTER', 'SHRINK', 'DOUBLETS', 'SORTER'] },
    grain_process_amount: { key: 'grain_process_amount', label: 'PROC AMT', min: 0, max: 1, step: 0.01, value: 0.5 },
    grain_regions: { key: 'grain_regions', label: 'REGIONS', min: 1, max: 64, step: 1, value: 16, integer: true },
    grain_advance: { key: 'grain_advance', label: 'ADVANCE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['SCAN', 'PER GRAIN'] },
    long_grain_mode: { key: 'long_grain_mode', label: 'MODE', min: 0, max: 3, step: 1, value: 0, integer: true, labels: ['SCAN', 'FREEZE', 'CLOUD', 'SLICE'] },
    long_grain_length: { key: 'long_grain_length', label: 'LENGTH MS', min: 250, max: 12000, step: 10, value: 2000, integer: true },
    long_grain_layers: { key: 'long_grain_layers', label: 'LAYERS', min: 1, max: 4, step: 1, value: 2, integer: true },
    long_grain_pitch: { key: 'long_grain_pitch', label: 'PITCH', min: 0.25, max: 4, step: 0.02, value: 1 },
    long_grain_shape: { key: 'long_grain_shape', label: 'SHAPE', min: 0, max: 3, step: 1, value: 3, integer: true, labels: ['BELL', 'HANN', 'TRI', 'PLATEAU'] },
    long_grain_edge: { key: 'long_grain_edge', label: 'EDGE MS', min: 5, max: 2000, step: 5, value: 120, integer: true },
    long_grain_spread: { key: 'long_grain_spread', label: 'SPREAD', min: 0, max: 1, step: 0.01, value: 0.5 },
    long_grain_reverse: { key: 'long_grain_reverse', label: 'REVERSE %', min: 0, max: 100, step: 1, value: 0, integer: true },
    long_grain_spray: { key: 'long_grain_spray', label: 'POS SPRAY', min: 0, max: 1, step: 0.01, value: 0 },
    long_grain_bias: { key: 'long_grain_bias', label: 'SPRAY BIAS', min: 0, max: 2, step: 1, value: 1, integer: true, labels: ['BEHIND', 'AROUND', 'AHEAD'] },
    long_grain_duration_variation: { key: 'long_grain_duration_variation', label: 'DURATION VAR', min: 0, max: 1, step: 0.01, value: 0 },
    long_grain_pitch_spray: { key: 'long_grain_pitch_spray', label: 'PITCH SPRAY', min: 0, max: 24, step: 0.1, value: 0 },
    long_grain_level_variation: { key: 'long_grain_level_variation', label: 'LEVEL VAR', min: 0, max: 1, step: 0.01, value: 0 },
    long_grain_phase: { key: 'long_grain_phase', label: 'LAYER PHASE', min: 0, max: 1, step: 0.01, value: 0 },
    long_grain_advance: { key: 'long_grain_advance', label: 'ADVANCE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['SCAN', 'PER LAYER'] },
    cutups_mode: { key: 'cutups_mode', label: 'MODE', min: 0, max: 5, step: 1, value: 0, integer: true, labels: ['FWD', 'REV', 'PALIN', 'RANDOM', 'WALK', 'RCYC'] },
    cut_clock: { key: 'cut_clock', label: 'CLOCK', min: 0, max: 1, step: 1, value: 1, integer: true, labels: ['FREE', 'TEMPO'] },
    cut_division: { key: 'cut_division', label: 'DIVISION', min: 0, max: 5, step: 1, value: 3, integer: true, labels: ['1/4', '1/8', '1/8T', '1/16', '1/16T', '1/32'] },
    cut_rate: { key: 'cut_rate', label: 'FREE RATE', min: 0.25, max: 64, step: 0.25, value: 8 },
    cut_time_variation: { key: 'cut_time_variation', label: 'TIME VAR', min: 0, max: 1, step: 0.01, value: 0 },
    cut_gate: { key: 'cut_gate', label: 'CUT GATE', min: 5, max: 100, step: 1, value: 100, integer: true },
    cut_join: { key: 'cut_join', label: 'CUT JOIN MS', min: 0, max: 100, step: 1, value: 10, integer: true },
    cut_regions: { key: 'cut_regions', label: 'REGIONS', min: 1, max: 64, step: 1, value: 16, integer: true },
    cut_repeats: { key: 'cut_repeats', label: 'REPEATS', min: 1, max: 16, step: 1, value: 1, integer: true },
    cut_reverse: { key: 'cut_reverse', label: 'REVERSE %', min: 0, max: 100, step: 1, value: 0, integer: true },
    cut_pitch_variation: { key: 'cut_pitch_variation', label: 'PITCH VAR', min: 0, max: 24, step: 0.1, value: 0 },
    cut_level_variation: { key: 'cut_level_variation', label: 'LEVEL VAR', min: 0, max: 1, step: 0.01, value: 0 },
    cut_lane_order: { key: 'cut_lane_order', label: 'LANE ORDER', min: 0, max: 6, step: 1, value: 0, integer: true, labels: ['DOWN', 'UP', 'PALIN', 'RANDOM', 'PAIRS', 'OUT-IN', 'CTR-OUT'] },
    cut_poly_path: { key: 'cut_poly_path', label: 'POLY PATH', min: 0, max: 3, step: 1, value: 0, integer: true, labels: ['TOGETHER', 'STAGGER', 'ROTATE', 'RANDOM'] },
    cut_region_mode: { key: 'cut_region_mode', label: 'REGION SOURCE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['EQUAL', 'TRANSIENTS'] },
    sample_mode: { key: 'sample_mode', label: 'PLAY MODE', min: 0, max: 3, step: 1, value: 0, integer: true, labels: ['FORWARD', 'REVERSE', 'LOOP', 'PINGPONG'] },
    slice_region_mode: { key: 'slice_region_mode', label: 'SLICE SOURCE', min: 0, max: 4, step: 1, value: 1, integer: true, labels: ['EQUAL', 'TRANS', 'FILE', 'EDIT', 'AUTO'] },
    slice_regions: { key: 'slice_regions', label: 'SLICE COUNT', min: 1, max: 64, step: 1, value: 16, integer: true },
    glitch_mode: { key: 'glitch_mode', label: 'MODE', min: 0, max: 4, step: 1, value: 4, integer: true, labels: ['REPEAT', 'STUTTER', 'SCATTER', 'DIVIDE', 'OVERRIDE'] },
    glitch_clock: { key: 'glitch_clock', label: 'CLOCK', min: 0, max: 1, step: 1, value: 1, integer: true, labels: ['FREE', 'TEMPO'] },
    glitch_division: { key: 'glitch_division', label: 'DIVISION', min: 0, max: 5, step: 1, value: 3, integer: true, labels: ['1/4', '1/8', '1/8T', '1/16', '1/16T', '1/32'] },
    glitch_rate: { key: 'glitch_rate', label: 'FREE RATE', min: 0.25, max: 64, step: 0.25, value: 8 },
    glitch_slice: { key: 'glitch_slice', label: 'BUFFER MS', min: 2, max: 160, step: 1, value: 32, integer: true },
    glitch_repeats: { key: 'glitch_repeats', label: 'REPEATS', min: 1, max: 16, step: 1, value: 4, integer: true },
    glitch_chance: { key: 'glitch_chance', label: 'CHANCE %', min: 0, max: 100, step: 1, value: 100, integer: true },
    glitch_mix: { key: 'glitch_mix', label: 'OVERRIDE %', min: 0, max: 100, step: 1, value: 100, integer: true },
    glitch_steps: { key: 'glitch_steps', label: 'STEPS', min: 1, max: 16, step: 1, value: 11, integer: true },
    glitch_pulses: { key: 'glitch_pulses', label: 'PULSES', min: 0, max: 16, step: 1, value: 5, integer: true },
    glitch_rotate: { key: 'glitch_rotate', label: 'ROTATE', min: 0, max: 15, step: 1, value: 0, integer: true },
    glitch_jump: { key: 'glitch_jump', label: 'JUMP MS', min: 0, max: 160, step: 1, value: 120, integer: true },
    glitch_pitch: { key: 'glitch_pitch', label: 'PITCH RANGE', min: 0, max: 24, step: 0.1, value: 7 },
    glitch_reverse: { key: 'glitch_reverse', label: 'REVERSE %', min: 0, max: 100, step: 1, value: 25, integer: true },
    glitch_decay: { key: 'glitch_decay', label: 'DECAY %', min: 0, max: 100, step: 1, value: 90, integer: true },
    glitch_seed: { key: 'glitch_seed', label: 'SEED', min: 1, max: 9999, step: 1, value: 1, integer: true },
    waveset_group: { key: 'waveset_group', label: 'GROUP', min: 0, max: 5, step: 1, value: 3, integer: true, labels: ['1 CYCLE', '2 CYCLES', '4 CYCLES', '8 CYCLES', '16 CYCLES', '32 CYCLES'] },
    waveset_repeats: { key: 'waveset_repeats', label: 'REPEATS', min: 1, max: 16, step: 1, value: 2, integer: true },
    waveset_stride: { key: 'waveset_stride', label: 'STRIDE', min: 1, max: 16, step: 1, value: 1, integer: true },
    waveset_advance: { key: 'waveset_advance', label: 'ADVANCE', min: 0, max: 2, step: 1, value: 0, integer: true, labels: ['STRETCH', 'PRESERVE', 'HOLD'] },
    waveset_direction: { key: 'waveset_direction', label: 'ORDER', min: 0, max: 3, step: 1, value: 0, integer: true, labels: ['FORWARD', 'REVERSE', 'PENDULUM', 'SHUFFLE'] },
    waveset_process: { key: 'waveset_process', label: 'PROCESS', min: 0, max: 9, step: 1, value: 0, integer: true, labels: ['REPEAT', 'OMIT', 'REPLACE', 'ENVELOPE', 'MULTIPLY', 'INTERPOLATE', 'FRACTAL', 'HARMONIC', 'GROUP REV', 'CYCLE REV'] },
    waveset_amount: { key: 'waveset_amount', label: 'DEPTH %', min: 0, max: 100, step: 1, value: 0, integer: true },
    waveset_detail: { key: 'waveset_detail', label: 'DETAIL', min: 0, max: 4, step: 1, value: 0, integer: true, labels: ['RAW', '8 KHZ', '4 KHZ', '1 KHZ', '250 HZ'] },
    effect_reverb_algo: {"key": "effect_reverb_algo", "label": "ALGORITHM", "min": 0, "max": 1, "step": 1, "value": 0, "integer": true, "labels": ["PLATE", "ROOM"]},
    effect_reverb_decay: {"key": "effect_reverb_decay", "label": "DECAY SECONDS", "min": 0.2, "max": 20, "step": 0.01, "value": 2.5, "integer": false},
    effect_reverb_pre: {"key": "effect_reverb_pre", "label": "PREDELAY MS", "min": 0, "max": 200, "step": 1, "value": 0, "integer": true},
    effect_reverb_damp: {"key": "effect_reverb_damp", "label": "DAMPING HZ", "min": 500, "max": 18000, "step": 100, "value": 8000, "integer": true},
    effect_reverb_diff: {"key": "effect_reverb_diff", "label": "DIFFUSION %", "min": 0, "max": 100, "step": 1, "value": 70, "integer": true},
    effect_reverb_mod: {"key": "effect_reverb_mod", "label": "MODULATION %", "min": 0, "max": 100, "step": 1, "value": 10, "integer": true},
    effect_reverb_width: {"key": "effect_reverb_width", "label": "STEREO WIDTH %", "min": 0, "max": 100, "step": 1, "value": 100, "integer": true},
    effect_reverb_mix: {"key": "effect_reverb_mix", "label": "REVERB MIX %", "min": 0, "max": 100, "step": 1, "value": 0, "integer": true},
    effect_order: {"key": "effect_order", "label": "ORDER", "min": 0, "max": 719, "step": 1, "value": 0, "integer": true},
    effect_tone: { key: 'effect_tone', label: 'TONE', min: -1, max: 1, step: 0.01, value: 0 },
    effect_drive: { key: 'effect_drive', label: 'DRIVE', min: 0, max: 1, step: 0.01, value: 0 },
    effect_bits: { key: 'effect_bits', label: 'BITS', min: 0, max: 1, step: 0.01, value: 0 },
    effect_rate: { key: 'effect_rate', label: 'RATE REDUCE', min: 1, max: 16, step: 1, value: 1, integer: true },
    effect_delay: { key: 'effect_delay', label: 'DELAY MS', min: 1, max: 750, step: 1, value: 250, integer: true },
    effect_feedback: { key: 'effect_feedback', label: 'FEEDBACK %', min: 0, max: 90, step: 1, value: 35, integer: true },
    effect_width: { key: 'effect_width', label: 'WIDTH %', min: 0, max: 100, step: 1, value: 100, integer: true },
    effect_mix: { key: 'effect_mix', label: 'DELAY MIX %', min: 0, max: 100, step: 1, value: 0, integer: true },
    effect_cross_pan: { key: 'effect_cross_pan', label: 'STEREO CROSS PAN', min: -100, max: 100, step: 1, value: -100, integer: true },
    effect_rotary_rate: { key: 'effect_rotary_rate', label: 'ROTARY RATE', min: 0, max: 8, step: 0.1, value: 0.8 },
    effect_rotary_depth: { key: 'effect_rotary_depth', label: 'ROTARY DEPTH %', min: 0, max: 100, step: 1, value: 70, integer: true },
    effect_rotary_mix: { key: 'effect_rotary_mix', label: 'ROTARY MIX %', min: 0, max: 100, step: 1, value: 0, integer: true },
    effect_rotary_ramp: { key: 'effect_rotary_ramp', label: 'ROTARY RAMP', min: 50, max: 8000, step: 10, value: 350, integer: true },
    effect_rotary_balance: { key: 'effect_rotary_balance', label: 'ROTOR BALANCE', min: -100, max: 100, step: 1, value: 0, integer: true },
    effect_rotary_crossover: { key: 'effect_rotary_crossover', label: 'ROTARY CROSSOVER', min: 250, max: 2000, step: 10, value: 800, integer: true },
    effect_rotary_spread: { key: 'effect_rotary_spread', label: 'ROTARY SPREAD %', min: 0, max: 100, step: 1, value: 100, integer: true },
    effect_tape_format: { key: 'effect_tape_format', label: 'TAPE FORMAT', min: 0, max: 4, step: 1, value: 0, integer: true, labels: ['2"', '1"', '1/2"', '1/4"', '1/8"'] },
    effect_tape_drive: { key: 'effect_tape_drive', label: 'TAPE RECORD LEVEL %', min: 0, max: 100, step: 1, value: 10, integer: true },
    effect_tape_wow_rate: { key: 'effect_tape_wow_rate', label: 'WOW RATE', min: 0.05, max: 3, step: 0.01, value: 0.18 },
    effect_tape_wow_depth: { key: 'effect_tape_wow_depth', label: 'WOW %', min: 0, max: 100, step: 1, value: 4, integer: true },
    effect_tape_flutter_rate: { key: 'effect_tape_flutter_rate', label: 'FLUTTER RATE', min: 3, max: 18, step: 0.1, value: 7 },
    effect_tape_flutter_depth: { key: 'effect_tape_flutter_depth', label: 'FLUTTER %', min: 0, max: 100, step: 1, value: 3, integer: true },
    effect_tape_hiss: { key: 'effect_tape_hiss', label: 'HISS DBFS', min: -84, max: -54, step: 1, value: -72, integer: true },
    effect_tape_mix: { key: 'effect_tape_mix', label: 'TAPE MIX %', min: 0, max: 100, step: 1, value: 0, integer: true },
    effect_tape_age: { key: 'effect_tape_age', label: 'TAPE AGE %', min: 0, max: 100, step: 1, value: 0, integer: true },
    effect_tape_crinkle: { key: 'effect_tape_crinkle', label: 'TAPE CRINKLE %', min: 0, max: 100, step: 1, value: 0, integer: true },
    effect_tape_mechanics: { key: 'effect_tape_mechanics', label: 'TAPE MECHANICS %', min: 0, max: 100, step: 1, value: 5, integer: true },
    effect_tape_inertia: { key: 'effect_tape_inertia', label: 'TRANSPORT INERTIA', min: 0, max: 100, step: 1, value: 0, integer: true },
    pad_mode: { key: 'pad_mode', label: 'PAD MODE', min: 0, max: 3, step: 1, value: 0, integer: true, labels: ['KEYS', 'SLICES', 'CUES', 'ROLLS'] },
    lane_variation: { key: 'lane_variation', label: 'VARIATION', min: 0, max: 4, step: 1, value: 0, integer: true, labels: ['MANUAL', 'CYCLE', 'RANDOM', 'NO REPEAT', 'VELOCITY'] },
    preview_route: {key:'preview_route',label:'PREVIEW ROUTE',min:0,max:1,step:1,value:0,integer:true,labels:['PART','DIRECT']},
    preview_level: { key: 'preview_level', label: 'PREVIEW LEVEL', min: 0, max: 1, step: 0.01, value: 0.7 },
    lane_1_gain: { key: 'lane_1_gain', label: 'LANE 1', min: 0, max: 2, step: 0.02, value: 1 },
    lane_2_gain: { key: 'lane_2_gain', label: 'LANE 2', min: 0, max: 2, step: 0.02, value: 1 },
    lane_3_gain: { key: 'lane_3_gain', label: 'LANE 3', min: 0, max: 2, step: 0.02, value: 1 },
    lane_4_gain: { key: 'lane_4_gain', label: 'LANE 4', min: 0, max: 2, step: 0.02, value: 1 },
    pressure_mode: { key: 'pressure_mode', label: 'PRESSURE', min: 0, max: 4, step: 1, value: 1, integer: true, labels: ['OFF', 'OFFSET', 'BRAKE', 'CHAR', 'BOTH'] },
    root_note: { key: 'root_note', label: 'ROOT', min: 24, max: 96, step: 1, value: 60, integer: true, note: true },
    scale: { key: 'scale', label: 'SCALE', min: 0, max: 24, step: 1, value: 0, integer: true, labels: SCALE_LABELS },
    record_gain: { key: 'record_gain', label: 'REC GAIN', min: 0, max: 4, step: 0.05, value: 1 },
    record_source: { key: 'record_source', label: 'REC SOURCE', min: 0, max: 5, step: 1, value: 0, integer: true, labels: ['INPUT', 'MAIN MIX', 'PART A', 'PART B', 'PART C', 'PART D'] },
    monitor_level: { key: 'monitor_level', label: 'MONITOR', min: 0, max: 1, step: 0.01, value: 0 },
    clip_rate: { key: 'clip_rate', label: 'LOOP RATE', min: 0.25, max: 4, step: 0.05, value: 1 },
    clip_record_mode: { key: 'clip_record_mode', label: 'REC MODE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['REPLACE', 'OVERDUB'] },
    clip_take_undo: { key: 'clip_take_undo', label: 'UNDO TAKE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['TURN', 'UNDO'], momentary: true, result: 'TAKE UNDONE' },
    clip_timing: { key: 'clip_timing', label: 'TIMING', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['FREE', 'BEAT SYNC'] },
    launch_quantize: { key: 'launch_quantize', label: 'LAUNCH', min: 0, max: 2, step: 1, value: 0, integer: true, labels: ['OFF', 'BEAT', 'BAR'] },
    tempo_bpm: { key: 'tempo_bpm', label: 'TEMPO', min: 1, max: 999, step: 1, value: 120, integer: true },
    tempo_swing: { key: 'tempo_swing', label: 'SWING', min: 50, max: 75, step: 1, value: 50, integer: true },
    hold_release_ms: { key: 'hold_release_ms', label: 'HOLD OFF', min: 0, max: 60000, step: 50, value: 0, integer: true },
    chain_enabled: { key: 'chain_enabled', label: 'CHAIN', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['OFF', 'ON'] },
    chain_slot: { key: 'chain_slot', label: 'CHAIN SLOT', min: 1, max: 64, step: 1, value: 1, integer: true },
    chain_clip: { key: 'chain_clip', label: 'CLIP', min: 1, max: 64, step: 1, value: 1, integer: true },
    chain_repeats: { key: 'chain_repeats', label: 'REPEATS', min: 0, max: 16, step: 1, value: 1, integer: true },
    chain_bank: { key:'chain_bank',label:'BANK',min:1,max:16,step:1,value:1,integer:true },
    chain_end: { key:'chain_end',label:'END',min:0,max:2,step:1,value:0,integer:true,labels:['LOOP','STAY','STOP'] },
    chain_advance: { key:'chain_advance',label:'NEXT',min:0,max:1,step:1,value:0,integer:true,momentary:true,labels:['TURN','NEXT'] },
    chain_clear: { key: 'chain_clear', label: 'CLEAR SLOT', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['TURN', 'CLEAR'], momentary: true },
    sample_trim_start: { key: 'sample_trim_start', label: 'TRIM START', min: 0, max: 0.99, step: 0.01, value: 0 },
    sample_trim_end: { key: 'sample_trim_end', label: 'TRIM END', min: 0.01, max: 1, step: 0.01, value: 1 },
    sample_reverse: { key: 'sample_reverse', label: 'REVERSE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['TURN', 'APPLY'], momentary: true, result: 'REVERSED' },
    sample_normalize: { key: 'sample_normalize', label: 'NORMALIZE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['TURN', 'APPLY'], momentary: true, result: 'NORMALIZED' },
    sample_fade: { key: 'sample_fade', label: 'EDGE FADE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['TURN', 'APPLY'], momentary: true, result: 'FADED' },
    sample_trim_apply: { key: 'sample_trim_apply', label: 'TRIM APPLY', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['TURN', 'APPLY'], momentary: true, result: 'TRIMMED' },
};

const MAIN_ENGINE_MODES = ['motion_mode', 'lanes_mode', 'grain_mode',
    'cutups_mode', 'sample_mode', 'sample_mode', 'glitch_mode',
    'long_grain_mode', 'waveset_direction','prism_mode','morph_amplitude','texture_mode','formant_shift','stochastic_timing','pulsar_form'];
const MAIN_ENGINE_CONTROLS = MAIN_ENGINE_MODES.map((mode) => [
    'locus', 'field', 'speed', 'manual_lane',
    'lane_cycles', 'join', mode, 'output_gain',
]);
MAIN_ENGINE_CONTROLS[3][4] = 'cut_lane_advance';
for(const algorithm of [4,5])MAIN_ENGINE_CONTROLS[algorithm][4]=null;
const ENGINE_PAGE = { title: 'ENGINE', keys: ['algorithm', 'locus', 'field',
    'speed', 'manual_lane', 'lane_cycles', 'join', 'output_gain'] };
const CUTUPS_ENGINE_PAGE = { ...ENGINE_PAGE, keys: ENGINE_PAGE.keys.map((key) => key === 'lane_cycles' ? 'cut_lane_advance' : key) };
const DIRECT_ENGINE_PAGE = { ...ENGINE_PAGE, keys: ENGINE_PAGE.keys.map((key) => key === 'lane_cycles' ? null : key) };
const STACK_PATH_PAGE = { title: 'STACK PATH', keys: ['lane_path',
    'lane_blend', 'lane_path_shape', 'lane_path_offset', 'lane_path_skew',
    'lane_path_curve', 'lane_slew'] };
const PATH_EDITOR_PAGE = {title:'PATH EDITOR',pathEditor:true,keys:['path_point','path_time','path_lane','path_curve','path_count',null,null,'path_reset']};
const DIRECT_PLAY_PAGE = { title: 'DIRECT PLAY', keys: ['sample_mode',
    'lane_variation'] };
const SAMPLE_PLAY_PAGE = { title: 'DIRECT PLAY', keys: ['sample_mode',
    'lane_variation', 'lanes_stretch', 'stretch_ratio'] };
const PADS_SCALE_PAGE = { title: 'PADS & SCALE', keys: ['pad_mode',
    'root_note', 'scale', 'hold_release_ms', 'voice_mode','pressure_mode','voice_engine_mode','voice_engine_order'] };
const BANK_PADS_PAGE={title:'BANK PADS',keys:['bank_slot','bank_rate','bank_pan','bank_play','bank_choke','hold_release_ms','bank_record_source',null]};
const BANK_SAMPLE_PAGE={title:'BANK SAMPLE',global:true,scopeLabel:'B ',keys:['bank_start','bank_end','bank_master_rate','bank_reverse','bank_level','bank_attack','bank_release','bank_fade']};
const MIDI_NOTES_PAGE={title:'MIDI NOTES',global:true,keys:['midi_note_route','midi_note_channel','midi_bend_range','midi_mod_target','midi_velocity_depth','midi_sync_source','midi_sync_transport',null]};
const MIDI_MAPS_PAGE={title:'MIDI MAPS',global:true,customMaps:true,keys:Array(8).fill(null)};
function isMidiPage(page){return page===MIDI_CONTROL_PAGE||page===MIDI_NOTES_PAGE||page===MIDI_MAPS_PAGE;}
const MIDI_CONTROL_PAGE = {title:'MIDI CONTROL',global:true,keys:[
    'midi_control_mode','midi_cc_channel','midi_cc_base','midi_takeover',
    null,null,null,null]};
const VOICE_ENGINES_PAGE={title:'VOICE ENGINES',voiceEngines:true,keys:Array.from({length:8},(_,i)=>'voice_engine_'+(i+1))};
const VOICE_SETUP_PAGE={title:'VOICE SETUP',voiceSetup:true,keys:['voice_engine_edit','voice_engine_assign','voice_engine_octave','voice_engine_position','voice_engine_stack','voice_engine_pan','voice_engine_depth',null]};
const PLAY_GAIN_PAGE={title:'PLAY GAIN',global:true,scopeLabel:'',keys:['polyphony_comp']};
const AMP_ENVELOPE_PAGE = { title: 'AMP ENVELOPE', breakpoint: true,
    keys: ['amp_attack', 'amp_decay', 'amp_sustain', 'amp_release'] };
const SEQUENCE_ENVELOPE_PAGE = { title: 'SEQ AMP', sequenceEnvelope: true,
    keys: AMP_ENVELOPE_PAGE.keys };
const MOTION_SOUND_PAGE = { title: 'MOTION SOUND', keys: ['motion_sound'] };
const PACKETS_PAGE = { title: 'PACKETS', keys: ['motion_rate',
    'packet_duty'] };
const MOTOR_PAGE = { title: 'MOTOR', keys: ['motion_rate', 'packet_duty',
    'motor_rate', 'motor_symmetry', 'motor_shape'] };
const EVENT_CLOCK_PAGE = { title: 'EVENT CLOCK', keys: ['segment_trigger',
    'segment_rate', 'segment_repeats', 'segment_step'] };
const EVENT_SHAPE_PAGE = { title: 'EVENT SHAPE', keys: ['segment_overlap',
    'segment_layers', 'segment_pitch', 'segment_level', 'segment_curve',
    'motor_symmetry', 'motor_shape', 'packet_duty'] };
const PLAYBACK_PAGES = [
    [
        STACK_PATH_PAGE,
        { title: 'MOTION PATH', keys: ['motion_mode', 'motion_travel',
            'motion_jitter', 'motion_rate_basis', 'motion_path_rate',
            'motion_step', 'motion_seed'] },
        MOTION_SOUND_PAGE,
    ],
    [
        STACK_PATH_PAGE,
        { title: 'LANES PLAY', keys: ['lanes_mode', 'lanes_stretch',
            'stretch_ratio'] },
    ],
    [
        STACK_PATH_PAGE,
        { title: 'GRAINS MAIN', keys: ['grain_mode', 'grain_size',
            'grain_density', 'grain_pitch', 'grain_window', 'grain_spread'] },
        { title: 'GRAINS CLOUD', keys: ['grain_spray', 'grain_bias',
            'grain_pitch_spray', 'grain_reverse', 'grain_size_variation',
            'grain_level_variation', 'grain_timing', 'grain_timing_scatter'] },
        { title: 'GRAINS PROCESS', keys: ['grain_process',
            'grain_process_amount', 'grain_envelope_skew', 'grain_regions',
            'grain_advance', 'grain_quality'] },
    ],
    [
        { title: 'CUTUPS CLOCK', keys: ['cutups_mode', 'cut_clock',
            'cut_division', 'cut_rate', 'cut_time_variation', 'cut_gate',
            'cut_join', 'cut_region_mode'] },
        { title: 'CUTUPS PATH', keys: ['cut_repeats', 'cut_reverse',
            'cut_pitch_variation', 'cut_level_variation', 'cut_lane_order',
            'cut_poly_path', 'cut_regions'] },
    ],
    [SAMPLE_PLAY_PAGE],
    [
        DIRECT_PLAY_PAGE,
        { title: 'SLICE SOURCE', sliceEditor: true, keys: ['slice_region_mode', 'slice_regions'] },
    ],
    [
        STACK_PATH_PAGE,
        { title: 'GLITCH CLOCK', keys: ['glitch_mode', 'glitch_clock',
            'glitch_division', 'glitch_rate', 'glitch_slice',
            'glitch_repeats', 'glitch_chance', 'glitch_mix'] },
        { title: 'GLITCH PATTERN', keys: ['glitch_steps', 'glitch_pulses',
            'glitch_rotate', 'glitch_jump', 'glitch_pitch',
            'glitch_reverse', 'glitch_decay', 'glitch_seed'] },
    ],
    [
        STACK_PATH_PAGE,
        { title: 'WEAVE', keys: ['long_grain_mode',
            'long_grain_length', 'long_grain_layers', 'long_grain_pitch',
            'long_grain_shape', 'long_grain_edge', 'long_grain_spread',
            'long_grain_reverse'] },
        { title: 'WEAVE MOTION', keys: ['long_grain_spray',
            'long_grain_bias', 'long_grain_duration_variation',
            'long_grain_pitch_spray', 'long_grain_level_variation',
            'long_grain_phase', 'long_grain_advance'] },
    ],
    [
        STACK_PATH_PAGE,
        { title: 'WAVESETS', keys: ['waveset_group', 'waveset_repeats',
            'waveset_stride', 'waveset_advance', 'waveset_direction',
            'waveset_process', 'waveset_amount', 'waveset_detail'] },
    ],
    [STACK_PATH_PAGE,{title:'PRISM',keys:['prism_mode','prism_blur','prism_decay','prism_pitch','prism_tilt','prism_smear']}],
    [STACK_PATH_PAGE,{title:'MORPH',keys:['morph_amplitude','morph_frequency','morph_offset','morph_blur','morph_pitch','morph_mode']}],
    [STACK_PATH_PAGE,{title:'TEXTURE',keys:['texture_mode','texture_rate','texture_length','texture_scatter','texture_range','texture_group','texture_spread','texture_seed']}],
    [STACK_PATH_PAGE,{title:'FORMANT',keys:['formant_pitch','formant_shift','formant_smoothing','formant_tune','formant_freeze','formant_keytrack','formant_rough','formant_sub']} ],
    [STACK_PATH_PAGE,{title:'STOCHASTIC',stochastic:true,keys:["stochastic_size","stochastic_amplitude","stochastic_timing","stochastic_evolution","stochastic_memory","stochastic_pull","stochastic_law","stochastic_drift"]}],
    [STACK_PATH_PAGE,{title:'PULSAR',pulsar:true,keys:["pulsar_rate","pulsar_form","pulsar_size","pulsar_window","pulsar_skew","pulsar_omit","pulsar_burst","pulsar_jitter"]},{title:'PULSAR MOD',pulsar:true,keys:["pulsar_emit_mod_rate","pulsar_emit_mod_depth","pulsar_form_mod_rate","pulsar_form_mod_depth","pulsar_spray","pulsar_form_variation","pulsar_reverse","pulsar_keytrack"]}]

];

function playbackPagesForAlgorithm(algorithm, motionSound) {
    const pages = [...(PLAYBACK_PAGES[algorithm] || PLAYBACK_PAGES[0])];
    if(algorithm===13 && typeof PARAMS!=='undefined' && PARAMS.stochastic_law.value>=2.5) return pages.map(p=>p.stochastic?{...p,keys:p.keys.map(k=>k==='stochastic_pull'?'stochastic_capture':k)}:p);
    if (algorithm !== 0) return pages;
    if (motionSound === 1) pages.push(PACKETS_PAGE);
    else if (motionSound === 2) pages.push(MOTOR_PAGE);
    else if (motionSound >= 3) pages.push(EVENT_CLOCK_PAGE, EVENT_SHAPE_PAGE);
    return pages;
}
const QUAD_SEND_PAGE={title:'FX SENDS',global:true,keys:['split_delay_a','split_reverb_a','split_delay_b','split_reverb_b','split_delay_c','split_reverb_c','split_delay_d','split_reverb_d']};
const QUAD_SETUP_PAGE={title:'QUAD SETUP',global:true,keys:['split_mode','split_focus','split_copy','part_mute']};
const SPLIT_SETUP_PAGE = {title:'DUAL SETUP',global:true,keys:['split_mode','split_focus','split_copy','part_mute']};
const SPLIT_SEND_KEYS = ['split_delay_a','split_reverb_a','split_delay_b','split_reverb_b'];
const SPLIT_MIX_PAGE = {title:'FX SENDS',global:true,keys:[...SPLIT_SEND_KEYS,'split_priority','split_duck_threshold','split_curve','split_blend']};
const PRIORITY_PAGE = {title:'PRIORITY DUCK',global:true,keys:['split_duck_attack','split_duck_hold','split_duck_release','split_duck_hpf','split_duck_knee','split_duck_route']};
const PART_COLOR_PAGE = {title:'PART COLOR',keys:['effect_tone','effect_drive','effect_bits','effect_rate']};
const DELAY_RETURN_PAGE = {title:'DELAY RETURN',global:true,keys:['effect_delay','effect_feedback','effect_width']};
const MIX_REC_PAGE = { title: 'MIX / REC', keys: ['lane_1_gain', 'lane_2_gain',
    'lane_3_gain', 'lane_4_gain', 'record_source', 'record_gain',
    'monitor_level', 'preview_level'] };
const EFFECTS_PAGE = { title: 'EFFECTS', keys: ['effect_tone', 'effect_drive',
    'effect_bits', 'effect_rate', 'effect_delay', 'effect_feedback',
    'effect_width', 'effect_mix'] };
const STEREO_ROTARY_PAGE = { title: 'STEREO / ROTARY', keys: [
    'effect_cross_pan', 'effect_rotary_rate', 'effect_rotary_depth',
    'effect_rotary_spread', 'effect_rotary_ramp', 'effect_rotary_balance',
    'effect_rotary_crossover', 'effect_rotary_mix'] };
const TAPE_PAGE = { title: 'TAPE', keys: [
    'effect_tape_format', 'effect_tape_drive', 'effect_tape_age',
    'effect_tape_crinkle', 'effect_tape_mechanics',
    'effect_tape_inertia', 'effect_tape_hiss', 'effect_tape_mix'] };
const REVERB_PAGE = { title: 'REVERB', keys: [
    'effect_reverb_algo', 'effect_reverb_decay', 'effect_reverb_pre',
    'effect_reverb_damp', 'effect_reverb_diff', 'effect_reverb_mod',
    'effect_reverb_width', 'effect_reverb_mix'] };
const FX_ORDER_PAGE = { title: 'FX ORDER', route: true, global: true, keys: Array(8).fill(null) };
const FX_BLOCK_NAMES = ['Tone / Drive', 'Bits / Rate', 'Delay', 'Stereo / Rotary', 'Tape', 'Reverb'];
const FX_ROUTE_PRESETS = [[0,1,2,3,4,5], [0,1,5,2,3,4], [5,1,0,2,3,4]];
const FX_ROUTE_NAMES = ['DFLT','ECHO','CRSH'];
const SAMPLE_PAGE = { title: 'SAMPLE EDIT', keys: ['sample_trim_start',
    'sample_trim_end', 'sample_trim_apply', 'sample_reverse',
    'sample_normalize', 'sample_fade'] };
// Keep the full current/total page count visible in Move's 21-character
// title row. The manual continues to use the unabbreviated page name.
const LOOP_PAGE = { title: 'PERF LOOP', global:true,scopeLabel:'',keys: ['clip_rate',
    'clip_timing', 'launch_quantize', 'tempo_bpm', 'clip_record_mode',
    'tempo_swing'] };
const PATTERN_FADE_PAGE = { title: 'PATTERN FADE', patternFade: true, global: true,scopeLabel:'', keys: Array(8).fill(null) };
const FADE_CURVES=['LIN','IN','OUT','SMTH','PWR'];
const MAX_PATTERN_FADE_MS=30000;
const FADE_TIMES=[0,25,50,75,100,150,200,250,300,400,500,650,800,1000,1250,1500,1750,2000,2500,3000,3500,4000,4500,5000,5500,6000,6500,7000,7500,8000,8500,9000,9500,10000,...Array.from({length:20},(_,i)=>11000+i*1000)];
const TAKE_PAGE = {global:true,scopeLabel:'', title: 'OVERDUB TAKES', keys: [null, 'clip_take_undo'] };
const CHAIN_FADE_PAGE = {global:true,scopeLabel:'',title:'CHAIN FADE',chainFade:'chain',keys:Array(8).fill(null)};
const CHAIN_TOOLS_PAGE={title:'CHAIN TOOLS',chainTools:true,global:true,scopeLabel:'',keys:Array(8).fill(null)};
const ENTRY_FADE_PAGE = {global:true,scopeLabel:'',title:'ENTRY FADE',chainFade:'entry',keys:Array(8).fill(null)};
const CHAIN_PAGE = {global:true,scopeLabel:'', title: 'CHAIN', keys: ['chain_enabled', 'chain_slot',
    'chain_clip', 'chain_repeats', 'chain_clear', 'chain_bank', 'chain_end', 'chain_advance'] };

const LOCKABLE_KEYS = [
    'locus', 'field', 'speed', 'manual_lane', 'lane_cycles', 'algorithm',
    'join', 'output_gain', 'motion_mode', 'motion_sound', 'motion_rate',
    'lanes_mode', 'lane_path', 'lanes_stretch', 'stretch_ratio',
    'lane_1_gain', 'lane_2_gain', 'lane_3_gain', 'lane_4_gain',
    'grain_mode', 'grain_size', 'grain_density', 'grain_pitch',
    'grain_window', 'grain_spread', 'cutups_mode',
    'motion_travel', 'motion_jitter', 'packet_duty', 'motor_rate',
    'motor_symmetry', 'motor_shape', 'lane_blend', 'lane_path_shape',
    'lane_path_offset', 'lane_path_skew', 'lane_path_curve', 'lane_slew',
    'grain_spray', 'grain_bias', 'grain_pitch_spray', 'grain_reverse',
    'grain_size_variation', 'grain_level_variation', 'grain_timing',
    'grain_timing_scatter', 'grain_envelope_skew', 'grain_process',
    'grain_process_amount', 'grain_regions', 'grain_advance', 'cut_clock',
    'cut_division', 'cut_rate', 'cut_time_variation', 'cut_gate', 'cut_join',
    'cut_regions', 'cut_repeats', 'cut_reverse', 'cut_pitch_variation',
    'cut_level_variation', 'cut_lane_order', 'cut_poly_path',
    'cut_region_mode', 'sample_mode', 'slice_region_mode', 'slice_regions',
    'motion_rate_basis', 'motion_path_rate', 'motion_step', 'motion_seed',
    'segment_trigger', 'segment_overlap', 'segment_layers', 'segment_rate',
    'segment_repeats', 'segment_step', 'segment_pitch', 'segment_level',
    'segment_curve', 'glitch_mode', 'glitch_clock', 'glitch_division',
    'glitch_rate', 'glitch_slice', 'glitch_repeats', 'glitch_chance',
    'glitch_mix', 'glitch_steps', 'glitch_pulses', 'glitch_rotate',
    'glitch_jump', 'glitch_pitch', 'glitch_reverse', 'glitch_decay',
    'glitch_seed', 'effect_tone', 'effect_drive', 'effect_bits',
    'effect_rate', 'effect_delay', 'effect_feedback', 'effect_width',
    'effect_mix', 'effect_cross_pan', 'effect_rotary_rate',
    'effect_rotary_depth', 'effect_rotary_mix', 'effect_rotary_ramp',
    'effect_rotary_balance', 'effect_rotary_crossover',
    'effect_rotary_spread', 'effect_tape_format', 'effect_tape_drive',
    'effect_tape_wow_rate', 'effect_tape_wow_depth',
    'effect_tape_flutter_rate', 'effect_tape_flutter_depth',
    'effect_tape_hiss', 'effect_tape_mix', 'effect_tape_age',
    'effect_tape_crinkle', 'effect_tape_mechanics', 'effect_tape_inertia',
    'long_grain_mode', 'long_grain_length',
    'long_grain_layers', 'long_grain_pitch', 'long_grain_shape',
    'long_grain_edge', 'long_grain_spread', 'long_grain_reverse',
    'long_grain_spray', 'long_grain_bias',
    'long_grain_duration_variation', 'long_grain_pitch_spray',
    'long_grain_level_variation', 'long_grain_phase', 'long_grain_advance',
    'amp_attack', 'amp_decay', 'amp_sustain', 'amp_release',
    'waveset_group', 'waveset_repeats', 'waveset_stride',
    'waveset_advance', 'waveset_direction', 'waveset_process',
    'waveset_amount', 'waveset_detail',
    'effect_reverb_algo', 'effect_reverb_decay', 'effect_reverb_pre', 'effect_reverb_damp', 'effect_reverb_diff', 'effect_reverb_mod', 'effect_reverb_width', 'effect_reverb_mix', 'effect_order', 'cut_lane_advance', 'split_blend','split_level_a','split_level_b','split_delay_a','split_delay_b','split_reverb_a','split_reverb_b','split_curve','split_priority','split_duck_threshold','split_duck_attack','split_duck_hold','split_duck_release','split_duck_hpf','split_duck_knee','split_duck_route',
];
const NEW_ENGINE_KEYS=["pulsar_emit_mod_rate","pulsar_emit_mod_depth","pulsar_form_mod_rate","pulsar_form_mod_depth","pulsar_spray","pulsar_form_variation","pulsar_reverse","pulsar_keytrack","pulsar_rate","pulsar_form","pulsar_size","pulsar_window","pulsar_skew","pulsar_omit","pulsar_burst","pulsar_jitter","stochastic_capture","stochastic_size","stochastic_amplitude","stochastic_timing","stochastic_evolution","stochastic_memory","stochastic_pull","stochastic_law","stochastic_drift","prism_mode", "prism_blur", "prism_decay", "prism_pitch", "prism_tilt", "prism_smear", "morph_amplitude", "morph_frequency", "morph_offset", "morph_blur", "morph_pitch", "texture_mode", "texture_rate", "texture_length", "texture_scatter", "texture_range", "texture_group", "texture_spread", "texture_seed","morph_mode","formant_pitch","formant_shift","formant_smoothing","formant_tune","formant_freeze","formant_keytrack","formant_rough","formant_sub"];
const LOCKABLE_PARAMS = new Set([...NEW_ENGINE_KEYS,...LOCKABLE_KEYS,'split_level_c','split_level_d','split_delay_c','split_delay_d','split_reverb_c','split_reverb_d']);

const SHORT_LABELS = {
    pulsar_emit_mod_rate:"ERAT",pulsar_emit_mod_depth:"EDEP",pulsar_form_mod_rate:"FRAT",pulsar_form_mod_depth:"FDEP",pulsar_spray:"SPRY",pulsar_form_variation:"FVAR",pulsar_reverse:"REV%",pulsar_keytrack:"KEYT",
    pulsar_rate:"RATE",pulsar_form:"FORM",pulsar_size:"SIZE",pulsar_window:"WIN",pulsar_skew:"SKEW",pulsar_omit:"OMIT",pulsar_burst:"BRST",pulsar_jitter:"JITT",pulsar_source_hold:"SOURCE HOLD",pulsar_hold_position:"HELD POSITION",
    stochastic_size:"SIZE",stochastic_amplitude:"AMUT",stochastic_timing:"TMUT",stochastic_evolution:"EVOL",stochastic_memory:"MEM",stochastic_pull:"PULL",stochastic_capture:"CAPT",stochastic_law:"LAW",stochastic_drift:"DRFT",
    prism_mode: "MODE",
    prism_blur: "BLUR",
    prism_decay: "DCAY",
    prism_pitch: "PIT",
    prism_tilt: "TILT",
    prism_smear: "SMER",
    formant_pitch:'PIT',formant_shift:'FORM',formant_smoothing:'SMTH',formant_tune:'TUNE',formant_freeze:'FRZE',formant_keytrack:'KEYT',formant_rough:'ROUG',formant_sub:'SUB',
    morph_mode:'MODE',
    morph_amplitude: "AMP",
    morph_frequency: "FREQ",
    morph_offset: "OFST",
    morph_blur: "BLUR",
    morph_pitch: "PIT",
    texture_mode: "MODE",
    texture_rate: "RATE",
    texture_length: "LEN",
    texture_scatter: "SCAT",
    texture_range: "RNGE",
    texture_group: "GRUP",
    texture_spread: "SPRD",
    texture_seed: "SEED",

    split_blend:'BLND',split_level_a:'ALVL',split_level_b:'BLVL',split_delay_a:'ADLY',split_delay_b:'BDLY',split_reverb_a:'AREV',split_reverb_b:'BREV',split_curve:'CURV',split_priority:'PRIO',split_duck_threshold:'THR',split_duck_attack:'ATK',split_duck_hold:'HOLD',split_duck_release:'REL',split_duck_hpf:'HPF',split_duck_knee:'KNEE',split_duck_route:'ROUT',split_copy:'COPY',split_focus:'EDIT',part_mute:'MUTE',
    effect_reverb_algo: 'ALGO', effect_reverb_decay: 'DECY', effect_reverb_pre: 'PRE', effect_reverb_damp: 'DAMP', effect_reverb_diff: 'DIFF', effect_reverb_mod: 'MOD', effect_reverb_width: 'WDTH', effect_reverb_mix: 'MIX', effect_order: 'ORDR',
    cut_lane_advance: 'ADV',
    split_delay_c:'CDLY',split_reverb_c:'CREV',split_delay_d:'DDLY',split_reverb_d:'DREV',
    locus: 'POS', field: 'FLD', speed: 'SPD', manual_lane: 'STAK',
    lane_cycles: 'RATE', algorithm: 'ALG', join: 'JOIN', output_gain: 'LEVL',
    motion_mode: 'MODE', motion_sound: 'SND', motion_rate: 'RATE',
    motion_rate_basis: 'BAS', motion_path_rate: 'PATH',
    motion_step: 'STEP', motion_seed: 'SEED',
    motion_travel: 'TRVL', motion_jitter: 'JITR', packet_duty: 'DUTY',
    motor_rate: 'MRAT', motor_symmetry: 'MSYM', motor_shape: 'MSHP',
    segment_trigger: 'TRIG', segment_overlap: 'OVLP',
    segment_layers: 'LAYR', segment_rate: 'ERAT',
    segment_repeats: 'REPT', segment_step: 'ESTP',
    segment_pitch: 'PITC', segment_level: 'LEVL', segment_curve: 'CURV',
    lanes_mode: 'MODE', lane_path: 'PATH', lanes_stretch: 'STR',
    lane_blend: 'BLND', lane_path_shape: 'SHAP', lane_path_offset: 'OFST',
    lane_path_skew: 'SKEW', lane_path_curve: 'CURV', lane_slew: 'SLEW',
    stretch_ratio: 'RATIO', grain_mode: 'MODE', grain_size: 'SIZE',
    grain_density: 'DENS', grain_pitch: 'PIT', grain_window: 'WIN',
    grain_quality: 'QUAL',
    grain_spread: 'SPRD', cutups_mode: 'MODE', lane_1_gain: 'L1',
    grain_spray: 'SPRY', grain_bias: 'BIAS', grain_pitch_spray: 'PSPR',
    grain_reverse: 'REV%', grain_size_variation: 'SVAR',
    grain_level_variation: 'LVAR', grain_timing: 'TIME',
    grain_timing_scatter: 'TSC', grain_envelope_skew: 'ESKW',
    grain_process: 'PROC', grain_process_amount: 'PAMT',
    grain_regions: 'REGN', grain_advance: 'ADVN', cut_clock: 'CLCK',
    cut_division: 'DIV', cut_rate: 'RATE', cut_time_variation: 'TVAR',
    cut_gate: 'GATE', cut_join: 'JOIN', cut_regions: 'REGN',
    cut_repeats: 'REPT', cut_reverse: 'REV%', cut_pitch_variation: 'PVAR',
    cut_level_variation: 'LVAR', cut_lane_order: 'LORD',
    cut_poly_path: 'POLY',
    cut_region_mode: 'RSRC', sample_mode: 'PLAY',
    slice_region_mode: 'SSRC', slice_regions: 'SLIC', pad_mode: 'PADS',
    lane_variation: 'VARI', preview_level: 'PREV',
    lane_2_gain: 'L2', lane_3_gain: 'L3', lane_4_gain: 'L4',
    pressure_mode: 'PRES', root_note: 'ROOT', scale: 'SCAL', record_gain: 'REC',
    record_source: 'SRC', monitor_level: 'MON', clip_rate: 'RATE',
    clip_record_mode: 'MODE', clip_timing: 'TIME', launch_quantize: 'LNCH',
    tempo_bpm: 'BPM', tempo_swing: 'SWNG', hold_release_ms: 'HOLD',
    chain_enabled: 'CHN',
    chain_slot: 'SLOT', chain_clip: 'CLIP', chain_repeats: 'REPT',
    chain_clear: 'CLR',
    sample_trim_start: 'STRT', sample_trim_end: 'END',
    sample_trim_apply: 'TRIM', sample_reverse: 'REV',
    sample_normalize: 'NORM', sample_fade: 'FADE',
    glitch_mode: 'MODE', glitch_clock: 'CLCK', glitch_division: 'DIV',
    glitch_rate: 'RATE', glitch_slice: 'BUFF', glitch_repeats: 'REPT',
    glitch_chance: 'CHNC', glitch_mix: 'OVRD', glitch_steps: 'STEP',
    glitch_pulses: 'PULS', glitch_rotate: 'ROTA', glitch_jump: 'JUMP',
    glitch_pitch: 'PITC', glitch_reverse: 'REV%', glitch_decay: 'DCAY',
    glitch_seed: 'SEED', effect_tone: 'TONE', effect_drive: 'DRIV',
    effect_bits: 'BITS', effect_rate: 'RATE', effect_delay: 'DLY',
    effect_feedback: 'FDBK', effect_width: 'WDTH', effect_mix: 'MIX',
    effect_cross_pan: 'X-PN', effect_rotary_rate: 'RATE',
    effect_rotary_depth: 'DPTH', effect_rotary_mix: 'MIX',
    effect_rotary_ramp: 'RAMP', effect_rotary_balance: 'BAL',
    effect_rotary_crossover: 'XOVR', effect_rotary_spread: 'SPRD',
    effect_tape_format: 'FMT', effect_tape_drive: 'REC',
    effect_tape_wow_rate: 'W.RT', effect_tape_wow_depth: 'WOW',
    effect_tape_flutter_rate: 'F.RT', effect_tape_flutter_depth: 'FLUT',
    effect_tape_hiss: 'HISS', effect_tape_mix: 'MIX',
    effect_tape_age: 'AGE', effect_tape_crinkle: 'CRNK',
    effect_tape_mechanics: 'MECH', effect_tape_inertia: 'INRT',
    long_grain_mode: 'MODE', long_grain_length: 'LEN',
    long_grain_layers: 'LAYR', long_grain_pitch: 'PIT',
    long_grain_shape: 'SHAP', long_grain_edge: 'EDGE',
    long_grain_spread: 'SPRD', long_grain_reverse: 'REV%',
    long_grain_spray: 'SPRY', long_grain_bias: 'BIAS',
    long_grain_duration_variation: 'DVAR', long_grain_pitch_spray: 'PSPR',
    long_grain_level_variation: 'LVAR', long_grain_phase: 'PHAS',
    long_grain_advance: 'ADVN',
    waveset_group: 'GRUP', waveset_repeats: 'REPT',
    waveset_stride: 'STRD', waveset_advance: 'ADVN',
    waveset_direction: 'ORDR', waveset_process: 'PROC',
    waveset_amount: 'DPTH', waveset_detail: 'DTAL',
    amp_attack: 'ATK', amp_decay: 'DEC', amp_sustain: 'SUS',
    amp_release: 'REL',
};

const state = {
    ticks: 0,
    splitEnabled:false,sharedDual:false,quadMode:false,modeSelection:0,lastNonSingleMode:1,splitFocus:0,splitBlend:.5,splitParts:[{},{}],splitPages:[0,0,0,0],
    pathReady:false,pathCount:4,pathPoint:0,pathPoints:Array.from({length:8},(_,i)=>({time:[0,.25,.5,.75,.8,.85,.9,.95][i],lane:[0,.5,1,.5,.4,.3,.2,.1][i],curve:0})),
    focusButtonHeld:false,focusButtonUsed:false,captureStartTick:0,performanceVisible:false,performanceArmed:false,performanceOwner:0,performancePads:new Set(),performancePadLive:new Set(),performanceValues:Array(8).fill(0),voiceAlgorithms:Array(8).fill(-1),
    editorReady:false,engineListReturn:false,engineListVoices:false,
    fxMenuSelection:0,fxMenuReturn:false,
    pageSelection:0,pageReturn:null,pageDestinationReturn:null,pageTier:'hub',pageCategory:0,
    mixPeaks:[0,0],mixVoices:[0,0],mixReasons:[5,5],
    saveStatus:0,savedTicks:0,
    lane: 1,
    mask: 0,
    recording: false,
    meter: 0,
    position: 0,
    fieldStart: 0,
    fieldEnd: 1,
    weights: [0, 0, 0, 0],
    pulsarVoices:Array(8).fill(null),
    playheads: Array(8).fill(-1),
    wavesetVoices: Array(8).fill(null),
    cutupsVoices: Array(8).fill(null),
    spectralPending:0,
    wavesetAnalysisPending: 0,
    stackPositions: Array(8).fill(-1),
    voicePads: Array(8).fill(-1),
    voiceNotes: Array(8).fill(-1),
    rollPads: Array(8).fill(-1),
    rollNotes: Array(8).fill(-1),
    voiceCount: 0,
    limited: false,
    tenths: 0,
    scanning: false,
    waves: Array.from({ length: 4 }, () =>
        Array.from({ length: WAVE_BINS }, () => [0, 0])),
    octave: 0,
    algorithm: 0,
    recordSource: 0,
    copyShortcut: null,
    shiftHeld: false,
    deleteHeld: false,
    view: 'main',
    settingsPage: 0,
    persistenceEnabled: false,
    recalled: false,
    message: '',
    messageTicks: 0,
    actionMessage: '',
    actionMessageTicks: 0,
    iconPressed: new Set(),
    ledPending: new Map(),
    ledSent: new Map(),
    ledOrder: 0,
    activePads: new Set(),
    clipRecording: false,
    clipPlaying: false,
    stackScanPaused: false,
    recordedStackHold: false,
    glitchBufferLocked: false,
    glitchLockLatched: false,
    glitchLockMomentary: false,
    glitchLockChord: false,
    glitchLockLastTapTick: -1000,
    clipOverdub: false,
    clipPosition: 0,
    clipDuration: 0,
    clipNoteCount: 0,
    clipPointCount: 0,
    clipRate: 1,
    clipLaunchPending: false,
    clipTiming: 0,
    clipRecordedTempo: 120,
    clipBeats: 0,
    clipNoteCapacity: 1024,
    clipPointCapacity: 8192,
    clipNoteFull: false,
    clipPointFull: false,
    clipTakeCount: 0,
    clipTakeCanUndo: false,
    clipRecordingTake: 0,
    activeClip: 0,
    fxOrder: [0,1,2,3,4,5], fxSavedOrder: 0, fxRouteClip: -1, fxRoutePending: false, fxRouteTransition: 0,
    fxRouteSelected: 0, fxRouteGrabbed: false,
    queuedClip: -1,
    clipBank: 0,
    clipMaskLow: 0,
    clipMaskHigh: 0,
    libraryLane: 1,
    libraryDir: LIBRARY_HOME,
    capturesDirectory: '',
    libraryItems: [],
    libraryIndex: 0,
    libraryScroll: 0,
    laneSource: 'EMPTY',
    laneSourceState: 'EMPTY:EMPTY',
    patternFaderTouched: false,
    patternFade: {active:false,held:false,from:-1,to:-1,position:0,time:0,curve:0,underflows:0,mode:0,low:2000,high:12000,duration:0,override:false},
    clipPrepareState: 0,
    chainBank:0,chainPage:0,chainEnd:0,chainAdvance:false,
    chainPlayBank:0,chainLaunchBank:-1,chainLaunchSlot:-1,
    chainFades:[0,0,2000,12000,0,0,0,2000,12000,0],
    chainEnabled: false,
    chainPosition: 0,
    chainRepeat: 0,
    chainSlot: 0,
    chainQueuedSlot: -1,
    chainEntries: Array.from({ length: 64 }, () => ({
        enabled: false, clip: 0, repeats: 1,
    })),
    undoAvailable: false,
    maximumRenderMicros: 0,
    renderOverruns: 0,
    controlPeakMicros: 0, queryPeakMicros: 0, workPeakMicros: 0, workOverruns: 0,
    limiterReductionDb: 0,
    limiterMaximumReductionDb: 0,
    helpPage: 0,
    renderCpuAvailable:false, renderCpuPeakMicros:0, renderWaitPeakMicros:0, renderCpuOverruns:0,
    sampleLoadState: 0,
    sampleLoadProgress: 0,
    sampleLoadName: '',
    regions: [], regionLane:0, waveText:null,
    waveKeys:Array(4).fill(''),laneRegions:Array(4).fill(null),
    padMode: 0,
    scale: 0,
    cues: Array(8).fill(-1),
};

function parseRegionState(text) {
    state.regionLane=state.lane;
    const values = String(text || '').split(',').map((item) => Number(item));
    const count = values.length > 0 && Number.isFinite(values[0])
        ? clamp(values[0] | 0, 0, 64) : 0;
    state.regions = [];
    for (let index = 1; index < values.length && index <= count + 1; index++) {
        if (Number.isFinite(values[index])) {
            state.regions.push(clamp(values[index] / 1000, 0, 1));
        }
    }
    state.laneRegions[state.lane-1]={source:state.waveKeys[state.lane-1],regions:state.regions};
}

function parseCueState(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    let changed = false;
    for (let index = 0; index < state.cues.length; index++) {
        const position = Number.isFinite(values[index]) && values[index] >= 0
            ? clamp(values[index] / 1000, 0, 1) : -1;
        changed = changed || position !== state.cues[index];
        state.cues[index] = position;
    }
    if (changed) rebuildLedQueue();
}

function clamp(value, low, high) {
    return Math.max(low, Math.min(high, value));
}

function longGrainLengthIndex(value) {
    const length = clamp(value, 250, 12000);
    if (length <= 1000) return Math.round((length - 250) / 10);
    if (length <= 4000) return 75 + Math.round((length - 1000) / 50);
    return 135 + Math.round((length - 4000) / 100);
}

function longGrainLengthAt(index) {
    const detent = clamp(Math.round(index), 0, 215);
    if (detent <= 75) return 250 + detent * 10;
    if (detent <= 135) return 1000 + (detent - 75) * 50;
    return 4000 + (detent - 135) * 100;
}

function rotaryRampAt(index) {
    const detent = clamp(Math.round(index), 0, 99);
    if (detent <= 45) return 50 + detent * 10;
    if (detent <= 75) return 500 + (detent - 45) * 50;
    return 2000 + (detent - 75) * 250;
}

function rotaryRampIndex(value) {
    const target = clamp(value, 50, 8000);
    if (target <= 500) return Math.round((target - 50) / 10);
    if (target <= 2000) return 45 + Math.round((target - 500) / 50);
    return 75 + Math.round((target - 2000) / 250);
}

function rotaryCrossoverAt(index) {
    const detent = clamp(Math.round(index), 0, 65);
    if (detent <= 25) return 250 + detent * 10;
    if (detent <= 45) return 500 + (detent - 25) * 25;
    return 1000 + (detent - 45) * 50;
}

function rotaryCrossoverIndex(value) {
    const target = clamp(value, 250, 2000);
    if (target <= 500) return Math.round((target - 250) / 10);
    if (target <= 1000) return 25 + Math.round((target - 500) / 25);
    return 45 + Math.round((target - 1000) / 50);
}

function tapeInertiaSeconds(value) {
    const amount = clamp(value, 0, 100);
    return amount <= 0 ? 0 : 0.05 * Math.pow(80, amount / 100);
}

function logarithmicControlIndex(control) {
    const value = clamp(control.value, control.min, control.max);
    return Math.round(Math.log(value / control.min)
        / Math.log(control.max / control.min) * 96);
}

function logarithmicControlAt(control, index) {
    const detent = clamp(Math.round(index), 0, 96);
    return control.min * Math.pow(control.max / control.min, detent / 96);
}

function stochasticEvolutionAt(index) {
    const i=clamp(Math.round(index),0,193);return i===0?0:Math.min(1024,.25*Math.pow(2,(i-1)/16));
}
function stochasticEvolutionIndex(value) {
    return value<=0?0:clamp(1+Math.round(Math.log2(Math.max(.25,value)/.25)*16),1,193);
}

function holdReleaseAt(index) {
    const detent = clamp(Math.round(index), 0, 67);
    if (detent === 0) return 0;
    if (detent <= 20) return detent * 50;
    if (detent <= 36) return 1000 + (detent - 20) * 250;
    if (detent <= 61) return 5000 + (detent - 36) * 1000;
    return 30000 + (detent - 61) * 5000;
}

function holdReleaseIndex(value) {
    const target = clamp(value, 0, 60000);
    let nearest = 0;
    let distance = Math.abs(target - holdReleaseAt(0));
    for (let index = 1; index <= 67; index++) {
        const candidate = Math.abs(target - holdReleaseAt(index));
        if (candidate < distance) {
            nearest = index;
            distance = candidate;
        }
    }
    return nearest;
}

const ENVELOPE_TIMES = [
    0, 1, 2, 3, 5, 8, 10, 15, 20, 30, 40, 50, 75, 100, 150, 200,
    300, 400, 500, 750, 1000, 1500, 2000, 3000, 4000, 6000, 8000,
    10000, 15000, 20000,
];

function envelopeTimeIndex(value, maximum) {
    const target = clamp(value, 0, maximum);
    let nearest = 0;
    let distance = Math.abs(target - ENVELOPE_TIMES[0]);
    for (let index = 1; index < ENVELOPE_TIMES.length
        && ENVELOPE_TIMES[index] <= maximum; index++) {
        const candidate = Math.abs(target - ENVELOPE_TIMES[index]);
        if (candidate < distance) {
            nearest = index;
            distance = candidate;
        }
    }
    return nearest;
}

function envelopeTimeAt(index, maximum) {
    let last = 0;
    for (let candidate = 1; candidate < ENVELOPE_TIMES.length; candidate++) {
        if (ENVELOPE_TIMES[candidate] > maximum) break;
        last = candidate;
    }
    return ENVELOPE_TIMES[clamp(Math.round(index), 0, last)];
}

function decodeDelta(value) {
    const v = value | 0;
    if (v === 0 || v === 64) return 0;
    return v < 64 ? v : v - 128;
}

function resetParameterEncoderMotion(keepIndex = -1) {
    for (const [index, motion] of parameterEncoderMotion.entries()) {
        if (index === keepIndex) continue;
        motion.key = '';
        motion.remainder = 0;
    }
}

function parameterEncoderDelta(index, key, delta) {
    const motion = parameterEncoderMotion[index];
    if (!motion || delta === 0) return 0;
    if (motion.key !== key) {
        motion.key = key;
        motion.remainder = 0;
    }
    const response = key === 'chain_bank' ? CHAIN_BANK_ENCODER_RESPONSE : PARAMETER_ENCODER_RESPONSE;
    const scaled = motion.remainder + delta * response;
    const magnitude = Math.floor(Math.abs(scaled) + 0.5);
    const applied = scaled < 0 ? -magnitude : magnitude;
    motion.remainder = scaled - applied;
    if (Math.abs(motion.remainder) < 1e-9) motion.remainder = 0;
    // BANK is a selector: consume acceleration without jumping several banks
    // or retaining a backlog of whole steps for later messages.
    return key === 'chain_bank' ? clamp(applied, -1, 1) : applied;
}

function getParam(key, fallback) {
    try {
        const value = host_module_get_param(key);
        return value == null ? fallback : value;
    } catch (error) {
        return fallback;
    }
}

function getNumber(key, fallback) {
    const raw = getParam(key, fallback);
    if (raw === '' || raw == null) return fallback;
    const value = Number(raw);
    return Number.isFinite(value) ? value : fallback;
}

function setParam(key, value) {
    try {
        host_module_set_param(key, String(value));
    } catch (error) {}
}

function setParamBlocking(key, value) {
    try {
        if (typeof host_module_set_param_blocking === 'function') {
            host_module_set_param_blocking(key, String(value), 180);
        } else {
            host_module_set_param(key, String(value));
        }
    } catch (error) {}
}

function refreshEditorSnapshot() {
    const previousPage=state.view==='settings'?parameterPages()[state.settingsPage]:null;
    const keepPage=previousPage&&(previousPage.scopeLabel===''||previousPage.patternFade||previousPage.chainFade||previousPage.chainTools||previousPage===CHAIN_PAGE)?previousPage:null;
    state.editorReady=false;
    let snapshot;
    try { snapshot=JSON.parse(String(getParam('ui_editor_state',''))); }
    catch (_) { return false; }
    const values=snapshot?.values,split=snapshot?.split;
    if(snapshot?.v!==1 || !values || typeof values!=='object'
        || !split || ![0,1].includes(split.enabled) || !Number.isInteger(split.focus)||split.focus<0||split.focus>(split.enabled&&split.quad?3:1)
        || !Array.isArray(split.parts) || split.parts.length!==(split.enabled&&split.quad?4:2)
        || split.parts.some((part)=>!part || typeof part!=='object')) return false;
    // Validate the complete response before replacing any displayed controls.
    for(const [key,control] of Object.entries(PARAMS))
        if(!control.momentary && !key.startsWith('layers_') && !key.startsWith('seq_') && !Number.isFinite(values[key])) return false;
    for(const key of ['pad_octave','record_lane','stack_scan_paused','glitch_buffer_lock'])
        if(!Number.isFinite(values[key])) return false;
    for(const [key,control] of Object.entries(PARAMS))
        if(!key.startsWith('layers_') && !key.startsWith('seq_')) control.value=control.momentary?0:clamp(values[key],control.min,control.max);
    parseSplitState(split,false);
    state.engineListVoices=Number(values.voice_engine_active)===1;
    state.algorithm=Math.round(PARAMS.algorithm.value);
    state.padMode=Math.round(PARAMS.pad_mode.value);state.scale=Math.round(PARAMS.scale.value);
    state.octave=clamp(values.pad_octave,-3,3);
    state.lane=clamp(Math.round(values.record_lane),1,4);
    state.recordSource=Math.round(PARAMS.record_source.value);
    state.stackScanPaused=values.stack_scan_paused!==0;
    state.glitchBufferLocked=values.glitch_buffer_lock!==0;
    state.voiceAlgorithms=String(snapshot.voices||'').split(',').map(Number);
    state.editorReady=true;
    if(Number(values.split_mode)===5||layersUi.active)readLayers();if(Number(values.split_mode)===6||sequenceUi.active)readSequence();
    if(keepPage){const index=parameterPages().findIndex(page=>page.title===keepPage.title);if(index>=0)state.settingsPage=index;}
    if(state.message==='UPDATING EDITOR')state.messageTicks=0;
    return true;
}

function showMessage(text, ticks) {
    state.message = String(text || '');
    state.messageTicks = ticks || 45;
}

function showActionMessage(text, ticks) {
    state.actionMessage = String(text || '');
    state.actionMessageTicks = ticks || 55;
}

function parseUiState(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    if (values.length < 14) return;
    const oldLane = state.lane;
    const oldMask = state.mask;
    const oldRecording = state.recording;
    state.lane = clamp(values[0] | 0, 1, 4);
    state.mask = values[1] | 0;
    state.recording = values[2] !== 0;
    state.meter = clamp(values[3] / 1000, 0, 1);
    state.position = clamp(values[4] / 1000, 0, 1);
    state.fieldStart = clamp(Number.isFinite(values[17])?values[17]:values[5]/1000,0,1);
    state.fieldEnd = clamp(Number.isFinite(values[18])?values[18]:values[6]/1000,0,1);
    state.weights = [7, 8, 9, 10].map((index) => clamp(values[index] / 1000, 0, 1));
    state.limited = values[11] !== 0;
    state.tenths = Math.max(0, values[12] | 0);
    state.scanning = values[13] !== 0;
    if (values.length >= 16) {
        state.persistenceEnabled = values[14] !== 0;
        state.recalled = values[15] !== 0;
    }
    if (values.length >= 17) state.voiceCount = Math.max(0, values[16] | 0);
    if (oldLane !== state.lane || oldMask !== state.mask
        || oldRecording !== state.recording) rebuildLedQueue();
}


// Layers uses the existing Thru renderer, encoder response and page browser.
const SEQUENCE_KEYS=['seq_length','seq_rate','seq_rotation','seq_active','seq_mute','seq_velocity','seq_gate','seq_scale','seq_step','seq_child','seq_pos','seq_note','seq_vel','seq_trig_gate','seq_micro','seq_prob','seq_burst','seq_when','seq_clip_beats','seq_duration_mode','seq_time_ms','seq_trig_time_ms','seq_pan_1','seq_pan_2','seq_pan_3','seq_pan_4','seq_eq_low','seq_eq_mid','seq_eq_high','seq_eq_freq','seq_eq_solo','seq_duck_key','seq_duck_depth','seq_duck_threshold','seq_duck_attack','seq_duck_hold','seq_duck_release','seq_duck_hpf','seq_duck_knee','seq_cycle_hit','seq_cycle_every'];
const sequenceUi={active:false,data:null,pad:-1,downAt:0,held:false,copy:false,copyReady:false};
const SEQUENCE_LANE_PAGE={title:'SEQ LANE',sequence:true,keys:['seq_length','seq_rate','seq_rotation','seq_active','seq_velocity','seq_gate','tempo_bpm','seq_clip_beats']};
const SEQUENCE_TRIG_PAGE={title:'TRIG',sequence:true,sequenceTrig:true,keys:['seq_pos','seq_note','seq_vel','seq_trig_gate','seq_micro','seq_prob','seq_cycle_hit','seq_cycle_every']};
const SEQUENCE_BURST_PAGE={title:'BURST',sequence:true,sequenceBurst:true,keys:['seq_pos','seq_note','seq_vel','seq_trig_gate','seq_burst','seq_child','seq_when',null]};
const SEQUENCE_PITCH_PAGE={title:'SEQ SETUP',sequence:true,keys:['root_note','scale','seq_scale','seq_duration_mode',null,null,null,null]};
const SEQUENCE_MIX_PAGE={title:'SEQ MIX',sequenceMix:true,global:true,keys:['split_level_a','split_level_b','split_level_c','split_level_d','seq_pan_1','seq_pan_2','seq_pan_3','seq_pan_4']};
const SEQUENCE_STRIP_KEYS=['seq_eq_low','seq_eq_mid','seq_eq_high','seq_eq_freq','seq_eq_solo',null,null,null];
const SEQUENCE_STRIP_PAGES=Array.from({length:4},(_,lane)=>({title:'SEQ STRIP '+(lane+1),sequenceMix:true,sequenceStrip:lane,global:true,keys:SEQUENCE_STRIP_KEYS}));
const SEQUENCE_DUCK_PAGE={title:'SEQ DUCK',sequenceMix:true,global:true,keys:['seq_duck_key','seq_duck_depth','seq_duck_threshold','seq_duck_attack','seq_duck_hold','seq_duck_release','seq_duck_hpf','seq_duck_knee']};
const sequenceSpecs={
 seq_length:['LEN',1,32,1,16,true],seq_rate:['RATE',0,8,1,4,true,['1/1','1/2','1/4','1/8','1/16','1/16T','1/32','1/32T','1/64']],seq_rotation:['ROT',0,31,1,0,true],seq_active:['ON',0,1,1,1,true,['OFF','ON']],seq_mute:['MUTE',0,1,1,0,true,['OFF','ON']],
 seq_velocity:['VEL',1,127,1,100,true],seq_gate:['DUR',.01,32,.01,.8,false],seq_scale:['ENTRY',0,1,1,0,true,['CHROM','SCALE']],seq_step:['STEP',1,32,1,1,true],seq_child:['EVENT',0,8,1,0,true],
 seq_pos:['POS',0,1,.005,0,false],seq_note:['NOTE',-48,48,1,0,true],seq_vel:['VEL',1,127,1,100,true],seq_trig_gate:['DUR',.01,32,.01,.8,false],seq_micro:['MICRO',-.5,.5,.01,0,false],seq_prob:['PROB',0,1,.01,1,false],seq_cycle_hit:['HIT',1,8,1,1,true],seq_cycle_every:['EVERY',1,8,1,1,true],seq_burst:['BURST',1,8,1,1,true],seq_when:['WHEN',0,.999,.01,0,false],seq_clip_beats:['CLIP',1,64,1,4,true],seq_clear:['CLEAR',0,4,1,4,true],seq_delete:['DEL',0,1,1,1,true],
 seq_duration_mode:['UNIT',0,1,1,0,true,['STEP','TIME']],seq_time_ms:['DUR',10,30000,1,1000,false],seq_trig_time_ms:['DUR',10,30000,1,1000,false],
 seq_pan_1:['PAN1',-1,1,.01,0,false],seq_pan_2:['PAN2',-1,1,.01,0,false],seq_pan_3:['PAN3',-1,1,.01,0,false],seq_pan_4:['PAN4',-1,1,.01,0,false],
 seq_eq_low:['LOW',-12,12,.25,0,false],seq_eq_mid:['MID',-12,12,.25,0,false],seq_eq_high:['HIGH',-12,12,.25,0,false],seq_eq_freq:['FREQ',120,8000,1,900,false],seq_eq_solo:['SOLO',0,1,1,0,true,['OFF','ON']],
 seq_duck_key:['KEY',0,4,1,0,true,['OFF','L1','L2','L3','L4']],seq_duck_depth:['DEPTH',0,24,.5,0,false],seq_duck_threshold:['THR',-60,0,.5,-24,false],seq_duck_attack:['ATK',1,250,1,12,false],seq_duck_hold:['HOLD',0,1000,1,40,false],seq_duck_release:['REL',10,3000,1,220,false],seq_duck_hpf:['HPF',20,2000,1,100,false],seq_duck_knee:['KNEE',0,24,.5,6,false]
};
for(const [key,[label,min,max,step,value,integer,labels]] of Object.entries(sequenceSpecs))PARAMS[key]={key,label,shortLabel:label,min,max,step,value,integer,...(labels?{labels}:{}),...(['seq_clear','seq_delete'].includes(key)?{momentary:true}:{})};
function readSequence(){
 let data;try{data=JSON.parse(String(getParam('seq_state','{}')));}catch(_){return;}
 if(!data||![0,1].includes(data.active))return;
 const previous=sequenceUi.data;sequenceUi.active=Boolean(data.active);sequenceUi.data=data;
 if(Number.isInteger(data.clipboard))sequenceUi.copyReady=sequenceUi.copyReady||Boolean(data.clipboard);
 if(Array.isArray(data.values))SEQUENCE_KEYS.forEach((key,i)=>{if(Number.isFinite(data.values[i]))PARAMS[key].value=data.values[i];});
 const timed=PARAMS.seq_duration_mode.value>=.5;
 SEQUENCE_LANE_PAGE.keys[5]=timed?'seq_time_ms':'seq_gate';
 SEQUENCE_TRIG_PAGE.keys[3]=timed?'seq_trig_time_ms':'seq_trig_gate';
 if(previous?.active!==data.active||previous?.held!==data.held||previous?.edit!==data.edit||JSON.stringify(previous?.lanes)!==JSON.stringify(data.lanes)||JSON.stringify(previous?.steps)!==JSON.stringify(data.steps))rebuildLedQueue();
}
function sequencePages(base){
 const pages=base.filter(page=>!page.voiceEngines&&!page.voiceSetup&&page!==AMP_ENVELOPE_PAGE&&page.title!==STACK_PATH_PAGE.title&&page.title!=='PADS & SCALE'&&page!==QUAD_SETUP_PAGE&&page!==SPLIT_MIX_PAGE&&page!==PRIORITY_PAGE);
 return [SEQUENCE_LANE_PAGE,SEQUENCE_TRIG_PAGE,SEQUENCE_PITCH_PAGE,SEQUENCE_BURST_PAGE,SEQUENCE_ENVELOPE_PAGE,SEQUENCE_MIX_PAGE,...SEQUENCE_STRIP_PAGES,SEQUENCE_DUCK_PAGE,...pages];
}
function sequencePadColor(pad){
 const d=sequenceUi.data;if(!sequenceUi.active||!d)return null;
 const step=(3-Math.floor(pad/8))*8+pad%8;if(step>=d.length)return Black;
 if(sequenceUi.pad===pad&&sequenceUi.held)return White;
 if(d.playing&&d.lanes?.[d.edit]?.[0]===step)return NeonGreen;
 return d.steps?.[step]?.[0]?Cyan:DeepRed;
}
function sequencePadGesture(pad,pressed){
 const step=(3-Math.floor(pad/8))*8+pad%8;
 if(pressed&&(state.shiftHeld||sequenceUi.copy)){
   if(state.shiftHeld&&!sequenceUi.copyReady){showMessage('COPY TRIG FIRST');return;}
   const copied=!state.shiftHeld;
   setParamBlocking('seq_consume',step);setParamBlocking('seq_step',step+1);
   setParamBlocking(state.shiftHeld?'seq_paste':'seq_copy',1);
   showMessage(state.shiftHeld?'PASTED TRIG':'COPIED TRIG');readSequence();
   if(copied)sequenceUi.copyReady=true;
   rebuildLedQueue();return;
 }
 if(pressed){
   sequenceUi.pad=pad;sequenceUi.downAt=Date.now();sequenceUi.held=false;
 }else if(sequenceUi.pad===pad){
   if(sequenceUi.held){setParamBlocking('seq_hold',-1);state.view='main';resetParameterEncoderMotion();}
   sequenceUi.pad=-1;sequenceUi.held=false;
 }
 readSequence();rebuildLedQueue();
}
function tickSequenceGesture(){
 if(sequenceUi.active&&sequenceUi.pad>=0&&!sequenceUi.held&&Date.now()-sequenceUi.downAt>=300){
   const step=(3-Math.floor(sequenceUi.pad/8))*8+sequenceUi.pad%8;
   setParamBlocking('seq_hold',step);sequenceUi.held=true;readSequence();
   state.view='settings';state.settingsPage=parameterPages().indexOf(SEQUENCE_TRIG_PAGE);resetParameterEncoderMotion();
 }
}
function sequenceMarkers(center=18,overview=false){
 const d=sequenceUi.data;if(!d)return;
 for(let i=0;i<d.length;++i)if(d.steps?.[i]?.[0]){const x=4+Math.round(clamp(d.steps[i][1],0,1)*119);drawRect(x,center+4,1,3,1);}
 const current=overview&&d.playing?d.lanes?.[d.edit]?.[0]:d.step;
 const position=overview?d.steps?.[current]?.[1]??PARAMS.seq_pos.value:PARAMS.seq_pos.value;
 const x=4+Math.round(clamp(position,0,1)*119);
 if(overview){
   // A short top pointer is the current step's start; only the reader below
   // is a moving playhead. Keep both visible when a sample loops quickly.
   drawRect(x-2,center-8,5,1,1);drawRect(x-1,center-7,3,1,1);
   drawRect(x,center-6,1,2,1);return;
 }
 drawRect(x-1,center-7,3,15,0);drawRect(x,center-7,1,15,1);drawRect(x-2,center-8,5,1,1);
}
function sequenceHeads(){
 const lane=sequenceUi.data?.edit;
 return (sequenceUi.data?.heads||[]).filter(h=>Array.isArray(h)&&h.length>=5
   &&h[0]===lane&&Number.isFinite(h[1])&&Number.isFinite(h[2])
   &&Number.isFinite(h[3])&&h[3]>0).sort((a,b)=>b[4]-a[4]).slice(0,2);
}
function drawSequenceHeads(heads){
 for(const [index,head] of heads.entries()){
   const start=waveformX(clamp(head[1],0,1),[0,1],4,120);
   const cursor=waveformX(clamp(head[2],0,1),[0,1],4,120);
   if(start<0||cursor<0)continue;
   // A fixed underline shows where this voice started. A connecting span
   // looks like reverse travel after a short loop wraps to the left.
   if(index)drawRect(start,27,1,1,1);
   else {drawRect(start-2,27,5,1,1);drawRect(start,25,1,3,1);}
   if(index)drawDashedPlayhead(cursor,11,14);
   else drawRect(cursor,11,1,14,1);
 }
}
function drawSequence(){
 clearDisplay();const d=sequenceUi.data;if(!d){print(1,0,'SEQUENCE',1);return;}
 const edit=d.edit+1,rec=state.recording?d.rec+1:edit;
 drawTitleWithFraction('EDIT '+edit+' REC '+rec,d.playing?'PLAY':'STOP');drawRect(0,8,128,1,1);
 drawWaveformLane(d.edit,[0,1],18,4,120);sequenceMarkers(18,true);
 const heads=sequenceHeads();drawSequenceHeads(heads);
 for(let lane=0;lane<4;++lane){const l=d.lanes[lane]||[-1,16,4,1,0],x=lane*32;
   if(lane===d.edit)drawRect(x,29,30,9,1);
   print(x+2,29,String(lane+1)+(l[4]?' M':l[3]?'':' OFF'),lane===d.edit?0:1);
   drawRect(x+2,40,27,1,1);if(l[0]>=0)drawRect(x+2+Math.round(l[0]*25/Math.max(1,l[1]-1)),39,2,3,1);
   print(x+1,46,(l[0]>=0?String(l[0]+1).padStart(2,'0'):'--')+'/'+String(l[1]).padStart(2,'0'),1);
 }
 const duration=heads.length?sequenceTimeText(heads[0][3]):PARAMS.seq_duration_mode.value>=.5
   ?sequenceTimeText(PARAMS.seq_time_ms.value):sequenceDurationText(PARAMS.seq_gate.value);
 print(1,57,'BPM '+Math.round(PARAMS.tempo_bpm.value)+'  DUR '+duration,1);
}
function sequenceNoteName(offset){const n=clamp((sequenceUi.data?.root||60)+Math.round(offset),0,127);return ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'][n%12]+(Math.floor(n/12)-1);}
function sequenceTimeText(ms){
 if(ms<1)return '<1ms';
 if(ms<10)return ms.toFixed(1)+'ms';
 if(ms<1000)return Math.round(ms)+'ms';
 if(ms<10000)return (ms/1000).toFixed(1)+'s';
 return Math.round(ms/1000)+'s';
}
function sequenceStepMs(){
 const stepMs=Number(sequenceUi.data?.step_ms);
 const beats=[4,2,1,.5,.25,1/6,.125,1/12,.0625][clamp(Math.round(PARAMS.seq_rate.value),0,8)];
 return Number.isFinite(stepMs)&&stepMs>0?stepMs:beats*60000/(Math.max(1,PARAMS.tempo_bpm.value)*clamp(PARAMS.clip_rate.value,.25,4));
}
function sequenceDurationText(value){return sequenceTimeText(Math.max(.25,value*sequenceStepMs()));}
function sequenceAmpDurationMs(){
 const locked=Boolean((sequenceUi.data?.mask||0)&8);
 if(PARAMS.seq_duration_mode.value>=.5)return clamp(locked?PARAMS.seq_trig_time_ms.value:PARAMS.seq_time_ms.value,10,30000);
 return Math.max(.25,(locked?PARAMS.seq_trig_gate.value:PARAMS.seq_gate.value)*sequenceStepMs());
}
function sequenceAmpTiming(){
 const dur=sequenceAmpDurationMs();
 const raw=[Math.max(0,PARAMS.amp_attack.value),Math.max(0,PARAMS.amp_decay.value),Math.max(3,PARAMS.amp_release.value)];
 const scale=Math.min(1,dur/(raw[0]+raw[1]+raw[2]+1e-9));
 const sustain=clamp(PARAMS.amp_sustain.value/100,0,1);
 const release=raw[2]*scale;
 const attack=Math.min(raw[0]*scale,Math.max(0,dur-release));
 // DSP uses a 1 ms safety decay when Sustain sits below the attack peak.
 const decay=Math.min(Math.max(0,dur-release-attack),Math.max(raw[1]*scale,sustain<.999999?1:0));
 return {dur,raw,scale,applied:[attack,decay,release],sustain};
}
function sequenceValue(control){
 const v=control.value,key=control.key;
 if(key==='seq_note')return sequenceNoteName(v);
 if(key==='seq_child')return v===0?'TRIG':String(Math.round(v));
 if(key==='seq_gate'||key==='seq_trig_gate')return sequenceDurationText(v);
 if(key==='seq_time_ms'||key==='seq_trig_time_ms'||['seq_duck_attack','seq_duck_hold','seq_duck_release'].includes(key))return sequenceTimeText(v);
 if(/^seq_pan_[1-4]$/.test(key))return Math.abs(v)<.005?'C':(v<0?'L':'R')+Math.round(Math.abs(v)*100);
 if(['seq_eq_low','seq_eq_mid','seq_eq_high'].includes(key))return (v>0?'+':'')+Number(v.toFixed(1));
 if(['seq_duck_depth','seq_duck_threshold','seq_duck_knee'].includes(key))return (v>0?'+':'')+Number(v.toFixed(1))+'dB';
 if(key==='seq_eq_freq')return String(Math.round(v));
 if(key==='seq_duck_hpf')return Math.round(v)+'Hz';
 if(['seq_pos','seq_prob','seq_when'].includes(key))return Math.round(v*100)+'%';
 if(key==='seq_micro')return (v>0?'+':'')+Math.round(v*100)+'%';
 if(key==='seq_clear'||key==='seq_delete')return 'APPLY';
 return control.labels?control.labels[Math.round(v)]:String(Math.round(v));
}
function drawSequenceTrig(){
 clearDisplay();const page=currentSettingsPage(),d=sequenceUi.data;
 drawSettingsTitle('L'+((d?.edit||0)+1)+' T'+String((d?.step||0)+1).padStart(2,'0')+(d?.child?' E'+d.child:'')+' '+page.title);
 drawRect(0,8,128,1,1);drawWaveformLane(d?.edit||0,[0,1],18,4,120);sequenceMarkers();
 for(let i=0;i<8;++i){const control=controlForKey(page.keys[i]);const x=(i%4)*32,y=i<4?31:47;if(!control){drawRect(x+14,y+5,4,4,1);drawRect(x+15,y+6,2,2,0);continue;}
   const field=['seq_pos','seq_note','seq_vel','seq_trig_gate'].indexOf(control.key);
   const locked=(control.key==='seq_trig_time_ms'?((d?.mask||0)&8):field>=0&&((d?.mask||0)&(1<<field)));
   print(x+1,y,control.shortLabel+(locked?'*':''),1);print(x+1,y+7,sequenceValue(control),1);
 }
}
function drawSequenceBurst(){
 clearDisplay();const page=currentSettingsPage(),d=sequenceUi.data;
 const event=clamp(Math.round(PARAMS.seq_child.value),0,8);
 drawSettingsTitle('L'+((d?.edit||0)+1)+' T'+String((d?.step||0)+1).padStart(2,'0')+' BURST');
 drawRect(0,8,128,1,1);
 drawRect(5,19,118,1,1);drawRect(5,17,1,5,1);drawRect(122,17,1,5,1);
 const positions=Array.isArray(d?.burst)&&d.burst.length?d.burst:Array.from({length:Math.max(1,Math.round(PARAMS.seq_burst.value))},(_,i)=>i/Math.max(1,Math.round(PARAMS.seq_burst.value)));
 positions.forEach((when,index)=>{
   const x=5+Math.round(clamp(when,0,.999)*113),selected=event===index+1;
   // Each small wave is one sample retrigger on the step-time ruler.
   const height=[1,2,4,2,5,2,3,1];
   for(let i=0;i<height.length;++i)drawRect(x-3+i,14-Math.floor(height[i]/2),1,height[i],1);
   drawRect(x,18,1,4,1);
   if(selected){drawRect(x-5,10,12,1,1);drawRect(x-5,10,1,10,1);drawRect(x+6,10,1,10,1);}
 });
 for(let i=0;i<8;++i){const control=controlForKey(page.keys[i]),x=(i%4)*32,y=i<4?27:45;if(!control){drawRect(x+14,y+5,4,4,1);drawRect(x+15,y+6,2,2,0);continue;}
   const field=['seq_pos','seq_note','seq_vel','seq_trig_gate'].indexOf(control.key);
   const locked=control.key==='seq_trig_time_ms'?((d?.mask||0)&8):field>=0&&((d?.mask||0)&(1<<field));
   print(x+1,y,control.shortLabel+(locked?'*':''),1);
   print(x+1,y+7,control.key==='seq_when'&&!event?'—':sequenceValue(control),1);
 }
}

const layersUi={data:null,active:false,wave:false,returnView:null,serial:-1,dismissed:-1,editCorner:-1,holdPad:-1,holdTime:0,holdFired:false,tapPad:-1,tapTime:-1000,focus:0,pages:['','FX CORNER']};
const LAYERS_WAVE_LEFT=12,LAYERS_WAVE_TOP=10,LAYERS_WAVE_WIDTH=51;
const layersDisplay={valid:false,header:null,gutter:null,fx:null,footer:null,notice:null,
    waves:null,mask:-1,window:null,base:new Uint32Array(LAYERS_WAVE_WIDTH*2),
    frame:new Uint32Array(LAYERS_WAVE_WIDTH*2),painted:new Uint32Array(LAYERS_WAVE_WIDTH*2)};
let layersRasterTarget=null;

const LAYERS_FX_NAMES=['DRY','FILT','BLUR','DLY','DFLD','SPRY','FRAC','SHRD','BUF','RVRB','TAPE','PITC','ROTR','RRUN','SHUT','TMBL','SPOL'];
const LAYERS_FX_SHORT=['DRY','FLT','BLR','DLY','DFL','SPR','FRC','SHR','BUF','RVB','TAP','PIT','ROT','RRN','SHT','TMB','SPL'];
const LAYERS_FX_TITLES=['Dry','Filter','Blur','Delay','Delay Field','Spectral Spray','Fracture','Shred','Buffer','Reverb','Tape','Pitch/Detune','Rotary','Rerun','Shutter','Tumble','Spool'];
const LAYERS_FX_GROUPS=[{name:'Direct',ids:[0]},{name:'Tone / Color',ids:[1,10,6,7]},{name:'Pitch / Motion',ids:[11,12]},{name:'Space',ids:[3,4,9]},{name:'Spectral',ids:[2,5]},{name:'Rhythm / Buffer',ids:[8,13,14,15,16]}];
const LAYERS_FX_ORDER=LAYERS_FX_GROUPS.flatMap(group=>group.ids);
const layersPicker={tier:0,group:0,selection:0,corner:0};
function openLayersPicker(){
    if(!layersUi.active){if(state.view==='layerFxPicker')state.view='main';return;}
    const algorithm=Math.round(PARAMS.layers_fx0.value);
    layersPicker.tier=0;layersPicker.group=Math.max(0,LAYERS_FX_GROUPS.findIndex(group=>group.ids.includes(algorithm)));
    layersPicker.selection=Math.max(0,LAYERS_FX_GROUPS[layersPicker.group].ids.indexOf(algorithm));
    layersPicker.corner=Math.round(PARAMS.layers_corner.value);state.view='layerFxPicker';resetParameterEncoderMotion();draw();
}
function closeLayersPicker(){
    state.view='settings';state.settingsPage=Math.max(0,parameterPages().findIndex(page=>page.title==='FX CORNER'));
    rememberLayersPage();resetParameterEncoderMotion();
}
function handleLayersPicker(number,value){
    if(state.view!=='layerFxPicker')return false;
    if((number===MoveBack&&state.shiftHeld)||(number===MoveMenu&&(state.shiftHeld||state.focusButtonHeld)))return false;
    if(number===MoveBack||number===MoveMenu){
        if(value>0){if(number===MoveMenu&&state.shiftHeld)return false;if(layersPicker.tier){layersPicker.tier=0;}else closeLayersPicker();}
        return true;
    }
    if(number===MoveMainKnob||number===MoveUp||number===MoveDown){
        if(number!==MoveMainKnob&&value<=0)return true;
        const delta=number===MoveMainKnob?decodeDelta(value):number===MoveUp?-1:1;
        if(layersPicker.tier)layersPicker.selection=clamp(layersPicker.selection+delta,0,LAYERS_FX_GROUPS[layersPicker.group].ids.length-1);
        else {layersPicker.group=clamp(layersPicker.group+delta,0,LAYERS_FX_GROUPS.length-1);layersPicker.selection=0;}
        return true;
    }
    if(number===MoveLeft||number===MoveRight){
        if(value>0){layersPicker.group=clamp(layersPicker.group+(number===MoveLeft?-1:1),0,LAYERS_FX_GROUPS.length-1);layersPicker.selection=0;}
        return true;
    }
    if(number===MoveMainButton){
        if(value>0){
            if(!layersPicker.tier)layersPicker.tier=1;
            else {
                const algorithm=LAYERS_FX_GROUPS[layersPicker.group].ids[layersPicker.selection];
                setParamBlocking('layers_fx0',algorithm);readLayers();closeLayersPicker();showMessage(LAYERS_FX_TITLES[algorithm].toUpperCase());
            }
        }
        return true;
    }
    return false;
}
function drawLayersPicker(){
    clearDisplay();
    const group=LAYERS_FX_GROUPS[layersPicker.group];
    const labels=layersPicker.tier?group.ids.map(id=>LAYERS_FX_TITLES[id]):LAYERS_FX_GROUPS.map(g=>g.name);
    const selected=layersPicker.tier?layersPicker.selection:layersPicker.group;
    drawTitleWithFraction('C'+(layersPicker.corner+1)+' '+(layersPicker.tier?'CHOOSE FX':'FX FAMILY'),(selected+1)+'/'+labels.length);
    fill_rect(0,8,128,1,1);
    const top=clamp(selected-3,0,Math.max(0,labels.length-5));
    for(let row=0;row<5&&top+row<labels.length;++row){const index=top+row,y=12+row*9;
        if(index===selected)fill_rect(0,y-1,128,8,1);
        print(2,y,labels[index],index===selected?0:1);
    }
    print(1,56,layersPicker.tier?'APPLY / BACK':'OPEN / BACK',1);
}

const LAYERS_SETUP_PAGE={title:'LAYERS',layersSetup:true,keys:['layers_corner','layers_glide','layers_z_mode','layers_return','layers_fx_x','layers_fx_y',null,'layers_depth']};
const LAYERS_TOOLS_PAGE={title:'FX TOOLS',layerFx:true,keys:['layers_corner',null,null,null,'layers_copy','layers_paste',null,null]};
const LAYERS_FX_PAGE={title:'FX CORNER',layerFx:true,keys:Array.from({length:8},(_,i)=>'layers_fx'+i)};
const LAYERS_FX_DETAIL_PAGE={title:'FX DETAIL',layerFx:true,layerFxDetail:true,keys:['layers_fx8','layers_fx9','layers_fx10','layers_fx11',null,null,null,null]};
const LAYERS_MAIN_KEYS=['locus','field','speed','manual_lane','layers_fx_x','layers_fx_y','layers_glide','layers_depth'];
Object.assign(PARAMS,{
    layers_corner:{key:'layers_corner',label:'EDIT CORNER',shortLabel:'CORN',min:0,max:3,step:1,integer:true,labels:['1 TL','2 TR','3 BL','4 BR'],value:0},
    layers_glide:{key:'layers_glide',label:'XY GLIDE',shortLabel:'GLID',min:5,max:1500,step:5,value:120,format:v=>v>=1000?(v/1000).toFixed(1)+'s':Math.round(v)+'ms'},
    layers_depth:{key:'layers_depth',label:'FX DEPTH',shortLabel:'MIX',min:0,max:1,step:.01,value:1,format:v=>Math.round(v*100)+'%'},
    layers_fx_x:{key:'layers_fx_x',label:'FX X',shortLabel:'FX X',min:0,max:1,step:.01,value:0,format:v=>Math.round(v*100)+'%'},
    layers_fx_y:{key:'layers_fx_y',label:'FX Y',shortLabel:'FX Y',min:0,max:1,step:.01,value:0,format:v=>Math.round(v*100)+'%'},
    layers_z_mode:{key:'layers_z_mode',label:'FX PRESSURE',shortLabel:'Z',min:0,max:1,step:1,integer:true,labels:['DEPTH','CHAR'],value:0},
    layers_return:{key:'layers_return',label:'AFTER RELEASE',shortLabel:'RLS',min:0,max:1,step:1,integer:true,labels:['STAY','DRY'],value:0},
});
for(const key of ['layers_copy','layers_paste'])PARAMS[key]={key,label:key.slice(7).toUpperCase(),shortLabel:key==='layers_paste'?'PAST':key.slice(7).toUpperCase(),min:0,max:1,step:1,value:0,momentary:true};
for(let i=0;i<12;++i)PARAMS['layers_fx'+i]={key:'layers_fx'+i,label:['FX ALGORITHM','CONTROL 1','CONTROL 2','CONTROL 3','CONTROL 4','Z TARGET','Z AMOUNT','OUTPUT TRIM'][i],shortLabel:['FX','P1','P2','P3','P4','Z TO','ZAMT','LVL'][i],min:i===6?-1:0,max:i===0?16:i===5?9:i===7?2:1,step:i===0||i===5?1:.01,integer:i===0||i===5,value:i===7?1:0,...(i===0?{labels:LAYERS_FX_NAMES}:i===5?{labels:['OFF','P1','P2','P3','P4','P5','P6','P7','P8','MACRO']}:{format:v=>Math.round(v*100)+'%'})};
function layersFxControl(key) {
    const control=PARAMS[key],i=Number(key.slice(9)),alg=Math.round(PARAMS.layers_fx0.value);
    if(!control)return control;
    if(i>=8)return layersDetailControl(control,i-8,alg);
    if(i===0)return {...control,values:LAYERS_FX_ORDER};
    if(i===5&&(alg===1||alg===11))return {...control,value:control.value>4&&control.value!==9?0:control.value,values:[0,1,2,3,4,9]};
    if(i<1||i>4)return control;
    const labels=[[],['CUT','RES','DRIV','RESP'],['SMEA','PERS','TONE','WIDE'],['TIME','FDBK','TONE','PING'],
        ['TIME','SPRD','FDBK','MOVE'],['SPRY','DRFT','HOLD','DMG'],['PROC','AMNT','COLR','BIAS'],
        ['CIRC','DRIV','FDBK','COLR'],['SIZE','RATE','REPT','MOVE'],['TYPE','DCAY','DAMP','PRE'],['FMT','REC','AGE','MECH'],['PITC','FINE','WIND','SPRD'],['RATE','DEPT','RAMP','SPRD'],['SIZE','HOLD','GATE','PITC'],['RATE','GATE','SHAP','SWNG'],['SPAN','PATT','REPT','REVR'],['TIME','DPTH','REVR','RCVR']];
    const result={...control,shortLabel:labels[alg]?.[i-1],label:labels[alg]?.[i-1]};if(!result.label)return null;
    const choice=(names)=>Object.assign(result,{step:1/(names.length-1),choiceCount:names.length,format:v=>names[clamp(Math.round(v*(names.length-1)),0,names.length-1)]});
    const time=v=>v>=1000?(v/1000).toFixed(1)+'s':Math.round(v)+'ms';
    const hz=v=>v>=1000?(v/1000).toFixed(1)+'k':Math.round(v)+'Hz';
    if(alg===1&&i===1)result.format=v=>hz(25*Math.pow(720,v));
    if(alg===1&&i===4)choice(['LP','BP','HP']);
    if(alg===3&&i===1)result.format=v=>time(20*Math.pow(100*Math.pow(2,Math.round(PARAMS.layers_fx8.value*2)),v));
    if(alg===3&&i===2)result.format=v=>Math.round(v*90)+'%';
    if(alg===2&&i===4)result.format=v=>(v*2).toFixed(1)+'x';
    if(alg===4&&i===1)result.format=v=>time(20*Math.pow(25*Math.pow(2,Math.round(PARAMS.layers_fx8.value*2)),v));
    if(alg===4&&i===3||alg===7&&i===3)result.format=v=>Math.round(v*85)+'%';
    if(alg===5&&i===3)result.format=v=>v>=.995?'FRZ':Math.round(v*100)+'%';
    if(alg===6) {
        const mode=clamp(Math.round(PARAMS.layers_fx1.value*9),0,9);
        const contextual=[['THRS','CHAT','ASYM'],['BITS','RATE','DITH'],['DPTH','LENG','DIR'],['DPTH','THRS','BAL'],['DPTH','RECV','SKEW'],['MIX','VOWL','SHFT'],['DPTH','CARR','CHAR'],['MIX','FILT','TRAK'],['MIX','TONE','SHAP'],['MIX','TONE','BAL']];
        if(i===1)choice(['RELAY','CRUSH','SPLC','LOGIC','VOID','THRT','ROBOT','OCT-','OCT+','OCTS']);
        else {result.shortLabel=contextual[mode][i-2];result.label=result.shortLabel;if(i===4)result.format=v=>(v>.5?'+':'')+Math.round((v*2-1)*100)+'%';
            if(mode===1&&i===2)result.format=v=>(16-14*v*v).toFixed(1)+'b';
            if(mode===1&&i===3)result.format=v=>hz(44100/(44100/420+(Math.max(1,44100/18000)-44100/420)*v*v));
            if(mode===2&&i===3)result.format=v=>time(Math.pow(80,v*v));
            if(mode===6&&i===3)result.format=v=>hz(28*Math.pow(40,v));
        }
    }
    if(alg===7&&i===1)choice(['SHRED','WOOL','RAT','ZONEA','ZONEB','FUZI','FUZII','DIODE']);
    if(alg===8&&i===1)result.format=v=>time(40*Math.pow(25,v));
    if(alg===8&&i===2)result.format=v=>(.25*Math.pow(16,v)).toFixed(2)+'x';
    if(alg===9&&i===1)choice(['PLATE','ROOM']);
    if(alg===9&&i===2)result.format=v=>(.2*Math.pow(100,v)).toFixed(1)+'s';
    if(alg===9&&i===3)result.format=v=>hz(500*Math.pow(36,v));
    if(alg===9&&i===4)result.format=v=>time(v*200);
    if(alg===10&&i===1)choice(['2"','1"','1/2"','1/4"','1/8"']);
    if(alg===11&&i===1){result.step=1/48;result.choiceCount=49;result.format=v=>{const n=Math.round(v*48)-24;return (n>0?'+':'')+n+'st';};}
    if(alg===11&&i===2){result.step=.005;result.format=v=>{const n=Math.round(v*200)-100;return (n>0?'+':'')+n+'c';};}
    if(alg===11&&i===3)result.format=v=>time(20*Math.pow(9,v));
    if(alg===11&&i===4)result.format=v=>Math.round(v*100)+'ct';
    if(alg===12&&i===1)result.format=v=>(8*v*v).toFixed(2);
    if(alg===12&&i===3)result.format=v=>time(50*Math.pow(160,v));
    if(alg===12&&i===4)result.format=v=>(v*2).toFixed(1)+'x';
    if(alg>=13){
        const sync=PARAMS.layers_fx8.value>=.5;
        if(i===1&&sync)choice(['1/128','1/64','1/32','1/16','1/8','1/4','1/2','1/1']);
        else if(i===1){
            if(alg===14)result.format=v=>(.5*Math.pow(160,v)).toFixed(1);
            else result.format=v=>time((alg===13?20:alg===15?80:50)*Math.pow(alg===13?100:alg===15?25:80,v));
        }
        if(alg===13&&i===2)choice(['LIVE','HOLD']);
        if(alg===13&&i===4){result.step=1/48;result.choiceCount=49;result.format=v=>{const n=Math.round(v*48)-24;return(n>0?'+':'')+n+'st';};}
        if(alg===15&&i===2)choice(['FWD','BACK','PING','SWAP','ROTA','SKIP','RAND','JUMP']);
        if(alg===15&&i===3){result.choiceCount=16;result.step=1/15;result.format=v=>String(Math.round(v*15))+'x';}
        if(alg===16&&i===4)result.format=v=>time(50*Math.pow(80,v));
        if(alg===14&&i===4)result.format=v=>Math.round(v*60)+'%';
    }
    return result;
}
function layersDetailControl(control,index,algorithm){
    const labels={2:['SOFT','DAMP','TRNS','LINK'],3:['RANG','HPF','DRIV','DUCK'],4:['RANG','TONE','WIDE','DUCK'],5:['SOFT','DAMP','TRNS','LINK'],6:['HEAD','HPF','DAMP','DYN'],7:['HEAD','HPF','DAMP','DYN'],8:['XFD','GLID','GRID','WIDE'],9:['DIFF','MOD','WIDE','HPF'],10:['WOW','FLUT','TONE','WIDE'],12:['XOVR','BAL','DOPL','TONE'],13:['SYNC','SWNG','REVR','SEAM'],14:['SYNC','PATT','PAN','SEED'],15:['SYNC','SLCS','SEED','SEAM'],16:['SYNC','CURV','REPT','SEAM']};
    const label=labels[algorithm]?.[index];if(!label)return null;
    const result={...control,label,shortLabel:label};
    const hz=v=>v>=1000?(v/1000).toFixed(1)+'k':Math.round(v)+'Hz';
    if((algorithm===2||algorithm===5)&&index===1)result.format=v=>hz(1200*Math.pow(15,v));
    if((algorithm===3||algorithm===4)&&index===0){const names=algorithm===3?['2s','4s','8s']:['0.5s','1s','2s'];result.choiceCount=3;result.step=.5;result.format=v=>names[clamp(Math.round(v*2),0,2)];}
    if(algorithm===3&&index===1||algorithm===9&&index===3)result.format=v=>hz(20*Math.pow(50,v));
    if((algorithm===6||algorithm===7)&&index===0)result.format=v=>(v*18).toFixed(1)+'dB';
    if((algorithm===6||algorithm===7)&&index===1)result.format=v=>hz(20*Math.pow(30,v));
    if((algorithm===6||algorithm===7)&&index===2||algorithm===4&&index===1)result.format=v=>hz(1200*Math.pow(15,v));
    if(algorithm===4&&index===2||algorithm===10&&index>=2)result.format=v=>index===2&&algorithm===10?(v>.5?'+':'')+Math.round((v-.5)*200)+'%':(v*2).toFixed(1)+'x';
    if(algorithm===8&&index===0)result.format=v=>Math.round((.01+.44*v)*100)+'%';
    if(algorithm===8&&index===1)result.format=v=>Math.round(10*Math.pow(25,v))+'ms';
    if(algorithm===12&&index===0)result.format=v=>hz(120*Math.pow(5000/120,v));
    if(algorithm===12&&index===1)result.format=v=>{const n=Math.round(v*200)-100;return (n>0?'+':'')+n+'%';};
    if(algorithm===12&&index===3)result.format=v=>hz(500*Math.pow(36,v));
    if(algorithm===8&&index===2){const names=['FREE','1/8','1/12','1/16','1/32'];result.choiceCount=5;result.step=.25;result.format=v=>names[clamp(Math.round(v*4),0,4)];}
    if(algorithm>=13){
        const choice=names=>Object.assign(result,{choiceCount:names.length,step:1/(names.length-1),format:v=>names[clamp(Math.round(v*(names.length-1)),0,names.length-1)]});
        if(index===0||algorithm===13&&index===2||algorithm===16&&index===2)choice(['OFF','ON']);
        if(index===3&&algorithm!==14)result.format=v=>Math.round(1+29*v)+'ms';
        if(algorithm===13&&index===1)result.format=v=>Math.round(v*60)+'%';
        if(algorithm===14&&index===1)choice(['ALL','ODD','PAIR','THREE','EUCL','SKIP','RAND','BURST']);
        if(algorithm===14&&index===2)result.format=v=>Math.abs(v-.5)<.005?'C':(v<.5?'L':'R')+Math.round(Math.abs(v-.5)*200);
        if(algorithm===14&&index===3||algorithm===15&&index===2){result.choiceCount=128;result.step=1/127;result.format=v=>String(Math.round(v*127));}
        if(algorithm===15&&index===1)choice(['4','8','16']);
        if(algorithm===16&&index===1)choice(['LIN','SMTH','EXP']);
    }
    return result;
}
function layersPageSide(page){
    if(page?.layerFx||page?.layersSetup)return 1;
    const category=pageCategoryForPage(page);
    return category===0||category===1?0:-1;
}
function rememberLayersPage(page){
    if(!layersUi.active||state.view!=='settings')return;
    page=page||currentSettingsPage();const side=layersPageSide(page);
    if(side<0)return;
    layersUi.focus=side;layersUi.pages[side]=page.title;
}
function layersFocusPage(side,pages=parameterPages()){
    const stored=pages.findIndex(p=>p.title===layersUi.pages[side]&&layersPageSide(p)===side);
    if(stored>=0)return stored;
    return Math.max(0,side?pages.findIndex(p=>p.title==='FX CORNER'):pages.findIndex(p=>layersPageSide(p)===0));
}
function toggleLayersFocus(){
    if(!layersUi.active||!['main','settings'].includes(state.view))return;
    rememberLayersPage();
    const side=layersUi.focus?0:1,pages=parameterPages(),index=layersFocusPage(side,pages);
    layersUi.returnView=null;layersUi.editCorner=-1;layersUi.holdFired=true;layersUi.holdPad=-1;layersUi.tapPad=-1;
    layersUi.focus=side;layersUi.pages[side]=pages[index].title;state.settingsPage=index;
    state.actionMessageTicks=0;
    if(state.view==='settings'&&state.pageDestinationReturn){
        const category=pageCategoryForPage(pages[index]);
        const entries=state.pageDestinationReturn.tier==='category'?categoryPageEntries(category):pageEntries();
        state.pageDestinationReturn={...state.pageDestinationReturn,category,selection:Math.max(0,entries.findIndex(e=>e.view==='settings'&&e.index===index))};
    }
    resetParameterEncoderMotion();
    if(state.view==='settings'&&pages[index].pathEditor)readStackPath();
    rebuildLedQueue();draw();
}
function closeLayersEditor(){
    rememberLayersPage();
    const back=layersUi.returnView;layersUi.returnView=null;layersUi.editCorner=-1;layersUi.holdFired=true;
    if(back){state.view=back.view;state.settingsPage=clamp(back.page,0,parameterPages().length-1);layersUi.focus=back.focus??0;rememberLayersPage();resetParameterEncoderMotion();}
}
function toggleLayersCornerEditor(pad){
    const x=(pad%4)/3,y=1-Math.floor(pad/8)/3;
    const corner=Math.round(y)*2+Math.round(x);
    if(layersUi.editCorner===corner&&layersUi.returnView){closeLayersEditor();return;}
    if(!['main','settings'].includes(state.view))return;
    rememberLayersPage();
    if(!layersUi.returnView)layersUi.returnView={view:state.view,page:state.settingsPage,focus:layersUi.focus};
    layersUi.editCorner=corner;PARAMS.layers_corner.value=corner;
    setParamBlocking('layers_corner',corner);readLayers();
    const page=parameterPages().findIndex(p=>p.title==='FX CORNER');
    if(page>=0){state.view='settings';state.settingsPage=page;rememberLayersPage();resetParameterEncoderMotion();}
}
function layersCornerGesture(pad,pressed,now=Date.now()){
    if(!layersUi.active||![4,7,28,31].includes(pad))return;
    if(!pressed){if(layersUi.holdPad===pad)layersUi.holdPad=-1;return;}
    if(state.shiftHeld||state.deleteHeld||state.focusButtonHeld)return;
    if(layersUi.holdPad===pad)return;
    layersUi.holdPad=pad;layersUi.holdTime=now;layersUi.holdFired=false;
    if(layersUi.tapPad===pad&&now-layersUi.tapTime<=300){
        layersUi.holdFired=true;layersUi.tapPad=-1;toggleLayersCornerEditor(pad);
    }else{layersUi.tapPad=pad;layersUi.tapTime=now;}
}
function tickLayersCornerGesture(now=Date.now()){
    if(!layersUi.active||layersUi.holdPad<0||layersUi.holdFired)return;
    if(state.shiftHeld||state.deleteHeld||state.focusButtonHeld){layersUi.holdFired=true;return;}
    if(now-layersUi.holdTime>=500){layersUi.holdFired=true;layersUi.tapPad=-1;toggleLayersCornerEditor(layersUi.holdPad);}
}
function layersPages(engine,pads,enginePages){
    const keyboard={...pads,keys:pads.keys.map(k=>k?.startsWith('voice_engine_')&&k!=='voice_engine_depth'?null:k)};
    return [engine,keyboard,AMP_ENVELOPE_PAGE,PLAY_GAIN_PAGE,...enginePages,LAYERS_SETUP_PAGE,
        {...LAYERS_FX_PAGE,keys:LAYERS_FX_PAGE.keys.map((key,i)=>PARAMS.layers_fx0.value===0&&i>0&&i<7?null:key)},
        ...([2,3,4,5,6,7,8,9,10,12,13,14,15,16].includes(Math.round(PARAMS.layers_fx0.value))?[LAYERS_FX_DETAIL_PAGE]:[]),LAYERS_TOOLS_PAGE,MIX_REC_PAGE,SAMPLE_PAGE,MIDI_CONTROL_PAGE,MIDI_NOTES_PAGE,MIDI_MAPS_PAGE,
        LOOP_PAGE,PATTERN_FADE_PAGE,TAKE_PAGE,CHAIN_PAGE,CHAIN_FADE_PAGE,ENTRY_FADE_PAGE,CHAIN_TOOLS_PAGE];
}
function readLayers(){
    let data;try{data=JSON.parse(String(getParam('layers_view','{}')));}catch(_){return;}
    if(!data||typeof data!=='object')return;
    const oldPage=state.view==='settings'&&layersUi.active?currentSettingsPage():null;
    const was=layersUi.active,previous=layersUi.data;
    layersUi.active=data.active===1;layersUi.data=data;
    const redrawLeds=was!==layersUi.active||data.fxPads!==previous?.fxPads||(data.touches||[]).some((value,index)=>value!==previous?.touches?.[index]);
    if(layersUi.active){PARAMS.split_mode.value=5;state.lastNonSingleMode=5;}
    else if(was){layersUi.focus=0;layersUi.pages=['','FX CORNER'];layersUi.returnView=null;layersUi.editCorner=-1;layersUi.holdPad=-1;layersUi.tapPad=-1;layersUi.wave=false;resetParameterEncoderMotion();rebuildLedQueue();}
    if(!layersUi.active){if(state.view==='layerFxPicker')state.view='main';return;}
    PARAMS.layers_corner.value=Number(data.fxCorner??data.corner)||0;
    const values=data.values||[],config=values.slice(32);
    const settings=[null,null,null,'layers_glide',null,null,null,null,'layers_depth','layers_z_mode',null,'layers_return'];
    settings.forEach((key,i)=>{if(key&&Number.isFinite(config[i]))PARAMS[key].value=config[i];});
    for(let i=0;i<8;++i)if(Number.isFinite(values[(data.fxCorner??data.corner??0)*8+i]))PARAMS['layers_fx'+i].value=values[(data.fxCorner??data.corner??0)*8+i];
    for(let i=0;i<4;++i)if(Number.isFinite(data.extra?.[(data.fxCorner??data.corner??0)*4+i]))PARAMS['layers_fx'+(i+8)].value=data.extra[(data.fxCorner??data.corner??0)*4+i];
    let x=0,y=0;for(let c=0;c<4;++c){x+=(config[4+c]||0)*(c%2);y+=(config[4+c]||0)*Math.floor(c/2);}
    PARAMS.layers_fx_x.value=x;PARAMS.layers_fx_y.value=y;
    if(oldPage){const pages=parameterPages();let index=pages.findIndex(p=>p.title===oldPage.title);if(index<0&&oldPage.layerFxDetail)index=pages.findIndex(p=>p.title==='FX CORNER');if(index>=0)state.settingsPage=index;}
    rememberLayersPage();
    if(layersUi.returnView)layersUi.editCorner=PARAMS.layers_corner.value;
    layersUi.serial=Number(data.serial)||0;
    if(state.view==='layerFxPicker'&&layersPicker.corner!==PARAMS.layers_corner.value)closeLayersPicker();
    if(redrawLeds)rebuildLedQueue();
}
function layersRasterRect(x,y,width,height,color) {
    const first=clamp(x-LAYERS_WAVE_LEFT,0,LAYERS_WAVE_WIDTH);
    const last=clamp(x+width-LAYERS_WAVE_LEFT,0,LAYERS_WAVE_WIDTH);
    const lowStart=clamp(y-LAYERS_WAVE_TOP,0,32),lowEnd=clamp(y+height-LAYERS_WAVE_TOP,0,32);
    const highStart=clamp(y-LAYERS_WAVE_TOP-32,0,8),highEnd=clamp(y+height-LAYERS_WAVE_TOP-32,0,8);
    const low=lowEnd>lowStart?(0xffffffff>>>(32-(lowEnd-lowStart)))<<lowStart:0;
    const high=highEnd>highStart?((1<<(highEnd-highStart))-1)<<highStart:0;
    for(let column=first;column<last;++column) {
        const at=column*2;
        layersRasterTarget[at]=color?layersRasterTarget[at]|low:layersRasterTarget[at]&~low;
        layersRasterTarget[at+1]=color?layersRasterTarget[at+1]|high:layersRasterTarget[at+1]&~high;
    }
}

function paintLayersWaveChanges() {
    const {frame,painted}=layersDisplay;
    for(let column=0;column<LAYERS_WAVE_WIDTH;++column) {
        const at=column*2;
        const diff=[frame[at]^painted[at],frame[at+1]^painted[at+1]];
        if(!(diff[0]||diff[1]))continue;
        const changed=(y)=>(diff[y>>>5]>>>(y&31))&1;
        const pixel=(y)=>(frame[at+(y>>>5)]>>>(y&31))&1;
        let y=0;
        while(y<40) {
            if(!changed(y)){++y;continue;}
            const first=y,color=pixel(y++);
            while(y<40&&changed(y)&&pixel(y)===color)++y;
            drawRect(LAYERS_WAVE_LEFT+column,LAYERS_WAVE_TOP+first,1,y-first,color);
        }
    }
    painted.set(frame);
}

function drawLayersMain() {
    const data=layersUi.data||{},v=data.values||[],config=v.slice(32),mix=data.mix||config.slice(4,8);
    if(!layersDisplay.valid) {
        clearDisplay();layersDisplay.painted.fill(0);
        layersDisplay.header=layersDisplay.gutter=layersDisplay.fx=layersDisplay.footer=layersDisplay.notice=null;
        drawRect(63,10,1,44,1);
    }
    const title='LAYERS '+(layersUi.focus?'FX ':'A ')+(ALGORITHMS[state.algorithm]||'ENGINE'),fraction=state.clipRecording?'REC':'V'+(data.voices||[]).length;
    const header=title+fraction;
    if(header!==layersDisplay.header) {
        drawRect(0,0,128,10,0);drawTitleWithFraction(title,fraction);drawRect(0,8,128,1,1);
        layersDisplay.header=header;
    }
    const gutter=[state.lane,...state.weights.map(w=>Math.round(w*7))].join(',');
    if(gutter!==layersDisplay.gutter) {
        drawRect(0,10,12,40,0);
        for(let lane=0;lane<4;++lane) {
            const y=15+lane*10,selected=state.lane===lane+1;
            if(selected)drawRect(1,y-4,8,8,1);
            print(2,y-4,String(lane+1),selected?0:1);
            const height=Math.round((state.weights[lane]||0)*7);
            if(height)drawRect(10,y+4-height,2,height,1);
        }
        layersDisplay.gutter=gutter;
    }
    const window=normalizedWaveWindow([state.fieldStart,state.fieldEnd]),windowKey=window.join(',');
    if(layersDisplay.waves!==state.waves||layersDisplay.mask!==state.mask||layersDisplay.window!==windowKey) {
        layersDisplay.base.fill(0);layersRasterTarget=layersDisplay.base;
        for(let lane=0;lane<4;++lane)drawWaveformLane(lane,window,15+lane*10,14,47);
        layersRasterTarget=null;layersDisplay.waves=state.waves;layersDisplay.mask=state.mask;layersDisplay.window=windowKey;
    }
    layersDisplay.frame.set(layersDisplay.base);layersRasterTarget=layersDisplay.frame;
    const markers=[];
    const reader=(position,lane,outgoing=false)=>{
        const x=waveformX(position,window,14,47);
        if(x<0||!Number.isFinite(lane)||lane<0||lane>3)return;
        if(outgoing)drawDashedPlayhead(x,10,40);else drawRect(x,10,1,40,1);
        markers.push([x,15+lane*10]);
    };
    if(state.mask)for(let voice=0;voice<8;++voice) {
        const cut=state.cutupsVoices[voice],ws=state.wavesetVoices[voice],lane=state.stackPositions[voice]*3;
        if(state.algorithm===3) {
            if(cut?.outgoingGain>0)reader(cut.outgoingPosition,cut.outgoingLane,true);
            if(cut?.gain>0)reader(cut.position,cut.lane);
        } else if(state.algorithm===8) {
            if(ws) {reader(ws.secondarySourcePosition,lane,true);reader(ws.sourcePosition,lane);}
        } else reader(state.playheads[voice],lane);
    }
    for(const [x,y] of markers)clearStackMarker(x,y);
    for(const [x,y] of markers)drawStackMarker(x,y);
    layersRasterTarget=null;paintLayersWaveChanges();
    const notice=state.saveStatus===4?saveNotice():state.messageTicks>0?state.message:'';
    if(Boolean(notice)!==Boolean(layersDisplay.notice)){layersDisplay.footer=layersDisplay.fx=null;}
    layersDisplay.notice=notice;
    const types=[0,8,16,24].map(i=>Math.round(v[i]||0)),z=Math.round((data.z??config[10]??0)*100);
    const touches=(data.touches||[]).map((touch,p)=>p%8>=4?touch:0),x=Math.round(((mix[1]||0)+(mix[3]||0))*48),y=Math.round(((mix[2]||0)+(mix[3]||0))*21);
    const bars=mix.map(w=>Math.round(w*4));
    const fx=[...types,z,x,y,...bars,...touches].join(',');
    if(fx!==layersDisplay.fx) {
        drawRect(66,10,62,54,0);print(68,11,'FX Z '+z,1);
        const names=LAYERS_FX_SHORT;
        print(68,19,names[types[0]],1);print(102,19,names[types[1]],1);
        print(68,49,names[types[2]],1);print(102,49,names[types[3]],1);
        for(let row=0;row<4;++row)for(let col=0;col<4;++col)drawRect(68+col*16,26+row*7,1,1,1);
        // Hollow markers show held/latched pad targets. The solid cursor
        // always follows the DSP's smoothed mix, including during a touch.
        for(let p=0;p<32;++p)if(touches[p]) {
            const px=67+(p%4)*16,py=25+(3-Math.floor(p/8))*7;
            drawRect(px,py,3,3,1);drawRect(px+1,py+1,1,1,0);
        }
        drawRect(67+x,25+y,3,3,1);
        // Compact meters stop at x=95, leaving the save badge its own area.
        for(let c=0;c<4;++c){drawRect(66+c*8,62,6,1,1);drawRect(66+c*8,62-bars[c],6,1+bars[c],1);}
        layersDisplay.fx=fx;
    }
    const progress=state.clipDuration>0?Math.round(61*state.clipPosition):0,footer='LANE '+state.lane;
    if(footer+progress!==layersDisplay.footer) {
        drawRect(0,51,63,13,0);if(progress)drawRect(1,52,progress,1,1);print(1,56,footer,1);
        layersDisplay.footer=footer+progress;
    }
    if(notice){drawRect(0,57,128,7,0);print(1,57,String(notice).slice(0,21),1);}
    layersDisplay.valid=true;
}


function isQuad() {return state.splitEnabled&&state.quadMode;}
function partCount(){return isQuad()?4:2;}
function loopAvailable(){return !isQuad()||state.algorithm===6||PARAMS.effect_tape_inertia.value>0;}
function partName(part=state.splitFocus){return String.fromCharCode(65+part);}
function padPart(index){return isQuad()?(1-Math.floor(index/16))*2+(index%8>=4?1:0):(index%8>=4?1:0);}
function padLocal(index){return state.splitEnabled?(isQuad()?Math.floor(index/8)%2:Math.floor(index/8))*4+index%4:index;}
function cueForPad(index){const local=padLocal(index);return state.splitEnabled?(local<(isQuad()?2:4)?padPart(index)*(isQuad()?2:4)+local:-1):(index<8?index:-1);}
function partLevelKey(part=state.splitFocus){return 'split_level_'+partName(part).toLowerCase();}
function isSendPage(page){return page===SPLIT_MIX_PAGE||page===QUAD_SEND_PAGE;}
function isSetupPage(page){return page===SPLIT_SETUP_PAGE||page===QUAD_SETUP_PAGE;}
function quadIdle(index){return [ScaleNoteGrey,SplitBIdle,DarkYellowOlive,SharedBIdle][padPart(index)];}

function parseSplitState(text, refresh=true) {
    let data;
    try { data=typeof text==='object'?text:JSON.parse(String(text||'{}')); } catch (_) { return; }
    if(!data || typeof data!=='object')return;
    const enabled=Number(data.enabled)===1;
    const quad=Number(data.quad)===1;
    const focus=clamp(Number(data.focus)||0,0,enabled&&quad?3:1);
    const shared=Number(data.shared)===1;
    const bank=Number(data.bank)===1;
    const changed=bank!==Boolean(state.bankMode) || enabled!==state.splitEnabled || shared!==state.sharedDual || quad!==state.quadMode || focus!==state.splitFocus;
    if(changed && refresh && refreshEditorSnapshot())return;
    if(changed)state.splitPages[state.splitFocus]=state.settingsPage;
    state.bankMode=bank;state.splitEnabled=enabled;state.sharedDual=shared;state.quadMode=quad;state.splitFocus=focus;
    if(!enabled && (state.view==='fxMenu'||state.view==='mixStatus')){state.view='main';state.fxMenuReturn=false;}
    state.splitBlend=clamp(Number(data.blend)||0,0,1);
    let padMapChanged = false;
    if (Array.isArray(data.parts) && data.parts.length === (enabled&&quad?4:2)) {
        padMapChanged = data.parts.some((part, index) =>
            ['root', 'octave', 'scale', 'mode', 'mute'].some((key) =>
                part[key] !== state.splitParts[index]?.[key]));
        state.splitParts = data.parts;
    }
    PARAMS.split_mode.value=bank?4:enabled?(quad?3:shared?2:1):0;PARAMS.split_focus.value=focus;
    if(PARAMS.split_mode.value>0)state.lastNonSingleMode=PARAMS.split_mode.value;
    if(changed) {
        state.engineListReturn=false;
        state.algorithm=Math.round(PARAMS.algorithm.value);
        state.padMode=Math.round(PARAMS.pad_mode.value);state.scale=Math.round(PARAMS.scale.value);
        state.octave=clamp(Number(data.parts?.[focus]?.octave)||0,-3,3);
        state.settingsPage=state.splitPages[focus]||0;
        state.fxRouteClip=-1;state.fxRouteGrabbed=false;state.fxRoutePending=false;
        resetParameterEncoderMotion();rebuildLedQueue();
    }
    // Mute can change from the hardware button, recall, or another editor.
    // Focus need not change; never leave Split Setup displaying a stale OFF.
    PARAMS.part_mute.value=state.splitParts[focus]?.mute?1:0;
    if (padMapChanged && !changed) rebuildLedQueue();
}
function padSettings(index) {
    if(state.splitEnabled)return state.splitParts[padPart(index)]||{};
    return {root:Math.round(PARAMS.root_note.value),octave:state.octave,scale:state.scale,mode:state.padMode};
}
function setSplitFocus(part) {
    endPerformanceLayer();
    setParamBlocking('split_focus',part);
    if(!refreshEditorSnapshot())parseSplitState(getParam('split_state','{}'),false);
    if(!state.editorReady)showMessage('UPDATING EDITOR');
    else if(state.view!=='main')showMessage('EDIT PART '+partName());
    if(state.view==='settings'&&currentSettingsPage().pathEditor)readStackPath();
    rebuildLedQueue();draw();
}
const MODE_MESSAGES=['SINGLE STACK','SPLIT A / B','SHARED DUAL A B','QUAD A B C D','BANK A + 16 SAMPLES','LAYERS ENGINE + FX','FOUR LANE SEQUENCE'];
function currentPerformanceMode(){return layersUi.active?5:state.bankMode?4:state.splitEnabled?(isQuad()?3:state.sharedDual?2:1):0;}
function toggleSplitMode() {
    endPerformanceLayer();
    parseSplitState(getParam('split_state','{}'));readLayers();
    const current=currentPerformanceMode();
    if(current>0)state.lastNonSingleMode=current;
    const previous=clamp(Math.round(getNumber('split_return_mode',state.lastNonSingleMode)),1,5);
    const wanted=current>0?0:previous;
    setParamBlocking('split_mode',wanted);
    parseSplitState(getParam('split_state','{}'));readLayers();
    const actual=currentPerformanceMode();
    const error=String(getParam('last_error','')||'');
    showMessage(error|| (actual===wanted?MODE_MESSAGES[actual]:'STOP RECORD FIRST'));rebuildLedQueue();
    primePadLeds();
}
function adjustSplitBlend(delta) {
    if(isQuad()){const previous=state.view;state.view='main';adjustControl(7,delta);state.view=previous;return;}
    if(!state.editorReady){showMessage('UPDATING EDITOR');return;}
    delta=parameterEncoderDelta(7,'split_blend',delta);if(!delta)return;
    const control=PARAMS.split_blend;control.value=clamp(control.value+delta*.01,0,1);
    setParam('split_blend',control.value.toFixed(4));
    if(state.clipRecording)setParam('clip_automation','split_blend');
    showMessage('BLEND A '+Math.round((1-control.value)*100)+' B '+Math.round(control.value*100));
}

function parsePulsarState(text) {
    state.pulsarVoices=Array(8).fill(null);
    for(const item of String(text||'').split(';')) {
        const v=item.split(',').map(Number);if((v.length!==7&&v.length!==8)||!v.every(Number.isFinite)||!Number.isInteger(v[0])||v[0]<0||v[0]>7)continue;
        state.pulsarVoices[v[0]]={start:clamp(v[1]/1000,0,1),end:clamp(v[2]/1000,0,1),active:v[3],limited:v[4]>0,frequency:v[5],pulse:v[6],durationMs:v.length===8&&v[7]>0?v[7]:null};
    }
}
function pulsarDurationFooter() {
    const voices=state.pulsarVoices.filter((p,i)=>p&&(!state.splitEnabled||Math.floor(i/(isQuad()?2:4))===state.splitFocus));
    const durations=voices.map(p=>p.durationMs).filter(ms=>Number.isFinite(ms)&&ms>0);
    const format=ms=>ms<10?String(Number(ms.toFixed(1))):String(Math.round(ms));
    let duration;
    if(durations.length) {
        const low=format(Math.min(...durations)),high=format(Math.max(...durations));
        duration=(low===high?low:low+'-'+high)+'ms';
    } else {
        // At rest, estimate the root-note pulse with unmodulated controls.
        const requested=1000/PARAMS.pulsar_form.value;
        const limit=2900/PARAMS.pulsar_rate.value*(1-.45*PARAMS.pulsar_jitter.value/100);
        duration='~'+format(Math.min(requested,limit))+'ms';
    }
    return duration+' '+(voices.some(p=>p.limited)?'LIMIT':PARAMS.pulsar_source_hold.value>=.5?'CLICK:RUN':'CLICK:HOLD');
}
function drawPulsarFragment(slot,lane,window) {
    const p=state.pulsarVoices[slot];if(!p||p.active<=0||lane<0||lane>3||p.end<window[0]||p.start>window[1])return;
    const a=waveformX(clamp(p.start,window[0],window[1]),window),b=waveformX(clamp(p.end,window[0],window[1]),window);
    if(a<0||b<a)return;const y=11+Math.round(lane)*10;drawRect(a,y,Math.max(1,b-a+1),1,1);drawRect(a,y,1,2,1);drawRect(b,y,1,2,1);if(p.frequency>20||p.pulse)drawRect(a,y+2,2,2,1);
}
function parsePlayheads(text) {
    parsePulsarState(getParam('pulsar_state',''));
    const values = String(text || '').split(',').map((item) => Number(item));
    state.playheads = Array.from({ length: 8 }, (_, index) => {
        const value = values[index];
        return Number.isFinite(value) && value >= 0
            ? clamp(value / 1000, 0, 1) : -1;
    });
}

function parseCutupsState(text) {
    const voices = Array(8).fill(null);
    for (const record of String(text || '').split(';')) {
        if (!record) continue;
        const v = record.split(',').map(Number);
        if (v.length !== 7 || !v.every(Number.isFinite)) continue;
        const slot = v[0];
        if (!Number.isInteger(slot) || slot < 0 || slot >= voices.length) continue;
        const validLane = (value) => Number.isInteger(value) && value >= 0 && value < 4;
        voices[slot] = {
            lane: validLane(v[1]) ? v[1] : -1,
            position: v[2] >= 0 ? clamp(v[2] / 1000, 0, 1) : -1,
            outgoingLane: validLane(v[3]) ? v[3] : -1,
            outgoingPosition: v[4] >= 0 ? clamp(v[4] / 1000, 0, 1) : -1,
            gain: clamp(v[5] / 1000, 0, 1),
            outgoingGain: clamp(v[6] / 1000, 0, 1),
        };
    }
    state.cutupsVoices = voices;
}

function parseWavesetState(text) {
    const voices = Array(8).fill(null);
    for (const record of String(text || '').split(';')) {
        if (!record) continue;
        const values = record.split(',').map((item) => Number(item));
        if (values.length < 14 || !values.every(Number.isFinite)) continue;
        const slot = values[0] | 0;
        if (slot < 0 || slot >= voices.length) continue;
        const flags = values[13] | 0;
        voices[slot] = {
            slot,
            groupSize: clamp(values[1] | 0, 1, 32),
            cycleOffset: Math.max(0, values[2] | 0),
            repeatIndex: Math.max(0, values[3] | 0),
            repeats: clamp(values[4] | 0, 1, 16),
            direction: clamp(values[5] | 0, 0, 3),
            requestedDirection: clamp(values[6] | 0, 0, 3),
            process: clamp(values[7] | 0, 0, 11),
            phase: clamp(values[8] / 1000, 0, 1),
            groupPosition: values[9] >= 0
                ? clamp(values[9] / 1000, 0, 1) : -1,
            secondaryGroupPosition: values[10] >= 0
                ? clamp(values[10] / 1000, 0, 1) : -1,
            sourcePosition: values[11] >= 0
                ? clamp(values[11] / 1000, 0, 1) : -1,
            secondarySourcePosition: values[12] >= 0
                ? clamp(values[12] / 1000, 0, 1) : -1,
            orderPending: (flags & 1) !== 0,
            overlap: (flags & 2) !== 0,
            multipleSources: (flags & 4) !== 0,
            primaryReversed: (flags & 8) !== 0,
            focused: (flags & 16) !== 0,
        };
    }
    state.wavesetVoices = voices;
    const focus = focusedWavesetVoice();
    if (state.algorithm === 8 && focus && focus.sourcePosition >= 0) {
        // Cue storage must follow the same primary source head the player sees,
        // including Reverse processes and processed-source replacement.
        state.position = focus.sourcePosition;
    }
}

function focusedWavesetVoice() {
    return state.wavesetVoices.find((voice) => voice && voice.focused)
        || state.wavesetVoices.find((voice) => voice) || null;
}

function parseStackPositions(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    state.stackPositions = Array.from({ length: 8 }, (_, index) => {
        const value = values[index];
        return Number.isFinite(value) && value >= 0
            ? clamp(value / 1000, 0, 1) : -1;
    });
}

function parseVoicePads(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    const previous = state.voicePads.join(',');
    state.voicePads = Array.from({ length: 8 }, (_, index) => {
        const value = values[index];
        return Number.isFinite(value) && value >= 0 && value < 32
            ? value | 0 : -1;
    });
    if (previous !== state.voicePads.join(',')) rebuildLedQueue();
}

function parseVoiceNotes(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    const previous = state.voiceNotes.join(',');
    state.voiceNotes = Array.from({ length: 8 }, (_, index) => {
        const value = values[index];
        return Number.isFinite(value) && value >= 0 && value <= 127
            ? value | 0 : -1;
    });
    if (previous !== state.voiceNotes.join(',')) rebuildLedQueue();
}

function parseRollPads(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    const previous = state.rollPads.join(',');
    state.rollPads = Array.from({ length: 8 }, (_, index) => {
        const value = values[index];
        return Number.isFinite(value) && value >= 0 && value < 32
            ? value | 0 : -1;
    });
    if (previous !== state.rollPads.join(',')) rebuildLedQueue();
}

function parseRollNotes(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    const previous = state.rollNotes.join(',');
    state.rollNotes = Array.from({ length: 8 }, (_, index) => {
        const value = values[index];
        return Number.isFinite(value) && value >= 0 && value <= 127
            ? value | 0 : -1;
    });
    if (previous !== state.rollNotes.join(',')) rebuildLedQueue();
}

function parseWaveforms(text) {
    text=String(text||'');
    if(text===state.waveText)return;
    state.waveText=text;
    const lanes = text.split('|');
    for(let lane=0;lane<4;++lane)if(lanes[lane]!==state.waveKeys[lane])state.laneRegions[lane]=null;
    state.waveKeys=lanes;
    const decoded = [];
    for (let lane = 0; lane < 4; lane++) {
        const source = lanes[lane] || '';
        const bins = [];
        for (let bin = 0; bin < WAVE_BINS; bin++) {
            const low = parseInt(source[bin * 2] || '7', 16);
            const high = parseInt(source[bin * 2 + 1] || '7', 16);
            bins.push([
                Number.isFinite(low) ? low - 7 : 0,
                Number.isFinite(high) ? high - 7 : 0,
            ]);
        }
        decoded.push(bins);
    }
    state.waves = decoded;
}

function parseClipState(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    if (values.length < 8 || !values.every((value) => Number.isFinite(value))) return;
    const previous = [state.clipRecording, state.clipPlaying,
        state.clipPosition, state.clipDuration].join(',');
    state.clipRecording = values[0] !== 0;
    state.clipPlaying = values[1] !== 0;
    state.clipOverdub = values[2] !== 0;
    state.clipPosition = clamp(values[3] / 1000, 0, 1);
    state.clipDuration = Math.max(0, values[4] | 0);
    state.clipNoteCount = Math.max(0, values[5] | 0);
    state.clipPointCount = Math.max(0, values[6] | 0);
    state.clipRate = clamp(values[7] / 1000, 0.25, 4);
    if (values.length >= 12) {
        state.clipLaunchPending = values[8] !== 0;
        state.clipTiming = clamp(values[9] | 0, 0, 1);
        state.clipRecordedTempo = clamp(values[10] / 100, 1, 999);
        state.clipBeats = Math.max(0, values[11] / 1000);
        PARAMS.clip_timing.value = state.clipTiming;
    }
    PARAMS.clip_rate.value = state.clipRate;
    if (previous !== [state.clipRecording, state.clipPlaying,
        state.clipPosition, state.clipDuration].join(',')) rebuildLedQueue();
}

function parseClipCapacityState(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    if (values.length < 6 || !values.every((value) => Number.isFinite(value))) return;
    const wasFull = state.clipNoteFull || state.clipPointFull;
    state.clipNoteCount = Math.max(0, values[0] | 0);
    state.clipNoteCapacity = Math.max(1, values[1] | 0);
    state.clipPointCount = Math.max(0, values[2] | 0);
    state.clipPointCapacity = Math.max(1, values[3] | 0);
    state.clipNoteFull = values[4] !== 0;
    state.clipPointFull = values[5] !== 0;
    if (!wasFull && (state.clipNoteFull || state.clipPointFull)) {
        showMessage('CLIP FULL - ENDPOINTS KEPT', 100);
    }
}

function parseClipTakeState(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    if (values.length < 3 || !values.every((value) => Number.isFinite(value))) return;
    state.clipTakeCount = clamp(values[0] | 0, 0, 255);
    state.clipTakeCanUndo = values[1] !== 0;
    state.clipRecordingTake = clamp(values[2] | 0, 0, 255);
}

function parseClipBankState(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    if (values.length < 4 || !values.every((value) => Number.isFinite(value))) return;
    const previousActive = state.activeClip;
    const active = clamp(values[0] | 0, 0, 63);
    const queued = values[1] < 0 ? -1 : clamp(values[1] | 0, 0, 63);
    const low = values[2] >>> 0;
    const high = values[3] >>> 0;
    const changed = active !== state.activeClip || queued !== state.queuedClip
        || low !== state.clipMaskLow || high !== state.clipMaskHigh;
    state.activeClip = active;
    state.queuedClip = queued;
    state.clipMaskLow = low;
    state.clipMaskHigh = high;
    if (active !== previousActive) {
        state.clipBank = Math.floor(active / 16);
        resetParameterEncoderMotion();
        refreshEditorSnapshot();
        state.algorithm = Math.round(PARAMS.algorithm.value);
    }
    if (active !== previousActive
        || (state.view === 'settings' && currentSettingsPage().route)) syncFxOrder();
    if (changed) rebuildLedQueue();
}

function parseSampleLoadState(text) {
    const parts = String(text || '').split(',');
    const loadState = Number(parts.shift());
    parts.shift();
    const progress = Number(parts.shift());
    const previous = state.sampleLoadState;
    if (Number.isFinite(loadState)) state.sampleLoadState = loadState | 0;
    if (Number.isFinite(progress)) state.sampleLoadProgress = clamp(progress / 10, 0, 100);
    state.sampleLoadName = parts.join(',');
    if (state.sampleLoadState === 6 && previous !== 6) {
        showMessage('LOADED ' + state.sampleLoadName, 70);
    } else if (state.sampleLoadState === 7 && previous !== 7) {
        showMessage('LOAD ERROR ' + state.sampleLoadName, 100);
    }
}

function parseClipPrepareState(text) {
    const values = String(text || '').split(',').map((item) => Number(item));
    if (values.length < 2 || !Number.isFinite(values[0])) return;
    const previous = state.clipPrepareState;
    state.clipPrepareState = values[0] | 0;
    if (state.clipPrepareState === 4 && previous !== 4) {
        showMessage('CLIP SOURCE MISSING', 100);
    }
}

function parseChainState(text) {
    const v=String(text||'').split(',').map(Number);if(v.length<5||!v.every(Number.isFinite))return;
    const previous=[state.chainEnabled,state.chainPosition,state.chainSlot,state.chainQueuedSlot,state.chainBank,state.chainPlayBank,state.chainLaunchBank,state.chainLaunchSlot];
    state.chainEnabled=!!v[0];state.chainPosition=clamp(v[1]|0,0,63);state.chainRepeat=Math.max(0,v[2]|0);
    state.chainSlot=clamp(v[3]|0,0,63);state.chainQueuedSlot=v[4]<0?-1:clamp(v[4]|0,0,63);
    state.chainBank=clamp(v[5]|0,0,15);state.chainEnd=clamp(v[6]|0,0,2);state.chainAdvance=!!v[7];
    state.chainPlayBank=Number.isFinite(v[8])?clamp(v[8]|0,0,15):state.chainBank;
    state.chainLaunchBank=Number.isFinite(v[9])?v[9]|0:-1;state.chainLaunchSlot=Number.isFinite(v[10])?v[10]|0:-1;
    state.chainPage=Math.floor(state.chainSlot/16);
    PARAMS.chain_enabled.value=state.chainEnabled?1:0;PARAMS.chain_slot.value=state.chainSlot+1;
    PARAMS.chain_bank.value=state.chainBank+1;PARAMS.chain_end.value=state.chainEnd;
    if(previous.some((x,i)=>x!==[state.chainEnabled,state.chainPosition,state.chainSlot,state.chainQueuedSlot,state.chainBank,state.chainPlayBank,state.chainLaunchBank,state.chainLaunchSlot][i]))rebuildLedQueue();
}
function readChainFades() {
    const v=String(getParam('chain_fades_state','0,0,2000,12000,0,0,0,2000,12000,0')).split(',').map(Number);
    if(v.length===10&&v.every(Number.isFinite))state.chainFades=v;
}

function parseChainEntries(text) {
    const entries = String(text || '').split(',');
    for (let index = 0; index < state.chainEntries.length; index++) {
        const fields = String(entries[index] || '').split(':').map(Number);
        state.chainEntries[index] = fields.length >= 3
            && fields.every((value) => Number.isFinite(value))
            ? { enabled: fields[0] !== 0, clip: clamp(fields[1] | 0, 0, 63),
                repeats: clamp(fields[2] | 0, 0, 16) }
            : { enabled: false, clip: 0, repeats: 1 };
    }
    const selected=state.chainEntries[state.chainSlot];
    PARAMS.chain_clip.value=selected.enabled?selected.clip+1:state.activeClip+1;
    PARAMS.chain_repeats.value=selected.enabled?selected.repeats:1;
}

function clipOccupied(index) {
    if (index < 32) return ((state.clipMaskLow >>> index) & 1) !== 0;
    return ((state.clipMaskHigh >>> (index - 32)) & 1) !== 0;
}

function basePadColor(owner = padSettings(0), index = 0) {
    if(isQuad())return quadIdle(index);
    if (state.splitEnabled && index % 8 >= 4) return state.sharedDual?SharedBIdle:SplitBIdle;
    if (owner.mode === 1) return AzureBlue;
    if (owner.mode === 2) return WhiteLedDim;
    if (owner.mode === 3) return Purple;
    if (owner.scale !== 0) return ScaleNoteGrey;
    return Black;
}

function padColor(index) {
    if(sequenceUi.active)return sequencePadColor(index);
    if(state.view==='sliceEdit'&&!state.performanceVisible){const color=sliceEditorPadColor(index);if(color!==null)return color;}
    if(state.performanceVisible && index<8 && !state.activePads.has(MovePads[index])) {
        if(index<4&&!performanceLaneEnabled(index))return Black;
        return state.performancePadLive.has(MovePads[index])?Cyan:(index<4?BrightOrange:WhiteLedDim);
    }
    if(layersUi.active&&index%8>=4){
        return ((layersUi.data?.touches?.[index]||0)||((Number(layersUi.data?.fxPads)>>>index)&1))?BrightOrange:DarkYellowOlive;
    }
    if(state.bankMode && index%8>=4) {
        const slot=Math.floor(index/8)*4+index%4,status=state.bank?.slots?.[slot]||0;
        return status===2?BrightRed:status===3?BrightOrange:status===4?Cyan:status===1?WhiteLedDim:status===5?Purple:Black;
    }
    const owner=padSettings(index);
    for (let roll = 0; roll < state.rollPads.length; roll++) {
        if (state.rollPads[roll] === index) {
            return state.rollNotes[roll] >= 0
                && state.rollNotes[roll] !== padMidiNote(index)
                ? DarkLightMagenta : LightMagenta;
        }
    }
    for (let voice = 0; voice < state.voicePads.length; voice++) {
        if (state.voicePads[voice] === index) {
            return state.voiceNotes[voice] >= 0
                && state.voiceNotes[voice] !== padMidiNote(index)
                ? VOICE_DIM_COLORS[voice] : VOICE_COLORS[voice];
        }
    }
    if (owner.mode === 2) {
        if(cueForPad(index)<0)return Black;
        if (state.activePads.has(MovePads[index])) return Cyan;
        return state.cues[cueForPad(index)] >= 0
            ? BrightOrange : (isQuad()?quadIdle(index):state.splitEnabled&&index%8>=4?(state.sharedDual?SharedBIdle:SplitBIdle):WhiteLedDim);
    }
    if (state.activePads.has(MovePads[index])) return Cyan;
    if (owner.mode === 0 && owner.scale !== 0
        && (padMidiNote(index) - Math.round(owner.root||60)) % 12 === 0) {
        return isQuad()?[White,SplitBRoot,BrightYellow,SharedBRoot][padPart(index)]:state.splitEnabled&&index%8>=4?(state.sharedDual?SharedBRoot:SplitBRoot):White;
    }
    return basePadColor(owner, index);
}

function ledKey(type, id) {
    return type + ':' + id;
}

function queueLed(type, id, color, priority) {
    const key = ledKey(type, id);
    if (state.ledSent.get(key) === color) {
        state.ledPending.delete(key);
        return;
    }
    const existing = state.ledPending.get(key);
    state.ledPending.set(key, {
        type,
        id,
        color,
        priority: Math.min(priority, existing ? existing.priority : priority),
        order: existing ? existing.order : state.ledOrder++,
    });
}

function sendLedNow(type, id, color, force = false) {
    const key = ledKey(type, id);
    state.ledPending.delete(key);
    if (type === 'cc') setButtonLED(id, color, force);
    else setLED(id, color, force);
    state.ledSent.set(key, color);
}

function iconLedStates() {
    const menu=['settings','pages','mode','fxMenu','help','projects'].includes(state.view);
    const upDown=['bankRecovery','projects'].includes(state.view)||customMidiViews.includes(state.view)||['main','settings','pages','mode','fxMenu','help','library','chain','sliceEdit'].includes(state.view);
    const leftRight=['main','settings','help'].includes(state.view);
    return [
        [MoveMenu,menu?WhiteLedBright:IconLedDim],
        [MoveBack,state.view!=='main'?WhiteLedBright:IconLedDim],
        [MoveUndo,customMidiContext()?(customMidiUi.data?.undo||['midiMapEdit','midiMapPicker'].includes(state.view)?WhiteLedBright:Black):state.undoAvailable&&!state.recording&&!state.clipRecording?WhiteLedBright:Black],
        [MoveShift,state.shiftHeld?WhiteLedBright:IconLedDim],
        [MoveDelete,state.deleteHeld?WhiteLedBright:IconLedDim],
        [MoveUp,upDown?IconLedDim:Black],[MoveDown,upDown?IconLedDim:Black],
        [MoveLeft,leftRight?IconLedDim:Black],[MoveRight,leftRight?IconLedDim:Black],
        [MoveCopy,state.recording||state.clipRecording?Black:currentPerformanceMode()>0?WhiteLedBright:IconLedDim],
        [MoveCapture,state.focusButtonHeld||state.view==='pages'||(state.splitEnabled&&state.splitFocus)||(layersUi.active&&layersUi.focus)?WhiteLedBright:IconLedDim],
        [MoveMute,state.splitEnabled?(state.splitParts[state.splitFocus]?.mute?WhiteLedBright:IconLedDim):Black],
        [MoveLoop,state.view==='sliceEdit'?(sliceUi.data?.loop?WhiteLedBright:IconLedDim):!loopAvailable()?Black:state.glitchBufferLocked||state.stackScanPaused?WhiteLedBright:state.recordedStackHold?IconLedMedium:IconLedDim],
    ].map(([id,value])=>[id,value!==Black&&state.iconPressed.has(id)?WhiteLedBright:value]);
}

function rebuildIconLedQueue() {
    for(const [id,value] of iconLedStates())queueLed('cc',id,value,0);
}

function primeIconLeds() {
    for(const [id,value] of iconLedStates())sendLedNow('cc',id,value,true);
}

function rebuildLedQueue() {
    rebuildIconLedQueue();
    queueLed('cc', MovePlay,
        state.clipLaunchPending ? BlueViolet
            : (state.clipPlaying ? NeonGreen : WhiteLedDim), 0);
    for (let i = 0; i < MovePads.length; i++) {
        const note = MovePads[i];
        queueLed('note', note, padColor(i), 3);
    }
    for (let i = 0; i < MoveSteps.length; i++) {
        let color = Black;
        let urgent = false;
        if (chainEditorActive()) {
            const row=state.chainPage*16+i;
            if (state.chainEntries[row].enabled) color = White;
            if (row === state.chainSlot) color = BrightOrange;
            if (row === state.chainPosition && state.chainEnabled && state.chainBank===state.chainPlayBank) color = NeonGreen;
            if ((state.chainLaunchBank===state.chainBank&&row===state.chainLaunchSlot)||(state.chainLaunchBank<0&&state.chainPlayBank===state.chainBank&&row===state.chainQueuedSlot)) color = BlueViolet;
            urgent = row === state.chainSlot || row === state.chainPosition
                || row === state.chainQueuedSlot;
        } else {
            const clip = state.clipBank * 16 + i;
            if (clipOccupied(clip)) color = White;
            if (clip === state.activeClip) {
                color = state.clipRecording ? BrightRed
                    : (state.clipPlaying ? NeonGreen : BrightOrange);
            }
            if (clip === state.queuedClip) color = BlueViolet;
            urgent = clip === state.activeClip || clip === state.queuedClip;
        }
        queueLed('note', MoveSteps[i], color, urgent ? 0 : 1);
    }
    const categoryNavigation=state.focusButtonHeld||state.view==='pages';
    const categoryLed=state.view==='pages'
        ?(state.pageTier==='category'?state.pageCategory
            :state.pageTier==='hub'&&state.pageSelection<4?state.pageSelection
            :currentPageCategory())
        :currentPageCategory();
    for (let i = 0; i < MoveTracks.length; i++) {
        const color=categoryNavigation
            ?(i===categoryLed?WhiteLedBright:WhiteLedDim)
            :state.view === 'clips'
                ?(i === state.clipBank ? WhiteLedBright : WhiteLedDim)
                :sequenceUi.active?(i===sequenceUi.data?.edit?WhiteLedBright:state.recording&&i===sequenceUi.data?.rec&&state.ticks%8<4?IconLedMedium:WhiteLedDim)
                :(i + 1 === state.lane ? WhiteLedBright : WhiteLedDim);
        queueLed('cc', MoveTracks[i],color,2);
    }
    queueLed('cc', MoveRecord,
        state.clipRecording ? BrightRed
            : (state.recording ? BrightOrange : WhiteLedDim), 1);
    if (MoveRecordAlt !== MoveRecord) {
        queueLed('cc', MoveRecordAlt,
            state.clipRecording ? BrightRed
                : (state.recording ? BrightOrange : WhiteLedDim), 1);
    }
}

function primePadLeds() {
    for (let index = 0; index < MovePads.length; index++) {
        sendLedNow('note', MovePads[index], padColor(index), true);
    }
}

function drainLedQueue() {
    const pending = [...state.ledPending.values()].sort((left, right) =>
        left.priority - right.priority || left.order - right.order);
    const count = Math.min(LEDS_PER_TICK, pending.length);
    for (let index = 0; index < count; index++) {
        const entry = pending[index];
        sendLedNow(entry.type, entry.id, entry.color);
    }
}

// Retain the Split waveform pixels. Focus, text and meters have separate
// regions; moving readers restore only the pixels they actually change.
const SPLIT_CANVAS_LEFT=12,SPLIT_CANVAS_TOP=10,SPLIT_CANVAS_WIDTH=116;
const splitDisplay={valid:false,header:null,gutter:null,rail:null,footer:null,meters:null,
    waves:null,mask:-1,windows:null,
    base:new Uint32Array(SPLIT_CANVAS_WIDTH*2),
    frame:new Uint32Array(SPLIT_CANVAS_WIDTH*2),
    painted:new Uint32Array(SPLIT_CANVAS_WIDTH*2)};
let splitRasterTarget=null;

function clearDisplay() {
    layersDisplay.valid=false;
    splitDisplay.valid=false;
    clear_screen();
}

function splitRasterRect(x,y,width,height,color) {
    const first=clamp(x-SPLIT_CANVAS_LEFT,0,SPLIT_CANVAS_WIDTH);
    const last=clamp(x+width-SPLIT_CANVAS_LEFT,0,SPLIT_CANVAS_WIDTH);
    const lowStart=clamp(y-SPLIT_CANVAS_TOP,0,32),lowEnd=clamp(y+height-SPLIT_CANVAS_TOP,0,32);
    const highStart=clamp(y-SPLIT_CANVAS_TOP-32,0,8),highEnd=clamp(y+height-SPLIT_CANVAS_TOP-32,0,8);
    const low=lowEnd>lowStart?(0xffffffff>>>(32-(lowEnd-lowStart)))<<lowStart:0;
    const high=highEnd>highStart?((1<<(highEnd-highStart))-1)<<highStart:0;
    for(let column=first;column<last;++column) {
        const at=column*2;
        splitRasterTarget[at]=color?splitRasterTarget[at]|low:splitRasterTarget[at]&~low;
        splitRasterTarget[at+1]=color?splitRasterTarget[at+1]|high:splitRasterTarget[at+1]&~high;
    }
}

function paintSplitWaveChanges() {
    const {frame,painted}=splitDisplay;
    for(let column=0;column<SPLIT_CANVAS_WIDTH;++column) {
        const at=column*2;
        const diff=[frame[at]^painted[at],frame[at+1]^painted[at+1]];
        if(!(diff[0]||diff[1]))continue;
        const changed=(y)=>(diff[y>>>5]>>>(y&31))&1;
        const pixel=(y)=>(frame[at+(y>>>5)]>>>(y&31))&1;
        let y=0;
        while(y<40) {
            if(!changed(y)){++y;continue;}
            const first=y,color=pixel(y++);
            while(y<40&&changed(y)&&pixel(y)===color)++y;
            drawRect(SPLIT_CANVAS_LEFT+column,SPLIT_CANVAS_TOP+first,1,y-first,color);
        }
    }
    painted.set(frame);
}

function normalizedWaveWindow(window) {
    if(!Array.isArray(window)||window.length!==2||!window.every(Number.isFinite))return [0,1];
    const start=clamp(window[0],0,1),end=clamp(window[1],0,1);
    return end-start>1e-6?[start,end]:[0,1];
}

function splitWaveWindow(part) {return normalizedWaveWindow(state.splitParts[part]?.window);}

function waveformX(position,window,left=14,width=114) {
    if(!Number.isFinite(position)||position<window[0]-1e-6||position>window[1]+1e-6)return -1;
    return left+Math.round(clamp((position-window[0])/(window[1]-window[0]),0,1)*(width-1)+1e-9);
}

function drawWaveformLane(lane,window,center,left=14,width=114) {
    if(!(state.mask&(1<<lane))){drawRect(left,center,width,1,1);return;}
    const bins=state.waves[lane];
    for(let pixel=0;pixel<width;++pixel) {
        const source=(window[0]+pixel/(width-1)*(window[1]-window[0]))*(WAVE_BINS-1);
        const first=Math.min(WAVE_BINS-1,Math.floor(source)),next=Math.min(WAVE_BINS-1,first+1),mix=source-first;
        const low=clamp(bins[first][0]+(bins[next][0]-bins[first][0])*mix,-7,7);
        const high=clamp(bins[first][1]+(bins[next][1]-bins[first][1])*mix,-7,7);
        const top=center-Math.round(high*4/7),bottom=center-Math.round(low*4/7);
        drawRect(left+pixel,top,1,Math.max(1,bottom-top+1),1);
    }
}

function drawSplitWavePixels(windows) {
    const key=windows.flat().join(',')+','+(state.sharedDual?1:0);
    if(splitDisplay.waves!==state.waves||splitDisplay.mask!==state.mask||splitDisplay.windows!==key) {
        splitDisplay.base.fill(0);splitRasterTarget=splitDisplay.base;
        for(let lane=0;lane<4;++lane) {
            drawWaveformLane(lane,windows[isQuad()?lane:lane<2?0:1],15+lane*10);
        }
        splitRasterTarget=null;
        splitDisplay.waves=state.waves;splitDisplay.mask=state.mask;splitDisplay.windows=key;
    }
    splitDisplay.frame.set(splitDisplay.base);splitRasterTarget=splitDisplay.frame;
    // Remember landmarks on each source lane. Editing focus must not move
    // or erase them, and replacing a sample invalidates only its own cache.
    for(let lane=0;lane<4;++lane) {
        const entry=state.laneRegions[lane];
        if(!entry||entry.source!==state.waveKeys[lane]||entry.regions.length<=2)continue;
        const center=15+lane*10,window=windows[isQuad()?lane:lane<2?0:1];
        for(const region of entry.regions.slice(1,-1)) {
            const x=waveformX(region,window);
            if(x>=0){drawRect(x,center-4,1,2,1);drawRect(x,center+3,1,2,1);}
        }
    }
    const markers=[];
    if(state.mask)for(let slot=0;slot<8;++slot) {
        const part=isQuad()?Math.floor(slot/2):slot<4?0:1,top=state.sharedDual?10:10+part*(isQuad()?10:20),height=state.sharedDual?40:isQuad()?10:20,window=windows[part];
        const first=state.sharedDual?0:isQuad()?part:part*2,last=state.sharedDual?3:isQuad()?part:part*2+1;
        const cap=(x)=>{if(state.sharedDual)drawRect(x-1,part?49:10,3,1,1);};
        const algorithm=state.voiceAlgorithms[slot],cut=state.cutupsVoices[slot],ws=state.wavesetVoices[slot];
        const head=algorithm===3?(cut?.gain>0?cut.position:-1):algorithm===8?(ws?.sourcePosition??-1):state.playheads[slot];
        const lane=algorithm===3?(cut?.lane??-1):(state.stackPositions[slot]>=0?(isQuad()?Math.round(state.stackPositions[slot]*3):state.stackPositions[slot]*3):-1);
        const x=waveformX(head,window);
        if(algorithm===14)drawPulsarFragment(slot,lane,window);
        if(x>=0&&lane>=first&&lane<=last){drawRect(x,top,1,height,1);cap(x);markers.push([x,15+lane*10]);}
        if(algorithm===3&&cut?.outgoingLane>=first&&cut.outgoingLane<=last&&cut.outgoingGain>0) {
            const outgoing=waveformX(cut.outgoingPosition,window);
            if(outgoing>=0){drawDashedPlayhead(outgoing,top,height);cap(outgoing);markers.push([outgoing,15+cut.outgoingLane*10]);}
        }
        if(algorithm===8&&ws?.secondarySourcePosition>=0) {
            const outgoing=waveformX(ws.secondarySourcePosition,window);
            if(outgoing>=0){drawDashedPlayhead(outgoing,top,height);cap(outgoing);}
        }
    }
    for(const [x,y] of markers)clearStackMarker(x,y);
    for(const [x,y] of markers)drawStackMarker(x,y);
    splitRasterTarget=null;paintSplitWaveChanges();
}

function drawSplitMain() {
    if(!splitDisplay.valid) {
        clearDisplay();splitDisplay.painted.fill(0);
        splitDisplay.header=splitDisplay.gutter=splitDisplay.rail=splitDisplay.footer=splitDisplay.meters=null;
    }
    const name=(part)=>state.splitParts[part]?.mute?'MUT':state.splitParts[part]?.engine_list?'LST':(ALGORITHMS[state.splitParts[part]?.algorithm]||'---').slice(0,3);
    const title=(isQuad()?'Q'+state.splitFocus+state.mixVoices.join(',')+state.splitParts.map(p=>p.algorithm+':'+p.mute).join(','):state.sharedDual?'SH ':'')+'A:'+name(0)+state.mixVoices[0]+' B:'+name(1)+state.mixVoices[1]+' '+Math.round(state.splitBlend*100)+'%';
    const header=title+(state.sharedDual?':'+state.splitFocus:'');
    if(header!==splitDisplay.header) {
        drawRect(0,0,128,9,0);
        if(isQuad()) {
            for(let p=0;p<4;++p){const x=1+p*24;if(p===state.splitFocus)drawRect(x,0,6,7,1);print(x,0,partName(p),p===state.splitFocus?0:1);print(x+6,0,String(state.mixVoices[p]||0),1);}
            print(103,0,name(state.splitFocus),1);
        } else if(state.sharedDual) {
            print(1,0,'SH',1);
            for(let part=0;part<2;++part) {
                const x=part?61:19,focused=state.splitFocus===part;
                if(focused)drawRect(x,0,6,7,1);
                print(x,0,part?'B':'A',focused?0:1);
                print(x+6,0,':'+name(part)+state.mixVoices[part],1);
            }
            print(103,0,Math.round(state.splitBlend*100)+'%',1);
        } else print(1,0,title,1);
        splitDisplay.header=header;splitDisplay.meters=null;
    }
    const weights=state.weights.map((weight)=>Math.round(weight*7));
    const gutter=[isQuad()?4+state.splitFocus:state.sharedDual?2:state.splitFocus,state.lane,...weights].join(',');
    if(gutter!==splitDisplay.gutter) {
        drawRect(0,10,12,40,0);
        for(let lane=0;lane<4;++lane) {
            const y=11+lane*10,focused=!state.sharedDual&&(isQuad()?lane:Math.floor(lane/2))===state.splitFocus;
            if(state.lane===lane+1)drawRect(0,y,2,8,1);
            if(focused)drawRect(2,y,7,8,1);
            print(3,y,String(lane+1),focused?0:1);
            if(weights[lane]>0)drawRect(9,y+8-weights[lane],2,weights[lane],1);
        }
        if(!state.sharedDual&&!isQuad())drawRect(0,30,12,1,1);
        splitDisplay.gutter=gutter;
    }
    // A one-pixel post-Blend activity bar under each part name.
    const meterWidths=Array.from({length:partCount()},(_,p)=>meterWidth(state.mixPeaks[p]||0,isQuad()?18:state.sharedDual?20:30));
    const meters=meterWidths.join(',');
    if(meters!==splitDisplay.meters){
        drawRect(0,8,128,1,0);
        for(let p=0;p<partCount();++p)if(meterWidths[p])drawRect(isQuad()?1+p*24:p?(state.sharedDual?73:66):(state.sharedDual?31:12),8,meterWidths[p],1,1);
        splitDisplay.meters=meters;
    }
    const common=normalizedWaveWindow([state.fieldStart,state.fieldEnd]);
    drawSplitWavePixels(isQuad()?[0,1,2,3].map(splitWaveWindow):state.sharedDual?[common,common]:[splitWaveWindow(0),splitWaveWindow(1)]);
    const blend=clamp(Math.round((isQuad()?PARAMS[partLevelKey()].value/2:state.splitBlend)*125),0,125),meter=Math.max(1,Math.round(114*state.meter));
    const progress=state.clipDuration>0?Math.max(1,Math.round(128*state.clipPosition)):0;
    const rail=[blend,state.recording?meter:0,progress].join(',');
    if(rail!==splitDisplay.rail) {
        drawRect(0,50,128,5,0);drawRect(0,51,128,1,1);drawRect(blend,50,3,3,1);
        if(state.recording)drawRect(14,51,meter,2,1);
        if(progress){drawRect(0,53,128,1,0);drawRect(0,53,progress,1,1);}
        splitDisplay.rail=rail;
    }
    const footer=mainFooter(null);
    if(footer!==splitDisplay.footer) {
        drawRect(0,55,128,9,0);print(1,55,footer,1);splitDisplay.footer=footer;
    }
    splitDisplay.valid=true;
}

function drawRect(x, y, width, height, value) {
    if (width <= 0 || height <= 0) return;
    if(layersRasterTarget){layersRasterRect(x|0,y|0,width|0,height|0,value!==0);return;}
    if(splitRasterTarget){splitRasterRect(x|0,y|0,width|0,height|0,value!==0);return;}
    if (typeof fill_rect === 'function') {
        fill_rect(x | 0, y | 0, width | 0, height | 0, value === 0 ? 0 : 1);
    }
}

function clearStackMarker(x, y) {
    const centerX = clamp(Math.round(x), 2, 125);
    const centerY = clamp(Math.round(y), 2, 61);
    drawRect(centerX - 2, centerY - 2, 5, 5, 0);
}

function drawStackMarker(x, y) {
    const centerX = clamp(Math.round(x), 2, 125);
    const centerY = clamp(Math.round(y), 2, 61);
    drawRect(centerX - 2, centerY - 2, 5, 1, 1);
    drawRect(centerX - 2, centerY + 2, 5, 1, 1);
    drawRect(centerX - 2, centerY - 1, 1, 3, 1);
    drawRect(centerX + 2, centerY - 1, 1, 3, 1);
}

function drawDashedPlayhead(x, top, height) {
    for (let y = top; y < top + height; y += 4) {
        drawRect(x, y, 1, Math.min(2, top + height - y), 1);
    }
}

function drawWavesetRail(left, width) {
    const focus = focusedWavesetVoice();
    if (state.algorithm !== 8 || !focus) return null;
    const group = focus.groupSize;
    const y = 51;
    drawRect(left, y, width, 1, 1);
    for (let boundary = 0; boundary <= group; boundary++) {
        const x = left + Math.round(boundary * (width - 1) / group);
        drawRect(x, y - 1, 1, 3, 1);
    }
    if (focus.multipleSources) {
        for (let slot = 0; slot < group; slot++) {
            const x = left + Math.round((slot + 0.5) * (width - 1) / group);
            drawRect(x, y + 2, 1, 1, 1);
        }
    }
    for (const voice of state.wavesetVoices) {
        if (!voice || voice.secondaryGroupPosition < 0) continue;
        const x = clamp(left + Math.round(
            voice.secondaryGroupPosition * (width - 1)),
            left + 1, left + width - 2);
        // Move's display is monochrome; a hollow cap is the dim/secondary
        // visual language used for an alternate processed or outgoing reader.
        drawRect(x - 1, y - 2, 3, 1, 1);
        drawRect(x - 1, y + 2, 3, 1, 1);
    }
    for (const voice of state.wavesetVoices) {
        if (!voice || voice.groupPosition < 0) continue;
        const x = clamp(left + Math.round(voice.groupPosition * (width - 1)),
            left + 1, left + width - 2);
        if (voice.focused) {
            drawRect(x - 1, y - 3, 3, 1, 1);
            drawRect(x, y - 2, 1, 5, 1);
        } else {
            drawRect(x, y - 1, 1, 3, 1);
        }
    }
    return focus;
}

function wavesetStatus(voice) {
    if (state.wavesetAnalysisPending) return 'WS PREPARING';
    if (!voice) return '';
    const orders = ['>', '<', '<>', 'SH'];
    const order = orders[voice.direction] || '?';
    const pending = voice.orderPending ? '*' : '';
    const overlap = voice.overlap ? '~' : '';
    return 'WS ' + order + pending + overlap
        + ' C' + (voice.cycleOffset + 1) + '/' + voice.groupSize
        + ' R' + (voice.repeatIndex + 1) + '/' + voice.repeats;
}

function readBankState() {
    if(!state.bankMode)return;
    try {
        const data=JSON.parse(String(getParam('bank_state','{}')));
        if(Array.isArray(data.slots)&&data.slots.length===16) {
            const changed=data.selected!==state.bank?.selected;
            const statuses=String(data.slots)!==String(state.bank?.slots);
            if(Number.isInteger(data.discard_seq)&&data.discard_seq>0
                &&data.discard_seq!==(state.bank?.discard_seq||0)) {
                const message=data.discard_reason===1?'BANK TAKE < 250 MS':data.discard_reason===3?'BANK TAKE DISCARDED':'BANK TAKE: NO AUDIO';
                showMessage(message);showActionMessage(message);
            }
            state.bank=data;
            if(changed || state.splitFocus===1)for(const key of [...BANK_SAMPLE_PAGE.keys,...BANK_PADS_PAGE.keys].filter(Boolean))
                PARAMS[key].value=getNumber(key,PARAMS[key].value);
            if(statuses)rebuildLedQueue();
        }
    } catch(_){}
    if(state.splitFocus===1)state.bankWave=String(getParam('bank_waveform','')).split(',').map(Number);
}

const bankRecovery={selection:0,confirm:false,token:0,returnView:'main'};
function openBankRecovery() {
    readBankState();if(!state.bank?.failed||state.bank.pending<0)return false;
    bankRecovery.selection=0;bankRecovery.confirm=false;bankRecovery.token=state.bank.take;
    bankRecovery.returnView=state.view;state.view='bankRecovery';draw();return true;
}
function drawBankRecovery() {
    clearDisplay();print(1,0,'UNSAVED BANK TAKE',1);drawRect(0,8,128,1,1);
    print(1,11,'PAD '+String((state.bank?.pending??-1)+1).padStart(2,'0')+' IN MEMORY',1);
    const rows=bankRecovery.confirm?['KEEP TAKE','DISCARD THIS TAKE']:['RETRY SAVE','KEEP WAITING','DISCARD...'];
    rows.forEach((text,i)=>{const y=23+i*10,active=i===bankRecovery.selection;
        if(active)drawRect(0,y-1,128,9,1);print(1,y,text,active?0:1);});
    print(1,57,bankRecovery.confirm?'CLICK YES; BACK NO':'CLICK SET; BACK WAIT',1);
}
function handleBankRecovery(number,value) {
    if(state.view!=='bankRecovery')return false;
    if(number===MovePlay&&value>0&&state.iconPressed.has(MoveDelete)){triggerPanic();return true;}
    if(number===MoveMainKnob||number===MoveUp||number===MoveDown){
        if(number!==MoveMainKnob&&value<=0)return true;
        const delta=number===MoveMainKnob?decodeDelta(value):number===MoveUp?-1:1;
        bankRecovery.selection=clamp(bankRecovery.selection+delta,0,bankRecovery.confirm?1:2);return true;
    }
    if(number===MoveBack||number===MoveMenu){
        if(value>0){if(bankRecovery.confirm){bankRecovery.confirm=false;bankRecovery.selection=2;}
            else state.view=bankRecovery.returnView==='bankRecovery'?'main':bankRecovery.returnView;}
        return true;
    }
    if(number===MoveMainButton&&value>0){
        readBankState();const current=state.bank;
        if(current?.take!==bankRecovery.token||!current.failed||current.pending<0){
            state.view='main';showMessage('TAKE ALREADY FINISHED');return true;
        }
        if(bankRecovery.confirm){
            if(bankRecovery.selection===0){bankRecovery.confirm=false;bankRecovery.selection=2;}
            else {setParamBlocking('bank_take_discard',bankRecovery.token);readBankState();
                if(state.bank?.failed)showMessage('SAVE BUSY: TRY AGAIN');
                else {state.view='main';showMessage('BANK TAKE DISCARDED');}}
        }else if(bankRecovery.selection===0){setParamBlocking('bank_take_retry',bankRecovery.token);state.view='main';showMessage('RETRYING BANK SAVE');}
        else if(bankRecovery.selection===1)state.view='main';
        else {bankRecovery.confirm=true;bankRecovery.selection=0;}
        return true;
    }
    // Keep the recovery menu stable. Pads and safety controls still reach DSP.
    return number!==MoveDelete&&number!==MoveShift&&number!==MovePlay;
}

const projectUi={data:null,files:[],mode:'menu',selection:0,confirm:0,pending:null};
function readProject() {
    try {
        const data=JSON.parse(String(getParam('project_state','{}')));if(data.v!==1)return;
        projectUi.data=data;
        if(data.job===0){
            const files=JSON.parse(String(getParam('project_files','[]')));
            if(Array.isArray(files)&&files.length<=32&&files.every(f=>typeof f==='string'))projectUi.files=files;
            if(projectUi.pending){projectUi.mode=projectUi.pending;projectUi.pending=null;}
            if(projectUi.mode==='list')projectUi.selection=clamp(projectUi.selection,0,Math.max(0,projectUi.files.length-1));
        }
    } catch(_){}
}
function projectCommand(key,value,after='complete') {
    projectUi.pending=after;setParamBlocking('project_'+key,value);readProject();
}
function openProjects() {
    state.view='projects';projectUi.mode='menu';projectUi.selection=0;projectUi.confirm=0;
    projectCommand('scan',0,'menu');pollMidiControls();draw();
}
function drawProjects() {
    clearDisplay();const data=projectUi.data||{},busy=data.job>0;
    drawTitleWithFraction('THRU PROJECTS',projectUi.mode==='list'?(projectUi.selection+1)+'/'+Math.max(1,projectUi.files.length):'');drawRect(0,8,128,1,1);
    if(busy){
        print(1,14,data.action===3?'READING PROJECTS':data.job===1?'SAVING CURRENT SET':data.action===1?'EXPORTING PROJECT':'IMPORTING PROJECT',1);
        const progress=data.total>0?clamp(data.bytes/data.total,0,1):0;
        print(1,28,Math.round(progress*100)+'%  '+(data.bytes/1048576).toFixed(1)+' MB',1);
        drawRect(1,40,126,6,1);drawRect(2,41,124,4,0);drawRect(2,41,Math.round(124*progress),4,1);
        print(1,57,'BACK: CANCEL',1);return;
    }
    if(data.reopen){
        print(1,15,data.ok?'PROJECT IMPORTED':'CHECK PROJECT COMMIT',1);
        print(1,29,'REOPEN THRU TO PLAY',1);print(1,57,'CLICK / BACK: EXIT',1);return;
    }
    if(projectUi.mode==='confirm'){
        print(1,12,'OPEN THIS PROJECT?',1);print(1,22,'BACKS UP CURRENT SET',1);
        ['CANCEL','IMPORT PROJECT'].forEach((t,i)=>{const y=35+i*10;if(i===projectUi.confirm)drawRect(0,y-1,128,9,1);print(1,y,t,i===projectUi.confirm?0:1);});
        print(1,57,'CLICK SELECT; BACK NO',1);return;
    }
    if(projectUi.mode==='complete'){
        print(1,15,String(data.message||'PROJECT READY').slice(0,21),1);
        print(1,30,data.ok?'SEE THRU PROJECTS':'CURRENT SET RETAINED',1);print(1,57,'CLICK / BACK: MENU',1);return;
    }
    const rows=projectUi.mode==='list'?projectUi.files:['EXPORT PROJECT','IMPORT PROJECT'];
    if(!rows.length)print(1,20,'NO .THRU FILES FOUND',1);
    const start=clamp(projectUi.selection-1,0,Math.max(0,rows.length-4));
    for(let row=0;row<4&&start+row<rows.length;++row){const i=start+row,y=12+row*10;
        if(i===projectUi.selection)drawRect(0,y-1,128,9,1);print(1,y,String(rows[i]).slice(0,21),i===projectUi.selection?0:1);}
    print(1,57,projectUi.mode==='list'?'<> PAGE  CLICK OPEN':'ALL CLIPS AND SAMPLES',1);
}
function handleProjects(number,value) {
    const data=projectUi.data||{};
    if(state.view!=='projects'&&!data.reopen)return false;
    if(number===MovePlay&&value>0&&state.iconPressed.has(MoveDelete)){triggerPanic();return true;}
    if(data.reopen){if(value>0&&(number===MoveMainButton||number===MoveBack)&&typeof host_exit_module==='function')host_exit_module();return true;}
    if(data.job>0){if(number===MoveShift||number===MoveDelete)return false;if(number===MoveBack&&value>0)projectCommand('cancel',1,'menu');return true;}
    if((number===MoveLeft||number===MoveRight)&&value>0&&projectUi.mode==='list'){
        projectUi.selection=0;projectCommand('scan',Math.max(0,data.page+(number===MoveRight?1:-1)),'list');return true;
    }
    if(number===MoveMainKnob||number===MoveUp||number===MoveDown){
        if(number!==MoveMainKnob&&value<=0)return true;const delta=number===MoveMainKnob?decodeDelta(value):number===MoveUp?-1:1;
        if(projectUi.mode==='confirm')projectUi.confirm=clamp(projectUi.confirm+delta,0,1);
        else projectUi.selection=clamp(projectUi.selection+delta,0,projectUi.mode==='list'?Math.max(0,projectUi.files.length-1):1);
        return true;
    }
    if(number===MoveBack||number===MoveMenu){if(value>0){
        if(projectUi.mode==='confirm')projectUi.mode='list';
        else if(projectUi.mode!=='menu'){projectUi.mode='menu';projectUi.selection=0;}
        else if(!returnToPagePicker())state.view='main';
    }return true;}
    if(number===MoveMainButton&&value>0){
        if(projectUi.mode==='menu'){
            if(projectUi.selection===0)projectCommand('export',1);
            else {projectUi.selection=0;projectCommand('scan',0,'list');}
        }else if(projectUi.mode==='list'&&projectUi.files.length){projectUi.confirm=0;projectUi.mode='confirm';}
        else if(projectUi.mode==='confirm'){
            if(projectUi.confirm)projectCommand('import',projectUi.selection);
            else projectUi.mode='list';
        }else {projectUi.mode='menu';projectUi.selection=0;}
        return true;
    }
    return number!==MoveDelete&&number!==MoveShift&&number!==MovePlay;
}

function drawBankMain() {
    clearDisplay();const bank=state.bank||{selected:0,slots:Array(16).fill(0)};
    print(1,0,(state.splitFocus===0?'A '+(ALGORITHMS[state.algorithm]||'ENGINE'):'B PAD '+String(bank.selected+1).padStart(2,'0'))+' BANK',1);
    drawRect(0,8,128,1,1);
    if(state.splitFocus===0) {
        const window=splitWaveWindow(0);
        for(let lane=0;lane<4;++lane) {
            const y=14+lane*9;print(2,y-3,String(lane+1),1);drawWaveformLane(lane,window,y);
        }
        const markers=[];
        for(let v=0;v<4;++v)if(state.playheads[v]>=0) {
            const x=waveformX(state.playheads[v],window),position=state.stackPositions[v];
            if(x>=0 && position>=0) {
                const row=clamp(Math.round(position*3),0,3);
                drawRect(x,10+row*9,1,8,1);markers.push([x,14+position*27]);
            }
        }
        for(const [x,y] of markers)clearStackMarker(x,y);
        for(const [x,y] of markers)drawStackMarker(x,y);
    } else {
        const wave=state.bankWave||[],bins=Math.floor(wave.length/2);
        const region=[PARAMS.bank_start.value,PARAMS.bank_end.value];
        for(let x=0;x<90;++x) {
            const bin=Math.min(bins-1,Math.floor((region[0]+x/89*(region[1]-region[0]))*(bins-1)));
            const low=wave[bin*2]||0,high=wave[bin*2+1]||0;
            const y1=30-Math.round(high*2),y2=30-Math.round(low*2);
            drawRect(1+x,y1,1,Math.max(1,y2-y1+1),1);
        }
        for(let v=4;v<8;++v)if(state.voicePads[v]>=0&&padLocal(state.voicePads[v])===bank.selected&&state.playheads[v]>=0)
            drawRect(1+Math.round(clamp((state.playheads[v]-region[0])/(region[1]-region[0]),0,1)*89),12,1,35,1);
        for(let slot=0;slot<16;++slot) {
            const x=95+(slot%4)*8,y=13+(3-Math.floor(slot/4))*9,status=bank.slots[slot];
            drawRect(x,y,6,6,status?1:0);
            if(!status){drawRect(x,y,6,1,1);drawRect(x,y+5,6,1,1);drawRect(x,y,1,6,1);drawRect(x+5,y,1,6,1);}
            if(slot===bank.selected)drawRect(x,y+7,6,1,1);
            if(status===4)drawRect(x+2,y+2,2,2,0);
        }
    }
    const status=bank.slots[bank.selected]||0;
    const hint=state.deleteHeld?'X+PAD: CLEAR BANK':status===2?'RELEASE TO FINISH':status===3?'SAVING BANK TAKE':status===0?'HOLD EMPTY PAD: REC':'HOLD PLAY; SH:LATCH';
    print(1,55,(bank.failed?'SAVE FAILED: CLICK':errorFooter(state.messageTicks>0?state.message:hint)).slice(0,21),1);
}

function drawMain() {
    if(layersUi.active&&!layersUi.wave){drawLayersMain();return;}
    if(state.bankMode){drawBankMain();return;}
    if(state.splitEnabled){drawSplitMain();return;}
    clearDisplay();
    const algorithm = ALGORITHMS[state.algorithm] || ALGORITHMS[0];
    const recordTag = state.clipRecording ? ' DATA REC'
        : (state.recording ? (state.limited ? ' LIM' : ' AUDIO') : '');
    const displayedAlgorithm = voiceEngineListEnabled()?'LIST':state.algorithm === 6 && state.glitchBufferLocked
        ? 'GLCH LOCK' : algorithm;
    const title='THRU '+displayedAlgorithm+' '+clipLabel(state.activeClip)+' V'+state.voiceCount+recordTag;
    print(1,0,title.slice(0,21),1);

    const left = 14;
    const width = 114;
    const top = 10;
    const rowHeight = 10;
    const window=normalizedWaveWindow([state.fieldStart,state.fieldEnd]);
    for (let lane = 0; lane < 4; lane++) {
        const center = top + lane * rowHeight + 5;
        const selected = state.lane === lane + 1;
        if (selected) drawRect(0, center - 4, 2, 8, 1);
        print(3, center - 4, String(lane + 1), 1);
        const weightHeight = Math.round(state.weights[lane] * 7);
        if (weightHeight > 0) {
            drawRect(9, center + 4 - weightHeight, 2, weightHeight, 1);
        }
        drawWaveformLane(lane,window,center);
        if (selected && (state.mask&(1<<lane)) && state.regionLane===state.lane && state.regions.length > 2) {
            for (let region = 1; region < state.regions.length - 1; region++) {
                const x=waveformX(state.regions[region],window);
                if(x<0)continue;
                drawRect(x, center - 4, 1, 2, 1);
                drawRect(x, center + 3, 1, 2, 1);
            }
        }
    }

    const wavesetFocus = !voiceEngineListEnabled()&&state.algorithm === 8 ? focusedWavesetVoice() : null;
    const displayedPlayheads = wavesetFocus
        ? state.wavesetVoices.map((voice) => voice
            ? voice.sourcePosition : -1)
        : state.playheads;
    if (state.mask !== 0) {
        if(voiceEngineListEnabled()||state.engineListVoices) {
            const markers=[];
            for(let slot=0;slot<8;++slot) {
                const algorithm=state.voiceAlgorithms[slot],cut=state.cutupsVoices[slot],ws=state.wavesetVoices[slot];
                const head=algorithm===3?(cut?.gain>0?cut.position:-1):algorithm===8?(ws?.sourcePosition??-1):state.playheads[slot];
                const lane=algorithm===3?(cut?.lane??-1):state.stackPositions[slot]*3;
                const x=waveformX(head,window);
                if(algorithm===14)drawPulsarFragment(slot,lane,window);
                if(x>=0&&lane>=0&&lane<=3){drawRect(x,top,1,40,1);markers.push([x,top+5+lane*rowHeight]);}
                const outgoing=algorithm===3&&cut?.outgoingGain>0?cut.outgoingPosition:algorithm===8?ws?.secondarySourcePosition:-1;
                const ox=waveformX(outgoing??-1,window);
                if(ox>=0){drawDashedPlayhead(ox,top,40);if(algorithm===3&&cut.outgoingLane>=0)markers.push([ox,top+5+cut.outgoingLane*rowHeight]);}
            }
            for(const [x,y] of markers)clearStackMarker(x,y);
            for(const [x,y] of markers)drawStackMarker(x,y);
        } else if (state.algorithm === 3) {
            // Match the shared full-height playheads and hollow stack
            // markers, using the actual source readers for each cut.
            const markers = [];
            for (const voice of state.cutupsVoices) {
                if (!voice || voice.outgoingLane < 0 || voice.outgoingPosition < 0
                    || voice.outgoingGain <= 0 || (state.mask & (1 << voice.outgoingLane)) === 0) continue;
                const x=waveformX(voice.outgoingPosition,window);
                if(x<0)continue;
                drawDashedPlayhead(x, top, 40);
                markers.push([x, top + 5 + voice.outgoingLane * rowHeight]);
            }
            for (const voice of state.cutupsVoices) {
                if (!voice || voice.lane < 0 || voice.position < 0
                    || voice.gain <= 0 || (state.mask & (1 << voice.lane)) === 0) continue;
                const x=waveformX(voice.position,window);
                if(x<0)continue;
                drawRect(x, top, 1, 40, 1);
                markers.push([x, top + 5 + voice.lane * rowHeight]);
            }
            for (const [x,y] of markers) clearStackMarker(x,y);
            for (const [x,y] of markers) drawStackMarker(x,y);
        } else {
            if (wavesetFocus) {
                for (const voice of state.wavesetVoices) {
                    if (!voice || voice.secondarySourcePosition < 0) continue;
                    const playX=waveformX(voice.secondarySourcePosition,window);
                    if(playX<0)continue;
                    drawDashedPlayhead(playX, top, 40);
                }
            }
            if(state.algorithm===14)for(let slot=0;slot<8;++slot)drawPulsarFragment(slot,state.stackPositions[slot]*3,window);
            for (const playhead of displayedPlayheads) {
                if (playhead < 0) continue;
                const playX=waveformX(playhead,window);
                if(playX<0)continue;
                drawRect(playX, top, 1, 40, 1);
            }
            for (let voice = 0; voice < displayedPlayheads.length; voice++) {
                const playhead = displayedPlayheads[voice];
                const stackPosition = state.stackPositions[voice];
                if (playhead < 0 || stackPosition < 0) continue;
                const playX=waveformX(playhead,window);
                if(playX<0)continue;
                const stackY = top + 5 + stackPosition * rowHeight * 3;
                clearStackMarker(playX, stackY);
            }
            for (let voice = 0; voice < displayedPlayheads.length; voice++) {
                const playhead = displayedPlayheads[voice];
                const stackPosition = state.stackPositions[voice];
                if (playhead < 0 || stackPosition < 0) continue;
                const playX=waveformX(playhead,window);
                if(playX<0)continue;
                const stackY = top + 5 + stackPosition * rowHeight * 3;
                drawStackMarker(playX, stackY);
            }
        }
    }

    const railFocus = drawWavesetRail(left, width);
    if (state.recording) {
        drawRect(left, railFocus ? 53 : 51,
            Math.max(1, Math.round(width * state.meter)), railFocus ? 1 : 2, 1);
    }
    if (state.clipDuration > 0) {
        const clipY = railFocus ? 54 : 53;
        drawRect(0, clipY, 128, 1, 0);
        drawRect(0, clipY,
            Math.max(1, Math.round(128 * state.clipPosition)), 1, 1);
    }
    print(1,55,mainFooter(railFocus),1);
}

function mainFooter(railFocus) {
    if(state.patternFade.active&&state.saveStatus!==4) {const f=state.patternFade;const name=i=>'ABCD'[Math.floor(clamp(i,0,63)/16)]+String(clamp(i,0,63)%16+1).padStart(2,'0');return name(f.from)+'>'+name(f.to)+' '+Math.round(f.position)+'%'+(f.held?' HOLD':'');}

    const clipStatus = state.clipNoteFull || state.clipPointFull
        ? 'CLIP FULL N' + state.clipNoteCount + ' G' + state.clipPointCount
        : (state.clipLaunchPending ? 'QUEUED ' + PARAMS.launch_quantize.labels[
            Math.round(PARAMS.launch_quantize.value)]
            : ((state.clipPlaying ? 'PLAY ' : 'STOP ')
                + (state.clipTiming ? state.clipBeats.toFixed(1) + 'b SYNC '
                    : (state.clipDuration / 1000).toFixed(1) + 's ')
                + state.clipRate.toFixed(2) + 'x T' + state.clipTakeCount));
    const mutedParts=state.splitEnabled?Array.from({length:partCount()},(_,p)=>p).filter((part)=>state.splitParts[part]?.mute):[];
    const muteHint=mutedParts.length
        ? mutedParts.map(partName).join('+')+' MUTED: '
            +(state.splitParts[state.splitFocus]?.mute?'PRESS MUTE':'CAPTURE MUTE')
        : '';
    const silent=state.splitEnabled&&state.mixReasons[state.splitFocus]>=2&&state.mixReasons[state.splitFocus]<=3
        ? (partName()+' ')+(state.mixReasons[state.splitFocus]===2?'LEVEL 0: ENGINE KNOB8':'FADED OUT: BLND KNOB8') : '';
    if(state.saveStatus===4)return saveNotice().slice(0,21);
    const footer = state.messageTicks > 0
        ? state.message
        : (([9,10,12].includes(state.algorithm)&&state.spectralPending?(state.algorithm===12?'PITCH PREPARING':'SPECTRAL PREPARING'):'') || muteHint || silent || (state.algorithm === 6 && state.glitchBufferLocked
            ? 'GLITCH BUFFER LOCKED'
            : (state.laneSourceState.startsWith('MISSING')
            ? 'MISSING SHIFT+TRK FIX'
            : (state.scanning ? 'SCANNING WAVEFORM'
                : (state.stackScanPaused ? 'STACK HOLD - LOOP RUN'
                    : (state.recordedStackHold ? 'RECORDED STACK HOLD'
                    : (railFocus ? wavesetStatus(railFocus)
                    : (state.clipDuration > 0 ? clipStatus
                    : state.laneSourceState + ' MENU'))))))));
    return String(footer).slice(0,21);
}

function decodeFxOrder(code) {
    const available = [0,1,2,3,4,5];
    return [120,24,6,2,1,1].map((factor) => {
        const index = Math.floor(code / factor); code %= factor;
        return available.splice(index, 1)[0];
    });
}
function encodeFxOrder(order) {
    const available = [0,1,2,3,4,5];
    return order.reduce((code, block, slot) => {
        const index = available.indexOf(block); available.splice(index, 1);
        return code + index * [120,24,6,2,1,1][slot];
    }, 0);
}
function routePresets() {return state.splitEnabled?[[2,5,4],[2,4,5],[4,2,5]]:FX_ROUTE_PRESETS;}
function routeCode(order) {return encodeFxOrder(state.splitEnabled?[0,1,3,...order]:order);}
function syncFxOrder() {
    const values = String(getParam('effect_order_state',
        Math.round(PARAMS.effect_order.value) + ',0,0')).split(',').map(Number);
    const code = clamp(Math.round(values[0] || 0), 0, 719);
    const clipChanged = state.fxRouteClip !== state.activeClip;
    // A lifted block is protected even before its first move. Audio/recorded
    // automation can advance behind the editor until the user drops or cancels.
    if (clipChanged || (!state.fxRoutePending && !state.fxRouteGrabbed)) {
        state.fxOrder = state.splitEnabled?decodeFxOrder(code).filter((block)=>[2,4,5].includes(block)):decodeFxOrder(code);
        state.fxSavedOrder = state.splitEnabled?routeCode(state.fxOrder):code;
        state.fxRoutePending = false;
        state.fxRouteClip = state.activeClip;
        if (clipChanged) {
            state.fxRouteSelected = 0;
            state.fxRouteGrabbed = false;
        }
    }
    state.fxRouteTransition = values[2] || 0;
}
function cancelFxOrder(announce = true) {
    state.fxRouteGrabbed = false;
    state.fxRoutePending = false;
    syncFxOrder();
    if (announce) showActionMessage('ORDER CANCELLED');
}
function applyFxOrder() {
    if (state.fxRoutePending) {
        const code = routeCode(state.fxOrder);
        setParamBlocking('effect_order', code);
        PARAMS.effect_order.value = code;
        if (state.clipRecording) setParam('clip_automation', 'effect_order');
        state.fxSavedOrder = state.splitEnabled?routeCode(state.fxOrder):code;
        state.fxRoutePending = false;
        showActionMessage('ORDER APPLIED');
    }
    state.fxRouteGrabbed = false;
}
function turnFxOrder(delta) {
    if (delta === 0) return;
    const from = state.fxRouteSelected;
    const to = clamp(from + (delta > 0 ? 1 : -1), 0, state.fxOrder.length-1);
    if (state.fxRouteGrabbed && from !== to) {
        const block = state.fxOrder.splice(from, 1)[0];
        state.fxOrder.splice(to, 0, block);
        state.fxRoutePending = routeCode(state.fxOrder) !== state.fxSavedOrder;
    }
    state.fxRouteSelected = to;
    state.actionMessageTicks = 0;
}
function clickFxOrder() {
    if (state.fxRouteGrabbed || state.fxRoutePending) applyFxOrder();
    else { state.fxRouteGrabbed = true; state.actionMessageTicks = 0; }
}
function adjustFxOrder(index, delta) {
    // Only knob 7 has a role here. The main encoder owns list editing.
    if (index !== 6 || delta === 0) return;
    delta = parameterEncoderDelta(index, 'fx_order_preset', delta);
    if (delta === 0) return;
    const current = routePresets().findIndex((order) =>
        routeCode(order) === routeCode(state.fxOrder));
    const next = clamp((current < 0 ? 0 : current) + delta, 0, 2);
    state.fxOrder = [...routePresets()[next]];
    state.fxRouteGrabbed = false;
    state.fxRoutePending = routeCode(state.fxOrder) !== state.fxSavedOrder;
    state.fxRouteSelected = 0;
    state.actionMessageTicks = 0;
}
function drawFxOrder() {
    const preset = routePresets().findIndex((order) =>
        routeCode(order) === routeCode(state.fxOrder));
    const presetName = preset < 0 ? 'CUST' : FX_ROUTE_NAMES[preset];
    const heading = (state.clipRecording ? 'REC FX' : (state.clipPlaying ? '> FX ORD' : 'FX ORDER'))
        + ' 7:' + presetName;
    drawSettingsTitle(state.actionMessageTicks > 0 ? state.actionMessage : heading);
    drawRect(0, 8, 128, 1, 1);
    for (let index = 0; index < state.fxOrder.length; ++index) {
        const y = state.splitEnabled?12+index*14:9+index*8;
        const selected = index === state.fxRouteSelected;
        const marker = selected ? (state.fxRouteGrabbed ? '*' : '>') : ' ';
        if (selected) drawRect(0, y, 128, 8, 1);
        print(1, y, marker + (index + 1) + ' ' + FX_BLOCK_NAMES[state.fxOrder[index]], selected ? 0 : 1);
    }
    const footer = state.fxRouteGrabbed ? 'TURN MOVE CLICK SET'
        : (state.fxRoutePending ? 'PRESET CLICK SET' : 'TURN PICK CLICK LIFT');
    print(1, 57, errorFooter(footer), 1);
}

function splitFxPages() {
    const part=partName();
    return [
        {title:isQuad()?'ALL PART SENDS':'PART SENDS + PRIORITY',page:isQuad()?QUAD_SEND_PAGE:SPLIT_MIX_PAGE},
        ...(!isQuad()?[{title:'PRIORITY DUCK',page:PRIORITY_PAGE}]:[]),
        {title:'DELAY',page:DELAY_RETURN_PAGE},
        {title:'REVERB',page:REVERB_PAGE},
        {title:part+' COLOR INSERT',page:PART_COLOR_PAGE},
        {title:part+' STEREO / ROTARY',page:STEREO_ROTARY_PAGE},
        {title:'SHARED TAPE',page:TAPE_PAGE},
        {title:'EFFECT ORDER',page:FX_ORDER_PAGE}
    ];
}

function syncSplitFxValues() {
    for(const key of [...SPLIT_MIX_PAGE.keys,...PRIORITY_PAGE.keys,...QUAD_SEND_PAGE.keys,'split_level_a','split_level_b','split_level_c','split_level_d']) {
        const control=PARAMS[key];
        control.value=clamp(getNumber(key,control.value),control.min,control.max);
    }
}

function openSplitSends() {
    syncSplitFxValues();state.view='settings';state.fxMenuReturn=false;
    state.settingsPage=parameterPages().indexOf(isQuad()?QUAD_SEND_PAGE:SPLIT_MIX_PAGE);
    state.actionMessageTicks=0;resetParameterEncoderMotion();
}

function openSplitFxMenu() {
    syncSplitFxValues();state.view='fxMenu';state.fxMenuReturn=false;
    state.fxMenuSelection=clamp(state.fxMenuSelection,0,splitFxPages().length-1);
    state.actionMessageTicks=0;
}

function moveSplitFxMenu(delta) {
    if(delta)state.fxMenuSelection=clamp(state.fxMenuSelection+(delta>0?1:-1),0,splitFxPages().length-1);
}

function activateSplitFxPage() {
    const entry=splitFxPages()[state.fxMenuSelection];
    state.settingsPage=parameterPages().findIndex((page)=>page.title===entry.page.title);
    state.view='settings';state.fxMenuReturn=true;state.actionMessageTicks=0;
    resetParameterEncoderMotion();
}

function splitSendFooter() {
    if(isQuad())return 'RETURNS: DELAY/REVERB';
    const delayOff=PARAMS.effect_mix.value<=0 && (PARAMS.split_delay_a.value>0||PARAMS.split_delay_b.value>0);
    const reverbOff=PARAMS.effect_reverb_mix.value<=0 && (PARAMS.split_reverb_a.value>0||PARAMS.split_reverb_b.value>0);
    if(delayOff&&reverbOff)return 'RETURNS OFF: CLICK';
    if(delayOff)return 'DLY RETURN OFF: CLICK';
    if(reverbOff)return 'REV RETURN OFF: CLICK';
    return 'CLICK: FX PAGES';
}

function drawSplitFxMenu() {
    clearDisplay();
    const entries=splitFxPages();
    drawTitleWithFraction('FX PAGES EDIT '+partName(),
        (state.fxMenuSelection+1)+'/'+entries.length);
    drawRect(0,8,128,1,1);
    const start=clamp(state.fxMenuSelection-4,0,entries.length-5);
    for(let row=0;row<5;++row) {
        const index=start+row,selected=index===state.fxMenuSelection,y=10+row*9;
        if(selected)drawRect(0,y,128,8,1);
        print(1,y,(selected?'>':' ')+entries[index].title,selected?0:1);
    }
    print(1,57,errorFooter('TURN PICK CLICK OPEN'),1);
}

function currentSettingsPage() {
    const pages = parameterPages();
    state.settingsPage = clamp(state.settingsPage | 0, 0, pages.length - 1);
    return pages[state.settingsPage];
}

function voiceEngineCount(){return state.splitEnabled?(isQuad()?2:4):8;}
function voiceEngineListEnabled(){return !sequenceUi.active&&PARAMS.voice_engine_mode.value!==0;}
function parseVoiceEngineState(raw){
    const values=String(raw).split(',').map(Number);
    if(values.length!==18||values.some(v=>!Number.isFinite(v))||values.slice(0,13).some(v=>!Number.isInteger(v))||values[0]<0||values[0]>1||values[1]<0||values[1]>5||values[2]<1||values[2]>8||values.slice(3,11).some(v=>v<0||v>=ALGORITHMS.length)||values[11]<0||values[11]>1||values[12]<0||values[12]>2||values[13]<0||values[13]>100||!Number.isInteger(values[14])||values[14]<-2||values[14]>2||values.slice(15).some(v=>v< -100||v>100))return;
    const previous=state.view==='settings'&&values[0]!==PARAMS.voice_engine_mode.value?parameterPages()[state.settingsPage]?.title:null;
    for(const [i,key] of ['voice_engine_mode','voice_engine_order','voice_engine_edit',...VOICE_ENGINES_PAGE.keys].entries())PARAMS[key].value=values[i];
    state.engineListVoices=values[11]===1;
    ['voice_engine_assign','voice_engine_depth','voice_engine_octave','voice_engine_position','voice_engine_stack','voice_engine_pan'].forEach((k,i)=>PARAMS[k].value=values[i+12]);
    if(!voiceEngineListEnabled())state.engineListReturn=false;
    if(previous){const index=parameterPages().findIndex(p=>p.title===previous);state.settingsPage=index>=0?index:0;}
}
function editVoiceEngineSlot(delta){
    if(!delta)return;
    const title=currentSettingsPage().title;
    setParamBlocking('voice_engine_edit',clamp(Math.round(PARAMS.voice_engine_edit.value)+(delta>0?1:-1),1,voiceEngineCount()));
    refreshEditorSnapshot();
    state.settingsPage=parameterPages().findIndex(p=>p.title===title);
    resetParameterEncoderMotion();
}
function openVoiceEngineEditor(){
    state.engineListReturn=true;state.engineListReturnPage=currentSettingsPage().title;
    const detail=playbackPagesForAlgorithm(state.algorithm,Math.round(PARAMS.motion_sound.value)).find(p=>p.title!==STACK_PATH_PAGE.title);
    const index=parameterPages().findIndex(p=>p.title===detail?.title);
    state.settingsPage=Math.max(0,index);
    resetParameterEncoderMotion();
}
function parameterPagesBase() {
    let enginePages=playbackPagesForAlgorithm(state.algorithm,Math.round(PARAMS.motion_sound.value));
    if(!isQuad() && Math.round(PARAMS.lane_path.value)===8)
        enginePages=enginePages.flatMap(page=>page.title===STACK_PATH_PAGE.title
            ? [{...page,keys:page.keys.map(key=>['lane_path_shape','lane_path_skew','lane_path_curve'].includes(key)?null:key)},PATH_EDITOR_PAGE]
            : [page]);
    enginePages=enginePages.map(p=>p.title==='PRISM'?{...p,keys:p.keys.map(k=>k==='prism_decay'&&Math.round(PARAMS.prism_mode.value)!==4?null:k)}:p.title==='MORPH'?{...p,keys:p.keys.map(k=>k==='morph_frequency'&&Math.round(PARAMS.morph_mode.value)===1?null:k)}:p.title==='TEXTURE'?{...p,keys:p.keys.map(k=>k==='texture_group'&&Math.round(PARAMS.texture_mode.value)===0?null:k)}:p);
    const engine=state.algorithm===3?CUTUPS_ENGINE_PAGE:state.algorithm===4||state.algorithm===5?DIRECT_ENGINE_PAGE:ENGINE_PAGE;
    const pads={...PADS_SCALE_PAGE,keys:PADS_SCALE_PAGE.keys.map(k=>k==='voice_engine_order'?(voiceEngineListEnabled()?(PARAMS.voice_engine_assign.value===0?k:null):((isQuad()?PARAMS.pressure_mode.value===3:PARAMS.pressure_mode.value!==0)?'voice_engine_depth':null)):k)};
    const lists=voiceEngineListEnabled()?[{...VOICE_ENGINES_PAGE,keys:VOICE_ENGINES_PAGE.keys.map((k,i)=>i<voiceEngineCount()?k:null)},
        {...VOICE_SETUP_PAGE,keys:VOICE_SETUP_PAGE.keys.map(k=>isQuad()&&k==='voice_engine_stack'?null:k)}]:[];
    if(layersUi.active)return layersPages(engine,pads,enginePages);
    if(!state.splitEnabled)return [engine,pads,MIDI_CONTROL_PAGE,MIDI_NOTES_PAGE,MIDI_MAPS_PAGE,...lists,AMP_ENVELOPE_PAGE,PLAY_GAIN_PAGE,...enginePages,
        MIX_REC_PAGE,EFFECTS_PAGE,STEREO_ROTARY_PAGE,TAPE_PAGE,REVERB_PAGE,FX_ORDER_PAGE,SAMPLE_PAGE,LOOP_PAGE,PATTERN_FADE_PAGE,TAKE_PAGE,CHAIN_PAGE,CHAIN_FADE_PAGE,ENTRY_FADE_PAGE,CHAIN_TOOLS_PAGE];
    const engineSplit={...engine,keys:engine.keys.map((key)=>key==='output_gain'?partLevelKey():isQuad()&&['manual_lane','lane_cycles','cut_lane_advance'].includes(key)?null:key)};
    if(isQuad()) {
        const pages=enginePages.filter(p=>p.title!==STACK_PATH_PAGE.title).map(p=>({...p,keys:p.keys.map(k=>['manual_lane','lane_cycles','cut_lane_advance','cut_lane_order','cut_poly_path'].includes(k)?null:k)}));
        const rec={...MIX_REC_PAGE,keys:['lane_'+(state.splitFocus+1)+'_gain',null,null,null,...MIX_REC_PAGE.keys.slice(4)]};
        return [engineSplit,pads,MIDI_CONTROL_PAGE,MIDI_NOTES_PAGE,MIDI_MAPS_PAGE,...lists,AMP_ENVELOPE_PAGE,PLAY_GAIN_PAGE,...pages,QUAD_SEND_PAGE,QUAD_SETUP_PAGE,rec,PART_COLOR_PAGE,STEREO_ROTARY_PAGE,
            {...DELAY_RETURN_PAGE,keys:['effect_delay','effect_feedback','effect_width',null,null,null,null,'effect_mix']},
            {...TAPE_PAGE,global:true},{...REVERB_PAGE,global:true},FX_ORDER_PAGE,SAMPLE_PAGE,
            LOOP_PAGE,PATTERN_FADE_PAGE,TAKE_PAGE,CHAIN_PAGE,CHAIN_FADE_PAGE,ENTRY_FADE_PAGE,CHAIN_TOOLS_PAGE];
    }
    if(state.bankMode && state.splitFocus===1)return [BANK_SAMPLE_PAGE,BANK_PADS_PAGE,PLAY_GAIN_PAGE,MIDI_CONTROL_PAGE,MIDI_NOTES_PAGE,MIDI_MAPS_PAGE,SPLIT_MIX_PAGE,PRIORITY_PAGE,SPLIT_SETUP_PAGE,PART_COLOR_PAGE,STEREO_ROTARY_PAGE,DELAY_RETURN_PAGE,{...TAPE_PAGE,global:true},{...REVERB_PAGE,global:true},FX_ORDER_PAGE,LOOP_PAGE,PATTERN_FADE_PAGE,TAKE_PAGE,CHAIN_PAGE,CHAIN_FADE_PAGE,ENTRY_FADE_PAGE,CHAIN_TOOLS_PAGE];
    const rec=state.sharedDual?MIX_REC_PAGE:{...MIX_REC_PAGE,keys:[state.splitFocus?'lane_3_gain':'lane_1_gain',state.splitFocus?'lane_4_gain':'lane_2_gain',null,null,...MIX_REC_PAGE.keys.slice(4)]};
    return [engineSplit,pads,MIDI_CONTROL_PAGE,MIDI_NOTES_PAGE,MIDI_MAPS_PAGE,...lists,AMP_ENVELOPE_PAGE,PLAY_GAIN_PAGE,...enginePages,SPLIT_MIX_PAGE,PRIORITY_PAGE,SPLIT_SETUP_PAGE,
        rec,PART_COLOR_PAGE,STEREO_ROTARY_PAGE,DELAY_RETURN_PAGE,{...TAPE_PAGE,global:true},
        {...REVERB_PAGE,global:true,keys:REVERB_PAGE.keys.map((key)=>key==='effect_reverb_mix'?null:key)},FX_ORDER_PAGE,SAMPLE_PAGE,LOOP_PAGE,PATTERN_FADE_PAGE,TAKE_PAGE,CHAIN_PAGE,CHAIN_FADE_PAGE,ENTRY_FADE_PAGE,CHAIN_TOOLS_PAGE];
}

function inactiveControl(key) {
    if(!key)return false;
    const direct=state.algorithm===4||state.algorithm===5;
    if(key==='join'&&direct&&Math.round(PARAMS.sample_mode.value)!==2)return true;
    if(key==='stretch_ratio'&&(state.algorithm===1||state.algorithm===4)
        &&PARAMS.lanes_stretch.value<.5)return true;
    if(key==='lane_variation'&&(isQuad()||sequenceUi.active))return true;
    if(sequenceUi.active&&['manual_lane','lane_cycles','cut_lane_advance',
        'cut_lane_order','cut_poly_path','lane_path','lane_blend',
        'lane_path_shape','lane_path_offset','lane_path_skew',
        'lane_path_curve','lane_slew'].includes(key))return true;
    return false;
}
function maskInactivePage(page) {
    if(!page.keys.some(inactiveControl))return page;
    return {...page,keys:page.keys.map(key=>inactiveControl(key)?null:key)};
}
function parameterPages(){const pages=parameterPagesBase();return (sequenceUi.active?sequencePages(pages):pages).map(maskInactivePage);}
function controlForKey(key) {
    if(key==='seq_child')return {...PARAMS.seq_child,max:PARAMS.seq_burst.value};
    if(key==='seq_cycle_hit')return {...PARAMS.seq_cycle_hit,max:PARAMS.seq_cycle_every.value};
    if(key==='seq_note'){const root=sequenceUi.data?.root||60;return {...PARAMS.seq_note,min:Math.max(-48,-root),max:Math.min(48,127-root)};}
    const stripPage=sequenceUi.active&&state.view==='settings'?currentSettingsPage():null;
    const stripField=SEQUENCE_STRIP_KEYS.indexOf(key);
    if(Number.isInteger(stripPage?.sequenceStrip)&&stripField>=0){
        const value=sequenceUi.data?.strips?.[stripPage.sequenceStrip]?.[stripField];
        return {...PARAMS[key],value:Number.isFinite(value)?value:PARAMS[key].value};
    }
    if(key?.startsWith('layers_fx')&&/^layers_fx(?:[0-9]|1[01])$/.test(key))return layersFxControl(key);
    const control = PARAMS[key];
    if(control&&key==='voice_engine_edit')return {...control,max:voiceEngineCount(),shortLabel:'EDIT'};
    if(control&&key==='voice_engine_assign')return {...control,shortLabel:'ASSN'};
    if(control&&key==='pressure_mode'&&isQuad())return {...control,value:control.value===3?3:0,min:0,max:3,step:3};
    if(control&&key==='algorithm'&&voiceEngineListEnabled())return {...control,shortLabel:'E'+Math.round(PARAMS.voice_engine_edit.value)};
    if(control&&key==='morph_amplitude'&&state.algorithm===10&&Math.round(PARAMS.morph_mode.value)===1)return {...control,label:'IMPRINT AMOUNT',shortLabel:'IMPR'};
    if(control&&key==='record_source')return {...control,max:state.splitEnabled?(isQuad()?5:3):1};
    if(control&&key==='split_focus')return {...control,max:isQuad()?3:1};
    if(control&&key==='midi_control_part')return {...control,max:isQuad()?3:state.splitEnabled?1:0};
    if (control && state.splitEnabled) {
        if (key==='effect_mix') return {...control,shortLabel:'DRTN',label:'DELAY RETURN %'};
        if (key==='effect_reverb_mix') return {...control,shortLabel:'RRTN',label:'REVERB RETURN %'};
        if (['split_level_a','split_level_b','split_level_c','split_level_d'].includes(key)) return {...control,shortLabel:sequenceUi.active?'LVL'+('abcd'.indexOf(key.slice(-1))+1):'LEVL',label:sequenceUi.active?'SEQUENCE LANE LEVEL':'PART LEVEL'};
    }
    if(control && key==='manual_lane' && state.splitEnabled&&!state.sharedDual) return {...control,max:2,value:clamp(control.value,1,2),...(state.algorithm===3?{label:'LANE',shortLabel:'LANE',integer:true,step:1,value:clamp(Math.round(control.value),1,2)}:{})};
    if (control && key === 'manual_lane' && state.algorithm === 3) {
        return { ...control, label: 'LANE', shortLabel: 'LANE',
            integer: true, step: 1, value: Math.round(control.value) };
    }
    return control;
}

function copyShortcutScope() {
    if(state.view==='clips'||(state.view==='settings'&&[LOOP_PAGE,TAKE_PAGE,PATTERN_FADE_PAGE].some(p=>p.title===currentSettingsPage().title)))return 'clip';
    if(!chainEditorActive())return null;
    return 'chain_clip';
}
function runCopyShortcut(scope,paste,target=state.activeClip) {
    if(state.recording||state.clipRecording){showMessage('STOP RECORDING FIRST');return;}
    if(scope==='clip') {
        setParamBlocking(paste?'clip_clipboard_paste':'clip_clipboard_copy',target);
        const error=String(getParam('last_error',''));
        if(!error&&paste){if(target<32)state.clipMaskLow|=1<<target;else state.clipMaskHigh|=1<<(target-32);}
        state.undoAvailable=getNumber('undo_available',0)!==0;
        showMessage(error||('CLIP '+clipLabel(target)+(paste?' PASTED':' COPIED')));rebuildLedQueue();
        if(!error&&paste&&target===state.activeClip)refreshEditorSnapshot();
    } else chainAction(paste?'chain_clip_paste':'chain_clip_copy',()=>{
        const entry=state.chainEntries[state.chainSlot];
        return entry?.enabled?'CLIP '+clipLabel(entry.clip)+(paste?' TO E'+String(state.chainSlot+1).padStart(2,'0'):' COPIED'):'NO CLIP ENTRY';
    });
}
function copyShortcutStep(step) {
    const gesture=state.copyShortcut;if(!gesture)return false;
    gesture.used=true;
    if(copyShortcutScope()!==gesture.scope){showMessage('COPY CANCELLED');return true;}
    if(gesture.scope==='clip')runCopyShortcut('clip',state.shiftHeld,state.clipBank*16+step);
    else {
        state.chainSlot=state.chainPage*16+step;
        setParamBlocking('chain_slot',state.chainSlot+1);
        runCopyShortcut('chain_clip',state.shiftHeld);
    }
    return true;
}
function chainEditorActive() {return state.view==='chain'||(state.view==='settings'&&(currentSettingsPage()===CHAIN_PAGE||!!currentSettingsPage().chainFade||currentSettingsPage().chainTools));}
function activeControlKeys() {
    if(sequenceUi.active&&state.view==='main')return SEQUENCE_LANE_PAGE.keys;
    if(state.view==='layerFxPicker')return Array(8).fill(null);
    if(customMidiViews.includes(state.view)||['projects','bankRecovery'].includes(state.view))return Array(8).fill(null);
    if(state.view==='sliceEdit')return Array(8).fill(null);
    if(state.view==='library')return [null,null,null,null,null,null,'preview_route','preview_level'];
    if(layersUi.active && state.view==='main'&&!layersUi.wave)return LAYERS_MAIN_KEYS;
    const keys=state.view === 'settings'
        ? currentSettingsPage().keys
        : state.bankMode&&state.splitFocus===1?BANK_SAMPLE_PAGE.keys
        : (MAIN_ENGINE_CONTROLS[state.algorithm] || MAIN_ENGINE_CONTROLS[0]).map((key,index)=>isQuad()?([3,4].includes(index)?null:index===7?partLevelKey():key):state.splitEnabled&&index===7?'split_blend':key);
    return keys.map(key=>inactiveControl(key)?null:key);
}

function noteName(value) {
    const names = ['C', 'C#', 'D', 'D#', 'E', 'F',
        'F#', 'G', 'G#', 'A', 'A#', 'B'];
    const note = Math.round(value);
    return names[((note % 12) + 12) % 12] + (Math.floor(note / 12) - 1);
}

function formatControlValue(control) {
    if(control.key.startsWith('seq_'))return sequenceValue(control);
    if(control?.key?.startsWith('layers_')&&control.format)return control.format(control.value);
    if (!control) return '';
    if(control.key==='bank_pan')return Math.abs(control.value)<.5?'CENTRE':(control.value<0?'L ':'R ')+Math.round(Math.abs(control.value))+'%';
    if(['bank_start','bank_end','bank_level'].includes(control.key))return Math.round(control.value*100)+'%';
    if(['voice_engine_position','voice_engine_stack','voice_engine_pan','voice_engine_depth'].includes(control.key))return (control.value>0&&control.key!=='voice_engine_depth'?'+':'')+Math.round(control.value)+'%';
    if(/^voice_engine_[1-8]$/.test(control.key))return ['MOTN','LANE','GRNS','CUTS','SMPL','SLIC','GLCH','WEAV','WSET','PRSM','MORF','TXTR','FORM','STOC','PLSR'][Math.round(control.value)]||'';
    if(/^(pattern|chain|entry)_fade_.*ms$/.test(control.key))return formatFadeTime(control.value);
    if(control.key==='pattern_fade_position')return control.value.toFixed(1)+'%';
    if(['prism_blur','prism_decay','morph_blur','texture_length','formant_smoothing'].includes(control.key))return formatFadeTime(control.value);
    if(['formant_tune','formant_keytrack','formant_rough','formant_sub','morph_amplitude','morph_frequency','morph_offset','prism_tilt','prism_smear','texture_scatter','texture_spread'].includes(control.key))return Math.round(control.value)+'%';
    if(control.key.startsWith('pulsar_')&&control.unit==='st')return control.value===0?'0':control.value.toFixed(control.value<10?2:1);
    if(control.key.startsWith('pulsar_')&&control.unit==='%')return Math.round(control.value)+'%';
    if(['pulsar_rate','pulsar_form','pulsar_size','pulsar_emit_mod_rate','pulsar_form_mod_rate'].includes(control.key))return control.value<10?control.value.toFixed(2):control.value<100?control.value.toFixed(1):String(Math.round(control.value));
    if(control.key.startsWith('stochastic_')&&control.unit==='%')return Math.round(control.value)+'%';
    if(control.key==='stochastic_evolution')return control.value===0?'HOLD':control.value>=1024?'CYC':control.value<10?control.value.toFixed(2):control.value<100?control.value.toFixed(1):String(Math.round(control.value));
    if(control.key==='chain_repeats'&&control.value===0)return 'WAIT';
    if([...SPLIT_SEND_KEYS,...QUAD_SEND_PAGE.keys].includes(control.key))return Math.round(control.value*100)+'%';
    if(control.key==='split_priority') {
        if(Math.abs(control.value)<.25)return 'OFF';
        return (control.value<0?'A > B ':'B > A ')+Math.abs(control.value).toFixed(1)+' dB';
    }
    if(control.key==='split_duck_threshold')return control.value.toFixed(1)+' dBFS';
    if(['split_duck_attack','split_duck_hold','split_duck_release'].includes(control.key))
        return control.value>=1000?(control.value/1000).toFixed(2)+' s':Math.round(control.value)+' ms';
    if(control.key==='split_duck_hpf')return Math.round(control.value)+' Hz';
    if(control.key==='split_duck_knee')return control.value.toFixed(1)+' dB';
    if(control.key==='split_blend')return 'A '+Math.round((1-control.value)*100)+' B '+Math.round(control.value*100);
    if (control.key === 'cut_lane_advance') {
        const n = Math.round(control.value);
        return n === 0 ? 'HOLD' : control.labels[n] + (n === 1 ? ' CUT' : ' CUTS');
    }
    if (control.key === 'waveset_direction') {
        const value = control.labels[clamp(Math.round(control.value), 0,
            control.labels.length - 1)] || '';
        const pending = state.wavesetVoices.some((voice) =>
            voice && voice.orderPending);
        return value + (pending ? ' *' : '');
    }
    if (control.key === 'effect_cross_pan') {
        if (control.value <= -99.5) return 'NORMAL';
        if (Math.abs(control.value) < 0.5) return 'MONO';
        if (control.value >= 99.5) return 'SWAPPED';
        return Math.round(control.value) + ' %';
    }
    if (control.key === 'effect_rotary_rate' && control.value < 0.05) {
        return 'STOP';
    }
    if (control.key === 'effect_rotary_ramp') {
        if (control.value < 1000) return Math.round(control.value) + ' ms';
        return (control.value / 1000).toFixed(
            control.value % 1000 === 0 ? 0 : 2) + ' s';
    }
    if (control.key === 'effect_rotary_balance') {
        if (control.value <= -99.5) return 'DRUM';
        if (Math.abs(control.value) < 0.5) return 'EVEN';
        if (control.value >= 99.5) return 'HORN';
        return (control.value > 0 ? 'HORN ' : 'DRUM ')
            + Math.abs(Math.round(control.value)) + ' %';
    }
    if (control.key === 'effect_rotary_crossover') {
        return Math.round(control.value) + ' Hz';
    }
    if (control.key === 'effect_tape_wow_rate'
        || control.key === 'effect_tape_flutter_rate') {
        return control.value.toFixed(control.value < 1 ? 2 : 1) + ' Hz';
    }
    if (control.key === 'effect_tape_hiss') {
        if (control.value <= control.min + 0.5) return 'OFF';
        return Math.round(control.value) + ' dBFS';
    }
    if (control.key === 'effect_tape_inertia') {
        const seconds = tapeInertiaSeconds(control.value);
        if (seconds <= 0) return 'OFF';
        if (seconds < 1) return Math.round(seconds * 1000) + ' ms';
        return seconds.toFixed(seconds < 2 ? 2 : 1) + ' s';
    }
    if(control.key==='bank_rate'||control.key==='bank_master_rate')return control.value.toFixed(control.value<1?2:1);
    if (control.key === 'bank_attack' || control.key === 'bank_release' || control.key === 'bank_fade' || control.key === 'amp_attack' || control.key === 'amp_decay'
        || control.key === 'amp_release') {
        if (control.value < 1000) return Math.round(control.value) + ' ms';
        return (control.value / 1000).toFixed(
            control.value % 1000 === 0 ? 0 : 2) + ' s';
    }
    if (control.key === 'amp_sustain') {
        return Math.round(control.value) + ' %';
    }
    if (control.key === 'hold_release_ms') {
        if (control.value <= 0) return 'MANUAL';
        if (control.value < 1000) return Math.round(control.value) + ' ms';
        return (control.value / 1000).toFixed(
            control.value % 1000 === 0 ? 0 : 2) + ' s';
    }
    if (control.note) return noteName(control.value);
    if (Array.isArray(control.labels)) {
        return control.labels[clamp(Math.round(control.value), 0,
            control.labels.length - 1)] || '';
    }
    if (control.integer) return String(Math.round(control.value));
    if (control.value >= 10) return control.value.toFixed(1);
    return control.value.toFixed(2);
}

function compactControlValue(control) {
    if(control?.key?.startsWith('seq_'))return sequenceValue(control);
    if(control?.key?.startsWith('layers_')&&control.format)return control.format(control.value);
    if(control?.key==='bank_pan')return Math.abs(control.value)<.5?'C':(control.value<0?'L':'R')+Math.round(Math.abs(control.value));
    if(control&&control.key.startsWith('pulsar_'))return formatControlValue(control).slice(0,5);
    if(control&&control.key.startsWith('stochastic_')&&(control.unit==='%'||control.key==='stochastic_evolution'))return formatControlValue(control).slice(0,4);
    if(control&&control.key==='voice_engine_assign')return ['TOUCH','DEGR','REG'][Math.round(control.value)];
    if(control&&['voice_engine_position','voice_engine_stack','voice_engine_pan','voice_engine_depth'].includes(control.key))return String(Math.round(control.value));
    if(control&&/^voice_engine_[1-8]$/.test(control.key))return formatControlValue(control);
    if(control&&control.key==='record_source')return ['IN','MIX','PT A','PT B','PT C','PT D'][clamp(Math.round(control.value),0,5)];

    if (control && control.key === 'effect_reverb_algo') return control.value < .5 ? 'PLAT' : 'ROOM';
    if (control && control.key === 'effect_reverb_decay') return control.value.toFixed(control.value < 10 ? 2 : 1);
    if (control && control.key === 'effect_reverb_damp') return control.value >= 1000
        ? (control.value / 1000).toFixed(control.value < 10000 ? 1 : 0) + 'k'
        : String(Math.round(control.value));
    if (!control) return '';
    if(['bank_start','bank_end','bank_level'].includes(control.key))return Math.round(control.value*100)+'%';
    if([...SPLIT_SEND_KEYS,...QUAD_SEND_PAGE.keys].includes(control.key))return Math.round(control.value*100)+'%';
    if(control.key==='split_priority')return Math.abs(control.value)<.25?'OFF':(control.value<0?'A>B':'B>A')+Math.round(Math.abs(control.value));
    if(control.key==='split_duck_threshold')return String(Math.round(control.value));
    if(['split_duck_attack','split_duck_hold','split_duck_release'].includes(control.key))return control.value>=1000?(control.value/1000).toFixed(1)+'s':String(Math.round(control.value));
    if(control.key==='split_duck_hpf'||control.key==='split_duck_knee')return String(Math.round(control.value));
    if(control.key==='split_duck_route')return control.value<.5?'DRY':'D+S';
    if(state.splitEnabled&&(control.key==='effect_mix'||control.key==='effect_reverb_mix'))return Math.round(control.value)+'%';
    if(control.key==='split_blend')return Math.round(control.value*100)+'%';
    if (control.key === 'waveset_direction') {
        const labels = ['FWD', 'REV', 'PEN', 'SHF'];
        const pending = state.wavesetVoices.some((voice) =>
            voice && voice.orderPending);
        return labels[clamp(Math.round(control.value), 0, 3)]
            + (pending ? '*' : '');
    }
    if (control.key === 'effect_cross_pan') {
        if (control.value <= -99.5) return 'NORM';
        if (Math.abs(control.value) < 0.5) return 'MONO';
        if (control.value >= 99.5) return 'SWAP';
        return String(Math.round(control.value)).slice(0, 4);
    }
    if (control.key === 'effect_rotary_rate' && control.value < 0.05) {
        return 'STOP';
    }
    if (control.key === 'effect_rotary_ramp') {
        if (control.value < 1000) return String(Math.round(control.value));
        const seconds = control.value / 1000;
        return (Number.isInteger(seconds) ? seconds.toFixed(0)
            : seconds.toFixed(1)).slice(0, 3) + 's';
    }
    if (control.key === 'effect_rotary_balance') {
        if (control.value <= -99.5) return 'DRUM';
        if (Math.abs(control.value) < 0.5) return 'EVEN';
        if (control.value >= 99.5) return 'HORN';
        return (control.value > 0 ? 'H' : 'D')
            + Math.abs(Math.round(control.value));
    }
    if (control.key === 'effect_rotary_crossover') {
        if (control.value < 1000) return String(Math.round(control.value));
        return (control.value / 1000).toFixed(
            control.value % 1000 === 0 ? 0 : 1) + 'k';
    }
    if (control.key === 'effect_tape_wow_rate'
        || control.key === 'effect_tape_flutter_rate') {
        return control.value.toFixed(control.value < 1 ? 2 : 1);
    }
    if (control.key === 'effect_tape_hiss') {
        if (control.value <= control.min + 0.5) return 'OFF';
        return String(Math.round(control.value));
    }
    if (control.key === 'effect_tape_inertia') {
        const seconds = tapeInertiaSeconds(control.value);
        if (seconds <= 0) return 'OFF';
        if (seconds < 1) {
            return String(Math.round(seconds * 1000)).slice(0, 3) + 'm';
        }
        return seconds.toFixed(seconds < 2 ? 1 : 0) + 's';
    }
    if(control.key==='bank_rate'||control.key==='bank_master_rate')return control.value.toFixed(control.value<1?2:1);
    if (control.key === 'bank_attack' || control.key === 'bank_release' || control.key === 'bank_fade' || control.key === 'amp_attack' || control.key === 'amp_decay'
        || control.key === 'amp_release') {
        if (control.value < 1000) return String(Math.round(control.value));
        const seconds = control.value / 1000;
        return (Number.isInteger(seconds) ? seconds.toFixed(0)
            : seconds.toFixed(seconds < 10 ? 1 : 0)).slice(0, 3) + 's';
    }
    if (control.key === 'amp_sustain') {
        return Math.round(control.value) + '%';
    }
    if (control.key === 'hold_release_ms') {
        if (control.value <= 0) return 'MAN';
        if (control.value < 1000) return String(Math.round(control.value));
        const seconds = control.value / 1000;
        return (Number.isInteger(seconds) ? seconds.toFixed(0)
            : seconds.toFixed(seconds < 10 ? 2 : 1)).slice(0, 3) + 's';
    }
    if (control.note) return noteName(control.value).slice(0, 4);
    if (control.key === 'scale') {
        return SCALE_ABBREVIATIONS[clamp(Math.round(control.value), 0,
            SCALE_ABBREVIATIONS.length - 1)];
    }
    if (Array.isArray(control.labels)) {
        const value = control.labels[clamp(Math.round(control.value), 0,
            control.labels.length - 1)] || '';
        if (control.key === 'lane_path') {
            return ['DOWN', 'UP', 'TRI', 'SINE', 'SDN', 'SUP', 'MAN', 'RAND', 'DRAW'][
                clamp(Math.round(control.value), 0, 8)];
        }
        return String(value).slice(0, 4);
    }
    if (control.integer) return String(Math.round(control.value)).slice(0, 4);
    if (control.value >= 100) return String(Math.round(control.value)).slice(0, 4);
    if (control.value >= 10) return control.value.toFixed(1).slice(0, 4);
    return control.value.toFixed(2).slice(0, 4);
}

function drawParameterCell(index, control, compact = false) {
    const column = index % 4;
    const row = Math.floor(index / 4);
    const x = column * 32;
    const y = (compact ? 11 : 13) + row * (compact ? 24 : 26);
    const label = String(control.shortLabel || SHORT_LABELS[control.key] || control.label).slice(0, 4);
    const value = compactControlValue(control);
    print(x + 3, y, label, 1);
    print(x + 3, y + 11, value, 1);
}

function drawReservedParameterCell(index, compact = false) {
    const column = index % 4;
    const row = Math.floor(index / 4);
    const x = column * 32 + 14;
    const y = (compact ? 18 : 20) + row * (compact ? 24 : 26);
    drawRect(x, y, 4, 4, 1);
    drawRect(x + 1, y + 1, 2, 2, 0);
}

function drawPixelLine(x0, y0, x1, y1) {
    let x = Math.round(x0);
    let y = Math.round(y0);
    const endX = Math.round(x1);
    const endY = Math.round(y1);
    const dx = Math.abs(endX - x);
    const sx = x < endX ? 1 : -1;
    const dy = -Math.abs(endY - y);
    const sy = y < endY ? 1 : -1;
    let error = dx + dy;
    while (true) {
        drawRect(x, y, 1, 1, 1);
        if (x === endX && y === endY) break;
        const twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x += sx;
        }
        if (twice <= dx) {
            error += dx;
            y += sy;
        }
    }
}

function envelopeSegmentWidth(value, maximum) {
    return 7 + 20 * Math.log(1 + clamp(value, 0, maximum))
        / Math.log(1 + maximum);
}

function drawAmpEnvelopePage() {
    const attack = PARAMS.amp_attack.value;
    const decay = PARAMS.amp_decay.value;
    const sustain = clamp(PARAMS.amp_sustain.value / 100, 0, 1);
    const release = PARAMS.amp_release.value;
    const widths = [
        envelopeSegmentWidth(attack, PARAMS.amp_attack.max),
        envelopeSegmentWidth(decay, PARAMS.amp_decay.max),
        22,
        envelopeSegmentWidth(release, PARAMS.amp_release.max),
    ];
    const scale = 118 / widths.reduce((sum, width) => sum + width, 0);
    const x0 = 4;
    const xAttack = x0 + widths[0] * scale;
    const xDecay = xAttack + widths[1] * scale;
    const xGate = xDecay + widths[2] * scale;
    const xRelease = 122;
    const baseline = 30;
    const peak = 11;
    const sustainY = Math.round(baseline - sustain * (baseline - peak));
    drawRect(x0, baseline, xRelease - x0 + 1, 1, 1);
    drawPixelLine(x0, baseline, xAttack, peak);
    drawPixelLine(xAttack, peak, xDecay, sustainY);
    drawPixelLine(xDecay, sustainY, xGate, sustainY);
    drawPixelLine(xGate, sustainY, xRelease, baseline);
    for (let y = 10; y <= baseline; y += 3) drawRect(xGate, y, 1, 2, 1);
    for (const point of [[xAttack, peak], [xDecay, sustainY],
        [xGate, sustainY]]) {
        drawRect(Math.round(point[0]) - 1, Math.round(point[1]) - 1, 3, 3, 1);
    }
    const controls = [PARAMS.amp_attack, PARAMS.amp_decay,
        PARAMS.amp_sustain, PARAMS.amp_release];
    for (let index = 0; index < controls.length; index++) {
        const control = controls[index];
        const x = index * 32 + 3;
        print(x, 34, SHORT_LABELS[control.key], 1);
        print(x, 45, compactControlValue(control), 1);
        const reservedX = index * 32 + 14;
        drawRect(reservedX, 57, 4, 4, 1);
        drawRect(reservedX + 1, 58, 2, 2, 0);
    }
}

function drawSequenceEnvelopePage() {
    const timing=sequenceAmpTiming(),[attack,decay,release]=timing.applied;
    const x0=4,x1=122,width=x1-x0,baseline=31,peak=19;
    const xAttack=x0+Math.round(width*attack/timing.dur);
    const xDecay=Math.min(x1,xAttack+Math.round(width*decay/timing.dur));
    const xGate=Math.max(xDecay,x1-Math.round(width*release/timing.dur));
    const sustainY=Math.round(baseline-timing.sustain*(baseline-peak));
    print(3,10,'DUR '+sequenceTimeText(timing.dur),1);
    if(timing.scale<.9999)print(100,10,'FIT',1);
    drawRect(x0,baseline,width+1,1,1);
    drawPixelLine(x0,baseline,xAttack,peak);
    drawPixelLine(xAttack,peak,xDecay,sustainY);
    drawPixelLine(xDecay,sustainY,xGate,sustainY);
    drawPixelLine(xGate,sustainY,x1,baseline);
    for(let y=19;y<=baseline;y+=3)drawRect(xGate,y,1,1,1);
    const labels=['ATK','DEC','SUS','REL'];
    const values=[sequenceTimeText(attack),sequenceTimeText(decay),Math.round(timing.sustain*100)+'%',sequenceTimeText(release)];
    for(let i=0;i<4;++i){const x=i*32+3;print(x,35,labels[i],1);print(i*32+1,46,values[i],1);const reserved=i*32+14;drawRect(reserved,57,4,4,1);drawRect(reserved+1,58,2,2,0);}
}

function readPatternFade() {
    const v=String(getParam('pattern_fade_state','0,0,-1,-1,0,0,0,0,0,2000,12000,0,0')).split(',').map(Number);
    if(v.length!==13||!v.every(Number.isFinite)||v[9]>v[10])return false;
    state.patternFade={active:!!v[0],held:!!v[1],from:v[2],to:v[3],position:clamp(v[4]/100,0,100),
        time:clamp(v[5],0,MAX_PATTERN_FADE_MS),curve:clamp(Math.round(v[6]),0,4),underflows:Math.max(0,v[7]),
        mode:clamp(Math.round(v[8]),0,1),low:clamp(v[9],0,MAX_PATTERN_FADE_MS),
        high:clamp(v[10],0,MAX_PATTERN_FADE_MS),duration:clamp(v[11],0,MAX_PATTERN_FADE_MS),override:!!v[12]};return true;
}
function formatFadeTime(ms) {
    if(ms===0)return 'CUT';
    if(ms<1000)return Math.round(ms)+'ms';
    const seconds=ms/1000;
    return seconds.toFixed(ms%1000===0?0:seconds>=10?1:2)+'s';
}
function engagePatternFader() {
    readPatternFade();
    if(!state.patternFade.held) {
        setParamBlocking('pattern_fade_hold',1);
        readPatternFade();
    }
}
function drawPatternFade() {
    const f=state.patternFade;
    drawSettingsTitle(state.actionMessageTicks>0?state.actionMessage:'PATTERN FADE');drawRect(0,8,128,1,1);
    const name=i=>'ABCD'[Math.floor(clamp(i,0,63)/16)]+String(clamp(i,0,63)%16+1).padStart(2,'0');
    if(f.active) {
        print(1,11,name(f.from)+'>'+name(f.to)+(f.held?' H':''),1);
        drawRect(77,13,49,1,1);
        drawRect(77+Math.round(f.position*.48),11,2,5,1);
    } else print(1,11,state.queuedClip>=0?(state.clipPrepareState===4?'SAMPLE LOAD ERROR':'WAIT '+name(state.queuedClip)):
        f.held?'ARMED: PICK PATTERN':'SELECT NEXT PATTERN',1);
    const controls=[
        [f.mode||f.override?'NOW':'TIME',f.mode||f.override?(f.from>=0?formatFadeTime(f.duration):'-'):formatFadeTime(f.time)],
        ['CURVE',FADE_CURVES[f.curve]],['XFADE',Math.round(f.position)+'%'],['HOLD',f.held?'ON':'OFF'],
        f.mode&&!f.override?['LOW',formatFadeTime(f.low)]:null,f.mode&&!f.override?['HIGH',formatFadeTime(f.high)]:null,
        ['MODE',f.override?'CHN':f.mode?'RAND':'FIX'],[state.shiftHeld?'FROM':'DONE','TURN']
    ];
    controls.forEach((control,i)=>{
        const x=i%4*32,y=i<4?21:44;
        if(!control){drawRect(x+14,y+8,4,4,1);drawRect(x+15,y+9,2,2,0);return;}
        print(x+1,y,control[0],1);print(x+1,y+10,control[1],1);
    });
}
function adjustPatternFade(index,delta) {
    readPatternFade();
    if((state.patternFade.override&&[0,4,5,6].includes(index))||(index===0&&state.patternFade.mode===1)||([4,5].includes(index)&&state.patternFade.mode===0))return;
    if(index===2&&delta!==0)engagePatternFader();
    delta=parameterEncoderDelta(index,'pattern_fade_'+index,delta);if(!delta)return;
    readPatternFade();const f=state.patternFade;
    if(index===7&&delta>0&&state.shiftHeld){chainAction('pattern_fade_return','RETURNING TO SOURCE');readPatternFade();draw();return;}
    if(index===0||index===4||index===5) {
        const current=index===0?f.time:index===4?f.low:f.high;
        let closest=0;FADE_TIMES.forEach((v,i)=>{if(Math.abs(v-current)<Math.abs(FADE_TIMES[closest]-current))closest=i;});
        setParamBlocking(index===0?'pattern_fade_ms':index===4?'pattern_fade_low_ms':'pattern_fade_high_ms',
            FADE_TIMES[clamp(closest+delta,0,FADE_TIMES.length-1)]);
    } else if(index===1)setParamBlocking('pattern_fade_curve',clamp(f.curve+delta,0,4));
    else if(index===2)setParamBlocking('pattern_fade_nudge',delta);
    else if(index===3)setParamBlocking('pattern_fade_hold',delta>0?1:0);
    else if(index===6)setParamBlocking('pattern_fade_mode',delta>0?1:0);
    else if(index===7&&delta>0)setParamBlocking('pattern_fade_finish',1);
    readPatternFade();draw();
}
function parseStackPath(text) {
    const values=String(text||'').split(',').map(Number);
    if(values.length!==26 || !values.every(Number.isFinite))return false;
    const count=values[0];if(count<4||count>8||count!==Math.round(count))return false;
    const points=Array.from({length:8},(_,i)=>({time:values[2+i*3],lane:values[3+i*3],curve:values[4+i*3]}));
    if(points[0].time!==0 || points[count-1].time>.990001 || points.some((p,i)=>p.time<0||p.time>1||p.lane<0||p.lane>1||p.curve< -1||p.curve>1
        ||(i>0&&i<count&&p.time<points[i-1].time+.00999)))return false;
    state.pathCount=count;state.pathPoint=clamp(Math.round(values[1]),0,count-1);
    state.pathPoints=points;state.pathReady=true;return true;
}
function readStackPath() {
    state.pathReady=parseStackPath(getParam('stack_path_state',''));return state.pathReady;
}
function pathLaneBounds() {
    return state.splitEnabled&&!state.sharedDual?[state.splitFocus*2+1,state.splitFocus*2+2]:[1,4];
}
function stackPathValue(phase) {
    phase-=Math.floor(phase);const points=state.pathPoints,count=state.pathCount;
    let before=0;while(before+1<count&&phase>=points[before+1].time)++before;
    const after=(before+1)%count,end=after?points[after].time:1;
    const u=clamp((phase-points[before].time)/(end-points[before].time),0,1);
    const bent=u+points[before].curve*(u*u-u);
    return points[before].lane+(points[after].lane-points[before].lane)*bent;
}
function pathEditorControls() {
    const p=state.pathPoints[state.pathPoint],bounds=pathLaneBounds();
    return [
        {label:'PT',value:(state.pathPoint+1)+'/'+state.pathCount},
        {label:'TIME',value:Math.round(p.time*100)+'%'},
        {label:'LANE',value:(bounds[0]+p.lane*(bounds[1]-bounds[0])).toFixed(2)},
        {label:'CURV',value:String(Math.round(p.curve*100))},
        {label:'PNTS',value:String(state.pathCount)},null,null,{label:'RSET',value:'SHFT'}
    ];
}
function drawPathEditor() {
    drawRect(0,0,128,8,0);drawRect(0,8,128,1,1);
    drawSettingsTitle(state.actionMessageTicks>0?state.actionMessage:
        ((state.splitEnabled?partName()+' ':'')+'PATH EDIT '+(state.pathPoint+1)+'/'+state.pathCount));
    if(!state.pathReady){print(1,19,'LOADING PATH',1);return;}
    const left=12,right=125,top=11,bottom=26,bounds=pathLaneBounds();
    print(0,9,String(bounds[0]),1);print(0,22,String(bounds[1]),1);
    for(let y=top;y<=bottom;y+=5)for(let x=left;x<=right;x+=4)drawRect(x,y,1,1,1);
    let priorY=top+stackPathValue(0)*(bottom-top);
    for(let x=left+1;x<=right;++x) {
        const phase=(x-left)/(right-left),y=top+stackPathValue(phase>=1?0:phase)*(bottom-top);
        drawPixelLine(x-1,priorY,x,y);priorY=y;
    }
    for(let i=0;i<state.pathCount;++i) {
        const p=state.pathPoints[i],x=Math.round(left+p.time*(right-left)),y=Math.round(top+p.lane*(bottom-top));
        if(i===state.pathPoint){drawRect(x-2,y-2,5,5,1);drawRect(x-1,y-1,3,3,0);drawRect(x,y,1,1,1);}
        else drawRect(x-1,y-1,3,3,1);
    }
    for(const [index,control] of pathEditorControls().entries()) {
        const x=(index%4)*32,y=index<4?31:48;
        if(!control){drawRect(x+14,y+6,4,4,1);drawRect(x+15,y+7,2,2,0);continue;}
        print(x+2,y,control.label,1);print(x+2,y+8,control.value,1);
    }
}
function openPathEditor() {
    const page=parameterPages().findIndex(p=>p.pathEditor);if(page<0)return;
    state.view='settings';state.settingsPage=page;state.actionMessageTicks=0;resetParameterEncoderMotion();readStackPath();draw();
}
function adjustPathEditor(index,delta) {
    if(!state.pathReady && !readStackPath()){showActionMessage('PATH UNAVAILABLE');return;}
    if(index===5||index===6)return;
    delta=parameterEncoderDelta(index,'path_'+index+'_'+state.pathPoint,delta);if(!delta)return;
    const p=state.pathPoints[state.pathPoint];let operation,value;
    if(index===0){operation='select';value=0;}
    if(index===1){operation='time';value=clamp(p.time+delta*.005,0,.99);}
    if(index===2){operation='lane';value=clamp(p.lane+delta*.01,0,1);}
    if(index===3){operation='curve';value=clamp(p.curve+delta*.025,-1,1);}
    if(index===4){operation='count';value=clamp(state.pathCount+delta,4,8);}
    if(index===7){
        if(!state.shiftHeld){showActionMessage('SHIFT+K8 RESETS PATH');return;}
        if(delta<0)return;operation='reset';value=0;
    }
    if(!operation)return;
    const point=index===0?clamp(state.pathPoint+delta,0,state.pathCount-1):state.pathPoint;
    setParamBlocking('stack_path_edit',operation+','+point+','+value);
    readStackPath();
    const error=String(getParam('last_error','')||'');
    if(error)showActionMessage(index===1&&point===0?'FIRST POINT TIME IS 0':'MOVE POINTS APART');
    else if(index===7)showActionMessage('RESET DRAW PATH');
    draw();
}
function drawSettings() {
    clearDisplay();
    if(!state.editorReady){print(1,0,'UPDATING EDITOR',1);return;}
    const page = currentSettingsPage();
    if(page.sequenceTrig){drawSequenceTrig();return;}
    if(page.sequenceBurst){drawSequenceBurst();return;}
    if(page.customMaps){drawCustomMidiProfile();return;}
    if (page.route) { drawFxOrder(); return; }
    if(page.pathEditor){drawPathEditor();return;}
    if(page.patternFade){drawPatternFade();return;}
    if(page.chainTools){drawChainTools();return;}
    if(page.chainFade){drawChainFade(page.chainFade);return;}
    const target = state.clipRecording ? 'REC ' : (state.clipPlaying ? '> ' : '');
    const pages = parameterPages();
    const takeSuffix = page===CHAIN_PAGE?' '+String(state.chainBank+1).padStart(2,'0'):page === TAKE_PAGE ? ' ' + state.clipTakeCount
        : (page === FX_ORDER_PAGE && state.fxRoutePending ? '*' : '');
    const lockTarget = state.algorithm === 6 && state.glitchBufferLocked
        ? 'LCK ' : '';
    const scope=state.splitEnabled?((isMidiPage(page)||isSendPage(page)||isSetupPage(page))?'':state.sharedDual&&page===SAMPLE_PAGE?'SRC ':page.global?(page.scopeLabel??(page.sequenceMix?'':'FX ')):((partName()+' '))):'';
    const engineTitle=voiceEngineListEnabled()&&(page.title==='ENGINE'||playbackPagesForAlgorithm(state.algorithm,Math.round(PARAMS.motion_sound.value)).some(p=>p.title===page.title)||page.pathEditor||page.voiceSetup)?'E'+Math.round(PARAMS.voice_engine_edit.value)+' ':'';
    rememberLayersPage(page);
    const layerTitle=layersUi.active?((page.title==='FX CORNER'||page.layerFxDetail)?'C'+(Number(PARAMS.layers_corner.value)+1)+' ':layersPageSide(page)===0?'A ':''):'';
    const pageTitle = target + lockTarget + scope + engineTitle + layerTitle+page.title + (page.sequenceEnvelope?' T'+String((sequenceUi.data?.step??0)+1).padStart(2,'0'):takeSuffix);
    const title = state.actionMessageTicks > 0
        ? state.actionMessage : pageTitle;
    drawSettingsTitle(title,pages);
    drawRect(0, 8, 128, 1, 1);
    if (page.sequenceEnvelope) { drawSequenceEnvelopePage(); return; }
    if (page.breakpoint) {
        drawAmpEnvelopePage();
        return;
    }
    const drawPath=page.title===STACK_PATH_PAGE.title&&Math.round(PARAMS.lane_path.value)===8;
    const hasFooter = page.sliceEditor || page.pulsar || page.stochastic || page.voiceEngines || page.voiceSetup || drawPath || (state.splitEnabled && (isSendPage(page)
        || page===DELAY_RETURN_PAGE || page.title===REVERB_PAGE.title));
    for (let index = 0; index < 8; index++) {
        const key = page.keys[index];
        if (!key) {
            if(page.title==='MORPH'&&index===7){print(98,34,'PAIR',1);print(98,46,String(getParam('morph_pair','1>2')).slice(0,5),1);}else drawReservedParameterCell(index, hasFooter);
            continue;
        }
        const control = controlForKey(key);
        drawParameterCell(index, control, hasFooter);
        if(page.voiceEngines&&index===Math.round(PARAMS.voice_engine_edit.value)-1)
            drawRect((index%4)*32+3,Math.floor(index/4)*24+19,12,1,1);
    }
    if(page.pulsar)print(1,57,pulsarDurationFooter(),1);
    else if(page.stochastic)print(1,57,PARAMS.stochastic_source_hold.value>=.5?'SRC HELD CLICK: RUN':'CLICK: HOLD SOURCE',1);
    else if(page.voiceEngines||page.voiceSetup)print(1,57,'TURN SLOT CLICK EDIT',1);
    else if(drawPath)print(1,57,'CLICK: EDIT PATH',1);
    else if(page.sliceEditor)print(1,57,'CLICK: EDIT SLICES',1);
    else if (state.splitEnabled && isSendPage(page)) print(1,57,splitSendFooter(),1);
    else if (state.splitEnabled && !isQuad() && (page===DELAY_RETURN_PAGE || page.title===REVERB_PAGE.title))
        print(1,57,'RETURN: FX SENDS',1);
}

function compactReportCount(value) {
    const count = Math.max(0, Math.round(Number(value) || 0));
    if (count < 1000000) return String(count);
    if (count < 1000000000) return (count / 1000000).toFixed(1) + 'M';
    return (count / 1000000000).toFixed(1) + 'G';
}

function helpReportRow(label, value) {
    const name = String(label).slice(0, 15);
    const available = Math.max(1, 21 - name.length);
    return name + String(value).slice(-available).padStart(available, ' ');
}

function drawHelp() {
    clearDisplay();
    if (state.helpPage === 0) {
        drawTitleWithFraction('SYSTEM REPORT <>','1/5');
        print(1, 11, helpReportRow('RENDER PEAK',
            compactReportCount(state.maximumRenderMicros) + ' us'), 1);
        print(1, 21, helpReportRow('RENDER MISSES',
            compactReportCount(state.renderOverruns)), 1);
        print(1, 31, helpReportRow('LIMIT CURRENT',
            state.limiterReductionDb.toFixed(1) + ' dB'), 1);
        print(1, 41, helpReportRow('LIMIT MAX',
            state.limiterMaximumReductionDb.toFixed(1) + ' dB'), 1);
        print(1,51,helpReportRow('FADE BUF MISSES',compactReportCount(state.patternFade.underflows)),1);
        return;
    }
    if (state.helpPage === 1) {
        drawTitleWithFraction('CALLBACK WORK <>','2/5');
        print(1, 13, helpReportRow('CONTROL PEAK', compactReportCount(state.controlPeakMicros) + ' us'), 1);
        print(1, 25, helpReportRow('QUERY PEAK', compactReportCount(state.queryPeakMicros) + ' us'), 1);
        print(1, 37, helpReportRow('WORK PEAK', compactReportCount(state.workPeakMicros) + ' us'), 1);
        print(1, 49, helpReportRow('WORK MISSES', compactReportCount(state.workOverruns)), 1);
        return;
    }
    if(state.helpPage===2) {
        drawTitleWithFraction('RENDER TIMING <>','3/5');
        const cpu=state.renderCpuAvailable;
        print(1,11,helpReportRow('CPU PEAK',cpu?compactReportCount(state.renderCpuPeakMicros)+' us':'N/A'),1);
        print(1,23,helpReportRow('WALL PEAK',compactReportCount(state.maximumRenderMicros)+' us'),1);
        print(1,35,helpReportRow('WAIT PEAK',cpu?compactReportCount(state.renderWaitPeakMicros)+' us':'N/A'),1);
        print(1,47,helpReportRow('CPU MISSES',cpu?compactReportCount(state.renderCpuOverruns):'N/A'),1);
        print(1,57,'SHIFT+DEL RESET',1);return;
    }
    if(state.helpPage===4){
        const d=remoteE16.diagnostics;
        drawTitleWithFraction('E16 LINK <>','5/5');
        print(1,11,helpReportRow('STATE',d.state),1);
        print(1,21,helpReportRow('LAST',d.issue),1);
        print(1,31,helpReportRow('ACK/NACK',compactReportCount(d.acks)+'/'+compactReportCount(d.nacks)),1);
        print(1,41,helpReportRow('RETRY/RECON',compactReportCount(d.retries)+'/'+compactReportCount(d.reconnects)),1);
        print(1,51,helpReportRow('PENDING PX',d.pixels+'/'+d.pixelWindow),1);return;
    }
    drawTitleWithFraction('SHORTCUTS <>','4/5');
    print(1, 9, 'REC        data loop', 1);
    print(1, 17, 'SHIFT+REC  audio lane', 1);
    print(1, 25, 'UNDO      sample/clip', 1);
    print(1, 33, 'SHIFT+PAD  note hold', 1);
    print(1, 41, 'SH+LOOP glitch lock', 1);
    print(1, 49, 'CAP+MENU/TRK page nav', 1);
    print(1, 57, 'DEL+PLAY   panic', 1);
}

function clipLabel(index) {
    return BANK_NAMES[Math.floor(index / 16)]
        + String(index % 16 + 1).padStart(2, '0');
}

function saveNotice() {
    if(state.saveStatus===4)return 'SAVE FAILED: RETRYING';
    if(state.saveStatus===2)return 'SAVING...';
    if(state.savedTicks>0 && state.saveStatus===3)return 'SAVED';
    return '';
}

function errorFooter(text) {return state.saveStatus===4?saveNotice():text;}

function parseSaveStatus(raw) {
    if(raw===''||raw==null)return;
    const next=Number(raw);
    if(!Number.isInteger(next)||next<0||next>4)return;
    if(next===3 && state.saveStatus!==3 && state.saveStatus!==0)state.savedTicks=180;
    if(next!==3)state.savedTicks=0;
    state.saveStatus=next;
}

function parseMixStatus(raw) {
    const v=String(raw||'').split(',').map(Number);
    const count=partCount();if(v.length!==count*3||!v.every(Number.isFinite))return;
    state.mixPeaks=v.slice(0,count).map(x=>Math.max(0,x));
    state.mixVoices=v.slice(count,count*2).map(x=>clamp(Math.round(x),0,isQuad()?2:4));
    state.mixReasons=v.slice(count*2,count*3).map(x=>clamp(Math.round(x),0,5));
}

function pageEntries() {
    const result=[{title:'WAVEFORMS',view:'main'},{title:'INSTRUMENT MODE',view:'mode'}];
    if(state.splitEnabled)result.push({title:isQuad()?'QUAD MIX STATUS':'A/B MIX STATUS',view:'mixStatus'});
    for(const [index,page] of parameterPages().entries()) {
        const shared=page.global||isSendPage(page)||isSetupPage(page);
        const scope=state.splitEnabled&&!shared?(state.sharedDual&&page===SAMPLE_PAGE?'SRC ':partName()+' '):'';
        result.push({title:scope+page.title,view:'settings',index});
    }
    result.push({title:'CLIP OVERVIEW',view:'clips'});
    result.push({title:'SAMPLE BROWSER L'+state.lane,view:'library'});
    return result;
}

const PAGE_CATEGORY_NAMES=['VOICE','LANE','MIX / FX','PERF'];
const LANE_PAGE_TITLES=new Set(['MIX / REC','SAMPLE EDIT']);
const MIX_PAGE_TITLES=new Set(['FX SENDS','PRIORITY DUCK','PART COLOR',
    'EFFECTS','STEREO / ROTARY','DELAY RETURN','TAPE','REVERB','FX ORDER']);
const PERF_PAGE_TITLES=new Set(['PERF LOOP','PATTERN FADE','OVERDUB TAKES',
    'CHAIN','CHAIN FADE','ENTRY FADE','CHAIN TOOLS']);

function pageCategoryForPage(page) {
    if(page?.sequenceMix)return 2;
    if(page?.sequence)return 0;
    if(page?.layerFx)return 2;
    if(page?.layersSetup)return 2;
    if(!page||isMidiPage(page)||isSetupPage(page))return -1;
    if(LANE_PAGE_TITLES.has(page.title))return 1;
    if(MIX_PAGE_TITLES.has(page.title))return 2;
    if(PERF_PAGE_TITLES.has(page.title))return 3;
    return 0;
}

function categorySettingsIndices(category,pages=parameterPages()) {
    const indices=[];
    for(let index=0;index<pages.length;++index) {
        if(pageCategoryForPage(pages[index])===category)indices.push(index);
    }
    return indices;
}

function settingsPagePosition(pages=parameterPages()) {
    const page=pages[state.settingsPage],category=pageCategoryForPage(page);
    const indices=category>=0?categorySettingsIndices(category,pages)
        :isMidiPage(page)?pages.map((p,i)=>isMidiPage(p)?i:-1).filter(i=>i>=0)
        :isSetupPage(page)?pages.map((p,i)=>isSetupPage(p)?i:-1).filter(i=>i>=0)
        :[state.settingsPage];
    return String(Math.max(0,indices.indexOf(state.settingsPage))+1)+'/'+indices.length;
}

function drawTitleWithFraction(left,fraction) {
    const right=String(fraction),rightX=Math.max(1,128-right.length*6);
    const leftCharacters=Math.max(0,Math.floor((rightX-2)/6));
    print(1,0,String(left).slice(0,leftCharacters),1);
    print(rightX,0,right,1);
}

function drawSettingsTitle(left,pages=parameterPages()) {
    drawTitleWithFraction(left,settingsPagePosition(pages));
}

function pageEntryForSettings(index,page) {
    const shared=page.global||isSendPage(page)||isSetupPage(page);
    const scope=layersUi.active&&layersPageSide(page)===0?'A ':state.splitEnabled&&!shared
        ?(state.sharedDual&&page===SAMPLE_PAGE?'SRC ':partName()+' '):'';
    return {title:scope+page.title,view:'settings',index,
        category:pageCategoryForPage(page)};
}

function categoryPageEntries(category) {
    const result=[];
    for(const [index,page] of parameterPages().entries()) {
        if(pageCategoryForPage(page)===category)result.push(pageEntryForSettings(index,page));
    }
    if(category===1)result.push({title:'SAMPLE BROWSER L'+state.lane,
        view:'library',category});
    if(category===2&&state.splitEnabled)result.push({
        title:isQuad()?'QUAD MIX STATUS':'A/B MIX STATUS',view:'mixStatus',category});
    if(category===3)result.splice(Math.min(1,result.length),0,
        {title:'CLIP OVERVIEW',view:'clips',category});
    return result;
}

function setupPageEntry() {
    const pages=parameterPages();
    const wanted=sequenceUi.active?SEQUENCE_PITCH_PAGE
        :state.splitEnabled?(isQuad()?QUAD_SETUP_PAGE:SPLIT_SETUP_PAGE):PADS_SCALE_PAGE;
    const index=pages.findIndex(page=>page===wanted||page.title===wanted.title);
    return {title:sequenceUi.active?'SEQ SETUP'
        :state.splitEnabled?wanted.title:'PADS & SCALE',view:'settings',index};
}

function midiPageEntry() {
    return {title:'MIDI CONTROL',view:'settings',
        index:parameterPages().findIndex(page=>page===MIDI_CONTROL_PAGE)};
}

function pageHubEntries() {
    return [
        ...PAGE_CATEGORY_NAMES.map((title,category)=>({title:category===3?'PERFORMANCE':title,category,hub:'category'})),
        {title:'MODE',view:'mode',hub:'mode'},
        {...setupPageEntry(),title:sequenceUi.active?'SEQ SETUP':'SETUP',hub:'setup'},
        {...midiPageEntry(),title:'MIDI',hub:'midi'},
        {title:'ALL',hub:'all'},
        {title:'PROJECTS',view:'projects',hub:'projects'},
    ];
}

function currentPageCategory() {
    if(state.view==='settings')return pageCategoryForPage(currentSettingsPage());
    if(state.view==='library')return 1;
    if(state.view==='mixStatus'||state.view==='fxMenu')return 2;
    if(state.view==='clips'||state.view==='chain')return 3;
    return -1;
}

function currentEntryIndex(entries) {
    if(state.view==='settings')return entries.findIndex(entry=>
        entry.view==='settings'&&entry.index===state.settingsPage);
    return entries.findIndex(entry=>entry.view===state.view);
}

function pagePickerEntries() {
    if(state.pageTier==='category')return categoryPageEntries(state.pageCategory);
    if(state.pageTier==='all')return pageEntries();
    return pageHubEntries();
}

function openPagePicker() {
    endPerformanceLayer();
    if(state.view==='pages') {
        const category=state.pageTier==='category'?state.pageCategory
            :state.pageTier==='hub'&&state.pageSelection<4?state.pageSelection:-1;
        state.pageSelection=category>=0?category:state.pageTier==='all'?7:0;
        state.pageTier='hub';rebuildLedQueue();return;
    }
    if(state.fxRouteGrabbed||state.fxRoutePending)cancelFxOrder(false);
    state.pageReturn={view:state.view,page:state.settingsPage,focus:state.splitFocus,fx:state.fxMenuReturn};
    state.pageDestinationReturn=null;
    state.pageTier='hub';
    const category=currentPageCategory();
    state.pageSelection=category>=0?category
        :state.view==='mode'?4
        :state.view==='settings'&&isMidiPage(currentSettingsPage())?6
        :state.view==='settings'&&isSetupPage(currentSettingsPage())?5
        :0;
    state.view='pages';state.actionMessageTicks=0;
    rebuildLedQueue();
}

function movePagePicker(delta) {
    state.pageSelection=clamp(state.pageSelection+delta,0,pagePickerEntries().length-1);
    rebuildLedQueue();
}

function activatePageEntry(entry,returnToPicker=false) {
    if(!entry)return;
    const destinationReturn=returnToPicker?{
        tier:state.pageTier,category:state.pageCategory,selection:state.pageSelection,
    }:null;
    if(entry.view==='mode'){
        state.pageDestinationReturn=destinationReturn;
        state.modeSelection=Math.round(PARAMS.split_mode.value);state.view='mode';return;
    }
    if((state.pageReturn?.view==='library'||state.view==='library')&&entry.view!=='library')
        setParamBlocking('preview_stop',1);
    if(entry.view==='projects')openProjects();
    else if(entry.view==='library')openLibrary(state.lane);
    else {
        state.view=entry.view;state.fxMenuReturn=false;
        if(entry.view==='settings') {
            state.settingsPage=entry.index;
            if(state.splitEnabled)state.splitPages[state.splitFocus]=entry.index;
            rememberLayersPage();
        }
        if(entry.view==='clips')state.clipBank=Math.floor(state.activeClip/16);
        resetParameterEncoderMotion();
        if(state.view==='settings'&&currentSettingsPage().pathEditor)readStackPath();
        rebuildLedQueue();
    }
    state.pageDestinationReturn=destinationReturn;
    if(!returnToPicker)state.pageReturn=null;
}

function openPageCategory(category) {
    endPerformanceLayer();
    if(state.fxRouteGrabbed||state.fxRoutePending)cancelFxOrder(false);
    const entries=categoryPageEntries(category);
    if(!entries.length)return;
    const current=currentPageCategory()===category?currentEntryIndex(entries):-1;
    state.pageCategory=category;
    activatePageEntry(entries[(current+1)%entries.length]);
}

function enterPageCategory(category) {
    state.pageTier='category';state.pageCategory=category;
    const entries=categoryPageEntries(category);
    state.pageSelection=Math.max(0,currentEntryIndex(entries));
    rebuildLedQueue();
}

function activatePagePicker() {
    const entries=pagePickerEntries();
    const entry=entries[clamp(state.pageSelection,0,entries.length-1)];
    if(!entry)return;
    if(state.pageTier==='hub') {
        if(entry.hub==='category'){enterPageCategory(entry.category);return;}
        if(entry.hub==='all'){
            state.pageTier='all';
            state.pageSelection=Math.max(0,currentEntryIndex(pageEntries()));
            return;
        }
    }
    activatePageEntry(entry,true);
}

function returnToPagePicker() {
    const destination=state.pageDestinationReturn;
    if(!destination)return false;
    if(state.view==='library')setParamBlocking('preview_stop',1);
    state.view='pages';state.pageTier=destination.tier;
    state.pageCategory=destination.category;
    state.pageSelection=destination.selection;
    state.pageDestinationReturn=null;state.fxMenuReturn=false;
    resetParameterEncoderMotion();rebuildLedQueue();
    return true;
}

function pagePickerBack() {
    if(state.pageTier!=='hub') {
        const category=state.pageTier==='category'?state.pageCategory:currentPageCategory();
        state.pageTier='hub';state.pageSelection=category>=0?category:7;
        rebuildLedQueue();return;
    }
    closePagePicker();
}

function closePagePicker() {
    const previous=state.pageReturn;
    state.view=previous?.view||'main';
    if(previous){if(!state.splitEnabled||previous.focus===state.splitFocus)state.settingsPage=previous.page;state.fxMenuReturn=previous.fx;}
    if(!state.splitEnabled&&(state.view==='mixStatus'||state.view==='fxMenu')){state.view='main';state.fxMenuReturn=false;}
    state.pageReturn=null;state.pageDestinationReturn=null;
}

function drawModePicker() {
    clearDisplay();print(1,0,'INSTRUMENT MODE',1);drawRect(0,8,128,1,1);
    const names=['SINGLE','SPLIT','SHARED DUAL','QUAD','BANK','LAYERS','SEQUENCE'];
    const start=clamp(state.modeSelection-4,0,2);
    for(let i=start;i<Math.min(start+5,names.length);++i) {
        const selected=i===state.modeSelection,y=10+(i-start)*9;
        if(selected)drawRect(0,y,128,9,1);
        print(1,y,((i===Math.round(PARAMS.split_mode.value)?'* ':'  ')+names[i]),selected?0:1);
    }
    print(1,57,errorFooter(state.actionMessageTicks>0?state.actionMessage:'TURN PICK CLICK SET'),1);
}

function applyModePicker() {
    endPerformanceLayer();
    const wanted=state.modeSelection;
    setParamBlocking('split_mode',wanted);parseSplitState(getParam('split_state','{}'));readLayers();readSequence();
    if(Math.round(getNumber('split_mode',PARAMS.split_mode.value))!==wanted) {
        showMessage('STOP RECORD FIRST');showActionMessage('STOP RECORD FIRST');return;
    }
    state.view='main';state.pageReturn=null;state.pageDestinationReturn=null;state.fxMenuReturn=false;
    showMessage(MODE_MESSAGES[wanted]);
    primePadLeds();rebuildLedQueue();
}

function drawPagePicker() {
    clearDisplay();const entries=pagePickerEntries();
    state.pageSelection=clamp(state.pageSelection,0,entries.length-1);
    const hub=state.pageTier==='hub';
    const heading=hub?'PAGES':state.pageTier==='category'?PAGE_CATEGORY_NAMES[state.pageCategory]:'ALL PAGES';
    drawTitleWithFraction(heading+(state.splitEnabled?' '+partName():''),
        (state.pageSelection+1)+'/'+entries.length);
    drawRect(0,8,128,1,1);
    const start=clamp(state.pageSelection-2,0,Math.max(0,entries.length-5));
    for(let row=0;row<5;++row){
        const index=start+row,entry=entries[index];if(!entry)continue;
        const selected=index===state.pageSelection,y=10+row*9;
        if(selected)drawRect(0,y,hub?124:128,8,1);
        const label=hub&&entry.hub==='category'?(entry.category+1)+' '+entry.title:entry.title;
        print(1,y,((selected?'> ':'  ')+label).slice(0,hub?20:21),selected?0:1);
    }
    if(hub&&entries.length>5){
        const height=Math.max(3,Math.floor(44*5/entries.length));
        const top=10+Math.round((44-height)*start/(entries.length-5));
        drawRect(127,10,1,44,1);drawRect(125,top,2,height,1);
    }
    print(1,57,errorFooter(hub?'T1-4 BANK CLICK OPEN':'CLICK OPEN  BACK HUB'),1);
}

function meterWidth(peak,width) {
    return peak<=.001?0:clamp(Math.round((20*Math.log10(peak)+60)/60*width),0,width);
}

function drawMixStatus() {
    clearDisplay();
    if(isQuad()) {
        print(1,0,'QUAD MIX EDIT '+partName(),1);drawRect(0,8,128,1,1);
        const reasons=['ACTIVE','MUTED','LEVEL 0','FADED OUT','NO AUDIO','IDLE'];
        for(let p=0;p<4;++p){const y=12+p*10;print(1,y,partName(p)+(state.mixVoices[p]||0),1);print(19,y,(ALGORITHMS[state.splitParts[p]?.algorithm]||'---').slice(0,3),1);drawRect(43,y+2,24,3,0);const w=meterWidth(state.mixPeaks[p]||0,24);if(w)drawRect(43,y+2,w,3,1);print(73,y,reasons[state.mixReasons[p]??5],1);}
        print(1,57,errorFooter('TRACK / CAPTURE: EDIT'),1);return;
    }
    print(1,0,(state.sharedDual?'SHARED MIX EDIT ':'MIX STATUS EDIT ')+partName(),1);drawRect(0,8,128,1,1);
    const reasons=['ACTIVE','MUTED','LEVEL 0','FADED OUT','NO AUDIO','IDLE'];
    for(let part=0;part<2;++part) {
        const y=12+part*21;
        print(1,y,(part?'B ':'A ')+(ALGORITHMS[state.splitParts[part]?.algorithm]||'---')+' '+state.mixVoices[part]+'/4',1);
        drawRect(1,y+10,43,6,1);drawRect(2,y+11,41,4,0);
        const width=meterWidth(state.mixPeaks[part],41);if(width)drawRect(2,y+11,width,4,1);
        print(49,y+9,reasons[state.mixReasons[part]],1);
    }
    print(1,57,errorFooter(('BLND '+Math.round(state.splitBlend*100)+'% '+'').slice(0,21)),1);
}

function undoFeedback(text) {showMessage(text);showActionMessage(text);}

function triggerSafetyUndo() {
    state.recording=getNumber('recording',state.recording?1:0)!==0;
    parseClipState(getParam('clip_state',''));
    state.undoAvailable=getNumber('undo_available',0)!==0;
    if(state.recording||state.clipRecording) {
        undoFeedback('STOP RECORD TO UNDO');rebuildIconLedQueue();return;
    }
    if(!state.undoAvailable) {
        undoFeedback('NOTHING TO UNDO');rebuildIconLedQueue();return;
    }
    setParamBlocking('undo',1);
    state.undoAvailable=getNumber('undo_available',0)!==0;
    if(state.undoAvailable) {
        const error=String(getParam('last_error','')).toLowerCase();
        undoFeedback(error.includes('archive')?'WAIT FOR TAKE TO SAVE':error.includes('prepar')?'WAIT FOR CLIP LOAD':'UNDO NOT READY');
    } else {
        refreshEditorSnapshot();refreshChainEditors();
        if(state.fxRouteGrabbed||state.fxRoutePending)cancelFxOrder(false);
        undoFeedback('UNDONE');
    }
    rebuildLedQueue();
}

function triggerPanic() {
    setParamBlocking('panic',1);
    state.activePads.clear();state.messageTicks=0;state.actionMessageTicks=0;
    pollDsp();rebuildLedQueue();showMessage('PANIC - NOTES OFF');
}

function drawClips() {
    clearDisplay();
    const active = clipLabel(state.activeClip);
    const queued = state.queuedClip >= 0
        ? ' >' + clipLabel(state.queuedClip) : '';
    print(1, 0, ('PERF CLIPS ' + active + queued).slice(0, 21), 1);
    drawRect(0, 8, 128, 1, 1);
    for (let index = 0; index < 16; index++) {
        const clip = state.clipBank * 16 + index;
        const column = index % 4;
        const row = Math.floor(index / 4);
        const x = 2 + column * 31;
        const y = 13 + row * 11;
        let marker = clipOccupied(clip) ? '*' : '-';
        if (clip === state.activeClip) marker = '>';
        if (clip === state.queuedClip) marker = '+';
        print(x, y, marker + String(index + 1).padStart(2, '0'), 1);
    }
    print(1, 57, errorFooter(state.messageTicks>0?state.message:'COPY: HOLD + STEP'), 1);
}

function drawChain() {
    clearDisplay();
    const selected=state.chainEntries[state.chainSlot];
    const detail=selected.enabled?clipLabel(selected.clip)+(selected.repeats===0?' W':'x'+selected.repeats):'EMPTY';
    print(1,0,('C'+String(state.chainBank+1).padStart(2,'0')+' '+(state.chainEnabled?'RUN'+String(state.chainPlayBank+1).padStart(2,'0'):'OFF')+' E'+String(state.chainSlot+1).padStart(2,'0')+' '+detail).slice(0,21),1);
    drawRect(0,8,128,1,1);
    for(let i=0;i<16;i++) {
        const index=state.chainPage*16+i,entry=state.chainEntries[index],x=i%4*32,y=13+Math.floor(i/4)*11;
        let marker=index===state.chainSlot?':':' ';
        if(index===state.chainPosition&&state.chainEnabled&&state.chainBank===state.chainPlayBank)marker='>';
        if((state.chainLaunchBank===state.chainBank&&index===state.chainLaunchSlot)||(state.chainLaunchBank<0&&state.chainPlayBank===state.chainBank&&index===state.chainQueuedSlot))marker='+';
        const repeat=entry.repeats===0?'W':entry.repeats>9?'+':String(entry.repeats);
        const body=entry.enabled?String(entry.clip+1).padStart(2,'0')+'x'+repeat:'--';
        if(index===state.chainSlot)drawRect(x,y-1,32,9,1);
        print(x+1,y,(marker+body).slice(0,5),index===state.chainSlot?0:1);
    }
    print(1,57,errorFooter(state.messageTicks>0?state.message:(state.chainLaunchBank>=0?'QUEUED C'+String(state.chainLaunchBank+1).padStart(2,'0')+' E'+String(state.chainLaunchSlot+1).padStart(2,'0'):'P'+(state.chainPage+1)+'/4 <> MENU:PARAMS')),1);
}
function refreshChainEditors() {
    parseChainState(getParam('chain_state',''));parseChainEntries(getParam('chain_entries',''));readChainFades();
    state.undoAvailable=getNumber('undo_available',0)!==0;rebuildLedQueue();
}
function chainAction(key,message,value=1) {
    setParamBlocking(key,value);
    const error=String(getParam('last_error',''));
    refreshChainEditors();const text=error||(typeof message==='function'?message():message);showMessage(text);showActionMessage(text);
}
function effectiveFadeSummary(scope) {
    const v=String(getParam(scope==='entry'?'entry_effective_fade':'chain_effective_fade','0,0,0,2000,12000,0')).split(',').map(Number);
    if(v.length!==6||!v.every(Number.isFinite))return 'FADE NOT READY';
    const timing=v[1]?'R '+formatFadeTime(v[3])+'-'+formatFadeTime(v[4]):formatFadeTime(v[2]);
    const summary=['GLBL','CHAIN','ENTRY'][v[0]]+' '+timing+' '+FADE_CURVES[v[5]];
    if(summary.length<=21)return summary;
    const seconds=ms=>String(Number((ms/1000).toPrecision(3))).replace(/^0\./,'.');
    const range=v[4]<1000?Math.round(v[3])+'-'+Math.round(v[4])+'ms':seconds(v[3])+'-'+seconds(v[4])+'s';
    return ['GLBL','CHN','ENT'][v[0]]+' R'+range+' '+FADE_CURVES[v[5]];
}
function drawChainTools() {
    const feedback=state.actionMessageTicks>0;
    drawSettingsTitle(feedback?state.actionMessage:
        'CHAIN TOOLS C'+String(state.chainBank+1).padStart(2,'0'));
    drawRect(0,8,128,1,1);
    print(17,11,'CHAIN',1);print(81,11,'ENTRY',1);
    drawRect(63,11,1,28,1);
    print(3,21,'1 COPY',1);print(3,31,'2 PASTE',1);
    print(67,21,'3 COPY',1);print(67,31,'4 PASTE',1);
    drawRect(0,41,128,1,1);
    const controls=[['BANK',String(state.chainBank+1).padStart(2,'0')],
        ['SLOT',String(state.chainSlot+1).padStart(2,'0')],['CUE','HERE'],['CNCL','QUEU']];
    controls.forEach((c,i)=>{print(i*32+2,44,c[0],1);print(i*32+2,54,c[1],1);});
}
function selectEditSlot(index,delta) {
    delta=parameterEncoderDelta(index,'chain_slot',delta);if(!delta)return;
    setParamBlocking('chain_slot',clamp(state.chainSlot+delta,0,63)+1);
    refreshChainEditors();resetParameterEncoderMotion(index);draw();
}
function adjustChainTools(index,delta) {
    if(!delta)return;
    if(index===4){delta=parameterEncoderDelta(index,'chain_bank',delta);if(delta){setParamBlocking('chain_bank',clamp(state.chainBank+delta,0,15)+1);refreshChainEditors();resetParameterEncoderMotion(index);}return;}
    if(index===5){selectEditSlot(index,delta);return;}
    if(delta<0)return;
    const keys=['chain_copy','chain_paste','entry_copy','entry_paste',null,null,'chain_launch','chain_launch_cancel'];
    const labels=['CHAIN COPIED','CHAIN PASTED','ENTRY COPIED','ENTRY PASTED','','','CHAIN QUEUED','QUEUE CANCELLED'];
    chainAction(keys[index],labels[index]);draw();
}
function drawChainFade(scope) {
    readChainFades();const offset=scope==='entry'?5:0,v=state.chainFades.slice(offset,offset+5),mode=v[0];
    const fixed=scope==='entry'?mode===2:mode===1,random=scope==='entry'?mode===3:mode===2;
    drawSettingsTitle((scope==='entry'?'ENTRY FADE ':'CHAIN FADE ')
        +'C'+String(state.chainBank+1).padStart(2,'0')
        +(scope==='entry'?':'+String(state.chainSlot+1).padStart(2,'0'):''));drawRect(0,8,128,1,1);
    print(1,11,effectiveFadeSummary(scope).slice(0,21),1);
    const selected=scope==='entry'?state.chainEntries[state.chainSlot]:null;
    const controls=[fixed?['TIME',formatFadeTime(v[1])]:null,fixed||random?['CURVE',FADE_CURVES[v[4]]]:null,
        scope==='entry'?['SLOT',String(state.chainSlot+1).padStart(2,'0')]:null,
        scope==='entry'?['PATT',selected?.enabled?clipLabel(selected.clip):'-']:null,
        random?['LOW',formatFadeTime(v[2])]:null,random?['HIGH',formatFadeTime(v[3])]:null,
        ['MODE',(scope==='entry'?['DEF','CUT','FIX','RAND']:['GLBL','FIX','RAND'])[mode]],null];
    controls.forEach((c,i)=>{const x=i%4*32,y=i<4?21:44;
        if(c){print(x+1,y,c[0],1);print(x+1,y+10,c[1],1);}
        else{drawRect(x+14,y+8,4,4,1);drawRect(x+15,y+9,2,2,0);}});
}
function adjustChainFade(scope,index,delta) {
    if(scope==='entry'&&index===2) {
        delta=parameterEncoderDelta(index,'chain_slot',delta);if(!delta)return;
        const slot=clamp(state.chainSlot+delta,0,63);if(slot===state.chainSlot)return;
        setParamBlocking('chain_slot',slot+1);
        state.chainSlot=slot;state.chainPage=Math.floor(slot/16);PARAMS.chain_slot.value=slot+1;
        parseChainEntries(getParam('chain_entries',''));readChainFades();
        resetParameterEncoderMotion(index);rebuildLedQueue();draw();return;
    }
    readChainFades();const v=state.chainFades.slice(scope==='entry'?5:0),mode=v[0];
    const fixed=scope==='entry'?mode===2:mode===1,random=scope==='entry'?mode===3:mode===2;
    if(index!==6 && !(index===0&&fixed) && !(index===1&&(fixed||random)) && !([4,5].includes(index)&&random))return;
    delta=parameterEncoderDelta(index,scope+'_fade_'+index,delta);if(!delta)return;
    const prefix=scope+'_fade_';
    if(index===6)setParamBlocking(prefix+'mode',clamp(mode+delta,0,scope==='entry'?3:2));
    else if(index===1)setParamBlocking(prefix+'curve',clamp(v[4]+delta,0,4));
    else {const current=index===0?v[1]:index===4?v[2]:v[3];let nearest=0;
        FADE_TIMES.forEach((x,i)=>{if(Math.abs(x-current)<Math.abs(FADE_TIMES[nearest]-current))nearest=i;});
        setParamBlocking(prefix+(index===0?'ms':index===4?'low_ms':'high_ms'),FADE_TIMES[clamp(nearest+delta,0,FADE_TIMES.length-1)]);}
    readChainFades();draw();
}

function libraryLeaf(path) {
    if (path === LIBRARY_HOME) return 'SOURCES';
    if (path === state.capturesDirectory) return 'THRU RECORDINGS';
    if (path === LIBRARY_ROOT) return 'MOVE SAMPLES';
    const parts = String(path || '').split('/');
    return parts[parts.length - 1] || 'Samples';
}

function drawLibrary() {
    clearDisplay();
    const leaf = libraryLeaf(state.libraryDir);
    const heading='L'+state.libraryLane+' '+PARAMS.preview_route.labels[Math.round(PARAMS.preview_route.value)]+(state.splitEnabled&&state.sharedDual&&PARAMS.preview_route.value===0?' '+partName():'')+' '+leaf;
    print(1, 0, heading.slice(0, 21), 1);
    drawRect(0, 8, 128, 1, 1);
    if (state.libraryItems.length === 0) {
        print(2, 23, 'NO WAV FILES', 1);
    }
    for (let row = 0; row < 4; row++) {
        const index = state.libraryScroll + row;
        const item = state.libraryItems[index];
        if (!item) continue;
        const prefix = index === state.libraryIndex ? '>' : ' ';
        const suffix = item.directory ? '/' : '';
        print(1, 12 + row * 11,
            (prefix + item.name + suffix).slice(0, 21), 1);
    }
    let footer='7:'+PARAMS.preview_route.labels[Math.round(PARAMS.preview_route.value)]+' 8:'+Math.round(PARAMS.preview_level.value*100)+'% CLICK';
    if (state.sampleLoadState >= 1 && state.sampleLoadState <= 5) {
        footer = 'LOADING ' + Math.round(state.sampleLoadProgress) + '%';
    } else if (state.sampleLoadState === 7) {
        footer = state.sampleLoadName || 'LOAD ERROR';
    } else if (state.sampleLoadState === 8) {
        footer='7:'+PARAMS.preview_route.labels[Math.round(PARAMS.preview_route.value)]+' 8:'+Math.round(PARAMS.preview_level.value*100)+'% CLICK';
    }
    if(state.saveStatus===4)footer=saveNotice();
    print(1, 57, footer.slice(0, 21), 1);
}

function performancePressureAmount(pad,value) {
    const pressure=clamp(value/64,0,1),curve=pressure*(2-pressure),floor=pad<4?0.2:0.65;
    return floor+(1-floor)*curve;
}
function performanceLaneEnabled(lane) {
    return !isQuad() && (state.mask & (1<<lane))!==0
        && (!state.splitEnabled || state.sharedDual || Math.floor(lane/2)===state.performanceOwner);
}
function endPerformanceLayer(send = true) {
    if(state.focusButtonHeld)state.focusButtonUsed=true;
    state.performanceArmed=false;
    state.performanceVisible=false;
    state.performanceValues.fill(0);state.performancePadLive.clear();
    splitDisplay.valid=false;
    if(send)setParam('performance_cancel',1);
    rebuildLedQueue();
}
function drawPerformanceLayer() {
    clearDisplay();
    print(1,0,'PERFORM'+(state.splitEnabled?' '+String.fromCharCode(65+state.performanceOwner):'')+' BOTTOM 8',1);
    drawRect(0,8,128,1,1);
    const names=['L1','L2','L3','L4','DARK','CRSH','ECHO','SPCE'];
    for(let index=0;index<8;++index) {
        const x=(index%4)*32,y=11+Math.floor(index/4)*21;
        const enabled=index>=4||performanceLaneEnabled(index);
        const pressed=state.performancePadLive.has(MovePads[index]);
        if(enabled&&pressed)drawRect(x,y,31,18,1);
        print(x+2,y,names[index],enabled&&pressed?0:1);
        let amount=state.performanceValues[index];
        if(index<4 && pressed && enabled) {
            let sum=0;
            for(let lane=0;lane<4;++lane)if(performanceLaneEnabled(lane)&&state.performancePadLive.has(MovePads[lane]))sum+=state.performanceValues[lane];
            amount=sum>0?amount/sum:0;
        }
        print(x+2,y+9,enabled?Math.round(amount*100)+'%':'--',enabled&&pressed?0:1);
    }
    print(1,57,'RELEASE TO RETURN',1);
}
function draw() {
    const overlay=state.saveStatus===2?'saving':state.savedTicks>0&&state.saveStatus===3?'saved':'';
    if(overlay!==state.saveOverlayLast){splitDisplay.footer=null;layersDisplay.fx=null;state.saveOverlayLast=overlay;}
    if(state.performanceVisible)drawPerformanceLayer();
    else if(customMidiViews.includes(state.view))drawCustomMidi();
    else if(state.view==='layerFxPicker')drawLayersPicker();
    else if(state.view==='projects')drawProjects();
    else if(state.view==='bankRecovery')drawBankRecovery();
    else if(state.view==='sliceEdit')drawSliceEditor();
    else if(state.view==='mode')drawModePicker();
    else if(state.view==='pages')drawPagePicker();
    else if(state.view==='mixStatus')drawMixStatus();
    else if (state.view === 'help') drawHelp();
    else if (state.view === 'settings') drawSettings();
    else if (state.view === 'clips') drawClips();
    else if (state.view === 'chain') drawChain();
    else if (state.view === 'library') drawLibrary();
    else if (state.view === 'fxMenu') drawSplitFxMenu();
    else if(sequenceUi.active)drawSequence();
    else drawMain();
    drawSaveIndicator();
}

function drawSaveIndicator() {
    if(state.view==='projects')return; // The project panel owns progress and completion feedback.
    // Fixed footer overlay: parameter cells and waveform coordinates stay put.
    if(state.saveStatus===4) {
        if((state.view!=='main'&&state.view!=='bankRecovery'&&state.view!=='projects')||state.performanceVisible) {
            drawRect(0,57,128,7,0);print(1,57,saveNotice(),1);
        }
        return;
    }
    const text=state.saveStatus===2?'saving':state.savedTicks>0&&state.saveStatus===3?'saved':'';
    if(!text)return;
    const glyphs={s:[7,4,7,1,7],a:[0,7,5,7,5],v:[0,5,5,5,2],i:[2,0,2,2,2],n:[0,6,5,5,5],g:[0,7,5,7,1],e:[0,7,6,4,7],d:[1,1,7,5,7]};
    const x=127-text.length*4;
    drawRect(x-2,57,text.length*4+3,7,0);
    for(let i=0;i<text.length;++i)for(let row=0;row<5;++row)
        for(let col=0;col<3;++col)if(glyphs[text[i]][row]&(4>>col))drawRect(x+i*4+col,58+row,1,1,1);
}

function selectLane(lane) {
    endPerformanceLayer();
    if (state.recording&&!sequenceUi.active) return;
    if(sequenceUi.active){setParamBlocking('seq_hold',-1);if(sequenceUi.pad>=0)sequenceUi.held=true;state.view='main';}
    state.lane = clamp(lane | 0, 1, 4);
    setParamBlocking('record_lane', state.lane);
    parseSplitState(getParam('split_state','{}'));
    readSequence();showMessage(sequenceUi.active?'EDIT LANE '+lane:'RECORD LANE ' + state.lane);
    rebuildLedQueue();
}

function statPath(path) {
    try {
        const result = os.stat(path);
        return Array.isArray(result) ? result[0] : result;
    } catch (error) {
        return null;
    }
}

function pathIsDirectory(path) {
    const info = statPath(path);
    return !!info && Number.isFinite(info.mode)
        && (info.mode & 0xf000) === 0x4000;
}

function scanLibrary(path) {
    if (path === LIBRARY_HOME) {
        const items = [{
            name: 'Move Samples', directory: true, path: LIBRARY_ROOT,
        }];
        if (state.capturesDirectory) {
            items.push({
                name: 'Thru Recordings', directory: true,
                path: state.capturesDirectory,
            });
        }
        state.libraryDir = path;
        state.libraryItems = items;
        state.libraryIndex = 0;
        state.libraryScroll = 0;
        return;
    }
    let names = [];
    try {
        const result = os.readdir(path);
        names = Array.isArray(result) && Array.isArray(result[0])
            ? result[0] : result;
    } catch (error) {}
    if (!Array.isArray(names)) names = [];
    const items = [];
    if (path !== LIBRARY_ROOT && path !== state.capturesDirectory) {
        items.push({ name: '..', directory: true, path: LIBRARY_HOME });
    }
    for (const name of names) {
        if (!name || name[0] === '.') continue;
        const fullPath = path + '/' + name;
        const directory = pathIsDirectory(fullPath);
        if (!directory && !String(name).toLowerCase().endsWith('.wav')) continue;
        items.push({ name: String(name), directory, path: fullPath });
    }
    const parent = items.length > 0 && items[0].name === '..' ? items.shift() : null;
    items.sort((a, b) => Number(b.directory) - Number(a.directory)
        || (path === state.capturesDirectory
            ? b.name.toLowerCase().localeCompare(a.name.toLowerCase())
            : a.name.toLowerCase().localeCompare(b.name.toLowerCase())));
    if (parent) items.unshift(parent);
    state.libraryDir = path;
    state.libraryItems = items;
    state.libraryIndex = 0;
    state.libraryScroll = 0;
}

function openLibrary(lane) {
    endPerformanceLayer();
    if (state.recording) return;
    state.libraryLane = clamp(lane | 0, 1, 4);
    state.lane = state.libraryLane;
    setParamBlocking('record_lane', state.lane);
    parseSplitState(getParam('split_state','{}'));
    state.capturesDirectory = String(getParam(
        'captures_directory', state.capturesDirectory) || '');
    scanLibrary(LIBRARY_HOME);
    state.view = 'library';
    previewLibraryItem();
    rebuildLedQueue();
}

function moveLibrarySelection(delta) {
    if (state.libraryItems.length === 0 || delta === 0) return;
    state.libraryIndex = clamp(state.libraryIndex + (delta > 0 ? 1 : -1),
        0, state.libraryItems.length - 1);
    if (state.libraryIndex < state.libraryScroll) {
        state.libraryScroll = state.libraryIndex;
    } else if (state.libraryIndex >= state.libraryScroll + 4) {
        state.libraryScroll = state.libraryIndex - 3;
    }
    previewLibraryItem();
}

function previewLibraryItem() {
    const item = state.libraryItems[state.libraryIndex];
    if (!item || item.directory || item.name === '..') {
        setParam('preview_stop', 1);
        return;
    }
    setParam('preview_file', item.path);
}

function libraryBack() {
    if (state.libraryDir === LIBRARY_HOME) {
        if(returnToPagePicker())return;
        setParam('preview_stop', 1);
        state.view = 'main';
        rebuildLedQueue();
        return;
    }
    if (state.libraryDir === LIBRARY_ROOT
        || state.libraryDir === state.capturesDirectory) {
        scanLibrary(LIBRARY_HOME);
        return;
    }
    const slash = state.libraryDir.lastIndexOf('/');
    scanLibrary(state.libraryDir.slice(0, slash));
}

function activateLibraryItem() {
    const item = state.libraryItems[state.libraryIndex];
    if (!item) return;
    if (item.name === '..') {
        libraryBack();
        return;
    }
    const path = item.path;
    if (item.directory) {
        scanLibrary(path);
        previewLibraryItem();
        return;
    }
    setParamBlocking('load_lane_file', state.libraryLane + ',' + path);
    state.sampleLoadState = 1;
    state.sampleLoadProgress = 0;
    state.sampleLoadName = item.name;
    state.laneSource = item.name;
    state.view = 'main';state.pageReturn=null;state.pageDestinationReturn=null;
    showMessage('LOADING L' + state.libraryLane + ' ' + item.name, 90);
    rebuildLedQueue();
}

function toggleClipView() {
    if (state.view === 'clips') {
        state.view = 'chain';
    } else if (state.view === 'chain') {
        state.view = 'main';
    } else {
        state.clipBank = Math.floor(state.activeClip / 16);
        state.view = 'clips';
    }
    rebuildLedQueue();
}

function selectChainSlot(step) {
    resetParameterEncoderMotion();
    const destination = state.chainPage*16+clamp(step | 0, 0, 15);
    if (state.deleteHeld) {
        setParamBlocking('chain_delete', destination);
        state.chainEntries.splice(destination, 1);
        state.chainEntries.push({ enabled: false, clip: 0, repeats: 1 });
        state.chainSlot = clamp(destination, 0, 63);
        showMessage('DELETED CHAIN ' + (destination + 1));
    } else if (state.shiftHeld && destination !== state.chainSlot) {
        const source = state.chainSlot;
        setParamBlocking('chain_move', source + ',' + destination);
        const moved = state.chainEntries.splice(source, 1)[0];
        state.chainEntries.splice(destination, 0, moved);
        state.chainSlot = destination;
        showMessage('MOVED ' + (source + 1) + ' TO ' + (destination + 1));
    } else {
        state.chainSlot = destination;
        showMessage('CHAIN SLOT ' + (destination + 1));
    }
    setParamBlocking('chain_slot', state.chainSlot + 1);
    const entry = state.chainEntries[state.chainSlot];
    PARAMS.chain_slot.value = state.chainSlot + 1;
    PARAMS.chain_clip.value = entry.enabled ? entry.clip + 1 : state.activeClip + 1;
    PARAMS.chain_repeats.value = entry.enabled ? entry.repeats : 1;
    rebuildLedQueue();
}

function adjustChainClip(delta) {
    if (delta === 0) return;
    const entry = state.chainEntries[state.chainSlot];
    const current = entry.enabled ? entry.clip : state.activeClip;
    const clip = clamp(current + (delta > 0 ? 1 : -1), 0, 63);
    setParamBlocking('chain_slot', state.chainSlot + 1);
    setParamBlocking('chain_clip', clip + 1);
    entry.enabled = true;
    entry.clip = clip;
    PARAMS.chain_clip.value = clip + 1;
    showMessage('CHAIN ' + (state.chainSlot + 1) + ' CLIP ' + clipLabel(clip));
    rebuildLedQueue();
}

function adjustChainRepeats(delta) {
    if (delta === 0) return;
    const entry = state.chainEntries[state.chainSlot];
    if (!entry.enabled) {
        entry.enabled = true;
        entry.clip = state.activeClip;
    }
    entry.repeats = clamp(entry.repeats + (delta > 0 ? 1 : -1), 0, 16);
    setParamBlocking('chain_slot', state.chainSlot + 1);
    setParamBlocking('chain_clip', entry.clip + 1);
    setParamBlocking('chain_repeats', entry.repeats);
    PARAMS.chain_repeats.value = entry.repeats;
    showMessage('CHAIN ' + (state.chainSlot + 1) + ' x' + entry.repeats);
    rebuildLedQueue();
}

function insertActiveClipInChain() {
    setParamBlocking('chain_insert', state.chainSlot + ',' + state.activeClip);
    state.chainEntries.splice(state.chainSlot, 0, {
        enabled: true, clip: state.activeClip, repeats: 1,
    });
    state.chainEntries.length = 64;
    PARAMS.chain_slot.value = state.chainSlot + 1;
    PARAMS.chain_clip.value = state.activeClip + 1;
    PARAMS.chain_repeats.value = 1;
    showMessage('INSERTED ' + clipLabel(state.activeClip));
    rebuildLedQueue();
}

function selectClipSlot(step) {
    const clip = state.clipBank * 16 + step;
    if (state.clipRecording) {
        showMessage('STOP DATA RECORD FIRST');
        return;
    }
    if (state.deleteHeld) {
        setParamBlocking('clip_reset', clip);
        showMessage('CLEARED LOOP ' + clipLabel(clip));
    } else if (state.shiftHeld) {
        setParamBlocking('clip_copy', clip);
        if (clip < 32) state.clipMaskLow |= 1 << clip;
        else state.clipMaskHigh |= 1 << (clip - 32);
        showMessage('COPIED TO ' + clipLabel(clip));
    } else {
        setParamBlocking('maintenance', 1);
        setParamBlocking('clip_select', clip);
        state.queuedClip = clip === state.activeClip ? -1 : clip;
        showMessage((state.queuedClip >= 0 ? 'PREPARING ' : 'CURRENT ')
            + clipLabel(clip));
    }
    rebuildLedQueue();
}

function selectAlgorithm(index) {
    state.algorithm = clamp(index | 0, 0, ALGORITHMS.length - 1);
    PARAMS.algorithm.value = state.algorithm;
    if(voiceEngineListEnabled())PARAMS['voice_engine_'+Math.round(PARAMS.voice_engine_edit.value)].value=state.algorithm;
    setParamBlocking('algorithm', state.algorithm);
    if (state.clipRecording) setParam('clip_automation', 'algorithm');
    showMessage(ALGORITHMS[state.algorithm]);
    rebuildLedQueue();
}

function toggleRecording() {
    if (state.clipRecording) {
        showMessage('STOP DATA RECORD FIRST');
        return;
    }
    const next = state.recording ? 0 : 1;
    setParamBlocking('record_gate', next);
    state.recording = next !== 0;
    if (!state.recording) setParamBlocking('maintenance', 1);
    showMessage(state.recording ? 'RECORDING LANE ' + state.lane : 'RECORD STOPPED');
    rebuildLedQueue();
}

function togglePerformanceRecording() {
    if (state.recording) {
        showMessage('STOP AUDIO RECORD FIRST');
        return;
    }
    setParamBlocking('clip_record', 1);
    syncFxOrder();
    parseClipState(getParam('clip_state', ''));
    parseClipTakeState(getParam('clip_take_state', '0,0,0'));
    showMessage(state.clipRecording
        ? ((PARAMS.clip_record_mode.value | 0) === 0
            ? 'RECORD PERFORMANCE' : 'OVERDUB PERFORMANCE')
        : 'LOOP CAPTURED');
    rebuildLedQueue();
}

function toggleClipPlaying() {
    const running = state.clipPlaying || state.clipLaunchPending;
    setParamBlocking('clip_playing', running ? 0 : 1);
    parseClipState(getParam('clip_state', ''));
    showMessage(state.clipLaunchPending ? 'LOOP QUEUED'
        : (state.clipPlaying ? 'LOOP PLAY' : 'LOOP STOP'));
    rebuildLedQueue();
}

function padMidiNote(padIndex) {
    const owner=padSettings(padIndex);
    if (owner.mode === 1 || owner.mode === 3) {
        return Math.round(owner.root||60);
    }
    const row = isQuad()?Math.floor(padIndex/8)%2:Math.floor(padIndex/8);
    const column = padIndex % (state.splitEnabled||layersUi.active?4:8);
    if (owner.scale !== 0) {
        const intervals = SCALE_INTERVALS[owner.scale] || SCALE_INTERVALS[0];
        const degree = row * 3 + column;
        const octave = Math.floor(degree / intervals.length);
        return clamp(Math.round(owner.root||60) - 12
            + (owner.octave||0) * 12 + octave * 12
            + intervals[degree % intervals.length], 0, 127);
    }
    return clamp((state.splitEnabled||layersUi.active?(owner.root||60)-12:48) + (owner.octave||0) * 12 + row * 5 + column, 0, 127);
}

function adjustOctave(delta) {
    const next = clamp(state.octave + (delta > 0 ? 1 : -1), -3, 3);
    if (next === state.octave) return;
    state.octave = next;
    setParamBlocking('pad_octave', state.octave);
    showMessage('OCTAVE ' + (state.octave >= 0 ? '+' : '') + state.octave);
    rebuildLedQueue();
}

function adjustClipBank(delta) {
    const next = clamp(state.clipBank + (delta > 0 ? 1 : -1), 0, 3);
    if (next === state.clipBank) {
        showMessage('CLIP BANK ' + BANK_NAMES[next]);
        return;
    }
    state.clipBank = next;
    showMessage('CLIP BANK ' + BANK_NAMES[next]);
    rebuildLedQueue();
}

function adjustSettingsPage(delta) {
    if (currentSettingsPage().route) cancelFxOrder(false);
    const pages = parameterPages();
    const previous=pages[state.settingsPage];
    const category=pageCategoryForPage(pages[state.settingsPage]);
    if(category>=0) {
        const indices=categorySettingsIndices(category,pages);
        const current=Math.max(0,indices.indexOf(state.settingsPage));
        state.settingsPage=indices[(current+(delta>0?1:-1)+indices.length)%indices.length];
    } else if(isMidiPage(pages[state.settingsPage])) {
        const indices=pages.map((p,i)=>isMidiPage(p)?i:-1).filter(i=>i>=0);
        state.settingsPage=indices[(indices.indexOf(state.settingsPage)+(delta>0?1:-1)+indices.length)%indices.length];
    } else {
        state.settingsPage = (state.settingsPage + (delta > 0 ? 1 : -1)
            + pages.length) % pages.length;
    }
    if(Number.isInteger(previous?.sequenceStrip)||Number.isInteger(pages[state.settingsPage]?.sequenceStrip))
        resetParameterEncoderMotion();
    state.actionMessageTicks = 0;
    rememberLayersPage();
    if(currentSettingsPage().pathEditor)readStackPath();
    if(currentSettingsPage().patternFade)readPatternFade();
}

function applyMomentaryControl(control) {
    control.value = 0;
    if (['layers_copy', 'layers_paste'].includes(control.key)) {
        setParamBlocking(control.key, 1);
        const error = String(getParam('last_error', '') || '');
        readLayers();
        const action = {layers_copy:'COPIED', layers_paste:'PASTED'}[control.key];
        showActionMessage(error ? (error.startsWith('Copy ') ? 'COPY CORNER FIRST' : 'CORNER EDIT FAILED')
            : action + ' FX C' + (PARAMS.layers_corner.value + 1));
        return;
    }
    if (control.key === 'clip_take_undo') {
        if (state.clipRecording) {
            showActionMessage('STOP RECORDING FIRST');
            return;
        }
        if (!state.clipTakeCanUndo) {
            showActionMessage('BASE TAKE PROTECTED');
            return;
        }
        const removed = state.clipTakeCount;
        setParamBlocking('clip_take_undo', 1);
        parseClipTakeState(getParam('clip_take_state', '0,0,0'));
        showActionMessage('UNDID TAKE ' + removed);
        return;
    }
    if (state.recording) {
        showActionMessage('STOP RECORDING FIRST');
        return;
    }
    if ((state.mask & (1 << (state.lane - 1))) === 0) {
        showActionMessage('LANE ' + state.lane + ' IS EMPTY');
        return;
    }
    if (state.clipPrepareState === 1 || state.clipPrepareState === 2) {
        showActionMessage('WAIT FOR CLIP');
        return;
    }
    setParamBlocking(control.key, 1);
    const error = String(getParam('last_error', '') || '');
    if (error) {
        showActionMessage('EDIT FAILED');
        return;
    }
    state.laneSource = String(getParam('lane_source', control.result)
        || control.result);
    showActionMessage((control.result || 'APPLIED') + ' L' + state.lane);
}

function nextControlValue(control,delta,fine=false) {
    if(control.values){const index=Math.max(0,control.values.indexOf(Math.round(control.value)));return control.values[clamp(index+delta,0,control.values.length-1)];}
    if(control.choiceCount)return clamp(Math.round(control.value*(control.choiceCount-1))+delta,0,control.choiceCount-1)/(control.choiceCount-1);
    if(fine&&!control.labels)return clamp(control.value+delta*(control.integer?Math.max(1,control.step):control.step*.1),control.min,control.max);
    let value;
    if(control.key==='seq_gate'||control.key==='seq_trig_gate') {
        // Duration in steps needs fine short gates and quick access to long notes.
        const v=control.value;
        const toIndex=x=>x<=1?Math.round((x-.01)/.01):x<=4?99+Math.round((x-1)/.1):129+Math.round((x-4)/.5);
        const index=clamp(toIndex(v)+delta,0,185);
        value=index<=99?.01+index*.01:index<=129?1+(index-99)*.1:4+(index-129)*.5;
        value=Number(value.toFixed(2));
    } else if(['seq_time_ms','seq_trig_time_ms','seq_eq_freq','seq_duck_attack','seq_duck_hold','seq_duck_release','seq_duck_hpf'].includes(control.key)) {
        if(control.min===0) {
            const index=Math.round(control.value/control.max*96)+delta;
            value=control.max*clamp(index,0,96)/96;
        } else value=logarithmicControlAt(control,logarithmicControlIndex(control)+delta);
    } else if(['pulsar_rate','pulsar_form','pulsar_size','pulsar_emit_mod_rate','pulsar_form_mod_rate'].includes(control.key)) {
        // Preserve turn precision when extending SIZE and the low FORM range.
        const steps=control.key==='pulsar_size'?352:control.key==='pulsar_form'?396:240;
        const i=Math.round(steps*Math.log(control.value/control.min)/Math.log(control.max/control.min))+delta;
        value=Number((control.min*Math.pow(control.max/control.min,clamp(i,0,steps)/steps)).toFixed(4));
    } else if(control.key==='stochastic_evolution') {
        value=stochasticEvolutionAt(stochasticEvolutionIndex(control.value)+delta);
    } else if (control.key === 'effect_reverb_decay' || control.key === 'effect_reverb_damp') {
        value = logarithmicControlAt(control, logarithmicControlIndex(control) + delta);
    } else if (control.key === 'long_grain_length') {
        value = longGrainLengthAt(longGrainLengthIndex(control.value) + delta);
    } else if (control.key === 'effect_rotary_ramp') {
        value = rotaryRampAt(rotaryRampIndex(control.value) + delta);
    } else if (control.key === 'effect_rotary_crossover') {
        value = rotaryCrossoverAt(
            rotaryCrossoverIndex(control.value) + delta);
    } else if (control.key === 'effect_tape_wow_rate'
        || control.key === 'effect_tape_flutter_rate') {
        value = logarithmicControlAt(control,
            logarithmicControlIndex(control) + delta);
    } else if (control.key === 'split_duck_attack'
        || control.key === 'split_duck_release'
        || control.key === 'split_duck_hpf') {
        value = logarithmicControlAt(control,
            logarithmicControlIndex(control) + delta);
    } else if (control.key === 'hold_release_ms') {
        value = holdReleaseAt(holdReleaseIndex(control.value) + delta);
    } else if (control.key === 'amp_attack'
        || control.key === 'amp_decay' || control.key === 'amp_release') {
        value = envelopeTimeAt(envelopeTimeIndex(
            control.value, control.max) + delta, control.max);
    } else {
        value = clamp(control.value + delta * control.step,
            control.min, control.max);
    }
    if (control.integer) value = Math.round(value);
    return value;
}

function adjustSequenceEnvelope(index,delta){
    if(index>3||!delta)return;
    const key=SEQUENCE_ENVELOPE_PAGE.keys[index];
    delta=parameterEncoderDelta(index,key,delta);if(!delta)return;
    if(index===2){
        const control=controlForKey(key),value=nextControlValue(control,delta);
        PARAMS[key].value=value;setParam(key,String(value));
    }else{
        const timing=sequenceAmpTiming(),field=index===3?2:index;
        const current=timing.applied[field];
        const step=timing.dur<10?.1:timing.dur<100?1:timing.dur<1000?5:timing.dur<10000?20:100;
        const wanted=clamp(current+delta*step,0,timing.dur);
        const others=timing.raw.reduce((sum,value,i)=>sum+(i===field?0:value),0);
        const raw=wanted<=timing.dur-others||others<=1e-9?wanted
            :wanted*others/Math.max(1e-9,timing.dur-wanted);
        const value=clamp(Math.round(Math.max(index===3?3:0,raw)*10)/10,0,PARAMS[key].max);
        PARAMS[key].value=value;setParam(key,value.toFixed(1));
    }
    if(state.clipRecording&&LOCKABLE_PARAMS.has(key))setParam('clip_automation',key);
    showMessage(SHORT_LABELS[key]+' '+(index===2?Math.round(PARAMS[key].value)+'%':sequenceTimeText(sequenceAmpTiming().applied[index===3?2:index])),35);
}
function adjustControl(index, delta) {
    if(!state.editorReady){showMessage('UPDATING EDITOR');return;}
    if(state.view==='settings'&&currentSettingsPage().sequenceEnvelope){adjustSequenceEnvelope(index,delta);return;}
    if(state.view==='settings'&&currentSettingsPage().chainTools){adjustChainTools(index,delta);return;}
    if(state.view==='settings'&&currentSettingsPage().pathEditor){adjustPathEditor(index,delta);return;}
    if(state.view==='settings'&&currentSettingsPage().patternFade){adjustPatternFade(index,delta);return;}
    if(state.view==='settings'&&currentSettingsPage().chainFade){adjustChainFade(currentSettingsPage().chainFade,index,delta);return;}
    if (state.view === 'settings' && currentSettingsPage().route) {
        if (delta !== 0) adjustFxOrder(index, delta);
        return;
    }
    const keys = state.view==='chain'?CHAIN_PAGE.keys:activeControlKeys();
    const control = controlForKey(keys[index]);
    if (!control || delta === 0) return;
    if(control.key==='seq_when'&&sequenceUi.active&&Math.round(PARAMS.seq_child.value)===0){showMessage('CHOOSE EVENT 1-8');return;}
    if(control.key==='chain_advance'&&state.shiftHeld){if(delta>0)chainAction('chain_launch','CHAIN ENTRY QUEUED');return;}
    if(control.key==='chain_advance'){if(delta>0&&state.chainEnabled)setParamBlocking('chain_advance',1);showActionMessage(state.chainEnabled?'NEXT AT LOOP END':'CHAIN OFF');return;}
    if (control.key === 'chain_clear') {
        parameterEncoderMotion[index].key = control.key;
        parameterEncoderMotion[index].remainder = 0;
        setParamBlocking('chain_clear', 1);
        PARAMS.chain_clip.value = 1;
        PARAMS.chain_repeats.value = 1;
        showActionMessage('CLEARED CHAIN ' + (state.chainSlot + 1));
        return;
    }
    if(control.key.startsWith('seq_')&&state.deleteHeld){const field=['seq_pos','seq_note','seq_vel','seq_trig_gate'].indexOf(control.key);if(field>=0||control.key==='seq_trig_time_ms'){setParamBlocking('seq_clear',control.key==='seq_trig_time_ms'?3:field);readSequence();showMessage('INHERIT LANE');return;}}
    if(control.key.startsWith('seq_')&&control.momentary){setParamBlocking(control.key,control.key==='seq_clear'?4:1);readSequence();return;}
    if (control.momentary) {
        parameterEncoderMotion[index].key = control.key;
        parameterEncoderMotion[index].remainder = 0;
        applyMomentaryControl(control);
        return;
    }
    delta = parameterEncoderDelta(index, control.key, delta);
    if (delta === 0) return;
    const value=nextControlValue(control,delta);
    const stripPage=state.view==='settings'?currentSettingsPage():null;
    if(Number.isInteger(stripPage?.sequenceStrip)){
        const field=SEQUENCE_STRIP_KEYS.indexOf(control.key);
        if(field>=0){
            setParamBlocking('seq_strip_edit',stripPage.sequenceStrip+','+field+','+value);
            readSequence();return;
        }
    }
    control.value = value;
    PARAMS[control.key].value = value;
    if(control.key.startsWith('seq_')){setParamBlocking(control.key,value);readSequence();return;}
    if (control.key === 'algorithm') {
        selectAlgorithm(value);
        return;
    }
    if(control.key.startsWith('voice_engine_')){
        const title=currentSettingsPage().title;
        setParamBlocking(control.key,value);refreshEditorSnapshot();
        const index=parameterPages().findIndex(p=>p.title===title);if(index>=0)state.settingsPage=index;
        showMessage(control.label+' '+formatControlValue(control));return;
    }
    if(['split_mode','split_focus','part_mute'].includes(control.key)){setParamBlocking(control.key,value);parseSplitState(getParam('split_state','{}'));}
    else if(control.key.startsWith('chain_'))setParamBlocking(control.key,value.toFixed(control.integer?0:4));
    else setParam(control.key, value.toFixed(control.integer ? 0 : 4));
    if(control.key==='chain_bank'){parseChainState(getParam('chain_state','0,0,0,0,-1,0,0,0'));parseChainEntries(getParam('chain_entries',''));readChainFades();resetParameterEncoderMotion(index);}
    if (control.key === 'chain_enabled') {
        state.chainEnabled = value !== 0;
    } else if (control.key === 'chain_slot') {
        state.chainSlot = clamp(Math.round(value) - 1, 0, 63);state.chainPage=Math.floor(state.chainSlot/16);
        PARAMS.chain_clip.value = clamp(getNumber('chain_clip', 1), 1, 64);
        PARAMS.chain_repeats.value = clamp(getNumber('chain_repeats', 1), 0, 16);
    } else if (control.key === 'pad_mode') {
        state.padMode = clamp(Math.round(value), 0, 3);
        rebuildLedQueue();
    } else if (control.key === 'scale') {
        state.scale = clamp(Math.round(value), 0, SCALE_LABELS.length - 1);
        state.activePads.clear();
        rebuildLedQueue();
    } else if (control.key === 'root_note') {
        rebuildLedQueue();
    } else if (control.key === 'record_source') {
        state.recordSource = clamp(Math.round(value), 0, state.splitEnabled?(isQuad()?5:3):1);
    } else if (control.key === 'clip_rate') {
        state.clipRate = value;
    }
    if (state.clipRecording && LOCKABLE_PARAMS.has(control.key)) {
        setParam('clip_automation', control.key);
    }
    showMessage(control.label + ' ' + formatControlValue(control), 35);
    if(state.splitEnabled && [...SPLIT_SEND_KEYS,...QUAD_SEND_PAGE.keys].includes(control.key) && value>0) {
        const delay=control.key.includes('delay');
        if(PARAMS[delay?'effect_mix':'effect_reverb_mix'].value<=0)
            showActionMessage(delay?'RAISE DRTN: DELAY':'RAISE RRTN: REVERB');
    }
}

function pollDiagnostics() {
    readPatternFade();
    const performance = String(getParam('performance_state', '0,0'))
        .split(',').map((item) => Number(item));
    state.maximumRenderMicros = Number.isFinite(performance[0])
        ? Math.max(0, Math.round(performance[0])) : 0;
    state.renderOverruns = Number.isFinite(performance[1])
        ? Math.max(0, Math.round(performance[1])) : 0;
    const callback = String(getParam('callback_performance_state', '0,0,0,0'))
        .split(',').map((item) => Number(item));
    const bounded = (value) => Number.isFinite(value) ? Math.max(0, Math.round(value)) : 0;
    state.controlPeakMicros = bounded(callback[0]);
    state.queryPeakMicros = bounded(callback[1]);
    state.workPeakMicros = bounded(callback[2]);
    state.workOverruns = bounded(callback[3]);
    const timing=String(getParam('render_timing_state','0,0,0,0')).split(',').map(Number);
    state.renderCpuAvailable=timing[0]===1;
    state.renderCpuPeakMicros=bounded(timing[1]);state.renderWaitPeakMicros=bounded(timing[2]);state.renderCpuOverruns=bounded(timing[3]);
    const limiter = String(getParam('limiter_state', '0,0'))
        .split(',').map((item) => Number(item));
    state.limiterReductionDb = Number.isFinite(limiter[0])
        ? Math.max(0, limiter[0]) : 0;
    state.limiterMaximumReductionDb = Number.isFinite(limiter[1])
        ? Math.max(0, limiter[1]) : 0;
}

function pollDsp() {
    const previousPage=state.view==='settings'?parameterPages()[state.settingsPage]:null;
    const keepPage=previousPage&&(previousPage.scopeLabel===''||previousPage.patternFade||previousPage.chainFade||previousPage.chainTools||previousPage===CHAIN_PAGE)?previousPage:null;
    if(!state.editorReady)refreshEditorSnapshot();
    if ((state.fxRouteGrabbed || state.fxRoutePending)
        && !(state.view === 'settings' && currentSettingsPage().route)) cancelFxOrder(false);
    const algorithm = clamp(Math.round(getNumber('algorithm', state.algorithm)),
        0, ALGORITHMS.length - 1);
    if (algorithm !== state.algorithm) {
        state.algorithm = algorithm;
        PARAMS.algorithm.value = algorithm;
        state.settingsPage = clamp(state.settingsPage, 0, parameterPages().length - 1);
    }
    parseSplitState(getParam('split_state','{}'));if(PARAMS.split_mode.value===6||sequenceUi.active)readSequence();
    parseVoiceEngineState(getParam('voice_engine_state',''));
    state.voiceAlgorithms=String(getParam('voice_algorithms','')).split(',').map(Number);
    parseSaveStatus(getParam('save_status',''));
    parseMixStatus(getParam('mix_status',''));
    PARAMS.preview_route.value=clamp(getNumber('preview_route',PARAMS.preview_route.value),0,1);
    parseUiState(getParam('ui_state', ''));
    parsePlayheads(getParam('playheads', ''));
    parseWavesetState(getParam('waveset_state', ''));
    parseCutupsState(getParam('cutups_state', ''));
    state.spectralPending=Number(String(getParam('spectral_analysis_state','0,0')).split(',')[1])||0;
    state.wavesetAnalysisPending = Number(String(getParam(
        'waveset_analysis_state', '0,0,0')).split(',')[1]) || 0;
    parseStackPositions(getParam('voice_lanes', ''));
    parseVoicePads(getParam('voice_pads', ''));
    parseVoiceNotes(getParam('voice_notes', ''));
    parseRollPads(getParam('roll_pads', ''));
    parseRollNotes(getParam('roll_notes', ''));
    parseClipBankState(getParam('clip_bank_state', '0,-1,0,0'));
    parseClipState(getParam('clip_state', ''));
    parseClipTakeState(getParam('clip_take_state', '0,0,0'));
    parseClipCapacityState(getParam('clip_capacity_state', '0,1024,0,8192,0,0'));
    parseSampleLoadState(getParam('sample_load_state', '0,1,0,'));
    parseRegionState(getParam('region_state', '0'));
    parseCueState(getParam('cue_state', '-1,-1,-1,-1,-1,-1,-1,-1'));
    const nextPadMode = clamp(Math.round(
        getNumber('pad_mode', state.padMode)), 0, 3);
    const nextScale = clamp(Math.round(
        getNumber('scale', state.scale)), 0, SCALE_LABELS.length - 1);
    const nextRoot = clamp(Math.round(
        getNumber('root_note', PARAMS.root_note.value)), 24, 96);
    if (nextPadMode !== state.padMode || nextScale !== state.scale
        || nextRoot !== PARAMS.root_note.value) {
        state.padMode = nextPadMode;
        state.scale = nextScale;
        PARAMS.scale.value = nextScale;
        PARAMS.root_note.value = nextRoot;
        state.activePads.clear();
        rebuildLedQueue();
    }
    readPatternFade();
    parseClipPrepareState(getParam('clip_prepare_state', '0,-1'));
    parseChainState(getParam('chain_state', '0,0,0,0,-1'));
    parseChainEntries(getParam('chain_entries', ''));
    state.laneSource = String(getParam('lane_source', state.laneSource) || 'EMPTY');
    state.laneSourceState = String(getParam('lane_source_state',
        state.laneSourceState) || 'EMPTY:EMPTY');
    state.undoAvailable = getNumber('undo_available', 0) !== 0;
    state.stackScanPaused = getNumber('stack_scan_paused', 0) !== 0;
    state.recordedStackHold = getNumber('clip_stack_hold', 0) !== 0;
    state.glitchBufferLocked = getNumber('glitch_buffer_lock', 0) !== 0;
    if (!state.glitchBufferLocked && !state.glitchLockMomentary) {
        state.glitchLockLatched = false;
    }
    if (state.ticks % 60 === 0) pollDiagnostics();
    if(keepPage){const index=parameterPages().findIndex(page=>page.title===keepPage.title);if(index>=0)state.settingsPage=index;}
    if(state.view==='settings'&&currentSettingsPage().pathEditor)readStackPath();
    parseWaveforms(getParam('waveforms', ''));
    readBankState();
    readLayers();
    readSliceEditor();
}

// Slice editing is a focused source view, separate from the engine's live pages.
const sliceUi={data:null,wave:null,zoom:1,low:0,high:1,viewKey:'',returnPage:0,notice:'',noticeUntil:0};
function sliceEditorPadOwner(pad) {return !state.splitEnabled||padPart(pad)===sliceUi.data?.part;}
function sliceEditorPerPage() {return state.splitEnabled?(isQuad()?8:16):32;}
function sliceEditorCommand(key,value) {
    const before=sliceUi.data;
    setParamBlocking('slice_editor_'+key,value);
    readSliceEditor();const after=sliceUi.data;
    sliceUi.notice='';
    if(after&&before){
        if(key==='add'&&after.count===before.count)sliceUi.notice=before.count>=64?'64 SLICE LIMIT':'CUT TOO CLOSE';
        if(key==='delete'&&after.count===before.count)sliceUi.notice='EDGE PROTECTED';
        if(key==='lane'&&after.lane!==Number(value))sliceUi.notice='LANE UNAVAILABLE';
    }
    sliceUi.noticeUntil=state.ticks+40;rebuildLedQueue();draw();
}
function openSliceEditor() {
    setParamBlocking('slice_editor_open',1);
    let data;try{data=JSON.parse(String(getParam('slice_editor_state','{}')));}catch(e){}
    if(!data?.active){showActionMessage('LOAD A LANE FIRST');return;}
    sliceUi.notice='';sliceUi.noticeUntil=0;
    sliceUi.returnPage=state.settingsPage;sliceUi.data=data;sliceUi.zoom=1;sliceUi.viewKey='';sliceUi.wave=null;
    state.view='sliceEdit';state.deleteHeld=false;setParam('delete_held',0);
    resetParameterEncoderMotion();readSliceEditor();rebuildLedQueue();draw();
}
function closeSliceEditor(send=true) {
    if(send)setParamBlocking('slice_editor_open',0);
    sliceUi.data=null;sliceUi.wave=null;sliceUi.viewKey='';
    state.view='settings';state.settingsPage=clamp(sliceUi.returnPage,0,parameterPages().length-1);
    resetParameterEncoderMotion();rebuildLedQueue();
}
function readSliceEditor() {
    if(state.view!=='sliceEdit')return;
    let data;try{data=JSON.parse(String(getParam('slice_editor_state','{}')));}catch(e){return;}
    if(!data.active){closeSliceEditor(false);return;}
    if(data.v!==1||!Number.isInteger(data.count)||data.count<1||data.count>64
        ||!Array.isArray(data.points)||data.points.length!==data.count+1
        ||!data.points.every(Number.isFinite)||!(data.duration>0))return;
    data.selected=clamp(data.selected|0,0,data.count-1);
    data.boundary=clamp(data.boundary|0,0,data.count);
    data.page=clamp(data.page|0,0,Math.ceil(data.count/sliceEditorPerPage())-1);
    if(sliceUi.data?.lane!==data.lane){sliceUi.zoom=1;sliceUi.wave=null;sliceUi.viewKey='';}
    sliceUi.data=data;state.lane=data.lane;
    PARAMS.slice_region_mode.value=data.mode;
    const width=1/sliceUi.zoom;
    // At the full overview, keep both file edges visible. Zoomed navigation
    // scrolls the waveform beneath a fixed central edit cursor.
    const center=sliceUi.zoom===1?.5:data.cursor;
    sliceUi.low=center-width*.5;sliceUi.high=center+width*.5;
    const low=clamp(sliceUi.low,0,1),high=clamp(sliceUi.high,0,1);
    const viewKey=data.lane+':'+low.toFixed(9)+','+high.toFixed(9);
    if(viewKey!==sliceUi.viewKey){
        sliceUi.viewKey=viewKey;setParamBlocking('slice_editor_view',low.toFixed(9)+','+high.toFixed(9));
    }
    const wave=String(getParam('slice_editor_wave','')).split(',').map(Number);
    if(wave.length===4+WAVE_BINS*2&&wave.every(Number.isFinite)
        &&wave[0]>0&&wave[1]===data.lane&&wave[3]>wave[2])sliceUi.wave=wave;
}
function sliceEditorTime(value) {
    const ms=value*(sliceUi.data?.duration||0);
    return ms<1000?Math.round(ms)+'m':ms<10000?(ms/1000).toFixed(2)+'s':(ms/1000).toFixed(1)+'s';
}
function adjustSliceEditor(index,delta) {
    const d=sliceUi.data;if(!d||!delta||index>4)return;
    delta=parameterEncoderDelta(index,'slice-edit-'+index,delta);if(!delta)return;
    if(index===0){
        const selected=clamp(d.selected+delta,0,d.count-1);
        setParamBlocking('slice_editor_select',selected);
        setParamBlocking('slice_editor_page',Math.floor(selected/sliceEditorPerPage()));
    } else if(index===3){sliceUi.zoom=clamp(sliceUi.zoom*Math.pow(2,delta/4),1,65536);}
    else if(index===4)setParamBlocking('slice_editor_preroll',clamp(d.preroll+delta*(state.shiftHeld?.1:1),0,30).toFixed(1));
    else {
        const point=d.selected+(index===2?1:0);
        const step=state.shiftHeld?.1/d.duration:1/(sliceUi.zoom*256);
        setParamBlocking('slice_editor_move',point+','+clamp(d.points[point]+delta*step,0,1).toFixed(9));
    }
    readSliceEditor();rebuildLedQueue();draw();
}
function sliceEditorPadColor(index) {
    const d=sliceUi.data;if(!d||!sliceEditorPadOwner(index))return null;
    const slice=padLocal(index)+d.page*sliceEditorPerPage();
    if(slice>=d.count)return Black;
    if(d.playing&&slice===d.selected)return d.latched?LightMagenta:Cyan;
    return slice===d.selected?BrightOrange:WhiteLedDim;
}
function handleSliceEditorCc(number,value) {
    if(state.view!=='sliceEdit')return false;
    const d=sliceUi.data;if(!d)return false;
    if(number===MoveMainKnob){
        const delta=decodeDelta(value),step=state.shiftHeld?.1/d.duration:1/(sliceUi.zoom*128);
        sliceEditorCommand('cursor',clamp(d.cursor+delta*step,0,1).toFixed(9));return true;
    }
    if(number>=MoveKnob1&&number<MoveKnob1+8){adjustSliceEditor(number-MoveKnob1,decodeDelta(value));return true;}
    if(number===MoveMainButton){if(value>0)sliceEditorCommand('add',d.cursor.toFixed(9));return true;}
    if(number===MoveDelete){
        state.deleteHeld=false;setParam('delete_held',0);
        if(value>0)sliceEditorCommand('delete',d.boundary);return true;
    }
    if(number===MoveLoop){if(value>0)sliceEditorCommand('loop',d.loop?0:1);return true;}
    if(number===MoveBack||number===MoveMenu){
        if(value>0){closeSliceEditor();if(number===MoveMenu&&state.focusButtonHeld){state.focusButtonUsed=true;openPagePicker();}}
        return true;
    }
    const track=MoveTracks.indexOf(number);
    if(track>=0&&value>0){
        if(state.focusButtonHeld){closeSliceEditor();state.focusButtonUsed=true;openPageCategory(track);}
        else if(state.shiftHeld){closeSliceEditor();openLibrary(track+1);}
        else {sliceEditorCommand('lane',track+1);}
        return true;
    }
    if(number===MoveUp||number===MoveDown){
        if(value>0)sliceEditorCommand('page',clamp(d.page+(number===MoveUp?1:-1),0,Math.ceil(d.count/sliceEditorPerPage())-1));
        return true;
    }
    return false;
}
function drawSliceEditor() {
    clearDisplay();const d=sliceUi.data;if(!d)return;
    const index=String(d.selected+1).padStart(2,'0'),count=String(d.count).padStart(2,'0');
    if(sliceUi.notice&&state.ticks<sliceUi.noticeUntil)print(1,0,sliceUi.notice,1);
    else {
        print(1,0,'L'+d.lane+' S'+index+'/'+count,1);
        print(61,0,String(d.overflow?'LIMIT64':d.label||(d.loop?'LOOP':'ONCE')).slice(0,7),1);
        print(110,0,(d.page+1)+'/'+Math.ceil(d.count/sliceEditorPerPage()),1);
    }
    const xFor=p=>Math.round((p-sliceUi.low)/(sliceUi.high-sliceUi.low)*127);
    const wave=sliceUi.wave,ready=wave&&wave[1]===d.lane;
    for(let x=0;x<128;++x){
        const position=sliceUi.low+x/127*(sliceUi.high-sliceUi.low);
        if(position<0||position>1)continue;
        let lo=0,hi=0;
        if(ready&&position>=wave[2]&&position<=wave[3]){
            const bin=clamp(Math.floor((position-wave[2])/(wave[3]-wave[2])*WAVE_BINS),0,WAVE_BINS-1);
            lo=wave[4+bin*2];hi=wave[5+bin*2];
        } else {
            const bin=clamp(Math.floor(position*(WAVE_BINS-1)),0,WAVE_BINS-1);
            [lo,hi]=state.waves[d.lane-1]?.[bin]||[0,0];
        }
        const top=20-Math.round(clamp(hi,-7,7)*8/7),bottom=20-Math.round(clamp(lo,-7,7)*8/7);
        drawRect(x,top,1,Math.max(1,bottom-top+1),1);
    }
    for(let p=0;p<=d.count;++p){
        const x=xFor(d.points[p]);if(x<0||x>127)continue;
        if(p===d.selected||p===d.selected+1){
            drawRect(x,10,1,20,1);
            const y=p===d.selected?10:28;drawRect(Math.max(0,x-2),y,Math.min(5,128-Math.max(0,x-2)),2,1);
            if(p===d.boundary){drawRect(Math.max(0,x-2),16,Math.min(5,128-Math.max(0,x-2)),5,0);drawRect(x,16,1,5,1);}
        } else for(let y=11;y<29;y+=4)drawRect(x,y,1,1,1);
    }
    const lead=xFor(Math.max(0,d.points[d.selected]-d.preroll/d.duration));
    const nominal=xFor(d.points[d.selected]);
    if(lead>=0&&lead<128&&d.preroll>0){drawRect(lead,25,1,5,1);for(let x=lead;x<=Math.min(127,nominal);x+=2)drawRect(x,30,1,1,1);}
    const cursor=sliceUi.zoom===1?xFor(d.cursor):64;
    if(cursor>=0&&cursor<128){drawRect(Math.max(0,cursor-1),11,3,18,0);for(let y=11;y<29;y+=2)drawRect(cursor,y,1,1,1);}
    const cells=[['SLIC',index],['STRT',sliceEditorTime(d.points[d.selected])],['END',sliceEditorTime(d.points[d.selected+1])],['ZOOM',Math.round(sliceUi.zoom)+'x']];
    for(let i=0;i<4;++i){print(i*32+1,32,cells[i][0],1);print(i*32+1,40,cells[i][1].slice(0,5),1);}
    print(1,49,'PRER',1);print(1,57,d.preroll.toFixed(1)+'m',1);
    for(let i=1;i<4;++i){drawRect(i*32+14,49,4,4,1);drawRect(i*32+15,50,2,2,0);}
    print(33,57,('B '+(d.points[d.boundary]*d.duration).toFixed(1)+'ms').slice(0,14),1);
}

// MIDI and E16 integration is appended below; functions are initialized before init runs.
globalThis.init = function() {
    sequenceUi.active=false;sequenceUi.data=null;sequenceUi.pad=-1;sequenceUi.held=false;sequenceUi.copy=false;sequenceUi.copyReady=false;
    splitDisplay.valid=false;state.waveText=null;state.patternFaderTouched=false;
    // Schwung may clear or restore the physical surface between takeover sessions.
    if (typeof moveInputFilter.invalidateLedCache === 'function') {
        moveInputFilter.invalidateLedCache();
    }
    state.ticks = 0;
    resetCustomMidiUi();
    resetMidiSurface();
    setParam('slice_editor_open',0);sliceUi.data=null;sliceUi.wave=null;
    state.view = 'main';
    state.settingsPage = 0;
    state.pathReady=false;state.pathPoint=0;
    state.helpPage = 0;
    projectUi.data=null;projectUi.files=[];projectUi.pending=null;projectUi.mode='menu';
    layersUi.active=false;layersUi.data=null;layersUi.wave=false;layersUi.returnView=null;layersUi.serial=-1;layersUi.dismissed=-1;layersUi.editCorner=-1;layersUi.holdPad=-1;layersUi.tapPad=-1;layersUi.focus=0;layersUi.pages=['','FX CORNER'];
    state.bankMode=false;state.bank=null;state.bankWave=[];state.splitEnabled = false; state.sharedDual=false;state.quadMode=false;state.modeSelection=0;state.lastNonSingleMode=clamp(Math.round(getNumber('split_return_mode',1)),1,6);state.splitFocus = 0; state.splitPages = [0,0,0,0];
    state.focusButtonHeld = false; state.focusButtonUsed = false;
    state.performanceVisible=false;state.performanceArmed=false;state.performancePads.clear();state.performancePadLive.clear();state.performanceValues.fill(0);
    state.iconPressed.clear();
    state.fxMenuSelection=0;state.fxMenuReturn=false;
    state.pageSelection=0;state.pageReturn=null;state.pageDestinationReturn=null;state.pageTier='hub';state.pageCategory=0;
    state.message = 'REC CAPTURES PERFORMANCE';
    state.messageTicks = 80;
    state.actionMessage = '';
    state.actionMessageTicks = 0;
    state.copyShortcut=null;
    state.shiftHeld = false;
    state.deleteHeld = false;
    state.glitchLockLatched = false;
    state.glitchLockMomentary = false;
    state.glitchLockChord = false;
    state.glitchLockLastTapTick = -1000;
    state.activePads.clear();
    state.ledPending.clear();
    state.ledSent.clear();
    state.ledOrder = 0;
    resetParameterEncoderMotion();
    state.fxRouteClip = -1; state.fxRoutePending = false;
    state.fxRouteSelected = 0; state.fxRouteGrabbed = false;
    setParam('shift_held', 0);
    setParam('delete_held', 0);
    refreshEditorSnapshot();
    parseSaveStatus(getParam('save_status',''));
    parseMixStatus(getParam('mix_status',''));
    PARAMS.preview_route.value=clamp(getNumber('preview_route',PARAMS.preview_route.value),0,1);
    parseUiState(getParam('ui_state', ''));
    parsePlayheads(getParam('playheads', ''));
    parseWavesetState(getParam('waveset_state', ''));
    parseCutupsState(getParam('cutups_state', ''));
    state.spectralPending=Number(String(getParam('spectral_analysis_state','0,0')).split(',')[1])||0;
    state.wavesetAnalysisPending = Number(String(getParam(
        'waveset_analysis_state', '0,0,0')).split(',')[1]) || 0;
    parseStackPositions(getParam('voice_lanes', ''));
    parseVoicePads(getParam('voice_pads', ''));
    parseVoiceNotes(getParam('voice_notes', ''));
    parseRollPads(getParam('roll_pads', ''));
    parseRollNotes(getParam('roll_notes', ''));
    parseClipBankState(getParam('clip_bank_state', '0,-1,0,0'));
    parseClipState(getParam('clip_state', ''));
    parseClipTakeState(getParam('clip_take_state', '0,0,0'));
    parseClipCapacityState(getParam('clip_capacity_state', '0,1024,0,8192,0,0'));
    parseSampleLoadState(getParam('sample_load_state', '0,1,0,'));
    parseRegionState(getParam('region_state', '0'));
    parseCueState(getParam('cue_state', '-1,-1,-1,-1,-1,-1,-1,-1'));
    pollDiagnostics();
    parseClipPrepareState(getParam('clip_prepare_state', '0,-1'));
    parseChainState(getParam('chain_state', '0,0,0,0,-1'));
    parseChainEntries(getParam('chain_entries', ''));
    state.laneSource = String(getParam('lane_source', 'EMPTY') || 'EMPTY');
    state.laneSourceState = String(getParam('lane_source_state', 'EMPTY:EMPTY')
        || 'EMPTY:EMPTY');
    state.undoAvailable = getNumber('undo_available', 0) !== 0;
    state.stackScanPaused = getNumber('stack_scan_paused', 0) !== 0;
    state.recordedStackHold = getNumber('clip_stack_hold', 0) !== 0;
    state.glitchBufferLocked = getNumber('glitch_buffer_lock', 0) !== 0;
    state.glitchLockLatched = state.glitchBufferLocked;
    state.padMode = clamp(Math.round(getNumber('pad_mode', 0)), 0, 3);
    state.scale = clamp(Math.round(getNumber('scale', 0)),
        0, SCALE_LABELS.length - 1);
    parseWaveforms(getParam('waveforms', ''));
    readBankState();
    readLayers();
    if (state.recalled) {
        state.message = 'RECALLED ' + state.mask.toString(2).replace(/0/g, '').length
            + ' STEREO LANES';
    } else if (state.persistenceEnabled) {
        state.message = 'EXIT SAVES BANK';
    }
    state.message = 'THRU ' + UI_VERSION;
    syncFxOrder();
    rebuildLedQueue();
    // Scale and recalled voice ownership are meaningful as soon as takeover
    // opens. Send the grid immediately rather than placing it behind the
    // throttled button/step LED queue.
    primePadLeds();
    primeIconLeds();
    draw();
};

globalThis.tick = function() {
    state.ticks++;
    tickLayersCornerGesture();tickSequenceGesture();if(sequenceUi.active)readSequence();
    if(!sequenceUi.active&&state.focusButtonHeld && state.performanceArmed && !state.shiftHeld && !state.focusButtonUsed
        && state.ticks-state.captureStartTick>=10) {
        state.focusButtonUsed=true;state.performanceVisible=true;rebuildLedQueue();
    }
    if (state.ticks % 3 === 0) {pollDsp();pollMidiControls();if(state.view==='projects'||projectUi.pending)readProject();}
    tickMidiFeedback();
    if (state.ticks % 180 === 0) setParam('maintenance', 1);
    if(state.savedTicks>0)state.savedTicks--;
    if (state.messageTicks > 0) state.messageTicks--;
    if (state.actionMessageTicks > 0) state.actionMessageTicks--;
    rebuildIconLedQueue();
    drainLedQueue();
    draw();
};

globalThis.onMidiMessageInternal = function(data) {
    if (!data || data.length < 3) return;
    const status = data[0] & 0xF0;
    const number = data[1] | 0;
    const value = data[2] | 0;

    if (status === 0x90 || status === 0x80) {
        const pressed = status === 0x90 && value > 0;
        if(number===MoveCrossfaderTouch) {
            const firstTouch=pressed&&!state.patternFaderTouched;
            state.patternFaderTouched=pressed;
            if(firstTouch&&state.view==='settings'&&currentSettingsPage().patternFade) {
                engagePatternFader();draw();
            }
            return;
        }
        const step = MoveSteps.indexOf(number);
        if (step >= 0) {
            if (pressed) {
                if(state.copyShortcut)copyShortcutStep(step);
                else if (chainEditorActive()) selectChainSlot(step);
                else selectClipSlot(step);
            }
            return;
        }
        const pad = MovePads.indexOf(number);
        if(sequenceUi.active&&pad>=0){sequencePadGesture(pad,pressed);return;}
        if (pad >= 0) {
            if(layersUi.active&&MovePads.indexOf(number)%8>=4&&!state.focusButtonHeld&&!state.performancePads.has(number)){layersCornerGesture(pad,pressed);if(pressed)state.activePads.add(number);else state.activePads.delete(number);readLayers();rebuildLedQueue();return;}
            if(state.bankMode&&pad%8>=4) {
                if(pressed)state.activePads.add(number);else state.activePads.delete(number);
                readBankState();rebuildLedQueue();return;
            }

            if(state.performancePads.has(number)) {
                if(!pressed) {
                    state.performancePads.delete(number);state.performancePadLive.delete(number);state.performanceValues[pad]=0;
                    rebuildLedQueue();
                }
                return;
            }
            if(pressed && pad<8 && state.focusButtonHeld && state.performanceArmed && !state.shiftHeld && !state.activePads.has(number)) {
                state.focusButtonUsed=true;state.performanceVisible=true;
                state.performancePads.add(number);state.performancePadLive.add(number);state.performanceValues[pad]=0.75;
                rebuildLedQueue();draw();return;
            }
            if(state.view==='sliceEdit'&&sliceEditorPadOwner(pad)){
                if(pressed)state.activePads.add(number);else state.activePads.delete(number);
                readSliceEditor();rebuildLedQueue();draw();return;
            }
            const owner=padSettings(pad),mode=owner.mode;
            if (pressed) {
                state.activePads.add(number);
                if (mode === 2) {
                    if(cueForPad(pad)<0){
                        showMessage(isQuad()?'CUES: FIRST 2 IN ZONE':state.splitEnabled?'CUES USE TOP 4':'CUES USE PADS 1-8');
                        rebuildLedQueue();
                        return;
                    }
                    const cue = cueForPad(pad);
                    if (state.deleteHeld) {
                        setParamBlocking('cue_clear', cue);
                        showMessage('CLEARED CUE ' + (cue + 1));
                    } else if (state.shiftHeld) {
                        setParamBlocking('cue_store', cue + ','
                            + (state.splitEnabled?(owner.position||0):state.position).toFixed(4));
                        showMessage('STORED CUE ' + (cue + 1));
                    } else {
                        setParamBlocking('cue_trigger', cue);
                        showMessage('CUE ' + (cue + 1));
                    }
                    rebuildLedQueue();
                } else {
                    sendLedNow('note', number, Cyan);
                    if (mode === 3) {
                        showMessage('ROLL '
                            + ROLL_DIVISIONS[padLocal(pad) % ROLL_DIVISIONS.length]
                            + ' ALL' + (state.shiftHeld ? ' LATCH' : ''));
                    } else if (state.shiftHeld) {
                        showMessage('PAD HOLD TOGGLE');
                    }
                }
            } else {
                state.activePads.delete(number);
                rebuildLedQueue();
            }
            return;
        }
    }

    if (status === 0xA0 && layersUi.active && MovePads.indexOf(number)>=0 && MovePads.indexOf(number)%8>=4){readLayers();rebuildLedQueue();return;}
    if (status === 0xA0 && MovePads.indexOf(number) >= 0) {
        if(state.performancePads.has(number)) {
            if(state.performancePadLive.has(number))state.performanceValues[MovePads.indexOf(number)]=performancePressureAmount(MovePads.indexOf(number),value);
            return;
        }
        // Raw pad pressure also reaches the DSP, where it is associated with
        // the exact polyphonic voice. Do not collapse it to a global value.
        return;
    }

    if (status !== 0xB0) return;
    if(ICON_BUTTONS.includes(number)) {
        if(value>0)state.iconPressed.add(number);else state.iconPressed.delete(number);
        rebuildIconLedQueue();
    }
    if(handleProjects(number,value))return;
    if(handleBankRecovery(number,value))return;
    if(handleCustomMidiCcUi(number,value))return;
    if(handleSliceEditorCc(number,value))return;
    if(number===MoveUndo&&value>0){triggerSafetyUndo();readSliceEditor();return;}
    if(handleLayersPicker(number,value))return;
    if(number===MoveMainButton&&value>0&&sequenceUi.active&&(state.view==='main'||(state.view==='settings'&&currentSettingsPage().sequence))){
        if(state.view==='settings'&&currentSettingsPage().sequenceTrig)setParamBlocking('seq_hold',sequenceUi.data?.step||0);
        else{state.view='settings';state.settingsPage=parameterPages().indexOf(SEQUENCE_LANE_PAGE);resetParameterEncoderMotion();}
        return;
    }
    if(number===MoveCopy&&sequenceUi.active){sequenceUi.copy=value>0;showMessage(value>0?'COPY + PAD':'SEQUENCE');return;}
    if(number===MoveCopy){
        if(value<=0){
            const gesture=state.copyShortcut;state.copyShortcut=null;
            if(gesture&&!gesture.used&&copyShortcutScope()===gesture.scope)runCopyShortcut(gesture.scope,gesture.paste);
            return;
        }
        if(state.copyShortcut)return;
        const scope=copyShortcutScope();
        if(scope){
            state.copyShortcut={scope,paste:state.shiftHeld,used:state.recording||state.clipRecording};
            showMessage(state.copyShortcut.used?'STOP RECORDING FIRST':state.shiftHeld?'PASTE: STEP / RELEASE':'COPY: STEP / RELEASE');return;
        }
        if(state.shiftHeld&&state.splitEnabled){setParamBlocking('split_copy',1);showMessage('COPIED TO OTHER PART');}
        else toggleSplitMode();return;
    }
    if(number===MoveCapture){
        if(value>0) {
            if(state.shiftHeld) {state.focusButtonUsed=true;setParam('performance_capture',0);return;}
            state.focusButtonHeld=true;state.focusButtonUsed=false;
            state.performanceArmed=true;
            state.captureStartTick=state.ticks;state.performanceOwner=state.splitEnabled?state.splitFocus:0;
            setParam('performance_capture',1);
            rebuildLedQueue();
        } else {
            const tap=state.focusButtonHeld&&(state.splitEnabled||layersUi.active)&&!state.focusButtonUsed&&!state.shiftHeld;
            state.focusButtonHeld=false;endPerformanceLayer();
            if(tap){if(layersUi.active)toggleLayersFocus();else setSplitFocus((state.splitFocus+1)%partCount());}
        }
        return;
    }
    if(number===MoveMute&&value>0&&state.splitEnabled){
        parseSplitState(getParam('split_state','{}'));
        const muted=!(state.splitParts[state.splitFocus]?.mute);
        setParamBlocking('part_mute',muted?1:0);parseSplitState(getParam('split_state','{}'));showMessage(muted?'PART MUTED':'PART UNMUTED');return;
    }
    if(number===MoveKnob1+7&&state.splitEnabled&&state.focusButtonHeld){state.focusButtonUsed=true;adjustSplitBlend(decodeDelta(value));return;}
    if(state.focusButtonHeld && number>=MoveKnob1 && number<MoveKnob1+8){if(layersUi.active)state.focusButtonUsed=true;return;}
    if (number === MoveShift) {
        if(value>0 && state.focusButtonHeld) {state.focusButtonUsed=true;endPerformanceLayer();}
        state.shiftHeld = value > 0;
        if(state.view==='settings'&&currentSettingsPage().patternFade)draw();
        setParam('shift_held', state.shiftHeld ? 1 : 0);
        if (!state.shiftHeld && state.glitchLockMomentary
            && !state.glitchLockLatched) {
            state.glitchLockMomentary = false;
            state.glitchLockLastTapTick = state.ticks;
            state.glitchBufferLocked = false;
            setParamBlocking('glitch_buffer_lock', 0);
            showMessage('GLITCH BUFFER LIVE');
            rebuildLedQueue();
        }
        return;
    }
    if(number===MoveDelete && state.view==='help' && state.shiftHeld) {
        state.deleteHeld=false;
        setParam('delete_held',0);
        if(value>0){setParamBlocking('performance_reset',1);pollDiagnostics();draw();}
        return;
    }
    if (number === MoveDelete) {
        state.deleteHeld = value > 0;
        setParam('delete_held',state.deleteHeld?1:0);
        if (state.view === 'clips' || state.view === 'chain') return;
    }
    if (number === MovePlay && value > 0) {
        if(state.deleteHeld){triggerPanic();return;}
        if (state.shiftHeld) toggleClipView();
        else toggleClipPlaying();
        return;
    }
    if (number === MoveLoop) {
        if(!loopAvailable()||(isQuad()&&state.shiftHeld&&state.algorithm!==6))return;
        if (value > 0 && state.shiftHeld) {
            state.glitchLockChord = true;
            if (state.glitchLockLatched) {
                state.glitchLockLatched = false;
                state.glitchLockMomentary = false;
                state.glitchLockLastTapTick = -1000;
                state.glitchBufferLocked = false;
                setParamBlocking('glitch_buffer_lock', 0);
                showMessage('GLITCH BUFFER LIVE');
            } else if (state.algorithm !== 6) {
                showMessage('BUFFER LOCK: GLITCH');
            } else if (state.ticks - state.glitchLockLastTapTick
                <= GLITCH_LOCK_TAP_TICKS) {
                state.glitchLockLatched = true;
                state.glitchLockMomentary = false;
                state.glitchLockLastTapTick = -1000;
                state.glitchBufferLocked = true;
                setParamBlocking('glitch_buffer_lock', 1);
                showMessage('GLITCH LOCK LATCHED');
            } else {
                state.glitchLockMomentary = true;
                state.glitchBufferLocked = true;
                setParamBlocking('glitch_buffer_lock', 1);
                showMessage('GLITCH BUFFER HELD');
            }
            rebuildLedQueue();
            return;
        }
        if (value <= 0 && state.glitchLockChord) {
            state.glitchLockChord = false;
            if (state.glitchLockMomentary && !state.glitchLockLatched) {
                state.glitchLockMomentary = false;
                state.glitchLockLastTapTick = state.ticks;
                state.glitchBufferLocked = false;
                setParamBlocking('glitch_buffer_lock', 0);
                showMessage('GLITCH BUFFER LIVE');
                rebuildLedQueue();
            }
            return;
        }
        if(sequenceUi.active)return;
        if (value > 0) {
            state.stackScanPaused = !state.stackScanPaused;
            setParamBlocking('stack_scan_paused', state.stackScanPaused ? 1 : 0);
            showMessage(isQuad()?(state.stackScanPaused?'TAPE TRANSPORT HELD':'TAPE TRANSPORT RUN'):(state.stackScanPaused?'STACK SCAN HELD':'STACK SCAN RUNNING'));
            rebuildLedQueue();
        }
        return;
    }
    if (value <= 0 && number !== MoveMainKnob
        && (number < MoveKnob1 || number > MoveKnob1 + 7)) return;

    const track = MoveTracks.indexOf(number);
    if (track >= 0 && value > 0) {
        if(state.focusButtonHeld) {
            state.focusButtonUsed=true;
            openPageCategory(track);
        } else if(state.view==='pages') {
            enterPageCategory(track);
        } else if (state.view === 'clips'||state.copyShortcut?.scope==='clip') {
            state.clipBank = track;
            if(state.copyShortcut)state.copyShortcut.used=true;
            rebuildLedQueue();
        } else if (state.shiftHeld) {
            openLibrary(track + 1);
        } else {
            selectLane(track + 1);
        }
        return;
    }
    if ((number === MoveRecord || number === MoveRecordAlt) && value > 0) {
        if (state.deleteHeld) {
            if (state.clipTakeCanUndo && !state.clipRecording) {
                const removed = state.clipTakeCount;
                setParamBlocking('clip_take_undo', 1);
                parseClipTakeState(getParam('clip_take_state', '0,0,0'));
                showMessage('UNDID TAKE ' + removed);
            } else {
                showMessage(state.clipRecording
                    ? 'STOP DATA RECORD FIRST' : 'BASE TAKE PROTECTED');
            }
        } else if (state.shiftHeld) toggleRecording();
        else togglePerformanceRecording();
        return;
    }
    if (number === MoveDelete && value > 0 && state.shiftHeld && !state.recording) {
        setParamBlocking('clear_lane', 'Clear');
        showMessage('CLEARED LANE ' + state.lane);
        return;
    }
    if((number===MoveLeft||number===MoveRight)&&value>0&&chainEditorActive()){
        state.chainPage=clamp(state.chainPage+(number===MoveRight?1:-1),0,3);
        state.chainSlot=state.chainPage*16+state.chainSlot%16;setParamBlocking('chain_slot',state.chainSlot+1);
        rebuildLedQueue();return;
    }
    if ((number === MoveLeft || number === MoveRight) && value > 0
        && state.view === 'help') {
        state.helpPage = clamp(state.helpPage + (number === MoveRight ? 1 : -1), 0, HELP_PAGE_COUNT-1);
        return;
    }
    if ((number === MoveLeft || number === MoveRight) && value > 0
        && (state.view === 'main' || state.view === 'settings')) {
        adjustClipBank(number === MoveRight ? 1 : -1);
        return;
    }
    if (number === MoveUp && value > 0) {
        if(state.view==='mode')state.modeSelection=clamp(state.modeSelection-1,0,6);
        else if(state.view==='pages')movePagePicker(-1);
        else if (state.view === 'fxMenu') moveSplitFxMenu(-1);
        else if (state.view === 'help') state.helpPage = Math.max(0, state.helpPage - 1);
        else if (state.view === 'chain') adjustChainRepeats(1);
        else if (state.shiftHeld
            && (state.view === 'main' || state.view === 'settings')) {
            adjustClipBank(1);
        }
        else if (state.view === 'settings') adjustSettingsPage(1);
        else if (state.view === 'library') moveLibrarySelection(-1);
        else if (state.view === 'main') adjustOctave(1);
        return;
    }
    if (number === MoveDown && value > 0) {
        if(state.view==='mode')state.modeSelection=clamp(state.modeSelection+1,0,6);
        else if(state.view==='pages')movePagePicker(1);
        else if (state.view === 'fxMenu') moveSplitFxMenu(1);
        else if (state.view === 'help') state.helpPage = Math.min(HELP_PAGE_COUNT-1, state.helpPage + 1);
        else if (state.view === 'chain') adjustChainRepeats(-1);
        else if (state.shiftHeld
            && (state.view === 'main' || state.view === 'settings')) {
            adjustClipBank(-1);
        }
        else if (state.view === 'settings') adjustSettingsPage(-1);
        else if (state.view === 'library') moveLibrarySelection(1);
        else if (state.view === 'main') adjustOctave(-1);
        return;
    }
    if (number === MoveMainKnob) {
        const delta = decodeDelta(value);
        if(state.view==='mode')state.modeSelection=clamp(state.modeSelection+delta,0,6);
        else if(state.view==='pages')movePagePicker(delta);
        else if (state.view === 'fxMenu') moveSplitFxMenu(delta);
        else if (state.view === 'help' && delta !== 0) {
            state.helpPage = clamp(state.helpPage + (delta > 0 ? 1 : -1), 0, HELP_PAGE_COUNT-1);
        }
        else if(state.view==='settings'&&(currentSettingsPage().voiceEngines||currentSettingsPage().voiceSetup))editVoiceEngineSlot(delta);
        else if (state.view === 'settings' && currentSettingsPage().route) turnFxOrder(delta);
        else if (state.view === 'library') moveLibrarySelection(delta);
        else if (state.view === 'chain') adjustChainClip(delta);
        else if (delta !== 0) selectLane(state.lane + (delta > 0 ? 1 : -1));
        return;
    }
    if (number === MoveMainButton && value > 0) {
        if(state.bankMode&&state.bank?.failed&&state.view==='main'){openBankRecovery();return;}
        if(state.view==='mode')applyModePicker();
        else if(state.view==='pages')activatePagePicker();
        else if(layersUi.active&&state.view==='settings'&&!state.shiftHeld&&!state.focusButtonHeld&&currentSettingsPage().title==='FX CORNER')openLayersPicker();
        else if(state.view==='settings'&&currentSettingsPage().title==='SLICE SOURCE')openSliceEditor();
        else if (state.view === 'fxMenu') activateSplitFxPage();
        else if(state.view==='settings'&&(currentSettingsPage().voiceEngines||currentSettingsPage().voiceSetup))openVoiceEngineEditor();
        else if(state.view==='settings'&&currentSettingsPage().pulsar){setParamBlocking('pulsar_source_hold',PARAMS.pulsar_source_hold.value>=.5?0:1);PARAMS.pulsar_source_hold.value=getNumber('pulsar_source_hold',PARAMS.pulsar_source_hold.value>=.5?0:1);showMessage(PARAMS.pulsar_source_hold.value>=.5?'SOURCE HELD':'SOURCE RUNNING');draw();}
        else if(state.view==='settings'&&currentSettingsPage().stochastic){setParamBlocking('stochastic_source_hold',PARAMS.stochastic_source_hold.value>=.5?0:1);PARAMS.stochastic_source_hold.value=getNumber('stochastic_source_hold',PARAMS.stochastic_source_hold.value>=.5?0:1);showMessage(PARAMS.stochastic_source_hold.value>=.5?'SOURCE HELD':'SOURCE RUNNING');draw();}
        else if(state.view==='settings'&&currentSettingsPage().patternFade){readPatternFade();setParamBlocking('pattern_fade_hold',state.patternFade.held?0:1);readPatternFade();draw();}
        else if(state.view==='settings'&&currentSettingsPage().title===STACK_PATH_PAGE.title&&Math.round(PARAMS.lane_path.value)===8)openPathEditor();
        else if (state.splitEnabled && state.view==='settings' && isSendPage(currentSettingsPage()) && !state.shiftHeld) openSplitFxMenu();
        else if (state.view === 'settings' && currentSettingsPage().route) clickFxOrder();
        else if (state.view === 'library') activateLibraryItem();
        else if (state.view === 'chain') {
            if (state.shiftHeld) {
                insertActiveClipInChain();
            } else {
                state.chainEnabled = state.chainBank!==state.chainPlayBank || !state.chainEnabled;
                PARAMS.chain_enabled.value = state.chainEnabled ? 1 : 0;
                setParamBlocking('chain_enabled', state.chainEnabled ? 1 : 0);
                showMessage(state.chainEnabled ? 'CHAIN ON' : 'CHAIN OFF');
                rebuildLedQueue();
            }
        }
        else if (state.shiftHeld) toggleRecording();
        else togglePerformanceRecording();
        return;
    }
    if (number >= MoveKnob1 && number < MoveKnob1 + 8) {
        if (state.view === 'main' || state.view === 'settings' || state.view==='library' || state.view==='chain') {
            adjustControl(number - MoveKnob1, decodeDelta(value));
        }
        return;
    }
    if (number === MoveMenu && value > 0) {
        if(layersUi.returnView){layersUi.returnView=null;layersUi.editCorner=-1;layersUi.holdFired=true;}
        if(state.focusButtonHeld){state.focusButtonUsed=true;openPagePicker();return;}
        if(state.view==='mode'){
            if(!returnToPagePicker()){state.view='pages';state.pageTier='hub';state.pageSelection=4;rebuildLedQueue();}
            return;
        }
        if(state.view==='pages'){pagePickerBack();return;}
        if(state.view==='mixStatus'){state.view='main';state.pageReturn=null;state.pageDestinationReturn=null;return;}
        if(!state.shiftHeld&&state.view==='chain'){state.chainEditorReturn=true;state.view='settings';state.settingsPage=parameterPages().indexOf(CHAIN_PAGE);resetParameterEncoderMotion();return;}
        if(!state.shiftHeld&&state.view==='settings'&&state.chainEditorReturn){state.chainEditorReturn=false;state.view='chain';return;}
        if (state.view === 'settings' && currentSettingsPage().route) cancelFxOrder(false);
        if (state.shiftHeld) {
            state.pageReturn=null;state.pageDestinationReturn=null;
            if (state.view === 'help') {
                state.view = 'main';
            } else {
                pollDiagnostics();
                state.helpPage = 0;
                state.view = 'help';
            }
        } else if (state.view === 'help' || state.view === 'fxMenu') {
            state.view = 'main';state.fxMenuReturn=false;state.pageReturn=null;state.pageDestinationReturn=null;
        } else {
            state.pageReturn=null;state.pageDestinationReturn=null;
            state.fxMenuReturn=false;
            state.view = state.view === 'settings' ? 'main' : 'settings';
        }
        return;
    }
    if (number === MoveBack && value > 0) {
        if(layersUi.active && state.shiftHeld){layersUi.returnView=null;layersUi.editCorner=-1;layersUi.holdFired=true;layersUi.dismissed=Number(layersUi.data?.held??-1);layersUi.wave=!layersUi.wave;state.view='main';resetParameterEncoderMotion();return;}
        if(layersUi.returnView){closeLayersEditor();return;}
        if(state.view==='main'&&state.bankMode){
            readBankState();
            if(openBankRecovery())return;
            if(state.bank?.pending>=0){showMessage('WAIT FOR BANK SAVE');return;}
        }
        if(state.view==='settings'&&state.engineListReturn&&voiceEngineListEnabled()){
            state.engineListReturn=false;state.settingsPage=parameterPages().findIndex(p=>p.title===(state.engineListReturnPage||VOICE_ENGINES_PAGE.title));resetParameterEncoderMotion();return;
        }
        if(state.view==='settings'&&state.chainEditorReturn){state.chainEditorReturn=false;state.view='chain';return;}
        if(state.view==='settings'&&currentSettingsPage().pathEditor){state.settingsPage=parameterPages().findIndex(p=>p.title===STACK_PATH_PAGE.title);resetParameterEncoderMotion();return;}
        if(state.view==='mode'){
            if(!returnToPagePicker()){state.view='pages';state.pageTier='hub';state.pageSelection=4;rebuildLedQueue();}
            return;
        }
        if(state.view==='pages'){pagePickerBack();return;}
        if (state.view === 'settings' && currentSettingsPage().route
            && (state.fxRouteGrabbed || state.fxRoutePending)) {
            cancelFxOrder();
            return;
        }
        if (state.view==='fxMenu') {
            openSplitSends();
        } else if (state.view==='settings' && state.fxMenuReturn && state.splitEnabled) {
            openSplitFxMenu();
        } else if (state.view!=='library' && returnToPagePicker()) {
            // A page opened from the hierarchy returns to its owning list.
        } else if (state.view === 'library') {
            libraryBack();
        } else if (state.view !== 'main') {
            state.view = 'main';
            rebuildLedQueue();
        } else if (typeof host_exit_module === 'function') {
            host_exit_module();
        }
    }
};

globalThis.onUnload = function() {
    setParamBlocking('midi_custom_cancel',1);
    setParamBlocking('slice_editor_open',0);
    remoteE16.stop();
    endPerformanceLayer();state.performancePads.clear();
    if (state.recording) setParamBlocking('record_gate', 0);
    if (state.clipRecording) setParamBlocking('clip_record', 1);
    setParamBlocking('all_notes_off', 1);
    setParam('shift_held', 0);
    setParam('delete_held', 0);
    setParamBlocking('glitch_buffer_lock', 0);
    setButtonLED(MoveRecord, Black);
    if (MoveRecordAlt !== MoveRecord) setButtonLED(MoveRecordAlt, Black);
    setButtonLED(MovePlay, Black);
    state.iconPressed.clear();
    for(const id of ICON_BUTTONS)setButtonLED(id,Black,true);
};



// BEGIN GENERATED MIDI ADAPTER
// On-device Custom CC profiles. Draft edits are committed explicitly; entering
// Learn or loading a profile never writes an instrument parameter.
const customMidiUi={data:null,entries:[],entriesKey:'',selection:0,tab:0,returnPage:0,
    copyDestination:1,targetPart:0,pickerTier:0,pickerGroup:'Core',pickerSelection:0,
    confirm:null,notice:'',noticeUntil:0};
const CUSTOM_COMMANDS=['Play clip','Stop clip','Record performance','Undo clip/source','Panic','Previous bank','Next bank',
    'Bank 1','Bank 2','Bank 3','Bank 4','Bank 5','Bank 6','Bank 7','Bank 8','Undo overdub take',
    'Previous clip','Next clip','Hold pattern fade','Ignore CC'];
const CUSTOM_PROFILES=['NONE','CUSTOM1','CUSTOM2','CUSTOM3','CUSTOM4','NANO CC'];
const CUSTOM_PARTS=['SHR','A','B','C','D','FOC'];
const customMidiViews=['midiMapList','midiMapEdit','midiMapPicker','midiMapConfirm'];
function customMidiActive() {return customMidiUi.data?.profile||0;}
function customMidiPage() {return state.view==='settings'&&currentSettingsPage().customMaps;}
function customMidiContext() {return customMidiPage()||customMidiViews.includes(state.view);}
function customMidiEditable() {return customMidiActive()>=1&&customMidiActive()<=4;}
function customMidiNotice(text) {customMidiUi.notice=text;customMidiUi.noticeUntil=state.ticks+45;}
function readCustomMidi() {
    let data;try{data=JSON.parse(String(getParam('midi_custom_state','{}')));}catch(_){return;}
    if(data.v!==1||!Number.isInteger(data.profile)||data.profile<0||data.profile>5
        ||!Array.isArray(data.draft)||data.draft.length!==13||!data.draft.every(Number.isFinite))return;
    const previous=customMidiUi.data;customMidiUi.data=data;
    if(customMidiContext()) {
        const key=data.profile+':'+data.revision;
        if(key!==customMidiUi.entriesKey){
            let entries;try{entries=JSON.parse(String(getParam('midi_custom_entries','[]')));}catch(_){return;}
            if(Array.isArray(entries)&&entries.length<=128&&entries.every(e=>Array.isArray(e)&&e.length===9&&e.every(Number.isFinite))){
                customMidiUi.entries=entries;customMidiUi.entriesKey=key;
                customMidiUi.selection=clamp(customMidiUi.selection,0,entries.length);
            }
        }
        if(previous?.profile!==data.profile&&customMidiViews.includes(state.view)){
            state.view='midiMapList';customMidiUi.selection=0;customMidiUi.confirm=null;
        }
        if(data.result===1&&previous?.ui!==data.ui)customMidiNotice('LEARNED CH'+data.draft[0]+' CC'+data.draft[1]);
        if(data.result===7&&previous?.ui!==data.ui)customMidiNotice('LEARN TIMED OUT');
    }
}
function customMidiSet(key,value) {setParamBlocking('midi_custom_'+key,value);readCustomMidi();rebuildIconLedQueue();}
function resetCustomMidiUi() {
    setParam('midi_custom_cancel',1);customMidiUi.data=null;customMidiUi.entries=[];customMidiUi.entriesKey='';
    customMidiUi.selection=0;customMidiUi.confirm=null;customMidiUi.notice='';
}
function customMidiTitle(title,fraction='') {
    const width=fraction.length*6;
    const limit=Math.floor((128-(width?width+6:0)-1)/6);
    print(1,0,String(title).slice(0,limit),1);if(fraction)print(128-width,0,fraction,1);
    drawRect(0,8,128,1,1);
}
function customMidiCell(index,label,value) {
    const x=index%4*32,y=11+Math.floor(index/4)*22;
    if(!label){drawRect(x+14,y+8,4,4,1);drawRect(x+15,y+9,2,2,0);return;}
    print(x+1,y,String(label).slice(0,5),1);print(x+1,y+10,String(value).slice(0,5),1);
}
function customMidiFooter(fallback) {
    return customMidiUi.notice&&state.ticks<customMidiUi.noticeUntil?customMidiUi.notice:fallback;
}
function drawCustomMidiProfile() {
    const d=customMidiUi.data;clearDisplay();drawSettingsTitle('MIDI MAPS',parameterPages());drawRect(0,8,128,1,1);
    if(!d){print(1,22,'READING MIDI MAPS',1);return;}
    const cells=[['PROF',['NONE','C1','C2','C3','C4','NANO'][d.profile]],['BANK',d.bank],['EDIT','OPEN'],['COPY','TURN'],
        ['DEST','C'+customMidiUi.copyDestination],['CLEAR','TURN'],null,null];
    cells.forEach((cell,i)=>customMidiCell(i,...(cell||[])));
    print(1,57,customMidiFooter(d.mode===6?'MIDI CONTROL MODE OFF':d.save===3?'MAP SAVE FAILED':d.save===1?'SAVING MAPS':d.profile===5?'COPY TO CUSTOM TO EDIT':d.profile?'CLICK: EDIT MAPPINGS':'CHOOSE A PROFILE'),1);
}
function openCustomMidiList() {
    readCustomMidi();if(!customMidiActive()){customMidiNotice('CHOOSE A PROFILE');return;}
    customMidiUi.returnPage=state.settingsPage;customMidiUi.selection=0;customMidiUi.entriesKey='';
    state.view='midiMapList';readCustomMidi();resetParameterEncoderMotion();draw();
}
function customMidiTargetLabel(entry,long=false) {
    const [index,ch,cc,part,kind,target,page,slot]=entry;
    void index;void ch;void cc;
    const prefix=part<0?'':CUSTOM_PARTS[part+1]+' ';
    if(kind===1)return prefix+'E'+(page+1)+'.'+(slot+1);
    if(kind===2)return CUSTOM_COMMANDS[target]||'COMMAND';
    const p=thruMidi.MIDI_BY_ID.get(target);
    return prefix+(p?(long?p.name:(SHORT_LABELS[p.key]||p.short_name||p.name).trim()):'UNKNOWN');
}
function customMidiRows(rows,selected) {
    const offset=clamp(selected-1,0,Math.max(0,rows.length-4));
    for(let row=0;row<4&&offset+row<rows.length;++row){
        const i=offset+row,y=11+row*10,active=i===selected;
        if(active)drawRect(0,y-1,128,9,1);
        print(1,y,String(rows[i]).slice(0,21),active?0:1);
    }
}
function drawCustomMidiList() {
    clearDisplay();const d=customMidiUi.data;if(!d)return;
    const rows=[d.profile===5?'FACTORY: READ ONLY':'+ ADD MAPPING',...customMidiUi.entries.map(e=>
        String(e[1]).padStart(2,'0')+'/'+String(e[2]).padStart(3,'0')+' '+customMidiTargetLabel(e).slice(0,10)+(e[8]?' '+e[8]:''))];
    customMidiTitle(d.profile===5?'NANO MAPS':'C'+d.profile+' MAPS',(customMidiUi.selection+1)+'/'+rows.length);
    customMidiRows(rows,customMidiUi.selection);
    print(1,57,customMidiFooter(d.profile===5?'COPY PROFILE TO EDIT':'CLICK EDIT  X REMOVE'),1);
}
function customMidiBegin(index) {
    customMidiSet('begin',index);if(!customMidiUi.data?.editing)return;
    customMidiUi.tab=0;customMidiUi.targetPart=Math.max(0,customMidiUi.data.draft[2]);
    customMidiUi.notice='';state.view='midiMapEdit';resetParameterEncoderMotion();draw();
}
function customMidiDraft(change) {
    const next=customMidiUi.data.draft.slice();change(next);
    if(next[3]===0){const p=thruMidi.MIDI_BY_ID.get(next[4]);if(p){if(p.scope!=='part')next[2]=-1;if(p.action)next[7]=3;}}
    if(next[3]===1&&next[2]<0)next[2]=customMidiUi.targetPart;
    if(next[3]===2){next[2]=-1;next[7]=3;}
    customMidiSet('draft',next.join(','));
}
function customMidiGroups() {return [...new Set(thruMidi.MIDI_PARAMETERS.map(p=>p.group))];}
function customMidiParameters(group) {return thruMidi.MIDI_PARAMETERS.filter(p=>p.group===group);}
function customMidiDraftTarget() {
    const b=customMidiUi.data?.draft;if(!b)return null;
    if(b[3]===0)return thruMidi.MIDI_BY_ID.get(b[4])||null;
    if(b[3]===1){try{const p=JSON.parse(String(getParam('midi_custom_target','{}')));return thruMidi.MIDI_BY_ID.get(p.id)||null;}catch(_){return null;}}
    return null;
}
function drawCustomMidiEdit() {
    clearDisplay();const d=customMidiUi.data;if(!d)return;const b=d.draft;
    const tab=customMidiUi.tab,names=['TARGET','SOURCE','RESPONSE'];
    customMidiTitle((d.index<0?'NEW MAP':'MAP '+(d.index+1))+' '+names[tab],(tab+1)+'/3');
    const p=customMidiDraftTarget();let cells,footer;
    if(tab===0){
        const groups=customMidiGroups(),group=b[3]===0?p?.group:'',params=customMidiParameters(group);
        void groups;void params;
        cells=[['PART',CUSTOM_PARTS[b[2]+1]],['BIND',['PARAM','SLOT','CMD'][b[3]]],
            b[3]===0?['GROUP',group]:null,b[3]===0?['PARAM',p?(SHORT_LABELS[p.key]||p.short_name||p.name).trim():'---']:b[3]===2?['CMD',b[4]+1]:null,
            b[3]===1?['EPAGE',b[5]+1]:null,b[3]===1?['KNOB',b[6]+1]:null,null,null];
        footer=b[3]===2?CUSTOM_COMMANDS[b[4]]:b[3]===1?(p?.name||'RESERVED ENGINE SLOT'):(p?.name||'CLICK CHOOSE TARGET');
    } else if(tab===1){
        cells=[['CH',b[0]],['CC',b[1]],['TYPE',['ABS','HOLD','TOGL','TRIG'][b[7]]],['LEARN',d.learn?'WAIT':'CLICK'],
            ['FILT',d.filter||'ANY'],['BANK',b[12]||'ALL'],null,null];
        footer=d.learn?'MOVE CONTROL TO LEARN':'CLICK: LEARN CONTROL';
    } else {
        cells=b[3]===2?Array(8).fill(null):[['MIN',Math.round(b[10]*100)+'%'],['MAX',Math.round(b[11]*100)+'%'],
            ['REV',b[9]?'ON':'OFF'],['TAKE',b[8]?'PICK':'JUMP'],null,null,null,null];
        footer='CLICK: SAVE MAPPING';
    }
    cells.forEach((cell,i)=>customMidiCell(i,...(cell||[])));
    print(1,57,customMidiFooter(customMidiEditable()?footer:'FACTORY: COPY TO EDIT').slice(0,21),1);
}
function customMidiPickerRows() {
    if(!customMidiUi.pickerTier)return ['Engine page slot','Controller command',...customMidiGroups()];
    return customMidiUi.pickerGroup==='Commands'?CUSTOM_COMMANDS:customMidiParameters(customMidiUi.pickerGroup).map(p=>p.name);
}
function drawCustomMidiPicker() {
    clearDisplay();const rows=customMidiPickerRows();customMidiUi.pickerSelection=clamp(customMidiUi.pickerSelection,0,rows.length-1);
    customMidiTitle(customMidiUi.pickerTier?customMidiUi.pickerGroup:'DESTINATION',(customMidiUi.pickerSelection+1)+'/'+rows.length);
    customMidiRows(rows,customMidiUi.pickerSelection);print(1,57,'CLICK SELECT  BACK UP',1);
}
function customMidiChooseTarget() {
    const i=customMidiUi.pickerSelection;
    if(!customMidiUi.pickerTier){
        if(i===0){customMidiDraft(b=>{b[3]=1;b[4]=-1;b[2]=customMidiUi.targetPart;b[7]=0;});state.view='midiMapEdit';return;}
        customMidiUi.pickerGroup=i===1?'Commands':customMidiGroups()[i-2];customMidiUi.pickerTier=1;customMidiUi.pickerSelection=0;return;
    }
    if(customMidiUi.pickerGroup==='Commands')customMidiDraft(b=>{b[3]=2;b[4]=i;b[7]=3;});
    else {const p=customMidiParameters(customMidiUi.pickerGroup)[i];customMidiDraft(b=>{b[3]=0;b[4]=p.id;b[2]=p.scope==='part'?customMidiUi.targetPart:-1;b[7]=p.action?3:0;});}
    state.view='midiMapEdit';
}
function customMidiConfirm(type,argument,title,detail) {
    customMidiUi.confirm={type,argument,title,detail,returnView:state.view};state.view='midiMapConfirm';draw();
}
function drawCustomMidiConfirm() {
    clearDisplay();const c=customMidiUi.confirm;if(!c)return;
    customMidiTitle(c.title);print(1,19,c.detail.slice(0,21),1);
    print(1,35,'CLICK TO CONFIRM',1);print(1,47,'BACK TO CANCEL',1);
}
function customMidiCommit(replace=false) {
    customMidiSet('apply',replace?2:1);const d=customMidiUi.data;
    if(d?.result===2){customMidiConfirm('replace',0,'CC ALREADY ASSIGNED','Replace map '+(d.conflict+1)+'?');return;}
    if(d?.result===5){state.view='midiMapList';customMidiUi.notice='';customMidiUi.entriesKey='';readCustomMidi();return;}
    customMidiNotice(d?.result===3?'128 MAPPING LIMIT':'INVALID MAPPING');
}
function customMidiAccept() {
    const c=customMidiUi.confirm;if(!c)return;customMidiUi.confirm=null;state.view=c.returnView;
    if(c.type==='replace'){customMidiCommit(true);return;}
    customMidiSet(c.type,c.argument);customMidiUi.entriesKey='';
    if(c.type==='delete'){state.view='midiMapList';customMidiUi.selection=0;}
    else state.view='settings';
    readCustomMidi();
}
function customMidiBack() {
    if(state.view==='midiMapConfirm'){
        state.view=customMidiUi.confirm?.returnView||'midiMapList';customMidiUi.confirm=null;return;
    }
    if(state.view==='midiMapPicker'){
        if(customMidiUi.pickerTier){customMidiUi.pickerTier=0;customMidiUi.pickerSelection=0;}else state.view='midiMapEdit';return;
    }
    if(state.view==='midiMapEdit'){customMidiSet('cancel',1);state.view='midiMapList';return;}
    customMidiSet('cancel',1);state.view='settings';state.settingsPage=customMidiUi.returnPage;
}
function adjustCustomMidiProfile(index,delta) {
    const d=customMidiUi.data;if(!d||!delta)return;
    if(index===2){openCustomMidiList();return;}
    if(index===3){if(d.profile)customMidiConfirm('copy',customMidiUi.copyDestination,'COPY PROFILE','Overwrite CUSTOM'+customMidiUi.copyDestination+'?');return;}
    if(index===5){if(customMidiEditable())customMidiConfirm('clear',1,'CLEAR PROFILE','Remove all mappings?');else customMidiNotice('CUSTOM PROFILES ONLY');return;}
    delta=parameterEncoderDelta(index,'custom-profile-'+index,delta);if(!delta)return;
    if(index===0){customMidiSet('profile',clamp(d.profile+delta,0,5));syncRemoteTransport();}
    if(index===1)customMidiSet('bank',clamp(d.bank+delta,1,8));
    if(index===4)customMidiUi.copyDestination=clamp(customMidiUi.copyDestination+delta,1,4);
}
function adjustCustomMidiEdit(index,delta) {
    const d=customMidiUi.data;if(!d||!delta)return;
    if(!customMidiEditable()){customMidiNotice('COPY TO CUSTOM FIRST');return;}
    if(customMidiUi.tab===1&&index===3){customMidiSet('learn',d.learn?0:1);return;}
    delta=parameterEncoderDelta(index,'custom-edit-'+customMidiUi.tab+'-'+index,delta);if(!delta)return;
    const b=d.draft,tab=customMidiUi.tab;
    if(tab===0){
        if(index===0){const part=clamp((b[2]<0?customMidiUi.targetPart:b[2])+delta,0,4);customMidiUi.targetPart=part;customMidiDraft(n=>{n[2]=part;});}
        else if(index===1){const kind=clamp(b[3]+delta,0,2);customMidiDraft(n=>{n[3]=kind;n[4]=kind===1?-1:0;n[2]=kind===2?-1:customMidiUi.targetPart;n[7]=kind===2?3:0;});}
        else if(index===2&&b[3]===0){
            const groups=customMidiGroups(),p=thruMidi.MIDI_BY_ID.get(b[4]);const group=groups[clamp(groups.indexOf(p?.group)+delta,0,groups.length-1)];
            const next=customMidiParameters(group)[0];customMidiDraft(n=>{n[4]=next.id;n[2]=next.scope==='part'?customMidiUi.targetPart:-1;n[7]=next.action?3:0;});
        } else if(index===3&&b[3]===0){
            const p=thruMidi.MIDI_BY_ID.get(b[4]),params=customMidiParameters(p?.group);const next=params[clamp(params.indexOf(p)+delta,0,params.length-1)];
            if(next)customMidiDraft(n=>{n[4]=next.id;n[2]=next.scope==='part'?customMidiUi.targetPart:-1;n[7]=next.action?3:0;});
        } else if(index===3&&b[3]===2)customMidiDraft(n=>{n[4]=clamp(n[4]+delta,0,CUSTOM_COMMANDS.length-1);});
        else if(index===4&&b[3]===1)customMidiDraft(n=>{n[5]=clamp(n[5]+delta,0,3);});
        else if(index===5&&b[3]===1)customMidiDraft(n=>{n[6]=clamp(n[6]+delta,0,7);});
    }else if(tab===1){
        if(index===0||index===1){customMidiSet('learn',0);customMidiDraft(n=>{n[index]=clamp(n[index]+delta,index===0?1:0,index===0?16:127);});}
        if(index===2)customMidiDraft(n=>{n[7]=clamp(n[7]+delta,0,3);});
        if(index===4)customMidiSet('learn_filter',clamp(d.filter+delta,0,16));
        if(index===5)customMidiDraft(n=>{n[12]=clamp(n[12]+delta,0,8);});
    }else if(b[3]!==2){
        if(index===0)customMidiDraft(n=>{n[10]=clamp(Math.round(n[10]*100+delta)/100,0,n[11]);});
        if(index===1)customMidiDraft(n=>{n[11]=clamp(Math.round(n[11]*100+delta)/100,n[10],1);});
        if(index===2)customMidiDraft(n=>{n[9]=delta>0?1:0;});
        if(index===3)customMidiDraft(n=>{n[8]=delta>0?1:0;});
    }
}
function handleCustomMidiCcUi(number,value) {
    if(!customMidiContext())return false;
    const view=state.view;
    if(number===MovePlay&&value>0&&state.iconPressed.has(MoveDelete)) {
        customMidiSet('cancel',1);customMidiUi.confirm=null;
        if(customMidiViews.includes(state.view))state.view='midiMapList';
        setParamBlocking('panic',1);customMidiNotice('PANIC: NOTES OFF');return true;
    }
    if(number===MoveMenu&&value>0&&customMidiViews.includes(view)){
        customMidiSet('cancel',1);state.view='settings';state.settingsPage=customMidiUi.returnPage;
        return !state.shiftHeld&&!state.focusButtonHeld;
    }
    if(number===MoveBack&&value>0&&customMidiViews.includes(view)){customMidiBack();return true;}
    if(number===MoveUndo&&value>0){
        if(view==='midiMapEdit'||view==='midiMapPicker'){customMidiSet('cancel',1);state.view='midiMapList';}
        else {customMidiSet('undo',1);customMidiUi.entriesKey='';readCustomMidi();}
        return true;
    }
    if(number===MoveDelete&&customMidiViews.includes(view)){
        state.deleteHeld=false;setParam('delete_held',0);
        if(value>0&&customMidiEditable()){
            const row=view==='midiMapList'?customMidiUi.entries[customMidiUi.selection-1]:null;
            const index=row?row[0]:view==='midiMapEdit'?customMidiUi.data.index:-1;
            if(index>=0)customMidiConfirm('delete',index,'REMOVE MAPPING','Delete map '+(index+1)+'?');
        }
        return true;
    }
    if(number===MoveMainButton&&value>0){
        if(customMidiPage())openCustomMidiList();
        else if(view==='midiMapList'){
            if(customMidiUi.selection===0){if(customMidiEditable())customMidiBegin(-1);else customMidiNotice('COPY TO CUSTOM FIRST');}
            else customMidiBegin(customMidiUi.entries[customMidiUi.selection-1][0]);
        }else if(view==='midiMapPicker')customMidiChooseTarget();
        else if(view==='midiMapConfirm')customMidiAccept();
        else if(view==='midiMapEdit'){
            if(!customMidiEditable()){customMidiNotice('COPY TO CUSTOM FIRST');return true;}
            if(customMidiUi.tab===0){customMidiUi.pickerTier=0;customMidiUi.pickerSelection=0;state.view='midiMapPicker';}
            else if(customMidiUi.tab===1)customMidiSet('learn',customMidiUi.data.learn?0:1);
            else customMidiCommit();
        }
        draw();return true;
    }
    if(number>=MoveKnob1&&number<MoveKnob1+8){
        if(customMidiPage())adjustCustomMidiProfile(number-MoveKnob1,decodeDelta(value));
        else if(view==='midiMapEdit')adjustCustomMidiEdit(number-MoveKnob1,decodeDelta(value));
        draw();return true;
    }
    if(customMidiViews.includes(view)&&(number===MoveMainKnob||number===MoveUp||number===MoveDown||number===MoveLeft||number===MoveRight)){
        if(number!==MoveMainKnob&&value<=0)return true;
        const delta=number===MoveMainKnob?decodeDelta(value):number===MoveUp||number===MoveLeft?-1:1;
        if(view==='midiMapList')customMidiUi.selection=clamp(customMidiUi.selection+delta,0,customMidiUi.entries.length);
        if(view==='midiMapPicker')customMidiUi.pickerSelection=clamp(customMidiUi.pickerSelection+delta,0,customMidiPickerRows().length-1);
        if(view==='midiMapEdit'){
            customMidiSet('learn',0);customMidiUi.tab=(customMidiUi.tab+delta%3+3)%3;resetParameterEncoderMotion();
        }
        draw();return true;
    }
    if(customMidiViews.includes(view)&&state.focusButtonHeld&&MoveTracks.includes(number)&&value>0){
        customMidiSet('cancel',1);state.view='settings';state.settingsPage=customMidiUi.returnPage;
    }
    return false;
}
function drawCustomMidi() {
    if(state.view==='midiMapList')drawCustomMidiList();
    else if(state.view==='midiMapEdit')drawCustomMidiEdit();
    else if(state.view==='midiMapPicker')drawCustomMidiPicker();
    else if(state.view==='midiMapConfirm')drawCustomMidiConfirm();
}

// Generated by controllers/oxi-e16/generate-layout.mjs.
const E16_LABELS={"0":"POS ","1":"FLD ","2":"SPD ","3":"STK ","4":"ENG ","5":"JOIN","6":"LVL ","133":"VARI","384":"SCAN","385":"MODE","386":"PATH","388":"BLND","389":"SHAP","390":"OFST","391":"SKEW","392":"CURV","393":"SLEW","394":"STR ","395":"RATI","512":"MODE","513":"SND ","514":"RATE","515":"BAS ","516":"PATH","517":"STEP","518":"SEED","519":"TRVL","520":"JITR","521":"DUTY","522":"MRAT","523":"MSYM","524":"MSHP","525":"TRIG","526":"OVLP","527":"LAYR","528":"ERAT","529":"REPT","530":"ESTP","531":"PITC","532":"LEVL","533":"CURV","640":"MODE","641":"SIZE","642":"DENS","643":"QUAL","644":"PIT ","645":"WIN ","646":"SPRD","647":"SPRY","648":"BIAS","649":"PSPR","650":"REV%","651":"SVAR","652":"LVAR","653":"TIME","654":"TSC ","655":"ESKW","656":"PROC","657":"PAMT","658":"REGN","659":"ADVN","768":"MODE","769":"LEN ","770":"LAYR","771":"PIT ","772":"SHAP","773":"EDGE","774":"SPRD","775":"REV%","776":"SPRY","777":"BIAS","778":"DVAR","779":"PSPR","780":"LVAR","781":"PHAS","782":"ADVN","896":"ADV ","897":"MODE","898":"CLCK","899":"DIV ","900":"RATE","901":"TVAR","902":"GATE","903":"JOIN","904":"REGN","905":"REPT","906":"REV%","907":"PVAR","908":"LVAR","909":"LORD","910":"POLY","911":"RSRC","1030":"PLAY","1031":"SSRC","1032":"SLIC","1152":"MODE","1153":"CLCK","1154":"DIV ","1155":"RATE","1156":"BUFF","1157":"REPT","1158":"CHNC","1159":"OVRD","1160":"STEP","1161":"PULS","1162":"ROTA","1163":"JUMP","1164":"PITC","1165":"REV%","1166":"DCAY","1167":"SEED","1280":"GRUP","1281":"REPT","1282":"STRD","1283":"ADVN","1284":"ORDR","1285":"PROC","1286":"DPTH","1287":"DTAL","1408":"MODE","1409":"BLUR","1410":"DCAY","1411":"PIT ","1412":"TILT","1413":"SMER","1536":"MODE","1537":"AMP ","1538":"FREQ","1539":"OFST","1540":"BLUR","1541":"PIT ","1664":"MODE","1665":"RATE","1666":"LEN ","1667":"SCAT","1668":"RNGE","1669":"GRUP","1670":"SPRD","1671":"SEED","1792":"PIT ","1793":"FORM","1794":"SMTH","1795":"TUNE","1796":"FRZE","1797":"KEYT","1798":"ROUG","1799":"SUB ","1921":"TONE","1922":"DRIV","1923":"BITS","1924":"RATE","1925":"DLY ","1926":"FDBK","1927":"WDTH","1928":"MIX ","1929":"X-PN","2048":"RATE","2049":"DPTH","2050":"MIX ","2051":"RAMP","2052":"BAL ","2053":"XOVR","2054":"SPRD","2176":"FMT ","2177":"REC ","2182":"HISS","2183":"MIX ","2184":"AGE ","2185":"CRNK","2186":"MECH","2187":"INRT","2304":"ALGO","2305":"DECY","2306":"PRE ","2307":"DAMP","2308":"DIFF","2309":"MOD ","2310":"WDTH","2311":"MIX ","2432":"OUT ","2438":"BLND","2441":"ADLY","2442":"BDLY","2443":"AREV","2444":"BREV","2445":"CURV","2448":"CDLY","2449":"DDLY","2450":"CREV","2451":"DREV","2453":"PRIO","2454":"THR ","3200":"SIZE","3201":"AMUT","3202":"TMUT","3203":"EVOL","3204":"MEM ","3205":"PULL","3206":"LAW ","3207":"DRFT","3210":"CAPT","3328":"RATE","3329":"FORM","3330":"SIZE","3331":"WIN ","3332":"SKEW","3333":"OMIT","3334":"BRST","3335":"JITT","3338":"ERAT","3339":"EDEP","3340":"FRAT","3341":"FDEP","3342":"SPRY","3343":"FVAR","3344":"REV%","3345":"KEYT"};
const E16_ROW_TITLES=["ENGINE","STACK PATH","MOTION PATH","MOTION SOUND","LANES PLAY","GRAINS MAIN","GRAINS CLOUD","GRAINS PROCESS","CUTUPS CLOCK","CUTUPS PATH","DIRECT PLAY","DIRECT PLAY","SLICE SOURCE","GLITCH CLOCK","GLITCH PATTERN","WEAVE","WEAVE MOTION","WAVESETS","PRISM","MORPH","TEXTURE","FORMANT","STOCHASTIC","PULSAR","PULSAR MOD","PACKETS","MOTOR","EVENT CLOCK","EVENT SHAPE","PART COLOR","STEREO ROTARY","FX SENDS","FX SENDS","DELAY RETURN","REVERB","TAPE"];
const E16_PAGES=["ENGINE","STACK","ENG 1","ENG 2","ENG 3","ENG 4","SENDS","COLOR","ROTARY","DELAY","REVERB","TAPE"];
const E16_ENGINES=["MOTN","LANS","GRNS","CUTS","SAMP","SLIC","GLCH","WEAV","WSET","PRSM","MORP","TEXT","FORM","STOC","PLSR"];
// Small Remote-only transport and renderer. No Lua runs on E16.
// OXI protocol: https://docs.google.com/spreadsheets/d/1Yccnrluv10QL_PauMmtCt64EtYjfSeZEXKrlrw8P24w/edit
const e16RemoteProtocol=(()=>{
 const HEADER=[0,33,91,2,1];
 const FONT={'"':[5,5,0,0,0],"0":[7,5,5,5,7],"1":[2,6,2,2,7],"2":[6,1,2,4,7],"3":[6,1,2,1,6],"4":[5,5,7,1,1],"5":[7,4,6,1,6],"6":[3,4,7,5,7],"7":[7,1,2,2,2],"8":[7,5,7,5,7],"9":[7,5,7,1,6],"A":[2,5,7,5,5],"B":[6,5,6,5,6],"C":[3,4,4,4,3],"D":[6,5,5,5,6],"E":[7,4,6,4,7],"F":[7,4,6,4,4],"G":[3,4,5,5,3],"H":[5,5,7,5,5],"I":[7,2,2,2,7],"J":[1,1,1,5,2],"K":[5,5,6,5,5],"L":[4,4,4,4,7],"M":[5,7,7,5,5],"N":[5,7,7,7,5],"O":[2,5,5,5,2],"P":[6,5,6,4,4],"Q":[2,5,5,7,3],"R":[6,5,6,5,5],"S":[3,4,2,1,6],"T":[7,2,2,2,2],"U":[5,5,5,5,7],"V":[5,5,5,5,2],"W":[5,5,7,7,5],"X":[5,5,2,5,5],"Y":[5,5,2,2,2],"Z":[7,1,2,4,7]," ":[0,0,0,0,0],".":[0,0,0,0,2],"-":[0,0,7,0,0],"%":[5,1,2,4,5],"/":[1,1,2,4,4],":":[0,2,0,2,0],"+":[0,2,7,2,0],"?":[6,1,2,0,2]};
 const ascii=s=>String(s??'').toUpperCase().replace(/[^\x20-\x7e]/g,'?');
 const pack=bytes=>{const out=[];for(let at=0;at<bytes.length;at+=7){const part=bytes.slice(at,at+7);out.push(part.reduce((v,b,i)=>v|((b>>7)<<i),0),...part.map(b=>b&127));}return out;};
 const unpack=bytes=>{const out=[];for(let at=0;at<bytes.length;){const bits=bytes[at++];for(let i=0;i<7&&at<bytes.length;i++)out.push(bytes[at++]|(((bits>>i)&1)<<7));}return out;};
 const sysex=(op,data=[])=>[240,...HEADER,...op,...pack(data),247];
 const packetize=bytes=>{const out=[];for(let at=0;at<bytes.length;at+=3){const n=Math.min(3,bytes.length-at);out.push(at+n===bytes.length?n+4:4,...bytes.slice(at,at+n));while(out.length%4)out.push(0);}return out;};
 const tiles=Array.from({length:36},(_,i)=>i<4?{x:i*32,y:0,w:32,h:5}:{x:Math.floor((i-4)/2)%4*32,y:8+Math.floor(Math.floor((i-4)/2)/4)*14+(i%2?6:0),w:32,h:5});
 function render(view){
  const frame=new Uint8Array(8192);
  const text=(x,y,str,limit)=>{for(const ch of ascii(str).slice(0,limit)){const glyph=FONT[ch]||FONT['?'];for(let row=0;row<5;row++)for(let col=0;col<3;col++)if(glyph[row]&(4>>col))frame[(y+row)*128+x+col]=1;x+=4;}};
  text(1,0,view.title,31);
  for(let i=0;i<16;i++){const c=view.cells[i];if(!c)continue;const x=i%4*32,y=8+Math.floor(i/4)*14;if(c.reserved){for(let dy=0;dy<3;dy++)for(let dx=0;dx<3;dx++)if(dx!==1||dy!==1)frame[(y+6+dy)*128+x+14+dx]=1;continue;}text(x+1,y,c.label,7);text(x+1,y+6,c.text,7);}
  return tiles.map(t=>{const b=Array(20).fill(0);for(let y=0;y<5;y++)for(let x=0;x<32;x++)if(frame[(t.y+y)*128+t.x+x])b[y*4+(x>>3)]|=1<<(7-(x&7));return b;});
 }
 function create(options){
  // Bound OLED work by acknowledged pixels and a packet-rate budget. Each
  // complete message fits one host write (12 USB packets); never split SysEx.
  const ACK_MS=1000,RETRY_LIMIT=2,HEARTBEAT_MS=2000,MAX_FLIGHT=8;
  const now=options.now||(()=>Date.now());
  let leased=false,enabled=true,pendingExit=false,active=false,ready=false;
  let lastProbe=-10000,lastAck=0,reconnectAt=0,probePending=false,clearAt=null,clearTries=0;
  let cursor=0,ringCursor=0,touched=-1,shift=false,buttons=0,window=320,preferText=false;
  let packetCredit=12,budgetAt=now();
  let pageKey=null,assembler=null,midiStatus=0,midiData=[];
  let view={title:'ENGINE',cells:Array(16).fill(null)},signature='',desired=render(view);
  let acks=0,nacks=0,retries=0,reconnects=0,issue='NONE';
  const sent=Array(36).fill(null),flight=Array(36).fill(null),retryAt=Array(36).fill(0),failures=Array(36).fill(0);
  const rings=Array(16).fill(''),zero=Array(20).fill(0).join(',');
  const transmit=(b,metered=false)=>{
   const p=packetize(b),cost=p.length/4;if(p.length>48)throw Error('Remote write exceeds 12 USB-MIDI packets');
   if(metered&&cost>packetCredit)return false;
   if(options.send(p)===false)return false;
   if(metered)packetCredit-=cost;return true;
  };
  const pendingPixels=()=>flight.reduce((n,f)=>n+(f?.pixels||0),0);
  const ownership=(a,r)=>{const ok=options.onOwnership?.(a,r)!==false;if(ok)leased=r;return ok;};
  function stop(){
   if(active)pendingExit=!transmit(sysex([6,0]));
   active=ready=false;probePending=false;clearAt=null;flight.fill(null);
   shift=false;buttons=0;options.onButtons?.(0);assembler=null;midiStatus=0;midiData=[];
   if(pendingExit)ownership(false,true);else if(leased)ownership(false,false);
  }
  function recover(reason){
   issue=reason;reconnects++;stop();reconnectAt=now()+1000;lastProbe=-10000;
   options.onError?.('E16 RECONNECTING');
  }
  function sendClear(){if(transmit(sysex([7]))){clearAt=now();clearTries++;return true;}return false;}
  function receive(body){
   if(pendingExit||!HEADER.every((v,i)=>body[i]===v))return;
   const tail=body.slice(5),off=tail[0]===6?1:0,op=tail[off],payload=unpack(tail.slice(off+1));
   if(op===83&&payload.length===0){
    // A heartbeat confirms presence, without repainting or clearing the view.
    if(!probePending)return;probePending=false;lastAck=now();
    if(!active){
     if(!ownership(false,true)){transmit(sysex([6,0]));lastProbe=now();return;}
     active=true;ready=false;clearAt=null;clearTries=0;sendClear();
    }else if(ready)ownership(true,true);
    return;
   }
   if(!active||payload.length!==6)return;
   if(payload[0]===7&&!ready&&clearAt!==null){
    if(op===83&&payload[1]===0){
     if(!ownership(true,true))return;
     lastAck=now();ready=true;clearAt=null;sent.fill(zero);flight.fill(null);
     retryAt.fill(0);failures.fill(0);rings.fill('');cursor=ringCursor=0;window=320;preferText=false;packetCredit=12;budgetAt=now();
     options.onReady?.();
    }else if(op===84){recover('CLEAR NACK');}
    return;
   }
   if(!ready||payload[0]!==8)return;
   const i=flight.findIndex(f=>f&&f.rect.every((v,j)=>v===payload[j+2]));
   const f=flight[i];if(!f)return; // Ignore another surface's or late replies.
   if(op===83&&payload[1]===0){
    lastAck=now();acks++;sent[i]=f.sig;flight[i]=null;failures[i]=0;
    // ACKs carry geometry, not a sequence number. Let duplicate replies from
    // a retransmission drain before sending a newer value at that geometry.
    retryAt[i]=f.tries?now()+ACK_MS:0;
    if(acks%8===0)window=Math.min(320,window+80);
   }else if(op===84){
    lastAck=now();nacks++;issue='TILE NACK';flight[i]=null;sent[i]=null;window=Math.max(160,Math.floor(window/2));
    retryAt[i]=now()+Math.min(800,100*(1+failures[i]));
    if(payload[1]===4||++failures[i]>=3)recover('TILE NACK');
   }
  }
  function message(status,n,value){
   if(!active||(status&15)!==0)return;
   const type=status&240;
   if(type===176&&n>=1&&n<=16){if(!shift)touched=n-1;return;}
   if((type===144||type===128)&&n<=16){
    const down=type===144&&value>0,bit=1<<n,fresh=down&&!(buttons&bit),next=down?buttons|bit:buttons&~bit;
    if(next!==buttons){buttons=next;options.onButtons?.(buttons);}
    if(n===16)shift=down;
    else if(fresh&&shift&&n===0)options.onPair?.();
    else if(fresh&&shift&&n===15){stop();enabled=false;}
   }
  }
  function sendRings(){
   const changed=[],order=touched>=0?[touched,...Array.from({length:16},(_,i)=>(ringCursor+i)%16).filter(i=>i!==touched)]:Array.from({length:16},(_,i)=>(ringCursor+i)%16);
   for(const i of order){
    const c=view.cells[i],amount=Math.max(0,Math.min(16383,Math.round(c?.raw||0)));
    const raw=[i,...(c?.rgb||[0,0,0]),amount>>7,amount&127,c?.bipolar?1:0],sig=raw.join(',');
    if(rings[i]!==sig)changed.push({i,raw,sig});if(changed.length===3)break;
   }
   if(!changed.length)return null;
   if(!transmit(sysex([6,4],changed.flatMap(c=>c.raw)),true))return false;
   for(const c of changed)rings[c.i]=c.sig;
   ringCursor=(changed[changed.length-1].i+1)%16;return true;
  }
  function headingPending(){
   for(let i=0;i<4;i++)if(flight[i]||sent[i]!==desired[i].join(','))return true;
   return false;
  }
  // Trim a tile to changed pixels, including pixels that must turn OFF.
  // Rows remain byte-aligned in the wire payload even at a non-byte x/width.
  function patch(i,raw){
   const before=sent[i]===null?null:sent[i].split(',').map(Number);
   let x0=32,y0=5,x1=-1,y1=-1;
   for(let y=0;y<5;y++)for(let x=0;x<32;x++)if(!before||((raw[y*4+(x>>3)]^before[y*4+(x>>3)])&(128>>(x&7)))){
    x0=Math.min(x0,x);x1=Math.max(x1,x);y0=Math.min(y0,y);y1=Math.max(y1,y);
   }
   if(x1<0)return null;
   const w=x1-x0+1,h=y1-y0+1,stride=Math.ceil(w/8),bits=Array(stride*h).fill(0),t=tiles[i];
   for(let y=0;y<h;y++)for(let x=0;x<w;x++)if(raw[(y+y0)*4+((x+x0)>>3)]&(128>>((x+x0)&7)))bits[y*stride+(x>>3)]|=128>>(x&7);
   return {rect:[t.x+x0,t.y+y0,w,h],raw:bits,pixels:w*h};
  }
  function sendText(time,headerOnly=false){
   if(flight.filter(Boolean).length>=MAX_FLIGHT)return null;
   const urgent=touched>=0?5+touched*2:-1,pending=pendingPixels();
   for(let pass=0;pass<(headerOnly?4:37);pass++){
    const i=headerOnly?pass:pass===0&&urgent>=0?urgent:(cursor+pass-(urgent>=0?1:0)+36)%36;
    if(flight[i]||time<retryAt[i])continue;
    const raw=desired[i],sig=raw.join(',');if(sent[i]===sig)continue;
    const region=patch(i,raw);if(!region||pending+region.pixels>window)continue;
    if(!transmit(sysex([8],[...region.rect,...region.raw]),true))return false;
    flight[i]={...region,sig,at:time,tries:0};cursor=(i+1)%36;return true;
   }
   return null;
  }
  const api={
   get active(){return active;},get ready(){return ready;},
   get diagnostics(){return {state:!enabled?'OFF':pendingExit||now()<reconnectAt?'RECONNECT':!active?'SEARCH':ready?'LIVE':'WAIT CLEAR',issue,acks,nacks,retries,reconnects,inflight:flight.filter(Boolean).length,window:MAX_FLIGHT,pixels:pendingPixels(),pixelWindow:window};},
   stop,enable(v){if(!v)stop();enabled=!!v;if(enabled){lastProbe=-10000;reconnectAt=0;}},
   resume(){stop();enabled=true;lastProbe=-10000;reconnectAt=0;},
   setView(next){
    if(next.pageKey!==pageKey){pageKey=next.pageKey;cursor=0;touched=-1;}
    view=next;
    const nextSig=view.title+'|'+view.cells.map(c=>c?c.label+':'+c.text+':'+!!c.reserved:'').join('|');
    if(nextSig!==signature){signature=nextSig;desired=render(view);}
   },
   feed(bytes){
    if(!enabled)return;
    for(const v of bytes||[]){const b=v&255;if(b>=248)continue;
     if(b===240){assembler=[];midiStatus=0;midiData=[];continue;}
     if(b===247){if(assembler)receive(assembler);assembler=null;continue;}
     if(assembler){if(b<128&&assembler.length<128){assembler.push(b);continue;}assembler=null;}
     if(b>=128){midiStatus=b;midiData=[];continue;}
     if(!midiStatus)continue;midiData.push(b);
     const n=[192,208].includes(midiStatus&240)?1:2;
     if(midiData.length===n){if(n===2)message(midiStatus,midiData[0],midiData[1]);midiData=[];}
    }
   },
   tick(){
    if(pendingExit){if(transmit(sysex([6,0]))){pendingExit=false;if(leased)ownership(false,false);}return;}
    if(!enabled)return;const time=now();
    // Below the shim's default drain (~1 packet/ms). An idle tick may spend
    // at most 24 packets; it cannot accumulate a large reconnect/page burst.
    packetCredit=Math.min(24,packetCredit+Math.max(0,time-budgetAt)*.75);budgetAt=time;
    if(time<reconnectAt)return;
    if(active&&time-lastAck>6500){recover('LINK TIMEOUT');return;}
    if(active&&!ready){
     if(clearAt===null){sendClear();return;}
     if(time-clearAt>=ACK_MS){
      if(clearTries>RETRY_LIMIT){recover('CLEAR TIMEOUT');return;}
      if(sendClear())retries++;
     }
     return;
    }
    if(time-lastProbe>=(active?HEARTBEAT_MS:1500)){
     if(active&&!ownership(ready,true))return;
     if(transmit(sysex([6,85]))){lastProbe=time;probePending=true;}return;
    }
    if(!active||!ready)return;
    for(let i=0;i<flight.length;i++){
     const f=flight[i];if(!f||time-f.at<ACK_MS)continue;
     if(f.tries>=RETRY_LIMIT){recover('TILE TIMEOUT');return;}
     if(transmit(sysex([8],[...f.rect,...f.raw]),true)){f.tries++;f.at=time;retries++;window=160;issue='TILE TIMEOUT';}
     return;
    }
    // Complete the heading before the new body/rings. Within that priority,
    // spend a packet budget instead of waiting one UI tick for every message.
    for(let pass=0;pass<8;pass++){
     if(headingPending()){
      if(sendText(time,true)===true)continue;
      return;
     }
     let result;
     if(preferText){
      result=sendText(time);
      if(result!==null){if(!result)return;preferText=false;continue;}
     }
     result=sendRings();
     if(result!==null){if(!result)return;preferText=true;continue;}
     result=sendText(time);if(result!==true)return;preferText=false;
    }
   }
  };return api;
 }
 return {create,pack,unpack,sysex,packetize,render,tiles};
})();
// MIDI view feedback plus the official E16 Remote adapter. Remote uses the
// current high-resolution parameter map while rendering readable names and
// formatted values on E16 itself.
const midiSurface={query:'',epoch:null};
const E16_PAGE_JUMP_MASK=1<<17;
const remoteButtons={mask:0,sent:null,physical:0,pair:0,target:null,mode:0,pageJump:false,data:null};
function sendExternal(packets) {
    return typeof move_midi_external_send==='function' && move_midi_external_send(packets);
}
function createNrpnFeedback(send) {
    const pending=new Map(),urgent=new Map(),sent=new Map();
    return {
        reset(){pending.clear();urgent.clear();sent.clear();},
        update(values){
            for(const entry of values){
                if(!Array.isArray(entry)||(entry.length!==3&&entry.length!==4))continue;
                const [channel,id,value,priority=0]=entry;
                if(![channel,id,value].every(Number.isInteger)||channel<1||channel>16||id<0||id>=16383||value<0||value>16383)continue;
                const key=channel+':'+id;
                // Priority marks an explicit controller resync request. Send it
                // even when the same value was delivered before disconnection;
                // the newly connected E16 has not necessarily received it.
                if(priority===1){pending.delete(key);urgent.set(key,{channel,id,value});}
                else if(urgent.has(key))urgent.set(key,{channel,id,value});
                else if(sent.get(key)===value)pending.delete(key);
                else pending.set(key,{channel,id,value});
            }
        },
        tick(){
            // Three complete NRPNs fit the host's 12-packet output bound.
            for(let i=0;i<3;++i){
                const queue=urgent.size?urgent:pending;
                const first=queue.entries().next();if(first.done)return;
                const[key,{channel,id,value}]=first.value,status=0xb0|(channel-1);
                const packets=[];
                for(const[cc,v]of [[99,id>>7],[98,id&127],[6,value>>7],[38,value&127]])packets.push(0x0b,status,cc,v);
                if(send(packets)===false)return;
                sent.set(key,value);queue.delete(key);
            }
        },
    };
}
const midiFeedback=createNrpnFeedback(sendExternal);
let remoteModeAllowed=false;
function syncRemoteTransport(readDsp=false,force=false){
    if(readDsp){
        const raw=Number(getParam('midi_control_mode',PARAMS.midi_control_mode.value));
        if(Number.isFinite(raw))PARAMS.midi_control_mode.value=clamp(Math.round(raw),0,6);
    }
    const mode=Math.round(PARAMS.midi_control_mode.value);
    const allowed=(mode===0&&customMidiActive()===0)||mode===4;
    if(force||allowed!==remoteModeAllowed){
        remoteModeAllowed=allowed;
        if(allowed)remoteE16.resume();else remoteE16.enable(false);
    }
    return allowed;
}
function resetMidiSurface(){
    readCustomMidi();
    midiSurface.query='';midiSurface.epoch=null;midiFeedback.reset();e16NativeFeedback.reset();
    syncRemoteTransport(true,true);
}
function pollMidiControls(){
    readCustomMidi();
    // Move's eight visible controls use midi_view. E16 has a separate
    // page snapshot and never changes Move's part or editing page.
    const keys=state.view==='chain'?CHAIN_PAGE.keys:activeControlKeys();
    const selection=[];
    for(const key of keys){
        const p=thruMidi.MIDI_BY_KEY.get(key);
        // Keep each CC aligned to its physical slot, including blank cells.
        selection.push(p&&!p.action?(p.scope==='part'?state.splitFocus:-1)+':'+p.id:'-1:-1');
    }
    const query=selection.slice(0,8).join(';');
    if(query!==midiSurface.query){
        try{
            const result=typeof host_module_set_param_blocking==='function'
                ?host_module_set_param_blocking('midi_view',query,10):host_module_set_param('midi_view',query);
            if(result===false||result===0)return;
            midiSurface.query=query;
        }catch(_){return;}
    }
    let data;
    try{data=JSON.parse(String(getParam('midi_state','')));}catch(_){return;}
    if(data?.v!==1||!Array.isArray(data.values))return;
    if(Number.isInteger(data.epoch)&&data.epoch!==midiSurface.epoch){
        midiSurface.epoch=data.epoch;midiFeedback.reset();
    }
    for(const[part,id,value]of data.values){
        const p=thruMidi.MIDI_BY_ID.get(id);
        if(!p||!Number.isFinite(value)||!selection.includes(part+':'+id))continue;
        if(PARAMS[p.key]&&(part<0||part===state.splitFocus)){
            const control=controlForKey(p.key);
            if(control)PARAMS[p.key].value=clamp(value,control.min,control.max);
        }
    }
}
function tickMidiFeedback(){
    if(typeof move_midi_external_send!=='function')return;
    const remoteAllowed=syncRemoteTransport();
    if(remoteAllowed&&remoteE16.ready)flushRemoteButtons();
    if(remoteAllowed&&remoteE16.ready)try{updateRemoteE16(JSON.parse(String(getParam('e16_remote_state','{}'))));}catch(_){}
    remoteE16.tick();
    if(remoteE16.active)return;
    if(Math.round(PARAMS.midi_control_mode.value)===5){
        try{e16NativeFeedback.update(JSON.parse(String(getParam('e16_native_state','{}'))));}catch(_){}
        e16NativeFeedback.tick();return;
    }
    e16NativeFeedback.reset();
    try{const values=JSON.parse(String(getParam('midi_feedback','[]')));if(Array.isArray(values))midiFeedback.update(values);}catch(_){}
    midiFeedback.tick();
}
function e16Number(value,places=2){return String(Number(value.toFixed(places)));}
function e16Time(ms){return ms>=1000?e16Number(ms/1000,2)+'S':e16Number(ms,ms<10?2:0)+'MS';}
function e16Value(p,value){
    const key=p.key;
    if(key==='algorithm')return E16_ENGINES[Math.round(value)]||'?';
    if(key==='effect_cross_pan')return value<=-99.5?'NORMAL':value>=99.5?'SWAP':Math.abs(value)<.5?'MONO':Math.round(value)+'%';
    if(key==='effect_rotary_rate'&&value<.05)return 'STOP';
    if(key==='effect_rotary_balance')return value<=-99.5?'DRUM':value>=99.5?'HORN':Math.abs(value)<.5?'EVEN':(value>0?'H':'D')+Math.abs(Math.round(value));
    if(key==='effect_tape_hiss')return value<=p.min+.5?'OFF':Math.round(value)+'DB';
    if(key==='effect_tape_inertia')return value<=0?'OFF':e16Time(tapeInertiaSeconds(value)*1000);
    if(key==='stochastic_evolution')return value<=0?'HOLD':value>=1023.5?'CYC':e16Number(value,value<10?2:value<100?1:0);
    if(p.options)return String(p.options[Math.round(value-p.min)]??Math.round(value)).toUpperCase();
    if(['speed','stretch_ratio','grain_pitch','long_grain_pitch'].includes(key))return value.toFixed(2)+'X';
    const unit=({prism_blur:'ms',prism_decay:'ms',morph_blur:'ms',texture_length:'ms',
        effect_reverb_decay:'s',effect_reverb_pre:'ms',effect_reverb_damp:'Hz',
        prism_pitch:'st',morph_pitch:'st'})[key]||p.unit;
    if(unit==='ms')return e16Time(value);
    if(unit==='s')return e16Number(value,2)+'S';
    if(unit==='Hz')return value>=1000?e16Number(value/1000,1)+'KHZ':e16Number(value,value<10?2:1)+'HZ';
    if(unit==='st')return e16Number(value,1)+'ST';
    if(unit==='%')return Math.round(value)+'%';
    if(p.integer)return String(Math.round(value));
    if(p.min>=-1&&p.max<=2)return Math.round(value*100)+'%';
    if(p.max===100&&p.min>=-100)return Math.round(value)+'%';
    return e16Number(value,value<10?2:value<100?1:0);
}
function e16PartFamily(mode,part){return part===0?0:part===1?(mode===2?3:1):part===2?2:3;}
function e16PageView(data){
    if(data?.v!==2||data.active!==1||!Array.isArray(data.values)||!Array.isArray(data.alg)
        ||!Array.isArray(data.pages)||!Array.isArray(data.rows))return null;
    const shared=data.shared===1,multi=data.mode!==0;
    const cells=Array.from({length:16},(_,i)=>i<(shared||!multi?8:16)?{reserved:true}:null);
    for(const[index,raw,id,part,steps,max]of data.values){
        const p=thruMidi.MIDI_BY_ID.get(id);if(!p||!Number.isInteger(index)||index<0||index>15||!Number.isInteger(raw)||raw<0||raw>16383||!Number.isFinite(max)||max<p.min)continue;
        const ratio=raw/16383,value=p.curve==='log'?p.min*Math.pow(max/p.min,ratio):p.min+(max-p.min)*ratio;
        let label=E16_LABELS[id]||p.name.slice(0,4),text;
        if(p.key==='algorithm')label='ALG';
        if(p.key==='manual_lane'){label=data.alg[part]===3?'LANE':'STK';text=steps?String(Math.round(value)):value.toFixed(2);}
        else text=e16Value(p,value);
        const namedPart=/^split_(delay|reverb)_([abcd])$/.exec(p.key);
        const colorPart=namedPart?namedPart[2].charCodeAt(0)-97:part;
        cells[index]={label:label.trim(),text:String(text).slice(0,7),raw,bipolar:p.min<0&&max>0,family:colorPart<0&&shared?4:e16PartFamily(data.mode,Math.max(0,colorPart))};
    }
    const pair=data.mode===3?data.pair:0,left=pair===2?'C':'A',right=pair===2?'D':'B';
    const number=(data.page+1)+'/12';
    const page=E16_PAGES[data.page]||'ENGINE';let title;
    if(data.shift)title='TURN1 '+number+' '+page;
    else if(data.page>=2&&data.page<=5){
        const name=i=>data.rows[i]>0?(E16_ROW_TITLES[data.rows[i]-1]||page).slice(0,9):'---';
        title=number+' '+left+':'+name(0)+(multi?' / '+right+':'+name(1):'');
    }else title=number+' '+page+(shared?(data.page===6?(data.mode===3?' A-D':' A/B'):''):' '+left+(multi?'/'+right:''));
    return {title:title.slice(0,31),pageKey:data.mode+':'+pair+':'+data.page,cells};
}
const E16_REMOTE_RGB=[[28,28,24],[2,26,22],[30,28,4],[22,8,20],[24,8,34]];
function remoteOwnership(active,requested){
    if(requested&&!remoteModeAllowed)return false;
    try{
        const value=(active?1:0)+','+(requested?1:0);
        const result=typeof host_module_set_param_blocking==='function'
            ?host_module_set_param_blocking('e16_remote',value,10):host_module_set_param('e16_remote',value);
        return result!==false&&result!==0;
    }catch(_){return false;}
}
function setRemoteControl(key,value){
    try{
        const result=typeof host_module_set_param_blocking==='function'
            ?host_module_set_param_blocking(key,String(value),10):host_module_set_param(key,String(value));
        return result!==false&&result!==0&&result!==null;
    }catch(_){return false;}
}
function flushRemoteButtons(){
    if(!remoteE16.ready)return false;
    if(remoteButtons.sent!==remoteButtons.mask){
        if(!setRemoteControl('e16_remote_buttons',remoteButtons.mask))return false;
        remoteButtons.sent=remoteButtons.mask;
    }
    if(remoteButtons.target!==null&&setRemoteControl('e16_remote_pair',remoteButtons.target)){
        remoteButtons.pair=remoteButtons.target;remoteButtons.target=null;
    }
    return true;
}
function remoteButtonState(mask){
    const previous=remoteButtons.physical,fresh=mask&~previous,shifted=(mask&(1<<16))!==0;
    remoteButtons.physical=mask;
    // Shift + encoder 2 toggles a local index. Releasing Shift and pressing
    // encoders 1-12 selects a page; encoder 16 cancels without changing page.
    if(shifted&&(fresh&(1<<1))){
        remoteButtons.pageJump=!remoteButtons.pageJump;
        remoteButtons.mask=remoteButtons.pageJump?E16_PAGE_JUMP_MASK:0;
        if(!flushRemoteButtons()){remoteButtons.pageJump=false;remoteButtons.mask=mask;flushRemoteButtons();return;}
        if(remoteButtons.data)remoteE16.setView(remoteButtons.pageJump
            ?remotePageJumpView(remoteButtons.data):remoteE16View(remoteButtons.data));
        return;
    }
    if(remoteButtons.pageJump){
        remoteButtons.mask=E16_PAGE_JUMP_MASK;flushRemoteButtons();
        if(!shifted){
            let page=-1;for(let i=0;i<12;++i)if(fresh&(1<<i)){page=i;break;}
            if(page>=0&&setRemoteControl('e16_remote_page',page)){
                remoteButtons.pageJump=false;remoteButtons.mask=0;flushRemoteButtons();
            }else if(fresh&(1<<15)){
                remoteButtons.pageJump=false;remoteButtons.mask=0;flushRemoteButtons();
                if(remoteButtons.data)remoteE16.setView(remoteE16View(remoteButtons.data));
            }
        }
        return;
    }
    remoteButtons.mask=mask;flushRemoteButtons();
}
function remotePairSwitch(){
    if(remoteButtons.mode!==3)return;
    remoteButtons.target=(remoteButtons.target??remoteButtons.pair)===0?2:0;
    flushRemoteButtons();
}
const remoteE16=e16RemoteProtocol.create({send:sendExternal,onOwnership:(active,requested)=>{
    const ok=remoteOwnership(active,requested);
    if(ok&&active)remoteButtons.sent=null;
    if(!requested)Object.assign(remoteButtons,{mask:0,sent:null,physical:0,pair:0,target:null,mode:0,pageJump:false,data:null});
    return ok;
},onButtons:remoteButtonState,onPair:remotePairSwitch,onError:message=>showActionMessage(message)});
function remoteE16View(data){
    const view=e16PageView(data);if(!view)return null;
    return {...view,cells:view.cells.map(c=>c&&!c.reserved?{...c,rgb:E16_REMOTE_RGB[c.family]||E16_REMOTE_RGB[0]}:c)};
}
function remotePageJumpView(data){
    const cells=Array(16).fill(null);
    for(let i=0;i<12;++i)cells[i]={label:E16_PAGES[i],text:i===data.page?'CURRENT':'OPEN',
        raw:i===data.page?16383:3072,rgb:E16_REMOTE_RGB[0],bipolar:false};
    cells[15]={label:'BACK',text:'CANCEL',raw:0,rgb:E16_REMOTE_RGB[4],bipolar:false};
    return {title:'PAGE JUMP - PRESS 1-12',pageKey:'jump:'+data.mode+':'+data.pair+':'+data.page,cells};
}
function updateRemoteE16(data){
    const view=remoteE16View(data);if(!view)return;
    remoteButtons.data=data;
    remoteButtons.mode=data.mode;
    if(remoteButtons.target===null)remoteButtons.pair=data.pair;
    else if(data.pair===remoteButtons.target){remoteButtons.pair=data.pair;remoteButtons.target=null;}
    remoteE16.setView(remoteButtons.pageJump?remotePageJumpView(data):view);
}
globalThis.onMidiMessageExternal=bytes=>{if(syncRemoteTransport())remoteE16.feed(bytes);};
globalThis.onResume=function(){resetMidiSurface();};
// One direction controls sound (native relative CC); this direction draws rings.
function createE16NativeFeedback(send) {
    let identity='';const pending=new Map(),sent=new Map();
    return {
        reset(){identity='';pending.clear();sent.clear();},
        update(data){
            if(data?.v!==1||!Number.isInteger(data.page)||data.page<0||data.page>11
                ||!Number.isInteger(data.token)||data.token<0||data.token>127||!Array.isArray(data.values))return;
            const next=data.page+':'+data.token;
            if(next!==identity){identity=next;pending.clear();sent.clear();}
            for(const entry of data.values){
                if(!Array.isArray(entry)||entry.length!==3)continue;
                const [index,raw,color]=entry;
                if(!entry.every(Number.isInteger)||index<0||index>15||raw<0||raw>16383||color<0||color>100)continue;
                const value=raw+':'+color;
                if(sent.get(index)===value)pending.delete(index);
                else pending.set(index,{index,raw,color,value,page:data.page,token:data.token});
            }
        },
        tick(){
            const entries=[...pending.values()].slice(0,6);if(!entries.length)return;
            const first=entries[0],bytes=[240,125,83,84,1,first.page,first.token];
            for(const e of entries)bytes.push(e.index,e.raw>>7,e.raw&127,e.color);
            bytes.push(247);
            const packets=[];
            for(let i=0;i<bytes.length;i+=3){
                const remaining=bytes.length-i,cin=remaining>3?4:4+remaining;
                packets.push(cin,bytes[i],bytes[i+1]??0,bytes[i+2]??0);
            }
            if(send(packets)===false)return;
            for(const e of entries){sent.set(e.index,e.value);pending.delete(e.index);}
        },
    };
}
const e16NativeFeedback=createE16NativeFeedback(sendExternal);
