#!/usr/bin/env node
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const scene=JSON.parse(fs.readFileSync(path.join(root,'controllers/oxi-e16-monolit/Scenes/THRU E16 MNLT.oxie16')));
assert.equal(scene.title,'THRU EM');
assert.equal(scene.code.scriptName,'Thru Rings');
assert(Buffer.byteLength(scene.code.code)<2000);
assert(!scene.code.code.includes('onEncoderTurn'));
assert(!scene.code.code.includes('controller.set'));
assert(scene.code.code.includes('leds.updateByIndex'));
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
        const action=control.turn_actions[0],slot=physical%8,cc=page*8+slot;
        assert.equal(action.type,1);assert.equal(action.display,0);assert.equal(action.mode,3);
        assert.equal(action.lower,0);assert.equal(action.upper,127);
        assert.equal(action.channel,physical<8?14:15);
        assert.equal(action.nr1,cc);assert.equal(action.nr2,0);
        const push=control.push_action,resetCc=page*8+slot;
        assert.equal(push.type,2);assert.equal(push.mode,0);
        assert.equal(push.channel,physical<8?14:15);
        assert.equal(push.nr1,127);assert.equal(push.nr2,resetCc+1);assert.equal(push.upper,resetCc+1);
    }
}
console.log('s3g Thru native E16 relative CC scene with display-only feedback schema, safe startup and default pushes: PASS');
