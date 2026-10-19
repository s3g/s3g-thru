# s3g Thru 0.14.0 — Engine Fidelity

This release deepens the four hardware-bounded playback engines and corrects
their note semantics to match the s3g Sample family more closely.

## Playback

- Grains notes now retune source advance; Grain Pitch remains an independent
  timbral control.
- Cutups notes now retune each cut reader without changing its free/tempo
  clock.
- Cutups selects a discrete loaded lane and one of 1–64 equal source regions.
  It adds musical clock divisions, free rate, swing from the pattern clock,
  timing variation, gate, join, repeats, reverse/pitch/level variation, seven
  lane orders, and four per-voice poly paths.
- Grains expands to 16 bounded grain slots per playback voice and adds position
  spray/bias, pitch spray, reverse/size/level variation, regular/scattered
  timing, envelope skew, variable slice regions, scan/per-grain advance, and
  Ordinary, Stutter, Shrink, and Doublets processes.
- Motion adds Drunk, travel and jitter controls, packet duty, and independent
  Motor rate, symmetry, and shape.
- Lanes adds Random, Crossfade/Jump, path shape, offset, skew, curve, and
  transition slew.

## Control and recall

- Engine-specific parameters are split across two Motion pages, two Lanes
  pages, three Grains pages, and two Cutups pages. Unrelated engine pages remain
  hidden.
- All 64 engine/global controls can be locked from a held Step and automated.
  `seq_locks_hi` exposes lock bits 32–63 to the takeover UI.
- External MIDI CC 69–106 covers the new fidelity controls while the existing
  CC 20–68 map remains stable.
- Pattern persistence is version 8. Versions 1–7 migrate with their existing
  26 parameters intact and receive safe defaults for new parameters.
- JSON state is version 3 and includes every new control.

## Verification

Native host, ASan/UBSan, and takeover UI regressions cover all four grain
processes, Drunk/Motor Motion, Random/Jump Lanes, polyphonic lane/region
Cutups, all new MIDI endpoints, high-word locks, and v8 recall.

## Deliberate hardware limits

Transient-derived Cutups regions, the desktop Grains Sorter process, and the
desktop Motion segment-event router remain future work. Cutups uses equal
regions, and Motion Bounce remains a bounded articulation. Dense eight-voice
Grains (up to 128 active grain slots) still requires acceptance on Move.
