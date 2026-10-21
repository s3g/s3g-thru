#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
binary="${1:-$repo_dir/build/native/s3g-thru/dsp.so}"

if [[ ! -f "$binary" ]]; then
  echo "Move ABI check: binary not found: $binary" >&2
  exit 1
fi

if [[ -n "${AARCH64_READELF:-}" ]]; then
  readelf_bin="$AARCH64_READELF"
elif [[ -n "${AARCH64_CXX:-}" ]]; then
  readelf_bin="${AARCH64_CXX%g++}readelf"
elif command -v aarch64-unknown-linux-gnu-readelf >/dev/null 2>&1; then
  readelf_bin="aarch64-unknown-linux-gnu-readelf"
elif command -v aarch64-none-linux-gnu-readelf >/dev/null 2>&1; then
  readelf_bin="aarch64-none-linux-gnu-readelf"
elif command -v aarch64-linux-gnu-readelf >/dev/null 2>&1; then
  readelf_bin="aarch64-linux-gnu-readelf"
elif command -v readelf >/dev/null 2>&1; then
  readelf_bin="readelf"
else
  echo "Move ABI check: no readelf executable found" >&2
  exit 1
fi

if ! command -v "$readelf_bin" >/dev/null 2>&1; then
  echo "Move ABI check: readelf executable not found: $readelf_bin" >&2
  exit 1
fi

versions_text="$("$readelf_bin" --version-info "$binary" | \
  sed -n 's/.*Name: GLIBC_\([0-9][0-9.]*\).*/\1/p' | sort -Vu)"

if [[ -z "$versions_text" ]]; then
  echo "Move ABI check: no GLIBC version requirements found in $binary" >&2
  exit 1
fi

highest="0.0"
while IFS= read -r version; do
  [[ -n "$version" ]] || continue
  major="${version%%.*}"
  minor="${version#*.}"
  minor="${minor%%.*}"
  if (( major > 2 || (major == 2 && minor > 34) )); then
    echo "Move ABI check: $binary requires GLIBC_$version; Move provides GLIBC_2.34" >&2
    exit 1
  fi
  highest="$version"
done <<< "$versions_text"

echo "Move ABI compatibility: PASS (highest required GLIBC_$highest; target GLIBC_2.34)"
