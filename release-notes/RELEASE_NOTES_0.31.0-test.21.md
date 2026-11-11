# s3g Thru 0.31.0-test.21

Local Sequence interface test build. It is not a GitHub or Schwung catalog release.

- The Mix/FX page list now labels FX ORDER as shared. The former "A FX ORDER" label incorrectly implied that each lane had its own order.
- Delay and Reverb remain shared processors with individual lane sends. Tape processes the combined signal. FX ORDER places those shared processors relative to one another; it does not reorder any lane's private inserts.
- The Sequence manual explains the routing. No DSP routing, clip format, project format or MIDI parameter changes are in this update.

All changes from 0.31.0-test.20 remain included. Check the page label on Move before release.
