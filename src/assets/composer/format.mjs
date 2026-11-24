import {PARAMETERS} from './catalog.mjs';
export {PARAMETERS};
export const ENGINES = [
  'motion',  'lanes',      'grains',   'cutups', 'sample',  'slices',
  'glitch',  'weave',      'wavesets', 'prism',  'morph',   'texture',
  'formant', 'stochastic', 'pulsar',   'table',  'scanned', 'fm',
  'noise',   'reson',      'partials', 'lowform'
];
export const MODES =
    [ 'single', 'split', 'shared-dual', 'quad', 'bank', 'layers', 'sequence' ];
export const RATES =
    [ '1/1', '1/2', '1/4', '1/8', '1/16', '1/16T', '1/32', '1/32T', '1/64' ];
export const RATE_BEATS = [ 4, 2, 1, .5, .25, 1 / 6, .125, 1 / 12, .0625 ];
export const DIRECTIONS =
    [ 'forward', 'backward', 'ping-pong', 'repeat-ends', 'random' ];
export const PATHS = [ 'horizontal', 'vertical', 'diagonal' ];
export const POINT_TYPES = [
  'parameter',         'pressure',          'stack-position',
  'stack-hold',        'roll-position',     'roll-lane-position',
  'part-parameter',    'part-hold',         'performance',
  'stack-path',        'note-pressure',     'midi-expression',
  'layers-parameter',  'layers-baseline',   'sequence-lane',
  'sequence-recipe',   'control-lfo',       'pooled-lfo',
  'table-parameter',   'scanned-parameter', 'fm-parameter',
  'noise-parameter',   'reson-parameter',   'partials-parameter',
  'lowform-parameter', 'reson-density', 'source-parameter'
];
export const BY_KEY = new Map(PARAMETERS.map(p => [p.key, p]));
export const SOURCE_FREE = new Set(
    [ 'table', 'scanned', 'fm', 'noise', 'reson', 'partials', 'lowform' ]);
export const LIMITS = {
  clips : 64,
  assets : 512,
  tracks : 4,
  steps : 32,
  burst : 8,
  notesPerClip : 1024,
  pointsPerClip : 8192,
  totalNotes : 16384,
  totalPoints : 65536,
  jsonBytes : 16 * 1024 * 1024,
  assetBytes : 512 * 1024 * 1024,
  packBytes : 2 * 1024 * 1024 * 1024
};
const utf8 = new TextEncoder();
export const clone = x => JSON.parse(JSON.stringify(x));
export const safeId = s =>
    typeof s === 'string' && /^[A-Za-z0-9_-]{1,64}$/.test(s) &&
    !['__proto__', 'constructor', 'prototype'].includes(s);
export const hashId = s => typeof s === 'string' && /^[a-f0-9]{64}$/.test(s);
export const validName = s =>
    typeof s === 'string' && /^[A-Za-z0-9 _-]{0,40}$/.test(s);
