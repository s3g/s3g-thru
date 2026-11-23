# s3g Thru 0.33.1

A stability update focused on processing headroom when combining synthesis engines.

- **Lower synthesis CPU load:** TABLE, SCANNED, FM, NOISE, RESON, PARTIALS and LOWFORM now limit work to the patches each part can reach. This particularly benefits Sequence, Quad and Split configurations.
- **Lighter note starts:** RESON avoids repeated overload preparation during note starts and patch re-entry. TABLE has a dedicated path for single-lane voices.
- **Eight-voice playing and stack scanning retained:** all saved patches remain available, with normal phase and envelope progression through the reachable stack. Existing projects, MIDI mappings and parameter settings remain compatible.
- **Broader stability coverage:** mixed-engine playback, rapid retriggering, Burst, sound locks, MIDI, effects, LFOs, releases and timed clip fades have been checked together. Audio comparison renders match the previous behavior.

Install or update through Schwung Manager using `https://github.com/s3g/s3g-thru`, or download `s3g-thru-module.tar.gz` from this release. No controller preset changes are required.
