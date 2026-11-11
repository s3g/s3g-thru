# s3g Thru 0.31.0-test.18

Local Sequence test build. No GitHub release or catalog update is planned for this candidate.

- Sequence DUR now reaches 32 steps. SEQ SETUP adds UNIT: STEP follows lane RATE, BPM and clip speed; TIME holds a note for 10 ms to 30 s regardless of those clocks. Trigs and Burst events can lock either duration independently. The envelope fits within the note lifetime.
- SEQ MIX gives four lane levels and stereo balances. SEQ STRIP gives each lane low, mid and high EQ, mid frequency and Solo. SEQ DUCK lets one lane act as the priority key while the other three make room, with threshold, depth, attack, hold, release, detector HPF and knee.
- Sequence mixer settings and absolute durations are stored with clips. New storage format 39 reads formats 38 and 37; older builds cannot read format 39 projects.
- The manual explains engine choices and how POS, retrigger fades and ADSR affect sample starts. POS remains exact rather than snapping to a zero crossing.
- Burst count, event selection, timing and event POS/NOTE/VEL/DUR now share one editor page. A step-time ruler draws each retrigger as a small waveform and highlights the selected event.
- Copy + step copies a complete trig; Shift + step pastes it into another cell. Sequence pads do not use Shift for note latch.
- Sequence lane Mute edits now update the active clip, so they survive playback restarts and clip recall. The overview waveform marks each parent trig’s POS rather than inheriting the first Burst event’s offset.

Test with four active lanes, long overlapping notes, clip changes and live mixer edits on Move before release.