export function uniqueId(prefix = 'clip') {
  const bytes = new Uint8Array(8);
  if (globalThis.crypto?.getRandomValues)
    crypto.getRandomValues(bytes);
  else
    for (let i = 0; i < bytes.length; i++)
      bytes[i] = Math.floor(Math.random() * 256);
  return prefix + '-' +
         Array.from(bytes, x => x.toString(16).padStart(2, '0')).join('');
}
// JSON.parse loses duplicate keys. Import rejects them before opening a draft.
export function parseCompositionJSON(input) {
  if (typeof input !== 'string' || utf8.encode(input).length > LIMITS.jsonBytes)
    throw Error('JSON file exceeds 16 MiB');
  let at = 0, nodes = 0;
  const fail = s => { throw Error(`${s} at byte ${at}`); };
  const ws = () => {
    while (/[\x20\t\n\r]/.test(input[at] || '!'))
      at++;
  };
  const take = c => {
    ws();
    if (input[at] === c) {
      at++;
      return true;
    }
    return false;
  };
  const string = () => {
    ws();
    if (input[at] !== '"')
      fail('Expected string');
    const start = at++;
    let escaped = false;
    while (at < input.length) {
      const c = input[at++];
      if (c === '"' && !escaped) {
        const s = JSON.parse(input.slice(start, at));
        if (utf8.encode(s).length > 1024 * 1024)
          fail('String too long');
        for (let i = 0; i < s.length; i++) {
          const c = s.charCodeAt(i);
          if (c >= 0xd800 && c <= 0xdbff) {
            const n = s.charCodeAt(++i);
            if (!(n >= 0xdc00 && n <= 0xdfff))
              fail('Unpaired surrogate');
          } else if (c >= 0xdc00 && c <= 0xdfff)
            fail('Unpaired surrogate');
        }
        return s;
      }
      if (c === '\\' && !escaped)
        escaped = true;
      else
        escaped = false;
    }
    fail('Unterminated string');
  };
  const value = depth => {
    ws();
    if (depth > 48 || ++nodes > 600000)
      fail('JSON complexity limit');
    if (input[at] === '"')
      return string();
    if (take('{')) {
      const out = Object.create(null);
      if (take('}'))
        return out;
      do {
        const key = string();
        if (Object.hasOwn(out, key))
          fail('Duplicate object key');
        if (!take(':'))
          fail('Missing colon');
        out[key] = value(depth + 1);
        if (take('}'))
          return out;
      } while (take(','));
      fail('Expected object delimiter');
    }
    if (take('[')) {
      const out = [];
      if (take(']'))
        return out;
      do {
        out.push(value(depth + 1));
        if (take(']'))
          return out;
      } while (take(','));
      fail('Expected array delimiter');
    }
    for (const literal of ['true', 'false', 'null'])
      if (input.startsWith(literal, at)) {
        at += literal.length;
        return JSON.parse(literal);
      }
    const m =
        /^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?/.exec(input.slice(at));
    if (!m)
      fail('Invalid value');
    at += m[0].length;
    const n = Number(m[0]);
    if (!Number.isFinite(n))
      fail('Non-finite number');
    return n;
  };
  const out = value(0);
  ws();
  if (at !== input.length)
    fail('Trailing JSON data');
  return out;
}
const need = (ok, path, message) => {
  if (!ok)
    throw Error(`${path}: ${message}`);
};
const object = (v, p) =>
    need(v !== null && typeof v === 'object' && !Array.isArray(v), p,
         'expected object');
const keys = (v, p, allowed) => {
  object(v, p);
  for (const k of Object.keys(v))
    need(allowed.includes(k), `${p}.${k}`, 'unknown field');
};
const field = (v, k, p) => {
  object(v, p);
  need(Object.hasOwn(v, k), `${p}.${k}`, 'required field');
  return v[k];
};
const num = (v, p, min, max, integer = false) => {
  need(typeof v === 'number' && Number.isFinite(v) && v >= min && v <= max &&
           (!integer || Number.isInteger(v)),
       p, 'number outside supported range');
  return v;
};
const str = (v, p, max = 120) => {
  need(typeof v === 'string' && utf8.encode(v).length <= max &&
           !v.includes('\0'),
       p, 'invalid text');
  return v;
};
const bool = (v, p) => {
  need(typeof v === 'boolean', p, 'expected boolean');
  return v;
};
const list = (v, p, max, min = 0) => {
  need(Array.isArray(v) && v.length >= min && v.length <= max, p,
       'array length outside supported range');
  return v;
};
const choice = (v, p, options) => {
  str(v, p);
  need(options.includes(v), p, 'unsupported choice ' + v);
  return v;
};
const optNum = (v, k, p, f, min, max, int = false) =>
    Object.hasOwn(v, k) ? num(v[k], p + '.' + k, min, max, int) : f;
const optBool = (v, k, p, f) =>
    Object.hasOwn(v, k) ? bool(v[k], p + '.' + k) : f;
