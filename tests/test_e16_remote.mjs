import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const context={};vm.createContext(context);
vm.runInContext(fs.readFileSync(new URL('../src/e16-remote.inc.js',import.meta.url),'utf8')+';globalThis.protocol=e16RemoteProtocol;',context);
const p=context.protocol;
const unpack=packet=>packet.flatMap((v,i)=>i%4===0?packet.slice(i+1,i+1+([5,6,7].includes(v)?v-4:3)):[]);
const raw=packet=>{const b=unpack(packet);return Array.from(p.unpack(b.slice(b[6]===6?8:7,-1)));};
const command=packet=>{const b=unpack(packet);return b[6]===6?b[7]:b[6];};
function fixture(){
 let time=0,blocked=false,ownerBlocked=false,pairs=0;const sent=[],owners=[],buttons=[],errors=[],applied=p.tiles.map(()=>Array(20).fill(0));
 const remote=p.create({now:()=>time,send:packet=>{if(blocked)return false;sent.push(Array.from(packet));return true;},
  onOwnership:(a,r)=>{owners.push([a,r]);return !ownerBlocked;},onButtons:mask=>buttons.push(mask),onPair:()=>pairs++,onError:e=>errors.push(e)});
 const feed=(op,data=[])=>remote.feed(Array.from(p.sysex(op,data)));
 function ack(packet,status=0){
  const cmd=command(packet);
  if(cmd===8){
   const data=raw(packet),[x,y,w,h]=data,stride=Math.ceil(w/8);
   assert.equal(data.length,4+stride*h,'Row-aligned rectangle payload');
   assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=128&&y+h<=64);
   if(!status){
    const i=p.tiles.findIndex(t=>x>=t.x&&y>=t.y&&x+w<=t.x+t.w&&y+h<=t.y+t.h);assert(i>=0,'Patch stays inside its text tile');
    const t=p.tiles[i];
    for(let dy=0;dy<h;dy++)for(let dx=0;dx<w;dx++){
     const xx=x-t.x+dx,yy=y-t.y+dy,at=yy*4+(xx>>3),mask=128>>(xx&7);
     if(data[4+dy*stride+(dx>>3)]&(128>>(dx&7)))applied[i][at]|=mask;else applied[i][at]&=~mask;
    }
   }
   feed([status?84:83],[8,status,...data.slice(0,4)]);
  }else if(cmd===85)feed([6,83]);else if(cmd===7)feed([83],[7,0,255,255,255,255]);
 }
 function tick(ms=33,auto=true){
  time+=ms;const n=sent.length;remote.tick();const packets=sent.slice(n);
  assert(packets.reduce((n,a)=>n+a.length/4,0)<=24,'At most 24 USB packets per tick');
  assert(packets.every(a=>a.length<=48),'At most 12 USB packets per message');
  assert(remote.diagnostics.pixels<=320,'Never exceed the working 320-pixel queue');
  assert(remote.diagnostics.inflight<=8,'Bound the count of small outstanding patches');
  if(auto)packets.forEach(a=>ack(a));return packets;
 }
 function connect(){tick(0);feed([6,83]);feed([83],[7,0,255,255,255,255]);assert(remote.ready);}
 return {remote,sent,owners,buttons,errors,applied,feed,ack,tick,connect,get time(){return time;},set blocked(v){blocked=v;},set ownerBlocked(v){ownerBlocked=v;},get pairs(){return pairs;}};
}
let cells=Array.from({length:16},(_,i)=>({label:i<8?'POS':'FLD',text:'50%',raw:8192,rgb:i<8?[38,22,3]:[2,26,22]}));
const f=fixture(),{remote,sent,owners,feed}=f;
f.tick(0,false);assert.equal(command(sent[0]),85);assert.equal(owners.length,0,'A probe cannot reserve another device’s MIDI channel');
const hello=Array.from(p.sysex([6,83]));remote.feed(hello.slice(0,4));remote.feed([248,...hello.slice(4)]);
assert(remote.active&&!remote.ready);assert.deepEqual(owners[0],[false,true]);assert.equal(command(sent.at(-1)),7);
feed([83],[7,0,255,255,255,255]);assert(remote.ready);assert.deepEqual(owners.at(-1),[true,true]);
remote.setView({title:'A MOTN / B GRNS',cells});
for(let i=0;i<60;i++)f.tick();
const rings=sent.filter(packet=>command(packet)===4).flatMap(packet=>{const a=raw(packet),out=[];for(let i=0;i<a.length;i+=7)out.push(a.slice(i,i+7));return out;});
assert.equal(rings.length,16);assert.equal(new Set(rings.map(x=>x[0])).size,16);
for(const r of rings){assert.deepEqual(r.slice(1,4),r[0]<8?[38,22,3]:[2,26,22]);assert.equal(r[4]*128+r[5],8192);}
let count=sent.length;for(let i=0;i<10;i++)f.tick();assert(sent.slice(count).every(a=>command(a)===85),'Steady views and heartbeat ACKs never redraw');
cells=cells.map((c,i)=>i===9?{...c,text:'1.23X',raw:10000}:c);
remote.feed([176,10,1]);remote.setView({title:'A MOTN / B GRNS',cells});count=sent.length;
for(let i=0;i<4;i++)f.tick();
let changed=sent.slice(count).filter(a=>command(a)!==85);assert.equal(changed.length,2);
assert.equal(command(changed[0]),4);assert.equal(raw(changed[0])[0],9);
assert.equal(command(changed[1]),8);assert(raw(changed[1])[0]>=32&&raw(changed[1])[1]>=42&&raw(changed[1])[2]<=32&&raw(changed[1])[3]<=5);
assert(raw(changed[1]).length<24,'Trim blank pixel padding');
// Host refusal cannot confirm any ring/tile; all changes survive the refusal.
cells=cells.map((c,i)=>i===9?{...c,text:'1.24X',raw:10100}:c);remote.setView({title:'A MOTN / B GRNS',cells});
f.blocked=true;count=sent.length;f.tick();assert.equal(sent.length,count);f.blocked=false;for(let i=0;i<4;i++)f.tick();assert.equal(sent.length,count+2);
// Two outstanding rectangles maximum; dirty history is coalesced, not queued.
const queue=fixture();queue.connect();queue.remote.setView({title:'PAGE SPANNING ALL FOUR TILES',cells});
for(let i=0;i<20;i++)queue.tick(33,false);
assert(queue.remote.diagnostics.inflight>=2&&queue.remote.diagnostics.inflight<=8);
assert.equal(queue.sent.filter(a=>command(a)===8).length,queue.remote.diagnostics.inflight);
for(let i=0;i<100;i++){cells=cells.map((c,n)=>n===9?{...c,raw:11000+i,text:String(i)}:c);queue.remote.setView({title:'PAGE SPANNING ALL FOUR TILES',cells});}
queue.sent.filter(a=>command(a)===8).forEach(a=>queue.ack(a));
let applied=queue.sent.length;for(let i=0;i<60;i++)queue.tick();
assert.equal(queue.remote.diagnostics.inflight,0);
assert.deepEqual(Array.from(queue.applied,a=>Array.from(a)),Array.from(p.render({title:'PAGE SPANNING ALL FOUR TILES',cells}),a=>Array.from(a)),'Every tile eventually shows the latest desired view');
assert(queue.sent.slice(applied).filter(a=>command(a)===4).some(a=>{const data=raw(a);for(let j=0;j<data.length;j+=7)if(data[j]===9&&data[j+4]*128+data[j+5]===11099)return true;return false;}));
// All changed heading tiles finish before any new labels, values or rings.
// Shift+turn events must not promote encoder 1's value ahead of the heading.
const head=fixture();head.connect();
const headView={title:'ENGINE A/B 1/12',pageKey:'0',cells};head.remote.setView(headView);
for(let i=0;i<70;i++)head.tick();
head.remote.feed([144,16,127]);
head.remote.setView({...headView,title:'TURN1 12/12 TAPE CHANGED HEADING',pageKey:'11',cells:cells.map(c=>({...c,label:'TAPE',text:'25%',raw:4096}))});
const headingPackets=[];
for(let i=0;i<12;i++){
 head.remote.feed([176,1,1]);
 for(const packet of head.tick(33,false)){
  assert.equal(command(packet),8,'Heading precedes rings');
  assert.equal(raw(packet)[1],0,'Heading precedes parameter text');headingPackets.push(packet);
 }
}
assert(headingPackets.length>=2&&headingPackets.length<=8,'Heading respects the pixel window');
for(const packet of headingPackets)head.ack(packet);
const expectedHeading=Array.from(p.render({...headView,title:'TURN1 12/12 TAPE CHANGED HEADING'}).slice(0,4),a=>Array.from(a));
for(let i=0;i<8&&JSON.stringify(head.applied.slice(0,4))!==JSON.stringify(expectedHeading);i++){
 for(const packet of head.tick(33,false)){assert.equal(command(packet),8);assert.equal(raw(packet)[1],0);head.ack(packet);}
}
assert.deepEqual(Array.from(head.applied.slice(0,4),a=>Array.from(a)),expectedHeading);
const bodyStart=head.sent.length;for(let i=0;i<80;i++)head.tick();
assert(head.sent.slice(bodyStart).some(a=>command(a)===4),'Rings follow the acknowledged heading');
assert(head.sent.slice(bodyStart).some(a=>command(a)===8&&raw(a)[1]>0),'Parameter text follows the acknowledged heading');
// Coalesce two page changes while the earlier heading is awaiting ACK.
const rapid=fixture();rapid.connect();rapid.remote.setView({title:'OLD HEADING ACROSS FOUR TILES',pageKey:'0',cells});
const oldHeading=[];for(let i=0;i<2;i++)oldHeading.push(...rapid.tick(33,false));
rapid.remote.setView({title:'MIDDLE PAGE',pageKey:'1',cells});
rapid.remote.setView({title:'LATEST HEADING AFTER FAST TURNS',pageKey:'2',cells});
oldHeading.forEach(a=>rapid.ack(a));
for(let i=0;i<10;i++){
 const packets=rapid.tick(33,false);
 for(const packet of packets){if(command(packet)===4)break;assert.equal(raw(packet)[1],0);rapid.ack(packet);}
 if(command(packets[0]||[])===4)break;
}
assert.deepEqual(Array.from(rapid.applied.slice(0,4),a=>Array.from(a)),Array.from(p.render({title:'LATEST HEADING AFTER FAST TURNS',cells}).slice(0,4),a=>Array.from(a)));
// A NACK retries only the affected tile; repeated NACKs reconnect, never
// permanently disable controls. Unsolicited/stale NACKs do nothing.
const n=fixture();n.connect();n.remote.setView({title:'TEST',cells:Array(16).fill(null)});
const tile=n.tick(33,false).find(a=>command(a)===8);assert(tile);n.ack(tile,3);
assert(n.remote.ready);assert.equal(n.remote.diagnostics.pixelWindow,160);
for(let i=0;i<20;i++)n.feed([84],[8,3,96,56,32,5]);assert.equal(n.remote.diagnostics.nacks,1);
for(let i=0;i<20&&n.remote.diagnostics.reconnects===0;i++)for(const a of n.tick(120,false))if(command(a)===8)n.ack(a,3);
assert.equal(n.remote.diagnostics.reconnects,1);assert.equal(n.remote.diagnostics.state,'RECONNECT');
n.tick(1000);assert(n.remote.active&&!n.remote.ready);n.feed([83],[7,0,255,255,255,255]);assert(n.remote.ready);
for(let i=0;i<60;i++)n.tick();assert.equal(n.remote.diagnostics.state,'LIVE');assert.equal(n.remote.diagnostics.issue,'TILE NACK');
// Missing ACK retries the identical payload, even if the desired view changes.
const lost=fixture();lost.connect();lost.remote.setView({title:'OLD',cells:Array(16).fill(null)});
const original=lost.tick(33,false).find(a=>command(a)===8);
lost.remote.setView({title:'NEW',cells:Array(16).fill(null)});
const retry=lost.tick(1000,false).find(a=>command(a)===8);assert.deepEqual(retry,original);
lost.ack(retry);const beforeLate=lost.remote.diagnostics.acks;lost.ack(original);assert.equal(lost.remote.diagnostics.acks,beforeLate);
for(let i=0;i<180;i++)lost.tick();assert(lost.remote.ready);assert.equal(lost.remote.diagnostics.inflight,0);
// Missing Clear ACK and host backpressure recover automatically.
const clear=fixture();clear.tick(0,false);clear.blocked=true;clear.feed([6,83]);assert(clear.remote.active&&!clear.remote.ready);
clear.tick(33,false);clear.blocked=false;assert.equal(command(clear.tick(33,false)[0]),7);
for(let i=0;i<8&&clear.remote.diagnostics.reconnects===0;i++)clear.tick(1000,false);
assert.equal(clear.remote.diagnostics.issue,'CLEAR TIMEOUT');clear.tick(1000);clear.feed([83],[7,0,255,255,255,255]);assert(clear.remote.ready);
// Clear-screen rejection also reconnects instead of leaving a disabled client.
const rejected=fixture();rejected.tick(0,false);rejected.feed([6,83]);rejected.feed([84],[7,3,255,255,255,255]);
assert.equal(rejected.remote.diagnostics.issue,'CLEAR NACK');assert(!rejected.remote.active);
rejected.tick(1000);rejected.feed([83],[7,0,255,255,255,255]);assert(rejected.remote.ready);
// A completely unresponsive controller is not flooded; reconnect starts a new
// bounded handshake. Disconnect/replug does not require reopening takeover.
const silent=fixture();silent.connect();silent.remote.setView({title:'OLD',cells});
for(let i=0;i<9;i++)silent.tick(1000,false);
assert(silent.remote.diagnostics.reconnects>=1);assert(silent.sent.length<16);
for(let i=0;i<180;i++)silent.tick();assert(silent.remote.ready);
// Sweep all 12 page families forwards and backwards with delayed ACKs, while
// button/turn messages continue. The last page has no distinct terminal state.
const nav=fixture();nav.connect();const due=[];let observed=nav.sent.length;
for(let i=0;i<480;i++){
 const page=i<240?Math.floor(i/20)%12:11-Math.floor((i-240)/20)%12;
 nav.remote.feed([144,16,127,176,1,i<240?1:127]);
 nav.remote.setView({title:'PAGE '+page,pageKey:String(page),cells:cells.map(c=>({...c,text:String(page)}))});
 nav.tick(33,false);
 for(const packet of nav.sent.slice(observed))due.push({packet,at:nav.time+150});observed=nav.sent.length;
 for(let j=due.length-1;j>=0;j--)if(due[j].at<=nav.time){nav.ack(due[j].packet);due.splice(j,1);}
 assert(nav.remote.diagnostics.pixels<=320);assert(nav.remote.ready);
}
for(const {packet} of due)nav.ack(packet);for(let i=0;i<80;i++)nav.tick();
assert.equal(nav.remote.diagnostics.reconnects,0);assert.equal(nav.remote.diagnostics.inflight,0);
assert.deepEqual(Array.from(nav.applied,a=>Array.from(a)),Array.from(p.render({title:'PAGE 0',cells:cells.map(c=>({...c,text:'0'}))}),a=>Array.from(a)),'Final backwards page is fully drawn');
assert.equal(nav.buttons.at(-1),65536);nav.remote.feed([128,16,0]);assert.equal(nav.buttons.at(-1),0);
// A full page completes in under one second with 33 ms UI ticks and replies.
// This measures acknowledged screen contents, rather than queued sends.
const perf=fixture();perf.connect();
const perfBefore={title:'1/12 ENGINE A/B',pageKey:'0',cells:Array.from({length:16},()=>({label:'FREQ',text:'1000MS',raw:15000,rgb:[20,10,5]}))};
perf.remote.setView(perfBefore);for(let i=0;i<80;i++)perf.tick();
const perfAfter={title:'2/12 COLOR A/B',pageKey:'1',cells:perfBefore.cells.map(c=>({...c,label:'DRIV',text:'48KHZ',raw:5000}))};
perf.remote.setView(perfAfter);const perfExpected=JSON.stringify(p.render(perfAfter)),start=perf.time,awaiting=[];let finished=0;
for(let i=0;i<60;i++){
 for(const packet of perf.tick(33,false))awaiting.push({packet,at:perf.time+33});
 for(let j=awaiting.length-1;j>=0;j--)if(awaiting[j].at<=perf.time){perf.ack(awaiting[j].packet);awaiting.splice(j,1);}
 if(JSON.stringify(perf.applied)===perfExpected){finished=perf.time-start;break;}
}
assert(finished>0&&finished<1000,'A 30 Hz full page with one-tick ACKs should finish below 1 s: '+finished);
// Shorter text, disappearing cells and reserved squares must erase old pixels.
const sparse={title:'X',pageKey:'2',cells:perfAfter.cells.map((c,i)=>i%3===0?null:i%3===1?{reserved:true}:{...c,label:'I',text:'1'})};
perf.remote.setView(sparse);awaiting.forEach(({packet})=>perf.ack(packet));for(let i=0;i<80;i++)perf.tick();
assert.deepEqual(Array.from(perf.applied,a=>Array.from(a)),Array.from(p.render(sparse),a=>Array.from(a)));
assert(perf.sent.some(a=>command(a)===8&&raw(a)[2]%8!==0),'Exercise byte-aligned rows with a non-byte width');
// Repeated synchronous calls cannot bypass the real-time packet-rate budget.
const budget=fixture();budget.connect();budget.remote.setView(perfBefore);let budgetStart=budget.sent.length;
for(let i=0;i<100;i++)budget.tick(0);
assert(budget.sent.slice(budgetStart).reduce((n,a)=>n+a.length/4,0)<=24);
budgetStart=budget.sent.length;budget.tick(500);assert(budget.sent.slice(budgetStart).reduce((n,a)=>n+a.length/4,0)<=24,'Idle time cannot bank an unbounded burst');
console.log('s3g Thru Remote acknowledged full page at 30 Hz / 33 ms replies: '+finished+' ms; sparse clearing and rate limiting: PASS');
// Fresh presses, velocity-zero release, explicit Exit and refused Exit retry.
remote.feed([144,16,127,144,0,127]);assert.equal(f.pairs,1);assert.equal(f.buttons.at(-1),65537);
remote.feed([144,0,127]);assert.equal(f.pairs,1);remote.feed([128,0,0,144,16,0]);assert.equal(f.buttons.at(-1),0);
remote.feed([144,16,127,248,144,15,127]);assert(!remote.active&&!remote.ready);assert.equal(command(sent.at(-1)),0);
count=sent.length;f.tick(10000);assert.equal(sent.length,count,'Explicit exit stays off');
remote.resume();f.tick(0);feed([83],[7,0,255,255,255,255]);assert(remote.ready);
f.blocked=true;remote.enable(false);assert.deepEqual(owners.at(-1),[false,true]);f.tick();
f.blocked=false;f.tick();assert.equal(command(sent.at(-1)),0);assert.deepEqual(owners.at(-1),[false,false]);
const reserved=p.render({title:'',cells:[{reserved:true},...Array(15).fill(null)]});
assert(reserved[5].some(x=>x!==0)&&reserved[4].every(x=>x===0));assert(reserved.slice(6).every(tile=>tile.every(x=>x===0)));
console.log('s3g Thru Remote: 320-pixel window, cropped rectangles, packet-rate pacing, coalescing, delayed/missing ACKs, NACK recovery, replug, heading-first navigation, full page sweep, buttons and explicit Exit: PASS');
