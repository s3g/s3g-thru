#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
module_root="$repo_dir/build/native"
dist_dir="$repo_dir/dist"
archive="$dist_dir/s3g-thru-module.tar.gz"

"$repo_dir/scripts/validate-native.sh"
"$repo_dir/scripts/build-native.sh"
mkdir -p "$dist_dir"
COPYFILE_DISABLE=1 tar -C "$module_root" -czf "$archive" s3g-thru
"$repo_dir/scripts/verify-package.sh" "$archive"

echo "Packaged $archive"
if command -v shasum >/dev/null 2>&1; then
  shasum -a 256 "$archive"
else
  sha256sum "$archive"
fi

