# S3G Thru for Ableton Move

S3G Thru is a four-lane live sampler for Ableton Move. It carries a fixed-
memory adaptation of the S3G Sample family into a native
[Schwung](https://github.com/charlesvestal/schwung) **overtake module** that
owns Move's pads, display, LEDs, and recording gestures while it is open.

It is intentionally a new instrument rather than a Sample Neon variant.

## What this installable does

- records Move's selected stereo input into any of four lanes;
- loads mono or stereo WAV files from Move's User Library into any lane;
- provides 12 seconds of stereo recording per lane at 44.1 kHz;
- replaces only the selected lane, while other loaded lanes keep playing;
- displays all four cached waveforms, the active field, active-voice playheads,
  per-voice stack markers, lane weights, recording meter, and selected lane on
  Move's 128x64 display;
- moves continuously through the lane stack;
- includes Motion, Lanes, Grains, and Cutups playback algorithms;
- plays directly from Move's 32 pads with no track MIDI routing;
- lights a free-play pad only while it is held; sequencer note entry latches
  pads until their note-off Step is chosen;
- uses Track 1-4 for lane selection and all 16 Step keys for sequencing;
- uses note velocity and poly pressure for stack depth, never sample volume;
- plays eight notes polyphonically, with an independent engine and playhead for
  each active voice;
- accepts a fixed external MIDI CC map for every engine/global parameter;
- recalls stereo captures, 64 patterns, chains, step expression, parameter
  locks, and engine settings on the next launch;
- exposes Engine, multiple selected-engine pages, Mix / Rec, Sample, Sequence,
  and Chain as padded name/value cells;
- runs a 1–64-step internal sequencer in four 16-Step pages with tempo, swing,
  explicit note boundaries, probability, ratchets, microtiming, and DSP-side
  parameter locks;
- sustains each note until its chosen later Step, allowing notes in a chord to
  have different lengths and overlapping instances of the same pitch;
- assigns a stable color to each of the eight DSP voice/playhead slots and
  shows that color on its source pad;
- has a separate output Level control; and
- defaults input monitoring to disabled, so the raw input is not passed through
  unless external MIDI CC 51 explicitly enables it.

The composition layer has four banks (A-D) of 16 patterns. Up to eight notes
can be assigned to each step, with free play remaining available at the same
time. A 16-slot chain provides pattern order and repeat counts. Live knob
motion can be written as step automation, and recent free play can be captured
back into the sequence. Host/Move transport synchronization remains a later
milestone; v0.14.0 uses its internal clock.
Eight-voice realtime CPU behavior, particularly with the Grains engine, still
needs acceptance on Move.

## Requirements

- Ableton Move with Schwung 1.7.3 or newer;
- macOS build: Homebrew's `aarch64-unknown-linux-gnu` toolchain;
- Node 18 or newer for syntax validation.

Install the documented cross-compiler on macOS with:

```sh
brew tap messense/macos-cross-toolchains
brew trust --formula messense/macos-cross-toolchains/aarch64-unknown-linux-gnu
brew install aarch64-unknown-linux-gnu
```

## Build and test

```sh
npm test
npm run validate
npm run build
```

The package is written to:

```text
dist/s3g-thru-module.tar.gz
```

It contains one `s3g-thru/` directory with `module.json`, `ui.js`,
`dsp.so`, the license, and third-party notices. Packaging refuses a binary
that is not an AArch64 Linux ELF shared object.

## Install on Move

1. Back up Move and install or update Schwung 1.7.3 or newer.
2. If version 0.1 is installed as a sound generator, uninstall it first and
   restart Move. Changing component type moves the same module ID to a different
   Schwung directory, so this one-time cleanup avoids two stale copies.
3. Open `http://move.local:7700`.
4. Choose **Modules -> Install Custom Module -> Upload tarball**.
5. Upload `dist/s3g-thru-module.tar.gz`.

Building this repository does not modify Move; uploading the package is a
separate action.

## Open and play

1. Hold **Shift**, touch the master-volume knob, and jog-click to open the
   Overtake menu.
2. Select **S3G Thru**.
3. Press the pads. They play Thru directly; no Move MIDI Out or Schwung Signal
   Chain slot is involved.

Use **Up/Down** to transpose the pad grid by octaves. Use **Left/Right** to
select Step pages 1–16, 17–32, 33–48, or 49–64; **Shift + Up/Down** is an
alternate gesture. Both work from the waveform and parameter views. The
current page is shown as `P1`–`P4` in the waveform header. **Menu** opens the
parameter pages; **Up/Down** then changes pages. Only the page for the selected
playback engine is included. Each screen cell is aligned with one of the eight
physical dials and shows a padded parameter name with its value underneath—no
small dial graphic. **Shift + jog-click** commits recent free play to the
sequence, and **Shift + Back** restores the last destructive pattern edit.
**Shift + Menu** shows compact help. **Back** closes a secondary page, or exits
from the waveform page. Schwung's universal
**Shift + volume touch + jog-click** escape remains available.

## Record the first lane

1. Use headphones, select Move's microphone or line input, and set its hardware
   input gain.
2. Open S3G Thru from the Overtake menu.
3. Press **Track 1-4** to choose the lane to replace.
4. Press **Record** or jog-click to begin recording; press it again to stop.
5. Play the pads immediately.

The waveform is computed incrementally after recording stops. `SCAN` may appear
briefly. `REC` changes to `LIM` if the capture safety ceiling engaged.

Hold **Shift** and press **Record** to switch between Move's input and resampling
Thru's current output. Hold **Shift** and press **Delete** to clear the selected
lane. Monitor stays at zero in this direct first pass.

## Load a lane from Move's sample library

Hold **Shift** and press **Track 1-4** to open the browser for that lane. It
starts at `/data/UserData/UserLibrary/Samples`. Turn the main encoder (or use
Up/Down) to select a folder or WAV, click the encoder to enter/load, and press
Back to move up or leave the browser.

The selected lane may therefore contain either a pattern-local stereo capture
or a library sample. Loading replaces only that lane. Mono/stereo PCM 16/24/32-bit
and 32-bit float WAV files are supported; other sample rates are resampled to
Move's rate and material longer than 12 seconds is truncated. File decoding is
done on a background worker. The waveform footer shows ownership and source.

Every pattern lane has explicit ownership: `INHERIT`, `FILE`, `CAPTURE`, or
`EMPTY`; the display additionally reports `LOADING` and `MISSING`. `INHERIT`
resolves through numerically preceding patterns. Recording assigns a
pattern-local file under `captures/`; loading assigns a User Library path; and
clearing creates an intentional `EMPTY` lane. Queued-pattern sources are
decoded into a second four-lane bank and swapped together at a pattern
boundary. If preparation is incomplete, the current pattern plays for another
cycle instead of mixing old and new lane sources.

The Sample page provides non-destructive source selection followed by explicit
editing actions: normalized Start/End trim, Trim Apply, Reverse, Normalize, and
a short edge fade. Select the lane with a Track button, then turn the relevant
Sample-page encoder in either direction. `TURN` is the ready state; a successful
edit temporarily replaces the page title with `NORMALIZED L1`, `REVERSED L1`,
`TRIMMED L1`, or `FADED L1`. Edits alternate between two pattern-local capture files, so
one-level Undo can return to the previous file or capture without that audio
having been overwritten.

## Saving and recall

Thru periodically autosaves changed metadata and completed stereo captures,
then performs a final save on normal exit. Current-bank recovery WAVs remain
`thru-lane-1.wav` through `thru-lane-4.wav`; pattern takes live under
`captures/`; controls are in `thru-state.json`; patterns, chains, notes,
timing, step expression, locks, source ownership, and engine settings are in
`thru-pattern.bin`. Files are written through temporary files and renamed only
after a complete write.

Use **Back** from the main waveform page for a normal save-and-exit. Autosave
narrows the power-loss window, but power can still be removed before the next
maintenance pass. Uninstalling the module
can also remove its saved bank, so copy those files elsewhere before an
uninstall or future clean reinstall. Pattern files from versions 1–7 are
migrated into the v8 format when loaded; v8 adds the high half of the
64-parameter lock mask and preserves older locks and base values.

## Performance controls

The main waveform page carries eight knobs:

| Knob | Control |
|---|---|
| 1 | Position |
| 2 | Field width |
| 3 | Speed |
| 4 | Stack position |
| 5 | Stack scan rate |
| 6 | Join/crossfade |
| 7 | Current engine mode |
| 8 | Output level |

Track buttons 1-4 select the recording lane. In Free Play, dial 7 follows the
selected engine and edits Motion Mode, Lanes Mode, Grain Mode, or Cutups Mode.
Engine selection is dial 1 on the Engine parameter page; Step keys no longer
change the engine.

## Patterns, note boundaries, and parameter locks

Hold **Shift** and press **Play** to open Pattern mode. Track 1-4 select banks
A-D and the 16 Step keys address that bank's patterns. A normal Step press
prepares the complete sample bank and loads it at the next safe block while
stopped, or at a pattern boundary while playing. White Steps contain patterns,
orange is current, and blue-violet is prepared/queued. Hold **Shift** and press
a Step to copy the current
pattern into that slot; hold **Delete** and press a Step to clear it. Press
**Shift + Play** again or Back to return.

Each pattern stores up to 64 Steps, explicit note ends, probability, ratchets,
microtiming, parameter locks, length, swing, base engine settings, and four
lane-source ownership records.
BPM is global to the four-bank set, so pattern changes do not change tempo.

Outside Pattern mode, tap a Step to select a note-on
boundary; the selected Step is orange. Then tap up to eight pads. Each pad is
auditioned normally, added at that Step with its strike velocity, and remains
orange after release. Tap a later Step to move the edit cursor; every active
note stays lit. At that Step, tap each lit pad that should turn off. Its light
goes out and its individual note-off boundary is written. Other lit notes keep
holding and can be ended at still later Steps. Tapping an active pad on its
starting Step makes a full-pattern hold. Note boundaries can cross Step pages;
use **Left/Right** without losing the lit pending note. Tap a selected Step again when no notes
are pending to leave note-entry mode. To remove everything programmed on one
Step, hold **Delete first**, then press that Step; this clears its notes,
unfinished note boundaries, and locks together.

Each note event keeps the physical source-pad identity as well as pitch. This
allows the two isomorphic pads for the same pitch to create independent,
overlapping voices. Active DSP voice slots 1–8 use red, orange, yellow, green,
azure, blue-violet, purple, and magenta respectively. The color appears on the
event's source pad, matching that slot's playhead and stack marker on the
waveform display. One physical LED can show only one color if the exact same
pad is retriggered before its earlier event ends.

Press **Play** to start or stop the pattern. Note-start Steps are white and the
currently playing Step is cyan. Free Play remains live while the pattern runs,
sharing the same eight-voice pool.

Hold a Step and turn any lockable physical dial to store a parameter lock. The
lock is applied to a DSP-side control snapshot when that Step starts its notes,
so later global edits do not alter an already-triggered voice. Hold the Step
and press **Delete** to clear all of its locks without removing its notes.
The order is intentional: **Step then Delete** clears locks only, while
**Delete then Step** clears the complete sequence Step.

The Sequence page provides Tempo, Swing, Length, Step Page, Automation Record,
and expression for the selected Step: Probability, Ratchets, and Microtiming.
**BPM is knob 1** and ranges from 40–240. Note duration comes only
from the note-on and note-off Steps for newly entered events; it does not depend
on a Gate percentage. The older Gate state and MIDI CC 57 remain readable for
loading version 1–3 patterns and controlling their legacy note entries. Tempo
is currently internal; Move/host transport synchronization is not yet
implemented.

When Automation Record is enabled, turning a lockable main or engine-page dial
while the pattern runs writes its value to the current Step. External MIDI CC
20–45 and 69–106 record through the same path. Free-play notes are retained in a rolling
buffer while the sequence runs; **Shift + jog-click** commits them to their
quantized Steps. **Shift + Back** restores the most recent Step, pattern, lane,
or source edit.

The Chain page edits a 16-slot pattern list. Choose a slot, its A01–D16 pattern
number, and a repeat count from 1–16, then enable Chain. Play advances only
after the next pattern's complete source bank is ready; manual pattern selection
leaves Chain mode so it cannot silently override the requested pattern.

Strike velocity chooses an offset within the moving lane-stack cycle and poly
aftertouch moves that offset while the note is held. The lane scan continues
moving; holding a pad no longer switches the path to a fixed manual lane.
Neither velocity nor pressure changes output amplitude. Each of the eight voices
has its own playback phase, pitch, release envelope, stack offset, and on-screen
playhead. A voice's playhead disappears on note-off even though its short audio
release continues; with no held voices, no playhead is drawn.

Each vertical playhead carries a small hollow square showing that voice's
continuous position through the four-lane stack. A square centered on a lane
means that lane is dominant; a square between rows shows a crossfade between
adjacent lanes. The waveform is briefly cleared behind the outline so up to
eight markers remain legible on the monochrome display.

On the waveform page, each row now separates its lane number, activity fader,
and waveform with dedicated padding so the three indicators remain legible.

## External MIDI map

S3G Thru accepts notes, note-off, polyphonic aftertouch, All Sound Off (CC 120),
and All Notes Off (CC 123) from Move's external/host MIDI path. All parameter
mapping is 7-bit absolute: `0` selects the minimum/first choice and `127`
selects the maximum/last choice. The map is intentionally fixed so a
programmable controller such as the LightReft Monolit can recall it without a
MIDI-learn file.

| CC | Parameter | CC | Parameter |
|---:|---|---:|---|
| 20 | Position | 37 | Lane 3 gain |
| 21 | Field width | 38 | Lane 4 gain |
| 22 | Speed | 39 | Grain mode |
| 23 | Manual stack position | 40 | Grain size |
| 24 | Stack scan rate | 41 | Grain density |
| 25 | Algorithm | 42 | Grain pitch |
| 26 | Join/crossfade | 43 | Grain window |
| 27 | Output level | 44 | Grain stereo spread |
| 28 | Motion mode | 45 | Cutups mode |
| 29 | Motion sound | 46 | Pressure Off/Offset |
| 30 | Motion gesture rate | 47 | Root note (24–96) |
| 31 | Lanes mode | 48 | Record lane 1–4 |
| 32 | Stack path | 49 | Record source Input/Resample |
| 33 | Lanes stretch Off/On | 50 | Record gain |
| 34 | Stretch ratio | 51 | Monitor level |
| 35 | Lane 1 gain | 52 | Record gate (>=64 records) |
| 36 | Lane 2 gain | 53 | Clear selected lane (>=64) |
| 54 | Sequence BPM | 56 | Sequence length |
| 55 | Sequence swing | 57 | Legacy sequence gate 5–800% |
| 58 | Step page 1–4 | 59 | Undo trigger |
| 60 | Selected-Step probability | 61 | Selected-Step ratchets |
| 62 | Selected-Step microtiming | 63 | Automation record Off/On |
| 64 | Commit performance Capture | 65 | Chain Off/On |
| 66 | Chain slot 1–16 | 67 | Chain pattern A01–D16 |
| 68 | Chain repeats 1–16 |  |  |
| 69 | Motion travel | 88 | Grain timing scatter |
| 70 | Motion jitter | 89 | Grain envelope skew |
| 71 | Packet duty | 90 | Grain process |
| 72 | Motor rate | 91 | Grain process amount |
| 73 | Motor symmetry | 92 | Grain regions |
| 74 | Motor shape | 93 | Grain source advance |
| 75 | Lane transition | 94 | Cut clock Free/Tempo |
| 76 | Lane path shape | 95 | Cut clock division |
| 77 | Lane path offset | 96 | Free cut rate |
| 78 | Lane path skew | 97 | Cut timing variation |
| 79 | Lane path curve | 98 | Cut gate |
| 80 | Lane transition slew | 99 | Cut join time |
| 81 | Grain position spray | 100 | Cut regions |
| 82 | Grain spray bias | 101 | Cut repeats |
| 83 | Grain pitch spray | 102 | Cut reverse chance |
| 84 | Grain reverse chance | 103 | Cut pitch variation |
| 85 | Grain size variation | 104 | Cut level variation |
| 86 | Grain level variation | 105 | Cut lane order |
| 87 | Grain timing Regular/Scatter | 106 | Cut polyphonic path |

CC 51 can create acoustic feedback when a microphone and speakers are active;
leave it at `0` unless direct monitoring is intentional. Internal Move button
CCs are excluded from this mapping and continue to operate the takeover UI.

## Engine settings

Press **Menu** to open the parameter surface. The eight physical knobs edit the
eight aligned name/value cells. Use **Up/Down** to move through contextual
pages. Only the pages for the Algorithm chosen on the Engine page are present:
Motion and Lanes each have two engine pages, Grains has three, and Cutups has
two.

- Engine: algorithm, position, field, speed, stack position, scan rate, join,
  and level.
- Motion Path: eight trajectories including Drunk, travel, jitter, position,
  field, speed, join, and level. Motion Sound adds packet duty plus independent
  motor rate, symmetry, and shape.
- Lanes Path/Shape: eight stack paths including Random, Crossfade/Jump,
  breakpoint shape, offset, skew, curve, transition slew, stretch, and ratio.
- Grains Main/Cloud/Process: 16 grains per playback voice; source mode, size,
  density, independent grain pitch, six windows, stereo spread, position and
  pitch spray, directional bias, reverse/size/level variation, regular or
  scattered timing, envelope skew, slice regions, scan/per-grain advance, and
  Ordinary, Stutter, Shrink, or Doublets processing.
- Cutups Clock/Path: fixed or tempo clock with musical divisions, timing
  variation, gate and join, 1–64 regions, repeats, reverse/pitch/level
  variation, seven discrete lane orders, and four polyphonic lane paths.
- Mix / Global: four independent lane gains, pressure behavior, root note,
  record gain, and output level.
- Sample: trim start/end and apply, reverse, normalize, edge fade, record gain,
  and output level.
- Sequence: tempo, swing, length, Step page, automation record, probability,
  ratchets, and microtiming.
- Chain: enable, slot, pattern, repeats, root note, algorithm, join, and level.

Press **Back** once to return to the waveform page without exiting.

Motion Mode always owns the read-head trajectory. Mirror runs to the far edge
and back; Zigzag alternates forward, backward, then forward segments; Drunk
interpolates between deterministic random targets. Motion
Sound (Continuous, Packets, Motor, or Bounce) shapes the sound without replacing
that trajectory, so the audio reader and displayed playhead retain the selected
direction changes.

Note pitch follows the Sample-family semantics. Motion and Lanes notes retune
their transport. Grains notes change source scan speed while Grain Pitch stays
independent. Cutups notes multiply the base Speed/pitch of each cut reader while
the cut clock stays on its free rate or tempo division. In a stack, every cut chooses a discrete lane
and source region; it does not read the continuously crossfaded Lanes mix.

## Safety and current limits

- Monitor defaults to `0` and is not on the Move UI; external CC 51 is the only
  direct monitor control.
- Capture gain is adjustable from the Mix / Global settings page and is
  followed by a 0.95 safety ceiling. `LIM` means that ceiling reduced at least
  one stored sample.
- Pad velocity does not attenuate playback. Use knob 8 for loudness.
- One instance allocates about 37 MiB for its active bank, prepared-pattern
  bank, and one-lane decoder staging area.
- Playback uses eight independent voices. Voice stealing prefers a released
  voice, then the oldest held voice when all eight are occupied.
- Each playback voice has 16 bounded grain slots (128 across eight voices).
  Eight simultaneous dense Grains voices are the highest expected DSP load and
  must be checked on Move before this build is considered accepted.
- Transient-derived Cutups regions, the desktop Grains Sorter process, and the
  desktop Motion segment-event router are not in this hardware-bounded pass;
  Cutups currently uses equal regions and Motion Bounce remains an articulation.
- Persistent WAV and metadata writes run only from non-audio maintenance or
  teardown calls, never in the audio callback.
- User-library WAV decoding/resampling and queued-pattern preparation run on a
  worker thread. A prepared bank is swapped at a block/pattern boundary.
- The Help screen reports maximum render time in microseconds and deadline
  overruns since launch; these are evidence aids, not substitutes for listening.
- WAV is the supported library format; AIFF/AIF requires conversion before use.
- Move hardware remains the acceptance authority; desktop tests cannot certify
  input routing, control feel, display cadence, or realtime CPU margin.

See [docs/HARDWARE_TEST.md](docs/HARDWARE_TEST.md) for the first device pass.

## Repository layout

```text
include/sample_motion_app.h  Fixed-memory Sample Motion engine
include/thru_schwung_abi.h   Minimal Schwung API v2 declaration
src/dsp/thru_module.cpp      Move wrapper, recording, MIDI, waveform cache
src/ui.js                    Overtake controls, LEDs, and waveform screen
src/module.json              Schwung overtake manifest
tests/                       Device-free ABI and DSP regressions
scripts/                     Test, cross-build, validate, and package tools
```

