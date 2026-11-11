# s3g Thru 0.31.0-test.17

Local Sequence display and navigation refinement on top of test.16.

- The Sequence waveform now shows each sounding trig's current sample position. A solid cursor marks the newest voice and a dashed cursor marks a second voice.
- A highlighted bar connects each trig's POS to its current reader position. This is a live position span; engines that loop or jump can move it nonlinearly.
- The overview footer displays the newest voice's scheduled note duration in milliseconds or seconds, or the selected lane's default duration when no voice sounds. A short non-looping source can end before that time.
- Capture + Menu's PITCH SETUP item now opens SEQ PITCH. It previously fell through to SEQ LANE because Quad setup is absent in Sequence mode.
- Manual screens and on-device help describe the indicators.

Local testing only. The public GitHub installer remains stable 0.30.1.
