#!/usr/bin/env node
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const scene=JSON.parse(fs.readFileSync(path.join(root,'controllers/oxi-e16-nrpn/Scenes/THRU NRPN MNLT.oxie16')));
assert.equal(scene.title,'THRU NM');
assert.deepEqual(scene.code,{code:'',fullScript:'',scriptName:''});
assert.equal(scene.pages.length,12);
assert.deepEqual(scene.pages.map(page=>page.title),
    ['ENGINE','STACK','ENG 1','ENG 2','ENG 3','ENG 4','SENDS','COLOR','ROTARY','DELAY','REVERB','TAPE']);
assert.deepEqual(scene.pages[6].encoders.slice(0,8).map(control=>control.abbr),
    ['ADLY','AREV','BDLY','BREV','PRIO','THR','CURV','XFD']);
for(let page=0;page<scene.pages.length;++page) {
    assert.equal(scene.pages[page].encoders.length,16);
    for(let physical=0;physical<16;++physical) {
        const control=scene.pages[page].encoders[physical];
        const shared=page===6||page>=9;
        if(shared&&physical>=8) {
            assert.equal(control.turn_actions[0].type,0);assert.equal(control.push_action.type,0);continue;
        }
        if(!control.abbr) {
            assert.equal(control.turn_actions[0].type,0);assert.equal(control.push_action.type,0);continue;
        }
        const action=control.turn_actions[0],slot=physical%8,id=3456+page*8+slot;
        assert.equal(action.type,9);assert.equal(action.display,11);assert.equal(action.mode,3);
        assert.equal(action.lower,1);assert.equal(action.upper,16383);
        assert.equal(action.channel,physical<8?5:6);
        assert.equal(action.nr1,id&127);assert.equal(action.nr2,id>>7);
        const push=control.push_action,resetCc=page*8+slot;
        assert.equal(push.type,2);assert.equal(push.mode,0);
        assert.equal(push.channel,physical<8?14:15);
        assert.equal(push.nr1,resetCc);assert.equal(push.nr2,127);
    }
}
console.log('s3g Thru script-free E16 NRPN scene schema, guarded aliases and isolated CC default pushes: PASS');
