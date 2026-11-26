# s3g Thru 0.34.2

Numbered slices for Sequence and simpler live browser editing.

- **Sequence slice selection:** when a lane uses the Slices engine, choose a numbered **SLIC** on each trig or Burst event. Use WAV markers, edited boundaries, detected transients or equal regions. The highlighted region and slice name show what will play; NOTE and duration remain independent. Clear a slice lock to return to position-based slice selection.
- **Live browser editor:**

  - Choose Single, Split, Shared Dual, Quad, Bank, Layers or Sequence from **MODE** in the header. Changes made on Move update the browser too. A mode change stops playback; finish recording or a clip transition first.
  - Click an empty step to add and select it. Click a different lit step to select it for editing, then click the selected step again to toggle it. Its locks remain intact; the separate Trigger Off button is removed.
  - Set numbered slices from the trig controls or waveform, with slice labels, highlighted boundaries and Burst overrides. Missing slices are identified and remain silent.
- **Build reliability:** corrected a diagnostic-message warning that blocked automated GitHub builds.
- **Manual and help:** updated slice workflows, browser controls and MIDI reference.

Update through Schwung Manager using the catalog entry or `https://github.com/s3g/s3g-thru`, or install this release’s `s3g-thru-module.tar.gz`. Reopen Thru and hard-refresh the browser editor after updating.

Existing projects remain supported. Use 0.34.2 or newer when sharing clips with numbered slice locks. See the [Projects guide](https://s3g.github.io/s3g-thru/projects.html) for backup and restore.
