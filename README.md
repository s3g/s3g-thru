# s3g Thru for Ableton Move

s3g Thru is a four-lane live sampler for Ableton Move. It carries a fixed-
memory adaptation of the s3g Sample family into a native
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
- includes Motion with continuous, packet, motor, and bounded segment sounds;
  Lanes; Grains with energy sorting; transient-aware Cutups with non-repeating
  random cycles; direct Sample; pad-addressed Slices; per-voice Glitch repeat
  buffers; and Weave, a long-grain approach with one to four sustained
  250 ms–12 s layers;
- adds a clip-local post-engine chain with tone, drive, bit/rate reduction,
  and stereo cross-feedback delay;
- detects up to 64 transient regions per lane outside the audio callback and
  draws their boundaries over the selected waveform;
- prepares loaded-sample and queued-clip waveforms on the background worker,
  with display-only normalization that leaves audio gain unchanged;
- auditions WAV files while browsing, before they replace a clip lane;
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
- retains a provisional external MIDI CC map for the existing engine and global
  controls;
- recalls stereo captures, 64 performance clips, chains, recorded pressure and
  parameter gestures, and engine settings on the next launch;
- exposes Engine, selected-engine pages, Mix / Rec, Effects, Sample,
  Performance Loop, Overdub Takes, and Chain as padded name/value cells;
- records free-time polyphonic performances rather than quantizing them to a
  step grid, with Replace, layered undoable Overdub takes, and continuously
  variable 0.25x–4x loop speed;
- stores each clip's capture BPM and exact beat length, with Free or Beat Sync
  playback plus Off/Beat/Bar launch quantization;
- exposes 4-, 8-, and 16-grain quality levels for trading density against
  Move's realtime CPU margin;
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
notes. Beat Sync follows Thru's internal BPM while preserving the captured
microtiming; synchronization to Move's native transport remains a later
milestone.
Eight-voice realtime CPU behavior, particularly with the Grains and Long
Grains engines, still needs acceptance on Move.

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

## Manual website

The illustrated manual starts at [`docs/index.html`](docs/index.html). Its Move
screen figures are generated from the actual UI drawing calls rather than
redrawn by hand:

```sh
npm run docs:render
npm run docs:check
npm run docs:serve
```

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
2. Select **s3g Thru**.
3. Press the pads. They play Thru directly; no Move MIDI Out or Schwung Signal
   Chain slot is involved.

Use **Up/Down** to transpose the pad grid by octaves. Use **Left/Right** to
select performance-clip banks A–D; **Shift + Up/Down** is an alternate gesture.
The 16 Step keys select the 16 clips in that bank from the waveform or
parameter view. **Shift + Play** cycles through the clip overview, the 16-slot
chain editor, and the main display. **Menu** opens the
parameter pages; **Up/Down** then changes pages. Only the page for the selected
playback engine is included. Each screen cell is aligned with one of the eight
physical dials and shows a padded parameter name with its value underneath—no
small dial graphic. **Loop** holds every active voice at its current stack
position, including an in-between lane crossfade; press it again to resume each
voice's independent scan from that point. Releasing and pressing the same pad
while Loop remains held recalls that pad's frozen position; returning to Run
clears the retained positions. During performance recording, Loop's
Hold/Run changes and each held voice's exact stack position are stored in the
clip. A recorded hold is recalled even after the physical Loop hold is released.
In Glitch, hold **Shift + Loop** for
a temporary repeat-history lock. Tap the chord twice quickly to latch it; press
it once more to release. The Loop key is orange while histories are locked.
**Shift + Back** restores the last destructive clip,
source, or sample edit.
**Shift + Menu** shows compact help. **Back** closes a secondary page, or exits
from the waveform page. Schwung's universal
**Shift + volume touch + jog-click** escape remains available.

## Record a performance loop

1. Press **Record** to start free-time performance capture in the active clip.
   Notes already held at that moment are included from the beginning of the
   recording (or from the current loop position when overdubbing).
2. Play pads, vary their pressure, turn engine dials, and use **Loop** to capture
   Stack Hold/Run gestures. Starting Record while Loop is already held stores
   every active voice's current lane or between-lane crossfade.
3. Press **Record** again. The exact-duration performance immediately loops.
4. Press **Play** to stop or restart loop playback.

