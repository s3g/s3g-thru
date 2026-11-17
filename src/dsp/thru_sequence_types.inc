// Persistent recipes materialize normal trig data. Work happens on control
// edits, source publication and recorded gesture edges; never per audio sample.
std::array<std::uint64_t,4> sequenceTypeSourceStamp{};
std::array<std::array<char,64>,4> sequenceTypeStatus{};
const char* sequenceAuthorKey=nullptr;
bool sequenceTypeBuilding=false;
int sequenceTypeSourceCursor=0;
static constexpr const char* sequenceRecipeKeys[]{nullptr,"seq_recipe_hits","seq_recipe_map","seq_recipe_order","seq_recipe_repeat","seq_recipe_write","seq_recipe_amount","seq_recipe_seed","seq_recipe_regions","seq_recipe_preroll","seq_recipe_from","seq_recipe_to","seq_recipe_pitch","seq_recipe_span","seq_recipe_accents","seq_recipe_depth","seq_recipe_duration","seq_recipe_min","seq_recipe_max","seq_recipe_human","seq_recipe_probability","seq_recipe_shift"};
bool sequenceTypeKeyKnown(const char* key) const {
    if(!std::strcmp(key,"seq_type")||!std::strcmp(key,"seq_to_normal")||!std::strcmp(key,"seq_regenerate"))return true;
    for(int i=1;i<22;++i)if(!std::strcmp(key,sequenceRecipeKeys[i]))return true;
    return false;
}
float sequenceTypeValue(const char* key,int part) const {
    const auto& recipe=(sequenceRunning()?sequencePlayback:sequenceLive).tracks[part].generator;
    if(!std::strcmp(key,"seq_type"))return float(recipe.type);
    if(!std::strcmp(key,"seq_recipe_shift"))return float(recipe.shift%std::max(1,(sequenceRunning()?sequencePlayback:sequenceLive).tracks[part].length));
    for(int i=1;i<21;++i)if(!std::strcmp(key,sequenceRecipeKeys[i]))return recipe.values[i];
    return 0;
}
bool buildSequenceRecipe(int part,const s3g_thru_sequence::Lane& base,
        const s3g_thru_generate::Config& config,s3g_thru_generate::Context& context,
        s3g_thru_generate::Result& result,const char*& mapName,char* status,std::size_t capacity) {
    using namespace s3g_thru_generate;
    context={};result={};result.lane=base;const auto& v=config.v;
    for(int i=0;i<base.length;++i)if(base.trigs[i].enabled){++result.enabled;result.events+=base.trigs[i].count;}
    const auto settings=settingsForPart(part);context.root=settings.root;context.scale=0;
    const auto& scale=kScaleDefinitions[std::size_t(settings.scale)];
    for(std::size_t i=0;i<scale.count;++i)context.scale|=std::uint16_t(1u<<scale.intervals[i]);
    const auto* clip=activeClipSlot();context.stepMs=float(s3g_thru_sequence::rates[base.rate]*60000.0/(std::max(1.f,tempoBpm)*(clip?clampValue(clip->clipRate,.25f,4.f):1.f)));
    mapName="KEEP";
    if(v[map]>0){
        const auto lane=std::size_t(part);
        if(!engine.laneLoaded(lane)){std::snprintf(status,capacity,"LOAD LANE SAMPLE");return false;}
        if(v[high]-v[low]<.000001f){std::snprintf(status,capacity,"SET SOURCE RANGE");return false;}
        auto program=liveControlsForPart(part);
        program.sampleRegionMode=static_cast<Engine::RegionMode>(v[map]==1?4:v[map]==2?1:v[map]==3?2:v[map]==4?3:0);program.sampleRegionCount=v[regions];
        int kind=0;const auto source=sliceMapForLane(lane,program,kind);const int requested=int(v[map]);
        if((requested==2&&kind!=1)||(requested==3&&kind!=2)||(requested==4&&kind!=3)){
            std::snprintf(status,capacity,requested==2?"TRANSIENTS NOT READY":requested==3?"NO FILE MARKERS":"NO EDITED MAP");return false;
        }
        mapName=kind==1?"TRANS":kind==2?"FILE":kind==3?"EDIT":"EQUAL";
        for(std::uint32_t i=0;i<source.count;++i){const float p=kind==0?v[low]+source.points[i]*(v[high]-v[low]):source.points[i];if(p>=v[low]&&p<v[high]&&context.count<64)context.points[context.count++]=p;}
        if(!context.count){std::snprintf(status,capacity,"NO STARTS IN RANGE");return false;}
        const float pre=v[preroll]<0?slicePreRoll(lane,kind):v[preroll];context.preRollNormalized=pre*engine.laneSampleRate(lane)/(1000.f*std::max<std::size_t>(1,engine.laneFrames(lane)));
    }
    result=build(base,config,context);std::snprintf(status,capacity,"%d/%d",result.enabled,base.length);return true;
}
void initializeSequenceRecipe(int part,const s3g_thru_sequence::Lane* context=nullptr){
    auto& lane=sequenceLive.tracks[part];auto& recipe=lane.generator;
    const auto& timing=context?*context:lane;
    if(!recipe.ready){recipe.base=lane.trigs;recipe.timed=timing.timeMode;recipe.values=s3g_thru_sequence::generatorDefaults;
        recipe.values[17]=timing.timeMode?50.f:.25f;recipe.values[18]=timing.timeMode?1000.f:4.f;
        const auto window=sliceSourceWindow(liveControlsForPart(part));recipe.values[10]=window[0];recipe.values[11]=window[1];recipe.ready=true;
    }
    if(recipe.timed!=timing.timeMode){
        const auto* clip=activeClipSlot();const float step=float(s3g_thru_sequence::rates[timing.rate]*60000.0/(std::max(1.f,tempoBpm)*(clip?clampValue(clip->clipRate,.25f,4.f):1.f)));
        for(int i:{17,18})recipe.values[i]=clampValue(timing.timeMode?recipe.values[i]*step:recipe.values[i]/step,timing.timeMode?10.f:.01f,timing.timeMode?30000.f:32.f);
        recipe.timed=timing.timeMode;
    }
}
std::uint64_t sequenceRecipeStamp(int part) const {
    return wavesetSourceRevisions[part]*0x9e3779b9u+laneRegionRevisions[part]*1979u+activeRegions[part].count;
}
// Both live edits and clip gestures use the same bounded materialization.
// Replay never dirties the saved recipe or advances an audio/sequence clock.
std::array<std::array<bool,22>,4> sequenceRecipeOverrides{};
std::array<double,4> sequenceRecipeNext{};
struct RecipePlaybackCache {
    double position=-1; int length=-1,rate=-1,unit=-1,root=-1,scale=-1;
    float tempo=-1;std::uint64_t source=0;int direction=-1,path=-1,rotation=-1;
};
std::array<RecipePlaybackCache,4> sequenceRecipeCache{};
std::array<std::uint64_t,4> sequenceRecipeBuilds{};
void invalidateSequenceRecipePlayback(){sequenceRecipeCache={};sequenceRecipeNext.fill(0);}
bool materializeRecipeLane(int part,s3g_thru_sequence::Lane& lane,bool report){
    auto& recipe=lane.generator;if(!recipe.type)return false;
    s3g_thru_generate::Config config;config.v=recipe.values;config.v[0]=float(recipe.type);config.shift=recipe.shift;
    config.set(s3g_thru_generate::hits,config.v[1],lane.length,lane.timeMode);
    if(recipe.type==2&&config.v[2]==0)config.v[2]=1;
    auto base=lane;base.trigs=recipe.base;
    s3g_thru_generate::Context context;s3g_thru_generate::Result result;const char* mapName=nullptr;char status[64]{};
    recipe.values=config.v;
    const bool valid=buildSequenceRecipe(part,base,config,context,result,mapName,status,sizeof(status));
    ++sequenceRecipeBuilds[part];
    if(report)std::snprintf(sequenceTypeStatus[part].data(),sequenceTypeStatus[part].size(),"%s",status);
    if(!valid)return false;
    for(int i=0;i<32;++i)if(recipe.enabledMask&(1u<<i))result.lane.trigs[i].enabled=(recipe.enabledValues&(1u<<i))!=0;
    if(config.v[s3g_thru_generate::writePolicy]<.5f)for(int i=0;i<32;++i)if(recipe.burstMask&(1u<<i)){
        result.lane.trigs[i].count=recipe.base[i].count;
        for(int c=0;c<8;++c)result.lane.trigs[i].events[c].when=recipe.base[i].events[c].when;
    }
    // Refit after manual hit and Burst overrides have established the final grid.
    if(recipe.enabledMask||(config.v[s3g_thru_generate::writePolicy]<.5f&&recipe.burstMask))
        s3g_thru_generate::refitGaps(result.lane,base.trigs,config,context.stepMs);
    lane.trigs=result.lane.trigs;return true;
}
bool materializeSequenceType(int part,const s3g_thru_sequence::Lane* context=nullptr){
    auto& lane=sequenceLive.tracks[part];if(!lane.generator.type)return false;
    initializeSequenceRecipe(part,context);sequenceTypeSourceStamp[part]=sequenceRecipeStamp(part);
    auto generated=context?*context:lane;generated.generator=lane.generator;
    const bool ok=materializeRecipeLane(part,generated,true);
    lane.trigs=generated.trigs;lane.generator=generated.generator;
    sequencePlayback.tracks[part]=generated;invalidateSequenceRecipePlayback();return ok;
}
void captureSequenceRecipeRecording(int part){
    const auto& g=sequenceLive.tracks[part].generator;
    if(clipRecording&&g.type)for(int i=1;i<22;++i)
        if(clipReplacing||sequenceRecipeOverrides[part][i])
            captureClipPoint(ClipPointType::sequenceRecipe,i,part,i==21?float(g.shift):g.values[i]);
}
void seedSequenceRecipeRecording(){
    const auto* clip=activeClipSlot();if(!clip)return;
    for(int p=0;p<4;++p){const auto& g=(clipReplacing?sequenceLive:sequencePlayback).tracks[p].generator;
        if(g.type)for(int i=1;i<22;++i)
            if(clipReplacing||!clip->clipPointCounts[sequenceRecipeCurve(p,i)])
                captureClipPoint(ClipPointType::sequenceRecipe,i,p,i==21?float(g.shift):g.values[i]);
    }
}
void serviceSequenceRecipePlayback(){
    const auto* clip=activeClipSlot();if(!clipPlaying||(clipRecording&&clipReplacing)||!clip)return;
    for(int p=0;p<4;++p){
        auto& lane=sequencePlayback.tracks[p];auto& cache=sequenceRecipeCache[p];
        if(!lane.generator.type){sequenceRecipeNext[p]=INFINITY;continue;}
        const auto settings=settingsForPart(p);const auto stamp=sequenceRecipeStamp(p);
        if(cache.position>=0&&clipPositionMs>=cache.position&&clipPositionMs<sequenceRecipeNext[p]
            &&cache.length==lane.length&&cache.rate==lane.rate&&cache.unit==int(lane.timeMode)
            &&cache.tempo==tempoBpm&&cache.root==settings.root&&cache.scale==settings.scale&&cache.source==stamp
            &&(lane.generator.values[16]!=2||(cache.direction==lane.direction&&cache.path==lane.path&&cache.rotation==lane.rotation))){
            cache.position=clipPositionMs;continue;
        }
        const auto previous=lane.generator.values;const int previousShift=lane.generator.shift;
        lane.generator=sequenceLive.tracks[p].generator;
        auto& g=lane.generator;
        s3g_thru_generate::Config config;config.v=g.values;config.v[0]=float(g.type);config.shift=g.shift;
        sequenceRecipeNext[p]=INFINITY;
        for(int field=1;field<22;++field){
            const auto curve=sequenceRecipeCurve(p,field);
            const auto first=clip->clipPoints.begin()+clip->clipPointStarts[curve];
            const auto last=first+clip->clipPointCounts[curve];
            if(first==last)continue;
            const auto next=std::upper_bound(first,last,clipPositionMs,[](double time,const ClipPoint& point){return time<point.timeMs;});
            if(next!=last)sequenceRecipeNext[p]=std::min(sequenceRecipeNext[p],double(next->timeMs));
            if(!sequenceRecipeOverrides[p][field]){
                const auto value=(next==first?*(last-1):*(next-1)).value;
                if(field==21)config.shift=int(value);else config.v[field]=value;
            }
        }
        // Recorded bounds already use the unit of their accompanying UNIT
        // gesture. An authored/overridden bound retains its own saved unit.
        if(g.timed!=lane.timeMode){
            const float step=float(s3g_thru_sequence::rates[lane.rate]*60000.0/(std::max(1.f,tempoBpm)*clampValue(clip->clipRate,.25f,4.f)));
            for(int i:{17,18})if(!clip->clipPointCounts[kClipRecipeOffset+p*20+i-1]||sequenceRecipeOverrides[p][i])
                config.v[i]=clampValue(lane.timeMode?config.v[i]*step:config.v[i]/step,lane.timeMode?10.f:.01f,lane.timeMode?30000.f:32.f);
        }
        config.set(s3g_thru_generate::hits,config.v[1],lane.length,lane.timeMode);
        for(int i:{17,18})config.v[i]=clampValue(config.v[i],lane.timeMode?10.f:.01f,lane.timeMode?30000.f:32.f);
        // An untouched moving bound must respect its newly authored partner.
        const auto constrainPair=[&](int low,int high,float gap){
            if(config.v[low]+gap<=config.v[high])return;
            const bool lowMoves=clip->clipPointCounts[kClipRecipeOffset+p*20+low-1]>0&&!sequenceRecipeOverrides[p][low];
            const bool highMoves=clip->clipPointCounts[kClipRecipeOffset+p*20+high-1]>0&&!sequenceRecipeOverrides[p][high];
            if(lowMoves&&!highMoves)config.v[low]=std::max(0.f,config.v[high]-gap);
            else config.v[high]=high==11?std::min(1.f,config.v[low]+gap):config.v[low]+gap;
        };
        constrainPair(10,11,.00001f);constrainPair(17,18,0.f);
        g.values=config.v;g.timed=lane.timeMode;g.shift=config.shift;
        for(int field=1;field<22;++field)if(sequenceRecipeOverrides[p][field])
            captureSequenceOverdubHold(ClipPointType::sequenceRecipe,p,field,field==21?float(g.shift):g.values[field]);
        const bool changed=cache.position<0||previous!=g.values||previousShift!=g.shift||cache.length!=lane.length||cache.rate!=lane.rate||cache.unit!=int(lane.timeMode)||cache.tempo!=tempoBpm||cache.root!=settings.root||cache.scale!=settings.scale||cache.source!=stamp;
        const bool routeChanged=g.values[16]==2&&(cache.direction!=lane.direction||cache.path!=lane.path||cache.rotation!=lane.rotation);
        if(changed||routeChanged)materializeRecipeLane(p,lane,false);
        cache={clipPositionMs,lane.length,lane.rate,int(lane.timeMode),settings.root,settings.scale,tempoBpm,stamp,lane.direction,lane.path,lane.rotation};
    }
}
static bool validSequenceRecipePoint(std::uint32_t field,float value){
    if(field<1||field>21||!std::isfinite(value))return false;
    if(field==21)return value>=0&&value<=31&&std::floor(value)==value;
    s3g_thru_generate::Config c;
    // MIN/MAX can be recorded in either unit. The unit gesture determines
    // their range at playback; all other fields have a fixed domain.
    if(field==17||field==18)return value>=.01f&&value<=30000.f;
    return c.set(int(field),value,32,false)&&c.v[field]==value;
}
void serviceSequenceTypes(){
    if(!sequenceMode()||sequenceTypeBuilding||sequenceGenerateActive)return;
    bool changed=false;
    for(int i=0;i<4;++i){const int p=(sequenceTypeSourceCursor+i)%4;
        if(sequenceLive.tracks[p].generator.type&&sequenceTypeSourceStamp[p]!=sequenceRecipeStamp(p)){
            changed=materializeSequenceType(p);sequenceTypeSourceCursor=(p+1)%4;break;
        }
    }
    if(changed){if(auto* clip=activeClipSlot())clip->sequence=sequenceLive;++sequenceRevision;markPersistentStateDirty();}
}
void rememberSequenceTypeEdit(int part,int row,int child,const char* key){
    if(sequenceTypeBuilding||!key)return;
    auto& lane=sequenceLive.tracks[part];auto& g=lane.generator;if(!g.type)return;
    // Recipe controls already materialized in their effective playback unit.
    // Do not convert them back to the clip's final UNIT through this trig hook.
    if(sequenceTypeKeyKnown(key))return;
    initializeSequenceRecipe(part);auto& base=g.base[row];const auto& step=lane.trigs[row];
    if(!std::strcmp(key,"seq_toggle")||!std::strcmp(key,"seq_delete")||!std::strcmp(key,"seq_paste")){
        g.enabledMask|=1u<<row;if(step.enabled)g.enabledValues|=1u<<row;else g.enabledValues&=~(1u<<row);
    }
    if(!std::strcmp(key,"seq_delete")||!std::strcmp(key,"seq_paste"))base=step;
    else {
        const char* fields[]{"seq_pos","seq_note","seq_vel","seq_trig_gate"};
        auto& dst=child?base.events[child-1].locks:base.locks;
        const auto& from=child?step.events[child-1].locks:step.locks;
        for(int f=0;f<4;++f)if(!std::strcmp(key,fields[f])){dst.mask=(dst.mask&~(1u<<f))|(from.mask&(1u<<f));dst.values[f]=from.values[f];}
        if(!std::strcmp(key,"seq_clear")){dst.mask&=from.mask;if(!from.timeLocked)dst.timeLocked=false;}
        if(!std::strcmp(key,"seq_trig_time_ms")){dst.timeLocked=from.timeLocked;dst.timeMs=from.timeMs;}
        if(!std::strcmp(key,"seq_micro"))base.micro=step.micro;
        if(!std::strcmp(key,"seq_prob"))base.probability=step.probability;
        if(!std::strcmp(key,"seq_cycle_hit")||!std::strcmp(key,"seq_cycle_every")){base.cycleHit=step.cycleHit;base.cycleEvery=step.cycleEvery;}
        if(!std::strcmp(key,"seq_burst")){g.burstMask|=1u<<row;base.count=step.count;for(int i=0;i<8;++i)base.events[i].when=step.events[i].when;}
        if(!std::strcmp(key,"seq_when")&&child){g.burstMask|=1u<<row;base.count=step.count;base.events[child-1].when=step.events[child-1].when;}
    }
    if(!std::strcmp(key,"seq_length")||!std::strcmp(key,"seq_duration_mode")
        ||(g.values[16]==2&&(!std::strcmp(key,"seq_direction")||!std::strcmp(key,"seq_path")
            ||!std::strcmp(key,"seq_rotation")||!std::strcmp(key,"seq_rate")
            ||!std::strcmp(key,"seq_toggle")||!std::strcmp(key,"seq_delete")||!std::strcmp(key,"seq_paste")
            ||!std::strcmp(key,"seq_burst")||!std::strcmp(key,"seq_when")||!std::strcmp(key,"seq_micro"))))materializeSequenceType(part);
}
void rememberSequenceTypeEdit(){rememberSequenceTypeEdit(splitFocus,sequenceStep,sequenceChild,sequenceAuthorKey);}
std::array<char,32> sequenceRecipeUndoKey{};
int sequenceRecipeUndoPart=-1,sequenceRecipeUndoClip=-1;
std::uint64_t sequenceRecipeUndoAt=0;
bool setSequenceTypeParameter(const char* key,const char* text){
    if(!sequenceTypeKeyKnown(key))return false;
    if(!sequenceMode()){setError("Sequence mode required");return true;}
    const bool setupAction=!std::strcmp(key,"seq_type")||!std::strcmp(key,"seq_to_normal")||!std::strcmp(key,"seq_regenerate");
    if((clipRecording&&setupAction)||audioRecording()||patternBusy()||clipPreparationInFlight()){setError("Finish recording or clip transition");return true;}
    const float value=parseNumber(text);if(!std::isfinite(value))return true;
    const int part=splitFocus;auto& lane=sequenceLive.tracks[part];clearError();
    if(!std::strcmp(key,"seq_type")&&clampValue(int(std::lround(value)),0,2)==lane.generator.type)return true;
    if((!std::strcmp(key,"seq_to_normal")||!std::strcmp(key,"seq_regenerate"))&&value<=0)return true;
    const auto now=renderClockSamples.load(std::memory_order_relaxed);
    cancelSequenceGenerate();
    bool pushedRecipeUndo=false;
    if(!clipRecording&&(!undoAvailable||activeClip!=sequenceRecipeUndoClip||part!=sequenceRecipeUndoPart||std::strcmp(key,sequenceRecipeUndoKey.data())||now-sequenceRecipeUndoAt>std::uint64_t(sampleRate*2))){
        pushClipUndo(activeClip);pushedRecipeUndo=true;
    }
    sequenceRecipeUndoClip=activeClip;sequenceRecipeUndoPart=part;sequenceRecipeUndoAt=now;
    std::snprintf(sequenceRecipeUndoKey.data(),sequenceRecipeUndoKey.size(),"%s",key);
    if(!std::strcmp(key,"seq_type")){
        const int type=clampValue(int(std::lround(value)),0,2);
        const auto values=lane.generator.values;const bool preserve=lane.generator.ready,previousUnit=lane.generator.timed;
        if(type!=lane.generator.type){lane.generator.base=lane.trigs;lane.generator.ready=false;lane.generator.enabledMask=lane.generator.enabledValues=lane.generator.burstMask=0;}
        lane.generator.type=type;
        if(type){initializeSequenceRecipe(part);if(preserve){lane.generator.values=values;lane.generator.timed=previousUnit;initializeSequenceRecipe(part);}materializeSequenceType(part);}
    } else if(!std::strcmp(key,"seq_to_normal")){
        if(sequenceRunning()){lane.trigs=sequencePlayback.tracks[part].trigs;lane.generator.values=sequencePlayback.tracks[part].generator.values;}
        lane.generator.type=0;
    }
    else if(!std::strcmp(key,"seq_regenerate")){lane.generator.enabledMask=lane.generator.enabledValues=lane.generator.burstMask=0;materializeSequenceType(part);}
    else {
        const auto context=sequenceRunning()?sequencePlayback.tracks[part]:lane;
        initializeSequenceRecipe(part,&context);s3g_thru_generate::Config c;c.v=sequenceRunning()?context.generator.values:lane.generator.values;
        c.shift=context.generator.shift;const auto previous=c.v;const int previousShift=c.shift;
        for(int i=1;i<22;++i)if(!std::strcmp(key,sequenceRecipeKeys[i])){
            if(i==21)c.shift=int(std::lround(clampValue(value,0.f,float(context.length-1))));
            else c.set(i,value,context.length,context.timeMode);
        }
        const auto changed=[&](int i){return !std::strcmp(key,sequenceRecipeKeys[i])||(i==21?c.shift!=previousShift:c.v[i]!=previous[i]);};
        std::array<std::size_t,21> edited{};edited.fill(kClipCurveLaneCount);
        for(int i=1;i<22;++i)if(changed(i))edited[i-1]=sequenceRecipeCurve(part,i);
        replaceSequenceAutomation(edited,!pushedRecipeUndo);
        lane.generator.values=c.v;lane.generator.timed=context.timeMode;lane.generator.shift=c.shift;
        if(clipPlaying)for(int i=1;i<22;++i)if(changed(i))sequenceRecipeOverrides[part][i]=true;
        materializeSequenceType(part,&context);
    }
    sequenceTypeBuilding=true;ensureSequenceClip();commitSequenceEdit();sequenceTypeBuilding=false;return true;
}
