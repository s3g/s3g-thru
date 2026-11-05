# s3g Thru 0.30.0-test.11

Local Pulsar Mod test, including all earlier 0.30.0 features.
The published GitHub installer remains at 0.29.1.

- Adds **PULSAR MOD**, a second eight-control page: emission speed/depth,
  formant speed/depth, source scatter, formant variation, reverse probability
  and formant keyboard tracking.
- Lets held notes follow live edits, with continuous pulse-clock phase and
  completed windows for sounding pulses. Stereo channels share all choices.
- Keeps scatter inside Field, anchored to the held point during source hold.
  REV% reverses recorded audio independently of the selected amplitude window.
- Defaults the new amounts to zero, retaining existing sounds and pulse masks.
- Adds NRPN 3338–3345. ERAT/FRAT use logarithmic 14-bit encoding.
- Updates the manual, screen diagrams and in-device help.

From Pulsar, press Down to reach Pulsar Mod. Try FRAT 0.2–1 Hz and FDEP
3–7 semitones on a held vocal source, then add a little SPRY or REV%.
Source-hold click is available on either Pulsar page.

Back up your set before testing: the build reads older banks, including
format 29, and writes format 30. Older firmware cannot read the new format.
Move listening and full-polyphony deadline checks remain necessary.

