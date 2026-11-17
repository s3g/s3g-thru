"""Check the native export with Python's independent standard ZIP reader."""
import json
import struct
import sys
import zipfile

with zipfile.ZipFile(sys.argv[1]) as archive:
    assert archive.testzip() is None
    assert set(archive.namelist()) == {
        'manifest.json', 'thru-state.json', 'thru-clips.bin', 'assets/a000000.wav'}
    manifest = json.loads(archive.read('manifest.json'))
    assert manifest == {'format': 's3g-thru-project', 'version': 1,
                        'clip_format': 46, 'assets': 1}
    state = json.loads(archive.read('thru-state.json'))
    clips = archive.read('thru-clips.bin')
    version, generation = struct.unpack_from('<IQ', clips, 8)
    assert version == 46 and generation == state['generation']
    checksum = 2166136261
    for value in clips[:-4]:
        checksum = ((checksum ^ value) * 16777619) & 0xffffffff
    assert checksum == struct.unpack('<I', clips[-4:])[0]
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
