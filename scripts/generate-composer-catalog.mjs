import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {MIDI_PARAMETERS} from '../src/midi-map.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const text=fs.readFileSync(path.join(root,'include/thru_midi_parameters.h'),'utf8');
const locks=new Map([...text.matchAll(/^\s*\{(\d+),\s*"([^"]+)",([^}]+)\},/gm)].map(m=>{const f=m[3].split(',');return[Number(m[1]),{initial:Number(f[2].trim().replace(/f$/,'')),lock:Number(f.at(-1))}];}));
const descriptors=new Set([...fs.readFileSync(path.join(root,'include/thru_sequence_lock_catalog.h'),'utf8').matchAll(/^\s*\{(\d+),/gm)].map(m=>Number(m[1])));
const families=[[4200, 'table'], [4300, 'scanned'], [4400, 'fm'], [4500, 'noise'], [4600, 'reson'], [4800, 'partials'], [5000, 'lowform']];
const ranges=families.map(([base,name])=>[base,Number(fs.readFileSync(path.join(root,'include',`thru_${name}_parameters.h`),'utf8').match(/\bfields\s*=\s*(\d+)/)[1])]);
const parameters=MIDI_PARAMETERS.filter(p=>{const l=locks.get(p.id);return !p.action&&l&&(l.lock>=0||ranges.some(([base,fields])=>p.id>=base&&p.id<base+fields)||p.id===5091||p.id>=5200&&p.id<5213);}).map(p=>({...p,...locks.get(p.id),lockable:descriptors.has(p.id)}));
if(!parameters.length||parameters.some(p=>!Number.isFinite(p.initial)||!Number.isFinite(p.lock)))throw new Error('Invalid composer parameter registry');
const output='// Generated from the existing stable MIDI/sound-lock registries.\nexport const PARAMETERS = '+JSON.stringify(parameters)+';\nexport const PANEL_PARAMETERS = '+JSON.stringify(MIDI_PARAMETERS.filter(p=>!p.action).map(p=>({...p,...locks.get(p.id),lockable:descriptors.has(p.id)})))+';\n';
const target=path.join(root,'src/assets/composer/catalog.mjs');
if(process.argv.includes('--check')){if(!fs.existsSync(target)||fs.readFileSync(target,'utf8')!==output)throw new Error('Composer catalog is stale');}
else {fs.mkdirSync(path.dirname(target),{recursive:true});fs.writeFileSync(target,output);}
console.log(`s3g Thru composer sound catalog: PASS (${parameters.length} parameters)`);
