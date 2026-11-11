# s3g Thru 0.31.0

This release adds two ways to perform and compose with samples, along with new playback and editing controls.

- **Layers mode** pairs a sample keyboard with an XY surface of four parallel stereo effects. Each corner can use its own effect and settings, including duplicate effects. The expanded palette includes filter, blur, delay, tape, rotary, spectral, buffer and rhythmic treatments. Pressure, glide and clip recording work with FX gestures.
- **Sequence mode** provides four independent sample lanes with two voices per lane. Each lane has a 1–32 step trigger sequence, its own length, rate and rotation, and a selected engine. Trigs support position, note, velocity, duration, microtime, probability, Burst and cycle conditions. Repeating clips preserve lane phases; new launches reset them. Audio recording remains separate from sequencer editing.
- **Sequence mixing and editing** adds lane level, pan, channel strip EQ and Priority Duck; trig copy/paste, stopped audition, waveform markers and a duration-fitted envelope display. Live rate changes and sequence position displays have been corrected.
- **Sample playback** adds pitch-preserving stretch from 0.25× to 4×. The stretch ratio changes source duration independently of Sequence note duration.
- **Tempo and interface** extends BPM to 1–999, shows unavailable controls as hollow squares, and updates on-device help, MIDI mappings and the illustrated manual.

Install through Schwung Manager using `https://github.com/s3g/s3g-thru`, or download `s3g-thru-module.tar.gz` from this release. Controller users should download the matching E16 bundle from this release. Existing projects and parameter identities remain supported.
