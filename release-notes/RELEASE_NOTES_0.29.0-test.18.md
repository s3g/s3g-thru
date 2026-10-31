# s3g Thru 0.29.0-test.18 — Formant articulation

Local testing build following test.17. Formant now fills one eight-control page:

| Knob | Display | What it controls |
| --- | --- | --- |
| 1 | PIT | Pitch transpose, −24 to +24 semitones. |
| 2 | FORM | Formant shift, −24 to +24 semitones. |
| 3 | SMTH | Live-change smoothing, 5–200 ms. |
| 4 | TUNE | Blend the original pitch contour toward the played MIDI note, 0–100%. |
| 5 | FRZE | Capture and sustain a source position for existing and new notes. |
| 6 | KEYT | Formant tracking of keyboard/PIT transposition, −100 to +100%. |
| 7 | ROUG | Reproducible, smooth irregularity in pulse timing, 0–100%. |
| 8 | SUB | Alternate-cycle emphasis for an octave-below component, 0–100%. |

- **Keyboard tuning:** TUNE at 100% follows the played MIDI pitch in confidently voiced regions. Zero retains test.17’s relative pitch behavior and original source contour.
- **Freeze across notes:** FRZE captures the oldest held playhead in that part, otherwise a release tail, or Position when idle. New notes use that same point. Stack scanning remains independent. Off resumes sample travel; Off/On captures again. Cue recall updates the frozen point. Position/Field still bound the usable region.
- **Freeze timing:** the initial state and frozen source point are retained, and freeze/cue changes replay at their captured times.
- **Independent parts and performance recall:** all controls and the frozen source position save with the part/clip and can be captured in a performance. The frozen point survives note release and a fresh note.
- **MIDI:** CCs 40–45 address knobs 1–6; CC 28 addresses ROUG, CC 29 addresses SUB. FRZE is Off below 64 and On from 64 upward. Other engines keep their existing mappings.
- **Continuity and cost:** existing grains finish their captured windows; new values are smoothed. ROUG uses bounded clock variation; SUB uses continuous phase and never boosts peak gain. The same fixed grain pool and six-period overlap cap remain. Control smoothers settle cleanly at their targets.
- **Documentation:** updated eight-control screen, parameter reference, MIDI guide, and starting points.

## Sets and compatibility

Export/back up the set before testing. New saves use **format 23**, with 205 stored parameters: the five new controls and their frozen source position are appended after the existing IDs. Formats 1–22 remain readable. Older sets initialize all five controls to zero/Off; their existing Formant settings retain test.17 behavior. Older builds cannot read a format-23 set.

## Hardware testing

Check tuning with a voiced phrase, freeze while a note sounds and then play new notes, recall a cue while frozen, and resume scanning. Check independent A–D settings, performance capture/recall, long release tails, eight-note playing, stack travel and simultaneous effects. Extreme combined shifts, noisy sources, and heavy polyphony still need listening and CPU checks on Move; host checks do not establish hardware acceptance.
