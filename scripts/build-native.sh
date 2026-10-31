#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
module_dir="$repo_dir/build/native/s3g-thru"

if [[ -n "${AARCH64_CXX:-}" ]]; then
  cxx_bin="$AARCH64_CXX"
elif command -v aarch64-unknown-linux-gnu-g++ >/dev/null 2>&1; then
  cxx_bin="aarch64-unknown-linux-gnu-g++"
elif command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
  cxx_bin="aarch64-linux-gnu-g++"
else
  echo "No aarch64 Linux C++ cross-compiler found." >&2
  echo "On macOS: brew tap messense/macos-cross-toolchains" >&2
  echo "            brew install aarch64-unknown-linux-gnu" >&2
  exit 1
fi

if [[ -n "${AARCH64_STRIP:-}" ]]; then
  strip_bin="$AARCH64_STRIP"
else
  strip_bin="${cxx_bin%g++}strip"
fi

rm -rf "$repo_dir/build/native/s3g-thru"
mkdir -p "$module_dir"
"$cxx_bin" \
  -std=c++17 -O3 -Wall -Wextra -Werror -fPIC -fvisibility=hidden \
  -static-libstdc++ -static-libgcc \
  -I"$repo_dir/include" \
  -shared -Wl,-soname,dsp.so -Wl,--no-undefined -Wl,--exclude-libs,ALL \
  "$repo_dir/src/dsp/thru_module.cpp" \
  -lm -pthread \
  -o "$module_dir/dsp.so"

if command -v "$strip_bin" >/dev/null 2>&1; then
  "$strip_bin" --strip-unneeded "$module_dir/dsp.so"
fi

readelf_bin="${cxx_bin%g++}readelf"
if command -v "$readelf_bin" >/dev/null 2>&1; then
  if "$readelf_bin" -d "$module_dir/dsp.so" | grep -Eq 'libstdc\+\+|libgcc_s'; then
    echo "Device build unexpectedly depends on a C++ runtime shared library." >&2
    exit 1
  fi
fi

nm_bin="${cxx_bin%g++}nm"
if command -v "$nm_bin" >/dev/null 2>&1; then
  exported_symbols="$($nm_bin -D --defined-only "$module_dir/dsp.so" | awk '{print $3}')"
  if [[ "$exported_symbols" != "move_plugin_init_v2" ]]; then
    echo "Device build exports symbols other than move_plugin_init_v2:" >&2
    printf '%s\n' "$exported_symbols" >&2
    exit 1
  fi
fi

"$repo_dir/scripts/verify-move-abi.sh" "$module_dir/dsp.so"

cp "$repo_dir/src/module.json" "$module_dir/module.json"
cp "$repo_dir/src/ui.js" "$module_dir/ui.js"
cp "$repo_dir/src/help.json" "$module_dir/help.json"
cp "$repo_dir/LICENSE" "$module_dir/LICENSE"
cp "$repo_dir/THIRD_PARTY_NOTICES.md" "$module_dir/THIRD_PARTY_NOTICES.md"

file "$module_dir/dsp.so"
if ! file "$module_dir/dsp.so" | grep -Eq 'ELF 64-bit.*ARM aarch64'; then
  echo "Device build is not an aarch64 Linux ELF shared object." >&2
  exit 1
fi

echo "Built $module_dir"

