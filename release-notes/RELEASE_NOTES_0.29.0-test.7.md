# s3g Thru 0.29.0-test.7

Local testing candidate. Pattern Fade can now vary transition lengths automatically.

- Choose FIX for the existing fixed duration, or RAND for a random duration between LOW and HIGH, up to 30 seconds. The range applies globally to clip selection and chain playback.
- Each transition chooses its duration once, when it launches. Hold, manual crossfading, resuming, and timing edits keep that choice; edits set up the next change.
- The Pattern Fade display uses a smaller progress strip and more space between control names and values. The strip retains the source and destination clip numbers.
- NOW shows the chosen duration in Random mode. Knobs 5 and 6 set LOW and HIGH; knob 7 selects MODE. Those range positions remain reserved in Fixed mode.
- Find the outgoing pattern buffer count under FADE BUF MISSES on the first Shift + Menu System Report page.

Touch knob 3 / XFADE to hold a blend and move it manually. Click the main encoder to resume, or turn DONE clockwise to finish at the destination.
