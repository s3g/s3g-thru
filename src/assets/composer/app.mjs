import {WaveMarkers} from './wave-markers.mjs';
import {SamplePicker} from './sample-picker.mjs';
import {MoveConnection,PANEL_PROTOCOL,PANEL_BUILD} from './protocol.mjs';
import {LiveEdits,validatePanelFrame,panelSequenceValues,TRIG_FIELDS,trigAt,trigLocks,trigValues,lockBinding,lockState,setLocalTrigValue,panelEntryTokens} from './live.mjs';
import {PANEL_PARAMETERS,PARAMETERS} from './catalog.mjs';
import {ClipWorkspace} from './workspace.mjs';
import {StepClock} from './clock.mjs';
import {SampleView,sampleSource} from './sample-view.mjs';
import {LayersView} from './layers-view.mjs';
import {describeMovePages,modulationInfo} from './move-pages.mjs';
const $=id=>document.getElementById(id),meta=new Map(PANEL_PARAMETERS.map(p=>[p.key,p]));
meta.set('root_note',{key:'root_note',scope:'part',name:'Root note',min:24,max:96,initial:60,integer:true});
const sound=new Set(PARAMETERS.map(p=>p.key)),extras=new Set(['root_note','scale','pad_mode','voice_mode','hold_release_ms']);
const groups=[['play','Play / Engine'],['sample','Sample'],['fx','Mix / FX'],['performance','Performance']];
const engines=['motion','lanes','grains','cutups','sample','slices','glitch','weave','wavesets','prism','morph','texture','formant','stochastic','pulsar','table','scanned','fm','noise','reson','partials','lowform'];
let connection,edits,frame,version='',loading=false,linked=false,lastStatus=0,minimumRevision=0n,signature='',generation=0,observedClip=null;
let editNotice='';
let workspaceExclusive=false,activeMenuGroup='panel-play';const menuScroll=new Map();
let selectedStep=-1,selectedChild=0,pendingSelection=null,gridSignature='',cursors=[];
const stepClock=new StepClock();
const workspace=new ClipWorkspace(document.getElementById('clip-workspace'),{frame:()=>frame,linked:()=>linked,hasStep:()=>hasStep(),connection:()=>connection,pending:()=>!!edits?.pending,minimumRevision:()=>minimumRevision,refreshValues:()=>{if(frame){for(const c of controls.values())updateControl(c,valueFor(c.key));enable();}},liveValue:key=>baseValue(key),showEditor:()=>{setPaneView('editor');workspace.setEditorView('piano',true);},editorView:()=>{if(frame){$('lock-toolbar').hidden=!clip().sequence||workspace.editorView!=='sequence';}},revealControl:revealAutomationControl,push:(kind,values,key)=>{edits?.push(kind,values,key);link('Sending…','pending');},message,keysOff:(includeKeys=true)=>{if(!connection?.connected||!frame||!edits)return;for(const[k,v]of edits.queue)if(['touch',...(includeKeys?['key','keysalive']:[])].includes(v.kind))edits.queue.delete(k);const id=++edits.sequence;connection.send('composer_edit',[connection.client,id,'gestureoff',frame.activeClip,frame.focus,includeKeys?1:0].join('|'));},laneDefaults:()=>{clearSelection();draw();},exclusive:value=>{workspaceExclusive=value;enable();},relink:()=>{edits?.invalidate();linked=false;version='';signature='';workspace.version='';enable();}});
const samplePicker=new SamplePicker({frame:()=>frame,linked:()=>linked,connection:()=>connection,pending:()=>!!edits?.pending,recording:()=>workspace.recording,exclusive:value=>{workspaceExclusive=value;enable();},message,relink:()=>{version='';workspace.version='';clearSelection();}});
$('sample-open').onclick=()=>samplePicker.open();
const sampleRoot=nodeSampleRoot();
function nodeSampleRoot(){const overview=document.createElement('div');overview.id='source-overview';const e=document.createElement('section');e.id='sample-editor';overview.append(e);document.getElementById('editor-tabs').before(overview);return e;}
const sampleView=new SampleView(sampleRoot,{begin:key=>{if(!frame)return;const p=meta.get(key);if(!hasStep())workspace.beginControl(p);workspace.touch(p,true);editing.add(key);},end:key=>{editing.delete(key);if(frame)workspace.touch(meta.get(key),false);},change:(key,value)=>{send(key,value);updateSampleView();},clear:()=>send('locus',valueFor('locus'),true),select:step=>selectStep(frame.focus,step),markers:()=>waveMarkers.open()});
const waveMarkers=new WaveMarkers({frame:()=>frame,linked:()=>linked,connection:()=>connection,pending:()=>!!edits?.pending,recording:()=>workspace.recording,folder:()=>samplePicker.uploadFolder,exclusive:value=>{workspaceExclusive=value;enable();},message,relink:()=>{version='';workspace.version='';clearSelection();}});
const layersRoot=document.createElement('section');layersRoot.id='layers-browser';sampleRoot.after(layersRoot);let layersTelemetry=null;
const layersView=new LayersView(layersRoot,{change:(x,y)=>{if(!linked)return;send('layers_fx_x',x);send('layers_fx_y',y);},corner:corner=>{send('layers_corner',corner);activateMenuSection('panel-fx');setPaneView('controls');},live:()=>{workspace.setCursorWriting(false);updateSampleView();}});
function updateLayersView(){if(!frame)return;const active=clip().mode==='layers',current=layersTelemetry?.active?layersTelemetry:null;document.getElementById('source-overview').classList.toggle('has-layers',active);layersView.update({active,cursorWriting:workspace.cursorWriting,reason:!linked?'Not linked':workspaceExclusive?'Working…':'Points at cursor',stamp:editStamp(),enabled:linked&&!workspaceExclusive&&!!connection?.connected&&!workspace.cursorWriting&&!hasStep(),recording:workspace.recording,captureParameters:!!((workspace.captureMask??3)&2),playing:workspace.playing,x:baseValue('layers_fx_x'),y:baseValue('layers_fx_y'),audible:current?.xy,weights:current?.weights,algorithms:current?.algorithms,recorded:current?.recorded,override:current?.override});}
function updateSampleView(){if(!frame)return;updateLayersView();const c=clip(),source=sampleSource(frame),engine=c.parts[frame.focus].engine,live=['grains','weave','cutups','glitch','pulsar','texture'].includes(engine)&&baseValue('live_source')>0,available=[...controls.values()].some(control=>control.key==='locus'&&control.input.dataset.unavailable!=='true'),step=hasStep()?selectedStep:-1,t=step>=0?selectedTrig():null,binding=hasStep()?lockBinding('locus',frame.focus):null,locked=!!t&&lockState(t,selectedChild,binding).own;
 const target=`${'ABCD'[frame.focus]} · LANE ${source.lane+1} · `+(step>=0?`STEP ${step+1}${selectedChild?' / HIT '+selectedChild:''} · POS ${locked?'LOCK':'INHERITED'}`:workspace.cursorWriting?'POS AUTOMATION · @ '+workspace.toDisplay(workspace.cursorMs)+' '+workspace.unit():'POS / FIELD · LIVE');
 const markers=c.sequence?.tracks[frame.focus].trigs.filter(t=>t.enabled&&t.step!==step).map(t=>({step:t.step,position:t.locks?.position??c.sequence.tracks[frame.focus].defaults.position??c.parts[frame.focus].parameters.locus??.5}))||[];
 waveMarkers.observe();
 sampleView.update({...source,fileMarkers:frame.sampleMarkers?.[source.lane]?.file||[],markerEnabled:linked&&!workspaceExclusive&&!live&&!!source.asset?.frames,visible:!!source.asset||(!['table','scanned','fm','noise','reson','partials','lowform'].includes(engine)&&!live),enabled:linked&&!workspaceExclusive&&!!connection?.connected&&available&&!live,reason:live?'Live Memory source — edit its memory window':!available?'POS is unavailable for this engine or editing target':undefined,stamp:editStamp()+':'+(source.asset?.id||'')+':'+(source.asset?.frames||0),pos:valueFor('locus'),field:valueFor('field'),target,locked,markers});
}
const playbackValues=new Map();let watchSignature='';
const pendingValues=new Map();
const controls=new Map(),editing=new Set(),pads=new Map();
const clip=()=>frame.document.clips[0],hasStep=()=>selectedStep>=0&&!!frame?.document.clips[0].sequence;
const editStamp=()=>frame?`${frame.activeClip}:${frame.document.clips[0].mode}:${frame.focus}:${selectedStep}:${selectedChild}:${frame.patches[frame.focus]}:${panelEntryTokens(frame)[frame.focus]}`:'';
const selectedTrig=()=>trigAt(clip(),frame.focus,selectedStep);
const node=(tag,text,cls)=>{const e=document.createElement(tag);if(text!==undefined)e.textContent=text;if(cls)e.className=cls;return e;};
function message(text,error=false){$('panel-message').textContent=text;$('panel-message').dataset.error=String(error);}
function link(text,state='waiting'){$('connection-status').textContent=text;$('connection-status').dataset.state=state;}
function enable(){const disabled=workspaceExclusive||!linked||!connection?.connected;for(const e of document.querySelectorAll('main input,main select,main button,#part,#patch,#play,#stop,#sample-open'))e.disabled=e.dataset.offline==='true'?false:disabled||e.dataset.unavailable==='true';if(frame)updateSampleView();}
function fail(text){linked=false;version='';link('Not linked','error');message(text,true);enable();}
function supported(key){return sound.has(key)||extras.has(key)||!!frame?.settings[frame.focus]&&Object.hasOwn(frame.settings[frame.focus],key)||(frame?.document.clips[0].sequence&&Object.hasOwn(panelSequenceValues(clip(),frame.focus),key))||(hasStep()&&(key in TRIG_FIELDS||key==='seq_child'))||key==='tempo_bpm';}
function baseValue(key){const c=clip(),part=c.parts[frame.focus],p=meta.get(key);
 if(pendingValues.has(key))return pendingValues.get(key);
 if(playbackValues.has(key))return playbackValues.get(key);
 if(key==='tempo_bpm')return frame.document.tempo;
 if(key==='algorithm')return engines.indexOf(part.engine);
 if(key==='root_note')return part.root??60;if(key==='scale')return part.scale??0;
 const seq=panelSequenceValues(c,frame.focus);if(Object.hasOwn(seq,key))return seq[key];
 return (p.scope==='part'?part.parameters:c.parameters)?.[key]??frame.settings[frame.focus]?.[key]??p.initial;
}
function valueFor(key){
 if(hasStep()){
  const t=selectedTrig(),values=trigValues(clip(),frame.focus,selectedStep,selectedChild);if(Object.hasOwn(values,key))return values[key];
  const binding=lockBinding(key,frame.focus),own=trigLocks(t,selectedChild),parent=t.locks||{};
  if(binding?.kind==='s'){const v=own.sound?.[binding.key]??(selectedChild?parent.sound?.[binding.key]:undefined);if(v!==undefined)return v;}
  if(key==='locus')return values.seq_pos;
 }
 const automated=workspace.valueAtControl(meta.get(key));return Number.isFinite(automated)?automated:baseValue(key);
}
function materializedTrig(){const lane=clip().sequence.tracks[frame.focus];let t=lane.trigs.find(t=>t.step===selectedStep);if(!t){t=trigAt(clip(),frame.focus,selectedStep);lane.trigs.push(t);}return t;}
function coreEdit(field,value,clear=false){if(!linked||!hasStep())return;
 const child=selectedChild,part=frame.focus,step=selectedStep;
 edits.push('q',[part,step,child,field,value,clear?1:0],`q:${part}:${step}:${child}:${field}`);
 setLocalTrigValue(materializedTrig(),child,field,value,clear);link('Sending…','pending');
}
function send(key,value,clear=false){if(!linked||!Number.isFinite(value))return;const p=meta.get(key);if(!p)return;editNotice='';
 if(key==='seq_child'){selectedChild=Math.max(0,Math.min(selectedTrig().burst?.length||1,Math.round(value)));draw();return;}
 const entry=[...controls.values()].find(c=>c.key===key),low=entry?.spec.min??p.min,high=entry?.spec.max??p.max;
 value=Math.max(low,Math.min(high,value));if(p.integer)value=Math.round(value);
 if(!hasStep()&&workspace.writeControl(p,value)){for(const control of controls.values())if(control.key===key)updateControl(control,value,true);updateSampleView();return;}
 if(hasStep()){
  const binding=lockBinding(key,frame.focus);if(!binding)return;
  if(binding.kind==='q')coreEdit(binding.field,value,clear);
  else{
   const part=frame.focus,child=selectedChild,step=selectedStep;
   edits.push('s',[part,step,binding.id,value,clear?1:0,child],`s:${part}:${step}:${child}:${binding.id}`);
   const t=materializedTrig(),locks=child?(t.burst[child-1].locks??={}):(t.locks??={});locks.sound??={};if(clear)delete locks.sound[binding.key];else locks.sound[binding.key]=value;
  }
 }else {
  edits.push(key==='tempo_bpm'?'r':key==='root_note'?'root':'p',['tempo_bpm','root_note'].includes(key)?[frame.focus,value]:[frame.focus,p.id,value],key);
  pendingValues.set(key,value);if(key==='algorithm')clip().parts[frame.focus].engine=engines[value];
 }

 link('Sending…','pending');draw();
 if(['voice_engine_edit','voice_engine_mode'].includes(key)){linked=false;enable();link('Reading engine entry…','pending');}
}
function updateControl(c,value,force=false){
 const preserve=!force&&editing.has(c.key);const v=Number(Number(value).toPrecision(7));
 if(!preserve){if(document.activeElement!==c.input||force)c.input.value=String(v);if(c.slider)c.slider.value=String(v);if(document.activeElement!==c.input)c.input.dataset.editTarget=editStamp();}
 if(hasStep()&&['seq_child','seq_cycle_hit','seq_when'].includes(c.key)){
  const t=selectedTrig();if(c.key==='seq_child')c.spec.max=t.burst?.length||1;
  if(c.key==='seq_cycle_hit')c.spec.max=t.cycle?.every||1;
  if(c.key==='seq_when'){c.spec.min=selectedChild>1?t.burst[selectedChild-2].offset:0;c.spec.max=t.burst?.[selectedChild]?.offset??.999;}
  for(const input of[c.input,c.slider].filter(Boolean)){input.min=c.spec.min;input.max=c.spec.max;}
 }
 const note=c.key==='root_note'?v:c.key==='seq_note'?clip().parts[frame.focus].root+v:null;
 c.output.textContent=note!==null?['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'][((Math.round(note)%12)+12)%12]+(Math.floor(note/12)-1):c.spec.labels?'':c.spec.unit;
 const binding=hasStep()?lockBinding(c.key,frame.focus):null,state=hasStep()?lockState(selectedTrig(),selectedChild,binding):{};
 c.slot.classList.toggle('locked',!!state.own);c.slot.classList.toggle('inherited-lock',!state.own&&!!state.inherited);
 c.clear.hidden=!state.own;c.clear.title='Clear this lock and inherit '+(selectedChild?'the parent trig or lane':'the lane default');
 const unavailable=workspace.cursorWriting&&!hasStep()&&!workspace.binding(meta.get(c.key))||hasStep()&&!binding&&c.key!=='seq_child'||(c.key==='seq_when'&&!selectedChild)||(c.key==='seq_burst'&&!!selectedChild);
 for(const input of [c.input,c.slider,c.clear].filter(Boolean))input.dataset.unavailable=String(unavailable);
 c.slot.classList.toggle('lane-only',hasStep()&&!binding&&c.key!=='seq_child');
 c.slot.title=hasStep()&&!binding&&c.key!=='seq_child'?'Lane setting: choose Lane defaults to edit.':'';
}
function makeControl(cell,index){const slot=node('div',undefined,'control'),p=cell&&meta.get(cell.key);
 if(!p||!supported(cell.key)){slot.classList.add('reserved');slot.append(node('span','□'));slot.title=cell?'Edit this control on Move':'Reserved';return slot;}
 const choices=cell.choices?.length?cell.choices:null;const labels=choices?choices.map(c=>c[1]):cell.labels?.length?cell.labels:p.options?.length?p.options:null;
 const timed=clip().sequence?.tracks[frame.focus].defaults.duration.unit==='ms';
 const spec={min:cell.min??p.min,max:cell.max??p.max,labels,unit:cell.unit||p.unit||(['seq_time_ms','seq_trig_time_ms','seq_recipe_min','seq_recipe_max'].includes(cell.key)&&timed?'ms':['seq_gate','seq_trig_gate'].includes(cell.key)?'steps':'')};
 if(hasStep()){
  if(cell.key==='seq_child')spec.max=selectedTrig().burst?.length||1;
  if(cell.key==='seq_cycle_hit')spec.max=selectedTrig().cycle?.every||1;
 }
 const short=p.short_name&&cell.shortLabel?.replaceAll(' ','').toUpperCase()===p.name?.replaceAll(' ','').toUpperCase()?p.short_name:cell.shortLabel?.length>8&&p.short_name?p.short_name:cell.shortLabel||p.short_name||cell.label;
 const label=node('label',(index+1)+' '+short,'control-label'),id='control-'+controls.size,labelId=id+'-label';
 label.id=labelId;label.htmlFor=id;label.title=p.name||cell.label;
 const head=node('div',undefined,'control-heading'),clear=node('button','×','clear-lock');clear.type='button';clear.hidden=true;clear.setAttribute('aria-label','Clear '+(p.name||cell.label)+' lock');clear.dataset.clearLock=cell.key;clear.onclick=()=>send(cell.key,valueFor(cell.key),true);head.append(label,clear);slot.append(head);
 const row=node('div',undefined,'value-row'),input=node(labels?'select':'input');input.id=id;input.dataset.parameter=cell.key;input.setAttribute('aria-labelledby',labelId);
 const output=node('output',undefined,'readout');output.htmlFor=id;output.setAttribute('aria-hidden','true');
 if(labels){labels.forEach((label,i)=>{const o=node('option',label);o.value=choices?choices[i][0]:i;input.append(o);});}
 else{input.type='number';input.min=spec.min;input.max=spec.max;input.step=p.integer?1:Math.max(.00001,(spec.max-spec.min)/1000);input.inputMode='decimal';}
 input.onkeydown=e=>{if(e.key==='Enter'&&!e.isComposing&&input.type==='number'){e.preventDefault();input.blur();}};
 input.onchange=()=>{editing.delete(cell.key);if(!input.dataset.editTarget||input.dataset.editTarget===editStamp())send(cell.key,input.type==='number'?input.valueAsNumber:Number(input.value));input.dataset.editTarget=editStamp();};input.onfocus=()=>{workspace.beginControl(p,spec);workspace.touch(p,true);editing.add(cell.key);input.dataset.editTarget=editStamp();};input.onblur=()=>{editing.delete(cell.key);workspace.touch(p,false);};
 row.append(input);slot.append(row);const c={key:cell.key,input,output,spec,slot,clear};
 if(!labels){const slider=node('input');slider.type='range';slider.min=spec.min;slider.max=spec.max;slider.step=input.step;slider.dataset.slider=cell.key;slider.setAttribute('aria-labelledby',labelId);
  slider.oninput=()=>{const value=Number(slider.value);send(cell.key,value);for(const control of controls.values())if(control.key===cell.key)updateControl(control,value,true);};slider.onfocus=()=>workspace.beginControl(p,spec);slider.onpointerdown=()=>{workspace.beginControl(p,spec);editing.add(cell.key);workspace.touch(p,true);};slider.onpointerup=slider.onpointercancel=()=>{editing.delete(cell.key);workspace.touch(p,false);};c.slider=slider;slot.append(slider);}
 const tools=node("div",undefined,"automation-tools");tools.append(output);workspace.armControl(p,tools);if(!hasStep()&&modulationInfo(p.key,frame.focus)){const mod=node("button","LFO","assign-lfo");mod.type="button";mod.title="Assign an LFO to "+p.name;mod.onclick=()=>assignParameterLfo(p);tools.append(mod);}slot.append(tools);controls.set(id,c);return slot;
}
function assignParameterLfo(parameter){
 if(!linked||hasStep()||!frame)return;const target=modulationInfo(parameter.key,frame.focus);if(!target)return;
 const values=frame.settings[frame.focus].__lfos;if(!Array.isArray(values))return;
 const captured={clip:frame.activeClip,part:frame.focus,algorithm:engines.indexOf(clip().parts[frame.focus].engine)};
 const dialog=node('dialog',undefined,'modulation-dialog'),title=node('h3','Assign LFO - '+parameter.name),list=node('div',undefined,'modulation-slots');dialog.append(title,list);
 values.forEach((v,slot)=>{const matching=v[0]===target.dest&&v[9]===target.target,button=node('button','LFO '+(slot+1)+' - '+(matching?'Edit current route':v[0]?'Replace existing route':'Free'));button.type='button';button.onclick=()=>{
  if(frame.activeClip!==captured.clip||frame.focus!==captured.part){dialog.close();message('Editing target changed.');return;}
  if(v[0]&&!matching&&!window.confirm('Replace LFO '+(slot+1)+"'s existing destination?"))return;
  edits.push('mod',[captured.part,slot,parameter.id,v[0],v[9],captured.algorithm],'mod:'+slot);dialog.close();message('LFO '+(slot+1)+' assigned. Set depth and rate on its Performance pages.');
 };list.append(button);});
 const close=node('button','Cancel');close.type='button';close.onclick=()=>dialog.close();dialog.append(close);dialog.onclose=()=>dialog.remove();document.body.append(dialog);dialog.showModal();
}
function clearSelection(){selectedStep=-1;selectedChild=0;pendingSelection=null;editing.clear();}
function selectStep(part,step,toggle=false){if(!linked)return;workspace.setEditorView('sequence');workspace.setCursorWriting(false);
 if(part!==frame.focus){pendingSelection={clip:frame.activeClip,part,step,toggle};edits.push('f',[part,0],'focus');linked=false;enable();link('Selecting lane…','pending');return;}
 selectedStep=step;selectedChild=0;editing.clear();if(toggle)coreEdit(0,selectedTrig().enabled?0:1);else if(!selectedTrig().enabled)coreEdit(0,1);draw();
}
function drawSequence(){const c=clip(),active=!!c.sequence;$('sequence-editor').hidden=$('lock-toolbar').hidden=!active||workspace.editorView!=='sequence';if(!active){pads.clear();gridSignature='';return;}
 const shape=c.sequence.tracks.map(t=>t.length).join(':');
 if(shape!==gridSignature){gridSignature=shape;pads.clear();const grid=$('sequence-grid');grid.replaceChildren();c.sequence.tracks.forEach((lane,part)=>{
  const row=node('div',undefined,'sequence-lane'),heading=node('button','ABCD'[part],'sequence-lane-name');heading.type='button';heading.dataset.part=part;heading.onclick=()=>{if(linked){clearSelection();edits.push('f',[part,0],'focus');linked=false;enable();link('Selecting lane…','pending');}};row.append(heading);
  const cells=node('div',undefined,'sequence-cells');for(let step=0;step<32;step++){const pad=node('button',String(step+1).padStart(2,'0'),'sequence-step');pad.type='button';pad.dataset.lane=part;pad.dataset.step=step;pad.dataset.unavailable=String(step>=lane.length);pad.onclick=()=>selectStep(part,step);pad.oncontextmenu=e=>{e.preventDefault();selectStep(part,step,true);};cells.append(pad);pads.set(part+':'+step,pad);}row.append(cells);grid.append(row);
 });}
 for(const[route,pad]of pads){const[part,step]=route.split(':').map(Number),trig=trigAt(c,part,step),locked=Object.keys(trig.locks||{}).length||trig.burst?.some(e=>Object.keys(e.locks||{}).length);
  pad.classList.toggle('on',!!trig.enabled);pad.classList.toggle('has-locks',!!locked);pad.classList.toggle('selected',part===frame.focus&&step===selectedStep);pad.classList.toggle('playhead',cursors[part]===step);pad.setAttribute('aria-pressed',String(part===frame.focus&&step===selectedStep));pad.setAttribute('aria-label',`Lane ${'ABCD'[part]}, step ${step+1}, ${trig.enabled?'on':'off'}${locked?', locks':''}`);
 }
 $('edit-target').textContent=hasStep()?`LOCKS · ${'ABCD'[frame.focus]} · STEP ${String(selectedStep+1).padStart(2,'0')} · ${selectedChild?'EVENT '+selectedChild:'ALL HITS'}`:`LANE ${'ABCD'[frame.focus]} DEFAULTS`;
 for(const id of ['trig-toggle','trig-audition','trig-clear'])$(id).hidden=!hasStep();
 $('trig-toggle').textContent=hasStep()&&selectedTrig().enabled?'Trig on':'Trig off';$('trig-toggle').setAttribute('aria-pressed',String(hasStep()&&!!selectedTrig().enabled));
 $('trig-audition').dataset.unavailable=String(!!$('play').getAttribute('aria-pressed')&&$('play').getAttribute('aria-pressed')==='true');
}
function draw(){const c=clip(),part=frame.focus,settings={...frame.settings[part],...panelSequenceValues(c,part)};
 let displayed=c;if(hasStep()){
  Object.assign(settings,trigValues(c,part,selectedStep,selectedChild));displayed={...c,parts:c.parts.map((p,i)=>i===part?{...p,parameters:{...p.parameters}}:p)};
  const trig=selectedTrig(),effective={...trig.locks?.sound,...trigLocks(trig,selectedChild).sound};Object.assign(displayed.parts[part].parameters,effective);
 }
 for(const[k,v]of pendingValues)settings[k]=v;
 const pages=describeMovePages(displayed,part,frame.document.tempo,settings).filter(page=>page.cells.some(c=>c&&meta.has(c.key)&&supported(c.key)));
 if(hasStep())pages.sort((a,b)=>{const rank=p=>p.title==='TRIG'?0:p.title==='BURST'?1:p.cells.some(c=>c&&lockBinding(c.key,part))?2:3;return rank(a)-rank(b);});
 const next=JSON.stringify([part,frame.patches,panelEntryTokens(frame)[part],c.mode,hasStep(),pages.map(p=>[p.title,p.category,p.keys,p.cells.map(c=>c&&[c.min,c.max,c.labels,c.choices,c.shortLabel,c.unit])])]);
 if(next!==signature){signature=next;controls.clear();editing.clear();const panels=$('menu-panels'),nav=$('panel-sections');panels.replaceChildren();nav.replaceChildren();
  groups.forEach(([id,name],category)=>{const visible=pages.filter(p=>p.category===category);if(!visible.length)return;const section=node('section',undefined,'menu-group');section.id='panel-'+id;section.append(node('h2',name));
   const grid=node('div',undefined,'page-grid');for(const page of visible){const card=node('section',undefined,'menu-page');card.dataset.page=page.title;card.append(node('h3',(page.global?'':c.mode==='layers'&&['LAYERS','FX CORNER','FX DETAIL','FX TOOLS'].includes(page.title)?'FX / ':'ABCD'[part]+' / ')+page.title));const cells=node('div',undefined,'control-grid');page.cells.forEach((c,i)=>cells.append(makeControl(c,i)));card.append(cells);grid.append(card);}section.append(grid);panels.append(section);const tab=node('button',name);tab.type='button';tab.dataset.menuGroup=section.id;tab.dataset.offline='true';tab.setAttribute('role','tab');tab.setAttribute('aria-controls',section.id);tab.onclick=()=>activateMenuSection(section.id);nav.append(tab);
  });
 }
 activateMenuSection(activeMenuGroup,false);
 const watchIds=[...new Set([...controls.values()].map(c=>meta.get(c.key)?.id).filter(Number.isInteger))],watchKey=frame.activeClip+':'+part+':'+watchIds.join(',');if(watchKey!==watchSignature){watchSignature=watchKey;for(let i=0;i<watchIds.length;i+=18)edits.push('watch',[part,i===0?1:0,...watchIds.slice(i,i+18)],'watch:'+i);}
 for(const c of controls.values())updateControl(c,valueFor(c.key));
 const select=$('part');if(select.options.length!==c.parts.length){select.replaceChildren();c.parts.forEach((_,i)=>{const o=node('option','Part '+'ABCD'[i]);o.value=i;select.append(o);});}select.value=String(part);
 const first=c.mode==='split'?part*2:0,last=c.mode==='split'?first+1:3,patch=$('patch');patch.replaceChildren();for(let i=first;i<=last;i++){const o=node('option','Lane '+(i+1));o.value=i;patch.append(o);}patch.value=String(frame.patches[part]);$('patch-field').hidden=['quad','sequence','bank','layers'].includes(c.mode);
 $('clip-title').textContent='Clip '+(frame.activeClip+1)+(c.name?' · '+c.name:'');$('mode').textContent=c.mode.replaceAll('-',' ').toUpperCase();const entry=frame.settings[part];$('engine-entry').hidden=!entry.voice_engine_mode||c.mode==='sequence';$('engine-entry').textContent='LIST · E'+(entry.voice_engine_edit||1)+'/'+(['quad','sequence'].includes(c.mode)?2:['split','shared-dual','bank'].includes(c.mode)?4:8);$('empty-state').hidden=true;drawSequence();workspace.observe(frame);samplePicker.observe();enable();
}
async function refresh(s){if(!edits||loading||!s.panel||s.panelVersion===version||edits.pending)return;loading=true;const mine=generation;
 try{const next=await(await connection.file(s.panel)).json();if(mine!==generation||next.activeClip!==observedClip||next.client!==connection.client||next.epoch!==connection.epoch)return;validatePanelFrame(next,connection,PANEL_PROTOCOL,PANEL_BUILD);
  if(edits.pending||BigInt(next.revision)<minimumRevision||next.editSequence<edits.acknowledged)return;
  const contextChanged=frame&&(frame.activeClip!==next.activeClip||frame.focus!==next.focus||frame.document.clips[0].mode!==next.document.clips[0].mode);
  const target=pendingSelection;if(contextChanged)clearSelection();
  frame=next;pendingValues.clear();version=s.panelVersion;edits.reset(frame.document,frame.activeClip,frame.patches,panelEntryTokens(frame));linked=true;
  if(target&&target.clip===frame.activeClip&&target.part===frame.focus&&target.step<clip().sequence?.tracks[frame.focus].length){selectedStep=target.step;selectedChild=0;pendingSelection=null;if(target.toggle)coreEdit(0,selectedTrig().enabled?0:1);else if(!selectedTrig().enabled)coreEdit(0,1);}
  if(hasStep()){if(selectedStep>=clip().sequence.tracks[frame.focus].length)clearSelection();else selectedChild=Math.min(selectedChild,selectedTrig().burst?.length||1);}
  draw();link(edits.pending?'Sending…':'Linked',edits.pending?'pending':'linked');message(editNotice|| (workspace.cursorWriting?'Adjust a menu control to place a point at the amber cursor.':clip().sequence&&workspace.editorView==='sequence'?'Select a step for locks; Notes / motion edits recorded notes and automation.':'Place the cursor to write points, or use Live controls to adjust the sound.'),!!editNotice);
 }catch(e){if(mine===generation)fail(e.message);}finally{loading=false;}
}
function onStatus(s){if(s.ok===false){fail(s.message);return;}lastStatus=Date.now();
 if(Number.isInteger(s.activeClip)){
  if(observedClip!==null&&observedClip!==s.activeClip){edits?.invalidate();clearSelection();pendingValues.clear();version='';linked=false;enable();link('Reading clip…');}
  observedClip=s.activeClip;
 }
 if(edits?.acknowledge(s.edits,s.editErrors))minimumRevision=BigInt(s.revision);
 if(s.panelError&&s.panelErrorClip===observedClip){fail('Clip '+(observedClip+1)+': '+s.panelError);return;}
 $('trig-audition').dataset.unavailable=String(!!s.playing);enable();
 $('play').setAttribute('aria-pressed',String(!!s.playing));$('stop').setAttribute('aria-pressed',String(!s.playing));
 playbackValues.clear();if(s.playing&&s.readout&&s.readout.clip===frame?.activeClip&&s.readout.part===frame.focus&&s.readout.patch===frame.patches[frame.focus]&&!hasStep()){for(const[id,value]of s.readout.values||[]){const p=PANEL_PARAMETERS.find(p=>p.id===id);if(p)playbackValues.set(p.key,value);}for(const c of controls.values())updateControl(c,valueFor(c.key));}
 stepClock.ingest(s);layersTelemetry=s.layers;workspace.status(s);samplePicker.status(s);waveMarkers.status(s);updateSampleView();
 if(linked){const pending=edits?.pending||frame.editSequence<edits.acknowledged;link(pending?'Sending…':'Linked',pending?'pending':'linked');}refresh(s);
}
async function connect(){editNotice='';const mine=++generation;linked=false;loading=false;frame=undefined;version=signature=watchSignature='';playbackValues.clear();observedClip=null;layersTelemetry=null;pendingValues.clear();clearSelection();minimumRevision=0n;edits?.close();connection?.close();enable();link('Connecting…');message('Waiting for Thru live state…');
 connection=new MoveConnection(s=>{if(mine===generation)onStatus(s);});
 try{await connection.connect();if(mine!==generation)return;connection.live=true;edits=new LiveEdits(connection,(text,kind)=>{if(kind==='parameter-rejected'){editNotice=text;pendingValues.clear();version='';signature='';workspace.version='';editing.clear();message(text,true);return;}if(["note","point","points","at","phrase","cliptool","loop","name","arm","armall","touch","record","key","keyboardoff","keysalive","gestureoff","watch"].includes(kind)){workspace.version="";message(["note","phrase","loop"].includes(kind)?"Edit was not applied. Check the eight-note overlap limit, clip range, and recording state.":text,true);}else fail(text);});await refresh(await connection.poll());}catch(e){if(mine===generation)fail(e.message);}
}
$('part').onchange=e=>{if(linked){clearSelection();edits.push('f',[Number(e.target.value),0],'focus');linked=false;enable();link('Selecting part…','pending');}};
$('patch').onchange=e=>{if(linked){edits.push('v',[frame.focus,Number(e.target.value)],'patch');linked=false;enable();link('Selecting lane…','pending');}};
$('play').onclick=()=>{if(linked)edits.push('play',[frame.focus,1],'transport');};$('stop').onclick=()=>{if(linked)edits.push('play',[frame.focus,0],'transport');};$('reconnect').onclick=connect;
$('lane-defaults').onclick=()=>{clearSelection();draw();};$('trig-toggle').onclick=()=>{coreEdit(0,selectedTrig().enabled?0:1);draw();};
$('trig-clear').onclick=()=>{coreEdit(12,0);draw();};$('trig-audition').onclick=()=>{if(linked&&hasStep())edits.push('a',[frame.focus,selectedStep,selectedChild],'audition');};
window.addEventListener('keydown',e=>{if(e.key==='Escape'&&hasStep()){clearSelection();draw();}if(e.key==='Delete'&&hasStep()&&!['INPUT','SELECT','TEXTAREA'].includes(document.activeElement.tagName)){e.preventDefault();coreEdit(0,0);draw();}});
window.addEventListener('pointerup',()=>editing.clear());window.addEventListener('pointercancel',()=>editing.clear());window.addEventListener('blur',()=>editing.clear());
const navSize=new ResizeObserver(()=>document.documentElement.style.setProperty('--menu-nav-height',Math.ceil($('panel-sections').getBoundingClientRect().height)+'px'));navSize.observe($('panel-sections'));
let animation;function animate(){const next=stepClock.positions(frame?.activeClip??-1);for(let part=0;part<4;++part)if(next[part]!==cursors[part]){pads.get(part+':'+cursors[part])?.classList.remove('playhead');pads.get(part+':'+next[part])?.classList.add('playhead');}cursors=next;workspace.animate(performance.now());animation=requestAnimationFrame(animate);}animation=requestAnimationFrame(animate);
const watchdog=setInterval(()=>{if(linked&&Date.now()-lastStatus>4000)fail('Live feedback stopped. Check the connection and reconnect.');},1000);
window.addEventListener('pagehide',()=>{cancelAnimationFrame(animation);navSize.disconnect();clearInterval(watchdog);edits?.close();connection?.close();});
if(location.pathname.includes('/api/remote-ui/module-assets/'))connect();else fail('Open this panel from Schwung Manager with s3g Thru takeover running.');

