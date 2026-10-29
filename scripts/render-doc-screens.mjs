import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const outputDirectory = path.join(repoRoot, 'docs', 'assets', 'screens');
const checkOnly = process.argv.includes('--check');
let frame = [];

globalThis.__moveConstants = {
    MoveMainKnob: 14,
    MoveMainButton: 3,
    MoveShift: 49,
    MoveMenu: 50,
    MoveBack: 51,
    MoveUp: 55,
    MoveDown: 54,
    MoveLeft: 62,
    MoveRight: 63,
    MoveRec: 86,
    MoveRecord: 118,
    MovePlay: 85,
    MoveLoop: 58,
    MoveDelete: 119,
    MoveKnob1: 71,
    MovePads: Array.from({ length: 32 }, (_, index) => 68 + index),
    MoveSteps: Array.from({ length: 16 }, (_, index) => 16 + index),
    MoveRow1: 43,
    MoveRow2: 42,
    MoveRow3: 41,
    MoveRow4: 40,
};

globalThis.__moveInputFilter = {
    setLED() {},
    setButtonLED() {},
};

const library = {
    '/data/UserData/UserLibrary/Samples': [
        'Drums', 'Field Recordings', 'glass-loop.wav', 'radio-voice.wav',
    ],
    '/data/UserData/UserLibrary/Samples/Drums': [
        'break-93.wav', 'kick-soft.wav', 'metal-hit.wav',
    ],
    '/data/UserData/UserLibrary/Samples/Field Recordings': [
        'air-vent.wav', 'station.wav',
    ],
    '/data/s3g-thru/captures': [
        't0000000017-c01-l2-input.wav',
        't0000000018-c01-l2-normalize.wav',
    ],
};

globalThis.__os = {
    readdir(directory) {
        return [library[directory] || [], 0];
    },
    stat(target) {
        const directory = Object.hasOwn(library, target);
        return [{ mode: directory ? 0x4000 : 0x8000 }, 0];
    },
};

function nibble(value) {
    return Math.max(0, Math.min(14, value + 7)).toString(16);
}

function waveform(lane) {
    const laneEvents = [
        [
            { center: 5, width: 2, peak: 7 },
            { center: 31, width: 3, peak: 5 },
            { center: 70, width: 2, peak: 7 },
            { center: 105, width: 4, peak: 4 },
        ],
        [
            { center: 13, width: 7, peak: 4 },
            { center: 43, width: 5, peak: 7 },
            { center: 78, width: 8, peak: 5 },
        ],
        [
            { center: 3, width: 2, peak: 5 },
            { center: 28, width: 2, peak: 7 },
            { center: 35, width: 2, peak: 4 },
            { center: 67, width: 3, peak: 6 },
            { center: 106, width: 2, peak: 7 },
        ],
        [
            { center: 10, width: 4, peak: 6 },
            { center: 40, width: 2, peak: 4 },
            { center: 64, width: 6, peak: 7 },
            { center: 100, width: 3, peak: 5 },
        ],
    ];
    let encoded = '';
    for (let index = 0; index < 112; index++) {
        let amplitude = 0;
        for (const event of laneEvents[lane]) {
            const distance = Math.abs(index - event.center);
            if (distance > event.width) continue;
            const envelope = 1 - distance / (event.width + 0.5);
            amplitude = Math.max(amplitude, Math.round(event.peak * envelope));
        }
        encoded += nibble(-amplitude) + nibble(amplitude);
    }
    return encoded;
}

