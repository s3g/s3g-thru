import assert from 'node:assert/strict';
import fs from 'node:fs';

const uiPath = process.argv[2];
assert(uiPath, 'ui.js path is required');
const testVersion=JSON.parse(fs.readFileSync(new URL('../package.json',import.meta.url),'utf8')).version;

const padWrites = [];
const buttonWrites = [];
const paramWrites = [];
const paramReads = [];
let corruptEditorSnapshot = false;
const blockingWrites = [];
const drawWrites = [];
const screenBits=new Uint8Array(128*64),screenText=new Map();
const screenPixel=(x,y)=>screenBits[y*128+x];
const forcedPadWrites = [];
let emulateSharedLedCache = false;
const sharedLedCache = new Map();
let invalidations = 0;
let exitCount = 0;
let waveformReads = 0;

globalThis.__moveConstants = {
    MovePads: Array.from({ length: 32 }, (_, i) => 68 + i),
    MoveSteps: Array.from({ length: 16 }, (_, i) => 16 + i),
    MovePlay: 85,
    MoveLoop: 58,
    MoveLeft: 62,
    MoveRight: 63,
};
globalThis.__moveInputFilter = {
    setLED(note, color, force = false) {
        if (emulateSharedLedCache && !force && sharedLedCache.get(note) === color) return;
        sharedLedCache.set(note,color);
        padWrites.push([note, color]);forcedPadWrites.push([note,color,force]);
    },
    invalidateLedCache() { sharedLedCache.clear();invalidations++; },
    setButtonLED(cc, color) { buttonWrites.push([cc, color]); },
};
globalThis.__os = {
    readdir(path) {
        if (path === '/data/UserData/UserLibrary/Samples') {
            return [['Drums', 'Atmosphere.wav', 'ignore.txt'], 0];
        }
        if (path === '/data/UserData/UserLibrary/Samples/Drums') {
            return [['Kick 01.wav'], 0];
        }
        if (path === '/data/s3g-thru/captures') {
            return [[
                't0000000001-c01-l1-input.wav',
                't0000000002-c01-l1-fade.wav',
                'unfinished.wav.tmp',
            ], 0];
        }
        return [[], 0];
    },
    stat(path) {
        return [{ mode: path.endsWith('/Drums')
            || path === '/data/s3g-thru/captures' ? 0x4000 : 0x8000 }, 0];
    },
};

const flatWave = '77'.repeat(112);
const params = {
    locus: '0.5', field: '1', speed: '1', manual_lane: '1',
    cut_lane_advance: '1', lane_cycles: '1', stack_scan_paused: '0', clip_stack_hold: '0',
    algorithm: '0', join: '1', output_gain: '1', polyphony_comp: '1',
    glitch_buffer_lock: '0',
    record_lane: '1', pad_octave: '0', record_source: '0',
    record_gain: '1', monitor_level: '0', preview_level: '0.7',
    hold_release_ms: '0', shift_held: '0',
    amp_attack: '3', amp_decay: '0', amp_sustain: '100', amp_release: '25',
    clip_rate: '1', clip_record_mode: '0', clip_timing: '0',
    launch_quantize: '0', grain_quality: '1',
    clip_state: '0,0,0,250,4000,3,10,1000,0,0,12000,8000',
    clip_take_state: '2,1,0',
    clip_capacity_state: '3,1024,10,8192,0,0',
    ui_state: '1,1,0,0,250,100,900,1000,0,0,0,0,20,0,1,1,2',
    playheads: '100,700,-1,-1,-1,-1,-1,-1',
    waveset_state: '', cutups_state: '',
    voice_lanes: '333,800,-1,-1,-1,-1,-1,-1',
    voice_pads: '-1,-1,-1,-1,-1,-1,-1,-1',
    voice_notes: '-1,-1,-1,-1,-1,-1,-1,-1',
    roll_pads: '-1,-1,-1,-1,-1,-1,-1,-1',
    roll_notes: '-1,-1,-1,-1,-1,-1,-1,-1',
    tempo_bpm: '120', tempo_swing: '50',
    chain_enabled: '0', chain_slot: '1',
    chain_clip: '1', chain_repeats: '1',
    clip_bank_state: '0,-1,1,0',
    sample_load_state: '0,1,0,',
    region_state: '2,0,500,1000', pad_mode: '0', scale: '0', root_note: '60',
    cue_state: '-1,-1,-1,-1,-1,-1,-1,-1',
    clip_prepare_state: '0,-1', chain_state: '0,0,0,0,-1',
    chain_entries: ['1:0:2', '1:1:1', ...Array(14).fill('0:0:1')].join(','),
    lane_source: 'LIVE',
    lane_source_state: 'CAPTURE:LIVE', undo_available: '0',
    captures_directory: '/data/s3g-thru/captures',
    performance_state: '724,0', limiter_state: '1.5,3.2',
    last_error: '',
    waveforms: [flatWave, flatWave, flatWave, flatWave].join('|'),
};
globalThis.host_module_get_param = (key) => {
    paramReads.push(key);
    if (key === 'ui_editor_state') {
        if (corruptEditorSnapshot) return '{truncated';
        const values = Object.fromEntries(Object.entries(globalThis.__testParams || {})
            .filter(([,p]) => !p.momentary)
            .map(([key,p]) => [key, Number(params[key] ?? p.value)]));
        const split = JSON.parse(params.split_state || '{"enabled":0,"focus":0,"blend":0.5,"parts":[{},{}]}');
        values.split_mode = split.enabled; values.split_focus = split.focus;
        values.part_mute = split.parts[split.focus]?.mute || 0;
        for (const key of ['pad_octave','record_lane','stack_scan_paused','glitch_buffer_lock'])
            values[key]=Number(params[key] || 0);
        return JSON.stringify({v:1,values,split,voices:params.voice_algorithms || '-1,-1,-1,-1,-1,-1,-1,-1'});
    }
    if (key === 'waveforms') waveformReads++;
    return params[key] ?? '';
};
globalThis.host_module_set_param = (key, value) => {
    paramWrites.push([key, value]);
    if (Object.hasOwn(params, key)) params[key] = value;
};
globalThis.host_module_set_param_blocking = (key, value) => {
    blockingWrites.push([key, value]);
    if (Object.hasOwn(params, key)) params[key] = value;
    if (key === 'effect_order') params.effect_order_state = value + ',' + value + ',0';
    if (key === 'clip_playing') {
        const clip = params.clip_state.split(',');
        clip[1] = value;
        params.clip_state = clip.join(',');
    }
    if (key === 'clip_record') {
        const clip = params.clip_state.split(',');
        const recording = clip[0] === '1';
        clip[0] = recording ? '0' : '1';
        clip[1] = recording ? '1' : (params.clip_record_mode === '1' ? '1' : '0');
        params.clip_state = clip.join(',');
    }
    if (key === 'clip_take_undo') {
        const take = params.clip_take_state.split(',');
        take[0] = String(Math.max(1, Number(take[0]) - 1));
        take[1] = Number(take[0]) > 1 ? '1' : '0';
        params.clip_take_state = take.join(',');
    }
};
globalThis.host_exit_module = () => { exitCount++; };
globalThis.clear_screen = () => {drawWrites.push(['clear']);screenBits.fill(0);screenText.clear();};
globalThis.print = (...args) => {drawWrites.push(['print',...args]);screenText.set(args[0]+','+args[1],args);};
globalThis.fill_rect = (x,y,width,height,color) => {
    drawWrites.push(['rect',x,y,width,height,color]);
    for(let yy=Math.max(0,y);yy<Math.min(64,y+height);++yy)
        for(let xx=Math.max(0,x);xx<Math.min(128,x+width);++xx)screenBits[yy*128+xx]=color?1:0;
    for(const [key,text] of screenText)if(text[0]>=x&&text[0]<x+width&&text[1]>=y&&text[1]<y+height)screenText.delete(key);
};

let source = fs.readFileSync(uiPath, 'utf8');
source = source.replace(
    /import \* as moveConstants[^;]+;/,
    'const moveConstants = globalThis.__moveConstants;');
source = source.replace(
    /import \* as moveInputFilter[^;]+;/,
    'const moveInputFilter = globalThis.__moveInputFilter;');
source = source.replace(
    /import \* as os[^;]+;/,
    'const os = globalThis.__os;');
source += `
globalThis.__testParameterPageSets = PLAYBACK_PAGES.map((enginePages, algorithm) =>
    [algorithm === 3 ? CUTUPS_ENGINE_PAGE : ENGINE_PAGE, PADS_SCALE_PAGE, AMP_ENVELOPE_PAGE, ...enginePages,
        MIX_REC_PAGE, EFFECTS_PAGE, STEREO_ROTARY_PAGE, TAPE_PAGE, REVERB_PAGE, FX_ORDER_PAGE,
        SAMPLE_PAGE, LOOP_PAGE, TAKE_PAGE, CHAIN_PAGE].map((page) => page.keys));
globalThis.__testMotionPages = Array.from({ length: 8 }, (_, sound) =>
    playbackPagesForAlgorithm(0, sound).map((page) => ({
        title: page.title, keys: page.keys,
    })));
globalThis.__testPlaybackPageTitles = PLAYBACK_PAGES.map((pages) =>
    pages.map((page) => page.title));
globalThis.__testMainEngineControls = MAIN_ENGINE_CONTROLS;
globalThis.__testLockableParams = Array.from(LOCKABLE_PARAMS);
globalThis.__testDrawMain = drawMain;
globalThis.__testClearDisplay=clearDisplay;
globalThis.__testParseCutups = parseCutupsState;
globalThis.__testParseUi = parseUiState;
globalThis.__testParams = PARAMS;
globalThis.__testSplit = {pages:parameterPages,parse:parseSplitState,keys:activeControlKeys};
globalThis.__testResetEncoders = resetParameterEncoderMotion;
globalThis.__testLongGrainLengthIndex = longGrainLengthIndex;
globalThis.__testLongGrainLengthAt = longGrainLengthAt;
globalThis.__testHoldReleaseIndex = holdReleaseIndex;
globalThis.__testHoldReleaseAt = holdReleaseAt;
globalThis.__testEnvelopeTimeIndex = envelopeTimeIndex;
globalThis.__testEnvelopeTimeAt = envelopeTimeAt;
globalThis.__testRotaryRampIndex = rotaryRampIndex;
globalThis.__testRotaryRampAt = rotaryRampAt;
globalThis.__testRotaryCrossoverIndex = rotaryCrossoverIndex;
globalThis.__testRotaryCrossoverAt = rotaryCrossoverAt;
globalThis.__testFormatControlValue = formatControlValue;
globalThis.__testCompactControlValue = compactControlValue;
globalThis.__testPadMidiNote = padMidiNote;
globalThis.__testStereoRotaryPage = STEREO_ROTARY_PAGE.keys;
globalThis.__testTapePage = TAPE_PAGE.keys;
globalThis.__testWavesetProcess = PARAMS.waveset_process;
globalThis.__testFx = { adjust: adjustFxOrder, sync: syncFxOrder,
    encode: encodeFxOrder, decode: decodeFxOrder,
    state: () => state,
    parseClip: parseClipBankState, draw: drawSettings,
    pageIndex: () => parameterPages().indexOf(FX_ORDER_PAGE) };
`;
await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));

