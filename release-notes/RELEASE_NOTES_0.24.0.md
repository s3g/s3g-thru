# s3g Thru 0.24.0

## Recorded stack motion

- Records Stack Hold and Run changes as part of each performance clip rather
  than leaving the Loop gesture outside the data recorder.
- Captures the exact stack phase of every active voice when Hold begins,
  including positions between lanes, so each voice recalls its own crossfade.
- Seeds held notes and their frozen stack positions when recording begins while
  Loop is already held. This works for Replace and for an active Overdub pass.
- Keeps pressure and stack position as independent per-pad curves: pressure can
  still be performed without displacing a recorded hold point.

## Playback and feedback

- Replays recorded Hold/Run transitions as stepped data while continuous knob
  and pressure curves retain their existing interpolation.
- Keeps the physical Loop button as a live override. Releasing a live hold
  returns playback to the clip's recorded Hold/Run state.
- Shows `RECORDED STACK HOLD` in the footer and lights Loop yellow for a stored
  hold. A physical live hold remains bright white; Glitch Buffer Lock remains
  orange.

## Compatibility and verification

- Keeps state schema version 6, clip-bank format 4, engine and parameter IDs,
  and existing clips compatible. Clips without stack data continue in Run.
- Adds read-only `clip_stack_hold` metadata for the takeover display.
- Native DSP tests cover recording while already held, exact per-voice recall,
  persistence, and stable held playback. UI tests cover the recorded-hold
  indication; sanitizer, documentation, AArch64 build, and package checks remain
  release gates.
