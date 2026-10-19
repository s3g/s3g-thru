# S3G Thru 0.16.0 overtake hardware acceptance

Desktop tests prove the module ABI and DSP behavior; they do not prove the
exact Move input, screen, pad, or realtime paths. Preserve the previous
working Schwung installation and do this pass with headphones first.

## Install identity

- Package: `dist/s3g-thru-module.tar.gz`
- Module ID: `s3g-thru`
- Component type: `overtake`
- Minimum Schwung: 1.7.3
- Expected binary: AArch64 Linux ELF, exported `move_plugin_init_v2`

## First safe pass

1. Set Move's main volume low and disconnect speakers.
2. Uninstall the earlier 0.1 sound-generator build if present, restart Move,
   then install the 0.16.0 tarball through Schwung Manager.
3. Open the Overtake menu with Shift + volume touch + jog-click and select
   S3G Thru.
4. Confirm Thru opens directly on a four-row waveform page without selecting a
   Move track or configuring MIDI Out.
5. With no lane recorded and monitoring disabled, confirm that mic/line input is
   silent at the output.
6. Select Move's desired input in the stock sampling flow. Start with Record
   Gain at 1.0 on the Mix / Global settings page.
7. Press Track 1, hold Shift and press Record, capture two or three seconds,
   then hold Shift and press Record again.
8. Confirm the waveform appears after the short `SCAN` interval.
   Confirm the lane number, activity fader, and waveform have visible spacing.
9. Play one pad softly and firmly. The stack offset may differ, but the
   single populated lane should have the same playback level. Outside note-entry
   mode, the pad should light only while held and switch fully off on release.
10. Hold the note and use pressure. Confirm it changes the stack offset without
    stopping the underlying lane motion or changing a velocity gain stage.
11. Hold eight pads. Confirm eight independently moving playheads are shown.
    Confirm the source pads use eight stable voice colors: red, orange, yellow,
    green, azure, blue-violet, purple, and magenta.
    Release them one at a time and confirm each cursor disappears immediately;
    no cursor should loop when all notes are off.
    Confirm each playhead has a hollow stack marker that moves continuously
    between the four waveform rows as its lane blend changes.
12. Record contrasting material into lanes 2-4. Confirm replacing one lane
    does not erase the others.
    Then hold Shift and press a Track button. Confirm the sample browser opens
    at Move's User Library, folders and WAVs can be navigated with the main
    encoder. Confirm each highlighted WAV auditions without replacing the lane,
    and clicking loads it. Test one mono and one
    stereo WAV at a non-44.1-kHz rate; confirm loading does not glitch currently
    playing audio and the footer shows the loaded filename.
    Assign a different library WAV to the same lane in two patterns and switch
    between them. Confirm each pattern recalls its own assignment and no lane
    changes before the complete queued bank is ready. Create a new pattern and
    confirm `INHERIT`; clear the lane and confirm explicit `EMPTY`. Rename one
    assigned WAV temporarily and confirm `MISSING` leaves the current pattern
    playing instead of partially switching.
13. Press Menu and use dial 1 on the Engine page to test Motion, Lanes, Grains,
    Cutups, Sample, and Slices. Confirm the 16 Step keys no longer change the engine.
    In Motion, test Mirror, Zigzag, and Drunk with each of Continuous, Packets,
    Motor, and Bounce; the cursor must visibly travel backward as well as
    forward. Verify Packet Duty and independent Motor rate/symmetry/shape.
    In Lanes, compare Crossfade and Jump, all path shapes, Random, and the
    transition slew. In Grains, test all four processes and variation controls;
    notes must change scan speed without changing Grain Pitch. In Cutups, test
    Free and Tempo clocks, divisions, repeats, several region counts, lane
    orders, and two held notes; notes must change slice pitch without changing
    the cut clock. Switch Cut Region Source between Equal and Transients and
    confirm the selected waveform shows detected boundary ticks. In Sample,
    test forward, reverse, loop, and ping-pong. In Slices, select Slices pad mode
    and confirm different pads start different detected regions. Test Cycle,
    Random, No Repeat, and Velocity lane variation with contrasting lanes;
    velocity must select a lane only in Velocity mode, never alter gain. In Cues
    pad mode, use Shift+pads 1–8 to store, pad to jump, and Delete+pad to clear.
    In Rolls mode, hold several pads and confirm independent tempo divisions
    stop on release.
14. Use Up/Down to transpose the grid and confirm held/released notes do not
    stick.
