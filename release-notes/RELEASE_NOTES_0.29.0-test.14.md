# s3g Thru 0.29.0-test.14

Local testing candidate.

- Added Prism: playable spectral scanning, frame freeze, amplitude/frequency hold, accumulation, blur, transpose, tilt, and smear.
- Added Morph: independently blend spectral amplitudes and frequencies with the next source lane. Quad pairs lanes 1 → 2 → 3 → 4 → 1; Split wraps inside each two-lane part. An unavailable partner temporarily uses the main source.
- Added Texture: held notes generate bounded stereo sample phrases with Spray, Grouped, Motif, and Ornament modes, scale-aware pitch intervals, timing scatter, group size, spread, and a repeatable seed.
- Each engine has one dedicated parameter page, shares the existing keyboard, envelope, and performance-data recording, and retains independent per-part settings.
- Spectral analysis runs off the audio thread after loading, recording, or editing. The waveform footer displays SPECTRAL PREPARING while sources are being prepared. Spectral playback uses fixed-size overlapping synthesis windows.
- Added engine manual pages, parameter tables, screen diagrams, and Quad pairing guidance.

Saved sets use format 20. Formats through 19 remain readable; existing parameter IDs and chain layouts are retained. Back up a set before testing: older builds cannot read a set after it has been saved in format 20.

Prepared for local Move testing, not published. On-device sound, timing, peak CPU load with eight notes and effects, source loading, and pattern transitions require hardware testing.
