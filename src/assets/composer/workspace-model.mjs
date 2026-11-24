import {POINT_TYPES,noteName} from './format.mjs';
import {PANEL_PARAMETERS} from './catalog.mjs';
import {describeLayersAutomation} from './move-pages.mjs';
export const TYPES=POINT_TYPES;
const byKey=new Map(PANEL_PARAMETERS.map(p=>[p.key,p]));
const domains=[['table-parameter',4200,40],['scanned-parameter',4300,31],['fm-parameter',4400,32],['noise-parameter',4500,88],['reson-parameter',4600,32],['partials-parameter',4800,104],['lowform-parameter',5000,32]];
const byLock=new Map(PANEL_PARAMETERS.filter(p=>p.lock>=0).map(p=>[p.lock,p]));
const laneKeys=['seq_length','seq_rate','seq_rotation','seq_active','seq_mute','seq_velocity','seq_gate','seq_scale','seq_duration_mode','seq_time_ms','seq_direction','seq_path'];
const recipeKeys=['seq_type','seq_recipe_hits','seq_recipe_map','seq_recipe_order','seq_recipe_repeat','seq_recipe_write','seq_recipe_amount','seq_recipe_seed','seq_recipe_regions','seq_recipe_preroll','seq_recipe_from','seq_recipe_to','seq_recipe_pitch','seq_recipe_span','seq_recipe_accents','seq_recipe_depth','seq_recipe_duration','seq_recipe_min','seq_recipe_max','seq_recipe_human','seq_recipe_probability','seq_recipe_shift'];
export function curveMeta(target,context={}){
 if(target.key&&byKey.has(target.key))return byKey.get(target.key);
 if(['parameter','part-parameter'].includes(target.type))return byLock.get(target.index)||null;
 for(const[type,base,count]of domains)if(target.type===type&&target.index<count)return PANEL_PARAMETERS.find(p=>p.id===base+target.index)||null;
 if(target.type==='reson-density')return byKey.get('reson_density');
 if(target.type==='sequence-lane')return byKey.get(laneKeys[target.index]);
 if(target.type==='sequence-recipe')return byKey.get(recipeKeys[target.index]);
 if(target.type==='stack-hold'||target.type==='part-hold')return byKey.get('stack_scan_paused');
 if(target.type==='layers-parameter'||target.type==='layers-baseline'){
  const layout=context.layers;if(!layout)return {name:'Layers · saved control '+(target.index+1),min:0,max:1,integer:false};
  const index=target.owner*192+target.index,fx=describeLayersAutomation(layout,index);
  if(fx){const p=byKey.get(fx.key),names={ALG:'Algorithm',CUT:'Cutoff',RES:'Resonance',DRIV:'Drive',RESP:'Response',SMEA:'Smear',PERS:'Persistence',TONE:'Tone',WIDE:'Width',TIME:'Time',FDBK:'Feedback',PING:'Ping-pong',SPRD:'Spread',MOVE:'Movement',SPRY:'Spray',DRFT:'Drift',HOLD:'Hold',DMG:'Damage',AMNT:'Amount',COLR:'Color',BIAS:'Bias',SIZE:'Size',RATE:'Rate',REPT:'Repeats',DCAY:'Decay',DAMP:'Damping',PRE:'Pre-delay',FMT:'Format',AGE:'Age',MECH:'Mechanics',PITC:'Pitch',FINE:'Fine tune',DEPT:'Depth',GATE:'Gate',PAN:'Pan',LVL:'Level',SOFT:'Softness',TRNS:'Transients',LINK:'Stereo link'};return {...p,...fx,name:names[fx.name]?names[fx.name]+' ('+fx.name+')':fx.name,context:'FX corner '+'ABCD'[fx.corner]+' · '+(fx.fxName||''),integer:p?.integer||false};}
  const setting=index-layout.config,keys={3:'layers_glide',8:'layers_depth',9:'layers_z_mode',10:'layers_z',11:'layers_return'};
  if(keys[setting])return {...byKey.get(keys[setting]),context:'FX layer'};
  if(setting>=4&&setting<=7)return {name:'Corner '+'ABCD'[setting-4]+' weight',context:'FX layer',min:0,max:1,integer:false};
  if(index<layout.advanced){const part=Math.floor(index/layout.fields),p=byLock.get(index%layout.fields);return {...p,name:(p?.name||'Reserved control')+' (inactive)',context:'Legacy corner '+'ABCD'[part],min:p?.min??0,max:p?.max??1,inactive:true};}
  return {name:'Reserved Layers control',context:'FX layer',min:0,max:1,integer:false,inactive:true};
 }
 if(target.type==='pooled-lfo'){const key='lfo'+(target.owner+1)+'_'+['dest','wave','clock','rate','depth','mode','phase','smooth','flow','target','engine'][target.index],p=byKey.get(key);if(p)return {...p,context:'LFO '+(target.owner+1)};}
 const names={'pressure':'Pad pressure','note-pressure':'Note pressure','midi-expression':'Keyboard expression','stack-position':'Stack position','roll-position':'Roll position','roll-lane-position':'Roll lane','stack-path':'Stack path','performance':'Performance gesture','control-lfo':'Legacy LFO setting'};
 if(names[target.type])return {name:names[target.type]};
 return null;
}
export function curveLabel(target,context={}){const meta=curveMeta(target,context),typed=domains.some(d=>d[0]===target.type),part=typed?Math.floor((target.owner||0)/4):target.owner;
 const prefix=meta?.context?meta.context+' · ':Number.isInteger(part)&&part<4?'Part '+'ABCD'[part]+(typed?' · Patch '+((target.owner%4)+1):'')+' · ':'';
 return prefix+(meta?.name||target.key||'Recorded gesture')+' · Take '+((target.take??0)+1);
}
export function curveBounds(curve,context={}){const p=curveMeta(curve.target,context);if(p&&Number.isFinite(p.min)&&Number.isFinite(p.max))return {min:p.min,max:p.max,step:p.integer?1:p.step||Math.max(.00001,(p.max-p.min)/1000),discrete:p.integer,labels:p.options};
 const values=curve.points.map(p=>p[1]);let min=Math.min(0,...values),max=Math.max(1,...values);if(min===max)max=min+1;return {min,max,step:.001,discrete:false};
}
export const storageValue=(meta,value)=>meta?.key==='waveset_process'?[0,1,2,3,4,6,7,8,9,10][Math.round(value)]??0:value;
export const menuValue=(meta,value)=>meta?.key==='waveset_process'?Math.max(0,[0,1,2,3,4,6,7,8,9,10].indexOf(Math.round(value))):value;
export const routeKey=t=>[t.type,t.index??0,t.owner??255].join(':');
export function bindingTarget(tuple){return {type:TYPES[tuple[1]],index:tuple[2],owner:tuple[3],take:0};}
export function activeCurve(curves,target,time,span){const matching=curves.map((c,index)=>({c,index})).filter(x=>routeKey(x.c.target)===routeKey(target));let active=null;
 for(const x of matching){if((x.c.windows||[]).some(w=>((time-w[0]+span)%span)<w[1])&&(!active||(x.c.target.take||0)>=(active.c.target.take||0)))active=x;}
 return active||matching.filter(x=>!x.c.windows?.length).sort((a,b)=>(b.c.target.take||0)-(a.c.target.take||0))[0]||null;
}
export function curveValue(curve,time,span,context={}){if(!curve?.points.length)return null;const bounds=curveBounds(curve,context),meta=curveMeta(curve.target,context),window=curve.windows?.find(w=>((time-w[0]+span)%span)<w[1]);
 if(window){const here=(time-window[0]+span)%span,points=curve.points.map(p=>[(p[0]-window[0]+span)%span,p[1]]).filter(p=>p[0]<=window[1]).sort((a,b)=>a[0]-b[0]);if(!points.length)return null;let a=points[0],b=points.at(-1);for(const p of points){if(p[0]<=here)a=p;else{b=p;break;}}if(bounds.discrete||here<=a[0]||here>=b[0]||a[0]===b[0])return menuValue(meta,here>=b[0]?b[1]:a[1]);return menuValue(meta,a[1]+(b[1]-a[1])*(here-a[0])/(b[0]-a[0]));}
 const points=curve.points;let a=points.at(-1),b=points[0],at=a[0]-span,bt=b[0];for(const p of points){if(p[0]<=time){a=p;at=p[0];}else{b=p;bt=p[0];break;}}if(time>=points.at(-1)[0]){b=points[0];bt=b[0]+span;}if(bounds.discrete||bt===at)return menuValue(meta,a[1]);return menuValue(meta,a[1]+(b[1]-a[1])*Math.max(0,Math.min(1,(time-at)/(bt-at))));
}
// Half-open note intervals, including the tail of the previous loop. Count
// every part together; note-offs at a boundary precede note-ons there.
export const MAX_PIANO_NOTES=8;
export function notePolyphony(notes,span){
 if(!(span>0))return 0;let active=0,peak=0;const edges=[];
 for(const n of notes){const start=((Math.round(n.timeMs)%span)+span)%span,duration=Math.min(span,Math.max(1,Math.round(n.durationMs)));
  if(duration===span){active++;continue;}const end=start+duration;
  if(end>span){active++;edges.push([end-span,-1]);}else edges.push([end,-1]);edges.push([start,1]);
 }
 peak=active;edges.sort((a,b)=>a[0]-b[0]||a[1]-b[1]);for(const[,delta]of edges){active+=delta;peak=Math.max(peak,active);}return peak;
}
export const PIANO_ROW=14,PIANO_HEIGHT=128*PIANO_ROW;
export const pitchY=note=>(127-note)*PIANO_ROW;
export const pitchAt=y=>Math.max(0,Math.min(127,127-Math.floor(y/PIANO_ROW)));
export function snapTime(ms,{unit='ms',snap=0,tempo=120}={}){const tick=Number(snap)>0?(unit==='beats'?60000/tempo*Number(snap):Number(snap)):0;return Math.max(0,Math.round(tick?Math.round(ms/tick)*tick:ms));}
export function notePayload(note,id,deleted=false){return [note.part??0,id,Math.round(note.timeMs),Math.max(1,Math.round(note.durationMs)),note.note,Math.max(1,note.velocity),note.pad??255,deleted?1:0,note.engineEntry??255];}
export function pointPayload(curve,index,id,deleted=false){const t=curve.target,p=curve.points[index];return [0,id,TYPES.indexOf(t.type),t.index??0,t.owner??255,t.take??0,Math.round(p[0]),p[1],deleted?1:0];}
export function displayTime(ms,unit,tempo){return unit==='beats'?Number((ms*tempo/60000).toFixed(4)):Math.round(ms);}
export function readTime(value,unit,tempo){return Math.round(Number(value)*(unit==='beats'?60000/tempo:1));}
export function validPerformance(p){if(!p||!Number.isInteger(p.clip)||!Array.isArray(p.notes)||p.notes.length>1024||!Array.isArray(p.noteIds)||p.noteIds.length!==p.notes.length||!Array.isArray(p.automation)||!Array.isArray(p.pointIds)||p.pointIds.length!==p.automation.length)throw Error('Incomplete clip performance snapshot');
 const ids=new Set;for(const id of p.noteIds){if(!Number.isSafeInteger(id)||id<1||ids.has(id))throw Error('Invalid note identity');ids.add(id);}p.automation.forEach((c,i)=>{if(c.points.length!==p.pointIds[i].length||c.points.some(x=>!Number.isFinite(x[0])||!Number.isFinite(x[1])))throw Error('Incomplete curve snapshot');});return p;
}
export {noteName};
