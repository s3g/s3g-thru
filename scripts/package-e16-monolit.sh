#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
node "$repo_dir/controllers/oxi-e16-monolit/generate-scene.mjs" --check

python3 - "$repo_dir" <<'PY'
import hashlib
import json
from pathlib import Path
import sys
import zipfile

root = Path(sys.argv[1])
source = root / 'controllers/oxi-e16-monolit'
scene_name = 'Scenes/THRU E16 MNLT.oxie16'
scene = json.loads((source / scene_name).read_text())
assert scene['title'] == 'THRU EM'
assert scene['code']['scriptName'] == 'Thru Rings'
assert len(scene['code']['code'].encode()) < 2000
files = {name: (source / name).read_bytes() for name in ['README.md', scene_name, 'Scripts/Thru Rings.e16script']}
files['Monolit/presets/s3g-thru'] = (root / 'controllers/lightreft-monolit/presets/s3g-thru').read_bytes()
files['Monolit/mapping.csv'] = (root / 'controllers/lightreft-monolit/mapping.csv').read_bytes()
folder = root / 'dist/e16-monolit'
for name in ['Scenes', 'Scripts']:
    (folder / name).mkdir(parents=True, exist_ok=True)
for name, data in files.items():
    (folder / name).parent.mkdir(parents=True, exist_ok=True)
    (folder / name).write_bytes(data)
archive = root / 'dist/s3g-thru-e16-monolit.zip'
temporary = archive.with_suffix('.zip.tmp')
with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED) as output:
    for name, data in [('Scenes/', b''), ('Scripts/', b''), *sorted(files.items())]:
        entry = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
        entry.create_system = 3
        entry.external_attr = (0o40755 if name.endswith('/') else 0o100644) << 16
        entry.compress_type = zipfile.ZIP_DEFLATED
        output.writestr(entry, data)
with zipfile.ZipFile(temporary) as check:
    assert set(check.namelist()) == set(files) | {'Scenes/', 'Scripts/'}
    for name, data in files.items():
        assert check.read(name) == data, name
    assert check.testzip() is None
temporary.replace(archive)
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
archive.with_name(archive.name + '.sha256').write_text(digest + '  ' + archive.name + '\n')
print('E16 relative CC + Monolit bundle: PASS')
print('OXI App storage folder: ' + str(folder))
print('Packaged ' + str(archive))
print('SHA256: ' + digest)
PY