const params = {
    effect_reverb_algo: '0', effect_reverb_decay: '2.50', effect_reverb_pre: '18',
    effect_reverb_damp: '8000', effect_reverb_diff: '70', effect_reverb_mod: '10',
    effect_reverb_width: '100', effect_reverb_mix: '25', effect_order: '0',
    effect_order_state: '0,0,0',
    locus: '0.42', field: '0.80', speed: '1.00', manual_lane: '1.60',
    cut_lane_advance: '2', lane_cycles: '0.50', stack_scan_paused: '0', clip_stack_hold: '0',
    algorithm: '1', join: '0.72',
    output_gain: '1.00', polyphony_comp: '1', motion_mode: '5',
    motion_sound: '0', motion_rate: '4',
    motion_rate_basis: '0', motion_path_rate: '1.25', motion_step: '0.08',
    motion_seed: '23', motion_travel: '0.8', motion_jitter: '0.25',
    packet_duty: '65', motor_rate: '1.5', motor_symmetry: '58', motor_shape: '1',
    segment_trigger: '0', segment_overlap: '1', segment_layers: '3',
    segment_rate: '5', segment_repeats: '4', segment_step: '0.08',
    segment_pitch: '3', segment_level: '0.2', segment_curve: '0.1',
    lanes_mode: '2', lane_path: '2', lane_blend: '0', lane_path_shape: '1',
    lane_path_offset: '0.15', lane_path_skew: '0.50', lane_path_curve: '0.20',
    lane_slew: '20', lanes_stretch: '1', stretch_ratio: '1',
    grain_mode: '0', grain_size: '90', grain_density: '24', grain_quality: '1',
    grain_pitch: '1', grain_window: '5', grain_spread: '0.35',
    grain_process: '4', grain_process_amount: '0.75',
    long_grain_mode: '0', long_grain_length: '4000',
    long_grain_layers: '3', long_grain_pitch: '1', long_grain_shape: '3',
    long_grain_edge: '180', long_grain_spread: '0.65',
    long_grain_reverse: '20', long_grain_spray: '0.32',
    long_grain_bias: '1', long_grain_duration_variation: '0.28',
    long_grain_pitch_spray: '5', long_grain_level_variation: '0.18',
    long_grain_phase: '0.4', long_grain_advance: '1',
    cutups_mode: '5', sample_mode: '2', slice_region_mode: '1',
    slice_regions: '16', pad_mode: '0', scale: '1', lane_variation: '0',
    glitch_mode: '4', glitch_clock: '1', glitch_division: '3',
    glitch_rate: '8', glitch_slice: '32', glitch_repeats: '4',
    glitch_chance: '100', glitch_mix: '100', glitch_steps: '11',
    glitch_pulses: '5', glitch_rotate: '2', glitch_jump: '120',
    glitch_pitch: '7', glitch_reverse: '25', glitch_decay: '90',
    glitch_seed: '23', effect_tone: '-0.18', effect_drive: '0.22',
    effect_bits: '0.16', effect_rate: '2', effect_delay: '375',
    effect_feedback: '48', effect_width: '100', effect_mix: '28',
    effect_cross_pan: '-100', effect_rotary_rate: '0.8',
    effect_rotary_depth: '70', effect_rotary_mix: '35',
    effect_rotary_ramp: '350', effect_rotary_balance: '0',
    effect_rotary_crossover: '800', effect_rotary_spread: '100',
    effect_tape_format: '0', effect_tape_drive: '10',
    effect_tape_wow_rate: '0.18', effect_tape_wow_depth: '4',
    effect_tape_flutter_rate: '7', effect_tape_flutter_depth: '3',
    effect_tape_hiss: '-72', effect_tape_mix: '30',
    effect_tape_age: '18', effect_tape_crinkle: '8',
    effect_tape_mechanics: '14', effect_tape_inertia: '32',
    lane_1_gain: '1', lane_2_gain: '1', lane_3_gain: '1', lane_4_gain: '1',
    pressure_mode: '1', root_note: '60', record_lane: '2', pad_octave: '0',
    hold_release_ms: '0', shift_held: '0',
    amp_attack: '20', amp_decay: '300', amp_sustain: '62', amp_release: '750',
    record_source: '0', record_gain: '1', monitor_level: '0', preview_level: '0.7', preview_route:'0', save_status:'0', mix_status:'0,0,0,0,5,5',
    clip_rate: '1', clip_record_mode: '1', clip_timing: '1', launch_quantize: '1',
    tempo_bpm: '120', tempo_swing: '54', chain_enabled: '1', chain_slot: '2',
    chain_clip: '5', chain_repeats: '2',
    clip_state: '0,1,1,420,6800,6,128,1000,0,1,12000,13600',
    clip_take_state: '3,1,0',
    clip_capacity_state: '6,1024,128,8192,0,0',
    ui_state: '2,15,0,0,420,100,900,180,520,240,60,0,45,0,1,0,3',
    playheads: '190,510,790,-1,-1,-1,-1,-1',
    waveset_state: '', cutups_state: '',
    voice_lanes: '160,530,810,-1,-1,-1,-1,-1',
    voice_pads: '2,11,25,-1,-1,-1,-1,-1',
    voice_notes: '50,58,73,-1,-1,-1,-1,-1',
    roll_pads: '-1,-1,-1,-1,-1,-1,-1,-1',
    roll_notes: '-1,-1,-1,-1,-1,-1,-1,-1',
    clip_bank_state: '0,-1,37,0',
    sample_load_state: '0,2,0,',
    region_state: '5,0,180,410,680,1000',
    cue_state: '-1,-1,-1,-1,-1,-1,-1,-1',
    clip_prepare_state: '0,-1',
    chain_state: '1,1,0,1,-1',
    chain_entries: [
        '1:0:2', '1:4:2', '1:2:1', '1:8:4',
        '1:12:1', '0:0:1', '0:0:1', '0:0:1',
        '0:0:1', '0:0:1', '0:0:1', '0:0:1',
        '0:0:1', '0:0:1', '0:0:1', '0:0:1',
    ].join(','),
    lane_source: 'glass-loop.wav',
    lane_source_state: 'LIBRARY:glass-loop.wav',
    captures_directory: '/data/s3g-thru/captures',
    undo_available: '1',
    performance_state: '724,0',
    callback_performance_state: '28,16,760,0',
    limiter_state: '0.0,3.2',
    last_error: '',
    waveforms: [0, 1, 2, 3].map(waveform).join('|'),
};