function parameters(v, p, {locks = false} = {}) {
  object(v, p);
  need(Object.keys(v).length <= 943, p, 'too many parameters');
  for (const [k, n] of Object.entries(v)) {
    const parameter = BY_KEY.get(k);
    need(parameter && (!locks || parameter.lockable), p + '.' + k,
         'unsupported sound parameter');
    num(n, p + '.' + k, parameter.min, parameter.max, parameter.integer);
  }
}
function duration(v, p) {
  keys(v, p, [ 'unit', 'value' ]);
  const unit = choice(field(v, 'unit', p), p + '.unit', [ 'ms', 'steps' ]);
  num(field(v, 'value', p), p + '.value', unit === 'ms' ? 10 : .01,
      unit === 'ms' ? 30000 : 32);
}
function locks(v, p) {
  keys(v, p, [ 'position', 'pitch', 'velocity', 'duration', 'sound' ]);
  if (v.position !== undefined)
    num(v.position, p + '.position', 0, 1);
  if (v.pitch !== undefined)
    num(v.pitch, p + '.pitch', -48, 48, true);
  if (v.velocity !== undefined)
    num(v.velocity, p + '.velocity', 1 / 127, 1);
  if (v.duration !== undefined)
    duration(v.duration, p + '.duration');
  if (v.sound !== undefined)
    parameters(v.sound, p + '.sound', {locks : true});
}
function trig(v, p, indexed = true) {
  keys(v, p, [
    'step', 'enabled', 'microtime', 'probability', 'cycle', 'locks', 'burst',
    'protected'
  ]);
  if (indexed)
    num(field(v, 'step', p), p + '.step', 0, 31, true);
  optBool(v, 'enabled', p, false);
  optBool(v, 'protected', p, false);
  optNum(v, 'microtime', p, 0, -.5, .5);
  optNum(v, 'probability', p, 1, 0, 1);
  if (v.cycle !== undefined) {
    keys(v.cycle, p + '.cycle', [ 'hit', 'every' ]);
    const every =
        num(field(v.cycle, 'every', p), p + '.cycle.every', 1, 8, true);
    num(field(v.cycle, 'hit', p), p + '.cycle.hit', 1, every, true);
  }
  if (v.locks !== undefined)
    locks(v.locks, p + '.locks');
  if (v.burst !== undefined) {
    for (const c of list(v.burst, p + '.burst', 8, 1)) {
      keys(c, p + '.burst', [ 'offset', 'locks' ]);
      const t = num(field(c, 'offset', p), p + '.burst.offset', 0, 1);
      need(t < 1, p + '.burst.offset', 'must be less than one step');
      if (c.locks !== undefined)
        locks(c.locks, p + '.burst.locks');
    }
  }
}
const GEN_LOW =
    [ 0, 0, 0, 0, 1, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, .01, .01, 0, -1 ];
