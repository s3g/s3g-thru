# s3g Thru 0.30.0-test.55

Local testing build; not a published release.

- Repairs lost sequencer note-offs, including release messages that match the
  native-suppression release and USB batches without timestamps.
- Repeated note-ons on one input/channel/pitch replace the active gate. The
  next note-off ends it, without leaving extra held-note counts.
- MIDI Stop releases the sending input’s notes even with Internal timing,
  preserving physical Move pads and independently controlled Thru clips.
- A note without channel pressure retains its configured initial stack offset.
  Held MIDI and pad playhead/scan behavior is covered by parity checks.
- The MIDI guide and help include native Move MIDI In/Out Off as an extra
  isolation check, plus gate, release-tail, Stack Hold/Brake and OXI guidance.

Clip-bank format remains 35. E16/Monolit bundles and the public installer
(0.29.1) are unchanged. Actual OXI/Move testing remains necessary.