globalThis.host_module_get_param = (key) => {
    if(key==='ui_state') {
        const values=params.ui_state.split(',');
        const split=params.split_state?JSON.parse(params.split_state):null;
        if(split?.enabled&&split.shared) {
            const width=Math.max(.02,Math.min(1,Number(params.field))),half=width*.5;
            const center=Math.max(half,Math.min(1-half,Number(params.locus)));
            const low=center-half,high=center+half;
            values[5]=String(Math.round(low*1000));values[6]=String(Math.round(high*1000));
            values[17]=String(low);values[18]=String(high);
        }
        if(!params.split_state||!JSON.parse(params.split_state).enabled) {
            const width=Number(params.field),half=width*.5;
            const center=Math.max(half,Math.min(1-half,Number(params.locus)));
            const low=center-half,high=center+half;
            values[5]=String(Math.round(low*1000));values[6]=String(Math.round(high*1000));
            values[17]=String(low);values[18]=String(high);
        }
        return values.join(',');
    }

    if(key==='ui_editor_state') {
        const values=Object.fromEntries(Object.entries(globalThis.__docSplit.params)
            .filter(([,p])=>!p.momentary).map(([key,p])=>[key,Number(params[key]??p.value)]));
        const split=JSON.parse(params.split_state||'{"enabled":0,"focus":0,"blend":0.5,"parts":[{},{}]}');
        values.split_mode=split.enabled?(split.quad?3:split.shared?2:1):0;values.split_focus=split.focus;
        values.part_mute=split.parts[split.focus]?.mute||0;
        for(const key of ['pad_octave','record_lane','stack_scan_paused','glitch_buffer_lock'])
            values[key]=Number(params[key]||0);
        return JSON.stringify({v:1,values,split,voices:params.voice_algorithms||'-1,-1,-1,-1,-1,-1,-1,-1'});
    }
    return params[key]??'';
};
globalThis.host_module_set_param = (key, value) => {
    if (Object.hasOwn(params, key)) params[key] = String(value);
};
globalThis.host_module_set_param_blocking = (key, value) => {
    if (Object.hasOwn(params, key)) params[key] = String(value);
    if(key==='split_focus'&&params.split_state) {
        const split=JSON.parse(params.split_state);split.focus=Number(value);
        const part=split.parts[split.focus];params.split_state=JSON.stringify(split);
        Object.assign(params,{algorithm:String(part.algorithm),root_note:String(part.root),
            pad_octave:String(part.octave),pad_mode:String(part.mode),scale:String(part.scale),
            ...(split.shared||split.quad?{locus:String((part.window[0]+part.window[1])*.5),field:String(part.window[1]-part.window[0])}:{}),
            record_lane:split.shared?params.record_lane:String(split.quad?split.focus+1:split.focus*2+1)});
        const ui=params.ui_state.split(',');ui[0]=params.record_lane;params.ui_state=ui.join(',');
    }
};
globalThis.host_exit_module = () => {};
globalThis.clear_screen = () => { frame = []; };
globalThis.print = (x, y, value, color) => {
    frame.push({ type: 'text', x, y, value: String(value), color });
};
globalThis.fill_rect = (x, y, width, height, color) => {
    frame.push({ type: 'rect', x, y, width, height, color });
};