const GEN_HIGH = [
  2, 32, 5,  6,  16, 1, 1,     65535, 64,  30, 1,
  1, 5,  24, 32, 1,  3, 30000, 30000, .25, 1
];
const unitMatches = (t, unit, p) => {
  if (t.locks?.duration)
    need(t.locks.duration.unit === unit, p,
         'duration unit must match the lane');
  for (const c of t.burst || [])
    if (c.locks?.duration)
      need(c.locks.duration.unit === unit, p,
           'Burst duration unit must match the lane');
};
function track(v, p) {
  keys(v, p, [
    'length', 'rate', 'rotation', 'direction', 'path', 'seed', 'enabled',
    'scaleEntry', 'defaults', 'trigs', 'generator'
  ]);
  num(field(v, 'length', p), p + '.length', 1, 32, true);
  choice(field(v, 'rate', p), p + '.rate', RATES);
  optNum(v, 'rotation', p, 0, 0, 31, true);
  optNum(v, 'seed', p, 0, 0, 4294967295, true);
  if (v.direction !== undefined)
    choice(v.direction, p + '.direction', DIRECTIONS);
  if (v.path !== undefined)
    choice(v.path, p + '.path', PATHS);
  optBool(v, 'enabled', p, true);
  optBool(v, 'scaleEntry', p, false);
  if (v.defaults !== undefined)
    {locks(v.defaults,p+'.defaults');need(v.defaults.pitch===undefined||v.defaults.pitch===0,p+'.defaults.pitch','set the lane root instead');need(!v.defaults.sound||Object.keys(v.defaults.sound).length===0,p+'.defaults.sound','put lane sound defaults in part.parameters');}
  const unit = v.defaults?.duration?.unit || 'ms';
  const seen = new Set();
  for (const t of list(field(v, 'trigs', p), p + '.trigs', 32)) {
    trig(t, p + '.trig');
    need(!seen.has(t.step), p + '.trigs', 'duplicate step');
    seen.add(t.step);
    unitMatches(t, unit, p + '.trig');
  }
  if (v.generator !== undefined) {
    const g = v.generator;
    keys(g, p + '.generator', [
      'type', 'shift', 'ready', 'timed', 'values', 'base', 'enabledMask',
      'enabledValues', 'burstMask'
    ]);
    choice(field(g, 'type', p), p + '.generator.type',
           [ 'normal', 'euclidean', 'onset' ]);
    optNum(g, 'shift', p, 0, 0, 31, true);
    optBool(g, 'ready', p, false);
    optBool(g, 'timed', p, false);
    for (const k of ['enabledMask', 'enabledValues', 'burstMask'])
      optNum(g, k, p, 0, 0, 4294967295, true);
    if (g.values !== undefined)
      list(g.values, p + '.generator.values', 21, 21)
          .forEach((x, i) => num(x, p + '.generator.values[' + i + ']',
                                 GEN_LOW[i], GEN_HIGH[i]));
    if (g.base !== undefined)
      for (const t of list(g.base, p + '.generator.base', 32, 32)) {
        trig(t, p + '.generator.base', false);
        unitMatches(t, g.timed ? 'ms' : 'steps', p + '.generator.base');
      }
  }
}
function mix(v, p) {
  keys(v, p, [
    'strips', 'priority', 'depthDb', 'thresholdDb', 'attackMs', 'holdMs',
    'releaseMs', 'hpfHz', 'kneeDb'
  ]);
  if (v.strips !== undefined)
    for (const s of list(v.strips, p + '.strips', 4, 4)) {
      keys(s, p + '.strip',
           [ 'pan', 'lowDb', 'midDb', 'highDb', 'midHz', 'solo', 'muted' ]);
      optNum(s, 'pan', p, 0, -1, 1);
      for (const k of ['lowDb', 'midDb', 'highDb'])
        optNum(s, k, p, 0, -12, 12);
      optNum(s, 'midHz', p, 900, 120, 8000);
      optBool(s, 'solo', p, false);
      optBool(s, 'muted', p, false);
    }
  for (const [k, f, min, max, int] of [
           [ 'priority', 0, 0, 4, true ], [ 'depthDb', 0, 0, 24 ],
           [ 'thresholdDb', -24, -60, 0 ], [ 'attackMs', 12, 1, 250 ],
           [ 'holdMs', 40, 0, 1000 ], [ 'releaseMs', 220, 10, 3000 ],
           [ 'hpfHz', 100, 20, 2000 ], [ 'kneeDb', 6, 0, 24 ]])
    optNum(v, k, p, f, min, max, int);
}
function fade(v, p) {
  keys(v, p, [ 'mode', 'timeMs', 'lowMs', 'highMs', 'curve' ]);
  choice(field(v, 'mode', p), p + '.mode',
         [ 'default', 'cut', 'fixed', 'random' ]);
  for (const k of ['timeMs', 'lowMs', 'highMs'])
    optNum(v, k, p, 0, 0, 30000);
  if (v.curve !== undefined)
    choice(v.curve, p + '.curve', [ 'linear', 'in', 'out', 'smooth', 'power' ]);
  if (v.lowMs !== undefined && v.highMs !== undefined)
    need(v.lowMs <= v.highMs, p, 'lowMs must not exceed highMs');
}
export function validateComposition(doc) {
  keys(doc, 'composition', [
    '$schema', 'format', 'version', 'title', 'tempo', 'clips', 'chains',
    'assets', 'extensions', 'provenance'
  ]);
  need(field(doc, 'format', 'composition') === 's3g-thrupack', 'format',
       'unsupported format');
  num(field(doc, 'version', 'composition'), 'version', 1, 1, true);
  need(validName(field(doc, 'title', 'composition')), 'title',
       'use up to 40 letters, digits, spaces, hyphens or underscores');
  num(field(doc, 'tempo', 'composition'), 'tempo', 1, 999);
  const assets = new Set(), clipIds = new Set();
  let notesTotal = 0, pointsTotal = 0;
  for (const a of list(field(doc, 'assets', 'composition'), 'assets', 512)) {
    keys(a, 'asset', [
      'id', 'name', 'sha256', 'size', 'file', 'hint', 'url', 'waveform',
      'frames', 'sampleRate'
    ]);
    need(safeId(field(a, 'id', 'asset')) && !assets.has(a.id), 'asset.id',
         'invalid or duplicate ID');
    assets.add(a.id);
    str(field(a, 'name', 'asset'), 'asset.name', 200);
    if (a.sha256 !== undefined)
      need(hashId(a.sha256), 'asset.sha256', 'expected lowercase SHA-256');
    if (a.size !== undefined)
      num(a.size, 'asset.size', 1, LIMITS.assetBytes, true);
    if (a.file !== undefined)
      need(hashId(a.sha256) && a.file === `assets/${a.sha256}.wav`,
           'asset.file', 'invalid asset path');
    if (a.hint !== undefined)
      str(a.hint, 'asset.hint', 200);
    if (a.url !== undefined)
      need(str(a.url, 'asset.url', 768).startsWith('/files/download?path='),
           'asset.url', 'only local file downloads are allowed');
    if (a.waveform !== undefined)
      for (const n of list(a.waveform, 'asset.waveform', 256))
        num(n, 'asset.waveform', -1, 1);
    if (a.frames !== undefined)
      num(a.frames, 'asset.frames', 1, 2147483647, true);
    if (a.sampleRate !== undefined)
      num(a.sampleRate, 'asset.sampleRate', 8000, 384000);
  }
  for (const c of list(field(doc, 'clips', 'composition'), 'clips', 64, 1)) {
    keys(c, 'clip', [
      'id', 'name', 'mode', 'sources', 'parts', 'parameters', 'sequence',
      'notes', 'automation', 'loop', 'extensions'
    ]);
    need(safeId(field(c, 'id', 'clip')) && !clipIds.has(c.id), 'clip.id',
         'invalid or duplicate ID');
    clipIds.add(c.id);
    need(validName(field(c, 'name', 'clip')), 'clip.name',
         'invalid saved name');
    choice(field(c, 'mode', 'clip'), 'clip.mode', MODES);
    const parts = ['single','layers'].includes(c.mode)
                      ? 1
                      : [ 'quad', 'sequence' ].includes(c.mode) ? 4 : 2;
    for (const s of list(field(c, 'sources', 'clip'), 'clip.sources', 4, 4)) {
      keys(s, 'source', [ 'asset', 'label' ]);
      if (s.asset != null)
        need(assets.has(s.asset), 'source.asset', 'unknown asset');
      if (s.label !== undefined)
        str(s.label, 'source.label', 63);
    }
    for (const p of list(field(c, 'parts', 'clip'), 'clip.parts', parts,
                         parts)) {
      keys(p, 'part', [ 'engine', 'root', 'scale', 'parameters' ]);
      choice(field(p, 'engine', 'part'), 'part.engine', ENGINES);
      optNum(p, 'root', 'part', 60, 0, 127, true);
      optNum(p, 'scale', 'part', 0, 0, 24, true);
      if (p.parameters !== undefined)
        parameters(p.parameters, 'part.parameters');
    }
    if (c.parameters !== undefined)
      parameters(c.parameters, 'clip.parameters');
    if (c.loop !== undefined) {
      keys(c.loop, 'clip.loop',
           [ 'durationMs', 'beats', 'rate', 'timing', 'takes', 'recorded', 'recordedTempo' ]);
      optNum(c.loop, 'durationMs', 'clip.loop', 2000, 1, 86400000, true);
      optNum(c.loop, 'beats', 'clip.loop', 4, .0001, 100000);optNum(c.loop,'recordedTempo','clip.loop',120,1,999);
      optNum(c.loop, 'rate', 'clip.loop', 1, .25, 4);
      optNum(c.loop, 'takes', 'clip.loop', 1, 0, 255, true);
      optBool(c.loop, 'recorded', 'clip.loop', true);
      if (c.loop.timing !== undefined)
        choice(c.loop.timing, 'clip.loop.timing', [ 'free', 'beats' ]);
    }
    if (c.sequence !== undefined) {
      keys(c.sequence, 'clip.sequence', [ 'tracks', 'mix' ]);
      list(field(c.sequence, 'tracks', 'sequence'), 'sequence.tracks', 4, 4)
          .forEach((t, i) => track(t, `sequence.track[${i}]`));
      if (c.sequence.mix !== undefined)
        mix(c.sequence.mix, 'sequence.mix');
    } else
      need(c.mode !== 'sequence', 'clip.sequence',
           'required for Sequence mode');
    if (c.notes !== undefined) {
      notesTotal += list(c.notes, 'clip.notes', 1024).length;
      for (const n of c.notes) {
        keys(n, 'note', [
          'timeMs', 'durationMs', 'note', 'velocity', 'pad', 'part', 'take',
          'engineEntry', 'engine', 'pressure', 'variation'
        ]);
        num(field(n, 'timeMs', 'note'), 'note.timeMs', 0, 86400000, true);
        num(field(n, 'durationMs', 'note'), 'note.durationMs', 1, 86400000,
            true);
        num(field(n, 'note', 'note'), 'note.note', 0, 127, true);
        num(field(n, 'velocity', 'note'), 'note.velocity', 0, 127, true);
        for (const [k, f, max] of [[ 'pad', 255, 255 ], [ 'part', 0, 3 ],
                                   [ 'take', 0, 255 ],
                                   [ 'engineEntry', 255, 255 ]])
          optNum(n, k, 'note', f, 0, max, true);
        need(n.engineEntry === undefined || n.engineEntry < 8 ||
                 n.engineEntry === 255,
             'note.engineEntry', 'expected list entry 0-7 or 255');
        if (n.engine != null)
          choice(n.engine, 'note.engine', ENGINES);
        optNum(n, 'pressure', 'note', 0, 0, 1);
        if (n.variation !== undefined) {
          keys(n.variation, 'note.variation',
               [ 'octave', 'position', 'stack', 'pan' ]);
          optNum(n.variation, 'octave', 'variation', 0, -4, 4, true);
          for (const k of ['position', 'stack', 'pan'])
            optNum(n.variation, k, 'variation', 0, -1, 1);
        }
      }
    }
    if (c.automation !== undefined) {
      let perClip = 0, windowCount = 0;
      for (const curve of list(c.automation, 'clip.automation', 8192)) {
        keys(curve, 'curve', [ 'target', 'points', 'windows' ]);
        if(curve.windows)for(const w of list(curve.windows,'curve.windows',128)){need(++windowCount<=128,'clip.automation','too many touch windows');list(w,'curve.window',2,2);num(w[0],'window.startMs',0,86400000,true);num(w[1],'window.durationMs',1,86400000,true);}
        const t = field(curve, 'target', 'curve');
        keys(t, 'curve.target', [ 'type', 'key', 'index', 'owner', 'take' ]);
        choice(field(t, 'type', 'curve.target'), 'curve.type', POINT_TYPES);
        if (t.key !== undefined)
          need(BY_KEY.has(t.key), 'curve.key', 'unknown parameter');
        optNum(t, 'index', 'curve.target', 0, 0, 255, true);
        optNum(t, 'owner', 'curve.target', 255, 0, 255, true);
        optNum(t, 'take', 'curve.target', 0, 0, 255, true);
        const points =
            list(field(curve, 'points', 'curve'), 'curve.points', 8192, 1);
        perClip += points.length;
        let previous = -1;
        for (const p of points) {
          list(p, 'curve.point', 2, 2);
          const at = num(p[0], 'curve.timeMs', 0, 86400000, true);
          need(at >= previous, 'curve.timeMs', 'points must be sorted');
          previous = at;
          num(p[1], 'curve.value', -1000000, 1000000);
        }
      }
      need(perClip <= 8192, 'clip.automation', 'too many control points');
      pointsTotal += perClip;
    }
  }
  need(notesTotal <= 16384 && pointsTotal <= 65536, 'composition',
       'event budget exceeded');
 for(const c of list(doc.chains??[],'chains',16)){
   keys(c, 'chain', [ 'name', 'end', 'fade', 'entries' ]);
   need(validName(field(c, 'name', 'chain')), 'chain.name',
        'invalid saved name');
   choice(field(c, 'end', 'chain'), 'chain.end', [ 'loop', 'stay', 'stop' ]);
   if (c.fade !== undefined)
     fade(c.fade, 'chain.fade');
   for (const e of list(field(c, 'entries', 'chain'), 'chain.entries', 32)) {
     keys(e, 'entry', [ 'clip', 'repeats', 'fade', 'enabled' ]);
     need(clipIds.has(field(e, 'clip', 'entry')), 'entry.clip', 'unknown clip');
     optNum(e, 'repeats', 'entry', 1, 0, 16, true);
     optBool(e, 'enabled', 'entry', true);
     if (e.fade !== undefined)
       fade(e.fade, 'entry.fade');
   }
 }
 if (doc.extensions !== undefined) {
   object(doc.extensions, 'extensions');
   const n = doc.extensions.s3gThru;
   if (n !== undefined) {
     keys(n, 'extensions.s3gThru', [ 'version', 'clips' ]);
     num(field(n, 'version', 'native'), 'native.version', 1, 1, true);
     const records = field(n, 'clips', 'native');
     object(records, 'native.clips');
     need(Object.keys(records).length <= 64, 'native.clips',
          'too many capsules');
     for (const [id, r] of Object.entries(records)) {
       need(safeId(id), 'native.clips', 'invalid capsule ID');
       keys(r, 'native.clip', [ 'file', 'assets', 'bankNames' ]);
       need(field(r, 'file', 'native.clip') === `extensions/s3g-thru/${id}.bin`,
            'native.file', 'invalid capsule path');
       for (const a of list(field(r, 'assets', 'native.clip'), 'native.assets',
                            512))
         need(assets.has(a), 'native.asset', 'unknown asset');
       if (r.bankNames !== undefined)
         for (const name of list(r.bankNames, 'native.bankNames', 16, 16))
           need(validName(name), 'native.bankName', 'invalid label');
     }
   }
 }
 for (const c of doc.clips)
   if (c.extensions !== undefined) {
     object(c.extensions, 'clip.extensions');
     const n = c.extensions.s3gThru;
     if (n !== undefined) {
       keys(n, 'clip.extensions.s3gThru', [ 'base' ]);
       need(safeId(n.base) &&
                Object.hasOwn(doc.extensions?.s3gThru?.clips || {}, n.base),
            'clip.native.base', 'missing native capsule');
     }
   }
 return doc;
}
export function defaultTrig(step) {
  return {
    step,
    enabled : false,
    microtime : 0,
    probability : 1,
    cycle : {hit : 1, every : 1},
    locks : {},
    burst : [ {offset : 0, locks : {}} ]
  };
}
export function defaultTrack(length = 16) {
  return {
    length,
    rate : '1/16',
    rotation : 0,
    direction : 'forward',
    path : 'horizontal',
    seed : 0,
    enabled : true,
    scaleEntry : false,
    defaults : {
      position : 0,
      pitch : 0,
      velocity : 100 / 127,
      duration : {unit : 'ms', value : 200}
    },
    trigs : Array.from({length : 32}, (_, i) => defaultTrig(i))
  };
}
export function newClip(name = 'New Clip') {
  return {
    id : uniqueId(),
    name,
    mode : 'sequence',
    sources : Array.from({length : 4}, () => ({asset : null, label : ''})),
    parts : Array.from(
        {length : 4},
        () => ({engine : 'sample', root : 60, scale : 0, parameters : {}})),
    parameters : {},
    sequence : {tracks : Array.from({length : 4}, () => defaultTrack())},
    notes : [],
    automation : [],
    loop : {durationMs : 2000, beats : 4, rate : 1, timing : 'beats', takes : 1}
  };
}
export function newComposition(title = 'New Composition') {
  return {
    format : 's3g-thrupack',
    version : 1,
    title,
    tempo : 120,
    clips : [ newClip() ],
    chains : [],
    assets : []
  };
}
export function sourceRequirements(doc, clip) {
  const ids = new Set(clip.sources.map(s => s.asset).filter(Boolean));
  const base = clip.extensions?.s3gThru?.base;
  for (const id of doc.extensions?.s3gThru?.clips?.[base]?.assets || [])
    ids.add(id);
  return ids;
}
export function retainedFiles(doc) {
  const assets = new Set(), capsules = new Set();
  for (const clip of doc.clips) {
    for (const id of sourceRequirements(doc, clip))
      assets.add(id);
    const base = clip.extensions?.s3gThru?.base;
    if (base)
      capsules.add(base);
  }
  const paths = new Set();
  for (const a of doc.assets)
    if (assets.has(a.id) && a.file)
      paths.add(a.file);
  for (const id of capsules) {
    const n = doc.extensions.s3gThru.clips[id];
    paths.add(n.file);
  }
  return {assets, capsules, paths};
}
export function portableDocument(doc) {
  const out = clone(doc), used = retainedFiles(out);
  out.assets = out.assets.filter(a => used.assets.has(a.id)).map(a => {
    delete a.url;
    return a;
  });
  if (out.extensions?.s3gThru) {
    for (const id of Object.keys(out.extensions.s3gThru.clips))
      if (!used.capsules.has(id))
        delete out.extensions.s3gThru.clips[id];
    if (!Object.keys(out.extensions.s3gThru.clips).length)
      delete out.extensions.s3gThru;
  }
  return out;
}
export function stepMilliseconds(doc, clip, track) {
  return RATE_BEATS[RATES.indexOf(track.rate)] * 60000 /
         (doc.tempo * (clip.loop?.rate || 1));
}
export function noteName(note) {
  return [ 'C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#',
           'B' ][((note % 12) + 12) % 12] +
         (Math.floor(note / 12) - 1);
}
export function gridPath(length, path = 'horizontal') {
  const out = [];
  if (path === 'horizontal') {
    for (let i = 0; i < length; i++)
      out.push(i);
  } else if (path === 'vertical') {
    for (let col = 0; col < 8; col++)
      for (let row = 0; row < 4; row++)
        if (row * 8 + col < length)
          out.push(row * 8 + col);
  } else
    for (let band = 0; band < 11; band++)
      for (let row = 0; row < 4; row++) {
        const col = band - row, n = row * 8 + col;
        if (col >= 0 && col < 8 && n < length)
          out.push(n);
      }
  return out;
}
export function hash32(x) {
  x ^= x >>> 16;
  x = Math.imul(x, 0x7feb352d);
  x ^= x >>> 15;
  x = Math.imul(x, 0x846ca68b);
  return (x ^ (x >>> 16)) >>> 0;
}
export function routeCell(track, tick) {
  const n = track.length, mode = track.direction || 'forward',
        period = n === 1 ? 1
                         : mode === 'ping-pong'
                               ? 2 * (n - 1)
                               : mode === 'repeat-ends' ? 2 * n : n;
  const p = tick % period;
  let ordinal = p;
  if (mode === 'backward')
    ordinal = n - 1 - p;
  else if (mode === 'ping-pong')
    ordinal = p < n ? p : period - p;
  else if (mode === 'repeat-ends')
    ordinal = p < n ? p : period - 1 - p;
  else if (mode === 'random')
    ordinal = hash32(0x44495231 ^ (track.seed || 0) ^ (tick >>> 0)) % n;
  return gridPath(n, track.path)[(ordinal + (track.rotation || 0)) % n];
}
export function changeDurationUnit(doc, clip, track, unit) {
  const old = track.defaults?.duration?.unit || 'ms';
  if (old === unit)
    return;
  const span = stepMilliseconds(doc, clip, track);
  const convert = d => {
    if (!d)
      return;
    const ms = d.unit === 'ms' ? d.value : d.value * span;
    d.unit = unit;
    d.value = unit === 'ms' ? Math.max(10, Math.min(30000, Math.round(ms)))
                            : Math.max(.01, Math.min(32, ms / span));
  };
  convert(track.defaults.duration);
  for (const t of track.trigs) {
    convert(t.locks?.duration);
    for (const c of t.burst || [])
      convert(c.locks?.duration);
  }
  if (track.generator) {
    track.generator.timed = unit === 'ms';
    for (const t of track.generator.base || []) {
      convert(t.locks?.duration);
      for (const c of t.burst || [])
        convert(c.locks?.duration);
    }
  }
}
export function varyTrack(track, {
  density = .5,
  timing = 8,
  accent = .3,
  spread = .2,
  seed = 74,
  protect = true
},
                          stepMs) {
  const out = clone(track);
  out.trigs = Array.from(
      {length : 32},
      (_, i) => clone(track.trigs.find(t => t.step === i) || defaultTrig(i)));
  let stream = seed >>> 0;
  const rand = () => {
    stream = hash32(stream + 0x9e3779b9);
    return stream / 4294967296;
  };
  for (const t of out.trigs) {
    if (t.step >= out.length || (protect && t.protected))
      continue;
    t.enabled = rand() < density;
    t.microtime =
        Math.max(-.5, Math.min(.5, (rand() * 2 - 1) * timing / stepMs));t.locks.velocity=Math.max(1/127,Math.min(1,(out.defaults?.velocity??100/127)+(rand()*2-1)*accent));
    if (spread > 0)
      t.locks.position =
          Math.max(0, Math.min(1, (out.defaults?.position || 0) +
                                      (rand() * 2 - 1) * spread));
  }
  if (out.generator) {
    out.generator.enabledMask = 4294967295;
    out.generator.burstMask = 4294967295;
    out.generator.enabledValues = out.trigs.reduce(
        (n, t) => t.enabled ? (n | (1 << t.step)) >>> 0 : n, 0);
    out.generator.base = out.trigs.map(t => {
      const x = clone(t);
      delete x.step;
      return x;
    });
  }
  return out;
}

export function selectComposition(doc,ids,includeChains=false){
 const out=clone(doc),selected=new Set(ids);out.clips=out.clips.filter(c=>selected.has(c.id));
 if(!includeChains)out.chains=[];
 else for(const c of out.chains||[])for(const e of c.entries)if(!selected.has(e.clip))throw Error('Include every clip used by the selected chains');
 return validateComposition(portableDocument(out));
}
