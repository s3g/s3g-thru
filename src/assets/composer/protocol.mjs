export const PANEL_PROTOCOL=2, PANEL_BUILD='0.34.1';
import {uniqueId} from './format.mjs';
export class MoveConnection {
  constructor(onStatus = () => {}) {
    this.client = uniqueId('browser'); this.onStatus = onStatus;
    this.revision = '0'; this.root = '/data/UserData/UserLibrary/Samples/s3g Thru/Composer';
    this.connected = false; this.pending = false; this.epoch = null;
    this.cachedAssets = new Set(); this.polling = null; this.closed = false;
  }
  async connect() {
    const ws = this.ws = new WebSocket(`${location.protocol === 'https:' ? 'wss:' : 'ws:'}//${location.host}/ws/remote-ui`);
    await new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(Error('Move connection timed out')), 5000);
      ws.onopen = () => { clearTimeout(timer); resolve(); };
      ws.onerror = () => { clearTimeout(timer); reject(Error('Open this editor from Schwung Manager on Move')); };
    });
    ws.onmessage = event => {
      try { const message = JSON.parse(event.data); if (message.type === 'error') this.wireError = message.message || message.error || 'Move rejected the command'; } catch {}
    };
    ws.onclose = () => {
      this.connected = false; clearTimeout(this.timer); clearInterval(this.heartbeat);
      if (!this.closed) this.onStatus({ok:false,message:'Move disconnected. Reconnect to continue live control.'});
    };
    const reply = await this.command('hello', '', {initial:true});
    if(reply.panelProtocol!==PANEL_PROTOCOL||reply.panelBuild!==PANEL_BUILD){ws.close();throw Error('Restart Thru with '+PANEL_BUILD+', then hard-reload this tab');}
    this.connected = true; this.epoch = reply.epoch;
    this.heartbeat = setInterval(() => { if (ws.readyState === 1) this.send('composer_command',`${this.client}|ping|heartbeat||${this.revision}`); },2000);
    const poll = async () => {
      if (this.closed || !this.connected) return;
      try { await this.poll(); } catch (error) { this.onStatus({ok:false,message:error.message}); }
      if (!this.closed && this.connected) this.timer = setTimeout(poll,this.live || this.preview || this.pending ? 100 : 600);
    };
    this.timer = setTimeout(poll,100);
    return reply;
  }
  send(key,value) {
    if (this.ws?.readyState !== 1) throw Error('Move disconnected');
    if (new TextEncoder().encode(value).length >= 224) throw Error('Command too long');
    this.ws.send(JSON.stringify({type:'set_param',slot:0,key:'overtake_dsp:'+key,value}));
  }
  async file(path) {
    const controller = new AbortController(), timer = setTimeout(()=>controller.abort(),8000);
    try {
      const response=await fetch(`/files/download?path=${encodeURIComponent(path)}`,{credentials:'same-origin',cache:'no-store',signal:controller.signal});
      if (!response.ok) throw Object.assign(Error('Move file transfer failed ('+response.status+')'),{status:response.status});
      // Finish the body before removing the timeout; headers alone are not completion.
      const bytes=await response.arrayBuffer(); return new Response(bytes,{status:response.status,headers:response.headers});
    } catch(error) { if(error.name==='AbortError')throw Error('Move file transfer timed out; try again');throw error; }
    finally {clearTimeout(timer);}
  }
  async poll() {
    if (this.polling) return this.polling;
    this.polling=(async()=>{
      const status=await(await this.file(this.root+'/status.json')).json();
      if(this.epoch&&status.epoch!==this.epoch){this.connected=false;this.cachedAssets.clear();throw Error('Thru restarted. Reconnect to the live panel.');}
      if(status.client===this.client){this.revision=status.revision;this.preview=!!status.preview;this.onStatus(status);}
      return status;
    })();
    try {return await this.polling;} finally {this.polling=null;}
  }
  rememberAssets(doc,fromRead=false) {
    for(const asset of doc.assets||[]){
      if(!fromRead){this.cachedAssets.add(asset.sha256);continue;}
      try {const url=new URL(asset.url,location.origin||'http://'+location.host);if(url.origin===(location.origin||'http://'+location.host)&&url.pathname==='/files/download'&&url.searchParams.get('path')===this.root+'/cache/'+asset.sha256+'.wav')this.cachedAssets.add(asset.sha256);}catch{}
    }
  }
  async upload(blob,suffix) {
    if(blob.size>2*1024*1024*1024)throw Error('Transfer exceeds 2 GiB');
    const name=uniqueId('upload')+suffix, token=document.cookie.split('; ').find(v=>v.startsWith('csrf_token='))?.slice(11)||'',form=new FormData();
    form.append('path',this.root+'/uploads');form.append('file',blob,name);
    const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),120000);
    try {
      const response=await fetch('/files/upload',{method:'POST',credentials:'same-origin',headers:{'X-CSRF-Token':decodeURIComponent(token)},body:form,signal:controller.signal});
      if(!response.ok)throw Error('Upload failed ('+response.status+')');
      await response.text();return name;
    } catch(error){if(error.name==='AbortError')throw Error('Move upload timed out; try again');throw error;}
    finally{clearTimeout(timer);}
  }
  async command(op,file='',{initial=false,revision=this.revision}={}) {
    if(this.pending)throw Error('Wait for the current Move operation');
    if(!initial&&!this.connected)throw Error('Connect to Move first');
    this.pending=true;this.wireError=null;
    const request=uniqueId('request'),value=`${this.client}|${request}|${op}|${file}|${revision}`;
    let accepted=false,lastSent=0,lastError='';const until=Date.now()+60000;
    try {
      while(Date.now()<until){
        if(!accepted&&Date.now()-lastSent>=750){this.send('composer_command',value);lastSent=Date.now();}
        if(this.wireError)throw Error(this.wireError);
        await new Promise(r=>setTimeout(r,80));
        try {
          const status=await(await this.file(`${this.root}/replies/${this.client}-${request}.json`)).json();
          if(status.client!==this.client||status.request!==request)continue;
          if(this.epoch&&this.epoch!==status.epoch)throw Error('Thru restarted during the operation');
          this.root=status.root;this.revision=status.revision;this.preview=!!status.preview;
          if(status.ok||!['samples','sample','markers'].includes(op))this.onStatus(status);
          if(!status.ok)throw Object.assign(Error(status.message),{device:true});return status;
        }catch(error){
          if(error.device||error.message.includes('restarted')||error.status&&error.status!==404)throw error;
          if(error.status!==404)lastError=error.message;
        }
        if(op!=='hello'){
          const status=await this.poll().catch(error=>{lastError=error.message;return null;});
          if(status?.client===this.client&&status.request===request){accepted=true;if(!status.ok)throw Object.assign(Error(status.message),{device:true});}
        }
        if(this.ws.readyState!==1)throw Error('Move disconnected');
      }
      throw Error(lastError||'Thru did not answer. Keep takeover open, then reconnect.');
    } finally {this.pending=false;}
  }
  cancel(){return this.command('cancel');}
  close(){
    this.closed=true;clearTimeout(this.timer);clearInterval(this.heartbeat);
    if(this.ws?.readyState===1&&this.preview)this.send('composer_command',`${this.client}|${uniqueId('leave')}|cancel||${this.revision}`);
    this.ws?.close();
  }
}
