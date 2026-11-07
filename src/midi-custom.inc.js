// On-device Custom CC profiles. Draft edits are committed explicitly; entering
// Learn or loading a profile never writes an instrument parameter.
const customMidiUi={data:null,entries:[],entriesKey:'',selection:0,tab:0,returnPage:0,
    copyDestination:1,targetPart:0,pickerTier:0,pickerGroup:'Core',pickerSelection:0,
    confirm:null,notice:'',noticeUntil:0};
const CUSTOM_COMMANDS=['Play clip','Stop clip','Record performance','Undo clip/source','Panic','Previous bank','Next bank',
    'Bank 1','Bank 2','Bank 3','Bank 4','Bank 5','Bank 6','Bank 7','Bank 8','Undo overdub take',
    'Previous clip','Next clip','Hold pattern fade','Ignore CC'];
const CUSTOM_PROFILES=['NONE','CUSTOM1','CUSTOM2','CUSTOM3','CUSTOM4','NANO CC'];
const CUSTOM_PARTS=['SHR','A','B','C','D','FOC'];
const customMidiViews=['midiMapList','midiMapEdit','midiMapPicker','midiMapConfirm'];
function customMidiActive() {return customMidiUi.data?.profile||0;}
function customMidiPage() {return state.view==='settings'&&currentSettingsPage().customMaps;}
function customMidiContext() {return customMidiPage()||customMidiViews.includes(state.view);}
function customMidiEditable() {return customMidiActive()>=1&&customMidiActive()<=4;}
function customMidiNotice(text) {customMidiUi.notice=text;customMidiUi.noticeUntil=state.ticks+45;}
function readCustomMidi() {
    let data;try{data=JSON.parse(String(getParam('midi_custom_state','{}')));}catch(_){return;}
    if(data.v!==1||!Number.isInteger(data.profile)||data.profile<0||data.profile>5
        ||!Array.isArray(data.draft)||data.draft.length!==13||!data.draft.every(Number.isFinite))return;
    const previous=customMidiUi.data;customMidiUi.data=data;
    if(customMidiContext()) {
        const key=data.profile+':'+data.revision;
        if(key!==customMidiUi.entriesKey){
            let entries;try{entries=JSON.parse(String(getParam('midi_custom_entries','[]')));}catch(_){return;}
            if(Array.isArray(entries)&&entries.length<=128&&entries.every(e=>Array.isArray(e)&&e.length===9&&e.every(Number.isFinite))){
                customMidiUi.entries=entries;customMidiUi.entriesKey=key;
                customMidiUi.selection=clamp(customMidiUi.selection,0,entries.length);
            }
        }
        if(previous?.profile!==data.profile&&customMidiViews.includes(state.view)){
            state.view='midiMapList';customMidiUi.selection=0;customMidiUi.confirm=null;
        }
        if(data.result===1&&previous?.ui!==data.ui)customMidiNotice('LEARNED CH'+data.draft[0]+' CC'+data.draft[1]);
        if(data.result===7&&previous?.ui!==data.ui)customMidiNotice('LEARN TIMED OUT');
    }
}
function customMidiSet(key,value) {setParamBlocking('midi_custom_'+key,value);readCustomMidi();rebuildIconLedQueue();}
function resetCustomMidiUi() {
    setParam('midi_custom_cancel',1);customMidiUi.data=null;customMidiUi.entries=[];customMidiUi.entriesKey='';
    customMidiUi.selection=0;customMidiUi.confirm=null;customMidiUi.notice='';
}
function customMidiTitle(title,fraction='') {
    const width=fraction.length*6;
    const limit=Math.floor((128-(width?width+6:0)-1)/6);
    print(1,0,String(title).slice(0,limit),1);if(fraction)print(128-width,0,fraction,1);
    drawRect(0,8,128,1,1);
}
function customMidiCell(index,label,value) {
    const x=index%4*32,y=11+Math.floor(index/4)*22;
    if(!label){drawRect(x+14,y+8,4,4,1);drawRect(x+15,y+9,2,2,0);return;}
    print(x+1,y,String(label).slice(0,5),1);print(x+1,y+10,String(value).slice(0,5),1);
}
function customMidiFooter(fallback) {
    return customMidiUi.notice&&state.ticks<customMidiUi.noticeUntil?customMidiUi.notice:fallback;
}
function drawCustomMidiProfile() {
    const d=customMidiUi.data;clearDisplay();drawSettingsTitle('MIDI MAPS',parameterPages());drawRect(0,8,128,1,1);
    if(!d){print(1,22,'READING MIDI MAPS',1);return;}
    const cells=[['PROF',['NONE','C1','C2','C3','C4','NANO'][d.profile]],['BANK',d.bank],['EDIT','OPEN'],['COPY','TURN'],
        ['DEST','C'+customMidiUi.copyDestination],['CLEAR','TURN'],null,null];
    cells.forEach((cell,i)=>customMidiCell(i,...(cell||[])));
    print(1,57,customMidiFooter(d.mode===6?'MIDI CONTROL MODE OFF':d.save===3?'MAP SAVE FAILED':d.save===1?'SAVING MAPS':d.profile===5?'COPY TO CUSTOM TO EDIT':d.profile?'CLICK: EDIT MAPPINGS':'CHOOSE A PROFILE'),1);
}
function openCustomMidiList() {
    readCustomMidi();if(!customMidiActive()){customMidiNotice('CHOOSE A PROFILE');return;}
    customMidiUi.returnPage=state.settingsPage;customMidiUi.selection=0;customMidiUi.entriesKey='';
    state.view='midiMapList';readCustomMidi();resetParameterEncoderMotion();draw();
}
function customMidiTargetLabel(entry,long=false) {
    const [index,ch,cc,part,kind,target,page,slot]=entry;
    void index;void ch;void cc;
    const prefix=part<0?'':CUSTOM_PARTS[part+1]+' ';
    if(kind===1)return prefix+'E'+(page+1)+'.'+(slot+1);
    if(kind===2)return CUSTOM_COMMANDS[target]||'COMMAND';
    const p=thruMidi.MIDI_BY_ID.get(target);
    return prefix+(p?(long?p.name:(SHORT_LABELS[p.key]||p.short_name||p.name).trim()):'UNKNOWN');
}
function customMidiRows(rows,selected) {
    const offset=clamp(selected-1,0,Math.max(0,rows.length-4));
    for(let row=0;row<4&&offset+row<rows.length;++row){
        const i=offset+row,y=11+row*10,active=i===selected;
        if(active)drawRect(0,y-1,128,9,1);
        print(1,y,String(rows[i]).slice(0,21),active?0:1);
    }
}
function drawCustomMidiList() {
    clearDisplay();const d=customMidiUi.data;if(!d)return;
    const rows=[d.profile===5?'FACTORY: READ ONLY':'+ ADD MAPPING',...customMidiUi.entries.map(e=>
        String(e[1]).padStart(2,'0')+'/'+String(e[2]).padStart(3,'0')+' '+customMidiTargetLabel(e).slice(0,10)+(e[8]?' '+e[8]:''))];
    customMidiTitle(d.profile===5?'NANO MAPS':'C'+d.profile+' MAPS',(customMidiUi.selection+1)+'/'+rows.length);
    customMidiRows(rows,customMidiUi.selection);
    print(1,57,customMidiFooter(d.profile===5?'COPY PROFILE TO EDIT':'CLICK EDIT  X REMOVE'),1);
}
function customMidiBegin(index) {
    customMidiSet('begin',index);if(!customMidiUi.data?.editing)return;
    customMidiUi.tab=0;customMidiUi.targetPart=Math.max(0,customMidiUi.data.draft[2]);
    customMidiUi.notice='';state.view='midiMapEdit';resetParameterEncoderMotion();draw();
}
function customMidiDraft(change) {
    const next=customMidiUi.data.draft.slice();change(next);
    if(next[3]===0){const p=thruMidi.MIDI_BY_ID.get(next[4]);if(p){if(p.scope!=='part')next[2]=-1;if(p.action)next[7]=3;}}
    if(next[3]===1&&next[2]<0)next[2]=customMidiUi.targetPart;
    if(next[3]===2){next[2]=-1;next[7]=3;}
    customMidiSet('draft',next.join(','));
}
function customMidiGroups() {return [...new Set(thruMidi.MIDI_PARAMETERS.map(p=>p.group))];}
function customMidiParameters(group) {return thruMidi.MIDI_PARAMETERS.filter(p=>p.group===group);}
function customMidiDraftTarget() {
    const b=customMidiUi.data?.draft;if(!b)return null;
    if(b[3]===0)return thruMidi.MIDI_BY_ID.get(b[4])||null;
    if(b[3]===1){try{const p=JSON.parse(String(getParam('midi_custom_target','{}')));return thruMidi.MIDI_BY_ID.get(p.id)||null;}catch(_){return null;}}
    return null;
}
function drawCustomMidiEdit() {
    clearDisplay();const d=customMidiUi.data;if(!d)return;const b=d.draft;
    const tab=customMidiUi.tab,names=['TARGET','SOURCE','RESPONSE'];
    customMidiTitle((d.index<0?'NEW MAP':'MAP '+(d.index+1))+' '+names[tab],(tab+1)+'/3');
    const p=customMidiDraftTarget();let cells,footer;
    if(tab===0){
        const groups=customMidiGroups(),group=b[3]===0?p?.group:'',params=customMidiParameters(group);
        void groups;void params;
        cells=[['PART',CUSTOM_PARTS[b[2]+1]],['BIND',['PARAM','SLOT','CMD'][b[3]]],
            b[3]===0?['GROUP',group]:null,b[3]===0?['PARAM',p?(SHORT_LABELS[p.key]||p.short_name||p.name).trim():'---']:b[3]===2?['CMD',b[4]+1]:null,
            b[3]===1?['EPAGE',b[5]+1]:null,b[3]===1?['KNOB',b[6]+1]:null,null,null];
        footer=b[3]===2?CUSTOM_COMMANDS[b[4]]:b[3]===1?(p?.name||'RESERVED ENGINE SLOT'):(p?.name||'CLICK CHOOSE TARGET');
    } else if(tab===1){
        cells=[['CH',b[0]],['CC',b[1]],['TYPE',['ABS','HOLD','TOGL','TRIG'][b[7]]],['LEARN',d.learn?'WAIT':'CLICK'],
            ['FILT',d.filter||'ANY'],['BANK',b[12]||'ALL'],null,null];
        footer=d.learn?'MOVE CONTROL TO LEARN':'CLICK: LEARN CONTROL';
    } else {
        cells=b[3]===2?Array(8).fill(null):[['MIN',Math.round(b[10]*100)+'%'],['MAX',Math.round(b[11]*100)+'%'],
            ['REV',b[9]?'ON':'OFF'],['TAKE',b[8]?'PICK':'JUMP'],null,null,null,null];
        footer='CLICK: SAVE MAPPING';
    }
    cells.forEach((cell,i)=>customMidiCell(i,...(cell||[])));
    print(1,57,customMidiFooter(customMidiEditable()?footer:'FACTORY: COPY TO EDIT').slice(0,21),1);
}
function customMidiPickerRows() {
    if(!customMidiUi.pickerTier)return ['Engine page slot','Controller command',...customMidiGroups()];
    return customMidiUi.pickerGroup==='Commands'?CUSTOM_COMMANDS:customMidiParameters(customMidiUi.pickerGroup).map(p=>p.name);
}
function drawCustomMidiPicker() {
    clearDisplay();const rows=customMidiPickerRows();customMidiUi.pickerSelection=clamp(customMidiUi.pickerSelection,0,rows.length-1);
    customMidiTitle(customMidiUi.pickerTier?customMidiUi.pickerGroup:'DESTINATION',(customMidiUi.pickerSelection+1)+'/'+rows.length);
    customMidiRows(rows,customMidiUi.pickerSelection);print(1,57,'CLICK SELECT  BACK UP',1);
}
function customMidiChooseTarget() {
    const i=customMidiUi.pickerSelection;
    if(!customMidiUi.pickerTier){
        if(i===0){customMidiDraft(b=>{b[3]=1;b[4]=-1;b[2]=customMidiUi.targetPart;b[7]=0;});state.view='midiMapEdit';return;}
        customMidiUi.pickerGroup=i===1?'Commands':customMidiGroups()[i-2];customMidiUi.pickerTier=1;customMidiUi.pickerSelection=0;return;
    }
    if(customMidiUi.pickerGroup==='Commands')customMidiDraft(b=>{b[3]=2;b[4]=i;b[7]=3;});
    else {const p=customMidiParameters(customMidiUi.pickerGroup)[i];customMidiDraft(b=>{b[3]=0;b[4]=p.id;b[2]=p.scope==='part'?customMidiUi.targetPart:-1;b[7]=p.action?3:0;});}
    state.view='midiMapEdit';
}
function customMidiConfirm(type,argument,title,detail) {
    customMidiUi.confirm={type,argument,title,detail,returnView:state.view};state.view='midiMapConfirm';draw();
}
function drawCustomMidiConfirm() {
    clearDisplay();const c=customMidiUi.confirm;if(!c)return;
    customMidiTitle(c.title);print(1,19,c.detail.slice(0,21),1);
    print(1,35,'CLICK TO CONFIRM',1);print(1,47,'BACK TO CANCEL',1);
}
function customMidiCommit(replace=false) {
    customMidiSet('apply',replace?2:1);const d=customMidiUi.data;
    if(d?.result===2){customMidiConfirm('replace',0,'CC ALREADY ASSIGNED','Replace map '+(d.conflict+1)+'?');return;}
    if(d?.result===5){state.view='midiMapList';customMidiUi.notice='';customMidiUi.entriesKey='';readCustomMidi();return;}
    customMidiNotice(d?.result===3?'128 MAPPING LIMIT':'INVALID MAPPING');
}
function customMidiAccept() {
    const c=customMidiUi.confirm;if(!c)return;customMidiUi.confirm=null;state.view=c.returnView;
    if(c.type==='replace'){customMidiCommit(true);return;}
    customMidiSet(c.type,c.argument);customMidiUi.entriesKey='';
    if(c.type==='delete'){state.view='midiMapList';customMidiUi.selection=0;}
    else state.view='settings';
    readCustomMidi();
}
function customMidiBack() {
    if(state.view==='midiMapConfirm'){
        state.view=customMidiUi.confirm?.returnView||'midiMapList';customMidiUi.confirm=null;return;
    }
    if(state.view==='midiMapPicker'){
        if(customMidiUi.pickerTier){customMidiUi.pickerTier=0;customMidiUi.pickerSelection=0;}else state.view='midiMapEdit';return;
    }
    if(state.view==='midiMapEdit'){customMidiSet('cancel',1);state.view='midiMapList';return;}
    customMidiSet('cancel',1);state.view='settings';state.settingsPage=customMidiUi.returnPage;
}
function adjustCustomMidiProfile(index,delta) {
    const d=customMidiUi.data;if(!d||!delta)return;
    if(index===2){openCustomMidiList();return;}
    if(index===3){if(d.profile)customMidiConfirm('copy',customMidiUi.copyDestination,'COPY PROFILE','Overwrite CUSTOM'+customMidiUi.copyDestination+'?');return;}
    if(index===5){if(customMidiEditable())customMidiConfirm('clear',1,'CLEAR PROFILE','Remove all mappings?');else customMidiNotice('CUSTOM PROFILES ONLY');return;}
    delta=parameterEncoderDelta(index,'custom-profile-'+index,delta);if(!delta)return;
    if(index===0){customMidiSet('profile',clamp(d.profile+delta,0,5));syncRemoteTransport();}
    if(index===1)customMidiSet('bank',clamp(d.bank+delta,1,8));
    if(index===4)customMidiUi.copyDestination=clamp(customMidiUi.copyDestination+delta,1,4);
}
function adjustCustomMidiEdit(index,delta) {
    const d=customMidiUi.data;if(!d||!delta)return;
    if(!customMidiEditable()){customMidiNotice('COPY TO CUSTOM FIRST');return;}
    if(customMidiUi.tab===1&&index===3){customMidiSet('learn',d.learn?0:1);return;}
    delta=parameterEncoderDelta(index,'custom-edit-'+customMidiUi.tab+'-'+index,delta);if(!delta)return;
    const b=d.draft,tab=customMidiUi.tab;
    if(tab===0){
        if(index===0){const part=clamp((b[2]<0?customMidiUi.targetPart:b[2])+delta,0,4);customMidiUi.targetPart=part;customMidiDraft(n=>{n[2]=part;});}
        else if(index===1){const kind=clamp(b[3]+delta,0,2);customMidiDraft(n=>{n[3]=kind;n[4]=kind===1?-1:0;n[2]=kind===2?-1:customMidiUi.targetPart;n[7]=kind===2?3:0;});}
        else if(index===2&&b[3]===0){
            const groups=customMidiGroups(),p=thruMidi.MIDI_BY_ID.get(b[4]);const group=groups[clamp(groups.indexOf(p?.group)+delta,0,groups.length-1)];
            const next=customMidiParameters(group)[0];customMidiDraft(n=>{n[4]=next.id;n[2]=next.scope==='part'?customMidiUi.targetPart:-1;n[7]=next.action?3:0;});
        } else if(index===3&&b[3]===0){
            const p=thruMidi.MIDI_BY_ID.get(b[4]),params=customMidiParameters(p?.group);const next=params[clamp(params.indexOf(p)+delta,0,params.length-1)];
            if(next)customMidiDraft(n=>{n[4]=next.id;n[2]=next.scope==='part'?customMidiUi.targetPart:-1;n[7]=next.action?3:0;});
        } else if(index===3&&b[3]===2)customMidiDraft(n=>{n[4]=clamp(n[4]+delta,0,CUSTOM_COMMANDS.length-1);});
        else if(index===4&&b[3]===1)customMidiDraft(n=>{n[5]=clamp(n[5]+delta,0,3);});
        else if(index===5&&b[3]===1)customMidiDraft(n=>{n[6]=clamp(n[6]+delta,0,7);});
    }else if(tab===1){
        if(index===0||index===1){customMidiSet('learn',0);customMidiDraft(n=>{n[index]=clamp(n[index]+delta,index===0?1:0,index===0?16:127);});}
        if(index===2)customMidiDraft(n=>{n[7]=clamp(n[7]+delta,0,3);});
        if(index===4)customMidiSet('learn_filter',clamp(d.filter+delta,0,16));
        if(index===5)customMidiDraft(n=>{n[12]=clamp(n[12]+delta,0,8);});
    }else if(b[3]!==2){
        if(index===0)customMidiDraft(n=>{n[10]=clamp(Math.round(n[10]*100+delta)/100,0,n[11]);});
        if(index===1)customMidiDraft(n=>{n[11]=clamp(Math.round(n[11]*100+delta)/100,n[10],1);});
        if(index===2)customMidiDraft(n=>{n[9]=delta>0?1:0;});
        if(index===3)customMidiDraft(n=>{n[8]=delta>0?1:0;});
    }
}
function handleCustomMidiCcUi(number,value) {
    if(!customMidiContext())return false;
    const view=state.view;
    if(number===MovePlay&&value>0&&state.iconPressed.has(MoveDelete)) {
        customMidiSet('cancel',1);customMidiUi.confirm=null;
        if(customMidiViews.includes(state.view))state.view='midiMapList';
        setParamBlocking('panic',1);customMidiNotice('PANIC: NOTES OFF');return true;
    }
    if(number===MoveMenu&&value>0&&customMidiViews.includes(view)){
        customMidiSet('cancel',1);state.view='settings';state.settingsPage=customMidiUi.returnPage;
        return !state.shiftHeld&&!state.focusButtonHeld;
    }
    if(number===MoveBack&&value>0&&customMidiViews.includes(view)){customMidiBack();return true;}
    if(number===MoveUndo&&value>0){
        if(view==='midiMapEdit'||view==='midiMapPicker'){customMidiSet('cancel',1);state.view='midiMapList';}
        else {customMidiSet('undo',1);customMidiUi.entriesKey='';readCustomMidi();}
        return true;
    }
    if(number===MoveDelete&&customMidiViews.includes(view)){
        state.deleteHeld=false;setParam('delete_held',0);
        if(value>0&&customMidiEditable()){
            const row=view==='midiMapList'?customMidiUi.entries[customMidiUi.selection-1]:null;
            const index=row?row[0]:view==='midiMapEdit'?customMidiUi.data.index:-1;
            if(index>=0)customMidiConfirm('delete',index,'REMOVE MAPPING','Delete map '+(index+1)+'?');
        }
        return true;
    }
    if(number===MoveMainButton&&value>0){
        if(customMidiPage())openCustomMidiList();
        else if(view==='midiMapList'){
            if(customMidiUi.selection===0){if(customMidiEditable())customMidiBegin(-1);else customMidiNotice('COPY TO CUSTOM FIRST');}
            else customMidiBegin(customMidiUi.entries[customMidiUi.selection-1][0]);
        }else if(view==='midiMapPicker')customMidiChooseTarget();
        else if(view==='midiMapConfirm')customMidiAccept();
        else if(view==='midiMapEdit'){
            if(!customMidiEditable()){customMidiNotice('COPY TO CUSTOM FIRST');return true;}
            if(customMidiUi.tab===0){customMidiUi.pickerTier=0;customMidiUi.pickerSelection=0;state.view='midiMapPicker';}
            else if(customMidiUi.tab===1)customMidiSet('learn',customMidiUi.data.learn?0:1);
            else customMidiCommit();
        }
        draw();return true;
    }
    if(number>=MoveKnob1&&number<MoveKnob1+8){
        if(customMidiPage())adjustCustomMidiProfile(number-MoveKnob1,decodeDelta(value));
        else if(view==='midiMapEdit')adjustCustomMidiEdit(number-MoveKnob1,decodeDelta(value));
        draw();return true;
    }
    if(customMidiViews.includes(view)&&(number===MoveMainKnob||number===MoveUp||number===MoveDown||number===MoveLeft||number===MoveRight)){
        if(number!==MoveMainKnob&&value<=0)return true;
        const delta=number===MoveMainKnob?decodeDelta(value):number===MoveUp||number===MoveLeft?-1:1;
        if(view==='midiMapList')customMidiUi.selection=clamp(customMidiUi.selection+delta,0,customMidiUi.entries.length);
        if(view==='midiMapPicker')customMidiUi.pickerSelection=clamp(customMidiUi.pickerSelection+delta,0,customMidiPickerRows().length-1);
        if(view==='midiMapEdit'){
            customMidiSet('learn',0);customMidiUi.tab=(customMidiUi.tab+delta%3+3)%3;resetParameterEncoderMotion();
        }
        draw();return true;
    }
    if(customMidiViews.includes(view)&&state.focusButtonHeld&&MoveTracks.includes(number)&&value>0){
        customMidiSet('cancel',1);state.view='settings';state.settingsPage=customMidiUi.returnPage;
    }
    return false;
}
function drawCustomMidi() {
    if(state.view==='midiMapList')drawCustomMidiList();
    else if(state.view==='midiMapEdit')drawCustomMidiEdit();
    else if(state.view==='midiMapPicker')drawCustomMidiPicker();
    else if(state.view==='midiMapConfirm')drawCustomMidiConfirm();
}
