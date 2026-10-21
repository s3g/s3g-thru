import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(scriptDir, '..');
const moduleMetadata = readJson('src/module.json');
const packageMetadata = readJson('package.json');
const releaseMetadata = readJson('release.json');

const version = moduleMetadata.version;
const expectedTag = `v${version}`;
const suppliedTag = process.argv[2] || expectedTag;
const assetName = 's3g-thru-module.tar.gz';
const expectedUrl = `https://github.com/s3g/s3g-thru/releases/download/${expectedTag}/${assetName}`;
const releaseNotes = path.join(
    repoDir, 'release-notes', `RELEASE_NOTES_${version}.md`);

assert(moduleMetadata.id === 's3g-thru',
    'src/module.json must keep the module id s3g-thru');
assert(/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(version),
    `src/module.json has an invalid semantic version: ${version}`);
assert(packageMetadata.version === version,
    `package.json version ${packageMetadata.version} does not match module version ${version}`);
assert(releaseMetadata.version === version,
    `release.json version ${releaseMetadata.version} does not match module version ${version}`);
assert(suppliedTag === expectedTag,
    `release tag ${suppliedTag} does not match module version ${version}; expected ${expectedTag}`);
assert(releaseMetadata.download_url === expectedUrl,
    `release.json download_url must be ${expectedUrl}`);
assert(fs.existsSync(releaseNotes),
    `missing release notes: ${path.relative(repoDir, releaseNotes)}`);

console.log(`s3g Thru release metadata: PASS (${expectedTag})`);

function readJson(relativePath) {
    const absolutePath = path.join(repoDir, relativePath);
    try {
        return JSON.parse(fs.readFileSync(absolutePath, 'utf8'));
    } catch (error) {
        throw new Error(`could not read ${relativePath}: ${error.message}`);
    }
}

function assert(condition, message) {
    if (!condition) throw new Error(message);
}
