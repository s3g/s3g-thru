import assert from 'node:assert/strict';
import {newComposition,clone,selectComposition,BY_KEY} from '../src/assets/composer/format.mjs';
import {LiveEdits,liveDiff,validatePanelFrame,panelSequenceValues,panelEntryTokens,trigAt,trigValues,lockBinding,lockState,setLocalTrigValue} from '../src/assets/composer/live.mjs';
import {describeMovePages} from '../src/assets/composer/move-pages.mjs';
const before=newComposition(),after=clone(before);after.clips[0].sequence.tracks[2].trigs[3].enabled=true;after.clips[0].sequence.tracks[2].trigs[3].locks={position:.75,pitch:7,duration:{unit:'ms',value:400}};
after.clips[0].sequence.tracks[2].rate='1/32';
const changes=liveDiff(before,after);assert(changes.some(e=>e.kind==='t'&&e.values[0]===2&&e.values[1]===3));assert(changes.some(e=>e.kind==='l'));
const writes=[],errors=[],connection={client:'browser-test',connected:true,pending:false,send:(key,value)=>writes.push({key,value})};
const edits=new LiveEdits(connection,e=>errors.push(e));edits.reset(before,5);edits.update(after);edits.flush();assert(writes.length>=2);for(const w of writes){assert.equal(w.key,'composer_edit');assert(new TextEncoder().encode(w.value).length<224);assert(w.value.split('|')[3]==='5');}
const receipts=writes.map(w=>Number(w.value.split('|')[1])*2+1);assert(edits.acknowledge(receipts));assert(!edits.pending);assert.equal(errors.length,0);
const changed=clone(after);changed.clips[0].parts[0].parameters.speed=.75;edits.update(changed);edits.flush();const last=writes.at(-1);assert(last.value.includes('|p|5|0|'));assert.equal(edits.acknowledge([Number(last.value.split('|')[1])*2]),true);assert(errors[0].includes('rejected'));edits.close();
// Use the same native page declarations across all engines and layout families.
for(const engine of ['motion','lanes','grains','cutups','sample','slices','glitch','weave','wavesets','prism','morph','texture','formant','stochastic','pulsar','table','scanned','fm','noise','reson','partials','lowform']){
 const c=clone(before.clips[0]);c.parts[0].engine=engine;for(const mode of ['sequence','quad','single','split','shared-dual','layers']){c.mode=mode;if(mode!=='sequence')delete c.sequence;const pages=describeMovePages(c,0);assert(pages.some(p=>p.title==='ENGINE'));if(mode!=='layers')assert(pages.some(p=>p.title==='TAPE'));assert(pages.every(p=>p.keys.length===8&&p.cells.length===8));}
}
const doc=clone(before);doc.clips.push({...clone(doc.clips[0]),id:'other'});doc.chains=[{id:'chain',name:'Chain',end:'loop',entries:[{clip:doc.clips[0].id,repeats:1,enabled:true,fade:{mode:'default'}}]}];assert.equal(selectComposition(doc,[doc.clips[0].id]).clips.length,1);assert.equal(selectComposition(doc,['other']).chains.length,0);assert.throws(()=>selectComposition(doc,['other'],true),/every clip/);
console.log('Live panel: direct bounded edit packets, coalescing/receipts, stale-context recovery, selected imports, and actual Move menu layouts across 22 engines: PASS');

const frame={editSequence:0,protocol:2,build:'test',client:connection.client,epoch:'same',revision:'1',activeClip:0,focus:0,patches:[0,1,2,3],settings:[{},{},{},{}],document:clone(before)};
frame.document.assets=[{id:'sample',name:'sample.wav',frames:128,sampleRate:44100,waveform:[-.5,.5]}];frame.document.clips[0].sources[0].asset='sample';
assert.equal(validatePanelFrame(frame,{...connection,epoch:'same'},2,'test'),frame);
frame.document.assets[0].channels=2;assert.throws(()=>validatePanelFrame(frame,{...connection,epoch:'same'},2,'test'),/unknown field/);delete frame.document.assets[0].channels;
assert.throws(()=>validatePanelFrame({...frame,focus:4},{...connection,epoch:'same'},2,'test'),/Incomplete/);
assert.throws(()=>validatePanelFrame(frame,{...connection,epoch:'other'},2,'test'),/another connection/);
assert.equal(panelSequenceValues(frame.document.clips[0],0).seq_type,0);
frame.document.clips[0].sequence.tracks[0].generator={type:'euclidean',values:Array(21).fill(0),shift:3};frame.document.clips[0].sequence.tracks[0].generator.values[1]=8;
const seq=panelSequenceValues(frame.document.clips[0],0);assert.equal(seq.seq_recipe_hits,8);assert.equal(seq.seq_recipe_shift,3);assert.equal(seq.seq_type,1);
const pages=describeMovePages(frame.document.clips[0],0,120,seq);assert(pages.some(p=>p.title==='EUCLIDEAN'));assert(pages.find(p=>p.title==='SEQ LANE').keys.includes('seq_time_ms'));
console.log('Loaded-source frames, strict version/identity/context validation, sequence type/recipe/duration menu readback: PASS');

const unlinkedWrites=[];const unlinked=new LiveEdits({...connection,send:(...v)=>unlinkedWrites.push(v)});unlinked.push('p',[0,2,1]);unlinked.flush();assert.equal(unlinkedWrites.length,0);unlinked.close();
assert.throws(()=>validatePanelFrame({...frame,editSequence:undefined},{...connection,epoch:'same'},2,'test'),/acknowledgement/);

