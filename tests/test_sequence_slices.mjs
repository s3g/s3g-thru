import assert from 'node:assert/strict';
import {sequenceSliceMap,sequenceSliceSelection} from '../src/assets/composer/sequence-slices.mjs';
import {describeMovePages} from '../src/assets/composer/move-pages.mjs';
import {newComposition,clone,validateComposition} from '../src/assets/composer/format.mjs';
import {lockBinding,lockState,liveDiff,LiveEdits} from '../src/assets/composer/live.mjs';
const document=newComposition(),clip=document.clips[0];
clip.parts.forEach(p=>p.engine='slices');
const rows=[{position:0,label:'Kick'},{position:.1,label:'Hat'},{position:.37,label:'Snare'},{position:.83,label:'Tail'}];
const frame={focus:0,document,sampleMarkers:Array.from({length:4},()=>({file:clone(rows),edited:[],transients:[]}))};
for(let part=0;part<4;part++){
 const p=clip.parts[part];p.parameters={locus:.5,field:1,slice_region_mode:2,slice_regions:16};
 const lane=clip.sequence.tracks[part];lane.trigs=[{step:0,enabled:true,locks:{sound:{seq_slice:3}},burst:[{offset:0},{offset:.5,locks:{sound:{seq_slice:4}}}]}];
 const selected=sequenceSliceSelection(frame,part,0);
 assert.equal(selected.count,4);assert.equal(selected.number,3);assert.equal(selected.start,.37);assert.equal(selected.end,.83);assert.equal(selected.label,'Snare');assert(selected.own);
 assert.equal(sequenceSliceSelection(frame,part,0,1).number,3);assert(sequenceSliceSelection(frame,part,0,1).inherited);
 assert.equal(sequenceSliceSelection(frame,part,0,2).start,.83);assert.equal(sequenceSliceSelection(frame,part,0,2).end,1);
 const pages=describeMovePages(clip,part,120,{__sliceCount:4,__sequenceStepActive:true,seq_slice:3});
 for(const title of ['TRIG','BURST']){const cell=pages.find(p=>p.title===title).cells[0];assert.equal(cell.key,'seq_slice');assert.equal(cell.shortLabel,'SLIC');assert.equal(cell.max,4);}
 const binding=lockBinding('seq_slice',part);assert.equal(binding.kind,'s');assert.equal(binding.id,3913);assert(lockState(lane.trigs[0],2,binding).own);
 lane.trigs[0].locks.sound.seq_slice=12;assert(!sequenceSliceSelection(frame,part,0).valid);assert.equal(sequenceSliceSelection(frame,part,0).number,12);
 delete lane.trigs[0].locks.sound.seq_slice;assert.equal(sequenceSliceSelection(frame,part,0).number,1);
}
validateComposition(document);
const p=clip.parts[0];p.parameters.slice_region_mode=3;frame.sampleMarkers[0].edited=[{position:0,label:'Edit'},{position:.6,label:'New'}];
clip.sequence.tracks[0].trigs[0].locks.sound.slice_region_mode=2;assert.equal(sequenceSliceSelection(frame,0,0).kind,'file');delete clip.sequence.tracks[0].trigs[0].locks.sound.slice_region_mode;
assert.equal(sequenceSliceMap(frame,0).kind,'edited');p.parameters.slice_region_mode=4;assert.equal(sequenceSliceMap(frame,0).count,2);
p.parameters.slice_region_mode=0;p.parameters.slice_regions=8;p.parameters.field=.4;p.parameters.locus=.5;
assert.equal(sequenceSliceMap(frame,0).count,8);assert(Math.abs(sequenceSliceMap(frame,0).rows[0].position-.3)<1e-6);
assert.equal(sequenceSliceMap(frame,0).end,.7);
p.parameters.slice_region_mode=2;frame.sampleMarkers[0].file=[];assert.equal(sequenceSliceMap(frame,0).count,0);assert(!sequenceSliceSelection(frame,0,0).valid);
p.engine='sample';assert.equal(sequenceSliceMap(frame,0),null);const pages=describeMovePages(clip,0,120,{__sequenceStepActive:true});assert.equal(pages.find(p=>p.title==='TRIG').cells[0].key,'seq_pos');
console.log('Sequence slice browser: actual maps/labels, one-based indices, Burst inheritance, missing addresses, typed portable locks and contextual Move pages: PASS');

const original=newComposition(),updated=clone(original);updated.clips[0].sequence.tracks[0].trigs[0].locks={sound:{seq_slice:3}};updated.clips[0].sequence.tracks[0].trigs[0].burst=[{offset:0,locks:{sound:{seq_slice:4}}}];
const delta=liveDiff(original,updated);assert(delta.some(p=>p.kind==='s'&&p.values[2]===3913&&p.values[3]===3));assert(delta.some(p=>p.kind==='s'&&p.values[2]===3913&&p.values[3]===4&&p.values[5]===1));
console.log('Sequence slice portable/live diffs: independent parent and Burst addresses: PASS');

const errors=[],messages=[],connection={client:'slice-test',connected:true,pending:false,send:(k,v)=>messages.push(v)},edits=new LiveEdits(connection,(message,kind)=>errors.push({message,kind}));
edits.reset(original,0);edits.push('s',[0,0,3913,3,0]);edits.flush();const serial=Number(messages[0].split('|')[1]);edits.acknowledge([serial*2],[[serial,'Slice is not available']]);assert.equal(errors[0].kind,'parameter-rejected');edits.close();
console.log('Sequence slice stale-map rejection: native reason retained without treating it as a disconnected editor: PASS');
