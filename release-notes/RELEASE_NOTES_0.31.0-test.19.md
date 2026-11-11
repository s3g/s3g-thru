# s3g Thru 0.31.0-test.19

Local Sequence display test build. It is not a GitHub or Schwung catalog release.

- The Sequence overview now distinguishes the step's start position, each sounding voice's start and its moving sample reader. The current step's POS is shown above the waveform; the newest voice's start stays below it while the reader moves or loops. This avoids a misleading line across the waveform after a loop wraps.
- TRIG and BURST pages have more space between their controls. BURST uses a smaller timing graphic, and TRIG no longer repeats its pitch in a bottom report.
- Tests confirm that Sample audio starts at a locked 75% POS on the first trig, a repeated trig and a restart. The remaining mismatch was in the display: a short loop can move past its start before the OLED refreshes.

All changes from 0.31.0-test.18 remain included. Listen and inspect the OLED on Move before release.
