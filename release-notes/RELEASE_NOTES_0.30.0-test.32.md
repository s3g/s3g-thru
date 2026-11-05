# s3g Thru 0.30.0-test.32

Local dual-instrument priority-duck test. The public GitHub installer remains
at 0.29.1.

## Priority Duck

- Adds stereo-linked ducking for Split and Shared Dual. PRIO selects A over B
  or B over A and sets up to 24 dB of maximum reduction; its center detent is
  Off.
- Adds threshold, attack, hold, release, detector high-pass, soft-knee and
  Dry / Dry + Sends routing controls. Existing Delay and Reverb tails are not
  reduced.
- Keeps the detector after each part's private processing and level but before
  Blend, so crossfading does not change how the dominant part is detected.
- Bypasses Priority Duck in Quad and Single. A shared, stereo-linked gain value
  preserves the subordinate part's stereo image.

## Control and recall

- Places PRIO and THR on dual-mode FX Sends. Main-click FX Pages opens the
  remaining Priority Duck controls. Delay and Reverb MIX remain on their own
  return pages.
- Adds eight fixed shared NRPN addresses and channel 13 direct CC 26–33. The
  fixed NRPN registry now contains 342 addresses.
- Updates the OXI E16 SENDS page to show PRIO and THR while preserving the
  existing A/B sends, Curve and Blend.
- Stores Priority settings with clips and migrates older clips with Priority
  safely disabled.

Install `dist/s3g-thru-module.tar.gz` for Move testing. Native, sanitizer, UI,
help, MIDI, controller, documentation and package checks are required before
promotion; hardware acceptance is not implied by the local build.
