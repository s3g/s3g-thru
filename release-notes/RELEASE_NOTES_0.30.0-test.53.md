# s3g Thru 0.30.0-test.53

Local testing build; not a published release.

- New shared MIDI Notes page: Parts/Focus routing, sustain, smoothed 14-bit
  pitch bend, channel pressure, mod wheel to Stack or Character, and optional
  external-key velocity volume. Parameter-control modes remain independent.
- Channels 1–4 address A–D in the active layout. Bank B uses notes 60–75 for
  slots 1–16, without arming empty-pad recording. Releases retain ownership
  after focus changes; Move pad latches survive external Stop and CC panic.
- Performance clips capture held and sustained notes plus expression, with
  saved-state migration to clip-bank format 35. Export a set before upgrading
  if a downgrade may be needed.
- Optional Move clock following offers Tempo Only or Follow Clips, including
  Start, Stop, Continue and clock-loss cleanup. Clock updates do not autosave.
- MIDI guide, OXI One example, current NRPN CSV, screen and in-device help
  updated. Existing E16 and Monolit scenes/scripts are unchanged.

The published installer remains 0.29.1. Actual Move/Schwung keyboard,
expression and clock behavior still requires hardware testing.
