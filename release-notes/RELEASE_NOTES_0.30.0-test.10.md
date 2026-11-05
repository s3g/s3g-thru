# s3g Thru 0.30.0-test.10

Local Pulsar engine test, including the earlier 0.30.0 features.
The published GitHub installer remains at 0.29.1.

- Adds **Pulsar**, a fifteenth engine: windowed sample fragments become playable
  pulse trains, from separate rhythmic events to pitched buzzes and textures.
- Adds RATE, FORM, SIZE, WIN, SKEW, OMIT, BRST and JITT on one page. Notes
  transpose pulse repetition; source travel and stack scanning remain independent.
- Adds main-encoder source hold, with saved and recorded source positions.
- Gives each note independent timing and four bounded internal readers. Active
  pulses finish through edits; filtering, zero-edge windows, overlap gain control
  and DC removal condition the result before the existing envelope/effects.
- Adds fragment brackets/activity marks and an overlap-limit footer.
- Adds NRPN 3328–3337. RATE, FORM and SIZE use logarithmic 14-bit encoding.
  Algorithm/Engine List ranges now span 0–14; update custom enum encoding.
- Updates the manual, screen diagram and in-device help.

Start with a sustained recording, select Pulsar and play notes. Try FORM
500–1200 and SIZE 2–8 ms. Click to hold its source. For slower rhythms, reduce
RATE to 2–12 Hz and try BRST 2:1. HOLD = MAN allows indefinite Shift+pad latching.

Back up your set before testing: this build reads older banks, including
format 28, and writes format 29. Older firmware cannot read the new format.
Move listening tests and full-polyphony deadline checks remain necessary.
