# s3g Thru 0.30.0-test.8

Local expanded Stochastic test. Includes test.7 features. The published
GitHub installer stays at 0.29.1.

- Makes AMUT reach polarity inversion and reflected folding of recorded PCM.
- Expands TMUT into strong accumulated logarithmic time deformation with
  sixteen landmarks. PULL restores the source shape; MEM retains momentum.
- Expands EVOL with precise logarithmic adjustment, faster free rates and CYC
  for one evolution per fragment cycle. Work stays bounded at incoming overlaps.
- Adds main-encoder click on the Stochastic page to hold/resume the source.
  The footer shows source hold; notes and stack scanning remain independent.
- Adds prepared stereo low-pass levels and antiderivative filtering of folding.
  Analysis stays on the background worker; note rendering uses fixed storage.
- Adds NRPN 3208–3209 for source hold and its position; preserves existing IDs.
  EVOL's range is now 0–1024. Update custom controller range encoding.
- Updates the manual, screen diagrams and in-device help.

Existing programs retain their settings. For the strongest transformation,
try SIZE 1–4, AMUT/TMUT 100%, MEM 90%, PULL zero and EVOL CYC or a fast rate.
Click to hold the source while it evolves. For indefinite Shift+pad latching,
set Pads & Scale HOLD to MAN.

Back up the set before testing. Reads format 26 and older banks; writes format
27, which older firmware cannot read. Hardware listening and eight-note CPU /
deadline checks remain necessary, including effects and pattern crossfades.
