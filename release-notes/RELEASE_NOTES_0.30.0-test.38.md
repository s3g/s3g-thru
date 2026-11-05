# s3g Thru 0.30.0-test.38

Local E16 reset-transport repair. The public GitHub installer remains at
0.29.1.

- Replaces test.37's Note reset pushes, which were filtered before reaching
  the module, with press-only CC messages.
- Uses isolated control channels 14 (A) and 15 (B), CC 0–95, avoiding Thru's
  musical notes, Monolit channels and the E16 turn NRPN selector.
- Retains dynamic per-engine defaults, immediate ring feedback and unchanged
  Move editing focus.
- Tests reset delivery for A and B and verifies that NRPN Data Entry continues
  to control the same encoder immediately after the press.
