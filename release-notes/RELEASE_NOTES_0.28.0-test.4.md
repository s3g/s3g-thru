# s3g Thru 0.28.0 — local testing build

- Adds Plate (Dattorro-style) and economy Room reverbs, with decay, predelay,
  damping, diffusion, modulation, stereo width, and mix controls.
- Adds FX ORDER: arrange Color, Lo-Fi, Delay, Stereo/Rotary, Tape, and Reverb.
  Turn the main encoder to select an effect, click to lift it, turn to move,
  then click to drop and apply. Back cancels a move. Knob 7 selects a starting
  order, confirmed with the main click. Pending edits are silent.
- Reordering uses a 20 ms fade down and 20 ms fade up, retaining effect buffers.
- Reverb and routing are stored per clip. All reverb controls and the complete
  applied order can be captured in performance recording and overdub takes.
- Existing clips default to the original chain and zero reverb mix.

Local test package only. The new clip-bank format reads older sets, but this
build's saved sets require this version or newer; export a set backup before
hardware testing if you intend to return to 0.27.0.

Test 2 replaces the slot editor with a readable list. Main clicks on FX Order
edit routing; the Record button remains available for performance recording.

Test 4 gives Cutups direct Lane and Advance controls in the existing waveform
and Engine-page knob positions. Lane selects a physical source at the next
cut, including when notes are held. Pressure adds an independent per-voice
offset. Advance selects Hold or 1, 2, 3, 4, 8, or 16 cuts per lane; it remains
independent of region Repeat and stays tied to cuts as their tempo changes.
Loop pauses automatic advancement and remembers each pad's sounding lane.

Cutups uses the shared full-height playhead lines and hollow actual-lane
markers again. Dashed lines identify outgoing cuts during joins. All eight
voices and note releases are represented. At 100% Gate the join overlaps
consecutive cuts; shorter gates retain their silent gaps.

Advance is saved per clip, recorded as stepped automation, and available on
MIDI CC 19. Existing parameter and MIDI assignments remain available. Older
sets load with Advance set to one cut per lane. New saved sets require this
build or newer; preserve an exported set to return to an earlier test build.
