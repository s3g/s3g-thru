# s3g Thru 0.29.0-test.3

Local testing build. Includes the repairs and features from test.1 and test.2.

## Editable stack paths

On **Stack Path**, choose **DRAW**, then click the main encoder to open
**Path Editor**. It is also available as the next parameter page and through
Capture + Menu while DRAW is selected.

- **Knob 1 / PT:** select one of four to eight breakpoints.
- **Knob 2 / TIME:** move its time within the cycle. The first point stays at 0%.
- **Knob 3 / LANE:** choose its lane position, including blends.
- **Knob 4 / CURV:** bend travel from this point to the next. Positive curves
  accelerate; negative curves slow down; zero is linear.
- **Knob 5 / PNTS:** add or remove points. Adding splits the selected segment
  without changing the curve. Removing the first point removes the last instead.
- **Knobs 6–7:** reserved.
- **Shift + knob 8 clockwise / RSET:** restore a four-point triangle.
- **Back:** return to Stack Path. The curved-arrow Undo key restores an edit.

Repeat a lane value at adjacent points for a pause. Place it again later for
repeated visits. Unequal time intervals give uneven travel. The final segment
returns to the first point, keeping the loop continuous. Scan Rate sets the
cycle speed; OFST and SLEW remain available. Named-path SHAP, SKEW, and CURV
positions are reserved while DRAW is selected.

Each voice keeps its independent phase when the path changes. Live edits use
the existing lane-transition smoothing, including active grains and packets.
The editor is available for Motion, Lanes, Grains, Glitch, Weave, and Wavesets;
Glitch's buffer refresh still determines when new source material is heard.

Single, Split, and Shared Dual store independent paths per clip and per part.
Split shows physical lane numbers for the owning pair. Quad, Cutups, Sample,
and Slices keep their existing controls. Named paths retain their behavior;
switching away from DRAW preserves the authored points.

Record captures edits as performance data. Overdub and Undo Take include path
changes. Existing cues, Loop holds, and Capture lane grabs retain their roles.

## Saved data

This build writes bank format 17 and reads previous banks, including format 16
from test.2. **Back up Thru's device data before installing if you need to return
to test.2 or an earlier build.** Earlier builds cannot read the new bank format.
