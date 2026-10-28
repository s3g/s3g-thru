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
