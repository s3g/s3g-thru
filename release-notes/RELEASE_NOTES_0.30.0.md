# s3g Thru 0.30.0 — draft

Unreleased. These notes describe the current local 0.30.0-test.39 build.
The published installer remains at 0.29.1.

## Native E16 ownership

- Prevents an uninitialized E16 scene from transmitting raw zero across the
  ENGINE page and silently replacing restored Position, Field, Speed, Scan,
  Join and Level values. Real encoder turns now begin at raw one, while raw
  zero requests immediate feedback instead of editing the instrument.
- Migrates the exact zero-sweep state written by test.38 back to playable
  defaults on first load; isolated intentional minimum values are preserved.
- Adds quick-press default reset to every active encoder in the script-free
  E16 NRPN + Monolit scene. Defaults are resolved inside Thru, so adaptive ENG
  pages reset the parameter belonging to the current engine. PRIO returns to
  neutral Off, XFD to center, levels to unity and effect mixes to dry.
- Uses dedicated CC presses on isolated channels 14/15 for reset, preserving
  each encoder's selected NRPN turn address while crossing Schwung's control
  path reliably.

- Prevents the UI's Remote heartbeat from claiming E16 while MIDI Control is
  set to NRPN, MNLT, N+M, Generic CC or Off. N+M now leaves the script-free
  THRU NM scene intact from takeover startup onward.
- Adds a second ownership guard around Remote handshakes and releases an active
  Remote session immediately when its control mode is changed.

## Dual-mode Priority Duck

- Adds stereo-linked Priority Duck to Split and Shared Dual. PRIO chooses A or
  B as the dominant part and caps attenuation at 24 dB; its center detent is
  Off. Quad and Single bypass it.
- Adds threshold, attack, hold, release, detector HPF, soft knee and Dry /
  Dry + Sends routing. Detection follows the dominant part's private insert
  and level but precedes Blend; existing shared-effect tails remain open.
- Places PRIO and THR on FX Sends, with the remaining controls on a dedicated
  FX Priority Duck page. Clip recall, E16 Remote, fixed NRPN and direct CC are
  included, and older clips migrate with ducking disabled.

## Selectable MIDI controllers and Monolit

- Adds a persistent MIDI Control page in every instrument layout, with Auto,
  NRPN, Generic CC, MNLT, E16 Remote and Off parameter modes.
- Maps eight consecutive ordinary CCs to Move's visible part and page. Channel,
  base CC and Pickup/Jump takeover are configurable.
- Adds channel-addressed MNLT control: channels 9–12 address A–D simultaneously,
  while channel 13 carries shared Performance controls and optional commands.
  CC 72–79 on each part channel add Stereo Cross Pan and seven Rotary controls.
- Supplies a Split / Shared Dual Monolit example with A on faders 1–4 and B on
  5–8 across LIVE, both halves of engine pages 1–2, Mix/Send, Color and Rotary.
  All paired buttons remain local MOTION controls; the complete receiver CSV is
  included for custom layouts.
- Adds a fixed shared A/B crossfade at channel 13 CC 24 and curve selection at
  CC 25. MIX/SEND places X-CURVE on fader 4 and X-FADE on fader 8. Per-part
  X-PAN moves into the focused Rotary bank.
- Keeps notes, note-offs and polyphonic aftertouch independent of parameter mode.
  Removes the obsolete hard-coded 7-bit CC route to prevent mapping conflicts.
- Keeps the full fixed 14-bit NRPN registry and E16 Remote behavior intact.
- Fixes Pickup for bounded integer and enum controls. A fader now engages
  anywhere within the current choice's region and stays engaged across later
  choices, so adaptive engine selectors such as Texture MODE respond reliably.
- Adds a combined **NRPN + MNLT** mode for Monolit TRS → E16 → Move. A
  script-free E16 native scene uses twelve stable A/B pages and dynamic
  high-resolution aliases, while E16 MIDI Thru merges Monolit's channel 9–13
  CC controls into the same direct USB connection. No USB hub, Lua script or
  Remote surface is required.




## Readable E16 Remote controller

- Restores OXI Remote as the recommended E16 interface, with compact parameter
  names and formatted choices/units directly underneath each name.
- Retains the current twelve-page Engine, Stack, four engine-detail, Sends,
  Color, Rotary, Delay, Reverb and Tape layout. A/C occupies the upper rows;
  B/D the lower rows, with Quad A/B ↔ C/D selection.
