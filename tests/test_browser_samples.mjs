import assert from 'node:assert/strict';
import {sampleLanes,sampleTarget,sameSampleTarget,sampleOwner,uploadFolderLabel} from '../src/assets/composer/sample-picker.mjs';
const frame=(mode,focus=0,activeClip=0)=>({document:{clips:[{mode}]},focus,activeClip});
for(const mode of ['single','shared-dual','layers'])assert.deepEqual(sampleLanes(frame(mode)),[0,1,2,3]);
assert.deepEqual(sampleLanes(frame('split',0)),[0,1,2,3]);assert.deepEqual(sampleLanes(frame('split',1)),[0,1,2,3]);
for(const mode of ['quad','sequence'])for(let part=0;part<4;part++){assert.deepEqual(sampleLanes(frame(mode,part)),[0,1,2,3]);for(let lane=0;lane<4;lane++)assert.equal(sampleOwner(frame(mode,part),lane),lane);}
assert.deepEqual(sampleLanes(frame('bank',0)),[0,1,2,3]);assert.deepEqual(sampleLanes(frame('bank',1)),[]);
const f=frame('split',1,7),target=sampleTarget(f);assert.deepEqual(target,{clip:7,part:1,mode:1});assert(sameSampleTarget(target,f));
for(const changed of [frame('split',0,7),frame('split',1,8),frame('shared-dual',1,7),null])assert(!sameSampleTarget(target,changed));
console.log('Browser sample targets: seven modes, part lane ownership, Bank exclusion and frozen destination: PASS');

assert.equal(sampleOwner(frame('split',1),0),0);assert.equal(sampleOwner(frame('split',0),3),1);assert.equal(sampleOwner(frame('shared-dual',1),3),1);assert.equal(sampleOwner(frame('bank',0),3),0);
assert.match(uploadFolderLabel(null),/Browser Samples.*default/);assert.equal(uploadFolderLabel('Drums/Kicks'),'Move Samples / Drums/Kicks');assert.equal(uploadFolderLabel(''),'Move Samples / (root)');
console.log('All four lane buttons, receiving-part ownership and upload folder labels: PASS');

const {newComposition}=await import('../src/assets/composer/format.mjs');
const {describeMovePages}=await import('../src/assets/composer/move-pages.mjs');
const clip=newComposition().clips[0];clip.parts[0].engine='texture';
const source=describeMovePages(clip,0).find(p=>p.title==='TEXTURE SOURCE');
assert(source);assert.deepEqual(source.keys.slice(0,2),['texture_region_mode','texture_regions']);assert.equal(source.cells[0].labels[2],'FILE');assert(source.cells.slice(2).every(c=>c===null));
const liveSource=describeMovePages(clip,0,120,{live_source:1}).find(p=>p.title==='TEXTURE SOURCE');assert(liveSource.cells.slice(0,2).every(c=>c===null));
clip.parts[0].engine='cutups';const clock=describeMovePages(clip,0).find(p=>p.title==='CUTUPS CLOCK');assert.equal(clock.cells[7].labels.length,5);
console.log('Browser engine source pages: File/Edited/Auto choices, Texture page and reserved cells: PASS');
