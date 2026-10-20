# S-4 study and s3g Thru direction

The Torso Electronics S-4 is useful here as a study in performance behavior,
not as a layout to reproduce. Its official documentation describes four
parallel stereo tracks, a lockable live granular buffer, additive modulation,
relative performance mappings, momentary changes that return on release, and
scene recall with rules and chance.

Primary references:

- <https://torsoelectronics.com/s4>
- <https://docs.torsoelectronics.com/s4/what-is-the-s4/s4-overview/>
- <https://docs.torsoelectronics.com/s4/devices/device-reference/mosaic/>
- <https://docs.torsoelectronics.com/s4/perform-and-scene/perform-overview/>
- <https://docs.torsoelectronics.com/s4/modulation/modulation-overview/>
- <https://docs.torsoelectronics.com/s4/changelog/>

## Ideas that fit Thru

### 1. Lockable live history

Glitch 0.21 establishes a private rolling history per voice. A future Buffer
Lock gesture could stop replacement of that history while notes, stack paths,
and repeat patterns continue. Releasing the gesture would return to live
capture. This extends the instrument's present material rather than adding a
second sampling workflow.

### 2. Relative performance macros

Map each of eight encoders on a dedicated Perform page to one or more existing
parameters. Store a bipolar depth per destination and apply the encoder as an
offset from the clip's recalled value. One gesture can then shorten Field,
increase Glitch Override, darken Tone, and raise Delay Feedback without
destroying the underlying settings.

### 3. Temporary changes

Holding a step key or pad modifier while turning an encoder could create a
temporary offset. The value returns when the modifier is released. This is a
better fit for destructive-sounding effects than saving and manually undoing
every move.

### 4. Additive modulation recorders

The next evolution of the performance-data looper should allow a small number
of looping modulation lanes to add to base parameters instead of replacing
them. Rate multiplication, phase offset, depth, and destination would produce
polymetric motion without returning to a step sequencer.

### 5. Conditional clip chains

Add chance, repeat-until, and next/previous rules to the existing clip chain.
The current fixed chain remains the default. Conditions should be visible and
editable without hiding which clip will play next.

### 6. Effects as characters, not a large rack

The 0.21 Tone/Drive/Reduction/Delay chain is an intentionally compact first
page. Later additions should arrive as a small choice of coherent characters
or a second contextual page—especially filter resonance, diffusion/freeze,
and pitch shift—not as a long generic effects list.

## Ideas not to import directly

- Four independent tracks would conflict with Thru's four lanes as one
  playable stack.
- A large device-chain browser would compete with the engine-page model and
  the limited screen.
- Scene duplication should not replace the existing 64 clip states and clip
  chain. Conditional launch behavior should extend those systems.
