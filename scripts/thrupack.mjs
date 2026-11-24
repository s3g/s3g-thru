#!/usr/bin/env node
import fs from 'node:fs/promises';
import path from 'node:path';
import {parseCompositionJSON,validateComposition} from '../src/assets/composer/format.mjs';
import {openPack,buildPack,allowedPath,inflate} from '../src/assets/composer/pack.mjs';
const [command,input,output]=process.argv.slice(2);
async function load(file){const bytes=new Uint8Array(await fs.readFile(file));return file.endsWith('.json')?{doc:validateComposition(parseCompositionJSON(new TextDecoder('utf-8',{fatal:true}).decode(bytes))),files:new Map()}:openPack(bytes);}
try {
 if(!['validate','inspect','pack','unpack'].includes(command)||!input)throw Error('Usage: node scripts/thrupack.mjs validate|inspect FILE\n       node scripts/thrupack.mjs pack composition.json OUTPUT.thrupack\n       node scripts/thrupack.mjs unpack INPUT.thrupack NEW_DIRECTORY');
 const pack=await load(input);
 if(command==='validate')console.log(`Valid s3g-thrupack v1: ${pack.doc.clips.length} clips, ${pack.doc.assets.length} sources`);
 if(command==='inspect')console.log(JSON.stringify({title:pack.doc.title,tempo:pack.doc.tempo,clips:pack.doc.clips.map(c=>({id:c.id,name:c.name,mode:c.mode,engines:c.parts.map(p=>p.engine),notes:c.notes?.length||0,controlPoints:c.automation?.reduce((n,c)=>n+c.points.length,0)||0})),chains:pack.doc.chains?.length||0,assets:pack.doc.assets.map(a=>({id:a.id,name:a.name,included:pack.files.has(a.file)}))},null,2));
 if(command==='pack'){
  if(!output)throw Error('Provide a new output filename');
  const base=path.dirname(path.resolve(input));
  for(const a of pack.doc.assets)if(a.file&&!pack.files.has(a.file)){if(!allowedPath(a.file))throw Error('Unsafe asset path');pack.files.set(a.file,new Uint8Array(await fs.readFile(path.join(base,a.file))));}
  for(const capsule of Object.values(pack.doc.extensions?.s3gThru?.clips||{}))if(!pack.files.has(capsule.file)){if(!allowedPath(capsule.file))throw Error('Unsafe native path');pack.files.set(capsule.file,new Uint8Array(await fs.readFile(path.join(base,capsule.file))));}
  const blob=await buildPack(pack.doc,pack.files);await fs.writeFile(output,new Uint8Array(await blob.arrayBuffer()),{flag:'wx'});console.log(`Wrote ${output}`);
 }
 if(command==='unpack'){
  if(!output)throw Error('Provide a new destination directory');await fs.mkdir(output);const root=path.resolve(output);
  for(const[name,entry]of pack.files){if(!allowedPath(name))throw Error('Unsafe pack path');const target=path.join(root,name);await fs.mkdir(path.dirname(target),{recursive:true});await fs.writeFile(target,await inflate(entry),{flag:'wx'});}
  console.log(`Extracted ${output}`);
 }
}catch(error){console.error(error.message);process.exitCode=1;}
