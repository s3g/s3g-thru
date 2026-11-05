#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';

const here=path.dirname(fileURLToPath(import.meta.url));
const layout=JSON.parse(fs.readFileSync(path.join(here,'adaptive-layout.json'),'utf8'));
assert.deepEqual(layout.pages,['ENGINE','STACK','ENG 1','ENG 2','ENG 3','ENG 4','SENDS','COLOR','ROTARY','DELAY','REVERB','TAPE']);

const off={instrument:127,parameter:0,type:0,display:0,mode:0,channel:0,lower:0,upper:127,nr1:0,nr2:0,output:12,scriptId:0,defaultValue:0};
const icon=Array.from({length:32},(_,i)=>i<2||i>=30?0:0x66);
const scene={title:'THRU RM',icon,selectedPreset:0,color:0,transmitMode:0,pcOnEntry:0,bankOnEntry:65535,recentPage:0,recentPreset:0,
 transmitOnSceneEntry:0,transmitOnPageSwitch:0,transmitOnPresetLoad:0,smartTransmit:15,outputOnEntry:0,holdMode:0,acceleration:0,
 code:{code:'',fullScript:'',scriptName:''},pages:[]};
scene.pages=layout.pages.map((_,page)=>({title:page===0?'REMOTE':'P'+(page+1),output:3,channel:1,
 encoders:Array.from({length:16},()=>({name:'',abbr:'',color:0,push_action:{...off},turn_actions:[{...off},{...off}],color2:0}))}));

assert(/^[\x20-\x7e]{1,7}$/.test(scene.title),'E16 scene title must be 1-7 ASCII characters');
assert(scene.code.code===''&&scene.code.fullScript===''&&scene.code.scriptName==='','Remote starter must not attach Lua');
const target=path.join(here,'Scenes','THRU REMOTE.oxie16'),text=JSON.stringify(scene);
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(target,'utf8'),text,'Remote scene is stale');
else {fs.mkdirSync(path.dirname(target),{recursive:true});fs.writeFileSync(target,text);}
console.log('s3g Thru Remote starter: PASS (12 fixed pages, no E16 script)');
