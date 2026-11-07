# Light Reft Monolit mapping

This is the s3g Thru live-control layout for Light Reft Monolit. It uses
ordinary absolute MIDI CC, so no NRPN support is required. It targets controls
that are useful during performance rather than exposing every Thru setting.

Requires the matching **0.30.0** module.

## Thru setup

1. Open **Menu → MIDI Control**.
2. Set **Mode** to **MNLT**.
3. Leave **Takeover** at **Pickup** for safe changes, or select **Jump** for
   immediate absolute response.
4. Parts are addressed directly by MIDI channel: 9 = A, 10 = B, 11 = C and
   12 = D. No target-selection step is required.

## Combined E16 connection

For the one-USB-device route, connect **Monolit TRS MIDI Out → E16 TRS MIDI
In → E16 USB-C → Move USB-A**. Power Monolit separately over USB-C, enable
MIDI Thru in E16's MIDI settings, set Schwung Surface to **CC Only**, and set
Thru's MIDI mode to **E+M**. Load the native relative-CC scene from
[`../oxi-e16-monolit/`](../oxi-e16-monolit/) on E16. E16 then supplies direct
relative A/B control while forwarding this preset's
channel 9–13 CC messages from its TRS input. A small feedback-only script on
E16 mirrors actual ring values; native turn and press assignments stay in
E16 firmware. No Remote surface or USB hub is required.

Part controls use CC 16–79 on each part channel. Channel 13 carries the A/B
crossfade at CC 24, crossfade curve at CC 25, Priority Duck at CC 26–33,
shared Performance controls at CC 72–79 and optional
commands at CC 16–23. Banks C–F follow
the addressed part's current playback engine, using the same engine-detail page
order as Thru and E16. This allows A and B controls to move simultaneously.

The supplied 64-fader example prioritizes Split and Shared Dual performance.
Every bank keeps Part A on faders 1–4 and Part B on faders 5–8:

| Bank | Faders 1–4 · Part A, channel 9 | Faders 5–8 · Part B, channel 10 |
| --- | --- | --- |
| 1 · LIVE | POS, FLD, SPD, LVL | POS, FLD, SPD, LVL |
| 2 · ENG 1A | Engine page 1, knobs 1–4 | Engine page 1, knobs 1–4 |
| 3 · ENG 1B | Engine page 1, knobs 5–8 | Engine page 1, knobs 5–8 |
| 4 · ENG 2A | Engine page 2, knobs 1–4 | Engine page 2, knobs 1–4 |
| 5 · ENG 2B | Engine page 2, knobs 5–8 | Engine page 2, knobs 5–8 |
| 6 · MIX / SEND | LVL, DLY, REV, X-CURVE | LVL, DLY, REV, X-FADE |
| 7 · COLOR | TONE, DRIVE, BITS, RATE | TONE, DRIVE, BITS, RATE |
| 8 · ROTARY | X-PN, RATE, DPTH, MIX | X-PN, RATE, DPTH, MIX |

Algorithm selection and engine-detail pages 3–4 stay on Move. ENG 1 and ENG 2
follow the algorithm currently selected for each part, so A and B can use the
same engine with different settings or different engines.

X-CURVE on fader 4 uses channel 13 CC 25: its lower region selects Linear and
its upper region selects Equal Power. X-FADE on fader 8 uses channel 13 CC 24:
left is A, center is an equal A/B blend and right is B. ROTARY keeps per-part
X-PN available while omitting SPRD from this focused example; the complete
receiver still exposes SPRD.

All eight paired buttons remain local **MOTION** controls on every bank. They
are not assigned to Thru commands. The preset enables Monolit MOTION mode for
all 64 faders and preserves the source preset's Motion configuration.

`mapping.csv` documents the complete Thru receiver, including all four part
channels, Mix/Color, Rotary, shared Performance and optional commands. Those
command addresses remain available for a different controller or custom preset;
the supplied Monolit example does not use them.

## Importable preset

`presets/s3g-thru` is generated from a Monolit preset exported under current
firmware. It preserves the template's device structure and Motion settings while
replacing the 64 fader MIDI addresses and display names. Import it through
<https://monolit.jp/>, then
select the new preset on Monolit. The Web-Editor uses the imported filename as
the preset name.

After uploading, select the preset on Monolit, enable MNLT or E+M in Thru,
and check the LIVE bank. Pickup waits for each fader to reach its current value;
Jump applies the fader position immediately.
Its SHA-256 checksum is recorded beside it in `presets/s3g-thru.sha256`.

To rebuild from another current Monolit export:

```sh
node controllers/lightreft-monolit/generate-preset.mjs \
  /path/to/exported-preset \
  controllers/lightreft-monolit/presets/s3g-thru
```

Use `--check` before the two paths to compare an existing generated preset with
the supplied template. The generator changes only the fader channel, CC range,
MIDI message type, MOTION mode and display labels. Button MIDI assignments and
Motion timing/shape settings remain those of the source export.

Official device and update information:

- <https://www.lightreft.jp/monolit>
- <https://www.lightreft.jp/support>
- <https://monolit.jp/>
