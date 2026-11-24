import {ClipWorkspace} from '../src/assets/composer/workspace.mjs';
import assert from 'node:assert/strict';
import {StepClock,ClipClock} from '../src/assets/composer/clock.mjs';
import {curveMeta,curveBounds,notePayload,pointPayload,snapTime,displayTime,readTime,validPerformance,curveLabel,bindingTarget,activeCurve,curveValue,pitchY,pitchAt,pianoLayout,PIANO_HEIGHT,PIANO_ROW,notePolyphony,MAX_PIANO_NOTES} from '../src/assets/composer/workspace-model.mjs';
import {newComposition,clone,validateComposition} from '../src/assets/composer/format.mjs';
import {buildPack,openPack} from '../src/assets/composer/pack.mjs';
let now=0;const clock=new StepClock(()=>now),route=Array.from({length:16},(_,i)=>i%9);
const status=phase=>({epoch:'move',emittedAt:now,sequenceClock:{at:now,clip:0,launch:1,running:true,lanes:Array.from({length:4},()=>({phase,stepMs:62.5,cells:Array.from({length:16},(_,i)=>(Math.floor(phase)+i)%9)}))}});
const seen=[];for(let frame=0;frame<120;++frame){now=frame*1000/60;if(frame%6===0)clock.ingest(status(now/62.5));const cell=clock.positions(0)[0];if(seen.at(-1)!==cell)seen.push(cell);}assert(seen.length>30);for(let i=1;i<seen.length;++i)assert.equal(seen[i],(seen[i-1]+1)%9);
// Native-provided route order is respected; the UI never assumes forward motion.
now+=100;const reverse=status(33);reverse.sequenceClock.launch=2;reverse.sequenceClock.lanes.forEach(l=>l.cells=route.map(x=>8-x));clock.ingest(reverse);assert.equal(clock.positions(0)[0],8);now+=64;assert.equal(clock.positions(0)[0],7);
// Stop/clip changes and stale telemetry cannot leave an unrelated pad lit.
const stopped=status(0);stopped.sequenceClock.running=false;clock.ingest(stopped);assert.deepEqual(clock.positions(0),[-1,-1,-1,-1]);assert.deepEqual(clock.positions(1),[-1,-1,-1,-1]);clock.ingest(reverse);now+=1000;assert.deepEqual(clock.positions(0),[-1,-1,-1,-1]);
const speed={target:{type:'parameter',key:'speed',index:2,owner:255,take:0},points:[[0,1],[1000,2]],windows:[[100,300]]};assert.equal(curveMeta(speed.target).key,'speed');assert(curveBounds(speed).max>=4);assert.equal(curveMeta({type:'fm-parameter',index:7,owner:4,take:0}).key,'fm_feedback');
assert.deepEqual(notePayload({part:1,timeMs:10.4,durationMs:200,note:60,velocity:100,pad:255,engineEntry:2},9),[1,9,10,200,60,100,255,0,2]);assert.equal(pointPayload(speed,1,10)[1],10);assert.equal(snapTime(138,{unit:'beats',snap:.25,tempo:120}),125);assert.equal(snapTime(138,{snap:0}),138);assert.equal(displayTime(250,'beats',120),.5);assert.equal(readTime(.5,'beats',120),250);
const doc=newComposition();doc.clips[0].automation=[speed];validateComposition(doc);const packed=await buildPack(doc,new Map());assert.deepEqual((await openPack(await packed.arrayBuffer())).doc.clips[0].automation[0].windows,[[100,300]]);
const bad=clone(doc);bad.clips[0].automation[0].windows=[[100,0]];assert.throws(()=>validateComposition(bad));
const perf={clip:0,notes:[{note:60}],noteIds:[1],automation:[speed],pointIds:[[2,3]],loop:{durationMs:2000}};validPerformance(perf);assert.throws(()=>validPerformance({...perf,noteIds:[1,1]}));assert.throws(()=>validPerformance({...perf,pointIds:[[2]]}));
// Reject delayed older status replies, and re-anchor immediately on rate edits.
now+=10;const updated=status(4.4);updated.sequenceClock.launch=3;updated.sequenceClock.lanes.forEach(l=>l.stepMs=125);clock.ingest(updated);assert.equal(clock.positions(0)[0],4);const late={...updated,emittedAt:updated.emittedAt-10};assert.equal(clock.ingest(late),false);
now+=20;const fast=status(4.7);fast.sequenceClock.launch=3;fast.sequenceClock.lanes.forEach(l=>l.stepMs=31.25);clock.ingest(fast);assert.equal(clock.positions(0)[0],4);now+=12;assert.equal(clock.positions(0)[0],5);
// Window limits apply to the entire clip, matching native storage capacity.
const crowded=clone(doc);crowded.clips[0].automation=Array.from({length:2},()=>({...clone(speed),windows:Array.from({length:65},()=>[100,10])}));assert.throws(()=>validateComposition(crowded),/touch windows/);
// Menu names and stored routes retain part, patch and Layers-corner identity.
assert.equal(curveMeta({type:'parameter',index:0,owner:255}).key,'locus');assert.match(curveLabel({type:'part-parameter',index:2,owner:1,take:0}),/Part B.*Speed/);assert.match(curveLabel({type:'sequence-lane',index:1,owner:2,take:0}),/Sequence Rate/);
const layers={fx:800,advanced:768,config:832,fields:200,values:Array(80).fill(0)};layers.values[800-768+8]=1;const layerMeta=curveMeta({type:'layers-parameter',index:(800+8+1)%192,owner:Math.floor((800+8+1)/192),take:0},{layers});assert.match(layerMeta.context,/FX corner B.*Filter/);assert.equal(layerMeta.name,'Cutoff (CUT)');assert(!curveLabel({type:'layers-parameter',index:8,owner:0,take:0},{layers}).includes('layers-parameter'));
for(let n=0;n<128;++n)assert.equal(pitchAt(pitchY(n)+PIANO_ROW/2),n);assert.equal(PIANO_HEIGHT,1792);
const base={target:{...speed.target,take:0},points:[[0,1],[1000,2]]},touch={target:{...speed.target,take:1},points:[[100,.3],[300,.7]],windows:[[100,300]]};assert.equal(activeCurve([base,touch],speed.target,200,2000).index,1);assert.equal(activeCurve([base,touch],speed.target,800,2000).index,0);assert.equal(curveValue(base,500,2000),1.5);assert.deepEqual(bindingTarget([2,0,2,255]),{type:'parameter',index:2,owner:255,take:0});
now=500;const cc=new ClipClock(()=>now);cc.ingest({epoch:'move',emittedAt:1000,sequenceClock:{at:990,clip:0,launch:1,transport:true,positionMs:300,speed:2}});assert.equal(cc.position(0,2000),320);now+=100;assert.equal(cc.position(0,2000),520);assert.equal(cc.position(1,2000),null);now+=1000;assert.equal(cc.position(0,2000),null);
assert.equal(ClipWorkspace.prototype.pointIndex.call({selectedPoint:0,curve:0,performance:{pointIds:[[0,0]]}}),-1);assert.equal(ClipWorkspace.prototype.pointIndex.call({selectedPoint:42,curve:0,performance:{pointIds:[[42]]}}),0);
console.log('Clip workspace models: ordered native-clock animation at 1/32, stop/stale guards, typed curves, identities, time units and portable touch windows: PASS');

