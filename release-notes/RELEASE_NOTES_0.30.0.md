# s3g Thru 0.30.0 — draft

Unreleased. These notes describe the current local 0.30.0-test.52 build.
The published installer remains at 0.29.1.

## Four-bank page navigation

- Replaces the long first-level Pages list with a compact hub: Voice, Lane,
  Mix/FX and Performance on the first row; Mode, Setup, MIDI and All below.
- Adds Capture + Track 1–4 shortcuts for the four working banks. The first
  chord opens the bank's main page and repeated chords advance through it.
- Keeps ordinary Track lane/part selection and Shift + Track sample browsing
  unchanged. Track LEDs temporarily identify the page banks while Capture or
  the hub is active.
- Keeps Up/Down inside the current bank and places a consistent, right-aligned
  current/total fraction on every parameter page. Counts are local to Voice,
  Lane, Mix/FX, Performance, Setup, or MIDI. Back returns page → bank → hub → the original view; All
  retains the complete contextual destination list.

## Pulsar size and Bank recording

- Extends Pulsar SIZE to 500 ms with fine logarithmic adjustment and matching
  MIDI ranges. Existing saved sizes and the 4 ms default are retained.
- Bank discards touches shorter than 10 ms. Longer recordings include their
  audio from the initial press, preserving the attack.

## Bank controls and display refinements

- Restores the black centre of Bank A’s hollow lane markers, including when
  multiple playheads cross the same area of the waveform.

- Corrects the physical X/Delete + pad shortcut and synchronizes its modifier
  through the UI as well as direct hardware MIDI. Menu does not arm deletion.
- Drawing follows Move’s physical pad rows without moving saved assignments.
- Repeated deletion of an empty pad preserves the previous Undo.
- Clarifies COMP on Play Gain as the former POLY gain compensation, and the
  native E16 ring colors and synchronized feedback.

- Adds B-wide master RATE with per-pad RATIO, smoothed stereo PAN, held loop
  or one-shot PLAY and choke groups. Existing Bank rates retain their sound.
- Bank Pads SRC matches Mix/Rec: IN, MIX, PT A and PT B. PT B resamples
  the Bank pads; MIX includes both parts and shared effects. A take keeps
  its source until it finishes. Settings save per clip, and older A/IN
  selections retain their routing when upgrading.

## Native E16 + Monolit

- Corrects the native E16 turn direction: clockwise increases values and
  counterclockwise decreases them, matching the rings. Update the Move module;
  the test.48 scene and feedback script remain compatible.

- Replaces experimental absolute NRPN encoder turns with explicit native
  relative CC. MIDI Mode is E+M (E16 + Monolit), with OFF last.
- Keeps A on encoders 1–8 and B on 9–16, twelve adaptive pages, Monolit’s
  CC map and encoder-press default reset.
- Native relative turns start at Thru’s current value and do not depend on
  feedback timing. A small feedback-only script now mirrors actual values on
  E16 rings, including Move/Monolit edits, default resets and clip recalls.
- Requires the matching THRU EM scene. Remote remains a separate option.

## Legato and Bank playing

- Adds per-part Poly, Mono retrigger and Legato on Pads & Scale. Legato keeps
  the playhead, envelope, stack phase and first engine during overlapping
  notes, with last-note priority and return to a previous held key.
- Adds Bank mode: left pads play A through all four lanes; sixteen right pads
  hold independent stereo recordings. Four voices serve each side.
- Hold an empty right pad to record A after private effects, before level/blend.
  Release finishes silently. Hold a filled pad to loop; Shift + pad latches;
  Delete + pad clears; Undo restores the assignment.
- Saves immutable WAV takes in Thru Recordings and retains assignments per clip.
  Bank playback notes can be captured in performance clips.
- Adds a selected-pad waveform, slot overview, Start, End, Rate, Reverse, Level,
  Attack, Release and Loop Fade. Maximum twelve seconds per pad; one save at a time.
- Adds compact fixed-position saving/saved feedback without resizing pages.

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
- Adds combined **E16 + MNLT** mode for Monolit TRS → E16 → Move.
  Native relative CC turns retain immediate response. A small Thru Rings
  script draws actual parameter values while Monolit’s channel 9–13 CC
  controls share the direct USB connection. No USB hub or Remote surface
  is required.




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

For native E16 + Monolit, install the matching module and THRU EM scene
with its Thru Rings script. Use Schwung Surface **CC Only**, Thru MIDI
Control **E+M**, USB1 and E16 TRS-input MIDI Thru. Monolit connects to E16
by TRS while E16 remains connected to Move by USB. E16 and Thru may start
in either order; initial synchronization reads the project without replacing
its values.

The Remote example is **controllers/oxi-e16** and the native route is
**controllers/oxi-e16-monolit**. Build their settings with
`npm run controllers:generate`; package Remote with
`npm run controllers:package` and the native E16 + Monolit bundle with
`npm run controllers:monolit:package`.

The **MIDI** manual chapter covers setup, A/B layout, mode
handling, the page reference, reading values, connection checks and mapping
another controller. The complete NRPN address/range table is in
`controllers/midi/parameters.csv`.

These are draft release notes for local testing. Device installation, listening
and E16 ring acceptance remain part of release preparation. The public
installer remains 0.29.1.
