# s3g Thru 0.28.0 — local testing build 17

- Shared Dual adds two independently configured instruments reading the same
  four stereo source lanes. A plays from the left pad group, B from the right,
  with four voices each. Each keeps its own engine, parameters, keyboard,
  envelope, stack movement, lane gains, and private effects.
- Choose Capture + Menu → Instrument Mode → Shared Dual, then click the main
  encoder. Single and Split remain available. Copy returns to Single and back
  to the last chosen dual layout; Dual Setup also provides the three-way MODE
  control. Stop audio and performance recording before changing mode.
- Shared B uses muted plum idle pads and brighter pink scale roots, distinct
  from Split B’s teal. Active voice, latch, cue, and Roll colors retain their
  usual meanings.
- In Shared Dual, Track selects a recording/loading/editing lane without
  changing A/B focus. Capture changes focus without changing that destination.
  Both instruments use the same audio: replacing, clearing, editing, or recording
  a lane affects its source for both. Mix / Rec offers four lane gains per part.
- Shared waveforms use one four-row zoom containing both instruments’ sounding
  windows. The inverted A/B letter marks editing focus. Top cursor caps identify
  A, bottom caps B; hollow markers show each reader’s actual stack position.
- Blend, part levels, mutes, sends, Delay/Reverb returns, and Tape work as in
  Split. Part-mode browser preview follows the focused instrument in Shared
  Dual, with a smoothed level transition when focus changes.
- Clips and saved sets recall Shared Dual, its independent programs, and both
  sides’ recorded notes and automation. Existing Single/Split sets load normally.
  Export a set backup before testing: saves written by this build require this
  build or newer when reopening the bank.
- Corrected four-voice-per-side allocation after playing in Single and returning
  to a dual mode. Updated manual, screenshots, and mode instructions.

Includes testing builds 14–16. Local package for testing on Move; hardware sound,
CPU performance, and physical LED colors still need device testing.
