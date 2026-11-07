# s3g Thru 0.30.0-test.59 — Slice Edit

Local testing build; not published.

- Open Slice Source and click the main encoder for Slice Edit. Audition slices
  with pads, latch with Shift + pad, move starts/ends, zoom, add cuts and merge
  neighbouring slices with Delete. Curved Undo reverses the latest edit.
- SSRC adds File, Edited and Auto. Standard WAV cue markers and labels are
  imported on load. Maps contain at most 64 slices.
- PRER sets a lane's pre-roll from 0–30 ms. Shift gives 0.1 ms adjustment;
  new transient maps default to 3 ms, while WAV markers default to zero.
- Existing held notes retain their bounds while edits crossfade in the editor's
  audition. Waveform zoom is analysed in the background.
- Edits and pre-roll are stored per clip and source lane in clip-bank format 36.
  Older banks load, including their former transient lead-in. Back up the set
  before testing; this bank format cannot be opened by older firmware.
- Includes test.58's external Mono/Legato fixes and reserved Sample/Slices RATE.

No release, public installer change or controller scene reimport is required.
