# s3g Thru 0.34.1

A corrective update for browser sample editing and saved marker recall, with new sample-loading and slice tools in the live panel.

- **Browser fixes:** saved WAV markers and labels return after reopening Thru, including File-source playback. The marker popup opens with the correct boundary map; source selection, detected-transient boundaries and draft Undo now work consistently. Preview and Stop no longer mark the project as edited or interfere with the next audition.
- **Browser sample tools:**

  - Browse Move’s sample library or upload a WAV from the computer, choose a destination folder and load any of the four lanes. Audition samples before loading.
  - Keep readable filenames, reuse identical uploads and number name collisions.
  - View and edit embedded WAV markers, label slices and audition the selected slice with short edge fades. Save a marked WAV copy while retaining the original audio and clip performance.
- **Cutups and Texture:** File, Edited and Auto region sources use authored boundaries. Texture gains a Source page. Existing defaults are retained; new source choices affect newly launched cuts or events while those already sounding finish normally.
- **Manual and device help:** updated sample-loading, marker-editing and engine-source instructions, with current screenshots.

Update through Schwung Manager using the catalog entry or `https://github.com/s3g/s3g-thru`, or install this release’s `s3g-thru-module.tar.gz`. Reopen Thru and hard-refresh the browser editor after updating.

Existing projects and MIDI parameter identities remain supported. Projects saved with 0.34.1 require 0.34.1 or newer. The [Projects guide](https://s3g.github.io/s3g-thru/projects.html) explains complete backup and restore.
