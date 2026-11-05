# s3g Thru 0.30.0-test.23

Local hardware-test build. The public GitHub installer remains at 0.29.1.

## Readable OXI E16 Remote

- Restores the script-free **THRU RM** Remote interface as the recommended E16
  setup. Small parameter names appear above formatted values and units.
- Carries forward the current twelve-page Engine, Stack, ENG 1–4, Sends, Color,
  Rotary, Delay, Reverb and Tape map. A/C uses the top rows; B/D the bottom.
- Keeps all twelve page positions stable when engines change. Unused cells show
  reserved squares instead of changing the page sequence.
- Redraws only changed pixel rectangles and rings. Page headings are confirmed
  first; choice edits and common engine controls avoid whole-screen resets.
- Retains Quad A/B ↔ C/D selection, held-encoder fine control, Move-focus
  preservation, mode colors, bounded display traffic and reconnect recovery.
- Establishes the script-free THRU RM scene as the controller example used for
  local release testing.

## Test setup

Install `dist/s3g-thru-module.tar.gz`, set Schwung Surface to **E16**, transfer
the script-free **THRU RM** scene from `dist/s3g-thru-e16-remote.zip`, select it,
then open takeover. Use current E16 firmware, USB1, USB Multi Port Off and USB
MIDI Thru Off.

This remains a local test build pending E16 hardware confirmation.
