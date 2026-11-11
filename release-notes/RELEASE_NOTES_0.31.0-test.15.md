# s3g Thru 0.31.0-test.15

Local Sequence usability update on top of test.14.

- The sequencer now calls its note-length control **DUR**. SEQ LANE sets the default; TRIG and Burst events inherit it until explicitly locked. A star marks a locked value.
- DUR reads out the resulting milliseconds or seconds at the current lane RATE, BPM and clip rate. Its saved range remains 0.01–4 steps, so existing clips and MIDI mappings retain their values and addresses.
- Stopped trig audition follows clip rate and the trig’s duration lock, matching the duration shown for normal playback.
- On-device help, MIDI names, generated screens and the Sequence manual reflect the new wording.

Local testing only. The public GitHub installer remains stable 0.30.1.
