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
let rejectSafetyUndo = false;
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
        if(!split.enabled||!split.quad)split.parts=split.parts.slice(0,2);
        values.split_mode = split.enabled ? (split.quad ? 3 : split.shared ? 2 : 1) : 0; values.split_focus = split.focus;
        values.part_mute = split.parts[split.focus]?.mute || 0;
        for (const key of ['pad_octave','record_lane','stack_scan_paused','glitch_buffer_lock'])
            values[key]=Number(params[key] || 0);
        return JSON.stringify({v:1,values,split,voices:params.voice_algorithms || '-1,-1,-1,-1,-1,-1,-1,-1'});
    }
    if (key === 'waveforms') waveformReads++;
    if(key==='ui_state') {
        const split=JSON.parse(params.split_state||'{}');
        if(split.enabled&&split.shared) {
            const values=params.ui_state.split(','),width=Math.max(.02,Math.min(1,Number(params.field))),half=width*.5;
            const center=Math.max(half,Math.min(1-half,Number(params.locus)));
            values[5]=String(Math.round((center-half)*1000));values[6]=String(Math.round((center+half)*1000));
            values[17]=String(center-half);values[18]=String(center+half);return values.join(',');
        }
    }
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
    if (key === 'undo' && params.undo_available === '1') {
        if (rejectSafetyUndo) params.last_error='Wait for the recording archive before undoing';
        else {params.undo_available='0';params.locus='0.23';}
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
globalThis.__testRefreshEditor=refreshEditorSnapshot;
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
globalThis.__testTools={entries:pageEntries,parseSave:parseSaveStatus,parseMix:parseMixStatus,drawMix:drawMixStatus};
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
for(const [algorithm,title] of [[9,'PRISM'],[10,'MORPH'],[11,'TEXTURE'],[12,'FORMANT']])assert.deepEqual(globalThis.__testPlaybackPageTitles[algorithm],['STACK PATH',title]);
for(const key of ['prism_mode','prism_blur','prism_decay','prism_pitch','prism_tilt','prism_smear','morph_amplitude','morph_frequency','morph_offset','morph_blur','morph_pitch','texture_mode','texture_rate','texture_length','texture_scatter','texture_range','texture_group','texture_spread','texture_seed','formant_pitch','formant_shift','formant_smoothing'])assert(globalThis.__testLockableParams.includes(key));
{
    const state=globalThis.__testFx.state(),prior=state.algorithm;state.algorithm=9;
    globalThis.__testParams.prism_mode.value=0;assert.equal(globalThis.__testSplit.pages().find(p=>p.title==='PRISM').keys[2],null);
    globalThis.__testParams.prism_mode.value=4;assert.equal(globalThis.__testSplit.pages().find(p=>p.title==='PRISM').keys[2],'prism_decay');
    globalThis.__testParams.prism_mode.value=0;state.algorithm=11;
    assert.equal(globalThis.__testSplit.pages().find(p=>p.title==='TEXTURE').keys[5],null);
    globalThis.__testParams.texture_mode.value=2;assert.equal(globalThis.__testSplit.pages().find(p=>p.title==='TEXTURE').keys[5],'texture_group');
    globalThis.__testParams.texture_mode.value=0;state.algorithm=prior;
}
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
    ['pad_mode', 'root_note', 'scale', 'hold_release_ms', 'polyphony_comp','pressure_mode']);
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
    'long_grain_mode', 'waveset_direction','prism_mode','morph_amplitude','texture_mode','formant_shift'];
for (let algorithm = 0; algorithm < 13; algorithm++) {
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
/* Parameter encoders accumulate 3/8 response: eight raw increments produce
   three complete parameter steps, and automation records only actual changes. */
globalThis.__testResetEncoders();
const fineStart=paramWrites.length;
globalThis.onMidiMessageInternal([0xB0,71,1]);assert.equal(paramWrites.length,fineStart);
for(let turn=1;turn<8;turn++)globalThis.onMidiMessageInternal([0xB0,71,1]);
assert.equal(params.locus,'0.5300');
assert.equal(paramWrites.slice(fineStart).filter(w=>w[0]==='clip_automation'&&w[1]==='locus').length,3);
for(let turn=0;turn<8;turn++)globalThis.onMidiMessageInternal([0xB0,71,127]);
assert.equal(params.locus,'0.5000');
assert.deepEqual(paramWrites.slice(-2),[['locus','0.5000'],['clip_automation','locus']]);
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
globalThis.onMidiMessageInternal([0xB0, 71, 2]);
assert.deepEqual(blockingWrites.pop(), ['algorithm', '1']);

/* Pads and Scale is shared by every engine. Named scales fold the grid to
   consecutive scale degrees with overlapping rows; Chromatic is unchanged. */
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
const scalePageDrawStart = drawWrites.length;
globalThis.tick();
const scaleHeader = drawWrites.slice(scalePageDrawStart).find((write) =>
    write[0] === 'print' && write[2] === 0);
assert(String(scaleHeader?.[3]).includes('PADS & SCALE 2/19'));
assert(drawWrites.slice(scalePageDrawStart).some((write) =>
    write[0] === 'print' && String(write[3]).includes('POLY')));
globalThis.onMidiMessageInternal([0xB0, 73, 2]);
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
    && String(write[3]).includes('AMP ENVELOPE 3/19')));
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
globalThis.onMidiMessageInternal([0xB0, 71, 2]);
assert.deepEqual(paramWrites.at(-1), ['amp_attack', '5']);

/* Only pages belonging to the chosen playback engine are exposed. */
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
const lanesPageDrawStart = drawWrites.length;
globalThis.tick();
const lanesHeader = drawWrites.slice(lanesPageDrawStart).find((write) =>
    write[0] === 'print' && write[2] === 0);
