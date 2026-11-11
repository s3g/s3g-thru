# s3g Thru 0.31.0-test.16

Local Sequence playback correction on top of test.15.

- Changing a lane’s RATE during playback no longer causes its sample sequence to fall silent. The sequencer now discards duplicate-trigger history from the previous rate when that lane changes division.
- The fix applies to direct Move edits and recorded RATE gestures. Trigs, loaded samples, clip content, and other lanes retain their state.
- A slower RATE still schedules the next hit at its new division, which can produce an expected longer interval.
- The Sequence manual and on-device help now explain live RATE changes.

Local testing only. The public GitHub installer remains stable 0.30.1.
