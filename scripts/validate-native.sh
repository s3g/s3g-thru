#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"

"$repo_dir/scripts/test-native.sh"
node --check "$repo_dir/src/ui.js"
python3 -m json.tool "$repo_dir/src/module.json" >/dev/null
python3 -m json.tool "$repo_dir/release.json" >/dev/null
node "$repo_dir/scripts/validate-release.mjs"
node "$repo_dir/scripts/render-doc-screens.mjs" --check
node "$repo_dir/scripts/check-docs.mjs"

python3 - "$repo_dir/src/module.json" <<'PY'
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
module = json.loads(path.read_text())
assert module["id"] == "s3g-thru"
assert module["name"] == "s3g Thru"
assert module["author"] == "s3g"
assert module["component_type"] == "overtake"
assert module["ui"] == "ui.js"
assert module["dsp"] == "dsp.so"
assert module["api_version"] == 2
assert module["capabilities"]["audio_in"] is True
assert module["capabilities"]["audio_out"] is True
assert module["capabilities"]["component_type"] == "overtake"
PY

if [[ -f "$repo_dir/build/native/s3g-thru/dsp.so" ]]; then
  file "$repo_dir/build/native/s3g-thru/dsp.so" | grep -Eq 'ELF 64-bit.*ARM aarch64'
  if [[ -n "${AARCH64_NM:-}" ]]; then
    nm_bin="$AARCH64_NM"
  elif command -v aarch64-unknown-linux-gnu-nm >/dev/null 2>&1; then
    nm_bin="aarch64-unknown-linux-gnu-nm"
  else
    nm_bin="aarch64-linux-gnu-nm"
  fi
  "$nm_bin" -D --defined-only "$repo_dir/build/native/s3g-thru/dsp.so" | \
    grep -Eq '[[:space:]]T[[:space:]]move_plugin_init_v2$'
fi

echo "s3g Thru native validation: PASS"

