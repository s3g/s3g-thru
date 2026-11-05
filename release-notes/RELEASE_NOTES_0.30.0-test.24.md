# s3g Thru 0.30.0-test.24

Local hardware-test build. The public GitHub installer remains at 0.29.1.

## Official OXI E16 Remote integration

- Makes the script-free **THRU RM** Remote interface the sole official E16
  controller integration for the release in progress.
- Keeps the readable twelve-page Engine, Stack, ENG 1–4, Sends, Color, Rotary,
  Delay, Reverb and Tape layout with A/B and Quad C/D control.
- Removes the experimental native-scene controller, its generated artifacts,
  protocol endpoints, tests and public documentation from the repository.
- Retains high-resolution values, compact names and formatted units, changed-
  cell redraws, heading priority, mode colors, held-encoder fine adjustment,
  bounded display traffic and reconnect recovery.
- Keeps Move's current page and part focus unchanged while the E16 edits either
  instrument.

## Test setup

Install `dist/s3g-thru-module.tar.gz`, set Schwung Surface to **E16**, transfer
the script-free **THRU RM** scene from `dist/s3g-thru-e16-remote.zip`, select it,
then open takeover. Use current E16 firmware, USB1, USB Multi Port Off and USB
MIDI Thru Off.

This remains a local test build pending final E16 hardware confirmation.
