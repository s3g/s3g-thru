// Small Remote-only transport and renderer. No Lua runs on E16.
// OXI protocol: https://docs.google.com/spreadsheets/d/1Yccnrluv10QL_PauMmtCt64EtYjfSeZEXKrlrw8P24w/edit
const e16RemoteProtocol=(()=>{
 const HEADER=[0,33,91,2,1];
 const FONT={'"':[5,5,0,0,0],"0":[7,5,5,5,7],"1":[2,6,2,2,7],"2":[6,1,2,4,7],"3":[6,1,2,1,6],"4":[5,5,7,1,1],"5":[7,4,6,1,6],"6":[3,4,7,5,7],"7":[7,1,2,2,2],"8":[7,5,7,5,7],"9":[7,5,7,1,6],"A":[2,5,7,5,5],"B":[6,5,6,5,6],"C":[3,4,4,4,3],"D":[6,5,5,5,6],"E":[7,4,6,4,7],"F":[7,4,6,4,4],"G":[3,4,5,5,3],"H":[5,5,7,5,5],"I":[7,2,2,2,7],"J":[1,1,1,5,2],"K":[5,5,6,5,5],"L":[4,4,4,4,7],"M":[5,7,7,5,5],"N":[5,7,7,7,5],"O":[2,5,5,5,2],"P":[6,5,6,4,4],"Q":[2,5,5,7,3],"R":[6,5,6,5,5],"S":[3,4,2,1,6],"T":[7,2,2,2,2],"U":[5,5,5,5,7],"V":[5,5,5,5,2],"W":[5,5,7,7,5],"X":[5,5,2,5,5],"Y":[5,5,2,2,2],"Z":[7,1,2,4,7]," ":[0,0,0,0,0],".":[0,0,0,0,2],"-":[0,0,7,0,0],"%":[5,1,2,4,5],"/":[1,1,2,4,4],":":[0,2,0,2,0],"+":[0,2,7,2,0],"?":[6,1,2,0,2]};
 const ascii=s=>String(s??'').toUpperCase().replace(/[^\x20-\x7e]/g,'?');
 const pack=bytes=>{const out=[];for(let at=0;at<bytes.length;at+=7){const part=bytes.slice(at,at+7);out.push(part.reduce((v,b,i)=>v|((b>>7)<<i),0),...part.map(b=>b&127));}return out;};
 const unpack=bytes=>{const out=[];for(let at=0;at<bytes.length;){const bits=bytes[at++];for(let i=0;i<7&&at<bytes.length;i++)out.push(bytes[at++]|(((bits>>i)&1)<<7));}return out;};
 const sysex=(op,data=[])=>[240,...HEADER,...op,...pack(data),247];
 const packetize=bytes=>{const out=[];for(let at=0;at<bytes.length;at+=3){const n=Math.min(3,bytes.length-at);out.push(at+n===bytes.length?n+4:4,...bytes.slice(at,at+n));while(out.length%4)out.push(0);}return out;};
 const tiles=Array.from({length:36},(_,i)=>i<4?{x:i*32,y:0,w:32,h:5}:{x:Math.floor((i-4)/2)%4*32,y:8+Math.floor(Math.floor((i-4)/2)/4)*14+(i%2?6:0),w:32,h:5});
 function render(view){
  const frame=new Uint8Array(8192);
  const text=(x,y,str,limit)=>{for(const ch of ascii(str).slice(0,limit)){const glyph=FONT[ch]||FONT['?'];for(let row=0;row<5;row++)for(let col=0;col<3;col++)if(glyph[row]&(4>>col))frame[(y+row)*128+x+col]=1;x+=4;}};
  text(1,0,view.title,31);
  for(let i=0;i<16;i++){const c=view.cells[i];if(!c)continue;const x=i%4*32,y=8+Math.floor(i/4)*14;if(c.reserved){for(let dy=0;dy<3;dy++)for(let dx=0;dx<3;dx++)if(dx!==1||dy!==1)frame[(y+6+dy)*128+x+14+dx]=1;continue;}text(x+1,y,c.label,7);text(x+1,y+6,c.text,7);}
  return tiles.map(t=>{const b=Array(20).fill(0);for(let y=0;y<5;y++)for(let x=0;x<32;x++)if(frame[(t.y+y)*128+t.x+x])b[y*4+(x>>3)]|=1<<(7-(x&7));return b;});
 }
 function create(options){
  // Bound OLED work by acknowledged pixels and a packet-rate budget. Each
  // complete message fits one host write (12 USB packets); never split SysEx.
  const ACK_MS=1000,RETRY_LIMIT=2,HEARTBEAT_MS=2000,MAX_FLIGHT=8;
  const now=options.now||(()=>Date.now());
  let leased=false,enabled=true,pendingExit=false,active=false,ready=false;
  let lastProbe=-10000,lastAck=0,reconnectAt=0,probePending=false,clearAt=null,clearTries=0;
  let cursor=0,ringCursor=0,touched=-1,shift=false,buttons=0,window=320,preferText=false;
  let packetCredit=12,budgetAt=now();
  let pageKey=null,assembler=null,midiStatus=0,midiData=[];
  let view={title:'ENGINE',cells:Array(16).fill(null)},signature='',desired=render(view);
  let acks=0,nacks=0,retries=0,reconnects=0,issue='NONE';
  const sent=Array(36).fill(null),flight=Array(36).fill(null),retryAt=Array(36).fill(0),failures=Array(36).fill(0);
  const rings=Array(16).fill(''),zero=Array(20).fill(0).join(',');
  const transmit=(b,metered=false)=>{
   const p=packetize(b),cost=p.length/4;if(p.length>48)throw Error('Remote write exceeds 12 USB-MIDI packets');
   if(metered&&cost>packetCredit)return false;
   if(options.send(p)===false)return false;
   if(metered)packetCredit-=cost;return true;
  };
  const pendingPixels=()=>flight.reduce((n,f)=>n+(f?.pixels||0),0);
  const ownership=(a,r)=>{const ok=options.onOwnership?.(a,r)!==false;if(ok)leased=r;return ok;};
  function stop(){
   if(active)pendingExit=!transmit(sysex([6,0]));
   active=ready=false;probePending=false;clearAt=null;flight.fill(null);
   shift=false;buttons=0;options.onButtons?.(0);assembler=null;midiStatus=0;midiData=[];
   if(pendingExit)ownership(false,true);else if(leased)ownership(false,false);
  }
  function recover(reason){
   issue=reason;reconnects++;stop();reconnectAt=now()+1000;lastProbe=-10000;
   options.onError?.('E16 RECONNECTING');
  }
  function sendClear(){if(transmit(sysex([7]))){clearAt=now();clearTries++;return true;}return false;}
  function receive(body){
   if(pendingExit||!HEADER.every((v,i)=>body[i]===v))return;
   const tail=body.slice(5),off=tail[0]===6?1:0,op=tail[off],payload=unpack(tail.slice(off+1));
   if(op===83&&payload.length===0){
    // A heartbeat confirms presence, without repainting or clearing the view.
    if(!probePending)return;probePending=false;lastAck=now();
    if(!active){
     if(!ownership(false,true)){transmit(sysex([6,0]));lastProbe=now();return;}
     active=true;ready=false;clearAt=null;clearTries=0;sendClear();
    }else if(ready)ownership(true,true);
    return;
   }
   if(!active||payload.length!==6)return;
   if(payload[0]===7&&!ready&&clearAt!==null){
    if(op===83&&payload[1]===0){
     if(!ownership(true,true))return;
     lastAck=now();ready=true;clearAt=null;sent.fill(zero);flight.fill(null);
     retryAt.fill(0);failures.fill(0);rings.fill('');cursor=ringCursor=0;window=320;preferText=false;packetCredit=12;budgetAt=now();
     options.onReady?.();
    }else if(op===84){recover('CLEAR NACK');}
    return;
   }
   if(!ready||payload[0]!==8)return;
   const i=flight.findIndex(f=>f&&f.rect.every((v,j)=>v===payload[j+2]));
   const f=flight[i];if(!f)return; // Ignore another surface's or late replies.
   if(op===83&&payload[1]===0){
    lastAck=now();acks++;sent[i]=f.sig;flight[i]=null;failures[i]=0;
    // ACKs carry geometry, not a sequence number. Let duplicate replies from
    // a retransmission drain before sending a newer value at that geometry.
    retryAt[i]=f.tries?now()+ACK_MS:0;
    if(acks%8===0)window=Math.min(320,window+80);
   }else if(op===84){
    lastAck=now();nacks++;issue='TILE NACK';flight[i]=null;sent[i]=null;window=Math.max(160,Math.floor(window/2));
    retryAt[i]=now()+Math.min(800,100*(1+failures[i]));
    if(payload[1]===4||++failures[i]>=3)recover('TILE NACK');
   }
  }
  function message(status,n,value){
   if(!active||(status&15)!==0)return;
   const type=status&240;
   if(type===176&&n>=1&&n<=16){if(!shift)touched=n-1;return;}
   if((type===144||type===128)&&n<=16){
    const down=type===144&&value>0,bit=1<<n,fresh=down&&!(buttons&bit),next=down?buttons|bit:buttons&~bit;
    if(next!==buttons){buttons=next;options.onButtons?.(buttons);}
    if(n===16)shift=down;
    else if(fresh&&shift&&n===0)options.onPair?.();
    else if(fresh&&shift&&n===15){stop();enabled=false;}
   }
  }
  function sendRings(){
   const changed=[],order=touched>=0?[touched,...Array.from({length:16},(_,i)=>(ringCursor+i)%16).filter(i=>i!==touched)]:Array.from({length:16},(_,i)=>(ringCursor+i)%16);
   for(const i of order){
    const c=view.cells[i],amount=Math.max(0,Math.min(16383,Math.round(c?.raw||0)));
    const raw=[i,...(c?.rgb||[0,0,0]),amount>>7,amount&127,c?.bipolar?1:0],sig=raw.join(',');
    if(rings[i]!==sig)changed.push({i,raw,sig});if(changed.length===3)break;
   }
   if(!changed.length)return null;
   if(!transmit(sysex([6,4],changed.flatMap(c=>c.raw)),true))return false;
   for(const c of changed)rings[c.i]=c.sig;
   ringCursor=(changed[changed.length-1].i+1)%16;return true;
  }
  function headingPending(){
   for(let i=0;i<4;i++)if(flight[i]||sent[i]!==desired[i].join(','))return true;
   return false;
  }
  // Trim a tile to changed pixels, including pixels that must turn OFF.
  // Rows remain byte-aligned in the wire payload even at a non-byte x/width.
  function patch(i,raw){
   const before=sent[i]===null?null:sent[i].split(',').map(Number);
   let x0=32,y0=5,x1=-1,y1=-1;
   for(let y=0;y<5;y++)for(let x=0;x<32;x++)if(!before||((raw[y*4+(x>>3)]^before[y*4+(x>>3)])&(128>>(x&7)))){
    x0=Math.min(x0,x);x1=Math.max(x1,x);y0=Math.min(y0,y);y1=Math.max(y1,y);
   }
   if(x1<0)return null;
   const w=x1-x0+1,h=y1-y0+1,stride=Math.ceil(w/8),bits=Array(stride*h).fill(0),t=tiles[i];
   for(let y=0;y<h;y++)for(let x=0;x<w;x++)if(raw[(y+y0)*4+((x+x0)>>3)]&(128>>((x+x0)&7)))bits[y*stride+(x>>3)]|=128>>(x&7);
   return {rect:[t.x+x0,t.y+y0,w,h],raw:bits,pixels:w*h};
  }
  function sendText(time,headerOnly=false){
   if(flight.filter(Boolean).length>=MAX_FLIGHT)return null;
   const urgent=touched>=0?5+touched*2:-1,pending=pendingPixels();
   for(let pass=0;pass<(headerOnly?4:37);pass++){
    const i=headerOnly?pass:pass===0&&urgent>=0?urgent:(cursor+pass-(urgent>=0?1:0)+36)%36;
    if(flight[i]||time<retryAt[i])continue;
    const raw=desired[i],sig=raw.join(',');if(sent[i]===sig)continue;
    const region=patch(i,raw);if(!region||pending+region.pixels>window)continue;
    if(!transmit(sysex([8],[...region.rect,...region.raw]),true))return false;
    flight[i]={...region,sig,at:time,tries:0};cursor=(i+1)%36;return true;
   }
   return null;
  }
  const api={
   get active(){return active;},get ready(){return ready;},
   get diagnostics(){return {state:!enabled?'OFF':pendingExit||now()<reconnectAt?'RECONNECT':!active?'SEARCH':ready?'LIVE':'WAIT CLEAR',issue,acks,nacks,retries,reconnects,inflight:flight.filter(Boolean).length,window:MAX_FLIGHT,pixels:pendingPixels(),pixelWindow:window};},
   stop,enable(v){if(!v)stop();enabled=!!v;if(enabled){lastProbe=-10000;reconnectAt=0;}},
   resume(){stop();enabled=true;lastProbe=-10000;reconnectAt=0;},
   setView(next){
    if(next.pageKey!==pageKey){pageKey=next.pageKey;cursor=0;touched=-1;}
    view=next;
    const nextSig=view.title+'|'+view.cells.map(c=>c?c.label+':'+c.text+':'+!!c.reserved:'').join('|');
    if(nextSig!==signature){signature=nextSig;desired=render(view);}
   },
   feed(bytes){
    if(!enabled)return;
    for(const v of bytes||[]){const b=v&255;if(b>=248)continue;
     if(b===240){assembler=[];midiStatus=0;midiData=[];continue;}
     if(b===247){if(assembler)receive(assembler);assembler=null;continue;}
     if(assembler){if(b<128&&assembler.length<128){assembler.push(b);continue;}assembler=null;}
     if(b>=128){midiStatus=b;midiData=[];continue;}
     if(!midiStatus)continue;midiData.push(b);
     const n=[192,208].includes(midiStatus&240)?1:2;
     if(midiData.length===n){if(n===2)message(midiStatus,midiData[0],midiData[1]);midiData=[];}
    }
   },
   tick(){
    if(pendingExit){if(transmit(sysex([6,0]))){pendingExit=false;if(leased)ownership(false,false);}return;}
    if(!enabled)return;const time=now();
    // Below the shim's default drain (~1 packet/ms). An idle tick may spend
    // at most 24 packets; it cannot accumulate a large reconnect/page burst.
    packetCredit=Math.min(24,packetCredit+Math.max(0,time-budgetAt)*.75);budgetAt=time;
    if(time<reconnectAt)return;
    if(active&&time-lastAck>6500){recover('LINK TIMEOUT');return;}
    if(active&&!ready){
     if(clearAt===null){sendClear();return;}
     if(time-clearAt>=ACK_MS){
      if(clearTries>RETRY_LIMIT){recover('CLEAR TIMEOUT');return;}
      if(sendClear())retries++;
     }
     return;
    }
    if(time-lastProbe>=(active?HEARTBEAT_MS:1500)){
     if(active&&!ownership(ready,true))return;
     if(transmit(sysex([6,85]))){lastProbe=time;probePending=true;}return;
    }
    if(!active||!ready)return;
    for(let i=0;i<flight.length;i++){
     const f=flight[i];if(!f||time-f.at<ACK_MS)continue;
     if(f.tries>=RETRY_LIMIT){recover('TILE TIMEOUT');return;}
     if(transmit(sysex([8],[...f.rect,...f.raw]),true)){f.tries++;f.at=time;retries++;window=160;issue='TILE TIMEOUT';}
     return;
    }
    // Complete the heading before the new body/rings. Within that priority,
    // spend a packet budget instead of waiting one UI tick for every message.
    for(let pass=0;pass<8;pass++){
     if(headingPending()){
      if(sendText(time,true)===true)continue;
      return;
     }
     let result;
     if(preferText){
      result=sendText(time);
      if(result!==null){if(!result)return;preferText=false;continue;}
     }
     result=sendRings();
     if(result!==null){if(!result)return;preferText=true;continue;}
     result=sendText(time);if(result!==true)return;preferText=false;
    }
   }
  };return api;
 }
 return {create,pack,unpack,sysex,packetize,render,tiles};
})();
