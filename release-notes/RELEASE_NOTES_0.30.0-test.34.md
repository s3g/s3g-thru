# s3g Thru 0.30.0-test.34

Local E16 + Monolit feedback refinement. The public GitHub installer remains at
0.29.1.

## Responsive E16 rings

- Prioritizes the parameter most recently changed by a Monolit fader instead
  of waiting for the background sweep of all 192 native E16 aliases.
- Adds an urgent UI feedback queue so the changed NRPN is the first complete
  message sent to E16 on the next feedback tick, even when page initialization
  values are still pending.
- Retains all 128 positions supplied by Monolit's 7-bit CC faders. E16 turns
  remain 14-bit; the improvement removes temporal subsampling rather than
  inventing resolution absent from the source controller.
- Retains the test.33 native-ownership repair: N+M cannot be replaced by E16
  Remote discovery or handshakes.
