# s3g Thru 0.30.0-test.4

Local Tape-alignment test build. The published installer remains at 0.29.1.

- Corrects Tape’s static dry/wet timing mismatch, removing the deep periodic
  notches that could make partial MIX settings sound hollow or phasey.
- Keeps a fixed two-sample reference for both paths, including MIX zero;
  stable Tape no longer adds the former 7 ms wet-path lookback. The reference
  adds about 0.045 ms at Move’s 44.1 kHz rate.
- Removes the FMT/AGE-dependent right-channel delay. Tape coloration and
  stereo-width character remain; transport motion is shared by both channels.
- MECH, CRNK and INRT retain deliberate timing movement, including stop and
  return gestures. That movement can still chorus against dry audio at
  partial MIX settings. Hiss remains an independent stereo noise source.
- Keeps all eight Tape controls and their MIDI mappings.

The [Tape manual](../docs/effects.html#tape) describes the updated behavior.
See the [draft 0.30.0 release notes](RELEASE_NOTES_0.30.0.md) for the combined
MIDI/E16 feature set. Compare MIX settings with MECH/CRNK zero and INRT off,
then check live changes and intentional transport effects on Move.
