#!/usr/bin/env node
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const here=path.dirname(fileURLToPath(import.meta.url));
const aliasBase=3456;
const pages=[
    ['ENGINE',['ALG','POS','FLD','SPD','STK','SCAN','JOIN','LVL']],
    ['STACK',['PATH','BLND','SHAP','OFFS','SKEW','CURV','SLEW','']],
    ['ENG 1',['P1','P2','P3','P4','P5','P6','P7','P8']],
    ['ENG 2',['P1','P2','P3','P4','P5','P6','P7','P8']],
    ['ENG 3',['P1','P2','P3','P4','P5','P6','P7','P8']],
    ['ENG 4',['P1','P2','P3','P4','P5','P6','P7','P8']],
    ['SENDS',['ADLY','AREV','BDLY','BREV','PRIO','THR','CURV','XFD']],
    ['COLOR',['TONE','DRIV','BITS','RATE','','','','']],
    ['ROTARY',['X-PN','RATE','DPTH','SPRD','RAMP','BAL','XOVR','MIX']],
    ['DELAY',['TIME','FDBK','WDTH','','','','','MIX']],
    ['REVERB',['ALG','DEC','PRE','DAMP','DIFF','MOD','WDTH','MIX']],
    ['TAPE',['FMT','DRIV','AGE','CRNK','MECH','INRT','HISS','MIX']],
];
const sharedPage=page=>page===6||page>=9;
const off={instrument:127,parameter:0,type:0,display:0,mode:0,channel:0,
    lower:0,upper:127,nr1:0,nr2:0,output:12,scriptId:0,defaultValue:0};
const disabled=()=>({name:'',abbr:'',color:0,color2:0,push_action:{...off},
    turn_actions:[{...off},{...off}]});
const scene={title:'THRU NM',icon:Array.from({length:32},(_,i)=>i<2||i>=30?0:0x66),
    selectedPreset:0,color:0,transmitMode:0,pcOnEntry:0,bankOnEntry:65535,
    recentPage:0,recentPreset:0,transmitOnSceneEntry:0,transmitOnPageSwitch:0,
    transmitOnPresetLoad:0,smartTransmit:15,outputOnEntry:0,holdMode:0,
    acceleration:0,code:{code:'',fullScript:'',scriptName:''},pages:[]};

scene.pages=pages.map(([title,labels],page)=>({title,output:3,channel:1,
    encoders:Array.from({length:16},(_,physical)=>{
        if(sharedPage(page)&&physical>=8)return disabled();
        const slot=physical%8,label=labels[slot];
        if(!label)return disabled();
        const channel=physical<8?5:6,id=aliasBase+page*8+slot;
        // Raw zero is reserved as E16's uninitialized scene value. Thru
        // rejects it, so scene entry cannot replace a restored instrument
        // with the minimum value of every control.
        const turn={...off,type:9,display:11,mode:3,channel,lower:1,upper:16383,
            nr1:id&127,nr2:id>>7,defaultValue:0};
        const resetCc=page*8+slot,resetChannel=physical<8?14:15;
        // A CC push on isolated A/B control channels leaves E16's selected
        // NRPN turn address untouched and reaches Thru through Schwung.
        const push={...off,type:2,mode:0,channel:resetChannel,lower:0,upper:127,
            nr1:resetCc,nr2:127,defaultValue:0};
        return {name:label,abbr:label,color:sharedPage(page)?30:physical<8?30:100,
            color2:0,push_action:push,turn_actions:[turn,{...off}]};
    })}));

assert.equal(scene.pages.length,12);
assert(scene.code.code===''&&scene.code.fullScript===''&&scene.code.scriptName==='');
for(const page of scene.pages)assert.equal(page.encoders.length,16);
const target=path.join(here,'Scenes','THRU NRPN MNLT.oxie16');
const text=JSON.stringify(scene);
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(target,'utf8'),text,'native scene is stale');
else {fs.mkdirSync(path.dirname(target),{recursive:true});fs.writeFileSync(target,text);}
console.log('s3g Thru E16 NRPN + MNLT scene: PASS (12 pages, no Lua or Remote)');