let source = fs.readFileSync(path.join(repoRoot, 'src', 'ui.js'), 'utf8');
source = source.replace(
    /import \* as moveConstants[^;]+;/,
    'const moveConstants = globalThis.__moveConstants;');
source = source.replace(
    /import \* as moveInputFilter[^;]+;/,
    'const moveInputFilter = globalThis.__moveInputFilter;');
source = source.replace(
    /import \* as os[^;]+;/,
    'const os = globalThis.__os;');
source += '\nglobalThis.__docSplit = {state, params:PARAMS, pages:parameterPages, redraw:()=>{clearDisplay();draw();}};';
await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));

function escapeXml(value) {
    return String(value)
        .replaceAll('&', '&amp;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;')
        .replaceAll('"', '&quot;');
}

function svgFor(title) {
    const elements = frame.map((item) => {
        const fill = item.color === 0 ? '#050706' : '#dce8e1';
        if (item.type === 'rect') {
            return `    <rect x="${item.x}" y="${item.y}" width="${item.width}" height="${item.height}" fill="${fill}"/>`;
        }
        return `    <text x="${item.x}" y="${item.y + 5.5}" fill="${fill}">${escapeXml(item.value)}</text>`;
    }).join('\n');
    return `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="768" height="384" viewBox="0 0 128 64" role="img" aria-labelledby="title desc">
  <title id="title">${escapeXml(title)}</title>
  <desc id="desc">${escapeXml(title)} shown as it appears on Ableton Move.</desc>
  <rect width="128" height="64" fill="#050706"/>
  <g shape-rendering="crispEdges" font-family="DejaVu Sans Mono, IBM Plex Mono, monospace" font-size="6" font-style="normal" font-weight="400" text-rendering="geometricPrecision">
${elements}
  </g>
  <rect x="0.25" y="0.25" width="127.5" height="63.5" fill="none" stroke="#5b6661" stroke-width="0.5"/>
</svg>
`;
}

let mismatches = 0;
function capture(filename, title, expectedHeading = '') {
    if (expectedHeading) {
        const heading = frame.find((item) => item.type === 'text'
            && item.x === 1 && item.y === 0)?.value || '';
        if (heading !== expectedHeading
            && !heading.startsWith(expectedHeading + ' ')) {
            throw new Error(`Expected ${expectedHeading} while rendering `
                + `${filename}, found ${heading || 'no page heading'}`);
        }
    }
    const target = path.join(outputDirectory, filename);
    const contents = svgFor(title);
    if (checkOnly) {
        if (!fs.existsSync(target) || fs.readFileSync(target, 'utf8') !== contents) {
            console.error(`Documentation screen is stale: ${path.relative(repoRoot, target)}`);
            mismatches++;
        }
        return;
    }
    fs.mkdirSync(outputDirectory, { recursive: true });
    fs.writeFileSync(target, contents);
    console.log(`Rendered ${path.relative(repoRoot, target)}`);
}

function cc(number, value = 127) {
    globalThis.onMidiMessageInternal([0xB0, number, value]);
}

function tick(count = 1) {
    for (let index = 0; index < count; index++) globalThis.tick();
}

function moveSettingsPage(delta) {
    const control = delta > 0 ? 55 : 54;
    for (let index = 0; index < Math.abs(delta); index++) cc(control);
    tick();
}

