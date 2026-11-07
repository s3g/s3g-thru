# s3g Thru MIDI map, version 1

The high-resolution interface for 0.30.1 has 364
fixed NRPN addresses. MIDI Notes settings (NRPN 3584–3590 on channel 16) configure
keyboard/sequencer routing and expression independently of parameter mode.
See the [manual](https://s3g.github.io/s3g-thru/midi.html#note-input) for
channels 1–4, sustain, bend, pressure, wheel, velocity and Move clock setup.
`parameters.csv` is the full channel/address/range
reference; `parameters.json` is the editable source. Parameter addresses
belong to their controls across all engines; selecting another engine does
not change an address's meaning.

| MIDI channel | Purpose |
| --- | --- |
| 1–4 | Notes, sustain, bend, wheel and pressure; A–D or Focus routing |
| 5–8 | NRPN controls for A–D; identical addresses on each part's channel |
| 9–12 | MNLT direct CC for A–D; identical CC numbers on each part's channel |
| 13 | MNLT shared Performance controls and commands |
| 16 | Shared NRPN controls: transport, recording, mixer, effect returns, Tape, chains and fades |

Parameter channels are separate from note-input channels.
Single accepts part parameters on channel 5. Split and Shared Dual accept
5–6. Quad accepts all four. An unavailable part or a parameter on the wrong
NRPN channel is ignored. A/B edits leave Move's current part and page selected;
an explicit Edit Part or mode command performs the selection it names.

## Select the controller protocol

Open **Menu → MIDI Control** on Move. **Mode** chooses which parameter
protocol Thru accepts; musical notes, note-offs, polyphonic aftertouch, MIDI
clock and transport remain independent.

| Mode | Parameter input |
| --- | --- |
| Auto | NRPN, current-page Generic CC and E16 Remote |
| NRPN | The fixed 14-bit map in this directory only |
| Generic CC | Eight consecutive absolute CCs for Move's visible page |
| MNLT | Channel-addressed A–D banks and shared controls described in `../lightreft-monolit/` |
| E16 Remote | OXI Remote relative turns only |
| E16 + MNLT | Native E16 relative CC turns plus the MNLT absolute-CC bank |
| Off | No external parameter control |

For Generic CC, **CC Channel** defaults to 9 and **CC Base** to 20, so CC
20–27 address knobs 1–8. **Takeover** defaults to Pickup; Jump applies the first
received value. The current Move part and page supply the eight assignments, so
reserved or unavailable cells do nothing. Auto uses the same Generic CC settings.
MNLT is a separate mode, so its use of channels 9–13 does not overlap Generic CC.

**E16 + MNLT** is the combined hardware route. Connect Monolit TRS MIDI Out
to E16 TRS MIDI In, enable E16 MIDI Thru, then connect E16 USB-C directly to
Move USB-A. The stock E16 scene in `../oxi-e16-monolit/` sends relative CC
on channels 14/15, alongside Monolit's absolute CC on 9–13. Select E+M and
Schwung Surface CC Only. See that folder for setup. Its small Thru Rings script
mirrors actual values without taking over native turns. The fixed public NRPN map remains a separate interface.

## Sending a value

Send these four CC messages on the parameter's assigned channel, in this order:

1. CC 99 = NRPN address MSB (`address >> 7`).
2. CC 98 = NRPN address LSB (`address & 127`).
3. CC 6 = value MSB (`value >> 7`).
4. CC 38 = value LSB (`value & 127`).

Values run from **0 to 16383**, mapped across the parameter's documented
minimum and maximum. This includes choices and integers, which are rounded
to their available values. For a linear parameter:

```text
value = round(16383 * (wanted - minimum) / (maximum - minimum))
```

Default/unity and bipolar zero values snap exactly at their nearest MIDI code.
For example, Speed can select exactly 1×. Stack Start has a mode-specific
range: 1–2 within each Split part's pair, 1–4 in Shared Dual/Single, and the
one available lane in Quad. Cue commands likewise use local cue numbers for
the addressed part.

Thru commits only a complete, MSB-first Data Entry pair. It does not apply the
coarse value while waiting for the fine byte. A new address, RPN selection,
NRPN null (`127/127`) or CC 121 resets an unfinished transaction. CC 96/97
increment/decrement are consumed but do not change a parameter. State is
separate per channel and per external/host input source.

Actions are marked in the CSV. A 0–1 action normally fires at 16383; zero does
not fire it unless zero has a defined meaning, such as stopping a Record
Gate. Indexed actions use their documented slot range. Repeated complete
messages can repeat an action; use an intentional encoder press or button.

## Feedback

Thru sends matching NRPN values for common playback and effect controls from
startup, as listed in `startup_feedback` in the MIDI registry. Other
non-action parameters subscribe when first used through NRPN. Later Move edits,
clip recalls and other changes are reflected. Output is bounded and coalesces
intermediate values. Identical feedback echoed into Thru is ignored. Configure
your controller to receive values without retransmitting them.

The current [E16 Remote example](../oxi-e16/README.md) provides adaptive engine and effect pages.
Hold E16 Shift and turn encoder 1 to move through twelve fixed page positions.
Remote sends relative encoder turns on channel 1; the DSP applies them directly
and retains high-resolution values. Move draws the E16 display and RGB rings.
Ordinary NRPN feedback pauses while Remote is active. Other controllers can
still use the fixed NRPN map above. E16 and ordinary parameter edits leave
Move’s editing focus in place.

### Scene push commands

These notes belong to the **control channels**, separate from musical notes.
Only Note On with nonzero velocity acts; Note Off is consumed without action.

| Channel | Note | Action |
| --- | --- | --- |
| 5–8 | 0 | Toggle that part's Stack Hold/Run, if the part exists |
| 16 | 0 | Toggle pattern-fade Hold/Run |
| 16 | 1 | Finish the pattern fade |
| 16 | 2 | Sync: resend current scene/subscribed values to the controller |

Turning the pattern-fade position NRPN engages Hold for manual interpolation.
Press Hold/Run to resume the automatic fade. Sync is useful after reconnecting
another mapped controller; it does not change audio or Move's focus. Other
notes on these control channels are consumed without triggering a voice.

## Address groups

NRPN MSB groups keep controls together: Core, Pads, Envelope, Stack, Motion,
Grains, Weave, Cutups, Sample, Glitch, Wavesets, Prism, Morph, Texture, Formant,
Color, Rotary, Tape, Reverb, Mixer, Record, Performance, Chain, Pattern Fade and
Actions. The same group can contain part and shared parameters; the CSV's
Channels column is authoritative. Part Level, Delay Send and Reverb Send also
have uniform part-channel aliases.

Do not renumber existing IDs when expanding the map. Append a new unused ID
within the appropriate group. Generate/check the source and documentation with:

```sh
npm run midi:generate
npm run midi:check
```

Reserved address 2452 must not be reassigned.

## E16 Remote

The current E16 Remote example uses OXI’s published Remote messages, with
header `F0 00 21 5B 02 01`. Continuous turns are applied in `onMidi`. Raw UI
button state and connection ownership use the parameter mailbox. Absolute button masks and Quad pair
targets are safe to retry; mirrored copies of button notes do not apply
shortcuts twice.

Channel 1 CC 1–16 contains encoder-relative increments; notes 0–15 are encoder
pushes and note 16 is Shift. These messages are reserved only after an E16
acknowledges Remote entry. Other musical notes and NRPN channels stay available.
A connection lease releases the routing if the UI or controller disconnects.

The fixed NRPN addresses are unchanged.

## Custom CC profiles

The MIDI Maps page provides four editable profiles, 128 entries each and eight
controller banks. Learn or enter a channel/CC and assign a named parameter,
engine-page slot or controller command. Destinations can target A–D, current
Focus or a shared parameter. Each entry has range, inversion, pickup and button
behavior. An active profile replaces current-page Generic CC; only explicitly
claimed addresses override the other selected protocol routes. MODE Off still
disables parameter control.

Profiles save in `thru-midi-maps.bin` with a backup, separately from clips.
Loading a profile installs routing without writing instrument values. A bank
entry overrides an All-bank entry at the same address; duplicate entries in
the same bank require confirmation. A read-only nanoKONTROL2 example is in
`../korg-nanokontrol2/` and can be copied to a Custom slot on Move.
