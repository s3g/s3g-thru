/* s3g Thru 0.28.0-test.13 — polyphonic sampler/performance-looper takeover for Move. */

import * as moveConstants from '/data/UserData/schwung/shared/constants.mjs';
import * as moveInputFilter from '/data/UserData/schwung/shared/input_filter.mjs';
import * as os from 'os';

function constant(name, fallback) {
    const value = moveConstants[name];
    return Number.isFinite(value) ? value : fallback;
}

const MoveMainKnob = constant('MoveMainKnob', 14);
const MoveMainButton = constant('MoveMainButton', 3);
const MoveShift = constant('MoveShift', 49);
const MoveMenu = constant('MoveMenu', 50);
const MoveBack = constant('MoveBack', 51);
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

const UI_VERSION = '0.28.0-test.13';
const WAVE_BINS = 112;
const ALGORITHMS = ['MOTION', 'LANES', 'GRAINS', 'CUTUPS', 'SAMPLE', 'SLICES',
    'GLITCH', 'WEAVE', 'WAVESETS'];
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
const PARAMETER_ENCODER_RESPONSE = 0.5;
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
    split_mode: {key:'split_mode',label:'MODE',min:0,max:1,step:1,value:0,integer:true,labels:['SINGLE','SPLIT']},
    split_focus: {key:'split_focus',label:'EDIT PART',min:0,max:1,step:1,value:0,integer:true,labels:['A','B']},
    split_copy: {key:'split_copy',label:'COPY OTHER',min:0,max:1,step:1,value:0,integer:true,labels:['TURN','COPY'],momentary:true,result:'COPIED OTHER PART'},
    part_mute: {key:'part_mute',label:'MUTE',min:0,max:1,step:1,value:0,integer:true,labels:['OFF','ON']},
    split_blend: {key:'split_blend',label:'A B BLEND',min:0,max:1,step:.01,value:.5},
    split_level_a: {key:'split_level_a',label:'A LEVEL',min:0,max:2,step:.02,value:1},
    split_level_b: {key:'split_level_b',label:'B LEVEL',min:0,max:2,step:.02,value:1},
    split_delay_a: {key:'split_delay_a',label:'A DELAY',min:0,max:1,step:.01,value:0},
    split_delay_b: {key:'split_delay_b',label:'B DELAY',min:0,max:1,step:.01,value:0},
    split_reverb_a: {key:'split_reverb_a',label:'A REVERB',min:0,max:1,step:.01,value:0},
    split_reverb_b: {key:'split_reverb_b',label:'B REVERB',min:0,max:1,step:.01,value:0},
    split_curve: {key:'split_curve',label:'CURVE',min:0,max:1,step:1,value:1,integer:true,labels:['LINEAR','POWER']},
    locus: { key: 'locus', label: 'POSITION', min: 0, max: 1, step: 0.01, value: 0.5 },
    field: { key: 'field', label: 'FIELD', min: 0.02, max: 1, step: 0.01, value: 1 },
    speed: { key: 'speed', label: 'SPEED', min: 0.05, max: 4, step: 0.02, value: 1 },
    manual_lane: { key: 'manual_lane', label: 'STACK START', min: 1, max: 4, step: 0.04, value: 1 },
    cut_lane_advance: { key: 'cut_lane_advance', label: 'ADVANCE', min: 0, max: 6, step: 1, value: 1, integer: true, labels: ['HOLD', '1', '2', '3', '4', '8', '16'] },
    lane_cycles: { key: 'lane_cycles', label: 'SCAN RATE', min: 0.125, max: 16, step: 0.125, value: 1 },
    algorithm: { key: 'algorithm', label: 'ALGORITHM', min: 0, max: 8, step: 1, value: 0, integer: true, labels: ALGORITHMS },
    join: { key: 'join', label: 'JOIN', min: 0, max: 1, step: 0.01, value: 1 },
    output_gain: { key: 'output_gain', label: 'LEVEL', min: 0, max: 2, step: 0.02, value: 1 },
    polyphony_comp: { key: 'polyphony_comp', label: 'POLY COMP', min: 0, max: 1, step: 1, value: 1, integer: true, labels: ['OFF', 'ON'] },
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
    lane_path: { key: 'lane_path', label: 'PATH', min: 0, max: 7, step: 1, value: 2, integer: true, labels: ['DOWN', 'UP', 'TRI', 'SINE', 'STEP DN', 'STEP UP', 'MANUAL', 'RANDOM'] },
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
    slice_region_mode: { key: 'slice_region_mode', label: 'SLICE SOURCE', min: 0, max: 1, step: 1, value: 1, integer: true, labels: ['EQUAL', 'TRANSIENTS'] },
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
    preview_level: { key: 'preview_level', label: 'PREVIEW LEVEL', min: 0, max: 1, step: 0.01, value: 0.7 },
    lane_1_gain: { key: 'lane_1_gain', label: 'LANE 1', min: 0, max: 2, step: 0.02, value: 1 },
    lane_2_gain: { key: 'lane_2_gain', label: 'LANE 2', min: 0, max: 2, step: 0.02, value: 1 },
    lane_3_gain: { key: 'lane_3_gain', label: 'LANE 3', min: 0, max: 2, step: 0.02, value: 1 },
    lane_4_gain: { key: 'lane_4_gain', label: 'LANE 4', min: 0, max: 2, step: 0.02, value: 1 },
    pressure_mode: { key: 'pressure_mode', label: 'PRESSURE', min: 0, max: 1, step: 1, value: 1, integer: true, labels: ['OFF', 'OFFSET'] },
    root_note: { key: 'root_note', label: 'ROOT', min: 24, max: 96, step: 1, value: 60, integer: true, note: true },
    scale: { key: 'scale', label: 'SCALE', min: 0, max: 24, step: 1, value: 0, integer: true, labels: SCALE_LABELS },
    record_gain: { key: 'record_gain', label: 'REC GAIN', min: 0, max: 4, step: 0.05, value: 1 },
    record_source: { key: 'record_source', label: 'REC SOURCE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['INPUT', 'RESAMPLE'] },
    monitor_level: { key: 'monitor_level', label: 'MONITOR', min: 0, max: 1, step: 0.01, value: 0 },
    clip_rate: { key: 'clip_rate', label: 'LOOP RATE', min: 0.25, max: 4, step: 0.05, value: 1 },
    clip_record_mode: { key: 'clip_record_mode', label: 'REC MODE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['REPLACE', 'OVERDUB'] },
    clip_take_undo: { key: 'clip_take_undo', label: 'UNDO TAKE', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['TURN', 'UNDO'], momentary: true, result: 'TAKE UNDONE' },
    clip_timing: { key: 'clip_timing', label: 'TIMING', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['FREE', 'BEAT SYNC'] },
    launch_quantize: { key: 'launch_quantize', label: 'LAUNCH', min: 0, max: 2, step: 1, value: 0, integer: true, labels: ['OFF', 'BEAT', 'BAR'] },
    tempo_bpm: { key: 'tempo_bpm', label: 'TEMPO', min: 40, max: 240, step: 1, value: 120, integer: true },
    tempo_swing: { key: 'tempo_swing', label: 'SWING', min: 50, max: 75, step: 1, value: 50, integer: true },
    hold_release_ms: { key: 'hold_release_ms', label: 'HOLD OFF', min: 0, max: 60000, step: 50, value: 0, integer: true },
    chain_enabled: { key: 'chain_enabled', label: 'CHAIN', min: 0, max: 1, step: 1, value: 0, integer: true, labels: ['OFF', 'ON'] },
    chain_slot: { key: 'chain_slot', label: 'CHAIN SLOT', min: 1, max: 16, step: 1, value: 1, integer: true },
    chain_clip: { key: 'chain_clip', label: 'CLIP', min: 1, max: 64, step: 1, value: 1, integer: true },
    chain_repeats: { key: 'chain_repeats', label: 'REPEATS', min: 1, max: 16, step: 1, value: 1, integer: true },
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
    'long_grain_mode', 'waveset_direction'];
const MAIN_ENGINE_CONTROLS = MAIN_ENGINE_MODES.map((mode) => [
    'locus', 'field', 'speed', 'manual_lane',
    'lane_cycles', 'join', mode, 'output_gain',
]);
MAIN_ENGINE_CONTROLS[3][4] = 'cut_lane_advance';
const ENGINE_PAGE = { title: 'ENGINE', keys: ['algorithm', 'locus', 'field',
    'speed', 'manual_lane', 'lane_cycles', 'join', 'output_gain'] };
const CUTUPS_ENGINE_PAGE = { ...ENGINE_PAGE, keys: ENGINE_PAGE.keys.map((key) => key === 'lane_cycles' ? 'cut_lane_advance' : key) };
const STACK_PATH_PAGE = { title: 'STACK PATH', keys: ['lane_path',
    'lane_blend', 'lane_path_shape', 'lane_path_offset', 'lane_path_skew',
    'lane_path_curve', 'lane_slew'] };
const DIRECT_PLAY_PAGE = { title: 'DIRECT PLAY', keys: ['sample_mode',
    'lane_variation'] };
const PADS_SCALE_PAGE = { title: 'PADS & SCALE', keys: ['pad_mode',
    'root_note', 'scale', 'hold_release_ms', 'polyphony_comp'] };
const AMP_ENVELOPE_PAGE = { title: 'AMP ENVELOPE', breakpoint: true,
    keys: ['amp_attack', 'amp_decay', 'amp_sustain', 'amp_release'] };
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
    [DIRECT_PLAY_PAGE],
    [
        DIRECT_PLAY_PAGE,
        { title: 'SLICE SOURCE', keys: ['slice_region_mode', 'slice_regions'] },
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
];

function playbackPagesForAlgorithm(algorithm, motionSound) {
    const pages = [...(PLAYBACK_PAGES[algorithm] || PLAYBACK_PAGES[0])];
    if (algorithm !== 0) return pages;
    if (motionSound === 1) pages.push(PACKETS_PAGE);
    else if (motionSound === 2) pages.push(MOTOR_PAGE);
    else if (motionSound >= 3) pages.push(EVENT_CLOCK_PAGE, EVENT_SHAPE_PAGE);
    return pages;
}
const SPLIT_SETUP_PAGE = {title:'SPLIT SETUP',global:true,keys:['split_mode','split_focus','split_copy','part_mute']};
const SPLIT_SEND_KEYS = ['split_delay_a','split_reverb_a','split_delay_b','split_reverb_b'];
const SPLIT_MIX_PAGE = {title:'FX SENDS',global:true,keys:[...SPLIT_SEND_KEYS,'effect_mix','effect_reverb_mix','split_curve','split_blend']};
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
const FX_ORDER_PAGE = { title: 'FX ORDER', route: true, keys: Array(8).fill(null) };
const FX_BLOCK_NAMES = ['Tone / Drive', 'Bits / Rate', 'Delay', 'Stereo / Rotary', 'Tape', 'Reverb'];
const FX_ROUTE_PRESETS = [[0,1,2,3,4,5], [0,1,5,2,3,4], [5,1,0,2,3,4]];
const FX_ROUTE_NAMES = ['DFLT','ECHO','CRSH'];
const SAMPLE_PAGE = { title: 'SAMPLE EDIT', keys: ['sample_trim_start',
    'sample_trim_end', 'sample_trim_apply', 'sample_reverse',
    'sample_normalize', 'sample_fade'] };
// Keep the full current/total page count visible in Move's 21-character
// title row. The manual continues to use the unabbreviated page name.
const LOOP_PAGE = { title: 'PERF LOOP', keys: ['clip_rate',
    'clip_timing', 'launch_quantize', 'tempo_bpm', 'clip_record_mode',
    'tempo_swing', 'pressure_mode'] };
const TAKE_PAGE = { title: 'OVERDUB TAKES', keys: [null, 'clip_take_undo'] };
const CHAIN_PAGE = { title: 'CHAIN', keys: ['chain_enabled', 'chain_slot',
    'chain_clip', 'chain_repeats', 'chain_clear'] };

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
    'effect_reverb_algo', 'effect_reverb_decay', 'effect_reverb_pre', 'effect_reverb_damp', 'effect_reverb_diff', 'effect_reverb_mod', 'effect_reverb_width', 'effect_reverb_mix', 'effect_order', 'cut_lane_advance', 'split_blend','split_level_a','split_level_b','split_delay_a','split_delay_b','split_reverb_a','split_reverb_b','split_curve',
];
const LOCKABLE_PARAMS = new Set(LOCKABLE_KEYS);

