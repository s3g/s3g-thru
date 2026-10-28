# s3g Thru 0.28.0 — local testing build 11

- Faster A/B editing changes. Capture now refreshes all controls with one
  consistent snapshot instead of hundreds of separate parameter reads.
- The screen updates immediately after switching, with the selected part’s
  parameter values, octave, stack hold, mute status and playhead information.
- If a control refresh is interrupted, knobs wait for a valid refresh so a stale
  value from the other part cannot be applied accidentally.
- Startup and clip recall also use the consolidated control refresh.

Includes the Split mute, mixer and sample-loading repairs from testing build 10.
Existing sounds, recorded performances, effects, MIDI assignments and saved-set
formats are unchanged. This is a local package for further testing on Move.
