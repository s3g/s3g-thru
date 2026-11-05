# s3g Thru 0.30.0-test.31

Local combined E16 NRPN + Monolit routing test. The public GitHub installer
remains at 0.29.1.

## One-USB controller route

- Adds persistent **NRPN + MNLT** (`N+M`) parameter mode without changing the
  numeric values of the existing Auto, NRPN, CC, MNLT, E16 and Off modes.
- Adds a stock, script-free E16 scene with twelve stable Engine, Stack,
  engine-detail and effect pages. Encoders 1–8 control A and 9–16 control B.
- Resolves E16's private 14-bit aliases against each part's current engine and
  returns matching feedback without changing Move's editing focus.
- Keeps Monolit on channels 9–13 at the same time. With E16 MIDI Thru enabled,
  the intended topology is Monolit TRS Out → E16 TRS In → E16 USB-C → Move
  USB-A. Monolit is powered separately.
- Keeps the existing E16 Remote controller available; the native route uses
  Schwung Surface **CC Only** and does not load Remote or Lua.

Install `dist/s3g-thru-module.tar.gz` and load
`dist/s3g-thru-e16-nrpn-monolit.zip` for hardware testing. Native, sanitizer,
UI, help, MIDI, controller and package checks are required before promotion;
hardware acceptance is not implied by the local build.
