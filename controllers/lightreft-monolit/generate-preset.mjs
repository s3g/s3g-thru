#!/usr/bin/env node

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

const args = process.argv.slice(2);
const checkOnly = args[0] === '--check';
const validateOnly = args[0] === '--validate';
const [templatePath, outputPath] = checkOnly ? args.slice(1) : args;
if ((!validateOnly && (!templatePath || !outputPath))
    || (validateOnly && !args[1])) {
    console.error('Usage: node generate-preset.mjs [--check] TEMPLATE OUTPUT\n'
        + '       node generate-preset.mjs --validate PRESET');
    process.exit(2);
}

const marker = Buffer.from([0x00, 0x53, 0x00, 0x4c, 0x00, 0x44, 0x00, 0x52]);
const optionCount = 45;
const faderNames = [
    ['A POS', 'A FLD', 'A SPD', 'A LVL', 'B POS', 'B FLD', 'B SPD', 'B LVL'],
    ['A E1.1', 'A E1.2', 'A E1.3', 'A E1.4', 'B E1.1', 'B E1.2', 'B E1.3', 'B E1.4'],
    ['A E1.5', 'A E1.6', 'A E1.7', 'A E1.8', 'B E1.5', 'B E1.6', 'B E1.7', 'B E1.8'],
    ['A E2.1', 'A E2.2', 'A E2.3', 'A E2.4', 'B E2.1', 'B E2.2', 'B E2.3', 'B E2.4'],
    ['A E2.5', 'A E2.6', 'A E2.7', 'A E2.8', 'B E2.5', 'B E2.6', 'B E2.7', 'B E2.8'],
    ['A LVL', 'A DLY', 'A REV', 'X-CURVE', 'B LVL', 'B DLY', 'B REV', 'X-FADE'],
    ['A TONE', 'A DRIVE', 'A BITS', 'A RATE', 'B TONE', 'B DRIVE', 'B BITS', 'B RATE'],
    ['A X-PN', 'A R.RT', 'A R.DP', 'A R.MX', 'B X-PN', 'B R.RT', 'B R.DP', 'B R.MX'],
];
const bankCcs = [
    [17, 18, 19, 23],
    [32, 33, 34, 35],
    [36, 37, 38, 39],
    [40, 41, 42, 43],
    [44, 45, 46, 47],
    [64, 65, 66, 25],
    [68, 69, 70, 71],
    [72, 73, 74, 79],
];

function findRecords(buffer) {
    const records = [];
    for (let offset = 0; (offset = buffer.indexOf(marker, offset)) >= 0;
        offset += marker.length) records.push(offset);
    return records;
}

function writeFixedUtf16BE(buffer, offset, count, value) {
    assert(value.length <= count, `${value} exceeds ${count} characters`);
    buffer.fill(0, offset, offset + count * 2);
    for (let index = 0; index < value.length; ++index) {
        const code = value.charCodeAt(index);
        assert(code >= 32 && code <= 122, `unsupported preset label: ${value}`);
        buffer.writeUInt16BE(code, offset + index * 2);
    }
}

function readFixedUtf16BE(buffer, offset, count) {
    let value = '';
    for (let index = 0; index < count; ++index) {
        const code = buffer.readUInt16BE(offset + index * 2);
        if (!code) break;
        value += String.fromCharCode(code);
    }
    return value;
}

function valueOffset(record, setting) {
    return record + 0x58 + setting * 8;
}

function configuredValues(index) {
    const bank = Math.floor(index / 8);
    const fader = index % 8;
    const half = fader < 4 ? 0 : 1;
    const sharedMixControl=bank===5 && fader%4===3;
    return new Map([
        [0, sharedMixControl?13:half === 0 ? 9 : 10], // A / B or shared
        [1, sharedMixControl && half===1?24:bankCcs[bank][fader % 4]],
        [2, 0],
        [3, 127],
        [6, 3], // MOTION: paired buttons retain Monolit motion operation
        [43, 0], // CC message
    ]);
}

function buildPreset(template) {
    assert.equal(template.length % 200, 0,
        'Monolit preset length must use complete 200-byte transfer chunks');
    assert.equal(template.subarray(0, 8).toString('hex'),
        '0050005200530054', 'missing PRST header');
    const result = Buffer.from(template);
    const records = findRecords(result);
    assert.equal(records.length, 64, 'expected eight banks of eight SLDR records');

    records.forEach((record, index) => {
        const bank = Math.floor(index / 8);
        const fader = index % 8;
        assert.equal(result.readUInt32BE(record + 0x14), bank,
            `record ${index} bank index mismatch`);
        assert.equal(result.readUInt32BE(record + 0x1c), fader,
            `record ${index} fader index mismatch`);
        assert.equal(result.readUInt32BE(record + 0x50), optionCount,
            `record ${index} option count mismatch`);

        configuredValues(index).forEach((value, setting) =>
            result.writeUInt32BE(value >>> 0, valueOffset(record, setting)));

        writeFixedUtf16BE(result, record + 0x20, 10, faderNames[bank][fader]);
        writeFixedUtf16BE(result, record + 0x36, 10, 'MOTION');
    });
    return result;
}

function validatePreset(preset) {
    assert.equal(preset.length % 200, 0,
        'Monolit preset length must use complete 200-byte transfer chunks');
    const records = findRecords(preset);
    assert.equal(records.length, 64, 'expected eight banks of eight SLDR records');
    records.forEach((record, index) => {
        const bank = Math.floor(index / 8);
        const fader = index % 8;
        assert.equal(preset.readUInt32BE(record + 0x14), bank);
        assert.equal(preset.readUInt32BE(record + 0x1c), fader);
        assert.equal(preset.readUInt32BE(record + 0x50), optionCount);
        configuredValues(index).forEach((value, setting) =>
            assert.equal(preset.readUInt32BE(valueOffset(record, setting)), value,
                `bank ${bank + 1} fader ${fader + 1} setting ${setting}`));
        assert.equal(readFixedUtf16BE(preset, record + 0x20, 10),
            faderNames[bank][fader]);
        assert.equal(readFixedUtf16BE(preset, record + 0x36, 10), 'MOTION');
    });
}

if (validateOnly) {
    const preset = fs.readFileSync(args[1]);
    validatePreset(preset);
    console.log(`s3g Thru Monolit preset: PASS (${preset.length} bytes, 64 controls)`);
} else {
  const expected = buildPreset(fs.readFileSync(templatePath));
  validatePreset(expected);
  if (checkOnly) {
    assert(fs.existsSync(outputPath), `missing generated preset: ${outputPath}`);
    assert.deepEqual(fs.readFileSync(outputPath), expected,
        'generated preset is stale or uses another template');
    console.log(`s3g Thru Monolit preset: PASS (${expected.length} bytes, 64 controls)`);
  } else {
    fs.mkdirSync(path.dirname(outputPath), { recursive: true });
    fs.writeFileSync(outputPath, expected);
    console.log(`Generated ${outputPath} (${expected.length} bytes)`);
  }
}