assert(String(lanesHeader?.[3]).includes('STACK PATH 4/19'));
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
    for (let page = 0; page < 11; page++) {
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
    [58, 127], 'Loop uses bright white while Glitch history is locked');
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
    && String(write[3]).includes('C01 OFF')));
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
assert(callbackRows.some((write) => String(write[3]).includes('CALLBACK WORK 2/4')));
assert(callbackRows.some((write) => String(write[3]).includes('12345 us')));
assert(callbackRows.some((write) => String(write[3]).includes('1.2M')));
assert(callbackRows.every((write) => String(write[3]).length <= 21));
params.render_timing_state='1,1800,1504,0';
globalThis.onMidiMessageInternal([0xB0, 63, 127]);
const timingDrawStart=drawWrites.length;
for(let n=0;n<65;++n)globalThis.tick();
const timingRows=drawWrites.slice(timingDrawStart).filter(w=>w[0]==='print');
assert(timingRows.some(w=>String(w[3]).includes('RENDER TIMING 3/4')));
assert(timingRows.some(w=>String(w[3]).includes('CPU PEAK')&&String(w[3]).includes('1800 us')));
assert(timingRows.some(w=>String(w[3]).includes('WAIT PEAK')&&String(w[3]).includes('1504 us')));
assert(timingRows.every(w=>String(w[3]).length<=21));
const resetWrites=blockingWrites.length;
globalThis.onMidiMessageInternal([0xB0,49,127]);globalThis.onMidiMessageInternal([0xB0,119,127]);
globalThis.onMidiMessageInternal([0xB0,119,0]);globalThis.onMidiMessageInternal([0xB0,49,0]);
assert(blockingWrites.slice(resetWrites).some(([key])=>key==='performance_reset'));
assert(!blockingWrites.slice(resetWrites).some(([key])=>key==='clear_lane'||key==='clip_clear'||key==='undo'));
params.render_timing_state='0,0,0,0';for(let n=0;n<65;++n)globalThis.tick();
assert([...screenText.values()].some(w=>String(w[2]).includes('CPU PEAK')&&String(w[2]).includes('N/A')));
globalThis.onMidiMessageInternal([0xB0, 63, 127]);
const shortcutDrawStart = drawWrites.length;
globalThis.tick();
assert(drawWrites.slice(shortcutDrawStart).some((write) =>
    write[0] === 'print' && String(write[3]).includes('SHORTCUTS 4/4')));
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
let rejectModeSwitch=false;
globalThis.host_module_set_param_blocking=(key,value)=>{
    priorSet(key,value);
    if(key==='split_mode'&&rejectModeSwitch){params.split_mode=String(splitState.enabled?(splitState.quad?3:splitState.shared?2:1):0);return;}
    if(key==='split_mode'){splitState.enabled=Number(value)!==0?1:0;if(splitState.enabled){splitState.shared=Number(value)===2?1:0;splitState.quad=Number(value)===3?1:0;}}
    if(key==='split_focus'){splitState.focus=Number(value);if(splitState.quad)params.record_lane=String(splitState.focus+1);}
    if(key==='record_lane'&&splitState.enabled&&!splitState.shared)splitState.focus=splitState.quad?Number(value)-1:Number(value)>2?1:0;
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
const skipbackPage=fx.state().settingsPage;
cc(49);cc(52);cc(52,0);cc(49,0);
assert.equal(fx.state().settingsPage,skipbackPage,'Shift+Capture remains reserved for Schwung');
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
fx.state().view='settings';fx.state().settingsPage=split.pages().findIndex((p)=>p.title==='DUAL SETUP');
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
for(let algorithm=0;algorithm<13;++algorithm) {
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
globalThis.init();cc(52);cc(50);cc(52,0);fx.state().pageSelection=globalThis.__testTools.entries().findIndex(e=>e.title==='FX SENDS');cc(3);cc(3,0);
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
for(let i=0;i<5;++i){cc(75,63);cc(76,63);}
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
// General navigation is contextual and never fires recording or a focus toggle.
const tools=globalThis.__testTools,ui=fx.state();
ui.view='settings';ui.settingsPage=2;ui.shiftHeld=false;ui.deleteHeld=false;
let toolWrites=blockingWrites.length;
cc(52);cc(50);cc(50,0);cc(52,0);
assert.equal(ui.view,'pages');
assert(!blockingWrites.slice(toolWrites).some(([k])=>['clip_record','record_gate','split_focus'].includes(k)));
assert(tools.entries().some(e=>e.title==='AMP ENVELOPE'));
assert(!tools.entries().some(e=>e.title.includes('A/B')),'Single hides Split status');
let destination=tools.entries().findIndex(e=>e.title==='REVERB');
ui.pageSelection=destination;cc(3);assert.equal(ui.view,'settings');
assert.equal(globalThis.__testSplit.pages()[ui.settingsPage].title,'REVERB');
cc(52);cc(50);cc(52,0);cc(51);assert.equal(ui.view,'settings');
assert.equal(globalThis.__testSplit.pages()[ui.settingsPage].title,'REVERB');
cc(60);assert(ui.splitEnabled);ui.view='main';
const pickerFocus=ui.splitFocus;
cc(52);cc(50);cc(52,0);assert.equal(ui.splitFocus,pickerFocus);
destination=tools.entries().findIndex(e=>e.view==='mixStatus');assert(destination>=0);
ui.pageSelection=destination;cc(3);assert.equal(ui.view,'mixStatus');
tools.parseMix('.12,0,2,3,0,3');tools.drawMix();
assert.equal(ui.mixVoices[0],2);assert.equal(ui.mixVoices[1],3);
assert([...screenText.values()].some(t=>t[2]==='FADED OUT'));
cc(50);assert.equal(ui.view,'main');
// Cancelling after changing focus returns to that part's remembered page.
if(ui.splitFocus!==0){cc(52);cc(52,0);}
ui.view='settings';ui.settingsPage=2;ui.splitPages=[2,5];
cc(52);cc(50);cc(52,0);cc(52);cc(52,0);
assert.equal(ui.splitFocus,1);assert.equal(ui.settingsPage,5);
cc(51);assert.equal(ui.view,'settings');assert.equal(ui.settingsPage,5);
cc(52);cc(52,0);assert.equal(ui.settingsPage,2);
// The browser exposes only its two relevant encoders, without an audio load.
cc(49);cc(43);cc(49,0);assert.equal(ui.view,'library');
assert.deepEqual(globalThis.__testSplit.keys(),[null,null,null,null,null,null,'preview_route','preview_level']);
globalThis.__testResetEncoders();toolWrites=paramWrites.length;
cc(77,2);cc(78,2);
assert(paramWrites.slice(toolWrites).some(([k,v])=>k==='preview_route'&&Number(v)===1));
assert(paramWrites.slice(toolWrites).some(([k])=>k==='preview_level'));
assert(!paramWrites.slice(toolWrites).some(([k])=>k==='algorithm'));
// A mode change inside Pages cannot restore a Split-only screen in Single.
ui.view='mixStatus';cc(52);cc(50);cc(52,0);cc(60);cc(51);
assert(!ui.splitEnabled);assert.equal(ui.view,'main');cc(60);
// Saved follows the completion transition, and errors stay until recovery.
tools.parseSave('2');assert.equal(ui.savedTicks,0);
tools.parseSave('4');assert.equal(ui.saveStatus,4);
ui.view='main';ui.messageTicks=0;globalThis.__testDrawMain();
assert([...screenText.values()].some(t=>t[2]==='SAVE FAILED: RETRYING'));
tools.drawMix();assert([...screenText.values()].some(t=>t[2]==='SAVE FAILED: RETRYING'));
cc(52);cc(50);cc(52,0);globalThis.tick();
assert([...screenText.values()].some(t=>t[2]==='SAVE FAILED: RETRYING'));
cc(51);
tools.parseSave('2');tools.parseSave('3');assert(ui.savedTicks>0);
tools.parseSave('bad');assert.equal(ui.saveStatus,3);
// Delete+Play calls the module command, never toggles playback or changes views.
toolWrites=blockingWrites.length;ui.activePads.add(70);
cc(119);cc(85);cc(85,0);cc(119,0);
assert(blockingWrites.slice(toolWrites).some(([k,v])=>k==='panic'&&Number(v)===1));
assert(!blockingWrites.slice(toolWrites).some(([k])=>k==='clip_playing'));
assert.equal(ui.activePads.size,0);
// Used white icon keys are primed immediately, including Capture in Single.
if(ui.splitEnabled){cc(60);cc(60,0);}
params.glitch_buffer_lock='0';params.stack_scan_paused='0';params.clip_stack_hold='0';
params.undo_available='0';
const idleUi=params.ui_state.split(',');idleUi[2]='0';params.ui_state=idleUi.join(',');
const idleClip=params.clip_state.split(',');idleClip[0]='0';params.clip_state=idleClip.join(',');
globalThis.init();
const lastButton=(id)=>buttonWrites.filter(w=>w[0]===id).at(-1)?.[1];
const settleIcons=()=>{for(let i=0;i<6;++i)globalThis.tick();};
const tapIcon=(id)=>{cc(id);cc(id,0);};
for(const id of [49,50,51,52,54,55,58,60,62,63,119])assert.equal(lastButton(id),16,'available icon '+id);
assert.equal(lastButton(88),0,'Mute is unassigned in Single');
assert.equal(lastButton(56),0,'Undo is dark without a safety undo point');
settleIcons();let stableIcons=buttonWrites.length;settleIcons();
assert.equal(buttonWrites.length,stableIcons,'steady icons generate no repeated MIDI writes');
idleUi[2]='1';params.ui_state=idleUi.join(',');settleIcons();assert.equal(lastButton(60),0);
idleUi[2]='0';params.ui_state=idleUi.join(',');settleIcons();assert.equal(lastButton(60),16);
idleClip[0]='1';params.clip_state=idleClip.join(',');settleIcons();assert.equal(lastButton(60),0);
idleClip[0]='0';params.clip_state=idleClip.join(',');settleIcons();assert.equal(lastButton(60),16);
tapIcon(50);settleIcons();assert.equal(lastButton(50),127);assert.equal(lastButton(51),127);
tapIcon(50);settleIcons();assert.equal(lastButton(50),16);assert.equal(lastButton(51),16);
params.undo_available='1';settleIcons();cc(49);settleIcons();
assert.equal(lastButton(49),127);assert.equal(lastButton(51),16,'Back brightness follows navigation, not undo');
assert.equal(lastButton(56),127,'the dedicated Undo key advertises availability');
cc(49,0);settleIcons();assert.equal(lastButton(49),16);assert.equal(lastButton(51),16);
cc(119);settleIcons();assert.equal(lastButton(119),127);
cc(119,0);settleIcons();assert.equal(lastButton(119),16);
cc(49);tapIcon(43);cc(49,0);settleIcons();
assert.equal(ui.view,'library');assert.equal(lastButton(62),0);assert.equal(lastButton(63),0);
assert.equal(lastButton(55),16);cc(55);settleIcons();assert.equal(lastButton(55),127);
cc(55,0);settleIcons();assert.equal(lastButton(55),16);
ui.view='main';tapIcon(60);settleIcons();assert(ui.splitEnabled);
assert.equal(lastButton(60),127);assert.equal(lastButton(88),16);
tapIcon(88);settleIcons();assert.equal(lastButton(88),127);
tapIcon(52);settleIcons();assert.equal(lastButton(52),127);assert.equal(lastButton(88),16);
tapIcon(60);settleIcons();assert.equal(lastButton(52),16);assert.equal(lastButton(88),0);
cc(52);tapIcon(50);cc(52,0);settleIcons();assert.equal(ui.view,'pages');
assert.equal(lastButton(52),127);assert.equal(lastButton(50),127);
tapIcon(51);settleIcons();assert.equal(lastButton(52),16);
globalThis.onUnload();
for(const id of [49,50,51,52,54,55,56,58,60,62,63,88,119])assert.equal(lastButton(id),0,'unload clears icon '+id);
globalThis.init();assert.equal(lastButton(52),16,'re-entry restores the page-picker hint before any tick');
// Physical Undo restores the safety snapshot without navigating or exiting.
const beforeUndoExit=exitCount;
params.undo_available='1';settleIcons();let undoWrites=blockingWrites.length;
tapIcon(56);settleIcons();
assert(blockingWrites.slice(undoWrites).some(([key])=>key==='undo'));
assert.equal(ui.view,'main');assert.equal(exitCount,beforeUndoExit);
assert.equal(params.undo_available,'0');assert.equal(lastButton(56),0);
assert.equal(globalThis.__testParams.locus.value,.23,'Undo refreshes restored parameter values');
undoWrites=blockingWrites.length;tapIcon(56);settleIcons();
assert.equal(ui.message,'NOTHING TO UNDO');assert.equal(exitCount,beforeUndoExit);
assert(!blockingWrites.slice(undoWrites).some(([key])=>key==='undo'));
// A refused undo retains its point and gives a readable wait message.
params.undo_available='1';rejectSafetyUndo=true;settleIcons();tapIcon(56);settleIcons();
assert.equal(lastButton(56),127);assert.equal(ui.message,'WAIT FOR TAKE TO SAVE');
rejectSafetyUndo=false;tapIcon(50);settleIcons();
ui.settingsPage=fx.pageIndex();ui.fxRouteGrabbed=true;ui.fxRoutePending=true;
tapIcon(56);settleIcons();
assert(!ui.fxRouteGrabbed&&!ui.fxRoutePending,'restoring a snapshot cancels stale FX-order drafts');
assert.equal(ui.view,'settings');assert.equal(params.undo_available,'0');
assert.equal(ui.actionMessage,'UNDONE');
// Recording gates Undo even if the last periodic poll was stale.
params.undo_available='1';params.recording='1';idleUi[2]='1';params.ui_state=idleUi.join(',');ui.recording=false;undoWrites=blockingWrites.length;
tapIcon(56);settleIcons();assert.equal(lastButton(56),0);
assert.equal(ui.message,'STOP RECORD TO UNDO');
assert(!blockingWrites.slice(undoWrites).some(([key])=>key==='undo'));
params.recording='0';idleUi[2]='0';params.ui_state=idleUi.join(',');idleClip[0]='1';params.clip_state=idleClip.join(',');undoWrites=blockingWrites.length;
tapIcon(56);settleIcons();assert.equal(lastButton(56),0);
assert(!blockingWrites.slice(undoWrites).some(([key])=>key==='undo'));
idleClip[0]='0';params.clip_state=idleClip.join(',');settleIcons();assert.equal(lastButton(56),127);
// Shift+Back now navigates normally; it never consumes the safety snapshot.
undoWrites=blockingWrites.length;cc(49);tapIcon(51);cc(49,0);settleIcons();
assert.equal(ui.view,'main');assert.equal(params.undo_available,'1');
assert(!blockingWrites.slice(undoWrites).some(([key])=>key==='undo'));
// Exit/re-entry releases and restores the dedicated key along with other icons.
globalThis.onUnload();assert.equal(lastButton(56),0);
globalThis.init();assert.equal(lastButton(56),127);
cc(49);tapIcon(50);cc(49,0);tapIcon(63);tapIcon(63);tapIcon(63);globalThis.tick();
assert.equal(ui.helpPage,3);
assert([...screenText.values()].some(t=>t[2]==='UNDO      sample/clip'));
for(const [x,y,text] of screenText.values()) {
    assert(x+String(text).length*6<=128,'shortcut text must fit the display');
    assert(y>=0&&y+7<=64,'shortcut rows must fit the display');
}
// Shared Dual is selectable from Single, without entering Split first.
ui.view='main';ui.activePads.clear();ui.voicePads=Array(8).fill(-1);ui.rollPads=Array(8).fill(-1);
params.voice_pads=params.voice_notes=params.roll_pads=params.roll_notes=Array(8).fill(-1).join(',');
params.playheads=params.voice_lanes=Array(8).fill(-1).join(',');
splitState.enabled=0;splitState.shared=0;splitState.focus=0;
for(const p of splitState.parts){p.mute=0;p.mode=0;p.scale=0;}
params.split_state=JSON.stringify(splitState);params.split_mode='0';params.mix_status='0,0,0,0,5,5';
globalThis.init();
const pickInstrumentMode=()=>{cc(52);tapIcon(50);cc(52,0);ui.pageSelection=tools.entries().findIndex(e=>e.view==='mode');tapIcon(3);};
pickInstrumentMode();assert.equal(ui.view,'mode');assert.equal(ui.modeSelection,0);
tapIcon(54);tapIcon(54);assert.equal(ui.modeSelection,2);
tapIcon(3);settleIcons();assert(ui.sharedDual&&ui.splitEnabled);assert.equal(ui.view,'main');
assert.equal(globalThis.__testParams.split_mode.value,2);
assert.equal(padWrites.filter(w=>w[0]===73).at(-1)[1],35,'Shared B idle pads use muted plum');
assert.equal(padWrites.filter(w=>w[0]===69).at(-1)[1],0,'A retains its chromatic idle color');
// Both programs expose all four lane gains, one time each.
const sharedKeys=split.pages().flatMap(p=>p.keys||[]).filter(Boolean);
assert.equal(new Set(sharedKeys).size,sharedKeys.length);
const mixRec=split.pages().find(p=>p.title==='MIX / REC');
assert.deepEqual(mixRec.keys.slice(0,4),['lane_1_gain','lane_2_gain','lane_3_gain','lane_4_gain']);
tapIcon(40);assert.equal(ui.splitFocus,0,'Track 4 selects a shared source without focusing B');
const sharedLane=ui.lane;tapIcon(52);assert.equal(ui.splitFocus,1);assert.equal(ui.lane,sharedLane);
tapIcon(43);assert.equal(ui.splitFocus,1,'Track 1 must not focus A in Shared');
splitState.parts[1].scale=1;params.split_state=JSON.stringify(splitState);settleIcons();
const bRootIndex=Array.from({length:32},(_,i)=>i).find(i=>i%8>=4&&(globalThis.__testPadMidiNote(i)-splitState.parts[1].root)%12===0);
assert.notEqual(bRootIndex,undefined);
assert.equal(padWrites.filter(w=>w[0]===68+bRootIndex).at(-1)[1],52,'Shared B scale roots use brighter pink');
// Copy returns to the last dual layout; a failed change keeps the chooser open.
tapIcon(60);assert(!ui.splitEnabled&&ui.sharedDual);tapIcon(60);assert(ui.splitEnabled&&ui.sharedDual);
pickInstrumentMode();tapIcon(51);assert.equal(ui.view,'pages');tapIcon(51);assert.equal(ui.view,'main');
pickInstrumentMode();ui.modeSelection=1;rejectModeSwitch=true;tapIcon(3);globalThis.tick();
assert.equal(ui.view,'mode');assert(ui.sharedDual);assert.equal(ui.actionMessage,'STOP RECORD FIRST');
assert([...screenText.values()].some(t=>t[2]==='STOP RECORD FIRST'));rejectModeSwitch=false;
tapIcon(3);settleIcons();assert(ui.splitEnabled&&!ui.sharedDual);
assert.equal(padWrites.filter(w=>w[0]===73).at(-1)[1],93,'Split retains its distinct teal background');
pickInstrumentMode();ui.modeSelection=2;tapIcon(3);
// Shared source readers use a common four-row zoom, with A top/B bottom caps.
ui.view='main';ui.messageTicks=0;ui.mask=15;ui.regions=[];ui.laneRegions=Array(4).fill(null);
ui.waves=Array.from({length:4},()=>Array.from({length:112},()=>[0,0]));
ui.fieldStart=0;ui.fieldEnd=1;ui.playheads=Array(8).fill(-1);ui.stackPositions=Array(8).fill(-1);
ui.voiceAlgorithms=Array(8).fill(-1);ui.playheads[0]=.2;ui.playheads[4]=.8;
ui.stackPositions[0]=1;ui.stackPositions[4]=0;ui.voiceAlgorithms[0]=ui.voiceAlgorithms[4]=4;
ui.mixVoices=[1,1];ui.splitFocus=0;
globalThis.__testClearDisplay();globalThis.__testDrawMain();
for(const [slot,capY] of [[0,10],[4,49]]) {
    const x=14+Math.round(ui.playheads[slot]*113);
    assert.equal(screenPixel(x,10),1);assert.equal(screenPixel(x,49),1);
    assert.equal(screenPixel(x-1,capY),1,'caps identify A above and B below the shared stack');
}
// Shared sources have plain labels and no A/B lane-pair divider.
for(let lane=0;lane<4;++lane) {
    const y=11+lane*10;
    assert.equal(screenText.get('3,'+y)?.[3],1,'Shared lane numbers remain white on black');
    assert.equal(screenPixel(2,y),0,'Shared lane labels have no inverted background');
}
assert.equal(screenPixel(11,30),0,'Shared must not divide lanes 2 and 3');
assert.equal(screenPixel(0,11+(ui.lane-1)*10),1,'the source destination mark remains visible');
const sharedPixels=wavePixels();let sharedFocusDraws=drawWrites.length;ui.splitFocus=1;globalThis.__testDrawMain();
assert.deepEqual(wavePixels(),sharedPixels,'focus changes must preserve the common source raster');
assert(!drawWrites.slice(sharedFocusDraws).some(w=>w[0]==='clear'),'Shared focus avoids a full clear');
for(const [x,y,text] of screenText.values())assert(x+String(text).length*6<=128,'Shared headings must fit');
// Real encoder turns can narrow and move the shared window while both parts play.
splitState.enabled=splitState.shared=1;splitState.focus=0;
splitState.parts[0].algorithm=splitState.parts[1].algorithm=4;
splitState.parts[0].window=splitState.parts[1].window=[0,1];
Object.assign(params,{split_state:JSON.stringify(splitState),split_mode:'2',locus:'.5',field:'1',
    playheads:'500,-1,-1,-1,800,-1,-1,-1',voice_lanes:'0,-1,-1,-1,1000,-1,-1,-1',
    voice_algorithms:'4,-1,-1,-1,4,-1,-1,-1',mix_status:'.1,.1,1,1,0,0'});
globalThis.init();settleIcons();
const zoomWaves=ui.waves;
globalThis.__testResetEncoders();cc(72,96);settleIcons();
assert(ui.fieldEnd-ui.fieldStart<.99,'FLD must zoom in with both instruments active');
const beforePan=(ui.fieldStart+ui.fieldEnd)*.5;
cc(71,8);settleIcons();
assert((ui.fieldStart+ui.fieldEnd)*.5>beforePan,'POS must pan the zoom while notes are sounding');
assert.equal(ui.waves,zoomWaves,'zoom reuses cached source waveforms');
assert(ui.playheads[0]>=0&&ui.playheads[4]>=0,'both parts continue to report active readers');
// The waveform and both cursor kinds use the same crop; off-window readers vanish.
ui.waves=Array.from({length:4},()=>Array.from({length:112},()=>[0,0]));
ui.fieldStart=.4;ui.fieldEnd=.6;ui.playheads[0]=.5;ui.playheads[4]=.8;
ui.regions=[];ui.laneRegions=Array(4).fill(null);ui.messageTicks=0;
globalThis.__testClearDisplay();globalThis.__testDrawMain();
const zoomA=14+Math.round(.5*113),outsideB=14+Math.round(.8*113);
assert.equal(screenPixel(zoomA,10),1);assert.equal(screenPixel(zoomA,49),1);
assert.equal(screenPixel(outsideB,10),0,'B must not be drawn at a misleading unzoomed position');
// Changing the crop must erase the previous waveform and cursor raster completely.
ui.waves=Array.from({length:4},(_,lane)=>Array.from({length:112},(_,bin)=>[-(bin+lane)%4,(bin*3+lane)%8]));
ui.fieldStart=.1;ui.fieldEnd=.5;ui.playheads[0]=.25;ui.playheads[4]=.82;
globalThis.__testClearDisplay();globalThis.__testDrawMain();
ui.fieldStart=.55;ui.fieldEnd=.95;ui.splitFocus=1;globalThis.__testDrawMain();
const retainedZoomPixels=wavePixels();
globalThis.__testClearDisplay();globalThis.__testDrawMain();
assert.deepEqual(wavePixels(),retainedZoomPixels,'live zoom must match a fresh raster without old pixels');
// Quad has four independent eight-pad zones and two source voices per zone.
splitState.enabled=0;splitState.shared=0;splitState.quad=0;splitState.focus=0;
splitState.parts.push({...splitState.parts[0],root:64,window:[.1,.5]}, {...splitState.parts[1],root:67,window:[.55,.95]});
for(const part of splitState.parts){part.mode=0;part.scale=0;part.mute=0;part.window=part.window||[0,1];}
params.split_state=JSON.stringify({...splitState,parts:splitState.parts.slice(0,2)});params.split_mode='0';
params.playheads=params.voice_lanes=params.voice_pads=params.voice_notes=Array(8).fill(-1).join(',');
params.mix_status='0,0,0,0,0,0,0,0,5,5,5,5';
globalThis.init();pickInstrumentMode();ui.modeSelection=3;tapIcon(3);settleIcons();
assert(ui.quadMode&&ui.splitEnabled);assert.equal(globalThis.__testParams.split_mode.value,3);
const quadPad=[16,20,0,4];
for(let p=0;p<4;++p) {
    assert.equal(globalThis.__testPadMidiNote(quadPad[p]),splitState.parts[p].root-12);
    assert.equal(padWrites.filter(w=>w[0]===68+quadPad[p]+1).at(-1)[1],[123,93,79,35][p]);
}
for(let p=0;p<4;++p){tapIcon([43,42,41,40][p]);assert.equal(ui.splitFocus,p);assert.equal(ui.lane,p+1);assert(ui.editorReady,'all four editors must accept snapshots');}
tapIcon(52);assert.equal(ui.splitFocus,0);tapIcon(52);assert.equal(ui.splitFocus,1);
const quadPages=split.pages();const quadKeys=quadPages.flatMap(p=>p.keys||[]).filter(Boolean);
assert.equal(quadKeys.length,new Set(quadKeys).size,'Quad parameters appear in one place each');
assert(!quadPages.some(p=>p.title==='STACK PATH'));
assert(!quadKeys.includes('manual_lane')&&!quadKeys.includes('lane_cycles'));
assert.equal(quadPages.find(p=>p.title==='ENGINE').keys[7],'split_level_b');
assert.equal(quadPages.find(p=>p.title==='FX SENDS').keys.length,8);
assert.equal(quadPages.find(p=>p.title==='DELAY RETURN').keys[7],'effect_mix');
const quadSkipbackPage=ui.settingsPage;cc(49);tapIcon(52);cc(49,0);assert.equal(ui.settingsPage,quadSkipbackPage);
cc(52);cc(50);cc(52,0);ui.pageSelection=globalThis.__testTools.entries().findIndex(e=>e.title==='FX SENDS');tapIcon(3);
tapIcon(3);assert.equal(ui.view,'fxMenu');ui.fxMenuSelection=1;tapIcon(3);
assert.equal(split.pages()[ui.settingsPage].title,'DELAY RETURN');
// C/D controls, focus indicators and one-lane cursors fit the display.
ui.view='main';ui.messageTicks=0;ui.mask=15;ui.waves=Array.from({length:4},()=>Array.from({length:112},()=>[0,0]));
ui.laneRegions=Array(4).fill(null);ui.playheads=Array.from({length:8},(_,i)=>(i+1)/10);
ui.stackPositions=Array.from({length:8},(_,i)=>Math.round(Math.floor(i/2)/3*1000)/1000);ui.voiceAlgorithms=Array(8).fill(4);
ui.splitParts=splitState.parts.map(p=>({...p,window:[0,1]}));ui.mixVoices=[2,2,2,2];ui.mixPeaks=[.1,.2,.3,.4];
globalThis.__testClearDisplay();globalThis.__testDrawMain();
for(let slot=0;slot<8;++slot) {
    const x=14+Math.round(ui.playheads[slot]*113),top=10+Math.floor(slot/2)*10;
    assert.equal(screenPixel(x,top),1);assert.equal(screenPixel(x,top+9),1);
    assert.equal(screenPixel(x,top===10?40:10),0,'Quad cursors stay in their own source row');
}
for(const [x,y,text] of screenText.values())assert(x+String(text).length*6<=128,'Quad heading must fit');
tools.drawMix();for(const [x,y,text] of screenText.values())assert(x+String(text).length*6<=128,'Quad status must fit');
ui.view='main';tapIcon(60);assert(!ui.splitEnabled&&ui.quadMode);tapIcon(60);assert(ui.splitEnabled&&ui.quadMode);
// With no stack scan in Quad, Loop is lit only for an applicable gesture.
params.effect_tape_inertia='0';globalThis.__testParams.effect_tape_inertia.value=0;ui.algorithm=4;settleIcons();
assert.equal(lastButton(58),0);
params.effect_tape_inertia='30';globalThis.__testParams.effect_tape_inertia.value=30;settleIcons();
assert.notEqual(lastButton(58),0);const quadLoopWrites=blockingWrites.length;tapIcon(58);
assert(blockingWrites.slice(quadLoopWrites).some(([key])=>key==='stack_scan_paused'));
// Holding Capture is a temporary layer; quick taps keep their focus action.
ui.activePads.clear();ui.performancePads.clear();ui.focusButtonHeld=false;ui.shiftHeld=false;
const captureFocus=ui.splitFocus;
cc(52);for(let tick=0;tick<10;++tick)globalThis.tick();
assert(ui.performanceVisible&&ui.focusButtonUsed);
assert.equal(ui.splitFocus,captureFocus);
const performanceDraw=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(performanceDraw).some(w=>w[0]==='print'&&String(w[3]).startsWith('PERFORM')));
const cueWrites=blockingWrites.length;
globalThis.onMidiMessageInternal([0x90,72,100]);
assert(ui.performancePads.has(72)&&!ui.activePads.has(72));
globalThis.onMidiMessageInternal([0xa0,72,64]);
assert.equal(ui.performanceValues[4],1,'Normal firm pressure should reach full punch strength');
cc(52,0);assert(!ui.performanceVisible&&ui.splitFocus===captureFocus);
assert(ui.performancePads.has(72),'Consumed pad releases remain owned after the modifier is released');
cc(52);for(let tick=0;tick<10;++tick)globalThis.tick();
globalThis.onMidiMessageInternal([0xa0,72,100]);
assert.equal(ui.performanceValues[4],0,'Rearming Capture must not revive a quarantined pad');
cc(52,0);
globalThis.onMidiMessageInternal([0x80,72,0]);assert(!ui.performancePads.has(72));
assert(!blockingWrites.slice(cueWrites).some(([key])=>key.startsWith('cue_')));
// Notes held before the modifier retain normal UI ownership.
globalThis.onMidiMessageInternal([0x90,69,100]);assert(ui.activePads.has(69));
cc(52);globalThis.onMidiMessageInternal([0x80,69,0]);assert(!ui.activePads.has(69));
cc(50);assert.equal(ui.view,'pages');assert(!ui.performanceVisible);cc(52,0);
for(let tick=0;tick<12;++tick)globalThis.tick();
assert(!ui.performanceVisible&&ui.view==='pages','Canceled Capture layers must not reopen on their hold timer');
assert.equal(ui.splitFocus,captureFocus,'Capture+Menu must not cycle focus');
// The host shortcut is safe in either press/release order, including a
// Capture performance panel opened before Shift is pressed.
for(const captureFirst of [false,true])for(const releaseShiftFirst of [false,true]) {
    ui.view='main';
    const beforeFocus=ui.splitFocus,beforePage=ui.settingsPage,startWrites=paramWrites.length;
    if(captureFirst) {
        cc(52);for(let tick=0;tick<12;++tick)globalThis.tick();
        assert(ui.performanceVisible,'Capture alone still opens the performance layer');
        cc(49);
    } else {cc(49);cc(52);}
    assert(!ui.performanceVisible&&!ui.performanceArmed,'Shift cancels the Thru performance layer');
    const firstRelease=releaseShiftFirst?49:52,secondRelease=releaseShiftFirst?52:49;
    cc(firstRelease,0);for(let tick=0;tick<12;++tick)globalThis.tick();cc(secondRelease,0);
    assert(!ui.shiftHeld&&!ui.focusButtonHeld&&!ui.performanceVisible&&!ui.performanceArmed);
    assert.equal(ui.splitFocus,beforeFocus,'The host shortcut must not change editing focus');
    assert.equal(ui.settingsPage,beforePage,'The host shortcut must not select a Thru page');
    assert.equal(ui.view,'main');
    assert(!paramWrites.slice(startWrites).some(([key])=>['split_focus','clip_record','record_gate','cue_store','cue_trigger'].includes(key)),
        'The host shortcut must not trigger Thru recording, focus or cues');
}
// All source labels must identify their part inside the four-character cell.
for(let source=0;source<6;++source) {
    const value=globalThis.__testCompactControlValue({...globalThis.__testParams.record_source,value:source});
    assert.equal(value,['IN','MIX','PT A','PT B','PT C','PT D'][source]);
    assert(value.length<=4);
}
ui.splitEnabled=true;ui.quadMode=false;ui.sharedDual=true;ui.mask=15;ui.view='main';
ui.activePads.clear();ui.performancePads.clear();ui.performancePadLive.clear();
cc(52);globalThis.onMidiMessageInternal([0x90,68,100]);
globalThis.onMidiMessageInternal([0xa0,68,0]);
ui.ticks=1;const fullLaneDraw=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(fullLaneDraw).some(w=>w[0]==='print'&&w[1]===2&&w[2]===20&&w[3]==='100%'),
    'One held lane pad is a full lane selection even with light pressure');
