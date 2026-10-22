# s3g Thru 0.26.0

This release expands s3g Thru as a playable instrument and live-performance
recorder, with predictable pitched pads, scale layouts, shaped note envelopes,
grouped stutter rolls, and a new stereo rotary effect.

## Playable pads and scales

- Grains and Weave now follow pad pitch in semitone steps, so higher notes play
  higher and lower notes play lower while retaining each engine's character.
- Adds Chromatic and 24 named scale layouts, with Root and Octave controls and
  a folded isomorphic pad layout. Available scales follow the collection used
  by Ableton Live and Push.
- Gives scale notes neutral grey LEDs and tonics bright white LEDs, clearly
  separating the scale map from the colored polyphonic playheads. The recalled
  scale is shown immediately when takeover opens.
- Adds Shift+pad note latching. Held notes can remain on until toggled off, or
  use the global Hold Off time for a scheduled release.
- Latched pads remain bright on their original octave, root, and scale mapping;
  they dim when the current pad map changes and brighten again when it returns.

## Note shaping and grouped rolls

- Adds a per-voice ADSR amplitude envelope with a graphical breakpoint page.
  Attack, Decay, Sustain, and Release can shape both short percussive playback
  and longer sustained material.
- Reworks Roll as a stereo stutter effect that captures the combined output of
  every active playhead instead of only one voice. Six pad divisions range from
  quarter notes through thirty-second notes, including triplet values.
- Up to eight grouped rolls can run without stealing source voices. Roll
  presses, lane position, and roll position are captured by performance-data
  recording and can be latched for hands-free playback.

## Stereo and rotary effects

- Adds a clip-local Stereo / Rotary page. X-PAN moves from the original stereo
  image through a centered mono sum to exchanged left and right channels.
- Adds a two-part rotary speaker with independent horn and drum motion. Rate,
  Depth, Spread, Ramp, Balance, Crossover, and Mix cover stopped rotors, gradual
  acceleration, frequency-dependent movement, and narrow or wide wet images.
- Places Mix at the lower-right control position used by the main Effects page.
- All Stereo / Rotary controls can be captured as performance-loop gestures
  and are saved with the clip.

## Compatibility and installation

- Existing 0.25.x recordings, clips, and banks continue to load with safe
  defaults for the new controls. Banks saved by 0.26.0 use the expanded format
  and require s3g Thru 0.26.0 or newer.
- The release remains compatible with Schwung 1.7.3 or newer and Ableton Move's
  system libraries.
- In Schwung Manager, choose **Modules → Install Custom Module → GitHub URL**
  and enter `https://github.com/s3g/s3g-thru`. The manager resolves this version
  through `release.json` and downloads `s3g-thru-module.tar.gz` from the
  `v0.26.0` GitHub release.
