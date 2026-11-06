// MIDI view feedback plus the official E16 Remote adapter. Remote uses the
// current high-resolution parameter map while rendering readable names and
// formatted values on E16 itself.
const midiSurface={query:'',epoch:null};
const E16_PAGE_JUMP_MASK=1<<17;
const remoteButtons={mask:0,sent:null,physical:0,pair:0,target:null,mode:0,pageJump:false,data:null};
function sendExternal(packets) {
    return typeof move_midi_external_send==='function' && move_midi_external_send(packets);
}
function createNrpnFeedback(send) {
    const pending=new Map(),urgent=new Map(),sent=new Map();
    return {
        reset(){pending.clear();urgent.clear();sent.clear();},
        update(values){
            for(const entry of values){
                if(!Array.isArray(entry)||(entry.length!==3&&entry.length!==4))continue;
                const [channel,id,value,priority=0]=entry;
                if(![channel,id,value].every(Number.isInteger)||channel<1||channel>16||id<0||id>=16383||value<0||value>16383)continue;
                const key=channel+':'+id;
                // Priority marks an explicit controller resync request. Send it
                // even when the same value was delivered before disconnection;
                // the newly connected E16 has not necessarily received it.
                if(priority===1){pending.delete(key);urgent.set(key,{channel,id,value});}
                else if(urgent.has(key))urgent.set(key,{channel,id,value});
                else if(sent.get(key)===value)pending.delete(key);
                else pending.set(key,{channel,id,value});
            }
        },
        tick(){
            // Three complete NRPNs fit the host's 12-packet output bound.
            for(let i=0;i<3;++i){
                const queue=urgent.size?urgent:pending;
                const first=queue.entries().next();if(first.done)return;
                const[key,{channel,id,value}]=first.value,status=0xb0|(channel-1);
                const packets=[];
                for(const[cc,v]of [[99,id>>7],[98,id&127],[6,value>>7],[38,value&127]])packets.push(0x0b,status,cc,v);
                if(send(packets)===false)return;
                sent.set(key,value);queue.delete(key);
            }
        },
    };
}
const midiFeedback=createNrpnFeedback(sendExternal);
let remoteModeAllowed=false;
function syncRemoteTransport(readDsp=false,force=false){
    if(readDsp){
        const raw=Number(getParam('midi_control_mode',PARAMS.midi_control_mode.value));
        if(Number.isFinite(raw))PARAMS.midi_control_mode.value=clamp(Math.round(raw),0,6);
    }
    const mode=Math.round(PARAMS.midi_control_mode.value);
    const allowed=mode===0||mode===4;
    if(force||allowed!==remoteModeAllowed){
        remoteModeAllowed=allowed;
        if(allowed)remoteE16.resume();else remoteE16.enable(false);
    }
    return allowed;
}
function resetMidiSurface(){
    midiSurface.query='';midiSurface.epoch=null;midiFeedback.reset();e16NativeFeedback.reset();
    syncRemoteTransport(true,true);
}
function pollMidiControls(){
    // Move's eight visible controls use midi_view. E16 has a separate
    // page snapshot and never changes Move's part or editing page.
    const keys=state.view==='chain'?CHAIN_PAGE.keys:activeControlKeys();
    const selection=[];
    for(const key of keys){
        const p=thruMidi.MIDI_BY_KEY.get(key);
        if(p&&!p.action)selection.push((p.scope==='part'?state.splitFocus:-1)+':'+p.id);
    }
    const query=selection.slice(0,8).join(';');
    if(query!==midiSurface.query){
        try{
            const result=typeof host_module_set_param_blocking==='function'
                ?host_module_set_param_blocking('midi_view',query,10):host_module_set_param('midi_view',query);
            if(result===false||result===0)return;
            midiSurface.query=query;
        }catch(_){return;}
    }
    let data;
    try{data=JSON.parse(String(getParam('midi_state','')));}catch(_){return;}
    if(data?.v!==1||!Array.isArray(data.values))return;
    if(Number.isInteger(data.epoch)&&data.epoch!==midiSurface.epoch){
        midiSurface.epoch=data.epoch;midiFeedback.reset();
    }
    for(const[part,id,value]of data.values){
        const p=thruMidi.MIDI_BY_ID.get(id);
        if(!p||!Number.isFinite(value)||!selection.includes(part+':'+id))continue;
        if(PARAMS[p.key]&&(part<0||part===state.splitFocus)){
            const control=controlForKey(p.key);
            if(control)PARAMS[p.key].value=clamp(value,control.min,control.max);
        }
    }
}
function tickMidiFeedback(){
    if(typeof move_midi_external_send!=='function')return;
    const remoteAllowed=syncRemoteTransport();
    if(remoteAllowed&&remoteE16.ready)flushRemoteButtons();
    if(remoteAllowed&&remoteE16.ready)try{updateRemoteE16(JSON.parse(String(getParam('e16_remote_state','{}'))));}catch(_){}
    remoteE16.tick();
    if(remoteE16.active)return;
    if(Math.round(PARAMS.midi_control_mode.value)===5){
        try{e16NativeFeedback.update(JSON.parse(String(getParam('e16_native_state','{}'))));}catch(_){}
        e16NativeFeedback.tick();return;
    }
    e16NativeFeedback.reset();
    try{const values=JSON.parse(String(getParam('midi_feedback','[]')));if(Array.isArray(values))midiFeedback.update(values);}catch(_){}
    midiFeedback.tick();
}
function e16Number(value,places=2){return String(Number(value.toFixed(places)));}
function e16Time(ms){return ms>=1000?e16Number(ms/1000,2)+'S':e16Number(ms,ms<10?2:0)+'MS';}
function e16Value(p,value){
    const key=p.key;
    if(key==='algorithm')return E16_ENGINES[Math.round(value)]||'?';
    if(key==='effect_cross_pan')return value<=-99.5?'NORMAL':value>=99.5?'SWAP':Math.abs(value)<.5?'MONO':Math.round(value)+'%';
    if(key==='effect_rotary_rate'&&value<.05)return 'STOP';
    if(key==='effect_rotary_balance')return value<=-99.5?'DRUM':value>=99.5?'HORN':Math.abs(value)<.5?'EVEN':(value>0?'H':'D')+Math.abs(Math.round(value));
    if(key==='effect_tape_hiss')return value<=p.min+.5?'OFF':Math.round(value)+'DB';
    if(key==='effect_tape_inertia')return value<=0?'OFF':e16Time(tapeInertiaSeconds(value)*1000);
    if(key==='stochastic_evolution')return value<=0?'HOLD':value>=1023.5?'CYC':e16Number(value,value<10?2:value<100?1:0);
    if(p.options)return String(p.options[Math.round(value-p.min)]??Math.round(value)).toUpperCase();
    if(['speed','stretch_ratio','grain_pitch','long_grain_pitch'].includes(key))return value.toFixed(2)+'X';
    const unit=({prism_blur:'ms',prism_decay:'ms',morph_blur:'ms',texture_length:'ms',
        effect_reverb_decay:'s',effect_reverb_pre:'ms',effect_reverb_damp:'Hz',
        prism_pitch:'st',morph_pitch:'st'})[key]||p.unit;
    if(unit==='ms')return e16Time(value);
    if(unit==='s')return e16Number(value,2)+'S';
    if(unit==='Hz')return value>=1000?e16Number(value/1000,1)+'KHZ':e16Number(value,value<10?2:1)+'HZ';
    if(unit==='st')return e16Number(value,1)+'ST';
    if(unit==='%')return Math.round(value)+'%';
    if(p.integer)return String(Math.round(value));
    if(p.min>=-1&&p.max<=2)return Math.round(value*100)+'%';
    if(p.max===100&&p.min>=-100)return Math.round(value)+'%';
    return e16Number(value,value<10?2:value<100?1:0);
}
function e16PartFamily(mode,part){return part===0?0:part===1?(mode===2?3:1):part===2?2:3;}
function e16PageView(data){
    if(data?.v!==2||data.active!==1||!Array.isArray(data.values)||!Array.isArray(data.alg)
        ||!Array.isArray(data.pages)||!Array.isArray(data.rows))return null;
    const shared=data.shared===1,multi=data.mode!==0;
    const cells=Array.from({length:16},(_,i)=>i<(shared||!multi?8:16)?{reserved:true}:null);
    for(const[index,raw,id,part,steps,max]of data.values){
        const p=thruMidi.MIDI_BY_ID.get(id);if(!p||!Number.isInteger(index)||index<0||index>15||!Number.isInteger(raw)||raw<0||raw>16383||!Number.isFinite(max)||max<p.min)continue;
        const ratio=raw/16383,value=p.curve==='log'?p.min*Math.pow(max/p.min,ratio):p.min+(max-p.min)*ratio;
        let label=E16_LABELS[id]||p.name.slice(0,4),text;
        if(p.key==='algorithm')label='ALG';
        if(p.key==='manual_lane'){label=data.alg[part]===3?'LANE':'STK';text=steps?String(Math.round(value)):value.toFixed(2);}
        else text=e16Value(p,value);
        const namedPart=/^split_(delay|reverb)_([abcd])$/.exec(p.key);
        const colorPart=namedPart?namedPart[2].charCodeAt(0)-97:part;
        cells[index]={label:label.trim(),text:String(text).slice(0,7),raw,bipolar:p.min<0&&max>0,family:colorPart<0&&shared?4:e16PartFamily(data.mode,Math.max(0,colorPart))};
    }
    const pair=data.mode===3?data.pair:0,left=pair===2?'C':'A',right=pair===2?'D':'B';
    const number=(data.page+1)+'/12';
    const page=E16_PAGES[data.page]||'ENGINE';let title;
    if(data.shift)title='TURN1 '+number+' '+page;
    else if(data.page>=2&&data.page<=5){
        const name=i=>data.rows[i]>0?(E16_ROW_TITLES[data.rows[i]-1]||page).slice(0,9):'---';
        title=number+' '+left+':'+name(0)+(multi?' / '+right+':'+name(1):'');
    }else title=number+' '+page+(shared?(data.page===6?(data.mode===3?' A-D':' A/B'):''):' '+left+(multi?'/'+right:''));
    return {title:title.slice(0,31),pageKey:data.mode+':'+pair+':'+data.page,cells};
}
const E16_REMOTE_RGB=[[28,28,24],[2,26,22],[30,28,4],[22,8,20],[24,8,34]];
function remoteOwnership(active,requested){
    if(requested&&!remoteModeAllowed)return false;
    try{
        const value=(active?1:0)+','+(requested?1:0);
        const result=typeof host_module_set_param_blocking==='function'
            ?host_module_set_param_blocking('e16_remote',value,10):host_module_set_param('e16_remote',value);
        return result!==false&&result!==0;
    }catch(_){return false;}
}
function setRemoteControl(key,value){
    try{
        const result=typeof host_module_set_param_blocking==='function'
            ?host_module_set_param_blocking(key,String(value),10):host_module_set_param(key,String(value));
        return result!==false&&result!==0&&result!==null;
    }catch(_){return false;}
}
function flushRemoteButtons(){
    if(!remoteE16.ready)return false;
    if(remoteButtons.sent!==remoteButtons.mask){
        if(!setRemoteControl('e16_remote_buttons',remoteButtons.mask))return false;
        remoteButtons.sent=remoteButtons.mask;
    }
    if(remoteButtons.target!==null&&setRemoteControl('e16_remote_pair',remoteButtons.target)){
        remoteButtons.pair=remoteButtons.target;remoteButtons.target=null;
    }
    return true;
}
function remoteButtonState(mask){
    const previous=remoteButtons.physical,fresh=mask&~previous,shifted=(mask&(1<<16))!==0;
    remoteButtons.physical=mask;
    // Shift + encoder 2 toggles a local index. Releasing Shift and pressing
    // encoders 1-12 selects a page; encoder 16 cancels without changing page.
    if(shifted&&(fresh&(1<<1))){
        remoteButtons.pageJump=!remoteButtons.pageJump;
        remoteButtons.mask=remoteButtons.pageJump?E16_PAGE_JUMP_MASK:0;
        if(!flushRemoteButtons()){remoteButtons.pageJump=false;remoteButtons.mask=mask;flushRemoteButtons();return;}
        if(remoteButtons.data)remoteE16.setView(remoteButtons.pageJump
            ?remotePageJumpView(remoteButtons.data):remoteE16View(remoteButtons.data));
        return;
    }
    if(remoteButtons.pageJump){
        remoteButtons.mask=E16_PAGE_JUMP_MASK;flushRemoteButtons();
        if(!shifted){
            let page=-1;for(let i=0;i<12;++i)if(fresh&(1<<i)){page=i;break;}
            if(page>=0&&setRemoteControl('e16_remote_page',page)){
                remoteButtons.pageJump=false;remoteButtons.mask=0;flushRemoteButtons();
            }else if(fresh&(1<<15)){
                remoteButtons.pageJump=false;remoteButtons.mask=0;flushRemoteButtons();
                if(remoteButtons.data)remoteE16.setView(remoteE16View(remoteButtons.data));
            }
        }
        return;
    }
    remoteButtons.mask=mask;flushRemoteButtons();
}
function remotePairSwitch(){
    if(remoteButtons.mode!==3)return;
    remoteButtons.target=(remoteButtons.target??remoteButtons.pair)===0?2:0;
    flushRemoteButtons();
}
const remoteE16=e16RemoteProtocol.create({send:sendExternal,onOwnership:(active,requested)=>{
    const ok=remoteOwnership(active,requested);
    if(ok&&active)remoteButtons.sent=null;
    if(!requested)Object.assign(remoteButtons,{mask:0,sent:null,physical:0,pair:0,target:null,mode:0,pageJump:false,data:null});
    return ok;
},onButtons:remoteButtonState,onPair:remotePairSwitch,onError:message=>showActionMessage(message)});
function remoteE16View(data){
    const view=e16PageView(data);if(!view)return null;
    return {...view,cells:view.cells.map(c=>c&&!c.reserved?{...c,rgb:E16_REMOTE_RGB[c.family]||E16_REMOTE_RGB[0]}:c)};
}
function remotePageJumpView(data){
    const cells=Array(16).fill(null);
    for(let i=0;i<12;++i)cells[i]={label:E16_PAGES[i],text:i===data.page?'CURRENT':'OPEN',
        raw:i===data.page?16383:3072,rgb:E16_REMOTE_RGB[0],bipolar:false};
    cells[15]={label:'BACK',text:'CANCEL',raw:0,rgb:E16_REMOTE_RGB[4],bipolar:false};
    return {title:'PAGE JUMP - PRESS 1-12',pageKey:'jump:'+data.mode+':'+data.pair+':'+data.page,cells};
}
function updateRemoteE16(data){
    const view=remoteE16View(data);if(!view)return;
    remoteButtons.data=data;
    remoteButtons.mode=data.mode;
    if(remoteButtons.target===null)remoteButtons.pair=data.pair;
    else if(data.pair===remoteButtons.target){remoteButtons.pair=data.pair;remoteButtons.target=null;}
    remoteE16.setView(remoteButtons.pageJump?remotePageJumpView(data):view);
}
globalThis.onMidiMessageExternal=bytes=>{if(syncRemoteTransport())remoteE16.feed(bytes);};
globalThis.onResume=function(){resetMidiSurface();};
