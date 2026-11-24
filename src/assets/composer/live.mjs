import {PANEL_PARAMETERS} from './catalog.mjs';
const PANEL_KEYS=new Map(PANEL_PARAMETERS.map(p=>[p.key,p]));
import {BY_KEY,clone,defaultTrig,validateComposition,DIRECTIONS,PATHS,RATES} from './format.mjs';
const json=x=>JSON.stringify(x),number=x=>{const n=Number(Number(x).toPrecision(7));return x>0&&x<1&&n===1?.9999999:n;};
export function liveDiff(before,after){
 const a=before.clips[0],b=after.clips[0],out=[];
 if(a.id!==b.id||a.mode!==b.mode||json(a.sources)!==json(b.sources))return null;
 const put=(key,kind,values)=>out.push({key,kind,values:values.map(number)});
 if(before.tempo!==after.tempo)put('tempo','r',[0,after.tempo]);
 const parameters=(prior,next,part)=>{for(const key of new Set([...Object.keys(prior||{}),...Object.keys(next||{})])){const p=BY_KEY.get(key),value=next?.[key]??p?.initial;if(p&&value!==(prior?.[key]??p.initial))put('p:'+part+':'+p.id,'p',[part,p.id,value]);}};
 parameters(a.parameters,b.parameters,0);
 b.parts.forEach((p,l)=>{
  const old=a.parts[l];parameters(old.parameters,p.parameters,l);
  for(const [key,id]of[['root',3],['scale',4]])if(p[key]!==old[key]){const meta=PANEL_KEYS.get(key==='root'?'root_note':'scale');if(meta)put('p:'+l+':'+meta.id,'p',[l,meta.id,p[key]]);}
  if(p.engine!==old.engine){const meta=BY_KEY.get('algorithm');put('p:'+l+':algo','p',[l,meta.id,['motion','lanes','grains','cutups','sample','slices','glitch','weave','wavesets','prism','morph','texture','formant','stochastic','pulsar','table','scanned','fm','noise','reson','partials','lowform'].indexOf(p.engine)]);}
 });
 if(b.sequence){for(let p=0;p<4;p++){
  const old=a.sequence.tracks[p],lane=b.sequence.tracks[p];
  const laneValues=t=>[t.length,RATES.indexOf(t.rate),t.rotation??0,DIRECTIONS.indexOf(t.direction??'forward'),PATHS.indexOf(t.path??'horizontal'),t.enabled!==false?1:0,(t.defaults.velocity??100/127)*127,t.defaults.duration.unit==='ms'?1:0,t.defaults.duration.value,t.defaults.position??0,b.parts[p].root??60,b.parts[p].scale??0];
  if(json(laneValues(old))!==json(laneValues(lane)))put('l:'+p,'l',[p,...laneValues(lane)]);
  for(let step=0;step<32;step++){
   const prior=old.trigs.find(t=>t.step===step)||defaultTrig(step),next=lane.trigs.find(t=>t.step===step)||defaultTrig(step);
   const core=t=>{const lock=t.locks||{},dur=lock.duration,burst=t.burst||[{offset:0}],mask=(lock.position!==undefined?1:0)|(lock.pitch!==undefined?2:0)|(lock.velocity!==undefined?4:0);return [t.enabled?1:0,t.microtime??0,t.probability??1,t.cycle?.hit??1,t.cycle?.every??1,mask,lock.position??0,lock.pitch??0,(lock.velocity??100/127)*127,dur?(dur.unit==='ms'?1:0):-1,dur?.value??0,burst.length,...burst.map(e=>e.offset)];};
   if(json(core(prior))!==json(core(next)))put('t:'+p+':'+step,'t',[p,step,...core(next)]);
   for(const key of new Set([...Object.keys(prior.locks?.sound||{}),...Object.keys(next.locks?.sound||{})])){const meta=BY_KEY.get(key),value=next.locks?.sound?.[key];if(meta&&value!==prior.locks?.sound?.[key])put('s:'+p+':'+step+':'+meta.id,'s',[p,step,meta.id,value??meta.initial,value===undefined?1:0]);}
  }
 }}
 return out;
}
export class LiveEdits {
 constructor(connection,onError=()=>{}){this.connection=connection;this.onError=onError;this.sequence=0;this.acknowledged=0;this.queue=new Map();this.inflight=new Map();this.baseline=null;this.active=-1;this.timer=setInterval(()=>this.flush(),16);}
 reset(doc,active,patches=[],entries=[]){this.patches=patches;this.entries=entries;this.baseline=clone(doc);this.active=active;this.queue.clear();this.inflight.clear();}
 update(doc){if(!this.baseline)return;const changes=liveDiff(this.baseline,doc);if(changes===null){this.onError('Source or layout changed. Apply this draft to load it onto Move.');return;}for(const edit of changes)this.queue.set(edit.key,edit);this.baseline=clone(doc);}
 push(kind,values,key=kind){this.queue.set(key,{kind,values});}
 flush(){
  if(!this.connection.connected||this.connection.pending||this.active<0)return;
  const now=Date.now();
  for(const [id,entry]of this.inflight)if(now-entry.at>700){if(++entry.tries>5){this.inflight.delete(id);this.onError('Move did not acknowledge an edit. Reconnect the live panel.');}else{entry.at=now;this.connection.send('composer_edit',entry.packet);}}
  // Limit bursts on the same serialized control path as audio/MIDI.
  let budget=2;
  while(this.queue.size&&this.inflight.size<8&&budget--){const[key,edit]=this.queue.entries().next().value;this.queue.delete(key);const id=++this.sequence,packet=[this.connection.client,id,edit.kind,this.active,...edit.values,...(edit.kind==='p'?[this.patches?.[edit.values[0]]??-1,this.entries?.[edit.values[0]]??-1]:[])].join('|');try{this.connection.send('composer_edit',packet);this.inflight.set(id,{packet,kind:edit.kind,at:now,tries:0});}catch(e){this.onError(e.message);}}
 }
 acknowledge(receipts=[],errors=[]){const reasons=new Map(errors);let changed=false;for(const value of receipts){const id=Math.floor(value/2);if(this.inflight.has(id)){changed=true;this.acknowledged=Math.max(this.acknowledged,id);}const kind=this.inflight.get(id)?.kind;if(this.inflight.delete(id)&&value%2===0){this.queue.clear();const reason=reasons.get(id);this.onError(reason||'Move rejected an edit: its context changed or a recording/file operation is active. Finish the operation, then reconnect.',kind==='p'&&reason?'parameter-rejected':kind);}}return changed;}
 invalidate(){this.queue.clear();this.inflight.clear();this.baseline=null;this.active=-1;}
 get pending(){return this.queue.size>0||this.inflight.size>0;}
 close(){clearInterval(this.timer);}
}

