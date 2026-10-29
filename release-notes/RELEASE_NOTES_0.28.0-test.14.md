# s3g Thru 0.28.0 — local testing build 14

Five additions make navigation, Split mixing, recording, and audition easier:

- Hold Capture and press Menu for the contextual Pages picker. Turn the main
  encoder or use Up/Down, then click to open a page. Back cancels. A and B keep
  their remembered parameter pages.
- Hold Delete and press Play for Panic. It stops performance playback, closes
  an active performance take, and releases source voices, latches, Rolls, and
  browser preview with short fades. Samples and recorded data are retained.
  Existing effect tails decay naturally; audio-lane recording is unaffected.
- Split waveform headings show source-voice counts and small post-Blend
  activity meters. A/B Mix Status in Pages gives larger meters and explicit
  Muted, Level 0, Faded Out, No Audio, Idle, or Active readouts.
- Saving and Saved notices track completion of capture archives and the latest
  set metadata. Save failures remain visible while the retained data is retried.
- In the sample browser, knob 7 selects Part or Direct audition; knob 8 adjusts
  Preview Level. Part preserves the existing route. Direct bypasses engines,
  effects, part levels, mute, and Blend, while retaining the program compressor
  and final limiter. It uses the main stereo output. Route changes crossfade.

The preview route is remembered globally; older saved settings default to Part.
No existing MIDI assignments or clip-bank binary formats are changed.
Includes the reverb, effect ordering, Cutups, Split, and waveform improvements
from testing builds 1–13. Manual illustrations and shortcuts are updated.

Local package for testing on Move; public release remains pending hardware tests.
