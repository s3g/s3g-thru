# s3g Thru 0.25.0

## Weave playback engine

- Adds Weave as the eighth playback engine without changing the indices of the
  existing seven engines. Weave takes a long-grain approach to sustained,
  overlapping sample layers.
- Runs one to four bounded macro-granular layers per note, with a 250–12,000 ms
  duration range and independent captured source position and stack blend for
  every layer.
- Adds Scan, Freeze, Cloud, and Slice origins; Bell, Hann, Triangle, and
  Plateau envelopes; Plateau edge time; stereo spread; reverse probability;
  position and pitch spray; duration and level variation; phase separation;
  and Scan or Per Layer source advance.
- Uses fine 10 ms encoder steps through 1 second, 50 ms steps through 4
  seconds, and 100 ms steps through 12 seconds. Turning backward follows the
  same detents exactly.
- Keeps note pitch assigned to source motion while the Layer Pitch control
  independently retunes each event, matching the established Grains behavior.
- Bounds maximum work to four Weave layer readers per playback voice, or 32
  readers across the eight-voice instrument.

## Surface, clips, and manual

- Adds dedicated Weave and Weave Motion parameter pages plus the shared
  Stack Path page. All 15 controls can be captured as performance-loop data.
- Adds generated Move-screen figures and a complete Weave engine guide.
- Advances JSON state to version 7 and the clip bank to format 5. Version 6
  state and clip-bank formats 1–4 remain readable; existing parameter and
  engine IDs retain their meanings.
