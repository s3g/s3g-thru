# s3g Thru 0.29.0-test.4

Local testing candidate, including the additions and repairs in test.1–test.3.

## Pattern fades

Open **Pattern Fade** after Performance Loop, or select it in Capture + Menu.

- **Knob 1 / TIME:** CUT (the existing immediate change) through 10 seconds.
- **Knob 2 / CURVE:** LIN, IN, OUT, SMTH, or PWR. Power maintains equal power
  through the blend; Linear keeps the gains complementary.
- **Knob 3 / POS:** move to any blend position and hold there automatically.
- **Knob 4 / HOLD:** pause or resume the transition. Clicking the main encoder
  toggles Hold on this page.
- **Knobs 5–7 / FROM, TO, BUF:** source clip, destination clip, and buffer-underrun
  count. These are readouts.
- **Knob 8 / DONE:** turn clockwise to finish the change with a short smooth fade.

Choose a different clip using the existing bank buttons. Samples are prepared
before the change; playing clips retain their existing loop-boundary launch.
The outgoing performance continues with its sample bank, engine settings,
automation, held voices and effect tails while the destination starts. The
fade blends both stereo outputs. It does not interpolate note numbers, engine
enums, or sample-file contents into a new pattern.

Hold pauses the blend, while both performance clocks and their samples keep
playing. Resume continues from the held position. New notes and edits belong
to the destination. Note releases, transport stop, and Panic also reach the
outgoing scene. The waveform and normal parameter pages show the destination;
the waveform footer and Pattern Fade show transition progress.

Single, Split, Shared Dual and Quad can transition, including between modes.
Each side retains its eight-voice instrument and separate effect histories.
An outgoing scene runs on a companion thread with a short stereo FIFO; the
main audio callback retains one shared compressor and limiter. BUF counts
underruns that could affect continuity; Shift + Menu's reset also clears BUF.
Heavy combinations still need testing on Move.

A further selection waits until the current transition finishes. A held fade
therefore holds chain advancement too. Resume or DONE before the next change.
Finish a fade before recording, clearing/copying clips, or restoring a set.
Changing TIME during a fade sets the next duration. Curve edits are smoothed.

Duration and curve are saved globally. Mid-transition audio and the held
position are temporary; reopening recalls the destination. Saved-bank format
remains 17, as in test.3; no additional migration is needed from test.3.
