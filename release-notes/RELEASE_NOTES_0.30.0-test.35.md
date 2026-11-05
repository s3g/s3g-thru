# s3g Thru 0.30.0-test.35

Local MIDI Control ordering refinement. The public GitHub installer remains at
0.29.1.

- Reorders MODE as Auto, NRPN, CC, MNLT, E16, N+M, Off, placing Off at the
  final encoder position.
- Migrates saved test.34 state so an existing N+M selection remains N+M and an
  existing Off selection remains Off after the two numeric positions swap.
- Retains the test.34 priority path for responsive E16 rings during Monolit
  fader movement and the test.33 native-surface ownership repair.
