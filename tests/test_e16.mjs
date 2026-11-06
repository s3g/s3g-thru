import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const thruMidi=await import('../src/midi-map.mjs');
const read=p=>fs.readFileSync(new URL('../'+p,import.meta.url),'utf8');
const registry=JSON.parse(read('controllers/midi/parameters.json'));
const byKey=new Map(registry.parameters.map(p=>[p.key,p]));
// Test the adapter used in the shipped UI, including host output backpressure.
const sandbox={globalThis:{}};vm.createContext(sandbox);
vm.runInContext(read('src/e16-remote.inc.js')+read('src/midi-ui.inc.js')+'\nglobalThis.createFeedback=createNrpnFeedback;',sandbox);
const sent=[];let busy=true;
const feedback=sandbox.globalThis.createFeedback(p=>{if(busy)return false;sent.push(p);return true;});
feedback.update([[5,384,42],[6,1921,8192],[16,2438,16383],[16,2949,1]]);feedback.tick();assert.equal(sent.length,0);
feedback.update([[5,384,100]]);busy=false;feedback.tick();assert.equal(sent.length,3);assert(sent.every(p=>p.length===16));
assert.deepEqual(Array.from(sent[0]),[11,0xb4,99,3,11,0xb4,98,0,11,0xb4,6,0,11,0xb4,38,100]);
feedback.tick();assert.equal(sent.length,4);
feedback.update([[5,384,100]]);feedback.tick();assert.equal(sent.length,4);
feedback.update([[5,384,101],[5,384,100]]);feedback.tick();assert.equal(sent.length,4,'returning to last sent value cancels stale output');
feedback.update([[0,0,1],[17,0,1],[5,16383,1],[5,0,16384],[5,0,NaN],[5,0,0.5],null]);feedback.tick();assert.equal(sent.length,4);
feedback.reset();feedback.update([[5,384,100]]);feedback.tick();assert.equal(sent.length,5);
// A reconnect/pickup resync must survive output backpressure. A later normal
// snapshot can equal the old cache even though the controller never received
// the requested refresh. Active performance must not cancel that pending work.
busy=true;
feedback.update([[5,384,100,1]]);feedback.tick();assert.equal(sent.length,5);
feedback.update([[5,384,100]]);busy=false;feedback.tick();
assert.equal(sent.length,6,'ordinary snapshots must not cancel an unsent urgent resync');
assert.deepEqual(Array.from(sent.at(-1)),Array.from(sent[0]).map((v,i)=>i===15?100:v));
console.log('s3g Thru NRPN 14-bit wire format, coalescing, sync and backpressure: PASS');


const viewContext={globalThis:{},thruMidi,clamp:(v,a,b)=>Math.max(a,Math.min(b,v))};vm.createContext(viewContext);
vm.runInContext(read('src/ui.js').match(/function tapeInertiaSeconds[\s\S]*?\n}/)[0]+read('src/e16-labels.inc.js')+read('src/e16-remote.inc.js')+read('src/midi-ui.inc.js')+';globalThis.view=e16PageView;globalThis.value=e16Value;globalThis.remoteView=remoteE16View;globalThis.jumpView=remotePageJumpView;',viewContext);
const p=key=>byKey.get(key);
const snapshot={v:2,active:1,mode:2,pair:0,page:0,pages:[0,1,2,3,7,8,9,10,11],rows:[1,1],shift:0,shared:0,alg:[0,2,4,4],values:[
 [0,0,p('algorithm').id,0,14,14],[1,8192,p('locus').id,0,0,1],
 [3,3941,p('speed').id,0,0,4],[7,8192,p('part_level').id,0,0,2],
 [8,2340,p('algorithm').id,1,14,14],[9,0,p('locus').id,1,0,1]]};
