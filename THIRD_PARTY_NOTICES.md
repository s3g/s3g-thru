# Third-party notices

## Schwung plugin ABI

The minimal ABI declaration in `include/thru_schwung_abi.h` follows the
MIT-licensed Schwung `plugin_api_v1.h` structure prefix.

- Project: https://github.com/charlesvestal/schwung
- Copyright: Schwung contributors
- License: MIT

## s3g Sample Motion engine

`include/sample_motion_app.h` is the fixed-memory engine developed in the
s3g Daisy firmware repository and is redistributed under this repository's
BSD-3-Clause license.

- Source repository: `s3g-daisy`
- Source path: `firmware/common/sample_motion_app.h`
- Imported source SHA-256:
  `73385c00a02d05a3af1dea74ced8c60ff57261c2e87efc6d0c6f82c11492f2d7`


## Reverb design reference

`include/thru_reverb.h` is an original BSD-3-Clause implementation. Its Plate
algorithm follows the network topology and delay-time constants in Jon Dattorro,
“Effect Design, Part 1: Reverberator and Other Filters”, JAES 45(9), 1997.
The Room algorithm uses a four-line orthogonal feedback-delay network.

Reference: https://www.convexoptimization.com/TOOLS/EffectDesignPart1.pdf

## miniz ZIP archive library

`include/vendor/miniz` contains the upstream miniz 3.1.2 amalgamation, used
for portable `.thru` project archives.

- Source: https://github.com/richgel999/miniz/releases/tag/3.1.2
- Source archive SHA-256: `f0446d863f9c19926ad9483c523fdc42e42b8d4a6a431d27e09d49c79a140d9a`

Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

## s3g-dsp stereo effects

The portable kernels in `include/s3g_fx/` are adapted from
https://github.com/s3g/s3g-dsp at commit
`ed75ef2646c574fe8f9f3f66628b0c4c290ff562`:

- `s3g_macro_fracture.h`, `s3g_fracture_processors.h`
- `s3g_macro_shred.h`, `s3g_analog_drive_circuits.h`
- `s3g_buffer_processor.h`, `s3g_math.h`, `s3g_realtime.h`

Copyright (c) 2026, Shawn Greenlee. BSD-3-Clause, reproduced in `LICENSE`.

The Thru stereo adapter selects two processing cores, keeps all audio
buffers prepared, bounds buffer seam searches, validates recent history
and caches control geometry. The rate multiplier range is extended to 4x.
Parameter slews include quiet-tail protection. The stereo adapter uses a
filtered 2x stream and disables the original nested midpoint approximation;
stereo-linked input/output conditioning and output damping suit a polyphonic bus.
Spectral Spray is a new compact stereo implementation using Thru's spectral
framework. Layers Reverb reuses `thru_reverb.h`; Layers Tape adapts Thru's
existing stereo tape equations. These implementations are also BSD-3-Clause.

Layers Pitch/Detune (`include/thru_layers_pitch_rotary.h`) adapts the overlapping
sample-reader approach in s3g-dsp `dsp/s3g_macro_pitch.h`. Copyright (c) 2026,
Shawn Greenlee; BSD-3-Clause, reproduced in `LICENSE`. The stereo adaptation
uses signed reader phases, smooth unison transitions, cubic interpolation and
source anti-alias filtering. Layers Rotary adapts Thru's existing independent
horn/drum and Doppler model without adding the shared drive/lo-fi stages.

Sequence scheduling and trig/Burst adapters in include/thru_sequence.h and
src/dsp/thru_sequence.inc adapt s3g Opps include/opps/model.h and
src/core/engine.cpp, revision d3f4ed8d6e3b1c3d5299bc23212308cf932813aa.
Copyright (c) 2026, Shawn Greenlee; BSD-3-Clause, reproduced in LICENSE.
The adaptation uses four 32-step sample lanes, inherited trig values,
sample-offset scheduling and Thru's transport, voices and persistence.
The Sequence channel strip and protected-lane duck in
`include/thru_sequence_mix.h` and `src/dsp/thru_split.inc` adapt Opps's final
mix design with four stereo sample buses and Thru's existing send path.

## s3g Drum synthesis cores

`include/s3g_drum/` contains the host-independent synthesis models from
https://github.com/s3g/s3g-dsp, `dsp/`. Copyright (c) 2026 Shawn Greenlee.
Redistributed under the BSD-3-Clause license included in this module's LICENSE.
No CLAP host or plugin GUI is included. The math and realtime forwarding headers reuse the byte-identical copies already in `include/s3g_fx/`. Original imported source SHA-256:

- `s3g_math.h`: `b649f4001237735225b7df28823731283ff5e2f550ff38fc018619324e62baf7`
- `s3g_realtime.h`: `9a1ab464e86a76af663c91879f0245aed94e0cb7c56eecc251c684611d1fd9d1`
- `s3g_drum_primitives.h`: `dc414ecff6cf9e517804d99b99f1e1908b1817a0620dd59d2caa0d0da8eaf45e`
- `s3g_drum_character.h`: `3d77f70ca337c13e6139972e9f680ad025ee057198a389bb2f63443263fadf9c`
- `s3g_drum_kick.h`: `1f4983e760afbb21ba13d42e65a9e0ee6f1a9fb299b5b75491a4a5f8df43d367`
- `s3g_drum_snare.h`: `77b2300190cbe4cab8c37c1090f28e8dbe47968373b47b70084abd47a4884d9b`
- `s3g_drum_floor_tom.h`: `8983893fdca8ebdc0ea36d24a7dff90c8c9ade808020648a425f672c290b2503`
- `s3g_drum_tom_voice.h`: `310a8a4f1cdcf054e335e0a30eae24c4b6d29e6d5973bdd8c489762af336cd43`
- `s3g_drum_cowbell.h`: `ddd94551bd6509f2bfa06085eb7547f1872ec59ae70885281151edfd1509850d`
- `s3g_drum_hi_hat.h`: `d07f6a94761e63f5fd85311540a2d4004154d8c91238ee887430fb0e0c32e00a`
- `s3g_drum_crash.h`: `a04c41e15e3e6e367e72f7e01e5e1e0df17cc90b4157a280074926f60ccbd01b`

