# s3g Thru 0.30.0-test.52

Local testing build; not published.

- Pulsar SIZE now spans 0.5–500 ms. Move and E16 retain fine adjustment for
  short fragments; MIDI ranges and documentation match. SIZE is the source
  material read within a pulse; FORM continues to set pulse duration.
- Bank ignores empty-pad touches shorter than 10 ms, saving no recording.
  Longer takes retain the audio from the initial press, including the attack.
- Existing clip values, Bank levels, controller scenes and mappings remain
  available. No E16 reimport is needed; update the Move module.
