import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const filename = process.argv[2] || path.join(root, 'src', 'help.json');

// Printable-ASCII advance widths measured from Schwung's device font atlas:
// https://github.com/charlesvestal/schwung/blob/main/tools/param-pages/font5x7.json
// Includes its one-pixel character spacing. Help text starts at x=4 and does
// not wrap. List titles have a smaller allowance for the selection gutter.
const advances = [
    6, 2, 4, 6, 6, 6, 6, 3, 4, 4, 6, 6, 3, 6, 3, 6,
    6, 4, 6, 6, 6, 6, 6, 6, 6, 6, 3, 3, 5, 6, 5, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6, 4, 6, 6,
    4, 6, 6, 5, 6, 6, 6, 6, 6, 4, 5, 5, 4, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 2, 4, 6,
];
let topics = 0;
let lines = 0;
let widestLine = 0;

function checkText(text, budget, location) {
    if (typeof text !== 'string' || /[^\x20-\x7e]/.test(text)) {
        throw new Error(`${location}: expected printable ASCII text`);
    }
    const width = [...text].reduce((sum, char) => sum + advances[char.charCodeAt(0) - 32], 0);
    if (width > budget) {
        throw new Error(`${location}: ${width}px exceeds ${budget}px: ${JSON.stringify(text)}`);
    }
    return width;
}

function checkNode(node, location) {
    if (!node || typeof node !== 'object' || Array.isArray(node)) {
        throw new Error(`${location}: expected a topic object`);
    }
    checkText(node.title, 110, `${location} title`);
    if (!node.title.trim()) throw new Error(`${location}: empty title`);
    const branch = Object.hasOwn(node, 'children');
    const leaf = Object.hasOwn(node, 'lines');
    if (branch === leaf) throw new Error(`${location}: expected either children or lines`);
    if (branch) {
        if (!Array.isArray(node.children) || !node.children.length) {
            throw new Error(`${location}: children must be a non-empty array`);
        }
        const titles = new Set();
        for (const child of node.children) {
            if (titles.has(child?.title)) throw new Error(`${location}: duplicate topic title`);
            titles.add(child?.title);
            checkNode(child, `${location} / ${child?.title || '?'}`);
        }
    } else {
        if (!Array.isArray(node.lines) || !node.lines.length) {
            throw new Error(`${location}: lines must be a non-empty array`);
        }
        for (const [index, line] of node.lines.entries()) {
            widestLine = Math.max(widestLine, checkText(line, 124, `${location} line ${index + 1}`));
            lines++;
        }
        topics++;
    }
}

try {
    const help = JSON.parse(fs.readFileSync(filename, 'utf8'));
    if (!Array.isArray(help.children) || !help.children.length) {
        throw new Error('top-level children must be a non-empty array for Schwung discovery');
    }
    if (help.title !== 's3g Thru') throw new Error('expected the title s3g Thru');
    checkNode(help, 's3g Thru');
    console.log(`s3g Thru device help: PASS (${topics} topics, ${lines} lines, widest ${widestLine}/124px)`);
} catch (error) {
    console.error(`s3g Thru device help: FAIL (${filename}): ${error.message}`);
    process.exitCode = 1;
}