document.getElementById('automation-live').onclick=()=>workspace.setCursorWriting(false);document.getElementById('automation-show').onclick=()=>{setPaneView('editor');workspace.setEditorView('piano',true);};
document.getElementById('automation-cursor').onchange=e=>workspace.setCursor(workspace.fromDisplay(e.target.valueAsNumber));

async function revealAutomationControl(target,parameter){
 const active=frame.activeClip;const wait=async predicate=>{const start=Date.now();while(Date.now()-start<4000){if(frame?.activeClip!==active)throw Error('Clip changed while selecting the control.');if(predicate()&&!edits.pending&&linked)return;await new Promise(r=>setTimeout(r,25));}throw Error('Wait for the control context, then try Find control again.');};
 const typed=['table-parameter','scanned-parameter','fm-parameter','noise-parameter','reson-parameter','partials-parameter','lowform-parameter'].includes(target.type),part=typed?Math.floor(target.owner/4):target.type==='part-parameter'||target.type==='sequence-lane'||target.type==='sequence-recipe'||target.type==='part-hold'||target.type==='reson-density'?target.owner:frame.focus;
 if(part!==frame.focus&&part<clip().parts.length){clearSelection();edits.push('f',[part,0],'focus');link('Selecting control…','pending');await wait(()=>frame.focus===part);}
 if(typed&&frame.patches[part]!==target.owner%4){edits.push('v',[part,target.owner%4],'patch');await wait(()=>frame.patches[part]===target.owner%4);}
 if(Number.isInteger(parameter.corner)&&frame.settings[part].layers_corner!==parameter.corner){edits.push('p',[part,meta.get('layers_corner').id,parameter.corner],'layers_corner');await wait(()=>frame.settings[part].layers_corner===parameter.corner);}
 const control=document.querySelector('[data-parameter="'+parameter.key+'"]');if(control){activateMenuSection(control.closest('.menu-group').id);setPaneView('controls');}return control;
}

