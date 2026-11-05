# s3g Thru 0.30.0-test.6

Local voice-assignment and expression test build. The published installer remains
at 0.29.1. Includes the working Engine List, MIDI/E16 and Tape alignment changes.

- Adds Cycle Up, Cycle Down and Shuffle orders, plus scale Degree and keyboard
  Register assignment. Single-note phrases can travel through the engine list.
- Adds Voice Setup for each entry's octave, source launch offset, stack offset
  and stereo pan. Existing notes retain their captured entry variations.
- Adds pressure Brake, Character and Both, with pressure depth per part. Brake
  catches one note's stack scan and resumes from that point. Character applies
  engine-specific expression or a smooth per-voice tone treatment. Quad offers
  Off/Character. Strike velocity remains separate from playback volume.
- Records entry variations and initial held pressure, and adds recording of
  external polyphonic aftertouch by note and part. Pressure mode/depth are clip
  setup; pressure gestures are performance data.
- Preserves eight source voices and the existing voice-stealing fades. Source
  offsets also work with full-width regions; pan keeps stereo at centre and
  folds both channels toward the chosen side without adding gain.
- Adds NRPN 146–151. Existing IDs remain compatible. ORDR and PRES have
  expanded ranges: update custom controllers' encoded values
  for their selected options. Configure the new playing behaviors on Move.

See the [Engine List and Voice Setup manual](../docs/engines.html#voice-setup).
Back up your set before testing. This build reads older banks and writes format
25; earlier firmware cannot read the new saved bank. Hardware audio, pressure
feel, controller feedback and worst-case CPU testing remain necessary.
