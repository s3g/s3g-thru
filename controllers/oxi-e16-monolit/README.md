# E16 + Monolit: native relative CC

Use **E+M** in s3g Thru and **THRU EM** on E16. This example requires the
**0.30.0 draft** module with the matching THRU EM scene and Thru Rings
script. The scene and script are unchanged from the current test.52 setup;
existing users of that setup do not need to reimport them.

```text
Monolit TRS OUT -> E16 TRS IN -> E16 USB-C -> Move USB-A
```

1. Power Monolit separately. Use compatible type-A TRS MIDI connections.
2. In Schwung Global Settings choose **Surfaces -> Surface -> CC Only**.
3. Open Thru, then choose **MIDI Control -> MODE -> E+M**.
4. In OXI App select the extracted ZIP folder (or `dist/e16-monolit`) as
   computer storage and transfer
   **Scenes/THRU E16 MNLT.oxie16**. Select **THRU EM** on E16.
5. On E16 use USB1, turn USB Multi Port off, and enable TRS-input MIDI Thru
   to USB. Keep automatic scene, page and preset transmission disabled.
6. Import the Monolit example from `../lightreft-monolit/presets/s3g-thru`
   with the official Monolit Web-Editor, or retain a custom channel 9–13 map.

The native scene uses a small feedback-only Lua script and no Remote display. Transfer the attached **Thru Rings** script with the scene; it is also supplied in `Scripts/Thru Rings.e16script`. Native turn and press assignments remain in E16 firmware. Installing the
scene does not change the Monolit preset. Replace the older native NRPN scene;
that scene's private NRPN messages are ignored by this module.

## Controls

- Encoders **1–8: A**, **9–16: B**. Move keeps its existing editing focus.
- Pages: Engine, Stack, ENG 1–4, Sends, Color, Rotary, Delay, Reverb, Tape.
- Shared pages use the first eight encoders. ENG labels are P1–P8; read the
  parameter names, units and option text on Move.
- Clockwise turns increase values; counterclockwise turns decrease them.
  Relative turns use Thru's current value immediately. Enum choices advance
  one step per detent; numerical controls use a high-resolution internal step.
- **Press an encoder** to restore the current destination's default. Release
  and continue turning normally.
- Monolit faders retain their absolute CC map. **Pick** prevents jumps until a
  fader reaches the current value; **Jump** applies its position immediately.
- The native scene targets A/B. Quad C/D remain available from Move or the
  fixed NRPN map. In Bank, B’s ENG 1 slots match Bank Sample and ENG 2 matches Bank Pads; Engine/Stack and ENG 3–4 have no B assignments.
- B ENG 2 / P7 selects Bank recording source:
  IN, MIX, PT A or PT B. PT B records the Bank pads themselves. No scene or
  ring-script reimport is needed for these additional choices.

Pulsar SIZE reaches 500 ms. Its native relative turns retain fine adjustment
at small sizes.

The rings show **Thru’s actual values** for the selected E16 page, even while
Move displays another page. Minimum is at the start of the ring, maximum at
the end; bipolar controls such as pan have their zero at the midpoint.
Enum choices are spread evenly across the ring. HOLD follows Move’s musical
time steps, from Manual to 60 seconds, and its ring follows those steps. Colours identify A and B.
Move edits, Monolit moves, clip recalls and default presses update the rings.
Inactive destinations show zero fill. Native text remains the scene’s labels;
the separate `../oxi-e16/` Remote example provides formatted text values.

The script only announces the current page and draws returned ring values.
It never writes a stored parameter value or handles encoder turns. Connecting
or changing pages therefore reads Thru without replacing project settings.
Feedback is coalesced and limited to six rings per output batch. A fresh page
snapshot and one-second refresh recover reconnections; turns never wait for it.
The matching script/module still require device testing for visual response.

## Addresses

| Action | Channel | CC | Value |
| --- | --- | --- | --- |
| A turn | 14 | page × 8 + slot, 0–95 | 127 increase / 1 decrease |
| B turn | 15 | page × 8 + slot, 0–95 | 127 increase / 1 decrease |
| A/B default press | 14 / 15 | 127 | page × 8 + slot + 1, 1–96 |
| Default release | 14 / 15 | 127 | 0, ignored |
| Ring request | 14 | 125, then 126 | Snapshot token 1–127, then page 1–12; read-only |
| Ring response | USB1 | Private SysEx | Selected page/token and absolute ring values; never turn CCs |
| Monolit | 9–13 | Existing MNLT map | Absolute 0–127 |

Page and slot indices above start at zero. Turns accept magnitudes up to eight
in two's-complement form; the supplied scene uses no encoder acceleration.
Relative CC describes the MIDI message; Thru keeps full internal parameter
precision between turns. No stored controller value is used as the origin.

## Troubleshooting

If the scene becomes Remote, set both Schwung Surface **CC Only** and Thru
MODE **E+M**, then reselect THRU EM. If turns do nothing, check the matching
module/scene, USB1 and Multi Port Off. If only Monolit is missing, check its
power, TRS cable, MIDI Thru and channel map. Connecting E16 after opening a
set does not intentionally send scene values; no ring pickup is required.

Rebuild with `npm run controllers:monolit:package`. The importable archive is
`dist/s3g-thru-e16-monolit.zip`; OXI App storage is `dist/e16-monolit`.
