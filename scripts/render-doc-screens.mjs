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
    callback_performance_state: '28,16,760,0',render_timing_state:'1,720,110,0',pattern_worker_state:'1,1,910,780,0,0,0,42.3,-10,46.4,160',
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

    if(key==='ui_editor_state'||key.startsWith('ui_editor_chunk:')) {
        const values=Object.fromEntries(Object.entries(globalThis.__docSplit.params)
            .filter(([,p])=>!p.momentary).map(([key,p])=>[key,Number(params[key]??p.value)]));
        const split=JSON.parse(params.split_state||'{"enabled":0,"focus":0,"blend":0.5,"parts":[{},{}]}');
        values.split_mode=split.bank?4:split.enabled?(split.quad?3:split.shared?2:1):0;values.split_focus=split.focus;
        values.part_mute=split.parts[split.focus]?.mute||0;
        for(const key of ['pad_octave','record_lane','stack_scan_paused','glitch_buffer_lock'])
            values[key]=Number(params[key]||0);
        const full={v:1,values,split,voices:params.voice_algorithms||'-1,-1,-1,-1,-1,-1,-1,-1'};
        if(key==='ui_editor_state')return JSON.stringify(full);
        const rows=Object.entries(values),chunk=Number(key.slice(16));
        return JSON.stringify({...full,chunk,chunks:Math.ceil(rows.length/64),
            owner:values.split_mode*4+split.focus,clip:parseInt(params.clip_bank_state||'0',10),
            values:Object.fromEntries(rows.slice(chunk*64,(chunk+1)*64))});
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

globalThis.__thruMidi=await import('../src/midi-map.mjs');
let source = fs.readFileSync(path.join(repoRoot, 'src', 'ui.js'), 'utf8');
source = source.replace(/import \* as thruMidi[^;]+;/,'const thruMidi=globalThis.__thruMidi;');
source = source.replace(
    /import \* as moveConstants[^;]+;/,
    'const moveConstants = globalThis.__moveConstants;');
source = source.replace(
    /import \* as moveInputFilter[^;]+;/,
    'const moveInputFilter = globalThis.__moveInputFilter;');
source = source.replace(
    /import \* as os[^;]+;/,
    'const os = globalThis.__os;');
source += '\nglobalThis.__docSound={read:readSequenceSound,model:sequenceSoundUi};';
source += '\nglobalThis.__docSequence={read:readSequence,model:sequenceUi};globalThis.__docGenerate={ui:sequenceGeneratorUi,read:readSequenceGenerator};';
source += '\nglobalThis.__docLayers={read:readLayers,model:layersUi,openPicker:openLayersPicker,picker:layersPicker};';
source += '\nglobalThis.__docSplit = {state, readMixerState, projectUi, readProject, openProjects, openBankRecovery, customMidiUi, readCustomMidi, openCustomMidiList, sliceUi, openSliceEditor, readSliceEditor, params:PARAMS, pages:parameterPages, redraw:()=>{clearDisplay();draw();}};';
source += '\nglobalThis.__docLfo={read:readControlLfos,readLive:readControlLfoLive,draw:drawControlLfo,drawSettings,pages:LFO_PAGES,setup:LFO_SETUP_PAGE,trig:SEQUENCE_LFO_PAGE,model:lfoUi,openPicker:openLfoDestPicker,drawPicker:drawLfoDestPicker,groups:lfoDestGroups,catalog:LFO_DESTINATIONS};';
source += '\nglobalThis.__docSynth={ui:synthUi,read:readSynth,draw:drawSynth};';
source += '\nglobalThis.__docFm={read:readFm,model:fmUi};';
source += '\nglobalThis.__docScanned={read:readScanned,model:scannedUi};';
source += '\nglobalThis.__docTable={read:readTable,scopes:readTableScope,model:tableUi};';
source += '\nglobalThis.__docReadout={read:readPlaybackReadout,model:playbackReadout};';
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
    if(frame.some(item=>item.type==='text'&&item.value==='UPDATING EDITOR'))
        throw new Error('Editor snapshot is incomplete for '+filename);
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
    // Documentation fixtures address the global parameter array directly.
    // Runtime Up/Down stays within the current navigation category.
    const doc=globalThis.__docSplit,pages=doc.pages();
    doc.state.settingsPage=(doc.state.settingsPage+delta%pages.length+pages.length)%pages.length;
    doc.state.actionMessageTicks=0;
    doc.redraw();
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
    moveSettingsPage(1); // Shared MIDI Control page.
    moveSettingsPage(1); // Shared MIDI Notes page.
    moveSettingsPage(1); // Shared MIDI Maps page.
    moveSettingsPage(1); // Shared Amp Envelope page.
    moveSettingsPage(1); // Shared Play Gain page.
    for (const page of pages) {
        moveSettingsPage(1);
        if (page.filename) {
            capture(page.filename, page.title, page.heading);
        }
    }
    moveSettingsPage(-(pages.length + 6));
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
capture('pages.svg','Scrolling Pages hub with working banks and position counter');
const hubDoc=globalThis.__docSplit;hubDoc.state.pageSelection=8;hubDoc.redraw();capture('pages-projects.svg','Pages hub with Projects selected at 9 of 9','PAGES');
cc(51);tick();
cc(51);tick();capture('exit-confirm.svg','Exit confirmation with Keep Playing selected','EXIT s3g Thru?');cc(51);tick();
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
capture('midi-control.svg', 'Selectable MIDI controller mode page',
    'MIDI CONTROL');
moveSettingsPage(1);
capture('midi-notes.svg', 'Keyboard and sequencer MIDI Notes page', 'MIDI NOTES');
moveSettingsPage(1); // MIDI Maps is captured with a populated profile below.
moveSettingsPage(1);
capture('amp-envelope.svg', 'Shared breakpoint amplitude envelope page',
    'AMP ENVELOPE');
moveSettingsPage(-5);

// Engine List is an optional page; restore ONE before the standard engine examples.
{
    const doc=globalThis.__docSplit,oldPage=doc.state.settingsPage,oldAlgorithm=doc.state.algorithm,oldAlgorithmValue=doc.params.algorithm.value;
    doc.params.voice_engine_mode.value=1;doc.params.voice_engine_edit.value=2;doc.state.algorithm=2;doc.params.algorithm.value=2;
    [4,2,7,3,8,9,10,12].forEach((a,i)=>doc.params['voice_engine_'+(i+1)].value=a);
    doc.state.settingsPage=doc.pages().findIndex(p=>p.voiceEngines);doc.redraw();
    capture('voice-engines.svg','Engine List with eight assignments and entry 2 selected','VOICE ENGINES');
    Object.assign(doc.params.voice_engine_octave,{value:-1});doc.params.voice_engine_assign.value=1;
    doc.params.voice_engine_position.value=25;doc.params.voice_engine_stack.value=20;doc.params.voice_engine_pan.value=-60;doc.params.voice_engine_depth.value=75;
    doc.state.settingsPage=doc.pages().findIndex(p=>p.voiceSetup);doc.redraw();
    capture('voice-setup.svg','Voice Setup for entry 2 with keyboard assignment and launch variations','E2 VOICE SETUP');
    doc.params.voice_engine_assign.value=0;doc.params.voice_engine_octave.value=0;doc.params.voice_engine_position.value=0;doc.params.voice_engine_stack.value=0;doc.params.voice_engine_pan.value=0;doc.params.voice_engine_depth.value=100;

    doc.params.voice_engine_mode.value=0;doc.params.voice_engine_edit.value=1;doc.state.settingsPage=oldPage;doc.state.algorithm=oldAlgorithm;doc.params.algorithm.value=oldAlgorithmValue;
}

captureEnginePages(0, [
    { filename: 'stack-path.svg', title: 'Shared Stack Path parameter page', heading: 'STACK PATH' },
    { filename: 'motion-path.svg', title: 'Motion Path parameter page', heading: 'MOTION PATH' },
    { filename: 'motion-sound.svg', title: 'Motion Sound parameter page', heading: 'MOTION SOUND' },
]);
moveSettingsPage(9);
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
moveSettingsPage(-11);
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
    { filename: 'sample-play.svg', title: 'Sample Direct Play with stretch controls', heading: 'DIRECT PLAY' },
]);
captureEnginePages(5, [
    { filename: 'direct-play.svg', title: 'Slices Direct Play parameter page', heading: 'DIRECT PLAY' },
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
globalThis.__docSplit.state.settingsPage=globalThis.__docSplit.pages().findIndex(p=>p.title==='EFFECTS');globalThis.__docSplit.redraw();
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
capture('help-timing.svg','On-device render CPU and wait report','RENDER TIMING');
cc(63);tick(3);
capture('help-shortcuts.svg', 'On-device shortcut help');
cc(63);tick(3);
capture('help-e16.svg','On-device E16 connection and recovery report','E16 LINK');
cc(63);cc(63);cc(63);tick(3);
capture('help-fade.svg','Outgoing fade audio worker timing and buffer report','FADE AUDIO');

// Split examples use the same live draw code and parameter pages as the device.
Object.assign(params, {
    algorithm:'4', record_lane:'1', manual_lane:'1.4',
    split_blend:'0.5', split_level_a:'1', split_level_b:'0.85',
    split_delay_a:'0.2', split_delay_b:'0.45', split_reverb_a:'0.6', split_reverb_b:'0.3', split_curve:'1',
    split_priority:'-12',split_duck_threshold:'-24',split_duck_attack:'12',
    split_duck_hold:'40',split_duck_release:'220',split_duck_hpf:'100',
    split_duck_knee:'6',split_duck_route:'1',
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
    ['PRIORITY DUCK','split-priority.svg','Shared Priority Duck'],
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
params.mixer_state=JSON.stringify({mode:1,count:2,layers:0,strips:[[0,0,0,900,0,0],[-2,1,0,1800,0,0],[0,0,0,900,0,0],[0,0,0,900,0,0]],pan:[-.25,.25,0,0],duck:[0,0,-24,12,40,220,100,6]});
fxDoc.readMixerState();fxDoc.state.view='settings';
fxDoc.state.settingsPage=fxDoc.pages().findIndex(p=>p.title==='MIXER');fxDoc.redraw();capture('output-mixer.svg','Dual-mode output mixer with levels and balances','MIXER');
fxDoc.state.settingsPage=fxDoc.pages().findIndex(p=>p.title==='STRIP B');fxDoc.redraw();capture('output-strip.svg','Part B output EQ, Solo and Mute','STRIP B');
delete params.mixer_state;
fxDoc.state.view='settings';fxDoc.state.settingsPage=fxDoc.pages().findIndex((page)=>page.title==='FX SENDS');
cc(3);tick();capture('split-fx-menu.svg','Split FX Pages menu');
cc(51);params.effect_mix='0';params.effect_reverb_mix='0';
fxDoc.state.view='settings';fxDoc.state.settingsPage=fxDoc.pages().findIndex(p=>p.title==='FX SENDS');
fxDoc.params.effect_mix.value=0;fxDoc.params.effect_reverb_mix.value=0;fxDoc.redraw();
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

// Temporary performance controls on the bottom eight hardware pads.
Object.assign(quadDoc.state,{splitEnabled:false,sharedDual:false,quadMode:false,
    performanceVisible:true,performanceOwner:0,mask:15});
quadDoc.state.performancePads=new Set([69,72]);quadDoc.state.performancePadLive=new Set([69,72]);
quadDoc.state.performanceValues=[0,0.2,0,0,0.8,0,0,0];
quadDoc.redraw();capture('capture-performance.svg','Capture lane rides and momentary effects');

// Compact source names retain the part letter in a four-character value.
Object.assign(quadDoc.state,{splitEnabled:true,sharedDual:false,quadMode:true,
    performanceVisible:false,view:'settings',recordSource:5});
quadDoc.state.settingsPage=quadDoc.pages().findIndex(p=>p.title==='MIX / REC');
quadDoc.params.record_source.value=5;quadDoc.redraw();
capture('part-capture-source.svg','Mix and Record with Part D selected');

// A closed six-point route with two pauses and unequal travel times.
params.algorithm='1';params.lane_path='8';
params.split_state=JSON.stringify({enabled:0,shared:0,quad:0,focus:0,blend:.5,parts:[{algorithm:1},{algorithm:1}]});
const authoredTimes=[0,.15,.35,.55,.7,.9,.94,.97];
const authoredLanes=[0,0,.75,.75,.25,1,.5,.25];
const authoredCurves=[0,.8,0,-.5,.6,-.8,0,0];
params.stack_path_state=[6,2,...authoredTimes.flatMap((time,i)=>[time,authoredLanes[i],authoredCurves[i]])].join(',');
globalThis.init();tick(82);
const pathDoc=globalThis.__docSplit;pathDoc.state.view='settings';
pathDoc.state.settingsPage=pathDoc.pages().findIndex(p=>p.title==='STACK PATH');
pathDoc.redraw();capture('stack-path-draw.svg','Stack Path with DRAW selected');
cc(3);tick();capture('stack-path-editor.svg','Editable path with six points and pauses');

// A paused blend between independent performance clips.
params.pattern_fade_state='1,1,0,1,5000,30000,3,0,0,2000,12000,30000,0';tick(3);
pathDoc.state.view='settings';pathDoc.state.settingsPage=pathDoc.pages().findIndex(p=>p.patternFade);
pathDoc.redraw();capture('pattern-fade.svg','Pattern Fade held at a smooth midpoint');
params.pattern_fade_state='1,1,0,1,5000,30000,3,0,1,2000,12000,6374,0';tick(3);
pathDoc.redraw();capture('pattern-fade-random.svg','Random Pattern Fade with a chosen duration and low/high bounds');

// Stored chain defaults and a destination entry override.
params.chain_effective_fade='1,1,8000,2000,12000,3';params.entry_effective_fade='2,0,8000,2000,12000,3';params.chain_state='1,0,1,1,2,0,2,0';params.chain_end='2';params.chain_fades_state='2,8000,2000,12000,3,2,8000,2000,12000,3';
tick(3);pathDoc.state.view='settings';pathDoc.state.settingsPage=pathDoc.pages().findIndex(p=>p.title==='CHAIN');
pathDoc.state.actionMessageTicks=0;pathDoc.redraw();capture('chain-controls.svg','Saved Chain controls with Bank, End and Next');
pathDoc.state.settingsPage=pathDoc.pages().findIndex(p=>p.chainFade==='chain');
pathDoc.redraw();capture('chain-fade.svg','Chain 01 with saved random fade defaults');
pathDoc.state.settingsPage=pathDoc.pages().findIndex(p=>p.chainFade==='entry');
pathDoc.redraw();capture('entry-fade.svg','Entry 02 with a saved fixed smooth fade');

// Chain copy/launch with full-width action labels and encoder numbers.
params.chain_state='1,0,1,1,2,0,2,0,0,-1,-1';params.chain_effective_fade='0,1,0,2000,12000,3';params.entry_effective_fade='2,0,8000,0,0,3';
pathDoc.state.view='settings';pathDoc.state.settingsPage=pathDoc.pages().findIndex(p=>p.chainTools);
pathDoc.state.actionMessageTicks=0;pathDoc.redraw();capture('chain-tools.svg','Chain and Entry Copy/Paste with numbered encoders and wider text spacing');

for(const [algorithm,title,filename] of [[9,'PRISM','prism'],[10,'MORPH','morph'],[11,'TEXTURE','texture'],[12,'FORMANT','formant'],[13,'STOCHASTIC','stochastic'],[14,'PULSAR','pulsar']]) {
    Object.assign(params,{algorithm:String(algorithm),prism_mode:'4',texture_mode:'2',pattern_fade_state:'0,0,-1,-1,0,0,0,0,0,0,0,0,0',
        spectral_analysis_state:'15,0',morph_pair:'1>2',split_state:JSON.stringify({enabled:0,shared:0,quad:0,focus:0,parts:Array.from({length:2},()=>({algorithm,root:60,octave:0,scale:0,mode:0,mute:0}))})});
    if(algorithm===14)Object.assign(params,{pulsar_rate:'261.625565',pulsar_form:'500',pulsar_size:'4',pulsar_window:'0',pulsar_skew:'50',pulsar_omit:'0',pulsar_burst:'0',pulsar_jitter:'0',pulsar_source_hold:'0'});
    if(algorithm===13)Object.assign(params,{stochastic_source_hold:'0',stochastic_size:'2',stochastic_amplitude:'65',stochastic_timing:'70',stochastic_evolution:'100',stochastic_memory:'80',stochastic_pull:'10',stochastic_law:'2',stochastic_drift:'0'});
    if(algorithm===12)Object.assign(params,{formant_tune:'100',formant_freeze:'1',formant_keytrack:'0',formant_rough:'0',formant_sub:'0'});
    globalThis.init();tick(82);const ui=globalThis.__docSplit;ui.state.view='settings';
    ui.state.settingsPage=ui.pages().findIndex(p=>p.title===title);if(ui.state.settingsPage<0)throw new Error('Missing '+title+' page');ui.state.actionMessageTicks=0;ui.redraw();capture(filename+'.svg',title+' engine parameters',title);
}

Object.assign(params,{algorithm:'14',pulsar_emit_mod_rate:'0.13',pulsar_emit_mod_depth:'6',pulsar_form_mod_rate:'1.7',pulsar_form_mod_depth:'12',pulsar_spray:'20',pulsar_form_variation:'3',pulsar_reverse:'50',pulsar_keytrack:'100'});globalThis.init();tick(82);const pulsarModDoc=globalThis.__docSplit;pulsarModDoc.state.view='settings';pulsarModDoc.state.settingsPage=pulsarModDoc.pages().findIndex(p=>p.title==='PULSAR MOD');pulsarModDoc.state.actionMessageTicks=0;pulsarModDoc.redraw();capture('pulsar-mod.svg','Pulsar modulation and source variation example','PULSAR MOD');

Object.assign(params,{algorithm:'13',stochastic_source_hold:'1',stochastic_evolution:'1024'});globalThis.init();tick(82);const stochasticDoc=globalThis.__docSplit;stochasticDoc.state.view='settings';stochasticDoc.state.settingsPage=stochasticDoc.pages().findIndex(p=>p.title==='STOCHASTIC');stochasticDoc.state.actionMessageTicks=0;stochasticDoc.redraw();capture('stochastic-held.svg','Stochastic source hold and Cycle evolution','STOCHASTIC');

Object.assign(params,{algorithm:'13',stochastic_law:'3',stochastic_capture:'65',stochastic_source_hold:'1'});globalThis.init();tick(82);const listenerDoc=globalThis.__docSplit;listenerDoc.state.view='settings';listenerDoc.state.settingsPage=listenerDoc.pages().findIndex(p=>p.stochastic);listenerDoc.state.actionMessageTicks=0;listenerDoc.redraw();capture('stochastic-listener.svg','Stochastic listener capture with conditional CAPT knob','STOCHASTIC');

Object.assign(params,{algorithm:'10',morph_mode:'1',morph_amplitude:'100',morph_pair:'1>2',split_state:JSON.stringify({enabled:0,shared:0,quad:0,focus:0,parts:Array.from({length:2},()=>({algorithm:10,root:60,octave:0,scale:0,mode:0,mute:0}))})});
globalThis.init();tick(82);const imprintDoc=globalThis.__docSplit;imprintDoc.state.view='settings';imprintDoc.state.settingsPage=imprintDoc.pages().findIndex(p=>p.title==='MORPH');imprintDoc.state.actionMessageTicks=0;imprintDoc.redraw();capture('morph-imprint.svg','Morph Imprint envelope transfer','MORPH');

Object.assign(params,{algorithm:'4',voice_mode:'2',bank_slot:'1',bank_start:'0',bank_end:'1',bank_rate:'.5',bank_master_rate:'1',bank_pan:'-25',bank_play:'0',bank_choke:'1',bank_record_source:'3',bank_reverse:'0',bank_level:'1',bank_attack:'5',bank_release:'40',bank_fade:'8',
    split_mode:'4',split_focus:'1',split_state:JSON.stringify({enabled:1,shared:1,quad:0,bank:1,focus:1,blend:.5,parts:Array.from({length:2},()=>({algorithm:4,root:60,octave:0,scale:0,mode:0,mute:0}))}),
    bank_state:JSON.stringify({selected:0,slots:[4,1,0,1,0,1,1,0,1,0,0,1,0,0,1,0]}),
    bank_waveform:Array.from({length:112},(_,i)=>{const a=Math.round(1+6*Math.exp(-i/50)*Math.abs(Math.sin(i*.19)));return [-a,a];}).flat().join(','),
    voice_pads:'-1,-1,-1,-1,4,-1,-1,-1',playheads:'-1,-1,-1,-1,420,-1,-1,-1'});
globalThis.init();tick(82);const bankDoc=globalThis.__docSplit;bankDoc.state.view='main';bankDoc.state.messageTicks=0;
bankDoc.redraw();capture('bank-waveform.svg','Bank selected stereo sample and sixteen-slot overview');
bankDoc.state.view='settings';bankDoc.state.settingsPage=bankDoc.pages().findIndex(p=>p.title==='BANK SAMPLE');bankDoc.state.actionMessageTicks=0;
bankDoc.redraw();capture('bank-sample.svg','Bank sample playback controls','B BANK SAMPLE');
bankDoc.state.settingsPage=bankDoc.pages().findIndex(p=>p.title==='BANK PADS');bankDoc.redraw();capture('bank-pads.svg','Bank per-pad controls and recording source','B BANK PADS');
Object.assign(bankDoc.state,{view:'main',splitFocus:0,algorithm:4,mask:15,playheads:[.16,.38,.65,.85,-1,-1,-1,-1],stackPositions:[0,1/3,2/3,1,-1,-1,-1,-1]});
bankDoc.state.splitParts[0].window=[0,1];bankDoc.redraw();capture('bank-waveform-a.svg','Bank A four-lane waveforms and hollow stack position markers');

Object.assign(params,{algorithm:'5',slice_region_mode:'3',record_lane:'1',split_state:JSON.stringify({enabled:0,focus:0,parts:[{},{}]}),
    slice_editor_state:JSON.stringify({v:1,active:1,lane:1,part:0,selected:1,boundary:1,cursor:.25,count:4,preroll:6,duration:1000,source:3,mode:3,overflow:0,loop:1,playing:1,latched:1,pad:1,page:0,label:'Snare',request:1,points:[0,.25,.5,.875,1]})});
globalThis.init();tick(82);const sliceDoc=globalThis.__docSplit;sliceDoc.state.view='settings';sliceDoc.state.settingsPage=sliceDoc.pages().findIndex(p=>p.title==='SLICE SOURCE');
sliceDoc.openSliceEditor();sliceDoc.sliceUi.zoom=2;sliceDoc.readSliceEditor();sliceDoc.redraw();capture('slice-edit.svg','Slice Edit: labelled slice, boundary caps, pre-roll and waveform zoom','L1 S02/04');

const customDocState={v:1,profile:1,bank:2,count:3,revision:1,ui:1,editing:0,index:0,learn:0,filter:0,result:0,conflict:-1,undo:0,save:2,recovered:0,mode:0,draft:[1,16,0,0,641,0,0,0,1,0,.1,.9,0]};
Object.assign(params,{algorithm:'2',midi_control_mode:'2',record_lane:'1',split_state:JSON.stringify({enabled:0,focus:0,parts:[{},{}]}),
    midi_custom_state:JSON.stringify(customDocState),midi_custom_entries:JSON.stringify([[0,1,0,0,0,0,0,0,0],[1,1,4,1,0,6,0,0,0],[2,1,16,0,1,-1,0,0,2]])});
globalThis.init();tick(82);const cmDoc=globalThis.__docSplit;cmDoc.state.view='settings';cmDoc.state.settingsPage=cmDoc.pages().findIndex(p=>p.customMaps);cmDoc.readCustomMidi();cmDoc.redraw();capture('midi-maps.svg','Custom MIDI profile selection, banks, copy and clear','MIDI MAPS');
cmDoc.openCustomMidiList();cmDoc.redraw();capture('midi-map-list.svg','Custom MIDI assignments: incoming channel and CC to explicit parts and engine slots');
customDocState.editing=1;params.midi_custom_state=JSON.stringify(customDocState);cmDoc.readCustomMidi();cmDoc.state.view='midiMapEdit';cmDoc.customMidiUi.tab=0;cmDoc.customMidiUi.notice='';cmDoc.redraw();capture('midi-map-target.svg','Custom mapping Target: named parameter and explicit part');
cmDoc.customMidiUi.tab=1;customDocState.learn=1;params.midi_custom_state=JSON.stringify(customDocState);cmDoc.readCustomMidi();cmDoc.redraw();capture('midi-map-source.svg','Custom mapping Source: Learn or manual channel and CC');
cmDoc.customMidiUi.tab=2;customDocState.learn=0;params.midi_custom_state=JSON.stringify(customDocState);cmDoc.readCustomMidi();cmDoc.redraw();capture('midi-map-response.svg','Custom mapping Response: range, reverse and pickup');

const projectExample={v:1,job:0,action:3,ok:1,reopen:0,page:0,count:2,bytes:0,total:0,message:'PROJECTS READY'};
params.project_state=JSON.stringify(projectExample);params.project_files=JSON.stringify(['Thru-20261106-183000.thru','BeforeImport-20261106.thru']);
const projectDoc=globalThis.__docSplit;projectDoc.openProjects();projectDoc.readProject();projectDoc.redraw();capture('projects-menu.svg','Projects menu: complete project export and import','THRU PROJECTS');
projectDoc.projectUi.mode='list';projectDoc.redraw();capture('projects-list.svg','Project import browser listing portable .thru files','THRU PROJECTS');
projectDoc.projectUi.mode='confirm';projectDoc.projectUi.confirm=0;projectDoc.redraw();capture('projects-confirm.svg','Project import confirmation with Cancel selected','THRU PROJECTS');
Object.assign(projectExample,{job:3,action:1,bytes:6291456,total:12582912});params.project_state=JSON.stringify(projectExample);projectDoc.readProject();projectDoc.redraw();capture('projects-progress.svg','Project export progress in megabytes','THRU PROJECTS');
params.bank_state=JSON.stringify({selected:0,slots:[3,...Array(15).fill(0)],take:7,pending:0,failed:1,discard_seq:0,discard_reason:0});
Object.assign(projectDoc.state,{view:'main',bankMode:true,splitEnabled:true,sharedDual:true,splitFocus:1,saveStatus:4,performanceVisible:false});
projectDoc.openBankRecovery();projectDoc.redraw();capture('bank-save-recovery.svg','Failed Bank take: Retry Save, Keep Waiting or confirmed Discard','UNSAVED BANK TAKE');

// Layers examples use the production renderer and actual control descriptors.
const layerExample={active:1,surface:1,corner:2,held:-1,serial:10,fxPads:2**4,
 values:[0,.5,.2,0,0,0,0,1,1,.64,.2,0,0,1,.35,1,2,.4,.55,.5,.65,2,.4,1,3,.58,.4,.65,.65,2,.25,1,
  0,.35,.6,120,.1,.3,.4,.2,1,1,.65,0,0,0,0,0,.35],
 extra:[0,0,0,0,0,0,0,0,.65,.75,.4,1,0,0,0,0],
 mix:[.35,.2,.3,.15],touches:Array.from({length:32},(_,i)=>i===31?1:0),voices:[[24,63,0],[9,55,1]]};
Object.assign(params,{layers_view:JSON.stringify(layerExample),algorithm:'2',lane_path:'2',split_mode:'5',
 split_state:JSON.stringify({enabled:0,shared:0,quad:0,focus:0,parts:[{},{}]})});
globalThis.init();tick(82);
const layersDoc=globalThis.__docSplit;globalThis.__docLayers.read();
Object.assign(layersDoc.state,{view:'main',bankMode:false,splitEnabled:false,actionMessageTicks:0,messageTicks:0,saveStatus:0,savedTicks:0,performanceVisible:false,mask:15,lane:2,fieldStart:.1,fieldEnd:.9,playheads:[.28,.7,-1,-1,-1,-1,-1,-1],stackPositions:[.12,.72,-1,-1,-1,-1,-1,-1],weights:[.1,.65,.2,.05]});
layersDoc.redraw();capture('layers-field.svg','Layers four-lane waveform navigation and parallel FX field','LAYERS');
layersDoc.state.saveStatus=2;layersDoc.redraw();capture('layers-field-saving.svg','Layers weight meters remain visible beside Saving','LAYERS');
layersDoc.state.saveStatus=3;layersDoc.state.savedTicks=30;layersDoc.redraw();capture('layers-field-saved.svg','Layers weight meters remain visible beside Saved','LAYERS');
layersDoc.state.saveStatus=0;layersDoc.state.savedTicks=0;
globalThis.onMidiMessageInternal([0xb0,52,127]);globalThis.onMidiMessageInternal([0xb0,52,0]);
layersDoc.redraw();capture('layers-focus-fx.svg','Layers overview with FX editing focus','LAYERS FX');
globalThis.onMidiMessageInternal([0xb0,52,127]);globalThis.onMidiMessageInternal([0xb0,52,0]);
layersDoc.state.view='settings';layersDoc.state.settingsPage=0;
layersDoc.redraw();capture('layers-a-engine.svg','Instrument A engine page in Layers','A ENGINE');

layersDoc.state.view='settings';layersDoc.state.settingsPage=layersDoc.pages().findIndex(p=>p.layersSetup);
layersDoc.redraw();capture('layers-setup.svg','Layers FX corner, glide, pressure, release and position settings','LAYERS');
layersDoc.state.settingsPage=layersDoc.pages().findIndex(p=>p.layerFx);
layersDoc.redraw();capture('layers-blur.svg','Independent Blur corner with pressure target and output trim','C3 FX CORNER');

for(const [type,name,values,extra] of [[2, "blur", [0.4, 0.55, 0.5, 0.65], [0.65, 0.75, 0.4, 1]], [3, "delay", [0.65, 0.35, 0.6, 0.7], [1, 0.2, 0.1, 0.3]], [4, "delay-field", [0.35, 0.6, 0.35, 0.25], [1, 0, 0.5, 0.25]], [5, "spray", [0.45, 0.25, 0.6, 0.2], [0.65, 0.75, 0.4, 1]], [6, "fracture", [0.1111111111111111, 0.45, 0.5, 0.5], [0.3333333333333333, 0.1, 0.75, 0.7]], [7, "shred", [1, 0.15, 0, 0.5], [0.3333333333333333, 0.1, 0.75, 0.7]], [8, "buffer", [0.43, 0.5, 0.65, 0.15], [0.44, 0.4, 0, 0]], [9, "reverb", [0, 0.5, 0.69, 0.1], [0.72, 0.12, 1, 0]], [10, "tape", [0, 0.2, 0.1, 0], [0.5, 0.5, 0.5, 0.5]], [11, "pitch", [0.5, 0.5, 0.63092975, 0.12], [0, 0, 0, 0]], [12, "rotary", [0.35355339, 0.65, 0.5463034, 0.5], [0.50865197, 0.5, 1, 1]], [13,"rerun",[3/7,1,1,.5],[1,0,0,.25]], [14,"shutter",[3/7,.55,0,0],[1,0,.5,0]], [15,"tumble",[6/7,3/7,0,.15],[1,.5,.17,.25]], [16,"spool",[.5,1,0,.5],[0,.5,0,.25]]]){
    layerExample.corner=2;layerExample.serial++;
    layerExample.values.splice(16,8,type,...values,0,0,1);layerExample.extra.splice(8,4,...extra);
    params.layers_view=JSON.stringify(layerExample);globalThis.__docLayers.read();
    layersDoc.state.settingsPage=layersDoc.pages().findIndex(p=>p.title==='FX CORNER');
    if(type!==2){layersDoc.redraw();capture('layers-'+name+'.svg','Independent '+name+' corner controls','C3 FX CORNER');}
    const detailPage=layersDoc.pages().findIndex(p=>p.layerFxDetail);
    if(detailPage>=0){layersDoc.state.settingsPage=detailPage;
      layersDoc.redraw();capture('layers-'+name+'-detail.svg','Independent '+name+' detail controls','C3 FX DETAIL');}

}
layerExample.values[16]=13;params.layers_view=JSON.stringify(layerExample);globalThis.__docLayers.read();
layersDoc.state.view='settings';layersDoc.state.settingsPage=layersDoc.pages().findIndex(p=>p.title==='FX CORNER');
globalThis.__docLayers.openPicker();layersDoc.redraw();capture('layers-fx-families.svg','Layers grouped effect families','C3 FX FAMILY');
globalThis.onMidiMessageInternal([0xb0,3,127]);globalThis.onMidiMessageInternal([0xb0,3,0]);
layersDoc.redraw();capture('layers-fx-rhythm.svg','Layers rhythm and buffer effect choices','C3 CHOOSE FX');
layersDoc.state.view='mode';layersDoc.state.modeSelection=5;
layersDoc.redraw();capture('layers-mode.svg','Instrument mode picker scrolled to Layers','INSTRUMENT MODE');


const seqExample={active:1,edit:1,rec:0,step:6,held:-1,child:0,root:60,mask:3,length:13,playing:1,
 lanes:[[3,7,4,1,0],[6,13,5,1,0],[8,11,6,1,0],[2,9,4,1,0]],
 strips:[[0,0,0,900,0],[0,0,0,900,0],[2.5,-1.5,0,1800,0],[0,0,0,900,0]],
 heads:[[1,.28,.54,300,14],[1,.73,.81,120,13]],
 steps:Array.from({length:32},(_,i)=>[i%3===0?1:0,[.08,.28,.5,.73,.91][i%5]]),
 values:[13,5,2,1,0,100,.8,0,7,0,.73,7,100,.8,-.1,.85,4,0,4]};
Object.assign(params,{split_mode:'6',split_focus:'1',layers_view:'{}',seq_state:JSON.stringify(seqExample),split_state:JSON.stringify({enabled:1,shared:0,quad:1,bank:0,focus:1,blend:.5,parts:Array.from({length:4},()=>({algorithm:4,root:60,octave:0,scale:0,mode:0,mute:0}))})});
globalThis.init();tick(82);const seqDoc=globalThis.__docSplit;globalThis.__docSequence.read();
Object.assign(seqDoc.state,{view:'main',recording:true,mask:15,messageTicks:0,actionMessageTicks:0});
seqDoc.redraw();capture('sequence-overview.svg','Sequence overview with independent lane clocks and fixed recording destination','EDIT 2 REC 1');
seqExample.held=6;params.seq_state=JSON.stringify(seqExample);globalThis.__docSequence.read();seqDoc.state.view='settings';seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title==='TRIG');seqDoc.redraw();capture('sequence-trig.svg','Trig source position, pitch and timing controls','L2 T07 TRIG');
seqExample.step_ms=83.3333;params.seq_state=JSON.stringify(seqExample);globalThis.__docSequence.read();
seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title==='SEQ AMP');seqDoc.redraw();capture('sequence-amp.svg','Sequence envelope with applied attack, decay and release inside DUR','B SEQ AMP T07');
seqExample.child=3;seqExample.burst=[0,.18,.625,.88];seqExample.values[9]=3;seqExample.values[16]=4;seqExample.values[17]=.625;params.seq_state=JSON.stringify(seqExample);globalThis.__docSequence.read();seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title==='BURST');seqDoc.redraw();capture('sequence-burst.svg','Burst step-time ruler with selected retrigger and unified event controls','L2 T07 BURST');
seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title==='STRIP 3');seqDoc.redraw();capture('sequence-strip.svg','Third sequence channel strip in the Mix/FX page group','STRIP 3');

