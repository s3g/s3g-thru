# s3g Thru 0.30.0-test.47

Local test build. The published installer remains 0.29.1.

- Fixes Bank clearing with the physical X/Delete key plus a right-hand pad.
  Menu no longer acts as the delete modifier. The modifier also synchronizes
  through the UI control path and resets on entering/leaving takeover.
- Keeps Undo intact when X is pressed with an already-empty Bank pad.
- Corrects the Bank screen grid to match Move's pad rows. Slot 01 is at the
  bottom left and slot 16 at the top right; saved sample assignments keep
  their existing pad positions.
- Gives the global Play Gain page a consistent title. COMP is the same
  polyphonic gain compensation previously labeled POLY; VOICE remains the
  separate Poly/Mono/Legato choice.
- Clarifies the native E16 rings: colors identify A/B encoder banks, while
  ring fill is not synchronized to Thru's parameter values. Read values on
  Move. Keep the existing THRU EM scene for this update.
