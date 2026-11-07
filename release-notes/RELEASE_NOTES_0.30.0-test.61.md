# s3g Thru 0.30.0-test.61 — Bank capture safeguards

Local testing build; not published.

- Bank pad recordings must last at least 250 ms. Short accidental presses
  leave the pad empty, archive no recording and preserve the previous Undo.
- Effectively silent takes are discarded too: at least one stereo channel
  must exceed −90 dBFS peak during the take. This is a conservative silence
  check, not a gate or onset delay; quiet material and short attacks are kept.
- Accepted takes retain audio from the initial press. Filled pads still play
  immediately. A brief on-screen message explains a discarded take.
- All four Bank sources use these safeguards. Existing recordings, clip/state
  formats, MIDI assignments and controller examples remain unchanged.

Includes the Custom MIDI profiles and Learn workflow from test.60.
Actual Move/Schwung controller and listening tests remain to be completed.