function selectAlgorithm(algorithm) {
    const target = Math.max(0, Math.min(8, algorithm | 0));
    for (let guard = 0; guard < 64
        && Number(params.algorithm) !== target; guard++) {
        cc(71, Number(params.algorithm) < target ? 1 : 127);
        tick();
    }
    if (Number(params.algorithm) !== target) {
        throw new Error(`Could not select algorithm ${target}`);
    }
}

function selectCurrentIntegerParam(key, target, control = 71) {
    const wanted = target | 0;
    for (let guard = 0; guard < 128
        && Math.round(Number(params[key])) !== wanted; guard++) {
        cc(control, Number(params[key]) < wanted ? 1 : 127);
        tick();
    }
    if (Math.round(Number(params[key])) !== wanted) {
        throw new Error(`Could not set ${key} to ${wanted}`);
    }
}

function captureEnginePages(algorithm, pages) {
    selectAlgorithm(algorithm);
    moveSettingsPage(1); // Shared Pads & Scale page.
    moveSettingsPage(1); // Shared Amp Envelope page.
    for (const page of pages) {
        moveSettingsPage(1);
        if (page.filename) {
            capture(page.filename, page.title, page.heading);
        }
    }
    moveSettingsPage(-(pages.length + 2));
}

globalThis.init();
tick(82);
capture('waveform.svg', 'Waveform page with three polyphonic playheads');

const wideWaveformFixture = {
    locus: params.locus, field: params.field, playheads: params.playheads,
};
Object.assign(params, {
    locus: '0.55', field: '0.22',
    playheads: '480,550,620,-1,-1,-1,-1,-1',
});
tick(3);
capture('waveform-zoom.svg', 'Single waveform zoom at Position 0.55 and Field 0.22');
Object.assign(params, wideWaveformFixture);
tick(3);
cc(52);cc(50);cc(50,0);cc(52,0);tick();
capture('pages.svg','Contextual Pages picker');cc(51);tick();
params.save_status='2';tick(3);capture('save-pending.svg','Waveform with saving status');
params.save_status='3';tick(3);capture('save-complete.svg','Waveform with saved confirmation');
params.save_status='0';tick(3);

params.algorithm = '3';
params.cutups_state = '0,0,190,-1,-1000,1000,0;1,2,510,1,440,800,400;2,3,790,-1,-1000,600,0';
const ordinaryUiState = params.ui_state;
params.ui_state = '2,15,0,0,420,100,900,1000,400,800,600,0,45,0,1,0,3';
tick(3);
capture('cutups-waveform.svg',
    'Cutups waveform page with shared playhead lines and actual-lane stack markers');
params.cutups_state = '';
params.ui_state = ordinaryUiState;

params.algorithm = '8';
params.waveset_state = [
    '0,8,5,0,2,1,1,0,620,297,-1,310,-1,0',
    '1,8,3,1,2,1,1,10,350,581,693,690,510,30',
].join(';');
tick(3);
capture('wavesets-waveform.svg',
    'Wavesets waveform page with ordered cycle rail and source cursors');
params.algorithm = '1';
params.waveset_state = '';
tick(3);

params.clip_state = '0,0,1,420,6800,6,128,1000,0,1,12000,13600';
tick(3);
cc(50);
tick();
capture('engine-page.svg', 'Engine parameter page', 'ENGINE');
moveSettingsPage(1);
capture('pads-scale.svg', 'Shared Pads and Scale parameter page',
    'PADS & SCALE');
moveSettingsPage(1);
capture('amp-envelope.svg', 'Shared breakpoint amplitude envelope page',
    'AMP ENVELOPE');
moveSettingsPage(-1);
moveSettingsPage(-1);

captureEnginePages(0, [
    { filename: 'stack-path.svg', title: 'Shared Stack Path parameter page', heading: 'STACK PATH' },
    { filename: 'motion-path.svg', title: 'Motion Path parameter page', heading: 'MOTION PATH' },
    { filename: 'motion-sound.svg', title: 'Motion Sound parameter page', heading: 'MOTION SOUND' },
]);
moveSettingsPage(5);
selectCurrentIntegerParam('motion_sound', 1);
moveSettingsPage(1);
capture('motion-packets.svg', 'Motion Packets parameter page', 'PACKETS');
moveSettingsPage(-1);
selectCurrentIntegerParam('motion_sound', 2);
moveSettingsPage(1);
capture('motion-motor.svg', 'Motion Motor parameter page', 'MOTOR');
moveSettingsPage(-1);
selectCurrentIntegerParam('motion_sound', 3);
moveSettingsPage(1);
capture('motion-event-clock.svg', 'Motion Event Clock parameter page',
    'EVENT CLOCK');