The Performance Loop page selects `REPLACE` (clear the old event data and set a
new duration) or `OVERDUB` (retain the duration and merge new notes/gestures),
and sets Loop Rate from 0.25x to 4x. `FREE` retains the recorded wall-clock
duration; `BEAT SYNC` scales it from the BPM captured with the clip to the
current BPM. Launch can be immediate or wait for the next internal beat/bar.
Turning a lockable engine parameter during recording writes a continuous
gesture point; pressure and held stack position are recorded separately for
each pad/voice. Loop Hold/Run is stored as stepped clip data. The thin line
below the waveforms shows loop position.

Replace establishes take 1 as the protected base performance. Every completed
Overdub pass that contains a note or gesture becomes another clip-local take;
an empty pass does not consume a take. The Overdub Takes page displays the
current count. Turn `UNDO TAKE`, or hold **Delete** and press **Record**, to
remove the latest overdub's notes and gestures. Repeat to peel back takes in
reverse order. The base take is never removed by Undo Take; use Replace or
Clear Performance when a new blank foundation is intended.

Each clip has a fixed capacity of 1,024 notes and 8,192 pressure/parameter
points. Gesture capture progressively thins nearly identical points as the
buffer fills. At capacity the latest endpoint for an existing gesture is still
updated, playback continues, and the display reports `CLIP FULL` with the note
and gesture counts.

Use **Left/Right** for banks A–D and press a Step key to select or queue one of
its 16 clips. White slots contain data or sample assignments, orange is the
stopped active clip, green is the playing clip, red is recording, and
blue-violet is queued. **Shift + Step** copies the current clip and its sample
assignments. **Delete + Step** resets that complete clip slot, including its
sample assignments. Reset is undoable.

## Record the first audio lane

1. Use headphones, select Move's microphone or line input, and set its hardware
   input gain.
2. Open s3g Thru from the Overtake menu.
3. Press **Track 1-4** to choose the lane to replace.
4. Hold **Shift** and press **Record** (or Shift+jog-click) to begin recording;
   repeat the gesture to stop.
5. Play the pads immediately.

The waveform is computed incrementally after recording stops. `SCAN` may appear
briefly; loaded-library and prepared clip waveforms arrive from their background
load already cached. Display normalization makes quiet material legible without
changing playback level. `REC` changes to `LIM` if the capture safety ceiling
engaged.

Choose Input or Resample with `REC SOURCE` on the Mix / Rec page. Hold **Shift**
and press **Delete** to clear the selected lane. Monitor stays at zero by
default and is also available on Mix / Rec.

## Load a lane from Move's samples or Thru Recordings

Hold **Shift** and press **Track 1-4** to open the browser for that lane. It
opens a source choice: **Move Samples** enters
`/data/UserData/UserLibrary/Samples`, while **Thru Recordings** lists the
module's saved captures newest first. Turn the main encoder (or use Up/Down) to
select a source, folder, or WAV, click the encoder to enter/load, and press Back
to move up or leave the browser. Highlighting a WAV auditions it through
Preview Level; clicking promotes that decoded preview into the selected lane.

The selected lane may therefore contain either a clip-local stereo capture
or a library sample. Loading replaces only that lane. Mono/stereo PCM 16/24/32-bit
and 32-bit float WAV files are supported; other sample rates are resampled to
Move's rate and material longer than 12 seconds is truncated. File decoding is
done on a background worker. The waveform footer shows ownership and source.

Every clip lane has explicit ownership: `FILE`, `CAPTURE`, or `EMPTY`; the
display additionally reports `LOADING` and `MISSING`. The first time a blank
clip is selected it snapshots the four sources currently in use, after which
its assignments are independent—there is no dynamic previous-clip lookup.
Every completed input recording, resample, or committed sample edit gets a
unique WAV under `captures/`. Recording changes the clip lane's active
assignment but does not overwrite the prior WAV, so one-level **Shift + Back**
can restore the preceding lane source and older recordings remain available in
**Thru Recordings**. Loading assigns a file path and resetting creates
intentional `EMPTY` lanes. Queued-clip
sources are decoded into a second four-lane bank and swapped together at a clip
boundary. If preparation is incomplete, the current clip plays for another
cycle instead of mixing old and new lane sources.

