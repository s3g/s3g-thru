# s3g Thru 0.30.0-test.27

Local Light Reft Monolit Split / Shared Dual controller test build. The public
GitHub installer remains at 0.29.1.

## Monolit A/B performance layout

- Replaces the earlier one-part-per-bank example with eight A/B banks. Faders
  1–4 always address Part A on channel 9; faders 5–8 address Part B on channel
  10.
- Adds LIVE; both four-control halves of ENG 1 and ENG 2; MIX / SEND; COLOR;
  and ROTARY banks. Algorithm selection and engine pages 3–4 remain on Move.
- Adds part-specific CC 72–79 for Stereo Cross Pan and all seven Rotary
  parameters. The same CC numbers on channel 13 continue to address shared
  Performance controls.
- Keeps all paired Monolit buttons as local MOTION controls. The generated
  preset enables MOTION mode for every fader and does not assign Thru commands
  to the buttons.
- Updates the reproducible preset generator, receiver CSV, manual and setup
  instructions. `controllers/lightreft-monolit/presets/s3g-thru` is ready for
  import through the official Monolit Web-Editor.

Install `dist/s3g-thru-module.tar.gz` for hardware testing. Native, sanitizer,
UI, help, MIDI, controller and package checks are required before promotion;
hardware acceptance is not implied by the local build.
