# s3g Thru 0.30.0-test.36

Local E16 native reset refinement. The public GitHub installer remains at
0.29.1.

- Quick-pressing an active encoder in the THRU NM scene resets its current
  parameter to the Thru default.
- Resolves defaults dynamically, including ENG 1–4 pages after either part
  changes playback engine.
- Returns immediate feedback to the ordinary turn alias so the E16 value and
  ring follow the reset.
- Accepts E16's seven-bit NRPN push value without weakening the normal fixed
  NRPN path's complete 14-bit transaction requirement.
- Includes the test.35 MIDI Control ordering change: N+M precedes Off, leaving
  Off as the final MODE choice while preserving saved test.34 behavior.