- Keeps all twelve page positions stable when engines change. Unused controls
  remain as reserved squares instead of reshuffling page numbers.
- Redraws only changed pixel rectangles and rings. Headings are acknowledged
  first; ordinary text choices change only their value tile. Algorithm changes
  preserve common cells, the other part and effects.
- Retains bounded packet output, a 320-pixel acknowledged window, missing-ACK/
  NACK recovery, reconnect handling, focus preservation and held-encoder fine
  control. Remote RGB provides neutral, teal, plum, yellow and shared colors.
- Uses a script-free THRU RM scene and Schwung Surface **E16**. Remote is the
  official E16 integration distributed with this release.
- Adds Page Jump: Shift + encoder 2 opens all twelve page names at once;
  encoders 1–12 jump directly and encoder 16 cancels. Turns are suppressed
  while the overlay is open, and Move's editing focus remains unchanged.

## Pulsar modulation

- Adds a second page with paired emission/formant modulation, source scatter,
  formant variation, reverse probability and formant keyboard tracking.
- Keeps modulation per note, supports live edits through new pulses and bounds
  source scatter to Field around the current or held anchor.
- Starts new amounts at zero to retain existing sounds; keeps stereo timing
  and the bounded reader budget. REV% controls sample reading direction.
- Adds NRPN 3338–3345, plus a full parameter reference and screen diagram.
  ERAT/FRAT use logarithmic 14-bit encoding.
- Reads prior banks with neutral new settings; new saves use format 30.

## Pulsar sample trains

- Adds a fifteenth engine using recorded fragments as windowed pulsarets, with
  independent per-note pulse clocks, pitched pads, source travel and lane paths.
- Adds one page: RATE, FORM, SIZE, WIN, SKEW, OMIT, BRST and JITT. Main-encoder
  click holds the source; source holds and parameter movements save with clips.
- Includes fixed reader budgets, smooth-edged windows, sample filtering,
  retained launch settings, overlap gain control and DC removal.
- Adds fragment brackets/activity marks, overlap-limit feedback and NRPN
  3328–3337. RATE/FORM/SIZE use logarithmic 14-bit encoding.
- Extends Algorithm/Engine List enums to 0–14. Reads older banks and writes
  format 30. Update custom enum encoding for the new engine range.

## Stochastic listener capture

- Adds LAW LSTN: internal note activity restrains mutation, momentum and evolution
  to let each note settle into a found shape, with quick capture and slow release.
- Uses CAPT on knob 6 in LSTN, keeping its saved value separate from PULL.
  AMUT/TMUT retain their depths. Source hold, pitch and stack scan stay independent.
- Listens before part/output level and effects, with independent part references
  and per-note capture. It does not require microphone or external audio routing.
- Adds NRPN 3210 for CAPT and extends LAW to 0–3. Custom LAW encodings need updating.
- Reads older sets, including formats 26 and 27; new clip banks use format 30.

## Expanded Stochastic transformation

- Extends AMUT into signed waveform deformation and reflected folding; TMUT
  reaches much wider accumulated logarithmic time maps across sixteen points.
- Adds precise logarithmic EVOL adjustment, rates up to 1024 and a Cycle endpoint.
  Walk updates enter at overlap boundaries with a fixed maximum work budget.
- Adds source hold on the Stochastic page's main-encoder click, with visible
  status. Stack scanning and deformation continue; new notes reuse the source.
- Adds prepared stereo low-pass levels and antiderivative anti-aliasing for folding.
- Adds NRPN 3208–3209, hold automation and old format-26 import; writes format 30.

## Stochastic sample deformation

- Adds a fourteenth playback engine that continuously deforms recorded stereo
  fragments using bounded amplitude and timing walks, with independent state
  per note. Silent sources stay silent.
- Adds Size, Amplitude Mutation, Timing Mutation, Evolution, Memory, Pull,
  Distribution and Period Drift on one page. Keyboard pitch remains separate
  from source travel; Character pressure increases deformation for that note.
- Includes overlap transitions, bounded work, stereo-linked timing and DC removal.
- Adds NRPN 3200–3207 for direct controller access.
- Reads older sets; saved clip banks now use format 30.

## Per-note Engine Lists

