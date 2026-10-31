# s3g Thru 0.29.0-test.8

Local testing candidate. Chain now stores 16 independent arrangements, with up to 64 entries each.

- Your existing chain opens as CHAIN 01 and keeps following the global Pattern Fade settings. Patterns, sample assignments, free playing, Hold and manual crossfading retain their existing roles.
- Left/Right page through four groups of 16 entries. The screen highlights the editing selection separately from the playing and queued entries.
- The first five Chain parameter controls retain their positions. BANK on knob 6 selects a saved chain; END on knob 7 chooses LOOP or STAY; NEXT on knob 8 requests advancement.
- Set REPT to WAIT for an entry that continues until NEXT. Selecting a different chain keeps the current pattern playing and turns automatic chaining off until enabled.
- Chain Fade can follow the global settings or save its own Fixed/Random defaults. Entry Fade can inherit those defaults or specify CUT, Fixed or Random with its own duration/range and curve, up to 30 seconds.
- Transitions belong to the destination entry. Each random fade chooses once when it launches; Hold and manual movement retain the choice. Live edits to saved transition settings prepare the next launch.
- All chains and their settings save with the set. Background snapshots retain the existing maximum of 16 KiB per audio callback.

Open Chain with Shift + Play twice from the waveform view. Menu opens its parameter controls; the following two pages are Chain Fade and Entry Fade.

The set format advances to 18. Existing formats through 17 remain readable. Export your current set before testing; restoring an earlier firmware candidate requires a compatible set backup.
