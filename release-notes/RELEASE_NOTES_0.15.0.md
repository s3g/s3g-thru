# s3g Thru 0.15.0 — Regions and Performance

This release brings the direct-sample, transient-slice, Kit-variation, and
compact Decks-style performance ideas into one compatible Thru update.

- Adds Sample and Slices after the four existing algorithm IDs, preserving all
  saved Motion, Lanes, Grains, and Cutups selections.
- Derives a bounded 64-region transient map for every loaded, recorded, edited,
  restored, and prepared lane outside the audio callback. Cutups can switch
  between equal and transient regions; Slices uses the same map.
- Adds forward, reverse, loop, and ping-pong direct playback. Slice pad identity
  is retained in sequencer notes, so patterns replay the authored region.
- Adds Manual, Cycle, Random, No Repeat, and Velocity lane variation. Velocity
  only selects a discrete lane in Velocity mode and never controls output gain;
  pressure remains the continuous stack gesture.
- Adds automatic browser audition using the existing bounded decode stage. A
  click promotes the already decoded preview into the selected pattern lane.
- Adds Keys, Slices, Cues, and Rolls pad modes. Eight cues are pattern-local and
  persisted in the new v9 pattern file. Its third lock-mask word also preserves
  Cut Region Source, Sample Mode, Slice Source, and Slice Count locks. Rolls are
  held polyphonic Cutups voices with pad-selected tempo divisions.
- Draws transient boundary ticks on the selected waveform and exposes the new
  controls on contextual, padded parameter pages.
- Extends the fixed external MIDI map through CC 113. Existing CC and parameter
  identifiers are unchanged.

Desktop ABI, DSP, UI, migration, sanitizer, and package tests pass. Move remains
the authority for pad feel, browser latency, transient usefulness, and the
eight-voice realtime CPU margin.
