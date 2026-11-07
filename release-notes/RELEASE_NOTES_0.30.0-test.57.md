# s3g Thru 0.30.0-test.57

Local testing build; not a published release.

- Sample and Slices lane activity bars now identify the actual source lane
  instead of following a stack scan that does not affect these engines.
- Single mode follows the most recently triggered held voice. Individual
  playhead markers keep showing their own lanes during polyphonic playing.
- Split, Shared Dual and Quad combine activity from the actual readers,
  including Capture lane changes that differ from a note’s initial lane.
- Manual and in-device help explain discrete lane selection and the indicators.

Includes test.56’s external MIDI slice mapping. Playback, controls, set formats,
controller bundles and the published installer retain their existing behavior.
