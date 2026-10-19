# s3g Thru 0.18.1

This maintenance release fixes delayed, flat, or stale waveform drawings after
loading samples and changing clip sources.

- Library-sample and prepared clip-bank waveforms are analyzed on the existing
  background worker and published with the completed audio instead of starting
  a second slow UI-driven scan.
- Live-recording and edited-sample scans remain outside the audio callback, but
  advance on every UI poll with a larger bounded work unit.
- Waveform display gain is normalized independently per lane. Quiet samples are
  now legible without changing their playback gain or stored audio.
- Replacing or clearing a source cancels an older in-progress scan, preventing
  stale sample data from overwriting the new drawing.
