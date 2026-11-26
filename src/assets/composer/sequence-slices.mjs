// The same map priority and one-based explicit address used by Sequence on Move.
export function sequenceSliceMap(frame,part=frame?.focus??0,step=-1,child=0){
 const clip=frame?.document?.clips[0],trig=clip?.sequence?.tracks[part]?.trigs.find(t=>t.step===step),parent=trig?.locks?.sound||{},own=child?trig?.burst?.[child-1]?.locks?.sound||{}:parent;
 const program={...clip?.parts[part]?.parameters,...parent,...own};
 if(clip?.mode!=='sequence'||clip.parts[part]?.engine!=='slices')return null;
 const maps=frame.sampleMarkers?.[part]||{},mode=Math.round(program.slice_region_mode??1);
 const sources=mode===3||mode===4?['edited','file','transients']:mode===2?['file']:mode===1?['transients']:[];
 for(const key of sources){const rows=maps[key];if(Array.isArray(rows)&&rows.length&&rows.every(p=>Number.isFinite(p.position)))return {rows,count:rows.length,kind:key,program};}
 if(mode===2)return {rows:[],count:0,kind:'missing-file',program};
 const count=Math.max(1,Math.min(64,Math.round(program.slice_regions??16))),width=Math.max(.000001,Math.min(1,program.field??1)),low=Math.max(0,Math.min(1-width,(program.locus??.5)-width/2));
 return {rows:Array.from({length:count},(_,i)=>({position:low+i/count*width,label:''})),count,kind:'equal',end:low+width,program};
}
export function sequenceSliceSelection(frame,part,step,child=0){
 const map=sequenceSliceMap(frame,part,step,child);if(!map)return null;
 const lane=frame.document.clips[0].sequence.tracks[part],trig=lane.trigs.find(t=>t.step===step)||{},parent=trig.locks||{},own=child?trig.burst?.[child-1]?.locks||{}:parent;
 const explicit=own.sound?.seq_slice??(child?parent.sound?.seq_slice:undefined);
 const position=own.position??parent.position??lane.defaults.position??map.program.locus??.5;
 const number=explicit??(map.count?(Math.max(0,Math.floor(position*(map.program.slice_regions??16)))%map.count)+1:1),index=number-1,valid=Number.isInteger(index)&&index>=0&&index<map.count;
 return {...map,mapEnd:map.end??1,number,valid,own:own.sound?.seq_slice!==undefined,inherited:child>0&&own.sound?.seq_slice===undefined&&parent.sound?.seq_slice!==undefined,explicit:explicit!==undefined,start:valid?map.rows[index].position:position,end:valid?(map.rows[index+1]?.position??map.end??1):position,label:valid?map.rows[index].label||'':''};
}
