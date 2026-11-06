# s3g Thru 0.30.0-test.44

Local E16 encoder-response candidate. The published installer remains 0.29.1.

- Makes choice controls and short counters respond to the first turn after
  feedback reaches E16, without waiting for a value echo from the controller.
- Accepts fine and coarse native movements, with full list travel in both
  directions and rapid consecutive turns.
- Corrects Wavesets PROC feedback: all ten menu choices now match Move, including
  the processes after Multiply. Historical internal process IDs remain intact
  in saved clips and automation.
- Sends feedback for the edited control without restarting the complete MIDI
  feedback sweep on every choice change.
- Keeps continuous parameters and larger numerical ranges at absolute 14-bit
  resolution, with startup pickup for recalled controller values.
- Uses the existing test.39-or-newer THRU NM scene; no E16 transfer is required.

Validation covers every short integer/choice across each adaptive engine page,
complete Wavesets process travel with no feedback echoes, A/B public NRPN menu
indices, and the existing MIDI/controller regressions. Hardware feel still
requires testing on Move and E16.
