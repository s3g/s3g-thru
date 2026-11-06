// One direction controls sound (native relative CC); this direction draws rings.
function createE16NativeFeedback(send) {
    let identity='';const pending=new Map(),sent=new Map();
    return {
        reset(){identity='';pending.clear();sent.clear();},
        update(data){
            if(data?.v!==1||!Number.isInteger(data.page)||data.page<0||data.page>11
                ||!Number.isInteger(data.token)||data.token<0||data.token>127||!Array.isArray(data.values))return;
            const next=data.page+':'+data.token;
            if(next!==identity){identity=next;pending.clear();sent.clear();}
            for(const entry of data.values){
                if(!Array.isArray(entry)||entry.length!==3)continue;
                const [index,raw,color]=entry;
                if(!entry.every(Number.isInteger)||index<0||index>15||raw<0||raw>16383||color<0||color>100)continue;
                const value=raw+':'+color;
                if(sent.get(index)===value)pending.delete(index);
                else pending.set(index,{index,raw,color,value,page:data.page,token:data.token});
            }
        },
        tick(){
            const entries=[...pending.values()].slice(0,6);if(!entries.length)return;
            const first=entries[0],bytes=[240,125,83,84,1,first.page,first.token];
            for(const e of entries)bytes.push(e.index,e.raw>>7,e.raw&127,e.color);
            bytes.push(247);
            const packets=[];
            for(let i=0;i<bytes.length;i+=3){
                const remaining=bytes.length-i,cin=remaining>3?4:4+remaining;
                packets.push(cin,bytes[i],bytes[i+1]??0,bytes[i+2]??0);
            }
            if(send(packets)===false)return;
            for(const e of entries){sent.set(e.index,e.value);pending.delete(e.index);}
        },
    };
}
const e16NativeFeedback=createE16NativeFeedback(sendExternal);
