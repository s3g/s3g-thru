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
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
  "$test_dir/test-thru-sanitize"

node "$repo_dir/tests/test_ui.mjs" "$repo_dir/src/ui.js"

echo "S3G Thru host and sanitizer tests: PASS"

