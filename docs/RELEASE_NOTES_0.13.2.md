# s3g Thru 0.13.2

This patch restores responsive sequencer and transport LEDs under continuous
DSP status updates.

- Step selection is orange, a held Step is red, and the running Step is cyan.
- The Play button changes brightness immediately when the sequencer starts or
  stops.
- LED updates are deduplicated and prioritized instead of restarting a long
  queue whenever pad, voice, or sequencer state changes.
- A Step press retains its absolute 1–64 identity until release, even if a DSP
  page-state refresh arrives during the gesture.

All audio, pattern, sample, and persistence behavior remains compatible with
0.13.1.
