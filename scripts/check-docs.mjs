import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const docsRoot = path.join(repoRoot, 'docs');
const htmlFiles = fs.readdirSync(docsRoot)
    .filter((name) => name.endsWith('.html'))
    .map((name) => path.join(docsRoot, name));

if (htmlFiles.length < 6) {
    throw new Error('The manual must include the six core HTML pages.');
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
