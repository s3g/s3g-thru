# s3g Thru 0.30.0-test.33

Local E16 native-ownership repair. The public GitHub installer remains at
0.29.1.

## E16 NRPN + Monolit

- Prevents Thru's UI Remote heartbeat from probing, clearing or claiming E16
  while MIDI Control is set to N+M, NRPN, MNLT, Generic CC or Off.
- Rejects incoming Remote handshakes in those modes as a second guard, matching
  the existing DSP-side ownership rule.
- Releases an active Remote session immediately when the user changes to a
  non-Remote MIDI mode. Auto and E16 continue to support the readable Remote
  interface normally.
- Updates setup instructions to select Schwung Surface CC Only and persist N+M
  before connecting E16 to Move. The script-free THRU NM scene can then remain
  active while receiving dynamic 14-bit A/B mappings and forwarding Monolit.

This build also contains the complete Priority Duck implementation introduced
in test.32. Install `dist/s3g-thru-module.tar.gz` and load the matching
`dist/s3g-thru-e16-nrpn-monolit.zip` for hardware testing.
