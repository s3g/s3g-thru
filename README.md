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
- includes Motion, Lanes, Grains, transient-aware Cutups, direct Sample, and
  pad-addressed Slices playback algorithms;
- detects up to 64 transient regions per lane outside the audio callback and
  draws their boundaries over the selected waveform;
- auditions WAV files while browsing, before they replace a pattern lane;
- offers Keys, Slices, eight Cue, and held Roll pad modes plus Manual, Cycle,
  Random, No Repeat, and Velocity four-lane variation;
- plays directly from Move's 32 pads with no track MIDI routing;
- lights each free-play pad only while it is held;
- uses Track 1-4 for lane selection and all 16 Step keys as performance-clip
  slots;
- keeps velocity out of the gain stage; poly pressure controls continuous stack
  depth, while Velocity lane variation can deliberately choose a discrete lane;
- plays eight notes polyphonically, with an independent engine and playhead for
  each active voice;
- accepts a fixed external MIDI CC map for every engine/global parameter;
- recalls stereo captures, 64 performance clips, chains, recorded pressure and
  parameter gestures, and engine settings on the next launch;
- exposes Engine, selected-engine pages, Mix / Rec, Sample, Performance Loop,
  and Chain as padded name/value cells;
- records free-time polyphonic performances rather than quantizing them to a
  step grid, with Replace and Overdub modes and continuously variable
  0.25x–4x loop speed;
- preserves each note's recorded onset, duration, strike velocity, source pad,
  poly pressure, and overlapping voice identity;
- assigns a stable color to each of the eight DSP voice/playhead slots and
  shows that color on its source pad;
- has a separate output Level control; and
- defaults input monitoring to disabled, so the raw input is not passed through
  unless external MIDI CC 51 explicitly enables it.

The composition layer has four banks (A-D) of 16 free-time performance clips.
A 16-slot chain provides clip order and repeat counts, advancing at recorded
loop boundaries. Live knob motion and per-pad pressure are captured with the
notes. Host/Move transport synchronization remains a later milestone; v0.17.0
uses each clip's own recorded duration.
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
select performance-clip banks A–D; **Shift + Up/Down** is an alternate gesture.
The 16 Step keys select the 16 clips in that bank from the waveform or
parameter view. **Menu** opens the
parameter pages; **Up/Down** then changes pages. Only the page for the selected
playback engine is included. Each screen cell is aligned with one of the eight
physical dials and shows a padded parameter name with its value underneath—no
small dial graphic. **Shift + Back** restores the last destructive clip,
pattern, source, or sample edit.
**Shift + Menu** shows compact help. **Back** closes a secondary page, or exits
from the waveform page. Schwung's universal
**Shift + volume touch + jog-click** escape remains available.

## Record a performance loop

1. Press **Record** to start free-time performance capture in the active clip.
   Notes already held at that moment are included from the beginning of the
   recording (or from the current loop position when overdubbing).
2. Play pads, vary their pressure, and turn engine dials.
3. Press **Record** again. The exact-duration performance immediately loops.
4. Press **Play** to stop or restart loop playback.

The Performance Loop page selects `REPLACE` (clear the old event data and set a
new duration) or `OVERDUB` (retain the duration and merge new notes/gestures),
and sets Loop Rate from 0.25x to 4x. Turning a lockable engine parameter during
recording writes a continuous gesture point; pressure is recorded separately
for each pad/voice. The thin line below the waveforms shows loop position.

Use **Left/Right** for banks A–D and press a Step key to select or queue one of
its 16 clips. White slots contain data or sample assignments, orange is the
stopped active clip, green is the playing clip, red is recording, and
blue-violet is queued. **Shift + Step** copies the current clip and its sample
assignments. **Delete + Step** clears only that slot's performance events; it
does not delete its samples. Clearing is undoable.

## Record the first audio lane

1. Use headphones, select Move's microphone or line input, and set its hardware
   input gain.
2. Open S3G Thru from the Overtake menu.
3. Press **Track 1-4** to choose the lane to replace.
4. Hold **Shift** and press **Record** (or Shift+jog-click) to begin recording;
   repeat the gesture to stop.
5. Play the pads immediately.

The waveform is computed incrementally after recording stops. `SCAN` may appear
briefly. `REC` changes to `LIM` if the capture safety ceiling engaged.

Choose Input or Resample with `REC SOURCE` on the Mix / Rec page. Hold **Shift**
and press **Delete** to clear the selected lane. Monitor stays at zero by
default and is also available on Mix / Rec.

## Load a lane from Move's sample library

Hold **Shift** and press **Track 1-4** to open the browser for that lane. It
starts at `/data/UserData/UserLibrary/Samples`. Turn the main encoder (or use
Up/Down) to select a folder or WAV, click the encoder to enter/load, and press
Back to move up or leave the browser. Highlighting a WAV auditions it through
Preview Level; clicking promotes that decoded preview into the selected lane.

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
`captures/`; controls are in `thru-state.json`; performance notes, gesture
points, durations, rates, chains, source ownership, and engine settings are in
`thru-pattern.bin`. Files are written through temporary files and renamed only
after a complete write.

Use **Back** from the main waveform page for a normal save-and-exit. Autosave
narrows the power-loss window, but power can still be removed before the next
maintenance pass. Uninstalling the module
can also remove its saved bank, so copy those files elsewhere before an
uninstall or future clean reinstall.

Version 0.17.0 deliberately starts a new pre-release persistence format.
Earlier step-pattern and compatibility-era state files are not imported. Move
or delete an older `thru-pattern.bin` and `thru-state.json` before first
launch if they are present; stereo WAV captures can be copied into the User
Library and loaded again.

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
selected engine and edits its principal mode. Sample exposes direct playback;
Slices addresses regions from the 32 pads.
Engine selection is dial 1 on the Engine parameter page; Step keys no longer
change the engine.

