# s3g Thru 0.30.0-test.26

Local MIDI-mode and Light Reft Monolit test build. The public GitHub installer
remains at 0.29.1.

## Selectable MIDI parameter control

- Adds **Menu → MIDI Control** to Single, Split, Shared Dual and Quad.
- Adds Auto, NRPN, Generic CC, MNLT, E16 Remote and Off modes. Musical notes,
  note-offs and polyphonic aftertouch remain independent of parameter mode.
- Fixes the editor snapshot used by Move so the MIDI Control page opens instead
  of remaining on `UPDATING EDITOR`.
- Generic CC maps eight consecutive absolute CCs to Move's visible parameter
  page. Channel, first CC and Pickup/Jump takeover are configurable.
- Removes the obsolete hard-coded 7-bit parameter route, preventing unmapped CC
  traffic from changing the instrument.

## Light Reft Monolit

- Addresses parts directly on channels 9–12 for A–D, using CC 16–71 for
  Engine, Stack, four adaptive engine-detail pages and Mix/Color. Multiple
  parts can be adjusted simultaneously without a target selector.
- Places shared Performance controls on channel 13, CC 72–79, and commands on
  channel 13, CC 16–23 for Stack Hold A–D, clip record, pattern-fade Hold,
  Panic and one reserved command.
- Supplies a 64-control preset focused on simultaneous A/B Engine, Stack and
  first two engine-detail pages.
- Includes `controllers/lightreft-monolit/presets/s3g-thru`, a reproducible
  generator, mapping CSV and setup instructions. The 64 controls and transfer
  chunks pass structural validation; physical upload testing remains pending.
Install `dist/s3g-thru-module.tar.gz` for hardware testing. Native, sanitizer,
UI, help, MIDI and package checks are required before promotion; hardware
acceptance is not implied by the local build.
