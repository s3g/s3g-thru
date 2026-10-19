# s3g Thru 0.19.0

This release makes Overdub a layered, reversible part of the free-time
performance recorder.

- Move's Loop key now toggles a live Stack Scan hold. Every polyphonic voice
  freezes at its own exact lane crossfade, ignores pressure drift while held,
  and resumes from that audible point. The former Stack Position label is now
  Stack Start to clarify its role as Manual position or moving-path phase seed.
- Parameter pages now give every control one location. Engine holds the common
  source controls, Stack Path is shared by Motion/Lanes/Grains, Direct Play is
  shared by Sample/Slices, and engine-specific pages contain only unique
  controls. Unassigned encoder positions show a small hollow reserved box and
  remain inert.
- A twelve-page illustrated manual now covers setup, the screen model, audio and
  library sources, every playback engine, performance clips and chains, Move
  shortcuts, LEDs, and external MIDI. Screen figures are generated from the
  actual takeover UI and checked for drift with the rest of the build. Every
  playback engine now has an individual parameter-by-parameter guide, every
  engine-specific parameter page has its own figure, and the waveform example
  uses four distinct sparse lane shapes so polyphonic playheads remain legible.
  The waveform retains the hardware display convention: a short Position tick
  at the top and a short Field tick at the bottom.
- The manual is ready to publish from `main/docs` with only HTML and web assets
  in that directory. Release history now lives in `release-notes/`, while the
  hardware acceptance guide lives at the repository root.

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
- The binary clip bank advances to format 2. Version-1 banks from s3g Thru
  0.18 are imported as a protected base take and upgraded on the next save.
- Gesture take tags reuse unused bits in the existing point descriptor, so the
  feature does not enlarge the fixed 64-clip gesture bank or add render-time
  allocation.

The state schema remains version 6 and the binary magic remains `S3GTHR3`.
