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

Test 3 makes waveform Scan Rate control Cutups' lane progression independently
of its cut rhythm and region repeats. Stack Start sets the initial route offset.
Loop holds the audible lane phase across note releases and performance recall.
Current cuts keep their source until the next event, retaining the existing
anti-click fades. At 100% Gate, Join now overlaps the outgoing and incoming
cuts; shorter gates retain their silent gaps. Random follows sixteen repeatable choices per scan cycle.

Cutups playheads now stay within the lane waveform actually being read.
Outgoing cuts use dashed markers during joins, and all eight voices—including
note releases—can be shown. Lane bars reflect the sounding cut envelopes.