globalThis.onMidiMessageInternal([0x80,68,0]);cc(52,0);
// Authored path controls use one graph page, without taking over note/clip pads.
const defaultPathPoints=Array.from({length:8},(_,i)=>({time:[0,.25,.5,.75,.8,.85,.9,.95][i],lane:[0,.5,1,.5,.4,.3,.2,.1][i],curve:0}));
const pathResponse=(count,selected,points=defaultPathPoints)=>[count,selected,...points.flatMap(p=>[p.time,p.lane,p.curve])].join(',');
Object.assign(params,{algorithm:'1',lane_path:'8',stack_path_state:pathResponse(4,0),
    split_state:JSON.stringify({enabled:0,shared:0,quad:0,focus:0,blend:.5,parts:[{},{}]})});
globalThis.init();ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.title==='STACK PATH');
assert(ui.settingsPage>=0);assert(split.pages().some(p=>p.pathEditor));
assert.deepEqual(split.pages()[ui.settingsPage].keys,['lane_path','lane_blend',null,'lane_path_offset',null,null,'lane_slew']);
const pathOpenWrites=paramWrites.length;cc(3);cc(3,0);
assert(split.pages()[ui.settingsPage].pathEditor&&ui.pathReady);
assert(!paramWrites.slice(pathOpenWrites).some(([key])=>key==='clip_record'),'Opening the editor must not start data recording');
params.stack_path_state=pathResponse(4,1);globalThis.__testResetEncoders();cc(71,2);
assert.deepEqual(blockingWrites.at(-1),['stack_path_edit','select,1,0']);assert.equal(ui.pathPoint,1);
const movedPath=defaultPathPoints.map(p=>({...p}));movedPath[1].time=.255;
params.stack_path_state=pathResponse(4,1,movedPath);globalThis.__testResetEncoders();cc(72,2);
assert.deepEqual(blockingWrites.at(-1),['stack_path_edit','time,1,0.255']);
movedPath[1].lane=.51;params.stack_path_state=pathResponse(4,1,movedPath);globalThis.__testResetEncoders();cc(73,2);
assert.deepEqual(blockingWrites.at(-1),['stack_path_edit','lane,1,0.51']);
movedPath[1].curve=.025;params.stack_path_state=pathResponse(4,1,movedPath);globalThis.__testResetEncoders();cc(74,2);
assert.deepEqual(blockingWrites.at(-1),['stack_path_edit','curve,1,0.025']);
const fivePoints=defaultPathPoints.map(p=>({...p}));
for(let i=0;i<5;++i){fivePoints[i].time=i/5;fivePoints[i].lane=i/5;}
params.stack_path_state=pathResponse(5,2,fivePoints);globalThis.__testResetEncoders();cc(75,2);
assert.deepEqual(blockingWrites.at(-1),['stack_path_edit','count,1,5']);assert.equal(ui.pathCount,5);
const beforeUnshiftedReset=blockingWrites.length;globalThis.__testResetEncoders();cc(78,2);
assert.equal(blockingWrites.length,beforeUnshiftedReset);assert.equal(ui.actionMessage,'SHIFT+K8 RESETS PATH');
params.stack_path_state=pathResponse(4,0);cc(49);globalThis.__testResetEncoders();cc(78,2);cc(49,0);
assert.deepEqual(blockingWrites.at(-1),['stack_path_edit','reset,2,0']);assert.equal(ui.pathCount,4);
// Numeric labels stay inside the screen, including the bottom row.
ui.actionMessageTicks=0;ui.ticks=1;globalThis.__testClearDisplay();globalThis.tick();
for(const [x,y,text] of screenText.values())assert(x+String(text).length*6<=128&&y+8<=64,'Path editor text must fit');
cc(51);assert.equal(split.pages()[ui.settingsPage].title,'STACK PATH');
for(const algorithm of [3,4,5]){ui.algorithm=algorithm;assert(!split.pages().some(p=>p.pathEditor));}
ui.algorithm=1;ui.splitEnabled=true;ui.quadMode=true;assert(!split.pages().some(p=>p.pathEditor));
console.log('s3g Thru overtake UI tests: PASS');

