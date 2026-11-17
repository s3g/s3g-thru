# s3g Thru 0.32.0

This release expands Sequence with saved generators, new playback routes and more direct performance editing, and brings the instrument mixer to every mode.

- **Euclidean and Onset sequencers:** choose Normal, Euclidean or Onset independently for each lane. Generate rhythms and sample positions, edit their trigs and locks, and save the result in clips. PRINT freezes a generated sequence to Normal; REGEN clears manual overrides. Euclidean SHIFT moves the generated hit pattern across the pads.
- **Directions and grid paths:** combine FWD, REV, PING, F+B or RAND with HORIZ, VERT or DIAG. Each lane retains its own clock, length, rate and rotation. Direction, path and generator controls can be recorded in performance clips.
- **More predictable clip editing:** in Sequence, with Record off, editing a control replaces only that control's recorded automation, so the new setting survives Stop/Play. Untouched automation continues during overdubs; edited controls respond immediately, with take Undo retained.
- **Clear without losing the sequence:** Delete/X + Capture clears recorded notes and parameter motion while retaining authored trigs and samples. Delete/X + a bottom clip button clears that clip's sequences, locks, performance and sample assignments. Undo restores the last clear.
- **Mixer in every mode:** the shared MIXER provides the active instrument levels and stereo balances, with numbered strips for EQ, Solo and Mute. Priority Duck is available for multiple instruments. Source-lane gains remain separate.
- **Clearer navigation and duration controls:** PLAY begins with SEQ LANE and SEQ SETUP, followed by the selected type's pages. Source pages keep LOCKS, PRINT and REGEN together. New sequences default to TIME; STEP displays duration in lane-step counts. Inactive controls show reserved squares.
- **Sequencer stability:** GAP durations follow the actual direction and path, Burst and microtime while preserving manual DUR locks. Live rotation and length edits retain pending Burst events, including generated-grid changes. Source-range save/recall, duration-unit handling, input validation and editor feedback are improved.
- **Updated manual, screen diagrams and device help**, with the expanded MIDI reference for sequencer controls.

Existing projects and samples remain supported, and existing MIDI parameter identities are retained. Projects saved in 0.32.0 require 0.32.0 or newer to reopen. Export a project backup before updating if you may need to return to an older version.

Install or update through Schwung Manager using `https://github.com/s3g/s3g-thru`, or download `s3g-thru-module.tar.gz` from this release. Requires Schwung 1.7.3 or newer. E16 Remote and E16 + Monolit examples are included as separate downloads.
