# s3g Thru 0.30.0-test.54

Local testing build; not a published release.

- USB-A keyboard notes play Thru exclusively during takeover, preventing
  native Move from doubling the sound. Thru reads the original input channels
  directly; native track MIDI Out is no longer required.
- Track levels, MIDI settings and the Move set are preserved. Ordinary keyboard
  input resumes when takeover exits. Note-offs, sustain, bend and pressure
  remain available, with timestamp checks to prevent repeated or echoed notes.
- Existing E16 and Monolit parameter-control paths and bundles are unchanged.
- MIDI Notes manual, on-device help and the 0.30.0 release draft are updated.

Keep a set backup for downgrading from clip-bank format 35. The public
installer remains 0.29.1. Actual Move/Schwung listening tests remain pending.
