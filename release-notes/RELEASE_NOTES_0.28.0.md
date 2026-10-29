# s3g Thru 0.28.0

This release adds two- and four-instrument layouts, stereo reverb, configurable
effect order, and clearer controls for live playing and recording.

## Single, Split, Shared Dual, and Quad

- Single keeps the original four-lane instrument with eight source voices.
  Split divides the pads into A/B instruments with two stereo lanes and four
  voices each. Shared Dual gives both programs access to all four source lanes.
- Quad provides four eight-pad instruments, each with one stereo lane and two
  voices. A/B occupy the upper half of the grid; C/D occupy the lower half.
- Each part has its own engine, parameters, keyboard, envelope, lane gains,
  and private Color and Stereo / Rotary effects. Use different engines, or
  several versions of the same engine with different settings.
- Choose a layout with **Capture + Menu → Instrument Mode**. Capture changes
  editing focus; Copy returns to Single and back to the last selected layout.
  Track buttons select the source lane and, in Split or Quad, its owning part.
- Dual modes use A/B Blend. Quad uses independent part levels. Both offer
  Delay/Reverb sends, shared Tape, performance recording, and per-clip recall.
  MIDI channels 1–2 address A/B in dual modes; channels 1–4 address A–D in Quad.

## Reverb and effect order

- Plate and Room reverbs provide decay, predelay, damping, diffusion,
  modulation, width, and mix controls.
- FX Order uses a readable list: turn to select an effect, click to lift,
  turn to move, and click to apply. Back cancels an unapplied move. Changes
  crossfade while retaining effect tails.
- Single can reorder the complete chain. Multi-instrument layouts keep private
  inserts and shared parallel Delay/Reverb sends; Tape’s position determines
  which returns receive its processing.
- Shift + Capture opens FX Sends; main click opens FX Pages. Dual-mode return
  levels are on FX Sends. In Quad, all eight positions are sends, with DRTN/RRTN
  at knob 8 on Delay Return/Reverb.

## Cutups and waveform editing

- Cutups gains direct Lane and Advance controls. Choose Hold or 1, 2, 3, 4,
  8, or 16 cuts per lane, independently of region repeats. Loop pauses automatic
  advancement and remembers the pad’s lane. Advance is also available on CC 19.
- At full Gate, Join overlaps consecutive cuts; shorter gates keep their gaps.
  Solid and dashed readers show incoming and outgoing positions, including
  release tails.
- Waveforms enlarge the Position/Field window. Split zooms its lane pairs
  independently; Shared Dual follows the focused program across four rows;
  Quad gives every row its own live window. Shared Dual and Quad stay responsive
  to POS/FLD edits while notes or performance data play.
- Focus, source selection, lane contribution, voice counts, and audio activity
  have separate indicators. Muted teal, plum, and amber pad backgrounds help
  identify the multi-instrument zones.

## Playing, audition, and recovery

- Capture + Menu opens the contextual Pages picker. Used icon keys illuminate
  from startup and show their current availability.
- Delete + Play provides Panic: stop the performance loop, finish a data take,
  and release notes, latches, Rolls, and audition. Audio recording continues;
  existing effect tails decay naturally.
- The curved-arrow Undo key restores the latest supported destructive edit,
  source replacement, lane clear, or clip operation. It lights when a one-level
  safety undo is available. Back remains navigation; Delete + Record still
  removes the latest completed overdub take while protecting the base take.
- The sample browser offers Part and Direct audition. Part follows the relevant
  instrument mixer; Direct bypasses it and the effects, while retaining shared
  mastering. Preview level remains independent of recording gain.
- Mix Status explains activity, muted parts, zero levels, faded-out parts,
  missing audio, and idle voices. Live dual/Quad mixer corrections override
  recorded mixing until playback restarts or another clip launches.
- Saving/Saved feedback tracks archive and metadata completion. Save errors
  remain visible while retrying. Part mute and source loading/preview behavior
  remain consistent across focus changes and recall.

## Upgrading

- Existing sets load forward with defaults for new controls. The expanded bank
  format stores four programs and C/D performance data; files saved by this
  release require s3g Thru 0.28.0 or newer. Export a set backup before upgrading
  if you may need to return to an older version.
- The source-voice total remains eight; all programs and effects share Move’s
  processor. Use Shift + Menu to check demanding engine/effect combinations.
- Schwung 1.7.3 or newer remains the declared minimum host version. Install
  through Schwung Manager’s GitHub URL option
  using `https://github.com/s3g/s3g-thru`.

See the illustrated [s3g Thru manual](https://s3g.github.io/s3g-thru/) for the
mode overview, all nine engine guides, effects, recording, and shortcuts.
