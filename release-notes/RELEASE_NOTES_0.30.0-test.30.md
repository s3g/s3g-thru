# s3g Thru 0.30.0-test.30

Local Monolit crossfade-layout refinement. The public GitHub installer remains
at 0.29.1.

## Crossfade and curve placement

- Places X-CURVE on MIX / SEND fader 4 and X-FADE on fader 8.
- Adds the fixed shared X-CURVE receiver at channel 13 CC 25. Values 0–63
  select Linear and 64–127 select Equal Power.
- Retains X-FADE at channel 13 CC 24, where 0 selects A, 64 is centered and
  127 selects B.
- Keeps per-part X-PN on the focused ROTARY bank and preserves local MOTION
  button behavior.

Install `dist/s3g-thru-module.tar.gz` for hardware testing. Native, sanitizer,
UI, help, MIDI, controller and package checks are required before promotion;
hardware acceptance is not implied by the local build.
