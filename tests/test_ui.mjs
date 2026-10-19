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

globalThis.__moveConstants = {
    MovePads: Array.from({ length: 32 }, (_, i) => 68 + i),
    MoveSteps: Array.from({ length: 16 }, (_, i) => 16 + i),
    MovePlay: 85,
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
        return [[], 0];
    },
    stat(path) {
        return [{ mode: path.endsWith('/Drums') ? 0x4000 : 0x8000 }, 0];
    },
};

const flatWave = '77'.repeat(112);
const params = {
    locus: '0.5', field: '1', speed: '1', manual_lane: '1',
    lane_cycles: '1', algorithm: '0', join: '1', output_gain: '1',
    record_lane: '1', pad_octave: '0', record_source: '0',
    ui_state: '1,1,0,0,250,100,900,1000,0,0,0,0,20,0,1,1,2',
    playheads: '100,700,-1,-1,-1,-1,-1,-1',
    voice_lanes: '333,800,-1,-1,-1,-1,-1,-1',
    voice_pads: '-1,-1,-1,-1,-1,-1,-1,-1',
    seq_bpm: '120', seq_swing: '50', seq_length: '16', seq_gate: '80',
    seq_page: '1', seq_probability: '100', seq_ratchets: '1', seq_micro: '0',
    automation_record: '0', chain_enabled: '0', chain_slot: '1',
    chain_pattern: '1', chain_repeats: '1',
    seq_state: '0,-1,0,120,16,50,80,1',
    seq_locks: Array(16).fill('0').join(','),
    seq_locks_hi: Array(16).fill('0').join(','),
    seq_note_counts: Array(16).fill('0').join(','),
    seq_pad_masks: Array(16).fill('0').join(','),
    seq_open_mask: '0',
    pattern_state: '0,-1,1,0',
    sample_load_state: '0,1,0,',
    pattern_prepare_state: '0,-1', chain_state: '0,0,0,0',
    lane_source: 'LIVE',
    lane_source_state: 'CAPTURE:LIVE', undo_available: '0',
    last_error: '',
    waveforms: [flatWave, flatWave, flatWave, flatWave].join('|'),
};
globalThis.host_module_get_param = (key) => params[key] ?? '';
globalThis.host_module_set_param = (key, value) => {
    paramWrites.push([key, value]);
    if (Object.hasOwn(params, key)) params[key] = value;
};
globalThis.host_module_set_param_blocking = (key, value) => {
    blockingWrites.push([key, value]);
    if (Object.hasOwn(params, key)) params[key] = value;
    if (key === 'seq_running') {
        const values = params.seq_state.split(',');
        values[0] = value;
        if (value === '0') values[1] = '-1';
        params.seq_state = values.join(',');
    }
    if (key === 'seq_page') {
        const values = params.seq_state.split(',');
        values[7] = value;
        params.seq_state = values.join(',');
    }
    if (key === 'seq_note_pad_toggle') {
        const [step, pad] = String(value).split(',').map(Number);
        const masks = params.seq_pad_masks.split(',').map(Number);
        const counts = params.seq_note_counts.split(',').map(Number);
        const bit = (1 << pad) >>> 0;
        if ((masks[step] & bit) !== 0) {
            masks[step] = (masks[step] & ~bit) >>> 0;
            counts[step]--;
            params.seq_open_mask = String((Number(params.seq_open_mask) & ~bit) >>> 0);
        } else {
            masks[step] = (masks[step] | bit) >>> 0;
            counts[step]++;
            params.seq_open_mask = String((Number(params.seq_open_mask) | bit) >>> 0);
        }
        params.seq_pad_masks = masks.join(',');
        params.seq_note_counts = counts.join(',');
    }
    if (key === 'seq_note_length') {
        const pad = Number(String(value).split(',')[1]);
        params.seq_open_mask = String(
            (Number(params.seq_open_mask) & ~((1 << pad) >>> 0)) >>> 0);
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
await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));

assert.equal(typeof globalThis.init, 'function');
assert.equal(typeof globalThis.tick, 'function');
assert.equal(typeof globalThis.onMidiMessageInternal, 'function');
assert.equal(typeof globalThis.onUnload, 'function');

