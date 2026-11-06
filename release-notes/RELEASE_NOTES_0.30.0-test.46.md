# s3g Thru 0.30.0-test.46

Local test build; the published GitHub installer remains 0.29.1.

- **E+M** replaces the combined native NRPN controller mode with explicit
  relative CC turns for E16 and the existing absolute CC map for Monolit.
  Install the matching **THRU EM** scene. Enum choices advance directly;
  encoder presses restore defaults. Automatic ring/value feedback is disabled
  in this native example, so controller edits do not depend on ring alignment.
- **VOICE** on Pads & Scale selects Poly, Mono retrigger, or Legato. Legato
  preserves playback position, stack phase, envelope and the first engine
  assignment while keys overlap, with last-note priority.
- **Bank mode** gives the left pads a four-lane instrument and the right pads
  sixteen independent stereo sample slots, with four voices on each side.
  Hold an empty right pad to capture A; release to finish silently. Hold a
  filled pad to loop, Shift + pad to latch, Delete + pad to clear, Undo to restore.
- Captures are archived as immutable WAVs and recalled per clip. Bank playback
  notes can be recorded in performance clips. Maximum twelve seconds per slot;
  finish the current capture/save before recording another slot.
- Adds Bank waveform/slot displays and Start, End, Rate, Reverse, Level, Attack,
  Release and Loop Fade controls.
- Shows saving/saved in a fixed, small footer overlay without moving the page.

The module and E16 scene require hardware testing together before release.
