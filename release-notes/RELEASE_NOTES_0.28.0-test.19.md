# s3g Thru 0.28.0 — local testing build 19

- Fixed Shared Dual waveform zoom while playing. The four source rows now follow
  the focused A or B instrument’s Position and Field controls, even when both
  instruments are sounding or a recorded performance is playing.
- A wide Field on the other side, existing readers, and release tails no longer
  force the shared view to zoom out. Capture follows the newly focused side’s
  Position/Field settings.
- Waveforms, playheads, outgoing readers, and stack markers use the same crop.
  Readers outside it are hidden while their audio continues. Zoom reuses the
  cached source waveforms.
- Updated manual instructions and Shared Dual screen images. Single and Split
  retain their existing waveform behavior.

Includes testing build 18. Playback algorithms, recorded automation, controls,
and saved-set formats are unchanged. Local package for testing on Move.
