# S3G Thru 0.18.0

This release turns the free-time performance recorder into a clearer clip and
composition system while keeping Overdub and the existing external MIDI map
unchanged pending further instrument testing.

## Performance and timing

- Waveform-cache work no longer runs in the audio render path.
- Grains Quality selects 4, 8, or 16 grain slots per playback voice so density
  can be balanced against Move's realtime margin.
- Every recorded clip stores capture BPM and exact beat length. Free playback
  retains wall-clock timing; Beat Sync scales the original microtiming to the
  current BPM.
- Play launch quantization can be Off, Beat, or Bar. A pending launch is shown
  in blue-violet and does not report as playing before its boundary.

## Capture and composition

- The display reports note/gesture usage and a visible `CLIP FULL` condition.
  Gesture capture progressively thins redundant points and preserves the latest
  endpoint when its fixed buffer is full.
- Shift+Play now cycles Main -> Clips -> Chain. The 16-slot Chain view shows
  empty, selected, current, and queued entries and supports insert, compacting
  delete, reorder, clip selection, and repeat editing.
- Public controls and current UI text consistently use Clip terminology; the
  unreleased Pattern-era parameter names have been removed.

## Samples and persistence

- A new clip snapshots the current four lane sources on first use. Each clip
  then owns explicit File, Capture, or Empty assignments; later changes never
  resolve dynamically through an earlier clip.
- Clip metadata uses `thru-clips.bin` with an FNV-1a checksum. JSON and binary
  metadata carry save-generation values, are replaced through temporary files,
  and keep one `.bak` generation. Recovery selects the newest matching pair
  across current and backup files, including when only one current file is
  invalid or belongs to an interrupted save.
- `scripts/export-set.sh` creates a non-overwriting archive of metadata,
  backups, saved lane WAVs, and local captures. External User Library samples
  remain references and can be relinked from the on-device browser.

The state schema is version 6 and the binary bank magic is `S3GTHR3`. Earlier
pre-release metadata is intentionally not imported.