const SHORT_LABELS = {
    split_blend:'BLND',split_level_a:'ALVL',split_level_b:'BLVL',split_delay_a:'ADLY',split_delay_b:'BDLY',split_reverb_a:'AREV',split_reverb_b:'BREV',split_curve:'CURV',split_copy:'COPY',split_focus:'EDIT',part_mute:'MUTE',
    effect_reverb_algo: 'ALGO', effect_reverb_decay: 'DECY', effect_reverb_pre: 'PRE', effect_reverb_damp: 'DAMP', effect_reverb_diff: 'DIFF', effect_reverb_mod: 'MOD', effect_reverb_width: 'WDTH', effect_reverb_mix: 'MIX', effect_order: 'ORDR',
    cut_lane_advance: 'ADV',
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
    splitEnabled:false,splitFocus:0,splitBlend:.5,splitParts:[{},{}],splitPages:[0,0],
    focusButtonHeld:false,focusButtonUsed:false,voiceAlgorithms:Array(8).fill(-1),
    editorReady:false,
    fxMenuSelection:0,fxMenuReturn:false,
    lane: 1,
    mask: 0,
    recording: false,
    meter: 0,
    position: 0,
    fieldStart: 0,
    fieldEnd: 1,
    weights: [0, 0, 0, 0],
    playheads: Array(8).fill(-1),
    wavesetVoices: Array(8).fill(null),
    cutupsVoices: Array(8).fill(null),
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
    clipPrepareState: 0,
    chainEnabled: false,
    chainPosition: 0,
    chainRepeat: 0,
    chainSlot: 0,
    chainQueuedSlot: -1,
    chainEntries: Array.from({ length: 16 }, () => ({
        enabled: false, clip: 0, repeats: 1,
    })),
    undoAvailable: false,
    maximumRenderMicros: 0,
    renderOverruns: 0,
    controlPeakMicros: 0, queryPeakMicros: 0, workPeakMicros: 0, workOverruns: 0,
    limiterReductionDb: 0,
    limiterMaximumReductionDb: 0,
    helpPage: 0,
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

function resetParameterEncoderMotion() {
    for (const motion of parameterEncoderMotion) {
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
    const scaled = motion.remainder + delta * PARAMETER_ENCODER_RESPONSE;
    const magnitude = Math.floor(Math.abs(scaled) + 0.5);
    const applied = scaled < 0 ? -magnitude : magnitude;
    motion.remainder = scaled - applied;
    if (Math.abs(motion.remainder) < 1e-9) motion.remainder = 0;
    return applied;
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
    state.editorReady=false;
    let snapshot;
    try { snapshot=JSON.parse(String(getParam('ui_editor_state',''))); }
    catch (_) { return false; }
    const values=snapshot?.values,split=snapshot?.split;
    if(snapshot?.v!==1 || !values || typeof values!=='object'
        || !split || ![0,1].includes(split.enabled) || ![0,1].includes(split.focus)
        || !Array.isArray(split.parts) || split.parts.length!==2
        || split.parts.some((part)=>!part || typeof part!=='object')) return false;
    // Validate the complete response before replacing any displayed controls.
    for(const [key,control] of Object.entries(PARAMS))
        if(!control.momentary && !Number.isFinite(values[key])) return false;
    for(const key of ['pad_octave','record_lane','stack_scan_paused','glitch_buffer_lock'])
        if(!Number.isFinite(values[key])) return false;
    for(const [key,control] of Object.entries(PARAMS))
        control.value=control.momentary?0:clamp(values[key],control.min,control.max);
    parseSplitState(split,false);
    state.algorithm=Math.round(PARAMS.algorithm.value);
    state.padMode=Math.round(PARAMS.pad_mode.value);state.scale=Math.round(PARAMS.scale.value);
    state.octave=clamp(values.pad_octave,-3,3);
    state.lane=clamp(Math.round(values.record_lane),1,4);
    state.recordSource=Math.round(PARAMS.record_source.value);
    state.stackScanPaused=values.stack_scan_paused!==0;
    state.glitchBufferLocked=values.glitch_buffer_lock!==0;
    state.voiceAlgorithms=String(snapshot.voices||'').split(',').map(Number);
    state.editorReady=true;
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

function parseSplitState(text, refresh=true) {
    let data;
    try { data=typeof text==='object'?text:JSON.parse(String(text||'{}')); } catch (_) { return; }
    if(!data || typeof data!=='object')return;
    const enabled=Number(data.enabled)===1;
    const focus=clamp(Number(data.focus)||0,0,1);
    const changed=enabled!==state.splitEnabled || focus!==state.splitFocus;
    if(changed && refresh && refreshEditorSnapshot())return;
    if(changed)state.splitPages[state.splitFocus]=state.settingsPage;
    state.splitEnabled=enabled;state.splitFocus=focus;
    if(!enabled && state.view==='fxMenu'){state.view='main';state.fxMenuReturn=false;}
    state.splitBlend=clamp(Number(data.blend)||0,0,1);
    let padMapChanged = false;
    if (Array.isArray(data.parts) && data.parts.length === 2) {
        padMapChanged = data.parts.some((part, index) =>
            ['root', 'octave', 'scale', 'mode', 'mute'].some((key) =>
                part[key] !== state.splitParts[index]?.[key]));
        state.splitParts = data.parts;
    }
    PARAMS.split_mode.value=enabled?1:0;PARAMS.split_focus.value=focus;
    if(changed) {
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
    if(state.splitEnabled)return state.splitParts[index%8>=4?1:0]||{};
    return {root:Math.round(PARAMS.root_note.value),octave:state.octave,scale:state.scale,mode:state.padMode};
}
function setSplitFocus(part) {
    setParamBlocking('split_focus',part);
    if(!refreshEditorSnapshot())parseSplitState(getParam('split_state','{}'),false);
    if(!state.editorReady)showMessage('UPDATING EDITOR');
    else if(state.view!=='main')showMessage('EDIT PART '+(state.splitFocus?'B':'A'));
    rebuildLedQueue();draw();
}
function toggleSplitMode() {
    setParamBlocking('split_mode',state.splitEnabled?0:1);
    parseSplitState(getParam('split_state','{}'));
    const error=String(getParam('last_error','')||'');
    showMessage(error|| (state.splitEnabled?'SPLIT A / B':'SINGLE STACK'));rebuildLedQueue();
    primePadLeds();
}
function adjustSplitBlend(delta) {
    if(!state.editorReady){showMessage('UPDATING EDITOR');return;}
    delta=parameterEncoderDelta(7,'split_blend',delta);if(!delta)return;
    const control=PARAMS.split_blend;control.value=clamp(control.value+delta*.01,0,1);
    setParam('split_blend',control.value.toFixed(4));
    if(state.clipRecording)setParam('clip_automation','split_blend');
    showMessage('BLEND A '+Math.round((1-control.value)*100)+' B '+Math.round(control.value*100));
}

function parsePlayheads(text) {
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
        state.clipRecordedTempo = clamp(values[10] / 100, 40, 240);
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
    const values = String(text || '').split(',').map((item) => Number(item));
    if (values.length < 4 || !values.every((value) => Number.isFinite(value))) return;
    state.chainEnabled = values[0] !== 0;
    state.chainPosition = clamp(values[1] | 0, 0, 15);
    state.chainRepeat = Math.max(0, values[2] | 0);
    state.chainSlot = clamp(values[3] | 0, 0, 15);
    state.chainQueuedSlot = values.length >= 5 && values[4] >= 0
        ? clamp(values[4] | 0, 0, 15) : -1;
    PARAMS.chain_enabled.value = state.chainEnabled ? 1 : 0;
    PARAMS.chain_slot.value = state.chainSlot + 1;
}

function parseChainEntries(text) {
    const entries = String(text || '').split(',');
    for (let index = 0; index < state.chainEntries.length; index++) {
        const fields = String(entries[index] || '').split(':').map(Number);
        state.chainEntries[index] = fields.length >= 3
            && fields.every((value) => Number.isFinite(value))
            ? { enabled: fields[0] !== 0, clip: clamp(fields[1] | 0, 0, 63),
                repeats: clamp(fields[2] | 0, 1, 16) }
            : { enabled: false, clip: 0, repeats: 1 };
    }
}

function clipOccupied(index) {
    if (index < 32) return ((state.clipMaskLow >>> index) & 1) !== 0;
    return ((state.clipMaskHigh >>> (index - 32)) & 1) !== 0;
}

function basePadColor(owner = padSettings(0), index = 0) {
    if (state.splitEnabled && index % 8 >= 4) return SplitBIdle;
    if (owner.mode === 1) return AzureBlue;
    if (owner.mode === 2) return WhiteLedDim;
    if (owner.mode === 3) return Purple;
    if (owner.scale !== 0) return ScaleNoteGrey;
    return Black;
}

function padColor(index) {
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
        if(state.splitEnabled ? Math.floor(index/8)>0 : index>=8)return Black;
        if (state.activePads.has(MovePads[index])) return Cyan;
        return state.cues[state.splitEnabled?(index%4)+(index%8>=4?4:0):index] >= 0
            ? BrightOrange : (state.splitEnabled && index % 8 >= 4 ? SplitBIdle : WhiteLedDim);
    }
    if (state.activePads.has(MovePads[index])) return Cyan;
    if (owner.mode === 0 && owner.scale !== 0
        && (padMidiNote(index) - Math.round(owner.root||60)) % 12 === 0) {
        return state.splitEnabled && index % 8 >= 4 ? SplitBRoot : White;
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

function rebuildLedQueue() {
    queueLed('cc',MoveCopy,state.splitEnabled?WhiteLedBright:WhiteLedDim,0);
    queueLed('cc',MoveCapture,state.splitEnabled?(state.splitFocus?WhiteLedBright:WhiteLedDim):Black,0);
    queueLed('cc',MoveMute,state.splitEnabled&&state.splitParts[state.splitFocus]?.mute?WhiteLedBright:WhiteLedDim,0);
    queueLed('cc', MovePlay,
        state.clipLaunchPending ? BlueViolet
            : (state.clipPlaying ? NeonGreen : WhiteLedDim), 0);
    queueLed('cc', MoveLoop,
        state.glitchBufferLocked ? BrightOrange
            : (state.stackScanPaused ? WhiteLedBright
                : (state.recordedStackHold ? BrightYellow : WhiteLedDim)), 0);
    for (let i = 0; i < MovePads.length; i++) {
        const note = MovePads[i];
        queueLed('note', note, padColor(i), 3);
    }
    for (let i = 0; i < MoveSteps.length; i++) {
        let color = Black;
        let urgent = false;
        if (state.view === 'chain') {
            if (state.chainEntries[i].enabled) color = White;
            if (i === state.chainSlot) color = BrightOrange;
            if (i === state.chainPosition && state.chainEnabled) color = NeonGreen;
            if (i === state.chainQueuedSlot) color = BlueViolet;
            urgent = i === state.chainSlot || i === state.chainPosition
                || i === state.chainQueuedSlot;
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
    for (let i = 0; i < MoveTracks.length; i++) {
        queueLed('cc', MoveTracks[i], state.view === 'clips'
            ? (i === state.clipBank ? WhiteLedBright : WhiteLedDim)
            : (i + 1 === state.lane ? WhiteLedBright : WhiteLedDim), 2);
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
const splitDisplay={valid:false,header:null,gutter:null,rail:null,footer:null,
    waves:null,mask:-1,windows:null,
    base:new Uint32Array(SPLIT_CANVAS_WIDTH*2),
    frame:new Uint32Array(SPLIT_CANVAS_WIDTH*2),
    painted:new Uint32Array(SPLIT_CANVAS_WIDTH*2)};
let splitRasterTarget=null;

function clearDisplay() {
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

function waveformX(position,window) {
    if(!Number.isFinite(position)||position<window[0]-1e-6||position>window[1]+1e-6)return -1;
    return 14+Math.round(clamp((position-window[0])/(window[1]-window[0]),0,1)*113+1e-9);
}

function drawWaveformLane(lane,window,center) {
    if(!(state.mask&(1<<lane))){drawRect(14,center,114,1,1);return;}
    const bins=state.waves[lane];
    for(let pixel=0;pixel<114;++pixel) {
        const source=(window[0]+pixel/113*(window[1]-window[0]))*(WAVE_BINS-1);
        const first=Math.min(WAVE_BINS-1,Math.floor(source)),next=Math.min(WAVE_BINS-1,first+1),mix=source-first;
        const low=clamp(bins[first][0]+(bins[next][0]-bins[first][0])*mix,-7,7);
        const high=clamp(bins[first][1]+(bins[next][1]-bins[first][1])*mix,-7,7);
        const top=center-Math.round(high*4/7),bottom=center-Math.round(low*4/7);
        drawRect(14+pixel,top,1,Math.max(1,bottom-top+1),1);
    }
}

function drawSplitWavePixels(windows) {
    const key=windows.flat().join(',');
    if(splitDisplay.waves!==state.waves||splitDisplay.mask!==state.mask||splitDisplay.windows!==key) {
        splitDisplay.base.fill(0);splitRasterTarget=splitDisplay.base;
        for(let lane=0;lane<4;++lane) {
            drawWaveformLane(lane,windows[lane<2?0:1],15+lane*10);
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
        const center=15+lane*10,window=windows[lane<2?0:1];
        for(const region of entry.regions.slice(1,-1)) {
            const x=waveformX(region,window);
            if(x>=0){drawRect(x,center-4,1,2,1);drawRect(x,center+3,1,2,1);}
        }
    }
    const markers=[];
    if(state.mask)for(let slot=0;slot<8;++slot) {
        const part=slot<4?0:1,top=10+part*20,window=windows[part];
        const algorithm=state.voiceAlgorithms[slot],cut=state.cutupsVoices[slot],ws=state.wavesetVoices[slot];
        const head=algorithm===3?(cut?.gain>0?cut.position:-1):algorithm===8?(ws?.sourcePosition??-1):state.playheads[slot];
        const lane=algorithm===3?(cut?.lane??-1):(state.stackPositions[slot]>=0?state.stackPositions[slot]*3:-1);
        const x=waveformX(head,window);
        if(x>=0&&lane>=part*2&&lane<=part*2+1){drawRect(x,top,1,20,1);markers.push([x,15+lane*10]);}
        if(algorithm===3&&cut?.outgoingLane>=part*2&&cut.outgoingLane<=part*2+1&&cut.outgoingGain>0) {
            const outgoing=waveformX(cut.outgoingPosition,window);
            if(outgoing>=0){drawDashedPlayhead(outgoing,top,20);markers.push([outgoing,15+cut.outgoingLane*10]);}
        }
        if(algorithm===8&&ws?.secondarySourcePosition>=0) {
            const outgoing=waveformX(ws.secondarySourcePosition,window);
            if(outgoing>=0)drawDashedPlayhead(outgoing,top,20);
        }
    }
    for(const [x,y] of markers)clearStackMarker(x,y);
    for(const [x,y] of markers)drawStackMarker(x,y);
    splitRasterTarget=null;paintSplitWaveChanges();
}

function drawSplitMain() {
    if(!splitDisplay.valid) {
        clearDisplay();splitDisplay.painted.fill(0);
        splitDisplay.header=splitDisplay.gutter=splitDisplay.rail=splitDisplay.footer=null;
    }
    const name=(part)=>state.splitParts[part]?.mute?'MUT':(ALGORITHMS[state.splitParts[part]?.algorithm]||'---').slice(0,3);
    const title='A:'+name(0)+' B:'+name(1)+' '+Math.round(state.splitBlend*100)+'%';
    if(title!==splitDisplay.header) {
        drawRect(0,0,128,9,0);print(1,0,title,1);splitDisplay.header=title;
    }
    const weights=state.weights.map((weight)=>Math.round(weight*7));
    const gutter=[state.splitFocus,state.lane,...weights].join(',');
    if(gutter!==splitDisplay.gutter) {
        drawRect(0,10,12,40,0);
        for(let lane=0;lane<4;++lane) {
            const y=11+lane*10,focused=Math.floor(lane/2)===state.splitFocus;
            if(state.lane===lane+1)drawRect(0,y,2,8,1);
            if(focused)drawRect(2,y,7,8,1);
            print(3,y,String(lane+1),focused?0:1);
            if(weights[lane]>0)drawRect(9,y+8-weights[lane],2,weights[lane],1);
        }
        drawRect(0,30,12,1,1);splitDisplay.gutter=gutter;
    }
    drawSplitWavePixels([splitWaveWindow(0),splitWaveWindow(1)]);
    const blend=clamp(Math.round(state.splitBlend*125),0,125),meter=Math.max(1,Math.round(114*state.meter));
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

function drawMain() {
    if(state.splitEnabled){drawSplitMain();return;}
    clearDisplay();
    const algorithm = ALGORITHMS[state.algorithm] || ALGORITHMS[0];
    const recordTag = state.clipRecording ? ' DATA REC'
        : (state.recording ? (state.limited ? ' LIM' : ' AUDIO') : '');
    const displayedAlgorithm = state.algorithm === 6 && state.glitchBufferLocked
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

    const wavesetFocus = state.algorithm === 8 ? focusedWavesetVoice() : null;
    const displayedPlayheads = wavesetFocus
        ? state.wavesetVoices.map((voice) => voice
            ? voice.sourcePosition : -1)
        : state.playheads;
    if (state.mask !== 0) {
        if (state.algorithm === 3) {
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
    const clipStatus = state.clipNoteFull || state.clipPointFull
        ? 'CLIP FULL N' + state.clipNoteCount + ' G' + state.clipPointCount
        : (state.clipLaunchPending ? 'QUEUED ' + PARAMS.launch_quantize.labels[
            Math.round(PARAMS.launch_quantize.value)]
            : ((state.clipPlaying ? 'PLAY ' : 'STOP ')
                + (state.clipTiming ? state.clipBeats.toFixed(1) + 'b SYNC '
                    : (state.clipDuration / 1000).toFixed(1) + 's ')
                + state.clipRate.toFixed(2) + 'x T' + state.clipTakeCount));
    const mutedParts=state.splitEnabled?[0,1].filter((part)=>state.splitParts[part]?.mute):[];
    const muteHint=mutedParts.length
        ? mutedParts.map((part)=>part?'B':'A').join('+')+' MUTED: '
            +(state.splitParts[state.splitFocus]?.mute?'PRESS MUTE':'CAPTURE MUTE')
        : '';
    const footer = state.messageTicks > 0
        ? state.message
        : (muteHint || (state.algorithm === 6 && state.glitchBufferLocked
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
    const page = (state.settingsPage + 1) + '/' + parameterPages().length;
    const heading = (state.clipRecording ? 'REC FX' : (state.clipPlaying ? '> FX ORD' : 'FX ORDER'))
        + ' ' + page + ' 7:' + presetName;
    print(1, 0, (state.actionMessageTicks > 0 ? state.actionMessage : heading).slice(0, 21), 1);
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
    print(1, 57, footer, 1);
}

function splitFxPages() {
    const part=state.splitFocus?'B':'A';
    return [
        {title:'SENDS + RETURNS',page:SPLIT_MIX_PAGE},
        {title:'DELAY',page:DELAY_RETURN_PAGE},
        {title:'REVERB',page:REVERB_PAGE},
        {title:part+' COLOR INSERT',page:PART_COLOR_PAGE},
        {title:part+' STEREO / ROTARY',page:STEREO_ROTARY_PAGE},
        {title:'SHARED TAPE',page:TAPE_PAGE},
        {title:'EFFECT ORDER',page:FX_ORDER_PAGE}
    ];
}

function syncSplitFxValues() {
    for(const key of [...SPLIT_MIX_PAGE.keys,'split_level_a','split_level_b']) {
        const control=PARAMS[key];
        control.value=clamp(getNumber(key,control.value),control.min,control.max);
    }
}

function openSplitSends() {
    syncSplitFxValues();state.view='settings';state.fxMenuReturn=false;
    state.settingsPage=parameterPages().indexOf(SPLIT_MIX_PAGE);
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
    const delayOff=PARAMS.effect_mix.value<=0 && (PARAMS.split_delay_a.value>0||PARAMS.split_delay_b.value>0);
    const reverbOff=PARAMS.effect_reverb_mix.value<=0 && (PARAMS.split_reverb_a.value>0||PARAMS.split_reverb_b.value>0);
    if(delayOff&&reverbOff)return 'RETURNS OFF: KNOBS 5/6';
    if(delayOff)return 'DELAY OFF: KNOB 5';
    if(reverbOff)return 'REVERB OFF: KNOB 6';
    return 'CLICK: FX PAGES';
}

function drawSplitFxMenu() {
    clearDisplay();
    const entries=splitFxPages();
    print(1,0,'FX PAGES '+(state.fxMenuSelection+1)+'/'+entries.length+' EDIT '+(state.splitFocus?'B':'A'),1);
    drawRect(0,8,128,1,1);
    const start=clamp(state.fxMenuSelection-4,0,entries.length-5);
    for(let row=0;row<5;++row) {
        const index=start+row,selected=index===state.fxMenuSelection,y=10+row*9;
        if(selected)drawRect(0,y,128,8,1);
        print(1,y,(selected?'>':' ')+entries[index].title,selected?0:1);
    }
    print(1,57,'TURN PICK CLICK OPEN',1);
}

function currentSettingsPage() {
    const pages = parameterPages();
    state.settingsPage = clamp(state.settingsPage | 0, 0, pages.length - 1);
    return pages[state.settingsPage];
}

function parameterPages() {
    const enginePages=playbackPagesForAlgorithm(state.algorithm,Math.round(PARAMS.motion_sound.value));
    const engine=state.algorithm===3?CUTUPS_ENGINE_PAGE:ENGINE_PAGE;
    if(!state.splitEnabled)return [engine,PADS_SCALE_PAGE,AMP_ENVELOPE_PAGE,...enginePages,
        MIX_REC_PAGE,EFFECTS_PAGE,STEREO_ROTARY_PAGE,TAPE_PAGE,REVERB_PAGE,FX_ORDER_PAGE,SAMPLE_PAGE,LOOP_PAGE,TAKE_PAGE,CHAIN_PAGE];
    const engineSplit={...engine,keys:engine.keys.map((key)=>key==='output_gain'?(state.splitFocus?'split_level_b':'split_level_a'):key)};
    const rec={...MIX_REC_PAGE,keys:[state.splitFocus?'lane_3_gain':'lane_1_gain',state.splitFocus?'lane_4_gain':'lane_2_gain',null,null,...MIX_REC_PAGE.keys.slice(4)]};
    return [engineSplit,PADS_SCALE_PAGE,AMP_ENVELOPE_PAGE,...enginePages,SPLIT_MIX_PAGE,SPLIT_SETUP_PAGE,
        rec,PART_COLOR_PAGE,STEREO_ROTARY_PAGE,DELAY_RETURN_PAGE,{...TAPE_PAGE,global:true},
        {...REVERB_PAGE,global:true,keys:REVERB_PAGE.keys.map((key)=>key==='effect_reverb_mix'?null:key)},FX_ORDER_PAGE,SAMPLE_PAGE,LOOP_PAGE,TAKE_PAGE,CHAIN_PAGE];
}

function controlForKey(key) {
    const control = PARAMS[key];
    if (control && state.splitEnabled) {
        if (key==='effect_mix') return {...control,shortLabel:'DRTN',label:'DELAY RETURN %'};
        if (key==='effect_reverb_mix') return {...control,shortLabel:'RRTN',label:'REVERB RETURN %'};
        if (key==='split_level_a'||key==='split_level_b') return {...control,shortLabel:'LEVL',label:'PART LEVEL'};
    }
    if(control && key==='manual_lane' && state.splitEnabled) return {...control,max:2,value:clamp(control.value,1,2),...(state.algorithm===3?{label:'LANE',shortLabel:'LANE',integer:true,step:1,value:clamp(Math.round(control.value),1,2)}:{})};
    if (control && key === 'manual_lane' && state.algorithm === 3) {
        return { ...control, label: 'LANE', shortLabel: 'LANE',
            integer: true, step: 1, value: Math.round(control.value) };
    }
    return control;
}

function activeControlKeys() {
    return state.view === 'settings'
        ? currentSettingsPage().keys
        : (MAIN_ENGINE_CONTROLS[state.algorithm] || MAIN_ENGINE_CONTROLS[0]).map((key,index)=>state.splitEnabled&&index===7?'split_blend':key);
}

function noteName(value) {
    const names = ['C', 'C#', 'D', 'D#', 'E', 'F',
        'F#', 'G', 'G#', 'A', 'A#', 'B'];
    const note = Math.round(value);
    return names[((note % 12) + 12) % 12] + (Math.floor(note / 12) - 1);
}

function formatControlValue(control) {
    if (!control) return '';
    if(SPLIT_SEND_KEYS.includes(control.key))return Math.round(control.value*100)+'%';
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
    if (control.key === 'amp_attack' || control.key === 'amp_decay'
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
    if (control && control.key === 'effect_reverb_algo') return control.value < .5 ? 'PLAT' : 'ROOM';
    if (control && control.key === 'effect_reverb_decay') return control.value.toFixed(control.value < 10 ? 2 : 1);
    if (control && control.key === 'effect_reverb_damp') return control.value >= 1000
        ? (control.value / 1000).toFixed(control.value < 10000 ? 1 : 0) + 'k'
        : String(Math.round(control.value));
    if (!control) return '';
    if(SPLIT_SEND_KEYS.includes(control.key))return Math.round(control.value*100)+'%';
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
    if (control.key === 'amp_attack' || control.key === 'amp_decay'
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
            return ['DOWN', 'UP', 'TRI', 'SINE', 'SDN', 'SUP', 'MAN', 'RAND'][
                clamp(Math.round(control.value), 0, 7)];
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

function drawSettings() {
    clearDisplay();
    if(!state.editorReady){print(1,0,'UPDATING EDITOR',1);return;}
    const page = currentSettingsPage();
    if (page.route) { drawFxOrder(); return; }
    const target = state.clipRecording ? 'REC ' : (state.clipPlaying ? '> ' : '');
    const pages = parameterPages();
    const takeSuffix = page === TAKE_PAGE ? ' ' + state.clipTakeCount
        : (page === FX_ORDER_PAGE && state.fxRoutePending ? '*' : '');
    const lockTarget = state.algorithm === 6 && state.glitchBufferLocked
        ? 'LCK ' : '';
    const scope=state.splitEnabled?((page===SPLIT_MIX_PAGE||page===SPLIT_SETUP_PAGE)?'':page.global?'FX ':((state.splitFocus?'B ':'A '))):'';
    const pageTitle = target + lockTarget + scope + page.title + takeSuffix + ' '
        + (state.settingsPage + 1)
        + '/' + pages.length;
    const title = state.actionMessageTicks > 0
        ? state.actionMessage : pageTitle;
    print(1, 0, title.slice(0, 21), 1);
    drawRect(0, 8, 128, 1, 1);
    if (page.breakpoint) {
        drawAmpEnvelopePage();
        return;
    }
    const hasFooter = state.splitEnabled && (page===SPLIT_MIX_PAGE
        || page===DELAY_RETURN_PAGE || page.title===REVERB_PAGE.title);
    for (let index = 0; index < 8; index++) {
        const key = page.keys[index];
        if (!key) {
            drawReservedParameterCell(index, hasFooter);
            continue;
        }
        const control = controlForKey(key);
        drawParameterCell(index, control, hasFooter);
    }
    if (state.splitEnabled && page===SPLIT_MIX_PAGE) print(1,57,splitSendFooter(),1);
    else if (state.splitEnabled && (page===DELAY_RETURN_PAGE || page.title===REVERB_PAGE.title))
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
        print(1, 0, 'SYSTEM REPORT 1/3 <>', 1);
        print(1, 13, helpReportRow('RENDER PEAK',
            compactReportCount(state.maximumRenderMicros) + ' us'), 1);
        print(1, 25, helpReportRow('RENDER MISSES',
            compactReportCount(state.renderOverruns)), 1);
        print(1, 37, helpReportRow('LIMIT CURRENT',
            state.limiterReductionDb.toFixed(1) + ' dB'), 1);
        print(1, 49, helpReportRow('LIMIT MAX',
            state.limiterMaximumReductionDb.toFixed(1) + ' dB'), 1);
        return;
    }
    if (state.helpPage === 1) {
        print(1, 0, 'CALLBACK WORK 2/3 <>', 1);
        print(1, 13, helpReportRow('CONTROL PEAK', compactReportCount(state.controlPeakMicros) + ' us'), 1);
        print(1, 25, helpReportRow('QUERY PEAK', compactReportCount(state.queryPeakMicros) + ' us'), 1);
        print(1, 37, helpReportRow('WORK PEAK', compactReportCount(state.workPeakMicros) + ' us'), 1);
        print(1, 49, helpReportRow('WORK MISSES', compactReportCount(state.workOverruns)), 1);
        return;
    }
    print(1, 0, 'SHORTCUTS 3/3 <>', 1);
    print(1, 13, 'REC        data loop', 1);
    print(1, 23, 'SHIFT+REC  audio lane', 1);
    print(1, 33, 'STEPS      clip slots', 1);
    print(1, 43, 'SHIFT+PAD  note hold', 1);
    print(1, 55, 'SH+LOOP glitch lock', 1);
}

function clipLabel(index) {
    return BANK_NAMES[Math.floor(index / 16)]
        + String(index % 16 + 1).padStart(2, '0');
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
    print(1, 57, 'TRK BANK SHIFT=COPY', 1);
}

function drawChain() {
    clearDisplay();
    const selected = state.chainEntries[state.chainSlot];
    const selection = selected && selected.enabled
        ? String(selected.clip + 1).padStart(2, '0') + 'x' + selected.repeats
        : 'EMPTY';
    print(1, 0, ('CHAIN ' + (state.chainEnabled ? 'ON ' : 'OFF ')
        + String(state.chainSlot + 1).padStart(2, '0') + ' ' + selection)
        .slice(0, 21), 1);
    drawRect(0, 8, 128, 1, 1);
    for (let index = 0; index < 16; index++) {
        const entry = state.chainEntries[index];
        const column = index % 4;
        const row = Math.floor(index / 4);
        const x = column * 32;
        const y = 13 + row * 11;
        let marker = index === state.chainSlot ? ':' : ' ';
        if (index === state.chainPosition && state.chainEnabled) marker = '>';
        if (index === state.chainQueuedSlot) marker = '+';
        const repeat = entry.repeats > 9 ? '+' : String(entry.repeats);
        const body = entry.enabled
            ? String(entry.clip + 1).padStart(2, '0') + 'x' + repeat : '--';
        print(x + 1, y, (marker + body).slice(0, 5), 1);
    }
    print(1, 57, 'DEL STEP SH=MV CLICK+', 1);
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
    const heading = leaf === 'THRU RECORDINGS'
        ? 'L' + state.libraryLane + ' THRU RECORDINGS'
        : 'L' + state.libraryLane + ' SAMPLE ' + leaf;
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
    let footer = 'KNOB browse CLICK load';
    if (state.sampleLoadState >= 1 && state.sampleLoadState <= 5) {
        footer = 'LOADING ' + Math.round(state.sampleLoadProgress) + '%';
    } else if (state.sampleLoadState === 7) {
        footer = state.sampleLoadName || 'LOAD ERROR';
    } else if (state.sampleLoadState === 8) {
        footer = 'PREVIEW  CLICK LOAD';
    }
    print(1, 57, footer.slice(0, 21), 1);
}

function draw() {
    if (state.view === 'help') drawHelp();
    else if (state.view === 'settings') drawSettings();
    else if (state.view === 'clips') drawClips();
    else if (state.view === 'chain') drawChain();
    else if (state.view === 'library') drawLibrary();
    else if (state.view === 'fxMenu') drawSplitFxMenu();
    else drawMain();
}

function selectLane(lane) {
    if (state.recording) return;
    state.lane = clamp(lane | 0, 1, 4);
    setParamBlocking('record_lane', state.lane);
    parseSplitState(getParam('split_state','{}'));
    showMessage('RECORD LANE ' + state.lane);
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
    state.view = 'main';
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
    const destination = clamp(step | 0, 0, 15);
    if (state.deleteHeld) {
        setParamBlocking('chain_delete', destination);
        state.chainEntries.splice(destination, 1);
        state.chainEntries.push({ enabled: false, clip: 0, repeats: 1 });
        state.chainSlot = clamp(destination, 0, 15);
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
    entry.repeats = clamp(entry.repeats + (delta > 0 ? 1 : -1), 1, 16);
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
    state.chainEntries.length = 16;
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
    const row = Math.floor(padIndex / 8);
    const column = padIndex % (state.splitEnabled?4:8);
    if (owner.scale !== 0) {
        const intervals = SCALE_INTERVALS[owner.scale] || SCALE_INTERVALS[0];
        const degree = row * 3 + column;
        const octave = Math.floor(degree / intervals.length);
        return clamp(Math.round(owner.root||60) - 12
            + (owner.octave||0) * 12 + octave * 12
            + intervals[degree % intervals.length], 0, 127);
    }
    return clamp((state.splitEnabled?(owner.root||60)-12:48) + (owner.octave||0) * 12 + row * 5 + column, 0, 127);
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
    state.settingsPage = (state.settingsPage + (delta > 0 ? 1 : -1)
        + pages.length) % pages.length;
    state.actionMessageTicks = 0;
}

function applyMomentaryControl(control) {
    control.value = 0;
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

function adjustControl(index, delta) {
    if(!state.editorReady){showMessage('UPDATING EDITOR');return;}
    if (state.view === 'settings' && currentSettingsPage().route) {
        if (delta !== 0) adjustFxOrder(index, delta);
        return;
    }
    const keys = activeControlKeys();
    const control = controlForKey(keys[index]);
    if (!control || delta === 0) return;
    if (control.key === 'chain_clear') {
        parameterEncoderMotion[index].key = control.key;
        parameterEncoderMotion[index].remainder = 0;
        setParamBlocking('chain_clear', 1);
        PARAMS.chain_clip.value = 1;
        PARAMS.chain_repeats.value = 1;
        showActionMessage('CLEARED CHAIN ' + (state.chainSlot + 1));
        return;
    }
    if (control.momentary) {
        parameterEncoderMotion[index].key = control.key;
        parameterEncoderMotion[index].remainder = 0;
        applyMomentaryControl(control);
        return;
    }
    delta = parameterEncoderDelta(index, control.key, delta);
    if (delta === 0) return;
    let value;
    if (control.key === 'effect_reverb_decay' || control.key === 'effect_reverb_damp') {
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
    control.value = value;
    PARAMS[control.key].value = value;
    if (control.key === 'algorithm') {
        selectAlgorithm(value);
        return;
    }
    if(['split_mode','split_focus','part_mute'].includes(control.key)){setParamBlocking(control.key,value);parseSplitState(getParam('split_state','{}'));}
    else setParam(control.key, value.toFixed(control.integer ? 0 : 4));
    if (control.key === 'chain_enabled') {
        state.chainEnabled = value !== 0;
    } else if (control.key === 'chain_slot') {
        state.chainSlot = clamp(Math.round(value) - 1, 0, 15);
        PARAMS.chain_clip.value = clamp(getNumber('chain_clip', 1), 1, 64);
        PARAMS.chain_repeats.value = clamp(getNumber('chain_repeats', 1), 1, 16);
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
        state.recordSource = clamp(Math.round(value), 0, 1);
    } else if (control.key === 'clip_rate') {
        state.clipRate = value;
    }
    if (state.clipRecording && LOCKABLE_PARAMS.has(control.key)) {
        setParam('clip_automation', control.key);
    }
    showMessage(control.label + ' ' + formatControlValue(control), 35);
    if(state.splitEnabled && SPLIT_SEND_KEYS.includes(control.key) && value>0) {
        const delay=control.key.includes('delay');
        if(PARAMS[delay?'effect_mix':'effect_reverb_mix'].value<=0)
            showActionMessage(delay?'RAISE DRTN: KNOB 5':'RAISE RRTN: KNOB 6');
    }
}

function pollDiagnostics() {
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
    const limiter = String(getParam('limiter_state', '0,0'))
        .split(',').map((item) => Number(item));
    state.limiterReductionDb = Number.isFinite(limiter[0])
        ? Math.max(0, limiter[0]) : 0;
    state.limiterMaximumReductionDb = Number.isFinite(limiter[1])
        ? Math.max(0, limiter[1]) : 0;
}

function pollDsp() {
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
    parseSplitState(getParam('split_state','{}'));
    state.voiceAlgorithms=String(getParam('voice_algorithms','')).split(',').map(Number);
    parseUiState(getParam('ui_state', ''));
    parsePlayheads(getParam('playheads', ''));
    parseWavesetState(getParam('waveset_state', ''));
    parseCutupsState(getParam('cutups_state', ''));
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
    parseWaveforms(getParam('waveforms', ''));
}

globalThis.init = function() {
    splitDisplay.valid=false;state.waveText=null;
    // Schwung may clear or restore the physical surface between takeover sessions.
    if (typeof moveInputFilter.invalidateLedCache === 'function') {
        moveInputFilter.invalidateLedCache();
    }
    state.ticks = 0;
    state.view = 'main';
    state.settingsPage = 0;
    state.helpPage = 0;
    state.splitEnabled = false; state.splitFocus = 0; state.splitPages = [0,0];
    state.focusButtonHeld = false; state.focusButtonUsed = false;
    state.fxMenuSelection=0;state.fxMenuReturn=false;
    state.message = 'REC CAPTURES PERFORMANCE';
    state.messageTicks = 80;
    state.actionMessage = '';
    state.actionMessageTicks = 0;
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
    refreshEditorSnapshot();
    parseUiState(getParam('ui_state', ''));
    parsePlayheads(getParam('playheads', ''));
    parseWavesetState(getParam('waveset_state', ''));
    parseCutupsState(getParam('cutups_state', ''));
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
    draw();
};

globalThis.tick = function() {
    state.ticks++;
    if (state.ticks % 3 === 0) pollDsp();
    if (state.ticks % 180 === 0) setParam('maintenance', 1);
    if (state.messageTicks > 0) state.messageTicks--;
    if (state.actionMessageTicks > 0) state.actionMessageTicks--;
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
        const step = MoveSteps.indexOf(number);
        if (step >= 0) {
            if (pressed) {
                if (state.view === 'chain') selectChainSlot(step);
                else selectClipSlot(step);
            }
            return;
        }
        const pad = MovePads.indexOf(number);
        if (pad >= 0) {
            const owner=padSettings(pad),mode=owner.mode;
            if (pressed) {
                state.activePads.add(number);
                if (mode === 2) {
                    if(state.splitEnabled?Math.floor(pad/8)>0:pad>=8){
                        showMessage(state.splitEnabled?'CUES USE TOP 4':'CUES USE PADS 1-8');
                        rebuildLedQueue();
                        return;
                    }
                    const cue = state.splitEnabled?(pad%4)+(pad%8>=4?4:0):pad%8;
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
                            + ROLL_DIVISIONS[(state.splitEnabled?Math.floor(pad/8)*4+pad%4:pad) % ROLL_DIVISIONS.length]
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

    if (status === 0xA0 && MovePads.indexOf(number) >= 0) {
        // Raw pad pressure also reaches the DSP, where it is associated with
        // the exact polyphonic voice. Do not collapse it to a global value.
        return;
    }

    if (status !== 0xB0) return;
    if(number===MoveCopy&&value>0){
        if(state.shiftHeld&&state.splitEnabled){setParamBlocking('split_copy',1);showMessage('COPIED TO OTHER PART');}
        else toggleSplitMode();return;
    }
    if(number===MoveCapture&&state.splitEnabled){
        if(value>0){state.focusButtonHeld=true;state.focusButtonUsed=false;
            if(state.shiftHeld){state.focusButtonUsed=true;openSplitSends();}}
        else {if(!state.focusButtonUsed)setSplitFocus(1-state.splitFocus);state.focusButtonHeld=false;}
        return;
    }
    if(number===MoveMute&&value>0&&state.splitEnabled){
        parseSplitState(getParam('split_state','{}'));
        const muted=!(state.splitParts[state.splitFocus]?.mute);
        setParamBlocking('part_mute',muted?1:0);parseSplitState(getParam('split_state','{}'));showMessage(muted?'PART MUTED':'PART UNMUTED');return;
    }
    if(number===MoveKnob1+7&&state.splitEnabled&&state.focusButtonHeld){state.focusButtonUsed=true;adjustSplitBlend(decodeDelta(value));return;}
    if (number === MoveShift) {
        state.shiftHeld = value > 0;
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
    if (number === MoveDelete) {
        state.deleteHeld = value > 0;
        if (state.view === 'clips' || state.view === 'chain') return;
    }
    if (number === MovePlay && value > 0) {
        if (state.shiftHeld) toggleClipView();
        else toggleClipPlaying();
        return;
    }
    if (number === MoveLoop) {
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
        if (value > 0) {
            state.stackScanPaused = !state.stackScanPaused;
            setParamBlocking('stack_scan_paused', state.stackScanPaused ? 1 : 0);
            showMessage(state.stackScanPaused
                ? 'STACK SCAN HELD' : 'STACK SCAN RUNNING');
            rebuildLedQueue();
        }
        return;
    }
    if (value <= 0 && number !== MoveMainKnob
        && (number < MoveKnob1 || number > MoveKnob1 + 7)) return;

    const track = MoveTracks.indexOf(number);
    if (track >= 0 && value > 0) {
        if (state.view === 'clips') {
            state.clipBank = track;
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
    if ((number === MoveLeft || number === MoveRight) && value > 0
        && state.view === 'help') {
        state.helpPage = clamp(state.helpPage + (number === MoveRight ? 1 : -1), 0, 2);
        return;
    }
    if ((number === MoveLeft || number === MoveRight) && value > 0
        && (state.view === 'main' || state.view === 'settings')) {
        adjustClipBank(number === MoveRight ? 1 : -1);
        return;
    }
    if (number === MoveUp && value > 0) {
        if (state.view === 'fxMenu') moveSplitFxMenu(-1);
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
        if (state.view === 'fxMenu') moveSplitFxMenu(1);
        else if (state.view === 'help') state.helpPage = Math.min(2, state.helpPage + 1);
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
        if (state.view === 'fxMenu') moveSplitFxMenu(delta);
        else if (state.view === 'help' && delta !== 0) {
            state.helpPage = clamp(state.helpPage + (delta > 0 ? 1 : -1), 0, 2);
        }
        else if (state.view === 'settings' && currentSettingsPage().route) turnFxOrder(delta);
        else if (state.view === 'library') moveLibrarySelection(delta);
        else if (state.view === 'chain') adjustChainClip(delta);
        else if (delta !== 0) selectLane(state.lane + (delta > 0 ? 1 : -1));
        return;
    }
    if (number === MoveMainButton && value > 0) {
        if (state.view === 'fxMenu') activateSplitFxPage();
        else if (state.splitEnabled && state.view==='settings' && currentSettingsPage()===SPLIT_MIX_PAGE && !state.shiftHeld) openSplitFxMenu();
        else if (state.view === 'settings' && currentSettingsPage().route) clickFxOrder();
        else if (state.view === 'library') activateLibraryItem();
        else if (state.view === 'chain') {
            if (state.shiftHeld) {
                insertActiveClipInChain();
            } else {
                state.chainEnabled = !state.chainEnabled;
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
        if (state.view === 'main' || state.view === 'settings') {
            adjustControl(number - MoveKnob1, decodeDelta(value));
        }
        return;
    }
    if (number === MoveMenu && value > 0) {
        if (state.view === 'settings' && currentSettingsPage().route) cancelFxOrder(false);
        if (state.shiftHeld) {
            if (state.view === 'help') {
                state.view = 'main';
            } else {
                pollDiagnostics();
                state.helpPage = 0;
                state.view = 'help';
            }
        } else if (state.view === 'help' || state.view === 'fxMenu') {
            state.view = 'main';state.fxMenuReturn=false;
        } else {
            state.fxMenuReturn=false;
            state.view = state.view === 'settings' ? 'main' : 'settings';
        }
        return;
    }
    if (number === MoveBack && value > 0) {
        if (state.view === 'settings' && currentSettingsPage().route
            && (state.fxRouteGrabbed || state.fxRoutePending)) {
            cancelFxOrder();
            return;
        }
        if (state.shiftHeld && state.undoAvailable) {
            setParamBlocking('undo', 1);
            showMessage('UNDO');
        } else if (state.view==='fxMenu') {
            openSplitSends();
        } else if (state.view==='settings' && state.fxMenuReturn && state.splitEnabled) {
            openSplitFxMenu();
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
    if (state.recording) setParamBlocking('record_gate', 0);
    if (state.clipRecording) setParamBlocking('clip_record', 1);
    setParamBlocking('all_notes_off', 1);
    setParam('shift_held', 0);
    setParamBlocking('glitch_buffer_lock', 0);
    setButtonLED(MoveRecord, Black);
    if (MoveRecordAlt !== MoveRecord) setButtonLED(MoveRecordAlt, Black);
    setButtonLED(MovePlay, Black);
    setButtonLED(MoveLoop, Black);
};