- `s3g_drum_concert_bass.h`: `ab9f852c56a9838fd58f3996cde2585b47299d9ab94c081860a98f0b4f112a8f`

The Drum adapter exposes native excitation, independent decay and character
controls. An optional short onset envelope changes transient/tail balance
before file normalization; a configurable submillisecond boundary fade retains
native beater attacks. New conventional Drum renders use the core's tone
controls once. Earlier recipes retain their original routing and edge settings.

## s3g modal and waveguide Drum models

Membrane Kick, Modal and Medium in the offline Drum generator adapt the
s3g-dsp kernels below. Copyright (c) 2026 Shawn Greenlee. BSD-3-Clause,
reproduced in LICENSE. The original CLAP plugins and GUIs are not included.
The Modal copy adds `tuneOfflineSample`, called before excitation to scale
mode frequencies and pole decays. The source resonators and exciters remain
intact. Thru uses Membrane's stereo pickup fold, stereo-panned Modal body
stems and a fixed stereo virtual pair for Medium. Listener and autonomous
sequencer features are not active in these finite sample renders.

Original imported source SHA-256 (math/realtime reuse the forwarding headers
listed above):

- `s3g_24ch_layout.h`: `e45de08fc3509cbac46884683847f6996d31b6b73a9cf14902c9b5207206f405`
- `s3g_accelerometer_field_encoder.h`: `977f41af53a5836c9256bf28e085045e28a97987a8c526da15b3a719e9a80519`
- `s3g_allrad.h`: `42c351a5ab627a0ae686e64a26d8b8a9c98c2d902b2caeb84db2f6330121cc49`
- `s3g_ambi_field_listener.h`: `5ed46da75fc346e2bdc43755622bfc12b8f16c932f1ff9c33d10b6f852a1e988`
- `s3g_ambi_membrane_kick.h`: `dd5b1155ee912373cd20ff95bee80cb7e3c38d3ac88876cb1d315aa1e5a0491b`
- `s3g_ambisonic_geometry.h`: `ef9be1fa72786ac87b9a2b451ef97343d6c8241b4700a66fcc648467a4a43cc4`
- `s3g_ambisonic_speaker_decoder.h`: `eeb20b2d7f8511c68a87a3e3c347d3f7ba0b3c0a8e7d65d76fe50b7b4d20dd6d`
- `s3g_cube41_layout.h`: `012446e79c5d6a6cea04d8f4333a9d6bb9a120ddd80af5e79dd225bf141338ed`
- `s3g_fractional_waveguide_network.h`: `a220214df5859872fe78f6fae8ce6a96db1c007bace295f702f85276e469e3b7`

## s3g-mc offline sample rendering

The Pluck, Subharmonic, Hybrid, RSYN Trace and shared Events additions adapt
the Karplus Field, Subharmonic Bank, Fata Morgana Resynth, Partial Trace Resynth,
Modal Terrain and Mass Partial Field processes in s3g-mc. They are bounded
stereo C++ implementations integrated with Thru's renderer; Python, NumPy,
REAPER and the original multichannel host scripts are not required on Move.
Pluck uses Thru's imported fractional allpass delay; Subharmonic uses
an editable sine bank of harmonic multipliers and undertone dividers. RSYN retains its stereo-linked
analysis for the original linked mode. Hybrid sources are embedded in WAVs.

Source: https://github.com/s3g/s3g-mc, Scripts/s3g-mc/s3g_numpy_render.py
SHA-256: `b3f381e2ef23a9abd7e15ef08db17bce46cbc6b5090b0beaccf1fcaac8efbb1f`

Original s3g-mc license:

Zero-Clause BSD
===============

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

## FM Drum design reference

`include/thru_synth_fm_drum.h` is an original BSD-3-Clause FM percussion
implementation: an oversampled carrier/modulator pair, independent FM and
pitch decays, feedback, shaped carrier, noise and click envelopes. The
LXR desktop firmware was consulted as a high-level instrument reference:
https://github.com/erica-synths/lxr-desktop-app
No LXR source, tables, samples or presets are included. This is not a port
or an emulation of the LXR firmware. Drum coloration reuses the already
credited s3g-dsp native drive/compression and adds original offline bit/rate/tone
processing.

## Lowform voice adaptation

The Lowform engine adapts Foundation, Harmonic Body and Texture construction from s3g Processor Lowform, with voice management and output processing for Thru.

- Source: https://github.com/s3g/s3g-dsp/blob/main/dsp/s3g_processor_lowform.h
- Copyright: s3g
- License: BSD-3-Clause (see LICENSE)

## IBM Plex Mono

The browser composer bundles the unmodified IBM Plex Mono Regular font.
Copyright © 2017 IBM Corp., Reserved Font Name “Plex”. Licensed under the
SIL Open Font License 1.1; the complete license is installed alongside the font
in `assets/composer/fonts/OFL.txt`.