// Pattern interpolation has a single shared page and keeps its controls in bounds.
params.pattern_fade_state='1,0,0,1,5000,30000,3,0,0,2000,12000,30000,0';
params.split_state=JSON.stringify({enabled:0,shared:0,quad:0,focus:0,parts:[{},{}]});
params.algorithm='1';globalThis.init();
for(const mode of [0,1,2,3]) {
    ui.splitEnabled=mode>0;ui.sharedDual=mode===2;ui.quadMode=mode===3;
    assert(split.pages().some(p=>p.patternFade));
}
ui.splitEnabled=false;ui.sharedDual=false;ui.quadMode=false;ui.algorithm=1;ui.view='settings';
ui.settingsPage=split.pages().findIndex(p=>p.patternFade);ui.ticks=2;
const fadeDraw=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(fadeDraw).some(w=>w[0]==='print'&&w[3]==='30s'));
assert(drawWrites.slice(fadeDraw).some(w=>w[0]==='print'&&w[3]==='SMTH'));
for(const w of drawWrites.slice(fadeDraw).filter(w=>w[0]==='print'))assert(w[1]+String(w[3]).length*6<=128&&w[2]+8<=64);
globalThis.__testResetEncoders();cc(71,127);cc(71,127);
assert.equal(blockingWrites.at(-1)[0],'pattern_fade_ms');assert(+blockingWrites.at(-1)[1]===29000);
globalThis.__testResetEncoders();cc(72,2);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_curve','4']);
globalThis.__testResetEncoders();cc(73,2);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_nudge','1']);
globalThis.__testResetEncoders();cc(74,2);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_hold','1']);
const beforeFadeReadouts=blockingWrites.length;
for(const ccNumber of [75,76]){globalThis.__testResetEncoders();cc(ccNumber,2);}
assert.equal(blockingWrites.length,beforeFadeReadouts,'Low and High are reserved in Fixed mode');
globalThis.__testResetEncoders();cc(78,2);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_finish','1']);
cc(3);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_hold','1']);

