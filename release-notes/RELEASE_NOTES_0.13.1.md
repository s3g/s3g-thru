# s3g Thru 0.13.1

This patch makes 64-Step navigation unambiguous on Move.

- Left/Right now select Step pages 1–4 from the waveform or parameter views.
- Shift+Up/Down remains available and now selects Step pages consistently from
  both views instead of navigating parameter pages while Menu is open.
- The waveform header permanently shows the active Step page as `P1`–`P4`.

All sampling, sequencing, persistence, and compatibility changes from 0.13.0
remain unchanged.
