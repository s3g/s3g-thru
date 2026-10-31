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
    'index.html', 'interface.html', 'playing.html', 'recording.html', 'engines.html',
    'modes.html', 'split.html', 'shared-dual.html', 'quad.html',
    'engine-motion.html', 'engine-lanes.html', 'engine-grains.html',
    'engine-cutups.html', 'engine-sample.html', 'engine-slices.html',
    'engine-glitch.html', 'engine-weave.html', 'engine-wavesets.html',
    'engine-prism.html','engine-morph.html','engine-texture.html','engine-formant.html',
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

const footerLinks = new Map();
for (const file of htmlFiles) {
    const source = fs.readFileSync(file, 'utf8');
    const navs = [...source.matchAll(/<nav class="page-nav"[^>]*>([\s\S]*?)<\/nav>/g)];
    if (navs.length !== 1) throw new Error(`Expected one Previous/Next footer in ${path.basename(file)}`);
    const links = [...navs[0][1].matchAll(/<a\b([^>]*)>/g)].map((match) => {
        const attrs = Object.fromEntries([...match[1].matchAll(/([\w-]+)="([^"]*)"/g)]
            .map((attribute) => [attribute[1], attribute[2]]));
        return { href: attrs.href, next: (attrs.class || '').split(/\s+/).includes('next') };
    });
    const next = links.filter((link) => link.next);
    const previous = links.filter((link) => !link.next);
    if (next.length !== 1 || previous.length > 1) {
        throw new Error(`Invalid Previous/Next links in ${path.basename(file)}`);
    }
    footerLinks.set(path.basename(file), { next: next[0].href, previous: previous[0]?.href });
}
const visited = new Set();
let chapter = 'index.html';
let previous;
while (true) {
    if (visited.has(chapter)) throw new Error(`Previous/Next loop repeats ${chapter} before returning to Start`);
    const links = footerLinks.get(chapter);
    if (!links) throw new Error(`Previous/Next points outside the manual: ${chapter}`);
    if (links.previous !== previous) {
        throw new Error(`Previous link in ${chapter} must point to ${previous || 'no page (Start)'}`);
    }
    visited.add(chapter);
    if (links.next === 'index.html') break;
    previous = chapter;
    chapter = links.next;
}
const unreachable = [...footerLinks.keys()].filter((name) => !visited.has(name));
if (unreachable.length) throw new Error('Pages missing from Previous/Next sequence: ' + unreachable.join(', '));
console.log(`s3g Thru Previous/Next navigation: PASS (${visited.size} chapters)`);

console.log(`s3g Thru documentation links: PASS (${htmlFiles.length} pages)`);