15. Press Menu. For each Algorithm, use Up/Down to visit Engine, only that
    selected playback engine's pages, Mix / Rec, Sample, Performance Loop, and Chain. Confirm
    unrelated engine pages are absent. Confirm every parameter has a padded name with its value
    underneath, no miniature dial graphic or text overlap, and each physical
    dial edits the cell directly beneath it.
16. On Performance Loop choose Replace. Press Record, perform overlapping pads
    with different strike velocities, vary pressure independently, and turn
    Position, Field, Stack, and one engine-specific dial. Press Record again.
    Confirm the exact free-time phrase loops, the thin position line traverses
    the display, and the pads reproduce independent voice colors/playheads.
    Repeat once while holding a pad before pressing Record; confirm that note
    is present from the start of the captured performance and closes normally.
    Press Play twice and confirm stop/restart resets cleanly. Sweep Loop Rate
    from 0.25x to 4x and confirm note timing and gesture motion scale together.
17. Choose Overdub, record another polyphonic pass, and confirm the original
    duration and events remain. Use Left/Right (and Shift+Up/Down) to select
    banks A-D; every Step key should launch a clip slot. Copy A01 to A02 with
    Shift+Step, edit A02, and verify A01 is unchanged. While running, select
    another clip and confirm it stays blue-violet/queued until the current loop
    seam. Use Delete+Step to clear only a disposable slot's performance data,
    confirm its sample assignments remain, then use Shift+Back to restore it.
    Program two Chain slots with different repeat counts and confirm changes
    happen only at full loop boundaries without partial sample-bank swaps.
18. On the Sample page, test trim, reverse, normalization, and edge fade on a
    disposable capture. Confirm Undo restores the preceding source and that an
    edited capture survives reopen.
19. Connect the programmed external controller. Test CC 20–51, compatibility
    CC 54–68, engine controls CC 69–113, and loop controls CC 114–117 at both
    endpoints; test CC 52 as an audio record gate and CC 53 as a clear trigger. Confirm
    Move's own Menu/Back/Shift buttons do not alter mapped parameters.
20. Open Help and record its maximum render-time and overrun counters while
    holding eight dense Grains voices and switching prepared patterns. Return
    to the waveform page, press Back, and confirm Thru exits cleanly
    with no hanging note or monitor.
21. Reopen S3G Thru. Confirm all four waveforms, stereo audio, selected engine,
    octave, edited settings, performance notes/durations, source-pad identities,
    pressure/parameter paths, loop rates, active clip, and 64-slot bank are
    recalled. Confirm the performance transport is stopped after recall until
    Play is pressed.
22. Make an edit, wait at least ten seconds for an autosave interval, then use a
    normal overtake close/reopen and confirm the edit recovers. This does not
    authorize pulling power during a file write.
23. Clear one lane with Shift + Delete, exit, reopen, and confirm only that
    lane remains absent.
24. On the Sample page, confirm Trim, Reverse, Normalize, and Fade show `TURN`.
    Turn Normalize in either direction and confirm the header briefly reads
    `NORMALIZED L#`; compare the result at a fixed Level. Repeat on an empty lane
    and confirm the UI reports that the lane is empty instead of failing silently.

## Monitor and input checks

- Monitoring defaults to zero. Mix / Rec or external CC 51 can enable it; use
  headphones and return it to zero after the explicit monitor test.
- Compare Move's mic and line selections. S3G Thru receives whichever source
  Move exposes in the audio-input mailbox.
- If `LIM` appears, reduce Move's input gain.

## Performance evidence to record

- maximum reliable lane length;
- display responsiveness while `SCAN` is active;
- audio continuity while replacing a lane;
- audio continuity and load time for long library WAVs;
- CPU behavior for each algorithm, especially dense Grains;
- CPU behavior with eight simultaneous voices and eight dense Grains voices;
- Help-screen maximum render microseconds and overrun count for each engine;
- peak memory/launch reliability with the active and prepared sample banks;
- preparation time and boundary behavior for four long, different pattern WAVs;
- whether note-off release feels appropriate for short pad gestures;
- whether pressure-controlled depth is the right default;
- any mismatch between the selected Move input and captured audio; and
- whether the overtake open/exit cycle is reliable;
- time required to save four full 12-second lanes on exit and recall them on
  the next launch;
- Replace/Overdub feel, pressure/knob path continuity at the loop seam; and
- 0.25x–4x timing stability, clip-queue timing, bank/copy/clear feedback, and
  Play/Step LEDs.

Do not call the module hardware-accepted until this pass succeeds on the exact
installed tarball.