assert.equal(typeof globalThis.init, 'function');
assert.equal(typeof globalThis.tick, 'function');
assert.equal(typeof globalThis.onMidiMessageInternal, 'function');
assert.equal(typeof globalThis.onUnload, 'function');
for (const pageSet of globalThis.__testParameterPageSets) {
    const keys = pageSet.flat().filter(Boolean);
    assert.equal(new Set(keys).size, keys.length,
        'a parameter should appear on only one active page');
}
for (const motionPages of globalThis.__testMotionPages) {
    const keys = motionPages.flatMap((page) => page.keys).filter(Boolean);
    assert.equal(new Set(keys).size, keys.length,
        'a Motion parameter should appear on only one active page');
}
assert.deepEqual(globalThis.__testMotionPages[0].map((page) => page.title),
    ['STACK PATH', 'MOTION PATH', 'MOTION SOUND']);
assert.equal(globalThis.__testMotionPages[1].at(-1).title, 'PACKETS');
assert.equal(globalThis.__testMotionPages[2].at(-1).title, 'MOTOR');
for (let sound = 3; sound < 8; sound++) {
    assert.deepEqual(globalThis.__testMotionPages[sound].slice(-2)
        .map((page) => page.title), ['EVENT CLOCK', 'EVENT SHAPE']);
}
assert.deepEqual(globalThis.__testParameterPageSets[0][1],
    ['pad_mode', 'root_note', 'scale', 'hold_release_ms', 'polyphony_comp']);
assert.deepEqual(globalThis.__testParameterPageSets[0][2],
    ['amp_attack', 'amp_decay', 'amp_sustain', 'amp_release']);
assert.deepEqual(globalThis.__testParameterPageSets[0][3],
    globalThis.__testParameterPageSets[1][3]);
assert.deepEqual(globalThis.__testParameterPageSets[1][3],
    globalThis.__testParameterPageSets[2][3]);
assert.deepEqual(globalThis.__testParameterPageSets[4][3],
    globalThis.__testParameterPageSets[5][3]);
assert.deepEqual(globalThis.__testPlaybackPageTitles[6],
    ['STACK PATH', 'GLITCH CLOCK', 'GLITCH PATTERN']);
assert.deepEqual(globalThis.__testPlaybackPageTitles[7],
    ['STACK PATH', 'WEAVE', 'WEAVE MOTION']);
assert.deepEqual(globalThis.__testPlaybackPageTitles[8],
    ['STACK PATH', 'WAVESETS']);
const waveformModeControls = ['motion_mode', 'lanes_mode', 'grain_mode',
    'cutups_mode', 'sample_mode', 'sample_mode', 'glitch_mode',
    'long_grain_mode', 'waveset_direction'];
for (let algorithm = 0; algorithm < 9; algorithm++) {
    assert.deepEqual(globalThis.__testMainEngineControls[algorithm], [
        'locus', 'field', 'speed', 'manual_lane', algorithm === 3 ? 'cut_lane_advance' : 'lane_cycles', 'join',
        waveformModeControls[algorithm], 'output_gain',
    ], 'the waveform encoder layout must stay stable across engines');
}
assert(globalThis.__testLockableParams.includes('glitch_mix'));
assert(globalThis.__testLockableParams.includes('effect_feedback'));
assert(globalThis.__testLockableParams.includes('effect_cross_pan'));
assert(globalThis.__testLockableParams.includes('effect_rotary_mix'));
assert(globalThis.__testLockableParams.includes('effect_rotary_ramp'));
assert(globalThis.__testLockableParams.includes('effect_rotary_balance'));
assert(globalThis.__testLockableParams.includes('effect_rotary_crossover'));
assert(globalThis.__testLockableParams.includes('effect_rotary_spread'));
assert.deepEqual(globalThis.__testStereoRotaryPage, [
    'effect_cross_pan', 'effect_rotary_rate', 'effect_rotary_depth',
    'effect_rotary_spread', 'effect_rotary_ramp', 'effect_rotary_balance',
    'effect_rotary_crossover', 'effect_rotary_mix',
]);
assert.deepEqual(globalThis.__testTapePage, [
    'effect_tape_format', 'effect_tape_drive', 'effect_tape_age',
    'effect_tape_crinkle', 'effect_tape_mechanics',
    'effect_tape_inertia', 'effect_tape_hiss', 'effect_tape_mix',
]);
for (const key of globalThis.__testTapePage) {
    assert(globalThis.__testLockableParams.includes(key));
}
assert.equal(globalThis.__testFormatControlValue({
    key: 'effect_tape_hiss', min: -84, value: -84,
}), 'OFF');
assert.equal(globalThis.__testCompactControlValue({
    key: 'effect_tape_inertia', value: 32,
}), '203m');
assert(globalThis.__testLockableParams.includes('long_grain_length'));
assert.deepEqual(globalThis.__testWavesetProcess.labels, [
    'REPEAT', 'OMIT', 'REPLACE', 'ENVELOPE', 'MULTIPLY', 'INTERPOLATE',
    'FRACTAL', 'HARMONIC', 'GROUP REV', 'CYCLE REV',
]);
assert.equal(globalThis.__testWavesetProcess.max, 9);
for (const key of ['waveset_group', 'waveset_repeats', 'waveset_stride',
    'waveset_advance', 'waveset_direction', 'waveset_process',
    'waveset_amount', 'waveset_detail']) {
    assert(globalThis.__testLockableParams.includes(key));
}
assert(globalThis.__testLockableParams.includes('amp_attack'));
assert(globalThis.__testLockableParams.includes('amp_release'));
assert.equal(globalThis.__testLongGrainLengthAt(0), 250);
assert.equal(globalThis.__testLongGrainLengthAt(75), 1000);
assert.equal(globalThis.__testLongGrainLengthAt(135), 4000);
assert.equal(globalThis.__testLongGrainLengthAt(215), 12000);
for (const length of [250, 500, 1000, 2000, 4000, 8000, 12000]) {
    assert.equal(globalThis.__testLongGrainLengthAt(
        globalThis.__testLongGrainLengthIndex(length)), length);
}
assert.equal(globalThis.__testHoldReleaseAt(0), 0);
assert.equal(globalThis.__testHoldReleaseAt(20), 1000);
assert.equal(globalThis.__testHoldReleaseAt(36), 5000);
assert.equal(globalThis.__testHoldReleaseAt(61), 30000);
assert.equal(globalThis.__testHoldReleaseAt(67), 60000);
for (const delay of [0, 50, 500, 1000, 2500, 5000, 15000, 30000, 60000]) {
    assert.equal(globalThis.__testHoldReleaseAt(
        globalThis.__testHoldReleaseIndex(delay)), delay);
}
for (const time of [0, 1, 3, 10, 50, 100, 500, 1000, 4000, 10000]) {
    assert.equal(globalThis.__testEnvelopeTimeAt(
        globalThis.__testEnvelopeTimeIndex(time, 10000), 10000), time);
}
assert.equal(globalThis.__testEnvelopeTimeAt(29, 20000), 20000);
for (const time of [50, 100, 350, 500, 1000, 2000, 4000, 8000]) {
    assert.equal(globalThis.__testRotaryRampAt(
        globalThis.__testRotaryRampIndex(time)), time);
}
for (const frequency of [250, 400, 500, 800, 1000, 1250, 2000]) {
    assert.equal(globalThis.__testRotaryCrossoverAt(
        globalThis.__testRotaryCrossoverIndex(frequency)), frequency);
}

/* A recalled named scale must color the RGB pad grid during initialization,
   before any note input or periodic LED-queue drain. RGB light grey/white are
   deliberately used instead of WhiteLedDim, whose numeric value is blue in
   the RGB palette. */
params.scale = '1';
globalThis.init();
assert.deepEqual(padWrites.filter((write) => write[0] === 68).at(-1),
    [68, 120], 'the recalled tonic should light white immediately');
assert.deepEqual(padWrites.filter((write) => write[0] === 69).at(-1),
    [69, 123], 'other recalled scale notes should light dark neutral grey');
assert.equal(padWrites.filter((write) => write[0] >= 68
    && write[0] < 100).slice(-32).length, 32,
    'initialization should prime all 32 pad LEDs');

params.scale = '0';
globalThis.init();
const waveformReadsAfterInit = waveformReads;
assert(drawWrites.length > 100, 'waveform page should draw');
assert(drawWrites.some((write) => write[0] === 'print'
    && String(write[3]).includes('A01 V')),
    'waveform header should identify the active performance clip');
assert(drawWrites.some((write)=>write[0]==='rect'&&write[1]===14&&write[2]===10&&write[3]===1&&write[4]===40));
assert(drawWrites.some((write)=>write[0]==='rect'&&write[1]===99&&write[2]===10&&write[3]===1&&write[4]===40));
assert(!drawWrites.some((write)=>write[0]==='rect'&&write[1]===14&&write[2]===10&&write[3]===1&&write[4]===2),
    'Single zoom replaces the old Position tick');
assert(!drawWrites.some((write)=>write[0]==='rect'&&write[1]===127&&write[2]===48&&write[3]===1&&write[4]===2),
    'Single zoom replaces the old Field tick');
assert(drawWrites.some((write)=>write[0]==='rect'&&write[1]===12&&write[2]===23&&write[3]===5&&write[4]===5&&write[5]===0));
assert(drawWrites.some((write)=>write[0]==='rect'&&write[1]===12&&write[2]===23&&write[3]===5&&write[4]===1&&write[5]===1));
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 71 && write[2] === 11 && write[3] === 1
    && write[4] === 2), 'selected waveform should show transient markers');
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 0 && write[2] === 53 && write[3] === 32),
    'performance loop position should be drawn below the waveforms');