// Count cyclic half-open note intervals across every part, irrespective of pitch.
const chord=(count,start=100,duration=200)=>Array.from({length:count},(_,i)=>({part:i%4,timeMs:start,durationMs:duration,note:60+i,velocity:100}));
assert.equal(MAX_PIANO_NOTES,8);assert.equal(notePolyphony(chord(8),2000),8);assert.equal(notePolyphony(chord(9),2000),9);
assert.equal(notePolyphony([...chord(8),...chord(8,300)],2000),8);
assert.equal(notePolyphony([...chord(8,1900),...chord(1,0,100)],2000),9);
assert.equal(notePolyphony([...chord(8,1900,100),...chord(1,0,100)],2000),8);
assert.equal(notePolyphony(chord(8,177,2000),2000),8);
// Compare to an independent per-millisecond oracle, including full-span notes.
let seed=82;const rand=n=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed%n;};
for(let trial=0;trial<250;++trial){const span=1+rand(40),notes=Array.from({length:rand(25)},()=>({timeMs:rand(span),durationMs:1+rand(span)}));let peak=0;for(let at=0;at<span;++at)peak=Math.max(peak,notes.filter(n=>(at-n.timeMs+span)%span<n.durationMs).length);assert.equal(notePolyphony(notes,span),peak);}
// Refused edits do not send or overwrite the existing note in the browser.
const existing=chord(8),ids=existing.map((_,i)=>i+1),sent=[],messages=[];
const view={performance:{notes:existing,noteIds:ids},span:()=>2000,send:(...x)=>sent.push(x),api:{message:(...x)=>messages.push(x)},drawTimeline:()=>{}};
ClipWorkspace.prototype.editNote.call(view,{...existing[0],note:80},0);assert.equal(sent.length,0);assert.match(messages[0][0],/Eight simultaneous/);assert.equal(existing.length,8);
ClipWorkspace.prototype.editNote.call(view,{...existing[0],note:80},1);assert.equal(sent.length,1);assert.equal(existing[0].note,80);
// Keyboard ownership: typing never deletes notes or toggles transport.
let prevented=0,deleted=0;const keyView={editorView:'piano',performance:{},api:{linked:()=>true},frame:()=>({focus:1}),note:()=>({id:1,note:{}}),currentCurve:()=>null,pointIndex:()=>-1,editNote:()=>deleted++,send:(...x)=>sent.push(x)};
const key=(key,field=null,extra={})=>({key,target:{closest:selector=>selector==='dialog'?null:field},preventDefault:()=>prevented++,...extra});
ClipWorkspace.prototype.editorKey.call(keyView,key('Delete',{}));assert.equal(deleted,0);
ClipWorkspace.prototype.editorKey.call(keyView,key('Backspace'));assert.equal(deleted,1);
ClipWorkspace.prototype.editorKey.call(keyView,key(' ',null,{code:'Space'}));assert.deepEqual(sent.at(-1),['play',[1,1],'transport']);ClipWorkspace.prototype.editorKey.call(keyView,key(' ',null,{code:'Space'}));assert.deepEqual(sent.at(-1),['play',[1,0],'transport']);const prior=sent.length;ClipWorkspace.prototype.editorKey.call(keyView,key(' ',{}, {code:'Space'}));ClipWorkspace.prototype.editorKey.call(keyView,key(' ',null,{repeat:true,code:'Space'}));assert.equal(sent.length,prior);
console.log('Piano roll: eight-note cyclic overlap limit, safe edit rejection and keyboard ownership: PASS');

