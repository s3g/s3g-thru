# s3g Thru 0.30.0-test.5

Local Engine List test build. The published installer remains at 0.29.1.
Includes the working high-resolution MIDI/E16 and Tape alignment changes.

- Adds per-part Engine Lists for combining algorithms within a chord.
- Pads & Scale: ENGS ONE/LIST and ORDR UP/DOWN/RAND. Repeated algorithms
  weight RAND among free entries. Voice Engines assigns eight entries in
  Single, four per part in Split/Shared Dual, or two per part in Quad.
- Turn/click the main encoder on Voice Engines to select an entry and edit
  its engine pages. Back returns. Held notes keep their original engine;
  entries sharing an engine also share its controls within their part.
- Records resolved note assignments for repeatable playback and captures
  already-held notes. Saves lists with clips and includes them in copies.
- Preserves the eight-voice limit, voice-stealing fades and shared effects.
  Waveform cursors follow each voice's engine. Clip preparation includes
  every required Wavesets or spectral analysis, not only the edited engine.
- Adds eleven NRPN addresses (135–145), preserving existing IDs.

The [manual](../docs/engines.html#engine-list) explains setup and behavior.
Back up your set before testing: this build reads older banks and writes
format 24 for lists and note assignments; older firmware cannot read that
new bank. Keep the backup when returning to an earlier version.

Hardware validation remains necessary for mixed-engine CPU load, audio,
list editing, clip save/recall and E16 feedback. Nothing has been published.
