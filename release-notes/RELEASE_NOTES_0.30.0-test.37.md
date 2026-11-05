# s3g Thru 0.30.0-test.37

Local E16 default-reset repair. The public GitHub installer remains at 0.29.1.

- Replaces test.36's reset NRPN address with a dedicated Note push for each
  active E16 encoder.
- Prevents a reset press from replacing the encoder's selected NRPN address,
  which had made subsequent turns continue sending reset requests.
- Retains dynamic per-engine defaults, immediate ring feedback and unchanged
  Move editing focus.
- Adds a regression that resets an adaptive ENG parameter, then continues the
  same encoder turn using Data Entry without resending the NRPN selector.