// A destination can change engine and instrument mode while this global page stays accessible.
params.algorithm='8';params.split_state=JSON.stringify({enabled:1,shared:0,quad:1,focus:2,
    parts:Array.from({length:4},()=>({algorithm:8,root:60,octave:0,scale:0,mode:0,mute:0}))});
params.clip_bank_state='1,-1,3,0';ui.ticks=2;globalThis.tick();
assert(split.pages()[ui.settingsPage].patternFade,'Pattern Fade must stay open across a mode/engine change');

// Capacitive knob 3 touch (note 2) latches Hold. Touch alone never moves the blend.
params.pattern_fade_state='1,0,0,1,4300,10000,3,0,0,2000,12000,10000,0';
ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.patternFade);
ui.patternFaderTouched=false;const beforeTouch=blockingWrites.length;
globalThis.onMidiMessageInternal([0x90,2,127]);
assert.deepEqual(blockingWrites.at(-1),['pattern_fade_hold','1']);
assert.equal(blockingWrites.length,beforeTouch+1);
assert(!blockingWrites.slice(beforeTouch).some(([k])=>k==='pattern_fade_nudge'||k==='pattern_fade_position'));
// Repeated touch-on cannot override a deliberate main-encoder resume.
globalThis.onMidiMessageInternal([0x90,2,127]);assert.equal(blockingWrites.length,beforeTouch+1);
params.pattern_fade_state='1,1,0,1,4300,10000,3,0,0,2000,12000,10000,0';cc(3);
assert.deepEqual(blockingWrites.at(-1),['pattern_fade_hold','0']);
const afterResume=blockingWrites.length;
globalThis.onMidiMessageInternal([0x90,2,127]);assert.equal(blockingWrites.length,afterResume);
globalThis.onMidiMessageInternal([0x90,2,0]);assert.equal(blockingWrites.length,afterResume);
assert(!ui.patternFaderTouched);
// A fresh touch, even with no turn, freezes the current point again.
params.pattern_fade_state='1,0,0,1,4500,10000,3,0,0,2000,12000,10000,0';
globalThis.onMidiMessageInternal([0x90,2,127]);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_hold','1']);
const beforeRelease=blockingWrites.length;globalThis.onMidiMessageInternal([0x80,2,64]);
assert(!ui.patternFaderTouched&&blockingWrites.length===beforeRelease,'Finger release must leave Hold engaged');
// Arming at rest works with TIME=CUT; main click can cancel the arm.
params.pattern_fade_state='0,0,-1,-1,0,0,0,0,0,2000,12000,0,0';
globalThis.onMidiMessageInternal([0x90,2,127]);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_hold','1']);
globalThis.onMidiMessageInternal([0x80,2,0]);params.pattern_fade_state='0,1,-1,-1,0,0,0,0,0,2000,12000,0,0';cc(3);
assert.deepEqual(blockingWrites.at(-1),['pattern_fade_hold','0']);
const unrelatedTouches=blockingWrites.length;
for(const note of [0,1,3,4,5,6,7,8,9]){globalThis.onMidiMessageInternal([0x90,note,127]);globalThis.onMidiMessageInternal([0x80,note,0]);}
assert.equal(blockingWrites.length,unrelatedTouches);
ui.view='main';globalThis.onMidiMessageInternal([0x90,2,127]);globalThis.onMidiMessageInternal([0x80,2,0]);
assert.equal(blockingWrites.length,unrelatedTouches,"Touching knob 3 on another page must keep that page's behavior");