globalThis.init();
assert(drawWrites.length > 100, 'waveform page should draw');
assert(drawWrites.some((write) => write[0] === 'print'
    && String(write[3]).includes(' P1 V')),
    'waveform header should identify the active Step page');
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 25 && write[2] === 10 && write[3] === 1 && write[4] === 40));
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 93 && write[2] === 10 && write[3] === 1 && write[4] === 40));
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 23 && write[2] === 23 && write[3] === 5
    && write[4] === 5 && write[5] === 0));
assert(drawWrites.some((write) => write[0] === 'rect'
    && write[1] === 23 && write[2] === 23 && write[3] === 5
    && write[4] === 1 && write[5] === 1));

/* DSP voice slots have stable colors. Two isomorphic pads that produce the
   same pitch can therefore show two independently identifiable playheads. */
params.voice_pads = '-1,-1,-1,-1,-1,-1,-1,-1';
params.voice_pads = '5,8,-1,-1,-1,-1,-1,-1';
for (let tick = 0; tick < 6; tick++) globalThis.tick();
assert(padWrites.some((write) => write[0] === 73 && write[1] === 1));
assert(padWrites.some((write) => write[0] === 76 && write[1] === 3));
params.voice_pads = '-1,-1,-1,-1,-1,-1,-1,-1';
for (let tick = 0; tick < 3; tick++) globalThis.tick();

globalThis.onMidiMessageInternal([0xB0, 42, 127]);
assert.deepEqual(blockingWrites.pop(), ['record_lane', '2']);

globalThis.onMidiMessageInternal([0xB0, 86, 127]);
assert.deepEqual(blockingWrites.pop(), ['record_gate', '1']);

const blockingBeforeStepSelect = blockingWrites.length;
globalThis.onMidiMessageInternal([0x90, 19, 127]);
globalThis.onMidiMessageInternal([0x80, 19, 0]);
assert.equal(blockingWrites.length, blockingBeforeStepSelect,
    'a Step tap selects a note-on boundary without toggling a trigger');
/* Repeated DSP-driven LED rebuilds must not starve the selected Step behind
   the 32 pad updates. This was the 0.13.1 hardware regression. */
params.voice_pads = '5,-1,-1,-1,-1,-1,-1,-1';
for (let tick = 0; tick < 3; tick++) globalThis.tick();
params.voice_pads = '-1,-1,-1,-1,-1,-1,-1,-1';
for (let tick = 0; tick < 3; tick++) globalThis.tick();
assert.deepEqual(padWrites.filter((write) => write[0] === 19).at(-1), [19, 3],
    'selected Step should remain orange while DSP status refreshes');
globalThis.onMidiMessageInternal([0x90, 19, 127]);
globalThis.onMidiMessageInternal([0x80, 19, 0]);

/* Free Play retains its immediate performance map. Engine selection has moved
   to the dedicated Engine page, leaving every Step key to the pattern. */
globalThis.onMidiMessageInternal([0xB0, 77, 1]);
assert.deepEqual(paramWrites.pop(), ['motion_mode', '1']);

globalThis.onMidiMessageInternal([0xB0, 71, 1]);
assert.deepEqual(paramWrites.pop(), ['locus', '0.5100']);

globalThis.onMidiMessageInternal([0xB0, 55, 127]);
assert.deepEqual(blockingWrites.slice(-2), [
    ['all_notes_off', '1'],
    ['pad_octave', '1'],
]);

/* Left/Right are dedicated Step-page buttons. Shift+Up/Down remains an alias
   and must not fall through to octave or settings-page navigation. */
globalThis.onMidiMessageInternal([0xB0, 63, 127]);
assert.deepEqual(blockingWrites.pop(), ['seq_page', '2']);
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
assert.deepEqual(blockingWrites.pop(), ['seq_page', '3']);
globalThis.onMidiMessageInternal([0xB0, 54, 127]);
assert.deepEqual(blockingWrites.pop(), ['seq_page', '2']);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
globalThis.onMidiMessageInternal([0xB0, 62, 127]);
assert.deepEqual(blockingWrites.pop(), ['seq_page', '1']);

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
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
assert.deepEqual(blockingWrites.pop(), ['seq_page', '2']);
globalThis.onMidiMessageInternal([0xB0, 54, 127]);
assert.deepEqual(blockingWrites.pop(), ['seq_page', '1']);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
assert.deepEqual(blockingWrites.pop(), ['algorithm', '1']);

