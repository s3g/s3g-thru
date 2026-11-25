export const SAMPLE_MODES=['single','split','shared-dual','quad','bank','layers','sequence'];
export function sampleLanes(frame){return frame.document.clips[0].mode==='bank'&&frame.focus===1?[]:[0,1,2,3];}
export function sampleOwner(frame,lane){
 const mode=frame.document.clips[0].mode;
 return mode==='split'?Math.floor(lane/2):['quad','sequence'].includes(mode)?lane:mode==='shared-dual'?frame.focus:0;
}
export function sampleTarget(frame){return{clip:frame.activeClip,part:frame.focus,mode:SAMPLE_MODES.indexOf(frame.document.clips[0].mode)};}
export function sameSampleTarget(target,frame){return !!frame&&JSON.stringify(target)===JSON.stringify(sampleTarget(frame));}
export function uploadFolderLabel(folder){return 'Move Samples / '+(folder===null?'s3g Thru / Browser Samples (default)':folder||'(root)');}
export class SamplePicker{
 constructor(api){
  this.api=api;this.generation=0;this.busy=false;this.listing=false;this.uploadFolder=null;
  const d=this.dialog=document.createElement('dialog');d.id='sample-picker';d.className='sample-picker';
  d.innerHTML=`<header><h2 id="sample-picker-title">Load sample</h2><button id="sample-picker-close" type="button">Close</button></header>
   <div class="sample-picker-lanes"><span>LOAD INTO LANE</span><div id="sample-picker-lanes" role="group" aria-label="Destination sample lane"></div></div>
   <p id="sample-picker-target"></p>
   <div class="sample-picker-tools"><label>BROWSE <select id="sample-picker-root"><option value="0">Move Samples</option><option value="1">Thru Recordings</option><option value="2">Thru Process</option></select></label><button id="sample-picker-up" type="button">Up</button><button id="sample-picker-refresh" type="button">Refresh</button><label>SEARCH <input id="sample-picker-search" type="search" maxlength="100"></label></div>
   <p id="sample-picker-path"></p><div id="sample-picker-list" class="sample-picker-list" aria-label="Move sample folders and WAVs"></div>
   <div id="sample-picker-paging" class="sample-picker-paging" hidden><button id="sample-picker-prev" type="button">Previous files</button><output id="sample-picker-page"></output><button id="sample-picker-next" type="button">Next files</button></div>
   <div class="sample-picker-upload"><label>Or choose a WAV from this computer <input id="sample-picker-upload" type="file" accept=".wav,audio/wav,audio/x-wav"></label>
    <div class="sample-picker-destination"><span>SAVE UPLOAD TO</span><output id="sample-picker-destination"></output><button id="sample-picker-use-folder" type="button">Use this folder</button><button id="sample-picker-default-folder" type="button">Default folder</button></div>
    <small>Open a folder in Move Samples above, then choose Use this folder. Your computer WAV stays selected.</small>
    <small>Mono/stereo PCM or float WAV, up to 128 MiB. Thru uses up to 12 seconds per lane.</small></div>
   <p id="sample-picker-selection">Choose a sample.</p><label id="sample-picker-file-option" hidden><input type="checkbox" id="sample-picker-use-file" checked><span id="sample-picker-file-label"></span></label>
   <p id="sample-picker-message" role="status"></p><progress id="sample-picker-progress" max="1000" hidden></progress><footer><button id="sample-picker-audition" type="button" disabled>Audition</button><button id="sample-picker-load" type="button" disabled>Load into lane</button></footer>`;
  d.setAttribute('aria-labelledby','sample-picker-title');d.addEventListener('keydown',e=>e.stopPropagation());document.body.append(d);this.$=id=>d.querySelector('#sample-picker-'+id);
  for(let lane=0;lane<4;lane++){const b=document.createElement('button');b.type='button';b.dataset.lane=lane;b.textContent=String(lane+1);b.setAttribute('aria-label','Load into lane '+(lane+1));b.onclick=()=>{this.stopAudition();this.lane=lane;this.drawFileOption();this.drawSelection();};this.$('lanes').append(b);}
  this.$('close').onclick=()=>this.close();d.oncancel=e=>{if(this.busy){e.preventDefault();return;}this.stopAudition();this.generation++;};
  this.$('root').onchange=()=>{this.path='';this.offset=0;this.read();};
  this.$('up').onclick=()=>{this.path=this.path.split('/').slice(0,-1).join('/');this.offset=0;this.read();};this.$('refresh').onclick=()=>this.read();
  this.$('search').oninput=()=>{clearTimeout(this.searchTimer);this.searchTimer=setTimeout(()=>{this.offset=0;this.read();},250);};
  this.$('prev').onclick=()=>{this.offset=Math.max(0,this.offset-64);this.read();};this.$('next').onclick=()=>{this.offset+=64;this.read();};
  this.$('use-folder').onclick=()=>{if(!this.folderReady||this.folderReady.root!==0||this.busy||this.listing)return;this.uploadFolder=this.folderReady.path;this.drawUploadFolder();};
  this.$('default-folder').onclick=()=>{this.uploadFolder=null;this.drawUploadFolder();};
  this.$('upload').onchange=()=>{const file=this.$('upload').files[0];this.stopAudition();this.selection=null;this.drawSelection();if(file&&!/\.wav$/i.test(file.name)){this.message('Choose a WAV file.',true);return;}if(file&&file.size>128*1024*1024){this.message('Choose a WAV smaller than 128 MiB.',true);return;}this.selection=file?{file}:null;this.drawSelection();};
  this.$('load').onclick=()=>this.load();this.$('audition').onclick=()=>this.auditioning?this.stopAudition():this.load(true);
  window.addEventListener('pagehide',()=>this.stopAudition());
 }
 message(text,error=false){this.$('message').textContent=text;this.$('message').dataset.error=String(error);}
 context(){if(this.owner!==this.api.connection()||!this.api.linked()||!sameSampleTarget(this.target,this.api.frame()))throw Error('Clip, part or mode changed. Reopen Samples for the new destination.');}
 async open(){
  if(this.dialog.open)return;if(!this.api.linked()||this.api.pending())return this.api.message('Finish pending edits before opening Samples.',true);
  const frame=this.api.frame(),lanes=sampleLanes(frame);if(!lanes.length)return this.api.message('Use Move to load Bank pads. Samples can load the instrument’s lanes.',true);
  this.owner=this.api.connection();this.target=sampleTarget(frame);this.generation++;this.path='';this.offset=0;this.total=0;this.selection=null;this.folderReady=null;
  this.$('root').value='0';this.$('search').value='';this.$('upload').value='';this.$('list').replaceChildren();this.$('paging').hidden=true;
  this.lane=lanes.includes(frame.patches[frame.focus])?frame.patches[frame.focus]:lanes[0];this.$('progress').hidden=true;this.drawFileOption();this.drawUploadFolder();this.drawSelection();this.dialog.showModal();await this.read();
 }
 close(){if(this.busy)return;this.stopAudition();this.generation++;clearTimeout(this.searchTimer);this.dialog.close();}
 observe(){if(this.dialog.open&&(this.owner!==this.api.connection()||!sameSampleTarget(this.target,this.api.frame()))){this.stopAudition();this.$('load').disabled=true;this.$('audition').disabled=true;this.message('Clip, part or mode changed. Close and reopen Samples.',true);}}
 status(s){const job=s.sampleLoad;this.auditioning=!!(this.auditionOwned&&job?.audition);this.drawAudition();if(!this.busy)return;if(job&&job.job){this.$('progress').hidden=false;if(job.progress>0)this.$('progress').value=job.progress;else this.$('progress').removeAttribute('value');}}
 drawAudition(){const valid=sameSampleTarget(this.target,this.api.frame())&&this.owner===this.api.connection();this.$('audition').textContent=this.auditioning?'Stop audition':'Audition';this.$('audition').disabled=this.busy||this.listing||!this.selection||!valid;}
 stopAudition(){if(this.auditionOwned){try{this.owner?.send('preview_stop','1');}catch{}}this.auditionOwned=false;this.auditioning=false;this.drawAudition();}
 drawFileOption(){
  const f=this.api.frame(),part=sampleOwner(f,this.lane),live=Number(f.settings[part]?.live_source??f.document.clips[0].parts[part].parameters.live_source??0)>0;
  this.$('file-option').hidden=!live;this.$('use-file').checked=live;this.$('file-label').textContent=`Switch Part ${'ABCD'[part]} from Live Memory to File after loading`;
 }
 drawUploadFolder(){
  this.$('destination').textContent=uploadFolderLabel(this.uploadFolder);this.$('use-folder').disabled=this.busy||this.listing||this.folderReady?.root!==0;this.$('default-folder').disabled=this.busy||this.uploadFolder===null;
 }
 drawSelection(){
  const s=this.selection,frame=this.api.frame(),valid=sameSampleTarget(this.target,frame)&&this.owner===this.api.connection();
  for(const b of this.$('lanes').children)b.setAttribute('aria-pressed',String(Number(b.dataset.lane)===this.lane));
  if(valid){const part=sampleOwner(frame,this.lane),shared=frame.document.clips[0].mode==='shared-dual';this.$('target').textContent=`Clip ${this.target.clip+1} · Lane ${this.lane+1} · ${shared?'shared A + B':'Part '+'ABCD'[part]}. Notes and automation are retained.`;}
  this.$('selection').textContent=s?'Selected: '+(s.file?s.file.name:s.name):'Choose a sample.';this.$('load').textContent=(s?.file?'Upload and load':'Load')+' → lane '+(this.lane+1);this.$('load').disabled=!s||this.busy||this.listing||!valid;this.drawAudition();
 }
 setBusy(value){this.busy=value;for(const e of this.dialog.querySelectorAll('input,select,button'))e.disabled=value;this.api.exclusive(value);if(!value){this.$('progress').hidden=true;this.$('up').disabled=!this.path;this.$('prev').disabled=this.offset===0;this.$('next').disabled=this.offset+64>=(this.total??0);this.drawUploadFolder();this.drawSelection();}}
 async request(op,data){this.context();const c=this.owner;const file=await c.upload(new Blob([JSON.stringify(data)],{type:'application/json'}),'.json');this.context();return c.command(op,file);}
 async read(){
  if(this.busy)return;if(this.listing){this.reread=true;return;}const generation=this.generation;this.listing=true;this.folderReady=null;
  if(!this.selection?.file){this.stopAudition();this.selection=null;this.$('upload').value='';}this.$('list').replaceChildren();this.$('paging').hidden=true;this.message('Reading sample folder…');this.drawSelection();this.drawUploadFolder();
  try{
   this.context();const root=Number(this.$('root').value),query={root,path:this.path,offset:this.offset,search:this.$('search').value},reply=await this.request('samples',query);
   if(generation!==this.generation||!this.dialog.open)return;const data=await(await this.api.connection().file(reply.result)).json();this.context();
   if(generation!==this.generation||!this.dialog.open||data.root!==Number(this.$('root').value)||data.path!==this.path||query.offset!==this.offset||query.search!==this.$('search').value||!Array.isArray(data.entries))return;
   this.folderReady={root,path:data.path};this.$('path').textContent=this.$('root').selectedOptions[0].textContent+(this.path?' / '+this.path:'');const list=this.$('list');list.replaceChildren();
   for(const row of data.entries){const button=document.createElement('button');button.type='button';button.className='sample-picker-entry';button.textContent=(row.directory?'Folder · ':'WAV · ')+row.name;button.title=row.directory?'Open folder':Math.ceil(row.bytes/1024)+' KiB';button.onclick=()=>{if(this.busy)return;if(row.directory){this.path+=(this.path?'/':'')+row.name;this.offset=0;this.read();}else{this.stopAudition();this.selection={root,path:this.path+(this.path?'/':'')+row.name,name:row.name};this.$('upload').value='';for(const b of list.children)b.classList.toggle('selected',b===button);this.drawSelection();}};list.append(button);}
   if(!data.entries.length){const empty=document.createElement('p');empty.textContent='No matching WAVs or folders.';list.append(empty);}
   this.total=data.total;this.$('paging').hidden=data.total<=64;this.$('page').textContent=`Files ${data.total?data.offset+1:0}–${Math.min(data.offset+64,data.total)} of ${data.total}`;this.$('prev').disabled=data.offset===0;this.$('next').disabled=data.offset+64>=data.total;this.$('up').disabled=!this.path;this.message('Select a WAV, then load it into the chosen lane.');
  }catch(e){this.message(e.message,true);}finally{this.listing=false;this.drawUploadFolder();this.drawSelection();if(this.reread){this.reread=false;this.read();}}
 }
 async load(preview=false){
  if(this.busy||!this.selection)return;const s=this.selection,generation=this.generation,folder=this.uploadFolder;
  try{
   this.context();if(this.api.recording())throw Error('Finish recording before replacing a sample.');if(this.api.pending())throw Error('Finish pending edits before loading.');this.setBusy(true);
   const request={...this.target,lane:this.lane,file:!this.$('file-option').hidden&&this.$('use-file').checked,preview};
   if(s.file){this.message('Uploading '+s.file.name+'…');request.upload=await this.api.connection().upload(s.file,'.wav');request.name=s.file.name;if(folder!==null)request.folder=folder;}else{request.root=s.root;request.path=s.path;}
   this.context();if(generation!==this.generation)throw Error('Sample picker closed before loading.');this.message((preview?'Auditioning ':'Loading ')+(s.file?s.file.name:s.name)+' on Move…');const reply=await this.request('sample',request);if(preview){this.auditionOwned=true;this.auditioning=!!reply.sampleLoad?.audition;this.message('Audition through Move’s preview output. The lane sample is unchanged.');}else{this.auditionOwned=false;this.auditioning=false;this.api.relink();this.dialog.close();this.generation++;this.api.message('Sample loaded into lane '+(request.lane+1)+'.');}
  }catch(e){this.message(e.message,true);}finally{this.setBusy(false);}
 }
}
