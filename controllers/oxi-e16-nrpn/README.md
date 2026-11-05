# OXI E16 NRPN + Monolit routing

This local controller example uses one physical USB connection to Move:

```text
Monolit TRS MIDI OUT -> E16 TRS MIDI IN -> E16 USB-C -> Move USB-A
```

E16 supplies the high-resolution A/B controls and receives parameter feedback.
Its MIDI Thru setting merges Monolit's ordinary CC messages into the same USB
connection. No Lua script, E16 Remote surface or USB hub is used.

Requires s3g Thru **0.30.0-test.39 or newer** and current E16 firmware.

## Connect and configure

1. Power Monolit separately over USB-C. TRS MIDI carries data, not power.
2. Leave E16 disconnected from Move. In Schwung Global Settings, set
   **Surface** to **CC Only** so E16 Remote does not own the port.
3. Open s3g Thru and set **Menu -> MIDI
   Control -> Mode** to **N+M** (NRPN + MNLT). The selection is saved.
4. On E16, use USB1, turn USB Multi Port off, and enable MIDI Thru in
   **Settings / Conf -> MIDI** so messages received at TRS input are echoed to
   the USB output. Do not enable automatic scene or page transmission.
5. In OXI App, use this directory as the computer storage folder and transfer
   **Scenes/THRU NRPN MNLT.oxie16** to an unused E16 scene slot. Select
   **THRU NM** on E16 before opening Thru takeover.
6. Load the supplied Monolit example from
   `../lightreft-monolit/presets/s3g-thru`, or retain a custom preset that uses
   Thru's documented channel 9-13 MNLT map.
7. Connect Monolit **TRS MIDI OUT** to E16 **TRS MIDI IN** with a compatible
   type-A TRS MIDI cable. Connect E16 USB-C directly to Move USB-A, select
   THRU NM, then open Thru takeover.

N+M disables Thru's E16 Remote discovery and rejects Remote handshakes. If an
older build has already replaced THRU NM with Remote, install test.39,
disconnect E16, set Surface to CC Only and Mode to N+M, then reconnect and
reselect THRU NM.

## Updating from test.38

Install the test.39-or-newer module and transfer the supplied THRU NM scene
again. The revised scene reserves raw zero for startup synchronization, so
selecting or reconnecting it cannot force the Engine page to minimum values.
Thru also recognizes the exact state test.38 could save—Position 0, minimum
Field, Speed and Scan, Join 0 and possibly Level 0—and restores playable
defaults on first load. An individual control intentionally set to its minimum
does not trigger the repair.

If library preview is audible while pads light and playheads move without
performance audio, confirm the installed version is test.39 or newer, reopen
takeover once, and check that FIELD is full, SPD and SCAN are 1, JOIN is full
and LVL is at unity.

OXI documents that E16 can echo an input to its other outputs when MIDI Thru is
enabled. Both devices use type-A-compatible TRS MIDI; verify any adapter or
cable rather than relying only on its physical connector shape.

## E16 pages

The scene has the same twelve stable page positions as the Thru Remote layout:

| Page | Controls |
| --- | --- |
| ENGINE | Algorithm, Position, Field, Speed, Stack, Scan, Join, Level |
| STACK | Stack Path controls when available |
| ENG 1-4 | Current engine's four detail pages |
| SENDS | A/B sends, Priority direction/depth and threshold, curve and crossfade |
| COLOR | Per-part Tone, Drive, Bits and Rate |
| ROTARY | Per-part Stereo / Rotary controls |
| DELAY | Shared Delay return |
| REVERB | Shared Reverb return |
| TAPE | Shared Tape effect |

Encoders 1-8 are A and 9-16 are B. Shared pages use encoders 1-8 only. ENG
pages use stable labels P1-P8 because a stock scene cannot rename controls when
the selected engine changes; their destinations and values do follow each
part's current engine. Move remains the authoritative display for full names,
units and option labels.

A quick press on any active encoder resets its current Thru parameter to the
registered default. This remains adaptive on ENG pages: the reset follows the
engine selected for that part. For example, PRIO returns to neutral **Off**,
XFD returns to center, levels return to unity and effect mixes return to dry.

The scene uses controller-private turn aliases 3456-3551 on channels 5 and 6.
Reset presses use CC 0-95 on dedicated channels 14 (A) and 15 (B), so they do
not replace the encoder's active NRPN address or collide with Monolit.
Do not map those aliases on another controller. Thru resolves them dynamically
and sends matching feedback. Monolit remains on channels 9-13, so its faders
can be moved at the same time without changing E16's A/B assignments.

## Expected limitations

- The stock E16 display shows its native numeric value format, not Thru's full
  formatted strings.
- The fixed ring colors identify A and B; they do not dynamically copy every
  Move mode color.
- This scene exposes A/B. Quad C/D remain available through Move or the fixed
  NRPN registry.
- Feedback returns to E16 only. Monolit's CC faders use Pickup or Jump and do
  not receive motorized feedback.
- Monolit edits are prioritized ahead of the background alias sweep, so a
  matching E16 ring follows the fader on the next feedback tick. The source
  remains 7-bit, providing 128 positions rather than E16's full 14-bit turn
  resolution.

Regenerate or validate the scene with:

```sh
node controllers/oxi-e16-nrpn/generate-scene.mjs
node controllers/oxi-e16-nrpn/generate-scene.mjs --check
```

Official references: [OXI E16 MIDI In](https://docs.oxiinstruments.com/oxi-e16-manual/midi-in/)
and [Connect & Control](https://docs.oxiinstruments.com/oxi-e16-manual/connect-and-control/).