/* A held Step turns knob edits into per-step locks. */
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
const lanesPageDrawStart = drawWrites.length;
globalThis.tick();
const lanesHeader = drawWrites.slice(lanesPageDrawStart).find((write) =>
    write[0] === 'print' && write[2] === 0);
assert(String(lanesHeader?.[3]).includes('LANES PATH 2/7'));
assert(!drawWrites.slice(lanesPageDrawStart).some((write) =>
    write[0] === 'print' && String(write[3]).includes('GRAIN')));
const blockingBeforeLock = blockingWrites.length;
globalThis.onMidiMessageInternal([0x90, 19, 127]);
globalThis.onMidiMessageInternal([0xB0, 72, 1]);
assert.deepEqual(paramWrites.pop(), ['seq_lock', '3,lane_path,3']);
globalThis.onMidiMessageInternal([0x80, 19, 0]);
assert.equal(blockingWrites.length, blockingBeforeLock,
    'holding a trig to lock a parameter must not toggle it off');
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
globalThis.onMidiMessageInternal([0x90, 19, 127]);
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
assert.deepEqual(paramWrites.pop(), ['seq_lock', '3,lane_path_offset,0.0100']);
globalThis.onMidiMessageInternal([0x80, 19, 0]);

/* Step, pads, later Step, pads writes explicit note-on/note-off boundaries.
   Moving the Step cursor must not extinguish notes; each lit pad is turned off
   individually, allowing members of a chord to have different lengths. */
globalThis.onMidiMessageInternal([0x90, 20, 127]);
globalThis.onMidiMessageInternal([0x80, 20, 0]);
globalThis.onMidiMessageInternal([0x90, 68, 100]);
assert.deepEqual(blockingWrites.pop(), ['seq_note_pad_toggle', '4,0,60,100']);
globalThis.onMidiMessageInternal([0x90, 69, 96]);
assert.deepEqual(blockingWrites.pop(), ['seq_note_pad_toggle', '4,1,61,96']);
globalThis.onMidiMessageInternal([0x80, 68, 0]);
globalThis.onMidiMessageInternal([0x80, 69, 0]);
for (let tick = 0; tick < 5; tick++) globalThis.tick();
assert(padWrites.some((write) => write[0] === 68 && write[1] === 3),
    'pending note-on pad should stay orange after release');
globalThis.onMidiMessageInternal([0x90, 22, 127]);
globalThis.onMidiMessageInternal([0x80, 22, 0]);
for (let tick = 0; tick < 5; tick++) globalThis.tick();
assert.deepEqual(padWrites.filter((write) => write[0] === 68).at(-1), [68, 3]);
assert.deepEqual(padWrites.filter((write) => write[0] === 69).at(-1), [69, 3]);
globalThis.onMidiMessageInternal([0x90, 68, 100]);
assert.deepEqual(blockingWrites.pop(), ['seq_note_length', '4,0,2']);
globalThis.onMidiMessageInternal([0x80, 68, 0]);
for (let tick = 0; tick < 5; tick++) globalThis.tick();
assert.deepEqual(padWrites.filter((write) => write[0] === 68).at(-1), [68, 0]);
assert.deepEqual(padWrites.filter((write) => write[0] === 69).at(-1), [69, 3]);
globalThis.onMidiMessageInternal([0x90, 24, 127]);
globalThis.onMidiMessageInternal([0x80, 24, 0]);
globalThis.onMidiMessageInternal([0x90, 69, 96]);
assert.deepEqual(blockingWrites.pop(), ['seq_note_length', '4,1,4']);
globalThis.onMidiMessageInternal([0x80, 69, 0]);

globalThis.onMidiMessageInternal([0xB0, 85, 127]);
assert.deepEqual(blockingWrites.pop(), ['seq_running', '1']);
for (let tick = 0; tick < 3; tick++) globalThis.tick();
assert.deepEqual(buttonWrites.filter((write) => write[0] === 85).at(-1),
    [85, 127], 'Play should show the running transport state');

/* The visible pages include only the selected engine plus Mix, Sample,
   Sequence, and Chain. */
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
assert.deepEqual(paramWrites.pop(), ['lane_1_gain', '1.0200']);
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
assert.deepEqual(paramWrites.pop(), ['sample_trim_start', '0.0100']);
/* Sample edits are actions rather than stored values. Either encoder direction
   uses the critical path, and the otherwise-invisible reset gets a header
   confirmation instead of leaving an inert-looking dash. */
