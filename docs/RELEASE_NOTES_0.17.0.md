# s3g Thru 0.17.0

This pre-release makes performance clips the only composition model.

- Removes the step sequencer, step locks, gate/probability/ratchet/microtiming
  controls, their `seq_*` module API, and the corresponding MIDI CC aliases.
- Renames performance transport to `clip_playing` and exposes timing as
  `tempo_bpm` and `tempo_swing`.
- Keeps the fixed external engine MIDI map; CC 54–58 now control tempo,
  performance playback, Undo, and chain-slot clearing.
- Adds Clear Slot directly to the Chain page.
- Removes old pattern-file migration. The new version-1 performance-bank format
  stores only current clips, gestures, lane sources, cues, engine state, and
  chains. Earlier pre-release `thru-pattern.bin` files are intentionally
  rejected.
- Simplifies the takeover UI and saved JSON state by removing unused step-edit
  fields and compatibility data.

Back up any captures you want to keep, then remove old `thru-pattern.bin` and
`thru-state.json` files before first launch.
