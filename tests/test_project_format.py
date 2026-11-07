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
                        'clip_format': 36, 'assets': 1}
    state = json.loads(archive.read('thru-state.json'))
    clips = archive.read('thru-clips.bin')
    version, generation = struct.unpack_from('<IQ', clips, 8)
    assert version == 36 and generation == state['generation']
    checksum = 2166136261
    for value in clips[:-4]:
        checksum = ((checksum ^ value) * 16777619) & 0xffffffff
    assert checksum == struct.unpack('<I', clips[-4:])[0]
    assert b'assets/a000000.wav' in clips and b'/thru-project-source.' not in clips
    wave = archive.read('assets/a000000.wav')
    assert wave[:4] == b'RIFF' and wave[8:12] == b'WAVE'
    assert struct.unpack_from('<ff', wave, 44) == (.125, -.0625)
print('s3g Thru project format: independent ZIP64/CRC/JSON/binary checksum and portable stereo asset validation: PASS')
