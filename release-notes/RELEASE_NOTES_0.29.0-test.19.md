# s3g Thru 0.29.0-test.19 — rendering headroom and timing reports

Local testing build following test.18.

- **Prism / Morph / Imprint:** prepare source-frame readers once for each synthesis frame, reuse constant pitch gain, and skip source reads that have no audible lane contribution. Spectral resolution, hop size, controls, and voice counts are preserved.
- **Formant:** cache grain-window reciprocals and subharmonic clock increments, and skip sample reads outside a grain’s usable window. Grain limits, smoothing, tuning, roughness, and source behavior are preserved.
- **Render Timing 3/4:** CPU PEAK, WALL PEAK, WAIT PEAK, and CPU MISSES help distinguish heavy DSP from descheduling or blocking during a render call. Existing System Report and Callback Work pages retain their readings; Shortcuts is now 4/4.
- **Fresh diagnostic tests:** Shift+Delete resets peaks and miss counters while a report view is open. It preserves samples, clips and transport. Outside report views the existing Shift+Delete action still applies.
- **Precise miss counting:** counters compare full-resolution timing against the block period. Rounding a displayed microsecond upward no longer produces a false WORK MISS just below the deadline.
- **Manual:** updated report images, metric descriptions, reset shortcut and interpretation.

## Compatibility and testing

Set format remains **23**, with the same 205 stored parameters as test.18. No additional state upgrade is required for test.18 sets; older formats remain readable.

The observed 3,304 µs render peak exceeds the roughly 2,902 µs block period. This build reduces avoidable DSP work and adds information to investigate sporadic peaks; it does not establish that the reported hardware spike is eliminated.

After installing on Move, open Shift+Menu, press Shift+Delete in the report, and test the same sounds and note count. If misses rise, record the engine, active notes, effects, CPU PEAK, WALL PEAK, WAIT PEAK and CPU MISSES. Watch whether this happens during steady playing, interaction, source loading or pattern changes. Host profiling does not establish device CPU headroom.
