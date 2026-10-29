# s3g Thru 0.28.0 — local testing build 20

- Quad adds four independent eight-pad instruments: A upper left, B upper right,
  C lower left, and D lower right. Each owns one stereo source lane and two
  source voices. Engines, parameters, keyboards, envelopes, and private effects
  can differ for all four parts.
- Choose Capture + Menu → Instrument Mode → Quad. Track 1–4 selects A–D and its
  source lane; Capture cycles editing focus. Copy returns to Single and back to
  the last chosen multi-instrument mode. Shift + Copy copies settings to the
  next part, preserving its source audio, notes, and mute.
- Muted neutral, teal, amber, and plum pad backgrounds identify the four zones.
  Active voices, scale roots, latches, Cues, and Rolls keep their status colors.
- Waveforms show one row per instrument, with its own live Position/Field zoom.
  Cursors stay within that row. The header shows focus and four voice counts;
  Quad Mix Status shows four activity meters and silence reasons.
- Quad uses independent part levels rather than A/B Blend. Waveform knob 8 is
  LEVL for the focused part; Capture + knob 8 reaches it from any page. Each part
  enters the sum with 6 dB of headroom before the shared mastering path.
- FX Sends exposes all eight A–D Delay/Reverb sends. Delay Return and Reverb have
  DRTN/RRTN on knob 8. Color and Stereo / Rotary remain private inserts; Delay,
  Reverb, Tape, and FX Order remain shared.
- One-lane Quad pages omit Stack Path and lane-scan controls, leaving reserved
  cells. Each part supports an eight-pad Slices/Rolls layout and two Cues on the
  leftmost pads of its quadrant’s lower row. Loop retains Glitch’s Shift + Loop buffer gesture and, when Tape Inertia is
  enabled, the Tape transport hold.
- Performance Record, already-held notes, automation, overdub/take undo, clip
  banks, chaining, and saved sources include C/D. MIDI channels 1–4 address A–D;
  Quad CC 13, 15, and 17 control the addressed part’s level, Delay, and Reverb send.
- Extended the bank format for four programs and C/D mixing. Existing sets remain
  readable; export a backup before testing because new saves require this build
  or newer. Existing parameter IDs retain their order.
- Updated the manual and generated Quad screen illustrations. The eight-source-
  voice limit remains; device CPU load and physical LED colors need Move testing.

Includes testing build 19. Local package for testing on Move.
