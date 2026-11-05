# s3g Thru 0.30.0-test.9

Local Stochastic listener test, including the earlier 0.30.0 features.
The published GitHub installer remains at 0.29.1.

- Adds **LAW LSTN** for sample deformation governed by internal note activity.
  Each note settles through reduced mutation, momentum and evolution, with a
  quick capture and slower release. A small evolution floor avoids hard freezing.
- Replaces knob 6 PULL with **CAPT** in LSTN only. Both retain separate saved
  values. AMUT/TMUT set the available depth; EVOL sets the underlying clock.
- Keeps listening within each part, before output/part level and effects. Pitch,
  source hold, note latching and stack scan remain independent controls.
- Adds CAPT at NRPN 3210 and expands LAW to 0–3. Update custom LAW enum encoding.
- Updates the manual, page graphic and in-device help.

Try LAW LSTN and CAPT 65%, then latch a note and click the main encoder to hold
its source. Raise CAPT for stronger settling; lower it for freer deformation.
For indefinite note latching use Pads & Scale HOLD = MAN.

Back up the set before testing: this build reads older clip banks, including
formats 26/27, and writes format 28. Older firmware cannot read the new format.
Move listening tests and full-polyphony deadline checks are still required.
