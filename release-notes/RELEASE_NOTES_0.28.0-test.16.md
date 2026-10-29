# s3g Thru 0.28.0 — local testing build 16

- The curved reverse-arrow Undo key now performs the one-level safety undo for
  the latest destructive clip operation, source replacement, lane clear, or
  sample edit. It lights when an undo is available and goes dark after use.
- Undo is blocked during audio or performance recording. If a recording archive
  or queued clip is still being prepared, the undo point is retained and a short
  message explains what to wait for.
- Back handles navigation, cancelling pending FX-order edits, and exiting from
  the waveform view. Shift + Back no longer triggers undo.
- Undo refreshes the restored parameter readouts. The
  on-device shortcut reminder and manual identify the curved-arrow button.
- Overdub-take undo remains Delete + Record after finishing recording. The
  safety undo remains one level; parameter turns do not create undo history.

Includes the features and icon illumination from testing builds 14–15. UI-only
update; audio processing, external MIDI assignments, and saved-set formats are unchanged.
Local package for testing on Move.
