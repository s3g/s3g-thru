# s3g Thru 0.30.0-test.29

Local Monolit A/B crossfade build. The public GitHub installer remains at
0.29.1.

## Direct A/B crossfade

- Adds a fixed shared MNLT mapping at channel 13 CC 24 for Split and Shared
  Dual. CC 0 selects A, 64 is the center blend and 127 selects B.
- Places mirrored X-FADE controls at the fourth fader of both halves of the
  Monolit MIX / SEND bank. They address the same value; use or motion-record
  only one copy at a time.
- Moves per-part X-PN into the ROTARY example bank alongside RATE, DPTH and MIX.
  Rotary SPRD remains available in the complete direct map but is omitted from
  the focused eight-bank preset.
- Keeps the local MOTION button behavior and the test.28 enum Pickup repair.

Install `dist/s3g-thru-module.tar.gz` for hardware testing. Native, sanitizer,
UI, help, MIDI, controller and package checks are required before promotion;
hardware acceptance is not implied by the local build.
