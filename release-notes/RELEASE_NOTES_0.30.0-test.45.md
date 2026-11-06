# s3g Thru 0.30.0-test.45

Local E16 feedback reliability candidate. The published installer remains
0.29.1.

- Preserves requested E16 ring resynchronization while MIDI output is busy.
  A normal snapshot matching a previously sent value can no longer cancel a
  requested refresh before the controller receives it.
- Retains test.44's first-turn response and corrected Wavesets process indices.
- Uses the existing THRU NM scene; no controller transfer is required.

Validation reproduces the canceled-resync bug under output backpressure, then
checks the repair. Native input tests exercise eight held notes, pad pressure
between NRPN bytes, rendering, continuous controls and Wavesets process turns
with delayed feedback in Single, Split and Shared Dual.

These tests do not establish the sole cause of the reported hardware slowdown
while holding a pad. Compare Move's parameter response and E16's ring response
on this exact candidate during performance.
