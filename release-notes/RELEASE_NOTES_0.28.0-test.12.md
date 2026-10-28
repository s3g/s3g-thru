# s3g Thru 0.28.0 — local testing build 12

- Split editing focus is shown by inverted lane numbers: black 1–2 on white
  backgrounds for A, or black 3–4 on white for B. The A/B heading stays fixed.
- Split waveforms zoom independently to each instrument’s Position/Field window.
  Each playhead and outgoing reader is placed within its own zoomed window.
  Multiple sounding voices can widen the view to keep their readers visible.
- Focus changes leave the waveform pixels in place. Moving cursors and changed
  waveform data update only the pixels they change, without blanking the view.
- Region markers stay attached to the correct source lane during focus changes.

Includes the fast editor switching and mute/mixer repairs from earlier test
builds. Playback, recording, MIDI mappings and saved-set formats are unchanged.
The zoom uses the existing waveform overview; narrow windows have less detail.
This is a local package for further testing on Move.
