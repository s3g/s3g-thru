#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
test_dir="$(mktemp -d /tmp/s3g-thru-tests.XXXXXX)"
trap 'rm -rf "$test_dir"' EXIT

cxx_bin="${CXX:-clang++}"
common=(
  -std=c++17 -Wall -Wextra -Werror
  -Wno-deprecated-declarations
  -pthread
  -I"$repo_dir/include"
  "$repo_dir/src/dsp/thru_module.cpp"
  "$repo_dir/tests/test_thru.cpp"
)

"$cxx_bin" -O2 "${common[@]}" -o "$test_dir/test-thru"
"$test_dir/test-thru"

"$cxx_bin" -O1 -g -fno-omit-frame-pointer \
  -fsanitize=address,undefined "${common[@]}" \
  -o "$test_dir/test-thru-sanitize"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  "$test_dir/test-thru-sanitize"

node "$repo_dir/tests/test_ui.mjs" "$repo_dir/src/ui.js"

set_dir="$test_dir/set data"
set_archive="$test_dir/exported set.tar.gz"
mkdir -p "$set_dir/captures"
touch "$set_dir/thru-state.json" "$set_dir/thru-clips.bin" \
  "$set_dir/thru-state.json.bak" "$set_dir/thru-clips.bin.bak" \
  "$set_dir/thru-lane-1.wav" "$set_dir/captures/c01-lane-1.wav" \
  "$set_dir/captures/unfinished.wav.tmp"
"$repo_dir/scripts/export-set.sh" "$set_dir" "$set_archive"
archive_entries="$(tar -tzf "$set_archive")"
grep -qx 'thru-state.json' <<<"$archive_entries"
grep -qx 'thru-clips.bin' <<<"$archive_entries"
grep -qx 'thru-state.json.bak' <<<"$archive_entries"
grep -qx 'thru-clips.bin.bak' <<<"$archive_entries"
grep -qx 'thru-lane-1.wav' <<<"$archive_entries"
grep -qx 'captures/c01-lane-1.wav' <<<"$archive_entries"
if grep -q '\.tmp$' <<<"$archive_entries"; then
  echo "set export included a temporary file" >&2
  exit 1
fi
if "$repo_dir/scripts/export-set.sh" "$set_dir" "$set_archive" >/dev/null 2>&1; then
  echo "set export overwrote an existing archive" >&2
  exit 1
fi

echo "S3G Thru host and sanitizer tests: PASS"

