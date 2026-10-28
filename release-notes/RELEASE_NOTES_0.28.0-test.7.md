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

FX Order uses a readable list. Main clicks on FX Order
edit routing; the Record button remains available for performance recording.

Cutups gains direct Lane and Advance controls in the existing waveform
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


## Split instruments — test 5

- Copy selects Single or Split. In Split, A uses the left four pad columns and
  stereo lanes 1–2; B uses the right four columns and lanes 3–4. Each part has
  four source voices and independent engine, envelope, scale, pad, stack-hold,
  Tone/Drive/Lo-Fi, and Stereo/Rotary settings. Both parts can use the same engine.
- Capture switches editing focus without changing held notes. Track selection
  also selects its part. Each side remembers its parameter page. Shift+Copy
  copies settings to the other side while keeping its samples.
- Waveform knob 8 controls Blend; Capture+knob 8 reaches it from any page.
  Shift+Capture opens Split Mix: levels, individual delay/reverb sends,
  crossfade curve, and Blend. Mute fades the focused side while it keeps playing.
- Delay and Reverb are shared stereo returns; Tape treats the combined signal.
  FX Order arranges those three blocks around Tape. Part inserts remain private.
- Performance clips record both instruments, separate parameter gestures,
  stack holds, sends, and Blend. Overdub takes remain undoable. Each clip saves
  both setups and its four source assignments. Single settings are remembered
  separately. MIDI channels 1/2 address A/B; CC 11–18 control the shared mixer.
- Cues offer four pads per side, and Rolls capture only that part’s playheads.
  Both engines and all sounding playheads remain visible on the waveform page.

Local hardware testing is still required for demanding engine/effect combinations.
Saved Split sets require test 5 or newer. Export a set before testing if you need
to return to an earlier version.


## Split waveform update — test 6

Solid playheads and dashed transition cursors now stay within their instrument’s
two-lane stack: A spans lanes 1–2 and B spans lanes 3–4. Hollow markers retain
the actual lane or crossfade position. This applies to every playback engine,
including Cutups joins and Wavesets transitions, whichever part is being edited.
Single mode retains its four-lane cursor view.


## Split pad colors — test 7

B’s idle pads use a muted grey-green tint, with lighter sea-green scale roots.
These desaturated colors identify the right-hand instrument without replacing
active voice, held-note, cue, or roll indicators. The tint also appears in
Chromatic mode; unused Cues rows remain off. Recalled colors appear immediately,
and changes to either part’s pad map refresh without requiring a pad press.
Audio processing and the saved-set format are unchanged.