// Persistent types, displayed through the production settings renderer.
seqExample.held=-1;seqExample.child=0;seqExample.values.length=67;
seqExample.values.fill(0,19);seqExample.values[19]=1;seqExample.values[20]=1000;seqExample.values[21]=1000;
seqExample.values[41]=1;
[5,2,0,2,0,1,27,7,0,.08,.92,3,12,2,.5,2,50,1000,.03,-1].forEach((v,i)=>seqExample.values[44+i]=v);
seqExample.values[64]=2;seqExample.values[65]=2;seqExample.values[66]=1;seqExample.values[41]=0;params.seq_state=JSON.stringify(seqExample);globalThis.__docSequence.read();
Object.assign(seqDoc.state,{view:'settings',recording:false,saveStatus:0,savedTicks:0});
seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title==='SEQ SETUP');seqDoc.redraw();
capture('sequence-setup-normal.svg','Normal Sequence setup with reserved generation actions','B SEQ SETUP');
seqExample.values[41]=1;
seqExample.type_status='5/13';params.seq_state=JSON.stringify(seqExample);globalThis.__docSequence.read();
Object.assign(seqDoc.state,{view:'settings',recording:false,saveStatus:0,savedTicks:0});
for(const [title,file] of [['SEQ SETUP','sequence-setup'],['EUCLIDEAN','sequence-euclidean'],['EUC SOURCE','sequence-euclidean-source'],['EUC VALUES','sequence-euclidean-values']]){
 seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title===title);seqDoc.redraw();capture(file+'.svg',title+' controls for the selected sequence lane',title==='SEQ SETUP'?'B SEQ SETUP':'L2 '+title);
}
seqExample.values[41]=2;params.seq_state=JSON.stringify(seqExample);globalThis.__docSequence.read();
for(const [title,file] of [['ONSET','sequence-onset'],['ONSET SOURCE','sequence-onset-source'],['ONSET VALUES','sequence-onset-values']]){
 seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title===title);seqDoc.redraw();capture(file+'.svg',title+' controls for the selected sequence lane',title==='SEQ SETUP'?'B SEQ SETUP':'L2 '+title);
}