let view=viewContext.globalThis.view(snapshot);
assert.equal(view.title,'1/12 ENGINE A/B');assert.equal(view.cells[0].text,'MOTN');assert.equal(view.cells[8].text,'GRNS');
assert.equal(view.cells[1].text,'50%');assert.equal(view.cells[7].text,'100%');assert(view.cells[3].text.endsWith('X'));
assert.equal(view.cells[0].family,0);assert.equal(view.cells[8].family,3);assert(view.cells[6].reserved);
view=viewContext.globalThis.view({...snapshot,mode:3,pair:2,alg:[0,2,4,14]});assert.equal(view.title,'1/12 ENGINE C/D');
view=viewContext.globalThis.view({...snapshot,mode:0,values:snapshot.values.slice(0,4)});assert.equal(view.title,'1/12 ENGINE A');assert.equal(view.cells[8],null);
assert.equal(viewContext.globalThis.view({}),null);
assert(/e16Remote|e16_remote/.test(read('src/ui.js')),'Shipped UI contains the Remote transport and renderer');
let remoteView=viewContext.globalThis.remoteView(snapshot);assert.deepEqual(Array.from(remoteView.cells[0].rgb),[28,28,24]);assert.deepEqual(Array.from(remoteView.cells[8].rgb),[22,8,20]);
const jumpView=viewContext.globalThis.jumpView({...snapshot,page:4});
assert.equal(jumpView.title,'PAGE JUMP - PRESS 1-12');assert.equal(jumpView.cells[4].text,'CURRENT');
assert.equal(jumpView.cells[0].label,'ENGINE');assert.equal(jumpView.cells[11].label,'TAPE');
assert.equal(jumpView.cells[15].label,'BACK');assert.equal(jumpView.cells[12],null);
console.log('s3g Thru Remote A/B values, physical units and mode views: PASS');

const encode=(key,value)=>{const x=p(key),ratio=x.curve==='log'?Math.log(value/x.min)/Math.log(x.max/x.min):(value-x.min)/(x.max-x.min);return [x,Math.round(ratio*16383)];};
const cell=(index,key,value,part)=>{const[x,raw]=encode(key,value);return [index,raw,x.id,part,x.integer&&x.max-x.min<=127?x.max-x.min:0,x.max];};
view=viewContext.globalThis.view({...snapshot,page:2,rows:[22,6],values:[cell(0,'formant_pitch',0,0),cell(8,'grain_mode',2,1),cell(9,'grain_size',500,1)]});
assert(view.title.includes('A:FORMANT')&&view.title.includes('B:GRAINS MA'));assert.equal(view.cells[8].text,'CLOUD');assert.equal(view.cells[9].text,'500MS');
view=viewContext.globalThis.view({...snapshot,page:3,rows:[0,7],values:[cell(8,'grain_spray',.5,1)]});assert(view.cells[0].reserved&&view.title.includes('A:---'));
view=viewContext.globalThis.view({...snapshot,page:9,shared:1,rows:[34,0],values:[cell(0,'effect_delay',250,-1),cell(7,'effect_mix',25,-1)]});
assert.equal(view.title,'10/12 DELAY');assert.equal(view.cells[7].text,'25%');assert.equal(view.cells[0].text,'250MS');assert.equal(view.cells[8],null);assert.equal(view.cells[7].family,4);
view=viewContext.globalThis.view({...snapshot,page:7,shift:1,values:[]});assert.equal(view.title,'TURN1 8/12 COLOR');
assert.equal(viewContext.globalThis.value(p('effect_tape_format'),2),'1/2"');
assert.equal(viewContext.globalThis.value(p('effect_tape_inertia'),100),'4S');
assert.equal(viewContext.globalThis.value(p('effect_tape_hiss'),-84),'OFF');
assert.equal(viewContext.globalThis.value(p('effect_reverb_damp'),12000),'12KHZ');
assert.equal(viewContext.globalThis.value(p('pulsar_rate'),.1),'0.1HZ');
assert.equal(viewContext.globalThis.value(p('stochastic_evolution'),1024),'CYC');
assert.equal(viewContext.globalThis.value(p('effect_mix'),25),'25%','Actual percentages must not be multiplied by 100');
const layout=JSON.parse(read('controllers/oxi-e16/adaptive-layout.json'));
for(const x of registry.parameters.filter(x=>layout.meta.some(m=>m.id===x.id)))for(const value of [x.min,x.max]){
 const text=viewContext.globalThis.value(x,value);assert(typeof text==='string'&&text.length>0&&!/NaN|undefined/.test(text),x.key);
}
console.log('s3g Thru Remote page titles, units, blank slots, shared returns and all mapped readouts: PASS');

for(const mode of [0,1,2,3]){
 const v=viewContext.globalThis.view({...snapshot,mode});assert.equal(v.cells[0].family,0);
 if(mode)assert.equal(v.cells[8].family,mode===2?3:1);
}
view=viewContext.globalThis.view({...snapshot,mode:3,pair:2,values:snapshot.values.map(v=>[...v.slice(0,3),v[3]+2,...v.slice(4)])});
assert.equal(view.cells[0].family,2);assert.equal(view.cells[8].family,3);
console.log('Remote E16 mode colors and shared-return family: PASS');
