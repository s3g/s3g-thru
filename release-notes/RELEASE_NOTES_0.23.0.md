# s3g Thru 0.23.0

## Thru Recordings archive

- Gives every completed input recording, resample, and committed sample edit a
  unique stereo WAV instead of reusing a clip/lane filename.
- Keeps older WAVs when a lane is rerecorded, edited, cleared, or its clip is
  reset. Existing fixed-name captures remain readable and require no migration.
- Flushes a pending capture before another audio recording begins, preventing a
  rapid rerecord from replacing audio that has not reached disk.
- Persists a monotonic take number and also checks the filesystem for collisions,
  so recovery from an older metadata backup cannot reuse an existing filename.

## Recall and browsing

- Makes audio-lane recording participate in the existing one-level clip undo.
  Shift+Back can now restore the preceding source because its WAV still exists.
- Adds a source chooser to Shift+Track: Move Samples opens the User Library and
  Thru Recordings opens the module capture directory, newest take first.
- Allows archived recordings and edits to be auditioned and loaded into any
  lane with the existing background WAV decoder.

## Compatibility

- Keeps state schema version 6, clip-bank format 4, parameter IDs, MIDI mapping,
  engine IDs, and previously stored source paths compatible.
- Adds read-only `captures_directory` and `lane_source_path` metadata for the
  browser and diagnostics.
- Capture storage is intentionally append-only during normal use. Set export
  includes the complete archive; manual removal should happen only after backup.

## Verification

Native DSP/ABI tests cover unique take allocation, preservation of earlier WAVs,
and rerecord undo/reload. UI tests cover both browser roots and newest-first
archive loading. ASan/UBSan, export, documentation, AArch64 build, and package
checks remain release gates.