let pointsDeleted=0;const pointView={...keyView,selectionKind:'point',selectedPoint:42,virtualCurve:null,currentCurve:()=>({points:[[0,1]]}),pointIndex:()=>0,editPoint:(_c,_i,id,deleted)=>{assert.equal(id,42);assert.equal(deleted,true);pointsDeleted++;}};
ClipWorkspace.prototype.editorKey.call(pointView,key('Delete'));assert.equal(pointsDeleted,1);
let applied=0;const inputView={root:{contains:()=>true},$:id=>{assert.equal(id,'event-update');return{disabled:false,click:()=>applied++};}};ClipWorkspace.prototype.editorKey.call(inputView,key('Enter',{id:'event-duration',type:'number'}));assert.equal(applied,1);

// The same scale definitions drive Move and the browser. Legacy notes remain
// editable after changing scale; new notes and drags use scale degrees.
const major=pianoLayout(60,1);assert(major.allowed.has(60));assert(!major.allowed.has(61));assert.equal(major.pitches.length,75);
const dMinor=pianoLayout(62,2,[61]);assert(dMinor.pitches.includes(61));assert(!dMinor.allowed.has(61));assert(dMinor.allowed.has(65));assert(!dMinor.allowed.has(66));
for(let scale=0;scale<25;++scale)for(const root of [0,60,62,127]){const layout=pianoLayout(root,scale);assert(layout.allowed.size>0);assert.equal(layout.height,layout.pitches.length*PIANO_ROW);for(const note of layout.pitches){assert.equal(layout.at(layout.y(note)+PIANO_ROW/2),note);assert(layout.allowed.has(layout.nearest(note)));}}
const both=pianoLayout(60,1,[],[[61,1]]);assert(both.pitches.includes(61));assert(!both.allowed.has(61));
const scaleView={...view,pianoLayout:()=>major};const beforeScale=sent.length;ClipWorkspace.prototype.editNote.call(scaleView,{...existing[0],note:61},0);assert.equal(sent.length,beforeScale);assert.match(messages.at(-1)[0],/root and scale/);
console.log('Piano roll scale rows: 25 shared scales, changed roots, legacy-note access, all-part view and constrained new-note validation: PASS');

