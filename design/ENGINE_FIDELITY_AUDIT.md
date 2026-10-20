# Playback engine fidelity audit

This audit compares s3g Thru 0.21 with the current Sample-family engines in
`s3g-dsp`. It distinguishes musical identity from desktop-host features that
do not fit Move. Multichannel output assignment, plug-in voice routing, and
host-specific editor behavior are not fidelity requirements for Thru.

## Summary

| Engine | Fidelity | What is already carried over | Meaningful remaining gaps |
|---|---|---|---|
| Motion | High | Eight trajectories; Normal/Hertz path rates; travel, jitter, moving-loop step and seed; Continuous, Packets, Motor, Bounce, Freeze, Iterate, Pulser, and Doublets; clock/packet/turn event triggers; cut/layer overlap; repeat, pitch, level, and interval shaping | Routed Iterate; independently selectable articulation and segment model; the desktop engine's distinct inner/outer rate model; higher event-rate range |
| Lanes | Medium-high | Forward/reverse/ping-pong transport; eight stack paths; Crossfade/Jump; four path shapes; offset, skew, curve, manual lane, scan rate, slew, and pitch-preserving stretch | Normal/Hertz rate basis; per-lane speed, stretch, and nudge; editable manual-path breakpoints; independent per-lane transport tuning |
| Grains | High core | Scan/Freeze/Cloud/Slice source modes; six windows; regular/scatter timing; behind/around/ahead spray; pitch/reverse/size/level variation; source advance; Ordinary, Sorter, Stutter, Shrink, and Doublets; selectable 4/8/16 grain budget | Source-time sync; channel source modes and stereo-link choice; inherited per-lane Lanes controls; desktop extremes above Thru's bounded 96 Hz, 1000 ms, and ±24 st ranges |
| Cutups | High performance core | Free/tempo clocks; equal/transient regions; six source orders including non-repeating Random Cycle; repeat, gate, join, timing, reverse, pitch, and level variation; seven lane orders; four poly paths | Whole/half divisions; manual source and file patterns; explicit pattern length; per-lane tempo metadata and sync; manual lane/source steps |
| Sample | Medium | Forward, reverse, loop, and ping-pong; chromatic eight-voice playback; four discrete-lane variation modes plus velocity-selected lane; cues and rolls | Rate/Stretch/Hybrid pitch modes; host sync; Auto/Gate/One Shot/Toggle triggers; Layer/Restart/Ignore retrigger; Mono/Legato; dedicated attack/release and glide |
| Slices | Medium | Equal or detected transient regions; pad-addressed regions; four playback directions; discrete lane variation; chromatic, slice, cue, and roll pad layouts | Per-slice gain, pitch, pan, direction, launch, loop bounds, and choke; trigger/retrigger/voice modes; stretch/sync pitch behavior; proportional slice envelopes |
| Glitch | Native to Thru | Per-voice rolling history, five repeat-buffer modes, free/tempo clock, rotating distributed pulse patterns, deterministic chance, jump, pitch, reverse, decay, and full buffer override | Not a port. Future changes should follow Thru performance needs rather than desktop parity |

## Fidelity decisions

- Eight-note polyphony, independent playheads, and independent stack paths are
  more important on Move than desktop multichannel output allocation.
- Pad velocity remains excluded from playback level. It may choose a discrete
  lane in Sample/Slices, but it does not attenuate the voice.
- The shared Position/Field and Sample Edit pages replace duplicate start/end
  controls inside every engine.
- Per-clip performance data replaces a conventional step-pattern editor.

## Recommended fidelity work after 0.21

1. Add a Lanes Detail page with per-lane speed, stretch, and nudge, plus a
   compact manual-path editor. This is the largest remaining gap in an engine
   whose identity depends on relationships between lanes.
2. Add a Direct Voice page shared by Sample and Slices for pitch mode,
   trigger, retrigger, and voice mode. Expose only behaviors that remain clear
   on pads; do not reintroduce velocity-to-level coupling.
3. Extend Cutups with pattern length and a recorded/manual source-order lane.
   The performance recorder could write this more naturally than a step grid.
4. Add Grains source-time sync and stereo-link behavior only after measuring
   the 16-grain setting on hardware. The present bounded ranges are musically
   complete and safer for Move's audio deadline.
5. Treat Routed Iterate, desktop output routing, and multichannel allocation as
   out of scope unless a future Move host exposes a genuine destination for
   them.
