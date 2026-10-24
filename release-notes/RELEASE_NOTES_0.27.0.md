# s3g Thru 0.27.0

This release adds Wavesets and a complete tape stage, while making live
playback fuller, smoother, and more dependable across all nine engines.

## Wavesets engine

- Adds a ninth playback engine based on the s3g-dsp Wavesets instrument.
  Rising zero-crossing cycles become pitched groups that can repeat, stride,
  stretch, preserve time, or hold their source position.
- Adds Forward, Reverse, Pendulum, and repeatable Shuffle ordering, plus 12
  real-time processes: Repeat, Omit, Replace, Envelope, Multiply, Average,
  Interpolate, Fractal, Harmonic, Group Reverse, Cycle Reverse, and Telescope.
- Adds selectable crossing Detail from Raw through 250 Hz. Analysis uses the
  filtered timing reference while playback retains the original recording.
- Keeps every note's group, pitch, order, and lane-stack path independent.
  Quiet lead-ins and difficult source material now fall back to a temporary
  audible cycle while bounded analysis continues through the active field.
- Adds a Wavesets-specific group rail and source-accurate cursor telemetry.
  Reverse, Pendulum, and Shuffle are visible as slot movement; backward
  processes, multi-source processes, pending Order changes, and overlap-add
  transitions no longer masquerade as one generic forward cursor.

## Tape effect

- Adds a clip-local Tape page after Stereo / Rotary, shared by every engine.
- Tape Format moves from 2-inch mastering tape through 1/8-inch cassette
  character. Record level, Age, Crinkle, Mechanics, Hiss, and Mix shape the
  medium without adding phonograph-style clicks or surface noise.
- Transport Inertia turns the Loop button's Stack Hold into a tape stop and
  restart gesture. At partial Mix, the dry path continues underneath it.
- Every Tape control can be captured in a performance loop and stored per
  clip. Parameter changes are smoothed to avoid read-head jumps and zippering.

## Playback quality and gain staging

- Adds a stereo-linked program compressor with gentle 2:1 control and 3 dB of
  makeup at normal Level, followed by a stereo-linked −1 dBFS lookahead safety
  limiter. Output is fuller without hard-clipping stacked voices or effects.
- Improves polyphony compensation, capture limiting, sample-rate conversion,
  and transposed playback interpolation. Imported and recorded samples retain
  more detail while dense stacks remain controlled.
- Makes cue storage and recall click-safe by snapping new cues to a nearby
  stereo zero crossing and fading an active reader around a cue jump.
- Smooths Sample loops, same-note retriggers, and voice stealing during long
  releases. A replacement note now waits for the stolen tail's short fade
  instead of hard-resetting an audible reader.
- Extends seam, transition, and live-parameter safeguards throughout the
  playback engines and Effects, Stereo / Rotary, and Tape stages.
- Adds coherent audio-block control snapshots, 10 ms Join smoothing, longer
  effect and wet/dry glides, and queued transitions that cannot restart midway
  through rapid encoder or recorded-automation changes.
- Makes Motion Travel and Jitter continuous by keeping Zigzag's three temporal
  sections stable while their targets move. Seed changes now fade before the
  active path is re-hashed instead of moving its reader in one sample.
- Queues Wavesets Group, Repeat, Stride, Advance, Order, Process, Depth,
  Detail, and Join changes until the next rendered zero crossing or low-level
  point, with the next measured cycle boundary as a guaranteed fallback.
  A short raised-cosine constant-overlap-add window joins the old and new
  readers, including filtered boundaries that remain nonzero in stereo. Every
  Stretch source-group handoff now receives that transition; very short groups
  retain an audible-safe minimum window and can restart an in-flight overlap
  without exposing a raw splice.
- Adds regression sweeps for shared Sample controls, Motion, Lanes, engine
  switching, Wavesets process/order combinations, effects, retriggering, and
  voice stealing. Intentional Bits, Rate-reduction, and Glitch edges remain
  part of those effects rather than being filtered from the final output.

## Interface and manual

- Uses one consistent waveform-page encoder layout for all engines: Position,
  Field, Speed, Stack, Scan Rate, Join, engine Mode, and Level.
- Slows continuous encoder adjustment by 50% for finer control while keeping
  one-shot actions responsive to the first turn.
- Updates the illustrated manual for all nine engines, the Tape signal path,
  gain staging, waveform controls, page numbering, and click-safe behavior.
  Wavesets has its own complete parameter and process reference.
- Replaces the crowded Shift+Menu title report with two readable pages. The
  System Report gives render peak, deadline misses, current limiter reduction,
  and maximum limiter reduction separate rows; the second page retains the
  shortcut reminder. Left/Right, Up/Down, or the main encoder changes page.

## Compatibility and installation

- Existing 0.26.x settings, clips, lane assignments, recordings, and
  performance data load forward with defaults for the new controls. Data saved
  by 0.27.0 uses the expanded state and clip-bank formats and should be opened
  with s3g Thru 0.27.0 or newer.
- The release remains compatible with Schwung 1.7.3 or newer and Ableton Move's
  system libraries.
- In Schwung Manager, choose **Modules → Install Custom Module → GitHub URL**
  and enter `https://github.com/s3g/s3g-thru`. The manager resolves this
  version through `release.json` and downloads `s3g-thru-module.tar.gz` from
  the `v0.27.0` GitHub release.