const stepClip=clone(before.clips[0]),step=stepClip.sequence.tracks[0].trigs[4];step.enabled=true;step.locks.sound={fm_feedback:40};step.burst=[{offset:0},{offset:.5,locks:{sound:{fm_feedback:60},pitch:7}}];
assert.equal(lockBinding('algorithm',0),null);assert.equal(lockBinding('tempo_bpm',0),null);assert.equal(lockBinding('locus',0).field,1);assert.equal(lockBinding('fm_feedback',0).id,BY_KEY.get('fm_feedback').id);
assert.equal(lockBinding('split_level_a',0).key,'part_level');assert.equal(lockBinding('split_level_b',0),null);
assert.deepEqual(lockState(step,2,lockBinding('fm_feedback',0)),{own:true,inherited:true});assert.equal(trigValues(stepClip,0,4,2).seq_note,7);
setLocalTrigValue(step,2,2,0,true);assert.equal(trigValues(stepClip,0,4,2).seq_note,0);assert.equal(trigAt(stepClip,0,4),step);
setLocalTrigValue(step,0,4,2500);assert.equal(trigValues(stepClip,0,4).seq_trig_time_ms,2500);setLocalTrigValue(step,0,12,0);assert.equal(step.enabled,true);assert.equal(step.burst.length,2);assert.equal(step.burst[1].locks.sound.fm_feedback,60);assert.equal(step.locks.duration,undefined);
console.log('Browser lock bindings: lane-only exclusions, canonical part sends, core values, parent/event inheritance and scoped clearing: PASS');

const listDoc=clone(before);listDoc.clips[0].mode='single';delete listDoc.clips[0].sequence;listDoc.clips[0].parts=listDoc.clips[0].parts.slice(0,1);
const listSettings={voice_engine_mode:1,voice_engine_edit:2,voice_engine_assign:0,voice_engine_order:2};const listPages=describeMovePages(listDoc.clips[0],0,120,listSettings);assert(listPages.some(p=>p.title==='VOICE ENGINES'));assert(listPages.some(p=>p.title==='VOICE SETUP'));assert(!describeMovePages(listDoc.clips[0],0,120,{voice_engine_mode:0}).some(p=>p.title==='VOICE ENGINES'));
assert.deepEqual(panelEntryTokens({settings:[{voice_engine_mode:1,voice_engine_edit:2}]}),[18]);
const drops=[],guarded=new LiveEdits({...connection,send:(k,v)=>drops.push(v)});guarded.reset(listDoc,0,[0],[18]);guarded.push('p',[0,4,15]);guarded.flush();assert(drops[0].endsWith('|0|18'));guarded.invalidate();assert(!guarded.pending);assert.equal(guarded.active,-1);guarded.push('p',[0,4,17]);guarded.flush();assert.equal(drops.length,1);guarded.close();
console.log('Engine LIST menu exposure, entry tokens and discarded stale-clip writes: PASS');

// Native setter rejections carry a useful reason without closing the link.
{
 const errors=[],connection={connected:true};const edit=Object.create(LiveEdits.prototype);
 Object.assign(edit,{connection,inflight:new Map([[7,{kind:'p'}]]),queue:new Map([['next',{kind:'p'}]]),acknowledged:0,onError:(...args)=>errors.push(args)});
 assert(edit.acknowledge([14],[[7,'Choose input or an available part without feedback']]));
 assert.deepEqual(errors,[['Choose input or an available part without feedback','parameter-rejected']]);assert.equal(connection.connected,true);assert.equal(edit.pending,false);
 console.log('Browser setter refusal: native error reason, cleared pending edits and retained connection: PASS');
}

// Browser header modes use the same seven ordered identities as Move.
{
 const {MODE_OPTIONS,stepClickValue}=await import('../src/assets/composer/live.mjs');
 assert.deepEqual(MODE_OPTIONS.map(x=>x.value),['single','split','shared-dual','quad','bank','layers','sequence']);
 assert.deepEqual(MODE_OPTIONS.map(x=>x.index),[0,1,2,3,4,5,6]);
 assert.equal(stepClickValue(false,false),true,'An empty step is enabled and selected');
 assert.equal(stepClickValue(false,true),null,'Another lit step is selected without disabling it');
 assert.equal(stepClickValue(true,true),false,'A second click turns the selected trig off');
 assert.equal(stepClickValue(true,false),true,'The next click re-enables it');
 assert.equal(stepClickValue(false,true,true),false,'Right-click toggles directly');
 assert.equal(stepClickValue(false,false,true),true);
 const failures=[],sent=[],wire={client:'mode-test',connected:true,pending:false,send:(key,value)=>sent.push(value)},pending=new LiveEdits(wire,(message,kind)=>failures.push({message,kind}));
 pending.reset(before,0);pending.push('mode',[0,6,0],'mode');pending.flush();const id=Number(sent[0].split('|')[1]);assert(sent[0].endsWith('|mode|0|0|6|0'));
 pending.acknowledge([id*2],[[id,'Stop recording before changing mode']]);assert.equal(failures[0].kind,'parameter-rejected');assert(!pending.pending);pending.close();
 console.log('Browser mode requests and step clicks: native enum order, safe selection, repeat toggles, direct context click and acknowledged refusal: PASS');
}