// Show the two task groups beside the Sequence page-order explanation.
Object.assign(seqDoc.state,{view:'pages',pageTier:'category',pageCategory:0,pageSelection:1});
seqDoc.redraw();capture('sequence-play-pages.svg','Sequence Play pages with Setup second','PLAY');
Object.assign(seqDoc.state,{pageCategory:1,pageSelection:0,lane:seqExample.edit+1});
seqDoc.redraw();capture('sample-pages.svg','Sample group for recording, editing and browsing source audio','SAMPLE');

// Spline source-creation workspace, using the actual production renderer.
const synthDoc=globalThis.__docSynth;
const synthExample={type:8,events:[1,8,0,0,1,0,0,1],drums:[0,48,850,.25,.04,.2,.55,0,24,45,.1,.78,0,.6,.4,.7],drumExtra:[.94,4,80,.12,0,0,.35,.15],open:1,job:0,op:0,progress:100,ready:1,preview:0,rev:1,row:0,point:3,seed:37,locks:0x113f,name:'Glass Current',message:'READY',values:[2,60,.3,5,20,.75,7,32,.2,100,1,.25,0],
 curves:Array.from({length:4},()=>[255,...Array.from({length:8},(_,i)=>[i/7,0,2])]),
 wave:Array.from({length:112},(_,i)=>[-.55*Math.abs(Math.sin(i*.37)),.55*Math.abs(Math.cos(i*.31))])};
