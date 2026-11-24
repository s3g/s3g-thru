import assert from 'node:assert/strict';
export function testNamingUi({params,blockingWrites,drawWrites}){
 const names=globalThis.__testNaming,ui=globalThis.__testFx.state(),tools=globalThis.__testSplit,project=globalThis.__testProjects.model;
 const oldGet=globalThis.host_module_get_param,oldSet=globalThis.host_module_set_param_blocking;
 let job=0,ok=1,path='',error='',deny=false;const writes=[];
 const labels={clips:Array(64).fill(''),chains:Array(16).fill(''),pads:Array(16).fill(''),project:'',lane:'',source:''};
 const context=()=>String(ui.activeClip)+':'+ui.lane;
 const cc=(n,v=127)=>globalThis.onMidiMessageInternal([0xb0,n,v]);
 globalThis.host_module_get_param=k=>k==='naming_state'?JSON.stringify({context:context(),job,ok,path,message:'FILE RENAMED'}):k==='naming_labels'?JSON.stringify(labels):k==='last_error'?error:oldGet(k);
 globalThis.host_module_set_param_blocking=(k,v)=>{
  if(k==='naming_open')return true;
  if(k!=='naming_apply')return oldSet(k,v);
  if(deny){deny=false;return false;}
  const [kind,target,value,file]=String(v).split('\n');assert.equal(target,context());writes.push({kind,value,file});error='';
  if(kind==='clip')labels.clips[ui.activeClip]=value;if(kind==='project')labels.project=value;
  if(['rename','copy','copyload','projectfile'].includes(kind)){job=1;ok=0;}return true;
 };
 const reset=()=>{
  names.model.busy=false;names.model.open=false;job=0;ok=1;error='';path='';
  Object.assign(ui,{view:'settings',activeClip:0,lane:1,splitEnabled:false,quadMode:false,sharedDual:false,bankMode:false,splitFocus:0,shiftHeld:false,focusButtonHeld:false,performanceVisible:false,editorReady:true});
  globalThis.__testSequence.model.active=false;globalThis.__testLayers.model.active=false;ui.settingsPage=tools.pages().findIndex(p=>p.title==='AMP ENVELOPE');
 };
 const key=logical=>globalThis.onMidiMessageInternal([0x90,68+(3-Math.floor(logical/8))*8+logical%8,100]);
 const save=()=>{cc(3,0);cc(3);};
 try{
  reset();const page=ui.settingsPage;names.open('clip','');assert.equal(ui.view,'naming');key(0);key(1);assert.equal(names.model.name,'AB');cc(14,127);key(27);assert.equal(names.model.name,'B');
  const start=writes.length;cc(3);assert.equal(writes.length,start,'opening press cannot save');save();assert.equal(writes.at(-1).value,'B');assert.equal(ui.view,'settings');assert.equal(ui.settingsPage,page);
  names.open('clip','Before');key(2);cc(51);assert.equal(labels.clips[0],'B','Back discards the draft');
  names.open('clip','Another');ui.activeClip=1;save();assert.equal(writes.length,1);assert.equal(ui.message,'NAMING TARGET CHANGED');
  reset();names.open('clip','No Ack');deny=true;save();assert.equal(ui.message,'NAME NOT CONFIRMED');assert.equal(writes.length,1);
  reset();names.open('clip','X'.repeat(40));key(0);assert.equal(names.model.name.length,40);key(27);key(26);assert.equal(names.model.name.length,40);key(28);key(29);
  const drawn=drawWrites.length;names.draw(names.model,'NAME CLIP A01');assert(drawWrites.slice(drawn).filter(w=>w[0]==='print').every(w=>w[1]>=0&&w[1]+String(w[3]).length*6<=128&&w[2]<=57));cc(51);
  reset();ui.view='library';ui.libraryLane=3;ui.libraryDir='/data/s3g-thru/captures';ui.capturesDirectory='/data/s3g-thru/captures';ui.libraryItems=[{path:ui.libraryDir+'/t0000000001-take.wav',name:'t0000000001-take.wav',directory:false}];ui.libraryIndex=0;cc(49);cc(3);cc(49,0);assert.equal(ui.view,'namingActions');assert.equal(names.model.rows[0][0],'RENAME FILE');
  names.model.selection=2;save();assert.equal(ui.view,'naming');assert.equal(names.model.kind,'copyload');names.model.name='Named Take';save();assert.equal(ui.view,'namingBusy');assert.equal(writes.at(-1).kind,'copyload');
  job=0;ok=1;path='/data/UserData/UserLibrary/Samples/s3g Thru/Named/Named Take.wav';names.model.lastPoll=0;names.tick();assert.equal(ui.view,'library');assert(blockingWrites.some(([k,v])=>k==='load_lane_file'&&v==='3,'+path));
  reset();ui.view='projects';Object.assign(project,{data:{v:1,job:0,reopen:0},mode:'menu',selection:0,files:[]});params.project_state=JSON.stringify({v:1,job:0,reopen:0,ok:1});params.project_files='[]';
  const exports=blockingWrites.filter(([k])=>k==='project_export').length;cc(3);assert.equal(ui.view,'naming');assert.equal(names.model.kind,'project');names.model.name='Evening Study';save();assert.equal(labels.project,'Evening Study');assert.equal(blockingWrites.filter(([k])=>k==='project_export').length,exports+1);
  reset();ui.view='projects';Object.assign(project,{mode:'list',selection:0,files:['Evening Study.thru']});params.project_directory='/data/UserData/UserLibrary/Samples/Thru Projects';cc(49);cc(3);cc(49,0);assert.equal(ui.view,'namingActions');save();assert.equal(names.model.kind,'projectfile');assert.equal(names.model.path,params.project_directory+'/Evening Study.thru');cc(51);
  console.log('Shared naming UI: actual pad order, insert/delete/case/numbers, 40-character bound, safe confirmation/cancel, stale targets, failed ACK, file actions, captured load lane, project export and text bounds: PASS');
 }finally{names.model.busy=false;names.model.open=false;globalThis.host_module_get_param=oldGet;globalThis.host_module_set_param_blocking=oldSet;ui.view='main';}
}
