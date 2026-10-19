# S3G Thru 0.19.0

This release makes Overdub a layered, reversible part of the free-time
performance recorder.

- Replace creates take 1, the protected base performance. Every completed
  non-empty Overdub pass adds one take containing both its notes and its
  pressure/parameter gestures.
- Undo Take removes the newest overdub without changing the base, loop length,
  sample assignments, or earlier takes. It can be repeated until only the base
  remains.
- The new Overdub Takes parameter page shows the take count and provides an
  explicit Undo Take action. Delete+Record invokes the same action from the
  waveform or any parameter page.
- Take membership follows clip copy and survives autosave, normal exit, recall,
  checksum recovery, and backup-generation recovery.
- The binary clip bank advances to format 2. Version-1 banks from S3G Thru
  0.18 are imported as a protected base take and upgraded on the next save.
- Gesture take tags reuse unused bits in the existing point descriptor, so the
  feature does not enlarge the fixed 64-clip gesture bank or add render-time
  allocation.

The state schema remains version 6 and the binary magic remains `S3GTHR3`.