moveSettingsPage(1);
capture('motion-event-shape.svg', 'Motion Event Shape parameter page',
    'EVENT SHAPE');
moveSettingsPage(-7);
captureEnginePages(1, [
    {},
    { filename: 'lanes-play.svg', title: 'Lanes Play parameter page', heading: 'LANES PLAY' },
]);
captureEnginePages(2, [
    {},
    { filename: 'grains-main.svg', title: 'Grains Main parameter page', heading: 'GRAINS MAIN' },
    { filename: 'grains-cloud.svg', title: 'Grains Cloud parameter page', heading: 'GRAINS CLOUD' },
    { filename: 'grains-process.svg', title: 'Grains Process parameter page', heading: 'GRAINS PROCESS' },
]);
captureEnginePages(3, [
    { filename: 'cutups-clock.svg', title: 'Cutups Clock parameter page', heading: 'CUTUPS CLOCK' },
    { filename: 'cutups-path.svg', title: 'Cutups Path parameter page', heading: 'CUTUPS PATH' },
]);
capture('cutups-engine.svg', 'Cutups Engine page with physical Lane and cut-count Advance', 'ENGINE');
captureEnginePages(4, [
    { filename: 'direct-play.svg', title: 'Shared Direct Play parameter page', heading: 'DIRECT PLAY' },
]);
captureEnginePages(5, [
    {},
    { filename: 'slice-source.svg', title: 'Slice Source parameter page', heading: 'SLICE SOURCE' },
]);
captureEnginePages(6, [
    {},
    { filename: 'glitch-clock.svg', title: 'Glitch Clock parameter page', heading: 'GLITCH CLOCK' },
    { filename: 'glitch-pattern.svg', title: 'Glitch Pattern parameter page', heading: 'GLITCH PATTERN' },
]);
captureEnginePages(7, [
    {},
    { filename: 'weave.svg', title: 'Weave parameter page', heading: 'WEAVE' },
    { filename: 'weave-motion.svg', title: 'Weave Motion parameter page', heading: 'WEAVE MOTION' },
]);
captureEnginePages(8, [
    {},
    { filename: 'wavesets.svg', title: 'Wavesets parameter page', heading: 'WAVESETS' },
]);

selectAlgorithm(1);
moveSettingsPage(6);
capture('effects.svg', 'Effects parameter page', 'EFFECTS');
moveSettingsPage(1);
capture('effects-stereo-rotary.svg', 'Stereo and Rotary effects parameter page',
    'STEREO / ROTARY');
moveSettingsPage(1);
capture('effects-tape.svg', 'Tape effects parameter page', 'TAPE');
moveSettingsPage(1);
capture('effects-reverb.svg', 'Reverb parameter page', 'REVERB');
moveSettingsPage(1);
capture('effects-order.svg', 'Effects order list', 'FX ORDER');
for (let step = 0; step < 5; ++step) cc(14, 1);
cc(3);
for (let step = 0; step < 3; ++step) cc(14, 127);
tick();
capture('effects-order-move.svg', 'Moving Reverb before Delay', 'FX ORDER');
cc(51);
moveSettingsPage(2);
capture('performance-page.svg', 'Performance Loop parameter page', 'PERF LOOP');

cc(51);
tick();
cc(49);
cc(85);
cc(49, 0);
tick();
capture('clips.svg', 'Performance clip bank overview');

cc(49);
cc(85);
cc(49, 0);
tick();
capture('chain.svg', 'Performance clip chain editor');

cc(49);
cc(85);
cc(49, 0);
tick();
cc(49);
cc(42);
cc(49, 0);
tick();
capture('library.svg', 'Sample library browser');
params.preview_route='1';tick(3);capture('library-direct.svg','Sample browser with Direct audition');params.preview_route='0';tick(3);

