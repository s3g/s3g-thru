# s3g Thru 0.28.0 — local testing build 13

- The regular four-lane instrument now uses the same Position/Field waveform
  zoom as Split. The active source window fills the waveform width, replacing
  the two boundary ticks. All playheads, outgoing readers and source markers
  use the zoomed coordinates.
- The four-lane view includes every active voice. When live and recorded voices
  read different regions, the view widens to keep their readers visible.
- More precise window bounds keep narrow zooms and playheads aligned.
- Fixed two unpainted waveform columns in both Single and Split, including the
  black vertical seam near the middle of the display.

Updated manual screenshots and waveform guidance. Zoom uses the cached overview;
very narrow windows have limited fine detail. Playback, recordings, MIDI maps and
saved-set formats are unchanged. This is a local package for testing on Move.
