/* s3g Thru — stereo sample and synthesis instrument for Move. */

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

const UI_VERSION = '0.33.0-test.36';
const HELP_PAGE_COUNT=8;
const WAVE_BINS = 112;
const ALGORITHMS = ['MOTION', 'LANES', 'GRAINS', 'CUTUPS', 'SAMPLE', 'SLICES',
    'GLITCH', 'WEAVE', 'WAVESETS', 'PRISM', 'MORPH', 'TEXTURE', 'FORMANT', 'STOCH', 'PULSAR', 'TABLE', 'SCANNED', 'FM', 'NOISE', 'RESON', 'PARTIALS', 'LOWFORM'];
function isSynthAlgorithm(a){return a===15||a===16||a===17||a===18||a===19||a===20||a===21;}
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
    bank_master_rate:{key:'bank_master_rate',label:'BANK GLOBAL RATE',shortLabel:'RATE',min:.125,max:8,step:.01,value:1},
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
    ...Object.fromEntries(Array.from({length:8},(_,i)=>['voice_engine_'+(i+1),{key:'voice_engine_'+(i+1),label:'SLOT '+(i+1),shortLabel:'E'+(i+1),min:0,max:21,step:1,value:0,integer:true,labels:ALGORITHMS}])),
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
    algorithm: { key: 'algorithm', label: 'ALGORITHM', min: 0, max: 21, step: 1, value: 0, integer: true, labels: ALGORITHMS },
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
    pattern_fade_lookahead: { key: 'pattern_fade_lookahead', label: 'FADE LOOKAHEAD', shortLabel: 'LOOK', min: 0, max: 1, step: 1, value: 1, integer: true, labels: ['OFF', '12ms'] },
    clip_rate: { key: 'clip_rate', label: 'LOOP RATE', min: 0.25, max: 4, step: 0.05, value: 1 },
    clip_record_mode: { key: 'clip_record_mode', label: 'REC MODE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['REPLACE', 'OVERDUB'] },
    clip_clear_recording: { key:'clip_clear_recording',label:'CLEAR RECORDING',shortLabel:'CLR',min:0,max:1,step:1,value:0,integer:true,labels:['TURN','CLEAR'],momentary:true },
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
MAIN_ENGINE_CONTROLS[16]=['locus','field',null,'manual_lane','lane_cycles',null,'scanned_freeze','output_gain'];
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
// TABLE uses the four existing patch lanes; all sample engine IDs stay fixed.
const TABLE_CONTROLS=[{"index":0,"key":"table_type","symbol":"type","label":"TYPE","name":"Table Variant","min":0,"max":2,"default":0,"step":1,"options":["WAVE","SPLINE","WT DRUM"],"page":"TABLE","curve":null},{"index":1,"key":"table_tune","symbol":"tune","label":"TUNE","name":"Transpose","min":-48,"max":48,"default":0,"step":0.1,"options":null,"page":"TABLE","curve":null},{"index":2,"key":"table_width","symbol":"width","label":"WID","name":"Stereo Width","min":0,"max":100,"default":40,"step":1,"options":null,"page":"TABLE","curve":null},{"index":3,"key":"table_level","symbol":"level","label":"LVL","name":"Generator Level","min":0,"max":100,"default":75,"step":1,"options":null,"page":"TABLE","curve":null},{"index":4,"key":"table_art","symbol":"art","label":"ART","name":"Articulation","min":0,"max":1,"default":0,"step":1,"options":["TONE","HIT"],"page":"TABLE","curve":null},{"index":5,"key":"table_decay","symbol":"decay","label":"DEC","name":"Hit Decay","min":10,"max":12000,"default":650,"step":1,"options":null,"page":"TABLE","curve":"log"},{"index":6,"key":"table_noise","symbol":"noise","label":"NOIS","name":"Noise Level","min":0,"max":100,"default":0,"step":1,"options":null,"page":"TABLE","curve":null},{"index":7,"key":"table_drive","symbol":"drive","label":"DRIV","name":"Drive","min":0,"max":100,"default":0,"step":1,"options":null,"page":"TABLE","curve":null},{"index":8,"key":"table_shape","symbol":"shape","label":"SHAP","name":"Wave Shape","min":0,"max":3,"default":0,"step":1,"options":["SINE","TRI","SAW","SQR"],"page":"TABLE WAVE","curve":null},{"index":9,"key":"table_pulse","symbol":"pulse","label":"PW","name":"Pulse Width","min":5,"max":95,"default":50,"step":0.1,"options":null,"page":"TABLE WAVE","curve":null},{"index":10,"key":"table_detune","symbol":"detune","label":"DET","name":"Wave Detune","min":0,"max":50,"default":0,"step":0.1,"options":null,"page":"TABLE WAVE","curve":null},{"index":11,"key":"table_sub","symbol":"sub","label":"SUB","name":"Wave Sub Level","min":0,"max":100,"default":0,"step":1,"options":null,"page":"TABLE WAVE","curve":null},{"index":12,"key":"table_cutoff","symbol":"cutoff","label":"CUT","name":"Filter Cutoff","min":20,"max":20000,"default":18000,"step":1,"options":null,"page":"TABLE WAVE","curve":"log"},{"index":13,"key":"table_resonance","symbol":"resonance","label":"RES","name":"Filter Resonance","min":0,"max":95,"default":0,"step":1,"options":null,"page":"TABLE WAVE","curve":null},{"index":14,"key":"table_points","symbol":"points","label":"PTS","name":"Spline Points","min":1,"max":32,"default":4,"step":0.1,"options":null,"page":"TABLE SPLINE","curve":null},{"index":15,"key":"table_evolution","symbol":"evolution","label":"EVOL","name":"Spline Evolution","min":1,"max":4096,"default":16,"step":0.1,"options":null,"page":"TABLE SPLINE","curve":"log"},{"index":16,"key":"table_curve","symbol":"curve","label":"CURV","name":"Spline Curvature","min":0,"max":100,"default":100,"step":1,"options":null,"page":"TABLE SPLINE","curve":null},{"index":17,"key":"table_drift","symbol":"drift","label":"DRFT","name":"Spline Pitch Drift","min":0,"max":12,"default":0.15,"step":0.01,"options":null,"page":"TABLE SPLINE","curve":null},{"index":18,"key":"table_drift_rate","symbol":"drift_rate","label":"RATE","name":"Spline Drift Time","min":1,"max":1000,"default":100,"step":1,"options":null,"page":"TABLE SPLINE","curve":"log"},{"index":19,"key":"table_dynamics","symbol":"dynamics","label":"DYN","name":"Spline Dynamics","min":0,"max":100,"default":20,"step":1,"options":null,"page":"TABLE SPLINE","curve":null},{"index":20,"key":"table_seed","symbol":"seed","label":"SEED","name":"Spline Seed","min":0,"max":65535,"default":1,"step":1,"options":null,"page":"TABLE SPLINE","curve":null},{"index":21,"key":"table_model","symbol":"model","label":"MODL","name":"Drum Start","min":0,"max":5,"default":0,"step":1,"options":["KICK","SNARE","TOM","HAT","CLAP","METAL"],"page":"TABLE WT OSC","curve":null},{"index":22,"key":"table_bank_a","symbol":"bank_a","label":"BNKA","name":"Oscillator A Bank","min":0,"max":7,"default":0,"step":1,"options":["SINE","ROUND","SAW","ODD","METAL","FORM","HOLLOW","GLASS"],"page":"TABLE WT OSC","curve":null},{"index":23,"key":"table_pos_a","symbol":"pos_a","label":"POSA","name":"Oscillator A Position","min":0,"max":100,"default":0,"step":1,"options":null,"page":"TABLE WT OSC","curve":null},{"index":24,"key":"table_bank_b","symbol":"bank_b","label":"BNKB","name":"Oscillator B Bank","min":0,"max":7,"default":1,"step":1,"options":["SINE","ROUND","SAW","ODD","METAL","FORM","HOLLOW","GLASS"],"page":"TABLE WT OSC","curve":null},{"index":25,"key":"table_pos_b","symbol":"pos_b","label":"POSB","name":"Oscillator B Position","min":0,"max":100,"default":25,"step":1,"options":null,"page":"TABLE WT OSC","curve":null},{"index":26,"key":"table_blend","symbol":"blend","label":"BLND","name":"Oscillator Blend","min":0,"max":100,"default":15,"step":1,"options":null,"page":"TABLE WT OSC","curve":null},{"index":27,"key":"table_ratio","symbol":"ratio","label":"RATI","name":"Oscillator Ratio","min":0.125,"max":16,"default":1,"step":0.01,"options":null,"page":"TABLE WT OSC","curve":"log"},{"index":28,"key":"table_fm","symbol":"fm","label":"FM","name":"Phase Modulation","min":0,"max":100,"default":0,"step":1,"options":null,"page":"TABLE WT OSC","curve":null},{"index":29,"key":"table_wt_detune","symbol":"wt_detune","label":"DET","name":"Drum Detune","min":0,"max":50,"default":0,"step":0.1,"options":null,"page":"TABLE WT COLOR","curve":null},{"index":30,"key":"table_bend","symbol":"bend","label":"BEND","name":"Pitch Sweep","min":-48,"max":48,"default":24,"step":0.1,"options":null,"page":"TABLE WT HIT","curve":null},{"index":31,"key":"table_pitch_decay","symbol":"pitch_decay","label":"PDEC","name":"Pitch Sweep Time","min":1,"max":2000,"default":30,"step":0.1,"options":null,"page":"TABLE WT HIT","curve":"log"},{"index":32,"key":"table_scan","symbol":"scan","label":"SCAN","name":"Table Sweep","min":-100,"max":100,"default":30,"step":1,"options":null,"page":"TABLE WT HIT","curve":null},{"index":33,"key":"table_scan_decay","symbol":"scan_decay","label":"SDEC","name":"Table Sweep Time","min":1,"max":4000,"default":60,"step":0.1,"options":null,"page":"TABLE WT HIT","curve":"log"},{"index":34,"key":"table_wt_sub","symbol":"wt_sub","label":"SUB","name":"Drum Sub Level","min":0,"max":100,"default":35,"step":1,"options":null,"page":"TABLE WT HIT","curve":null},{"index":35,"key":"table_impact","symbol":"impact","label":"IMPT","name":"Impact Level","min":0,"max":100,"default":25,"step":1,"options":null,"page":"TABLE WT HIT","curve":null},{"index":36,"key":"table_noise_decay","symbol":"noise_decay","label":"NDEC","name":"Noise Decay","min":1,"max":4000,"default":40,"step":0.1,"options":null,"page":"TABLE WT HIT","curve":"log"},{"index":37,"key":"table_color","symbol":"color","label":"COLR","name":"Noise Color","min":0,"max":3,"default":0,"step":1,"options":["WHITE","PINK","HP","METAL"],"page":"TABLE WT HIT","curve":null},{"index":38,"key":"table_tone","symbol":"tone","label":"TONE","name":"Drum Brightness","min":0,"max":100,"default":65,"step":1,"options":null,"page":"TABLE WT COLOR","curve":null},{"index":39,"key":"table_compression","symbol":"compression","label":"COMP","name":"Drum Compression","min":0,"max":100,"default":10,"step":1,"options":null,"page":"TABLE WT COLOR","curve":null}];
Object.assign(PARAMS,{"table_type":{"key":"table_type","label":"TYPE","shortLabel":"TYPE","min":0,"max":2,"value":0,"step":1,"integer":true,"labels":["WAVE","SPLINE","WT DRUM"]},"table_tune":{"key":"table_tune","label":"TUNE","shortLabel":"TUNE","min":-48,"max":48,"value":0,"step":0.1,"integer":false},"table_width":{"key":"table_width","label":"WID","shortLabel":"WID","min":0,"max":100,"value":40,"step":1,"integer":false},"table_level":{"key":"table_level","label":"LVL","shortLabel":"LVL","min":0,"max":100,"value":75,"step":1,"integer":false},"table_art":{"key":"table_art","label":"ART","shortLabel":"ART","min":0,"max":1,"value":0,"step":1,"integer":true,"labels":["TONE","HIT"]},"table_decay":{"key":"table_decay","label":"DEC","shortLabel":"DEC","min":10,"max":12000,"value":650,"step":1,"integer":false,"curve":"log","unit":"ms"},"table_noise":{"key":"table_noise","label":"NOIS","shortLabel":"NOIS","min":0,"max":100,"value":0,"step":1,"integer":false},"table_drive":{"key":"table_drive","label":"DRIV","shortLabel":"DRIV","min":0,"max":100,"value":0,"step":1,"integer":false},"table_shape":{"key":"table_shape","label":"SHAP","shortLabel":"SHAP","min":0,"max":3,"value":0,"step":1,"integer":true,"labels":["SINE","TRI","SAW","SQR"]},"table_pulse":{"key":"table_pulse","label":"PW","shortLabel":"PW","min":5,"max":95,"value":50,"step":0.1,"integer":false},"table_detune":{"key":"table_detune","label":"DET","shortLabel":"DET","min":0,"max":50,"value":0,"step":0.1,"integer":false},"table_sub":{"key":"table_sub","label":"SUB","shortLabel":"SUB","min":0,"max":100,"value":0,"step":1,"integer":false},"table_cutoff":{"key":"table_cutoff","label":"CUT","shortLabel":"CUT","min":20,"max":20000,"value":18000,"step":1,"integer":false,"curve":"log","unit":"Hz"},"table_resonance":{"key":"table_resonance","label":"RES","shortLabel":"RES","min":0,"max":95,"value":0,"step":1,"integer":false},"table_points":{"key":"table_points","label":"PTS","shortLabel":"PTS","min":1,"max":32,"value":4,"step":0.1,"integer":false},"table_evolution":{"key":"table_evolution","label":"EVOL","shortLabel":"EVOL","min":1,"max":4096,"value":16,"step":0.1,"integer":false,"curve":"log"},"table_curve":{"key":"table_curve","label":"CURV","shortLabel":"CURV","min":0,"max":100,"value":100,"step":1,"integer":false},"table_drift":{"key":"table_drift","label":"DRFT","shortLabel":"DRFT","min":0,"max":12,"value":0.15,"step":0.01,"integer":false},"table_drift_rate":{"key":"table_drift_rate","label":"RATE","shortLabel":"RATE","min":1,"max":1000,"value":100,"step":1,"integer":false,"curve":"log","unit":"ms"},"table_dynamics":{"key":"table_dynamics","label":"DYN","shortLabel":"DYN","min":0,"max":100,"value":20,"step":1,"integer":false},"table_seed":{"key":"table_seed","label":"SEED","shortLabel":"SEED","min":0,"max":65535,"value":1,"step":1,"integer":true},"table_model":{"key":"table_model","label":"MODL","shortLabel":"MODL","min":0,"max":5,"value":0,"step":1,"integer":true,"labels":["KICK","SNARE","TOM","HAT","CLAP","METAL"]},"table_bank_a":{"key":"table_bank_a","label":"BNKA","shortLabel":"BNKA","min":0,"max":7,"value":0,"step":1,"integer":true,"labels":["SINE","ROUND","SAW","ODD","METAL","FORM","HOLLOW","GLASS"]},"table_pos_a":{"key":"table_pos_a","label":"POSA","shortLabel":"POSA","min":0,"max":100,"value":0,"step":1,"integer":false},"table_bank_b":{"key":"table_bank_b","label":"BNKB","shortLabel":"BNKB","min":0,"max":7,"value":1,"step":1,"integer":true,"labels":["SINE","ROUND","SAW","ODD","METAL","FORM","HOLLOW","GLASS"]},"table_pos_b":{"key":"table_pos_b","label":"POSB","shortLabel":"POSB","min":0,"max":100,"value":25,"step":1,"integer":false},"table_blend":{"key":"table_blend","label":"BLND","shortLabel":"BLND","min":0,"max":100,"value":15,"step":1,"integer":false},"table_ratio":{"key":"table_ratio","label":"RATI","shortLabel":"RATI","min":0.125,"max":16,"value":1,"step":0.01,"integer":false,"curve":"log"},"table_fm":{"key":"table_fm","label":"FM","shortLabel":"FM","min":0,"max":100,"value":0,"step":1,"integer":false},"table_wt_detune":{"key":"table_wt_detune","label":"DET","shortLabel":"DET","min":0,"max":50,"value":0,"step":0.1,"integer":false},"table_bend":{"key":"table_bend","label":"BEND","shortLabel":"BEND","min":-48,"max":48,"value":24,"step":0.1,"integer":false},"table_pitch_decay":{"key":"table_pitch_decay","label":"PDEC","shortLabel":"PDEC","min":1,"max":2000,"value":30,"step":0.1,"integer":false,"curve":"log","unit":"ms"},"table_scan":{"key":"table_scan","label":"SCAN","shortLabel":"SCAN","min":-100,"max":100,"value":30,"step":1,"integer":false},"table_scan_decay":{"key":"table_scan_decay","label":"SDEC","shortLabel":"SDEC","min":1,"max":4000,"value":60,"step":0.1,"integer":false,"curve":"log","unit":"ms"},"table_wt_sub":{"key":"table_wt_sub","label":"SUB","shortLabel":"SUB","min":0,"max":100,"value":35,"step":1,"integer":false},"table_impact":{"key":"table_impact","label":"IMPT","shortLabel":"IMPT","min":0,"max":100,"value":25,"step":1,"integer":false},"table_noise_decay":{"key":"table_noise_decay","label":"NDEC","shortLabel":"NDEC","min":1,"max":4000,"value":40,"step":0.1,"integer":false,"curve":"log","unit":"ms"},"table_color":{"key":"table_color","label":"COLR","shortLabel":"COLR","min":0,"max":3,"value":0,"step":1,"integer":true,"labels":["WHITE","PINK","HP","METAL"]},"table_tone":{"key":"table_tone","label":"TONE","shortLabel":"TONE","min":0,"max":100,"value":65,"step":1,"integer":false},"table_compression":{"key":"table_compression","label":"COMP","shortLabel":"COMP","min":0,"max":100,"value":10,"step":1,"integer":false}});
const SCANNED_CONTROLS=[{"index":0,"key":"scanned_excitation","label":"EXC","name":"Excitation","min":0,"max":3,"default":0,"step":1,"options":["PLUCK","STRIKE","NOISE","SAMPLE"],"page":"SCANNED","curve":null,"symbol":"excitation"},{"index":1,"key":"scanned_pick","label":"PICK","name":"Excitation Position","min":1,"max":99,"default":24,"step":0.1,"options":null,"page":"SCANNED","curve":null,"symbol":"pick"},{"index":2,"key":"scanned_hit","label":"HIT","name":"Initial Energy","min":0,"max":100,"default":80,"step":1,"options":null,"page":"SCANNED","curve":null,"symbol":"hit"},{"index":3,"key":"scanned_tune","label":"TUNE","name":"Transpose","min":-48,"max":48,"default":0,"step":0.1,"options":null,"page":"SCANNED","curve":null,"symbol":"tune"},{"index":4,"key":"scanned_motion","label":"MOTN","name":"Mechanical Motion","min":0.05,"max":6,"default":1.2,"step":0.01,"options":null,"page":"SCANNED","curve":"log","symbol":"motion"},{"index":5,"key":"scanned_damping","label":"DAMP","name":"Mechanical Damping","min":0,"max":100,"default":12,"step":1,"options":null,"page":"SCANNED","curve":null,"symbol":"damping"},{"index":6,"key":"scanned_brightness","label":"BRGT","name":"Readout Brightness","min":0,"max":100,"default":75,"step":1,"options":null,"page":"SCANNED","curve":null,"symbol":"brightness"},{"index":7,"key":"scanned_width","label":"WID","name":"Stereo Width","min":0,"max":100,"default":30,"step":1,"options":null,"page":"SCANNED","curve":null,"symbol":"width"},{"index":8,"key":"scanned_mass","label":"MASS","name":"Mass Variation","min":0,"max":100,"default":30,"step":1,"options":null,"page":"SCAN BODY","curve":null,"symbol":"mass"},{"index":9,"key":"scanned_tension","label":"TENS","name":"Spring Variation","min":0,"max":100,"default":25,"step":1,"options":null,"page":"SCAN BODY","curve":null,"symbol":"tension"},{"index":10,"key":"scanned_center","label":"CENT","name":"Centering Springs","min":0,"max":100,"default":15,"step":1,"options":null,"page":"SCAN BODY","curve":null,"symbol":"center"},{"index":11,"key":"scanned_profile","label":"PROF","name":"Physical Profile","min":0,"max":3,"default":1,"step":1,"options":["EVEN","TAPER","WAVE","RANDOM"],"page":"SCAN BODY","curve":null,"symbol":"profile"},{"index":12,"key":"scanned_force","label":"FORC","name":"Continuous Force","min":0,"max":100,"default":0,"step":1,"options":null,"page":"SCAN BODY","curve":null,"symbol":"force"},{"index":13,"key":"scanned_pressure","label":"PRES","name":"Pressure Force Depth","min":0,"max":100,"default":20,"step":1,"options":null,"page":"SCAN BODY","curve":null,"symbol":"pressure"},{"index":14,"key":"scanned_freeze","label":"FRZ","name":"Freeze Motion","min":0,"max":1,"default":0,"step":1,"options":["OFF","ON"],"page":"SCAN BODY","curve":null,"symbol":"freeze"},{"index":15,"key":"scanned_seed","label":"SEED","name":"Profile Seed","min":0,"max":65535,"default":1,"step":1,"options":null,"page":"SCAN BODY","curve":null,"symbol":"seed"},{"index":16,"key":"scanned_art","label":"ART","name":"Articulation","min":0,"max":1,"default":0,"step":1,"options":["TONE","PERC"],"page":"SCAN HIT","curve":null,"symbol":"art"},{"index":17,"key":"scanned_impact","label":"IMPT","name":"Impact Level","min":0,"max":100,"default":35,"step":1,"options":null,"page":"SCAN HIT","curve":null,"symbol":"impact"},{"index":18,"key":"scanned_hardness","label":"HARD","name":"Hammer Hardness","min":0,"max":100,"default":60,"step":1,"options":null,"page":"SCAN HIT","curve":null,"symbol":"hardness"},{"index":19,"key":"scanned_decay","label":"DEC","name":"Percussion Decay","min":10,"max":4000,"default":350,"step":1,"options":null,"page":"SCAN HIT","curve":"log","symbol":"decay"},{"index":20,"key":"scanned_bend","label":"BEND","name":"Pitch Sweep","min":-48,"max":48,"default":0,"step":0.1,"options":null,"page":"SCAN HIT","curve":null,"symbol":"bend"},{"index":21,"key":"scanned_sweep_time","label":"TIME","name":"Pitch Sweep Time","min":5,"max":2000,"default":35,"step":1,"options":null,"page":"SCAN HIT","curve":"log","symbol":"sweep_time"},{"index":22,"key":"scanned_loss","label":"LOSS","name":"Harmonic Decay","min":0,"max":100,"default":50,"step":1,"options":null,"page":"SCAN HIT","curve":null,"symbol":"loss"},{"index":23,"key":"scanned_amp_mode","label":"AMP","name":"Amplifier","min":0,"max":2,"default":0,"step":1,"options":["OFF","TUBE","FUZZ"],"page":"SCAN AMP","curve":null,"symbol":"amp_mode"},{"index":24,"key":"scanned_amp_gain","label":"GAIN","name":"Amp Gain","min":0,"max":100,"default":35,"step":1,"options":null,"page":"SCAN AMP","curve":null,"symbol":"amp_gain"},{"index":25,"key":"scanned_amp_bass","label":"BASS","name":"Amp Bass","min":-12,"max":12,"default":0,"step":0.1,"options":null,"page":"SCAN AMP","curve":null,"symbol":"amp_bass"},{"index":26,"key":"scanned_amp_mid","label":"MID","name":"Amp Midrange","min":-20,"max":10,"default":0,"step":0.1,"options":null,"page":"SCAN AMP","curve":null,"symbol":"amp_mid"},{"index":27,"key":"scanned_amp_freq","label":"FREQ","name":"Amp Mid Frequency","min":0,"max":4,"default":2,"step":1,"options":["220","450","800","1.6k","3k"],"page":"SCAN AMP","curve":null,"symbol":"amp_freq"},{"index":28,"key":"scanned_amp_treble","label":"TREB","name":"Amp Treble","min":-20,"max":15,"default":0,"step":0.1,"options":null,"page":"SCAN AMP","curve":null,"symbol":"amp_treble"},{"index":29,"key":"scanned_amp_cab","label":"CAB","name":"Cabinet Amount","min":0,"max":100,"default":70,"step":1,"options":null,"page":"SCAN AMP","curve":null,"symbol":"amp_cab"},{"index":30,"key":"scanned_amp_level","label":"OUT","name":"Amp Output","min":-18,"max":6,"default":0,"step":0.1,"options":null,"page":"SCAN AMP","curve":null,"symbol":"amp_level"}];
Object.assign(PARAMS,{"scanned_excitation":{"key":"scanned_excitation","label":"EXC","shortLabel":"EXC","min":0,"max":3,"value":0,"step":1,"integer":true,"labels":["PLUCK","STRIKE","NOISE","SAMPLE"]},"scanned_pick":{"key":"scanned_pick","label":"PICK","shortLabel":"PICK","min":1,"max":99,"value":24,"step":0.1,"integer":false},"scanned_hit":{"key":"scanned_hit","label":"HIT","shortLabel":"HIT","min":0,"max":100,"value":80,"step":1,"integer":false},"scanned_tune":{"key":"scanned_tune","label":"TUNE","shortLabel":"TUNE","min":-48,"max":48,"value":0,"step":0.1,"integer":false},"scanned_motion":{"key":"scanned_motion","label":"MOTN","shortLabel":"MOTN","min":0.05,"max":6,"value":1.2,"step":0.01,"integer":false,"curve":"log"},"scanned_damping":{"key":"scanned_damping","label":"DAMP","shortLabel":"DAMP","min":0,"max":100,"value":12,"step":1,"integer":false},"scanned_brightness":{"key":"scanned_brightness","label":"BRGT","shortLabel":"BRGT","min":0,"max":100,"value":75,"step":1,"integer":false},"scanned_width":{"key":"scanned_width","label":"WID","shortLabel":"WID","min":0,"max":100,"value":30,"step":1,"integer":false},"scanned_mass":{"key":"scanned_mass","label":"MASS","shortLabel":"MASS","min":0,"max":100,"value":30,"step":1,"integer":false},"scanned_tension":{"key":"scanned_tension","label":"TENS","shortLabel":"TENS","min":0,"max":100,"value":25,"step":1,"integer":false},"scanned_center":{"key":"scanned_center","label":"CENT","shortLabel":"CENT","min":0,"max":100,"value":15,"step":1,"integer":false},"scanned_profile":{"key":"scanned_profile","label":"PROF","shortLabel":"PROF","min":0,"max":3,"value":1,"step":1,"integer":true,"labels":["EVEN","TAPER","WAVE","RANDOM"]},"scanned_force":{"key":"scanned_force","label":"FORC","shortLabel":"FORC","min":0,"max":100,"value":0,"step":1,"integer":false},"scanned_pressure":{"key":"scanned_pressure","label":"PRES","shortLabel":"PRES","min":0,"max":100,"value":20,"step":1,"integer":false},"scanned_freeze":{"key":"scanned_freeze","label":"FRZ","shortLabel":"FRZ","min":0,"max":1,"value":0,"step":1,"integer":true,"labels":["OFF","ON"]},"scanned_seed":{"key":"scanned_seed","label":"SEED","shortLabel":"SEED","min":0,"max":65535,"value":1,"step":1,"integer":true},"scanned_art":{"key":"scanned_art","label":"ART","shortLabel":"ART","min":0,"max":1,"value":0,"step":1,"integer":true,"labels":["TONE","PERC"]},"scanned_impact":{"key":"scanned_impact","label":"IMPT","shortLabel":"IMPT","min":0,"max":100,"value":35,"step":1,"integer":false},"scanned_hardness":{"key":"scanned_hardness","label":"HARD","shortLabel":"HARD","min":0,"max":100,"value":60,"step":1,"integer":false},"scanned_decay":{"key":"scanned_decay","label":"DEC","shortLabel":"DEC","min":10,"max":4000,"value":350,"step":1,"integer":false,"curve":"log","unit":"ms"},"scanned_bend":{"key":"scanned_bend","label":"BEND","shortLabel":"BEND","min":-48,"max":48,"value":0,"step":0.1,"integer":false},"scanned_sweep_time":{"key":"scanned_sweep_time","label":"TIME","shortLabel":"TIME","min":5,"max":2000,"value":35,"step":1,"integer":false,"curve":"log","unit":"ms"},"scanned_loss":{"key":"scanned_loss","label":"LOSS","shortLabel":"LOSS","min":0,"max":100,"value":50,"step":1,"integer":false},"scanned_amp_mode":{"key":"scanned_amp_mode","label":"AMP","shortLabel":"AMP","min":0,"max":2,"value":0,"step":1,"integer":true,"labels":["OFF","TUBE","FUZZ"]},"scanned_amp_gain":{"key":"scanned_amp_gain","label":"GAIN","shortLabel":"GAIN","min":0,"max":100,"value":35,"step":1,"integer":false},"scanned_amp_bass":{"key":"scanned_amp_bass","label":"BASS","shortLabel":"BASS","min":-12,"max":12,"value":0,"step":0.1,"integer":false},"scanned_amp_mid":{"key":"scanned_amp_mid","label":"MID","shortLabel":"MID","min":-20,"max":10,"value":0,"step":0.1,"integer":false},"scanned_amp_freq":{"key":"scanned_amp_freq","label":"FREQ","shortLabel":"FREQ","min":0,"max":4,"value":2,"step":1,"integer":true,"labels":["220","450","800","1.6k","3k"]},"scanned_amp_treble":{"key":"scanned_amp_treble","label":"TREB","shortLabel":"TREB","min":-20,"max":15,"value":0,"step":0.1,"integer":false},"scanned_amp_cab":{"key":"scanned_amp_cab","label":"CAB","shortLabel":"CAB","min":0,"max":100,"value":70,"step":1,"integer":false},"scanned_amp_level":{"key":"scanned_amp_level","label":"OUT","shortLabel":"OUT","min":-18,"max":6,"value":0,"step":0.1,"integer":false}});
const SCANNED_PAGE={title:"SCANNED",scanned:true,keys:["scanned_excitation", "scanned_pick", "scanned_hit", "scanned_tune", "scanned_motion", "scanned_damping", "scanned_brightness", "scanned_width"]};
const SCANNED_BODY_PAGE={title:"SCAN BODY",scanned:true,keys:["scanned_mass", "scanned_tension", "scanned_center", "scanned_profile", "scanned_force", "scanned_pressure", "scanned_freeze", "scanned_seed"]};
const SCANNED_HIT_PAGE={title:"SCAN HIT",scanned:true,keys:["scanned_art", "scanned_impact", "scanned_hardness", "scanned_decay", "scanned_bend", "scanned_sweep_time", "scanned_loss", null]};
const SCANNED_AMP_PAGE={title:"SCAN AMP",scanned:true,keys:["scanned_amp_mode", "scanned_amp_gain", "scanned_amp_bass", "scanned_amp_mid", "scanned_amp_freq", "scanned_amp_treble", "scanned_amp_cab", "scanned_amp_level"]};
const scannedUi={data:null,floor:0};
function readScanned(cached=uiPolling.cached){let data;try{data=JSON.parse(String(cached?uiPolling.cache.get('scanned_state'):getParam('scanned_state','{}')));}catch(_){return;}
 if(!data||data.part!==(state.splitEnabled?state.splitFocus:0)||!Number.isInteger(data.edit)||data.edit<0||data.edit>3||!Array.isArray(data.patches)||data.patches.length!==4||data.patches.some(p=>!Array.isArray(p)||p.length!==31||p.some(x=>!Number.isFinite(x)))||data.revision<scannedUi.floor)return;
 const expected=state.splitEnabled&&!state.sharedDual?(isQuad()?state.splitFocus:Math.floor(clamp(state.lane-1,state.splitFocus*2,state.splitFocus*2+1))):state.lane-1;
 if(data.edit!==expected)return;
 scannedUi.data=data;if(!cached)scannedUi.floor=data.revision;
 SCANNED_CONTROLS.forEach(p=>PARAMS[p.key].value=data.patches[data.edit][p.index]);
}

const FM_CONTROLS=[{"index":0,"key":"fm_mode","symbol":"mode","label":"MODE","name":"FM Articulation","min":0,"max":1,"default":0,"step":1,"options":["TONE","DRUM"],"curve":null,"page":"FM"},{"index":1,"key":"fm_tune","symbol":"tune","label":"TUNE","name":"FM Transpose","min":-48,"max":48,"default":0,"step":0.1,"options":null,"curve":null,"page":"FM"},{"index":2,"key":"fm_width","symbol":"width","label":"WID","name":"FM Stereo Width","min":0,"max":100,"default":35,"step":1,"options":null,"curve":null,"page":"FM"},{"index":3,"key":"fm_level","symbol":"level","label":"LVL","name":"FM Level","min":0,"max":100,"default":75,"step":1,"options":null,"curve":null,"page":"FM"},{"index":4,"key":"fm_algorithm","symbol":"algorithm","label":"ALG","name":"FM Routing","min":0,"max":3,"default":0,"step":1,"options":["2OP","SER","PAR","RING"],"curve":null,"page":"FM"},{"index":5,"key":"fm_ratio1","symbol":"ratio1","label":"RAT1","name":"Modulator 1 Ratio","min":0.125,"max":32,"default":1,"step":0.125,"options":null,"curve":null,"page":"FM"},{"index":6,"key":"fm_index1","symbol":"index1","label":"IDX1","name":"Modulator 1 Index","min":0,"max":12,"default":1,"step":0.05,"options":null,"curve":null,"page":"FM"},{"index":7,"key":"fm_feedback","symbol":"feedback","label":"FDBK","name":"FM Feedback","min":0,"max":90,"default":0,"step":1,"options":null,"curve":null,"page":"FM"},{"index":8,"key":"fm_ratio2","symbol":"ratio2","label":"RAT2","name":"Modulator 2 Ratio","min":0.125,"max":32,"default":2,"step":0.125,"options":null,"curve":null,"page":"FM COLOR"},{"index":9,"key":"fm_index2","symbol":"index2","label":"IDX2","name":"Modulator 2 Index","min":0,"max":12,"default":2,"step":0.05,"options":null,"curve":null,"page":"FM COLOR"},{"index":10,"key":"fm_detune","symbol":"detune","label":"DETN","name":"FM Stereo Detune","min":0,"max":30,"default":0,"step":0.1,"options":null,"curve":null,"page":"FM COLOR"},{"index":11,"key":"fm_mix","symbol":"mix","label":"MIX","name":"FM Wet Mix","min":0,"max":100,"default":100,"step":1,"options":null,"curve":null,"page":"FM COLOR"},{"index":12,"key":"fm_cutoff","symbol":"cutoff","label":"CUTO","name":"FM Filter Cutoff","min":20,"max":20000,"default":18000,"step":1,"options":null,"curve":"log","page":"FM COLOR"},{"index":13,"key":"fm_resonance","symbol":"resonance","label":"RES","name":"FM Filter Resonance","min":0,"max":85,"default":0,"step":1,"options":null,"curve":null,"page":"FM COLOR"},{"index":14,"key":"fm_drive","symbol":"drive","label":"DRIV","name":"FM Drive","min":0,"max":100,"default":0,"step":1,"options":null,"curve":null,"page":"FM COLOR"},{"index":15,"key":"fm_decay","symbol":"decay","label":"DEC","name":"FM Drum Decay","min":10,"max":8000,"default":600,"step":1,"options":null,"curve":"log","page":"FM HIT"},{"index":16,"key":"fm_fm_decay","symbol":"fm_decay","label":"FDEC","name":"FM Index Decay","min":2,"max":4000,"default":120,"step":1,"options":null,"curve":"log","page":"FM HIT"},{"index":17,"key":"fm_bend","symbol":"bend","label":"BEND","name":"FM Pitch Sweep","min":-48,"max":48,"default":24,"step":0.1,"options":null,"curve":null,"page":"FM HIT"},{"index":18,"key":"fm_sweep_time","symbol":"sweep_time","label":"TIME","name":"FM Pitch Sweep Time","min":1,"max":2000,"default":25,"step":1,"options":null,"curve":"log","page":"FM HIT"},{"index":19,"key":"fm_impact","symbol":"impact","label":"HIT","name":"FM Impact","min":0,"max":100,"default":20,"step":1,"options":null,"curve":null,"page":"FM HIT"},{"index":20,"key":"fm_noise","symbol":"noise","label":"NOIS","name":"FM Noise Body","min":0,"max":100,"default":0,"step":1,"options":null,"curve":null,"page":"FM HIT"},{"index":21,"key":"fm_noise_decay","symbol":"noise_decay","label":"NDEC","name":"FM Noise Decay","min":1,"max":4000,"default":70,"step":1,"options":null,"curve":"log","page":"FM HIT"},{"index":22,"key":"fm_hardness","symbol":"hardness","label":"HARD","name":"FM Impact Hardness","min":0,"max":100,"default":60,"step":1,"options":null,"curve":null,"page":"FM HIT"},{"index":23,"key":"fm_harm","symbol":"harm","label":"HARM","name":"Operator Harmonics","min":-100,"max":100,"default":0,"step":1,"options":null,"curve":null,"page":"FM COLOR"},{"index":24,"key":"fm_attack1","symbol":"attack1","label":"ATK1","name":"Modulator 1 Attack","min":0,"max":4000,"default":0,"step":1,"options":null,"curve":null,"page":"FM ENVS"},{"index":25,"key":"fm_decay1","symbol":"decay1","label":"DEC1","name":"Modulator 1 Decay","min":0,"max":8000,"default":750,"step":1,"options":null,"curve":null,"page":"FM ENVS"},{"index":26,"key":"fm_end1","symbol":"end1","label":"END1","name":"Modulator 1 End Level","min":-1,"max":100,"default":35,"step":1,"options":null,"curve":null,"page":"FM ENVS"},{"index":27,"key":"fm_curve1","symbol":"curve1","label":"CRV1","name":"Modulator 1 Curve","min":0,"max":2,"default":1,"step":1,"options":["LIN","EXP","LOG"],"curve":null,"page":"FM ENVS"},{"index":28,"key":"fm_attack2","symbol":"attack2","label":"ATK2","name":"Modulator 2 Attack","min":0,"max":4000,"default":0,"step":1,"options":null,"curve":null,"page":"FM ENVS"},{"index":29,"key":"fm_decay2","symbol":"decay2","label":"DEC2","name":"Modulator 2 Decay","min":0,"max":8000,"default":1200,"step":1,"options":null,"curve":null,"page":"FM ENVS"},{"index":30,"key":"fm_end2","symbol":"end2","label":"END2","name":"Modulator 2 End Level","min":-1,"max":100,"default":20,"step":1,"options":null,"curve":null,"page":"FM ENVS"},{"index":31,"key":"fm_curve2","symbol":"curve2","label":"CRV2","name":"Modulator 2 Curve","min":0,"max":2,"default":1,"step":1,"options":["LIN","EXP","LOG"],"curve":null,"page":"FM ENVS"}];
Object.assign(PARAMS,{"fm_mode":{"key":"fm_mode","label":"MODE","shortLabel":"MODE","min":0,"max":1,"value":0,"step":1,"integer":true,"labels":["TONE","DRUM"]},"fm_tune":{"key":"fm_tune","label":"TUNE","shortLabel":"TUNE","min":-48,"max":48,"value":0,"step":0.1,"integer":false},"fm_width":{"key":"fm_width","label":"WID","shortLabel":"WID","min":0,"max":100,"value":35,"step":1,"integer":false},"fm_level":{"key":"fm_level","label":"LVL","shortLabel":"LVL","min":0,"max":100,"value":75,"step":1,"integer":false},"fm_algorithm":{"key":"fm_algorithm","label":"ALG","shortLabel":"ALG","min":0,"max":3,"value":0,"step":1,"integer":true,"labels":["2OP","SER","PAR","RING"]},"fm_ratio1":{"key":"fm_ratio1","label":"RAT1","shortLabel":"RAT1","min":0.125,"max":32,"value":1,"step":0.125,"integer":false},"fm_index1":{"key":"fm_index1","label":"IDX1","shortLabel":"IDX1","min":0,"max":12,"value":1,"step":0.05,"integer":false},"fm_feedback":{"key":"fm_feedback","label":"FDBK","shortLabel":"FDBK","min":0,"max":90,"value":0,"step":1,"integer":false},"fm_ratio2":{"key":"fm_ratio2","label":"RAT2","shortLabel":"RAT2","min":0.125,"max":32,"value":2,"step":0.125,"integer":false},"fm_index2":{"key":"fm_index2","label":"IDX2","shortLabel":"IDX2","min":0,"max":12,"value":2,"step":0.05,"integer":false},"fm_detune":{"key":"fm_detune","label":"DETN","shortLabel":"DETN","min":0,"max":30,"value":0,"step":0.1,"integer":false},"fm_mix":{"key":"fm_mix","label":"MIX","shortLabel":"MIX","min":0,"max":100,"value":100,"step":1,"integer":false},"fm_cutoff":{"key":"fm_cutoff","label":"CUTO","shortLabel":"CUTO","min":20,"max":20000,"value":18000,"step":1,"integer":false,"curve":"log"},"fm_resonance":{"key":"fm_resonance","label":"RES","shortLabel":"RES","min":0,"max":85,"value":0,"step":1,"integer":false},"fm_drive":{"key":"fm_drive","label":"DRIV","shortLabel":"DRIV","min":0,"max":100,"value":0,"step":1,"integer":false},"fm_decay":{"key":"fm_decay","label":"DEC","shortLabel":"DEC","min":10,"max":8000,"value":600,"step":1,"integer":false,"curve":"log","unit":"ms"},"fm_fm_decay":{"key":"fm_fm_decay","label":"FDEC","shortLabel":"FDEC","min":2,"max":4000,"value":120,"step":1,"integer":false,"curve":"log","unit":"ms"},"fm_bend":{"key":"fm_bend","label":"BEND","shortLabel":"BEND","min":-48,"max":48,"value":24,"step":0.1,"integer":false},"fm_sweep_time":{"key":"fm_sweep_time","label":"TIME","shortLabel":"TIME","min":1,"max":2000,"value":25,"step":1,"integer":false,"curve":"log","unit":"ms"},"fm_impact":{"key":"fm_impact","label":"HIT","shortLabel":"HIT","min":0,"max":100,"value":20,"step":1,"integer":false},"fm_noise":{"key":"fm_noise","label":"NOIS","shortLabel":"NOIS","min":0,"max":100,"value":0,"step":1,"integer":false},"fm_noise_decay":{"key":"fm_noise_decay","label":"NDEC","shortLabel":"NDEC","min":1,"max":4000,"value":70,"step":1,"integer":false,"curve":"log","unit":"ms"},"fm_hardness":{"key":"fm_hardness","label":"HARD","shortLabel":"HARD","min":0,"max":100,"value":60,"step":1,"integer":false},"fm_harm":{"key":"fm_harm","label":"HARM","shortLabel":"HARM","min":-100,"max":100,"value":0,"step":1,"integer":false},"fm_attack1":{"key":"fm_attack1","label":"ATK1","shortLabel":"ATK1","min":0,"max":4000,"value":0,"step":1,"integer":false,"unit":"ms"},"fm_decay1":{"key":"fm_decay1","label":"DEC1","shortLabel":"DEC1","min":0,"max":8000,"value":750,"step":1,"integer":false,"unit":"ms"},"fm_end1":{"key":"fm_end1","label":"END1","shortLabel":"END1","min":-1,"max":100,"value":35,"step":1,"integer":false},"fm_curve1":{"key":"fm_curve1","label":"CRV1","shortLabel":"CRV1","min":0,"max":2,"value":1,"step":1,"integer":true,"labels":["LIN","EXP","LOG"]},"fm_attack2":{"key":"fm_attack2","label":"ATK2","shortLabel":"ATK2","min":0,"max":4000,"value":0,"step":1,"integer":false,"unit":"ms"},"fm_decay2":{"key":"fm_decay2","label":"DEC2","shortLabel":"DEC2","min":0,"max":8000,"value":1200,"step":1,"integer":false,"unit":"ms"},"fm_end2":{"key":"fm_end2","label":"END2","shortLabel":"END2","min":-1,"max":100,"value":20,"step":1,"integer":false},"fm_curve2":{"key":"fm_curve2","label":"CRV2","shortLabel":"CRV2","min":0,"max":2,"value":1,"step":1,"integer":true,"labels":["LIN","EXP","LOG"]}});
const FM_PAGE={title:"FM",fm:true,keys:["fm_mode", "fm_tune", "fm_width", "fm_level", "fm_algorithm", "fm_ratio1", "fm_index1", "fm_feedback"]};
const FM_COLOR_PAGE={title:"FM COLOR",fm:true,keys:["fm_ratio2", "fm_index2", "fm_detune", "fm_mix", "fm_cutoff", "fm_resonance", "fm_drive", "fm_harm"]};
const FM_HIT_PAGE={title:"FM HIT",fm:true,keys:["fm_decay", "fm_fm_decay", "fm_bend", "fm_sweep_time", "fm_impact", "fm_noise", "fm_noise_decay", "fm_hardness"]};
const FM_ENVS_PAGE={title:"FM ENVS",fm:true,keys:["fm_attack1", "fm_decay1", "fm_end1", "fm_curve1", "fm_attack2", "fm_decay2", "fm_end2", "fm_curve2"]};
const fmUi={data:null,floor:0,routingTouch:false,startsOpen:false,start:0};
function readFm(cached=uiPolling.cached){let data;try{data=JSON.parse(String(cached?uiPolling.cache.get('fm_state'):getParam('fm_state','{}')));}catch(_){return;}
 if(!data||data.part!==(state.splitEnabled?state.splitFocus:0)||!Number.isInteger(data.edit)||data.edit<0||data.edit>3||!Array.isArray(data.patches)||data.patches.length!==4||data.patches.some(p=>!Array.isArray(p)||p.length!==32||p.some(x=>!Number.isFinite(x)))||data.revision<fmUi.floor)return;
 const expected=state.splitEnabled&&!state.sharedDual?(isQuad()?state.splitFocus:Math.floor(clamp(state.lane-1,state.splitFocus*2,state.splitFocus*2+1))):state.lane-1;
 if(data.edit!==expected)return;
 fmUi.data=data;if(!cached)fmUi.floor=data.revision;
 FM_CONTROLS.forEach(p=>PARAMS[p.key].value=data.patches[data.edit][p.index]);
}

const FM_STARTS=['BASS','BELL','KEYS','PAD','KICK','SNARE','TOM','METAL','HAT'];
function fmPageActive(){return state.view==='settings'&&Boolean(currentSettingsPage().fm)&&!state.performanceVisible;}
function handleFmMidi(status,number,value){
 if((status===0x90||status===0x80)&&number===4){
   const pressed=status===0x90&&value>0;
   if(!pressed){const shown=fmUi.routingTouch;fmUi.routingTouch=false;if(shown){draw();return true;}return false;}
   if(fmPageActive()&&currentSettingsPage()===FM_PAGE&&!fmUi.startsOpen){fmUi.routingTouch=true;draw();return true;}
 }
 if(status!==0xb0)return false;
 if(fmUi.startsOpen&&!fmPageActive())fmUi.startsOpen=false;
 if(!fmUi.startsOpen){
   if(number===MoveMainButton&&value>0&&fmPageActive()&&!state.shiftHeld&&!state.focusButtonHeld){
     if(sequenceSoundActive()){showActionMessage('LANE SETTING');draw();return true;}
     fmUi.startsOpen=true;fmUi.routingTouch=false;fmUi.start=0;draw();return true;
   }return false;
 }
 if(number===MoveMainKnob){fmUi.start=clamp(fmUi.start+decodeDelta(value),0,FM_STARTS.length-1);draw();return true;}
 if([MoveUp,MoveDown,MoveLeft,MoveRight].includes(number)){if(value>0){fmUi.start=clamp(fmUi.start+([MoveUp,MoveLeft].includes(number)?-1:1),0,FM_STARTS.length-1);draw();}return true;}
 if(number>=MoveKnob1&&number<MoveKnob1+8)return true;
 if(number===MoveBack||number===MoveMenu){if(value>0){fmUi.startsOpen=false;draw();}return true;}
 if(number===MoveMainButton){if(value>0){setParamBlocking('fm_start',fmUi.start+1);fmUi.startsOpen=false;readFm(false);draw();}return true;}
 return false;
}
function drawFmStarts(){
 clearDisplay();drawSettingsTitle('FM START L'+((fmUi.data?.edit??state.lane-1)+1));drawRect(0,8,128,1,1);
 const start=clamp(fmUi.start-1,0,FM_STARTS.length-4);
 for(let row=0;row<4;++row){const i=start+row,y=13+row*10;if(i===fmUi.start)drawRect(0,y-1,128,9,1);print(3,y,FM_STARTS[i],i===fmUi.start?0:1);}
 print(1,57,'CLICK LOAD BACK CANCEL',1);
}
function drawFmRouting(){
 clearDisplay();const alg=clamp(Math.round(controlForKey('fm_algorithm')?.value??0),0,3);
 drawSettingsTitle('FM '+['2OP','SER','PAR','RING'][alg]);drawRect(0,8,128,1,1);
 const i1=Number(controlForKey('fm_index1')?.value||0),i2=Number(controlForKey('fm_index2')?.value||0),wet=Number(controlForKey('fm_mix')?.value||0);
 const box=(x,y,n,carrier=false,unused=false)=>{drawRect(x,y,14,12,1);if(!carrier)drawRect(x+1,y+1,12,10,0);print(x+4,y+2,String(n),carrier?0:1);if(unused){drawRect(x,y,2,2,0);drawRect(x+12,y+10,2,2,0);}};
 const line=(x0,y0,x1,y1,active=true)=>{if(active){if(y0===y1)drawRect(Math.min(x0,x1),y0,Math.abs(x1-x0)+1,1,1);else if(x0===x1)drawRect(x0,Math.min(y0,y1),1,Math.abs(y1-y0)+1,1);else drawPixelLine(x0,y0,x1,y1);}else{const n=Math.max(Math.abs(x1-x0),Math.abs(y1-y0));for(let i=0;i<=n;i+=3)drawRect(Math.round(x0+(x1-x0)*i/Math.max(1,n)),Math.round(y0+(y1-y0)*i/Math.max(1,n)),1,1,1);}};
 const arrow=(x0,y0,x1,y1,on=true)=>{line(x0,y0,x1,y1,on);const dx=Math.sign(x1-x0),dy=Math.sign(y1-y0);if(dy===0){line(x1-3*dx,y1-2,x1,y1,on);line(x1-3*dx,y1+2,x1,y1,on);}else{line(x1-2,y1-3*dy,x1,y1,on);line(x1+2,y1-3*dy,x1,y1,on);}};
 const feedback=(x,y)=>{const on=Number(controlForKey('fm_feedback')?.value||0)>0&&i1>0&&wet>0;line(x+7,y,x+7,y-6,on);line(x+7,y-6,x-5,y-6,on);line(x-5,y-6,x-5,y+6,on);arrow(x-5,y+6,x,y+6,on);};
 if(alg===0){box(24,28,2);box(69,28,1,true);arrow(38,34,69,34,i1>0&&wet>0);feedback(24,28);box(3,45,3,false,true);print(20,47,'UNUSED',1);arrow(83,34,109,34);print(110,31,'O',1);}
 if(alg===1){box(5,28,3);box(48,28,2);box(91,28,1,true);arrow(19,34,48,34,i2>0&&wet>0);arrow(62,34,91,34,i1>0&&wet>0);feedback(48,28);arrow(105,34,121,34);}
 if(alg===2){box(24,17,2);box(24,41,3);box(83,29,1,true);line(38,23,63,23,i1>0&&wet>0);line(63,23,63,35,i1>0&&wet>0);line(38,47,63,47,i2>0&&wet>0);line(63,47,63,35,i2>0&&wet>0);arrow(63,35,83,35,(i1>0||i2>0)&&wet>0);feedback(24,17);arrow(97,35,122,35);}
 if(alg===3){box(15,20,2);box(53,20,1,true);box(53,43,3);arrow(29,26,53,26,i1>0&&wet>0);feedback(15,20);drawRect(94,20,13,12,1);drawRect(95,21,11,10,0);print(98,22,'X',1);arrow(67,26,94,26);line(67,49,100,49,i2>0&&wet>0);arrow(100,49,100,32,i2>0&&wet>0);arrow(107,26,123,26);}
 if(alg!==3)print(1,57,'2:RAT1 3:RAT2',1);
}

const NOISE_CONTROLS=[{"index":0,"key":"noise_type","symbol":"type","label":"TYPE","name":"Noise Type","min":0,"max":6,"default":0,"step":1,"options":["BANK","DUST","CRKL","STAT","CLIK","CUTS","DATA"],"unit":null,"curve":null,"page":"NOISE"},{"index":1,"key":"noise_tune","symbol":"tune","label":"TUNE","name":"Noise Transpose","min":-48,"max":48,"default":0,"step":0.1,"options":null,"unit":"st","curve":null,"page":"NOISE"},{"index":2,"key":"noise_width","symbol":"width","label":"WID","name":"Noise Stereo Width","min":0,"max":100,"default":50,"step":1,"options":null,"unit":"%","curve":null,"page":"NOISE"},{"index":3,"key":"noise_level","symbol":"level","label":"LVL","name":"Noise Level","min":0,"max":100,"default":75,"step":1,"options":null,"unit":"%","curve":null,"page":"NOISE"},{"index":4,"key":"noise_color","symbol":"color","label":"COLR","name":"Noise Color","min":0,"max":3,"default":0,"step":1,"options":["WHT","PINK","BRWN","BLUE"],"unit":null,"curve":null,"page":"NOISE"},{"index":5,"key":"noise_tone","symbol":"tone","label":"TONE","name":"Noise Low Pass","min":20,"max":20000,"default":16000,"step":1,"options":null,"unit":"Hz","curve":"log","page":"NOISE"},{"index":6,"key":"noise_bits","symbol":"bits","label":"BITS","name":"Noise Resolution","min":2,"max":24,"default":24,"step":0.1,"options":null,"unit":null,"curve":null,"page":"NOISE"},{"index":7,"key":"noise_seed","symbol":"seed","label":"SEED","name":"Noise Seed","min":0,"max":65535,"default":1,"step":1,"options":null,"unit":null,"curve":null,"page":"NOISE"},{"index":8,"key":"noise_count","symbol":"count","label":"BAND","name":"Band Count","min":1,"max":32,"default":8,"step":0.1,"options":null,"unit":null,"curve":null,"page":"NOISE BANK"},{"index":9,"key":"noise_bandwidth","symbol":"bandwidth","label":"BW","name":"Band Width","min":0.1,"max":48,"default":2,"step":0.1,"options":null,"unit":null,"curve":null,"page":"NOISE BANK"},{"index":10,"key":"noise_tilt","symbol":"tilt","label":"TILT","name":"Band Tilt","min":-24,"max":12,"default":0,"step":0.5,"options":null,"unit":null,"curve":null,"page":"NOISE BANK"},{"index":11,"key":"noise_stretch","symbol":"stretch","label":"STR","name":"Band Stretch","min":0.25,"max":4,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANK"},{"index":12,"key":"noise_motion","symbol":"motion","label":"MOVE","name":"Band Motion","min":0,"max":24,"default":0,"step":0.1,"options":null,"unit":null,"curve":null,"page":"NOISE BANK"},{"index":13,"key":"noise_motion_rate","symbol":"motion_rate","label":"RATE","name":"Band Motion Rate","min":0.01,"max":20,"default":0.2,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANK"},{"index":14,"key":"noise_leak","symbol":"leak","label":"LEAK","name":"Unfiltered Noise","min":0,"max":100,"default":0,"step":1,"options":null,"unit":null,"curve":null,"page":"NOISE BANK"},{"index":15,"key":"noise_style","symbol":"style","label":"GRID","name":"Band Grid","min":0,"max":3,"default":0,"step":1,"options":["HARM","ODD","METL","CUST"],"unit":null,"curve":null,"page":"NOISE BANK"},{"index":16,"key":"noise_rate","symbol":"rate","label":"RATE","name":"Event Rate","min":0.1,"max":2000,"default":20,"step":0.1,"options":null,"unit":null,"curve":null,"page":"NOISE EVENTS"},{"index":17,"key":"noise_size","symbol":"size","label":"SIZE","name":"Event Size","min":0.02,"max":500,"default":1.2,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE EVENTS"},{"index":18,"key":"noise_clump","symbol":"clump","label":"CLMP","name":"Event Clustering","min":0,"max":100,"default":20,"step":1,"options":null,"unit":null,"curve":null,"page":"NOISE EVENTS"},{"index":19,"key":"noise_edge","symbol":"edge","label":"EDGE","name":"Event Edge","min":0,"max":100,"default":25,"step":1,"options":null,"unit":null,"curve":null,"page":"NOISE EVENTS"},{"index":20,"key":"noise_variation","symbol":"variation","label":"VAR","name":"Event Variation","min":0,"max":100,"default":75,"step":1,"options":null,"unit":null,"curve":null,"page":"NOISE EVENTS"},{"index":21,"key":"noise_bed","symbol":"bed","label":"BED","name":"Noise Bed","min":0,"max":100,"default":0,"step":1,"options":null,"unit":null,"curve":null,"page":"NOISE EVENTS"},{"index":22,"key":"noise_repeat","symbol":"repeat","label":"REPT","name":"Event Repeats","min":1,"max":16,"default":1,"step":1,"options":null,"unit":null,"curve":null,"page":"NOISE EVENTS"},{"index":23,"key":"noise_fault","symbol":"fault","label":"FAIL","name":"Data Variation","min":0,"max":100,"default":25,"step":1,"options":null,"unit":null,"curve":null,"page":"NOISE EVENTS"},{"index":24,"key":"noise_ratio_1","symbol":"ratio_1","label":"R1","name":"Band 1 Ratio","min":1,"max":64,"default":1,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":25,"key":"noise_ratio_2","symbol":"ratio_2","label":"R2","name":"Band 2 Ratio","min":1,"max":64,"default":2,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":26,"key":"noise_ratio_3","symbol":"ratio_3","label":"R3","name":"Band 3 Ratio","min":1,"max":64,"default":3,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":27,"key":"noise_ratio_4","symbol":"ratio_4","label":"R4","name":"Band 4 Ratio","min":1,"max":64,"default":4,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":28,"key":"noise_ratio_5","symbol":"ratio_5","label":"R5","name":"Band 5 Ratio","min":1,"max":64,"default":5,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":29,"key":"noise_ratio_6","symbol":"ratio_6","label":"R6","name":"Band 6 Ratio","min":1,"max":64,"default":6,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":30,"key":"noise_ratio_7","symbol":"ratio_7","label":"R7","name":"Band 7 Ratio","min":1,"max":64,"default":7,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":31,"key":"noise_ratio_8","symbol":"ratio_8","label":"R8","name":"Band 8 Ratio","min":1,"max":64,"default":8,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":32,"key":"noise_ratio_9","symbol":"ratio_9","label":"R9","name":"Band 9 Ratio","min":1,"max":64,"default":9,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":33,"key":"noise_ratio_10","symbol":"ratio_10","label":"R10","name":"Band 10 Ratio","min":1,"max":64,"default":10,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":34,"key":"noise_ratio_11","symbol":"ratio_11","label":"R11","name":"Band 11 Ratio","min":1,"max":64,"default":11,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":35,"key":"noise_ratio_12","symbol":"ratio_12","label":"R12","name":"Band 12 Ratio","min":1,"max":64,"default":12,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":36,"key":"noise_ratio_13","symbol":"ratio_13","label":"R13","name":"Band 13 Ratio","min":1,"max":64,"default":13,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":37,"key":"noise_ratio_14","symbol":"ratio_14","label":"R14","name":"Band 14 Ratio","min":1,"max":64,"default":14,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":38,"key":"noise_ratio_15","symbol":"ratio_15","label":"R15","name":"Band 15 Ratio","min":1,"max":64,"default":15,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":39,"key":"noise_ratio_16","symbol":"ratio_16","label":"R16","name":"Band 16 Ratio","min":1,"max":64,"default":16,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":40,"key":"noise_ratio_17","symbol":"ratio_17","label":"R17","name":"Band 17 Ratio","min":1,"max":64,"default":17,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":41,"key":"noise_ratio_18","symbol":"ratio_18","label":"R18","name":"Band 18 Ratio","min":1,"max":64,"default":18,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":42,"key":"noise_ratio_19","symbol":"ratio_19","label":"R19","name":"Band 19 Ratio","min":1,"max":64,"default":19,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":43,"key":"noise_ratio_20","symbol":"ratio_20","label":"R20","name":"Band 20 Ratio","min":1,"max":64,"default":20,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":44,"key":"noise_ratio_21","symbol":"ratio_21","label":"R21","name":"Band 21 Ratio","min":1,"max":64,"default":21,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":45,"key":"noise_ratio_22","symbol":"ratio_22","label":"R22","name":"Band 22 Ratio","min":1,"max":64,"default":22,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":46,"key":"noise_ratio_23","symbol":"ratio_23","label":"R23","name":"Band 23 Ratio","min":1,"max":64,"default":23,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":47,"key":"noise_ratio_24","symbol":"ratio_24","label":"R24","name":"Band 24 Ratio","min":1,"max":64,"default":24,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":48,"key":"noise_ratio_25","symbol":"ratio_25","label":"R25","name":"Band 25 Ratio","min":1,"max":64,"default":25,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":49,"key":"noise_ratio_26","symbol":"ratio_26","label":"R26","name":"Band 26 Ratio","min":1,"max":64,"default":26,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":50,"key":"noise_ratio_27","symbol":"ratio_27","label":"R27","name":"Band 27 Ratio","min":1,"max":64,"default":27,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":51,"key":"noise_ratio_28","symbol":"ratio_28","label":"R28","name":"Band 28 Ratio","min":1,"max":64,"default":28,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":52,"key":"noise_ratio_29","symbol":"ratio_29","label":"R29","name":"Band 29 Ratio","min":1,"max":64,"default":29,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":53,"key":"noise_ratio_30","symbol":"ratio_30","label":"R30","name":"Band 30 Ratio","min":1,"max":64,"default":30,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":54,"key":"noise_ratio_31","symbol":"ratio_31","label":"R31","name":"Band 31 Ratio","min":1,"max":64,"default":31,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":55,"key":"noise_ratio_32","symbol":"ratio_32","label":"R32","name":"Band 32 Ratio","min":1,"max":64,"default":32,"step":0.05,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":56,"key":"noise_band_1","symbol":"band_1","label":"L1","name":"Band 1 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":57,"key":"noise_band_2","symbol":"band_2","label":"L2","name":"Band 2 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":58,"key":"noise_band_3","symbol":"band_3","label":"L3","name":"Band 3 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":59,"key":"noise_band_4","symbol":"band_4","label":"L4","name":"Band 4 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":60,"key":"noise_band_5","symbol":"band_5","label":"L5","name":"Band 5 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":61,"key":"noise_band_6","symbol":"band_6","label":"L6","name":"Band 6 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":62,"key":"noise_band_7","symbol":"band_7","label":"L7","name":"Band 7 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":63,"key":"noise_band_8","symbol":"band_8","label":"L8","name":"Band 8 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":64,"key":"noise_band_9","symbol":"band_9","label":"L9","name":"Band 9 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":65,"key":"noise_band_10","symbol":"band_10","label":"L10","name":"Band 10 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":66,"key":"noise_band_11","symbol":"band_11","label":"L11","name":"Band 11 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":67,"key":"noise_band_12","symbol":"band_12","label":"L12","name":"Band 12 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":68,"key":"noise_band_13","symbol":"band_13","label":"L13","name":"Band 13 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":69,"key":"noise_band_14","symbol":"band_14","label":"L14","name":"Band 14 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":70,"key":"noise_band_15","symbol":"band_15","label":"L15","name":"Band 15 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":71,"key":"noise_band_16","symbol":"band_16","label":"L16","name":"Band 16 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":72,"key":"noise_band_17","symbol":"band_17","label":"L17","name":"Band 17 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":73,"key":"noise_band_18","symbol":"band_18","label":"L18","name":"Band 18 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":74,"key":"noise_band_19","symbol":"band_19","label":"L19","name":"Band 19 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":75,"key":"noise_band_20","symbol":"band_20","label":"L20","name":"Band 20 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":76,"key":"noise_band_21","symbol":"band_21","label":"L21","name":"Band 21 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":77,"key":"noise_band_22","symbol":"band_22","label":"L22","name":"Band 22 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":78,"key":"noise_band_23","symbol":"band_23","label":"L23","name":"Band 23 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":79,"key":"noise_band_24","symbol":"band_24","label":"L24","name":"Band 24 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":80,"key":"noise_band_25","symbol":"band_25","label":"L25","name":"Band 25 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":81,"key":"noise_band_26","symbol":"band_26","label":"L26","name":"Band 26 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":82,"key":"noise_band_27","symbol":"band_27","label":"L27","name":"Band 27 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":83,"key":"noise_band_28","symbol":"band_28","label":"L28","name":"Band 28 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":84,"key":"noise_band_29","symbol":"band_29","label":"L29","name":"Band 29 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":85,"key":"noise_band_30","symbol":"band_30","label":"L30","name":"Band 30 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":86,"key":"noise_band_31","symbol":"band_31","label":"L31","name":"Band 31 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"},{"index":87,"key":"noise_band_32","symbol":"band_32","label":"L32","name":"Band 32 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"NOISE BANDS"}];
Object.assign(PARAMS,{"noise_type":{"key":"noise_type","label":"TYPE","shortLabel":"TYPE","min":0,"max":6,"value":0,"step":1,"integer":true,"labels":["BANK","DUST","CRKL","STAT","CLIK","CUTS","DATA"]},"noise_tune":{"key":"noise_tune","label":"TUNE","shortLabel":"TUNE","min":-48,"max":48,"value":0,"step":0.1,"integer":false,"unit":"st"},"noise_width":{"key":"noise_width","label":"WID","shortLabel":"WID","min":0,"max":100,"value":50,"step":1,"integer":false,"unit":"%"},"noise_level":{"key":"noise_level","label":"LVL","shortLabel":"LVL","min":0,"max":100,"value":75,"step":1,"integer":false,"unit":"%"},"noise_color":{"key":"noise_color","label":"COLR","shortLabel":"COLR","min":0,"max":3,"value":0,"step":1,"integer":true,"labels":["WHT","PINK","BRWN","BLUE"]},"noise_tone":{"key":"noise_tone","label":"TONE","shortLabel":"TONE","min":20,"max":20000,"value":16000,"step":1,"integer":false,"unit":"Hz","curve":"log"},"noise_bits":{"key":"noise_bits","label":"BITS","shortLabel":"BITS","min":2,"max":24,"value":24,"step":0.1,"integer":false},"noise_seed":{"key":"noise_seed","label":"SEED","shortLabel":"SEED","min":0,"max":65535,"value":1,"step":1,"integer":true},"noise_count":{"key":"noise_count","label":"BAND","shortLabel":"BAND","min":1,"max":32,"value":8,"step":0.1,"integer":false},"noise_bandwidth":{"key":"noise_bandwidth","label":"BW","shortLabel":"BW","min":0.1,"max":48,"value":2,"step":0.1,"integer":false},"noise_tilt":{"key":"noise_tilt","label":"TILT","shortLabel":"TILT","min":-24,"max":12,"value":0,"step":0.5,"integer":false},"noise_stretch":{"key":"noise_stretch","label":"STR","shortLabel":"STR","min":0.25,"max":4,"value":1,"step":0.01,"integer":false},"noise_motion":{"key":"noise_motion","label":"MOVE","shortLabel":"MOVE","min":0,"max":24,"value":0,"step":0.1,"integer":false},"noise_motion_rate":{"key":"noise_motion_rate","label":"RATE","shortLabel":"RATE","min":0.01,"max":20,"value":0.2,"step":0.01,"integer":false},"noise_leak":{"key":"noise_leak","label":"LEAK","shortLabel":"LEAK","min":0,"max":100,"value":0,"step":1,"integer":false},"noise_style":{"key":"noise_style","label":"GRID","shortLabel":"GRID","min":0,"max":3,"value":0,"step":1,"integer":true,"labels":["HARM","ODD","METL","CUST"]},"noise_rate":{"key":"noise_rate","label":"RATE","shortLabel":"RATE","min":0.1,"max":2000,"value":20,"step":0.1,"integer":false},"noise_size":{"key":"noise_size","label":"SIZE","shortLabel":"SIZE","min":0.02,"max":500,"value":1.2,"step":0.01,"integer":false},"noise_clump":{"key":"noise_clump","label":"CLMP","shortLabel":"CLMP","min":0,"max":100,"value":20,"step":1,"integer":false},"noise_edge":{"key":"noise_edge","label":"EDGE","shortLabel":"EDGE","min":0,"max":100,"value":25,"step":1,"integer":false},"noise_variation":{"key":"noise_variation","label":"VAR","shortLabel":"VAR","min":0,"max":100,"value":75,"step":1,"integer":false},"noise_bed":{"key":"noise_bed","label":"BED","shortLabel":"BED","min":0,"max":100,"value":0,"step":1,"integer":false},"noise_repeat":{"key":"noise_repeat","label":"REPT","shortLabel":"REPT","min":1,"max":16,"value":1,"step":1,"integer":true},"noise_fault":{"key":"noise_fault","label":"FAIL","shortLabel":"FAIL","min":0,"max":100,"value":25,"step":1,"integer":false},"noise_ratio_1":{"key":"noise_ratio_1","label":"R1","shortLabel":"R1","min":1,"max":64,"value":1,"step":0.05,"integer":false},"noise_ratio_2":{"key":"noise_ratio_2","label":"R2","shortLabel":"R2","min":1,"max":64,"value":2,"step":0.05,"integer":false},"noise_ratio_3":{"key":"noise_ratio_3","label":"R3","shortLabel":"R3","min":1,"max":64,"value":3,"step":0.05,"integer":false},"noise_ratio_4":{"key":"noise_ratio_4","label":"R4","shortLabel":"R4","min":1,"max":64,"value":4,"step":0.05,"integer":false},"noise_ratio_5":{"key":"noise_ratio_5","label":"R5","shortLabel":"R5","min":1,"max":64,"value":5,"step":0.05,"integer":false},"noise_ratio_6":{"key":"noise_ratio_6","label":"R6","shortLabel":"R6","min":1,"max":64,"value":6,"step":0.05,"integer":false},"noise_ratio_7":{"key":"noise_ratio_7","label":"R7","shortLabel":"R7","min":1,"max":64,"value":7,"step":0.05,"integer":false},"noise_ratio_8":{"key":"noise_ratio_8","label":"R8","shortLabel":"R8","min":1,"max":64,"value":8,"step":0.05,"integer":false},"noise_ratio_9":{"key":"noise_ratio_9","label":"R9","shortLabel":"R9","min":1,"max":64,"value":9,"step":0.05,"integer":false},"noise_ratio_10":{"key":"noise_ratio_10","label":"R10","shortLabel":"R10","min":1,"max":64,"value":10,"step":0.05,"integer":false},"noise_ratio_11":{"key":"noise_ratio_11","label":"R11","shortLabel":"R11","min":1,"max":64,"value":11,"step":0.05,"integer":false},"noise_ratio_12":{"key":"noise_ratio_12","label":"R12","shortLabel":"R12","min":1,"max":64,"value":12,"step":0.05,"integer":false},"noise_ratio_13":{"key":"noise_ratio_13","label":"R13","shortLabel":"R13","min":1,"max":64,"value":13,"step":0.05,"integer":false},"noise_ratio_14":{"key":"noise_ratio_14","label":"R14","shortLabel":"R14","min":1,"max":64,"value":14,"step":0.05,"integer":false},"noise_ratio_15":{"key":"noise_ratio_15","label":"R15","shortLabel":"R15","min":1,"max":64,"value":15,"step":0.05,"integer":false},"noise_ratio_16":{"key":"noise_ratio_16","label":"R16","shortLabel":"R16","min":1,"max":64,"value":16,"step":0.05,"integer":false},"noise_ratio_17":{"key":"noise_ratio_17","label":"R17","shortLabel":"R17","min":1,"max":64,"value":17,"step":0.05,"integer":false},"noise_ratio_18":{"key":"noise_ratio_18","label":"R18","shortLabel":"R18","min":1,"max":64,"value":18,"step":0.05,"integer":false},"noise_ratio_19":{"key":"noise_ratio_19","label":"R19","shortLabel":"R19","min":1,"max":64,"value":19,"step":0.05,"integer":false},"noise_ratio_20":{"key":"noise_ratio_20","label":"R20","shortLabel":"R20","min":1,"max":64,"value":20,"step":0.05,"integer":false},"noise_ratio_21":{"key":"noise_ratio_21","label":"R21","shortLabel":"R21","min":1,"max":64,"value":21,"step":0.05,"integer":false},"noise_ratio_22":{"key":"noise_ratio_22","label":"R22","shortLabel":"R22","min":1,"max":64,"value":22,"step":0.05,"integer":false},"noise_ratio_23":{"key":"noise_ratio_23","label":"R23","shortLabel":"R23","min":1,"max":64,"value":23,"step":0.05,"integer":false},"noise_ratio_24":{"key":"noise_ratio_24","label":"R24","shortLabel":"R24","min":1,"max":64,"value":24,"step":0.05,"integer":false},"noise_ratio_25":{"key":"noise_ratio_25","label":"R25","shortLabel":"R25","min":1,"max":64,"value":25,"step":0.05,"integer":false},"noise_ratio_26":{"key":"noise_ratio_26","label":"R26","shortLabel":"R26","min":1,"max":64,"value":26,"step":0.05,"integer":false},"noise_ratio_27":{"key":"noise_ratio_27","label":"R27","shortLabel":"R27","min":1,"max":64,"value":27,"step":0.05,"integer":false},"noise_ratio_28":{"key":"noise_ratio_28","label":"R28","shortLabel":"R28","min":1,"max":64,"value":28,"step":0.05,"integer":false},"noise_ratio_29":{"key":"noise_ratio_29","label":"R29","shortLabel":"R29","min":1,"max":64,"value":29,"step":0.05,"integer":false},"noise_ratio_30":{"key":"noise_ratio_30","label":"R30","shortLabel":"R30","min":1,"max":64,"value":30,"step":0.05,"integer":false},"noise_ratio_31":{"key":"noise_ratio_31","label":"R31","shortLabel":"R31","min":1,"max":64,"value":31,"step":0.05,"integer":false},"noise_ratio_32":{"key":"noise_ratio_32","label":"R32","shortLabel":"R32","min":1,"max":64,"value":32,"step":0.05,"integer":false},"noise_band_1":{"key":"noise_band_1","label":"L1","shortLabel":"L1","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_2":{"key":"noise_band_2","label":"L2","shortLabel":"L2","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_3":{"key":"noise_band_3","label":"L3","shortLabel":"L3","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_4":{"key":"noise_band_4","label":"L4","shortLabel":"L4","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_5":{"key":"noise_band_5","label":"L5","shortLabel":"L5","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_6":{"key":"noise_band_6","label":"L6","shortLabel":"L6","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_7":{"key":"noise_band_7","label":"L7","shortLabel":"L7","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_8":{"key":"noise_band_8","label":"L8","shortLabel":"L8","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_9":{"key":"noise_band_9","label":"L9","shortLabel":"L9","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_10":{"key":"noise_band_10","label":"L10","shortLabel":"L10","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_11":{"key":"noise_band_11","label":"L11","shortLabel":"L11","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_12":{"key":"noise_band_12","label":"L12","shortLabel":"L12","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_13":{"key":"noise_band_13","label":"L13","shortLabel":"L13","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_14":{"key":"noise_band_14","label":"L14","shortLabel":"L14","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_15":{"key":"noise_band_15","label":"L15","shortLabel":"L15","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_16":{"key":"noise_band_16","label":"L16","shortLabel":"L16","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_17":{"key":"noise_band_17","label":"L17","shortLabel":"L17","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_18":{"key":"noise_band_18","label":"L18","shortLabel":"L18","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_19":{"key":"noise_band_19","label":"L19","shortLabel":"L19","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_20":{"key":"noise_band_20","label":"L20","shortLabel":"L20","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_21":{"key":"noise_band_21","label":"L21","shortLabel":"L21","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_22":{"key":"noise_band_22","label":"L22","shortLabel":"L22","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_23":{"key":"noise_band_23","label":"L23","shortLabel":"L23","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_24":{"key":"noise_band_24","label":"L24","shortLabel":"L24","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_25":{"key":"noise_band_25","label":"L25","shortLabel":"L25","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_26":{"key":"noise_band_26","label":"L26","shortLabel":"L26","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_27":{"key":"noise_band_27","label":"L27","shortLabel":"L27","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_28":{"key":"noise_band_28","label":"L28","shortLabel":"L28","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_29":{"key":"noise_band_29","label":"L29","shortLabel":"L29","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_30":{"key":"noise_band_30","label":"L30","shortLabel":"L30","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_31":{"key":"noise_band_31","label":"L31","shortLabel":"L31","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"noise_band_32":{"key":"noise_band_32","label":"L32","shortLabel":"L32","min":-1,"max":1,"value":1,"step":0.01,"integer":false}});
const NOISE_PAGE={title:"NOISE",noise:true,keys:["noise_type", "noise_tune", "noise_width", "noise_level", "noise_color", "noise_tone", "noise_bits", "noise_seed"]};
const NOISE_BANK_PAGE={title:"NOISE BANK",noise:true,keys:["noise_count", "noise_bandwidth", "noise_tilt", "noise_stretch", "noise_motion", "noise_motion_rate", "noise_leak", "noise_style"]};
const NOISE_EVENTS_PAGE={title:"NOISE EVENTS",noise:true,keys:["noise_rate", "noise_size", "noise_clump", "noise_edge", "noise_variation", "noise_bed", "noise_repeat", "noise_fault"]};
const NOISE_BANDS_PAGE={title:"NOISE BANDS",noise:true,noiseBands:true,keys:["noise_ratio_1", "noise_ratio_2", "noise_ratio_3", "noise_ratio_4", "noise_band_1", "noise_band_2", "noise_band_3", "noise_band_4"]};
const noiseUi={data:null,floor:0,group:0};
function readNoise(cached=uiPolling.cached){let data;try{data=JSON.parse(String(cached?uiPolling.cache.get('noise_state'):getParam('noise_state','{}')));}catch(_){return;}
 if(!data||data.part!==(state.splitEnabled?state.splitFocus:0)||!Number.isInteger(data.edit)||data.edit<0||data.edit>3||!Array.isArray(data.patches)||data.patches.length!==4||data.patches.some(p=>!Array.isArray(p)||p.length!==88||p.some(x=>!Number.isFinite(x)))||data.revision<noiseUi.floor)return;
 const expected=state.splitEnabled&&!state.sharedDual?(isQuad()?state.splitFocus:Math.floor(clamp(state.lane-1,state.splitFocus*2,state.splitFocus*2+1))):state.lane-1;
 if(data.edit!==expected)return;
 noiseUi.data=data;noiseUi.group=clamp(data.group||0,0,7);if(!cached)noiseUi.floor=data.revision;
 NOISE_CONTROLS.forEach(p=>PARAMS[p.key].value=p.index>=24&&p.index<56&&data.patches[data.edit][15]!==3?(data.patches[data.edit][15]===0?p.index-23:data.patches[data.edit][15]===1?2*(p.index-24)+1:1+1.41421356*(p.index-24)):data.patches[data.edit][p.index]);
}

function noiseBandsPage(){const g=noiseUi.group;return {...NOISE_BANDS_PAGE,title:"BANDS "+(g*4+1)+"-"+(g*4+4),keys:[0,1,2,3].map(i=>"noise_ratio_"+(g*4+i+1)).concat([0,1,2,3].map(i=>"noise_band_"+(g*4+i+1)))};}

const RESON_CONTROLS=[{"index":0,"key":"reson_mode","symbol":"mode","label":"MODE","name":"Reson Articulation","min":0,"max":1,"default":0,"step":1,"options":["TONE","HIT"],"unit":null,"curve":null,"page":"RESON"},{"index":1,"key":"reson_tune","symbol":"tune","label":"TUNE","name":"Reson Transpose","min":-48,"max":48,"default":0,"step":0.1,"options":null,"unit":"st","curve":null,"page":"RESON"},{"index":2,"key":"reson_width","symbol":"width","label":"WID","name":"Reson Stereo Width","min":0,"max":100,"default":30,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON"},{"index":3,"key":"reson_level","symbol":"level","label":"LVL","name":"Reson Level","min":0,"max":100,"default":80,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON"},{"index":4,"key":"reson_model","symbol":"model","label":"BODY","name":"Reson Body","min":0,"max":7,"default":0,"step":1,"options":["OPEN","KICK","SNAR","TOM","BELL","HAT","CRSH","CBAS"],"unit":null,"curve":null,"page":"RESON"},{"index":5,"key":"reson_decay","symbol":"decay","label":"DEC","name":"Reson Ring Decay","min":10,"max":8000,"default":1000,"step":1,"options":null,"unit":"ms","curve":null,"page":"RESON"},{"index":6,"key":"reson_brightness","symbol":"brightness","label":"BRGT","name":"Modal Brightness","min":0,"max":100,"default":55,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON"},{"index":7,"key":"reson_damping","symbol":"damping","label":"DAMP","name":"Upper Mode Damping","min":0,"max":100,"default":35,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON"},{"index":8,"key":"reson_stretch","symbol":"stretch","label":"STRC","name":"Modal Stretch","min":0.5,"max":2,"default":1,"step":0.01,"options":null,"unit":null,"curve":null,"page":"RESON BODY"},{"index":9,"key":"reson_inharmonic","symbol":"inharmonic","label":"INHR","name":"Body Inharmonicity","min":0,"max":100,"default":100,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON BODY"},{"index":10,"key":"reson_mass","symbol":"mass","label":"MASS","name":"Body Mass","min":0,"max":100,"default":50,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON BODY"},{"index":11,"key":"reson_bend","symbol":"bend","label":"BEND","name":"Strike Pitch Sweep","min":-48,"max":48,"default":0,"step":0.1,"options":null,"unit":"st","curve":null,"page":"RESON BODY"},{"index":12,"key":"reson_sweep","symbol":"sweep","label":"TIME","name":"Pitch Sweep Time","min":1,"max":2000,"default":25,"step":1,"options":null,"unit":"ms","curve":null,"page":"RESON BODY"},{"index":13,"key":"reson_noise","symbol":"noise","label":"NOIS","name":"Body Noise","min":0,"max":100,"default":0,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON BODY"},{"index":14,"key":"reson_noise_decay","symbol":"noise_decay","label":"NDEC","name":"Body Noise Decay","min":1,"max":4000,"default":120,"step":1,"options":null,"unit":"ms","curve":null,"page":"RESON BODY"},{"index":15,"key":"reson_color","symbol":"color","label":"COLR","name":"Body Noise Color","min":0,"max":100,"default":50,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON BODY"},{"index":16,"key":"reson_hit","symbol":"hit","label":"HIT","name":"Strike Level","min":0,"max":100,"default":10,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON HIT"},{"index":17,"key":"reson_hardness","symbol":"hardness","label":"HARD","name":"Beater Hardness","min":0,"max":100,"default":60,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON HIT"},{"index":18,"key":"reson_hit_decay","symbol":"hit_decay","label":"HDEC","name":"Strike Decay","min":0.2,"max":100,"default":4,"step":0.1,"options":null,"unit":"ms","curve":null,"page":"RESON HIT"},{"index":19,"key":"reson_punch","symbol":"punch","label":"PNCH","name":"Attack Punch","min":0,"max":100,"default":35,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON HIT"},{"index":20,"key":"reson_position","symbol":"position","label":"SPOT","name":"Strike Position","min":0,"max":100,"default":20,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON HIT"},{"index":21,"key":"reson_coupling","symbol":"coupling","label":"COUP","name":"Mode Coupling","min":0,"max":100,"default":10,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON HIT"},{"index":22,"key":"reson_detune","symbol":"detune","label":"DETN","name":"Modal Stereo Detune","min":0,"max":30,"default":0,"step":0.1,"options":null,"unit":"cent","curve":null,"page":"RESON HIT"},{"index":23,"key":"reson_seed","symbol":"seed","label":"SEED","name":"Strike Seed","min":0,"max":65535,"default":1,"step":1,"options":null,"unit":null,"curve":null,"page":"RESON HIT"},{"index":24,"key":"reson_drive","symbol":"drive","label":"DRIV","name":"Overload Drive","min":0,"max":100,"default":0,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON DRIVE"},{"index":25,"key":"reson_clip","symbol":"clip","label":"CLIP","name":"Overload Shape","min":0,"max":100,"default":20,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON DRIVE"},{"index":26,"key":"reson_bias","symbol":"bias","label":"BIAS","name":"Overload Asymmetry","min":-100,"max":100,"default":0,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON DRIVE"},{"index":27,"key":"reson_sag","symbol":"sag","label":"SAG","name":"Overload Sag","min":0,"max":100,"default":15,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON DRIVE"},{"index":28,"key":"reson_recovery","symbol":"recovery","label":"REC","name":"Sag Recovery","min":5,"max":500,"default":80,"step":1,"options":null,"unit":"ms","curve":null,"page":"RESON DRIVE"},{"index":29,"key":"reson_tone","symbol":"tone","label":"TONE","name":"Overload Low Pass","min":200,"max":20000,"default":20000,"step":1,"options":null,"unit":"Hz","curve":"log","page":"RESON DRIVE"},{"index":30,"key":"reson_mix","symbol":"mix","label":"MIX","name":"Overload Mix","min":0,"max":100,"default":100,"step":1,"options":null,"unit":"%","curve":null,"page":"RESON DRIVE"},{"index":31,"key":"reson_output","symbol":"output","label":"OUT","name":"Overload Output","min":-18,"max":12,"default":0,"step":0.5,"options":null,"unit":"dB","curve":null,"page":"RESON DRIVE"}];
Object.assign(PARAMS,{"reson_mode":{"key":"reson_mode","label":"MODE","shortLabel":"MODE","min":0,"max":1,"value":0,"step":1,"integer":true,"labels":["TONE","HIT"]},"reson_tune":{"key":"reson_tune","label":"TUNE","shortLabel":"TUNE","min":-48,"max":48,"value":0,"step":0.1,"integer":false,"unit":"st"},"reson_width":{"key":"reson_width","label":"WID","shortLabel":"WID","min":0,"max":100,"value":30,"step":1,"integer":false,"unit":"%"},"reson_level":{"key":"reson_level","label":"LVL","shortLabel":"LVL","min":0,"max":100,"value":80,"step":1,"integer":false,"unit":"%"},"reson_model":{"key":"reson_model","label":"BODY","shortLabel":"BODY","min":0,"max":7,"value":0,"step":1,"integer":true,"labels":["OPEN","KICK","SNAR","TOM","BELL","HAT","CRSH","CBAS"]},"reson_decay":{"key":"reson_decay","label":"DEC","shortLabel":"DEC","min":10,"max":8000,"value":1000,"step":1,"integer":false,"unit":"ms"},"reson_brightness":{"key":"reson_brightness","label":"BRGT","shortLabel":"BRGT","min":0,"max":100,"value":55,"step":1,"integer":false,"unit":"%"},"reson_damping":{"key":"reson_damping","label":"DAMP","shortLabel":"DAMP","min":0,"max":100,"value":35,"step":1,"integer":false,"unit":"%"},"reson_stretch":{"key":"reson_stretch","label":"STRC","shortLabel":"STRC","min":0.5,"max":2,"value":1,"step":0.01,"integer":false},"reson_inharmonic":{"key":"reson_inharmonic","label":"INHR","shortLabel":"INHR","min":0,"max":100,"value":100,"step":1,"integer":false,"unit":"%"},"reson_mass":{"key":"reson_mass","label":"MASS","shortLabel":"MASS","min":0,"max":100,"value":50,"step":1,"integer":false,"unit":"%"},"reson_bend":{"key":"reson_bend","label":"BEND","shortLabel":"BEND","min":-48,"max":48,"value":0,"step":0.1,"integer":false,"unit":"st"},"reson_sweep":{"key":"reson_sweep","label":"TIME","shortLabel":"TIME","min":1,"max":2000,"value":25,"step":1,"integer":false,"unit":"ms"},"reson_noise":{"key":"reson_noise","label":"NOIS","shortLabel":"NOIS","min":0,"max":100,"value":0,"step":1,"integer":false,"unit":"%"},"reson_noise_decay":{"key":"reson_noise_decay","label":"NDEC","shortLabel":"NDEC","min":1,"max":4000,"value":120,"step":1,"integer":false,"unit":"ms"},"reson_color":{"key":"reson_color","label":"COLR","shortLabel":"COLR","min":0,"max":100,"value":50,"step":1,"integer":false,"unit":"%"},"reson_hit":{"key":"reson_hit","label":"HIT","shortLabel":"HIT","min":0,"max":100,"value":10,"step":1,"integer":false,"unit":"%"},"reson_hardness":{"key":"reson_hardness","label":"HARD","shortLabel":"HARD","min":0,"max":100,"value":60,"step":1,"integer":false,"unit":"%"},"reson_hit_decay":{"key":"reson_hit_decay","label":"HDEC","shortLabel":"HDEC","min":0.2,"max":100,"value":4,"step":0.1,"integer":false,"unit":"ms"},"reson_punch":{"key":"reson_punch","label":"PNCH","shortLabel":"PNCH","min":0,"max":100,"value":35,"step":1,"integer":false,"unit":"%"},"reson_position":{"key":"reson_position","label":"SPOT","shortLabel":"SPOT","min":0,"max":100,"value":20,"step":1,"integer":false,"unit":"%"},"reson_coupling":{"key":"reson_coupling","label":"COUP","shortLabel":"COUP","min":0,"max":100,"value":10,"step":1,"integer":false,"unit":"%"},"reson_detune":{"key":"reson_detune","label":"DETN","shortLabel":"DETN","min":0,"max":30,"value":0,"step":0.1,"integer":false,"unit":"cent"},"reson_seed":{"key":"reson_seed","label":"SEED","shortLabel":"SEED","min":0,"max":65535,"value":1,"step":1,"integer":true},"reson_drive":{"key":"reson_drive","label":"DRIV","shortLabel":"DRIV","min":0,"max":100,"value":0,"step":1,"integer":false,"unit":"%"},"reson_clip":{"key":"reson_clip","label":"CLIP","shortLabel":"CLIP","min":0,"max":100,"value":20,"step":1,"integer":false,"unit":"%"},"reson_bias":{"key":"reson_bias","label":"BIAS","shortLabel":"BIAS","min":-100,"max":100,"value":0,"step":1,"integer":false,"unit":"%"},"reson_sag":{"key":"reson_sag","label":"SAG","shortLabel":"SAG","min":0,"max":100,"value":15,"step":1,"integer":false,"unit":"%"},"reson_recovery":{"key":"reson_recovery","label":"REC","shortLabel":"REC","min":5,"max":500,"value":80,"step":1,"integer":false,"unit":"ms"},"reson_tone":{"key":"reson_tone","label":"TONE","shortLabel":"TONE","min":200,"max":20000,"value":20000,"step":1,"integer":false,"unit":"Hz","curve":"log"},"reson_mix":{"key":"reson_mix","label":"MIX","shortLabel":"MIX","min":0,"max":100,"value":100,"step":1,"integer":false,"unit":"%"},"reson_output":{"key":"reson_output","label":"OUT","shortLabel":"OUT","min":-18,"max":12,"value":0,"step":0.5,"integer":false,"unit":"dB"}});
const RESON_PAGE={title:"RESON",reson:true,keys:["reson_mode", "reson_tune", "reson_width", "reson_level", "reson_model", "reson_decay", "reson_brightness", "reson_damping"]};
const RESON_BODY_PAGE={title:"RESON BODY",reson:true,keys:["reson_stretch", "reson_inharmonic", "reson_mass", "reson_bend", "reson_sweep", "reson_noise", "reson_noise_decay", "reson_color"]};
const RESON_HIT_PAGE={title:"RESON HIT",reson:true,keys:["reson_hit", "reson_hardness", "reson_hit_decay", "reson_punch", "reson_position", "reson_coupling", "reson_detune", "reson_seed"]};
const RESON_DRIVE_PAGE={title:"RESON DRIVE",reson:true,keys:["reson_drive", "reson_clip", "reson_bias", "reson_sag", "reson_recovery", "reson_tone", "reson_mix", "reson_output"]};
const resonUi={data:null,floor:0,startsOpen:false,start:0,armed:false,renderKey:null};
function readReson(cached=uiPolling.cached){let data;try{data=JSON.parse(String(cached?uiPolling.cache.get('reson_state'):getParam('reson_state','{}')));}catch(_){return;}
 if(!data||data.part!==(state.splitEnabled?state.splitFocus:0)||!Number.isInteger(data.edit)||data.edit<0||data.edit>3||!Array.isArray(data.patches)||data.patches.length!==4||data.patches.some(p=>!Array.isArray(p)||p.length!==32||p.some(x=>!Number.isFinite(x)))||data.revision<resonUi.floor)return;
 const expected=state.splitEnabled&&!state.sharedDual?(isQuad()?state.splitFocus:Math.floor(clamp(state.lane-1,state.splitFocus*2,state.splitFocus*2+1))):state.lane-1;
 if(data.edit!==expected)return;
 resonUi.data=data;if(!cached)resonUi.floor=data.revision;
 RESON_CONTROLS.forEach(p=>PARAMS[p.key].value=data.patches[data.edit][p.index]);
}

const RESON_STARTS=['OPEN','KICK','SNARE','TOM','BELL','HAT','CRASH','CBASS'];
function handleResonMidi(status,number,value){
 const active=state.view==='settings'&&Boolean(currentSettingsPage().reson)&&!state.performanceVisible;
 if(resonUi.startsOpen&&!active)resonUi.startsOpen=false;
 if(status!==0xb0)return false;
 if(!resonUi.startsOpen){if(number===MoveMainButton&&value>0&&active&&!state.shiftHeld&&!state.focusButtonHeld){if(sequenceSoundActive()){showActionMessage('LANE SETTING');draw();return true;}resonUi.startsOpen=true;resonUi.start=0;resonUi.armed=false;resonUi.renderKey=null;draw();return true;}return false;}
 if(number===MoveMainKnob){resonUi.start=clamp(resonUi.start+decodeDelta(value),0,RESON_STARTS.length-1);draw();return true;}
 if([MoveUp,MoveDown,MoveLeft,MoveRight].includes(number)){if(value>0){resonUi.start=clamp(resonUi.start+([MoveUp,MoveLeft].includes(number)?-1:1),0,RESON_STARTS.length-1);draw();}return true;}
 if(number===MoveBack||number===MoveMenu){if(value>0){resonUi.startsOpen=false;draw();}return true;}
 if(number===MoveMainButton){if(value<=0)resonUi.armed=true;else if(resonUi.armed){setParamBlocking('reson_start',resonUi.start+1);resonUi.startsOpen=false;readReson(false);draw();}return true;}
 if(number>=MoveKnob1&&number<MoveKnob1+8)return true;return false;
}
function drawResonStarts(){const signature=[resonUi.start,resonUi.data?.edit,state.splitFocus,state.saveOverlayLast].join(':');if(signature===resonUi.renderKey)return;resonUi.renderKey=signature;clearDisplay();drawSettingsTitle('RESON START L'+((resonUi.data?.edit??state.lane-1)+1));drawRect(0,8,128,1,1);const first=clamp(resonUi.start-1,0,RESON_STARTS.length-4);for(let row=0;row<4;++row){const i=first+row,y=13+row*10;if(i===resonUi.start)drawRect(0,y-1,128,9,1);print(3,y,RESON_STARTS[i],i===resonUi.start?0:1);}print(1,57,'CLICK LOAD BACK',1);}

const PARTIALS_CONTROLS=[{"index":0,"key":"partials_type","symbol":"type","label":"TYPE","name":"Partials Type","min":0,"max":2,"default":0,"step":1,"options":["BANK","SUB","PACKET"],"unit":null,"page":"PARTIALS"},{"index":1,"key":"partials_tune","symbol":"tune","label":"TUNE","name":"Partials Transpose","min":-48,"max":48,"default":0,"step":0.1,"options":null,"unit":"st","page":"PARTIALS"},{"index":2,"key":"partials_width","symbol":"width","label":"WID","name":"Partials Stereo Width","min":0,"max":100,"default":35,"step":1,"options":null,"unit":"%","page":"PARTIALS"},{"index":3,"key":"partials_level","symbol":"level","label":"LVL","name":"Partials Level","min":0,"max":100,"default":80,"step":1,"options":null,"unit":"%","page":"PARTIALS"},{"index":4,"key":"partials_count","symbol":"count","label":"PART","name":"Partial Count","min":1,"max":32,"default":16,"step":0.1,"options":null,"unit":null,"page":"PARTIALS"},{"index":5,"key":"partials_tilt","symbol":"tilt","label":"TILT","name":"Spectral Tilt","min":-24,"max":12,"default":0,"step":0.5,"options":null,"unit":"dB","page":"PARTIALS"},{"index":6,"key":"partials_stretch","symbol":"stretch","label":"STRC","name":"Partial Stretch","min":0.25,"max":4,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS"},{"index":7,"key":"partials_phase","symbol":"phase","label":"PHAS","name":"Initial Phase Spread","min":0,"max":100,"default":0,"step":1,"options":null,"unit":"%","page":"PARTIALS"},{"index":8,"key":"partials_harmonics","symbol":"harmonics","label":"HARM","name":"Harmonic Count","min":1,"max":16,"default":8,"step":0.1,"options":null,"unit":null,"page":"PARTIALS TONE"},{"index":9,"key":"partials_subs","symbol":"subs","label":"SUBS","name":"Subharmonic Count","min":0,"max":16,"default":4,"step":0.1,"options":null,"unit":null,"page":"PARTIALS TONE"},{"index":10,"key":"partials_balance","symbol":"balance","label":"BAL","name":"Subharmonic Balance","min":0,"max":100,"default":25,"step":1,"options":null,"unit":"%","page":"PARTIALS TONE"},{"index":11,"key":"partials_detune","symbol":"detune","label":"DETN","name":"Partial Detune","min":0,"max":30,"default":0,"step":0.1,"options":null,"unit":"cent","page":"PARTIALS TONE"},{"index":12,"key":"partials_drift","symbol":"drift","label":"DRFT","name":"Partial Pitch Drift","min":0,"max":30,"default":0,"step":0.1,"options":null,"unit":"cent","page":"PARTIALS TONE"},{"index":13,"key":"partials_drift_rate","symbol":"drift_rate","label":"RATE","name":"Partial Drift Rate","min":0.01,"max":20,"default":0.5,"step":0.01,"options":null,"unit":"Hz","page":"PARTIALS TONE"},{"index":14,"key":"partials_damping","symbol":"damping","label":"DAMP","name":"Upper Partial Decay","min":0,"max":100,"default":0,"step":1,"options":null,"unit":"%","page":"PARTIALS TONE"},{"index":15,"key":"partials_time","symbol":"time","label":"TIME","name":"Spectral Decay Time","min":10,"max":12000,"default":2000,"step":1,"options":null,"unit":"ms","page":"PARTIALS TONE"},{"index":16,"key":"partials_gap","symbol":"gap","label":"GAP","name":"Packet Interval","min":10,"max":2000,"default":150,"step":1,"options":null,"unit":"ms","page":"PARTIALS PACKET"},{"index":17,"key":"partials_scatter","symbol":"scatter","label":"SCAT","name":"Packet Timing Scatter","min":0,"max":100,"default":20,"step":1,"options":null,"unit":"%","page":"PARTIALS PACKET"},{"index":18,"key":"partials_rise","symbol":"rise","label":"RISE","name":"Packet Rise","min":0.5,"max":200,"default":10,"step":0.1,"options":null,"unit":"ms","page":"PARTIALS PACKET"},{"index":19,"key":"partials_sustain","symbol":"sustain","label":"SUS","name":"Packet Sustain","min":0,"max":1000,"default":20,"step":1,"options":null,"unit":"ms","page":"PARTIALS PACKET"},{"index":20,"key":"partials_decay","symbol":"decay","label":"DEC","name":"Packet Decay","min":1,"max":4000,"default":120,"step":1,"options":null,"unit":"ms","page":"PARTIALS PACKET"},{"index":21,"key":"partials_variation","symbol":"variation","label":"VAR","name":"Packet Variation","min":0,"max":100,"default":20,"step":1,"options":null,"unit":"%","page":"PARTIALS PACKET"},{"index":22,"key":"partials_pitch_jitter","symbol":"pitch_jitter","label":"PJIT","name":"Packet Pitch Variation","min":0,"max":12,"default":0,"step":0.1,"options":null,"unit":"st","page":"PARTIALS PACKET"},{"index":23,"key":"partials_shape","symbol":"shape","label":"SHAP","name":"Packet Envelope Shape","min":0.25,"max":4,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS PACKET"},{"index":24,"key":"partials_ratio_1","symbol":"ratio_1","label":"R1","name":"Partial 1 Ratio","min":1,"max":64,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":25,"key":"partials_ratio_2","symbol":"ratio_2","label":"R2","name":"Partial 2 Ratio","min":1,"max":64,"default":2,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":26,"key":"partials_ratio_3","symbol":"ratio_3","label":"R3","name":"Partial 3 Ratio","min":1,"max":64,"default":3,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":27,"key":"partials_ratio_4","symbol":"ratio_4","label":"R4","name":"Partial 4 Ratio","min":1,"max":64,"default":4,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":28,"key":"partials_ratio_5","symbol":"ratio_5","label":"R5","name":"Partial 5 Ratio","min":1,"max":64,"default":5,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":29,"key":"partials_ratio_6","symbol":"ratio_6","label":"R6","name":"Partial 6 Ratio","min":1,"max":64,"default":6,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":30,"key":"partials_ratio_7","symbol":"ratio_7","label":"R7","name":"Partial 7 Ratio","min":1,"max":64,"default":7,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":31,"key":"partials_ratio_8","symbol":"ratio_8","label":"R8","name":"Partial 8 Ratio","min":1,"max":64,"default":8,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":32,"key":"partials_ratio_9","symbol":"ratio_9","label":"R9","name":"Partial 9 Ratio","min":1,"max":64,"default":9,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":33,"key":"partials_ratio_10","symbol":"ratio_10","label":"R10","name":"Partial 10 Ratio","min":1,"max":64,"default":10,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":34,"key":"partials_ratio_11","symbol":"ratio_11","label":"R11","name":"Partial 11 Ratio","min":1,"max":64,"default":11,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":35,"key":"partials_ratio_12","symbol":"ratio_12","label":"R12","name":"Partial 12 Ratio","min":1,"max":64,"default":12,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":36,"key":"partials_ratio_13","symbol":"ratio_13","label":"R13","name":"Partial 13 Ratio","min":1,"max":64,"default":13,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":37,"key":"partials_ratio_14","symbol":"ratio_14","label":"R14","name":"Partial 14 Ratio","min":1,"max":64,"default":14,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":38,"key":"partials_ratio_15","symbol":"ratio_15","label":"R15","name":"Partial 15 Ratio","min":1,"max":64,"default":15,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":39,"key":"partials_ratio_16","symbol":"ratio_16","label":"R16","name":"Partial 16 Ratio","min":1,"max":64,"default":16,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":40,"key":"partials_ratio_17","symbol":"ratio_17","label":"R17","name":"Partial 17 Ratio","min":1,"max":64,"default":17,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":41,"key":"partials_ratio_18","symbol":"ratio_18","label":"R18","name":"Partial 18 Ratio","min":1,"max":64,"default":18,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":42,"key":"partials_ratio_19","symbol":"ratio_19","label":"R19","name":"Partial 19 Ratio","min":1,"max":64,"default":19,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":43,"key":"partials_ratio_20","symbol":"ratio_20","label":"R20","name":"Partial 20 Ratio","min":1,"max":64,"default":20,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":44,"key":"partials_ratio_21","symbol":"ratio_21","label":"R21","name":"Partial 21 Ratio","min":1,"max":64,"default":21,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":45,"key":"partials_ratio_22","symbol":"ratio_22","label":"R22","name":"Partial 22 Ratio","min":1,"max":64,"default":22,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":46,"key":"partials_ratio_23","symbol":"ratio_23","label":"R23","name":"Partial 23 Ratio","min":1,"max":64,"default":23,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":47,"key":"partials_ratio_24","symbol":"ratio_24","label":"R24","name":"Partial 24 Ratio","min":1,"max":64,"default":24,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":48,"key":"partials_ratio_25","symbol":"ratio_25","label":"R25","name":"Partial 25 Ratio","min":1,"max":64,"default":25,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":49,"key":"partials_ratio_26","symbol":"ratio_26","label":"R26","name":"Partial 26 Ratio","min":1,"max":64,"default":26,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":50,"key":"partials_ratio_27","symbol":"ratio_27","label":"R27","name":"Partial 27 Ratio","min":1,"max":64,"default":27,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":51,"key":"partials_ratio_28","symbol":"ratio_28","label":"R28","name":"Partial 28 Ratio","min":1,"max":64,"default":28,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":52,"key":"partials_ratio_29","symbol":"ratio_29","label":"R29","name":"Partial 29 Ratio","min":1,"max":64,"default":29,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":53,"key":"partials_ratio_30","symbol":"ratio_30","label":"R30","name":"Partial 30 Ratio","min":1,"max":64,"default":30,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":54,"key":"partials_ratio_31","symbol":"ratio_31","label":"R31","name":"Partial 31 Ratio","min":1,"max":64,"default":31,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":55,"key":"partials_ratio_32","symbol":"ratio_32","label":"R32","name":"Partial 32 Ratio","min":1,"max":64,"default":32,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":56,"key":"partials_band_1","symbol":"band_1","label":"L1","name":"Partial 1 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":57,"key":"partials_band_2","symbol":"band_2","label":"L2","name":"Partial 2 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":58,"key":"partials_band_3","symbol":"band_3","label":"L3","name":"Partial 3 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":59,"key":"partials_band_4","symbol":"band_4","label":"L4","name":"Partial 4 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":60,"key":"partials_band_5","symbol":"band_5","label":"L5","name":"Partial 5 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":61,"key":"partials_band_6","symbol":"band_6","label":"L6","name":"Partial 6 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":62,"key":"partials_band_7","symbol":"band_7","label":"L7","name":"Partial 7 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":63,"key":"partials_band_8","symbol":"band_8","label":"L8","name":"Partial 8 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":64,"key":"partials_band_9","symbol":"band_9","label":"L9","name":"Partial 9 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":65,"key":"partials_band_10","symbol":"band_10","label":"L10","name":"Partial 10 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":66,"key":"partials_band_11","symbol":"band_11","label":"L11","name":"Partial 11 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":67,"key":"partials_band_12","symbol":"band_12","label":"L12","name":"Partial 12 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":68,"key":"partials_band_13","symbol":"band_13","label":"L13","name":"Partial 13 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":69,"key":"partials_band_14","symbol":"band_14","label":"L14","name":"Partial 14 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":70,"key":"partials_band_15","symbol":"band_15","label":"L15","name":"Partial 15 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":71,"key":"partials_band_16","symbol":"band_16","label":"L16","name":"Partial 16 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":72,"key":"partials_band_17","symbol":"band_17","label":"L17","name":"Partial 17 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":73,"key":"partials_band_18","symbol":"band_18","label":"L18","name":"Partial 18 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":74,"key":"partials_band_19","symbol":"band_19","label":"L19","name":"Partial 19 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":75,"key":"partials_band_20","symbol":"band_20","label":"L20","name":"Partial 20 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":76,"key":"partials_band_21","symbol":"band_21","label":"L21","name":"Partial 21 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":77,"key":"partials_band_22","symbol":"band_22","label":"L22","name":"Partial 22 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":78,"key":"partials_band_23","symbol":"band_23","label":"L23","name":"Partial 23 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":79,"key":"partials_band_24","symbol":"band_24","label":"L24","name":"Partial 24 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":80,"key":"partials_band_25","symbol":"band_25","label":"L25","name":"Partial 25 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":81,"key":"partials_band_26","symbol":"band_26","label":"L26","name":"Partial 26 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":82,"key":"partials_band_27","symbol":"band_27","label":"L27","name":"Partial 27 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":83,"key":"partials_band_28","symbol":"band_28","label":"L28","name":"Partial 28 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":84,"key":"partials_band_29","symbol":"band_29","label":"L29","name":"Partial 29 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":85,"key":"partials_band_30","symbol":"band_30","label":"L30","name":"Partial 30 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":86,"key":"partials_band_31","symbol":"band_31","label":"L31","name":"Partial 31 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":87,"key":"partials_band_32","symbol":"band_32","label":"L32","name":"Partial 32 Level","min":-1,"max":1,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":88,"key":"partials_divisor_1","symbol":"divisor_1","label":"D1","name":"Subharmonic 1 Divider","min":1,"max":64,"default":1,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":89,"key":"partials_divisor_2","symbol":"divisor_2","label":"D2","name":"Subharmonic 2 Divider","min":1,"max":64,"default":2,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":90,"key":"partials_divisor_3","symbol":"divisor_3","label":"D3","name":"Subharmonic 3 Divider","min":1,"max":64,"default":3,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":91,"key":"partials_divisor_4","symbol":"divisor_4","label":"D4","name":"Subharmonic 4 Divider","min":1,"max":64,"default":4,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":92,"key":"partials_divisor_5","symbol":"divisor_5","label":"D5","name":"Subharmonic 5 Divider","min":1,"max":64,"default":5,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":93,"key":"partials_divisor_6","symbol":"divisor_6","label":"D6","name":"Subharmonic 6 Divider","min":1,"max":64,"default":6,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":94,"key":"partials_divisor_7","symbol":"divisor_7","label":"D7","name":"Subharmonic 7 Divider","min":1,"max":64,"default":7,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":95,"key":"partials_divisor_8","symbol":"divisor_8","label":"D8","name":"Subharmonic 8 Divider","min":1,"max":64,"default":8,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":96,"key":"partials_divisor_9","symbol":"divisor_9","label":"D9","name":"Subharmonic 9 Divider","min":1,"max":64,"default":9,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":97,"key":"partials_divisor_10","symbol":"divisor_10","label":"D10","name":"Subharmonic 10 Divider","min":1,"max":64,"default":10,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":98,"key":"partials_divisor_11","symbol":"divisor_11","label":"D11","name":"Subharmonic 11 Divider","min":1,"max":64,"default":11,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":99,"key":"partials_divisor_12","symbol":"divisor_12","label":"D12","name":"Subharmonic 12 Divider","min":1,"max":64,"default":12,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":100,"key":"partials_divisor_13","symbol":"divisor_13","label":"D13","name":"Subharmonic 13 Divider","min":1,"max":64,"default":13,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":101,"key":"partials_divisor_14","symbol":"divisor_14","label":"D14","name":"Subharmonic 14 Divider","min":1,"max":64,"default":14,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":102,"key":"partials_divisor_15","symbol":"divisor_15","label":"D15","name":"Subharmonic 15 Divider","min":1,"max":64,"default":15,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"},{"index":103,"key":"partials_divisor_16","symbol":"divisor_16","label":"D16","name":"Subharmonic 16 Divider","min":1,"max":64,"default":16,"step":0.01,"options":null,"unit":null,"page":"PARTIALS BANDS"}];
Object.assign(PARAMS,{"partials_type":{"key":"partials_type","label":"TYPE","shortLabel":"TYPE","min":0,"max":2,"value":0,"step":1,"integer":true,"labels":["BANK","SUB","PACKET"]},"partials_tune":{"key":"partials_tune","label":"TUNE","shortLabel":"TUNE","min":-48,"max":48,"value":0,"step":0.1,"integer":false,"unit":"st"},"partials_width":{"key":"partials_width","label":"WID","shortLabel":"WID","min":0,"max":100,"value":35,"step":1,"integer":false,"unit":"%"},"partials_level":{"key":"partials_level","label":"LVL","shortLabel":"LVL","min":0,"max":100,"value":80,"step":1,"integer":false,"unit":"%"},"partials_count":{"key":"partials_count","label":"PART","shortLabel":"PART","min":1,"max":32,"value":16,"step":0.1,"integer":false},"partials_tilt":{"key":"partials_tilt","label":"TILT","shortLabel":"TILT","min":-24,"max":12,"value":0,"step":0.5,"integer":false,"unit":"dB"},"partials_stretch":{"key":"partials_stretch","label":"STRC","shortLabel":"STRC","min":0.25,"max":4,"value":1,"step":0.01,"integer":false},"partials_phase":{"key":"partials_phase","label":"PHAS","shortLabel":"PHAS","min":0,"max":100,"value":0,"step":1,"integer":false,"unit":"%"},"partials_harmonics":{"key":"partials_harmonics","label":"HARM","shortLabel":"HARM","min":1,"max":16,"value":8,"step":0.1,"integer":false},"partials_subs":{"key":"partials_subs","label":"SUBS","shortLabel":"SUBS","min":0,"max":16,"value":4,"step":0.1,"integer":false},"partials_balance":{"key":"partials_balance","label":"BAL","shortLabel":"BAL","min":0,"max":100,"value":25,"step":1,"integer":false,"unit":"%"},"partials_detune":{"key":"partials_detune","label":"DETN","shortLabel":"DETN","min":0,"max":30,"value":0,"step":0.1,"integer":false,"unit":"cent"},"partials_drift":{"key":"partials_drift","label":"DRFT","shortLabel":"DRFT","min":0,"max":30,"value":0,"step":0.1,"integer":false,"unit":"cent"},"partials_drift_rate":{"key":"partials_drift_rate","label":"RATE","shortLabel":"RATE","min":0.01,"max":20,"value":0.5,"step":0.01,"integer":false,"unit":"Hz"},"partials_damping":{"key":"partials_damping","label":"DAMP","shortLabel":"DAMP","min":0,"max":100,"value":0,"step":1,"integer":false,"unit":"%"},"partials_time":{"key":"partials_time","label":"TIME","shortLabel":"TIME","min":10,"max":12000,"value":2000,"step":1,"integer":false,"unit":"ms"},"partials_gap":{"key":"partials_gap","label":"GAP","shortLabel":"GAP","min":10,"max":2000,"value":150,"step":1,"integer":false,"unit":"ms"},"partials_scatter":{"key":"partials_scatter","label":"SCAT","shortLabel":"SCAT","min":0,"max":100,"value":20,"step":1,"integer":false,"unit":"%"},"partials_rise":{"key":"partials_rise","label":"RISE","shortLabel":"RISE","min":0.5,"max":200,"value":10,"step":0.1,"integer":false,"unit":"ms"},"partials_sustain":{"key":"partials_sustain","label":"SUS","shortLabel":"SUS","min":0,"max":1000,"value":20,"step":1,"integer":false,"unit":"ms"},"partials_decay":{"key":"partials_decay","label":"DEC","shortLabel":"DEC","min":1,"max":4000,"value":120,"step":1,"integer":false,"unit":"ms"},"partials_variation":{"key":"partials_variation","label":"VAR","shortLabel":"VAR","min":0,"max":100,"value":20,"step":1,"integer":false,"unit":"%"},"partials_pitch_jitter":{"key":"partials_pitch_jitter","label":"PJIT","shortLabel":"PJIT","min":0,"max":12,"value":0,"step":0.1,"integer":false,"unit":"st"},"partials_shape":{"key":"partials_shape","label":"SHAP","shortLabel":"SHAP","min":0.25,"max":4,"value":1,"step":0.01,"integer":false},"partials_ratio_1":{"key":"partials_ratio_1","label":"R1","shortLabel":"R1","min":1,"max":64,"value":1,"step":0.01,"integer":false},"partials_ratio_2":{"key":"partials_ratio_2","label":"R2","shortLabel":"R2","min":1,"max":64,"value":2,"step":0.01,"integer":false},"partials_ratio_3":{"key":"partials_ratio_3","label":"R3","shortLabel":"R3","min":1,"max":64,"value":3,"step":0.01,"integer":false},"partials_ratio_4":{"key":"partials_ratio_4","label":"R4","shortLabel":"R4","min":1,"max":64,"value":4,"step":0.01,"integer":false},"partials_ratio_5":{"key":"partials_ratio_5","label":"R5","shortLabel":"R5","min":1,"max":64,"value":5,"step":0.01,"integer":false},"partials_ratio_6":{"key":"partials_ratio_6","label":"R6","shortLabel":"R6","min":1,"max":64,"value":6,"step":0.01,"integer":false},"partials_ratio_7":{"key":"partials_ratio_7","label":"R7","shortLabel":"R7","min":1,"max":64,"value":7,"step":0.01,"integer":false},"partials_ratio_8":{"key":"partials_ratio_8","label":"R8","shortLabel":"R8","min":1,"max":64,"value":8,"step":0.01,"integer":false},"partials_ratio_9":{"key":"partials_ratio_9","label":"R9","shortLabel":"R9","min":1,"max":64,"value":9,"step":0.01,"integer":false},"partials_ratio_10":{"key":"partials_ratio_10","label":"R10","shortLabel":"R10","min":1,"max":64,"value":10,"step":0.01,"integer":false},"partials_ratio_11":{"key":"partials_ratio_11","label":"R11","shortLabel":"R11","min":1,"max":64,"value":11,"step":0.01,"integer":false},"partials_ratio_12":{"key":"partials_ratio_12","label":"R12","shortLabel":"R12","min":1,"max":64,"value":12,"step":0.01,"integer":false},"partials_ratio_13":{"key":"partials_ratio_13","label":"R13","shortLabel":"R13","min":1,"max":64,"value":13,"step":0.01,"integer":false},"partials_ratio_14":{"key":"partials_ratio_14","label":"R14","shortLabel":"R14","min":1,"max":64,"value":14,"step":0.01,"integer":false},"partials_ratio_15":{"key":"partials_ratio_15","label":"R15","shortLabel":"R15","min":1,"max":64,"value":15,"step":0.01,"integer":false},"partials_ratio_16":{"key":"partials_ratio_16","label":"R16","shortLabel":"R16","min":1,"max":64,"value":16,"step":0.01,"integer":false},"partials_ratio_17":{"key":"partials_ratio_17","label":"R17","shortLabel":"R17","min":1,"max":64,"value":17,"step":0.01,"integer":false},"partials_ratio_18":{"key":"partials_ratio_18","label":"R18","shortLabel":"R18","min":1,"max":64,"value":18,"step":0.01,"integer":false},"partials_ratio_19":{"key":"partials_ratio_19","label":"R19","shortLabel":"R19","min":1,"max":64,"value":19,"step":0.01,"integer":false},"partials_ratio_20":{"key":"partials_ratio_20","label":"R20","shortLabel":"R20","min":1,"max":64,"value":20,"step":0.01,"integer":false},"partials_ratio_21":{"key":"partials_ratio_21","label":"R21","shortLabel":"R21","min":1,"max":64,"value":21,"step":0.01,"integer":false},"partials_ratio_22":{"key":"partials_ratio_22","label":"R22","shortLabel":"R22","min":1,"max":64,"value":22,"step":0.01,"integer":false},"partials_ratio_23":{"key":"partials_ratio_23","label":"R23","shortLabel":"R23","min":1,"max":64,"value":23,"step":0.01,"integer":false},"partials_ratio_24":{"key":"partials_ratio_24","label":"R24","shortLabel":"R24","min":1,"max":64,"value":24,"step":0.01,"integer":false},"partials_ratio_25":{"key":"partials_ratio_25","label":"R25","shortLabel":"R25","min":1,"max":64,"value":25,"step":0.01,"integer":false},"partials_ratio_26":{"key":"partials_ratio_26","label":"R26","shortLabel":"R26","min":1,"max":64,"value":26,"step":0.01,"integer":false},"partials_ratio_27":{"key":"partials_ratio_27","label":"R27","shortLabel":"R27","min":1,"max":64,"value":27,"step":0.01,"integer":false},"partials_ratio_28":{"key":"partials_ratio_28","label":"R28","shortLabel":"R28","min":1,"max":64,"value":28,"step":0.01,"integer":false},"partials_ratio_29":{"key":"partials_ratio_29","label":"R29","shortLabel":"R29","min":1,"max":64,"value":29,"step":0.01,"integer":false},"partials_ratio_30":{"key":"partials_ratio_30","label":"R30","shortLabel":"R30","min":1,"max":64,"value":30,"step":0.01,"integer":false},"partials_ratio_31":{"key":"partials_ratio_31","label":"R31","shortLabel":"R31","min":1,"max":64,"value":31,"step":0.01,"integer":false},"partials_ratio_32":{"key":"partials_ratio_32","label":"R32","shortLabel":"R32","min":1,"max":64,"value":32,"step":0.01,"integer":false},"partials_band_1":{"key":"partials_band_1","label":"L1","shortLabel":"L1","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_2":{"key":"partials_band_2","label":"L2","shortLabel":"L2","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_3":{"key":"partials_band_3","label":"L3","shortLabel":"L3","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_4":{"key":"partials_band_4","label":"L4","shortLabel":"L4","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_5":{"key":"partials_band_5","label":"L5","shortLabel":"L5","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_6":{"key":"partials_band_6","label":"L6","shortLabel":"L6","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_7":{"key":"partials_band_7","label":"L7","shortLabel":"L7","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_8":{"key":"partials_band_8","label":"L8","shortLabel":"L8","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_9":{"key":"partials_band_9","label":"L9","shortLabel":"L9","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_10":{"key":"partials_band_10","label":"L10","shortLabel":"L10","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_11":{"key":"partials_band_11","label":"L11","shortLabel":"L11","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_12":{"key":"partials_band_12","label":"L12","shortLabel":"L12","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_13":{"key":"partials_band_13","label":"L13","shortLabel":"L13","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_14":{"key":"partials_band_14","label":"L14","shortLabel":"L14","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_15":{"key":"partials_band_15","label":"L15","shortLabel":"L15","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_16":{"key":"partials_band_16","label":"L16","shortLabel":"L16","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_17":{"key":"partials_band_17","label":"L17","shortLabel":"L17","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_18":{"key":"partials_band_18","label":"L18","shortLabel":"L18","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_19":{"key":"partials_band_19","label":"L19","shortLabel":"L19","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_20":{"key":"partials_band_20","label":"L20","shortLabel":"L20","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_21":{"key":"partials_band_21","label":"L21","shortLabel":"L21","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_22":{"key":"partials_band_22","label":"L22","shortLabel":"L22","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_23":{"key":"partials_band_23","label":"L23","shortLabel":"L23","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_24":{"key":"partials_band_24","label":"L24","shortLabel":"L24","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_25":{"key":"partials_band_25","label":"L25","shortLabel":"L25","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_26":{"key":"partials_band_26","label":"L26","shortLabel":"L26","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_27":{"key":"partials_band_27","label":"L27","shortLabel":"L27","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_28":{"key":"partials_band_28","label":"L28","shortLabel":"L28","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_29":{"key":"partials_band_29","label":"L29","shortLabel":"L29","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_30":{"key":"partials_band_30","label":"L30","shortLabel":"L30","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_31":{"key":"partials_band_31","label":"L31","shortLabel":"L31","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_band_32":{"key":"partials_band_32","label":"L32","shortLabel":"L32","min":-1,"max":1,"value":1,"step":0.01,"integer":false},"partials_divisor_1":{"key":"partials_divisor_1","label":"D1","shortLabel":"D1","min":1,"max":64,"value":1,"step":0.01,"integer":false},"partials_divisor_2":{"key":"partials_divisor_2","label":"D2","shortLabel":"D2","min":1,"max":64,"value":2,"step":0.01,"integer":false},"partials_divisor_3":{"key":"partials_divisor_3","label":"D3","shortLabel":"D3","min":1,"max":64,"value":3,"step":0.01,"integer":false},"partials_divisor_4":{"key":"partials_divisor_4","label":"D4","shortLabel":"D4","min":1,"max":64,"value":4,"step":0.01,"integer":false},"partials_divisor_5":{"key":"partials_divisor_5","label":"D5","shortLabel":"D5","min":1,"max":64,"value":5,"step":0.01,"integer":false},"partials_divisor_6":{"key":"partials_divisor_6","label":"D6","shortLabel":"D6","min":1,"max":64,"value":6,"step":0.01,"integer":false},"partials_divisor_7":{"key":"partials_divisor_7","label":"D7","shortLabel":"D7","min":1,"max":64,"value":7,"step":0.01,"integer":false},"partials_divisor_8":{"key":"partials_divisor_8","label":"D8","shortLabel":"D8","min":1,"max":64,"value":8,"step":0.01,"integer":false},"partials_divisor_9":{"key":"partials_divisor_9","label":"D9","shortLabel":"D9","min":1,"max":64,"value":9,"step":0.01,"integer":false},"partials_divisor_10":{"key":"partials_divisor_10","label":"D10","shortLabel":"D10","min":1,"max":64,"value":10,"step":0.01,"integer":false},"partials_divisor_11":{"key":"partials_divisor_11","label":"D11","shortLabel":"D11","min":1,"max":64,"value":11,"step":0.01,"integer":false},"partials_divisor_12":{"key":"partials_divisor_12","label":"D12","shortLabel":"D12","min":1,"max":64,"value":12,"step":0.01,"integer":false},"partials_divisor_13":{"key":"partials_divisor_13","label":"D13","shortLabel":"D13","min":1,"max":64,"value":13,"step":0.01,"integer":false},"partials_divisor_14":{"key":"partials_divisor_14","label":"D14","shortLabel":"D14","min":1,"max":64,"value":14,"step":0.01,"integer":false},"partials_divisor_15":{"key":"partials_divisor_15","label":"D15","shortLabel":"D15","min":1,"max":64,"value":15,"step":0.01,"integer":false},"partials_divisor_16":{"key":"partials_divisor_16","label":"D16","shortLabel":"D16","min":1,"max":64,"value":16,"step":0.01,"integer":false}});
const PARTIALS_PAGE={title:"PARTIALS",partials:true,keys:["partials_type", "partials_tune", "partials_width", "partials_level", "partials_count", "partials_tilt", "partials_stretch", "partials_phase"]};
const PARTIALS_TONE_PAGE={title:"PARTIALS TONE",partials:true,keys:["partials_harmonics", "partials_subs", "partials_balance", "partials_detune", "partials_drift", "partials_drift_rate", "partials_damping", "partials_time"]};
const PARTIALS_PACKET_PAGE={title:"PARTIALS PACKET",partials:true,keys:["partials_gap", "partials_scatter", "partials_rise", "partials_sustain", "partials_decay", "partials_variation", "partials_pitch_jitter", "partials_shape"]};
const PARTIALS_BANDS_PAGE={title:"PARTIALS BANDS",partials:true,partialsBands:true,keys:["partials_ratio_1", "partials_ratio_2", "partials_ratio_3", "partials_ratio_4", "partials_band_1", "partials_band_2", "partials_band_3", "partials_band_4"]};
const partialsUi={data:null,floor:0,group:0};
function readPartials(cached=uiPolling.cached){let data;try{data=JSON.parse(String(cached?uiPolling.cache.get('partials_state'):getParam('partials_state','{}')));}catch(_){return;}
 if(!data||data.part!==(state.splitEnabled?state.splitFocus:0)||!Number.isInteger(data.edit)||data.edit<0||data.edit>3||!Array.isArray(data.patches)||data.patches.length!==4||data.patches.some(p=>!Array.isArray(p)||p.length!==104||p.some(x=>!Number.isFinite(x)))||data.revision<partialsUi.floor)return;
 const expected=state.splitEnabled&&!state.sharedDual?(isQuad()?state.splitFocus:Math.floor(clamp(state.lane-1,state.splitFocus*2,state.splitFocus*2+1))):state.lane-1;if(data.edit!==expected)return;
 partialsUi.data=data;partialsUi.group=clamp(data.group||0,0,7);if(!cached)partialsUi.floor=data.revision;
 PARTIALS_CONTROLS.forEach(p=>PARAMS[p.key].value=p.index===4&&data.patches[data.edit][0]===2?Math.min(8,data.patches[data.edit][4]):data.patches[data.edit][p.index]);
}
function partialsBandsPage(){const g=partialsUi.group,t=sequenceSoundValue('partials_type',PARAMS.partials_type.value);return {...PARTIALS_BANDS_PAGE,title:'PART '+(g*4+1)+'-'+(g*4+4),keys:[0,1,2,3].map(i=>t===1&&g>=4?'partials_divisor_'+(g*4+i-15):'partials_ratio_'+(g*4+i+1)).concat([0,1,2,3].map(i=>'partials_band_'+(g*4+i+1)))};}
const LOWFORM_CONTROLS=[{"index":0,"key":"lowform_body","symbol":"body","label":"BODY","name":"Lowform Body","min":0,"max":5,"default":0,"step":1,"options":["PRESS","SWARM","ACID","RAVE","THROAT","SYNC"],"unit":null,"curve":null,"page":"LOWFORM"},{"index":1,"key":"lowform_tune","symbol":"tune","label":"TUNE","name":"Lowform Transpose","min":-48,"max":48,"default":0,"step":0.1,"options":null,"unit":"st","curve":null,"page":"LOWFORM"},{"index":2,"key":"lowform_width","symbol":"width","label":"WID","name":"Lowform Stereo Width","min":0,"max":100,"default":45,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM"},{"index":3,"key":"lowform_level","symbol":"level","label":"LVL","name":"Lowform Level","min":0,"max":100,"default":80,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM"},{"index":4,"key":"lowform_foundation_wave","symbol":"foundation_wave","label":"FWAV","name":"Foundation Wave","min":0,"max":2,"default":0,"step":1,"options":["SINE","TRI","ROUND"],"unit":null,"curve":null,"page":"LOWFORM SHAPE"},{"index":5,"key":"lowform_foundation_octave","symbol":"foundation_octave","label":"FOCT","name":"Foundation Octave","min":-1,"max":0,"default":0,"step":1,"options":null,"unit":null,"curve":null,"page":"LOWFORM SHAPE"},{"index":6,"key":"lowform_foundation_level","symbol":"foundation_level","label":"FND","name":"Foundation Level","min":0,"max":100,"default":80,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM"},{"index":7,"key":"lowform_body_level","symbol":"body_level","label":"BODL","name":"Body Level","min":0,"max":100,"default":65,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM"},{"index":8,"key":"lowform_a","symbol":"a","label":"WAVE","name":"Body Control A","min":0,"max":100,"default":42,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM BODY"},{"index":9,"key":"lowform_b","symbol":"b","label":"PRES","name":"Body Control B","min":0,"max":100,"default":48,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM BODY"},{"index":10,"key":"lowform_c","symbol":"c","label":"PW","name":"Body Control C","min":0,"max":100,"default":40,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM BODY"},{"index":11,"key":"lowform_d","symbol":"d","label":"CROSS","name":"Body Control D","min":0,"max":100,"default":24,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM BODY"},{"index":12,"key":"lowform_e","symbol":"e","label":"DRIVE","name":"Body Control E","min":0,"max":100,"default":45,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM BODY"},{"index":13,"key":"lowform_body_decay","symbol":"body_decay","label":"BDEC","name":"Body Decay","min":20,"max":12000,"default":12000,"step":1,"options":null,"unit":"ms","curve":null,"page":"LOWFORM BODY"},{"index":14,"key":"lowform_texture_type","symbol":"texture_type","label":"TYPE","name":"Texture Type","min":0,"max":4,"default":0,"step":1,"options":["OFF","NOISE","CORR","RING","WIRE"],"unit":null,"curve":null,"page":"LOWFORM TEXTURE"},{"index":15,"key":"lowform_texture_level","symbol":"texture_level","label":"LVL","name":"Texture Level","min":0,"max":100,"default":0,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM TEXTURE"},{"index":16,"key":"lowform_texture_color","symbol":"texture_color","label":"COLR","name":"Texture Colour","min":0,"max":100,"default":52,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM TEXTURE"},{"index":17,"key":"lowform_texture_track","symbol":"texture_track","label":"TRK","name":"Texture Pitch Track","min":0,"max":100,"default":72,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM TEXTURE"},{"index":18,"key":"lowform_texture_width","symbol":"texture_width","label":"TWID","name":"Texture Width","min":0,"max":100,"default":45,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM TEXTURE"},{"index":19,"key":"lowform_filter_type","symbol":"filter_type","label":"FILT","name":"Filter Type","min":0,"max":5,"default":2,"step":1,"options":["OFF","LP12","LP24","BP","HP","NOTCH"],"unit":null,"curve":null,"page":"LOWFORM SHAPE"},{"index":20,"key":"lowform_cutoff","symbol":"cutoff","label":"CUT","name":"Filter Cutoff","min":20,"max":18000,"default":1200,"step":1,"options":null,"unit":"Hz","curve":null,"page":"LOWFORM"},{"index":21,"key":"lowform_resonance","symbol":"resonance","label":"RES","name":"Filter Resonance","min":0,"max":95,"default":18,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM"},{"index":22,"key":"lowform_filter_drive","symbol":"filter_drive","label":"FDRV","name":"Filter Drive","min":0,"max":100,"default":25,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM SHAPE"},{"index":23,"key":"lowform_keytrack","symbol":"keytrack","label":"KEY","name":"Filter Key Track","min":-100,"max":100,"default":34,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM SHAPE"},{"index":24,"key":"lowform_filter_env","symbol":"filter_env","label":"FENV","name":"Filter Envelope","min":-8,"max":8,"default":1.4,"step":0.05,"options":null,"unit":"oct","curve":null,"page":"LOWFORM SHAPE"},{"index":25,"key":"lowform_filter_decay","symbol":"filter_decay","label":"FDEC","name":"Filter Envelope Decay","min":5,"max":4000,"default":180,"step":1,"options":null,"unit":"ms","curve":null,"page":"LOWFORM SHAPE"},{"index":26,"key":"lowform_punch","symbol":"punch","label":"PUNC","name":"Pitch Punch","min":-24,"max":48,"default":3,"step":0.1,"options":null,"unit":"st","curve":null,"page":"LOWFORM BODY"},{"index":27,"key":"lowform_punch_time","symbol":"punch_time","label":"PTIM","name":"Pitch Punch Time","min":1,"max":1000,"default":48,"step":1,"options":null,"unit":"ms","curve":null,"page":"LOWFORM BODY"},{"index":28,"key":"lowform_drive","symbol":"drive","label":"DRIV","name":"Lowform Overload","min":0,"max":100,"default":0,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM TEXTURE"},{"index":29,"key":"lowform_low_return","symbol":"low_return","label":"LOW","name":"Foundation Low Return","min":0,"max":100,"default":72,"step":1,"options":null,"unit":"%","curve":null,"page":"LOWFORM SHAPE"},{"index":30,"key":"lowform_texture_decay","symbol":"texture_decay","label":"TDEC","name":"Texture Decay","min":20,"max":12000,"default":12000,"step":1,"options":null,"unit":"ms","curve":null,"page":"LOWFORM TEXTURE"},{"index":31,"key":"lowform_texture_attack","symbol":"texture_attack","label":"TATK","name":"Texture Attack","min":0,"max":2000,"default":0,"step":1,"options":null,"unit":"ms","curve":null,"page":"LOWFORM TEXTURE"}];
Object.assign(PARAMS,{"lowform_body":{"key":"lowform_body","label":"BODY","shortLabel":"BODY","min":0,"max":5,"value":0,"step":1,"integer":true,"labels":["PRESS","SWARM","ACID","RAVE","THROAT","SYNC"]},"lowform_tune":{"key":"lowform_tune","label":"TUNE","shortLabel":"TUNE","min":-48,"max":48,"value":0,"step":0.1,"integer":false,"unit":"st"},"lowform_width":{"key":"lowform_width","label":"WID","shortLabel":"WID","min":0,"max":100,"value":45,"step":1,"integer":false,"unit":"%"},"lowform_level":{"key":"lowform_level","label":"LVL","shortLabel":"LVL","min":0,"max":100,"value":80,"step":1,"integer":false,"unit":"%"},"lowform_foundation_wave":{"key":"lowform_foundation_wave","label":"FWAV","shortLabel":"FWAV","min":0,"max":2,"value":0,"step":1,"integer":true,"labels":["SINE","TRI","ROUND"]},"lowform_foundation_octave":{"key":"lowform_foundation_octave","label":"FOCT","shortLabel":"FOCT","min":-1,"max":0,"value":0,"step":1,"integer":true},"lowform_foundation_level":{"key":"lowform_foundation_level","label":"FND","shortLabel":"FND","min":0,"max":100,"value":80,"step":1,"integer":false,"unit":"%"},"lowform_body_level":{"key":"lowform_body_level","label":"BODL","shortLabel":"BODL","min":0,"max":100,"value":65,"step":1,"integer":false,"unit":"%"},"lowform_a":{"key":"lowform_a","label":"WAVE","shortLabel":"WAVE","min":0,"max":100,"value":42,"step":1,"integer":false,"unit":"%"},"lowform_b":{"key":"lowform_b","label":"PRES","shortLabel":"PRES","min":0,"max":100,"value":48,"step":1,"integer":false,"unit":"%"},"lowform_c":{"key":"lowform_c","label":"PW","shortLabel":"PW","min":0,"max":100,"value":40,"step":1,"integer":false,"unit":"%"},"lowform_d":{"key":"lowform_d","label":"CROSS","shortLabel":"CROSS","min":0,"max":100,"value":24,"step":1,"integer":false,"unit":"%"},"lowform_e":{"key":"lowform_e","label":"DRIVE","shortLabel":"DRIVE","min":0,"max":100,"value":45,"step":1,"integer":false,"unit":"%"},"lowform_body_decay":{"key":"lowform_body_decay","label":"BDEC","shortLabel":"BDEC","min":20,"max":12000,"value":12000,"step":1,"integer":false,"unit":"ms"},"lowform_texture_type":{"key":"lowform_texture_type","label":"TYPE","shortLabel":"TYPE","min":0,"max":4,"value":0,"step":1,"integer":true,"labels":["OFF","NOISE","CORR","RING","WIRE"]},"lowform_texture_level":{"key":"lowform_texture_level","label":"LVL","shortLabel":"LVL","min":0,"max":100,"value":0,"step":1,"integer":false,"unit":"%"},"lowform_texture_color":{"key":"lowform_texture_color","label":"COLR","shortLabel":"COLR","min":0,"max":100,"value":52,"step":1,"integer":false,"unit":"%"},"lowform_texture_track":{"key":"lowform_texture_track","label":"TRK","shortLabel":"TRK","min":0,"max":100,"value":72,"step":1,"integer":false,"unit":"%"},"lowform_texture_width":{"key":"lowform_texture_width","label":"TWID","shortLabel":"TWID","min":0,"max":100,"value":45,"step":1,"integer":false,"unit":"%"},"lowform_filter_type":{"key":"lowform_filter_type","label":"FILT","shortLabel":"FILT","min":0,"max":5,"value":2,"step":1,"integer":true,"labels":["OFF","LP12","LP24","BP","HP","NOTCH"]},"lowform_cutoff":{"key":"lowform_cutoff","label":"CUT","shortLabel":"CUT","min":20,"max":18000,"value":1200,"step":1,"integer":false,"unit":"Hz"},"lowform_resonance":{"key":"lowform_resonance","label":"RES","shortLabel":"RES","min":0,"max":95,"value":18,"step":1,"integer":false,"unit":"%"},"lowform_filter_drive":{"key":"lowform_filter_drive","label":"FDRV","shortLabel":"FDRV","min":0,"max":100,"value":25,"step":1,"integer":false,"unit":"%"},"lowform_keytrack":{"key":"lowform_keytrack","label":"KEY","shortLabel":"KEY","min":-100,"max":100,"value":34,"step":1,"integer":false,"unit":"%"},"lowform_filter_env":{"key":"lowform_filter_env","label":"FENV","shortLabel":"FENV","min":-8,"max":8,"value":1.4,"step":0.05,"integer":false,"unit":"oct"},"lowform_filter_decay":{"key":"lowform_filter_decay","label":"FDEC","shortLabel":"FDEC","min":5,"max":4000,"value":180,"step":1,"integer":false,"unit":"ms"},"lowform_punch":{"key":"lowform_punch","label":"PUNC","shortLabel":"PUNC","min":-24,"max":48,"value":3,"step":0.1,"integer":false,"unit":"st"},"lowform_punch_time":{"key":"lowform_punch_time","label":"PTIM","shortLabel":"PTIM","min":1,"max":1000,"value":48,"step":1,"integer":false,"unit":"ms"},"lowform_drive":{"key":"lowform_drive","label":"DRIV","shortLabel":"DRIV","min":0,"max":100,"value":0,"step":1,"integer":false,"unit":"%"},"lowform_low_return":{"key":"lowform_low_return","label":"LOW","shortLabel":"LOW","min":0,"max":100,"value":72,"step":1,"integer":false,"unit":"%"},"lowform_texture_decay":{"key":"lowform_texture_decay","label":"TDEC","shortLabel":"TDEC","min":20,"max":12000,"value":12000,"step":1,"integer":false,"unit":"ms"},"lowform_texture_attack":{"key":"lowform_texture_attack","label":"TATK","shortLabel":"TATK","min":0,"max":2000,"value":0,"step":1,"integer":false,"unit":"ms"}});
const LOWFORM_PAGE={title:"LOWFORM",lowform:true,keys:["lowform_body", "lowform_tune", "lowform_foundation_level", "lowform_body_level", "lowform_cutoff", "lowform_resonance", "lowform_width", "lowform_level"]};
const LOWFORM_BODY_PAGE={title:"LOWFORM BODY",lowform:true,keys:["lowform_a", "lowform_b", "lowform_c", "lowform_d", "lowform_e", "lowform_body_decay", "lowform_punch", "lowform_punch_time"]};
const LOWFORM_SHAPE_PAGE={title:"LOWFORM SHAPE",lowform:true,keys:["lowform_filter_type", "lowform_filter_drive", "lowform_keytrack", "lowform_filter_env", "lowform_filter_decay", "lowform_foundation_wave", "lowform_foundation_octave", "lowform_low_return"]};
const LOWFORM_TEXTURE_PAGE={title:"LOWFORM TEXTURE",lowform:true,keys:["lowform_texture_type", "lowform_texture_level", "lowform_texture_color", "lowform_texture_track", "lowform_texture_width", "lowform_texture_attack", "lowform_texture_decay", "lowform_drive"]};
const lowformUi={data:null,floor:0,startsOpen:false,start:0,armed:false,renderKey:null};
function readLowform(cached=uiPolling.cached){let data;try{data=JSON.parse(String(cached?uiPolling.cache.get('lowform_state'):getParam('lowform_state','{}')));}catch(_){return;}
 if(!data||data.part!==(state.splitEnabled?state.splitFocus:0)||!Number.isInteger(data.edit)||data.edit<0||data.edit>3||!Array.isArray(data.patches)||data.patches.length!==4||data.patches.some(p=>!Array.isArray(p)||p.length!==32||p.some(x=>!Number.isFinite(x)))||data.revision<lowformUi.floor)return;
 const expected=state.splitEnabled&&!state.sharedDual?(isQuad()?state.splitFocus:Math.floor(clamp(state.lane-1,state.splitFocus*2,state.splitFocus*2+1))):state.lane-1;
 if(data.edit!==expected)return;
 lowformUi.data=data;if(!cached)lowformUi.floor=data.revision;
 LOWFORM_CONTROLS.forEach(p=>PARAMS[p.key].value=data.patches[data.edit][p.index]);
}

const LOWFORM_STARTS=['PRESSURE','SWARM','ACID','RAVE','THROAT','SYNC'];
function handleLowformMidi(status,number,value){
 const active=state.view==='settings'&&Boolean(currentSettingsPage().lowform)&&!state.performanceVisible;
 if(lowformUi.startsOpen&&!active)lowformUi.startsOpen=false;
 if(status!==0xb0)return false;
 if(!lowformUi.startsOpen){if(number===MoveMainButton&&value>0&&active&&!state.shiftHeld&&!state.focusButtonHeld){if(sequenceSoundActive()){showActionMessage('LANE SETTING');draw();return true;}lowformUi.startsOpen=true;lowformUi.start=0;lowformUi.armed=false;lowformUi.renderKey=null;draw();return true;}return false;}
 if(number===MoveMainKnob){lowformUi.start=clamp(lowformUi.start+decodeDelta(value),0,LOWFORM_STARTS.length-1);draw();return true;}
 if([MoveUp,MoveDown,MoveLeft,MoveRight].includes(number)){if(value>0){lowformUi.start=clamp(lowformUi.start+([MoveUp,MoveLeft].includes(number)?-1:1),0,LOWFORM_STARTS.length-1);draw();}return true;}
 if(number===MoveBack||number===MoveMenu){if(value>0){lowformUi.startsOpen=false;draw();}return true;}
 if(number===MoveMainButton){if(value<=0)lowformUi.armed=true;else if(lowformUi.armed){setParamBlocking('lowform_start',lowformUi.start+1);lowformUi.startsOpen=false;readLowform(false);draw();}return true;}
 if(number>=MoveKnob1&&number<MoveKnob1+8)return true;return false;
}
function drawLowformStarts(){const signature=[lowformUi.start,lowformUi.data?.edit,state.splitFocus,state.saveOverlayLast].join(':');if(signature===lowformUi.renderKey)return;lowformUi.renderKey=signature;clearDisplay();drawSettingsTitle('LOWFORM START L'+((lowformUi.data?.edit??state.lane-1)+1));drawRect(0,8,128,1,1);const first=clamp(lowformUi.start-1,0,LOWFORM_STARTS.length-4);for(let row=0;row<4;++row){const i=first+row,y=13+row*10;if(i===lowformUi.start)drawRect(0,y-1,128,9,1);print(3,y,LOWFORM_STARTS[i],i===lowformUi.start?0:1);}print(1,57,'CLICK LOAD BACK',1);}

const LOWFORM_BODY_LABELS=[['WAVE','PRES','PW','CROS','DRIV'],['DRIV','DETN','VOIC','DRFT','TONE'],['WAVE','RES','PW','DRIV','CUT'],['WAVE','SUB','SMER','NOIS','DRIV'],['VOWL','SHFT','FOCS','SRC','DRIV'],['WAVE','RATI','SWP','DEC','RING']];
const TABLE_MAIN_PAGE={title:'TABLE',table:true,keys:['table_type','table_tune','table_width','table_level','table_art','table_decay','table_noise','table_drive']};
const TABLE_WAVE_PAGE={title:'TABLE WAVE',table:true,keys:['table_shape','table_pulse','table_detune','table_sub','table_cutoff','table_resonance',null,null]};
const TABLE_SPLINE_PAGE={title:'TABLE SPLINE',table:true,keys:['table_points','table_evolution','table_curve','table_drift','table_drift_rate','table_dynamics','table_seed',null]};
const TABLE_OSC_PAGE={title:'TABLE WT OSC',table:true,keys:['table_model','table_bank_a','table_pos_a','table_bank_b','table_pos_b','table_blend','table_ratio','table_fm']};
const TABLE_HIT_PAGE={title:'TABLE WT HIT',table:true,keys:['table_bend','table_pitch_decay','table_scan','table_scan_decay','table_wt_sub','table_impact','table_noise_decay','table_color']};
const TABLE_COLOR_PAGE={title:'TABLE WT COLOR',table:true,keys:['table_wt_detune','table_tone','table_compression',null,null,null,null,null]};
const tableUi={data:null,scopes:['','','',''],mask:0,revision:-1,floor:0,scopeKey:''};
function readTable(cached=uiPolling.cached){let data;try{data=JSON.parse(String(cached?uiPolling.cache.get('table_state'):getParam('table_state','{}')));}catch(_){return;}
 if(!data||data.part!==(state.splitEnabled?state.splitFocus:0)||!Number.isInteger(data.edit)||data.edit<0||data.edit>3||!Array.isArray(data.patches)||data.patches.length!==4||data.patches.some(p=>!Array.isArray(p)||p.length!==40||p.some(x=>!Number.isFinite(x)))||data.revision<tableUi.floor)return;
 const expected=state.splitEnabled&&!state.sharedDual?(isQuad()?state.splitFocus:Math.floor(clamp(state.lane-1,state.splitFocus*2,state.splitFocus*2+1))):state.lane-1;
 if(data.edit!==expected)return;
 tableUi.data=data;if(!cached)tableUi.floor=data.revision;
 TABLE_CONTROLS.forEach(p=>PARAMS[p.key].value=data.patches[data.edit][p.index]);
}
function readTableScope(cached=uiPolling.cached){let data;try{data=JSON.parse(String(cached?uiPolling.cache.get('table_scope'):getParam('table_scope','{}')));}catch(_){return;}
 if(!data||data.mode!==currentPerformanceMode()||(data.focus??-1)!==(state.bankMode?0:state.splitEnabled&&state.sharedDual?state.splitFocus:-1)||!Number.isInteger(data.mask)||!Array.isArray(data.waves)||data.waves.length!==4||data.waves.some(w=>typeof w!=='string'||!(/^[0-9a-e]{128}$/).test(w)))return;
 tableUi.mask=data.mask;tableUi.scopes=data.waves;tableUi.scopeKey=data.waves.join(':')+data.mask;
}
function drawTableScope(lane,center,left,width){const text=tableUi.scopes[lane];if(!text||text.length!==128)return false;
 let previous=null;
 for(let x=0;x<width;++x){const at=Math.min(63,Math.round(x*63/Math.max(1,width-1)))*2,low=parseInt(text[at],16)-7,high=parseInt(text[at+1],16)-7;
   const top=center-Math.round(high*4/7),bottom=center-Math.round(low*4/7);const mid=Math.round((top+bottom)/2),from=previous===null?top:Math.min(top,previous),to=previous===null?bottom:Math.max(bottom,previous);
   drawRect(left+x,from,1,Math.max(1,to-from+1),1);previous=mid;}
 for(let v=0;v<state.stackPositions.length;++v)if((!state.sharedDual||state.bankMode||(v<4?0:1)===state.splitFocus)&&isSynthAlgorithm(state.voiceAlgorithms?.[v])&&state.stackPositions[v]>=0){const position=state.stackPositions[v]*3;
   if(Math.abs(position-lane)<=.6){const y=center+Math.round((position-lane)*10);drawRect(left+width-2-(v%4)*3,y-1,2,2,1);}}
 return true;
}

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
    [STACK_PATH_PAGE,{title:'PULSAR',pulsar:true,keys:["pulsar_rate","pulsar_form","pulsar_size","pulsar_window","pulsar_skew","pulsar_omit","pulsar_burst","pulsar_jitter"]},{title:'PULSAR MOD',pulsar:true,keys:["pulsar_emit_mod_rate","pulsar_emit_mod_depth","pulsar_form_mod_rate","pulsar_form_mod_depth","pulsar_spray","pulsar_form_variation","pulsar_reverse","pulsar_keytrack"]}],
    [STACK_PATH_PAGE,TABLE_MAIN_PAGE,TABLE_WAVE_PAGE,TABLE_SPLINE_PAGE,TABLE_OSC_PAGE,TABLE_HIT_PAGE,TABLE_COLOR_PAGE],
    [STACK_PATH_PAGE,SCANNED_PAGE,SCANNED_BODY_PAGE,SCANNED_HIT_PAGE,SCANNED_AMP_PAGE],
    [STACK_PATH_PAGE,FM_PAGE,FM_COLOR_PAGE,FM_HIT_PAGE,FM_ENVS_PAGE],
    [STACK_PATH_PAGE,NOISE_PAGE,NOISE_BANK_PAGE,NOISE_EVENTS_PAGE,NOISE_BANDS_PAGE],
    [STACK_PATH_PAGE,RESON_PAGE,RESON_BODY_PAGE,RESON_HIT_PAGE,RESON_DRIVE_PAGE],
    [STACK_PATH_PAGE,PARTIALS_PAGE,PARTIALS_TONE_PAGE,PARTIALS_PACKET_PAGE,PARTIALS_BANDS_PAGE],
    [STACK_PATH_PAGE,LOWFORM_PAGE,LOWFORM_BODY_PAGE,LOWFORM_SHAPE_PAGE,LOWFORM_TEXTURE_PAGE]

];

function playbackPagesForAlgorithm(algorithm, motionSound) {
    if(algorithm===21)return [STACK_PATH_PAGE,LOWFORM_PAGE,LOWFORM_BODY_PAGE,LOWFORM_SHAPE_PAGE,LOWFORM_TEXTURE_PAGE];
    if(algorithm===20)return [STACK_PATH_PAGE,PARTIALS_PAGE,PARTIALS_TONE_PAGE,PARTIALS_PACKET_PAGE,partialsBandsPage()];
    if(algorithm===19)return [STACK_PATH_PAGE,RESON_PAGE,RESON_BODY_PAGE,RESON_HIT_PAGE,RESON_DRIVE_PAGE];
    if(algorithm===18)return [STACK_PATH_PAGE,NOISE_PAGE,NOISE_BANK_PAGE,NOISE_EVENTS_PAGE,noiseBandsPage()];
    if(algorithm===17)return [STACK_PATH_PAGE,FM_PAGE,FM_COLOR_PAGE,FM_HIT_PAGE,FM_ENVS_PAGE];
    if(algorithm===16)return [STACK_PATH_PAGE,SCANNED_PAGE,SCANNED_BODY_PAGE,SCANNED_HIT_PAGE,SCANNED_AMP_PAGE];
    if(algorithm===15)return [STACK_PATH_PAGE,TABLE_MAIN_PAGE,...(Math.round(sequenceSoundValue('table_type',PARAMS.table_type.value))===0?[TABLE_WAVE_PAGE]:Math.round(sequenceSoundValue('table_type',PARAMS.table_type.value))===1?[TABLE_SPLINE_PAGE]:[TABLE_OSC_PAGE,TABLE_HIT_PAGE,TABLE_COLOR_PAGE])];
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
const SPLIT_MIX_PAGE = {title:'FX SENDS',global:true,keys:[...SPLIT_SEND_KEYS,'split_duck_route',null,'split_curve','split_blend']};
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
    'tempo_swing', 'pattern_fade_lookahead', 'clip_clear_recording'] };
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

/* SEQUENCE_SOUND_CATALOG_BEGIN */
const SEQUENCE_SOUND_SCOPES={"1":"voice","2":"voice","5":"voice","6":"lane","7":"lane","8":"lane","256":"voice","257":"voice","258":"voice","259":"voice","385":"voice","394":"voice","395":"voice","512":"voice","513":"voice","514":"voice","515":"voice","516":"voice","517":"voice","518":"voice","519":"voice","520":"voice","521":"voice","522":"voice","523":"voice","524":"voice","525":"voice","526":"voice","527":"voice","528":"voice","529":"voice","530":"voice","531":"voice","532":"voice","533":"voice","640":"voice","641":"voice","642":"voice","644":"voice","645":"voice","646":"voice","647":"voice","648":"voice","649":"voice","650":"voice","651":"voice","652":"voice","653":"voice","654":"voice","655":"voice","656":"voice","657":"voice","658":"voice","659":"voice","768":"voice","769":"voice","770":"voice","771":"voice","772":"voice","773":"voice","774":"voice","775":"voice","776":"voice","777":"voice","778":"voice","779":"voice","780":"voice","781":"voice","782":"voice","896":"voice","897":"voice","898":"voice","899":"voice","900":"voice","901":"voice","902":"voice","903":"voice","904":"voice","905":"voice","906":"voice","907":"voice","908":"voice","909":"voice","910":"voice","911":"voice","1030":"voice","1031":"voice","1032":"voice","1152":"voice","1153":"voice","1154":"voice","1155":"voice","1156":"voice","1157":"voice","1158":"voice","1159":"voice","1160":"voice","1161":"voice","1162":"voice","1163":"voice","1164":"voice","1165":"voice","1166":"voice","1167":"voice","1280":"voice","1281":"voice","1282":"voice","1283":"voice","1284":"voice","1285":"voice","1286":"voice","1287":"voice","1408":"voice","1409":"voice","1410":"voice","1411":"voice","1412":"voice","1413":"voice","1536":"voice","1537":"voice","1538":"voice","1539":"voice","1540":"voice","1541":"voice","1664":"voice","1665":"voice","1666":"voice","1667":"voice","1668":"voice","1669":"voice","1670":"voice","1671":"voice","1792":"voice","1793":"voice","1794":"voice","1795":"voice","1797":"voice","1798":"voice","1799":"voice","1921":"lane","1922":"lane","1923":"lane","1924":"lane","1929":"lane","2048":"lane","2049":"lane","2050":"lane","2051":"lane","2052":"lane","2053":"lane","2054":"lane","3200":"voice","3201":"voice","3202":"voice","3203":"voice","3204":"voice","3205":"voice","3206":"voice","3207":"voice","3210":"voice","3328":"voice","3329":"voice","3330":"voice","3331":"voice","3332":"voice","3333":"voice","3334":"voice","3335":"voice","3338":"voice","3339":"voice","3340":"voice","3341":"voice","3342":"voice","3343":"voice","3344":"voice","3345":"voice","3867":"lane","3871":"lane","3872":"lane","3873":"lane","3874":"lane","4200":"voice","4201":"voice","4202":"voice","4203":"voice","4204":"voice","4205":"voice","4206":"voice","4207":"voice","4208":"voice","4209":"voice","4210":"voice","4211":"voice","4212":"voice","4213":"voice","4214":"voice","4215":"voice","4216":"voice","4217":"voice","4218":"voice","4219":"voice","4220":"voice","4222":"voice","4223":"voice","4224":"voice","4225":"voice","4226":"voice","4227":"voice","4228":"voice","4229":"voice","4230":"voice","4231":"voice","4232":"voice","4233":"voice","4234":"voice","4235":"voice","4236":"voice","4237":"voice","4238":"voice","4239":"voice","4300":"voice","4301":"voice","4302":"voice","4303":"voice","4304":"voice","4305":"voice","4306":"voice","4307":"voice","4308":"voice","4309":"voice","4310":"voice","4311":"voice","4312":"voice","4313":"voice","4314":"voice","4315":"voice","4316":"voice","4317":"voice","4318":"voice","4319":"voice","4320":"voice","4321":"voice","4322":"voice","4323":"voice","4324":"voice","4325":"voice","4326":"voice","4327":"voice","4328":"voice","4329":"voice","4330":"voice","4400":"voice","4401":"voice","4402":"voice","4403":"voice","4404":"voice","4405":"voice","4406":"voice","4407":"voice","4408":"voice","4409":"voice","4410":"voice","4411":"voice","4412":"voice","4413":"voice","4414":"voice","4415":"voice","4416":"voice","4417":"voice","4418":"voice","4419":"voice","4420":"voice","4421":"voice","4422":"voice","4423":"voice","4424":"voice","4425":"voice","4426":"voice","4427":"voice","4428":"voice","4429":"voice","4430":"voice","4431":"voice","4500":"voice","4501":"voice","4502":"voice","4503":"voice","4504":"voice","4505":"voice","4506":"voice","4507":"voice","4508":"voice","4509":"voice","4510":"voice","4511":"voice","4512":"voice","4513":"voice","4514":"voice","4515":"voice","4516":"voice","4517":"voice","4518":"voice","4519":"voice","4520":"voice","4521":"voice","4522":"voice","4523":"voice","4524":"voice","4525":"voice","4526":"voice","4527":"voice","4528":"voice","4529":"voice","4530":"voice","4531":"voice","4532":"voice","4533":"voice","4534":"voice","4535":"voice","4536":"voice","4537":"voice","4538":"voice","4539":"voice","4540":"voice","4541":"voice","4542":"voice","4543":"voice","4544":"voice","4545":"voice","4546":"voice","4547":"voice","4548":"voice","4549":"voice","4550":"voice","4551":"voice","4552":"voice","4553":"voice","4554":"voice","4555":"voice","4556":"voice","4557":"voice","4558":"voice","4559":"voice","4560":"voice","4561":"voice","4562":"voice","4563":"voice","4564":"voice","4565":"voice","4566":"voice","4567":"voice","4568":"voice","4569":"voice","4570":"voice","4571":"voice","4572":"voice","4573":"voice","4574":"voice","4575":"voice","4576":"voice","4577":"voice","4578":"voice","4579":"voice","4580":"voice","4581":"voice","4582":"voice","4583":"voice","4584":"voice","4585":"voice","4586":"voice","4587":"voice","4600":"voice","4601":"voice","4602":"voice","4603":"voice","4604":"voice","4605":"voice","4606":"voice","4607":"voice","4608":"voice","4609":"voice","4610":"voice","4611":"voice","4612":"voice","4613":"voice","4614":"voice","4615":"voice","4616":"voice","4617":"voice","4618":"voice","4619":"voice","4620":"voice","4621":"voice","4622":"voice","4623":"voice","4624":"voice","4625":"voice","4626":"voice","4627":"voice","4628":"voice","4629":"voice","4630":"voice","4631":"voice","4800":"voice","4801":"voice","4802":"voice","4803":"voice","4804":"voice","4805":"voice","4806":"voice","4807":"voice","4808":"voice","4809":"voice","4810":"voice","4811":"voice","4812":"voice","4813":"voice","4814":"voice","4815":"voice","4816":"voice","4817":"voice","4818":"voice","4819":"voice","4820":"voice","4821":"voice","4822":"voice","4823":"voice","4824":"voice","4825":"voice","4826":"voice","4827":"voice","4828":"voice","4829":"voice","4830":"voice","4831":"voice","4832":"voice","4833":"voice","4834":"voice","4835":"voice","4836":"voice","4837":"voice","4838":"voice","4839":"voice","4840":"voice","4841":"voice","4842":"voice","4843":"voice","4844":"voice","4845":"voice","4846":"voice","4847":"voice","4848":"voice","4849":"voice","4850":"voice","4851":"voice","4852":"voice","4853":"voice","4854":"voice","4855":"voice","4856":"voice","4857":"voice","4858":"voice","4859":"voice","4860":"voice","4861":"voice","4862":"voice","4863":"voice","4864":"voice","4865":"voice","4866":"voice","4867":"voice","4868":"voice","4869":"voice","4870":"voice","4871":"voice","4872":"voice","4873":"voice","4874":"voice","4875":"voice","4876":"voice","4877":"voice","4878":"voice","4879":"voice","4880":"voice","4881":"voice","4882":"voice","4883":"voice","4884":"voice","4885":"voice","4886":"voice","4887":"voice","4888":"voice","4889":"voice","4890":"voice","4891":"voice","4892":"voice","4893":"voice","4894":"voice","4895":"voice","4896":"voice","4897":"voice","4898":"voice","4899":"voice","4900":"voice","4901":"voice","4902":"voice","4903":"voice","5000":"voice","5001":"voice","5002":"voice","5003":"voice","5004":"voice","5005":"voice","5006":"voice","5007":"voice","5008":"voice","5009":"voice","5010":"voice","5011":"voice","5012":"voice","5013":"voice","5014":"voice","5015":"voice","5016":"voice","5017":"voice","5018":"voice","5019":"voice","5020":"voice","5021":"voice","5022":"voice","5023":"voice","5024":"voice","5025":"voice","5026":"voice","5027":"voice","5028":"voice","5029":"voice","5030":"voice","5031":"voice"};
const SEQUENCE_SOUND_IDS={"field":1,"speed":2,"join":5,"part_level":6,"part_delay":7,"part_reverb":8,"amp_attack":256,"amp_decay":257,"amp_sustain":258,"amp_release":259,"lanes_mode":385,"lanes_stretch":394,"stretch_ratio":395,"motion_mode":512,"motion_sound":513,"motion_rate":514,"motion_rate_basis":515,"motion_path_rate":516,"motion_step":517,"motion_seed":518,"motion_travel":519,"motion_jitter":520,"packet_duty":521,"motor_rate":522,"motor_symmetry":523,"motor_shape":524,"segment_trigger":525,"segment_overlap":526,"segment_layers":527,"segment_rate":528,"segment_repeats":529,"segment_step":530,"segment_pitch":531,"segment_level":532,"segment_curve":533,"grain_mode":640,"grain_size":641,"grain_density":642,"grain_pitch":644,"grain_window":645,"grain_spread":646,"grain_spray":647,"grain_bias":648,"grain_pitch_spray":649,"grain_reverse":650,"grain_size_variation":651,"grain_level_variation":652,"grain_timing":653,"grain_timing_scatter":654,"grain_envelope_skew":655,"grain_process":656,"grain_process_amount":657,"grain_regions":658,"grain_advance":659,"long_grain_mode":768,"long_grain_length":769,"long_grain_layers":770,"long_grain_pitch":771,"long_grain_shape":772,"long_grain_edge":773,"long_grain_spread":774,"long_grain_reverse":775,"long_grain_spray":776,"long_grain_bias":777,"long_grain_duration_variation":778,"long_grain_pitch_spray":779,"long_grain_level_variation":780,"long_grain_phase":781,"long_grain_advance":782,"cut_lane_advance":896,"cutups_mode":897,"cut_clock":898,"cut_division":899,"cut_rate":900,"cut_time_variation":901,"cut_gate":902,"cut_join":903,"cut_regions":904,"cut_repeats":905,"cut_reverse":906,"cut_pitch_variation":907,"cut_level_variation":908,"cut_lane_order":909,"cut_poly_path":910,"cut_region_mode":911,"sample_mode":1030,"slice_region_mode":1031,"slice_regions":1032,"glitch_mode":1152,"glitch_clock":1153,"glitch_division":1154,"glitch_rate":1155,"glitch_slice":1156,"glitch_repeats":1157,"glitch_chance":1158,"glitch_mix":1159,"glitch_steps":1160,"glitch_pulses":1161,"glitch_rotate":1162,"glitch_jump":1163,"glitch_pitch":1164,"glitch_reverse":1165,"glitch_decay":1166,"glitch_seed":1167,"waveset_group":1280,"waveset_repeats":1281,"waveset_stride":1282,"waveset_advance":1283,"waveset_direction":1284,"waveset_process":1285,"waveset_amount":1286,"waveset_detail":1287,"prism_mode":1408,"prism_blur":1409,"prism_decay":1410,"prism_pitch":1411,"prism_tilt":1412,"prism_smear":1413,"morph_mode":1536,"morph_amplitude":1537,"morph_frequency":1538,"morph_offset":1539,"morph_blur":1540,"morph_pitch":1541,"texture_mode":1664,"texture_rate":1665,"texture_length":1666,"texture_scatter":1667,"texture_range":1668,"texture_group":1669,"texture_spread":1670,"texture_seed":1671,"formant_pitch":1792,"formant_shift":1793,"formant_smoothing":1794,"formant_tune":1795,"formant_keytrack":1797,"formant_rough":1798,"formant_sub":1799,"effect_tone":1921,"effect_drive":1922,"effect_bits":1923,"effect_rate":1924,"effect_cross_pan":1929,"effect_rotary_rate":2048,"effect_rotary_depth":2049,"effect_rotary_mix":2050,"effect_rotary_ramp":2051,"effect_rotary_balance":2052,"effect_rotary_crossover":2053,"effect_rotary_spread":2054,"stochastic_size":3200,"stochastic_amplitude":3201,"stochastic_timing":3202,"stochastic_evolution":3203,"stochastic_memory":3204,"stochastic_pull":3205,"stochastic_law":3206,"stochastic_drift":3207,"stochastic_capture":3210,"pulsar_rate":3328,"pulsar_form":3329,"pulsar_size":3330,"pulsar_window":3331,"pulsar_skew":3332,"pulsar_omit":3333,"pulsar_burst":3334,"pulsar_jitter":3335,"pulsar_emit_mod_rate":3338,"pulsar_emit_mod_depth":3339,"pulsar_form_mod_rate":3340,"pulsar_form_mod_depth":3341,"pulsar_spray":3342,"pulsar_form_variation":3343,"pulsar_reverse":3344,"pulsar_keytrack":3345,"seq_pan_1":3867,"seq_eq_low":3871,"seq_eq_mid":3872,"seq_eq_high":3873,"seq_eq_freq":3874,"table_type":4200,"table_tune":4201,"table_width":4202,"table_level":4203,"table_art":4204,"table_decay":4205,"table_noise":4206,"table_drive":4207,"table_shape":4208,"table_pulse":4209,"table_detune":4210,"table_sub":4211,"table_cutoff":4212,"table_resonance":4213,"table_points":4214,"table_evolution":4215,"table_curve":4216,"table_drift":4217,"table_drift_rate":4218,"table_dynamics":4219,"table_seed":4220,"table_bank_a":4222,"table_pos_a":4223,"table_bank_b":4224,"table_pos_b":4225,"table_blend":4226,"table_ratio":4227,"table_fm":4228,"table_wt_detune":4229,"table_bend":4230,"table_pitch_decay":4231,"table_scan":4232,"table_scan_decay":4233,"table_wt_sub":4234,"table_impact":4235,"table_noise_decay":4236,"table_color":4237,"table_tone":4238,"table_compression":4239,"scanned_excitation":4300,"scanned_pick":4301,"scanned_hit":4302,"scanned_tune":4303,"scanned_motion":4304,"scanned_damping":4305,"scanned_brightness":4306,"scanned_width":4307,"scanned_mass":4308,"scanned_tension":4309,"scanned_center":4310,"scanned_profile":4311,"scanned_force":4312,"scanned_pressure":4313,"scanned_freeze":4314,"scanned_seed":4315,"scanned_art":4316,"scanned_impact":4317,"scanned_hardness":4318,"scanned_decay":4319,"scanned_bend":4320,"scanned_sweep_time":4321,"scanned_loss":4322,"scanned_amp_mode":4323,"scanned_amp_gain":4324,"scanned_amp_bass":4325,"scanned_amp_mid":4326,"scanned_amp_freq":4327,"scanned_amp_treble":4328,"scanned_amp_cab":4329,"scanned_amp_level":4330,"fm_mode":4400,"fm_tune":4401,"fm_width":4402,"fm_level":4403,"fm_algorithm":4404,"fm_ratio1":4405,"fm_index1":4406,"fm_feedback":4407,"fm_ratio2":4408,"fm_index2":4409,"fm_detune":4410,"fm_mix":4411,"fm_cutoff":4412,"fm_resonance":4413,"fm_drive":4414,"fm_decay":4415,"fm_fm_decay":4416,"fm_bend":4417,"fm_sweep_time":4418,"fm_impact":4419,"fm_noise":4420,"fm_noise_decay":4421,"fm_hardness":4422,"fm_harm":4423,"fm_attack1":4424,"fm_decay1":4425,"fm_end1":4426,"fm_curve1":4427,"fm_attack2":4428,"fm_decay2":4429,"fm_end2":4430,"fm_curve2":4431,"noise_type":4500,"noise_tune":4501,"noise_width":4502,"noise_level":4503,"noise_color":4504,"noise_tone":4505,"noise_bits":4506,"noise_seed":4507,"noise_count":4508,"noise_bandwidth":4509,"noise_tilt":4510,"noise_stretch":4511,"noise_motion":4512,"noise_motion_rate":4513,"noise_leak":4514,"noise_style":4515,"noise_rate":4516,"noise_size":4517,"noise_clump":4518,"noise_edge":4519,"noise_variation":4520,"noise_bed":4521,"noise_repeat":4522,"noise_fault":4523,"noise_ratio_1":4524,"noise_ratio_2":4525,"noise_ratio_3":4526,"noise_ratio_4":4527,"noise_ratio_5":4528,"noise_ratio_6":4529,"noise_ratio_7":4530,"noise_ratio_8":4531,"noise_ratio_9":4532,"noise_ratio_10":4533,"noise_ratio_11":4534,"noise_ratio_12":4535,"noise_ratio_13":4536,"noise_ratio_14":4537,"noise_ratio_15":4538,"noise_ratio_16":4539,"noise_ratio_17":4540,"noise_ratio_18":4541,"noise_ratio_19":4542,"noise_ratio_20":4543,"noise_ratio_21":4544,"noise_ratio_22":4545,"noise_ratio_23":4546,"noise_ratio_24":4547,"noise_ratio_25":4548,"noise_ratio_26":4549,"noise_ratio_27":4550,"noise_ratio_28":4551,"noise_ratio_29":4552,"noise_ratio_30":4553,"noise_ratio_31":4554,"noise_ratio_32":4555,"noise_band_1":4556,"noise_band_2":4557,"noise_band_3":4558,"noise_band_4":4559,"noise_band_5":4560,"noise_band_6":4561,"noise_band_7":4562,"noise_band_8":4563,"noise_band_9":4564,"noise_band_10":4565,"noise_band_11":4566,"noise_band_12":4567,"noise_band_13":4568,"noise_band_14":4569,"noise_band_15":4570,"noise_band_16":4571,"noise_band_17":4572,"noise_band_18":4573,"noise_band_19":4574,"noise_band_20":4575,"noise_band_21":4576,"noise_band_22":4577,"noise_band_23":4578,"noise_band_24":4579,"noise_band_25":4580,"noise_band_26":4581,"noise_band_27":4582,"noise_band_28":4583,"noise_band_29":4584,"noise_band_30":4585,"noise_band_31":4586,"noise_band_32":4587,"reson_mode":4600,"reson_tune":4601,"reson_width":4602,"reson_level":4603,"reson_model":4604,"reson_decay":4605,"reson_brightness":4606,"reson_damping":4607,"reson_stretch":4608,"reson_inharmonic":4609,"reson_mass":4610,"reson_bend":4611,"reson_sweep":4612,"reson_noise":4613,"reson_noise_decay":4614,"reson_color":4615,"reson_hit":4616,"reson_hardness":4617,"reson_hit_decay":4618,"reson_punch":4619,"reson_position":4620,"reson_coupling":4621,"reson_detune":4622,"reson_seed":4623,"reson_drive":4624,"reson_clip":4625,"reson_bias":4626,"reson_sag":4627,"reson_recovery":4628,"reson_tone":4629,"reson_mix":4630,"reson_output":4631,"partials_type":4800,"partials_tune":4801,"partials_width":4802,"partials_level":4803,"partials_count":4804,"partials_tilt":4805,"partials_stretch":4806,"partials_phase":4807,"partials_harmonics":4808,"partials_subs":4809,"partials_balance":4810,"partials_detune":4811,"partials_drift":4812,"partials_drift_rate":4813,"partials_damping":4814,"partials_time":4815,"partials_gap":4816,"partials_scatter":4817,"partials_rise":4818,"partials_sustain":4819,"partials_decay":4820,"partials_variation":4821,"partials_pitch_jitter":4822,"partials_shape":4823,"partials_ratio_1":4824,"partials_ratio_2":4825,"partials_ratio_3":4826,"partials_ratio_4":4827,"partials_ratio_5":4828,"partials_ratio_6":4829,"partials_ratio_7":4830,"partials_ratio_8":4831,"partials_ratio_9":4832,"partials_ratio_10":4833,"partials_ratio_11":4834,"partials_ratio_12":4835,"partials_ratio_13":4836,"partials_ratio_14":4837,"partials_ratio_15":4838,"partials_ratio_16":4839,"partials_ratio_17":4840,"partials_ratio_18":4841,"partials_ratio_19":4842,"partials_ratio_20":4843,"partials_ratio_21":4844,"partials_ratio_22":4845,"partials_ratio_23":4846,"partials_ratio_24":4847,"partials_ratio_25":4848,"partials_ratio_26":4849,"partials_ratio_27":4850,"partials_ratio_28":4851,"partials_ratio_29":4852,"partials_ratio_30":4853,"partials_ratio_31":4854,"partials_ratio_32":4855,"partials_band_1":4856,"partials_band_2":4857,"partials_band_3":4858,"partials_band_4":4859,"partials_band_5":4860,"partials_band_6":4861,"partials_band_7":4862,"partials_band_8":4863,"partials_band_9":4864,"partials_band_10":4865,"partials_band_11":4866,"partials_band_12":4867,"partials_band_13":4868,"partials_band_14":4869,"partials_band_15":4870,"partials_band_16":4871,"partials_band_17":4872,"partials_band_18":4873,"partials_band_19":4874,"partials_band_20":4875,"partials_band_21":4876,"partials_band_22":4877,"partials_band_23":4878,"partials_band_24":4879,"partials_band_25":4880,"partials_band_26":4881,"partials_band_27":4882,"partials_band_28":4883,"partials_band_29":4884,"partials_band_30":4885,"partials_band_31":4886,"partials_band_32":4887,"partials_divisor_1":4888,"partials_divisor_2":4889,"partials_divisor_3":4890,"partials_divisor_4":4891,"partials_divisor_5":4892,"partials_divisor_6":4893,"partials_divisor_7":4894,"partials_divisor_8":4895,"partials_divisor_9":4896,"partials_divisor_10":4897,"partials_divisor_11":4898,"partials_divisor_12":4899,"partials_divisor_13":4900,"partials_divisor_14":4901,"partials_divisor_15":4902,"partials_divisor_16":4903,"lowform_body":5000,"lowform_tune":5001,"lowform_width":5002,"lowform_level":5003,"lowform_foundation_wave":5004,"lowform_foundation_octave":5005,"lowform_foundation_level":5006,"lowform_body_level":5007,"lowform_a":5008,"lowform_b":5009,"lowform_c":5010,"lowform_d":5011,"lowform_e":5012,"lowform_body_decay":5013,"lowform_texture_type":5014,"lowform_texture_level":5015,"lowform_texture_color":5016,"lowform_texture_track":5017,"lowform_texture_width":5018,"lowform_filter_type":5019,"lowform_cutoff":5020,"lowform_resonance":5021,"lowform_filter_drive":5022,"lowform_keytrack":5023,"lowform_filter_env":5024,"lowform_filter_decay":5025,"lowform_punch":5026,"lowform_punch_time":5027,"lowform_drive":5028,"lowform_low_return":5029,"lowform_texture_decay":5030,"lowform_texture_attack":5031};
/* SEQUENCE_SOUND_CATALOG_END */

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
    splitEnabled:false,sharedDual:false,quadMode:false,modeSelection:0,modeRequest:null,exitConfirmation:null,lastNonSingleMode:1,splitFocus:0,splitBlend:.5,splitParts:[{},{}],splitPages:[0,0,0,0],
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

const uiPolling={supported:null,cache:new Map(),cached:false,lastFrame:-Infinity,job:0,editorPending:false};
const uiTiming={last:null,gap:0,query:0,key:'',slow:0,jumps:0,step:-1,lane:-1,length:0,rate:-1,clip:-1};
let renderProfileRequested=false;
function resetUiTiming(){Object.assign(uiTiming,{last:null,gap:0,query:0,key:'',slow:0,jumps:0,step:-1,lane:-1,length:0,rate:-1,clip:-1});}
function noteUiFrame(){
    const now=Date.now();
    if(uiTiming.last!==null){const gap=Math.max(0,now-uiTiming.last);uiTiming.gap=Math.max(uiTiming.gap,gap);if(gap>50)uiTiming.slow++;}
    uiTiming.last=now;
}

const UI_POLL_JOBS=['sequence','status','composition','sequence','sources','diagnostics','sequence','midi'];
function fetchUiPoll(group){
    try{
        const data=JSON.parse(String(getParam('ui_poll_'+group,'')));
        if(data?.v!==1||!data.values||typeof data.values!=='object'
            ||Object.values(data.values).some(value=>typeof value!=='string'))return false;
        for(const [key,value] of Object.entries(data.values))uiPolling.cache.set(key,value);
        return true;
    }catch(_){return false;}
}
function withUiPollCache(work){
    const previous=uiPolling.cached;uiPolling.cached=true;
    try{return work();}finally{uiPolling.cached=previous;}
}
function getParam(key, fallback) {
    if(uiPolling.cached)return uiPolling.cache.has(key)?uiPolling.cache.get(key):fallback;
    const before=Date.now();
    try {
        const value = host_module_get_param(key);
        if(value!=null)uiPolling.cache.set(key,value);
        return value == null ? fallback : value;
    } catch (error) {
        return fallback;
    } finally {
        const elapsed=Math.max(0,Date.now()-before);
        if(elapsed>uiTiming.query){uiTiming.query=elapsed;uiTiming.key=key;}
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
            return host_module_set_param_blocking(key, String(value), 180);
        } else {
            return host_module_set_param(key, String(value));
        }
    } catch (error) {return false;}
}

function readEditorSnapshot() {
    // A failed/empty read is retried by normal editor recovery. Never fall
    // back to the large synchronous query that this path is designed to avoid.
    const first=JSON.parse(String(getParam('ui_editor_chunk:0','')));
    const count=first?.chunks,owner=first?.owner,clip=first?.clip;
    const valid=(part,index)=>part?.v===1&&part.chunk===index
        &&part.chunks===count&&part.owner===owner&&part.clip===clip
        &&part.split?.focus===owner%4&&part.values
        &&typeof part.values==='object'&&!Array.isArray(part.values)
        &&Object.keys(part.values).length<=64;
    if(!Number.isInteger(count)||count<1||count>16
        ||!Number.isInteger(owner)||owner<0||owner>27
        ||!Number.isInteger(clip)||clip<0||clip>63||!valid(first,0))return null;
    const values={...first.values};let latest=first;
    for(let index=1;index<count;++index){
        latest=JSON.parse(String(getParam('ui_editor_chunk:'+index,'')));
        if(!valid(latest,index))return null;
        Object.assign(values,latest.values);
    }
    return {...latest,values};
}

function refreshEditorSnapshot(readDetails=true) {
    if(uiPolling.cached){uiPolling.editorPending=true;return false;}
    uiPolling.editorPending=false;
    const previousPage=state.view==='settings'?parameterPages()[state.settingsPage]:null;
    const keepPage=previousPage&&(previousPage.scopeLabel===''||previousPage.patternFade||previousPage.chainFade||previousPage.chainTools||previousPage===CHAIN_PAGE)?previousPage:null;
    state.editorReady=false;
    let snapshot;
    try { snapshot=readEditorSnapshot(); }
    catch (_) { return false; }
    const values=snapshot?.values,split=snapshot?.split;
    if(snapshot?.v!==1 || !values || typeof values!=='object'
        || !split || ![0,1].includes(split.enabled) || !Number.isInteger(split.focus)||split.focus<0||split.focus>(split.enabled&&split.quad?3:1)
        || !decodeSplitState({...split,mode:values.split_mode})
        || !Array.isArray(split.parts) || split.parts.length!==(split.enabled&&split.quad?4:2)
        || split.parts.some((part)=>!part || typeof part!=='object')) return false;
    // TABLE patch values arrive through table_state, just as Layers and
    // Sequence use their dedicated snapshots. Validate only scalar chunks here.
    for(const [key,control] of Object.entries(PARAMS))
        if(!control.lfoField && !control.momentary && !key.startsWith('layers_') && !key.startsWith('seq_') && !key.startsWith('table_') && !key.startsWith('scanned_') && !key.startsWith('fm_') && !key.startsWith('noise_') && !key.startsWith('reson_')&&!key.startsWith('partials_')&&!key.startsWith('lowform_') && key!=='mix_mute' && !Number.isFinite(values[key])) return false;
    for(const key of ['pad_octave','record_lane','stack_scan_paused','glitch_buffer_lock'])
        if(!Number.isFinite(values[key])) return false;
    for(const [key,control] of Object.entries(PARAMS))
        if(!control.lfoField && !key.startsWith('layers_') && !key.startsWith('seq_') && !key.startsWith('table_') && !key.startsWith('scanned_') && !key.startsWith('fm_') && !key.startsWith('noise_') && !key.startsWith('reson_')&&!key.startsWith('partials_')&&!key.startsWith('lowform_') && key!=='mix_mute') control.value=control.momentary?0:clamp(values[key],control.min,control.max);
    parseSplitState({...split,mode:values.split_mode},false);
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
    uiPolling.cache.set('split_state',JSON.stringify(split));
    for(const [key,value] of Object.entries(values))uiPolling.cache.set(key,String(value));
    if(state.algorithm===15)readTable(false);if(state.algorithm===16)readScanned(false);if(state.algorithm===21)readLowform(false);else if(state.algorithm===20)readPartials(false);else if(state.algorithm===19)readReson(false);else if(state.algorithm===18)readNoise(false);else if(state.algorithm===17)readFm(false);
    if(readDetails){if(Number(values.split_mode)===5||layersUi.active)readLayers();if(Number(values.split_mode)===6||sequenceUi.active)readSequence();}
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
// Shared control LFO pool. Base PARAMS stay editable; display overlays are read-only.
// View-only playback values for the eight subscribed page controls. The base
// PARAMS values remain the source for encoder edits and MIDI feedback.
const playbackReadout={data:null};
function readPlaybackReadout(cachedOnly=false){
 let d;try{d=JSON.parse(String(cachedOnly?uiPolling.cache.get('midi_state'):getParam('midi_state','{}')));}catch(_){playbackReadout.data=null;return;}
 if(!Number.isInteger(d.owner)||!Number.isInteger(d.clip)||![0,1].includes(d.playing)||!Array.isArray(d.playback)||d.playback.length>8||d.playback.some(r=>!Array.isArray(r)||r.length!==3||!Number.isInteger(r[0])||!Number.isInteger(r[1])||!Number.isFinite(r[2]))){playbackReadout.data=null;return;}
 playbackReadout.data=d;
}
function playbackDisplayControl(c){
 const d=playbackReadout.data;if(!c||c.automated||!d||!state.clipPlaying||state.clipRecording||!d.playing||d.clip!==state.activeClip||d.owner!==currentPerformanceMode()*4+(state.splitEnabled?state.splitFocus:0))return c;
 const p=thruMidi.MIDI_BY_KEY.get(c.key);if(!p)return c;
 if(p.scope==='part'&&Number.isInteger(d.entry)&&d.entry!==Math.round(PARAMS.voice_engine_edit.value)-1)return c;
 if(/^layers_fx(?:[0-9]|1[01])$/.test(c.key)&&Number.isInteger(d.corner)&&d.corner!==Math.round(PARAMS.layers_corner.value))return c;
 const owner=p.scope==='part'?state.splitFocus:-1,row=d.playback.find(r=>r[0]===owner&&r[1]===p.id);if(!row)return c;
 const shown={...c,value:clamp(row[2],c.min,c.max),baseValue:c.value,automated:true};
 if(c.lfoField==='rate'){const key=c.key.replace(/_rate$/,'_clock'),clock=thruMidi.MIDI_BY_KEY.get(key),r=d.playback.find(r=>r[0]===-1&&r[1]===clock?.id);if(r)shown.lfoClock=Math.round(r[2]);}
 return shown;
}
function lfoActiveParts(){const mode=currentPerformanceMode();return mode===0||mode===5?[0,4]:mode===3||mode===6?[0,1,2,3,4]:[0,1,4];}

const lfoUi={data:null,live:null,visible:false,picker:null};
/* LFO_DESTINATIONS_BEGIN */
const LFO_DESTINATIONS=[{"key":"off","page":"Off","name":"Off","scope":"off","engines":[],"modes":[0,1,2,3,4,5,6],"parameter":-99,"min":0,"max":1,"octaves":0,"id":0,"condition":"always"},{"key":"speed","page":"ENGINE","name":"Speed","scope":"engine","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,5,6],"parameter":2,"min":0.05,"max":4,"octaves":2,"id":1,"condition":"always"},{"key":"part_level","page":"MIXER","name":"Level","scope":"mixer","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18],"modes":[0,1,2,3,4,5,6],"parameter":-1,"min":0,"max":2,"octaves":0,"id":2,"condition":"always"},{"key":"part_pan","page":"MIXER","name":"Pan","scope":"mixer","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18],"modes":[0,1,2,3,4,5,6],"parameter":-2,"min":-1,"max":1,"octaves":0,"id":3,"condition":"always"},{"key":"part_delay","page":"FX SENDS","name":"Delay Send","scope":"mixer","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18],"modes":[1,2,3,4,6],"parameter":-3,"min":0,"max":1,"octaves":0,"id":4,"condition":"always"},{"key":"part_reverb","page":"FX SENDS","name":"Reverb Send","scope":"mixer","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18],"modes":[1,2,3,4,6],"parameter":-4,"min":0,"max":1,"octaves":0,"id":5,"condition":"always"},{"key":"effect_tone","page":"PART COLOR","name":"Tone","scope":"insert","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":97,"min":-1,"max":1,"octaves":0,"id":6,"condition":"always"},{"key":"effect_drive","page":"PART COLOR","name":"Drive","scope":"insert","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":98,"min":0,"max":1,"octaves":0,"id":7,"condition":"always"},{"key":"effect_bits","page":"PART COLOR","name":"Bit Reduction","scope":"insert","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":99,"min":0,"max":1,"octaves":0,"id":8,"condition":"always"},{"key":"effect_rate","page":"PART COLOR","name":"Rate Reduction","scope":"insert","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":100,"min":1,"max":16,"octaves":0,"id":9,"condition":"always"},{"key":"stretch_ratio","page":"LANES PLAY","name":"Stretch Factor","scope":"engine","engines":[1,4],"modes":[0,1,2,3,4,5,6],"parameter":14,"min":0.25,"max":4,"octaves":2,"id":10,"condition":"always"},{"key":"grain_size","page":"GRAINS MAIN","name":"Grain Size","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":20,"min":8,"max":1000,"octaves":2,"id":11,"condition":"always"},{"key":"grain_pitch","page":"GRAINS MAIN","name":"Pitch","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":22,"min":0.25,"max":4,"octaves":2,"id":12,"condition":"always"},{"key":"grain_spread","page":"GRAINS MAIN","name":"Stereo Spread","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":24,"min":0,"max":1,"octaves":0,"id":13,"condition":"always"},{"key":"grain_spray","page":"GRAINS CLOUD","name":"Position Spray","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":38,"min":0,"max":1,"octaves":0,"id":14,"condition":"always"},{"key":"grain_bias","page":"GRAINS CLOUD","name":"Position Bias","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":39,"min":0,"max":2,"octaves":0,"id":15,"condition":"always"},{"key":"long_grain_length","page":"WEAVE","name":"Grain Length","scope":"engine","engines":[7],"modes":[0,1,2,3,4,5,6],"parameter":106,"min":250,"max":12000,"octaves":2,"id":16,"condition":"always"},{"key":"long_grain_pitch","page":"WEAVE","name":"Pitch","scope":"engine","engines":[7],"modes":[0,1,2,3,4,5,6],"parameter":108,"min":0.25,"max":4,"octaves":2,"id":17,"condition":"always"},{"key":"long_grain_edge","page":"WEAVE","name":"Grain Edge","scope":"engine","engines":[7],"modes":[0,1,2,3,4,5,6],"parameter":110,"min":5,"max":2000,"octaves":0,"id":18,"condition":"always"},{"key":"long_grain_spread","page":"WEAVE","name":"Stereo Spread","scope":"engine","engines":[7],"modes":[0,1,2,3,4,5,6],"parameter":111,"min":0,"max":1,"octaves":0,"id":19,"condition":"always"},{"key":"long_grain_spray","page":"WEAVE MOTION","name":"Position Spray","scope":"engine","engines":[7],"modes":[0,1,2,3,4,5,6],"parameter":113,"min":0,"max":1,"octaves":0,"id":20,"condition":"always"},{"key":"cut_gate","page":"CUTUPS CLOCK","name":"Cut Duration","scope":"engine","engines":[3],"modes":[0,1,2,3,4,5,6],"parameter":55,"min":5,"max":100,"octaves":0,"id":21,"condition":"always"},{"key":"cut_join","page":"CUTUPS CLOCK","name":"Cut Crossfade","scope":"engine","engines":[3],"modes":[0,1,2,3,4,5,6],"parameter":56,"min":0,"max":100,"octaves":0,"id":22,"condition":"always"},{"key":"cut_pitch_variation","page":"CUTUPS PATH","name":"Pitch Variation","scope":"engine","engines":[3],"modes":[0,1,2,3,4,5,6],"parameter":60,"min":0,"max":24,"octaves":0,"id":23,"condition":"always"},{"key":"glitch_mix","page":"GLITCH CLOCK","name":"Glitch Mix","scope":"engine","engines":[6],"modes":[0,1,2,3,4,5,6],"parameter":88,"min":0,"max":100,"octaves":0,"id":24,"condition":"always"},{"key":"glitch_chance","page":"GLITCH CLOCK","name":"Chance","scope":"engine","engines":[6],"modes":[0,1,2,3,4,5,6],"parameter":87,"min":0,"max":100,"octaves":0,"id":25,"condition":"always"},{"key":"glitch_pitch","page":"GLITCH PATTERN","name":"Pitch","scope":"engine","engines":[6],"modes":[0,1,2,3,4,5,6],"parameter":93,"min":0,"max":24,"octaves":0,"id":26,"condition":"always"},{"key":"glitch_decay","page":"GLITCH PATTERN","name":"Decay","scope":"engine","engines":[6],"modes":[0,1,2,3,4,5,6],"parameter":95,"min":0,"max":100,"octaves":0,"id":27,"condition":"always"},{"key":"prism_pitch","page":"PRISM","name":"Pitch","scope":"engine","engines":[9],"modes":[0,1,2,3,4,5,6],"parameter":179,"min":-24,"max":24,"octaves":0,"id":28,"condition":"always"},{"key":"prism_tilt","page":"PRISM","name":"Spectral Tilt","scope":"engine","engines":[9],"modes":[0,1,2,3,4,5,6],"parameter":180,"min":-100,"max":100,"octaves":0,"id":29,"condition":"always"},{"key":"prism_smear","page":"PRISM","name":"Smear","scope":"engine","engines":[9],"modes":[0,1,2,3,4,5,6],"parameter":181,"min":0,"max":100,"octaves":0,"id":30,"condition":"always"},{"key":"morph_amplitude","page":"MORPH","name":"Amplitude Blend","scope":"engine","engines":[10],"modes":[0,1,2,3,4,5,6],"parameter":182,"min":0,"max":100,"octaves":0,"id":31,"condition":"always"},{"key":"morph_frequency","page":"MORPH","name":"Frequency Blend","scope":"engine","engines":[10],"modes":[0,1,2,3,4,5,6],"parameter":183,"min":0,"max":100,"octaves":0,"id":32,"condition":"always"},{"key":"morph_pitch","page":"MORPH","name":"Pitch","scope":"engine","engines":[10],"modes":[0,1,2,3,4,5,6],"parameter":186,"min":-24,"max":24,"octaves":0,"id":33,"condition":"always"},{"key":"texture_scatter","page":"TEXTURE","name":"Scatter","scope":"engine","engines":[11],"modes":[0,1,2,3,4,5,6],"parameter":190,"min":0,"max":100,"octaves":0,"id":34,"condition":"always"},{"key":"texture_range","page":"TEXTURE","name":"Pitch Range","scope":"engine","engines":[11],"modes":[0,1,2,3,4,5,6],"parameter":191,"min":0,"max":24,"octaves":0,"id":35,"condition":"always"},{"key":"texture_spread","page":"TEXTURE","name":"Stereo Spread","scope":"engine","engines":[11],"modes":[0,1,2,3,4,5,6],"parameter":193,"min":0,"max":100,"octaves":0,"id":36,"condition":"always"},{"key":"formant_pitch","page":"FORMANT","name":"Pitch","scope":"engine","engines":[12],"modes":[0,1,2,3,4,5,6],"parameter":196,"min":-24,"max":24,"octaves":0,"id":37,"condition":"always"},{"key":"formant_shift","page":"FORMANT","name":"Formant Shift","scope":"engine","engines":[12],"modes":[0,1,2,3,4,5,6],"parameter":197,"min":-24,"max":24,"octaves":0,"id":38,"condition":"always"},{"key":"formant_sub","page":"FORMANT","name":"Sub Level","scope":"engine","engines":[12],"modes":[0,1,2,3,4,5,6],"parameter":203,"min":0,"max":100,"octaves":0,"id":39,"condition":"always"},{"key":"stochastic_amplitude","page":"STOCHASTIC","name":"Amplitude Deform","scope":"engine","engines":[13],"modes":[0,1,2,3,4,5,6],"parameter":206,"min":0,"max":100,"octaves":0,"id":40,"condition":"always"},{"key":"stochastic_timing","page":"STOCHASTIC","name":"Timing Deform","scope":"engine","engines":[13],"modes":[0,1,2,3,4,5,6],"parameter":207,"min":0,"max":100,"octaves":0,"id":41,"condition":"always"},{"key":"pulsar_form","page":"PULSAR","name":"Form Frequency","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":217,"min":1,"max":2000,"octaves":2,"id":42,"condition":"always"},{"key":"pulsar_size","page":"PULSAR","name":"Pulsaret Size","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":218,"min":0.5,"max":1000,"octaves":2,"id":43,"condition":"always"},{"key":"pulsar_skew","page":"PULSAR","name":"Window Skew","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":220,"min":0,"max":100,"octaves":0,"id":44,"condition":"always"},{"key":"pulsar_omit","page":"PULSAR","name":"Omit","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":221,"min":0,"max":100,"octaves":0,"id":45,"condition":"always"},{"key":"pulsar_spray","page":"PULSAR MOD","name":"Position Spray","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":230,"min":0,"max":100,"octaves":0,"id":46,"condition":"always"},{"key":"effect_cross_pan","page":"STEREO / ROTARY","name":"Stereo Pan","scope":"insert","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":124,"min":-100,"max":100,"octaves":0,"id":47,"condition":"always"},{"key":"effect_rotary_rate","page":"STEREO / ROTARY","name":"Rotary Rate","scope":"insert","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":125,"min":0,"max":8,"octaves":2,"id":48,"condition":"always"},{"key":"effect_rotary_depth","page":"STEREO / ROTARY","name":"Rotary Depth","scope":"insert","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":126,"min":0,"max":100,"octaves":0,"id":49,"condition":"always"},{"key":"effect_rotary_mix","page":"STEREO / ROTARY","name":"Rotary Mix","scope":"insert","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":127,"min":0,"max":100,"octaves":0,"id":50,"condition":"always"},{"key":"effect_rotary_balance","page":"STEREO / ROTARY","name":"Horn Drum Balance","scope":"insert","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":129,"min":-100,"max":100,"octaves":0,"id":51,"condition":"always"},{"key":"effect_rotary_spread","page":"STEREO / ROTARY","name":"Rotary Spread","scope":"insert","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":131,"min":0,"max":100,"octaves":0,"id":52,"condition":"always"},{"key":"effect_delay","page":"DELAY RETURN","name":"Delay Time","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":101,"min":1,"max":750,"octaves":0,"id":53,"condition":"always"},{"key":"effect_feedback","page":"DELAY RETURN","name":"Feedback","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":102,"min":0,"max":90,"octaves":0,"id":54,"condition":"always"},{"key":"effect_width","page":"DELAY RETURN","name":"Stereo Width","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":103,"min":0,"max":100,"octaves":0,"id":55,"condition":"always"},{"key":"effect_mix","page":"DELAY RETURN","name":"Delay Mix Return","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":104,"min":0,"max":100,"octaves":0,"id":56,"condition":"always"},{"key":"effect_reverb_decay","page":"REVERB","name":"Decay","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":153,"min":0.2,"max":20,"octaves":2,"id":57,"condition":"always"},{"key":"effect_reverb_damp","page":"REVERB","name":"Damping","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":155,"min":500,"max":18000,"octaves":2,"id":58,"condition":"always"},{"key":"effect_reverb_diff","page":"REVERB","name":"Diffusion","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":156,"min":0,"max":100,"octaves":0,"id":59,"condition":"always"},{"key":"effect_reverb_width","page":"REVERB","name":"Stereo Width","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":158,"min":0,"max":100,"octaves":0,"id":60,"condition":"always"},{"key":"effect_reverb_mix","page":"REVERB","name":"Reverb Mix Return","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":159,"min":0,"max":100,"octaves":0,"id":61,"condition":"always"},{"key":"effect_tape_drive","page":"TAPE","name":"Drive","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":133,"min":0,"max":100,"octaves":0,"id":62,"condition":"always"},{"key":"effect_tape_age","page":"TAPE","name":"Age","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":140,"min":0,"max":100,"octaves":0,"id":63,"condition":"always"},{"key":"effect_tape_hiss","page":"TAPE","name":"Hiss Level","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":138,"min":-84,"max":-54,"octaves":0,"id":64,"condition":"always"},{"key":"effect_tape_mix","page":"TAPE","name":"Tape Mix","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,6],"parameter":139,"min":0,"max":100,"octaves":0,"id":65,"condition":"always"},{"key":"split_blend","page":"FX SENDS","name":"A B Crossfade","scope":"shared","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[1,2,4],"parameter":162,"min":0,"max":1,"octaves":0,"id":66,"condition":"always"},{"key":"layers_fx_x","page":"LAYERS","name":"FX X Position","scope":"layers","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[5],"parameter":-5,"min":0,"max":1,"octaves":0,"id":67,"condition":"always"},{"key":"layers_fx_y","page":"LAYERS","name":"FX Y Position","scope":"layers","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[5],"parameter":-6,"min":0,"max":1,"octaves":0,"id":68,"condition":"always"},{"key":"join","page":"ENGINE","name":"Crossfade","scope":"engine","engines":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,5,6],"parameter":6,"min":0,"max":1,"octaves":0,"id":69,"condition":"join"},{"key":"motion_travel","page":"MOTION PATH","name":"Travel","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":26,"min":0,"max":1,"octaves":0,"id":70,"condition":"always"},{"key":"motion_jitter","page":"MOTION PATH","name":"Jitter","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":27,"min":0,"max":1,"octaves":0,"id":71,"condition":"always"},{"key":"motion_rate","page":"PACKETS","name":"Event Rate","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":10,"min":0.25,"max":20,"octaves":2,"id":72,"condition":"packets"},{"key":"packet_duty","page":"PACKETS","name":"Duty","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":28,"min":2,"max":98,"octaves":0,"id":73,"condition":"packets-events"},{"key":"motor_rate","page":"MOTOR","name":"Motor Rate","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":29,"min":0.05,"max":20,"octaves":2,"id":74,"condition":"motor"},{"key":"motor_symmetry","page":"MOTOR","name":"Symmetry","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":30,"min":5,"max":95,"octaves":0,"id":75,"condition":"motor-events"},{"key":"segment_rate","page":"EVENT CLOCK","name":"Event Rate","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":75,"min":0.25,"max":80,"octaves":2,"id":76,"condition":"events-clock"},{"key":"segment_step","page":"EVENT CLOCK","name":"Event Step","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":77,"min":0,"max":1,"octaves":0,"id":77,"condition":"events"},{"key":"segment_pitch","page":"EVENT SHAPE","name":"Pitch Scatter","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":78,"min":0,"max":12,"octaves":0,"id":78,"condition":"events"},{"key":"segment_level","page":"EVENT SHAPE","name":"Level Variation","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":79,"min":0,"max":1,"octaves":0,"id":79,"condition":"events"},{"key":"segment_curve","page":"EVENT SHAPE","name":"Interval Curve","scope":"engine","engines":[0],"modes":[0,1,2,3,4,5,6],"parameter":80,"min":-1,"max":1,"octaves":0,"id":80,"condition":"events-clock"},{"key":"grain_pitch_spray","page":"GRAINS CLOUD","name":"Pitch Spray","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":40,"min":0,"max":24,"octaves":0,"id":81,"condition":"always"},{"key":"grain_reverse","page":"GRAINS CLOUD","name":"Reverse Chance","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":41,"min":0,"max":100,"octaves":0,"id":82,"condition":"always"},{"key":"grain_size_variation","page":"GRAINS CLOUD","name":"Size Variation","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":42,"min":0,"max":1,"octaves":0,"id":83,"condition":"always"},{"key":"grain_level_variation","page":"GRAINS CLOUD","name":"Level Variation","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":43,"min":0,"max":1,"octaves":0,"id":84,"condition":"always"},{"key":"grain_timing_scatter","page":"GRAINS CLOUD","name":"Timing Scatter","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":45,"min":0,"max":1,"octaves":0,"id":85,"condition":"always"},{"key":"grain_process_amount","page":"GRAINS PROCESS","name":"Process Amount","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":48,"min":0,"max":1,"octaves":0,"id":86,"condition":"grain-process"},{"key":"grain_envelope_skew","page":"GRAINS PROCESS","name":"Envelope Skew","scope":"engine","engines":[2],"modes":[0,1,2,3,4,5,6],"parameter":46,"min":-1,"max":1,"octaves":0,"id":87,"condition":"always"},{"key":"cut_rate","page":"CUTUPS CLOCK","name":"Cut Rate","scope":"engine","engines":[3],"modes":[0,1,2,3,4,5,6],"parameter":53,"min":0.25,"max":64,"octaves":2,"id":88,"condition":"cut-hz"},{"key":"cut_time_variation","page":"CUTUPS CLOCK","name":"Time Variation","scope":"engine","engines":[3],"modes":[0,1,2,3,4,5,6],"parameter":54,"min":0,"max":1,"octaves":0,"id":89,"condition":"always"},{"key":"cut_repeats","page":"CUTUPS PATH","name":"Repeats","scope":"engine","engines":[3],"modes":[0,1,2,3,4,5,6],"parameter":58,"min":1,"max":16,"octaves":0,"id":90,"condition":"always"},{"key":"cut_reverse","page":"CUTUPS PATH","name":"Reverse Chance","scope":"engine","engines":[3],"modes":[0,1,2,3,4,5,6],"parameter":59,"min":0,"max":100,"octaves":0,"id":91,"condition":"always"},{"key":"cut_level_variation","page":"CUTUPS PATH","name":"Level Variation","scope":"engine","engines":[3],"modes":[0,1,2,3,4,5,6],"parameter":61,"min":0,"max":1,"octaves":0,"id":92,"condition":"always"},{"key":"glitch_rate","page":"GLITCH CLOCK","name":"Glitch Rate","scope":"engine","engines":[6],"modes":[0,1,2,3,4,5,6],"parameter":84,"min":0.25,"max":64,"octaves":2,"id":93,"condition":"glitch-hz"},{"key":"glitch_slice","page":"GLITCH CLOCK","name":"Slice Length","scope":"engine","engines":[6],"modes":[0,1,2,3,4,5,6],"parameter":85,"min":2,"max":160,"octaves":0,"id":94,"condition":"always"},{"key":"glitch_reverse","page":"GLITCH PATTERN","name":"Reverse Chance","scope":"engine","engines":[6],"modes":[0,1,2,3,4,5,6],"parameter":94,"min":0,"max":100,"octaves":0,"id":95,"condition":"always"},{"key":"long_grain_reverse","page":"WEAVE","name":"Reverse Chance","scope":"engine","engines":[7],"modes":[0,1,2,3,4,5,6],"parameter":112,"min":0,"max":100,"octaves":0,"id":96,"condition":"always"},{"key":"long_grain_duration_variation","page":"WEAVE MOTION","name":"Duration Variation","scope":"engine","engines":[7],"modes":[0,1,2,3,4,5,6],"parameter":115,"min":0,"max":1,"octaves":0,"id":97,"condition":"always"},{"key":"long_grain_pitch_spray","page":"WEAVE MOTION","name":"Pitch Spray","scope":"engine","engines":[7],"modes":[0,1,2,3,4,5,6],"parameter":116,"min":0,"max":24,"octaves":0,"id":98,"condition":"always"},{"key":"long_grain_level_variation","page":"WEAVE MOTION","name":"Level Variation","scope":"engine","engines":[7],"modes":[0,1,2,3,4,5,6],"parameter":117,"min":0,"max":1,"octaves":0,"id":99,"condition":"always"},{"key":"long_grain_phase","page":"WEAVE MOTION","name":"Phase Scatter","scope":"engine","engines":[7],"modes":[0,1,2,3,4,5,6],"parameter":118,"min":0,"max":1,"octaves":0,"id":100,"condition":"always"},{"key":"waveset_repeats","page":"WAVESETS","name":"Repeats","scope":"engine","engines":[8],"modes":[0,1,2,3,4,5,6],"parameter":145,"min":1,"max":16,"octaves":0,"id":101,"condition":"always"},{"key":"waveset_stride","page":"WAVESETS","name":"Stride","scope":"engine","engines":[8],"modes":[0,1,2,3,4,5,6],"parameter":146,"min":1,"max":16,"octaves":0,"id":102,"condition":"always"},{"key":"waveset_amount","page":"WAVESETS","name":"Process Amount","scope":"engine","engines":[8],"modes":[0,1,2,3,4,5,6],"parameter":150,"min":0,"max":100,"octaves":0,"id":103,"condition":"waveset-process"},{"key":"prism_blur","page":"PRISM","name":"Blur Time","scope":"engine","engines":[9],"modes":[0,1,2,3,4,5,6],"parameter":177,"min":0,"max":2000,"octaves":2,"id":104,"condition":"always"},{"key":"prism_decay","page":"PRISM","name":"Decay Time","scope":"engine","engines":[9],"modes":[0,1,2,3,4,5,6],"parameter":178,"min":50,"max":20000,"octaves":2,"id":105,"condition":"prism-freeze"},{"key":"morph_offset","page":"MORPH","name":"Source Offset","scope":"engine","engines":[10],"modes":[0,1,2,3,4,5,6],"parameter":184,"min":-100,"max":100,"octaves":0,"id":106,"condition":"always"},{"key":"morph_blur","page":"MORPH","name":"Blur Time","scope":"engine","engines":[10],"modes":[0,1,2,3,4,5,6],"parameter":185,"min":0,"max":2000,"octaves":2,"id":107,"condition":"always"},{"key":"texture_rate","page":"TEXTURE","name":"Event Rate","scope":"engine","engines":[11],"modes":[0,1,2,3,4,5,6],"parameter":188,"min":0.25,"max":24,"octaves":2,"id":108,"condition":"always"},{"key":"texture_length","page":"TEXTURE","name":"Event Length","scope":"engine","engines":[11],"modes":[0,1,2,3,4,5,6],"parameter":189,"min":20,"max":2000,"octaves":2,"id":109,"condition":"always"},{"key":"formant_smoothing","page":"FORMANT","name":"Smoothing","scope":"engine","engines":[12],"modes":[0,1,2,3,4,5,6],"parameter":198,"min":5,"max":200,"octaves":2,"id":110,"condition":"always"},{"key":"formant_tune","page":"FORMANT","name":"Tune","scope":"engine","engines":[12],"modes":[0,1,2,3,4,5,6],"parameter":199,"min":0,"max":100,"octaves":0,"id":111,"condition":"always"},{"key":"formant_keytrack","page":"FORMANT","name":"Key Tracking","scope":"engine","engines":[12],"modes":[0,1,2,3,4,5,6],"parameter":201,"min":-100,"max":100,"octaves":0,"id":112,"condition":"always"},{"key":"formant_rough","page":"FORMANT","name":"Roughness","scope":"engine","engines":[12],"modes":[0,1,2,3,4,5,6],"parameter":202,"min":0,"max":100,"octaves":0,"id":113,"condition":"always"},{"key":"stochastic_memory","page":"STOCHASTIC","name":"Memory","scope":"engine","engines":[13],"modes":[0,1,2,3,4,5,6],"parameter":209,"min":0,"max":100,"octaves":0,"id":114,"condition":"always"},{"key":"stochastic_pull","page":"STOCHASTIC","name":"Source Pull","scope":"engine","engines":[13],"modes":[0,1,2,3,4,5,6],"parameter":210,"min":0,"max":100,"octaves":0,"id":115,"condition":"stochastic-manual"},{"key":"stochastic_capture","page":"STOCHASTIC","name":"Listener Capture","scope":"engine","engines":[13],"modes":[0,1,2,3,4,5,6],"parameter":215,"min":0,"max":100,"octaves":0,"id":116,"condition":"stochastic-listener"},{"key":"stochastic_drift","page":"STOCHASTIC","name":"Drift","scope":"engine","engines":[13],"modes":[0,1,2,3,4,5,6],"parameter":212,"min":0,"max":100,"octaves":0,"id":117,"condition":"always"},{"key":"pulsar_rate","page":"PULSAR","name":"Pulse Rate","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":216,"min":0.1,"max":2000,"octaves":2,"id":118,"condition":"always"},{"key":"pulsar_jitter","page":"PULSAR","name":"Timing Jitter","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":223,"min":0,"max":100,"octaves":0,"id":119,"condition":"always"},{"key":"pulsar_emit_mod_rate","page":"PULSAR MOD","name":"Emission Mod Rate","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":226,"min":0.01,"max":20,"octaves":2,"id":120,"condition":"always"},{"key":"pulsar_emit_mod_depth","page":"PULSAR MOD","name":"Emission Mod Depth","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":227,"min":0,"max":24,"octaves":0,"id":121,"condition":"always"},{"key":"pulsar_form_mod_rate","page":"PULSAR MOD","name":"Form Mod Rate","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":228,"min":0.01,"max":20,"octaves":2,"id":122,"condition":"always"},{"key":"pulsar_form_mod_depth","page":"PULSAR MOD","name":"Form Mod Depth","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":229,"min":0,"max":24,"octaves":0,"id":123,"condition":"always"},{"key":"pulsar_form_variation","page":"PULSAR MOD","name":"Form Variation","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":231,"min":0,"max":24,"octaves":0,"id":124,"condition":"always"},{"key":"pulsar_reverse","page":"PULSAR MOD","name":"Reverse Chance","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":232,"min":0,"max":100,"octaves":0,"id":125,"condition":"always"},{"key":"pulsar_keytrack","page":"PULSAR MOD","name":"Key Tracking","scope":"engine","engines":[14],"modes":[0,1,2,3,4,5,6],"parameter":233,"min":0,"max":100,"octaves":0,"id":126,"condition":"always"},{"key":"locus","page":"ENGINE","name":"Sample Position","scope":"engine","engines":[0,1,2,3,4,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,5,6],"parameter":0,"min":0,"max":1,"octaves":0,"id":127,"condition":"window"},{"key":"field","page":"ENGINE","name":"Field Width","scope":"engine","engines":[0,1,2,3,4,6,7,8,9,10,11,12,13,14],"modes":[0,1,2,3,4,5,6],"parameter":1,"min":0.02,"max":1,"octaves":0,"id":128,"condition":"window"},{"key":"stochastic_size","page":"STOCHASTIC","name":"Source Group Size","scope":"engine","engines":[13],"modes":[0,1,2,3,4,5,6],"parameter":205,"min":1,"max":64,"octaves":0,"id":129,"condition":"always"},{"key":"stochastic_evolution","page":"STOCHASTIC","name":"Evolution Rate","scope":"engine","engines":[13],"modes":[0,1,2,3,4,5,6],"parameter":208,"min":0,"max":1023,"octaves":2,"id":130,"condition":"always"},{"id":131,"key":"table_tune","page":"TABLE","name":"Transpose","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":257,"min":-48,"max":48,"octaves":0,"condition":"always"},{"id":132,"key":"table_width","page":"TABLE","name":"Stereo Width","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":258,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":133,"key":"table_level","page":"TABLE","name":"Generator Level","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":259,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":134,"key":"table_decay","page":"TABLE","name":"Hit Decay","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":261,"min":10,"max":12000,"octaves":2,"condition":"table-hit"},{"id":135,"key":"table_noise","page":"TABLE","name":"Noise Level","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":262,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":136,"key":"table_drive","page":"TABLE","name":"Drive","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":263,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":137,"key":"table_pulse","page":"TABLE WAVE","name":"Pulse Width","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":265,"min":5,"max":95,"octaves":0,"condition":"table-wave"},{"id":138,"key":"table_detune","page":"TABLE WAVE","name":"Wave Detune","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":266,"min":0,"max":50,"octaves":0,"condition":"table-wave"},{"id":139,"key":"table_sub","page":"TABLE WAVE","name":"Wave Sub Level","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":267,"min":0,"max":100,"octaves":0,"condition":"table-wave"},{"id":140,"key":"table_cutoff","page":"TABLE WAVE","name":"Filter Cutoff","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":268,"min":20,"max":20000,"octaves":2,"condition":"table-wave"},{"id":141,"key":"table_resonance","page":"TABLE WAVE","name":"Filter Resonance","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":269,"min":0,"max":95,"octaves":0,"condition":"table-wave"},{"id":142,"key":"table_points","page":"TABLE SPLINE","name":"Spline Points","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":270,"min":1,"max":32,"octaves":0,"condition":"table-spline"},{"id":143,"key":"table_evolution","page":"TABLE SPLINE","name":"Spline Evolution","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":271,"min":1,"max":4096,"octaves":2,"condition":"table-spline"},{"id":144,"key":"table_curve","page":"TABLE SPLINE","name":"Spline Curvature","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":272,"min":0,"max":100,"octaves":0,"condition":"table-spline"},{"id":145,"key":"table_drift","page":"TABLE SPLINE","name":"Spline Pitch Drift","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":273,"min":0,"max":12,"octaves":0,"condition":"table-spline"},{"id":146,"key":"table_drift_rate","page":"TABLE SPLINE","name":"Spline Drift Time","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":274,"min":1,"max":1000,"octaves":2,"condition":"table-spline"},{"id":147,"key":"table_dynamics","page":"TABLE SPLINE","name":"Spline Dynamics","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":275,"min":0,"max":100,"octaves":0,"condition":"table-spline"},{"id":148,"key":"table_pos_a","page":"TABLE WT OSC","name":"Oscillator A Position","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":279,"min":0,"max":100,"octaves":0,"condition":"table-drum"},{"id":149,"key":"table_pos_b","page":"TABLE WT OSC","name":"Oscillator B Position","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":281,"min":0,"max":100,"octaves":0,"condition":"table-drum"},{"id":150,"key":"table_blend","page":"TABLE WT OSC","name":"Oscillator Blend","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":282,"min":0,"max":100,"octaves":0,"condition":"table-drum"},{"id":151,"key":"table_ratio","page":"TABLE WT OSC","name":"Oscillator Ratio","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":283,"min":0.125,"max":16,"octaves":2,"condition":"table-drum"},{"id":152,"key":"table_fm","page":"TABLE WT OSC","name":"Phase Modulation","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":284,"min":0,"max":100,"octaves":0,"condition":"table-drum"},{"id":153,"key":"table_wt_detune","page":"TABLE WT COLOR","name":"Drum Detune","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":285,"min":0,"max":50,"octaves":0,"condition":"table-drum"},{"id":154,"key":"table_bend","page":"TABLE WT HIT","name":"Pitch Sweep","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":286,"min":-48,"max":48,"octaves":0,"condition":"table-drum"},{"id":155,"key":"table_pitch_decay","page":"TABLE WT HIT","name":"Pitch Sweep Time","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":287,"min":1,"max":2000,"octaves":2,"condition":"table-drum"},{"id":156,"key":"table_scan","page":"TABLE WT HIT","name":"Table Sweep","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":288,"min":-100,"max":100,"octaves":0,"condition":"table-drum"},{"id":157,"key":"table_scan_decay","page":"TABLE WT HIT","name":"Table Sweep Time","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":289,"min":1,"max":4000,"octaves":2,"condition":"table-drum"},{"id":158,"key":"table_wt_sub","page":"TABLE WT HIT","name":"Drum Sub Level","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":290,"min":0,"max":100,"octaves":0,"condition":"table-drum"},{"id":159,"key":"table_impact","page":"TABLE WT HIT","name":"Impact Level","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":291,"min":0,"max":100,"octaves":0,"condition":"table-drum"},{"id":160,"key":"table_noise_decay","page":"TABLE WT HIT","name":"Noise Decay","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":292,"min":1,"max":4000,"octaves":2,"condition":"table-drum"},{"id":161,"key":"table_tone","page":"TABLE WT COLOR","name":"Drum Brightness","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":294,"min":0,"max":100,"octaves":0,"condition":"table-drum"},{"id":162,"key":"table_compression","page":"TABLE WT COLOR","name":"Drum Compression","scope":"engine","engines":[15],"modes":[0,1,2,3,4,5,6],"parameter":295,"min":0,"max":100,"octaves":0,"condition":"table-drum"},{"id":163,"key":"scanned_tune","page":"SCANNED","name":"Transpose","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":323,"min":-48,"max":48,"octaves":0,"condition":"always"},{"id":164,"key":"scanned_motion","page":"SCANNED","name":"Mechanical Motion","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":324,"min":0.05,"max":6,"octaves":2,"condition":"always"},{"id":165,"key":"scanned_damping","page":"SCANNED","name":"Mechanical Damping","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":325,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":166,"key":"scanned_brightness","page":"SCANNED","name":"Readout Brightness","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":326,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":167,"key":"scanned_width","page":"SCANNED","name":"Stereo Width","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":327,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":168,"key":"scanned_mass","page":"SCAN BODY","name":"Mass Variation","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":328,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":169,"key":"scanned_tension","page":"SCAN BODY","name":"Spring Variation","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":329,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":170,"key":"scanned_center","page":"SCAN BODY","name":"Centering Springs","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":330,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":171,"key":"scanned_force","page":"SCAN BODY","name":"Continuous Force","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":332,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":172,"key":"scanned_pressure","page":"SCAN BODY","name":"Pressure Force Depth","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":333,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":173,"key":"scanned_decay","page":"SCAN HIT","name":"Percussion Decay","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":339,"min":10,"max":4000,"octaves":2,"condition":"always"},{"id":174,"key":"scanned_bend","page":"SCAN HIT","name":"Pitch Sweep","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":340,"min":-48,"max":48,"octaves":0,"condition":"always"},{"id":175,"key":"scanned_sweep_time","page":"SCAN HIT","name":"Pitch Sweep Time","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":341,"min":5,"max":2000,"octaves":2,"condition":"always"},{"id":176,"key":"scanned_loss","page":"SCAN HIT","name":"Harmonic Decay","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":342,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":177,"key":"scanned_amp_gain","page":"SCAN AMP","name":"Amp Gain","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":344,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":178,"key":"scanned_amp_bass","page":"SCAN AMP","name":"Amp Bass","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":345,"min":-12,"max":12,"octaves":0,"condition":"always"},{"id":179,"key":"scanned_amp_mid","page":"SCAN AMP","name":"Amp Midrange","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":346,"min":-20,"max":10,"octaves":0,"condition":"always"},{"id":180,"key":"scanned_amp_treble","page":"SCAN AMP","name":"Amp Treble","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":348,"min":-20,"max":15,"octaves":0,"condition":"always"},{"id":181,"key":"scanned_amp_cab","page":"SCAN AMP","name":"Cabinet Amount","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":349,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":182,"key":"scanned_amp_level","page":"SCAN AMP","name":"Amp Output","scope":"engine","engines":[16],"modes":[0,1,2,3,4,5,6],"parameter":350,"min":-18,"max":6,"octaves":0,"condition":"always"},{"id":183,"key":"fm_tune","page":"FM","name":"FM Transpose","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":385,"min":-48,"max":48,"octaves":0,"condition":"always"},{"id":184,"key":"fm_width","page":"FM","name":"FM Stereo Width","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":386,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":185,"key":"fm_level","page":"FM","name":"FM Level","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":387,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":186,"key":"fm_ratio1","page":"FM","name":"Modulator 1 Ratio","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":389,"min":0.125,"max":32,"octaves":0,"condition":"always"},{"id":187,"key":"fm_index1","page":"FM","name":"Modulator 1 Index","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":390,"min":0,"max":12,"octaves":0,"condition":"always"},{"id":188,"key":"fm_feedback","page":"FM","name":"FM Feedback","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":391,"min":0,"max":90,"octaves":0,"condition":"always"},{"id":189,"key":"fm_ratio2","page":"FM COLOR","name":"Modulator 2 Ratio","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":392,"min":0.125,"max":32,"octaves":0,"condition":"always"},{"id":190,"key":"fm_index2","page":"FM COLOR","name":"Modulator 2 Index","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":393,"min":0,"max":12,"octaves":0,"condition":"always"},{"id":191,"key":"fm_detune","page":"FM COLOR","name":"FM Stereo Detune","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":394,"min":0,"max":30,"octaves":0,"condition":"always"},{"id":192,"key":"fm_mix","page":"FM COLOR","name":"FM Wet Mix","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":395,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":193,"key":"fm_cutoff","page":"FM COLOR","name":"FM Filter Cutoff","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":396,"min":20,"max":20000,"octaves":2,"condition":"always"},{"id":194,"key":"fm_resonance","page":"FM COLOR","name":"FM Filter Resonance","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":397,"min":0,"max":85,"octaves":0,"condition":"always"},{"id":195,"key":"fm_drive","page":"FM COLOR","name":"FM Drive","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":398,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":196,"key":"fm_decay","page":"FM HIT","name":"FM Drum Decay","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":399,"min":10,"max":8000,"octaves":2,"condition":"always"},{"id":197,"key":"fm_fm_decay","page":"FM HIT","name":"FM Index Decay","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":400,"min":2,"max":4000,"octaves":2,"condition":"always"},{"id":198,"key":"fm_bend","page":"FM HIT","name":"FM Pitch Sweep","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":401,"min":-48,"max":48,"octaves":0,"condition":"always"},{"id":199,"key":"fm_sweep_time","page":"FM HIT","name":"FM Pitch Sweep Time","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":402,"min":1,"max":2000,"octaves":2,"condition":"always"},{"id":200,"key":"fm_noise","page":"FM HIT","name":"FM Noise Body","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":404,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":201,"key":"fm_noise_decay","page":"FM HIT","name":"FM Noise Decay","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":405,"min":1,"max":4000,"octaves":2,"condition":"always"},{"id":202,"key":"fm_harm","page":"FM COLOR","name":"Operator Harmonics","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":407,"min":-100,"max":100,"octaves":0,"condition":"always"},{"id":203,"key":"fm_attack1","page":"FM ENVS","name":"Modulator 1 Attack","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":408,"min":0,"max":4000,"octaves":0,"condition":"always"},{"id":204,"key":"fm_decay1","page":"FM ENVS","name":"Modulator 1 Decay","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":409,"min":1,"max":8000,"octaves":0,"condition":"always"},{"id":205,"key":"fm_end1","page":"FM ENVS","name":"Modulator 1 End Level","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":410,"min":0,"max":100,"octaves":0,"condition":"always"},{"id":206,"key":"fm_attack2","page":"FM ENVS","name":"Modulator 2 Attack","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":412,"min":0,"max":4000,"octaves":0,"condition":"always"},{"id":207,"key":"fm_decay2","page":"FM ENVS","name":"Modulator 2 Decay","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":413,"min":1,"max":8000,"octaves":0,"condition":"always"},{"id":208,"key":"fm_end2","page":"FM ENVS","name":"Modulator 2 End Level","scope":"engine","engines":[17],"modes":[0,1,2,3,4,5,6],"parameter":414,"min":0,"max":100,"octaves":0,"condition":"always"},{"key":"noise_width","page":"NOISE","name":"Noise Stereo Width","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":450,"min":0,"max":100,"octaves":0,"id":209,"condition":"always"},{"key":"noise_level","page":"NOISE","name":"Noise Level","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":451,"min":0,"max":100,"octaves":0,"id":210,"condition":"always"},{"key":"noise_tone","page":"NOISE","name":"Noise Low Pass","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":453,"min":20,"max":20000,"octaves":1,"id":211,"condition":"always"},{"key":"noise_bits","page":"NOISE","name":"Noise Resolution","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":454,"min":2,"max":24,"octaves":0,"id":212,"condition":"always"},{"key":"noise_count","page":"NOISE BANK","name":"Band Count","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":456,"min":1,"max":32,"octaves":0,"id":213,"condition":"always"},{"key":"noise_bandwidth","page":"NOISE BANK","name":"Band Width","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":457,"min":0.1,"max":48,"octaves":1,"id":214,"condition":"always"},{"key":"noise_tilt","page":"NOISE BANK","name":"Band Tilt","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":458,"min":-24,"max":12,"octaves":0,"id":215,"condition":"always"},{"key":"noise_stretch","page":"NOISE BANK","name":"Band Stretch","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":459,"min":0.25,"max":4,"octaves":0,"id":216,"condition":"always"},{"key":"noise_motion","page":"NOISE BANK","name":"Band Motion","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":460,"min":0,"max":24,"octaves":0,"id":217,"condition":"always"},{"key":"noise_motion_rate","page":"NOISE BANK","name":"Band Motion Rate","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":461,"min":0.01,"max":20,"octaves":1,"id":218,"condition":"always"},{"key":"noise_leak","page":"NOISE BANK","name":"Unfiltered Noise","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":462,"min":0,"max":100,"octaves":0,"id":219,"condition":"always"},{"key":"noise_rate","page":"NOISE EVENTS","name":"Event Rate","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":464,"min":0.1,"max":2000,"octaves":1,"id":220,"condition":"always"},{"key":"noise_size","page":"NOISE EVENTS","name":"Event Size","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":465,"min":0.02,"max":500,"octaves":1,"id":221,"condition":"always"},{"key":"noise_clump","page":"NOISE EVENTS","name":"Event Clustering","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":466,"min":0,"max":100,"octaves":0,"id":222,"condition":"always"},{"key":"noise_edge","page":"NOISE EVENTS","name":"Event Edge","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":467,"min":0,"max":100,"octaves":0,"id":223,"condition":"always"},{"key":"noise_variation","page":"NOISE EVENTS","name":"Event Variation","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":468,"min":0,"max":100,"octaves":0,"id":224,"condition":"always"},{"key":"noise_bed","page":"NOISE EVENTS","name":"Noise Bed","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":469,"min":0,"max":100,"octaves":0,"id":225,"condition":"always"},{"key":"noise_fault","page":"NOISE EVENTS","name":"Data Variation","scope":"engine","engines":[18],"modes":[0,1,2,3,4,5,6],"parameter":471,"min":0,"max":100,"octaves":0,"id":226,"condition":"always"},{"key":"reson_width","page":"RESON","name":"Reson Stereo Width","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":546,"min":0,"max":100,"octaves":0,"id":227,"condition":"always"},{"key":"reson_level","page":"RESON","name":"Reson Level","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":547,"min":0,"max":100,"octaves":0,"id":228,"condition":"always"},{"key":"reson_decay","page":"RESON","name":"Reson Ring Decay","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":549,"min":10,"max":8000,"octaves":1,"id":229,"condition":"always"},{"key":"reson_brightness","page":"RESON","name":"Modal Brightness","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":550,"min":0,"max":100,"octaves":0,"id":230,"condition":"always"},{"key":"reson_damping","page":"RESON","name":"Upper Mode Damping","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":551,"min":0,"max":100,"octaves":0,"id":231,"condition":"always"},{"key":"reson_stretch","page":"RESON BODY","name":"Modal Stretch","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":552,"min":0.5,"max":2,"octaves":0,"id":232,"condition":"always"},{"key":"reson_inharmonic","page":"RESON BODY","name":"Body Inharmonicity","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":553,"min":0,"max":100,"octaves":0,"id":233,"condition":"always"},{"key":"reson_mass","page":"RESON BODY","name":"Body Mass","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":554,"min":0,"max":100,"octaves":0,"id":234,"condition":"always"},{"key":"reson_bend","page":"RESON BODY","name":"Strike Pitch Sweep","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":555,"min":-48,"max":48,"octaves":0,"id":235,"condition":"always"},{"key":"reson_sweep","page":"RESON BODY","name":"Pitch Sweep Time","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":556,"min":1,"max":2000,"octaves":1,"id":236,"condition":"always"},{"key":"reson_noise","page":"RESON BODY","name":"Body Noise","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":557,"min":0,"max":100,"octaves":0,"id":237,"condition":"always"},{"key":"reson_noise_decay","page":"RESON BODY","name":"Body Noise Decay","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":558,"min":1,"max":4000,"octaves":1,"id":238,"condition":"always"},{"key":"reson_color","page":"RESON BODY","name":"Body Noise Color","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":559,"min":0,"max":100,"octaves":0,"id":239,"condition":"always"},{"key":"reson_hit","page":"RESON HIT","name":"Strike Level","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":560,"min":0,"max":100,"octaves":0,"id":240,"condition":"always"},{"key":"reson_hardness","page":"RESON HIT","name":"Beater Hardness","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":561,"min":0,"max":100,"octaves":0,"id":241,"condition":"always"},{"key":"reson_hit_decay","page":"RESON HIT","name":"Strike Decay","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":562,"min":0.2,"max":100,"octaves":1,"id":242,"condition":"always"},{"key":"reson_punch","page":"RESON HIT","name":"Attack Punch","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":563,"min":0,"max":100,"octaves":0,"id":243,"condition":"always"},{"key":"reson_position","page":"RESON HIT","name":"Strike Position","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":564,"min":0,"max":100,"octaves":0,"id":244,"condition":"always"},{"key":"reson_coupling","page":"RESON HIT","name":"Mode Coupling","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":565,"min":0,"max":100,"octaves":0,"id":245,"condition":"always"},{"key":"reson_detune","page":"RESON HIT","name":"Modal Stereo Detune","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":566,"min":0,"max":30,"octaves":0,"id":246,"condition":"always"},{"key":"reson_drive","page":"RESON DRIVE","name":"Overload Drive","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":568,"min":0,"max":100,"octaves":0,"id":247,"condition":"always"},{"key":"reson_clip","page":"RESON DRIVE","name":"Overload Shape","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":569,"min":0,"max":100,"octaves":0,"id":248,"condition":"always"},{"key":"reson_bias","page":"RESON DRIVE","name":"Overload Asymmetry","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":570,"min":-100,"max":100,"octaves":0,"id":249,"condition":"always"},{"key":"reson_sag","page":"RESON DRIVE","name":"Overload Sag","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":571,"min":0,"max":100,"octaves":0,"id":250,"condition":"always"},{"key":"reson_recovery","page":"RESON DRIVE","name":"Sag Recovery","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":572,"min":5,"max":500,"octaves":1,"id":251,"condition":"always"},{"key":"reson_tone","page":"RESON DRIVE","name":"Overload Low Pass","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":573,"min":200,"max":20000,"octaves":1,"id":252,"condition":"always"},{"key":"reson_mix","page":"RESON DRIVE","name":"Overload Mix","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":574,"min":0,"max":100,"octaves":0,"id":253,"condition":"always"},{"key":"reson_output","page":"RESON DRIVE","name":"Overload Output","scope":"engine","engines":[19],"modes":[0,1,2,3,4,5,6],"parameter":575,"min":-18,"max":12,"octaves":0,"id":254,"condition":"always"},{"key":"partials_width","page":"PARTIALS","name":"Partials Stereo Width","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":578,"min":0,"max":100,"octaves":0,"id":255,"condition":"always"},{"key":"partials_level","page":"PARTIALS","name":"Partials Level","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":579,"min":0,"max":100,"octaves":0,"id":256,"condition":"always"},{"key":"partials_count","page":"PARTIALS","name":"Partial Count","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":580,"min":1,"max":32,"octaves":0,"id":257,"condition":"always"},{"key":"partials_tilt","page":"PARTIALS","name":"Spectral Tilt","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":581,"min":-24,"max":12,"octaves":0,"id":258,"condition":"always"},{"key":"partials_stretch","page":"PARTIALS","name":"Partial Stretch","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":582,"min":0.25,"max":4,"octaves":0,"id":259,"condition":"always"},{"key":"partials_harmonics","page":"PARTIALS TONE","name":"Harmonic Count","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":584,"min":1,"max":16,"octaves":0,"id":260,"condition":"always"},{"key":"partials_subs","page":"PARTIALS TONE","name":"Subharmonic Count","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":585,"min":0,"max":16,"octaves":0,"id":261,"condition":"always"},{"key":"partials_balance","page":"PARTIALS TONE","name":"Subharmonic Balance","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":586,"min":0,"max":100,"octaves":0,"id":262,"condition":"always"},{"key":"partials_detune","page":"PARTIALS TONE","name":"Partial Detune","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":587,"min":0,"max":30,"octaves":0,"id":263,"condition":"always"},{"key":"partials_drift","page":"PARTIALS TONE","name":"Partial Pitch Drift","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":588,"min":0,"max":30,"octaves":0,"id":264,"condition":"always"},{"key":"partials_drift_rate","page":"PARTIALS TONE","name":"Partial Drift Rate","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":589,"min":0.01,"max":20,"octaves":1,"id":265,"condition":"always"},{"key":"partials_damping","page":"PARTIALS TONE","name":"Upper Partial Decay","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":590,"min":0,"max":100,"octaves":0,"id":266,"condition":"always"},{"key":"partials_time","page":"PARTIALS TONE","name":"Spectral Decay Time","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":591,"min":10,"max":12000,"octaves":1,"id":267,"condition":"always"},{"key":"partials_gap","page":"PARTIALS PACKET","name":"Packet Interval","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":592,"min":10,"max":2000,"octaves":1,"id":268,"condition":"always"},{"key":"partials_scatter","page":"PARTIALS PACKET","name":"Packet Timing Scatter","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":593,"min":0,"max":100,"octaves":0,"id":269,"condition":"always"},{"key":"partials_rise","page":"PARTIALS PACKET","name":"Packet Rise","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":594,"min":0.5,"max":200,"octaves":1,"id":270,"condition":"always"},{"key":"partials_sustain","page":"PARTIALS PACKET","name":"Packet Sustain","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":595,"min":0,"max":1000,"octaves":0,"id":271,"condition":"always"},{"key":"partials_decay","page":"PARTIALS PACKET","name":"Packet Decay","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":596,"min":1,"max":4000,"octaves":1,"id":272,"condition":"always"},{"key":"partials_variation","page":"PARTIALS PACKET","name":"Packet Variation","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":597,"min":0,"max":100,"octaves":0,"id":273,"condition":"always"},{"key":"partials_pitch_jitter","page":"PARTIALS PACKET","name":"Packet Pitch Variatio","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":598,"min":0,"max":12,"octaves":0,"id":274,"condition":"always"},{"key":"partials_shape","page":"PARTIALS PACKET","name":"Packet Envelope Shape","scope":"engine","engines":[20],"modes":[0,1,2,3,4,5,6],"parameter":599,"min":0.25,"max":4,"octaves":0,"id":275,"condition":"always"},{"key":"lowform_width","page":"LOWFORM","name":"Lowform Stereo Width","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":706,"min":0,"max":100,"octaves":0,"id":276,"condition":"always"},{"key":"lowform_level","page":"LOWFORM","name":"Lowform Level","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":707,"min":0,"max":100,"octaves":0,"id":277,"condition":"always"},{"key":"lowform_foundation_level","page":"LOWFORM","name":"Foundation Level","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":710,"min":0,"max":100,"octaves":0,"id":278,"condition":"always"},{"key":"lowform_body_level","page":"LOWFORM","name":"Body Level","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":711,"min":0,"max":100,"octaves":0,"id":279,"condition":"always"},{"key":"lowform_a","page":"LOWFORM BODY","name":"Body Control A","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":712,"min":0,"max":100,"octaves":0,"id":280,"condition":"always"},{"key":"lowform_b","page":"LOWFORM BODY","name":"Body Control B","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":713,"min":0,"max":100,"octaves":0,"id":281,"condition":"always"},{"key":"lowform_c","page":"LOWFORM BODY","name":"Body Control C","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":714,"min":0,"max":100,"octaves":0,"id":282,"condition":"always"},{"key":"lowform_d","page":"LOWFORM BODY","name":"Body Control D","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":715,"min":0,"max":100,"octaves":0,"id":283,"condition":"always"},{"key":"lowform_e","page":"LOWFORM BODY","name":"Body Control E","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":716,"min":0,"max":100,"octaves":0,"id":284,"condition":"always"},{"key":"lowform_body_decay","page":"LOWFORM BODY","name":"Body Decay","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":717,"min":20,"max":12000,"octaves":1,"id":285,"condition":"always"},{"key":"lowform_texture_level","page":"LOWFORM TEXTURE","name":"Texture Level","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":719,"min":0,"max":100,"octaves":0,"id":286,"condition":"always"},{"key":"lowform_texture_color","page":"LOWFORM TEXTURE","name":"Texture Colour","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":720,"min":0,"max":100,"octaves":0,"id":287,"condition":"always"},{"key":"lowform_texture_track","page":"LOWFORM TEXTURE","name":"Texture Pitch Track","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":721,"min":0,"max":100,"octaves":0,"id":288,"condition":"always"},{"key":"lowform_texture_width","page":"LOWFORM TEXTURE","name":"Texture Width","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":722,"min":0,"max":100,"octaves":0,"id":289,"condition":"always"},{"key":"lowform_cutoff","page":"LOWFORM","name":"Filter Cutoff","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":724,"min":20,"max":18000,"octaves":1,"id":290,"condition":"always"},{"key":"lowform_resonance","page":"LOWFORM","name":"Filter Resonance","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":725,"min":0,"max":95,"octaves":0,"id":291,"condition":"always"},{"key":"lowform_filter_drive","page":"LOWFORM SHAPE","name":"Filter Drive","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":726,"min":0,"max":100,"octaves":0,"id":292,"condition":"always"},{"key":"lowform_keytrack","page":"LOWFORM SHAPE","name":"Filter Key Track","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":727,"min":-100,"max":100,"octaves":0,"id":293,"condition":"always"},{"key":"lowform_filter_env","page":"LOWFORM SHAPE","name":"Filter Envelope","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":728,"min":-8,"max":8,"octaves":0,"id":294,"condition":"always"},{"key":"lowform_filter_decay","page":"LOWFORM SHAPE","name":"Filter Envelope Decay","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":729,"min":5,"max":4000,"octaves":1,"id":295,"condition":"always"},{"key":"lowform_punch","page":"LOWFORM BODY","name":"Pitch Punch","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":730,"min":-24,"max":48,"octaves":0,"id":296,"condition":"always"},{"key":"lowform_punch_time","page":"LOWFORM BODY","name":"Pitch Punch Time","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":731,"min":1,"max":1000,"octaves":1,"id":297,"condition":"always"},{"key":"lowform_drive","page":"LOWFORM TEXTURE","name":"Lowform Overload","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":732,"min":0,"max":100,"octaves":0,"id":298,"condition":"always"},{"key":"lowform_low_return","page":"LOWFORM SHAPE","name":"Foundation Low Return","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":733,"min":0,"max":100,"octaves":0,"id":299,"condition":"always"},{"key":"lowform_texture_decay","page":"LOWFORM TEXTURE","name":"Texture Decay","scope":"engine","engines":[21],"modes":[0,1,2,3,4,5,6],"parameter":734,"min":20,"max":12000,"octaves":1,"id":300,"condition":"always"}];
/* LFO_DESTINATIONS_END */
const LFO_FIELDS=['dest','wave','clock','rate','depth','mode','phase','smooth','flow','target','engine','offset'];
const LFO_BEATS=[.0625,1/12,.125,1/6,.25,1/3,.5,2/3,1,1.5,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256];
const LFO_BEAT_NAMES=['1/64','1/32T','1/32','1/16T','1/16','1/8T','1/8','1/4T','1/4','1/4D','1/2','1/2D','1BAR','1.5BAR','2BAR','3BAR','4BAR','6BAR','8BAR','12BAR','16BAR','24BAR','32BAR','48BAR','64BAR'];
const LFO_LABELS=[null,['SINE','TRI','SAW','RAMP','PULS','RAND','DRFT'],['BEAT','HZ'],null,null,['FREE','PLAY','TRIG','ONE'],null,null,['CLIP','LIVE'],['A','B','C','D','FX'],null,['BASE','AUTO']];
const LFO_SHORT=['DEST','WAVE','CLCK','RATE','DPTH','MODE','PHAS','SMTH','FLOW','PART','ENG','OFST'];
const LFO_ENGINE_NAMES=['MOTN','LANE','GRNS','CUTS','SMPL','SLIC','GLCH','WEAV','WSET','PRSM','MORF','TXTR','FORM','STOC','PLSR','TABLE','SCAN','FM'];
PARAMS.lfo_select={key:'lfo_select',label:'LFO',shortLabel:'LFO',min:1,max:8,step:1,value:1,integer:true,lfoField:'select'};
for(let slot=0;slot<8;++slot){
 LFO_FIELDS.forEach((field,i)=>{const key='lfo'+(slot+1)+'_'+field;PARAMS[key]={key,label:LFO_SHORT[i],shortLabel:LFO_SHORT[i],value:[0,0,0,1,0,0,0,10,0,0,-1,0][i],min:[0,0,0,1/256,-1,0,0,3,0,0,-1,0][i],max:[LFO_DESTINATIONS.length-1,6,1,16,1,3,1,1000,1,4,15,1][i],step:[1,1,1,.01,.01,1,.01,1,1,1,1,1][i],integer:[0,1,2,5,8,9,10,11].includes(i),labels:LFO_LABELS[i],lfoSlot:slot,lfoField:field};});
 for(const field of ['reset','rate','depth']){const source=PARAMS['lfo'+(slot+1)+'_'+field],key='seq_lfo'+(slot+1)+'_'+field;PARAMS[key]={...(source||{}),key,label:field==='reset'?'RST':field==='rate'?'RATE':'DPTH',shortLabel:field==='reset'?'RST':field==='rate'?'RATE':'DPTH',value:field==='reset'?1:field==='rate'?1:0,min:field==='reset'?0:source.min,max:field==='reset'?1:source.max,step:field==='reset'?1:source.step,integer:field==='reset',labels:field==='reset'?['OFF','ON']:null,lfoSlot:slot,lfoField:field,lfoLock:true};}
}
const LFO_PAGE={title:'LFO',lfo:true,keys:[]};
const LFO_SETUP_PAGE={title:'LFO SETUP',lfoSetup:true,keys:[]};
const LFO_PAGES=[LFO_PAGE,LFO_SETUP_PAGE];
const SEQUENCE_LFO_PAGE={title:'TRIG LFO',sequence:true,sequenceLfo:true,keys:[]};
function syncLfoPageKeys(){const p='lfo'+((lfoUi.data?.selected||0)+1)+'_',q='seq_'+p;
 LFO_PAGE.keys=[null,p+'target',p+'dest',p+'depth',p+'wave',p+'clock',p+'rate',p+'mode'];
 LFO_SETUP_PAGE.keys=[null,p+'engine',p+'phase',p+'smooth',p+'flow',p+'offset',null,null];
 SEQUENCE_LFO_PAGE.keys=[null,q+'reset',q+'rate',q+'depth',null,null,null,null];
}syncLfoPageKeys();
function readControlLfos(cachedOnly=false){
 let d;try{d=JSON.parse(String(cachedOnly?uiPolling.cache.get('lfo_state'):getParam('lfo_state','{}')));}catch(_){return;}
 if(!Number.isInteger(d.selected)||d.selected<0||d.selected>7||!Array.isArray(d.values)||d.values.length!==8||d.values.some(v=>!Array.isArray(v)||![11,12].includes(v.length)||v.some(x=>!Number.isFinite(x)))||!Array.isArray(d.allowed)||d.allowed.some(x=>!Number.isInteger(x)||x<0||x>=LFO_DESTINATIONS.length))return;
 for(const v of d.values)if(v.length===11)v.push(0);
 if(state.view==='lfoDest'&&lfoUi.picker&&(lfoUi.picker.slot!==d.selected||(lfoUi.picker.context&&lfoUi.picker.context!==lfoDestContext(d))))closeLfoDestPicker();
 const oldTitle=lfoUi.data&&state.view==='settings'?currentSettingsPage()?.title:null;lfoUi.data=d;PARAMS.lfo_select.value=d.selected+1;
 for(let s=0;s<8;++s){LFO_FIELDS.forEach((f,i)=>PARAMS['lfo'+(s+1)+'_'+f].value=d.values[s][i]);if(d.trig?.length===18){const t=d.trig;PARAMS['seq_lfo'+(s+1)+'_reset'].value=(t[1]>>s)&1;PARAMS['seq_lfo'+(s+1)+'_rate'].value=t[0]&(1<<(s*2))?t[2+s*2]:d.values[s][3];PARAMS['seq_lfo'+(s+1)+'_depth'].value=t[0]&(2<<(s*2))?t[3+s*2]:d.values[s][4];}}
 syncLfoPageKeys();if(oldTitle){let n=parameterPages().findIndex(p=>p.title===oldTitle);if(n<0&&oldTitle==='TRIG LFO')n=parameterPages().findIndex(p=>p.title==='LFO');if(n>=0)state.settingsPage=n;}
}
function readControlLfoLive(cachedOnly=false){let d;try{d=JSON.parse(String(cachedOnly?uiPolling.cache.get('lfo_live'):getParam('lfo_live','{}')));}catch(_){return;}
 if(!Number.isInteger(d.owner)||!Array.isArray(d.v)||d.v.length>24||d.v.some(r=>!Array.isArray(r)||r.length!==4||!Number.isInteger(r[0])||r[0]<0||r[0]>4||typeof r[1]!=='string'||!Number.isFinite(r[2])||!Number.isFinite(r[3])))return;lfoUi.live=d;
}
function lfoDisplayControl(c){
 if(arguments[0]?.trigLocked)return arguments[0];
 c=playbackDisplayControl(c);
 if(!c||c.lfoField||c.modulated||!lfoUi.live||lfoUi.live.owner!==(state.splitEnabled?state.splitFocus:0))return c;
 let owner=state.splitEnabled?state.splitFocus:0;
 const p=c.key.match(/^seq_pan_([1-4])$/),s=c.key.match(/^split_(?:level|delay|reverb)_([abcd])$/);
 if(p)owner=Number(p[1])-1;else if(s)owner='abcd'.indexOf(s[1]);else if(LFO_DESTINATIONS.some(d=>d.key===c.key&&(d.scope==='shared'||d.scope==='layers')))owner=4;
 const row=lfoUi.live.v.find(r=>r[0]===owner&&r[1]===c.key);if(!row)return c;
 return {...c,value:clamp(row[2],c.min,c.max),baseValue:c.baseValue??c.value,modulated:true};
}
function controlLfoForKey(key){
 const c=PARAMS[key];if(!c?.lfoField)return null;if(c.lfoField==='select')return c;
 const data=lfoUi.data,base=data?.values?.[c.lfoSlot];if(!base)return null;
 const dest=LFO_DESTINATIONS[Math.round(base[0])];
 if(c.lfoLock&&(!sequenceUi.active||!base[0]||![state.splitFocus,4].includes(base[9])||!data.allowed.includes(base[0])||(c.lfoField==='reset'&&base[5]<2)))return null;
 if(c.lfoField==='engine'&&(base[9]===4||!data.parts.includes(base[9])))return null;
 return {...c,values:c.lfoField==='dest'?data.allowed:c.lfoField==='target'?data.parts.filter(p=>lfoActiveParts().includes(p)):c.lfoField==='engine'?data.engines:undefined,lfoClock:base[2]};
}
function lfoBeatIndex(rate){let best=0,distance=Infinity;LFO_BEATS.forEach((b,i)=>{const x=Math.abs(Math.log2(Math.max(rate,1e-12)*b));if(x<distance){distance=x;best=i;}});return best;}
function controlLfoText(c,compact=false){
 if(c.lfoField==='select')return Math.round(c.value)+'/8';
 if(c.lfoField==='dest'){const d=LFO_DESTINATIONS[Math.round(c.value)];if(!d)return 'OFF';if(!c.automated&&!lfoUi.data?.allowed.includes(c.value))return '□';return compact?String(SHORT_LABELS[d.key]||PARAMS[d.key]?.shortLabel||({part_level:'LVL',part_pan:'PAN',part_delay:'DLY',part_reverb:'REV'}[d.key])||d.name).slice(0,4):d.page+' / '+d.name;}
 if(c.lfoField==='target')return lfoActiveParts().includes(Math.round(c.value))?(LFO_LABELS[9][Math.round(c.value)]||'?'):'□';
 if(c.lfoField==='engine')return c.value<0?'ANY':(lfoUi.data?.engines?.includes(c.value)?LFO_ENGINE_NAMES[Math.round(c.value)]:'□');
 if(c.lfoField==='rate'){if(c.lfoClock===0){const n=LFO_BEAT_NAMES[lfoBeatIndex(c.value)];return compact?n.replace('BAR','B').replace(/^1\/(.*[TD])$/,'$1'):n;}return compact?(c.value<.1?Math.round(c.value*1000)+'mHz':Number(c.value.toPrecision(2)).toString()):Number(c.value.toPrecision(3))+' Hz';}
 if(c.lfoField==='depth')return Math.round(c.value*100)+(compact?'':'%');if(c.lfoField==='phase')return Math.round(c.value*100)+'%';if(c.lfoField==='smooth')return c.value>=1000?'1s':Math.round(c.value)+(compact?'':' ms');return c.labels?.[Math.round(c.value)]||String(c.value);
}
function adjustControlLfo(index,delta){const page=currentSettingsPage(),c=index===0?PARAMS.lfo_select:controlLfoForKey(page.keys[index]);if(!c||!delta)return;
 if(c.lfoLock&&state.deleteHeld){setParamBlocking(c.key,'reset');readControlLfos();return;}
 delta=parameterEncoderDelta(index,c.key,delta);if(!delta)return;
 if(c.lfoField==='dest'){openLfoDestPicker();return;}
 const value=c.values&&!c.values.includes(c.value)?c.values[0]:nextControlValue(c,delta);setParamBlocking(c.key,String(value));readControlLfos();
}
function lfoDestinationPage(d){if(d.page==='PACKETS'){const sound=lfoUi.data?.motionSound||0;return sound===2?'MOTOR':sound>=3?'EVENT SHAPE':'PACKETS';}if(d.key==='motor_symmetry'&&(lfoUi.data?.motionSound||0)>=3)return 'EVENT SHAPE';if(!state.splitEnabled&&(d.page==='PART COLOR'||d.page==='DELAY RETURN'))return 'EFFECTS';const cfg=lfoUi.data?.values?.[lfoUi.data.selected];if(d.key==='stretch_ratio'&&((cfg?.[10]??-1)===4||(cfg?.[10]===-1&&lfoUi.data?.primary===4)))return 'SAMPLE PLAY';return d.page;}
function lfoDestinationName(d){if(d.key==='effect_mix')return state.splitEnabled?'Delay Return':'Delay Mix';if(d.key==='effect_reverb_mix')return state.splitEnabled?'Reverb Return':'Reverb Mix';return d.name;}
function lfoDestContext(d=lfoUi.data){if(!d)return '';const c=d.values[d.selected];return [d.selected,c[9],c[10],d.primary,d.motionSound,...d.allowed].join(':');}
function lfoDestGroups(){
 const groups=[{name:'Off',ids:[0]}],cfg=lfoUi.data?.values?.[lfoUi.data.selected],primary=cfg?.[10]>=0?cfg[10]:lfoUi.data?.primary;
 for(const id of lfoUi.data?.allowed||[]){if(!id)continue;const d=LFO_DESTINATIONS[id],name=lfoDestinationPage(d);let group=groups.find(g=>g.name===name);if(!group){group={name,ids:[]};groups.push(group);}group.ids.push(id);}
 const rank=g=>g.ids[0]===0?-1:g.ids.every(id=>LFO_DESTINATIONS[id].scope==='engine')?(g.name==='ENGINE'?0:g.ids.some(id=>LFO_DESTINATIONS[id].engines.includes(primary))?1:2):3;
 groups.sort((a,b)=>rank(a)-rank(b));
 for(const g of groups)if(g.ids.every(id=>LFO_DESTINATIONS[id].scope==='engine'))g.ids.sort((a,b)=>LFO_DESTINATIONS[a].parameter-LFO_DESTINATIONS[b].parameter);
 return groups;
}
function openLfoDestPicker(){if(!lfoUi.data)return;const cfg=lfoUi.data.values[lfoUi.data.selected],groups=lfoDestGroups();lfoUi.picker={context:lfoDestContext(),slot:lfoUi.data.selected,stage:'groups',group:Math.max(0,groups.findIndex(g=>g.ids.includes(Math.round(cfg[0])))),selection:Math.max(0,groups.findIndex(g=>g.ids.includes(Math.round(cfg[0]))))};state.view='lfoDest';state.actionMessageTicks=0;resetParameterEncoderMotion();draw();}
function closeLfoDestPicker(){lfoUi.picker=null;state.view='settings';state.settingsPage=Math.max(0,parameterPages().findIndex(p=>p.title==='LFO'));resetParameterEncoderMotion();}
function lfoPickerRows(){const ui=lfoUi.picker,groups=lfoDestGroups();ui.group=clamp(ui.group,0,groups.length-1);return ui.stage==='groups'?groups:groups[ui.group].ids.map(id=>({name:lfoDestinationName(LFO_DESTINATIONS[id]),id}));}
function drawLfoDestPicker(){clearDisplay();const ui=lfoUi.picker;if(!ui){closeLfoDestPicker();return;}
 const groups=lfoDestGroups(),rows=lfoPickerRows();ui.selection=clamp(ui.selection,0,rows.length-1);const cfg=lfoUi.data.values[ui.slot];drawTitleWithFraction(ui.stage==='groups'?'DEST '+(LFO_LABELS[9][cfg[9]]||'?'):groups[ui.group].name,(ui.selection+1)+'/'+rows.length);
 const start=clamp(ui.selection-2,0,Math.max(0,rows.length-4));for(let n=0;n<4&&n+start<rows.length;++n){const i=n+start,y=13+n*10;if(i===ui.selection)drawRect(0,y-2,128,9,1);print(3,y,String(rows[i].name).slice(0,21),i===ui.selection?0:1);}
 print(1,57,ui.stage==='groups'?'SELECT A PAGE':'CLICK: ASSIGN',1);
}
function handleLfoPickerMidi(status,number,value){
 if(state.view!=='lfoDest'&&!(state.view==='settings'&&status===0xb0&&number===MoveMainButton&&value>0))return false;
 const page=state.view==='settings'?currentSettingsPage():null;
 if(state.view!=='lfoDest'){if(status===0xb0&&number===MoveMainButton&&value>0&&page?.lfo){openLfoDestPicker();return true;}return false;}
 if(status!==0xb0)return false;const ui=lfoUi.picker;if(!ui)return true;
 if(number===MoveMainKnob||number===73){const delta=decodeDelta(value),rows=lfoPickerRows();ui.selection=clamp(ui.selection+delta,0,rows.length-1);draw();return true;}
 if(number===MoveUp||number===MoveDown){if(value>0){ui.selection=clamp(ui.selection+(number===MoveUp?-1:1),0,lfoPickerRows().length-1);draw();}return true;}
 if(number===MoveBack||number===MoveMenu){if(value>0){if(number===MoveBack&&ui.stage==='params'){ui.stage='groups';ui.selection=ui.group;}else closeLfoDestPicker();draw();}return true;}
 if(number===MoveMainButton){if(value>0){const groups=lfoDestGroups(),rows=lfoPickerRows();if(ui.stage==='groups'&&ui.selection>0){ui.group=ui.selection;ui.stage='params';ui.selection=Math.max(0,groups[ui.group].ids.indexOf(Math.round(lfoUi.data.values[ui.slot][0])));}else{const id=ui.stage==='groups'?0:rows[ui.selection].id;setParamBlocking('lfo'+(ui.slot+1)+'_dest',String(id));readControlLfos();if(lfoUi.data.values[ui.slot][0]===id)closeLfoDestPicker();else showActionMessage('DEST UNAVAILABLE');}draw();}return true;}
 if(number>=71&&number<=78)return true;return false;
}
function drawControlLfo(page){lfoUi.visible=true;readControlLfos(uiPolling.supported===true);readControlLfoLive(uiPolling.supported===true);clearDisplay();drawSettingsTitle(page.sequenceLfo?'TRIG LFO':page.lfoSetup?'LFO SETUP':'LFO');drawRect(0,8,128,1,1);
 for(let i=0;i<8;++i){const c=i===0?PARAMS.lfo_select:controlLfoForKey(page.keys[i]);drawParameterCell(i,c,true,4);if(c?.lfoLock){const t=lfoUi.data?.trig||[0,255],bit=1<<c.lfoSlot,locked=c.lfoField==='reset'?!(t[1]&bit):Boolean(t[0]&((c.lfoField==='rate'?1:2)<<(c.lfoSlot*2)));if(locked)drawRect(i*32+28,12,3,3,1);}}
 if(page.sequenceLfo)print(1,57,'X+DIAL: INHERIT',1);else if(page.lfoSetup)print(1,57,'CLIP / LIVE FLOW',1);else{const d=LFO_DESTINATIONS[Math.round(lfoUi.data?.values?.[lfoUi.data.selected]?.[0]||0)];print(1,57,(!lfoActiveParts().includes(lfoUi.data?.values?.[lfoUi.data.selected]?.[9])?'PART '+(LFO_LABELS[9][lfoUi.data?.values?.[lfoUi.data.selected]?.[9]]||'?')+' INACTIVE':d?.id?lfoDestinationName(d):'DEST: CLICK TO CHOOSE').slice(0,21),1);}
}

const SEQUENCE_KEYS=['seq_length','seq_rate','seq_rotation','seq_active','seq_mute','seq_velocity','seq_gate','seq_scale','seq_step','seq_child','seq_pos','seq_note','seq_vel','seq_trig_gate','seq_micro','seq_prob','seq_burst','seq_when','seq_clip_beats','seq_duration_mode','seq_time_ms','seq_trig_time_ms','seq_pan_1','seq_pan_2','seq_pan_3','seq_pan_4','seq_eq_low','seq_eq_mid','seq_eq_high','seq_eq_freq','seq_eq_solo','seq_duck_key','seq_duck_depth','seq_duck_threshold','seq_duck_attack','seq_duck_hold','seq_duck_release','seq_duck_hpf','seq_duck_knee','seq_cycle_hit','seq_cycle_every','seq_type','seq_to_normal','seq_regenerate','seq_recipe_hits','seq_recipe_map','seq_recipe_order','seq_recipe_repeat','seq_recipe_write','seq_recipe_amount','seq_recipe_seed','seq_recipe_regions','seq_recipe_preroll','seq_recipe_from','seq_recipe_to','seq_recipe_pitch','seq_recipe_span','seq_recipe_accents','seq_recipe_depth','seq_recipe_duration','seq_recipe_min','seq_recipe_max','seq_recipe_human','seq_recipe_probability','seq_direction','seq_recipe_shift','seq_path'];
const sequenceUi={active:false,data:null,pad:-1,downAt:0,held:false,copy:false,copyReady:false,fullRaw:'',revision:0,cursorRevision:0,headRevision:0,cursorLedPad:-1,cursorLedRefresh:-Infinity,cursorRestores:new Map()};
const sequenceDisplay={valid:false,revision:-1,cursorRevision:-1,headRevision:-1,waves:null,mask:-1,recording:false,tempo:NaN,selectedCursor:-1};
const sequenceLaneDisplay={valid:false,signature:''};
const SEQUENCE_LANE_PAGE={title:'SEQ LANE',sequence:true,keys:['seq_length','seq_rate','seq_rotation','seq_active','seq_velocity','seq_gate','tempo_bpm','seq_clip_beats']};
const SEQUENCE_TRIG_PAGE={title:'TRIG',sequence:true,sequenceTrig:true,keys:['seq_pos','seq_note','seq_vel','seq_trig_gate','seq_micro','seq_prob','seq_cycle_hit','seq_cycle_every']};
const SEQUENCE_BURST_PAGE={title:'BURST',sequence:true,sequenceBurst:true,keys:['seq_pos','seq_note','seq_vel','seq_trig_gate','seq_burst','seq_child','seq_when',null]};
const SEQUENCE_PITCH_PAGE={title:'SEQ SETUP',sequence:true,keys:['root_note','scale','seq_scale','seq_duration_mode','seq_type','seq_direction','seq_path',null]};
const SEQUENCE_MIX_PAGE={title:'MIXER',sequenceMix:true,mixerOverview:true,global:true,keys:['split_level_a','split_level_b','split_level_c','split_level_d','seq_pan_1','seq_pan_2','seq_pan_3','seq_pan_4']};
const SEQUENCE_STRIP_KEYS=['seq_eq_low','seq_eq_mid','seq_eq_high','seq_eq_freq','seq_eq_solo','mix_mute',null,null];
const SEQUENCE_STRIP_PAGES=Array.from({length:4},(_,lane)=>({title:'STRIP '+(lane+1),sequenceMix:true,sequenceStrip:lane,global:true,keys:SEQUENCE_STRIP_KEYS}));
const SEQUENCE_DUCK_PAGE={title:'PRIORITY DUCK',sequenceMix:true,global:true,keys:['seq_duck_key','seq_duck_depth','seq_duck_threshold','seq_duck_attack','seq_duck_hold','seq_duck_release','seq_duck_hpf','seq_duck_knee']};
const sequenceSpecs={
 seq_length:['LEN',1,32,1,16,true],seq_rate:['RATE',0,8,1,4,true,['1/1','1/2','1/4','1/8','1/16','1/16T','1/32','1/32T','1/64']],seq_rotation:['ROT',0,31,1,0,true],seq_active:['ON',0,1,1,1,true,['OFF','ON']],seq_mute:['MUTE',0,1,1,0,true,['OFF','ON']],
 seq_velocity:['VEL',1,127,1,100,true],seq_gate:['DUR',.01,32,.01,.8,false],seq_scale:['ENTRY',0,1,1,0,true,['CHROM','SCALE']],seq_step:['STEP',1,32,1,1,true],seq_child:['EVENT',0,8,1,0,true],
 seq_pos:['POS',0,1,.005,0,false],seq_note:['NOTE',-48,48,1,0,true],seq_vel:['VEL',1,127,1,100,true],seq_trig_gate:['DUR',.01,32,.01,.8,false],seq_micro:['MICRO',-.5,.5,.01,0,false],seq_prob:['PROB',0,1,.01,1,false],seq_cycle_hit:['HIT',1,8,1,1,true],seq_cycle_every:['EVERY',1,8,1,1,true],seq_burst:['BURST',1,8,1,1,true],seq_when:['WHEN',0,.999,.01,0,false],seq_clip_beats:['CLIP',1,64,1,4,true],seq_clear:['CLEAR',0,4,1,4,true],seq_delete:['DEL',0,1,1,1,true],
 seq_direction:['DIR',0,4,1,0,true,['FWD','REV','PING','F+B','RAND']],
 seq_path:['PATH',0,2,1,0,true,['HORIZ','VERT','DIAG']],
 seq_duration_mode:['UNIT',0,1,1,1,true,['STEP','TIME']],seq_time_ms:['DUR',10,30000,1,1000,false],seq_trig_time_ms:['DUR',10,30000,1,1000,false],
 seq_pan_1:['PAN1',-1,1,.01,0,false],seq_pan_2:['PAN2',-1,1,.01,0,false],seq_pan_3:['PAN3',-1,1,.01,0,false],seq_pan_4:['PAN4',-1,1,.01,0,false],
 seq_eq_low:['LOW',-12,12,.25,0,false],seq_eq_mid:['MID',-12,12,.25,0,false],seq_eq_high:['HIGH',-12,12,.25,0,false],seq_eq_freq:['FREQ',120,8000,1,900,false],seq_eq_solo:['SOLO',0,1,1,0,true,['OFF','ON']],
 seq_duck_key:['KEY',0,4,1,0,true,['OFF','L1','L2','L3','L4']],seq_duck_depth:['DEPTH',0,24,.5,0,false],seq_duck_threshold:['THR',-60,0,.5,-24,false],seq_duck_attack:['ATK',1,250,1,12,false],seq_duck_hold:['HOLD',0,1000,1,40,false],seq_duck_release:['REL',10,3000,1,220,false],seq_duck_hpf:['HPF',20,2000,1,100,false],seq_duck_knee:['KNEE',0,24,.5,6,false]
};
for(const [key,[label,min,max,step,value,integer,labels]] of Object.entries(sequenceSpecs))PARAMS[key]={key,label,shortLabel:label,min,max,step,value,integer,...(labels?{labels}:{}),...(['seq_clear','seq_delete'].includes(key)?{momentary:true}:{})};
function sequenceCursorPad(data){
 if(!data?.active||!data.playing)return -1;
 const step=data.lanes?.[data.edit]?.[0];
 if(!Number.isInteger(step)||step<0||step>=data.length)return -1;
 return (3-Math.floor(step/8))*8+step%8;
}
function syncSequenceCursorLeds(){
 const previous=sequenceUi.cursorLedPad,next=sequenceCursorPad(sequenceUi.data),now=Date.now();
 const changed=previous!==next;
 sequenceUi.cursorLedPad=next;
 if(changed){
   if(previous>=0)sequenceUi.cursorRestores.set(previous,4);
   if(next>=0)sequenceUi.cursorRestores.delete(next);
 }
 // Both our cache and Schwung's shared helper remember QUEUED colors.
 // The host's later LED flush can still lose a packet to backpressure. An
 // unchanged cursor must therefore be reissued, bypassing both color caches.
 // At most three cursor packets per UI frame: two old pads, then current.
 const refresh=changed||now<sequenceUi.cursorLedRefresh||now-sequenceUi.cursorLedRefresh>=12;
 if(refresh){
   sequenceUi.cursorLedRefresh=now;
   // Retain a restore until it has been issued four times, rather than
   // expiring by age while a fast sequence is still adding newer pads.
   const repair=changed&&previous>=0?[previous]:[];
   for(const pad of sequenceUi.cursorRestores.keys()){
     if(repair.length>=2)break;
     if(!repair.includes(pad))repair.push(pad);
   }
   for(const pad of repair){
     sendLedNow('note',MovePads[pad],padColor(pad),true);
     const remaining=(sequenceUi.cursorRestores.get(pad)||1)-1;
     if(remaining>0)sequenceUi.cursorRestores.set(pad,remaining);
     else sequenceUi.cursorRestores.delete(pad);
   }
   if(next>=0)sendLedNow('note',MovePads[next],padColor(next),true);
 }else{
   for(const pad of new Set([previous,next]))if(pad>=0&&pad<MovePads.length){
     const note=MovePads[pad],key=ledKey('note',note),color=padColor(pad);
     if(state.ledSent.get(key)!==color)sendLedNow('note',note,color);
     else state.ledPending.delete(key);
   }
 }
 if(!sequenceUi.active){sequenceUi.cursorRestores.clear();}
}
const sequenceSoundUi={data:null};
function readSequenceSound(){
 let d;try{d=JSON.parse(String(getParam('seq_sound_state','{}')));}catch(_){sequenceSoundUi.data=null;return;}
 if(!d||![0,1].includes(d.active)||!Number.isInteger(d.part)||!Number.isInteger(d.step)||!Number.isInteger(d.event)||!Array.isArray(d.values)||d.values.length>32||d.values.some(x=>!Array.isArray(x)||x.length!==3||!Number.isInteger(x[0])||!Number.isFinite(x[1])||![0,1].includes(x[2]))){sequenceSoundUi.data=null;return;}
 sequenceSoundUi.data=d;
}
function sequenceSoundActive(){const d=sequenceSoundUi.data;return sequenceUi.active&&sequenceUi.held&&d?.active===1&&d.part===state.splitFocus&&d.step===sequenceUi.data?.held;}
function sequenceSoundPageAvailable(page){
 if(page.sequenceTrig||page.sequenceBurst||page.sequenceEnvelope||page.sequenceLfo)return true;
 if(Number.isInteger(page.sequenceStrip)&&page.sequenceStrip!==state.splitFocus)return false;
 return page.keys.some(key=>{const id=SEQUENCE_SOUND_IDS[sequenceSoundKey(key)];return key==='locus'&&!isSynthAlgorithm(state.algorithm)||SEQUENCE_SOUND_SCOPES[id];});
}
function sequenceSoundKey(key){
 const part=state.splitFocus;
 for(const [prefix,target] of [['split_level_','part_level'],['split_delay_','part_delay'],['split_reverb_','part_reverb']])if(key?.startsWith(prefix))return 'abcd'.indexOf(key.slice(-1))===part?target:null;
 if(/^seq_pan_[1-4]$/.test(key||''))return Number(key.slice(-1))-1===part?'seq_pan_1':null;
 return key;
}
function sequenceSoundValue(key,fallback){
 if(!sequenceSoundActive())return fallback;
 const id=SEQUENCE_SOUND_IDS[sequenceSoundKey(key)],row=sequenceSoundUi.data.values.find(x=>x[0]===id);
 return row?row[1]:fallback;
}
function sequenceSoundControl(control){
 if(control?.key==='partials_count'&&sequenceSoundValue('partials_type',PARAMS.partials_type.value)===2)control={...control,max:8,value:Math.min(8,control.value)};
 if(!control||!sequenceSoundActive()||control.lfoField)return control;
 const id=SEQUENCE_SOUND_IDS[sequenceSoundKey(control.key)];
 if(control.key==='locus')return {...control,value:PARAMS.seq_pos.value,soundId:0,trigLocked:Boolean(sequenceUi.data?.mask&1),soundScope:'voice'};
 if(control.key.startsWith('seq_')&& !control.key.startsWith('seq_eq_')&&!control.key.startsWith('seq_pan_')){
  return ['seq_pos','seq_note','seq_vel','seq_trig_gate','seq_trig_time_ms','seq_micro','seq_prob','seq_cycle_hit','seq_cycle_every','seq_burst','seq_child','seq_when','seq_clear'].includes(control.key)?control:null;
 }
 if(!SEQUENCE_SOUND_SCOPES[id])return null;
 const row=sequenceSoundUi.data.values.find(x=>x[0]===id);
 return {...control,value:row?row[1]:control.value,soundId:id,soundScope:SEQUENCE_SOUND_SCOPES[id],trigLocked:Boolean(row),trigInherited:row?.[2]===0};
}
function adjustSequenceSound(control,index,delta){
 if(!sequenceSoundActive()||!control||!Number.isInteger(control.soundId))return false;
 if(state.deleteHeld){
  if(control.soundId===0)setParamBlocking('seq_clear',0);else setParamBlocking('seq_sound_clear',control.soundId);
 }else{
  delta=parameterEncoderDelta(index,control.key,delta);if(!delta)return true;
  const value=nextControlValue(control,delta);
  if(control.soundId===0)setParamBlocking('seq_pos',value);else setParamBlocking('seq_sound_edit',control.soundId+','+value);
 }
 const title=state.view==='settings'?currentSettingsPage().title:null;
 readSequence();
 if(title){const pages=parameterPages();const at=pages.findIndex(p=>p.title===title);state.settingsPage=at>=0?at:Math.max(0,pages.findIndex(p=>p.title==='TABLE'));}
 const error=String(getParam('last_error',''));if(error)showActionMessage(error.slice(0,21));
 else showMessage(state.deleteHeld?'INHERIT LANE':'TRIG '+String(sequenceUi.data.step+1).padStart(2,'0')+' '+control.label,25);
 return true;
}

function readSequence(skipCursorLeds=false){
 readSequenceSound();
 const raw=String(getParam('seq_state','{}'));
 let data;try{data=JSON.parse(raw);}catch(_){return;}
 if(!data||![0,1].includes(data.active))return;
 const previous=sequenceUi.data;
 const lanesChanged=JSON.stringify(previous?.lanes)!==JSON.stringify(data.lanes);
 const headsChanged=JSON.stringify(previous?.heads)!==JSON.stringify(data.heads);
 const stepsChanged=JSON.stringify(previous?.steps)!==JSON.stringify(data.steps);
 const cursorChanged=previous?.active!==data.active||previous?.playing!==data.playing||previous?.edit!==data.edit||lanesChanged;
 if(sequenceUi.held&&previous?.held>=0&&data.held<0){sequenceUi.held=false;sequenceUi.pad=-1;state.view='main';resetParameterEncoderMotion();}
 const wasActive=sequenceUi.active;
 sequenceUi.active=Boolean(data.active);sequenceUi.data=data;
 if(sequenceUi.active){PARAMS.split_mode.value=6;state.lastNonSingleMode=6;}
 else if(wasActive&&PARAMS.split_mode.value===6)PARAMS.split_mode.value=state.splitEnabled?(state.quadMode?3:state.sharedDual?2:1):layersUi.active?5:0;
 // Moving cursors and sample readers have their own partial redraw paths.
 // They must not invalidate the whole OLED on every full-state poll.
 const staticRaw=JSON.stringify({...data,lanes:undefined,heads:undefined});
 if(staticRaw!==sequenceUi.fullRaw){sequenceUi.fullRaw=staticRaw;sequenceUi.revision++;}
 if(cursorChanged)sequenceUi.cursorRevision++;
 if(headsChanged)sequenceUi.headRevision++;
 if(Number.isInteger(data.clipboard))sequenceUi.copyReady=sequenceUi.copyReady||Boolean(data.clipboard);
 if(Array.isArray(data.values))SEQUENCE_KEYS.forEach((key,i)=>{if(Number.isFinite(data.values[i]))PARAMS[key].value=data.values[i];});
 const timed=PARAMS.seq_duration_mode.value>=.5;
 SEQUENCE_LANE_PAGE.keys[5]=timed?'seq_time_ms':'seq_gate';
 SEQUENCE_TRIG_PAGE.keys[3]=timed?'seq_trig_time_ms':'seq_trig_gate';
 SEQUENCE_BURST_PAGE.keys[3]=timed?'seq_trig_time_ms':'seq_trig_gate';
 if(!skipCursorLeds||!sequenceUi.active)syncSequenceCursorLeds();
 if(cursorChanged||previous?.held!==data.held||stepsChanged)rebuildLedQueue();
}
function readSequenceCursor(){
 const data=sequenceUi.data;if(!sequenceUi.active||!data)return;
 const values=String(getParam('seq_cursor_state','')).split(',').map(Number);
 if(values.length!==23||values.some(v=>!Number.isInteger(v))||values[0]<0||values[0]>1||values[1]<0||values[1]>3||values[2]<0||values[2]>1)return;
 if(values[0]!==1||values[1]!==data.edit){readSequence();return;}
 const lanes=[];
 for(let p=0;p<4;++p){
   const row=values.slice(3+p*5,8+p*5);
   if(row[0]<-1||row[0]>=row[1]||row[1]<1||row[1]>32||row[2]<0||row[2]>8||row[3]<0||row[3]>1||row[4]<0||row[4]>1)return;
   lanes.push(row);
 }
 const selected=lanes[data.edit];
 const direction=Number.isInteger(data.values?.[64])?data.values[64]:0;
 if(values[2]&&selected[0]>=0){
   if(uiTiming.lane===data.edit&&uiTiming.length===selected[1]&&uiTiming.rate===selected[2]
      &&uiTiming.clip===state.activeClip&&uiTiming.direction===direction&&uiTiming.step>=0){
     const delta=(selected[0]-uiTiming.step+selected[1])%selected[1];
     const distance=direction===1?(selected[1]-delta)%selected[1]:direction===2||direction===3?Math.min(delta,selected[1]-delta):delta;
     if(direction!==4&&distance>1)uiTiming.jumps++;
   }
   Object.assign(uiTiming,{lane:data.edit,length:selected[1],rate:selected[2],step:selected[0],clip:state.activeClip,direction});
 }else uiTiming.step=-1;
 const changed=data.playing!==values[2]||JSON.stringify(data.lanes)!==JSON.stringify(lanes);
 if(!changed){syncSequenceCursorLeds();return;}
 data.playing=values[2];data.lanes=lanes;data.length=lanes[data.edit][1];
 sequenceUi.cursorRevision++;
 syncSequenceCursorLeds();
 rebuildLedQueue();
}
// Sequence drafting is a temporary transaction; generated notes remain normal
// clip content. Its controls are local tools, outside the fixed MIDI registry.
const sequenceGeneratorUi={data:null,raw:'',page:0};
const GEN_PAGES=[
 {title:'SEQ GENERATE',fields:[0,1,2,3,4,5,6,7]},
 {title:'GEN SOURCE',fields:[8,9,10,11,12,13,14,15]},
 {title:'GEN VALUES',fields:[16,17,18,19,20,null,null,null]},
].map((page,generatorPage)=>({...page,sequence:true,sequenceGenerator:true,generatorPage,keys:[]}));
const SEQUENCE_GENERATE_PAGE=GEN_PAGES[0];
const GEN_SPECS=[
 ['RHYM',0,2,1,['KEEP','EUC','AUDIO']],['HITS',0,32,1],['MAP',0,5,1,['KEEP','AUTO','TRANS','FILE','EDIT','EQUAL']],
 ['ORDER',0,6,1,['FWD','REV','PAL','RAND','WALK','CYCLE','SPRD']],['REPT',1,16,1],['LOCKS',0,1,1,['KEEP','RPLC']],['AMT',0,1,.01],['VAR',0,65535,1],
 ['EVTS',1,64,1],['PRER',-1,30,1],['FROM',0,1,.005],['TO',0,1,.005],['PITCH',0,5,1,['KEEP','UP','DOWN','PAL','RAND','WALK']],['SPAN',0,24,1],['ACCNT',0,32,1],['DEPTH',0,1,.01],
 ['DUR',0,3,1,['KEEP','SHORT','GAP','RAND']],['MIN',.01,32,.01],['MAX',.01,32,.01],['HUMAN',0,.25,.005],['PROB',-1,1,.01],
];

const RECIPE_KEYS=[null,...SEQUENCE_KEYS.slice(44,64),'seq_recipe_shift'];
PARAMS.seq_type={key:'seq_type',label:'SEQUENCER TYPE',shortLabel:'TYPE',min:0,max:2,step:1,integer:true,value:0,labels:['NORM','EUC','ONSET']};
for(const [key,label] of [['seq_to_normal','PRINT'],['seq_regenerate','REGEN']])PARAMS[key]={key,label,shortLabel:label,min:0,max:1,step:1,integer:true,value:0,momentary:true};
PARAMS.mix_mute={key:'mix_mute',label:'CHANNEL MUTE',shortLabel:'MUTE',min:0,max:1,step:1,integer:true,value:0,labels:['OFF','ON']};
for(let i=1;i<21;++i){const [label,min,max,step,labels]=GEN_SPECS[i];const key=RECIPE_KEYS[i];PARAMS[key]={key,label,shortLabel:label,min,max,step,integer:step===1,value:[1,8,0,0,1,0,1,1,16,-1,0,1,0,12,0,.5,0,50,1000,0,-1][i],...(labels?{labels}:{}),recipeField:i};}
PARAMS.seq_recipe_shift={key:'seq_recipe_shift',label:'HIT SHIFT',shortLabel:'SHIFT',min:0,max:31,step:1,integer:true,value:0,recipeField:21};
const recipePage=(title,fields)=>({title,sequence:true,sequenceRecipe:true,keys:fields.map(i=>typeof i==='string'?i:i===null?null:RECIPE_KEYS[i])});
const EUCLIDEAN_PAGES=[recipePage('EUCLIDEAN',[1,21,14,15,7,6,3,4]),recipePage('EUC SOURCE',[2,8,10,11,9,5,'seq_to_normal','seq_regenerate']),recipePage('EUC VALUES',[12,13,19,20,16,17,18,null])];
const ONSET_PAGES=[recipePage('ONSET',[2,8,9,null,7,6,14,15]),recipePage('ONSET SOURCE',[3,4,10,11,null,5,'seq_to_normal','seq_regenerate']),recipePage('ONSET VALUES',[12,13,19,20,16,17,18,null])];
const mixerUi={data:null,raw:'',visible:false};
function readMixerState(){
 const raw=String(getParam('mixer_state','{}'));if(raw===mixerUi.raw)return;
 let d;try{d=JSON.parse(raw);}catch(_){return;}
 if(!d||!Number.isInteger(d.mode)||d.mode<0||d.mode>6||!Number.isInteger(d.count)||d.count<1||d.count>4||!Array.isArray(d.strips)||d.strips.length!==4||d.strips.some(row=>!Array.isArray(row)||row.length!==6||row.some(v=>!Number.isFinite(v)))||!Array.isArray(d.pan)||d.pan.length!==4||!Array.isArray(d.duck)||d.duck.length!==8)return;
 mixerUi.raw=raw;mixerUi.data=d;
 d.pan.forEach((v,i)=>{PARAMS['seq_pan_'+(i+1)].value=v;});
 ['seq_duck_key','seq_duck_depth','seq_duck_threshold','seq_duck_attack','seq_duck_hold','seq_duck_release','seq_duck_hpf','seq_duck_knee'].forEach((key,i)=>{PARAMS[key].value=d.duck[i];});
}
function currentSequenceType(){return sequenceUi.active?Math.round(PARAMS.seq_type.value):0;}
function recipeControl(key){
 const p=PARAMS[key],f=p?.recipeField;if(!p||!Number.isInteger(f))return null;
 const value=n=>PARAMS[RECIPE_KEYS[n]].value;
 if([1,21].includes(f)&&currentSequenceType()!==1)return null;
 if([3,4,9,10,11].includes(f)&&value(2)===0)return null;
 if(f===8&&!(value(2)===5||(value(2)===1&&(sequenceUi.data?.source_kind??0)===0)))return null;
 if(f===13&&value(12)===0)return null;
 if(f===15&&value(14)===0)return null;
 if([17,18].includes(f)&&(value(16)===0||(f===18&&value(16)===1)))return null;
 const timed=PARAMS.seq_duration_mode.value>=.5;
 return {...p,min:f===2&&currentSequenceType()===2?1:[17,18].includes(f)&&timed?10:p.min,max:[1,21].includes(f)?Math.max(f===21?0:1,(sequenceUi.data?.length||32)-(f===21?1:0)):[17,18].includes(f)&&timed?30000:p.max,step:[17,18].includes(f)&&timed?1:p.step};
}
function recipeValue(control){
 const v=control.value,f=control.recipeField;
 if(control.labels)return control.labels[Math.round(v)];
 if(f===9)return v<0?'AUTO':Math.round(v)+'ms';
 if(f===20&&v<0)return 'KEEP';
 if([6,10,11,15,19,20].includes(f))return Math.round(v*100)+'%';
 if(f===14&&v===0)return 'OFF';
 if([17,18].includes(f))return PARAMS.seq_duration_mode.value>=.5?sequenceTimeText(v):sequenceStepText(v);
 if(f===13)return Math.round(v)+'st';
 return String(Math.round(v));
}
function outputMixerPages(base){
 const count=state.splitEnabled?partCount():1;
 const labels=Array.from({length:count},(_,i)=>sequenceUi.active?String(i+1):state.splitEnabled?'ABCD'[i]:'1');
 const levels=state.splitEnabled?['split_level_a','split_level_b','split_level_c','split_level_d']:['output_gain'];
 const overview={...SEQUENCE_MIX_PAGE,keys:Array.from({length:8},(_,i)=>i<4?(i<count?levels[i]:null):(i-4<count?'seq_pan_'+(i-3):null))};
 const strips=Array.from({length:count},(_,i)=>({...SEQUENCE_STRIP_PAGES[i],title:'STRIP '+labels[i]}));
 const output=[overview,...strips,...(count>1?[SEQUENCE_DUCK_PAGE]:[])];
 const pages=base.filter(p=>!p.sequenceMix&&p!==PRIORITY_PAGE);
 const index=pages.findIndex(p=>MIX_PAGE_TITLES.has(p.title));pages.splice(index<0?pages.length:index,0,...output);return pages;
}
function sequenceRecipeStatus(){
 const status=String(sequenceUi.data?.type_status||'');
 if(!/[A-Z]{3}/.test(status))return '';
 return ({'TRANSIENTS NOT READY':'WAIT: TRANSIENTS','NO STARTS IN RANGE':'NO STARTS: RANGE'}[status]||status).slice(0,16);
}
function drawSequenceRecipePage(){
 const page=currentSettingsPage(),d=sequenceUi.data;
 drawSettingsTitle('L'+((d?.edit||0)+1)+' '+page.title);drawRect(0,8,128,1,1);
 for(let i=0;i<8;++i)drawParameterCell(i,controlForKey(page.keys[i]),false,5);
 const status=sequenceRecipeStatus();if(status)print(1,58,status,1);
}

function readSequenceGenerator(){
 const raw=String(getParam('seq_gen_state','{}'));if(raw===sequenceGeneratorUi.raw)return;
 let d;try{d=JSON.parse(raw);}catch(_){return;}
 if(d?.active===0){
   if(state.view==='seqGenerate')closeSequenceGenerator(false);
   return;
 }
 if(d?.active!==1||!Number.isInteger(d.part)||d.part<0||d.part>3||!Array.isArray(d.config)||d.config.length!==21||d.config.some(v=>!Number.isFinite(v))||!Array.isArray(d.steps)||d.steps.length!==32||!Number.isInteger(d.length)||d.length<1||d.length>32)return;
 sequenceGeneratorUi.raw=raw;sequenceGeneratorUi.data=d;rebuildLedQueue();
}
function openSequenceGenerator(page=0){
 const target=GEN_PAGES[clamp(page,0,GEN_PAGES.length-1)];
 state.settingsPage=Math.max(0,parameterPages().indexOf(target));
 if(state.view!=='seqGenerate'||!sequenceGeneratorUi.data?.active){
   sequenceGeneratorUi.data=null;sequenceGeneratorUi.raw='';setParamBlocking('seq_gen_begin',1);readSequenceGenerator();
 }
 if(!sequenceGeneratorUi.data?.active){state.view='settings';showActionMessage(String(getParam('last_error','GEN UNAVAILABLE')).slice(0,21));return;}
 sequenceUi.pad=-1;sequenceUi.held=false;sequenceGeneratorUi.page=target.generatorPage;
 state.view='seqGenerate';state.actionMessageTicks=0;resetParameterEncoderMotion();rebuildLedQueue();
}
function closeSequenceGenerator(applied=false,cancel=true){
 if(cancel)setParamBlocking('seq_gen_cancel',1);
 sequenceGeneratorUi.data=null;sequenceGeneratorUi.raw='';
 state.view='main';state.settingsPage=Math.max(0,parameterPages().indexOf(SEQUENCE_LANE_PAGE));
 if(!applied)returnToPagePicker();
 else {state.pageReturn=null;state.pageDestinationReturn=null;}
 resetParameterEncoderMotion();readSequence();rebuildLedQueue();
}
function generatorControl(field){
 const d=sequenceGeneratorUi.data;if(!d||field===null)return null;
 const v=d.config;
 if(field===1&&v[0]!==1)return null;
 if([3,4,9,10,11].includes(field)&&v[2]===0)return null;
 if(field===8&&![1,5].includes(v[2]))return null;
 if(field===13&&v[12]===0)return null;
 if(field===15&&v[14]===0)return null;
 if([17,18].includes(field)&&(v[16]===0||(field===18&&v[16]===1)))return null;
 const [label,min,max,step,labels]=GEN_SPECS[field];
 return {key:'gen_'+field,label,shortLabel:label,min:field===1?0:field===2&&v[0]===2?1:[17,18].includes(field)&&d.timed?10:min,
   max:field===1?d.length:[17,18].includes(field)&&d.timed?30000:max,
   step:[17,18].includes(field)&&d.timed?1:step,integer:step===1,value:v[field],...(labels?{labels}:{}),field};
}
function generatorValue(control){
 const {value:v,field:f}=control;
 if(control.labels)return control.labels[Math.round(v)];
 if(f===9)return v<0?'AUTO':Math.round(v)+'ms';
 if(f===20&&v<0)return 'KEEP';
 if([6,10,11,15,19,20].includes(f))return Math.round(v*100)+'%';
 if(f===14&&v===0)return 'OFF';
 if([17,18].includes(f)){
   if(sequenceGeneratorUi.data.timed)return sequenceTimeText(v);
   return sequenceStepText(v);
 }
 if(f===13)return Math.round(v)+'st';
 return String(Math.round(v));
}
function adjustSequenceGenerator(index,delta){
 const field=GEN_PAGES[sequenceGeneratorUi.page].fields[index],control=generatorControl(field);if(!control||!delta)return;
 delta=parameterEncoderDelta(index,control.key,delta);if(!delta)return;
 let value=nextControlValue(control,delta);
 if(field===20)value=control.value<0?(delta>0?0:-1):value<0?-1:value;
 if(field===9&&value<0)value=-1;
 state.actionMessageTicks=0;setParamBlocking('seq_gen_edit',field+','+value);sequenceGeneratorUi.raw='';readSequenceGenerator();
}
function generatorPadColor(pad){
 const d=sequenceGeneratorUi.data;if(state.view!=='seqGenerate'||!d)return null;
 const step=(3-Math.floor(pad/8))*8+pad%8;if(step>=d.length)return Black;
 if(sequenceUi.data?.playing&&sequenceUi.data.lanes?.[d.part]?.[0]===step)return NeonGreen;
 if(step===d.selected)return White;
 const row=d.steps[step];
 if(row[0])return row[2]?BrightYellow:Cyan;
 return row[2]?BrightRed:DeepRed;
}
function drawSequenceGenerator(){
 const d=sequenceGeneratorUi.data;clearDisplay();
 if(!d){print(1,0,'GEN UNAVAILABLE',1);return;}
 const page=GEN_PAGES[sequenceGeneratorUi.page];
 drawTitleWithFraction('L'+(d.part+1)+' '+page.title,settingsPagePosition());drawRect(0,8,128,1,1);
 drawWaveformLane(d.part,[0,1],16,4,120);
 for(const p of d.source||[])drawRect(4+Math.round(clamp(p,0,1)*119),10,1,2,1);
 for(let i=0;i<d.length;++i)if(d.steps[i][0])drawRect(4+Math.round(clamp(d.steps[i][1],0,1)*119),20,1,2,1);
 const selected=d.steps[d.selected];if(selected?.[0]){
   const x=4+Math.round(clamp(selected[1],0,1)*119);drawRect(x,11,1,11,1);drawRect(x-1,10,3,1,1);
 }
 const count=d.steps.slice(0,d.length).filter(s=>s[0]).length;
 const info=state.actionMessageTicks>0?state.actionMessage:d.valid?count+'/'+d.length+(d.dropped?' LIMIT '+d.dropped:d.events!==count?' '+d.events+'EV APPLY':d.config[2]===1?' '+d.source_kind+' CLICK':(state.clipPlaying?' LIVE CLICK':' PLAY/CLICK')):String(d.status||'REOPEN GENERATOR');
 print(1,23,info.slice(0,21),1);
 for(let i=0;i<8;++i){
   const c=generatorControl(page.fields[i]),x=i%4*32,y=i<4?32:48;
   if(!c){drawRect(x+14,y+5,4,4,1);drawRect(x+15,y+6,2,2,0);continue;}
   print(x+1,y,c.shortLabel.slice(0,5),1);print(x+1,y+7,generatorValue(c).slice(0,5),1);
 }
}
function handleSequenceGeneratorCc(number,value){
 if(state.view!=='seqGenerate')return false;
 if(number===MoveBack||number===MoveMenu){
   if(value>0)closeSequenceGenerator(false);
   return !state.shiftHeld&&!state.focusButtonHeld;
 }
 if(number===MoveUndo){if(value>0)closeSequenceGenerator(false);return true;}
 if(number===MovePlay){
   if(value>0){
     if(state.deleteHeld)return false;
     if(state.shiftHeld){closeSequenceGenerator(false);return false;}
     setParamBlocking('seq_gen_play',1);parseClipState(getParam('clip_state',''));readSequence();
     const error=String(getParam('last_error','')||'');
     if(error)showActionMessage(error.slice(0,21));
     rebuildLedQueue();
   }
   return true;
 }
 if(number===MoveRecord||number===MoveRecordAlt||number===MoveCopy){if(value>0)closeSequenceGenerator(false);return false;}
 if(number===MoveMainButton){
   if(value>0){
     if(state.shiftHeld){const d=sequenceGeneratorUi.data;setParamBlocking('seq_gen_edit','7,'+((Math.round(d.config[7])+1)%65536));readSequenceGenerator();}
     else if(!sequenceGeneratorUi.data?.valid){setParamBlocking('seq_gen_refresh',1);sequenceGeneratorUi.raw='';readSequenceGenerator();}
     else {setParamBlocking('seq_gen_apply',1);const error=String(getParam('last_error','')||'');if(error){showActionMessage(error.includes('transition')?'WAIT: CLIP TRANSITION':error.slice(0,21));readSequenceGenerator();}else closeSequenceGenerator(true,false);}
   }
   return true;
 }
 if(number===MoveMainKnob||number===MoveUp||number===MoveDown){
   const delta=number===MoveMainKnob?decodeDelta(value):value>0?(number===MoveUp?1:-1):0;
   if(delta){adjustSettingsPage(delta);resetParameterEncoderMotion();}
   return true;
 }
 if(number>=MoveKnob1&&number<MoveKnob1+8){if(state.focusButtonHeld)return false;adjustSequenceGenerator(number-MoveKnob1,decodeDelta(value));return true;}
 return false;
}

function sequencePages(base){
 const pages=base.filter(page=>!page.voiceEngines&&!page.voiceSetup&&page!==AMP_ENVELOPE_PAGE&&page.title!==STACK_PATH_PAGE.title&&page.title!=='PADS & SCALE'&&page!==QUAD_SETUP_PAGE&&page!==SPLIT_MIX_PAGE&&page!==PRIORITY_PAGE);
 const generator=currentSequenceType()===1?EUCLIDEAN_PAGES:currentSequenceType()===2?ONSET_PAGES:[];
 // Keep lane setup ahead of conditional pages, and the selected engine's
 // sound pages together before its fitted envelope and gain compensation.
 const soundPages=pages.filter(page=>page!==PLAY_GAIN_PAGE&&pageCategoryForPage(page)===0);
 const otherPages=pages.filter(page=>page!==PLAY_GAIN_PAGE&&pageCategoryForPage(page)!==0);
 return [SEQUENCE_LANE_PAGE,SEQUENCE_PITCH_PAGE,...generator,SEQUENCE_TRIG_PAGE,SEQUENCE_BURST_PAGE,...(lfoUi.data?.values?.some(v=>v[0]>0)?[SEQUENCE_LFO_PAGE]:[]),
   ...soundPages,SEQUENCE_ENVELOPE_PAGE,PLAY_GAIN_PAGE,
   SEQUENCE_MIX_PAGE,...SEQUENCE_STRIP_PAGES,SEQUENCE_DUCK_PAGE,...otherPages];
}
function sequencePadColor(pad){
 const preview=generatorPadColor(pad);if(preview!==null)return preview;
 const d=sequenceUi.data;if(!sequenceUi.active||!d)return null;
 const step=(3-Math.floor(pad/8))*8+pad%8;if(step>=d.length)return Black;
 if(sequenceUi.pad===pad&&sequenceUi.held)return White;
 if(d.playing&&d.lanes?.[d.edit]?.[0]===step)return NeonGreen;
 return d.steps?.[step]?.[0]?Cyan:DeepRed;
}
function sequencePadGesture(pad,pressed){
 if(state.view==='seqGenerate'){
   if(pressed){const step=(3-Math.floor(pad/8))*8+pad%8;if(step<sequenceGeneratorUi.data.length){setParamBlocking('seq_gen_select',step);sequenceGeneratorUi.raw='';readSequenceGenerator();}}
   return;
 }
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
   state.view='settings';state.settingsPage=Math.max(0,parameterPages().findIndex(page=>page.sequenceTrig));resetParameterEncoderMotion();
 }
}
function sequenceMarkers(center=18,overview=false){
 const d=sequenceUi.data;if(!d||isSynthAlgorithm(PARAMS.algorithm.value))return;
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
 if(isSynthAlgorithm(PARAMS.algorithm.value))return;
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
function drawSequenceHeader(d){
 const edit=d.edit+1,rec=state.recording?d.rec+1:edit;
 drawTitleWithFraction('EDIT '+edit+' REC '+rec,d.playing?'PLAY':'STOP');drawRect(0,8,128,1,1);
}
function drawSequenceWave(d){
 drawWaveformLane(d.edit,[0,1],18,4,120);sequenceMarkers(18,true);
 drawSequenceHeads(sequenceHeads());
}
function drawSequenceOverview(d){
 for(let lane=0;lane<4;++lane){const l=d.lanes[lane]||[-1,16,4,1,0],x=lane*32;
   if(lane===d.edit)drawRect(x,29,30,9,1);
   print(x+2,29,String(lane+1)+(l[4]?' M':l[3]?'':' OFF'),lane===d.edit?0:1);
   drawRect(x+2,40,27,1,1);if(l[0]>=0)drawRect(x+2+Math.round(l[0]*25/Math.max(1,l[1]-1)),39,2,3,1);
   print(x+1,46,(l[0]>=0?String(l[0]+1).padStart(2,'0'):'--')+'/'+String(l[1]).padStart(2,'0'),1);
 }
}
function drawSequenceFooter(d){
 const heads=sequenceHeads();
 const duration=heads.length?sequenceTimeText(heads[0][3]):PARAMS.seq_duration_mode.value>=.5
   ?sequenceTimeText(PARAMS.seq_time_ms.value):sequenceTimeText(Math.max(.25,PARAMS.seq_gate.value*sequenceStepMs()));
 print(1,57,'BPM '+Math.round(PARAMS.tempo_bpm.value)+'  TIME '+duration,1);
}
function drawSequence(){
 const d=sequenceUi.data;
 const full=!sequenceDisplay.valid||sequenceDisplay.revision!==sequenceUi.revision
   ||sequenceDisplay.waves!==state.waves||sequenceDisplay.mask!==state.mask
   ||sequenceDisplay.recording!==state.recording||sequenceDisplay.tempo!==PARAMS.tempo_bpm.value;
 if(!full&&sequenceDisplay.cursorRevision===sequenceUi.cursorRevision
   &&sequenceDisplay.headRevision===sequenceUi.headRevision)return;
 if(full){
   clearDisplay();
   if(!d){print(1,0,'SEQUENCE',1);return;}
   drawSequenceHeader(d);drawSequenceWave(d);drawSequenceOverview(d);drawSequenceFooter(d);
 }else if(d){
   drawRect(0,0,128,9,0);drawSequenceHeader(d);
   const selected=d.lanes?.[d.edit]?.[0]??-1;
   if(selected!==sequenceDisplay.selectedCursor
      ||sequenceDisplay.headRevision!==sequenceUi.headRevision){
     drawRect(0,9,128,20,0);drawSequenceWave(d);
   }
   drawRect(0,29,128,28,0);drawSequenceOverview(d);
   if(sequenceDisplay.headRevision!==sequenceUi.headRevision){
     drawRect(0,57,128,7,0);drawSequenceFooter(d);
   }
 }
 sequenceDisplay.valid=true;sequenceDisplay.revision=sequenceUi.revision;
 sequenceDisplay.cursorRevision=sequenceUi.cursorRevision;
 sequenceDisplay.headRevision=sequenceUi.headRevision;
 sequenceDisplay.waves=state.waves;sequenceDisplay.mask=state.mask;
 sequenceDisplay.recording=state.recording;sequenceDisplay.tempo=PARAMS.tempo_bpm.value;
 sequenceDisplay.selectedCursor=d?.lanes?.[d.edit]?.[0]??-1;
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
// STEP values are counts; reserve explicit ms/s suffixes for TIME values.
function sequenceStepText(value){const v=clamp(value,.01,32);return String(Number(v.toFixed(v<10?3:2)));}
function sequenceAmpDurationMs(){
 const locked=Boolean((sequenceUi.data?.mask||0)&8);
 if(PARAMS.seq_duration_mode.value>=.5)return clamp(locked?PARAMS.seq_trig_time_ms.value:PARAMS.seq_time_ms.value,10,30000);
 return Math.max(.25,(locked?PARAMS.seq_trig_gate.value:PARAMS.seq_gate.value)*sequenceStepMs());
}
function sequenceAmpTiming(display=false){
 const value=key=>{const c=sequenceSoundControl(PARAMS[key])||PARAMS[key];return display?lfoDisplayControl(c).value:c.value;};
 const dur=sequenceAmpDurationMs();
 const raw=[Math.max(0,value('amp_attack')),Math.max(0,value('amp_decay')),Math.max(3,value('amp_release'))];
 const scale=Math.min(1,dur/(raw[0]+raw[1]+raw[2]+1e-9));
 const sustain=clamp(value('amp_sustain')/100,0,1);
 const release=raw[2]*scale;
 const attack=Math.min(raw[0]*scale,Math.max(0,dur-release));
 // DSP uses a 1 ms safety decay when Sustain sits below the attack peak.
 const decay=Math.min(Math.max(0,dur-release-attack),Math.max(raw[1]*scale,sustain<.999999?1:0));
 return {dur,raw,scale,applied:[attack,decay,release],sustain};
}
function sequenceValue(control){
 const v=control.value,key=control.key;
 if(key.startsWith('seq_recipe_'))return recipeValue(control);
 if(key==='seq_to_normal'||key==='seq_regenerate')return 'APPLY';
 if(key==='seq_note')return sequenceNoteName(v);
 if(key==='seq_child')return v===0?'TRIG':String(Math.round(v));
 if(key==='seq_gate'||key==='seq_trig_gate')return sequenceStepText(v);
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
function layersPages(engine,pads,enginePages,lists){
    return [engine,pads,...lists,AMP_ENVELOPE_PAGE,PLAY_GAIN_PAGE,...enginePages,LAYERS_SETUP_PAGE,
        {...LAYERS_FX_PAGE,keys:LAYERS_FX_PAGE.keys.map((key,i)=>PARAMS.layers_fx0.value===0&&i>0&&i<7?null:key)},
        ...([2,3,4,5,6,7,8,9,10,12,13,14,15,16].includes(Math.round(PARAMS.layers_fx0.value))?[LAYERS_FX_DETAIL_PAGE]:[]),LAYERS_TOOLS_PAGE,MIX_REC_PAGE,SAMPLE_PAGE,MIDI_CONTROL_PAGE,MIDI_NOTES_PAGE,MIDI_MAPS_PAGE,
        LOOP_PAGE,PATTERN_FADE_PAGE,TAKE_PAGE,CHAIN_PAGE,CHAIN_FADE_PAGE,ENTRY_FADE_PAGE,CHAIN_TOOLS_PAGE];
}
function readLayers(){
    let data;try{data=JSON.parse(String(getParam('layers_view','{}')));}catch(_){return;}
    if(!data||![0,1].includes(data.active))return;
    const oldPage=state.view==='settings'&&layersUi.active?currentSettingsPage():null;
    const was=layersUi.active,previous=layersUi.data;
    layersUi.active=data.active===1;layersUi.data=data;
    const redrawLeds=was!==layersUi.active||data.fxPads!==previous?.fxPads||(data.touches||[]).some((value,index)=>value!==previous?.touches?.[index]);
    if(layersUi.active){PARAMS.split_mode.value=5;state.lastNonSingleMode=5;}
    else if(was){if(PARAMS.split_mode.value===5)PARAMS.split_mode.value=0;layersUi.focus=0;layersUi.pages=['','FX CORNER'];layersUi.returnView=null;layersUi.editCorner=-1;layersUi.holdPad=-1;layersUi.tapPad=-1;layersUi.wave=false;resetParameterEncoderMotion();rebuildLedQueue();}
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
    const title='LAYERS '+(layersUi.focus?'FX ':'A ')+(voiceEngineListEnabled()||state.engineListVoices?'LIST':ALGORITHMS[state.algorithm]||'ENGINE'),fraction=state.clipRecording?'REC':'V'+(data.voices||[]).length;
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
    const window=normalizedWaveWindow([state.fieldStart,state.fieldEnd]),windowKey=window.join(',')+':'+tableUi.scopeKey;
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
        const algorithm=voiceEngineListEnabled()||state.engineListVoices?state.voiceAlgorithms[voice]:state.algorithm;
        if(algorithm===3) {
            if(cut?.outgoingGain>0)reader(cut.outgoingPosition,cut.outgoingLane,true);
            if(cut?.gain>0)reader(cut.position,cut.lane);
        } else if(algorithm===8) {
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

function decodeSplitState(text) {
    let data;
    try {data=typeof text==='object'?text:JSON.parse(String(text));}catch(_){return null;}
    if(!data||typeof data!=='object'||![0,1].includes(data.enabled)
        ||!Number.isInteger(data.focus)||data.focus<0||data.focus>(data.enabled&&data.quad?3:1)
        ||!Array.isArray(data.parts)||data.parts.length!==(data.enabled&&data.quad?4:2)
        ||data.parts.some(part=>!part||typeof part!=='object'))return null;
    if(data.mode!==undefined&&(!Number.isInteger(data.mode)||data.mode<0||data.mode>6
        ||Boolean(data.enabled)!==[1,2,3,4,6].includes(data.mode)
        ||(data.enabled&&(Boolean(data.quad)!==[3,6].includes(data.mode)
            ||Boolean(data.shared)!==[2,4].includes(data.mode)))
        ||Boolean(data.bank)!==(data.mode===4)))return null;
    return data;
}
function parseSplitState(text, refresh=true) {
    const data=decodeSplitState(text);if(!data)return;
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
    PARAMS.split_mode.value=data.mode??(bank?4:enabled?(quad?(sequenceUi.active?6:3):shared?2:1):layersUi.active?5:0);
    if(data.mode!==undefined){sequenceUi.active=data.mode===6;layersUi.active=data.mode===5;}
    PARAMS.split_focus.value=focus;
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
    const owner=state.splitEnabled?(state.splitParts[padPart(index)]||{}):{root:Math.round(PARAMS.root_note.value),octave:state.octave,scale:state.scale,mode:state.padMode};
    return isSynthAlgorithm(state.splitEnabled?owner.algorithm:state.algorithm)?{...owner,mode:0}:owner;
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
function currentPerformanceMode(){return Math.round(PARAMS.split_mode.value);}
// Queue until the next host tick: Schwung flushes accumulated jog turns
// before tick, while its click callback arrives immediately.
function queueModeChange(picker,wanted=null) {
    if(state.modeRequest)return;
    state.editorReady=false;
    state.modeRequest={picker,wanted,sent:false,tries:0,next:0,deadline:Date.now()+1500};
    showActionMessage('SELECTING MODE');
}
function toggleSplitMode(){queueModeChange(false);}
function freshModeState() {
    const data=decodeSplitState(getParam('split_state',null));if(!data)return null;
    if(data.mode===undefined){
        const raw=getParam('split_mode',null),mode=raw===null||raw===''?NaN:Number(raw);
        if(!Number.isInteger(mode)||mode<0||mode>6)return null;
        return decodeSplitState({...data,mode});
    }
    return data;
}
function serviceModeChange() {
    const request=state.modeRequest;if(!request)return false;
    if(request.picker&&state.view!=='mode'){state.modeRequest=null;return false;}
    const now=Date.now();if(now<request.next)return false;
    request.next=now+40;
    const fail=message=>{state.modeRequest=null;showMessage(message);showActionMessage(message);};
    let data=freshModeState();
    if(request.wanted===null){
        if(request.picker)request.wanted=clamp(Math.round(state.modeSelection),0,6);
        else if(data){
            const raw=getParam('split_return_mode',null),previous=raw===null||raw===''?NaN:Number(raw);
            if(data.mode>0)request.wanted=0;
            else if(Number.isInteger(previous)&&previous>=1&&previous<=6)request.wanted=previous;
        }
    }
    if(data&&request.wanted!==null&&data.mode!==request.wanted&&!request.sent&&request.tries<2){
        endPerformanceLayer();
        const result=setParamBlocking('split_mode',request.wanted);
        request.sent=result===true||result===1;request.tries++;
        data=freshModeState();
    }
    if(data&&data.mode===request.wanted){
        parseSplitState(data,false);
        state.editorReady=false;
        refreshEditorSnapshot();readLayers();readSequence();
        // A failed editor query retains the coherent mode and disables edits
        // until the existing snapshot recovery obtains fresh parameter values.
        parseSplitState(data,false);
        if(!state.editorReady)uiPolling.editorPending=true;
        if(request.picker){state.view='main';state.pageReturn=null;state.pageDestinationReturn=null;state.fxMenuReturn=false;}
        state.modeRequest=null;state.actionMessageTicks=0;
        showMessage(state.editorReady?MODE_MESSAGES[data.mode]:'UPDATING EDITOR');
        primePadLeds();rebuildLedQueue();return true;
    }
    if(request.sent&&data&&data.mode!==request.wanted){
        const error=String(getParam('last_error','')||'');
        if(error==='Stop recording before changing mode'){fail('STOP RECORD FIRST');return true;}
        if(error==='Wait for capture publication'){fail('WAIT FOR CAPTURE SAVE');return true;}
        if(error==='Wait for Bank sample job'){fail('WAIT FOR BANK SAMPLES');return true;}
    }
    if(now>=request.deadline)fail('MODE NOT CONFIRMED');
    return true;
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
    if(state.view==='synth')return synthPadColor(index);
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
    const menu=['synth','seqGenerate','settings','pages','mode','fxMenu','help','projects'].includes(state.view);
    const upDown=['synth','bankRecovery','projects'].includes(state.view)||customMidiViews.includes(state.view)||['lfoDest','seqGenerate','main','settings','pages','mode','fxMenu','help','library','chain','sliceEdit'].includes(state.view);
    const leftRight=['synth','main','settings','help'].includes(state.view);
    return [
        [MoveMenu,menu?WhiteLedBright:IconLedDim],
        [MoveBack,state.view!=='main'?WhiteLedBright:IconLedDim],
        [MoveUndo,state.view==='synth'?Black:customMidiContext()?(customMidiUi.data?.undo||['midiMapEdit','midiMapPicker'].includes(state.view)?WhiteLedBright:Black):(state.view==='seqGenerate'||state.undoAvailable)&&!state.recording&&!state.clipRecording?WhiteLedBright:Black],
        [MoveShift,state.shiftHeld?WhiteLedBright:IconLedDim],
        [MoveDelete,state.deleteHeld?WhiteLedBright:IconLedDim],
        [MoveUp,upDown?IconLedDim:Black],[MoveDown,upDown?IconLedDim:Black],
        [MoveLeft,leftRight?IconLedDim:Black],[MoveRight,leftRight?IconLedDim:Black],
        [MoveCopy,state.recording||state.clipRecording?Black:currentPerformanceMode()>0?WhiteLedBright:IconLedDim],
        [MoveCapture,state.view==='synth'?(state.focusButtonHeld||!synthUi.data?.ready?WhiteLedBright:IconLedDim):state.focusButtonHeld||state.view==='pages'||(state.splitEnabled&&state.splitFocus)||(layersUi.active&&layersUi.focus)?WhiteLedBright:IconLedDim],
        [MoveMute,state.view==='synth'?Black:state.splitEnabled?(state.splitParts[state.splitFocus]?.mute?WhiteLedBright:IconLedDim):Black],
        [MoveLoop,state.view==='synth'?(synthUi.data?.preview?WhiteLedBright:IconLedDim):state.view==='sliceEdit'?(sliceUi.data?.loop?WhiteLedBright:IconLedDim):!loopAvailable()?Black:state.glitchBufferLocked||state.stackScanPaused?WhiteLedBright:state.recordedStackHold?IconLedMedium:IconLedDim],
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
    // Resolving the current page builds the engine's complete menu layout.
    // It is invariant across this refresh, including all sixteen clip LEDs.
    const chainEditing=chainEditorActive();
    for (let i = 0; i < MoveSteps.length; i++) {
        let color = Black;
        let urgent = false;
        if (chainEditing) {
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
    const categoryLed=!categoryNavigation?-1:state.view==='pages'
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
    const recordColor=state.view==='synth'?(synthUi.data?.ready&&synthUi.data.job===0?BrightRed:WhiteLedDim):state.clipRecording?BrightRed:state.recording?BrightOrange:WhiteLedDim;
    queueLed('cc',MoveRecord,recordColor,1);
    if(MoveRecordAlt!==MoveRecord)queueLed('cc',MoveRecordAlt,recordColor,1);
}

function primePadLeds() {
    for (let index = 0; index < MovePads.length; index++) {
        sendLedNow('note', MovePads[index], padColor(index), true);
    }
    sequenceUi.cursorLedPad=sequenceCursorPad(sequenceUi.data);
    sequenceUi.cursorRestores.clear();sequenceUi.cursorLedRefresh=-Infinity;
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
    sequenceDisplay.valid=false;
    sequenceLaneDisplay.valid=false;
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
    if(tableUi.mask&(1<<lane)){drawTableScope(lane,center,left,width);return;}
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
    const key=windows.flat().join(',')+','+(state.sharedDual?1:0)+':'+tableUi.scopeKey;
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
        if((tableUi.mask&(1<<lane))||!entry||entry.source!==state.waveKeys[lane]||entry.regions.length<=2)continue;
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
        if(selected&&(tableUi.mask&(1<<lane)))drawRect(2,center-4,7,8,1);
        print(3, center - 4, String(lane + 1), selected&&(tableUi.mask&(1<<lane))?0:1);
        const weightHeight = Math.round(state.weights[lane] * 7);
        if (weightHeight > 0) {
            drawRect(9, center + 4 - weightHeight, 2, weightHeight, 1);
        }
        drawWaveformLane(lane,window,center);
        if (!(tableUi.mask&(1<<lane)) && selected && (state.mask&(1<<lane)) && state.regionLane===state.lane && state.regions.length > 2) {
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
        state.fxOrder = state.splitEnabled?decodeFxOrder(code).filter((block)=>[2,4,5,10].includes(block)):decodeFxOrder(code);
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
        {title:isQuad()?'ALL PART SENDS':'PART SENDS',page:isQuad()?QUAD_SEND_PAGE:SPLIT_MIX_PAGE},
        {title:'PRIORITY DUCK',page:SEQUENCE_DUCK_PAGE},
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
        const control=PARAMS[key];if(!control)continue;
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
    if(layersUi.active)return layersPages(engine,pads,enginePages,lists);
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
    if(state.algorithm===15&&['locus','field','speed','join','pad_mode','lane_variation','seq_pos','voice_engine_position'].includes(key))return true;
    if(state.algorithm===16&&(['speed','join','pad_mode','lane_variation','voice_engine_position'].includes(key)||(['locus','field','seq_pos'].includes(key)&&sequenceSoundValue('scanned_excitation',PARAMS.scanned_excitation.value)!==3)))return true;
    if((state.algorithm===17||state.algorithm===18||state.algorithm===19||state.algorithm===20||state.algorithm===21)&&['locus','field','speed','join','pad_mode','lane_variation','seq_pos','voice_engine_position'].includes(key))return true;
    if(key.startsWith('lowform_')){const f=LOWFORM_CONTROLS.find(p=>p.key===key)?.index,v=k=>sequenceSoundValue(k,PARAMS[k].value);
      if([4,5,29].includes(f))return v('lowform_foundation_level')===0;
      if(f>=8&&f<=13)return v('lowform_body_level')===0;
      if(f>=15&&f<=18||f===30||f===31)return v('lowform_texture_type')===0||f!==15&&v('lowform_texture_level')===0||f===17&&![2,4].includes(v('lowform_texture_type'));
      if(f>=20&&f<=25)return v('lowform_filter_type')===0&&!(v('lowform_body')===2&&v('lowform_body_level')>0)||f===25&&v('lowform_filter_env')===0;
      if(f===27)return v('lowform_punch')===0;return false;}
    if(key.startsWith('partials_')){const f=PARTIALS_CONTROLS.find(p=>p.key===key)?.index,v=k=>sequenceSoundValue(k,PARAMS[k].value),t=v('partials_type');if(f>=8&&f<=10)return t!==1||f===10&&v('partials_subs')===0;if(f===4)return t===1;if(f>=16&&f<=23)return t!==2;if(f===13)return v('partials_drift')===0;if(f===14||f===15)return t===2||f===15&&v('partials_damping')===0;if(f>=88)return t!==1;if(f>=40&&f<56)return t===1;return false;}
    if(key.startsWith('reson_')){const f=RESON_CONTROLS.find(p=>p.key===key)?.index,v=k=>sequenceSoundValue(k,PARAMS[k].value);if((f===11||f===12)&&v('reson_mode')<.5||f===12&&v('reson_bend')===0||(f===14||f===15)&&(v('reson_noise')===0||f===14&&v('reson_mode')<.5)||(f===17||f===18)&&v('reson_hit')===0||[25,26,27,28].includes(f)&&v('reson_drive')===0||f===28&&v('reson_sag')===0)return true;return false;}
    if(key.startsWith('noise_')){const f=NOISE_CONTROLS.find(p=>p.key===key)?.index,t=sequenceSoundValue('noise_type',PARAMS.noise_type.value);if(f>=8&&f<=15||f>=24)return t!==0;if(f>=16&&f<=23){if(t===0)return true;if(f===23)return t!==6;if(t===3)return ![16,17,18,20].includes(f);}return false;}
    if(key.startsWith('fm_')){
      const f=FM_CONTROLS.find(x=>x.key===key)?.index,v=k=>sequenceSoundValue(k,PARAMS[k].value),mode=v('fm_mode'),algo=v('fm_algorithm');
      const linked=n=>v('fm_decay'+n)===0&&(v('fm_end'+n)<0?mode>.5:v('fm_end'+n)<100);
      if((f===8||f===9||f>=28)&&algo===0||f>=15&&f<=22&&f!==16&&mode<.5||f===16&&!(linked(1)||algo!==0&&linked(2))||f===18&&v('fm_bend')===0||f===21&&v('fm_noise')===0||f===22&&v('fm_impact')===0)return true;
    }
    if(key.startsWith('scanned_amp_')&&key!=='scanned_amp_mode'&&sequenceSoundValue('scanned_amp_mode',PARAMS.scanned_amp_mode.value)<.5)return true;
    if(key.startsWith('scanned_')){const exc=Math.round(sequenceSoundValue('scanned_excitation',PARAMS.scanned_excitation.value)),profile=Math.round(sequenceSoundValue('scanned_profile',PARAMS.scanned_profile.value));if(key.startsWith('scanned_')&&SCANNED_CONTROLS.find(p=>p.key===key)?.index>16&&SCANNED_CONTROLS.find(p=>p.key===key)?.index<23&&sequenceSoundValue('scanned_art',PARAMS.scanned_art.value)<.5)return true;if(key==='scanned_pick'&&exc>=2&&sequenceSoundValue('scanned_art',PARAMS.scanned_art.value)<.5||['scanned_mass','scanned_tension'].includes(key)&&profile===0||key==='scanned_seed'&&profile!==3&&exc!==2&&!(sequenceSoundValue('scanned_art',PARAMS.scanned_art.value)>.5&&sequenceSoundValue('scanned_impact',PARAMS.scanned_impact.value)>0)||key==='scanned_sweep_time'&&sequenceSoundValue('scanned_bend',PARAMS.scanned_bend.value)===0)return true;}
    if(key.startsWith('table_')){
      if(key==='table_decay'&&sequenceSoundValue('table_art',PARAMS.table_art.value)<.5)return true;
      if(key==='table_pulse'&&Math.round(sequenceSoundValue('table_shape',PARAMS.table_shape.value))!==3)return true;
      if(['table_noise_decay','table_color'].includes(key)&&sequenceSoundValue('table_noise',PARAMS.table_noise.value)<=0)return true;
    }

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
function parameterPages(){const pages=parameterPagesBase();const lfoAt=pages.findIndex(p=>p.title===CHAIN_TOOLS_PAGE.title);pages.splice(lfoAt>=0?lfoAt+1:pages.length,0,...LFO_PAGES);const result=outputMixerPages(sequenceUi.active?sequencePages(pages):pages).map(maskInactivePage);return sequenceSoundActive()?result.filter(sequenceSoundPageAvailable):result;}
function controlForKey(key){const c=sequenceSoundControl(controlForKeyBase(key));if(c&&/^lowform_[abcde]$/.test(key)){const body=Math.round(sequenceSoundValue('lowform_body',PARAMS.lowform_body.value)),label=LOWFORM_BODY_LABELS[clamp(body,0,5)][key.charCodeAt(8)-97];return {...c,label,shortLabel:label};}return c;}
function controlForKeyBase(key) {
    if(state.algorithm===15&&(['locus','field','speed','join','pad_mode','lane_variation','seq_pos','voice_engine_position'].includes(key)||(key?.startsWith('table_')&&inactiveControl(key))))return null;
    if((state.algorithm===16||state.algorithm===17||state.algorithm===18||state.algorithm===19||state.algorithm===20||state.algorithm===21)&&inactiveControl(key))return null;
    if(!key)return null;
    if(PARAMS[key]?.lfoField)return controlLfoForKey(key);
    if(key.startsWith('seq_recipe_'))return recipeControl(key);
    if(/^seq_pan_[1-4]$/.test(key))return {...PARAMS[key],shortLabel:sequenceUi.active?'PAN'+key.slice(-1):state.splitEnabled?'PAN'+'ABCD'[Number(key.slice(-1))-1]:'PAN'};
    if(key==='seq_duck_key')return {...PARAMS[key],max:state.splitEnabled?partCount():1,labels:sequenceUi.active?['OFF','L1','L2','L3','L4']:['OFF','A','B','C','D']};
    if(key==='seq_to_normal'||key==='seq_regenerate')return currentSequenceType()?PARAMS[key]:null;
    if(key==='seq_child')return {...PARAMS.seq_child,max:PARAMS.seq_burst.value};
    if(key==='seq_cycle_hit')return {...PARAMS.seq_cycle_hit,max:PARAMS.seq_cycle_every.value};
    if(key==='seq_note'){const root=sequenceUi.data?.root||60;return {...PARAMS.seq_note,min:Math.max(-48,-root),max:Math.min(48,127-root)};}
    // Only channel-strip EQ controls depend on which strip page is open.
    // Ordinary labels/values must not rebuild every menu for each of 8 cells.
    const stripField=SEQUENCE_STRIP_KEYS.indexOf(key);
    if(stripField>=0&&state.view==='settings'){
        const stripPage=currentSettingsPage();
        if(Number.isInteger(stripPage?.sequenceStrip)){
            const value=(mixerUi.data?.mode===currentPerformanceMode()?mixerUi.data.strips:sequenceUi.data?.strips)?.[stripPage.sequenceStrip]?.[stripField];
            return {...PARAMS[key],value:Number.isFinite(value)?value:PARAMS[key].value};
        }
    }
    if(key==='mix_mute'&&state.view==='settings'){const page=currentSettingsPage();const p=Number.isInteger(page.sequenceStrip)?page.sequenceStrip:state.splitFocus;return {...PARAMS[key],value:mixerUi.data?.strips?.[p]?.[5]||0};}
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
        if (['split_level_a','split_level_b','split_level_c','split_level_d'].includes(key)) return {...control,shortLabel:sequenceUi.active?'LVL'+('abcd'.indexOf(key.slice(-1))+1):state.view==='settings'&&mixerUi.visible?'LVL'+key.slice(-1).toUpperCase():'LEVL',label:sequenceUi.active?'SEQUENCE LANE LEVEL':'PART LEVEL'};
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
function chainEditorActive() {
    if(state.view==='chain')return true;
    if(state.view!=='settings')return false;
    const page=currentSettingsPage();
    return page===CHAIN_PAGE||Boolean(page.chainFade)||Boolean(page.chainTools);
}
function activeControlKeys() {
    if(state.view==='lfoDest')return Array(8).fill(null);
    if(state.view==='settings'){const p=currentSettingsPage();if(p.lfo||p.lfoSetup||p.sequenceLfo)return ['lfo_select',...p.keys.slice(1)];}
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
    if(['fm_end1','fm_end2'].includes(control?.key)&&control.value<0)return 'AUTO';
    if(['fm_decay1','fm_decay2'].includes(control?.key)&&control.value===0)return 'LINK';
    if(/^fm_(attack|decay)[12]$/.test(control?.key))return formatFadeTime(control.value);
    if(control?.lfoField)return controlLfoText(control);
    if(control.key.startsWith('seq_'))return sequenceValue(control);
    if(control?.key?.startsWith('layers_')&&control.format)return control.format(control.value);
    if (!control) return '';
    if(control.key==='bank_pan')return Math.abs(control.value)<.5?'CENTRE':(control.value<0?'L ':'R ')+Math.round(Math.abs(control.value))+'%';
    if(['bank_start','bank_end','bank_level'].includes(control.key))return Math.round(control.value*100)+'%';
    if(['voice_engine_position','voice_engine_stack','voice_engine_pan','voice_engine_depth'].includes(control.key))return (control.value>0&&control.key!=='voice_engine_depth'?'+':'')+Math.round(control.value)+'%';
    if(/^voice_engine_[1-8]$/.test(control.key))return ['MOTN','LANE','GRNS','CUTS','SMPL','SLIC','GLCH','WEAV','WSET','PRSM','MORF','TXTR','FORM','STOC','PLSR','TABLE','SCAN','FM','NOIS','RSN','PART','LOWF'][Math.round(control.value)]||'';
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
    if(control?.key?.startsWith('lowform_')&&!control.labels){const v=Number(control.value);if(['lowform_body_decay','lowform_texture_decay'].includes(control.key)&&v>=11999)return 'HOLD';if(control.unit==='ms')return formatFadeTime(v);if(control.unit==='Hz')return v>=1000?String(Number((v/1000).toFixed(1)))+'k':String(Math.round(v));if(control.unit==='%')return Math.round(v)+'%';return String(Number(v.toFixed(Math.abs(v)<10?2:1)));}

    if(control?.key?.startsWith('partials_')&&!control.labels){const v=Number(control.value);if(control.unit==='ms')return formatFadeTime(v);if(control.unit==='Hz')return v>=1000?String(Number((v/1000).toFixed(1)))+'k':String(Number(v.toFixed(v<1?2:1)));if(control.unit==='%')return Math.round(v)+'%';return String(Number(v.toFixed(Math.abs(v)<10?2:1)));}
    if(control?.key?.startsWith('reson_')&&!control.labels){const v=Number(control.value);if(control.unit==='ms')return formatFadeTime(v);if(control.unit==='Hz')return v>=1000?String(Number((v/1000).toFixed(1)))+'k':String(Math.round(v));if(control.unit==='%')return Math.round(v)+'%';return String(Number(v.toFixed(Math.abs(v)<10?2:1)));}
    if(control?.key?.startsWith('noise_')&&!control.labels){const v=Number(control.value);if(control.unit==='ms')return formatFadeTime(v);if(control.unit==='Hz')return v>=1000?String(Number((v/1000).toFixed(1)))+'k':String(Number(v.toFixed(v<1?2:1)));if(control.unit==='%')return Math.round(v)+'%';return String(Number(v.toFixed(Math.abs(v)<1?2:Math.abs(v)<10?2:1)));}
    if(/^fm_(attack|decay)[12]$/.test(control?.key)&&control.value!==0)return formatFadeTime(control.value);
    if(/^fm_attack[12]$/.test(control?.key)&&control.value===0)return '0ms';
    if(['fm_end1','fm_end2'].includes(control?.key)&&control.value<0)return 'AUTO';
    if(['fm_decay1','fm_decay2'].includes(control?.key)&&control.value===0)return 'LINK';
    if(control?.key?.startsWith('scanned_')&&!control.labels){const v=Number(control.value);if(control.unit==='ms')return Math.round(v)>=1000?String(Number((v/1000).toFixed(2)))+'s':String(Math.round(v))+'ms';if(control.key==='scanned_motion')return v.toFixed(v<1?2:1).replace(/^0\./,'.')+'Hz';return String(Number(v.toFixed(Math.abs(v)<10?1:0)));}
    if(control?.key==='table_type')return ['WAVE','SPLN','WTDR'][Math.round(control.value)]||'';
    if(control?.key?.startsWith('table_')&&!control.labels){
        const v=control.value,a=Math.abs(v);
        if(control.unit==='ms'&&a>=1000)return String(Number((v/1000).toFixed(a<10000?2:1)))+'s';
        if(control.key==='table_cutoff'&&a>=1000)return String(Number((v/1000).toFixed(a<10000?2:1)))+'k';
        return String(Number(v.toFixed(a<1?3:a<10?2:1)));
    }
    if(control?.lfoField)return controlLfoText(control,true);
    control=lfoDisplayControl(control);
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

function drawParameterCell(index, control, compact = false, maxLabelChars = null) {
    // A page slot can resolve to no control when its action is unavailable
    // (for example PRINT/REGEN with the Normal sequencer type).
    if (!control) { drawReservedParameterCell(index, compact); return; }
    control=lfoDisplayControl(control);
    const column = index % 4;
    const row = Math.floor(index / 4);
    const x = column * 32;
    const y = (compact ? 11 : 13) + row * (compact ? 24 : 26);
    const label = String(control.shortLabel || SHORT_LABELS[control.key] || control.label).slice(0, maxLabelChars ?? (['seq_to_normal','seq_regenerate'].includes(control.key)?5:4));
    const value = compactControlValue(control);
    print(x + 3, y, label, 1);
    print(x + ((control.key?.startsWith('reson_')||control.key?.startsWith('partials_')||control.key?.startsWith('lowform_'))&&String(value).length>=5?1:3), y + 11, value, 1);
    if(control.trigLocked){drawRect(x+28,y,3,3,1);if(control.soundScope==='lane')drawRect(x+29,y+1,1,1,0);if(control.trigInherited)drawRect(x+27,y+4,4,1,1);}
    else if(control.modulated||control.automated){drawRect(x+28,y,3,3,1);if(!control.modulated)drawRect(x+29,y+1,1,1,0);}
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
    const controls = [PARAMS.amp_attack, PARAMS.amp_decay, PARAMS.amp_sustain, PARAMS.amp_release].map(lfoDisplayControl);
    const [attack, decay, rawSustain, release] = controls.map(c=>c.value);
    const sustain = clamp(rawSustain / 100, 0, 1);
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
    for (let index = 0; index < controls.length; index++) {
        const control = controls[index];
        const x = index * 32 + 3;
        print(x, 34, SHORT_LABELS[control.key], 1);
        print(x, 45, compactControlValue(control), 1);
        if(control.automated){drawRect(index*32+28,34,3,3,1);drawRect(index*32+29,35,1,1,0);}
        const reservedX = index * 32 + 14;
        drawRect(reservedX, 57, 4, 4, 1);
        drawRect(reservedX + 1, 58, 2, 2, 0);
    }
}

function drawSequenceEnvelopePage() {
 if(sequenceSoundActive())for(let i=0;i<4;++i)if(controlForKey(SEQUENCE_ENVELOPE_PAGE.keys[i])?.trigLocked)drawRect(i*32+28,35,3,3,1);
    const timing=sequenceAmpTiming(true),[attack,decay,release]=timing.applied;
    const x0=4,x1=122,width=x1-x0,baseline=31,peak=19;
    const xAttack=x0+Math.round(width*attack/timing.dur);
    const xDecay=Math.min(x1,xAttack+Math.round(width*decay/timing.dur));
    const xGate=Math.max(xDecay,x1-Math.round(width*release/timing.dur));
    const sustainY=Math.round(baseline-timing.sustain*(baseline-peak));
    const locked=Boolean((sequenceUi.data?.mask||0)&8);
    const duration=PARAMS.seq_duration_mode.value>=.5?sequenceTimeText(timing.dur)
        :sequenceStepText(locked?PARAMS.seq_trig_gate.value:PARAMS.seq_gate.value)+'='+sequenceTimeText(timing.dur);
    print(3,10,'DUR '+duration,1);
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

const enginePicker={open:false,armed:false,selected:0,key:'algorithm'};
function openEnginePicker(delta=0){if(sequenceSoundActive()){showActionMessage('LANE SETTING');return;}enginePicker.open=true;enginePicker.renderKey='';enginePicker.armed=false;enginePicker.key=currentSettingsPage().voiceEngines?'voice_engine_'+Math.round(PARAMS.voice_engine_edit.value):'algorithm';enginePicker.selected=clamp(Math.round(PARAMS[enginePicker.key]?.value??state.algorithm)+delta,0,ALGORITHMS.length-1);draw();}
function handleEnginePicker(status,number,value){
 if(!enginePicker.open){if(status===0xb0&&number===MoveMainButton&&value>0&&state.view==='settings'&&currentSettingsPage().title==='ENGINE'&&!state.shiftHeld&&!state.focusButtonHeld){openEnginePicker();return true;}return false;}
 if(state.view!=='settings'){enginePicker.open=false;return false;}
 if(status!==0xb0)return false;
 if(number===MoveMainKnob||number===MoveUp||number===MoveDown||number===MoveLeft||number===MoveRight){const d=number===MoveMainKnob?decodeDelta(value):value>0?([MoveUp,MoveLeft].includes(number)?-1:1):0;enginePicker.selected=clamp(enginePicker.selected+d,0,ALGORITHMS.length-1);draw();return true;}
 if(number===MoveBack||number===MoveMenu){if(value>0){enginePicker.open=false;draw();}return true;}
 if(number===MoveMainButton){if(value<=0){enginePicker.armed=true;return true;}if(value>0&&enginePicker.armed){enginePicker.armed=false;const key=enginePicker.key,selection=enginePicker.selected;enginePicker.open=false;if(key==='algorithm')selectAlgorithm(selection);else{setParamBlocking(key,selection);refreshEditorSnapshot();}draw();}return true;}
 if(number>=MoveKnob1&&number<MoveKnob1+8)return true;return false;
}
function drawEnginePicker(){const signature=[enginePicker.selected,state.splitFocus,state.activeClip,state.saveOverlayLast].join(':');if(enginePicker.renderKey===signature)return;enginePicker.renderKey=signature;clearDisplay();print(1,0,'PLAYBACK ENGINE',1);drawRect(0,8,128,1,1);const first=clamp(enginePicker.selected-1,0,ALGORITHMS.length-4);for(let row=0;row<4;++row){const i=first+row,y=13+row*10;if(i===enginePicker.selected)drawRect(0,y-1,128,9,1);print(3,y,String(i+1).padStart(2,'0')+' '+ALGORITHMS[i],i===enginePicker.selected?0:1);}print(1,57,'CLICK SET BACK',1);}
function drawSettings() {
    if(enginePicker.open){drawEnginePicker();return;}
    const page = currentSettingsPage();
    if(page.lowform&&lowformUi.startsOpen){drawLowformStarts();return;}
    if(page.reson&&resonUi.startsOpen){drawResonStarts();return;}
    if(page.fm&&fmUi.startsOpen){drawFmStarts();return;}
    if(page===FM_PAGE&&fmUi.routingTouch){drawFmRouting();return;}
    if(page.lfo!==undefined||page.lfoSetup||page.sequenceLfo){drawControlLfo(page);return;}
    lfoUi.visible=false;
    if(page.sequenceMix&&!mixerUi.visible)readMixerState();
    mixerUi.visible=Boolean(page.sequenceMix);
    // SEQ LANE is often left open while a fast sequence runs. Its eight
    // readouts are static between edits, so avoid a full OLED clear and text
    // transfer on every UI tick. The main Sequence view owns the playheads.
    let laneSignature='';
    if((page===SEQUENCE_LANE_PAGE||page.sequenceMix||page.sequenceRecipe)&&state.editorReady){
        laneSignature=JSON.stringify([
            state.settingsPage,page.title,state.algorithm,state.splitFocus,
            state.clipPlaying,state.clipRecording,page.sequenceRecipe?sequenceRecipeStatus():null,
            state.actionMessageTicks>0?state.actionMessage:'',
            ...page.keys.map(key=>{
                if(!key)return null;
                const control=controlForKey(key);
                return control?[control.shortLabel||SHORT_LABELS[key]||control.label,
                    compactControlValue(control),Boolean(lfoDisplayControl(control).automated),Boolean(lfoDisplayControl(control).modulated),Boolean(control.trigLocked),Boolean(control.trigInherited),control.soundScope]:null;
            }),
        ]);
        if(sequenceLaneDisplay.valid&&sequenceLaneDisplay.signature===laneSignature)return;
    }
    clearDisplay();
    if(!state.editorReady){print(1,0,'UPDATING EDITOR',1);return;}
    if(page.sequenceRecipe){drawSequenceRecipePage();sequenceLaneDisplay.valid=true;sequenceLaneDisplay.signature=laneSignature;return;}
    if(page.sequenceGenerator){drawSettingsTitle('SEQ GENERATE');print(1,23,state.actionMessageTicks>0?state.actionMessage:'CLICK TO OPEN',1);return;}
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
    const pageTitle = (sequenceSoundActive()?'T'+String(sequenceSoundUi.data.step+1).padStart(2,'0')+' ':'') + target + lockTarget + scope + engineTitle + layerTitle+page.title + (page.sequenceEnvelope?' T'+String((sequenceUi.data?.step??0)+1).padStart(2,'0'):takeSuffix);
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
    const hasFooter = page.title==='ENGINE' || sequenceSoundActive() || page.sliceEditor || page.pulsar || page.stochastic || page.voiceEngines || page.voiceSetup || page.table || page.scanned || page.fm || page.noise || page.reson || page.partials || page.lowform || drawPath || (state.splitEnabled && (isSendPage(page)
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
    if(sequenceSoundActive()){const scopes=new Set(page.keys.map(k=>controlForKey(k)?.soundScope).filter(Boolean));print(1,57,scopes.size===1?(scopes.has('lane')?'LANE LOCKS':'VOICE LOCKS'):'HOLD TRIG  X:DIAL CLR',1);}
    else if(page.pulsar)print(1,57,pulsarDurationFooter(),1);
    else if(page.stochastic)print(1,57,PARAMS.stochastic_source_hold.value>=.5?'SRC HELD CLICK: RUN':'CLICK: HOLD SOURCE',1);
    else if(page.title==='ENGINE')print(1,57,'CLICK: CHOOSE ENGINE',1);
    else if(page.lowform)print(1,57,'L'+((lowformUi.data?.edit??state.lane-1)+1)+' CLICK: STARTS',1);
    else if(page.reson)print(1,57,'L'+((resonUi.data?.edit??state.lane-1)+1)+' CLICK: STARTS',1);
    else if(page.partials)print(1,57,page.partialsBands?'TURN MAIN: GROUP':'PATCH L'+((partialsUi.data?.edit??state.lane-1)+1),1);
    else if(page.noise)print(1,57,page.noiseBands?'TURN MAIN: BAND GROUP':'PATCH L'+((noiseUi.data?.edit??state.lane-1)+1)+' TRACK:SELECT',1);
    else if(page.fm)print(1,57,'L'+((fmUi.data?.edit??state.lane-1)+1)+' CLICK: STARTS',1);
    else if(page.table||page.scanned)print(1,57,'PATCH L'+(((page.fm?fmUi:page.scanned?scannedUi:tableUi).data?.edit??state.lane-1)+1)+' TRACK:SELECT',1);
    else if(page.voiceEngines||page.voiceSetup)print(1,57,'TURN SLOT CLICK EDIT',1);
    else if(drawPath)print(1,57,'CLICK: EDIT PATH',1);
    else if(page.sliceEditor)print(1,57,'CLICK: EDIT SLICES',1);
    else if (state.splitEnabled && isSendPage(page)) print(1,57,splitSendFooter(),1);
    else if (state.splitEnabled && !isQuad() && (page===DELAY_RETURN_PAGE || page.title===REVERB_PAGE.title))
        print(1,57,'RETURN: FX SENDS',1);
    if(page===SEQUENCE_LANE_PAGE||page.sequenceMix){
        sequenceLaneDisplay.signature=laneSignature;
        sequenceLaneDisplay.valid=true;
    }
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
        drawTitleWithFraction('SYSTEM REPORT <>','1/8');
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
        drawTitleWithFraction('CALLBACK WORK <>','2/8');
        print(1, 13, helpReportRow('CONTROL PEAK', compactReportCount(state.controlPeakMicros) + ' us'), 1);
        print(1, 25, helpReportRow('QUERY PEAK', compactReportCount(state.queryPeakMicros) + ' us'), 1);
        print(1, 37, helpReportRow('WORK PEAK', compactReportCount(state.workPeakMicros) + ' us'), 1);
        print(1, 49, helpReportRow('WORK MISSES', compactReportCount(state.workOverruns)), 1);
        return;
    }
    if(state.helpPage===2) {
        drawTitleWithFraction('RENDER TIMING <>','3/8');
        const cpu=state.renderCpuAvailable;
        print(1,11,helpReportRow('CPU PEAK',cpu?compactReportCount(state.renderCpuPeakMicros)+' us':'N/A'),1);
        print(1,23,helpReportRow('WALL PEAK',compactReportCount(state.maximumRenderMicros)+' us'),1);
        print(1,35,helpReportRow('WAIT PEAK',cpu?compactReportCount(state.renderWaitPeakMicros)+' us':'N/A'),1);
        print(1,47,helpReportRow('CPU MISSES',cpu?compactReportCount(state.renderCpuOverruns):'N/A'),1);
        print(1,57,'SHIFT+DEL RESET',1);return;
    }
    if(state.helpPage===5){
        drawTitleWithFraction('UI TIMING <>','6/8');
        print(1,11,helpReportRow('FRAME GAP',Math.round(uiTiming.gap)+' ms'),1);
        print(1,21,helpReportRow('QUERY WAIT',Math.round(uiTiming.query)+' ms'),1);
        print(1,31,helpReportRow('GAPS >50MS',compactReportCount(uiTiming.slow)),1);
        print(1,41,helpReportRow('STEP JUMPS',compactReportCount(uiTiming.jumps)),1);
        print(1,51,String(uiTiming.key||'NO SLOW QUERY').slice(0,21),1);return;
    }
    if(state.helpPage===7){
        const d=state.patternWorker||[-1,0,0,0,0,0,0,0,99,0,0];
        drawTitleWithFraction('FADE AUDIO <>','8/8');
        print(1,11,helpReportRow('WORKER CPU',d[1]?compactReportCount(d[3])+' us':'N/A'),1);
        print(1,21,helpReportRow('WALL/WAKE',(d[2]/1000).toFixed(1)+'/'+(d[10]/1000).toFixed(1)+'ms'),1);
        print(1,31,helpReportRow('START/RUN',compactReportCount(d[4])+'/'+compactReportCount(d[5])),1);
        print(1,41,helpReportRow('OUT MISSING',d[6].toFixed(1)+' ms'),1);
        print(1,51,helpReportRow('FLUSH/Q',(d[0]<0?'N/A':d[0]?'ON':'OFF')+' '+Math.round(d[7])+'/'+Math.round(d[9])+'ms '+(d[8]===99?'?':d[8])),1);return;
    }
    if(state.helpPage===6){
        const d=state.renderProfile||Array(8).fill(0);
        drawTitleWithFraction(d[0]?'DSP CPU MISS <>':'DSP PEAK <>','7/8');
        ['JOBS','PREP/SEQ','VOICES','PART MIX','FX','REC/OUT'].forEach((label,i)=>
            print(1,10+i*9,helpReportRow(label,compactReportCount(d[i+2])+' us'),1));return;
    }
    if(state.helpPage===4){
        const d=remoteE16.diagnostics;
        drawTitleWithFraction('E16 LINK <>','5/8');
        print(1,11,helpReportRow('STATE',d.state),1);
        print(1,21,helpReportRow('LAST',d.issue),1);
        print(1,31,helpReportRow('ACK/NACK',compactReportCount(d.acks)+'/'+compactReportCount(d.nacks)),1);
        print(1,41,helpReportRow('RETRY/RECON',compactReportCount(d.retries)+'/'+compactReportCount(d.reconnects)),1);
        print(1,51,helpReportRow('PENDING PX',d.pixels+'/'+d.pixelWindow),1);return;
    }
    drawTitleWithFraction('SHORTCUTS <>','4/8');
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
    result.push({title:'PROCESS',view:'synth'});
    return result;
}

const PAGE_CATEGORY_NAMES=['PLAY','SAMPLE','MIX / FX','PERFORMANCE'];
const SAMPLE_PAGE_TITLES=new Set(['MIX / REC','SAMPLE EDIT']);
const MIX_PAGE_TITLES=new Set(['FX SENDS','PRIORITY DUCK','PART COLOR',
    'EFFECTS','STEREO / ROTARY','DELAY RETURN','TAPE','REVERB','FX ORDER']);
const PERF_PAGE_TITLES=new Set(['LFO','LFO SETUP','PERF LOOP','PATTERN FADE','OVERDUB TAKES',
    'CHAIN','CHAIN FADE','ENTRY FADE','CHAIN TOOLS']);

function pageCategoryForPage(page) {
    if(page?.sequenceMix)return 2;
    if(page?.sequence)return 0;
    if(page?.layerFx)return 2;
    if(page?.layersSetup)return 2;
    if(!page||isMidiPage(page)||isSetupPage(page))return -1;
    if(SAMPLE_PAGE_TITLES.has(page.title))return 1;
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
    if(category===1)result.push({title:'PROCESS',view:'synth',category});
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
        ...PAGE_CATEGORY_NAMES.map((title,category)=>({title,category,hub:'category'})),
        {title:'MODE',view:'mode',hub:'mode'},
        {...setupPageEntry(),title:sequenceUi.active?'SEQ SETUP':'SETUP',hub:'setup'},
        {...midiPageEntry(),title:'MIDI',hub:'midi'},
        {title:'ALL',hub:'all'},
        {title:'PROJECTS',view:'projects',hub:'projects'},
    ];
}

function currentPageCategory() {
    if(state.view==='settings'||state.view==='seqGenerate')return pageCategoryForPage(currentSettingsPage());
    if(state.view==='library'||state.view==='synth')return 1;
    if(state.view==='mixStatus'||state.view==='fxMenu')return 2;
    if(state.view==='clips'||state.view==='chain')return 3;
    return -1;
}

function currentEntryIndex(entries) {
    if(state.view==='settings'||state.view==='seqGenerate')return entries.findIndex(entry=>
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
    if(entry.view==='settings'&&parameterPages()[entry.index]?.sequenceGenerator){state.pageDestinationReturn=destinationReturn;openSequenceGenerator(parameterPages()[entry.index].generatorPage);return;}
    if(entry.view==='mode'){
        state.pageDestinationReturn=destinationReturn;
        state.modeSelection=currentPerformanceMode();state.view='mode';return;
    }
    if((state.pageReturn?.view==='library'||state.view==='library')&&entry.view!=='library')
        setParamBlocking('preview_stop',1);
    if(entry.view==='synth')openSynth();
    else if(entry.view==='projects')openProjects();
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

function applyModePicker(){queueModeChange(true);}

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
    const heading=synthUi.browserTarget?'PICK '+(synthUi.browserTarget.startsWith('hybrid')?'SOURCE '+String.fromCharCode(65+Number(synthUi.browserTarget.slice(6))):synthUi.browserTarget.toUpperCase()):'L'+state.libraryLane+' '+PARAMS.preview_route.labels[Math.round(PARAMS.preview_route.value)]+(state.splitEnabled&&state.sharedDual&&PARAMS.preview_route.value===0?' '+partName():'')+' '+leaf;
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
function openExitConfirmation() {
    state.exitConfirmation={selection:0,sent:false};resetParameterEncoderMotion();
}
function drawExitConfirmation() {
    if(state.exitConfirmation.painted===state.exitConfirmation.selection)return;
    state.exitConfirmation.painted=state.exitConfirmation.selection;
    clearDisplay();print(1,0,'EXIT s3g Thru?',1);
    print(1,14,'EXIT STOPS AUDIO',1);
    ['KEEP PLAYING','EXIT TO SCHWUNG'].forEach((text,index)=>{
        const y=31+index*11,selected=state.exitConfirmation.selection===index;
        if(selected)drawRect(0,y-1,128,9,1);print(1,y,text,selected?0:1);
    });
    print(1,57,'TURN/CLICK  BACK:KEEP',1);
}
function handleExitConfirmation(status,number,value) {
    const request=state.exitConfirmation;if(!request||status!==0xb0)return false;
    const navigation=[MoveMainKnob,MoveMainButton,MoveUp,MoveDown,MoveLeft,MoveRight,MoveBack,MoveMenu];
    if(!navigation.includes(number)){
        if(value>0){state.exitConfirmation=null;resetParameterEncoderMotion();}
        return false;
    }
    if(ICON_BUTTONS.includes(number)){
        if(value>0)state.iconPressed.add(number);else state.iconPressed.delete(number);
        rebuildIconLedQueue();
    }
    if(request.sent)return true;
    if(number===MoveMainKnob){const delta=decodeDelta(value);request.selection=clamp(request.selection+Math.sign(delta),0,1);}
    else if(value>0){
        if(number===MoveBack||number===MoveMenu){state.exitConfirmation=null;resetParameterEncoderMotion();}
        else if(number===MoveUp||number===MoveLeft)request.selection=0;
        else if(number===MoveDown||number===MoveRight)request.selection=1;
        else if(number===MoveMainButton){
            if(!request.selection){state.exitConfirmation=null;resetParameterEncoderMotion();}
            else{
                if(state.bankMode){
                    readBankState();
                    if(openBankRecovery()){state.exitConfirmation=null;return true;}
                    if(state.bank?.pending>=0){state.exitConfirmation=null;showMessage('WAIT FOR BANK SAVE');return true;}
                }
                request.sent=true;
                if(typeof host_exit_module==='function')host_exit_module();
            }
        }
    }
    return true;
}

function draw() {
    if(state.exitConfirmation){drawExitConfirmation();return;}
    if(state.view!=='settings'&&state.view!=='lfoDest')lfoUi.visible=false;
    if(state.view!=='settings'||state.performanceVisible){sequenceLaneDisplay.valid=false;mixerUi.visible=false;}
    const overlay=state.saveStatus===2?'saving':state.savedTicks>0&&state.saveStatus===3?'saved':'';
    if(overlay!==state.saveOverlayLast){splitDisplay.footer=null;layersDisplay.fx=null;sequenceDisplay.valid=false;sequenceLaneDisplay.valid=false;state.saveOverlayLast=overlay;}
    if(state.performanceVisible)drawPerformanceLayer();
    else if(state.view==='lfoDest')drawLfoDestPicker();
    else if(customMidiViews.includes(state.view))drawCustomMidi();
    else if(state.view==='layerFxPicker')drawLayersPicker();
    else if(state.view==='synth')drawSynth();
    else if(state.view==='projects')drawProjects();
    else if(state.view==='bankRecovery')drawBankRecovery();
    else if(state.view==='seqGenerate')drawSequenceGenerator();
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
    if(state.view==='synth'||state.view==='projects'||(state.view==='seqGenerate'&&state.saveStatus!==4))return; // Dedicated panels own their available space.
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
    if(isSynthAlgorithm(state.algorithm)){if(state.algorithm===21)readLowform(false);else if(state.algorithm===20)readPartials(false);else if(state.algorithm===19)readReson(false);else if(state.algorithm===18)readNoise(false);else if(state.algorithm===17)readFm(false);else if(state.algorithm===16)readScanned(false);else readTable(false);readTableScope(false);}
    readSequence();showMessage(sequenceUi.active?'EDIT LANE '+lane:isSynthAlgorithm(state.algorithm)?'EDIT PATCH '+state.lane:'RECORD LANE ' + state.lane);
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
        items.push({name:'Thru Process',directory:true,path:SYNTH_DIRECTORY});
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
    if (path !== LIBRARY_ROOT && path !== state.capturesDirectory && path !== SYNTH_DIRECTORY) {
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
        || state.libraryDir === state.capturesDirectory || state.libraryDir === SYNTH_DIRECTORY) {
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
    if(synthUi.browserTarget){const which=synthUi.browserTarget;setParamBlocking('synth_'+which+'_file',path);returnSynthBrowser();synthUi.notice=String(getParam('last_error',''));return;}
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
        const error=String(getParam('last_error','')||'');
        if(error){showMessage(error.slice(0,21));return;}
        if(clip===state.activeClip){
            parseClipState(getParam('clip_state',''));
            if(sequenceUi.active){
                sequenceUi.pad=-1;sequenceUi.held=false;
                if(state.view==='seqGenerate')closeSequenceGenerator(false,false);
                else readSequence();
            }
        }
        showMessage('CLEARED CLIP ' + clipLabel(clip));
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

function clearPerformanceRecording(){
    if(state.recording||state.clipRecording){showActionMessage('STOP RECORDING FIRST');return;}
    setParamBlocking('clip_clear_recording',1);
    const error=String(getParam('last_error','')||'');
    if(error){showActionMessage('CLEAR REC NOT READY');return;}
    parseClipState(getParam('clip_state',''));
    parseClipTakeState(getParam('clip_take_state','0,0,0'));
    parseClipCapacityState(getParam('clip_capacity_state',''));
    parseClipBankState(getParam('clip_bank_state',''));
    readSequence();readLayers();rebuildLedQueue();
    showActionMessage('RECORDING CLEARED');
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
    if(currentSettingsPage().sequenceGenerator){openSequenceGenerator(currentSettingsPage().generatorPage);return;}
    if(previous?.sequenceGenerator&&state.view==='seqGenerate'){
        setParamBlocking('seq_gen_cancel',1);sequenceGeneratorUi.data=null;sequenceGeneratorUi.raw='';
        state.view='settings';readSequence();rebuildLedQueue();
    }
    if(currentSettingsPage().pathEditor)readStackPath();
    if(currentSettingsPage().patternFade)readPatternFade();
}

function applyMomentaryControl(control) {
    control.value = 0;
    if(control.key==='clip_clear_recording'){clearPerformanceRecording();return;}
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
    if(control.lfoField==='rate')return control.lfoClock===0?1/LFO_BEATS[clamp(lfoBeatIndex(control.value)-delta,0,LFO_BEATS.length-1)]:Number((control.min*Math.pow(control.max/control.min,clamp(Math.log(control.value/control.min)/Math.log(control.max/control.min)*160+delta,0,160)/160)).toPrecision(6));
    if(control.curve==='log'&&!fine)return logarithmicControlAt(control,logarithmicControlIndex(control)+delta);
    if(control.lfoField==='smooth')return logarithmicControlAt(control,logarithmicControlIndex(control)+delta);
    if(control.key==='seq_recipe_probability') {
        const divisions=Math.round(1/(fine?control.step*.1:control.step));
        const index=control.value<0?0:1+Math.round(control.value*divisions);
        const next=clamp(index+delta,0,divisions+1);
        return next===0?-1:(next-1)/divisions;
    }
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
    } else if (/^lowform_(body_decay|texture_decay|texture_attack|punch_time|filter_decay|cutoff)$/.test(control.key)||/^reson_(decay|sweep|noise_decay|hit_decay|recovery)$/.test(control.key)||/^fm_(attack|decay)[12]$/.test(control.key)||control.key === 'amp_attack'
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
    if(adjustSequenceSound(controlForKey(key),index,delta))return;
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
    if(state.view==='settings'){const p=currentSettingsPage();if(p.lfo!==undefined||p.lfoSetup||p.sequenceLfo){adjustControlLfo(index,delta);return;}}
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
    if(adjustSequenceSound(control,index,delta))return;
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
    if(control.key.startsWith('seq_')&&control.momentary){if(['seq_to_normal','seq_regenerate'].includes(control.key)&&delta<0)return;setParamBlocking(control.key,control.key==='seq_clear'?4:1);readSequence();if(control.key==='seq_to_normal'){state.view='main';state.settingsPage=0;resetParameterEncoderMotion();}return;}
    if(control.key==='clip_clear_recording'&&delta<=0)return;
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
            readSequence();readMixerState();return;
        }
    }
    if(control.key==='split_mode'){queueModeChange(false,value);return;}
    if(control.key==='pattern_fade_lookahead'){
        // Configuration is held fixed during a fade. Show the accepted value,
        // rather than leaving an optimistic 12ms label after a rejected edit.
        setParamBlocking(control.key,String(value));
        control.value=PARAMS[control.key].value=clamp(getNumber(control.key,control.value),0,1);
        if(control.value!==value)showActionMessage('FINISH FADE FIRST');
        else showMessage('LOOK '+formatControlValue(control),35);
        return;
    }
    control.value = value;
    PARAMS[control.key].value = value;
    if(control.key.startsWith('seq_')){
        // TYPE inserts/removes pages as soon as its displayed value changes.
        // Use the page resolved before that change, not the shifted index.
        const title=stripPage?.title;
        setParamBlocking(control.key,value);readSequence();
        if(control.key==='seq_type'&&title){
            state.settingsPage=Math.max(0,parameterPages().findIndex(page=>page.title===title));
            resetParameterEncoderMotion();
        }
        if(control.key.startsWith('seq_pan_')||control.key.startsWith('seq_duck_'))readMixerState();
        return;
    }
    if(control.key.startsWith('partials_')){setParamBlocking(control.key,String(value));readPartials(false);return;}
    if(control.key.startsWith('lowform_')){setParamBlocking(control.key,String(value));readLowform(false);return;}
    if(control.key.startsWith('reson_')){setParamBlocking(control.key,String(value));readReson(false);return;}
    if(control.key.startsWith('noise_')){setParamBlocking(control.key,String(value));readNoise(false);return;}
    if(control.key.startsWith('fm_')){setParamBlocking(control.key,String(value));readFm(false);return;}
    if(control.key.startsWith('scanned_')){setParamBlocking(control.key,String(value));readScanned(false);return;}
    if(control.key.startsWith('table_')){
        const title=stripPage?.title||currentSettingsPage().title;
        setParamBlocking(control.key,String(value));readTable(false);
        const index=parameterPages().findIndex(p=>p.title===title);state.settingsPage=index>=0?index:parameterPages().findIndex(p=>p.title==='TABLE');
        resetParameterEncoderMotion();return;
    }
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
    const profile=String(getParam('render_profile_state','0,0,0,0,0,0,0,0')).split(',').map(Number);
    if(profile.length===8&&profile.every(Number.isFinite))state.renderProfile=profile;
    const worker=String(getParam('pattern_worker_state','-1,0,0,0,0,0,0,0,99,0,0')).split(',').map(Number);
    if(worker.length===11&&worker.every(Number.isFinite))state.patternWorker=worker;
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
    parseSplitState(getParam('split_state','{}'));readTable(uiPolling.cached);readScanned(uiPolling.cached);readFm(uiPolling.cached);readNoise(uiPolling.cached);readReson(uiPolling.cached);readPartials(uiPolling.cached);readLowform(uiPolling.cached);readTableScope(uiPolling.cached);readControlLfos(true);readControlLfoLive(uiPolling.cached);if(PARAMS.split_mode.value===6||sequenceUi.active)readSequence(true);
    parseVoiceEngineState(getParam('voice_engine_state',''));
    state.voiceAlgorithms=String(getParam('voice_algorithms','')).split(',').map(Number);
    parseSaveStatus(getParam('save_status',''));
    parseMixStatus(getParam('mix_status',''));
    if(mixerUi.visible||uiPolling.cached)readMixerState();
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
    resetUiTiming();renderProfileRequested=false;
    uiPolling.supported=null;uiPolling.cached=false;uiPolling.cache.clear();uiPolling.lastFrame=-Infinity;uiPolling.job=0;uiPolling.editorPending=false;
    sequenceUi.active=false;sequenceUi.data=null;sequenceUi.pad=-1;sequenceUi.held=false;sequenceUi.copy=false;sequenceUi.copyReady=false;sequenceUi.fullRaw='';sequenceUi.revision=sequenceUi.cursorRevision=sequenceUi.headRevision=0;sequenceUi.cursorLedPad=-1;sequenceUi.cursorLedRefresh=-Infinity;sequenceUi.cursorRestores.clear();sequenceDisplay.valid=false;sequenceLaneDisplay.valid=false;
    splitDisplay.valid=false;state.waveText=null;state.patternFaderTouched=false;
    // Schwung may clear or restore the physical surface between takeover sessions.
    if (typeof moveInputFilter.invalidateLedCache === 'function') {
        moveInputFilter.invalidateLedCache();
    }
    state.ticks = 0;state.modeRequest=null;state.exitConfirmation=null;lfoUi.data=null;lfoUi.live=null;lfoUi.picker=null;lfoUi.visible=false;syncLfoPageKeys();
    resetCustomMidiUi();
    resetMidiSurface();
    setParam('slice_editor_open',0);sliceUi.data=null;sliceUi.wave=null;
    state.view = 'main';
    state.settingsPage = 0;
    sequenceGeneratorUi.data=null;sequenceGeneratorUi.raw='';sequenceGeneratorUi.page=0;mixerUi.data=null;mixerUi.raw='';mixerUi.visible=false;
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
    readLayers();readMixerState();
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
    // The input picker is part of Process; keep its native worker/session alive.
    if(synthUi.opened&&!synthContext()){
        synthCommand('close');synthUi.opened=false;synthUi.copy=false;synthUi.browserTarget='';
    }
    if(state.modeRequest){if(serviceModeChange())draw();return;}
    // Schwung can call overtake tick at 500 Hz. Count display frames using
    // elapsed time; never couple a MIDI/LED cursor to a long serial query sweep.
    // Check cadence BEFORE any page predicates. In particular,
    // customMidiContext() resolves the menu and must not run at host tick rate.
    if((state.clipPlaying&&uiPolling.supported!==false)||sequenceUi.active||mixerUi.visible||lfoUi.visible||Boolean(lfoUi.live?.v?.length)||synthContext()){
        const now=Date.now();
        if(now>=uiPolling.lastFrame&&now-uiPolling.lastFrame<16)return;
        uiPolling.lastFrame=now;
    }
    const sequencePolling=(state.clipPlaying||sequenceUi.active||lfoUi.visible||Boolean(lfoUi.live?.v?.length))&&uiPolling.supported!==false
        &&!['projects','sliceEdit','bankRecovery'].includes(state.view)&&!customMidiContext();
    if(sequencePolling){
        const ok=fetchUiPoll('fast');
        if(uiPolling.supported===null)uiPolling.supported=ok;
        if(!ok&&uiPolling.supported)return; // Retain the last complete frame on a transient failure.
    }
    if(sequenceUi.active)noteUiFrame();else uiTiming.last=null;
    const profileWanted=state.view==='help';
    if(profileWanted!==renderProfileRequested){const ok=setParamBlocking('render_profile_enable',profileWanted?1:0);if(ok!==false&&ok!==0)renderProfileRequested=profileWanted;}
    state.ticks++;
    tickLayersCornerGesture();tickSequenceGesture();
    if(!sequenceUi.active&&state.focusButtonHeld && state.performanceArmed && !state.shiftHeld && !state.focusButtonUsed
        && state.ticks-state.captureStartTick>=10) {
        state.focusButtonUsed=true;state.performanceVisible=true;rebuildLedQueue();
    }
    if(sequencePolling&&uiPolling.supported){
        // At most two synchronous query transactions per display frame.
        // One fast coherent cursor snapshot; one slow job OR MIDI feedback.
        const recoveringEditor=uiPolling.editorPending;
        if(recoveringEditor)refreshEditorSnapshot(false);
        else if(state.view==='synth'&&state.ticks%6===1)readSynth();
        else if(state.view==='seqGenerate'&&state.ticks%6===1)readSequenceGenerator();
        else if(state.ticks%3!==0)fetchUiPoll(UI_POLL_JOBS[uiPolling.job++%UI_POLL_JOBS.length]);
        withUiPollCache(()=>{
            if(state.ticks%3===0){pollDsp();pollMidiControls();}
            readPlaybackReadout(true);readControlLfoLive(true);readSequenceCursor();
        });
        if(state.ticks%3===0&&!recoveringEditor)tickMidiFeedback();
    }else{
        if (state.ticks % 3 === 0) {pollDsp();pollMidiControls();if(state.view==='projects'||projectUi.pending)readProject();}
        if(sequenceUi.active)readSequenceCursor();
        if(state.view==='synth'&&state.ticks%6===1)readSynth();
        if(state.view==='seqGenerate'&&state.ticks%6===1)readSequenceGenerator();
        tickMidiFeedback();
    }
    if (state.ticks % 180 === 0) setParam('maintenance', 1);
    if(state.savedTicks>0)state.savedTicks--;
    if (state.messageTicks > 0) state.messageTicks--;
    if (state.actionMessageTicks > 0) state.actionMessageTicks--;
    rebuildIconLedQueue();
    drainLedQueue();
    if(sequencePolling&&uiPolling.supported)withUiPollCache(draw);else draw();
};

globalThis.onMidiMessageInternal = function(data) {
    if (!data || data.length < 3) return;
    const status = data[0] & 0xF0;
    const number = data[1] | 0;
    const value = data[2] | 0;
    if(handleExitConfirmation(status,number,value))return;
    if(handleLfoPickerMidi(status,number,value))return;
    if(handleSynthMidi(status,number,value))return;
    if(handleEnginePicker(status,number,value))return;
    if(handleLowformMidi(status,number,value))return;
    if(handleResonMidi(status,number,value))return;
    if(handleFmMidi(status,number,value))return;

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
    if(handleSequenceGeneratorCc(number,value))return;
    if(handleProjects(number,value))return;
    if(handleBankRecovery(number,value))return;
    if(handleCustomMidiCcUi(number,value))return;
    if(handleSliceEditorCc(number,value))return;
    if(number===MoveUndo&&value>0){triggerSafetyUndo();readSliceEditor();return;}
    if(handleLayersPicker(number,value))return;
    if(number===MoveMainButton&&value>0&&sequenceUi.active&&(state.view==='main'||(state.view==='settings'&&currentSettingsPage().sequence))){
        if(state.view==='settings'&&currentSettingsPage().sequenceGenerator){openSequenceGenerator(currentSettingsPage().generatorPage);return;}
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
            if(state.deleteHeld&&!state.shiftHeld){state.focusButtonUsed=true;clearPerformanceRecording();return;}
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
    if(number===MoveMute&&value>0){
        parseSplitState(getParam('split_state','{}'));
        const muted=state.splitEnabled?!(state.splitParts[state.splitFocus]?.mute):!(mixerUi.data?.strips?.[0]?.[5]);
        setParamBlocking('part_mute',muted?1:0);readMixerState();parseSplitState(getParam('split_state','{}'));showMessage(muted?'PART MUTED':'PART UNMUTED');return;
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
        if(value>0){resetUiTiming();setParamBlocking('performance_reset',1);pollDiagnostics();draw();}
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
        if(state.view==='mode'){if(!state.modeRequest||state.modeRequest.wanted===null)state.modeSelection=clamp(state.modeSelection-1,0,6);}
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
        if(state.view==='mode'){if(!state.modeRequest||state.modeRequest.wanted===null)state.modeSelection=clamp(state.modeSelection+1,0,6);}
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
        if(state.view==='mode'){if(!state.modeRequest||state.modeRequest.wanted===null)state.modeSelection=clamp(state.modeSelection+delta,0,6);}
        else if(state.view==='pages')movePagePicker(delta);
        else if (state.view === 'settings'&&currentSettingsPage().partialsBands){setParamBlocking('partials_band_group',clamp(partialsUi.group+delta,0,7));readPartials(false);draw();}
        else if (state.view === 'settings'&&currentSettingsPage().noiseBands){setParamBlocking('noise_band_group',clamp(noiseUi.group+delta,0,7));readNoise(false);draw();}
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
        if(sequenceSoundActive()&&!state.shiftHeld&&!state.focusButtonHeld&&state.view!=='pages'){openPagePicker();return;}
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
            openExitConfirmation();
        }
    }
};

globalThis.onUnload = function() {
    state.modeRequest=null;state.exitConfirmation=null;
    setParamBlocking('render_profile_enable',0);renderProfileRequested=false;
    setParamBlocking('midi_custom_cancel',1);
    setParamBlocking('slice_editor_open',0);
    if(synthContext()){rememberSynthEditor();setParamBlocking('synth_close',1);}
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




// Source creation is a transient tool, separate from performance automation.
const synthUi={data:null,raw:'',page:0,overlay:'',selection:0,row:0,point:0,
    amount:.25,low:-12,high:12,keepEnds:true,copy:false,clipboard:null,
    name:'',nameBefore:'',cursor:0,lower:false,numbers:false,notice:'',opened:false,groupMemory:[0,5,4],browserTarget:'',editorRaw:''};
const SYNTH_FIELDS=['LEN','ROOT','WID','ATK','REL','LVL','PTS','EVOL','DRFT','DRT','CURV','DYN','LOOP'];
const SYNTH_ROWS=['PITCH','POINTS','EVOLUTION','LEVEL'];
const SYNTH_LIMITS=[24,16,4,24];
const SYNTH_PAGES=['PROCESS SETUP','SPLINE','CURVES','RANDOM','PREVIEW'];
const SYNTH_ACTIONS=['RENDER','PREVIEW','NAME','SAVE','SAVE & LOAD','OPEN SOURCE RECIPE',
    'NEW SEED','MUTATE SOUND','RANDOM CURVE','PROTECT PARAMS','CANCEL RENDER','CLEAN'];
const SYNTH_DIRECTORY=LIBRARY_ROOT+'/s3g Thru/Synths';
const SYNTH_TYPES=['SPLN','WAVE','BLOOM','RSYN','PART','NOISE','FM','CONV','DRUM','PLUCK','SUB','HYBR','DUST','WTDR','FMDR'];
// Presentation order only: saved recipe IDs and per-type workspaces stay fixed.
const SYNTH_TYPE_ORDER=[3,11,7];
const SYNTH_FAMILIES=['','wave','bloom','resynth','additive','noise','fm','conv','drum','pluck','sub','hybrid','dust','wt','drum'];
const SYNTH_TITLES=['SPLINE','WAVE','BLOOM','RESYNTH','PARTIALS','NOISE BANK','FM','CONVOLVE','DRUM','PLUCK','SUBHARMONIC','HYBRID','DUST','WT DRUM','FM DRUM'];
const SYNTH_ADDITIVE_SPECS=[['PART',1,32,.1],['TILT',-24,12,.5,'db'],['STRC',.25,4,.01,'ratio'],['DETN',0,30,.1,'cent'],['PHAS',0,1,.01,'pct'],['DRFT',0,1,.01,'pct'],['RATE',.01,20,0,'hz'],['DEC',0,1,.01,'pct']];
const SYNTH_NOISE_SPECS=[['BAND',1,32,.1],['BW',.1,48,.1,'st'],['TILT',-24,12,.5,'db'],['STRC',.25,4,.01,'ratio'],['COLR',0,2,1,['WHT','WARM','DARK']],['MOVE',0,24,.1,'st'],['RATE',.01,20,0,'hz'],['LEAK',0,1,.01,'pct']];
const SYNTH_FM_SPECS=[['ALG',0,3,1,['2OP','SER','PAR','RING']],['RAT1',.125,32,.125,'ratio'],['IDX1',0,12,.05],['RAT2',.125,32,.125,'ratio'],['IDX2',0,12,.05],['FDBK',0,.9,.01,'pct'],['DETN',0,30,.1,'cent'],['MIX',0,1,.01,'pct']];
const SYNTH_WAVE_SPECS=[['WAVE',0,3,1,['SINE','TRI','SAW','PULS']],['PW',.05,.95,.01,'pct'],['DETN',0,50,.1,'cent'],['SUB',0,1,.01,'pct'],['CUTO',20,20000,0,'hz'],['RES',0,.95,.01,'pct'],['NOIS',0,1,.01,'pct']];
const SYNTH_BLOOM_SPECS=[['MODE',0,2,1,['FIX','EVOL','RAND']],['SPEC',0,3,1,['A','B','C','D']],['PART',1,8,.1],['GAP',50,1000,0,'ms'],['SCAT',0,1,.01,'pct'],['RISE',2,200,0,'ms'],['SUS',0,200,0,'ms'],['DEC',20,2000,0,'ms'],['RVAR',0,1,.01,'pct'],['SVAR',0,1,.01,'pct'],['DVAR',0,1,.01,'pct'],['PJIT',0,1,.01,'st'],['AJIT',0,1,.01,'pct'],['BEND',0,1,.01,'st'],['OCT',0,1,.01,'pct'],['SHAP',.25,4,.01]];
const SYNTH_MOD_SPECS=[['LFO',0,4,1,['SINE','TRI','SAW','SQR','RAND']],['RATE',.01,40,0,'hz'],['AMT',0,1,.01,'pct'],['DEST',0,4,1],['FADE',0,12000,0,'ms'],['PHAS',0,1,.01,'phase'],['FIT',0,1,1,['FREE','FILE']]];
const SYNTH_RESYNTH_SPECS=[['MODE',0,1,1,['PVOC','SINE']],['TUNE',-24,24,.1,'st'],['STRT',0,.99,.005,'pct'],['END',.01,1,.005,'pct'],['BLUR',0,1,.01,'pct'],['SMTH',0,1,.01,'pct'],['PART',4,64,1],['FLOR',-96,-6,1,'db']];
const SYNTH_CONV_SPECS=[['PPOS',0,.9999,.001,'pct1'],['PLEN',1,12000,0,'ms'],['IPOS',0,.9999,.001,'pct1'],['ILEN',.05,12000,0,'ms'],['WIN',0,3,1,['FLAT','TRI','HANN','EXP']],['TONE',-1,1,.01,'pct'],['MIX',0,1,.01,'pct'],['TAIL',0,1,1,['TRIM','FULL']]];
const SYNTH_DRUM_SPECS=[['MODL',0,10,1,['KICK','SNAR','TOM','BELL','HAT','CRSH','MEMB','MODAL','MEDM','CBAS','FMD']],['TUNE',18,3200,0,'hz'],['DEC',30,8000,0,'ms'],['BODY',0,1,.01,'pct'],['NOIS',0,1,.01,'pct'],['HIT',0,1,.01,'pct'],['TONE',0,1,.01,'pct'],['DRIV',0,1,.01,'pct'],['BEND',0,48,.5,'st'],['SWP',2,500,0,'ms'],['RING',0,1,.01,'pct'],['DAMP',0,1,.01,'pct'],['FORM',0,15,1],['X',0,1,.01,'pct'],['Y',0,1,.01,'pct'],['AMNT',0,1,.01,'pct']];
const SYNTH_DUST_SPECS=[['MODE',0,5,1,['DUST','CRKL','STAT','CLIK','CUTS','DATA']],['RATE',.1,2000,0,'hz'],['SIZE',.02,500,0,'ms'],['CLMP',0,1,.01,'pct'],['TONE',20,20000,0,'hz'],['EDGE',0,1,.01,'pct'],['BITS',2,24,1],['VAR',0,1,.01,'pct'],['BED',0,1,.01,'pct'],['COLR',0,3,1,['WHT','PINK','BRWN','BLUE']],['REPT',1,16,1],['ERR',0,1,.01,'pct']];
const SYNTH_WT_TABLES=['ROUND','PUNCH','BUZZ','FOLD','METAL','HOLW','REED','GLASS'];
const SYNTH_WT_SPECS=[['MODL',0,5,1,['KICK','SNARE','TOM','HAT','CLAP','METAL']],['TUNE',20,4000,0,'hz'],['DEC',20,6000,0,'ms'],['WT A',0,7,1,SYNTH_WT_TABLES],['WT B',0,7,1,SYNTH_WT_TABLES],['MIX',0,1,.01,'pct'],['SCAN',-1,1,.01,'pct'],['HIT',0,1,.01,'pct'],['POSA',0,1,.01,'pct'],['POSB',0,1,.01,'pct'],['RATI',.125,16,0,'ratio'],['PM',0,4,.02],['DETN',0,50,.5,'cent'],['SUB',0,1,.01,'pct'],['NOIS',0,1,.01,'pct'],['TONE',0,1,.01,'pct'],['BEND',-48,60,1,'st'],['PDEC',.25,500,0,'ms'],['SDEC',.25,2000,0,'ms'],['NDEC',1,6000,0,'ms'],['COLR',0,3,1,['WHT','PINK','HP','METAL']],['DRIV',0,1,.01,'pct'],['COMP',0,1,.01,'pct'],['EDGE',.05,5,0,'ms']];
const SYNTH_WT_DEFAULTS=[0,48,600,1,2,.18,.55,.8,.1,.35,2,.18,0,.35,.025,.85,32,22,38,18,2,.3,.45,.15];
const SYNTH_DUST_DEFAULTS=[0,20,1.2,.2,10000,0,16,.75,0,0,1,.25];
const SYNTH_PLUCK_SPECS=[['TUNE',20,2000,0,'hz'],['DEC',30,8000,0,'ms'],['DAMP',0,.98,.01,'pct'],['BRIT',0,1,.01,'pct'],['DISP',0,1,.01,'pct'],['PICK',0,1,.01,'pct'],['HARD',0,1,.01,'pct'],['DETN',0,50,.5,'cent']];
const SYNTH_SUB_SPECS=[['HARM',1,16,1],['SUBS',0,16,1],['BAL',0,1,.01,'pct'],['TILT',-18,6,.5,'db'],['DETN',0,30,.1,'cent'],['DRFT',0,30,.1,'cent'],['RATE',.01,4,0,'hz'],['REG',-2,2,1]];
const SYNTH_HYBRID_SPECS=[['MODE',0,4,1,['CHIM','GRAFT','MASK','MIRAG','SWARM']],['SRCS',2,4,1],['TUNE',-24,24,.1,'st'],['DENS',0,1,.01,'pct'],['MUT',0,1,.01,'pct'],['TEXT',0,1,.01,'pct'],['LIFE',20,2000,0,'ms'],['DRFT',0,1,.01,'pct']];
const SYNTH_TRANSFER_SPECS=[['ENV',0,1,.01,'pct'],['SPEC',0,1,.01,'pct'],['PTCH',0,1,.01,'pct'],['ALGN',0,2,1,['FIT','TIME','LOOP']],['OFFS',-10000,10000,5,'signedMs'],['ATK',0,1,.01,'pct'],['NOIS',0,1,.01,'pct'],['TUNE',-24,24,.1,'st'],['SRCS',2,4,1],['ENVS',1,3,1,['','B','C','D']],['SPCS',1,3,1,['','B','C','D']],['PTCS',1,3,1,['','B','C','D']],['SMTH',5,200,1,'ms'],['RAND',0,1,.01,'pct'],['STRT',0,4,1,['CUST','RHYTH','COLR','PITCH','COMB']]];
const SYNTH_TRANSFER_DEFAULTS=[.8,0,0,0,0,.75,1,0,2,1,1,1,20,0,1];
function synthHybridNew(){return synthType()===11&&synthUi.data?.hybridStyle===1;}
function synthHybridSourceCount(){return Math.round(synthHybridNew()?synthUi.data.transfer[8]:synthUi.data.hybridParams[1]);}
const SYNTH_TRACE_SPECS=[['TRCE',0,3,1,['LINK','POINT','SMEAR','FREEZ']],['DENS',0,1,.01,'pct'],['LIFE',10,2000,0,'ms'],['DRFT',0,1,.01,'pct'],['JITR',0,1,.01,'pct'],['TOL',10,600,1,'centInt'],['HOLD',0,1,.01,'pct'],['SPRD',0,1,.01,'pct']];
const SYNTH_EVENT_SPECS=[['MODE',0,4,1,['OFF','GRID','CHANC','SWELL','GROUP']],['EVTS',1,64,1],['DENS',0,1,.01,'pct'],['JITR',0,1,.01,'pct'],['SPAN',0,48,.5,'st'],['LIFE',40,4000,0,'ms'],['VAR',0,1,.01,'pct'],['PAN',0,1,.01,'pct']];
const SYNTH_TIMING_SPECS=[['CLCK',0,1,1,['FREE','BEAT']],['BPM',1,999,1],['DIV',0,13,1,['1/1','1/2','1/2T','1/4','1/4T','1/8','1/8T','1/16','1/16T','1/32','1/32T','1/64','1/8D','1/16D']],['GATE',.05,4,.01,'pct'],['SWNG',0,.75,.01,'pct'],['',0,0,0],['',0,0,0],['',0,0,0]];
const SYNTH_ATTACK_SPECS=[['PNCH',0,1,.01,'pct'],['HDEC',.25,4000,0,'ms'],['NDEC',1,12000,0,'ms'],['EXCT',0,1,.01,'pct'],['BIAS',-1,1,.01,'pct'],['COMP',0,1,.01,'pct'],['SNAP',0,1,.01,'pct'],['EDGE',.05,2,0,'ms']];
const SYNTH_COLOR_SPECS=[['BITS',4,24,1],['RATE',375,48000,0,'colorHz'],['TONE',20,20000,0,'colorTone'],['MIX',0,1,.01,'pct'],['',0,0,0],['',0,0,0],['',0,0,0],['',0,0,0]];
const SYNTH_WORK_FAMILIES=['pluck','sub','hybrid','trace','events','timing','attack','color'];
const SYNTH_WORK_DEFAULTS=[[110,1200,.35,.65,.08,.35,.1,8],[8,4,.25,-6,0,0,.25,0],[0,2,0,.8,.65,.35,140,.1],[0,1,120,0,0,144,.5,0],[0,8,1,0,0,500,0,.35],[1,120,5,.7,0,0,0,0],[.94,4,80,.12,0,0,.35,.15],[24,48000,20000,1,0,0,0,0]];
function synthIsDrum(){return [8,14].includes(synthType());}
function synthEventsAvailable(){return [2,4,8,9,14].includes(synthType());}
function synthEventsActive(){return synthEventsAvailable()&&synthUi.data?.events?.[0]>0;}
function synthWorkshopFamily(page=synthUi.page){return page===9?'events':page===10?'trace':page===13&&synthType()!==13?'attack':synthHybridNew()?'transfer':SYNTH_FAMILIES[synthType()];}
function synthDuration(){return synthType()===7?(synthUi.data?.seconds||synthUi.data.values[0]):synthUi.data.values[0];}
function synthSoundPages(){const t=synthType();return [0,...(t===7?[8]:t===11?[11]:[]),1,...(t===2||t===8||t===12||t===13||t===14||synthHybridNew()?[6]:[]),...([8,14].includes(t)?[13,14]:t===13?[13]:[]),...([2,4,5,10].includes(t)?[7]:[]),...(synthEventsAvailable()?[9,12]:[]),...(t===3&&synthUi.data?.spectral?.[0]===1?[10]:[])];}
function synthGroup(){return [5,2,3].includes(synthUi.page)?1:synthUi.page===4?2:0;}
function synthGroupPages(group=synthGroup()){return group===0?synthSoundPages():group===1?[5,2,3]:[4];}
function synthNav(){synthUi.groupMemory[synthGroup()]=synthUi.page;synthUi.selection=synthGroup();synthUi.overlay='nav';resetParameterEncoderMotion();}
function selectSynthGroup(){const group=synthUi.selection;if(group===3){synthUi.overlay='actions';synthUi.selection=0;return;}const pages=synthGroupPages(group);synthUi.page=pages.includes(synthUi.groupMemory[group])?synthUi.groupMemory[group]:pages[0];synthUi.overlay='';resetParameterEncoderMotion();}
function rememberSynthEditor(){
    const d=synthUi.data;if(!d?.open)return;
    synthUi.groupMemory[synthGroup()]=synthUi.page;
    const raw=[synthType(),synthUi.page,...synthUi.groupMemory,synthUi.amount,synthUi.low,synthUi.high,synthUi.keepEnds?1:0].join(',');
    if(raw===synthUi.editorRaw)return;
    setParamBlocking('synth_editor',raw);synthUi.editorRaw=raw;
}
function restoreSynthEditor(){
    const raw=String(getParam('synth_editor','')||''),v=raw.split(',').map(Number);
    if(v.length!==9||!v.every(Number.isFinite)||v[0]!==synthType())return;
    synthUi.page=synthPageIds().includes(v[1])?v[1]:0;
    synthUi.groupMemory=v.slice(2,5).map((page,group)=>synthGroupPages(group).includes(page)?page:synthGroupPages(group)[0]);
    synthUi.amount=clamp(v[5],0,1);const limit=synthLimits()[synthUi.row];synthUi.low=clamp(v[6],-limit,limit);synthUi.high=clamp(v[7],synthUi.low,limit);synthUi.keepEnds=Boolean(v[8]);synthUi.editorRaw='';
}
function synthContext(){return state.view==='synth'||(state.view==='library'&&Boolean(synthUi.browserTarget));}
function openSynthBrowser(which){if(which.startsWith('hybrid'))synthUi.page=11;synthUi.browserTarget=which;synthUi.overlay='';synthCommand('preview',0);setParam('preview_stop',1);state.libraryLane=state.lane;state.capturesDirectory=String(getParam('captures_directory',state.capturesDirectory)||'');scanLibrary(LIBRARY_HOME);state.view='library';rebuildLedQueue();}
function returnSynthBrowser(){setParam('preview_stop',1);synthUi.browserTarget='';state.view='synth';readSynth();resetParameterEncoderMotion();rebuildLedQueue();}
function synthType(){return synthUi.data?.type||0;}
function synthPageIds(){return [...synthSoundPages(),5,2,3,4];}
function moveSynthPage(delta){const pages=synthGroupPages(),index=Math.max(0,pages.indexOf(synthUi.page));synthUi.page=pages[clamp(index+Math.sign(delta),0,pages.length-1)];resetParameterEncoderMotion();}
function synthRowNames(){if(synthHybridNew())return ['PITCH','ENVELOPE','COLOUR','LEVEL'];if(synthType()===13)return ['PITCH','TABLE','TONE','LEVEL'];if(synthType()===12)return ['DENSITY','SIZE','TONE','LEVEL'];if(synthIsDrum())return synthEventsActive()?['PITCH','DENSITY','LIFETIME','LEVEL']:['RESERVED','RESERVED','RESERVED','LEVEL'];return [['PITCH','POINTS','EVOLUTION','LEVEL'],['PITCH','PWIDTH','CUTOFF','LEVEL'],['PITCH','PARTIALS','SPACING','LEVEL'],['PITCH','BLUR','SMOOTH','LEVEL'],['PITCH','COUNT','STRETCH','LEVEL'],['PITCH','BANDWIDTH','MOTION','LEVEL'],['PITCH','INDEX 1','INDEX 2','LEVEL'],['RESERVED','RESERVED','RESERVED','LEVEL'],['RESERVED','RESERVED','RESERVED','LEVEL'],['PITCH','DAMPING','BRIGHTNESS','LEVEL'],['PITCH','SUB BALANCE','TILT','LEVEL'],['PITCH','DENSITY','MUTATION','LEVEL']][synthType()];}
function synthRowLabel(row){return synthType()===6&&[1,2].includes(row)?'IDX'+row:synthRowNames()[row].slice(0,4);}
function synthLimits(){return [[24,16,4,24],[24,.45,4,24],[24,7,4,24],[24,1,1,24],[24,31,2,24],[24,4,24,24],[24,12,12,24],[24,1,1,24],[24,1,1,24],[24,1,1,24],[24,1,12,24],[24,1,1,24],[4,4,4,24],[24,1,1,24],[24,1,1,24]][synthType()];}
function synthCurveAvailable(row){if((synthType()===7||(synthIsDrum()&&!synthEventsActive()))&&row!==3)return false;return !(synthType()===1&&row===1&&synthUi.data.osc[0]!==3)&&!(synthType()===6&&row===2&&synthUi.data.fmOsc[0]===0);}
function synthModActive(){const m=synthUi.data?.mod;return m&&m[2]>0&&m[3]>0&&synthCurveAvailable(m[3]-1);}
function synthToneActive(){const d=synthUi.data,c=d.curves[2];return d.osc[4]<19999||(synthModActive()&&d.mod[3]===3)||c.slice(1).some((p,i)=>(c[0]&(1<<i))&&p[1]<0);}
function synthDetailAvailable(family,field){
    const d=synthUi.data;
    if(family==='transfer')return field<15;
    if(family==='dust')return !(field===8&&d.dustParams[0]===2)&&!(field===9&&d.dustParams[0]===5&&d.dustParams[8]===0)&&!(field===11&&d.dustParams[0]!==5);
    if(family==='wave')return (field!==1||d.osc[0]===3)&&(field!==5||synthToneActive());
    if(family==='bloom')return field!==1||d.packets[0]===0;
    if(family==='resynth')return field!==6||d.spectral[0]===1;
    if(family==='additive')return field!==6||d.additive[5]>0;
    if(family==='noise')return field!==6||d.noise[5]>0||(synthModActive()&&d.mod[3]===3)||d.curves[2].slice(1).some((p,i)=>(d.curves[2][0]&(1<<i))&&p[1]>0);
    if(family==='fm')return d.fmOsc[0]!==0||![3,4].includes(field);
    if(family==='events')return synthEventsAvailable()&&(field===0||d.events[0]>0)&&!(field===7&&d.values[2]===0)&&!(field===5&&d.timing[0]===1);
    if(family==='trace')return d.spectral[0]===1&&!(field===2&&d.trace[0]===0)&&!(field===4&&d.trace[0]===0)&&!(field===5&&d.trace[0]!==0)&&!(field===6&&d.trace[0]!==3)&&!(field===7&&d.values[2]===0)&&!(field===5&&d.timing[0]===1);
    if(family==='sub')return !(field===6&&d.sub[5]===0)&&!(field===2&&d.sub[1]===0);
    if(family==='timing')return synthEventsActive()&&field<5&&(field===0||d.timing[0]===1);
    if(['pluck','hybrid'].includes(family))return true;
    if(family==='wt')return !(field===17&&d.wt[16]===0)&&!(field===18&&d.wt[6]===0)&&!([19,20].includes(field)&&d.wt[14]===0)&&!(field===12&&d.values[2]===0);
    if(family==='conv')return true;
    if(family==='attack')return synthAttackAvailable(field);
    if(family==='color')return synthIsDrum()&&field<4;
    if(family==='drum'&&d.drums[0]===10)return true;
    if(family==='drum'){const m=d.drums[0];if(m===9)return !(field===15&&d.drums[12]!==2)&&!(field===2&&d.drums[12]!==0);return !(field>=12&&m<6)&&!(field===4&&[2,6].includes(m))&&!(field===10&&m===6)&&!([8,9].includes(field)&&[4,5,7,8].includes(m));}
    if(field===3)return true;
    if(field===2)return d.mod[3]>0&&synthCurveAvailable(d.mod[3]-1);
    return synthModActive();
}
function synthSetupAvailable(field){
    if(field===13&&synthHybridNew())return synthUi.data.transfer[13]>0||(synthModActive()&&synthUi.data.mod[0]===4);
    const d=synthUi.data;if(field===13&&synthType()===10)return d.values[2]>0||d.sub[4]>0||d.sub[5]>0||(synthModActive()&&d.mod[0]===4);if(field===13&&(synthType()>=9||synthEventsActive()||(synthType()===3&&d.spectral[0]===1&&(d.trace[0]>0||d.trace[1]<1||d.trace[3]>0||d.trace[7]>0))))return true;
    if(field===2&&synthType()===9&&d.plucked[7]===0&&(!synthEventsActive()||d.events[7]===0))return false;
    if(field===13&&synthType()>=7&&!(synthType()===8&&(d.drums[0]===7||(d.drums[0]===8&&d.drums[4]>0)||(d.drums[0]===10&&(d.drums[4]>0||d.drums[5]>0))))&&!(synthModActive()&&d.mod[0]===4))return false;
    if([3,4].includes(field)&&d.values[12]>.5)return false;
    if(field===2&&synthType()===1&&d.osc[2]===0&&d.osc[6]===0)return false;
    if(field===13&&[1,3].includes(synthType())&&!(synthModActive()&&d.mod[0]===4)&&(synthType()!==1||d.osc[6]===0))return false;
    if(field===2&&synthType()===6&&d.fmOsc[6]===0)return false;
    if(field===13&&synthType()===6&&!(synthModActive()&&d.mod[0]===4))return false;
    if(field===13&&synthType()===4&&d.additive[3]===0&&d.additive[4]===0&&d.additive[5]===0&&d.values[2]===0&&!(synthModActive()&&d.mod[0]===4))return false;
    return true;
}
function synthSpecs(family){if(family==='transfer'){const specs=SYNTH_TRANSFER_SPECS.map(v=>[...v]);for(const f of [9,10,11])specs[f][2]=(synthUi.data?.transfer?.[8]||2)-1;return specs;}if(family==='wt')return SYNTH_WT_SPECS;if(family==='dust')return SYNTH_DUST_SPECS;const work={pluck:SYNTH_PLUCK_SPECS,sub:SYNTH_SUB_SPECS,hybrid:SYNTH_HYBRID_SPECS,trace:SYNTH_TRACE_SPECS,events:SYNTH_EVENT_SPECS,timing:SYNTH_TIMING_SPECS,attack:synthAttackSpecs(),color:SYNTH_COLOR_SPECS};if(work[family])return work[family];return family==='wave'?SYNTH_WAVE_SPECS:family==='bloom'?SYNTH_BLOOM_SPECS:family==='resynth'?SYNTH_RESYNTH_SPECS:family==='additive'?SYNTH_ADDITIVE_SPECS:family==='noise'?SYNTH_NOISE_SPECS:family==='fm'?SYNTH_FM_SPECS:family==='conv'?SYNTH_CONV_SPECS:family==='drum'?synthDrumSpecs():SYNTH_MOD_SPECS;}
function synthDrumSpecs(){
    const m=synthUi.data?.drums?.[0]||0,s=SYNTH_DRUM_SPECS.map(v=>[...v]);
    if(synthType()===8)s[0][2]=9;
    s[1][1]=[20,70,35,180,320,180,25,30,20,24,20][m];s[1][2]=[180,420,180,2400,3200,2400,90,1200,2000,96,3200][m];
    s[2][1]=[30,30,40,30,30,80,80,50,50,150,30][m];s[2][2]=[8000,3000,4000,2000,1000,8000,6000,12000,12000,8000,8000][m];
    s[8][2]=[48,36,30,24,48,48,48,48,48,1,48][m];s[9][1]=m===3?3:m===6?5:2;s[9][2]=[500,250,300,500,500,500,250,500,500,1,500][m];
    if(m===6){s[12]=['SHAP',0,4,1,['CIRC','ELLIP','SQUAR','TRI','IRREG']];s[15][0]='AMNT';}
    if(m===7){s[3][0]='MASS';s[4][0]='IRRG';s[10][0]='COUP';s[12]=['MATL',0,15,1,['DEEP','TIER','BROAD','BRGHT','CARBN','GLASS','ALUM','SPRC','STEEL','PORC','EARTH','SKIN','LOAD','COUP','CAV','LOOSE']];s[15]=['BODS',0,1,.25,'bodies'];}
    if(m===8){s[1][0]='SPED';s[1][4]='';s[3][0]='SIZE';s[3][4]='metres';s[10][0]='DISP';s[12]=['EXCT',0,3,1,['HIT','BOW','REED','AIR']];s[15][0]='Z';}
    if(m===9){s[4][0]='AIR';s[5][0]='IMPT';s[8]=['SIZE',0,1,.01,'pct'];s[9]=['TENS',0,1,.01,'pct'];s[10][0]='BLOM';s[12]=['ART',0,2,1,['CNTR','MUTE','RIM']];s[13][0]='POS';s[14][0]='SHEL';s[15][0]='RLEV';}
    if(m===10){s[10]=['RATI',.125,16,.025,'ratio'];s[11][0]='FDBK';s[12]=['WAVE',0,3,1,['SINE','TRI','SAW','PULS']];s[13][0]='FM';s[14]=['FDEC',0,1,.01,'fmDecay'];s[15][0]='MOD';}
    return s;
}
function synthFmDrumCells(page=synthUi.page){return page===1?[['drum',1],['drum',2],['drum',3],['drum',6],['drum',10],['drum',13],['drum',12],['drum',14]]:page===6?[['drum',8],['drum',9],['drum',11],['drum',15],['drum',4],['drum',5],['attack',1],['attack',2]]:[['attack',0],['attack',6],['attack',7],null,null,null,null,null];}
function synthColorCells(){return [['drum',7],['attack',4],['attack',5],null,['color',0],['color',1],['color',2],['color',3]];}
function synthAttackAvailable(field){
 const d=synthUi.data,m=d.drums[0];
 if(!synthIsDrum())return false;
 if(field===0)return m<6||m>=9;
 if(field===1)return [0,2,3,10].includes(m)||(m===9&&d.drums[12]===1);
 if(field===2)return [0,1,3,5,10].includes(m)||(m===9&&d.drums[12]===2);
 if(field===3)return (m<6&&m!==2)||(m===9&&d.drums[12]===2);
 if(field===4)return ![6,8].includes(m)&&d.drums[7]>0;
 return true;
}
function synthAttackSpecs(){
 const m=synthUi.data?.drums?.[0]||0,s=SYNTH_ATTACK_SPECS.map(v=>[...v]);
 s[0][0]=['PNCH','PNCH','PNCH','SHAP','ATK','TEXT','PNCH','PNCH','PNCH','HARD','PNCH'][m];
 s[1][0]=m===3?'BDEC':m===9?'MDEC':'HDEC';s[2][0]=m===1?'WDEC':m===5?'WDEC':m===9?'RDEC':'NDEC';
 s[3][0]=['PSET','RATL','EXCT','DETN','ALLO','ALLO','EXCT','EXCT','EXCT','RTON','EXCT'][m];
 s[1][1]=m===2?.5:m===3?20:m===9?40:.25;s[1][2]=[3,9].includes(m)?1500:40;
 s[2][1]=m===3?4:m===5?80:m===9?15:10;s[2][2]=m===3?400:m===5?8000:m===9?1000:4000;
 return s;
}
function synthLegacyAttack(data){const m=data.drums?.[0]||0,decay=data.drums?.[2]||600,character=data.drums?.[7]||0;
 const a=[.72,6.5,120,.25,0,character*.25,0,2];
 if(m===1){a[0]=.78;a[2]=clamp(decay*.8,10,4000);a[3]=.18;}
 if(m===2)a[1]=4.5;
 if(m===3){a[0]=.68;a[1]=clamp(decay*.7,20,1500);a[2]=35;a[3]=.08;}
 if(m===4){a[0]=.66;a[3]=.62;}
 if(m===5){a[0]=.58;a[2]=decay;a[3]=.58;}
 return a;
}
function synthDetailValues(family){const d=synthUi.data;if(family==='transfer')return d.transfer;if(family==='wt')return d.wt;if(family==='dust')return d.dustParams;const work={pluck:d.plucked,sub:d.sub,hybrid:d.hybridParams,trace:d.trace,events:d.events,timing:d.timing,attack:d.drumExtra,color:d.drumColor};if(work[family])return work[family];return family==='wave'?d.osc:family==='bloom'?d.packets:family==='resynth'?d.spectral:family==='additive'?d.additive:family==='noise'?d.noise:family==='fm'?d.fmOsc:family==='conv'?d.conv:family==='drum'?d.drums:d.mod;}
function synthDetailValue(family,field){
    const spec=synthSpecs(family)[field],v=synthDetailValues(family)[field],format=spec[4];
    if(family==='mod'&&field===3)return ['OFF',...synthRowNames().map((_,row)=>synthRowLabel(row))][Math.round(v)];
    if(family==='mod'&&field===1&&synthUi.data.mod[6])return Math.round(v)+'cy';
    if(Array.isArray(format))return format[Math.round(v)];
    if(format==='fmDecay'){const ms=2*Math.pow(2000,v);return ms>=1000?(ms/1000).toFixed(1)+'s':Math.round(ms)+'ms';}
    if(format==='colorHz'||format==='colorTone')return v>=(format==='colorHz'?48000:19999)?(format==='colorHz'?'FULL':'OPEN'):v>=1000?(v/1000).toFixed(1)+'k':Math.round(v)+'Hz';
    if(format==='bodies')return String(4+Math.round(v*4));
    if(format==='metres')return (.02+v*3.98).toFixed(2)+'m';
    if(format==='pct1')return (v*100).toFixed(1)+'%';
    if(format==='pct')return Math.round(v*100)+'%';
    if(format==='ratio')return v.toFixed(v<10?3:2).replace(/0+$/,'').replace(/\.$/,'');
    if(format==='phase')return Math.round(v*360)+'d';
    if(format==='signedMs'){const a=Math.abs(v),sign=v<0?'-':v>0?'+':'';return sign+(a>=10000?Math.round(a/1000)+'s':a>=1000?(a/1000).toFixed(1)+'s':a>=100?(a/1000).toFixed(2).replace(/^0/,'')+'s':Math.round(a)+'ms');}
    if(format==='ms'&&v<1)return v.toFixed(2).replace(/^0/,'')+'ms';
    if(format==='ms')return v>=1000?(v/1000).toFixed(1)+'s':Math.round(v)+'ms';
    if(format==='hz')return v>=1000?(v/1000).toFixed(1)+'k':v.toFixed(v<10?2:0);
    if(format==='st')return v.toFixed(1);
    if(format==='centInt')return Math.round(v)+'c';
    if(format==='cent')return v.toFixed(1)+'c';
    if(format==='db')return Math.round(v)+'dB';
    return Number.isInteger(v)?String(v):v.toFixed(v<1?2:1);
}
function adjustSynthDetail(family,field,delta){
    if(!synthDetailAvailable(family,field))return;
    const spec=synthSpecs(family)[field],current=synthDetailValues(family)[field];let value=current+delta*spec[3];
    if(Array.isArray(spec[4])||(spec[3]===1&&spec[2]-spec[1]<=8))value=current+Math.sign(delta);
    if(family==='mod'&&field===1&&synthUi.data.mod[6])value=clamp(Math.round(current+delta),1,64);
    else if(family==='wt'&&[1,2,10,17,18,19,23].includes(field))value=current*Math.pow(2,delta/12);
    else if(family==='dust'&&[1,2,4].includes(field))value=current*Math.pow(2,delta/12);
    else if(spec[3]===0)value=Math.max(spec[1],(current+1)*Math.pow(2,delta/12)-1);
    value=clamp(value,spec[1],family==='mod'&&field===1&&synthUi.data.mod[6]?64:spec[2]);
    synthCommand(family,field+','+value);
}
function synthLockEntries(){
    const d=synthUi.data,result=[0,1,2,3,4,5,12].map(field=>({family:'lock',field,label:SYNTH_FIELDS[field],on:Boolean(d.locks&(1<<field))}));
    if(synthType()===0)for(let field=6;field<=11;++field)result.push({family:'lock',field,label:SYNTH_FIELDS[field],on:Boolean(d.locks&(1<<field))});
    const family=SYNTH_FAMILIES[synthType()],offset=[0,0,7,30,38,46,54][synthType()];
    const appendWork=(name)=>{const group=SYNTH_WORK_FAMILIES.indexOf(name);synthSpecs(name).forEach((spec,field)=>{if((name==='timing'&&field>=5)||(name==='color'&&field>=4))return;result.push({family:'work_lock',field:group*8+field,label:(name==='events'?'E ':name==='trace'?'T ':'')+spec[0],on:d.workProtected[group*8+field]==='1'});});};
    if(synthHybridNew())synthSpecs('transfer').forEach((spec,field)=>result.push({family:'transfer_lock',field,label:spec[0],on:Boolean(d.hybridLocks&(1<<field))}));
    else if(synthType()===13)SYNTH_WT_SPECS.forEach((spec,field)=>result.push({family:'wt_lock',field,label:spec[0],on:d.wtProtected[field]==='1'}));
    else if(synthType()===12)SYNTH_DUST_SPECS.forEach((spec,field)=>result.push({family:'dust_lock',field,label:spec[0],on:d.dustProtected[field]==='1'}));
    else if(synthType()===14)synthSpecs('drum').forEach((spec,field)=>{if(field)result.push({family:'extended_lock',field:8+field,label:spec[0],on:Boolean(d.extendedLocks&(1<<(8+field)))});});
    else if(synthType()>=9)appendWork(family);
    else if(family)synthSpecs(family).forEach((spec,field)=>result.push(synthType()>=7?{family:'extended_lock',field:(synthType()===8?8:0)+field,label:spec[0],on:Boolean(d.extendedLocks&(1<<((synthType()===8?8:0)+field)))}:{family:'detail_lock',field:offset+field,label:spec[0],on:d.protected?.[offset+field]==='1'}));
    if(synthEventsAvailable()){appendWork('events');appendWork('timing');}if(synthType()===3&&d.spectral[0]===1)appendWork('trace');if(synthIsDrum()){appendWork('attack');appendWork('color');}
    SYNTH_MOD_SPECS.forEach((spec,field)=>result.push({family:'detail_lock',field:23+field,label:'M '+spec[0],on:d.protected?.[23+field]==='1'}));
    return result;
}
function synthActions(){
    const entries=SYNTH_ACTIONS.map((title,id)=>({title,id}));
    entries[1].title=synthUi.data?.preview?'PREVIEW OFF':'PREVIEW ON';entries[4].title='SAVE & LOAD L'+state.lane;
    if(synthIsDrum())entries.unshift({title:synthType()===14?'FM DRUM START':'DRUM START',id:24});
    if(synthType()===13)entries.unshift({title:'MODEL START',id:25});
    if(synthType()===11)entries.unshift(...Array.from({length:synthHybridSourceCount()},(_,i)=>({title:'PICK SOURCE '+String.fromCharCode(65+i),id:20+i})));
    if(synthType()===11&&!synthHybridNew())entries.push({title:'UPGRADE HYBRID',id:26});
    if(synthType()===7)entries.unshift({title:'PICK PRIMARY',id:13},{title:'PICK IMPULSE',id:14});
    if(synthType()===3)entries.unshift({title:'USE LANE '+state.lane+' AS SOURCE',id:12});
    return entries;
}
function synthSpectrumPad(row,point){
    const d=synthUi.data,current=d.partials?.[point];
    if(synthUi.copy){
        let p=current;if(row!==d.spectrum){const hex=getParam('synth_spectrum:'+row,'');if(!/^[0-9a-f]{128}$/.test(hex))return;const bytes=Uint8Array.from(hex.match(/../g).map(s=>parseInt(s,16))),view=new DataView(bytes.buffer);p=[view.getFloat32(point*8,true),view.getFloat32(point*8+4,true)];}
        synthUi.clipboard={family:'spectrum',ratio:p[0],level:p[1]};synthUi.notice='PARTIAL COPIED';}
    else if(state.shiftHeld&&synthUi.clipboard?.family==='spectrum'){
        synthCommand('spec_select',row+','+point);synthCommand('spec_edit',[row,point,0,synthUi.clipboard.ratio].join(','));synthCommand('spec_edit',[row,point,1,synthUi.clipboard.level].join(','));
    }else{synthCommand('spec_select',row+','+point);if(state.deleteHeld)synthCommand('spec_edit',[row,point,1,0].join(','));}
    resetParameterEncoderMotion();
}
function synthComponentActive(row,point){
 const d=synthUi.data,t=synthType(),i=row*8+point;
 if(t===10)return i<16?i<d.sub[0]&&d.sub[2]<1:i-16<d.sub[1]&&d.sub[2]>0;
 return t===2||i<(t===4?d.additive[0]:d.noise[0]);
}
function synthEventCells(){return synthUi.page===9?[['events',0],['timing',1],['timing',2],['events',1],['timing',3],['timing',4],['events',4],['events',7]]:[['events',2],['events',3],['events',6],null,['timing',0],['events',5],null,null];}
function drawSynthSpectrum(){
    const d=synthUi.data,selected=d.component||0;
    for(let i=0;i<8;++i){const value=d.partials[i][1],x=5+i*15,y=25,height=Math.max(1,Math.round(Math.abs(value)*13));
        drawRect(x,y-1,10,1,1);drawRect(x,value>=0?y-height:y,10,height,1);if(value<0&&height>2)drawRect(x+1,y+1,8,height-2,0);
        if(i===selected)drawRect(x-1,10,12,2,1);
    }
    const p=d.partials[selected];for(const [index,label,value] of [[0,synthType()===10&&d.spectrum>=2?'DIV':'RATIO',p[0].toFixed(2)],[1,'LEVEL',Math.round(p[1]*100)+'%'],[3,synthType()===10?(d.spectrum<2?'HARM':'SUB'):'PART',(synthType()===10?d.spectrum%2*8+selected+1:synthType()===2?selected+1:d.spectrum*8+selected+1)+'/'+(synthType()===10?16:synthType()===2?8:32)]]){print(index*32+3,40,label.slice(0,5),1);print(index*32+3,51,String(value).slice(0,5),1);}drawReservedParameterCell(6);
}

function decodeSynthRecipe(hex,data){
    if(typeof hex!=='string'||![1784,1976,2144,2176,2512,2576,2648,2712,2816,2952,3016].includes(hex.length)||!/^[0-9a-f]+$/.test(hex))return false;
    const bytes=Uint8Array.from(hex.match(/../g).map(s=>parseInt(s,16))),view=new DataView(bytes.buffer);let at=0,crc=2166136261;
    for(let i=0;i<bytes.length-4;++i)crc=Math.imul(crc^bytes[i],16777619)>>>0;
    if(crc!==view.getUint32(bytes.length-4,true))return false;
    const uint=()=>{const v=view.getUint32(at,true);at+=4;return v;},scalar=()=>{const v=view.getFloat32(at,true);at+=4;return v;},array=n=>Array.from({length:n},scalar);
    const version=uint();if(!((version===2&&bytes.length===892)||(version===3&&bytes.length===988)||(version===4&&bytes.length===1072)||(version===5&&bytes.length===1088)||(version===6&&bytes.length===1256)||(version===7&&bytes.length===1288)||(version===8&&bytes.length===1324)||(version===9&&bytes.length===1356)||(version===10&&bytes.length===1408)||(version===11&&bytes.length===1508)||(version===12&&bytes.length===1408)||(version===13&&bytes.length===1476)))return false;data.type=uint();if(data.type<0||data.type>14||(version===13&&data.type!==11)||(version===12&&data.type!==14)||(version===11&&data.type!==13)||(version===10&&data.type>12)||(version<10&&data.type>11)||(version<=5&&data.type>8)||(version===2&&data.type>3)||(version===3&&data.type>6))return false;data.seed=uint();data.locks=uint();data.scale=uint();data.values=array(13);
    data.curves=Array.from({length:4},()=>[uint(),...Array.from({length:8},()=>[scalar(),scalar(),uint()])]);
    const low=uint(),high=uint();data.protected=Array.from({length:62},(_,i)=>((i<32?low>>>i:high>>>(i-32))&1)?'1':'0').join('');
    data.osc=array(7);data.packets=array(16);data.mod=array(7);data.spectral=array(8);const ratios=Array.from({length:4},()=>array(8)),partials=Array.from({length:4},()=>array(8));
    data.partial_masks=partials.map(p=>p.reduce((mask,v,i)=>mask|(v!==0?1<<i:0),0));const spectrum=clamp(data.spectrum|0,0,3);data.partials=ratios[spectrum].map((v,i)=>[v,partials[spectrum][i]]);
    if(version>=3){data.additive=array(8);data.noise=array(8);data.fmOsc=array(8);}
    if(version>=4){data.conv=array(8);data.drums=array(version===4?12:16);if(version===4)data.drums.push(0,.6,.4,.7);data.extendedLocks=uint();}
    if(version>=6){for(const key of ['plucked','sub','hybridParams','trace','events'])data[key]=array(8);const low=uint(),high=uint();data.workProtected=Array.from({length:64},(_,i)=>(i<32?low>>>i:high>>>(i-32))&1?'1':'0').join('');}
    if(version>=7)data.timing=array(8);
    else{
      data.timing=[...SYNTH_WORK_DEFAULTS[5]];
      data.workProtected=(data.workProtected||'0'.repeat(48)).slice(0,40)+'11111000';
      if(version===6){
        const previous=data.sub;data.sub=[...SYNTH_WORK_DEFAULTS[1]];
        if(data.type===10){data.sub[0]=clamp(Math.round(previous[1]*.5),1,16);data.sub[1]=clamp(Math.round(previous[1]*.25),0,16);data.partials=Array.from({length:8},(_,i)=>[spectrum<2?spectrum*8+i+1:(spectrum-2)*8+i+2,1]);data.partial_masks=[255,255,255,255];}
        if(data.events[0]>0)data.timing[1]=clamp(Math.round(30/(data.values[0]/Math.max(1,data.events[1]))),1,999);
      }
    }
    if(version>=8){data.drumExtra=array(8);data.drumFlags=uint();}else{data.drumFlags=1;data.drumExtra=synthLegacyAttack(data);data.workProtected=(data.workProtected||'0'.repeat(56)).padEnd(54,'0').slice(0,54)+'11';}
    if(version>=9)data.drumColor=array(8);else{data.drumColor=[...SYNTH_WORK_DEFAULTS[7]];data.workProtected=(data.workProtected||'0'.repeat(64)).padEnd(56,'0').slice(0,56)+'11110000';}
    if(version>=10){data.dustParams=array(12);const mask=uint();data.dustProtected=Array.from({length:12},(_,i)=>(mask>>i)&1?'1':'0').join('');}else{data.dustParams=[...SYNTH_DUST_DEFAULTS];data.dustProtected='100000000000';}
    if(version===11){data.wt=array(24);const mask=uint();data.wtProtected=Array.from({length:24},(_,i)=>(mask>>i)&1?'1':'0').join('');}
    else{data.wt=[...SYNTH_WT_DEFAULTS];data.wtProtected='11'+'0'.repeat(22);}
    data.hybridStyle=0;data.hybridLocks=0x5f98;data.transfer=[...SYNTH_TRANSFER_DEFAULTS];
    if(version===13){data.hybridStyle=uint();data.hybridLocks=uint();data.transfer=array(15);if(data.hybridStyle>1||data.hybridLocks>32767)return false;}
    return at===bytes.length-4;
}
function readSynth(){
    const raw=getParam('synth_state','');if(!raw||raw===synthUi.raw)return;
    let d;try{d=JSON.parse(raw);}catch{return;}
    if(d?.recipe&&!decodeSynthRecipe(d.recipe,d))return;
    if(!d||!Array.isArray(d.values)||d.values.length!==13||!d.values.every(Number.isFinite)
        ||!Array.isArray(d.curves)||d.curves.length!==4
        ||!d.curves.every(c=>Array.isArray(c)&&c.length===9&&c.slice(1).every(p=>Array.isArray(p)&&p.length===3&&p.every(Number.isFinite))))return;
    d.hybridStyle=d.hybridStyle??0;d.transfer=d.transfer||[...SYNTH_TRANSFER_DEFAULTS];d.hybridLocks=d.hybridLocks??0x5f98;
    d.type=d.type??0;if(!Number.isInteger(d.type)||d.type<0||d.type>14)return;
    if(Array.isArray(d.drums)&&d.drums.length===12)d.drums=[...d.drums,0,.6,.4,.7];
    for(const [i,key] of ['plucked','sub','hybridParams','trace','events','timing','drumExtra','drumColor'].entries())d[key]=d[key]||[...SYNTH_WORK_DEFAULTS[i]];
    d.workProtected=d.workProtected||Array.from({length:64},(_,i)=>[0,8,9,16,17,24,32,33,40,41,42,43,44,54,55,56,57,58,59].includes(i)?'1':'0').join('');d.inputs=Array.isArray(d.inputs)?d.inputs:[];
    const validArray=(value,count)=>Array.isArray(value)&&value.length===count&&value.every(Number.isFinite);
    for(const [key,specs] of [['osc',SYNTH_WAVE_SPECS],['packets',SYNTH_BLOOM_SPECS],['mod',SYNTH_MOD_SPECS],['spectral',SYNTH_RESYNTH_SPECS],['additive',SYNTH_ADDITIVE_SPECS],['noise',SYNTH_NOISE_SPECS],['fmOsc',SYNTH_FM_SPECS],['conv',SYNTH_CONV_SPECS],['drums',SYNTH_DRUM_SPECS],['plucked',SYNTH_PLUCK_SPECS],['sub',SYNTH_SUB_SPECS],['hybridParams',SYNTH_HYBRID_SPECS],['trace',SYNTH_TRACE_SPECS],['events',SYNTH_EVENT_SPECS],['timing',SYNTH_TIMING_SPECS],['drumExtra',SYNTH_ATTACK_SPECS],['drumColor',SYNTH_COLOR_SPECS]]){
        if(d[key]!==undefined&&!validArray(d[key],specs.length))return;
    }
    if(!validArray(d.transfer,15))return;
    d.wt=d.wt||[...SYNTH_WT_DEFAULTS];d.wtProtected=d.wtProtected||'11'+'0'.repeat(22);if(!validArray(d.wt,24))return;
    d.dustParams=d.dustParams||[...SYNTH_DUST_DEFAULTS];d.dustProtected=d.dustProtected||'100000000000';if(!validArray(d.dustParams,12))return;
    d.osc=d.osc||[0,.5,0,0,20000,0,0];d.packets=d.packets||[0,0,8,150,.2,10,20,120,.2,.2,.2,0,.1,0,0,1];d.mod=d.mod||[0,.5,0,1,0,0,0];d.spectral=d.spectral||[0,0,0,1,0,0,24,-96];
    d.additive=d.additive||[16,0,1,0,0,0,.5,0];d.noise=d.noise||[8,3,0,1,0,0,.5,0];d.fmOsc=d.fmOsc||[0,1,1,2,0,0,0,1];
    d.conv=d.conv||[0,12000,0,1000,0,0,1,1];d.drums=d.drums||[0,48,600,.7,.25,.2,.5,0,12,40,.3,.4,0,.6,.4,.7];d.extendedLocks=d.extendedLocks??0x10f;
    d.partials=d.partials||Array.from({length:8},(_,i)=>[i+1,i===0?1:0]);d.partial_masks=d.partial_masks||[1,1,1,1];d.spectrum=d.spectrum||0;d.component=d.component||0;
    if(!Array.isArray(d.partials)||d.partials.length!==8||!d.partials.every(p=>validArray(p,2))||!validArray(d.partial_masks,4))return;
    const oldType=synthUi.data?.type;
    if(typeof d.wave==='string')d.wave=/^[0-9a-e]{224}$/.test(d.wave)?Array.from({length:112},(_,i)=>[(parseInt(d.wave[i*2],16)-7)/7,(parseInt(d.wave[i*2+1],16)-7)/7]):[];
    if(state.view==='synth'&&synthUi.opened&&d.open===0&&[0,1,2,4,5,6,8,10,12,13,14].includes(d.type)){
        synthUi.opened=false;synthUi.overlay='';state.view='main';refreshEditorSnapshot();if([2,4,10].includes(d.type))readPartials(false);else if(d.type===8)readReson(false);else if([5,12].includes(d.type))readNoise(false);else if([6,14].includes(d.type))readFm(false);else readTable(false);readTableScope(false);showActionMessage([2,4,10].includes(d.type)?'RECIPE TO PARTIALS':d.type===8?'RECIPE TO RESON':[5,12].includes(d.type)?'RECIPE TO NOISE':[6,14].includes(d.type)?'RECIPE TO FM':'RECIPE TO TABLE');
    }
    synthUi.raw=raw;synthUi.data=d;synthUi.row=clamp(d.row|0,0,3);synthUi.point=clamp(d.point|0,0,7);
    if(oldType!==undefined&&oldType!==d.type){synthUi.low=-synthLimits()[synthUi.row]/2;synthUi.high=synthLimits()[synthUi.row]/2;resetParameterEncoderMotion();}
    if(oldType!==d.type)restoreSynthEditor();
    if(!synthPageIds().includes(synthUi.page))synthUi.page=0;
    if(synthUi.overlay!=='name')synthUi.name=String(d.name||'Spline');
    rebuildLedQueue();
}
function synthCommand(key,value=1){if(['type','render','save','restore','close'].includes(key))rememberSynthEditor();setParamBlocking('synth_'+key,value);readSynth();}
function openSynth(){
    endPerformanceLayer();synthCommand('open');
    if(!synthUi.data?.open){showActionMessage(String(getParam('last_error','PROCESS UNAVAILABLE')).slice(0,21));return;}
    restoreSynthEditor();synthUi.opened=true;synthUi.overlay='';synthUi.notice='';synthUi.copy=false;
    state.view='synth';state.actionMessageTicks=0;resetParameterEncoderMotion();rebuildLedQueue();
}
function closeSynth(){
    synthUi.browserTarget='';setParam('preview_stop',1);
    synthCommand('close');synthUi.opened=false;synthUi.copy=false;synthUi.overlay='';
    if(!returnToPagePicker()){
        state.view='pages';state.pageTier='category';state.pageCategory=1;
        state.pageSelection=Math.max(0,categoryPageEntries(1).findIndex(e=>e.view==='synth'));
    }
    resetParameterEncoderMotion();rebuildLedQueue();
}
function synthPadIndex(index){return (3-Math.floor(index/8))*8+index%8;}
function synthNameKeys(){return (synthUi.numbers?'0123456789ABCDEFGHIJKLMNOP':synthUi.lower?'abcdefghijklmnopqrstuvwxyz':'ABCDEFGHIJKLMNOPQRSTUVWXYZ').padEnd(26,' ').slice(0,26).split('').concat(['SP','BK','Aa','09','-','_']);}
function synthPad(index,pressed){
    if(!pressed)return;
    const logical=synthPadIndex(index),row=Math.floor(logical/8),point=logical%8;
    if(synthUi.overlay==='name'){
        const key=synthNameKeys()[logical];
        if(logical===27){if(synthUi.cursor>0){synthUi.name=synthUi.name.slice(0,synthUi.cursor-1)+synthUi.name.slice(synthUi.cursor);--synthUi.cursor;}}
        else if(logical===28)synthUi.lower=!synthUi.lower;
        else if(logical===29)synthUi.numbers=!synthUi.numbers;
        else if(synthUi.name.length<40&&/^[A-Za-z0-9 _-]$/.test(key==='SP'?' ':key)){
            const char=key==='SP'?' ':key;synthUi.name=synthUi.name.slice(0,synthUi.cursor)+char+synthUi.name.slice(synthUi.cursor);++synthUi.cursor;
        }
    }else if(!synthUi.overlay&&synthUi.page===7&&[2,4,5,10].includes(synthType())){synthSpectrumPad(row,point);
    }else if(!synthUi.overlay){
        if(!synthCurveAvailable(row)){synthCommand('select',row+','+point);synthUi.page=2;rebuildLedQueue();return;}
        const from=synthUi.data?.curves?.[row]?.[point+1];if(!from)return;
        if(synthUi.copy){synthUi.clipboard={family:'curve',value:from[1],shape:from[2]};synthUi.notice='POINT COPIED';}
        else if(state.shiftHeld&&synthUi.clipboard?.family==='curve'){
            synthCommand('point',row+','+point);
            synthCommand('point_edit',[row,point,1,synthUi.clipboard.value].join(','));
            synthCommand('point_edit',[row,point,2,synthUi.clipboard.shape].join(','));synthUi.notice='POINT PASTED';
        }else{
            synthCommand(state.deleteHeld?'delete':'point',row+','+point);
            synthUi.page=2;synthUi.notice='';
        }
        resetParameterEncoderMotion();
    }
    rebuildLedQueue();
}
function synthPadColor(index){
    if(synthUi.overlay==='name')return synthPadIndex(index)>=26?BrightOrange:White;
    const logical=synthPadIndex(index),row=Math.floor(logical/8),point=logical%8;
    if(synthUi.page===7&&[2,4,5,10].includes(synthType())){const d=synthUi.data;return row===d.spectrum&&point===d.component?Cyan:(d.partial_masks[row]&(1<<point))&&synthComponentActive(row,point)?[BrightYellow,NeonGreen,Purple,BrightOrange][row]:ScaleNoteGrey;}
    if(row===synthUi.row&&point===synthUi.point)return Cyan;
    if(!synthCurveAvailable(row))return Black;
    return (synthUi.data?.curves?.[row]?.[0]||0)&(1<<point)?[BrightYellow,NeonGreen,Purple,BrightOrange][row]:ScaleNoteGrey;
}
function synthFieldValue(f,v){
    if(f===-1)return SYNTH_TYPES[synthType()];
    if(f===13)return String(Math.round(v));
    if(f===0)return v.toFixed(v<10?2:1)+'s';
    if(f===1)return ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'][Math.round(v)%12]+(Math.floor(v/12)-1);
    if([2,5,10,11].includes(f))return Math.round(v*100)+'%';
    if(f===12)return v>.5?'ON':'OFF';
    if([3,4,9].includes(f))return v>=1000?(v/1000).toFixed(1)+'s':Math.round(v)+'ms';
    if(f===8)return v.toFixed(v<10?1:0)+'st';
    return String(Math.round(v));
}
function drawSynthCell(index,label,value){
    const x=index%4*32+3,y=11+Math.floor(index/4)*24;
    print(x,y,label.slice(0,4),1);print(x,y+11,String(value).slice(0,5),1);
}
function drawSynthList(title,items,selected,footer){
    drawTitleWithFraction(title,(selected+1)+'/'+items.length);drawRect(0,8,128,1,1);
    const start=clamp(selected-1,0,Math.max(0,items.length-4));
    for(let i=0;i<4&&start+i<items.length;++i){const active=start+i===selected,y=12+i*10;
        if(active)drawRect(0,y-1,128,9,1);print(2,y,items[start+i].slice(0,20),active?0:1);
    }
    print(1,57,footer.slice(0,21),1);
}
function synthCurveControlAvailable(index){
    if(!synthCurveAvailable(synthUi.row))return index===7;
    if(index===4)return synthUi.point>0&&synthUi.point<7;
    if(index===6)return synthUi.point<7;
    return index===5||index===7;
}
function drawSynth(){
    rememberSynthEditor();clearDisplay();const d=synthUi.data;if(!d){print(1,0,'PROCESS NOT READY',1);return;}
    if(synthUi.overlay==='nav'){drawSynthList('PROCESS GROUPS',['SOUND','MOTION','OUTPUT','ACTIONS'],synthUi.selection,'CLICK SELECT  BACK');return;}
    if(synthUi.overlay==='actions'){
        const actions=synthActions().map(entry=>entry.title);
        drawSynthList('PROCESS ACTIONS',actions,synthUi.selection,'CLICK SELECT  BACK');return;
    }
    if(synthUi.overlay==='locks'){
        drawSynthList('PROTECT PARAMS',synthLockEntries().map(e=>e.label+(e.on?' LOCKED':' FREE')),synthUi.selection,'CLICK TOGGLE  BACK');return;
    }
    if(synthUi.overlay==='name'){
        drawTitleWithFraction('NAME WAV',synthUi.name.length+'/40');
        print(1,11,synthUi.name.slice(0,20),1);print(1,21,synthUi.name.slice(20,40),1);
        drawRect(1+(Math.min(39,synthUi.cursor)%20)*6,19+Math.floor(Math.min(39,synthUi.cursor)/20)*10,5,1,1);
        const keys=synthNameKeys();for(let i=0;i<32;++i)print(i%8*16+1,33+Math.floor(i/8)*7,keys[i].slice(0,2),1);
        return;
    }
    const pages=synthGroupPages(),titles=['PROCESS SETUP',SYNTH_TITLES[synthType()],'CURVE '+synthRowNames()[synthUi.row],'RANDOM','PREVIEW','PROCESS MOD',synthType()===13?'WT OSC':synthType()===14?'FM SHAPE':synthType()===8?'DRUM DETAIL':synthType()===12?'DUST DETAIL':synthHybridNew()?'HYBRID ROUTE':'BLOOM VAR',(synthType()===2?'SPECTRUM '+String.fromCharCode(65+d.spectrum):(synthType()===10?(d.spectrum<2?'HARM ':'SUBS '):synthType()===4?'PART ':'BANDS ')+(synthType()===10?d.spectrum%2*8+1:d.spectrum*8+1)+'-'+(synthType()===10?d.spectrum%2*8+8:d.spectrum*8+8)),'CONV FILES','PROCESS EVENTS','RSYN TRACE','HYBRID FILES','EVENT VAR',synthType()===13?'WT SHAPE':synthType()===14?'FM ATTACK':'DRUM ATTACK',synthType()===14?'FM COLOR':'DRUM COLOR'];
    drawTitleWithFraction(titles[synthUi.page],(pages.indexOf(synthUi.page)+1)+'/'+pages.length);
    drawRect(0,8,128,1,1);
    if(synthUi.page===11){for(let i=0;i<4;++i){print(1,12+i*11,String.fromCharCode(65+i),1);print(13,12+i*11,String(d.inputs[i]||'NOT SELECTED').slice(0,19),1);}print(1,57,'CLICK: PICK FILES',1);return;}
    if(synthUi.page===8){print(1,11,'PRIMARY',1);print(1,22,String(d.primary||'NOT SELECTED').slice(0,21),1);print(1,34,'IMPULSE',1);print(1,45,String(d.impulse||'NOT SELECTED').slice(0,21),1);print(1,57,'CLICK: PICK FILES',1);return;}
    if(synthUi.page===0){
        [-1,0,1,2,3,4,5,13].forEach((f,i)=>{if(!synthSetupAvailable(f))drawReservedParameterCell(i,true);else drawSynthCell(i,f===-1?'TYPE':f===13?'SEED':f===0&&synthType()===7?'MAX':SYNTH_FIELDS[f],synthFieldValue(f,f===13?d.seed:d.values[f]));});
    }else if(synthUi.page===14){
        synthColorCells().forEach((cell,i)=>{if(!cell||!synthDetailAvailable(...cell))drawReservedParameterCell(i,true);else drawSynthCell(i,synthSpecs(cell[0])[cell[1]][0],synthDetailValue(...cell));});
    }else if([9,12].includes(synthUi.page)){
        synthEventCells().forEach((cell,i)=>{if(!cell||!synthDetailAvailable(...cell))drawReservedParameterCell(i,true);else drawSynthCell(i,synthSpecs(cell[0])[cell[1]][0],synthDetailValue(...cell));});
    }else if(synthType()===14&&[1,6,13].includes(synthUi.page)){
        synthFmDrumCells().forEach((cell,i)=>{if(!cell||!synthDetailAvailable(...cell))drawReservedParameterCell(i,true);else drawSynthCell(i,synthSpecs(cell[0])[cell[1]][0],synthDetailValue(...cell));});
    }else if(synthUi.page===1||synthUi.page===6||synthUi.page===10||synthUi.page===13){
        if(synthType()===0){[6,7,8,9,10,11,null,null].forEach((f,i)=>f===null?drawReservedParameterCell(i,true):drawSynthCell(i,SYNTH_FIELDS[f],synthFieldValue(f,d.values[f])));}
        else{const family=synthWorkshopFamily(),specs=synthSpecs(family),offset=synthUi.page===6?8:synthUi.page===13&&synthType()===13?16:0;
            for(let i=0;i<8;++i){const field=offset+i;if((synthType()===8&&((synthUi.page===1&&field===7)||(synthUi.page===13&&[4,5].includes(field))))||field>=specs.length||!synthDetailAvailable(family,field))drawReservedParameterCell(i,true);else drawSynthCell(i,specs[field][0],synthDetailValue(family,field));}}
    }else if(synthUi.page===5){
        for(let i=0;i<7;++i){if(!synthDetailAvailable('mod',i))drawReservedParameterCell(i,true);else drawSynthCell(i,i===1&&d.mod[6]?'CYCL':SYNTH_MOD_SPECS[i][0],synthDetailValue('mod',i));}drawSynthCell(7,'LOOP',synthFieldValue(12,d.values[12]));
    }else if(synthUi.page===7){drawSynthSpectrum();return;
    }else if(synthUi.page===2){
        const c=d.curves[synthUi.row],limit=synthLimits()[synthUi.row];
        drawPixelLine(3,23,124,23);let last=null;
        for(let i=0;i<8;++i)if(c[0]&(1<<i)){
            const p=c[i+1],xy=[3+p[0]*121,23-p[1]/limit*11];
            if(last){const shape=last.shape;for(let k=1;k<=24;++k){let t=k/24,q=shape===0?(k===24?1:0):shape===2?t*t*(3-2*t):t;
                drawPixelLine(last.x+(xy[0]-last.x)*(k-1)/24,last.y+(xy[1]-last.y)*last.q,last.x+(xy[0]-last.x)*t,last.y+(xy[1]-last.y)*q);last.q=q;}}
            drawRect(Math.round(xy[0])-1,Math.round(xy[1])-1,i===synthUi.point?4:2,i===synthUi.point?4:2,1);
            last={x:xy[0],y:xy[1],shape:p[2],q:0};
        }
        const p=c[synthUi.point+1],labels=['TIME','VALUE','SHAP','PT'],values=[p[0]*synthDuration()<1?Math.round(p[0]*synthDuration()*1000)+'ms':(p[0]*synthDuration()).toFixed(2)+'s',synthUi.row===1&&synthType()===1||[1,2].includes(synthUi.row)&&synthType()===3?Math.round(p[1]*100)+'%':p[1].toFixed(1),['HOLD','LIN','SMTH'][p[2]],(synthUi.point+1)+'/8'];
        if(!(c[0]&(1<<synthUi.point))){
            const x=Math.round(3+p[0]*121)-1,y=Math.round(23-p[1]/limit*11)-1;
            drawRect(x,y,4,4,1);drawRect(x+1,y+1,2,2,0);
        }
        for(let i=0;i<4;++i){
            if(!synthCurveControlAvailable(i+4)){drawReservedParameterCell(i+4);continue;}
            print(i*32+3,40,labels[i],1);print(i*32+3,51,values[i].slice(0,5),1);
        }
        return;
    }else if(synthUi.page===3){
        [['AMT',Math.round(synthUi.amount*100)+'%'],['ROW',synthRowLabel(synthUi.row)],['MIN',synthUi.low.toFixed(1)],['MAX',synthUi.high.toFixed(1)],['ENDS',synthUi.keepEnds?'KEEP':'FREE']].forEach(([l,v],i)=>drawSynthCell(i,l,v));
        for(let i=5;i<8;++i)drawReservedParameterCell(i,true);
    }else{
        print(1,11,synthUi.name.slice(0,20),1);
        if(synthType()===7)print(1,20,(d.seconds?d.seconds.toFixed(2)+'s':'PICK TWO FILES'),1);
        if(synthType()===3)print(1,20,String(d.source||'USE LANE SOURCE').slice(0,20),1);
        for(let i=0;i<(d.wave?.length||0);++i){const p=d.wave[i],height=synthType()===3||synthType()===7?8:11,center=synthType()===3||synthType()===7?37:35;drawRect(i+8,Math.round(center-p[1]*height),1,Math.max(1,Math.round((p[1]-p[0])*height)),1);}
        print(1,47,synthType()===3&&!d.source?'CHOOSE SOURCE':d.ready?'READY  LANE '+state.lane:(d.job===1||d.job===2)?'RENDERING '+d.progress+'%':d.wave?.length?'LAST RENDER / EDITED':'CAPTURE TO RENDER',1);
    }
    const busy=d.job===1||d.job===2;
    const fit=[9,12].includes(synthUi.page)&&synthEventsActive()&&d.message==='READY'?'FIT '+(d.eventfit??'?')+'/'+Math.round(d.events[1]):'';
    const status=(busy?(d.op===1?'SAVING':d.op===2?'OPENING RECIPE':'RENDER')+' '+d.progress+'%':synthUi.notice||fit||d.message||'CLICK: ACTIONS');
    print(1,57,status.slice(0,21),1);
}
function adjustSynth(index,delta){
    const d=synthUi.data;if(!d||synthUi.overlay)return;
    if(synthUi.page===2&&!synthCurveControlAvailable(index))return;
    const motionKey=synthUi.page===2&&index===7?'synth_point_'+synthUi.row
        :'synth_'+synthUi.page+'_'+synthUi.row+'_'+synthUi.point+'_'+index;
    delta=parameterEncoderDelta(index,motionKey,delta);if(!delta)return;synthUi.notice='';
    if(synthUi.page===8||synthUi.page===11)return;
    if(synthUi.page===0){
        if(index===0){
            const position=SYNTH_TYPE_ORDER.indexOf(synthType());
            synthCommand('type',SYNTH_TYPE_ORDER[clamp(position+Math.sign(delta),0,SYNTH_TYPE_ORDER.length-1)]);return;
        }
        const f=[-1,0,1,2,3,4,5,13][index];if(!synthSetupAvailable(f))return;
        const steps=[.02,1,.01,1,1,.01,1,0,.1,1,.01,.01,1,1];
        const value=[3,4].includes(f)?Math.max(0,(d.values[f]+1)*Math.pow(2,delta/12)-1):(f===13?d.seed:d.values[f])+steps[f]*delta;synthCommand('edit',f+','+value);
    }else if(synthUi.page===14){
        const cell=synthColorCells()[index];if(cell)adjustSynthDetail(cell[0],cell[1],delta);
    }else if([9,12].includes(synthUi.page)){
        const cell=synthEventCells()[index];if(cell)adjustSynthDetail(cell[0],cell[1],delta);
    }else if(synthType()===14&&[1,6,13].includes(synthUi.page)){
        const cell=synthFmDrumCells()[index];if(cell)adjustSynthDetail(cell[0],cell[1],delta);
    }else if(synthUi.page===1||synthUi.page===6||synthUi.page===10||synthUi.page===13){
        if(synthType()===0){if(index>=6)return;const f=6+index,steps=[1,0,.1,1,.01,.01];const value=f===7?d.values[f]*Math.pow(2,delta/8):f===9?Math.max(1,(d.values[f]+1)*Math.pow(2,delta/12)-1):d.values[f]+steps[index]*delta;synthCommand('edit',f+','+value);}
        else{if(synthType()===8&&((synthUi.page===1&&index===7)||(synthUi.page===13&&[4,5].includes(index))))return;const family=synthWorkshopFamily(),field=index+(synthUi.page===6?8:synthUi.page===13&&synthType()===13?16:0);if(field<synthSpecs(family).length)adjustSynthDetail(family,field,delta);}
    }else if(synthUi.page===5){if(index===7)synthCommand('edit','12,'+clamp(d.values[12]+Math.sign(delta),0,1));else adjustSynthDetail('mod',index,delta);
    }else if(synthUi.page===7){
        if(index===7){const current=d.component+(synthType()===2?0:d.spectrum*8),component=clamp(current+Math.sign(delta),0,synthType()===2?7:31);if(component!==current)synthCommand('spec_select',(synthType()===2?d.spectrum:Math.floor(component/8))+','+(component%8));}
        else if(index===4||index===5){const field=index-4,p=d.partials[d.component],step=field===0?(state.shiftHeld?.01:.1):.01;synthCommand('spec_edit',[d.spectrum,d.component,field,p[field]+delta*step].join(','));}
    }else if(synthUi.page===2&&index===7){
        const point=clamp(synthUi.point+Math.sign(delta),0,7);
        if(point!==synthUi.point)synthCommand('select',synthUi.row+','+point);
    }else if(synthUi.page===2&&index>=4&&index<7){
        const p=d.curves[synthUi.row][synthUi.point+1],field=index-4;
        synthCommand('point_edit',[synthUi.row,synthUi.point,field,p[field]+delta*([.005,synthUi.row===0?1:synthLimits()[synthUi.row]<2?.01:.1,1][field])].join(','));
    }else if(synthUi.page===3){
        const limit=synthLimits()[synthUi.row];
        if(index===0)synthUi.amount=clamp(synthUi.amount+delta*.01,0,1);
        if(index===1){const row=clamp(synthUi.row+delta,0,3);synthCommand('select',row+','+synthUi.point);synthUi.low=-synthLimits()[row]/2;synthUi.high=synthLimits()[row]/2;}
        if(index===2)synthUi.low=clamp(synthUi.low+delta*.5,-limit,synthUi.high);
        if(index===3)synthUi.high=clamp(synthUi.high+delta*.5,synthUi.low,limit);
        if(index===4)synthUi.keepEnds=delta<0;
    }
}
function synthAction(){
    const action=synthActions()[synthUi.selection]?.id;synthUi.overlay='';synthUi.notice='';
    if(action===0)synthCommand('render');
    if(action===1)synthCommand('preview',synthUi.data?.preview?0:1);
    if(action===2){synthUi.overlay='name';synthUi.nameBefore=synthUi.name;synthUi.cursor=synthUi.name.length;}
    if(action===3||action===4){synthCommand('save',action===4?1:0);synthUi.notice=String(getParam('last_error',''));}
    if(action===5)synthCommand('restore');
    if(action===6)synthCommand('seed_next');
    if(action===7)synthCommand('mutate',synthUi.amount);
    if(action===8)synthCommand('curve_random',[synthUi.row,synthUi.low,synthUi.high,synthUi.amount,synthUi.keepEnds?1:0].join(','));
    if(action===9){synthUi.overlay='locks';synthUi.selection=0;}
    if(action===10)synthCommand('cancel');
    if(action===11)synthCommand('clean');
    if(action===24)synthCommand('drum_start');
    if(action===25)synthCommand('wt_start');
    if(action===26)synthCommand('hybrid_upgrade');
    if(action>=20&&action<=23){openSynthBrowser('hybrid'+(action-20));return;}
    if(action===13||action===14){openSynthBrowser(action===13?'primary':'impulse');return;}
    if(action===12){synthCommand('source');synthUi.notice=String(getParam('last_error',''));}
    rebuildLedQueue();
}
function handleSynthMidi(status,number,value){
    if(state.view==='library'&&synthUi.browserTarget){
        if([0x80,0x90,0xa0].includes(status)&&MovePads.includes(number))return true;
        if(status!==0xb0)return false;
        if(number===MoveRecord||number===MoveRecordAlt||MoveTracks.includes(number))return true;
        if(number===MoveMenu&&value>0){returnSynthBrowser();return true;}
        if(number===MoveBack&&value>0&&state.libraryDir===LIBRARY_HOME){returnSynthBrowser();return true;}
        return false;
    }
    if(state.view!=='synth')return false;
    if(state.focusButtonHeld&&number!==MoveCapture&&((status===0xb0&&value>0)||(status===0x90&&value>0)))state.focusButtonUsed=true;
    if(status===0x90||status===0x80){const pad=MovePads.indexOf(number);if(pad>=0){synthPad(pad,status===0x90&&value>0);return true;}}
    if(status===0xa0&&MovePads.includes(number))return true;
    if(status!==0xb0)return false;
    if(number===MoveShift){state.shiftHeld=value>0;setParam('shift_held',state.shiftHeld?1:0);return true;}
    if(number===MoveDelete){state.deleteHeld=value>0;setParam('delete_held',state.deleteHeld?1:0);return true;}
    if(number===MoveCopy){synthUi.copy=value>0;return true;}
    if(number===MoveCapture){
        if(value>0){state.focusButtonHeld=true;state.focusButtonUsed=Boolean(synthUi.overlay||state.shiftHeld||state.deleteHeld||synthUi.copy);}
        else{const render=state.focusButtonHeld&&!state.focusButtonUsed&&!synthUi.overlay;state.focusButtonHeld=false;state.focusButtonUsed=true;if(render){synthUi.notice='';synthCommand('render');}}
        rebuildIconLedQueue();return true;
    }
    if(number===MoveRecord||number===MoveRecordAlt){if(value>0){if(synthUi.overlay==='name'){synthCommand('name',synthUi.name);synthUi.overlay='';}synthCommand('save',state.shiftHeld?0:1);synthUi.notice=String(getParam('last_error',''));}return true;}
    const track=MoveTracks.indexOf(number);if(track>=0){if(value>0){if(state.focusButtonHeld){closeSynth();openPageCategory(track);return true;}state.lane=track+1;setParamBlocking('record_lane',state.lane);synthUi.notice='LOAD TO LANE '+state.lane;}return true;}
    if(number>=MoveKnob1&&number<MoveKnob1+8){adjustSynth(number-MoveKnob1,decodeDelta(value));return true;}
    if(number===MoveMainKnob){
        const delta=decodeDelta(value);
        if(synthUi.overlay==='name')synthUi.cursor=clamp(synthUi.cursor+delta,0,synthUi.name.length);
        else if(synthUi.overlay)synthUi.selection=clamp(synthUi.selection+delta,0,synthUi.overlay==='locks'?synthLockEntries().length-1:synthUi.overlay==='nav'?3:synthActions().length-1);
        else moveSynthPage(delta);return true;
    }
    if(value<=0)return [MoveBack,MoveMenu,MoveMainButton,MoveUp,MoveDown,MoveLeft,MoveRight,MoveLoop,MoveUndo,MoveMute].includes(number);
    if(number===MoveMainButton){
        if(synthUi.overlay==='nav')selectSynthGroup();
        else if(synthUi.overlay==='actions')synthAction();
        else if(synthUi.overlay==='locks'){const entry=synthLockEntries()[synthUi.selection];if(entry)synthCommand(entry.family,entry.field);}
        else if(synthUi.overlay==='name'){synthCommand('name',synthUi.name);synthUi.overlay='';}
        else{synthUi.overlay='actions';synthUi.selection=0;}
        rebuildLedQueue();return true;
    }
    if(number===MoveBack||number===MoveMenu){
        if(number===MoveMenu&&state.focusButtonHeld){closeSynth();openPagePicker();return true;}
        if(synthUi.overlay==='nav'){if(number===MoveBack)closeSynth();else synthUi.overlay='';}
        else if(synthUi.overlay){if(synthUi.overlay==='name')synthUi.name=synthUi.nameBefore;synthUi.overlay='';}
        else synthNav();rebuildLedQueue();return true;
    }
    if(number===MoveUp||number===MoveDown||number===MoveLeft||number===MoveRight){
        const delta=number===MoveUp||number===MoveLeft?-1:1;
        if(['actions','locks','nav'].includes(synthUi.overlay))synthUi.selection=clamp(synthUi.selection+delta,0,synthUi.overlay==='locks'?synthLockEntries().length-1:synthUi.overlay==='nav'?3:synthActions().length-1);
        else if(!synthUi.overlay)moveSynthPage(delta);return true;
    }
    if(number===MoveLoop){
        if(state.shiftHeld){if(!synthUi.overlay){if(synthUi.amount>0){synthCommand('mutate',synthUi.amount);synthCommand('render');synthUi.notice='RANDOM + RENDER';}else synthUi.notice='RANDOM AMT IS ZERO';}}
        else synthCommand('preview',synthUi.data?.preview?0:1);return true;
    }
    if(number===MoveUndo||number===MoveMute)return true;
    return false; // Play and bottom clip launchers retain their normal roles.
}

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
const E16_LABELS={"0":"POS ","1":"FLD ","2":"SPD ","3":"STK ","4":"ENG ","5":"JOIN","6":"LVL ","133":"VARI","384":"SCAN","385":"MODE","386":"PATH","388":"BLND","389":"SHAP","390":"OFST","391":"SKEW","392":"CURV","393":"SLEW","394":"STR ","395":"RATI","512":"MODE","513":"SND ","514":"RATE","515":"BAS ","516":"PATH","517":"STEP","518":"SEED","519":"TRVL","520":"JITR","521":"DUTY","522":"MRAT","523":"MSYM","524":"MSHP","525":"TRIG","526":"OVLP","527":"LAYR","528":"ERAT","529":"REPT","530":"ESTP","531":"PITC","532":"LEVL","533":"CURV","640":"MODE","641":"SIZE","642":"DENS","643":"QUAL","644":"PIT ","645":"WIN ","646":"SPRD","647":"SPRY","648":"BIAS","649":"PSPR","650":"REV%","651":"SVAR","652":"LVAR","653":"TIME","654":"TSC ","655":"ESKW","656":"PROC","657":"PAMT","658":"REGN","659":"ADVN","768":"MODE","769":"LEN ","770":"LAYR","771":"PIT ","772":"SHAP","773":"EDGE","774":"SPRD","775":"REV%","776":"SPRY","777":"BIAS","778":"DVAR","779":"PSPR","780":"LVAR","781":"PHAS","782":"ADVN","896":"ADV ","897":"MODE","898":"CLCK","899":"DIV ","900":"RATE","901":"TVAR","902":"GATE","903":"JOIN","904":"REGN","905":"REPT","906":"REV%","907":"PVAR","908":"LVAR","909":"LORD","910":"POLY","911":"RSRC","1030":"PLAY","1031":"SSRC","1032":"SLIC","1152":"MODE","1153":"CLCK","1154":"DIV ","1155":"RATE","1156":"BUFF","1157":"REPT","1158":"CHNC","1159":"OVRD","1160":"STEP","1161":"PULS","1162":"ROTA","1163":"JUMP","1164":"PITC","1165":"REV%","1166":"DCAY","1167":"SEED","1280":"GRUP","1281":"REPT","1282":"STRD","1283":"ADVN","1284":"ORDR","1285":"PROC","1286":"DPTH","1287":"DTAL","1408":"MODE","1409":"BLUR","1410":"DCAY","1411":"PIT ","1412":"TILT","1413":"SMER","1536":"MODE","1537":"AMP ","1538":"FREQ","1539":"OFST","1540":"BLUR","1541":"PIT ","1664":"MODE","1665":"RATE","1666":"LEN ","1667":"SCAT","1668":"RNGE","1669":"GRUP","1670":"SPRD","1671":"SEED","1792":"PIT ","1793":"FORM","1794":"SMTH","1795":"TUNE","1796":"FRZE","1797":"KEYT","1798":"ROUG","1799":"SUB ","1921":"TONE","1922":"DRIV","1923":"BITS","1924":"RATE","1925":"DLY ","1926":"FDBK","1927":"WDTH","1928":"MIX ","1929":"X-PN","2048":"RATE","2049":"DPTH","2050":"MIX ","2051":"RAMP","2052":"BAL ","2053":"XOVR","2054":"SPRD","2176":"FMT ","2177":"REC ","2182":"HISS","2183":"MIX ","2184":"AGE ","2185":"CRNK","2186":"MECH","2187":"INRT","2304":"ALGO","2305":"DECY","2306":"PRE ","2307":"DAMP","2308":"DIFF","2309":"MOD ","2310":"WDTH","2311":"MIX ","2432":"OUT ","2438":"BLND","2441":"ADLY","2442":"BDLY","2443":"AREV","2444":"BREV","2445":"CURV","2448":"CDLY","2449":"DDLY","2450":"CREV","2451":"DREV","2453":"PRIO","2454":"THR ","3200":"SIZE","3201":"AMUT","3202":"TMUT","3203":"EVOL","3204":"MEM ","3205":"PULL","3206":"LAW ","3207":"DRFT","3210":"CAPT","3328":"RATE","3329":"FORM","3330":"SIZE","3331":"WIN ","3332":"SKEW","3333":"OMIT","3334":"BRST","3335":"JITT","3338":"ERAT","3339":"EDEP","3340":"FRAT","3341":"FDEP","3342":"SPRY","3343":"FVAR","3344":"REV%","3345":"KEYT","4200":"TYPE","4201":"TUNE","4202":"WID ","4203":"LVL ","4204":"ART ","4205":"DEC ","4206":"NOIS","4207":"DRIV","4208":"SHAP","4209":"PW  ","4210":"DET ","4211":"SUB ","4212":"CUT ","4213":"RES ","4214":"PTS ","4215":"EVOL","4216":"CURV","4217":"DRFT","4218":"RATE","4219":"DYN ","4220":"SEED","4221":"MODL","4222":"BNKA","4223":"POSA","4224":"BNKB","4225":"POSB","4226":"BLND","4227":"RATI","4228":"FM  ","4229":"DET ","4230":"BEND","4231":"PDEC","4232":"SCAN","4233":"SDEC","4234":"SUB ","4235":"IMPT","4236":"NDEC","4237":"COLR","4238":"TONE","4239":"COMP","4300":"EXC ","4301":"PICK","4302":"HIT ","4303":"TUNE","4304":"MOTN","4305":"DAMP","4306":"BRGT","4307":"WID ","4308":"MASS","4309":"TENS","4310":"CENT","4311":"PROF","4312":"FORC","4313":"PRES","4314":"FRZ ","4315":"SEED","4316":"ART ","4317":"IMPT","4318":"HARD","4319":"DEC ","4320":"BEND","4321":"TIME","4322":"LOSS","4323":"AMP ","4324":"GAIN","4325":"BASS","4326":"MID ","4327":"FREQ","4328":"TREB","4329":"CAB ","4330":"OUT ","4400":"MODE","4401":"TUNE","4402":"WID ","4403":"LVL ","4404":"ALG ","4405":"RAT1","4406":"IDX1","4407":"FDBK","4408":"RAT2","4409":"IDX2","4410":"DETN","4411":"MIX ","4412":"CUTO","4413":"RES ","4414":"DRIV","4415":"DEC ","4416":"FDEC","4417":"BEND","4418":"TIME","4419":"HIT ","4420":"NOIS","4421":"NDEC","4422":"HARD","4423":"HARM","4424":"ATK1","4425":"DEC1","4426":"END1","4427":"CRV1","4428":"ATK2","4429":"DEC2","4430":"END2","4431":"CRV2","4500":"TYPE","4501":"TUNE","4502":"WID ","4503":"LVL ","4504":"COLR","4505":"TONE","4506":"BITS","4507":"SEED","4508":"BAND","4509":"BW  ","4510":"TILT","4511":"STR ","4512":"MOVE","4513":"RATE","4514":"LEAK","4515":"GRID","4516":"RATE","4517":"SIZE","4518":"CLMP","4519":"EDGE","4520":"VAR ","4521":"BED ","4522":"REPT","4523":"FAIL","4524":"R1  ","4525":"R2  ","4526":"R3  ","4527":"R4  ","4528":"R5  ","4529":"R6  ","4530":"R7  ","4531":"R8  ","4532":"R9  ","4533":"R10 ","4534":"R11 ","4535":"R12 ","4536":"R13 ","4537":"R14 ","4538":"R15 ","4539":"R16 ","4540":"R17 ","4541":"R18 ","4542":"R19 ","4543":"R20 ","4544":"R21 ","4545":"R22 ","4546":"R23 ","4547":"R24 ","4548":"R25 ","4549":"R26 ","4550":"R27 ","4551":"R28 ","4552":"R29 ","4553":"R30 ","4554":"R31 ","4555":"R32 ","4556":"L1  ","4557":"L2  ","4558":"L3  ","4559":"L4  ","4560":"L5  ","4561":"L6  ","4562":"L7  ","4563":"L8  ","4564":"L9  ","4565":"L10 ","4566":"L11 ","4567":"L12 ","4568":"L13 ","4569":"L14 ","4570":"L15 ","4571":"L16 ","4572":"L17 ","4573":"L18 ","4574":"L19 ","4575":"L20 ","4576":"L21 ","4577":"L22 ","4578":"L23 ","4579":"L24 ","4580":"L25 ","4581":"L26 ","4582":"L27 ","4583":"L28 ","4584":"L29 ","4585":"L30 ","4586":"L31 ","4587":"L32 ","4600":"MODE","4601":"TUNE","4602":"WID ","4603":"LVL ","4604":"BODY","4605":"DEC ","4606":"BRGT","4607":"DAMP","4608":"STRC","4609":"INHR","4610":"MASS","4611":"BEND","4612":"TIME","4613":"NOIS","4614":"NDEC","4615":"COLR","4616":"HIT ","4617":"HARD","4618":"HDEC","4619":"PNCH","4620":"SPOT","4621":"COUP","4622":"DETN","4623":"SEED","4624":"DRIV","4625":"CLIP","4626":"BIAS","4627":"SAG ","4628":"REC ","4629":"TONE","4630":"MIX ","4631":"OUT ","4800":"TYPE","4801":"TUNE","4802":"WID ","4803":"LVL ","4804":"PART","4805":"TILT","4806":"STRC","4807":"PHAS","4808":"HARM","4809":"SUBS","4810":"BAL ","4811":"DETN","4812":"DRFT","4813":"RATE","4814":"DAMP","4815":"TIME","4816":"GAP ","4817":"SCAT","4818":"RISE","4819":"SUS ","4820":"DEC ","4821":"VAR ","4822":"PJIT","4823":"SHAP","4824":"R1  ","4825":"R2  ","4826":"R3  ","4827":"R4  ","4828":"R5  ","4829":"R6  ","4830":"R7  ","4831":"R8  ","4832":"R9  ","4833":"R10 ","4834":"R11 ","4835":"R12 ","4836":"R13 ","4837":"R14 ","4838":"R15 ","4839":"R16 ","4840":"R17 ","4841":"R18 ","4842":"R19 ","4843":"R20 ","4844":"R21 ","4845":"R22 ","4846":"R23 ","4847":"R24 ","4848":"R25 ","4849":"R26 ","4850":"R27 ","4851":"R28 ","4852":"R29 ","4853":"R30 ","4854":"R31 ","4855":"R32 ","4856":"L1  ","4857":"L2  ","4858":"L3  ","4859":"L4  ","4860":"L5  ","4861":"L6  ","4862":"L7  ","4863":"L8  ","4864":"L9  ","4865":"L10 ","4866":"L11 ","4867":"L12 ","4868":"L13 ","4869":"L14 ","4870":"L15 ","4871":"L16 ","4872":"L17 ","4873":"L18 ","4874":"L19 ","4875":"L20 ","4876":"L21 ","4877":"L22 ","4878":"L23 ","4879":"L24 ","4880":"L25 ","4881":"L26 ","4882":"L27 ","4883":"L28 ","4884":"L29 ","4885":"L30 ","4886":"L31 ","4887":"L32 ","4888":"D1  ","4889":"D2  ","4890":"D3  ","4891":"D4  ","4892":"D5  ","4893":"D6  ","4894":"D7  ","4895":"D8  ","4896":"D9  ","4897":"D10 ","4898":"D11 ","4899":"D12 ","4900":"D13 ","4901":"D14 ","4902":"D15 ","4903":"D16 ","5000":"BODY","5001":"TUNE","5002":"WID ","5003":"LVL ","5004":"FWAV","5005":"FOCT","5006":"FND ","5007":"BODL","5008":"WAVE","5009":"PRES","5010":"PW  ","5011":"CROS","5012":"DRIV","5013":"BDEC","5014":"TYPE","5015":"LVL ","5016":"COLR","5017":"TRK ","5018":"TWID","5019":"FILT","5020":"CUT ","5021":"RES ","5022":"FDRV","5023":"KEY ","5024":"FENV","5025":"FDEC","5026":"PUNC","5027":"PTIM","5028":"DRIV","5029":"LOW ","5030":"TDEC","5031":"TATK","5090":"STAR"};
const E16_ROW_TITLES=["ENGINE","STACK PATH","MOTION PATH","MOTION SOUND","LANES PLAY","GRAINS MAIN","GRAINS CLOUD","GRAINS PROCESS","CUTUPS CLOCK","CUTUPS PATH","DIRECT PLAY","DIRECT PLAY","SLICE SOURCE","GLITCH CLOCK","GLITCH PATTERN","WEAVE","WEAVE MOTION","WAVESETS","PRISM","MORPH","TEXTURE","FORMANT","STOCHASTIC","PULSAR","PULSAR MOD","TABLE","TABLE WAVE","TABLE SPLINE","TABLE WT OSC","TABLE WT HIT","TABLE WT COLOR","SCANNED","SCAN BODY","SCAN HIT","SCAN AMP","FM","FM COLOR","FM HIT","FM ENVS","NOISE","NOISE BANK","NOISE EVENTS","NOISE BANDS","RESON","RESON BODY","RESON HIT","RESON DRIVE","PARTIALS","PARTIALS TONE","PARTIALS PACKET","PARTIALS BANDS","LOWFORM","LOWFORM BODY","LOWFORM SHAPE","LOWFORM TEXTURE","PACKETS","MOTOR","EVENT CLOCK","EVENT SHAPE","PART COLOR","STEREO ROTARY","FX SENDS","FX SENDS","DELAY RETURN","REVERB","TAPE"];
const E16_PAGES=["ENGINE","STACK","ENG 1","ENG 2","ENG 3","ENG 4","SENDS","COLOR","ROTARY","DELAY","REVERB","TAPE"];
const E16_ENGINES=["MOTN","LANS","GRNS","CUTS","SAMP","SLIC","GLCH","WEAV","WSET","PRSM","MORP","TEXT","FORM","STOC","PLSR","TABL","SCAN","FM","NOIS","RESON","PART","LOWF"];
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
    readPlaybackReadout(true);
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
    if(['fm_end1','fm_end2'].includes(p.key)&&value<0)return 'AUTO';
    if(['fm_decay1','fm_decay2'].includes(p.key)&&value===0)return 'LINK';
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