/* DSP voice slots have stable colors. Two isomorphic pads that produce the
   same pitch can therefore show two independently identifiable playheads. */
params.voice_pads = '-1,-1,-1,-1,-1,-1,-1,-1';
params.voice_pads = '5,8,-1,-1,-1,-1,-1,-1';
params.voice_notes = '53,53,-1,-1,-1,-1,-1,-1';
for (let tick = 0; tick < 6; tick++) globalThis.tick();
assert.equal(waveformReads, waveformReadsAfterInit + 2,
    'each DSP poll should advance or publish waveform analysis');
assert(padWrites.some((write) => write[0] === 73 && write[1] === 1));
assert(padWrites.some((write) => write[0] === 76 && write[1] === 3));
params.voice_pads = '-1,-1,-1,-1,-1,-1,-1,-1';
params.voice_notes = '-1,-1,-1,-1,-1,-1,-1,-1';
for (let tick = 0; tick < 3; tick++) globalThis.tick();

/* Wavesets replaces the ambiguous generic cursor with source-accurate heads
   and a group-local order rail. The newest voice reports active cycle/repeat
   state, a pending Order change, and the outgoing overlap reader. */
params.algorithm = '8';
params.waveset_state = '0,8,1,0,2,1,2,10,250,656,188,760,230,31';
for (let tick = 0; tick < 90; tick++) globalThis.tick();
const wavesetDrawStart = drawWrites.length;
globalThis.__testDrawMain();
const wavesetWrites = drawWrites.slice(wavesetDrawStart);
assert(wavesetWrites.some((write) => write[0] === 'print'
    && String(write[3]).includes('WS <*~ C2/8 R1/2')),
    'Wavesets footer should identify active order, pending state, cycle, and repeat');
assert(wavesetWrites.some((write) => write[0] === 'rect'
    && write[1] === 107 && write[2] === 10 && write[4] === 40),
    'Wavesets primary cursor should follow the audible processed source');
assert(wavesetWrites.some((write) => write[0] === 'rect'
    && write[1] === 32 && write[2] === 10 && write[4] === 2),
    'Wavesets secondary source should use a sparse cursor');
assert(wavesetWrites.some((write) => write[0] === 'rect'
    && write[2] === 51 && write[3] === 114 && write[4] === 1),
    'Wavesets should draw a group-local cycle rail');
params.waveset_analysis_state = '1,2,3';
for (let tick = 0; tick < 3; tick++) globalThis.tick();
const preparingStart = drawWrites.length;
globalThis.__testDrawMain();
assert(drawWrites.slice(preparingStart).some((write) => write[0] === 'print'
    && String(write[3]).includes('WS PREPARING')));
params.waveset_analysis_state = '3,0,0';
for (let tick = 0; tick < 3; tick++) globalThis.tick();
params.pad_mode = '2';
for (let tick = 0; tick < 3; tick++) globalThis.tick();
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0x90, 68, 100]);
assert.deepEqual(blockingWrites.at(-1), ['cue_store', '0,0.7600'],
    'cue storage should follow the source-accurate Wavesets cursor');
globalThis.onMidiMessageInternal([0x80, 68, 0]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
params.pad_mode = '0';
params.algorithm = '0';
params.waveset_state = '';
for (let tick = 0; tick < 3; tick++) globalThis.tick();

globalThis.onMidiMessageInternal([0xB0, 42, 127]);
assert.deepEqual(blockingWrites.pop(), ['record_lane', '2']);

/* Record is now the free-time performance-data recorder. Shift+Record retains
   stereo audio capture, so the two recording layers cannot be confused. */
globalThis.onMidiMessageInternal([0xB0, 86, 127]);
assert.deepEqual(blockingWrites.pop(), ['clip_record', '1']);
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
assert.deepEqual(paramWrites.slice(-2), [
    ['locus', '0.5100'],
    ['clip_automation', 'locus'],
]);
/* Parameter encoders run at 50% response: four physical detents produce
   two existing value steps without putting parameters between their grids. */
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
assert.deepEqual(paramWrites.slice(-2), [
    ['locus', '0.5200'],
    ['clip_automation', 'locus'],
]);
globalThis.onMidiMessageInternal([0xB0, 71, 127]);
globalThis.onMidiMessageInternal([0xB0, 71, 127]);
globalThis.onMidiMessageInternal([0xB0, 71, 127]);
globalThis.onMidiMessageInternal([0xB0, 71, 127]);
assert.deepEqual(paramWrites.slice(-4), [
    ['locus', '0.5100'],
    ['clip_automation', 'locus'],
    ['locus', '0.5000'],
    ['clip_automation', 'locus'],
]);
globalThis.onMidiMessageInternal([0xB0, 86, 127]);
assert.deepEqual(blockingWrites.pop(), ['clip_record', '1']);
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
assert.deepEqual(paramWrites.at(-1), ['shift_held', '1']);
globalThis.onMidiMessageInternal([0xB0, 86, 127]);
assert.deepEqual(blockingWrites.pop(), ['record_gate', '1']);
globalThis.onMidiMessageInternal([0xB0, 86, 127]);
assert.deepEqual(blockingWrites.slice(-2), [
    ['record_gate', '0'],
    ['maintenance', '1'],
]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
assert.deepEqual(paramWrites.at(-1), ['shift_held', '0']);

/* Delete+Record removes only the most recent overdub take. */
params.clip_take_state = '3,1,0';
globalThis.onMidiMessageInternal([0xB0, 119, 127]);
globalThis.onMidiMessageInternal([0xB0, 86, 127]);
assert.deepEqual(blockingWrites.pop(), ['clip_take_undo', '1']);
assert.equal(params.clip_take_state, '2,1,0');
globalThis.onMidiMessageInternal([0xB0, 119, 0]);

/* Every Step key is a performance-clip slot in the visible 16-slot bank. */
globalThis.onMidiMessageInternal([0x90, 19, 127]);
assert.deepEqual(blockingWrites.pop(), ['clip_select', '3']);
globalThis.onMidiMessageInternal([0xB0, 119, 127]);
globalThis.onMidiMessageInternal([0x90, 18, 127]);
assert.deepEqual(blockingWrites.pop(), ['clip_reset', '2']);
globalThis.onMidiMessageInternal([0xB0, 119, 0]);
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0x90, 17, 127]);
assert.deepEqual(blockingWrites.pop(), ['clip_copy', '1']);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);

/* Left/Right changes A-D banks. The selected bank is used immediately by the
   Step row without entering a separate sequencing mode. */
globalThis.onMidiMessageInternal([0xB0, 63, 127]);
globalThis.onMidiMessageInternal([0x90, 16, 127]);
assert.deepEqual(blockingWrites.pop(), ['clip_select', '16']);
globalThis.onMidiMessageInternal([0xB0, 62, 127]);

globalThis.onMidiMessageInternal([0xB0, 55, 127]);
assert.deepEqual(blockingWrites.at(-1), ['pad_octave', '1']);
assert.notDeepEqual(blockingWrites.at(-2), ['all_notes_off', '1']);

/* A held voice keeps physical-pad ownership across octave changes. Its exact
   voice color is dimmed while the pad maps elsewhere, then restored when the
   original octave returns. */
params.voice_pads = '0,-1,-1,-1,-1,-1,-1,-1';
params.voice_notes = '48,-1,-1,-1,-1,-1,-1,-1';
for (let tick = 0; tick < 3; tick++) globalThis.tick();
assert.deepEqual(padWrites.filter((write) => write[0] === 68).at(-1), [68, 65]);
globalThis.onMidiMessageInternal([0xB0, 54, 127]);
globalThis.tick();
assert.deepEqual(padWrites.filter((write) => write[0] === 68).at(-1), [68, 1]);
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
globalThis.tick();
params.voice_pads = '-1,-1,-1,-1,-1,-1,-1,-1';
params.voice_notes = '-1,-1,-1,-1,-1,-1,-1,-1';
for (let tick = 0; tick < 3; tick++) globalThis.tick();

globalThis.onMidiMessageInternal([0x90, 68, 100]);
globalThis.onMidiMessageInternal([0xA0, 68, 64]);
globalThis.tick();
assert(!paramWrites.some((write) => write[0] === 'performance_lane'));
globalThis.onMidiMessageInternal([0x80, 68, 0]);
for (let tick = 0; tick < 5; tick++) globalThis.tick();
assert(padWrites.some((write) => write[0] === 68));
assert.deepEqual(padWrites.filter((write) => write[0] === 68).at(-1), [68, 0]);

/* Menu opens a compact page list for the selected engine. Each physical
   encoder gets a padded name/value cell, without a dial glyph. */
globalThis.onMidiMessageInternal([0xB0, 50, 127]);
const settingsDrawStart = drawWrites.length;
globalThis.tick();
const settingsLines = drawWrites.slice(settingsDrawStart).filter((write) =>
    write[0] === 'print' && write[2] >= 10);
assert.equal(settingsLines.length, 16, 'eight dials need separate name/value rows');
assert.deepEqual([...new Set(settingsLines.map((write) => write[2]))],
    [13, 24, 39, 50]);
assert(!drawWrites.slice(settingsDrawStart).some((write) => write[0] === 'rect'
    && write[3] === 7 && write[4] === 1), 'parameter cells should not draw dials');
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
assert.deepEqual(blockingWrites.pop(), ['algorithm', '1']);

/* Pads and Scale is shared by every engine. Named scales fold the grid to
   consecutive scale degrees with overlapping rows; Chromatic is unchanged. */
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
const scalePageDrawStart = drawWrites.length;
globalThis.tick();
const scaleHeader = drawWrites.slice(scalePageDrawStart).find((write) =>
    write[0] === 'print' && write[2] === 0);
assert(String(scaleHeader?.[3]).includes('PADS & SCALE 2/15'));
assert(drawWrites.slice(scalePageDrawStart).some((write) =>
    write[0] === 'print' && String(write[3]).includes('POLY')));
globalThis.onMidiMessageInternal([0xB0, 73, 1]);
assert.deepEqual(paramWrites.at(-1), ['scale', '1']);
assert.equal(globalThis.__testPadMidiNote(0), 60);
assert.equal(globalThis.__testPadMidiNote(1), 62);
assert.equal(globalThis.__testPadMidiNote(7), 72);
assert.equal(globalThis.__testPadMidiNote(8), 65);

