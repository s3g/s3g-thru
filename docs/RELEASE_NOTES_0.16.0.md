# S3G Thru 0.16.0

This release replaces the takeover's Step-grid composition workflow with a
free-time performance-data recorder and looper inspired by the continuous
control recorders in the S3G Daisy instruments.

## Performance looper

- Record polyphonic pad performances with exact note onset, duration, velocity,
  and physical source-pad identity.
- Record per-pad pressure and lockable engine-parameter gestures as continuous
  loop paths with interpolation across the seam.
- Choose Replace for a new variable-duration loop or Overdub to merge a pass
  without changing the existing duration.
- Play the complete note/gesture loop at continuously variable 0.25x–4x speed.
- Store 64 clips in four A–D banks, launch them from the 16 Step buttons, and
  retain the existing 16-slot chain with changes at loop boundaries.
- Use Record for performance data and Shift+Record for stereo lane capture.

## Display and control changes

- The waveform page identifies the active A01–D16 clip and draws its loop
  position below the waveforms.
- Step LEDs show stored, active, playing, recording, and queued clip states.
- Play is green while the performance loop runs; Record is red for data capture
  and orange for audio-lane capture.
- The new Performance Loop page exposes Rate, Replace/Overdub, pressure/root,
  and primary chain controls. Mix / Rec now exposes Input/Resample and Monitor.
- Left/Right selects clip banks; Step keys are clip launchers in every main
  performance view. Delete+Step clears only performance data; Shift+Step copies.

## Persistence and compatibility

- Pattern storage advances to version 10 and persists clip notes, paths,
  duration, and rate.
- Version 1–9 step patterns remain readable and are deterministically migrated
  to timed performance clips.
- Saved stereo captures, pattern-local sample ownership, cues, chains, engine
  state, stable parameter IDs, and the existing external MIDI map are retained.
- External MIDI CC 114–117 control loop rate, record mode, record toggle, and
  clear respectively.

Desktop ABI, DSP, ASan/UBSan, UI, cross-build, and package checks are automated.
Move hardware remains the authority for realtime feel, CPU margin, and physical
control acceptance.