// The long end reaches 30 seconds while retaining the existing 10-second point.
ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.patternFade);
params.pattern_fade_state='1,1,0,1,5000,30000,3,0,0,2000,12000,30000,0';
globalThis.__testResetEncoders();cc(71,2);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_ms','30000']);
params.pattern_fade_state='1,1,0,1,5000,10000,3,0,0,2000,12000,10000,0';
globalThis.__testResetEncoders();cc(71,2);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_ms','11000']);

// Random mode exposes range controls and the chosen duration, never a second editable TIME.
params.pattern_fade_state='1,1,0,1,5000,30000,3,17,1,2000,12000,6374,0';
ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.patternFade);ui.ticks=2;ui.actionMessageTicks=0;
const randomDrawStart=drawWrites.length;globalThis.tick();
const randomDraw=drawWrites.slice(randomDrawStart),randomText=randomDraw.filter(w=>w[0]==='print');
for(const word of ['NOW','6.37s','LOW','HIGH','MODE','RAND','2s','12s'])assert(randomText.some(w=>w[3]===word));
assert(!randomText.some(w=>['FROM','TO','BUF','TIME'].includes(w[3])));
for(const w of randomText)assert(w[1]+String(w[3]).length*6<=128&&w[2]+8<=64);
assert(randomText.some(w=>w[2]===21&&w[3]==='CURVE')&&randomText.some(w=>w[2]===31&&w[3]==='SMTH'));
assert(randomText.some(w=>w[2]===44&&w[3]==='LOW')&&randomText.some(w=>w[2]===54&&w[3]==='2s'));
assert(randomDraw.some(w=>w[0]==='rect'&&w[1]===77&&w[2]===13&&w[3]===49&&w[4]===1));
const beforeNowTurn=blockingWrites.length;globalThis.__testResetEncoders();cc(71,2);
assert.equal(blockingWrites.length,beforeNowTurn,'NOW is a readout');
globalThis.__testResetEncoders();cc(75,2);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_low_ms','2500']);
globalThis.__testResetEncoders();cc(76,2);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_high_ms','13000']);
globalThis.__testResetEncoders();cc(77,127);cc(77,127);assert.deepEqual(blockingWrites.at(-1),['pattern_fade_mode','0']);
// Buffer history remains available in the first Shift+Menu report.
ui.view='help';ui.helpPage=0;ui.ticks=1;const bufferReportStart=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(bufferReportStart).some(w=>w[0]==='print'&&w[3].includes('FADE BUF MISSES')&&w[3].trim().endsWith('17')));
for(const w of drawWrites.slice(bufferReportStart).filter(w=>w[0]==='print'))assert(w[1]+String(w[3]).length*6<=128&&w[2]+8<=64);

// The chain grid pages over 64 entries and routes step edits to the visible page.
ui.view='chain';ui.chainBank=0;ui.chainPage=0;ui.chainSlot=0;ui.chainEditorReturn=false;
cc(63);assert.equal(ui.chainPage,1);assert.deepEqual(blockingWrites.at(-1),['chain_slot','17']);
globalThis.onMidiMessageInternal([0x90,31,127]);assert.equal(ui.chainSlot,31);assert.deepEqual(blockingWrites.at(-1),['chain_slot','32']);
cc(63);cc(63);assert.equal(ui.chainPage,3);
globalThis.onMidiMessageInternal([0x90,31,127]);assert.equal(ui.chainSlot,63);assert.deepEqual(blockingWrites.at(-1),['chain_slot','64']);
cc(119);globalThis.onMidiMessageInternal([0x90,31,127]);cc(119,0);
assert.deepEqual(blockingWrites.slice(-2),[['chain_delete','63'],['chain_slot','64']]);
// Menu keeps the established first five controls and exposes Bank/End/Next.
cc(50);assert.equal(split.pages()[ui.settingsPage].title,'CHAIN');
assert.deepEqual(split.pages()[ui.settingsPage].keys,['chain_enabled','chain_slot','chain_clip','chain_repeats','chain_clear','chain_bank','chain_end','chain_advance']);
params.chain_state='0,0,0,0,-1,1,1,0';params.chain_entries=Array(64).fill('0:0:1').join(',');
globalThis.__testResetEncoders();cc(76,2);assert.deepEqual(blockingWrites.at(-1),['chain_bank','2']);
assert.equal(ui.chainBank,1);assert.equal(ui.chainEnabled,false);
cc(51);assert.equal(ui.view,'chain');
ui.chainEnabled=true;globalThis.__testResetEncoders();cc(78,2);assert.deepEqual(blockingWrites.at(-1),['chain_advance','1']);
// The default and entry editors share positions, and reserve inactive controls.
params.chain_fades_state='2,8000,2000,12000,3,2,3500,2000,12000,4';
ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.chainFade==='chain');ui.ticks=2;ui.actionMessageTicks=0;
const chainFadeDraw=drawWrites.length;globalThis.tick();
const chainFadeText=drawWrites.slice(chainFadeDraw).filter(w=>w[0]==='print');
for(const word of ['CHAIN FADE','LOW','HIGH','RAND','2s','12s'])assert(chainFadeText.some(w=>w[3]===word));
const beforeRandomTime=blockingWrites.length;globalThis.__testResetEncoders();cc(71,2);assert.equal(blockingWrites.length,beforeRandomTime);
globalThis.__testResetEncoders();cc(75,2);assert.deepEqual(blockingWrites.at(-1),['chain_fade_low_ms','2500']);
ui.settingsPage=split.pages().findIndex(p=>p.chainFade==='entry');ui.ticks=2;const entryDraw=drawWrites.length;globalThis.tick();
const entryText=drawWrites.slice(entryDraw).filter(w=>w[0]==='print');
for(const word of ['ENTRY FADE','TIME','3.50s','PWR','FIX'])assert(entryText.some(w=>w[3]===word));
for(const w of [...entryText,...chainFadeText])assert(w[1]+String(w[3]).length*6<=128&&w[2]+8<=64);
const beforeEntryStep=blockingWrites.length;globalThis.onMidiMessageInternal([0x90,18,127]);
assert.equal(blockingWrites.length,beforeEntryStep+1);assert.equal(blockingWrites.at(-1)[0],'chain_slot');
globalThis.__testResetEncoders();cc(71,2);assert.deepEqual(blockingWrites.at(-1),['entry_fade_ms','4000']);
// A stored transition exposes its actual duration, without allowing TIME to edit global defaults.
params.pattern_fade_state='1,0,0,1,5000,0,4,0,0,2000,12000,3500,1';
ui.settingsPage=split.pages().findIndex(p=>p.patternFade);ui.ticks=2;const storedDraw=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(storedDraw).some(w=>w[0]==='print'&&w[3]==='NOW'));
const beforeStoredTime=blockingWrites.length;globalThis.__testResetEncoders();cc(71,2);assert.equal(blockingWrites.length,beforeStoredTime);