- Adds ONE / LIST and UP / DOWN / RAND on Pads & Scale, with a Voice Engines
  page for eight Single entries, four per part in dual modes or two in Quad.
- Lets a chord combine engines while keeping the existing eight-voice limit,
  sources, envelopes, effects and per-part controls. Repeated engines weight
  random assignment among free entries and share their engine settings.
- Select an entry with the main encoder, click to edit its existing engine
  pages, and use Back to return. Engine changes affect new live notes.
- Saves lists with clips and records resolved per-note choices for repeatable
  playback, including notes held before recording begins.
- Adds eleven NRPN controls without renumbering earlier addresses. Existing
  E16 ALG edits the selected entry; list setup and engine-specific pages stay on Move.

## Voice assignment and expression

- Adds Cycle Up, Cycle Down and Shuffle for single-note phrases through lists.
- Adds Degree and Register assignment, independent of note-playing order.
- Adds Voice Setup with per-entry octave, source launch phase, stack phase/lane
  and stereo pan. Notes retain their launch values, including during voice steals.
- Adds pressure Brake, Character and Both, with part depth; Quad supports Character.
  Character uses engine-specific controls or a smooth per-voice tone treatment.
- Records resolved entry variation, already-held pressure and external MIDI
  polyphonic aftertouch. Saves new setup with clips, parts and copies.
- Adds six NRPN addresses while preserving existing IDs.
  Expanded choice ranges require updated numeric encoding in custom mappings.

## High-resolution MIDI control

- Adds 342 fixed NRPN addresses covering playback engines, pads, envelopes,
  stack motion, effects, mixing, recording, performance loops, chains and fades.
- Separates notes and pressure on channels 1–4 from A–D parameter controls on
  channels 5–8 and shared controls on channel 16. Each part uses the same
  parameter addresses, making independent instrument mappings easier.
- Applies complete 14-bit values, including exact unity/default and bipolar
  centre settings. Incomplete Data Entry pairs are ignored.
- Move’s visible parameter values follow external edits. Part-control edits
  preserve Move’s current editing focus and page. Supported parameter gestures
  can be captured by the performance recorder.
- Returns current values after Move edits and clip recalls. Feedback is
  coalesced. Ordinary NRPN feedback pauses while E16 Remote owns the controller
  connection.

## Tape alignment

- Aligns stable dry/wet Tape mixing, removing the fixed 7 ms mismatch that
  could produce deep comb-filter notches at partial MIX settings.
- Uses a shared two-sample base reference (about 0.045 ms at 44.1 kHz),
  including MIX zero, keeping coloration changes consistent in timing.
- FMT and AGE retain bandwidth, saturation, head contour and stereo-width
  character while removing their automatic right-channel timing offset.
- Keeps wow, flutter, drag and inertia linked across left and right. These
  intentional wet-path timing changes can still chorus at partial MIX.
- Preserves the eight Tape controls, their positions, saved parameter IDs,
  performance automation and E16 mappings.

## Setup and documentation

Use current E16 firmware and OXI App. For readable Remote control, install the
matching module and script-free THRU RM scene, set USB Multi Port and USB MIDI
Thru Off, use USB1 and Schwung Surface **E16**, then connect Move USB-A to E16
USB-C. Select the scene before opening takeover; Remote starts automatically.

For the script-free native E16 + Monolit route, install the matching module and
THRU NM scene, use Schwung Surface **CC Only**, set Thru MIDI Control to
**N+M**, and enable E16 MIDI Thru. Monolit connects to E16 over TRS MIDI while
E16 remains connected directly to Move over USB. Users upgrading from test.38
must replace the THRU NM scene as well as the module. Test.39 repairs the exact
saved startup-zero signature on first load.

The Remote example is **controllers/oxi-e16** and the native route is
**controllers/oxi-e16-nrpn**. Build their settings with
`npm run controllers:generate`; package Remote with
`npm run controllers:package` and the native E16 + Monolit bundle with
`npm run controllers:nrpn:package`.

The new **MIDI and OXI E16** manual chapter covers setup, A/B layout, mode
handling, the page reference, reading values, connection checks and mapping
another controller. The complete NRPN address/range table is in
`controllers/midi/parameters.csv`.

These are draft release notes. The test.39 module, native scene and package
validation pass locally. Installation and listening acceptance on Move remain
part of release preparation; no tag, GitHub release or installer-manifest
change has been made.