The Sample page provides non-destructive source selection followed by explicit
editing actions: normalized Start/End trim, Trim Apply, Reverse, Normalize, and
a short edge fade. Select the lane with a Track button, then turn the relevant
Sample-page encoder in either direction. `TURN` is the ready state; a successful
edit temporarily replaces the page title with `NORMALIZED L1`, `REVERSED L1`,
`TRIMMED L1`, or `FADED L1`. Each committed edit creates another uniquely named
capture, so one-level Undo can return to the previous file without that audio
having been overwritten.

## Saving and recall

Thru periodically autosaves changed metadata and completed stereo captures,
then performs a final save on normal exit. Current-bank recovery WAVs remain
`thru-lane-1.wav` through `thru-lane-4.wav`; clip takes live under
`captures/`; controls are in `thru-state.json`; performance notes, gesture
points, durations, rates, captured tempo, timing modes, chains, source
ownership, and engine settings are in `thru-clips.bin`. Metadata uses generation
numbers, the binary bank has a checksum, and each successful replacement keeps
one `.bak` generation. On launch, Thru selects the newest matching JSON/binary
generation across the current and backup files, so it does not combine two
individually valid halves of interrupted saves.

The capture archive is append-only during normal use: rerecording, editing,
clearing a lane, or resetting a clip does not delete old WAVs. This makes takes
recoverable and reusable, but disk use grows with recording. Export the set
before manually removing files from the module data directory; a clip that
still refers to a removed WAV will report `MISSING`.

The take- and engine-aware binary bank is format 4. Format-1 through format-3
banks are imported with their original parameter identities intact; newly added
controls receive safe defaults. A format-1 recorded performance becomes its
protected base take. Subsequent saves write format 4 and preserve individual
overdub membership and all lockable engine controls.

Use **Back** from the main waveform page for a normal save-and-exit. Autosave
narrows the power-loss window, but power can still be removed before the next
maintenance pass. Uninstalling the module
can also remove its saved bank, so copy those files elsewhere before an
uninstall or future clean reinstall.

Version 0.18.x deliberately started a new pre-release persistence format.
Earlier step-sequence and compatibility-era metadata is not imported. Move or
delete an older `thru-pattern.bin`, `thru-clips.bin`, and `thru-state.json`
before first launch if present; stereo WAV captures can be copied into the User
Library and loaded again.

To make a portable backup of a module data directory, run:

```sh
scripts/export-set.sh /path/to/s3g-thru-data Thru-set.tar.gz
```

The archive includes current metadata, its recovery generation, saved lane
WAVs, and local captures. User Library WAVs remain external references; after
moving or renaming one, use **Shift + Track** on the reported `MISSING` lane to
select its replacement.

## Performance controls

The main waveform page carries eight knobs:

| Knob | Control |
|---|---|
| 1 | Position |
| 2 | Field width |
| 3 | Speed |
| 4 | Stack start / manual position |
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
new events without changing its duration. Each completed non-empty overdub is
stored as a take, so Undo Take can remove its notes and gestures without
touching the base performance or prior passes. Take membership is saved with
the clip and copied with Shift+Step. Parameter and pressure paths are
linearly interpolated around the loop seam, following the continuous recorder
behavior used by the s3g Daisy instruments. Playback Rate scales the entire
performance—including note timing and gesture motion—from 0.25x to 4x.

Hold **Shift** and press **Play** for a compact overview of the current 16-slot
bank; Track 1–4 changes banks in that view. The Step row remains the clip
launcher in either view. A requested clip with different sample sources is
prepared on a worker and changes at the current loop boundary; if preparation
is incomplete, the current clip takes another pass rather than mixing banks.

The dedicated Chain view edits a 16-slot clip list. Use a Step key to select a
slot, the main encoder to choose its A01–D16 clip, and Up/Down for 1–16 repeats.
Click the main encoder to enable/disable playback, **Shift + click** to insert
the active clip before the selected slot, **Delete + Step** to remove and
compact an entry, or **Shift + Step** to move the selected entry. The screen and
Step LEDs distinguish selected, current, queued, populated, and empty slots.
The chain advances only on complete recorded-loop boundaries and only after the
next clip's sample bank is ready. Manual clip selection disables Chain so it
cannot silently override the request.

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

## Provisional external MIDI map

s3g Thru accepts notes, note-off, polyphonic aftertouch, All Sound Off (CC 120),
and All Notes Off (CC 123) from Move's external/host MIDI path. All parameter
mapping is 7-bit absolute: `0` selects the minimum/first choice and `127`
selects the maximum/last choice. The current map is intentionally fixed so a
programmable controller such as the LightReft Monolit can recall it without a
MIDI-learn file. The new Quality, Clip Timing, and Launch controls are not
mapped while the external-controller design is still being evaluated.

