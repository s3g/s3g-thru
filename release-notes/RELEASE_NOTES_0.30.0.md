# s3g Thru 0.30.0 — draft

Unreleased. Prepared from local **0.30.0-test.52**. The GitHub installer
continues to provide **0.29.1** while testing and release review continue.

Two new engines, per-note Engine Lists, Legato playing and a sixteen-pad
sample bank extend the instrument. This update also adds MIDI controller
options, grouped page navigation, Priority Duck and aligned Tape mixing.

## Stochastic and Pulsar

- **Stochastic** continuously deforms recorded stereo fragments with independent
  amplitude and timing walks for each note. Folding, time warping, memory,
  source attraction and period drift range from subtle motion to drastic
  transformation. Keyboard pitch remains separate from source travel.
- Hold the Stochastic source with a main-encoder click while its shape keeps
  evolving. **LAW LSTN** uses internal note activity to restrain mutation and
  evolution; **CAPT** replaces PULL on knob 6. It needs no microphone input.
- **Pulsar** turns sample fragments into windowed pulse trains. RATE sets the
  pitched repetition clock; FORM sets pulse duration; SIZE selects how much
  source audio is read inside each pulse. SIZE spans **0.5–500 ms**, with fine
  logarithmic adjustment and the existing 4 ms default.
- Pulsar's second page adds emission and formant modulation, source scatter,
  formant variation, reverse probability and formant keyboard tracking.
  Main-encoder click holds source travel independently from stack scanning.
- Both engines retain stereo-linked reading, bounded reader counts and smooth
  transitions between fragments. There are now **fifteen playback engines**.

## Per-note engines and Legato

- **ONE / LIST** on Pads & Scale chooses one engine or an authored list.
  Configure up to eight entries in Single, four per instrument in the dual
  layouts, or two per instrument in Quad.
- Choose Up, Down, Random, Cycle Up, Cycle Down or Shuffle assignment.
  Repeated entries increase an engine's chance in Random. Degree and Register
  assignment map engines to keyboard position instead of playing order.
- Voice Setup adds per-entry octave, source launch position, stack phase/lane
  and stereo pan, plus pressure depth for the part. Entries using the same
  engine share that part's engine parameters. Clips capture resolved per-note
  assignments.
- Pressure can offset stack position, brake scanning, alter engine character,
  or combine these behaviors where the layout supports them.
- **VOICE** on Pads & Scale selects Poly, Mono retrigger or **Legato**.
  Overlapping Legato notes change pitch without restarting the playhead,
  envelope, stack phase or first engine. Last-note priority returns to the
  previous held key on release.
- **COMP** on Play Gain names the existing polyphony gain compensation; it
  remains independent of the Poly/Mono/Legato voice setting.

## Bank mode

- Play A through all four source lanes on the left half of the pads. The
  sixteen right pads hold independent, persistent stereo samples. Each side
  has four playback voices.
- Hold an empty right pad to record; release to finish silently. Hold a filled
  pad to play, **Shift + pad** to latch and **Delete (X) + pad** to clear.
  The curved-arrow **Undo** key restores the previous assignment.
- Choose **SRC** on Bank Pads: **IN**, **MIX**, **PT A** or **PT B**. IN records
  the selected mic/line input with recording gain. MIX captures both instruments
  and shared effects. PT A/B capture a part after its private effects, before
  part level, mute and crossfade; PT B can sample the Bank's own playing pads.
  PT A is the default. Source changes apply to the next take.
- Empty-pad touches under **10 ms** save nothing. Accepted recordings include
  the initial attack. Filled-pad playback remains immediate.
- Each sample has Start, End, Reverse, Level, Attack, Release and Loop Fade,
  plus a speed ratio, stereo pan, held-loop/one-shot choice and choke group.
  B's master RATE multiplies every pad's speed ratio.
- The waveform view shows the selected sample and a slot overview matching
  Move's physical pad rows. A retains four-lane waveforms with hollow lane
  markers. Tap Capture to change editing focus.
- Takes are stereo WAVs, up to twelve seconds each, archived in Thru Recordings.
  One capture/save runs at a time. Clips retain their own sample assignments
  and Bank settings, and record playback gestures alongside A's performance.
- Saving and Saved use compact, fixed-position indicators.

## Grouped page navigation

