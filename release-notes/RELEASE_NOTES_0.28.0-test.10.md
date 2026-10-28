# s3g Thru 0.28.0 — local testing build 10

This build includes the reverb, effect ordering, Cutups and two-instrument Split
features from testing build 9, with fixes for silent instruments in Split.

- The hardware Mute button and Split Setup MUTE readout now stay synchronized.
  A muted part shows MUT in the waveform header and a persistent unmute hint.
  Mute is remembered when leaving Split; focus the part and press Mute to restore it.
- Copying a sound between parts preserves the destination’s mute state.
- Live changes to Blend, part levels, sends and crossfade curve take precedence
  over recorded mixer automation until playback is restarted or another clip is
  launched. A live correction no longer silently reverts after half a second.
  Overdub records these movements when you want to keep them.
- Loading an already-previewed sample into a different lane now uses the chosen
  destination, including when moving from A to B.

Saved-set format, MIDI assignments and Single mode are unchanged. This is a local
test package for Move; publication remains pending hardware testing.