const chainModeDraw=drawWrites.length;ui.ticks=2;globalThis.tick();
assert(drawWrites.slice(chainModeDraw).some(w=>w[0]==='print'&&w[3]==='CHN'));
const beforeStoredMode=blockingWrites.length;globalThis.__testResetEncoders();cc(77,2);
assert.equal(blockingWrites.length,beforeStoredMode,'A stored transition keeps its mode in the chain editors');

// Chain END exposes STOP and preserves it in the status used by the UI.
params.chain_state='1,0,0,0,-1,0,2,0';params.chain_end='2';
ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.title==='CHAIN');ui.ticks=2;ui.actionMessageTicks=0;
const stopStart=drawWrites.length;globalThis.tick();
assert.equal(ui.chainEnd,2);assert(drawWrites.slice(stopStart).some(w=>w[0]==='print'&&w[3]==='STOP'));
assert.deepEqual(globalThis.__testParams.chain_end.labels,['LOOP','STAY','STOP']);

// BANK keeps its fine-motion remainder through real bank selections.
const originalBlockingEncoderHost=globalThis.host_module_set_param_blocking;
globalThis.host_module_set_param_blocking=(key,value,...rest)=>{
    originalBlockingEncoderHost(key,value,...rest);
    if(key==='chain_bank'){
        params.chain_bank=value;params.chain_slot='1';
        params.chain_state='0,0,0,0,-1,'+(Number(value)-1)+',0,0';
    }
};
params.chain_bank='1';params.chain_state='0,0,0,0,-1,0,0,0';
ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.title==='CHAIN');
ui.chainBank=0;ui.chainSlot=0;globalThis.__testParams.chain_bank.value=1;
globalThis.__testResetEncoders();const fineBankStart=blockingWrites.length;
for(let turn=0;turn<16;turn++)cc(76,1);
assert.deepEqual(blockingWrites.slice(fineBankStart).filter(w=>w[0]==='chain_bank').map(w=>w[1]),['2','3','4','5']);
for(let turn=0;turn<16;turn++)cc(76,127);
assert.equal(params.chain_bank,'1','Equal travel in either direction must return to the same bank');
// Acceleration cannot jump multiple banks in one message or leave whole steps queued.
const burstBankStart=blockingWrites.length;cc(76,63);
assert.deepEqual(blockingWrites.slice(burstBankStart),[['chain_bank','2']]);
const afterBankBurst=blockingWrites.length;cc(76,1);assert.equal(blockingWrites.length,afterBankBurst);
cc(76,65);assert.equal(params.chain_bank,'1');
globalThis.host_module_set_param_blocking=originalBlockingEncoderHost;

// Entry Fade selects its own slot without changing the playing pattern.
const originalEntryHost=globalThis.host_module_set_param_blocking;
const entryRows=Array.from({length:64},(_,i)=>({enabled:i!==63,clip:i===1?20:i,repeats:1}));
params.chain_entries=entryRows.map(r=>`${r.enabled?1:0}:${r.clip}:${r.repeats}`).join(',');
params.chain_fades_state='0,0,2000,12000,0,2,3000,2000,12000,3';
globalThis.host_module_set_param_blocking=(key,value,...rest)=>{
    originalEntryHost(key,value,...rest);
    if(key==='chain_slot'){
        params.chain_slot=value;
        params.chain_state='1,12,0,'+(Number(value)-1)+',13,0,2,0';
        params.chain_fades_state=Number(value)===2?'0,0,2000,12000,0,3,8000,1000,9000,4':'0,0,2000,12000,0,2,3000,2000,12000,3';
    }
};
ui.chainEntries=entryRows;ui.chainBank=0;ui.chainSlot=0;ui.chainPage=0;
ui.chainPosition=12;ui.chainEnabled=true;ui.clipPlaying=true;const playingEntryPattern=ui.activeClip;
ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.chainFade==='entry');
const entrySelectorPage=ui.settingsPage;
globalThis.__testResetEncoders();const selectorWrites=blockingWrites.length,selectorDraw=drawWrites.length;
cc(73,2);
assert.deepEqual(blockingWrites.slice(selectorWrites),[['chain_slot','2']]);
assert.equal(ui.chainSlot,1);assert.equal(ui.chainPage,0);assert.equal(ui.settingsPage,entrySelectorPage);
assert.equal(ui.chainPosition,12);assert(ui.chainEnabled&&ui.clipPlaying);assert.equal(ui.activeClip,playingEntryPattern);
assert.equal(ui.chainFades[5],3,'The new entry immediately supplies its own fade mode');
const selectorText=drawWrites.slice(selectorDraw).filter(w=>w[0]==='print');
for(const word of ['ENTRY FADE','C01:02','SLOT','02','PATT','B05','RAND','1s','9s'])assert(selectorText.some(w=>w[3]===word));
for(const w of selectorText)assert(w[1]+String(w[3]).length*6<=128&&w[2]+8<=64);
const beforePatternReadout=blockingWrites.length;globalThis.__testResetEncoders();cc(74,2);
assert.equal(blockingWrites.length,beforePatternReadout,'PATT identifies the entry without editing its assignment');
ui.chainSlot=15;globalThis.__testResetEncoders();cc(73,2);
assert.equal(ui.chainSlot,16);assert.equal(ui.chainPage,1);assert.deepEqual(blockingWrites.at(-1),['chain_slot','17']);
ui.chainSlot=63;globalThis.__testResetEncoders();const atSlotLimit=blockingWrites.length;cc(73,2);
assert.equal(ui.chainSlot,63);assert.equal(blockingWrites.length,atSlotLimit);
ui.chainSlot=0;ui.chainPage=0;globalThis.__testResetEncoders();const selectorFine=blockingWrites.length;
for(let i=0;i<16;i++)cc(73,1);
assert.deepEqual(blockingWrites.slice(selectorFine).map(w=>w[1]),['2','3','4','5','6','7']);
for(let i=0;i<16;i++)cc(73,127);assert.equal(ui.chainSlot,0);
const atFirstSlot=blockingWrites.length;cc(73,126);assert.equal(ui.chainSlot,0);assert.equal(blockingWrites.length,atFirstSlot);
ui.settingsPage=split.pages().findIndex(p=>p.chainFade==='chain');
const beforeChainReserved=blockingWrites.length;cc(73,2);cc(74,2);
assert.equal(blockingWrites.length,beforeChainReserved,'Chain Fade keeps these positions reserved');
globalThis.host_module_set_param_blocking=originalEntryHost;

// Chain workflow actions and inherited timing readouts.
params.last_error='';params.chain_state='1,12,0,1,13,1,2,0,0,-1,-1';
params.chain_entries=entryRows.map(r=>`${r.enabled?1:0}:${r.clip}:${r.repeats}`).join(',');
params.chain_effective_fade='0,1,0,2000,12000,3';params.entry_effective_fade='1,0,8000,2000,12000,3';
ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.chainTools);ui.actionMessageTicks=0;
let workflowDraw=drawWrites.length;globalThis.tick();
let workflowText=drawWrites.slice(workflowDraw).filter(w=>w[0]==='print');
for(const word of ['CHAIN TOOLS','1 COPY','2 PASTE','3 COPY','4 PASTE','CHAIN','ENTRY','BANK','SLOT','CUE','HERE','CNCL','QUEU'])assert(workflowText.some(w=>w[3]===word),word+' absent: '+workflowText.map(w=>w[3]).join('|'));
for(const [index,key] of [[0,'chain_copy'],[1,'chain_paste'],[2,'entry_copy'],[3,'entry_paste'],[6,'chain_launch'],[7,'chain_launch_cancel']]){
    cc(71+index,2);assert.deepEqual(blockingWrites.at(-1),[key,'1']);
}
assert.equal(ui.chainBank,1);assert.equal(ui.chainPlayBank,0);assert(ui.chainEnabled,'Browsing and performing chains are independent');
ui.settingsPage=split.pages().findIndex(p=>p.chainFade==='entry');ui.actionMessageTicks=0;workflowDraw=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(workflowDraw).some(w=>w[0]==='print'&&w[3]==='CHAIN 8s SMTH'));
ui.settingsPage=split.pages().findIndex(p=>p.chainFade==='chain');workflowDraw=drawWrites.length;globalThis.tick();
assert(drawWrites.slice(workflowDraw).some(w=>w[0]==='print'&&w[3]==='GLBL R 2s-12s SMTH'));
assert(!split.pages().some(p=>p.transitionTake||p.title==='TRANSITION TAKE'));
// Copy/Paste labels have half the screen each, even with wider display glyphs.
for(const w of workflowText.filter(w=>/^[1-4] (COPY|PASTE)$/.test(w[3]))) {
    assert(w[1]+w[3].length*8<=(w[1]<64?63:128));assert(w[2]+8<41);
}
ui.settingsPage=split.pages().findIndex(p=>p.patternFade);globalThis.__testResetEncoders();cc(49);cc(78,2);cc(49,0);
assert(blockingWrites.slice(-3).some(w=>w[0]==='pattern_fade_return'));
console.log('s3g Thru chain workflow UI tests: PASS');

// Inherited mixed-unit/random ranges must keep the complete curve label.
ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.chainFade==='entry');ui.actionMessageTicks=0;
for(const [low,high] of [[25,1250],[750,30000],[25,750],[12345,29975]]) {
    params.entry_effective_fade=`0,1,0,${low},${high},3`;const start=drawWrites.length;globalThis.tick();
    const summary=drawWrites.slice(start).find(w=>w[0]==='print'&&w[2]===11);
    assert(summary&&summary[3].length<=21&&summary[3].endsWith('SMTH'));
}
console.log('s3g Thru inherited fade readout fit: PASS');