cc(51);
tick();
cc(49);
cc(50);
cc(49, 0);
tick();
capture('help.svg', 'On-device system performance report');
cc(63);
tick();
capture('help-callbacks.svg', 'On-device callback work report');
cc(63);
tick();
capture('help-shortcuts.svg', 'On-device shortcut help');

// Split examples use the same live draw code and parameter pages as the device.
Object.assign(params, {
    algorithm:'4', record_lane:'1', manual_lane:'1.4',
    split_blend:'0.5', split_level_a:'1', split_level_b:'0.85',
    split_delay_a:'0.2', split_delay_b:'0.45', split_reverb_a:'0.6', split_reverb_b:'0.3', split_curve:'1',
    effect_order:'7', effect_order_state:'7,7,0', effect_mix:'100',effect_reverb_mix:'100',
    split_state:JSON.stringify({enabled:1,focus:0,blend:.5,parts:[
        {algorithm:4,root:60,octave:0,scale:1,mode:0,hold:0,mute:0,position:.25,window:[.1,.5]},
        {algorithm:2,root:67,octave:0,scale:1,mode:0,hold:1,mute:0,position:.82,window:[.55,.95]}]}),
    playheads:'250,-1,-1,-1,820,-1,-1,-1',
    voice_lanes:'200,-1,-1,-1,750,-1,-1,-1',
    voice_algorithms:'4,-1,-1,-1,2,-1,-1,-1',
    voice_pads:'1,-1,-1,-1,20,-1,-1,-1',
    voice_notes:'50,-1,-1,-1,69,-1,-1,-1',
    clip_state:'0,0,1,420,6800,6,128,1000,0,1,12000,13600',
    ui_state:'1,15,0,0,250,100,900,1000,0,1000,0,0,45,0,1,0,2',mix_status:'.12,.08,1,1,0,0'
});
globalThis.init();tick(82);
capture('split-waveform.svg','Split waveform with A focused and independent A/B zoom');
cc(52);cc(52,0);tick();capture('split-waveform-b.svg','Split waveform with B focused');
cc(52);cc(52,0);tick();
const mutedExample=JSON.parse(params.split_state);
mutedExample.parts[1].mute=1;params.split_state=JSON.stringify(mutedExample);tick(3);
capture('split-muted.svg','Split with B muted and an unmute hint');
params.mix_status='.12,0,1,1,0,1';globalThis.__docSplit.state.view='mixStatus';tick(3);
capture('split-mix-status.svg','Split activity and muted B status');
globalThis.__docSplit.state.view='main';params.mix_status='.12,.08,1,1,0,0';
mutedExample.parts[1].mute=0;params.split_state=JSON.stringify(mutedExample);tick(3);

const splitExamples=[
    ['FX SENDS','split-mix.svg','Split FX Sends'],
    ['ENGINE','split-engine.svg','A Engine and Part Level'],
    ['DUAL SETUP','split-setup.svg','Dual Setup'],
    ['PART COLOR','split-color.svg','A Part Color'],
    ['DELAY RETURN','split-delay.svg','Shared Delay Return'],
    ['REVERB','split-reverb.svg','Shared Reverb parameters'],
    ['FX ORDER','split-order.svg','Shared Split FX Order']
];
for(const [heading,filename,title] of splitExamples) {
    const ui=globalThis.__docSplit;
    ui.state.view='settings';ui.state.settingsPage=ui.pages().findIndex((page)=>page.title===heading);
    if(ui.state.settingsPage<0)throw new Error('Missing Split page '+heading);
    tick();capture(filename,title);
}

const fxDoc=globalThis.__docSplit;
fxDoc.state.view='settings';fxDoc.state.settingsPage=fxDoc.pages().findIndex((page)=>page.title==='FX SENDS');
cc(3);tick();capture('split-fx-menu.svg','Split FX Pages menu');
cc(51);params.effect_mix='0';params.effect_reverb_mix='0';
cc(49);cc(52);cc(52,0);cc(49,0);tick();
capture('split-sends-off.svg','Split sends with return-level hints');