synthExample.curves[0]=[255,...[0,3,7,12,7,3,0,0].map((v,i)=>[i/7,v,2])];
params.synth_state=JSON.stringify(synthExample);synthDoc.read();
Object.assign(seqDoc.state,{view:'synth',saveStatus:0,savedTicks:0});
for(const [page,file,heading] of [[0,'setup','SYNTH SETUP'],[1,'sound','DRUM'],[2,'curve','CURVE PITCH'],[3,'random','RANDOM'],[4,'preview','PREVIEW']]){
 synthDoc.ui.page=page;synthDoc.ui.overlay='';synthDoc.draw();capture('synth-'+file+'.svg','Drum '+file+' page',heading);
}
synthExample.ready=0;synthExample.message='CAPTURE: RENDER';params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=4;synthDoc.draw();capture('synth-edited.svg','Edited draft keeps the last render until Capture is tapped','PREVIEW');
synthExample.ready=1;synthExample.message='READY';
synthDoc.ui.page=2;synthExample.point=7;params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.draw();
capture('synth-curve-end.svg','Final Drum point with fixed time and no outgoing shape','CURVE PITCH');
synthExample.point=3;synthExample.curves[0][0]&=~(1<<3);params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.draw();
capture('synth-curve-unused.svg','Unused Drum point selected with PT, before editing enables it','CURVE PITCH');
// Contextual generator and shared-modulation pages.
synthExample.curves=Array.from({length:4},()=>[129,...Array.from({length:8},(_,i)=>[i/7,0,2])]);
synthExample.type=0;synthExample.mod=[1,5,.35,3,150,.25,1];synthExample.curves[0][0]=255;
params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=5;synthDoc.draw();capture('synth-mod.svg','Shared generator modulation and file loop controls','SYNTH MOD');
synthExample.type=1;synthExample.osc=[3,.37,7.5,.15,3200,.25,.08];params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-wave.svg','Wave generator pulse, detune, sub, filter and noise controls','WAVE');
synthExample.type=2;synthExample.packets=[1,0,6.4,170,.3,12,30,240,.25,.3,.4,.2,.3,.4,.2,1.5];synthExample.spectrum=2;synthExample.component=2;
synthExample.partials=[[1,1],[1.414,.7],[2.76,-.4],[3.414,.3],[4.8,.2],[6.1,0],[8.3,.1],[11,.05]];synthExample.partial_masks=[255,85,223,255];
params.synth_state=JSON.stringify(synthExample);synthDoc.read();
for(const [page,file,heading] of [[1,'bloom','BLOOM'],[6,'bloom-var','BLOOM VAR'],[7,'spectrum','SPECTRUM C']]){synthDoc.ui.page=page;synthDoc.draw();capture('synth-'+file+'.svg','Bloom '+file+' controls',heading);}
synthExample.type=3;synthExample.spectral=[1,-7,.08,.92,.18,.25,24,-60];synthExample.source='glass-loop.wav';
params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-resynth.svg','Resynth source window, tuning and spectral controls','RESYNTH');
synthDoc.ui.page=4;synthDoc.draw();capture('synth-resynth-source.svg','Resynth preview identifies its captured source','PREVIEW');
synthExample.type=4;synthExample.additive=[24,-3,1.02,7,.4,.2,.5,.3];synthExample.spectrum=3;synthExample.component=7;
synthExample.partials=Array.from({length:8},(_,i)=>[i+25,(i%2?-1:1)/(i+1)]);synthExample.partial_masks=[255,255,255,255];
params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-partials.svg','Continuous additive partial bank controls','PARTIALS');
synthDoc.ui.page=7;synthDoc.draw();capture('synth-partial-bank.svg','Partial components 25 to 32 with selected ratio and level','PART 25-32');
synthExample.type=5;synthExample.noise=[12,3,-2,1,1,4,.7,.05];synthExample.spectrum=1;synthExample.component=2;synthExample.partials=Array.from({length:8},(_,i)=>[i+9,1/(i+1)]);
params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-noise.svg','Noise Bank tuned bands, bandwidth, color and motion controls','NOISE BANK');
synthDoc.ui.page=7;synthDoc.draw();capture('synth-noise-bands.svg','Noise band centers and levels selected with the pads','BANDS 9-16');
synthExample.type=12;synthExample.dustParams=[1,28,3.2,.6,11000,.1,12,.8,.05,1,3,.4];params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-dust.svg','Dust crackles rate duration clustering edges and bit depth','DUST');
synthExample.dustParams[0]=5;params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=6;synthDoc.draw();capture('synth-dust-detail.svg','Dust noise bed color repeats and data-pattern variation','DUST DETAIL');
synthExample.type=6;synthExample.fmOsc=[1,2,3.5,1.5,2.2,.15,8,.85];params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-fm.svg','FM operator routing, ratios, indices, feedback, detune and mix','FM');
synthExample.type=7;synthExample.conv=[.12,900,0,180,3,.35,.8,1];synthExample.primary='spoken-air.wav';synthExample.impulse='metal-hit.wav';synthExample.seconds=1.08;
params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=8;synthDoc.draw();capture('synth-conv-files.svg','Convolution primary and impulse file choices','CONV FILES');
synthDoc.ui.page=1;synthDoc.draw();capture('synth-convolve.svg','Convolution regions, impulse window, tone, blend and tail controls','CONVOLVE');
synthExample.type=8;synthExample.drumExtra=[.94,6.5,340,.18,0,0,.35,.15];synthExample.drums=[1,210,420,.65,.7,.15,.6,.1,7,22,.35,.45];synthExample.values[0]=2;synthExample.values[3]=0;synthExample.values[4]=2;
params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-drum.svg','Compact physical drum sample generator','DRUM');
synthDoc.ui.page=6;synthDoc.draw();capture('synth-drum-detail.svg','Drum bend, sweep, ring and damping controls','DRUM DETAIL');
synthExample.drums=[0,48,850,.25,.04,.2,.55,0,24,45,.1,.78,0,.6,.4,.7];synthExample.drumExtra=[.94,4,80,.12,0,0,.35,.15];params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=13;synthDoc.draw();capture('synth-drum-attack.svg','Native Kick attack and transient controls','DRUM ATTACK');
for(const [model,name] of [[6,'membrane'],[7,'modal'],[8,'medium']]){
  synthExample.drums=[model,model===6?43:model===7?180:343,1800,.55,.3,.4,.5,.15,24,42,.35,.4,model===6?1:model===7?5:2,.7,.3,.75];
  params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-'+name+'.svg',name+' stereo sample generation','DRUM');
  synthDoc.ui.page=6;synthDoc.draw();capture('synth-'+name+'-detail.svg',name+' material and excitation controls','DRUM DETAIL');
}
synthExample.type=8;synthExample.drums=[9,44,3200,.88,.62,.72,.38,0,.56,.45,.5,.22,0,.2,.28,.55];synthExample.drumExtra=[.65,320,140,.35,0,0,.35,.15];params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-concert.svg','Concert Bass main controls','DRUM');
synthDoc.ui.page=6;synthDoc.draw();capture('synth-concert-detail.svg','Concert Bass head and articulation controls','DRUM DETAIL');
synthExample.drums[12]=2;params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=13;synthDoc.draw();capture('synth-concert-attack.svg','Concert Bass rim excitation and transient controls','DRUM ATTACK');
synthExample.type=14;synthExample.drums=[10,80,650,.8,.025,.3,.7,.4,24,30,1.5,.12,0,.35,.35,0];synthExample.drumExtra=[.8,3,120,0,0,0,.35,.15];synthExample.drumColor=[12,16000,9000,.7,0,0,0,0];params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-fm-drum.svg','FM Drum carrier and transient controls','FM DRUM');
synthDoc.ui.page=6;synthDoc.draw();capture('synth-fm-drum-detail.svg','FM Drum ratio feedback and decay controls','FM SHAPE');
synthDoc.ui.page=13;synthDoc.draw();capture('synth-fm-drum-attack.svg','FM Drum punch snap and attack cleanup','FM ATTACK');
synthDoc.ui.page=14;synthDoc.draw();capture('synth-fm-drum-color.svg','FM Drum drive compression bit and rate coloration','FM COLOR');
synthExample.type=8;params.synth_state=JSON.stringify(synthExample);synthDoc.read();
synthDoc.ui.page=14;synthDoc.draw();capture('synth-drum-color.svg','Native drive compression and offline bit rate coloration','DRUM COLOR');
synthExample.type=13;synthExample.wt=[0,48,600,1,2,.18,.55,.8,.1,.35,2,.18,0,.35,.025,.85,32,22,38,18,2,.3,.45,.15];synthExample.values[2]=.5;
for(const [page,file,title,caption]of [[1,'synth-wt-drum.svg','WT DRUM','Wavetable drum starting model body scan and attack'],[6,'synth-wt-osc.svg','WT OSC','Wavetable frame positions oscillator blend modulation noise and tone'],[13,'synth-wt-shape.svg','WT SHAPE','Wavetable drum envelope drive compression and edge controls']]){params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=page;synthDoc.draw();capture(file,caption,title);}
synthExample.type=9;synthExample.plucked=[110,1400,.35,.65,.1,.4,.2,12];synthExample.events=[1,8,1,0,0,500,0,.35];synthExample.timing=[1,120,5,.7,0,0,0,0];synthExample.eventfit=8;
params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-pluck.svg','Pluck string synthesis controls','PLUCK');
synthDoc.ui.page=9;synthDoc.draw();capture('synth-events.svg','Shared event composition controls','SYNTH EVENTS');
synthDoc.ui.page=12;synthDoc.draw();capture('synth-event-var.svg','Optional event variation and clock controls','EVENT VAR');
synthExample.events[0]=0;params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=9;synthDoc.draw();capture('synth-events-off.svg','Inactive event controls','SYNTH EVENTS');
synthExample.type=10;synthExample.sub=[8,4,.25,-6,0,0,.25,0];params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=1;synthDoc.draw();capture('synth-subharmonic.svg','Additive harmonic and subharmonic layers','SUBHARMONIC');
synthExample.spectrum=2;synthExample.component=1;synthExample.partials=Array.from({length:8},(_,i)=>[i+2,i===1?.7:1]);params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=7;synthDoc.draw();capture('synth-sub-bank.svg','Subharmonic divisors and component levels','SUBS 1-8');
synthExample.type=3;synthExample.spectral=[1,-3,0,1,.1,.2,24,-60];
for(const [mode,name]of [[0,'linked'],[1,'point'],[2,'smear'],[3,'freeze']]){synthExample.trace=[mode,.75,150,.2,.15,144,.6,.5];params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=10;synthDoc.draw();capture('synth-trace-'+name+'.svg',name+' partial trace controls','RSYN TRACE');}
synthExample.type=11;synthExample.hybridParams=[1,4,-2,.8,.65,.35,160,.2];synthExample.inputs=['voice.wav','bell.wav','rain.wav','strings.wav'];
params.synth_state=JSON.stringify(synthExample);synthDoc.read();synthDoc.ui.page=11;synthDoc.draw();capture('synth-hybrid-files.svg','Four Hybrid source selections','HYBRID FILES');
synthDoc.ui.page=1;synthDoc.draw();capture('synth-hybrid.svg','Multi-source trait Hybrid controls','HYBRID');
synthDoc.ui.overlay='nav';synthDoc.ui.selection=0;synthDoc.draw();capture('synth-groups.svg','Synth Sound, Motion, Output and Actions navigation','SYNTH GROUPS');synthDoc.ui.overlay='';
Object.assign(synthDoc.ui,{overlay:'name',name:'Glass Current',cursor:13});synthDoc.draw();capture('synth-name.svg','Name a generated Spline WAV with the pad keyboard','NAME WAV');

