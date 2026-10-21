# s3g Thru 0.24.1

## Stack Hold retrigger recall

- Retains each physical pad's exact frozen lane or between-lane crossfade after
  its note is released while Stack Hold remains active.
- Restores that position when the same pad is pressed again, even after the
  previous voice's release tail has ended and its voice slot has been reused.
- Keeps retained positions independent for all 32 physical pads and gives a
  newly pressed pad its own frozen starting position.
- Clears the retained pad positions when Loop returns to Run, allowing normal
  independent stack scanning to continue from the audible held positions.

## Recording and compatibility

- Uses the retained live position when a retrigger is captured into performance
  data, so recording during Stack Hold cannot fall back to the start lane.
- Keeps state schema version 6, clip-bank format 4, parameter IDs, stored clips,
  and the 0.24.0 Stack Hold data format unchanged.
- Native and sanitizer regressions cover a complete note release, voice cleanup,
  same-pad retrigger at the exact held crossfade, and normal motion after Run.
