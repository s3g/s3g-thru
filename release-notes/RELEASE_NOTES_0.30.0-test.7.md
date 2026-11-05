# s3g Thru 0.30.0-test.7

Local Stochastic playback test. Includes all test.6 features; the published
GitHub installer remains at 0.29.1.

- Adds Stochastic: recorded PCM deformed by bounded timing and amplitude walks,
  with momentum, source attraction, distribution choice and optional period drift.
  New notes start with an independent shape, including short notes at EVOL Hold.
- Adds one eight-knob page, keyboard pitch, independent note evolution, stack
  scanning and Character pressure. Compatible with Engine Lists and every mode.
- Blends source/shape changes through overlapping stereo-linked readers, with
  smooth size/rate changes, bounded processing and a DC blocker.
- Adds eight NRPN addresses, 3200–3207, without reassigning existing IDs. The
  Algorithm and Engine List enum ranges include index 13: custom controllers
  must use the new range when encoding a choice.
- Updates the manual, screen diagram and in-device help.

Back up your set before testing. Reads prior banks and writes format 26;
earlier firmware cannot read the new saved bank. Hardware listening and Move
render/deadline telemetry remain part of testing, especially with eight notes
and effects. The waveform cursor follows source travel, not each tiny fragment.
