import {
  hashId,
  LIMITS,
  parseCompositionJSON,
  portableDocument,
  retainedFiles,
  safeId,
  validateComposition
} from './format.mjs';

const enc = new TextEncoder(), dec = new TextDecoder('utf-8', {fatal : true});
const K = Uint32Array.from([
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
  0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
  0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
  0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
  0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
  0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
]);
const rotr = (x, n) => (x >>> n) | (x << (32 - n));
export class SHA256 {
  constructor() {
    this.h = Uint32Array.from([
      0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c,
      0x1f83d9ab, 0x5be0cd19
    ]);
    this.block = new Uint8Array(64);
    this.used = 0;
    this.bytes = 0;
    this.w = new Uint32Array(64);
  }
  compress() {
    const v = new DataView(this.block.buffer), w = this.w;
    for (let i = 0; i < 16; i++)
      w[i] = v.getUint32(i * 4);
    for (let i = 16; i < 64; i++) {
      const a = w[i - 15], b = w[i - 2];
      w[i] = (w[i - 16] + (rotr(a, 7) ^ rotr(a, 18) ^ (a >>> 3)) + w[i - 7] +
              (rotr(b, 17) ^ rotr(b, 19) ^ (b >>> 10))) >>>
             0;
    }
    let [a, b, c, d, e, f, g, h] = this.h;
    for (let i = 0; i < 64; i++) {
      const t = (h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) +
                 ((e & f) ^ (~e & g)) + K[i] + w[i]) >>>
                0,
            u = ((rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) +
                 ((a & b) ^ (a & c) ^ (b & c))) >>>
                0;
      h = g;
      g = f;
      f = e;
      e = (d + t) >>> 0;
      d = c;
      c = b;
      b = a;
      a = (t + u) >>> 0;
    }
    [a, b, c, d, e, f, g, h].forEach((n, i) => this.h[i] =
                                         (this.h[i] + n) >>> 0);
  }
  update(bytes) {
    if (typeof bytes === 'string')
      bytes = enc.encode(bytes);
    this.bytes += bytes.length;
    for (let at = 0; at < bytes.length;) {
      const n = Math.min(64 - this.used, bytes.length - at);
      this.block.set(bytes.subarray(at, at + n), this.used);
      this.used += n;
      at += n;
      if (this.used === 64) {
        this.compress();
        this.used = 0;
      }
    }
    return this;
  }
  finish() {
    const total = this.bytes;
    this.update(Uint8Array.of(128));
    while (this.used !== 56)
      this.update(Uint8Array.of(0));
    const end = new Uint8Array(8), v = new DataView(end.buffer);
    v.setUint32(0, Math.floor(total / 0x20000000));
    v.setUint32(4, (total * 8) >>> 0);
    this.update(end);
    return Array.from(this.h, x => x.toString(16).padStart(8, '0')).join('');
  }
}
export const sha256 = bytes => new SHA256().update(bytes).finish();
const crcTable = Uint32Array.from({length : 256}, (_, n) => {
  for (let i = 0; i < 8; i++)
    n = (n >>> 1) ^ ((n & 1) ? 0xedb88320 : 0);
  return n >>> 0;
});
export function crc32(bytes) {
  let n = 0xffffffff;
  for (const b of bytes)
    n = crcTable[(n ^ b) & 255] ^ (n >>> 8);
  return (n ^ 0xffffffff) >>> 0;
}
export const allowedPath = name =>
    name === 'manifest.json' || name === 'composition.json' ||
    /^assets\/[a-f0-9]{64}\.wav$/.test(name) ||
    (/^extensions\/s3g-thru\/[A-Za-z0-9_-]{1,64}\.bin$/.test(name) &&
     safeId(name.split('/')[2].slice(0, -4)));
