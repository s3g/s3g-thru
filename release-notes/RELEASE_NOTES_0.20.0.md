# s3g Thru 0.20.0

This release brings the deeper s3g Sample-family motion and event ideas into
the Move-native playback engines while keeping one coherent Motion instrument.

- Motion retains one algorithm and gains eight selectable sounds. Continuous,
  Packets, and Motor share the same trajectory; Bounce, Freeze, Iterate,
  Pulser, and Doublets use a bounded segment-event renderer inside Motion.
- Path Rate now has Normal and Hertz bases. Moving Loop has an adjustable step,
  while Zigzag and Drunk evolve repeatably from Seed, Travel, and Jitter.
- Packets use Hann-shaped pulses with their own rate and duty. Motor keeps that
  inner clock and adds an independent outer rate, symmetry, and Triangle,
  Rounded, Pulse, Exponential, or Plateau envelope.
- Segment sounds can trigger from their clock, packet wraps, or trajectory
  turns. Events can cut or overlap, with a hardware-adjustable one-to-four
  reader limit per playback voice plus repeat, step, pitch-scatter,
  level-variation, interval-curve, and envelope controls.
- Motion's parameter surface is conditional: Continuous shows no unused sound
  page, Packets adds one compact page, Motor adds its own page, and segment
  sounds add Event Clock and Event Shape. Every active parameter still has one
  page location.
- Grains adds Sorter. It divides Field into up to 64 equal regions, samples the
  dominant lane's stereo energy, orders the regions from quietest to loudest,
  and uses Process Amount to blend from the ordinary grain origin into that
  order. The ranking is cached and rebuilt only when its source context changes.
- Cutups adds Random Cycle, a deterministic shuffled bag that visits every
  active region exactly once before producing another shuffle.
- The external MIDI map gains Motion controls on CC 59–64, 118, 119, 121, 122,
  and 124–126. Standard All Sound Off (CC 120) and All Notes Off (CC 123)
  remain unassigned to parameters.
- Clip-bank format advances to 3 to store the appended lockable controls.
  Formats 1 and 2 remain importable, existing parameter indices remain stable,
  and new controls receive defaults when an older bank is opened.
- The illustrated manual includes each conditional Motion page and complete
  references for Sorter, Random Cycle, the new MIDI controls, and hardware CPU
  checks.

The state schema remains version 6 and the binary magic remains `S3GTHR3`.
