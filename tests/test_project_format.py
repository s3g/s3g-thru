"""Check the native export with Python's independent standard ZIP reader."""
import json
import struct
import sys
from pathlib import Path
DESTINATION_COUNT = len(json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/lfo-destinations.json').read_text()))
import zipfile

with zipfile.ZipFile(sys.argv[1]) as archive:
    assert archive.testzip() is None
    assert set(archive.namelist()) == {
        'manifest.json', 'thru-state.json', 'thru-clips.bin', 'assets/a000000.wav'}
    manifest = json.loads(archive.read('manifest.json'))
    assert manifest == {'format': 's3g-thru-project', 'version': 1,
                        'clip_format': 60, 'assets': 1}
    state = json.loads(archive.read('thru-state.json'))
    clips = archive.read('thru-clips.bin')
    version, generation = struct.unpack_from('<IQ', clips, 8)
    assert version == 60 and generation == state['generation']
    checksum = 2166136261
    for value in clips[:-4]:
        checksum = ((checksum ^ value) * 16777619) & 0xffffffff
    assert checksum == struct.unpack('<I', clips[-4:])[0]
    lowform_size=4+64*5*4*32*4
    lowform_at=len(clips)-4-lowform_size
    assert clips[lowform_at:lowform_at+4]==b'LFP1'
    meta=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/lowform-parameters.json').read_text())
    for i,x in enumerate(struct.unpack_from('<'+str(64*5*4*32)+'f',clips,lowform_at+4)):
        m=meta[i%32];assert m['min']<=x<=m['max']
        if m['options'] or i%32==5:assert x==int(x)
    clips=clips[:lowform_at]+clips[-4:]
    partials_size=4+64*5*4*104*4
    partials_at=len(clips)-4-partials_size
    assert clips[partials_at:partials_at+4]==b'PTP1'
    meta=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/partials-parameters.json').read_text())
    for i,x in enumerate(struct.unpack_from('<'+str(64*5*4*104)+'f',clips,partials_at+4)):
        m=meta[i%104];assert m['min']<=x<=m['max']
        if m['options']:assert x==int(x)
    clips=clips[:partials_at]+clips[-4:]
    reson_size=4+64*5*4*32*4
    reson_at=len(clips)-4-reson_size
    assert clips[reson_at:reson_at+4]==b'RSP1'
    meta=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/reson-parameters.json').read_text())
    for i,x in enumerate(struct.unpack_from('<'+str(64*5*4*32)+'f',clips,reson_at+4)):
        m=meta[i%32];assert m['min']<=x<=m['max']
        if m['options']:assert x==int(x)
    clips=clips[:reson_at]+clips[-4:]
    noise_size=4+64*5*4*88*4
    noise_at=len(clips)-4-noise_size
    assert clips[noise_at:noise_at+4]==b'NIS1'
    meta=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/noise-parameters.json').read_text())
    for i,x in enumerate(struct.unpack_from('<'+str(64*5*4*88)+'f',clips,noise_at+4)):
        m=meta[i%88];assert m['min']<=x<=m['max']
    clips=clips[:noise_at]+clips[-4:]
    extra_size=4+64*5*4*9*4
    extra_at=len(clips)-4-extra_size
    assert clips[extra_at:extra_at+4]==b'FMX1'
    meta=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/fm-parameters.json').read_text())
    for i,x in enumerate(struct.unpack_from('<'+str(64*5*4*9)+'f',clips,extra_at+4)):
        m=meta[23+i%9];assert m['min']<=x<=m['max']
    clips=clips[:extra_at]+clips[-4:]
    fm_size=4+64*5*4*23*4
    fm_at=len(clips)-4-fm_size
    assert clips[fm_at:fm_at+4]==b'FMP1'
    fm_meta=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/fm-parameters.json').read_text())
    for i,x in enumerate(struct.unpack_from('<'+str(64*5*4*23)+'f',clips,fm_at+4)):
        m=fm_meta[i%23];assert m['min']<=x<=m['max']
        if m['options']:assert x==int(x)
    clips=clips[:fm_at]+clips[-4:]
    amp_size=4+64*5*4*8*4
    amp_at=len(clips)-4-amp_size
    assert clips[amp_at:amp_at+4]==b'SAP1'
    amp_meta=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/scanned-parameters.json').read_text())[23:]
    for i,x in enumerate(struct.unpack_from('<'+str(64*5*4*8)+'f',clips,amp_at+4)):
        m=amp_meta[i%8];assert m['min']<=x<=m['max']
        if m['options']:assert x==int(x)
    clips=clips[:amp_at]+clips[-4:]
    hit_size=4+64*5*4*7*4
    hit_at=len(clips)-4-hit_size
    assert clips[hit_at:hit_at+4]==b'SHP1'
    hit_meta=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/scanned-parameters.json').read_text())[16:23]
    for i,x in enumerate(struct.unpack_from('<'+str(64*5*4*7)+'f',clips,hit_at+4)):
        m=hit_meta[i%7];assert m['min']<=x<=m['max']
        if m['options']:assert x==int(x)
    clips=clips[:hit_at]+clips[-4:]
    scan_size=4+64*5*4*16*4
    scan_at=len(clips)-4-scan_size
    assert clips[scan_at:scan_at+4]==b'SCN1'
    scan_meta=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/scanned-parameters.json').read_text())
    for i,x in enumerate(struct.unpack_from('<'+str(64*5*4*16)+'f',clips,scan_at+4)):
        m=scan_meta[i%16];assert m['min']<=x<=m['max']
        if m['options'] or m['symbol']=='seed':assert x==int(x)
    clips=clips[:scan_at]+clips[-4:]
    sound_size=struct.unpack_from('<I',clips,len(clips)-8)[0]
    sound_at=len(clips)-4-sound_size
    assert clips[sound_at:sound_at+4]==b'SLK1'
    at=sound_at+4
    for clip_index in range(64):
        count=struct.unpack_from('<I',clips,at)[0];at+=4
        assert count<=1024
        previous=-1;step_counts={}
        for n in range(count):
            packed,value=struct.unpack_from('<If',clips,at);at+=8
            assert packed>previous;previous=packed
            route,id=packed>>16,packed&65535
            assert route<4*32*9
            catalog=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/sequence-locks.json').read_text())
            assert id in {entry['id'] for entry in catalog}
            step_counts[route//9]=step_counts.get(route//9,0)+1
            assert step_counts[route//9]<=32
    assert at==len(clips)-8
    clips=clips[:sound_at]+clips[-4:]
    table_meta=json.loads((Path(__file__).resolve().parents[1]/'controllers/midi/table-parameters.json').read_text())
    table_size=4+64*5*4*40*4
    table_at=len(clips)-4-table_size
    assert clips[table_at:table_at+4]==b'TAB1'
    values=struct.unpack_from('<'+str(64*5*4*40)+'f',clips,table_at+4)
    for i,x in enumerate(values):
        m=table_meta[i%40]
        assert m['min']<=x<=m['max']
        if m['options'] or m['symbol']=='seed':assert x==int(x)
    clips=clips[:table_at]+clips[-4:]
    auto_size=4+64*8*72
    auto_at=len(clips)-4-auto_size
    assert clips[auto_at:auto_at+4]==b'LAT1'
    at=auto_at+4
    for i in range(64*8):
        offset,span=struct.unpack_from('<If',clips,at)
        assert offset in (0,1) and 0<=span<=1.001
        for automatic,width in struct.iter_unpack('<ff',clips[at+8:at+72]):
            assert 0<=width<=1.001 and abs(automatic)<=width+.0001
        at+=72
    assert at==len(clips)-4
    clips=clips[:auto_at]+clips[-4:]
    pool_size = 4 + 64 * (8 * 200 + 4 * 2 * 32 * 72)
    pool_at = len(clips) - 4 - pool_size
    assert clips[pool_at:pool_at+4] == b'LFP2'
    at = pool_at + 4
    for clip in range(64):
        for slot in range(8):
            controls = struct.unpack_from('<9f', clips, at)
            assert 0 <= controls[0] < DESTINATION_COUNT and 0 <= controls[1] <= 6 and controls[2] in (0, 1)
            assert 1/256 <= controls[3] <= 16 and -1 <= controls[4] <= 1 and controls[5] in range(4)
            assert 0 <= controls[6] <= 1 and 3 <= controls[7] <= 1000 and controls[8] in (0, 1)
            target, engine, anchor, seed, cycle, captured, output, running = struct.unpack_from('<IId3IfI', clips, at+36)
            assert target <= 4 and engine <= 17 and 0 <= anchor < 1
            assert captured in (0, 1) and running in (0, 1) and abs(output) <= 1.001
            for route in range(8):
                part, alg, dest, value = struct.unpack_from('<IIIf', clips, at+72+route*16)
                assert part <= 5 and alg <= 17 and dest < DESTINATION_COUNT and abs(value) <= 1.001
            at += 200
        for step in range(4*2*32):
            mask, reset = struct.unpack_from('<II', clips, at)
            assert mask <= 65535 and reset <= 255
            values = struct.unpack_from('<16f', clips, at+8)
            assert all(1/256 <= x <= 16 for x in values[::2])
            assert all(abs(x) <= 1 for x in values[1::2])
            at += 72
    assert at == len(clips)-4
    clips = clips[:pool_at] + clips[-4:]
    lfo_size=4+64*(10*(9*4+8+4*4+4+15*4)+4*2*32*24)
    lfo_at=len(clips)-4-lfo_size
    assert clips[lfo_at:lfo_at+4]==b'LFO1'
    at=lfo_at+4
    for clip in range(64):
        for group in range(5):
            for slot in range(2):
                # 9 controls, phase, seed, cycle, captured, output, routes, running.
                # Read explicitly because the running bit follows the route weights.
                controls=struct.unpack_from('<9f',clips,at)
                assert 0<=controls[0]<=14 and 0<=controls[1]<=6 and controls[2] in (0,1)
                assert 1/256<=controls[3]<=16 and -1<=controls[4]<=1 and controls[5] in range(4)
                assert 0<=controls[6]<=1 and 3<=controls[7]<=1000 and controls[8] in (0,1)
                anchor,seed,cycle,captured,output=struct.unpack_from('<d3If',clips,at+36)
                assert 0<=anchor<1 and captured in (0,1) and abs(output)<=1.001
                routes=struct.unpack_from('<15f',clips,at+60)
                assert all(0<=x<=1.001 for x in routes) and sum(routes)<=1.001
                assert struct.unpack_from('<I',clips,at+120)[0] in (0,1)
                at+=124
        for _ in range(4*2*32):
            mask,reset,r0,d0,r1,d1=struct.unpack_from('<II4f',clips,at)
            assert mask<=15 and reset<=3 and all(1/256<=x<=16 for x in (r0,r1)) and all(abs(x)<=1 for x in (d0,d1))
            at+=24
    assert at==len(clips)-4
    clips=clips[:lfo_at]+clips[-4:]
    path_size = 4+64*4*4
    path_at = len(clips)-4-path_size
    assert clips[path_at:path_at+4] == b'PTH1'
    assert all(0 <= value <= 2 for value in struct.unpack_from('<256I', clips, path_at+4))
    clips = clips[:path_at] + clips[-4:]
    shift_size = 4+64*4*4
    shift_at = len(clips)-4-shift_size
    assert clips[shift_at:shift_at+4] == b'SHT1'
    assert all(0 <= value <= 31 for value in struct.unpack_from('<256I', clips, shift_at+4))
    clips = clips[:shift_at] + clips[-4:]
    direction_size = 4+64*4*8
    direction_at = len(clips)-4-direction_size
    assert clips[direction_at:direction_at+4] == b'SDI1'
    directions = struct.unpack_from('<512I', clips, direction_at+4)
    assert all(0 <= d <= 4 for d in directions[::2]) and all(directions[1::2])
    clips = clips[:direction_at] + clips[-4:]
    recipe_size = struct.unpack_from('<I', clips, len(clips)-8)[0]
    recipe_start = len(clips)-8-recipe_size
    assert clips[recipe_start:recipe_start+4] == b'SYT1'
    # Earlier extension offsets remain relative to the checksum-free old tail.
    legacy_clips = clips[:recipe_start] + clips[-4:]
    assert legacy_clips[-1032:-1028] == b'PSE1'
    seeds = struct.unpack_from('<256I', legacy_clips, len(legacy_clips)-1028)
    assert all(seeds) and len(set(seeds)) == 256
    assert tuple(seeds[:4]) == tuple(state[f'pulsar_seed_{i}'] for i in range(4))
    assert legacy_clips[-2060:-2056] == b'SLP1'
    preferences = struct.unpack_from('<256f', legacy_clips, len(legacy_clips)-2056)
    assert all(v == -1 or 0 <= v <= 3 for v in preferences)
    assert b'assets/a000000.wav' in clips and b'/thru-project-source.' not in clips
    wave = archive.read('assets/a000000.wav')
    assert wave[:4] == b'RIFF' and wave[8:12] == b'WAVE'
    assert struct.unpack_from('<ff', wave, 44) == (.125, -.0625)
print('s3g Thru project format: independent ZIP64/CRC/JSON/binary checksum and portable stereo asset validation: PASS')
