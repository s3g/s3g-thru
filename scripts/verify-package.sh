#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
archive="${1:-$repo_dir/dist/s3g-thru-module.tar.gz}"
verify_dir="$(mktemp -d /tmp/s3g-thru-package.XXXXXX)"
trap 'rm -rf "$verify_dir"' EXIT

test -f "$archive"
tar -xzf "$archive" -C "$verify_dir"
test -f "$verify_dir/s3g-thru/module.json"
test -f "$verify_dir/s3g-thru/ui.js"
test -f "$verify_dir/s3g-thru/dsp.so"
test -f "$verify_dir/s3g-thru/LICENSE"
test -f "$verify_dir/s3g-thru/THIRD_PARTY_NOTICES.md"
test ! -e "$verify_dir/s3g-thru/canvas.js"

file_count="$(find "$verify_dir/s3g-thru" -maxdepth 1 -type f | wc -l | tr -d ' ')"
test "$file_count" = "5"

entry_count="$(find "$verify_dir" -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')"
test "$entry_count" = "1"
file "$verify_dir/s3g-thru/dsp.so" | grep -Eq 'ELF 64-bit.*ARM aarch64'
python3 -m json.tool "$verify_dir/s3g-thru/module.json" >/dev/null
node --check "$verify_dir/s3g-thru/ui.js"

echo "S3G Thru package verification: PASS"

