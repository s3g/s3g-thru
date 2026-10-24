import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const docsRoot = path.join(repoRoot, 'docs');
const unexpectedRootEntries = fs.readdirSync(docsRoot, { withFileTypes: true })
    .filter((entry) => entry.isDirectory()
        ? entry.name !== 'assets'
        : !entry.name.endsWith('.html'))
    .map((entry) => entry.name);

if (unexpectedRootEntries.length !== 0) {
    throw new Error('The GitHub Pages root may contain only HTML files and assets/: '
        + unexpectedRootEntries.join(', '));
}

const htmlFiles = fs.readdirSync(docsRoot)
    .filter((name) => name.endsWith('.html'))
    .map((name) => path.join(docsRoot, name));

const requiredPages = [
    'index.html', 'interface.html', 'recording.html', 'engines.html',
    'engine-motion.html', 'engine-lanes.html', 'engine-grains.html',
    'engine-cutups.html', 'engine-sample.html', 'engine-slices.html',
    'engine-glitch.html', 'engine-weave.html', 'engine-wavesets.html',
    'effects.html', 'performance.html', 'shortcuts.html',
];
const missingPages = requiredPages.filter((name) =>
    !fs.existsSync(path.join(docsRoot, name)));
if (missingPages.length !== 0) {
    throw new Error('The manual is missing required HTML pages: '
        + missingPages.join(', '));
}

const idsByFile = new Map();
for (const file of htmlFiles) {
    const source = fs.readFileSync(file, 'utf8');
    const ids = new Set([...source.matchAll(/\sid="([^"]+)"/g)]
        .map((match) => match[1]));
    idsByFile.set(path.resolve(file), ids);
    if (!/<title>[^<]+<\/title>/.test(source)) {
        throw new Error(`Missing title: ${path.relative(repoRoot, file)}`);
    }
}

for (const file of htmlFiles) {
    const source = fs.readFileSync(file, 'utf8');
    for (const match of source.matchAll(/\s(?:href|src)="([^"]+)"/g)) {
        const reference = match[1];
        if (/^(?:https?:|mailto:|data:)/.test(reference)) continue;
        const [pathname, fragment] = reference.split('#', 2);
        const target = path.resolve(path.dirname(file), pathname || path.basename(file));
        if (!fs.existsSync(target)) {
            throw new Error(`Broken local reference ${reference} in ${path.relative(repoRoot, file)}`);
        }
        if (fragment && target.endsWith('.html')) {
            const ids = idsByFile.get(target);
            if (!ids || !ids.has(fragment)) {
                throw new Error(`Broken anchor ${reference} in ${path.relative(repoRoot, file)}`);
            }
        }
    }
}

console.log(`s3g Thru documentation links: PASS (${htmlFiles.length} pages)`);
