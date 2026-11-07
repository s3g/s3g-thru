# nanoKONTROL2 starter

Use CC mode with factory channel-1 CC assignments and momentary buttons
(nonzero on press, zero on release). In Schwung select CC Only;
in s3g Thru select MIDI Control MODE Auto or CC, then MIDI Maps PROF NANO.
This is built into the Move module: no controller script or preset transfer is
required. Existing customized hardware can be handled by copying NANO to C1–C4
and relearning or manually entering its channel/CC addresses.

Faders 1–4 address A Position/Field/Speed/Level; faders 5–8 address B.
Knobs use the bank selected by S1–S8: Engine 1–4, Sends/Crossfade, Color,
Rotary, Reverb. In the Reverb bank all eight knobs address shared parameters.
M1/M5 mute A/B; M2/M6 hold A/B scanning. Transport buttons control performance
clips. Track/Marker arrows change controller bank. Set Marker is Undo; Cycle
holds a pattern fade. R buttons and the other M buttons are ignored until you
reassign them. Read the [manual](https://s3g.github.io/s3g-thru/midi.html#nano-profile)
for the complete workflow and response settings.

`profile.json` is the source for the compiled starter. Regenerate/check it with
`node scripts/generate-custom-midi.mjs [--check]`. Parameter names resolve through
the shared registry; public MIDI IDs and other controller examples stay stable.
Custom user profiles are saved in `thru-midi-maps.bin` in the module data
directory, separate from project archives. Importing a project retains the
destination Move’s controller profiles.

[Korg software and manuals](https://www.korg.com/us/support/download/product/0/159/)
describe CC mode, programmable hardware assignments and button settings.
