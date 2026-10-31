# s3g Thru 0.29.0-test.1

Local testing build. This version has not been published to the Schwung catalog.

## Capture performance layer

Hold **Capture** to reveal temporary controls on the bottom eight pads. From
left to right: **L1, L2, L3, L4, DARK, CRSH, ECHO, SPCE**. The display shows the
same eight controls in two rows. The other 24 pads keep their normal function.

- Lane pads pull active voices toward a lane; pressure controls the strength.
  Press multiple lane pads to blend their destinations. Release them to return
  to the path underneath. The scan continues unless Loop was already holding it.
- Split offers the focused part's two lanes; Shared Dual offers all four.
  Quad has no lane rides. Empty or unavailable lanes are unlit.
- DARK momentarily filters the sound; CRSH adds bit/rate reduction. ECHO and
  SPCE increase delay and reverb. In multi modes they change only the focused
  part's sends, so its shared return levels must be audible.
- Controls return smoothly to their current settings or recorded automation.
  Notes already held keep their ordinary pressure and release behavior.
- Capture taps still switch editing focus. Capture + Menu opens Pages;
  Capture + encoder 8 retains Blend in the dual modes.
- Record captures these gestures as performance data, including gestures held
  when recording starts. Overdub Take Undo also removes their recorded data.
- Shift + Capture is reserved for Schwung Skipback. Open FX Sends with
  **Capture + Menu → FX SENDS → main encoder click**.

## Cues remember lane blends

Shift + cue pad stores the source position and the owning part's most recent
playhead lane/blend. Recall seeks to the cue and holds that blend; **Loop**
resumes stack movement. Delete + cue pad clears both. Older cues retain their
original time-only behavior.

## Record a part into a lane

On **Mix / Rec**, SRC now offers **INPUT**, **MAIN MIX**, and the parts available
in the current mode. Choose a destination with Track, select a part as SRC, and
use Shift + Record to start and stop. The part is captured after its private
processing, before mute, level, and A/B Blend. Shared Tape and send returns are
excluded. The mixer stays as you set it.

The old lane remains available during capture, including Shared Dual and
self-capture. After stopping, the completed take is loaded into the destination
with the usual sample transition and archived as a new stereo WAV. Wait for
publication/saving before another capture. Stop sample preview before starting
a part capture. Takes are free length, up to the existing 12-second lane limit.

## Other fixes and testing

- Quad Rolls steal only within their own two-slot group.
- Cue and performance-data saves use bank format 16 and read older banks.
  Back up the device's Thru user data before testing if you need to return to
  0.28.0: the older release cannot read banks saved by this testing build.
- Check audible transitions, pressure response, part routing, and the System
  Report on Move. Automated host tests do not establish device CPU headroom.
