# s3g Thru 0.30.0-test.58

Local testing build; not a published release.

- External MIDI follows VOICE = Poly, Mono or Legato regardless of Move’s
  PADS layout. Last-note priority, sustain, recording notes already held and
  clip playback follow the same rule.
- Physical Slices, Cues and Rolls pads retain their separate behavior and
  are not absorbed into the external keyboard’s mono voice.
- Sample and Slices reserve the unused RATE position on the waveform and
  Engine pages. Move shows the usual hollow reserved box on the Engine page;
  adaptive E16 pages reserve that position independently for each part.
- Generic CC preserves reserved and unavailable cells, keeping later controls
  aligned with the corresponding physical knobs.
- Updated MIDI, playing, interface and engine guides, plus in-device help.

Includes test.57’s accurate lane indicators and test.56’s MIDI slice mapping.
Set formats, MIDI parameter IDs and controller scene/script files are unchanged.
