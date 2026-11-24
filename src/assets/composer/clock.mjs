// A device-authored route avoids guessing random, vertical or rotated traversal.
export class StepClock {
 constructor(now=()=>performance.now()){this.now=now;this.offset=Infinity;this.anchor=null;this.phases=[];this.emission=-Infinity;this.epoch=null;}
 ingest(status){const c=status.sequenceClock;if(!c||!Array.isArray(c.lanes)||c.lanes.length!==4||!Number.isFinite(c.at)||!Number.isFinite(status.emittedAt))return false;
  if(this.epoch===status.epoch&&status.emittedAt<this.emission)return false;this.epoch=status.epoch;this.emission=status.emittedAt;
  const received=this.now(),offset=received-status.emittedAt;this.offset=Math.min(this.offset,offset);
  const identity=`${status.epoch}:${c.clip}:${c.launch}`;
  if(this.anchor?.identity!==identity){this.phases=[];this.offset=offset;}
  this.anchor={...c,identity,localAt:c.at+this.offset,received};return true;
 }
 positions(clip,now=this.now()){const a=this.anchor;if(!a||a.clip!==clip||!a.running||now-a.received>750){this.phases=[];return [-1,-1,-1,-1];}
  return a.lanes.map((lane,p)=>{if(!Number.isFinite(lane.phase)||!(lane.stepMs>0)||!Array.isArray(lane.cells))return -1;
   const desired=lane.phase+Math.max(0,now-a.localAt)/lane.stepMs;
   const old=this.phases[p];let phase=desired;
   if(old&&old.stepMs===lane.stepMs&&now>=old.at){const predicted=old.phase+(now-old.at)/lane.stepMs;const correction=Math.max(-.08,Math.min(.08,desired-predicted));phase=Math.max(old.phase,predicted+correction);if(Math.abs(desired-predicted)>2)phase=desired;}
   phase=Math.max(phase,Math.floor(lane.phase));this.phases[p]={phase,at:now,stepMs:lane.stepMs};const index=Math.floor(phase+1e-8)-Math.floor(lane.phase+1e-8);return index>=0&&index<lane.cells.length?lane.cells[index]:-1;
  });
 }
}

// Clip ruler animation shares the device-authored timestamp with Sequence.
export class ClipClock {
 constructor(now=()=>performance.now()){this.now=now;this.offset=Infinity;this.anchor=null;}
 ingest(status){const c=status.sequenceClock;if(!c||!Number.isFinite(c.positionMs)||!Number.isFinite(c.at)||!Number.isFinite(status.emittedAt))return;
 const received=this.now(),identity=status.epoch+':'+c.clip+':'+c.launch;if(this.anchor?.identity!==identity)this.offset=Infinity;
 if(this.anchor?.identity===identity&&c.at<this.anchor.at)return;this.offset=Math.min(this.offset,received-status.emittedAt);this.anchor={...c,identity,localAt:c.at+this.offset,received};
 }
 position(clip,span,now=this.now()){const a=this.anchor;if(!a||a.clip!==clip||!a.transport||now-a.received>750||now-a.localAt>750)return null;return ((a.positionMs+Math.max(0,now-a.localAt)*a.speed)%span+span)%span;}
}
