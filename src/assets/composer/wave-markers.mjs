import {waveformPath,sampleSource} from './sample-view.mjs';
import {sampleTarget,sameSampleTarget,uploadFolderLabel} from './sample-picker.mjs';
const NS='http://www.w3.org/2000/svg',clone=x=>x.map(p=>({...p}));
const svg=(tag,attributes={})=>{const e=document.createElementNS(NS,tag);for(const[k,v]of Object.entries(attributes))e.setAttribute(k,v);return e;};
export function normalizedMarkers(rows=[],gap=0){
 const result=[{position:0,label:''}];
 for(const m of Array.isArray(rows)?[...rows].sort((a,b)=>a.position-b.position):[]){if(!Number.isFinite(m.position)||m.position<0||m.position>=1)continue;const point={position:m.position,label:String(m.label||'').slice(0,23)};if(point.position===0){result[0]=point;continue;}if(result.length<64&&point.position-result.at(-1).position>=Math.max(gap,1e-12))result.push(point);}
 return result;
}
const SOURCE_NAMES={file:'File markers',edited:'Edited slices',transients:'Detected transients'};
export function initialMarkerDraft(maps={},gap=0){
 const source=['file','edited','transients'].find(key=>Array.isArray(maps[key])&&maps[key].some(m=>Number.isFinite(m.position)&&m.position>=0&&m.position<1))||'file';
 return {source,points:normalizedMarkers(maps[source],gap)};
}
export function markerIdentity(frame){const source=sampleSource(frame);return frame?JSON.stringify([frame.activeClip,frame.document.clips[0].mode,frame.focus,source.lane,source.asset?.id,source.asset?.frames,frame.waveVersion]):'';}
export function markerDefaultName(name='sample.wav'){const stem=name.replace(/\.wav$/i,''),marked=stem.match(/^(.*-marked)(?:-\d+)?$/i);return (marked?marked[1]:stem.slice(0,169)+'-marked')+'.wav';}
export function selectedMarkerSlice(points,index){
 if(!Array.isArray(points)||!Number.isInteger(index)||index<0||index>=points.length)return null;
 const start=points[index].position,end=points[index+1]?.position??1;
 return Number.isFinite(start)&&Number.isFinite(end)&&start>=0&&end<=1&&start<end?{start,end}:null;
}
export class WaveMarkers{
 constructor(api){
  this.api=api;const d=this.dialog=document.createElement('dialog');d.id='wave-markers';d.className='wave-markers';
  d.innerHTML=`<header><h2 id="wm-title">WAV markers</h2><button id="wm-cancel" type="button">Cancel</button></header><p id="wm-target"></p>
   <div class="wm-toolbar"><label>START FROM <select id="wm-source"><option value="file">File markers</option><option value="edited">Edited slices</option><option value="transients">Detected transients</option></select></label><button id="wm-seed" type="button">Use boundaries</button><button id="wm-clear" type="button">Clear cuts</button><button id="wm-undo" type="button">Undo draft</button><label>ZOOM <select id="wm-zoom"><option>1</option><option>2</option><option>4</option><option>8</option><option>16</option><option>32</option><option>64</option></select>×</label></div>
   <svg id="wm-wave" viewBox="0 0 1000 150" preserveAspectRatio="none" tabindex="0" aria-label="WAV marker waveform"></svg><input id="wm-pan" type="range" min="0" max="0" step=".0001" value="0" aria-label="Waveform scroll" hidden>
   <div class="wm-toolbar"><label>CURSOR <input id="wm-cursor" type="number" min="0" step=".1">ms</label><button id="wm-add" type="button">Add at cursor</button><span id="wm-count"></span></div>
   <div class="wm-editor"><select id="wm-list" size="6" aria-label="Slice starts"></select><div><label>POSITION <input id="wm-position" type="number" min="0" step=".1">ms</label><label>LABEL <input id="wm-label" type="text" maxlength="23"></label><button id="wm-delete" type="button">Delete marker</button><div class="wm-audition"><button id="wm-audition" type="button">Audition slice</button><span id="wm-range"></span></div><p>Drag a marker to move it. Click the wave to set the cursor; double-click to add. Start at zero is fixed. Labels: up to 23 plain text characters.</p></div></div>
   <p>Edits stay in this draft until saved. Slices uses these cues with SSRC File or Auto. The overview covers the loaded audio, up to 12 seconds.</p><label class="wm-name">SAVE AS <input id="wm-name" type="text" maxlength="180"></label><p id="wm-folder"></p><p id="wm-status" role="status"></p>
   <footer><button id="wm-save" type="button">Save WAV copy &amp; load lane</button></footer>`;
  d.setAttribute('aria-labelledby','wm-title');d.addEventListener('keydown',e=>{e.stopPropagation();if(e.key==='Enter'&&e.target.tagName==='INPUT'){e.preventDefault();e.target.blur();}if(['Delete','Backspace'].includes(e.key)&&e.target.id==='wm-wave'){e.preventDefault();this.remove();}});document.body.append(d);this.$=id=>d.querySelector('#wm-'+id);
  this.$('cancel').onclick=()=>this.close();d.oncancel=e=>{if(this.busy)e.preventDefault();else this.stopAudition();};
  window.addEventListener('pagehide',()=>this.stopAudition());
  this.$('list').onchange=()=>{this.stopAudition();this.selected=Number(this.$('list').value);this.cursor=this.points[this.selected].position;this.paint();};
  this.$('source').onchange=()=>{this.paint();const key=this.$('source').value,rows=this.maps[key]||[];this.$('status').textContent=rows.length?`${SOURCE_NAMES[key]} selected. Press Use boundaries to replace the current draft. Undo draft restores it.`:'No boundaries are available for this source.';};
  this.$('seed').onclick=()=>{const key=this.$('source').value;this.edit(()=>{this.points=normalizedMarkers(this.maps[key],this.gap);this.selected=0;this.cursor=0;});this.$('status').textContent=key==='transients'&&this.points.length===1?'No additional onsets detected. Add markers manually or choose another source.':`${SOURCE_NAMES[key]} loaded into the draft. Save a WAV copy to keep these boundaries.`;};
  this.$('clear').onclick=()=>this.edit(()=>{this.points=[{position:0,label:''}];this.selected=0;});
  this.$('undo').onclick=()=>{if(!this.busy&&this.history.length){this.stopAudition();this.points=this.history.pop();this.selected=Math.min(this.selected,this.points.length-1);this.paint();}};
  this.$('position').onchange=()=>this.edit(()=>this.move(Number(this.$('position').value)/this.duration));
  this.$('label').onchange=()=>this.edit(()=>{this.points[this.selected].label=this.$('label').value;});
  this.$('cursor').onchange=()=>{this.cursor=Math.max(0,Math.min(1-this.gap,Number(this.$('cursor').value)/this.duration));this.paint();};
  this.$('audition').onclick=()=>this.auditioning?this.stopAudition():this.audition();
  this.$('add').onclick=()=>this.add();this.$('delete').onclick=()=>this.remove();this.$('save').onclick=()=>this.save();
  this.$('zoom').onchange=()=>{this.zoom=Number(this.$('zoom').value);this.start=Math.max(0,Math.min(1-1/this.zoom,this.cursor-.5/this.zoom));this.paint();};this.$('pan').oninput=()=>{this.start=Number(this.$('pan').value);this.paint();};
  const wave=this.$('wave');wave.onpointerdown=e=>{if(e.button!==0||this.busy||!this.valid())return;e.preventDefault();wave.focus({preventScroll:true});const index=e.target.closest('[data-marker]')?.dataset.marker;if(index!==undefined){this.stopAudition();this.selected=Number(index);this.cursor=this.points[this.selected].position;this.drag={pointer:e.pointerId,before:clone(this.points)};wave.setPointerCapture(e.pointerId);}else this.cursor=this.position(e);this.paint();};
  wave.onpointermove=e=>{if(this.drag?.pointer===e.pointerId){this.move(this.position(e));this.cursor=this.points[this.selected].position;this.paint();}};
  wave.onpointerup=wave.onpointercancel=()=>{if(this.drag&&JSON.stringify(this.drag.before)!==JSON.stringify(this.points))this.history.push(this.drag.before);this.drag=null;this.paint();};
  wave.ondblclick=e=>{if(!e.target.closest('[data-marker]'))this.add();};
 }
 valid(){return this.api.linked()&&this.owner===this.api.connection()&&this.identity===markerIdentity(this.api.frame());}
 position(e){const b=this.$('wave').getBoundingClientRect();return Math.max(0,Math.min(1-this.gap,this.start+(e.clientX-b.left)/b.width/this.zoom));}
 open(){
  if(this.dialog.open||!this.api.linked()||this.api.pending())return;
  const frame=this.api.frame(),source=sampleSource(frame);if(!source.asset?.frames||!source.asset.waveform?.length)return this.api.message('Load a lane sample before editing WAV markers.',true);
  this.owner=this.api.connection();this.identity=markerIdentity(frame);this.target={...sampleTarget(frame),lane:source.lane,asset:source.asset.id,wave:frame.waveVersion};this.asset=source.asset;this.maps=frame.sampleMarkers?.[source.lane]||{};
  this.duration=this.asset.frames/this.asset.sampleRate*1000;this.gap=1/this.asset.frames;const draft=initialMarkerDraft(this.maps,this.gap);this.points=draft.points;this.history=[];this.selected=0;this.cursor=0;this.zoom=1;this.start=0;this.folder=this.api.folder();this.busy=false;this.auditionOwned=false;this.auditioning=false;
  for(const option of this.$('source').options){const count=this.maps[option.value]?.length||0;option.disabled=!count;option.textContent=SOURCE_NAMES[option.value]+' · '+count;}this.$('source').value=draft.source;
  this.$('name').value=markerDefaultName(this.asset.name).slice(0,180);this.$('target').textContent=`Clip ${frame.activeClip+1} · Lane ${source.lane+1} · ${this.asset.name}`;this.$('folder').textContent='Destination: '+uploadFolderLabel(this.folder)+'. Change the upload folder in Samples before opening this editor.';
  this.$('status').textContent=(!this.maps[draft.source]?.length?'No saved boundaries are available; add markers manually. ':draft.source==='transients'&&this.points.length===1?'No additional onsets detected. Add markers manually. ':SOURCE_NAMES[draft.source]+' loaded into the draft. ')+'Saving makes a new WAV and loads it into this lane. The original stays intact.';this.$('zoom').value='1';this.paint();this.dialog.showModal();this.drawAudition();
 }
 close(){if(!this.busy){this.stopAudition();this.dialog.close();}}
 observe(){if(this.dialog.open&&!this.valid()){this.drag=null;this.stopAudition();this.$('status').textContent='Source, lane or clip changed. Cancel and reopen WAV markers.';this.paint();}}
 edit(action){if(this.busy||!this.valid())return;this.stopAudition();const before=clone(this.points);action();if(JSON.stringify(before)!==JSON.stringify(this.points))this.history.push(before);this.history=this.history.slice(-64);this.paint();}
 move(position){if(!this.selected||!Number.isFinite(position))return;this.points[this.selected].position=Math.max(this.points[this.selected-1].position+this.gap,Math.min((this.points[this.selected+1]?.position??1)-this.gap,position));}
 add(){this.edit(()=>{if(this.points.length>=64)return;const at=this.cursor;if(at<this.gap||this.points.some(p=>Math.abs(p.position-at)<this.gap))return;this.points.push({position:at,label:''});this.points.sort((a,b)=>a.position-b.position);this.selected=this.points.findIndex(p=>p.position===at);});}
 remove(){this.edit(()=>{if(this.selected){this.points.splice(this.selected,1);this.selected=Math.min(this.selected,this.points.length-1);}});}
 status(status){const was=this.auditioning;this.auditioning=!!(this.auditionOwned&&status.sampleLoad?.audition);if(was&&!this.auditioning&&this.auditionOwned&&this.dialog.open&&!this.busy)this.$('status').textContent='Slice audition finished.';this.drawAudition();}
 drawAudition(){
  const slice=selectedMarkerSlice(this.points,this.selected);this.$('audition').textContent=this.auditioning?'Stop audition':'Audition slice';this.$('audition').disabled=this.busy||!this.dialog.open||!this.valid()||!slice;
  this.$('range').textContent=slice?`Slice ${this.selected+1} · ${(slice.start*this.duration).toFixed(1)}–${(slice.end*this.duration).toFixed(1)} ms · ${((slice.end-slice.start)*this.duration).toFixed(1)} ms`:'';
 }
 stopAudition(){if(this.auditionOwned){try{this.owner?.send('preview_stop','1');}catch{}if(this.dialog.open&&!this.busy)this.$('status').textContent='Slice audition stopped.';}this.auditionOwned=false;this.auditioning=false;this.drawAudition();}
 async audition(){
  const slice=selectedMarkerSlice(this.points,this.selected);if(this.busy||!this.valid()||!slice)return;
  try{
   if(this.api.pending()||this.api.recording())throw Error('Finish recording and pending edits first.');this.busy=true;this.api.exclusive(true);this.paint();this.$('status').textContent='Preparing selected slice…';
   const request={...this.target,audition:true,...slice},leaf=await this.owner.upload(new Blob([JSON.stringify(request)],{type:'application/json'}),'.json');if(!this.valid())throw Error('Source changed; reopen WAV markers.');
   const reply=await this.owner.command('markers',leaf);this.auditionOwned=true;if(!this.valid()||!this.dialog.open){this.stopAudition();throw Error('Source changed; reopen WAV markers.');}
   this.auditioning=!!reply.sampleLoad?.audition;this.$('status').textContent=`Auditioning slice ${this.selected+1}. Uses the draft boundaries and Move’s Preview Level/route.`;
  }catch(e){this.$('status').textContent=e.message;}finally{this.busy=false;this.api.exclusive(false);this.paint();}
 }
 paint(){
  if(!this.points)return;const enabled=!this.busy&&this.valid(),p=this.points[this.selected];
  for(const e of this.dialog.querySelectorAll('input,select,button'))e.disabled=!enabled;if(!this.busy)this.$('cancel').disabled=false;
  this.$('position').disabled=!enabled||!this.selected;this.$('delete').disabled=!enabled||!this.selected;this.$('undo').disabled=!enabled||!this.history.length;this.$('add').disabled=!enabled||this.points.length>=64;this.$('seed').disabled=!enabled||!this.maps[this.$('source').value]?.length;this.drawAudition();
  if(document.activeElement!==this.$('position'))this.$('position').value=(p.position*this.duration).toFixed(2);if(document.activeElement!==this.$('label'))this.$('label').value=p.label;
  this.$('cursor').value=(this.cursor*this.duration).toFixed(2);this.$('cursor').max=this.duration;this.$('position').max=this.duration;this.$('count').textContent=`${this.points.length}/64 slice starts`;
  const list=this.$('list');list.replaceChildren();this.points.forEach((p,i)=>{const o=document.createElement('option');o.value=i;o.textContent=`${i+1} · ${(p.position*this.duration).toFixed(1)} ms${p.label?' · '+p.label:''}`;list.append(o);});list.value=this.selected;
  this.$('pan').hidden=this.zoom===1;this.$('pan').max=1-1/this.zoom;this.$('pan').value=this.start;const wave=this.$('wave');wave.replaceChildren();const selectedSlice=selectedMarkerSlice(this.points,this.selected);if(selectedSlice)wave.append(svg('rect',{x:(selectedSlice.start-this.start)*1000*this.zoom,y:0,width:(selectedSlice.end-selectedSlice.start)*1000*this.zoom,height:150,class:'wm-slice-region'}));wave.append(svg('path',{d:waveformPath(this.asset.waveform,this.start,this.start+1/this.zoom,1000,150),class:'wm-trace'}));const x=at=>(at-this.start)*1000*this.zoom;
  this.points.forEach((p,i)=>{if(p.position<this.start||p.position>this.start+1/this.zoom)return;const g=svg('g',{'data-marker':i,class:i===this.selected?'wm-marker selected':'wm-marker'});g.append(svg('line',{x1:x(p.position),x2:x(p.position),y1:0,y2:150}));const t=svg('text',{x:x(p.position)+4,y:16});t.textContent=String(i+1);g.append(t);wave.append(g);});wave.append(svg('line',{x1:x(this.cursor),x2:x(this.cursor),y1:20,y2:150,class:'wm-cursor'}));
 }
 async save(){
  if(this.busy||!this.valid())return;
  try{if(this.api.pending()||this.api.recording())throw Error('Finish recording and pending edits first.');this.stopAudition();this.busy=true;this.api.exclusive(true);this.paint();this.$('status').textContent='Saving marked WAV copy and loading lane…';
   const request={...this.target,points:clone(this.points),name:this.$('name').value};if(this.folder!==null)request.folder=this.folder;const leaf=await this.owner.upload(new Blob([JSON.stringify(request)],{type:'application/json'}),'.json');if(!this.valid())throw Error('Source changed before saving. Reopen WAV markers.');await this.owner.command('markers',leaf);this.api.relink();this.dialog.close();this.api.message('Marked WAV saved and loaded. In Slices, choose SSRC File or Auto.');
  }catch(e){this.$('status').textContent=e.message;}finally{this.busy=false;this.api.exclusive(false);this.paint();}
 }
}
