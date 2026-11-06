# s3g Thru 0.30.0-test.41

Local controller-startup safety candidate. The public GitHub installer remains
at 0.29.1.

- Keeps Thru authoritative when an E16 native scene is connected, powered or
  recalled after a set is already open.
- Gives all 192 private A/B aliases independent 14-bit pickup. Remembered zero
  or nonzero E16 values are rejected until the ring reaches Thru's current
  value, even when general MIDI takeover is set to Jump.
- Prioritizes current-value feedback after a rejected startup value. E16 and
  takeover may be started in either order; Monolit direct CC behavior is
  unchanged.
- Retains the test.40 four-bank Pages hub, Capture + Track shortcuts, bank-local
  Up/Down, hierarchical Back behavior and complete All list.