| CC | Parameter | CC | Parameter |
|---:|---|---:|---|
| 20 | Position | 37 | Lane 3 gain |
| 21 | Field width | 38 | Lane 4 gain |
| 22 | Speed | 39 | Grain mode |
| 23 | Stack start / manual position | 40 | Grain size |
| 24 | Stack scan rate | 41 | Grain density |
| 25 | Algorithm | 42 | Grain pitch |
| 26 | Join/crossfade | 43 | Grain window |
| 27 | Output level | 44 | Grain stereo spread |
| 28 | Motion mode | 45 | Cutups mode |
| 29 | Motion sound | 46 | Pressure Off/Offset |
| 30 | Packet rate | 47 | Root note (24–96) |
| 31 | Lanes mode | 48 | Record lane 1–4 |
| 32 | Stack path | 49 | Record source Input/Resample |
| 33 | Lanes stretch Off/On | 50 | Record gain |
| 34 | Stretch ratio | 51 | Monitor level |
| 35 | Lane 1 gain | 52 | Record gate (>=64 records) |
| 36 | Lane 2 gain | 53 | Clear selected lane (>=64) |
| 54 | Tempo 40–240 BPM | 65 | Chain Off/On |
| 55 | Tempo swing 50–75% | 66 | Chain slot 1–16 |
| 56 | Performance playback Off/On | 67 | Chain clip A01–D16 |
| 57 | Undo trigger | 68 | Chain repeats 1–16 |
| 58 | Clear selected chain slot |  |  |
| 59 | Motion path rate basis | 118 | Motion event layers 1–4 |
| 60 | Motion path rate | 119 | Motion event rate |
| 61 | Moving Loop step | 121 | Motion event repeats |
| 62 | Motion seed | 122 | Motion event step |
| 63 | Motion event trigger | 124 | Motion event pitch scatter |
| 64 | Motion event overlap | 125 | Motion event level variation |
| 126 | Motion event interval curve |  |  |
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
pages. Each parameter has one page location. Shared pages keep the same knob
positions across the engines that use them; a small hollow box marks an
unassigned knob position reserved for a future control.

- Engine: algorithm, position, field, speed, stack start, scan rate, join,
  and level.
- Stack Path, shared by Motion, Lanes, Grains, Glitch, and Weave: eight
  stack paths including Random, Crossfade/Jump, shape, offset, skew, curve, and
  transition slew.
- Motion Path: eight trajectories including Drunk, travel, and jitter. Motion
  Sound adds packet duty plus independent motor rate, symmetry, and shape.
- Lanes Play: source direction, stretch, and stretch ratio.
- Grains Main/Cloud/Process: selectable 4, 8, or 16 grains per playback voice;
  source mode, size,
  density, independent grain pitch, six windows, stereo spread, position and
  pitch spray, directional bias, reverse/size/level variation, regular or
  scattered timing, envelope skew, slice regions, scan/per-grain advance, and
  Ordinary, Sorter, Stutter, Shrink, or Doublets processing.
- Cutups Clock/Path: fixed or tempo clock with musical divisions, timing
  variation, gate and join, equal or detected transient regions, repeats,
  reverse/pitch/level
  variation, seven discrete lane orders, and four polyphonic lane paths.
- Direct Play, shared by Sample and Slices: forward, reverse, loop, or
  ping-pong playback, discrete-lane variation, and pad mode. Slice Source adds
  equal or detected transient regions; pad identity is retained by the
  performance looper.
- Glitch Clock/Pattern: five repeat-buffer behaviors, free or tempo clock,
  fragment length, repeats, chance and override depth, plus a rotating
  distributed pulse cycle with seeded jump, pitch, reverse, decay, and
  temporary or latched live-history locking with Shift + Loop.
- Weave/Weave Motion: 250–12,000 ms layer duration with fine control below one
  second, one to four bounded overlapping layers, Scan/Freeze/Cloud/Slice
  origins, Bell/Hann/Triangle/Plateau envelopes, independent Plateau edge time,
  stereo spread, reverse chance, position and pitch spray, duration and level
  variation, layer phase spread, and scan/per-layer source advance.
