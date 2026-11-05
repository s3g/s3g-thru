# s3g Thru 0.30.0-test.28

Local MIDI Pickup repair build. The public GitHub installer remains at 0.29.1.

## Reliable engine selectors

- Fixes Pickup for bounded integer and enum parameters used by MNLT and Generic
  CC. Pickup now compares the selected choice rather than requiring the fader
  to touch one canonical CC position.
- Keeps Pickup engaged while moving through later choices. Previously a mode
  could change once and then immediately disengage because its physical fader
  position differed from the canonical position of that enum value.
- Adds direct regression coverage for Texture MODE across Spray, Grouped,
  Motif and Ornament, including re-pickup after a value changes elsewhere.
- Retains the test.27 Split / Shared Dual Monolit preset and its local MOTION
  buttons unchanged.

Install `dist/s3g-thru-module.tar.gz` for hardware testing. Native, sanitizer,
UI, help, MIDI, controller and package checks are required before promotion;
hardware acceptance is not implied by the local build.
