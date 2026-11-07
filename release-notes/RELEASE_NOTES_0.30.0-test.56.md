# s3g Thru 0.30.0-test.56

Local testing build; not a published release.

- External MIDI notes in the Slices engine now select slices without changing
  playback pitch. Root triggers slice 1; each semitone above Root selects the
  next slice. The selection wraps after the last region; notes below Root
  select slice 1.
- All slice keys use the engine’s SPEED. Pitch bend and an Engine List entry’s
  octave variation remain available as deliberate pitch controls.
- Works with equal and transient regions, each part’s Root and recorded clips.
  The physical Move pad layouts retain their existing behavior.
- MIDI guide, Slices engine guide and in-device MIDI help explain the mapping.

Includes the MIDI gate and takeover isolation repairs from test.55. Clip-bank
format remains 35; controller bundles and the published installer are unchanged.
