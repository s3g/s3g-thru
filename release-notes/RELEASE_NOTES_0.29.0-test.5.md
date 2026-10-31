# s3g Thru 0.29.0-test.5

Local testing candidate. Includes the previous 0.29.0 testing updates.

## Touch-controlled manual pattern crossfade

On **Pattern Fade**, knob 3 is now **XFADE**:

1. Touch knob 3 to engage Hold at the current position without moving the blend.
2. Turn it clockwise toward the destination or counterclockwise toward the source.
3. Lift your finger to leave the blend held. Both performance loops keep playing.
4. Click the main encoder, or turn HOLD off with knob 4, to resume the timed fade.

Touch knob 3 before choosing the destination to arm a manual change starting at
0%, including when TIME is CUT. The page says **HOLD: SELECT PATTERN**. Clicking
the main encoder again cancels this arm. A normal selection with Hold off keeps
the existing CUT/timed behavior.

While Hold is on, 0% and 100% retain both running patterns: reaching an endpoint
does not commit the change. You can move back and forth for as long as needed.
Turn **DONE** clockwise to commit the destination, or release Hold to let the
fade finish. With TIME=CUT, releasing Hold finishes with a short smooth fade.

Relative turns accumulate even before an audio block has rendered. Repeated
touch-on messages do not override a deliberate main-encoder resume; rotating
XFADE engages Hold again. Other knobs and pages keep their existing controls.

The saved-bank format remains 17. Duration and curve still save globally;
manual arming and blend position are temporary.