// Render from the real UI, including the full-name picker and read-only target value.
const lfoDoc=globalThis.__docLfo,lfoState=globalThis.__docSplit.state;
const lfoId=key=>lfoDoc.catalog.find(d=>d.key===key).id;
Object.assign(lfoState,{view:'settings',splitEnabled:true,splitFocus:0,quadMode:false,bankMode:false,sharedDual:false,algorithm:2,editorReady:true,actionMessageTicks:0});
globalThis.__docLayers.model.active=false;globalThis.__docSequence.model.active=false;
globalThis.__docSplit.params.algorithm.value=2;
const lfoExample={selected:2,part:0,primary:2,values:Array.from({length:8},()=>[0,0,0,1,0,0,0,10,0,0,-1]),parts:[0,1,4],engines:[-1,2],allowed:lfoDoc.catalog.filter(d=>['off','engine','insert','mixer'].includes(d.scope)&&d.modes.includes(1)&&(!d.engines.length||d.engines.includes(2))).map(d=>d.id),trig:[]};
lfoExample.values[2]=[lfoId('grain_size'),0,0,.25,.5,0,.25,20,0,0,-1,1];
params.lfo_state=JSON.stringify(lfoExample);
params.lfo_live=JSON.stringify({owner:0,v:[[0,'grain_size',180,90]],phase:Array(8).fill(.37),output:Array(8).fill(.29)});
lfoDoc.read();lfoDoc.readLive();
lfoState.settingsPage=globalThis.__docSplit.pages().findIndex(p=>p.title==='LFO');lfoDoc.draw(lfoDoc.pages[0]);capture('control-lfo.svg','Assignable Control LFO with real Grain Size destination','LFO');
lfoState.settingsPage=globalThis.__docSplit.pages().findIndex(p=>p.title==='LFO SETUP');lfoDoc.draw(lfoDoc.setup);capture('control-lfo-setup.svg','Control LFO engine filter, phase, smoothing, offset and clip ownership','LFO SETUP');
lfoDoc.openPicker();lfoDoc.drawPicker();capture('control-lfo-groups.svg','LFO destinations grouped by their existing parameter pages','DEST A');
lfoDoc.model.picker.group=lfoDoc.groups().findIndex(g=>g.name==='GRAINS MAIN');lfoDoc.model.picker.selection=0;lfoDoc.model.picker.stage='params';lfoDoc.drawPicker();capture('control-lfo-destinations.svg','Full-name Grain Size, Pitch and Stereo Spread destinations','GRAINS MAIN');
lfoDoc.model.picker=null;lfoState.view='settings';globalThis.__docSplit.params.grain_size.value=90;
lfoState.settingsPage=globalThis.__docSplit.pages().findIndex(p=>p.title==='GRAINS MAIN');lfoDoc.drawSettings();capture('control-lfo-target.svg','Grains Main showing a modulated Size of 180 ms with its solid marker','A GRAINS MAIN');
globalThis.__docSequence.model.active=true;lfoState.quadMode=true;lfoExample.parts=[0,1,2,3,4];lfoExample.values[2][5]=2;
lfoExample.trig=[3<<4,255,...Array.from({length:8},(_,i)=>[i===2?.5:1,i===2?.6:0]).flat()];
params.lfo_state=JSON.stringify(lfoExample);lfoDoc.read();lfoState.settingsPage=globalThis.__docSplit.pages().findIndex(p=>p.title==='TRIG LFO');lfoDoc.draw(lfoDoc.trig);capture('control-lfo-trig.svg','Sequence selected-LFO restart, rate and depth locks','TRIG LFO');
// Stochastic engine destinations share the same real grouped picker.
lfoExample.primary=13;lfoExample.engines=[-1,13];lfoExample.values[2][10]=-1;
lfoExample.allowed=lfoDoc.catalog.filter(d=>['off','engine','insert','mixer'].includes(d.scope)&&d.condition!=='stochastic-listener'&&(!d.engines.length||d.engines.includes(13))).map(d=>d.id);
params.lfo_state=JSON.stringify(lfoExample);lfoDoc.read();lfoDoc.openPicker();lfoDoc.model.picker.group=lfoDoc.groups().findIndex(g=>g.name==='STOCHASTIC');lfoDoc.model.picker.stage='params';lfoDoc.model.picker.selection=0;lfoDoc.drawPicker();capture('control-lfo-stochastic.svg','Stochastic LFO targets including Source Group Size and Evolution Rate','STOCHASTIC');
lfoDoc.model.picker=null;lfoState.view='settings';
// Playback readouts are independent of saved encoder values.
globalThis.__docSequence.model.active=false;Object.assign(lfoState,{view:'settings',splitEnabled:true,splitFocus:0,quadMode:false,bankMode:false,sharedDual:false,algorithm:2,clipPlaying:true,clipRecording:false,activeClip:0});
globalThis.__docSplit.params.split_mode.value=1;globalThis.__docSplit.params.voice_engine_edit.value=1;
params.midi_state=JSON.stringify({v:1,owner:4,clip:0,playing:1,corner:0,entry:0,values:[[0,641,90]],playback:[[0,641,150]]});globalThis.__docReadout.read();
params.lfo_live=JSON.stringify({owner:0,v:[]});lfoDoc.readLive();lfoState.settingsPage=globalThis.__docSplit.pages().findIndex(p=>p.title==='GRAINS MAIN');lfoDoc.drawSettings();capture('clip-automation.svg','Recorded Size movement with a hollow clip automation marker','> A GRAINS MAIN');
params.lfo_live=JSON.stringify({owner:0,v:[[0,'grain_size',300,150]]});lfoDoc.readLive();lfoDoc.drawSettings();capture('clip-automation-lfo.svg','Recorded Size movement combined with LFO modulation and its solid marker','> A GRAINS MAIN');
// TABLE scopes are actual native audio, captured by tests/test_table.inc.
const tableDoc=globalThis.__docTable,tableExample=JSON.parse(fs.readFileSync(path.join(repoRoot,'tests/table_scope_fixture.json'),'utf8'));
Object.assign(seqDoc.state,{view:'main',algorithm:15,splitEnabled:false,sharedDual:false,quadMode:false,bankMode:false,splitFocus:0,lane:2,mask:0,voiceCount:1,playheads:Array(8).fill(-1),stackPositions:[.5,...Array(7).fill(-1)],voiceAlgorithms:[15,...Array(7).fill(-1)],editorReady:true,clipPlaying:false,clipRecording:false,actionMessageTicks:0,messageTicks:0,saveStatus:0});
globalThis.__docLayers.model.active=false;globalThis.__docSequence.model.active=false;
Object.assign(params,{algorithm:'15',split_mode:'0',voice_engine_mode:'0',record_lane:'2'});seqDoc.params.split_mode.value=0;seqDoc.params.algorithm.value=15;seqDoc.params.voice_engine_mode.value=0;
tableExample.state.edit=1;tableExample.state.revision=99999;params.table_state=JSON.stringify(tableExample.state);params.table_scope=JSON.stringify(tableExample.scope);tableDoc.read(false);tableDoc.scopes(false);
seqDoc.redraw();capture('table-scope.svg','TABLE live pre-effects patch scopes without sample waveforms','THRU TABLE');
for(const [variant,title,file] of [[0,'TABLE','table-common'],[0,'TABLE WAVE','table-wave'],[1,'TABLE SPLINE','table-spline'],[2,'TABLE WT OSC','table-wt-osc'],[2,'TABLE WT HIT','table-wt-hit'],[2,'TABLE WT COLOR','table-wt-color']]){
 tableExample.state.patches[1][0]=variant;tableExample.state.patches[1][4]=variant===2?1:0;tableExample.state.patches[1][6]=variant===2?25:0;
 params.table_state=JSON.stringify(tableExample.state);tableDoc.read(false);seqDoc.state.view='settings';seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title===title);
 seqDoc.redraw();capture(file+'.svg',title+' patch controls',title);
}
// SCANNED shows actual physical displacement from the native engine.
const scannedDoc=globalThis.__docScanned,scannedExample=JSON.parse(fs.readFileSync(path.join(repoRoot,'tests/scanned_scope_fixture.json'),'utf8'));
Object.assign(seqDoc.state,{view:'main',algorithm:16,splitEnabled:false,sharedDual:false,quadMode:false,bankMode:false,splitFocus:0,lane:2,mask:0,voiceCount:1,playheads:Array(8).fill(-1),stackPositions:[.5,...Array(7).fill(-1)],voiceAlgorithms:[16,...Array(7).fill(-1)],editorReady:true,clipPlaying:false,clipRecording:false,actionMessageTicks:0,messageTicks:0,saveStatus:0});
seqDoc.params.algorithm.value=16;seqDoc.params.split_mode.value=0;seqDoc.params.voice_engine_mode.value=0;
scannedExample.state.edit=1;scannedExample.state.revision=999999;params.scanned_state=JSON.stringify(scannedExample.state);params.table_scope=JSON.stringify(scannedExample.scope);scannedDoc.read(false);tableDoc.scopes(false);
seqDoc.redraw();capture('scanned-string.svg','SCANNED moving string profiles and per-note stack markers','THRU SCANNED');
for(const [title,file] of [['SCANNED','scanned-main'],['SCAN BODY','scanned-body'],['SCAN HIT','scanned-hit'],['SCAN AMP','scanned-amp']]){if(title==='SCAN HIT'){scannedExample.state.patches[1][16]=1;scannedExample.state.patches[1][20]=24;params.scanned_state=JSON.stringify(scannedExample.state);scannedDoc.read(false);}if(title==='SCAN AMP'){scannedExample.state.patches[1][23]=1;params.scanned_state=JSON.stringify(scannedExample.state);scannedDoc.read(false);}seqDoc.state.view='settings';seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title===title);seqDoc.redraw();capture(file+'.svg',title+' physical patch controls',title);}
// FM uses the same live scope surface as TABLE, with independent patch controls.
const fmDoc=globalThis.__docFm,fmExample=JSON.parse(fs.readFileSync(path.join(repoRoot,'tests/fm_scope_fixture.json'),'utf8'));
Object.assign(seqDoc.state,{view:'main',algorithm:17,splitEnabled:false,sharedDual:false,quadMode:false,bankMode:false,splitFocus:0,lane:2,mask:0,voiceCount:1,playheads:Array(8).fill(-1),stackPositions:[.5,...Array(7).fill(-1)],voiceAlgorithms:[17,...Array(7).fill(-1)],editorReady:true,clipPlaying:false,clipRecording:false,actionMessageTicks:0,messageTicks:0,saveStatus:0});
seqDoc.params.algorithm.value=17;seqDoc.params.split_mode.value=0;seqDoc.params.voice_engine_mode.value=0;fmExample.state.edit=1;fmExample.state.revision=999999;params.fm_state=JSON.stringify(fmExample.state);params.table_scope=JSON.stringify(fmExample.scope);fmDoc.read(false);tableDoc.scopes(false);seqDoc.redraw();capture('fm-scope.svg','FM live scopes and per-note stack positions','THRU FM');
for(const [title,file]of [['FM','fm-main'],['FM COLOR','fm-color'],['FM HIT','fm-hit']]){fmExample.state.patches[1][4]=1;if(title==='FM HIT'){fmExample.state.patches[1][0]=1;fmExample.state.patches[1][20]=20;}params.fm_state=JSON.stringify(fmExample.state);fmDoc.read(false);seqDoc.state.view='settings';seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title===title);seqDoc.redraw();capture(file+'.svg',title+' patch controls',title);}
// Held-trig pages use the same native sound-lock snapshot as the UI tests.
const soundExample=JSON.parse(fs.readFileSync(path.join(repoRoot,'tests/sequence_sound_fixture.json'),'utf8'));
Object.assign(seqDoc.state,{view:'settings',splitEnabled:true,quadMode:true,sharedDual:false,bankMode:false,splitFocus:0,algorithm:15,lane:1,editorReady:true,actionMessageTicks:0,messageTicks:0,saveStatus:0,savedTicks:0});
seqDoc.params.split_mode.value=6;seqDoc.params.algorithm.value=15;
globalThis.__docLayers.model.active=false;globalThis.__docSequence.model.active=true;globalThis.__docSequence.model.held=true;globalThis.__docSequence.model.pad=24;
soundExample.table.revision=9999999;globalThis.__docTable.model.floor=0;
Object.assign(params,{seq_state:JSON.stringify(soundExample.sequence),seq_sound_state:JSON.stringify(soundExample.sound),table_state:JSON.stringify(soundExample.table)});
globalThis.__docSequence.read();globalThis.__docTable.read(false);globalThis.__docSound.read();
seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.title==='TABLE');seqDoc.redraw();capture('sequence-table-locks.svg','TABLE voice locks on a held Sequence trig','T01 A TABLE');
soundExample.sound.values=[[3867,-.5,1],[3871,3,1],[3872,-4.5,1],[3873,2,1],[3874,1400,1]];
params.seq_sound_state=JSON.stringify(soundExample.sound);globalThis.__docSound.read();
seqDoc.state.settingsPage=seqDoc.pages().findIndex(p=>p.sequenceStrip===0);seqDoc.redraw();capture('sequence-lane-locks.svg','Lane EQ locks change the shared lane bus','T01 STRIP 1');
if (checkOnly && mismatches !== 0) process.exit(1);
if (checkOnly) console.log('s3g Thru documentation screens: current');
