# s3g Thru 0.16.1

This maintenance release improves performance-capture continuity and realtime
CPU headroom.

- Notes already held when Record is pressed are captured at the new loop origin
  or the current overdub position, including their current pressure depth.
- Dormant playback engines no longer run full DSP every audio block. Required
  sample-lane transitions still complete while a voice is inactive.
- Clip note attacks use sorted range lookup, and pressure/parameter curves use
  a per-lane index instead of rescanning every stored gesture point each block.
- The held-before-record case is covered by the native regression suite.

Desktop ABI, DSP, ASan/UBSan, UI, cross-build, and package checks remain
automated. Move hardware remains the authority for final realtime acceptance.