// Live telemetry must pass the same strict schema as the native serializer.
export function validatePanelFrame(frame,connection,protocol,build){
 if(frame.protocol!==protocol||frame.build!==build)throw Error('Live panel version differs from Thru. Restart Thru and hard-reload this tab.');
 if(frame.client!==connection.client||frame.epoch!==connection.epoch)throw Error('Live state belongs to another connection. Reconnect.');
 if(!Number.isSafeInteger(frame.editSequence)||frame.editSequence<0)throw Error('Incomplete live edit acknowledgement.');
 if(!Number.isInteger(frame.activeClip)||frame.activeClip<0||frame.activeClip>63||!/^\d+$/.test(frame.revision))throw Error('Invalid live clip context.');
 validateComposition(frame.document);
 const count=frame.document.clips[0].parts.length;
 if(frame.document.clips.length!==1||!Number.isInteger(frame.focus)||frame.focus<0||frame.focus>=count||!Array.isArray(frame.settings)||frame.settings.length!==count||!Array.isArray(frame.patches)||frame.patches.length!==count||frame.patches.some(p=>!Number.isInteger(p)||p<0||p>3))throw Error('Incomplete live panel state.');
 return frame;
}
const recipeKeys=['seq_recipe_hits','seq_recipe_map','seq_recipe_order','seq_recipe_repeat','seq_recipe_write','seq_recipe_amount','seq_recipe_seed','seq_recipe_regions','seq_recipe_preroll','seq_recipe_from','seq_recipe_to','seq_recipe_pitch','seq_recipe_span','seq_recipe_accents','seq_recipe_depth','seq_recipe_duration','seq_recipe_min','seq_recipe_max','seq_recipe_human','seq_recipe_probability'];
export function panelSequenceValues(clip,part){
 const t=clip.sequence?.tracks[part];if(!t)return {};
 const values={seq_length:t.length,seq_rate:RATES.indexOf(t.rate),seq_rotation:t.rotation??0,seq_direction:DIRECTIONS.indexOf(t.direction??'forward'),seq_path:PATHS.indexOf(t.path??'horizontal'),seq_active:t.enabled!==false?1:0,seq_scale:t.scaleEntry?1:0,seq_velocity:(t.defaults.velocity??100/127)*127,seq_duration_mode:t.defaults.duration.unit==='ms'?1:0,seq_time_ms:t.defaults.duration.value,seq_gate:t.defaults.duration.value,seq_clip_beats:clip.loop?.beats??4,seq_type:['normal','euclidean','onset'].indexOf(t.generator?.type??'normal')};
 if(t.generator){recipeKeys.forEach((key,i)=>values[key]=t.generator.values[i+1]);values.seq_recipe_shift=t.generator.shift??0;}
 return values;
}
// Step edits are addressed individually; a slider never replaces an entire trig.
export const TRIG_FIELDS={seq_pos:1,seq_note:2,seq_vel:3,seq_trig_time_ms:4,seq_trig_gate:5,seq_micro:6,seq_prob:7,seq_cycle_hit:8,seq_cycle_every:9,seq_burst:10,seq_when:11};
export function trigAt(clip,part,step){return clip.sequence?.tracks[part]?.trigs.find(t=>t.step===step)||defaultTrig(step);}
export function trigLocks(trig,child=0){return child?trig.burst?.[child-1]?.locks||{}:trig.locks||{};}
export function trigValues(clip,part,step,child=0){
 const t=clip.sequence.tracks[part],trig=trigAt(clip,part,step),parent=trig.locks||{},own=trigLocks(trig,child),locks=child?{...parent,...own}:own;
 const duration=locks.duration?.unit===t.defaults.duration.unit?locks.duration:t.defaults.duration;
 return {seq_pos:locks.position??t.defaults.position??0,seq_note:locks.pitch??t.defaults.pitch??0,seq_vel:(locks.velocity??t.defaults.velocity??100/127)*127,seq_trig_time_ms:duration.value,seq_trig_gate:duration.value,seq_micro:trig.microtime??0,seq_prob:trig.probability??1,seq_cycle_hit:trig.cycle?.hit??1,seq_cycle_every:trig.cycle?.every??1,seq_burst:trig.burst?.length||1,seq_child:child,seq_when:child?trig.burst[child-1].offset:0};
}
export function lockBinding(key,part){
 const field=key==='locus'?1:TRIG_FIELDS[key];
 if(field!==undefined)return {kind:'q',field,property:[null,'position','pitch','velocity','duration','duration'][field]};
 const alias=key.match(/^split_(level|delay|reverb)_([abcd])$/);
 if(alias){if('abcd'.indexOf(alias[2])!==part)return null;key='part_'+alias[1];}
 const p=PANEL_KEYS.get(key);return p?.lockable?{kind:'s',id:p.id,key:p.key}:null;
}
export function lockState(trig,child,binding){
 if(!binding)return {own:false,inherited:false};
 const test=locks=>binding.kind==='s'?Object.hasOwn(locks.sound||{},binding.key):binding.property?Object.hasOwn(locks,binding.property):false;
 return {own:test(trigLocks(trig,child)),inherited:child>0&&test(trig.locks||{})};
}
export function setLocalTrigValue(trig,child,field,value,clear=false,timed=true){
 const target=child?(trig.burst[child-1].locks??={}):(trig.locks??={});
 if(field>=1&&field<=5){const property=[null,'position','pitch','velocity','duration','duration'][field];if(clear)delete target[property];else target[property]=field>=4?{unit:field===4?'ms':'steps',value}:field===3?value/127:value;return;}
 if(field===0)trig.enabled=!!value;
 if(field===6)trig.microtime=value;if(field===7)trig.probability=value;
 if(field===8||field===9){trig.cycle??={hit:1,every:1};trig.cycle[field===8?'hit':'every']=value;trig.cycle.hit=Math.min(trig.cycle.hit,trig.cycle.every);}
 // Native setCount owns actual event spacing; keep existing child data until readback.
 if(field===11&&child)trig.burst[child-1].offset=value;
 if(field===12){for(const key of ['position','pitch','velocity','duration','sound'])delete target[key];}
}

export function panelEntryTokens(frame){return frame.settings.map(s=>(s.voice_engine_mode?16:0)+(s.voice_engine_edit??1));}