## Performance clips, overdub, and chains

Each of the 64 A01–D16 slots stores a variable-duration performance clip plus
base engine settings and four lane-source ownership records. A clip can contain
up to 1,024 note events and 8,192 pressure/parameter points over a maximum of
120 seconds. Recording is not quantized: overlapping notes retain their exact
onset and hold time, velocity, pitch, and physical source-pad identity.

This allows the two isomorphic pads for the same pitch to create independent,
overlapping voices. Active DSP voice slots 1–8 use red, orange, yellow, green,
azure, blue-violet, purple, and magenta respectively. The color appears on the
event's source pad, matching that slot's playhead and stack marker on the
waveform display. Free play remains live while a clip runs and shares the same
eight-voice pool.

Replace starts a blank event recording and determines the new loop duration
when Record is pressed again. Overdub follows the active loop phase and merges
new events without changing its duration. Parameter and pressure paths are
linearly interpolated around the loop seam, following the continuous recorder
behavior used by the S3G Daisy instruments. Playback Rate scales the entire
performance—including note timing and gesture motion—from 0.25x to 4x.

Hold **Shift** and press **Play** for a compact overview of the current 16-slot
bank; Track 1–4 changes banks in that view. The Step row remains the clip
launcher in either view. A requested clip with different sample sources is
prepared on a worker and changes at the current loop boundary; if preparation
is incomplete, the current clip takes another pass rather than mixing banks.

The Chain page edits a 16-slot clip list. Choose a slot, its A01–D16 clip, and a
repeat count from 1–16, then enable Chain. The chain advances only on complete
recorded-loop boundaries and only after the next clip's sample bank is ready.
Manual clip selection disables Chain so it cannot silently override the request.
Turn Clear Slot on the Chain page to remove the selected chain entry.

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
| 54 | Tempo 40–240 BPM | 65 | Chain Off/On |
| 55 | Tempo swing 50–75% | 66 | Chain slot 1–16 |
| 56 | Performance playback Off/On | 67 | Chain pattern A01–D16 |
| 57 | Undo trigger | 68 | Chain repeats 1–16 |
| 58 | Clear selected chain slot |  |  |
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
| 107 | Cut region source Equal/Transient | 108 | Sample play mode |
| 109 | Slice source Equal/Transient | 110 | Equal slice count |
| 111 | Pad mode Keys/Slices/Cues/Rolls | 112 | Lane variation |
| 113 | Browser preview level | 114 | Performance loop rate |
| 115 | Replace/Overdub | 116 | Performance record toggle |
| 117 | Clear active performance |  |  |

CC 51 can create acoustic feedback when a microphone and speakers are active;
leave it at `0` unless direct monitoring is intentional. Internal Move button
CCs are excluded from this mapping and continue to operate the takeover UI.

## Engine settings

Press **Menu** to open the parameter surface. The eight physical knobs edit the
eight aligned name/value cells. Use **Up/Down** to move through contextual
pages. Only the pages for the Algorithm chosen on the Engine page are present:
Motion and Lanes each have two engine pages, Grains has three, Cutups has two,
and Sample and Slices each have one.

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
  variation, gate and join, equal or detected transient regions, repeats,
  reverse/pitch/level
  variation, seven discrete lane orders, and four polyphonic lane paths.
- Sample: direct forward, reverse, loop, or ping-pong playback of a discrete
  lane. Slices uses the same modes with equal or detected transient regions;
  source-pad identity is retained by the performance looper.
- Performance: Keys is the chromatic grid; Slices maps the 32 pads to regions;
  Cues uses pads 1–8 (Shift+pad stores the current playhead, Delete+pad clears,
  pad jumps); Rolls turns held pads into tempo-divided Cutups voices.
- Mix / Rec: four independent lane gains, Input/Resample source, record gain,
  monitor level, and browser preview level.
- Sample edit: trim start/end and apply, reverse, normalize, edge fade, record gain,
  and output level.
- Performance Loop: playback rate, Replace/Overdub, pressure behavior, root
  note, and the four primary Chain controls.
- Chain: enable, slot, clip, repeats, root note, algorithm, join, and level.

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
Direct Sample and Slices can select lanes manually or by Cycle, Random, No
Repeat, or Velocity variation. Pressure remains a separate continuous stack
gesture for the motion-family engines.

## Safety and current limits

- Monitor defaults to `0`; enable it deliberately on Mix / Rec or with CC 51.
- Capture gain is adjustable from the Mix / Global settings page and is
  followed by a 0.95 safety ceiling. `LIM` means that ceiling reduced at least
  one stored sample.
- Pad velocity does not attenuate playback. Use knob 8 for loudness.
- One instance allocates about 46 MiB for its audio banks, decoder staging,
  and bounded 64-clip event storage.
- Playback uses eight independent voices. Voice stealing prefers a released
  voice, then the oldest held voice when all eight are occupied.
- Each playback voice has 16 bounded grain slots (128 across eight voices).
  Eight simultaneous dense Grains voices are the highest expected DSP load and
  must be checked on Move before this build is considered accepted.
- The desktop Grains Sorter process and Motion segment-event router are not in
  this hardware-bounded pass. Motion Bounce remains an articulation rather
  than a separate event router. Transient maps are automatic and bounded to 64
  regions; manual marker editing is not yet included.
- Persistent WAV and metadata writes run only from non-audio maintenance or
  teardown calls, never in the audio callback.
- User-library WAV decoding/resampling and queued-clip preparation run on a
  worker thread. A prepared bank is swapped at a block/loop boundary.
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

