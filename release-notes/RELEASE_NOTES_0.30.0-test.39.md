# s3g Thru 0.30.0-test.39

Local performance-audio and E16 startup-state repair. The public GitHub
installer remains at 0.29.1.

- Prevents the native E16 scene's uninitialized raw-zero values from replacing
  Thru's restored ENGINE controls when the scene is selected or reconnected.
- Reserves raw zero for startup synchronization and begins real E16 encoder
  turns at raw one, retaining the full practical 14-bit control range.
- Repairs the exact test.38 zero-sweep signature—minimum Position, Field,
  Speed, Scan and Join, with optional zero Level—back to playable defaults on
  first load. Individual intentional minimum settings are left unchanged.
- Retains adaptive per-engine pages, encoder-press defaults, immediate ring
  feedback, Monolit MIDI Thru and unchanged Move editing focus.
- Adds controller, persistence-migration and native DSP regressions for the
  silent-performance failure.
