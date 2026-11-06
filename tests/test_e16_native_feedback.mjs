import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {spawnSync} from 'node:child_process';
const source=fs.readFileSync(new URL('../src/e16-native-feedback.inc.js',import.meta.url),'utf8');
const ctx=vm.createContext({sendExternal:()=>true});
vm.runInContext(source+';globalThis.make=createE16NativeFeedback;',ctx);
const messages=[];let accept=true;
const feedback=ctx.make(p=>{assert(p.length<=48);if(!accept)return false;messages.push(p);return true;});
const values=Array.from({length:16},(_,i)=>[i,Math.round(i*16383/15),i<8?30:100]);
const snapshot={v:1,page:2,token:4,values};
feedback.update(snapshot);feedback.tick();feedback.tick();feedback.tick();feedback.tick();
assert.equal(messages.length,3);
const decode=p=>{const b=[];for(let i=0;i<p.length;i+=4){const cin=p[i]&15;assert(cin>=4&&cin<=7);b.push(...p.slice(i+1,i+1+(cin===4?3:cin-4)));}return b;};
const bytes=messages.map(decode);assert(bytes.every(b=>b[0]===240&&b.at(-1)===247));
const entries=bytes.flatMap(b=>{assert.deepEqual(b.slice(1,7),[125,83,84,1,2,4]);const a=[];for(let i=7;i<b.length-1;i+=4)a.push([b[i],b[i+1]*128+b[i+2],b[i+3]]);return a;});
assert.deepEqual(entries,values);
feedback.update(snapshot);feedback.tick();assert.equal(messages.length,3);
values[7]=[7,8192,30];feedback.update(snapshot);feedback.tick();assert.equal(messages.length,4);
assert.equal(decode(messages.at(-1)).length,12);
feedback.update({...snapshot,token:5});feedback.tick();
feedback.update({...snapshot,page:3,token:6,values:[[9,0,100]]});feedback.tick();
assert.deepEqual(decode(messages.at(-1)).slice(5,7),[3,6]);
assert.equal(decode(messages.at(-1))[7],9);
accept=false;feedback.update({...snapshot,page:3,token:6,values:[[9,4000,100]]});feedback.tick();
const before=messages.length;accept=true;feedback.tick();assert.equal(messages.length,before+1);
feedback.reset();feedback.update({v:1,page:-1,token:6,values});feedback.tick();assert.equal(messages.length,before+1);
console.log('s3g Thru E16 feedback: bounded SysEx, exact values, coalescing, stale-page discard and retry: PASS');
const lua=spawnSync('lua',['-',new URL('../controllers/oxi-e16-monolit/rings.lua',import.meta.url).pathname],{encoding:'utf8',input:`
local pg=1
local sent={}
local rings={}
controller={getPage=function() return pg end}
page={}
system={setUpdateRate=function(n) assert(n==1000) end}
midi={sendCC=function(port,ch,cc,value)
  assert(port==3 and ch==13 and (cc==125 or cc==126))
  sent[#sent+1]={cc,value};if #sent>2 then table.remove(sent,1) end
end}
leds={updateByIndex=function(i,v,c) assert(i>=1 and i<=16);rings[i]={v,c} end}
collectgarbage('collect')
local initial=collectgarbage('count')
dofile(arg[1]);page.onInit()
assert(sent[1][1]==125 and sent[2][1]==126 and sent[2][2]==1)
local function data(pageIndex,token,value)
 return {240,125,83,84,1,pageIndex,token,0,value//128,value%128,30,247}
end
controller.onSysex(data(0,1,8192));assert(rings[1][1]==8192)
controller.onSysex(data(0,0,100));assert(rings[1][1]==8192)
controller.onSysex(data(1,1,100));assert(rings[1][1]==8192)
pg=2;page.onPageChange(1,2);assert(rings[1][1]==0)
controller.onSysex(data(0,1,16383));assert(rings[1][1]==0)
controller.onSysex(data(1,2,16383));assert(rings[1][1]==16383)
pg=1;page.onPageChange(2,1);controller.onSysex(data(0,1,100));assert(rings[1][1]==0)
controller.onSysex(data(0,3,4000));assert(rings[1][1]==4000)
system.update();assert(rings[1][1]==4000)
assert(controller.onEncoderTurn==nil and controller.onEncoderPress==nil)
for i=1,7200 do pg=i%12+1;page.onPageChange(1,pg);controller.onSysex(data(pg-1,sent[#sent-1][2],i%16384)) end
collectgarbage('collect');assert(collectgarbage('count')-initial<12)
print('s3g Thru tiny E16 ring receiver: native turns untouched, matching page/token only, bounded memory: PASS')
`});
assert.equal(lua.status,0,lua.stdout+lua.stderr);process.stdout.write(lua.stdout);
