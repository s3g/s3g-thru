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
    locus: '0.42', field: '0.80', speed: '1.00', manual_lane: '1.60',
    lane_cycles: '0.50', stack_scan_paused: '0', algorithm: '1', join: '0.72',
    output_gain: '1.00', motion_mode: '5', motion_sound: '0', motion_rate: '4',
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
    cutups_mode: '5', sample_mode: '2', slice_region_mode: '1',
    slice_regions: '16', pad_mode: '0', lane_variation: '0',
    lane_1_gain: '1', lane_2_gain: '1', lane_3_gain: '1', lane_4_gain: '1',
    pressure_mode: '1', root_note: '60', record_lane: '2', pad_octave: '0',
    record_source: '0', record_gain: '1', monitor_level: '0', preview_level: '0.7',
    clip_rate: '1', clip_record_mode: '1', clip_timing: '1', launch_quantize: '1',
    tempo_bpm: '120', tempo_swing: '54', chain_enabled: '1', chain_slot: '2',
    chain_clip: '5', chain_repeats: '2',
    clip_state: '0,1,1,420,6800,6,128,1000,0,1,12000,13600',
    clip_take_state: '3,1,0',
    clip_capacity_state: '6,1024,128,8192,0,0',
    ui_state: '2,15,0,0,420,100,900,180,520,240,60,0,45,0,1,0,3',
    playheads: '190,510,790,-1,-1,-1,-1,-1',
    voice_lanes: '160,530,810,-1,-1,-1,-1,-1',
    voice_pads: '2,11,25,-1,-1,-1,-1,-1',
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
    undo_available: '1',
    performance_state: '724,0',
    last_error: '',
    waveforms: [0, 1, 2, 3].map(waveform).join('|'),
};

globalThis.host_module_get_param = (key) => params[key] ?? '';
globalThis.host_module_set_param = (key, value) => {
    if (Object.hasOwn(params, key)) params[key] = String(value);
};
globalThis.host_module_set_param_blocking = (key, value) => {
    if (Object.hasOwn(params, key)) params[key] = String(value);
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
function capture(filename, title) {
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

let selectedAlgorithm = Number(params.algorithm);
function selectAlgorithm(algorithm) {
    while (selectedAlgorithm < algorithm) {
        cc(71, 1);
        selectedAlgorithm++;
    }
    while (selectedAlgorithm > algorithm) {
        cc(71, 127);
        selectedAlgorithm--;
    }
    tick();
}

function captureEnginePages(algorithm, pages) {
    selectAlgorithm(algorithm);
    for (const page of pages) {
        moveSettingsPage(1);
        if (page.filename) capture(page.filename, page.title);
    }
    moveSettingsPage(-pages.length);
}

globalThis.init();
tick(82);
capture('waveform.svg', 'Waveform page with three polyphonic playheads');

params.clip_state = '0,0,1,420,6800,6,128,1000,0,1,12000,13600';
tick(3);
cc(50);
tick();
capture('engine-page.svg', 'Engine parameter page');

captureEnginePages(0, [
    { filename: 'stack-path.svg', title: 'Shared Stack Path parameter page' },
    { filename: 'motion-path.svg', title: 'Motion Path parameter page' },
    { filename: 'motion-sound.svg', title: 'Motion Sound parameter page' },
]);
moveSettingsPage(3);
cc(71, 1);
tick();
moveSettingsPage(1);
capture('motion-packets.svg', 'Motion Packets parameter page');
moveSettingsPage(-1);
cc(71, 1);
tick();
moveSettingsPage(1);
capture('motion-motor.svg', 'Motion Motor parameter page');
moveSettingsPage(-1);
cc(71, 1);
tick();
moveSettingsPage(1);
capture('motion-event-clock.svg', 'Motion Event Clock parameter page');
moveSettingsPage(1);
capture('motion-event-shape.svg', 'Motion Event Shape parameter page');
moveSettingsPage(-5);
captureEnginePages(1, [
    {},
    { filename: 'lanes-play.svg', title: 'Lanes Play parameter page' },
]);
captureEnginePages(2, [
    {},
    { filename: 'grains-main.svg', title: 'Grains Main parameter page' },
    { filename: 'grains-cloud.svg', title: 'Grains Cloud parameter page' },
    { filename: 'grains-process.svg', title: 'Grains Process parameter page' },
]);
captureEnginePages(3, [
    { filename: 'cutups-clock.svg', title: 'Cutups Clock parameter page' },
    { filename: 'cutups-path.svg', title: 'Cutups Path parameter page' },
]);
captureEnginePages(4, [
    { filename: 'direct-play.svg', title: 'Shared Direct Play parameter page' },
]);
captureEnginePages(5, [
    {},
    { filename: 'slice-source.svg', title: 'Slice Source parameter page' },
]);

selectAlgorithm(1);
moveSettingsPage(5);
capture('performance-page.svg', 'Performance Loop parameter page');

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

cc(51);
tick();
cc(49);
cc(50);
cc(49, 0);
tick();
capture('help.svg', 'On-device shortcut help');

if (checkOnly && mismatches !== 0) process.exit(1);
if (checkOnly) console.log('s3g Thru documentation screens: current');
