# s3g Thru 0.29.0-test.15

Local testing candidate, including Prism, Morph, and Texture from test.14.

- Added contextual Copy / Shift+Copy shortcuts for clips, chains, and entries. Taps act on release; held Step gestures target a slot without launching it or triggering a whole-chain action.
- Clip copy uses a temporary snapshot of the complete performance, takes, per-part programs, paths, cues, and source assignments. Subsequent source edits do not change the clipboard. Audio files remain shared references. Paste supports Undo; replacing the active clip requires stopped playback.
- PERF LOOP is now visibly shared across all parts. Replace/Overdub, rate, timing, tempo, launch and swing operate on the combined performance. Changing A/B/C/D focus keeps the shared page open.
- Moved per-part Pressure to Pads & Scale, knob 6, and reserved its cell in Quad. Performance Loop knobs 7–8 are reserved.
- Removed part labels from shared take and arrangement pages. Updated the shortcut reference, performance chapter, and screen diagrams.

Set format remains 20. Earlier readable set formats and all existing parameter IDs remain unchanged. This candidate is for local Move testing and has not been published.