// Match Move's centered FIELD bounds and keep selected physical lane identity.
const {sampleWindow,sampleSource,waveformPath}=await import('../src/assets/composer/sample-view.mjs');
assert.deepEqual(sampleWindow(.5,.2),[.4,.6000000000000001]);assert.deepEqual(sampleWindow(0,1),[0,1]);assert.deepEqual(sampleWindow(.99,.2),[.8,1]);
const sourceFrame={focus:1,patches:[0,3],document:{assets:[{id:'a',frames:44100,sampleRate:44100}],clips:[{sources:[{},{},{},{asset:'a'}]}]}};
assert.equal(sampleSource(sourceFrame).lane,3);assert.equal(sampleSource(sourceFrame).asset.frames,44100);assert.equal(sampleSource({...sourceFrame,focus:0}).asset,undefined);
assert.equal(waveformPath([]),'');assert.equal(waveformPath([0,1,0]),'');for(const region of[[0,1],[.9,1],[0,.01]]){const d=waveformPath([-.8,.7,-.2,.4],...region);assert(d.endsWith('Z'));assert(!/NaN|Infinity/.test(d));}
console.log('Browser sample overview: native FIELD boundaries, physical lane identity, cached wave shapes and zoom edges: PASS');

// A time-only drag must preserve legacy pitches; rows are fixed for the gesture.
{
 const dummy=()=>({setPointerCapture(){},focus(){},getBoundingClientRect(){return {left:0,top:0,width:800,height:140};}});
 const elements=Object.fromEntries(['clip-notes','clip-automation','clip-ruler','clip-note-scroll','clip-ruler-scroll','clip-curve-scroll','piano-key-scroll'].map(k=>[k,dummy()]));
 const original={part:0,timeMs:100,durationMs:200,note:61,velocity:100};let committed;
 const view={recording:false,performance:{notes:[{...original}],noteIds:[1]},$:(k)=>elements[k],span:()=>2000,xTime:e=>100+e.clientX,pianoLayout(){return pianoLayout(60,1,this.performance.notes.map(n=>n.note));},drawTimeline(){},drawNoteFields(){},note(){return {id:1,index:0,note:this.performance.notes[0]};},editNote(n){committed=n;},frame:()=>({focus:0}),api:{linked:()=>true}};
 ClipWorkspace.prototype.bindTimeline.call(view);
 const event=(x,y)=>({button:0,pointerId:1,clientX:x,clientY:y,target:{closest:()=>({dataset:{note:'1'}}),dataset:{} }});
 elements['clip-notes'].onpointerdown(event(0,0));
 for(const x of [20,40,80]){elements['clip-notes'].onpointermove(event(x,0));assert.equal(view.performance.notes[0].note,61);}
 elements['clip-notes'].onpointermove(event(80,14));assert.equal(view.performance.notes[0].note,60);
 elements['clip-notes'].onpointermove(event(80,28));assert.equal(view.performance.notes[0].note,59);
 elements['clip-notes'].onpointerup(event(80,28));assert.equal(committed.note,59);assert.equal(committed.timeMs,180);
 // Exact-time edits reject inaudible points before sending; wrapped spans work.
 const curve={target:{type:'parameter',index:2,owner:255,take:1},points:[[900,2]],windows:[[200,400]]};let sent=0;
 const point={performance:{automation:[curve],pointIds:[[1]]},curve:0,span:()=>2000,send(){sent++;},drawAutomation(){},api:{message(){}}};
 assert.equal(ClipWorkspace.prototype.editPoint.call(point,curve,0,1),false);assert.equal(sent,0);
 curve.points[0][0]=300;assert(ClipWorkspace.prototype.editPoint.call(point,curve,0,1));assert.equal(sent,1);
 curve.windows=[[1800,400]];curve.points[0][0]=100;assert(ClipWorkspace.prototype.editPoint.call(point,curve,0,1));assert.equal(sent,2);
 console.log('Browser stability: original pitch/time-only dragging, fixed scale rows and owned exact-time points: PASS');
}
