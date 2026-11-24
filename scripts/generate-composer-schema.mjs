import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {ENGINES,MODES,RATES,DIRECTIONS,PATHS,POINT_TYPES,newComposition} from '../src/assets/composer/format.mjs';
const num=(minimum,maximum,type='number')=>({type,minimum,maximum});
const text=(maxLength=120)=>({type:'string',maxLength});
const choice=values=>({enum:values});
const array=(items,maxItems,minItems=0)=>({type:'array',items,maxItems,minItems});
const ref=name=>({$ref:'#/$defs/'+name});
const object=(properties,required=[])=>({type:'object',additionalProperties:false,properties,required});
const name={type:'string',maxLength:40,pattern:'^[A-Za-z0-9 _-]*$'};
const id={type:'string',maxLength:64,pattern:'^[A-Za-z0-9_-]+$'};
const anyObject={type:'object'};
const locks=object({position:num(0,1),pitch:num(-48,48,'integer'),velocity:num(1/127,1),duration:ref('duration'),sound:ref('parameters')});
const trig=object({step:num(0,31,'integer'),enabled:{type:'boolean'},microtime:num(-.5,.5),probability:num(0,1),cycle:object({hit:num(1,8,'integer'),every:num(1,8,'integer')},['hit','every']),locks:ref('locks'),burst:array(object({offset:{type:'number',minimum:0,exclusiveMaximum:1},locks:ref('locks')},['offset']),8,1),protected:{type:'boolean'}},['step']);
const base=structuredClone(trig);base.required=[];delete base.properties.step;
const fade=object({mode:choice(['default','cut','fixed','random']),timeMs:num(0,30000),lowMs:num(0,30000),highMs:num(0,30000),curve:choice(['linear','in','out','smooth','power'])},['mode']);
const schema={
 $schema:'https://json-schema.org/draft/2020-12/schema',
 $id:'https://s3g.github.io/s3g-thru/assets/formats/thrupack-v1.schema.json',
 title:'s3g Thru portable composition version 1',
 description:'Structural schema. Run scripts/thrupack.mjs validate for cross-reference, registered-parameter, timing-unit and event-budget validation. Move performs final engine/ownership/take validation before playback.',
 ...object({$schema:text(300),format:{const:'s3g-thrupack'},version:{const:1},title:name,tempo:num(1,999),assets:array(ref('asset'),512),clips:array(ref('clip'),64,1),chains:array(ref('chain'),16),extensions:anyObject,provenance:{}},['format','version','title','tempo','assets','clips']),
 $defs:{
  duration:{oneOf:[object({unit:{const:'ms'},value:num(10,30000)},['unit','value']),object({unit:{const:'steps'},value:num(.01,32)},['unit','value'])]},
  parameters:{type:'object',maxProperties:943,additionalProperties:{type:'number'},description:'Keys are stable parameter keys from src/assets/composer/catalog.mjs. Actions are prohibited. Trig sound locks use the lockable subset.'},
  locks,trig,base,
  generator:object({type:choice(['normal','euclidean','onset']),shift:num(0,31,'integer'),ready:{type:'boolean'},timed:{type:'boolean'},values:array(num(-1,30000),21,21),base:array(ref('base'),32,32),enabledMask:num(0,4294967295,'integer'),enabledValues:num(0,4294967295,'integer'),burstMask:num(0,4294967295,'integer')},['type']),
  track:object({length:num(1,32,'integer'),rate:choice(RATES),rotation:num(0,31,'integer'),direction:choice(DIRECTIONS),path:choice(PATHS),seed:num(0,4294967295,'integer'),enabled:{type:'boolean'},scaleEntry:{type:'boolean'},defaults:ref('locks'),trigs:array(ref('trig'),32),generator:ref('generator')},['length','rate','trigs']),
  asset:object({id,name:text(200),sha256:{type:'string',pattern:'^[a-f0-9]{64}$'},size:num(1,536870912,'integer'),file:{type:'string',pattern:'^assets/[a-f0-9]{64}\\.wav$'},hint:text(200),url:{type:'string',maxLength:768,pattern:'^/files/download\\?path='},waveform:array(num(-1,1),256),frames:num(1,2147483647,'integer'),sampleRate:num(8000,192000)},['id','name']),
  note:object({timeMs:num(0,86400000,'integer'),durationMs:num(1,86400000,'integer'),note:num(0,127,'integer'),velocity:num(0,127,'integer'),pad:num(0,255,'integer'),part:num(0,3,'integer'),take:num(0,255,'integer'),engineEntry:{enum:[0,1,2,3,4,5,6,7,255]},engine:{enum:[null,...ENGINES]},pressure:num(0,1),variation:object({octave:num(-4,4,'integer'),position:num(-1,1),stack:num(-1,1),pan:num(-1,1)})},['timeMs','durationMs','note','velocity']),
  automation:object({windows:array({type:'array',prefixItems:[num(0,86400000,'integer'),num(1,86400000,'integer')],minItems:2,maxItems:2},128),target:object({type:choice(POINT_TYPES),key:text(),index:num(0,255,'integer'),owner:num(0,255,'integer'),take:num(0,255,'integer')},['type']),points:array({type:'array',prefixItems:[num(0,86400000,'integer'),num(-1000000,1000000)],minItems:2,maxItems:2},8192,1)},['target','points']),
  clip:object({id,name,mode:choice(MODES),sources:array(object({asset:{type:['string','null']},label:text(63)}),4,4),parts:array(object({engine:choice(ENGINES),root:num(0,127,'integer'),scale:num(0,24,'integer'),parameters:ref('parameters')},['engine']),4,1),parameters:ref('parameters'),sequence:object({tracks:array(ref('track'),4,4),mix:anyObject},['tracks']),notes:array(ref('note'),1024),automation:array(ref('automation'),8192),loop:object({durationMs:num(1,86400000,'integer'),beats:num(.0001,100000),recordedTempo:num(1,999),rate:num(.25,4),timing:choice(['free','beats']),takes:num(0,255,'integer'),recorded:{type:'boolean'}}),extensions:anyObject},['id','name','mode','sources','parts']),
  chain:object({name,end:choice(['loop','stay','stop']),fade:ref('fade'),entries:array(object({clip:id,repeats:num(0,16,'integer'),enabled:{type:'boolean'},fade:ref('fade')},['clip']),32)},['name','end','entries']),fade
 }
};
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..'),target=path.join(root,'formats/thrupack-v1.schema.json');
const out=JSON.stringify(schema,null,2)+'\n';if(process.argv.includes('--check')){if(fs.readFileSync(target,'utf8')!==out)throw Error('Regenerate composer schema');}else{fs.mkdirSync(path.dirname(target),{recursive:true});fs.writeFileSync(target,out);}
