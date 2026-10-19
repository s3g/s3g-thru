#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 MODULE_DATA_DIR OUTPUT.tar.gz" >&2
  exit 2
fi

data_dir="$1"
output="$2"
if [[ ! -d "$data_dir" ]]; then
  echo "module data directory not found: $data_dir" >&2
  exit 1
fi
if [[ ! -f "$data_dir/thru-state.json" || ! -f "$data_dir/thru-clips.bin" ]]; then
  echo "module data directory lacks thru-state.json or thru-clips.bin" >&2
  exit 1
fi
case "$output" in
  /*) ;;
  *) output="$PWD/$output" ;;
esac
if [[ -e "$output" ]]; then
  echo "refusing to overwrite: $output" >&2
  exit 1
fi
output_dir="$(dirname "$output")"
if [[ ! -d "$output_dir" ]]; then
  echo "output directory not found: $output_dir" >&2
  exit 1
fi

members=(thru-state.json thru-clips.bin)
for optional in \
  thru-state.json.bak thru-clips.bin.bak \
  thru-lane-1.wav thru-lane-2.wav thru-lane-3.wav thru-lane-4.wav \
  captures; do
  if [[ -e "$data_dir/$optional" ]]; then
    members+=("$optional")
  fi
done

temporary="$output.tmp"
trap 'rm -f "$temporary"' EXIT
tar --exclude='*.tmp' -czf "$temporary" -C "$data_dir" "${members[@]}"
mv "$temporary" "$output"
trap - EXIT
echo "Exported s3g Thru set: $output"
