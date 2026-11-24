# s3g Thru 0.34.0

Browser composition, live sample processing, and clearer performance workflows.

- **Live browser editor:**

  - **Live control:** adjust the active clip using Thru’s familiar menu pages. Move and the browser share parameter values, editing focus, clip selection and transport. Waveform POS/FIELD editing and a Layers XY field give direct visual control. Slider values update throughout a drag.
  - **Clip composition:** edit notes in a scale-aware piano roll, draw parameter breakpoints through the menu controls, and reshape passages with Range tools. Clip naming, copy/paste, duplication, resizing, separate clears and labelled Undo/Redo support quick revisions. Touch overdubs retain earlier takes outside the edited intervals.
  - **Sequence editing:** view all four lanes, select trigs and set engine, amp and supported effects locks through the same panels. Burst events retain their own inheritance; animated playheads follow native direction, path and rate.
  - **Portable clip collections:** import and export `.thrupack` files with selected clips, optional WAVs and chains. Composition JSON, a schema and command-line tools also support collections made in other editors or generated externally. Complete backups continue to use `.thru` projects.
  - **Connection fixes:** loaded samples and Single clips with remembered multi-part settings now link correctly. Clip changes recover automatically, and delayed edits cannot reach a different clip, synth patch or Engine List entry.
  - **Editing safeguards:** an eight-note overlap limit, pitch-preserving note drags, automation edits constrained to their audible take intervals, and clear feedback for rejected input routes.

- **Live Memory:** Grains, Weave, Glitch, Cutups, Pulsar and Texture can read continuous stereo input or another part, including a synth feeding a sample engine. Freeze the history or save its window as a sample. Split and Quad overviews show the receiving part’s live waveform.
- **Travel Envelopes and LFO assignment:** Grains and Weave gain per-note attack, decay, sustain and release travel rates, independent of pitch and amplitude. Assign an LFO directly from a supported parameter on Move or in the browser.
- **Page Actions and naming:** main-encoder clicks open contextual tools for the current page without starting performance recording. One naming editor labels clips, chains, Bank pads, lanes and project exports. Sample file tools rename managed recordings or save named library copies.
- **Clearer navigation and playback feedback:** clip-bank changes show the bank and slot range; clip recalls identify their mode. Copy handles contextual copying rather than switching layouts. Layers recordings update the OLED and pad LEDs, and Cutups now responds to L1–L4 lane levels, including overlapping cuts and live changes.
- **Stability repairs and documentation:** more reliable mode recall and correct File waveform restoration. Lightweight Manager polling reduces repeated full-state reads while idle. The manual, screenshots and in-device help cover the new workflows.

Update through Schwung Manager using `https://github.com/s3g/s3g-thru`, or install this release’s `s3g-thru-module.tar.gz`. Open the browser panel from Thru’s module page using **Open web UI: Tool**, or use the [manual’s direct editor link](https://s3g.github.io/s3g-thru/composer.html#open). Reopen Thru and hard-refresh any existing editor tab after updating.

Existing clips, samples and MIDI parameter identities are retained. Export a [project backup](https://s3g.github.io/s3g-thru/projects.html) before updating; projects saved with 0.34.0 require 0.34.0 or newer. Live Memory audio is temporary until saved as a sample.
