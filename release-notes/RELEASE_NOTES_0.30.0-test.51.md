# s3g Thru 0.30.0-test.51

Local testing build; not published.

- Bank Pads SRC now matches Mix/Rec: IN, MIX, PT A and PT B.
- PT B records Bank’s own playing pads after their private effects. Play or
  latch a filled pad, then hold an empty pad to capture it. Release to save;
  the new recording waits until you trigger it.
- MIX includes both parts, their mix settings and shared effects. IN uses
  Move’s selected mic/line input with recording gain; monitoring can stay off.
- PT A and PT B record before part level, mute and crossfade.
- The selected source stays fixed for each take. Older A/IN settings migrate
  to PT A/IN, retaining the previous recording source.
- Retains the E16 direction and Bank A marker fixes. E16 source selection
  follows the new choices automatically; no controller reimport is needed.
