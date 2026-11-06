# s3g Thru 0.30.0-test.42

Local E16 enum and page-position candidate. The public GitHub installer remains
at 0.29.1.

- Makes native E16 enum pickup region-aware. ALG and other choice controls can
  accumulate fine 14-bit turns within the current option and enter the next
  option without being mistaken for an external state change.
- Retains test.41 startup protection: connecting or recalling E16 cannot apply
  zero or nonzero remembered values to an already-open Thru set.
- Gives every parameter page a group-local `current/total` marker at a fixed
  right-aligned title position, including Path Editor, Pattern Fade, chain fade
  pages, Chain Tools, Effect Order, Setup and MIDI.
- Uses the same aligned fraction treatment on the page lists, FX list, and five
  Shift + Menu diagnostic pages.
- Retains the four-bank Pages hub and hierarchical Back behavior from test.40.