const check = (ok, message) => {
  if (!ok)
    throw Error(message);
};
export function readZip(bytes) {
  if (bytes instanceof ArrayBuffer)
    bytes = new Uint8Array(bytes);
  check(bytes.length <= LIMITS.packBytes, 'Pack exceeds 2 GiB');
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.length);
  let end = -1;
  for (let i = bytes.length - 22; i >= Math.max(0, bytes.length - 65557); i--)
    if (v.getUint32(i, true) === 0x06054b50 &&
        i + 22 + v.getUint16(i + 20, true) === bytes.length) {
      end = i;
      break;
    }
  check(end >= 0, 'Missing ZIP directory');
  check(v.getUint16(end + 4, true) === 0 && v.getUint16(end + 6, true) === 0,
        'Multipart archives are not supported');
  const count = v.getUint16(end + 10, true),
        start = v.getUint32(end + 16, true), size = v.getUint32(end + 12, true);
  check(count <= 640 && count === v.getUint16(end + 8, true) &&
            start + size === end,
        'Invalid ZIP directory');
  const files = new Map(), ranges = [];
  let at = start, total = 0;
  for (let i = 0; i < count; i++) {
    check(at + 46 <= end && v.getUint32(at, true) === 0x02014b50,
          'Invalid ZIP entry');
    const flags = v.getUint16(at + 8, true),
          method = v.getUint16(at + 10, true), crc = v.getUint32(at + 16, true),
          packed = v.getUint32(at + 20, true),
          length = v.getUint32(at + 24, true), n = v.getUint16(at + 28, true),
          extra = v.getUint16(at + 30, true),
          comment = v.getUint16(at + 32, true),
          local = v.getUint32(at + 42, true);
    check(!(flags & 1) && (method === 0 || method === 8) &&
              ((v.getUint32(at + 38, true) >>> 16) & 0xf000) !== 0xa000,
          'Encrypted, symlink or unsupported ZIP entry');
    check(at + 46 + n + extra + comment <= end, 'Truncated directory');
    const name = dec.decode(bytes.subarray(at + 46, at + 46 + n));
    check(allowedPath(name) && !files.has(name),
          'Unsafe or duplicate ZIP path');
    const limit =
        name.startsWith('assets/')
            ? LIMITS.assetBytes
            : name.endsWith('.bin') ? 64 * 1024 * 1024 : LIMITS.jsonBytes;
    check(length <= limit && (total += length) <= LIMITS.packBytes,
          'Expanded pack exceeds budget');
    check(local + 30 <= start && v.getUint32(local, true) === 0x04034b50,
          'Invalid local header');
    const ln = v.getUint16(local + 26, true),
          le = v.getUint16(local + 28, true), data = local + 30 + ln + le;
    check(data + packed <= start &&
              dec.decode(bytes.subarray(local + 30, local + 30 + ln)) ===
                  name &&
              v.getUint16(local + 8, true) === method &&
              !(v.getUint16(local + 6, true) & 1),
          'Local ZIP header mismatch');
    if (!(flags & 8))
      check(v.getUint32(local + 14, true) === crc &&
                v.getUint32(local + 18, true) === packed &&
                v.getUint32(local + 22, true) === length,
            'Local ZIP sizes mismatch');
    ranges.push([ local, data + packed ]);
    const raw = bytes.subarray(data, data + packed);
    if (method === 0)
      check(packed === length && crc32(raw) === crc,
            'ZIP checksum mismatch: ' + name);
    files.set(name, {name, raw, method, crc, size : length});
    at += 46 + n + extra + comment;
  }
  check(at === end, 'Directory size mismatch');
  ranges.sort((a, b) => a[0] - b[0]);
  for (let i = 1; i < ranges.length; i++)
    check(ranges[i][0] >= ranges[i - 1][1], 'Overlapping ZIP entries');
  return files;
}
export async function inflate(entry) {
  if (entry.method === 0)
    return entry.raw;
  if(typeof process!=='undefined'&&process.versions?.node){
    const {inflateRawSync}=await import('node:zlib');
    const data=inflateRawSync(entry.raw,{maxOutputLength:Math.max(1,entry.size)});
    check(data.length===entry.size&&crc32(data)===entry.crc,'ZIP checksum or size mismatch');return new Uint8Array(data);
  }
  check(typeof DecompressionStream === 'function',
        'Browser cannot read deflated JSON; use stored ZIP entries');
  const reader = new Blob([ entry.raw ])
                     .stream()
                     .pipeThrough(new DecompressionStream('deflate-raw'))
                     .getReader();
  let n = 0;
  const chunks = [];
  try {
    while (true) {
      const r = await reader.read();
      if (r.done)
        break;
      n += r.value.length;
      check(n <= entry.size, 'Inflated ZIP exceeds declared size');
      chunks.push(r.value);
    }
  } finally {
    reader.releaseLock();
  }
  check(n === entry.size, 'Truncated ZIP data');
  const data = new Uint8Array(n);
  let at = 0;
  for (const c of chunks) {
    data.set(c, at);
    at += c.length;
  }
  check(crc32(data) === entry.crc, 'ZIP checksum mismatch');
  return data;
}
export function writeZip(entries) {
  let at = 0;
  const parts = [], central = [];
  for (const [name, entry] of entries) {
    check(allowedPath(name), 'Unsafe export path');
    const raw = entry.raw ?? entry, method = entry.method ?? 0,
          size = entry.size ?? raw.length, crc = entry.crc ?? crc32(raw),
          n = enc.encode(name);
    const local = new Uint8Array(30 + n.length), l = new DataView(local.buffer);
    l.setUint32(0, 0x04034b50, true);
    l.setUint16(4, 20, true);
    l.setUint16(6, 0x800, true);
    l.setUint16(8, method, true);
    l.setUint32(14, crc, true);
    l.setUint32(18, raw.length, true);
    l.setUint32(22, size, true);
    l.setUint16(26, n.length, true);
    local.set(n, 30);
    const dir = new Uint8Array(46 + n.length), d = new DataView(dir.buffer);
    d.setUint32(0, 0x02014b50, true);
    d.setUint16(4, 20, true);
    d.setUint16(6, 20, true);
    d.setUint16(8, 0x800, true);
    d.setUint16(10, method, true);
    d.setUint32(16, crc, true);
    d.setUint32(20, raw.length, true);
    d.setUint32(24, size, true);
    d.setUint16(28, n.length, true);
    d.setUint32(42, at, true);
    dir.set(n, 46);
    parts.push(local, raw);
    central.push(dir);
    at += local.length + raw.length;
  }
  const end = new Uint8Array(22), v = new DataView(end.buffer),
        directory = central.reduce((n, x) => n + x.length, 0);
  v.setUint32(0, 0x06054b50, true);
  v.setUint16(8, central.length, true);
  v.setUint16(10, central.length, true);
  v.setUint32(12, directory, true);
  v.setUint32(16, at, true);
  check(at + directory + 22 <= LIMITS.packBytes, 'Export exceeds 2 GiB');
  return new Blob([...parts, ...central, end ], {type : 'application/zip'});
}
export async function openPack(bytes) {
  const files = readZip(bytes),
        manifest = parseCompositionJSON(
            dec.decode(await inflate(files.get('manifest.json') || {})));
  check(manifest.format === 's3g-thrupack' && manifest.version === 1 &&
            manifest.composition === 'composition.json' &&
            Array.isArray(manifest.files),
        'Unsupported pack manifest');
  const compositionBytes = await inflate(files.get('composition.json') || {});
  check(sha256(compositionBytes) === manifest.sha256,
        'Composition hash mismatch');
  const doc =
      validateComposition(parseCompositionJSON(dec.decode(compositionBytes)));
  const declared = new Set([ 'manifest.json', 'composition.json' ]);
  for (const f of manifest.files) {
    check(allowedPath(f.path) && f.path !== 'manifest.json' &&
              f.path !== 'composition.json' && !declared.has(f.path) &&
              hashId(f.sha256),
          'Invalid manifest file');
    const e = files.get(f.path);
    check(e && e.size === f.size, 'Missing or mismatched pack file: ' + f.path);
    const data = await inflate(e);
    check(sha256(data) === f.sha256, 'File hash mismatch: ' + f.path);
    declared.add(f.path);
  }
  check(declared.size === files.size, 'Undeclared pack entries');
  const used = retainedFiles(doc);
  for (const path of used.paths) {
    if (path.startsWith('extensions/') || manifest.portable)
      check(files.has(path), 'Required file is missing: ' + path);
  }
  for (const a of doc.assets)
    if (files.has(a.file)) {
      const e = files.get(a.file);
      check(e.size === a.size && sha256(await inflate(e)) === a.sha256,
            'Asset metadata mismatch');
    }
  return {doc, files, portable : !!manifest.portable};
}
export async function buildPack(doc, files,
                                {portable = true, fetchAsset = null, omitAssets = new Set()} = {}) {
  validateComposition(doc);
  const out = portableDocument(doc), used = retainedFiles(out),
        entries = new Map(), records = [];
  for (const path of used.paths) {
    if(!portable && path.startsWith('assets/') && omitAssets.has(out.assets.find(a=>a.file===path)?.sha256))continue;
    let entry = files.get(path);
    if (!entry && portable && path.startsWith('assets/')) {
      const asset = doc.assets.find(a => a.file === path);
      check(fetchAsset && asset, 'Audio unavailable: ' + path);
      const bytes = await fetchAsset(asset);
      check(sha256(bytes) === asset.sha256 && bytes.length === asset.size,
            'Downloaded audio hash mismatch');
      entry = bytes;
    }
    if (!entry) {
      check(!path.startsWith('extensions/') && !portable,
            'Required file missing: ' + path);
      continue;
    }
    const data = entry instanceof Uint8Array ? entry : await inflate(entry);
    records.push({path, size : data.length, sha256 : sha256(data)});
    entries.set(path, entry);
  }
  const composition = enc.encode(JSON.stringify(out));
  const manifest = enc.encode(JSON.stringify({
    format : 's3g-thrupack',
    version : 1,
    composition : 'composition.json',
    sha256 : sha256(composition),
    portable,
    files : records
  }));
  entries.set('composition.json', composition);
  entries.set('manifest.json', manifest);
  return writeZip(entries);
}
export function wavInfo(bytes) {
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.length);
  const tag = n => String.fromCharCode(...bytes.subarray(n, n + 4));
  check(bytes.length >= 44 && tag(0) === 'RIFF' && tag(8) === 'WAVE',
        'Use a RIFF WAV sample');
  let format, channels, rate, bits, align, start, size;
  for (let at = 12; at + 8 <= bytes.length;) {
    const n = v.getUint32(at + 4, true), next = at + 8 + n;
    check(next <= bytes.length, 'Truncated WAV chunk');
    if (tag(at) === 'fmt ') {
      check(n >= 16, 'Invalid WAV format');
      format = v.getUint16(at + 8, true);
      channels = v.getUint16(at + 10, true);
      rate = v.getUint32(at + 12, true);
      align = v.getUint16(at + 20, true);
      bits = v.getUint16(at + 22, true);
    }
    if (tag(at) === 'data') {
      start = at + 8;
      size = n;
    }
    at = next + (n & 1);
  }
  check((format === 1 && [ 16, 24, 32 ].includes(bits) ||
         format === 3 && bits === 32) &&
            channels >= 1 && channels <= 2 && rate >= 8000 && rate <= 192000 &&
            align === channels * bits / 8 && size >= align &&
            size % align === 0,
        'Unsupported WAV encoding');
  const frames = size / align, bins = 128, waveform = [];
  for (let i = 0; i < bins; i++) {
    let lo = 0, hi = 0;
    const from = Math.floor(i * frames / bins),
          to = Math.max(from + 1, Math.floor((i + 1) * frames / bins));
    for (let frame = from; frame < Math.min(frames, to); frame++) {
      let x = 0;
      for (let c = 0; c < channels; c++) {
        const at = start + frame * align + c * bits / 8;
        let n;
        if (format === 3)
          n = v.getFloat32(at, true);
        else if (bits === 8)
          n = (v.getUint8(at) - 128) / 128;
        else if (bits === 16)
          n = v.getInt16(at, true) / 32768;
        else if (bits === 24) {
          let q = v.getUint8(at) | (v.getUint8(at + 1) << 8) |
                  (v.getInt8(at + 2) << 16);
          n = q / 8388608;
        } else
          n = v.getInt32(at, true) / 2147483648;
        check(Number.isFinite(n), 'Non-finite WAV audio');
        x += n / channels;
      }
      lo = Math.min(lo, x);
      hi = Math.max(hi, x);
    }
    waveform.push(Math.max(-1, lo), Math.min(1, hi));
  }
  return {frames, sampleRate : rate, waveform};
}
