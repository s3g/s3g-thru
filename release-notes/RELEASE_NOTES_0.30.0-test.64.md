# s3g Thru 0.30.0-test.64 — Project sample recall

Local testing build; not published.

- Fixes missing lane and Bank samples after importing a project with a longer
  save history than the destination. The archive's audio was intact; reopening
  could select the wrong set of sample references.
- Automatically recovers earlier affected imports when their installed sample
  copies are intact. Install this build, fully exit and reopen s3g Thru. A new
  export or import is not required for those projects.
- Retains the destination's MIDI configuration during import and recovery.
- Adds coverage for all five modes, normal sample loading, stereo input
  recordings, Bank pads, deleted original sources and repeated reopening.
- Existing .thru files remain compatible. Clip and project formats, controller
  scenes, MIDI mappings and audio processing are unchanged.

The published GitHub installer remains 0.29.1. Keep the original .thru backup
until you have checked the restored lanes and filled Bank pads on Move.