globalThis.onMidiMessageInternal([0xB0, 75, 127]);
assert.deepEqual(blockingWrites.pop(), ['sample_normalize', '1']);
const normalizeDrawStart = drawWrites.length;
globalThis.tick();
assert(drawWrites.slice(normalizeDrawStart).some((write) => write[0] === 'print'
    && String(write[3]).includes('NORMALIZED L1')));
globalThis.onMidiMessageInternal([0xB0, 55, 127]);
globalThis.onMidiMessageInternal([0xB0, 71, 1]);
assert.deepEqual(paramWrites.pop(), ['seq_bpm', '121']);
globalThis.onMidiMessageInternal([0x90, 21, 127]);
globalThis.onMidiMessageInternal([0xB0, 75, 1]);
assert.deepEqual(paramWrites.pop(), ['automation_record', '1']);
globalThis.onMidiMessageInternal([0x80, 21, 0]);
globalThis.onMidiMessageInternal([0xB0, 50, 127]);

/* Holding Delete before a sequence Step clears its notes, note boundaries,
   and locks together, without selecting the Step on release. */
globalThis.onMidiMessageInternal([0xB0, 119, 127]);
globalThis.onMidiMessageInternal([0x90, 20, 127]);
assert.deepEqual(blockingWrites.pop(), ['seq_clear_step', '4']);
globalThis.onMidiMessageInternal([0x80, 20, 0]);
globalThis.onMidiMessageInternal([0xB0, 119, 0]);

/* Shift+Play opens four banks of sixteen patterns. Track buttons choose the
   bank; Shift+Step copies, Delete+Step clears, and a normal Step queues while
   transport is running. */
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 85, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
const patternDrawStart = drawWrites.length;
globalThis.tick();
assert(drawWrites.slice(patternDrawStart).some((write) => write[0] === 'print'
    && String(write[3]).includes('PATTERNS A01')));
globalThis.onMidiMessageInternal([0xB0, 42, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0x90, 16, 127]);
assert.deepEqual(blockingWrites.pop(), ['pattern_copy', '16']);
globalThis.onMidiMessageInternal([0x80, 16, 0]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
globalThis.onMidiMessageInternal([0x90, 17, 127]);
assert.deepEqual(blockingWrites.pop(), ['pattern_select', '17']);
globalThis.onMidiMessageInternal([0x80, 17, 0]);
globalThis.onMidiMessageInternal([0xB0, 119, 127]);
globalThis.onMidiMessageInternal([0x90, 18, 127]);
assert.deepEqual(blockingWrites.pop(), ['pattern_clear', '18']);
globalThis.onMidiMessageInternal([0x80, 18, 0]);
globalThis.onMidiMessageInternal([0xB0, 119, 0]);
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 85, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);

/* Shift+Track opens the device sample library for that lane. Directories and
   WAVs are browsed with the main encoder/button, then handed to the worker. */
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 41, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
globalThis.onMidiMessageInternal([0xB0, 3, 127]);
globalThis.onMidiMessageInternal([0xB0, 14, 1]);
globalThis.onMidiMessageInternal([0xB0, 3, 127]);
assert.deepEqual(blockingWrites.pop(), ['load_lane_file',
    '3,/data/UserData/UserLibrary/Samples/Drums/Kick 01.wav']);

/* Shift+Menu is help. Back returns to the waveform before a second Back
   exits, which is the DSP persistence save boundary. */
globalThis.onMidiMessageInternal([0xB0, 49, 127]);
globalThis.onMidiMessageInternal([0xB0, 50, 127]);
globalThis.onMidiMessageInternal([0xB0, 49, 0]);
globalThis.onMidiMessageInternal([0xB0, 51, 127]);
assert.equal(exitCount, 0, 'Back closes settings/help before exiting');
globalThis.onMidiMessageInternal([0xB0, 51, 127]);
assert.equal(exitCount, 1);

globalThis.onUnload();
assert(blockingWrites.some((write) => write[0] === 'all_notes_off'));
assert(buttonWrites.length > 0);

console.log('S3G Thru overtake UI tests: PASS');
