# s3g Thru 0.29.0-test.12

Local testing candidate, including the previous 0.29.0 additions.

- Chain edits now use the curved-arrow safety Undo, including deletion, insertion, movement, repeats, fade settings, paste and transition takes. Short runs of dial changes are grouped.
- Chain Tools copies and pastes whole chains or individual entries, including transition settings. Patterns and their audio remain shared references.
- Browse a saved chain without stopping the performing chain. Choose an entry and use START HERE, or Shift + NEXT, to queue it at the next loop boundary. QUEUE CANCEL removes the explicit launch.
- Pattern Fade: Shift + knob 8 / FROM returns smoothly to the source. The restored performance keeps running and automatic chaining turns off.
- Chain Fade and Entry Fade show effective inherited timing and its source, or the active saved transition take.
- Transition Take explicitly arms with main encoder click or REC. First XFADE movement starts capture; DONE finishes and saves. Holds, reversals and curve changes replay with that destination entry. USE switches between TAKE and TIMED without deleting the take. Cancelling or returning to the source preserves the previous take; Undo restores a replaced or cleared take.
- Takes support up to two minutes with bounded, allocation-free gesture capture. Background set snapshots retain the maximum 16 KiB copy per callback; disk writes stay off the audio callback.

Open Chain Tools and Transition Take after Entry Fade or through Capture + Menu → Pages. The ordinary Record button retains performance-data recording.

This candidate writes set format 19 and reads older sets, including format 18. Earlier builds cannot read format 19. Export your current set before testing if rollback may be needed. This build has not been published or installed on hardware.