/* The shared amp envelope is a breakpoint editor rather than eight generic
   cells. The first four encoders edit ADSR and draw its gate boundary. */
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
const envelopePageDrawStart = drawWrites.length;
globalThis.tick();
const envelopeWrites = drawWrites.slice(envelopePageDrawStart);
assert(envelopeWrites.some((write) => write[0] === 'print'
    && String(write[3]).includes('AMP ENVELOPE 3/15')));
for (const label of ['ATK', 'DEC', 'SUS', 'REL']) {
    assert(envelopeWrites.some((write) => write[0] === 'print'
        && write[3] === label));
}
assert(envelopeWrites.filter((write) => write[0] === 'rect'
    && write[3] === 1 && write[4] === 1).length > 30,
    'breakpoint envelope should draw connected curve pixels');
for (const x of [14, 46, 78, 110]) {
    assert(envelopeWrites.some((write) => write[0] === 'rect'
        && write[1] === x && write[2] === 57 && write[3] === 4
        && write[4] === 4 && write[5] === 1),
    'knobs 5-8 should remain visibly reserved');
}
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
assert.deepEqual(paramWrites.at(-1), ['amp_attack', '5']);

/* Only pages belonging to the chosen playback engine are exposed. */
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
const lanesPageDrawStart = drawWrites.length;
globalThis.tick();
const lanesHeader = drawWrites.slice(lanesPageDrawStart).find((write) =>
    write[0] === 'print' && write[2] === 0);
assert(String(lanesHeader?.[3]).includes('STACK PATH 4/15'));
assert(!drawWrites.slice(lanesPageDrawStart).some((write) =>
    write[0] === 'print' && String(write[3]).includes('GRAIN')));
const lanesPageWrites = drawWrites.slice(lanesPageDrawStart);
assert.equal(lanesPageWrites.filter((write) => write[0] === 'print'
    && write[2] >= 10).length, 14,
    'Stack Path should show seven controls and one reserved slot');
assert(lanesPageWrites.some((write) => write[0] === 'rect'
    && write[1] === 110 && write[2] === 46 && write[3] === 4
    && write[4] === 4 && write[5] === 1),
    'a reserved encoder slot should use a small hollow box');
const writesBeforeReservedTurn = paramWrites.length;
globalThis.onMidiMessageInternal([0xB0, 78, 1]);
assert.equal(paramWrites.length, writesBeforeReservedTurn,
    'turning a reserved encoder slot should do nothing');

/* The take page displays the layer count and provides the same action as
   Delete+Record. The following Chain page retains its explicit slot clear. */
    for (let page = 0; page < 10; page++) {
    globalThis.onMidiMessageInternal([0xB0, 55, 127]);
}
const takePageDrawStart = drawWrites.length;
globalThis.tick();
assert(drawWrites.slice(takePageDrawStart).some((write) => write[0] === 'print'
    && String(write[3]).includes('OVERDUB TAKES 2')));
globalThis.onMidiMessageInternal([0xB0, 72, 1]);
assert.deepEqual(blockingWrites.pop(), ['clip_take_undo', '1']);
assert.equal(params.clip_take_state, '1,0,0');
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
globalThis.onMidiMessageInternal([0xB0, 75, 1]);
assert.deepEqual(blockingWrites.pop(), ['chain_clear', '1']);

globalThis.onMidiMessageInternal([0xB0, 85, 127]);
assert.deepEqual(blockingWrites.pop(), ['clip_playing', '0']);
globalThis.onMidiMessageInternal([0xB0, 85, 127]);
assert.deepEqual(blockingWrites.pop(), ['clip_playing', '1']);
for (let tick = 0; tick < 3; tick++) globalThis.tick();
assert.deepEqual(buttonWrites.filter((write) => write[0] === 85).at(-1),
    [85, 11], 'Play should turn green for a running performance loop');

/* Loop holds every voice at its exact current stack blend and lights while
   held; pressing it again resumes independent stack motion. */
globalThis.onMidiMessageInternal([0xB0, 58, 127]);
assert.deepEqual(blockingWrites.pop(), ['stack_scan_paused', '1']);
for (let tick = 0; tick < 3; tick++) globalThis.tick();
assert.deepEqual(buttonWrites.filter((write) => write[0] === 58).at(-1),
    [58, 127], 'Loop should light brightly while stack scanning is held');
globalThis.onMidiMessageInternal([0xB0, 58, 127]);
assert.deepEqual(blockingWrites.pop(), ['stack_scan_paused', '0']);

/* A stack hold stored in performance data is visible even after the physical
   Loop hold has returned to its live Run state. */
params.clip_stack_hold = '1';
const recordedHoldDrawStart = drawWrites.length;
for (let tick = 0; tick < 90; tick++) globalThis.tick();
globalThis.__testDrawMain();
assert(drawWrites.slice(recordedHoldDrawStart).some((write) =>
    write[0] === 'print' && String(write[3]).includes('RECORDED STACK HOLD')));
params.clip_stack_hold = '0';
for (let tick = 0; tick < 3; tick++) globalThis.tick();

/* Shift+Loop temporarily freezes each Glitch voice's rolling history. A
   second quick tap latches the lock; one more chord releases it. Plain Loop
   remains Stack Hold and is never overloaded. */
params.algorithm = '6';
for (let tick = 0; tick < 3; tick++) globalThis.tick();
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 58, 127]);
assert.deepEqual(blockingWrites.pop(), ['glitch_buffer_lock', '1']);
for (let tick = 0; tick < 3; tick++) globalThis.tick();
assert.deepEqual(buttonWrites.filter((write) => write[0] === 58).at(-1),
    [58, 3], 'Loop should turn orange while Glitch history is locked');
