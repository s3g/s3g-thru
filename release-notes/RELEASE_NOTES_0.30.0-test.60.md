# s3g Thru 0.30.0-test.60 — Custom MIDI profiles

Local testing build; not published.

- MIDI Maps adds four editable controller profiles, 128 assignments per profile,
  eight controller banks, MIDI Learn and manual channel/CC entry on Move.
- Map named parameters to A–D or Focus, follow an engine-page slot, or assign
  clip transport, recording, Undo, Panic and bank commands.
- Set range limits, reverse direction, pickup and absolute/Hold/Toggle/Trigger
  behavior independently. Conflicts require confirmation before replacement.
- Copy the read-only nanoKONTROL2 starter into a Custom slot and edit it.
- Profile selection leaves instrument values intact. Profile changes save in a
  separate background-written file; clip changes leave the map in place.
- Includes Slice Edit and WAV-marker import from test.59. Clip-bank format 36,
  JSON state 19 and all 364 public MIDI IDs remain unchanged.

Controller connection and listening tests on Move are still needed.
