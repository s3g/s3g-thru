import assert from 'node:assert/strict';
import {normalizedMarkers,markerDefaultName,markerIdentity,selectedMarkerSlice,initialMarkerDraft} from '../src/assets/composer/wave-markers.mjs';
assert.deepEqual(normalizedMarkers([]),[{position:0,label:''}]);
assert.deepEqual(normalizedMarkers([{position:.5,label:'middle'},{position:0,label:'start'},{position:.5,label:'duplicate'},{position:1,label:'end'},{position:-1}]),[{position:0,label:'start'},{position:.5,label:'middle'}]);
assert.equal(normalizedMarkers(Array.from({length:100},(_,i)=>({position:i/100}))).length,64);
assert.equal(markerDefaultName('kick.WAV'),'kick-marked.wav');assert.equal(markerDefaultName('x'.repeat(180)+'.wav').length,180);assert(markerDefaultName('x'.repeat(180)+'.wav').endsWith('.wav'));
const f={activeClip:1,focus:0,patches:[0],waveVersion:3,document:{clips:[{sources:[{asset:'file'}]}],assets:[{id:'file',frames:100}]}};
const identity=markerIdentity(f);assert.notEqual(identity,markerIdentity({...f,waveVersion:4}));assert.notEqual(identity,markerIdentity({...f,activeClip:2}));assert.equal(identity,markerIdentity(structuredClone(f)));
console.log('WAV marker draft model: fixed zero, ordering/dedup, 64 slice limit, readable copy names and source identity: PASS');

assert.equal(markerDefaultName('kick-marked.wav'),'kick-marked.wav');assert.equal(markerDefaultName('kick-marked-2.wav'),'kick-marked.wav');

const slices=[{position:0},{position:.2},{position:.8}];
assert.deepEqual(selectedMarkerSlice(slices,0),{start:0,end:.2});assert.deepEqual(selectedMarkerSlice(slices,1),{start:.2,end:.8});assert.deepEqual(selectedMarkerSlice(slices,2),{start:.8,end:1});
for(const index of [-1,3,1.5,NaN])assert.equal(selectedMarkerSlice(slices,index),null);
assert.equal(selectedMarkerSlice([{position:.2},{position:.2}],0),null);assert.deepEqual(selectedMarkerSlice([{position:0}],0),{start:0,end:1});
console.log('Selected draft slice audition: first/middle/final boundaries, whole-file default and invalid selection guards: PASS');

const files=[{position:0,label:'Start'},{position:.6,label:'Saved cue'}],edit=[{position:0},{position:.2}],transients=[{position:0},{position:.35},{position:.8}];
for(const maps of [{file:files,edited:edit,transients},{file:files},{file:files,edited:[]}]){const d=initialMarkerDraft(maps);assert.equal(d.source,'file');assert.deepEqual(d.points,files);}
assert.deepEqual(initialMarkerDraft({file:[],edited:edit,transients}),{source:'edited',points:[{position:0,label:''},{position:.2,label:''}]});
assert.deepEqual(initialMarkerDraft({file:[],edited:[],transients}),{source:'transients',points:transients.map(p=>({...p,label:''}))});
assert.deepEqual(initialMarkerDraft({}),{source:'file',points:[{position:0,label:''}]});assert.deepEqual(files,[{position:0,label:'Start'},{position:.6,label:'Saved cue'}]);
console.log('Marker popup initial source: saved file markers preferred, matching Edited/Transient fallbacks, empty maps and immutable originals: PASS');