function activateMenuSection(id,remember=true){
 const sections=[...document.querySelectorAll('#menu-panels .menu-group')];if(!sections.length)return;const scroller=$('menu-panels'),chosen=sections.find(s=>s.id===id)||sections[0];
 if(remember&&chosen.id!==activeMenuGroup)menuScroll.set(activeMenuGroup,scroller.scrollTop);const changed=activeMenuGroup!==chosen.id;activeMenuGroup=chosen.id;for(const section of sections)section.hidden=section!==chosen;for(const button of $('panel-sections').querySelectorAll('button'))button.setAttribute('aria-selected',String(button.dataset.menuGroup===chosen.id));if(changed)scroller.scrollTop=menuScroll.get(chosen.id)||0;
}
function setPaneView(view){$('composition-layout').dataset.pane=view;for(const id of['editor','controls'])$('pane-'+id).setAttribute('aria-selected',String(id===view));}
$('pane-editor').onclick=()=>setPaneView('editor');$('pane-controls').onclick=()=>setPaneView('controls');
const divider=$('dock-divider'),layout=$('composition-layout');let dockWidth=410;try{dockWidth=Number(localStorage.getItem('s3g-thru-menu-width'))||410;}catch{}
function resizeDock(width,save=false){const maximum=Math.max(340,Math.min(720,layout.clientWidth-500));dockWidth=Math.max(340,Math.min(maximum,width));layout.style.setProperty('--dock-width',dockWidth+'px');divider.setAttribute('aria-valuenow',Math.round(dockWidth));divider.setAttribute('aria-valuemax',maximum);if(save)try{localStorage.setItem('s3g-thru-menu-width',String(dockWidth));}catch{}}
resizeDock(dockWidth);let dockDrag=null;
divider.onpointerdown=e=>{e.preventDefault();dockDrag={pointer:e.pointerId,x:e.clientX,width:dockWidth};divider.dataset.dragging=String(e.pointerId);divider.setPointerCapture(e.pointerId);};
window.addEventListener('pointermove',e=>{if(dockDrag&&e.pointerId===dockDrag.pointer)resizeDock(dockDrag.width+dockDrag.x-e.clientX);},true);
const finishDockResize=e=>{if(!dockDrag||e&&e.pointerId!==dockDrag.pointer)return;dockDrag=null;delete divider.dataset.dragging;resizeDock(dockWidth,true);};window.addEventListener('pointerup',finishDockResize,true);window.addEventListener('pointercancel',finishDockResize,true);window.addEventListener('blur',()=>finishDockResize());
divider.onkeydown=e=>{if(['ArrowLeft','ArrowRight','Home','End'].includes(e.key)){e.preventDefault();resizeDock(e.key==='Home'?340:e.key==='End'?720:dockWidth+(e.key==='ArrowLeft'?20:-20),true);}};
window.addEventListener('resize',()=>{if(innerWidth>980)resizeDock(dockWidth);});
const skipLink=document.querySelector('.skip-link');skipLink.onclick=e=>{e.preventDefault();setPaneView('controls');$('panel-sections').querySelector('button')?.focus();};
