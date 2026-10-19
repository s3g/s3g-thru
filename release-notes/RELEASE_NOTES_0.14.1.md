# s3g Thru 0.14.1 — Visible Sample Actions

Sample-page edits now behave as explicit actions on Move:

- Trim, Reverse, Normalize, and Fade display `TURN` when ready rather than an
  ambiguous dash.
- Turning an action encoder in either direction applies it through the critical
  host command path.
- The parameter-page header confirms the result and selected lane, for example
  `NORMALIZED L1`.
- Empty lanes, active recording, and pattern preparation produce visible
  explanations instead of silent no-ops.
- The DSP exposes its last edit error to the takeover UI and clears stale
  errors when a sample edit begins successfully.