- Performance: Keys is the chromatic grid; Slices maps the 32 pads to regions;
  Cues uses pads 1–8 (Shift+pad stores the current playhead, Delete+pad clears,
  pad jumps); Rolls turns held pads into tempo-divided Cutups voices.
- Mix / Rec: four independent lane gains, Input/Resample source, record gain,
  monitor level, and browser preview level.
- Effects: tone, soft drive, bit and rate reduction, and stereo delay time,
  feedback, cross-width, and wet mix. All eight values are clip-local and
  recordable as performance-loop gestures.
- Sample edit: trim start/end and apply, reverse, normalize, and edge fade.
- Performance Loop: playback rate, Free/Beat Sync timing, Off/Beat/Bar launch,
  BPM, Replace/Overdub, swing, pressure behavior, and root note.
- Overdub Takes: the current take count in the page title and Undo Take.
  Replace/Overdub remains on Performance Loop. Delete+Record invokes the same
  take-only undo directly from the waveform or parameter view.
- Chain: enable, slot, clip, repeats, and clear slot.

Press **Back** once to return to the waveform page without exiting.

`Stack Start` is the exact lane/crossfade position when Stack Path is Manual.
For a moving path, it is the phase offset assigned to a newly triggered voice;
Scan Rate then advances that voice from the offset, and pressure can move its
offset independently. The Loop-button hold freezes the final audible blend,
including pressure offset, rather than snapping to a numbered lane.

Motion Mode always owns the read-head trajectory. Path Rate can either scale
normal source motion or run in hertz. Moving Loop has an adjustable source
step; Zigzag and Drunk evolve from a repeatable Seed, Travel, and Jitter.
Motion Sound leaves that path Continuous, applies Hann-windowed Packets, nests
packets in a five-shape Motor envelope, or launches bounded Bounce, Freeze,
Iterate, Pulser, and Doublets events. Segment events can follow the event clock,
packet wraps, or path turns and can cut or layer up to four readers per voice.
Only the pages needed by the selected sound appear.

Note pitch follows the Sample-family semantics. Motion and Lanes notes retune
their transport. Grains and Weave notes change source scan speed while
their event Pitch controls stay independent. Grains Sorter divides Field into Regions, ranks those regions from
quietest to loudest on the dominant lane, and blends toward that order with
Process Amount. Cutups notes multiply the base Speed/pitch of each cut reader while
the cut clock stays on its free rate or tempo division; Random Cycle visits every
region once before reshuffling. In a stack, every cut chooses a discrete lane
and source region; it does not read the continuously crossfaded Lanes mix.
Sample and Slices can select lanes manually or by Cycle, Random, No
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
- Each playback voice has 16 bounded grain slots, with Quality limiting active
  grains to 4, 8, or 16 per voice. Start at 8 on Move and use 4 if the Help
  screen's overrun counter rises; 16 is the maximum-density mode to validate.
- Motion segment sounds use a fixed pool of four readers inside each playback
  voice. Start with two layers and raise the limit only while the Help screen's
  overrun counter remains at zero. Grains Sorter measures at most 64 regions
  and caches its ranking outside the per-grain render loop. Transient maps are
  automatic and bounded to 64 regions; manual marker editing is not yet included.
- Persistent WAV and metadata writes run only from non-audio maintenance or
  teardown calls, never in the audio callback.
- User-library WAV decoding/resampling and queued-clip preparation run on a
  worker thread. A prepared bank is swapped at a block/loop boundary.
- The Help screen reports maximum render time in microseconds and deadline
  overruns since launch; these are evidence aids, not substitutes for listening.
- WAV is the supported library format; AIFF/AIF requires conversion before use.
- Move hardware remains the acceptance authority; desktop tests cannot certify
  input routing, control feel, display cadence, or realtime CPU margin.

See [HARDWARE_TEST.md](HARDWARE_TEST.md) for the first device pass.

## Repository layout

```text
include/sample_motion_app.h  Fixed-memory Sample Motion engine
include/thru_schwung_abi.h   Minimal Schwung API v2 declaration
src/dsp/thru_module.cpp      Move wrapper, recording, MIDI, waveform cache
src/ui.js                    Overtake controls, LEDs, and waveform screen
src/module.json              Schwung overtake manifest
docs/*.html                  Illustrated static manual website
release-notes/              Pre-release change notes
tests/                       Device-free ABI and DSP regressions
scripts/                     Test, docs, cross-build, validate, package tools
```

