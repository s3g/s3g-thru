import assert from 'node:assert/strict';
import fs from 'node:fs';

const uiPath = process.argv[2];
assert(uiPath, 'ui.js path is required');

const padWrites = [];
const buttonWrites = [];
const paramWrites = [];
const blockingWrites = [];
const drawWrites = [];
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
    setLED(note, color) { padWrites.push([note, color]); },
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
    lane_cycles: '1', stack_scan_paused: '0', clip_stack_hold: '0',
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
globalThis.clear_screen = () => drawWrites.push(['clear']);
globalThis.print = (...args) => drawWrites.push(['print', ...args]);
globalThis.fill_rect = (...args) => drawWrites.push(['rect', ...args]);

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
globalThis.__testParameterPageSets = PLAYBACK_PAGES.map((enginePages) =>
    [ENGINE_PAGE, PADS_SCALE_PAGE, AMP_ENVELOPE_PAGE, ...enginePages,
        MIX_REC_PAGE, EFFECTS_PAGE, STEREO_ROTARY_PAGE, SAMPLE_PAGE,
        LOOP_PAGE, TAKE_PAGE, CHAIN_PAGE].map((page) => page.keys));
globalThis.__testMotionPages = Array.from({ length: 8 }, (_, sound) =>
    playbackPagesForAlgorithm(0, sound).map((page) => ({
        title: page.title, keys: page.keys,
    })));
globalThis.__testPlaybackPageTitles = PLAYBACK_PAGES.map((pages) =>
    pages.map((page) => page.title));
globalThis.__testLockableParams = Array.from(LOCKABLE_PARAMS);
globalThis.__testDrawMain = drawMain;
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
globalThis.__testPadMidiNote = padMidiNote;
globalThis.__testStereoRotaryPage = STEREO_ROTARY_PAGE.keys;
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
assert(globalThis.__testLockableParams.includes('long_grain_length'));
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
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 25 && write[2] === 10 && write[3] === 1 && write[4] === 40));
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 93 && write[2] === 10 && write[3] === 1 && write[4] === 40));
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 25 && write[2] === 10 && write[3] === 1
    && write[4] === 2 && write[5] === 1),
    'Position should use a short vertical tick at the top edge');
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 116 && write[2] === 48 && write[3] === 1
    && write[4] === 2 && write[5] === 1),
    'Field should use a short vertical tick at the bottom edge');
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 23 && write[2] === 23 && write[3] === 5
    && write[4] === 5 && write[5] === 0));
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 23 && write[2] === 23 && write[3] === 5
    && write[4] === 1 && write[5] === 1));
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
assert(String(scaleHeader?.[3]).includes('PADS & SCALE 2/12'));
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
    && String(write[3]).includes('AMP ENVELOPE 3/12')));
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
assert(String(lanesHeader?.[3]).includes('STACK PATH 4/12'));
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
for (let page = 0; page < 7; page++) {
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
const helpDrawStart = drawWrites.length;
for (let tick = 0; tick < 60; tick++) globalThis.tick();
assert(drawWrites.slice(helpDrawStart).some((write) =>
    write[0] === 'print' && String(write[3]).includes('L1.5')));
globalThis.onMidiMessageInternal([0xB0, 51, 127]);
assert.equal(exitCount, 0, 'Back closes settings/help before exiting');
globalThis.onMidiMessageInternal([0xB0, 51, 127]);
assert.equal(exitCount, 1);

globalThis.onUnload();
assert(blockingWrites.some((write) => write[0] === 'all_notes_off'));
assert(buttonWrites.length > 0);

console.log('s3g Thru overtake UI tests: PASS');
