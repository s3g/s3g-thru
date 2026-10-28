# s3g Thru 0.28.0 — local testing build 9

- Plate and Room reverbs, plus customizable effect order, are saved and
  recordable with each performance clip.
- Cutups provides direct Lane and cut-count Advance controls, with shared
  playhead styling, physical-lane markers, and remembered lane holds.
- Split provides two independently configured instruments: left pads/lanes 1–2
  for A, right pads/lanes 3–4 for B, with four source voices each. Both can use
  the same engine. Capture selects editing focus; waveform knob 8 is Blend.
- Each part has Color and Stereo/Rotary inserts. Delay and Reverb are shared
  sends; Tape treats the combined stereo signal. Performance recording and
  undoable overdubs capture both instruments, parameters, sends, and Blend.
- Split cursors stay inside their two-lane instrument. B's idle pads use dim
  teal with brighter teal roots, with an explicit repaint at takeover entry.

## Easier effects setup

Shift + Capture opens **FX Sends**. Its knobs are now:

1. A Delay
2. A Reverb
3. B Delay
4. B Reverb
5. Delay Return
6. Reverb Return
7. Blend Curve
8. Blend

Sends and returns display percentages. New Split setups start with both returns
at 100%; existing setups retain saved values. If a raised send feeds a muted
return, the screen identifies the return knob to raise.

Main click on FX Sends opens **FX Pages**: turn/select and click/open Delay,
Reverb, the focused part's Color/Rotary, shared Tape, and Effect Order. Back
returns to the menu, then FX Sends. Record remains available for data capture.

A/B source levels are on knob 8 of their Engine parameter pages. Delay and
Reverb return levels appear only on FX Sends, with their other parameters on
named effect pages. The vacated positions remain reserved.

Audio tests cover both sides' stereo delay/reverb paths, zero sends, muted
returns, and tails after crossfading. Saved-set format and MIDI assignments
are unchanged from test 5 onward. This is a local test package; retain an
exported older set if you need to return to 0.27.0.
