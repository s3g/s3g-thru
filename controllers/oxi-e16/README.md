# OXI E16 — s3g Thru Remote

The E16 Remote example for the **0.30.0** uses OXI Remote so
each encoder can show a small parameter name with its formatted value directly
underneath. A/C occupies the first two rows and B/D the lower two. The E16
follows each part's selected engine without changing Move's current focus.

## Install and connect

1. Install `dist/s3g-thru-module.tar.gz` with Schwung Manager and restart Move.
2. Extract `dist/s3g-thru-e16-remote.zip`, or select `dist/e16-remote` as OXI
   App's computer storage folder. Transfer **THRU RM** to an unused E16 slot.
   It is deliberately script-free; no Lua needs compiling or transferring.
3. On Move choose **Schwung Global Settings → Surfaces → Surface → E16**.
4. Use current E16 firmware, USB1, USB Multi Port Off and USB MIDI Thru Off.
5. Connect Move USB-A to E16 USB-C, select THRU RM, then open Thru takeover.

Remote starts automatically and replaces the blank scene with the current
parameter page. If `THRU RM–REMOTE` remains, check the selected Schwung surface,
USB settings, matching module version and scene, then reopen takeover.

## Pages and layout

Hold the round **E16 Shift** button inside the OXI O and turn encoder 1 to move
through twelve stable page positions:

For direct access, press **Shift + encoder 2** to open **PAGE JUMP**, release
Shift, then press encoder 1–12 under the page you want. The selected page opens
and the overlay closes. Press encoder 16 to cancel without changing page.

| Page | Controls |
| --- | --- |
| ENGINE | Algorithm, Position, Field, Speed, Stack/Lane, Scan/Advance, Join and Level. |
| STACK | Stack Path, Blend, Shape, Offset, Skew, Curve and Slew. |
| ENG 1–4 | The selected engine's four possible detail pages. |
| SENDS | Per-part Delay and Reverb sends in multi-instrument modes. |
| COLOR | Tone, Drive, Bits and Rate. |
| ROTARY | Cross Pan and Rotary controls. |
| DELAY | Shared Delay return. |
| REVERB | Shared Reverb return. |
| TAPE | Shared Tape effect. |

Unused controls show a small reserved square and their rings stay off. Page
numbers do not move when an engine changes. In Single, only the upper half is
used. Split and Shared Dual show A/B. Quad shows A/B or C/D; use
**Shift + press encoder 1** to switch pairs. Shared SENDS still exposes all four
Quad parts.

The display uses a 3×5 Remote font: labels are above percentages, choices,
ratios, milliseconds, seconds, hertz or semitones. Remote redraws only changed
pixel rectangles and changed rings. A new heading is acknowledged before the
body changes, while ordinary text-choice edits update only their affected
value tile. Algorithm changes preserve common cells and the other part.

Ring colors follow the performance mode: neutral A, teal Split B, plum Shared
Dual B, yellow C and plum D. Shared returns use purple. These colors are sent
as Remote RGB rather than being restricted to E16's native color swatches.

## Shortcuts

- **Shift + turn encoder 1:** previous/next page.
- **Shift + press encoder 2:** open Page Jump; release Shift and press 1–12.
- **Press encoder 16 in Page Jump:** cancel and return to the current page.
- **Hold an encoder while turning:** fine continuous adjustment.
- **Shift + press encoder 1 in Quad:** A/B ↔ C/D.
- **Shift + press encoder 16:** leave Remote until takeover is reopened.

Pads & Scale, envelopes, Engine List setup, Path Editor points, sample browsing,
recording, performance loops and chains remain on Move. For native relative
turns and Monolit forwarding, use the separate
[E16 + Monolit example](../oxi-e16-monolit/README.md) with Schwung Surface CC Only.

## Link report and recovery

On Move open **Shift + Menu → E16 Link 5/5**. STATE reports LIVE, SEARCH,
WAIT CLEAR, RECONNECT or OFF. LAST retains the most recent issue. ACK/NACK and
RETRY/RECON report display transport results; PENDING PX is unacknowledged
screen work, not DSP/audio load.

Remote output is bounded to complete MIDI messages and a 320-pixel acknowledged
window. Missing acknowledgements, rejected patches, reconnects and USB replug
are retried without requiring a new takeover session. Channel 1 CC 1–16 and
notes 0–16 are reserved while Remote owns the surface. Other musical notes and
the normal NRPN channels remain available.

## Build and test

Run `npm run controllers:generate`, `npm run midi:generate`, and `npm test`.
`npm run controllers:package` creates `dist/s3g-thru-e16-remote.zip`. Tests cover
all engines and modes, fixed page positions, formatted values, fine adjustment,
pair switching, changed-pixel updates, heading priority, delayed/missing
acknowledgements, reconnects and rate limits.

[OXI scene naming limits](https://docs.oxiinstruments.com/oxi-e16-manual/scenes/) ·
[OXI Remote protocol](https://docs.google.com/spreadsheets/d/1Yccnrluv10QL_PauMmtCt64EtYjfSeZEXKrlrw8P24w/edit)