- **Capture + Menu** opens a hub with Voice, Lane, Mix/FX and Performance,
  followed by Mode, Setup, MIDI and All.
- **Capture + Track 1–4** jumps to the four working groups; repeating the chord
  advances through that group. Ordinary lane selection and Shift + Track
  sample browsing retain their functions.
- Parameter pages show a consistent, right-aligned position within their
  group. Up/Down stays in the group. Back returns from page to group, hub and
  the original view. All offers the complete contextual page list.

## MIDI and controller examples

- MIDI Control selects **Auto, NRPN, Generic CC, MNLT, E16 Remote, E+M or Off**.
  Note input and pressure remain independent of parameter control.
- The fixed high-resolution map has **357 NRPN addresses**. Channels 5–8
  address A–D and channel 16 addresses shared controls; notes and polyphonic
  aftertouch use channels 1–4. Current values follow supported edits and clip
  recalls without changing Move's editing focus.
- **Generic CC** maps eight consecutive absolute CCs to Move's visible page,
  with configurable channel, base CC and Pickup/Jump takeover.
- **MNLT** addresses A–D on channels 9–12 and shared controls on channel 13.
  The Monolit example puts A on faders 1–4 and B on 5–8 across live, engine,
  mix/send, color and rotary banks. Paired buttons remain Monolit MOTION
  controls. MIX/SEND puts X-CURVE on fader 4 and X-FADE on fader 8.
- **E16 + Monolit (E+M)** combines native relative E16 turns with Monolit's
  absolute CCs: Monolit TRS Out → E16 TRS In → E16 USB → Move. Use Schwung
  Surface **CC Only**, the **THRU EM** scene and its **Thru Rings** script.
  Clockwise increases values, one choice advances per detent, and an encoder
  press restores its parameter's default. Connecting E16 reads the project
  without replacing its settings; rings follow actual values from Move,
  Monolit and clip recalls.
- **E16 Remote** is a separate, script-free **THRU RM** example using Schwung
  Surface **E16**. It displays compact names with formatted values beneath,
  A/C on the top rows and B/D below. Shift + turn encoder 1 changes pages;
  Shift + press encoder 2 opens Page Jump. Headings update first, while
  ordinary value changes update only affected screen areas and rings.
- Both E16 options provide twelve stable page positions with adaptive engine
  controls. Use Remote for readable value text or E+M for the combined Monolit
  route. Controller files and the complete NRPN CSV are included in
  [controllers/](https://github.com/s3g/s3g-thru/tree/main/controllers).

## Mixing and Tape

- **Priority Duck** lets A or B make space for the other in Split and Shared
  Dual. PRIO chooses the dominant part and maximum reduction up to 24 dB;
  its midpoint is Off. Threshold, attack, hold, release, detector HPF, knee
  and Dry / Dry + Sends routing shape the response. Existing shared tails
  remain open. Older clips start with ducking disabled.
- Tape aligns dry/wet mixing and removes the automatic channel timing offset
  from Format and Age, reducing unwanted comb filtering at partial MIX.
  Intentional wow, flutter, drag and inertia remain available through MECH
  and the existing transport controls. The eight Tape controls retain their
  positions and saved IDs.

## Upgrading and documentation

- **Export a set backup before upgrading if you may need to downgrade.** This
  version reads earlier sets and writes clip-bank format **34**. Older releases
  cannot read the expanded bank. Existing A/IN Bank source choices migrate to
  their corresponding routes. Archived WAV recordings remain available.
- Existing engine and parameter IDs stay stable. Custom NRPN controllers must
  use the current ranges for expanded choices and Pulsar SIZE; the complete
  address, channel, curve and range reference is `controllers/midi/parameters.csv`.
- The total source-voice budget stays eight; all instruments and effects share
  Move's processor. Check fresh Shift + Menu timing counters with dense sounds.
- The illustrated manual includes all fifteen engine guides, Bank and Legato,
  grouped navigation, recording sources and both E16 setups. On-device help
  contains **39 topics**.
- Schwung **1.7.3 or newer** remains the declared minimum. **Shift + Capture
  remains Schwung Skipback**; use Capture + Menu for Thru's page hub.

See the [s3g Thru manual](https://s3g.github.io/s3g-thru/) for illustrated
controls, workflows and controller setup. This is a local release draft;
publication remains on hold.
