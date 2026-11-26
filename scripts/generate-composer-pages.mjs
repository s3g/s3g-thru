import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
let source=fs.readFileSync(path.join(root,'src/ui.js'),'utf8');
source=source.replace(/import \* as moveConstants[^;]+;/,'const moveConstants={};').replace(/import \* as moveInputFilter[^;]+;/,'const moveInputFilter={};').replace(/import \* as os[^;]+;/,'const os={};').replace("from './midi-map.mjs'","from '../../midi-map.mjs'").replaceAll('globalThis.','panelRuntime.');
const adapter=`
const panelDefaults=Object.fromEntries(Object.entries(PARAMS).map(([k,p])=>[k,p.value]));
export function describeMovePages(clip,part,tempo=120,settings={}){
 for(const [k,p] of Object.entries(PARAMS))p.value=panelDefaults[k];
 const own=clip.parts[part];
 for(const [key,value] of Object.entries({...clip.parameters,...own.parameters}))if(PARAMS[key])PARAMS[key].value=value;
 state.splitEnabled=!['single','layers'].includes(clip.mode);state.sharedDual=['shared-dual','bank'].includes(clip.mode);state.quadMode=['quad','sequence'].includes(clip.mode);state.bankMode=clip.mode==='bank';state.splitFocus=part;state.view='main';
 state.algorithm=thruMidi.MIDI_PARAMETERS.find(p=>p.key==='algorithm')?.options?.indexOf(own.engine.toUpperCase())??0;
 state.algorithm=['motion','lanes','grains','cutups','sample','slices','glitch','weave','wavesets','prism','morph','texture','formant','stochastic','pulsar','table','scanned','fm','noise','reson','partials','lowform'].indexOf(own.engine);
 sequenceUi.active=clip.mode==='sequence';layersUi.active=clip.mode==='layers';sequenceSoundUi.active=false;sequenceUi.held=!!settings.__sequenceStepActive;
 if(PARAMS.algorithm)PARAMS.algorithm.value=state.algorithm;
 if(PARAMS.root_note)PARAMS.root_note.value=own.root??60;if(PARAMS.scale)PARAMS.scale.value=own.scale??0;if(PARAMS.tempo_bpm)PARAMS.tempo_bpm.value=tempo;
 const track=clip.sequence?.tracks[part];
 if(track){const values={seq_length:track.length,seq_rate:['1/1','1/2','1/4','1/8','1/16','1/16T','1/32','1/32T','1/64'].indexOf(track.rate),seq_rotation:track.rotation??0,seq_direction:['forward','backward','ping-pong','repeat-ends','random'].indexOf(track.direction??'forward'),seq_path:['horizontal','vertical','diagonal'].indexOf(track.path??'horizontal'),seq_active:track.enabled!==false?1:0,seq_velocity:(track.defaults.velocity??100/127)*127,seq_duration_mode:track.defaults.duration.unit==='ms'?1:0,seq_time_ms:track.defaults.duration.value,seq_gate:track.defaults.duration.value,seq_scale:track.scaleEntry?1:0,seq_clip_beats:clip.loop?.beats??4,seq_type:['normal','euclidean','onset'].indexOf(track.generator?.type??'normal')};for(const[k,v]of Object.entries(values))if(PARAMS[k])PARAMS[k].value=v;}
 if(track?.generator){for(let i=1;i<21;i++)if(PARAMS[RECIPE_KEYS[i]])PARAMS[RECIPE_KEYS[i]].value=track.generator.values[i];PARAMS.seq_recipe_shift.value=track.generator.shift??0;}
 for(const [k,v]of Object.entries(settings))if(PARAMS[k])PARAMS[k].value=v;
 if(Array.isArray(settings.__lfos)){const selected=Math.max(0,Math.min(7,settings.__lfoEdit||0));lfoUi.data={selected,values:settings.__lfos,parts:[...Array(clip.parts.length).keys(),4],engines:settings.__lfoEngines||[-1,...clip.parts.map(p=>['motion','lanes','grains','cutups','sample','slices','glitch','weave','wavesets','prism','morph','texture','formant','stochastic','pulsar','table','scanned','fm','noise','reson','partials','lowform'].indexOf(p.engine))],allowed:settings.__lfoAllowed||[0],primary:state.algorithm,motionSound:PARAMS.motion_sound.value};for(let slot=0;slot<8;++slot)LFO_FIELDS.forEach((f,i)=>{PARAMS['lfo'+(slot+1)+'_'+f].value=settings.__lfos[slot][i];});PARAMS.lfo_select.value=selected+1;syncLfoPageKeys();}
 sequenceUi.data={slice:{count:settings.__sliceCount??64,own:!!settings.__sliceExplicit}};
 SEQUENCE_TRIG_PAGE.keys[0]=SEQUENCE_BURST_PAGE.keys[0]=state.algorithm===5?'seq_slice':'seq_pos';
 SEQUENCE_LANE_PAGE.keys[5]=PARAMS.seq_duration_mode.value>=.5?'seq_time_ms':'seq_gate';
 SEQUENCE_TRIG_PAGE.keys[3]=SEQUENCE_BURST_PAGE.keys[3]=PARAMS.seq_duration_mode.value>=.5?'seq_trig_time_ms':'seq_trig_gate';
 const panelKeys=page=>page.lfo||page.lfoSetup?['lfo_select',...page.keys.slice(1)]:page.keys;
 const result=parameterPages().map(page=>({title:page.title,category:pageCategoryForPage(page),global:!!page.global,keys:Array.from({length:8},(_,i)=>panelKeys(page)[i]||null),cells:Array.from({length:8},(_,i)=>{const key=panelKeys(page)[i],p=key&&!inactiveControl(key)&&controlForKey(key);return p?{key:p.key||key,label:p.label||key,shortLabel:p.shortLabel||p.label||key,min:p.min,max:p.max,step:p.step,choices:p.lfoField&&p.values?p.values.map(value=>[value,p.lfoField==='dest'?(value?(LFO_DESTINATIONS[value].page+' / '+LFO_DESTINATIONS[value].name):'OFF'):p.lfoField==='engine'?(value<0?'ANY':ALGORITHMS[value]):p.lfoField==='target'?['A','B','C','D','FX'][value]:String(value)]):null,labels:p.labels||null,unit:p.unit||'',value:p.value}:null;})}));
 return result.filter(p=>p.category>=0);
}
export function moveScale(scale){return {label:SCALE_LABELS[scale]||SCALE_LABELS[0],shortLabel:SCALE_ABBREVIATIONS[scale]||SCALE_ABBREVIATIONS[0],intervals:SCALE_INTERVALS[scale]||SCALE_INTERVALS[0]};}
export function layerEffectName(index){return LAYERS_FX_TITLES[index]||'FX';}
export function modulationInfo(key,part){return modulationForControl(key,part);}
// Name archived Layers routes using the same contextual controls as Move.
export function describeLayersAutomation(layout,index){
 const fx=layout.fx,advanced=layout.advanced;
 let corner=-1,field=-1;if(index>=fx&&index<fx+32){corner=Math.floor((index-fx)/8);field=(index-fx)%8;}else if(index>=advanced&&index<advanced+16){corner=Math.floor((index-advanced)/4);field=8+(index-advanced)%4;}else return null;
 const saved=Array.from({length:12},(_,i)=>PARAMS['layers_fx'+i].value);
 const read=i=>layout.values[i-advanced]??0;
 try{for(let i=0;i<12;++i)PARAMS['layers_fx'+i].value=read(i<8?fx+corner*8+i:advanced+corner*4+i-8);
  const p=layersFxControl('layers_fx'+field),algorithm=Math.round(PARAMS.layers_fx0.value);if(!p)return {corner,field,name:'Unused effect control',inactive:true,min:0,max:1};
  return {corner,field,key:'layers_fx'+field,name:p.shortLabel||p.label,min:p.min,max:p.max,step:p.step,options:p.labels||null,fxName:LAYERS_FX_TITLES[algorithm]||'Effect'};
 }finally{for(let i=0;i<12;++i)PARAMS['layers_fx'+i].value=saved[i];}
}
`;
const out='// Generated from src/ui.js. Menu order, contextual pages and unavailable cells share the Move implementation.\nconst panelRuntime={};\n'+source+'\n'+adapter;
const target=path.join(root,'src/assets/composer/move-pages.mjs');
if(process.argv.includes('--check')){if(!fs.existsSync(target)||fs.readFileSync(target,'utf8')!==out)throw Error('Move panel page adapter is stale');}else fs.writeFileSync(target,out);
console.log('Move panel page adapter: PASS');
