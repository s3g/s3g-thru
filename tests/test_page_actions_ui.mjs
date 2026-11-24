import assert from 'node:assert/strict';

export function testPageActionsUi({params,blockingWrites,drawWrites}) {
 const ui=globalThis.__testFx.state(),P=globalThis.__testParams,actions=globalThis.__testPageActions,seq=globalThis.__testSequence;
 const oldGet=globalThis.host_module_get_param,oldSet=globalThis.host_module_set_param_blocking;
 const cc=(n,v=127)=>globalThis.onMidiMessageInternal([0xb0,n,v]);
 const context=()=>[ui.splitFocus,ui.lane,ui.algorithm,ui.activeClip].join(',');
 const keys=['amp_attack','amp_decay','amp_sustain','amp_release','root_note','scale','pad_mode','voice_mode','hold_release_ms','pressure_mode'];
 const caps={group:'',ids:'',engine:17,safe:globalThis.__thruMidi.MIDI_PARAMETERS.map(p=>keys.includes(p.key)?'1':'0').join('')};
 let nativeError='',requests=[],rejectAction=false;
 globalThis.host_module_get_param=key=>key==='page_actions_state'?JSON.stringify({...caps,context:context()}):key==='last_error'?nativeError:oldGet(key);
 globalThis.host_module_set_param_blocking=(key,value)=>{
   if(key!=='page_action')return oldSet(key,value);
   if(rejectAction){rejectAction=false;return false;}
   const [op,target,group,payload]=String(value).split('|');
   if(target!==context()){nativeError='Page target changed';return;}
   nativeError='';requests.push({op,group,payload});
   if(op==='copy'){caps.group=group;caps.ids=payload.split(',').map(k=>globalThis.__thruMidi.MIDI_BY_KEY.get(k).id).join(',');}
 };
 const reset=()=>{
   actions.close();globalThis.__testEnginePicker.ui.open=false;globalThis.__testFm.model.startsOpen=false;globalThis.__testReson.model.startsOpen=false;globalThis.__testLowform.model.startsOpen=false;
   Object.assign(ui,{view:'settings',splitEnabled:false,sharedDual:false,quadMode:false,bankMode:false,splitFocus:0,algorithm:17,lane:1,activeClip:0,performanceVisible:false,focusButtonHeld:false,shiftHeld:false,deleteHeld:false,editorReady:true,pageReturn:null,pageDestinationReturn:null});
   P.split_mode.value=0;P.algorithm.value=17;P.voice_engine_mode.value=0;seq.model.active=false;seq.model.held=false;globalThis.__testSequenceSound.model.data=null;globalThis.__testLayers.model.active=false;
   Object.assign(params,{algorithm:'17',record_lane:'1',voice_engine_mode:'0',split_mode:'0',split_state:JSON.stringify({enabled:0,focus:0,blend:.5,parts:[{},{}]}),clip_bank_state:'0,-1,1,0'});
   ui.settingsPage=globalThis.__testSplit.pages().findIndex(p=>p.title==='AMP ENVELOPE');requests=[];nativeError='';
 };
 const select=label=>{
   const i=actions.model.rows.findIndex(r=>r.label===label);assert(i>=0,label);actions.model.index=i;cc(3,0);cc(3);
 };
 try {
   reset();const page=ui.settingsPage,at=blockingWrites.length;cc(3);cc(3);
   assert.equal(ui.view,'pageActions');assert.equal(requests.length,0,'opening and duplicate presses do nothing');
   assert.deepEqual(actions.model.rows.map(r=>r.label),['COPY SETTINGS','RESET THIS PAGE']);
   cc(51);assert.equal(ui.view,'settings');assert.equal(ui.settingsPage,page);
   assert(!blockingWrites.slice(at).some(([key])=>['clip_record','record_gate'].includes(key)));
   cc(49);cc(3);assert.equal(ui.view,'pageActions','Shift+main in a menu never records audio');cc(49,0);cc(51);
   assert.equal(ui.view,'settings');
   cc(3);rejectAction=true;select('COPY SETTINGS');assert.equal(ui.actionMessage,'ACTION NOT CONFIRMED');assert.equal(requests.length,0);
   cc(3);select('COPY SETTINGS');assert.equal(requests.at(-1).op,'copy');
   cc(3);assert(actions.model.rows.some(r=>r.label==='PASTE SETTINGS'));
   select('RESET THIS PAGE');assert(actions.model.confirm);assert.equal(actions.model.index,0);
   cc(3);assert(actions.model.confirm,'duplicate press cannot confirm reset');
   cc(3,0);cc(3);assert.equal(ui.view,'pageActions');assert.equal(actions.model.confirm,null,'Cancel is the default');
   select('RESET THIS PAGE');cc(3,0);cc(14,1);cc(3);
   assert.equal(requests.at(-1).op,'reset');assert.equal(ui.view,'settings');assert.equal(globalThis.__testSplit.pages()[ui.settingsPage].title,'AMP ENVELOPE');
   const commands=requests.length;cc(3);ui.lane=2;cc(3,0);cc(3);
   assert.equal(requests.length,commands,'changing the target cannot execute the old action');cc(51);
   reset();cc(3);ui.view='settings';actions.close();ui.view='help';const before=blockingWrites.length;cc(3);
   assert(!blockingWrites.slice(before).some(([key])=>['clip_record','record_gate'].includes(key)),'non-parameter menu clicks have no recording fallback');
   reset();ui.settingsPage=globalThis.__testSplit.pages().findIndex(p=>p.title==='PADS & SCALE');cc(3);select('CHOOSE SCALE');
   assert.equal(ui.view,'pageActions');assert.equal(actions.model.rows.length,P.scale.labels.length);actions.model.index=1;cc(3,0);cc(3);assert.equal(requests.at(-1).op,'set');assert.equal(requests.at(-1).payload,'scale:1');assert.equal(globalThis.__testSplit.pages()[ui.settingsPage].title,'PADS & SCALE');
   reset();ui.settingsPage=globalThis.__testSplit.pages().findIndex(p=>p.title==='ENGINE');cc(3);select('CHOOSE ENGINE');assert(globalThis.__testEnginePicker.ui.open);
   cc(3);assert(globalThis.__testEnginePicker.ui.open,'selector also requires release');cc(3,0);cc(51);
   reset();ui.settingsPage=globalThis.__testSplit.pages().findIndex(p=>p.fm);cc(3);select('STARTING SOUNDS');assert(globalThis.__testFm.model.startsOpen);cc(3);assert(globalThis.__testFm.model.startsOpen);cc(3,0);cc(51);
   reset();const rowsAt=drawWrites.length;cc(3);actions.draw();
   assert(drawWrites.slice(rowsAt).filter(w=>w[0]==='print').every(w=>w[1]>=0&&w[1]+String(w[3]).length*6<=128&&w[2]<=57),'list and target heading fit the OLED');
   cc(51);reset();cc(3);cc(86);assert.equal(ui.view,'settings');assert(blockingWrites.some(([key])=>key==='clip_record'),'Record keeps its dedicated path');
   console.log('s3g Thru Page Actions UI: release-before-confirm, scoped tools, compatible paste, safe default, exact Back, stale target isolation, selector handoff and recording-safe menu clicks: PASS');
 } finally {actions.close();globalThis.host_module_get_param=oldGet;globalThis.host_module_set_param_blocking=oldSet;}
}
