import assert from 'node:assert/strict';
import {MoveConnection,PANEL_PROTOCOL,PANEL_BUILD} from '../src/assets/composer/protocol.mjs';
globalThis.location={protocol:'http:',host:'move.test:7700'};
globalThis.document={cookie:'csrf_token=token%2Bvalue'};
const writes=[],replies=new Map();let currentRevision='10',uploads=0,dropPackets=0;
globalThis.WebSocket=class{
 constructor(url){assert.equal(url,'ws://move.test:7700/ws/remote-ui');this.readyState=1;queueMicrotask(()=>this.onopen?.());}
 send(text){const msg=JSON.parse(text);writes.push(msg);assert.equal(msg.type,'set_param');assert.equal(msg.key,'overtake_dsp:composer_command');assert(new TextEncoder().encode(msg.value).length<224);const[client,request,op,,revision]=msg.value.split('|');if(op==='heartbeat')return;if(dropPackets>0){--dropPackets;return;}const result={protocol:1,epoch:'test-epoch',client,request,phase:'ready',panelVersion:'',panelProtocol:PANEL_PROTOCOL,panelBuild:PANEL_BUILD,ok:!['sample','samples','markers'].includes(op)&&(op!=='preview'||revision===currentRevision),message:revision===currentRevision?'OK':'Project changed',revision:currentRevision,activeClip:0,playing:false,preview:op==='preview'&&revision===currentRevision,progress:0,root:'/data/UserData/UserLibrary/Samples/s3g Thru/Composer',result:''};replies.set(`${client}-${request}.json`,result);}
 close(){this.readyState=3;this.onclose?.();}
};
globalThis.fetch=async(url,init)=>{
 if(url==='/files/upload'){uploads++;assert.equal(init.headers['X-CSRF-Token'],'token+value');assert.equal(init.body.get('path'),'/data/UserData/UserLibrary/Samples/s3g Thru/Composer/uploads');assert(init.body.get('file').name.endsWith('.json'));return new Response('{}');}
 const path=new URL(url,'http://move.test:7700').searchParams.get('path');assert(path.startsWith('/data/UserData/UserLibrary/Samples/s3g Thru/Composer/'));const name=path.split('/').at(-1);if(name==='status.json')return Response.json({protocol:1,epoch:'test-epoch',client:connection.client,revision:currentRevision,ok:true});const result=replies.get(name);return result?Response.json(result):new Response('',{status:404});
};
const observations=[];const connection=new MoveConnection(s=>observations.push(s));await connection.connect();assert.equal(connection.revision,'10');const filename=await connection.upload(new Blob(['{}']),'.json');assert.match(filename,/^upload-[a-f0-9]+\.json$/);assert.equal(uploads,1);
// A new live revision must never replace the draft's expected base revision.
currentRevision='11';await connection.poll();assert.equal(connection.revision,'11');await assert.rejects(()=>connection.command('preview','draft.json',{revision:'10'}),/Project changed/);assert(writes.at(-1).value.endsWith('|10'));assert(!connection.pending);
await connection.command('preview','draft.json',{revision:'11'});assert(!writes.some(m=>m.type==='subscribe'||m.type==='subscribe_tool'||m.key==='overtake_dsp:state'));assert(!writes.some(m=>new TextEncoder().encode(m.value).length>=256));dropPackets=1;const start=writes.length;await connection.command('cancel');const retried=writes.slice(start).filter(m=>m.value.includes('|cancel|'));assert.equal(retried.length,2);assert.equal(retried[0].value,retried[1].value);assert(!connection.pending);
// A rejected sample stays a dialog error without disconnecting a healthy live panel.
for(const op of ['samples','sample','markers']){const before=observations.length;await assert.rejects(()=>connection.command(op,'request.json'),/OK/);assert(!observations.slice(before).some(s=>s.ok===false));assert(connection.connected&&!connection.pending);}
const hash='a'.repeat(64);connection.rememberAssets({assets:[{sha256:hash,url:'http://move.test:7700/files/download?path='+encodeURIComponent(connection.root+'/cache/'+hash+'.wav')}]},true);assert(connection.cachedAssets.has(hash));connection.close();
console.log('Composer wire protocol: short ordered commands, no state subscriptions, CSRF upload, reply identity and stale-project protection: PASS');
