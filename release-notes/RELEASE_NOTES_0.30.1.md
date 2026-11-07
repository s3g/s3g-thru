# s3g Thru 0.30.1

Pulsar now supports longer audible sample pulses, with clearer feedback about
their duration.

- **Longer pulses:** SIZE reaches 1000 ms and FORM reaches 1 Hz. SIZE selects
  the amount of recorded audio; FORM sets how long each pulse sounds.
- **Duration display:** both Pulsar pages show the effective duration, or a
  range across held notes. LIMIT identifies overlap protection; ~ marks the
  Root-note estimate before playing.
- **Fine control:** Move and E16 retain precise logarithmic adjustment across
  the wider ranges. MIDI, saved clips, the manual and on-device help are updated.

To hear a full second, hold a Root note with **RATE 0.5–2 Hz, FORM 1 Hz,
SIZE 1000 ms, JITT 0**, and modulation off. Use a source region at least one
second long. Higher notes or faster rates can shorten the pulse safely.

Update through Schwung Manager. Requires **Schwung 1.7.3 or newer**.
Existing 0.30.0 sets and controller presets carry forward; no controller
preset reimport is needed.

See the [Pulsar guide](https://s3g.github.io/s3g-thru/engine-pulsar.html)
for controls and starting points.