const shortcutNoteOn=(key,velocity=100)=>globalThis.onMidiMessageInternal([0x90,key,velocity]);
const shortcutNoteOff=key=>globalThis.onMidiMessageInternal([0x80,key,0]);
// Copy owns the gesture until release: held Steps never launch or move entries.
Object.assign(ui,{view:'clips',recording:false,clipRecording:false,shiftHeld:false,copyShortcut:null,clipBank:2,activeClip:5});params.last_error='';
let copyStart=blockingWrites.length;cc(60);assert.equal(blockingWrites.length,copyStart);shortcutNoteOn(19,100);
assert.deepEqual(blockingWrites.slice(copyStart),[['clip_clipboard_copy','35']]);cc(60,0);shortcutNoteOff(19);
assert.equal(blockingWrites.length,copyStart+1);
cc(49);copyStart=blockingWrites.length;cc(60);assert.equal(blockingWrites.length,copyStart);shortcutNoteOn(20,100);
assert.deepEqual(blockingWrites.slice(copyStart),[['clip_clipboard_paste','36']]);cc(60,0);shortcutNoteOff(20);cc(49,0);
copyStart=blockingWrites.length;cc(60);cc(60,0);assert.deepEqual(blockingWrites.slice(copyStart),[['clip_clipboard_copy','5']]);
cc(49);copyStart=blockingWrites.length;cc(60);cc(49,0);cc(60,0);assert.deepEqual(blockingWrites.slice(copyStart),[['clip_clipboard_paste','5']]);
ui.view='chain';ui.chainPage=2;copyStart=blockingWrites.length;cc(60);cc(60,0);
assert.deepEqual(blockingWrites.slice(copyStart),[['chain_clip_copy','1']]);
cc(49);copyStart=blockingWrites.length;cc(60);cc(60,0);cc(49,0);assert.deepEqual(blockingWrites.slice(copyStart),[['chain_clip_paste','1']]);
ui.chainPage=2;copyStart=blockingWrites.length;cc(60);shortcutNoteOn(21,100);cc(60,0);shortcutNoteOff(21);
assert.deepEqual(blockingWrites.slice(copyStart),[['chain_slot','38'],['chain_clip_copy','1']]);
ui.chainPage=2;cc(49);copyStart=blockingWrites.length;cc(60);shortcutNoteOn(22,100);cc(60,0);shortcutNoteOff(22);cc(49,0);
assert.deepEqual(blockingWrites.slice(copyStart),[['chain_slot','39'],['chain_clip_paste','1']]);
ui.view='settings';ui.settingsPage=split.pages().findIndex(p=>p.chainFade==='entry');copyStart=blockingWrites.length;cc(60);cc(60,0);
assert.deepEqual(blockingWrites.slice(copyStart),[['chain_clip_copy','1']]);
cc(49);copyStart=blockingWrites.length;cc(60);cc(60,0);cc(49,0);assert.deepEqual(blockingWrites.slice(copyStart),[['chain_clip_paste','1']]);
ui.view='clips';ui.recording=true;copyStart=blockingWrites.length;cc(60);shortcutNoteOn(23,100);cc(60,0);shortcutNoteOff(23);
assert.equal(blockingWrites.length,copyStart);ui.recording=false;
// Changing view cancels the pending tap rather than pasting in another context.
ui.view='chain';copyStart=blockingWrites.length;cc(60);ui.view='main';cc(60,0);assert.equal(blockingWrites.length,copyStart);

for(const mode of [0,1,2,3]) {
    Object.assign(ui,{splitEnabled:mode>0,sharedDual:mode===2,quadMode:mode===3,splitFocus:0,algorithm:4});
    const pages=split.pages(),loop=pages.filter(p=>p.title==='PERF LOOP');assert.equal(loop.length,1);assert(loop[0].global);
    assert(!loop[0].keys.includes('pressure_mode'));
    assert.equal(pages.find(p=>p.title==='PADS & SCALE').keys[5],mode===3?null:'pressure_mode');
    for(const title of ['PERF LOOP','OVERDUB TAKES','CHAIN','CHAIN FADE','ENTRY FADE','CHAIN TOOLS'])assert(globalThis.__testTools.entries().some(p=>p.title===title));
}
Object.assign(ui,{splitEnabled:true,sharedDual:false,quadMode:false,splitFocus:0,algorithm:4,view:'settings'});
ui.settingsPage=split.pages().findIndex(p=>p.title==='PERF LOOP');
params.algorithm='2';params.split_state=JSON.stringify({enabled:1,shared:0,quad:0,focus:1,blend:.5,parts:[{algorithm:4,root:60,octave:0,scale:0,mode:0,mute:0},{algorithm:2,root:60,octave:0,scale:0,mode:0,mute:0}]});
assert(globalThis.__testRefreshEditor());assert.equal(ui.algorithm,2);assert.equal(ui.splitFocus,1);assert.equal(split.pages()[ui.settingsPage].title,'PERF LOOP');
ui.actionMessageTicks=0;ui.clipRecording=false;ui.clipPlaying=false;const sharedPageDraw=drawWrites.length;fx.draw();
assert(drawWrites.slice(sharedPageDraw).some(w=>w[0]==='print'&&/^PERF LOOP /.test(w[3])));

copyStart=blockingWrites.length;cc(60);cc(60,0);
assert.deepEqual(blockingWrites.slice(copyStart),[['clip_clipboard_copy',String(ui.activeClip)]], 'Copy on PERF LOOP addresses the combined clip');

cc(49);copyStart=blockingWrites.length;cc(60);cc(42);assert.equal(ui.view,'settings');assert.equal(ui.clipBank,1);shortcutNoteOn(18);cc(60,0);shortcutNoteOff(18);cc(49,0);
assert.deepEqual(blockingWrites.slice(copyStart),[['clip_clipboard_paste','18']],'Track keys choose a clipboard bank without changing part or opening the browser');
cc(49);copyStart=blockingWrites.length;cc(60);cc(40);cc(60,0);cc(49,0);
assert.equal(blockingWrites.length,copyStart,'Bank navigation without a Step must not paste over the current clip');
console.log('s3g Thru copy shortcuts and shared performance pages: PASS');

// Keep Copy held: Shift switches each Step gesture from copy to paste.
ui.view='chain';ui.chainPage=1;ui.shiftHeld=false;copyStart=blockingWrites.length;
cc(60);shortcutNoteOn(16);shortcutNoteOff(16);cc(49);ui.chainPage=1;shortcutNoteOn(17);shortcutNoteOff(17);cc(49,0);ui.chainPage=1;shortcutNoteOn(18);shortcutNoteOff(18);cc(60,0);
assert.deepEqual(blockingWrites.slice(copyStart),[['chain_slot','17'],['chain_clip_copy','1'],['chain_slot','18'],['chain_clip_paste','1'],['chain_slot','19'],['chain_clip_copy','1']]);
ui.view='clips';ui.clipBank=0;copyStart=blockingWrites.length;cc(60);shortcutNoteOn(18);shortcutNoteOff(18);cc(49);shortcutNoteOn(19);shortcutNoteOff(19);cc(60,0);cc(49,0);
assert.deepEqual(blockingWrites.slice(copyStart),[['clip_clipboard_copy','2'],['clip_clipboard_paste','3']]);
assert(!blockingWrites.slice(copyStart).some(([key])=>key==='chain_paste'));
console.log('s3g Thru recorded-clip chain shortcuts: PASS');

// Imprint shares Morph's page: no duplicate control and no inactive FREQ dial.
ui.algorithm=10;ui.view='settings';globalThis.__testParams.morph_mode.value=1;
let imprintPage=split.pages().find(p=>p.title==='MORPH');assert.equal(imprintPage.keys[1],null);assert.equal(imprintPage.keys[5],'morph_mode');
assert(globalThis.__testLockableParams.includes('morph_mode'));
globalThis.__testParams.morph_mode.value=0;assert.equal(split.pages().find(p=>p.title==='MORPH').keys[1],'morph_frequency');
console.log('s3g Thru Morph Imprint controls: PASS');

{
    const ui=globalThis.__testSplit,state=globalThis.__testFx.state();
    state.algorithm=12;state.view='settings';state.splitEnabled=false;state.editorReady=true;state.actionMessageTicks=0;
    state.settingsPage=ui.pages().findIndex(p=>p.title==='FORMANT');
    assert.deepEqual(ui.pages()[state.settingsPage].keys,['formant_pitch','formant_shift','formant_smoothing','formant_tune','formant_freeze','formant_keytrack','formant_rough','formant_sub']);
    assert.equal(globalThis.__testParams.formant_smoothing.value,30);
    const before=drawWrites.length;globalThis.__testFx.draw();
    for(const label of ['PIT','FORM','SMTH','TUNE','FRZE','KEYT','ROUG','SUB'])assert(drawWrites.slice(before).some(w=>w[0]==='print'&&w[3]===label),'Formant shows '+label);
    for(const key of ['formant_tune','formant_freeze','formant_keytrack','formant_rough','formant_sub'])assert(globalThis.__testLockableParams.includes(key));
}
console.log('s3g Thru Formant controls: PASS');