globalThis.onMidiMessageInternal([0xB0, 58, 0]);
assert.deepEqual(blockingWrites.pop(), ['glitch_buffer_lock', '0']);
globalThis.onMidiMessageInternal([0xB0, 58, 127]);
assert.deepEqual(blockingWrites.pop(), ['glitch_buffer_lock', '1']);
globalThis.onMidiMessageInternal([0xB0, 58, 0]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
assert.equal(params.glitch_buffer_lock, '1',
    'the second quick Shift+Loop tap should latch Buffer Lock');
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 58, 127]);
assert.deepEqual(blockingWrites.pop(), ['glitch_buffer_lock', '0']);
globalThis.onMidiMessageInternal([0xB0, 58, 0]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
params.algorithm = '1';
for (let tick = 0; tick < 3; tick++) globalThis.tick();

/* Shift+Play opens the optional clip overview. */
globalThis.onMidiMessageInternal([0xB0, 50, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 85, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
const clipDrawStart = drawWrites.length;
globalThis.tick();
assert(drawWrites.slice(clipDrawStart).some((write) => write[0] === 'print'
    && String(write[3]).includes('PERF CLIPS A01')));
globalThis.onMidiMessageInternal([0xB0, 42, 127]);
globalThis.onMidiMessageInternal([0x90, 17, 127]);
assert.deepEqual(blockingWrites.pop(), ['clip_select', '17']);
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 85, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);

const chainDrawStart = drawWrites.length;
globalThis.tick();
assert(drawWrites.slice(chainDrawStart).some((write) => write[0] === 'print'
    && String(write[3]).includes('CHAIN OFF')));
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 3, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
assert.deepEqual(blockingWrites.at(-1), ['chain_insert', '0,0']);
globalThis.onMidiMessageInternal([0xB0, 119, 127]);
globalThis.onMidiMessageInternal([0x90, 17, 127]);
globalThis.onMidiMessageInternal([0xB0, 119, 0]);
assert.deepEqual(blockingWrites.slice(-2), [
    ['chain_delete', '1'], ['chain_slot', '2'],
]);
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0x90, 18, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
assert.deepEqual(blockingWrites.slice(-2), [
    ['chain_move', '1,2'], ['chain_slot', '3'],
]);

/* Shift+Track opens the device sample library for that lane. Directories and
   WAVs are browsed with the main encoder/button, then handed to the worker. */
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 41, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
globalThis.onMidiMessageInternal([0xB0, 3, 127]);
globalThis.onMidiMessageInternal([0xB0, 3, 127]);
globalThis.onMidiMessageInternal([0xB0, 14, 1]);
assert(paramWrites.some((write) => write[0] === 'preview_file'
    && write[1].endsWith('/Drums/Kick 01.wav')));
globalThis.onMidiMessageInternal([0xB0, 3, 127]);
assert.deepEqual(blockingWrites.pop(), ['load_lane_file',
    '3,/data/UserData/UserLibrary/Samples/Drums/Kick 01.wav']);

/* The same browser exposes immutable Thru recordings, newest first. */
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 41, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
globalThis.onMidiMessageInternal([0xB0, 14, 1]);
globalThis.onMidiMessageInternal([0xB0, 3, 127]);
assert(paramWrites.some((write) => write[0] === 'preview_file'
    && write[1] === '/data/s3g-thru/captures/t0000000002-c01-l1-fade.wav'));
globalThis.onMidiMessageInternal([0xB0, 3, 127]);
assert.deepEqual(blockingWrites.pop(), ['load_lane_file',
    '3,/data/s3g-thru/captures/t0000000002-c01-l1-fade.wav']);

/* Cue pad mode uses Shift+pad to store the displayed playhead, a normal pad
   to jump, and Delete+pad to clear. */
params.pad_mode = '2';
globalThis.tick(); globalThis.tick(); globalThis.tick();
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0x90, 68, 100]);
assert.equal(blockingWrites.at(-1)[0], 'cue_store');
globalThis.onMidiMessageInternal([0x80, 68, 0]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
globalThis.onMidiMessageInternal([0x90, 68, 100]);
assert.deepEqual(blockingWrites.at(-1), ['cue_trigger', '0']);
globalThis.onMidiMessageInternal([0x80, 68, 0]);
globalThis.onMidiMessageInternal([0xB0, 119, 127]);
globalThis.onMidiMessageInternal([0x90, 68, 100]);
assert.deepEqual(blockingWrites.at(-1), ['cue_clear', '0']);
globalThis.onMidiMessageInternal([0x80, 68, 0]);
globalThis.onMidiMessageInternal([0xB0, 119, 0]);

/* Roll pads report their repeating six-division layout on press. */
params.pad_mode = '3';
globalThis.tick(); globalThis.tick(); globalThis.tick();
const rollDrawStart = drawWrites.length;
globalThis.onMidiMessageInternal([0x90, 73, 100]);
globalThis.tick();
assert(drawWrites.slice(rollDrawStart).some((write) =>
    write[0] === 'print' && String(write[3]).includes('ROLL 1/32')));
globalThis.onMidiMessageInternal([0x80, 73, 0]);
params.pad_mode = '0';

/* Shift+Menu is help. Back returns to the waveform before a second Back
   exits, which is the DSP persistence save boundary. */
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 50, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
params.performance_state = '12345678,987654321';
params.limiter_state = '12.3,45.6';
params.callback_performance_state = '12345,67890,9876543,1234567';
const helpDrawStart = drawWrites.length;
for (let tick = 0; tick < 60; tick++) globalThis.tick();
const reportWrites = drawWrites.slice(helpDrawStart).filter((write) =>
    write[0] === 'print');
assert(reportWrites.some((write) => String(write[3]).includes('SYSTEM REPORT')));
assert(reportWrites.some((write) => String(write[3]).includes('12.3M us')));
assert(reportWrites.some((write) => String(write[3]).includes('987.7M')));
assert(reportWrites.some((write) => String(write[3]).includes('12.3 dB')));
assert(reportWrites.some((write) => String(write[3]).includes('45.6 dB')));
assert(reportWrites.every((write) => String(write[3]).length <= 21),
    'diagnostic report rows must fit the Move display');
globalThis.onMidiMessageInternal([0xB0, 63, 127]);
const callbackDrawStart = drawWrites.length;
globalThis.tick();
const callbackRows = drawWrites.slice(callbackDrawStart).filter((write) => write[0] === 'print');
assert(callbackRows.some((write) => String(write[3]).includes('CALLBACK WORK 2/3')));
assert(callbackRows.some((write) => String(write[3]).includes('12345 us')));
assert(callbackRows.some((write) => String(write[3]).includes('1.2M')));
assert(callbackRows.every((write) => String(write[3]).length <= 21));
globalThis.onMidiMessageInternal([0xB0, 63, 127]);
const shortcutDrawStart = drawWrites.length;
globalThis.tick();
assert(drawWrites.slice(shortcutDrawStart).some((write) =>
    write[0] === 'print' && String(write[3]).includes('SHORTCUTS 3/3')));
globalThis.onMidiMessageInternal([0xB0, 51, 127]);
assert.equal(exitCount, 0, 'Back closes settings/help before exiting');
globalThis.onMidiMessageInternal([0xB0, 51, 127]);
assert.equal(exitCount, 1);

globalThis.onUnload();
assert(blockingWrites.some((write) => write[0] === 'all_notes_off'));
assert(buttonWrites.length > 0);



// Exercise the real main-wheel, click, Back and preset dispatch. Merely
// selecting/lifting/moving never changes sound or starts performance recording.
const fx = globalThis.__testFx;
for (let code = 0; code < 720; ++code) assert.equal(fx.encode(fx.decode(code)), code);
params.effect_order = '0'; params.effect_order_state = '0,0,0';
fx.state().view = 'settings'; fx.state().settingsPage = fx.pageIndex();
fx.state().shiftHeld = false; fx.state().deleteHeld = false;
fx.state().clipRecording = false; fx.state().clipPlaying = false;
fx.state().fxRouteClip = -1; fx.state().fxRoutePending = false; fx.sync();
const routeCount = () => blockingWrites.filter(([key]) => key === 'effect_order').length;
const recordingCount = () => blockingWrites.filter(([key]) => key === 'clip_record').length;
const beforeRouteWrites = routeCount(), beforeRecordWrites = recordingCount();
const ccFx = (number, value = 127) => globalThis.onMidiMessageInternal([0xB0, number, value]);
for (let turn = 0; turn < 5; ++turn) ccFx(14, 1);
assert.equal(fx.state().fxRouteSelected, 5); // Reverb
ccFx(3);
assert.equal(fx.state().fxRouteGrabbed, true);
fx.sync(); // Idle polling cannot drop a lifted block before its first move.
assert.equal(fx.state().fxRouteGrabbed, true);
for (let turn = 0; turn < 3; ++turn) ccFx(14, 127);
assert.deepEqual(fx.state().fxOrder, [0,1,5,2,3,4]);
assert.equal(fx.state().fxRouteSelected, 2);
assert.equal(routeCount(), beforeRouteWrites);
assert.equal(recordingCount(), beforeRecordWrites);
let drawStartFx = drawWrites.length; fx.draw();
const editText = drawWrites.slice(drawStartFx).filter(([kind]) => kind === 'print');
assert(editText.some((write) => write[3] === '*3 Reverb'));
assert(editText.some((write) => write[3] === 'TURN MOVE CLICK SET'));
assert(editText.every((write) => write[2] >= 0 && write[2] <= 57 && String(write[3]).length <= 21));
ccFx(51); // cancel, stay in the editor
assert.equal(fx.state().view, 'settings');
assert.equal(fx.state().fxRouteGrabbed, false);
assert.deepEqual(fx.state().fxOrder, [0,1,2,3,4,5]);
assert.equal(routeCount(), beforeRouteWrites);
// Drop during recording emits exactly one complete routing event.
ccFx(3); ccFx(14, 1); fx.state().clipRecording = true; ccFx(3);
assert.equal(routeCount(), beforeRouteWrites + 1);
assert.equal(recordingCount(), beforeRecordWrites);
assert(paramWrites.some(([key,value]) => key === 'clip_automation' && value === 'effect_order'));
assert.equal(fx.state().fxRouteGrabbed, false);
assert.equal(fx.state().fxRoutePending, false);
fx.state().clipRecording = false;
// Knobs 1-6 and 8 have no legacy slot/apply action. Knob 7 stages presets.
const heldOrder = [...fx.state().fxOrder];
for (const number of [71,72,73,74,75,76,78]) ccFx(number, 1);
assert.deepEqual(fx.state().fxOrder, heldOrder);
for (let turn = 0; turn < 12; ++turn) ccFx(77, 1);
assert.equal(fx.state().fxRoutePending, true);
assert.equal(routeCount(), beforeRouteWrites + 1);
ccFx(3);
assert.equal(routeCount(), beforeRouteWrites + 2);
// Page navigation and actual clip selection both discard unfinished moves.
ccFx(3); ccFx(14, 1); ccFx(55);
assert.equal(fx.state().fxRouteGrabbed, false);
assert.equal(fx.state().fxRoutePending, false);
ccFx(54); ccFx(3); ccFx(14, 1);
params.effect_order_state = '0,0,0';
fx.parseClip(((fx.state().activeClip + 1) % 64) + ',-1,1,0');
assert.equal(fx.state().fxRouteGrabbed, false);
assert.equal(fx.state().fxRoutePending, false);
assert.deepEqual(fx.state().fxOrder, [0,1,2,3,4,5]);
// Leaving through Menu cancels and restores normal main-click recording.
ccFx(3); ccFx(14, 1); ccFx(50);
assert.equal(fx.state().fxRouteGrabbed, false);
assert.equal(fx.state().fxRoutePending, false);
ccFx(3);
assert.equal(recordingCount(), beforeRecordWrites + 1);
// Cutups uses the shared full-height lines/hollow markers for actual readers, including all eight voices
// and outgoing/released readers. Missing/stale data cannot revive a scanner.
const cutState = fx.state();
cutState.algorithm = 3; cutState.mask = 15; cutState.regions = [];
cutState.fieldStart=0;cutState.fieldEnd=1;
cutState.waves = Array.from({ length: 4 }, () => Array.from({ length: 112 }, () => [0,0]));
globalThis.__testParseCutups(Array.from({ length: 8 }, (_, slot) =>
    `${slot},${slot % 4},${100 + slot * 100},-1,-1000,800,0`).join(';'));
let cutDrawStart = drawWrites.length; globalThis.__testDrawMain();
let cutRects = drawWrites.slice(cutDrawStart).filter(([kind]) => kind === 'rect');
assert.equal(cutRects.filter((write) => write[3] === 1 && write[4] === 40).length, 8);
assert.equal(cutRects.filter((write) => write[3] === 5 && write[4] === 5 && write[5] === 0).length, 8);
globalThis.__testParseCutups('0,2,500,0,250,800,200');
cutDrawStart = drawWrites.length; globalThis.__testDrawMain();
cutRects = drawWrites.slice(cutDrawStart).filter(([kind]) => kind === 'rect');
assert(cutRects.some((w) => w[1] === 71 && w[2] === 10 && w[3] === 1 && w[4] === 40));
assert(cutRects.some((w) => w[1] === 42 && w[2] === 10 && w[3] === 1 && w[4] === 2));
assert(cutRects.some((w) => w[1] === 69 && w[2] === 33 && w[3] === 5 && w[4] === 1));
// Parsing an empty getter clears all stale lines; malformed records are ignored.
globalThis.__testParseCutups('');
assert(cutState.cutupsVoices.every((voice) => voice === null));
globalThis.__testParseCutups('9,0,500,-1,-1000,800,0;0,nope,500,-1,-1000,800,0');
assert(cutState.cutupsVoices.every((voice) => voice === null));
cutDrawStart = drawWrites.length; globalThis.__testDrawMain();
assert(!drawWrites.slice(cutDrawStart).some((w) => w[0] === 'rect' && w[3] === 1 && w[4] === 40));
// The same physical knobs now edit physical Lane and cut-count Advance.
cutState.view = 'main'; cutState.shiftHeld = false; cutState.clipRecording = true;
globalThis.__testParams.manual_lane.value = 2;
globalThis.__testParams.cut_lane_advance.value = 1;
globalThis.__testResetEncoders();
globalThis.onMidiMessageInternal([0xB0,74,2]);
assert(paramWrites.some(([key,value]) => key === 'manual_lane' && value === '3'));
assert(paramWrites.some(([key,value]) => key === 'clip_automation' && value === 'manual_lane'));
globalThis.onMidiMessageInternal([0xB0,75,2]);
assert(paramWrites.some(([key,value]) => key === 'cut_lane_advance' && value === '2'));
assert(paramWrites.some(([key,value]) => key === 'clip_automation' && value === 'cut_lane_advance'));
assert(globalThis.__testLockableParams.includes('cut_lane_advance'));
assert.equal(globalThis.__testFormatControlValue(globalThis.__testParams.cut_lane_advance), '2 CUTS');
cutState.view = 'settings'; cutState.settingsPage = 0;
cutDrawStart = drawWrites.length; fx.draw();
const cutLabels = drawWrites.slice(cutDrawStart).filter(([kind]) => kind === 'print').map((w) => w[3]);
assert(cutLabels.includes('LANE') && cutLabels.includes('ADV'));
assert(!cutLabels.includes('STAK') && !cutLabels.includes('RATE'));
// Split controls use the actual dispatcher with an emulated two-part DSP scope.
const split = globalThis.__testSplit;
const splitState = {enabled:0,focus:0,blend:.5,parts:[
    {algorithm:4,root:60,octave:0,scale:0,mode:0,hold:0,mute:0,position:.2},
    {algorithm:2,root:67,octave:1,scale:1,mode:0,hold:0,mute:0,position:.7}]};
params.split_blend='0.5'; params.split_mode='0';
const priorSet=globalThis.host_module_set_param_blocking;
globalThis.host_module_set_param_blocking=(key,value)=>{
    priorSet(key,value);
    if(key==='split_mode')splitState.enabled=Number(value);
    if(key==='split_focus')splitState.focus=Number(value);
    if(key==='record_lane')splitState.focus=Number(value)>2?1:0;
    if(key==='part_mute')splitState.parts[splitState.focus].mute=Number(value);
    const p=splitState.parts[splitState.focus];
    Object.assign(params,{split_state:JSON.stringify(splitState),algorithm:String(p.algorithm),
        root_note:String(p.root),scale:String(p.scale),pad_mode:String(p.mode),pad_octave:String(p.octave),part_mute:String(p.mute)});
};
params.split_state=JSON.stringify(splitState);
params.voice_algorithms='4,-1,-1,-1,2,-1,-1,-1';
globalThis.init();
const cc=(key,value=127)=>globalThis.onMidiMessageInternal([0xb0,key,value]);
cc(60); assert(fx.state().splitEnabled);assert.equal(fx.state().splitFocus,0);
assert.equal(split.keys().at(-1),'split_blend');
assert.equal(globalThis.__testPadMidiNote(0),48);
assert.equal(globalThis.__testPadMidiNote(4),67);
const keys=split.pages().flatMap((p)=>p.keys||[]).filter(Boolean);
assert.equal(new Set(keys).size,keys.length,'Split pages must not duplicate parameters');
assert(split.pages().some((p)=>p.title==='PART COLOR'));
assert(!split.pages().some((p)=>p.title==='EFFECTS'));
fx.state().settingsPage=2;
const focusReadsBefore=paramReads.length;
cc(52);cc(52,0);assert.equal(fx.state().splitFocus,1);
const focusReads=paramReads.slice(focusReadsBefore);
assert(focusReads.length <= 3,
    'A/B focus must use one editor snapshot, not read every control: '+focusReads.length+' reads');
assert.equal(focusReads.filter((key)=>key==='ui_editor_state').length,1);
fx.state().settingsPage=3;
cc(52);cc(52,0);assert.equal(fx.state().splitFocus,0);assert.equal(fx.state().settingsPage,2);
fx.state().view='settings';globalThis.__testResetEncoders();
const beforeBlend=paramWrites.length;cc(52);cc(78,4);cc(52,0);
assert.equal(fx.state().splitFocus,0,'Blend chord must not switch editing focus');
assert(paramWrites.slice(beforeBlend).some(([key])=>key==='split_blend'));
cc(49);cc(52);cc(52,0);cc(49,0);
assert.equal(split.pages()[fx.state().settingsPage].title,'FX SENDS');
const startCopy=blockingWrites.length;cc(49);cc(60);cc(49,0);
assert(blockingWrites.slice(startCopy).some(([key])=>key==='split_copy'));
assert(fx.state().splitEnabled);
const focus=fx.state().splitFocus;
globalThis.onMidiMessageInternal([0x90,72,100]);
assert.equal(fx.state().splitFocus,focus,'Playing the other instrument must not change editing focus');
cc(88);assert.equal(fx.state().splitParts[0].mute,1);
assert.equal(globalThis.__testParams.part_mute.value,1,
    'hardware Mute must update the Split Setup control without changing focus');
fx.state().messageTicks=0;drawWrites.length=0;globalThis.__testDrawMain();
assert([...screenText.values()].some((w)=>String(w[2]).includes('A:MUT')),
    'a muted instrument must stay identifiable on the waveform screen');
// B can be muted/unmuted independently, including after changing focus.
cc(52);cc(52,0);assert.equal(fx.state().splitFocus,1);
cc(88);assert.equal(splitState.parts[1].mute,1);
assert.equal(globalThis.__testParams.part_mute.value,1);
cc(52);cc(52,0);cc(88);assert.equal(splitState.parts[0].mute,0);
cc(52);cc(52,0);assert.equal(globalThis.__testParams.part_mute.value,1);
fx.state().messageTicks=0;drawWrites.length=0;globalThis.__testDrawMain();
assert([...screenText.values()].some((w)=>String(w[2]).includes('B:MUT')));
assert([...screenText.values()].some((w)=>w[2]==='B MUTED: PRESS MUTE'));
// The Setup encoder must recover B from the actual ON value.
fx.state().view='settings';fx.state().settingsPage=split.pages().findIndex((p)=>p.title==='SPLIT SETUP');
globalThis.__testResetEncoders();cc(74,124);
assert.equal(splitState.parts[1].mute,0);assert.equal(globalThis.__testParams.part_mute.value,0);
// A recalled mute can arrive between polls: the button must toggle the DSP state.
splitState.parts[1].mute=1;params.split_state=JSON.stringify(splitState);
cc(88);assert.equal(splitState.parts[1].mute,0);
assert.equal(globalThis.__testParams.part_mute.value,0);
cc(52);cc(52,0);
fx.state().settingsPage=fx.pageIndex();params.effect_order='7';params.effect_order_state='7,7,0';fx.sync();
assert.equal(fx.state().fxOrder.length,3,'Only shared returns and Tape are reorderable in Split');
assert.deepEqual(new Set(fx.state().fxOrder),new Set([2,4,5]));
cc(60);assert(!fx.state().splitEnabled);
// Split cursors stay in their owner's pair even when the other part is focused.
const visual = fx.state();
visual.splitEnabled = true; visual.mask = 15; visual.regions = [];visual.laneRegions=Array(4).fill(null);
visual.waves = Array.from({length:4},()=>Array.from({length:112},()=>[0,0]));
visual.playheads = Array.from({length:8},(_,slot)=>(slot+1)/10);
visual.stackPositions = Array.from({length:8},(_,slot)=>
    ((slot<4?0:2)+.2+(slot%4)*.2)/3);
const drawCursorRects = () => {
    const start=drawWrites.length;globalThis.__testDrawMain();
    return drawWrites.slice(start).filter(([kind])=>kind==='rect');
};
for (let engine=0;engine<9;++engine) {
    visual.voiceAlgorithms=Array(8).fill(engine);
    visual.cutupsVoices=Array.from({length:8},(_,slot)=>({
        lane:slot<4?0:2,position:visual.playheads[slot],gain:1,
        outgoingLane:-1,outgoingPosition:-1,outgoingGain:0}));
    visual.wavesetVoices=Array.from({length:8},(_,slot)=>({
        sourcePosition:visual.playheads[slot],secondarySourcePosition:-1}));
    for(const focus of [0,1]) {
        visual.splitFocus=focus;
        const rects=drawCursorRects();
        for(let slot=0;slot<8;++slot) {
            const x=14+Math.round(visual.playheads[slot]*113),top=slot<4?10:30;
            assert.equal(screenPixel(x,top),1,'all eight sounding Split voices must remain visible');
            assert.equal(screenPixel(x,top+19),1);
            assert.equal(screenPixel(x,slot<4?30:10),0,'a cursor must stay in its owning pair');
        }
        assert(!rects.some((w)=>w[3]===1&&w[4]===40),'Split must not draw a four-lane cursor');
    }
}
// The outgoing Cutups reader and secondary Wavesets reader obey the same limits.
for(const engine of [3,8])for(const slot of [0,3,4,7]) {
    visual.voiceAlgorithms=Array(8).fill(-1);visual.voiceAlgorithms[slot]=engine;
    visual.playheads=Array(8).fill(-1);visual.stackPositions=Array(8).fill(-1);
    visual.stackPositions[slot]=(slot<4?.2:2.8)/3;
    visual.cutupsVoices=Array(8).fill(null);visual.wavesetVoices=Array(8).fill(null);
    visual.cutupsVoices[slot]={lane:slot<4?0:3,position:.2,gain:1,
        outgoingLane:slot<4?1:2,outgoingPosition:.7,outgoingGain:.5};
    visual.wavesetVoices[slot]={sourcePosition:.2,secondarySourcePosition:.7};
    const rects=drawCursorRects();const x=14+Math.round(.7*113);
    const top=slot<4?10:30;
    for(let offset=0;offset<20;++offset) {
        const y=top+offset,markerY=15+visual.cutupsVoices[slot].outgoingLane*10;
        if(engine===3&&Math.abs(y-markerY)<=2)continue;
        if(y%10===5)continue; // the source's zero line
        assert.equal(screenPixel(x,y),offset%4<2?1:0,'outgoing readers retain their dash pattern');
    }
    assert.equal(screenPixel(x,slot<4?30:10),0,'transition cursor stays in its owning pair');
}
// Focus is a gutter-only change; source pixels and the A/B heading stay put.
visual.regions=[];visual.messageTicks=0;visual.splitFocus=0;visual.lane=1;
visual.laneRegions=[{source:visual.waveKeys[0],regions:[0,.25,.75,1]},null,null,null];
visual.splitParts=[{algorithm:4,window:[0,1]},{algorithm:4,window:[0,1]}];
visual.voiceAlgorithms=Array(8).fill(-1);visual.voiceAlgorithms[0]=visual.voiceAlgorithms[4]=4;
visual.playheads=Array(8).fill(-1);visual.playheads[0]=.3;visual.playheads[4]=.7;
visual.stackPositions=Array(8).fill(-1);visual.stackPositions[0]=.2/3;visual.stackPositions[4]=2.8/3;
globalThis.__testClearDisplay();globalThis.__testDrawMain();
const wavePixels=()=>Uint8Array.from({length:116*40},(_,i)=>screenPixel(12+i%116,10+Math.floor(i/116)));
const beforeWaveFocus=wavePixels(),focusHeader=screenText.get('1,0')[2];
let waveWrites=drawWrites.length;visual.splitFocus=1;visual.lane=3;globalThis.__testDrawMain();
assert.deepEqual(wavePixels(),beforeWaveFocus);
assert.equal(screenText.get('1,0')[2],focusHeader,'the A/B heading does not shift with focus');
assert(!drawWrites.slice(waveWrites).some((w)=>w[0]==='clear'));
assert(!drawWrites.slice(waveWrites).some((w)=>w[0]==='rect'&&w[1]+w[3]>12&&w[2]<50&&w[2]+w[4]>10),
    'a focus change must not clear or repaint the waveform canvas');
for(let lane=0;lane<4;++lane)assert.equal(screenText.get('3,'+(11+lane*10))[3],lane<2?1:0,
    'focused lane numbers are black on a white background');
assert.equal(screenPixel(2,31),1);assert.equal(screenPixel(2,41),1);
assert.equal(screenPixel(2,11),0);assert.equal(screenPixel(2,21),0);
waveWrites=drawWrites.length;globalThis.__testDrawMain();
assert.equal(drawWrites.length,waveWrites,'an unchanged Split frame emits no display writes');
// An advancing reader restores its old pixels without blanking the waveform.
const oldHead=14+Math.round(.7*113);visual.playheads[4]=.8;
globalThis.__testDrawMain();assert.equal(screenPixel(oldHead,30),0);
assert.equal(screenPixel(14+Math.round(.8*113),30),1);
const checkRetainedFrame=()=>{
    const retained=screenBits.slice();globalThis.__testClearDisplay();globalThis.__testDrawMain();
    assert.deepEqual(screenBits,retained,'incremental pixels must equal a complete fresh frame');
};
checkRetainedFrame();
// Separate A/B zoom windows stretch only their own source field across the view.
visual.splitParts[0].window=[.2,.4];visual.splitParts[1].window=[.55,.95];
visual.laneRegions=Array(4).fill(null);
visual.waves=Array.from({length:4},(_,lane)=>Array.from({length:112},(_,bin)=>{
    const position=bin/111,amplitude=lane<2?(position>=.18&&position<=.45?2:7):(position>=.5?4:0);
    return [-amplitude,amplitude];
}));
visual.playheads[0]=.3;visual.playheads[4]=.75;
waveWrites=drawWrites.length;globalThis.__testDrawMain();
assert.equal(screenPixel(71,10),1);assert.equal(screenPixel(71,30),1,
    'different absolute source positions share the midpoint of their own zoom windows');
assert.equal(screenPixel(14,11),0,'unused loud source material before A’s window is hidden');
assert.equal(screenPixel(14,14),1);assert.equal(screenPixel(14,33),1,
    'B uses its own window instead of A’s field');
assert(!drawWrites.slice(waveWrites).some((w)=>w[0]==='clear'));
checkRetainedFrame();
visual.playheads[0]=.9;globalThis.__testDrawMain();
assert.equal(screenPixel(127,10),0,'an out-of-window cursor must not be pinned to a false edge position');
checkRetainedFrame();
// Old region markers cannot be drawn into a newly selected source lane.
visual.regions=[0,.3,1];visual.regionLane=1;visual.lane=3;
globalThis.__testDrawMain();checkRetainedFrame();
visual.waves=Array.from({length:4},()=>Array.from({length:112},()=>[0,0]));
globalThis.__testDrawMain();checkRetainedFrame();

// The regular instrument uses the same source crop for all four lanes and
// preserves eight full-height playheads across every playback algorithm.
visual.splitEnabled=false;visual.mask=15;visual.fieldStart=.2;visual.fieldEnd=.4;
visual.regions=[];visual.laneRegions=Array(4).fill(null);visual.messageTicks=0;
visual.waves=Array.from({length:4},()=>Array.from({length:112},(_,bin)=>{
    const amplitude=bin/111>=.18&&bin/111<=.45?2:7;return [-amplitude,amplitude];
}));
for(let algorithm=0;algorithm<9;++algorithm) {
    visual.algorithm=algorithm;
    visual.playheads=Array.from({length:8},(_,slot)=>.21+slot*.02);
    visual.stackPositions=Array.from({length:8},(_,slot)=>(slot%4)/3);
    visual.cutupsVoices=visual.playheads.map((position,slot)=>({lane:slot%4,position,gain:1,
        outgoingLane:-1,outgoingPosition:-1,outgoingGain:0}));
    visual.wavesetVoices=visual.playheads.map((sourcePosition,slot)=>({sourcePosition,
        secondarySourcePosition:-1,groupSize:8,groupPosition:.3,secondaryGroupPosition:-1,focused:slot===0}));
    globalThis.__testDrawMain();
    for(const position of visual.playheads) {
        const x=14+Math.round((position-.2)/.2*113+1e-9);
        assert.equal(screenPixel(x,10),1,'Single playhead must use the zoomed source coordinate');
        assert.equal(screenPixel(x,49),1,'Single retains full four-lane playhead height');
    }
    assert.equal(screenPixel(14,11),0,'the unused loud source before the field must be cropped out');
    assert.equal(screenPixel(14,14),1);
}
for(const algorithm of [3,8]) {
    visual.algorithm=algorithm;visual.playheads=Array(8).fill(-1);
    visual.stackPositions=Array(8).fill(-1);visual.stackPositions[0]=0;
    visual.cutupsVoices=Array(8).fill(null);visual.wavesetVoices=Array(8).fill(null);
    visual.cutupsVoices[0]={lane:0,position:.25,gain:1,outgoingLane:2,outgoingPosition:.35,outgoingGain:.5};
    visual.wavesetVoices[0]={sourcePosition:.25,secondarySourcePosition:.35,
        groupSize:8,groupPosition:.3,secondaryGroupPosition:-1,focused:true};
    globalThis.__testDrawMain();
    assert.equal(screenPixel(99,10),1);assert.equal(screenPixel(99,11),1);
    assert.equal(screenPixel(99,12),0,'outgoing readers keep their dash pattern after zooming');
}
// A flat source must have an uninterrupted baseline in both display modes.
// The old 112-bin-to-114-pixel mapping left black seams at x70 and x126.
visual.algorithm=4;visual.playheads=Array(8).fill(-1);visual.stackPositions=Array(8).fill(-1);
visual.voiceAlgorithms=Array(8).fill(-1);visual.cutupsVoices=Array(8).fill(null);visual.wavesetVoices=Array(8).fill(null);
visual.waves=Array.from({length:4},()=>Array.from({length:112},()=>[0,0]));
visual.splitParts=[{algorithm:4,window:[0,1]},{algorithm:4,window:[0,1]}];
visual.fieldStart=0;visual.fieldEnd=1;
for(const mode of [false,true]) {
    visual.splitEnabled=mode;globalThis.__testClearDisplay();globalThis.__testDrawMain();
    for(let lane=0;lane<4;++lane)for(let x=14;x<128;++x)
        assert.equal(screenPixel(x,15+lane*10),1,'every waveform column must be painted');
}
visual.splitEnabled=false;
globalThis.__testParseUi('1,15,0,0,0,315,339,0,0,0,0,0,45,0,1,0,1,0.3154650,0.3388350');
assert.equal(visual.fieldStart,.315465);assert.equal(visual.fieldEnd,.338835);
visual.playheads[0]=.32715;visual.stackPositions[0]=0;
globalThis.__testDrawMain();assert.equal(screenPixel(71,10),1,
    'narrow fields must use precise bounds instead of rounded per-mille values');
visual.playheads[0]=.9;globalThis.__testDrawMain();
assert.equal(screenPixel(127,10),0,'a reader outside the visible window is not falsely pinned to an edge');
visual.mask=0;visual.lane=1;visual.regionLane=1;visual.regions=[0,.32715,1];
const emptyLaneDraw=drawWrites.length;globalThis.__testDrawMain();
assert(!drawWrites.slice(emptyLaneDraw).some((w)=>w[0]==='rect'&&w[1]===71&&w[2]===11&&w[4]===2),
    'a cleared source lane cannot retain old transient marks');


visual.splitEnabled=false;
// B's background tint must be visible at startup and yield to performance states.
const emptySlots=Array(8).fill(-1).join(',');
const colors={enabled:1,focus:0,blend:.5,parts:[
    {algorithm:4,root:60,octave:0,scale:1,mode:0,hold:0,mute:0},
    {algorithm:4,root:67,octave:1,scale:1,mode:0,hold:0,mute:0}]};
Object.assign(params,{split_state:JSON.stringify(colors),algorithm:'4',root_note:'60',
    scale:'1',pad_mode:'0',pad_octave:'0',voice_pads:emptySlots,voice_notes:emptySlots,
    roll_pads:emptySlots,roll_notes:emptySlots,cue_state:emptySlots});
const lastPad=(index)=>padWrites.filter(([note])=>note===68+index).at(-1)?.[1];
globalThis.init();
assert.equal(lastPad(0),120);assert.equal(lastPad(1),123);
assert.equal(lastPad(4),15,'B tonic uses the brighter teal');
assert.equal(lastPad(5),93,'B idle uses dim teal, not a voice/status color');
// Focus does not recolor either instrument's grid.
const beforeFocus=Array.from({length:32},(_,index)=>lastPad(index));
colors.focus=1;params.split_state=JSON.stringify(colors);
for(let i=0;i<24;++i)globalThis.tick();
assert.deepEqual(Array.from({length:32},(_,index)=>lastPad(index)),beforeFocus);
// The named scale can be changed on the nonfocused side without needing a note press.
colors.focus=0;colors.parts[1].scale=0;params.split_state=JSON.stringify(colors);
for(let i=0;i<24;++i)globalThis.tick();
assert.equal(lastPad(4),93,'Chromatic B remains tinted');
assert.equal(lastPad(1),123,'A keeps its scale colors');
colors.parts[1].scale=1;params.split_state=JSON.stringify(colors);
for(let i=0;i<24;++i)globalThis.tick();
assert.equal(lastPad(4),15,'B scale roots refresh without changing focus');
// Active voice and out-of-octave latch colors override the B background.
const bPitch=globalThis.__testPadMidiNote(4);
params.voice_pads='-1,-1,-1,-1,4,-1,-1,-1';
params.voice_notes=`-1,-1,-1,-1,${bPitch},-1,-1,-1`;
for(let i=0;i<24;++i)globalThis.tick();assert.equal(lastPad(4),16);
colors.parts[1].octave=2;params.split_state=JSON.stringify(colors);
for(let i=0;i<24;++i)globalThis.tick();assert.equal(lastPad(4),95);
colors.parts[1].octave=1;params.split_state=JSON.stringify(colors);
for(let i=0;i<24;++i)globalThis.tick();assert.equal(lastPad(4),16);
params.voice_pads=emptySlots;params.voice_notes=emptySlots;
for(let i=0;i<24;++i)globalThis.tick();assert.equal(lastPad(4),15);
// The B tint remains an idle indicator in Slices/Rolls; capture colors keep priority.
for(const mode of [1,3]) {
    colors.parts[1].mode=mode;params.split_state=JSON.stringify(colors);
    for(let i=0;i<24;++i)globalThis.tick();assert.equal(lastPad(5),93);
}
params.roll_pads='-1,-1,-1,-1,5,-1,-1,-1';
params.roll_notes=`-1,-1,-1,-1,${globalThis.__testPadMidiNote(5)},-1,-1,-1`;
for(let i=0;i<24;++i)globalThis.tick();assert.equal(lastPad(5),26);
params.roll_pads=emptySlots;params.roll_notes=emptySlots;
colors.parts[1].mode=2;params.split_state=JSON.stringify(colors);
params.cue_state='-1,-1,-1,-1,500,-1,-1,-1';
for(let i=0;i<24;++i)globalThis.tick();
assert.equal(lastPad(4),3,'assigned B cues retain their orange status');
assert.equal(lastPad(5),93,'empty B cue pads retain the B tint');
assert.equal(lastPad(12),0,'unused B cue rows remain dark');
params.cue_state=emptySlots;
for(let i=0;i<24;++i)globalThis.tick();
assert.equal(lastPad(4),93,'cleared B cues immediately return to their idle tint');
// Re-entry must repaint even when the shared LED helper remembers old colors.
emulateSharedLedCache=true;
globalThis.init();
const invalidationsBefore=invalidations;
const writesBeforeReentry=padWrites.length;
globalThis.init();
assert.equal(invalidations,invalidationsBefore+1);
assert.equal(padWrites.length-writesBeforeReentry,32);
assert(forcedPadWrites.slice(-32).every(([, , force])=>force),
    'startup must bypass the shared LED cache for every pad');
assert(drawWrites.slice(-500).some((w)=>w[0]==='print'&&w[3]==='THRU '+testVersion),
    'startup should identify the installed test build');
// A mode toggle also sends all 32 pad states immediately, without waiting for the queue.
Object.assign(splitState,colors);
const beforeModeRepaint=forcedPadWrites.length;
cc(60);
assert.equal(forcedPadWrites.length-beforeModeRepaint,32);
assert(forcedPadWrites.slice(-32).every(([, , force])=>force));
cc(60);
assert(forcedPadWrites.slice(-32).every(([, , force])=>force));
assert.equal(lastPad(5),93);
// Older shared helpers may have force support without cache invalidation.
const savedInvalidation=globalThis.__moveInputFilter.invalidateLedCache;
globalThis.__moveInputFilter.invalidateLedCache=undefined;
const beforeLegacyReentry=padWrites.length;
globalThis.init();
assert.equal(padWrites.length-beforeLegacyReentry,32,
    'force repaint must also work without invalidateLedCache');
globalThis.__moveInputFilter.invalidateLedCache=savedInvalidation;
// The sends and both wet return levels are on a single, directly reachable page.
Object.assign(splitState,{enabled:1,focus:0,parts:[
    {algorithm:4,root:60,octave:0,scale:0,mode:0,hold:0,mute:0},
    {algorithm:4,root:60,octave:0,scale:0,mode:0,hold:0,mute:0}]});
Object.assign(params,{split_state:JSON.stringify(splitState),effect_mix:'0',effect_reverb_mix:'0',
    split_delay_a:'0.2',split_delay_b:'0.25',split_reverb_a:'0.3',split_reverb_b:'0.4',
    split_level_a:'1',split_level_b:'0.9',split_curve:'1',split_blend:'0.5',algorithm:'4'});
globalThis.init();cc(49);cc(52);cc(52,0);cc(49,0);
const sendsKeys=['split_delay_a','split_reverb_a','split_delay_b','split_reverb_b',
    'effect_mix','effect_reverb_mix','split_curve','split_blend'];
assert.deepEqual(split.keys(),sendsKeys);
let fxDrawStart=drawWrites.length;globalThis.tick();
let fxText=drawWrites.slice(fxDrawStart).filter(([kind])=>kind==='print').map((w)=>String(w[3]));
assert(fxText.some((text)=>text.includes('FX SENDS ')));
assert(fxText.includes('DRTN')&&fxText.includes('RRTN')&&fxText.includes('25%'));
assert(fxText.includes('RETURNS OFF: KNOBS 5/6'));
globalThis.__testResetEncoders();cc(71,2);
assert.equal(fx.state().actionMessage,'RAISE DRTN: KNOB 5');
cc(72,2);assert.equal(fx.state().actionMessage,'RAISE RRTN: KNOB 6');
for(let i=0;i<4;++i){cc(75,63);cc(76,63);}
assert.equal(Number(params.effect_mix),100);assert.equal(Number(params.effect_reverb_mix),100);
fx.state().actionMessageTicks=0;
fxDrawStart=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(fxDrawStart).some((w)=>w[0]==='print'&&w[3]==='CLICK: FX PAGES'));
// A click opens a readable list instead of accidentally starting a take.
let menuWrites=paramWrites.length;cc(3);
assert.equal(fx.state().view,'fxMenu');
assert(!paramWrites.slice(menuWrites).some(([key])=>key==='clip_record'||key==='record_gate'));
const laneWrites=blockingWrites.filter(([key])=>key==='record_lane').length;
cc(14,1);cc(3);
assert.equal(split.pages()[fx.state().settingsPage].title,'DELAY RETURN');
assert(!split.keys().includes('effect_mix'),'Delay return level must not be duplicated');
assert.equal(blockingWrites.filter(([key])=>key==='record_lane').length,laneWrites);
cc(51);assert.equal(fx.state().view,'fxMenu');cc(14,1);cc(3);
assert.equal(split.pages()[fx.state().settingsPage].title,'REVERB');
assert(!split.keys().includes('effect_reverb_mix'),'Reverb return level must not be duplicated');
cc(51);cc(52);cc(52,0);assert.equal(fx.state().splitFocus,1);
cc(14,1);cc(3);assert.equal(split.pages()[fx.state().settingsPage].title,'PART COLOR');
fxDrawStart=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(fxDrawStart).some((w)=>w[0]==='print'&&String(w[3]).includes('B PART COLOR')));
cc(51);for(let i=0;i<3;++i)cc(14,1);
fxDrawStart=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(fxDrawStart).some((w)=>w[0]==='print'&&String(w[3]).includes('EFFECT ORDER')));
cc(3);assert(split.pages()[fx.state().settingsPage].route);
cc(3);cc(14,1);cc(51);assert.equal(fx.state().view,'settings','Back first cancels a pending route edit');
cc(51);assert.equal(fx.state().view,'fxMenu');cc(51);
assert.equal(split.pages()[fx.state().settingsPage].title,'FX SENDS');
// A part's level now lives on its own Engine page, with no repeated controls.
fx.state().settingsPage=0;assert.equal(split.keys()[7],'split_level_b');
fxDrawStart=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(fxDrawStart).some((w)=>w[0]==='print'&&w[3]==='LEVL'));
for(let engine=0;engine<9;++engine)for(let focus=0;focus<2;++focus) {
    fx.state().algorithm=engine;fx.state().splitFocus=focus;
    const controls=split.pages().flatMap((page)=>page.keys||[]).filter(Boolean);
    assert.equal(new Set(controls).size,controls.length);
}
// Fresh backend values and an immediate redraw on each focus change.
const focusSetter=globalThis.host_module_set_param_blocking;
globalThis.host_module_set_param_blocking=(key,value)=>{
    focusSetter(key,value);
    if(key==='split_focus')Object.assign(params,Number(value)
        ?{locus:'0.83',effect_drive:'0.42',amp_release:'1234'}
        :{locus:'0.17',effect_drive:'0.12',amp_release:'250'});
};
fx.state().view='main';
if(fx.state().splitFocus!==0){cc(52);cc(52,0);}
let focusDraws=drawWrites.length,focusCalls=paramReads.length;
cc(52);cc(52,0);
assert.equal(paramReads.length-focusCalls,1,'focus uses exactly one snapshot read: '+paramReads.slice(focusCalls).join(','));
assert.equal(globalThis.__testParams.locus.value,.83);
assert.equal(globalThis.__testParams.effect_drive.value,.42);
assert.equal(globalThis.__testParams.amp_release.value,1234);
assert(drawWrites.length>focusDraws,'focus must redraw without waiting for the next tick');
const immediate=paramWrites.length;globalThis.__testResetEncoders();cc(71,2);
assert(paramWrites.slice(immediate).some(([key,value])=>key==='locus'&&Number(value)>.83),
    'the first turn after switching must start from B’s value: '+JSON.stringify(paramWrites.slice(immediate)));
// A malformed snapshot must not expose stale A/B knob values for editing.
corruptEditorSnapshot=true;cc(52);cc(52,0);
assert.equal(fx.state().editorReady,false);
const staleWrites=paramWrites.length;cc(71,4);
assert.equal(paramWrites.length,staleWrites,'do not send a stale parameter while the snapshot is incomplete');
corruptEditorSnapshot=false;
for(let i=0;i<3;++i)globalThis.tick();
assert.equal(fx.state().editorReady,true);
assert.equal(globalThis.__testParams.locus.value,.17);
// A snapshot refresh is bounded even when the DSP changed focus between polls.
focusCalls=paramReads.length;
splitState.focus=1;params.split_state=JSON.stringify(splitState);
for(let i=0;i<3;++i)globalThis.tick();
assert(paramReads.slice(focusCalls).filter((key)=>key==='ui_editor_state').length<=1);
// Leaving Split while the menu is open must not leave a stale Split-only view.
fx.state().view='fxMenu';cc(60);assert.equal(fx.state().view,'main');
console.log('s3g Thru overtake UI tests: PASS');
