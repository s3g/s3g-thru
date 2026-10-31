# s3g Thru 0.29.0-test.2

Local testing build, with all features from 0.29.0-test.1 and the following repairs.

- **Readable recording source:** SRC uses IN, MIX, PT A, PT B, PT C, and PT D,
  so the part letter fits on screen.
- **Full lane grabs:** while holding Capture, one available lane pad selects
  that lane fully even at light pressure. Multiple held lane pads choose a
  pressure-weighted stack position. Release returns to the underlying path or
  held position. The transition remains smoothed.
- **Existing events follow:** active long grains, short grains, and Motion
  packets crossfade to the selected lane without restarting. Events retain
  their natural lane selection underneath the temporary grab.
- **Stronger momentary effects:** a press starts at 75% rather than 35%.
  Pressure reaches full strength sooner and stays effective at a light touch.
  DARK closes further; CRSH has deeper bit/rate reduction; ECHO and SPCE reach
  higher wet amounts. These punches preserve stronger existing effect settings.
  In multi-instrument modes, ECHO/SPCE still use the focused part's sends and
  require audible shared return levels.
- **Cue and Stack Start agree:** recalling a cue updates the Stack Start value
  to its held lane/blend. Turn Stack Start, or send its MIDI control, to move
  that held position while preserving scan hold. The saved cue stays unchanged;
  recall it again to restore its original position. Loop resumes the scan.

Shift + Capture remains Schwung Skipback. Open FX Sends with
**Capture + Menu → FX SENDS → main encoder click**.

Bank format remains 16, compatible with test.1. Keep a device-data backup if you
need to return to 0.28.0, which cannot read the newer bank format.
