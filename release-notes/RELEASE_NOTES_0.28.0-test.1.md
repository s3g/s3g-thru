# s3g Thru 0.28.0 — local testing build

- Adds Plate (Dattorro-style) and economy Room reverbs, with decay, predelay,
  damping, diffusion, modulation, stereo width, and mix controls.
- Adds FX ORDER: arrange Color, Lo-Fi, Delay, Stereo/Rotary, Tape, and Reverb.
  Knobs 1–6 edit positions; knob 7 selects a starting order. Turn knob 8
  clockwise to apply or counterclockwise to cancel. Pending edits are silent.
- Reordering uses a 20 ms fade down and 20 ms fade up, retaining effect buffers.
- Reverb and routing are stored per clip. All reverb controls and the complete
  applied order can be captured in performance recording and overdub takes.
- Existing clips default to the original chain and zero reverb mix.

Local test package only. The new clip-bank format reads older sets, but this
build's saved sets require this version or newer; export a set backup before
hardware testing if you intend to return to 0.27.0.
