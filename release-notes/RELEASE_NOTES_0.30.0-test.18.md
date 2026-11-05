# s3g Thru 0.30.0-test.18

Local E16 redraw improvement. The published installer remains at 0.29.1.

- Sends only the changed pixels of labels and values, including clearing pixels
  when text gets shorter or a control disappears.
- Budgets USB packets by elapsed time, allowing several small updates per tick.
  Keeps every message within twelve USB packets, pending pixels within 320 and
  at most eight small screen patches in flight.
- Keeps the heading first, fixed ring colors, direct parameter control and
  automatic display recovery. E16 Link now shows PENDING PX.

The full-page simulation at 33 ms UI ticks/replies improved from 1,386 ms to
858 ms. Actual redraw speed depends on controller/host replies and needs device
confirmation. DSP, MIDI mappings and the THRU RM scene are unchanged.

Install the module tarball and reopen takeover. Keep the working E16 scene and
Schwung Surface E16; no new scene transfer is needed. The current settings
bundle is built with `npm run controllers:package`.