// Shared Dual zoom follows the focused program across all four source rows.
const sharedExample=JSON.parse(params.split_state);
sharedExample.shared=1;sharedExample.focus=0;
params.split_state=JSON.stringify(sharedExample);
params.locus=String((sharedExample.parts[0].window[0]+sharedExample.parts[0].window[1])*.5);
params.field=String(sharedExample.parts[0].window[1]-sharedExample.parts[0].window[0]);
params.effect_mix=params.effect_reverb_mix='100';
globalThis.init();tick(82);
capture('shared-waveform.svg','Shared Dual with A focused and its live source zoom');
cc(52);cc(52,0);tick(3);
if(Math.abs(globalThis.__docSplit.state.fieldStart-.55)>.00001
    ||Math.abs(globalThis.__docSplit.state.fieldEnd-.95)>.00001)
    throw new Error('Shared B focus must update the live source zoom');
globalThis.__docSplit.redraw();
capture('shared-waveform-b.svg','Shared Dual with B focused and its own live source zoom');
const sharedDoc=globalThis.__docSplit;
sharedDoc.state.view='settings';sharedDoc.state.settingsPage=sharedDoc.pages().findIndex(p=>p.title==='MIX / REC');
tick();capture('shared-mix-rec.svg','Shared Dual B lane gains and common recording controls');
sharedDoc.state.view='mode';sharedDoc.state.modeSelection=2;
tick();capture('instrument-mode.svg','Instrument mode chooser with Shared Dual selected');

const quadExample={enabled:1,shared:0,quad:1,focus:0,blend:.5,parts:[
    {algorithm:4,root:60,octave:0,scale:1,mode:0,hold:0,mute:0,position:.3,window:[.1,.6]},
    {algorithm:2,root:62,octave:0,scale:1,mode:0,hold:0,mute:0,position:.65,window:[.4,.9]},
    {algorithm:3,root:64,octave:0,scale:1,mode:0,hold:0,mute:0,position:.2,window:[0,.5]},
    {algorithm:8,root:67,octave:0,scale:1,mode:0,hold:0,mute:0,position:.8,window:[.5,1]}]};
Object.assign(params,{split_state:JSON.stringify(quadExample),algorithm:'4',record_lane:'1',locus:'.35',field:'.5',
    split_level_c:'1',split_level_d:'.85',split_delay_c:'.25',split_reverb_c:'.4',split_delay_d:'.1',split_reverb_d:'.5',
    voice_algorithms:'4,-1,2,-1,3,-1,8,-1',playheads:'300,-1,650,-1,200,-1,800,-1',
    voice_lanes:'0,-1,333,-1,667,-1,1000,-1',voice_pads:'16,-1,20,-1,0,-1,4,-1',
    voice_notes:'48,-1,50,-1,52,-1,55,-1',cutups_state:'4,2,200,-1,-1000,1000,0',
    waveset_state:'6,2,0,0,1,0,0,0,500,500,-1,800,-1,16',mix_status:'.12,.08,.1,.06,1,1,1,1,0,0,0,0'});
globalThis.init();tick(82);capture('quad-waveform.svg','Quad waveform with one source lane per instrument');
const quadDoc=globalThis.__docSplit;
cc(52);cc(52,0);cc(52);cc(52,0);tick(3);quadDoc.redraw();
if(quadDoc.state.splitFocus!==2||quadDoc.state.lane!==3)throw new Error('Quad C focus must select lane 3');
capture('quad-waveform-c.svg','Quad waveform with C focused and all source windows retained');
cc(52);cc(52,0);cc(52);cc(52,0);tick(3);
quadDoc.state.view='mode';quadDoc.state.modeSelection=3;tick();capture('quad-mode.svg','Instrument Mode with Quad selected');
for(const [heading,file] of [['FX SENDS','quad-sends.svg'],['QUAD SETUP','quad-setup.svg'],['ENGINE','quad-engine.svg'],['DELAY RETURN','quad-delay.svg'],['REVERB','quad-reverb.svg']]) {
    quadDoc.state.view='settings';quadDoc.state.settingsPage=quadDoc.pages().findIndex(p=>p.title===heading);
    tick();capture(file,'Quad '+heading);
}
quadDoc.state.view='mixStatus';tick();capture('quad-status.svg','Quad activity and voice counts');

if (checkOnly && mismatches !== 0) process.exit(1);
if (checkOnly) console.log('s3g Thru documentation screens: current');
